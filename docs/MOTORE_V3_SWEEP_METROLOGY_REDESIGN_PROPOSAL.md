# Motore v3 — Proposal: SWEEP METROLOGY REDESIGN (S1 + S2)

**PARKED — out of G1 closing set (option C)**  
Not deleted. Not under CC/RT. No measure. Body unchanged; litigation stopped.
See `docs/MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md`. Tip at park ~`9b8f8305`.

**Suggested commit title:**  
`docs(v3): kill source_time_selected := mirror; fix RIDGE claim; P32`

| Field | Value |
|-------|--------|
| **Status** | **PARKED — out of G1 closing set (option C)** — document-only; not under evaluation / CC / RT; D1 **LOCKED**; Prior-4 (`49c9f7eb`) **claimed closed — not reopened**; residual POROUS pins frozen at park (re-CC `1577733c` CRITICAL + delta RT `68a39a49` HIGH); P28/P33–P36 + P37–P43 **OPEN/PINNED at park** (no self-CLOSED); **not** self-SOUND; **MEASURE_AUTHORIZED NO** |
| **≠** | G1 PASS · ACCEPT · measure · G1b tip · reopen as gate-4 closing · A3 reopen · A4 ACTIVE · silent §13.2.4 / gate-4 equivalence · self-SOUND · Dual-SOUND |
| **Date** | 2026-07-26 |
| **Authority** | Marco authorize docs-only residual POROUS (re-CC `1577733c` CRITICAL + delta RT `68a39a49` HIGH) on tip ~`81f10dea`; MEASURE NO; no CONTRACT/lock/Source |
| **Mandate** | `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` (incl. stationary≠trajectory pin) |
| **A3 status (status only)** | **RETIRED AS SOLUTION** — `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| **Freeze structure (read-only)** | Living: `docs/MOTORE_V3_G1_CONTRACT.md` **REV7 CONSOLIDATED** (gate-4 closing = stationary on `R`; this proposal stays PARKED `G1e-nonstat` debt). Ancestor REV6 tip `6d254d0a`. |
| **Prior tip** | `81f10dea` (pin REPORT TSTAR nearest + match_ok in COMPLETE; residual POROUS — `source_time_selected:=` vacuous TSTAR; false RIDGE claim; P32 stale EQ_PUBLISH; filter-first useful nearest; unbound tie; PCM “not pinned”) |

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
| Match radius | **absolute** nearest `source_time` within `H/fs_c` (all frames); then useful ⊆ check; tie→FAIL∨`min(frame_index)`; else FAIL | §13.1 |
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
| D1 | S1 = **guard regions** on checkpoint closing domain, from aperture geometry only | **LOCKED** — §4.2 geometry (`T_LEAD`/`T_TRAIL`, `T_MEM=0`, REPORT/CLOSE); S1 locked ≠ components self-SOUND; **MEASURE** still **NO** until dual non-POROUS stamps |
| D2 | S2 **menu components** = ATRL + CONTRACT companions + artifact veto (+ side-gate hardness) — **not** a close predicate | **LOCKED (menu freeze)** — alts **S2-ALT\*** archival only; not “A3”; sole close = **D3** |
| D3 | **⇔** `SWEEP_TRANSPORT_CLOSE` (§6.3 full conjunct — **sole** close predicate; short ATRL∧COMPANIONS∧VETO∧SIDE AND **REJECT**); report alone **does not** close | **LOCKED (fail-closed)** |
| D4 | Immutable: max aggregator; 0.25 dB; stationary `R`; report-only ∉`R`; no post-hoc mask; no threshold shopping | **LOCKED by mandate** |
| D5 | No CONTRACT / lock / T6 / SHA256SUMS / `Source/` edit in this phase | **LOCKED** |
| D6 | No measure; no ACCEPT / PASS / G1b tip claim | **LOCKED** |
| D7 | REV7 = **one** future package after S1+S2 CC — not consolidated here | **LOCKED** |
| D8 | \|CHK_CLOSE\| floor ≥7; empty set → FAIL; `T_MEM=0` locked | **LOCKED (fail-closed)** |
| D9 | `mid_valid=false` / unmatched / empty activity-union on required domains → FAIL | **LOCKED (fail-closed)** |
| D10 | `CHK_REPORT_PUBLISH_COMPLETE ⇔` §4.5 (\|REPORT_SET\|≥3 floor `{20,45,20000}` + bijection keys + `match_ok∧mid_valid` + named `TSTAR_LAW_OK`/`RIDGE_LAW_OK` on REPORT); TRANSPORT≠PASS if incomplete | **LOCKED (fail-closed)** |
| D11 | **Menu freeze (components only):** ATRL + §5.3 companions + §6.2 veto (+ §5.3 side-gate). Open menu → no SOUND / no measure. **≠** close — sole close = **D3⇔TRANSPORT** | **LOCKED** |

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

### 4.2 Locked guard (normative — D1)

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
| S1-R3 | Opportunistic second-nearest / filter-first useful∩hop when abs nearest awkward | **REJECT** — CONTRACT §13.1 absolute nearest; useful ⊆ after |
| S1-R4 | Manual time shift / proxy timestamps | **REJECT** |
| S1-R5 | Widen/narrow guards after seeing a Δ table | **REJECT** — threshold/domain shopping |
| S1-R6 | Treat lead/trail REPORT checkpoints as if they never existed | **REJECT** — must remain observable in report |
| S1-R7 | Treat missing CHK_REPORT fields as soft / ignore for TRANSPORT PASS | **REJECT** — see §4.5 |
| S1-R9 | COMPLETE = non-null only / omit ridge law / soft `¬match_ok` / vacuous or under-floor REPORT set | **REJECT** — §4.5: \|REPORT_SET\|≥3 `{20,45,20000}` + bijection + `match_ok∧mid_valid` + named TSTAR/RIDGE on REPORT |
| S1-R8 | Silent `T_MEM>0` widening under CC without fresh untainted amend | **REJECT** — `T_MEM=0` locked |

### 4.5 CHK_REPORT publish schema (fail-closed)

For **every** `f` with `CHK_REPORT(f)`, each gate SR path **must** publish at
least:

| Field | Requirement |
|-------|-------------|
| `checkpoint_hz` | frozen grid value |
| `role` | `REPORT` |
| `t_cross` | analytic crossing time |
| `source_time_selected` | **independent published** field; COMPLETE/`TSTAR_LAW_OK` **check** `source_time_selected == t★_nearest(f, path)` — **never** `source_time_selected := t★_nearest` (vacuous-by-assignment FORBIDDEN); unmatched → **FAIL** |
| `match_distance_s` | \|source_time_selected − t_cross\| |
| `match_ok` | **must be `true`** for COMPLETE; ⇔ \|source_time_selected − t_cross\| ≤ H/fs_c ∧ `source_time_selected = t_star = t★_nearest`; `false` → ¬COMPLETE → ¬TRANSPORT |
| `b_star`, `f_star`, `t_star` | **used/published** per-path ridge operands (**independent** of assignment-by-law): must satisfy §5.3 `TSTAR_LAW_OK` ∧ `RIDGE_LAW_OK` on REPORT×path — **not** presence-only; **not** `f_star:=f_chk`; **FORBIDDEN** define `f_star:=f(t_star)` then “check” |
| `L` = `mid_psd_db[b_star]` | **required** under COMPLETE (`match_ok ∧ mid_valid`); else ¬COMPLETE |
| `mid_valid` | **must be `true`** for COMPLETE; `false` → ¬COMPLETE → ¬TRANSPORT |

**Fail-closed:** any missing / null required field on any CHK_REPORT cell →
**FAIL**. TRANSPORT (sweep branch) **≠ PASS** if any CHK_REPORT publication is
absent, incomplete, or law-violating — report presence **and** ridge-law
compliance are **hard preconditions**, not soft diagnostics. CHK_REPORT still
does **not** enter the ATRL max; it blocks PASS when absent or unlawful.
`¬match_ok` / `¬mid_valid` / unmatched → ¬`CHK_REPORT_PUBLISH_COMPLETE` →
¬`SWEEP_TRANSPORT_CLOSE`.

**Normative publish-complete predicate (TRANSPORT conjunct):**

```text
REPORT_SET  :=  { f ∈ SWEEP_CHECKPOINT_HZ : CHK_REPORT(f) }   # §4.2 predicate
# Under §4.3 geometry: REPORT_SET ⊇ {20, 45, 20000}  (lead/trail guards)
GATE_SR     :=  {44100, 48000, 96000}

CHK_REPORT_REQUIRED_FIELDS  :=
    { checkpoint_hz, role, t_cross, source_time_selected, match_distance_s,
      match_ok, b_star, f_star, t_star, L, mid_valid }

# Used/published REPORT operands (ANTI-TAUTOLOGY — not := by law):
#   t_star, f_star, b_star, L  are independent published values.
#   FORBIDDEN: define f_star:=f(t_star) then check f_star=f(t_star).
#   Law checks published values against analytic f(·) / Band(·) / nearest.

# Key set (bijection — duplicate or missing key → FAIL):
REPORT_KEYS  :=  published (f, path) keys of CHK_REPORT cells
# REQUIRE: REPORT_KEYS  bijects  REPORT_SET × GATE_SR
#   |REPORT_KEYS| = |REPORT_SET| · |GATE_SR|
#   no duplicate (f, path); no extra key outside REPORT_SET × GATE_SR

# Absolute nearest oracle (CONTRACT §13.1 — NOT filter-first useful search):
t★_abs(f, path)  :=
    source_time of the frame minimizing |source_time − t_cross(f)| over ALL frames
    on that path (absolute nearest). Tie: if ≥2 frames share d_min → FAIL
    OR deterministic min(frame_index) among ties (no PASS-shopping).
# Hop + useful are post-select checks (useful ⊆ after abs nearest):
#   |t★_abs − t_cross| ≤ H/fs_c  else FAIL
#   t★_abs ∈ useful-segment      else FAIL
# FORBIDDEN: argmin over (useful-segment ∩ hop) / second-nearest when abs is awkward.

t★_nearest(f, path)  :=  t★_abs(f, path) after hop∧useful checks above
# unmatched / outside hop / abs∉useful → FAIL (no soft missing)

# Named §5.3 laws on REPORT×path (same operands as close path):
TSTAR_LAW_OK(f, path)  ⇔
    t_star(f, path) = source_time_selected(f, path)
    ∧  source_time_selected(f, path) = t★_nearest(f, path)
# check-equality only — FORBIDDEN schema/source_time_selected := t★_nearest
#   (that would make the second conjunct vacuous-by-assignment)
# unmatched / outside hop / abs∉useful → ¬TSTAR_LAW_OK → FAIL

RIDGE_LAW_OK(f, path)  ⇔
    TSTAR_LAW_OK(f, path)
    ∧  f_star(f, path) = f(t_star(f, path))     # check published vs analytic
    ∧  b_star(f, path) = Band(f_star(f, path))
# RIDGE_LAW is check-only equality of independent used/published fields vs law.
# FORBIDDEN: define f_star:=f(t_star) then “check” — assign-then-check always
#   passes RIDGE_LAW_OK algebraically; that is process/schema FAIL, not ¬RIDGE.
# f_star:=f_chk → ¬RIDGE_LAW_OK whenever f(t_star) ≠ f_chk.

CHK_REPORT_PUBLISH_COMPLETE  ⇔
    (|REPORT_SET| ≥ 3)                            # floor; ⊇ {20,45,20000} under §4.3
    ∧  ({20, 45, 20000} ⊆ REPORT_SET)             # explicit guard checkpoints
    ∧  (REPORT_KEYS  bijects  REPORT_SET × GATE_SR)
    ∧  (|REPORT_KEYS| = |REPORT_SET| · |GATE_SR|)
    ∧  ∀ f ∈ REPORT_SET, ∀ path ∈ GATE_SR :
        ∀ field ∈ CHK_REPORT_REQUIRED_FIELDS :
            field is published ∧ field ≠ null
            # key absence / JSON null → FAIL (no soft missing marker under COMPLETE)
        ∧  role = REPORT
        ∧  match_ok = true
        ∧  mid_valid = true
        ∧  match_distance_s = |source_time_selected − t_cross|
        ∧  |t_star − t_cross| ≤ H/fs_c            # hop radius (⇔ match_ok)
        ∧  TSTAR_LAW_OK(f, path)                  # named; same §5.3 operands
        ∧  RIDGE_LAW_OK(f, path)                  # named; ⇒ TSTAR; check-only
        ∧  L = mid_psd_db[b_star]
# ¬match_ok → ¬CHK_REPORT_PUBLISH_COMPLETE → ¬TRANSPORT
# empty / partial / duplicate-key emission → ¬COMPLETE → FAIL
# binding f_star:=f_chk on REPORT → FAIL whenever f(t_star)≠f_chk
# SWEEP_TRANSPORT_CLOSE ⇒ CHK_REPORT_PUBLISH_COMPLETE  (§6.3)
```

**Publish equality (fail-closed — no ATRL-used ambiguity on REPORT):**

```text
# CLOSE cells: used ATRL/TRANSPORT operands ≡ published fields.
# REPORT cells: REPORT is not ATRL-closing; equality is publish-vs-publish /
# publish-vs-law only — do NOT require "ATRL-used" operands on REPORT.

CLOSE_OPERANDS_EQ_PUBLISH  ⇔
    ∀ cell/path ∈ {CHK_CLOSE} × GATE_SR :
        used close/ATRL/TRANSPORT operands
        (t★_used, f★_used, b★_used, L, …)
        == published fields (t_star, f_star, b_star, L, …)
        under the same names/law as this schema
    ∧  ∀ f ∈ REPORT_SET, ∀ path ∈ GATE_SR :
        published (t_star, f_star, b_star, L) satisfy
        TSTAR_LAW_OK(f, path) ∧ RIDGE_LAW_OK(f, path)
        ∧ L = mid_psd_db[b_star]
        # no "ATRL-used" operand required on REPORT (REPORT ∉ ATRL max)
# mismatch on CLOSE used≠publish → FAIL
# REPORT law/publish violation → FAIL via COMPLETE / this conjunct
```

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

### 5.2 Direction family (mandate) — closed on D11 components

Ridge / trajectory formulations are the **allowed direction family**. Under
**D11**, that family is **closed** on the locked **menu components**
**ATRL + §5.3 companions + §6.2 veto (+ §5.3 side-gate hardness)**. There is
**no** open menu: a different form does **not** “win at CC.” Component swap
only via **fresh untainted amend + CC** — not a silent reopen of S2-ALT\* or
OR-shopping against ATRL. **D11 ≠ close:** the sole close predicate is
**D3 ⇔ `SWEEP_TRANSPORT_CLOSE`** (§6.3 full); a short ATRL∧COMPANIONS∧VETO∧SIDE
“package PASS” under D2/D11 is **REJECT**.

### 5.3 Locked menu component — ATRL (analytic-trajectory ridge level)

**Name:** ATRL  
**Claim:** at each close-eligible checkpoint, the two SR paths agree (within
0.25 dB, max aggregator) on the **fused mid PSD level of the unique triangular
band that contains the analytic instantaneous frequency at the selected
frame’s `source_time`.**

#### Procedure (executable prose; not implemented here)

For each gate SR path \(\in\{44100,48000,96000\}\) and each
\(f_{\mathrm{chk}}\) with `CHK_CLOSE(f_chk)`:

1. \(t_\times \leftarrow \texttt{sweep_crossing_time}(f_{\mathrm{chk}})\) (frozen law).
2. Selection oracle (CONTRACT §13.1): **absolute** nearest `source_time` to
   \(t_\times\) over all frames → \(t^\star_{\mathrm{abs}}\); require hop
   radius and useful ⊆; tie→FAIL or `min(frame_index)`; else FAIL.
   **Do not** filter-first to useful-segment then nearest.
3. Per path, take **used/published** operands
   \((t^\star_{\mathrm{used}},\,f^\star_{\mathrm{used}},\,b^\star_{\mathrm{used}})\)
   (independent values consumed by close + published as `t_star`/`f_star`/`b_star`).
   **Do not** assign \(f^\star:=f(t^\star)\) then “check” equality.
4. Enforce `TSTAR_LAW_OK`: \(t^\star_{\mathrm{used}}=t^\star_{\mathrm{nearest}}\).
   Binding \(t^\star:=t_\times\) → FAIL.
5. Enforce `RIDGE_LAW_OK`: \(f^\star_{\mathrm{used}}=f(t^\star_{\mathrm{used}})\)
   (analytic; **not** PSD argmax; **\(f^\star:=f_{\mathrm{chk}}\) FORBIDDEN**)
   and \(b^\star_{\mathrm{used}}=\mathrm{Band}(f^\star_{\mathrm{used}})\)
   (unique triangular band; shared edge → **lower-index**; outside
   \([20,20000]\) → FAIL).
6. **Common ridge band (fail-closed, per cell \(c\in D\)):** evaluate
   `RIDGE_MATCH(c) ⇔ b★_used_ref(c)=b★_used_sr(c)` (conjunct of `ATRL_OK`,
   `REQUIRED_ACTIVE`, and `SWEEP_TRANSPORT_CLOSE` — not procedure-only).
   If \(\neg\texttt{RIDGE\_MATCH}(c)\) → **FAIL**. The common index is written
   \(b^\star\) and is the **sole** ridge band for that cell’s ATRL \(L\),
   companions, side-gate neighbours, and
   \(B_{\mathrm{OFF}}:=\mathrm{UNIVERSE\_B}\setminus\{b^\star\}\) / veto operands.
   Independent per-path \(b^\star\) shopping is **REJECT**.
7. Observable: \(L = \texttt{mid_psd_db}[b^\star]\) after frozen fusion/floor/clamp
   — **one scalar per path** at that checkpoint (not a multi-band / multi-channel
   Cartesian product); under `MONO_ASSET`, \(L\) and mid companions ∈
   `{mid_psd_db, mid_shape_db, mid_prominence_db}` only.
8. **Validity / match (fail-closed):** if frame unmatched within hop, or
   `mid_valid=false` on either path for \(b^\star\), or \(L\) is NaN/Inf/missing
   → **FAIL** (no silent skip of that cell).
9. For each pair in `PAIRS` (ref = 48 kHz path; both pairs obligatory), the
   ATRL closing contribution is the single scalar
   \(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) at that cell
   \(c\in D=\{\mathrm{CHK\_CLOSE}\}\times\mathrm{PAIRS}\).

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
# Per close cell c ∈ D (interior CHK_CLOSE under D1 ⇒ candidate bands ∈ {1,…,118}).
# path ∈ {ref, sr} of the pair in c; f_chk labels the checkpoint only.
#
# ANTI-TAUTOLOGY: used/published (t★,f★,b★) are independent operands.
# FORBIDDEN: define f★:=f(t★) then check f★=f(t★) (definitional tautology).
# FORBIDDEN: bind f★_used:=f_chk or t★_used:=t_cross(f_chk) as SR-blind shortcuts.

# Selection oracle (CONTRACT §13.1 absolute nearest — not assignment of used f★/b★):
t★_abs(c, path)  :=
    source_time of the frame minimizing |source_time − t_cross(f_chk)| over ALL
    frames on that path. Tie (≥2 at d_min) → FAIL OR min(frame_index).
# Post-select (not filter-first search):
#   |t★_abs − t_cross| ≤ H/fs_c  else FAIL
#   t★_abs ∈ useful-segment      else FAIL
t★_nearest(c, path)  :=  t★_abs(c, path) after hop∧useful checks
# FORBIDDEN: argmin over useful∩hop / second-nearest shopping when abs awkward.

# Used/published triples (closing + CHK_REPORT publish; §4.5 / §6.1):
#   (t★_used, f★_used, b★_used)  — values actually consumed by ATRL / companions /
#   veto / TRANSPORT and published as (t_star, f_star, b_star). Not := by law.
# FORBIDDEN: define f★:=f(t★) then check f★=f(t★) (assign-then-check always
#   passes RIDGE_LAW_OK; process/schema FAIL — law remains check-only).

TSTAR_LAW_OK(c, path)  ⇔
    t★_used(c, path) = t★_nearest(c, path)
# t★_used := t_cross(f_chk) (ignore absolute nearest source_time) → ¬TSTAR_LAW_OK
# unmatched / outside hop / abs∉useful → FAIL (no soft t★)

RIDGE_LAW_OK(c, path)  ⇔
    TSTAR_LAW_OK(c, path)
    ∧  f★_used(c, path) = f(t★_used(c, path))   # analytic; NOT PSD argmax
    ∧  b★_used(c, path) = Band(f★_used(c, path))  # shared edge → lower-index
# Check-only vs independent used/published fields (§5.3 / §6.1).
# If used/published f★ was set to f_chk → FAIL whenever f(t★_used) ≠ f_chk.
# ¬RIDGE_LAW_OK(c, path) → FAIL.

TSTAR_LAW_OK(c)   ⇔  ∀ path ∈ {ref, sr} of c : TSTAR_LAW_OK(c, path)
RIDGE_LAW_OK(c)   ⇔  ∀ path ∈ {ref, sr} of c : RIDGE_LAW_OK(c, path)
# ATRL_OK / REQUIRED_ACTIVE / TRANSPORT require RIDGE_LAW_OK on used+published.

RIDGE_MATCH(c)  ⇔  b★_used_ref(c) = b★_used_sr(c)
# ¬RIDGE_MATCH(c) → FAIL.
# When RIDGE_MATCH(c): b★(c) := that common index
# (sole ridge for L / companions / neighbours / B_OFF).

REQUIRED_ACTIVE(c)  ⇔
    RIDGE_LAW_OK(c) ∧ RIDGE_MATCH(c) ∧ ACTIVE(b★) ∧ ACTIVE(b−) ∧ ACTIVE(b+)
# ¬REQUIRED_ACTIVE(c) → FAIL (no silent neighbour / ridge skip)
# REQUIRED_ACTIVE(c) ⇒ TSTAR_LAW_OK(c)  (via RIDGE_LAW_OK)

# Off-ridge universe (see §6.2):
UNIVERSE_B       :=  {0,…,119}
B_OFF(c)         :=  UNIVERSE_B \ {b★(c)}   # requires RIDGE_MATCH
B_OFF_ACTIVE(c)  :=  { b ∈ B_OFF(c) : ACTIVE(b) }
# B_OFF_ACTIVE(c) = ∅ → FAIL   (vacuous off-ridge veto / empty-union soft forbidden)
# B_GEOM must not shrink below B_OFF (§6.2).
```

Polarity `ONE_SIDED(b)` is **total** on every band where veto/side-gate
reads it: missing / NaN / Inf `mid_psd_db` → **FAIL** (not `false`).

**ATRL companions (mandatory AND — not OR-shopping; no custom median):**
ATRL level alone does **not** close. Companions are the **CONTRACT §7**
scalars at the **common** analytic ridge band \(b^\star\) — not a custom
off-ridge median prominence and not a “when prominence is required” escape.

**Cell domain (pinned — omit pair → FAIL):**

```text
PAIRS  :=  { (44100, 48000), (96000, 48000) }   # ref = 48 kHz; both pairs obligatory
D      :=  {CHK_CLOSE} × PAIRS
# Missing / omitted evaluation of any (f_chk, pair) ∈ D → FAIL
# (no silent drop of 44.1↔48 or 96↔48)
```

**Cell predicates** (one evaluation per \(c\in D\); operands on **common** \(b^\star\)):

```text
# Mono pin (executable) — normative frozen path (M2 fixture-spec @ e9916319 /
# digest 513c3baf…; structure cite only; this proposal does not edit it):
MONO_ASSET  ⇔  fixture_spec.categories.log_sweep.channels == 1
# SIDE_PAIR both-false trivial ONLY when MONO_ASSET under this pin; else FAIL.

# wav_channels — executable operand from frozen log_sweep WAV bytes (cite only;
# this proposal does not edit fixtures / SHA256SUMS):
#   ml_v3/fixtures/g1/audio/log_sweep/log_sweep_44100.wav
#   ml_v3/fixtures/g1/audio/log_sweep/log_sweep_48000.wav
#   ml_v3/fixtures/g1/audio/log_sweep/log_sweep_96000.wav
#   (listed in ml_v3/fixtures/g1/SHA256SUMS)
GATE_SR  :=  {44100, 48000, 96000}
# Sole authority — RIFF/WAVE `fmt ` chunk only (SHA256SUMS-bound path above):
wav_channels(sr)  :=
    NumChannels  uint16 at `fmt ` payload offset +2 of log_sweep_{sr}.wav
    # PCM IEEE-float tag 3 as rendered; no alternate definition.
decoded_pcm_arity(sr)  :=  channel count of decoded PCM array for that WAV
# Sole PCM layouts (interleaved IEEE-float as rendered) — no soft alternate:
#   ch = 1   ⇒  shape (N,)
#   ch = C>1 ⇒  shape (N, C)   (frame-major interleaved)
#   any other shape incl. (C, N) / planar / transposed → FAIL
# FORBIDDEN: “layout not pinned” / soft-accept non-sole decode shapes.
# Cross-check (not a second definition of wav_channels):
#   decoded_pcm_arity(sr) == wav_channels(sr)   else FAIL
#   ∧  decoded array conforms to sole layout for that ch
# FORBIDDEN: wav_channels := decoded PCM arity / ndim / shape[1]  ("equivalently")
# FORBIDDEN: wav_channels := asset_manifest.channels   (copy / tautology)
# FORBIDDEN: wav_channels := fixture_spec…channels     (copy / tautology)
# Operand source = RIFF `fmt ` NumChannels of the frozen fixture path above.

# Per-cell companions; threshold = 0.25 dB
SHAPE_OK(c)       ⇔  |mid_shape_db_ref[b★] − mid_shape_db_sr[b★]| ≤ 0.25
PROMINENCE_OK(c)  ⇔  |mid_prominence_db_ref[b★] − mid_prominence_db_sr[b★]| ≤ 0.25

# Side pair (CONTRACT side_* at common b★) — executable polarity:
sv_ref(c)  :=  side_valid_ref[b★]
sv_sr(c)   :=  side_valid_sr[b★]
SIDE_BOTH_FALSE(c)  ⇔  ¬sv_ref(c) ∧ ¬sv_sr(c)
SIDE_BOTH_TRUE(c)   ⇔   sv_ref(c) ∧  sv_sr(c)
SIDE_XOR(c)         ⇔  sv_ref(c) XOR sv_sr(c)

SIDE_DELTA_OK(c)  ⇔
    |side_shape_db_ref[b★] − side_shape_db_sr[b★]| ≤ 0.25
    ∧  |side_prominence_db_ref[b★] − side_prominence_db_sr[b★]| ≤ 0.25

# CONTRACT §4.1 mono → field checks (not prose-only):
#   mono: mid = input; side not valid  (CONTRACT §4.1)
# Crisp enum — closing L / companion mid operands under MONO_ASSET:
MONO_CLOSING_MID_FIELDS  :=  { mid_psd_db, mid_shape_db, mid_prominence_db }
# side_* excluded from closing mid operands when MONO_ASSET.
§4.1_MONO_FIELDS  ⇔
    ∀ path ∈ GATE_SR paths used in D, ∀ b ∈ UNIVERSE_B :
        side_valid_path[b] = false
    ∧  ATRL / companion closing L operands ∈ MONO_CLOSING_MID_FIELDS
       # at common b★ under RIDGE_MATCH; not side_psd_db / side_shape_db /
       # side_prominence_db / any side_* as mid substitute
# ¬§4.1_MONO_FIELDS → FAIL when MONO_ASSET requires it.

# Mono cross-check — normative close conjunct (not prose-only):
MONO_CROSSCHECK  ⇔
    asset_manifest.channels == fixture_spec.categories.log_sweep.channels
    ∧  (∀ sr ∈ GATE_SR : wav_channels(sr) == asset_manifest.channels)
    ∧  (∀ sr ∈ GATE_SR : decoded_pcm_arity(sr) == wav_channels(sr))
    ∧  ( MONO_ASSET  ⇒
           (∀ c ∈ D : SIDE_BOTH_FALSE(c))
           ∧  §4.1_MONO_FIELDS )
# ¬MONO_CROSSCHECK → FAIL. Mismatch fixture_spec / manifest / RIFF / decode /
# §4.1 fields → FAIL.

SIDE_PAIR_OK(c)  ⇔
    MONO_CROSSCHECK
    ∧  ( ( SIDE_BOTH_FALSE(c) ∧ MONO_ASSET )
         ∨  ( SIDE_BOTH_TRUE(c) ∧ SIDE_DELTA_OK(c) ) )
# ⇒ SIDE_PAIR_OK ⇒ MONO_CROSSCHECK
# ⇒ SIDE_XOR(c) → false; (¬MONO_ASSET ∧ SIDE_BOTH_FALSE) → false
# mid SHAPE/PROMINENCE still AND; trivial both-false ≠ skip of mid companions

COMPANIONS_OK(c)  ⇔  SHAPE_OK(c) ∧ PROMINENCE_OK(c) ∧ SIDE_PAIR_OK(c)
```

**Required |Δ| set and domain close (∀ — no soft fold / “when binding” shopping):**

```text
# Req(c) = required |Δ| component set at cell c:
#   always {|Δ_shape|, |Δ_prominence|};
#   + {|Δ_side_shape|, |Δ_side_prominence|} iff SIDE_BOTH_TRUE(c)
#     (MONO_ASSET ∧ SIDE_BOTH_FALSE: side |Δ| ∉ Req(c) under trivial pin)
# Any required component null / missing / NaN / Inf → FAIL (not soft skip).

δ_companions(c)  :=  max  Req(c)
                     # max over the required component set only
                     # (not mean / p95 / any soft fold of components)

# Sole normative close (∀ form):
COMPANIONS_OK  ⇔  ∀ c ∈ D : COMPANIONS_OK(c)
               ⇔  ∀ c ∈ D : SIDE_PAIR_OK(c) ∧ δ_companions(c) ≤ 0.25 dB
# Corollary only (not an alternate norm): max_{c∈D} δ_companions(c) ≤ 0.25
# is necessary for the δ conjunct, **not** sufficient alone
# (does not imply SIDE_PAIR_OK / COMPANIONS_OK).
# Forbidden: claiming δ-alone ≡ COMPANIONS_OK; claiming max-line ≡ sole norm.
# Forbidden aggregators: mean / p95 / “any cell” / single-cell PASS / component fold≠max
```

Missing / null any required companion field → **FAIL**. Forbidden: replacing
`mid_prominence_db[b★]` with \(L[b^\star]-\mathrm{median}(L[b])\) over
off-ridge; forbidden: dropping prominence when an off-ridge set is empty.

**Side gate (AND hardness only — never a substitute for companions):**

```text
# Triangular adjacency under frozen 120-band partition (§6.1)
# Same domain D := {CHK_CLOSE} × PAIRS as ATRL_OK / COMPANIONS_OK / VETO_OK
# Requires RIDGE_MATCH(c); b★ = common ridge index; else cell FAIL
b− = b★ − 1
b+ = b★ + 1
# Close-eligible f★ maps to interior b★ under D1 geometry; if b★∈{0,119}
# → FAIL (no silent neighbour collapse)

SIDE_ASYM(path) = |mid_psd_db_path[b−] − mid_psd_db_path[b+]|

SIDE_GATE_FIRE(c)  ⇔  ONE_SIDED(b−) ∨ ONE_SIDED(b+)
                   ∨  |SIDE_ASYM(ref) − SIDE_ASYM(sr)| > 0.25
                   ∨  missing evaluation of any operand

SIDE_GATE_OK(c)    ⇔  ¬ SIDE_GATE_FIRE(c)
```

**Domain close (∀ binding; max ≡ ∀ on asymmetry operand; FIRE_GLOBAL ≡ ∃):**

```text
δ_side(c)  :=  |SIDE_ASYM(ref) − SIDE_ASYM(sr)| at c
# ONE_SIDED(b±) or missing operand at any c → SIDE_GATE_FIRE(c) (not a soft max skip)

SIDE_GATE_FIRE_GLOBAL  ⇔  ∃ c ∈ D : SIDE_GATE_FIRE(c)

SIDE_GATE_OK  ⇔  ∀ c ∈ D : SIDE_GATE_OK(c)
              ⇔  ∀ c ∈ D : ¬ SIDE_GATE_FIRE(c)
              ⇔  SIDE_GATE_FIRE_GLOBAL = false
              ⇔  (∀ c ∈ D : ¬ONE_SIDED(b±) ∧ operands present)
                 ∧  max_{c ∈ D}  δ_side(c)  ≤  0.25 dB
# ∀ and max lines are definitionally equivalent on the asymmetry conjunct;
# polarity / missing are FIRE predicates, not “when binding” optional max terms.
# Forbidden: single-cell OK / soft OR across cells / mean / p95
```

Side gate **adds** hardness (`∧ SIDE_GATE_OK`). It does **not** replace
`COMPANIONS_OK`. OR-shopping “companions **or** side gate” is **REJECT**.
`COMPANIONS_OK` does **not** include `VETO_OK` (veto remains a separate conjunct
in §6.3).

**ATRL level rule (component — `RIDGE_LAW_OK`/`TSTAR_LAW_OK` + `RIDGE_MATCH`):**

```text
ATRL_OK(c)  ⇔
    RIDGE_LAW_OK(c)                 # ⇒ TSTAR_LAW_OK; used/published triples
    ∧  RIDGE_MATCH(c)
    ∧  |L_ref − L_sr|(c) ≤ 0.25 dB
    where L = mid_psd_db[b★]   (defined only under RIDGE_MATCH; one scalar/path)
# ATRL_OK(c) ⇒ RIDGE_LAW_OK(c) ⇒ TSTAR_LAW_OK(c)
# kills f★_used:=f_chk and t★_used:=t_cross via algebra on used operands
# (not definitional f★:=f(t★) then check; not comments)

ATRL_OK  ⇔  ∀ c ∈ D : ATRL_OK(c)          # sole normative form
# Corollary only (not an alternate norm): once ∀c RIDGE_MATCH(c),
#   max_{c∈D} |L_ref−L_sr|(c) ≤ 0.25  ⇔  ∀c |ΔL|(c) ≤ 0.25
# mean / p95 / RMSE forbidden.
# Forbidden: treating RIDGE_LAW_OK / TSTAR_LAW_OK / RIDGE_MATCH / used t★·f★·b★
# as procedure-only outside ATRL_OK / REQUIRED_ACTIVE / TRANSPORT / COMPLETE.
```

**Pin (domain D):** the ATRL / companions / side-gate / veto domain is **the
same** \(D=\{\mathrm{CHK\_CLOSE}\}\times\mathrm{PAIRS}\);
**not** `{valid mid channels}`, **not** a Cartesian product over mid bands /
channels, and **not** a single soft cell. Exactly one **common** \(b^\star\)
(via `RIDGE_MATCH`) and one \(L\) per path per close-eligible checkpoint;
exactly one \(|L_{\mathrm{ref}}-L_{\mathrm{sr}}|\) and one companions/side-gate/veto
cell evaluation per cell in \(D\). \(\neg\texttt{RIDGE\_MATCH}(c)\) → **FAIL**.
Omit either pair → **FAIL**.

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

**D11 menu freeze (normative — components only):** the locked **menu
components** are **ATRL + §5.3 companions + §6.2 artifact veto (+ §5.3
side-gate hardness)**. D11 does **not** name a close predicate and does **not**
authorize a short 4-AND “package PASS.” Sole close = **D3 ⇔
`SWEEP_TRANSPORT_CLOSE`** (§6.3 full). S2-ALT\* rows below are **archival /
non-closing**. Selecting an alt, leaving the menu open, OR-shopping ATRL vs
alt, or claiming D2/D11 short-AND close → **no SOUND / no measure
authorization** / **REJECT**. A later component swap requires a **fresh
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
| S2-R12 | Leave S2-ALT\* menu open / close on an alt without fresh amend | **REJECT** — D11 freeze **components only**; sole close `D3⇔TRANSPORT` |
| S2-R13 | Label activity-admitted off-ridge \|Δ\|>0.25 as diagnostic-only (veto escape) | **REJECT** — see §6.2 |
| S2-R14 | Silent §13.2.4 / gate-4 equivalence of this ATRL package | **REJECT** — not gate-4 equivalent until REV7 |
| S2-R15 | Treat empty `B_OFF_ACTIVE` / missing `ACTIVE(b★∧b±)` as soft PASS | **REJECT** — FAIL; see §5.3 |
| S2-R16 | Soft max fold / mean/p95 of companion \|Δ\|; “when binding” optional max | **REJECT** — δ:=max of required \|Δ\|; max≡∀ |
| S2-R17 | Omit either pair in PAIRS / shrink D below `{CHK_CLOSE}×PAIRS` | **REJECT** — omit pair → FAIL |
| S2-R18 | Treat both `side_valid=false` as trivial on stereo / non-mono | **REJECT** — trivial iff `MONO_ASSET ⇔ fixture_spec.categories.log_sweep.channels==1` |
| S2-R19 | Independent \(b^\star_{\mathrm{ref}}\neq b^\star_{\mathrm{sr}}\) / per-path ridge shopping | **REJECT** — `¬RIDGE_MATCH` → FAIL (in ATRL_OK / TRANSPORT) |
| S2-R20 | Claim δ-alone ≡ `COMPANIONS_OK` (drop `SIDE_PAIR_OK`) | **REJECT** — `COMPANIONS_OK ⇔ ∀c SIDE_PAIR_OK ∧ δ≤0.25` |
| S2-R21 | Soft-skip null/missing component in `Req(c)` when folding δ | **REJECT** — null→FAIL |
| S2-R22 | Use \(f^\star:=f_{\mathrm{chk}}\) as ridge operand (SR-blind tautology) | **REJECT** — `¬RIDGE_LAW_OK` on used/published; conjunct in ATRL_OK / REQUIRED_ACTIVE / TRANSPORT / COMPLETE |
| S2-R23 | Omit `MONO_CROSSCHECK` from `SIDE_PAIR_OK` / TRANSPORT | **REJECT** — both ⇒ `MONO_CROSSCHECK` |
| S2-R24 | Shrink `B_GEOM` below `B_OFF:=UNIVERSE_B\{b★}` | **REJECT** — §6.2 |
| S2-R25 | CLOSE used ≠ published; REPORT law/publish violation | **REJECT** — EQ_PUBLISH: CLOSE used≡publish; REPORT publish-vs-law only (§4.5) |
| S2-R26 | Define `f★:=f(t★)` then check `f★=f(t★)` (definitional tautology) | **REJECT** — law quantifies used/published operands vs `f(t★_used)` / `Band(f★_used)` |
| S2-R27 | Bind `t★_used:=t_cross(f_chk)` / ignore absolute nearest / filter-first useful | **REJECT** — `¬TSTAR_LAW_OK` (§13.1) |
| S2-R28 | `wav_channels`:=decoded PCM arity / “equivalently decode” second authority | **REJECT** — RIFF/`fmt ` NumChannels sole; decode≠RIFF → FAIL |
| S2-R29 | `CHK_REPORT_PUBLISH_COMPLETE` = non-null only / soft `¬match_ok` / vacuous or under-floor REPORT / duplicate keys | **REJECT** — §4.5: \|REPORT_SET\|≥3 `{20,45,20000}` + bijection + `match_ok∧mid_valid` + named TSTAR/RIDGE |
| S2-R30 | Claim D2/D11 short 4-AND “package PASS” as close | **REJECT** — D2/D11=components only; sole close `D3⇔TRANSPORT` |
| S2-R31 | §6.1 / REPORT define `f★:=f(t★)` then check identity (definitional tautology) | **REJECT** — publish independent; check-only `RIDGE_LAW_OK` |
| S2-R32 | Require ATRL-used operands on REPORT for `EQ_PUBLISH` | **REJECT** — REPORT∉ATRL max; publish-vs-law only (§4.5) |
| S2-R33 | `source_time_selected := t★_nearest` schema mirror (vacuous TSTAR) | **REJECT** — check-equality only |
| S2-R34 | Claim `f★:=f(t★) ⇒ ¬RIDGE_LAW_OK` (false; assign-then-check always passes) | **REJECT** — FORBIDDEN as process; RIDGE remains check-only |
| S2-R35 | Unbound nearest-frame tie / PASS-shop among equidistant frames | **REJECT** — tie→FAIL or `min(frame_index)` |
| S2-R36 | Soft-accept planar/(C,N)/“layout not pinned” PCM decode | **REJECT** — sole layouts only (§5.3) |

---

## 6. Off-ridge policy (fail-closed closing conjunction)

Off-ridge energy / artifacts **cannot vanish**.

### 6.1 Mandatory report (necessary, not sufficient)

For each close-eligible checkpoint frame, publish at least:

- **per path** independent used/published operands
  \((t^\star_{\mathrm{path}},\, f^\star_{\mathrm{path}},\, b^\star_{\mathrm{path}})\)
  — **ANTI-TAUTOLOGY HARD:** publish as independent fields; then
  **check-only** §5.3 `TSTAR_LAW_OK` ∧ `RIDGE_LAW_OK`
  (\(t^\star=t^\star_{\mathrm{nearest}}\),
  \(f^\star=f(t^\star)\),
  \(b^\star=\mathrm{Band}(f^\star)\)).
  **FORBIDDEN:** define \(f^\star:=f(t^\star)\) then check the identity;
  **FORBIDDEN:** \(f^\star:=f_{\mathrm{chk}}\) / \(t^\star:=t_{\mathrm{cross}}\);
- common \(b^\star\) when `RIDGE_MATCH` (else FAIL already);
- `mid_psd_db[b^\star]` per SR path + `mid_valid`;
- ATRL companions / side-gate fields (§5.3);
- CLOSE publish fields **must equal** used close/ATRL operands (§4.5
  `CLOSE_OPERANDS_EQ_PUBLISH`; mismatch→FAIL). REPORT cells use publish-vs-law
  only — **no** ATRL-used operand required on REPORT;
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
# Frozen triangular partition (§6.1): off-ridge = complement of common ridge
UNIVERSE_B :=  {0,…,119}
B_OFF(c)   :=  UNIVERSE_B \ {b★(c)}          # needs RIDGE_MATCH
B_OFF(f★)  :=  B_OFF(c)                      # alias (cell’s common ridge)

# Classical periodic Hann (structure only; not run-fitted):
ENBW_HANN_BINS          = 1.5
MAIN_LOBE_HALF_BINS     = 2          # centre → first null
# Off-ridge obligation is non-vacuous: |B_OFF| = 119 ≥ ceil(ENBW) and
# triangular supports exist outside b★. These constants justify the set;
# they do **not** shrink the veto domain below B_OFF.

B_GEOM(f★) := B_OFF(f★)             # full off-ridge index set
# FORBIDDEN: B_GEOM ⊊ B_OFF  (must not shrink below B_OFF)
```

**Executable veto (per cell \(c\in D\); domain close via GLOBAL / ∀ — not prose-only OR):**

```text
# Same D := {CHK_CLOSE} × PAIRS as ATRL / companions / side-gate (§5.3)

OFF_RIDGE_DELTA_FIRE(c)  ⇔
    ∃ b ∈ B_OFF(f★) :
        ACTIVE(b)
        ∧  |mid_psd_db_ref[b] − mid_psd_db_sr[b]| > 0.25

SUPPORT_POLARITY_MISMATCH(c)  ⇔
    ∃ b ∈ B_GEOM(f★) : ONE_SIDED(b)

ARTIFACT_VETO_FIRE(c)  ⇔
    OFF_RIDGE_DELTA_FIRE(c)
    ∨  SUPPORT_POLARITY_MISMATCH(c)
    ∨  missing / NaN / Inf evaluation of any operand above

ARTIFACT_VETO_FIRE_GLOBAL  ⇔  ∃ c ∈ D : ARTIFACT_VETO_FIRE(c)

VETO_OK  ⇔  ∀ c ∈ D : ¬ ARTIFACT_VETO_FIRE(c)
         ⇔  ARTIFACT_VETO_FIRE_GLOBAL = false
         ⇔  ¬∃ c ∈ D : ARTIFACT_VETO_FIRE(c)
# The three lines are definitionally equivalent (same style as SIDE_GATE_OK).
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
    ∧  ∀c∈D : RIDGE_LAW_OK(c)          # used/published; ⇒ TSTAR_LAW_OK; ≠f_chk / ≠t_cross bind
    ∧  ∀c∈D : RIDGE_MATCH(c)           # b★_used_ref=b★_used_sr (§5.3)
    ∧  ATRL_OK                         # sole ∀c ATRL_OK(c) ⇒ RIDGE_LAW_OK ∧ RIDGE_MATCH
    ∧  COMPANIONS_OK                   # sole ∀c COMPANIONS_OK(c); δ≠alone
    ∧  VETO_OK                         # §6.2 ∀c∈D ¬FIRE(c) ⇔ ¬FIRE_GLOBAL
    ∧  SIDE_GATE_OK                    # §5.3 ∀c∈D; FIRE_GLOBAL=false
    ∧  MONO_CROSSCHECK                 # ⇒ SIDE_PAIR_OK already; RIFF+decode≡; explicit here
    ∧  CHK_REPORT_PUBLISH_COMPLETE     # ⇔ §4.5: |REPORT_SET|≥3 + bijection + match_ok + named TSTAR/RIDGE
    ∧  CLOSE_OPERANDS_EQ_PUBLISH       # CLOSE: used≡publish; REPORT: publish-vs-law (no ATRL-used)
    ∧  ∀c∈D : REQUIRED_ACTIVE(c)       # RIDGE_LAW_OK∧RIDGE_MATCH∧ACTIVE(b★∧b±)
    ∧  ∀c∈D : B_OFF_ACTIVE(c) ≠ ∅      # B_OFF:=UNIVERSE_B\{b★}; empty → FAIL
    ∧  no mid_valid=false / unmatched / empty-union-on-required FAIL (§5.3)
# D := {CHK_CLOSE} × PAIRS; omit either pair → FAIL
# D3 ⇔ SWEEP_TRANSPORT_CLOSE (this full conjunct — SOLE close predicate).
# D2/D11 name menu components only — short ATRL∧COMP∧VETO∧SIDE “package PASS” REJECT.
# COMPLETE ⇒ match_ok∧mid_valid∧TSTAR_LAW_OK∧RIDGE_LAW_OK on every REPORT×path.
# RIDGE_LAW_OK / TSTAR_LAW_OK + RIDGE_MATCH + MONO_CROSSCHECK + COMPLETE
# are algebra on used/published operands, not procedure-only / :=-tautology.
# SWEEP_TRANSPORT_CLOSE ⇒ RIDGE_LAW_OK; ⇒ TSTAR_LAW_OK; ⇒ MONO_CROSSCHECK;
# SIDE_PAIR_OK ⇒ MONO_CROSSCHECK.
```

**Report alone does not close.** ATRL≤0.25 alone does not close. Companions
**and** veto **and** side-gate hardness all required. Any missing conjunct
field → **FAIL**. This close is **not** §13.2.4 / gate-4 equivalent
(S2-R14; not until REV7). `D3` names **this** predicate as the **sole** close
— a short `ATRL_OK ∧ COMPANIONS_OK ∧ VETO_OK ∧ SIDE_GATE_OK` stand-in, or any
D2/D11 “package PASS” under that short AND, is **REJECT**.

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
| S1 guards | **D1 LOCKED** — §4.2 `T_LEAD`/`T_TRAIL` geometry; no silent widen |
| Ridge operands | used/published `(t★,f★,b★)`; `TSTAR_LAW_OK ⇔ t★_used=t★_nearest` (check; never `source_time_selected:=`); `RIDGE_LAW_OK` check-only vs analytic; **FORBIDDEN** `f★:=f(t★)` then check (always passes algebra); `f★:=f_chk` / `t★:=t_cross` → FAIL |
| Common \(b^\star\) | `RIDGE_MATCH(c) ⇔ b★_used_ref=b★_used_sr` in ATRL_OK / REQUIRED_ACTIVE / TRANSPORT; else **FAIL** |
| `RIDGE_LAW_OK` / `TSTAR_LAW_OK` | named conjuncts in `ATRL_OK(c)`, `REQUIRED_ACTIVE(c)`, `SWEEP_TRANSPORT_CLOSE` (∀c); COMPLETE enforces same law on REPORT |
| `MONO_ASSET` | **⇔** `fixture_spec.categories.log_sweep.channels==1`; SIDE_PAIR trivial only then |
| `wav_channels(sr)` | **:=** RIFF/`fmt ` `NumChannels` sole (∀sr∈GATE_SR); decode arity **≡** RIFF else FAIL; **≠** decode definition; **≠** manifest/spec copy |
| Nearest oracle | CONTRACT §13.1 **absolute** nearest over all frames; hop∧useful ⊆ **after**; tie→FAIL∨`min(frame_index)`; filter-first useful **REJECT** |
| PCM decode layout | sole: ch=1⇒`(N,)`; ch=C>1⇒`(N,C)`; `(C,N)`/planar/other→FAIL; “not pinned” **REJECT** |
| `MONO_CROSSCHECK` | manifest==fixture_spec…channels ∧ (∀sr `wav_channels==manifest`) ∧ (∀sr decode≡RIFF) ∧ (`MONO_ASSET`⇒∀c SIDE_BOTH_FALSE ∧ `§4.1_MONO_FIELDS`); in `SIDE_PAIR_OK` **and** TRANSPORT |
| `§4.1_MONO_FIELDS` | `MONO_ASSET`⇒∀path∀b `side_valid=false` ∧ closing mid operands ∈ `{mid_psd_db, mid_shape_db, mid_prominence_db}` only (`side_*` excluded) |
| `SIDE_PAIR_OK` | **⇔** `MONO_CROSSCHECK ∧ ((BOTH_FALSE∧MONO_ASSET) ∨ (BOTH_TRUE∧SIDE_DELTA_OK))`; XOR / non-mono both-false → false |
| `CHK_REPORT_PUBLISH_COMPLETE` | **⇔** \|REPORT_SET\|≥3 ∧ `{20,45,20000}⊆REPORT_SET` ∧ keys biject REPORT_SET×GATE_SR ∧ ∀REPORT×path: `match_ok∧mid_valid` ∧ named `TSTAR_LAW_OK`∧`RIDGE_LAW_OK` ∧ `L=mid_psd_db[b_star]`; ¬match_ok→¬COMPLETE→¬TRANSPORT |
| `UNIVERSE_B` / `B_OFF` | `UNIVERSE_B:={0..119}`; `B_OFF:=UNIVERSE_B\{b★}`; `B_GEOM` ≮ `B_OFF` |
| \|CHK_CLOSE\| | **≥7**; empty → FAIL |
| D2 / D11 | menu **components only**; **≠** close |
| D3 / Off-ridge close | **sole close: D3 ⇔** `SWEEP_TRANSPORT_CLOSE` (§6.3 full); short 4-AND / D2-D11 “package PASS” **REJECT**; report≠close |
| ATRL_OK / COMPANIONS_OK | sole normative **⇔** ∀c form; max/δ corollary only; `ATRL_OK`⇒`RIDGE_LAW_OK`⇒`TSTAR_LAW_OK` |
| Close publish | CLOSE: used operands **≡** published; REPORT: publish-vs-law only (no ATRL-used / no used≡publish on REPORT); mismatch→FAIL |
| COMPANIONS_OK | **⇔** ∀c `COMPANIONS_OK(c)` (⇒ `SIDE_PAIR_OK` ∧ δ≤0.25); δ-alone **≠** COMPANIONS_OK |
| A3 / A4 ACTIVE | **closed** — A3 retired-as-solution; no A4 |
| Menu freeze | components **ATRL + companions + veto (+ side-gate)** only; S2-ALT\* archival; close = D3⇔TRANSPORT only (no “package PASS”) |

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
| S1 guard direction | **LOCKED** (D1) on §4.2 geometry + cardinality / REPORT fail-closed — S1 locked ≠ package self-SOUND; **MEASURE** still **NO** until dual non-POROUS stamps |
| S2 menu | **LOCKED components** — ATRL + CONTRACT companions + executable veto (+ side-gate AND); S2-ALT\* archival; **≠** close |
| Off-ridge vanishing | **NO** — **sole close D3 ⇔** `SWEEP_TRANSPORT_CLOSE` (§6.3 full); short ATRL∧COMPANIONS∧VETO∧SIDE / D2-D11 “package PASS” **REJECT** |
| §13.2.4 / gate-4 equivalence | **NO** — not gate-4 equivalent until REV7 (S2-R14) |
| Self-SOUND / MEASURE | **NO** — not self-SOUND (fresh re-CC+RT required); **MEASURE_AUTHORIZED NO**; Dual-SOUND **NO** |
| Next lab action | fresh re-CC+RT on D1 + `:=` kill + RIDGE claim fix + EQ_PUBLISH split + §13.1 abs nearest/tie + sole PCM + COMPLETE pins + sole `D3⇔TRANSPORT` → only then measure auth (still NO until dual non-POROUS) |

---

## 11. Closed POROUS findings (this revision)

| ID | Source | Finding | Closure in this doc |
|----|--------|---------|---------------------|
| P1 | CC | ATRL closing max Cartesian `{valid mid channels}` | **CLOSED** — one scalar \|L_ref−L_sr\| per cell in `D={CHK_CLOSE}×PAIRS`; L=mid_psd_db[b★] |
| P2 | CC | Additive T_MEM CC-open shopping | **CLOSED** — normative `T_MEM=0`; non-zero needs fresh untainted amend |
| P3 | CC | S2 alt id “A3” collides retired ACTIVE A3 | **CLOSED** — renamed **S2-ALT1/2/3** |
| P4 | RT CRITICAL | Off-ridge report-alone / optional / example-only veto | **CLOSED** — executable `ARTIFACT_VETO_FIRE` §6.2; report≠close; missing→FAIL; no diagnostic escape |
| P5 | RT / re-CC CRITICAL | Companions vs side-gate OR-shopping; custom median; “when required” escape; soft/single-cell aggregator | **CLOSED** — COMPANIONS_OK ⇔ ∀c∈D SIDE_PAIR_OK ∧ δ≤0.25 (CONTRACT mid shape/prominence + side); SIDE_GATE_OK = ∀c∈D ¬FIRE; **VETO_OK separate** (§6.2/§6.3) |
| P6 | RT HIGH | CHK_REPORT soft / TRANSPORT PASS without REPORT | **CLOSED** — §4.5 publish schema; missing→FAIL; TRANSPORT≠PASS if REPORT absent |
| P7 | RT HIGH | mid_valid=false / unmatched / empty activity skipped | **CLOSED** — FAIL; required empty-union→FAIL; polarity total |
| P8 | RT HIGH | \|CHK_CLOSE\| vacuous / under-populated; T_MEM drift | **CLOSED** — \|CHK_CLOSE\|<7 or ∅ → FAIL; `T_MEM=0` locked |
| P9 | RT / re-CC CRITICAL | Open S2/alt menu | **CLOSED** — D11 freeze **components only**; sole close `D3⇔TRANSPORT`; S2-ALT\* archival |
| P10 | RT MED | Deep LF-per-checkpoint aperture | **NOTE only** — §4.6 deferred; not redesigned here |
| P11 | re-CC + delta RT | Never-fire / unevaluated veto | **CLOSED** — missing operands → FIRE/FAIL; formula pinned §6.2 |
| P12 | delta RT `8aa8027c` HIGH | Empty-union soft; missing neighbour activity; empty B_OFF_ACTIVE | **CLOSED** — `REQUIRED_ACTIVE ⇔ ACTIVE(b★)∧ACTIVE(b±)`; `B_OFF_ACTIVE=∅ → FAIL`; both enter §6.3 close |
| P13 | re-CC residual / delta CRITICAL | COMPANIONS_OK / SIDE_GATE_OK soft/single-cell aggregator | **CLOSED** — COMPANIONS_OK ⇔ ∀c∈D SIDE_PAIR_OK∧δ≤0.25 on same `D={CHK_CLOSE}×PAIRS`; SIDE_GATE_OK ∀c / FIRE_GLOBAL≡∃; ATRL max≡∀ |
| P14 | re-CC residual | §5.2 “another form may win at CC” open-menu echo vs D11 | **CLOSED** — §5.2 closed on D11 **components only**; sole close `D3⇔TRANSPORT`; swap only via fresh untainted amend + CC |
| P15 | delta RT `8aa8027c` HIGH | Silent §13.2.4 / gate-4 equivalence via domain shrink | **CLOSED** — S2-R14 REJECT; **not gate-4 equivalent until REV7** (no mid-flight meter expand) |
| P16 | delta RT MED / `bd10f03c` + `765045fb` | Mono `SIDE_PAIR` both `side_valid=false` ambiguous | **CLOSED** — `MONO_ASSET ⇔ fixture_spec.categories.log_sweep.channels==1`; trivial both-false **only then**; else FAIL; mid companions still AND |
| P17 | delta RT MED / re-CC `98e2caa1` residual | D1 PROPOSED blocked SOUND/measure; readable as ready | **CLOSED** — D1 **LOCKED** on §4.2; S1 locked ≠ package self-SOUND; MEASURE still NO until dual non-POROUS (§3, §10) |
| P18 | re-CC `258d5f66` WARNING | §6.2 VETO_OK prose-only “OR over cells” | **CLOSED** — `ARTIFACT_VETO_FIRE_GLOBAL ⇔ ∃c∈D FIRE(c)`; `VETO_OK ⇔ ∀c∈D ¬FIRE(c)` |
| P19 | re-CC `258d5f66` WARNING / RT `bd10f03c` HIGH + `765045fb` | Soft max “folds” / false δ-alone ≡ COMPANIONS_OK | **CLOSED** — `δ:=max Req(c)`; `COMPANIONS_OK ⇔ ∀c SIDE_PAIR_OK ∧ δ≤0.25` (δ-alone **not** ≡); ATRL/SIDE max≡∀ unchanged |
| P20 | delta RT `bd10f03c` HIGH | Unpinned SR-vs-ref pair set / silent omit pair | **CLOSED** — `PAIRS:={(44100,48000),(96000,48000)}`; `D:=CHK_CLOSE×PAIRS`; omit pair → FAIL |
| P21 | delta RT `765045fb` HIGH | False δ-alone ≡ COMPANIONS_OK | **CLOSED** — drop that ⇔; normative `∀c SIDE_PAIR_OK ∧ δ≤0.25` (§5.3) |
| P22 | delta RT `765045fb` HIGH | Independent \(b^\star_{\mathrm{ref}}\neq b^\star_{\mathrm{sr}}\) / per-path ridge | **CLOSED** — `RIDGE_MATCH` in ATRL_OK + TRANSPORT; common \(b^\star\) for \(L\)/companions/neighbours/`B_OFF` |
| P23 | delta RT `765045fb` HIGH / re-CC WARNING | Mono trivial pin without frozen `channels` field | **CLOSED** — `MONO_ASSET ⇔ fixture_spec.categories.log_sweep.channels==1`; manifest/§4.1 cross-check FAIL if mismatch; SIDE_PAIR trivial only then |
| P24 | delta RT `765045fb` HIGH | Null/missing `Req(c)` component soft in δ fold | **CLOSED** — any null in `Req(c)` → FAIL |
| P25 | delta RT `9ce35909` HIGH | `b★_ref=b★_sr` procedure-only (outside ATRL/TRANSPORT algebra) | **CLOSED** — `RIDGE_MATCH(c)` conjunct in `ATRL_OK` and `SWEEP_TRANSPORT_CLOSE` |
| P26 | delta RT `9ce35909` HIGH | `SIDE_PAIR_OK` prose-only polarity | **CLOSED** — executable `(BOTH_FALSE∧MONO_ASSET) ∨ (BOTH_TRUE∧SIDE_DELTA_OK)` |
| P27 | delta RT `9ce35909` / cheap | `B_OFF` not stated as complement | **CLOSED** — `B_OFF:=complement({b★})` |
| P28 | delta RT `3971c3c7` / `a0960860` / re-CC `1e6f969e` CRITICAL / `f0a5fe98` | `f★:=f_chk` / definitional `f★:=f(t★)` tautology / no `TSTAR_LAW` / procedure-only | **OPEN/PINNED** — used/published `RIDGE_LAW_OK`⇒`TSTAR_LAW_OK`; `f★_used=f(t★_used)` ∧ `b★_used=Band(f★_used)`; in ATRL/REQUIRED_ACTIVE/TRANSPORT/COMPLETE; **no self-CLOSED** until dual non-POROUS |
| P29 | delta RT `3971c3c7` HIGH | Mono cross-check prose-only (outside close algebra) | **CLOSED** — `MONO_CROSSCHECK` conjunct; `SIDE_PAIR_OK`⇒`MONO_CROSSCHECK`; `SWEEP_TRANSPORT_CLOSE`⇒`MONO_CROSSCHECK` |
| P30 | delta RT `3971c3c7` MED | `B_OFF`/`B_GEOM` universe shrink ambiguity | **CLOSED** — `UNIVERSE_B:={0..119}`; `B_OFF:=UNIVERSE_B\{b★}`; `B_GEOM` ≮ `B_OFF` |
| P31 | delta RT `3971c3c7` MED | max/δ dual-norm vs ∀ sole form | **CLOSED** — ATRL_OK / COMPANIONS_OK sole normative = ∀; max/δ corollary only |
| P32 | delta RT `3971c3c7` MED / re-CC `1577733c` WARNING | close operands ≠ publish; stale used≡publish on REPORT | **CLOSED** — `CLOSE_OPERANDS_EQ_PUBLISH`: CLOSE used≡publish; REPORT publish-vs-law only (not used≡publish on REPORT); mismatch→FAIL |
| P33 | re-CC `3f421a4a` / `a0960860` / delta RT `f0a5fe98` HIGH | `wav_channels` unbound / dual-def decode / manifest copy | **OPEN/PINNED** — RIFF/`fmt ` NumChannels sole; decode≡RIFF else FAIL; “equivalently decode” REJECT; pending dual stamp |
| P34 | re-CC `3f421a4a` / `1e6f969e` / `f0a5fe98` / `98755492` HIGH | `CHK_REPORT_PUBLISH_COMPLETE` unbound / presence-only / vacuous emission | **OPEN/PINNED** — COMPLETE ⇔ \|REPORT_SET\|≥3 + bijection + `match_ok` + named TSTAR/RIDGE (extends Prior-4 presence pin; not reopened); pending dual stamp |
| P35 | delta RT `a0960860` / `f0a5fe98` HIGH | D3 short AND / D2-D11 short “package” alias ≠ TRANSPORT | **OPEN/PINNED** — D2/D11=components only; sole close `D3⇔SWEEP_TRANSPORT_CLOSE`; short 4-AND / package PASS REJECT; pending dual stamp |
| P36 | re-CC `3f421a4a` / `1e6f969e` HIGH | §4.1 mono semantics prose-only / soft L substitute | **OPEN/PINNED** — `§4.1_MONO_FIELDS`: ∀b `side_valid=false` ∧ closing mid ∈ `{mid_psd_db, mid_shape_db, mid_prominence_db}` only; `side_*` excluded; pending dual stamp |
| P37 | delta RT `bd16cc8f` / re-CC `98755492` HIGH | COMPLETE soft TSTAR / `source_time_selected` ≠ `t★_nearest` on REPORT | **OPEN/PINNED** — COMPLETE⇒ check `source_time_selected==t★_nearest` (never `:=`) ∧ `t_star=source_time_selected` ∧ named `TSTAR_LAW_OK`/`RIDGE_LAW_OK`; unmatched→FAIL; pending dual stamp |
| P38 | delta RT `bd16cc8f` / re-CC `98755492` HIGH | COMPLETE allows `¬match_ok` / soft L missing | **OPEN/PINNED** — COMPLETE⇒`match_ok=true`∧`mid_valid=true`∧`L=mid_psd_db[b_star]`∧hop≤H/fs_c; ¬match_ok→¬COMPLETE→¬TRANSPORT; pending dual stamp |
| P39 | re-CC `98755492` CRITICAL/HIGH | §6.1 :=-mirror / `f★:=f(t★)` identity tautology; EQ_PUBLISH ATRL-used on REPORT | **OPEN/PINNED** — §6.1 HARD: publish independent + check-only RIDGE/TSTAR; EQ_PUBLISH CLOSE used≡publish / REPORT publish-vs-law only; pending dual stamp |
| P40 | re-CC `98755492` HIGH | COMPLETE vacuous / under-floor REPORT_SET; duplicate keys | **OPEN/PINNED** — `\|REPORT_SET\|≥3` ∧ `{20,45,20000}⊆REPORT_SET` ∧ keys biject REPORT_SET×GATE_SR; duplicate→FAIL; pending dual stamp |
| P41 | re-CC `1577733c` CRITICAL / delta RT `68a39a49` HIGH | `source_time_selected:=t★_nearest` vacuous TSTAR; false `f★:=f(t★)⇒¬RIDGE` | **OPEN/PINNED** — check-equality only; RIDGE check-only + FORBIDDEN assign-then-check; pending dual stamp |
| P42 | delta RT `68a39a49` HIGH | filter-first useful nearest ≠ §13.1 abs; unbound tie | **OPEN/PINNED** — abs nearest then useful ⊆; tie→FAIL∨`min(frame_index)`; pending dual stamp |
| P43 | delta RT `68a39a49` HIGH | PCM “layout not pinned” / planar soft | **OPEN/PINNED** — sole `(N,)` / `(N,C)`; other→FAIL; pending dual stamp |

---

## 12. Handoff

| Lane | Ask |
|------|-----|
| Independent CC / guardian | **Fresh re-CC** on D1 LOCKED + `source_time_selected==` (not `:=`) + RIDGE check-only / FORBIDDEN assign-then-check + EQ_PUBLISH split + §13.1 abs nearest (+useful after; tie) + sole PCM layouts + COMPLETE TSTAR/match_ok/floor/bijection + RIFF `wav_channels` + sole `D3⇔TRANSPORT`; no SOUND / no measure without dual non-POROUS stamps; P28/P33–P43 remain OPEN/PINNED until then |
| `ember-metrology-redteam` | Delta-attack `:=` mirrors / filter-first useful nearest / unbound tie / planar PCM / assign-then-check RIDGE; leftover NOTES only unless new CRITICAL |
| `ember-phase-builder` | Idle on code until GO post-CC |
| `ember-parity-lab` | No measure until authorized |

**§A (one-line):** D1 LOCKED §4.2; `source_time_selected==t★_nearest` (never `:=`); RIDGE check-only + FORBIDDEN `f★:=f(t★)` then check; EQ_PUBLISH CLOSE used≡publish / REPORT publish-vs-law; §13.1 abs nearest→useful ⊆; tie FAIL∨min(frame_index); sole PCM `(N,)`/`(N,C)`; P28/P33–P43 OPEN/PINNED; ¬self-SOUND; **MEASURE_AUTHORIZED NO**.

**END PROPOSAL — not self-SOUND.**
