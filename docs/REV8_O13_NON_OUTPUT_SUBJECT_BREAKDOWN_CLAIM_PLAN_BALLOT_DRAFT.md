# REV8 — O-13 — NON-OUTPUT SUBJECT, BREAKDOWN E CLAIM-PLAN BALLOT (O13F_05)

## Stato del documento

```text
Tipo                                = micro-ballot normativo docs-only pre-firma
Data preparazione                   = 2026-08-10
Branch                              = feature/motore-v3-rev8-spec-go
Base commit                         = 1275dbbd29a232dadad1729fa9e5605fba2402ce
Decisione O13F_05                   = NON FIRMATA
Registry companion/data package     = NON MATERIALIZZATO
Registry validator/expander code    = NON AUTORIZZATO
Official registry instance/policies = NON MATERIALIZZABILI
REV8 dispatcher activation          = NO
REV8 SPEC GO                        = NO
```

Questo ballot non modifica formule metriche, floor, soglie, power result,
membership o candidate REV8. Chiude soltanto quattro lacune di
rappresentabilità verificate prima della materializzazione O13F_04.

## 1. Authority consumata

```text
Candidate REV8 SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_03 firma/recheck:
c3dafbb64cd156d020a8a4566d7adc937acefd06
e7bae25e0fe17dc060d931d250473589664d6968

O13F_04 pre-sign/firma/recheck:
019dcdadac0872331a8d885256673cb39ebb0bb0
3d636eff6dc9d4c9891422f5b4b8545a5e785b5b
1275dbbd29a232dadad1729fa9e5605fba2402ce

O13F_04 signed SHA-256:
7202df3db4b461480f9bbdc63d05a82d413e34d4bdab91df2157914686850344
```

O13F_01–04 restano integralmente authority salvo le sole estensioni e
separazioni esplicitamente dichiarate qui.

## 2. Finding normativi

Il report:

```text
docs/REV8_O13F05_REGISTRY_REPRESENTABILITY_AUDIT.md
```

ha verificato quattro blocchi:

1. nessuna metrica può essere pubblicata legalmente su `calibration` per
   ospitare i floor di fit §10.4;
2. `electronic_subgenre` è distinto da `source_family` ma non possiede una
   population/expansion O-13;
3. i breakdown elettronici richiesti non possono diventare claim o metric ID
   normativi data-dependent;
4. primary-gate set, Holm order e power binding non sono ancora decisi e non
   possono essere scelti dal builder della definition.

## 3. Decisione O13F_05-A — calibration-fit subject non metrico

### 3.1 Subject definitions

La registry definition aggiunge l'array:

```text
calibrator_fit_subject_definitions
```

Ogni entry ha exact keys:

```text
calibrator_fit_subject_id
calibrator_family
problem_type
evaluation_scope_id
allowed_split_role
```

Domini:

```text
calibrator_family   = tonal | anomaly
allowed_split_role  = calibration

tonal:
  problem_type      = null

anomaly:
  problem_type      = resonance | harshness | sibilance
```

La prima definition deve enumerare esattamente:

```text
calibrator_fit.tonal.global
calibrator_fit.anomaly.resonance.global
calibrator_fit.anomaly.harshness.global
calibrator_fit.anomaly.sibilance.global
```

Tutti usano `evaluation_scope_id=global`. «Global» significa globale rispetto
a profilo, dominio e sottogenere; non permette di fondere classi anomaly.

### 3.2 Binding di fit

Il registry claim plan aggiunge:

```text
calibrator_fit_binding_blueprints
```

Ogni entry ha exact keys:

```text
binding_id
calibrator_fit_subject_id
split_role
support_template_blueprint_id
precondition_blueprint_ids
```

`split_role` deve essere `calibration`. Ogni subject ha esattamente un
binding. Il binding usa support template e precondizioni per verificare i
numeri §10.4, ma non produce `metric_id`, valore metrico o policy scientifica.
`binding_id` è canonico, unico fra tutti i binding dello stesso kind e non
viene analizzato per ricostruire subject o split.

La registry instance aggiunge:

```text
expanded_calibrator_fit_bindings
```

come proiezione exact-value dei binding applicabili. L'esito osservato non
entra nell'instance: viene calcolato contro il population plan hash-bound e
legato nel digest dei risultati delle precondizioni/readiness già previsto da
O13F_04.

### 3.3 Semantica dell'esito

```text
READY      = tutti i floor e predicati del subject sono soddisfatti
NOT_READY  = supporto insufficiente; nessun fit o fallback
FAIL       = schema, digest, membership o identity incoerente
```

`NOT_READY` non è una metrica e non entra in macro-medie. Rende non
disponibile il calibratore o la componente di classe interessata; ogni
metrica/gate development o final che la richiede diventa N/A/no PASS secondo
il candidate. `FAIL` impedisce il report.

È vietato catalogare `calibration_readiness`, ECE, Brier o AP come output
sullo split `calibration`.

## 4. Decisione O13F_05-B — subject discriminato per le precondizioni

Ogni precondition blueprint e ogni precondition concreta aggiungono una
reference discriminata:

```text
binding_subject_kind
binding_subject_id
```

`binding_subject_kind` appartiene esclusivamente a:

```text
metric_binding
calibrator_fit_binding
benchmark_readiness_binding
```

Regole:

- `metric_binding` risolve a un solo `(metric_id,split_role)`;
- `calibrator_fit_binding` risolve a un solo
  `(calibrator_fit_subject_id,calibration)`;
- `benchmark_readiness_binding` risolve a un solo subject/binding mandatory
  nello stesso split e scope;
- `binding_subject_id` non è testo libero e deve risolversi nella stessa
  definition/claim plan;
- precondizione orphan, extra, duplicate o cross-split: FAIL;
- nessun consumer può reinterpretare un subject non metrico come `metric_id`.

Ogni metric binding e calibrator-fit binding aggiunge quindi un exact-key
`binding_id`; una precondizione referenzia quel valore. Il breakdown-set usa
il proprio `diagnostic_breakdown_blueprint_id`. Gli ID rispettano la grammatica
O13F_03, sono unici dentro il rispettivo kind e non vengono mai parsati per
ricostruire semantica.

Questa discriminazione sostituisce soltanto l'assunzione O13F_03 secondo cui
ogni predicato doveva risolversi a una metrica. I due `predicate_kind` e i
threshold numerici restano invariati.

## 5. Decisione O13F_05-C — electronic-subgenre registry-only

### 5.1 Population ed expansion

L'enumerazione registry-only di `population_kind` aggiunge:

```text
electronic_subgenre_groups
```

L'enumerazione di `expansion_mode` aggiunge:

```text
for_each_electronic_subgenre_in_parent
```

Il selector template è `{}`. Il selector concreto ha exact keys:

```text
electronic_subgenre
```

Il valore conserva esattamente il dominio del manifest firmato:

```text
string | null
```

Il registry confronta il valore JSON per uguaglianza esatta, senza alias o
normalizzazione Unicode implicita. Tutti e soli i valori osservati nel parent
sono espansi, incluso `null` e le stringhe diverse da `techno`, `house` e
`breakbeat`; ometterne o aggiungerne uno produce FAIL. Se asset dello stesso
`group_id` dichiarano valori distinti, il gruppo appartiene a ciascun
breakdown corrispondente ma conta una sola volta nel parent. I breakdown sono
diagnostici; una futura claim deve decidere esplicitamente l'eventuale
overlap tra sottogeneri prima delle prediction.

La population parent elettronica contiene i `group_id` final-test il cui
manifest include `electronic-stratified`. Il floor parent §11.1 usa interi:

```text
5 * n_unique_electronic_parent >= 2 * n_unique_frozen_final_test_groups
```

Il denominatore contiene tutti i `group_id` unici ammessi e congelati nel
ruolo final-test, prima di eligibility o N/A metric-specific. Non è una
`source_family`, una population metricamente definita o la somma di asset/crop.

`electronic_subgenre_groups` è registry/report-only. Non viene proiettato in
`support-floor-policy-3`, la cui enumerazione resta invariata.

### 5.1.1 Readiness della famiglia elettronica

La registry definition aggiunge:

```text
benchmark_readiness_subject_definitions
```

con una entry exact-key:

```text
benchmark_readiness_subject_id
evaluation_scope_id
split_role
mandatory
```

e valore:

```text
benchmark_readiness_subject_id = benchmark_readiness.electronic_stratified
evaluation_scope_id            = benchmark.electronic_stratified
split_role                     = final-test
mandatory                      = true
```

Il claim plan aggiunge `benchmark_readiness_binding_blueprints`; ogni entry
usa exact keys:

```text
binding_id
benchmark_readiness_subject_id
split_role
precondition_blueprint_ids
```

Il subject ha esattamente un binding e deve referenziare la precondizione
`minimum_parent_fraction=2/5`. Esito `NOT_READY` impedisce PASS dell'intero
report final-test, pur lasciando i breakdown disponibili come diagnostica con
reason code. Omissione o declassamento del subject produce FAIL.

### 5.2 Breakdown diagnostici

La registry definition aggiunge:

```text
diagnostic_breakdown_blueprint_definitions
```

Ogni definition ha exact keys:

```text
diagnostic_breakdown_blueprint_id
base_metric_id
parent_evaluation_scope_id
breakdown_evaluation_scope_id
population_kind
expansion_mode
population_selector_template
publication_status
claim_status
```

Per il breakdown elettronico:

```text
breakdown_evaluation_scope_id  = benchmark.electronic_stratified
population_kind              = electronic_subgenre_groups
expansion_mode               = for_each_electronic_subgenre_in_parent
population_selector_template = {}
publication_status           = diagnostic_only
claim_status                 = forbidden_without_signed_claim_plan
```

Il registry claim plan aggiunge:

```text
diagnostic_breakdown_blueprints
precondition_blueprints
```

Ogni entry ha exact keys:

```text
diagnostic_breakdown_blueprint_id
split_role
precondition_blueprint_ids
benchmark_readiness_binding_ids
power_binding_kind
power_binding_id
```

Per il breakdown elettronico:

```text
split_role          = final-test
precondition_blueprint_ids = []
benchmark_readiness_binding_ids =
  [binding del subject benchmark_readiness.electronic_stratified]
power_binding_kind  = null
power_binding_id    = null
```

La prima definition crea una definition per ogni metrica della famiglia madre
che il candidate richiede di riportare in `electronic-stratified`. Il
`parent_evaluation_scope_id` è lo scope della famiglia madre applicabile, non
lo scope elettronico. La popolazione concreta è esattamente:

```text
G_eligible(base_metric_id, parent_evaluation_scope_id)
INTERSECT group_id con benchmark_families contenente electronic-stratified
INTERSECT group_id con almeno un asset avente electronic_subgenre == selector
```

`G_eligible` deriva soltanto dagli input hash-bound e non dagli output. Il
breakdown calcola il proprio `G_defined` dopo l'evaluazione, pubblica entrambe
le population e non prende in prestito definibilità dalla metrica parent.

La matrice di tracciabilità deve mostrare la family madre e la clausola
sorgente. Ogni definition deve risolvere un `base_metric_id` catalogato e
ammesso su final-test; la coppia
`(base_metric_id,parent_evaluation_scope_id)` è unica.

Ogni `diagnostic_breakdown_blueprint_definition` ha esattamente una row
`diagnostic_breakdown_blueprints` nello stesso claim plan, con
`split_role=final-test`, e nessuna row extra. La row deve dipendere dal binding
`benchmark_readiness.electronic_stratified`; omissione, duplicazione o
dependency diversa produce FAIL.

La registry instance aggiunge:

```text
expanded_diagnostic_breakdowns
```

Ogni concrete row ha exact keys:

```text
diagnostic_breakdown_id
diagnostic_breakdown_blueprint_id
base_metric_id
split_role
parent_evaluation_scope_id
breakdown_evaluation_scope_id
population_kind
population_selector
precondition_ids
benchmark_readiness_binding_ids
```

L'ID concreto è:

```text
diagnostic_breakdown_blueprint_id + ".h" +
SHA256(canonical_bytes(population_selector))
```

Il digest è lowercase hex-64 completo. L'ID non viene analizzato. Il risultato
usa formula e N/A della metrica parent ma calcola supporto e aggregazione sulla
sola population concreta; non prende in prestito `G_defined` dal parent.

Un breakdown:

- non è un nuovo `metric_id`;
- non soddisfa gate o Holm;
- non sostiene claim o GO;
- pubblica supporto, esclusioni e status diagnostico;
- resta hash-bound prima delle prediction.

Una futura claim di sottogenere richiede prima delle prediction una nuova
versione firmata della registry definition che introduca `metric_id` ed
`evaluation_scope_id` subgroup distinti, seguita da un claim plan firmato con
il relativo power binding. Nessuna promozione automatica è ammessa.

## 6. Decisione O13F_05-D — registry claim-plan overlay

### 6.1 Motivazione

La metric definition può congelare identità e formule prima dei dati. La
scelta di primary gate, Holm family e power binding deve avvenire dopo il
catalogo ma prima del pilot/prediction. Non appartiene né all'identity catalog
né al power plan osservato.

O13F_04 viene quindi raffinata in tre livelli:

```text
registry definition = catalogo, subject e blueprint statici
registry claim plan = binding metrici, gate/Holm e power-binding statici
registry instance   = espansione per split contro input reali o fixture
```

### 6.2 Claim-plan artifact

Schema:

```text
aieq-v3-rev8-o13-registry-claim-plan-1
```

Top-level exact keys:

```text
schema
contract_revision
source_contract_sha256
registry_definition_sha256
claim_plan_id
claim_plan_purpose
allowed_split_roles
holm_primary_gate_metric_ids
power_design_blueprints
metric_binding_blueprints
metric_stratum_power_binding_blueprints
calibrator_fit_binding_blueprints
calibrator_dependency_blueprints
benchmark_readiness_binding_blueprints
diagnostic_breakdown_blueprints
precondition_blueprints
```

`claim_plan_purpose` appartiene esclusivamente a:

```text
conformance_fixture
scientific_claim_plan
```

`claim_plan_id` rispetta la grammatica ID O13F_03, viene assegnato nel freeze,
non è candidate-controlled e non viene parsato. `allowed_split_roles` deve
essere esattamente la union ordinata per byte UTF-8 degli split presenti in
metric binding, calibrator-fit binding, benchmark-readiness binding e
diagnostic breakdown. Sono ammessi
soltanto `calibration`, `development-metric` e `final-test`; valore extra,
mancante o duplicato produce FAIL.

`holm_primary_gate_metric_ids` è una lista non vuota, senza duplicati e
ordinata nell'enumerazione firmata. Ogni ID risolve esattamente una metrica
normativa con binding sia `development-metric` sia `final-test`. I due report
usano obbligatoriamente lo stesso insieme, enumerazione e power design; una
divergenza per split produce FAIL.

L'ordine firmato è enumerazione canonica e tie-break, non sostituisce
l'algoritmo Holm: il report ordina i p-value osservati in senso crescente e,
soltanto a p-value esattamente uguale, usa l'ordinal dell'entry firmata. Usare
l'ordine ID al posto dell'ordine dei p-value produce FAIL.

Ogni `power_design_blueprint` ha exact keys:

```text
metric_id
power_design_rule_id
```

Esiste esattamente una blueprint per ogni primary-gate metric ID e nessuna per
metriche non primarie. `power_design_rule_id` appartiene esclusivamente a:

```text
curve_error.relative_reduction_0_10_no_clean_regression
macro_or_ap.relative_0_10_or_absolute_0_10_below_baseline_0_10
clean_actionable.binomial_p0_0_02_p1_0_01
false_events.joint_rate_0_50_vs_0_25
```

Ogni ID rinvia alla formula completa già congelata nel candidate §11.2 e ne
deriva statistic, orientation ed effect-size rule; non è un alias testuale
libero. Una metrica non compatibile con uno dei quattro design non può essere
primary gate senza un amendment firmato pre-pilot.

Il futuro power plan calcola `n_power`, viene validato insieme al claim-plan
SHA e deve proiettare exact-value statistic, orientation ed effect-size
derivati dal rule ID; non può sceglierli o modificarli dopo i dati. Il rule ID
resta authority nel claim plan anche se il power-plan schema legacy espone
soltanto i campi derivati. Le
costanti decimali dei rule ID sono razionali esatti (`1/10`, `1/50`, `1/100`,
`1/2`, `1/4`), mai binary64 nel claim plan.

Ogni Holm entry può usare strata con power binding `gate`, `family` o `null`
soltanto come espressamente firmato. Una riga `gate` è ammessa soltanto per un
primary-gate binding e deve avere `power_binding_id == metric_id` della propria
metric binding. Una riga `family` deve risolvere una family power row ammessa
e può appartenere anche a una metrica non primaria; in quel caso non richiede
una power-design blueprint. La power-design blueprint resta obbligatoria per
ogni primary metric anche quando il root è family-bound o alcuni child sono
`null`.

Per ogni primary metric e per entrambi i binding D/F, ogni root stratum del
template (`parent_blueprint_id=null`) deve avere
`power_binding_kind=gate|family`; un primary gate con tutti i root `null`
produce FAIL. Il relativo `n_power` viene proiettato nel `n_required` di ogni
root concreto secondo `max(contract_floor,n_power)`. I child non governanti
possono restare `null` soltanto se la matrice dimostra che il loro floor non è
la numerosità indipendente usata dal test primario.

Ogni `metric_binding_blueprint` conserva gli exact keys O13F_04:

```text
binding_id
metric_id
split_role
mandatory
support_template_blueprint_id
precondition_blueprint_ids
```

Ogni metrica normativa con `own_policy` deve avere tutti e soli i binding
richiesti dal proprio `metric_definition.allowed_split_roles`. Il top-level
`allowed_split_roles` è soltanto la closure degli split coperti dall'artefatto
e non impone `calibration` alle metriche di output. Diagnostici
`none_diagnostic` non entrano nei metric binding.

Ogni `metric_stratum_power_binding_blueprint` ha exact keys:

```text
metric_id
split_role
support_template_blueprint_id
stratum_blueprint_id
power_binding_kind
power_binding_id
```

Per ogni metric binding e per ogni stratum del template referenziato esiste
esattamente una riga. `power_binding_kind` e `power_binding_id` mantengono i
domini e i vincoli O13F_03. Righe extra, mancanti, duplicate, cross-template o
cross-split producono FAIL. Calibrator-fit subject e breakdown diagnostici non
possono avere righe metric-power.

Ogni `calibrator_dependency_blueprint` ha exact keys:

```text
metric_binding_id
calibrator_fit_binding_id
dependency_kind
```

`dependency_kind` è esclusivamente `required_for_evaluation`. La matrice di
tracciabilità deve dichiarare, per ogni metric binding che consuma confidence
calibrata, soglia actionable, bundle o evento estratto dalla calibration
policy, tutti e soli i calibrator-fit binding richiesti. Una dependency extra,
mancante, duplicata, verso altra classe o non risolta produce FAIL.

`NOT_READY` si propaga a N/A/no PASS per i soli metric binding dipendenti; non
può essere ignorato né rendere indisponibile una metrica indipendente. `FAIL`
del subject o della dependency graph impedisce il report.

Il claim plan è firmato e hash-bound prima di:

```text
calcolo del power plan dal development_pilot
apertura delle prediction development-metric
materializzazione di qualunque official registry instance
```

Il power plan deve usare esattamente gate, ordine e family ID del claim plan;
non può aggiungerli o modificarli. Assenza o mismatch produce FAIL.

Un claim plan `conformance_fixture` non costituisce authority scientifica. Il
primo `scientific_claim_plan` resta un artefatto separato da firmare:
O13F_05 non sceglie oggi i gate primari né inventa `n_power`.

### 6.3 Modifiche agli artefatti O13F_04

Poiché cambiano exact-key e referential graph, gli schema ID O13F_04 mai
materializzati non vengono riutilizzati. Gli ID efficaci diventano:

```text
aieq-v3-rev8-o13-registry-definition-2
aieq-v3-rev8-o13-registry-instance-2
```

I letterali `...registry-definition-1` e `...registry-instance-1` restano
provenance O13F_04 superseded-before-materialization e devono essere rifiutati
dal path O13F_05. Nessuna migrazione è necessaria perché non esiste alcuna
istanza v1 materializzata.

Il top-level exact-key sostitutivo della registry definition è:

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
o13f05_authority_commit
o13f05_recheck_commit
population_selector_schema_id
population_selector_schema_sha256
registry_precondition_schema_id
registry_precondition_schema_sha256
metric_definitions
calibrator_fit_subject_definitions
benchmark_readiness_subject_definitions
diagnostic_breakdown_blueprint_definitions
support_template_blueprints
```

Rispetto a O13F_04:

- `binding_blueprints` viene rimosso dal top-level e sostituito dalle
  definition/enum strutturali firmate;
- `support_template_blueprints` resta nella definition come struttura
  riutilizzabile senza scelta del gate;
- `precondition_blueprints` si sposta nel claim plan, perché ogni
  precondizione referenzia un binding subject creato da quel claim plan;
- gli exact keys `power_binding_kind` e `power_binding_id` vengono rimossi
  dagli stratum blueprint statici e trasferiti nelle righe
  `metric_stratum_power_binding_blueprints` del claim plan;
- floor, basis, parent e ceiling restano immutati nella definition;
- le scelte concrete per metric binding, gate/Holm e power binding
  appartengono al claim plan;
- `calibrator_fit_subject_definitions` e
  `benchmark_readiness_subject_definitions` e
  `diagnostic_breakdown_blueprint_definitions` appartengono alla definition;
- i concrete calibration binding e diagnostic breakdown blueprint
  appartengono al claim plan perché determinano conseguenze e scope.

Ogni stratum di `support_template_blueprints` usa quindi l'exact-key
sostitutivo:

```text
stratum_blueprint_id
population_kind
expansion_mode
population_selector_template
parent_blueprint_id
support_basis
contract_floor
max_parent_fraction_numerator
max_parent_fraction_denominator
```

Il claim plan deve risolvere ogni precondition, binding e power row soltanto
contro ID presenti nella stessa claim plan e nella definition referenziata.

Il top-level exact-key sostitutivo della registry instance è:

```text
schema
instance_purpose
registry_definition_sha256
registry_claim_plan_sha256
split_role
evaluation_unit_index_id
evaluation_unit_index_sha256
asset_manifest_sha256
annotation_package_sha256
frontend_contract_sha256
upstream_readiness_dependencies
expanded_support_templates
expanded_bindings
expanded_calibrator_fit_bindings
expanded_calibrator_dependencies
expanded_benchmark_readiness_bindings
expanded_diagnostic_breakdowns
expanded_metric_stratum_power_bindings
expanded_preconditions
```

e ogni espansione deve essere la proiezione esatta della coppia
definition+claim plan. Definition o claim-plan digest mancanti/mismatch: FAIL.

Ogni entry di `expanded_support_templates` ha exact keys:

```text
support_template_id
strata
```

Ogni strato concreto v2 ha exact keys:

```text
stratum_id
stratum_blueprint_id
population_kind
population_selector
parent_stratum_id
support_basis
contract_floor
max_parent_fraction_numerator
max_parent_fraction_denominator
```

Gli strati concreti v2 non contengono power fields. Ogni entry di
`expanded_metric_stratum_power_bindings` li aggiunge con exact keys:

```text
metric_id
split_role
support_template_id
stratum_blueprint_id
stratum_id
power_binding_kind
power_binding_id
```

Il join è esatto su
`(support_template_id,stratum_blueprint_id,stratum_id)`; una riga power non
può alterare kind, selector, floor, basis, parent o ceiling.

Ogni entry di `expanded_bindings` ha exact keys:

```text
binding_id
metric_id
split_role
mandatory
support_template_id
precondition_ids
```

Ogni entry di `expanded_calibrator_fit_bindings` ha exact keys:

```text
binding_id
calibrator_fit_subject_id
split_role
support_template_id
precondition_ids
```

Ogni entry di `expanded_benchmark_readiness_bindings` ha exact keys:

```text
binding_id
benchmark_readiness_subject_id
split_role
precondition_ids
```

Ogni entry di `expanded_calibrator_dependencies` ha exact keys:

```text
metric_binding_id
calibrator_fit_binding_id
dependency_kind
```

Le dependency vengono espanse nell'instance dello split del metric binding,
mai copiate nell'instance `calibration`. Il fit binding target viene risolto
nel claim plan e nella precedente instance calibration hash-bound.

`upstream_readiness_dependencies` è una lista di entry exact-key:

```text
source_split_role
registry_instance_sha256
readiness_results_sha256
```

L'instance `calibration` usa lista vuota. Una instance development/final con
almeno una calibrator dependency deve avere esattamente una entry con
`source_split_role=calibration`, digest lowercase hex-64 della calibration
instance e dei suoi readiness results. I purpose devono coincidere:

```text
conformance_fixture -> conformance_fixture
official_split      -> official_split
```

Una instance senza
calibrator dependency usa lista vuota. Copiare membership calibration dentro
lo split consumer è vietato.

Il report development/final deve ripubblicare e verificare gli stessi due
digest. Fit binding o risultato mancante, extra, di altra claim plan, classe o
calibration instance produce FAIL; `NOT_READY` si propaga come definito in
§6.2.

### 6.4 Ordine canonico degli array

Nel creation path gli array sono ordinati come segue; nei committed bytes un
ordine diverso produce FAIL, non riordino silenzioso:

```text
definition.metric_definitions
  -> metric_id, byte UTF-8
definition.calibrator_fit_subject_definitions
  -> calibrator_fit_subject_id, byte UTF-8
definition.benchmark_readiness_subject_definitions
  -> benchmark_readiness_subject_id, byte UTF-8
definition.diagnostic_breakdown_blueprint_definitions
  -> diagnostic_breakdown_blueprint_id, byte UTF-8
definition.support_template_blueprints
  -> support_template_blueprint_id, byte UTF-8
definition.support_template_blueprints[].stratum_blueprints
  -> stratum_blueprint_id, byte UTF-8

claim_plan.allowed_split_roles
  -> valore ruolo, byte UTF-8
claim_plan.holm_primary_gate_metric_ids
  -> enumerazione firmata; non UTF-8 sort
claim_plan.power_design_blueprints
  -> metric_id, byte UTF-8
claim_plan.metric_binding_blueprints
  -> (metric_id, split_role), byte UTF-8
claim_plan.metric_stratum_power_binding_blueprints
  -> (metric_id, split_role, support_template_blueprint_id,
      stratum_blueprint_id), byte UTF-8
claim_plan.calibrator_fit_binding_blueprints
  -> (calibrator_fit_subject_id, split_role), byte UTF-8
claim_plan.calibrator_dependency_blueprints
  -> (metric_binding_id, calibrator_fit_binding_id), byte UTF-8
claim_plan.benchmark_readiness_binding_blueprints
  -> (benchmark_readiness_subject_id, split_role), byte UTF-8
claim_plan.diagnostic_breakdown_blueprints
  -> (diagnostic_breakdown_blueprint_id, split_role), byte UTF-8
claim_plan.precondition_blueprints
  -> precondition_blueprint_id, byte UTF-8

instance.expanded_support_templates
  -> support_template_id, byte UTF-8
instance.upstream_readiness_dependencies
  -> source_split_role, byte UTF-8
instance.expanded_bindings
  -> (metric_id, split_role), byte UTF-8
instance.expanded_metric_stratum_power_bindings
  -> (metric_id, split_role, support_template_id,
      stratum_blueprint_id, stratum_id), byte UTF-8
instance.expanded_calibrator_fit_bindings
  -> (calibrator_fit_subject_id, split_role), byte UTF-8
instance.expanded_calibrator_dependencies
  -> (metric_binding_id, calibrator_fit_binding_id), byte UTF-8
instance.expanded_benchmark_readiness_bindings
  -> (benchmark_readiness_subject_id, split_role), byte UTF-8
instance.expanded_diagnostic_breakdowns
  -> diagnostic_breakdown_id, byte UTF-8
instance.expanded_preconditions
  -> precondition_id, byte UTF-8
```

Ogni lista `precondition_blueprint_ids` e ogni lista concreta
`precondition_ids`, in metric binding, calibrator-fit binding,
benchmark-readiness binding e diagnostic breakdown, è senza duplicati e
ordinata per byte UTF-8 dell'ID. Un committed order diverso produce FAIL.
La stessa regola vale per ogni lista `benchmark_readiness_binding_ids`.

## 7. Decisione O13F_05-E — interpretazione del catalogo base

Per impedire al builder di scegliere silenziosamente le espansioni, si
congelano gli insiemi:

```text
P8 = resonance, muddiness, boominess, thinness,
     boxy_midrange, dull_sound, harshness, sibilance
P3 = resonance, harshness, sibilance
D/F = development-metric, final-test

M/own  = mandatory_normative / own_policy
D/none = diagnostic_only / none_diagnostic
```

Il catalogo base contiene esattamente questa matrice semantica, prima dello
spelling degli ID:

| Misura | Record/type expansion | `evaluation_scope_id` | Stato | Split | Righe |
|---|---|---|---|---|---:|
| weighted MAE curva | `null/null` | `global` | M/own | D/F | 1 |
| weighted RMSE curva | `null/null` | `benchmark.tonal_controlled`, `benchmark.tonal_natural` | M/own | D/F | 2 |
| p95 errore assoluto | `null/null` | stessi due scope tonali | D/none | D/F | 2 |
| residual improvement; sign error | `null/null` | `benchmark.tonal_controlled` | M/own | D/F | 2 |
| clean actionable rate | `null/null` | `benchmark.tonal_natural`, `benchmark.clean_safety` | M/own | D/F | 2 |
| coverage_minus | `semantic_region/null` | `benchmark.tonal_natural` | M/own | D/F | 1 |
| coverage_plus | `semantic_region/null` | `benchmark.tonal_natural` | D/none | D/F | 1 |
| severity MAE; severity Spearman | `semantic_region × P8` | `benchmark.tonal_natural` | M/own | D/F | 16 |
| TP, FP, FN, precision, recall, F1 | `semantic_region × P8` | `benchmark.tonal_natural` | M/own | D/F | 48 |
| onset; offset | `semantic_region × P8` | `benchmark.tonal_natural` | M/own | D/F | 16 |
| Resonance center error | `semantic_region × resonance` | `benchmark.tonal_natural` | M/own | D/F | 1 |
| duration; occupancy | `semantic_region × P8` | `benchmark.tonal_natural` | D/none | D/F | 16 |
| TP, FP, FN, precision, recall, F1 | `dynamic_event × P3` | `benchmark.anomaly_natural` | M/own | D/F | 18 |
| severity MAE; onset; offset; severity Spearman | `dynamic_event × P3` | `benchmark.anomaly_natural` | M/own | D/F | 12 |
| Resonance center error | `dynamic_event × resonance` | `benchmark.anomaly_natural` | M/own | D/F | 1 |
| Average Precision | `dynamic_event × P3` | `benchmark.anomaly_natural` | M/own | D/F | 3 |
| false events/min | `dynamic_event × P3` | `benchmark.anomaly_natural`, `benchmark.clean_safety` | M/own | D/F | 6 |
| duration; occupancy | `dynamic_event × P3` | `benchmark.anomaly_natural` | D/none | D/F | 6 |
| ECE; Brier tonal | `null/null` | `global` | M/own | D/F | 2 |
| ECE; Brier anomaly dense | `null × P3` | `global` | M/own | D/F | 6 |
| adapter macro-F1; adapter clean FP group rate | `null/null` | `global` | M/own | D/F | 2 |

Checksum obbligatorio:

```text
tonal_curve/global+family = 7
tonal/semantic_region     = 101
dynamic_event             = 46
calibration outputs       = 8
homologous adapter        = 2
TOTAL                     = 164
```

Ogni definition usa esattamente `allowed_split_roles=[development-metric,
final-test]` nell'ordine byte UTF-8 canonico. Nessun output ammette
`calibration`; i breakdown elettronici final-only restano fuori dalle 164
definition.

ECE e Brier sono quindi `mandatory_normative/own_policy`: possono pubblicare
un valore con supporto maggiore di zero, ma non soddisfano prova o gate finché
i floor candidate non sono raggiunti. P95, coverage_plus, durata e occupancy
sono gli unici `diagnostic_only/none_diagnostic` congelati da questa matrice.

Lo spelling dei `metric_id`, le `definition_key` e la matrice concreta sono
firmati nel primo package definition. Conteggio, status, scope, record/type e
split devono proiettare esattamente la matrice; ogni scostamento produce FAIL.

## 8. Hash binding e futuro envelope

La catena ufficiale diventa:

```text
expected registry definition SHA
expected registry claim-plan SHA
expected registry instance SHA
registry precondition/readiness results SHA
upstream calibration registry-instance SHA, per consumer dipendente
upstream calibration readiness-results SHA, per consumer dipendente
expected support-floor policy SHA, quando applicabile
population plan SHA
power plan SHA, quando applicabile
```

Il digest dei risultati deve includere subject kind, subject ID, split, scope,
support counts, precondition outcomes e status. Un risultato metric binding
non può essere sostituito da un calibrator/breakdown result o viceversa.
I due digest upstream devono coincidere con
`instance.upstream_readiness_dependencies` e con il report calibration
precedente; non sono auto-dichiarati dal report consumer.

Nessun campo viene aggiunto oggi a `support-floor-policy-3` o al dispatcher
REV7. Il futuro report/activation envelope resta bloccato finché questa catena
non è materializzata e firmata.

## 9. Fixture e mutation obbligatorie

### Calibration

- ECE/Brier/AP catalogata su `calibration` → FAIL;
- calibrator subject mancante, extra o duplicato → FAIL;
- anomaly class scambiata fra subject → FAIL;
- supporto insufficiente → `NOT_READY`, nessun fit/fallback;
- `NOT_READY` trattato come valore metrico zero o PASS → mutation killed;
- precondition calibration legata a metric binding dev/final → FAIL.
- dependency calibrator→metric mancante, extra o di classe errata → FAIL;
- dependency espansa in calibration anziché nello split consumer → FAIL;
- consumer senza calibration-instance/readiness SHA upstream → FAIL;
- purpose upstream diverso dal purpose consumer → FAIL;
- `NOT_READY` non propagato a un metric binding dipendente → mutation killed;
- `NOT_READY` propagato a un binding indipendente → mutation killed.

### Electronic breakdown

- `source_family` usata al posto di `electronic_subgenre` → FAIL;
- sottogenere osservato non espanso → FAIL;
- sottogenere extra o hardcoded non osservato → FAIL;
- stesso group con due stringhe distinte → entrambe le righe, una sola unità
  nel parent;
- valore `null` osservato → riga `null`, mai omissione silenziosa;
- stringhe Unicode canonically equivalent ma byte-distinte → breakdown e
  digest distinti, nessuna normalizzazione implicita;
- breakdown promosso a gate/claim/power row → FAIL;
- supporto preso in prestito dalla metrica parent → FAIL;
- population breakdown non uguale all'intersezione firmata → FAIL;
- breakdown definition senza esattamente una claim-plan row final-test → FAIL;
- base metric assente/non ammessa su final-test o coppia base/scope duplicata →
  FAIL;
- `5*n_electronic == 2*n_frozen_final` → precondizione soddisfatta;
- escludere gruppi final-test dal denominatore per N/A/eligibility → FAIL;
- appena sotto il confine → N/A/no PASS;
- readiness elettronica mancante/declassata o `NOT_READY` ignorato → FAIL;
- sottogenere non hardcoded valido → riga presente e digest differente.

### Claim plan

- power plan con gate extra/mancante/riordinato → FAIL;
- power plan gate set/enumerazione diverso da
  `holm_primary_gate_metric_ids` → FAIL;
- primary metric priva di binding D o F, o set primario divergente per report
  → FAIL;
- Holm applicata in ordine ID anziché per p-value crescente → mutation killed;
- power design mancante/extra o cambiato dopo il pilot → FAIL;
- power design rule non compatibile con la misura → FAIL;
- primary statistical gate senza root governing stratum gate/family-bound →
  FAIL;
- `n_power` calcolato ma non proiettato nel `n_required` del root → FAIL;
- definition/claim-plan digest mismatch → FAIL;
- official instance senza claim plan firmato → FAIL;
- claim plan creato dopo prediction → FAIL;
- conformance claim plan promosso ad authority → FAIL;
- registry definition/instance schema v1 nel path O13F_05 → FAIL;
- metrica normativa senza binding richiesto → FAIL;
- diagnostico `none_diagnostic` con metric binding → FAIL.
- power-binding row missing/extra/duplicate per uno strato metrico → FAIL;
- power-binding row applicata a calibrator o breakdown → FAIL.
- precondition blueprint extra/mancante/riordinato → FAIL;

### Regressione

- suite REV8 completa invariata;
- protected diff zero verso `Source`, `CMakeLists.txt`, `Resources`,
  `ml_v2`, `ml`, `training` e dispatcher REV7;
- nessuna policy, prediction, runtime, training o push.

## 10. Sequencing dopo firma e recheck

Dopo un post-signature recheck `CLEAN` di O13F_05 è autorizzata soltanto una
tranche docs/data pre-implementazione:

1. companion selector schema;
2. companion precondition/subject schema;
3. registry definition con catalogo e subject definitions;
4. conformance claim plan con gate/power fittizi esplicitamente fixture;
5. conformance registry instance positive/negative;
6. matrice completa candidate → metric/scope/split/subject;
7. freeze, counter-check, firma e post-signature recheck del package.

Il claim plan scientifico, le official instance, i population plan e le
policy ufficiali restano bloccati fino ai rispettivi input e firma. Il codice
validator/expander resta bloccato fino al post-signature `CLEAN` del package
data/schema.

## 11. Non-decisioni

O13F_05 non decide:

- spelling finale dei 164 metric ID;
- gate primari o Holm order scientifici;
- `n_power` o effect size;
- membership, sottogeneri realmente osservati o digest split;
- esiti readiness/metrici;
- claim elettroniche;
- policy ufficiali;
- activation o REV8 SPEC GO.

## 12. Stop rule

Fino a firma e post-signature recheck O13F_05:

```text
registry companion/data package   = NON AUTORIZZATO
claim plan artifact               = NON AUTORIZZATO
registry validator/expander code  = NON AUTORIZZATO
official registry instance        = NON MATERIALIZZABILE
official policies                 = NON MATERIALIZZABILI
REV8 dispatcher                   = INVARIATO
REV8 SPEC GO                      = NO
G1c close / G1 PASS               = NO
runtime / training / push         = NON AUTORIZZATI
```

## 13. Ballot

```text
Decisione O13F_05: NON FIRMATA
Firma/nome:
Data:

SHA-256 ballot pre-firma:
PENDING FREEZE

Commit ballot pre-firma:
PENDING FREEZE
```

Opzioni ammesse:

```text
APPROVO
RESPINGO
RICHIEDO MODIFICHE (con motivazione testuale)
```
