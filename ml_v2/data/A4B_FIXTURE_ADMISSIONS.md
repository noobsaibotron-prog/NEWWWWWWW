# A4b Fixture Admissions

## fsld-synth-fixture-20260717

Purpose: repair the A6 `clean_synth` neutral-fixture shortage without changing
the training corpus. This is a **test-only** `g3-external` group, never a
train or heldout group.

- Group: `fsld:46593`
- Source: Freesound 46593, `chipfork - Springloop01`, CC-BY-3.0
- Canonical data path: `~/aieq_data/real_audio/tier2_eval/clean_synth/fsld_46593.wav`
- Canonical SHA-256:
  `59630c4454b28ba70782db121f84cfe1bc283fe23c661b3ad90cee30967acdd9`
- Raw source SHA-256:
  `7bb910d22ddb3c6bf2080ed5804e29095b04cf629bb66954308d22fbc67eca4d`
- Admission ledger SHA-256:
  `1681b00b15a4d6987d8792ce62fa92e1024360604c3270dd014ce9bfda3f82e2`
- Ledger and canonicalisation report:
  `~/aieq_data/a4b_admissions/fsld-synth-fixture-20260717/`

Selection evidence: 12 per-file-licensed, pack-disjoint FSLD synth probes were
canonicalised; 7 passed A6 neutrality screening. `fsld:46593` also stayed below
10% occupancy for every active CNN class in all three A4b CONTROL seeds.

Pre-commit admission preflight rehashed all audio and passed with counts train
3062, heldout 493, test 835. The split-contract batch assignment is
`g3-external`, derived by `sha256-rank-v1` for this one-group batch.
