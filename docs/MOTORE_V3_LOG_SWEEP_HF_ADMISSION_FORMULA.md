# Motore v3 — Concrete formula: log_sweep A3 admission (`ACTIVE_sweep`)

**Suggested commit title (if/when Marco authorizes commit):**  
`docs(v3): pin log_sweep A3 ACTIVE_sweep formula (F1 neighbourhood)`

| Field | Value |
|-------|--------|
| **Status** | **FORMULA DRAFT** — document-only; ready for **second** delta redteam; **CC authorized after that** if verdict ≠ `CONTRACT-BROKEN`; **re-measure still blocked** until residuals accepted |
| **Parent stamp** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` (A · A3 · fuori REV7 LF) |
| **Decisions** | **A** (a-priori admission amend) + **A3** (sweep-scoped predicate **AND** neighbourhood around `f_inst`) |
| **Date** | 2026-07-26 |
| **Freeze contract** | `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` (REV6) — **not edited** |
| **Lock digest** | `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` — **not re-hashed** |
| **SHA256SUMS** | untouched |
| **Contamination guard** | No constant fitted to 6.435 / 5.838 dB; widths from §6 + fixture_spec + lock delay only |

**≠** G1 PASS · ≠ REV7 consolidate · ≠ G1b tip · ≠ frontend implement · ≠ re-measure authorization

---

## 0. Closing statement (normative boolean — not deferred)

This note pins a single executable boolean

```text
ACTIVE_sweep(b, ch, chk) ∈ {true, false}
```

and the gate-4 closing cell set for `log_sweep` checkpoint portions. There is
**no** “formula deferred” slot. Downstream redteam judges this prose; after a
**second** delta redteam with verdict ≠ `CONTRACT-BROKEN`, independent CC is
authorized. Re-measure remains **blocked** until residuals are accepted.

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

**REV7 LF packaging:** sweep-HF admission stays **outside** the report-only
`i ∉ R` / `EXCLUDED_GEOMETRY` vehicle until a later measure **PASS** under this
formula (parent stamp). A3 is **not** an `EXCLUDED_GEOMETRY` reason.

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

### 2.3 Checkpoint frame, `f_inst`, and single cross-SR `N` (analytic — PSD-argmax forbidden)

**Reference stream for neighbourhood geometry:** after cross-SR alignment
(§13.2 / lock `alignment`: nearest `source_time`, tie-break smaller
`frame_end_sample`), `N(chk)` and `f_inst(chk)` are computed **once** from
the **reference 48 kHz** selected frame. The same `N(chk)` applies to every
host-rate cell of that checkpoint. Per-host recomputation of `N` / `f_inst`
from 44.1 kHz or 96 kHz frames is **forbidden**.

For each checkpoint `chk` with frequency `f_chk ∈ SWEEP_CHECKPOINT_HZ`:

1. `t_cross = sweep_crossing_time(f_chk)` — selection target only.
2. On the **48 kHz** useful-segment lattice, select the `V3FeatureFrame`
   whose `source_time` minimizes `|source_time - t_cross|` (tie-break:
   smaller `frame_end_sample`). Require that distance `≤ T_HOP`; else
   checkpoint-missing **FAIL** (§13.1).
3. Let `source_time*(chk)` and `frame_end_sample*(chk)` be that **48 kHz**
   frame’s stamps. Let
   `dt(chk) := source_time*(chk) - t_cross`.

```text
f_inst(chk) := f( source_time*(chk) )     # analytic chirp law only; 48 kHz frame
```

**Forbidden as neighbourhood centre (normative):**
`argmax_b psd_db[b]` on either render, any smoothed peak tracker, or any
other data-dependent frequency. Diagnostic peak quotes (e.g. “energy near
b111”) are **not** inputs to `N`.

**Forbidden as membership evidence:** using `t_cross` (or any other
in-radius frame than the §2.3 minimizer) in place of `source_time*` when
building `F_TRAJ` / `N`.

`T_HOP` enters **only** as the locked frame-selection match radius that
defines which `source_time*` is admissible. It does **not** further dilate
the frequency aperture of `N` (§4.2) — dilation would double-count the same
timing budget and is rejected.

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

### 3.1 Floor / union atoms (REV6 intent retained)

On the checkpoint-aligned frames of `chk`, channel `ch` (only if that channel
is valid on both aligned renders; else the cell is ignored as in §7 / §13.2).
PSD values may come from any host rate under test; membership `b ∈ N(chk)`
always uses the single 48 kHz neighbourhood of §2.3 / §4:

```text
ABOVE_FLOOR_UNION(b, ch, chk) ⇔
    max( psd_db_ref[b, ch, chk], psd_db_sr[b, ch, chk] ) > -120

AT_FLOOR(x) ⇔ x = -120

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
   everywhere in the 120-band grid, subject only to unrelated future domain
   amends such as geometric `R` if/when consolidated — not by this note).
2. If `ONE_SIDED_FLOOR_UNION ∧ b ∈ N(chk)` → cell is ACTIVE (one-sided
   artifact **inside** the signal-bearing neighbourhood remains gate-visible).
3. If `ONE_SIDED_FLOOR_UNION ∧ b ∉ N(chk)` → cell is **inactive** for the
   gate-4 max (far skirt / leakage residue).
4. **Rejected alternate form:** `closing_domain := N(chk)` alone. That form
   is **not** adopted here.

### 3.3 Single closing predicate

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
REV6 activity. Level scalars unchanged in rule (valid-channel only).

Gate-4 aggregator on the sweep checkpoint portion:

```text
max |Δ|  over all cells with ACTIVE_sweep = true
         (PSD / inherited shape / prominence; level as scoped)
```

Hard threshold `0.25` dB and aggregator `max` remain immutable. One active
cell above threshold → entire gate FAIL.

---

## 4. Neighbourhood `N(chk)` (frozen geometry — no dB fit)

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

If `t_win_lo > t_win_hi` after clamping (should not occur for reachable
checkpoints inside the active interval) → treat `N(chk) = ∅` and apply
vacuous FAIL (§5).

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
| `N` membership | triangular support ∩ `F_TRAJ` | §6.1 supports + window image + lock memory |
| floor cut | `> -120` | §6.1 / REV6 union |
| `ONE_SIDED_FLOOR_UNION` | union ∧ either side `= -120` | skirt class |
| F1 | N restricts exemption only | redteam must-fix; closing domain ≠ only N |
| threshold | `0.25` dB | immutable |

**Explicit non-inputs:** any function of measured `max|Δ|`, band-failure lists,
or the numeric pair `(6.435, 5.838)`.

---

## 5. Vacuous FAIL (fail-closed)

**Normative:**

> For every `chk ∈ SWEEP_CHECKPOINT_HZ` and every channel `ch` that is valid
> on the aligned checkpoint frames (`mid_valid` / `side_valid` as applicable;
> `log_sweep` is mono → `mid` only), if
> `{ b | ACTIVE_sweep(b, ch, chk) } = ∅`, then gate 4 is **FAIL** for that
> checkpoint × channel. Empty active set ≠ PASS. No N/A skip, soft-pass, or
> “no cells to compare.”

This applies even when `N(chk)` is non-empty but every in-`N` cell is at
floor on both sides and every out-`N` one-sided cell has been exempted away.

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
| `portion` | `checkpoint_grid` |
| `chk_hz` | checkpoint frequency |
| `source_time_star` | selected **48 kHz** frame `source_time*` |
| `frame_end_sample_star` | selected **48 kHz** `frame_end_sample*` |
| `dt_to_t_cross` | `source_time* − t_cross` |
| `f_inst` | analytic `f(source_time*)` |
| `N_sorted` | sorted band-index list of `N(chk)` |
| `band` | `b` |
| `channel` | `mid` / `side` |
| `psd_db_ref`, `psd_db_sr` | clamped values |
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

Omitting these fields → report **FAIL**.

### 6.3 Anti-laundering vs REV7 LF `EXCLUDED_GEOMETRY`

| rule | |
|------|--|
| Forbidden | Tagging an A3-inactivated HF skirt cell as `EXCLUDED_GEOMETRY`, `i ∉ R`, or report-only LF solely to remove it from the gate-closing max |
| Forbidden | Moving band **b106-class** HF checkpoint cells into the REV7 LF report-only vehicle because they fail under REV6 union |
| Required | A3 inactivations use reason `A3_ONE_SIDED_OUT_OF_N` only; LF geometry exclusions (if/when consolidated) remain a **separate** table with `EXCLUDED_GEOMETRY` |
| Required | Gate-4 PASS/FAIL prose must state that sweep checkpoint activity is `ACTIVE_sweep` (F1), not “domain = N”, and not “∉ R” |

---

## 7. Paper acceptance tests (redteam must-fix surface)

These are **paper** tests on the formula (no re-measure in this tranche):

| # | Construction | Required outcome |
|---|--------------|------------------|
| T1 | `psd_ref = -120`, `psd_sr = -100`, band `b` with `b ∈ N(chk)` at a checkpoint where `f_inst` lies in/near that band’s support (instantaneous peak neighbourhood) | `ONE_SIDED_FLOOR_UNION` ∧ `ACTIVE_sweep = true` (one-sided **in-N** stays ACTIVE) |
| T2 | Same one-sided floor pattern on a band `b ∉ N(chk)` (skirt outside MAIN/LF window chirp image + `T_MEM`) | `ACTIVE_sweep = false`; appears in §6.1 table with `A3_ONE_SIDED_OUT_OF_N` |
| T3 | Both sides `> -120` on a band outside `N(chk)` | `ACTIVE_sweep = true` (F1: domain ≠ only N) |
| T4 | Claim that b106-class HF FAIL is report-only via `R` / `EXCLUDED_GEOMETRY` without A3 | **REJECT** / laundering (§6.3) |
| T5 | Empty `ACTIVE_sweep` set on any required checkpoint × valid channel scored as PASS | **REJECT** (vacuous) |
| T6 | Neighbourhood centre taken from PSD-argmax | **REJECT** (§2.3) |
| T7 | Stationary multitone / pseudo_noise closing predicate altered by this note | **REJECT** (§1) |
| T8 | `N` width chosen by targeting 6.435 / 5.838 or by post-hoc dropping 16 kHz | **REJECT** (contamination) |
| T9 | `N` / `F_TRAJ` built from `t_cross` proxy or any non-minimizer in-hop frame | **REJECT** (§2.3 / §8) |
| T10 | Distinct `N` per host rate after cross-SR alignment | **REJECT** (§2.3: single 48 kHz `N`) |
| T11 | `t_win_*` not equal to the §6.2 PSD window source-time image (plus `T_MEM` on lo) | **REJECT** (§4.1) |

---

## 8. Worked geometry check (illustrative — normative path only)

### 8.1 Quarantine: `t_cross` proxy is non-normative

A prior draft illustrated 16 kHz with
`source_time* := t_cross(16000)`, yielding
`F_TRAJ ≈ [9984.35, 16000]` and `N = {107,…,116}` with `b106 ∉ N`.

That construction is **quarantined**. It **MUST NOT** be cited as
membership evidence. Normative `N` uses only the §2.3 nearest-frame
`source_time*` after `|dt|` minimization (tie: smaller `frame_end_sample`)
on the reference 48 kHz lattice.

### 8.2 Honest nearest-frame identity at 16 kHz (REF-48k)

Locked hop lattice at 48 kHz (`frame_end_sample = 8192 + k·H` for useful
frames; identity `gd = 0` ⇒ `source_time = frame_end_sample / fs_c`).
Binary64 / fixture_spec law; `T_MEM = 16/11025`; MAIN-pure aperture:

```text
f_chk              = 16000
t_cross            ≈ 1.7096208279 s
source_time*       = 81920 / 48000 = 1.706666… s
frame_end_sample*  = 81920
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
| `sample_rate_parity.sweep_neighbourhood.rule` | support ∩ `F_TRAJ` |
| `sample_rate_parity.sweep_neighbourhood.T_anal_split_hz` | `320` |
| `sample_rate_parity.sweep_neighbourhood.f_inst` | `analytic_chirp_at_source_time_star_48k` |
| `sample_rate_parity.sweep_neighbourhood.ref_stream` | `48000` |
| `sample_rate_parity.sweep_neighbourhood.T_mem` | `max_gate_resampler_group_delay_seconds` |
| `sample_rate_parity.sweep_neighbourhood.t_win` | `§6.2_psd_window_source_time_image` |
| keep | `threshold_max_abs_db = 0.25`, aggregator `max`, `SWEEP_CHECKPOINT_HZ`, reachability |

Silent lock edit without CONTRACT amend GO → BLOCKER.

---

## 10. Next permitted action

| step | status |
|------|--------|
| Parent decisions A / A3 / fuori REV7 LF | stamped |
| First delta redteam | **CONTRACT-POROUS** (must-fixes addressed in this draft) |
| This concrete formula (revised) | **ready for second delta `ember-metrology-redteam`** |
| Independent CC under written rule | **authorized after** second delta ≠ `CONTRACT-BROKEN` |
| Re-measure on gate platform | **blocked** until residuals accepted |
| Frontend implement / tip / REV7 consolidate | **forbidden** as next step |

**Ready for second delta redteam; CC authorized after that if non-BROKEN; remeasure still blocked until residuals accepted.**

---

## 11. References

1. `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` — A / A3 stamp
2. `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` — §6, §13.1–13.2
3. `ml_v3/contracts/fixture_spec.py` — `sweep_crossing_time`, active interval
4. `ml_v3/contracts/metrology_lock.py` — `resampler_group_delay_rational` / `T_MEM`
5. `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` — `EXCLUDED_GEOMETRY` (anti-launder foil)
6. `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` — Hann null-to-null pin
7. `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` — trigger evidence only (not a fit target)

---

≠ G1 PASS. ≠ REV7 consolidate. ≠ G1b tip. ≠ re-measure GO. ≠ lock mutate.
