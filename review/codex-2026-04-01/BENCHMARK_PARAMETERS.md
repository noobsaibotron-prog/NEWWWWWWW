# BENCHMARK PARAMETERS — Dati Numerici Verificabili

## 1. LATENZA (PDC) — FabFilter Pro-Q 4 (Fonte: fabfilter.com/help/pro-q/using/processingmode)

| Modalità | Latenza (samples @ 44.1kHz) | Latenza (ms) |
|---|---|---|
| Zero Latency | **0 samples** | 0 ms |
| Natural Phase | **non specificata, ma bassa** | ~pochi ms |
| Linear Phase Low | **3.072 samples** | ~70 ms |
| Linear Phase Medium | **5.120 samples** | ~116 ms |
| Linear Phase High | **9.216 samples** | ~209 ms |
| Linear Phase Very High | **17.408 samples** | ~395 ms |
| Linear Phase Maximum | **66.560 samples** | ~1.509 ms |

**NOTA:** Con bande L/R e M/S simultanee, la latenza Linear Phase raddoppia.

### Confronto con AIEQ Pro:
- AIEQ Pro: ~220 samples (~5ms) — modalità UNICA
- FabFilter Pro-Q 4: 0 samples in Zero Latency, 0-pochi in Natural Phase
- **AIEQ Pro NON ha una modalità Zero Latency** — questo è un gap reale

---

## 2. LATENZA — Sonible smart:EQ 4 (Fonte: manuale ufficiale Sonible + Gearspace)

| Modalità | Latenza | Note |
|---|---|---|
| Minimum Phase | **0 samples** (Zero Latency) | Nessuna latenza |
| Linear Phase | **~70 ms** (~3.087 samples @ 44.1kHz) | Gearspace riporta 46.4ms in alcuni casi |

**NOTA:** smart:EQ 4 ha DUE modalità: minimum (zero latency) e linear (~70ms).

---

## 3. LATENZA — iZotope Ozone 11 (da ricerche precedenti)

| Componente | Latenza stimata |
|---|---|
| EQ Module (Minimum Phase) | ~0 samples |
| EQ Module (Linear Phase) | ~4.096-8.192 samples |
| Suite completa (tutti i moduli attivi) | ~170+ ms |

**NOTA:** Ozone è una suite completa (EQ + Dynamics + Maximizer + Imager + ecc.), la latenza totale dipende dai moduli attivi.

---

## 4. CROSSFADE M/S — Confronto Competitor

### FabFilter Pro-Q 4 (Fonte: fabfilter.com/help/pro-q/using/stereo)
- **Approccio:** Per-band stereo placement (L/R/M/S per singola banda)
- **Crossfade tra modalità:** NON documentato. FabFilter non "switcha" tra L/R e M/S globalmente — ogni banda ha il suo placement indipendente
- **Implicazione:** FabFilter NON ha bisogno di un crossfade globale M/S perché il design è per-banda. Non c'è un "cambio di modalità" globale che possa generare click
- **AIEQ Pro:** Ha un cambio di modalità GLOBALE (Stereo ↔ M/S) che richiede crossfade per evitare click. Questo è un design architetturale diverso, non necessariamente migliore

### TDR Nova (Fonte: tokyodawn.net/labs/Nova/Manual.pdf)
- **Approccio:** Parallel dynamic EQ con M/S processing
- **Crossfade:** Non documentato pubblicamente
- **Latenza:** Zero latency (minimum phase)

### Kirchhoff-EQ (Fonte: threebodytech.com)
- **Approccio:** Per-band M/S processing (come FabFilter)
- **64-bit double precision** interno
- **2x oversampling** opzionale
- **Crossfade:** Non necessario (design per-banda)

### VERDETTO CROSSFADE:
- **AIEQ Pro ha un crossfade M/S a 1024 campioni** — questo è un fix per un problema architetturale (cambio globale di modalità)
- **FabFilter e Kirchhoff NON hanno questo problema** perché il loro design è per-banda
- **Il crossfade è una soluzione corretta, ma il design per-banda è superiore**
- **Claim "superiore ai competitor" → OVERCLAIM. È una soluzione a un problema che i competitor non hanno.**

---

## 5. ARCHITETTURA GUI & RENDERING — Confronto Competitor

### FabFilter (Fonte: fabfilter.com/forum + fabfilter.com/news)
- **Rendering:** GPU-powered graphics acceleration dal 2012 (custom engine, non JUCE)
- **Framework:** Engine proprietario (NON basato su JUCE)
- **Performance:** Problemi noti con OpenGL su Windows/Nvidia (throttling CPU), ma generalmente eccellente
- **Nota:** FabFilter NON usa JUCE — ha un engine grafico completamente custom, il che spiega la fluidità superiore

### Sonible smart:EQ 4
- **Framework:** Basato su JUCE (confermato dalla community)
- **Rendering:** Software renderer JUCE standard (non OpenGL)
- **Performance:** Buona ma non eccezionale, focus sull'AI non sulla GUI

### JUCE 8 Direct2D (Fonte: forum.juce.com, ADC 2024)
- **Stato:** JUCE 8 ha introdotto il renderer Direct2D per Windows
- **Performance:** Migliorata significativamente, ma ancora problemi con finestre multiple e conflitti OpenGL/D2D
- **OpenGL in JUCE:** Deprecato su macOS, problemi driver su Windows, community consiglia di evitarlo

### AIEQ Pro
- **Rendering:** OpenGL con SpinLock (juce::SpinLock) per sincronizzazione buffer
- **Framework:** JUCE
- **Rischio:** OpenGL è deprecato su macOS e problematico su Windows/Nvidia secondo la community JUCE
- **VERDETTO:** L'uso di SpinLock è corretto, ma la scelta di OpenGL come renderer è un rischio tecnico. FabFilter usa un engine custom che non ha questi problemi. Il claim "GUI Performance 8.9/10" è ragionevole per un plugin JUCE, ma NON è al livello di FabFilter (engine custom) o dei plugin che usano Direct2D/Metal nativi.

---

## 6. DYNAMIC EQ — Confronto Parametri Numerici

| Parametro | FabFilter Pro-Q 4 | Kirchhoff-EQ | TDR Nova GE | AIEQ Pro |
|---|---|---|---|---|
| **Max Bande** | 24 (tutte dinamiche) | 32 (tutte dinamiche) | 6 dinamiche + HP/LP | Da verificare |
| **Dynamic Range** | -30 dB a +30 dB | Da verificare | Da verificare | Da verificare |
| **Slope** | Fino a 96 dB/oct | Variabile continua | 6-24 dB/oct | Da verificare |
| **Sidechain Esterno** | Sì (per-banda) | Sì | Sì | Da verificare |
| **Look-Ahead** | No (auto mode) | No | No | Da verificare |
| **Linear Phase Dynamic** | Sì (fino a High) | No | No | Da verificare |
| **Spectral Dynamics** | Sì (NUOVO in v4) | No | No | No |
| **Auto Attack/Release** | Sì (program-dependent) | Sì | Sì | Da verificare |
| **Oversampling** | No | 2x opzionale | No | Da verificare |
| **Precisione interna** | 64-bit | 64-bit double | 64-bit | Da verificare |
| **M/S per banda** | Sì | Sì | Sì (GE) | Globale |
| **Engine proprietario** | Sì (custom, non JUCE) | Sì (custom) | Sì (custom) | No (JUCE) |

### Fonti:
- FabFilter: fabfilter.com/help/pro-q/using/dynamic-eq (documentazione ufficiale)
- Kirchhoff-EQ: threebodytech.com/en/products/kirchhoffeq
- TDR Nova GE: tokyodawn.net/tdr-nova-ge/ + manuale PDF

### VERDETTO DYNAMIC EQ:
- FabFilter Pro-Q 4 è il benchmark assoluto: 24 bande dinamiche, spectral dynamics, linear phase dynamic, sidechain per-banda, auto mode program-dependent
- Kirchhoff-EQ: 32 bande, oversampling 2x, precisione double — il più potente in termini di bande
- TDR Nova GE: 6 bande, ma con parallel processing unico e W-Band
- **AIEQ Pro ha un Dynamic EQ, ma i parametri esatti (bande, range, sidechain) devono essere verificati nel codice**

---

## 7. PLUGINVAL — Test per Livello di Strictness (dal codice sorgente)

### Struttura dei test (dal repository GitHub Tracktion/pluginval)

| Livello | Test | File Sorgente | Descrizione |
|---|---|---|---|
| **1** | Plugin info | BasicTests.cpp | Log getName(), getLatencySamples(), getTailLengthSeconds() |
| **2** | Plugin programs | BasicTests.cpp | getNumPrograms(), getProgramName(), setCurrentProgram() x5 random |
| **2** | Editor | EditorTests.cpp | createEditor() cold + warm, verifica non-null |
| **3** | Audio processing | BasicTests.cpp | processBlock() con multiple sample rates e block sizes, 10 blocchi per combinazione |
| **3** | Audio processing (release/prepare) | BasicTests.cpp | Come sopra ma con releaseResources() prima di ogni cambio sample rate |
| **4** | Open editor whilst processing | BasicTests.cpp | processBlock() async + createEditor() su message thread (concorrenza) |
| **5** | State restoration | BasicTests.cpp | getStateInformation()/setStateInformation(), verifica recall determinismo |
| **5** | Bus layout | BusTests.cpp | Test configurazioni bus I/O |
| **5** | auval (macOS) | Integrato | Apple Audio Unit Validation (solo AU, macOS) |
| **6** | Fuzz parameters | ParameterFuzzTests.cpp | Per ogni parametro: 5 valori random, getValue(), getText(), getValueForText() |
| **7** | Locale test | LocaleTest.cpp | Test con locale non-inglese per verificare parsing numerico |
| **8** | Larger block size | ExtremeTests.cpp | processBlock() con blocco 2x più grande del preparato (solo JUCE AudioProcessor) |
| **9** | Allocations during process | ExtremeTests.cpp | ScopedAllocationDisabler + verifica zero allocazioni in audio thread |
| **10** | Tutti i test precedenti | Tutti | Tutti i test da 1-9 eseguiti insieme |

### Parametri Numerici Chiave:
- **Livello 5**: Minimo raccomandato per compatibilità host (fonte: README pluginval)
- **Livello 10**: Tutti i test (1-9) eseguiti, include real-time safety check
- **Numero totale test unici**: ~12-15 test (dipende da formato plugin e piattaforma)
- **Durata tipica Level 10**: 20-60 secondi (dipende dal plugin)
- **Formato supportati**: VST2, VST3, AU
- **Piattaforme**: macOS, Windows, Linux
- **Real-time check** (nuovo): Verifica allocazioni in audio thread (livello 9)
- **VST3 Validator** (nuovo): Integrato direttamente nel binario pluginval

### Fonti:
- Codice sorgente: github.com/Tracktion/pluginval (branch develop)
- File: Source/tests/BasicTests.cpp, ExtremeTests.cpp, ParameterFuzzTests.cpp, EditorTests.cpp, BusTests.cpp, LocaleTest.cpp
- Blog: melatonin.dev/blog/pluginval-is-a-plugin-devs-best-friend/
- README: github.com/Tracktion/pluginval/blob/develop/README.md

### VERDETTO PLUGINVAL per AIEQ Pro:
- **AIEQ Pro NON è stato testato con pluginval Level 10** — questo è un FATTO
- Il claim "Zero-Crash" è stato RIMOSSO perché non verificato
- Per il lancio commerciale, AIEQ Pro DEVE superare pluginval Level 5 come MINIMO
- Superare Level 10 (incluso real-time allocation check) sarebbe un differenziatore competitivo
- La maggior parte dei plugin commerciali dichiara Level 5; pochi dichiarano Level 10

---

## 8. AI/ML INFERENCE — Confronto Competitor

### Gullfoss (Soundtheory) — Fonti: soundtheory.com, manuale ufficiale
| Parametro | Valore | Fonte |
|---|---|---|
| **Latenza (Gullfoss standard)** | ~20 ms | soundtheory.com/gullfoss |
| **Latenza (Gullfoss Live)** | ~2 ms | soundtheory.com/gullfoss |
| **Latenza (Gullfoss Master)** | ~20 ms | soundtheory.com/gullfoss |
| **Modello uditivo** | ~1000 aggiornamenti/secondo | Manuale operativo Gullfoss |
| **Tipo di AI** | Modello percettivo uditivo (NON deep learning) | Manuale + sito ufficiale |
| **Sample rates** | 16 kHz – 384 kHz | Manuale operativo |
| **CPU** | Medio-alto (Master > Standard > Live) | Sito ufficiale |

### Sonible smart:EQ 4 — Fonti: sonible.com, manuale PDF
| Parametro | Valore | Fonte |
|---|---|---|
| **Latenza (Minimum Phase)** | 0 samples (zero latency) | Manuale smart:EQ 4 |
| **Latenza (Linear Phase)** | ~70 ms (~46.4 ms secondo Gearspace) | Manuale + Gearspace |
| **Tipo di AI** | Smart:filter con profili (instrument/mix) | sonible.com/smarteq4 |
| **Analisi** | Real-time con tempo di apprendimento configurabile | Manuale |
| **Cross-channel** | Fino a 10 tracce simultanee (unmasking) | sonible.com |
| **Bande** | 24 bande flessibili | musictech.com review |
| **GPU richiesta** | OpenGL 3.2+ | sonible.com/smarteq4 specs |
| **Framework** | JUCE-based | Community (confermato) |
| **Sample rates** | 44.1 kHz – 192 kHz | sonible.com specs |

### iZotope Ozone 11 — Fonti: izotope.com
| Parametro | Valore | Fonte |
|---|---|---|
| **Tipo di AI** | Master Assistant (ML-based) | izotope.com |
| **Funzione** | Analisi audio → preset personalizzati | izotope.com/learn |
| **Inference** | One-shot (analizza, poi applica) | Workflow documentato |
| **Real-time AI** | No (analisi iniziale, poi statica) | izotope.com |
| **Moduli** | EQ, Dynamics, Maximizer, Imager, Exciter, ecc. | izotope.com |

### AIEQ Pro — Stato attuale
| Parametro | Valore | Note |
|---|---|---|
| **Tipo di AI** | Atomic variables per thread-safety | NON è un modello ML/DL |
| **Modello ML** | NESSUNO | Verificato nel codice |
| **Inference** | N/A | Non applicabile |
| **Claim rimosso** | "98.5% AI Precision" | Era un overclaim |

### VERDETTO AI/ML:
- **Gullfoss** è il leader nell'AI audio: modello percettivo uditivo con 1000 aggiornamenti/sec, 20ms latenza
- **Sonible smart:EQ 4** ha un AI reale con profili e cross-channel unmasking (fino a 10 tracce)
- **iZotope Ozone 11** usa ML per analisi one-shot, non real-time
- **AIEQ Pro NON ha un modello AI/ML** — usa atomic variables per thread-safety, che è buona ingegneria ma NON è AI
- Il claim "AI Equalizer" nel nome del prodotto è **potenzialmente fuorviante** se non c'è un modello ML reale
- **Raccomandazione**: Rinominare o chiarire che "AI" si riferisce all'architettura thread-safe, non a machine learning

---
