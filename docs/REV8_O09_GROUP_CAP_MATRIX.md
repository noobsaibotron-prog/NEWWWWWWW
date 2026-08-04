# REV8 O-09 — Group cap readiness matrix

**Audit snapshot:** `5efa8aa34a7270a784e5e41beff9950098ab2395`

**Protocol:** `REV8_O09_GROUP_READINESS_AUDIT_PROTOCOL.md`

**Protocol SHA-256:**
`22b106b153d5132a44d726fbc3f84618b46116e7038f1679ce40859e33cef930`

**Status:** `READINESS_BLOCKED`

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
| AP-01 | `len(graphs)` partitions | 32 | `GROUP_AP_PARTITIONS` | PASS | `— / yes / yes` | no |
| AP-02 | `sum(gt_count)` GT | 4096 | `GROUP_AP_GT` | PASS | `— / — / —` | no |
| AP-03 | `sum(prediction_count)` predictions | 1024 | `GROUP_AP_PREDICTIONS` | PASS | `— / — / —` | no |
| AP-04 | `sum(len(edges))` eligible edges | 16384 | `GROUP_AP_ELIGIBLE_EDGES` | PASS | `— / — / —` | no |
| AP-05 | cardinality of the global confidence union | 1024 | `GROUP_AP_DISTINCT_THRESHOLDS` | PASS | `— / yes / yes` | no |
| AP-06 | sum of local distinct-confidence counts | 1024 | `GROUP_AP_LOCAL_THRESHOLDS` | PASS | `— / yes / yes` | no |
| AP-07 | `sum_{partition,threshold} active-edge incidence` | 1056768 | `GROUP_AP_PREFIX_EDGE_INCIDENCE` | PASS | `— / yes / yes` | no |
| CV-01 | `len(by_unit)` units | 32 | `GROUP_COVERAGE_UNITS` | PASS | `— / yes / yes` | no |
| CV-02 | maximum partitions in one unit | 8 | `GROUP_COVERAGE_PARTITIONS_PER_UNIT` | PASS | `— / yes / yes` | no |
| CV-03 | `sum(gt_count)` GT | 8192 | `GROUP_COVERAGE_GT` | PASS | `— / — / —` | no |
| CV-04 | `sum(prediction_count)` predictions | 8192 | `GROUP_COVERAGE_PREDICTIONS` | PASS | `— / — / —` | no |
| CV-05 | `sum(len(edges))` eligible edges | 16384 | `GROUP_COVERAGE_ELIGIBLE_EDGES` | PASS | `— / — / —` | no |
| SP-01 | `len(graphs)` partitions | 16 | `GROUP_SPEARMAN_PARTITIONS` | PASS | `— / yes / yes` | no |
| SP-02 | `sum(gt_count)` GT | 128 | `GROUP_SPEARMAN_GT` | PASS | `— / — / —` | no |
| SP-03 | `sum(prediction_count)` predictions | 128 | `GROUP_SPEARMAN_PREDICTIONS` | PASS | `— / — / —` | no |
| SP-04 | `sum(len(edges))` eligible edges | 16384 | `GROUP_SPEARMAN_ELIGIBLE_EDGES` | PASS | `— / — / —` | no |
| SP-05 | `sum(min(gt_count,prediction_count))` support bound | 128 | `GROUP_SPEARMAN_SUPPORT` | PASS | `— / yes / yes` | no |

## 3. Totals

```text
Direct formula under/on/over:            17/17 PASS
Durable cap-1 fixture:                    0/17
Durable cap and cap+1 fixture:            8/17
No dedicated durable boundary:            9/17
Durable solver-not-called proof at over:  0/17
```

The existing full runner covers boundary evidence for AP partitions,
Coverage units, Spearman partitions and the transverse exact-scalar ceiling.
It does not materialize the required 17-row group matrix.

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

## 5. Matrix verdict

```text
GROUP_CAP_FORMULAS = ARITHMETICALLY_CONSISTENT
GROUP_CAP_DURABLE_FALSIFICATION_MATRIX = INCOMPLETE
GROUP_CAP_BALLOT_READINESS = NO
```

