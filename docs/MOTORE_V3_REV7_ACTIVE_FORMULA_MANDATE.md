# MANDATO — FORMULA ACTIVE(b, ch) per §13.2 gate 4 (candidato REV7)

**Status:** AUTHORIZED TO START (2026-07-25) — formula writer must be untainted  
**Draft commit (diagnosis frozen first):** `2eac2387`  
**Candidate doc (do not use §1 numbers):** `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`

**Destinatario:** NON chi ha prodotto o letto l'evidenza WS4 (lineage contaminato).  
Counter-check giudica la formula, non la scrive: ha visto i RED.

**Deliverable path:** write formula + constant derivations into  
`docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`  
(Do **not** paste RED numbers into that file. Do **not** edit CONTRACT freeze,
lock, or SHA256SUMS. Optionally note the open slot reference in the candidate
doc without copying §1 evidence tables.)

---

## Compito

Scrivere la definizione concreta di `ACTIVE(b, ch)` — il predicato che ammette
una cella (banda `b`, canale `ch`) nel massimo del gate 4 di sample-rate
parity — rispettando i vincoli 1–8 sotto.

## La domanda fisica a cui rispondere

Quando la misura di una banda è determinata dal contenuto spettrale **dentro**
il proprio supporto, e quando invece è determinata da leakage di componenti
**fuori** dal supporto o dall'instabilità del floor numerico?

Il predicato deve ammettere le prime ed escludere le seconde.

## Geometria congelata (tutto derivabile a priori, nessuna misura)

- Analisi **sempre** a `fs_c = 48000` Hz: la geometria delle bande è identica
  per ogni sample rate sorgente.
- 120 bande, centri `center[i] = 20 * (20000/20)**(i/119)`, estremi pinnati a
  `20.0` e `20000.0` Hz. Rapporto fra centri adiacenti
  `r = 1000**(1/119) ≈ 1.05955` (larghezza relativa ~5.96%).
- Supporto triangolare su `log2(f)`: la banda `i` copre
  `center[i-1]..center[i+1]`; per le bande 0 e 119 si usano centri virtuali
  allo stesso rapporto `r`.
- Due griglie FFT: MAIN 4096 (~11.719 Hz/bin) e LF 8192 (~5.859 Hz/bin).
- Finestra Hann **periodica**: `w[n] = 0.5 - 0.5*cos(2*pi*n/N)`. Le sue
  proprietà di leakage (livello del primo lobo laterale, decadimento,
  larghezza del lobo principale in bin) sono la fonte legittima delle
  costanti.
- Fusione: LF puro `<= 160` Hz, MAIN puro `>= 320` Hz, crossfade raised-cosine
  su `log2` fra i due. Sotto 160 Hz la banda è alimentata **solo** da LF.
- PSD: floor lineare `1e-12`, poi clamp `[-120, +12]` dB. DC e bin oltre
  20000 Hz esclusi.

Allowed reads for geometry only: `docs/MOTORE_V3_G1_CONTRACT.md` §6/§7
(structure), `ml_v3/contracts/` band/grid helpers, `ml_v3/fixtures/g1/metrology_lock.json`
for **non-outcome** frozen constants (FFT sizes, window name, floors).  
Do **not** treat the current activity predicate string as sacred — replacing
it is the point of this candidate — but do **not** invent numbers from
measurement runs.

## Conseguenza geometrica già calcolabile (NON è un risultato di misura)

Incrociando i supporti triangolari con le due griglie si ottiene, per pura
aritmetica:

- griglia MAIN 4096: **14** bande con ZERO bin nel supporto, **21** con UN SOLO bin;
- griglia LF 8192: **6** bande con ZERO bin, **17** con UN SOLO bin.

Le bande interessate stanno nell'estremo basso della griglia. Chiunque può
riprodurlo in cinque righe dai centri e dalle griglie: usalo.

## Vincoli 1–8 (da REV7 candidate §4 — senza numeri di run)

1. **A priori.** Scritto prima della ri-misura che reclama PASS. Vietato
   scegliere cutoff scansionando celle/margini di una run fino a
   `max|Δ| ≤ 0.25`.
2. **No post-hoc mask.** Vietato nascondere bande dopo aver visto errori.
3. **Preserve one-sided artifact intent.** Un difetto presente a 44.1/96 ma
   non a 48 non deve sparire dal max solo perché il riferimento sta al floor.
   La sola intersezione `both > −120` è **insufficiente** come sostituto
   completo.
4. **Signal-bearing admission.** Escludere celle dominate da leakage /
   interferenza inter-componente o instabilità del floor numerico; trattenere
   celle con energia di segnale stabile rilevante per la SR parity. Definizione
   operativa in linguaggio da contratto **senza** fit a magnitudini di run.
5. **Inheritance unchanged.** Shape/prominence della banda `b` ereditano
   l'attività PSD della banda `b` sullo stesso canale; canali invalidi ignorati.
6. **Vacuous-PASS fail-closed.** Zero celle attive su un asset/portion
   richiesto → gate **FAIL** (dominio vuoto ≠ PASS).
7. **0.25 dB immutable.** Dominio e aggregatore max invariati.
8. **Re-measure required.** Dopo che la formula è scritta — non in questo
   task — si ri-misura. Questo task **non** esegue la ri-misura per “vedere
   se passa”.

## Standard di accettazione

Per **ogni** costante numerica nella formula si deve poter indicare da quale
quantità congelata deriva: lobi laterali Hann, larghezza lobo principale in
bin, spaziatura bin, rapporto `r` fra centri, soglie già nel contratto.
Una costante non derivabile da queste è un fit → formula respinta.

## Divieti

- NON chiedere, cercare o usare risultati numerici della run WS4: né
  `max|Δ|`, né quali bande/asset/SR hanno fallito, né tabelle di margine.
  Se ti vengono offerti, rifiutali e dichiaralo.
- NON aprire / leggere:
  - `ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.*`
  - `ml_v3/benchmark/` evidence outputs keyed to WS4 runs
  - agent transcripts about WS4 RED diagnostics
  - §1 “Trigger” tables inside `MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`
    (contengono numeri di run — i vincoli 1–8 sono già copiati qui)
- NON toccare 0.25 dB, aggregatore max, campi dominio, P1–P7, criteri
  −100 dBFS/Hz di §10.
- NON modificare CONTRACT REV6 (`6d254d0a`), metrology lock, SHA256SUMS.
- NON eseguire la ri-misura in questo task.

## Esito atteso

1. La formula, in linguaggio da contratto, in
   `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`.
2. Per ogni costante, una riga di derivazione da geometria/finestra.
3. Dichiarazione esplicita se ritieni che **nessun** criterio principiato
   possa ammettere in modo stabile le bande a zero/uno bin — in tal caso
   dillo invece di forzare una formula (scoperta: emendare il **dominio**
   del gate, non solo il predicato).

## Dopo (fuori scope di questo mandato)

metrology-redteam (false-PASS) → counter-check indipendente (ri-misura) →
secondo GO. Nessun consolidate prima.
