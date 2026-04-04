---
name: test-quality-audit
description: >
  Use this sub-skill to audit the test suite of the AIEQ plugin. It verifies test coverage, assertion rigor, and test isolation.
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

# Test Quality Audit (Sub-Skill)

## Core Principle
A test is only as good as its assertions. This sub-skill evaluates the rigorousness, isolation, and coverage of the C++/JUCE test suite. It assumes tests must run in CI without flakiness.

## Forbidden Actions
- Do not evaluate DSP correctness or GUI rendering performance here; focus purely on the *testing methodology*.
- Do not accept "happy path" only tests as sufficient.
- Do not validate tests that use `Thread::sleep()` for synchronization.

## Mandatory Grounding Pass
Before judging a test file:
1. Identify the system under test (SUT).
2. Check if the test isolates the SUT (e.g., mocks/stubs for external dependencies).
3. Verify the assertions are checking specific, numerical, or stateful outcomes, not just "does it crash".

## Specialized Audits

### Audit 1: Assertion Rigor
- **Trigger:** Reading any `Source/Tests/*.cpp` file.
- **Check:** Does the test use strict tolerances for floating-point comparisons (e.g., `expectWithinAbsoluteError`)? Does it check edge cases (NaN, inf, boundary values)?
- **Pass:** Assertions are specific, strict, and cover boundaries.
- **Fail:** Assertions are vague, use exact equality for floats, or only test the happy path.

### Audit 2: Test Isolation and Flakiness
- **Trigger:** Tests involving threads, async operations, or UI components.
- **Check:** Does the test avoid arbitrary `sleep()` calls? Does it properly tear down state between runs?
- **Pass:** Uses deterministic synchronization (e.g., atomics, condition variables) and cleans up state.
- **Fail:** Uses `sleep()`, leaks state, or depends on execution order.

### Audit 3: Coverage and Scope
- **Trigger:** Reviewing the test suite as a whole.
- **Check:** Are the critical release blockers (RB-1 to RB-4) covered by automated tests?
- **Pass:** Critical paths have dedicated, automated test coverage.
- **Fail:** Critical paths rely on manual verification.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[Test Quality: <state>]`.

## Promotion Criteria
- A real test flakiness or coverage gap is discovered in production.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
