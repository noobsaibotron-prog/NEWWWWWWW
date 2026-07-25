# G1b Frontend Spike Plan — REV6 feasibility (0.25 dB + streaming≡offline)

**Status:** PLAN ONLY — not G1b tip, not gate proof, not G1 PASS  
**Date:** 2026-07-25 (refined: Codex/Claude P1–P7 pin review — P1–P6 ready; P7 awaiting Marco)  
**Contract:** `docs/MOTORE_V3_G1_CONTRACT.md` @ freeze `6d254d0a` (REV6)  
**Lab state:** `docs/MOTORE_V3_PLAN.md` — G1a CLOSE: GO @ tip `a2186ac1`; product G1b may unfreeze; **REV7: NO** until falsifiable impossibility  
**Authority for this doc:** planning spike under Marco mandate; does not amend CONTRACT  
**Pin readiness:** **P1–P6 ready** (Codex ACCEPTED / refined); **P7 awaiting Marco**; **worktree create blocked until P7 OK**

```text
PHASE:            G1b-SPIKE-PLAN (planning + inventory; not product G1b)
AUTHORIZED_BY:    PLAN G1a CLOSE stamp + G1b may-unfreeze / mandato storico
                  PLAN tip lineage (incl. 37f6ac60 mandato); Marco: "proceed
                  with planning the G1b spike" + refinements mandate
ALLOWED_PATHS:    ml_v3/reports/G1B_SPIKE_PLAN.md (this file)
                  (inventory read-only: ml_v3/frontend/**, test_g1b_*,
                   contracts/, fixtures/g1/, MOTORE_V3_*)
FORBIDDEN_PATHS:  Source/, CMakeLists.txt, Resources/, ml_v2/, AIEQ-mac/,
                  CONTRACT amend, training, ship weights, Ableton install,
                  hashed artifact mutation (lock / SHA256SUMS / fixture-spec)
```

---

## 1. Goal / non-goals

### Goal (the only question that matters before freezing more)

Under **REV6 unchanged**, answer with evidence on the gate platform:

> Are **(a)** sample-rate parity `max|Δ| ≤ 0.25 dB` (44.1 / 48 / 96) and
> **(b)** offline ≡ streaming **bit-identity** on required float32 frame fields
> reachable by a faithful §5/§6/§7 implementation?

Verdict taxonomy (spike overall uses the pair below; see §4):

| Verdict | Meaning |
|---------|---------|
| **GREEN** | Gates reachable under REV6 with pinned P1–P7 and margin |
| **AMBRA** | Gates appear to pass, but only via unpinned P* choice and/or without margin → freeze-from-prose preregistration debt (≠ REV7) |
| **RED** | Falsifiable impossibility under REV6 → REV7 mandate candidate |
| **INCONCLUSIVE** | Incomplete surface / wrong platform / harness bug; no CONTRACT amend |

### Non-goals (hard)

| Non-goal | Why |
|----------|-----|
| G1 PASS / G1e close | Spike ≠ full gate suite; G1e is later |
| Ableton / ship-line | `Source/`, CMake, Resources, AIEQ-mac stay 0-diff |
| Training / thresholds / routing | Not authorized |
| CONTRACT amend (REV7) | Only after RED with falsifiable impossibility |
| Treating current `ml_v3/frontend/` spike as proof | Explicit: **spike ≠ tip ≠ PASS** |
| Closing gain/M/S/anti-alias/split/evaluator gates | Out of spike scope (note only) |
| F1 WAV relocation | Durable debt; use in-repo T6 WAVs as-is |
| Full 7 streaming schedules in spike | Adversarial subset only (§3.1); full set = official G1b |
| Creating spike worktree in this plan session | Documented in WS0; execute only after Marco OK on **P7** (P1–P6 ready) |
| Mutating hashed G1a artifacts | lock / SHA256SUMS / fixture-spec / WAVs stay frozen |

---

## 2. Inventory of existing uncommitted spike (not proof)

Present on worktree `motore-v3-offline` (uncommitted; **not** official G1b tip):

| Path | What it is | What it is not |
|------|------------|----------------|
| `ml_v3/frontend/resampler_coeffs.py` | §5 FIR coeff generator; ratios/delays match lock | No streaming polyphase apply; no audio I/O |
| `ml_v3/frontend/feature_frame.py` | §7 schema stub + fail-closed structural validate | No FFT, no features, no emit path |
| `ml_v3/frontend/__init__.py` | Re-exports T1 surface | — |
| `ml_v3/tests/test_g1b_t1_resampler_coeffs.py` | Coeff length/sum/determinism + stub rejects | No gate 3 / gate 4 measurements |

**Explicit:** this spike proves only that frozen §5 *parameterization* can be
materialized as coefficients consistent with the metrology lock. It does
**not** demonstrate 0.25 dB SR-parity or streaming≡offline bit-identity.

---

## 3. Contract / lock anchors the spike must obey (REV6)

### 3.1 Gates under test (spike success = feasibility of these two)

From CONTRACT §13.2 + lock `metrology_lock.json`
(`metrology_lock_sha256` =
`d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`):

**(a) Sample-rate parity (gate 4)** — spike adversarial subset  
- Compare **44.1 and 96 vs ref 48 kHz** on preregistered useful window (§13.1).  
- **Signals (primary):** `multitone`, `log_sweep`, `pseudo_noise` only.  
- Domain: `mid/side_{psd,shape,prominence}_db[120]` + `mid/side_level_dbfs`
  when channel valid; **exclude** `*_delta_db`.  
- Aggregator: **max** |Δ|; threshold **0.25 dB**; one active cell over → FAIL.  
- Activity: `max(psd_ref, psd_sr) > -120` (union); no post-hoc mask.  
- Align on nearest `source_time` (not raw index); no manual frame shift.  
- Multitone / noise: mode **(a)** entire useful segment (mode b diagnostic only).  
- Sweep: frozen checkpoints only
  `[20,45,60,80,250,1000,3500,8000,16000,20000]` Hz.  
- Transient / damped resonance / onset sub-gate: **out of primary spike scope**
  (risk declared in §10; not used to claim GREEN).

**(b) Streaming ≡ offline (gate 3)** — spike adversarial schedules  
- **Spike schedules only:** `{1, 8193, geometric-32}`  
  (geometric list from `PCG64(20260719)` via lock — do not regenerate U).  
- Full lock set `{1,63,1024,4095,8192,8193}` + geometric **deferred** to
  official product G1b gate (not required to close this spike).  
- On **gate platform**: byte-identical float32 frame fields + rational
  timestamps + validity flags (`gate_platform_float_tol = 0`).  
- Also-required smoke (optional in spike, not GREEN-blocking alone):
  multi-asset concat with delta history reset; interleaved silence.  
- Secondary 1e-6 allowlist: **empty** → cannot close G1 via tol.

### 3.2 Implementation surface required to even ask the question

| Piece | Contract |
|-------|----------|
| Resampler | §5 causal polyphase FIR; state across blocks; no pad/reflect/look-ahead |
| Timing | §13.1 warm-up additive: `delay + N_LF/fs_c + K_wu*H/fs_c`; coda `K_coda*H/fs_c`; `K_wu=K_coda=4` |
| Dual-res | §6.2 MAIN 4096 / LF 8192 / hop 1024 / Hann / same `frame_end_sample` / LF↔MAIN fuse 160–320 Hz |
| Frame | §7 full emit (not stub-only) |
| Inputs | T6 bytes via `render_*` float32 arrays (byte-identical to WAV); no scipy/soundfile loader |

### 3.3 Evidence platform (mandatory)

```text
Interpreter: /Users/marco/aieq_data/motore_v3/env/venv/bin/python
CPython:     3.12.13
Lock pin:    bit_identity.gate_platform
             {os:darwin, arch:arm64, python:CPython 3.12.13, numpy:2.5.1}
Deps:        numpy + stdlib only — no scipy, no soundfile
             do not touch requirements.lock
```

Any GREEN/RED/AMBRA claim recorded on another interpreter → **INCONCLUSIVE**.

---

## 4. Success criteria (spike verdict)

Record in a future evidence note (not this plan) as one of:

### (a) SR-parity 0.25 dB

| Verdict | Rule |
|---------|------|
| **GREEN** | On gate platform, for adversarial subset (multitone, pseudo_noise, log_sweep @ 44.1/48/96), after warm-up∩coda intersection and activity predicate, measured `max|Δ| ≤ 0.25` on the full §13.2 domain **with margin** (see AMBRA); sweep checkpoints all present; P1–P7 pinned *before* coding and not chosen to pass; no threshold relaxation |
| **AMBRA** | Same surface appears to pass, but (i) pass depends on an unpinned or post-hoc P1–P7 choice, **or** (ii) `max|Δ|` has no margin (e.g. 0.24 dB on a gate-closing cell). AMBRA ≠ REV7; produces freeze-from-prose preregistration debt for official G1b |
| **RED** | Faithful §5/§6/§7 path + correct alignment/window still yields `max|Δ| > 0.25` on a gate-closing fixture/domain cell, **and** root cause is contractual (formula/threshold/domain), not a bug — documented with numeric evidence + localization (band, field, asset, SR) |
| **INCONCLUSIVE** | Missing emit path, wrong window, wrong platform, empty active set mishandled, or bug suspected but not isolated |

### (b) Streaming ≡ offline bit-identity

| Verdict | Rule |
|---------|------|
| **GREEN** | For every **spike** schedule (`1`, `8193`, geometric-32), on gate platform, offline monolith vs chunked produce **byte-identical** required float32 fields + identical rational timestamps + validity/reason; P1–P7 pinned a priori |
| **AMBRA** | Bit-identity holds only under an unpinned P* (esp. P5 cast/reduction) or after post-hoc association change |
| **RED** | Same algorithm offline vs streaming diverges on gate platform after state/reset bugs ruled out — i.e. contract forces non-associative float path with no streaming-equivalent formulation |
| **INCONCLUSIVE** | Harness compares wrong fields, resets delta incorrectly, or platform ≠ lock |

**Spike overall:**

- Continue product G1b @ REV6 iff **(a)=GREEN and (b)=GREEN**.  
- **AMBRA** → continue engineering only after writing freeze-from-prose
  preregistration debt (P* pins) into the next G1b tranche docs; **do not**
  treat as GREEN; **do not** open REV7.  
- Propose **REV7** only if **(a)=RED or (b)=RED** with falsifiable impossibility write-up.  
- Any INCONCLUSIVE → stop product freeze; fix spike; no CONTRACT change.

**Rule (P* necessity):** if any P1–P7 choice is *necessary* to pass → verdict
**AMBRA**, never GREEN.

---

## 5. Pin-before-coding (P1–P7) — mandatory before WS1+

Unpinned §6/§7 choices that **silently change measured quantities**.  
**Choices MUST be written here (or amended by Marco) before any product/spike
coding run.** Changing a pin after seeing numbers → at best AMBRA.

**Meta (2026-07-25 Codex/Claude independent review):** **P1–P6 ready**;
**P7 awaiting Marco**; **worktree create blocked until P7 OK**.
No CONTRACT amend invented here.

| ID | Status | Ambiguity | Contract cite | Default (conservative) | Needs Marco? |
|----|--------|-----------|---------------|------------------------|--------------|
| **P1** | **ACCEPTED** (Codex) | Band energy weighted-mean denominator: `Σ(w·psd)/Σw` vs `Σ(w·psd)/N_bins` | §6.1 «media pesata lineare della PSD» | **`Σ(w·psd)/Σw`** (true weighted mean). Empty `Σw` → P2 | No |
| **P2** | **ACCEPTED** + evidence obligations (Codex refine) | Empty triangular support on MAIN 4096 (no FFT bin weight) | §6.1 bande triangolari; DC/>20 kHz non contribuiscono; **no** nearest-bin fallback in REV6 | See **P2 detail** below | No (fail-closed; forbid v2 fallback) |
| **P3** | **ACCEPTED** (Codex) | Prominence padding: numpy `reflect` vs `symmetric` | §7 «padding reflect» | **`reflect`** (as written). Not `symmetric`, not `edge`, not wrap | No |
| **P4** | **ACCEPTED** (Codex) | `shape_db` from clamped vs pre-clamp `psd_db` | §6.1 floor→clamp then dB; §7 `shape_db` uses `psd_db` | **Clamped `psd_db`** (the emitted field): `shape = psd_db - 10*log10(sum(10**(psd_db/10)))` on 120 bands | No |
| **P5** | **ACCEPTED** (Codex) | `float64→float32` cast point + reduction association | §7 frame float32; gate3 bit-identity; lock `gate_platform_float_tol=0` | **Accumulate FIR/FFT/band sums in float64; cast each emitted frame float field to float32 once at write.** Same association offline and streaming (left-to-right on frozen index order). No Kahan / blocked reassoc without new pin | No (default is spike baseline) |
| **P6** | **ACCEPTED** (Codex correct; was inconsistent) | Floor / zero values for invalid-channel vectors | §7 floor for invalid channel; PSD clamp `[-120,+12]`; `delta_db` clamp `[-24,+24]`; first-valid / history-reset → zeros | See **P6 detail** below | No |
| **P7** | **PENDING Marco** | `reason` enumeration when `valid=false` | §7 «motivo enumerato quando falso» — **enum not listed** on contract surface; stub accepts any non-empty `str` | **Provisional spike-only set** (not CONTRACT amend): `silence`, `non_finite_input`, `unsupported_sr`, `insufficient_samples`, `channel_invalid` — used consistently offline≡streaming. Recommended: accept as provisional + freeze-from-prose debt for official enum at G1b tip | **YES — accept provisional set or supply canonical enum** |

### P2 detail (fail-closed default + quantified evidence obligations)

**Keep (fail-closed):**
- **No** silent import of v2 nearest-bin.
- Empty triangular support → contribute **0** to the weighted sum.
- If `Σw==0` for a band after fuse inputs → band energy = linear floor
  `1e-12` before dB/clamp.

**Evidence obligations (verified geometry notes; declare in spike evidence):**
- Analysis always at `fs_c=48k` → band geometry identical across source rates →
  P2 choice largely cancels in cross-rate Δ (reduces “measuring the choice”
  risk for gate 4).
- MAIN 4096: ~14 empty-support bands (indices scattered 0–23, centres
  ~20–76 Hz) + ~21 single-bin bands.
- LF 8192: ~6 empty bands (~20–37.9 Hz) + ~17 single-bin.
- ~6 LF-pure bands below 160 Hz can be structurally dead (always floor /
  inactive) — declare honestly in evidence; does **not** alone break gate.
- **Preregister watch-list:** single-bin bands are primary suspects if
  `max|Δ|` exceeds 0.25 dB — report whether the max lands there.

### P6 detail (corrected; prior −120 on `delta_db` was INVALID)

Prior draft set `*_delta_db = -120` for invalid channel — **INVALID**: violates
§7 `delta_db` clamp `[-24, +24]`.

**NEW default (invalid channel):**
- `*_psd_db` / `*_shape_db` → **-120.0** (PSD/level floor; matches §6.1 clamp lower)
- `*_level_dbfs` → **-120.0**
- `*_delta_db[120]` → **0.0** (natural zero; matches §7 first-valid-frame /
  history-reset zeros)
- `*_prominence_db` → prefer **0.0** (residual centered at 0), not −120 —
  note lightly if prominence also gains a declared numeric range later;
  REV6 surface does not list a separate prominence clamp beyond derivation
  from shape
- `mid_valid` / `side_valid` / `valid` false as §7

### P* workflow

1. Marco OK on **P7** (and any cell amend) **before** worktree coding —
   P1–P6 already ACCEPTED under this review.  
2. Spike code may only implement pinned cells.  
3. If a pin must change to pass → record AMBRA + debt; do not silently rewrite this table after the run.  
4. AMBRA pins that product G1b will inherit → write into G1b tranche preregistration (still ≠ REV7).  
5. **Worktree create blocked until P7 OK.**

---

## 6. Minimal workstreams

Order is dependency order. Spike may stop early on RED / AMBRA-necessity.

### WS0 — Hygiene / isolation / freeze baseline (plan-approved; execute after P7 OK)

**Close isolation ambiguity (do not use `feature/motore-v3-offline` tip for spike commits):**

```text
Worktree path:  .claude/worktrees/motore-v3-g1b-spike
                (absolute under repo parent worktrees layout)
Branch:         spike/motore-v3-g1b-frontend
Base commit:    a2186ac1   # G1a remediation tip (code)
Seed:           COPY (not merge) uncommitted trees from motore-v3-offline:
                  - ml_v3/frontend/
                  - ml_v3/tests/test_g1b_t1_resampler_coeffs.py
Policy:         NO commits on feature/motore-v3-offline from the spike
                NO CONTRACT / lock / SHA256SUMS / fixture-spec mutation
```

Setup steps (**after Marco OK on P7; P1–P6 ready; not in this plan session**):

```bash
# From main repo / worktree parent — illustrative; run only post-approval
git worktree add -b spike/motore-v3-g1b-frontend \
  .claude/worktrees/motore-v3-g1b-spike a2186ac1
# Then copy uncommitted frontend + T1 test from motore-v3-offline into the
# new worktree working tree (cp -R); do not merge offline tip.
```

Also in WS0:

- Re-verify lock digest + SHA256SUMS (48) on gate venv (F4 pattern).  
- Do **not** treat uncommitted frontend as tip.  
- Reuse G1a artifacts; **zero** CONTRACT edits.  
- Evidence only on `/Users/marco/aieq_data/motore_v3/env/venv` (3.12.13).

### WS1 — Resampler coeffs (T1) → streaming apply (T1b)

**Reuse spike:** `fir_lowpass_coefficients`, `resample_ratio`,
`group_delay_rational` (already lock-aligned for gate rates).

**Add (product/spike code, post-approval, on spike branch only):**

1. Causal polyphase apply §5: phase-zero,
   `y[j] = sum_n x[n] * h[j*down - n*up]` with out-of-range h = 0.  
2. Streaming state: history of input samples across blocks; emit
   `0 <= j < ceil(N_block_cumulative * up/down)` without trailing filter flush.  
3. Identity path 48 kHz: delay 0, passthrough.  
4. Unit tests: offline block vs one-shot byte-identical under **P5** cast policy.  
5. Fail-closed: reject non-accepted SR, NaN/Inf input.

**Stop early:** if streaming vs offline **resampler output** alone is not
bit-identical on gate platform → investigate float reduction order (P5) before
building FFT stack (likely fixable; not yet REV7).

### WS2 — Feature path offline (canonical)

Implement §6+§7 offline only, under pinned P1–P7:

1. Mid/side from mono/stereo (§4.1).  
2. Resample → 48 kHz.  
3. Dual-res STFT: Hann periodic, right-aligned, hop 1024, first frame at
   8192 samples.  
4. PSD formula §6.1; band energy (**P1/P2**); dB after mean+fuse; floor/clamp.  
5. LF/MAIN fusion 160–320 Hz raised-cosine on log2.  
6. shape (**P4**) / prominence σ=4, j=-16..16 (**P3** reflect) / delta / level /
   valid / reason (**P6/P7**).  
7. Emit `V3FeatureFrame` with rational `source_time`.

**Tests:** determinism two-process; structural §7; silence / NaN reject;
LF and MAIN share `frame_end_sample`.

### WS3 — Streaming schedule from lock (adversarial subset)

Wrap WS1+WS2 with chunk feeder:

- Schedules: **`1`, `8193`, geometric-32** from
  `frozen_metrology_lock()["streaming_equivalence"]` (do not regenerate U).  
- Full 7-schedule matrix deferred to official G1b.  
- Compare full frame sequences offline vs streaming.  
- Multi-asset: explicit `delta_db` history reset at asset boundary
  (smoke; not sole GREEN criterion).

### WS4 — Eval harness (SR-parity) on adversarial assets

1. **Do not write a WAV loader.** Use `render_*` from
   `ml_v3/fixtures/g1/render_signals.py` (return float32 arrays
   byte-identical to committed WAVs). Optionally cross-check SHA256SUMS
   integrity of on-disk WAVs separately; not required for frame path I/O.  
2. Cache expensive `render_pseudo_noise` (seed fixed in fixture-spec).  
3. Useful window / frame grid / sweep crossing via `fixture_spec.py`:
   `common_useful_window`, `_useful_frame_times` (or public nearest helpers),
   `sweep_crossing_time`, `assert_sweep_checkpoints_reachable`;
   warm-up/coda via `metrology_lock.warm_up_seconds` / `coda_seconds`.  
4. Cross-SR: intersection of useful segments; nearest-`source_time` align.  
5. Activity predicate; max|Δ| over domain; report per-field / per-band peaks
   **and margin to 0.25**.  
6. Sweep: checkpoint table from lock; missing checkpoint → FAIL.  
7. Primary assets: `multitone`, `pseudo_noise`, `log_sweep` @ 44100/48000/96000.  
8. Optional diagnostic only: decorrelated / mid_only / side_only; transient
   onset **out of primary scope** (risk §10).

**Output:** `ml_v3/reports/G1B_SPIKE_EVIDENCE.md` (future; not claimed here)
with tables of max|Δ|, margin, bit-identity PASS/FAIL, and
GREEN/AMBRA/RED/INCONCLUSIVE — still **≠ G1 PASS**.

---

## 7. What to reuse from frozen G1a (do not reinvent)

| Artifact | Tip / digest | Spike use |
|----------|--------------|-----------|
| Contract freeze | `6d254d0a` | Thresholds, formulas, stop rules |
| Metrology lock | T4 `3bfd8aaf`; sha `d2c35ccc…` | Warm-up, schedules, domain, bit-identity platform, delays |
| Fixture-spec v1 | M2 `e9916319`; sha `513c3baf…` | Asset IDs, SR matrix, generators binding; **helpers:** `common_useful_window`, `sweep_crossing_time`, useful frame grid / nearest distance, checkpoint reachability |
| WAV inventory + generators | T6 `501a4e00` | Gate audio bytes; **`render_*` → float32** (byte-identical to WAV) — **no WAV loader in spike** |
| SHA256SUMS | chain from T5 `75cb6902` + M2/hygiene/T6 | Integrity verify before evidence; do not mutate |
| Schema registry / contracts | T2/T2.1 + F3 remediation `a2186ac1` | Frame schema id, constants, grid |
| Adapter mapping | T3 sha `6a978c01…` | Out of spike path (G1c+) |
| Grid centres hash | lock `grid_centers_sha256` | Band geometry |
| Uncommitted T1 frontend | copy into spike worktree only | Seed; not proof |

**Deps policy:** numpy + stdlib only; **no scipy / soundfile**; do not touch
`requirements.lock`. Cache `render_pseudo_noise`.

---

## 8. Stop-rules (when to STOP vs continue)

### STOP → propose REV7 (falsifiable impossibility)

Trigger only if **all** hold:

1. Implementation is contract-faithful (formulas §5/§6/§7; no threshold hacks).  
2. Harness matches lock (window, activity, alignment, **spike** schedules, platform).  
3. Bugs in state/reset/cast ruled out with unit evidence.  
4. Still: **(a) RED** (max|Δ| systematically > 0.25 on gate-closing domain)
   and/or **(b) RED** (offline≠streaming bit-identity forced by contract
   semantics on gate platform).  
5. Write-up shows *why* REV6 cannot hold (e.g. inherent SR imaging vs 0.25
   domain; non-associative reduction with no streaming-equivalent form) —
   not “hard to implement” or “slow”.

Then: invoke `ember-contract-guardian` for REV7 GO/NO-GO; **do not** silently
edit CONTRACT from this spike.

### STOP → AMBRA debt (no REV7)

- Pass only after changing a P1–P7 pin post-hoc.  
- Pass with no margin (e.g. max|Δ| = 0.24 dB on a closing cell).  
- Write freeze-from-prose preregistration debt; continue only after pins
  re-approved; verdict remains AMBRA until re-run under a priori pins + margin.

### STOP → durable debt / continue engineering (no REV7)

- Single-band miss due to alignment off-by-one → fix harness.  
- Float cast order bug → freeze cast policy (P5), re-test.  
- Empty active set mishandled → FAIL harness (lock packaging), fix.  
- Experimental SR 88.2/176.4/192 used to “close” gate → invalid; ignore.  
- Using mean/p95 instead of max → invalid; discard run.  
- Evidence on non-3.12.13 venv → INCONCLUSIVE.  
- Importing v2 nearest-bin for empty MAIN support (violates P2) → discard run.

### CONTINUE G1b @ REV6 unchanged

- Spike evidence **GREEN** on (a) and (b) on gate platform under a priori P*.  
- Proceed to suggested commit tranches (§9) **on spike branch / worktree**.  
- Still **no G1 PASS** until G1b–G1e DoD + guardian CLOSE.  
- **No commits on `feature/motore-v3-offline` from spike work.**

---

## 9. Suggested commit tranche order (AFTER Marco OK on P7 + this plan)

Do **not** implement product in the plan session. Do **not** create the
worktree until Marco OK on **P7** (P1–P6 ready). After approval:

| # | Tranche | Paths (typical) | DoD slice |
|---|---------|-----------------|-----------|
| 0 | Docs: this plan (optional, if not already landed) | `ml_v3/reports/G1B_SPIKE_PLAN.md` | Planning artifact only; may land on offline branch as docs-only |
| 0b | Create spike worktree + branch from `a2186ac1`; copy frontend seed | worktree only | Isolation policy §6 WS0 |
| 1 | **G1b-T1** coeffs + frame stub | `ml_v3/frontend/{resampler_coeffs,feature_frame,__init__}.py`, `ml_v3/tests/test_g1b_t1_*.py` | Land seed as tip **on spike branch**; lock ratio tests green on gate venv |
| 2 | **G1b-T1b** streaming polyphase apply | `ml_v3/frontend/resampler.py` (+tests) | Offline≡chunk at resampler output (P5) |
| 3 | **G1b-T2** offline feature path | `ml_v3/frontend/feature_*.py` (+tests) | §6/§7 emit under P1–P7; determinism |
| 4 | **G1b-T3** streaming feature + adversarial schedules | frontend + `test_g1b_streaming_*.py` | Gate-3 bit-identity on `{1,8193,geo-32}` |
| 5 | **G1b-T4** SR-parity harness | eval under `ml_v3/` ALLOWED + reports | Gate-4 numbers on adversarial assets; evidence doc; margin table |
| 6 | Guardian CC + redteam per PLAN mandato | — | One redteam + one independent CC per tranche |

After each commit (CONTRACT §14):

```bash
git diff --cached --check
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m compileall -q ml_v3
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest discover -s ml_v3/tests -p 'test_*.py'
git diff --name-only 2c88edad -- Source ml_v2 CMakeLists.txt Resources AIEQ-mac
```

Last command must be empty. Spike commits stay on
`spike/motore-v3-g1b-frontend` only.

---

## 10. Risk register (feasibility hotspots under REV6)

| Risk | Why it threatens 0.25 / bit-identity | Spike probe |
|------|--------------------------------------|-------------|
| 44.1↔48 polyphase length (`num_taps=20481`) | Long causal delay + edge energy vs 48 identity path | Measure multitone/noise after warm-up first |
| Ultrasonic / imaging at 96 | Anti-alias is separate gate (−80 dB); can still leak into PSD cells | Watch active cells near Nyquist of 48 |
| Dual-res fusion boundary 160–320 Hz | Cross-SR binning differences concentrate here | Report per-band max|Δ| heatmap |
| Empty MAIN triangular support (P2) | Silent v2 nearest-bin would fake GREEN | Assert no fallback; count empty bands |
| Prominence reflect pad + σ=4 (P3) | Edge bands sensitive | Include in domain; do not drop |
| Float32 reduction order FFT/sum (P5) | Streaming≡offline bit-identity | Freeze association; test schedules `1` and `8193` first |
| Margin-thin SR-parity | 0.24 dB “pass” is AMBRA not GREEN | Report margin column |
| Unpinned `reason` enum (P7) | Offline/streaming string mismatch → false RED | Pin provisional set before WS2 |
| Delta history across assets | also-required (a) | Explicit reset API in harness |
| Mis-alignment on `source_time` | False RED/GREEN | Unit-test warm-up formulas vs lock rationals |
| Transient onset sub-gate | Out of primary scope; latent product risk | Declared; do not claim covered by spike GREEN |

---

## 11. Commands (evidence platform)

```bash
# Platform pin
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -c \
  'import sys,numpy; print(sys.version); print(numpy.__version__)'
# Expect: 3.12.13 … and numpy 2.5.1

# Integrity (G1a) — read-only verify
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest \
  discover -s ml_v3/tests -p 'test_g1a_*.py'

# Spike / future G1b tests only (after code exists on spike worktree)
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest \
  discover -s ml_v3/tests -p 'test_g1b_*.py'
```

---

## 12. Handoff

| Agent | Action after plan + P1–P7 approval |
|-------|-------------------------------------|
| **Marco** | **P7 only remaining:** accept provisional `reason` enum (or supply canonical). P1–P6 ACCEPTED. Then authorize worktree create + WS1 |
| **ember-phase-builder** | Create worktree; implement one WS/tranche at a time inside ALLOWED_PATHS on spike branch |
| **ember-parity-lab** | When WS4 emits numbers: verify digests / max|Δ| / margin tables |
| **ember-contract-guardian** | Counter-check each tip; REV7 only if spike RED + write-up; AMBRA → debt not amend |
| **ember-metrology-redteam** | Attack false-PASS in harness (window, activity, platform, P* post-hoc) |
| **ember-rt-sentinel** | N/A until Source integration (not G1b lab) |

---

## 13. Declaration

- This document does **not** claim G1 PASS, G1b CLOSE, or Ableton readiness.  
- Current `ml_v3/frontend/` uncommitted tree is a **feasibility probe**, not
  evidence for gates 3/4.  
- CONTRACT @ `6d254d0a` remains frozen; **REV7: NO** unless spike returns RED
  under the stop-rule in §8.  
- **AMBRA ≠ REV7**; it is preregistration debt.  
- Spike worktree/branch isolation: no commits on `feature/motore-v3-offline`
  from spike.  
- This plan session stops **before** worktree creation and product code.  
- **P1–P6 ready; P7 awaiting Marco; worktree create blocked until P7 OK.**
