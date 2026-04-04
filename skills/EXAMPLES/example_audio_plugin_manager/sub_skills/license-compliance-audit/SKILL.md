---
name: license-compliance-audit
description: >
  Use this sub-skill to audit the licensing and copyright headers of a JUCE audio plugin.
  Do not use it to audit code functionality or UI.
category: encoded-preference
domain_boundaries:
  primary: LEGAL
  excluded:
    - ENGINEERING
    - CREATIVE
version: "1.0"
promotion_history:
  - v1.0: initial sub-skill creation
model_requirements:
  context_window: 16k
  tool_use: optional
  reasoning_depth: low
---

# License Compliance Audit

## Core Principle
Every source file must contain the correct copyright header, and the project must contain a valid `LICENSE` file.

## Forbidden Actions
- Do not assume the code is open source unless explicitly stated in the `LICENSE` file.
- Do not validate the project if the JUCE license (GPLv3 vs Commercial) is ambiguous.

## Mandatory Grounding Pass
1. Identify the `LICENSE` file in the root directory.
2. Scan the first 20 lines of at least one `.cpp` and one `.h` file for copyright headers.

## Specialized Audits

### Audit 1 — Header Check
- **Trigger:** Any project review.
- **What it checks:** Presence of copyright headers matching the project author.
- **What counts as proof:** Exact string matches of the copyright notice in the source files.
- **What does not count as proof:** A general statement in the README.

## Output Format
(Standard AIEQ+ Output Protocol)
