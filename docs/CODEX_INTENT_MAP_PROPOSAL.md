# Ember Core — Balanced Intent Map (proposal branch)

**Branch:** `exp/semantic-intent-map`  
**Base:** `43129bdd` (`exp/audio-transition-hardening`, includes bypass-exit coefficient fix)  
**Do not merge** onto the audio-hardening line until that line’s transition gates are accepted.

This is a GUI-only proposal for Codex to accept, cherry-pick, or reject. No FFT, analyzer cadence, filter DSP, or audio-callback behavior is intentionally changed. `appliedBandSlots` is filled on the **message thread** inside `applySemanticAdjustments()`.

## What shipped on this branch

| Layer | Behavior |
|---|---|
| Truth model | `Source/GUI/SemanticIntentMap.h` projects `SemanticPlan` → `SemanticIntentMapState` |
| Axes | Conservative 1:1 only: Warmth/Clarity/Smooth/Weight/Punch + Brightness Air/Brilliance facets. No Body-from-Warmth. No Presence/Tightness proxy. Generic “brighter” lights no axis. |
| Graph | Focus/Protect **behind** spectrum. Candidate rings in READY. Applied rings use **exact** slots. |
| APPLY | Snapshot survives `pendingTextPlan` destruction. |
| NoSafeMove | Graph unchanged (no focus/protect). |
| Tabs | Intent Map presentation suppressed on AI DETECT; state is kept. |

## Review questions

1. Header-only ~400-line projection vs `.cpp`?
2. `SemanticApplyResult::appliedBandSlots` vs a separate query?
3. Suppress on AI DETECT: keep?
4. Alphas quiet enough in Ableton?
5. Response-strip `Focus:` / `Protect:` suffixes vs UI-A copy freeze?
6. Candidate rings in v1, or regions+axes only?

## Gates to run

```
AIEqualizerPro_AI_Tests --name="Semantic Intent Map"
AIEqualizerPro_IntegrationTests --name="Semantic integration"
```
