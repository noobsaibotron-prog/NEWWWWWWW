# MANDATO — SWEEP METROLOGY REDESIGN (log sweep / SR-parity)

**Status:** AUTHORIZED TO OPEN — document-only mandate  
**Date:** 2026-07-26  
**Authority tip at open:** `78da84dd` (A3 falsification measure FAIL honest)  
**Decision owner:** Marco (architectural)

**≠** formula implement · **≠** measure · **≠** CONTRACT amend · **≠** lock /
SHA256SUMS / T6 mutate · **≠** Source · **≠** REV7 consolidate · **≠** G1b tip

---

## Goal

Define a **physically appropriate** sample-rate parity meter for the
**log sweep** observed through the **causal dual-resolution frontend**.

Decision line (locked by this mandate):

> Stop making the current sweep full-vector test pass. Establish whether
> that test asks a physically well-defined property. Stationary evidence ≈
> yes; chirp evidence says no — concentrate next work there.

---

## Frozen photograph (lab state at mandate open)

```text
G1a CLOSE                         GO
REV6                              authority corrente
stationary parity su R            evidence positiva
streaming spike                   feasibility positiva
sweep full-vector                 FAIL
A3                                falsificata come soluzione (archival CC/redteam in flight)
REV7                              NON consolidated
G1b official tip                  NON esiste
0.25 dB                           intoccato
```

---

## A3 status (do not reopen ACTIVE-family)

- **A3** is **retired-as-solution**, pending archival redteam / counter-check
  stamps (in flight). It may be well-specified in prose and still **wrong as
  a solution**.
- Do **not** reopen ACTIVE-family fixes (**A4+**).
- Archival stamps may close the record; they do **not** authorize a new
  ACTIVE amend, threshold shopping, or “one more hole close” path.

---

## Workstreams under this mandate

### S1 — Endpoint reachability

Resolve a priori the incompatibility between:

- checkpoint grid endpoints (20 Hz / 20 kHz),
- sweep endpoint,
- STFT **right-aligned**,
- nearest `source_time`.

**Allowed direction only:** guard regions derived **solely** from frozen
window / resampler memory / hop (geometry and causality already pinned).

**Forbidden:**

- frame clamp into the chirp,
- checkpoint removal to make the gate pass,
- opportunistic second-nearest frame selection,
- manual time shift / proxy timestamps.

### S2 — Non-stationary observable

Define the **sweep-specific** observable for SR-parity on the chirp.

**Formula MUST derive from:**

- analytic chirp trajectory,
- frozen Hann,
- frozen FFT / fusion,
- frozen causality / timestamps.

**MUST NOT derive from:**

- band `b106` (or any failing-band list),
- measured magnitude `6.435` dB (or any run max|Δ|),
- margin tables,
- failing-band inventories from prior runs.

**CRITICAL PIN — ridge is not the locked solution:**

- This mandate asks the **physical question**, not “implement the ridge
  formula we already decided.”
- An untainted proposal **may** choose a ridge/trajectory formulation **or**
  another formulation derived from the **same physics** if better justified.
- Ridge / trajectory is an allowed **direction family**, not a frozen formula.
- Forbidden: a mandate (or proposal) that treats a pre-chosen ridge metric
  as the solution to be rubber-stamped.

### Off-ridge (must remain observable)

Off-ridge energy / artifacts **must remain observable**:

- either as an **artifact veto**, or
- as a **mandatory preregistered report**.

They **cannot** simply vanish from the test by domain surgery or silent
masking.

---

## Immutable (not open for redesign)

| Item | Rule |
|------|------|
| Aggregator | **max** (no mean / p95 / soft-max) |
| Threshold | **0.25 dB** hard |
| Stationary domain | geometric **`R`** remains the gate-closing domain for stationary cells (as already decided for REV7 packaging) |
| Report-only `∉R` | stays **report-only** (does not close the gate) |
| Post-hoc mask | **forbidden** |
| Threshold shopping | **forbidden** |

---

## Output of future work under this mandate

**Document-only proposal** that closes S1 + S2 (and states off-ridge policy)
without implementing or measuring.

In the **proposal phase**:

- **No measure.**
- **No** modification of `docs/MOTORE_V3_G1_CONTRACT.md` freeze,
  metrology lock, SHA256SUMS, T6 fixtures/generators, or `Source/`.

Expected deliverable path (when authorized as a separate write):

`docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_PROPOSAL.md`

(name may match sibling `MOTORE_V3_*_PROPOSAL.md` style; do not invent
code paths).

---

## REV7 policy (document — do not consolidate)

Wait until **S1 + S2 are resolved**. Then **one** REV7 package that binds,
in a single consolidate moment:

- geometric `R` + report-only `∉R`,
- sweep endpoint / reachability rule (S1),
- non-stationary observable (S2),
- off-ridge policy,
- fixture / T6 consequences,
- lock / hash update.

**Sequence after S1+S2 proposal (not this mandate’s write):**

1. metrology-redteam (false-PASS surface)
2. independent counter-check
3. **one** fresh measure
4. Guardian GO
5. consolidate
6. rehash
7. official G1b tip

Notes:

- Spike frontend ≠ full G1b tip.
- Partial consolidate (LF-only, stationary-only, A3-only) remains **NO**.

---

## Destinatario / contamination

Untainted writer for the future **proposal**. Do not fit constants to:

- A3 falsification magnitudes,
- WS4 / R-remeasure failing-band lists,
- margin tables.

Allowed geometry reads (non-outcome): frozen CONTRACT structure (§6/§7),
window / hop / resampler memory pins, analytic chirp definition, timestamp
causality rules already in contract/lock **as structure**, not as run
outcomes.

If you discover you know a run magnitude that would steer the formula,
**declare contamination and stop**.

---

## Divieti (this open + proposal phase)

- NON implementare formula / frontend / evaluator.
- NON eseguire misura o ri-misura “per vedere se passa”.
- NON toccare CONTRACT freeze REV6, metrology lock, SHA256SUMS, T6.
- NON toccare `Source/`, CMake, Resources, ship weights.
- NON rilassare 0.25 dB / cambiare aggregatore max.
- NON riaprire ACTIVE-family A4+; NON trattare A3 FAIL come PASS.
- NON consolidare REV7 in questo mandato.
- NON dichiarare G1 PASS o tip G1b ufficiale.

---

## Esito di questo documento

1. Mandato aperto: domanda fisica + vincoli S1/S2 + immutabili + off-ridge.
2. Fotografia di stato congelata (sopra).
3. A3 retired-as-solution (archival stamps in flight).
4. Prossima scrittura autorizzata separatamente: proposal document-only —
   **non** measure, **non** lock, **non** consolidate.
