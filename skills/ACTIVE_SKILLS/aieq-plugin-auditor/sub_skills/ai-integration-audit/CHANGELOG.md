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
**Status:** Promotion pending formal SKILL.md update

### Promotion trigger
- Weakness observed: **boundary weakness**
- Times observed: **at least 1 strong documented case**
- Why promotion is justified:
  - The v1.0 audit risked overclaiming by treating mutex presence and synchronous inference as if they automatically implied direct audio-thread blocking.
  - The real weakness was incomplete thread-boundary and call-chain verification.

### What should change in v1.1
- Add **Call Chain / Thread Reachability Audit**
- Add **Shared-State Hazard Audit**
- Explicitly separate:
  - blocking risk
  - mutex reachability
  - data race risk
  - audio-thread contamination
  - AI-thread-only heaviness

### Evidence basis
- Known artifact retest: AI Equalizer Pro on `review/codex-2026-04-01`
- Tribunal reconciliation of AI/audio-thread claims

### Why this is not yet fully promoted
- `SKILL.md` is still on v1.0 in the branch state consulted during governance review
- The promotion path is now documented, but the skill file still needs its v1.1 update to complete the promotion chain

### What remains unproven
- Generalization beyond the current plugin architecture
- Reliability across fundamentally different AI integration models
