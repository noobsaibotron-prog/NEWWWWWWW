---
name: plugin-ui-audit
description: >
  Use this sub-skill to audit the UI/UX implementation of a JUCE audio plugin.
  Do not use it to audit DSP or licensing.
category: encoded-preference
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
  reasoning_depth: medium
---

# Plugin UI Audit

## Core Principle
UI updates must be decoupled from the audio thread.

## Forbidden Actions
- Do not validate UI code that reads directly from the `PluginProcessor` state without a thread-safe mechanism (e.g., `juce::AudioProcessorValueTreeState` listeners).
- Do not assume UI performance is fine without checking timer frequencies.

## Mandatory Grounding Pass
1. Identify the `PluginEditor.cpp` and `PluginEditor.h` artifacts.
2. Check the `timerCallback` or `paint` methods.

## Specialized Audits

### Audit 1 — APVTS Attachment Check
- **Trigger:** Any UI code review.
- **What it checks:** Usage of `AudioProcessorValueTreeState::SliderAttachment` (or similar) instead of direct polling.
- **What counts as proof:** Explicit instantiation of attachment objects in the editor constructor.
- **What does not count as proof:** Polling values in a `timerCallback`.

## Output Format
(Standard AIEQ+ Output Protocol)
