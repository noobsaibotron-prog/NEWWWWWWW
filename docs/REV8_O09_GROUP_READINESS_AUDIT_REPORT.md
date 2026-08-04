# REV8 O-09 — Group readiness audit report

**Date:** 2026-08-04

**Verdict:** `READINESS_BLOCKED`

```text
O09_GROUP_CAP_BALLOT_READY = NO
SPEARMAN_PUBLICATION_READY = NO
O13_SUPPORT_GATE_READY     = NO
```

No cap was activated. No authority, runtime, training or plugin path was
changed. `REV8 SPEC GO` remains `NO`.

## 1. Audit identity

```text
Branch:
feature/motore-v3-rev8-spec-go

Preparation protocol commit:
90a1123c

Preparation protocol SHA-256:
22b106b153d5132a44d726fbc3f84618b46116e7038f1679ce40859e33cef930

Effective audited HEAD:
5efa8aa34a7270a784e5e41beff9950098ab2395

Effective audited tree:
0ae4c8b177311b393a94553c5167cd2987573b3e
```

The effective HEAD differs from the preparation base because three isolated
fail-closed fixes were completed before execution:

```text
2ab1a476  solve-level TimeoutError
362acda3  failures raised inside _group_preflight
5efa8aa3  nullable-preflight evidence serialization in the runner
```

The audit declared and used the new source hashes consistently.

## 2. Snapshot and authority verification

| Artifact | SHA-256 |
|---|---|
| Living REV7 contract | `310d538647d71840c6dd8124f1e24281b758bb6dd3e4776aac4b34e5f658a5b0` |
| R23C immutable target | `684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb` |
| S6 signed transfer ballot | `8dab8b7ad0fb25f5263358dd26600d201a036210d808840d6c8e1d07051649c0` |
| S6 post-signature CLEAN report | `b148e6340dd3c9b46ca0f6787209cc6d2df00a6512e34c5bce164b81b1349c7a` |
| A1 signed ballot | `bf31ed0e1bbcfa001114808e569a5155427b54eef3b9dd1cd0d80fcd1c184d75` |
| A1 enforcement report | `f0a21177035d5161e89d702d06e8ddd0dbefbedc508cb545991a39da9175e565` |

Audited subjects:

| Subject | SHA-256 |
|---|---|
| Group candidate | `64aba0174e044121a012f013d4c5883ec0a1bb89919e931660eb82dfb64d7494` |
| Group runner | `650e8021ebfc0095601b0515d7b463266ba020fa7de99df022bd93eb2b4be7b2` |
| Isolated bootstrap | `cb3c3ccf07ec6aa1c9301783ea5d0a62038d385583a18fb99fda4a0fd34bbbba` |
| Candidate tests | `237c4788f9ab2e409524ab0fc9947669f07eeace9c2f78923585864c0b33c5a7` |
| Runner tests | `974d1608e11eb7164c64b6772c38a294dacef77d4d2d825c83f3da5e1b8ff13c` |

Initial worktree was clean. Protected `Source`, `CMakeLists.txt`, `Resources`
and `ml_v2` had zero diff. The live REV7 dispatcher/package surface does not
import or export the group candidate.

## 3. Independent lens results

### L1 — Metrics and statistics: `BLOCK`

The examined AP, Coverage and Spearman mathematics were consistent with their
current candidate contract:

- AP uses canonical binary64 confidence thresholds and atomic ties;
- K1 is recomputed at every local prefix;
- Coverage B-001 produces an exact lower/upper envelope;
- group and macro reductions preserve the pinned hierarchy;
- Spearman uses pooled pairs, exact midranks and the support floor;
- the general variable-marginal case is N/A, never zero or PASS.

The lens blocks on missing durable protocol evidence: support 11 and mutation
proof against `N/A -> zero/PASS` are absent.

### L2 — Optimizer, cap and fail-closed: `BLOCK`

The 17 arithmetic predicates passed direct under/on/over checks. The committed
matrix is incomplete and the complete pre-solve phase remains fail-open before
the guarded helper. The group kernel also consumes the historical provisional
A1 probe instead of the signed active A1 surface.

The exact solver has a deterministic structural ceiling for each candidate
family; no wall-clock threshold is proposed as scientific authority.

### L3 — Authority, security and provenance: `BLOCK`

The authority hashes, REV7 isolation, package reachability and
`ballot_ready=false` guardrails are clean. H-G03 is a blocking authority/status
mismatch. Historical S6 wording is stale but quarantined.

The old full evidence was stale. A new full evidence file was generated on the
effective HEAD during this audit and is recorded below.

## 4. Test and evidence record

```text
Full repository suite on effective snapshot:
563/563 PASS

Group candidate + runner suite during independent lenses:
61/61 PASS

Direct cap predicate countercheck:
17/17 arithmetic under/on/over PASS
```

Current external full evidence:

```text
Path:
/Users/marco/aieq_data/motore_v3/rev8_evidence/
rev8_o09_group_readiness_5efa8aa3_full_v1.json

File SHA-256:
c8aa1bc6b115c06d02247c94176ed870326491a7c250d6c90d89ff7d7f11c42b

Payload SHA-256:
9b5233fa41b0656a8ee5429e41af86bfe505436e0e3b5f5a5a0ccbc9aa79c741

Profile: full
Repeat: 3
Workloads: 26
Authority status: EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE
```

All subject hashes in this evidence match the audited snapshot. The output is
external to the repository and was atomically published only after the final
hygiene check.

## 5. Blocking findings

| ID | Severity | Finding |
|---|---|---|
| F-01 | HIGH | Canonical sorting and other preparation before `_guarded_group_preflight` can still propagate fatal exceptions. |
| F-02 | HIGH | Group preflight consumes the provisional probe, not the active A1 enforcement surface. |
| F-03 | HIGH | The required durable 17-cap under/on/over/no-solve matrix is incomplete. |
| F-04 | MEDIUM | Required support-11 and N/A mutation evidence is missing. |
| F-05 | MEDIUM | Historical group report contains stale S6 “current state” wording. |

Open but separately scoped:

| Area | State |
|---|---|
| General variable-marginal Spearman | not certified |
| Spearman `rho64` | not materialized |
| O-13 `G_eligible/G_defined/G_NA`, floors and p95 | not evaluated |

Detailed repro and disposition are in
`REV8_O09_GROUP_READINESS_FINDINGS.md`.

## 6. Verdict rationale

The protocol requires `READINESS_BLOCKED` if a failure can escape translation,
if active authority is ambiguous, or if a cap lacks deterministic durable
evidence. This snapshot has all three conditions.

The refreshed full benchmark and the mathematically correct cap predicates do
not compensate for missing fail-closed coverage or authority alignment.

## 7. Only next permitted action

One isolated remediation tranche may:

1. place the complete pre-solve preparation for AP/Coverage/Spearman under the
   same fail-closed taxonomy without swallowing malformed caller input;
2. make group evaluation consume the signed active A1 preflight surface and
   propagate its authority/status without duplicating the policy;
3. add the complete 17-row durable `under/on/over` and solver-not-called
   matrix, including combined A1/group ordering;
4. add support 11 and mutation tests that kill N/A-to-zero/PASS conversions;
5. add a historical/non-authoritative banner to the stale S6 sections.

After that tranche:

```text
full suite
-> fresh external evidence
-> new immutable snapshot
-> repeat all three audit lenses
```

Not authorized by this report:

```text
cap activation
group ballot signature
O-13 closure
runtime/training/plugin work
push
REV8 SPEC GO
```

