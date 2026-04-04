# AIEQ+ Repo Map

This index maps the AIEQ+ repository structure.
Last updated: 2026-04-04 (Phase 3 complete)

---

## /METHOD_BASE
The constitution of the method. Defines principles, vocabulary, and rules.

| File | Status | Description |
|---|---|---|
| `AIEQ_PLUS_METHOD_BASE.md` | Active | Core principles and canonical cycle |
| `AIEQ_PLUS_GLOSSARY.md` | Active | Core terminology and shared vocabulary |
| `AIEQ_PLUS_PROMOTION_POLICY.md` | Active | Evidence-based promotion criteria and cycle |

## /RUNTIME
Executable prompt fragments and protocols for the agent while working.

| File | Status | Description |
|---|---|---|
| `AIEQ_PLUS_SYSTEM_PROMPT_CORE.md` | Active | The core system prompt |
| `AIEQ_PLUS_RUNTIME_QUICKSTART.md` | Active | Quick operational discipline |
| `AIEQ_PLUS_OUTPUT_PROTOCOL.md` | Active | Standard output structure and forbidden behaviors |
| `AIEQ_PLUS_BOUNDARY_AND_FLAG_PROTOCOL.md` | Active | Rules for emitting flags |

## /TEMPLATES
Infrastructure for creating new skills and tracking tests.

| File | Status | Description |
|---|---|---|
| `AIEQ_PLUS_SKILL_TEMPLATE.md` | Active | Structure for new skills |
| `AIEQ_PLUS_TEST_RECORD_TEMPLATE.yaml` | Active | Format for archiving tests |
| `AIEQ_PLUS_EVALS_TEMPLATE.json` | Active | Evaluation structure |
| `AIEQ_PLUS_CHANGELOG_TEMPLATE.md` | Active | Version history tracking |
| `AIEQ_PLUS_SKILL_FRONTMATTER_TEMPLATE.yaml` | Planned | Metadata template |

## /GOVERNANCE
Formalizes states, criteria, and retirement policies.

| File | Status | Description |
|---|---|---|
| `AIEQ_PLUS_STATUS_MODEL.md` | Active | Governance states (Drafted > Retired) |
| `AIEQ_PLUS_WEAKNESS_CLASSIFICATION.md` | Active | Taxonomy of 9 skill weakness classes |
| `AIEQ_PLUS_MULTI_SKILL_CONFLICT_PROTOCOL.md` | Active | Handling skill conflicts |
| `AIEQ_PLUS_MODEL_DECAY_AND_RETIREMENT_POLICY.md` | Active | Retiring obsolete skills |
| `AIEQ_PLUS_HUMAN_IN_THE_LOOP_PROTOCOL.md` | Active | When human intervention is required |

## /DOMAINS
Domain-specific specializations (Legal, Medical, Strategy, Finance, Creative, Engineering).
Status: Scaffolding ready. No domain modules created yet.

## /EXAMPLES
Complete examples of skills, evals, and tests for training and reference.

| Example | Domain | Version | Description |
|---|---|---|---|
| `example_suno_prompt_audit/` | CREATIVE | v1.1 | Full lifecycle example: Suno V5 prompt audit skill with 2 audits, changelog, evals, and test record |

## /ACTIVE_SKILLS
Live, operational skills currently in use.
Status: Scaffolding ready. No active skills deployed yet.

## /RETIRED_SKILLS
Archived skills that are no longer recommended for active use.
Status: Scaffolding ready.
