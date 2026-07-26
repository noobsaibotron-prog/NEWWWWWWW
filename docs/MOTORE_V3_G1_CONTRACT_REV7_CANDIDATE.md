# Motore v3 — CONTRACT REV7 CANDIDATE (document-only)

**Status:** CANDIDATE PACKAGE — **NOT CONSOLIDATED** — ≠ G1 PASS  
**Date:** 2026-07-26  
**Nature:** Single authoritative packaging of **exactly two** closing amendments
for a future **one** REV7 consolidate (option C path). Not two REV7s.
**≠** amend of freeze `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a`  
**≠** edit of `metrology_lock.json` / SHA256SUMS in this commit  
**≠** self-GO / Guardian second GO

**Authority chain (evidence, not consolidate GO):**
| link | commit / path |
|------|----------------|
| Gate-4 scope clause GO | `31216df4` — `docs/MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md` |
| LF report-only clause | `71159469` — `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` |
| Stationary `R` closing MEASURE-PASS | `8cf38625` — `docs/MOTORE_V3_G1_STATIONARY_R_CLOSING_MEASURE.md` |
| Parked sweep redesign (G1c/G1e debt) | tip ~`9b8f8305` — `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_PROPOSAL.md` |

---

## This document does NOT

- consolidate REV7 into `docs/MOTORE_V3_G1_CONTRACT.md`;
- edit `metrology_lock.json` or SHA256SUMS;
- claim G1 PASS, ACCEPT, Ableton readiness, or official G1b tip;
- relax **0.25 dB**, replace **max** with mean/p95/RMSE, or redefine **R**;
- reopen A3 / ACTIVE / POROUS litigation on the sweep proposal;
- invent new constants or shop thresholds against measured cells;
- self-issue Guardian second GO.

## This document DOES

- package **exactly two** closing amendments for a future single REV7
  consolidate (sections 1–2 below);
- restate immutables and evidence pointers;
- state the remaining sequence: Guardian second GO → consolidate → rehash →
  official G1b tip;
- leave non-stationary / `log_sweep` redesign as **parked G1c/G1e input debt**.

---

## 1. Closing amendment A — LF report-only ∉R

**Cite (normative prose already written):**  
`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` @ `71159469`

**R geometry (immutable; not redefined here):**  
`R` = bands satisfying both ENBW `N_MIN = 2 = ceil(ENBW_Hann)` **and**
Rayleigh `SEPARATION_MIN_BINS = 2` (main-lobe null-to-null / 2), fail-closed on
fusion crossfade — see LF clause §2 and
`docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`.

| domain | role in future consolidate |
|--------|----------------------------|
| `i ∈ R` | Gate-closing SR-parity domain; `max\|Δ\| ≤ 0.25` dB closes or fails gate 4 |
| `i ∉ R` | **Report-only** — measured and **must** be published; does **not** enter the gate-closing max; omit table → report FAIL |

**Why (must appear in consolidated CONTRACT):** the frozen 120-band grid is
finer than periodic-Hann neighbour separation at low centres; per-band
SR-parity below that geometric limit is not a well-posed closing claim for
gate 4. Not an alternate LF dB tolerance. Not option-1 blindness.

**G4 debt (unchanged):** cross-SR low-end detection stability — LF clause §5.

---

## 2. Closing amendment B — Gate-4 scope (option C)

**Cite (scope GO):**  
`docs/MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md` @ `31216df4`

Normative intent for future consolidate (verbatim spirit of the six points):

1. **Gate-4 closing set in G1** = stationary assets only (`multitone`,
   `pseudo_noise`), evaluated on geometric domain **R**.
2. **`log_sweep`:** fixture remains generated and hashed in SHA256SUMS; **does
   not close** gate 4 in G1. Deviations still measured and published as
   **report-only**.
3. **Non-stationary SR-parity** is a **named requirement of G1c/G1e**, with
   parked mandate + proposal as input (not deleted).
4. Immutables restated in §3 below.
5. **A3 remains RETIRED**; ACTIVE family closed. No reopen.
6. **Phase scope decision, not a relaxation** — no threshold changed; no cell
   newly admitted that was not before.

---

## 3. Immutables (restatement — unchanged by this package)

- Aggregator: **max** (`max_i |x_i(sr) - x_i(48k)|`; one active cell out of
  threshold → FAIL entire gate; no mean / p95 / RMSE).
- Threshold: **0.25 dB**.
- Domain **R**: ENBW `N_MIN = 2` ∧ Rayleigh `SEPARATION_MIN_BINS = 2`
  (fail-closed on fusion crossfade) — LF clause §2 / scope clause §4.
- Mandatory publish **outside R** (report-only table; omit → report FAIL).
- A3 / ACTIVE: **RETIRED / closed** — no reopen via this package.

---

## 4. Evidence pointers (lab — ≠ G1 PASS)

| evidence | commit / path | lab role |
|----------|---------------|----------|
| Scope GO (option C) | `31216df4` / `docs/MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md` | decides closing set |
| LF report-only | `71159469` / `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` | ∉R publication + why |
| Stationary `R` MEASURE-PASS | `8cf38625` / `docs/MOTORE_V3_G1_STATIONARY_R_CLOSING_MEASURE.md` | closing set max\|Δ\| = **0.1915** dB ≤ 0.25 (stat only on `R`) |
| Prior formal R remeasure | `b3d7f71b` / `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` | byte-aligned stationary rows |

**Interpretation:** MEASURE-PASS under option C supports packaging readiness for
Guardian review. It does **not** authorize G1 PASS, ACCEPT, consolidate, or
official G1b tip by itself.

---

## 5. Parked sweep proposal — G1c/G1e input debt

`docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_PROPOSAL.md` tip ~`9b8f8305`
(+ mandate `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md`) remains
**PARKED**: out of G1 gate-4 closing set; not under CC/RT now; not deleted.
Resume at G1c/G1e when an observable exists. **Do not** reopen A3 or POROUS
litigation to force a G1 close.

Debt mirror: **Parity cross-SR non-stazionaria: non verificata a G1.**

---

## 6. Explicit non-claims (this commit / this file)

```text
≠ G1 PASS
≠ REV7 consolidated into CONTRACT freeze
≠ metrology_lock / SHA256SUMS updated
≠ official G1b tip
≠ Guardian second GO (this package is ready FOR that GO — not a self-GO)
```

Frozen technical authority remains CONTRACT @ `6d254d0a` (REV6) until a later
Guardian consolidate GO lands a coordinated CONTRACT + lock + SHA256SUMS
update.

---

## 7. Sequence remaining

1. **Guardian second GO** on this candidate package (docs-verify + evidence).
2. **Consolidate** the two amendments into `docs/MOTORE_V3_G1_CONTRACT.md`
   (single REV7 — not REV7a/b).
3. **Rehash** metrology lock + SHA256SUMS (coordinated; never silent).
4. **Official G1b tip** only after 1–3 (spike ≠ tip until then).

Until steps 1–3 complete: **REV7 consolidate = NO.** Product gate-4 claims
must not assert G1 PASS under this candidate alone.

---

## 8. Historical note (superseded packaging narrative)

Earlier drafts of this file tracked ACTIVE-formula redesign / redteam
POROUS closure / full-grid remeasure RED. That narrative is **historical
context** for why report-only LF + geometric `R` exist. Under option C, the
**authoritative closing package for the next REV7** is **only** amendments
A and B above. Sweep / non-stat work is deferred (§5), not packaged as a
third REV7 amend here.

---

## 9. Handoff

| agent | next |
|-------|------|
| ember-contract-guardian | **second GO** on this package (or NO-GO/BLOCK with evidence) |
| ember-parity-lab | n/a for this docs package; measure already at `8cf38625` |
| ember-phase-builder | wait — consolidate only after Guardian second GO |
| ember-metrology-redteam | no reopen of parked sweep proposal now |

≠ G1 PASS. ≠ REV7 consolidated. 0.25 dB / max / R not touched.
Ready for Guardian second GO — **not** self-GO.
