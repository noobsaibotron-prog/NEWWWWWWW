# REV8 O-09 — Group-level benchmark evidence

**Status:** `EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE`

**REV8 SPEC GO:** `NO`

**Reviewer:** single independent reviewer (Claude), continuing this tranche
after Codex exhausted its budget mid-run. **This is not the three-lens
counter-check used elsewhere in G1c/REV8.** The runner and kernel it exercises
were built and unit-tested by Codex (538 canonical tests, including 56
targeted to this tranche) before the handoff; this report covers what was
independently re-verified after the handoff, not a fresh three-party review.

**Benchmark source commit:** `692cda55ae9d5a2b0d89c9dafb7c9d35e4c49752`

**Gate platform:** macOS 15.5, Darwin 24.5, arm64, CPython 3.12.13,
NumPy 2.5.1

## 1. Scope

This report records the isolated group-level benchmark for the three surfaces
the per-subgraph O-09 evidence
(`REV8_O09_PER_SUBGRAPH_BENCHMARK_REPORT.md`, commit `030ec8bb`) explicitly
left unmeasured:

- exact group Average Precision with atomic confidence ties;
- the separable B-001 `coverage_minus`/`coverage_plus` envelope, per unit and
  macro-reduced;
- exact Spearman singleton certificates for unique and fixed-value-marginal
  optima, with explicit fail-closed `SPEARMAN_CERTIFICATE_UNAVAILABLE` for the
  general variable-value-marginal case.

It does **not** measure `G_eligible`/`G_defined`/`G_NA`, gate floors, or
publish `rho64`. It does not activate O-09 caps and does not make the
candidate ballot-ready.

## 2. What was independently re-verified after the handoff

Before running anything, the two new/untested files left uncommitted by
Codex were read directly (not skimmed) and cross-checked against the sealed
per-subgraph kernel's hardening lineage (bootstrap flags, `sys.orig_argv`
verification, tree hygiene, source/module provenance, atomic external
publish, determinism-across-replicas). No defect was found by reading; the
following was then confirmed by execution, not by inspection alone:

- 13/13 targeted runner tests, 56/56 combined with the sealed kernel,
  538/538 full canonical suite — all green, matching the state Codex had
  already reached.
- A `smoke`-profile run through the real isolated bootstrap (11 workloads,
  3 replicas): payload hash self-consistent (independently recomputed, not
  trusted from the declared field), `ballot_ready=false` as declared, the
  `spearman_variable_unavailable` fail-closed path confirmed through the
  real subprocess path (not just the direct unit test).
- The `full`-profile run (below) failed once on a clean-tree check, correctly:
  stray `__pycache__`/`.pyc` files existed under `ml_v3/` from an earlier,
  unrelated invocation. Removed (untracked build artifacts only, confirmed via
  `git status --ignored` before deletion) and the run repeated cleanly.

## 3. Full-profile results

27 workload/size combinations × 3 replicas = 78 isolated worker invocations.
Total summed wall time across all 78 invocations: **114.70 s**. All 26
workload rows report an identical scientific-result hash across their three
replicas; the runner aborts rather than publish on any divergence, and none
occurred.

| Workload | Size | Wall median | Wall max | Peak RSS |
|---|---:|---:|---:|---:|
| ap_distinct_diagonal | 16 / 64 / 128 | 0.003 / 0.012 / 0.041 s | ≤0.044 s | ~41-42 MB |
| ap_atomic_dense | 8 / 16 / 64 / 128 | 0.0004 - 0.094 s | ≤0.123 s | ~40-54 MB |
| coverage_unique | 16 / 64 / 128 | 0.026 / 0.618 / 2.024 s | ≤2.19 s | ~39-41 MB |
| coverage_ambiguous | 8 / 16 / 32 | 0.027 / 0.144 / 1.015 s | ≤1.33 s | ~40-42 MB |
| spearman_unique | 10 / 64 / 128 | 0.004 / 0.153 / 0.673 s | ≤1.38 s | ~40-41 MB |
| spearman_fixed_marginal | 10 / 64 / 128 | 0.020 / 1.059 / 3.562 s | ≤4.08 s | ~40-43 MB |
| spearman_ambiguous | 10 / 16 | 0.069 / 0.260 s | ≤0.26 s | ~41-42 MB |
| spearman_variable_unavailable | 10 / **64** | 0.119 / **28.726** s | **28.74 s** | ~43-44 MB |
| ap_macro_30, coverage_macro_30, cap_boundaries | 1 / 1 / 0 | ≤0.002 s | ≤0.002 s | ~42 MB |

**`spearman_variable_unavailable`/64 is the outlier**: ~250× the cost of its
own size-10 case and well above every other size-128 workload. It is the one
workload in the `full` profile that is structurally unable to take the cheap
"perfect matching" short-circuit (`gt_count != prediction_count` by
construction), so it exercises `_value_marginal_is_fixed` on both axes with a
near-maximal number of distinct value classes. This was anticipated from
reading the kernel before the run — a pre-run targeted probe measured the
*other* expensive-looking path (`spearman_ambiguous`/16, 0.26 s) but did not
separately probe this specific workload/size pair, so its cost was not
predicted with precision beforehand. The aggregate pre-run estimate (2-5
minutes total) held; the per-workload attribution within that estimate did
not.

Verified, not merely read from the declared field: `spearman_variable_unavailable`/64
produced `NOT_APPLICABLE` / `SPEARMAN_CERTIFICATE_UNAVAILABLE` / `value=None`
identically across all three replicas — the expensive path still fails
closed correctly under load.

## 4. Materialized artifact

```text
schema: aieq-v3-rev8-o09-group-candidate-benchmark-1

External location:
/Users/marco/aieq_data/motore_v3/rev8_evidence/rev8_o09_group_candidate_benchmark_full_v1.json

file_sha256 (independently recomputed after copy, matches the runner's
printed output and the in-repository record below):
74e4fc9d1ce47393d9654c5372a5cfc2b80009aa23d7777539d89f49757d7c2a

payload_sha256 (independently recomputed from the file's own contents,
excluding the field itself, using the project's canonical_bytes — not
trusted from the declared field):
70f0fb6658183936e76e04b5bd09d8ca983e0e5b17f89967dcce32724d29337b
```

## 5. Coverage declared by the artifact itself

```text
group_ap:                                     MEASURED
b001_group_coverage:                          MEASURED
spearman_unique:                              MEASURED
spearman_fixed_value_marginal:                MEASURED
spearman_ambiguity_detection:                 MEASURED
general_variable_value_marginal_spearman:     NOT_CERTIFIED
spearman_rho64_publication:                   NOT_MATERIALIZED
G_eligible_G_defined_G_NA_and_gate_floors:    NOT_EVALUATED
ballot_ready:                                 false
```

## 6. What this report does not claim

- Not a three-lens counter-check. A second independent reviewer has not yet
  examined this tranche the way the per-subgraph evidence and every prior
  O-02/O-03/O-18 and O-09 per-subgraph tranche were examined before this
  session took over.
- Not a claim that `spearman_variable_unavailable`'s cost is acceptable or
  unacceptable for any future cap — that is a scope/policy question for a
  ballot, not a benchmark finding.
- Not group-level `G_eligible`/`G_defined`/`G_NA`/gate-floor evidence, which
  remains unmeasured.
- Not an O-09 activation, a ballot, or a REV8 SPEC GO. All of those remain
  `NO`/unauthorized, as declared throughout.

## 7. Suggested next gate

Before any O-09 ballot: an independent second reviewer (ideally restoring the
three-lens pattern once available) should examine this tranche and, in
particular, decide whether `spearman_variable_unavailable`'s cost profile
needs its own structural cap distinct from the other Spearman paths, since it
is the one case in this run that did not scale like its neighbors.
