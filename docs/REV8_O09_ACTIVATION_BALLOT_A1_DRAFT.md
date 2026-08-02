# REV8 — O-09 — ACTIVATION BALLOT ADDENDUM (A1, NARROW)

**Stato:** `DRAFT — PENDING SIGNATURE`

Questo documento è **preparato, non firmato**. Nulla è attivo finché la §9 non
è compilata dall'autorità scientifica. Fino ad allora `REV8 SPEC GO = NO` e i
cap restano `PROVISIONAL_PENDING_BENCHMARK_O09`.

## 1. Oggetto e autorità invocata

Il ledger §15 del draft normativo firmato assegna a O-09 questa condizione di
materializzazione, unica fra tutte le righe del ledger:

```text
| O-09 | fail policy | FIRMATA; NUMERI PROVVISORI | benchmark + ballot addendum di attivazione |
```

§10 dichiara i quattro ceiling `PROVISIONAL_PENDING_BENCHMARK_O09` e stabilisce
che «non producono PASS/FAIL finché un ballot addendum, dopo benchmark A2 sul
platform lock, non li attiva».

Questo è quel ballot addendum, **ristretto ai soli quattro ceiling di §10**.

```text
Target normativo:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md §10

SHA-256 target:
be8658203e26fae3bc36d020733b6b7ed773d24ed0e78675330f08b459ff56cc

Evidenza per-sottografo (counter-check a tre lenti, commit 030ec8bb):
docs/REV8_O09_PER_SUBGRAPH_BENCHMARK_REPORT.md
SHA-256: 0789e9070fa959e8cd15f18961da7b053562245c5762c953fd6adc92a563977f

Evidenza group-level (revisore singolo + Hermes sugli hash):
docs/REV8_O09_GROUP_BENCHMARK_REPORT.md
SHA-256: 508906fdc7fc69f7e8411255f1fed74e1e61055eada11d5e0b5fbcc7f4fbac1a

Implementazione:
ml_v3/benchmark/rev8_o09_candidate.py
SHA-256: bb130138423bbf7bc0d6fe3ef3d2a3d6b27464244daa893b039ab1744860ec9a

Commit di riferimento: ea8e22b257ba95f06de60726a802470b41178985
```

## 2. Cap — i quattro numeri attivati

```text
GT                                <= 128
prediction                        <= 128
eligible edges                    <= 16384
reduced mathematical bit-length   <= 65536
```

I numeri **non cambiano**: il ballot conferma quelli già scritti in §10,
rimuovendone lo stato provvisorio. Nessun numero nuovo è introdotto.

### 2.1 Il ceiling sugli archi è derivato, non indipendente

Verificato per esecuzione, non per lettura:

```text
PROVISIONAL_MAX_GT * PROVISIONAL_MAX_PREDICTIONS = 128 * 128 = 16384
PROVISIONAL_MAX_EDGES                            =            16384
identici: True
```

`CandidateGraph.__post_init__` rifiuta le coppie `(gt, prediction)` duplicate
(«duplicate eligible edge»), quindi il numero massimo di archi distinti in un
grafo che rispetta GT ≤ 128 e prediction ≤ 128 è esattamente 16.384. Ne segue
che **`ELIGIBLE_EDGES` non può mai essere l'unica causa di superamento**: un
grafo con più di 16.384 archi viola necessariamente anche uno degli altri due
ceiling. Il probe di bordo lo conferma: a 16.385 archi lo stato riportato è
`('GT', 'ELIGIBLE_EDGES')`, mai `('ELIGIBLE_EDGES',)` da solo.

Attivarlo è innocuo e coerente, ma va registrato che **non offre protezione
aggiuntiva** rispetto ai due ceiling dimensionali. Chi legge §10 in futuro non
deve attribuirgli un potere che non ha.

## 3. Scope — cosa è attivato e cosa esplicitamente no

**ATTIVATO:** i quattro ceiling di §10, per sottografo
`(evaluation_unit_key, record_family, problem_type)`, valutati dal preflight
prima del calcolo parziale.

**NON ATTIVATO — fuori scope dichiarato:**

- I **16 cap group-level** definiti in `ml_v3/benchmark/rev8_o09_group_candidate.py`
  (`AP_MAX_PARTITIONS`, `AP_MAX_TOTAL_GT`, `AP_MAX_TOTAL_PREDICTIONS`,
  `AP_MAX_TOTAL_EDGES`, `AP_MAX_DISTINCT_THRESHOLDS`, `AP_MAX_LOCAL_THRESHOLDS`,
  `AP_MAX_PREFIX_EDGE_INCIDENCE`, `COVERAGE_MAX_UNITS`,
  `COVERAGE_MAX_PARTITIONS_PER_UNIT`, `COVERAGE_MAX_TOTAL_GT`,
  `COVERAGE_MAX_TOTAL_PREDICTIONS`, `COVERAGE_MAX_TOTAL_EDGES`,
  `SPEARMAN_MAX_PARTITIONS`, `SPEARMAN_MAX_TOTAL_GT`,
  `SPEARMAN_MAX_TOTAL_PREDICTIONS`, `SPEARMAN_MAX_TOTAL_EDGES`,
  `SPEARMAN_MAX_SUPPORT`).

  Due ragioni indipendenti, entrambe sufficienti:

  1. **Nessuna autorità normativa.** Nessuno di questi 16 numeri compare nel
     draft firmato, nel candidate contract, o in alcun ballot. Esistono solo
     come costanti Python. Un ballot che li "attivasse" li starebbe
     **introducendo**, non confermando — atto normativo di natura diversa da
     quello qui richiesto.
  2. **Condizione preesistente non soddisfatta.** Il report per-sottografo §7,
     scritto dalle tre lenti originali, elenca fra i requisiti pre-attivazione:
     «group-level conservative scalar/work bounds, under/on/over fixtures,
     reason codes, and fatal blast radius». Il workload `cap_boundaries` prova
     al bordo **4 superfici** (`AP_PARTITIONS`, `COVERAGE_UNITS`,
     `SPEARMAN_PARTITIONS`, `EXACT_SCALAR_BITS`), non tutte e 16.

- Il caso **Spearman general variable-value-marginal**, che resta
  `NOT CERTIFIED` (limitazione
  `GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED` invariata). Il
  micro-amend S6 ne ha nominato l'esito, non chiuso la scienza.

- `G_eligible` / `G_defined` / `G_NA` e i **gate floor**, che il ledger assegna
  a **O-13** («macro mean/p95 — support/floor fixtures»), non a O-09. Restano
  `NOT_EVALUATED`. Il p95 Type-7 normativo di §12 non è implementato.

## 4. Fixture — evidenza di bordo per ciascun ceiling

Un ceiling attivato deve avere prova under/on/over. Stato per ognuno:

| Ceiling | Under | On | Over | Fonte |
|---|---|---|---|---|
| GT ≤ 128 | 127 → nessun exceed | 128 → nessun exceed | 129 → `('GT',)` | probe diretto |
| prediction ≤ 128 | 127 → nessun exceed | 128 → nessun exceed | 129 → `('PREDICTION',)` | probe diretto |
| eligible edges ≤ 16384 | 16.383 → nessun exceed | 16.384 → nessun exceed | 16.385 → `('GT','ELIGIBLE_EDGES')` | probe diretto — vedi §2.1: l'over puro è irraggiungibile per costruzione |
| bit-length ≤ 65536 | 65.535 accettato | 65.536 accettato | 65.537 respinto | report per-sottografo §4, tre lenti |

I tre probe diretti sono stati eseguiti in questa sessione sull'interprete
canonico (`/Users/marco/aieq_data/motore_v3/env/venv/bin/python`, CPython
3.12.13) contro `provisional_preflight_probe`, senza modificare alcun file.
**Non sono stati committati come test.** Se l'autorità li ritiene necessari in
forma permanente, vanno materializzati come fixture prima della firma — vedi §8.

## 5. Reason code

Invariati, già in §10:

```text
SOLVER_STRUCTURAL_LIMIT_EXCEEDED
SOLVER_RUNTIME_FAILURE
SOLVER_CONSTRAINT_MODEL_INVALID
```

Il superamento di un ceiling attivato produce
`SOLVER_STRUCTURAL_LIMIT_EXCEEDED`. Ogni failure fatale in un sottografo che
contribuisce a una metrica di gate impedisce PASS. Sono ammessi report
diagnostici parziali, mai un PASS scientifico parziale.

## 6. Blast radius — verificato, non stimato

**Sul runtime live: nullo.** `ml_v3/contracts/__init__.py` — il dispatcher
REV7 attivo — non contiene alcun riferimento a REV8 (grep vuoto). I kernel
O-09 non sono raggiungibili dal path live. L'attivazione non cambia il
comportamento di nulla che giri oggi in produzione.

**Sul candidate:** i quattro ceiling passano da diagnostici a vincolanti. Un
sottografo che li supera diventa un fallimento fatale invece di un'annotazione.

**Costo operativo al soffitto** — è il dato che l'autorità sta accettando
firmando, e va guardato prima di firmare. Caso peggiore misurato
(per-sottografo, tre lenti):

| Workload | Wall max | Peak RSS |
|---|---:|---:|
| bit-stress degenerate | **187.65 s** | **915.898.368 B (~916 MB)** |

Per **un singolo sottografo** al soffitto. Una valutazione reale ne contiene
molti. Firmare significa dichiarare accettabile questo profilo di costo, o
accettare che valutazioni con molti sottografi vicini al ceiling siano
impraticabili nei tempi. **Il benchmark dimostra la fattibilità del cap, non
la sua ottimalità**: nessuna evidenza raccolta dice che 128 sia il numero
*giusto*, solo che il solver lo regge.

## 7. Cosa questo ballot NON fa

- Non attiva i 16 cap group-level (§3).
- Non chiude il caso Spearman variable-value-marginal.
- Non valuta `G_eligible`/`G_defined`/`G_NA` né i gate floor (competenza O-13).
- **Non costituisce `REV8 SPEC GO`.** O-09 è l'unica riga del ledger con
  condizione di attivazione esplicita, ma le altre 28 righe hanno ciascuna la
  propria condizione di materializzazione, e questo ballot non ne verifica
  nessuna. Nessuno ha controllato riga per riga quali siano effettivamente
  soddisfatte — è un lavoro separato e non fatto.
- Non autorizza codice, runtime, training, o modifiche a REV7.

## 8. Deroga di processo — dichiarata

**Questo ballot è preparato da un revisore singolo.** Un workflow di verifica
indipendente a 10 agenti (4 verificatori avversariali, 3 proposte a lenti
diverse, 3 giudici) è stato lanciato e **è fallito interamente** — 0/10 agenti
completati, limite settimanale dell'account raggiunto. Nessun contributo
indipendente è stato incorporato.

Stato reale della verifica al momento della preparazione:

```text
Evidenza per-sottografo         = tre lenti indipendenti (storica, commit 030ec8bb)
Evidenza group-level            = revisore singolo + Hermes sugli hash
Probe di bordo GT/PR/archi      = revisore singolo, esecuzione diretta, non committati
Scoperta cap-archi-derivato     = revisore singolo, verificata per esecuzione
Scope e raccomandazione A1      = revisore singolo
Counter-check a tre lenti       = NON ESEGUITO
```

Sarebbe la **seconda deroga consecutiva** dopo quella del micro-amend S6. Due
di fila stabiliscono un precedente: se l'autorità firma comunque, è una scelta
consapevole di cambiare lo standard del programma, non una svista.

**Opzione a costo zero:** il limite si resetta il 2 agosto. Attendere consente
di far girare il workflow indipendente prima della firma, senza perdere nulla
di quanto già fatto.

## 9. Firma dell'autorità

```text
Decisione complessiva: [ DA COMPILARE — APPROVO / RESPINGO / APPROVO CON MODIFICHE ]
Firma/nome:            [ DA COMPILARE ]
Data:                  [ DA COMPILARE ]

Deroga §8 accettata consapevolmente: [ SI / NO ]
```

## 10. Stato dopo la firma (se APPROVO)

```text
O-09 quattro ceiling §10        = ATTIVI
Cap group-level (16)            = NON ATTIVI, fuori scope
Spearman variable-marginal      = NON CERTIFICATO (invariato)
G_eligible/G_defined/G_NA       = NOT_EVALUATED (O-13, invariato)
Patch document-only §10         = AUTORIZZATA, NON ANCORA APPLICATA
Counter-check nuovo SHA         = OBBLIGATORIO
REV8 SPEC GO                    = NO (altre 28 righe di ledger non verificate)
Runtime/training                = NON AUTORIZZATI
```
