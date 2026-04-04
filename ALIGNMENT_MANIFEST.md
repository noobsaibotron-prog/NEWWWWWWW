# ALIGNMENT_MANIFEST.md

> **READ THIS FILE FIRST.** This is the single source of truth for any AI platform working on the AI Equalizer Pro (AIEQ) project. It declares the canonical state of the codebase, the AIEQ+ framework, the last audit verdict, and the current priority. Do not rely on any other file for project-level context until you have read this one.

**Last Updated:** 2026-04-04 (Post-Wave 1 Initial Fixes)
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
| `feature/aieq-plus-framework` | Contains the full AIEQ+ framework skill tree. | Frozen — do not commit here. |

**Current HEAD:** `dbca05b1` (governance: add Promotion Tribunal Report v1.0)

---

## 3. Repository Structure

```
NEWWWWWWW/
├── ALIGNMENT_MANIFEST.md          ← YOU ARE HERE
├── Source/                         ← Plugin source code (91 files)
│   ├── AI/                         ← 20 files
│   ├── Core/                       ← 5 files
│   ├── DSP/                        ← 12 files (Wave 1 Fixes applied here)
│   ├── GUI/                        ← 17 files
│   ├── Tests/                      ← 29 files (Added D1PeakIdentityTest.cpp)
│   └── Utils/                      ← 5 files
├── review/
│   └── codex-2026-04-01/           ← Audit results and strategy documents
│       ├── AIEQ_RELEASE_VERDICT.md ← Official verdict (Update Pending)
│       ├── PROMOTION_TRIBUNAL_REPORT_v1_0.md ← Skill promotions (v1.1)
│       └── ...
└── REPORTS/
    └── WAR_ROOM_ARCH_REVIEW_2026-04-01.md ← Detailed closure dossiers
```

---

## 4. Current Audit Verdict: RELEASE-RISKY

| Metric | Value |
|---|---|
| **Verdict** | **RELEASE-RISKY** (Achieved 2026-04-04) |
| **Previous Verdict** | DO-NOT-RELEASE (5.61 / 10.0) |
| **Status** | All 6/6 NOT RELEASE-READY → RELEASE-RISKY gates met. |
| **Full Report** | `REPORTS/WAR_ROOM_ARCH_REVIEW_2026-04-01.md` |

### 4.1 Recent Fixes (Wave 1)

| Issue | File | Status | Fix Detail |
|---|---|---|---|
| **D1 Peak Identity** | `DynamicEQProcessor.cpp` | ✅ Fixed | `makeAllPass` → `makeBypass()` at gain ≈ 0. Fixed comb filtering. |
| **P1 ensureChannels** | `LinearPhaseProcessor.cpp` | ✅ Fixed | Removed heap-allocating `ensureChannels()` from `process()`. |
| **T-5 Oversized Block** | Multiple | ✅ Verified | Safety fallback (silence/clamp) accepted per Tribunal v4.2. |

---

## 5. Current Priority: Transition to RELEASE-SAFE

The project is moving from **RELEASE-RISKY** to **RELEASE-SAFE**.

### 5.1 Remaining Blocker: T-6 (OSC Logging)

| File | Location | Issue | Required Fix |
|---|---|---|---|
| **OSCParameterServer.h** | `Source/Core/` | Hardcoded Desktop logging | Remove `juce::File::userDesktopDirectory` logging. Use `juce::Logger` or conditional DBG macros. |

### 5.2 Next Steps for AI Platforms

1. **Verify T-6 Fix:** Implement the fix in `OSCParameterServer.h` to remove Desktop logging.
2. **Regression Testing:** Run `D1PeakIdentityTest` and existing integration tests.
3. **Audit Update:** Perform a delta-audit on fixed files to update the domain scores.
4. **Final Gate Check:** Verify all RELEASE-SAFE gates (Host matrix, Recall determinism, etc.).

---

## 6. AIEQ+ Framework State

| Sub-Skill | Version | State |
|---|---|---|
| **dsp-safety-audit** | v1.1 | **TESTED** |
| **gui-performance-audit** | v1.1 | **TESTED** |
| **release-verdict-engine** | v1.0 | **DRAFTED** |
| Others | v1.0 | Reviewed |

---

## 10. Instructions for AI Platforms

- **T-6 Fix is the immediate priority.**
- Use `review/codex-2026-04-01` branch.
- Consult `REPORTS/WAR_ROOM_ARCH_REVIEW_2026-04-01.md` for the full closure standard.
