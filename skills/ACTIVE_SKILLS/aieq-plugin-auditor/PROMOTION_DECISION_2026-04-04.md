# AIEQ+ — Promotion Decision
**Date:** 2026-04-04  
**Branch:** `feature/aieq-plus-framework`

## Decision Summary

### Promote
- `dsp-safety-audit` → promoted (`v1.1` confirmed)
- `gui-performance-audit` → promoted (`v1.1` confirmed)

### Consolidate
- `ai-integration-audit` → promotion pending formal `SKILL.md` update to v1.1
- `release-verdict-engine`
- `build-system-audit`
- `code-hygiene-audit`
- `dsp-correctness-audit`
- `parameter-architecture-audit`
- `state-management-audit`
- remaining v1.0 sub-skills lacking enough archived promotion evidence

### Frozen
- `aieq-plugin-auditor`

## Why the composite is frozen
The composite is operational, but its promotion chain is not yet internally consistent:
- `SKILL.md` declares `v1.3`
- `CHANGELOG.md` only documents `v1.0` and `v1.1`
- `ORCHESTRATOR_CONFIG.yaml` still reflects `v1.1`

No further promotion beyond the last fully documented state should be treated as formally valid until these files are reconciled.

## Why two sub-skills are promoted now
### `dsp-safety-audit`
Promotion is justified by a documented boundary weakness: the earlier audit overclaimed direct audio-thread risk without sufficient call-chain verification. The promoted model adds explicit call-chain checking and has supporting test/retest material.

### `gui-performance-audit`
Promotion is justified by a documented severity weakness: the earlier audit over-penalized JUCE paint-time `Colour` / `Font` usage. The promoted model incorporates JUCE-specific calibration and has supporting test/retest material.

## Next required actions
1. Reconcile `aieq-plugin-auditor/SKILL.md`, `CHANGELOG.md`, and `ORCHESTRATOR_CONFIG.yaml`
2. Update `ai-integration-audit/SKILL.md` to formalize v1.1 with:
   - call-chain / thread-reachability audit
   - shared-state hazard audit
3. Keep all remaining v1.0 sub-skills in consolidation until they meet the promotion policy threshold
