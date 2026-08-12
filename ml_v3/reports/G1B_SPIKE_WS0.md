# G1b Spike WS0 — Isolation note

**Status:** WS0 ready — seed uncommitted; not G1 PASS; not official G1b tip
**Date:** 2026-07-25

```text
Worktree:     /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1b-spike
Branch:       spike/motore-v3-g1b-frontend
Base:         a2186ac1  (G1a remediation tip; per G1B_SPIKE_PLAN § WS0)
Plan tip:     feature/motore-v3-offline @ P7-accept commit (docs-only)
Pins:         P1–P7 ACCEPTED (Marco OK 2026-07-25 on P7 restricted enum)
P7 enum:      {silence, level_below_threshold}  # spike-only; ≠ CONTRACT amend
Gate Python:  /Users/marco/aieq_data/motore_v3/env/venv  (= CPython 3.12.13)
Numpy pin:    2.5.1 (lock bit_identity.gate_platform)
```

## Seed (COPY from motore-v3-offline; left uncommitted until WS1)

- `ml_v3/frontend/__init__.py`
- `ml_v3/frontend/resampler_coeffs.py`
- `ml_v3/frontend/feature_frame.py`
- `ml_v3/tests/test_g1b_t1_resampler_coeffs.py`

## Policy

- No commits on `feature/motore-v3-offline` from this spike
- No CONTRACT / lock / SHA256SUMS / fixture-spec mutation
- Spike ≠ product G1b tip; no G1 PASS claim
- Next: WS1 — causal polyphase streaming apply (T1b) under P5

## Pin freeze (a priori)

| ID | Pin |
|----|-----|
| P1 | `Σ(w·psd)/Σw` |
| P2 | empty support → 0; Σw==0 → linear floor 1e-12; no v2 nearest-bin |
| P3 | prominence pad `reflect` |
| P4 | `shape_db` from clamped `psd_db` |
| P5 | float64 accumulate; cast float32 once at emit; L→R association |
| P6 | invalid: psd/shape/level −120; delta 0; prominence 0 |
| P7 | `silence` \| `level_below_threshold` only; hard errors out of frame |
