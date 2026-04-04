# ALIGNMENT_MANIFEST.md

> **READ THIS FILE FIRST.** This is the single source of truth for any AI platform working on the AI Equalizer Pro (AIEQ) project. It declares the canonical state of the codebase, the AIEQ+ framework, the last audit verdict, and the current priority. Do not rely on any other file for project-level context until you have read this one.

**Last Updated:** 2026-04-04
**Updated By:** Manus (for Marco)
**Governance State of This File:** Reviewed

---

## 1. Project Identity

| Field | Value |
|---|---|
| **Project Name** | AI Equalizer Pro (AIEQ) |
| **Repository** | `https://github.com/noobsaibotron-prog/NEWWWWWWW` |
| **Language** | C++ (JUCE Framework) |
| **Build System** | CMake |
| **Plugin Formats** | VST3, AU, AAX |
| **Owner** | Marco (sound designer, prompt engineer) |

---

## 2. Canonical Branch

| Branch | Role | Status |
|---|---|---|
| **`review/codex-2026-04-01`** | **CANONICAL.** Contains the latest code, audit results, and framework evolution documents. | Active — all new work goes here. |
| `feature/aieq-plus-framework` | Contains the full AIEQ+ framework skill tree (17 core files, 11 sub-skills, examples, templates). | Frozen — do not commit here. Merge into canonical branch pending. |
| `main` | Stable release branch. | Stale — last updated before the AIEQ+ audit. Do not use for new work. |

**Rule:** If you are an AI starting work on this project, checkout `review/codex-2026-04-01`. Do not use `main`.

---

## 3. Repository Structure

```
NEWWWWWWW/
├── ALIGNMENT_MANIFEST.md          ← YOU ARE HERE
├── Source/                         ← Plugin source code (91 files)
│   ├── AI/                         ← 20 files (AI inference, ML, semantic engine)
│   ├── Core/                       ← 5 files (lock-free structures, capture, history)
│   ├── DSP/                        ← 12 files (EQ processors, spectrum, convolver)
│   ├── GUI/                        ← 17 files (panels, displays, OpenGL renderer)
│   ├── Tests/                      ← 28 files (unit, behavioral, regression tests)
│   ├── Utils/                      ← 5 files (logger, preset manager, debug)
│   ├── PluginProcessor.cpp/h       ← Central hub (41 transitive dependents)
│   └── PluginEditor.cpp/h          ← Main editor
├── skills/                         ← AIEQ+ framework (partial on this branch)
│   ├── ACTIVE_SKILLS/              ← Live operational skills
│   │   └── aieq-plugin-auditor/    ← Composite orchestrator v1.3
│   └── EXAMPLES/                   ← Reference examples
├── review/
│   └── codex-2026-04-01/           ← Audit results and strategy documents
│       ├── AIEQ_RELEASE_VERDICT.md ← Official DO-NOT-RELEASE verdict
│       ├── AIEQ_PLUS_EVOLUTION_STRATEGY.md ← Framework evolution roadmap
│       ├── slides/                 ← Verdict presentation (11 slides)
│       ├── verdict_dashboard.png   ← 4-quadrant dashboard visualization
│       ├── verdict_radar_projection.png ← Current vs. post-remediation radar
│       └── verdict_roadmap.png     ← 3-wave remediation plan
├── REPORTS/                        ← Historical reports (pre-AIEQ+)
├── SKILL_METHOD_FILES/             ← ⚠️ DEPRECATED (see Section 8)
├── PROMPTS/                        ← ⚠️ DEPRECATED (see Section 8)
├── JUCE/                           ← JUCE framework
├── ThirdParty/                     ← Third-party dependencies
└── CMakeLists.txt                  ← Build configuration
```

---

## 4. Last Audit Verdict

| Metric | Value |
|---|---|
| **Verdict** | **DO-NOT-RELEASE** |
| **Commercial Rating (Adjusted)** | **5.61 / 10.0** |
| **Commercial Rating (Raw)** | 6.61 / 10.0 |
| **Conflict Penalties** | -1.0 (2 HIGH @ state cap, 2 MEDIUM @ -0.5 each) |
| **Commercial Threshold** | 7.0 / 10.0 |
| **Files Analyzed** | 192 |
| **Domains** | 10 |
| **Sub-Skills Used** | 11 (10 domain audits + 1 meta-auditor) |
| **Audit Date** | 2026-04-04 |
| **Full Report** | `review/codex-2026-04-01/AIEQ_RELEASE_VERDICT.md` |

### 4.1 Domain State Vector

```
[DSP Safety: Drafted (8.42)] | [GUI Perf: Drafted (6.95)] | [AI Integration: Drafted (3.25)]
[Test Quality: Reviewed (7.61)] | [State Mgmt: Reviewed (5.43)] | [Build System: Drafted (4.38)]
[Param Arch: Reviewed (8.38)] | [Plugin Compliance: Reviewed (7.00)] | [DSP Correctness: Drafted (5.83)]
[Code Hygiene: Drafted (6.91)]
```

**Overall State:** Drafted (6/10 domains in Drafted state, 0/10 in Tested or higher)

### 4.2 Active Cross-Domain Conflicts

| ID | Conflict | Severity | Impact |
|---|---|---|---|
| C1 | DSP ↔ AI: Audio Path Contamination | **HIGH** | Caps DSP Safety and AI Integration at Tested maximum |
| C2 | GUI ↔ AI: Direct Processor Calls | **HIGH** | Caps GUI Performance and AI Integration at Tested maximum |
| C3 | GUI ↔ DSP: Spectrum Pipeline Contention | MEDIUM | -0.5 to Commercial Rating |
| C4 | AI ↔ DSP: Queue Overflow Risk | MEDIUM | -0.5 to Commercial Rating |

### 4.3 Critical Blockers

1. **AI Integration is systemically broken (3.25/10).** 8/20 files CRITICAL. Pattern: synchronous inference and mutex where lock-free structures exist.
2. **GUI Performance collapses under AI dependency (6.95/10).** 5 CRITICAL files. Pattern: direct processor calls bypassing APVTS.
3. **Build System has zero CLEAN files (4.38/10).** 21/21 files have issues. No CI/CD pipeline.
4. **4 cross-domain conflicts** compound failure risks and reduce the Commercial Rating by 1.0 point.

---

## 5. Current Priority: Wave 1 Remediation

The project is in the **remediation phase**. The 3-wave plan is defined in the Release Verdict report. The current priority is Wave 1.

### Wave 1 — P0: Architectural Hubs (2 files)

| File | Location | Transitive Dependents | Domains Affected | What to Fix |
|---|---|---|---|---|
| **AIEngine.cpp/h** | `Source/AI/` | 24 files | AI, GUI, DSP, Hygiene | Replace `std::mutex` with `AtomicSnapshot` from `LockFreeStructures.h`. Make all public methods async. Expose results via lock-free queue. |
| **LinearPhaseProcessor.cpp/h** | `Source/DSP/` | 26 files | DSP Safety, DSP Correctness, Compliance, Param | Remove `ensureChannels()` allocation from `process()`. Report latency to host via `setLatencySamples()`. Fix phase response accuracy. |

**Expected Impact:** AI Integration, GUI Performance, DSP Safety move from Drafted to Reviewed. Commercial Rating: 5.61 → ~6.0.

### Wave 2 — P1: Domain Blockers (8 files)

Not yet started. Do not begin Wave 2 until Wave 1 is verified by re-audit.

| File | Domain | Score | What to Fix |
|---|---|---|---|
| AIControlPanel.h | GUI | 2.0 | Replace direct processor calls with APVTS listeners |
| NeuralNetworkWrapper.cpp | AI | 2.0 | Add async inference with timeout and fallback |
| AdvancedSpectrumDisplay.h | GUI | 2.0 | Move resource creation out of paint() |
| MLEngine.cpp | AI | 4.0 | Async model loading with progress callback |
| AIProblemPanel.h | GUI | 3.0 | Replace direct processor calls with APVTS |
| SemanticControlPanel.h | GUI | 2.0 | Replace direct processor calls with APVTS |
| PluginEditor.cpp | GUI | 3.0 | Refactor panel initialization |
| AIEngine_Advanced.cpp | AI | 3.0 | Align with AIEngine async pattern |

### Wave 3 — P2: Commercial Polish (4 files)

Not yet started. Do not begin Wave 3 until Wave 2 is verified by re-audit.

| File | Domain | Weight | Score | What to Fix |
|---|---|---|---|---|
| BiquadCoefficients.h | DSP Correctness | 1.5x | 4.0 | Verify coefficient accuracy, add anti-cramping |
| PluginProcessor.h | Plugin Compliance | 1.5x | 4.0 | Add transactional state protection |
| DynamicEQProcessor.cpp | Plugin Compliance | 1.5x | 4.0 | Verify block size handling |
| PartitionedConvolver.h | DSP Safety | 2.0x | 6.0 | Verify partition boundary correctness |

**Projected final state after all 3 waves:** Commercial Rating ~7.2, Verdict: RELEASE-CONDITIONAL (Beta only).

---

## 6. AIEQ+ Framework

### 6.1 What It Is

AIEQ+ (AI Epistemological Quality Plus) is an evidence-based skill management system for AI agents. It enforces the principle: **no promotion without documented failure**. Skills are created narrowly, tested on real artifacts, and expanded only when a real test exposes a recurring weakness.

### 6.2 Framework Location

The complete AIEQ+ framework (17 core files) lives on the `feature/aieq-plus-framework` branch under `skills/`. It is structured as follows:

| Folder | Purpose | Files |
|---|---|---|
| `METHOD_BASE/` | Core principles, glossary, promotion policy | 3 |
| `RUNTIME/` | Executable protocols (output format, flags, quickstart) | 4 |
| `TEMPLATES/` | Skill template, test record template, evals template, changelog template | 4 |
| `GOVERNANCE/` | Status model, weakness classification, conflict protocol, retirement policy, HITL protocol | 5 |
| `INDEX/` | Repository map | 1 |
| `DOMAINS/` | Domain-specific rules (scaffolding only) | 6 (.gitkeep) |

### 6.3 Active Composite Skill: aieq-plugin-auditor v1.3

The only active skill is `aieq-plugin-auditor`, a Composite Skill (orchestrator) that coordinates 11 sub-skills:

| Sub-Skill | Domain | Weight | Last Score |
|---|---|---|---|
| dsp-safety-audit | DSP Safety | 2.0x | 8.42 |
| gui-performance-audit | GUI Performance | 1.0x | 6.95 |
| ai-integration-audit | AI Integration | 1.0x | 3.25 |
| test-quality-audit | Test Quality | 0.5x | 7.61 |
| state-management-audit | State Management | 1.0x | 5.43 |
| build-system-audit | Build System | 0.5x | 4.38 |
| parameter-architecture-audit | Parameter Architecture | 1.0x | 8.38 |
| plugin-compliance-audit | Plugin Compliance | 1.5x | 7.00 |
| dsp-correctness-audit | DSP Correctness | 1.5x | 5.83 |
| code-hygiene-audit | Code Hygiene | 0.5x | 6.91 |
| **release-verdict-engine** | **Meta-Auditor** | — | Verdict: DO-NOT-RELEASE |

### 6.4 Governance States

Skills and artifacts are classified into one of these states. States never collapse upward.

| State | Meaning |
|---|---|
| **Drafted** | Created in first form. Does not imply review or correctness. |
| **Reviewed** | Examined by a reviewer or protocol. Does not imply testing. |
| **Tested** | Exercised against real artifacts. Does not imply strong adequacy. |
| **Validated** | Evidence is strong enough for the intended claim within scope. |
| **Approved** | Formally accepted by a human governance authority. |
| **Retired** | No longer recommended for active use. |

---

## 7. Scoring Model Reference

The Release Verdict Engine uses a weighted scoring model:

**Commercial Rating (Raw)** = Sum(domain_score * domain_weight) / Sum(domain_weight)

**Commercial Rating (Adjusted)** = Raw - Sum(conflict_penalties)

| Conflict Severity | Penalty |
|---|---|
| HIGH | Caps affected domains at Tested maximum (no score penalty, state penalty) |
| MEDIUM | -0.5 to Commercial Rating per conflict |
| LOW | -0.25 to Commercial Rating per conflict |

| Verdict | Threshold |
|---|---|
| RELEASE-SAFE | Rating >= 8.0 AND all domains >= Tested AND zero HIGH conflicts |
| RELEASE-CONDITIONAL | Rating >= 7.0 AND <= 2 domains Drafted AND zero unresolved HIGH conflicts |
| RELEASE-RISKY | Rating >= 6.0 AND <= 4 domains Drafted |
| DO-NOT-RELEASE | Rating < 6.0 OR > 4 domains Drafted OR any unresolved HIGH conflict |
| CRITICAL-HALT | Rating < 4.0 OR any domain score < 2.0 |

---

## 8. Deprecated Files — Do Not Use

The following folders contain legacy files from before the AIEQ+ framework was created. They are preserved for historical reference only. **Do not use them for new audits or reviews.**

| Folder | Contents | Superseded By |
|---|---|---|
| `SKILL_METHOD_FILES/` | `AIEQ_SKILL_IMPLEMENTATION_METHOD_BASE_2026-04-02.md`, `AIEQ_SKILL_SELECTION_DIRECTIVES_2026-04-02.md`, `AIEQ_SKILL_SELECTION_QUICKSTART_2026-04-02.md` | `skills/METHOD_BASE/` and `skills/RUNTIME/` on `feature/aieq-plus-framework` branch |
| `PROMPTS/` | `AIEQ_CODEBASE_FILE_REVIEW_v1_1.md`, `AIEQ_CODEBASE_FILE_REVIEW_v1_2.md`, `AIEQ_REVIEW_TRIBUNAL_v4_IMPERIAL.md`, `AIEQ_REVIEW_TRIBUNAL_v4_1_ANTI_MISFIRE.md`, `AIEQ_REVIEW_TRIBUNAL_v4_2_SEVERITY_GOVERNOR.md` | `skills/ACTIVE_SKILLS/aieq-plugin-auditor/sub_skills/` on `feature/aieq-plus-framework` branch |

---

## 9. Source Code Map (91 files)

### Source/AI/ (20 files) — AI inference, ML, semantic engine

| File | Audit Score | Severity | Key Issue |
|---|---|---|---|
| AIEngine.cpp | 3.0 | CRITICAL | Mutex on shared state, 24 transitive dependents — **WAVE 1 TARGET** |
| AIEngine.h | 3.0 | CRITICAL | Exposes blocking API |
| AIEngine_Advanced.cpp | 3.0 | CRITICAL | Misaligned with AIEngine async pattern |
| AdaptiveAIEngine.cpp | 5.0 | HIGH | Synchronous adaptation loop |
| AdaptiveAIEngine.h | 5.0 | HIGH | Blocking interface |
| MLEngine.cpp | 4.0 | CRITICAL | Blocking model loading |
| MLEngine.h | 4.0 | CRITICAL | No async API |
| MultiTrackUnmasking.cpp | 4.0 | CRITICAL | Synchronous multi-track processing |
| MultiTrackUnmasking.h | 4.0 | CRITICAL | Blocking interface |
| NeuralNetworkWrapper.cpp | 2.0 | CRITICAL | Blocking inference, no timeout, no fallback |
| NeuralNetworkWrapper.h | 2.0 | CRITICAL | Blocking interface |
| OnlineLearningSystem.cpp | 5.0 | HIGH | Synchronous learning |
| OnlineLearningSystem.h | 5.0 | HIGH | Blocking interface |
| ReferenceMatcher.cpp | 5.0 | HIGH | Synchronous matching |
| ReferenceMatcher.h | 5.0 | HIGH | Blocking interface |
| SemanticEQEngine.cpp | 3.0 | CRITICAL | NLP parsing blocks calling thread |
| SemanticEQEngine.h | 3.0 | CRITICAL | Blocking interface |
| UserLearning.cpp | 5.0 | HIGH | Synchronous learning |
| UserLearning.h | 5.0 | HIGH | Blocking interface |
| OnlineLearningSystem 2.h | — | — | Duplicate file (space in name) |

### Source/Core/ (5 files) — Lock-free structures, capture, history

| File | Audit Score | Severity | Note |
|---|---|---|---|
| LockFreeStructures.h | 9.5 | CLEAN | **Gold standard.** 41 transitive dependents. Must stay CLEAN. |
| LockFreeAudioFIFO.h | 9.0 | CLEAN | Production-grade FIFO |
| CaptureService.h | 7.0 | CLEAN | Adequate |
| HistoryManager.h | 6.0 | HIGH | State management concerns |
| OSCParameterServer.h | 6.0 | HIGH | Network thread safety |

### Source/DSP/ (12 files) — EQ processors, spectrum, convolver

| File | Audit Score | Severity | Note |
|---|---|---|---|
| ParametricEQProcessor.cpp | 9.0 | CLEAN | Elite-level DSP |
| ParametricEQProcessor.h | 9.0 | CLEAN | Elite-level DSP |
| DynamicEQProcessor.cpp | 4.0 | HIGH | Block size handling — **WAVE 3 TARGET** |
| DynamicEQProcessor.h | 8.0 | CLEAN | Good interface |
| LinearPhaseProcessor.cpp | 5.0 | CRITICAL | Allocation in process() — **WAVE 1 TARGET** |
| LinearPhaseProcessor.h | 5.0 | CRITICAL | Missing latency reporting |
| PartitionedConvolver.h | 6.0 | HIGH | Partition boundary concerns — **WAVE 3 TARGET** |
| BiquadCoefficients.h | 4.0 | CRITICAL | Coefficient accuracy — **WAVE 3 TARGET** |
| SpectrumAnalyzer.cpp | 7.0 | CLEAN | Adequate |
| SpectrumAnalyzer.h | 7.0 | CLEAN | Adequate |
| SpectrumAnalyzerCore.h | 7.5 | CLEAN | Good |
| SpectrumDisplayMapper.h | 7.0 | CLEAN | Adequate |

### Source/GUI/ (17 files) — Panels, displays, OpenGL renderer

| File | Audit Score | Severity | Note |
|---|---|---|---|
| AIControlPanel.h | 2.0 | CRITICAL | Direct processor calls — **WAVE 2 TARGET** |
| AIProblemPanel.h | 3.0 | CRITICAL | Direct processor calls — **WAVE 2 TARGET** |
| AdvancedSpectrumDisplay.h | 2.0 | CRITICAL | Resource creation in paint() — **WAVE 2 TARGET** |
| SemanticControlPanel.h | 2.0 | CRITICAL | Direct processor calls — **WAVE 2 TARGET** |
| GLSpectrumComponent.h | 7.0 | CLEAN | Good OpenGL implementation |
| NewSpectrumPipeline.h | 8.0 | CLEAN | Production-grade pipeline |
| OpenGLSpectrumRenderer.h | 8.0 | CLEAN | Professional renderer |
| AnalyzerSettingsPanel.h | 7.0 | CLEAN | Adequate |
| BandControlPanel.h | 7.0 | CLEAN | Adequate |
| BandViewport.cpp | 7.0 | CLEAN | Adequate |
| BandViewport.h | 7.0 | CLEAN | Adequate |
| DynamicEQPanel.h | 7.0 | CLEAN | Adequate |
| EQBandControl.h | 8.0 | CLEAN | Good |
| LevelMeter.h | 7.5 | CLEAN | Good |
| LogScaleMapper.h | 8.0 | CLEAN | Good |
| ModernLookAndFeel.h | 7.0 | CLEAN | Adequate |
| PremiumKnob.h | 7.5 | CLEAN | Good |

### Source/ Root (4 files) — Plugin core

| File | Audit Score | Severity | Note |
|---|---|---|---|
| PluginProcessor.cpp | 6.0 | HIGH | Central hub, 41 transitive dependents |
| PluginProcessor.h | 4.0 | CRITICAL | Missing transactional state protection — **WAVE 3 TARGET** |
| PluginEditor.cpp | 3.0 | CRITICAL | Aggregates all problematic panels — **WAVE 2 TARGET** |
| PluginEditor.h | 6.0 | HIGH | Depends on PluginEditor.cpp fixes |

### Source/Tests/ (28 files)

Test suite is comprehensive. 0 CRITICAL files. 5 HIGH files (missing edge cases). 22 CLEAN files. Score: 7.61/10.

### Source/Utils/ (5 files)

| File | Audit Score | Severity | Note |
|---|---|---|---|
| Logger.cpp | 7.0 | CLEAN | Adequate |
| Logger.h | 7.0 | CLEAN | Adequate |
| PresetManager.cpp | 5.0 | HIGH | State persistence concerns |
| PresetManager.h | 5.0 | HIGH | State persistence concerns |
| DebugLog.h | 8.0 | CLEAN | Good |

---

## 10. Instructions for AI Platforms

### 10.1 Before Starting Any Work

1. Read this file (`ALIGNMENT_MANIFEST.md`) completely.
2. Confirm you are on branch `review/codex-2026-04-01`.
3. Check Section 5 to understand the current priority.
4. Do not use files from `SKILL_METHOD_FILES/` or `PROMPTS/` (see Section 8).

### 10.2 If You Are Fixing Code

1. The current priority is **Wave 1 remediation** (AIEngine.cpp/h and LinearPhaseProcessor.cpp/h).
2. Do not start Wave 2 until Wave 1 fixes are committed and verified by re-audit.
3. When fixing AIEngine.cpp, the reference implementation for lock-free patterns is `Source/Core/LockFreeStructures.h`.
4. When fixing LinearPhaseProcessor.cpp, the allocation in `process()` must be moved to `prepareToPlay()`.
5. After any fix, update this manifest's Section 5 with the new state.

### 10.3 If You Are Auditing Code

1. The AIEQ+ framework files are on branch `feature/aieq-plus-framework` under `skills/`.
2. The orchestrator is `skills/ACTIVE_SKILLS/aieq-plugin-auditor/SKILL.md` (v1.3).
3. Each sub-skill has its own `SKILL.md` under `sub_skills/<domain>-audit/`.
4. The scoring model is in `sub_skills/release-verdict-engine/SCORING_MODEL.md`.
5. After re-audit, update this manifest's Section 4 with the new verdict.

### 10.4 If You Are Evolving the Framework

1. Read `review/codex-2026-04-01/AIEQ_PLUS_EVOLUTION_STRATEGY.md` for the strategic roadmap.
2. The current evolution priority is **Layer 1: Infrastructure Consolidation** (unify repo, create state registry, deprecate legacy files).
3. Do not add new sub-skills unless a documented test failure justifies them.
4. Follow the promotion policy in `skills/METHOD_BASE/AIEQ_PLUS_PROMOTION_POLICY.md`.

### 10.5 Conflict Resolution Between AI Platforms

If two AI platforms produce conflicting recommendations:

1. The platform with **file-level empirical evidence** (actual code analysis) takes precedence over the platform with **inference-level reasoning** (general best practices).
2. If both have file-level evidence, the more recent analysis takes precedence (check commit dates).
3. If unresolvable, emit a `[CONFLICT FLAG]` in the output and escalate to Marco (HITL).

---

## 11. Version History of This Manifest

| Date | Author | Change |
|---|---|---|
| 2026-04-04 | Manus | Initial creation with full audit data, source code map, and cross-platform instructions. |

---

*This file is part of the AIEQ+ framework governance. It must be updated after every audit, remediation wave, or significant project state change.*
