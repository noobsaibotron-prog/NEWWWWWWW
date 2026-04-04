---
name: plugin-compliance-audit
description: >
  Use this sub-skill to audit the plugin compliance, host compatibility, recall determinism, and block size/sample rate handling of the AIEQ plugin.
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

# Plugin Compliance Audit (Sub-Skill)

## Core Principle
A plugin must behave predictably in every DAW. It must report its latency correctly, handle arbitrary block sizes and sample rates, and never assume the host will call methods in a specific order.

## Forbidden Actions
- Do not evaluate DSP algorithms or GUI performance here.
- Do not assume `prepareToPlay` is called exactly once before `processBlock`.
- Do not validate plugins that fail to report latency changes.

## Mandatory Grounding Pass
Before judging compliance code:
1. Identify the latency reporting mechanism (`setLatencySamples`).
2. Identify how block sizes and sample rates are handled (`prepareToPlay`, `processBlock`).
3. Check the host compatibility matrix (if documented).

## Specialized Audits

### Audit 1: Latency Reporting
- **Trigger:** Reading `setLatencySamples` calls.
- **Check:** Is latency reported accurately based on the current quality mode (e.g., oversampling, linear phase)? Does it update dynamically when modes change?
- **Pass:** Latency is accurate and updates dynamically.
- **Fail:** Latency is hardcoded, or changes are not reported to the host.

### Audit 2: Block Size and Sample Rate Handling
- **Trigger:** Reading `prepareToPlay` and `processBlock`.
- **Check:** Does the plugin handle block sizes larger or smaller than expected? Does it adapt correctly to sample rate changes (e.g., recomputing filter coefficients)?
- **Pass:** Handles arbitrary block sizes and recomputes coefficients on sample rate changes.
- **Fail:** Assumes fixed block sizes, or fails to update state on sample rate changes.

### Audit 3: Recall Determinism
- **Trigger:** Reading `getStateInformation` and `setStateInformation`.
- **Check:** Does the plugin restore exactly the same state, producing bit-accurate audio output after a recall?
- **Pass:** State recall is deterministic and bit-accurate.
- **Fail:** State recall introduces random variations or uninitialized memory.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[Compliance: <state>]`.

## Promotion Criteria
- A real host compatibility issue or recall bug is found in production.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
