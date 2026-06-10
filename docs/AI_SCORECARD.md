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
| AI-thread CPU | negligible (witness lands in P2) | ≤1.5% core, max ≤5 ms/frame | — |

Commercial claims ("beats X at Y") are permitted ONLY after the corresponding
proxy family is green in this scorecard.
