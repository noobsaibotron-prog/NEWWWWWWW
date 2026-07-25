# REV7 proposal — `ACTIVE(b, ch)` for §13.2 gate 4

**Status:** FORMULA PROPOSAL — REDTEAM CLOSURE DRAFT (untainted writer; a priori geometry/window only)  
**Authority for task:** `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_MANDATE.md`  
**Open amend vehicle (reference only):** `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`  
**Does not edit:** CONTRACT freeze REV6, metrology lock, SHA256SUMS, `Source/`  
**Does not contain:** run margins, max|Δ|, failing band/asset lists, WS4 evidence  
**Re-measure:** forbidden until delta redteam returns non-BROKEN on this prose

---

## 1. Physical question (operative, scoped)

**Normative perimeter of this amend (constraint 4 narrowed — see §2.0):**

Gate-4 **admission** answers only:

1. **ENBW aperture:** is the triangular support wide enough, on every fusion
   path with positive weight, to contain at least one Hann ENBW of in-support
   bins? (`N_BINS ≥ N_MIN = 2`)
2. **Floor / union:** is the cell strictly above the §6.1 clamp floor on the
   reference **or** under-test render?

Cells that fail (1) are **outside the gate-4 SR-parity domain** (domain amend).
Cells that fail (2) are inactive (floor).

**Explicitly out of scope for `ACTIVE`:** whether in-support content dominates
out-of-support **leakage** from strong neighbours. That is a different physical
question; this amend does **not** claim an anti-leakage operator. Constraint 4
of the candidate is hereby narrowed to the ENBW-aperture + floor-union perimeter
above. A residual disclaimer is not the normative statement — §2.0 is.

---

## 2. Contract-language definition

Let `fs_c = 48000`. Analysis geometry is identical for every host sample rate.
Band centres and triangular supports are those of §6.1. Fusion weights
`w_LF(f)`, `w_MAIN(f)` are those of §6.2. Grids: MAIN `N_MAIN = 4096`,
LF `N_LF = 8192`. Window: periodic Hann of §6.2.

### 2.0 Normative constraint-4 perimeter (ENBW + floor only)

**Normative (replaces any broader “leakage-dominated exclusion” reading of
candidate constraint 4 for this amend):**

> Gate-4 cell admission is exactly ENBW-aperture resolvability on every
> positive-weight fusion path (§2.2–2.3) conjoined with the floor/union cut
> (§2.4). Inter-component leakage / sidelobe domination is **out of scope**
> for the `ACTIVE` predicate and for the geometric domain carve-out. No
> a-priori anti-leakage operator is introduced in REV7 by this proposal.

### 2.1 Bin occupancy (pure geometry; bit-stable)

For band index `b ∈ {0,…,119}` and FFT length `N ∈ {N_LF, N_MAIN}`:

```text
Δf(N)        = fs_c / N
lo(b), hi(b) = triangular support of b on log2(f)
               (virtual centres at ratio r outside 0 and 119; §6.1)
N_BINS(b, N) = #{ k ≥ 1 : lo(b) < k·Δf(N) < hi(b) ∧ k·Δf(N) ≤ 20000 }
```

DC and bins above 20000 Hz remain excluded (§6.1).

**Bit-stable / hashable evaluation procedure (normative):**

1. Centres and virtual endpoints: closed form of §6.1 in IEEE-754 binary64,
   same expressions as `ml_v3/contracts` centre helpers (no measured tables).
2. Support edges `lo(b)`, `hi(b)`: evaluate in binary64 from those centres.
3. Bin loop: integer `k` from `1` to `floor(20000/Δf(N))` inclusive; admit `k`
   iff `lo(b) < k·Δf(N)` and `k·Δf(N) < hi(b)` and `k·Δf(N) ≤ 20000`, with
   products `k·Δf(N)` in binary64 (`Δf = fs_c/N` in binary64).
4. `N_BINS(b,N)` is the cardinality of that set (non-negative integer).
5. Canonical mask bytes (for lock / SHA256SUMS on consolidate): concatenate,
   for `b = 0..119` in order, the bits
   `RESOLVED(b)`, then for each valid channel role the per-cell activity bits
   as packed in the consolidate lock schema — all bits derived only from
   (1)–(4), §2.2–2.4, and the PSD floor test on the two renders. No run-time
   float tolerance beyond binary64 evaluation of the predicates above.

### 2.2 Material fusion paths (worst-case: any positive weight)

A fusion path is **material** for band `b` when its weight at `center[b]` is
strictly positive. Any path with `w > 0` can dominate the fused power under
adversarial energy ratios; equal-energy bounds are **not** used.

```text
MATERIAL_LF(b)   ⇔  w_LF(center[b])   > 0
MATERIAL_MAIN(b) ⇔  w_MAIN(center[b]) > 0
```

(At least one of the two always holds because `w_LF + w_MAIN = 1` with
non-negative weights. Pure-LF bands have `w_MAIN = 0`; pure-MAIN have
`w_LF = 0`; crossfade bands have both material.)

The immutable gate threshold `0.25 dB` is **not** an input to MATERIAL. It
remains only the max-|Δ| hard threshold of §13.2.

### 2.3 Geometric resolvability

Hann periodic ENBW = `1.5` bins (frozen window property; §3). ENBW is the
width of a rectangular filter that collects the same noise power as the
analysis window: it is the noise-equivalent aperture of one DFT bin
measurement under Hann. For a triangular support average to be an
**ENBW-aperture-resolved** measurement, the open support must be wide enough
to contain at least one full ENBW of aperture **inside** the support.

The smallest integer bin occupancy with width ≥ `ENBW_Hann` is therefore:

```text
N_MIN = ceil(ENBW_Hann) = ceil(1.5) = 2

RESOLVED(b) ⇔
    ( ¬MATERIAL_LF(b)   ∨ N_BINS(b, N_LF)   ≥ N_MIN )
  ∧ ( ¬MATERIAL_MAIN(b) ∨ N_BINS(b, N_MAIN) ≥ N_MIN )
```

This criterion does **not** claim main-lobe isolation (null-to-null width) and
does **not** claim leakage immunity (§2.0).

### 2.4 Floor / union (one-sided artifact intent)

With the §6.1 clamp domain and linear floor `1e-12`:

```text
ABOVE_FLOOR(b, ch) ⇔
    max( psd_db_ref[b, ch], psd_db_sr[b, ch] ) > -120
```

Strict inequality: cells stuck on the clamp floor are inactive. The `max`
(union across reference 48 kHz and under-test SR) preserves gate-4 intent that
an artifact present on only one side remains eligible for the max |Δ|.

### 2.5 Domain amend for unresolvable bands (sole normative packaging for 0/1-bin)

**Normative packaging (domain amend — not optional, not dual):**

> Band indices `b` with `RESOLVED(b) = false` under §2.2–2.3 (equivalently:
> material-path bin occupancy `N_BINS < 2` on at least one path with
> `w_path(center[b]) > 0`) are **outside the gate-4 SR-parity domain** for
> PSD, shape, and prominence on every channel. They do not contribute cells
> to the gate-4 max |Δ|.

**Equivalence of the max cell set (stated, not optional dual form):**

Let `D_domain` be the set of gate-4 cells after applying the domain sentence
above and then admitting PSD cells by `ABOVE_FLOOR` only inside the remaining
band set. Let `D_pred` be the set of cells with
`ACTIVE(b,ch) ⇔ RESOLVED(b) ∧ ABOVE_FLOOR(b,ch)` under §2.6, with shape /
prominence inheritance as amended in §2.7. Then `D_domain = D_pred` as sets
of `(field, b, ch)` cells entering the max. Implementations may compute via
the predicate; the **normative prose form** for 0/1-bin exclusion is the
domain sentence, not a second parallel “optional” wording.

There is **no** principled amplitude rule that can stably *admit* bands with
material occupancy `N_BINS ∈ {0,1}` as ENBW-aperture-resolved cells (§4).

### 2.6 Predicate (implementational form; set-equivalent to §2.5)

For a PSD cell on channel `ch ∈ {mid, side}` with that channel valid (§7):

```text
ACTIVE(b, ch) ⇔ RESOLVED(b) ∧ ABOVE_FLOOR(b, ch)
```

- Scalars `mid_level_dbfs` / `side_level_dbfs` stay in the gate domain when the
  respective channel is valid; they are not band-indexed and do not use
  `RESOLVED(b)`.
- Channels with `*_valid == false` are ignored (§7); their vectors do not
  contribute cells.

### 2.7 Shape / prominence coupling — choice (B) (CRITICAL)

**Problem:** §7 defines

- `shape_db` = PSD minus `10*log10(sum(10**(psd_db/10)))` over **all 120** bands;
- `prominence_db` = shape minus a Gaussian convolution on the log-band axis,
  kernel `j ∈ [-16..16]`, on that shape vector.

Excluding cell `(b,ch)` from the max via `ACTIVE` / domain carve-out does
**not** remove band `b`’s energy from those formulas. Predicate-only
“inherit ACTIVE” therefore leaves excluded-band energy able to move ACTIVE
cells’ shape/prominence — a false-PASS surface if those contaminants are
omitted from the max while still driving ACTIVE neighbours.

**Choice (A) rejected for closure:** keeping above-floor unresolved bands in
the max for shape/prominence measures the contaminant cells themselves but
does **not** stop their energy from altering ACTIVE neighbours’ shape /
prominence through the global sum and ±16 kernel. It is fail-closed for the
sparse cells’ own fields, not a coupling close.

**Choice (B) — normative (domain + gate-4 field redefinition):**

> For **gate-4 evaluation only**, let `R = { i ∈ {0,…,119} : RESOLVED(i) }`.
> Gate-4 `shape_db[b,ch]` and `prominence_db[b,ch]` are the §7 formulas with
> support restricted to `R`:
>
> - shape normalizer sums `10**(psd_db[i,ch]/10)` only over `i ∈ R`;
> - prominence kernel includes only offsets `j` with `b+j ∈ R` (renormalize
>   the retained kernel weights to sum one; reflect padding is applied only
>   within the `R`-indexed sequence in band order).
>
> Bands with `b ∉ R` are outside the gate-4 shape/prominence domain (no cells).
> For `b ∈ R`, shape/prominence are active in the max iff `ACTIVE(b,ch)` on
> that channel’s PSD (i.e. also `ABOVE_FLOOR`).
>
> **Testable statement:** mutating `psd_db[u,ch]` for any `u ∉ R` must leave
> every gate-4 `shape_db[b,ch]` and `prominence_db[b,ch]` for `b ∈ R`
> unchanged (bit-identical under the same binary64 reduction order as the
> consolidate harness). Mutating `psd_db[u,ch]` for `u ∈ R` may change those
> fields exactly as §7-on-`R` predicts.

Product §7 vectors used by other gates are untouched by this sentence; only
the gate-4 comparison domain and the fields entering the gate-4 max use the
`R`-restricted definitions.

Honesty note: closing coupling requires this **domain / derived-field amend**
for gate 4; a predicate-only mask on the existing §7 full-120 fields cannot
make the testable statement true.

### 2.8 Vacuous FAIL (fail-closed; no N/A skip)

**Normative:**

> For every asset/portion required by §13.1, and for every channel `ch` that
> is valid for that portion (`mid_valid` / `side_valid` as applicable), if the
> set of active gate-4 cells on that (portion, channel) is empty — counting
> PSD/shape/prominence cells under §2.5–2.7 and level scalars when in domain —
> then gate 4 is **FAIL** for that portion. Empty domain ≠ PASS. Implementations
> must not skip a required (portion, valid channel) with N/A, soft-pass, or
> “no cells to compare.”

Threshold `0.25 dB` and aggregator `max` over the declared gate-4 dB domain
remain immutable. No post-hoc band mask after seeing errors.

---

## 3. Per-constant derivation (frozen window / grid / contract only)

| Constant | Value | Derivation |
|---|---|---|
| `fs_c` | `48000` | §6 / lock timing |
| `N_MAIN` | `4096` | §6.2 / lock |
| `N_LF` | `8192` | §6.2 / lock |
| `Δf(N)` | `fs_c/N` | FFT bin spacing |
| centres / `r` | §6.1 closed form | `center[i]=20*(20000/20)**(i/119)`; endpoints pinned |
| triangular support | `center[b-1]..center[b+1]` | §6.1; virtual ends via same `r` |
| fusion split | `160` / `320` Hz | §6.2 pure LF / pure MAIN |
| `w_LF`, `w_MAIN` | raised-cosine on `log2` | §6.2 formula |
| window | periodic Hann `0.5-0.5*cos(2πn/N)` | §6.2 |
| `ENBW_Hann` | `1.5` bins | classical ENBW of periodic Hann (noise-equivalent bandwidth of one DFT bin under the frozen window) |
| `N_MIN` | `2` | `ceil(ENBW_Hann)` — minimum integer occupancy whose support width ≥ one Hann ENBW; guarantees the noise-equivalent aperture of the band average can lie inside the triangular support (see §2.3). Occupancy `1 < 1.5` fails that coverage; main-lobe null-to-null width is not used |
| PSD floor linear | `1e-12` | §6.1 / lock |
| activity floor dB | `-120` | `10*log10(1e-12)` after §6.1 clamp lower edge |
| `ABOVE_FLOOR` cut | `> -120` | strict above clamp floor; union via `max(ref,sr)` preserves one-sided artifacts |
| gate threshold | `0.25` dB | §13.2 immutable — **not** used to define MATERIAL |
| MATERIAL rule | `w_path(center[b]) > 0` | worst-case: any positive-weight path can dominate fused power under adversarial energy ratios; equal-energy `W_MATERIAL` bound withdrawn |

No constant above is taken from a measured max|Δ|, margin table, or band-failure list. `W_MATERIAL = 1 - 10**(-0.25/10)` is **removed** from this proposal.

### 3.1 A priori occupancy consequence (arithmetic, not a run)

Crossing triangular supports with the two grids (mandate § “Conseguenza geometrica”) yields on MAIN: 14 bands with `N_BINS=0`, 21 with `N_BINS=1`; on LF: 6 with `0`, 17 with `1`. Under `RESOLVED` with `N_MIN=2` and `w > 0` materiality, the non-resolvable set is exactly the bands whose **material** occupancy is `0` or `1` (low-frequency edge; in the crossfade, both grids must clear `N_MIN`). That identity is a check of the formula against geometry, not a fit to errors.

---

## 4. Why 0/1-bin bands stay outside the domain

**Claim (honest):** there is **no** principled amplitude rule that can stably
*admit* bands with material occupancy `N_BINS ∈ {0,1}` as ENBW-aperture-resolved
cells under §2.3.

Reasons (geometry / window only — ENBW, not main-lobe isolation):

1. **Zero bins.** No FFT bin lies inside the open triangular support on the
   material path. Support width is `0 < ENBW_Hann`; the band average has no
   in-support content. Any finite dB value is empty-support / floor behaviour,
   not a measurement of in-band spectrum.
2. **One bin.** Support width is `1` bin. Hann ENBW is `1.5` bins, so
   `1 < ENBW_Hann`: the noise-equivalent aperture of the single DFT bin
   measurement is wider than the triangular support. By the ENBW definition,
   out-of-support content is required to fill that aperture; the cell cannot
   be ENBW-aperture-resolved. Raising the level threshold cannot widen the
   support to ≥ `ENBW_Hann`.

Therefore the sole normative packaging is the **domain amend** of §2.5
(`N_MIN = ceil(ENBW_Hann) = 2` kept). Bands with material `N_BINS ≥ 2` remain
eligible; admission inside the domain is then only `ABOVE_FLOOR` (union).

---

## 5. Explicit non-goals / non-claims

- This note does **not** claim gate PASS and does **not** report a re-measure.
- Re-measure after adoption is mandatory (mandate constraint 8) and is out of
  scope for this writer; it remains **forbidden** until delta redteam clears
  the closure surface.
- §10 evaluator criteria (`−100` dBFS/Hz, etc.) are untouched.
- Replacing the REV6 string `max(psd_db_ref, psd_db_sr) > -120` is intentional;
  the floor/union factor is retained; geometric domain / `RESOLVED` is added;
  gate-4 shape/prominence are redefined on `R` (§2.7).
- No sidelobe-vs-neighbour dB cut is proposed: mapping Hann’s first sidelobe
  (`≈ −31.5` dB in bin space) onto band-index neighbours is not an isomorphism
  fixed by §6 alone, so it is rejected as a constant source here — and, under
  §2.0, leakage exclusion is out of scope for ACTIVE rather than half-solved
  by a weak residual.
- Main-lobe null-to-null width (`4` bins for periodic Hann) is **not** used to
  set `N_MIN`. That isolation argument would force `N_MIN ≥ 4` and is a
  different physical claim; this proposal commits only to ENBW aperture
  coverage (`N_MIN = ceil(1.5) = 2`).
- Forbidden: raise `N_MIN` to chase PASS; use run margins; edit CONTRACT freeze /
  lock / SHA256SUMS from this note.

---

## 6. Packaging sketch for the candidate (normative intent for consolidate prose)

Contract-shaped replacement for the gate-4 activity / domain block:

> **Domain (0/1-bin):** Band indices with `RESOLVED(b) = false` —
> `RESOLVED(b)` iff every fusion path with `w_path(center[b]) > 0` has
> `N_BINS(b, N_path) ≥ 2 = ceil(ENBW_Hann)` — are outside the gate-4
> PSD/shape/prominence domain.
>
> **PSD activity** on remaining bands: `ACTIVE(b,ch) ⇔ ABOVE_FLOOR(b,ch)` with
> `max(psd_db_ref, psd_db_sr) > -120` (equivalently
> `RESOLVED(b) ∧ ABOVE_FLOOR` over all `b`, same max cell set).
>
> **Gate-4 shape/prominence:** §7 formulas restricted to
> `R = {i : RESOLVED(i)}` (normalizer and ±16 kernel); test: PSD outside `R`
> does not change gate-4 shape/prominence on `R`. Active in the max iff
> `ACTIVE(b,ch)`.
>
> **Constraint-4 perimeter:** admission = ENBW aperture + floor/union only;
> leakage domination out of scope for ACTIVE.
>
> **Vacuous:** empty active set on any §13.1 portion × valid channel → FAIL.
> Max aggregator and `0.25` dB unchanged. Mask bits from §2.1 procedure.

---

## 7. Writer contamination statement

Allowed inputs used: mandate; CONTRACT §6/§7 structure; `ml_v3/contracts/`
centre helpers / constants; `metrology_lock.json` non-outcome fields (FFT
sizes, floor, clamp, fusion timing); candidate scope-reopen / must-fix list
wording (no §1 trigger tables). No WS4 evidence files, no `sr_parity.py`,
no agent transcripts, no REV7 candidate §1 trigger tables, no run reports
listing max|Δ| or failing bands were opened for this formula.

---

## 8. Scope statement (normative, not a residual hedge)

Independent coherence judgment previously noted that ENBW-only is narrower
than “leakage-dominated exclusion.” That narrowing is now **normative §2.0**,
not a §8 disclaimer. The formula is internally consistent under the ENBW +
floor perimeter. Leakage sufficiency is not claimed; an anti-leakage operator
is not smuggled via residual text.

Re-measure (when authorized) decides PASS/FAIL under this scoped rule — not
this note.

---

## 9. Pre-registered governance (before any re-measure)

Frozen **before** re-measure so the outcome cannot rewrite the rule:

1. If re-measure **FAILS** under `N_MIN = 2`, it is **forbidden** to raise
   `N_MIN` (or otherwise retune constants) until the gate appears to PASS.
   That would be threshold shopping.
2. Admissible responses to FAIL only:
   - a re-derivation from a **different** frozen window/grid property,
     justified on its own terms (not by the FAIL magnitudes); or
   - a further **domain** / derived-field amend authorized by a new GO —
     not silent threshold edits.
3. Sequence: **delta metrology-redteam on this closure prose** → then
   independent re-measure from scratch (only if non-BROKEN) → then second
   Guardian GO. No consolidate while this proposal is uncommitted / unaccepted.
4. `0.25 dB` is immutable as the gate threshold, not a shopping knob for
   MATERIAL or `N_MIN`.

---

## 10. Redteam closure (must-fix 1–5)

| # | Must-fix | Closure in this prose |
|---|---|---|
| 1 | Shape/prominence coupling (CRITICAL) | **Choice (B).** §2.7 redefines gate-4 shape/prominence on `R = {RESOLVED}`; testable bit-stability vs PSD outside `R`. Choice (A) rejected as insufficient to stop contamination of ACTIVE neighbours. Coupling closed only by domain/derived-field amend — stated honestly. |
| 2 | Constraint 4 perimeter | **Narrowed yes (normative).** §2.0: admission = ENBW-aperture + floor-union only; leakage out of scope for ACTIVE. No anti-leakage operator added. §8 is no longer a hedge — it restates the normative perimeter. |
| 3 | Single form for 0/1-bin | **Domain amend xor** (domain is the sole normative packaging). §2.5; predicate `RESOLVED=false` is implementationally set-equivalent (§2.5 equivalence), not an optional second contract sentence. Dual “optional” wording removed. |
| 4 | MATERIAL worst-case | **`w_path > 0` must be resolved** (§2.2). Equal-energy `W_MATERIAL` withdrawn. `0.25 dB` not used in MATERIAL; remains gate threshold only. |
| 5 | Pin + vacuous | §2.1 bit-stable `N_BINS`/`RESOLVED`/`ACTIVE` mask procedure; §2.8 vacuous FAIL per §13.1 portion **and** per required valid channel; no N/A skip. |

`N_MIN = 2` retained (`ceil(ENBW_Hann)`); not raised.
