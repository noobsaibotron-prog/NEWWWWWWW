---
name: release-verdict-engine
description: >
  The definitive Meta-Auditor. Use this skill ONLY after all 10 sub-skills have completed their audits. 
  It synthesizes the 10-dimensional state vector, cross-domain conflicts, and file-level metrics into an 
  unequivocal release verdict and a prioritized remediation plan.
metadata:
  category: encoded-preference
  domain_boundaries:
    primary: ENGINEERING
    excluded:
      - LEGAL
      - MARKETING
  version: "1.0"
  promotion_history:
    - v1.0: Initial Meta-Auditor based on 192-file empirical audit data and Claude's reconciliation feedback
  model_requirements:
    context_window: 128k
    tool_use: required
    reasoning_depth: high
---

# Release Verdict Engine (Meta-Auditor)

## Core Principle
This skill does not analyze code. It analyzes **audit data**. Its sole purpose is to convert the findings of 10 independent sub-skills into a single, objective, and commercially calibrated Release Verdict. It must eliminate all ambiguity and closure language.

## Forbidden Actions
- Do not invent new findings that were not reported by the 10 sub-skills.
- Do not average the 10 domain scores to determine the overall state; the overall state MUST collapse to the lowest common denominator.
- Do not issue a `RELEASE-SAFE` verdict if any domain is in the `Drafted` state or if there are unresolved `[CONFLICT FLAG]`s with CRITICAL severity.
- Do not provide generic remediation advice (e.g., "fix the bugs"). Remediation MUST be prioritized by cross-domain impact (e.g., fixing an AI bug that propagates to DSP).

## Mandatory Grounding Pass
Before issuing a verdict:
1. Verify that all 10 sub-skills have completed their execution.
2. Load the CSV data and the Cross-Module Synthesis from the Orchestrator.
3. Load the `REGRESSION_MATRIX.md` to evaluate the severity of any active conflicts.
4. Calculate the 10-dimensional State Vector.

## Specialized Audits

### Audit 1 — Vector Collapse & Verdict Synthesis
- **Trigger:** Always runs first.
- **What it checks:** Calculates the final State Vector and determines the Release Verdict (`RELEASE-SAFE`, `RELEASE-CONDITIONAL`, `RELEASE-RISKY`, `DO-NOT-RELEASE`).
- **What counts as proof:** The 10 domain states. If any domain is `Drafted`, the verdict CANNOT be `RELEASE-SAFE`.
- **What does not count as proof:** A high average score. A 9/10 in DSP does not compensate for a 2/10 in Build System.

### Audit 2 — Cross-Domain Propagation Impact
- **Trigger:** Runs if there are any `[CONFLICT FLAG]`s or files with CRITICAL severity.
- **What it checks:** Identifies the "Superconductor Hubs" (files that propagate errors across domains, like `PluginProcessor.h`).
- **What counts as proof:** The dependency graph and the Conflict Severity Matrix.
- **What does not count as proof:** Isolated bugs in leaf nodes.

### Audit 3 — Prioritized Remediation Strategy
- **Trigger:** Runs if the verdict is anything other than `RELEASE-SAFE`.
- **What it checks:** Generates a remediation plan ordered by Maximum ROI (Return on Investment).
- **What counts as proof:** Fixing a file that resolves a CRITICAL cross-domain conflict (e.g., `AIEngine.cpp`) ranks higher than fixing 10 isolated Code Hygiene issues.
- **What does not count as proof:** Alphabetical or domain-based ordering.

## Output Format

### 1. The Definitive Verdict
- **Verdict:** [`RELEASE-SAFE` | `RELEASE-CONDITIONAL` | `RELEASE-RISKY` | `DO-NOT-RELEASE`]
- **Overall State:** [Lowest common denominator state]
- **Commercial Rating:** [X.X / 10.0]

### 2. The 10-Dimensional State Vector
`[DSP: <st>] | [GUI: <st>] | [AI: <st>] | [Test: <st>] | [State: <st>] | [Build: <st>] | [Param: <st>] | [Compliance: <st>] | [Math: <st>] | [Hygiene: <st>]`

### 3. Critical Blockers (The "Why")
[List the specific domains, files, and cross-domain conflicts that are preventing a `RELEASE-SAFE` verdict. Explain the propagation impact.]

### 4. Maximum ROI Remediation Plan (The "How")
#### Wave 1: Architectural Hubs (P0)
[Files that propagate CRITICAL errors across domains. Fixing these shifts the state vector.]
#### Wave 2: Domain-Specific Blockers (P1)
[Files that keep a specific domain in `Drafted` state.]
#### Wave 3: Commercial Polish (P2)
[Files that affect the Commercial Rating but not the State Vector.]

## Promotion Criteria
This skill may be promoted only when:
- a release verdict is proven wrong by post-release user feedback (e.g., a plugin rated `RELEASE-SAFE` crashes in a specific DAW).
- the scoring model needs recalibration based on new commercial standards.
