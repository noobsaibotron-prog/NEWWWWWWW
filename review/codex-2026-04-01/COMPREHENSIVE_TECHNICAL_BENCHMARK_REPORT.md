# AIEQ+ Comprehensive Technical Benchmark Report
**Data**: 04 Aprile 2026
**Autore**: Manus AI (Alta Corte v5.1)
**Stato**: RELEASE-CANDIDATE (Code-Verified)

## 1. Executive Summary

Il presente documento fornisce una verifica numerica rigorosa e comparativa di tutte le affermazioni tecniche riguardanti **AIEQ Pro**, confrontandole con i leader di mercato (FabFilter Pro-Q 4, Sonible smart:EQ 4, iZotope Ozone 11, Soundtheory Gullfoss, TDR Nova GE, Kirchhoff-EQ). 

Ogni parametro di AIEQ Pro è stato **estratto e verificato direttamente dal codice sorgente** del repository GitHub (branch `review/codex-2026-04-01`).

## 2. Analisi Comparativa dei Parametri Numerici

### 2.1 Latenza (PDC - Plugin Delay Compensation)

La latenza è un parametro critico per l'uso in fase di tracking e live mixing. 
*Nota sul codice*: AIEQ Pro dichiara una "Zero Latency" mode nella UI (`phaseMode == 0`), ma internamente la latenza è allineata al "worst case" per evitare ricalcoli PDC del DAW.

| Plugin | Zero Latency Mode | Linear Phase Mode (Max) | Note |
|---|---|---|---|
| **FabFilter Pro-Q 4** | 0 samples (0 ms) | 66.560 samples (~1.509 ms) | Raddoppia con L/R e M/S simultanei [1] |
| **Sonible smart:EQ 4** | 0 samples (0 ms) | ~3.087 samples (~70 ms) | Due modalità distinte [2] |
| **Soundtheory Gullfoss** | ~88 samples (2 ms) | N/A (20 ms standard) | Versione Live ottimizzata [3] |
| **AIEQ Pro (dal codice)** | **NON PRESENTE (0 samples finti)** | **128 samples (Partitioned)** | Il codice forza `worstCaseLatencySamples = std::max(lpLatency, oversamplingLatency)` per tutte le modalità [4] |

**Verdetto AIEQ Pro**: Il claim sulla latenza "Zero Latency" nella UI è **tecnicamente falso** a causa della strategia di "Maximum Latency Padding" (`PluginProcessor.cpp:1068`). Il plugin riporta sempre la latenza peggiore (es. 128 campioni per Linear Phase o la latenza dell'oversampling) anche in modalità "Zero Latency", per evitare glitch audio durante il cambio di modalità.

### 2.2 Gestione M/S e Crossfade

AIEQ Pro implementa un crossfade per le transizioni M/S e per le modalità di fase.

| Plugin | Approccio M/S | Crossfade Necessario? |
|---|---|---|
| **FabFilter Pro-Q 4** | Per-band stereo placement | NO (Nessun cambio di stato globale) [5] |
| **Kirchhoff-EQ** | Per-band processing | NO [6] |
| **AIEQ Pro (dal codice)** | **Switch Globale (Stereo ↔ M/S)** | **SÌ (1024 campioni)** |

**Verdetto AIEQ Pro**: Il crossfade di 1024 campioni (`msModeTransitionCrossfadeSamples = 1024`, `PluginProcessor.h:622`) è una soluzione ingegneristica per un design globale. I competitor premium usano un design *per-banda* che non richiede crossfade.

### 2.3 Architettura GUI e Rendering

La fluidità dell'interfaccia è fondamentale per l'esperienza utente.

| Plugin | Engine Grafico | Note Performance |
|---|---|---|
| **FabFilter Pro-Q 4** | Custom GPU Engine (Non-JUCE) | Eccellente, accelerazione nativa dal 2012 [7] |
| **Sonible smart:EQ 4** | JUCE Software Renderer | Buona, focus sull'AI [8] |
| **AIEQ Pro** | **JUCE OpenGL + SpinLock** | **Ottimizzato con SpinLock, ma a rischio su macOS (OpenGL deprecato)** |

### 2.4 Capacità Dynamic EQ

I parametri esatti del Dynamic EQ di AIEQ Pro estratti dal file `DynamicEQProcessor.h` e `PluginProcessor.cpp`.

| Parametro | FabFilter Pro-Q 4 | Kirchhoff-EQ | AIEQ Pro (dal codice sorgente) |
|---|---|---|---|
| **Max Bande Dinamiche** | 24 | 32 | **24 bande** (`kMaxBands = 24`) |
| **Dynamic Range** | ±30 dB | Variabile | **0.0 dB a 48.0 dB** (Range max) |
| **Gain (EQ statico)** | ±30 dB | ±30 dB | **±24.0 dB** |
| **Threshold** | Auto / Manuale | Variabile | **-60.0 dB a 0.0 dB** |
| **Ratio** | Variabile | Variabile | **1:1 a 20:1** |
| **Attack** | 0.1 - 500 ms | Variabile | **0.1 ms a 500.0 ms** |
| **Release** | 10 - 2000 ms | Variabile | **1.0 ms a 2000.0 ms** |
| **Knee** | Hard/Soft | Hard/Soft | **0.0 dB a 24.0 dB** |
| **Look-Ahead** | No (auto mode) | No | **Sì (5.0 ms in HQ mode, 0 ms in ZL)** |
| **Sidechain Esterno** | Sì (per-banda) | Sì | **Sì (con Q regolabile, testato in unit test)** |

**Verdetto AIEQ Pro**: Il Dynamic EQ di AIEQ Pro è estremamente competitivo. Offre 24 bande, range di 48 dB, look-ahead reale (5 ms in HQ mode, dimostrato nel test `RB4BehavioralTest.cpp`), e sidechain per-banda. È un'implementazione di alto livello (TDR Nova / FabFilter style) basata su architettura lock-free (`juce::AbstractFifo` e atomics).

### 2.5 Intelligenza Artificiale e Machine Learning

Verifica dell'implementazione AI nel codice sorgente (`NeuralNetworkWrapper.cpp`, `MLEngine.cpp`).

| Plugin | Tipo di Tecnologia | Inference |
|---|---|---|
| **Gullfoss** | Modello Percettivo Uditivo (1000 update/sec) | 2-20 ms [9] |
| **Sonible smart:EQ 4** | Smart:filter con profili (Machine Learning) | 70 ms (Linear Phase) [10] |
| **AIEQ Pro (dal codice)** | **ML Classico (Dense Net 64->128->8) + TFLite Opzionale** | **~10 Hz cadence** |

**Verdetto AIEQ Pro**: Il codice sorgente **CONTIENE** un modello di Machine Learning reale (`MLEngine.cpp` usa una rete neurale densa con 64 mel-band in input) e un wrapper per TensorFlow Lite (`NeuralNetworkWrapper.cpp`). 
Tuttavia, **il supporto TFLite è opzionale (gated da `AIEQ_ENABLE_TFLITE`) e l'online training NON è supportato** (la C API di TFLite non lo permette, come commentato nel codice). Il claim di "Deep Learning" è eccessivo per la rete densa di base, ma il plugin *usa* effettivamente Machine Learning per la classificazione dei problemi (8 classi) e dei generi (8 classi).

## 3. Conclusioni e Raccomandazioni per il Lancio

1. **Correggere il claim sulla latenza**: AIEQ Pro non ha una vera "Zero Latency" a causa del padding per il DAW. Deve essere comunicato come "Fixed Latency" per evitare glitch del PDC.
2. **Dynamic EQ Eccellente**: I parametri estratti dal codice confermano un Dynamic EQ di livello premium (24 bande, look-ahead 5ms, lock-free). Questo è il vero punto di forza tecnico del plugin.
3. **Trasparenza sull'AI**: L'AI è basata su una piccola rete neurale densa integrata (o TFLite se compilato). Non è "Deep Learning" generativo, ma un classificatore ML classico. I claim di marketing devono riflettere questa realtà.

---

## Riferimenti

[1] FabFilter Pro-Q 4 Processing Mode: https://www.fabfilter.com/help/pro-q/using/processingmode
[2] Sonible smart:EQ 4 Manual: https://sonible.com/wp-content/uploads/2023/12/manual-sonible-smartEQ4_EN.pdf
[3] Soundtheory Gullfoss Specs: https://www.soundtheory.com/gullfoss
[4] Codice Sorgente AIEQ Pro: `Source/PluginProcessor.cpp` (Line 1068, Maximum Latency Padding)
[5] FabFilter Pro-Q 4 Stereo Options: https://www.fabfilter.com/help/pro-q/using/stereo
[6] Three-Body Technology Kirchhoff-EQ: https://www.threebodytech.com/en/products/kirchhoffeq
[7] FabFilter Forum / News
[8] Sonible smart:EQ 4 Specifications: https://www.sonible.com/smarteq4/
[9] Gullfoss Operation Manual: https://www.soundtheory.com/static/Gullfoss%20Operation%20Manual.pdf
[10] Sonible smart:EQ 4 Features: https://www.sonible.com/smarteq4/
