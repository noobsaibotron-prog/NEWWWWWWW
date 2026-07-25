# REV7 final re-measure — gate on `R` + report-only `∉R`

**Status:** EVIDENCE — ≠ G1 PASS — ≠ consolidate GO  
**Date:** 2026-07-25  
**Platform:** CPython 3.12.13 / numpy 2.5.1 (`~/aieq_data/motore_v3/env/venv`)  
**Spike runner:** `motore-v3-g1b-spike` / `ml_v3/reports/run_rev7_remeasure_r.py`  
**JSON:** spike `ml_v3/reports/G1B_REV7_REMEASURE_R_REPORT.json`  
**Perimeter:** ENBW `N_MIN=2` ∧ Rayleigh `SEPARATION_MIN_BINS=2` → `|R|=67` (first ≈433.7 Hz);  
report-only packaging per `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`

---

## Gate-closing (`i ∈ R`, §7-on-`R`, ABOVE_FLOOR union)

| asset | vs 48k | max\|Δ\| dB | verdict | peak |
|-------|--------|------------|---------|------|
| multitone | 44100 | **0.1915** | **PASS** | level |
| multitone | 96000 | **0.1898** | **PASS** | level |
| pseudo_noise | 44100 | **0.0458** | **PASS** | level |
| pseudo_noise | 96000 | **0.0464** | **PASS** | level |
| log_sweep | 44100 | **6.4350** | **FAIL** | psd b106 (~9404 Hz) |
| log_sweep | 96000 | **5.8381** | **FAIL** | psd b106 (~9404 Hz) |

**Overall gate:** **FAIL** — `max|Δ| = 6.435 dB` (threshold 0.25).

Stationary adversarial subset **PASSes** under the new perimeter with margin.
`log_sweep` checkpoint mode still fails.

### log_sweep peak (diagnostic)

At checkpoint **16 kHz** (`t_cross≈1.7096`): `psd_ref[106]=−120.0`,
`psd_sr[106]≈−113.56` → admitted by union `max>−120` though reference is on
the clamp floor; instantaneous sweep peak in that frame is near b111
(~12.6 kHz), not b106. Same one-sided floor/union skirt pattern as WS4
smoke on HF checkpoints — **not** an LF geometry issue (b106 ∈ `R`).

---

## Report-only (`i ∉ R` — does **not** close gate)

| asset | vs 48k | max\|Δ\| dB | peak |
|-------|--------|------------|------|
| multitone | 44100 | 4.780 | shape b35 (~152.5 Hz) |
| multitone | 96000 | 4.488 | shape b35 |
| pseudo_noise | 44100 | 9.537 | shape b18 (~56.9 Hz) |
| pseudo_noise | 96000 | 8.993 | shape b18 |
| log_sweep | 44100 | 108.021 | shape b2 (~22.5 Hz) |
| log_sweep | 96000 | 1.563 | shape b51 (~386 Hz) |

These numbers are **published debt**, not gate-closing. Omitting them would
be a report FAIL under the report-only clause.

---

## Verdict

1. Geometric `R` + report-only LF packaging **works as designed** for
   multitone / pseudo_noise (gate PASS).
2. Gate still **FAIL** on `log_sweep` HF checkpoint skirts via floor-union
   activity — separate from the LF inseparability problem already moved to
   report-only.
3. **REV7 consolidate: NO** until sweep admission / checkpoint rule is
   addressed a priori (not by shopping on 6.435). Options belong to a
   follow-up amend (e.g. require both sides above floor + margin, or
   checkpoint band neighbourhood tied to instantaneous peak) — **not**
   raising `N_MIN` / widening 0.25.

≠ G1 PASS. ≠ Ableton readiness.
