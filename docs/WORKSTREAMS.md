# Workstream map — 2026-08-24

One plugin trunk. Side queues do not mix. Motore v3 / ML lab are frozen for this beta.

## Trunk (work here)

| Item | Value |
|---|---|
| Branch | `feature/ember-core-unified` |
| Worktree | `.claude/worktrees/ember-core-unified` |
| Tip | `f4e36f33` — Assist G (mute until 8-frame persist window) |
| Next Assist commit | **D4 only** — honest UI/copy: do not promise Boom / Thin / Dull / Resonance as reliable live classes |
| Do not | persistence fraction, veto, retrain on the 12 Desktop WAVs, Motore v3, Semantic-as-class-detector |

G source branch `fix/assist-coldstart` (`512fd25f`) is absorbed via cherry-pick (same patch, different SHA). Leave it; delete later.

## Side queues (own PR, not into an Assist commit)

| Queue | Branch | Worktree | Rule |
|---|---|---|---|
| GUI 0 dB / decade | `fix/gui-truth-hierarchy` | `/private/tmp/ember-core-gui-truth-hierarchy` | PR after D4, or parallel if it does not touch `AIEngine` |
| Release gates | `cursor/ember-core-product-hardening` | `/private/tmp/ember-core-product-hardening` | review vs trunk; do not squash with D4 |
| Codex DSP fork | `codex/ember-core-surgical-integration` | `/private/tmp/ember-core-premium-codex` | fail-safe detector already on trunk (`7d7739d0`); remainder stays a fork |
| Host clicks | `debug/host-clicks-real` **or** `fix/click-free-audio` | Desktop repo / `charming-cohen` | keep **one**; archive the other |

Before D4: skim `unified..hardening` and `unified..codex` so a surprise merge does not land.

## Frozen (not this beta)

Motore v3 (`feature/motore-v3-*`, REV8 checkpoints, sandbox/offline/g1c).  
ML lab: prominence, A4b, recall M6–M9 (NO-GO reports).  
DynEQ / d1-exposure.  
Evidence only: `~/Desktop/AIEQ_REAL_GATE/` (briefs, occupancy, class WAVs) — not git.

Handoff text for v3 lab: `docs/EMBER_CORE_PARALLEL_HANDOFF.md` (stale vs this map; v3-only).

## Assist facts (do not reopen)

- Product Gate synthetic ≠ live DAW path (`/3`, persist 5/8, PFE).
- Live labeled set: after G, honest visible primary ≈ vocal sibilance. Mud/Boxy/Boom/Thin/Resonance are not stable live classes.
- B (loosen persist to 3/8) is a no-go. G is shipped on trunk. D1/D2 need a read-only measure before any code. D3 = separate corpus epic; these 12 WAVs stay held-out.

## Prune later (do not delete live queues)

Detached/prunable hermes audit worktree: already `git worktree prune`.  
Other `.claude/worktrees/*` and `/tmp/ember-core-*` stay until the matching queue is merged or explicitly dropped.
