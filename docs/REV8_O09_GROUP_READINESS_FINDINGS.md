# REV8 O-09 — Group readiness findings

**Audit snapshot:** `5efa8aa34a7270a784e5e41beff9950098ab2395`

**Status:** `READINESS_BLOCKED`

## F-01 — HIGH — The complete pre-solve phase is not fail-closed

Commits `2ab1a476`, `362acda3` and `5efa8aa3` correctly close:

- `TimeoutError` in the solve path;
- failures raised inside `_group_preflight`;
- evidence serialization when the failed preflight has no probe.

The public group entrypoints still perform work before the guarded call:

- canonical sorting through `normalized_canonical_bytes`;
- AP threshold and prefix-incidence derivation;
- Coverage unit grouping;
- Spearman marginal-class derivation.

Read-only injection on valid inputs produced:

```text
normalized_canonical_bytes -> RuntimeError

AP       -> propagated RuntimeError
Coverage -> propagated RuntimeError
Spearman -> propagated RuntimeError
```

The positive control, injecting the same runtime failure inside
`provisional_preflight_probe`, produced:

```text
REJECTED / SOLVER_RUNTIME_FAILURE / value=None
```

Therefore the guarded function boundary is closed, but the full pre-solve
phase is not. This is a direct stop-rule blocker.

## F-02 — HIGH — The group kernel does not consume the active A1 surface

The group candidate calls `provisional_preflight_probe` directly. The probe
materializes the current numeric ceilings, but reports:

```text
PROVISIONAL_GROUP_DIAGNOSTIC_ONLY
```

The signed A1 surface reports:

```text
A1_ACTIVE_ENFORCEMENT
```

Observed on the same clean graph:

```text
evaluate_a1_preflight_fail_closed -> EVALUATED / A1_ACTIVE_ENFORCEMENT
evaluate_group_ap                 -> CERTIFIED / PROVISIONAL_GROUP_DIAGNOSTIC_ONLY
```

The values are currently rejected before solve, but authority, provenance and
failure translation are duplicated instead of consuming the active A1
entrypoint. H-G03 is confirmed.

## F-03 — HIGH — Durable 17-cap evidence is incomplete

All 17 comparison predicates behaved correctly in an ad-hoc direct
`cap-1/cap/cap+1` countercheck. The committed suite does not satisfy the
protocol matrix:

```text
cap-1 fixtures:                    0/17
cap and cap+1 both present:        8/17
solver-not-called at cap+1:        0/17
```

Nine caps have no dedicated committed boundary test. See
`REV8_O09_GROUP_CAP_MATRIX.md`.

## F-04 — MEDIUM — Required metric falsification evidence is incomplete

The live metric behavior is correct in the examined cases, but the protocol
requires durable support `9/10/11` and a mutation test killing every
`N/A -> zero/PASS` conversion.

Current suite gaps:

- support 11 is not a committed boundary fixture;
- N/A tests often assert status and reason but not `value is None`;
- a mutation returning a well-formed zero value with the same N/A status and
  reason can survive the relevant tests.

Ad-hoc behavior was:

```text
Spearman 9  -> N/A / INSUFFICIENT_MATCHED_SUPPORT / value=None
Spearman 10 -> CERTIFIED / UNIQUE_OPTIMUM
Spearman 11 -> CERTIFIED / UNIQUE_OPTIMUM
```

## F-05 — MEDIUM — Historical S6 wording remains misleading

`REV8_O09_GROUP_BENCHMARK_REPORT.md` contains a historical “Current state” in
which S6 is not effective and still needs signature/transfer. S6 is now
effective by signed transfer plus post-signature CLEAN report.

The audit protocol correctly quarantines those sections as historical and
non-authoritative, so this does not corrupt the authority chain used by this
audit. A durable banner or erratum is still required to prevent isolated
misreading.

## F-06 — OPEN, correctly outside this closure — Spearman and O-13

The following declared limitations remain true:

```text
GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED
SPEARMAN_RHO64_PUBLICATION_NOT_MATERIALIZED
G_ELIGIBLE_G_DEFINED_G_NA_AND_GATE_FLOORS_NOT_EVALUATED
```

The Spearman candidate has a deterministic structural solve ceiling:

```text
3 * partitions + 2 * marginal value classes <= 560
```

This falsifies the need for an additional wall-clock cap merely to establish
boundedness. It does not materialize `rho64`, certify the general case or close
the O-13 support/gate policy.

## F-07 — RESOLVED DURING AUDIT — Current full evidence

The pre-audit evidence belonged to commit `692cda55` and could not cover the
three later fail-closed fixes. A fresh isolated full run completed after the
three lenses stopped writing Python caches:

```text
commit: 5efa8aa34a7270a784e5e41beff9950098ab2395
profile: full
repeat: 3
workloads: 26
authority_status: EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE
external file SHA-256:
c8aa1bc6b115c06d02247c94176ed870326491a7c250d6c90d89ff7d7f11c42b
payload SHA-256:
9b5233fa41b0656a8ee5429e41af86bfe505436e0e3b5f5a5a0ccbc9aa79c741
```

`spearman_variable_unavailable/64` remained deterministically
`NOT_APPLICABLE / SPEARMAN_CERTIFICATE_UNAVAILABLE` across all three runs,
with wall times between approximately 22.10 and 22.80 seconds.

The refreshed evidence resolves the snapshot mismatch. It does not override
F-01 through F-06.

## Disposition of protocol hypotheses

| Hypothesis | Disposition |
|---|---|
| H-G01 | CLOSED by `2ab1a476`; timeout taxonomy aligned. |
| H-G02 | CLOSED only inside `_group_preflight`; broader pre-solve failure path BLOCKED by F-01. |
| H-G03 | CONFIRMED; active A1 surface not consumed. |
| H-G04 | CONFIRMED; durable matrix incomplete. |
| H-G05 | No current runtime ordering bug; durable exact-order/no-solve fixture missing. |
| H-G06 | Additional cap not needed for mathematical boundedness; cost policy remains. |
| H-G07 | OPEN; `rho64` not materialized. |
| H-G08 | N/A/general-certification distinction is correct; publication still not ready. |
| H-G09 | CONFIRMED and assigned to O-13. |
| H-G10 | CONFIRMED historical-document debt. |
| H-G11 | Not realized on this snapshot; future hardening only. |
| H-G12 | Operational order deterministic, but not pinned through active A1 authority. |

