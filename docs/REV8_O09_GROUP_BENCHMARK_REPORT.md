# REV8 O-09 — Group-level benchmark evidence

**Status:** `EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE`

**REV8 SPEC GO:** `NO`

**Reviewers.** The runner and kernel were built and unit-tested by Codex (538
canonical tests, 56 targeted to this tranche) before it exhausted its budget
mid-run. Everything after the handoff passed through:

1. **Claude** — re-read the untested files, executed the smoke and full
   benchmarks through the real isolated bootstrap, recomputed the hashes
   independently, wrote §1-§5 of this report;
2. **Hermes** — external read-only counter-check. Round 1 was static only and
   carried three misattributed line citations (substance correct, positions
   wrong) and one health check run against the wrong worktree; those were
   found by re-verification and corrected. Round 2 re-executed the payload and
   file hash verification standalone and re-confirmed the disputed citations
   at their true positions;
3. **Claude, second pass** — semantic conformance of AP, B-001 coverage and
   Spearman against the signed contracts, recorded in §6 below.

**This is still not the three-lens counter-check used elsewhere in G1c/REV8**
— the lenses were not run by three mutually independent parties on the same
material. It is more than a single reviewer, and less than the established
process. No equivalence is claimed.

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

- 13/13 targeted runner tests and 56/56 combined with the sealed kernel —
  green, and re-confirmed since (they pass on a polluted tree too, because
  they perform no subprocess tree-hygiene check).
- **Correction to an earlier version of this report.** That version claimed
  "538/538 full canonical suite — all green", unqualified. That is withdrawn:
  it was not reproducible as written. Measured on a clean tree with the
  canonical interpreter, a plain `python -m unittest discover -s ml_v3/tests
  -t .` gave **535 pass, 3 fail** — `test_isolated_bootstrap_never_executes_hostile_sitecustomize`,
  `test_child_reports_loaded_and_hashed_source_provenance` and
  `test_loaded_modules_are_the_expected_python_sources`, all in
  `test_g1c_rev8_o09_candidate.py`, the **per-subgraph** tranche, not this one.
  The cause is in §2.1 and predates both commits of this tranche.
- **Current state, after the fix in `ac0dd38b`:** the suite is **538/538
  green** under `python -B -m unittest discover -s ml_v3/tests -t .` — the
  canonical invocation. It is **still 535/3 without `-B`**, and that is not a
  residual bug: the `unittest` parent writes bytecode while importing the test
  modules, so only `-B` keeps the tree archive-like for the isolated-bootstrap
  checks. `-B` is a requirement of this suite, consistent with the `-I -S -B`
  discipline the REV8 runners already enforce, and any future "all green"
  claim about this suite must name the invocation rather than assert the
  number alone.

### 2.1 Pre-existing order-dependent suite defect (not introduced here)

Established causally, not inferred:

- `test_g1a_t6_generators.py:194` spawns its determinism subprocesses as
  `[sys.executable, "-c", child, root]` without `-B`. Those children write
  `__pycache__` under `ml_v3/`, `ml_v3/contracts/`, `ml_v3/fixtures/` and
  `ml_v3/fixtures/g1/` **even when the parent runs with `-B`**, because the
  flag is not inherited across an explicit `sys.executable` invocation.
- The three O-09 per-subgraph tests above spawn the real isolated bootstrap,
  which refuses to run unless the source tree is archive-like. On the polluted
  tree it exits with
  `"requires an archive-like Python source tree before import"` — the runner
  behaving exactly as designed.
- Under `unittest discover`, `test_g1a_*` sorts before `test_g1c_*`, so the
  pollution always precedes the check and the failure is deterministic, not
  flaky.

Direct confirmation of each link: the three tests pass (`OK`, no cache left)
when run alone with `-B` on a clean tree; they fail when
`test_g1a_t6_generators` is run first, even with `-B` on the parent; and
`git diff 692cda55 HEAD -- ml_v3/tests ml_v3/benchmark ml_v3/contracts` is
empty, so no code in this tranche changed the outcome. The interaction dates
from `e8222688`, where the per-subgraph hygiene tests were added alongside a
`test_g1a_t6_generators.py` last modified on 2026-07-26.

Fixed in `ac0dd38b` as its own commit in the G1a tranche, not folded into
this evidence tranche: the two children now get `-B`, which is what the
test's own docstring already claimed ("two clean OS subprocesses") and what
the O-09 tests already do. The test still passes 13/13 and leaves no cache;
the G1a `SHA256SUMS` trust anchor still verifies at 49 entries, since this
file was never covered by it. That commit closes the pollution source but
does not remove the `-B` requirement on the parent, for the reason given
above.

**This fix has not been counter-checked by a second party.** It is a
one-line change to a sealed-tranche test, causally proven and verified in
both directions (test still green, suite now green, anchor intact), but the
program's standing rule is a counter-check before changes and no second
reviewer was available. It should be reviewed before G1a's seal is treated
as re-affirmed.
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

## 6. Semantic conformance against the signed contracts

The benchmark measures cost; this section records whether the kernel it
measures computes the metrics the contracts define. Each formula below was
compared line-by-line against its normative source, not against this report's
own prose.

**Average Precision — conformant to §13 of the Round 2.3 signed draft.**
Thresholds are the distinct confidences in descending order; the prefix is
`{prediction : confidence >= t}`, so equal-confidence predictions enter
atomically as §13 requires; the matching is *recomputed* per threshold via
`exact_maximum_cardinality`, not carried incrementally; `P_k = TP/(TP+FP)`,
`R_k = TP/N_GT`, `R_0 = 0`, `AP = Σ(R_k − R_{k−1})·P_k`. No trapezoid, no
precision envelope, no artificial endpoint — the three constructions §13:906
forbids. Both boundary cases hold: `N_GT>0, K=0 → AP=0` and `N_GT=0 → N/A`.
The implementation additionally rejects a non-monotone K1 prefix, which §13
does not require but cannot violate.

**B-001 coverage — conformant to `REV8_CANDIDATE_B001_COVERAGE_MICRO_AMEND_BALLOT.md`
(SIGNED — APPROVED), not to the Round 2.3 draft.** Worth recording explicitly:
B-001 has no definition and no ledger entry in the Round 2.3 signed draft —
"copertura" appears there once, as a gate the macro-mean is subordinate to,
with no formula. Its normative source is the separate signed micro-amend
ballot. Against that ballot: `covered(g,M)` is edge weight
`actionable_gt[g] AND actionable_prediction[p]`; `C_u(M) = Σcovered/|G_A(u)|`
with the denominator pooled over the unit's partitions;
`coverage_minus = min over M_u*`, `coverage_plus = max` (diagnostic);
`|G_A(u)|=0 → N/A / NO_ACTIONABLE_GT`; one rounding at the publication
boundary via `rn64`; reductions through the signed `mean64` hierarchy.

Two properties are worth naming because they are easy to mistake for
approximations and are not:

- The per-partition decomposition (solve each partition's envelope, then sum)
  is **exact**, not a bound. `M_u*` is a Cartesian product and the numerator
  of `C_u(M)` is separable, so the minimum of a sum of independent terms is
  the sum of the minima. This is what "separable" means in the kernel's
  docstring.
- The envelope ranges over the **full** `M*`, not over the larger set of
  maximum-cardinality matchings. `_edge_cost` builds the lexicographic cost
  `(−K2, K3, K4, −weight)`, so the coverage weight acts only as a tiebreak
  *inside* the scientific optimum. The complement trick used for the minimum
  (`ceiling − weight`) does not leak into the reported value, which is
  recomputed from the original weights.

**Spearman — conformant to §11.4 and DECISIONE_R23_02.** Midranks are
`((start+1)+end)/2` over tie runs; rho is carried as sign plus the exact
rational triple `(cov², var_x, var_y)` and never materialized as a root;
`rho_equal` implements exactly `cov₁²·var_x₂·var_y₂ == cov₂²·var_x₁·var_y₁`
with a sign precondition, which is R23_02 verbatim. All three §11.4 reason
codes are present with the right triggers (`support < 10` →
`INSUFFICIENT_MATCHED_SUPPORT`; zero variance → `SPEARMAN_UNDEFINED`;
lower/upper envelope disagree → `PAIRING_AMBIGUOUS`). The fixed-marginal
certificate is sound: when matched value multiplicities are proved invariant
across `M*`, both variances and both means are invariant, leaving only `Σxy`,
which is separable — so equal lower and upper envelopes prove rho is a
singleton rather than merely suggesting it.

**One deviation — CLOSED as of `11365cdb`, recorded here for the trail.**
`SPEARMAN_CERTIFICATE_UNAVAILABLE` was a fourth reason code §11.4 did not
enumerate. It is returned when the kernel can prove neither singleton nor
non-singleton. Forcing that case into `PAIRING_AMBIGUOUS` would assert "rho
is not a singleton" — a claim the kernel has not established — so a distinct
code is the more honest outcome, and both N/A results block PASS identically,
so there was never a gate-safety difference. The gap was normative, not
scientific: the authority did not name an outcome the implementation already
produced conservatively.

It was closed by a signed micro-amend ballot (`e36d2422`,
`docs/REV8_CANDIDATE_S6_SPEARMAN_REASON_CODE_MICRO_AMEND_BALLOT.md`) and the
document-only §11.4 patch it authorized (`11365cdb`). Two things about that
closure must not be lost:

- **It was signed without the three-lens counter-check** that every other
  REV8 decision went through. §6 of the ballot records that derogation
  explicitly and states the ballot must be reopened if a later review rejects
  or amends the text. A dedicated review prompt exists and was not run before
  signing.
- **It closes the naming, not the science.** The general
  variable-value-marginal case remains **NOT CERTIFIED**, and the
  `GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED` limitation still
  stands. Signing S6 did not make the candidate ballot-ready and did not
  activate anything.

As originally stated, before the closure above: this was an activation
prerequisite — before any O-09 ballot, either §11.4 had to enumerate this
code or the implementation had to close the general variable-value-marginal
case. The first of those two has now happened; the second has not.

**Numeric authority (O-18) verified present, not assumed.** `sum_pairwise64`
matches the §2.2 pinned form (`mid = floor(N/2)`, recursive split, rounding at
every addition); `mean64` sums pairwise and divides once; neither `numpy.sum`,
`math.fsum`, Kahan nor fast-math appears on any published quantity in the
kernel or runner. `exact_arith_v2.exact()` keeps the `int` and `float` paths
separate and is lossless above 2^53 — verified by execution
(`exact(2**60+1)`). An earlier internal note describing that as an open defect
was stale: it was fixed in `33a0957b`, the commit that materialized the O-18
authority.

## 7. Closure pass — historical parity, exit codes, and `ac0dd38b` equivalence

A prose self-audit of this tranche (by Claude) was itself reviewed by Codex,
who identified four concrete gaps rather than accepting the audit's "GO" at
face value: the suite's exit code had been read through a pipe into `tail`
and never captured directly; "0 mismatch" in the earlier self-audit referred
only to determinism *within* a freshly generated run, not to parity against
the already-published historical evidence; the requested S1-S8 and
three-lens tables had not been delivered in the requested format; and
`ac0dd38b`'s claim that bytecode caching does not affect WAV output had been
argued from unchanged source rather than executed. This section closes all
four with direct execution, not renewed argument. Codex's own contribution
here was a methodology review of the self-audit's completeness — it did not
itself re-execute the benchmark; that gap is why an independent Kilo pass
(§9) remains open.

**Suite exit code, captured directly.** `python -B -m unittest discover -s
ml_v3/tests -t .`, redirected to files with `set -o pipefail` in effect (no
pipe into `tail` masking the real status): `SUITE_EXIT=0`, 538/538.

**Historical↔fresh parity, cell by cell — not just internal determinism.**
The full-profile run in §3 was generated at `692cda55`. A second, independent
full-profile run was executed fresh at `5451a295` through the same isolated
bootstrap. Before treating the fresh run as a reproduction of the historical
one, the code between the two commits was diffed, not assumed identical:

```text
git diff --exit-code 692cda55 5451a295 -- \
  ml_v3/benchmark/rev8_o09_candidate.py \
  ml_v3/benchmark/rev8_o09_group_candidate.py \
  ml_v3/benchmark/rev8_o09_group_isolated_bootstrap.py \
  ml_v3/benchmark/run_rev8_o09_group_candidate.py \
  ml_v3/contracts/numeric_authority_v2.py \
  ml_v3/contracts/normalize_v2.py \
  ml_v3/contracts/exact_arith_v2.py \
  ml_v3/contracts/canonical.py \
  ml_v3/contracts/constants.py \
  ml_v3/contracts/grid.py
DIFF_EXIT=0
```

The full diff between the two commits touches exactly two files — this
report and the `ac0dd38b` test fix — confirming no O-09 scientific path
changed. With that established, the fresh run's 26 workload/size cells were
compared against the historical raw's `scientific_result_sha256` per cell:
**0 mismatches across 26/26 cells.** This is a reproduction of the already-
published evidence from an independent execution, not merely a
self-consistent new one.

**`ac0dd38b` WAV pre/post equivalence — executed, not argued from unchanged
source.** `render_signals.py` is untouched by `ac0dd38b` (`git diff --exit-code
ac0dd38b^ ac0dd38b -- ml_v3/fixtures/g1/render_signals.py` is empty), which
proves the renderer's *logic* is identical — but the test's interpreter
invocation changed (`-B` was added), and unchanged source alone does not
prove unchanged *behavior* under a different flag without executing it. Both
invocations were run directly: the pre-fix child command
(`sys.executable -c <child>`) and the post-fix command
(`sys.executable -B -c <child>`), each rendering the full 37-asset tree to a
fresh temp directory. Result: identical file list, 0 byte-level mismatches
across all 37 files, and identical aggregate tree hash on both sides
(`cfe15633e8609c6379bc44a713acb71cf0d4f09d24b11c92b78949b902cb5edf`). The
`SHA256SUMS` manifest coverage claim in the `ac0dd38b` commit message was
also re-checked directly against the manifest (`grep -c
test_g1a_t6_generators ml_v3/fixtures/g1/SHA256SUMS` → `0`), not taken from
the commit message on trust.

**S1–S8, formal table, with epistemic tier separated from verdict.** None of
S1–S7 have been independently reproduced by a second party; S8 and the
parity/exit-code/WAV checks above are direct executions. Both are recorded,
not blended.

| # | Claim | Evidence | Tier | Verdict |
|---|---|---|---|---|
| S1 | AP: atomic ties, per-threshold recompute, no trapezoid/envelope/endpoint, both boundary cases | draft §13:878-916 vs `evaluate_group_ap` :556-687 | SELF_VERIFIED | CONFERMATO |
| S2 | Coverage per-partition decomposition is exact, not a bound | ballot §2 vs `evaluate_group_coverage` :818-977; separability derivation | SELF_VERIFIED | CONFERMATO |
| S3 | Coverage envelope ranges over full `M*`, not the max-cardinality superset | `_edge_cost`/`_extreme_weight` :778-816 | SELF_VERIFIED | CONFERMATO |
| S4 | `rho_equal` implements R23_02 verbatim | draft §11.5:790-804 vs `rho_equal` :369-380 | SELF_VERIFIED | CONFERMATO |
| S5 | Fixed-marginal certificate is sufficient, not merely indicative | draft §11.4 vs :1042-1310; monotonicity-of-rho-in-Σxy derivation | SELF_VERIFIED | CONFERMATO |
| S6 | Fourth reason code not enumerated in §11.4 | draft §11.4:782-785 (3 codes at the time) vs `GroupReason`:131 | EXECUTION+STATIC_VERIFIED | **CONFERMATO — amend since signed (`e36d2422`) and applied (`11365cdb`); §11.4 now enumerates four** |
| S7 | No `numpy.sum`/`math.fsum`/Kahan/fast-math on published quantities | grep, re-run fresh, 0 hits | SELF_VERIFIED | CONFERMATO |
| S8 | `exact()` lossless above 2^53 | `exact_arith_v2.py`:56-74, re-executed | EXECUTION_VERIFIED | CONFERMATO |

**Three lenses, formal table. Lens B was AMEND at the time of this pass** — a
lens verdict must reflect its worst finding, and S6 was then a real, unclosed
normative gap, not a footnote alongside an otherwise-clean row. It has since
been closed by ballot (`e36d2422`) and patch (`11365cdb`); the verdict below
is left as recorded rather than retroactively upgraded, with the resolution
noted inline.

*Lens A — Optimizer/Numeric: CLEAN.* `sum_pairwise64` pinned recursive form
(`numeric_authority_v2.py`:104-127); `mean64` sums pairwise then divides once,
rejects unequal values at equal order-keys (:146-182); `exact()` keeps
int/float paths separate, lossless above 2^53, execution-verified; no
unpinned float reduction on published quantities in kernel or runner; declared
complexity matches the code.

*Lens B — Metrics/Statistics: AMEND (as recorded; the amend has since been
made).* S1–S5 and S7 confirmed. S6 was an open, named deviation:
`SPEARMAN_CERTIFICATE_UNAVAILABLE` is real, fail-closed, and does not produce
a false PASS — but it was not authorized by §11.4 as written, and that was a
normative gap, not a scientific defect. Resolved by `e36d2422`/`11365cdb`,
which enumerate it. **Lens B does not thereby become CLEAN retroactively**:
the amend was signed without the three-lens counter-check, so the lens whose
finding it was has still not been independently re-run over the resolution.

*Lens C — Semantics/Security/Governance: CLEAN.* `authority_status`/
`ballot_ready` read-only, never reassigned by the runner (:70, :818); atomic
publish with pre-`os.replace` provenance re-check (:822-840); gate-platform
enforcement is fail-closed on interpreter drift (`metrology_lock.py`:958-970,
runner:489-490); isolation hardening intact (bootstrap:47-80); `ac0dd38b`'s
pollution source is the sole one in `ml_v3/` (exhaustive grep), and its fix is
now execution-verified equivalent, not merely argued; historical↔fresh parity
holds 26/26.

**Corrected verdict.** Not a single unqualified "GO". Two separate questions,
two separate answers:

```text
O-09 group-level evidence reliability  = GO (the GO-CON-FIX condition was the
                                          S6 normative gap; it has since been
                                          signed and applied — see below)
O-09 activation / REV8 SPEC GO          = NO-GO
  (unchanged: no cap, no activation, REV7 and the REV8 candidate untouched)
Independent runtime reviewer            = Hermes (round 2, execution-verified
                                           on hashes)
Self-audit execution (this closure)     = Claude, execution-verified,
                                           not independent
Full independent three-lens pass        = not yet run
```

**S6 status update.** The remediation text below was proposed by Codex during
this review cycle. It was subsequently **signed** by the authority
(`e36d2422`) and **applied** to §11.4 (`11365cdb`), so it is now part of the
candidate normative authority rather than a proposal. Two qualifications
carry forward and must not be dropped:

- the signature was given **without** the three-lens counter-check every
  other REV8 decision received; §6 of the ballot records that derogation and
  requires the ballot to be reopened if a later review rejects the text;
- it closes the **naming** only. The general variable-value-marginal case is
  still **NOT CERTIFIED** and the corresponding limitation still stands.

Signed text, as applied to §11.4:

```text
Nel caso Spearman in cui la procedura esatta richiesta non disponga di un
certificato sufficiente a dimostrare l'unicità del valore su M*, il risultato
è N/A con reason code SPEARMAN_CERTIFICATE_UNAVAILABLE.

Questo esito:
- non è PASS;
- non è zero;
- non è una failure runtime;
- non può essere convertito in PAIRING_AMBIGUOUS senza prova di non-singleton;
- impedisce il PASS di qualunque gate che richieda una Spearman definita.
```

## 8. What this report does not claim

- Not a three-lens counter-check. This tranche has had two reviewers, a
  semantic conformance pass, and a methodology review, not three mutually
  independent lenses applied to the same material the way the per-subgraph
  evidence and every prior O-02/O-03/O-18 tranche were examined. §7's
  three-lens table exists in the requested *format* but was produced by the
  same party (Claude) that wrote the code and the rest of the report — it is
  a structured self-audit, not independent lens diversity. Codex's role in
  producing it was to find gaps in the self-audit's rigor, not to re-derive
  the findings independently.
- Not a claim that the surfaces §6 declares conformant have been proved
  correct by test. §6 is a reading of the implementation against its contract;
  the golden and mutation artifacts the ledger requires
  (`AP golden`, `exact singleton algorithm + full fixture`,
  `sum_pairwise64`/N64 artifacts) remain the separate materialization
  conditions listed in §15 of the signed draft.
- Not a claim that `spearman_variable_unavailable`'s cost is acceptable or
  unacceptable for any future cap — that is a scope/policy question for a
  ballot, not a benchmark finding.
- Not group-level `G_eligible`/`G_defined`/`G_NA`/gate-floor evidence, which
  remains unmeasured.
- Not an O-09 activation, a ballot, or a REV8 SPEC GO. All of those remain
  `NO`/unauthorized, as declared throughout.

## 9. Suggested next gate

Before any O-09 ballot, three items, in order of how much they constrain the
ballot's scope:

1. ~~**Reason-code enumeration.**~~ **DONE** — §11.4 now enumerates
   `SPEARMAN_CERTIFICATE_UNAVAILABLE` (ballot `e36d2422`, patch `11365cdb`).
   Two residual obligations attach to it and are not discharged: the ballot
   was signed **without** the three-lens counter-check and must be reopened if
   a later review rejects the text; and a targeted counter-check on the
   patched draft's new SHA-256
   (`be8658203e26fae3bc36d020733b6b7ed773d24ed0e78675330f08b459ff56cc`) is
   required by the ballot itself and has **not** been performed.
2. **Cost policy for `spearman_variable_unavailable`.** At size 64 it costs
   ~250× its own size-10 case and more than every size-128 workload measured
   here. Whether that warrants a structural cap distinct from the other
   Spearman paths is a scope decision, not a benchmark finding — but it is the
   one case in this run that did not scale like its neighbors, so it should be
   decided deliberately rather than inherited.
3. **The unmeasured surfaces.** `G_eligible`/`G_defined`/`G_NA` and gate
   floors remain `NOT_EVALUATED`; §12 of the signed draft defines them and
   this tranche does not touch them.

Restoring the three-lens pattern for this tranche, once a third independent
party is available, remains the cleanest way to close it.
