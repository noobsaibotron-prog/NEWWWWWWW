---
name: juce-dsp-audit
description: >
  Use this sub-skill to audit the C++ DSP code of a JUCE audio plugin.
  Do not use it to audit UI components or licensing.
category: capacity-uplift
domain_boundaries:
  primary: ENGINEERING
  excluded:
    - LEGAL
    - CREATIVE
version: "1.0"
promotion_history:
  - v1.0: initial sub-skill creation
model_requirements:
  context_window: 32k
  tool_use: optional
  reasoning_depth: high
---

# JUCE DSP Audit

## Core Principle
DSP code must be real-time safe. No locks, no allocations, no blocking calls on the audio thread.

## Forbidden Actions
- Do not validate `processBlock` if it contains `new`, `malloc`, or `std::mutex::lock`.
- Do not assume thread safety without explicitly checking for lock-free data structures.

## Mandatory Grounding Pass
1. Identify the `PluginProcessor.cpp` and `PluginProcessor.h` artifacts.
2. Scan the `processBlock` method specifically.

## Specialized Audits

### Audit 1 — Real-Time Safety Check
- **Trigger:** Any DSP code review.
- **What it checks:** Absence of blocking operations in `processBlock`.
- **What counts as proof:** Explicit line numbers showing `std::atomic` usage, lock-free queues, and absence of `new`/`malloc`.
- **What does not count as proof:** Comments saying "this is thread safe".

## Output Format
(Standard AIEQ+ Output Protocol)
