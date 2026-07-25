# MANDATO — riapertura scopo REV7 (post ri-misura + sonda densa)

**Status:** AUTHORIZED TO START — untainted writer only  
**Evidence (do not re-open for shopping):**  
`docs/MOTORE_V3_REV7_REMEASURE_R_REPORT.md`  
**Prior formula (reference, not sacred):**  
`docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`

## Destinatario

NON chi ha prodotto o letto i numeri di WS4 / ri-misura / sonda densa per
scegliere costanti. Counter-check giudica, non scrive.

## Diagnosi consegnata (fisica / geometria — non una costante)

1. Under-resolution 0/1-bin: già indirizzata da domain-on-`R` / `N_MIN=2`.
2. Ipotesi “solo fixture sparse / valli vuote” (**iii**): **testata e
   insufficiente** (dense probe FAIL ~1.19 dB).
3. Residuo diagnostico: su bande ancora `RESOLVED` sotto ENBW, i **centri
   delle bande vicine** possono stare **dentro il lobo principale** della
   Hann periodica congelata → inseparabilità inter-banda / interferenza
   locale. Questo è un fatto di geometria finestra↔griglia.

## Compito

Riscrivere lo scopo dell’emendamento (document-only) scegliendo e
giustificando a priori **una** direzione coerente:

- domain / field-definition amend; e/o
- criterio geometrico ripartendo da proprietà finestra/griglia
  (ENBW aperture **oppure** main-lobe separation **oppure** altro
  derivabile) — **sui propri termini**, senza chiedere “cosa passa”.

## Divieti

- NON chiedere / usare max|Δ| di run, tabelle margine, “quale N_MIN passa”.
- NON alzare `N_MIN` perché una misura è FAIL.
- NON toccare 0.25 dB, CONTRACT freeze, lock, SHA256SUMS.
- NON eseguire ri-misura in questo task.

## Esito

Proposta aggiornata + tabella costanti con derivazione; oppure dichiarazione
onesta che serve emendare il dominio/campi e non un predicato di ammissione.

Poi: redteam → ri-misura da giudice → secondo GO.
