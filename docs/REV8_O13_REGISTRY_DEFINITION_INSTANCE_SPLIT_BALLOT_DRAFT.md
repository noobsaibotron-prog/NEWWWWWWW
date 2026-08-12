# REV8 — O-13 — REGISTRY DEFINITION/INSTANCE SPLIT BALLOT (O13F_04)

## Stato del documento

```text
Tipo                             = micro-ballot normativo docs-only pre-firma
Data preparazione                = 2026-08-10
Branch                           = feature/motore-v3-rev8-spec-go
Base commit                      = e7bae25e0fe17dc060d931d250473589664d6968
Decisione O13F_04                = APPROVO — PENDING POST-SIGNATURE RECHECK
Static registry definition       = NON MATERIALIZZATA
Official split registry instance = NON MATERIALIZZATA
Registry implementation          = NON AUTORIZZATA
REV8 dispatcher activation       = NO
REV8 SPEC GO                     = NO
```

Questo ballot non modifica metriche, floor, power analysis, selector
scientifici o gate. Corregge esclusivamente una dipendenza circolare fra
definition statica del registry e dati reali dello split.

## 1. Authority consumata

```text
Candidate REV8:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_01 firma/recheck:
59bec34856a08aa43cc52bb123e904a44575f4f6
d0b9916cd08aff29dfde03ffe0c8f38c3656a96c

O13F_02 firma/recheck:
26f35e753f96ebc48d0453e432e8f2a3f5380687
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e

O13F_03 pre-sign freeze:
5120b5728c781e10a898335e0f0c9bbc72485944
SHA-256:
23a31daa99a78510c6b666a94749f590853faeb38064aee6cf06a40c99bddfa5

O13F_03 firma/recheck:
c3dafbb64cd156d020a8a4566d7adc937acefd06
e7bae25e0fe17dc060d931d250473589664d6968

O13F_03 signed SHA-256:
1f26519c2fee5a6363e28b09000e7a719343a851f618914e4cbf107206697d13
```

O13F_03 resta integralmente authority salvo la sola separazione di packaging e
sequencing esplicitamente descritta qui.

## 2. Finding verificato

O13F_03 colloca nello stesso artefatto:

```text
metric catalog e definition key statiche
selector/precondition schema statici
support template e binding statici
split_input_bindings reali
righe source_family data-dependent
```

Non esistono ancora evaluation-unit index reali per `calibration`,
`development-metric` o `final-test`; non esiste un power plan reale. L'unico
power plan è una fixture con `pilot_sha256="aa...aa"` e
`support.notes="fixture"`.

Il candidate §15 stabilisce invece che la disponibilità del corpus reale viene
verificata in G3/G4 e non blocca l'implementazione G1. Il packaging monolitico
impedirebbe di congelare gli schema necessari al validator G1c fino alla
disponibilità del corpus G3.

Promuovere le fixture, inventare index vuoti o omettere gli input binding sono
tutti comportamenti vietati.

## 3. Decisione proposta O13F_04-A — due artefatti, una sola semantica

L'authority O13F_03 viene materializzata in due livelli distinti:

```text
registry definition = identità e regole statiche, indipendenti dal corpus
registry instance   = espansione per un solo split e input hash-bound
```

La separazione non introduce fallback. Una policy ufficiale richiede entrambi
i digest attesi:

```text
expected_registry_definition_sha256
expected_registry_instance_sha256
```

più policy, population plan e power plan già richiesti da O13F_01–03.

## 4. Registry definition statica

### 4.1 Artefatto

Schema:

```text
aieq-v3-rev8-o13-registry-definition-1
```

Top-level exact keys:

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
o13f04_authority_commit
o13f04_recheck_commit
population_selector_schema_id
population_selector_schema_sha256
registry_precondition_schema_id
registry_precondition_schema_sha256
metric_definitions
support_template_blueprints
binding_blueprints
precondition_blueprints
```

La definition non contiene:

```text
evaluation-unit index
manifest o annotation digest di uno split
source_family osservate
population membership
n_power osservato
policy compilate
prediction o risultati
```

### 4.2 Metric catalog

`metric_definitions` conserva integralmente exact-key, enumerazioni, regole di
unicità e status firmati in O13F_03. La lista completa dei metric ID viene
materializzata nello stesso commit della prima registry definition.

### 4.3 Binding blueprint

Ogni binding blueprint ha exact keys:

```text
metric_id
split_role
mandatory
support_template_blueprint_id
precondition_blueprint_ids
```

La coppia `(metric_id,split_role)` è unica. Gli split ammessi e la coerenza
mandatory/optional/diagnostic restano quelli di O13F_03.

### 4.4 Support-template blueprint

Ogni template ha exact keys:

```text
support_template_blueprint_id
stratum_blueprints
```

Ogni stratum blueprint ha exact keys:

```text
stratum_blueprint_id
population_kind
expansion_mode
population_selector_template
parent_blueprint_id
support_basis
contract_floor
power_binding_kind
power_binding_id
max_parent_fraction_numerator
max_parent_fraction_denominator
```

`expansion_mode` appartiene esclusivamente a:

```text
single
for_each_profile
for_each_source_family_in_parent
for_each_tonal_region_direction
for_each_problem_type
```

Il companion selector schema definisce exact-key e dominio del
`population_selector_template` per ciascuna coppia
`(population_kind,expansion_mode)`. Testo descrittivo, regex libera o parsing
dello `stratum_blueprint_id` sono vietati.

Per `single`, il concrete `stratum_id` coincide con lo
`stratum_blueprint_id`. Per una expansion, il concrete ID è:

```text
stratum_blueprint_id + ".h" +
SHA256(canonical_bytes(concrete_population_selector))
```

Il digest è lowercase hex-64 completo. Il selector concreto resta comunque
campo autoritativo: l'ID non viene analizzato per ricostruirlo.

Un parent blueprint deve essere `single` oppure usare la stessa expansion e un
selector parent derivabile per proiezione exact-key dal selector child. Ogni
altra topologia è non implementabile e produce FAIL al freeze della
definition.

### 4.5 Precondition blueprint

I precondition blueprint congelano i due `predicate_kind` O13F_03 e i numeri
già presenti nel candidate, ma non membership o conteggi osservati.

Ogni precondition blueprint ha un ID canonico, split, scope, selector template,
predicate kind, threshold esatti e reason code. Lo schema discriminato completo
viene firmato nello stesso commit della definition.

## 5. Registry instance per split

### 5.1 Artefatto

Schema:

```text
aieq-v3-rev8-o13-registry-instance-1
```

Top-level exact keys:

```text
schema
instance_purpose
registry_definition_sha256
split_role
evaluation_unit_index_id
evaluation_unit_index_sha256
asset_manifest_sha256
annotation_package_sha256
frontend_contract_sha256
expanded_support_templates
expanded_bindings
expanded_preconditions
```

`instance_purpose` appartiene esclusivamente a:

```text
conformance_fixture
official_split
```

Ogni instance copre un solo split.

### 5.1.1 Exact-key delle righe espanse

Ogni entry di `expanded_support_templates` ha exact keys:

```text
support_template_id
strata
```

e conserva:

```text
support_template_id == support_template_blueprint_id
```

Ogni strato concreto usa l'exact-key companion schema già richiesto da
O13F_03. Il suo `stratum_id` e il suo `population_selector` sono la proiezione
deterministica del blueprint secondo §4.4; floor, basis, parent e power binding
non possono essere ridefiniti dall'instance.

Ogni entry di `expanded_bindings` ha exact keys:

```text
metric_id
split_role
mandatory
support_template_id
precondition_ids
```

ed è la proiezione exact-value di un solo binding blueprint applicabile allo
split. Ogni entry di `expanded_preconditions` usa il companion precondition
schema con selector concreto; non contiene l'esito osservato del predicato.

Gli array top-level e annidati sono ordinati con le stesse chiavi byte UTF-8
di O13F_03. I canonical bytes seguono l'autorità JSON G1a, terminano con un
solo LF e rifiutano chiavi extra, mancanti o duplicate. Il digest atteso della
definition e quello dell'instance sono esterni ai rispettivi artefatti.

### 5.2 Espansione deterministica

Il validator:

1. verifica definition e digest atteso;
2. verifica index, manifest, annotation package e frontend lock;
3. espande tutti e soli i blueprint applicabili allo split;
4. enumera tutti i sette profili richiesti, anche con supporto zero;
5. enumera ogni source family nella popolazione parent rilevante;
6. enumera tutte le celle regione×direzione e classi richieste;
7. materializza i selector concreti e gli ID derivati;
8. materializza tutti e soli i predicati relazionali concreti; la loro
   valutazione usa il population plan congelato e avviene prima delle
   prediction;
9. rifiuta qualsiasi riga extra, mancante o riordinata nei committed bytes.

Nessuna scelta data-dependent è lasciata al caller. L'espansione di un input
valido produce un solo documento canonico.

### 5.3 Conformance fixture

Una instance `conformance_fixture`:

- usa input sintetici esplicitamente marcati come fixture;
- può essere usata per oracle, mutation e test cross-language;
- non può essere usata per una policy ufficiale, un gate o un activation
  envelope;
- produce sempre authority status
  `CONFORMANCE_ONLY_NOT_SCIENTIFIC_AUTHORITY`.

Qualunque mutation che accetta una conformance fixture come `official_split`
deve essere uccisa.

### 5.4 Official split

Una instance `official_split`:

- richiede input reali ammessi e digest esterno preregistrato;
- viene materializzata prima di leggere prediction;
- non può essere rigenerata dopo N/A, metriche o gate;
- richiede power plan reale prima di compilare una riga power-governed;
- deve essere legata nel report envelope insieme a definition e policy digest.

I risultati delle precondizioni non vengono auto-dichiarati nell'instance.
Devono essere prodotti da un evaluator deterministico contro il population
plan congelato e hash-bound separatamente nel futuro report/activation
envelope. Risultato mancante, extra, fallito o non legato ai digest corretti
produce N/A/no PASS secondo O13F_03.

Input reali mancanti producono `SUPPORT_POLICY_UNAVAILABLE`, mai fallback a una
fixture.

## 6. Compatibilità con O13F_03 e policy v3

La proiezione ufficiale diventa:

```text
definition + expected definition SHA
  -> instance + expected instance SHA
  -> exact binding/template/precondition projection
  -> population plan reale
  -> power plan reale, quando richiesto
  -> support-floor-policy-3
```

`support-floor-policy-3` resta invariata. Non viene introdotto uno schema
policy v4. Nel futuro report/activation envelope il precedente concetto
`registry_sha256` è sostituito dalla coppia di digest registry, accompagnata
dal digest separato dei risultati delle precondizioni:

```text
registry_definition_sha256
registry_instance_sha256
registry_precondition_results_sha256
```

Il `POLICY_REVISION` candidate deve includere le commit di firma e recheck
O13F_03 e O13F_04. Il terzo digest lega l'evidenza deterministica dei predicati
referenziati; omissione o mismatch di uno dei tre impediscono PASS. Questo
chiarimento non aggiunge campi a `support-floor-policy-3`: riguarda soltanto il
futuro report/activation envelope, che resta fuori dalla presente tranche.

## 7. Freeze e sequencing

Dopo firma e recheck O13F_04 sono consentiti, in un'unica tranche atomica:

1. selector companion schema;
2. precondition companion schema;
3. registry definition completa con metric catalog e blueprint;
4. matrice di tracciabilità candidate-clause → metric/scope/split;
5. instance `conformance_fixture` positive e negative;
6. freeze pre-firma e counter-check del package immutabile;
7. firma esplicita dell'autorità e post-signature recheck.

Questa tranche non richiede corpus G3 e autorizza soltanto la successiva
implementazione candidate-only del validator/expander dopo esito
post-signature `CLEAN`; freeze o counter-check pre-firma da soli non
autorizzano codice.

Le instance `official_split`, population plan e policy ufficiali restano
bloccate fino alla disponibilità e al freeze degli input reali.

## 8. Fixture e mutation obbligatorie

- stesso definition + stessi input, ordine permutato in creation path → stessi
  canonical bytes;
- committed bytes riordinati → FAIL;
- definition digest errato → FAIL;
- instance digest errato → FAIL;
- blueprint extra/mancante → FAIL;
- concrete row extra/mancante → FAIL;
- nuova source family nell'input → instance digest differente e nuova riga;
- omissione di una source family dominante → FAIL;
- due sole family qualificate contro requisito tre → N/A/no PASS;
- electronic-stratified appena sotto `2/5` → N/A/no PASS;
- selector hash/ID mismatch → FAIL;
- collisione o duplicate selector → FAIL;
- instance fixture nel path official → FAIL;
- official instance senza input reali o digest preregistrato →
  `SUPPORT_POLICY_UNAVAILABLE`;
- policy compilata direttamente da blueprint senza instance projection → non
  ufficiale/FAIL;
- cambio di definition dopo instance freeze → hash mismatch/FAIL;
- REV7 dispatcher isolation invariata.

## 9. Non-decisioni

O13F_04 non decide:

- metric ID e definition key concreti;
- blueprint concreti;
- selector/precondition exact-key finali;
- evaluation-unit index o population membership;
- `n_power`;
- gate threshold o Holm family;
- esiti scientifici;
- activation.

## 10. Stop rule

Fino a firma e post-signature recheck O13F_04:

```text
companion schema/package          = NON AUTORIZZATI
registry definition data          = NON AUTORIZZATA
registry validator/expander code  = NON AUTORIZZATO
official registry instance        = NON MATERIALIZZABILE
official policies                 = NON MATERIALIZZABILI
REV8 dispatcher                   = INVARIATO
REV8 SPEC GO                      = NO
G1c close / G1 PASS               = NO
runtime / training / push         = NON AUTORIZZATI
```

## 11. Ballot

```text
Decisione O13F_04: APPROVO
Firma/nome: Marco
Data: 2026-08-10

SHA-256 ballot pre-firma:
13764e519ef64d231732d5ad89bfb36727b1479382db8afac383d70010662f69

Commit ballot pre-firma:
019dcdadac0872331a8d885256673cb39ebb0bb0
```

Opzioni ammesse:

```text
APPROVO
RESPINGO
RICHIEDO MODIFICHE (con motivazione testuale)
```
