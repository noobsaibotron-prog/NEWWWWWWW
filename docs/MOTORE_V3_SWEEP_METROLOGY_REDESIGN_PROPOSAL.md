# Motore v3 — Proposal: SWEEP METROLOGY REDESIGN (S1 + S2)

**Suggested commit title:**  
`docs(v3): quantify companions/side-gate ∀; align §5.2 to D11`

| Field | Value |
|-------|--------|
| **Status** | **PROPOSAL DRAFT** — document-only; **not** self-SOUND; residual POROUS must-fix (re-CC + delta RT `8aa8027c`) — ∀/max companions+side-gate, §5.2↔D11, empty B_OFF FAIL, ¬§13.2.4-equiv; ready for **re-CC**; **MEASURE_AUTHORIZED NO** |
| **≠** | G1 PASS · ACCEPT · measure · REV7 consolidate · G1b tip · CONTRACT/lock/T6 edit · A3 reopen · A4 ACTIVE · silent §13.2.4 / gate-4 equivalence |
| **Date** | 2026-07-26 |
| **Authority** | Marco authorize docs-only residual POROUS must-fix (re-CC `41e6f166…` NO-GO + interrupt merge delta RT `8aa8027c`) |
| **Mandate** | `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` (incl. stationary≠trajectory pin) |
| **A3 status (status only)** | **RETIRED AS SOLUTION** — `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| **Freeze structure (read-only)** | `docs/MOTORE_V3_G1_CONTRACT.md` REV6 @ `6d254d0a`; lock / `fixture_spec` **structure** as frozen quantities |
| **Prior tip** | `2885b0cb` (executable veto+companions+menu; still POROUS — soft aggregator / §5.2 menu echo) |

---

## 0. Contamination firewall

**CLEAN — untainted pen.**

This write used only:

- the redesign mandate (after the stationary≠trajectory pin);
- A3 archive stamp **status line only** (RETIRED AS SOLUTION) — no falsification magnitudes;
- frozen CONTRACT / lock / `fixture_spec` **structure**: window lengths, hop, Hann,
  fusion, resampler delay formula, analytic log-sweep law, checkpoint list,
  nearest-`source_time` / hop match radius, right-aligned causality;
- handoff §A photograph **status only**.

It did **not** open, cite, or use:

- `docs/MOTORE_V3_LOG_SWEEP_A3_FALSIFICATION_MEASURE.md`;
- any A3 FAIL dB values, margin tables, failing-band inventories, or peak-band
  indices from prior runs (incl. b106 / 6.435-class magnitudes);
- any remeasure JSON/output;
- Claude numeric conclusions as design inputs.

Residual POROUS pins in this revision derive constants only from frozen
`N` / hop / `fs_c` / floor −120 / triangular + Hann ENBW–main-lobe geometry.

If a reviewer later finds latent gate-failure magnitudes steering a formula
constant here, this proposal is **void** and must be rewritten by a fresh
untainted writer.

**Self-SOUND is forbidden.** This document marks itself ready for **re-CC**
only; it does **not** claim CONTRACT-SOUND, PASS, ACCEPT, or measure
authorization.

---

## 1. Problem restatement (physics, not outcome)

### 1.1 Two different properties (mandate pin)

| Gate family | Asserts |
|-------------|---------|
| **Stationary** | Two SR paths produce the same spectrum of a **steady** signal. |
| **Full-vector chirp / trajectory spectrum** | Two paths produce the same **windowed** spectrum of a **moving** signal — result depends on how far \(f_{\mathrm{inst}}\) moves inside the analysis window = **joint** property of signal + instrument. |

Stationary evidence on geometric `R` addresses the first property. The current
sweep full-vector PSD comparison asks the second. **S2 exists because these are
not the same claim.** Chirp ≠ “stationary with an asterisk.”

### 1.2 Endpoint geometry (a priori)

Independently of which spectral observable one chooses, checkpoint endpoints
at the chirp edges interact with **right-aligned** STFT + nearest
`source_time` within one hop. That is an **reachability / aperture** question
(**S1**), not a dB-threshold question.

---

## 2. Frozen quantities reused (definitions only)

From CONTRACT §5–§6 / §13.1 and lock / `fixture_spec` structure:

| Symbol | Value | Source |
|--------|-------|--------|
| `fs_c` | 48000 | §6 |
| `N_MAIN` | 4096 | §6.2 |
| `N_LF` | 8192 | §6.2 / lock |
| `H` | 1024 | §6.2 / lock |
| Window | periodic Hann `0.5 - 0.5·cos(2πn/N)` | §6.2 |
| Alignment | causal, **right-aligned** to exclusive `frame_end_sample` | §6.2 |
| Timestamp | `source_time = frame_end_sample/fs_c − resampler_group_delay_seconds` | §5 |
| Match radius | nearest `source_time` within `H/fs_c`; else missing → FAIL | §13.1 |
| Fusion | LF ≤160 Hz; MAIN ≥320 Hz; raised-cosine on `log2(f)` in (160,320) | §6.2 |
| Chirp band | \(f\in[20,20000]\) Hz log | `fixture_spec` |
| Active interval | \(t\in[t_0,t_1]=[1/2,\,7/4]\) s | `fixture_spec` rationals |
| Law | \(f(t)=f_0\cdot(f_1/f_0)^{(t-t_0)/(t_1-t_0)}\) | `sweep_crossing_time` |
| Checkpoints | `(20, 45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000)` Hz | lock `SWEEP_CHECKPOINT_HZ` |
| Resampler taps | `num_taps = 128·max(up,down)+1` (identity → no taps) | §5 / lock |
| Group delay | `(num_taps−1)/(2·up·fs_in)` s | §5 |

Derived (arithmetic only):

```text
T_HOP     = H / fs_c           = 1024 / 48000
T_WIN_LF  = N_LF / fs_c        = 8192 / 48000
T_WIN_MAIN= N_MAIN / fs_c      = 4096 / 48000
```

Group delay is already folded into `source_time`. Cross-SR alignment remains on
`source_time`, not raw output index.

---

## 3. Decision table (this proposal)

| ID | Decision | Status in this doc |
|----|----------|--------------------|
| D1 | S1 = **guard regions** on checkpoint closing domain, from aperture geometry only | **PROPOSED** — D1 alone ⇒ **no SOUND / no measure** |
| D2 | S2 closing package = **ATRL** + CONTRACT companions + artifact veto (+ side-gate hardness) | **LOCKED (menu freeze)** — alts **S2-ALT\*** archival only; not “A3” |
| D3 | Off-ridge **closing conjunction:** ATRL_OK **∧** COMPANIONS_OK **∧** VETO_OK **∧** SIDE_GATE_OK; report alone **does not** close | **LOCKED (fail-closed)** |
| D4 | Immutable: max aggregator; 0.25 dB; stationary `R`; report-only ∉`R`; no post-hoc mask; no threshold shopping | **LOCKED by mandate** |
| D5 | No CONTRACT / lock / T6 / SHA256SUMS / `Source/` edit in this phase | **LOCKED** |
| D6 | No measure; no ACCEPT / PASS / G1b tip claim | **LOCKED** |
| D7 | REV7 = **one** future package after S1+S2 CC — not consolidated here | **LOCKED** |
| D8 | \|CHK_CLOSE\| floor ≥7; empty set → FAIL; `T_MEM=0` locked | **LOCKED (fail-closed)** |
| D9 | `mid_valid=false` / unmatched / empty activity-union on required domains → FAIL | **LOCKED (fail-closed)** |
| D10 | CHK_REPORT publish obligatory; TRANSPORT≠PASS if REPORT absent | **LOCKED (fail-closed)** |
| D11 | **Menu freeze:** single closing package = ATRL + §5.3 companions + §6.2 veto (+ §5.3 side-gate hardness). Open menu → no SOUND / no measure | **LOCKED** |

---

## 4. S1 — Endpoint reachability (guard regions)

### 4.1 Physical ask

A checkpoint at crossing time \(t_\times=t_{\mathrm{cross}}(f)\) is
**closing-admissible** only if there exists a useful-segment frame within the
frozen hop match radius whose **entire longest analysis aperture** (LF window)
lies inside the chirp-active interval \([t_0,t_1]\), under right-aligned
causality.

Otherwise the selected frame’s window mixes chirp with pre-/post-active
content (or cannot be matched). That is an a-priori geometry FAIL mode, not a
spectrum-transport FAIL mode.

### 4.2 Proposed guard (normative candidate)

```text
T_LEAD  = T_WIN_LF + T_HOP     = (N_LF + H) / fs_c
T_TRAIL = T_HOP                = H / fs_c

GUARD_LEAD  = [t_0,  t_0 + T_LEAD)
GUARD_TRAIL = (t_1 − T_TRAIL,  t_1]

CHK_CLOSE(f)  ⇔  t_cross(f) ∈ [t_0 + T_LEAD,  t_1 − T_TRAIL]
CHK_REPORT(f) ⇔  f ∈ SWEEP_CHECKPOINT_HZ  ∧  ¬CHK_CLOSE(f)
```

**Semantics:**

- The preregistered checkpoint **grid is unchanged** (no checkpoint removal).
- `CHK_REPORT` checkpoints remain **obligatory to publish** (see §4.5 schema)
  but **do not enter** the ATRL closing max.
- Missing nearest frame within hop for any checkpoint (close or report) remains
  **FAIL** under the frozen match rule — guards do not invent frames.
- **Cardinality floor (fail-closed):** let \(N_{\mathrm{close}}=|\mathrm{CHK\_CLOSE}|\).
  If \(N_{\mathrm{close}}=\emptyset\) **or** \(N_{\mathrm{close}}<7\) → **FAIL**
  (vacuous / under-populated closing domain). Under the definitional partition
  in §4.3, \(N_{\mathrm{close}}=7\) (60…16000 Hz); the floor pins that geometry.

**Rationale for `T_LEAD`:** worst-case selected `source_time` is
\(t_\times - T_{\mathrm{HOP}}\); LF aperture looks back `T_WIN_LF`; require
window start \(\ge t_0\).

**Rationale for `T_TRAIL`:** worst-case selected `source_time` is
\(t_\times + T_{\mathrm{HOP}}\); right-aligned newest sample must stay
\(\le t_1\) so the aperture does not admit post-active silence at the tip.

**Resampler memory (normative pin):** group delay is already inside
`source_time`. Normative S1 sets **`T_MEM = 0`** — no additive FIR-span
widening of `T_LEAD` / `T_TRAIL` beyond the GD folding already in
`source_time`. A later non-zero `T_MEM` is **not** a silent CC widening of
this proposal: it requires a **fresh untainted amend** (new writer +
firewall) before any guard formula change. Forbidden: fitting `T_MEM` to a
measured Δ; forbidden: shopping additive `T_MEM` under a CC-open surface.

### 4.3 Worked geometry (from definitions, not from FAIL)

```text
T_LEAD  = 9216 / 48000 = 0.192 s
T_TRAIL = 1024 / 48000 ≈ 0.021333 s
closing t_cross ∈ [0.692, 1.728666…] s
f(t_0+T_LEAD) ≈ 57.79 Hz
f(t_1−T_TRAIL) ≈ 17776 Hz
```

A priori partition of the **frozen** checkpoint list under D1:

| Checkpoint (Hz) | `t_cross` (analytic) | Closing role under D1 |
|----------------:|----------------------|------------------------|
| 20 | \(t_0\) | **REPORT** (lead guard) |
| 45 | \(< t_0+T_{\mathrm{LEAD}}\) | **REPORT** (lead guard) |
| 60 | \(> t_0+T_{\mathrm{LEAD}}\) | **CLOSE-eligible** |
| 80 … 16000 | interior | **CLOSE-eligible** |
| 20000 | \(t_1\) | **REPORT** (trail guard) |

This table is a **definitional consequence** of the guard formulas + analytic
law + frozen checkpoint list. It is not a post-hoc mask of a measured band.

### 4.4 S1 REJECT rows

| ID | Proposal | Verdict |
|----|----------|---------|
| S1-R1 | Clamp frame selection into the chirp (ignore nearest/`source_time`) | **REJECT** — mandate forbidden |
| S1-R2 | Delete 20 / 20k (or any) checkpoints from the hashed grid to pass | **REJECT** — checkpoint removal |
| S1-R3 | Opportunistic second-nearest frame when first is awkward | **REJECT** |
| S1-R4 | Manual time shift / proxy timestamps | **REJECT** |
| S1-R5 | Widen/narrow guards after seeing a Δ table | **REJECT** — threshold/domain shopping |
| S1-R6 | Treat lead/trail REPORT checkpoints as if they never existed | **REJECT** — must remain observable in report |
| S1-R7 | Treat missing CHK_REPORT fields as soft / ignore for TRANSPORT PASS | **REJECT** — see §4.5 |
| S1-R8 | Silent `T_MEM>0` widening under CC without fresh untainted amend | **REJECT** — `T_MEM=0` locked |

### 4.5 CHK_REPORT publish schema (fail-closed)

For **every** `f` with `CHK_REPORT(f)`, each gate SR path **must** publish at
least:

| Field | Requirement |
|-------|-------------|
| `checkpoint_hz` | frozen grid value |
| `role` | `REPORT` |
| `t_cross` | analytic crossing time |
| `source_time_selected` | nearest useful-segment match, or explicit missing |
| `match_distance_s` | \|selected − t_cross\|; or missing marker |
| `match_ok` | boolean; `false` if outside hop radius |
| `b_star`, `f_star`, `t_star` | analytic ridge identity at selected frame (same law as ATRL) when `match_ok` |
| `L` = `mid_psd_db[b★]` | when `match_ok` and mid valid; else missing marker |
| `mid_valid` | boolean from frozen instrument |

**Fail-closed:** any missing / null required field on any CHK_REPORT cell →
**FAIL**. TRANSPORT (sweep branch) **≠ PASS** if any CHK_REPORT publication is
absent or incomplete — report presence is a **hard precondition**, not a soft
diagnostic. CHK_REPORT still does **not** enter the ATRL max; it blocks PASS
when absent.

### 4.6 NOTE — LF-per-checkpoint aperture (deferred)

A deeper per-checkpoint LF/MAIN aperture redesign (finer than the single
worst-case `T_WIN_LF` lead guard) is **out of scope** for this POROUS closure.
Tracked as NOTE only; any change requires a fresh untainted amend + CC. Not a
shopping surface for the current guard constants.

---

## 5. S2 — Non-stationary observable

### 5.1 Physical ask

On S1 close-eligible checkpoints, assert a **sweep-specific** SR-parity
property derived from:

- analytic chirp trajectory \(f(t)\),
- frozen Hann + FFT lengths + fusion,
- frozen causality / timestamps,

**without** asking equality of the full 120-band windowed spectrum of a moving
tone (ill-posed joint property; mandate pin).

### 5.2 Direction family (mandate) — closed on D11 package

Ridge / trajectory formulations are the **allowed direction family**. Under
**D11**, that family is **closed** on the single locked package
**ATRL + §5.3 companions + §6.2 veto (+ §5.3 side-gate hardness)**. There is
**no** open menu: a different form does **not** “win at CC.” Package swap
only via **fresh untainted amend + CC** — not a silent reopen of S2-ALT\* or
OR-shopping against ATRL.

### 5.3 Locked package — ATRL (analytic-trajectory ridge level)

**Name:** ATRL  
**Claim:** at each close-eligible checkpoint, the two SR paths agree (within
0.25 dB, max aggregator) on the **fused mid PSD level of the unique triangular
band that contains the analytic instantaneous frequency at the selected
frame’s `source_time`.**

#### Procedure (executable prose; not implemented here)

For each gate SR path \(\in\{44100,48000,96000\}\) and each
\(f_{\mathrm{chk}}\) with `CHK_CLOSE(f_chk)`:

1. \(t_\times \leftarrow \texttt{sweep_crossing_time}(f_{\mathrm{chk}})\) (frozen law).
2. Select frame by frozen rule: nearest useful-segment `source_time` within
   `H/fs_c`; else FAIL (unchanged).
3. Let \(t^\star\) = that frame’s `source_time`.
4. Let \(f^\star = f(t^\star)\) from the same analytic law (not a spectral peak
   pick; **not** argmax of measured PSD).
5. Let \(b^\star\) = the unique band \(b\in\{0,\ldots,119\}\) whose triangular
   support on `log2(f)` contains \(f^\star\) (§6.1).  
   - If \(f^\star\) lies exactly on a shared edge: pin
     **lower-index band** (fail-closed deterministic tie-break).  
   - If \(f^\star\) outside \([20,20000]\): FAIL (should not occur inside
     close-eligible set).
6. Observable: \(L = \texttt{mid_psd_db}[b^\star]\) after frozen fusion/floor/clamp
   — **one scalar per path** at that checkpoint (not a multi-band / multi-channel
   Cartesian product).
7. **Validity / match (fail-closed):** if frame unmatched within hop, or
   `mid_valid=false` on either path for \(b^\star\), or \(L\) is NaN/Inf/missing
   → **FAIL** (no silent skip of that cell).
8. For each SR-vs-ref pair (ref = 48 kHz path), the ATRL closing contribution is
   the single scalar \(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) at that
   `(CHK_CLOSE checkpoint × SR-vs-ref pair)`.

**Activity / floor primitives (frozen REV6 — reused, not retuned):**

```text
FLOOR_DB     = −120          # §6.1 clamp / lock ACTIVITY_FLOOR_DB
ACTIVE(b)    ⇔  max(mid_psd_db_ref[b], mid_psd_db_sr[b]) > FLOOR_DB
AT_FLOOR(x)  ⇔  x is the stored post-clamp float32 and x == FLOOR_DB
ONE_SIDED(b) ⇔  AT_FLOOR(mid_psd_db_ref[b]) XOR AT_FLOOR(mid_psd_db_sr[b])
```

**Empty-union / required activity (fail-closed, menu-locked):** with veto +
companions in the frozen closing menu, any **required** activity domain that
evaluates empty → **FAIL**. Empty union is never PASS-by-absence.

```text
# Per close cell c ∈ D (interior CHK_CLOSE under D1 ⇒ b★ ∈ {1,…,118}):
REQUIRED_ACTIVE(c)  ⇔  ACTIVE(b★) ∧ ACTIVE(b−) ∧ ACTIVE(b+)
# ¬REQUIRED_ACTIVE(c) → FAIL (no silent neighbour / ridge skip)

# Off-ridge activity-admitted set (per cell; feeds §6.2):
B_OFF_ACTIVE(c)  :=  { b ∈ B_OFF(f★) : ACTIVE(b) }
# B_OFF_ACTIVE(c) = ∅ → FAIL   (vacuous off-ridge veto / empty-union soft forbidden)
```

Polarity `ONE_SIDED(b)` is **total** on every band where veto/side-gate
reads it: missing / NaN / Inf `mid_psd_db` → **FAIL** (not `false`).

**ATRL companions (mandatory AND — not OR-shopping; no custom median):**
ATRL level alone does **not** close. Companions are the **CONTRACT §7**
scalars at the analytic ridge band — not a custom off-ridge median
prominence and not a “when prominence is required” escape.

**Cell predicates** (one evaluation per `(CHK_CLOSE × SR-vs-ref)` cell):

```text
# Cell domain D := {CHK_CLOSE} × {SR-vs-ref pairs}; ref = 48 kHz
# Per-cell companions; threshold = 0.25 dB; aggregator style = ATRL/veto (∀ / max)
SHAPE_OK(c)       ⇔  |mid_shape_db_ref[b★] − mid_shape_db_sr[b★]| ≤ 0.25
PROMINENCE_OK(c)  ⇔  |mid_prominence_db_ref[b★] − mid_prominence_db_sr[b★]| ≤ 0.25

# Side pair (CONTRACT side_* at b★) — polarity total:
#   both side_valid=false → SIDE_PAIR_OK(c) = true by **mono trivial pin**
#     (mid SHAPE/PROMINENCE still AND; trivial ≠ skip of mid companions)
#   exactly one path side_valid → FAIL
#   both side_valid → same 0.25 dB max on side_shape_db[b★] and
#                     side_prominence_db[b★]
SIDE_PAIR_OK(c)   ⇔  (side polarity rule above)

COMPANIONS_OK(c)  ⇔  SHAPE_OK(c) ∧ PROMINENCE_OK(c) ∧ SIDE_PAIR_OK(c)
```

**Domain close (∀ / max — same domain D as ATRL; forbids single-cell / soft aggregator false-PASS):**

```text
# D := {CHK_CLOSE} × {SR-vs-ref pairs}   — identical to ATRL_OK domain
COMPANIONS_OK  ⇔  ∀ c ∈ D : COMPANIONS_OK(c)
               ⇔  max_{c ∈ D}  δ_companions(c)  ≤  0.25 dB
                  where δ_companions(c) folds the required companion
                  |Δ| scalars at c (shape, prominence, and side_* when
                  side pair required); missing / null any required
                  companion field at any c → FAIL (not soft skip)
aggregator = max over D   (mean / p95 / “any cell” / single-cell PASS forbidden)
```

Missing / null any required companion field → **FAIL**. Forbidden: replacing
`mid_prominence_db[b★]` with \(L[b^\star]-\mathrm{median}(L[b])\) over
off-ridge; forbidden: dropping prominence when an off-ridge set is empty.

**Side gate (AND hardness only — never a substitute for companions):**

```text
# Triangular adjacency under frozen 120-band partition (§6.1)
# Same domain D := {CHK_CLOSE} × {SR-vs-ref pairs} as ATRL_OK / COMPANIONS_OK
b− = b★ − 1
b+ = b★ + 1
# Close-eligible f★ maps to interior b★ under D1 geometry; if b★∈{0,119}
# → FAIL (no silent neighbour collapse)

SIDE_ASYM(path) = |mid_psd_db_path[b−] − mid_psd_db_path[b+]|

SIDE_GATE_FIRE(c)  ⇔  ONE_SIDED(b−) ∨ ONE_SIDED(b+)
                   ∨  |SIDE_ASYM(ref) − SIDE_ASYM(sr)| > 0.25
                   ∨  missing evaluation of any operand

SIDE_GATE_OK(c)    ⇔  SIDE_GATE_FIRE(c) = false
```

**Domain close (∀ / max — same style and domain as ATRL / veto):**

```text
SIDE_GATE_FIRE_GLOBAL  ⇔  ∃ c ∈ D : SIDE_GATE_FIRE(c)

SIDE_GATE_OK  ⇔  ∀ c ∈ D : SIDE_GATE_OK(c)
              ⇔  SIDE_GATE_FIRE_GLOBAL = false
              ⇔  ¬∃ c ∈ D : SIDE_GATE_FIRE(c)
              ⇔  max_{c ∈ D}  δ_side(c)  ≤  0.25 dB
                 where δ_side(c) = |SIDE_ASYM(ref) − SIDE_ASYM(sr)| at c
                 when asymmetry is the binding operand; ONE_SIDED or
                 missing operand at any c → FIRE / FAIL
aggregator = max over D   (single-cell OK / soft OR across cells forbidden)
```

Side gate **adds** hardness (`∧ SIDE_GATE_OK`). It does **not** replace
`COMPANIONS_OK`. OR-shopping “companions **or** side gate” is **REJECT**.
`COMPANIONS_OK` does **not** fold `VETO_OK` (veto remains a separate conjunct
in §6.3).

**ATRL level rule (component):**

```text
ATRL_OK  ⇔  max |L_ref − L_sr| over D = {CHK_CLOSE} × {SR-vs-ref pairs}
              where L = mid_psd_db[b★]   (one scalar per path per checkpoint)
            ≤  0.25 dB
         ⇔  ∀ c ∈ D : |L_ref − L_sr|(c) ≤ 0.25 dB
aggregator = max   (mean / p95 / RMSE forbidden)
```

**Pin (CC POROUS):** the ATRL / companions / side-gate max domain is **the
same** \(D=\{\mathrm{CHK\_CLOSE}\}\times\{\mathrm{SR\text{-}vs\text{-}ref}\}\);
**not** `{valid mid channels}`, **not** a Cartesian product over mid bands /
channels, and **not** a single soft cell. Exactly one \(L\) per path per
close-eligible checkpoint (\(b^\star\) only); exactly one
\(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) and one companions/side-gate cell
evaluation per cell in \(D\).

**Domain-shrink honesty (delta RT HIGH — prefer REJECT over mid-flight meter expand):**
this package closes on ridge cell + neighbours + artifact veto over
`B_OFF` activity — **not** full §13.2.4 / gate-4 shape·prominence·level over
all activity-admitted bands. Silent claim of §13.2.4 / gate-4 equivalence
is **REJECT**. Status: **not gate-4 equivalent until REV7** (fresh package
may expand off-ridge shape/prom/level into close; not done here).

Stationary assets (multitone / pseudo_noise) keep geometric `R` + existing
stationary predicates; ATRL does **not** rewrite them.

### 5.4 Why ATRL (rationale)

| Point | |
|-------|--|
| Matches the pin | Compares a **trajectory-indexed** scalar, not full-vector spectra of a moving tone. |
| A priori | Band identity from **analytic** \(f(t^\star)\) + frozen triangular supports — no peak picking, no run-fitted neighbourhood width. |
| Instrument-native | Uses the same fused `mid_psd_db` field the stationary gate already speaks. |
| Minimal surface | Does not require a second FFT definition; leaves Hann/FFT/fusion untouched. |

### 5.5 Alternatives (archival only — menu frozen)

Identifiers **S2-ALT1 / S2-ALT2 / S2-ALT3** only — **do not** use bare **A3**
(collides with retired ACTIVE A3 solution lane).

**D11 menu freeze (normative):** the **single** closing observable package is
**ATRL + §5.3 companions + §6.2 artifact veto (+ §5.3 side-gate hardness)**.
S2-ALT\* rows below are **archival / non-closing** under this proposal.
Selecting an alt, leaving the menu open, or OR-shopping ATRL vs alt → **no
SOUND / no measure authorization**. A later package swap requires a **fresh
untainted amend** + CC — not a silent menu reopen.

| Alt | Idea | Status under D11 |
|-----|------|------------------|
| **S2-ALT1 — Dechirp then stationary PSD** | Demodulate by analytic phase law inside the Hann aperture; compare residual spectrum on a preregistered band set | **Archival** — not closing |
| **S2-ALT2 — Trajectory path integral** | Integrate fused power along \(f(t)\) for \(t\) in the selected aperture, Hann-weighted in time | **Archival** — not closing |
| **S2-ALT3 — Main-lobe ridge neighbourhood** | ATRL band plus neighbours whose centres lie inside the frozen Hann main-lobe half-width of \(f^\star\) (`MAIN_LOBE_HALF_BINS = 2` from periodic Hann; not from a run) | **Archival** — not closing |

Any future alt amend must still obey: max aggregator; 0.25 dB; no post-hoc
mask; off-ridge policy (§6); no CONTRACT edit in this phase.

### 5.6 S2 REJECT rows

| ID | Proposal | Verdict |
|----|----------|---------|
| S2-R1 | Keep full-vector 120-band max\|Δ\| on chirp as the closing ask | **REJECT** — asserts the ill-posed joint property |
| S2-R2 | “Stationary-with-asterisk”: reuse stationary predicate unchanged on chirp frames | **REJECT** — contradicts mandate pin |
| S2-R3 | Choose ridge band / neighbourhood from a measured failing-band list | **REJECT** — contamination / shopping |
| S2-R4 | Replace max with mean/p95; raise 0.25 dB | **REJECT** — immutable |
| S2-R5 | Peak-pick \(b^\star\) from measured PSD argmax | **REJECT** — not analytic-trajectory; SR-dependent selection |
| S2-R6 | Silent drop of off-ridge bands from all reporting | **REJECT** — see §6 |
| S2-R7 | Silent drop of shape / prominence / side (ATRL level-only close) | **REJECT** — companions **∧** veto required |
| S2-R8 | Close on ATRL report alone without artifact-veto conjunction | **REJECT** — see §6 |
| S2-R9 | Encode artifact-veto / companions from measured FAIL dB tables | **REJECT** — contamination |
| S2-R10 | Companions **or** side gate (OR-shopping); “when prominence required” escape | **REJECT** — AND only; CONTRACT prominence always |
| S2-R11 | Custom median off-ridge prominence instead of `mid_prominence_db[b★]` | **REJECT** |
| S2-R12 | Leave S2-ALT\* menu open / close on an alt without fresh amend | **REJECT** — D11 freeze ATRL+package |
| S2-R13 | Label activity-admitted off-ridge \|Δ\|>0.25 as diagnostic-only (veto escape) | **REJECT** — see §6.2 |
| S2-R14 | Silent §13.2.4 / gate-4 equivalence of this ATRL package | **REJECT** — not gate-4 equivalent until REV7 |
| S2-R15 | Treat empty `B_OFF_ACTIVE` / missing `ACTIVE(b★∧b±)` as soft PASS | **REJECT** — FAIL; see §5.3 |

---

## 6. Off-ridge policy (fail-closed closing conjunction)

Off-ridge energy / artifacts **cannot vanish**.

### 6.1 Mandatory report (necessary, not sufficient)

For each close-eligible checkpoint frame, publish at least:

- \(b^\star\), \(f^\star\), \(t^\star\);
- `mid_psd_db[b^\star]` per SR path + `mid_valid`;
- ATRL companions / side-gate fields (§5.3);
- **off-ridge report:** max `|Δ|` (and argmax band) over activity-admitted
  \(b\in B_{\mathrm{OFF}}\) — publish obligatory; **does not** escape
  `OFF_RIDGE_DELTA_FIRE` when `|Δ| > 0.25` on any admitted cell (§6.2).

**Missing any required off-ridge / companion / veto field → FAIL.**

### 6.2 Artifact veto (normative closing conjunct — executable)

**Artifact veto** is **not** an optional alternate and **not** example-only.
It is a **required closing conjunct**. Formula uses only frozen floor,
triangular partition, and (for non-emptiness of off-ridge obligation) Hann
ENBW / main-lobe geometry constants — **not** measured FAIL dB / band lists.

**A priori geometry sets (definitions only):**

```text
# Frozen triangular partition (§6.1): unique b★ contains f★
B_OFF(f★)  :=  { b ∈ {0,…,119} : b ≠ b★ }

# Classical periodic Hann (structure only; not run-fitted):
ENBW_HANN_BINS          = 1.5
MAIN_LOBE_HALF_BINS     = 2          # centre → first null
# Off-ridge obligation is non-vacuous: |B_OFF| = 119 ≥ ceil(ENBW) and
# triangular supports exist outside b★. These constants justify the set;
# they do **not** shrink the veto domain below B_OFF.

B_GEOM(f★) := B_OFF(f★)             # full off-ridge index set
```

**Executable veto (per CHK_CLOSE × SR-vs-ref cell; then OR over cells for fire):**

```text
OFF_RIDGE_DELTA_FIRE  ⇔
    ∃ b ∈ B_OFF(f★) :
        ACTIVE(b)
        ∧  |mid_psd_db_ref[b] − mid_psd_db_sr[b]| > 0.25

SUPPORT_POLARITY_MISMATCH  ⇔
    ∃ b ∈ B_GEOM(f★) : ONE_SIDED(b)

ARTIFACT_VETO_FIRE  ⇔
    OFF_RIDGE_DELTA_FIRE
    ∨  SUPPORT_POLARITY_MISMATCH
    ∨  missing / NaN / Inf evaluation of any operand above

VETO_OK  ⇔  ARTIFACT_VETO_FIRE = false
```

**Fail-closed pins:**

- Missing veto fields → **FAIL** (never-fire / unevaluated veto **forbidden**).
- Activity-admitted off-ridge with `|Δ| > 0.25` **cannot** be relabeled
  diagnostic-only to escape FIRE — report may still publish argmax, but the
  predicate above is closing.
- `ONE_SIDED` polarity is **total** on `B_GEOM` (missing L → FIRE via missing
  branch / FAIL).
- **Empty `B_OFF_ACTIVE` ⇒ FAIL** (per cell; see §5.3). Vacuous off-ridge
  (no activity-admitted band outside \(b^\star\)) is **not** VETO_OK by
  absence and does **not** skip polarity evaluation.
- **Forbidden:** fitting veto thresholds, band lists, or `B_GEOM` shrinks
  from measured FAIL dB / margin tables / b106 / 6.435.
- **Forbidden:** treating this veto+ATRL package as silent §13.2.4 / gate-4
  equivalence (S2-R14).

### 6.3 Sweep TRANSPORT close (normative)

```text
SWEEP_TRANSPORT_CLOSE  ⇔
    N_close ≥ 7
    ∧  ATRL_OK                         # §5.3 level ≤ 0.25 dB max over D
    ∧  COMPANIONS_OK                   # §5.3 ∀c∈D / max over D
    ∧  VETO_OK                         # §6.2 artifact-veto false
    ∧  SIDE_GATE_OK                    # §5.3 ∀c∈D; FIRE_GLOBAL=false
    ∧  CHK_REPORT_PUBLISH_COMPLETE     # §4.5
    ∧  ∀c∈D : REQUIRED_ACTIVE(c)       # ACTIVE(b★)∧ACTIVE(b±); §5.3
    ∧  ∀c∈D : B_OFF_ACTIVE(c) ≠ ∅      # empty off-ridge activity → FAIL
    ∧  no mid_valid=false / unmatched / empty-union-on-required FAIL (§5.3)
```

**Report alone does not close.** ATRL≤0.25 alone does not close. Companions
**and** veto **and** side-gate hardness all required. Any missing conjunct
field → **FAIL**. This close is **not** §13.2.4 / gate-4 equivalent
(S2-R14; not until REV7).

---

## 7. Immutable (reaffirmed)

| Item | Rule |
|------|------|
| Aggregator | **max** |
| Threshold | **0.25 dB** hard |
| Stationary domain | geometric **`R`** closes stationary cells |
| ∉`R` | **report-only** (does not close) |
| Post-hoc mask | **forbidden** |
| Threshold shopping | **forbidden** |
| `T_MEM` | **0** locked (GD already in `source_time`) |
| \|CHK_CLOSE\| | **≥7**; empty → FAIL |
| Off-ridge close | ATRL_OK **∧** COMPANIONS_OK **∧** VETO_OK **∧** SIDE_GATE_OK; report≠close |
| A3 / A4 ACTIVE | **closed** — A3 retired-as-solution; no A4 |
| Menu freeze | **ATRL + companions + veto (+ side-gate hardness)**; S2-ALT\* archival |

---

## 8. Explicit non-claims (this phase)

- **No** edit to `docs/MOTORE_V3_G1_CONTRACT.md`, metrology lock, SHA256SUMS,
  T6 fixtures/generators, or `Source/`.
- **No** measure / re-measure.
- **No** ACCEPT, PASS, G1 PASS, or official G1b tip.
- **No** REV7 consolidate in this document.
- **No** self-stamp of SOUND / CONTRACT-SOUND.

---

## 9. REV7 packaging note (future — do not consolidate now)

After S1+S2 survive redteam + independent CC, **one** REV7 package may bind
together:

- geometric `R` + report-only ∉`R`,
- S1 guard / `CHK_CLOSE` rule,
- S2 observable package (ATRL + companions + veto; menu already frozen here),
- off-ridge policy,
- fixture / T6 consequences,
- lock / hash update,

then: one fresh measure → Guardian GO → consolidate → rehash → official G1b tip.

Partial consolidate (LF-only, stationary-only, S1-only, S2-only) remains
**NO**.

---

## 10. Verdict language (prefer NO-GO on product claims)

| Claim | Status |
|-------|--------|
| This proposal = product ACCEPT / gate PASS | **NO** |
| This proposal = CONTRACT-SOUND by self-declaration | **NO** — re-CC required after this closure |
| S1 guard direction | **PROPOSED** (D1) + cardinality / REPORT fail-closed — **D1 PROPOSED ⇒ no SOUND / no measure** |
| S2 package | **LOCKED menu** — ATRL + CONTRACT companions + executable veto (+ side-gate AND); S2-ALT\* archival |
| Off-ridge vanishing | **NO** — close = ATRL_OK ∧ COMPANIONS_OK ∧ VETO_OK ∧ SIDE_GATE_OK (+ REQUIRED_ACTIVE ∧ B_OFF_ACTIVE≠∅) |
| §13.2.4 / gate-4 equivalence | **NO** — not gate-4 equivalent until REV7 (S2-R14) |
| Self-SOUND / MEASURE | **NO** — not self-SOUND; **MEASURE_AUTHORIZED NO** |
| Next lab action | re-CC on executable pins → only then measure auth (still NO until non-POROUS CC) |

---

## 11. Closed POROUS findings (this revision)

| ID | Source | Finding | Closure in this doc |
|----|--------|---------|---------------------|
| P1 | CC | ATRL closing max Cartesian `{valid mid channels}` | **CLOSED** — one scalar \|L_ref−L_sr\| per (CHK_CLOSE × SR-vs-ref); L=mid_psd_db[b★] |
| P2 | CC | Additive T_MEM CC-open shopping | **CLOSED** — normative `T_MEM=0`; non-zero needs fresh untainted amend |
| P3 | CC | S2 alt id “A3” collides retired ACTIVE A3 | **CLOSED** — renamed **S2-ALT1/2/3** |
| P4 | RT CRITICAL | Off-ridge report-alone / optional / example-only veto | **CLOSED** — executable `ARTIFACT_VETO_FIRE` §6.2; report≠close; missing→FAIL; no diagnostic escape |
| P5 | RT / re-CC CRITICAL | Companions vs side-gate OR-shopping; custom median; “when required” escape; soft/single-cell aggregator | **CLOSED** — COMPANIONS_OK = ∀c∈D CONTRACT `mid_shape_db`/`mid_prominence_db` (+ side pair), max over D; SIDE_GATE_OK = ∀c∈D ¬FIRE, max over D; **VETO_OK separate** (§6.2/§6.3), not folded into COMPANIONS_OK |
| P6 | RT HIGH | CHK_REPORT soft / TRANSPORT PASS without REPORT | **CLOSED** — §4.5 publish schema; missing→FAIL; TRANSPORT≠PASS if REPORT absent |
| P7 | RT HIGH | mid_valid=false / unmatched / empty activity skipped | **CLOSED** — FAIL; required empty-union→FAIL; polarity total |
| P8 | RT HIGH | \|CHK_CLOSE\| vacuous / under-populated; T_MEM drift | **CLOSED** — \|CHK_CLOSE\|<7 or ∅ → FAIL; `T_MEM=0` locked |
| P9 | RT / re-CC CRITICAL | Open S2/alt menu | **CLOSED** — D11 freeze ATRL+package; S2-ALT\* archival |
| P10 | RT MED | Deep LF-per-checkpoint aperture | **NOTE only** — §4.6 deferred; not redesigned here |
| P11 | re-CC + delta RT | Never-fire / unevaluated veto | **CLOSED** — missing operands → FIRE/FAIL; formula pinned §6.2 |
| P12 | delta RT `8aa8027c` HIGH | Empty-union soft; missing neighbour activity; empty B_OFF_ACTIVE | **CLOSED** — `REQUIRED_ACTIVE ⇔ ACTIVE(b★)∧ACTIVE(b±)`; `B_OFF_ACTIVE=∅ → FAIL`; both enter §6.3 close |
| P13 | re-CC residual / delta CRITICAL | COMPANIONS_OK / SIDE_GATE_OK soft/single-cell aggregator | **CLOSED** — ∀c∈D / max over same `{CHK_CLOSE}×{SR-vs-ref}` as ATRL; `SIDE_GATE_FIRE_GLOBAL ⇔ ∃c FIRE` |
| P14 | re-CC residual | §5.2 “another form may win at CC” open-menu echo vs D11 | **CLOSED** — §5.2 closed on ATRL+package; swap only via fresh untainted amend + CC |
| P15 | delta RT `8aa8027c` HIGH | Silent §13.2.4 / gate-4 equivalence via domain shrink | **CLOSED** — S2-R14 REJECT; **not gate-4 equivalent until REV7** (no mid-flight meter expand) |
| P16 | delta RT MED | Mono `SIDE_PAIR` both `side_valid=false` ambiguous | **CLOSED** — mono trivial pin (`SIDE_PAIR_OK=true`); mid companions still AND |
| P17 | delta RT MED | D1 PROPOSED readable as SOUND/measure-ready | **CLOSED** — D1 PROPOSED ⇒ no SOUND / no measure (§3, §10) |

---

## 12. Handoff

| Lane | Ask |
|------|-----|
| Independent CC / guardian | **Re-CC** executable pins (§5.2↔D11, §5.3 ∀/max companions+side-gate, §6.2); no SOUND / no measure without fresh CC |
| `ember-metrology-redteam` | Delta-attack ∀/max companions+side-gate + §5.2 freeze; leftover NOTES only unless new CRITICAL |
| `ember-phase-builder` | Idle on code until GO post-CC |
| `ember-parity-lab` | No measure until authorized |

**§A (one-line):** Companions/side-gate ∀c∈D (max) + FIRE_GLOBAL; §5.2↔D11; empty B_OFF_ACTIVE FAIL; ¬§13.2.4-equiv until REV7 — still not self-SOUND; ready for re-CC; **MEASURE_AUTHORIZED NO**.

**END PROPOSAL — not self-SOUND.**
