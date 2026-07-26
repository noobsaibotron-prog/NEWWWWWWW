# Motore v3 — Concrete formula: log_sweep A3 admission (`ACTIVE_sweep`)

**Archival commit title:**  
`docs(v3): archive A3 as SOUND+FALSIFIED+RETIRED`

| Field | Value |
|-------|--------|
| **Status** | **ARCHIVAL — CONTRACT-SOUND + RETIRED AS SOLUTION** — dual independent closure complete (redteam + CC/Guardian); formula tip `04e47b39`; falsification measure `78da84dd` (**FAIL** honest); **ACTIVE family closed**; **no further A3-as-solution work**; stamp sibling `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| **Parent stamp** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` (A · A3 · fuori REV7 LF) |
| **Archive stamp** | `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| **Falsification** | `docs/MOTORE_V3_LOG_SWEEP_A3_FALSIFICATION_MEASURE.md` @ `78da84dd` |
| **Decisions** | **A** (a-priori admission amend) + **A3** (sweep-scoped predicate **AND** neighbourhood = **solo `F_TRAJ`** — **not** peak-local ±K) |
| **Date** | 2026-07-26 |
| **Freeze contract** | `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` (REV6) — **not edited** |
| **Lock digest** | `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` — **not re-hashed** |
| **SHA256SUMS** | untouched |
| **Contamination guard** | No constant fitted to 6.435 / 5.838 dB; widths from §6 + fixture_spec + lock delay only |

**≠** G1 PASS · ≠ REV7 consolidate · ≠ G1b tip · ≠ frontend implement · 0.25 hard · **≠** reopen A3/A4 as solution

---

## 0. Closing statement (normative boolean — not deferred)

This note pins a single executable boolean

```text
ACTIVE_sweep(b, ch, chk) ∈ {true, false}
```

and the gate-4 closing cell set for `log_sweep` checkpoint portions. There is
**no** “formula deferred” slot. Downstream **independent** redteam/CC judges
this prose and alone may emit `CONTRACT-SOUND` / `CONTRACT-POROUS` /
`CONTRACT-BROKEN`. This document **forbids** self-stamping `CONTRACT-SOUND`
(including via commit-message authority shopping). Re-measure remains
**blocked** until a non-POROUS / non-BROKEN acceptance and Marco OK.

---

## 0.1 Must-fix changelog (this draft)

Independent CC @ tip `81e86dc5` returned **CONTRACT-POROUS**; third
redteam returned **CONTRACT-POROUS** again (HIGH#1 / MED#2; empty-N /
chirp-reachability as HIGH#2). Closures in this prose (no remeasure; no
lock/CONTRACT/SHA256SUMS edit):

| # | Must-fix | Closure |
|---|-----------|---------|
| 1 | A3 = solo `F_TRAJ`, non peak-local ±K | §1 / §4.0 / §4.2 / T1 rewrite / T12: neighbourhood := support ∩ `F_TRAJ` only; any peak-local / ±K / PSD-neighbour pad → REJECT |
| 2 | Vietare closing `ACTIVE∩R` sul sweep | §3.2 / §3.3 / §6.3 / T13: closing domain = `{ACTIVE_sweep}` alone until Guardian consolidate GO; `ACTIVE_sweep ∩ R` / `i∉R` / `EXCLUDED_GEOMETRY` on sweep closing max → report FAIL |
| 3 | `AT_FLOOR` float32 pin | §3.1: `AT_FLOOR` ⇔ stored post-clamp `float32` `psd_db == −120`; near-floor / ε-tolerance forbidden |
| 4 | Lattice warm-up consistency | §2.3: eligible frames only on useful-segment lattice (`frame_end = N_LF + m·H`) with `source_time` in §13.1 common useful window; pre-warm-up / off-lattice → FAIL |
| 5 | Pairing cross-SR pin | §2.3.1: each `sr ∈ {44100,96000}` selects its own nearest useful frame to `t_cross`; compare pair `(ref_48k*, sr*)`; `N`/`f_inst` only from ref; other pairing → FAIL |
| 6 | Portion hashed identity | §1.1: `portion_id = checkpoint_grid` is the sole A3 portion token; mismatch / alias → FAIL; future lock key sketched (not hashed this tranche) |
| 7 | Status ≠ self-SOUND | header / §0 / §10: no self-`CONTRACT-SOUND`; ready for redteam/CC delta; remeasure blocked until non-POROUS |
| **HIGH#1** | Tie-break order ≠ lock | §2.3 / §2.3.1 / §8.1 / T21: `alignment.tie_break` = `[smaller_frame_index, then smaller_frame_end_sample]` — byte-identical to lock digest `d2c35ccc…` / `metrology_lock.py`; inverted order deleted |
| **HIGH#2** | Empty `N` at chk 20 Hz / `st* ∉ [t_start,t_end]` | §2.3.2 / §4.1 / §5 / T18: **one** fail-closed rule — `CHK_CHIRP_REACHABLE ⇔ t_start ≤ source_time* ≤ t_end`; else checkpoint **FAIL** (no skip, no `t_cross` proxy, no clamp `st*` into chirp, no blanket `ONE_SIDED` exemption / 6.44 shopping) |
| **MED#2** | Escape «different hashed lattice» → `b106∉N` | §4.2 / §6.2 / T19: under lock digest `d2c35ccc…` + §2.3/§4, `b106_in_N_at_16k` **MUST** be `true`; alternate lattice only after Guardian lock amend; private / unamended hashes → geometric **FAIL** |
| **LOW#3** | `level_dbfs` vs N/ONE_SIDED | §3.1 / §3.3 / T20: `level_dbfs` enters gate-4 max iff channel valid on both paired frames; **not** subject to `N` / `ONE_SIDED_FLOOR_UNION` exemption |

**Preserved prior SOUND closures (unchanged intent):** nearest-frame REF-48k
`N`; lock `T_MEM`; §6.2 window binding; honest `b106 ∈ N(16000)`; F1
boolean; analytic `f_inst`; vacuous FAIL; immutable `0.25` dB / aggregator
`max` (no relax).

---

## 1. Scope (A3 only)

Let `asset`, `portion`, `ch`, `b`, `chk` denote asset id, §13.1 portion,
channel role, band index `b ∈ {0,…,119}`, and preregistered sweep checkpoint.

```text
SWEEP_A3_SCOPE(asset, portion) ⇔
    asset = log_sweep
  ∧ portion = checkpoint_grid
```

| case | activity rule |
|------|----------------|
| `SWEEP_A3_SCOPE` | `ACTIVE_sweep` of §3 (this note) |
| all other §13.1 assets / portions (multitone, pseudo_noise, …) | **unchanged** stationary / REV6 (or later consolidated geometric-`R`) activity — **not** rewritten by A3 |

**A3 neighbourhood meaning (normative disambiguation):** the phrase
“neighbourhood around `f_inst`” in the parent stamp **means exclusively**
the geometry of §4: triangular support ∩ `F_TRAJ(chk)`, where `F_TRAJ` is
the analytic chirp image of the §6.2 PSD window (+ `T_MEM`). It does **not**
mean a peak-local band pad (±K indices around the band containing `f_inst`),
PSD-argmax neighbours, or any Hz disk centred on `f_inst` alone.

**REV7 LF packaging:** sweep-HF admission stays **outside** the report-only
`i ∉ R` / `EXCLUDED_GEOMETRY` vehicle until a later measure **PASS** under this
formula (parent stamp). A3 is **not** an `EXCLUDED_GEOMETRY` reason.

### 1.1 Portion identity (fail-closed — not re-hashed this tranche)

```text
A3_PORTION_ID := "checkpoint_grid"
```

- `SWEEP_A3_SCOPE` holds only when `portion = A3_PORTION_ID` exactly
  (string identity).
- Renaming, aliasing, splitting, or merging sweep portions to avoid A3 or to
  mix stationary / geometric-`R` rules → report **FAIL**.
- Future lock binding (sketch only; **not** hashed in this tranche):
  `sample_rate_parity.sweep.portion_id = "checkpoint_grid"`. Until that key
  exists, evaluators **MUST** hard-pin the same string; mismatch with this
  note → FAIL.

---

## 2. Anchors (frozen geometry / fixture_spec only)

### 2.1 Analysis geometry (§6)

| symbol | value | authority |
|--------|-------|-----------|
| `fs_c` | `48000` | §6 / lock |
| `N_MAIN` | `4096` | §6.2 |
| `N_LF` | `8192` | §6.2 |
| `H` | `1024` | §6.2 / lock |
| `T_MAIN` | `N_MAIN / fs_c` | derived |
| `T_LF` | `N_LF / fs_c` | derived |
| `T_HOP` | `H / fs_c` | match radius §13.1 / fixture_spec |
| `Δf(N)` | `fs_c / N` | FFT bin spacing |
| window | periodic Hann `0.5 - 0.5·cos(2πn/N)` | §6.2 |
| `MAIN_LOBE_NULL_TO_NULL_BINS` | `4` | classical periodic Hann (same pin as scope-rewrite) |
| `W_LOBE_MAIN_HZ` | `(MAIN_LOBE_NULL_TO_NULL_BINS / 2) · Δf(N_MAIN)` = `2 · fs_c / N_MAIN` | half null-to-null in Hz on MAIN |
| centres | `center[i] = 20 · (20000/20)**(i/119)`, ends pinned | §6.1 / `band_centers_hz` |
| triangular support | `lo(b), hi(b)` from previous/next centre; virtual ends via same ratio | §6.1 |
| fusion knees | LF pure `≤160`, MAIN pure `≥320`, raised-cosine crossfade | §6.2 |
| PSD floor / clamp | linear `1e-12`, clamp dB `[-120, +12]` | §6.1 |
| gate threshold | `0.25` dB, aggregator `max` | §13.2 **immutable** |

### 2.2 Log-sweep law (fixture_spec)

Pinned by `ml_v3/contracts/fixture_spec.py` / frozen fixture-spec artifact:

```text
t_start = 1/2 s
t_end   = 7/4 s
T_active = t_end - t_start
f_start = 20 Hz
f_end   = 20000 Hz

for t ∈ [t_start, t_end]:
  u(t) = (t - t_start) / T_active
  f(t) = f_start · (f_end / f_start) ** u(t)     # instantaneous_freq_hz

t_cross(f_chk) = sweep_crossing_time(f_chk)       # inverse of f(·) at f_chk
```

`SWEEP_CHECKPOINT_HZ` and reachability (`nearest useful frame` within
`T_HOP`) are unchanged.

`t_cross` is used **only** as the checkpoint target for frame selection
(§2.3). It is **forbidden** as a substitute for `source_time*` when computing
`f_inst`, `t_win_*`, `F_TRAJ`, or `N` membership (§8 quarantine).

### 2.3 Checkpoint frame, `f_inst`, lattice eligibility, and single cross-SR `N`

**Reference stream for neighbourhood geometry:** after cross-SR pairing
(§2.3.1), `N(chk)` and `f_inst(chk)` are computed **once** from the
**reference 48 kHz** selected frame. The same `N(chk)` applies to every
host-rate cell of that checkpoint. Per-host recomputation of `N` / `f_inst`
from 44.1 kHz or 96 kHz frames is **forbidden**.

**Useful-segment lattice (normative — reconciles §8.2 with CONTRACT §13.1):**

```text
# Per gate rate fs (identity at 48 kHz ⇒ gd = 0):
frame_end_sample ∈ { N_LF + m·H | m ∈ ℕ₀ }
source_time(fs)  = frame_end_sample / fs_c − resampler_group_delay_seconds(fs)

# Eligible iff source_time lies in the §13.1 common useful window:
useful_start = max_fs warm_up_seconds(fs)     # additive warm-up; lock
useful_end   = T_asset − coda_seconds()
eligible(fs) ⇔ useful_start ≤ source_time(fs) ≤ useful_end
```

Only **eligible** frames may be selected. Pre-warm-up frames, coda frames,
and off-lattice `frame_end_sample` values are **not** candidates. Choosing a
non-eligible in-radius stamp to shift `F_TRAJ` → report **FAIL**.

For each checkpoint `chk` with frequency `f_chk ∈ SWEEP_CHECKPOINT_HZ`:

1. `t_cross = sweep_crossing_time(f_chk)` — selection target only.
2. On the **48 kHz** eligible lattice, select the `V3FeatureFrame` whose
   `source_time` minimizes `|source_time - t_cross|`. Tie-break equals lock
   `alignment.tie_break` byte-identical under digest `d2c35ccc…`:
   **smaller `frame_index`, then smaller `frame_end_sample`**
   (`metrology_lock.py` / `metrology_lock.json`). Require that distance
   `≤ T_HOP`; else checkpoint-missing **FAIL** (§13.1).
3. Let `source_time*(chk)` and `frame_end_sample*(chk)` be that **48 kHz**
   frame’s stamps. Let
   `dt(chk) := source_time*(chk) - t_cross`.
4. Apply §2.3.2 chirp-reachability **before** `f_inst` / `t_win_*` / `N`.

```text
f_inst(chk) := f( source_time*(chk) )     # only if CHK_CHIRP_REACHABLE (§2.3.2)
```

**Forbidden as neighbourhood centre (normative):**
`argmax_b psd_db[b]` on either render, any smoothed peak tracker, or any
other data-dependent frequency. Diagnostic peak quotes (e.g. “energy near
b111”) are **not** inputs to `N`.

**Forbidden as membership evidence:** using `t_cross` (or any other
in-radius frame than the §2.3 minimizer) in place of `source_time*` when
building `F_TRAJ` / `N`.

**Forbidden as neighbourhood definition:** peak-local ±K band pads, “bands
whose support contains `f_inst` only”, Hz disks about `f_inst`, or any
construction other than §4 `F_TRAJ` (§4.0).

`T_HOP` enters **only** as the locked frame-selection match radius that
defines which `source_time*` is admissible. It does **not** further dilate
the frequency aperture of `N` (§4.2) — dilation would double-count the same
timing budget and is rejected. `|dt| ≤ T_HOP` alone does **not** imply
chirp reachability (§2.3.2).

### 2.3.1 Cross-SR pairing (normative)

For every under-test rate `sr ∈ {44100, 96000}` at checkpoint `chk`:

```text
frame_sr*(chk) := argmin_{eligible frames at sr} |source_time − t_cross|
                  tie-break: smaller frame_index, then smaller frame_end_sample
                  # = lock alignment.tie_break under digest d2c35ccc…
require |source_time(frame_sr*) − t_cross| ≤ T_HOP   else FAIL (checkpoint missing)

compare cell (b, ch, chk) on the pair:
  ( frame_ref_48k*(chk), frame_sr*(chk) )
```

- `N(chk)` / `f_inst(chk)` / `t_win_*` come **only** from `frame_ref_48k*`.
- Manual shifts, ±1-hop shopping to minimize `|Δ|`, pairing by raw output
  index, or any host↔ref match other than the per-rate nearest-to-`t_cross`
  rule → report **FAIL** (lock `alignment` intent; this note makes the
  sweep pairing explicit).
- PSD / shape / prominence values are read from the paired frames; membership
  `b ∈ N(chk)` never uses the host-rate stamps.
- Host `frame_sr*` is still selected by §2.3.1 even when ref fails
  §2.3.2; the checkpoint outcome remains **FAIL** (pairing does not repair
  chirp-unreachability).

### 2.3.2 Chirp reachability / empty-`N` pin (HIGH#2 — one fail-closed rule)

**Chosen a-priori rule (sole normative pin for this porosity):**

```text
CHK_CHIRP_REACHABLE(chk) ⇔
    t_start ≤ source_time*(chk) ≤ t_end
```

where `source_time*` is the §2.3 REF-48k nearest-to-`t_cross` stamp
(after `|dt| ≤ T_HOP` and lock tie-break).

**If `¬CHK_CHIRP_REACHABLE(chk)` → gate-4 FAIL for that checkpoint**
(chirp-unreachable / N-geometry unreachable). This is mandatory for every
`chk ∈ SWEEP_CHECKPOINT_HZ`, including **chk 20 Hz**, where nearest REF-48k
under the lock lattice yields `source_time* ≈ 0.4907 < t_start = 0.5`
while still `|dt| ≤ T_HOP`.

**Forbidden repairs (any one → report FAIL / REJECT):**

| repair | why forbidden |
|--------|----------------|
| Skip / N/A / soft-pass the checkpoint | mandatory chk must score |
| Substitute `t_cross` for `source_time*` in `f_inst` / `t_win_*` / `N` | §2.3 / §8.1 quarantine |
| Clamp / project `source_time*` into `[t_start, t_end]` | changes the selected frame’s law |
| Widen `t_win_*`, drop `T_MEM`, or otherwise force non-empty `N` | free aperture shopping |
| Treat `N(chk) = ∅` as blanket `ONE_SIDED_FLOOR_UNION` exemption then PASS | empty-N porosity / repair shopping |
| Shop constants from measured 6.435 / 5.838 dB | contamination (§0.1 / T8) |

Only after `CHK_CHIRP_REACHABLE` may the evaluator compute `f_inst`,
`t_win_*`, `F_TRAJ`, and `N` (§4). Empty `N` is therefore **not** an
admission escape: either the checkpoint already FAILed on §2.3.2, or an
inverted window after clamps FAILs under §4.1 / §5.

### 2.4 Resampler memory (lock-derived — no dB fit)

Fail-closed FIR / group-delay memory for the trajectory aperture, a priori
from the frozen lock only:

```text
T_MEM := max_{fs ∈ GATE_SAMPLE_RATES} resampler_group_delay_seconds(fs)
       = max_fs (num_taps(fs) - 1) / (2 · up(fs) · fs)     # identity → 0
```

Under the current lock (`GATE_SAMPLE_RATES = {44100, 48000, 96000}`):

| `fs` | `num_taps` | `resampler_group_delay_seconds` |
|------|------------|----------------------------------|
| 48000 | identity (`None`) | `0` |
| 44100 | `20481` | `16/11025` ≈ `0.001451247` s |
| 96000 | `257` | `1/750` = `0.001333…` s |

Hence `T_MEM = 16/11025` s. No dB fit, no taps count chosen from FAIL bands.

---

## 3. Normative boolean `ACTIVE_sweep`

### 3.1 Floor / union atoms (REV6 intent retained — float32 pin)

On the checkpoint-paired frames of `chk` (§2.3.1), channel `ch` (only if that
channel is valid on both paired renders; else the cell is ignored as in §7 /
§13.2). PSD values may come from any host rate under test; membership
`b ∈ N(chk)` always uses the single 48 kHz neighbourhood of §2.3 / §4.

**`level_dbfs` pin (LOW#3):** `level_dbfs` enters the gate-4 closing max
**iff** the channel is valid on **both** paired frames. It is **not**
subject to `N(chk)` membership or `ONE_SIDED_FLOOR_UNION` exemption —
those restrict only PSD / inherited shape / prominence cells under F1.

**Type pin:** `psd_db_*` atoms below are the **stored post-clamp `float32`**
fields of `V3FeatureFrame` after §6.1 floor/clamp. Recomputing in float64,
promoting, or applying a “near floor” tolerance is **forbidden**.

```text
ABOVE_FLOOR_UNION(b, ch, chk) ⇔
    max( psd_db_ref[b, ch, chk], psd_db_sr[b, ch, chk] ) > -120

AT_FLOOR(x) ⇔ (x is float32) ∧ (x == float32(-120))
            # exact equality on the stored post-clamp value; no ε, no “≈ −120”

ONE_SIDED_FLOOR_UNION(b, ch, chk) ⇔
    ABOVE_FLOOR_UNION(b, ch, chk)
  ∧ ( AT_FLOOR(psd_db_ref[b, ch, chk]) ∨ AT_FLOOR(psd_db_sr[b, ch, chk]) )
```

`ONE_SIDED_FLOOR_UNION` is exactly the class admitted solely by floor-union
while at least one side sits on the §6.1 clamp (the smoking-gun skirt class).

Both-sides-strictly-above-floor cells have
`¬ONE_SIDED_FLOOR_UNION` and are **not** restricted by `N`.

### 3.2 F1 pin (neighbourhood = exemption restrictor — not closing domain)

**F1 (normative):** The neighbourhood `N(chk)` restricts **only** the
continued admission of the `ONE_SIDED_FLOOR_UNION` class. It does **not**
replace the gate-4 closing domain with “bands in `N` only.”

Consequences:

1. If `ABOVE_FLOOR_UNION ∧ ¬ONE_SIDED_FLOOR_UNION` → cell is ACTIVE whether
   or not `b ∈ N(chk)` (true both-sides signal / SR content stays gate-visible
   everywhere in the 120-band grid). Geometric `R` does **not** enter this
   implication for sweep (see §3.3 / §6.3).
2. If `ONE_SIDED_FLOOR_UNION ∧ b ∈ N(chk)` → cell is ACTIVE (one-sided
   artifact **inside** the signal-bearing neighbourhood remains gate-visible).
3. If `ONE_SIDED_FLOOR_UNION ∧ b ∉ N(chk)` → cell is **inactive** for the
   gate-4 max (far skirt / leakage residue).
4. **Rejected alternate form:** `closing_domain := N(chk)` alone. That form
   is **not** adopted here.
5. **Rejected alternate form:** `closing_domain := ACTIVE_sweep ∩ R` (or any
   silent intersection with geometric `R` / `i ∉ R` / report-only LF) on
   `log_sweep` / `checkpoint_grid`. Vehicles stay separate (§6.3).

### 3.3 Single closing predicate and closing domain

```text
ACTIVE_sweep(b, ch, chk) ⇔
    SWEEP_A3_SCOPE
  ∧ ABOVE_FLOOR_UNION(b, ch, chk)
  ∧ (
        ¬ ONE_SIDED_FLOOR_UNION(b, ch, chk)
      ∨   b ∈ N(chk)
    )
```

Shape / prominence cells on the same `(b, ch, chk)` **inherit**
`ACTIVE_sweep` from the channel PSD exactly as §13.2 inheritance works for
REV6 activity.

**Level scalars:** `level_dbfs` enters the closing max iff the channel is
valid on both paired frames (§3.1). It does **not** inherit
`ACTIVE_sweep` / `N` / `ONE_SIDED_FLOOR_UNION` exemption.

**Closing domain (fail-closed, until Guardian consolidate / amend GO):**

```text
CLOSING_CELLS_sweep := { (b, ch, chk) | ACTIVE_sweep(b, ch, chk) = true }
```

Gate-4 aggregator on the sweep checkpoint portion:

```text
max |Δ|  over CLOSING_CELLS_sweep
         (PSD / inherited shape / prominence)
         ∪ { level_dbfs on channels valid on both paired frames }
```

**Forbidden closing domains on sweep (report FAIL if used):**

- `ACTIVE_sweep ∩ R`
- `ACTIVE_sweep \ {i ∉ R}` / any `EXCLUDED_GEOMETRY` filter on the closing max
- `N(chk)` alone
- any post-hoc band subset fitted to measured `|Δ|`

Hard threshold `0.25` dB and aggregator `max` remain immutable. One active
cell above threshold → entire gate FAIL. Geometric-`R` / report-only LF remain
a **separate** vehicle and must not silently intersect the sweep closing max.

---

## 4. Neighbourhood `N(chk)` (frozen geometry — no dB fit)

### 4.0 Sole definition — `F_TRAJ` only (CC must-fix 1)

```text
N(chk) := { b | (lo(b), hi(b)) intersects F_TRAJ(chk) }
```

**Normative:** A3 neighbourhood **=** §4.1–§4.2 only.

**Forbidden neighbourhood definitions (any one → REJECT / report FAIL):**

| forbidden form | why |
|----------------|-----|
| ±K band indices about `argmin_b |center[b] − f_inst|` | peak-local pad; drops smoking-gun skirts (e.g. b106 at 16 kHz) while keeping the `f_inst` band |
| `{ b | f_inst ∈ (lo(b), hi(b)) }` alone | peak-containment only; not the window chirp image |
| Hz disk / lobe pad about `f_inst` (incl. ±`W_LOBE_MAIN_HZ`) | free dilation knob; `W_LOBE` is documentation-only (§2.1) |
| PSD-argmax ±K / peak-tracker neighbours | data-dependent; §2.3 already forbids PSD centre |

`f_inst` enters **only** as (a) the analytic stamp for `N_ANAL` knee
selection in §4.1 and (b) the upper end of `F_TRAJ` via `t_win_hi =
source_time*` under the chirp law — **not** as a local band-pad centre.

### 4.1 Analysis time support = §6.2 causal PSD window image (+ lock memory)

Normative binding: the trajectory interval is the **source-time image of the
same causal right-aligned sample window** used for the fused PSD of the
selected **48 kHz** frame (§6.2), extended on the low side by `T_MEM`
(§2.4).

At identity 48 kHz (`resampler_group_delay_seconds(48000) = 0`):

```text
source_time*(chk) = frame_end_sample*(chk) / fs_c

# §6.2: samples [frame_end_sample* - N_ANAL, frame_end_sample*) at fs_c
N_ANAL(f_inst) :=
    N_LF    if f_inst(chk) < 320 Hz    # LF material (pure or crossfade)
    N_MAIN  if f_inst(chk) ≥ 320 Hz    # MAIN-pure

T_ANAL(f_inst) := N_ANAL(f_inst) / fs_c

# Continuous source-time image of that sample window, fail-closed + T_MEM:
t_win_hi(chk) := min( t_end,   source_time*(chk) )
t_win_lo(chk) := max( t_start, source_time*(chk) - T_ANAL(f_inst(chk)) - T_MEM )
```

Proof of stamp identity at REF-48k: with `gd = 0`, the exclusive-end sample
`frame_end_sample*` maps to `source_time*`, and the first sample of the
right-aligned window maps to `source_time* - T_ANAL`. `T_MEM` then extends
`t_win_lo` earlier by the lock max group-delay so FIR memory at non-identity
gate rates cannot shrink the aperture. No look-ahead past `t_win_hi`.

**Precondition:** §2.3.2 `CHK_CHIRP_REACHABLE` must hold; otherwise do **not**
evaluate `t_win_*` / `N` — checkpoint already FAIL.

If `t_win_lo > t_win_hi` after clamping despite `CHK_CHIRP_REACHABLE` →
`N(chk)` is undefined for admission; gate-4 **FAIL** for that checkpoint
(§5). Do **not** interpret inverted-window / empty `N` as a blanket
`ONE_SIDED_FLOOR_UNION` exemption or soft-pass.

**Derivation:** HF checkpoints (incl. 8/16/20 kHz) are MAIN-pure, so the
binding aperture is the MAIN window image of the chirp (+ `T_MEM`). LF /
crossfade checkpoints use the longer LF window fail-closed when LF is
material. `W_LOBE_MAIN_HZ` is recorded in §2.1 as the Hann half-lobe scale;
at HF it is ≪ one triangular support width, so it is **not** used as an
extra Hz dilation of the aperture (a ±`W_LOBE` pad is a free parameter that
is not required once triangular supports discretize the trajectory — and is
rejected to avoid padding knobs).

### 4.2 Chirp image and band membership

```text
F_TRAJ(chk) := { f(t) | t ∈ [t_win_lo(chk), t_win_hi(chk)] }
            = [ f(t_win_lo(chk)), f(t_win_hi(chk)) ]
              # monotone increasing log-chirp on the active interval

b ∈ N(chk)  ⇔  (lo(b), hi(b)) intersects F_TRAJ(chk) as open intervals
            ⇔  lo(b) < f(t_win_hi(chk))  ∧  hi(b) > f(t_win_lo(chk))
```

Bit-stable evaluation: centres / `lo` / `hi` / `f(·)` in IEEE-754 binary64
with the same closed forms as fixture_spec + §6.1 (virtual ends via ratio
`r = (20000/20)**(1/119)`). No measured tables. No dependence on PSD values.
`N(chk)` is a function of the 48 kHz stamps of §2.3 only.

**Honesty pin (efficacy, not a PASS path — MED#2):** under lock digest
`d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` and the
normative §2.3 / §4 path at 16 kHz, `b106 ∈ N(16000)` (§8.2) is
**mandatory**. A report that claims evaluation under this formula and
publishes `b106 ∉ N(16000)` / `b106_in_N_at_16k = false` → geometric
**FAIL**. Alternate lattices (different hop / warm-up / selection) are
admissible **only** after an explicit Guardian lock amend that re-hashes
`alignment` / lattice keys; private hashes, unamended digests, or
“different hashed lattice proof” attachments that are not that amend →
geometric **FAIL**. If frontend PSD are unchanged, falsification measure
is still expected to **FAIL** on that ACTIVE cell — that is honest
ACCEPT=FAIL territory, not a licence to shrink `N`.

### 4.3 Constants table (hashable later; not hashed in this tranche)

| Constant | Value | Derivation (a priori) |
|----------|-------|------------------------|
| `fs_c` | `48000` | §6 |
| `N_MAIN` | `4096` | §6.2 |
| `N_LF` | `8192` | §6.2 |
| `H` | `1024` | §6.2 |
| `T_MAIN` | `4096/48000` | window duration MAIN |
| `T_LF` | `8192/48000` | window duration LF |
| `T_HOP` | `1024/48000` | frame match radius only (§2.3) |
| `T_MEM` | `16/11025` s | `max` lock `resampler_group_delay_seconds` over gate rates (§2.4) |
| `MAIN_LOBE_NULL_TO_NULL_BINS` | `4` | periodic Hann main lobe |
| `W_LOBE_MAIN_HZ` | `2 · 48000/4096 = 23.4375` Hz | half null-to-null; **non-dilating** documentation constant |
| `T_ANAL` split | `320` Hz | §6.2 MAIN-pure knee |
| `t_start`, `t_end` | `1/2`, `7/4` | fixture_spec active interval |
| `f_start`, `f_end` | `20`, `20000` | fixture_spec |
| `f_inst` | `f(source_time*)` at **48 kHz** selected frame | analytic law; **not** PSD-argmax |
| `t_win_*` | §6.2 PSD window source-time image − `T_MEM` on lo | §4.1 |
| `N` membership | triangular support ∩ `F_TRAJ` **only** | §4.0–§4.2; **≠** peak-local ±K |
| `A3_PORTION_ID` | `checkpoint_grid` | §1.1 |
| floor cut | `> -120` on stored float32 | §6.1 / REV6 union |
| `AT_FLOOR` | stored float32 `== -120` | §3.1 |
| `ONE_SIDED_FLOOR_UNION` | union ∧ either side `AT_FLOOR` | skirt class |
| F1 | N restricts exemption only | closing domain ≠ only N |
| closing domain | `{ACTIVE_sweep}` alone | **≠** `ACTIVE∩R` |
| `level_dbfs` | valid on both paired frames | **not** N / ONE_SIDED exempt (§3.1 / §3.3) |
| tie-break | `smaller_frame_index`, then `smaller_frame_end_sample` | lock `alignment.tie_break` @ `d2c35ccc…` |
| `CHK_CHIRP_REACHABLE` | `t_start ≤ source_time* ≤ t_end` | §2.3.2; else chk FAIL (empty-N pin) |
| `b106_in_N_at_16k` | `true` under current lock | §4.2 / §6.2 / §8.2; no private-hash escape |
| threshold | `0.25` dB | immutable |

**Explicit non-inputs:** any function of measured `max|Δ|`, band-failure lists,
or the numeric pair `(6.435, 5.838)`.

---

## 5. Vacuous FAIL / empty-`N` FAIL (fail-closed)

**Normative (three fail-closed cases — no shopping):**

1. **Chirp-unreachable (§2.3.2):** if `¬CHK_CHIRP_REACHABLE(chk)`
   (`source_time* ∉ [t_start, t_end]`), gate 4 is **FAIL** for that
   checkpoint. Applies in particular to mandatory chk **20 Hz** under
   nearest REF-48k when `source_time* < t_start`.
2. **Inverted window / empty `N` (§4.1):** if `CHK_CHIRP_REACHABLE` holds
   but `t_win_lo > t_win_hi` (or `N(chk) = ∅` after §4), gate 4 is
   **FAIL** for that checkpoint. Empty `N` **≠** blanket
   `ONE_SIDED_FLOOR_UNION` exemption and **≠** PASS.
3. **Empty ACTIVE set:** for every `chk ∈ SWEEP_CHECKPOINT_HZ` and every
   channel `ch` that is valid on the paired checkpoint frames
   (`mid_valid` / `side_valid` as applicable; `log_sweep` is mono → `mid`
   only), if `{ b | ACTIVE_sweep(b, ch, chk) } = ∅`, then gate 4 is
   **FAIL** for that checkpoint × channel. Empty active set ≠ PASS.

No N/A skip, soft-pass, “no cells to compare,” `t_cross` proxy, or clamp
of `source_time*` into the chirp interval. Case 3 also applies when
`N(chk)` is non-empty but every in-`N` cell is at floor on both sides and
every out-`N` one-sided cell has been exempted away.

---

## 6. Mandatory report (anti-laundering)

### 6.1 A3 inactivation table (required)

Any numeric report that claims to evaluate gate 4 under this formula **MUST**
publish (machine-readable OK) every cell such that:

```text
ABOVE_FLOOR_UNION(b, ch, chk) = true
∧ ACTIVE_sweep(b, ch, chk) = false
```

Minimum columns:

| column | content |
|--------|---------|
| `asset` | `log_sweep` |
| `portion` | `checkpoint_grid` (= `A3_PORTION_ID`) |
| `chk_hz` | checkpoint frequency |
| `source_time_star` | selected **48 kHz** frame `source_time*` |
| `frame_end_sample_star` | selected **48 kHz** `frame_end_sample*` |
| `dt_to_t_cross` | `source_time* − t_cross` |
| `f_inst` | analytic `f(source_time*)` |
| `N_sorted` | sorted band-index list of `N(chk)` |
| `band` | `b` |
| `channel` | `mid` / `side` |
| `psd_db_ref`, `psd_db_sr` | stored post-clamp **float32** values |
| `reason` | enum below |

**Reason enum (closed):**

| `reason` | meaning |
|----------|---------|
| `A3_ONE_SIDED_OUT_OF_N` | `ONE_SIDED_FLOOR_UNION` and `b ∉ N(chk)` |

No other A3 inactivation reason exists in this formula. Omitting the table
when any such cell exists → report **FAIL**.

### 6.2 Neighbourhood geometry report (required even when no inactivation)

Independently of §6.1, every gate-4 report under this formula **MUST** publish
per checkpoint:

| field | content |
|-------|---------|
| `chk_hz` | checkpoint frequency |
| `t_cross` | `sweep_crossing_time(f_chk)` (selection target only) |
| `source_time_star` | 48 kHz `source_time*` |
| `frame_end_sample_star` | 48 kHz `frame_end_sample*` |
| `dt_to_t_cross` | `source_time* − t_cross` |
| `f_inst` | analytic `f(source_time*)` |
| `t_win_lo`, `t_win_hi` | §4.1 interval |
| `T_MEM` | lock value used |
| `N_sorted` | ascending list of band indices in `N(chk)` |
| `b106_in_N_at_16k` | required when `chk_hz = 16000`: boolean; **MUST** be `true` under lock digest `d2c35ccc…` + §2.3/§4 / §8.2; `false` or omitted → geometric **FAIL**; alternate lattice only after Guardian lock amend (private / unamended hashes → FAIL) |

Omitting these fields → report **FAIL**.

### 6.3 Anti-laundering vs REV7 LF `EXCLUDED_GEOMETRY` / geometric `R`

| rule | |
|------|--|
| Forbidden | Tagging an A3-inactivated HF skirt cell as `EXCLUDED_GEOMETRY`, `i ∉ R`, or report-only LF solely to remove it from the gate-closing max |
| Forbidden | Moving band **b106-class** HF checkpoint cells into the REV7 LF report-only vehicle because they fail under REV6 union |
| Forbidden | Closing max on `ACTIVE_sweep ∩ R` (or any silent `R` / `i∉R` filter) for `log_sweep` / `checkpoint_grid` before Guardian consolidate GO — **report FAIL**, not a soft warning |
| Required | A3 inactivations use reason `A3_ONE_SIDED_OUT_OF_N` only; LF geometry exclusions (if/when consolidated) remain a **separate** table with `EXCLUDED_GEOMETRY` |
| Required | Gate-4 PASS/FAIL prose must state that sweep checkpoint activity is `ACTIVE_sweep` (F1), closing domain = `{ACTIVE_sweep}`, neighbourhood = `F_TRAJ` only — not “domain = N”, not “∉ R”, not “peak ±K” |

---

## 7. Paper acceptance tests (redteam / CC must-fix surface)

These are **paper** tests on the formula (no re-measure in this tranche):

| # | Construction | Required outcome |
|---|--------------|------------------|
| T1 | `psd_ref = -120`, `psd_sr = -100` (stored float32), band `b ∈ N(chk)` with `f_inst ∉ (lo(b), hi(b))` (one-sided **in-`F_TRAJ`** but not peak-local) | `ONE_SIDED_FLOOR_UNION` ∧ `ACTIVE_sweep = true` |
| T2 | Same one-sided floor pattern on a band `b ∉ N(chk)` (skirt outside MAIN/LF window chirp image + `T_MEM`) | `ACTIVE_sweep = false`; appears in §6.1 table with `A3_ONE_SIDED_OUT_OF_N` |
| T3 | Both sides `> -120` on a band outside `N(chk)` | `ACTIVE_sweep = true` (F1: domain ≠ only N) |
| T4 | Claim that b106-class HF FAIL is report-only via `R` / `EXCLUDED_GEOMETRY` without A3 | **REJECT** / laundering (§6.3) |
| T5 | Empty `ACTIVE_sweep` set on any required checkpoint × valid channel scored as PASS | **REJECT** (vacuous) |
| T6 | Neighbourhood centre taken from PSD-argmax | **REJECT** (§2.3) |
| T7 | Stationary multitone / pseudo_noise closing predicate altered by this note | **REJECT** (§1) |
| T8 | `N` width chosen by targeting 6.435 / 5.838 or by post-hoc dropping 16 kHz | **REJECT** (contamination) |
| T9 | `N` / `F_TRAJ` built from `t_cross` proxy or any non-minimizer in-hop frame | **REJECT** (§2.3 / §8) |
| T10 | Distinct `N` per host rate after cross-SR pairing | **REJECT** (§2.3: single 48 kHz `N`) |
| T11 | `t_win_*` not equal to the §6.2 PSD window source-time image (plus `T_MEM` on lo); **or** selected frame not on eligible useful-segment lattice (`N_LF + m·H` ∩ §13.1 useful window) | **REJECT** (§2.3 / §4.1) |
| T12 | Redefine `N` as peak-local ±K (or peak-containment only) such that `f_inst`’s band stays in `N` but `b106 ∉ N(16000)` | **REJECT** (§4.0) |
| T13 | Closing max := `ACTIVE_sweep ∩ R` (or apply `i∉R` / `EXCLUDED_GEOMETRY` to sweep closing cells) | **REJECT** / report FAIL (§3.3 / §6.3) |
| T14 | `AT_FLOOR` via float64 recompute or “near −120” tolerance | **REJECT** (§3.1) |
| T15 | Host↔ref pairing other than per-rate nearest-to-`t_cross` on eligible lattices | **REJECT** (§2.3.1) |
| T16 | `portion ≠ checkpoint_grid` while claiming A3 / or aliasing portion to dodge A3 | **REJECT** (§1.1) |
| T17 | Formula/report self-declares `CONTRACT-SOUND` without independent redteam/CC | **REJECT** (§0 / §10) |
| T18 | Nearest REF-48k `source_time* ∉ [t_start,t_end]` (e.g. chk 20 Hz with `st* ≈ 0.4907 < 0.5`) scored as skip / N/A / soft-pass; **or** clamp `st*` into chirp; **or** `t_cross` proxy; **or** treat empty `N` as blanket `ONE_SIDED` exemption / 6.44 repair | **REJECT** / checkpoint FAIL (§2.3.2 / §5) |
| T19 | Publish `b106 ∉ N(16000)` / `b106_in_N_at_16k = false` under lock digest `d2c35ccc…`, or attach a private / unamended “different hashed lattice” escape | **REJECT** / geometric FAIL (§4.2 / §6.2) |
| T20 | Exempt `level_dbfs` via `N` / `ONE_SIDED_FLOOR_UNION`, or include it when channel invalid on either paired frame | **REJECT** (§3.1 / §3.3) |
| T21 | Tie-break order `smaller_frame_end_sample` before `smaller_frame_index` (inverted vs lock `alignment.tie_break`) | **REJECT** (§2.3 / §2.3.1) |

---

## 8. Worked geometry check (illustrative — normative path only)

### 8.1 Quarantine: `t_cross` proxy is non-normative

A prior draft illustrated 16 kHz with
`source_time* := t_cross(16000)`, yielding
`F_TRAJ ≈ [9984.35, 16000]` and `N = {107,…,116}` with `b106 ∉ N`.

That construction is **quarantined**. It **MUST NOT** be cited as
membership evidence. Normative `N` uses only the §2.3 nearest-frame
`source_time*` after `|dt|` minimization (tie: smaller `frame_index`,
then smaller `frame_end_sample` — lock `alignment.tie_break`) on the
reference 48 kHz **eligible** lattice.

### 8.2 Honest nearest-frame identity at 16 kHz (REF-48k)

Locked hop lattice at 48 kHz (`frame_end_sample = N_LF + m·H` for useful
frames; identity `gd = 0` ⇒ `source_time = frame_end_sample / fs_c`).
Eligibility requires `source_time` in the §13.1 common useful window
(additive warm-up / coda). Binary64 / fixture_spec law; `T_MEM = 16/11025`;
MAIN-pure aperture:

```text
f_chk              = 16000
t_cross            ≈ 1.7096208279 s
source_time*       = 81920 / 48000 = 1.706666… s
frame_end_sample*  = 81920   # = N_LF + 72·H; eligible (post warm-up)
dt                 = source_time* − t_cross ≈ −0.00295416 s
                   (|dt| ≈ 2.95 ms < T_HOP; next in-hop frame at 1.728 s
                    has larger |dt| and is NOT selected)

f_inst             = f(source_time*) ≈ 15740.92 Hz
T_ANAL             = T_MAIN = 4096/48000
t_win_lo           = source_time* − T_MAIN − T_MEM ≈ 1.619882 s
t_win_hi           = source_time* ≈ 1.706667 s
F_TRAJ             ≈ [9744.22, 15740.92] Hz
N(chk)             = {106, 107, 108, 109, 110, 111, 112, 113, 114, 115}
```

**Honest membership:** `b106 ∈ N(16000)` under the normative nearest-frame
path. Band `106` support `(center[105], center[107]) ≈ (8873.4, 9965.7)`
intersects `F_TRAJ` because `hi(106) ≈ 9965.7 > 9744.22`. Therefore a
`ONE_SIDED_FLOOR_UNION` cell at b106 on this checkpoint remains
`ACTIVE_sweep = true` (F1). A3 does **not** paper-exempt the known smoking-gun
skirt at this checkpoint under nearest-frame geometry.

**Contrast (forbidden peak-local):** ±K about the band containing
`f_inst ≈ 15741` keeps ~b114–b115 and **excludes** b106 — that pad is
exactly the porosity §4.0 / T12 forbid.

(The quarantined `t_cross` proxy had `Flo ≈ 9984.35 > hi(106)`, which falsely
excluded b106 — that is exactly the porosity §8.1 forbids.)

This is a consequence of §6 + chirp law + MAIN window + lock `T_MEM` + hop
lattice selection — not a fit to the FAIL magnitude.

---

## 9. Lock / CONTRACT binding (future amend only — not this tranche)

**This note does not edit** freeze CONTRACT, `metrology_lock`, or SHA256SUMS.

If a later Guardian GO amends, expected new hashed keys (sketch only):

| key | intent |
|-----|--------|
| `sample_rate_parity.activity.sweep_predicate` | string form of `ACTIVE_sweep` |
| `sample_rate_parity.activity.sweep_f1_exemption` | `ONE_SIDED_FLOOR_UNION` definition |
| `sample_rate_parity.activity.at_floor` | `float32_stored_psd_db == -120` |
| `sample_rate_parity.sweep.portion_id` | `checkpoint_grid` |
| `sample_rate_parity.sweep_neighbourhood.rule` | support ∩ `F_TRAJ` (**forbid** peak-local ±K) |
| `sample_rate_parity.sweep_neighbourhood.T_anal_split_hz` | `320` |
| `sample_rate_parity.sweep_neighbourhood.f_inst` | `analytic_chirp_at_source_time_star_48k` |
| `sample_rate_parity.sweep_neighbourhood.ref_stream` | `48000` |
| `sample_rate_parity.sweep_neighbourhood.T_mem` | `max_gate_resampler_group_delay_seconds` |
| `sample_rate_parity.sweep_neighbourhood.t_win` | `§6.2_psd_window_source_time_image` |
| `sample_rate_parity.sweep_pairing` | per-rate nearest-to-`t_cross` on eligible lattice |
| `sample_rate_parity.sweep_closing_domain` | `ACTIVE_sweep_only` (no ∩`R` until consolidate GO) |
| keep | `threshold_max_abs_db = 0.25`, aggregator `max`, `SWEEP_CHECKPOINT_HZ`, reachability, warm-up additive |

Silent lock edit without CONTRACT amend GO → BLOCKER.

---

## 10. Archival status (closed)

| step | status |
|------|--------|
| Parent decisions A / A3 / fuori REV7 LF | stamped |
| Formula tip (POROUS closures landed) | `04e47b39` |
| Falsification measure | `78da84dd` — **FAIL** honest |
| Dual redteam + CC/Guardian | **CONTRACT-SOUND**; falsification **VALID**; **RETIRE_AS_SOLUTION YES** |
| Archive stamp | `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| ACTIVE-family A3-as-solution work | **CLOSED** — no further A3/A4 solution deltas |
| Next living path | `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` (S1+S2) |
| Frontend implement / tip / REV7 consolidate / G1 PASS | **forbidden** |

**Archival triad:** A3 CONTRACT-SOUND + hypothesis FALSIFIED + RETIRED AS SOLUTION.  
See sibling stamp for dual-agent refs. This file is archival prose only.

---

## 11. References

1. `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` — A / A3 stamp
2. `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` — dual SOUND+FALSIFIED+RETIRED
3. `docs/MOTORE_V3_LOG_SWEEP_A3_FALSIFICATION_MEASURE.md` @ `78da84dd`
4. `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` — §6, §13.1–13.2
5. `ml_v3/contracts/fixture_spec.py` — `sweep_crossing_time`, useful lattice, active interval
6. `ml_v3/contracts/metrology_lock.py` — `resampler_group_delay_rational` / `T_MEM` / alignment
7. `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` — `EXCLUDED_GEOMETRY` (anti-launder foil)
8. `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` — Hann null-to-null pin
9. `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` — trigger evidence only (not a fit target)
10. `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` — living next path

---

≠ G1 PASS. ≠ REV7 consolidate. ≠ G1b tip. ≠ lock mutate. 0.25 hard.
≠ reopen A3/A4 as solution.
