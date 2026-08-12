# REV8 — O-02 / O-03 / O-18 Materialization Report

## 1. Status

```text
MATERIALIZATION: COMPLETE
IMMUTABLE IMPLEMENTATION COMMIT: 33a0957b2353031118c44fe90eb7f8d85826c84c
PARENT: 5da87483b975a7ec4f3216cad52197684c029d2e
INDEPENDENT COUNTER-CHECK: PENDING
FREEZE / ACTIVATION: NOT AUTHORIZED
REV8 SPEC GO: NO
PUSH: NO
```

This report records the isolated materialization of the signed O-02, O-03,
and O-18 authorities. It is evidence for review, not an activation manifest
and not a claim that G1c or REV8 is complete.

## 2. Scope

The implementation commit contains:

- O-02 canonical 120-band frequency geometry and 119 golden boundaries;
- O-03 canonical width authority and projection semantics;
- O-18 canonical binary64 parsing, exact bit-to-rational conversion, and
  published binary64 reduction helpers;
- deterministic generators and sealed artifacts;
- positive, boundary, mutation, regeneration, and isolation tests.

It deliberately excludes:

- O-01 temporal-authority migration;
- `identity_v2.py` migration to canonical ticks and canonical band identity;
- O-09 solver resource policy;
- evaluator activation;
- changes to the live REV7 dispatcher or schemas;
- training, frontend changes, runtime, Ableton, and plugin integration.

## 3. Implementation commit

```text
33a0957b2353031118c44fe90eb7f8d85826c84c
feat(rev8): materialize O02 O03 O18 authorities
```

The commit changes 13 files: 1,402 insertions and 14 deletions.

Primary modules:

```text
ml_v3/contracts/exact_arith_v2.py
ml_v3/contracts/numeric_authority_v2.py
ml_v3/contracts/interval_refinement_v2.py
ml_v3/contracts/frequency_geometry_v2.py
ml_v3/contracts/numeric_artifact_v2.py
```

Artifacts and generator:

```text
ml_v3/fixtures/rev8/generate_o02_o03_o18.py
ml_v3/fixtures/rev8/o02_boundary_artifact_v1.json
ml_v3/fixtures/rev8/o03_width_artifact_v1.json
ml_v3/fixtures/rev8/o18_numeric_artifact_v1.json
ml_v3/fixtures/rev8/o02_o03_o18_generator_report_v1.json
```

Tests:

```text
ml_v3/tests/test_g1c_rev8_o02_o03_o18.py
ml_v3/tests/test_g1c_rev8_primitives_v2.py
```

## 4. O-18 numeric authority

The previous generic `exact()` path converted all values through `float()`.
That silently changed integers above `2^53`. The new implementation:

- preserves native Python integers directly as exact `Fraction` values;
- accepts canonical finite N64 tokens and rejects non-canonical, non-finite,
  uppercase, and negative-zero tokens;
- converts N64 bit patterns to their represented rational values exactly;
- defines a correctly-rounded binary64 authority;
- defines deterministic pairwise summation and mean publication helpers;
- separates optimizer exact arithmetic from published binary64 arithmetic.

Regression coverage includes:

```text
2^53 + 1
2^60 + 1
10^400
```

The signed C7-A fixture is pinned with:

```text
N64(0.2)          = 0x3FC999999999999A
N64(1.2) - N64(1) = 0x3FC9999999999998
distance          = 2 ULP at the scale of 0.2
```

The artifact also distinguishes single rounding of
`log2(9/8) / 3` from a double-round mutation.

## 5. O-02 frequency geometry

O-02 materializes:

- 120 correctly-rounded center values for
  `20 * 1000^(i/119)`, `i = 0..119`;
- 119 correctly-rounded geometric-mean boundaries;
- bit-exact hexadecimal binary64 storage;
- exact canonical JSON bytes and embedded seals;
- monotonicity and center/boundary relationship checks;
- full deterministic regeneration before an artifact is accepted.

Boundary projection is total for finite inputs:

```text
x <= boundary[0]                         -> band 0
boundary[k-1] < x <= boundary[k]         -> band k
x > boundary[118]                        -> band 119
non-finite x                             -> FAIL
```

Equality with a boundary maps to the lower band.

## 6. O-03 width authority

O-03 pins the correctly-rounded safety limit:

```text
W_MAX = 2 * log2(20000 / 20)
bits  = 0x4033EE7B471B3A95
```

The implementation:

- validates finite positive center and finite non-negative width;
- accepts `width_octaves == 0`;
- rejects widths above `W_MAX`;
- computes width endpoints with interval refinement;
- rejects non-finite endpoints;
- saturates finite endpoints outside the canonical grid;
- guarantees a non-degenerate one-band interval for zero width;
- binds the width artifact to the exact O-02 file SHA.

## 7. Correct-rounding procedure

The locked generation environment is:

```text
Python: 3.12.13
mpmath: 1.3.0
```

The shared interval-refinement procedure:

1. reads inputs from exact bit patterns or exact integers;
2. computes an enclosing interval;
3. doubles decimal precision from 80 dps, up to a 1,280 dps cap;
4. stops only when both interval endpoints correctly round to the same
   binary64 value;
5. records the resulting bit pattern in the golden artifact.

The golden artifact, not a platform `libm` call, is the normative output.

## 8. Artifact hashes

### O-02

```text
file SHA-256:
3f27bede753190d569d182d1dcdb683d6a8f5ffbdd083fc2465bb753144faa02

embedded artifact_hash:
ec416e1e25b28b253363ab850ddf205d6fd38d64a1c9f300731cfdbff1450d73

center_hash:
016bbb7394e949a6ee76425af3871c80a43f6f7dc3f32ede01339ff61879f103

boundary_hash:
7a0a015a0dd8145d72fbc1b2c56a0e1beaa75e302e3b9c28687f44d6bd9a836e
```

### O-03

```text
file SHA-256:
20e25255bccd129abbbaaf96c6227e19fc1b78d33d4e1aa93c98bf0e9f0e2f3c

embedded artifact_hash:
062b520fcb1bdd69f76c156fda8118034c259a4f0d0b0dd00ab353f5f60abdad
```

### O-18

```text
file SHA-256:
5f4c6703ac6f0a99acb2090d7a77d3dbafae48d9229327463753c7349e9f9588

embedded artifact_hash:
9fb3a3c084b69e4c238c23eb3f4ba911b7c07600fddf4efc74bc5ae33efdfb88
```

### Generator report

```text
file SHA-256:
1bf5e0572199333cf5bf31674ae0f41eb922259389064b05a42f99b27ed859a1

embedded artifact_hash:
0ec6c7e705ff7a3b080806c83b79fdf097d0e0d54e7da34252a5f603628a8202
```

## 9. Verification evidence

### Targeted immutable-commit run

```text
Ran 77 tests in 0.059s
OK
```

### Full immutable-commit suite

```text
Ran 440 tests in 53.652s
OK
```

The full run used:

```text
PYTHONDONTWRITEBYTECODE=1 \
/Users/marco/aieq_data/motore_v3/env/venv/bin/python \
  -m unittest discover -s ml_v3/tests -p 'test_*.py' -q
```

Additional checks completed:

- deterministic regeneration produces byte-identical artifacts;
- all 119 boundary bins independently satisfy strict midpoint-square
  containment;
- a resealed O-02 center mutation is rejected by regeneration;
- a resealed O-03 `W_MAX` mutation is rejected by regeneration;
- a resealed O-18 mean golden mutation is rejected by recomputation;
- syntax parsing succeeds for the changed Python files;
- `git diff --check` succeeds;
- candidate-only modules are not exported by the active REV7 dispatcher.

## 10. Important diagnostic: REV7 center drift

Correct rounding is not bit-identical to the legacy REV7 `pow` construction.

The generator report records:

```text
different positions: 83 / 120
maximum positive-binary64 order distance: 3 steps
```

This does not modify REV7 and does not alter the active evaluator. It is a
material difference that the independent counter-check must evaluate before
any activation or identity migration. The REV8 artifacts intentionally make
the new authority explicit instead of silently inheriting platform `pow`
results.

## 11. Protected document hashes

The following documents remain unchanged:

```text
REV8 candidate:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

live REV7 contract:
310d538647d71840c6dd8124f1e24281b758bb6dd3e4776aac4b34e5f658a5b0

plan:
8c45a01dbb207e8163f9b3d0613f4547c3fbd5f22680de0e74ba4785f62def77
```

## 12. Gate and next permitted action

This materialization is not yet frozen or activated.

The next permitted action is an independent, read-only counter-check of
immutable commit:

```text
33a0957b2353031118c44fe90eb7f8d85826c84c
```

The counter-check should specifically attack:

1. correct rounding and interval termination;
2. artifact self-seal and regeneration binding;
3. the 83/120 REV7-center diagnostic and its downstream identity impact;
4. integer preservation above `2^53`;
5. single-round publication semantics;
6. projection boundary equality and finite saturation;
7. O-03 width-zero and `W_MAX` edge behavior;
8. REV7 dispatcher and schema isolation.

Only after a clean counter-check should a separate freeze/activation decision
be considered. O-09 should remain a separate subsequent tranche.

