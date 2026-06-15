# Debt Remediation Plan — AI Equalizer Pro

Consolidated, priority-ordered plan for all non-AI-roadmap debt found in the
integral audit (2026-06-15). Every item is code-verified (file:line) by the main
session + 3 dedicated Plan agents. The AI "beyond-market" roadmap (P3–P6) is a
separate track tracked in `AI_SCORECARD.md`.

## Invariants (every item)
- AI clean-floor sacred: AI-Sweep **0/18**, AI-Corpus green — never regress.
- Green gate = the three no-arg binaries `AIEqualizerPro_AI_Tests`,
  `_IntegrationTests`, `_PerformanceTests` each report **0 fail** (no-arg excludes
  category `KnownDebt`). NOTE: `AIEqualizerPro_Tests` (DSP unit binary) is a 4th
  binary — items whose gate lives there (e.g. N4) must name it explicitly.
- Audio thread: lock-free / alloc-free / no logging. One causal commit per item,
  each independently verifiable by the tribunale. KnownDebt promotion = fix root,
  flip the test's category back, same commit, all gates green (never relax asserts).

---

## TIER 0 — Repo hygiene (zero code risk; do first — kills perennial noise)
| ID | What | Evidence | Effort |
|---|---|---|---|
| **H1** | Untrack stale duplicate `AIEQ-mac/` (3903 files) **+** `AIEQ-mac.zip` (76 MB). Divergent copy (AIEngine 3109 vs 3846 lines); zero build refs (grep CMake/scripts clean). | `git ls-files AIEQ-mac` = 3903; `.gitignore` lacks it | S |
| **H2** | Untrack `build-mac/` (13 files) + add to `.gitignore` (only `build/` is ignored today). This is the perennial "modified" pollution in `git status`. | `git ls-files build-mac` = 13 | S |
| **H3** | Remove duplicate `Resources/Models/ml_weights_retrained.bin` (byte-identical blob `bc888c1d` to `ml_weights.bin`) + delete the dead test fallbacks (`AIEngineIntegrationAuditTest.cpp:174`, `AIThresholdCalibrationTest.cpp:197`). Runtime loads only `ml_weights.bin`. | same git blob; fallbacks never hit | S |
| **H4 (D1)** | Redirect the Retrain test's `saveWeights` from `Resources/Models/` to a temp dir → hermetic test, no source-tree writes. (Root of the stray 482 KB binary.) | `AIAccuracyTest.cpp:918-925` | S |
| **H5 (D3)** | Replace `expect(true)` placeholder with a real empty-buffer no-op assertion. | `ParametricEQTest.cpp:420` | S |

Order within tier: H1 → H3 → H4 (so the stray binary is removed outright, no `git restore`) → H2 → H5.

## TIER 1 — High-value, low-risk
| ID | What | Evidence | Effort |
|---|---|---|---|
| **N5** | Spectrum calibration sign error: 0 dBFS sine reads ~−9 dB. Fix = **add** `CALIBRATION_OFFSET_DB`, exact value **+4.773 dB** (agent-verified invariant across N, fs). Ship with a numeric regression test (0 dBFS → ~0 dB). Resolves the calibration half of GUI-3. | `SpectrumDisplayMapper.h:66-71` | S |
| **GUI-1** | Remove dead OpenGL spectrum path (~805 lines): `renderOpenGL()` no-op, `GLSpectrumComponent.h` (433) + `OpenGLSpectrumRenderer.h` (372, **zero refs**) + `GLSpectrumHelper`. KEEP the GL compositing context. | `PluginEditor.cpp:291` | M |
| **GUI-4** | Hide the live MULTI-TRACK unmasking toggle behind its (disabled) feature flag in `AIProblemPanel`; delete the never-instantiated `AIControlPanel.h` (only `#include`d). Keep AIEngine flag/API (tests green). | `AIProblemPanel.h:128`; `AIControlPanel` unused | S |
| **GUI-5** | Remove permanently-hidden orphan components (`bandViewport`, `bandToggles`, `captureWaveform`) + their construction/update code. Verify band-enable stays reachable via curve/band panel first. | `PluginEditor.cpp:1459+` | M |

## TIER 2 — Audible DSP fixes (gated, RT-safe)
| ID | What | Evidence | Effort |
|---|---|---|---|
| **N4** | `removeBand` coefficient race: stop the plain `bandStates[].coefficients`/`numActiveStages` copy; let the existing per-band version+crossfade path rebuild from atomic params. Gate lives in `AIEqualizerPro_Tests` (name it). | `ParametricEQProcessor.cpp:552-555` | S |
| **N2** | DynEQ lookahead/dry comb: dry copied pre-delay (`:381`), main delayed by lookahead → comb when Mix<100% in HQ. Fix = delay-compensate dry by the same `laSamples`. | `DynamicEQProcessor.cpp:380-381,825-826` | S-M |
| **N3** | Lookahead absent from PDC: fold the **max** DynEQ lookahead into the constant `worstCaseLatencySamples` (Maximum-Latency-Padding model) so reported latency is correct and stable across HQ↔ZL; include `laSamples` in `actualWetLatency`. | `PluginProcessor.cpp:1204,2669-2684` | M |
| **5a** | Vintage soft-clip unbounded beyond \|x\|>3 (Padé grows instead of saturating). Bound output/drive. | `ParametricEQProcessor.cpp:6-11` | S |
| **5b** | Gate-mode meter ≠ applied gate gain (different curve/domain). Store `gainToDecibels(gateAmount)` for the meter in Gate mode. | `DynamicEQProcessor.cpp:729-744` | S |

## TIER 3 — Structural / KnownDebt promotions
| ID | What | Evidence | Effort |
|---|---|---|---|
| **P2-HAZARD-001** | De-macro the test-only DATA members (`AIEngine.h:788-791`, `MLEngine.h:258-260`) → identical object layout macro on/off → fixes the inline-accessor garbage reads. PROMOTE the 3 quarantined harness tests (flip category AI-Integration/AI-Calibration). High value: unblocks safe test hooks going forward. | CMake layout divergence confirmed | M |
| **GUI-3** | Unify on the per-pixel pipeline; remove the legacy 512-bin GUI consumption + editor FFT pump. Do AFTER N5. Must NOT touch the processor `SpectrumAnalyzer` (it feeds AI capture-analyze). Keep freeze/capture/tilt. | `AdvancedSpectrumDisplay.h`; `PluginEditor.cpp:1539-1545` | M-L |
| **KD-DSP: BlockSize** | Fix dry/wet tail on oversized DynEQ blocks (likely `dryBuffer` tail-clear); promote `BlockSize Regression` → Regression. Build-gated to pin exact line. | `DynamicEQProcessor.cpp:818-831` | M |
| **KD-DSP: Perceptual 1/3** | Close the ~76-82-sample dropout on Linear-Phase/phase-mode toggle (IR-ready-gated wet-pad ramp). Riskiest DSP; do AFTER N3; promote both. | `PluginProcessor.cpp:2682-2745` | L |

## TIER 4 — Features / large
| ID | What | Evidence | Effort |
|---|---|---|---|
| **GUI-2** | User UI scale 75–200%, persisted in state (biggest perceived gap vs all 2026 leaders). Mind P2-HAZARD: no new inline processor accessors (read APVTS directly or out-of-line getter). | `PluginEditor.cpp:243-245` | L |
| **KD-FixtureRealism** | Rebuild "AIEngine Pipeline" test on pink-tilted fixtures (reuse AI-Sweep stimulus shapes), then promote out of KnownDebt. Do NOT tune fixtures to pass. | `AIAccuracyTest.cpp:636-641` | L |
| **GUI-6** | (design note) Masking/collision overlay on the spectrum (soothe2/Pro-Q language) reusing existing `highlightProblem`/`ProblemHighlight` + pre/post delta. | `AdvancedSpectrumDisplay.h:186` | M |

## TIER 5 — Standing decisions (not scheduled)
- Dormant AI modules (~3850 lines: MultiTrackUnmasking, OnlineLearning, ReferenceMatcher, AdaptiveAIEngine, NeuralNetworkWrapper) — documented EXPERIMENTAL/DISABLED, gated, zero runtime cost. Decide keep-vs-cut when P5/P6 scope firms up.
- KnownDebt-ML (MLEngine Direct recall, Retrain) → resolved by P4 retraining track, not here.

---

## Recommended global sequence
1. **TIER 0** (one short hygiene session — H1,H3,H4,H2,H5): cleans the worktree, removes 145 MB + wrong-tree hazard, makes `git status` honest.
2. **N5** + **GUI-1/4/5** (quick wins, mostly deletions + one tested fix).
3. **N4, N2, 5a, 5b** (parallelizable; N4/N2/5a/5b independent) → **N3** → then **KD-DSP** promotions (BlockSize, then Perceptual after N3).
4. **P2-HAZARD-001** (structural, unblocks test hooks).
5. **GUI-3** (after N5).
6. **GUI-2**, **KD-FixtureRealism**, **GUI-6** (features/large, last).

## Verification protocol (per commit)
```
cmake --build build-mac --target AIEqualizerPro_AI_Tests AIEqualizerPro_IntegrationTests \
      AIEqualizerPro_PerformanceTests AIEqualizerPro_Tests -j8
build-mac/Release/bin/AIEqualizerPro_AI_Tests            # 0 fail
build-mac/Release/bin/AIEqualizerPro_IntegrationTests    # 0 fail (excl. KnownDebt)
build-mac/Release/bin/AIEqualizerPro_PerformanceTests    # 0 fail
build-mac/Release/bin/AIEqualizerPro_Tests               # 0 fail (DSP gate — N4, N5, 5a/5b)
build-mac/Release/bin/AIEqualizerPro_AI_Tests --category=AI-Sweep   # 0/18 floor
# promotions: --category=KnownDebt must shrink by exactly the promoted class(es)
```
Plugin rebuild + Ableton check at the close of any tier that touches audio/GUI.
