# Round 2 — Driver-keyed occupancy; complementary is a landmine

**When:** 2026-08-31 08:21:34 CEST  
**Wait before this round:** ~16.5 min after Round 1 files (08:04:49 → 08:21:34).  
**Using brainstorming skill to** evaluate Round 1 against the complementary loop, then evolve.

## Evaluation of Round 1

### 1. What it claimed

- `semanticBandAssignments` is 1:1 quality→band; Air’s two definition bands last-write-wins on one APVTS slot.
- Complementary adjustments are stamped with the **parent** quality.
- Map is not persisted; semantic apply has no undo; RESET does not release bands.
- Fix: 1:N occupancy + no-clobber + persist + undo + RESET-owned-slots.

### 2. What the codebase actually does

**Keep (confirmed):**

- 1:1 map: `std::array<int, SemanticEQEngine::numQualities> semanticBandAssignments` (`PluginProcessor.h:492`), ctor `fill(-1)` (`PluginProcessor.cpp:81`).
- Air is two bands: HighShelf 10 kHz + Peak 14 kHz (`SemanticEQEngine.cpp:175–178`).
- `claimSlotForQuality` returns the existing slot for that quality index (`PluginProcessor.cpp:3007–3010`). Second Air adjustment overwrites the same `setBandState`. Last write wins. **Not a guess.**
- Empty `adjustments` returns immediately (`2976–2987`). RESET → `generateEQFromState` with all zeros → empty vector → **bands unchanged**. Confirmed.
- `getStateInformation` serializes APVTS + SlotA–D only (`3221–3267`). No occupancy child. Confirmed.
- Semantic apply has **no** `historyManager.pushUndoState`. Only AI apply paths do (`2702`, `2843`).

**Falsify:**

Round 1 said complementary stamps the parent. **Wrong.** `generateEQForQuality` sets `adj.sourceQuality = comp.quality` (`SemanticEQEngine.cpp:802`), confidence 0.7. Air’s complementary is `{ Clarity, 0.2f }` (`179–181`). Moving **Air** therefore:

1. Claims/overwrites the **Air** slot twice (10k then 14k).
2. Claims a **Clarity** slot using Clarity’s own `bands` (not Air’s), at 0.2× amount.

If the user later moves the Clarity slider, `claimSlotForQuality(Clarity)` reuses that stolen slot. Occupancy keyed by `adj.sourceQuality` is not “user intent”; it is a mix of primary + side-effect qualities.

Most definitions use **two** primary `bands` (Air, Brilliance, Presence, Sizzle, …). The 1:1 map is systematically lossy, not an Air-only quirk.

`getBandState` comment says “Safe to call from any thread” (`PluginProcessor.h:209–212`); implementation reads APVTS raw pointers (`3081+`). Irrelevant to this slice; do not “fix” it here.

CMake still omits `IntegrationStateTest.cpp` / `SmoothedValueZipperTest.cpp` / `BlockSizeRegressionTest.cpp`. Round 1 was right that semantic apply is untested in the linked target.

### 3. Keep / reshape / kill

| Piece | Verdict |
|---|---|
| Last-write-wins is the bug | **Keep** |
| 1:N slots per quality | **Keep**, but key by **driver** (slider / parsed command quality), not raw `sourceQuality` |
| Persist + undo + RESET | **Keep**, but not all in task 1 |
| Legend-only UI | **Stay killed** |
| Full provenance platform | **Stay killed** |
| Engine TODO “merge overlapping bands” as the product fix | **Kill** — merging Air 10k+14k into one Peak destroys the written HighShelf+Peak design |
| Round 1 complementary attribution | **Killed**; must reshape |

### 4. Decision: **evolve** (not disconnect)

## Approaches (after evaluation)

### A — Merge in `generateEQFromState` (the existing TODO)

One adjustment per quality. 1:1 map stays.

- Pro: smallest Processor change.
- Con: Air is *defined* as two filters. Merge is a sound-design regression. **Reject.**

### B — Driver-keyed 1:N occupancy (evolve Round 1)

Occupancy: `SemanticQuality driver → vector of band indices` (cap 4). The apply batch is grouped by **the quality the user moved**, not by `adj.sourceQuality`.

Complementary this slice: **do not claim extra qualities**. Either (B1) skip complementary rows in `applySemanticAdjustments`, or (B2) rewrite complementary `sourceQuality` to the parent before claim. Prefer **B1** (YAGNI): complementary 0.2× Clarity while Air is up fights the Clarity slider and eats bands. Document as known engine behavior left untouched in `generateEQForQuality`; Processor ignores `sourceQuality != driver` if we pass driver explicitly.

Cleaner API: `applySemanticAdjustments(adjustments, driver)` is wrong for text commands that set many qualities. Better: **group by `sourceQuality` only for primary rows**; drop complementary in apply (`confidence < 1` is a hacky filter — complementary is 0.7, but learned/context might also change confidence). Safer: skip adjustments whose `description` starts with `"Complementary "` — **fragile**. Safest: add `bool complementary = false` on `SemanticEQAdjustment` (one field, engine sets it at line 802). Tiny engine header change, not a platform.

No-clobber: never write a band with `|gain|≥0.35 && enabled` unless already in this driver’s vector. Skip + count.

Persist: `ValueTree("SemanticOccupancy")` sibling of SlotA. Undo once per apply. RESET later.

**PluginProcessor required:** yes.

### C — Disconnect: no-clobber only on `applySingleCorrection`

Useful, different product. User asked for semantic intent map by branch name. **Do not disconnect** until occupancy is shown to be unshippable. It is shippable.

## Recommended design (B + complementary flag)

1. Add `bool complementary = false` on `SemanticEQAdjustment`; set true in the complementary loop.
2. `applySemanticAdjustments` skips `complementary == true`.
3. Group remaining by `sourceQuality`. For each group, assign **distinct** unused bands (high index first, same unused rule as today). Reuse this quality’s existing indices by nearest frequency (~1/5 octave) if still unused-or-owned.
4. No clobber. Skip extras if 24 bands exhausted.
5. Persist occupancy. One undo snapshot.
6. Tests: Air → two different band indices, types HighShelf then Peak; a manual 1 kHz +6 dB Peak is still there; complementary does not occupy Clarity.

**Still out:** Assist pin, `processBlock`, graph badges, merging TODO.

**Slim vs Round 1:** RESET-owned-slots is **task after** occupancy+persist. Do not block the map on RESET UX.
