# A4b CONTROL Baseline (2026-07-17) - NO-GO 0/3

This is the first reproducible A4b control, not a candidate for the plugin.
It was trained from `feature/a4b-control-baseline@bbaff651` after the corpus
contract preflight, with no corpus addition and no ablation.

## Fixed recipe

- Dataset contract SHA-256:
  `d37de0836b3568695bf5c1c78de6011057fa5c9fa0422e17a9f01a09acde0b6b`
- Dataset seeds: train `42`, heldout `43`; model-init seeds: `42,1337,2026`.
- 150 epochs, batch 32, AdamW `lr=0.001`, `min_lr=0.00005`,
  `weight_decay=0.01`.
- Hybrid mask: `[1,3]` (Harshness and Sibilance are heuristic-routed, so their
  CNN benchmark entries are intentionally `N/A`).
- Full deep preflight passed: train 3,062, heldout 493, test 834 manifest rows.
- Dataset build: 7,810 train and 1,340 heldout windows; 389 train real-
  Resonance positives across 82 files.

## Internal checkpoint selection

| Seed | Best epoch | Selection | Metric macro-F1 | Metric clean-FP |
|---:|---:|---:|---:|---:|
| 42 | 30 | 0.060 | 0.219 | 0.130 |
| 1337 | 4 | 0.061 | 0.091 | 0.065 |
| 2026 | 2 | 0.054 | 0.084 | 0.065 |

The selected checkpoints are intentionally retained even where their measured
clean-FP exceeds the 0.05 target. The selection penalty did not prevent this;
that behavior is evidence for the next controlled diagnosis, not grounds to
rewrite this baseline.

## A6 external gate

All three seeds fail A6. Shared core failures are Ableton clips `02` and `04`:
the model never reaches the required 5% occupancy for Boominess/Muddiness on
`02` or BoxyMidrange on `04`. Clip `08` (Resonance) fails in seeds 1337 and
2026. Seed 2026 also fails clips `06` and `07`; seed 42 overfires on electronic
drums and bass.

`clean_synth` is a separate fixture blocker, not a model score: the current
neutrality screen finds only 9 of the required 10 files. Do not relax that
requirement. A future fixture repair must admit one new, genuinely neutral,
group-disjoint synth file with the same manifest and licence checks.

The exact train and A6 reports are preserved beside the candidates. `SHA256SUMS`
binds every exported JSON and provenance file. None of these files may be
copied to `Resources/Models`, an EXP bundle, or the ship line.
