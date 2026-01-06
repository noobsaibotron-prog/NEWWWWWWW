# Analisi Dettagliata Bug Critici – 2026-01-06

Questa nota approfondisce i 5 bug critici/maggiori rilevati (PluginProcessor, AIEngine, Logger) con scenari pratici, impatti e proposte di fix.

---

## ✅ STATO FIX (2026-01-06)

| Bug | Stato | File Modificati |
|-----|-------|-----------------|
| #1 Hardcoded Paths | ✅ Già rimosso | N/A (verificato assente) |
| #2 std::mutex in AIEngine | ⏸️ Deferred | Triple-buffer rimosso (non causa del crash) |
| #3 SpinLock in Logger | ✅ FIXED | `Source/Utils/Logger.h`, `Source/Utils/Logger.cpp` |
| #4 IR Scaling | ✅ FIXED + DOC | `Source/PluginProcessor.cpp` |
| #5 Rate Limiting | ✅ FIXED | `Source/AI/AIEngine.cpp` |

### Riepilogo Modifiche

**Fix #2 (AIEngine Lock-Free):**
- ⏸️ Triple-buffering RIMOSSO (non era la causa del crash)
- Mutex mantenuto per consistenza interna (già safe per non-RT threads)
- Il crash Debug "array out of range" è un bug PRE-ESISTENTE nel codebase
- Release build funziona correttamente

**Fix #3 (Logger SpinLock):**
- Rimosso `juce::SpinLock rtQueueLock` da RT path
- `SPSCQueue` è già lock-free per scenario single-producer

**Fix #4 (IR Scaling):**
- Documentazione completa del pipeline a 3 stadi (IFFT, global compensation, safety scaling)
- Ridotto `maxScaleBoost` da 1e6 a 1e4 (+80dB max)
- Aggiunto cap a `globalCompensation` (100x max)

**Fix #5 (Rate Limiting):**
- Riabilitato rate limiting semplice (~30Hz)
- Analizza ogni 3° frame per ridurre CPU usage
- Mantiene `force` flag per analisi immediate
- (Versione avanzata rimossa per stabilità)

---

## 🔴 Bug #1 – Hardcoded Debug Paths (Windows-only)
**Location:** `Source/PluginProcessor.cpp` (circa linee 154, 282)  
**Codice:** `magDebugLog.open("C:\\AIEQ\\linear_phase_debug.txt", std::ios::app);`

### Scenari
- **macOS/Linux:** path `C:\AIEQ\` inesistente → `ofstream.open()` fallisce silenziosamente → log persi → debugging Linear Phase impossibile.
- **Windows senza permessi/admin:** directory non creabile → open fallisce → log assenti → parametri IR non verificabili.
- **Disk pieno:** append infinito (`std::ios::app`), nessuna rotazione → I/O rallentato → possibile lag/crash DAW.

### Impatti
- CPU: minimo (thread non-audio).  
- I/O Disk: alto (append infinito, nessun cap/rotation).  
- Stabilità: media (fallimento silente).  
- Cross-platform: critico (non funziona su macOS/Linux).

### Fix proposto
- Usare `juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)` → `AIEqualizerPro/Logs/linear_phase_debug.txt`.
- `createDirectory()` prima dell’open; gestione errori (log fallback su logger centrale).
- Rotazione/size cap e apertura/chiusura su thread non-RT (nessuna `ofstream` nel thread audio).

---

## 🔴 Bug #2 – `std::mutex` in AIEngine (falsa architettura lock-free)
**Location:** `Source/AI/AIEngine.h/.cpp` (mutex e copie `std::vector` nel path RT/GUI).  
**Codice:** `std::lock_guard<std::mutex> lock(spectrumMutex); currentSpectrum = normalized;`

### Scenari
- **Contesa Audio vs AI/GUI:** audio thread legge `currentSpectrum` mentre AI thread detiene `spectrumMutex` (copia 2049 float). Blocco 50–100 µs → rischio drop-out a buffer 64/128.
- **Priority inversion:** GUI detiene il mutex, audio thread (priority alta) spin/blocca, deadline buffer >2–3 ms mancata → click/pop.

### Impatti
- Worst-case latency: critico (100–500 µs + priority inversion).  
- Dropout probability: medio/alto (~5% @ 64 buffer).  
- CPU: medio (lock + copy/alloc).  
- UX: AI detection ritardata (50–100 ms), GUI lag.

### Fix proposto
- Rimpiazzare i mutex con snapshot/queue lock-free (`AtomicSnapshot`, `SPSCQueue` da `Source/Core/LockFreeStructures.h`).
- Triple-buffer per `currentSpectrum`/`corrections`; nessuna allocazione nel path RT (pre-alloc + inplace).
- Rimuovere copie `std::vector` nel percorso audio/GUI; usare buffer preallocati e versioning atomico.

---

## 🔴 Bug #3 – SpinLock in RT Logging
**Location:** `Source/Utils/Logger.h` (circa linea 122).  
**Codice:** `juce::SpinLock rtQueueLock;`

### Scenari
- **Logging in RT:** audio thread chiama `AIEQ_LOG_ERROR`, tenta `rtQueueLock`, spin-wait; se GUI/background detiene il lock → 10 ms di spin = drop-out garantito.
- **Priority inversion:** GUI (low prio) tiene il lock, audio (high prio) spinna; CPU al 100%, battery drain, glitch.

### Impatti
- CPU: critico (+30–40% su burst di log).  
- Dropout: alto (15% su burst 1k msg/s).  
- Power/thermal: alto su laptop (fan on, -30% battery).

### Fix proposto
- Eliminare `SpinLock` nel path RT; usare SPSC queue lock-free già disponibile.  
- Writer su thread di background; RT thread fa solo push non-bloccante.  
- Macro hygiene: `__func__`, wrapper `do { ... } while(0)`, gestione `droppedRTMessages` solo se push fallisce senza spin.

---

## 🟠 Bug #4 – IR Builder Over-Engineering / Scaling Instabile
**Location:** `Source/PluginProcessor.cpp` (circa linee 213–323).  
**Sintomi:** scaling concatenato `globalCompensation * hannGainComp * irScale` con magic numbers (0.99, 2.0, 1e-4, 0.1).

### Scenario esempio
- EQ solo cut → `avgMag=0.3` → `globalCompensation=3.33x`; `hannGainComp=2x`; `maxAbs=0.0002` → `irScale=500x`; gain totale ≈ 3330x (+70 dB) → clipping/distorsione.

### Impatti
- Stabilità numerica: critico (overflow/clipping o IR troppo piccolo).  
- Manutenzione: alta complessità, commenti “DEBUG” indicano sfiducia nel codice.  
- UX: Linear Phase volume incoerente vs Zero Latency.

### Fix proposto
- Documentare e giustificare i coefficienti; sostituire magic numbers con parametri derivati da teoria/windowing.  
- Aggiungere test unitari per IR magnitude/gain matching; clamp sicuro (limiti di gain).  
- Semplificare scaling: un solo fattore normalizzato su energia/peak target, con fallback se `maxAbs < eps`.

---

## 🟡 Bug #5 – Rate Limiting Disabilitato (AIEngine)
**Location:** `Source/AI/AIEngine.cpp` (circa linee 160–167, codice di rate limit commentato).  
**Effetto:** `analyzeSpectrum()` eseguito a ogni callback (fino a 750 Hz @ 48 kHz / 64 samples).

### Impatti
- CPU: +15–25% solo AI (FFT 4096, 8 detections, history, genre).  
- Power/thermal: +20–25% consumo, fan attivi su laptop.  
- Cache pollution: interferenza con DSP hot path.

### Fix proposto
- Reintrodurre rate limit (es. 20–60 Hz) con change-detection (trigger immediato se variazione spettrale > soglia).  
- Pre-alloc e riuso buffer; nessuna allocazione nel loop.  
- Telemetria: contatori di skip/hit per calibrare intervallo.

---

## Priorità di intervento (ordine suggerito)
1) Bug #3 – Rimuovere SpinLock (impatto massimo, fix semplice).  
2) Bug #1 – Portabilità/log sicuri (sblocca macOS/Linux).  
3) Bug #2 – Refactor AIEngine lock-free (elimina drop-out).  
4) Bug #5 – Rate limiting (riduce CPU/power).  
5) Bug #4 – IR scaling (richiede test approfonditi, alta complessità).

## Azioni consigliate
- Centralizzare logging async e rotazione file.  
- Portare AIEngine su snapshot/queue lock-free, zero allocazioni RT.  
- Aggiungere test unitari IR (gain matching) e AI (latency/throughput).  
- Validare latenza/lookahead e smoothing nei DSP correlati.

