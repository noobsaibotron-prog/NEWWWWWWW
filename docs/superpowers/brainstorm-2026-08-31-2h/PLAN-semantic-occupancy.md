# Implementation Plan: Semantic occupancy projector

> **For the next engineer:** TDD, one task per commit. Do **not** implement Assist pin. Do **not** edit `processBlock`. `PluginProcessor.cpp/.h` are required from Task 5 onward — say so in the commit message. Rounds 1–7 wrote **no** production feature code; this plan is the first implementation work.

**Goal:** Map each primary `SemanticQuality` to 1..N EQ bands without clobbering hot user bands; persist `SemanticState` + occupancy + intensity; grow `numActiveBands` from the first free slot.

**Architecture:** Pure `project()` in `Source/AI/SemanticOccupancy.{h,cpp}` linked into `AIEqualizerPro_Tests`. `AIEqualizerAudioProcessor::applySemanticAdjustments` becomes a message-thread wrapper. Persist as `ValueTree` child `SemanticIntent` beside `SlotA`. GUI hydrates sliders from the engine.

**Spec:** `SPEC-semantic-occupancy.md` in this folder.

**Stack:** C++20, JUCE, existing `juce::UnitTest` in `AIEqualizerPro_Tests`.

---

## File structure (expected)

| File | Role |
|---|---|
| `Source/AI/SemanticOccupancy.h` | `BandSnap`, `Occupancy`, `ProjectIn`/`Out`, `project`, ValueTree helpers |
| `Source/AI/SemanticOccupancy.cpp` | Implementation |
| `Source/Tests/SemanticOccupancyTest.cpp` | Unit tests (no Processor) |
| `CMakeLists.txt` | Add the three files to `AIEqualizerPro_Tests` |
| `Source/AI/SemanticEQEngine.h` | `bool complementary = false` on `SemanticEQAdjustment` |
| `Source/AI/SemanticEQEngine.cpp` | Set `complementary = true` in the complementary loop (~802) |
| `Source/PluginProcessor.h` | Replace `array<int,N> semanticBandAssignments` with `Occupancy` |
| `Source/PluginProcessor.cpp` | Wrapper + persist |
| `Source/GUI/SemanticControlPanel.h` | Slider/intensity hydrate |

Do **not** add `SemanticEQEngine.cpp` or `PluginProcessor.cpp` to `AIEqualizerPro_Tests`.

---

## Task 1: Failing tests + CMake wire-up (FIRST ATOMIC COMMIT)

**Files:** `Source/Tests/SemanticOccupancyTest.cpp`, `Source/AI/SemanticOccupancy.h` (declarations only / empty `project` that returns input bands), `CMakeLists.txt`

**Steps:**

1. Add to `add_executable(AIEqualizerPro_Tests …)` after the DSP sources:
   - `Source/AI/SemanticOccupancy.cpp`
   - `Source/Tests/SemanticOccupancyTest.cpp`
2. `target_include_directories` already has `Source`.
3. Write `SemanticOccupancyTest` category `"SemanticOccupancy"`:
   - **Air two bands:** `numActiveBands=8`, all bands unused (enabled false for i≥8, gain 0). Two adjustments, `sourceQuality=Air`, freq 10000 type 3, freq 14000 type 2, `complementary=false`. Expect occupied indices **8 and 9**, types 3 and 2, `desiredActiveBands==10`, `skipCount==0`.
   - **No-clobber:** band 1 enabled, gain +6, freq 1000. Same Air pair. Band 1 unchanged. Air still 8 and 9.
   - **Release:** occupancy Air = `{8,9}` with those bands enabled; empty `adjustments`. Expect bands 8–9 `enabled==false`, `gain==0`, Air slots empty, `releaseCount==2`.
4. Run `AIEqualizerPro_Tests --category=SemanticOccupancy` (or all). Tests **must fail** until Task 3.
5. Commit: `test(semantic): add failing SemanticOccupancy projector cases`

**Stop.** Do not implement `project()` in this commit beyond a stub that compiles.

---

## Task 2: `complementary` flag on `SemanticEQAdjustment`

**Files:** `Source/AI/SemanticEQEngine.h`, `Source/AI/SemanticEQEngine.cpp`

**Steps:**

1. Add `bool complementary = false;` to `SemanticEQAdjustment` (after `enabled` or with metadata).
2. In `generateEQForQuality` complementary loop (~796–807), set `adj.complementary = true` (keep `sourceQuality = comp.quality`).
3. No test in DSP target for the engine (not linked). Optional: occupancy test passes an adjustment with `complementary=true` and expects it ignored (Task 3).
4. Commit: `feat(semantic): mark complementary EQ adjustments`

This is a **header-compatible** default (`false`).

---

## Task 3: Implement `project()` (TDD green)

**Files:** `Source/AI/SemanticOccupancy.cpp`

**Steps:**

1. Filter `adj.complementary`.
2. Group by `sourceQuality`.
3. For each quality with a non-empty group (stable sort by frequency):
   - Cap at `kMaxSlotsPerQuality`.
   - For each adj: nearest owned slot within `kReuseLinear * freq`; else claim lowest unused `>= numActiveBands`; else lowest unused not hot; else `skipCount++`.
   - Write `BandSnap` from adj; `solo=false`.
4. For each quality with empty group: release leftover slots.
5. `desiredActiveBands = max(in.numActiveBands, maxIndex+1)`.
6. Add tests: complementary ignored; skip when 24 hot bands; reuse same indices when Air freq unchanged.
7. Run tests until green.
8. Commit: `feat(semantic): implement occupancy projector`

---

## Task 4: ValueTree helpers + tests (no Processor)

**Files:** `SemanticOccupancy.cpp/.h`, `SemanticOccupancyTest.cpp`

**Steps:**

1. `occupancyToTree` / `occupancyFromTree` as in spec (`SemanticIntent`, `q*`, `o*`, `intensity`).
2. Test: Air=0.8 in `SemanticState`, occ slots `{8,9}`, intensity 1.5 → tree → back. Compare with epsilon 1e-4.
3. Invalid/missing tree returns false, leaves outputs default.
4. Commit: `feat(semantic): serialize SemanticIntent ValueTree`

---

## Task 5: Processor wrapper — **touches PluginProcessor.cpp/.h**

**Required Processor edits (explicit).** Not `processBlock`.

**Files:** `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`

**Steps:**

1. Replace `std::array<int, SemanticEQEngine::numQualities> semanticBandAssignments` with `AIEQCore::Occupancy semanticOccupancy` (namespace as implemented).
2. Ctor: default empty vectors, no `fill(-1)`.
3. Rewrite `applySemanticAdjustments` to fill `ProjectIn` from `getBandState`, call `project`, `setBandState` for changed snaps, bump `numActiveBands` param if needed, save occupancy.
4. Do **not** call `historyManager.pushUndoState` here.
5. Keep message-thread `callAsync` guard.
6. Cannot unit-test via `AIEqualizerPro_Tests`. Smoke: instantiate is heavy (threads) — skip. Manual: slider Air in standalone if you have a build.
7. Commit: `feat(semantic): apply occupancy projector from applySemanticAdjustments`

---

## Task 6: Persist + hydrate engine

**Files:** `PluginProcessor.cpp` `getStateInformation` / `setStateInformation`

**Steps:**

1. After SlotD, `state.removeChild(state.getChildWithName("SemanticIntent"), nullptr)` then `addChild(occupancyToTree(...))` using `semanticEngine.getSemanticState()` and `getIntensity()`.
2. On load, find child `SemanticIntent`; `occupancyFromTree`; `setSemanticState`; `setIntensity`.
3. Commit: `feat(semantic): persist SemanticIntent with plugin state`

---

## Task 7: GUI hydrate

**Files:** `Source/GUI/SemanticControlPanel.h`

**Steps:**

1. End of ctor: `syncSlidersFromEngine()`; `intensitySlider.setValue(semanticEngine.getIntensity(), dontSendNotification)`.
2. In `timerCallback`, if no quality slider `isMouseButtonDown()`, call `syncSlidersFromEngine()` (morph path already does).
3. `statusLabel` optional skip text — skip unless `project` result is plumbed via callback; **YAGNI** leave status as-is.
4. Commit: `feat(semantic): hydrate semantic sliders from engine state`

---

## Task 8: Regression sanity

**Files:** none if green

**Steps:**

1. Run full `AIEqualizerPro_Tests` (DSP + occupancy).
2. Grep: `processBlock` diff empty; no Assist pin; no VST3 copy.
3. Commit only if you fixed a test: `test(semantic): occupancy projector regressions`

---

## Test strategy

| Layer | What | Where |
|---|---|---|
| Projector | Air 1:N, no-clobber, release, complementary skip, skip-full, reuse, ValueTree | `SemanticOccupancyTest.cpp` |
| Engine flag | complementary bit | code review + projector test |
| Processor persist | not in DSP target | manual / future integration target — **out of this plan’s CI** |
| GUI | constructor sync | manual |

Do not claim CI covers Processor round-trip until a second test binary exists (out of scope).

---

## First atomic commit (copy-paste)

`test(semantic): add failing SemanticOccupancy projector cases`

Only: stub header/cpp, failing tests, CMake list update.
