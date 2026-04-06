---
name: ai-integration-audit
description: >
  Use this skill to audit C++/JUCE AI integration for inference placement,
  thread reachability, shared-state correctness, and lock-free handoff discipline.
  Do not use it for DSP-only or GUI-only code analysis.
category: capacity-uplift
domain_boundaries:
  primary: ENGINEERING/AI
  excluded:
    - ENGINEERING/DSP
    - ENGINEERING/GUI
version: "1.1"
promotion_history:
  - v1.0: Initial release (model loading, inference safety, basic synchronization checks)
  - v1.1: Added Call Chain / Thread Reachability and Shared-State Hazard audits after boundary overclaim on AI/audio-thread interaction
model_requirements:
  context_window: 32k
  tool_use: optional
  reasoning_depth: high
---

# AI Integration Audit

## Core Principle
Separate subsystem heaviness from audio-thread reachability. Direct blocking risk, mutex reachability, and shared-state correctness are different claims and must not be collapsed.

## Forbidden Actions
- Do not claim audio-thread blocking merely because mutexes or synchronous inference exist somewhere in the AI subsystem.
- Do not approve code that performs ML inference directly inside `processBlock` or other audio-thread helpers.
- Do not approve code that loads model weights on the audio thread.
- Do not ignore non-blocking shared-state hazards (plain shared fields, torn structs, unsafe cross-thread mutation) simply because the audio thread does not lock.

## Mandatory Grounding Pass
Before judging anything:
1. Identify the exact AI artifacts under review.
2. Map thread entry points (`processBlock`, worker thread entry, GUI callbacks, timers).
3. Trace the call chain from the audio path to AI work and classify each proof as local / cross-artifact / inferred.
4. Explicitly separate:
   - direct blocking risk
   - mutex reachability
   - shared-state hazard
   - AI-thread-only heaviness

## Specialized Audits

### Audit 1: Asynchronous Inference
- **What to check:** Verify that inference runs on a dedicated worker thread or task system.
- **Proof requirement:** The call chain must show background execution rather than direct audio-path execution.
- **Classification if failed:** `Thread Safety Weakness`.

### Audit 2: Model Loading Safety
- **What to check:** Verify that model weights are loaded during initialization and not during playback.
- **Proof requirement:** Initialization must use constructor / prepare-time setup or guarded one-time init.
- **Classification if failed:** `Initialization Weakness`.

### Audit 3: Handoff Discipline
- **What to check:** Verify that audio-to-AI and AI-to-audio/GUI handoff uses lock-free queues, atomics, or snapshots appropriate to the thread boundary.
- **Proof requirement:** Results and inputs must cross the boundary via explicit lock-free or atomic structures.
- **Classification if failed:** `Real-Time Safety Weakness`.

### Audit 4: Call Chain / Thread Reachability
- **What to check:** Verify whether heavy AI functions are directly reachable from the audio thread or only through worker-thread handoff.
- **Proof requirement:** Show the chain from `processBlock` (or equivalent) to the heavy function.
- **Classification if failed:** `Boundary Weakness`.

### Audit 5: Shared-State Hazard
- **What to check:** Verify whether plain shared fields, non-atomic structs, or unsafe cross-thread mutation remain even when direct blocking is absent.
- **Proof requirement:** Identify writer thread, reader thread, and the synchronization discipline actually used.
- **Classification if failed:** `Shared-State Correctness Weakness`.

## Output Format
Use the standard AIEQ+ Output Protocol (Executive Summary, Classification & Flags, Findings/Proof Map, Governance State Assessment, Next Steps).

## Promotion Criteria
This skill may be promoted only when a real weakness is demonstrated by testing and the promoted model is re-tested on a known artifact with improved boundary discipline.
