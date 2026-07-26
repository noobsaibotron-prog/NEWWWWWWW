# Motore v3 — Stationary `R` closing measure (option C)

**Status:** EVIDENCE — MEASURE-PASS on gate-4 closing set under option C  
**Date:** 2026-07-26  
**Authority scope:** `docs/MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md` @ `31216df4`  
**≠** G1 PASS · ≠ ACCEPT · ≠ REV7 consolidate · ≠ G1b tip · ≠ CONTRACT/lock/SHA edit

**Platform:** CPython 3.12.13 (`/Users/marco/aieq_data/motore_v3/env/venv`)  
**Spike harness:** `motore-v3-g1b-spike` / `ml_v3/reports/run_rev7_remeasure_r.py`  
**PYTHONPATH:** spike root  
**Raw JSON (spike):** `ml_v3/reports/G1B_REV7_REMEASURE_R_REPORT.json`  
**Run log (spike):** `ml_v3/reports/G1B_REV7_REMEASURE_R_RUN_OPTION_C.log`

**Perimeter (immutable):** geometric `R` = ENBW `N_MIN=2` ∧ Rayleigh
`SEPARATION_MIN_BINS=2` → `|R|=67`, first_in_R ≈ 433.68 Hz (b53).  
**Aggregator:** `max` · **Threshold:** `0.25 dB` · **Rilassamenti:** NESSUNO.

---

## 1. Gate-closing under option C (`multitone`, `pseudo_noise` on `R`)

Closing set = stationary assets only, evaluated on geometric `R`
(§7-on-`R`, ACTIVE = `R` ∧ ABOVE_FLOOR union). vs reference 48 kHz.

| asset | vs 48k | max\|Δ\| dB | verdict | peak | n_pairs |
|-------|--------|------------|---------|------|---------|
| multitone | 44100 | **0.1915** | **PASS** | level | 77 |
| multitone | 96000 | **0.1898** | **PASS** | level | 77 |
| pseudo_noise | 44100 | **0.0458** | **PASS** | level | 77 |
| pseudo_noise | 96000 | **0.0464** | **PASS** | level | 77 |

**Stationary closing aggregate:** `max|Δ| = 0.1915 dB` ≤ 0.25 → **PASS**  
(worst cell: multitone@44100, peak field `level`).

Byte-reproduced vs prior formal R remeasure (`docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md`
@ `b3d7f71b` stationary rows). No threshold or aggregator change.

---

## 2. Report-only (does **not** close gate 4 in G1)

### 2a. Bands `∉ R` (published debt; omit → report FAIL)

| asset | vs 48k | max\|Δ\| dB | peak |
|-------|--------|------------|------|
| multitone | 44100 | 4.7796 | shape b35 (~152.5 Hz) |
| multitone | 96000 | 4.4875 | shape b35 (~152.5 Hz) |
| pseudo_noise | 44100 | 9.5374 | shape b18 (~56.9 Hz) |
| pseudo_noise | 96000 | 8.9935 | shape b18 (~56.9 Hz) |
| log_sweep | 44100 | 108.0214 | shape b2 (~22.5 Hz) |
| log_sweep | 96000 | 1.5628 | shape b51 (~386.1 Hz) |

### 2b. `log_sweep` on `R` (fixture hashed; scored for information only)

Under option C, `log_sweep` **does not close** gate 4. Numbers below are
published report-only — same harness fields as a gate score, **not** admitted
into the closing aggregate.

| asset | vs 48k | max\|Δ\| dB | note | peak | n_pairs |
|-------|--------|------------|------|------|---------|
| log_sweep | 44100 | 6.4350 | report-only (would FAIL if scored as close) | psd b106 (~9404 Hz) | 10 |
| log_sweep | 96000 | 5.8381 | report-only (would FAIL if scored as close) | psd b106 (~9404 Hz) | 10 |

Non-stationary SR-parity remains a named G1c/G1e requirement; sweep redesign
proposal stays **PARKED**.

---

## 3. Lab verdict (non-GO)

```text
VERDICT: MEASURE-PASS
DOMAIN: G1-parity (stationary R closing set under option C)
REF: MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md @ 31216df4
     harness run_rev7_remeasure_r.py (spike)
THRESHOLD_POLICY: max |Δ| ≤ 0.25 dB; rilassamenti NESSUNO
```

| case | expected role | observed max\|Δ\| | pass? |
|------|---------------|-------------------|-------|
| multitone@44100 on R | closes | 0.1915 | PASS |
| multitone@96000 on R | closes | 0.1898 | PASS |
| pseudo_noise@44100 on R | closes | 0.0458 | PASS |
| pseudo_noise@96000 on R | closes | 0.0464 | PASS |
| ∉R all assets | report-only | see §2a | n/a (publish) |
| log_sweep on R | report-only | 6.4350 / 5.8381 | n/a (does not close) |

**Interpretation (non-GO):**
- Questi numeri dicono: sotto option C, il closing set stazionario su `R`
  rispetta 0.25 dB max.
- **Non** autorizzano: G1 PASS, ACCEPT, REV7 consolidate, tip G1b ufficiale,
  training, promozione modello, ship Ableton, reopen A3, amend CONTRACT/lock.

**Handoff:**
- guardian: usare questa evidenza per interpretare GO/NO-GO sul packaging
  successivo (docs-verify già in catena; ≠ auto-GO G1).
- phase-builder: n/a — docs-only measure; no code change.

---

## 4. Reproduction

```bash
cd /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1b-spike
export PYTHONPATH="$PWD"
/Users/marco/aieq_data/motore_v3/env/venv/bin/python \
  ml_v3/reports/run_rev7_remeasure_r.py
```

Exit 0. Stationary gate rows must match §1 to four decimals as printed by the
harness (`0.1915` / `0.1898` / `0.0458` / `0.0464`).
