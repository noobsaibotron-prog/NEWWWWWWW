# REV8 — Ballot V2 Counter-check Report

**Etichetta epistemica:** REPORTED / REPRODUCIBLE EVIDENCE
**Oggetto scientifico verificato:** `docs/REV8_SCIENTIFIC_AUTHORITY_BALLOT_PRECOMPILED_V2.md` al commit `759fa19c7ef09d900201e7b5e13ee9e5d23d2399`
**SHA-256 dell'oggetto verificato:** `9cb899694aefeb7af95f714e2bc07130d318fca528dc4cfeac1d47a164d7f629`
**Base:** `e2bee113c02356c1dff34201f5d4ef002598a23b`
**Freeze Round 2.2:** `95f1f798a2a97aec960eb86faf5968e73d076564`

## Limite della prova

Questo file rende persistente il resoconto dei counter-check eseguiti nella sessione Codex. Non contiene firme crittografiche dei revisori e un lettore del solo repository non può ricostruire i transcript originali. Perciò il verdetto resta `REPORTED`, non `FORMALLY PROVED` né verifica indipendente repository-side.

Il `3/3 CLEAN` riguarda coerenza, completezza firmabile e assenza dei finding materiali noti nel testo del ballot. Non dimostra conformità del codice, correttezza di A2, chiusura G1c o `REV8 SPEC GO`.

## Lenti indipendenti

1. **Optimizer lens:** obiettivi K1–K4, K5_RESERVED, exact arithmetic, `M*`, A1/A2, comparator S/D, cap e failure policy.
2. **Metrics lens:** temporal authority, aliasing, ambiguity envelope, support/N/A, Spearman e Average Precision.
3. **Semantics/governance lens:** famiglie di record, eligibility, structural validity, payload, identity, ordinali, K6 e dipendenze.

I tre revisori hanno operato read-only sullo stesso file e non hanno modificato il repository.

## Esiti

### Primo giro — 3/3 BLOCK

Finding principali:

- `exact_arith_v2.py` ed `event_matching.py` correnti non erano probatori per REV8;
- O-01 aveva due percorsi temporali non osservabili;
- O-04a non separava completamente le famiglie;
- O-08/O-20 non definivano un optimizer e un total order coerenti;
- O-10–O-14b lasciavano supporti, ambiguity policy e AP incompleti;
- O-15 non enumerava payload e scope strutturale.

### Secondo giro — 3/3 BLOCK/AMEND

Finding residui:

- il vecchio A1 require/forbid non realizzava la gerarchia S/D;
- frame mapping, metamorfica O-03 e Spearman multi-unità richiedevano specifica ulteriore;
- O-20 lasciava indefiniti endpoint key, top-level context e geometria class-dependent;
- O-04a e O-15 richiedevano rationale operativo e payload exact-key.

### Conferma finale ultra-mirata — 3/3 CLEAN

- **Optimizer:** `CLEAN` dopo sostituzione di A1 con exhaustive oracle, definizione di `M_can=argmin(S,D)`, `prediction_top_level_key=evaluation_unit_key` e geometria class-dependent.
- **Metrics:** `CLEAN` dopo la regola Spearman applicata a ogni matching del prodotto cartesiano degli optimum, compresi supporto e varianza zero.
- **Semantics/governance:** `CLEAN` dopo separazione di `evaluation_unit_key` scientifica da `annotation_top_level_key` diagnostica e chiusura delle chiavi O-20.

## Verifiche locali riportate

- `414` test canonici: `PASS`, comando:

  ```text
  PYTHONDONTWRITEBYTECODE=1 /Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest discover -s ml_v3/tests -v
  ```

- controllo statico ballot: 18 O-ID unici, 18 scelte firmabili, nessun residuo K1–K5/K5 attivo;
- perimetro del commit `759fa19c`: un solo nuovo file Markdown, nessuna modifica a codice, candidate, schema o documenti Round 2.2 congelati.

## Controverifica Claude successiva

Claude ha letto ballot, Decision Register e template e ha verificato direttamente il codice. Ha confermato la sostanza delle 18 voci e ha aggiunto tre note non bloccanti:

1. `exact_arith_v2.exact()` perde gli interi oltre `2^53` perché converte prima a `float`;
2. firmare O-01/O-15 richiede la migrazione parziale di `identity_v2.py`;
3. il `3/3 CLEAN` doveva essere etichettato `REPORTED` e ancorato a un artefatto.

Le tre note sono ora registrate nel ballot come provenance e DoD post-firma. Non modificano le 18 raccomandazioni scientifiche e non costituiscono autorizzazione a implementare.

## Verdetto limitato

```text
Ballot V2 scientific text = REPORTED 3/3 CLEAN
Ballot authority          = NON FIRMATO
Implementation conformity = NON DIMOSTRATA
Round 2.3 normative       = NON AUTORIZZATO prima delle firme
REV8 SPEC GO              = NO
```
