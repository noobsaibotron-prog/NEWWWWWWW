Indagine IntegrationTests AI Equalizer Pro — 19 fail su 642 assertion.

Sostituisce ogni analisi precedente. Lavora solo sui file che leggi dalle URL
pin-nate qui sotto.

STATO REALE (commit-pinned, immutabile)
- Repo:          https://github.com/noobsaibotron-prog/NEWWWWWWW
- Sync commit:   4c6c2ab8b857ddfbff699a4d0e3fc4394064213b
- Source branch: review/codex-2026-04-01
- Base commit:   c3a8446ef6c37760f88d1c01fe3763dbf23cfbac
- Snapshot time: 2026-04-11 15:16:53

Tree browser pin-nato:
  https://github.com/noobsaibotron-prog/NEWWWWWWW/tree/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b

Diff pin-nato vs main:
  https://github.com/noobsaibotron-prog/NEWWWWWWW/compare/main...4c6c2ab8b857ddfbff699a4d0e3fc4394064213b

LOG COMPLETO (raw URL, 1253 righe totali):
  https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/.chatgpt-ci/integration_test.log

(Le prime 1500 righe del log sono anche embedded a fondo prompt come
 fallback se il fetch diretto dell'URL fallisce.)

SITUAZIONE
Ho eseguito `AIEqualizerPro_IntegrationTests` (18 test file) e ottenuto:
- Total assertions: 642
- Passed:           623
- Failed:           19

I fail si raggruppano in 6 categorie. Per ciascuna categoria serve un verdetto
separato: "bug reale", "test troppo severo", "fixture rotta", o "bug nel test
harness". NON mischiare le categorie nella risposta.

Le URL pin-nate ai file rilevanti per ogni categoria sono qui sotto. Leggile
fresche — non ricostruire da memoria.

==============================================================================
CATEGORIA 1 — BYPASS OUTPUT DRIFT (6 fail)
==============================================================================

Test failing:
  Bypass Transition Continuity / active -> bypass @ {44100,48000} / block {64,128,512}
  Test 7: "Bypass output should match dry input after settle window"
  maxDiff = 0.43–0.66 su scala -1..+1

Paradosso rispetto a Anti-Pop Regression (che passa):
  - AntiPopRegression "Bypass exit after long steady-state produces no click" → PASSA (clicks=0, maxDelta=0.2186)
  - AntiPopRegression "Bypass rapid toggle" → PASSA (clicks=0)
  - AntiPopRegression "Bypass exit with adversarial block sizes" → PASSA su tutti (1, 7, 31, 64, 127, 256, 513, 1024)
  - AntiPopRegression "Storm bypass toggle (every block × 200)" → PASSA (clicks=0)

→ NON è un click problem (no discontinuità brusche sample-by-sample).
→ È un OFFSET TAIL problem: dopo aver premuto bypass e atteso la settle
  window, l'output del plugin differisce dal dry input puro di 40-66%.
→ Ipotesi: c'è un decay residuo (es. IIR tail che non è stato cleared, o
  dry/wet mix non è al 100% dry in bypass state, o il lookahead ring buffer
  non viene svuotato).

File da leggere:
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/IntegrationStateTest.cpp
  (cerca la sezione "Bypass Transition Continuity" — voglio sapere come
   la test fixture costruisce il dry input di riferimento e come misura
   la settle window)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/AntiPopRegressionTest.cpp
  (confronta come questa fixture verifica il no-click — capire perché
   anti-pop passa mentre integration-state fallisce)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/PluginProcessor.cpp
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/PluginProcessor.h
  (cerca processBlockBypassed, setBypassed, DryWetMixer, lookahead buffer
   clear/reset — cosa succede quando il flag bypass si attiva?)

Domanda verdetto: il bypass ha un tail residuo reale (bug audio) o è il
test che pretende identity bit-perfect dopo settle che in realtà è
impossibile per un plugin con dry/wet e lookahead?

==============================================================================
CATEGORIA 2 — PHASE MODE SWITCH 0→1 + AB SWITCH (2 fail)
==============================================================================

Test failing:
  Phase Mode Switch Continuity / Phase mode 0 -> 1 (Natural → Zero Latency)
  Test 4: "Phase mode switch: maxDelta too high = 0.6058"

  Host Session Click Detection / A/B profile switch with different EQ curves
  Test 12: "AB switch@block60: 1 clicks detected (max allowed: 0)"
  (maxDelta=0.4011, peakAbs=0.7297, clicks=1)

Pattern direzionale: 0→1 fallisce, ma 1→0, 0→2, 2→0 passano tutti. Solo la
transizione Natural→Zero è rotta. Bug riproducibile dall'utente umano in
Ableton ("click continuo sul basso durante band drag in phase natural/zero-latency").

File da leggere:
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/TransitionContinuityTests.cpp
  (cerca "Phase Mode Switch Continuity" — capire cosa fa il test 4 e
   come calcola maxDelta)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/HostSessionClickTest.cpp
  (cerca "A/B profile switch" — capire perché block60 specifically fallisce
   mentre block30, block90, block110 passano)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/LinearPhaseProcessor.h
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/LinearPhaseProcessor.cpp
  (cerca switch quality mode / ZL→HQ transition, ring buffer reset,
   IR swap path — cosa succede quando si passa da Natural a Zero?)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/ParametricEQProcessor.h
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/ParametricEQProcessor.cpp
  (cerca come lo state dei filtri viene preservato o resettato nella
   transizione phase mode)

Domanda verdetto: c'è un path di switch 0→1 che non preserva la continuità
dello state (es. lookahead buffer non copiato, IR swap senza crossfade)?

==============================================================================
CATEGORIA 3 — BLOCKSIZE + BAND DRAG (4 fail)
==============================================================================

Test failing:
  BlockSize Regression / Dry/wet survives oversized block without re-prepare
    Test 3: "Tail of oversized block was cleared (regression for dry/wet)"
    Test 4: "Tail RMS is effectively silent"

  Host Session Click / Compound stress
    Test 5: "audio dropout, 99 silent samples"

  Host Session Click / Small block (32) transitions
    Test 5: "audio dropout, 103 silent samples"

Pattern: quando i block size escono dal range "normale" (oversized > buffer,
o piccolissimi = 32), il dry/wet mix o il band drag producono gap silenziosi
o tail azzerati.

File da leggere:
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/BlockSizeRegressionTest.cpp
  (capire cosa sono "Test 3" e "Test 4" di "Dry/wet survives oversized block")
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/BandDragContinuityTest.cpp
  (capire il test flow della band drag stress)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/PluginProcessor.cpp
  (cerca processBlock, prepareToPlay, setLatencySamples, DryWetMixer buffer
   sizing — la DryWetMixer è pre-dimensionata correttamente quando il block
   size supera il buffer di preparazione?)

Domanda verdetto: è un bug reale (la DryWetMixer/lookahead non gestisce
block > maxExpectedBlockSize) o è un test edge case che l'utente normale
non colpisce mai?

==============================================================================
CATEGORIA 4 — SPECTRUM PIPELINE 2× HOPS (3 fail)
==============================================================================

Test failing:
  Spectrum Pipeline / Partial FIFO pull does not lose samples (staging accumulates)
    Test 2: "Second tick with 1000 total should not produce a hop"
    Test 5: "Should have completed 1 hop, got 2"

  Spectrum Pipeline / Fragmented input (small blocks) continuously produces hops
    Test 1: "Expected ~25 hops from 100×256 samples, got 50"

  Spectrum Pipeline / Pipeline stats instrumentation reports correct values
    Test 5: "2048 samples / 1024 hop = 2 hops, got 4"

Pattern univoco e chiarissimo: **il pipeline produce esattamente il doppio
degli hop attesi**. 1→2, 25→50, 2→4. Ratio 2×.

Sospetto forte: fix hopSize già menzionato nel piano originale
(`hopSize = fftSize / 2` vs `hopSize = fftSize / 4` in NewSpectrumPipeline).
Se hopSize è dimezzato ma il consumer che pulls chiama con frequenza invariata,
il conteggio hop si raddoppia.

Possibile concausa del "feel spettro rumoroso" lamentato dall'utente:
se il pipeline sta calcolando il doppio dei frame per renderizzare lo stesso
spettro, potrebbe contribuire a una sensazione di "shimmer" o "iper-attività"
visiva.

File da leggere:
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/SpectrumPipelineTest.cpp
  (leggere le 3 test cases — capire quali API del pipeline chiamano e
   come contano i hops)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/GUI/NewSpectrumPipeline.h
  (cerca hopSize declaration + setFFTOrder — il bug noto del piano
   originale Phase 1.2)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/GUI/SpectrumAnalyzerCore.h
  (cerca push/pull semantic del FIFO — dove viene contato il "hop done"?)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/SpectrumDisplayMapper.h
  (questa è la sezione aggiornata in Wave 5 Option A smoothing: attack 15ms,
   release 500ms, ma NON è il pipeline consumer — è downstream)

Domanda verdetto: confermi che è il bug hopSize/2→/4 del piano originale, o
è un'altra source di doppio accounting nel pipeline?

==============================================================================
CATEGORIA 5 — RB-2 SAVE/LOAD XML FLOAT EPSILON (8 fail)
==============================================================================

Test failing:
  RB-2 Equivalence / save₁ → load → save₂ produces identical XML
  RB-2 Equivalence / 5 consecutive save/load cycles produce no drift
  RB-2 Equivalence / all APVTS params at non-default → roundtrip stable
  RB-2 Soak / 250 consecutive save/load cycles → zero drift
  RB-2 Soak / 50 randomized configurations → each roundtrip stable

Pattern: tutti i fail sono diff XML dove un valore float è rappresentato in
modo epsilon-diverso:

  save₁:  q="0.9999999403953552"
  save₂:  q="1.0"

oppure:

  save₁:  dynThreshold="-19.99999809265137"
  save₂:  dynThreshold="-20.0"

Questo è floating-point quantization error standard (un Q=1.0f non è
esattamente 1.0 quando costruito dal parser via juce::String::getDoubleValue,
e il write-back via juce::String(val) produce una rappresentazione esatta).

Impact audio: **zero** — un Q di 0.9999999 vs 1.0 è musicalmente identico.

Domanda verdetto:
1. È un bug reale nel parser/serializer (ParseErrorRoundTrip) o è il test
   troppo severo (dovrebbe usare epsilon-tolerant diff invece di bit-perfect
   XML match)?
2. Se è il test troppo severo, il fix più pulito è:
   (a) modificare il test per usare isApproximately(a, b, eps=1e-5), oppure
   (b) modificare il serializer per produrre canonical form (es. sempre
       float-to-string via stringprintf("%.6g", val))?

File da leggere:
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/RB2EquivalenceTest.cpp
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Utils/PresetManager.cpp
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Utils/PresetManager.h
  (cerca come serializes il state XML — usa replaceState, getParameter,
   e che formato float-to-string usa)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/PluginProcessor.cpp
  (cerca setStateInformation/getStateInformation — stesso path)

==============================================================================
CATEGORIA 6 — RB-4 BEHAVIORAL ZL=HQ=0 (3 fail)
==============================================================================

Test failing:
  RB-4 Behavioral / HQ lookahead reduces transient overshoot vs ZL
    "ZL peak=0.0000 rms=0.0000 | HQ peak=0.0000 rms=0.0000"
    Test 1: "HQ peak (0.0000) should be lower than ZL peak (0.0000)"

  RB-4 Behavioral / lookaheadSamples value changes with qualityMode
    Test 1: "HQ and ZL impulse responses should differ (lookahead delay)"

  RB-4 Behavioral / runtime switch from ZL to HQ changes output
    "RMS before switch (ZL): 0.000000"
    "RMS after switch (HQ):  0.000000"
    Test 1: "Output should change after ZL→HQ switch. Delta=0.000000"

**Paradosso**: RB-4 Closure tests (stessa area di codice) **passano tutti**:
  - Determinism: "Pass 1 vs Pass 2: RMS diff=-200.0 dBFS, Peak diff=-200.0 dBFS"
  - Repeatability: -200.0 dBFS self-diff su ogni block size
  - Playback vs offline: SNR 86.7 dB
  - Transition glitch: overshoot 0.0 dB above ref

Quindi il plugin STA producendo audio correttamente in RB-4 Closure.
Ma RB-4 Behavioral vede peak=0 su tutti i path. Questo suggerisce una
**fixture rotta** in RB4BehavioralTest.cpp: forse l'input del test non
arriva al processor (es. manca prepareToPlay, o il test genera un signal
con amplitude 0, o il buffer non viene passato correttamente).

File da leggere:
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/RB4BehavioralTest.cpp
  (capire come il test genera l'input signal e misura peak/RMS)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/RB4OfflineRenderTest.cpp
  (confronto con il test RB-4 Closure che PASSA — perché questo vede
   l'output e quello no?)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/Tests/LinearPhaseLatencyContractTest.cpp
  (il contract test lookahead che passa — conferma che il meccanismo
   lookahead funziona)
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/LinearPhaseProcessor.h
- https://raw.githubusercontent.com/noobsaibotron-prog/NEWWWWWWW/4c6c2ab8b857ddfbff699a4d0e3fc4394064213b/Source/DSP/LinearPhaseProcessor.cpp

Domanda verdetto: RB4BehavioralTest.cpp è una fixture rotta (test ha un
bug nel setup dell'input), o è un vero bug del LinearPhaseProcessor che
RB-4 Closure non cattura perché usa input differenti?

==============================================================================
LOG TEST FALLBACK (prime 1500 righe, vedi URL sopra per il log completo)
==============================================================================

```
========================================
     AI Equalizer Pro - Test Suite      
========================================
Running project test categories: DSP, Regression, Integration
Random seed: 0x567ef39
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push multiple small buffers...
Completed tests in DryWetMixer / Mixer can push multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can push and pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block...
Completed tests in DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push multiple small buffers...
Completed tests in DryWetMixer / Mixer can push multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can push and pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block...
Completed tests in DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push multiple small buffers...
Completed tests in DryWetMixer / Mixer can push multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can push and pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block...
Completed tests in DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push multiple small buffers...
Completed tests in DryWetMixer / Mixer can push multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop multiple small buffers...
Completed tests in DryWetMixer / Mixer can push and pop multiple small buffers
-----------------------------------------------------------------
Starting tests in: DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block...
Completed tests in DryWetMixer / Mixer can push and pop full-sized blocks after encountering a shorter block
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / AdditionTest...
Completed tests in Linear Algebra UnitTests / AdditionTest
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / DifferenceTest...
Completed tests in Linear Algebra UnitTests / DifferenceTest
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / ScalarMultiplication...
Completed tests in Linear Algebra UnitTests / ScalarMultiplication
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / HadamardProductTest...
Completed tests in Linear Algebra UnitTests / HadamardProductTest
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / MultiplicationTest...
Completed tests in Linear Algebra UnitTests / MultiplicationTest
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / IdentityMatrixTest...
Completed tests in Linear Algebra UnitTests / IdentityMatrixTest
-----------------------------------------------------------------
Starting tests in: Linear Algebra UnitTests / SolvingTest...
Completed tests in Linear Algebra UnitTests / SolvingTest
-----------------------------------------------------------------
Starting tests in: LogRampedValueTests / Curve...
Completed tests in LogRampedValueTests / Curve
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / InitializationTest...
Completed tests in SIMDRegister UnitTests / InitializationTest
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / AccessTest...
Completed tests in SIMDRegister UnitTests / AccessTest
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / AdditionOperators...
Completed tests in SIMDRegister UnitTests / AdditionOperators
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / SubtractionOperators...
Completed tests in SIMDRegister UnitTests / SubtractionOperators
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / MultiplicationOperators...
Completed tests in SIMDRegister UnitTests / MultiplicationOperators
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / BitANDOperators...
Completed tests in SIMDRegister UnitTests / BitANDOperators
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / BitOROperators...
Completed tests in SIMDRegister UnitTests / BitOROperators
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / BitXOROperators...
Completed tests in SIMDRegister UnitTests / BitXOROperators
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckComparisons...
Completed tests in SIMDRegister UnitTests / CheckComparisons
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckBoolEquals...
Completed tests in SIMDRegister UnitTests / CheckBoolEquals
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckMinMax...
Completed tests in SIMDRegister UnitTests / CheckMinMax
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckMultiplyAdd...
Completed tests in SIMDRegister UnitTests / CheckMultiplyAdd
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckSum...
Completed tests in SIMDRegister UnitTests / CheckSum
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckAbs...
Completed tests in SIMDRegister UnitTests / CheckAbs
-----------------------------------------------------------------
Starting tests in: SIMDRegister UnitTests / CheckTruncate...
Completed tests in SIMDRegister UnitTests / CheckTruncate
-----------------------------------------------------------------
Starting tests in: AudioBlock / Equality...
Completed tests in AudioBlock / Equality
-----------------------------------------------------------------
Starting tests in: AudioBlock / Constructors...
Completed tests in AudioBlock / Constructors
-----------------------------------------------------------------
Starting tests in: AudioBlock / Swap...
Completed tests in AudioBlock / Swap
-----------------------------------------------------------------
Starting tests in: AudioBlock / Getters and setters...
Completed tests in AudioBlock / Getters and setters
-----------------------------------------------------------------
Starting tests in: AudioBlock / Basic copying...
Completed tests in AudioBlock / Basic copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Addition...
Completed tests in AudioBlock / Addition
-----------------------------------------------------------------
Starting tests in: AudioBlock / Subtraction...
Completed tests in AudioBlock / Subtraction
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiplication...
Completed tests in AudioBlock / Multiplication
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiply add...
Completed tests in AudioBlock / Multiply add
-----------------------------------------------------------------
Starting tests in: AudioBlock / Negative abs min max...
Completed tests in AudioBlock / Negative abs min max
-----------------------------------------------------------------
Starting tests in: AudioBlock / Operators...
Completed tests in AudioBlock / Operators
-----------------------------------------------------------------
Starting tests in: AudioBlock / Process...
Completed tests in AudioBlock / Process
-----------------------------------------------------------------
Starting tests in: AudioBlock / Copying...
Completed tests in AudioBlock / Copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Smoothing...
Completed tests in AudioBlock / Smoothing
-----------------------------------------------------------------
Starting tests in: AudioBlock / Equality...
Completed tests in AudioBlock / Equality
-----------------------------------------------------------------
Starting tests in: AudioBlock / Constructors...
Completed tests in AudioBlock / Constructors
-----------------------------------------------------------------
Starting tests in: AudioBlock / Swap...
Completed tests in AudioBlock / Swap
-----------------------------------------------------------------
Starting tests in: AudioBlock / Getters and setters...
Completed tests in AudioBlock / Getters and setters
-----------------------------------------------------------------
Starting tests in: AudioBlock / Basic copying...
Completed tests in AudioBlock / Basic copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Addition...
Completed tests in AudioBlock / Addition
-----------------------------------------------------------------
Starting tests in: AudioBlock / Subtraction...
Completed tests in AudioBlock / Subtraction
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiplication...
Completed tests in AudioBlock / Multiplication
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiply add...
Completed tests in AudioBlock / Multiply add
-----------------------------------------------------------------
Starting tests in: AudioBlock / Negative abs min max...
Completed tests in AudioBlock / Negative abs min max
-----------------------------------------------------------------
Starting tests in: AudioBlock / Operators...
Completed tests in AudioBlock / Operators
-----------------------------------------------------------------
Starting tests in: AudioBlock / Process...
Completed tests in AudioBlock / Process
-----------------------------------------------------------------
Starting tests in: AudioBlock / Copying...
Completed tests in AudioBlock / Copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Smoothing...
Completed tests in AudioBlock / Smoothing
-----------------------------------------------------------------
Starting tests in: AudioBlock / Equality...
Completed tests in AudioBlock / Equality
-----------------------------------------------------------------
Starting tests in: AudioBlock / Constructors...
Completed tests in AudioBlock / Constructors
-----------------------------------------------------------------
Starting tests in: AudioBlock / Swap...
Completed tests in AudioBlock / Swap
-----------------------------------------------------------------
Starting tests in: AudioBlock / Getters and setters...
Completed tests in AudioBlock / Getters and setters
-----------------------------------------------------------------
Starting tests in: AudioBlock / Basic copying...
Completed tests in AudioBlock / Basic copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Addition...
Completed tests in AudioBlock / Addition
-----------------------------------------------------------------
Starting tests in: AudioBlock / Subtraction...
Completed tests in AudioBlock / Subtraction
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiplication...
Completed tests in AudioBlock / Multiplication
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiply add...
Completed tests in AudioBlock / Multiply add
-----------------------------------------------------------------
Starting tests in: AudioBlock / Negative abs min max...
Completed tests in AudioBlock / Negative abs min max
-----------------------------------------------------------------
Starting tests in: AudioBlock / Operators...
Completed tests in AudioBlock / Operators
-----------------------------------------------------------------
Starting tests in: AudioBlock / Process...
Completed tests in AudioBlock / Process
-----------------------------------------------------------------
Starting tests in: AudioBlock / Copying...
Completed tests in AudioBlock / Copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Smoothing...
Completed tests in AudioBlock / Smoothing
-----------------------------------------------------------------
Starting tests in: AudioBlock / Equality...
Completed tests in AudioBlock / Equality
-----------------------------------------------------------------
Starting tests in: AudioBlock / Constructors...
Completed tests in AudioBlock / Constructors
-----------------------------------------------------------------
Starting tests in: AudioBlock / Swap...
Completed tests in AudioBlock / Swap
-----------------------------------------------------------------
Starting tests in: AudioBlock / Getters and setters...
Completed tests in AudioBlock / Getters and setters
-----------------------------------------------------------------
Starting tests in: AudioBlock / Basic copying...
Completed tests in AudioBlock / Basic copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Addition...
Completed tests in AudioBlock / Addition
-----------------------------------------------------------------
Starting tests in: AudioBlock / Subtraction...
Completed tests in AudioBlock / Subtraction
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiplication...
Completed tests in AudioBlock / Multiplication
-----------------------------------------------------------------
Starting tests in: AudioBlock / Multiply add...
Completed tests in AudioBlock / Multiply add
-----------------------------------------------------------------
Starting tests in: AudioBlock / Negative abs min max...
Completed tests in AudioBlock / Negative abs min max
-----------------------------------------------------------------
Starting tests in: AudioBlock / Operators...
Completed tests in AudioBlock / Operators
-----------------------------------------------------------------
Starting tests in: AudioBlock / Process...
Completed tests in AudioBlock / Process
-----------------------------------------------------------------
Starting tests in: AudioBlock / Copying...
Completed tests in AudioBlock / Copying
-----------------------------------------------------------------
Starting tests in: AudioBlock / Smoothing...
Completed tests in AudioBlock / Smoothing
-----------------------------------------------------------------
Starting tests in: Convolution / Impulse responses can be loaded without allocating on the audio thread...
Completed tests in Convolution / Impulse responses can be loaded without allocating on the audio thread
-----------------------------------------------------------------
Starting tests in: Convolution / Convolution can be reset without allocating on the audio thread...
Completed tests in Convolution / Convolution can be reset without allocating on the audio thread
-----------------------------------------------------------------
Starting tests in: Convolution / Completely empty IRs don't crash...
Completed tests in Convolution / Completely empty IRs don't crash
-----------------------------------------------------------------
Starting tests in: Convolution / Convolutions can cope with a change in samplerate and blocksize...
Completed tests in Convolution / Convolutions can cope with a change in samplerate and blocksize
-----------------------------------------------------------------
Starting tests in: Convolution / Short uniform convolutions work...
Completed tests in Convolution / Short uniform convolutions work
-----------------------------------------------------------------
Starting tests in: Convolution / Longer uniform convolutions work...
Completed tests in Convolution / Longer uniform convolutions work
-----------------------------------------------------------------
Starting tests in: Convolution / Normalisation works...
Completed tests in Convolution / Normalisation works
-----------------------------------------------------------------
Starting tests in: Convolution / Stereo convolutions work...
Completed tests in Convolution / Stereo convolutions work
-----------------------------------------------------------------
Starting tests in: Convolution / Stereo IRs only use first channel if stereo is disabled...
Completed tests in Convolution / Stereo IRs only use first channel if stereo is disabled
-----------------------------------------------------------------
Starting tests in: Convolution / IRs with extra silence are trimmed appropriately...
Completed tests in Convolution / IRs with extra silence are trimmed appropriately
-----------------------------------------------------------------
Starting tests in: Convolution / IRs are resampled if their sample rate is different to the playback rate...
Completed tests in Convolution / IRs are resampled if their sample rate is different to the playback rate
-----------------------------------------------------------------
Starting tests in: Convolution / Non-uniform convolutions work...
Completed tests in Convolution / Non-uniform convolutions work
-----------------------------------------------------------------
Starting tests in: Convolution / Convolutions with latency work...
Completed tests in Convolution / Convolutions with latency work
-----------------------------------------------------------------
Starting tests in: FFT / Real input numbers Test...
Completed tests in FFT / Real input numbers Test
-----------------------------------------------------------------
Starting tests in: FFT / Frequency only Test...
Completed tests in FFT / Frequency only Test
-----------------------------------------------------------------
Starting tests in: FFT / Complex input numbers Test...
Completed tests in FFT / Complex input numbers Test
-----------------------------------------------------------------
Starting tests in: FIR Filter / Large Blocks...
Completed tests in FIR Filter / Large Blocks
-----------------------------------------------------------------
Starting tests in: FIR Filter / Sample by Sample...
Completed tests in FIR Filter / Sample by Sample
-----------------------------------------------------------------
Starting tests in: FIR Filter / Split Block...
Completed tests in FIR Filter / Split Block
-----------------------------------------------------------------
Starting tests in: ProcessorChain / After calling setBypass, processor is bypassed...
Completed tests in ProcessorChain / After calling setBypass, processor is bypassed
-----------------------------------------------------------------
Starting tests in: ProcessorChain / After calling prepare, all processors are prepared...
Completed tests in ProcessorChain / After calling prepare, all processors are prepared
-----------------------------------------------------------------
Starting tests in: ProcessorChain / After calling reset, all processors are reset...
Completed tests in ProcessorChain / After calling reset, all processors are reset
-----------------------------------------------------------------
Starting tests in: ProcessorChain / After calling process, all processors contribute to processing...
Completed tests in ProcessorChain / After calling process, all processors contribute to processing
-----------------------------------------------------------------
Starting tests in: ProcessorChain / Chains with trailing items that only support replacing contexts can be built...
Completed tests in ProcessorChain / Chains with trailing items that only support replacing contexts can be built
Random seed: 0x529ba42
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 0 -> 2 @ 44100 Hz / block 32...
Completed tests in Oversampling Reset Transition / OS 0 -> 2 @ 44100 Hz / block 32
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 2 -> 1 @ 44100 Hz / block 32...
Completed tests in Oversampling Reset Transition / OS 2 -> 1 @ 44100 Hz / block 32
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 1 -> 0 @ 44100 Hz / block 32...
Completed tests in Oversampling Reset Transition / OS 1 -> 0 @ 44100 Hz / block 32
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 0 -> 2 @ 44100 Hz / block 64...
Completed tests in Oversampling Reset Transition / OS 0 -> 2 @ 44100 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 2 -> 1 @ 44100 Hz / block 64...
Completed tests in Oversampling Reset Transition / OS 2 -> 1 @ 44100 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 1 -> 0 @ 44100 Hz / block 64...
Completed tests in Oversampling Reset Transition / OS 1 -> 0 @ 44100 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 0 -> 2 @ 44100 Hz / block 128...
Completed tests in Oversampling Reset Transition / OS 0 -> 2 @ 44100 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 2 -> 1 @ 44100 Hz / block 128...
Completed tests in Oversampling Reset Transition / OS 2 -> 1 @ 44100 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 1 -> 0 @ 44100 Hz / block 128...
Completed tests in Oversampling Reset Transition / OS 1 -> 0 @ 44100 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 0 -> 2 @ 48000 Hz / block 32...
Completed tests in Oversampling Reset Transition / OS 0 -> 2 @ 48000 Hz / block 32
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 2 -> 1 @ 48000 Hz / block 32...
Completed tests in Oversampling Reset Transition / OS 2 -> 1 @ 48000 Hz / block 32
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 1 -> 0 @ 48000 Hz / block 32...
Completed tests in Oversampling Reset Transition / OS 1 -> 0 @ 48000 Hz / block 32
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 0 -> 2 @ 48000 Hz / block 64...
Completed tests in Oversampling Reset Transition / OS 0 -> 2 @ 48000 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 2 -> 1 @ 48000 Hz / block 64...
Completed tests in Oversampling Reset Transition / OS 2 -> 1 @ 48000 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 1 -> 0 @ 48000 Hz / block 64...
Completed tests in Oversampling Reset Transition / OS 1 -> 0 @ 48000 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 0 -> 2 @ 48000 Hz / block 128...
Completed tests in Oversampling Reset Transition / OS 0 -> 2 @ 48000 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 2 -> 1 @ 48000 Hz / block 128...
Completed tests in Oversampling Reset Transition / OS 2 -> 1 @ 48000 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Oversampling Reset Transition / OS 1 -> 0 @ 48000 Hz / block 128...
Completed tests in Oversampling Reset Transition / OS 1 -> 0 @ 48000 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Phase Mode Switch Continuity / Phase mode 0 -> 1...
!!! Test 4 failed: Phase mode switch: maxDelta too high = 0.6058

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Phase Mode Switch Continuity / Phase mode 1 -> 0...
Completed tests in Phase Mode Switch Continuity / Phase mode 1 -> 0
-----------------------------------------------------------------
Starting tests in: Phase Mode Switch Continuity / Phase mode 0 -> 2...
Completed tests in Phase Mode Switch Continuity / Phase mode 0 -> 2
-----------------------------------------------------------------
Starting tests in: Phase Mode Switch Continuity / Phase mode 2 -> 0...
Completed tests in Phase Mode Switch Continuity / Phase mode 2 -> 0
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / active -> bypass @ 44100 Hz / block 64...
!!! Test 7 failed: Bypass output should match dry input after settle window, maxDiff=0.528499

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / bypass -> active @ 44100 Hz / block 64...
Completed tests in Bypass Transition Continuity / bypass -> active @ 44100 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / rapid toggle @ 44100 Hz / block 64...
Completed tests in Bypass Transition Continuity / rapid toggle @ 44100 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / active -> bypass @ 44100 Hz / block 128...
!!! Test 7 failed: Bypass output should match dry input after settle window, maxDiff=0.663198

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / bypass -> active @ 44100 Hz / block 128...
Completed tests in Bypass Transition Continuity / bypass -> active @ 44100 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / rapid toggle @ 44100 Hz / block 128...
Completed tests in Bypass Transition Continuity / rapid toggle @ 44100 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / active -> bypass @ 44100 Hz / block 512...
!!! Test 7 failed: Bypass output should match dry input after settle window, maxDiff=0.429041

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / bypass -> active @ 44100 Hz / block 512...
Completed tests in Bypass Transition Continuity / bypass -> active @ 44100 Hz / block 512
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / rapid toggle @ 44100 Hz / block 512...
Completed tests in Bypass Transition Continuity / rapid toggle @ 44100 Hz / block 512
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / active -> bypass @ 48000 Hz / block 64...
!!! Test 7 failed: Bypass output should match dry input after settle window, maxDiff=0.508444

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / bypass -> active @ 48000 Hz / block 64...
Completed tests in Bypass Transition Continuity / bypass -> active @ 48000 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / rapid toggle @ 48000 Hz / block 64...
Completed tests in Bypass Transition Continuity / rapid toggle @ 48000 Hz / block 64
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / active -> bypass @ 48000 Hz / block 128...
!!! Test 7 failed: Bypass output should match dry input after settle window, maxDiff=0.665564

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / bypass -> active @ 48000 Hz / block 128...
Completed tests in Bypass Transition Continuity / bypass -> active @ 48000 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / rapid toggle @ 48000 Hz / block 128...
Completed tests in Bypass Transition Continuity / rapid toggle @ 48000 Hz / block 128
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / active -> bypass @ 48000 Hz / block 512...
!!! Test 7 failed: Bypass output should match dry input after settle window, maxDiff=0.429689

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / bypass -> active @ 48000 Hz / block 512...
Completed tests in Bypass Transition Continuity / bypass -> active @ 48000 Hz / block 512
-----------------------------------------------------------------
Starting tests in: Bypass Transition Continuity / rapid toggle @ 48000 Hz / block 512...
Completed tests in Bypass Transition Continuity / rapid toggle @ 48000 Hz / block 512
-----------------------------------------------------------------
Starting tests in: BlockSize Regression / Dry/wet survives oversized block without re-prepare...
!!! Test 3 failed: Tail of oversized block was cleared (regression for dry/wet).
!!! Test 4 failed: Tail RMS is effectively silent.

FAILED!!  2 tests failed, out of a total of 6

Random seed: 0x7a1f4e
-----------------------------------------------------------------
Starting tests in: AIEqualizer Integration / State round-trip preserves bands and globals...
Completed tests in AIEqualizer Integration / State round-trip preserves bands and globals
-----------------------------------------------------------------
Starting tests in: AIEqualizer Integration / A/B slots preserve per-band Dynamic EQ state across save/load...
Completed tests in AIEqualizer Integration / A/B slots preserve per-band Dynamic EQ state across save/load
-----------------------------------------------------------------
Starting tests in: AIEqualizer Integration / Bypass leaves buffer untouched...
Completed tests in AIEqualizer Integration / Bypass leaves buffer untouched
-----------------------------------------------------------------
Starting tests in: LinearPhaseLatencyContract / Latency is non-negative for every phase/oversampling combination...
Completed tests in LinearPhaseLatencyContract / Latency is non-negative for every phase/oversampling combination
-----------------------------------------------------------------
Starting tests in: LinearPhaseLatencyContract / Same settings produce same latency across two independent instances...
Completed tests in LinearPhaseLatencyContract / Same settings produce same latency across two independent instances
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Stereo -> MSLinked...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Stereo -> MSLinked
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: MSLinked -> Stereo...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: MSLinked -> Stereo
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Stereo -> Mid...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Stereo -> Mid
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Mid -> Stereo...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Mid -> Stereo
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Stereo -> Side...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Stereo -> Side
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Side -> Stereo...
  maxDelta=0.0000 peakAbs=0.0000 energyRatio=0.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Side -> Stereo
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Mid -> Side...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Mid -> Side
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Side -> Mid...
  maxDelta=0.0000 peakAbs=0.0000 energyRatio=0.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Side -> Mid
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Mid -> MSLinked...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Mid -> MSLinked
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: MSLinked -> Mid...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: MSLinked -> Mid
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: Side -> MSLinked...
  maxDelta=0.0000 peakAbs=0.0000 energyRatio=0.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: Side -> MSLinked
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Symmetric: MSLinked -> Side...
  maxDelta=0.1299 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Symmetric: MSLinked -> Side
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Stereo -> MSLinked...
  maxDelta=0.1311 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Stereo -> MSLinked
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: MSLinked -> Stereo...
  maxDelta=0.1311 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: MSLinked -> Stereo
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Stereo -> Mid...
  maxDelta=0.1311 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Stereo -> Mid
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Mid -> Stereo...
  maxDelta=0.1303 peakAbs=0.9120 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Mid -> Stereo
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Stereo -> Side...
  maxDelta=0.1311 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Stereo -> Side
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Side -> Stereo...
  maxDelta=0.1303 peakAbs=0.9120 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Side -> Stereo
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Mid -> Side...
  maxDelta=0.1303 peakAbs=0.9120 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Mid -> Side
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Side -> Mid...
  maxDelta=0.1303 peakAbs=0.9120 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Side -> Mid
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Mid -> MSLinked...
  maxDelta=0.1303 peakAbs=0.9120 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Mid -> MSLinked
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: MSLinked -> Mid...
  maxDelta=0.1311 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: MSLinked -> Mid
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: Side -> MSLinked...
  maxDelta=0.1303 peakAbs=0.9120 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: Side -> MSLinked
-----------------------------------------------------------------
Starting tests in: MS Mode Switch Continuity / Asymmetric: MSLinked -> Side...
  maxDelta=0.1311 peakAbs=0.9949 energyRatio=1.0000 dropout=0
Completed tests in MS Mode Switch Continuity / Asymmetric: MSLinked -> Side
-----------------------------------------------------------------
Starting tests in: Solo Mode Transition / Solo enable transition (crossfade 256 samples)...
  Enable: maxDelta=0.1299 peakAbs=0.9949 dropout=0
Completed tests in Solo Mode Transition / Solo enable transition (crossfade 256 samples)
-----------------------------------------------------------------
Starting tests in: Solo Mode Transition / Solo disable transition...
  Disable: maxDelta=0.0651 peakAbs=0.4988 dropout=0
Completed tests in Solo Mode Transition / Solo disable transition
-----------------------------------------------------------------
Starting tests in: Solo Mode Transition / Frequency drag during solo (1kHz -> 4kHz)...
  Drag: maxDelta=0.1558 peakAbs=0.4477 dropout=0
Completed tests in Solo Mode Transition / Frequency drag during solo (1kHz -> 4kHz)
-----------------------------------------------------------------
Starting tests in: Band Drag Continuity / Band drag 500Hz -> 4kHz (Zero Latency, SmoothedValue)...
  Standard drag: maxDelta=0.1137 peakAbs=0.8606 dropout=0
Completed tests in Band Drag Continuity / Band drag 500Hz -> 4kHz (Zero Latency, SmoothedValue)
-----------------------------------------------------------------
Starting tests in: Band Drag Continuity / Band drag 500Hz -> 4kHz (Linear Phase, IR rebuild with debounce)...
  LP drag: maxDelta=0.1227 peakAbs=0.9175 dropout=0
Completed tests in Band Drag Continuity / Band drag 500Hz -> 4kHz (Linear Phase, IR rebuild with debounce)
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Bypass toggle (active â bypass â active) with 8 bands...
  Bypass@block30: maxDelta=0.1329 clicks=0 peakAbs=0.2781 dropout=0 avgDelta=0.035309
  Bypass@block60: maxDelta=0.1261 clicks=0 peakAbs=0.2341 dropout=0 avgDelta=0.033562
  Bypass@block90: maxDelta=0.1523 clicks=0 peakAbs=0.2781 dropout=0 avgDelta=0.036337
Completed tests in Host Session Click Detection / Bypass toggle (active â bypass â active) with 8 bands
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Bypass rapid toggle (every 2 blocks) stress test...
  Bypass rapid toggle: maxDelta=0.1596 clicks=0 peakAbs=0.2925 dropout=1 avgDelta=0.037234
Completed tests in Host Session Click Detection / Bypass rapid toggle (every 2 blocks) stress test
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Phase mode: Zero â Natural â Linear â Zero with audio...
  Phase: Zero→Natural: maxDelta=0.1615 clicks=0 peakAbs=0.2928 dropout=0 avgDelta=0.038430
  Phase: Natural→Linear: maxDelta=0.1788 clicks=0 peakAbs=0.2893 dropout=0 avgDelta=0.038473
  Phase: Linear→Zero: maxDelta=0.1474 clicks=0 peakAbs=0.3038 dropout=0 avgDelta=0.039425
Completed tests in Host Session Click Detection / Phase mode: Zero â Natural â Linear â Zero with audio
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Oversampling: Off â 2x â 4x â Off (Natural Phase)...
  OS: Off→2x: maxDelta=0.1514 clicks=0 peakAbs=0.2910 dropout=0 avgDelta=0.038885
  OS: 2x→4x: maxDelta=0.1788 clicks=0 peakAbs=0.2891 dropout=1 avgDelta=0.039464
  OS: 4x→Off: maxDelta=0.3158 clicks=0 peakAbs=0.3602 dropout=1 avgDelta=0.056592
Completed tests in Host Session Click Detection / Oversampling: Off â 2x â 4x â Off (Natural Phase)
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Single band freq sweep 200Hz â 10kHz (aggressive, every block)...
  Freq sweep 200â10k: maxDelta=0.1642 clicks=0 peakAbs=0.3047 dropout=1 avgDelta=0.038762
Completed tests in Host Session Click Detection / Single band freq sweep 200Hz â 10kHz (aggressive, every block)
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Single band gain sweep -18dB â +18dB...
  Gain sweep -18â+18dB: maxDelta=0.1605 clicks=0 peakAbs=0.4358 dropout=0 avgDelta=0.038268
Completed tests in Host Session Click Detection / Single band gain sweep -18dB â +18dB
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Multi-band simultaneous drag (3 bands moving at once)...
  Multi-band drag: maxDelta=0.1680 clicks=0 peakAbs=0.2992 dropout=1 avgDelta=0.037309
Completed tests in Host Session Click Detection / Multi-band simultaneous drag (3 bands moving at once)
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / A/B profile switch with different EQ curves...
  AB switch@block30: maxDelta=0.1371 clicks=0 peakAbs=0.2992 dropout=0 avgDelta=0.038187
  AB switch@block60: maxDelta=0.4011 clicks=1 peakAbs=0.7297 dropout=0 avgDelta=0.100060
!!! Test 12 failed: AB switch@block60: 1 clicks detected (max allowed: 0)
  AB switch@block90: maxDelta=0.1655 clicks=0 peakAbs=0.2970 dropout=0 avgDelta=0.039046
  AB switch@block110: maxDelta=0.3346 clicks=0 peakAbs=0.7139 dropout=0 avgDelta=0.079578

FAILED!!  1 test failed, out of a total of 24

-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Dynamic EQ threshold sweep -60dB â 0dB with active compression...
  DynEQ threshold sweep: maxDelta=0.1610 clicks=0 peakAbs=0.2959 dropout=1 avgDelta=0.038558
Completed tests in Host Session Click Detection / Dynamic EQ threshold sweep -60dB â 0dB with active compression
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Dynamic EQ mode toggle: Off â Compress â Expand â Gate â Off...
  DynEQ: Off→Compress: maxDelta=0.1432 clicks=0 peakAbs=0.2991 dropout=0 avgDelta=0.038507
  DynEQ: Compress→Expand: maxDelta=0.1528 clicks=0 peakAbs=0.3150 dropout=0 avgDelta=0.039597
  DynEQ: Expand→Gate: maxDelta=0.1654 clicks=0 peakAbs=0.3043 dropout=0 avgDelta=0.039614
  DynEQ: Gate→Off: maxDelta=0.1485 clicks=0 peakAbs=0.2957 dropout=0 avgDelta=0.039359
Completed tests in Host Session Click Detection / Dynamic EQ mode toggle: Off â Compress â Expand â Gate â Off
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / AI correction burst: 8 bands change simultaneously...
  AI correction burst: maxDelta=0.1615 clicks=0 peakAbs=0.2928 dropout=0 avgDelta=0.038960
Completed tests in Host Session Click Detection / AI correction burst: 8 bands change simultaneously
-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / Compound stress: bypass + phase + OS + drag + AB in rapid succession...
  Compound stress: maxDelta=0.6475 clicks=0 peakAbs=1.9309 dropout=99 avgDelta=0.046978
!!! Test 5 failed: Compound stress: audio dropout, 99 silent samples

FAILED!!  1 test failed, out of a total of 6

-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / All transitions @ blockSize=32 (extreme low latency)...
  Small block (32) transitions: maxDelta=0.1509 clicks=0 peakAbs=0.3197 dropout=103 avgDelta=0.032627
!!! Test 5 failed: Small block (32) transitions: audio dropout, 103 silent samples

FAILED!!  1 test failed, out of a total of 6

-----------------------------------------------------------------
Starting tests in: Host Session Click Detection / All transitions @ blockSize=2048 (high latency)...
  Large block (2048) transitions: maxDelta=0.1968 clicks=0 peakAbs=0.9042 dropout=1 avgDelta=0.039845
Completed tests in Host Session Click Detection / All transitions @ blockSize=2048 (high latency)
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real A/B continuity (near-identical profiles) [Zero]...
  [PRE block39] maxDelta=0.1371 clicks=0 peakAbs=0.2328
  [POST block40+] maxDelta=0.1615 clicks=0 peakAbs=0.2923 avgDelta=0.038999
  AB continuity[Zero]@block40: maxDelta=0.1615 clicks=0 peakAbs=0.2923 dropout=1 avgDelta=0.038999
  [PRE block79] maxDelta=0.1388 clicks=0 peakAbs=0.2526
  [POST block80+] maxDelta=0.1589 clicks=0 peakAbs=0.3210 avgDelta=0.039206
  AB continuity[Zero]@block80: maxDelta=0.1589 clicks=0 peakAbs=0.3210 dropout=0 avgDelta=0.039206
  [PRE block109] maxDelta=0.1370 clicks=0 peakAbs=0.2813
  [POST block110+] maxDelta=0.1485 clicks=0 peakAbs=0.3235 avgDelta=0.038790
  AB continuity[Zero]@block110: maxDelta=0.1485 clicks=0 peakAbs=0.3235 dropout=1 avgDelta=0.038790
Completed tests in Real Apply Path Click Regression / Real A/B continuity (near-identical profiles) [Zero]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real A/B state switch via setABState() under live audio [Zero]...
  [PRE-TRIGGER block39] maxDelta=0.1371 clicks=0 peakAbs=0.2328
  [POST-TRIGGER block40+] maxDelta=1.0219 clicks=168 peakAbs=0.5785 avgDelta=0.053973
  boundary maxDelta=0.1295 preRef maxDelta=0.1371 steady maxDelta=0.1960 ratio=0.66
  [PRE-TRIGGER block79] maxDelta=6.2679 clicks=244 peakAbs=3.2739
  [POST-TRIGGER block80+] maxDelta=11.7504 clicks=5560 peakAbs=6.5553 avgDelta=2.327836
  boundary maxDelta=11.4899 preRef maxDelta=6.2679 steady maxDelta=11.6553 ratio=0.99
  [PRE-TRIGGER block109] maxDelta=0.1703 clicks=0 peakAbs=0.2438
  [POST-TRIGGER block110+] maxDelta=0.8914 clicks=197 peakAbs=0.4909 avgDelta=0.055497
  boundary maxDelta=0.1423 preRef maxDelta=0.1703 steady maxDelta=0.1614 ratio=0.84
Completed tests in Real Apply Path Click Regression / Real A/B state switch via setABState() under live audio [Zero]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real AI single correction via applySingleCorrection() under live audio [Zero]...
  AI correction[Zero]@block50 boundary=0.4622 preRef=0.5322 steady=0.5457 ratio=0.85
  AI correction[Zero]@block54 boundary=0.4826 preRef=0.5436 steady=0.5457 ratio=0.88
Completed tests in Real Apply Path Click Regression / Real AI single correction via applySingleCorrection() under live audio [Zero]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real A/B continuity (near-identical profiles) [Natural]...
  [PRE block39] maxDelta=0.1285 clicks=0 peakAbs=0.1851
  [POST block40+] maxDelta=0.1514 clicks=0 peakAbs=0.2910 avgDelta=0.038909
  AB continuity[Natural]@block40: maxDelta=0.1514 clicks=0 peakAbs=0.2910 dropout=0 avgDelta=0.038909
  [PRE block79] maxDelta=0.1210 clicks=0 peakAbs=0.2717
  [POST block80+] maxDelta=0.1758 clicks=0 peakAbs=0.3174 avgDelta=0.038573
  AB continuity[Natural]@block80: maxDelta=0.1758 clicks=0 peakAbs=0.3174 dropout=0 avgDelta=0.038573
  [PRE block109] maxDelta=0.1288 clicks=0 peakAbs=0.3173
  [POST block110+] maxDelta=0.1435 clicks=0 peakAbs=0.2951 avgDelta=0.038147
  AB continuity[Natural]@block110: maxDelta=0.1435 clicks=0 peakAbs=0.2951 dropout=0 avgDelta=0.038147
Completed tests in Real Apply Path Click Regression / Real A/B continuity (near-identical profiles) [Natural]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real A/B state switch via setABState() under live audio [Natural]...
  [PRE-TRIGGER block39] maxDelta=0.1285 clicks=0 peakAbs=0.1851
  [POST-TRIGGER block40+] maxDelta=5.3786 clicks=677 peakAbs=2.7780 avgDelta=0.135717
  boundary maxDelta=0.1297 preRef maxDelta=0.1285 steady maxDelta=0.2391 ratio=0.54
  [PRE-TRIGGER block79] maxDelta=18.4583 clicks=247 peakAbs=9.7210
  [POST-TRIGGER block80+] maxDelta=21.1030 clicks=5488 peakAbs=10.5825 avgDelta=3.671475
  boundary maxDelta=21.1030 preRef maxDelta=18.4583 steady maxDelta=18.0827 ratio=1.14
  [PRE-TRIGGER block109] maxDelta=0.1198 clicks=0 peakAbs=0.2817
  [POST-TRIGGER block110+] maxDelta=4.2492 clicks=663 peakAbs=2.6350 avgDelta=0.132503
  boundary maxDelta=0.1334 preRef maxDelta=0.1198 steady maxDelta=0.3399 ratio=0.39
Completed tests in Real Apply Path Click Regression / Real A/B state switch via setABState() under live audio [Natural]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real AI single correction via applySingleCorrection() under live audio [Natural]...
  AI correction[Natural]@block50 boundary=0.5306 preRef=0.4946 steady=0.5577 ratio=0.95
  AI correction[Natural]@block54 boundary=0.5387 preRef=0.5306 steady=0.5291 ratio=1.02
Completed tests in Real Apply Path Click Regression / Real AI single correction via applySingleCorrection() under live audio [Natural]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real A/B continuity (near-identical profiles) [Linear]...
  [PRE block39] maxDelta=0.1371 clicks=0 peakAbs=0.2328
  [POST block40+] maxDelta=0.1615 clicks=0 peakAbs=0.2923 avgDelta=0.038999
  AB continuity[Linear]@block40: maxDelta=0.1615 clicks=0 peakAbs=0.2923 dropout=1 avgDelta=0.038999
  [PRE block79] maxDelta=0.1388 clicks=0 peakAbs=0.2526
  [POST block80+] maxDelta=0.1589 clicks=0 peakAbs=0.3210 avgDelta=0.039206
  AB continuity[Linear]@block80: maxDelta=0.1589 clicks=0 peakAbs=0.3210 dropout=0 avgDelta=0.039206
  [PRE block109] maxDelta=0.1370 clicks=0 peakAbs=0.2813
  [POST block110+] maxDelta=0.1485 clicks=0 peakAbs=0.3235 avgDelta=0.038790
  AB continuity[Linear]@block110: maxDelta=0.1485 clicks=0 peakAbs=0.3235 dropout=1 avgDelta=0.038790
Completed tests in Real Apply Path Click Regression / Real A/B continuity (near-identical profiles) [Linear]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real A/B state switch via setABState() under live audio [Linear]...
  [PRE-TRIGGER block39] maxDelta=0.1371 clicks=0 peakAbs=0.2328
  [POST-TRIGGER block40+] maxDelta=1.0219 clicks=168 peakAbs=0.5785 avgDelta=0.053973
  boundary maxDelta=0.1295 preRef maxDelta=0.1371 steady maxDelta=0.1960 ratio=0.66
  [PRE-TRIGGER block79] maxDelta=6.2679 clicks=244 peakAbs=3.2739
  [POST-TRIGGER block80+] maxDelta=11.7504 clicks=5560 peakAbs=6.5553 avgDelta=2.327836
  boundary maxDelta=11.4899 preRef maxDelta=6.2679 steady maxDelta=11.6553 ratio=0.99
  [PRE-TRIGGER block109] maxDelta=0.1703 clicks=0 peakAbs=0.2438
  [POST-TRIGGER block110+] maxDelta=0.8914 clicks=197 peakAbs=0.4909 avgDelta=0.055497
  boundary maxDelta=0.1423 preRef maxDelta=0.1703 steady maxDelta=0.1614 ratio=0.84
Completed tests in Real Apply Path Click Regression / Real A/B state switch via setABState() under live audio [Linear]
-----------------------------------------------------------------
Starting tests in: Real Apply Path Click Regression / Real AI single correction via applySingleCorrection() under live audio [Linear]...
  AI correction[Linear]@block50 boundary=0.4622 preRef=0.5322 steady=0.5457 ratio=0.85
  AI correction[Linear]@block54 boundary=0.4826 preRef=0.5436 steady=0.5457 ratio=0.88
Completed tests in Real Apply Path Click Regression / Real AI single correction via applySingleCorrection() under live audio [Linear]
-----------------------------------------------------------------
Starting tests in: RB Validation / RB-2: Slot A/B values stay coherent across rapid switching...
Completed tests in RB Validation / RB-2: Slot A/B values stay coherent across rapid switching
-----------------------------------------------------------------
Starting tests in: RB Validation / RB-2: All 4 slots survive getState â setState round-trip...
Completed tests in RB Validation / RB-2: All 4 slots survive getState â setState round-trip
-----------------------------------------------------------------
Starting tests in: RB Validation / RB-3: setStateInformation restores slots synchronously (no async gap)...
Completed tests in RB Validation / RB-3: setStateInformation restores slots synchronously (no async gap)
-----------------------------------------------------------------
Starting tests in: RB Validation / RB-4: Switch Zero Latency â HQ during processBlock â no crash...
Completed tests in RB Validation / RB-4: Switch Zero Latency â HQ during processBlock â no crash
-----------------------------------------------------------------
Starting tests in: RB Validation / RB-4: Quality mode persists across save/load and lookahead is effective...
Completed tests in RB Validation / RB-4: Quality mode persists across save/load and lookahead is effective
-----------------------------------------------------------------
Starting tests in: RB-4 Behavioral / RB-4 Behavioral: HQ lookahead reduces transient overshoot vs ZL...
ZL peak=0.0000 rms=0.0000  |  HQ peak=0.0000 rms=0.0000
!!! Test 1 failed: HQ peak (0.0000) should be lower than ZL peak (0.0000) — lookahead should reduce transient overshoot

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: RB-4 Behavioral / RB-4 Behavioral: lookaheadSamples value changes with qualityMode...
!!! Test 1 failed: HQ and ZL impulse responses should differ (lookahead delay)

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: RB-4 Behavioral / RB-4 Behavioral: runtime switch from ZL to HQ changes output...
  RMS before switch (ZL): 0.000000
  RMS after switch (HQ):  0.000000
!!! Test 1 failed: Output should change after ZLâHQ switch. Delta=0.000000

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Equivalence: saveâ â load â saveâ produces identical XML...
Idempotent roundtrip — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 2 failed: saveâ XML should be identical to saveâ XML after load

FAILED!!  1 test failed, out of a total of 2

-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Equivalence: 5 consecutive save/load cycles produce no drift...
Cycle 1 — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 1 failed: Cycle 1 XML should match reference
Cycle 2 — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 2 failed: Cycle 2 XML should match reference
Cycle 3 — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 3 failed: Cycle 3 XML should match reference
Cycle 4 — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 4 failed: Cycle 4 XML should match reference
Cycle 5 — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 5 failed: Cycle 5 XML should match reference

FAILED!!  5 tests failed, out of a total of 5

-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Equivalence: switching slots and back preserves all values...
Completed tests in RB-2 Equivalence / RB-2 Equivalence: switching slots and back preserves all values
-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Equivalence: save from procâ, load into procâ â parameter values match...
  Cross-instance: all parameters within tolerance
Completed tests in RB-2 Equivalence / RB-2 Equivalence: save from procâ, load into procâ â parameter values match
-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Equivalence: all APVTS params at non-default â roundtrip stable...
Full saturation — first diff at line 386:
  saveâ:            type="0" enabled="1" solo="0" slope="0" dynMode="1" dynThreshold="-19.99999809265137"
  saveâ:            type="0" enabled="1" solo="0" slope="0" dynMode="1" dynThreshold="-20.0"
!!! Test 1 failed: Full parameter saturation roundtrip should be stable

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Soak: 250 consecutive save/load cycles â zero drift...
Soak cycle 0 — first diff at line 382:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 1 failed: Drift detected at cycle 0

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: RB-2 Equivalence / RB-2 Soak: 50 randomized configurations â each roundtrip stable...
Seed 0 — first diff at line 545:
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 1 — first diff at line 477:
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 2 — first diff at line 557:
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 3 — first diff at line 477:
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 4 — first diff at line 554:
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 5 — first diff at line 385:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 6 — first diff at line 480:
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 7 — first diff at line 474:
  saveâ:     <band2 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band2 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 8 — first diff at line 381:
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 9 — first diff at line 381:
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 10 — first diff at line 637:
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 11 — first diff at line 401:
  saveâ:     <band8 freq="700.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band8 freq="700.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="0"
Seed 12 — first diff at line 634:
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 13 — first diff at line 563:
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 14 — first diff at line 640:
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 15 — first diff at line 377:
  saveâ:     <band2 freq="80.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band2 freq="80.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 16 — first diff at line 393:
  saveâ:     <band6 freq="350.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band6 freq="350.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 17 — first diff at line 381:
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 18 — first diff at line 480:
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 19 — first diff at line 373:
  saveâ:     <band1 freq="50.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="50.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 20 — first diff at line 486:
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 21 — first diff at line 551:
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 22 — first diff at line 397:
  saveâ:     <band7 freq="500.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band7 freq="500.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 23 — first diff at line 486:
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 24 — first diff at line 622:
  saveâ:     <band2 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band2 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 25 — first diff at line 389:
  saveâ:     <band5 freq="250.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band5 freq="250.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 26 — first diff at line 640:
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 27 — first diff at line 628:
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 28 — first diff at line 477:
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 29 — first diff at line 373:
  saveâ:     <band1 freq="50.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="50.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 30 — first diff at line 640:
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 31 — first diff at line 557:
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 32 — first diff at line 483:
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 33 — first diff at line 566:
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band8 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 34 — first diff at line 628:
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 35 — first diff at line 486:
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band6 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 36 — first diff at line 628:
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 37 — first diff at line 373:
  saveâ:     <band1 freq="50.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="50.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 38 — first diff at line 381:
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 39 — first diff at line 377:
  saveâ:     <band2 freq="80.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band2 freq="80.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 40 — first diff at line 385:
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band4 freq="170.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 41 — first diff at line 381:
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band3 freq="120.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 42 — first diff at line 545:
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 43 — first diff at line 483:
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 44 — first diff at line 544:
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 45 — first diff at line 557:
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band5 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 46 — first diff at line 471:
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band1 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 47 — first diff at line 377:
  saveâ:     <band2 freq="80.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band2 freq="80.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 48 — first diff at line 563:
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
Seed 49 — first diff at line 563:
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="0.9999999403953552"
  saveâ:     <band7 freq="1000.0" gain="3.576278686523438e-7" q="1.0" type="2" enabled="1"
!!! Test 1 failed: 50 of 50 random seeds failed roundtrip

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: Spectrum Pipeline / Partial FIFO pull does not lose samples (staging accumulates)...
!!! Test 2 failed: Second tick with 1000 total should not produce a hop
!!! Test 5 failed: Should have completed 1 hop, got 2

FAILED!!  2 tests failed, out of a total of 5

-----------------------------------------------------------------
Starting tests in: Spectrum Pipeline / Fragmented input (small blocks) continuously produces hops...
!!! Test 1 failed: Expected ~25 hops from 100Ã256 samples, got 50
  Fragmented: 50 hops, 25600 samples pulled, noUpdateTicks=50

FAILED!!  1 test failed, out of a total of 1

-----------------------------------------------------------------
Starting tests in: Spectrum Pipeline / Backlog: burst of data is caught up within reasonable ticks...
  Backlog: 16 hops in first tick, peak backlog=4096
Completed tests in Spectrum Pipeline / Backlog: burst of data is caught up within reasonable ticks
-----------------------------------------------------------------
Starting tests in: Spectrum Pipeline / Pipeline stats instrumentation reports correct values...
!!! Test 5 failed: 2048 samples / 1024 hop = 2 hops, got 4

FAILED!!  1 test failed, out of a total of 7

-----------------------------------------------------------------
Starting tests in: RB-4 Closure / A1: Determinism â two identical HQ passes produce identical output...
  Pass 1 vs Pass 2: RMS diff=-200.0 dBFS, Peak diff=-200.0 dBFS
Completed tests in RB-4 Closure / A1: Determinism â two identical HQ passes produce identical output
-----------------------------------------------------------------
Starting tests in: RB-4 Closure / A2: Repeatability â deterministic at each block size (128, 256, 512, 1024)...
  BlockSize 128: self-diff RMS = -200.0 dBFS
  BlockSize 256: self-diff RMS = -200.0 dBFS
  BlockSize 512: self-diff RMS = -200.0 dBFS
  BlockSize 1024: self-diff RMS = -200.0 dBFS
Completed tests in RB-4 Closure / A2: Repeatability â deterministic at each block size (128, 256, 512, 1024)
-----------------------------------------------------------------
Starting tests in: RB-4 Closure / B1: Playback-vs-offline â small blocks (256) vs large blocks (2048) output comparison...
  Playback (bs=256) vs Offline (bs=2048):
    Diff RMS  = -81.2 dBFS
    Diff Peak = -51.6 dBFS
    Signal RMS = 5.6 dBFS
    SNR = 86.7 dB
    Common samples compared: 47104
    bs=128 vs bs=4096: diff RMS = -75.5 dBFS
    bs=512 vs bs=2048: diff RMS = -90.7 dBFS
Completed tests in RB-4 Closure / B1: Playback-vs-offline â small blocks (256) vs large blocks (2048) output comparison
-----------------------------------------------------------------
Starting tests in: RB-4 Closure / C1: Transition glitch â no burst > 6 dB above reference during ZL->HQ switch...
  Ref peak (ZL steady): -0.0 dBFS
  Worst transition peak: -0.0 dBFS
  Overshoot: 0.0 dB above ZL ref
Completed tests in RB-4 Closure / C1: Transition glitch â no burst > 6 dB above reference during ZL->HQ switch
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / Bypass exit after long steady-state produces no click...
  bypass exit: maxDelta=0.2186 clicks=0 peakAbs=1.6691
Completed tests in Anti-Pop Regression / Bypass exit after long steady-state produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / Bypass rapid toggle (every 3 blocks) produces no click...
  rapid toggle: maxDelta=0.1896 clicks=0
Completed tests in Anti-Pop Regression / Bypass rapid toggle (every 3 blocks) produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / DynEQ threshold sweep produces no click...
  DynEQ sweep: maxDelta=0.1194 clicks=0
Completed tests in Anti-Pop Regression / DynEQ threshold sweep produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / LP IR rapid swap stress produces no click...
  LP IR swap stress: maxDelta=0.0616 clicks=0 peakAbs=1.0521
Completed tests in Anti-Pop Regression / LP IR rapid swap stress produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / DynEQ threshold sweep in Linear Phase produces no click...
  DynEQ LP sweep: maxDelta=0.1201 clicks=0 peakAbs=0.9120
Completed tests in Anti-Pop Regression / DynEQ threshold sweep in Linear Phase produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / LP band drag + DynEQ threshold drag simultaneously produces no click...
  LP+DynEQ combined: maxDelta=0.1062 clicks=0 peakAbs=1.8239
Completed tests in Anti-Pop Regression / LP band drag + DynEQ threshold drag simultaneously produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / Bypass exit with adversarial block sizes produces no click...
  blockSize=1: maxDelta=0.2598 clicks=0
  blockSize=7: maxDelta=0.2598 clicks=0
  blockSize=31: maxDelta=0.2598 clicks=0
  blockSize=64: maxDelta=0.2598 clicks=0
  blockSize=127: maxDelta=0.2598 clicks=0
  blockSize=256: maxDelta=0.2598 clicks=0
  blockSize=513: maxDelta=0.2598 clicks=0
  blockSize=1024: maxDelta=0.2598 clicks=0
Completed tests in Anti-Pop Regression / Bypass exit with adversarial block sizes produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / Storm bypass toggle (every block Ã 200) produces no click...
  storm bypass: maxDelta=0.0463 clicks=0 peakAbs=0.7661
Completed tests in Anti-Pop Regression / Storm bypass toggle (every block Ã 200) produces no click
-----------------------------------------------------------------
Starting tests in: Anti-Pop Regression / Storm parameter burst (freq+gain+Q every block Ã 100) produces no click...
  parameter burst: maxDelta=0.0745 clicks=0 peakAbs=0.6470
Completed tests in Anti-Pop Regression / Storm parameter burst (freq+gain+Q every block Ã 100) produces no click

========================================
              RESULTS                   
========================================
[PASS] AIEqualizer Integration
[PASS] AIEqualizer Integration
[PASS] AIEqualizer Integration
[PASS] LinearPhaseLatencyContract
[PASS] LinearPhaseLatencyContract
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] MS Mode Switch Continuity
[PASS] Solo Mode Transition
[PASS] Solo Mode Transition
[PASS] Solo Mode Transition
[PASS] Band Drag Continuity
[PASS] Band Drag Continuity
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[FAIL] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Host Session Click Detection
[FAIL] Host Session Click Detection
[FAIL] Host Session Click Detection
[PASS] Host Session Click Detection
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] Real Apply Path Click Regression
[PASS] RB Validation
[PASS] RB Validation
[PASS] RB Validation
[PASS] RB Validation
[PASS] RB Validation
[FAIL] RB-4 Behavioral
[FAIL] RB-4 Behavioral
[FAIL] RB-4 Behavioral
[FAIL] RB-2 Equivalence
[FAIL] RB-2 Equivalence
[PASS] RB-2 Equivalence
[PASS] RB-2 Equivalence
[FAIL] RB-2 Equivalence
[FAIL] RB-2 Equivalence
[FAIL] RB-2 Equivalence
[FAIL] Spectrum Pipeline
[FAIL] Spectrum Pipeline
[PASS] Spectrum Pipeline
[FAIL] Spectrum Pipeline
[PASS] RB-4 Closure
[PASS] RB-4 Closure
[PASS] RB-4 Closure
[PASS] RB-4 Closure
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression
[PASS] Anti-Pop Regression

----------------------------------------
Total assertions: 642
Passed:           623
Failed:           19
----------------------------------------
FAILED: 19 TEST(S) FAILED
```

==============================================================================
RISPOSTA ATTESA
==============================================================================

La PRIMA RIGA della tua risposta deve essere letteralmente:

    ANCHORED TO SHA: 4c6c2ab8b857ddfbff699a4d0e3fc4394064213b

Poi, per ciascuna delle 6 categorie, produci esattamente:

- CATEGORIA X [1-6]: [nome sintetico]
  - VERDETTO: [bug audio reale / test troppo severo / fixture rotta / bug harness]
  - ROOT CAUSE SOSPETTA: [1-3 righe, citando file:riga specifica]
  - PRIORITÀ FIX: [P0 critico / P1 importante / P2 medio / P3 basso]
  - FIX PROPOSTO: [1-3 righe, no codice, solo direzione]

Alla fine, una sezione:

- ORDINE DI ATTACCO RACCOMANDATO: [lista numerata delle 6 categorie,
  ordinate per impatto audio reale sull'utente umano, NON per numero di fail]

COSA NON VOGLIO DA TE
- NON proporre refactor del GUI, spectrum rendering, palette, font (quelli
  sono fronti paralleli e gestiti separatamente).
- NON ricostruire il codice da memoria — leggi fresco dalle URL pin-nate.
- NON suggerire "potrebbe essere" senza citare un file:riga specifica.
- NON rispondere senza la riga di anchor nella prima riga.
