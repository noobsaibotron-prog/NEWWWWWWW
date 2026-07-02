# Known Issue — pluginval SIGSEGV in "Parameter thread safety" (seed-dependent, reproducible at s8)

> Filename says "s10" for historical reasons (first observed at strictness 10).
> The 2026-07-02 measurement shows the crash is NOT strictness-gated — it is
> RANDOM-SEED-gated, and fires at strictness 8 with the seed below.

## Status
**Open. Preexisting bug** (bisected below CRIT-1); not blocking merge of unrelated work.
**2026-07-02 update — severity raised, workaround withdrawn:** the former claim
"strictness levels 1-8 pass" was a lucky-seed artifact. With a pinned seed the crash
reproduces deterministically at strictness 8.

## Symptom
pluginval 1.0.4 crashes with SIGSEGV (signal 11) at the start of the
"Parameter thread safety" test, immediately after "Background thread state" completes.

Manifestation is seed-dependent, not strictness-dependent:
- seed `0x782104d`, strictness 8 → deterministic SIGSEGV (pluginval exit code 9)
- other seeds (e.g. `0x213600`) and typical unseeded runs → full PASS at strictness 8
- a hang variant (multi-minute stall inside the same test, pluginval's `--timeout-ms`
  not firing) was observed once with the crashing seed on a WIP build
  (recover-ember-core worktree) — same trigger, different manifestation.

## Reproducer (verified 2026-07-02 — worktree p0-races-capture, Release build of 2026-07-01)
```
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 8 --timeout-ms 90000 --random-seed 0x782104d \
  --validate "build-mac/Release/lib/AI Equalizer Pro.vst3"
# → "pluginval received Segmentation fault: 11, exiting immediately"; exit code 9
```

PATH WARNING — validate the COMPLETE bundle under `build-mac/Release/lib/`.
The bundle under `build-mac/AIEqualizerPro_artefacts/Release/VST3/` is an incomplete
stub (no `Contents/MacOS/` binary): pluginval fails it with "Unable to load VST-3
plug-in file", which looks like a plugin failure but is a path-selection error.
`find build-mac -name "*.vst3" | head -1` can pick the stub — do not use it.

## lldb backtrace (2026-07-02, read-only attach on the reproducer)
```
thread #7, EXC_BAD_ACCESS
AI Equalizer Pro`juce::ClientRemappedBuffer<float>::~ClientRemappedBuffer()
AI Equalizer Pro`juce::JuceVST3Component::processAudio<float>()
AI Equalizer Pro`juce::JuceVST3Component::process()
pluginval`juce::VST3PluginInstance::processBlock()
pluginval`ParameterThreadSafetyTest::runTest()
```
Reading: the faulting frame is JUCE's VST3-wrapper channel-remap teardown on the
audio-render thread (thread #7) — the point where the remapped buffer copies back to
host buffers. That is a DOWNSTREAM-corruption signature (a dangling/stomped pointer
produced earlier in the same `process()` call), not the root cause itself. It does
NOT (yet) pin AtomicBandParams B.4 specifically.

## Bisection (unchanged)
- Commit `49f2a46f` (CRIT-1, SPSCQueue fix): crashes
- Commit `8247917c` (parent of CRIT-1): crashes identically
- Same test, same failure point
- Conclusion: bug preexists CRIT-1; fix is innocent

## User impact
None observed in normal DAW use (plugin runs in Ableton Live for weeks without
crashes). But the trigger threshold is lower than previously believed: strictness-8
parameter fuzzing with an unlucky seed is enough — no s10-only exotic workload needed.

## Likely root cause (hypotheses — still not verified)
The crash frame is consistent with heap/pointer corruption produced earlier in the
same process() call, e.g. parameter-callback code mutating DSP state while
processBlock runs. Candidates from Audit B (ChatGPT Codex):
- B.4: torn publish of AtomicBandParams (dirty-flag pattern may let the audio reader
  see inconsistent state during publish)
- addBand / clearBandFilterState / wholeChainXfade races (disaggregated HIGH findings)
The lldb frame alone cannot distinguish these; that requires the static-analysis pass
below.

## Blocking action (updated 2026-07-02)
1. ~~lldb backtrace of the SIGSEGV~~ **DONE** — see above. Frame = wrapper teardown;
   downstream-corruption signature; root cause still open.
2. Static analysis of the message/background-thread → audio-thread parameter path
   (parameterChanged → band/DSP mutation vs processBlock) to identify the corrupting
   write.
3. Targeted fix(es) with a dedicated UnitTest reproducing the race.
4. Re-run pluginval to full completion with a PINNED-SEED matrix that includes
   `0x782104d`, at strictness 8 and 10, as the regression gate.

## CI / validation guidance (SUPERSEDES the old workaround)
- WITHDRAWN: "use `--strictness-level 8` as interim gate". s8 crashes with seed
  `0x782104d`; an unseeded s8 gate is a coin flip (flaky red, or false green that
  hides the bug).
- Do NOT wire pluginval as a blocking CI gate until the race is fixed.
- Local/manual runs: always pass an explicit `--random-seed` (reproducibility) and
  point at the complete bundle under `build-mac/Release/lib/`.
- After the fix: promote pluginval to a blocking gate with pinned seeds including
  `0x782104d` (s8 + s10), per CODEBASE_EVALUATION_2026 recommendation #3.

Last verified: 2026-07-02 (pluginval 1.0.4, macOS, worktree p0-races-capture)
