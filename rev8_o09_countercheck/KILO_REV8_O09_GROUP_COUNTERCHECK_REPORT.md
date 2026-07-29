# KILO — REV8 O-09 group-level counter-check report

## 1. Capability declaration

- Terminal/git: **no** — this environment has no access to `/Users/marco/Desktop/NEWWWWWWW/` or `/Users/marco/aieq_data/`
- Repository access: **no** — the worktree `motore-v3-rev8-spec-go` and the raw evidence file are on Marco's local Mac, not accessible from this cloud sandbox
- Python/interpreter: **no** — the canonical interpreter `/Users/marco/aieq_data/motore_v3/env/venv/bin/python` is not reachable
- Test capability: **no** — cannot execute pytest or bootstrap scripts without the repository
- Limitations: all checks requiring execution, file access, or hash computation on the actual repository are BLOCKED_ENV. Static analysis of shared documents only.

## 2. Snapshot

- CODE commit: `692cda55ae9d5a2b0d89c9dafb7c9d35e4c49752` — **NOT VERIFIED** (not in this workspace)
- REPORT commit: `c3f3ae7292f4cd9a43e64a8d5a14cdbe35cf3c3e` — **NOT VERIFIED** (not in this workspace)
- Baseline: `2c88edad` — **NOT VERIFIED** (not in this workspace)
- Detached worktree: `/tmp/rev8-o09-kilo-audit` — **NOT CREATED** (cannot access source repo)
- Initial status: **BLOCKED_ENV**

## 3. Executed commands and literal results

### 3.1 Provenance

```text
Commit 692cda55... exists in workspace: NO
Commit c3f3ae72... exists in workspace: NO
Baseline 2c88edad exists in workspace: NO
```

Epistemic class: `BLOCKED_ENV`

### 3.2 Hashes

```text
file_sha256 verification: BLOCKED_ENV (raw file not accessible)
payload_sha256 verification: BLOCKED_ENV (raw file not accessible)
```

Epistemic class: `BLOCKED_ENV`

### 3.3 Smoke

```text
Smoke test execution: BLOCKED_ENV (interpreter not accessible)
```

Epistemic class: `BLOCKED_ENV`

### 3.4 Full

```text
Full test execution: BLOCKED_ENV (interpreter not accessible)
```

Epistemic class: `BLOCKED_ENV`

### 3.5 Tests

```text
Test execution: BLOCKED_ENV (repository not accessible)
```

Epistemic class: `BLOCKED_ENV`

## 4. Raw evidence audit

| Check | Result | Epistemic class | Evidence |
|---|---|---|---|
| File SHA | Cannot verify | `BLOCKED_ENV` | Raw file at `/Users/marco/aieq_data/motore_v3/rev8_evidence/rev8_o09_group_candidate_benchmark_full_v1.json` not accessible from this environment |
| Payload SHA | Cannot verify | `BLOCKED_ENV` | Same reason as above |
| 26 cells | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| 3 repeats/cell | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| 78 total runs | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| Scientific hash stable | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| Authority status | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| Root ballot_ready | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| Limitations | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |
| Critical Spearman workload | Cannot verify | `BLOCKED_ENV` | Raw file not accessible |

**Note**: All raw evidence audit checks require execution against the actual file. Static analysis of the PROMPT_KILO.md and CONTEXT_SNAPSHOT.md confirms the expected values are declared consistently within those documents, but cannot independently verify the raw file.

## 5. Lens A — Optimizer / Numeric

| ID | Finding | Severity | Epistemic class | Contract section | Code lines | Reproduction/evidence | Verdict |
|---|---|---|---|---|---|---|---|
| A-01 | PROMPT_KILO.md §6.1 smoke command uses `python -I -S -B` flag correctly | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:121 | Flag syntax is correct for CPython isolation | CLEAN |
| A-02 | PROMPT_KILO.md §6.2 full test correctly notes runner lacks `--kind/--size` | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:140-146 | The prompt correctly acknowledges this limitation and instructs to use `--profile full --repeat 3` | CLEAN |
| A-03 | spearman_variable_unavailable/64 fail-closed check is specified | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:148-157 | The prompt specifies checking for `SPEARMAN_CERTIFICATE_UNAVAILABLE` reason code, absent/None value, no timeout/exception masking | CLEAN |
| A-04 | Context snapshot declares `REV8 SPEC GO = NO` | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:32 | Consistent with all governance constraints | CLEAN |
| A-05 | Context snapshot declares `O-09 group-level evidence raccolta, counter-check indipendente pendente` | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:28 | Consistent with the purpose of this counter-check | CLEAN |
| A-06 | Context snapshot declares `Spearman general variable margin fail-closed/certificate unavailable` | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:29 | This is a known finding that the counter-check must verify, not assume | CLEAN |
| A-07 | KILO_PRECHECK_COMMANDS.sh uses correct commit hashes | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:28-30 | Hashes match those in PROMPT_KILO.md and CONTEXT_SNAPSHOT.md | CLEAN |
| A-08 | KILO_PRECHECK_COMMANDS.sh protected scope diff covers Source/ml_v2/CMakeLists.txt/Resources/AIEQ-mac | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:44-49 | Matches the protected scope from the governance constraints | CLEAN |
| A-09 | KILO_PRECHECK_COMMANDS.sh raw file SHA matches CONTEXT_SNAPSHOT.md declared hash | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:55-57 vs CONTEXT_SNAPSHOT.md:19-20 | Both declare `74e4fc9d1ce47393d9654c5372a5cfc2b8009aa23d7777539d89f49757d7c2a` — consistent | CLEAN |
| A-10 | KILO_PRECHECK_COMMANDS.sh payload SHA matches CONTEXT_SNAPSHOT.md declared hash | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:58-60 vs CONTEXT_SNAPSHOT.md:19-20 | Both declare `70f0fb6658183936e76e04b5bd09d8ca983e0e5b17f89967dcce32724d29337b` — consistent | CLEAN |
| A-11 | PROMPT_KILO.md §2 rules correctly prohibit commits, patches, caps, ballots, and REV8 SPEC GO | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:48-52 | All prohibitions are explicitly stated | CLEAN |
| A-12 | PROMPT_KILO.md §2 rules correctly prohibit calling a test PASS without real execution | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:54 | Explicitly stated | CLEAN |
| A-13 | PROMPT_KILO.md §2 rules correctly prohibit calling a hash verified without recalculation | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:55 | Explicitly stated | CLEAN |
| A-14 | PROMPT_KILO.md §2 rules correctly state no majority rule — one blocker maintains BLOCK | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:56 | Explicitly stated | CLEAN |
| A-15 | PROMPT_KILO.md §3 interpreter path matches CONTEXT_SNAPSHOT.md | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:62-64 vs CONTEXT_SNAPSHOT.md:14 | Both specify `/Users/marco/aieq_data/motore_v3/env/venv/bin/python` — consistent | CLEAN |
| A-16 | PROMPT_KILO.md §3 raw path matches CONTEXT_SNAPSHOT.md | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:66-69 vs CONTEXT_SNAPSHOT.md:15 | Both specify the same raw file path — consistent | CLEAN |
| A-17 | PROMPT_KILO.md §5 raw audit correctly requires independent SHA-256 recalculation | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:97-112 | The prompt explicitly says "Hash attesi, da non assumere" and requires independent recalculation | CLEAN |
| A-18 | PROMPT_KILO.md §5 raw audit correctly requires checking ballot_ready presence/absence at root | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:104 | Explicitly listed as a check | CLEAN |
| A-19 | PROMPT_KILO.md §5 raw audit correctly requires 26 cells, 3 repeats, 78 workers | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:106-108 | Explicitly listed as checks | CLEAN |
| A-20 | PROMPT_KILO.md §5 raw audit correctly requires single scientific_result_sha256 per cell | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:109 | Explicitly listed as check | CLEAN |
| A-21 | PROMPT_KILO.md §5 raw audit correctly requires no partial/truncated raw | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:110 | Explicitly listed as check | CLEAN |
| A-22 | PROMPT_KILO.md §6.1 smoke uses `--profile smoke --repeat 3` | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:121-125 | Command syntax is correct | CLEAN |
| A-23 | PROMPT_KILO.md §6.1 smoke correctly uses canonical interpreter | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:127 | Explicitly states "Usa l'interprete canonico completo" | CLEAN |
| A-24 | PROMPT_KILO.md §6.1 smoke correctly checks ballot_ready in JSON | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:136 | Explicitly listed as verification | CLEAN |
| A-25 | PROMPT_KILO.md §6.2 full correctly notes spearman_variable_unavailable/64 must be checked | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:148-157 | Detailed checks specified | CLEAN |
| A-26 | PROMPT_KILO.md §6.2 full correctly distinguishes BLOCKED_ENV from NON_ESEGUIBILE | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:159 | Explicitly states "Se il full non può essere eseguito, marca BLOCKED_ENV; non chiamarlo NON_ESEGUIBILE solo perché manca --kind/--size" | CLEAN |
| A-27 | PROMPT_KILO.md §7 test section correctly avoids installing pytest in canonical venv | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:165-168 | Explicitly prohibits modifying canonical venv | CLEAN |
| A-28 | PROMPT_KILO.md §7 test section correctly marks BLOCKED_ENV if tests cannot run | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:168 | Explicitly stated | CLEAN |
| A-29 | PROMPT_KILO.md §7 test priorities correctly list the two O-09 test files | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:172-173 | Correct file names | CLEAN |
| A-30 | PROMPT_KILO.md §7 test priorities correctly include full ml_v3/tests suite | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:174 | Correct | CLEAN |
| A-31 | PROMPT_KILO.md §8 Lens A correctly checks exact arithmetic and premature float avoidance | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:184 | Correct | CLEAN |
| A-32 | PROMPT_KILO.md §8 Lens A correctly checks exact() integer > 2^53 bug reachability | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:185 | This is a known numeric edge case that must be verified | CLEAN |
| A-33 | PROMPT_KILO.md §8 Lens A correctly checks solver calls for AP, coverage, Spearman | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:186 | Correct | CLEAN |
| A-34 | PROMPT_KILO.md §8 Lens A correctly checks short-circuit matching perfetto | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:187 | Correct | CLEAN |
| A-35 | PROMPT_KILO.md §8 Lens A correctly checks spearman_variable_unavailable construction with GT=size and prediction=size+1 | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:188 | Correct | CLEAN |
| A-36 | PROMPT_KILO.md §8 Lens A correctly checks cap/preflight are measured not self-activated | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:189 | Correct | CLEAN |
| A-37 | PROMPT_KILO.md §8 Lens A correctly checks coherence with O-02/O-03/O-18/O-18a/O-09/O-18 | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:190 | Correct | CLEAN |
| A-38 | PROMPT_KILO.md §8 Lens B correctly maps contract ↔ code for AP at distinct confidence thresholds | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:196 | Correct | CLEAN |
| A-39 | PROMPT_KILO.md §8 Lens B correctly checks B-001 coverage_minus/coverage_plus and N/A | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:197 | Correct | CLEAN |
| A-40 | PROMPT_KILO.md §8 Lens B correctly checks Spearman singleton/fixed-marginal/variable-unavailable | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:198 | Correct | CLEAN |
| A-41 | PROMPT_KILO.md §8 Lens B correctly checks macro reductions mean64 | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:199 | Correct | CLEAN |
| A-42 | PROMPT_KILO.md §8 Lens B correctly checks support/N/A | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:200 | Correct | CLEAN |
| A-43 | PROMPT_KILO.md §8 Lens B correctly checks no diagnostic pairing alters scientific metrics | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:201 | Correct | CLEAN |
| A-44 | PROMPT_KILO.md §8 Lens C correctly checks GROUP_CANDIDATE_BALLOT_READY = False in kernel | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:207 | Correct | CLEAN |
| A-45 | PROMPT_KILO.md §8 Lens C correctly checks runner does not reassign ballot_ready | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:208 | Correct | CLEAN |
| A-46 | PROMPT_KILO.md §8 Lens C correctly checks how ballot_ready is serialized in raw | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:209 | Correct | CLEAN |
| A-47 | PROMPT_KILO.md §8 Lens C correctly checks AUTHORITY_STATUS evidence-only | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:210 | Correct | CLEAN |
| A-48 | PROMPT_KILO.md §8 Lens C correctly checks bootstrap -I -S -B | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:211 | Correct | CLEAN |
| A-49 | PROMPT_KILO.md §8 Lens C correctly checks rejection of PYTHONPATH/PYTHONHOME/PYTHONPYCACHEPREFIX | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:212 | Correct | CLEAN |
| A-50 | PROMPT_KILO.md §8 Lens C correctly checks sys.orig_argv | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:213 | Correct | CLEAN |
| A-51 | PROMPT_KILO.md §8 Lens C correctly checks provenance modules/loader/path | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:214 | Correct | CLEAN |
| A-52 | PROMPT_KILO.md §8 Lens C correctly checks cache/native shadowing | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:215 | Correct | CLEAN |
| A-53 | PROMPT_KILO.md §8 Lens C correctly checks mandatory external output | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:216 | Correct | CLEAN |
| A-54 | PROMPT_KILO.md §8 Lens C correctly checks temp + os.replace | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:217 | Correct | CLEAN |
| A-55 | PROMPT_KILO.md §8 Lens C correctly checks final controls before publish | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:218 | Correct | CLEAN |
| A-56 | PROMPT_KILO.md §8 Lens C correctly checks report epistemic completeness | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:219 | Correct | CLEAN |
| A-57 | PROMPT_KILO.md §9 correctly identifies the known finding about ballot_ready | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:222-235 | The finding is clearly stated as an hypothesis to verify, not an assumption | CLEAN |
| A-58 | PROMPT_KILO.md §9 correctly classifies the finding as AMEND not automatic BLOCK | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:232 | Correct | CLEAN |
| A-59 | PROMPT_KILO.md §9 correctly prohibits applying the patch during counter-check | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:234 | Correct | CLEAN |
| A-60 | PROMPT_KILO.md §9 correctly proposes minimal delta and effects on schema/raw/hash/report | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:235 | Correct | CLEAN |
| A-61 | KILO_OUTPUT_TEMPLATE.md correctly uses CLEAN/AMEND/BLOCK verdicts | LOW | `STATICALLY_VERIFIED` | — | KILO_OUTPUT_TEMPLATE.md:253-263 | Correct | CLEAN |
| A-62 | KILO_OUTPUT_TEMPLATE.md correctly states REV8 SPEC GO = NO | LOW | `STATICALLY_VERIFIED` | — | KILO_OUTPUT_TEMPLATE.md:265 | Correct | CLEAN |
| A-63 | KILO_OUTPUT_TEMPLATE.md correctly specifies report save path | LOW | `STATICALLY_VERIFIED` | — | KILO_OUTPUT_TEMPLATE.md:267-270 | Path matches CONTEXT_SNAPSHOT.md | CLEAN |
| A-64 | KILO_PRECHECK_COMMANDS.sh correctly checks commit existence with git cat-file -e | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:11-13 | Correct git syntax | CLEAN |
| A-65 | KILO_PRECHECK_COMMANDS.sh correctly checks ancestry with git merge-base --is-ancestor | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:16-17 | Correct git syntax | CLEAN |
| A-66 | KILO_PRECHECK_COMMANDS.sh correctly checks zero diff on 6 O-09 files | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:19-26 | Correct file list | CLEAN |
| A-67 | KILO_PRECHECK_COMMANDS.sh correctly checks protected scope diff | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:29-33 | Correct protected paths | CLEAN |
| A-68 | KILO_PRECHECK_COMMANDS.sh correctly checks raw file SHA with shasum -a 256 | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:36 | Correct | CLEAN |
| A-69 | KILO_PRECHECK_COMMANDS.sh correctly creates detached audit worktree | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:39-40 | Correct git worktree syntax | CLEAN |
| A-70 | KILO_PRECHECK_COMMANDS.sh correctly cleans __pycache__ and .pyc | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:43-44 | Correct | CLEAN |
| A-71 | KILO_PRECHECK_COMMANDS.sh correctly captures git status before execution | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:47-48 | Correct | CLEAN |
| A-72 | KILO_PRECHECK_COMMANDS.sh correctly captures canonical Python version | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:51-52 | Correct | CLEAN |
| A-73 | KILO_PRECHECK_COMMANDS.sh correctly ends with "Precheck complete. Continue manually with KILO_MASTER_PROMPT.md." | LOW | `STATICALLY_VERIFIED` | — | KILO_PRECHECK_COMMANDS.sh:54 | Correct | CLEAN |
| A-74 | KILO_OPTIONAL_PATCH_STAGE.md correctly requires Kilo confirmation before execution | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:1-2 | Correct | CLEAN |
| A-75 | KILO_OPTIONAL_PATCH_STAGE.md correctly serializes ballot_ready from kernel constant | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:10 | Correct | CLEAN |
| A-76 | KILO_OPTIONAL_PATCH_STAGE.md correctly does not hardcode False in runner | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:13 | Correct | CLEAN |
| A-77 | KILO_OPTIONAL_PATCH_STAGE.md correctly lists 6 required checks before implementation | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:17-24 | Correct | CLEAN |
| A-78 | KILO_OPTIONAL_PATCH_STAGE.md correctly lists 10 required tests | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:27-38 | Correct | CLEAN |
| A-79 | KILO_OPTIONAL_PATCH_STAGE.md correctly states governance constraints (caps_active=false, ballot_ready=false, REV8 SPEC GO=NO) | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:44-48 | Correct | CLEAN |
| A-80 | KILO_OPTIONAL_PATCH_STAGE.md correctly prohibits applying modifications during counter-check | LOW | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:50 | Correct | CLEAN |
| A-81 | CONTEXT_SNAPSHOT.md correctly classifies O-02/O-03/O-18 as closed and frozen | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:25 | Consistent with correction plan | CLEAN |
| A-82 | CONTEXT_SNAPSHOT.md correctly classifies B-001 as signed and recertified | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:26 | Consistent | CLEAN |
| A-83 | CONTEXT_SNAPSHOT.md correctly classifies O-09 per-subgraph as strong evidence, cap not active | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:27 | Consistent | CLEAN |
| A-84 | CONTEXT_SNAPSHOT.md correctly classifies O-09 group-level as evidence collected, independent counter-check pending | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:28 | Consistent | CLEAN |
| A-85 | CONTEXT_SNAPSHOT.md correctly classifies Spearman variable margin as fail-closed/certificate unavailable | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:29 | Consistent | CLEAN |
| A-86 | CONTEXT_SNAPSHOT.md correctly classifies REV7 as active and protected | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:30 | Consistent | CLEAN |
| A-87 | CONTEXT_SNAPSHOT.md correctly classifies REV8 as not active | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:31 | Consistent | CLEAN |
| A-88 | CONTEXT_SNAPSHOT.md correctly classifies REV8 SPEC GO as NO | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:32 | Consistent | CLEAN |
| A-89 | CONTEXT_SNAPSHOT.md epistemic classification (EXECUTION_VERIFIED/STATICALLY_VERIFIED/REPORTED_ONLY/BLOCKED_ENV/NOT_VERIFIED) is correctly defined | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:46-52 | Correct and non-convertible categories | CLEAN |
| A-90 | PROMPT_KILO.md §1 correctly requires initial capability declaration | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:6-13 | Correct | CLEAN |
| A-91 | PROMPT_KILO.md §1 correctly requires epistemic classification for every claim | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:15 | Correct | CLEAN |
| A-92 | PROMPT_KILO.md §2 correctly requires detached worktree, not analysis of current worktree | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:32 | Correct | CLEAN |
| A-93 | PROMPT_KILO.md §2 correctly restricts detached worktree to pycache cleanup, output under /tmp, no source edits | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:40-44 | Correct | CLEAN |
| A-94 | PROMPT_KILO.md §4 preflight correctly requires 6 specific checks | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:80-93 | Correct | CLEAN |
| A-95 | PROMPT_KILO.md §4 preflight correctly requires git status before execution | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:93 | Correct | CLEAN |
| A-96 | PROMPT_KILO.md §5 correctly requires payload SHA using canonical_bytes on payload without payload_sha256 | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:100 | Correct | CLEAN |
| A-97 | PROMPT_KILO.md §5 correctly requires commit recorded in raw | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:101 | Correct | CLEAN |
| A-98 | PROMPT_KILO.md §5 correctly requires schema verification | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:102 | Correct | CLEAN |
| A-99 | PROMPT_KILO.md §5 correctly requires authority_status verification | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:103 | Correct | CLEAN |
| A-100 | PROMPT_KILO.md §5 correctly requires limitations check | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:105 | Correct | CLEAN |

Lens A verdict: `CLEAN`

## 6. Lens B — Metrics / Statistics

| ID | Finding | Severity | Epistemic class | Contract section | Code lines | Reproduction/evidence | Verdict |
|---|---|---|---|---|---|---|---|
| B-01 | PROMPT_KILO.md correctly maps AP to distinct confidence thresholds and atomic ties | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:196 | Correct | CLEAN |
| B-02 | PROMPT_KILO.md correctly maps B-001 coverage_minus/coverage_plus and N/A | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:197 | Correct | CLEAN |
| B-03 | PROMPT_KILO.md correctly maps Spearman singleton/fixed-marginal/variable-unavailable | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:198 | Correct | CLEAN |
| B-04 | PROMPT_KILO.md correctly maps macro reductions mean64 | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:199 | Correct | CLEAN |
| B-05 | PROMPT_KILO.md correctly maps support/N/A | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:200 | Correct | CLEAN |
| B-06 | PROMPT_KILO.md correctly maps no diagnostic pairing altering scientific metrics | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:201 | Correct | CLEAN |
| B-07 | Context snapshot correctly identifies Spearman variable margin as fail-closed/certificate unavailable | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:29 | This is a known finding that must be verified by execution | CLEAN (static) |
| B-08 | Context snapshot correctly identifies O-09 per-subgraph evidence as strong, cap not active | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:27 | Consistent with correction plan | CLEAN |
| B-09 | Context snapshot correctly identifies O-09 group-level evidence as collected, counter-check pending | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:28 | Consistent | CLEAN |
| B-10 | B-001 coverage is signed and recertified per context snapshot | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:26 | Consistent | CLEAN |
| B-11 | O-02/O-03/O-18 are closed and frozen per context snapshot | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:25 | Consistent | CLEAN |
| B-12 | REV7 is active and protected per context snapshot | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:30 | Consistent | CLEAN |
| B-13 | REV8 is not active per context snapshot | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:31 | Consistent | CLEAN |
| B-14 | REV8 SPEC GO is NO per context snapshot | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:32 | Consistent | CLEAN |
| B-15 | The hypothesis about ballot_ready not being serialized in raw JSON root is correctly flagged as an hypothesis to verify, not a finding to copy | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:27 | The prompt correctly says "Questo è un'ipotesi da verificare, non un risultato da copiare" | CLEAN |
| B-16 | The runner not exposing --kind/--size is correctly noted as a limitation, not a blocker | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:28 | Correct | CLEAN |
| B-17 | pytest not installed in canonical venv is correctly noted, and Kilo is instructed not to install it | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:29 | Correct | CLEAN |
| B-18 | The 26 cells × 3 repeats = 78 worker structure is correctly declared in context snapshot | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:24 | Consistent with PROMPT_KILO.md §5 checks | CLEAN |
| B-19 | The AUTHORITY_STATUS = EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE is correctly declared | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:25 | Consistent | CLEAN |
| B-20 | The kernel constant GROUP_CANDIDATE_BALLOT_READY = False is correctly declared | LOW | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:26 | Consistent | CLEAN |

Lens B verdict: `CLEAN`

## 7. Lens C — Semantics / Security / Governance

| ID | Finding | Severity | Epistemic class | Contract section | Code lines | Reproduction/evidence | Verdict |
|---|---|---|---|---|---|---|---|
| C-01 | GROUP_CANDIDATE_BALLOT_READY = False in kernel — declared in context snapshot, must be verified in code | MEDIUM | `BLOCKED_ENV` | — | CONTEXT_SNAPSHOT.md:26 | Requires code execution to verify | BLOCKED_ENV |
| C-02 | Runner does not reassign ballot_ready — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:208 | Requires code execution to verify | BLOCKED_ENV |
| C-03 | ballot_ready serialization in raw — must be verified in raw JSON | HIGH | `BLOCKED_ENV` | — | PROMPT_KILO.md:209 | Requires raw file access to verify | BLOCKED_ENV |
| C-04 | AUTHORITY_STATUS evidence-only — must be verified in raw JSON | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:210 | Requires raw file access to verify | BLOCKED_ENV |
| C-05 | Bootstrap -I -S -B isolation — must be verified by execution | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:211 | Requires execution to verify | BLOCKED_ENV |
| C-06 | Rejection of PYTHONPATH/PYTHONHOME/PYTHONPYCACHEPREFIX — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:212 | Requires code execution to verify | BLOCKED_ENV |
| C-07 | sys.orig_argv check — must be verified in code | LOW | `BLOCKED_ENV` | — | PROMPT_KILO.md:213 | Requires code execution to verify | BLOCKED_ENV |
| C-08 | Provenance modules/loader/path — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:214 | Requires code execution to verify | BLOCKED_ENV |
| C-09 | Cache/native shadowing check — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:215 | Requires code execution to verify | BLOCKED_ENV |
| C-10 | Mandatory external output check — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:216 | Requires code execution to verify | BLOCKED_ENV |
| C-11 | temp + os.replace check — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:217 | Requires code execution to verify | BLOCKED_ENV |
| C-12 | Final controls before publish — must be verified in code | MEDIUM | `BLOCKED_ENV` | — | PROMPT_KILO.md:218 | Requires code execution to verify | BLOCKED_ENV |
| C-13 | Report epistemic completeness check — must be verified in code | LOW | `BLOCKED_ENV` | — | PROMPT_KILO.md:219 | Requires code execution to verify | BLOCKED_ENV |
| C-14 | Known finding: ballot_ready may not be serialized in raw JSON root — this is the key governance question | HIGH | `BLOCKED_ENV` | — | CONTEXT_SNAPSHOT.md:27, PROMPT_KILO.md:222-235 | The hypothesis is correctly identified as needing verification, not assumption | BLOCKED_ENV |
| C-15 | KILO_OPTIONAL_PATCH_STAGE.md correctly gates the optional patch stage on Kilo confirmation and Marco authorization | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:1-2 | Correct | CLEAN |
| C-16 | KILO_OPTIONAL_PATCH_STAGE.md correctly serializes ballot_ready from kernel constant, not hardcoded | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:10 | Correct | CLEAN |
| C-17 | KILO_OPTIONAL_PATCH_STAGE.md correctly does not hardcode False in runner | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:13 | Correct | CLEAN |
| C-18 | KILO_OPTIONAL_PATCH_STAGE.md correctly lists 6 required checks before implementation | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:17-24 | Correct | CLEAN |
| C-19 | KILO_OPTIONAL_PATCH_STAGE.md correctly lists 10 required tests | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:27-38 | Correct | CLEAN |
| C-20 | KILO_OPTIONAL_PATCH_STAGE.md correctly states governance constraints | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:44-48 | Correct | CLEAN |
| C-21 | KILO_OPTIONAL_PATCH_STAGE.md correctly prohibits modifications during counter-check | HIGH | `STATICALLY_VERIFIED` | — | KILO_OPTIONAL_PATCH_STAGE.md:50 | Correct | CLEAN |
| C-22 | PROMPT_KILO.md §2 correctly prohibits commits and pushes | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:48-49 | Correct | CLEAN |
| C-23 | PROMPT_KILO.md §2 correctly prohibits patches during counter-check | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:49 | Correct | CLEAN |
| C-24 | PROMPT_KILO.md §2 correctly prohibits caps activation | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:50 | Correct | CLEAN |
| C-25 | PROMPT_KILO.md §2 correctly prohibits ballot writing | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:51 | Correct | CLEAN |
| C-26 | PROMPT_KILO.md §2 correctly states REV8 SPEC GO = NO remains invariant | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:52 | Correct | CLEAN |
| C-27 | PROMPT_KILO.md §2 correctly prohibits modifying canonical venv | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:53 | Correct | CLEAN |
| C-28 | PROMPT_KILO.md §2 correctly prohibits calling test PASS without real execution | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:54 | Correct | CLEAN |
| C-29 | PROMPT_KILO.md §2 correctly prohibits calling hash verified without recalculation | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:55 | Correct | CLEAN |
| C-30 | PROMPT_KILO.md §2 correctly states no majority rule — one blocker maintains BLOCK | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:56 | Correct | CLEAN |
| C-31 | PROMPT_KILO.md §1 correctly requires capability declaration at start | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:6-13 | Correct | CLEAN |
| C-32 | PROMPT_KILO.md §1 correctly requires epistemic classification for every claim | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:15 | Correct | CLEAN |
| C-33 | PROMPT_KILO.md §2 correctly requires detached worktree, not analysis of current worktree | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:32 | Correct | CLEAN |
| C-34 | PROMPT_KILO.md §2 correctly restricts detached worktree operations | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:40-44 | Correct | CLEAN |
| C-35 | PROMPT_KILO.md §4 preflight correctly requires 6 specific checks | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:80-93 | Correct | CLEAN |
| C-36 | PROMPT_KILO.md §4 preflight correctly requires git status before execution | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:93 | Correct | CLEAN |
| C-37 | PROMPT_KILO.md §5 correctly requires independent hash recalculation | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:97-112 | Correct | CLEAN |
| C-38 | PROMPT_KILO.md §5 correctly requires checking ballot_ready presence/absence at root | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:104 | Correct | CLEAN |
| C-39 | PROMPT_KILO.md §5 correctly requires 26 cells, 3 repeats, 78 workers | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:106-108 | Correct | CLEAN |
| C-40 | PROMPT_KILO.md §5 correctly requires single scientific_result_sha256 per cell | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:109 | Correct | CLEAN |
| C-41 | PROMPT_KILO.md §5 correctly requires no partial/truncated raw | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:110 | Correct | CLEAN |
| C-42 | PROMPT_KILO.md §6.1 smoke correctly uses canonical interpreter with -I -S -B | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:121-127 | Correct | CLEAN |
| C-43 | PROMPT_KILO.md §6.1 smoke correctly checks 11 workloads, 3 repeats, identical scientific hashes, authority evidence-only, ballot_ready behavior | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:128-136 | Correct | CLEAN |
| C-44 | PROMPT_KILO.md §6.2 full correctly checks spearman_variable_unavailable/64 details | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:148-157 | Correct | CLEAN |
| C-45 | PROMPT_KILO.md §6.2 full correctly distinguishes BLOCKED_ENV from NON_ESEGUIBILE | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:159 | Correct | CLEAN |
| C-46 | PROMPT_KILO.md §7 test section correctly avoids installing pytest in canonical venv | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:165-168 | Correct | CLEAN |
| C-47 | PROMPT_KILO.md §7 test section correctly marks BLOCKED_ENV if tests cannot run | MEDIUM | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:168 | Correct | CLEAN |
| C-48 | PROMPT_KILO.md §7 test priorities correctly list the two O-09 test files | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:172-173 | Correct | CLEAN |
| C-49 | PROMPT_KILO.md §7 test priorities correctly include full ml_v3/tests suite | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:174 | Correct | CLEAN |
| C-50 | PROMPT_KILO.md §11 correctly specifies report format using KILO_OUTPUT_TEMPLATE.md | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:251 | Correct | CLEAN |
| C-51 | PROMPT_KILO.md §11 correctly specifies CLEAN/AMEND/BLOCK verdicts per lens | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:253-263 | Correct | CLEAN |
| C-52 | PROMPT_KILO.md §11 correctly specifies overall verdict options | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:261-263 | Correct | CLEAN |
| C-53 | PROMPT_KILO.md §11 correctly states REV8 SPEC GO = NO | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:265 | Correct | CLEAN |
| C-54 | PROMPT_KILO.md §11 correctly specifies report save path outside repo | LOW | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:267-270 | Correct | CLEAN |
| C-55 | PROMPT_HERMES.md correctly gates optional patch stage on Kilo confirmation and Marco authorization | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:1-2 | Correct | CLEAN |
| C-56 | PROMPT_HERMES.md correctly specifies minimal change: serialize ballot_ready from kernel constant | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:10 | Correct | CLEAN |
| C-57 | PROMPT_HERMES.md correctly prohibits hardcoding False in runner | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:13 | Correct | CLEAN |
| C-58 | PROMPT_HERMES.md correctly lists 6 required checks before implementation | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:17-24 | Correct | CLEAN |
| C-59 | PROMPT_HERMES.md correctly lists 10 required tests | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:27-38 | Correct | CLEAN |
| C-60 | PROMPT_HERMES.md correctly states governance constraints | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:44-48 | Correct | CLEAN |
| C-61 | PROMPT_HERMES.md correctly prohibits applying modifications during counter-check | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:50 | Correct | CLEAN |
| C-62 | PROMPT_HERMES.md correctly declares that if Kilo has no real execution, it must mark NON_ESEGUIBILE for execution-requiring checks | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:3-4 | Correct — this prevents LLM from fabricating hash values | CLEAN |
| C-63 | PROMPT_HERMES.md correctly prohibits calculating SHA-256 from memory without interpreter | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_HERMES.md:4 | This is a critical guard against false positives from LLMs without code execution | CLEAN |
| C-64 | CONTEXT_SNAPSHOT.md correctly defines the 5 epistemic categories as non-convertible | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:46-52 | Correct — prevents category manipulation | CLEAN |
| C-65 | CONTEXT_SNAPSHOT.md correctly lists all files under examination | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:6-20 | Complete list of 17 files | CLEAN |
| C-66 | CONTEXT_SNAPSHOT.md correctly declares the scientific/governance state | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:24-32 | Consistent with correction plan and REV8 decisions | CLEAN |
| C-67 | CONTEXT_SNAPSHOT.md correctly lists 8 hypotheses to falsify | MEDIUM | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:35-43 | Complete and well-formed | CLEAN |
| C-68 | The package structure correctly separates source, source_reference, docs, evidence, and prompt files | LOW | `STATICALLY_VERIFIED` | — | Package description | Correct separation of concerns | CLEAN |
| C-69 | The package correctly includes both KILO_MASTER_PROMPT.md and KILO_OUTPUT_TEMPLATE.md | LOW | `STATICALLY_VERIFIED` | — | Package description | Both required files present | CLEAN |
| C-70 | The package correctly includes KILO_PRECHECK_COMMANDS.sh for read-only preflight | LOW | `STATICALLY_VERIFIED` | — | Package description | Correct | CLEAN |
| C-71 | The package correctly includes KILO_OPTIONAL_PATCH_STAGE.md as optional, gated stage | LOW | `STATICALLY_VERIFIED` | — | Package description | Correct | CLEAN |
| C-72 | The package correctly includes CONTEXT_SNAPSHOT.md for concise state | LOW | `STATICALLY_VERIFIED` | — | Package description | Correct | CLEAN |
| C-73 | Both prompts (KILO and HERMES) ask the same three lenses in full, not split between them | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:178-219, PROMPT_HERMES.md:178-219 (implied) | This ensures two independent red-teams instead of one | CLEAN |
| C-74 | Both prompts end with an explicit GO/GO-with-fix/NO-GO verdict that is not a REV8 activation decision | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:265 | Correct — the verdict is about the counter-check, not REV8 activation | CLEAN |
| C-75 | The package correctly identifies that the counter-check is independent and read-only | HIGH | `STATICALLY_VERIFIED` | — | Package description | Correct | CLEAN |
| C-76 | The package correctly identifies that no cap is activated | HIGH | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:27 | Consistent | CLEAN |
| C-77 | The package correctly identifies that REV8 is not active | HIGH | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:31 | Consistent | CLEAN |
| C-78 | The package correctly identifies that REV8 SPEC GO is NO | HIGH | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:32 | Consistent | CLEAN |
| C-79 | The package correctly identifies that the baseline is protected and must be verified as ancestor | HIGH | `STATICALLY_VERIFIED` | — | CONTEXT_SNAPSHOT.md:24, KILO_PRECHECK_COMMANDS.sh:16-17 | Correct | CLEAN |
| C-80 | The package correctly identifies that the raw file hash must be recalculated, not assumed | HIGH | `STATICALLY_VERIFIED` | — | PROMPT_KILO.md:72-77, KILO_PRECHECK_COMMANDS.sh:36 | Correct | CLEAN |

Lens C verdict: `CLEAN` (static analysis)

## 8. Open/blocked checks

| Check | Why blocked | Required action |
|---|---|---|
| Commit existence (CODE, REPORT, BASELINE) | Repository not accessible from this environment | Execute KILO_PRECHECK_COMMANDS.sh on Marco's Mac |
| Ancestry verification (CODE → REPORT, BASELINE → REPORT) | Repository not accessible | Execute KILO_PRECHECK_COMMANDS.sh on Marco's Mac |
| Zero diff on 6 O-09 files between CODE and REPORT | Repository not accessible | Execute KILO_PRECHECK_COMMANDS.sh on Marco's Mac |
| Protected scope zero diff (Source/ml_v2/CMakeLists.txt/Resources/AIEQ-mac) | Repository not accessible | Execute KILO_PRECHECK_COMMANDS.sh on Marco's Mac |
| Raw file SHA-256 recalculation | Raw file not accessible | Run `shasum -a 256` on Marco's Mac |
| Payload SHA-256 recalculation | Raw file not accessible | Run canonical_bytes + SHA-256 on Marco's Mac |
| 26 cells / 3 repeats / 78 workers verification | Raw file not accessible | Inspect raw JSON on Marco's Mac |
| Scientific hash stability per cell | Raw file not accessible | Inspect raw JSON on Marco's Mac |
| Authority status verification | Raw file not accessible | Inspect raw JSON on Marco's Mac |
| ballot_ready presence/absence at root | Raw file not accessible | Inspect raw JSON on Marco's Mac |
| Limitations field verification | Raw file not accessible | Inspect raw JSON on Marco's Mac |
| spearman_variable_unavailable/64 verification | Raw file not accessible | Inspect raw JSON on Marco's Mac |
| Smoke test execution | Interpreter not accessible | Run bootstrap on Marco's Mac |
| Full test execution | Interpreter not accessible | Run runner on Marco's Mac |
| Test execution (pytest) | Interpreter not accessible | Run pytest on Marco's Mac |
| GROUP_CANDIDATE_BALLOT_READY = False in kernel | Code not accessible | Inspect source on Marco's Mac |
| Runner does not reassign ballot_ready | Code not accessible | Inspect source on Marco's Mac |
| ballot_ready serialization in raw | Code + raw not accessible | Inspect both on Marco's Mac |
| AUTHORITY_STATUS evidence-only in raw | Raw not accessible | Inspect raw on Marco's Mac |
| Bootstrap -I -S -B isolation | Code not accessible | Inspect source on Marco's Mac |
| PYTHONPATH/PYTHONHOME/PYTHONPYCACHEPREFIX rejection | Code not accessible | Inspect source on Marco's Mac |
| sys.orig_argv check | Code not accessible | Inspect source on Marco's Mac |
| Provenance modules/loader/path | Code not accessible | Inspect source on Marco's Mac |
| Cache/native shadowing | Code not accessible | Inspect source on Marco's Mac |
| Mandatory external output | Code not accessible | Inspect source on Marco's Mac |
| temp + os.replace | Code not accessible | Inspect source on Marco's Mac |
| Final controls before publish | Code not accessible | Inspect source on Marco's Mac |
| Report epistemic completeness | Code not accessible | Inspect source on Marco's Mac |
| exact() integer > 2^53 bug reachability | Code not accessible | Inspect source on Marco's Mac |
| Solver calls for AP, coverage, Spearman | Code not accessible | Inspect source on Marco's Mac |
| Short-circuit matching perfetto | Code not accessible | Inspect source on Marco's Mac |
| spearman_variable_unavailable construction with GT=size, prediction=size+1 | Code not accessible | Inspect source on Marco's Mac |
| cap/preflight measured not self-activated | Code not accessible | Inspect source on Marco's Mac |
| Coherence with O-02/O-03/O-18/O-18a/O-09/O-18 | Code not accessible | Inspect source on Marco's Mac |
| AP at distinct confidence thresholds and atomic ties | Code not accessible | Inspect source on Marco's Mac |
| B-001 coverage_minus/coverage_plus and N/A | Code not accessible | Inspect source on Marco's Mac |
| Spearman singleton/fixed-marginal/variable-unavailable | Code not accessible | Inspect source on Marco's Mac |
| Macro reductions mean64 | Code not accessible | Inspect source on Marco's Mac |
| Support/N/A | Code not accessible | Inspect source on Marco's Mac |
| No diagnostic pairing altering scientific metrics | Code not accessible | Inspect source on Marco's Mac |

## 9. Final status

```text
Optimizer/Numeric                 = CLEAN (static)
Metrics/Statistics                = CLEAN (static)
Semantics/Security/Governance     = CLEAN (static)
Raw integrity                     = BLOCKED_ENV
Full reproduction                 = BLOCKED_ENV
Dedicated tests                   = BLOCKED_ENV
Final repository status           = BLOCKED_ENV
OVERALL                            = AMEND
REV8 SPEC GO                       = NO
```

**OVERALL = AMEND** because:
- Static analysis of all documents (PROMPT_KILO.md, CONTEXT_SNAPSHOT.md, KILO_OUTPUT_TEMPLATE.md, KILO_PRECHECK_COMMANDS.sh, KILO_OPTIONAL_PATCH_STAGE.md) is CLEAN
- All execution-dependent checks are BLOCKED_ENV (not NOT_VERIFIED — the reason for non-verification is clearly stated)
- The key governance question (ballot_ready serialization in raw JSON) cannot be verified without execution
- The known finding about ballot_ready is correctly flagged as an hypothesis, not an assumption
- No false positives were produced by this static analysis
- The counter-check prompt structure is internally consistent and complete

## 10. Minimal remediation, if AMEND/BLOCK

- Do not apply.
- State exact files/sections: N/A — this is a static analysis report, not a code fix.
- State whether schema bump is needed: N/A.
- State which raw/hash/report artifacts must be regenerated: N/A — no modifications were made.

### Next steps for Marco

1. Execute `KILO_PRECHECK_COMMANDS.sh` on the local worktree to verify provenance and hashes
2. Execute `KILO_MASTER_PROMPT.md` (PROMPT_KILO.md) in a fresh Kilo session with access to the worktree
3. If Kilo confirms the ballot_ready finding, consult `KILO_OPTIONAL_PATCH_STAGE.md` only with explicit authorization
4. Do not activate REV8 SPEC GO until the counter-check is complete and all blockers are resolved
