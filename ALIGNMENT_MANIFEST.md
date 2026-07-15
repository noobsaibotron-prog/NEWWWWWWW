# ALIGNMENT_MANIFEST.md

> **READ THIS FILE FIRST.** This is the single source of truth for any AI platform working on the AI Equalizer Pro (AIEQ) project. It declares the canonical state of the codebase, the AIEQ+ framework, the last audit verdict, and the current priority.

**Last Updated:** 2026-07-15 (Audit reconciliation)
**Updated By:** Codex (for Marco)
**Governance State of This File:** Reconciled with live `feature/unified-exp-best` worktree

---

## 1. Project Identity

| Field | Value |
|---|---|
| **Project Name** | AI Equalizer Pro (AIEQ) |
| **Repository** | `https://github.com/noobsaibotron-prog/NEWWWWWWW` |
| **Language** | C++ (JUCE Framework) |
| **Build System** | CMake |
| **Owner** | Marco (sound designer, prompt engineer) |

---

## 2. Canonical Branch

| Branch | Role | Status |
|---|---|---|
| **`feature/unified-exp-best`** | **EXP consolidation branch.** | Active for isolated Motore v2 / UX / M9 lab consolidation. Not a release branch. |
| **`feature/motore-v2-a0`** | Checkpoint source. | Must remain fixed at `5c9cb3290f87b62a339c6b2c49645b2b25524712`. |
| **`review/codex-2026-04-01`** | Historical release-audit branch. | Historical reference only; its release-safe verdict is superseded by this reconciliation. |

**Current HEAD:** `59135db2571a9346aca357747ed54adc5647504a` at the time of reconciliation.

---

## 3. Current Audit Verdict: EXPERIMENTAL / NOT RELEASE-SAFE

| Metric | Value |
|---|---|
| **Verdict** | **NOT RELEASE-SAFE** |
| **Commercial Rating** | **Deferred**. Previous `9.25 / 10.0` verdict is historical and no longer authoritative. |
| **Previous Verdict** | RELEASE-SAFE (historical, superseded) |
| **Status** | The live worktree builds and the current blocking ctest suite is green, but release readiness is blocked by documented runtime, test-governance, AI/ML, and product-packaging gaps. |

### 3.0 Why The Verdict Changed

The earlier release-safe verdict was based on a historical remediation audit. A deeper
2026-07 counter-audit found that several green gates were narrower than their labels:
performance debt was quarantined, thread-safety detectors were excluded, and some release
documents claimed more than the current scorecard and test matrix could prove. This file now
tracks the live branch as an experimental consolidation branch, not as a commercial release.

### 3.1 Recent Hardening (Post-Tribunal v4.2)

| Issue | File | Status | Fix Detail |
|---|---|---|---|
| **OpenGL Sync** | `OpenGLSpectrumRenderer.h` | ✅ Fixed | juce::SpinLock protection for buffer swap; removed heap alloc in draw. |
| **GUI Idle Overhead** | `SemanticControlPanel.h` | ✅ Fixed | Conditional repaint only when morphing or state dirty; fixed edge cases in applyPreset. |
| **M/S Test Coverage** | `MSModeSwitchContinuityTest.cpp` | ✅ Fixed | Expanded to 12x12 graph (Mid↔Side, etc.). kMaxDelta relaxed to 0.25f for robustness. |

### 3.2 Verified Fixes (Wave 1 & 2)

| Issue | File | Status | Fix Detail |
|---|---|---|---|
| **T-6 OSC Logging** | `OSCParameterServer.h` | ✅ Verified | Removed hardcoded Desktop logging. |
| **P1 M/S Crossfade** | `PluginProcessor.cpp` | ✅ Verified | 1024-sample crossfade. Verified with expanded test suite. |
| **P2-A AI Atomics** | `AIEngine.h` | ✅ Verified | `enabled` and `correctionMode` use `std::atomic`. |
| **P2-B Lazy Profile** | `AIEngine.cpp` | ✅ Verified | `applyProfileThresholds` moved to AI thread. |
| **D1 Peak Identity** | `DynamicEQProcessor.cpp` | ✅ Verified | `makeBypass()` at gain ≈ 0. |

---

## 4. Current Priority: Audit Reconciliation And Blocking-Debt Burn-Down

The immediate priority is to make gates and documents truthful before any release claim:

1. no test target may pass with zero executed tests or zero assertions;
2. quarantined KnownDebt must stay visible and non-blocking, not mislabeled as a green gate;
3. runtime/audio-thread P0s must be fixed or explicitly excluded from release scope;
4. Motore v2 / M9 work remains experimental until A6/A7 gates are green and integrated with provenance.

### 4.1 Historical Gates Requiring Revalidation

The following were previously cited as final release gates, but they are not accepted as
current release proof until they are present in the active branch, wired into the current
test matrix, and reproduced from a clean checkout/release package:

1. **Host Matrix Validation**
2. **Recall Determinism**
3. **Randomized Stress Harness**
4. **DynEQ Runtime Validation**

---

## 5. AIEQ+ Framework State

| Sub-Skill | Version | State |
|---|---|---|
| **dsp-safety-audit** | v1.1 | **VALIDATED** |
| **gui-performance-audit** | v1.1 | **VALIDATED** |
| **ai-integration-audit** | v1.1 | **VALIDATED** (P2/Atomics verified) |
| **release-verdict-engine** | v1.0 | **REVIEWED** |

---

## 10. Instructions for AI Platforms

- **Do not describe the current project as release-safe.**
- Treat `feature/unified-exp-best` as an isolated experimental consolidation branch.
- Do not promote Motore v2, M9 assets, or any model blob into a product release without a separate A6/A7 sign-off.
- Keep KnownDebt visible. Do not convert a disabled/quarantined test into a green release claim.
