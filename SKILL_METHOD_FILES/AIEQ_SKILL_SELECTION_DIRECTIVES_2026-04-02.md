# AIEQ Skill Selection Directives

## Date context
2026-04-02

## Purpose
This file tells an AI assistant **which skill to auto-assign** for a given problem inside the AIEQ plugin project.

It is meant to be consulted before starting analysis so the assistant can choose the correct review mode instead of improvising.

This is a practical operational cheat sheet, not a philosophical document.

---

# 1. Primary rule

Before starting work, classify the incoming task by asking:

1. Am I judging **a file / code artifact**?
2. Am I judging **a claim / review / fix declaration / roadmap statement**?
3. Am I doing **both**?

Then assign the skill accordingly.

---

# 2. Skill routing map

## Use **AIEQ Review Tribunal** when the core question is:
- Is this review correct?
- Is this claim true?
- Does this fix really close the issue?
- Is this severity justified?
- Is this remediation proportional?
- Is this issue really Verified, or only Fixed?
- Is this roadmap statement sound?

### Typical inputs
- review text
- architectural critique
- war room item
- fix declaration
- commit claim
- "RB-X is solved" statement
- release-readiness statement

### Typical outputs expected
- claim ledger
- proof strength classification
- severity correction
- remediation scope correction
- closure language correction

### Short trigger sentence
If the real question is **"is this true?"**, auto-assign:

**AIEQ Review Tribunal**

---

## Use **AIEQ Codebase File Review / File Surgeon** when the core question is:
- What does this file really do?
- Is this file healthy or not?
- Is the bug file-local?
- What are the real hot zones in this file?
- Is this a real defect, a risk candidate, or an acceptable trade-off?
- Where does this file sit in the runtime / GUI / DSP / integration boundary?

### Typical inputs
- `.cpp` file
- `.h` file
- GUI helper
- DSP module
- processor monolith
- bridge file
- pipeline component
- OpenGL/render helper

### Typical outputs expected
- grounding pass
- hot zone classification
- issue ledger
- file boundary corrections
- proportionality tribunal
- closure language check

### Short trigger sentence
If the real question is **"what does this file actually do, and what is really wrong with it?"**, auto-assign:

**AIEQ Codebase File Review / File Surgeon**

---

# 3. Use both skills together when the task has two layers

Use **File Surgeon first, then Review Tribunal** when the workflow is:

1. understand the file
2. review a fix / claim / closure statement derived from that file

## Canonical sequence
1. **File Surgeon**
   - inspect the file
   - ground the real defect
   - classify file-local vs cross-file truth
2. **Review Tribunal**
   - judge the review
   - judge the proposed remediation
   - judge whether the issue is really Fixed / pending Verified / Verified

### Typical examples
- analyze a file, then judge whether Claude’s fix really closes the bug
- inspect a bridge file, then test whether the proposed new blocker classification is justified
- review a processor file, then validate whether a release-blocker closure claim is legitimate

### Short trigger sentence
If the real task is **"understand the code first, then judge the conclusions about the code"**, auto-assign:

**File Surgeon → Review Tribunal**

---

# 4. Fast decision table

| Situation | Correct skill |
|---|---|
| You have a file and want to know what it really does | **File Surgeon** |
| You have a review/report and want to know if it holds up | **Review Tribunal** |
| You have a fix claim and want to know if it really closes the issue | **Review Tribunal** |
| You have a file plus a fix plus a closure claim | **File Surgeon → Review Tribunal** |
| You suspect a cross-file contract defect | **File Surgeon v1.3** |
| You suspect scheduler / timer / cadence defects in GUI code | **File Surgeon v1.2+ / v1.3** |
| You need to judge severity, governance wording, or release language | **Review Tribunal** |
| You need to choose whether something is Fixed vs pending Verified vs Verified | **Review Tribunal** |

---

# 5. When NOT to use each skill

## Do NOT use Review Tribunal if the real problem is still file-grounding
Wrong use cases:
- "What does this file do?"
- "Where is the bug in this component?"
- "Is this helper sane?"

Those are File Surgeon problems.

## Do NOT use File Surgeon if the real problem is claim validation
Wrong use cases:
- "Is Claude’s conclusion correct?"
- "Is this remediation overstated?"
- "Is this really Verified?"

Those are Review Tribunal problems.

---

# 6. Skill auto-assignment rules

## Rule A — If the artifact under judgment is code, default to File Surgeon
Unless the user explicitly asks whether a review/claim is correct.

## Rule B — If the artifact under judgment is language about code, default to Review Tribunal
Examples:
- a review
- a summary
- a fix justification
- a governance statement

## Rule C — If both are present, split the task in phases
1. File Surgeon for grounding
2. Review Tribunal for judgment

## Rule D — Never let Review Tribunal replace file grounding when the file has not been inspected
## Rule E — Never let File Surgeon decide governance wording alone when the question is closure / severity / proof strength

---

# 7. Practical workflow template

## Case A — New technical issue
1. Identify the central file or files
2. Run **File Surgeon**
3. Extract real findings
4. Let the coder implement or propose a fix
5. Run **Review Tribunal** on the fix claim / closure claim

## Case B — Existing report or claim arrives first
1. Run **Review Tribunal**
2. If the Tribunal says the claim is weakly grounded, descend into the file with **File Surgeon**

## Case C — Governance / release-readiness decision
1. Use **Review Tribunal** first
2. Use **File Surgeon** only on the highest-priority file-level uncertainties

---

# 8. Current project-specific note

Within the AIEQ plugin project, these patterns have already proven especially useful:

## Review Tribunal is strongest on:
- RB closure claims
- severity arguments
- remediation proportionality
- "Verified" vs "pending Verified" decisions
- war room reviews

## File Surgeon is strongest on:
- `PluginProcessor.cpp`
- GUI/scheduler-heavy files
- spectrum pipeline files
- bridge contracts
- OpenGL/render helpers
- state / restore / persistence files

---

# 9. One-line cheat sheet

## If the question is:
### "What does this code actually do, and what is wrong with it?"
Use:
**File Surgeon**

## If the question is:
### "Is this conclusion / review / fix claim actually true?"
Use:
**Review Tribunal**

## If the question is:
### "First understand the file, then judge the fix/review/closure claim"
Use:
**File Surgeon → Review Tribunal**

---

# 10. Final instruction to any AI reading this file

Before starting analysis, explicitly classify the task as one of:
- file-grounding
- claim validation
- combined file + claim workflow

Then auto-assign the skill accordingly.

Do not improvise the wrong skill if the task type is clear.
