# Round 3 — Project the full SemanticState; do not undo every slider tick

**When:** 2026-08-31 08:37:47 CEST  
**Wait:** ~16.2 min after Round 2 (08:21:34 → 08:37:47).  
**Using brainstorming skill to** stress-test Round 2’s apply/undo model against the live slider + morph path.

## Evaluation of Round 2

### 1. What it claimed

- Key occupancy by driver, not complementary `sourceQuality`.
- Skip complementary at apply (`bool complementary`).
- 1:N bands per quality, no-clobber, persist, **one `pushUndoState` per `applySemanticAdjustments`**.
- RESET deferred.

### 2. What the codebase actually does

**Keep:** complementary skip is a real fork. Air’s complementary is Clarity at 0.2 (`SemanticEQEngine.cpp:179–181`). Clarity primary bands are **three**: Peak 250 (−2.5 dB), 400 (−2.0), 3000 (+2.0) (`525–528`). Today, dragging Air also claims/overwrites a Clarity slot with those three last-write-wins (3000 Hz survives). Skipping complementary **changes the current sound** of Air (no hitchhiking mud cut / presence boost). That is a product delta, not a free cleanup. Acceptable for YAGNI if documented; not invisible.

**Falsify / reshape Round 2 undo:**

`SemanticControlPanel::sliderValueChanged` calls `setQuality` + `updateEQFromState()` on **every tick** (`SemanticControlPanel.h:251–270`). Morph: `timerCallback` at 30 Hz calls `updateMorph` + `updateEQFromState()` while `isMorphing()` (`236–244`). `updateEQFromState` always regenerates **all** non-zero qualities (`generateEQFromState` loop `SemanticEQEngine.cpp:721–730`), not a delta of the moved slider.

Therefore `applySemanticAdjustments` is a **high-frequency full projection**, not an “Apply” action. `HistoryManager` max depth is 20 (`kMaxHistorySize`, `HistoryManager.h:26`). `pushUndoState` on every apply would fill the stack in under a second of morph and destroy undo for AI fixes. **Round 2’s undo requirement is harmful.** Do not add it on the live path.

Existing `setBandState` already `beginChangeGesture`/`endChangeGesture` per param per call (`PluginProcessor.cpp:3153–3201`). That is already host-automation noisy. Occupancy work must not add HistoryManager on top. Optional undo only on **discrete** actions: text APPLY, RESET (when we do it).

**Gap Round 2 under-specified:** when Air returns to 0, `generateEQFromState` omits Air (`|amount|≤0.01`). `applySemanticAdjustments` only writes listed adjustments; it **never releases** previous Air slots. Zeroing a slider is the same hole as RESET, and it happens on the main path. Deferring RESET was wrong: **release-on-zero is the intent map**.

### 3. Keep / reshape / kill

| Piece | Verdict |
|---|---|
| 1:N no-clobber | **Keep** |
| Skip complementary this slice | **Keep**, document Air sound change (no Clarity hitchhike) |
| Persist occupancy | **Keep** |
| Undo every apply | **Kill** |
| Defer all release | **Kill** — release when quality absent from the batch |
| Legend UI / Assist pin / merge-TODO | still dead |

### 4. Decision: **evolve**

## Approaches

### A — Discrete “Apply” button only (sliders preview, APPLY commits)

Would need a preview path and stop live `onEQGenerated` on slider. Big UX break vs current “sliders are live”. **Reject** for this slice (behavior change the user did not ask for).

### B — Full-state projector (recommended)

`applySemanticAdjustments(adjustments)`:

1. Build `wanted[quality] = list of primary (non-complementary) adjustments`.
2. For each quality with `|amount|` implied by non-empty list: claim/reuse 1:N unused-or-owned bands, write them.
3. For each quality in occupancy whose list is **empty** this batch: zero gain, disable, clear those indices (release).
4. Never `pushUndoState` here.
5. Persist occupancy in plugin state.
6. Skip complementary rows.

This is the Semantic Intent Map: **SemanticState → owned bands**, continuously, without eating undo.

### C — Disconnect to AI `applySingleCorrection` no-clobber

Still useful, still not the named map. Stay on semantic.

## Recommended design (B)

**Invariants:**

- Message thread only.
- Complementary ignored at apply (engine may still generate them; Processor drops them).
- Occupied band is writable by that quality even if `|gain|≥0.35` (it is owned).
- Foreign band with `|gain|≥0.35 && enabled` is never taken; skip that adjustment.
- Quality dropped from batch → owned bands disabled, occupancy cleared for that quality.
- No HistoryManager on slider/morph.
- `PluginProcessor.cpp/.h` required; `processBlock` not.

**Tests (expand):**

1. Air 0.8 → two distinct bands, HighShelf ~10 kHz and Peak ~14 kHz.
2. Air 0.8 then Air 0 → those two disabled/zero, occupancy empty for Air.
3. Manual band 1 kHz +6 dB remains after (1) and (2).
4. Air 0.8 does not change a pre-existing Clarity occupancy (complementary skipped).
5. `getStateInformation` / `setStateInformation` round-trip occupancy + band params (`IntegrationStateTest` pattern; add file to CMake).

**Still out:** RESET button special-case (falls out of (2) if sliders zeroed first — `resetAllSliders` already zeros engine then `updateEQFromState`; **RESET starts working once release-on-zero exists**, no extra RESET feature). Graph, pin, VPA, `processBlock`.
