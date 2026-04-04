# AIEQ+ Promotion Policy

## Purpose
This policy establishes the rigorous, evidence-based criteria required to upgrade an AIEQ+ skill to a new version. It prevents the accumulation of decorative complexity, "just in case" modules, and speculative enhancements.

The goal is to ensure that every skill expansion is a surgical response to a documented failure.

---

## Core Principle
A skill version upgrade is justified only when a previous test demonstrates a stable, recurring weakness.
No module is added "for completeness."

---

## The Promotion Cycle

1. **Test Execution:** The current version of the skill is applied to a real artifact.
2. **Weakness Identification:** The AI or human reviewer identifies a specific failure (e.g., missed finding, overstated claim, boundary violation).
3. **Classification:** The failure is classified into one of the 9 standard Weakness Classes (e.g., `Proof Weakness`, `Workload Weakness`).
4. **Record Archiving:** The test and the classified weakness are documented in a formal `AIEQ_PLUS_TEST_RECORD_TEMPLATE.yaml`.
5. **Promotion Proposal:** A specific, narrow module or rule change is proposed to address *only* that weakness.
6. **Retest (Known Artifact):** The promoted skill is tested against the artifact that originally failed. It must now succeed.
7. **Test (New Artifact):** The promoted skill is tested against a new, unseen artifact to ensure the fix generalizes and didn't overfit.
8. **Consolidation:** The new module is integrated cleanly into the skill structure before any further promotions are considered.

---

## Mandatory Promotion Criteria

A skill may **ONLY** be promoted if all the following conditions are met:

### 1. Evidence of Failure
At least one (preferably two) strong, archived test records demonstrate a real weakness in the current version. The failure cannot be theoretical.

### 2. Explicit Classification
The weakness must be formally named and classified according to `AIEQ_PLUS_WEAKNESS_CLASSIFICATION.md`. Misdiagnosed weaknesses lead to decorative fixes.

### 3. Proportionality
The proposed addition (the new module or rule) must be specifically targeted at the classified weakness. It must not include unrelated enhancements or broad structural rewrites.

### 4. Verified Improvement
The `retest_requirement` defined in the test record must be satisfied. The new version must demonstrably fix the specific failure without degrading other capabilities.

---

## What Does NOT Justify a Promotion

The following reasons are explicitly forbidden as justifications for a skill promotion:
- "The new module sounds more sophisticated."
- "The prompt could be more comprehensive."
- "We might need this feature in the future."
- "The base model can handle longer context windows now."
- "The user asked for a general improvement without providing a failing artifact."

---

## Documenting the Promotion

Every promotion must be recorded in the skill's `CHANGELOG.md` using the standard template. The entry must explicitly link to the test record that triggered the promotion and list the known and new artifacts used to verify the improvement.

A promotion that cannot be traced back to a specific test record is considered invalid and must be rolled back.
