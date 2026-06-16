# AI Detection Scorecard

Tracked measurement ledger for the AI detection quality roadmap (Roadmap v1).
Every number here is EMITTED BY A TEST — nothing is hand-estimated. Update this
file in the same commit as any change that moves a number.

- Synthetic matrix / robustness: `AIEqualizerPro_AI_Tests --category=AI-Sweep`
- Corpus baseline: `AIEqualizerPro_AI_Tests --category=AI-Corpus --verbose`
- Diagnostics / witnesses: `--category=AI-Diag --verbose`

**Baseline recorded at:** commit `dd805620` (P1 Commit 1), 2026-06-10, macOS,
Release build, 48 kHz fixtures.

## 0. Gate policy (N1, honest gates — 2026-06-14)

The no-arg run of every test binary executes **all registered categories EXCEPT
`KnownDebt`**. `--all` runs everything incl. KnownDebt (may be red). The cited
"full suite 1,391,262" of earlier commits was JUCE's own self-test categories,
NOT the AI-detection gates — fixed by N1.

**GREEN GATE (blocking) = all three binaries no-arg report 0 fail:**
```
build-mac/Release/bin/AIEqualizerPro_AI_Tests            # 0 fail
build-mac/Release/bin/AIEqualizerPro_IntegrationTests    # 0 fail (excl. KnownDebt)
build-mac/Release/bin/AIEqualizerPro_PerformanceTests    # 0 fail
```
**Visibility (mandatory, non-blocking):** `--category=KnownDebt` runs the
documented quarantine; it WILL show failures until each debt is fixed/promoted.
Promotion = fix the root, flip the category back to its real one, in one commit.

### KnownDebt registry (quarantined 2026-06-14, exposed by N1)
| Bucket | Test class(es) | Root | Promotes when |
|---|---|---|---|
| KnownDebt-TestHarness | AI Integration Audit (Pipeline Diagnostics, Direct vs Pipeline); AI Threshold Calibration (Pipeline Validation) | **P2-HAZARD-001**: inline test hooks read divergent JUCE_UNIT_TESTS object layout (`isUsingMLDetection()=FALSE` after load SUCCESS → throw) | de-macro the gated data members |
| KnownDebt-ML | AI Accuracy — MLEngine Direct; Retrain + Re-evaluate | ML recall debt (Res ~40%, Thin ~30%) on synthetic fixtures. NOTE: NOT "64 Gaussians" — the shipped model is the c4a72b0f retrain (2850 samples, spectral tilt + clean + hard-negatives, BCE, 300 epochs, F1 65.2%). P4 diagnosis (3 agents): the 40/30% are mostly **eval artifacts** (Thinness train/eval shape mismatch; Resonance freq-range collision with Harshness + the 0.02 margin), not pure model failure — fix the ruler first (P4-D1). | P4-D1 ruler fix → P4-D2 re-measure |
| KnownDebt-FixtureRealism | AI Accuracy — AIEngine Pipeline | dead-flat fixtures unrealistic for the full pipeline → 100% clean FP (NOT a floor breach: AI-Sweep 0/18 + AI-Corpus authoritative) | rebuild test on pink-tilted fixtures |
| KnownDebt-DSP | BlockSize Regression; Perceptual TEST 1 / TEST 3 | dry/wet tail on oversized blocks; brief dropout on Linear-Phase / phase-mode toggle (latency-switch buffer flush) | DSP fixes (crossfade/defer latency switch; oversized-block tail) |

Healthy classes kept BLOCKING (not quarantined): AI Integration Audit — Bin
Mismatch Check; AI Threshold Calibration — Per-Class Sweep; Perceptual TEST 2/4/5.

---

## 1. Gate ledger — floors vs targets

| Class | Metric | Status | Rule |
|---|---|---|---|
| **FLOOR** (never reopened) | Synthetic clean FP cells | **0/18** | Hard gate, every commit |
| **FLOOR** | Multi-seed final FP — MLOnly | **0/64 (0.0%)** | Must not worsen |
| **FLOOR** | Multi-seed final FP — Hybrid | **4/64 (6.2%)** | Must not worsen (target ↓ ≤3%) |
| **FLOOR** | Multi-seed frame-level FP | **85/2048 (4.2%)** | Must not worsen |
| **FLOOR** | Synthetic resonance recall | **128/128 (100%)** | Must not worsen |
| **INFRA** | Mirror equivalence vs SpectrumAnalyzer | **0.00000 dB** (limit 0.1) | Hard gate |
| **INFRA** | Corpus determinism (2 identical runs) | **identical** | Hard gate |
| **BASELINE** | Corpus table below | recorded | known_fail = logged debt, not asserted |

## 2. Corpus baseline (live semantics: rate limiter + temporal persistence ON)

Emitted by AI-Corpus at the baseline commit:

| clip | expected | backend | sens | detections | status |
|---|---|---|---|---|---|
| res3200_pink.wav | Resonance@3200 | ML | 0.2 | (none) | KNOWN_FAIL |
| res3200_pink.wav | Resonance@3200 | Hybrid | 0.2 | Res@3198 c=0.76 | **PASS** |
| res3200_pink.wav | Resonance@3200 | ML | 0.5 | (none) | KNOWN_FAIL |
| res3200_pink.wav | Resonance@3200 | Hybrid | 0.5 | (none) | KNOWN_FAIL |
| clean_pink.wav | None | ML+Hybrid | 0.2/0.5 | (none) | PASS ×4 |
| clean_dark_tilt.wav | None | ML | 0.2/0.5 | (none) | PASS ×2 |
| clean_dark_tilt.wav | None | Hybrid | 0.2 | Res@8603 c=0.47 | KNOWN_FAIL |
| clean_dark_tilt.wav | None | Hybrid | 0.5 | (none) | PASS |

## 3. Known gaps (technical debt — registered, not masked)

| ID | What | Evidence | Expected fix |
|---|---|---|---|
| **P1-GAP-001** | True 3.2 kHz resonance MISSED in the live path: ML at every sensitivity, Hybrid at 0.5. ML raw probabilities over-fire (~1.0 on 4 classes — synthetic-trained model, the c4a72b0f retrain) and the decision rule + AIEngine reality-check vetoes flatten everything on real audio. **Bonus finding:** Hybrid PASSES at sens 0.2 but fails at 0.5 — a real-audio sensitivity inversion (heuristic candidate spray at higher sens saturates/competes in the persistence layer; same family as the Ticket #3 inversion). | AI-Corpus baseline; AI-Diag raws witness | P2 (perceptual front-end) + P4 (ML retraining on corpus) |
| **P1-GAP-002** | Clean dark-tilt material emits one heuristic `Res@8.6 kHz c=0.47` (Hybrid, sens 0.2). Same HF/tilt fragility family as the known low-sens heuristic FP. Survives temporal persistence (it is stable, not flicker). | AI-Corpus baseline | P2 front-end (octave-stable HF salience for the heuristic path) |
| **P1-QUIRK-001** | `SpectrumAnalyzer::prepare()` does not recompute the attack/release smoothing coefficients: at 48 kHz the live pipeline runs 44100-derived coeffs (at 96 kHz the discrepancy doubles). Discovered because the "correct" mirror deviated 1.59 dB from reality; the mirror now replicates the quirk (documented in `OfflineAnalysisPipeline.h`). | mirror-equivalence bring-up | Separate gated production fix (changes the live spectrum → must re-run floors + corpus) |
| **P2-HAZARD-001** | Test-macro-gated DATA members (`#if JUCE_UNIT_TESTS` in `AIEngine.h`/`MLEngine.h`) make the object layout differ between the plugin SharedCode (no macro) and test TUs (macro=1). Any header-INLINE accessor to processor members declared after `aiEngine` read from an integration test returns garbage (measured: frames ≈ ns-since-start in the P2C2 bring-up). Mitigated case-by-case with out-of-line accessors; pre-existing inline accessors survive only because they touch members declared before `aiEngine`. **STILL OPEN after P2C2.1** — every NEW inline accessor on the processor/AIEngine is suspect until the hygiene fix lands. | P2C2 bring-up (wiring witness) | Dedicated hygiene ticket: make gated data unconditional (tiny size cost) or move test hooks out of object layout |

## 4. Promotion criteria — known_fail → PASS

A known_fail entry may be promoted ONLY when, in one measured commit:
1. the corpus row(s) it covers turn PASS with the **same fixture** (retuning the
   fixture to make it easier is forbidden);
2. every FLOOR row in section 1 is unchanged or better;
3. the full AI suite is green;
4. the manifest `known_fail` flag is flipped in the same commit, and the row in
   section 2 is updated with the new measured values.

A regression on a previously-PASS corpus row is a hard failure of the harness
(the clip is not marked known_fail → the test asserts).

## 5. Roadmap targets (proxy families vs market)

| Metric | Baseline | Target | Competitor proxy |
|---|---|---|---|
| Clean FP (corpus, sens ≤0.5, live) | 1 KNOWN_FAIL cell | 0 | Pro-Q 4 / Neutron assistants |
| Static resonance recall (corpus, live) | 1/4 cells PASS | ≥95% of cells | smart:EQ |
| Dynamic resonance recall | not yet measured (fixtures in P3) | ≥90% | soothe2 (its core) |
| Balance direction accuracy | not yet measured (P5) | ≥90% | Gullfoss |
| Verified-fix rate | not yet measured (P6) | ≥85% | nobody — differentiator |
| AI-thread CPU (front-end) | **P2C3.1 combined CPU witness (main + LF, amortized over main frames): 0.057 ms/main-frame** — main 0.031 ms ×233, LF 0.052 ms ×116, max single work unit 0.118 ms (~0.13% core @23 fps). NOTE: the previous "incl. LF" claim measured only the main path (understated ~45%); fixed. | ≤1.5% core, mean ≤5 ms | — |
| LF resolution (P2C3 headline witness) | 45 Hz + 60 Hz sines: 4096-band profile is an indistinguishable smear; **fused (8192) profile separates them with 13.94 dB peak-to-dip** | separable ≥3 dB | smart:EQ LF detail |
| Onset stream (P2C3) | spectral flux: transient frame peak **41.2 dB** vs steady median **0.000 dB**, localization ±1 frame | peak ≫ steady | soothe2 dynamics prerequisite |
| Front-end isolation (editor-open witness, P2C2.1) | GUI consumer drained **102,400/102,400** preEq samples concurrently AND the front-end still produced **49/~48** expected frames — readers isolated by the dedicated `aiFrontEndFifo` (the P2C2 SPSC violation is fixed) | both consumers always whole | — |

Commercial claims ("beats X at Y") are permitted ONLY after the corresponding
proxy family is green in this scorecard.

---

## 6. Known-good checkpoints (restore points)

| Tag | Certified state | Date | What is green |
|---|---|---|---|
| `checkpoint/p2c1` | P2 Commit 1 (`00ed7c44`) + this docs-only checkpoint commit | 2026-06-10 | Synthetic floor 0/18; multi-seed ML 0% / Hybrid 6.2% / frame 4.2%; resonance recall 100%; mirror equivalence 0.00000 dB; corpus baseline recorded; AI-Front witnesses (front-end CPU 0.038 ms/frame); full AI suite 1,391,262/0. PerceptualFrontEnd is diagnostics-only (NOT wired into production). |
| `checkpoint/p2-complete` | Pillar P2 complete (`e99728cf` P2C3.1) + this docs-only checkpoint commit | 2026-06-11 | Everything in `p2c1` PLUS: front-end wired into the AI thread on a dedicated SPSC fifo (editor-open isolation witnessed 102,400/102,400 + 49/48 frames), re-prepare handshake, LF 8192 fusion (45/60 Hz separated 13.94 dB), equal-loudness salience, flux/onset stream, honest combined CPU witness (0.057 ms/main-frame). Still diagnostics-only: NO detector consumes the front-end. Floors unchanged; full AI suite 1,391,262/0; Integration = only the 3 pre-existing BlockSize failures. |
| `checkpoint/pre-p4` | N1 honest gates + TIER 0 hygiene + TIER 1 (N5 calibration + GUI-1/4/5 dead-code) + this docs-only checkpoint commit (`eea4801d` GUI-5c) | 2026-06-16 | Honest gates live (no-arg = all categories except KnownDebt). **All four test binaries no-arg = 0 fail** (AI / Integration / Performance / DSP); AI-Sweep floor 0/18; KnownDebt = the 15 quarantined (non-blocking, by design). N5: 0 dBFS sine reads 0.001 dB (was −9). ~1,500 lines dead code removed (OpenGL path, AIControlPanel, CaptureWaveformView, BandViewport, bandToggles). Repo hygiene: AIEQ-mac/+zip & build-mac/ untracked, ml_weights_retrained.bin dropped, test hermeticity. Model + ml_weights.bin UNCHANGED. Worktree clean. This is the restore point BEFORE any P4 work. |

**Restore procedure** (on the working branch, e.g. after a regression):

```bash
# ⚠️ reset --hard is DESTRUCTIVE. ALWAYS protect the dirty worktree first:
git stash push -u -m "pre-restore $(date +%Y%m%d-%H%M)"

# reset to the MOST RECENT certified checkpoint in the table above, e.g.:
git reset --hard checkpoint/pre-p4   # most recent certified checkpoint

# Re-certify the restored state before resuming work:
cmake --build build-mac --target AIEqualizerPro_AI_Tests -j8
build-mac/Release/bin/AIEqualizerPro_AI_Tests --category=AI-Sweep   # floor 0/18
build-mac/Release/bin/AIEqualizerPro_AI_Tests                       # full suite green
```

Parked (unreviewed, do not lose): branch `parked/aiaccuracytest-db-fixtures`
holds the prior-session AIAccuracyTest dB-domain fixtures (also in `stash@{0}`;
the branch is the durable pointer).

---

## 7. Gap ledger vs market (honest assessment — UPDATE AT EVERY PILLAR CLOSE)

Rules: competitor info is as-of the assessor's knowledge cutoff; OUR numbers are
measured in this repo (cite the witness). Status values: CLOSED / AHEAD /
PARTIAL / OPEN. No status may improve without a measured witness behind it.

**Last updated: P2 complete (`checkpoint/p2-complete`, 2026-06-12). Overall
honest standing: ~6.5/10 vs 2026 leaders (was 6.0 at the integral review;
+0.5 from AI fixes + stability + foundations — foundations are not yet
user-audible).**

| # | Axis | Status @P2 | Our evidence | Market reference | Closes at |
|---|---|---|---|---|---|
| 1 | Detection precision (clean FP) | **CLOSED / arguably AHEAD** | floor 0/18, multi-seed ML 0% / Hybrid 6.2%, tilt-robust veto family, permanent regression harness (1 residual KNOWN_FAIL: heuristic HF on real dark tilt @0.2) | assistants err little because they suggest little; nobody publishes numbers | — (hold forever) |
| 2 | Recall on REAL audio | **OPEN — most urgent** | P1-GAP-001: true 3.2 kHz resonance missed (ML all sens, Hybrid 0.5) + sensitivity inversion, measured on corpus | smart:EQ 4 learns robustly from real audio | P3 stats (lateral) + P4 retraining |
| 3 | Dynamic/intermittent problems | **OPEN — biggest perceived gap** | recall 0% on pulsed fixtures (before-witness pending in P3C2) | soothe2's core capability | P3 |
| 4 | ML brain quality | **OPEN — fundamental** | 24K-param MLP, c4a72b0f retrain (2850 synthetic samples w/ tilt+negatives, BCE, F1 65.2%); raws over-fire on real audio; precision survives only via AIEngine vetoes. P4 diagnosis: bottleneck is a stack (eval realism, mel smearing of narrow resonances, dB-polarity, class freq-overlap, no regularization) — mostly fixable WITHOUT new data; real labeled audio is a later step | sonible: models trained on thousands of real mixes | P4-D1 ruler → P4-D2 → targeted fixes |
| 5 | Perceptual decisions | **PARTIAL — foundations ready, unused** | front-end delivers salience/LF-fusion (13.94 dB witness)/flux at 0.057 ms/frame, but NO detector consumes it (by discipline) | Gullfoss/smart:EQ decide on perceptual representations | P2 consumer migrations + P3/P5 |
| 6 | Tonal balance / target curves | **OPEN — nothing built** | n/a | Gullfoss core, smart:EQ profiles | P5 |
| 7 | Auto source-awareness | **OPEN** | genre classifier runs, result consumed by nothing | smart:EQ profiles + learning | P5 |
| 8 | Verify-loop (apply→re-measure) | **OPEN — our differentiator** | postEq path exists (GUI-owned; needs dedicated fifo, see P2C2.1 lesson) | nobody ships this | P6 |
| 9 | EQ engine math (outside AI roadmap) | **OPEN — behind Pro-Q** | C3: 24/48 dB/oct cuts stack identical-Q biquads (non-Butterworth: droop, shifted corner); LP decent not pristine; DynEQ coupled ballistics + 0.5 dB stepped gain (S3) | FabFilter Pro-Q 4 is the reference | dedicated engine tickets (C3 first) |
| 10 | Suggestion UX | **OPEN — undervalued** | fixes are take-it-or-leave-it (no tweak-before-apply, no per-suggestion audition). NOTE: the earlier "3 panels, 2 dead" was imprecise — panels are tab-switched / on-demand; the real dead UI code is in §8 | smart:EQ/Neutron audition + tweak workflows | dedicated UX ticket |

Summary: axis 1 is green and defended by the harness; ZERO of the five
user-visible capability gaps (2,3,6,7,8) is closed yet — P0–P2 built the
instrument and the eyes; the visible-capability race starts at P3.

---

## 8. GUI gap ledger vs market (from the GUI deep-dive, 2026-06-13)

Read in full: PluginEditor.cpp, AdvancedSpectrumDisplay.h (3436 lines),
NewSpectrumPipeline.h, ModernLookAndFeel.h. Sampled (not line-by-line):
AIProblemPanel (1131), SemanticControlPanel (683), DynamicEQPanel (670),
BandControlPanel (590). Reference set: FabFilter Pro-Q 4, soothe2, sonible
smart:EQ 4, Soundtheory Gullfoss, TDR Nova.

**What is genuinely competitive (verified in code):** interaction model is
Pro-Q-grade — drag node freq/gain, Shift=Q, Alt-click delete, double-click
create, mouse-wheel, right-click per-band menu, spectrum-grab (click detected
peak → band), draggable tilt widget, per-band solo. Pre/Post EQ overlay correct.
Custom premium aesthetic (Inter 4-weight embedded, amber palette, brushed-metal
noise, grid cache). AI Problem Panel with FIX ALL (Pro-Q has no suggestion list).

| ID | GUI gap | Status | Evidence (file:line) | Market reference | Fix |
|---|---|---|---|---|---|
| **GUI-1** | ~805 lines of DEAD OpenGL spectrum path | OPEN — hygiene/weight | `renderOpenGL()` is a no-op (PluginEditor.cpp:291); GL context attached only for compositing; `GLSpectrumComponent.h` (433) + `OpenGLSpectrumRenderer.h` (372) + `GLSpectrumHelper` lifecycle run for nothing | — | remove dead path |
| **GUI-2** | No UI scaling / retina / fullscreen | OPEN — biggest perceived gap | no `setScaleFactor`/global-scale anywhere; window fixed-DPI 1200×810, resize 1100×740→1800×1200 only | ALL 2026 leaders offer UI scaling + fullscreen | add scale control |
| **GUI-3** | Two coexisting spectrum systems + ~3 dB mismatch | OPEN | primary per-pixel `NewSpectrumPipeline` (75% overlap) injects into a legacy 512-bin software path in `AdvancedSpectrumDisplay`; calibration sign error (N5) → views disagree ~3 dB | single coherent analyzer | unify on per-pixel pipeline, fix sign |
| **GUI-4** | Dead "MULTI-TRACK" unmasking toggle | OPEN | AIControlPanel.h:60 exposes the toggle; `MultiTrackUnmasking` has empty stubs → button does nothing | Pro-Q4/soothe2 masking display | wire or remove |
| **GUI-5** | Hidden orphan components | OPEN — hygiene | `resized()` permanently `setVisible(false)` on bandViewport, bandToggles, captureWaveform (PluginEditor.cpp:1459+) | — | remove leftovers |
| **GUI-6** | No masking/collision OVERLAY on the spectrum | OPEN — high value | detection exists but is shown only as a list, never as a spectral overlay | soothe2 core visual language; Pro-Q4 collision | new overlay (synergy with P3) |
| **GUI-7** | No audition / tweak-before-apply of a suggestion | OPEN | Fix-All path applies directly; no per-suggestion listen or pre-apply edit | smart:EQ/soothe/Neutron | UX ticket (= axis 10) |
| **GUI-8** | No EQ/curve match | OPEN | absent | Pro-Q4, Ozone, smart:EQ | feature ticket |

Performance (already logged elsewhere): peak-hold inverted decay; per-sample
meter atomic store; 60 Hz spectrum timer + 30/10 Hz panel timers.

**GUI honest standing:** interaction & aesthetics are competitive; the plugin
reads "below leaders" mainly for **no scaling (GUI-2)**, **no masking overlay
(GUI-6)**, **no audition (GUI-7)**, plus dead-code drag (GUI-1/3/4/5). Suggested
GUI order: GUI-2 → GUI-1+GUI-3 (unify spectrum, kill GL) → GUI-4+GUI-5 (hygiene)
→ GUI-6 (masking overlay, synergic with P3) → GUI-7.
