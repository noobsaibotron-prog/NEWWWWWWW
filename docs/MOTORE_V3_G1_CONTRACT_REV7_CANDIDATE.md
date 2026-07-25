# Motore v3 — CONTRACT REV7 CANDIDATE (document-only)

**Status:** CANDIDATE DRAFT — **NOT CONSOLIDATED** — ≠ amend of freeze `6d254d0a`  
**Date:** 2026-07-25  
**Authority:** Guardian GO (scoped draft) after G1b spike WS4 RED + independent CC  
**Spike tip (evidence):** `c7f05871` @ `spike/motore-v3-g1b-frontend`  
(`motore-v3-g1b-spike` worktree)

**This document does NOT:**
- consolidate REV7 into `docs/MOTORE_V3_G1_CONTRACT.md`;
- authorize silent edit of `metrology_lock.json` / SHA256SUMS;
- claim G1 PASS or product G1b tip;
- relax the 0.25 dB hard max;
- invent a replacement numeric threshold by fitting spike RED cells (9.537 dB).

**This document DOES:**
- record the falsifiable impossibility of a **specific REV6 clause**;
- authorize only the next step: design the replacement admission predicate
  under the constraints below, then redteam + re-measure, then a **second**
  Guardian GO before any consolidate.

---

## 1. Trigger (falsifiable impossibility)

Under REV6 §13.2 gate 4, with implementation faithful to §5/§6/§7 and a-priori
pins P1–P7 (unchanged; `changed_to_pass: false`), the G1b adversarial spike
measures:

| evidence | value |
|----------|--------|
| overall `max\|Δ\|` | **9.537 dB** (threshold 0.25 dB) |
| worst cell | `pseudo_noise@44100`, `mid_shape_db[18]` (~56.9 Hz) |
| all 6 SR cells | RED |
| pins rewritten to pass? | **no** |
| streaming / proof (b) | PASS on spike schedules |

Artifacts:
- `ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.md` (spike worktree tip `c7f05871`)
- `ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.json`
- harness: `ml_v3/benchmark/sr_parity.py` (implements lock predicate literally)

Independent CC (not stored as hashed G1a artifact) additionally showed that
**FFT bins within ~20 dB of the spectral peak** between 48 kHz direct and
44.1→48 resampled audio agree to ~**0.03 dB** — i.e. the resampler/frontend
path is not the failure mode on signal-bearing bins. That corroborates the
amend **direction** (admission, not 0.25). It is **not** a partial PASS and
must **not** be used to pick a new numeric cut.

---

## 2. Clause that fails (narrow)

REV6 §13.2 gate 4 currently states (product freeze `6d254d0a`):

> Predicato di attivita (nessuna maschera post-hoc): una cella di PSD e
> attiva sse `max(psd_db_ref, psd_db_sr) > -120` (unione: strettamente sopra
> il floor …). … Shape e prominence della stessa banda ereditano l'attivita
> della PSD del medesimo canale. … **Una sola cella attiva fuori soglia →
> FAIL dell'intero gate.** … `max_i |x_i(sr) - x_i(48k)| <= 0.25 dB`

**What is impossible under pinned degrees of freedom:** requiring
`max|Δ| ≤ 0.25 dB` on the **full set of cells admitted by**
`max(psd_ref, psd_sr) > -120` when that set includes cells whose energy is
dominated by **inter-component window leakage / interference** (and
near-floor union one-sided activations), especially on P2 single-bin /
LF-pure geometry (~50–70 Hz). Those cells are construction- and
sub-sample-phase-sensitive; no remaining implementable freedom (window,
FFT sizes, band geometry, fuse, resampler coeffs, P1–P7) removes the
failure without amending admission or illicitly relaxing the threshold.

**What is NOT claimed impossible:** the 0.25 dB hard max on cells that
actually carry stable signal content. Spike + CC evidence points the other
way. **REV7 must not raise, replace, or soft-max the 0.25 dB threshold.**

Smoking-gun shape (from spike evidence JSON, illustrative of the clause):
- `log_sweep` peak: `psd_ref = -120.0`, `psd_sr ≈ -113.6` → admitted only via
  union; `|Δ| ≈ 6.4 dB` on `mid_psd_db`.
- `pseudo_noise` / `multitone` peaks: `psd_ref ≈ -119.0…-119.3` (a hair above
  floor) driving `mid_shape_db` deltas of several dB on single-bin bands.

---

## 3. Amend target (scoped)

### In scope (updated after metrology-redteam 2026-07-25)

**Original draft scope:** replace the gate-4 activity / domain-admission
predicate in §13.2 item 4 (+ lock string on consolidate).

**Scope reopen (judge-endorsed, redteam VERDICT CONTRACT-BROKEN):** the amend
target may also need to include **one or more** of:

- an explicit **domain** carve-out for geometrically unresolvable bands
  (preferred durable form for 0/1-bin material occupancy — pick **either**
  domain wording **or** predicate exclusion, not dual optional packaging);
- fail-closed rules for **shape/prominence coupling**: excluding a PSD cell
  from the max does **not** remove that band’s energy from §7’s global
  shape normalizer (`sum` over 120) nor from prominence’s ±16 kernel — so
  “inherit ACTIVE” alone is insufficient;
- a normative narrowing of candidate constraint 4 (ENBW-aperture-only) **or**
  an a-priori anti-leakage operator (disclaimer/residual ≠ satisfaction of a
  normative constraint);
- a corrected **MATERIAL** rule (equal-energy `W_MATERIAL` bound is not
  worst-case);
- hashed/pinned `N_BINS` / `ACTIVE` mask procedure + vacuous FAIL per
  §13.1 portion and valid channel.

Invalid channels remain ignored as §7. **0.25 dB hard max and max
aggregator remain immutable.**

**Product decision (Marco, 2026-07-25) — report-only LF:** geometric
domain `R` from ENBW `N_MIN=2` ∧ Rayleigh `SEPARATION_MIN_BINS=2` (scope
rewrite). Gate 4 closes only on `i ∈ R`. Bands `i ∉ R` are **report-only**:
measured and **must** be published (omit table → report FAIL); they do not
enter the gate-closing max. Explicit CONTRACT why-sentence required (grid
finer than window separation). G4 debt: cross-SR low-end detection
stability. Normative prose:
`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`. No LF alternate dB
tolerance. No option-1 blindness.

### Out of scope (forbidden in this REV7)

- Changing `0.25 dB` hard max or replacing max with mean/p95/RMSE.
- Dropping `shape` / `prominence` / `level` from the declared domain
  **without** a fail-closed replacement definition that closes coupling
  (redefinition of domain membership or of shape/prominence for gate 4 may
  be in scope; silent drop to hide FAIL is not).
- Post-hoc prominence clamp; changing aggregator; changing P1–P7.
- Raising `N_MIN` after a FAIL to chase PASS (threshold shopping).
- Changing §10 evaluator −100 dBFS/Hz criteria (different gate).
- Product frontend “fixes” under an unconsolidated candidate rule.
- Silent lock / SHA256SUMS mutation.

---

## 4. Replacement admission — design constraints (a priori)

The concrete formula is **not chosen in this draft** (to avoid threshold
shopping against the 9.537 dB cells). Any successor predicate MUST satisfy
all of the following **before** consolidate:

1. **A priori.** Written into CONTRACT (and then lock) **before** the
   re-measurement that claims PASS. Forbidden: pick cutoffs by scanning
   spike RED cells / margin tables until `max|Δ| ≤ 0.25`.
2. **No post-hoc mask.** Still forbidden to hide bands after seeing errors
   on a run (same spirit as REV6).
3. **Preserve one-sided artifact intent.** A defect that appears at 44.1/96
   but not at 48 must not vanish from the max solely because the reference
   sits at the floor. Pure “intersection both > −120” is **insufficient**
   as a complete replacement (multitone peaks have both sides above floor
   and still fail).
4. **Signal-bearing admission (REV7 perimeter — narrowed).** For this amend,
   admission is **ENBW-aperture resolvability** on every positive-weight
   fusion path plus the floor/union cut — stated in CONTRACT language
   **without** fitting to spike RED magnitudes. Inter-component leakage /
   sidelobe domination is **out of scope** for `ACTIVE` unless a separate
   a-priori anti-leakage operator is added by a further authorized amend.
   (Proposal §2.0; residual disclaimer ≠ satisfaction.)
5. **Inheritance + coupling close.** Shape/prominence of band *b* inherit PSD
   activity of band *b* on that channel **after** gate-4 shape/prominence
   are redefined on `R = {RESOLVED}` so excluded-band energy cannot
   contaminate ACTIVE cells (proposal §2.7 choice B); invalid channels ignored.
6. **Vacuous-PASS fail-closed.** If a run admits **zero** active cells on a
   required asset/portion, the gate **FAILS** (empty domain ≠ PASS).
7. **0.25 dB immutable.** Domain fields and max aggregator unchanged.
8. **Re-measure required.** After the formula is written, re-run the spike
   adversarial subset (and redteam attacks in §5) on the gate platform;
   PASS/FAIL is that measurement, not this draft.

**Open slot (filled in proposal prose — not consolidated):**

See `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md` (redteam closure draft).
Packaging summary (normative intent; byte-equivalent max set):

```text
RESOLVED(b)  ⇔  every path with w_path(center[b]) > 0 has N_BINS(b,N_path) ≥ 2
DOMAIN       :  ¬RESOLVED(b) ⇒ b outside gate-4 PSD/shape/prominence domain
ACTIVE(b,ch) ⇔  RESOLVED(b) ∧ max(psd_ref, psd_sr)[b,ch] > -120
GATE-4 shape/prominence := §7 formulas on R={i: RESOLVED(i)} only
CONSTRAINT-4 := ENBW aperture + floor/union only (leakage out of scope)
VACUOUS      :  empty active set on any §13.1 portion × valid channel → FAIL
N_MIN = 2 = ceil(ENBW_Hann); 0.25 dB unchanged; no W_MATERIAL
```

Constraint 4 of this candidate §4 is **narrowed** for REV7 admission to the
ENBW + floor perimeter above (proposal §2.0). Dual optional 0/1-bin wording
is forbidden: domain amend is the sole normative packaging.

---

## 5. Required before consolidate (second GO)

1. **Write** the concrete `ACTIVE(…)` (+ domain / coupling / MATERIAL
   clauses from §3 scope reopen) — **done in proposal** (closure draft);
   pending delta redteam acceptance.
2. **`ember-metrology-redteam`:** first pass → **CONTRACT-BROKEN**; judge
   endorsed five must-fixes. Prose closure + **delta redteam** →
   **CONTRACT-POROUS** (2026-07-25): five must-fixes closed; residuals
   remain (prominence-on-R dual reading; ENBW↛occupancy isomorphism;
   MATERIAL center vs support wings; vacuous vs level scalars; A15
   wording). **Independent re-measure authorized YES** under residual
   acceptances A–F in the delta redteam handoff (ENBW-only PASS wording;
   harness §7-on-R; pin one prominence algorithm before lock; no N_MIN
   raise; no leakage-solved claim). ≠ consolidate GO.
3. **Independent re-measure** (judge lineage) — **DONE → RED**.
   Report: `docs/MOTORE_V3_REV7_REMEASURE_R_REPORT.md`.
   Stationary subset under ENBW+floor / §7-on-`R`: still FAIL (multitone
   ~4.77; noise 9.54→1.24 still FAIL). **Dense probe (iii)** — one tone per
   band centre, same rules — also FAIL (~1.19 dB, `prom` b24): sparse
   excitation **insufficient**; residual diagnosis = **inter-band
   inseparability** (neighbour centres inside Hann main lobe) on
   `RESOLVED` bands. **Forbidden:** fit cuts / raise `N_MIN` / shop
   constants against 4.77 / 1.24 / 1.19. **log_sweep** still owed before
   formal close. Next: untainted rewrite of domain/fields and/or a-priori
   geometric criterion (ENBW vs main-lobe separation re-openable on
   diagnostic grounds only) → redteam → re-measure → Guardian.
   ≠ consolidate GO.
4. **Guardian second GO** — **blocked** until scope rewrite addresses §3
   reopen (domain/fields / coupling) consistent with re-measure RED; then
   consolidate path into `docs/MOTORE_V3_G1_CONTRACT.md` only after a later
   PASS re-measure under the rewritten scope.
5. **Lock reopen / re-hash** (mandatory on consolidate): G1a freeze pins
   `predicate: max(psd_db_ref, psd_db_sr) > -120` and
   `threshold_max_abs_db: 0.25` under digest `d2c35ccc…`. Consolidate
   implies coordinated lock + SHA256SUMS update — never silent edit.

Until steps 1–5 complete: **REV7 consolidate = NO.** Product G1b tip must
not claim gate-4 PASS under REV6 admission.

### Redteam must-fix (paper; no N_MIN raise) — closure mapping

1. Coupling → proposal §2.7 choice **(B)** (gate-4 shape/prominence on `R`).
2. Constraint-4 → proposal §2.0 **narrowed** (ENBW + floor; leakage OOS).
3. 0/1-bin → proposal §2.5 **domain amend only** (predicate set-equivalent).
4. MATERIAL → proposal §2.2 **`w_path > 0`** (`W_MATERIAL` withdrawn).
5. Pin + vacuous → proposal §2.1 + §2.8.

---

## 6. Lock / G1a consequence (when consolidated)

| item | action |
|------|--------|
| `threshold_max_abs_db: 0.25` | **unchanged** |
| activity predicate string | replace with consolidated `ACTIVE(…)` |
| `metrology_lock_sha256` | recompute |
| SHA256SUMS | update affected entries |
| spike harness | align to new predicate only after consolidate GO |

---

## 7. PLAN / lab state impact (until consolidate)

- Frozen technical authority remains CONTRACT @ `6d254d0a` (REV6).
- This file is a **candidate** under PLAN lab authority.
- Spike remains ≠ G1 PASS; ≠ Ableton readiness.
- Official product G1b gate-4 claims stay blocked on REV6 admission until
  consolidate + re-measure PASS (or a different authorized path).

---

## 8. Handoff

| agent | next |
|-------|------|
| untainted author | closure draft in `MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md` |
| ember-metrology-redteam | **delta** pass on closure prose; re-measure still forbidden if BROKEN |
| ember-parity-lab | re-measure only after delta non-BROKEN |
| ember-contract-guardian | second GO for consolidate only |

≠ G1 PASS. ≠ REV7 consolidated. 0.25 dB not touched.
