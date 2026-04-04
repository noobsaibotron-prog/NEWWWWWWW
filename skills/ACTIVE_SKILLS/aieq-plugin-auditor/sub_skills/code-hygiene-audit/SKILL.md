---
name: code-hygiene-audit
description: >
  Use this sub-skill to audit the code hygiene, naming conventions, dead code, TODO/FIXME comments, documentation, and include guards of the AIEQ plugin codebase.
metadata:
  category: encoded-preference
  domain_boundaries:
    primary: ENGINEERING
    excluded:
      - LEGAL
      - MARKETING
  version: "1.0"
  promotion_history:
    - v1.0: Initial version
  model_requirements:
    context_window: 128k
    tool_use: optional
    reasoning_depth: high
---

# Code Hygiene Audit (Sub-Skill)

## Core Principle
Clean code is maintainable code. This sub-skill evaluates the structural health of the C++ codebase, focusing on consistency, dead code elimination, and documentation quality. It enforces a professional standard for a premium commercial plugin.

## Forbidden Actions
- Do not evaluate DSP logic, thread safety, or GUI performance here.
- Do not accept "magic numbers" without explanation.
- Do not validate files with excessive commented-out code.

## Mandatory Grounding Pass
Before judging hygiene:
1. Identify the coding standard (e.g., camelCase vs PascalCase, `m_` prefixes for members).
2. Scan for `#include` hygiene (forward declarations vs. full includes).
3. Look for technical debt markers (`TODO`, `FIXME`, `HACK`).

## Specialized Audits

### Audit 1: Naming Conventions and Consistency
- **Trigger:** Reading any `.cpp` or `.h` file.
- **Check:** Are class names, methods, and variables consistently named? Are member variables clearly distinguished (e.g., `m_` or trailing `_`)? Are magic numbers replaced with `constexpr` or `enum`?
- **Pass:** Consistent naming and no unexplained magic numbers.
- **Fail:** Inconsistent naming, or hardcoded magic numbers.

### Audit 2: Include Hygiene and Forward Declarations
- **Trigger:** Reading header files (`.h`).
- **Check:** Does the header use `#pragma once` or include guards? Are forward declarations used where possible instead of `#include` to reduce compilation time? Are includes ordered logically (local, project, JUCE, standard library)?
- **Pass:** Uses forward declarations and `#pragma once`, with ordered includes.
- **Fail:** Unnecessary `#include`s in headers, or missing `#pragma once`.

### Audit 3: Technical Debt and Dead Code
- **Trigger:** Scanning the codebase for comments and unused blocks.
- **Check:** Are there unresolved `TODO`, `FIXME`, or `HACK` comments? Is there commented-out code left behind from debugging? Are there unused variables or functions?
- **Pass:** No unresolved technical debt markers, and no dead code.
- **Fail:** Presence of `TODO`/`FIXME`, commented-out blocks, or unused code.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[Hygiene: <state>]`.

## Promotion Criteria
- A real maintenance issue, compilation slowdown, or confusion arises in production due to poor hygiene.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
