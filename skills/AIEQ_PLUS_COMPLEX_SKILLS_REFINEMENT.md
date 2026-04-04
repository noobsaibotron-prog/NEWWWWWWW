# AIEQ+ Refinement Proposal: Scaling to Complex Skills

## 1. The Current Architectural Limit

The AIEQ+ framework, as currently designed, excels at governing **narrow, linear skills** (e.g., `suno-prompt-audit`). It forces a strict cycle: Test $\rightarrow$ Observe Failure $\rightarrow$ Add Specific Module $\rightarrow$ Retest.

However, when applied to **deeply complex skills** (e.g., a full `audio-plugin-manager` that handles DSP C++ code review, UI/UX validation, and licensing compliance simultaneously), the current architecture encounters severe stress points:

1. **Monolithic Output Collapse:** The `OUTPUT_PROTOCOL` mandates a single, flat report. A complex skill executing 15 different audits across 3 domains will produce a massive, unreadable wall of text, making the `Findings (Proof Map)` impossible to navigate.
2. **All-or-Nothing Validation:** The `STATUS_MODEL` applies to the artifact as a whole. If the DSP code is perfect (`Validated`) but the licensing header is missing (`Drafted`), the entire artifact is dragged down, losing the nuance of partial validation.
3. **Conflict Protocol Bottleneck:** The `MULTI_SKILL_CONFLICT_PROTOCOL` assumes conflicts happen between *separate* skills. In a complex, multi-module skill, conflicts will happen *internally* between its own modules, which the current protocol doesn't explicitly handle.
4. **Test Record Bloat:** A single `TEST_RECORD.yaml` for a 15-module skill would require documenting every correct, missed, and overstated finding across all modules, making the archiving process too expensive to maintain.

---

## 2. Proposed Architectural Refinements

To scale AIEQ+ for complex skills without losing its empirical rigor, I propose introducing the concept of **Composite Skills (Skill Trees)** and refining the runtime protocols.

### Refinement A: The "Composite Skill" Pattern
Instead of allowing a single `SKILL.md` to grow infinitely by adding modules, we introduce a formal `Composite Skill` structure. 

**How it works:**
- A complex skill is defined not by its own audits, but as an **Orchestrator** of smaller, narrow AIEQ+ skills (Sub-Skills).
- Example: `audio-plugin-manager` becomes an orchestrator that calls `juce-dsp-audit`, `plugin-ui-audit`, and `license-compliance-audit`.
- **Why this fits AIEQ+:** It preserves the "narrow creation" principle. Each sub-skill maintains its own `CHANGELOG`, its own `TEST_RECORDs`, and its own promotion cycle. The orchestrator only manages the routing and the final synthesis.

### Refinement B: Hierarchical Output Schema
Update the `OUTPUT_PROTOCOL` to support a **Layered Proof Map**.

**Current Flat Structure:**
- Findings (Proven, Missed, Inferred)

**Proposed Hierarchical Structure:**
- **Module 1: DSP Analysis**
  - Findings (Proven, Missed)
- **Module 2: UI/UX Analysis**
  - Findings (Proven, Missed)
- **Synthesis:** (Cross-module inferences and conflicts)

**Why this fits AIEQ+:** It prevents cognitive overload while maintaining the strict separation of proven vs. unverified claims at the module level.

### Refinement C: Vectorized Governance States
Update the `STATUS_MODEL` to allow **Vectorized States** for complex artifacts.

**How it works:**
Instead of saying "The Plugin is Reviewed", the skill must output a state vector:
- `[DSP: Validated] | [UI: Tested] | [Licensing: Drafted]`
- **Overall State:** `Tested` (always collapses to the lowest common denominator for safety, but preserves the specific vectors for human visibility).

**Why this fits AIEQ+:** It honors the rule "Never collapse governance states" by making the distinct states explicit, preventing the AI from overclaiming readiness just because *most* of the artifact is good.

### Refinement D: The "Fast-Track" Regression Matrix
Update the `PROMOTION_POLICY` to handle complex skill retesting.

**How it works:**
Currently, promoting a skill requires retesting on "one known, one new artifact." For a complex skill, a change in the DSP module might break the UI module's context. 
We must introduce a **Regression Matrix**: when a sub-skill is promoted, the Orchestrator must run a fast-track eval suite (like `evals.json`) across *all* sub-skills to prove no cross-domain interference occurred.

---

## 3. Implementation Roadmap

If these refinements are approved, they can be implemented via a standard AIEQ+ promotion of the framework itself:

1. **Update `AIEQ_PLUS_SKILL_TEMPLATE.md`:** Add an optional `orchestrator_for: [list of sub-skills]` field in the frontmatter.
2. **Update `AIEQ_PLUS_OUTPUT_PROTOCOL.md`:** Add the `Hierarchical Output Schema` and `Vectorized Governance States` rules.
3. **Update `AIEQ_PLUS_PROMOTION_POLICY.md`:** Add the `Regression Matrix` requirement for Composite Skills.

These changes maintain the extreme severity of the AIEQ+ method while giving it the structural capacity to handle enterprise-grade complexity.
