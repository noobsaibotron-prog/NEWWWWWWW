# AIEQ Plugin Auditor — Regression Matrix

**Version:** 1.1
**Date:** 2026-04-04
**Scope:** Cross-domain conflict detection and regression prevention for the AI Equalizer Pro plugin.

---

## 1. Purpose

This document defines the operational regression matrix for the `aieq-plugin-auditor` Composite Skill. The matrix serves two functions: it **detects cross-domain conflicts** that no single sub-skill can see in isolation, and it **prevents regressions** when a sub-skill is promoted to a new version.

The matrix is not theoretical. Every conflict pair listed below was observed during the first full audit of the AI Equalizer Pro codebase (56 files, 3 domains, April 2026) and reconciled against Claude's counter-audit.

---

## 2. Conflict Pairs

Each conflict pair defines a boundary where two sub-skills' domains interact. The `evidence_anchor` points to the exact code pattern that creates the interaction. The `resolution_priority` determines which sub-skill's constraints take precedence when the conflict cannot be resolved without trade-offs.

### Pair 1: DSP ↔ AI — Audio Path Contamination

| Field | Value |
|---|---|
| **Sub-Skills** | `dsp-safety-audit` ↔ `ai-integration-audit` |
| **Watch For** | AI inference blocking the audio path |
| **Evidence Anchor** | `PluginProcessor.cpp` calls `aiEngine->analyzeSpectrum()`. If the AI engine acquires a `std::mutex` or runs synchronous inference, the audio thread stalls. |
| **Observed Instance** | v1.0 audit overclaimed this as CRITICAL. Claude's reconciliation proved `analyzeSpectrum()` runs on `aiAnalysisThread` (separate `std::thread`), not the audio thread. Actual risk: mutex contention on shared state, not direct audio blocking. |
| **Resolution Priority** | `dsp-safety-audit` — real-time safety is non-negotiable. |
| **Verification Protocol** | Trace the complete call chain from `processBlock()` to any AI method. If the chain crosses a thread boundary via lock-free queue, classify as SAFE. If it acquires a mutex, classify as HIGH. If it runs synchronous inference on the audio thread, classify as CRITICAL. |

### Pair 2: GUI ↔ DSP — Spectrum Pipeline Contention

| Field | Value |
|---|---|
| **Sub-Skills** | `gui-performance-audit` ↔ `dsp-safety-audit` |
| **Watch For** | Shared locks between GUI rendering and DSP processing |
| **Evidence Anchor** | `AdvancedSpectrumDisplay.h` uses `juce::SpinLock` for spectrum data. `SpectrumDisplayMapper.h` also uses `juce::SpinLock`. Under high CPU load, the GUI thread can spin-wait while the audio thread holds the lock. |
| **Observed Instance** | Both files use SpinLock independently. No deadlock observed, but priority inversion is possible under load. |
| **Resolution Priority** | `dsp-safety-audit` — the audio thread must never wait for the GUI thread. |
| **Verification Protocol** | Check if any lock acquired by the GUI thread is also acquired by the audio thread. If yes, verify that the audio thread always acquires first (no priority inversion). Recommend replacing shared locks with `AtomicSnapshot` from `LockFreeStructures.h`. |

### Pair 3: AI ↔ DSP — CaptureService Consumer Stall

| Field | Value |
|---|---|
| **Sub-Skills** | `ai-integration-audit` ↔ `dsp-safety-audit` |
| **Watch For** | AI thread stalling causes lock-free ring buffer overflow |
| **Evidence Anchor** | `CaptureService.h` provides RT-safe ring buffers. The audio thread writes lock-free. If the AI consumer stalls (due to mutex or `MessageManagerLock`), the ring buffer overflows and drops samples. |
| **Observed Instance** | `dropCount` atomic tracks overflows. The AI thread uses `std::mutex` for shared state, which can cause stalls under contention. |
| **Resolution Priority** | `dsp-safety-audit` — data loss in the capture pipeline degrades AI analysis accuracy silently. |
| **Verification Protocol** | Verify that the AI consumer thread never acquires a `MessageManagerLock` or a `std::mutex` while consuming from the ring buffer. If it does, the consumer must be split: lock-free consumption first, then mutex-protected processing. |

### Pair 4: GUI ↔ AI — Direct Processor Calls

| Field | Value |
|---|---|
| **Sub-Skills** | `gui-performance-audit` ↔ `ai-integration-audit` |
| **Watch For** | GUI panels calling processor/AI methods directly instead of APVTS |
| **Evidence Anchor** | `AIControlPanel.h` and `AIProblemPanel.h` call processor methods directly. If those methods internally acquire AI mutexes, the message thread blocks. |
| **Observed Instance** | Both panels make direct calls. The AI engine uses `std::mutex` for shared state. Under contention, the GUI freezes. |
| **Resolution Priority** | `gui-performance-audit` — GUI responsiveness is a user-facing quality metric. |
| **Verification Protocol** | Verify that all GUI-to-processor communication uses APVTS parameter reads or lock-free FIFOs. Flag any direct method call that can transitively acquire a mutex. |

---

## 3. Regression Protocol

### When to Run

The regression matrix is triggered by **any** of the following events:

| Trigger | Description | Scope |
|---|---|---|
| **Sub-skill promotion** | Any sub-skill is promoted to a new version (e.g., `dsp-safety-audit` v1.1 → v1.2) | Full matrix: re-run all sub-skill evals on the same artifact bundle |
| **New conflict pair added** | A new cross-domain interaction is discovered and documented | Targeted: run evals for the two sub-skills in the new pair |
| **Codebase architectural change** | A major refactor changes the thread model, data flow, or module boundaries | Full matrix: re-run all sub-skill evals + re-verify all conflict pairs |
| **Post-remediation verification** | A fix for a conflict pair finding has been implemented | Targeted: re-run the specific conflict pair verification protocol |

### How to Run

The regression protocol follows 5 steps:

**Step 1 — Freeze the artifact bundle.** Identify the exact commit or branch to audit. All sub-skills must operate on the same snapshot.

**Step 2 — Re-run all sub-skill evals.** Execute each sub-skill's audit suite independently. Record findings per file.

**Step 3 — Execute conflict pair verification.** For each conflict pair, follow the `Verification Protocol` defined above. This is the cross-domain step that no single sub-skill can perform alone.

**Step 4 — Compare against baseline.** Compare the new findings against the previous audit's findings. Classify changes as:

| Classification | Meaning |
|---|---|
| **New Finding** | Issue not present in the previous audit — may be a regression introduced by the promotion |
| **Resolved Finding** | Issue present in the previous audit but now fixed — confirms the promotion worked |
| **Persistent Finding** | Issue present in both audits — not affected by the promotion |
| **Shifted Finding** | Issue moved from one domain to another — indicates a cross-domain side effect |

**Step 5 — Emit flags.** If any `New Finding` or `Shifted Finding` is detected, emit a `[REGRESSION FLAG]` and halt the promotion. The promotion cannot be finalized until the regression is resolved or explicitly accepted via `[HITL REQUIRED]`.

---

## 4. Conflict Severity Matrix

This matrix shows the severity of each conflict pair based on the combination of sub-skill findings. Read it as: "If sub-skill A reports X and sub-skill B reports Y, the cross-domain severity is Z."

| | AI: Clean | AI: HIGH (mutex) | AI: CRITICAL (sync inference) |
|---|---|---|---|
| **DSP: Clean** | No conflict | MEDIUM — mutex doesn't affect audio path | HIGH — inference may contaminate audio path |
| **DSP: HIGH (lock contention)** | LOW — DSP issue is isolated | HIGH — both domains have concurrency issues | CRITICAL — compounding concurrency failures |
| **DSP: CRITICAL (RT violation)** | HIGH — DSP issue is primary | CRITICAL — AI may worsen DSP issue | CRITICAL — both domains are broken |

| | GUI: Clean | GUI: HIGH (direct calls) | GUI: CRITICAL (thread violation) |
|---|---|---|---|
| **DSP: Clean** | No conflict | LOW — GUI issue is isolated | MEDIUM — GUI may contend with audio thread |
| **DSP: HIGH (lock contention)** | LOW — DSP issue is isolated | HIGH — shared locks may cause priority inversion | CRITICAL — GUI and DSP contend on same locks |
| **DSP: CRITICAL (RT violation)** | HIGH — DSP issue is primary | CRITICAL — GUI calls may worsen RT violation | CRITICAL — both domains are broken |

| | GUI: Clean | GUI: HIGH (direct calls) | GUI: CRITICAL (thread violation) |
|---|---|---|---|
| **AI: Clean** | No conflict | LOW — GUI issue is isolated | MEDIUM — GUI thread violation is independent |
| **AI: HIGH (mutex)** | LOW — AI issue is isolated | HIGH — direct calls may acquire AI mutexes | CRITICAL — GUI thread violation + AI mutex = deadlock risk |
| **AI: CRITICAL (sync inference)** | MEDIUM — AI issue is isolated | HIGH — direct calls may trigger sync inference | CRITICAL — compounding thread violations |

---

## 5. Evolution Path

This regression matrix is at version 1.1 and covers the 4 conflict pairs observed during the first audit cycle. As the plugin evolves, the matrix will grow. The following conflict pairs are **anticipated** but not yet observed:

| Future Pair | Sub-Skills | Watch For | Trigger for Addition |
|---|---|---|---|
| DSP ↔ Build/CI | `dsp-safety-audit` ↔ (future) `build-ci-audit` | Compiler optimization flags that break real-time guarantees (e.g., `-ffast-math` causing NaN propagation) |  When a build-ci-audit sub-skill is created |
| AI ↔ Preset/State | `ai-integration-audit` ↔ (future) `preset-state-audit` | AI corrections overwriting user preset state during `setStateInformation()` | When preset/state management is audited (RB-2/RB-3 area) |
| GUI ↔ Accessibility | `gui-performance-audit` ↔ (future) `accessibility-audit` | Performance optimizations that break screen reader compatibility | When accessibility requirements are formalized |
| DSP ↔ Oversampling | `dsp-safety-audit` ↔ (future) `oversampling-audit` | Oversampling quality vs. CPU budget trade-offs, anti-cramping verification | When oversampling is promoted from feature to audited subsystem |

These pairs will be added to the matrix only when a real test exposes a real conflict — not before.

---

## 6. AIEQ+ Governance Note

This regression matrix is in state **Tested**. It has been validated against one full audit cycle (v1.0) and one promotion cycle (v1.0 → v1.1). The conflict pairs are grounded in real evidence from the codebase, and the severity matrix is calibrated against Claude's reconciliation feedback. The matrix will be promoted to **Validated** after it successfully catches a regression during a future sub-skill promotion.
