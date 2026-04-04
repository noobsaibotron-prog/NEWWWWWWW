# AIEQ+ Output Protocol

## Purpose
This protocol standardizes the format and structure of AI outputs when executing AIEQ+ skills. It ensures that findings are presented with empirical discipline, uncertainties are highlighted, and governance states are never collapsed.

The goal is to make AI analysis stable, severe, and directly comparable across different artifacts and sessions.

---

## Core Principle
Every output must separate what is proven from what is plausible, overstated, or unverified.
The AI must use the standard structural sections to prevent performative compliance and premature closure.

---

## Standard Output Structure

When executing a skill, the AI must produce an output adhering to the following structure, unless the specific skill template overrides it.

### 1. Executive Summary
A concise, 1-3 sentence summary of the analysis, explicitly stating the artifact reviewed and the primary conclusion.

### 2. Classification and Flags
This section must appear immediately after the summary. It must include:
- **Skill Category:** (e.g., `Capacity-Uplift`, `Encoded-Preference`)
- **Primary Domain:** (e.g., `Engineering`, `Legal`)
- **Boundary Flags:** Any `[BOUNDARY FLAG]`, `[CONFLICT FLAG]`, `[EVIDENCE FLAG]`, or `[DECAY FLAG]` triggered during the analysis. If none, state "None."

### 3. Findings (The Proof Map)
This is the core of the output. Findings must be categorized strictly by their evidentiary strength:

#### ✅ Proven (Grounded Evidence)
Claims that are directly supported by the artifact. The AI must cite the specific location (e.g., line number, section) in the artifact.
- *Local Evidence:* Proven within the single artifact.
- *Cross-Artifact Evidence:* Proven by synthesizing multiple provided artifacts.

#### ❌ Missed / Defects
Specific errors, omissions, or contradictions found in the artifact. Must be grounded in evidence.

#### ⚠️ Plausible / Inferred
Points that are logically deduced but lack direct, verifiable proof in the artifact. These must be explicitly labeled as inferred and never treated as facts.

#### ❓ Unverified / Overstated
Claims made in the artifact (or by the user) that cannot be substantiated by the provided evidence, or where the evidence is insufficient to support the strength of the claim.

### 4. Governance State Assessment
The AI must explicitly state the current governance level of the artifact based on the `AIEQ_PLUS_STATUS_MODEL.md`.
- **Current State:** (e.g., `Drafted`, `Reviewed`, `Tested`, `Validated`)
- **Justification:** Why this state applies (e.g., "The code has been reviewed but no test evidence was provided").
- **Missing for Next State:** What specific evidence is required to advance the artifact to the next governance level (e.g., `Approved`).

### 5. Weakness Classification (If Applicable)
If the analysis is evaluating the performance of the skill itself (e.g., during a test record creation), the AI must classify any observed failures using the `AIEQ_PLUS_WEAKNESS_CLASSIFICATION.md` (e.g., `Proof Weakness`, `Boundary Weakness`).

### 6. Next Steps / Recommendations
Clear, actionable steps required from the human or the system. This must not include executing high-severity remediations without a `[HITL REQUIRED]` flag.

---

## Forbidden Output Behaviors

The AI must **NEVER**:
- Output a single, unstructured narrative paragraph that mixes proven facts with inferred assumptions.
- Use closure language ("The issue is fully resolved", "The code is perfect") without fresh, verifiable evidence in the `Proven` section.
- Omit the `Unverified / Overstated` section if the user makes an unsupported claim.
- Resolve a `[CONFLICT FLAG]` silently without presenting both sides in the `Findings` section.
