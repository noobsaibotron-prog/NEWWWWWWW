---
name: dsp-safety-audit
description: >
  Use this skill to audit C++/JUCE DSP code for real-time safety, memory allocation, lock-free concurrency, and call chain verification.
  Do not use it for GUI or AI code analysis.
category: capacity-uplift
domain_boundaries:
  primary: ENGINEERING/DSP
  excluded:
    - ENGINEERING/GUI
    - ENGINEERING/AI
version: "1.1"
promotion_history:
  - v1.0: Initial release (Real-time safety and lock-free checks)
  - v1.1: Added Audit 4 (Call Chain Verification) to prevent overclaiming thread context (justified by test_001)
model_requirements:
  context_window: 32k
  tool_use: optional
  reasoning_depth: high
---

# DSP Safety Audit

## Core Principle
The audio thread (`processBlock`) must never wait. It cannot allocate memory, acquire blocking locks, or perform unbounded loops. Furthermore, you must never assume the execution thread of a function without tracing its call chain.

## Forbidden Actions
- Do not approve code that uses `new`, `delete`, `std::vector::push_back`, or `std::string` inside `processBlock`.
- Do not approve code that uses `std::mutex` or `juce::CriticalSection` inside `processBlock`.
- Do not infer that a data structure is lock-free just because it has "atomic" in the name; verify the implementation.
- **[v1.1]** Do not infer that a function blocks the audio thread simply because it is called from a DSP class; you must verify if the data is pushed to a queue and consumed on a separate thread.

## Mandatory Grounding Pass
Before judging anything:
1. Identify the real artifact(s) (the exact text of the C++ file).
2. Classify each proof as local (in the file) / cross-artifact (compared to a reference).
3. If an assertion relies on external context, flag it as `[EVIDENCE FLAG]`.

## Specialized Audits

### Audit 1: Allocation Safety
- **What to check:** Verify that no memory allocations occur in `processBlock`.
- **Proof requirement:** The code must use pre-allocated buffers (e.g., `juce::AudioBuffer` initialized in `prepareToPlay`) or stack variables.
- **Classification if failed:** `Real-Time Safety Weakness`.

### Audit 2: Lock-Free Concurrency
- **What to check:** Verify that communication between the audio thread and other threads (GUI, AI) uses lock-free structures (e.g., `std::atomic`, lock-free FIFOs).
- **Proof requirement:** Explicit use of `std::atomic` with memory ordering (e.g., `std::memory_order_acquire`), or bounded CAS loops.
- **Classification if failed:** `Real-Time Safety Weakness`.

### Audit 3: Phase & Latency Management
- **What to check:** Verify that lookahead and latency reporting are correctly synchronized.
- **Proof requirement:** `setLatencySamples()` must be called correctly, and lookahead buffers must be correctly sized.
- **Classification if failed:** `DSP Logic Weakness`.

### Audit 4: Call Chain Verification [v1.1]
- **What to check:** Verify the actual execution thread of any function suspected of being blocking before classifying it as a real-time violation.
- **Proof requirement:** You must trace the data flow. If a function pushes data to a lock-free queue (e.g., `SPSCQueue`) to be consumed by another thread, the call site on the audio thread is SAFE, even if the consumer function is blocking.
- **Classification if failed:** `Proof Weakness` (if the auditor fails to verify the chain).

## Output Format
Use the standard AIEQ+ Output Protocol (Executive Summary, Classification & Flags, Findings/Proof Map, Governance State Assessment, Next Steps).

## Promotion Criteria
This skill may be promoted only when a real weakness is demonstrated by testing (e.g., a specific SIMD optimization failure).
