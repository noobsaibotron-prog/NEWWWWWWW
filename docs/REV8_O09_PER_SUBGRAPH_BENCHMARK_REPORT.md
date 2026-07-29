# REV8 O-09 — Per-subgraph benchmark evidence

**Status:** `EVIDENCE_ONLY_CAPS_NOT_ACTIVE`

**REV8 SPEC GO:** `NO`

**Benchmark source commit:** `e8222688375612edc58914986aa223df625fb10a`

**Gate platform:** macOS 15.5, Darwin 24.5, arm64, CPython 3.12.13,
NumPy 2.5.1

## 1. Scope and epistemic boundary

This report records the isolated A2 benchmark for one normalized matching
subgraph:

```text
(evaluation_unit_key, record_family, problem_type)
```

It measures:

- exact K1–K4 optimization;
- K6 scientific/diagnostic canonicalization;
- exact severity, onset, and offset upper envelopes;
- candidate structural ceilings of 128 GT, 128 predictions, 16,384 eligible
  edges, and 65,536 reduced mathematical bits;
- a K1-only AP prefix pressure probe.

It does **not** measure normative group-level Average Precision, B-001
coverage, or Spearman singleton certification. Consequently this report does
not activate O-09, does not make the candidate ballot-ready, and cannot
support REV8 SPEC GO by itself.

## 2. Isolated invocation and provenance

Normative benchmark processes used:

```text
/Users/marco/aieq_data/motore_v3/env/venv/bin/python
  -I -S -B
  ml_v3/benchmark/rev8_o09_isolated_bootstrap.py
  --output <external-path>/rev8_o09_candidate_benchmark_raw_v3.json
  --max-size 128
  --repeat-small 3
  --repeat-large 3
```

The bootstrap executes before the repository or locked site-packages enter
`sys.path`. It rejects non-isolated startup, `sitecustomize`/`PYTHONPATH`
configuration, bytecode caches, `.pyc` files, native project shadows, and
non-source project loaders. Parent and workers attest the exact bootstrap
invocation, source hashes, loaded modules, platform lock, and absence of
bytecode.

Raw artifact:

```text
schema:
  aieq-v3-rev8-o09-candidate-benchmark-4

file_sha256:
  8fa7a09bb5aed7edf3c5e5607cb4000e1c7f473a54da484f2403af478c4accce

payload_sha256:
  1ce0886d89df81dfabfe01e2a1ebbc4da7e1017fd7915fa9a9a3577e7f5726a8
```

The run contains 27 workload cells with 3 fresh workers per cell: 81 workers
in total. Every worker reported the same 13 protected source hashes and 26
loaded project modules as the parent. All scientific-result hashes were
identical across the three repetitions of each cell.

## 3. Size-128 results

Times are wall-clock seconds. RSS is the maximum worker high-water mark,
including import, graph construction, preflight, solve, and result
materialization.

| Workload | Wall min | Wall median | Wall max | Peak RSS | Exact bound |
|---|---:|---:|---:|---:|---:|
| unique additive | 23.585 | 23.947 | 24.196 | 67,944,448 B | 2,054 |
| degenerate additive | 153.688 | 154.174 | 154.755 | 433,258,496 B | 1,806 |
| unique product | 26.500 | 26.682 | 26.782 | 70,205,440 B | 2,054 |
| bit-stress additive | 21.747 | 21.886 | 22.143 | 69,369,856 B | 65,536 |
| bit-stress product | 24.512 | 25.005 | 25.468 | 73,826,304 B | 65,536 |
| bit-stress degenerate | 185.508 | 186.325 | 187.652 | 915,898,368 B | 65,536 |
| AP K1 prefix pressure | 4.411 | 4.422 | 4.448 | 63,307,776 B | N/A |

The combined worst case uses 128 GT, 128 predictions, 16,384 eligible edges,
128 matched pairs, a huge optimum set, K6, and three independent metric
envelopes.

## 4. Materialized exact values

The candidate preflight bound is not supported only by synthetic metadata:

- additive K2 materializes a reduced 65,282-bit rational
  (32,641-bit numerator + 32,641-bit denominator);
- product K4 materializes a reduced 65,282-bit rational;
- degenerate K3 materializes exactly 65,536 bits;
- the three combined-limit graphs all report a conservative bound of 65,536
  with no provisional exceed.

The explicit scalar boundary bundle produces:

| Case | Bound | Result |
|---|---:|---|
| immediately below | 65,535 | accepted |
| on the ceiling | 65,536 | accepted |
| immediately above | 65,537 | `EXACT_SCALAR_BIT_LENGTH` |
| onset publication `/48` over-case | 65,542 | `EXACT_SCALAR_BIT_LENGTH` |

Only K3 reaches exactly 65,536 materialized bits. K2 and K4 retain 254 bits of
margin; this report does not claim that every exact scalar or envelope
materializes the ceiling.

## 5. Operational interpretation

The provisional per-subgraph ceilings are technically feasible on the locked
platform for the isolated A2 kernel. The measured worst worker is:

```text
wall max = 187.65187787497416 s
RSS max  = 915,898,368 B = 873.47 MiB
```

Non-normative operational headroom for later CI/evidence execution should be
at least:

```text
watchdog >= 300 s per worker
memory   >= 1.5 GiB per worker
preferred CI allowance = 2 GiB × simultaneous workers
```

Wall time and runtime memory are operational evidence, not reduced
mathematical limits. Structural caps and bit-length are deterministic
preflight properties; OOM and timeout remain fatal runtime failures.

## 6. Reported counter-check result

Three independent read-only lenses reported recalculating the raw artifact:

- provenance/epistemic scope;
- exact optimizer, boundaries, materialized bit-length, time, and RSS;
- metric coverage and ballot boundary.

All three reported the raw artifact clean as per-subgraph evidence and
rejected using it as a complete O-09 activation ballot. These are reported
review results from the working session; they are not a substitute for a
separately committed counter-check artifact.

## 7. Required next tranche

Before O-09 activation, a separate candidate-only group-level tranche must
materialize and benchmark:

1. full group Average Precision with distinct-confidence atomic ties,
   K1 recomputation at each prefix, exact AP, `AP_group64`, and macro
   `mean64`;
2. B-001 `coverage_minus`/`coverage_plus` over the product of per-partition
   optimum sets, including `NO_ACTIONABLE_GT`;
3. Spearman pooled support, tie/zero-variance handling, and exact singleton
   certification across composed optimum sets;
4. group-level conservative scalar/work bounds, under/on/over fixtures,
   reason codes, and fatal blast radius.

Until that tranche and its independent counter-check are complete:

```text
O-09 caps        = INACTIVE
ballot_ready     = false
REV8 SPEC GO     = NO
```
