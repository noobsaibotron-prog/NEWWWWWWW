# Round 1 — Semantic Intent Map (occupancy 1:N, no-clobber)

**When:** 2026-08-31 08:04:49 CEST  
**Codebase:** `/workspace` branch `cursor/semantic-intent-map-brainstorming-895c` @ `ef67a844`  
**Using brainstorming skill to** produce the first proposal after reading the live tree (skill file absent; process followed).

## Context (verified, not gospel)

This is **not** the Ember Core freeze of 2026-08-29. Zero hits for `IntentEnvelope`, `isSameDisplayedProblem`, `AIEQ_SEMANTIC_EXP`, `EffectiveDSPState`, `AssistPinState`. What exists is AI Equalizer Pro v2.1 on GitHub: `AIEngine` + `AIProblemPanel`, `SemanticEQEngine` + `SemanticControlPanel`, APVTS bands, A/B/C/D slots.

Parked 2026-08-29 Assist graph pin: **not reopened**. `onProblemSelected` → `AdvancedSpectrumDisplay::highlightProblem` already exists (3s fade). That glue is out of scope.

`PluginProcessor.cpp/.h`: **this idea requires them** (assignment table lives there). Stated explicitly. No `processBlock` change.

## What the code actually does

### Semantic path (message thread)

1. `SemanticControlPanel::updateEQFromState()` (`Source/GUI/SemanticControlPanel.h` ~467) calls `semanticEngine.generateEQFromState({}, currentSampleRate)` then `onEQGenerated`.
2. Editor wires that to `processor.applySemanticAdjustments` (`Source/PluginEditor.cpp` ~59–63).
3. `SemanticEQEngine::generateEQFromState` (`Source/AI/SemanticEQEngine.cpp` ~721–736) emits **one `SemanticEQAdjustment` per definition band**, plus complementary bands. Example: Air is **two** bands (HighShelf 10 kHz + Peak 14 kHz) in `initializeQualityDefinitions()` (~175–178). Comment at 733–734: `TODO: Implement intelligent band merging` — unimplemented.
4. `AIEqualizerAudioProcessor::applySemanticAdjustments` (`Source/PluginProcessor.cpp` ~2971) maps **one int per `SemanticQuality`**: `std::array<int, SemanticEQEngine::numQualities> semanticBandAssignments` (`PluginProcessor.h` ~492), filled with `-1` in the ctor (~81).
5. `claimSlotForQuality` reuses that single slot. Multiple adjustments with the same `sourceQuality` **overwrite the same APVTS band**. Last write wins. The 10 kHz Air shelf is discarded when the 14 kHz peak is applied.
6. If no unused slot, it **clobbers the band with smallest |gain|** that is not solo (~3029–3035). Manual EQ is not sacred.
7. **No** `historyManager.pushUndoState` on this path (unlike `applySingleCorrection` ~2843).
8. **Not** written in `getStateInformation` / `setStateInformation` (~3221–3370). Host recall drops the map; the next slider move can steal new bands.
9. `RESET` (`resetAllSliders` ~445) zeros sliders and calls `updateEQFromState` with empty adjustments (`generateEQFromState` skips |amount|≤0.01). **Assigned bands keep their last gain.** Neutral semantic state ≠ clean EQ.
10. `IntegrationStateTest.cpp` exists on disk but is **not** in the CMake test target (`CMakeLists.txt` ~926–931 lists ParametricEQ, LinearPhase, DynamicEQ, FuzzBlockSize, TestMain only). No test covers semantic apply.

### AI path (contrast)

`applySingleCorrection` (~2831) scores all **active** bands, prefers unused (`|gain|>0.5` = used) and ~1/5 octave reuse, then **always writes a band** if `bestBand >= 0`. Can overwrite a used band. Does push undo. Also not a visual pin (parked).

## Assumptions

- User override: brainstorm **this** tree, not wait for `/private/tmp/ember-semantic-intent-map`.
- Ship a slice an engineer can TDD without Ember Envelope 2.0.
- YAGNI: no neural remap, no VPA, no Analyzer A/B rewrite.
- `processBlock` stays untouched.

## Approaches

### A — Legend only (UI list “Air → Band 7”)

Read `semanticBandAssignments` and paint a strip on `SemanticControlPanel`.

- Pro: no DSP, no APVTS, no Processor policy change.
- Con: the 1:1 overwrite bug remains. User already rejected “glue UX” that does not change sound. **Kill as the product.** Maybe a 30-line follow-on after a real policy fix.

### B — Occupancy table 1:N + no-clobber + persist + undo (recommended)

Replace `array<int, numQualities>` with `array<vector<int>, numQualities>` (or a small `SemanticOccupancy` struct: quality → list of band indices, max 4). `claimSlotForQuality` becomes `claimSlotsForAdjustments`: one unused band per adjustment, never overwrite `enabled && |gain|≥0.35 && not owned by this quality`. Persist as a `ValueTree` child `SemanticOccupancy` next to `SlotA`. One undo snapshot per `applySemanticAdjustments`. RESET that owns: disable or zero **only occupied slots**, then clear the map.

- Pro: matches the automation name; fixes a real last-write-wins bug; host recall; TDD-able without `processBlock`.
- Con: **must edit `PluginProcessor.cpp/.h`**. Complementary qualities currently stamp `sourceQuality` as the parent (`generateEQForQuality` complementary loop) — occupancy must either attribute complements to the parent quality or to `comp.quality`; pick one and test it.
- Risk: increasing `numActiveBands` already happens (~3067). Keep that.

### C — Full provenance platform (band source enum, AI+Semantic+Manual, graph badges)

Phase-4-sized. This tree has no `IntentEnvelope`. Too much. **Kill for this freeze.**

## Recommended design (B)

**Name:** Semantic occupancy map (intent → bands).

**Behavior:**

1. Message thread only (already true).
2. For each `SemanticEQAdjustment` in the batch, assign a **distinct** band. Prefer: already occupied by this `sourceQuality` at similar freq (reuse threshold ~1/5 octave, same idea as AI `0.148 * f`); else highest-index unused band (`!enabled || |gain|<0.35`, not solo, not claimed this batch).
3. If none: **skip** that adjustment (do not clobber). Surface skip count on `statusLabel`.
4. Persist occupancy in `getStateInformation` / restore in `setStateInformation`.
5. `pushUndoState("Semantic: …")` once per apply.
6. RESET: zero/disable occupied bands, `fill(-1)` / clear vectors, then generate (no-op).
7. Tests: new `Source/Tests/SemanticOccupancyTest.cpp` added to CMake; round-trip; Air uses two distinct bands; manual Peak at 1 kHz +6 dB is not overwritten.

**Out:** graph highlight, Assist pin, `processBlock`, AI `applySingleCorrection` rewrite (can share a helper later; not this slice).

**First engineer task:** extract `claimSlotForQuality` into a pure function over `(adjustments, current BandState[24], occupancy)` returning new occupancy + writes; unit test Air’s two bands.

## Visual companion

A 1-line occupancy strip would help. Not blocking; note only.
