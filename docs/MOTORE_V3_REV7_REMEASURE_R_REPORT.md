# REV7 independent re-measure — ACTIVE / domain-on-R (ENBW+floor)

**Status:** EVIDENCE — ≠ G1 PASS — ≠ consolidate GO — ≠ anti-leakage claim  
**Date:** 2026-07-25  
**Judge:** independent CC lineage (reproduced WS4 RED; does not author ACTIVE)  
**Perimeter (acceptance A):** ENBW aperture + floor/union only; **leakage OOS** for ACTIVE  
**Harness:** implemented from proposal normative text (§7-on-`R`, choice B) — **not** product spike `sr_parity.py` relay  
**Proposal:** `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md` (redteam closure)  
**Governance:** `N_MIN` not raised; no new anti-leakage cut fitted to these numbers

---

## 1. Geometry (a priori)

| quantity | value |
|----------|--------|
| `\|R\|` / 120 | **96** / 120 |
| unresolved (material) | bands `0–23` except `21`, plus `36` (~161.7 Hz crossover) |
| historical sparse offenders vs R | `b16`, `b18` **out of R**; `b21` **in R** |

---

## 2. Stationary adversarial cells (useful window)

| asset | sr vs 48k | REV6 max\|Δ\| (WS4) | this re-measure (on R) | verdict | peak |
|-------|-----------|---------------------|------------------------|---------|------|
| multitone | 44100 | 4.780 | **4.771** | **FAIL** | `shape` b35 |
| multitone | 96000 | 4.488 | **4.479** | **FAIL** | `shape` b35 |
| pseudo_noise | 44100 | 9.537 | **1.237** | **FAIL** | `psd` b28 |
| pseudo_noise | 96000 | 8.993 | **1.136** | **FAIL** | `psd` b28 |

Vacuous FAIL: **not** triggered. Threshold 0.25 dB unchanged.

**log_sweep:** not executed this round (two stationary assets already determine FAIL). Required before any formal consolidate claim.

---

## 3. What the cure did / did not (sparse fixtures)

- **Did (geometry / under-resolution):** pseudo_noise 9.54 → 1.24 dB (~7.7×). Sparse 0/1-bin bands leave the domain; shape/prominence contamination via full-120 §7 is stopped by choice (B) on `R`.
- **Partial (sparse valleys):** multitone ~unchanged (~4.78 → ~4.77) on shape b35 — RESOLVED band between tones, near-floor skirts. Noise residual on psd b28 similarly sits between partials.
- **Hypothesis (iii) — sparse excitation as sole cause:** tested with dense probe (§4). **Refuted as complete explanation** (4.77 → 1.19 under dense, still FAIL). Do not correct the narrative by adding (iii) as the remaining fix; record that (iii) was measured and is **insufficient**.

---

## 4. Dense-excitation probe (hypothesis (iii) — tested)

**Hypothesis (iii):** residual FAIL on sparse fixtures (multitone / noise) is mostly empty-band / distant-leakage; a dense excitation with one sinusoid at every frozen band centre should largely clear the gate under the same ENBW+floor / §7-on-`R` rules.

**Construction (a priori fixture geometry, not fitted):** 120 equal-amplitude tones at `band_centers_hz()`, total RMS −24 dBFS, analytic render at each host rate, same useful window / nearest `source_time` / ACTIVE / §7-on-`R` pipeline as §2.

| probe | sr vs 48k | max\|Δ\| dB | verdict | peak |
|-------|-----------|------------|---------|------|
| 120 components | 44100 | **1.188** | **FAIL** | `prom` b24 (~81 Hz) |
| 120 components | 96000 | **1.094** | **FAIL** | `prom` b24 (~81 Hz) |

**Result:** hypothesis (iii) is **refuted as a complete explanation**. Sparse excitation mattered (multitone peak ~4.77 → ~1.19 under dense) but is **not sufficient**.

**Diagnostic (geometry of frozen window/grid — not a prescribed constant):** peak at band 24 (~80.6 Hz); neighbour centres ~4.54 Hz apart ≈ **0.78 LF bins**; Hann main-lobe half-width = 2 bins → adjacent band components lie inside each other’s main lobe. Residual failure mode is **inter-band inseparability under the analysis window**, not only distant leakage into empty bands. ENBW occupancy (`N_MIN=2`) can mark such a band `RESOLVED` while neighbours still co-interfere.

Zone sketch (pure geometry; centres vs main-lobe widths): on LF, adjacent centres exceed 2-bin separation only above ~204 Hz; full 4-bin separation only above ~409 Hz (MAIN thresholds higher). No claim here that any particular `N_MIN` would PASS — that measurement is intentionally not run by the contaminated judge.

---

## 5. Structural conclusion (updated after dense probe)

1. Narrowing constraint 4 to ENBW+floor was honest packaging; under that perimeter the gate still **FAILS** on sparse fixtures **and** on the dense probe.
2. Residual after R-restriction is **not** only “distant leakage into empty bands.” Dense probe shows **band-to-band separation** under the frozen Hann main lobe is also in play on `RESOLVED` cells.
3. Next amend must still target **domain and/or field definitions** and/or a **re-opened a-priori geometric criterion** justified from window/grid (including whether ENBW aperture vs main-lobe separation is the right property) — written **before** any further PASS chase. Contaminated judge delivers **diagnosis only**, not a chosen constant.
4. **Forbidden:** raise `N_MIN` / add anti-leakage or separation cuts chosen to clear 4.771 / 1.237 / 1.188; shopping “which N_MIN passes” after seeing these numbers.

---

## 6. Honesty notes on this measure

- Prominence text ambiguity (renorm retained weights vs reflect on R-sequence): implemented **renormalization**. Dense-probe peak is on prominence — dual reading residual C remains material for lock time; does not reverse FAIL under renorm.
- Acceptance B: fields compared are §7-on-`R`, not product §7-120 + mask.
- `log_sweep` still owed before any formal consolidate claim.

---

## 7. Verdict

**RED** under the ENBW+floor / domain-on-`R` candidate perimeter.

Diagnosis: under-resolution sparse bands **addressed** by R; sparse-fixture leakage **partial**; residual **inter-band inseparability** on geometrically `RESOLVED` low bands (dense probe). Hypothesis (iii) tested → **insufficient**.

**REV7 consolidate: NO.** Hand off to untainted scope rewrite with diagnosis “band separation under main lobe” — **without** a prescribed constant from this judge.
