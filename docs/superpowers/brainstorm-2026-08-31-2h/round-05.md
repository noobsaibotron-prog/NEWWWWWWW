# Round 5 — Pure projector in the DSP test target; do not boot PluginProcessor

**When:** 2026-08-31 09:13:27 CEST  
**Wait:** ~19.2 min after Round 4 (08:54:13 → 09:13:27). Cadence drift noted; later waits will be cut to hit 10:02 CEST.  
**Using brainstorming skill to** check Round 4’s test/persist plan against `CMakeLists.txt` and the processor constructor.

## Evaluation of Round 4

### 1. What it claimed

- Persist a `SemanticIntent` ValueTree (qualities + occupancy + intensity).
- Hydrate engine in `setStateInformation`; sync sliders (constructor + timer).
- Tests include processor round-trip after recall so Warmth does not release Air.
- Occupancy logic lives with `applySemanticAdjustments` in PluginProcessor.

### 2. What the codebase actually does

**Keep:** persist triple is still required for release-on-zero + recall. `setIntensity` / `getIntensity` exist (`SemanticEQEngine.h:253–254`, default 1.0). Slot children already ride on `apvts.copyState()` (`PluginProcessor.cpp:3236–3264`) — copy that pattern, do not invent a sidecar file.

**Falsify Round 4 test strategy:**

`AIEqualizerPro_Tests` is **explicitly DSP-only** (`CMakeLists.txt:916–931`):

```
# Unit tests - DSP-only, no AI/ML external dependencies (no TFLite, no Torch).
# Links only LinearPhaseProcessor, ParametricEQProcessor, DynamicEQProcessor
```

It does **not** compile `IntegrationStateTest.cpp`, `PluginProcessor.cpp`, or `SemanticEQEngine.cpp`. Round 1–4 “add SemanticOccupancyTest to CMake and construct `AIEqualizerAudioProcessor`” would either (a) not link, or (b) pull Torch/TFLite/IR+AI **threads**.

Constructor **always** starts `irBuilderThread` and `aiAnalysisThread` (`PluginProcessor.cpp:93–103`). A unit test that `AIEqualizerAudioProcessor proc;` is not a unit test.

`getIntensity` is there; persist of intensity is a one-liner, not a research gap.

**Timer sync (Round 4 B):** 30 Hz `timerCallback` already runs for morph (`SemanticControlPanel.h:236`). Syncing sliders every tick can fight a drag. Guard: `!isMorphing()` is wrong (morph **should** sync). Guard: skip if any quality slider `isMouseButtonDown()`. That is enough; no ChangeBroadcaster.

### 3. Keep / reshape / kill

| Piece | Verdict |
|---|---|
| Persist SemanticState + occupancy + intensity | **Keep** |
| Release-on-zero projector | **Keep** |
| Processor-constructed unit tests | **Kill** |
| Occupancy math inside PluginProcessor only | **Reshape** — extract pure function |
| 30 Hz slider sync with mouse-down guard | **Keep** (tiny GUI) |
| New full-plugin test binary | **Kill** this slice (YAGNI) |

### 4. Decision: **evolve**

## Approaches

### A — Link the full plugin into tests

Starts IR/AI threads, ML ifdefs, CI pain. **Reject.**

### B — `SemanticOccupancy` POD projector + ValueTree helpers, tested in `AIEqualizerPro_Tests` (recommended)

New files (no production feature yet — plan only this round):

- `Source/AI/SemanticOccupancy.h` (+ `.cpp` if non-inline)
- Types: band snapshot, adjustment (quality index, freq, gain, q, type, complementary flag), occupancy `array<vector<int>, N>` with `N = SemanticEQEngine::numQualities` **or** a local `kNumSemanticQualities` to avoid including the engine in tests.
- `project(bands[24], occupancy, adjustments) -> { bandsWrites, occupancy, skipCount, releasedCount }`
- `occupancyToValueTree` / `occupancyFromValueTree`
- `qualitiesToValueTree` / `from`

Tests: `Source/Tests/SemanticOccupancyTest.cpp` added to the **existing** DSP executable. No Processor. No Engine.cpp.

`PluginProcessor::applySemanticAdjustments` becomes: filter complementary, call `project`, `setBandState` for writes, save occupancy. `get/setStateInformation` writes/reads the ValueTree child like SlotA.

Processor still **required** for wiring + persist. `processBlock` not.

### C — Disconnect: skip persist, session-only projector

Still fails recall + release. **Reject.**

## Recommended design (B)

**First atomic commit for the next engineer:** `SemanticOccupancy.h` + `SemanticOccupancyTest.cpp` wired into `AIEqualizerPro_Tests`, with Air two-band claim and no-clobber tests **failing** until `project()` exists. No PluginProcessor in that commit.

**Cap:** max 4 bands per quality. Unused = `!enabled || abs(gain)<0.35`, not solo, not claimed this batch. Reuse owned slot if `|log2(f1/f2)|` small (use existing AI 0.148 linear ratio **or** log2 0.25 oct — pick **log2 ≤ 0.25** to match Ember folklore only if we implement; this tree’s AI uses `targetFreq * 0.148`. **Use the same 0.148 linear ratio** as `applySingleCorrection` for consistency in *this* repo.)

**PluginProcessor required:** yes, later tasks. **processBlock:** no.

**Cadence:** shortening remaining waits so Round 8 + MEGA finish by 10:02:22 CEST.
