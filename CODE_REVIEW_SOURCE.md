# Code Review: AI Equalizer Pro — `Source/`

**Branch:** `fix/code-review-march`
**Date:** 2026-03-24
**Scope:** 60 files, ~29,700 LOC across 8 modules
**Reviewer:** Claude (automated deep review)

---

## Executive Summary

The codebase demonstrates strong lock-free architecture fundamentals (ring buffers, atomic snapshots, parameter caching) and solid JUCE framework usage. However, **16 critical issues** were identified across thread safety, memory ordering, and numerical stability. The most pressing are race conditions in the GUI↔Processor synchronization, an incomplete seqlock in IR coefficient updates, and unbounded CAS loops in lock-free structures.

| Severity | Count |
|----------|-------|
| 🔴 Critical | 16 |
| 🟡 Warning | 38 |
| 🔵 Info | 15 |

---

## 🔴 Critical Issues (Priority Fix)

### C1. AtomicSnapshot::publish() — Unbounded CAS Loop (Livelock)
**File:** `Core/LockFreeStructures.h:73-80`
**Issue:** `compare_exchange_weak` loop has no retry limit. Under contention, producer can spin indefinitely.
**Fix:** Add `maxRetries = 10` with fallback `store()`.

### C2. AtomicSnapshot::read() — Incorrect CAS Logic
**File:** `Core/LockFreeStructures.h:94-102`
**Issue:** Inner `if (expected != currentRead)` after failed CAS is logically wrong — `expected` is updated by the CAS failure, not compared correctly against `currentRead`.
**Fix:** Simplify CAS logic; remove nested conditional.

### C3. CaptureService — Race on `currentSampleRate`
**File:** `Core/CaptureService.h:399`
**Issue:** Non-atomic `double` accessed from audio thread (`pushSamples`) and message thread (`prepare`) simultaneously.
**Fix:** `std::atomic<double> currentSampleRate { 44100.0 };`

### C4. CaptureService — Missing Acquire on Audio Thread Load
**File:** `Core/CaptureService.h:326`
**Issue:** `manualCaptureWritePos.load(memory_order_relaxed)` doesn't synchronize with the release store on message thread.
**Fix:** Use `memory_order_acquire`.

### C5. SpectrumAnalyzer — TOCTOU Race on Buffer Index
**File:** `DSP/SpectrumAnalyzer.cpp:165-166`
**Issue:** `activeBufferIndex` loaded twice without synchronization. Between loads, another thread could flip the index, aliasing read and write buffers.
**Fix:** Single `acquire` load, derive `writeIndex = 1 - readIndex`.

### C6. LinearPhaseProcessor — activeSlot Re-Read Per Sample
**File:** `DSP/LinearPhaseProcessor.cpp:95`
**Issue:** `activeSlot.load()` called per sample in OLA loop. If slot changes mid-block, IR switches mid-process → audible clicks.
**Fix:** Cache `activeSlot` once at block start.

### C7. DynamicEQProcessor — Lookahead Index Race
**File:** `DSP/DynamicEQProcessor.cpp:323-326`
**Issue:** `lookaheadWritePos` updated mid-block (line 213) but read position captured before the loop (line 185). Stale indices cause detector misalignment.
**Fix:** Update `lookaheadWritePos` once after the full block write.

### C8. MLEngine — NaN Propagation in dB Conversion
**File:** `AI/MLEngine.cpp:403`
**Issue:** `log10(energy + 1e-10f) / log10(1e-10f) * -1.0f` is numerically fragile.
**Fix:** Use `juce::Decibels::gainToDecibels(energy, -100.0f)`.

### C9. OnlineLearningSystem — Data Race in sampleFromBuffer()
**File:** `AI/OnlineLearningSystem.cpp:229-246`
**Issue:** Reads `replayBuffer` without holding `bufferMutex`. Function is `const` but accesses mutable shared state.
**Fix:** Acquire `bufferMutex` or document that caller must hold it.

### C10. AdaptiveAIEngine — Division by Zero in Energy Ratio
**File:** `AI/AdaptiveAIEngine.cpp:253`
**Issue:** `energyAfter / (energyBefore + 1e-10f)` unbounded if `energyBefore ≈ 0`.
**Fix:** `juce::jlimit(0.0f, 100.0f, energyAfter / (energyBefore + 1e-10f))`.

### C11. AIEngine — Unbounded CAS in publishSpectrum()
**File:** `AI/AIEngine.h:459-463`
**Issue:** Triple-buffer `compare_exchange_weak` loop has no retry limit.
**Fix:** Max 10 retries, then fallback `store()`.

### C12. GUI — Unsafe AIEngine Access from Timer Callback
**File:** `GUI/AIControlPanel.h:261,300`, `GUI/AIProblemPanel.h:98,134`
**Issue:** Timer callbacks directly call `processor.getAIEngine().getPendingCorrections()` without synchronization. Audio thread may be modifying AIEngine state concurrently.
**Fix:** Use atomic-backed cache or `RWLock`.

### C13. GUI — Use-After-Free on Timer Destruction
**File:** `GUI/AIControlPanel.h:162-168`, `GUI/LevelMeter.h:21-24`
**Issue:** Timer starts in constructor, destructor calls `stopTimer()`. If timer fires while destructor runs, accesses deleted members.
**Fix:** Stop timer **first** in destructor, before any member destruction.

### C14. GUI — Unsigned Underflow in drawProblems()
**File:** `GUI/AIControlPanel.h:367-373`
**Issue:** `corrections.size() - 6` underflows if fewer than 6 corrections (unsigned subtraction wraps to SIZE_MAX).
**Fix:** Guard with `if (corrections.size() > 6)`.

### C15. PluginProcessor — Incomplete Seqlock (Reader Missing)
**File:** `PluginProcessor.cpp:2183-2308`
**Issue:** `irCoeffVersion` is incremented (odd=writing, even=done) but the IR builder thread **never checks the version**. Reads may occur mid-write.
**Fix:** Add version-check loop in IR builder: load v1 → read data → load v2 → retry if v1 != v2.

### C16. PresetManager — Unchecked XML Dereference
**File:** `Utils/PresetManager.cpp:197`
**Issue:** `juce::ValueTree::fromXml(*stateXml)` without null check on `stateXml`.
**Fix:** Add `if (stateXml != nullptr)` guard.

---

## 🟡 Warnings (38 total, top 15 listed)

| # | File | Line | Issue | Category |
|---|------|------|-------|----------|
| W1 | CaptureService.h | 366-373 | Energy calculation O(n) per block without early exit | Performance |
| W2 | LockFreeStructures.h | 209 | False sharing on SPSCQueue head/tail | Performance |
| W3 | OSCParameterServer.h | 220 | No NaN/inf validation on incoming OSC floats | Correctness |
| W4 | HistoryManager.h | 267 | Implicit +1/-1 offset on numActiveBands | Correctness |
| W5 | ParametricEQProcessor.cpp | 730 | Default case returns AllPass instead of asserting | Correctness |
| W6 | DynamicEQProcessor.cpp | 387 | Division by `range` without zero check | Correctness |
| W7 | ParametricEQProcessor.cpp | 189-251 | Duplicate channel processing loop | Performance |
| W8 | DynamicEQProcessor.cpp | 430 | `processSample()` in loop prevents SIMD vectorization | Performance |
| W9 | ReferenceMatcher.cpp | 450-470 | O(n²) spectrum smoothing per frame | Performance |
| W10 | OnlineLearningSystem.cpp | 237 | `std::random_device` seeded every call | Performance |
| W11 | GUI/*.h | All | Header-only anti-pattern for 500+ line components | Maintainability |
| W12 | GUI (multiple) | Various | Hardcoded colors instead of ModernLookAndFeel::Colors | Maintainability |
| W13 | GUI (multiple) | Various | Timer `repaint()` every tick without dirty check | Performance |
| W14 | PluginProcessor.cpp | All | God class: 3630 lines, 180+ members | Architecture |
| W15 | Logger.cpp | 64 | `logFile.flush()` inside mutex (RT-safety violation) | Thread Safety |

---

## Architecture Observations

### God Class: PluginProcessor (3630 LOC)
`processBlock()` alone is ~750 lines. Responsibilities span DSP, parameter management, IR building, AI integration, capture service, undo/redo. Should be decomposed into:
- `DSPChain` — EQ processors and routing
- `IRBuilderService` — background thread, linear-phase IR
- `AIIntegration` — corrections, analysis command queue
- `ParameterSyncService` — APVTS ↔ audio thread bridging

### Bidirectional Parameter Sync Divergence
APVTS is authoritative, but `processAICommands()` directly mutates `eqProcessor` without updating APVTS. Host automation and AI corrections can silently diverge.

### Header-Only GUI Components
All 14 GUI components (except BandViewport) are header-only with full implementations. This increases compile time and leaks implementation details.

---

## What Looks Good

- **Lock-free ring buffer** for audio capture — zero allocations on audio thread
- **Parameter snapshot pattern** — loads all atomics once per block, uses locals in inner loops
- **Denormal protection** — `ScopedNoDenormals` in all three DSP processors
- **Cache-line alignment** — `alignas(64)` on hot buffers prevents false sharing
- **APVTS attachments** — all GUI controls properly use SliderAttachment/ComboBoxAttachment
- **OpenGL double-buffering** — spectrum renderer uses pending/active swap pattern
- **Pre-allocated FFT states** — all 4 analyzer resolutions allocated at construction
- **Crossfade on IR updates** — PartitionedConvolver avoids clicks during transitions
- **Exception safety** — PresetManager wraps state mutations in try/catch with restore
- **Test suite breadth** — DSP fuzz testing, block-size regression, linear-phase smoke tests

---

## Test Coverage Gaps

| Module | Coverage | Missing Tests |
|--------|----------|---------------|
| PluginProcessor | None | processBlock, state save/load, thread safety |
| GUI | None | Layout, resize, timer correctness |
| Logger | None | RT-safety, file rotation, overflow |
| PresetManager | None | XML round-trip, corruption recovery |
| DynamicEQ | Partial | Expansion, gating, knee, extreme params |
| AIEngine | Good | Temporal consensus enforcement (1-frame non-detection) |
| ParametricEQ | Good | ~80% API coverage |

---

## Recommended Fix Priority

### Immediate (before merge to main)
1. **C5** SpectrumAnalyzer TOCTOU → single acquire load
2. **C6** LinearPhaseProcessor → cache activeSlot per block
3. **C14** drawProblems unsigned underflow → bounds check
4. **C16** PresetManager null check → guard dereference
5. **C3/C4** CaptureService atomics → fix ordering

### High Priority (next sprint)
6. **C15** Complete seqlock in IR builder (add reader-side version check)
7. **C12** GUI thread safety → atomic cache for AIEngine state
8. **C13** Timer destruction order → stop before member cleanup
9. **C1/C2** AtomicSnapshot CAS logic → bounded retries + correct conditions
10. **C8/C10** Numerical stability (MLEngine, AdaptiveAIEngine)

### Medium Priority (tech debt reduction)
11. **W14** Split PluginProcessor into services
12. **W11** Move GUI implementations to .cpp files
13. **W13** Dirty-flag repaints instead of unconditional repaint()
14. Add Logger and PresetManager test suites
15. Centralize magic numbers and color constants
