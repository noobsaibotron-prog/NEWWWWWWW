# REV8 O-09 — Group cap readiness matrix

**Remediation base:** `fced92ec802e08b7910f315f6f3dc1d962ed3296`

**Protocol:** `REV8_O09_GROUP_READINESS_AUDIT_PROTOCOL.md`

**Protocol SHA-256:**
`22b106b153d5132a44d726fbc3f84618b46116e7038f1679ce40859e33cef930`

**Status:** `F03_IMPLEMENTED_PENDING_COUNTERCHECK`

This document closes only finding F-03: durable boundary and pre-solve
evidence for the 17 group caps.  It does not close F-04/F-05, O-09 as a
whole, Spearman publication, O-13 or G1c.

## 1. Legend

```text
Formula U/O/O PASS
    A read-only direct countercheck observed the expected predicate at
    cap-1, cap and cap+1: false, false, true.

Durable U/O/O
    Repository fixtures committed for under/on/over.

Durable no-solve
    A committed fixture proves the solver is not called after an over-cap
    rejection.
```

The direct arithmetic countercheck is useful evidence about the predicates.
It does not replace the durable falsification matrix required by the audit
protocol.

## 2. Matrix

| ID | Formula and unit | Cap | Exceed tag | Formula U/O/O | Durable U/O/O | Durable no-solve |
|---|---|---:|---|---:|---|---:|
| AP-01 | `len(graphs)` partitions | 32 | `GROUP_AP_PARTITIONS` | PASS | `yes / yes / yes` | yes |
| AP-02 | `sum(gt_count)` GT | 4096 | `GROUP_AP_GT` | PASS | `yes / yes / yes` | yes |
| AP-03 | `sum(prediction_count)` predictions | 1024 | `GROUP_AP_PREDICTIONS` | PASS | `yes / yes / yes` | yes |
| AP-04 | `sum(len(edges))` eligible edges | 16384 | `GROUP_AP_ELIGIBLE_EDGES` | PASS | `yes / yes / yes` | yes |
| AP-05 | cardinality of the global confidence union | 1024 | `GROUP_AP_DISTINCT_THRESHOLDS` | PASS | `yes / yes / yes` | yes |
| AP-06 | sum of local distinct-confidence counts | 1024 | `GROUP_AP_LOCAL_THRESHOLDS` | PASS | `yes / yes / yes` | yes |
| AP-07 | `sum_{partition,threshold} active-edge incidence` | 1056768 | `GROUP_AP_PREFIX_EDGE_INCIDENCE` | PASS | `yes / yes / yes` | yes |
| CV-01 | `len(by_unit)` units | 32 | `GROUP_COVERAGE_UNITS` | PASS | `yes / yes / yes` | yes |
| CV-02 | maximum partitions in one unit | 8 | `GROUP_COVERAGE_PARTITIONS_PER_UNIT` | PASS | `yes / yes / yes` | yes |
| CV-03 | `sum(gt_count)` GT | 8192 | `GROUP_COVERAGE_GT` | PASS | `yes / yes / yes` | yes |
| CV-04 | `sum(prediction_count)` predictions | 8192 | `GROUP_COVERAGE_PREDICTIONS` | PASS | `yes / yes / yes` | yes |
| CV-05 | `sum(len(edges))` eligible edges | 16384 | `GROUP_COVERAGE_ELIGIBLE_EDGES` | PASS | `yes / yes / yes` | yes |
| SP-01 | `len(graphs)` partitions | 16 | `GROUP_SPEARMAN_PARTITIONS` | PASS | `yes / yes / yes` | yes |
| SP-02 | `sum(gt_count)` GT | 128 | `GROUP_SPEARMAN_GT` | PASS | `yes / yes / yes` | yes |
| SP-03 | `sum(prediction_count)` predictions | 128 | `GROUP_SPEARMAN_PREDICTIONS` | PASS | `yes / yes / yes` | yes |
| SP-04 | `sum(len(edges))` eligible edges | 16384 | `GROUP_SPEARMAN_ELIGIBLE_EDGES` | PASS | `yes / yes / yes` | yes |
| SP-05 | `sum(min(gt_count,prediction_count))` support bound | 128 | `GROUP_SPEARMAN_SUPPORT` | PASS | `yes / yes / yes` | yes |

## 3. Totals

```text
Direct formula under/on/over:            17/17 PASS
Durable cap-1 fixture:                   17/17
Durable cap fixture:                     17/17
Durable cap+1 fixture:                   17/17
Durable solver-not-called proof at over: 17/17
```

The declarative fixtures live in
`ml_v3/benchmark/rev8_o09_group_cap_fixtures.py`.  The public-surface test
executes all 51 boundary cases and proves all three solve entrypoints remain
unreached on every `cap+1` rejection.  The full runner now materializes the
same 17-row matrix, records the observed aggregate, target tag, status,
reason and solve-call trace, and retains the transverse exact-scalar ceiling.

## 4. Combined ordering countercheck

A read-only dynamic countercheck observed:

```text
group-only:
  GROUP_COVERAGE_UNITS
  GROUP_COVERAGE_PARTITIONS_PER_UNIT
  solve calls = 0

A1-only:
  SUBGRAPH_<canonical-partition-key>_GT
  solve calls = 0

combined:
  SUBGRAPH_<canonical-partition-key>_GT
  GROUP_COVERAGE_UNITS
  GROUP_COVERAGE_PARTITIONS_PER_UNIT
  solve calls = 0
```

The order remained stable after reversing the input partitions because the
public surface canonicalizes them. This falsifies a present runtime ordering
bug, but the order and no-solve invariant are not pinned by durable fixtures.

## 5. Durable evidence

```text
test_all_17_group_caps_have_public_under_on_over_and_no_solve
test_combined_over_caps_are_ordered_and_still_pre_solve
test_empty_groups_and_zero_support_remain_explicit_na
test_cap_boundary_workload_contains_accept_and_reject_cases
test_cap_boundary_evidence_preserves_absent_preflight
```

The cap workload uses explicit solve-entrypoint instrumentation.  Under/on
results are cap evidence only; they are not solver-correctness evidence.
For every over case the public evaluator returns the structural rejection
with an empty solve-call trace.

## 6. Matrix verdict

```text
GROUP_CAP_FORMULAS = ARITHMETICALLY_CONSISTENT
GROUP_CAP_DURABLE_FALSIFICATION_MATRIX = COMPLETE
GROUP_CAP_SOLVER_NOT_CALLED_AT_OVER = PROVED_BY_COMMITTED_FIXTURES
F03_IMPLEMENTATION = PENDING_INDEPENDENT_COUNTERCHECK
O09_GROUP_CAP_BALLOT_READY = NOT_CLAIMED_BY_THIS_TRANCHE
```
