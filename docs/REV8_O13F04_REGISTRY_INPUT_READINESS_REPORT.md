# REV8 — O13F_04 — REGISTRY INPUT READINESS AUDIT

## Verdetto

```text
Verdetto                           = OFFICIAL_REGISTRY_INSTANCE_BLOCKED
Data                               = 2026-08-10
Branch                             = feature/motore-v3-rev8-spec-go
Base HEAD                          = e7bae25e0fe17dc060d931d250473589664d6968
O13F_03 authority                  = SIGNED + POST_SIGNATURE_CLEAN
Static registry definition         = NON MATERIALIZZATA
Official split registry instance   = NON MATERIALIZZABILE OGGI
Official support-floor policies    = NON MATERIALIZZABILI OGGI
REV8 SPEC GO                       = NO
```

## 1. Scopo

Questo audit verifica se esistono gli input necessari per eseguire il next
permitted action dichiarato dal recheck O13F_03 senza promuovere fixture o
inventare digest.

Sono stati cercati nel repository, negli altri worktree del progetto e in:

```text
/Users/marco/aieq_data/motore_v3
```

artefatti contenenti o dichiaranti:

```text
aieq-v3-evaluation-unit-index-1
evaluation_unit_index_sha256
annotation_package_sha256
aieq-v3-benchmark-power-plan-1
pilot_sha256
```

## 2. Evidenza trovata

### 2.1 Evaluation-unit index

Non è presente alcuna istanza materializzata di:

```text
aieq-v3-evaluation-unit-index-1
```

per:

```text
calibration
development-metric
final-test
```

Il literal compare soltanto nel candidate e nel ballot architetturale. Non
esistono quindi digest reali per:

```text
evaluation_unit_index_id
evaluation_unit_index_sha256
asset_manifest_sha256
annotation_package_sha256
frontend_contract_sha256
```

da inserire in un'istanza ufficiale del registry.

### 2.2 Power plan

L'unico power plan presente è:

```text
ml_v3/fixtures/g1/examples/benchmark_power_plan.json
SHA-256 28a623579e90c9aeee0c3f7f66d02d6d126daed759b230f406b9cc054c2c8a5a
```

Il file dichiara:

```text
pilot_sha256 = aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
support.notes = fixture
```

I suoi metric ID e `n_power` sono test data, non authority.

### 2.3 Manifest e annotazioni

Sono presenti soltanto gli esempi G1a:

```text
ml_v3/fixtures/g1/examples/asset_manifest.json
ml_v3/fixtures/g1/examples/annotation.json
```

Non costituiscono manifest ammessi o package adjudicated completi per uno dei
tre split O-13.

## 3. Blocco architetturale emerso

Il ballot O13F_03 firmato definisce un unico artefatto registry contenente sia:

- catalogo e template scientifici statici;
- binding agli input e righe data-dependent dello split.

Di conseguenza il primo freeze di catalogo/schema richiederebbe già corpus,
annotation package, evaluation-unit index e power evidence reali.

Questo confligge con l'ordine di progetto già congelato:

```text
G1c = implementazione e falsificazione dell'evaluator
G3  = corpus e target reali
```

Il candidate REV8, §15, dichiara inoltre che la disponibilità del corpus reale
viene verificata in G3/G4 e non deve bloccare l'implementazione G1. Nel formato
monolitico O13F_03, invece, l'implementazione ufficiale del registry path resta
bloccata fino a G3.

## 4. Alternative respinte

### Promuovere gli esempi G1a

`REJECTED`: produrrebbe falsi digest e `n_power` non derivati da pilot reale.

### Inventare evaluation-unit index vuoti o sintetici come official split

`REJECTED`: trasformerebbe fixture di conformance in authority scientifica.

### Omettere `split_input_bindings`

`REJECTED`: violerebbe direttamente O13F_03 e riaprirebbe la possibilità di
omettere source family o strata sfavorevoli.

### Implementare prima e definire il registry dopo

`REJECTED`: lascerebbe al builder selector, expansion e namespace byte-level.

## 5. Correzione proposta

Separare append-only:

```text
registry definition  = authority statica, congelabile prima di G3
registry instance    = binding a uno split reale, congelabile solo con input reali
```

La definition deve congelare metric catalog, selector/precondition schema,
template blueprint e binding blueprint. L'instance deve espandere i blueprint
contro un evaluation-unit index hash-bound prima delle prediction.

Fixture instance esplicitamente marcate `conformance_fixture` possono
falsificare il validator ma non diventano policy ufficiali. Soltanto una
instance `official_split`, con digest atteso esterno e input reali, può
alimentare il compiler policy v3.

Questa separazione preserva tutte le guardie O13F_03 e rimuove soltanto la
dipendenza circolare G1c→G3.

## 6. Stato e next action

```text
O13F_03 signed text             = INVARIATO
O13F_04 amendment ballot        = NECESSARIO PRIMA DI NUOVI ARTEFATTI
Registry code                   = NON AUTORIZZATO
Conformance fixture             = NON ANCORA AUTORIZZATA
Official registry instance      = BLOCKED DA INPUT REALI
Official policies               = BLOCKED
```

Next permitted action: preparare un micro-ballot O13F_04 docs-only che congeli
la separazione definition/instance senza scegliere metric ID, popolazioni,
`n_power` o digest di split.
