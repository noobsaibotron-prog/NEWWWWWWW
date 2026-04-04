---
name: ai-integration-audit
description: >
  Use this skill to audit C++/JUCE AI code for model loading, inference performance, and thread safety.
  Do not use it for DSP or GUI code analysis.
category: capacity-uplift
domain_boundaries:
  primary: ENGINEERING/AI
  excluded:
    - ENGINEERING/DSP
    - ENGINEERING/GUI
version: "1.0"
promotion_history:
  - v1.0: Initial release (Model loading and inference safety checks)
model_requirements:
  context_window: 32k
  tool_use: optional
  reasoning_depth: high
---

# AI Integration Audit

## Core Principle
The AI engine must operate asynchronously, without blocking the audio thread, and manage model weights safely.

## Forbidden Actions
- Do not approve code that performs ML inference (e.g., TFLite `Invoke()`) directly inside the audio thread's `processBlock`.
- Do not approve code that loads model weights (`.bin` or `.tflite`) on the audio thread.
- Do not assume that an inference thread is safe; verify that its inputs/outputs are synchronized lock-free with the audio thread.

## Mandatory Grounding Pass
Before judging anything:
1. Identify the real artifact(s) (the exact text of the C++ file).
2. Classify each proof as local (in the file) / cross-artifact (compared to a reference).
3. If an assertion relies on external context, flag it as `[EVIDENCE FLAG]`.

## Specialized Audits

### Audit 1: Asynchronous Inference
- **What to check:** Verify that ML inference runs on a dedicated background thread or task pool.
- **Proof requirement:** The code must use a `juce::Thread`, `juce::ThreadPool`, or similar asynchronous mechanism for inference.
- **Classification if failed:** `Thread Safety Weakness`.

### Audit 2: Model Loading Safety
- **What to check:** Verify that model weights are loaded during initialization (e.g., `prepareToPlay` or constructor) and not during playback.
- **Proof requirement:** Initialization logic must use `std::call_once` or similar mechanisms to prevent double-initialization races.
- **Classification if failed:** `Initialization Weakness`.

### Audit 3: Data Synchronization
- **What to check:** Verify that the AI engine communicates its results back to the DSP or GUI using lock-free structures.
- **Proof requirement:** The results must be written to an atomic variable or a lock-free queue.
- **Classification if failed:** `Real-Time Safety Weakness`.

## Output Format
Use the standard AIEQ+ Output Protocol (Executive Summary, Classification & Flags, Findings/Proof Map, Governance State Assessment, Next Steps).

## Promotion Criteria
This skill may be promoted only when a real weakness is demonstrated by testing (e.g., a specific memory leak in the inference wrapper).
