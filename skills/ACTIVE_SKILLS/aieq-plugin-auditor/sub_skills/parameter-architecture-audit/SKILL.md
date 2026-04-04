---
name: parameter-architecture-audit
description: >
  Use this sub-skill to audit the parameter architecture, APVTS integration, and smoothing strategies of the AIEQ plugin.
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

# Parameter Architecture Audit (Sub-Skill)

## Core Principle
Parameters are the bridge between user intent and DSP execution. They must be strictly bounded, safely communicated across threads, and smoothed to prevent audio artifacts (zipper noise).

## Forbidden Actions
- Do not evaluate GUI rendering or pure DSP math here.
- Do not assume a parameter is safe just because it uses `std::atomic`; verify its usage context.
- Do not accept unsmoothed parameters in the audio path if they control gain, frequency, or phase.

## Mandatory Grounding Pass
Before judging parameter code:
1. Identify the APVTS layout (e.g., `createParameterLayout`).
2. Identify how parameters are read in the audio thread (e.g., `getRawParameterValue`).
3. Identify where smoothing is applied (e.g., `juce::SmoothedValue`).

## Specialized Audits

### Audit 1: APVTS Correctness and Bounds
- **Trigger:** Reading `createParameterLayout` or parameter definitions.
- **Check:** Are all parameters given explicit, safe ranges (e.g., `juce::NormalisableRange`)? Are skew factors used appropriately for frequency/time parameters?
- **Pass:** Parameters have strict bounds and appropriate skew.
- **Fail:** Parameters lack bounds, or use linear mapping for logarithmic domains (e.g., frequency).

### Audit 2: Thread-Safe Parameter Access
- **Trigger:** Reading `processBlock` parameter access.
- **Check:** Does the audio thread read parameters lock-free (e.g., via atomic pointers from APVTS)?
- **Pass:** Uses `getRawParameterValue` or atomic listeners.
- **Fail:** Acquires locks to read parameters, or uses non-atomic shared variables.

### Audit 3: Smoothing and Zipper Noise Prevention
- **Trigger:** Reading DSP code that applies parameters.
- **Check:** Are continuous parameters (gain, freq, Q) smoothed using `juce::SmoothedValue` or equivalent? Is the smoothing rate appropriate for the block size?
- **Pass:** Continuous parameters are smoothed.
- **Fail:** Gain or frequency changes are applied instantaneously, causing zipper noise.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[Parameter Arch: <state>]`.

## Promotion Criteria
- A real zipper noise or parameter out-of-bounds crash is found in production.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
