---
name: aieq-plugin-auditor
description: >
  Use this skill to orchestrate a comprehensive, metrological audit of a C++/JUCE audio plugin project.
  Do not use it to audit single files; use the specific sub-skills directly instead.
metadata:
  category: encoded-preference
  domain_boundaries:
    primary: ENGINEERING
    excluded:
      - LEGAL
      - MARKETING
  version: "1.1"
  orchestrator_for:
    - dsp-safety-audit
    - gui-performance-audit
    - ai-integration-audit
  promotion_history:
    - v1.0: Initial Composite Skill for AI Equalizer Pro
    - v1.1: Promoted dsp-safety-audit and gui-performance-audit based on test_001 feedback
  model_requirements:
    context_window: 128k
    tool_use: required
    reasoning_depth: high
---

# AIEQ Plugin Auditor (Composite Orchestrator)

## Core Principle
This is a **Composite Skill**. It performs no direct audits itself. Its sole responsibility is to route C++/JUCE artifacts to the correct sub-skills, aggregate their findings, and synthesize a hierarchical output with a vectorized governance state.

## Forbidden Actions
- Do not perform DSP, GUI, or AI analysis directly in this prompt; you must delegate to the sub-skills.
- Do not collapse the final governance state into a single word without providing the state vector.
- Do not silently resolve conflicts between sub-skills (e.g., if GUI demands high refresh rate but DSP demands low lock contention).

## Orchestration Workflow
1. **Identify Artifacts:** Map the provided project files to their respective domains:
   - `Source/DSP/*` $\rightarrow$ `dsp-safety-audit`
   - `Source/GUI/*` $\rightarrow$ `gui-performance-audit`
   - `Source/AI/*` $\rightarrow$ `ai-integration-audit`
   - `Source/Core/*` $\rightarrow$ Relevant sub-skills based on context.
2. **Execute Sub-Skills:** Run the sub-skills sequentially or in parallel.
3. **Aggregate Findings:** Collect the `Proven` and `Missed` findings from each sub-skill.
4. **Synthesize:** Identify any cross-domain conflicts (e.g., a lock in the AI engine that might block the audio thread).
5. **Output Hierarchical Report:** Generate the final report using the Layered Proof Map format.

## Output Format (Layered Proof Map)

### Executive Summary
[1–3 sentence summary of the overall readiness.]

### Vectorized Governance State
- **State Vector:** `[DSP: <state>] | [GUI: <state>] | [AI: <state>]`
- **Overall State:** `[Lowest common denominator state]`
- **Justification:** [Brief explanation of the lowest state]

### Hierarchical Findings

#### 1. DSP Safety Analysis (`dsp-safety-audit`)
- ✅ **Proven:** [list]
- ❌ **Missed:** [list]

#### 2. GUI Performance Analysis (`gui-performance-audit`)
- ✅ **Proven:** [list]
- ❌ **Missed:** [list]

#### 3. AI Integration Analysis (`ai-integration-audit`)
- ✅ **Proven:** [list]
- ❌ **Missed:** [list]

### Cross-Module Synthesis
- ⚠️ **Inferred Risks:** [Cross-domain inferences, e.g., thread contention]
- 🚩 **Conflicts:** [List any `[CONFLICT FLAG]` between sub-skill findings]

### Next Steps
[Actionable remediation steps, grouped by domain.]
