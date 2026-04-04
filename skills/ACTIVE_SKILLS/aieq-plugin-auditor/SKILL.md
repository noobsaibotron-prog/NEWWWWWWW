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
1. **Identify Artifacts:** Map the provided project files to their respective domains using `ORCHESTRATOR_CONFIG.yaml` routing rules.
2. **Execute Sub-Skills:** Run the sub-skills sequentially or in parallel.
3. **Aggregate Findings:** Collect the `Proven` and `Missed` findings from each sub-skill.
4. **Execute Regression Matrix:** Read `REGRESSION_MATRIX.md` and verify each conflict pair using its Verification Protocol. Use the Conflict Severity Matrix to classify cross-domain severity.
5. **Synthesize:** Identify any cross-domain conflicts and emit `[CONFLICT FLAG]` for each.
6. **Output Hierarchical Report:** Generate the final report using the Layered Proof Map format.

> **On sub-skill promotion:** Before finalizing any promotion, re-run all sub-skill evals on the same artifact bundle and execute the full regression protocol (see `REGRESSION_MATRIX.md` Section 3).

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

### Cross-Module Synthesis (from Regression Matrix)
- ⚠️ **Inferred Risks:** [Cross-domain inferences from conflict pair verification]
- 🚩 **Conflicts:** [List any `[CONFLICT FLAG]` with severity from Conflict Severity Matrix]
- 🔄 **Regression Check:** [If this is a post-promotion audit: New / Resolved / Persistent / Shifted findings]

### Next Steps
[Actionable remediation steps, grouped by domain.]
