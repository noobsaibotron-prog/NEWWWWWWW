# Motore v3 — CONTRACT REV7 CANDIDATE (document-only)

**Status:** **SUPERSEDED BY CONSOLIDATE** — living authority is
`docs/MOTORE_V3_G1_CONTRACT.md` REVISIONE 7 CONSOLIDATA (this package’s A+B
landed). ≠ G1 PASS · ≠ official G1b tip  
**Date:** 2026-07-26  
**Nature:** Historical packaging of **exactly two** closing amendments
(option C path). Kept as evidence pointer; do not treat as living freeze.

**Authority chain (evidence → consolidate):**
| link | commit / path |
|------|----------------|
| Gate-4 scope clause GO | `31216df4` — `docs/MOTORE_V3_G1_GATE4_SCOPE_CLAUSE.md` |
| LF report-only clause | `71159469` — `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` |
| Stationary `R` closing MEASURE-PASS | `8cf38625` — `docs/MOTORE_V3_G1_STATIONARY_R_CLOSING_MEASURE.md` |
| Candidate package (2nd GO tip) | `5e0d32fc` — this file’s packaging tip |
| Hygiene before consolidate | `0965f975` |
| Guardian CONSOLIDATE_AUTHORIZED | YES (agent `2cc4e2c2` on `5e0d32fc`) |
| Marco authorize consolidate | "si" |

---

## Consolidated amendments (exactly A+B — no third)

### A — LF report-only ∉R
Geometric `R` = ENBW `N_MIN = 2` ∧ Rayleigh `SEPARATION_MIN_BINS = 2`
(fail-closed on fusion crossfade). `i ∈ R` closes gate 4; `i ∉ R`
report-only with mandatory publish (omit → report FAIL).

### B — Gate-4 scope (option C)
Closing set = stationary `multitone` + `pseudo_noise` on `R`. `log_sweep`
hashed, does **not** close (report-only). Non-stat SR-parity → `G1e-nonstat`
named debt (post-scope-closure destination; this file remains historical).
A3 RETIRED / ACTIVE closed.

### Immutables (unchanged)
max aggregator · 0.25 dB · R definition · mandatory publish outside R ·
A3/ACTIVE retired.

---

## Explicit non-claims

```text
≠ G1 PASS product claim
≠ reopen sweep as closing
≠ threshold shopping
≠ official G1b tip (separate auth after consolidate + rehash)
```

## Sequence

1. Guardian second GO on package — **done** @ `5e0d32fc`
2. Hygiene — **done** @ `0965f975`
3. Marco-authorized consolidate A+B into CONTRACT — **this consolidate**
4. Rehash metrology lock + SHA256SUMS — coordinated follow-up commit
5. Official G1b tip — **STOP** until separate Marco auth

Living next-path: G1b tip promotion (separate authorization). Parked sweep
proposal remains `G1e-nonstat` input debt.
