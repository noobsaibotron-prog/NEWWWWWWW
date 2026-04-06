# Changelog — ai-integration-audit

## Version 1.0
**Date:** 2026-04-02  
**Status:** Initial release

### What was introduced
- Initial AI integration audit structure
- Async vs sync inference review
- Model loading audit
- Shared data synchronization review

### Why this version exists
- Created to audit the AI subsystem of the plugin, especially thread interaction, inference placement, and synchronization strategy.

### Evidence basis
- Initial application on AI Equalizer Pro AI layer

### What remained unproven
- Whether the skill was distinguishing correctly between:
  - heavy AI code
  - direct audio-thread reachability
  - mutex reachability
  - shared-state hazards

---

## Version 1.1
**Date:** 2026-04-04  
**Status:** Promoted

### Promotion trigger
- Weakness observed: **boundary weakness**
- Times observed: **at least 1 strong documented case**
- Why promotion is justified:
  - The v1.0 audit risked overclaiming by treating mutex presence and synchronous inference as if they automatically implied direct audio-thread blocking.
  - The real weakness was incomplete thread-boundary and call-chain verification.

### What changed in v1.1
- Added **Call Chain / Thread Reachability Audit**
- Added **Shared-State Hazard Audit**
- Explicitly separates:
  - blocking risk
  - mutex reachability
  - data race risk
  - audio-thread contamination
  - AI-thread-only heaviness

### Evidence basis
- Known artifact retest: AI Equalizer Pro on `review/codex-2026-04-01`
- Tribunal reconciliation of AI/audio-thread claims
- Formal `SKILL.md` update completed on `feature/aieq-plus-framework`

### Governance result
- `ai-integration-audit` is now formally promoted to **v1.1**
- Remaining work is now about generalization, not branch-local formalization

### What remains unproven
- Generalization beyond the current plugin architecture
- Reliability across fundamentally different AI integration models
