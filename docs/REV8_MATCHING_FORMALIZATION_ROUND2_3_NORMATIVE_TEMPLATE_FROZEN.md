# REV8 — MATCHING FORMALIZATION ROUND 2.3 — NORMATIVE TEMPLATE — FROZEN

**Stato:** TEMPLATE — non è specifica normativa — non contiene decisioni al posto dell'autorità — ogni sezione con placeholder tracciato a O-XX
**Base:** e2bee113 immutabile document-only — REV8 SPEC GO = NO fino a ballot firmato + Round 2.3 normativo + red-team + patch doc-only + counter-check
**Precedenza:** Decision Register Final Patched + Addendum Corretto + correzioni 2 ULP e A1 infeasibility non-fatal + K5_RESERVED
**Correzioni applicate:** K5_RESERVED = unused in REV8, O-02 non dipende O-01, O-08a implementation-neutral, parsing binary64 generale sotto O-18

## 1. Authority / Revision

[TRANSFER/TEST O-19 — revision authority decided; transfer/test pending] — active evaluator authority: dispatcher / activation manifest, historical constants -> provenance
[WORDING O-17 — problem_type_id omission rationale decided; normative wording pending] — problem_type_id inclusion rationale in semantic_payload vs decision_key

```
CONTRACT_REVISION = [TRANSFER/TEST O-19 — revision authority decided; transfer/test pending]
ACTIVE_EVALUATOR_AUTHORITY = [TRANSFER/TEST O-19 — revision authority decided; transfer/test pending]
```

## 2. Canonical Numeric Representation — generale, non solo O-01

[ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending] — ownership exact rational vs sum_pairwise64
- Optimizer: exact rational arithmetic — [ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending]
- Published metrics binary64: sum_pairwise64 — [ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending]
- Tranche §14.1 — [ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending]
- Golden — [ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending]

- Binary64 parsing: round-to-nearest ties-to-even, no excess precision, no fast-math — generale sotto O-18/N64, non solo O-01 — [ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending]
- N64 bits to exact rational conversion: [ARTIFACT/TEST O-18 — exact rational vs pairwise64 and general N64 authority decided; artifact/test pending]

Bit-exact fixtures — 2 ULP distance:
```
0.0 = 0x0000000000000000
1.0 = 0x3FF0000000000000
0.2 = 0x3FC999999999999A
1.2 = 0x3FF3333333333333
N64(1.2)-N64(1.0) = 0x3FC9999999999998 distance 2 ULP = 2 increments total order positive binary64, spacing 2^-55 diff 2^-54
0.25 = 0x3FD0000000000000
1.25 = 0x3FF4000000000000
```

Formule:
```
1-IoU = (U-I)/U — FORMALLY PROVED — no old U/(U-I)
```
d=0.2 REJECTED, d=0.25 valid — STRONGLY SUPPORTED — C7-A numeric sensitivity distinta da C6-B onset/offset ambiguity

## 3. Temporal Geometry — C7 — 2 ULP — C7-A vs C6-B separati

[DECISION O-01 REQUIRED] — temporal authority — C7-A numeric representational sensitivity
```
Temporal authority = [DECISION O-01 REQUIRED: A) feature frame, B) tick sample/hop-derived exact PREFERRED, C) sample index, D) binary64 exact severe]
Raw annotation time -> canonical source-sample tick -> deterministic mapping frontend tick -> matching — [DECISION O-01 REQUIRED]
Time domain: source file vs resampled frontend — [DECISION O-01 REQUIRED]
Equivalence 44.1/48/96 kHz — [DECISION O-01 REQUIRED]
Segment boundaries inclusive/semi-open — [DECISION O-01 REQUIRED]
Interval representation inclusive/semi-open — [DECISION O-01 REQUIRED]
Causal frontend alignment — [DECISION O-01 REQUIRED]

FORMALLY SHOWN: N64(0.2)=0x3FC999999999999A, N64(1.2)-N64(1.0)=0x3FC9999999999998 distance 2 ULP
THREAT MODEL TO BE DEMONSTRATED: candidate-gamable micro-perturbation
Fixture normative: read N64 bits, convert to exact rationals, calculate differences with rational arithmetic, compare — no x87/FMA/runtime subtraction
```

[DECISION O-11 REQUIRED] — C6-B onset/offset metric ambiguity — GT[0,1] P_A[-0.25,1] P_B[0,1.25] IoU 0.8 K3 0.25 — envelope vs N/A/FAIL

## 4. Frequency Geometry — C1-B

[ARTIFACT O-02 — golden boundary authority decided; artifact generation pending] — boundary artifact bit-exact — DECIDED_PENDING_ARTIFACT single authority golden
```
Authority: boundary table golden — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
Explanation: argmin |log2(raw)-log2(center_j)| tie->lower — mathematical explanation only

Boundary artifact format — [ARTIFACT O-02 — bit-exact format decided; artifact generation pending]:
boundary_binary64_bits: ["0x..." lowercase hex uint64 bit pattern]
order: ascending index
encoding: UTF-8 LF, canonical JSON keys sorted, no whitespace extra, no decimal floats for center/boundary
center_hash = SHA256(canonical center bit list)
boundary_hash = SHA256(canonical boundary bit list)
artifact_hash = SHA256(canonical artifact excluding artifact_hash) includes center_hash and boundary_hash
generator report non-normative: index, input center bits, output boundary bits, precision bits, rounding mode, refinement outcome
Generator procedure: read center bits exact -> rationals -> sqrt(center_i*center_{i+1}) interval arithmetic -> increase precision until interval rounds to same binary64 -> emit bits — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending] — correctly-rounded via interval refinement

Extremes rule:
x non-finite -> FAIL before projection — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
center_hz non-finite or <=0 -> FAIL — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
width_octaves non-finite or <0 -> FAIL (v2 admits >=0) — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
raw_lo > raw_hi -> FAIL — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
finite raw outside grid domain -> saturate band 0 or 119 not FAIL — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
non-finite raw -> FAIL — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
band 0 if x <= boundary[0] — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
band k if boundary[k-1] < x <= boundary[k] 1<=k<=118 — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
band 119 if x > boundary[118] — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
Domain: min_hz 20 max_hz 20000 — [ARTIFACT O-02 — golden boundary authority decided; artifact generation pending]
```

### Width Bound + Aliasing
[DECISION O-03 REQUIRED]
```
Safety upper bound W_MAX = 2*log2(20000/20) ≈19.93 — [DECISION O-03 REQUIRED]
Semantic width bound: [DECISION O-03 REQUIRED: safety only vs safety+semantic per class]
Aliasing: (center1,width1)!=(center2,width2) but B1=B2 — separation L0 raw / L2 identity / L3 GT validity / L4 matching geometry — [DECISION O-03 REQUIRED]
Metamorphic H/S: B(x)=B(y) => Eligibility(x,z)=Eligibility(y,z) and GeometryCost(x,z)=GeometryCost(y,z) ∀z — PROPOSED REQUIREMENT — [DECISION O-03 REQUIRED]
```

## 5. GT Structural Validity — C4

[DECISION O-15 REQUIRED]
```
Duplicate semantics:
DUPLICATE_GT -> FAIL GT validity reason duplicate — [DECISION O-15 REQUIRED PROPOSED]
CONTRADICTORY_GT -> FAIL GT validity reason contradictory — [DECISION O-15 REQUIRED PROPOSED]

Structural target per class: [DECISION O-15 REQUIRED]
```

## 6. Eligible Edge Predicate — O-04a critical

[DECISION O-04a REQUIRED]
```
Hard eligibility = minimal semantic incompatibility — [DECISION O-04a REQUIRED recommendation C]
- different evaluation_unit_key -> ineligible
- different problem_type -> ineligible
- trusted validity mask invalid -> ineligible
- direction incompatibility where semantically necessary -> ineligible
- temporal relation absence only if normatively justified impossible

Graded similarity in K2-K5 — [DECISION O-04a REQUIRED]
No arbitrary hard thresholds IoU or center distance without scientific justification + exact arithmetic + fixtures just inside / exact on border / just outside + analysis effect on K1/TP/FP/FN — [DECISION O-04a REQUIRED]

Depends O-01 temporal geometry, O-02 band
```

## 7. K1–K4 and K5_RESERVED = unused in REV8

[DECISION O-04 REQUIRED] K2 sum IoU exact — [DECISION O-04 REQUIRED]
[DECISION O-05 REQUIRED] K3 temporal cost — depends O-01 — [DECISION O-05 REQUIRED]
[DECISION O-06 REQUIRED] K4 geometric — [DECISION O-06 REQUIRED]
[DECISION O-06a REQUIRED] Resonance center-cost — Alternatives A-E, E exact product r_e=max/min Prod r_e — PREFERRED CANDIDATE — [DECISION O-06a REQUIRED] — Objective components represent exact mathematical values, equality exact, any internal representation allowed if observationally equivalent, canonical reduction only at serialization boundaries
[DECISION O-07 REQUIRED] K5 — Alternatives: A) exists with per-class explicit function, B) K5_RESERVED = unused in REV8, K6 follows K4 — Norm: K5 does not participate in eligibility, optimization, matching equivalence or canonicalization. K6 follows K4. — [DECISION O-07 REQUIRED]

## 8. Exact OPT

[DECISION O-08 REQUIRED]
```
V* = OPT(∅,∅)
E = (e1..em) canonical order — [DECISION O-20 REQUIRED]
R required F forbidden

Invariants: [DECISION O-08 REQUIRED]

A1 procedure reference candidate — [DECISION O-08 REQUIRED]:
for each e_i canonical order: if conflicts R => F else Vi=OPT(R∪{e_i},F) if Vi==V* => R∪{e_i} else F terminate |R|=K1*

Internal non-fatal:
OPT_SUBPROBLEM_INFEASIBLE -> e_i forbidden continue — [DECISION O-08 REQUIRED]
OPT_VALUE_DIFFERS_FROM_GLOBAL_OPTIMUM -> e_i forbidden continue — [DECISION O-08 REQUIRED]

Fatal:
SOLVER_STRUCTURAL_LIMIT_EXCEEDED fatal — [DECISION O-09 REQUIRED]
SOLVER_RUNTIME_FAILURE fatal — [DECISION O-09 REQUIRED]
SOLVER_CONSTRAINT_MODEL_INVALID fatal — [DECISION O-08 REQUIRED]
No approximate fallback
```

[DECISION O-08a REQUIRED] Vector representation — implementation-neutral
```
Type each component — [DECISION O-08a REQUIRED]
Equality exact — [DECISION O-08a REQUIRED]
Objective components represent exact mathematical values. Equality and ordering are exact. Any internal representation allowed if observationally equivalent. Canonical reduction required only at normative serialization boundaries, not necessarily after each arithmetic operation. — [DECISION O-08a REQUIRED]
Bit complexity bound — [DECISION O-08a REQUIRED]
```

## 9. A1/K6 Canonicalization

[DECISION O-20 REQUIRED]
```
Canonical edge key form — [DECISION O-20 REQUIRED] no candidate-controlled metrics severity/confidence/ID/hash in K6
Total order edges — [DECISION O-20 REQUIRED]
occurrence_ordinal only for serializing identical payload copies not deduplication not scientific pairing foundation
```

## 10. Resource / Failure Policy

[DECISION O-09 REQUIRED]
```
Normative deterministic preflight caps: max GT, max prediction, max archi, max bit-length, max OPT calls, structural budget preflight calculable before execution — [DECISION O-09 REQUIRED]
Cap detected before partial computation — [DECISION O-09 REQUIRED]

Operational failures: OOM, runtime exception, unavailability, corruption — [DECISION O-09 REQUIRED]

Reason codes:
SOLVER_STRUCTURAL_LIMIT_EXCEEDED fatal
SOLVER_RUNTIME_FAILURE fatal
SOLVER_CONSTRAINT_MODEL_INVALID fatal
OPT_SUBPROBLEM_INFEASIBLE non-fatal internal A1
OPT_VALUE_DIFFERS_FROM_GLOBAL_OPTIMUM non-fatal internal A1

Any fatal in unit contributing to gate metric => metric/report not evaluable and prevents PASS
```

## 11. Metric Pairing Policy

[DECISION O-10 REQUIRED] Severity MAE — [DECISION O-10 REQUIRED]
[DECISION O-11 REQUIRED] Onset/Offset — C6-B onset/offset ambiguity — [DECISION O-11 REQUIRED] depends O-01
[DECISION O-12 REQUIRED] Spearman — [DECISION O-12 REQUIRED]

## 12. Aggregators

[DECISION O-13 REQUIRED] — RED — 5 dimensions published/gate/diagnostic/intra/inter macro weight 1 + quantile convention nearest-rank vs interpolation zero/one-based small-support tie N/A groups without matched — [DECISION O-13 REQUIRED]

## 13. Precision-Recall Metric / Average Precision Convention

[WORDING O-14 — distinct-confidence thresholds decided; normative wording pending] Distinct-confidence — P_t={p:conf>=t} t distinct — [WORDING O-14 — distinct-confidence thresholds decided; normative wording pending]
[DECISION O-14b REQUIRED] Integration — gradini vs trapezoidale, initial/final, recall duplicate, no GT, no Pred, macro per group, calibration authority — [DECISION O-14b REQUIRED]

## 14. Identity / Hashing

[TRANSFER/TEST O-16 — kind literals decided; transfer/test pending] Kind literals — [TRANSFER/TEST O-16 — kind literals decided; transfer/test pending]
[WORDING O-17 — problem_type_id omission rationale decided; normative wording pending] problem_type_id — [WORDING O-17 — problem_type_id omission rationale decided; normative wording pending]
[DECISION O-20 REQUIRED] Decision key split — [DECISION O-20 REQUIRED]

## 15. Schema v2 Isolation

- schemas_v2.py isolated — REPORTED 51 tests 12 mutation 414 suite PASS dispatcher isolation PASS
- Activation only atomic switch — PROPOSED
- REV7 live sealed SHA256SUMS line 49 digest fa506142->644081b5 — REPORTED — must remain immutable
- width_octaves >=0 for single-band — [ARTIFACT O-02 + DECISION O-03 REQUIRED]

## 16. Test Oracle / Metamorphic / Mutation

[DECISION O-08 REQUIRED] Oracle exhaustive |GT|<=3 |Pred|<=3
[ARTIFACT O-02] Projection fixtures:
- NaN -> FAIL; +Inf/-Inf -> FAIL;
- finite raw below the grid -> band 0; finite raw above the grid -> band 119;
- x == boundary[k] -> lower band k using bit-exact equality;
- raw_lo > raw_hi -> FAIL;
- raw_lo == raw_hi finite -> one-band interval with I=1.

[ARTIFACT O-02 + DECISION O-03/O-04a REQUIRED] Metamorphic: permutation, ID renaming, H/S canonical equivalence, severity permutation, input order adjacency
[DECISION O-08/O-20 REQUIRED] Mutation must kill:
- activate K5_RESERVED as an objective;
- allow K5_RESERVED to affect eligibility;
- allow K5_RESERVED to affect matching equivalence;
- allow K5_RESERVED to affect K6 canonicalization;
- invert K3;
- replace exact sums with floating-point sums;
- use ID/hash/severity/confidence in the tie-break;
- ignore occurrence_ordinal;
- reverse K6;
- use greedy first-fit;
- depend on adjacency/input order;
- reintroduce center/width in the H/S matcher;
- replace exact IoU with early-rounded binary64.

## 17. Activation Sequence

1. Decision register Final Patched approved — [DECISION ALL REQUIRED]
2. Round 2.3 normative with decisions filled — [DECISION ALL REQUIRED]
3. Independent red-team restricted — [DECISION ALL REQUIRED]
4. Candidate patch document-only — [DECISION ALL REQUIRED]
5. Counter-check immutable commit — [DECISION ALL REQUIRED]
6. Eventually REV8 SPEC GO — [DECISION ALL REQUIRED]

## 18. Decision Traceability Table

| Placeholder | Ballot ID | Status |
|---|---|---|
| Temporal authority C7-A 2 ULP | O-01 | UNDECIDED YELLOW |
| Boundary artifact bit-exact | O-02 | DECIDED_PENDING_ARTIFACT — non O-01, canonical center grid + O-03 only |
| Width bound | O-03 | UNDECIDED / PENDING wording |
| Eligible edge predicate | O-04a | UNDECIDED critical NEW |
| K2 sum IoU | O-04 | UNDECIDED |
| K3 temporal cost | O-05 | UNDECIDED depends O-01 |
| K4 geometric | O-06 | UNDECIDED |
| K4 Resonance numeric E | O-06a | UNDECIDED PREFERRED E implementation-neutral |
| K5_RESERVED = unused in REV8 | O-07 | UNDECIDED |
| OPT(R,F) A1 | O-08 | UNDECIDED RED |
| Vector representation | O-08a | UNDECIDED NEW implementation-neutral |
| FAIL blast radius | O-09 | UNDECIDED |
| Severity MAE policy | O-10 | UNDECIDED |
| Onset/Offset C6-B | O-11 | UNDECIDED |
| Spearman policy | O-12 | UNDECIDED |
| Aggregators quantile | O-13 | UNDECIDED |
| PR-AUC distinct | O-14 | DECIDED_PENDING_WORDING |
| PR metric integration / Average Precision convention | O-14b | UNDECIDED NEW |
| C4 structural keys | O-15 | UNDECIDED |
| Kind literals | O-16 | DECIDED_PENDING_TRANSFER |
| problem_type_id | O-17 | DECIDED_PENDING_WORDING |
| sum_pairwise64 | O-18 | DECIDED_PENDING_ARTIFACT_TEST — parsing binary64 generale |
| Revision authority | O-19 | DECIDED_PENDING_TRANSFER |
| Decision key split | O-20 | UNDECIDED |

Total 24 decision IDs: 23 in V2 + O-04a =24 — not 20+4

Document-only freeze package added on the dedicated branch. No code/runtime/schema/training changes; the candidate content at base e2bee113 remains unchanged; no SPEC GO; Round 2.3 normative not written, only this template.
