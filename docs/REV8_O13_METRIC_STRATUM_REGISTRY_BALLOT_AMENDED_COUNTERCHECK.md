# REV8 — O13F_03 — AMENDED PRE-SIGNATURE COUNTER-CHECK

## Verdetto

```text
Verdetto                         = AMENDED_PRE_SIGNATURE_CLEAN
Data                             = 2026-08-10
Branch                           = feature/motore-v3-rev8-spec-go
Amendment base HEAD              = 3c2bf9aad616f1e5b8959509cac91aed3545367c
Ballot                           = REV8_O13_METRIC_STRATUM_REGISTRY_BALLOT_DRAFT.md
Ballot SHA-256                   = 23a31daa99a78510c6b666a94749f590853faeb38064aee6cf06a40c99bddfa5
Decisione O13F_03                = NON FIRMATA
Registry data artifact           = NON MATERIALIZZATO
Official support-floor policies  = NON MATERIALIZZATE
REV8 SPEC GO                     = NO
```

Questo report sostituisce, per il solo ballot con SHA sopra indicato, il
precedente:

```text
docs/REV8_O13_METRIC_STRATUM_REGISTRY_BALLOT_COUNTERCHECK.md
pre-sign freeze commit 6ce33f9a30af23fd7c6b08ad5bcf7a61ac5d113d
old ballot SHA 2c6c8c37a1b8814b914ff11b59196122135678ee106d12ea3b9d2093cc217dd6
```

Il report precedente resta evidenza storica append-only, ma non deve essere
usato come counter-check del ballot emendato. Il verdetto corrente riguarda
soltanto l'architettura proposta; non sostituisce la firma dell'autorità, non
approva futuri metric ID o dati e non autorizza implementazione o activation.

## 1. Authority e immutabilità

### 1.1 Candidate

Il candidate consumato dal ballot è stato letto dal worktree e produce:

```text
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
SHA-256 398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745
```

Il digest coincide con quello dichiarato nel ballot.

### 1.2 Authority O13F_01 e O13F_02

Le commit richiamate esistono:

```text
59bec34856a08aa43cc52bb123e904a44575f4f6  O13F_01 firma
d0b9916cd08aff29dfde03ffe0c8f38c3656a96c  O13F_01 recheck
26f35e753f96ebc48d0453e432e8f2a3f5380687  O13F_02 firma
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e  O13F_02 recheck
2b57493917ead1074de8b90da5907790564cc34d  O13F_02 implementation
468f7e3212d386c916642f7a292d63af7e1d8ad0  implementation report
```

I due ballot firmati correnti sono byte-identici ai rispettivi commit di
firma:

```text
O13F_01 SHA-256 3b73e9b43e95140bb0e33a938afbedda57a907f49c3f67220d38b923363d0d14
O13F_02 SHA-256 4021353cac2f2a1b469b1e889508a7296bbac8e9f656b2b15511261ea6c5fcc7
```

Il primo emendamento conteneva una trascrizione errata del full hash del
recheck O13F_01; il valore è stato ricalcolato con `git rev-parse` e corretto
prima del presente freeze.

## 2. Finding emersi e correzioni applicate

### 2.1 Support inheritance rimosso

Il vecchio ballot permetteva:

```text
support_mode = inherit
```

Questa regola era fail-open: due metriche calcolate sulla stessa partizione
possono avere `G_defined` differente. Precision, recall, F1, severity,
onset/offset e Spearman non possono prendere in prestito supporto o N/A da un
altro output.

Il ballot emendato ammette soltanto:

```text
own_policy
none_diagnostic
```

Ogni output normativo mantiene policy, `SupportAccounting`, `G_defined`, N/A e
binding propri. Un diagnostico senza floor mantiene accounting proprio ma non
può soddisfare gate o claim.

### 2.2 TP/FP/FN e precision/recall/F1

Il vecchio ballot permetteva di trattarli come derivati senza policy. Il
candidate li classifica fra le metriche obbligatorie. Il ballot emendato li
richiede `mandatory_normative`, con policy separata per metrica e split anche
quando il template è condiviso.

### 2.3 Identità semantica e scope

L'identità del metric catalog ora include:

```text
definition_key
evaluation_scope_id
record_family
problem_type
```

La tupla completa è unica. Questo impedisce sia alias della stessa metrica sia
collisioni false fra la stessa formula applicata a family benchmark diverse.
`record_family=null` non viene più confuso con scope globale: significa
soltanto che l'output non è definito sulle famiglie di record del matcher.

### 2.4 Selector e proiezione completa

`population_kind` da solo non identifica profilo, source family, classe o cella
regione×direzione. Il ballot ora richiede `population_selector` registry-only,
exact-key e discriminato, con companion schema firmato insieme al primo data
artifact.

Le population devono essere derivate dagli input hash-bound dello split. Un
population plan costruito liberamente dal caller non costituisce il path
ufficiale. Omettere una family, un profilo, una cella o una classe richiesta
produce FAIL.

### 2.5 Binding agli input dello split

Ogni split del registry è legato a:

```text
evaluation_unit_index_id
evaluation_unit_index_sha256
asset_manifest_sha256
annotation_package_sha256
frontend_contract_sha256
```

Il registry viene materializzato dopo il freeze degli input e prima delle
prediction. Non può essere rigenerato dopo aver osservato N/A o gate.

### 2.6 Vincoli relazionali non esprimibili dalla policy v3

Sono stati verificati due requisiti del candidate che un insieme di soli floor
indipendenti e ceiling massimi non può rappresentare fedelmente:

```text
almeno 3 source_family con almeno 5 gruppi ciascuna
electronic-stratified almeno 2/5 del parent applicabile
```

Imporre cinque gruppi a ogni family sarebbe più restrittivo del candidate;
scegliere liberamente tre family sarebbe candidate-controlled. La policy v3
non contiene inoltre un minimo frazionario del parent.

Il ballot introduce quindi precondizioni registry-only hash-bound con due soli
predicate kind:

```text
minimum_qualifying_partitions
minimum_parent_fraction
```

Esse vengono valutate prima della proiezione nella policy v3 e legate al futuro
report envelope. Non aggiungono campi a `support-floor-policy-3` e non
autorizzano uno schema policy v4.

### 2.7 Compatibilità policy v3 e activation

Il path ufficiale ora richiede:

```text
registry + expected_registry_sha256
input/selector/precondition validation
exact registry projection
support-floor-policy-3 compiler
expected_policy_sha256
```

Il compiler che riceve template caller-supplied resta una primitiva candidate,
non authority. `POLICY_REVISION` dovrà includere firma e recheck O13F_03. Se il
futuro report/activation envelope non lega `registry_sha256`, l'activation
resta bloccata.

## 3. Copertura semantica

L'inventario del ballot copre le famiglie del candidate:

- curva tonale;
- semantic bundle e coverage;
- cardinalità e qualità del matching;
- severity, onset/offset, Spearman e center error;
- Average Precision;
- false events/min;
- ECE e Brier;
- adapter omologo;
- diagnostici p95, `coverage_plus`, duration/occupancy.

Il p95 resta diagnostico secondo O13F_01. `coverage_plus` e
duration/occupancy restano diagnostici. L'alias `PR-AUC` non crea un secondo
metric ID. Il supporto interno Spearman `n>=10` non diventa un floor di dieci
`group_id`.

Il ballot non materializza ancora spelling, espansione class-specific,
template, floor row, power binding o population membership: questi dati
restano correttamente bloccati fino al primo registry data artifact firmato.

## 4. Controlli eseguiti

### 4.1 Test

```text
Targeted policy compiler suite: 21/21 PASS
Full ml_v3 unittest suite:       641/641 PASS
```

Comandi:

```text
PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest \
  ml_v3.tests.test_g1c_rev8_support_floor_policy_compiler -v

PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest discover \
  -s ml_v3/tests -t . -p 'test_*.py' -q
```

### 4.2 Static e governance

```text
candidate SHA verificato                                      PASS
O13F_01 current vs signed commit byte-identical               PASS
O13F_02 current vs signed commit byte-identical               PASS
commit authority/recheck risolte con git                      PASS
git diff --check                                               PASS
Markdown fences bilanciate                                     PASS
support inheritance / support_parent_metric_id assenti         PASS
una sola decisione O13F_03, ancora NON FIRMATA                 PASS
policy schema v3 preservato; nessuna policy v4                 PASS
registry e official policies non materializzati                PASS
REV8 SPEC GO = NO                                              PASS
protected diff Source/CMake/Resources/ml_v2/ml/training        ZERO
diff codice ml_v3                                              ZERO
```

## 5. Residui dichiarati

Non sono residui nascosti del ballot, ma deliverable successivi ancora
deliberatamente non autorizzati:

1. lista completa e spelling dei `metric_id`;
2. allowlist dei `definition_key` e matrice candidate-clause → metric ID;
3. selector exact-key companion schema;
4. precondition exact-key companion schema;
5. espansione per scope, record family e problem type;
6. binding metric×split×template×precondition;
7. righe complete degli strata e valori contrattuali già derivati;
8. evaluation-unit index reali e relativi digest;
9. power plan reale e `n_power`;
10. population plan e policy ufficiali;
11. report/activation envelope con `registry_sha256`;
12. implementazione e mutation test del registry path ufficiale.

O13F_03 può approvare l'architettura senza approvare questi valori. Firma e
recheck O13F_03 non autorizzano il builder a inventarli: companion schema,
catalogo e primo data artifact costituiscono una successiva unità di freeze e
firma.

## 6. Next permitted action

```text
1. Commit docs-only del ballot emendato e di questo counter-check.
2. Verifica del commit immutabile e dei due SHA.
3. Firma esplicita dell'autorità sul ballot emendato.
4. Post-signature recheck read-only.
5. Solo dopo: companion schema + registry data artifact + trace matrix.
```

Prima della firma restano vietati:

```text
schema/code del registry
metric ID trattati come authority
population o power plan ufficiali
support policy ufficiali
dispatcher switch
REV8 SPEC GO
training/runtime/push
```
