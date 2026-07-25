# Motore v3 — Proposal: log_sweep HF admission (one-sided floor-union)

**Suggested commit title (if/when Marco authorizes commit):**  
`docs(v3): stamp log_sweep HF admission decisions (A / A3 / fuori REV7 LF)`

| Field | Value |
|-------|--------|
| **Status** | **DECISION STAMPED** — document-only; living choices locked; **uncommitted** until post-redteam/CC |
| **≠** | G1 PASS · REV7 consolidate · G1b tip ufficiale · freeze amend · lock re-hash |
| **Date** | 2026-07-26 |
| **Branch / tip at draft** | `feature/motore-v3-offline` @ `2c69606f` (living next-path) |
| **Freeze contract** | `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` (REV6) |
| **Lock digest (unchanged)** | `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` |
| **Trigger evidence** | `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` (+ `.json`) @ `b3d7f71b` |
| **Living path authority** | PLAN bullet post-`2c69606f`; handoff §A |

## DECISION STAMP (2026-07-26)

| Field | Value |
|-------|--------|
| **Date** | 2026-07-26 |
| **Reviewer** | Marco (confirmed **"ok"** on living choices) |
| **Path** | **Option A** — a-priori admission amend (NOT Option B tip-with-debt) |
| **Mechanism** | **A3** — BOTH: sweep-scoped activity predicate **AND** neighbourhood = **solo `F_TRAJ`** (support ∩ window chirp image + `T_MEM`; **not** peak-local ±K) |
| **REV7 packaging** | Sweep-HF stays **outside** the REV7 LF / geometric-`R` vehicle until measure **PASS** under the new admission; closing max on sweep **must not** silently use `ACTIVE∩R` |
| **Still true** | **REV7 consolidate: NO** · ≠ G1 PASS · ≠ G1b tip ufficiale |
| **Next** | Redteam/CC delta again on patched formula (CC + third redteam + CC-delta empty-N were **POROUS**; HIGH#1/#2 / MED#2 / LOW#3 closed in `MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md` incl. `CHK_CHIRP_REACHABLE` / T18) — measure still blocked until non-POROUS; formula must not self-stamp SOUND; **then** Marco authorizes commit |
| **Concrete formula** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md` (A3 executable boolean; document-only; tip packaging `81e86dc5`; POROUS closures HIGH#1/#2 / MED#2 / LOW#3 in prose) |

**REV7 consolidate: NO** — this document does **not** merge candidate REV7 prose
into the freeze CONTRACT. It addresses only the remaining gate-4 FAIL on
`log_sweep` HF under geometric `R` + report-only LF packaging already decided.

---

## 0. Verdict language (prefer NO-GO)

| Claim | Status |
|-------|--------|
| Gate-4 closable under current REV6 activity on sweep checkpoints | **NO-GO** (measured FAIL) |
| Stationary adversarial subset on geometric `R` | **PASS** (measured; does not close overall gate) |
| This proposal = G1 PASS / G1b tip / REV7 consolidate | **NO** |
| Recommended next lab action | Redteam/CC delta again on patched **A/A3** formula (POROUS closures incl. empty-N `CHK_CHIRP_REACHABLE` / T18 in formula prose) — **not** frontend implement; measure blocked until non-POROUS; commit only after that |

---

## 1. Problem statement (falsifiable, with evidence)

### 1.1 Measured FAIL (final R-remeasure)

Under perimeter ENBW `N_MIN=2` ∧ Rayleigh `SEPARATION_MIN_BINS=2` → `|R|=67`
(first ≈433.7 Hz), gate-closing cells use §7-on-`R` + REV6-style floor-union
`max(psd_ref, psd_sr) > −120`, with report-only packaging for `i ∉ R`
(`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`).

From `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` / `.json`:

| asset | vs 48k | max\|Δ\| dB | verdict | peak |
|-------|--------|------------|---------|------|
| multitone | 44100 / 96000 | **0.1915 / 0.1898** | **PASS** | level |
| pseudo_noise | 44100 / 96000 | **0.0458 / 0.0464** | **PASS** | level |
| log_sweep | 44100 / 96000 | **6.4350 / 5.8381** | **FAIL** | psd **b106** (~9404 Hz) |

**Overall gate:** **FAIL** — `max|Δ| = 6.435 dB` (hard threshold **0.25 dB**).

### 1.2 Smoking-gun cell (HF checkpoint skirt)

At checkpoint **16 kHz** (`t_cross≈1.7096`):

- `psd_ref[106] = −120.0` (clamp floor)
- `psd_sr[106] ≈ −113.56`
- admitted solely by union `max > −120` while the reference sits **on** the floor
- instantaneous sweep peak in that frame is near **b111** (~12.6 kHz), **not** b106

Same one-sided floor/union skirt pattern as WS4 smoke on HF checkpoints
(`docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` §2). This is **not** an LF
geometry / inseparability issue.

### 1.3 Contract clauses that admit the cell (REV6 freeze)

Freeze `6d254d0a` §13.1 (sweep) + §13.2 gate 4:

- **Sweep:** parity uses the preregistered checkpoint grid only (lock:
  `SWEEP_CHECKPOINT_HZ` = 20, 45, 60, 80, 250, 1000, 3500, 8000, **16000**,
  20000 Hz) — nearest frame to `t_cross`, ≤ one hop; missing checkpoint → FAIL.
- **Activity:** cell active iff `max(psd_db_ref, psd_db_sr) > −120` (union);
  shape/prominence inherit PSD activity; **one** active cell above 0.25 →
  entire gate FAIL.
- Lock pin (G1a T4, digest `d2c35ccc…`):
  `activity.predicate = "max(psd_db_ref, psd_db_sr) > -120"`,
  `threshold_max_abs_db = 0.25`, aggregator `max` (mean/p95/RMSE forbidden).

The FAIL is therefore a **predicate / checkpoint-domain admission** question
on a non-stationary fixture — not a resampler impossibility on stationary
content (already PASS on `R`).

---

## 2. Why report-only LF does **not** solve this

Report-only LF (`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`) moves
`i ∉ R` out of the gate-closing max because neighbour centres lie inside the
Hann main lobe at **low** centres.

| fact | implication |
|------|-------------|
| `first_in_R ≈ 433.7 Hz`, `|R|=67` | geometric carve-out is **LF** |
| peak FAIL band **b106 ≈ 9404 Hz** | **b106 ∈ R** |
| checkpoint 16 kHz is an HF §13.1 obligation | not excludable by ENBW/Rayleigh LF geometry |

Therefore: publishing ∉`R` tables (even the huge report-only `log_sweep`
shape deltas) **cannot** close or excuse the gate-closing FAIL at b106.
Consolidating REV7 **only** with report-only LF + ENBW domain would leave
this FAIL intact — consolidating now would be admission shopping, not
metrology (PLAN living counsel @ `2c69606f`).

---

## 3. Options considered (a priori; ≥3)

Hard constraints common to all options:

- **0.25 dB** hard max on the declared closing domain remains immutable.
- No replace max → mean / p95 / RMSE / post-hoc band subset.
- No `Source/` / ship / Ableton / training in this path.
- No consolidate REV7 inside this document.
- No fitting a new numeric cut against 6.435 / 5.838 (dB shopping).

### Option A — A-priori admission amend for sweep / checkpoint HF one-sided floor-union

**Idea (narrow):** before any consolidate or tip claim, write into candidate
CONTRACT language (then lock, only after GO) a **sweep-specific** or
**checkpoint-HF** admission rule that refuses to close the gate on cells
admitted solely by one-sided floor-union when the reference (or under-test)
is at the §6.1 clamp **and** the cell is not in the instantaneous
signal-bearing neighbourhood of the sweep peak at that checkpoint frame.

Concrete formula **not frozen here** (avoids shopping against 6.435). Design
slot must satisfy, a priori:

1. **Preserve one-sided artifact intent** for true SR defects (a real tone /
   artifact present only at 44.1/96 must still enter the max — pure
   “intersection both > −120” alone is **insufficient** as a complete
   replacement; see REV7 candidate §4 constraint 3).
2. **Target the skirt class:** cells where one side is exactly at floor and
   the other is a leakage/skirt residue far from the instantaneous chirp
   peak (diagnostic shape already recorded: peak near b111, FAIL at b106).
3. **Fail-closed vacuous:** empty active set on a required checkpoint ×
   valid channel → FAIL (not PASS).
4. **Stationary assets unchanged:** multitone / pseudo_noise closing rules
   on `R` must not be silently rewritten by the sweep clause.
5. **Written before** the re-measure that claims PASS.

| tradeoff | |
|----------|--|
| Pro | Closes the metrology question that currently blocks honest tip; aligns with living next-path `2c69606f`; keeps 0.25 intact; stationary PASS evidence remains meaningful. |
| Contro | Requires redteam + CC + later lock re-hash if accepted; risk of over-narrowing one-sided intent if poorly worded; **not** a tip authorization by itself. |
| Risk if skipped | Tip under REV6 would still claim a domain that measured FAIL — false progress. |

### Option B — Stay REV6 + durable debt + proceed G1b tip with explicit non-closing of sweep HF

**Idea:** do **not** amend admission. Tip G1b ufficiale under freeze REV6 /
lock `d2c35ccc…`, with PLAN + tip report stating explicitly that
**gate-4 SR-parity is not closed for `log_sweep` checkpoint HF** (durable
debt), while stationary-on-`R` results may be published as diagnostic /
partial evidence only — **never** as gate-4 PASS.

| tradeoff | |
|----------|--|
| Pro | No lock/CONTRACT mutation; fastest path to “tip exists”; honest if debt is loud and FAIL-closed in wording. |
| Contro | Product G1b tip ships with a known unmet §13.1/§13.2 obligation on a mandatory fixture; easy to launder into “almost PASS”; still blocks any true gate-4 PASS claim. |
| When justified | Only if Marco explicitly accepts **permanent** (or long-lived) non-closing of sweep HF as debt, with stop-rule that tip ≠ G1 PASS and ≠ gate-4 green. |

### Option C — Honest fixture / comparison-mode change (no dB shopping)

**Idea:** keep REV6 activity predicate for stationary assets; change the
**sweep comparison mode** a priori (hashed in lock) so the closing max only
includes bands in a preregistered **neighbourhood of the instantaneous
sweep frequency** at each checkpoint (e.g. bands whose triangular support
contains `f_inst(t_cross)` and/or ±K neighbour indices pinned before
measure) — **or** replace checkpoint PSD max with a preregistered
peak-tracking comparator that does not admit far skirts at floor.

This is **not** “drop b106 after seeing 6.435.” Neighbourhood width / rule
must be justified from window geometry (Hann main lobe / triangular
support) **before** re-measure.

| tradeoff | |
|----------|--|
| Pro | Attacks the mismatch “checkpoint frequency vs band that carries the chirp energy”; may preserve global union predicate for stationary tests; still a priori. |
| Contro | Amends §13.1 sweep closing procedure (lock `checkpoint_hz` / reachability / new neighbourhood constants) — still a contract/lock change later; must not shrink neighbourhood post-hoc to hide FAIL; redteam must attack vacuous PASS and cherry-picked K. |
| Forbidden variant | Deleting 16 kHz / 8 kHz checkpoints after FAIL; raising floor; widening 0.25. |

---

## 4. Locked decision (was: recommended option)

**LOCKED (Marco 2026-07-26):** **Option A** + mechanism **A3**
(sweep-scoped predicate **AND** neighbourhood of the instantaneous sweep
trajectory). **Normative reading (post-CC):** “around `f_inst`” =
triangular support ∩ `F_TRAJ` (PSD-window chirp image + lock `T_MEM`) as
pinned in the formula — **not** Option C’s peak-local ±K band pad (that
pad is explicitly REJECT). Option C’s geometry intuition is absorbed only
via `F_TRAJ`, not as a silent third freeze.
**Option B (tip-with-debt) rejected** as the living path.
Sweep-HF remains **fuori** dal veicolo REV7 LF / geometric-`R` until
measure PASSes under the new admission; sweep closing domain must not
silently become `ACTIVE∩R`.

Rationale (counsel already in PLAN @ `2c69606f`; now stamped):

1. Stationary-on-`R` already **PASS** — the open question is specifically
   sweep HF admission under floor-union, not LF inseparability.
2. Consolidating REV7 without closing this = shopping.
3. Option B is honest only as an explicit **debt tip**; Marco did **not**
   authorize tip-with-debt — A/A3 is the path.
4. Option C alone without admission language risks becoming a post-hoc
   band mask; packaging C **under** A’s a-priori constraints (A3) keeps
   fail-closed intent.

**Lab stance (locked):** **NO-GO** on G1b tip and **NO** on REV7
consolidate until A/A3 is redteamed + independently counter-checked
(ACCEPT or REJECT per §5).

---

## 5. Falsifiable ACCEPT / REJECT criteria

### 5.1 What a follow-up experiment / CC must produce

After a concrete formula draft under **A/A3** (sweep-scoped predicate +
neighbourhood around `f_inst`) is written **without** using 6.435 as a
fit target:

| outcome | verdict |
|---------|---------|
| Re-measure on gate platform (CPython 3.12.13 / lock env): `log_sweep` 44.1 & 96 vs 48, all preregistered checkpoints, under the **written** rule → `max\|Δ\| ≤ 0.25` on the **declared closing cell set**, **and** vacuous-FAIL checks pass, **and** stationary multitone/noise on `R` remain ≤ 0.25 without formula retune | **ACCEPT** candidate for later Guardian amend path (still ≠ consolidate in this doc; still ≠ G1 PASS) |
| Same re-measure still FAIL, but FAIL cells are **signal-bearing** both-sides-above-floor near `f_inst` (true SR defect) | **REJECT** “skirt-only” story → escalate: either deeper frontend bug hypothesis **or** Option B debt (not threshold shopping) |
| Formula only PASSes after widening neighbourhood / raising floor / dropping checkpoints / fitting K to 6.435 | **REJECT** as dB/admission shopping → **NO-GO** |
| Formula excludes all one-sided cells globally such that a planted SR-only artifact at 44.1 vanishes from the max | **REJECT** (violates one-sided artifact intent) |
| Empty active set on any required checkpoint × valid channel treated as PASS | **REJECT** (vacuous) |

### 5.2 Paper attacks redteam must run (before implement)

1. **One-sided intent:** construct (on paper) a cell with `psd_ref = −120`,
   `psd_sr = −100` at a band that **is** the instantaneous peak band —
   must remain ACTIVE / gate-visible.
2. **Skirt exclusion:** cell with ref at floor, sr above floor, band far
   from `f_inst` relative to pinned neighbourhood — must be inactive for
   closing max.
3. **No LF laundering:** confirm b106-class HF cannot be moved to report-only
   via `R`.
4. **Stationary non-regression:** A must not alter multitone/noise closing
   domain except by explicit shared predicates already accepted for `R`.
5. **Lock binding:** list exact lock keys that would change (§6).

**Redteam verdict language:** prefer
`CONTRACT-BROKEN | CONTRACT-POROUS | CONTRACT-SOUND` on the **proposal
formula**, not on this options doc alone. This file alone is **not**
SOUND for consolidate.

---

## 6. Explicit non-goals

| non-goal | |
|----------|--|
| Relax 0.25 → mean / p95 / RMSE / “soft max” | forbidden |
| Edit `Source/`, CMake, Resources, Ableton ship | forbidden |
| Training / model promotion | forbidden |
| Consolidate REV7 in this document | forbidden (**REV7 consolidate: NO**) |
| Claim G1 PASS or official G1b tip | forbidden |
| Mutate `metrology_lock` / SHA256SUMS claiming freeze | forbidden in this tranche |
| Implement `ml_v3/frontend` to “prove” the proposal | forbidden as next step (lab spike ≠ tip; measure only after formula + redteam non-BROKEN) |
| Raise `N_MIN` / shop Rayleigh constants against 6.435 | forbidden |
| Delete or demote HF checkpoints after seeing FAIL | forbidden |

---

## 7. Binding to metrology lock / SHA256SUMS (IF amend later chosen)

**This proposal does not re-hash anything.** If Option A (or C-under-A) later
receives Guardian GO to amend:

| artifact | action |
|----------|--------|
| `ml_v3/contracts/metrology_lock.py` → `sample_rate_parity.activity.predicate` (and any new sweep-neighbourhood / checkpoint-admission keys) | replace string(s); keep `threshold_max_abs_db: 0.25`; keep aggregator `max` |
| `union_cross_sr` / vacuous-FAIL flags | retain intent; extend only as formula requires |
| `SWEEP_CHECKPOINT_HZ` / reachability | unchanged unless Option C neighbourhood adds **new** hashed constants (not removals post-hoc) |
| `metrology_lock_sha256` (`d2c35ccc…`) | **recompute** |
| `fixture_spec` / dependencies embedding lock digest | update if digest-bound |
| `ml_v3/fixtures/g1/SHA256SUMS` | update **only** entries whose bytes change; do not self-hash SUMS |
| Freeze CONTRACT @ `6d254d0a` | superseded only by a later consolidate commit — **not** this proposal |

Silent lock edit without CONTRACT amend GO → **FAIL** / stop-rule BLOCKER.

---

## 8. Next permitted action (after DECISION STAMP)

**Living choices locked** (see DECISION STAMP). **In order:**

1. ~~Marco counter-check A vs B~~ — **DONE** 2026-07-26 (A / A3 / fuori REV7 LF).
2. ~~Concrete A/A3 formula draft~~ — **DONE** (doc-only):
   `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md`
   (`ACTIVE_sweep`, F1, `N`:=support∩`F_TRAJ`, vacuous/report/anti-launder).
3. ~~Tip packaging + independent CC @ `81e86dc5`~~ — **DONE**: verdict
   **CONTRACT-POROUS**; must-fixes 1–7 closed in formula prose (uncommitted
   until Marco OK). Formula **must not** self-declare `CONTRACT-SOUND`.
4. **Redteam/CC delta again** on the patched formula (empty-N /
   `CHK_CHIRP_REACHABLE` / T18 + tie-break / b106 lattice / level pins) —
   not on frontend code. **Re-measure blocked** until verdict ≠
   `CONTRACT-POROUS` / `CONTRACT-BROKEN`.
5. Only then: candidate CONTRACT amend path for **sweep admission**
   (still **separate** from full REV7 consolidate of LF+R; sweep-HF stays
   outside that vehicle until measure PASS; no `ACTIVE∩R` closing on sweep).
   Marco authorizes **commit** of this stamp / formula only after
   non-POROUS CC (or explicitly sooner).

**Not permitted as next step:** implement/close gate in `ml_v3/frontend/`;
ship; training; claim PASS from stationary-only tables; consolidate REV7
“because R PASS”; tip-with-debt (B) without a new Marco override.

---

## 9. Open questions for Marco — CLOSED (stamped)

| # | Question | Decision (2026-07-26) |
|---|----------|------------------------|
| 1 | Option A vs B tip-with-debt? | **A** (NOT B) |
| 2 | Mechanism: sweep-scoped / neighbourhood / both? | **A3 — BOTH** |
| 3 | Sweep-HF inside REV7 LF/`R` vehicle before PASS? | **NO — fuori** until measure PASS under new admission |

---

## 10. References (authority order used)

1. `docs/MOTORE_V3_PLAN.md` — living next-path @ `2c69606f`
2. `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` — §13.1 sweep, §13.2 gate 4
3. `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` + `.json`
4. `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`
5. `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`,
   `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`,
   `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` — context only; **not**
   consolidated here
6. `docs/EMBER_CORE_PARALLEL_HANDOFF.md` §A

---

≠ G1 PASS. ≠ REV7 consolidate. ≠ G1b tip. ≠ Ableton readiness.
