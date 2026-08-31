# Round 4 — Persist SemanticState with occupancy or recall will self-destruct

**When:** 2026-08-31 08:54:13 CEST  
**Wait:** ~16.4 min after Round 3 (08:37:47 → 08:54:13).  
**Using brainstorming skill to** falsify Round 3 persistence against host recall + editor slider init.

## Evaluation of Round 3

### 1. What it claimed

- `applySemanticAdjustments` is a full projector: write 1:N for qualities in the batch, **release** qualities absent from the batch.
- No HistoryManager on the live path.
- Persist occupancy. RESET “falls out” of release-on-zero.
- Complementary skipped; `PluginProcessor` required.

### 2. What the codebase actually does

**Keep:** live path is a full `generateEQFromState` (`SemanticControlPanel.h:467–477` every slider tick and 30 Hz morph). Release-on-zero is the only way RESET (`resetAllSliders` → empty adjustments) becomes a real reset. Undo-per-apply remains wrong (`kMaxHistorySize = 20`). Complementary skip still a documented sound change (Air no longer hitchhikes Clarity’s 3 peaks).

**Falsify Round 3 persist-occupancy-only:**

`SemanticEQEngine::currentState` is **not** in `getStateInformation` (`PluginProcessor.cpp:3221+`). Only APVTS + A/B/C/D slot trees. Engine is constructed empty; sliders in `setupQualitySlider` **always** `setValue(0.0)` (`SemanticControlPanel.h:323`). Editor has **no** `setStateInformation` hook and does not `syncSlidersFromEngine` on open.

Recall sequence if we persist occupancy but not qualities:

1. User had Air=0.8, two owned bands with shelf+peak in the DAW state.
2. Host restores APVTS bands + occupancy map.
3. Engine qualities = 0; GUI sliders = 0.
4. User moves Warmth. `generateEQFromState` emits **only Warmth**. Projector **releases Air** (absent from batch). Recalled Air EQ is destroyed.

Round 3’s persist plan **without SemanticState** is a footgun. Occupancy is meaningless unless the sliders match the engine that produced it.

**Also:** `setQuality` can damp an opposite quality (`SemanticEQEngine.cpp:983–1001`, e.g. Air vs Darkness if `|amount|>0.3` and same sign). The projector already follows `generateEQFromState` after that mutation — good. Do not special-case opposites.

**Behavior change honesty:** today, zeroing sliders **leaves** EQ (`adjustments.empty()` return). Round 3 release-on-zero **changes** that: sliders become a live macro. That is the feature. Call it out; do not pretend it is a silent bugfix only.

### 3. Keep / reshape / kill

| Piece | Verdict |
|---|---|
| 1:N no-clobber projector | **Keep** |
| Release absent qualities | **Keep** (live macro; breaking vs today, intended) |
| No live HistoryManager | **Keep** |
| Persist occupancy alone | **Kill** |
| Persist SemanticState + occupancy + intensity | **Required** |
| Editor slider init from engine | **Required** (GUI, not processBlock) |
| Assist pin / merge TODO | dead |

### 4. Decision: **evolve**

## Approaches

### A — Do not persist occupancy; rebuild from bands on load

Cannot map 10 kHz HighShelf back to Air vs Brilliance vs Sizzle without tags. **Reject.**

### B — Persist qualities + occupancy + `globalIntensity`; hydrate engine in `setStateInformation`; init sliders from engine (recommended)

`ValueTree("SemanticIntent")` child:

- `intensity` float
- `qualities` comma-separated or 32 properties `q0`…  
- `occ<i>` string of band indices `"7,6"` or child nodes

Restore into `semanticEngine.setSemanticState` + occupancy array. `SemanticControlPanel` constructor: `syncSlidersFromEngine()` and intensity slider from engine. If editor already open, `setStateInformation` already `callAsync` for slot restore (`PluginProcessor.cpp:3345`); add `semanticHydrateToken` or reuse that lambda to notify the editor. **YAGNI notify:** constructor + existing 20 Hz editor timer is enough if the panel **syncs from engine when not dragging** (compare values, `dontSendNotification`). Prefer constructor + one `sync` at end of `setStateInformation` via `changeListener` only if we already have a listener. Check later in plan: smallest is **constructor sync** (covers reopen) and **timerCallback sync when `!isMorphing()` and no slider is dragged** (covers recall with editor open).

### C — Disconnect: persist nothing; occupancy is session-only

Recall still loses the map; next Warmth move would last-write-wins on new high-index bands **and** leave orphan Air EQ. Session-only occupancy is weaker than today. **Reject** if we ship release-on-zero.

## Recommended design (B)

The intent map is a **triple**: `SemanticState` + occupancy + the APVTS bands it owns. All three must round-trip.

`PluginProcessor.cpp/.h` **required** for serialize/hydrate. `SemanticControlPanel.h` **required** for slider init/sync. `processBlock` **not** required. `SemanticEQEngine.h` maybe a getter/setter for intensity already exists (`setIntensity` used in panel).

**Tests:**

- Round-trip: set Air=0.8 via engine, apply, `getStateInformation`, new processor `setStateInformation`, engine Air≈0.8, occupancy two indices, those bands still shelf+peak.
- After recall, apply Warmth only: Air bands **remain** (engine still has Air). This is the test that kills persist-occupancy-only.

**Out:** live undo, complementary apply, graph, pin, `processBlock`.
