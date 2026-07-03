# RESOLVED — pluginval SIGSEGV in "Parameter thread safety" (was: seed-dependent, s8+)

> Filename says "s10" for historical reasons (first observed at strictness 10).
> Resolution: 2026-07-03, commit `eb88b2a0`. History of the diagnosis kept below.

## Status
**RESOLVED (pending one hands-on check).** Root cause identified, fixed, and verified
end-to-end on 2026-07-03. Remaining: a hands-on Ableton session (Stereo↔M/S flips during
playback with NaturalPhase + oversampling) as the final program-discipline check for a
runtime change.

## Root cause (proven, not hypothesized)
A deterministic **heap-buffer-overflow** — NOT a data race, and NOT the old
AtomicBandParams/B.4 hypothesis (explicitly disproven; parameterChanged() is flags-only):

- `preallocatedMaxSamples = jmax(samplesPerBlock * 4, 32768)`  (PluginProcessor.cpp:863)
- `msModeTransitionBuffer` preallocated at that size (32768 samples)             (:886)
- `oversampler2x/4x->initProcessing(samplesPerBlock)` → stages sized e.g. 512    (:1031)
- The msMode crossfade **Case A** (old mode Stereo/Mid/Side) passed the transition
  buffer RAW: `processStereoForPhaseMode(msModeTransitionBuffer, mode, false)`   (:2388)
- `processNaturalStereo` sized its `AudioBlock` with `getNumSamples()` == 32768  (:1859)
- → `Oversampling2TimesPolyphaseIIR::processSamplesUp` wrote 2×32768 floats into a
  2×512-float stage buffer (ASan: WRITE of 4 at 0 bytes past the region,
  juce_Oversampling.cpp:355), stomping neighbouring heap blocks.

The stomped neighbours detonate downstream — in pluginval, in the VST3 wrapper's
`ClientRemappedBuffer` teardown (the historical backtrace); in the in-process storm test,
in an AIEngine analysis vector. The "seed dependence" was never about strictness or
timing races: it was whether the fuzzer produced the NaturalPhase + oversampling +
msMode-flip combination. A single-threaded sequential flip reproduces it deterministically
(`MSTransitionOversamplingOverflowTest`).

## Fix (commit `eb88b2a0`)
`PluginProcessor.cpp:2388` now passes a **blockSamples-limited view** (`oldMsView`),
mirroring the sibling crossfades that already did this correctly (`oldModeView` :2050,
`oldOsView` :2189). Crossfaded audio is unchanged (causal IIR: the first blockSamples
output samples are identical); the fix also removes a 65k-samples-of-garbage CPU spike
per transition block.

## Verification (2026-07-03, all on the fixed build)
- `MSTransitionOversamplingOverflowTest` (ASan): deterministic overflow → **PASS**
- `ParameterStormThreadSafetyTest`: ASan overflow + TSan race → **PASS**, TSan **0 reports**
  (full survey, `abort_on_error=0`)
- `build_sanitize.sh` ASan+UBSan gate (DSP/Core/Regression/AI/Integration): **PASS**
- ctest Release 4/4: **PASS**
- pluginval 1.0.4, pinned killer seed `0x782104d`, rebuilt VST3 (binary 2026-07-03):
  **s8 SUCCESS and s10 SUCCESS** — "Parameter thread safety" completes; previously
  SIGSEGV at exactly that test with that seed.

## Reproducers (kept as regression tests, quarantined target)
`AIEqualizerPro_ThreadSafetyTests` (EXCLUDE_FROM_ALL, no ctest yet):
- `MSTransitionOversamplingOverflowTest` — deterministic single-thread repro (now green)
- `ParameterStormThreadSafetyTest` — pluginval-style concurrent param storm (now green)
Run via `build_sanitize.sh` (SAN=thread or RUN_THREADSAFETY=1).
**Follow-up:** promote both to blocking gates (add_test / default categories) now that
they are green, and revisit the CI pluginval gate (pinned-seed matrix incl. `0x782104d`,
s8+s10) that was blocked on this bug.

## Diagnosis history (audit trail)
- 2026-04-21: crash first documented at s10; "s1-8 passes" workaround recorded.
- 2026-07-02: workaround WITHDRAWN — crash reproduced at s8 with seed `0x782104d`
  (strictness-independent, seed-dependent). lldb: EXC_BAD_ACCESS in
  `ClientRemappedBuffer` teardown (downstream-corruption signature).
- 2026-07-02/03: static analysis cleared parameterChanged (flags-only) and demoted the
  B.4/AtomicBandParams and setNumActiveBands/processAICommands suspects; param-storm
  detector under TSan/ASan pinned the write (juce_Oversampling.cpp:355 from
  PluginProcessor.cpp:1862/2388); single-thread discriminant proved it deterministic;
  3-line view fix; full verification battery green.
- Bisection note (historical): bug preexisted CRIT-1 (`49f2a46f`); confirmed unrelated.

## CI / validation guidance
- pluginval can now be considered for a blocking CI gate again: pinned seeds including
  `0x782104d`, strictness 8 and 10, validating the COMPLETE bundle under
  `build-mac/Release/lib/` (the bundle under `AIEqualizerPro_artefacts/.../VST3/` is an
  incomplete stub that fails to load — do not `find | head -1`).
- Wire it only after the detector tests are promoted and one more multi-seed matrix run
  is recorded.

Last verified: 2026-07-03 (pluginval 1.0.4, macOS, worktree p0-races-capture, fix `eb88b2a0`)
