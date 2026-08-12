# REV8 — ROUND 2.3 — RESTRICTED RED-TEAM RECHECK REPORT

**Verdetto consolidato:** `BLOCK`

## 1. Artefatti verificati

```text
Target:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_AMENDED_DRAFT.md

SHA-256 iniziale:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011

SHA-256 finale:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011

Ballot R23A firmato:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_MICRO_AMEND_BALLOT.md

SHA-256 ballot:
4db761eeb017cd04595a1ac64943b535809675adefba9fe3d5cf8083336c8666
```

Il target non è stato modificato durante il recheck.

```text
REV8 SPEC GO = NO
Candidate patch = NON AUTORIZZATA
Implementation  = NON AUTORIZZATA
Training        = NON AUTORIZZATO
```

---

## 2. Esiti delle tre lenti

| Lens | Scope | Verdetto | Finding materiali |
|---|---|---:|---:|
| Lens A | Numeric / Optimizer | BLOCK | 1 BLOCKER |
| Lens B | Metrics / Statistics | CLEAN | 0 |
| Lens C | Semantics / Security / Governance | BLOCK | 1 BLOCKER |

Il consolidamento non usa voto di maggioranza. Entrambi i blocker sono stati
riprodotti dal consolidatore sul target esatto.

---

## 3. Stato dei cinque finding originari

| Finding | Stato recheck | Evidenza |
|---|---|---|
| RT-001 | CLEAN | single-round `f64:3fad0022f7fa735f`; double-round vietato `f64:3fad0022f7fa7360` |
| RT-002 | BLOCK | separazione `S_can`/`M_replay` corretta, ma divieto sui campi metrici troppo ampio |
| RT-003 | BLOCK | `mean64` corretto, ma una regola storica conserva macro-AP come somma |
| RT-004 | CLEAN | N64 adiacenti verificati; equality razionale irraggiungibile |
| RT-005 | CLEAN | bit-length definito; golden 80000; cap ancora inattivo e atomico |

---

## 4. RC-001 — BLOCKER — Macro-AP definita sia come somma sia come media

### Contraddizione

La sezione 2.3 conserva:

```text
la macro-AP usa sum_pairwise64
```

La sezione 13, coerentemente con R23A_03 firmata, prescrive invece:

```text
la macro-AP usa mean64 sugli AP_group64
```

`sum_pairwise64` è soltanto la fase di somma interna a `mean64`; non è
l’aggregatore finale.

### Controesempio

Per tre gruppi con:

```text
AP_group64 = [0.2, 0.3, 1.0]
```

si ottiene:

```text
sum_pairwise64 = 1.5
                 f64:3ff8000000000000

mean64          = 0.5
                 f64:3fe0000000000000
```

La prima lettura può pubblicare un’Average Precision maggiore di 1 e invertire
un gate.

### Correzione minima

In §2.3:

1. chiarire che `sum_pairwise64` è la somma interna usata da `mean64`;
2. sostituire “la macro-AP usa `sum_pairwise64`” con:

   ```text
   la macro-AP usa mean64 sugli AP_group64 ordinati per group_id
   ```

Non è richiesta una nuova decisione scientifica: la correzione riallinea il
testo a R23A_03 già firmata.

---

## 5. RC-002 — BLOCKER — Il divieto R23A_02 elimina usi metrici espliciti

### Contraddizione

La sezione 9 afferma che severity, confidence e actionable non possono
influire su:

```text
metriche o gate
```

La stessa specifica definisce però:

```text
Q_severity(M) =
  sum abs(severity_GT-severity_P)/K1
```

e costruisce Average Precision mediante threshold sui valori di confidence.
Questi sono usi metrici intenzionali e firmati.

La mutation:

```text
lasciare che D o M_replay influenzino metriche o gate
```

è corretta soltanto se “influenzino” significa scegliere il pairing o
introdurre una dipendenza aggiuntiva attraverso il percorso diagnostico.

### Controesempio

Una GT e una prediction con geometria fissa e matching unico:

```text
severity_GT = 0
severity_P  = 0 -> severity MAE = 0
severity_P  = 1 -> severity MAE = 1
```

`V*`, `M*` e `S_can` restano invariati, ma la metrica cambia come deve.
Il divieto letterale rende impossibile implementare simultaneamente §9 e
§11.2.

### Correzione minima

Limitare il divieto al percorso di pairing:

```text
Severity, confidence e actionable non possono influire su eligibility, V*,
M*, S o S_can. I loro usi metrici esplicitamente definiti restano normativi.
D e M_replay non possono selezionare il pairing usato da metriche o gate né
introdurre dipendenze ulteriori rispetto alle formule metriche firmate.
```

Aggiornare coerentemente §14.5 e §14.6:

```text
modificare campi diagnostici non consumati dalla metrica non cambia la metrica;
D/M_replay non possono scegliere il pairing scientifico o alimentare gate.
```

Anche questa è una correzione di scope del wording R23A_02, non una nuova
preferenza scientifica.

---

## 6. Controlli risultati CLEAN

Il consolidatore e le lenti hanno verificato:

- `RT-001`: interval refinement dell’intera espressione `log2(R)/K1`;
- `RT-003`: `mean64([0.2,0.3,1.0]) =
  f64:3fe0000000000000`, escluso il percorso divide-first;
- `RT-004`:

  ```text
  inside_bits + 1 = outside_bits
  inside^3 < 2*min^3
  outside^3 > 2*min^3
  ```

- prova che nessun razionale soddisfa `q^3=2`;
- `RT-005`:

  ```text
  rational_bit_length(2^39999/(2^39999+1)) = 80000
  ```

- ceiling O-09 ancora inattivo e attivabile soltanto insieme a bound, scope,
  soglia, fixture, reason code e blast radius;
- nessun residuo `M_can`;
- A1/A2 coerenti su `V*`, `M*`, `S_can` e `M_replay`;
- predecessor, report e ballot vincolati agli SHA dichiarati;
- la firma esterna del ballot vincola correttamente il target pre-firma senza
  mutarne lo SHA;
- `REV8 SPEC GO = NO` ancora esplicito.

---

## 7. Verdetto e next permitted action

```text
Round 2.3 amended draft = BLOCK
REV8 SPEC GO            = NO
```

**Next permitted action:** micro-correzione documentale limitata a `RC-001` e
`RC-002`, seguita da:

1. nuovo SHA del target;
2. ballot di conferma che vincoli il nuovo SHA;
3. recheck ultra-ristretto delle sole due correzioni;
4. soltanto con esito `CLEAN`, patch document-only del candidate REV8.

Non è autorizzata alcuna modifica a codice, schema live, runtime o training.
