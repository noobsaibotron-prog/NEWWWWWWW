# Ember Core — Balanced Intent Map

**Integrated branch:** `integration/ember-core-ui-p1`
**Audio-hardening base:** `38e88ae5`

This is a GUI-only projection of an already-attested Semantic plan. No FFT,
analyzer cadence, filter DSP, or audio-callback behavior is intentionally
changed. `appliedBandSlots` is filled on the **message thread** inside
`applySemanticAdjustments()`.

The map is deliberately a presentation summary, not an exhaustive scientific
report: it displays at most the two strongest focus regions and the most
relevant protection region. The 0.05 dB / 25%-of-peak focus threshold, visual
ranking, opacity and reveal timing are UI heuristics only; they do not alter the
plan, its constraints, its fitted bands, or APPLY eligibility.

## Integrated behavior

| Layer | Behavior |
|---|---|
| Truth model | `Source/GUI/SemanticIntentMap.h` projects `SemanticPlan` → `SemanticIntentMapState` |
| Axes | Conservative 1:1 only: Warmth/Clarity/Smooth/Weight/Punch + Brightness Air/Brilliance facets. No Body-from-Warmth. No Presence/Tightness proxy. Generic “brighter” lights no axis. |
| Graph | Focus/Protect **behind** spectrum. Candidate rings in READY. Applied rings use **exact** slots. |
| APPLY | Snapshot survives `pendingTextPlan` destruction. |
| NoSafeMove | Graph unchanged (no focus/protect). |
| Tabs | Intent Map presentation suppressed on AI DETECT; state is kept. |

## Remaining visual review questions

1. Are the focus/protect alphas quiet enough in Ableton?
2. Keep the response-strip `Focus:` / `Protect:` suffixes?
3. Keep candidate rings in READY, or show regions and axes only?

## Gates to run

```
AIEqualizerPro_AI_Tests --name="Semantic Intent Map"
AIEqualizerPro_IntegrationTests --name="Semantic Integration Harness"
```
