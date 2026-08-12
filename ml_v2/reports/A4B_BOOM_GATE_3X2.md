# A4b measured Boom gate 3x2

Verdict: **NO-GO (0/6 A6 PASS; clip 02 PASS 0/6)**

The measured gate remains an opt-in lab ablation. No artifact is promoted to a
runtime model, plugin bundle, or product branch.

## Frozen scope

- Implementation commit: `d913742133b2`
- Paired CONTROL report: `ml_v2/reports/A4B_CONTROL_GRID_3X2.md`
- Only intervention: `--boom-measured-gate`
- Model seeds: `42,1337,2026`
- Dataset seeds: `42,1337` (heldout RNG seeds `43,1338`)
- Recipe: 150 epochs, batch 32, lr 0.001, min-lr 0.00005, weight decay 0.01
- Hybrid mask: `[1,3]`
- Frozen output:
  `/Users/marco/aieq_data/baselines/a4b_boom_gate_grid_20260719_d9137421/`
- `SHA256SUMS` SHA-256:
  `585d6622283dacccba329fe8e31fae15664672ee6cb273fe32bb4df5b4f68644`

Default-OFF parity was proven before the run: d42 train and heldout `X`, `Y`,
and source arrays are exactly equal to the historical CONTROL caches.

## Dataset effect

| Dataset seed | Split | CONTROL Boom positives | Gated Boom positives | Floor |
|---:|---|---:|---:|---:|
| 42 | train | 344 | 114 | 100 |
| 42 | heldout | 77 | 27 | 20 |
| 1337 | train | 362 | 120 | 100 |
| 1337 | heldout | 81 | 30 | 20 |

The gate did what it was designed to do at label time and passed all viability
floors. It also reduced the total training windows from 7810 to 7466 (d42) and
from 7793 to 7435 (d1337).

## Paired heldout results

| Model/data seed | Selection CONTROL -> gate | Boom F1 CONTROL -> gate | Clean-FP CONTROL -> gate |
|---|---:|---:|---:|
| 42 / 42 | 0.060 -> 0.020 | 0.308 -> 0.000 | 0.130 -> 0.067 |
| 1337 / 42 | 0.061 -> 0.025 | 0.135 -> 0.000 | 0.065 -> 0.067 |
| 2026 / 42 | 0.054 -> 0.047 | 0.082 -> 0.143 | 0.065 -> 0.070 |
| 42 / 1337 | 0.030 -> 0.005 | 0.108 -> 0.000 | 0.075 -> 0.060 |
| 1337 / 1337 | 0.065 -> 0.046 | 0.115 -> 0.087 | 0.040 -> 0.074 |
| 2026 / 1337 | 0.034 -> -0.037 | 0.108 -> 0.000 | 0.072 -> 0.068 |

- Selection median: 0.057 -> 0.022
- Boom F1 median: 0.112 -> 0.000
- Worst clean-FP: 0.130 -> 0.074

The cleaner worst case is underfiring, not a usable precision improvement. Four
of six exported checkpoints have zero Boom F1; three checkpoints are epoch 0.

## Paired A6 results

| Model/data seed | Gate failures | New failures vs CONTROL | Recovered failures |
|---|---|---|---|
| 42 / 42 | `02,03,04,06,07,08` | `03,06,07,08` | three neutral domains |
| 1337 / 42 | `02,03,04,06,07,08` | `03,06,07` | `clean_synth` |
| 2026 / 42 | `02,04,06,07,08` | none | `clean_synth` |
| 42 / 1337 | `02,03,04,06,07,08` | `03` | none |
| 1337 / 1337 | `02,04,06,07,08` | none | `01,03` |
| 2026 / 1337 | `02,03,04,06,07,08` | `03` | `clean_synth` |

All five electronic-neutral domains pass for all six gated candidates. That is
a real specificity improvement, but the target endpoint `02` remains red in
6/6 and recall regressions violate the no-new-failure contract.

## Counter-check conclusion

The measured gate is label-safe but not quantity-neutral. It removes roughly
two thirds of Boom positives; the dynamic positive weight rises only to its cap
of 6.0, so effective Boom supervision falls sharply. The result does **not**
show that the accepted examples are bad. It shows that gate-only confounds
label quality with positive mass and is not a qualifying independent lever.

The planned "add Boom-vs-Mud contrastive pairs" step is stale: the CONTROL
already creates same-frame Muddiness and Boominess axis positives for
`clean_drums` and `clean_bass`. Re-adding them would duplicate an existing
recipe, not test a new cause.

The next falsifiable diagnostic, if continued, is a mass-matched factorial
cell: keep the measured gate and repeat accepted **train-only** Boom positives
three times, leaving heldout unique. This restores 342/360 train positives,
close to the 344/362 CONTROL, without changing labels, loss, shape, thresholds,
other classes, or heldout. It must be a new opt-in flag and is not a promotion;
if it fails the same paired gate, stop the measured-gate family rather than tune
repeat counts or thresholds.
