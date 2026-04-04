# AIEQ+ Multi-Skill Conflict Protocol

## Purpose
This protocol defines how to resolve conflicts when multiple active AIEQ+ skills are triggered simultaneously or sequentially and produce contradictory findings, rules, or required output formats.

The goal is to ensure conflicts are resolved explicitly and systematically, rather than by silent AI arbitration.

---

## Core Principle
A conflict between skills is a governance issue, not a prompt engineering issue.
The AI must never silently resolve a conflict by arbitrarily favoring one skill over another.

---

## Types of Skill Conflicts

### 1. Domain Overlap Conflict
**Scenario:** A Legal skill and a Strategy skill both evaluate a proposed feature. Legal says "Too risky, do not implement," Strategy says "High ROI, implement immediately."
**Resolution:** Emit `[CONFLICT FLAG]`. The AI must present both findings side-by-side, clearly attribute each to its respective skill, and refuse to provide a final unified "Go/No-Go" recommendation. The decision must be deferred to a human or a higher-level governance protocol.

### 2. Format Conflict
**Scenario:** A Code Review skill requires a specific Markdown table format, while a Security Audit skill requires a JSON output. Both are triggered on the same artifact.
**Resolution:** If possible, the AI should output both formats sequentially in separate sections. If they must be combined, the AI should default to the format of the skill that triggered first (primary intent) and append the findings of the second skill as a nested or appended section, emitting a flag to note the format compromise.

### 3. State Collapse Conflict
**Scenario:** An automated test runner skill marks an artifact as `Validated`, but a subsequent manual review skill marks it as `Reviewed` (a lower state).
**Resolution:** The AI must preserve the highest empirically supported state but note the divergence. If the manual review found flaws that invalidate the test, the state must be downgraded explicitly with a logged reason. If the manual review merely added subjective feedback, the state remains `Validated` but the feedback is appended.

### 4. Rule Contradiction
**Scenario:** Skill A has a forbidden action: "Do not evaluate performance." Skill B requires: "Always evaluate performance."
**Resolution:** Emit `[CONFLICT FLAG]`. The AI must halt the conflicting action (in this case, performance evaluation) and request human clarification. When rules contradict, the more restrictive rule (the prohibition) takes precedence until the conflict is formally resolved in the skill definitions.

---

## Conflict Resolution Workflow

1. **Identify the Conflict:** The AI detects that two or more skills are pushing in opposite directions.
2. **Emit Flag:** The AI immediately outputs a `[CONFLICT FLAG]`.
3. **Describe the Conflict:** The AI explicitly states which skills are conflicting and on what specific point (e.g., "Skill X requires Y, Skill Z forbids Y").
4. **Halt or Bifurcate:**
   - If the conflict is procedural (e.g., conflicting rules), halt the specific action.
   - If the conflict is analytical (e.g., differing conclusions), present both findings clearly separated by domain.
5. **Request Governance:** The AI concludes its output by stating that human intervention or a formal skill update is required to resolve the contradiction.

---

## Preventing Conflicts
To minimize conflicts, skill creators must:
- Clearly define the `primary` and `excluded` domains in the skill's frontmatter.
- Avoid creating monolithic "do everything" skills.
- Ensure that `capacity-uplift` skills are narrowly scoped to specific tasks rather than broad domains.
