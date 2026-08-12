# REV8 — O-13 — REGISTRY CONFORMANCE PACKAGE MATERIALIZATION REPORT

## Verdetto

```text
Verdetto                              = PRE_FREEZE_READY
Data                                  = 2026-08-10
Branch                                = feature/motore-v3-rev8-spec-go
Base HEAD                             = f201fa162752857c25fb5114e8ccc74a2798c85f
O13F_05 authority                     = SIGNED + POST_SIGNATURE_CLEAN
Package purpose                       = CONFORMANCE_ONLY_NOT_SCIENTIFIC_AUTHORITY
Official registry instance/policies   = NON MATERIALIZZATI
Registry validator/expander code      = NON MODIFICATO
REV8 dispatcher                       = INVARIATO
REV8 SPEC GO                          = NO
```

Il package materializza la sola tranche docs/data autorizzata dal recheck
O13F_05. Non contiene prediction, risultati scientifici, membership reale,
power plan reale, policy ufficiale o codice di produzione.

## 1. Perimetro materializzato

Directory:

```text
docs/data/rev8/o13_registry_conformance_v2/
```

Artefatti e SHA-256:

```text
6781e4a51bcc297b4b32dfc57543b1d7074da1d781f72ca22ac320d7810176db  calibration_readiness_results_v1.json
dd8e52a19acf0cbe70da1d2f451befa60d145421785e6e2716a3a4d4776a62ae  candidate_metric_scope_subject_matrix_v1.json
c87d5db81b74985102f6e2b14ca1077851326e209ba3cfa410c46c28cf24bcaf  conformance_input_facts_v1.json
76713a10547c090e58ba1df5d450cb0c00df91a82e4133795b24a9b5e6f56d88  population_selector_schema_v2.json
cb300740d31512b0140a0bea90b2499e98fe66a0402564d9fb75fed538daf8f0  registry_claim_plan_conformance_v1.json
d61a15db7c87ea07fd50ea4d7ce90153b8aefe2cc60ec0278d71645918e634c5  registry_conformance_mutations_v1.json
02a7b603bbb4a3cc934907f7d205123d015051bd1f8f84ac79c48d8c141ca443  registry_definition_v2.json
7ec116138426c3613c3fbb28f9c5805f7e50cdbe9428d36516d6b4996c3af61f  registry_instance_calibration_v2.json
587c5a4e4e97f6db392f5a6f1615302a4b5aca5d5a21767c9b369d2e3bc087c3  registry_instance_development_metric_v2.json
75de3d0e6aea3bce596a4ffc9947bfc012af61dd3f2177bb03977e6ef0c77bf3  registry_instance_final_test_v2.json
b0502b5f4f3cfaeed879381d324633e628878a1220f524d93db5db731cc8dd0c  registry_precondition_schema_v2.json
```

Il file `SHA256SUMS` contiene esattamente queste undici entry, ordinate per
nome file. Il suo SHA-256 è:

```text
4c6ff97ec04745d641f801add54407513a0dc3cf27cd5ef6225411de06a019d0
```

## 2. Companion schema

### 2.1 Population selector

`population_selector_schema_v2.json` congela:

- i dieci `population_kind`, incluso `electronic_subgenre_groups`;
- i sei `expansion_mode` O13F_04/O13F_05;
- exact-key distinti per selector blueprint e selector concreto;
- campi iniettati da ciascuna espansione;
- dominio dei sette profili, delle classi, delle direzioni e delle regioni;
- uguaglianza JSON esatta di `electronic_subgenre`, incluso `null`, senza
  normalizzazione Unicode;
- rifiuto dell'ordine non canonico nei committed bytes.

I selector child conservano tutti i campi necessari a ricostruire per
proiezione exact-key il selector parent. La prima bozza non rispettava questa
proprietà per regioni tonali, minuti clean e adapter; è stata corretta prima
del presente freeze candidate.

### 2.2 Registry precondition

`registry_precondition_schema_v2.json` congela exact-key blueprint/concreti
per:

```text
minimum_qualifying_partitions
minimum_parent_fraction
```

e discrimina i subject:

```text
metric_binding
calibrator_fit_binding
benchmark_readiness_binding
```

Non introduce predicate kind, threshold o reason code ulteriori rispetto a
O13F_03/O13F_05.

Le precondition relazionali selezionano popolazioni aggregate: i campi
`parent_population_selector` e `child_population_selector` conservano quindi
gli exact-key del selector template. `minimum_qualifying_partitions` applica
`partition_key` in fase di valutazione; non inietta il valore della singola
partizione nel selector e non materializza una precondition per ogni valore.
Il companion schema rende questa regola esplicita e fail-closed.

## 3. Registry definition v2

La definition contiene:

```text
metric_definitions                         = 164
mandatory_normative / own_policy          = 139
diagnostic_only / none_diagnostic          = 25
calibrator_fit_subject_definitions         = 4
benchmark_readiness_subject_definitions    = 1
diagnostic_breakdown_blueprint_definitions = 153
```

Checksum per famiglia:

```text
tonal_curve             = 7
tonal_semantic          = 101
matching_event          = 46
calibration             = 8
homologous_adapter      = 2
TOTAL                   = 164
```

Le sole definition diagnostiche sono p95, `coverage_plus`, durata e
occupancy. Tutte le definition output ammettono esattamente
`development-metric` e `final-test`; nessuna ammette `calibration`.

I 153 breakdown derivano da tutte e sole le metriche il cui parent scope è
una delle quattro famiglie benchmark:

```text
benchmark.tonal_controlled
benchmark.tonal_natural
benchmark.anomaly_natural
benchmark.clean_safety
```

Global calibration e adapter non vengono trasformati in breakdown
elettronici.

## 4. Claim-plan di conformance

Il claim plan è marcato:

```text
claim_plan_purpose = conformance_fixture
```

e contiene:

```text
metric binding blueprint                  = 278
metric-stratum power binding blueprint    = 350
calibrator-fit binding blueprint          = 4
calibrator dependency blueprint           = 268
benchmark readiness binding blueprint     = 1
diagnostic breakdown blueprint            = 153
precondition blueprint                    = 4
```

Le 278 binding sono la proiezione completa di 139 metriche `own_policy` sui
due split D/F. Le 25 metriche diagnostiche non hanno binding.

### 4.1 Gate e power fixture

Per falsificare il grafo, il claim plan congela cinque primary gate
esclusivamente fixture, uno per ciascuna famiglia di design necessaria:

```text
tonal_curve.weighted_rmse.tonal_controlled
tonal_semantic.clean_actionable_rate.clean_safety
tonal_semantic.f1.semantic_region.resonance.tonal_natural
matching.average_precision.dynamic_event.resonance.anomaly_natural
matching.false_events_per_minute.dynamic_event.resonance.clean_safety
```

Questo set non è una scelta scientifica e non può essere promosso al primo
`scientific_claim_plan`. Per ogni gate fixture il solo root è governing e
`gate`-bound; i child sono `null` perché verificano composizione/corpus, non la
numerosità indipendente dello statistico primario. La matrice di tracciabilità
registra esplicitamente questa motivazione.

### 4.2 Dependency graph

Le metriche di curva non consumano il fit. Le metriche semantic/event,
calibration-output e adapter che consumano confidence, soglie, bundle o eventi
sono collegate a tutti e soli i fit subject richiesti. Il grafo produce 268
dependency row D/F; calibration non contiene dependency consumer.

## 5. Istanze positive di conformance

Sono materializzate tre instance `conformance_fixture`, una per split:

```text
calibration
development-metric
final-test
```

La calibration instance non ha upstream dependency. Development e final
legano esattamente:

- SHA della calibration instance;
- SHA dei calibration readiness results;
- `source_split_role=calibration`.

La final instance contiene:

```text
153 breakdown blueprint × 7 selector osservati = 1071 breakdown concreti
```

I sette valori includono:

- `null`;
- un sottogenere non hardcoded (`trance`);
- due stringhe Unicode canonicamente equivalenti ma byte-distinte;
- membership sovrapposta dello stesso `group_id` in più breakdown.

Il parent elettronico usa quattro `group_id` unici su dieci final-test
congelati, quindi chiude esattamente il boundary:

```text
5 * 4 == 2 * 10
```

### 5.1 Limite deliberato degli input fixture

`conformance_input_facts_v1.json` contiene proiezioni sintetiche
esplicitamente marcate `fixture_only`. Non pretende di essere un
`aieq-v3-evaluation-unit-index-1`, un manifest ammesso o un package adjudicated
reale. Serve soltanto come oracle hash-bound delle espansioni registry.

Queste proiezioni:

- non possono essere usate da `official_split`;
- non verificano il trusted admission, i file audio o le mask;
- non possono alimentare policy, gate, report scientifici o activation;
- devono essere rifiutate se `instance_purpose` o `claim_plan_purpose` viene
  promosso a scientifico/officiale.

Gli input ufficiali restano bloccati fino a G3/G4.

## 6. Fixture negative

`registry_conformance_mutations_v1.json` congela 31 mutation recipe con target
SHA, JSON pointer, operazione ed esito/reason code atteso. Coprono almeno:

- schema v1 superseded;
- catalogo o subject incompleti;
- output illegale su calibration;
- definition/claim-plan digest mismatch;
- gate/Holm/power incompleti;
- primary root non powered;
- binding diagnostica illegale;
- breakdown non biunivoco o promosso;
- dependency calibrator mancante/errata;
- precondition orphan;
- interpretazione errata del selector relazionale come subgroup concreto;
- promozione del claim plan fixture a `scientific_claim_plan`;
- upstream readiness assente o incoerente;
- sottogenere omesso, inventato o Unicode-normalizzato;
- denominatore elettronico filtrato per eligibility;
- support inheritance dal parent;
- promozione dell'instance fixture a `official_registry_instance`;
- readiness usata come valore metrico.

Le recipe sono negative oracle; non sono documenti registry validi e non
possono essere promosse.

## 7. Validazioni eseguite sui file materializzati

Un consumer check indipendente dal generatore ha verificato:

```text
canonical JSON UTF-8 + singolo LF                    PASS (11/11)
SHA256SUMS                                           PASS (11/11)
definition -> companion schema SHA                  PASS
claim plan -> definition SHA                        PASS
instance -> definition/claim plan SHA               PASS (3/3)
readiness -> calibration instance SHA               PASS
D/F -> upstream calibration/readiness SHA           PASS (2/2)
metric identity + definition tuple uniqueness       PASS (164/164)
catalog family checksum                             PASS
own/diagnostic binding closure                      PASS
definition/claim-plan breakdown bijection           PASS (153/153)
parent selector exact-key projection                PASS
primary root/child power semantics                  PASS
electronic exact-value expansion                    PASS (1071/1071)
2/5 frozen-final denominator                        PASS esatto
mutation IDs unici e fail-closed                    PASS (31/31)
```

Regressione:

```text
Full ml_v3 unittest suite = 641/641 PASS
Runtime                   = 79.242 s
git diff --check          = PASS
protected code diff       = ZERO
runtime/training changes  = ZERO
```

## 8. Non-decisioni

Il package non decide:

- primary gate scientifici o Holm family reale;
- power plan, `n_power`, pilot o effect estimate osservato;
- membership o digest ufficiali;
- esiti metrici o readiness reali;
- subgroup claim elettroniche;
- policy ufficiali;
- validator implementation;
- activation, G1 PASS, runtime o training.

## 9. Stato e next action

```text
Package bytes                    = MATERIALIZZATI, NON ANCORA FIRMATI
Package counter-check            = PRE_SIGNATURE_CLEAN (report separato)
Package freeze commit            = NON ANCORA CREATO
Registry validator/expander code = NON ANCORA AUTORIZZATO
Official registry/policies       = BLOCKED
REV8 SPEC GO                     = NO
```

Next permitted action:

1. counter-check byte-level e semantico del package;
2. freeze atomico dei soli docs/data e ballot;
3. firma esplicita Marco riferita a commit e SHA;
4. post-signature recheck;
5. soltanto dopo, eventuale implementazione candidate-only del
   validator/expander.
