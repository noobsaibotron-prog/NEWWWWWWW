# Round 7 — Do not freeze the 23→0 scan; grow active count from the first free slot

**When:** 2026-08-31 09:34:23 CEST  
**Wait:** ~9.3 min after Round 6 (09:25:03 → 09:34:23), shortened.  
**Using brainstorming skill to** attack Round 6’s “copy claim from the top” rule against `numActiveBands` + `processBlock`.

## Evaluation of Round 6

### 1. What it claimed

- Frozen `project(ProjectIn) → ProjectOut`.
- Claim unused from **23 down to 0** (today’s `claimSlotForQuality`).
- Reuse owned slot **by zip index** after sorting adjustments by frequency.
- Reuse window `0.148 * f`; cap 4; skip complementary; persist `SemanticIntent` sibling of SlotA; no APVTS intensity param.
- DSP-only tests; Processor wrapper later.

### 2. What the codebase actually does

**Keep:** DSP-only tests, no Processor in unit tests, no APVTS intensity, complementary skip, 0.148 reuse, SlotA persist pattern, `complementary` field (not description prefix).

**`numActiveBands` is audible:** `loadParameterSnapshot` only copies `i < params.numActiveBands` (`PluginProcessor.cpp:3545–3565`). `updateEQFromParameters` stores `idx + 1` from the choice param (`2117–2121`). `ParametricEQProcessor` processes `numActiveBands`. Bands 8–23 default `Enabled = false` (`607–609`); band 23 defaults to **High Shelf** type (`597–600`).

**Current semantic apply already explodes the band count:** first unused from the top is index 23 (`3015–3026`), then `desiredActiveBands = max(current, slot+1)` (`3047–3076`) → **24**. Disabled middles should be silent (enabled false) but the DSP loop still walks 24 bands. Round 6 **froze that footgun**.

Zip-by-index reuse is brittle: if occupancy is `[23, 22]` written as 10 kHz then 14 kHz, and we sort adjustments 10k, 14k, zip works; if one skip happened, indices skew and a 10 kHz shelf can snap onto a 14 kHz slot. **Nearest remaining owned index** is the honest reuse.

### 3. Keep / reshape / kill

| Piece | Verdict |
|---|---|
| `project()` POD + DSP tests | **Keep** |
| SemanticIntent persist triple | **Keep** |
| Skip complementary | **Keep** |
| Scan 23→0 as spec | **Kill** |
| Zip-by-index reuse | **Kill** |
| Assist pin / merge-TODO / live HistoryManager / processBlock rewrite | **Stay killed** |

### 4. Decision: **evolve** (small contract patch, then freeze for Round 8)

## Approaches

### A — Keep 23→0 to “not touch user bands”

User bands are 0…`numActiveBands-1` with gain. High-index claim does avoid 0–7, at the cost of 24-wide DSP. **Reject as the spec.**

### B — Claim the lowest unused index `>= numActiveBands`, else lowest unused in `0..23` that is not user-hot (recommended)

Unused: `!solo && (!enabled || abs(gain)<0.35)` and not claimed this batch and not owned by another quality.

Prefer `i >= in.numActiveBands` first (grow 8,9,…). Only then search `0..numActiveBands-1` for unused (gain-flat disabled). Never take `|gain|≥0.35 && enabled`.

`desiredActiveBands = max(in.numActiveBands, maxWrittenIndex+1)`.

Air on a default 8-band preset → bands **8 and 9**, `numActiveBands=10`. User 0–7 untouched. DSP grows by 2 not 16.

### C — Disconnect; ship scan-direction only as a one-line Processor fix without occupancy

Would still last-write-wins Air. **Reject.**

## Recommended: Round 6 contract **plus** B + nearest-slot reuse

Reuse: among `occ.slots[q]` still free this batch, pick min `|f_owned - f_adj|` if that distance `< 0.148 * f_adj`; else new claim.

This is the last design change. Round 8 synthesizes **this** and writes spec + TDD plan.

**PluginProcessor required** (wrapper, persist, `numActiveBands` param). **processBlock** not edited. **SemanticEQEngine.h** for `complementary`. **SemanticControlPanel.h** slider hydrate. **CMake** adds occupancy test files to `AIEqualizerPro_Tests` only.
