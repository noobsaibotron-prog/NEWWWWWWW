# Round 8 — Synthesis: Semantic occupancy projector (intent map)

**When:** 2026-08-31 09:40:44 CEST  
**Wait:** ~6.3 min after Round 7 (09:34:23 → 09:40:44), shortened so spec/plan/MEGA fit before 10:02 CEST.  
**Using writing-plans skill to** produce spec + TDD plan (skill file **missing** on this Cloud VM; structure: goal, architecture, bite-sized TDD tasks, files, first commit).  
**Using brainstorming skill to** evaluate Round 7 and lock THE deliverable.

## Evaluation of Round 7

### 1. What it claimed

- Keep Round 6 `project()` + persist, but **kill** 23→0 claim (forces `numActiveBands=24`) and zip-index reuse.
- Claim lowest unused `>= numActiveBands`, else lowest unused that is not user-hot.
- Reuse nearest owned frequency within `0.148 * f`.

### 2. What the codebase actually does

**Confirmed:** `loadParameterSnapshot` only feeds `i < numActiveBands` into DSP (`PluginProcessor.cpp:3545–3565`). Enabled default `i < 8` (`607–609`). Current `claimSlotForQuality` walks `maxBands-1` down (`3016`) then sets `numActiveBands` to `slot+1` (`3047–3076`) → first semantic claim is band 23 → **24 active**. Round 7’s attack is not theoretical.

Nearest-owned reuse vs zip: no code today (1:1 map). Speculative API detail; Round 7’s nearest match is the safer rule. **Keep.**

### 3. Keep / reshape / kill

Round 7 is the last honest patch. **Keep as THE idea.** No disconnect.

Killed for good: Assist pin, list-to-graph glue, merge-TODO as product, complementary hitchhike, live HistoryManager, Processor-in-DSP-tests, APVTS intensity param, 23→0 scan, persist-occupancy-without-qualities, production `processBlock` edits.

### 4. THE deliverable

**Semantic occupancy projector** — a testable map from `SemanticEQEngine` primary adjustments to N APVTS bands that does not clobber manual EQ, grows `numActiveBands` from the first free slot, releases bands when a quality returns to ~0, and round-trips `SemanticState` + occupancy + intensity in plugin state.

Deliverables in this folder:

- Spec: `SPEC-semantic-occupancy.md`
- Plan: `PLAN-semantic-occupancy.md`
- User mega: `MEGA-DOCUMENT.md`

No production feature code in this round.
