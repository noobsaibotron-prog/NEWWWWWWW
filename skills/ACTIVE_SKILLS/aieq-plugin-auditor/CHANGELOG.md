# Changelog: aieq-plugin-auditor

## [v1.3] - 2026-04-07
### Changed
- Reconciled `SKILL.md`, `CHANGELOG.md`, and `ORCHESTRATOR_CONFIG.yaml` to the same documented orchestration surface.
- Added `formal_promoted_state: "1.1"` to distinguish the implemented composite surface from the last fully promoted composite state.
- Added explicit governance wording that the composite remains frozen pending a full composite regression rerun.
- Refined AI-related conflict handling so the composite distinguishes:
  - direct audio-path blocking risk
  - shared-state hazard across thread boundaries

### Justification
- The branch had a documentation mismatch: `SKILL.md` declared `v1.3`, while `CHANGELOG.md` and `ORCHESTRATOR_CONFIG.yaml` only documented `v1.1`.
- Reconciliation was required before any valid decision could be made about further promotion.
- This alignment documents the real working surface but does **not** by itself constitute a formal promotion beyond `v1.1`.

## [v1.2] - 2026-04-04
### Changed
- Expanded the orchestration surface from the initial 3 sub-skills to the broader plugin audit stack:
  - `test-quality-audit`
  - `state-management-audit`
  - `build-system-audit`
  - `parameter-architecture-audit`
  - `plugin-compliance-audit`
  - `dsp-correctness-audit`
  - `code-hygiene-audit`

### Justification
- Full-codebase coverage required more than DSP / GUI / AI surface audits.
- This was an implementation-state expansion of the composite surface.

## [v1.1] - 2026-04-04
### Changed
- Promoted `dsp-safety-audit` to v1.1: added Audit 4 (Call Chain Verification) to prevent overclaiming thread context.
- Promoted `gui-performance-audit` to v1.1: updated paint-time resource calibration to exclude `juce::Colour` and `juce::Font` as automatically expensive under JUCE 7+ semantics.
- Promoted `aieq-plugin-auditor` composite to **v1.1 formal promoted state** to reflect sub-skill upgrades and the original 3-skill orchestration surface.

### Justification (Test Record `test_001`)
- **Proof Weakness:** The v1.0 audit falsely claimed `analyzeSpectrum()` blocked the audio thread because it was called from `PluginProcessor`, without tracing that it pushed to a lock-free queue consumed by a separate AI thread.
- **Encoded-Preference Weakness:** The v1.0 audit falsely flagged `juce::Colour` and `juce::Font` creation in `paint()` as resource-management violations, applying a generic rule that ignores JUCE 7+ POD/COW semantics.

## [v1.0] - 2026-04-04
### Added
- Initial composite skill release with 3 sub-skills (`dsp-safety-audit`, `gui-performance-audit`, `ai-integration-audit`).
- Orchestrator logic for routing JUCE artifacts to the correct sub-skills.
- Hierarchical output schema and vectorized governance state generation.
