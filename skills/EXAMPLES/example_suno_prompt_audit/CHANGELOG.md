# Skill Changelog — suno-prompt-audit

## Version 1.0
**Date:** 2026-04-01  
**Status:** Initial release

### What was introduced
- Initial skill structure
- Core principle: Prompts are technical specifications, not creative wishes.
- Mandatory grounding pass for prompt review.
- Audit 1: Structural Tag Audit.

### Why this version exists
- Created to address: AI generators failing to produce coherent tracks because users were submitting unstructured narrative paragraphs instead of tagged prompts.
- Initial scope: Enforcing Suno V5 structural tags (e.g., `[Intro]`, `[Drop]`).

### Evidence basis
- Tested on: `evals/artifacts/prompt_narrative_fail.txt`
- Tested on: `evals/artifacts/prompt_tagged_success.txt`

### What remains unproven
- Whether structural tags alone are enough to guarantee genre fidelity for elite electronic music.

---

## Version 1.1
**Date:** 2026-04-04  
**Status:** Promoted

### Promotion trigger
- Weakness observed: Proof Weakness (Skill validated prompts that had structural tags but resulted in chaotic, off-genre outputs because BPM and Key were left to the model's random choice).
- Times observed: 3
- Why promotion was justified: Elite Techno/House requires exact tempo and tonal control. Structural tags without BPM/Key lead to commercial/generic outputs, failing the user's core intent.

### What changed
- Added module: Audit 2 — BPM & Key Coherence.
- Tightened rule: Forbidden action added to prevent closure language if BPM/Key are missing.

### Evidence basis
- Known artifact retest: `tests/test_001.yaml` (The prompt that failed previously was flagged correctly by v1.1).
- New artifact test: `tests/test_002.yaml` (A new prompt for a 'tkivilsaari style' track was successfully audited for missing BPM).

### Improvement confirmed
- The skill no longer validates prompts that lack tempo and tonal specifications.
- False confidence (overclaiming readiness) reduced to zero in tested cases.

### What remains unproven
- Whether specific sound design descriptors (e.g., "acid stabs", "FM synthesis") need their own dedicated audit module.
