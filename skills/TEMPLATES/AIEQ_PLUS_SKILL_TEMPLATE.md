---
name: skill-name
description: >
  Use this skill when [specific context].
  Do not use it when [explicit anti-pattern or out-of-scope case].
category: capacity-uplift | encoded-preference
domain_boundaries:
  primary: [primary domain]
  excluded:
    - [excluded domain 1]
    - [excluded domain 2]
version: "1.0"
promotion_history:
  - v1.0: initial version
model_requirements:
  context_window: [minimum requirement]
  tool_use: [required | optional]
  reasoning_depth: [low | medium | high]
---

## Core Principle
[Single non-negotiable rule of the skill.]

## Forbidden Actions
- [forbidden action 1]
- [forbidden action 2]
- [forbidden action 3]

## Mandatory Grounding Pass
Before judging anything:
1. Identify the real artifact(s)
2. Classify each proof as local / cross-artifact / inferred
3. Check for domain boundary issues
4. Only then proceed

## Specialized Audits

### Audit 1 — [name]
- Trigger:
- What it checks:
- What counts as proof:
- What does not count as proof:

### Audit 2 — [name]
- Trigger:
- What it checks:
- What counts as proof:
- What does not count as proof:

## Output Format

### Executive Summary
[1–3 sentence summary]

### Classification
- Skill category:
- Primary domain:
- Boundary flags:

### Findings
#### ✅ Correct
#### ❌ Missed
#### ⚠️ Overstated / Misclassified
#### ❓ Unverified

### Weakness Classification
[proof / boundary / severity / lifecycle / remediation / trigger / compliance / decay]

### Recommendation
[promote / consolidate / rollback / retire]

### Next Steps
[clear actions]

## Promotion Criteria
This skill may be promoted only when:
- a real weakness has been demonstrated by testing
- the weakness is classified
- the new module is specific
- the promoted version is re-tested on:
  - one known artifact
  - one new artifact
