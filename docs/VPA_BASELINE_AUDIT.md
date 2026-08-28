# VPA-0 Baseline Audit

Verified Plan Audition starts here. This file records the **checkout that actually exists
on this machine**, not the frozen SHA in the Cursor mega prompt. The prompt itself says
the source wins: if HEAD, branch, or tests disagree with the handoff, stop and document.
That is this document. No feature behaviour was changed to produce it.

## 1. Branch / HEAD

| Field | Handoff prompt | This checkout |
|---|---|---|
| Worktree | (shallow zip) | `/private/tmp/ember-semantic-intent-map` |
| Branch | `exp/semantic-intent-map` | `exp/semantic-intent-map` |
| HEAD | `dd79809cf4f0d00c81641bcb47d3376be8d3d55c` | `64646df154f80a695f3fbd7484c36efae650e793` |
| Audio ancestor | `38e88ae5` (stated in PDF) | `38e88ae5` is an ancestor of HEAD |

`dd79809c` is **not** an ancestor of `64646df1`. After the 28 Aug realign, the same three
Intent Map commits live on this line as `7eb796ea` / `c4d17a23` / `9edb56ad` (identical
`git patch-id` to the handoff trio). HEAD then adds docs bounds, envelope projection,
AI-panel disclosure, and the p1 integration pointer.

**Decision:** implement VPA on `64646df1`, not by resetting to `dd79809c`. Resetting would
drop envelope/disclosure work already rialigned with `integration/ember-core-ui-p1`.

Sister worktree `integration/ember-core-ui-p1` is the same SHA. DSP-only
`exp/audio-transition-hardening` remains at `38e88ae5` and is not the VPA coding line.

## 2. Clean / dirty state

Tracked tree at audit time: **clean** (after stashing unrelated persistence-evidence WIP
as `stash@{0}: wip: persistence evidence (not VPA)`).

Untracked, documented, not part of VPA:

```
?? PHASE1_INTENT_ENVELOPE_HANDOFF.md
?? PHASE2_DISCLOSURE_UI_RENDER.html
?? TestAssets/ai_corpus/res3200_pink.wav.asd
?? build-envelope/
?? build-intent/
```

`NEWWWWWWW` (`debug/host-clicks-real`) stays untouched.

## 3. Build config

Two existing Ninja trees:

| Dir | Type | Arch | Semantic | A1 | Telemetry | Copy after build |
|---|---|---|---|---|---|---|
| `build-envelope/` | **Release** | `x86_64;arm64` | ON | OFF | OFF | OFF |
| `build-intent/` | Debug | arm64 | ON | OFF | OFF | OFF |

Handoff contract is Release / Ninja / universal2 / `AIEQ_SEMANTIC_EXP=ON`. That is
`build-envelope`. Debug `build-intent` is used only as a faster test binary on this Mac;
VPA behaviour commits must still build against the Release universal config before a
listening install.

Generator: Ninja (both caches). `JUCE_COPY_PLUGIN_AFTER_BUILD=OFF`.

## 4. Baseline tests (this HEAD, Debug `build-intent`)

Handoff evidence cited 28 Intent Map assertions. This HEAD includes envelope tests.

| Suite | `--name` | Tests | Assertions | Result |
|---|---|---|---|---|
| Semantic Intent Map | `Semantic Intent Map` | 11 | 771 | PASS |
| Semantic Integration Harness | `Semantic Integration` | 8 | 29 | PASS |
| Semantic Planning T5.3 | `Semantic Planning` | 5 | 21 | PASS |
| Phase transition matrix | `Phase transition` | 24 | 480 | PASS |

Logs: `/tmp/vpa0_intent_map.log`, `/tmp/vpa0_harness.log`, `/tmp/vpa0_planning.log`,
`/tmp/vpa0_phase.log`.

Handoff 29/29 harness and 21/21 planning still match. Intent Map **771 ≠ 28** because
envelope cases landed after `dd79809c`. That is expected on this SHA, not a silent
threshold change.

## 5. PLAN → APPLY map (code, not PDF)

```
SemanticControlPanel
  PLAN  -> SemanticPlanningService worker -> SemanticPlanner -> SemanticPlan
  REVIEW -> pendingTextPlan (value copy)
  APPLY  -> semanticEngine.adjustmentsFromPlan(plan)
         -> AIEqualizerAudioProcessor::applySemanticAdjustments(
              adjustments, RequireCompletePlan)
              [message thread; off-thread callers are deferred]
         -> (quality, ordinal) keys
         -> snapshot SlotAvailability / ExistingSemanticSlot
         -> EmberSemantic::preflightSemanticSlots  (pure, no APVTS)
         -> RequireCompletePlan: any unresolved => atomicRejected, zero mutation
         -> reconcile takeover / restore obsolete owned slots
         -> claim slots, snapshot originals, write BandState, dynMode = 0
         -> maybe grow numActiveBands (APVTS 0-based choice)
         -> empty ownership => maybe restore pre-semantic band count
```

Authoritative sites:

- `Source/GUI/SemanticControlPanel.h` (`adjustmentsFromPlan`, `onTextPlanApply`)
- `Source/AI/SemanticPlan.h`, `SemanticEQEngine.cpp` (`adjustmentsFromPlan`)
- `Source/AI/SemanticSlotPreflight.h`
- `Source/PluginProcessor.cpp` `applySemanticAdjustments` (~5371)
- `Source/Tests/SemanticIntegrationHarnessTest.cpp`

`bandStatesEquivalent` (local lambda today) is the structural contract: freq 1 Hz, gain
0.05 dB, Q 0.02, plus type/enabled/solo/slope/curveMode and every Dynamic field. VPA
must reuse that contract, not invent another.

APPLY is **not** “write `plan.fit.bands` into slots 0..n”. Preflight, restore, takeover,
`dynMode` clear, and `numActiveBands` growth are part of the result. A projection that
stops at `preflightSemanticSlots` is incomplete (PA-I05).

## 6. `processBlock` map (relevant to later VPA-1.x, not VPA-1.0)

VPA-1.0 must not edit this function. Recorded so later phases insert at the real sites.

```
processBlock
  bus view / processorReady guard
  loadParameterSnapshot
  pendingReset (oversamplers, LP, wet-pad)
  bypass state machine (can skip DSP; still feeds wet-pad with dry)
  parameter smoothing
  PRE analyzer
  A/B whole-chain snapshot flag -> ParametricEQProcessor::beginWholeChainCrossfade
      (eq / HQ / Mid / Side only — not DynEQ, Auto Gain, Linear convolver)
  if needsParamUpdate -> updateEQFromParameters   // APVTS -> targets + eqProcessorForIR
  applySmoothedBandParams(block, needsParamUpdate)
      uses committed numActiveBands; does NOT update eqProcessorForIR
  ZL / Natural / Linear routing + M/S + DynEQ + phase-transition dual wet-pad
  solo acoustic monitor
  POST analyzer
  Auto Gain (RMS pre/post, not BS.1770)
  Dynamic Correction Engine (opt-in, default off)
  wet latency pad (skip second pad during aligned phase transition)
  dry/wet mix (after pad)
  output gain
  bypass crossfade
  safety limiter
```

Critical facts for later overlay (not implemented in VPA-1.0):

- Preview must be consumed **outside** `if (needsParamUpdate)`.
- `applySmoothedBandParams` reads `numActiveBands`; preview of slot 9 with committed 8
  needs `effectiveActiveBandCount`.
- `updateEQFromParameters` feeds `eqProcessorForIR` from APVTS. Linear preview B cannot
  use that path while APVTS stays A.
- `beginWholeChainCrossfade` is ParametricEQ-local, not whole-plugin A/B.

## 7. Thread ownership (objects VPA will touch)

| Object | Owner | Notes |
|---|---|---|
| `applySemanticAdjustments` / future projection | Message thread | Off-thread → `callAsync` defer |
| `semanticBandOwned` / assignments / snapshots | Message thread | PA-I01: preview must not write |
| APVTS / `numActiveBands` param | Message / host | Preview must not write |
| `HistoryManager` | Message thread | Preview must not push undo |
| `processBlock` / EQ processors / DynEQ / LP | Audio thread | VPA-1.0 does not enter |
| `LatestValueMailbox<T,4>` | Producer non-RT, consumer audio | API: `publish` / `acquireLatest` / `release`; `SlotCount >= 3`; trivially copyable payload; **per instance** |
| `eqProcessorForIR` | Message/audio via `updateEQFromParameters` | Not updated by `applySmoothedBandParams` |

## 8. Stateful components that can break Preview-B == Apply-B

Later phases, not VPA-1.0. Listed so they are not “forgotten Auto Gain”:

- ParametricEQ delay lines / topology fade (1024 samples)
- DynEQ envelopes + `smoothedDynEqMix`
- Linear Phase IR + partition state + dual wet-pad
- Oversamplers
- Auto Gain RMS + `smoothedAutoGain`
- Dynamic Correction Engine
- Dry/wet + output-gain smoothers
- Bypass crossfade + dry delay
- M/S mode crossfade
- Phase-transition padding rings

Capability-gate (PA-I12) is the honest fallback when a path is not proven. Silent disable
is forbidden.

## 9. CaptureService

`AIEqualizerAudioProcessor::kCaptureEnabledForShipping == false`
(`Source/PluginProcessor.h:481`). `processBlock` returns that flag at ~5051.
FA-001 / `AICaptureDisabledWitnessTest` remain the seal. VPA-1 must not re-enable
shipping capture. VPA-2 same-sample, if ever, needs a new bounded path.

## 10. A/B slots (rejected as preview backend)

`slotA..D` / `setABState()` load APVTS. Using them for Semantic audition would mutate the
project. Crossfade *technique* on ParametricEQ may be reused later inside its real
contract. Not in VPA-1.0.

## 11. What VPA-1.0 is allowed to change

Extract `buildSemanticApplyProjection()` from `applySemanticAdjustments()` so APPLY
becomes `projection → commit`. Shipping APPLY behaviour and `processBlock` stay
observably the same. Tests must cover the ten scenarios in the mega prompt and compare
**all** `BandState` fields via `bandStatesEquivalent`.

If that gate is red, stop. Do not open `processBlock`.
