# AI Equalizer Pro — Codebase Evaluation vs 2026 Top-Market EQs

**Method:** 7 specialized agents, one per subsystem, each instructed to read every assigned file in full and benchmark against the 2026 leaders (FabFilter Pro-Q 4, DMG EQuality, Sonnox, TDR, oeksound soothe2, iZotope Ozone/Neutron, Gullfoss, Sonible).

**STATUS — COMPLETE: 7 of 7 subsystems audited.**

| Subsystem | Status |
|---|---|
| ML / Learning (MLEngine, NeuralNetworkWrapper, UserLearning, OnlineLearningSystem) | ✅ DONE |
| Core infra / Concurrency (Source/Core/*) | ✅ DONE |
| Tests / QA / Build (Source/Tests/*, CMake, CI) | ✅ DONE |
| Core DSP / EQ engine (Source/DSP/*) | ✅ DONE |
| GUI / UX (Source/GUI/* incl. AdvancedSpectrumDisplay 3436) | ✅ DONE |
| AI detection engines (AIEngine, SemanticEQEngine, AdaptiveAIEngine, ReferenceMatcher, MultiTrackUnmasking, PerceptualFrontEnd) | ✅ DONE |
| Plugin core / processor / state (PluginProcessor 5215, PluginEditor, PresetManager, Core) | ✅ DONE |

Codebase size: 116 source files, ~60,800 LOC.

---

## EXECUTIVE SUMMARY (all 7 subsystems)

**Overall verdict: a well-engineered *chassis* wrapped around a mid-tier *sound* and a half-connected *intelligence*.** The plumbing — real-time safety, lock-free threading, bypass/PDC, state persistence, and DSP-correctness test discipline — is genuinely competitive with or better than the 2026 leaders. But on the **two axes that actually define an AI-EQ — audio quality and detection intelligence — it falls measurably short** of FabFilter Pro-Q 4 / oeksound soothe2 / iZotope Ozone-Neutron. It does **NOT yet "vastly exceed" the market**; it is a strong, careful *near*-competitor, held back precisely in its two most important dimensions.

**The recurring meta-pattern across subsystems:** elaborate scaffolding built around a core component that is *unused* or *mid-tier*. The modern perceptual front-end is computed every frame and thrown away; the "online-learning" stack is inert in shipping builds; the genre net is never trained; spectrum-zoom controls are fully built but unwired; 3 test files are dead; the entire dynamic-EQ warm-start/crossfade apparatus exists only to hide a control-rate design. There is a great deal of **aspiration encoded in code that does not ship as behavior.**

### The two product-defining gaps (where "vastly exceed" is won or lost)
**1. SOUND (DSP).** The default signal path has **no oversampling and no cramping correction** (only 1 of 3 phase modes is protected) → high shelves / high-Q bells near Nyquist are cramped and the vintage path aliases; **steep slopes are wrong** (identical biquads stacked, not Butterworth/Linkwitz-Riley); the **dynamic EQ is control-rate, not sample-accurate** (gain rebuilt on a ≥64-sample grid, mislabeled "RMS" detector, double-smoothed); coefficients are **single-precision**. Pro-Q 4 / soothe2 / Oxford are clean, correct, and sample-accurate here.

**2. INTELLIGENCE (AI/ML).** The one genuinely strong detector is **resonance** (tilt-invariant detrended prominence + multi-gate + ML reality-check veto — competitive with soothe2/Ozone for narrow resonances). Everything else falls short: the **modern PerceptualFrontEnd is wired but DIAGNOSTICS-ONLY — no detector consumes it**; the **7 broadband detectors are fragile threshold heuristics that are also frame-incoherent** (consuming-swap across frames — the exact bug the ML path was fixed for, never migrated); **multitrack unmasking is non-functional** (spread-of-masking is dead code → per-bin level compare, far below Neutron Unmask); **match-EQ has no loudness/tilt normalization**; the underlying **MLP is tiny, un-normalized, and its learning loop doesn't close**.

### Cross-cutting concurrency / correctness defects (real — but mostly NOT in the clean core)
The **PluginProcessor core itself is clean**: no audio-thread allocation, lock, or data race found — a strong result. The remaining defects live in the **engines hung off it**:
1. [HIGH] **AIEngine `processCorrections` audio-thread race** — recomputes coefficients *in place on the live active buffer* the GUI thread can be reassigning (only the index swap is protected, not this recompute path); also reads non-atomic `currentSampleRate` on the audio thread. (AIEngine.cpp:308–313.)
2. [CRITICAL-functional] **Capture ring analyzes STALE audio** — `AbstractFifo` misuse fills once then serves the OLDEST samples.

**Considered & REFUTED (verified 2026-06-26):** the earlier "ML weight hot-reload race" is NOT a live race — the model load (`AIEngine.cpp:117`) is gated by `if (!useMLDetection)` (latches true on first load, never resets → loads ONCE, not per-`prepareToPlay`), and that one load completes before `processorReady.store(true)` (`PluginProcessor.cpp:1219`), before which `processBlock` bails and the AI thread never runs inference. No concurrency window.

**Severity refined by cross-check (real, but gated/dead → NOT live ship-blockers):** the **OSC** OOB/security issue is **off by default** (env-gated, confirmed PluginProcessor:1115); **`AtomicSnapshot`** is buggy **but dead code and not on the audio path**; **multitrack unmasking** is **off by default**. Fix them, but nothing ships them today.

### Validation, UX, and state gaps
- **No `pluginval`, no `auval`, no sanitizers; macOS CI doesn't run the tests; `/GL` accidentally commented out** — the single biggest reason these defects aren't being caught.
- **GUI: no true HiDPI** (spectrum/grid blurry on Retina), **no spectrum zoom / full-screen**, **peak-only metering**, split/incorrect dB axis — below Pro-Q's UI bar. BUT the **AI-surfacing UX and accessibility EXCEED Pro-Q**.
- **State: undo/redo silently drops slope/solo/global modes; presets don't round-trip A/B/C/D slots** — user-visible.

### Genuine strengths (competitive or better — do NOT lose these)
- **RT-safety & threading core:** zero-alloc audio path, `ScopedNoDenormals`, NaN/Inf flush, a true **4-phase latency-compensated soft bypass**, internally-padded **PDC that avoids host re-negotiation on mode switch**, correct SPSC/seqlock discipline. Closer to top-tier than most JUCE projects.
- **State serialization** is transactional and restores all 4 A/B/C/D slots *including* per-band dynamics — the "sub-state dropped on save" bug class is fixed.
- **Resonance detection + the ML reality-check veto** are principled and unusually honest about false positives.
- **DSP test discipline:** biquad null-test ±0.05 dB vs JUCE across SR×freq×Q×gain×type; broad anti-click/pop/zipper `processBlock` regression — exceeds most commercial EQs.
- **One analyzer (Core+Mapper) is metrology-grade**; the partitioned-convolution frequency-domain IR crossfade and the weight-persistence format discipline (exact-size, truncate-on-save, FNV checksum) are genuinely good engineering.
- **Semantic NL EQ** (28 descriptors) and the **AI problem-panel UX/accessibility** (full AccessibilityHandler, keyboard, screen-reader) are real differentiators the leaders don't match.

---

## COMPLETED SUBSYSTEM REPORT 1 — ML / LEARNING

(See agent report — embedded verbatim below.)

> Executive: the append-bug fix is correct and verified (single 96,488 B record; truncate-on-save; exact-size + trailing-data warn on load; FNV checksum). Forward pass, BCE-through-sigmoid gradient, and determinism are sound. BUT a fundamentally inadequate feature pipeline (no per-input normalization), a genre net that is never trained, and a secondary ML stack (NeuralNetworkWrapper/OnlineLearningSystem) that is dead/no-op in shipping builds. vs 2026 ML-EQs: FALLS SHORT on capacity, feature engineering, learning — though persistence discipline exceeds the norm.

Key findings:
- [REFUTED, verified 2026-06-26] the originally-claimed weight hot-reload race is NOT live: the load is gated by `if(!useMLDetection)` (loads once) and completes before `processorReady` (PluginProcessor.cpp:1219); the AI thread runs no inference in that window.
- [HIGH] no per-input feature normalization (MLEngine.cpp:543–550) — highest-leverage accuracy fix.
- [HIGH] genreNet never trained → classifyGenre() returns random-classifier output (MLEngine.cpp:435–475).
- [HIGH] UserLearning has NO mutex; auto-save blocks UI thread; genrePrefs dropped on save (data-loss); rejection/lastUpdated state not persisted.
- [HIGH] OnlineLearningSystem + NeuralNetworkWrapper are inert in shipping builds (TFLite gated off; startOnlineTraining always false). No shipping path adapts MLEngine to the user (trainOnDataset is test-only).
- [MEDIUM] freqNet contribution is only 30% (0.3·pred + 0.7·argmax peak) and uses MSE×sigmoid' (vanishing-gradient) targets; small model capacity (~24k params, single-frame, overlapping low-freq classes); two divergent model-search paths; setWeights silently no-ops on size mismatch.

Top recommendations: add per-frame feature normalization (train+infer identically); make UserLearning thread-safe + fix persistence; resolve genreNet (train or delete); decide the fate of the dead ML stack; raise capacity / add temporal context for the weak classes (Resonance, Thinness); embed shape/provenance metadata in the weights format.

---

## COMPLETED SUBSYSTEM REPORT 2 — CORE INFRASTRUCTURE & CONCURRENCY

Files: LockFreeStructures.h (535), LockFreeAudioFIFO.h (115), CaptureService.h (427), HistoryManager.h (325), OSCParameterServer.h (317).

Key findings:
- [CRITICAL] AtomicSnapshot::read() is a multi-writer protocol with torn-read + buffer-collision bugs despite "wait-free/SPSC" labels; bounded-CAS "force-store" fallback collapses buffers (LockFreeStructures.h:94–123, 83–87). Unused (dead code) → latent.
- [CRITICAL] Retroactive capture ring is functionally wrong: AbstractFifo fills once then drops new audio and serves OLDEST samples; "analyze now" analyzes stale audio (CaptureService + LockFreeRingBuffer::readMono; comment at LockFreeStructures.h:300 is false).
- [CRITICAL] OSCParameterServer hand-rolled parser on untrusted UDP, all-interfaces bind, OOB hazards (no null-term/padding validation), no NaN/origin/rate limiting → remote OOB read + parameter-control/DoS (OSCParameterServer.h:109–161, 87, 228).
- [HIGH] Manual/auto capture lifecycle is shared mutable state edited from BOTH audio + message threads with split memory orders; genuine data race on the preview buffer (CaptureService.h:204–235, 326–346, 379–388).
- [HIGH] OSC: no origin restriction / no rate limiting on a write-capable surface; /aieq/list reads live params off the OSC thread.
- [MEDIUM] LockFreeAudioFIFO::push silently drops on overflow (no count/telemetry); HistoryManager redo stack not capped (invariant "≤20" not held); dropCount declared but never incremented.
- Strengths: SPSCQueue + LockFreeAudioFIFO correct & cache-aligned (the load-bearing primitives); HistoryManager thread-confinement is the right call; OSC writes correctly marshalled to message thread with a sound shared_ptr<atomic<bool>> liveness guard.

Verdict: FALLS SHORT, but unevenly — the audio-path SPSC primitives are sound; the failing grade is (a) the wrong-but-unused AtomicSnapshot, (b) the capture ring misuse + dual-owner lifecycle, (c) OSC network hardening. Fix P0+P1 → moves to "meets".

Top recommendations: delete/rewrite AtomicSnapshot; replace the capture ring with a real circular buffer + atomic write cursor (+ wire dropCount); replace the OSC parser with juce::OSCReceiver, bind 127.0.0.1, isfinite-filter inbound, add backpressure; give the capture lifecycle a single owner; enforce HistoryManager's thread contract on read accessors + cap redo; make FIFO push report partial writes.

---

## COMPLETED SUBSYSTEM REPORT 3 — TESTS / QA + BUILD

Read: all 50 files in Source/Tests/, CMakeLists.txt (1176), build scripts, both CI workflows, toolchains.

Coverage — strong (real gating asserts): biquad null-test (±0.05 dB); anti-click/pop/zipper (11 scenarios incl. oversampled HQ + compressing-band drag + adversarial block sizes); block-size/SR fuzzing; XML state soak/idempotency; linear-phase gain/latency; dynamic-EQ behavior (dB-delta); AI detection matrix (recall ≥85%, clean-FP ≤5% ML/≤20% hybrid) + corpus mirror-equivalence/determinism.

CRITICAL/HIGH gaps:
- [CRITICAL] NO pluginval, NO auval anywhere; macOS CI does not run the tests (only Windows CI runs ctest).
- [HIGH] 3 dead gtest files (#include <gtest/gtest.h>, GoogleTest not in build) — RecallDeterminismTest, DynEQRuntimeValidation, RandomizedStressHarness (+ TestMain.cpp, the gtest harness) — imply coverage that doesn't run. (Correction, verified 2026-06-26: SmoothedValueZipperTest.cpp is a `juce::UnitTest`, NOT gtest — not part of this dead-gtest set.)
- [HIGH] No enforced performance/RT-safety gate (the only one, EQGraphFluidityTest, quarantined as flaky 2026-06-17); no audio-thread allocation/lock detector.
- [HIGH] No sanitizers (ASan/UBSan/TSan) in any build/CI.
- [HIGH] Build: MSVC Release `/GL` accidentally commented out (CMakeLists.txt:822) while `/LTCG` still passed → broken whole-program opt on Windows.
- [MEDIUM] Real bugs parked as KnownDebt (ML ~40% Resonance recall; block-size DSP tail bug); no warnings-as-errors; no code signing/notarization; macOS CI untested; ~500 lines of duplicated Windows-SDK detection + a machine-specific windows-toolchain.cmake.
- Strengths: JUCE 8.0.10, C++20, universal binary with correct -march handling, ScopedNoDenormals, no -ffast-math, 4 well-scoped test targets.

Verdict: parts EXCEED the bar (DSP-correctness/anti-click tests), but the suite FALLS SHORT of a shippable 2026 top-tier bar — almost entirely on missing standard validation/CI infrastructure (pluginval/auval/sanitizers/CI-test-on-mac/signing) plus honesty-debt items.

Top recommendations: add pluginval (strictness 10, VST3+AU) + auval to CI; fix the `/GL` flag; make macOS CI run ctest; add ASan/UBSan/TSan CI jobs; resurrect or delete the 4 orphaned tests; restore a real performance/RT-safety gate + audio-thread alloc/lock detector; warnings-as-errors; code signing/notarization; true flat-EQ null-test + both-channel click analysis; PresetManager round-trip/corrupt-file test.

---

## COMPLETED SUBSYSTEM REPORT 4 — CORE DSP / EQ ENGINE (the most important subsystem)

Files (all read in full): BiquadCoefficients.h, ParametricEQProcessor.{h,cpp}, DynamicEQProcessor.{h,cpp}, LinearPhaseProcessor.{h,cpp}, PartitionedConvolver.h, SpectrumAnalyzer.{h,cpp}, SpectrumAnalyzerCore.h, SpectrumDisplayMapper.h.

**Headline verdict:** the static EQ's *infrastructure* (RT-safety, click-free updates, partitioned convolution) is **near reference-grade**, but the *sound-defining DSP* is **mid-tier — it does NOT "vastly exceed" the 2026 leaders; on filter accuracy, slopes, dynamic fidelity, and aliasing it falls measurably short, with parity only in plumbing.**

CRITICAL / HIGH findings:
- [HIGH→headline gap] **No oversampling on the default EQ path; cramping uncorrected.** Oversampling exists ONLY in PluginProcessor's NaturalPhase branch (separate HQ processors, PluginProcessor.cpp:1853–1898); **ZeroLatency and LinearPhase modes run plain RBJ biquads at base rate** (:1945–1971) → a +12 dB high shelf / high-Q bell near Nyquist is **cramped** (wrong magnitude approaching fs/2) and the vintage tanh path **aliases**. Pro-Q 4 keeps the top octave analog-accurate and clean.
- [HIGH] **Steep slopes are wrong.** 24/48 dB-oct cuts stack IDENTICAL RBJ biquads (ParametricEQProcessor.cpp:198–199, 1031–1037), not Butterworth/Linkwitz-Riley Q-staggered cascades → wrong −3 dB point + non-flat passband. Audible vs every reference.
- [CRITICAL] **Dynamic EQ is control-rate, not sample-accurate.** Band gain rebuilt on a ≥64-sample grid only when it moves >0.5 dB (DynamicEQProcessor.cpp:677–697), with an elaborate warm-start/crossfade/epsilon/rate-limit apparatus existing solely to suppress the resulting crackle. "RMS" is peak-of-instantaneous into a dB one-pole (:641–651); gain smoothing is a SECOND dB one-pole on top (:660–661); stereo is forced-mono. Smears transients vs soothe2/Nova/Pro-Q (sample-accurate).
- [HIGH] **Float coefficients** (BiquadCoefficients.h:11–14) — LF/high-Q precision below the double-precision leaders; no Nyquist/de-cramping correction.
- [HIGH] **Linear-phase IR**: hannGainComp == 1.0 (no-op placeholder → gain/curve mismatch, LinearPhaseProcessor.cpp:197); fixed 4096-tap IR → LF ripple/pre-ring (too short for surgical LF linear-phase).
- [HIGH] **PartitionedConvolver heap-allocs in an IR-update method** that claims RT-safety (PartitionedConvolver.h:149); time-domain entry points not RT-safe.
- [MEDIUM] **The plugin-wired analyzer (SpectrumAnalyzer) is uncalibrated** (~6 dB Hann offset, SpectrumAnalyzer.cpp:219–224) while a correct metrology-grade analyzer (SpectrumAnalyzerCore+SpectrumDisplayMapper) exists beside it — two parallel analyzers disagreeing by several dB.
- [MEDIUM] addBand/removeBand copy audio-thread-owned coefficients from the message thread without the crossfade-queue marshalling (race); sampleRate>192000 → 4× @ 96 kHz HQ silently no-ops the EQ (ParametricEQProcessor.cpp:1047); vintage tanh aliases (no oversampling).

Strengths: RT-safety/lock-free is solid and in places exemplary (SPSC crossfade-command marshalling; the frequency-domain IR-blend crossfade in PartitionedConvolver is genuinely good); click-free static-EQ updates are more thorough than most plugins; partitioned convolution (128-sample latency, click-free IR swap) is correct; one analyzer (Core+Mapper) is metrology-grade (Parseval-correct, calibrated to dBFS).

Top recommendations (P0): true Butterworth/LR Q-staggered slopes; add cramping/Nyquist correction (matched-biquad design) OR oversample the ZeroLatency/LinearPhase paths too; re-architect the dynamic engine to sample-accurate continuous gain modulation (removing the whole warm-start/crossfade apparatus). P1: double-precision coefficients; true power-domain RMS detector + decoupled smoothing + stereo modes; real Hann gain comp + frequency-adaptive LP IR length; oversample the vintage tanh. P2: RT-safe IR entry points; marshal band add/remove; raise the 192 kHz bypass ceiling; consolidate onto the calibrated analyzer.

---

## COMPLETED SUBSYSTEM REPORT 5 — GUI / UX

Files (all 19 in Source/GUI/ read in full): AdvancedSpectrumDisplay.h (3436), AIProblemPanel.h (1147), ModernLookAndFeel.h, PremiumKnob.h, LevelMeter.h, BandControlPanel.h, DynamicEQPanel.h, SemanticControlPanel.h, AnalyzerSettingsPanel.h, NewSpectrumPipeline.h, EQBandControl.h, LogScaleMapper.h, + small/legacy files.

**Headline verdict:** the interaction model and the AI assistant UX are **at or ABOVE Pro-Q 4 conceptually**, but render fidelity (HiDPI), analyzer feature-completeness (zoom/full-screen/EQ-match), dB-axis correctness, and metering depth are **clearly below the 2026 leaders.** A strong near-Pro-Q graph with a unique AI layer, held back by sharpness + missing flagship features.

CRITICAL / HIGH findings:
- [CRITICAL] **No true HiDPI/Retina correctness.** gridCache + spectrumImageCache allocated at logical px and blitted 1:1 (AdvancedSpectrumDisplay.h:259, 1786) → grid/labels/spectrum render SOFT/BLURRY on 2× displays while the EQ curve/nodes (drawn directly) are crisp. The single most visible gap vs Pro-Q.
- [HIGH] **Per-frame cost storms.** processor.getBandState(i) does ~7 runtime String concats + APVTS lookups per call, invoked per-band per-paint and per mouseMove (×24 bands × 60 Hz, e.g. :2472, 2487, 2861); AIEngine::getPendingCorrections() takes a mutex + returns a full vector copy 3–4× per paint and per mouseMove (:536, 2099, 2233, 2458, 1133). Read an atomic snapshot once per tick instead.
- [HIGH] **dB-axis is split/wrong.** Spectrum maps 0..+12 dB into the TOP half and −90..0 into the BOTTOM half (:2894–2907) → audible range crushed into 50%; EQ nodes use a DIFFERENT axis (gainToY −24..+24, :2910) → spectrum peak and EQ node at the same dB sit at different heights. Pro-Q shares one honest axis.
- [HIGH] **No spectrum zoom / no full-screen analyzer.** Freq axis hard-pinned 20 Hz–20 kHz; SpectrumZoomControls are fully built but UNWIRED (AnalyzerSettingsPanel.h:234) — a Pro-Q staple advertised in code but absent.
- [HIGH] **No EQ Match** in the UI (capture is a static dashed reference only, :1935); metering is **peak-only** (no true-peak/RMS/LUFS, LevelMeter.h) with frame-rate-coupled ballistics; **no global gain-reduction meter** (GR buried in a per-band overlay).
- [HIGH] **High-contrast mode is half-wired** (a11y false promise — Colors:: tokens never consult the HC flag, ModernLookAndFeel.h:18,169); **DynamicEQPanel re-implements the compressor transfer function** GUI-side (computeExpectedGR :380) = a second source of truth that can silently diverge from the DSP; the **FFT pipeline runs on the message thread** (≤16 hops/tick, NewSpectrumPipeline.h:71 / PluginEditor.cpp:1461) → UI-latency spikes on heavy FFT sessions.
- [MEDIUM] Amber-only filmstrip knobs can't carry per-band color identity; startup pre-selects band 0 + shows its tooltip; scattered 30 Hz timers contradict the codebase's single-heartbeat doctrine; uncached AI-suggestion biquad redraw per pixel per paint.

Strengths (real, some EXCEED Pro-Q): sophisticated paint-cost engineering (off-screen grid/spectrum image caches, version-gated EQ-curve path, zero-alloc Catmull-Rom path builder, adaptive 5/30/60 Hz timer); broad Pro-Q-aware curve interaction (drag, Shift-Q, wheel-Q, double-click create/reset, Alt-delete, right-click menus); snapshot-safe AI FIX-on-curve; **AI surfacing UX (problem cards with cause/impact/confidence + one-click FIX in list and on curve) EXCEEDS Pro-Q** (the product's genuine differentiator); **AIProblemPanel accessibility is best-in-class** (full AccessibilityHandler, complete keyboard model, screen-reader announcements, RTL) — exceeds the field.

Top recommendations (P0): fix HiDPI image caches (or draw under the already-attached OpenGL); unify the dB axis (audible range gets most of the canvas, spectrum+curve share it); kill the getBandState/getPendingCorrections per-frame storms (atomic snapshot once/tick); move the FFT pipeline off the message thread. P1: wire spectrum zoom + add full-screen; per-band solo/listen from the curve + continuous GR on the curve; deepen metering (true-peak/RMS/LUFS + global GR); single source of truth for the DynEQ GR curve. P2: make high-contrast real (or remove); per-band knob color; don't pre-select band 0; cache the AI-suggestion curve; delete dead code (unwired zoom, legacy ProblemRow/BandTabBar, hidden labels) + consolidate timers onto the editor heartbeat.

---

## COMPLETED SUBSYSTEM REPORT 6 — AI DETECTION / DECISION ENGINES

Files (read in full): AIEngine.{h,cpp} (3952), SemanticEQEngine.{h,cpp} (1377), AdaptiveAIEngine.{h,cpp}, ReferenceMatcher.{h,cpp}, MultiTrackUnmasking.{h,cpp}, PerceptualFrontEnd.{h,cpp}, AIEngine_Advanced.cpp. (MLEngine MLP internals referenced only — audited in Report 1.)

**Threading baseline (confirmed):** `analyzeSpectrum()` runs on a dedicated AI worker thread draining a lock-free SPSC queue (PluginProcessor.cpp:472–531); the audio thread only enqueues spectra + calls `processCorrections()`. So per-frame heap allocations inside detectors cost analysis latency, NOT audio glitches.

CRITICAL / HIGH findings:
- [HIGH] **Audio-thread data race on the approved-corrections vector.** `processCorrections()` (audio thread) on `correctionCoeffsNeedUpdate` calls `updateCachedCoefficients(activeIdx, approvedCorrectionBuffers[activeIdx])` — recompute IN PLACE on the live active buffer the GUI thread can be reassigning under `correctionsWriteMutex` (which the audio thread doesn't take). The double-buffer swap protects the index flip, not this in-place recompute. Also reads non-atomic `currentSampleRate`/`strength` on the audio thread. (AIEngine.cpp:311–313, 360–389.)
- [HIGH] **The modern PerceptualFrontEnd is wired but DIAGNOSTICS-ONLY** — no detector consumes rawDb/bandDbFused/salienceDb/fluxDb (header states it explicitly); the processor drains its FIFO only to publish CPU counters. The single most "exceed-the-leaders" component contributes ZERO to detection; the real detectors still run on the legacy ballistics-smoothed spectrum. (PerceptualFrontEnd.h:6–13, PluginProcessor.cpp:481–498.)
- [HIGH] **The 7 broadband heuristic detectors are NOT frame-coherent.** detectHarshness/Muddiness/Boxyness/Sibilance/LowEndBoom/ThinSound/DullSound each call `calculateBandEnergy`/`findPeakInRange`/etc., and each of those independently calls the CONSUMING `readSpectrumSnapshot()` triple-buffer swap → one detection pass compares band energy from frame N against "overall" from frame N-1. This is the exact P4-BUG-001 bug the ML vetoes were fixed for (via scratchTemp + pure `bandEnergyFromSpectrum`/`findPeakInSpectrum`) — never migrated to the heuristic family. On transient/non-stationary audio every broadband decision is computed across mismatched frames. (AIEngine.cpp:866–911, 1585–2050 vs the pure variants 2275–2361.)
- [CRITICAL-algorithmic] **MultiTrackUnmasking models masker and probe at the SAME frequency** (`calculateMaskingThreshold(maskerMag, freq, freq)`, MultiTrackUnmasking.cpp:210) → `freqRatio=1.0` → spread-of-masking = 0 → the entire psychoacoustic spread function is DEAD CODE; the detector reduces to per-bin level comparison, not masking. Far below Neutron Unmask. (OFF by default.)
- [MEDIUM] **ReferenceMatcher** has no loudness/tilt normalization before `reference−input` subtraction (ReferenceMatcher.cpp:223–250) → the match curve is dominated by an arbitrary broadband level offset, then crudely clipped; smoothing is a linear-bin moving average (over-smooths HF, under-smooths LF). Below Ozone Match-EQ.
- [MEDIUM] **Genre detection is a hardcoded decision tree of magic dB thresholds** (detectGenre 2105–2139), electronic-only, first-match-wins — a cosmetic label, not a classifier.
- [MEDIUM] Hybrid-mode resonance supplement re-runs the consuming path on a DIFFERENT frame than the ML vetoes just validated (AIEngine.cpp:3252).

Strengths: the **resonance detector is the standout** — OLS detrended (tilt-invariant) prominence, adaptive per-frequency windows, parabolic interpolation, multi-gate (prominence → z-score OR temporal-consensus OR octave-salience → harmonic rejection via autocorrelation f0 → coherence → confidence 0.45), competitive with the leaders for narrow resonances; the **ML reality-check veto** (re-validate the model's claim against the published spectrum: ≥11 dB prominence for resonance, trend-based ≥3 dB band-excess for mud/box/boom/sib) is genuinely good defensive design; **PerceptualFrontEnd is real psychoacoustics** (equal-loudness ~75-phon salience, dual-resolution FFT, spectral flux) — just unused; **SemanticEQ** 28 descriptors with context modifiers + per-quality learning is a real differentiator; code comments are unusually honest about residual false-positive trade-offs.

Top recommendations (P0): fix the processCorrections race (compute into inactive buffer + index swap; stop reading the live vector + non-atomic SR on the audio thread). P1 (highest leverage): WIRE PerceptualFrontEnd into the detectors; make the 7 broadband detectors frame-coherent (one scratchTemp snapshot/pass, route through the pure variants) + replace their band-excess heuristics with the existing `computeTrendBandExcess`. P2: rebuild unmasking (evaluate spread across probe≠masker, cross-track gating, A↔B conflict resolution); normalize ReferenceMatcher (loudness/tilt + log smoothing); replace genre with a learned classifier.

---

## COMPLETED SUBSYSTEM REPORT 7 — PLUGIN CORE / PROCESSOR / STATE

Files (read in full): PluginProcessor.{h,cpp} (864+5215), PluginEditor.{h,cpp} (lifecycle/threading/processor-access), LockFreeStructures.h, LockFreeAudioFIFO.h, CaptureService.h, HistoryManager.h, OSCParameterServer.h, PresetManager.{h,cpp}, Logger.{h,cpp}.

**Headline verdict: a genuinely strong, RT-safety-conscious core — closer to top-tier than most JUCE projects. NO CRITICAL audio-thread allocation or data race found in the processor itself.** Weaknesses concentrate in (a) the 5215-line god-class + confirmed dead code, (b) state-persistence gaps in undo/redo + presets, (c) low-severity RT-purity nits.

Verdict by dimension: processBlock RT-safety **MEETS / approaches EXCEEDS**; threading correctness **MEETS**; state robustness **PARTIALLY MEETS**; PDC correctness **MEETS / arguably EXCEEDS**; maintainability **FALLS SHORT**; multi-instance/bus **MEETS**; double-precision **FALLS SHORT (minor)**.

Findings:
- [MEDIUM] **Undo/redo silently drops `Slope` and `Solo` per band, and ALL global/slot state** (phaseMode/msMode/oversampling/dynEqMix/dynAutoMakeup not in history). `BandSnapshot` has no slope/solo fields. Ctrl-Z gives a partial revert — the most user-visible state bug. (HistoryManager.h:31–47, 228–304.)
- [MEDIUM] **Presets don't round-trip A/B/C/D slots** — they persist only `apvts.copyState()`, not the SlotA..D trees that `getStateInformation` adds; loading a preset leaves stale slot data, so post-load A→B switching can surface the previous session's bands. (PresetManager.cpp:406, 320; loadPreset 427–468.)
- [MEDIUM] **Dead per-band work every block** in `loadParameterSnapshot` (~24×9 atomic loads + clamps, 5171–5191) that `processBlock` never consumes (it runs off `cachedParams`); plus a latent `dynamicMode = jlimit(0,2,…)` that would silently turn Gate→Expand if the snapshot is ever wired back in. (5184.)
- [MEDIUM-maintainability] **5215-line god-class** mixing routing/crossfades/M-S/AI-orchestration/capture/A-B/serialization; confirmed dead code (`linearPhaseDelayBuffer`/`linearPhaseDelayWritePos`, `meterDataReady`, no-op `updateReportedLatency`); use-before-set `worstCaseLatencySamples` at 1123 (latent).
- [FALLS SHORT, minor] **No `supportsDoublePrecisionProcessing` / double processBlock** — double hosts get JUCE's float wrapper; Pro-Q processes native double on the master bus.
- [LOW] per-sample integer `%` in the dry-delay/wet-pad ring loops (non-pow2 → can't mask; branch-wrap is cheaper); unconditional `std::tanh` soft-limiter per sample; `WaitableEvent::signal()` reachable from a host-automation `parameterChanged` (theoretical priority-inversion, not a race).

Strengths (competitive / better): `ScopedNoDenormals` + NaN/Inf flush + soft limiter; zero-alloc audio path; `processorReady` acquire-gate makes plugin-scan processBlock safe; block-clamp + channel-mismatch guards (better than most commercial plugins); **true 4-phase latency-compensated soft bypass** with dry-delay phase-alignment (excellent, a real differentiator); snapshot+counter param propagation + per-band SmoothedValue anti-zipper; **PDC = fixed worst-case reported once + internal wet-padding that avoids host re-negotiation glitches on mode switch**; seqlock single-writer discipline for the IR shadow + SPSC everywhere on the audio boundary; **state save/load transactional + restores all 4 slots incl. per-band dynamics**; Logger RT-safe (lock-free SPSC from the audio thread, file/console disabled in release); OSC off by default + `callAsync` liveness-guarded; editor destructor order correct (stopTimer → join AI thread → detach GL).

Top recommendations: complete undo/redo (slope/solo/globals); round-trip preset slots (or sync slots on loadPreset); add a stored format-version gate to setStateInformation; delete the dead per-band snapshot loop + fix the clamp; branch-wrap the ring modulo; guard the tanh limiter; marshal IR-rebuild via a polled flag; remove dead code; **extract BypassEngine / PhaseRouter / StateSerializer from the god-class** (biggest maintainability lever); add native double-precision processing to exceed Pro-Q on master-bus integrity. (Confirmed: NO MLEngine/neural inference on the audio path — the core is architecturally ready for a future production-ML ticket without touching processBlock.)

---

## UNIFIED ROADMAP TO TOP-MARKET PARITY (then beyond)

**P0 — correctness/safety (defects shipping today or one toggle away; nothing else matters until these are caught):**
1. Fix the **1 live data race**: AIEngine `processCorrections` (compute into the inactive buffer + atomic index swap; stop reading the live vector + non-atomic SR on the audio thread). (The earlier "ML weight hot-reload race" was verified and refuted — see Report 1 / executive.)
2. Fix the **capture ring** (real circular buffer + atomic write cursor) so "analyze now" sees current audio.
3. **Add `pluginval` (strictness 10) + `auval` to CI; run the test suite on macOS CI; add ASan/UBSan/TSan jobs; fix the `/GL` flag.** Without this, none of the above gets caught before users do.

**P1 — the two product-defining axes (this is where "vastly exceed" is actually won):**
- **SOUND:** true Butterworth/Linkwitz-Riley slopes; oversampling + cramping correction on the DEFAULT path (not just NaturalPhase); sample-accurate dynamic-EQ gain modulation (retire the warm-start/crossfade apparatus); double-precision coefficients.
- **INTELLIGENCE:** WIRE PerceptualFrontEnd into the detectors; make the 7 broadband detectors frame-coherent + trend-baselined (`computeTrendBandExcess`); add per-input feature normalization to the MLP; resolve genreNet (train or delete) + the inert learning stack.

**P2 — UX + state + reach features:**
- **GUI:** fix HiDPI image caches; unify the dB axis; wire spectrum zoom + full-screen; deepen metering (true-peak/RMS/LUFS + global GR); kill the per-frame getBandState/getPendingCorrections storms.
- **STATE:** complete undo/redo (slope/solo/globals); round-trip preset slots.
- **FEATURES:** rebuild unmasking (real spread-of-masking + cross-track gating); normalize match-EQ (loudness/tilt + log smoothing).

**P3 — maintainability / hygiene:**
Extract the 5215-line god-class (BypassEngine / PhaseRouter / StateSerializer); delete dead code (linearPhaseDelayBuffer, unwired SpectrumZoomControls, 3 dead gtest files, AtomicSnapshot, legacy ProblemRow/BandTabBar); consolidate the two analyzers + the scattered 30 Hz timers onto one heartbeat; add native double-precision processing.

---

*Audit complete: 7/7 subsystems, 116 files / ~60,800 LOC, benchmarked against the 2026 top-market EQs. The path to "vastly exceed" is concentrated and knowable — P0 fixes the defects the missing validation can't catch; P1 closes the two axes (sound + intelligence) that actually define the product.*
