# MANDATO — riapertura scopo REV7 (post ri-misura + sonda densa)

**Status:** AUTHORIZED TO START — untainted writer only  
**Date:** 2026-07-25

## Destinatario

NON chi ha prodotto o letto i numeri di esito (WS4 / ri-misura / sonda
densa) per scegliere costanti. Counter-check giudica, non scrive.

**Contenimento:** non consultare report di ri-misura, evidenze di spike
WS4, né il candidate REV7 (contengono magnitudini di fail). Se ti vengono
offerti, rifiutali e dichiaralo. La propria proposta ACTIVE precedente
può essere riusata **a memoria / da copia locale già nota** senza
riaprire documenti che la citano insieme agli esiti.

**Test di integrità:** se ti accorgi di conoscere l’entità numerica di
qualunque fallimento di gate (max|Δ|, bande colpevoli, asset/SR che
falliscono), dichiaralo **prima** di scrivere e fermati — la
contaminazione deve emergere, non restare implicita.

## Diagnosi consegnata (fisica / geometria — non una costante)

1. Under-resolution 0/1-bin: già indirizzata da domain-on-`R` /
   criterio ENBW con `N_MIN = 2` nella proposta precedente.
2. Ipotesi “solo fixture sparse / valli vuote” (**iii**): **testata** —
   l’eccitazione densa (una componente per centro di banda, stesse regole
   di ammissione) **non ha chiuso il gate**. Quindi (iii) è
   **insufficiente** come spiegazione completa. Nessuna cifra di esito
   è fornita qui di proposito.
3. Residuo diagnostico: su bande ancora `RESOLVED` sotto ENBW, i **centri
   delle bande vicine** possono stare **dentro il lobo principale** della
   Hann periodica congelata → inseparabilità inter-banda / interferenza
   locale. Questo è un fatto di geometria finestra↔griglia.

## Geometria congelata (riuso del mandato ACTIVE; nessuna misura)

- `fs_c = 48000`; 120 bande; `center[i] = 20*(20000/20)**(i/119)`;
  `r = 1000**(1/119)`; supporto triangolare su `log2(f)`.
- MAIN 4096 / LF 8192; Hann periodica; fusione 160/320 Hz.
- Occupancy geometrica (aritmetica): MAIN 14 zero-bin + 21 single-bin;
  LF 6 zero-bin + 17 single-bin.
- Proprietà Hann legittime come fonte costanti: ENBW = 1.5 bin;
  lobo principale null-to-null = 4 bin; primo sidelobe classico.

## Compito

Riscrivere lo scopo dell’emendamento (document-only) in  
`docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`  
scegliendo e giustificando a priori **una** direzione coerente:

- domain / field-definition amend; e/o
- criterio geometrico ripartendo da proprietà finestra/griglia
  (ENBW aperture **oppure** main-lobe separation **oppure** altro
  derivabile) — **sui propri termini**, senza chiedere “cosa passa”.

## Divieti

- NON chiedere / usare max|Δ| di run, tabelle margine, “quale N_MIN passa”.
- NON alzare `N_MIN` perché una misura è FAIL.
- NON toccare 0.25 dB, CONTRACT freeze, lock, SHA256SUMS.
- NON eseguire ri-misura in questo task.
- NON aprire file il cui nome suggerisce evidenza/esito di run
  (`*REMEASURE*`, `*WS4*EVIDENCE*`, `*REV7_CANDIDATE*`, `sr_parity`
  evidence dumps).

## Esito

1. Proposta aggiornata + tabella costanti con derivazione; **oppure**
   dichiarazione onesta che serve emendare il dominio/campi e non un
   predicato di ammissione.
2. Dichiarazione di non-contaminazione (o stop se contaminato).

Poi (fuori scope): redteam → ri-misura da giudice → secondo GO.
