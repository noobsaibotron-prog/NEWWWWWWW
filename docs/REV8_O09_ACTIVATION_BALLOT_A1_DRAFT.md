# REV8 — O-09 — ACTIVATION BALLOT ADDENDUM (A1, NARROW)

**Stato:** `DRAFT — S6 TRANSFER EFFECTIVE — PENDING POST-TRANSFER RECHECK — NOT YET SIGNABLE`

Questo documento è **preparato e non firmato**. La policy S6 è ora efficace
nella composizione R23C, ma il retarget A1 deve ricevere un nuovo counter-check
indipendente sul proprio SHA prima della firma. Nulla diventa attivo con la
sola firma: servono anche un recheck post-firma `CLEAN` e il relativo report
immutabile. Fino ad allora `REV8 SPEC GO = NO` e i cap restano
`PROVISIONAL_PENDING_BENCHMARK_O09`.

## 1. Oggetto e autorità invocata

Il ledger §15 del draft normativo firmato assegna a O-09 questa condizione di
materializzazione, unica fra tutte le righe del ledger:

```text
| O-09 | fail policy | FIRMATA; R23A_05 APPROVATA; NUMERI PROVVISORI | bit-length/preflight definition + benchmark + ballot addendum di attivazione |
```

§10 dichiara i quattro ceiling `PROVISIONAL_PENDING_BENCHMARK_O09` e stabilisce
che «non producono PASS/FAIL finché un ballot addendum, dopo benchmark A2 sul
platform lock, non li attiva».

Questo è quel ballot addendum, **ristretto ai soli quattro ceiling di §10**.

```text
Target normativo corrente:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md §10
SHA-256 684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

Autorità composta R23C:
ballot firmato SHA-256
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99
report post-firma CLEAN SHA-256
e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157

S6 transfer firmato:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_BALLOT_SIGNED.md
SHA-256: 8dab8b7ad0fb25f5263358dd26600d201a036210d808840d6c8e1d07051649c0
Commit: 7fe229d0cd1f6e5af629a00cfa5ed79fb3b7b7b1

S6 transfer post-signature recheck:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_POST_SIGNATURE_RECHECK_REPORT.md
SHA-256: b148e6340dd3c9b46ca0f6787209cc6d2df00a6512e34c5bce164b81b1349c7a
Commit: 88315f0f82a96a8906fe6a382c8f679ab4f73304
Verdetto: 3/3 CLEAN — TRANSFER EFFICACE

Evidenza per-sottografo (counter-check a tre lenti, commit 030ec8bb):
docs/REV8_O09_PER_SUBGRAPH_BENCHMARK_REPORT.md
SHA-256: 0789e9070fa959e8cd15f18961da7b053562245c5762c953fd6adc92a563977f

Evidenza group-level (revisore singolo + Hermes sugli hash):
docs/REV8_O09_GROUP_BENCHMARK_REPORT.md
SHA-256: 508906fdc7fc69f7e8411255f1fed74e1e61055eada11d5e0b5fbcc7f4fbac1a

Implementazione:
ml_v3/benchmark/rev8_o09_candidate.py
SHA-256: bb130138423bbf7bc0d6fe3ef3d2a3d6b27464244daa893b039ab1744860ec9a

Test permanenti A1 e counter-check storico S6:
Commit evidence freeze: 9a16692bfee32396213f7487a80ef94f99f50076
ml_v3/tests/test_g1c_rev8_o09_candidate.py
SHA-256: 4d43c15d20a169786d78f6d51057432989b22adbd5468df0662f9ad028259970
docs/REV8_S6_TARGETED_COUNTERCHECK_REPORT.md
SHA-256: 88d24d1522e3bfb0a012b19b1b8edf2ff4087ff9e3534c82aa21aa946acd2570
```

Il precedente target S6 `NORMATIVE_SIGNED_DRAFT` SHA `be865...` è evidenza
storica, non l'artefatto normativo corrente. Il transfer esterno firmato e il
suo recheck CLEAN hanno chiuso quel blocker senza riscrivere il target R23C.

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

### 2.2 Formula conservativa di preflight attivata atomicamente

`R23A_05` richiede che il ballot congeli non soltanto la soglia, ma anche la
formula conservativa che domina ogni scalare normativo per-sottografo. Le
definizioni seguenti sono parte atomica della decisione A1.

Siano:

```text
G = numero GT
P = numero prediction
E = insieme degli archi eleggibili
K = max(1, min(G,P))
L(Q) = min(K, cardinalità(Q))
ceil_log2(n) = 0 se n<=1, altrimenti ceil(log2(n))
A(q) = int_bit_length(abs(numeratore(q)))
D(q) = int_bit_length(denominatore(q))
```

Ogni razionale è canonico ridotto con denominatore positivo. Per una sequenza
`Q` di razionali non negativi, con `L=L(Q)`:

```text
sum_bound([], divide) = 2

top_D = somma dei L maggiori D(q) in Q
other_D(i) = somma dei min(L-1, |Q|-1) maggiori D(q_j), j != i
largest_term = max(1, max_i(A(q_i) + other_D(i)))

sum_bound(Q, false) = top_D + largest_term + ceil_log2(L)
sum_bound(Q, true)  = sum_bound(Q, false)
                      + (ceil_log2(L) se L>1, altrimenti 0)
```

Per il prodotto razionale:

```text
product_bound([]) = 2
product_bound(Q)  = somma dei L maggiori A(q)
                    + somma dei L maggiori D(q)
```

Per una sequenza `Z` di interi non negativi:

```text
integer_sum_bound([], divide) = 1
base(Z) = max_z int_bit_length(z) + ceil_log2(L(Z))
integer_sum_bound(Z, false) = base(Z)
integer_sum_bound(Z, true)  = base(Z) + int_bit_length(L(Z) * 48)
```

La pubblicazione esatta tick→ms usa `sum/(K1*48)`, dove `K1` è la
cardinalità effettiva del matching. Nel bound conservativo si usa
`L(Z)*48`: poiché `K1<=L(Z)`, la sua bit-length domina quella del denominatore
pubblicabile prima di qualsiasi riduzione. Il bound per-sottografo è:

```text
B = max(
  sum_bound(all k2_iou, false),
  product_bound(all k4_cost)               se K4 è product,
  sum_bound(all k4_cost, false)             se K4 è additive,
  sum_bound(all severity_error, true),
  integer_sum_bound(all k3_tick_error, false),
  integer_sum_bound(all onset_error_ticks, true),
  integer_sum_bound(all offset_error_ticks, true),
  max rational_bit_length(q) per ogni input k2/k4/severity,
      con default 2
)
```

Il preflight normativo rifiuta se una qualunque condizione è vera:

```text
G > 128
P > 128
|E| > 16384
B > 65536
```

Lo scope comprende componenti esatte di `V`, accumulatori esatti degli
envelope severity/onset/offset e scalari pubblicati tick→ms. I temporanei
specifici dell'implementazione sono esclusi; OOM ed eccezioni restano failure
operativi. Un'implementazione può usare una formula interna differente soltanto
se dimostra equivalenza osservabile con la formula normativa A1.

## 3. Scope — cosa è attivato e cosa esplicitamente no

**ATTIVATO NORMATIVAMENTE DOPO LA FIRMA:** i quattro ceiling di §10, per
sottografo `(evaluation_unit_key, record_family, problem_type)`. Un evaluator
REV8 conforme DOVRÀ valutarli nel preflight prima del calcolo parziale.

Questa attivazione è una decisione dell'autorità, non la materializzazione del
reject-path. Il kernel evidence-only citato nel ballot continua intenzionalmente
a riportare `PROVISIONAL_DIAGNOSTIC_ONLY`; `a2_exact()` non consuma
`provisional_exceeded` e non emette status/reason. Fino a una tranche separata
che implementi e testi l'enforcement, nessun evaluator candidate può dichiararsi
conforme a questa decisione o produrre un PASS REV8.

**NON ATTIVATO — fuori scope dichiarato:**

- I **17 cap group-level** definiti in `ml_v3/benchmark/rev8_o09_group_candidate.py`
  (`AP_MAX_PARTITIONS`, `AP_MAX_TOTAL_GT`, `AP_MAX_TOTAL_PREDICTIONS`,
  `AP_MAX_TOTAL_EDGES`, `AP_MAX_DISTINCT_THRESHOLDS`, `AP_MAX_LOCAL_THRESHOLDS`,
  `AP_MAX_PREFIX_EDGE_INCIDENCE`, `COVERAGE_MAX_UNITS`,
  `COVERAGE_MAX_PARTITIONS_PER_UNIT`, `COVERAGE_MAX_TOTAL_GT`,
  `COVERAGE_MAX_TOTAL_PREDICTIONS`, `COVERAGE_MAX_TOTAL_EDGES`,
  `SPEARMAN_MAX_PARTITIONS`, `SPEARMAN_MAX_TOTAL_GT`,
  `SPEARMAN_MAX_TOTAL_PREDICTIONS`, `SPEARMAN_MAX_TOTAL_EDGES`,
  `SPEARMAN_MAX_SUPPORT`).

  Due ragioni indipendenti, entrambe sufficienti:

  1. **Nessuna autorità normativa.** Nessuno dei 17 identificatori o cap
     semantici group-level compare nel draft firmato, nel candidate contract,
     o in alcun ballot. Alcuni valori numerici (`128`, `16384`) ricorrono nel
     draft con significato per-sottografo, ma non conferiscono autorità ai cap
     group-level omonimi. Questi ultimi esistono solo come costanti Python. Un
     ballot che li "attivasse" li starebbe **introducendo**, non confermando —
     atto normativo di natura diversa da quello qui richiesto.
  2. **Condizione preesistente non soddisfatta.** Il report per-sottografo §7,
     scritto dalle tre lenti originali, elenca fra i requisiti pre-attivazione:
     «group-level conservative scalar/work bounds, under/on/over fixtures,
     reason codes, and fatal blast radius». Il workload `cap_boundaries` prova
     al bordo **4 superfici** (`AP_PARTITIONS`, `COVERAGE_UNITS`,
     `SPEARMAN_PARTITIONS`, `EXACT_SCALAR_BITS`), non tutte e 17.

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
| GT ≤ 128 | 127 → nessun exceed | 128 → nessun exceed | 129 → `('GT',)` | test permanente `test_shape_ceiling_boundaries_are_permanently_materialized` |
| prediction ≤ 128 | 127 → nessun exceed | 128 → nessun exceed | 129 → `('PREDICTION',)` | stesso test permanente |
| eligible edges ≤ 16384 | 16.383 → nessun exceed | 16.384 → nessun exceed | 16.385 → `('GT','ELIGIBLE_EDGES')` | stesso test permanente — vedi §2.1: l'over puro è irraggiungibile per costruzione |
| bit-length ≤ 65536 | 65.535 accettato | 65.536 accettato | 65.537 respinto | test permanente `test_exact_scalar_boundary_65535_65536_65537`; report per-sottografo §4 |

I probe sono materializzati in
`ml_v3/tests/test_g1c_rev8_o09_candidate.py`: il test
`test_shape_ceiling_boundaries_are_permanently_materialized` fissa
GT/prediction/archi under-on-over, mentre
`test_exact_scalar_boundary_65535_65536_65537` fissa il ceiling matematico.
Devono passare sull'interprete canonico prima della firma e dopo ogni patch al
testo di attivazione.

## 5. Reason code

Invariati, già in §10:

```text
SOLVER_STRUCTURAL_LIMIT_EXCEEDED
SOLVER_RUNTIME_FAILURE
SOLVER_CONSTRAINT_MODEL_INVALID
```

Normativamente, il superamento di un ceiling attivato DEVE produrre
`SOLVER_STRUCTURAL_LIMIT_EXCEEDED`. Ogni failure fatale in un sottografo che
contribuisce a una metrica di gate impedisce PASS. Sono ammessi report
diagnostici parziali, mai un PASS scientifico parziale.

**Stato implementativo corrente:** il probe evidence-only dimostra soltanto
le etichette `provisional_exceeded`; non materializza status, reason code o
blast radius. La futura tranche di enforcement deve aggiungere un wrapper
pre-solve fail-closed e reject-path test che dimostrino tutti e tre. Questo
ballot non autorizza quella modifica di codice.

## 6. Blast radius — verificato, non stimato

**Sul runtime live: nullo.** `ml_v3/contracts/__init__.py` — il dispatcher
REV7 attivo — non contiene alcun riferimento a REV8 (grep vuoto). I kernel
O-09 non sono raggiungibili dal path live. L'attivazione non cambia il
comportamento di nulla che giri oggi in produzione.

**Sul candidate corrente: nullo.** I quattro ceiling restano diagnostici nel
codice evidence-only; `a2_exact()` continua a ignorare
`provisional_exceeded`. La firma rende i ceiling vincolanti per la futura
implementazione REV8, non modifica retroattivamente il comportamento del
kernel citato. Dopo la tranche di enforcement, un sottografo che li supera
dovrà diventare un fallimento fatale invece di un'annotazione.

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

- Non attiva i 17 cap group-level (§3).
- Non chiude il caso Spearman variable-value-marginal.
- Non valuta `G_eligible`/`G_defined`/`G_NA` né i gate floor (competenza O-13).
- **Non costituisce `REV8 SPEC GO`.** O-09 è l'unica riga del ledger con
  condizione di attivazione esplicita qui invocata. Le restanti
  materializzazioni e decisioni del ledger non sono state verificate da A1;
  il loro conteggio non è usato come autorità. Il controllo riga per riga è un
  lavoro separato e non fatto.
- Non autorizza codice, runtime, training, o modifiche a REV7.
- Non dichiara materializzato l'enforcement candidate: wrapper, reason code e
  fatal blast radius richiedono una tranche separata e un nuovo counter-check.

## 8. Counter-check indipendente a tre lenti

Il primo tentativo storico di workflow a dieci agenti non aveva prodotto
evidenza utilizzabile. Prima di questo freeze è stato però completato un nuovo
counter-check indipendente e mirato con tre lenti distinte. I verdetti dello
snapshot pre-transfer erano:

```text
Metriche / fail-closed = CLEAN contenutistico; BLOCK operativo finché S6 non è trasferita
Optimizer / aritmetica = CLEAN; formula §2.2 equivalente al probe
Semantica / authority  = CLEAN dopo rimozione del conteggio ledger stale;
                         BLOCK operativo finché S6 non è trasferita
```

Il report pre-transfer è materializzato in
`docs/REV8_O09_A1_THREE_LENS_COUNTERCHECK_REPORT.md` (SHA-256
`57258ee4e9b2aeaf92484107c2a9d8b47cb43fdd97b0631726070e016565ee1d`).
Verificava A1 SHA `ffe2eca9...` e non può essere riutilizzato come verifica di
questo retarget. Le tre lenti non avevano modificato i file. Erano inoltre
passati 22 test mirati (8 A1 + 14 S6) e 540/540 test nella suite canonica
CPython 3.12.13.

Il counter-check sul patch S6 storico resta separato in
`docs/REV8_S6_TARGETED_COUNTERCHECK_REPORT.md`: è `CLEAN` soltanto sui byte
del predecessore. Il `BLOCK` R23C registrato in quel report era corretto per
quello snapshot ed è stato successivamente chiuso dagli artefatti S6 transfer
firmati e verificati elencati nella §1.

Un nuovo counter-check sullo SHA retargetizzato è obbligatorio prima della
firma. Non è ammessa alcuna deroga di processo: finché il nuovo report non è
`CLEAN`, lo stato resta `NOT YET SIGNABLE`.

### 8.1 Deroga tecnica di scope rispetto al report per-sottografo

Il report per-sottografo §7 stabiliva che, prima di qualsiasi «O-09
activation», la tranche group-level dovesse includere anche bounds conservativi,
fixture under/on/over, reason code e fatal blast radius group-level, seguiti da
counter-check indipendente. Quella condizione **non è interamente soddisfatta**:
il workload group-level prova quattro superfici su diciassette e i diciassette
cap non hanno autorità normativa.

A1 propone quindi una deroga tecnica esplicita e limitata a quella condizione:
confermare normativamente i soli quattro ceiling per-sottografo già firmati in
§10, senza dichiarare completa o attiva O-09 nel suo insieme e senza attivare
alcun cap group-level. Non è una conseguenza automatica del benchmark; è una
decisione di scope dell'autorità che deve essere accettata separatamente.

Se questa deroga vale `NO`, i quattro ceiling restano provvisori e nessuna
attivazione avviene. L'alternativa senza deroga è completare la condizione 4
del report per-sottografo e sottoporla a nuovo ballot/counter-check.

## 9. Firma dell'autorità

```text
Decisione complessiva: [ DA COMPILARE — APPROVO / RESPINGO / APPROVO CON MODIFICHE ]
Firma/nome:            [ DA COMPILARE ]
Data:                  [ DA COMPILARE ]

Deroga tecnica di scope §8.1 accettata consapevolmente: [ SI / NO ]
```

Transizione di stato vincolante:

- `APPROVO` firma la decisione soltanto se il transfer S6 è già efficace nella
  composizione R23C e la deroga tecnica §8.1 vale `SI`; la firma resta
  **non efficace** finché il recheck post-firma non è `CLEAN`;
- soltanto il freeze del report post-firma `CLEAN` rende attivi i quattro
  ceiling nella composizione esterna;
- transfer S6 non efficace oppure deroga tecnica `NO` lasciano i ceiling
  provvisori e inattivi;
- `RESPINGO` lascia i ceiling provvisori e inattivi;
- `APPROVO CON MODIFICHE` non attiva nulla: richiede patch, verifica su commit
  immutabile e nuova firma finale.

## 10. Stato dopo la firma, prima del recheck (se APPROVO)

```text
O-09 quattro ceiling §10        = FIRMATI, NON EFFICACI PENDING RECHECK
Cap group-level (17)            = NON ATTIVI, fuori scope
Spearman variable-marginal      = NON CERTIFICATO (invariato)
G_eligible/G_defined/G_NA       = NOT_EVALUATED (O-13, invariato)
Target R23C                     = IMMUTATO; NESSUNA PATCH IN-PLACE
Enforcement candidate           = NON MATERIALIZZATO; NESSUN PASS CONFORME
Counter-check nuovo SHA         = OBBLIGATORIO
REV8 SPEC GO                    = NO (restante ledger non verificato da A1)
Runtime/training                = NON AUTORIZZATI
```

### 10.1 Stato soltanto dopo recheck post-firma CLEAN

```text
authority applicabile           = R23C + S6 transfer + A1 firmato + report CLEAN
O-09 quattro ceiling §10        = ATTIVI NORMATIVAMENTE
Target R23C                     = IMMUTATO
Cap group-level (17)            = NON ATTIVI
Enforcement candidate           = NON MATERIALIZZATO; NESSUN PASS CONFORME
REV8 SPEC GO                    = NO
Runtime/training                = NON AUTORIZZATI
```
