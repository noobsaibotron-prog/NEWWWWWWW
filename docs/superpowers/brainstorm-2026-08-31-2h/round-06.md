# Round 6 — Lock the projector contract (no more architecture shopping)

**When:** 2026-08-31 09:25:03 CEST  
**Wait:** ~11.6 min after Round 5 (09:13:27 → 09:25:03), shortened on purpose.  
**Using brainstorming skill to** freeze an API after evaluating Round 5, not to invent a new product.

## Evaluation of Round 5

### 1. What it claimed

- Occupancy math as a POD `project()` testable in `AIEqualizerPro_Tests` (DSP-only).
- Do not construct `AIEqualizerAudioProcessor` in those tests (IR+AI threads in ctor).
- Processor remains a wrapper: apply + `SemanticIntent` ValueTree like SlotA.
- Reuse ratio `0.148 * f` from `applySingleCorrection`.
- Cap 4 bands/quality; slider sync with `isMouseButtonDown` guard.

### 2. What the codebase actually does

**Keep:** DSP test target still does not list Processor (`CMakeLists.txt:922–931`). Ctor threads still there (`PluginProcessor.cpp:93–103`). SlotA child pattern still the persist template (`3236–3264`). Intensity is **not** an APVTS parameter (`createParameters` has outputGain, dryWet, AI, analyzer, bands… **no** semantic intensity). It lives only on `SemanticEQEngine::globalIntensity`. Persist-in-ValueTree is the only recall path; do not add an automatable APVTS param (host would expose a mystery knob).

**Reuse threshold:** `applySingleCorrection` uses `targetFreq * 0.148f` with comment “~1/5 octave” (`PluginProcessor.cpp:2869`). 0.148 is ≈ 2^(1/5)−1. **Keep that**, do not import Ember’s 0.25 octave.

**Include drift:** `SemanticEQEngine::numQualities` is the enum size (`SemanticEQEngine.h:85`). A local `kNumSemanticQualities = 32` **will rot**. `SemanticOccupancy.h` **must** include `SemanticEQEngine.h` for `numQualities` and may reuse `SemanticEQAdjustment` **once** the complementary field exists. Until then, tests can build a local `Adj` struct; production projector should use the engine struct after the field is added.

**Falsify “header-only if needed”:** `SemanticEQAdjustment` has `juce::String description` (`SemanticEQEngine.h:101`). Copying vectors of those on a 30 Hz morph is existing cost. Projector should key on `sourceQuality`, floats, `filterType`, `complementary` — ignore description.

### 3. Keep / reshape / kill

| Piece | Verdict |
|---|---|
| POD `project()` in DSP tests | **Keep** |
| Full-plugin test exe | **Stay killed** |
| New APVTS `semanticIntensity` param | **Kill** |
| Invent a different reuse window | **Kill** |
| Complementary skip | **Keep** |
| Architecture alternatives A/C from R1–R4 | **Stay killed** — stop shopping |

### 4. Decision: **evolve** (lock contract)

## Approaches (narrow)

### A — Projector mutates in place

Harder to assert in tests. **Reject.**

### B — Pure function returning a delta (recommended)

```
struct BandSnap { float freq, gain, q; int type; bool enabled, solo; };
struct Occupancy { std::array<std::vector<int>, SemanticEQEngine::numQualities> slots; };
struct ProjectIn {
  std::array<BandSnap, 24> bands;
  Occupancy occ;
  std::vector<SemanticEQAdjustment> adjustments; // complementary skipped inside
  int numActiveBands;
};
struct ProjectOut {
  std::array<BandSnap, 24> bands;
  Occupancy occ;
  int desiredActiveBands;
  int skipCount;
  int releaseCount;
};
ProjectOut project(const ProjectIn&);
```

Algorithm:

1. Drop `complementary` (or description prefix if field not yet added — **field is required**, prefix is forbidden).
2. Group remaining by `sourceQuality`.
3. `needed = group[q].size()` clamped to 4.
4. Reuse existing `occ.slots[q][i]` if index in 0..23 and freq within `0.148 * target` of adjustment i (order: sort adjustments by freq so reuse is stable).
5. Else claim from 23..0 unused (`!enabled || abs(gain)<0.35`, `!solo`, not in any occ this pass, not in `claimedThisBatch`).
6. Else `skipCount++` (do not clobber).
7. Write claimed snaps from adjustments; `solo` forced false; `enabled` from adj.
8. Qualities with empty group: for each leftover index in `occ.slots[q]`, set gain 0, enabled false; clear vector; `releaseCount++` per band.
9. `desiredActiveBands = max(numActiveBands, max used index+1)`.

ValueTree: child `SemanticIntent` with `intensity`, `q0`…`q{N-1}`, and `o0`… as comma-separated ints. Same tree as SlotA (sibling).

### C — Disconnect to AI correction occupancy

Would still need the same projector. **Do not disconnect.**

## Recommended: B, frozen

This is the feature: **Semantic occupancy projector + persist + GUI hydrate**. Next round may only kill extras, not reopen merge-TODO / Assist pin / processBlock.

**PluginProcessor required** for wrapper + persist. **SemanticEQEngine.h** required for `complementary` flag. **SemanticControlPanel.h** required for slider hydrate. **processBlock** not.
