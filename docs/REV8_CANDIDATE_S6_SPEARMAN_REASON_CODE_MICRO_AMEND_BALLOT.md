# REV8 — CANDIDATE S6 — SPEARMAN REASON CODE MICRO-AMEND BALLOT

**Stato:** `SIGNED — APPROVED`

## 1. Oggetto

Questo ballot chiude esclusivamente S6, l'unico finding normativo aperto del
counter-check sul tranche O-09 group-level:

```text
Target normativo:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md §11.4

SHA-256 target:
8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041

Evidenza e counter-check:
docs/REV8_O09_GROUP_BENCHMARK_REPORT.md §6, §7

SHA-256 evidenza:
dbacafd272a732df5f9d2cc13ef7584677dcf4ccb7bdd2be82bf9ffecc418424

Implementazione interessata:
ml_v3/benchmark/rev8_o09_group_candidate.py (righe 131, 1205-1215)

SHA-256 implementazione:
53033d7fcc872420b5420ff4fb825762ddcfb4defa458ffdfdf82f124e3622f6

Commit di riferimento:
4eb2f63deaece5fcc6bd6b6486b97e1d6a5db309

Finding:
S6 — quarto reason code Spearman non enumerato in §11.4
```

La decisione non modifica K1-K4, `M*`, K6, `S_can`, `M_replay`, la formula
Spearman, le condizioni di pubblicabilità 1-3 di §11.4, né alcuna altra
metrica. Nomina soltanto un esito N/A che l'implementazione già produce in
modo conservativo e che il testo firmato non elencava.

## 2. Il gap chiuso

§11.4 enumera tre reason code Spearman:

```text
almeno un n<10             -> N/A / INSUFFICIENT_MATCHED_SUPPORT
almeno una varianza zero   -> N/A / SPEARMAN_UNDEFINED
rho non singleton          -> N/A / PAIRING_AMBIGUOUS
```

Il kernel candidate ne produce un quarto,
`SPEARMAN_CERTIFICATE_UNAVAILABLE`, raggiunto quando l'ottimo è ambiguo **e**
la procedura esatta non riesce a dimostrare né che le molteplicità dei valori
matched sono fisse su `M*`, né che non lo sono. In quel caso il codice non ha
stabilito la non-singolarità di rho, quindi `PAIRING_AMBIGUOUS` — che
asserisce "rho non è singleton" — sarebbe un'affermazione non provata.

Il comportamento è già fail-closed e non produce mai un falso PASS; mancava
soltanto l'autorità normativa che lo riconosce.

## 3. Decisione S6

```text
Nel caso Spearman in cui la procedura esatta richiesta non disponga di un
certificato sufficiente a dimostrare l'unicità del valore su M*, il risultato
è N/A con reason code SPEARMAN_CERTIFICATE_UNAVAILABLE.

Questo esito:
- non è PASS;
- non è zero;
- non è una failure runtime;
- non può essere convertito in PAIRING_AMBIGUOUS senza prova di non-singleton;
- impedisce il PASS di qualunque gate che richieda una Spearman definita.
```

## 4. Verifica dell'implementazione contro il testo firmato

Ogni clausola è stata confrontata con il comportamento reale del kernel prima
della firma:

| Clausola | Riscontro nel codice |
|---|---|
| non è PASS | `GroupStatus.NOT_APPLICABLE`, riga 1207 |
| non è zero | valore pubblicato `None`, riga 1209 |
| non è una failure runtime | `NOT_APPLICABLE`, non `REJECTED` — quest'ultimo è riservato a `SOLVER_RUNTIME_FAILURE`/`SOLVER_CONSTRAINT_MODEL_INVALID` |
| nessuna conversione in `PAIRING_AMBIGUOUS` senza prova | `PAIRING_AMBIGUOUS` è emesso solo dopo aver calcolato l'inviluppo min/max e trovato rho differenti (righe 1294-1301) |
| impedisce il PASS | esito N/A, che §11.4 già dichiara bloccante per un gate obbligatorio |

## 5. Scope della firma

La firma:

- approva la sola enumerazione del reason code
  `SPEARMAN_CERTIFICATE_UNAVAILABLE` nell'autorità §11.4;
- autorizza una patch document-only del draft normativo su un nuovo commit,
  con counter-check mirato sul nuovo SHA;
- **non** chiude il gap scientifico sottostante: il caso generale
  variable-value-marginal resta **non certificato**, e la limitazione
  `GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED` resta dichiarata;
- **non** autorizza codice, cap O-09, benchmark, runtime o training;
- **non** valuta `G_eligible`/`G_defined`/`G_NA` né i gate floor, che restano
  `NOT_EVALUATED`;
- **non** costituisce `REV8 SPEC GO`.

## 6. Deroga di processo — dichiarata, non implicita

**Questo è l'unico ballot del programma REV8 firmato senza il counter-check a
tre lenti indipendenti.** Ogni altra decisione — O-01…O-20, R23_01…R23_05,
B-001 — è passata da quel processo prima della firma.

Stato reale della verifica su S6 al momento della firma:

```text
Finding individuato da        = Claude (autore del report O-09)
Testo del micro-amend         = proposto da Codex, in revisione di metodo
Verifica clausola-per-clausola contro il codice = Claude, per lettura diretta
Counter-check a tre lenti     = NON ESEGUITO
```

Un prompt di revisione a tre lenti dedicato a questo micro-amend è stato
preparato (`PROMPT_S6_MICROAMEND_REVIEW.md`, esterno al repo) e non è stato
eseguito prima della firma. L'autorità scientifica ha scelto consapevolmente
di procedere: la deroga è registrata qui perché chi rilegge questo ballot in
futuro sappia distinguerlo dagli altri, non perché il difetto sia stato
ignorato.

Se una revisione successiva dovesse respingere o modificare il testo, questo
ballot va riaperto — la firma non lo rende immune.

## 7. Firma dell'autorità

```text
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-07-31

Policy firmata:
esito N/A con reason code SPEARMAN_CERTIFICATE_UNAVAILABLE quando il
certificato di unicità di rho su M* non è disponibile
```

## 8. Stato dopo la firma

```text
S6 reason code policy                   = FIRMATA
Patch document-only §11.4               = AUTORIZZATA, NON ANCORA APPLICATA
Counter-check nuovo SHA                 = OBBLIGATORIO
Counter-check a tre lenti su S6         = DOVUTO, NON ESEGUITO (deroga §6)
Caso generale variable-value-marginal   = NON CERTIFICATO (invariato)
G_eligible/G_defined/G_NA e gate floor  = NOT_EVALUATED (invariato)
Cap O-09                                = NON ATTIVI
REV8 SPEC GO                            = NO
Implementation/training                 = NON AUTORIZZATI
```
