# AI Equalizer Pro

**Advanced AI-Powered Parametric Equalizer — VST3 / AU / Standalone**

Production-grade audio plugin con analisi AI real-time, dynamic EQ, linear phase processing, semantic control via linguaggio naturale e architettura lock-free per il thread audio.

---

## Panoramica

AI Equalizer Pro è un equalizzatore parametrico fino a 24 bande con intelligenza artificiale integrata. Analizza lo spettro in tempo reale, rileva problemi frequenziali (risonanze, harshness, muddiness…) e suggerisce — o applica automaticamente — correzioni EQ precise. Dispone anche di un Dynamic EQ per-band, linear phase processing con partitioned convolution, semantic control (linguaggio naturale → EQ) e supporto OSC per automazione esterna.

---

## Funzionalità Principali

### EQ Parametrico (8–24 Bande)

Ogni banda supporta 9 tipi di filtro: LowCut, LowShelf, Peak, HighShelf, HighCut, Notch, BandPass, Vintage LowShelf (Pultec-style) e Vintage HighShelf. Le slope per LowCut/HighCut sono configurabili a 12/24/48 dB/oct con anti-cramping. Tutti i parametri sono lock-free con version counter, e le variazioni vengono smoothate per eliminare zipper noise durante l'automazione.

### Dynamic EQ (FabFilter Pro-Q / TDR Nova Style)

Ogni banda può operare in modalità statica o dinamica. Le modalità dinamiche disponibili sono Compress, Expand e Gate. Parametri per-band: threshold, ratio, attack, release, range e knee con soft-knee. Supporta detection in modalità Peak o RMS, sidechain filtering per banda, lookahead opzionale, auto-makeup gain e dry/wet mix globale. Tutti i meter (input level, gain reduction, output level) sono thread-safe e accessibili dalla GUI.

### Linear Phase Processing

Il processore linear phase utilizza partitioned convolution con blocchi da 128 sample (latenza di soli 128 sample anziché 4096 del classico OLA). L'IR zero-phase viene costruito dal magnitude response con windowing Hann e gain compensation. Il sistema usa doppio slot IR (A/B) con crossfade click-free durante il cambio di IR, e la costruzione avviene in un thread background dedicato con debounce di 80ms.

### Tre Modalità di Fase

Il plugin offre tre modalità: Zero-Latency (IIR standard, nessuna latenza aggiuntiva), Natural Phase (oversampling 2x/4x con anti-aliasing) e Linear Phase (partitioned convolution).

### Elaborazione Mid/Side

Quattro modalità di processing stereo: Stereo standard, Mid-Only, Side-Only e MS-Linked. I buffer M/S sono pre-allocati e allineati a 64 byte per AVX-512.

### AI Engine — Rilevamento Problemi Intelligente

Il motore AI analizza lo spettro e rileva automaticamente 8 tipi di problemi: Resonance, Harshness, Muddiness, Boxyness, Sibilance, Low-End Boom, Thin Sound e Dull Sound. Il sistema utilizza multi-scale peak detection, smoothing temporale su 3 frame, soglie adattive basate sul livello RMS, calcolo preciso della bandwidth via punti a -3dB, z-score locale, analisi armonica, coerenza spettrale e normalizzazione per dynamic range. L'analisi gira su un thread dedicato con comunicazione lock-free (SPSC queue) verso l'audio thread.

### Source Profiles

8 profili ottimizzati per diverse sorgenti: Generic, Vocals, Drums, Bass, Synth, Master, EDM e Techno. Ogni profilo adatta le soglie di detection e i range di frequenza alla sorgente specifica.

### Semantic EQ — Linguaggio Naturale → EQ

Il motore semantico traduce descrittori timbrici in regolazioni EQ precise. 28 qualità semantiche supportate, tra cui Air, Brilliance, Warmth, Punch, Clarity, Smoothness, Weight, Vintage e altre. Supporta input in linguaggio naturale multilingua (inglese e italiano), con word embeddings (GloVe, 50 dimensioni) e decoder seq2seq opzionale. Il sistema è context-aware: analizza il contenuto spettrale corrente per adattare le mappature. Supporta morphing smooth tra stati semantici e apprendimento dalle correzioni manuali dell'utente.

### Reference Track Matching

Sistema per confrontare lo spettro della traccia corrente con una traccia di riferimento e generare correzioni EQ per avvicinare il bilanciamento tonale.

### User Learning System

Il plugin impara dalle preferenze dell'utente nel tempo, adattando le correzioni AI in base allo storico delle regolazioni manuali.

### A/B/C/D Comparison

4 slot di memoria per confrontare impostazioni EQ. Switching istantaneo, copia e swap tra qualsiasi combinazione di slot.

### Auto-Gain

Compensazione automatica del volume basata su RMS. Smoothing per evitare artefatti, range limitato ±12 dB. Calcolato ogni 4 blocchi audio per efficienza.

### Analizzatore di Spettro Pre/Post EQ

Spettro FFT in tempo reale con overlay pre-EQ e post-EQ. Peak hold con decay configurabile. Risoluzione e velocità regolabili.

### Solo Acustico per Banda

Modalità solo che isola una singola banda EQ con filtro bandpass e crossfade di 256 sample all'attivazione/disattivazione. Include makeup gain di +6 dB.

### OSC Parameter Server

Server OSC integrato (porta UDP 11100, risposte su 11101) per controllo remoto di tutti i parametri. Thread dedicato, nessuna dipendenza dal message loop JUCE.

### Undo/Redo

Sistema undo/redo completo con stack di operazioni e descrizioni. Integrato con HistoryManager thread-safe.

### Preset Manager

Gestione preset completa con salvataggio/caricamento stato.

### GPU Compositing

La GUI utilizza OpenGL context su tutto l'editor per GPU compositing accelerato. Tutte le componenti figlie ne beneficiano automaticamente.

---

## Architettura

```
Source/
├── PluginProcessor.cpp/h          # Core: processBlock, A/B/C/D, Auto-Gain, M/S, fase
├── PluginEditor.cpp/h             # GUI TDR Nova style con OpenGL compositing
├── AI/
│   ├── AIEngine.cpp/h             # Rilevamento problemi, source profiles, triple-buffer
│   ├── AIEngine_Advanced.cpp      # Detection avanzata: z-score, armonica, coerenza
│   ├── SemanticEQEngine.cpp/h     # NLP → EQ, embeddings GloVe, seq2seq
│   ├── ReferenceMatcher.cpp/h     # Reference track matching
│   ├── UserLearning.cpp/h         # Apprendimento preferenze utente
│   ├── MLEngine.cpp/h             # ML detection (heuristic attiva, model in sviluppo)
│   ├── MultiTrackUnmasking.cpp/h  # Analisi multi-traccia per unmasking
│   ├── NeuralNetworkWrapper.cpp/h # Wrapper reti neurali (TFLite)
│   ├── AdaptiveAIEngine.cpp/h     # Analisi adattiva del segnale
│   └── OnlineLearningSystem.cpp/h # Apprendimento online continuo
├── DSP/
│   ├── ParametricEQProcessor.cpp/h   # EQ parametrico 24 bande, lock-free
│   ├── DynamicEQProcessor.cpp/h      # Dynamic EQ per-band (compress/expand/gate)
│   ├── LinearPhaseProcessor.cpp/h    # Convolution OLA + partitioned (128 sample)
│   ├── PartitionedConvolver.h        # Partitioned convolution engine
│   └── SpectrumAnalyzer.cpp/h        # FFT analyzer con peak hold
├── Core/
│   ├── LockFreeStructures.h       # SPSC queue, AICommandQueue, cache-line alignment
│   ├── CaptureService.h           # Audio capture lock-free
│   ├── HistoryManager.h           # Undo/redo thread-safe
│   └── OSCParameterServer.h       # Server OSC UDP (porta 11100)
├── GUI/
│   ├── AdvancedSpectrumDisplay.h  # Spettro pre/post con peak hold
│   ├── AIControlPanel.h           # Pannello AI e source profiles
│   ├── AIProblemPanel.h           # Problemi rilevati con severity indicator
│   ├── BandControlPanel.h        # Dettaglio banda selezionata
│   ├── BandViewport.cpp/h         # Viewport scrollabile per bande
│   ├── DynamicEQPanel.h           # Controlli dynamic EQ
│   ├── EQBandControl.h            # Widget singola banda
│   ├── ModernLookAndFeel.h        # Look & feel custom
│   ├── PremiumKnob.h              # Knob stile premium
│   └── SemanticControlPanel.h     # Pannello semantic EQ
├── Utils/
│   ├── Logger.cpp/h               # Logging system
│   └── PresetManager.cpp/h        # Gestione preset
└── Tests/
    ├── ParametricEQTest.cpp
    ├── LinearPhaseIRSmokeTest.cpp
    ├── LinearPhaseGainRegressionTest.cpp
    ├── DynamicEQRegressionTest.cpp
    ├── FuzzBlockSizeTest.cpp
    └── TestMain.cpp
```

---

## Thread Model

Il plugin segue un'architettura multi-thread production-grade:

**Audio Thread** — processBlock, operazioni real-time safe. Zero mutex, zero allocazioni. Parametri letti da snapshot atomici. Comunicazione con l'AI via SPSC queue.

**Message Thread** — GUI, cambio parametri, undo/redo. Scrive su APVTS per host automation.

**IR Builder Thread** — Costruzione IR linear phase in background con std::jthread e stop_token (RAII). Debounce automatico a 80ms.

**AI Analysis Thread** — Analisi spettrale e detection problemi. Risultati inviati all'audio thread via AICommandQueue lock-free.

**OSC Server Thread** — Ricezione/risposta comandi OSC su thread dedicato.

---

## Problemi Rilevati dall'AI

| Tipo | Range Frequenze | Descrizione |
|------|-----------------|-------------|
| Resonance | 20–20000 Hz | Picchi stretti pronunciati |
| Harshness | 1–8 kHz | Eccesso alte frequenze medio-alte |
| Muddiness | 200–500 Hz | Accumulo basse-medie |
| Boxyness | 400–800 Hz | Suono "scatolato" |
| Sibilance | 5–10 kHz | Eccesso sibilanti |
| Low-End Boom | 30–100 Hz | Accumulo sub/bassi |
| Thin Sound | 200–600 Hz | Mancanza di corpo |
| Dull Sound | 8–16 kHz | Mancanza di brillantezza |

---

## Build

### Requisiti

- C++20 compiler (MSVC 2022+, Clang, GCC)
- CMake 3.22+
- JUCE (incluso nella repo sotto `JUCE/`)
- TensorFlow Lite C API (Windows, incluso sotto `ThirdParty/TFLite/`)
- LibTorch (opzionale, attivabile con `-DAIEQ_ENABLE_TORCH=ON`)

### Build — Windows

```batch
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Oppure usa gli script forniti: `build_fix.cmd`, `FIX_RELEASE_BUILD.bat`.

Il CMake gestisce automaticamente il fallback da VS 18/2026 a VS 17/2022, e da Visual Studio a Ninja/MinGW se non disponibile.

### Build — macOS

```bash
chmod +x setup_mac.sh build_mac.sh
./setup_mac.sh
./build_mac.sh
```

Build universale (Intel + Apple Silicon) con deployment target macOS 11.0. Formato AU generato automaticamente su macOS oltre a VST3 e Standalone.

### Output

```
build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3
build/AIEqualizerPro_artefacts/Release/AU/AI Equalizer Pro.component   # solo macOS
build/AIEqualizerPro_artefacts/Release/Standalone/AI Equalizer Pro     # standalone app
```

### Installazione

**Windows:**
Il build copia automaticamente il VST3 in `%LOCALAPPDATA%\VST3\` (no admin) e tenta anche `C:\Program Files\Common Files\VST3\` (richiede admin).

**macOS:**
- `/Library/Audio/Plug-Ins/VST3/`
- `/Library/Audio/Plug-Ins/Components/` (AU)

### Test

```bash
cd build
ctest --output-on-failure
```

I test DSP includono: ParametricEQ, LinearPhase IR smoke test, LinearPhase gain regression, DynamicEQ regression e fuzz test con block size variabile.

---

## Parametri AI

- **Sensitivity (0–100%)** — Più alto = rileva anche problemi minori. Curva esponenziale per controllo fine.
- **Strength (0–100%)** — Intensità delle correzioni applicate.
- **Source Profile** — Adatta soglie e range al tipo di sorgente (Generic, Vocals, Drums, Bass, Synth, Master, EDM, Techno).
- **Correction Mode** — Off, Suggest, Gradual, Automatic.

---

## OSC Remote Control

Il plugin espone un server OSC sulla porta UDP 11100:

```
/aieq/ping            → risponde /aieq/pong
/aieq/list            → lista tutti i parametri con valori
/aieq/get   <id>      → valore corrente del parametro
/aieq/set   <id> <v>  → imposta valore normalizzato (0–1)
/aieq/set/denorm <id> <v> → imposta valore reale (denormalizzato)
```

Risposte su porta 11101. Utilizzabile con TouchOSC, Max/MSP, Pure Data o qualsiasi client OSC.

---

## Tecnologie

- **JUCE** — Framework audio cross-platform (moduli: audio_basics, audio_devices, audio_formats, audio_plugin_client, audio_processors, audio_utils, core, data_structures, dsp, events, graphics, gui_basics, gui_extra, osc, opengl)
- **C++20** — std::jthread, stop_token, [[nodiscard]], std::span, constexpr
- **Lock-free DSP** — SPSC queues, triple-buffer, atomic version counters, cache-line alignment (64 byte)
- **TensorFlow Lite** — Inferenza ML opzionale (Windows)
- **LibTorch** — Seq2seq parser opzionale per semantic engine
- **OpenGL** — GPU compositing per GUI

---

## Licenza

Progetto educativo/personale. JUCE richiede licenza per uso commerciale.

---

## Credits

Sviluppato con assistenza AI (Claude + ChatGPT) per Marco — Sound Designer & Music Producer.

Generi ottimizzati: Industrial, Techno, Jungle, Breakbeat, Dub Techno, Deep House.
