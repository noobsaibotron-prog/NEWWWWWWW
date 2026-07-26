# Motore v3 — Proposal: SWEEP METROLOGY REDESIGN (S1 + S2)

**Suggested commit title:**  
`docs(v3): untainted SWEEP_METROLOGY_REDESIGN proposal (S1+S2)`

| Field | Value |
|-------|--------|
| **Status** | **PROPOSAL DRAFT** — document-only; **not** self-SOUND; POROUS must-fixes applied; ready for **re-CC / redteam**; **MEASURE_AUTHORIZED NO** |
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

**Self-SOUND is forbidden.** This document marks itself ready for redteam/CC
only; it does **not** claim CONTRACT-SOUND, PASS, or ACCEPT.

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
| D2 | S2 candidate = **analytic-trajectory ridge level (ATRL)** | **PROPOSED** (direction family = ridge/trajectory; alts **S2-ALT1/2/3** CC-open) |
| D3 | Off-ridge = **mandatory preregistered report** (default) with **artifact-veto** as CC-open alternate | **PROPOSED** |
| D4 | Immutable: max aggregator; 0.25 dB; stationary `R`; report-only ∉`R`; no post-hoc mask; no threshold shopping | **LOCKED by mandate** |
| D5 | No CONTRACT / lock / T6 / SHA256SUMS / `Source/` edit in this phase | **LOCKED** |
| D6 | No measure; no ACCEPT / PASS / G1b tip claim | **LOCKED** |
| D7 | REV7 = **one** future package after S1+S2 CC — not consolidated here | **LOCKED** |

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
- `CHK_REPORT` checkpoints remain **obligatory to publish** (presence +
  nearest-frame distance + observables) but **do not enter** the gate-closing
  max.
- Missing nearest frame within hop for any checkpoint (close or report) remains
  **FAIL** under the frozen match rule — guards do not invent frames.

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
7. For each SR-vs-ref pair (ref = 48 kHz path), the closing contribution is the
   single scalar \(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) at that
   `(CHK_CLOSE checkpoint × SR-vs-ref pair)`.

**Closing rule (sweep branch only):**

```text
max |L_ref − L_sr| over {CHK_CLOSE checkpoints} × {SR-vs-ref pairs}
  where L = mid_psd_db[b★]   (one scalar per path per checkpoint)
  ≤  0.25 dB
aggregator = max   (mean / p95 / RMSE forbidden)
```

**Pin (closes POROUS leftover):** the closing max domain is **not**
`{valid mid channels}` and **not** a Cartesian product over mid bands /
channels. Exactly one \(L\) per path per close-eligible checkpoint
(\(b^\star\) only); exactly one \(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) per
`(CHK_CLOSE × SR-vs-ref)` cell.

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

---

## 6. Off-ridge policy

Off-ridge energy / artifacts **cannot vanish**.

**Default proposed:** **mandatory preregistered report**

For each close-eligible checkpoint frame, publish at least:

- \(b^\star\), \(f^\star\), \(t^\star\);
- `mid_psd_db[b^\star]` per SR path;
- **off-ridge report:** max `|Δ|` (and argmax band) over
  \(\{b : b\neq b^\star\}\) that are activity-admitted under the **frozen**
  floor/union predicate (REV6 structure), clearly labeled
  **non-closing / diagnostic**.

**CC-open alternate:** **artifact veto** — FAIL-closed if a preregistered
one-sided off-ridge predicate fires (e.g. one path active far from \(b^\star\)
while the other is at floor), with the predicate written a priori from floor +
triangular geometry, not from a run list.

Choosing report vs veto (or both) is left to redteam/CC; vanishing is not an
option.

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
| A3 / A4 ACTIVE | **closed** — A3 retired-as-solution; no A4 |

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
| This proposal = CONTRACT-SOUND by self-declaration | **NO** — redteam/CC required |
| S1 guard direction | **PROPOSED** (D1) |
| S2 ATRL candidate | **PROPOSED** (D2); alts **S2-ALT1/2/3** CC-open (not “A3”) |
| Off-ridge vanishing | **NO** |
| Self-SOUND / MEASURE | **NO** — not self-SOUND; **MEASURE_AUTHORIZED NO** until re-CC |
| Next lab action | re-CC / redteam on this must-fix revision → only then measure authorization |

---

## 11. Handoff

| Lane | Ask |
|------|-----|
| `ember-metrology-redteam` | Attack S1 vacuous PASS (empty `CHK_CLOSE`), S2 peak-pick smuggling, off-ridge vanishing, guard shopping, ATRL under-detection vs **S2-ALT\***; re-check POROUS must-fixes (ATRL max scalar pin; `T_MEM=0`; no “A3” alt id) |
| Independent CC / guardian | **Re-CC** this revision; counter-check firewall + REJECT rows; no SOUND / no measure without fresh CC |
| `ember-phase-builder` | Idle on code until GO post-CC |
| `ember-parity-lab` | No measure until authorized |

**§A (one-line):** POROUS must-fixes closed in this doc only — still not self-SOUND; ready for re-CC/redteam; **no measure**.

**END PROPOSAL — not self-SOUND.**
