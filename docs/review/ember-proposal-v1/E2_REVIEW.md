# E2 independent review (post-hoc auditor)

Date: 2026-09-02. Worktree `/private/tmp/ember-copilot-proposal-v1` `feat/copilot-proposal-v1` @ `cc716294`.

## Commit

`bba8694b` proposal-only client/inbox/UI. Local APPLY only. No OSCParameterServer on this channel. Follow-ups: `f8b3cfb5` two-processor isolation, `ca7d4076` AIQP identity, `ff37d29f` rendezvous dir, `81320bf1` IO-thread PAIR send, `cc716294` `missing_live_identity`.

## Gate vs evidence

| Gate | Evidence | Verdict |
| --- | --- | --- |
| Stage into existing Semantic planner | `ExternalSemanticProposalInbox` + Semantic panel | PASS |
| Local APPLY only | Live retry5/7 `user_applied` notification; no wire APPLY | PASS |
| Editor closed typed rejection | `target_ui_unavailable` in protocol + lifecycle tests | PASS (unit; not re-run this audit) |
| No processBlock JSON/socket | `Source/Integration/*.cpp` has no `processBlock` | PASS |
| Two instances: B never staged | `f8b3cfb5` + Live retry5 4-Audio unpaired | PASS |
| Identity AIQP / ProposalV1 | Live scanner CID `AIAUAIQP` retry2+ | PASS |
| pluginval / auval / eight CTest gates | **Not re-run this audit** | incomplete |
| Byte-identical offline render | **Not re-run this audit** | incomplete |

## Honest residual

E2 original gate asked pluginval, auval, and the eight CTest gates green. This auditor did not re-execute those binaries. Live E4 used the installed ProposalV1 Mach-O `52a5ee6e…`. Do not treat “Live loaded” as pluginval.

`OSCParameterServer` is still allocated in `PluginProcessor.cpp`; it is pre-existing product code, not the proposal transport.
