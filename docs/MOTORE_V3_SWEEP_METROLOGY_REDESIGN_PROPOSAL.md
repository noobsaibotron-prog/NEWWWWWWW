# Motore v3 — Proposal: SWEEP METROLOGY REDESIGN (S1 + S2)

**Suggested commit title:**  
`docs(v3): bind MONO_ASSET to fixture_spec channels`

| Field | Value |
|-------|--------|
| **Status** | **PROPOSAL DRAFT** — document-only; D1 **LOCKED**; delta RT `9ce35909` HIGH pins (`RIDGE_MATCH`/`MONO_ASSET`/`SIDE_PAIR_OK`/`B_OFF`); **not** self-SOUND (needs fresh re-CC+RT); **MEASURE_AUTHORIZED NO** |
| **≠** | G1 PASS · ACCEPT · measure · REV7 consolidate · G1b tip · CONTRACT/lock/T6 edit · A3 reopen · A4 ACTIVE · silent §13.2.4 / gate-4 equivalence · self-SOUND |
| **Date** | 2026-07-26 |
| **Authority** | Marco authorize docs-only `MONO_ASSET` bind + interrupt merge delta RT `9ce35909` HIGH |
| **Mandate** | `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` (incl. stationary≠trajectory pin) |
| **A3 status (status only)** | **RETIRED AS SOLUTION** — `docs/MOTORE_V3_LOG_SWEEP_A3_ARCHIVE_STAMP.md` |
| **Freeze structure (read-only)** | `docs/MOTORE_V3_G1_CONTRACT.md` REV6 @ `6d254d0a`; lock / `fixture_spec` **structure** as frozen quantities |
| **Prior tip** | `70cb61cc` (veto ∀/GLOBAL + max≡∀ prose; residual POROUS — D1 still PROPOSED + delta RT `765045fb` HIGH) |

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
| D1 | S1 = **guard regions** on checkpoint closing domain, from aperture geometry only | **LOCKED** — §4.2 geometry (`T_LEAD`/`T_TRAIL`, `T_MEM=0`, REPORT/CLOSE); S1 locked ≠ package self-SOUND; **MEASURE** still **NO** until dual non-POROUS stamps |
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
5. Per path, let \(b^\star_{\mathrm{path}}\) = the unique band
   \(b\in\{0,\ldots,119\}\) whose triangular support on `log2(f)` contains
   that path’s \(f^\star\) (§6.1).  
   - If \(f^\star\) lies exactly on a shared edge: pin
     **lower-index band** (fail-closed deterministic tie-break).  
   - If \(f^\star\) outside \([20,20000]\): FAIL (should not occur inside
     close-eligible set).
6. **Common ridge band (fail-closed, per cell \(c\in D\)):** evaluate
   `RIDGE_MATCH(c) ⇔ b★_ref(c)=b★_sr(c)` (also a conjunct of `ATRL_OK` and
   `SWEEP_TRANSPORT_CLOSE` — not procedure-only). If \(\neg\texttt{RIDGE\_MATCH}(c)\)
   → **FAIL**. The common index is written \(b^\star\) and is the **sole**
   ridge band for that cell’s ATRL \(L\), companions, side-gate neighbours,
   and \(B_{\mathrm{OFF}}:=\mathrm{complement}(\{b^\star\})\) / veto operands.
   Independent per-path \(b^\star\) shopping is **REJECT**.
7. Observable: \(L = \texttt{mid_psd_db}[b^\star]\) after frozen fusion/floor/clamp
   — **one scalar per path** at that checkpoint (not a multi-band / multi-channel
   Cartesian product).
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
# Per close cell c ∈ D (interior CHK_CLOSE under D1 ⇒ candidate bands ∈ {1,…,118}):
RIDGE_MATCH(c)  ⇔  b★_ref(c) = b★_sr(c)
# Normative algebra (not procedure-only). ¬RIDGE_MATCH(c) → FAIL.
# When RIDGE_MATCH(c): b★(c) := that common index (sole ridge for L/companions/neighbours/B_OFF).

REQUIRED_ACTIVE(c)  ⇔  RIDGE_MATCH(c) ∧ ACTIVE(b★) ∧ ACTIVE(b−) ∧ ACTIVE(b+)
# ¬REQUIRED_ACTIVE(c) → FAIL (no silent neighbour / ridge skip)

# Off-ridge = complement of the common ridge index (see §6.2):
B_OFF(c)         :=  complement({b★(c)})   # ≡ {0,…,119} \ {b★}; requires RIDGE_MATCH
B_OFF_ACTIVE(c)  :=  { b ∈ B_OFF(c) : ACTIVE(b) }
# B_OFF_ACTIVE(c) = ∅ → FAIL   (vacuous off-ridge veto / empty-union soft forbidden)
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
# Cross-check FAIL (not alternate norms): under test, asset_manifest.channels
# must equal that same integer; CONTRACT §4.1 mono rule
# (mid = input; side not valid) must hold for MONO_ASSET assets.
# Mismatch among fixture_spec / asset_manifest / §4.1 mono semantics → FAIL.
# SIDE_PAIR both-false trivial ONLY when MONO_ASSET under this pin; else FAIL.

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

SIDE_PAIR_OK(c)  ⇔
    ( SIDE_BOTH_FALSE(c) ∧ MONO_ASSET )
    ∨  ( SIDE_BOTH_TRUE(c) ∧ SIDE_DELTA_OK(c) )
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

# Normative close — SIDE_PAIR_OK is a separate conjunct; δ-alone ≠ COMPANIONS_OK:
COMPANIONS_OK  ⇔  ∀ c ∈ D : SIDE_PAIR_OK(c) ∧ δ_companions(c) ≤ 0.25 dB
               ⇔  ∀ c ∈ D : COMPANIONS_OK(c)
# Also: max_{c∈D} δ_companions(c) ≤ 0.25 is necessary for the δ conjunct,
# but **not** sufficient alone (does not imply SIDE_PAIR_OK / COMPANIONS_OK).
# Forbidden: claiming δ-alone ≡ COMPANIONS_OK.
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

**ATRL level rule (component — includes RIDGE_MATCH in algebra):**

```text
ATRL_OK(c)  ⇔  RIDGE_MATCH(c) ∧ |L_ref − L_sr|(c) ≤ 0.25 dB
              where L = mid_psd_db[b★]   (defined only under RIDGE_MATCH; one scalar/path)

ATRL_OK  ⇔  ∀ c ∈ D : ATRL_OK(c)
         ⇔  (∀ c ∈ D : RIDGE_MATCH(c))
            ∧  max_{c ∈ D} |L_ref − L_sr|(c) ≤ 0.25 dB
# ∀ and max are definitionally equivalent on the level conjunct once RIDGE_MATCH
# holds on all cells. mean / p95 / RMSE forbidden.
# Forbidden: treating RIDGE_MATCH as procedure-only outside ATRL_OK / TRANSPORT.
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
| S2-R16 | Soft max fold / mean/p95 of companion \|Δ\|; “when binding” optional max | **REJECT** — δ:=max of required \|Δ\|; max≡∀ |
| S2-R17 | Omit either pair in PAIRS / shrink D below `{CHK_CLOSE}×PAIRS` | **REJECT** — omit pair → FAIL |
| S2-R18 | Treat both `side_valid=false` as trivial on stereo / non-mono | **REJECT** — trivial iff `MONO_ASSET ⇔ fixture_spec.categories.log_sweep.channels==1` |
| S2-R19 | Independent \(b^\star_{\mathrm{ref}}\neq b^\star_{\mathrm{sr}}\) / per-path ridge shopping | **REJECT** — `¬RIDGE_MATCH` → FAIL (in ATRL_OK / TRANSPORT) |
| S2-R20 | Claim δ-alone ≡ `COMPANIONS_OK` (drop `SIDE_PAIR_OK`) | **REJECT** — `COMPANIONS_OK ⇔ ∀c SIDE_PAIR_OK ∧ δ≤0.25` |
| S2-R21 | Soft-skip null/missing component in `Req(c)` when folding δ | **REJECT** — null→FAIL |

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
# Frozen triangular partition (§6.1): off-ridge = complement of common ridge
B_OFF(c)   :=  complement({b★(c)})           # ≡ {0,…,119} \ {b★}; needs RIDGE_MATCH
B_OFF(f★)  :=  B_OFF(c)                      # alias (f★ labels the cell’s ridge)

# Classical periodic Hann (structure only; not run-fitted):
ENBW_HANN_BINS          = 1.5
MAIN_LOBE_HALF_BINS     = 2          # centre → first null
# Off-ridge obligation is non-vacuous: |B_OFF| = 119 ≥ ceil(ENBW) and
# triangular supports exist outside b★. These constants justify the set;
# they do **not** shrink the veto domain below B_OFF.

B_GEOM(f★) := B_OFF(f★)             # full off-ridge index set
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
    ∧  ∀c∈D : RIDGE_MATCH(c)           # algebra (also inside ATRL_OK / REQUIRED_ACTIVE)
    ∧  ATRL_OK                         # §5.3 ∀c∈D : RIDGE_MATCH ∧ |ΔL|≤0.25; max≡∀
    ∧  COMPANIONS_OK                   # §5.3 ∀c∈D : SIDE_PAIR_OK ∧ δ≤0.25 (δ≠alone)
    ∧  VETO_OK                         # §6.2 ∀c∈D ¬FIRE(c) ⇔ ¬FIRE_GLOBAL
    ∧  SIDE_GATE_OK                    # §5.3 ∀c∈D; FIRE_GLOBAL=false
    ∧  CHK_REPORT_PUBLISH_COMPLETE     # §4.5
    ∧  ∀c∈D : REQUIRED_ACTIVE(c)       # RIDGE_MATCH∧ACTIVE(b★∧b±); §5.3
    ∧  ∀c∈D : B_OFF_ACTIVE(c) ≠ ∅      # B_OFF:=complement({b★}); empty → FAIL
    ∧  no mid_valid=false / unmatched / empty-union-on-required FAIL (§5.3)
# D := {CHK_CLOSE} × PAIRS; omit either pair → FAIL
# RIDGE_MATCH appears explicitly here so mismatch cannot be procedure-only.
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
| S1 guards | **D1 LOCKED** — §4.2 `T_LEAD`/`T_TRAIL` geometry; no silent widen |
| Common \(b^\star\) | `RIDGE_MATCH(c) ⇔ b★_ref=b★_sr` in ATRL_OK **and** TRANSPORT; else **FAIL** |
| `MONO_ASSET` | **⇔** `fixture_spec.categories.log_sweep.channels==1`; SIDE_PAIR trivial only then; manifest/§4.1 mismatch → FAIL |
| `SIDE_PAIR_OK` | **⇔** `(BOTH_FALSE∧MONO_ASSET) ∨ (BOTH_TRUE∧SIDE_DELTA_OK)`; XOR / non-mono both-false → false |
| `B_OFF` | **:=** `complement({b★})` |
| \|CHK_CLOSE\| | **≥7**; empty → FAIL |
| Off-ridge close | ATRL_OK **∧** COMPANIONS_OK **∧** VETO_OK **∧** SIDE_GATE_OK; report≠close |
| COMPANIONS_OK | **⇔** ∀c `SIDE_PAIR_OK` ∧ δ≤0.25; δ-alone **≠** COMPANIONS_OK |
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
| S1 guard direction | **LOCKED** (D1) on §4.2 geometry + cardinality / REPORT fail-closed — S1 locked ≠ package self-SOUND; **MEASURE** still **NO** until dual non-POROUS stamps |
| S2 package | **LOCKED menu** — ATRL + CONTRACT companions + executable veto (+ side-gate AND); S2-ALT\* archival |
| Off-ridge vanishing | **NO** — close = ATRL_OK ∧ COMPANIONS_OK ∧ VETO_OK ∧ SIDE_GATE_OK (+ REQUIRED_ACTIVE ∧ B_OFF_ACTIVE≠∅) |
| §13.2.4 / gate-4 equivalence | **NO** — not gate-4 equivalent until REV7 (S2-R14) |
| Self-SOUND / MEASURE | **NO** — not self-SOUND (fresh re-CC+RT required); **MEASURE_AUTHORIZED NO** |
| Next lab action | fresh re-CC+RT on D1 lock + companions/b★/mono/Req pins → only then measure auth (still NO until dual non-POROUS) |

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
| P9 | RT / re-CC CRITICAL | Open S2/alt menu | **CLOSED** — D11 freeze ATRL+package; S2-ALT\* archival |
| P10 | RT MED | Deep LF-per-checkpoint aperture | **NOTE only** — §4.6 deferred; not redesigned here |
| P11 | re-CC + delta RT | Never-fire / unevaluated veto | **CLOSED** — missing operands → FIRE/FAIL; formula pinned §6.2 |
| P12 | delta RT `8aa8027c` HIGH | Empty-union soft; missing neighbour activity; empty B_OFF_ACTIVE | **CLOSED** — `REQUIRED_ACTIVE ⇔ ACTIVE(b★)∧ACTIVE(b±)`; `B_OFF_ACTIVE=∅ → FAIL`; both enter §6.3 close |
| P13 | re-CC residual / delta CRITICAL | COMPANIONS_OK / SIDE_GATE_OK soft/single-cell aggregator | **CLOSED** — COMPANIONS_OK ⇔ ∀c∈D SIDE_PAIR_OK∧δ≤0.25 on same `D={CHK_CLOSE}×PAIRS`; SIDE_GATE_OK ∀c / FIRE_GLOBAL≡∃; ATRL max≡∀ |
| P14 | re-CC residual | §5.2 “another form may win at CC” open-menu echo vs D11 | **CLOSED** — §5.2 closed on ATRL+package; swap only via fresh untainted amend + CC |
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

---

## 12. Handoff

| Lane | Ask |
|------|-----|
| Independent CC / guardian | **Fresh re-CC** on D1 LOCKED + `RIDGE_MATCH` in ATRL/TRANSPORT + executable `SIDE_PAIR_OK` + `MONO_ASSET`⇔`fixture_spec…channels==1` + `Req(c)`; no SOUND / no measure without dual non-POROUS stamps |
| `ember-metrology-redteam` | Delta-attack companions ⇔ / `RIDGE_MATCH` / mono channels / Req null / `B_OFF` complement; leftover NOTES only unless new CRITICAL |
| `ember-phase-builder` | Idle on code until GO post-CC |
| `ember-parity-lab` | No measure until authorized |

**§A (one-line):** D1 LOCKED §4.2; RIDGE_MATCH in ATRL∧TRANSPORT; COMPANIONS_OK⇔∀c SIDE_PAIR_OK∧δ≤0.25 (δ≠alone); MONO_ASSET⇔fixture_spec.categories.log_sweep.channels==1; B_OFF:=complement({b★}); Req(c) null→FAIL; ¬self-SOUND; fresh re-CC+RT; **MEASURE_AUTHORIZED NO**.

**END PROPOSAL — not self-SOUND.**
