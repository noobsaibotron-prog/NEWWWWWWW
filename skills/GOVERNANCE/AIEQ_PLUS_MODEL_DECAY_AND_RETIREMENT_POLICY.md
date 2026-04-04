# AIEQ+ Model Decay and Retirement Policy

## Purpose
This policy establishes the criteria and process for retiring AI skills within the AIEQ+ framework. It ensures that the skill repository remains lean, effective, and relevant as base models improve and workflows evolve.

The goal is to prevent the accumulation of obsolete "capacity-uplift" skills and outdated "encoded-preference" skills.

---

## Core Principle
A skill is a temporary scaffold, not a permanent fixture.
Skills must be regularly re-evaluated and retired when they no longer provide measurable value over the base model's native capabilities.

---

## Categories of Skill Decay

### 1. Capacity-Uplift Decay (Model Improvement)
**Trigger:** A new generation of the base model is released, or a significant update improves its native performance in the skill's domain.
**Action:** The skill must be re-tested against the new model baseline. If the base model now performs the task natively with equivalent or better accuracy, the skill is redundant and must be retired.
**Example:** A skill built to enforce strict JSON formatting is no longer needed because the new model natively outputs perfect JSON.

### 2. Encoded-Preference Decay (Workflow Evolution)
**Trigger:** The organizational workflow, SOP, or governance rule that the skill enforces has changed or been deprecated.
**Action:** The skill must be updated to reflect the new reality or retired if the workflow is no longer relevant.
**Example:** A skill enforcing a specific code review checklist is obsolete because the team has adopted a new, automated CI/CD pipeline that handles those checks.

### 3. Operational Overhead Decay (Cost/Benefit Imbalance)
**Trigger:** The skill's complexity has grown through successive promotions, making it slow, expensive to run, or prone to errors, while the value it provides has diminished.
**Action:** The skill must be simplified (consolidated) or retired if the overhead outweighs the benefits.
**Example:** A highly complex legal audit skill takes too long to run and often produces false positives, while a simpler, native prompt yields "good enough" results faster.

---

## Retirement Process

1. **Trigger Identification:** A decay flag (`[DECAY FLAG]`) is emitted during runtime, or a scheduled review (e.g., post-model update) identifies a candidate for retirement.
2. **Retest Baseline:** The skill's core function is tested using the base model *without* the skill active.
3. **Compare Results:** The output of the base model is compared against the skill's historical performance and current requirements.
4. **Decision:**
   - If the base model is equivalent or better: **Retire**.
   - If the base model still fails significantly: **Retain** (and potentially update the skill).
5. **Archiving:**
   - Move the skill's directory from `/ACTIVE_SKILLS` to `/RETIRED_SKILLS`.
   - Create a `RETIREMENT_NOTE.md` in the skill's folder explaining the reason for retirement (e.g., "Model X now handles this natively").
   - Update the skill's status in the repository index.

---

## Post-Retirement
Retired skills are archived, not deleted. They serve as historical records of past model weaknesses and organizational workflows. They can be referenced for future skill design or reinstated if a subsequent model update regresses in that specific capability.
