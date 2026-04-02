# AIEQ Skill Implementation Method Base

## Date context
2026-04-02

## Purpose
This file is not a verbatim export of the chat UI.
It is the distilled operational base derived from the full conversation and its real test cycles.
Its role is to preserve the most reliable method for:
- creating new skills
- testing skills on real code / real fix planning
- promoting or rejecting new skill modules
- archiving evidence for future skill development

This file is meant to be the canonical starting point for future skill evolution.

---

# 1. Core Method

The correct method is iterative and empirical.
A skill should not be enlarged because a new idea sounds intelligent.
A skill should only be enlarged when a real test exposes a recurring limit.

## Canonical cycle
1. Create an initial skill version
2. Use it on a real code/review/planning scenario inside the actual plugin codebase
3. Observe what it gets right
4. Observe what it misses, overstates, or misclassifies
5. Archive the test as a skill test file
6. Promote the skill only if the missing capability is:
   - real
   - repeated
   - important enough
7. Re-test the promoted skill on a new or previously reviewed case
8. Consolidate before promoting again

---

# 2. Fundamental Design Rules for Skills

## Rule A — Skills must be grounded on real artifacts
A skill should review or validate:
- actual files
- actual patches
- actual claims
- actual roadmap statements
- actual runtime test plans

Never optimize a skill around imagined use.

## Rule B — Promotion must be earned
A skill version upgrade is justified only when a previous test demonstrates a stable weakness.
Do not add modules “just in case.”

## Rule C — Keep file-local truth separate from cross-file truth
When reviewing files, always separate:
- file-local proof
- cross-file partial proof
- cross-file verified defect

This prevents overclaiming.

## Rule D — Keep code-fix truth separate from governance truth
Always separate:
- fixed in code
- pending verified
- verified
- closed

Skills must never collapse these into one status.

## Rule E — Archive tests systematically
Every meaningful skill test should be preserved in a dedicated file if it teaches something reusable.

---

# 3. What Makes a Good Skill Test Candidate

A conversation segment becomes a strong skill test candidate when at least one of these is true:
- the skill found a real bug that generic review would likely miss
- the skill corrected an overstated failure class
- the skill prevented a bad remediation choice
- the skill separated a known issue from a fake “new discovery”
- the skill materially improved proof discipline
- the skill revealed a recurring category of misses that can justify a new module

A segment should usually NOT be archived if it is only:
- generic praise
- vague planning with no concrete artifact
- a review with no reusable learning
- a restatement of previous conclusions without new discrimination

---

# 4. How to Archive Skill Tests Intelligently

## Required contents of each skill test file
Each archived test should contain:
- skill name and version
- date context
- branch / source of truth
- files or material under review
- why the test was chosen
- main findings produced by the skill
- what the skill got especially right
- what remained unverified
- what this test taught us about the skill
- whether the test supports promotion, consolidation, or rollback

## Archive discipline
Do not save raw chat fragments only.
Prefer structured records that preserve:
- artifact
- reasoning outcome
- what changed in the skill afterwards

---

# 5. Skill Family Lessons Learned So Far

## AIEQ Review Tribunal
The Review Tribunal is strongest when used on:
- review texts
- roadmap claims
- fix declarations
- “this commit closes X” statements

Its job is forensic validation, not file review.
It must remain strict about:
- evidence strength
- severity inflation
- remediation scope
- closure language

## AIEQ Codebase File Review / File Surgeon
The File Surgeon is strongest when used on:
- actual source files
- bridge files
- GUI-heavy files
- processor-scale files
- files with timer/scheduler or cross-file integration complexity

It must remain file-first, not architecture-first.
It should only recommend redesign when a local fix is genuinely insufficient.

---

# 6. Skill Promotion Logic Observed in Practice

## v1.0 -> v1.1
Promotion justified by need for:
- Thread Lifecycle Audit
- Parameter-Domain Consistency Pass
- Integration Hook Check
- Known-Issue Dedup Pass
- Closure Language Filter

## v1.1 -> v1.2
Promotion justified by GUI-heavy reviews that exposed the need for:
- GUI Timer / Scheduler Consistency Pass

## v1.2 -> v1.3
Promotion justified by bridge-file and scheduler tests that exposed the need for:
- Workload-Class Partitioning Audit
- Cross-File Contract Proof Floor

---

# 7. Practical Rules for Future Skill Development

## Rule 1
Do not create a new version merely because a module sounds sophisticated.
Require at least one strong test and preferably two.

## Rule 2
Prefer small version upgrades with one or two meaningful modules over large decorative upgrades.

## Rule 3
When a new module is added, retest it on:
- one previously known file
- one new file
This distinguishes explanation-only improvement from genuine finding improvement.

## Rule 4
When a test reveals a skill weakness, classify it before changing the skill:
- proof weakness
- boundary weakness
- workload/scheduler weakness
- lifecycle weakness
- runtime severity weakness
- remediation guidance weakness

## Rule 5
Always preserve honesty about uncertainty.
A skill becomes stronger when it refuses to overclaim.

---

# 8. Recommended Structure for Future Skill Families

When building future skills, prefer this architecture:

## A. Core identity
What the skill is for

## B. Non-negotiable rules
What it must never confuse

## C. Mandatory grounding pass
What it must inspect before judging anything

## D. Specialized audits
Only modules justified by prior tests

## E. Output format
Stable, severe, and comparable across uses

## F. Promotion criteria
When the skill should be upgraded

---

# 9. What Should Be Saved Instead of Raw Full Chat Dumps

A full verbatim chat transcript can be useful historically, but it is not the best operational base.
The best operational base is:
- a distilled method file (this file)
- the skill prompt files themselves
- the archived skill test files
- optional curated transcripts of especially important turning points

Therefore, the recommended stack is:
1. method base file
2. prompt files by version
3. skill test files
4. only then, optional raw transcript exports if ever needed

---

# 10. Final Operational Guidance

The method that has emerged as most correct is this:
- create narrowly
- test on real code
- archive systematically
- promote cautiously
- retest after promotion
- consolidate before the next upgrade

This is the correct foundation for future skill development in the AIEQ plugin project.
