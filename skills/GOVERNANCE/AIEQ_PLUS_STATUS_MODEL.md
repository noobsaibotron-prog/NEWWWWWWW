# AIEQ+ Status Model

## Purpose
This file defines the governance states used across the AIEQ+ system.

These states must never be collapsed into one another.

---

## Core Rule
A later state may imply earlier work was done.
An earlier state never implies a later one.

For example:
- Tested does not mean Validated
- Validated does not mean Approved
- Reviewed does not mean Correct
- Drafted does not mean Accepted

---

## Universal Status Model

### 1. Drafted
The artifact, skill, report, or procedure has been created in first form.

**Implies:**
- something exists

**Does not imply:**
- review
- correctness
- testing
- approval

---

### 2. Reviewed
The artifact has been examined by a reviewer or by a review protocol.

**Implies:**
- some level of examination occurred

**Does not imply:**
- correctness
- empirical testing
- validation
- approval

---

### 3. Tested
The artifact or skill has been exercised against one or more real artifacts, scenarios, or evals.

**Implies:**
- evidence has been generated

**Does not imply:**
- strong adequacy
- external validity
- approval

---

### 4. Validated
The evidence is strong enough to support the intended claim within the declared scope.

**Implies:**
- the artifact has passed meaningful scrutiny for its intended purpose

**Does not imply:**
- organizational approval
- permanence
- immunity from regression

---

### 5. Approved
A human or governance authority has accepted the artifact for use at the relevant level.

**Implies:**
- formal acceptance

**Does not imply:**
- perfection
- eternal correctness

---

### 6. Retired
The artifact or skill is no longer recommended for active use.

**Possible reasons:**
- redundancy
- obsolescence
- workflow mismatch
- model improvement
- better replacement

---

## Coding-State Mapping Example

| Coding workflow | AIEQ+ state |
|---|---|
| Written | Drafted |
| Code reviewed | Reviewed |
| Unit/integration tested | Tested |
| Verified against intended behavior | Validated |
| Accepted into stable workflow | Approved |
| No longer used | Retired |

---

## Skill Governance Mapping Example

| Skill lifecycle | AIEQ+ state |
|---|---|
| Initial draft written | Drafted |
| Reviewed for structure/scope | Reviewed |
| Tested on real artifacts | Tested |
| Promotion evidence accepted | Validated |
| Adopted into active stack | Approved |
| Removed from active use | Retired |

---

## Hard Rules
- Never say “complete” when you mean “drafted”
- Never say “verified” when you mean “tested”
- Never say “approved” when you only mean “validated”
- Never skip states silently
- Always state the current governance level explicitly
