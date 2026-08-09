# REV8 — O-13 — METRIC/STRATUM REGISTRY ARCHITECTURE BALLOT (O13F_03)

## Stato del documento

```text
Tipo                            = micro-ballot normativo docs-only pre-firma
Data preparazione               = 2026-08-09
Branch                          = feature/motore-v3-rev8-spec-go
Base commit                     = 468f7e3212d386c916642f7a292d63af7e1d8ad0
Amendment base                  = 3c2bf9aad616f1e5b8959509cac91aed3545367c
Pre-sign freeze superseded      = 6ce33f9a30af23fd7c6b08ad5bcf7a61ac5d113d
Counter-check superseded        = REV8_O13_METRIC_STRATUM_REGISTRY_BALLOT_COUNTERCHECK.md
Decisione O13F_03               = APPROVO — PENDING POST-SIGNATURE RECHECK
Registry data artifact          = NON MATERIALIZZATO
Official support-floor policies = NON MATERIALIZZATE
REV8 dispatcher activation      = NO
REV8 SPEC GO                    = NO
```

Questo ballot non inventa metric ID, non materializza popolazioni, non usa il
power plan di esempio e non attiva alcuna policy. Decide soltanto
l'architettura, lo scope e le guardie necessarie per poter congelare, in una
tranche successiva, il catalogo canonico delle metriche e il registry
`metric_id × split_role × stratum_id`.

## 1. Catena di autorità consumata

```text
Candidate REV8:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_01 — support-floor binding:
docs/REV8_O13_SUPPORT_FLOOR_BINDING_BALLOT_DRAFT.md
Commit firma:
59bec34856a08aa43cc52bb123e904a44575f4f6

O13F_01 post-signature recheck:
docs/REV8_O13_SUPPORT_FLOOR_POST_SIGNATURE_RECHECK_REPORT.md
Commit:
d0b9916cd08aff29dfde03ffe0c8f38c3656a96c

O13F_02 — support basis:
docs/REV8_O13_SUPPORT_BASIS_BALLOT_DRAFT.md
Commit firma:
26f35e753f96ebc48d0453e432e8f2a3f5380687

O13F_02 post-signature recheck:
docs/REV8_O13_SUPPORT_BASIS_POST_SIGNATURE_RECHECK_REPORT.md
Commit:
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e

O13F_02 candidate implementation:
commit:
2b57493917ead1074de8b90da5907790564cc34d

Implementation report:
docs/REV8_O13_SUPPORT_BASIS_IMPLEMENTATION_REPORT.md
Commit:
468f7e3212d386c916642f7a292d63af7e1d8ad0
```

O13F_01 e O13F_02 restano authority per floor, popolazioni e
`support_basis`. O13F_03 non modifica quelle decisioni.

## 2. Finding verificato

Il candidate definisce in prosa:

- metriche tonali obbligatorie;
- metriche evento obbligatorie;
- metriche di calibrazione;
- floor per calibration, development-metric e final-test;
- strata di profilo, source family, regione/direzione e clean safety;
- `n_required = max(contract_floor,n_power)`;
- regole di supporto N/A e fail-closed.

Non esiste ancora un artefatto autoritativo che congeli insieme:

```text
metric_id canonico
split_role
status normativa/diagnostica
mandatory status
template di supporto
strata completi
support_basis per strato
floor contrattuale
power binding
parent/ceiling
```

Le stringhe oggi osservabili nel repository, fra cui:

```text
curve_err_rel
clean_actionable_rate
average_precision:Resonance
clean_safety_pool
```

provengono da fixture o test candidate. Non costituiscono un namespace
scientifico congelato.

In particolare,
`ml_v3/fixtures/g1/examples/benchmark_power_plan.json` dichiara
`support.notes="fixture"` e un `pilot_sha256` fittizio. I suoi ID e i suoi
`n_power` non possono essere promossi a authority O-13.

### 2.1 Conseguenza

Materializzare oggi un registry completo richiederebbe all'implementatore di
scegliere almeno:

1. quali output obbligatori hanno una policy autonoma;
2. quali output condividono lo stesso template di supporto;
3. se diagnostici e intermedi hanno un `metric_id` normativo;
4. la spelling esatta degli ID;
5. quali split richiedono ciascuna policy;
6. quali righe sono power-governed e con quale binding.

Sono decisioni byte-level e scientifiche: non possono essere inferite dai test
esistenti né decise silenziosamente dal builder.

## 3. Decisione proposta O13F_03-A — separazione catalogo/registry/policy

Si adottano tre livelli distinti.

### 3.1 Metric catalog

Il **metric catalog** definisce l'identità e lo status di ogni output
scientifico o diagnostico. Non contiene popolazioni osservate né `n_power`.

Ogni entry deve avere exact-key schema:

```text
metric_id
metric_family
definition_key
evaluation_scope_id
record_family
problem_type
publication_status
support_mode
allowed_split_roles
```

Vincoli:

1. `metric_id` è unico e non candidate-controlled;
2. `publication_status` appartiene a:

   ```text
   mandatory_normative
   optional_normative
   diagnostic_only
   ```

3. `definition_key` identifica formula e semantica N/A definite dal candidate;
4. `evaluation_scope_id` identifica il pool globale, la family benchmark o il
   subgroup claim preregistrato;
5. due `metric_id` non possono avere la stessa tupla
   `(definition_key,evaluation_scope_id,record_family,problem_type)`;
6. `support_mode` appartiene a:

   ```text
   own_policy
   none_diagnostic
   ```

7. `own_policy` richiede un binding per ogni split ammesso;
8. `none_diagnostic` richiede `publication_status=diagnostic_only` e nessun
   binding; non può soddisfare gate o claim, ma conserva il proprio
   `SupportAccounting`, pubblica supporto/N/A e non prende in prestito
   `G_defined` da un'altra metrica;
9. ogni output `mandatory_normative` o `optional_normative` usa `own_policy`;
10. più metriche possono condividere lo stesso template soltanto mantenendo
   policy, `G_defined`, esiti N/A e binding distinti per `metric_id`;
11. non esiste support inheritance fra metriche: stessa partizione o stessa
   formula di aggregazione non dimostrano identica definibilità;
12. `record_family=null` significa che la metrica non è definita su
   `semantic_region` o `dynamic_event`; non implica scope globale.
   `problem_type=null` significa che la metrica non è class-specific. Lo scope
   scientifico resta sempre determinato separatamente da
   `evaluation_scope_id`;
13. `allowed_split_roles` è una lista non vuota, senza duplicati, ordinata per
   byte UTF-8 e composta soltanto da ruoli canonici;
14. `mandatory_normative` richiede `mandatory=true` in ogni binding;
15. `optional_normative` richiede `mandatory=false` in ogni binding;
16. `diagnostic_only` richiede `support_mode=none_diagnostic`, zero binding e
    non può soddisfare un gate o una claim.

`metric_family` appartiene esclusivamente a:

```text
tonal_curve
tonal_semantic
matching_event
calibration
homologous_adapter
```

`record_family` appartiene a:

```text
null
semantic_region
dynamic_event
```

`problem_type` appartiene a `null` oppure agli otto slug della sezione 5.
`definition_key` usa la stessa grammatica di `metric_id`, è enumerata nel
catalogo e non è testo descrittivo libero.

`evaluation_scope_id` usa la grammatica di `support_template_id`. Sono
predefiniti:

```text
global
benchmark.tonal_controlled
benchmark.tonal_natural
benchmark.anomaly_natural
benchmark.clean_safety
benchmark.electronic_stratified
```

Un subgroup scope ulteriore è ammesso soltanto se enumerato da un claim plan
preregistrato e hash-bound prima delle prediction; la grammatica da sola non
lo autorizza.

### 3.2 Metric/stratum registry

Il **metric/stratum registry** lega ciascuna policy richiesta a uno split e a
un template di strata. Non contiene membership di gruppi osservati.

Ogni binding ha exact-key schema:

```text
metric_id
split_role
mandatory
support_template_id
precondition_ids
```

`precondition_ids` è una lista ordinata, senza duplicati, di precondizioni
registry-only applicabili a quel binding. La lista può essere vuota; un ID
presente deve risolversi esattamente nello stesso split e scope della metrica.

Ogni template ha exact-key schema:

```text
support_template_id
strata[]:
  stratum_id
  population_kind
  population_selector
  parent_stratum_id
  support_basis
  contract_floor
  power_binding_kind
  power_binding_id
  max_parent_fraction_numerator
  max_parent_fraction_denominator
```

`population_kind` appartiene esclusivamente all'enumerazione già
materializzata dalla policy v3:

```text
all_eligible_groups
gt_positive_groups
gt_negative_groups
clean_groups
clean_standard_minute_groups
profile_groups
source_family_groups
tonal_region_direction_groups
paired_groups
```

`support_template_id` e `stratum_id` rispettano:

```text
^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)*$
```

La grammatica non autorizza valori non enumerati nel registry firmato.

`population_selector` è un oggetto registry-only, exact-key e discriminato da
`population_kind`. Conserva i valori semantici reali necessari a derivare la
membership, per esempio:

```text
profile_groups                  -> profile canonico
source_family_groups            -> source_family UTF-8 esatta
tonal_region_direction_groups   -> region index + direction
gt_positive/negative_groups     -> problem type/scope canonico
clean_standard_minute_groups    -> anomaly class canonica
paired_groups                   -> adapter class/scope canonico
```

Per `all_eligible_groups` il selector è l'oggetto vuoto canonico. I selector
exact-key completi per ciascun `population_kind` devono essere congelati in un
companion schema nello stesso commit del primo data artifact. Il registry e i
companion schema di selector e precondizioni sono una sola unità di firma;
stringhe descrittive o parsing dello `stratum_id` sono vietati. Il
`stratum_id` è identità, non una seconda fonte semantica.

`population_selector` non entra nella policy v3. Il validator registry deriva
la population dagli input hash-bound, verifica il population plan e poi
proietta nella policy soltanto i campi già ammessi da
`support-floor-policy-3`.

Firma e recheck del solo O13F_03 non autorizzano un validator che inventi i
selector: l'implementazione ufficiale del registry resta bloccata fino al
freeze congiunto di companion schema, catalogo e data artifact.

### 3.2.1 Precondizioni relazionali registry-only

Alcuni vincoli di corpus già congelati dal candidate non sono esprimibili come
un floor indipendente di ogni strato della policy v3. In particolare:

```text
anomaly positivo:
  almeno 3 source_family con almeno 5 group_id ciascuna

electronic-stratified:
  almeno 2/5 del parent final-test applicabile
```

Applicare `contract_floor=5` a **ogni** source family osservata sarebbe più
restrittivo del candidate; applicarlo soltanto a tre family scelte liberamente
sarebbe candidate-controlled. Analogamente, un minimo frazionario del parent
non è rappresentabile da `max_parent_fraction_*`.

Il registry contiene quindi una lista `registry_preconditions` di predicati
registry-only, ciascuno con ID canonico e schema exact-key discriminato. I soli
`predicate_kind` ammessi in questa revisione sono:

```text
minimum_qualifying_partitions
minimum_parent_fraction
```

Il companion schema congela per ciascun kind:

- selector parent e child;
- chiave canonica di partizionamento;
- conteggio minimo per child e numero minimo di child qualificati, quando
  `minimum_qualifying_partitions`;
- numeratore e denominatore ridotti, quando `minimum_parent_fraction`;
- `evaluation_scope_id`, split e metric binding cui il predicato si applica;
- reason code e regola exact-integer del confronto.

I valori concreti derivano soltanto dai requisiti numerici già presenti nel
candidate e sono firmati nel primo data artifact; non possono essere scelti
dai conteggi osservati per favorire un PASS. La membership è derivata dagli
stessi input hash-bound di §6.2 prima delle prediction.

Le righe `source_family_groups` restano complete per tutte le family osservate
e applicano il ceiling firmato a ciascuna. La precondizione separata verifica
il numero di family che raggiungono il minimo, senza trasformare quel minimo in
un obbligo per ogni family presente.

Queste precondizioni non entrano in `support-floor-policy-3`: sono validate dal
path registry ufficiale e il loro esito è legato al futuro report/activation
envelope insieme a `registry_sha256` e `policy_sha256`. Precondizione mancante,
extra, non risolta, fallita o incoerente con split/scope produce N/A/no PASS per
ogni binding che la referenzia; schema o membership incoerenti producono FAIL
di materializzazione.

Il registry è una allowlist globale. Una policy contenente metriche, split,
template, strata o basis non presenti esattamente nel registry deve fallire.

### 3.3 Compiled support-floor policy

La policy compilata resta l'artefatto per una specifica coppia
`(metric_id,split_role)`. È ottenuta soltanto da:

```text
catalogo metriche firmato
registry firmato
population plan pre-prediction firmato
power plan reale firmato, quando richiesto
```

La policy non può ridefinire il catalogo o il registry.

### 3.4 Ragione della separazione

La separazione impedisce due errori opposti:

- duplicare lo stesso template in molte righe e lasciarlo divergere;
- usare un generico `support_subject_id` al posto del vero `metric_id`,
  rendendo ambiguo quale output è protetto.

Più metriche possono riferire lo stesso `support_template_id` e la stessa
metrica può usare template diversi in split diversi, ma ogni metrica normativa
conserva un `metric_id` distinto e un binding esplicito per split. Il catalogo
non contiene un `support_template_id`: l'autorità del template appartiene
esclusivamente al binding `(metric_id,split_role)`.

Un template con `power_binding_kind="gate"` non può essere condiviso fra
metriche diverse, perché `power_binding_id` deve coincidere con il rispettivo
`metric_id`. Template unpowered o family-bound possono essere condivisi
soltanto quando tutti i campi e le popolazioni richieste coincidono.

## 4. Decisione proposta O13F_03-B — scope del catalogo

Il catalogo deve enumerare tutti gli output pubblicabili del candidate, inclusi
quelli diagnostici, ma soltanto gli output con `support_mode=own_policy`
entrano direttamente nel metric/stratum registry.

### 4.1 Devono essere catalogati

Almeno le seguenti famiglie descritte nel candidate:

```text
tonal curve:
  weighted MAE
  weighted RMSE
  p95 absolute error
  residual improvement
  sign error

tonal semantic:
  clean actionable rate
  coverage_minus
  coverage_plus
  severity MAE
  severity Spearman

matching/event:
  TP, FP, FN
  precision, recall, F1
  severity MAE upper envelope
  onset upper envelope
  offset upper envelope
  severity Spearman
  Resonance center error
  Average Precision
  false events per minute
  duration/occupancy

calibration:
  ECE
  Brier
  Average Precision reference/binding

homologous adapter:
  macro-F1 on six classes
  false-positive group rate on clean
```

Questa lista è un inventario semantico minimo, non assegna ancora gli ID.
L'appendice dati successiva deve espanderla per `record_family` e
`problem_type` dove richiesto e deve provare che nessuna metrica obbligatoria
del candidate sia omessa.

Quando la stessa misura possiede floor o power binding indipendenti in due
famiglie benchmark o subgroup claim, l'espansione usa `metric_id` distinti con
variant canonica. Non si introduce un nuovo `population_kind` per nascondere
più family gate dentro una sola policy. Questa regola mantiene il registry
esprimibile con i `population_kind` della policy v3 e rende ogni gate/power row
univoco.

### 4.2 Diagnostici senza floor autonomo

I soli output già classificati diagnostici dall'autorità corrente possono
usare `support_mode=none_diagnostic` senza policy autonoma:

```text
p95, finché resta diagnostico
coverage_plus
duration/occupancy
```

TP, FP, FN, precision, recall e F1 sono elencati dal candidate fra le metriche
obbligatorie. Devono quindi essere `mandatory_normative`, avere policy propria
per split e mantenere `G_defined` separato anche quando condividono lo stesso
template. La condivisione della popolazione non autorizza support inheritance.

Non è ammesso omettere output dal catalogo e poi introdurre naming o supporto
ad hoc nel report.

### 4.3 Average Precision

`average_precision` è un solo concetto normativo. L'alias `PR-AUC` non genera
un secondo `metric_id`. Eventuali riferimenti calibration/event devono puntare
allo stesso ID canonico per la stessa partizione scientifica oppure dichiarare
esplicitamente due metriche distinte; non possono divergere per spelling.

## 5. Decisione proposta O13F_03-C — namespace canonico

Gli ID devono essere ASCII lowercase e rispettare esattamente:

```text
^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$
```

Sono quindi vietati whitespace, NUL, `:`, slash, backslash, trattini, segmenti
vuoti, `..`, segmenti iniziati da cifra, maiuscole e alias.

Forma:

```text
<domain>.<measure>[.<record-family>][.<problem-type>][.<variant>]
```

Gli slug di problem type sono congelati come:

```text
resonance
muddiness
boominess
thinness
boxy_midrange
dull_sound
harshness
sibilance
```

Gli slug di record family sono:

```text
semantic_region
dynamic_event
```

La lista completa dei `metric_id` deve essere materializzata e firmata nello
stesso commit del primo registry data artifact. La grammatica non autorizza
ID non enumerati.

Le stringhe fixture legacy con `:` o naming abbreviato restano test data e non
sono migrate automaticamente.

## 6. Decisione proposta O13F_03-D — split e power

### 6.1 Split con policy O-13

Il registry può contenere policy soltanto per:

```text
calibration
development-metric
final-test
```

`development_pilot` è sorgente esclusiva del power plan e non è uno split di
valutazione O-13. `train` e `validation` non producono claim O-13.

Ogni coppia `(metric_id,split_role)` è unica. Una metrica ammessa su più split
richiede un binding separato per ciascuno.

### 6.2 Binding agli input dello split

Il registry materializzato dipende dagli input ammessi dello split, perché
profili, `source_family`, positivi/negativi GT e completezza delle classi non
sono deducibili dal solo contratto. Per ogni split referenziato dai binding
deve esistere una entry exact-key:

```text
split_role
evaluation_unit_index_id
evaluation_unit_index_sha256
asset_manifest_sha256
annotation_package_sha256
frontend_contract_sha256
```

I cinque digest/ID devono coincidere con
`aieq-v3-evaluation-unit-index-1`; il validator ricalcola e verifica l'indice,
la lista normalizzata dei manifest, il package adjudicated e il frontend lock.
Una entry extra, mancante o riferita a un altro split produce FAIL.

Il registry viene materializzato dopo il freeze degli evaluation-unit index e
prima di leggere prediction dello split destinatario. Non può essere rigenerato
dopo aver osservato N/A, metriche o gate.

La completezza degli strata data-dependent è una proiezione esatta degli input
legati:

1. tutti e sette i profili canonici richiesti devono avere la propria riga;
2. ogni `source_family` presente nella popolazione parent rilevante deve avere
   esattamente una riga child per il controllo del ceiling; ometterne una è
   FAIL. Il requisito «almeno tre family con almeno cinque gruppi» è verificato
   dalla precondizione relazionale di §3.2.1, non imponendo cinque gruppi a
   tutte le family osservate;
3. tutte le celle regione×direzione richieste dal candidate devono essere
   enumerate, incluse quelle a supporto zero;
4. ogni classe per cui sono richiesti positivi, negativi o minuti clean deve
   avere esattamente le righe previste;
5. le righe data-dependent non possono essere selezionate per far passare un
   minimo o un ceiling.

Questa regola impedisce, per esempio, di omettere dal registry una
`source_family` dominante e aggirare il ceiling del 50%.

### 6.3 Power binding

Il registry congela la **regola** di binding:

```text
power_binding_kind = null | gate | family
power_binding_id   = null | canonical ID
```

ma non inventa `n_power`.

Quando il binding è `gate`, `power_binding_id` deve coincidere con il
`metric_id` canonico della policy. Quando è `family`, deve essere una family
enumerata dal power plan reale e appartenere esclusivamente a:

```text
tonal-controlled
tonal-natural
anomaly-natural
clean-safety
electronic-stratified
```

Il primo power plan reale deve usare gli stessi canonical ID del catalogo; ID
fixture legacy non sono authority.

Una riga power-governed non può produrre policy ufficiale finché il power plan
reale, il suo digest atteso e la riga corrispondente non esistono.

## 7. Decisione proposta O13F_03-E — exact-key artifact e digest

Il primo artefatto deve avere schema:

```text
aieq-v3-rev8-o13-metric-stratum-registry-1
```

e top-level exact keys:

```text
schema
contract_revision
source_contract_sha256
o13f01_authority_commit
o13f01_recheck_commit
o13f02_authority_commit
o13f02_recheck_commit
o13f03_authority_commit
o13f03_recheck_commit
population_selector_schema_id
population_selector_schema_sha256
registry_precondition_schema_id
registry_precondition_schema_sha256
metric_definitions
support_templates
bindings
registry_preconditions
split_input_bindings
```

Regole:

1. JSON canonico UTF-8 secondo l'autorità G1a già congelata;
2. newline LF finale;
3. chiavi extra, duplicate o mancanti: FAIL;
4. array ordinati per le chiavi canoniche in byte UTF-8;
5. exact-key anche per ogni entry e ogni strato;
6. nessun numero binary64: floor e frazioni sono interi;
7. SHA-256 lowercase hex-64 dei canonical bytes dell'intero artefatto;
8. il digest atteso è esterno all'artefatto e firmato nel report di freeze;
9. auto-hash o digest ricavato dall'input non fidato non è authority;
10. l'artefatto deve legarsi esattamente al candidate SHA e alle commit
    authority/recheck O13F_01, O13F_02 e O13F_03;
11. duplicati, riferimenti pendenti, cicli parent, template non usati,
    metriche `own_policy` senza un binding per ogni split ammesso, metriche
    `none_diagnostic` con binding, precondizioni non referenziate o binding non
    ammessi dal catalogo: FAIL;
12. ogni policy deve essere confrontata con la proiezione esatta della propria
    entry registry, non soltanto con un digest auto-dichiarato.
13. gli ID dei due companion schema sono letterali canonici; i rispettivi
    digest sono lowercase hex-64 e devono corrispondere ai canonical bytes
    degli artefatti materializzati nello stesso commit.

Ordini normativi degli array:

```text
metric_definitions   -> metric_id, byte UTF-8
support_templates    -> support_template_id, byte UTF-8
bindings             -> (metric_id, split_role), byte UTF-8
registry_preconditions -> precondition_id, byte UTF-8
split_input_bindings -> split_role, byte UTF-8
strata               -> stratum_id, byte UTF-8
allowed_split_roles  -> valore del ruolo, byte UTF-8
precondition_ids     -> precondition_id, byte UTF-8
```

### 7.1 Compatibilità con `support-floor-policy-3`

O13F_03 non aggiunge campi alla policy e non introduce uno schema policy v4.
L'integrazione obbligatoria è:

```text
registry artifact + expected_registry_sha256
  -> validator exact-key/hash/input-binding
  -> valutazione delle precondizioni relazionali referenziate
  -> projection esatta del binding/template richiesto
  -> compiler support-floor-policy-3
```

Il `POLICY_REVISION` candidate deve essere aggiornato per includere commit di
firma e recheck O13F_03. Poiché non esistono policy ufficiali, nessuna authority
attiva viene migrata o invalidata.

Il digest del registry resta un input autoritativo esterno alla policy v3 e
deve essere materializzato insieme al `policy_sha256` nel futuro envelope di
report/activation. Una policy v3 isolata, anche se auto-consistente, non è
authority senza:

```text
expected_registry_sha256
registry projection match
registry precondition results
expected_policy_sha256
population_plan_sha256
power_plan_sha256, quando richiesto
```

Il compiler low-level corrente può restare una primitiva candidate per i test,
ma non può costituire il path ufficiale. Il path ufficiale deve ricevere il
registry e il suo digest atteso; passare template costruiti direttamente dal
caller senza projection check produce FAIL.

Se il futuro report/activation envelope non possiede un campo autoritativo per
`registry_sha256`, l'attivazione resta bloccata finché tale envelope non viene
definito e controfirmato. Questo requisito non autorizza a modificare la policy
v3 o il dispatcher REV7 dentro la tranche registry.

## 8. Regole per il data artifact successivo

Il primo registry data artifact può essere scritto soltanto dopo la firma e il
recheck di O13F_03. Deve contenere una matrice di tracciabilità con, per ogni
metrica del candidate:

```text
sezione sorgente
nome semantico
metric_id canonico
definition_key
evaluation_scope_id
publication_status
split ammessi
support_mode
per ogni split: support_template_id o assenza motivata del binding
per ogni split: precondition_ids o lista vuota motivata
motivazione dell'eventuale condivisione del template
per ogni split/template: power binding o null
per ogni split: evaluation-unit index e digest input legati
```

La matrice deve dimostrare:

1. copertura completa delle metriche obbligatorie §§10.1–10.5;
2. copertura degli strata §§10.4 e 11.1;
3. nessuna promozione del p95 diagnostico;
4. nessun alias `PR-AUC` distinto da Average Precision;
5. nessun floor `10 group_id` derivato dal supporto interno Spearman;
6. nessun uso degli ID fixture come authority implicita;
7. nessun `n_power` sintetico o di esempio;
8. nessuna policy ufficiale senza population plan e power plan reali firmati.
9. proiezione completa di profili, source family, regioni/direzioni e classi
   dagli input dello split, senza righe omesse.
10. ogni minimo relazionale o frazione minima del parent richiesti dal
    candidate è coperto da una precondizione firmata, senza irrigidire i floor
    dei singoli child.

## 9. Counter-check e fixture richiesti prima dell'efficacia

### 9.1 Completezza semantica

- ogni bullet di “Metriche obbligatorie” nel candidate è coperto;
- ogni metrica calibration/adapter è classificata;
- ogni problem type applicabile è espanso;
- ogni diagnostico è presente ma incapace di soddisfare gate;
- nessun output inventato dal codice è promosso senza authority.

### 9.2 Integrità referenziale

- metric ID duplicato: FAIL;
- tupla `(definition_key,evaluation_scope_id,record_family,problem_type)`
  duplicata: FAIL;
- ID conforme alla grammatica ma non enumerato: FAIL;
- output del report con metric ID non catalogato: FAIL;
- output obbligatorio catalogato ma assente dal report: FAIL;
- stessa formula/partizione/record/type rinominata con un secondo metric ID:
  FAIL;
- binding verso metrica/template assente: FAIL;
- template non referenziato: FAIL;
- split non ammesso dal catalogo: FAIL;
- policy missing/extra rispetto al binding: FAIL;
- strato missing/extra/basis differente: FAIL;
- parent assente, self-parent o ciclo: FAIL;
- mandatory metric senza binding in uno split richiesto: FAIL.
- metrica `none_diagnostic` usata in gate/claim o con policy: FAIL;
- metrica normativa senza policy propria per lo split: FAIL;
- riga profile/source-family/regione/direzione/classe omessa rispetto agli
  input legati: FAIL.
- precondition ID mancante, extra, duplicato o incoerente con binding,
  split o scope: FAIL;
- requisito «almeno tre family con almeno cinque gruppi» sostituito da un floor
  cinque su tutte le family: FAIL;
- `electronic-stratified < 2/5` del parent applicabile: N/A/no PASS;
- `minimum_qualifying_partitions` con due sole source family qualificate: N/A/no
  PASS; con tre al confine firmato: la sola precondizione è soddisfatta;

### 9.3 Support basis e power

- root mandatory diverso da `eligible_and_defined`: FAIL;
- root di una metrica normativa `own_policy` diverso da
  `eligible_and_defined`: FAIL nel registry path anche se `mandatory=false`;
- `eligible_only` non allowlisted dal registry: FAIL;
- supporto definito preso in prestito da un altro metric ID: FAIL;
- cambio del basis dopo la firma: hash mismatch/FAIL;
- gate binding con ID diverso dal `metric_id`: FAIL;
- family binding non presente nel power plan: FAIL;
- power richiesto ma piano reale assente: `SUPPORT_POLICY_UNAVAILABLE`;
- power plan fixture scambiato per authority: FAIL;
- `n_required != max(contract_floor,n_power)`: FAIL.
- policy v3 compilata da template caller-supplied senza registry projection:
  non ufficiale/FAIL nel path di report.
- population plan caller-supplied diverso dalla derivazione degli input legati:
  FAIL;
- esito di precondizione relazionale mancante nel report envelope: no PASS.
- `POLICY_REVISION` privo delle commit firma/recheck O13F_03: FAIL nel path
  ufficiale;

### 9.4 Canonicalizzazione

- permutare input semantici produce gli stessi canonical bytes soltanto nel
  percorso di creazione;
- un artefatto committed non canonico viene rifiutato, non riordinato in
  lettura;
- cambio di un solo ID, split, basis, floor, binding o ceiling cambia digest;
- cambio di selector, precondition, input-binding o `evaluation_scope_id`
  cambia digest;
- digest atteso diverso: FAIL;
- colon, uppercase, alias o slug non canonico: FAIL.
- cambio dell'evaluation-unit index dopo il freeze: hash mismatch/FAIL;

### 9.5 Regressione

- suite REV8 esistente invariata;
- protected diff zero verso `Source`, `CMakeLists.txt`, `Resources`, `ml_v2`,
  `ml` e `training`;
- nessuna esportazione dal dispatcher REV7;
- nessuna policy ufficiale o activation manifest prodotto dalla tranche
  registry.

## 10. Non-decisioni

O13F_03 non decide:

- la lista finale dei `metric_id`;
- la riga esatta metric×split×stratum;
- membership di popolazione;
- valori `n_power`;
- gate threshold;
- quali gate primari entreranno nella procedura Holm;
- esiti del candidate;
- attivazione REV8.

Questi elementi richiedono rispettivamente il registry data artifact firmato,
il corpus/power plan reale o gate successivi.

## 11. Stop rule

Fino a un post-signature recheck `CLEAN` di O13F_03:

```text
registry schema implementation  = NON AUTORIZZATA
registry data artifact          = NON AUTORIZZATO
official policies               = NON MATERIALIZZABILI
REV8 dispatcher                 = INVARIATO
REV8 SPEC GO                    = NO
G1c close / G1 PASS             = NO
runtime / training / push       = NON AUTORIZZATI
```

## 12. Ballot

```text
Decisione O13F_03: APPROVO
Firma/nome:          Marco
Data:                2026-08-10

SHA-256 ballot pre-firma verificato:
23a31daa99a78510c6b666a94749f590853faeb38064aee6cf06a40c99bddfa5

Commit ballot pre-firma verificato:
5120b5728c781e10a898335e0f0c9bbc72485944
```

Opzioni ammesse:

```text
APPROVO
RESPINGO
RICHIEDO MODIFICHE (con motivazione testuale)
```
