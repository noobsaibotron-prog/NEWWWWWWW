# G1b Spike WS4 Evidence — Gate 4 SR-parity + proof (b)

**Status:** SPIKE EVIDENCE — ≠ G1 PASS — ≠ G1b tip — ≠ CONTRACT amend
**Overall gate-4 verdict:** `RED`
**Overall max|Δ|:** 9.537365 dB (threshold 0.25 dB; GREEN margin floor 0.05 dB)

## Platform

- `os`: `darwin`
- `arch`: `arm64`
- `python`: `CPython 3.12.13`
- `numpy`: `2.5.1`
- `interpreter`: `/Users/marco/aieq_data/motore_v3/env/venv/bin/python`

## Useful window (warm-up ∩ coda)

- `[0.25745124716553286, 1.9146666666666667]` s

## Per-cell results

| asset | sr | mode | max\|Δ\| dB | margin | verdict | peak field/band | P2 watch |
|-------|----|------|------------|--------|---------|-----------------|----------|
| multitone | 44100 | full | 4.779564 | -4.529564 | **RED** | mid_shape_db[35]@152.5Hz | HIT |
| multitone | 96000 | full | 4.487526 | -4.237526 | **RED** | mid_shape_db[35]@152.5Hz | HIT |
| pseudo_noise | 44100 | full | 9.537365 | -9.287365 | **RED** | mid_shape_db[18]@56.9Hz | HIT |
| pseudo_noise | 96000 | full | 8.993477 | -8.743477 | **RED** | mid_shape_db[18]@56.9Hz | HIT |
| log_sweep | 44100 | sweep_checkpoints | 6.435013 | -6.185013 | **RED** | mid_psd_db[106]@9403.7Hz | — |
| log_sweep | 96000 | sweep_checkpoints | 5.838142 | -5.588142 | **RED** | mid_psd_db[106]@9403.7Hz | — |

## Peak localization detail

- **multitone@44100**: `mid_shape_db` band=35 hz=152.5397171804689 |Δ|=4.779564 psd_ref=-119.29441833496094 psd_sr=-114.52755737304688 t_ref=1.664000 t_sr=1.662549 meta={'t': 1.664} p2_main_sb=True p2_lf_sb=False
  - note: P2 watch-list HIT: max|Δ| on MAIN single-bin band 35
- **multitone@96000**: `mid_shape_db` band=35 hz=152.5397171804689 |Δ|=4.487526 psd_ref=-119.29441833496094 psd_sr=-114.81859588623047 t_ref=1.664000 t_sr=1.662667 meta={'t': 1.664} p2_main_sb=True p2_lf_sb=False
  - note: P2 watch-list HIT: max|Δ| on MAIN single-bin band 35
- **pseudo_noise@44100**: `mid_shape_db` band=18 hz=56.860609186053274 |Δ|=9.537365 psd_ref=-119.03687286376953 psd_sr=-109.50019073486328 t_ref=0.533333 t_sr=0.531882 meta={'t': 0.5333333333333333} p2_main_sb=True p2_lf_sb=True
  - note: P2 watch-list HIT: max|Δ| on MAIN+LF single-bin band 18
- **pseudo_noise@96000**: `mid_shape_db` band=18 hz=56.860609186053274 |Δ|=8.993477 psd_ref=-119.03687286376953 psd_sr=-110.04402923583984 t_ref=0.533333 t_sr=0.532000 meta={'t': 0.5333333333333333} p2_main_sb=True p2_lf_sb=True
  - note: P2 watch-list HIT: max|Δ| on MAIN+LF single-bin band 18
- **log_sweep@44100**: `mid_psd_db` band=106 hz=9403.702978599711 |Δ|=6.435013 psd_ref=-120.0 psd_sr=-113.56498718261719 t_ref=1.706667 t_sr=1.705215 meta={'checkpoint_hz': 16000, 't_cross': 1.7096208279133098} p2_main_sb=False p2_lf_sb=False
- **log_sweep@96000**: `mid_psd_db` band=106 hz=9403.702978599711 |Δ|=5.838142 psd_ref=-120.0 psd_sr=-114.16185760498047 t_ref=1.706667 t_sr=1.705333 meta={'checkpoint_hz': 16000, 't_cross': 1.7096208279133098} p2_main_sb=False p2_lf_sb=False

## P2 support geometry (preregistered watch)

- MAIN empty=14 single-bin=21
- LF empty=6 single-bin=17
- MAIN single-bin indices: `[2, 3, 9, 10, 14, 15, 18, 19, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 32, 35, 36]`
- LF single-bin indices: `[2, 3, 6, 7, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23]`

## Pin necessity check

- changed_to_pass: `False`
- P1–P7 remain the a-priori pins from G1B_SPIKE_PLAN.md; this run did not rewrite pins to chase a pass. RED under pinned P* is a REV7 candidate, not silent pin mutation.

## Streaming schedules (spike)

- Spike schedules: `[1, 8193, 'geometric-32']`
- Deferred full fixed set: `[1, 63, 1024, 4095, 8192, 8193]`
- Full product matrix {1,63,1024,4095,8192,8193}+geometric deferred to official G1b. Spike covers {1,8193,geometric-32}. schedule=1 on full-length 44.1 kHz T6 assets remains harness-heavy (long FIR); covered on short emit slices + 48k/96k full paths in tests.

## Proof (b) interleaved silence

- Status: **COVERED_BY_TESTS**
- ml_v3/tests/test_g1b_t2_streaming_features.py::InterleavedSilenceProofBTests — asset||silence||asset offline≡chunk on spike schedules {1,8193,geometric-32}; silence naturally clears delta history (also_required b). Proof (a) explicit reset remains in MultiAssetDeltaResetTests.

## Diagnostic (non-gating)

Tone-peak / sweep-peak bands are far tighter than the full §13.2 domain max|Δ|. Failures concentrate on near-floor active cells (activity predicate `max(psd_ref,psd_sr) > -120`), P2 single-bin MAIN bands, and sweep skirt bands away from the instantaneous peak. Prominence left **unclamped**. No threshold relaxation applied.

## REV7 candidacy (gate-4 (a) = RED)

Stop-rule checklist (plan §8):

1. Implementation is §5/§6/§7 under a-priori P1–P7 (no pin rewrite; prominence unclamped).
2. Harness matches lock: common useful window, nearest `source_time`, activity union `max> -120`, invalid-channel ignore, max aggregator, sweep checkpoints, gate platform.
3. Timeline grid matches fixture-spec `_useful_frame_times` (max |t| error = 0).
4. Still: every adversarial cell `max|Δ| ≫ 0.25` (worst **9.537 dB** on pseudo_noise@44100 `mid_shape_db[18]`).
5. Localization: peaks sit on near-floor active PSD cells and/or P2 single-bin bands; tone/sweep *peak* bands are ≪ 0.25 while the contractual full domain is not.

**Interpretation:** under pinned P* and faithful emit, REV6 gate-4 as packaged appears unreachable on the adversarial subset → **REV7 mandate candidate**. Do **not** silently raise the activity floor, clamp prominence, switch aggregator, or drop shape/prominence from the domain in this spike. Guardian decides REV7 GO/NO-GO.

## Notes

- Spike adversarial subset only; transient/damped onset out of scope.
- Full 7-schedule streaming product matrix deferred to official G1b.
- Prominence unclamped (P3 reflect); no post-hoc prominence clamp.
- Margin floor for GREEN packaging: 0.05 dB (threshold 0.25 dB).

## Handoff

- `ember-contract-guardian`: counter-check tip; REV7 only if RED write-up accepted
- `ember-parity-lab`: re-verify digests / max|Δ| / margin tables on gate venv
- `ember-metrology-redteam`: attack false-PASS (window, activity, invalid ignore, platform)

≠ G1 PASS. ≠ Ableton readiness. CONTRACT @ freeze unchanged.
