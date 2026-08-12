# Motore v3 — A3 falsification measure (`ACTIVE_sweep` only)

| Field | Value |
|-------|--------|
| **Status** | **EVIDENCE — FALSIFICATION ONLY** |
| **Date** | 2026-07-26 |
| **Formula HEAD** | `04e47b39610e8fda24b36a57eb8283388565b62b` (`feature/motore-v3-offline`) |
| **Formula doc** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md` @ that HEAD |
| **Closing domain** | `{ACTIVE_sweep}` alone — **not** `ACTIVE∩R` / `i∉R` / `EXCLUDED_GEOMETRY` |
| **Threshold** | `0.25` dB hard; aggregator `max` (immutable; no mean/p95) |
| **Platform** | CPython 3.12.13 / numpy 2.5.1 / macOS-15.5-arm64 (`~/aieq_data/motore_v3/env/venv`) |
| **Runner** | spike `motore-v3-g1b-spike` / `ml_v3/reports/run_a3_falsification_measure.py` |
| **JSON** | spike `ml_v3/reports/G1B_A3_FALSIFICATION_MEASURE.json` |

**≠** G1 PASS · **≠** REV7 consolidate · **≠** ACCEPT product path · **≠** PASS-claim

---

## Command

```bash
cd /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1b-spike
PYTHONPATH=$PWD ~/aieq_data/motore_v3/env/venv/bin/python \
  ml_v3/reports/run_a3_falsification_measure.py
```

Formula applied as written at HEAD `04e47b39…`:

- neighbourhood `N(chk) :=` triangular support ∩ `F_TRAJ` (analytic chirp image + `T_MEM`)
- `ACTIVE_sweep ⇔ ABOVE_FLOOR_UNION ∧ (¬ONE_SIDED_FLOOR_UNION ∨ b∈N)` with float32 `AT_FLOOR`
- `CHK_CHIRP_REACHABLE ⇔ t_start ≤ source_time* ≤ t_end` (else checkpoint FAIL)
- lock tie-break: smaller `frame_index`, then smaller `frame_end_sample`
- **no** intersection with geometric `R`; **no** threshold shopping

---

## Gate-closing domain (`log_sweep` / `checkpoint_grid`)

| chk Hz | CHK_CHIRP_REACHABLE | \|N\| | b106∈N | max\|Δ\| dB (worst host) | peak | verdict |
|--------|---------------------|------|--------|--------------------------|------|---------|
| 20 | **false** (`st*≈0.490667 < 0.5`) | — | — | — | — | **FAIL** |
| 45 | true | 15 | no | 1.4048 | shape b23 @44.1k | **FAIL** |
| 60 | true | 18 | no | 1.4056 | shape b33 @44.1k | **FAIL** |
| 80 | true | 18 | no | 0.7405 | psd b35 @44.1k | **FAIL** |
| 250 | true | 18 | no | 1.7522 | shape b51 @44.1k | **FAIL** |
| 1000 | true | 11 | no | 2.5751 | shape b69 @44.1k | **FAIL** |
| 3500 | true | 10 | no | 4.6642 | shape b89 @44.1k | **FAIL** |
| 8000 | true | 10 | no | 5.3797 | shape b103 @44.1k | **FAIL** |
| **16000** | true | **10** | **yes** | **6.4350** | **psd b106 (~9404 Hz) @44.1k** | **FAIL** |
| 20000 | true | 10 | no | 4.1235 | shape b119 @44.1k | **FAIL** |

### Smoking-gun cell (16 kHz) — still ACTIVE

| vs 48k | max\|Δ\| dB | peak | `b106∈N` | `n_active` |
|--------|------------|------|----------|------------|
| 44100 | **6.4350** | psd b106 (~9403.7 Hz) | **true** | 10 |
| 96000 | **5.8381** | psd b106 (~9403.7 Hz) | **true** | 10 |

`N(16000) = {106…115}` under REF-48k `source_time*≈1.706667`,
`F_TRAJ≈[9744.2, 15740.9]` Hz. Honesty pin MED#2 satisfied:
`b106_in_N_at_16k = true` (no private-lattice escape).

### Empty-N / 20 Hz pin (HIGH#2)

Nearest REF-48k useful frame for chk **20 Hz**: `source_time*≈0.490667`
with `|dt|≤T_HOP`, but `st* < t_start=0.5` →
`¬CHK_CHIRP_REACHABLE` → checkpoint **FAIL** (no skip, no `t_cross` proxy,
no clamp into chirp).

---

## Verdict (gate-closing domain only)

**FAIL**

- Overall `max|Δ| = 6.4350` dB (threshold 0.25) at chk 16 kHz / vs44100 / psd b106
- Independently: chk 20 Hz unreachable under §2.3.2
- `b106` **stayed ACTIVE** (in `N` ∧ one-sided floor-union)

### Explicit non-claims

- **No PASS-claim** for G1 / gate-4 product path
- **No ACCEPT** for product / tip packaging
- **No REV7 consolidate**
- This note falsifies “A3 formula closes sweep HF under current frontend PSD”
  for the declared closing domain; it does **not** authorize threshold relax,
  `ACTIVE∩R` shopping, or shrinking `N`

---

## Next (for Marco)

1. Treat this as honest **ACCEPT=FAIL** evidence on the written A3 domain (optional commit of this note alone).
2. Do **not** consolidate REV7 / claim G1 PASS.
3. Lab choice remains a-priori: amend admission further, change frontend/PSD path under contract, or keep debt explicit — not mean/p95 / 0.25 relax / ∩R laundering.
