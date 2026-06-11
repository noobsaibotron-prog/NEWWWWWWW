# AI Detection Scorecard

Tracked measurement ledger for the AI detection quality roadmap (Roadmap v1).
Every number here is EMITTED BY A TEST — nothing is hand-estimated. Update this
file in the same commit as any change that moves a number.

- Synthetic matrix / robustness: `AIEqualizerPro_AI_Tests --category=AI-Sweep`
- Corpus baseline: `AIEqualizerPro_AI_Tests --category=AI-Corpus --verbose`
- Diagnostics / witnesses: `--category=AI-Diag --verbose`

**Baseline recorded at:** commit `dd805620` (P1 Commit 1), 2026-06-10, macOS,
Release build, 48 kHz fixtures.

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
| **P1-GAP-001** | True 3.2 kHz resonance MISSED in the live path: ML at every sensitivity, Hybrid at 0.5. ML raw probabilities over-fire (~1.0 on 4 classes — model trained on 64 synthetic Gaussians) and the decision rule + AIEngine reality-check vetoes flatten everything on real audio. **Bonus finding:** Hybrid PASSES at sens 0.2 but fails at 0.5 — a real-audio sensitivity inversion (heuristic candidate spray at higher sens saturates/competes in the persistence layer; same family as the Ticket #3 inversion). | AI-Corpus baseline; AI-Diag raws witness | P2 (perceptual front-end) + P4 (ML retraining on corpus) |
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

**Restore procedure** (on the working branch, e.g. after a regression):

```bash
# ⚠️ reset --hard is DESTRUCTIVE. ALWAYS protect the dirty worktree first:
git stash push -u -m "pre-restore $(date +%Y%m%d-%H%M)"

git reset --hard checkpoint/p2c1

# Re-certify the restored state before resuming work:
cmake --build build-mac --target AIEqualizerPro_AI_Tests -j8
build-mac/Release/bin/AIEqualizerPro_AI_Tests --category=AI-Sweep   # floor 0/18
build-mac/Release/bin/AIEqualizerPro_AI_Tests                       # full suite green
```

Parked (unreviewed, do not lose): branch `parked/aiaccuracytest-db-fixtures`
holds the prior-session AIAccuracyTest dB-domain fixtures (also in `stash@{0}`;
the branch is the durable pointer).
