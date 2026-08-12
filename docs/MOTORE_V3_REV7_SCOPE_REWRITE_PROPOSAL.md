# REV7 — Scope rewrite proposal (document-only)

**Status:** PROPOSAL — product packaging = report-only LF (Marco)  
**Date:** 2026-07-25  
**Authority for this write:** `docs/MOTORE_V3_REV7_SCOPE_REWRITE_MANDATE.md`  
**Report-only clause:** `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`  
**Kind:** scope rewrite of the SR-parity admission domain and geometric resolvability predicates + report-only packaging for ∉`R`.  
**Not in scope of this document:** remeasure, lock edits, SHA256SUMS, CONTRACT freeze edits, threshold shopping.

---

## 0. Contamination statement

**CLEAN.**

This write used only:

- the cleaned scope-rewrite mandate;
- geometry / field structure from `docs/MOTORE_V3_G1_CONTRACT.md` §6–§7;
- band-centre helpers in `ml_v3/contracts/grid.py` / `constants.py`;
- lock *non-outcome* timing/floor names (`fs_c`, `N_LF`, hop `H`, `floor_linear`) without reading run tables.

It did **not** open `*REMEASURE*`, `*WS4*EVIDENCE*`, `*REV7_CANDIDATE*`, or `sr_parity` evidence dumps.  
It does **not** know, and does not use, any measured `max|Δ|`, margin table, failing asset/SR list, or measured failing band-index list from a gate run.

Structural band counts below are **a priori** consequences of frozen window↔grid arithmetic, of the same kind as the mandate’s occupancy census (zero-bin / single-bin). They are not run outcomes.

If a reviewer later discovers latent numeric gate-failure knowledge in the author, this proposal is void and must be rewritten by a fresh untainted writer.

---

## 1. Chosen direction (one coherent package)

**Domain amend of admissible set `R`, driven by a main-lobe separation predicate** — not a raise of `N_MIN`, and not a change of the 0.25 dB gate number.

Package:

1. **Keep** the prior ACTIVE under-resolution layer: domain-on-`R` with ENBW occupancy and `N_MIN = 2` (already justified from Hann ENBW = 1.5 bin; **not** raised here).
2. **Add** a second, independent geometric predicate on the same bands: **neighbor centres must lie outside the periodic-Hann main lobe** of a tone at the band centre, evaluated in the FFT bin metric of every path that contributes weight after fusion.
3. **Interpret** failure of (2) as *domain exclusion* (band ∉ `R`), not as evidence to loosen or tighten the dB gate.

### Why this direction

Mandate diagnosis, used only as physics/geometry:

| Layer | Status |
|---|---|
| 0/1-bin under-resolution | Already addressed by ENBW + `N_MIN = 2` on `R` |
| “Sparse fixtures / empty valleys” alone | Tested by dense one-tone-per-centre excitation under the same admission rules; **insufficient** as a complete explanation (no outcome magnitudes used) |
| Residual on bands still ENBW-`RESOLVED` | Neighbor centres can sit **inside** the frozen Hann main lobe → local inter-band inseparability |

ENBW occupancy answers “does this band own enough equivalent noise bandwidth?”  
Main-lobe separation answers “is the next centre even a distinct spectral peak under this window?”  
Those are different questions; passing the first does not imply the second on a log-spaced 120-band grid against MAIN 4096 / LF 8192.

Raising `N_MIN` because a measure failed is **forbidden** and also **misaligned**: the residual is centre-to-centre geometry inside the main lobe, not a thicker occupancy requirement.

---

## 2. Frozen geometry reused (no measure)

From contract §6 and the mandate’s freeze list:

| Symbol | Value | Source |
|---|---|---|
| `fs_c` | 48000 | §6 / lock timing |
| `N_MAIN` | 4096 | §6.2 |
| `N_LF` | 8192 | §6.2 / lock `timing.N_LF` |
| `H` | 1024 | §6.2 / lock |
| Window | periodic Hann `0.5 - 0.5*cos(2πn/N)` | §6.2 |
| PSD floor (linear) | `1e-12` then clamp `[-120, +12]` dBFS/Hz | §6.1 / lock activity floor |
| Centres | `center[i] = 20 * (20000/20)**(i/119)`, `i = 0..119` | §6.1 |
| Ratio | `r = 1000**(1/119)` | equivalent closed form |
| Support | triangular on `log2(f)` | §6.1 |
| Fusion | LF pure ≤160 Hz; MAIN pure ≥320 Hz; raised-cosine crossfade on `log2(f)` in (160, 320) | §6.2 |

Legitimate Hann constants (classical, not fitted to a run):

| Property | Bins |
|---|---|
| ENBW | 1.5 |
| Main lobe null-to-null | 4 |
| Main lobe half-width (centre → first null) | 2 |
| First sidelobe | classical Hann location/level (informational; not used as a gate constant here) |

Bin widths:

```text
Δf_MAIN = fs_c / N_MAIN = 48000 / 4096 = 11.71875 Hz
Δf_LF   = fs_c / N_LF   = 48000 / 8192 =  5.859375 Hz
```

Mandate occupancy census (unchanged, arithmetic only): MAIN 14 zero-bin + 21 single-bin; LF 6 zero-bin + 17 single-bin.

---

## 3. Domain and field definition

### 3.1 Fields under the SR-parity claim

Unchanged surface from §7: the parity claim for this amend continues to address **stationary mid (and side, when valid) `*_psd_db[120]` band energies** after the frozen PSD / fusion / dB path.

Still excluded from the parity domain (lock already names delta exclusion; this proposal does not reopen that list): frame-to-frame `*_delta_db`, and any field whose contract meaning is not a per-band fused PSD level.

**No change** to the numeric gate threshold 0.25 dB.  
**No change** to FFT sizes, window name, floors, fusion knees, grid, or SHA256SUMS in this task.

### 3.2 Admissible set `R`

A band index `i` at a compared frame is in `R` iff **all** of the following hold on every FFT path `p ∈ contributing_paths(i)`:

1. **Under-resolution (prior ACTIVE, retained):** ENBW-aware occupancy of the triangular support in path `p` satisfies `N_eff(i, p) ≥ N_MIN` with `N_MIN = 2`.
2. **Main-lobe separation (this rewrite):** nearest-neighbor centre separation in path-`p` bins is at least the Hann main-lobe half-width:

```text
sep_bins(i, p) = min_{j ∈ {i-1, i+1} ∩ [0,119]} |center[j] - center[i]| / Δf_p
sep_bins(i, p) ≥ SEPARATION_MIN_BINS
SEPARATION_MIN_BINS = 2   # = null-to-null / 2 = centre→first-null
```

3. **Path contribution after fusion:**

| Centre frequency | `contributing_paths(i)` |
|---|---|
| `center[i] ≤ 160` | `{LF}` |
| `center[i] ≥ 320` | `{MAIN}` |
| `160 < center[i] < 320` | `{LF, MAIN}` (fail-closed: both must satisfy (1) and (2)) |

Bands failing (1) or (2) are **not scored** for the SR-parity max-|Δ| claim: they are outside `R` (reportable as `EXCLUDED_GEOMETRY`), never silent PASS.

Endpoint bands use only the single existing neighbor (`i=0` → `{1}`; `i=119` → `{118}`).

---

## 4. Constants table with a priori derivation

| Constant | Value | Derivation |
|---|---|---|
| `ENBW_BINS` | 1.5 | Classical periodic Hann equivalent noise bandwidth |
| `N_MIN` | 2 | Prior ACTIVE under-resolution floor: smallest integer occupancy strictly above one ENBW bin in spirit of “more than a single ENBW cell”; **retained, not raised** |
| `MAIN_LOBE_NULL_TO_NULL_BINS` | 4 | Classical periodic Hann main-lobe null-to-null width |
| `SEPARATION_MIN_BINS` | 2 | `MAIN_LOBE_NULL_TO_NULL_BINS / 2` — neighbour must not lie strictly inside the main lobe |
| `Δf_MAIN` | `48000/4096` | Frozen MAIN FFT |
| `Δf_LF` | `48000/8192` | Frozen LF FFT |
| `r` | `1000**(1/119)` | Geometric centre ratio from §6.1 |
| Gate dB | 0.25 | Untouched |

### 4.1 Closed-form frequency thresholds (consequence, not knobs)

For interior bands, `|center[i+1] - center[i]| = center[i]·(r − 1)`.  
The half-lobe predicate `sep_bins ≥ 2` is equivalent to:

```text
center[i] ≥ SEPARATION_MIN_BINS · Δf_p / (r − 1)
```

Numerically (documentation aid only; implementation should use the bin formula in §3.2):

| Path | `2 · Δf_p / (r − 1)` |
|---|---|
| MAIN | ≈ 392.1528 Hz |
| LF | ≈ 196.0764 Hz |

Crossfade bands must clear **both** thresholds in the bin metric (i.e. the MAIN bin test is stricter).

### 4.2 A priori structural census under `SEPARATION_MIN_BINS = 2`

Using only centres + `Δf_p` (no audio, no Δ tables):

| Path | Band indices with `sep_bins < 2` to nearest neighbour |
|---|---|
| MAIN | 53 bands (low end of the grid up through the band whose centre is still below the MAIN threshold above) |
| LF | 41 bands (same construction with `Δf_LF`) |

After fusion path selection (§3.2), the admissible `R` is the intersection of ENBW-`N_MIN` survival and this separation survival on contributing paths. Exact set membership is a pure function of frozen geometry; it must be bit-stable on the gate platform.

**Why not `SEPARATION_MIN_BINS = 4`?**  
Null-to-null (= 4) would demand a full main-lobe *width between centres*, i.e. the neighbour at the far null. The residual diagnosis is membership **inside** the lobe (distance from centre to neighbour below the first null). The matching predicate is therefore half-width = 2. Choosing 4 would be a different, stricter physical claim and is not selected here.

**Why not raise `N_MIN`?**  
Forbidden by mandate when motivated by FAIL magnitudes; also orthogonal to centre-in-lobe inseparability.

---

## 5. What this amend claims — and what it refuses to claim

**Claims (document intent):**

- Per-band SR parity is only a well-posed independent-band claim on `R`, where each admitted centre is both ENBW-occupied and main-lobe-separated from its grid neighbours under the frozen Hann and the contributing FFT path(s).
- Exclusion is geometric and pre-registered; it is not outcome-conditional.

**Refuses:**

- Fitting `N_MIN` / `SEPARATION_MIN_BINS` / fusion knees / FFT sizes to a remeasure table.
- Treating dense multi-tone excitation as a substitute for fixing an ill-posed band claim.
- Touching 0.25 dB, CONTRACT freeze bytes, metrology lock payload, or `SHA256SUMS` in this proposal step.

---

## 6. Honest alternative considered (and not chosen as primary)

A pure **field-definition** rewrite (e.g. replace 120-band PSD parity with a coarser projection, or declare the entire fused vector incomparable) would also remove the ill-posed claim, but would discard band identity that remains geometrically separable at high centres under MAIN/LF. The main-lobe predicate keeps the §7 `*_psd_db[120]` field and removes only the bands for which neighbour centres are definitionally inside the window main lobe.

If redteam shows that even main-lobe-separated bands cannot carry an independent-band SR claim under the frozen frontend for a *non-outcome* structural reason not listed in the mandate, the fallback is a further domain/field amend — not threshold shopping.

---

## 7. Implementation notes (for a later builder; not authorized here)

Document-only now. A future GO’d builder would:

1. Serialize `SEPARATION_MIN_BINS = 2` and the contributing-path rule beside the existing ENBW/`N_MIN` predicate.
2. Emit per-band admission tags: `IN_R` | `EXCLUDED_UNDERRESOLVED` | `EXCLUDED_MAINLOBE` (names illustrative).
3. Compute the gate max-|Δ| **only** on `i ∈ R`.
4. Leave lock FFT/window/floor bytes unchanged unless a separate freeze GO says otherwise.

---

## 8. Exit criteria for this proposal artifact

1. Direction chosen and justified a priori: **domain-on-`R` + main-lobe separation (`SEPARATION_MIN_BINS = 2`), retaining ENBW `N_MIN = 2`.**
2. Constants table with derivation present (§4).
3. Contamination: **CLEAN** (§0).
4. Out of scope here: redteam → judge remeasure → second GO.

---

## 9. Judge coherence + geometric consequence (2026-07-25)

**Coherence: OK.** Two predicates, two window properties (`ENBW` → `N_MIN=2`; main-lobe null-to-null/2 → Rayleigh `SEPARATION_MIN_BINS=2`). Guardrail “`N_MIN` not raised” respected. Audit note: both constants equal `2` for different reasons — keep both derivations visible in normative text.

**A-priori geometric consequence** (bin arithmetic on frozen centres; not a run outcome):

| quantity | value |
|----------|--------|
| `\|R\|` / 120 | **67** / 120 (~56%) |
| first `IN_R` band | index **53** (~433.7 Hz); last excluded **52** (~409.2 Hz) |
| 20–80 Hz held | **0** / 24 |
| 80–200 Hz held | **0** / 16 |
| 200–500 Hz held | **3** / 16 |
| ≥500 Hz held | **64** / 64 |

Crossfade bands must clear **both** path tests → MAIN half-lobe (~392 Hz closed-form) dominates → effectively **no SR-parity claim below ~409 Hz**.

**Product decision required (Marco) — re-measure BLOCKED until answered:**

Separate two claims:

1. *Metrology:* below ~400 Hz, band-to-band SR parity is not well-posed under this grid+window (geometry demonstrates).
2. *Product:* therefore we verify **nothing** below ~400 Hz (does **not** follow automatically).

Alternatives that are not silent inheritance of (1): different low-band instrument (longer window / pre-band PSD), report-only LF region, or a-priori wider LF tolerance — each is a separate amend, not shopping after a PASS number.

**Product decision (Marco, 2026-07-25): report-only LF** — not option-1
blindness, not a wider LF dB tolerance, not a second instrument this tranche.
Bands ∉ `R` do **not** close gate 4; their per-band / per-SR max|Δ| **must**
be published (fail-closed if omitted). Contract must state *why* (grid finer
than window separation). G4 debt: cross-SR low-end detection stability.
Mandate for normative prose: `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_MANDATE.md`.

**Re-measure:** authorized **after** the report-only clause is written and
paper-redteamed — gate max on `i ∈ R` only; report-only table mandatory for
∉`R`.
