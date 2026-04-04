---
name: audio-plugin-manager
description: >
  Use this skill to orchestrate a comprehensive audit of a JUCE/C++ audio plugin project.
  Do not use it to audit single files; use the specific sub-skills directly instead.
category: encoded-preference
domain_boundaries:
  primary: ENGINEERING
  excluded:
    - CREATIVE
version: "2.0"
orchestrator_for:
  - juce-dsp-audit
  - plugin-ui-audit
  - license-compliance-audit
promotion_history:
  - v1.0: Monolithic skill (retired due to Workload Weakness)
  - v2.0: Refactored into Composite Skill orchestrator
model_requirements:
  context_window: 128k
  tool_use: required
  reasoning_depth: high
---

# Audio Plugin Manager (Composite Orchestrator)

## Core Principle
This is a **Composite Skill**. It performs no direct audits itself. Its sole responsibility is to route artifacts to the correct sub-skills, aggregate their findings, and synthesize a hierarchical output with a vectorized governance state.

## Forbidden Actions
- Do not perform DSP, UI, or Licensing analysis directly in this prompt; you must delegate to the sub-skills.
- Do not collapse the final governance state into a single word without providing the state vector.
- Do not silently resolve conflicts between sub-skills (e.g., UI performance vs. DSP load).

## Orchestration Workflow
1. **Identify Artifacts:** Map the provided project files to their respective domains (e.g., `PluginProcessor.cpp` $\rightarrow$ DSP, `PluginEditor.cpp` $\rightarrow$ UI, `LICENSE.txt` $\rightarrow$ Licensing).
2. **Execute Sub-Skills:** Run the three sub-skills sequentially or in parallel.
3. **Aggregate Findings:** Collect the `Proven` and `Missed` findings from each sub-skill.
4. **Synthesize:** Identify any cross-domain conflicts (e.g., DSP requires 4x oversampling, but UI module flags CPU overhead as too high).
5. **Output Hierarchical Report:** Generate the final report using the Layered Proof Map format.

## Output Format (Layered Proof Map)

### Executive Summary
[1–3 sentence summary of the plugin's overall readiness.]

### Vectorized Governance State
- **State Vector:** `[DSP: <state>] | [UI: <state>] | [Licensing: <state>]`
- **Overall State:** `[Lowest common denominator state]`
- **Justification:** [Brief explanation of the lowest state]

### Hierarchical Findings

#### 1. DSP Analysis (`juce-dsp-audit`)
- ✅ **Proven:** [list]
- ❌ **Missed:** [list]

#### 2. UI/UX Analysis (`plugin-ui-audit`)
- ✅ **Proven:** [list]
- ❌ **Missed:** [list]

#### 3. Licensing (`license-compliance-audit`)
- ✅ **Proven:** [list]
- ❌ **Missed:** [list]

### Cross-Module Synthesis
- ⚠️ **Inferred Risks:** [Cross-domain inferences]
- 🚩 **Conflicts:** [List any `[CONFLICT FLAG]` between sub-skill findings]

### Next Steps
[Actionable remediation steps, grouped by domain.]
