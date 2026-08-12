# MANDATO — clausola report-only LF (decisione prodotto Marco)

**Status:** AUTHORIZED 2026-07-25 — untainted writer  
**Product decision:** under the geometric resolvability limit of the frozen
120-band grid + periodic Hann, SR-parity **does not close** gate 4, but
**must** be measured and published (report-only). Not option-1 blindness.
Not a wider LF dB tolerance (forbidden: invent X dB from run residuals).
Not a second instrument (longer FFT / pre-band PSD) in this tranche.

## Destinatario

Untainted writer. Do **not** open remeasure/WS4 evidence / REV7 candidate
run tables. Do **not** use measured max|Δ| to pick constants.

## What is already decided (do not re-litigate)

1. Coherent geometric package from scope rewrite: ENBW `N_MIN=2` **and**
   main-lobe Rayleigh `SEPARATION_MIN_BINS=2` → domain `R` (a priori).
2. Consequence: `R` starts only above ~409 Hz under frozen geometry
   (documentation aid already in scope-rewrite proposal §4 / §9). That
   frequency is a **consequence of the predicates**, not a new knob.
3. Product: bands ∉ `R` are **report-only** for SR-parity — they do **not**
   contribute to the gate-closing max|Δ| ≤ 0.25, but their per-band /
   per-SR max|Δ| **must** appear in the G1e (or successor) numeric report.
4. **0.25 dB unchanged** on `i ∈ R`. No soft-max, no LF alternate threshold.
5. Contract must **state why** report-only: the 120-band grid is finer than
   the analysis window can separate below the geometric limit — not leave
   that as an implied side-effect of a formula.
6. Durable debt: G4 must carry an explicit cross-SR detection-stability
   check on low-end classes (mud/boom/boxy) — registered now, not discovered
   later. Writer drafts the debt sentence; does not invent G4 metrics.

## Compito

Update / write document-only:

1. Extend `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` (or a sibling
   `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`) with **normative**
   contract-shaped prose for:
   - gate-closing domain = `i ∈ R` only (predicates already specified);
   - report-only domain = geometrically excluded bands, mandatory publication
     fields (max|Δ| per band, per SR vs 48k, per required asset/portion);
   - explicit “why report-only” sentence (grid finer than window separation);
   - fail-closed: omitting the report-only table → G1e report **FAIL**
     (not optional appendix);
   - G4 debt registration sentence (cross-SR low-end detection stability).
2. Sync a short status note into
   `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` § product decision
   (without copying run magnitudes).

## Divieti

- No LF tolerance constant X dB.
- No raising `N_MIN` / retuning `SEPARATION_MIN_BINS` from outcomes.
- No CONTRACT freeze / lock / SHA256SUMS edit.
- No re-measure in this task.
- No opening `*REMEASURE*`, `*WS4*EVIDENCE*` outcome tables.

## Esito

Normative clause + contamination statement. Then: redteam on the clause →
independent re-measure (gate on `R` + mandatory report-only table for ∉`R`).
