---
name: suno-prompt-audit
description: >
  Use this skill when reviewing, auditing, or refining AI-generated text prompts intended for Suno V5 music generation (specifically Elite Techno/House or tkivilsaari style).
  Do not use it when generating Python code, writing legal documents, or generating images.
category: encoded-preference
domain_boundaries:
  primary: CREATIVE
  excluded:
    - LEGAL
    - ENGINEERING
version: "1.1"
promotion_history:
  - v1.0: initial version (basic prompt structure check)
  - v1.1: added BPM and Key coherence audit (justified by Test 001)
model_requirements:
  context_window: 8k
  tool_use: optional
  reasoning_depth: medium
---

# Suno V5 Prompt Audit

## Core Principle
A Suno prompt is not a creative wish; it is a technical specification. No prompt is "Validated" unless it explicitly defines style, tempo, key, and structural tags (e.g., `[Intro]`, `[Drop]`).

## Forbidden Actions
- Do not agree that a prompt is "ready" if it lacks structural tags.
- Do not use closure language ("The prompt is perfect") if BPM and Key are not specified.
- Do not silently correct a missing genre tag; flag it as a `Missed` requirement.

## Mandatory Grounding Pass
Before judging anything:
1. Identify the real artifact(s) (the exact text of the prompt).
2. Classify each proof as local (in the prompt text) / cross-artifact (compared to a reference track) / inferred.
3. Check for domain boundary issues (e.g., if the user asks to write a Python script to call the API, emit `[BOUNDARY FLAG]`).
4. Only then proceed.

## Specialized Audits

### Audit 1 — Structural Tag Audit
- **Trigger:** Any prompt review.
- **What it checks:** Presence of valid Suno V5 structural tags (e.g., `[Build-up]`, `[Bass Drop]`).
- **What counts as proof:** Exact string matches of bracketed tags in the artifact.
- **What does not count as proof:** Natural language descriptions like "then the bass drops".

### Audit 2 — BPM & Key Coherence (Added in v1.1)
- **Trigger:** Any prompt aiming for Elite Techno/House.
- **What it checks:** Explicit declaration of BPM (e.g., 135-145 for techno) and musical Key.
- **What counts as proof:** Explicit numeric BPM and Key signature in the prompt text.
- **What does not count as proof:** "Fast tempo" or "dark mood".

## Output Format

### Executive Summary
[1–3 sentence summary of the prompt's readiness for Suno V5 generation.]

### Classification
- **Skill category:** Encoded-Preference
- **Primary domain:** Creative
- **Boundary flags:** [List any emitted flags]

### Findings
#### ✅ Proven (Grounded Evidence)
- [List verified elements, e.g., "Structural tags [Intro] and [Outro] are present."]

#### ❌ Missed / Defects
- [List missing elements, e.g., "No BPM specified."]

#### ⚠️ Plausible / Inferred
- [List assumptions, e.g., "Style implies Techno, but not explicitly tagged."]

#### ❓ Unverified / Overstated
- [List unsupported claims, e.g., "User claims this will sound exactly like Hadone, but the prompt lacks specific texture descriptors."]

### Governance State Assessment
- **Current State:** [Drafted / Reviewed / Tested / Validated]
- **Justification:** [Why]
- **Missing for Next State:** [What is needed to advance]

### Next Steps
[Clear actions for the user to refine the prompt.]

## Promotion Criteria
This skill may be promoted only when:
- a real weakness has been demonstrated by testing
- the weakness is classified
- the new module is specific
- the promoted version is re-tested on:
  - one known artifact
  - one new artifact
