# Motore v3 — G1 Gate-4 Scope Clause (option C)

**Status:** PHASE SCOPE DECISION — **CONSOLIDATED into CONTRACT REV7**  
**Date:** 2026-07-26  
**Authority:** Marco — option (C)  
**Nature:** Scope decision for what closes gate 4 in G1. **Not a relaxation.**  
**Living authority:** `docs/MOTORE_V3_G1_CONTRACT.md` REVISIONE 7 CONSOLIDATA  
**≠** G1 PASS · ≠ official G1b tip · ≠ A3/A4/ACTIVE reopen · ≠ threshold shopping

**LF report-only (paired amend A):**  
`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` @ `71159469`  
**Parked non-stat input:** mandate  
`docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_MANDATE.md` + proposal  
`docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_PROPOSAL.md` tip ~`9b8f8305`  
(30 closed findings parked, not deleted; ~15 open at park).

This clause’s six points are now normative inside CONTRACT REV7 (amend B).
Historical REV6 freeze tip `6d254d0a` remains ancestor; living freeze is REV7.

---

## Normative (exactly six points)

1. **Gate-4 closing set in G1** = stationary assets only (`multitone`,
   `pseudo_noise`), evaluated on geometric domain **R**.

2. **`log_sweep`:** fixture remains generated and hashed in SHA256SUMS; **does
   not close** gate 4 in G1. Deviations still measured and published as
   **report-only** (preserve information, zero cost to closing).

3. **Non-stationary SR-parity** is a **named requirement of G1c/G1e**, with
   existing mandate + proposal as parked input (~1056 lines + 30 closed
   findings parked, not deleted). Tip of parked proposal ~`9b8f8305` / living
   HEAD as appropriate.

4. **Immutables (verbatim from existing contract language / REV7 geometry
   package — unchanged here):**
   - aggregator: **max** (`max_i |x_i(sr) - x_i(48k)|`; one active cell out of
     threshold → FAIL entire gate; no mean / p95 / RMSE substitute);
   - threshold: **0.25 dB**;
   - definition of **R**: ENBW `N_MIN = 2 = ceil(ENBW_Hann)` **and** Rayleigh
     `SEPARATION_MIN_BINS = 2` (main-lobe null-to-null / 2), fail-closed on
     fusion crossfade — see
     `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` §2 /
     `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`;
   - mandatory publish **outside R** (report-only table; omit → report FAIL).

5. **A3 remains RETIRED**; ACTIVE family closed. No reopen.

6. **Explicit nature:** this is a **phase scope decision, not a relaxation**.
   No threshold changed; no cell newly admitted that was not before.

---

## Debt entry (handoff §A mirror)

**Parity cross-SR non-stazionaria: non verificata a G1.** Input parcheggiati:
mandato `SWEEP_METROLOGY_REDESIGN` + proposta S1/S2 (30 finding chiusi, ~15
aperti al park). Da riprendere a G1c/G1e quando esiste un osservabile. Non è
un difetto noto del frontend: Independent CC su WS4 (spike tip `c7f05871`)
ha misurato accordo ~0.03 dB sulle bin con segnale tra audio 48 kHz diretto e
44.1→48 ricampionato (evidenza lab — **cite only**, non criterio / non soglia).

**Stationary close — binding peak `level` (watch):** sulla misura di chiusura
autoritativa `docs/MOTORE_V3_G1_STATIONARY_R_CLOSING_MEASURE.md` @ `8cf38625`,
il peak in tutte e quattro le celle di chiusura è broadband **`level`**
(MAIN-window RMS / `mid_level_dbfs`), non shape/psd per-banda. Causa fisica
plausibile: bordo di passabanda del resampler vicino a ~20 kHz. È l’avvicinamento
più stretto a 0.25 nel closing set (max|Δ|=0.1915 → ~23% headroom) — **watch
item** per il futuro; **non** FAIL oggi. Distinto dal debito non-stat /
sweep parcheggiato sopra.

---

## Forbidden by this clause

- Any new constant not already in contract / geometry package  
- Touch 0.25 / max / R / report-only obligation  
- Edit freeze CONTRACT, metrology lock, SHA256SUMS  
- Claim G1 PASS  
- Reopen A3 / A4 / ACTIVE  
- Self-SOUND on sweep proposal  
- Measure under this docs-only decision  
- Amend sweep proposal body to close POROUS findings  
