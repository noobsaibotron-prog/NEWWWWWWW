# REV8 — ROUND 2.3 — R23C CONFIRMATION BALLOT

**Stato:** `SIGNED — APPROVED`

## 1. Oggetto della decisione

Questo ballot riguarda esclusivamente le due chiarificazioni R23C introdotte
nel seguente documento:

```text
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md

SHA-256:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

La firma vale soltanto per questi byte. Qualunque modifica del target dopo la
firma invalida il ballot e richiede un nuovo digest e un nuovo controllo.

## 2. Catena di provenance

```text
Predecessore R23A:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_AMENDED_DRAFT.md
SHA-256:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011

Ballot R23A firmato:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_MICRO_AMEND_BALLOT.md
SHA-256:
4db761eeb017cd04595a1ac64943b535809675adefba9fe3d5cf8083336c8666

Recheck report R23A:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_RECHECK_REPORT.md
SHA-256:
0e4a92f1cfa2c11193a3f3dbb1f3f5809b743486509ba469ce5f08477467f44d

Counter-check pre-firma R23C:
docs/REV8_ROUND2_3_R23C_PRE_SIGNATURE_COUNTERCHECK_REPORT.md
SHA-256:
b9a157bc50a7b36af7c7cfc36c3e3149d83881b0c4714387f789a63c1b5b577b
Verdetto:
3/3 CLEAN
```

## 3. Decisione R23C_01 — dispatch normativo della macro-AP

### Testo approvato

Per qualunque media non pesata di valori binary64:

```text
sum_pairwise64 = fase di somma interna
mean64         = divisione finale della somma per il numero di elementi
```

La macro-AP normativa è:

```text
macro_AP64 =
mean64(AP_group64 ordinati per group_id in ordine byte UTF-8)
```

Il risultato grezzo di `sum_pairwise64` non è la macro-AP finale.

### Golden vincolante

```text
AP_group64 = [0.2, 0.3, 1.0]

sum-only mutation:
1.5 = f64:3ff8000000000000

mean64 normativo:
0.5 = f64:3fe0000000000000
```

### Classificazione

```text
R23C_01 = chiarificazione normativa del dispatch dell’aggregatore
nuova decisione scientifica = NO
```

## 4. Decisione R23C_02 — separazione matching, metriche e replay

### Testo approvato

Con il prediction set fissato, oppure all’interno di un insieme `P_t`
congelato:

```text
severity
confidence
actionable
prediction ID
hash
```

non possono influenzare:

```text
eligibility
V*
M*
S
S_can
```

Gli usi scientifici esplicitamente firmati restano ammessi:

```text
severity   -> formula della severity MAE in §11.2
confidence -> formazione di P_t e AP in §13
```

La separazione fra matching scientifico e replay è:

```text
matching scientifico:
eligibility, V*, M*, S, S_can

replay diagnostico:
D, M_replay
```

`D` e `M_replay` non possono scegliere il pairing scientifico né alimentare
metriche normative o gate. Possono produrre soltanto valori esplicitamente
etichettati `replay-only` e diagnostici.

### Fixture vincolanti

```text
severity_P 0 -> 1:
Q_severity 0 -> 1
V*, M*, S_can invariati

confidence TP>FP -> AP=1
confidence FP>TP -> AP=1/2

tie K6:
V*, M*, S_can invariati
M_replay può cambiare
output scientifici, metriche normative e gate invariati
rispetto alla scelta di replay
```

### Classificazione

```text
R23C_02 = chiarificazione normativa dei canali scientifici e diagnostici
nuova decisione scientifica = NO
```

## 5. Scope della firma

La firma:

- approva atomicamente `R23C_01` e `R23C_02`;
- non modifica le decisioni R23A già firmate;
- non autorizza modifiche al codice;
- non autorizza patch del candidate;
- non autorizza training;
- non costituisce `REV8 SPEC GO`;
- autorizza soltanto il recheck post-firma sullo SHA del target e su questo
  ballot firmato.

Una decisione parziale richiede un nuovo ballot. Non è ammessa una firma
parziale su uno solo dei due punti.

## 6. Firma dell’autorità

Compilare tutti i campi:

```text
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-07-29

SHA-256 target verificato:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

## 7. Stato dopo la firma

Anche in caso di `APPROVO`:

```text
R23C decisioni              = FIRMATE
Recheck post-firma          = ANCORA RICHIESTO
REV8 SPEC GO                = NO
Candidate patch             = NON AUTORIZZATA
Implementation / training   = NON AUTORIZZATI
```
