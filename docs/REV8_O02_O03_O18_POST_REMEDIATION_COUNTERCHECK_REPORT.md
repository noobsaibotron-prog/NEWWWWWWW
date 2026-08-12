# REV8 — O-02 / O-03 / O-18 Post-Remediation Counter-Check

## 1. Verdict

```text
TARGET: 882819625f3ff55b3699b5220017cbb7d1eff017
COUNTER-CHECK: 3 / 3 CLEAN
MATERIALIZATION GATE: CLEAN
ARTIFACT ACTIVATION: NO
REV8 SPEC GO: NO
PUSH: NO
```

No majority rule was used. Each lens independently returned `CLEAN` on the
same immutable remediation commit.

## 2. Why remediation was required

The first implementation commit:

```text
33a0957b2353031118c44fe90eb7f8d85826c84c
```

passed 440 tests, but the first independent review correctly returned
`BLOCK`. `mean64` accepted equal normative keys associated with different
values. Stable sorting then preserved input order inside the tied key and the
non-associative binary64 reduction produced different published results for
different permutations.

The root counter-check also found:

- macro-group ordering used canonical JSON string bytes rather than the
  signed raw UTF-8 `group_id` order;
- O-18 cross-language goldens did not yet cover every signed rounding class;
- O-02 regenerated the mathematical center formula instead of consuming the
  already hash-locked canonical G1 center bits;
- the non-normative O-02 generator report did not yet contain a complete
  per-entry input/output/refinement record.

These findings demonstrate why a green internal suite was not accepted as a
counter-check result.

## 3. Remediation commit

```text
882819625f3ff55b3699b5220017cbb7d1eff017
fix(rev8): close O02 O18 countercheck findings
```

The commit changes only:

```text
ml_v3/contracts/frequency_geometry_v2.py
ml_v3/contracts/numeric_artifact_v2.py
ml_v3/contracts/numeric_authority_v2.py
ml_v3/fixtures/rev8/o02_boundary_artifact_v1.json
ml_v3/fixtures/rev8/o03_width_artifact_v1.json
ml_v3/fixtures/rev8/o18_numeric_artifact_v1.json
ml_v3/fixtures/rev8/o02_o03_o18_generator_report_v1.json
ml_v3/tests/test_g1c_rev8_o02_o03_o18.py
```

No live contract, candidate contract, plan, dispatcher, schema, identity
module, evaluator, O-01 implementation, or O-09 implementation was changed.

## 4. O-18 closure

`mean64` now has two explicit fail-closed key domains:

```text
canonical -> normalized canonical bytes for structured normative keys
utf8      -> raw UTF-8 bytes for macro group_id ordering
```

It now:

- rejects unknown key domains;
- rejects non-string keys in the raw UTF-8 domain;
- rejects an equal normative key associated with different values;
- permits repeated equal keys only when their values are also equal;
- remains independent of input order after normative ordering.

Independent adversarial fixtures covered:

- every permutation of duplicate keys with unequal values;
- duplicate keys with equal values;
- newline, quote, backslash, accented Unicode, and emoji group IDs;
- canonical ordering versus raw UTF-8 ordering;
- half-ULP ties in both even directions;
- signed zero;
- minimum subnormal and half-minimum-subnormal;
- maximum finite and overflow;
- empty and singleton reductions;
- single-round versus double-round compound publication;
- mutated-and-resealed O-18 artifact rejection.

The previous `O18-001` blocker is closed.

## 5. O-02 closure

O-02 now consumes the frozen canonical grid from:

```text
ml_v3.contracts.grid.band_centers_hz
```

The canonical grid hash is identical in:

- the existing G1 metrology lock;
- the current `band_centers_hz()` result;
- the O-02 generator provenance.

```text
7c01b069dad31eea58347c0365c64140dea48f108e71543ccd3cf706c9811da2
```

All 120 center bit patterns match position-by-position. O-02 therefore does
not replace or reinterpret the G1 center authority. Correct rounding begins
at the 119 geometric-mean boundaries.

Independent proofs established:

- 120/120 center-bit identity;
- 119/119 correctly-rounded boundaries from the frozen center rationals;
- 119/119 boundary ties map to the lower band;
- the next positive binary64 maps to the upper band;
- the generator report contains 120 center records and 119 boundary records
  with index, inputs, output, precision, rounding, and refinement method;
- resealed center, boundary, provenance, and binding mutations are rejected.

The previous 83/120 drift is eliminated rather than accepted as a new hidden
grid authority.

## 6. O-03 closure

The O-03 authority remains:

```text
W_MAX bits = 0x4033ee7b471b3a95
```

Independent checks confirmed:

- interval certificate contains the exact independently reconstructed value;
- `W_MAX` is accepted;
- the immediately following binary64 is rejected;
- zero width remains a non-degenerate single-band interval;
- finite projection saturates at bands 0 and 119;
- non-finite projection inputs fail;
- the O-03 artifact is bound to the exact current O-02 file SHA.

## 7. Test evidence

### Targeted immutable-commit suite

```text
Ran 81 tests
OK
```

### Full immutable-commit suite

```text
Ran 444 tests in 55.225s
OK
```

One independent lens also reran the full suite:

```text
Ran 444 tests in 53.552s
OK
```

Deterministic regeneration of all four JSON files is byte-identical.
`git diff --check` is clean and the repository worktree is clean.

## 8. Three independent lenses

### Lens A — numeric authority

```text
VERDICT: CLEAN
```

Reproduced O18-001 before remediation, then falsified its closure across every
permutation and the signed numeric edge cases.

### Lens B — frequency geometry

```text
VERDICT: CLEAN
```

Independently reconstructed the center binding, every boundary, `W_MAX`,
projection behavior, certificates, and resealed-mutation rejection.

### Lens C — scope, governance, and isolation

```text
VERDICT: CLEAN
```

Verified no hidden activation, no new dispatcher exports, no live-schema
change, no O-01/O-09 implementation, no protected document change, and no
replacement of the G1 grid authority.

## 9. Current artifact hashes

### File SHA-256

```text
O-02:
8726e8c27db1f18cc13a21c590d0757d49c154bad6a8acb4256c4fd718f8a647

O-03:
c6cda37bc9f375fcb35a9098547618b67a31b9e3c8ce5fafc3e1256269916a03

O-18:
dd01947935ebf5392fb2e8fee9ce0c01a9d66a83f61ef36f7348eb1bb6227473

Generator report:
584f5d0c85eaf1ee67cc459a6a147d4958fa463bc098ca938b4c6eb2cb809d61
```

### Embedded hashes

```text
O-02 artifact_hash:
d619fdf00ad197c6692ceb26db5b3852ddf12ffdad45dd5e4fd0b747671234da

O-02 center_hash:
4000045ff0b52649d42e34013e32dab20390a53c1ace8614cfd9f3dddabb9755

O-02 boundary_hash:
3dd49d30ced594bcfa1eabba5a96f54314d5230226f1c16123de530b0d7d498a

O-03 artifact_hash:
b36ec7d98ac07578a83bc39f0d206b83636794deec2837fe8e0513248b2e7d02

O-18 artifact_hash:
c692833996f56f7fd457c3a3f91c33d38c31ba65638e943e2321f941a4b08f4b

Generator-report artifact_hash:
77a1c52e8102f0a013d3069ac3db1b44abdbfbf7b3557fba7078c18ab3d34293
```

## 10. Protected hashes

The following remain unchanged:

```text
REV8 candidate:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

Live REV7 contract:
310d538647d71840c6dd8124f1e24281b758bb6dd3e4776aac4b34e5f658a5b0

Plan:
8c45a01dbb207e8163f9b3d0613f4547c3fbd5f22680de0e74ba4785f62def77
```

## 11. Final gate state

```text
O-02 MATERIALIZED: YES
O-03 MATERIALIZED: YES
O-18 MATERIALIZED: YES
POST-REMEDIATION COUNTER-CHECK: 3/3 CLEAN
TECHNICAL ARTIFACT FREEZE: PERMITTED
ACTIVATION: NO
REV8 SPEC GO: NO
```

The next independent tranche is O-09 resource-cap benchmarking and ballot.
O-01 identity migration and evaluator activation remain separate later work.

