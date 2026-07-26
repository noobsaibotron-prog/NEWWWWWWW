# Motore v3 — Proposal: SWEEP METROLOGY REDESIGN (S1 + S2)

**Suggested commit title:**  
`docs(v3): close SWEEP redesign proposal full POROUS (CC+RT)`

| Field | Value |
|-------|--------|
| **Status** | **PROPOSAL DRAFT** — document-only; **not** self-SOUND; CC + redteam POROUS closure applied; ready for **re-CC**; **MEASURE_AUTHORIZED NO** |
| **≠** | G1 PASS · ACCEPT · measure · REV7 consolidate · G1b tip · CONTRACT/lock/T6 edit · A3 reopen · A4 ACTIVE |
| **Date** | 2026-07-26 |
| **Authority** | Marco **"ok"** on mandate open + this untainted proposal write |
| **Mandate** | `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` (incl. stationary≠trajectory pin) |
| **A3 status (status only)** | **RETIRED AS SOLUTION** — `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| **Freeze structure (read-only)** | `docs/MOTORE_V3_G1_CONTRACT.md` REV6 @ `6d254d0a`; lock / `fixture_spec` **structure** as frozen quantities |

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
  indices from prior runs;
- any remeasure JSON/output;
- Claude numeric conclusions as design inputs.

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
| D1 | S1 = **guard regions** on checkpoint closing domain, from aperture geometry only | **PROPOSED** |
| D2 | S2 candidate = **analytic-trajectory ridge level (ATRL)** + mandatory companions / side gate | **PROPOSED** (alts **S2-ALT1/2/3** CC-open; not “A3”) |
| D3 | Off-ridge **closing conjunction:** ATRL≤0.25 **AND** artifact-veto=false; report alone **does not** close | **LOCKED (fail-closed)** |
| D4 | Immutable: max aggregator; 0.25 dB; stationary `R`; report-only ∉`R`; no post-hoc mask; no threshold shopping | **LOCKED by mandate** |
| D5 | No CONTRACT / lock / T6 / SHA256SUMS / `Source/` edit in this phase | **LOCKED** |
| D6 | No measure; no ACCEPT / PASS / G1b tip claim | **LOCKED** |
| D7 | REV7 = **one** future package after S1+S2 CC — not consolidated here | **LOCKED** |
| D8 | \|CHK_CLOSE\| floor ≥7; empty set → FAIL; `T_MEM=0` locked | **LOCKED (fail-closed)** |
| D9 | `mid_valid=false` / unmatched / empty activity-union → FAIL | **LOCKED (fail-closed)** |
| D10 | CHK_REPORT publish obligatory; TRANSPORT≠PASS if REPORT absent | **LOCKED (fail-closed)** |
| D11 | S2/alt **menu freeze** before any measure authorization | **LOCKED** |

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

### 5.2 Direction family (mandate)

Ridge / trajectory formulations are an **allowed direction family**, not a
rubber-stamp of a pre-chosen metric. Another physics-derived form may win at
CC if better justified from the same frozen instrument.

### 5.3 Concrete candidate — ATRL (analytic-trajectory ridge level)

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

**Activity union (fail-closed):** off-ridge / companion band sets use the
**frozen** floor/union activity predicate (REV6 structure). If the
activity-admitted set for a required comparison is **empty** when the
predicate demands a non-empty admission (or both paths inactive when veto
expects a defined polarity) → **FAIL**. Empty union is never treated as
PASS-by-absence.

**ATRL companions / side gate (mandatory — no silent drop):** ATRL level
alone does **not** exhaust shape/prominence/side structure. Normative closing
requires **either**:

- **Companions (default menu):** preregistered scalar companions published and
  gated with the same max / 0.25 dB / fail-closed missing rules, derived a
  priori from frozen triangular geometry + floor (not from FAIL dB tables):
  - **prominence:** \(L[b^\star] - \mathrm{median}(L[b])\) over activity-admitted
    off-ridge bands (or FAIL if that set empty when prominence is required);
  - **side asymmetry:** \(|L[b^\star_{-}]-L[b^\star_{+}]|\) for the two
    nearest activity-admitted neighbour bands when they exist; if fewer than
    two neighbours exist under frozen geometry at that \(f^\star\), publish
    explicit `side_n/a` and route through the **side gate** below rather than
    dropping the cell;
- **or** an explicit **FAIL-closed side gate** (boolean): a preregistered
  one-sided shape/side predicate from floor + triangular geometry that **must**
  evaluate false for close; missing evaluation → FAIL.

Silent drop of prominence / side / shape is **REJECT** (S2-R7). Companion
thresholds remain 0.25 dB max where numeric; boolean side gate is separate
and must not encode measured FAIL dB.

**ATRL level rule (component):**

```text
ATRL_OK  ⇔  max |L_ref − L_sr| over {CHK_CLOSE} × {SR-vs-ref pairs}
              where L = mid_psd_db[b★]   (one scalar per path per checkpoint)
            ≤  0.25 dB
aggregator = max   (mean / p95 / RMSE forbidden)
```

**Pin (CC POROUS):** the ATRL max domain is **not** `{valid mid channels}` and
**not** a Cartesian product over mid bands / channels. Exactly one \(L\) per
path per close-eligible checkpoint (\(b^\star\) only); exactly one
\(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) per `(CHK_CLOSE × SR-vs-ref)` cell.

Stationary assets (multitone / pseudo_noise) keep geometric `R` + existing
stationary predicates; ATRL does **not** rewrite them.

### 5.4 Why ATRL (rationale)

| Point | |
|-------|--|
| Matches the pin | Compares a **trajectory-indexed** scalar, not full-vector spectra of a moving tone. |
| A priori | Band identity from **analytic** \(f(t^\star)\) + frozen triangular supports — no peak picking, no run-fitted neighbourhood width. |
| Instrument-native | Uses the same fused `mid_psd_db` field the stationary gate already speaks. |
| Minimal surface | Does not require a second FFT definition; leaves Hann/FFT/fusion untouched. |

### 5.5 Alternatives (CC-open; same physics budget)

Identifiers **S2-ALT1 / S2-ALT2 / S2-ALT3** only — **do not** use bare **A3**
(collides with retired ACTIVE A3 solution lane).

| Alt | Idea | When it might beat ATRL |
|-----|------|-------------------------|
| **S2-ALT1 — Dechirp then stationary PSD** | Demodulate by analytic phase law inside the Hann aperture; compare residual spectrum on a preregistered band set | If redteam shows single-band ATRL under-detects SR path differences distributed along the trajectory through the window |
| **S2-ALT2 — Trajectory path integral** | Integrate fused power along \(f(t)\) for \(t\) in the selected aperture, Hann-weighted in time | If a single timestamp \(t^\star\) is judged too thin vs the joint window motion |
| **S2-ALT3 — Main-lobe ridge neighbourhood** | ATRL band plus neighbours whose centres lie inside the frozen Hann main-lobe half-width of \(f^\star\) (width from §6 geometry, not from a run) | If single-band triangular support is thinner than the instrument’s spectral resolution at that \(f^\star\) |

Any alt must still obey: max aggregator; 0.25 dB; no post-hoc mask; off-ridge
policy (§6); no CONTRACT edit in this phase.

### 5.6 S2 REJECT rows

| ID | Proposal | Verdict |
|----|----------|---------|
| S2-R1 | Keep full-vector 120-band max\|Δ\| on chirp as the closing ask | **REJECT** — asserts the ill-posed joint property |
| S2-R2 | “Stationary-with-asterisk”: reuse stationary predicate unchanged on chirp frames | **REJECT** — contradicts mandate pin |
| S2-R3 | Choose ridge band / neighbourhood from a measured failing-band list | **REJECT** — contamination / shopping |
| S2-R4 | Replace max with mean/p95; raise 0.25 dB | **REJECT** — immutable |
| S2-R5 | Peak-pick \(b^\star\) from measured PSD argmax | **REJECT** — not analytic-trajectory; SR-dependent selection |
| S2-R6 | Silent drop of off-ridge bands from all reporting | **REJECT** — see §6 |
| S2-R7 | Silent drop of shape / prominence / side (ATRL level-only close) | **REJECT** — companions or side gate required |
| S2-R8 | Close on ATRL report alone without artifact-veto conjunction | **REJECT** — see §6 |
| S2-R9 | Encode artifact-veto / companions from measured FAIL dB tables | **REJECT** — contamination |

---

## 6. Off-ridge policy (fail-closed closing conjunction)

Off-ridge energy / artifacts **cannot vanish**.

### 6.1 Mandatory report (necessary, not sufficient)

For each close-eligible checkpoint frame, publish at least:

- \(b^\star\), \(f^\star\), \(t^\star\);
- `mid_psd_db[b^\star]` per SR path + `mid_valid`;
- ATRL companions / side-gate fields (§5.3);
- **off-ridge report:** max `|Δ|` (and argmax band) over
  \(\{b : b\neq b^\star\}\) that are activity-admitted under the **frozen**
  floor/union predicate (REV6 structure), labeled **diagnostic** when not
  part of the veto predicate.

**Missing any required off-ridge / companion / veto field → FAIL.**

### 6.2 Artifact veto (normative closing conjunct)

**Artifact veto** is **not** an optional alternate. It is a **required closing
conjunct**, written a priori from **floor + triangular geometry** only
(examples of admissible form: one path activity-admitted far from \(b^\star\)
while the other is at floor; polarity/support mismatch under frozen union).
**Forbidden:** fitting veto thresholds or band lists from measured FAIL dB /
margin tables.

```text
ARTIFACT_VETO_FIRE  ∈ {true, false}   # missing evaluation → FAIL
VETO_OK             ⇔  ARTIFACT_VETO_FIRE = false
```

### 6.3 Sweep TRANSPORT close (normative)

```text
SWEEP_TRANSPORT_CLOSE  ⇔
    N_close ≥ 7
    ∧  ATRL_OK                         # §5.3 level ≤ 0.25 dB max
    ∧  COMPANIONS_OR_SIDE_GATE_OK      # §5.3 — no silent drop
    ∧  VETO_OK                         # artifact-veto false
    ∧  CHK_REPORT_PUBLISH_COMPLETE     # §4.5
    ∧  no mid_valid=false / unmatched / empty-union FAIL (§5.3)
```

**Report alone does not close.** ATRL≤0.25 alone does not close. Veto must
evaluate and be false. Any missing conjunct field → **FAIL**.

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
| Off-ridge close | ATRL_OK **∧** VETO_OK **∧** companions/side gate; report≠close |
| A3 / A4 ACTIVE | **closed** — A3 retired-as-solution; no A4 |
| Menu freeze | S2 primary + **S2-ALT\*** menu **frozen before measure** |

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
- S2 observable (ATRL or CC-selected alt),
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
| S1 guard direction | **PROPOSED** (D1) + cardinality / REPORT fail-closed |
| S2 ATRL candidate | **PROPOSED** (D2) + companions/side gate; alts **S2-ALT1/2/3** |
| Off-ridge vanishing | **NO** — close = ATRL_OK ∧ VETO_OK ∧ companions/side |
| Self-SOUND / MEASURE | **NO** — not self-SOUND; **MEASURE_AUTHORIZED NO** |
| Next lab action | re-CC on full POROUS closure → menu freeze → only then measure auth |

---

## 11. Closed POROUS findings (this revision)

| ID | Source | Finding | Closure in this doc |
|----|--------|---------|---------------------|
| P1 | CC | ATRL closing max Cartesian `{valid mid channels}` | **CLOSED** — one scalar \|L_ref−L_sr\| per (CHK_CLOSE × SR-vs-ref); L=mid_psd_db[b★] |
| P2 | CC | Additive T_MEM CC-open shopping | **CLOSED** — normative `T_MEM=0`; non-zero needs fresh untainted amend |
| P3 | CC | S2 alt id “A3” collides retired ACTIVE A3 | **CLOSED** — renamed **S2-ALT1/2/3** |
| P4 | RT CRITICAL | Off-ridge report-alone / optional veto | **CLOSED** — close iff ATRL_OK ∧ VETO_OK; report≠close; missing→FAIL; veto a priori floor+geometry |
| P5 | RT CRITICAL | Silent drop shape/prominence/side | **CLOSED** — companions **or** FAIL-closed side gate mandatory |
| P6 | RT HIGH | CHK_REPORT soft / TRANSPORT PASS without REPORT | **CLOSED** — §4.5 publish schema; missing→FAIL; TRANSPORT≠PASS if REPORT absent |
| P7 | RT HIGH | mid_valid=false / unmatched / empty activity skipped | **CLOSED** — FAIL; activity union; empty set→FAIL |
| P8 | RT HIGH | \|CHK_CLOSE\| vacuous / under-populated; T_MEM drift | **CLOSED** — \|CHK_CLOSE\|<7 or ∅ → FAIL; `T_MEM=0` locked |
| P9 | RT MED | REPORT schema / menu freeze | **CLOSED** — §4.5 fields; D11 menu freeze before measure |
| P10 | RT MED | Deep LF-per-checkpoint aperture | **NOTE only** — §4.6 deferred; not redesigned here |

---

## 12. Handoff

| Lane | Ask |
|------|-----|
| Independent CC / guardian | **Re-CC** full POROUS closure table (§11); no SOUND / no measure without fresh CC |
| `ember-metrology-redteam` | Attack remaining false-PASS surface on conjunction + companions; do not wait on parallel lanes |
| `ember-phase-builder` | Idle on code until GO post-CC |
| `ember-parity-lab` | No measure until authorized + menu freeze |

**§A (one-line):** Full POROUS closure (CC+RT) in this doc only — still not self-SOUND; ready for re-CC; **MEASURE_AUTHORIZED NO**.

**END PROPOSAL — not self-SOUND.**
