# AIEQ Skill Selection Quickstart

## Purpose
Ultra-short routing file for any AI working on the AIEQ plugin project.
Consult this file first, then auto-assign the correct skill.

---

# 1. First question
Classify the task before doing anything else:

- Is this **code/file grounding**?
- Is this **claim/review/fix validation**?
- Is this **both**?

---

# 2. Routing rules

## If the real question is:
### "What does this file actually do, and what is really wrong with it?"
Use:
# **File Surgeon**

Typical use:
- `.cpp` / `.h` review
- DSP module
- GUI file
- bridge file
- pipeline file
- OpenGL/render helper

---

## If the real question is:
### "Is this conclusion / review / fix claim actually true?"
Use:
# **Review Tribunal**

Typical use:
- review text
- war room claim
- severity claim
- remediation claim
- `Verified` / closure claim
- release-readiness statement

---

## If the real task is:
### "First understand the code, then judge the fix / review / closure claim"
Use:
# **File Surgeon → Review Tribunal**

This is the default for serious issues.

---

# 3. Fast examples

| Situation | Correct skill |
|---|---|
| Analyze a source file | **File Surgeon** |
| Judge whether a review is correct | **Review Tribunal** |
| Judge whether a fix really closes RB-X | **Review Tribunal** |
| Analyze a file, then judge the fix/closure claim | **File Surgeon → Review Tribunal** |
| Suspected cross-file contract defect | **File Surgeon v1.3** |
| Governance wording (`Fixed` vs `Verified`) | **Review Tribunal** |

---

# 4. Hard rules

## Never use Review Tribunal as a substitute for file grounding
If the file has not been inspected, and the task is file-centric, use **File Surgeon** first.

## Never use File Surgeon alone to decide governance language
If the question is:
- severity
- proportionality
- closure
- `Verified` status
then use **Review Tribunal**.

## If both file truth and claim truth matter, split the work in phases
1. File Surgeon
2. Review Tribunal

---

# 5. Project-specific bias
Within AIEQ, prefer:

## File Surgeon for:
- `PluginProcessor.cpp`
- GUI/scheduler files
- spectrum pipeline files
- bridge files
- OpenGL/render helpers
- restore/persistence files

## Review Tribunal for:
- RB closure claims
- severity disputes
- remediation scope disputes
- `Fixed` vs `pending Verified` vs `Verified`

---

# 6. Final instruction
Before starting analysis, explicitly state:

- task type = file-grounding / claim-validation / combined
- chosen skill = File Surgeon / Review Tribunal / File Surgeon → Review Tribunal

Then proceed.
