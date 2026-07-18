# A4b CONTROL grid 3x2

Verdict: **NO-GO (0/6 A6 PASS)**

This run freezes the paired CONTROL for future A4b ablations. It does not
authorize a model promotion, a runtime change, or a corpus admission.

## Frozen scope

- Lab commit: `85fa55eced7382518f9804c480aadfd8a60c1608`
- Model seeds: `42,1337,2026`
- Dataset seeds: `42,1337`
- Recipe: 150 epochs, batch 32, lr 0.001, min-lr 0.00005, weight decay 0.01
- Hybrid mask: classes `[1,3]` (Harshness and Sibilance are not A6 CNN gates)
- Split contract SHA-256: `4b7981901341b7305d878c9949a4e6b31dae875fc7b0f13e2148d517c002b393`
- Manifest SHA-256: `f1fcc22bbaf32c9563a32c0ca8de6d2e1ab9109dde8f28750cfa1ae1cd9f247d`
- Deep-verified rows: train 3062, heldout 493, test 835
- Frozen output: `/Users/marco/aieq_data/baselines/a4b_control_grid_20260718_85fa55ec/`
- `SHA256SUMS` SHA-256: `6916dda9bfdaa6608293affe51ca3bf2d463125d421d4011a4d79043597a38c4`

The EXP/product worktree remained at `68ca31b43f5e523f63d70ce58a6cbf255760f82e`.

## Reproduction check

All three dataset-seed-42 artifacts are bit-identical to the committed A4b
CONTROL models. This proves that the grid refactor preserved the historical
CONTROL semantics.

| Model seed | New artifact SHA-256 | Historical CONTROL |
|---:|---|---|
| 42 | `434f803b6121ef83dc1664fe82c389710c195ed798470515cfe22292b2daeca3` | bit-identical |
| 1337 | `717f322036c8a0d3b6d42e5106fd4d8a49a9f0dca1c496e27f38201eb1456e9f` | bit-identical |
| 2026 | `bbf08e8c53ccec9582e7410d486344305895f01560d5d1b2119155ca705c919f` | bit-identical |

Dataset seed 42 built 7810 train and 1340 heldout windows. Dataset seed 1337
built 7793 train and 1333 heldout windows. Their cache keys differ, while both
retain 389 train and 77 heldout real-Resonance positives.

## Heldout selection

| Model/data seed | Selection | Macro-F1 | Clean-FP | Best epoch |
|---|---:|---:|---:|---:|
| 42 / 42 | 0.060 | 0.219 | 0.130 | 30 |
| 1337 / 42 | 0.061 | 0.091 | 0.065 | 4 |
| 2026 / 42 | 0.054 | 0.084 | 0.065 | 2 |
| 42 / 1337 | 0.030 | 0.080 | 0.075 | 3 |
| 1337 / 1337 | 0.065 | 0.065 | 0.040 | 9 |
| 2026 / 1337 | 0.034 | 0.078 | 0.072 | 2 |

Across all six runs, median macro-F1 is 0.082 (range 0.065-0.219), median
clean-FP is 0.069 (range 0.040-0.130), and median selection is 0.057 (range
0.030-0.065). The best selection score does not identify the best external
behavior: `1337/1337` has the highest selection but the worst Ableton result.

## A6 external gate

| Model/data seed | A6 failures |
|---|---|
| 42 / 42 | `02`, `04`, `clean_drums`, `clean_synth`, `clean_bass` |
| 1337 / 42 | `02`, `04`, `08`, `clean_synth` |
| 2026 / 42 | `02`, `04`, `06`, `07`, `08`, `clean_synth` |
| 42 / 1337 | `02`, `04`, `06`, `07`, `08` |
| 1337 / 1337 | `01`, `02`, `03`, `04`, `06`, `07`, `08` |
| 2026 / 1337 | `02`, `04`, `06`, `07`, `08`, `clean_synth` |

Electronic-neutral clean-file counts (domain PASS requires at least 7/10):

| Model/data seed | Drums | Synth | Bass | Mix | HF negative |
|---|---:|---:|---:|---:|---:|
| 42 / 42 | 3 | 6 | 3 | 8 | 8 |
| 1337 / 42 | 10 | 6 | 7 | 9 | 10 |
| 2026 / 42 | 10 | 6 | 7 | 9 | 10 |
| 42 / 1337 | 10 | 8 | 7 | 9 | 10 |
| 1337 / 1337 | 10 | 9 | 8 | 8 | 10 |
| 2026 / 1337 | 10 | 6 | 7 | 9 | 10 |

## Counter-check conclusions

1. The 3x2 harness is valid: dataset seed 42 reproduces all historical models
   bit-for-bit, and dataset seed 1337 produces distinct cache keys and artifacts.
2. No CONTROL candidate is usable. Clips `02` (Boominess/Muddiness) and `04`
   (BoxyMidrange) fail in 6/6 runs.
3. The apparent clean/recall trade-off is unstable. The high-recall `42/42`
   model overfires on three electronic domains; cleaner seeds often become
   silent on `06`, `07`, and `08`.
4. Heldout selection alone is not an adequate promotion criterion. A6 remains
   the external judge, and every seed must be reported.
5. F2a remains NO-GO: the waveform renderer is not admitted. New training
   corpus remains blocked by the existing license, identity, and coverage gate.

## Next controlled step

Before training another grid, run a dataset-only Boom measurement probe. A valid
Boom positive needs both measurable pre-existing 40-150 Hz content and a
measured post-injection excess over the local low-mid reference. This resolves
the contradiction between "inject only where low end exists" and "do not label
already-boomy raw material as clean". Thresholds must be selected from reported
raw/post/delta distributions and retained-positive counts, not chosen ad hoc.

The subsequent Boom-only ablation may qualify as an independent lever only if
it improves clip `02` in at least 4/6 paired runs, improves median heldout
Boominess, and introduces no new paired A6 or worst-clean-FP regression. It is
not an overall GO while clip `04` remains red. The final combined candidate must
still satisfy the unchanged full gate for both `02` and `04` in at least 4/6.

## Commands

```bash
/Users/marco/aieq_data/env/a4b-venv/bin/python -m ml_v2.train \
  --seeds 42,1337,2026 --data-seeds 42,1337 \
  --epochs 150 --batch-size 32 --lr 0.001 --min-lr 0.00005 \
  --weight-decay 0.01 --mask-classes 1,3 --device mps \
  --out /tmp/aieq_v2/a4b_control_grid_20260718_85fa55ec \
  --cache /tmp/aieq_v2/cache_a4b_control_grid_20260718_85fa55ec

/Users/marco/aieq_data/env/a4b-venv/bin/python -m ml_v2.eval_v2_benchmark \
  --model <candidate.json> --metadata <candidate.provenance.json> \
  --no-fail-exit
```
