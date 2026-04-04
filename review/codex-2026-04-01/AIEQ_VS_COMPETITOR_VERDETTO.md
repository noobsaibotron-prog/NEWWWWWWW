# AIEQ Pro vs Competitor — Verdetto Diretto, Prove alla Mano

**Data**: 04 Aprile 2026 | **Autore**: Manus AI | **Metodo**: Codice sorgente AIEQ Pro + documentazione ufficiale competitor

---

## TABELLA COMPARATIVA GLOBALE

### 1. BANDE EQ

| Plugin | Max Bande | Tutte Dinamiche? | Fonte |
|---|---|---|---|
| **FabFilter Pro-Q 4** | **24** | Sì | fabfilter.com |
| **Kirchhoff-EQ** | **32** | Sì | threebodytech.com |
| **TDR Nova GE** | **6** (+HP/LP) | Sì (6) | tokyodawn.net |
| **Sonible smart:EQ 4** | **24** | No (smart:filter) | sonible.com |
| **AIEQ Pro** | **24** | **Sì** | `LockFreeStructures.h:389` |

> **VERDETTO: PARI con FabFilter e Sonible. PERDE contro Kirchhoff (32). VINCE su TDR Nova (6).**

---

### 2. GAIN RANGE (EQ Statico)

| Plugin | Range Gain | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | **±30 dB** | fabfilter.com |
| **Kirchhoff-EQ** | **±30 dB** | threebodytech.com |
| **TDR Nova GE** | **±30 dB** | tokyodawn.net |
| **AIEQ Pro** | **±24 dB** | `PluginProcessor.cpp:619` |

> **VERDETTO: PERDE. 6 dB in meno di tutti i competitor premium. ±24 dB è sufficiente per il 99% dei casi, ma sulla carta è inferiore.**

---

### 3. DYNAMIC EQ — RANGE

| Plugin | Dynamic Range Max | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | **±30 dB** | fabfilter.com |
| **AIEQ Pro** | **0 – 48 dB** | `PluginProcessor.cpp:681` |

> **VERDETTO: VINCE. AIEQ Pro ha 48 dB di dynamic range vs 30 dB di FabFilter. Questo è un vantaggio reale e misurabile.**

---

### 4. DYNAMIC EQ — THRESHOLD

| Plugin | Threshold Range | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | Non specificato (auto) | fabfilter.com |
| **TDR Nova GE** | -60 dB a 0 dB | tokyodawn.net |
| **AIEQ Pro** | **-60 dB a 0 dB** | `PluginProcessor.cpp:661` |

> **VERDETTO: PARI con TDR Nova. FabFilter usa un sistema auto-threshold diverso, non direttamente comparabile.**

---

### 5. DYNAMIC EQ — RATIO

| Plugin | Ratio Range | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | Non esposto (auto) | fabfilter.com |
| **TDR Nova GE** | 1:1 – ∞:1 | tokyodawn.net |
| **AIEQ Pro** | **1:1 – 20:1** | `PluginProcessor.cpp:666` |

> **VERDETTO: PERDE contro TDR Nova (che arriva a ∞:1). 20:1 è comunque più che sufficiente per il 99.9% degli usi.**

---

### 6. DYNAMIC EQ — ATTACK / RELEASE

| Plugin | Attack | Release | Fonte |
|---|---|---|---|
| **FabFilter Pro-Q 4** | Program-dependent (auto) | Program-dependent (auto) | fabfilter.com |
| **TDR Nova GE** | 0.02 – 200 ms | 1 – 2000 ms | tokyodawn.net |
| **AIEQ Pro** | **0.1 – 500 ms** | **1 – 2000 ms** | `PluginProcessor.cpp:671,676` |

> **VERDETTO: PARI / VINCE. AIEQ Pro ha attack più lungo (500 ms vs 200 ms TDR) e release identico. FabFilter è auto, non comparabile direttamente.**

---

### 7. DYNAMIC EQ — LOOK-AHEAD

| Plugin | Look-Ahead | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | **No** (auto mode) | fabfilter.com |
| **TDR Nova GE** | **No** | tokyodawn.net |
| **Kirchhoff-EQ** | **No** | threebodytech.com |
| **AIEQ Pro** | **Sì (5 ms in HQ mode)** | `PluginProcessor.cpp`, `RB4BehavioralTest.cpp` |

> **VERDETTO: VINCE. Nessun competitor ha look-ahead nel Dynamic EQ. AIEQ Pro ha 5 ms di look-ahead in HQ mode, verificato nel codice e nel test unitario. Questo è un differenziatore reale.**

---

### 8. DYNAMIC EQ — SIDECHAIN

| Plugin | Sidechain Esterno | Per-Banda? | Fonte |
|---|---|---|---|
| **FabFilter Pro-Q 4** | **Sì** | **Sì** | fabfilter.com |
| **TDR Nova GE** | **Sì** | **Sì** | tokyodawn.net |
| **AIEQ Pro** | **Sì** (mono bus) | **Sì** (con Q regolabile) | `PluginProcessor.cpp:42`, `DynamicEQProcessor.h:69` |

> **VERDETTO: PARI. Tutti hanno sidechain per-banda. AIEQ Pro ha il Q regolabile sul filtro sidechain, che è un plus.**

---

### 9. DYNAMIC EQ — MODI

| Plugin | Modi Dinamici | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | Compress / Expand | fabfilter.com |
| **TDR Nova GE** | Compress / Expand | tokyodawn.net |
| **AIEQ Pro** | **Compress / Expand / Gate** | `DynamicEQProcessor.h:44` |

> **VERDETTO: VINCE. AIEQ Pro ha Gate mode che i competitor non hanno nel Dynamic EQ.**

---

### 10. TIPI DI FILTRO

| Plugin | Tipi Filtro | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | 9 (Bell, Low/High Shelf, Low/High Cut, Notch, Band Pass, Tilt Shelf, Flat Tilt) | fabfilter.com |
| **Kirchhoff-EQ** | 7+ | threebodytech.com |
| **AIEQ Pro** | **9** (Low Cut, Low Shelf, Peak, High Shelf, High Cut, Notch, Band Pass, Vintage Low Shelf, Vintage High Shelf) | `PluginProcessor.cpp:635` |

> **VERDETTO: PARI. 9 tipi entrambi, ma diversi. FabFilter ha Tilt Shelf/Flat Tilt, AIEQ Pro ha Vintage Shelf (Pultec-style). Scelta stilistica diversa, non inferiore.**

---

### 11. SLOPE (PENDENZA FILTRO)

| Plugin | Slope Disponibili | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | 6 dB/oct – **96 dB/oct** (continuo) | fabfilter.com |
| **Kirchhoff-EQ** | Variabile continua | threebodytech.com |
| **AIEQ Pro** | **12 / 24 / 48 dB/oct** (3 step) | `PluginProcessor.cpp:648` |

> **VERDETTO: PERDE. FabFilter arriva a 96 dB/oct con slope continua. AIEQ Pro ha solo 3 step fino a 48 dB/oct. Gap significativo per chirurgical EQ.**

---

### 12. LATENZA — ZERO LATENCY MODE

| Plugin | Zero Latency | Latenza Reale | Fonte |
|---|---|---|---|
| **FabFilter Pro-Q 4** | **0 samples** | 0 ms | fabfilter.com |
| **Sonible smart:EQ 4** | **0 samples** | 0 ms | sonible.com |
| **AIEQ Pro** | **Dichiarato ma FINTO** | Worst-case padding (128+ samples) | `PluginProcessor.cpp:1085` |

> **VERDETTO: PERDE. AIEQ Pro NON ha una vera zero latency. Il codice forza `setLatencySamples(worstCaseLatencySamples)` per tutte le modalità. FabFilter e Sonible hanno 0 samples reali.**

---

### 13. LATENZA — LINEAR PHASE

| Plugin | Linear Phase Latency | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | 3.072 – 66.560 samples (5 livelli) | fabfilter.com |
| **Sonible smart:EQ 4** | ~3.087 samples (~70 ms) | sonible.com |
| **AIEQ Pro** | **128 samples (~2.9 ms @ 44.1kHz)** | `PartitionedConvolver.h:24` |

> **VERDETTO: VINCE NETTAMENTE. 128 samples vs 3.072+ di FabFilter. La partitioned convolution di AIEQ Pro è un'architettura superiore per la latenza in linear phase. Questo è un vantaggio tecnico enorme.**

---

### 14. OVERSAMPLING

| Plugin | Oversampling | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | **No** (non necessario con engine custom) | fabfilter.com |
| **Kirchhoff-EQ** | **2x** opzionale | threebodytech.com |
| **AIEQ Pro** | **Off / 2x / 4x / Auto** | `PluginProcessor.cpp:533` |

> **VERDETTO: VINCE. AIEQ Pro offre fino a 4x oversampling con Auto mode. FabFilter non lo offre (non ne ha bisogno con il suo engine), Kirchhoff solo 2x.**

---

### 15. M/S PROCESSING

| Plugin | Approccio M/S | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | **Per-banda** (L/R/M/S per singola banda) | fabfilter.com |
| **Kirchhoff-EQ** | **Per-banda** | threebodytech.com |
| **AIEQ Pro** | **Globale** (Stereo / Mid / Side / M/S Linked) | `PluginProcessor.cpp:528` |

> **VERDETTO: PERDE. Il design per-banda di FabFilter e Kirchhoff è architetturalmente superiore. AIEQ Pro ha un switch globale che richiede crossfade (1024 campioni). Non è un bug, è un design diverso — ma inferiore per flessibilità.**

---

### 16. AI / MACHINE LEARNING

| Plugin | Tipo AI | Real-Time? | Fonte |
|---|---|---|---|
| **Gullfoss** | Modello percettivo uditivo (1000 update/sec) | **Sì** | soundtheory.com |
| **Sonible smart:EQ 4** | Smart:filter con profili ML | **Sì** | sonible.com |
| **iZotope Ozone 11** | Master Assistant (ML one-shot) | **No** (analisi offline) | izotope.com |
| **AIEQ Pro** | **Dense Net (64→128→64→8) + TFLite opzionale** | **~10 Hz** (non real-time audio) | `MLEngine.cpp`, `NeuralNetworkWrapper.cpp` |

> **VERDETTO: PERDE contro Gullfoss e Sonible (AI real-time). PARI con Ozone (analisi offline). AIEQ Pro ha un ML reale ma piccolo (rete densa classica, non deep learning). Il TFLite è opzionale e non compilato di default.**

---

### 17. FRAMEWORK / ENGINE

| Plugin | Framework | Fonte |
|---|---|---|
| **FabFilter** | **Engine proprietario custom** (non JUCE) | fabfilter.com |
| **Kirchhoff-EQ** | **Engine proprietario custom** | threebodytech.com |
| **TDR Nova** | **Engine proprietario custom** | tokyodawn.net |
| **Sonible smart:EQ 4** | **JUCE** | Community |
| **AIEQ Pro** | **JUCE** | Codice sorgente |

> **VERDETTO: PARI con Sonible (entrambi JUCE). PERDE contro FabFilter/Kirchhoff/TDR (engine custom). Un engine custom permette ottimizzazioni impossibili in JUCE.**

---

### 18. STABILITÀ (pluginval)

| Plugin | pluginval Level | Fonte |
|---|---|---|
| **FabFilter Pro-Q 4** | Presumibilmente 10 (standard industriale) | N/A |
| **AIEQ Pro** | **NON TESTATO** | Fatto verificato |

> **VERDETTO: NON VALUTABILE. AIEQ Pro non è stato testato con pluginval. Deve superare Level 5 (minimo) prima del rilascio.**

---

## RIEPILOGO FINALE — SCORECARD

| Parametro | vs FabFilter | vs Kirchhoff | vs TDR Nova | vs Sonible | vs Gullfoss |
|---|---|---|---|---|---|
| Bande | PARI (24) | PERDE (32) | VINCE (6) | PARI (24) | N/A |
| Gain Range | PERDE (±24 vs ±30) | PERDE | PERDE | N/A | N/A |
| Dynamic Range | **VINCE** (48 vs 30) | N/A | N/A | N/A | N/A |
| Look-Ahead DynEQ | **VINCE** (5ms vs 0) | **VINCE** | **VINCE** | N/A | N/A |
| Gate Mode | **VINCE** | N/A | N/A | N/A | N/A |
| Slope | PERDE (48 vs 96) | PERDE | N/A | N/A | N/A |
| Zero Latency | PERDE (finto vs reale) | N/A | N/A | PERDE | N/A |
| Linear Phase Lat. | **VINCE** (128 vs 3072+) | N/A | N/A | **VINCE** | N/A |
| Oversampling | **VINCE** (4x vs 0) | **VINCE** (4x vs 2x) | N/A | N/A | N/A |
| M/S Processing | PERDE (globale vs per-banda) | PERDE | N/A | N/A | N/A |
| AI/ML | PERDE (no AI vs no AI) | N/A | N/A | PERDE | PERDE |
| Framework | PERDE (JUCE vs custom) | PERDE | PERDE | PARI | N/A |
| Sidechain | PARI | N/A | PARI | N/A | N/A |
| Filter Types | PARI (9 vs 9) | PARI | N/A | N/A | N/A |
| pluginval | NON TESTATO | N/A | N/A | N/A | N/A |

---

## VERDETTO GLOBALE

**AIEQ Pro è a livello dei competitor premium in ALCUNE aree, e INFERIORE in altre.**

### Dove AIEQ Pro VINCE (differenziatori reali):
1. **Dynamic Range 48 dB** — il più alto del mercato
2. **Look-Ahead nel Dynamic EQ (5 ms)** — nessun competitor ce l'ha
3. **Gate mode nel Dynamic EQ** — unico
4. **Linear Phase a 128 samples** — latenza 24x inferiore a FabFilter
5. **Oversampling 4x con Auto** — FabFilter non lo offre

### Dove AIEQ Pro PERDE (gap da colmare):
1. **Gain range ±24 dB** — tutti gli altri hanno ±30 dB
2. **Slope max 48 dB/oct** — FabFilter arriva a 96 dB/oct
3. **M/S globale** — i premium hanno M/S per-banda
4. **Zero Latency finta** — padding worst-case, non vera zero latency
5. **AI piccola** — rete densa classica, non deep learning real-time
6. **pluginval NON testato** — bloccante per il rilascio
7. **JUCE vs engine custom** — limita le ottimizzazioni GUI

### Risposta secca alla domanda "è a livello degli altri?":

> **Nel DSP puro (Dynamic EQ, Linear Phase, Oversampling): SÌ, è competitivo e in alcune aree superiore.**
> **Nell'architettura generale (M/S, Zero Latency, GUI engine, AI): NO, è un gradino sotto FabFilter e Kirchhoff.**
> **Per il prezzo e il posizionamento: dipende. Se venduto a €49-79, è un ottimo prodotto. Se venduto a €149+, i gap diventano evidenti.**
