# AIEQ+ — Promotion Decision
**Date:** 2026-04-07  
**Branch:** `feature/aieq-plus-framework`

## Decision Summary

### Promote
- `dsp-safety-audit` → promoted (`v1.1` confirmed)
- `gui-performance-audit` → promoted (`v1.1` confirmed)
- `ai-integration-audit` → promoted (`v1.1` confirmed)

### Consolidate
- `release-verdict-engine`
- `build-system-audit`
- `code-hygiene-audit`
- `dsp-correctness-audit`
- `parameter-architecture-audit`
- `state-management-audit`
- remaining v1.0 sub-skills lacking enough archived promotion evidence

### Frozen
- `aieq-plugin-auditor` → reconciled, but still frozen beyond formal promoted state `v1.1`

## What changed in this decision
This update closes the formal gap for `ai-integration-audit`:
- its `CHANGELOG.md` already documented the v1.1 promotion case
- test and retest records already existed
- `SKILL.md` has now been updated so the promotion chain is complete

This update also reconciles the composite documentation:
- `SKILL.md`, `CHANGELOG.md`, and `ORCHESTRATOR_CONFIG.yaml` now describe the same implemented orchestration surface
- the composite is no longer frozen because of a documentation mismatch
- it remains frozen because the expanded composite surface has not yet been re-validated end-to-end

## Why `ai-integration-audit` is promoted now
Promotion is justified by a documented **boundary weakness**:
- the earlier audit model risked overclaiming direct audio-thread blocking
- the corrected model now explicitly separates:
  - direct blocking risk
  - thread reachability
  - mutex reachability
  - shared-state hazard

That correction is supported by:
- `ai_integration_test_001.yaml`
- `ai_integration_retest_001.yaml`
- tribunal reconciliation on the real plugin branch `review/codex-2026-04-01`

## Why the composite remains frozen
The composite is now **documentally aligned**, but not yet promotable beyond `v1.1` for governance purposes.

Reason:
- the broader orchestration surface (`v1.2` and `v1.3` implemented state) has not yet been rerun through a full composite-level regression on the same artifact bundle plus a new one
- the AI conflict model was materially refined during reconciliation, so a composite rerun is still required before formal promotion beyond the last safe state

## Current truthful status
- `ai-integration-audit` → **promoted**
- `aieq-plugin-auditor` → **aligned but still not promoted beyond v1.1**

## Next required actions
1. Run a full composite regression using the current orchestration surface on:
   - one known artifact bundle
   - one new artifact bundle
2. Validate that the refined AI conflict model does not introduce cross-domain regressions
3. Keep remaining v1.0 sub-skills in consolidation until they meet the promotion policy threshold
