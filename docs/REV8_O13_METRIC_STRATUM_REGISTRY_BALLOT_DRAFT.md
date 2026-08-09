# REV8 — O-13 — METRIC/STRATUM REGISTRY ARCHITECTURE BALLOT (O13F_03)

## Stato del documento

```text
Tipo                            = micro-ballot normativo docs-only pre-firma
Data preparazione               = 2026-08-09
Branch                          = feature/motore-v3-rev8-spec-go
Base commit                     = 468f7e3212d386c916642f7a292d63af7e1d8ad0
Decisione O13F_03               = NON FIRMATA
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
record_family
problem_type
publication_status
support_mode
support_parent_metric_id
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

3. `support_mode` appartiene a:

   ```text
   own_policy
   inherit
   none_diagnostic
   ```

4. `own_policy` richiede `support_parent_metric_id=null` e un binding per ogni
   split ammesso;
5. `inherit` richiede un `support_parent_metric_id` enumerato con
   `support_mode=own_policy`, vieta binding propri, eredita esattamente
   popolazioni, N/A e sufficienza del parent negli split comuni e non può
   ampliare gli split del parent;
6. `none_diagnostic` richiede `publication_status=diagnostic_only`, parent
   nullo e nessun binding; non può soddisfare gate o claim;
7. un output `mandatory_normative` deve usare `own_policy` o `inherit`;
8. i link `inherit` devono essere aciclici e non possono attraversare metriche
   con diversa partizione scientifica;
9. `record_family` e `problem_type` sono `null` soltanto quando la metrica è
   realmente globale;
10. `allowed_split_roles` è una lista non vuota, senza duplicati, ordinata per
   byte UTF-8 e composta soltanto da ruoli canonici;
11. una metrica diagnostica non può soddisfare un gate o una claim.

### 3.2 Metric/stratum registry

Il **metric/stratum registry** lega ciascuna policy richiesta a uno split e a
un template di strata. Non contiene membership di gruppi osservati.

Ogni binding ha exact-key schema:

```text
metric_id
split_role
mandatory
support_template_id
```

Ogni template ha exact-key schema:

```text
support_template_id
strata[]:
  stratum_id
  population_kind
  parent_stratum_id
  support_basis
  contract_floor
  power_binding_kind
  power_binding_id
  max_parent_fraction_numerator
  max_parent_fraction_denominator
```

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

### 4.2 Non diventano policy autonome per il solo fatto di essere catalogati

I seguenti possono essere catalogati `diagnostic_only` o come derivati senza
policy autonoma, ma la scelta deve essere esplicita:

```text
p95, finché resta diagnostico
coverage_plus
duration/occupancy
TP/FP/FN quando pubblicati soltanto come componenti dello stesso report F1
precision/recall quando condividono esattamente popolazione e definibilità F1
```

Non è ammesso ometterli dal catalogo e poi introdurre naming o supporto ad hoc
nel report.

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

### 6.2 Power binding

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
o13f02_authority_commit
metric_definitions
support_templates
bindings
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
    authority O13F_01/O13F_02;
11. duplicati, riferimenti pendenti, cicli parent/`inherit`, template non
    usati, metriche `own_policy` senza un binding per ogni split ammesso,
    metriche `inherit` o `none_diagnostic` con binding, o binding non ammessi
    dal catalogo: FAIL;
12. ogni policy deve essere confrontata con la proiezione esatta della propria
    entry registry, non soltanto con un digest auto-dichiarato.

## 8. Regole per il data artifact successivo

Il primo registry data artifact può essere scritto soltanto dopo la firma e il
recheck di O13F_03. Deve contenere una matrice di tracciabilità con, per ogni
metrica del candidate:

```text
sezione sorgente
nome semantico
metric_id canonico
publication_status
split ammessi
support_mode
support_parent_metric_id o null
per ogni split: support_template_id o assenza motivata del binding
motivazione dell'eventuale condivisione del template
per ogni split/template: power binding o null
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

## 9. Counter-check e fixture richiesti prima dell'efficacia

### 9.1 Completezza semantica

- ogni bullet di “Metriche obbligatorie” nel candidate è coperto;
- ogni metrica calibration/adapter è classificata;
- ogni problem type applicabile è espanso;
- ogni diagnostico è presente ma incapace di soddisfare gate;
- nessun output inventato dal codice è promosso senza authority.

### 9.2 Integrità referenziale

- metric ID duplicato: FAIL;
- ID conforme alla grammatica ma non enumerato: FAIL;
- binding verso metrica/template assente: FAIL;
- template non referenziato: FAIL;
- split non ammesso dal catalogo: FAIL;
- policy missing/extra rispetto al binding: FAIL;
- strato missing/extra/basis differente: FAIL;
- parent assente, self-parent o ciclo: FAIL;
- mandatory metric senza binding in uno split richiesto: FAIL.

### 9.3 Support basis e power

- root mandatory diverso da `eligible_and_defined`: FAIL;
- `eligible_only` non allowlisted dal registry: FAIL;
- cambio del basis dopo la firma: hash mismatch/FAIL;
- gate binding con ID diverso dal `metric_id`: FAIL;
- family binding non presente nel power plan: FAIL;
- power richiesto ma piano reale assente: `SUPPORT_POLICY_UNAVAILABLE`;
- power plan fixture scambiato per authority: FAIL;
- `n_required != max(contract_floor,n_power)`: FAIL.

### 9.4 Canonicalizzazione

- permutare input semantici produce gli stessi canonical bytes soltanto nel
  percorso di creazione;
- un artefatto committed non canonico viene rifiutato, non riordinato in
  lettura;
- cambio di un solo ID, split, basis, floor, binding o ceiling cambia digest;
- digest atteso diverso: FAIL;
- colon, uppercase, alias o slug non canonico: FAIL.

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

Fino a firma e post-signature recheck di O13F_03:

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
Decisione O13F_03: NON FIRMATA
Firma/nome:
Data:

SHA-256 ballot pre-firma:
[DA MATERIALIZZARE SUL COMMIT IMMUTABILE]

Commit ballot pre-firma:
[DA MATERIALIZZARE]
```

Opzioni ammesse:

```text
APPROVO
RESPINGO
RICHIEDO MODIFICHE (con motivazione testuale)
```
