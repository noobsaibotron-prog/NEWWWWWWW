# Spec: Semantic occupancy projector (intent map)

**Product:** AI Equalizer Pro (this GitHub tree, branch `cursor/semantic-intent-map-brainstorming-895c`)  
**Not:** Ember Core freeze 2026-08-29 (`IntentEnvelope`, `AIEQ_SEMANTIC_EXP`, VPA). Those symbols are absent here.  
**Date:** 2026-08-31  
**Status:** design only — do not treat this file as implemented code.

## Problem

`applySemanticAdjustments` (`PluginProcessor.cpp` ~2971) maps **one** APVTS band per `SemanticQuality` via `semanticBandAssignments` (`PluginProcessor.h:492`). Definitions such as Air use **two** primary bands (HighShelf 10 kHz + Peak 14 kHz, `SemanticEQEngine.cpp:175–178`). The second write overwrites the first.

Complementary rows stamp `sourceQuality = comp.quality` (`802`), so Air also last-write-wins a **Clarity** slot (Clarity has three peaks at 250 / 400 / 3000 Hz).

Empty batches return immediately (`2976–2987`): RESET and “slider back to 0” leave EQ stuck. Occupancy is not serialized (`getStateInformation` ~3221). Semantic apply does not use `HistoryManager` (only AI apply does). `AIEqualizerPro_Tests` is DSP-only and must not construct `AIEqualizerAudioProcessor` (ctor starts IR + AI threads, `93–103`).

## Goal

Ship one slice: **primary semantic qualities own 1..N bands**, never steal a hot user band, grow active count from the first free slot, release on zero, persist intent.

## Non-goals (OUT)

- Assist graph pin / `AssistPinState` / list-to-graph highlight (parked 2026-08-29; `highlightProblem` already exists).
- Phase 4 provenance platform / `IntentEnvelope`.
- Merging overlapping bands in `generateEQFromState` (TODO at ~733) as a substitute for 1:N.
- Applying complementary adjustments this slice (engine may still generate them).
- `HistoryManager` on slider/morph (30 Hz would trash a 20-deep stack).
- `processBlock` rewrite, VPA, Analyzer A/B, VST3 install, ditto SEMANTIC.
- New APVTS parameter for intensity (would show up as host automation).
- Full-plugin test executable.

## Invariants

1. Message thread only for APVTS writes (already true).
2. Complementary adjustments are ignored at project time (`complementary == true`).
3. A band is **hot** (untouchable unless already owned by this quality) if `enabled && abs(gain) >= 0.35f && !` already in this quality’s occupancy.
4. Unused: `!solo && (!enabled || abs(gain) < 0.35f)` and not owned by another quality this pass.
5. Claim order: lowest index `>= numActiveBands` that is unused; else lowest unused in `0..23` that is not hot.
6. Reuse: among this quality’s current slots, nearest frequency if `|f_slot - f_adj| < 0.148f * f_adj` (same ratio as `applySingleCorrection` ~2869).
7. Cap 4 slots per quality; extra adjustments increment `skipCount`.
8. Quality absent from the (non-complementary) batch: disable + zero gain on its slots, clear occupancy (`releaseCount`).
9. `desiredActiveBands = max(numActiveBands, maxWrittenIndex + 1)`.
10. `solo` always false on semantic writes.
11. Persist **triple**: `qualities[numQualities]`, occupancy vectors, `globalIntensity`.
12. No `pushUndoState` inside live `applySemanticAdjustments`.

## Types (illustrative — implement in `Source/AI/SemanticOccupancy.h`)

Reuse `SemanticEQEngine::numQualities` and, after Task 2, `SemanticEQAdjustment::complementary`.

```cpp
namespace ember { // or AIEQCore — pick AIEQCore to match HistoryManager
constexpr int kMaxEqBands = 24;
constexpr float kUnusedGainAbs = 0.35f;
constexpr float kReuseLinear = 0.148f; // ~1/5 octave, same as applySingleCorrection
constexpr int kMaxSlotsPerQuality = 4;

struct BandSnap {
    float freq = 1000.f, gain = 0.f, q = 1.f;
    int type = 2;
    bool enabled = true, solo = false;
};

struct Occupancy {
    std::array<std::vector<int>, SemanticEQEngine::numQualities> slots;
};

struct ProjectIn {
    std::array<BandSnap, kMaxEqBands> bands{};
    Occupancy occ{};
    std::vector<SemanticEQEngine::SemanticEQAdjustment> adjustments;
    int numActiveBands = 8;
};

struct ProjectOut {
    std::array<BandSnap, kMaxEqBands> bands{};
    Occupancy occ{};
    int desiredActiveBands = 8;
    int skipCount = 0;
    int releaseCount = 0;
};

ProjectOut project(const ProjectIn&);
juce::ValueTree occupancyToTree(const Occupancy&, const SemanticEQEngine::SemanticState&, float intensity);
bool occupancyFromTree(const juce::ValueTree&, Occupancy&, SemanticEQEngine::SemanticState&, float& intensity);
}
```

ValueTree type `SemanticIntent`, sibling of `SlotA`:

- property `intensity` (float)
- properties `q0` … `q{N-1}` (float)
- properties `o0` … `o{N-1}` (string `"8,9"` or empty)

## Processor wrapper

`applySemanticAdjustments`:

1. Early-out if not message thread (keep `callAsync`).
2. Build `ProjectIn` from `getBandState(0..23)`, current occupancy, `adjustments`, `getNumActiveBands()`.
3. `auto out = project(in)`.
4. For each band whose snap changed: `setBandState`.
5. If `out.desiredActiveBands > getNumActiveBands()`: set `numActiveBands` param as today (`3070–3076`).
6. Store `out.occ` in `semanticBandAssignments` **replacement** (`array<vector<int>>` or keep parallel structure; do not keep 1:1 `array<int>`).

`getStateInformation`: after SlotD, `state.addChild(occupancyToTree(...))`.  
`setStateInformation`: `occupancyFromTree`, `semanticEngine.setSemanticState`, `setIntensity`.

**This requires `PluginProcessor.cpp/.h`.** It does **not** require `processBlock` edits.

## GUI

`SemanticControlPanel`:

- Constructor: `syncSlidersFromEngine()`; intensity slider from `getIntensity()`.
- `timerCallback`: if no quality slider `isMouseButtonDown()`, `syncSlidersFromEngine()` (covers host recall with editor open + morph). Morph already calls `syncSlidersFromEngine`.

Optional: `statusLabel` shows `skipCount` if > 0. Not required for task 1.

## Behavior change (honest)

Today, zeroing sliders / RESET does **not** clear EQ. After this slice, sliders are a **live macro**: quality ~0 releases owned bands. Complementary hitchhike (Air→Clarity) **stops** at apply. First Air uses bands 8–9 on a default 8-band preset, not band 23.

## Risks

| Risk | Mitigation |
|---|---|
| Processor ctor threads in tests | Occupancy tests never construct Processor |
| Host recall wipes Air when Warmth moves | Persist qualities + occupancy together |
| 30 Hz morph + setBandState gestures | Existing; do not add HistoryManager |
| `SemanticEQEngine.h` in DSP tests | Include header only; do not link/construct engine |
| Torch in engine cpp | Do not add `SemanticEQEngine.cpp` to `AIEqualizerPro_Tests` |
| VPA / processBlock | Out of scope |

## Success

Air 0.8 → two distinct bands, types HighShelf and Peak, indices 8 and 9 if `numActiveBands` was 8. Air 0 → those disabled. Manual 1 kHz +6 dB band 1 unchanged. Complementary Clarity not occupied. ValueTree round-trip restores qualities and slots. `AIEqualizerPro_Tests` still DSP-only plus occupancy files.
