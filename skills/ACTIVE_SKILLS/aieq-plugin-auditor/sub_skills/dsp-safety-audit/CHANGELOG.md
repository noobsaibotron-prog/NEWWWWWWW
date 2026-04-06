# Changelog — dsp-safety-audit

## Version 1.0
**Date:** 2026-04-02  
**Status:** Initial release

### What was introduced
- Initial DSP RT-safety audit structure
- Allocation audit
- Lock-free / mutex reachability audit
- Latency reporting audit
- Standard AIEQ+ output discipline

### Why this version exists
- Created to audit real-time safety and runtime correctness in audio DSP code, with emphasis on plugin processing paths.

### Evidence basis
- Initial application on AI Equalizer Pro DSP layer
- Early review cycle on `review/codex-2026-04-01`

### What remained unproven
- Whether thread-context classification was always explicit enough
- Whether mutex presence was being distinguished correctly from true audio-thread reachability

---

## Version 1.1
**Date:** 2026-04-04  
**Status:** Promoted

### Promotion trigger
- Weakness observed: **boundary weakness**
- Times observed: **at least 1 strong documented case**
- Why promotion was justified:
  - The v1.0 audit overclaimed a direct audio-thread risk by treating a heavy AI function as if it were necessarily reachable from the audio thread.
  - The weakness was not in DSP reasoning itself, but in incomplete call-chain verification.

### What changed
- Added **Audit 4 — Call Chain Verification**
- Tightened thread-context classification
- Explicitly separated:
  - heavy function presence
  - thread reachability
  - actual audio-thread contamination
  - indirect subsystem risk

### Evidence basis
- Known artifact retest: AI Equalizer Pro, `review/codex-2026-04-01`
- New validation within same campaign: re-audit after tribunal reconciliation of AI/audio-thread claims

### Improvement confirmed
- The promoted skill correctly traced:
  - `processBlock -> enqueueAISpectrum -> SPSC queue -> aiAnalysisThread`
- The promoted skill no longer treated AI heaviness as direct audio-thread blocking without proof

### What remains unproven
- Generalization beyond the current plugin/JUCE/C++ domain
- Reliability across substantially different DSP architectures
