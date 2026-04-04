# Changelog: aieq-plugin-auditor

## [v1.1] - 2026-04-04
### Changed
- Promoted `dsp-safety-audit` to v1.1: Added Audit 4 (Call Chain Verification) to prevent overclaiming thread context.
- Promoted `gui-performance-audit` to v1.1: Updated Audit 3 to explicitly exclude `juce::Colour` and `juce::Font` from expensive resource checks, reflecting JUCE 7+ semantics.
- Promoted `aieq-plugin-auditor` Orchestrator to v1.1 to reflect sub-skill upgrades.

### Justification (Test Record `test_001`)
- **Proof Weakness:** The v1.0 audit falsely claimed `analyzeSpectrum()` blocked the audio thread because it was called from `PluginProcessor`, without tracing that it pushed to a lock-free queue consumed by a separate AI thread.
- **Encoded-Preference Weakness:** The v1.0 audit falsely flagged `juce::Colour` and `juce::Font` creation in `paint()` as resource management violations, applying a generic rule that ignores JUCE 7+ POD/COW semantics.

## [v1.0] - 2026-04-04
### Added
- Initial Composite Skill release with 3 sub-skills (`dsp-safety-audit`, `gui-performance-audit`, `ai-integration-audit`).
- Orchestrator logic for routing JUCE artifacts to the correct sub-skills.
- Hierarchical Output Schema and Vectorized Governance State generation.
