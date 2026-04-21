# Source/AI Stack — Verified Audit 2026-04-21

## Context

Follow-up to `KNOWN_ISSUE_pluginval_s10_param_thread_safety.md` (commit TBD).
A previous audit of `Source/AI/` performed by a Claude Code subagent contained
unverified claims about the stack's architecture. This file verifies a
specific subset of those claims directly against the source on
`feature/post-meeting-fixes` @ `d06bc92a`, with file paths, line ranges, and
snippets.

**Scope: 5 specific claims. This is NOT a full audit of `Source/AI/`.**

## Verified Facts (already established, for reference)

- Pluginval strictness 10 SIGSEGV preexists CRIT-1 (verified on parent commit
  `49f2a46f` and captured via lldb on `d06bc92a`).
- The fault frame is `juce::ClientRemappedBuffer<float>::copyToHostOutputBuses`
  inside JUCE's VST3 wrapper — NOT inside any DSP processor of this plugin.
- `Source/AI/` contains **10,590 LOC** total across 19 files.
- `NeuralNetworkWrapper.{cpp,h}`, `OnlineLearningSystem.{cpp,h}`,
  `MultiTrackUnmasking.{cpp,h}` all exist on disk in `Source/AI/`.

## Claim 1 — NeuralNetworkWrapper training

**Verdict: HARD_CODED_FALSE**

- File sizes: `NeuralNetworkWrapper.cpp` 411 lines, `NeuralNetworkWrapper.h` 153 lines.
- Public entry point: `NeuralNetworkWrapper::startOnlineTraining` at
  `Source/AI/NeuralNetworkWrapper.cpp:329-344`.
- It delegates to `pImpl->startTraining` (Pimpl inner class) at line 174-194.
- The inner `startTraining` body:

```cpp
// Source/AI/NeuralNetworkWrapper.cpp:174-194
bool startTraining(const std::vector<TrainingSample>& samples, const TrainingConfig& config)
{
    if (samples.empty())
        return false;

    // TFLite C API (v2.x) does not expose backpropagation or fine-tuning of
    // FlatBuffer models at runtime. Gradient-based training requires the
    // TFLite Model Maker Python library or a custom training delegate.
    // We log the intent so the caller knows why this returns false, and the
    // outer AIEngine will fall back to heuristic adjustments instead.
    juce::ignoreUnused(config);
#if defined(AIEQ_ENABLE_TFLITE)
    AIEQ_LOG_WARNING("NeuralNetworkWrapper: TFLite C API does not support online "
                     "fine-tuning. Use TFLite Model Maker offline to retrain.");
#else
    AIEQ_LOG_WARNING("NeuralNetworkWrapper: No ML backend compiled in — "
                     "training unavailable. Build with AIEQ_ENABLE_TFLITE to enable inference.");
#endif
    trainingActive.store(false, std::memory_order_release);
    return false;
}
```

The method returns `false` unconditionally (the only other early exit also
returns `false`). An explanatory warning log is emitted. No branch inside this
function returns `true`.

`exportToONNX()` has analogous behavior at
`Source/AI/NeuralNetworkWrapper.cpp:366-376`: logs a warning then returns false.

## Claim 2 — OnlineLearningSystem status

**Verdict: UPSTREAM_DEPENDENT_STUB (relies on `NeuralNetworkWrapper::startOnlineTraining`, which returns false — see Claim 1).**

- File sizes: `OnlineLearningSystem.cpp` 288 lines, `OnlineLearningSystem.h` 134 lines.
- No `EXPERIMENTAL` / `DISABLED` / `TODO` / `FIXME` / `#if` markers found inside
  `OnlineLearningSystem.cpp` (zero matches via grep).
- The header at `Source/AI/OnlineLearningSystem.h:12-24` describes the class
  neutrally as "Online Learning System for Real-Time Model Updates" with
  features including "Incremental gradient updates". No status annotation.
- Main entry point: `OnlineLearningSystem::performUpdate` at
  `Source/AI/OnlineLearningSystem.cpp:99-165`. Key excerpt:

```cpp
// Source/AI/OnlineLearningSystem.cpp:141-164
if (model.startOnlineTraining(trainingSamples, trainConfig))
{
    lastUpdateTime = currentTime;
    stats.updatesPerformed++;

    // Compute a proxy MSE loss from the training targets against their
    // zero-baseline (what the model would output without any correction).
    // This is a conservative lower-bound proxy; true loss would require
    // running inference before and after the update, which the TFLite C API
    // does not support for online training.
    float proxyLoss = 0.0f;
    for (const auto& ts : trainingSamples)
    {
        if (ts.target.empty()) continue;
        float sampleMse = 0.0f;
        for (float v : ts.target)
            sampleMse += v * v;
        proxyLoss += sampleMse / static_cast<float>(ts.target.size());
    }
    if (!trainingSamples.empty())
        proxyLoss /= static_cast<float>(trainingSamples.size());

    updateStats(proxyLoss);
}
```

- The block that computes `proxyLoss` and calls `updateStats` is gated by
  `model.startOnlineTraining(...) == true`. Per Claim 1, that call always
  returns `false`. Therefore the `proxyLoss` computation is unreachable at
  runtime under the current backend.
- The comment at lines 146-150 explicitly documents the TFLite C API limitation.

## Claim 3 — MultiTrackUnmasking usage

**Verdict: USED_IN_CODEBASE, DISABLED-BY-DEFAULT with explicit comment.**

- File sizes: `MultiTrackUnmasking.cpp` 332 lines, `MultiTrackUnmasking.h` 134 lines.
- References outside `Source/AI/MultiTrackUnmasking.*`:
  - `Source/AI/AIEngine.h:6` — `#include "MultiTrackUnmasking.h"`
  - `Source/AI/AIEngine.h:253-259` — `setMultiTrackUnmaskingEnabled` /
    `isMultiTrackUnmaskingEnabled` API
  - `Source/AI/AIEngine.h:287` — `getUnmaskingCorrections`
  - `Source/AI/AIEngine.h:730` — `std::unique_ptr<MultiTrackUnmasking> multiTrackUnmasking;`
  - `Source/AI/AIEngine.h:736` — `bool enableMultiTrackUnmasking = false;` (default off)
  - `Source/AI/AIEngine_Advanced.cpp:10-40` — actual call sites
    (`updateTrackSpectrum`, `setTrackActive`, `generateCorrections`)
  - `Source/GUI/AIControlPanel.h:75, 266` — UI toggle
  - `Source/GUI/AIProblemPanel.h:134-135, 225` — UI toggle
  - `Source/Tests/AIEngineTest.cpp:573, 579, 580, 592, 593` — enable/disable tests
- Explicit status comment in the header:

```cpp
// Source/AI/AIEngine.h:720-728
// SHIPPING:      AIEngine (heuristic detection), SemanticEQEngine,
//                ReferenceMatcher, AdaptiveAIEngine, UserLearning,
//                MLEngine (auto-loads ml_weights.bin; heuristic fallback if absent)
// EXPERIMENTAL:  OnlineLearningSystem (replay buffer works, training backend absent)
// DISABLED:      NeuralNetworkWrapper (requires TFLite model file),
//                MultiTrackUnmasking (requires multi-instance host support)
//
// Experimental/disabled modules are lazy-initialized and gated by feature flags.
// They compile and link but have no runtime cost when disabled (default).
```

- No `#if`/`#ifdef` that conditionally compiles `MultiTrackUnmasking.cpp`
  out: `grep "#if|#endif|DISABLED|TODO|EXPERIMENTAL"` on the cpp returns no
  matches.
- Net: the class is linked into the binary, toggleable from the UI, tested,
  but default off with a header comment declaring it "DISABLED".

## Claim 4 — ML detection silent fallback

**Verdict: PARTIAL_SILENT_FALLBACK.**
- Silent when `ml_weights.bin` is absent.
- Explicit warning when present-but-load-fails.
- Explicit warning when TFLite NN inference fails within the ML path.

- `useMLDetection` is declared at `Source/AI/AIEngine.h:661`:
  `std::atomic<bool> useMLDetection { false };`
- `shouldUseMLDetection()` definition at `Source/AI/AIEngine.h:676-687`:

```cpp
bool shouldUseMLDetection() const noexcept
{
    auto mode = static_cast<DetectionBackendMode>(detectionBackendMode.load(std::memory_order_relaxed));
    if (mode == DetectionBackendMode::HeuristicOnly)
        return false;

    int forced = forceMLDetectionForTests.load(std::memory_order_relaxed);
    if (forced >= 0)
        return forced != 0;

    return useMLDetection.load(std::memory_order_relaxed);
}
```

- Auto-enable path at `Source/AI/AIEngine.cpp:105-127` (inside
  prepareToPlay-like init):

```cpp
if (!useMLDetection)
{
    auto mlModelPath = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                           .getParentDirectory()
                           .getChildFile("models")
                           .getChildFile("ml_weights.bin");
    if (mlModelPath.existsAsFile())
    {
        if (mlEngine.loadWeights(mlModelPath))
        {
            useMLDetection = true;
            AIEQ_LOG_INFO("ML model loaded: " + mlModelPath.getFullPathName());
        }
        else
        {
            AIEQ_LOG_WARNING("Failed to load ML model: " + mlModelPath.getFullPathName()
                             + " - using heuristic fallback.");
        }
    }
}
```

  - If `ml_weights.bin` does NOT exist: no log line; `useMLDetection` stays
    false. Silent.
  - If it exists but `loadWeights` returns false: explicit warning with
    `"using heuristic fallback"`.
- Branching in `analyzeSpectrum`, `Source/AI/AIEngine.cpp:246-255`:

```cpp
// Perform detection — routed via shouldUseMLDetection() which respects
// DetectionBackendMode, forceMLDetectionForTests, and useMLDetection.
if (shouldUseMLDetection())
{
    detectProblemsWithML();
}
else
{
    detectProblems();
}
```

- Inside `detectProblemsWithML()` at `Source/AI/AIEngine.cpp:2370-2386`,
  the optional TFLite neural modulation fails loudly:

```cpp
if (enableNeuralNetworks && neuralNetwork && neuralNetwork->isModelLoaded())
{
    const auto nnResult = neuralNetwork->runInference(linearSpectrum);
    if (nnResult.success && !nnResult.output.empty()) { ... }
    else
    {
        AIEQ_LOG_WARNING("TFLite inference failed or empty output. Falling back to classical ML.");
    }
}
```

- `#if defined(AIEQ_ENABLE_TFLITE)` appears only in
  `NeuralNetworkWrapper.{cpp,h}`, not in `AIEngine.{cpp,h}` or
  `MLEngine.{cpp,h}`. The classical `mlEngine.detectProblems` path runs even
  without TFLite compiled in.

## Claim 5 — pendingCorrections synchronization

**Verdict: MUTEX_PROTECTED (consistent use of `correctionsWriteMutex`).**

- Declaration at `Source/AI/AIEngine.h:604`:

```cpp
std::vector<Correction> pendingCorrections;
```

  Plain `std::vector`, not `std::atomic`, not `juce::SpinLock`.
- A `correctionsWriteMutex` (`std::mutex`) is held at every observed access
  that mutates OR reads the vector:

| Site | Path | Access | Lock acquired |
|------|------|--------|---------------|
| `getPendingCorrections` | `AIEngine.cpp:584-588` | read (copy out) | `correctionsWriteMutex` @586 |
| `approveCorrection` | `AIEngine.cpp:600-622` | erase + index r/w | `correctionsWriteMutex` @602 |
| `approveAllCorrections` | `AIEngine.cpp:624-643` | iterate + clear | `correctionsWriteMutex` @626 |
| `rejectCorrection` | `AIEngine.cpp:645-650` | erase | `correctionsWriteMutex` @647 |
| `clearCorrections` | `AIEngine.cpp:652-657` | clear | `correctionsWriteMutex` @654 |
| `saveAnalysisSnapshot` | `AIEngine.cpp:688-700` | copy out | nested `correctionsWriteMutex` @694 |
| `detectProblems` | `AIEngine.cpp:829-864` | clear + populate (via detectXxx) + sort | `correctionsWriteMutex` @831 |
| `detectProblemsWithML` | `AIEngine.cpp:2326+` | clear + populate + sort + dedupe | `correctionsWriteMutex` @2392 |

- Internal `push_back` sites inside the detectXxx helpers (`AIEngine.cpp`
  lines 1127, 1204, 1281, 1344, 1407, 1470, 1534, 1598, 2460) are invoked
  synchronously from `detectProblems()` / `detectProblemsWithML()`, which
  already hold the mutex at their top.
- External usage: `Source/PluginProcessor.cpp:4171` has a comment referencing
  `pendingCorrections` but does not access the vector directly — it goes
  through the `approve*` / `reject*` public API.

No direct unsynchronized access to `pendingCorrections` was found in this
audit.

## Claims NOT verified in this audit

The following claims from the earlier subagent audit remain NOT verified
here and should be treated as hearsay until checked:

- "7 detector functions in `AIEngine.cpp` share ~70% duplicated logic."
- "`SemanticEQEngine` Seq2Seq is a stub without `AIEQ_ENABLE_TORCH`."
- "~1500 LOC of dead code across the 3 sub-systems."
- Specific per-file LOC numbers previously quoted (NeuralNetworkWrapper 565,
  OnlineLearning 423, MultiTrackUnmasking 467). Actual LOC measured here:
  `NeuralNetworkWrapper` 564, `OnlineLearningSystem` 422, `MultiTrackUnmasking`
  466 — close but not identical.
- "AIEngine.cpp claims LOCK-FREE at line 199 but then grabs mutex at
  207-208." Line numbers are wrong; the actual `LOCK-FREE` comment at
  `AIEngine.cpp:234` refers to a triple-buffer publish, and the subsequent
  mutex at 241-244 protects a separately-named legacy field
  (`currentSpectrum`), not the triple-buffer. This specific "contradiction"
  claim is not supported by the code.

These require dedicated deeper review before acting on them.

## Summary of the 5 verdicts

| # | Claim | Verdict |
|---|-------|---------|
| 1 | NeuralNetworkWrapper training returns false | HARD_CODED_FALSE |
| 2 | OnlineLearningSystem is experimental / proxy loss | UPSTREAM_DEPENDENT_STUB |
| 3 | MultiTrackUnmasking is unused | USED_IN_CODEBASE, DISABLED-BY-DEFAULT |
| 4 | ML detection silently falls back | PARTIAL_SILENT_FALLBACK |
| 5 | pendingCorrections lacks synchronization | MUTEX_PROTECTED |

## Next actions (suggested, not decided)

- Decide whether to keep `NeuralNetworkWrapper::startOnlineTraining` in the
  public API given it cannot succeed with the current backend.
- Decide whether the `OnlineLearningSystem` proxy-loss path should either be
  reachable (alternate backend) or pruned.
- Decide whether the "silent when `ml_weights.bin` absent" behavior in
  `AIEngine::prepareToPlay` should emit a diagnostic (info-level) for
  observability.
- Open dedicated audit items for the 4 still-unverified claims listed above
  if they matter for current roadmap.
- No action implied for Claim 5: synchronization on `pendingCorrections`
  matches standard practice under the observed pattern.

---
Last updated: 2026-04-21 (late session closure)
