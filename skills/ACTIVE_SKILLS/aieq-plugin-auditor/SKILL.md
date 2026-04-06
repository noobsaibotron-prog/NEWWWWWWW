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
  version: "1.3"
  formal_promoted_state: "1.1"
  governance_state: "frozen-pending-composite-regression"
  orchestrator_for:
    - dsp-safety-audit
    - gui-performance-audit
    - ai-integration-audit
    - test-quality-audit
    - state-management-audit
    - build-system-audit
    - parameter-architecture-audit
    - plugin-compliance-audit
    - dsp-correctness-audit
    - code-hygiene-audit
    - release-verdict-engine
  promotion_history:
    - v1.0: Initial composite skill for AI Equalizer Pro
    - v1.1: Promoted dsp-safety-audit and gui-performance-audit based on test_001 feedback
    - v1.2: Expanded orchestration surface to the full plugin audit stack (implementation state documented)
    - v1.3: Added release-verdict-engine and explicit composite regression requirement; formal promotion remains capped at v1.1 pending rerun
  model_requirements:
    context_window: 128k
    tool_use: required
    reasoning_depth: high
---

# AIEQ Plugin Auditor (Composite Orchestrator)

## Core Principle
This is a composite skill (coordinatore principale che mette insieme più sub-skill). It performs no direct domain audit itself. Its responsibility is to route artifacts to the correct sub-skills, aggregate their findings, and synthesize a hierarchical output with a vectorized governance state.

## Governance Note
The operational surface is documented at **v1.3**, but the last formally promoted composite state remains **v1.1** until the expanded orchestration surface is re-tested end-to-end on a full artifact bundle.

## Forbidden Actions
- Do not perform DSP, GUI, or AI analysis directly in this prompt; delegate to sub-skills.
- Do not collapse the final governance state into a single word without providing the state vector.
- Do not silently resolve conflicts between sub-skills.
- Do not treat added routing surface as formally promoted unless the composite regression protocol has been rerun.

## Orchestration Workflow
1. **Identify Artifacts:** Map project files to their domains using `ORCHESTRATOR_CONFIG.yaml`.
2. **Execute Sub-Skills:** Run the relevant sub-skills sequentially or in parallel.
3. **Aggregate Findings:** Collect proven findings, misses, and overstated points from each sub-skill.
4. **Execute Regression Matrix:** Read `REGRESSION_MATRIX.md` and verify conflict pairs using its protocol.
5. **Synthesize:** Identify cross-domain conflicts and emit `[CONFLICT FLAG]` when warranted.
6. **Output Hierarchical Report:** Generate the layered proof map.
7. **Execute Release Verdict Engine:** Pass the aggregated domain outputs and conflict results to `release-verdict-engine` for the final release verdict.

> **On promotion:** Before promoting the composite beyond the current formal promoted state, rerun all relevant sub-skill evals on the same artifact bundle and execute the full composite regression protocol.

## Output Format (Layered Proof Map)

### Executive Summary
[1–3 sentence summary of overall readiness.]

### Vectorized Governance State
- **State Vector:** `[DSP: <st>] | [GUI: <st>] | [AI: <st>] | [Test: <st>] | [State: <st>] | [Build: <st>] | [Param: <st>] | [Compliance: <st>] | [Math: <st>] | [Hygiene: <st>]`
- **Overall State:** `[Lowest common denominator state]`
- **Justification:** [Brief explanation]

### Hierarchical Findings
1. DSP Safety Analysis (`dsp-safety-audit`)
2. GUI Performance Analysis (`gui-performance-audit`)
3. AI Integration Analysis (`ai-integration-audit`)
4. Test Quality Analysis (`test-quality-audit`)
5. State Management Analysis (`state-management-audit`)
6. Build System Analysis (`build-system-audit`)
7. Parameter Architecture Analysis (`parameter-architecture-audit`)
8. Plugin Compliance Analysis (`plugin-compliance-audit`)
9. DSP Correctness Analysis (`dsp-correctness-audit`)
10. Code Hygiene Analysis (`code-hygiene-audit`)

### Cross-Module Synthesis
- **Inferred Risks:** [Cross-domain inferences]
- **Conflicts:** [Any `[CONFLICT FLAG]`]
- **Regression Check:** [New / Resolved / Persistent / Shifted findings]

### Next Steps
[Actionable remediation steps, grouped by domain.]
