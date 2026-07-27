# REV8 Implementation Specification

## Status

Draft — no repository files modified.
Date: 2026-07-27
Branch: `feature/motore-v3-g1c-reseal` (not accessible from this environment)
Worktree: `/Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1c-reseal`

---

## Table of Contents

1. [Decisions Summary](#1-decisions-summary)
2. [C1 — Event Band Derivation (Discrete Indices)](#2-c1--event-band-derivation-discrete-indices)
3. [C2 — Matching Solver (Exact + Resource Guard)](#3-c2--matching-solver-exact--resource-guard)
4. [C3 — Direction Sum (Exact Rational)](#4-c3--direction-sum-exact-rational)
5. [C4 — GT Duplicate Detection (Class-Specific)](#5-c4--gt-duplicate-detection-class-specific)
6. [C5 — problem_type_id vs Anomaly Class Index](#5-c5--problem_type_id-vs-anomaly-class-index)
7. [C6 — Pairing Independence / Anti-Gaming](#6-c6--pairing-independence--anti-gaming)
8. [C1-bis — Schema Fix (exclusiveMinimum → minimum)](#7-c1-bis--schema-fix-exclusiveminimum--minimum)
9. [Architecture: Modular _v2 Isolation](#8-architecture-modular--v2-isolation)
10. [Implementation Checklist](#9-implementation-checklist)
11. [Validation Checkpoints](#10-validation-checkpoints)
12. [Open Blockers](#11-open-blockers)
13. [Test Requirements](#12-test-requirements)
14. [References](#13-references)

---

## 1. Decisions Summary

| # | Decision | Status | Section |
|---|----------|--------|---------|
| **C1** | Event bands use discrete canonical indices (Option B) | ✅ Decided | §2 |
| **C2** | Exact solver (Option A) + resource FAIL guard | ✅ Decided | §3 |
| **C3** | Direction sum uses exact rational arithmetic | ✅ Decided | §4 |
| **C4** | GT duplicate detection — class-specific | 🟡 Reopened | §5 |
| **C5** | problem_type_id ≠ anomaly_class_index | ✅ Decided | §6 |
| **C6** | Pairing independence / anti-gaming | 🔴 Blocker | §6 |
| **C1-bis** | `exclusiveMinimum: 0` → `minimum: 0` on `width_octaves` | ✅ Decided | §7 |
| **Governance** | All REV8 code in `_v2` modules, REV7 untouched | ✅ Decided | §8 |

---

## 2. C1 — Event Band Derivation (Discrete Indices)

### 2.1 Decision

Event bands use discrete canonical indices (Option B), not continuous `band_iou_log2`.

### 2.2 Formal Specification

#### 2.2.1 Canonical Grid

120 centers, inclusive, from 20 to 20000 Hz:

```
center[i] = 20 * (20000 / 20) ** (i / 119), i = 0..119
```

Bands are triangular on the `log2(f)` axis. The internal support of band `i` goes from the previous center to the next center. For the two extreme bands, use a virtual center obtained with the same geometric ratio.

#### 2.2.2 Projection Rule

Given a dynamic event with `center_hz` and `width_octaves`:

```
raw_lo = center_hz * 2^(-width_octaves / 2)
raw_hi = center_hz * 2^(+width_octaves / 2)
```

The canonical band indices are:

```
i_lo = argmin_{i ∈ [0,119]} |log2(raw_lo) - log2(center[i])|
i_hi = argmin_{i ∈ [0,119]} |log2(raw_hi) - log2(center[i])|
```

Tie-break: smaller index.

Monotonicity guarantee: `raw_lo ≤ raw_hi ⟹ i_lo ≤ i_hi`.

#### 2.2.3 Boundary Freezing

The 119 boundaries between adjacent canonical centers are the geometric midpoints:

```
boundary[i] = √(center[i] * center[i+1]), i = 0..118
```

These 119 boundary values must be frozen as golden binary64 constants and hashed.

Projection rule using boundaries:

```
raw <= boundary[0]     → band 0
boundary[i-1] < raw <= boundary[i]  → band i
raw > boundary[118]    → band 119
```

#### 2.2.4 Discrete IoU Formula

For two event bands `[a, b]` and `[c, d]` where `0 ≤ a ≤ b < 120` and `0 ≤ c ≤ d < 120`:

```
L_A = b - a + 1
L_B = d - c + 1
I = max(0, min(b, d) - max(a, c) + 1)
U = L_A + L_B - I
IoU_index = I / U
```

Properties:
- `0 ≤ IoU_index ≤ 1`
- `IoU_index([a,a], [a,a]) = 1`
- `IoU_index([a,a], [a+1,a+1]) = 0`
- No division by zero: `U ≥ 1` for all valid intervals

Threshold check without division:

```
IoU_index ≥ 0.5  ⟺  2 * I ≥ U
```

#### 2.2.5 Width Upper Bound

```
W_MAX = 2 * log2(20000 / 20) ≈ 19.93 octaves
```

Constraint: `0 ≤ width_octaves ≤ W_MAX`.

#### 2.2.6 Metamorphic Property (H/S only)

After band derivation, no H/S matching path may re-read `center_hz` or `width_octaves` for eligibility or geometry cost. Formally:

```
B(x) = B(y)  ⇒  Eligibility(x, z) = Eligibility(y, z)
                   ∧ GeometryCost(x, z) = GeometryCost(y, z)
```

for every other event `z`.

#### 2.2.7 Resonance Exception

For Resonance events, `center_hz` remains a primary continuous geometry. The canonical band is secondary/support geometry only. Two Resonance events in the same canonical band but with different `center_hz` may have different geometry costs.

### 2.3 Implementation Requirements

- New module: `ml_v3/contracts/event_geometry_v2.py`
- Functions:
  - `derive_canonical_band(center_hz: float, width_octaves: float) -> tuple[int, int]`
  - `discrete_iou(band_a: tuple[int, int], band_b: tuple[int, int]) -> Fraction`
  - `boundary_projection(raw_hz: float) -> int`
- Golden fixture: `ml_v3/fixtures/g1/boundaries_golden.json` with binary64 boundary values
- All arithmetic on `I`, `U`, `IoU_index` must use `fractions.Fraction` (exact rational)

### 2.4 What Changes

- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §9.2: replace continuous band derivation with discrete derivation
- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §10.2: replace `band_iou_log2` with `band_iou_index` for events
- `ml_v3/contracts/schemas.py`: `width_octaves` changes from `exclusiveMinimum: 0` to `minimum: 0`

### 2.5 What Does NOT Change

- `docs/MOTORE_V3_G1_CONTRACT.md` (REV7) — untouched
- `Source/`, `CMakeLists.txt`, `ml_v2/` — untouched
- Semantic region IoU (§10.1) — remains continuous `band_iou_log2`

---

## 3. C2 — Matching Solver (Exact + Resource Guard)

### 3.1 Decision

Design an exact deterministic solver for the 6-key lexicographic bipartite matching objective. Add a resource guard that produces FAIL if the solver cannot complete within declared resources.

### 3.2 Formal Specification

#### 3.2.1 Matching Objective (K1–K5)

Given a bipartite graph `G = (L, R, E)` where:
- `L` = ground truth events (for a given `evaluation_unit_key` and `problem_type`)
- `R` = prediction events (same)
- `E` = eligible edges (same type, temporal IoU ≥ 0.3, frequency/band eligibility)

Each edge `e ∈ E` carries:

| Key | Quantity | Direction |
|-----|----------|-----------|
| K1 | `temporal_iou(e)` | maximize |
| K2 | `temporal_boundary_error(e)` | minimize |
| K3 | `frequency_error_octaves(e)` | minimize |
| K4 | `primary_geometry_cost(e)` | minimize |
| K5 | `secondary_geometry_cost(e)` | minimize |

All quantities are computed as binary64, then treated as exact rationals (diadic: `n / 2^q`).

The objective is lexicographic:

```
K1: maximize |M|
K2: maximize Σ temporal_iou(e) for e ∈ M
K3: minimize Σ temporal_boundary_error(e) for e ∈ M
K4: minimize Σ primary_geometry_cost(e) for e ∈ M
K5: minimize Σ secondary_geometry_cost(e) for e ∈ M
```

#### 3.2.2 K6 — Canonical Tie-Break

K6 selects the lexicographically smallest sequence of `(GT decision_key, prediction decision_key, occurrence_ordinal)` pairs among all K1–K5 optima.

K6 does NOT use: `severity`, `confidence`, `actionable`, or any candidate-controlled quality metric.

#### 3.2.3 Candidate Algorithm A1 (Reference)

```
V* = OPT_K1K5(∅, ∅)
R = {}  # required edges
F = {}  # forbidden edges

For each edge e_i in canonical order:
    if e_i conflicts with R:
        F.add(e_i)
    else:
        V_i = OPT_K1K5(R ∪ {e_i}, F)
        if V_i == V*:
            R.add(e_i)
        else:
            F.add(e_i)

Return R when |R| == K1*
```

Where `OPT_K1K5(R, F)` solves the standard K1–K5 bipartite matching problem with required/forbidden edge constraints.

#### 3.2.4 Candidate Algorithm A2 (Optimization)

Encode K6 as an additive priority weight:

```
P(e_i) = 2^(m - i)
```

where `m` is the total number of edges and `i` is the canonical index.

After K1–K5 are frozen, maximize:

```
Σ P(e_i) for e_i ∈ M
```

This encodes lexicographic preference: the first differing bit dominates all subsequent bits.

**A1 is the reference candidate. A2 is the optimization candidate.** A2 must prove bit-level/semantic equivalence with A1 before adoption.

#### 3.2.5 Resource Guard

If the solver cannot complete within declared resources → `EventMatchingError` (FAIL).

The resource guard must be deterministic and independent of record order.

Acceptable resource measures (not wall-clock time):
- `MAX_VERTICES`
- `MAX_EDGES`
- `MAX_MEMORY_UNITS`
- Deterministic operation count

The old `MAX_SEARCH_NODES = 200000` is a property of the historical matcher, not a normative constant for REV8.

#### 3.2.6 Exact Arithmetic

All K1–K5 cost accumulations use exact rational arithmetic. Every binary64 finite value `x` is represented as `n / 2^q` with integer `n` and bounded exponent `q`. Sums use `fractions.Fraction` or equivalent.

The `sum_pairwise64` function from §10.0 is used for published metric aggregation, NOT for the solver's internal optimization.

### 3.3 Implementation Requirements

- New module: `ml_v3/benchmark/event_matching_v2.py`
- Functions:
  - `build_bipartite_graph(gt_events, pred_events) -> Graph`
  - `eligible_edges(gt, pred) -> list[Edge]`
  - `edge_costs(edge) -> CostVector` (K1–K5 as exact rationals)
  - `OPT_K1K5(graph, required, forbidden) -> Matching`
  - `K6_canonical_select(optima, edges) -> Matching`
  - `match_events(gt, pred) -> Matching | EventMatchingError`
- The old `event_matching.py` remains unchanged (REV7)

### 3.4 Oracle Testing

The exhaustive solver (old `event_matching.py` with small graphs) serves as an oracle:

```
solver_new(G) == oracle_exhaustive(G)
```

for all graphs where `|L| ≤ 6` and `|R| ≤ 6`.

Comparison must be exact on:
- Matched pair set (semantic identity, not just count)
- K1 value
- K2 value
- K3 value
- K4 value
- K5 value
- K6 selected matching

### 3.5 What Changes

- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §10.1: replace solver description with exact deterministic algorithm + resource guard
- New: `ml_v3/benchmark/event_matching_v2.py`

### 3.6 What Does NOT Change

- `ml_v3/benchmark/event_matching.py` — untouched (REV7)
- `docs/MOTORE_V3_G1_CONTRACT.md` — untouched

---

## 4. C3 — Direction Sum (Exact Rational)

### 4.1 Decision

The evaluator computes the net sum of `tonal_curve_db` values whose canonical band centers fall within the bundle's band, in ascending band index order, as exact rationals of their binary64 bit patterns. The sign of the exact sum produces `boost` (positive) or `cut` (negative). An exactly null sum produces no Thinness/DullSound bundle and is FAIL.

### 4.2 Formal Specification

For a bundle with band `[i_lo, i_hi]`:

```
values = [tonal_curve_db[i] for i in range(i_lo, i_hi + 1) if center[i] is in band]
sum_exact = Σ exact_rational(bit_pattern(values[j]))
```

where `exact_rational(bit_pattern(x))` converts the binary64 bit pattern to the exact rational it represents.

```
if sum_exact > 0: direction = boost
if sum_exact < 0: direction = cut
if sum_exact == 0: FAIL (violates direction hypothesis)
```

No floating-point summation is permitted for this branch.

### 4.3 Implementation Requirements

- New or updated module: `ml_v3/contracts/evaluator_v2.py`
- Function: `compute_direction(band: tuple[int, int], tonal_curve_db: list[float]) -> str`
- Must use `fractions.Fraction` or equivalent for the sum
- Must reject `sum == 0` with a specific error

### 4.4 What Changes

- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §9.3: replace float summation with exact rational summation
- `ml_v3/contracts/validate.py` or new `validate_v2.py`: add null-sum FAIL check

---

## 5. C4 — GT Duplicate Detection (Class-Specific)

### 5.1 Decision

GT duplicate detection must be class-specific. Different problem types have different structural keys for duplicate/contradiction identification.

### 5.2 Structural Keys by Class

#### Dynamic Events (Harshness / Sibilance)

```
duplicate_key = (problem_type, N64(start_s), N64(end_s), i_lo, i_hi)
```

Where `i_lo`, `i_hi` are the canonical band indices derived from `center_hz` and `width_octaves`.

Two GT events with the same `duplicate_key` but different `severity` or `confidence` are `CONTRADICTORY_GT`.

Two GT events with the same `duplicate_key` and identical `severity`, `confidence`, `actionable` are `DUPLICATE_GT`.

#### Dynamic Events (Resonance)

```
duplicate_key = (problem_type, N64(start_s), N64(end_s), N64(center_hz), canonical_band)
```

`center_hz` is semantically observable for Resonance (enters eligibility within 1/3 octave, center error is measured).

#### Semantic Regions

Class-specific rules:

| Class | Significant fields for duplicate_key |
|-------|--------------------------------------|
| Resonance | problem_type, time interval, band, center_hz |
| Thinness/DullSound | problem_type, time interval, band, direction |
| Muddiness/Boominess/BoxyMidrange | problem_type, time interval, band_lo_hz, band_hi_hz |

Two semantic regions with the same `duplicate_key` but incompatible `direction` (e.g., boost vs cut for Thinness) are `CONTRADICTORY_GT`.

Two semantic regions with the same `duplicate_key` and identical values are `DUPLICATE_GT`.

Two semantic regions with different `duplicate_key` are `DISTINCT_GT`.

### 5.3 Implementation Requirements

- New module: `ml_v3/contracts/validate_v2.py`
- Functions:
  - `structural_duplicate_key_gt(event) -> tuple`
  - `structural_duplicate_key_region(region) -> tuple`
  - `check_gt_duplicates(annotations) -> list[DuplicateError]`
  - `check_gt_contradictions(annotations) -> list[ContradictionError]`
- `DuplicateError`: two GT records with same structural key and same values
- `ContradictionError`: two GT records with same structural key but incompatible values

### 5.4 What Changes

- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §9.6: add class-specific duplicate/contradiction rules
- New: `ml_v3/contracts/validate_v2.py`

### 5.5 What Does NOT Change

- `ml_v3/contracts/validate.py` — untouched (REV7)
- `ml_v3/contracts/schemas.py` — untouched (REV7)

---

## 6. C5 — problem_type_id vs Anomaly Class Index

### 6.1 Decision

`problem_type_id` and `anomaly_class_index` are distinct indices that must not be used interchangeably.

### 6.2 Mapping

| Class | problem_type_id | anomaly_class_index |
|-------|-----------------|---------------------|
| Resonance | 0 | 0 |
| Harshness | 1 | 1 |
| Muddiness | 2 | — |
| Sibilance | 3 | 2 |
| Boominess | 4 | — |
| Thinness | 5 | — |
| BoxyMidrange | 6 | — |
| DullSound | 7 | — |

### 6.3 Implementation Requirements

- `ml_v3/contracts/profiles.py`: ensure `problem_type_id()` and `anomaly_class_index()` are clearly separated
- Add explicit test: `problem_type_id("Sibilance") == 3` and `anomaly_class_index("Sibilance") == 2`

### 6.4 What Changes

- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §9.3: add explicit note about the distinction
- `ml_v3/contracts/profiles.py`: add clarifying comments or rename functions if needed

---

## 7. C1-bis — Schema Fix (exclusiveMinimum → minimum)

### 7.1 Decision

Change `exclusiveMinimum: 0` to `minimum: 0` on `width_octaves` in both event schemas in `schemas.py`.

### 7.2 Rationale

`exclusiveMinimum: 0` rejects `width_octaves = 0`, but C1 defines the single-band event as `width_octaves = 0` and `i_lo == i_hi` — explicitly "representable and non-degenerate".

### 7.3 Implementation

- File: `ml_v3/contracts/schemas.py`
- Lines 308 and 396 (approximate): change `"exclusiveMinimum": 0` to `"minimum": 0` for `width_octaves`

### 7.4 What Changes

- `ml_v3/contracts/schemas.py`: two occurrences of `exclusiveMinimum: 0` → `minimum: 0` for `width_octaves`

---

## 8. Architecture: Modular _v2 Isolation

### 8.1 Decision

All REV8 code lives in `_v2` modules. REV7 modules remain byte-identical. The activation switch is a single atomic commit.

### 8.2 Module Mapping

| REV7 Module (untouched) | REV8 Module |
|--------------------------|-------------|
| `ml_v3/contracts/schemas.py` | `ml_v3/contracts/schemas_v2.py` |
| `ml_v3/contracts/validate.py` | `ml_v3/contracts/validate_v2.py` |
| `ml_v3/benchmark/event_matching.py` | `ml_v3/benchmark/event_matching_v2.py` |
| `ml_v3/contracts/profiles.py` | `ml_v3/contracts/profiles_v2.py` (if needed) |
| `ml_v3/contracts/evaluator.py` | `ml_v3/contracts/evaluator_v2.py` (if needed) |
| `ml_v3/contracts/event_geometry.py` | `ml_v3/contracts/event_geometry_v2.py` (new) |

### 8.3 Activation

A single atomic commit switches the dispatcher from REV7 modules to REV8 modules. No partial activation.

### 8.4 What Is Forbidden

- Modifying any REV7 module (`schemas.py`, `validate.py`, `event_matching.py`, `profiles.py`)
- Creating a hybrid state (REV7 dispatcher + partial REV8 schema)
- Modifying `docs/MOTORE_V3_G1_CONTRACT.md` (REV7)
- Modifying `Source/`, `CMakeLists.txt`, `ml_v2/`

---

## 9. Implementation Checklist

### Phase 1: Document Changes

- [ ] Apply C1 (discrete band derivation, discrete IoU) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- [ ] Apply C2 (exact solver + resource guard) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- [ ] Apply C3 (exact rational direction sum) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- [ ] Apply C4 (class-specific duplicate detection) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- [ ] Apply C5 (problem_type_id vs anomaly_class_index) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- [ ] Apply C6 (pairing independence) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- [ ] Apply C1-bis (schema fix) to `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`

### Phase 2: Code Changes (_v2 modules only)

- [ ] Create `ml_v3/contracts/schemas_v2.py` (from `schemas.py` with `minimum: 0` fix)
- [ ] Create `ml_v3/contracts/validate_v2.py` (class-specific duplicate/contradiction checks)
- [ ] Create `ml_v3/contracts/event_geometry_v2.py` (band derivation, discrete IoU, boundary freezing)
- [ ] Create `ml_v3/benchmark/event_matching_v2.py` (exact solver A1 + resource guard)
- [ ] Create `ml_v3/contracts/evaluator_v2.py` (exact rational direction sum)
- [ ] Create `ml_v3/contracts/profiles_v2.py` (if needed for clarity)

### Phase 3: Test Changes

- [ ] Add golden fixture for boundaries: `ml_v3/fixtures/g1/boundaries_golden.json`
- [ ] Add test for discrete IoU: all edge cases (same band, adjacent, partial overlap, disjoint)
- [ ] Add test for band derivation: width=0, center on boundary, center off-center, raw_lo < 20, raw_hi > 20000
- [ ] Add test for metamorphic property: same band → same eligibility and cost for H/S
- [ ] Add test for Resonance: same band, different center → different cost
- [ ] Add test for exact rational direction sum: boost, cut, null (FAIL)
- [ ] Add test for class-specific duplicate detection: H/S duplicate, H/S contradictory, Resonance distinct
- [ ] Add test for problem_type_id vs anomaly_class_index: Sibilance=3 vs Sibilance index=2
- [ ] Add test for K6 tie-break: two optimum K1–K5 matchings, verify K6 selects canonical one
- [ ] Add oracle comparison test: new solver vs exhaustive solver on small graphs
- [ ] Add mutation tests: ignore K5, invert K3, use float sums, use ID in tie-break, use severity in tie-break, greedy first-fit
- [ ] Add ID permutation test: rename all IDs, verify same pairing and metrics
- [ ] Add input permutation test: permute GT/prediction order, verify same result

### Phase 4: Review

- [ ] Diff only on `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`, `ml_v3/contracts/schemas_v2.py`, `ml_v3/contracts/validate_v2.py`, `ml_v3/contracts/event_geometry_v2.py`, `ml_v3/benchmark/event_matching_v2.py`, `ml_v3/contracts/evaluator_v2.py`
- [ ] No modification to `docs/MOTORE_V3_G1_CONTRACT.md` (REV7)
- [ ] No modification to `Source/`, `CMakeLists.txt`, `ml_v2/`
- [ ] No modification to any non-`_v2` Python module

---

## 10. Validation Checkpoints

### Checkpoint 1: event_matching.py FAIL above MAX_SEARCH_NODES

```
grep -n "MAX_SEARCH_NODES\|EventMatchingError\|FAIL" ml_v3/benchmark/event_matching.py
```

Expected: `MAX_SEARCH_NODES = 200_000` (line 71), `EventMatchingError` raised at lines 260-262 when `nodes > MAX_SEARCH_NODES`.

### Checkpoint 2: schemas.py width_octaves uses exclusiveMinimum: 0

```
grep -n "exclusiveMinimum\|minimum" ml_v3/contracts/schemas.py | head -10
```

Expected: `width_octaves` uses `"exclusiveMinimum": 0` in both event schemas.

### Checkpoint 3: validate.py has no duplicate GT check

```
sed -n '348,358p' ml_v3/contracts/validate.py
```

Expected: validator checks direction, band, severity, confidence — no duplicate GT control.

### Checkpoint 4: problem_type_id vs anomaly_class_index

```
sed -n '60,85p' ml_v3/contracts/profiles.py
```

Expected: `problem_type_id()` maps 8 types (Resonance=0, Harshness=1, Muddiness=2, Sibilance=3, Boominess=4, Thinness=5, BoxyMidrange=6, DullSound=7). `anomaly_class_index()` maps 3 classes (Resonance=0, Harshness=1, Sibilance=2).

### Checkpoint 5: test_g1c_rev8_primitives_v2.py has 431 lines

```
wc -l ml_v3/tests/test_g1c_rev8_primitives_v2.py
```

Expected: 431 lines.

---

## 11. Open Blockers

### C6 — Pairing Independence / Anti-Gaming (🔴 BLOCKER)

**Problem**: The tie-break (K6) must not use candidate-controlled quantities that are subsequently evaluated as metrics. If K6 uses `severity`, `confidence`, or `actionable`, the matching can be gamed to optimize those metrics.

**Required before reseal**:
1. Inventory of all downstream metrics and their pairing sensitivity
2. Proof that K6 does not use any candidate-controlled quality metric
3. Ambiguity policy for when `|M*| > 1` and metrics differ across optima
4. Formal definition of `matching_equivalence_key`

**Options for ambiguity policy** (not yet decided):
- A: Ambiguity → N/A/FAIL
- B: Worst-case across optima
- C: Permutation-invariant metric on equivalence class
- D: Publish `[Q_min, Q_max]` envelope, use conservative extreme for gate

### C4 — Class-Specific Duplicate Key (🟡 YELLOW)

**Problem**: The exact structural key for dynamic events needs to be frozen. H/S uses `(problem_type, start_s, end_s, i_lo, i_hi)`. Resonance uses `(problem_type, start_s, end_s, N64(center_hz), canonical_band)`. This distinction must be normatively specified.

### Projection / Boundary Freezing (🟡 YELLOW)

**Problem**: The 119 boundary values must be frozen as golden binary64 constants. The exact procedure and golden fixture need to be specified and generated.

### W_MAX (🟡 YELLOW)

**Problem**: `W_MAX = 2 * log2(20000/20) ≈ 19.93` octaves is a safety upper bound. Whether this is also the semantic maximum needs a perceptual/dataset justification.

### K1–K5 Exact Optimizer (🟡 YELLOW/BLOCKING dependency)

**Problem**: The exact solver for K1–K5 must be implemented and proven correct. A1 (repeated feasibility) is the reference candidate. A2 (vector cost) is the optimization candidate.

---

## 12. Test Requirements

### 12.1 C1 Tests

- Discrete IoU: same single band → 1, adjacent single bands → 0, partial overlap → correct fraction, disjoint → 0
- Band derivation: width=0, center on canonical center, center between two centers, raw_lo < 20, raw_hi > 20000
- Metamorphic property: same band → same eligibility and cost for H/S
- Resonance exception: same band, different center → different cost
- Boundary projection: values exactly on boundary, just below, just above

### 12.2 C2 Tests

- Oracle equivalence: new solver vs exhaustive solver on small graphs (|L|≤3, |R|≤3)
- K6 tie-break: two optimum K1–K5 matchings, verify K6 selects canonical one
- ID permutation: rename all IDs, verify same pairing and metrics
- Input permutation: permute GT/prediction order, verify same result
- Resource guard: verify FAIL when graph exceeds resource limits
- Mutation tests: ignore K5, invert K3, use float sums, use ID in tie-break, use severity in tie-break, greedy first-fit

### 12.3 C3 Tests

- Direction sum: boost (positive), cut (negative), null (FAIL)
- Exact rational arithmetic: verify no floating-point summation is used

### 12.4 C4 Tests

- H/S duplicate GT: same type, time, band, severity → DUPLICATE_GT
- H/S contradictory GT: same type, time, band, different severity → CONTRADICTORY_GT
- Resonance distinct GT: same band, different center → DISTINCT_GT
- Thinness contradictory GT: same time, band, direction boost vs cut → CONTRADICTORY_GT

### 12.5 C5 Tests

- `problem_type_id("Sibilance") == 3`
- `anomaly_class_index("Sibilance") == 2`
- All 8 problem type IDs match their canonical strings

### 12.6 C6 Tests

- Metric invariance: permute severity among equivalent predictions, verify metric unchanged (or AMBIGUOUS)
- Tie-break independence: verify K6 does not use severity/confidence/actionable
- Pairing sensitivity inventory: each metric classified as INVARIANT, AMBIGUITY-SENSITIVE, or NOT PAIRING-DEPENDENT

---

## 13. References

- REV7 contract: `docs/MOTORE_V3_G1_CONTRACT.md` (1420 lines, frozen)
- REV8 candidate: `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`
- Correction plan: `/private/tmp/claude-501/-Users-marco/83106d04-230a-4bb8-a5e7-66eeb816f7d4/scratchpad/REV8_CORREZIONI_POST_REDTEAM.md`
- Plan file: `.kilo/plans/1785154801327-rev8-countercheck-decisions.md`
- Test primitives: `ml_v3/tests/test_g1c_rev8_primitives_v2.py` (431 lines)
- Fixtures: `ml_v3/fixtures/g1/SHA256SUMS`
- Worktree: `/Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1c-reseal`
- Repo: `https://github.com/noobsaibotron-prog/NEWWWWWWW`
- Branch: `feature/motore-v3-g1c-reseal`
- HEAD: `549b9a6c5b3ae396f95e7b6daa98af9d265e490e`

---

## Semaphore

| Area | Status |
|------|--------|
| C1 direction B | 🟢 GREEN |
| discrete IoU formula | 🟢 GREEN |
| exact rational arithmetic | 🟢 GREEN |
| projection/boundaries | 🟡 YELLOW |
| W_MAX safety bound | 🟡 YELLOW+ |
| semantic width bound | OPEN |
| H/S geometry authority | 🟢 GREEN direction |
| Resonance geometry | 🟡 YELLOW |
| C2 A + resource FAIL | 🟢 GREEN |
| A1 repeated optimization | 🟢 GREEN candidate |
| A2 vector cost | 🟡 YELLOW candidate |
| K1–K5 exact optimizer | 🟡 YELLOW/BLOCKING dependency |
| C3 | 🟢 GREEN |
| C4 | 🟡 YELLOW / reopened |
| C5 | 🟢 GREEN |
| **C6** | 🔴 **BLOCKER** |
| metric invariance inventory | 🔴 OPEN |
| ambiguity policy | 🔴 OPEN |
| _v2 isolation | 🟢 GREEN governance |
| live REV7 changes | 🔴 FORBIDDEN |
| REV8 reseal | 🔴 NO-GO |
| training | 🔴 NO-GO |