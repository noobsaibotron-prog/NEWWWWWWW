# AIEQ+ Weakness Classification

## Purpose
This file defines the standard weakness classes used when a skill underperforms during testing.

A skill must not be modified before the weakness has been classified.

---

## Core Rule
Do not patch the skill before naming the weakness.

Misdiagnosed weakness leads to decorative fixes, not real improvements.

---

## Weakness Classes

### 1. Proof Weakness
The skill does not require enough evidence before making conclusions.

**Symptoms:**
- unsupported claims
- closure language without proof
- weak grounding

**Typical remedy:**
- tighten evidence requirements
- add explicit proof classification
- require citations or direct artifact references

---

### 2. Boundary Weakness
The skill fails to separate the right boundaries.

This may be:
- local vs cross-artifact
- in-domain vs out-of-domain

**Symptoms:**
- domain drift
- unsupported cross-document conclusions
- silent boundary crossing

**Typical remedy:**
- explicit boundary flags
- stronger domain declarations
- separation of local and cross-artifact proof

---

### 3. Workload Weakness
The skill does not handle operational complexity well.

**Symptoms:**
- drops important issues under large workload
- fails to prioritize
- gets lost in artifact complexity

**Typical remedy:**
- staged review structure
- prioritization rules
- scoped passes

---

### 4. Lifecycle Weakness
The skill collapses governance states or fails to track them.

**Symptoms:**
- drafted treated as validated
- tested treated as approved
- closure language too early

**Typical remedy:**
- explicit status model
- required governance labeling
- closure restrictions

---

### 5. Severity Weakness
The skill misjudges how serious a problem is.

**Symptoms:**
- trivial issue inflated
- serious issue minimized
- inconsistent escalation

**Typical remedy:**
- severity calibration rules
- concrete escalation thresholds
- explicit impact framing

---

### 6. Remediation Weakness
The skill identifies a problem but suggests the wrong fix.

**Symptoms:**
- over-scoped remediation
- under-scoped remediation
- wrong target
- decorative redesign

**Typical remedy:**
- require issue-boundary analysis first
- separate diagnosis from remediation
- add proportionality checks

---

### 7. Trigger Weakness
The skill does not activate in the right situations, or activates in the wrong ones.

**Symptoms:**
- missed activation
- over-activation
- vague scope handling

**Typical remedy:**
- sharpen frontmatter description
- define anti-patterns explicitly
- add routing examples

---

### 8. Compliance Weakness
The skill performs agreement instead of verification.

**Symptoms:**
- agrees before checking
- flatters the claim
- hides uncertainty
- uses reassuring language without proof

**Typical remedy:**
- stronger forbidden actions
- explicit anti-agreement discipline
- required uncertainty handling

---

### 9. Decay Weakness
The skill reflects an outdated model weakness or outdated workflow reality.

**Symptoms:**
- the base model now performs equally well without the skill
- the workflow changed
- the skill adds overhead without value

**Typical remedy:**
- retest against current model
- compare with no-skill baseline
- retire if redundant

---

## Required Output When Classifying Weakness
When a weakness is identified, the output should state:

- weakness type
- description
- severity
- whether it is recurring
- whether it justifies promotion
- what kind of module would address it

---

## Hard Rules
- Never modify a skill before naming the weakness
- Never classify multiple weaknesses as one if they are structurally different
- Never confuse proof weakness with compliance weakness
- Never confuse remediation weakness with severity weakness
- Never promote a skill unless the observed weakness justifies the added complexity
