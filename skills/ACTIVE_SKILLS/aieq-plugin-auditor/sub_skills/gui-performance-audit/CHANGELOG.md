# Changelog — gui-performance-audit

## Version 1.0
**Date:** 2026-04-02  
**Status:** Initial release

### What was introduced
- Initial GUI performance review structure
- Paint-path audit
- Repaint frequency audit
- Rendering/resource creation audit
- Thread-safety checks for GUI/DSP handoff

### Why this version exists
- Created to identify performance, rendering, and GUI-thread safety issues in plugin interfaces, especially high-refresh and spectrum-heavy components.

### Evidence basis
- Initial application on AI Equalizer Pro GUI layer
- First-pass review of spectrum, OpenGL, and panel components

### What remained unproven
- Whether the skill was sufficiently calibrated for JUCE-specific lightweight paint constructs
- Whether all reported paint-time object creation carried real performance cost

---

## Version 1.1
**Date:** 2026-04-04  
**Status:** Promoted

### Promotion trigger
- Weakness observed: **severity weakness**
- Times observed: **1 strong documented case**
- Why promotion was justified:
  - The v1.0 audit over-penalized `juce::Colour` and `juce::Font` creation inside `paint()`, treating them as if they were always meaningful heap-allocation issues.
  - This produced a false positive in the JUCE-specific context.

### What changed
- Added JUCE-specific calibration rule for lightweight `Colour` / `Font` creation
- Tightened performance severity thresholds
- Explicitly separated:
  - lightweight stack/shared-impl paint objects
  - heap-allocating render work
  - repeated path/vector allocation
  - repaint scheduling waste

### Evidence basis
- Known artifact retest: AI Equalizer Pro GUI layer
- Re-audit after tribunal correction on paint-path overstatement

### Improvement confirmed
- The promoted skill no longer treats `juce::Colour` and `juce::Font` as automatic performance defects
- The promoted skill continues to catch real issues such as:
  - unconditional repaint loops
  - per-frame vector allocation
  - unsafe GUI/GL handoff
  - missing caching / LOD strategies

### What remains unproven
- Generalization beyond JUCE-heavy plugin GUIs
- Reliability on non-plugin desktop GUI frameworks
