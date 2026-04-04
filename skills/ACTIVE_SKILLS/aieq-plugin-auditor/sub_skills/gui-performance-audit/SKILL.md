---
name: gui-performance-audit
description: >
  Use this skill to audit C++/JUCE GUI code for rendering performance, thread safety, and resource management.
  Do not use it for DSP or AI code analysis.
category: capacity-uplift
domain_boundaries:
  primary: ENGINEERING/GUI
  excluded:
    - ENGINEERING/DSP
    - ENGINEERING/AI
version: "1.0"
promotion_history:
  - v1.0: Initial release (Rendering performance and thread safety checks)
model_requirements:
  context_window: 32k
  tool_use: optional
  reasoning_depth: high
---

# GUI Performance Audit

## Core Principle
The GUI must run efficiently, usually at 60 FPS, without stalling the message thread or interfering with the audio thread.

## Forbidden Actions
- Do not approve code that performs heavy computations (e.g., FFT, ML inference) on the message thread.
- Do not approve code that continuously repaints without a timer or proper invalidation regions.
- Do not assume that `repaint()` is cost-free; verify what triggers it.

## Mandatory Grounding Pass
Before judging anything:
1. Identify the real artifact(s) (the exact text of the C++ file).
2. Classify each proof as local (in the file) / cross-artifact (compared to a reference).
3. If an assertion relies on external context, flag it as `[EVIDENCE FLAG]`.

## Specialized Audits

### Audit 1: Message Thread Safety
- **What to check:** Verify that the GUI only communicates with the DSP via atomic variables, APVTS parameters, or lock-free FIFOs.
- **Proof requirement:** No direct method calls to the `PluginProcessor` that modify its state outside of the APVTS.
- **Classification if failed:** `Thread Safety Weakness`.

### Audit 2: Rendering Performance
- **What to check:** Verify that the GUI uses efficient rendering techniques (e.g., OpenGL, cached images, path flattening).
- **Proof requirement:** Complex graphics (like a spectrum analyzer) must not be redrawn entirely on every frame unless necessary; use staging buffers or VBOs if OpenGL is used.
- **Classification if failed:** `Rendering Weakness`.

### Audit 3: Resource Management
- **What to check:** Verify that images, fonts, and other resources are cached and not re-loaded continuously.
- **Proof requirement:** Resources must be loaded once (e.g., in the constructor or a LookAndFeel class) and reused.
- **Classification if failed:** `Resource Management Weakness`.

## Output Format
Use the standard AIEQ+ Output Protocol (Executive Summary, Classification & Flags, Findings/Proof Map, Governance State Assessment, Next Steps).

## Promotion Criteria
This skill may be promoted only when a real weakness is demonstrated by testing (e.g., a specific OpenGL rendering bottleneck).
