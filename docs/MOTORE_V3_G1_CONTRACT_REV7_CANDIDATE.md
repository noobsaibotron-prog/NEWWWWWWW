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

### In scope

Replace **only** the gate-4 **activity / domain-admission predicate** in
§13.2 item 4 (and the matching string in the G1a metrology lock when/if
consolidated — see §6). Shape/prominence continue to **inherit** PSD-band
activity of the same channel; invalid channels remain ignored as §7.

### Out of scope (forbidden in this REV7)

- Changing `0.25 dB` hard max or replacing max with mean/p95/RMSE.
- Dropping `shape` / `prominence` / `level` from the declared domain.
- Post-hoc prominence clamp; changing aggregator; changing P1–P7.
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
4. **Signal-bearing admission.** Exclude cells whose content is dominated by
   inter-component leakage / interference or numerical-floor instability,
   while retaining cells that carry stable signal energy relevant to SR
   parity. The operational definition must be stated in CONTRACT language
   (absolute floor, relative-to-frame-peak dynamic range, fixture-declared
   component occupancy, or another a-priori rule) **without** fitting that
   definition to the spike RED magnitudes.
5. **Inheritance unchanged.** Shape/prominence of band *b* inherit PSD
   activity of band *b* on that channel; invalid channels ignored.
6. **Vacuous-PASS fail-closed.** If a run admits **zero** active cells on a
   required asset/portion, the gate **FAILS** (empty domain ≠ PASS).
7. **0.25 dB immutable.** Domain fields and max aggregator unchanged.
8. **Re-measure required.** After the formula is written, re-run the spike
   adversarial subset (and redteam attacks in §5) on the gate platform;
   PASS/FAIL is that measurement, not this draft.

**Open slot (for the follow-up amend prose, not filled here):**

```text
ACTIVE(b, ch) ≜ <formula a priori satisfying constraints 1–8>
```

---

## 5. Required before consolidate (second GO)

1. **Write** the concrete `ACTIVE(…)` formula into a CONTRACT amend patch
   (still document-only until accepted) obeying §4.
2. **`ember-metrology-redteam`:** attack false-PASS surfaces — activity
   window, union/empty-set, vacuous PASS, invalid-channel ignore, platform
   drift, post-hoc threshold shopping.
3. **Optional `ember-parity-lab`:** re-verify max|Δ| / margin tables on
   CPython 3.12.13 gate venv.
4. **Guardian second GO** to consolidate REV7 into
   `docs/MOTORE_V3_G1_CONTRACT.md` and bump revision header.
5. **Lock reopen / re-hash** (mandatory on consolidate): G1a freeze pins
   `predicate: max(psd_db_ref, psd_db_sr) > -120` and
   `threshold_max_abs_db: 0.25` under digest `d2c35ccc…`. Consolidate
   implies coordinated lock + SHA256SUMS update — never silent edit.

Until steps 1–5 complete: **REV7 consolidate = NO.** Product G1b tip must
not claim gate-4 PASS under REV6 admission.

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
| Marco / author | fill `ACTIVE(…)` under §4 constraints (no fit to 9.5 dB) |
| ember-metrology-redteam | after formula written; before consolidate |
| ember-parity-lab | optional re-verify |
| ember-contract-guardian | second GO for consolidate only |

≠ G1 PASS. ≠ REV7 consolidated. 0.25 dB not touched.
