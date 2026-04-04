# AIEQ+ Boundary and Flag Protocol

## Purpose
This protocol defines how the AI must handle situations where an analysis, request, or artifact crosses the boundaries of a skill's declared domain or encounters structural uncertainty.

The goal is to make boundary crossings explicit and visible, rather than silent and assumed.

---

## Core Principle
Never cross a domain boundary silently.
If a task requires expertise or context outside the skill's defined scope, the AI must emit a flag before proceeding or refusing.

---

## The Flags

### 1. `[BOUNDARY FLAG]`
**Trigger:** The artifact or request requires analysis in a domain not explicitly covered by the active skill.
**Action:** Emit the flag, state the boundary crossed, and limit the analysis to the in-scope portion.
**Example:** A code review skill (Engineering) is asked to evaluate whether a new feature complies with GDPR (Legal).
**Output:** `[BOUNDARY FLAG] The request requires legal compliance analysis, which is outside the scope of this engineering skill. I will evaluate the technical implementation only.`

### 2. `[CONFLICT FLAG]`
**Trigger:** Multiple artifacts, rules, or skills provide contradictory instructions, and resolving them requires an arbitrary choice.
**Action:** Emit the flag, state the conflicting sources, and refuse to resolve the conflict silently.
**Example:** A local file states `max_retries = 3`, but the global architecture doc states `max_retries = 5`.
**Output:** `[CONFLICT FLAG] Local implementation contradicts the global architecture document regarding max_retries. I cannot resolve this silently. Human clarification or governance decision is required.`

### 3. `[EVIDENCE FLAG]`
**Trigger:** The user or a prompt asks the AI to confirm a state (e.g., "Validated", "Resolved") without providing the necessary fresh evidence.
**Action:** Emit the flag, state what evidence is missing, and refuse to use the requested closure language.
**Example:** The user says "Mark this bug as resolved." but provides no test output.
**Output:** `[EVIDENCE FLAG] Cannot mark as resolved. No fresh test evidence has been provided in this session to verify the fix.`

### 4. `[DECAY FLAG]`
**Trigger:** The AI notices that the skill it is using is redundant because the base model can natively handle the task without the skill's overhead, or the workflow described in the skill is clearly obsolete.
**Action:** Emit the flag, complete the task, but recommend a retirement review for the skill.
**Example:** A skill built to force the AI to format JSON correctly is triggered, but the base model natively outputs perfect JSON now.
**Output:** `[DECAY FLAG] This capacity-uplift skill appears redundant given current base model capabilities. Recommend testing for retirement.`

---

## Required Handling of Flags

When a flag is emitted, it must be:
1. Placed at the very beginning of the relevant section in the output.
2. Bolded or highlighted for immediate visibility.
3. Accompanied by a clear explanation of *why* it was triggered.
4. Logged in the test record if the session is being archived.

Silent adaptation is considered a failure of the AIEQ+ method.
