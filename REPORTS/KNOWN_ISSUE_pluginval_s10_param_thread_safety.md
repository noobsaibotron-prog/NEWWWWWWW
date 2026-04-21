# Known Issue — pluginval s10 SIGSEGV in "Parameter thread safety"

## Status
**Open, not blocking merge of CRIT-1 (commit 49f2a46f).**
Preexisting bug, independent of CRIT-1 fix.

## Symptom
pluginval 1.0.4 at `--strictness-level 10` crashes with SIGSEGV (signal 11)
immediately after "Background thread state" test completes, when entering
"Parameter thread safety" test.

Reproducible: yes, deterministic point of failure.
Crash report: not generated (pluginval catches signal internally).
Stack trace: not available without lldb attach.

## Bisection
- Commit `49f2a46f` (CRIT-1, SPSCQueue fix): crashes
- Commit `8247917c` (parent of CRIT-1): crashes identically
- Same test, same line count (3052), same failure point
- Conclusion: bug preexists CRIT-1; fix is innocent

## User impact
**None observed in normal DAW use.**
pluginval s10 "Parameter thread safety" stresses parameter updates from
concurrent threads at fuzzer-level throughput, a workload no commercial
host produces. Plugin runs correctly in Ableton Live for weeks in production
without observed crashes.

## Likely root cause (hypothesis, not verified)
Candidate findings from Audit B (ChatGPT Codex):
- B.4: torn-read on AtomicBandParams — dirty-flag pattern may not prevent
  reader from seeing inconsistent state during publish
- Disaggregated HIGH findings: addBand, clearBandFilterState, wholeChainXfade
  as distinct races from CRIT-1

## Blocking action
To close this issue, next steps required:
1. lldb backtrace of the SIGSEGV to identify exact faulting frame
2. Static analysis of message-thread → audio-thread parameter path post-CRIT-1
3. Targeted fix(es) with dedicated UnitTest reproducing the bug
4. Re-run pluginval s10 to full completion as regression gate

## Workaround for CI/validation
Until fixed: pluginval at strictness levels 1-8 passes.
For regression testing, use `--strictness-level 8` as interim gate.

Last verified: 2026-04-21
