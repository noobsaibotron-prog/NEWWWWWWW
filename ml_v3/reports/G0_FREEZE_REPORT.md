# Motore v3 G0 freeze report

Date: 2026-07-19

Verdict: **PASS**. This reproduces the frozen negative baseline; it does not
promote a model or authorize G1 implementation without counter-check.

## Isolation

- Branch: `feature/motore-v3-offline`, created from
  `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Runtime tag: `checkpoint/motore-v2-runtime-5c9cb329-2026-07-19`, annotated,
  peeled commit `5c9cb3290f87b62a339c6b2c49645b2b25524712`.
- Scientific tag: `checkpoint/motore-v2-a4b-final-88e70dd0-2026-07-19`,
  annotated, peeled commit `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- The dirty main worktree, EXP/product branches, installed plugins and all
  existing `Source/` files were left unchanged.

## Environment

- CPython 3.12.13 on macOS 15.5 arm64.
- uv 0.11.2; Torch 2.13.0; NumPy 2.5.1.
- Fresh venv: `~/aieq_data/motore_v3/env/venv`.
- `uv pip sync --require-hashes`: PASS, 11 packages checked.
- `requirements.in` SHA-256:
  `5110ddf0fdcba291e41aaaa04da6768a9b2b3ee559ce37fa7c4cc072cd256f60`.
- `requirements.lock` SHA-256:
  `7b12505922a96ee8b6f227a3b86d04f4600b8734503ca2308e854c046288adf9`.

## Data and contract preflight

- Deep audio rehash: train 3062 PASS, heldout 493 PASS, test 835 PASS.
- A4b committed model/provenance `SHA256SUMS`: 6/6 PASS.
- A4b guard module: 37/37 PASS.
- The frozen training report records 834 test rows. Commit `c80bd386` later
  admitted `fsld:46593` as a licensed, group-disjoint, test-only clean-synth
  fixture. It does not change training or model weights and explains the
  current 835-row test manifest.

## Behavioral replay

Current reference:
`~/aieq_data/baselines/a4b_control_grid_20260718_85fa55ec`.

- Reference `SHA256SUMS` SHA-256:
  `6916dda9bfdaa6608293affe51ca3bf2d463125d421d4011a4d79043597a38c4`.
- Every grid artifact listed by that file passed rehash.
- Dataset-seed-42 model JSONs for seeds 42, 1337 and 2026 are byte-identical
  to the three committed historical model JSONs.
- Each benchmark was rerun against the 835-row checkpoint. After normalizing
  only the `model=` path, all three outputs matched `eval_s*_d42.txt` exactly.

| Model seed | Replay | A6 failures |
|---:|---|---|
| 42 | exact | 02, 04, clean_drums, clean_synth, clean_bass |
| 1337 | exact | 02, 04, 08, clean_synth |
| 2026 | exact | 02, 04, 06, 07, 08, clean_synth |

Normalized replay SHA-256 values:

- seed 42: `a4c2a6debeffa9765431446334f4ffd9f967b5789e8ff72789fa233c8f384842`;
- seed 1337: `78bd01b7470920896f66296f77d2958403b9a19ff00257353f1f921bec95f9f3`;
- seed 2026: `a1a0b2eb397c359e336dedba71fcb7accc1bb18895dd1c52f731d1a74a6b7121`.

## Conclusion

G0 is reproducible and isolated. Motore v2 remains **NO-GO 0/3** for these
historical dataset-seed-42 candidates. The next permitted activity is the G1
contract and benchmark design; no training or runtime integration is enabled.
