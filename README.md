# AI Equalizer Pro

**Advanced AI-Powered Parametric Equalizer — VST3 / AU / Standalone**

Plugin audio cross-platform con EQ parametrico fino a 24 bande, Dynamic EQ per-band, tre modalità di fase (Zero / Natural / Linear), analisi AI in tempo reale, Semantic EQ, processing Mid/Side e architettura pensata per tenere il thread audio real-time safe.

---

## Stato attuale del progetto

**Baseline consigliata attuale:** branch `debug/host-clicks-real`

Stato verificato il **2026-03-28**:
- build `Release` riuscita su macOS
- VST3 installabile e caricabile in host
- validazione host-side in Ableton migliorata sui casi più critici:
  - semantic drag
  - linear phase drag
  - bypass
  - oversampling
  - A/B switch
  - AI Problem Panel / single correction
- regression principali verdi sulla stessa baseline:
  - `aieq_integration_regression`
  - `aieq_dsp_regression`

Questo README descrive **lo stato reale attuale della repo**, non una wishlist.

---

## Panoramica

AI Equalizer Pro è un equalizzatore parametrico con AI integrata. Analizza lo spettro in tempo reale, rileva problemi frequenziali come risonanze, harshness, muddiness e altre anomalie tonali, poi suggerisce o applica correzioni EQ. Include anche Dynamic EQ, Linear Phase processing, Semantic Control tramite linguaggio naturale, A/B/C/D comparison, auto-gain, analizzatore di spettro pre/post e controllo OSC.

---

## Funzionalità principali

### EQ parametrico (8–24 bande)

Ogni banda supporta 9 tipi di filtro:
- LowCut
- LowShelf
- Peak
- HighShelf
- HighCut
- Notch
- BandPass
- Vintage LowShelf
- Vintage HighShelf

Per LowCut / HighCut sono disponibili slope a:
- 12 dB/oct
- 24 dB/oct
- 48 dB/oct

Il path audio usa smoothing sui parametri e strutture lock-free / atomiche per ridurre zipper noise e glitch durante automazione e editing live.

### Dynamic EQ per-band

Ogni banda può lavorare in modalità statica o dinamica.

Modalità dinamiche disponibili:
- Compress
- Expand
- Gate

Parametri per banda:
- threshold
- ratio
- attack
- release
- range
- knee

Sono presenti meter thread-safe per:
- input level
- gain reduction
- output level

### Tre modalità di fase

Il plugin offre tre modalità operative:
- **Zero Latency** — path IIR standard, latenza minima
- **Natural Phase** — oversampling 2x / 4x con path HQ
- **Linear Phase** — convolution con partitioned convolver

### Linear Phase processing

Il path linear phase utilizza partitioned convolution con blocchi da **128 sample**. L'IR viene costruita da una magnitude response in background, con debounce sulle richieste di rebuild. Il sistema usa handoff/crossfade tra IR vecchia e nuova per evitare artefatti nei cambi durante playback.

### Mid/Side processing

Modalità stereo disponibili:
- Stereo
- Mid Only
- Side Only
- MS Linked

### AI Engine — rilevamento problemi intelligenti

L'AI engine rileva automaticamente problemi tonali come:
- Resonance
- Harshness
- Muddiness
- Boxyness
- Sibilance
- Low-End Boom
- Thin Sound
- Dull Sound

Il motore usa detection multi-scale, smoothing temporale, soglie adattive e comunicazione lock-free tra thread di analisi e audio thread.

### Source Profiles

Profili sorgente disponibili:
- Generic
- Vocals
- Drums
- Bass
- Synth
- Master
- EDM
- Techno

### Semantic EQ — linguaggio naturale → EQ

Il motore semantico traduce descrittori timbrici in regolazioni EQ.

Supporta qualità come:
- Air
- Brilliance
- Warmth
- Punch
- Clarity
- Smoothness
- Weight
- Vintage
- e altre

Supporta parsing in inglese e italiano, mapping context-aware e morphing smooth tra stati semantici.

### Reference Track Matching

Confronta lo spettro corrente con una reference track per generare correzioni tonali mirate.

### User Learning System

Il plugin può apprendere progressivamente dalle correzioni manuali e dai pattern d'uso dell'utente.

### A/B/C/D comparison

4 slot di memoria per confrontare settaggi diversi.

Supporta:
- switch tra slot
- copy tra slot
- swap tra slot

### Auto-Gain

Compensazione automatica RMS-based con smoothing e range limitato.

### Analizzatore di spettro pre/post EQ

Spettro FFT in tempo reale con overlay pre/post, peak hold, risoluzione configurabile e velocità configurabile.

### Solo acustico per banda

Modalità solo con band-pass dedicato e crossfade di attivazione/disattivazione per ridurre click.

### OSC Parameter Server

Server OSC integrato per controllo remoto dei parametri.

### Undo/Redo e preset

Sono inclusi:
- HistoryManager per undo/redo
- PresetManager per gestione preset

### GUI con compositing accelerato

La GUI usa OpenGL context sull'editor per compositing accelerato.

---

## Architettura

```text
Source/
├── PluginProcessor.cpp/h          # Core: processBlock, phase modes, A/B, auto-gain, M/S
├── PluginEditor.cpp/h             # GUI principale
├── AI/
│   ├── AIEngine.cpp/h
│   ├── AIEngine_Advanced.cpp
│   ├── SemanticEQEngine.cpp/h
│   ├── ReferenceMatcher.cpp/h
│   ├── UserLearning.cpp/h
│   ├── MLEngine.cpp/h
│   ├── MultiTrackUnmasking.cpp/h
│   ├── NeuralNetworkWrapper.cpp/h
│   ├── AdaptiveAIEngine.cpp/h
│   └── OnlineLearningSystem.cpp/h
├── DSP/
│   ├── ParametricEQProcessor.cpp/h
│   ├── DynamicEQProcessor.cpp/h
│   ├── LinearPhaseProcessor.cpp/h
│   ├── PartitionedConvolver.h
│   └── SpectrumAnalyzer.cpp/h
├── Core/
│   ├── LockFreeStructures.h
│   ├── CaptureService.h
│   ├── HistoryManager.h
│   └── OSCParameterServer.h
├── GUI/
│   ├── AdvancedSpectrumDisplay.h
│   ├── AIControlPanel.h
│   ├── AIProblemPanel.h
│   ├── BandControlPanel.h
│   ├── BandViewport.cpp/h
│   ├── DynamicEQPanel.h
│   ├── EQBandControl.h
│   ├── ModernLookAndFeel.h
│   ├── PremiumKnob.h
│   └── SemanticControlPanel.h
├── Utils/
│   ├── Logger.cpp/h
│   └── PresetManager.cpp/h
└── Tests/
    ├── ParametricEQTest.cpp
    ├── LinearPhaseIRSmokeTest.cpp
    ├── LinearPhaseGainRegressionTest.cpp
    ├── DynamicEQRegressionTest.cpp
    ├── FuzzBlockSizeTest.cpp
    ├── HostSessionClickTest.cpp
    ├── LinearPhaseLatencyContractTest.cpp
    ├── BlockSizeRegressionTest.cpp
    └── TestMain.cpp
```

---

## Thread model

Il plugin segue un modello multi-thread separando responsabilità principali:

- **Audio Thread**
  - `processBlock`
  - operazioni real-time safe
  - no mutex nel path audio
  - parametri letti via snapshot / atomiche

- **Message Thread**
  - GUI
  - scrittura parametri/APVTS
  - undo/redo

- **IR Builder Thread**
  - rebuild IR per linear phase
  - debounce automatico prima del rebuild

- **AI Analysis Thread**
  - analisi spettrale e detection problemi
  - handoff verso il resto del sistema via strutture lock-free

- **OSC Thread**
  - ricezione e risposta OSC

---

## Build

### Requisiti

- C++20 compiler
- CMake 3.22+
- JUCE incluso nella repo (`JUCE/`)
- TensorFlow Lite C API (opzionale / configurazione dipendente dal target)
- LibTorch opzionale (`-DAIEQ_ENABLE_TORCH=ON`)

### Build — macOS

Sono presenti script helper:

```bash
chmod +x setup_mac.sh build_mac.sh
./setup_mac.sh
./build_mac.sh
```

Oppure build diretto con CMake:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 4
```

### Build — Windows

```bat
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Sono presenti anche script helper come:
- `build_fix.cmd`
- `FIX_RELEASE_BUILD.bat`

### Output tipico

Su macOS il build release recente produce artefatti in:

```text
build-release/Release/lib/AI Equalizer Pro.vst3
build-release/Release/lib/AI Equalizer Pro.component
build-release/Release/bin/AI Equalizer Pro.app
```

In alcuni flussi di packaging trovi anche gli artefatti copiati sotto:

```text
build-release/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3
```

**Nota pratica importante:** per i test host recenti il VST3 corretto è stato preso da:

```text
build-release/Release/lib/AI Equalizer Pro.vst3
```

---

## Installazione

### macOS

Posizioni standard:
- `~/Library/Audio/Plug-Ins/VST3/`
- `/Library/Audio/Plug-Ins/VST3/`
- `/Library/Audio/Plug-Ins/Components/` (AU)

### Windows

Posizioni comuni:
- `%LOCALAPPDATA%\VST3\`
- `C:\Program Files\Common Files\VST3\`

---

## Test

### Elenco test principali

La build recente espone almeno questi target ctest:

- `aieq_dsp_regression`
- `aieq_ai_regression`
- `aieq_integration_regression`
- `aieq_performance`

### Esecuzione

```bash
ctest --test-dir build-release --output-on-failure
```

Esempi mirati:

```bash
ctest --test-dir build-release --output-on-failure -R aieq_integration_regression
ctest --test-dir build-release --output-on-failure -R aieq_dsp_regression
```

### Stato verificato recentemente

Su macOS, nella baseline valida del 2026-03-28:
- `aieq_integration_regression` ✅
- `aieq_dsp_regression` ✅

---

## Parametri AI principali

- **Sensitivity** — sensibilità del rilevamento problemi
- **Strength** — intensità delle correzioni suggerite/applicate
- **Source Profile** — adatta detection e comportamento alla sorgente

---

## OSC remote control

Il server OSC espone comandi del tipo:

```text
/aieq/ping
/aieq/list
/aieq/get <id>
/aieq/set <id> <v>
/aieq/set/denorm <id> <v>
```

Porta prevista nel codice:
- richieste su UDP `11100`
- risposte su `11101`

---

## Tecnologie

- **JUCE**
- **C++20**
- **Lock-free DSP structures**
- **OpenGL** per GUI compositing
- **TensorFlow Lite** opzionale
- **LibTorch** opzionale

---

## Limitazioni / note oneste

- Il progetto è ambizioso e ampio: alcune aree sono più mature di altre.
- La verità finale per i problemi audio interattivi resta il comportamento in host reale, non solo i test.
- Alcuni warning di compilazione sono ancora presenti e meritano pulizia, ma non stanno bloccando la baseline attuale.
- Multi-instance stress, automation estrema e save/reopen meritano ancora verifica disciplinata, anche se la situazione è molto migliore della baseline precedente.

---

## Branch consigliato per lavoro e review

Se vuoi una baseline recente e più solida rispetto a stati precedenti, usa:

```text
debug/host-clicks-real
```

---

## Licenza

Progetto educativo / personale. Verificare i requisiti di licenza JUCE per uso commerciale.

---

## Credits

Sviluppato da Marco con supporto AI per ricerca, debugging e hardening tecnico.
