# REV7 — Report-only LF SR-parity (normative candidate prose)

**Status:** DOCUMENT-ONLY CLAUSE — **CONSOLIDATED into CONTRACT REV7** (amend A)  
**Date:** 2026-07-25 (consolidate 2026-07-26)  
**Authority:** Marco product decision (report-only under geometric limit) via  
`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_MANDATE.md`  
**Geometry package:** `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`  
(ENBW `N_MIN = 2` ∧ Rayleigh `SEPARATION_MIN_BINS = 2` → domain `R`)  
**Living authority:** `docs/MOTORE_V3_G1_CONTRACT.md` REVISIONE 7 CONSOLIDATA  

**Does not:** change 0.25 dB; invent an LF alternate dB tolerance; raise
`N_MIN`; retune `SEPARATION_MIN_BINS`; claim G1 PASS / official G1b tip.

---

## 1. Why report-only (must appear in CONTRACT language)

Under the frozen §6 geometry (120-band log grid, triangular supports,
periodic Hann, dual FFT MAIN 4096 / LF 8192, fusion 160/320 Hz), neighbour
band centres can lie inside the analysis main lobe at low centres. Those
bands are **not independently separable** by the frozen window. Therefore
sample-rate parity of per-band features below the geometric resolvability
limit is **not a well-posed closing claim** for gate 4 — not because the
0.25 dB threshold is wrong, and not because the frontend is exempt from
scrutiny there.

This reason must be stated explicitly in the consolidated CONTRACT. It must
not be left as an unspoken side-effect of an admission formula.

---

## 2. Domains

Let `R` be the a-priori set of band indices that satisfy both:

1. under-resolution / ENBW occupancy on every fusion-contributing path
   (`N_MIN = 2 = ceil(ENBW_Hann)`), and
2. neighbour-centre separation on every fusion-contributing path
   (`sep_bins ≥ SEPARATION_MIN_BINS = 2 = main-lobe null-to-null / 2`),

as specified in the scope-rewrite proposal (fail-closed on crossfade: both
LF and MAIN paths must satisfy).

| domain | role |
|--------|------|
| `i ∈ R` | **Gate-closing** SR-parity domain for PSD / shape / prominence (and valid-channel level scalars as already scoped). `max\|Δ\| ≤ 0.25` dB closes or fails gate 4. |
| `i ∉ R` | **Report-only** SR-parity domain. Does **not** enter the gate-closing max. Must still be measured and published. |

Shape/prominence for gate-closing cells remain computed under the prior
fail-closed choice: §7 formulas restricted to support compatible with `R`
(proposal choice B / redteam closure). Report-only bands are tagged
`EXCLUDED_GEOMETRY` (or finer: under-resolved vs main-lobe) — never silent
PASS.

---

## 3. Mandatory publication (fail-closed)

The G1e frontend/benchmark numeric report (or the successor report named in
PLAN for the G1 close package) **MUST** include a table (or machine-readable
equivalent) with at least:

- asset id / portion id (§13.1);
- host sample rate under test (44.1 and 96 vs 48);
- band index `i` for **every** `i ∉ R` (no subsetting / “interesting band”
  cherry-pick); if a field is inactive under the same floor/union rule used
  for gate cells, publish the cell as inactive / N/A with the rule named —
  do not omit the band row;
- field id (`mid_psd_db` / `mid_shape_db` / `mid_prominence_db` / … as
  applicable on that channel);
- `|Δ|` dB vs the 48 kHz reference on the aligned frame (when both sides
  admit the cell under the named activity rule);
- per-asset and overall `max|Δ|` restricted to active cells with `i ∉ R`
  (report-only max — **not** used to close gate 4).

**Omitting this table, or publishing only a prose summary without per-band
numbers, is a report FAIL** — same severity class as omitting a required
gate-4 artifact. Report-only is **not** an optional appendix.

Gate 4 PASS/FAIL language in the report must state the perimeter:

> Gate-closing claim applies only to bands in `R`. Bands outside `R` are
> report-only because the 120-band grid is finer than periodic-Hann
> neighbour separation under the frozen analysis.

Forbidden PASS wording: “SR-parity verified across the full 20 Hz–20 kHz
band grid” while `R` excludes the low region.

---

## 4. What this clause does not authorize

- An alternate LF threshold (e.g. “tolerate X dB below 400 Hz”).
- Dropping shape/prominence/level from the declared domain to hide FAIL.
- Treating empty report-only domain as PASS.
- Silent lock / SHA256SUMS mutation.
- Claiming G1 PASS or Ableton readiness.

---

## 5. Durable debt — G4 cross-SR low-end detections

Register now (PLAN debt list + CONTRACT pointer on consolidate):

> G4 MUST include an explicit verification of cross-sample-rate stability of
> detections on low-end problem classes (at minimum the product classes that
> depend on content below the geometric resolvability limit of the G1 band
> grid — mud / boom / boxy-mid as named in the product taxonomy). This is
> **not** satisfied by G1 gate 4 PASS on `R` alone.

Exact G4 metrics are out of scope for this clause; the debt existence and
trigger (“low-end classes × host SR”) are in scope and mandatory.

---

## 6. Consolidation binding

**Done:** this clause is merged into `docs/MOTORE_V3_G1_CONTRACT.md` §13.2
gate 4 beside the `R` predicates (REV7 amend A). Coordinated metrology lock
+ SHA256SUMS rehash follows in the lock commit (hashed `R` procedure +
report-only publication required). ≠ G1 PASS.

---

## 7. Contamination

Writer path: mandate + geometric proposal structure. No run-magnitude
shopping. Product decision (report-only vs blind vs alternate instrument)
was taken by Marco before this prose.
