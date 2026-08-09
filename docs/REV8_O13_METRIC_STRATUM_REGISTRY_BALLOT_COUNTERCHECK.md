# REV8 — O13F_03 — PRE-SIGNATURE COUNTER-CHECK REPORT

## Verdetto

```text
Verdetto                         = PRE_SIGNATURE_CLEAN
Data                             = 2026-08-09
Branch                           = feature/motore-v3-rev8-spec-go
Base HEAD                        = 468f7e3212d386c916642f7a292d63af7e1d8ad0
Ballot                           = REV8_O13_METRIC_STRATUM_REGISTRY_BALLOT_DRAFT.md
Ballot SHA-256                   = 2c6c8c37a1b8814b914ff11b59196122135678ee106d12ea3b9d2093cc217dd6
Decisione O13F_03                = NON FIRMATA
Registry data artifact           = NON MATERIALIZZATO
Official support-floor policies  = NON MATERIALIZZATE
REV8 SPEC GO                     = NO
```

Il verdetto riguarda esclusivamente la coerenza del ballot pre-firma. Non
approva le future righe del registry, non sostituisce la firma dell'autorità e
non autorizza implementazione, policy ufficiali o activation.

## 1. Lente autorità e semantica

### 1.1 Catena verificata

Il candidate consumato dal ballot ha digest:

```text
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745
```

Le commit authority richiamate dal ballot esistono nel repository:

```text
59bec34856a08aa43cc52bb123e904a44575f4f6  O13F_01 firma
26f35e753f96ebc48d0453e432e8f2a3f5380687  O13F_02 firma
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e  O13F_02 recheck
2b57493917ead1074de8b90da5907790564cc34d  O13F_02 implementation
468f7e3212d386c916642f7a292d63af7e1d8ad0  implementation report
```

### 1.2 Finding confermato

Il candidate enumera metriche e floor in prosa, ma non contiene una lista
byte-authoritative completa di `metric_id` e binding
`metric_id × split_role × stratum_id`.

Gli ID presenti nelle fixture/test non colmano il vuoto. È stato verificato
che il solo power plan disponibile in:

```text
ml_v3/fixtures/g1/examples/benchmark_power_plan.json
```

contiene:

```text
support.notes = "fixture"
pilot_sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
```

Quindi non può essere fonte di ID o `n_power` ufficiali.

### 1.3 Inventario semantico

Il ballot conserva nel proprio inventario minimo tutte le famiglie dichiarate
nelle sezioni 10.1–10.5 del candidate:

- curva tonale;
- clean actionable rate e coverage;
- severity semantic bundle;
- cardinalità e qualità del matching;
- envelope severity/onset/offset;
- Spearman;
- errore centro Resonance;
- Average Precision;
- false events/min;
- ECE/Brier;
- metriche adapter omologhe;
- diagnostici p95, coverage_plus e duration/occupancy.

Il ballot non assegna ID a queste voci e non decide quali gate entrano nella
procedura Holm. È il comportamento corretto prima della firma dell'architettura
e della successiva matrice di tracciabilità.

### 1.4 Nessun drift scientifico

Il ballot non modifica:

- floor numerici;
- `support_basis` firmati;
- formula `n_required=max(contract_floor,n_power)`;
- regola interna Spearman `n>=10` matched pairs;
- status diagnostico del p95;
- semantica Average Precision;
- gate threshold;
- popolazioni osservate.

Esito lente 1:

```text
CLEAN
```

## 2. Lente schema e integrità referenziale

### 2.1 Finding corretto durante il counter-check

La prima stesura collocava `support_template_id` anche nel metric catalog. Era
incoerente perché la stessa metrica può richiedere template differenti in
`calibration`, `development-metric` e `final-test`.

La versione sottoposta a questo report colloca l'autorità del template
esclusivamente nel binding `(metric_id,split_role)`.

### 2.2 Support inheritance

La prima stesura usava il solo booleano `support_policy_required`, insufficiente
per distinguere:

- metrica con policy propria;
- puro derivato che condivide esattamente il supporto della metrica madre;
- diagnostico privo di policy e incapace di soddisfare gate.

La versione verificata usa:

```text
support_mode = own_policy | inherit | none_diagnostic
support_parent_metric_id = canonical metric ID | null
```

con guardie acicliche e divieto di ampliare split o partizione scientifica.
Questo chiude la possibilità che un diagnostico senza policy autonoma venga
pubblicato come se avesse supporto indipendente.

### 2.3 Namespace

La grammatica inizialmente permissiva è stata sostituita con:

```text
^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$
```

La grammatica non è un generatore di authority: un ID deve anche essere
presente nella lista firmata. Alias, colon, maiuscole e ID fixture restano
vietati.

### 2.4 Referenze e power

Il ballot richiede:

- exact-key per catalogo, template, binding e strata;
- proiezione esatta registry → policy;
- fail su riferimenti pendenti, duplicati, cicli e template non usati;
- un binding per ogni split ammesso per `own_policy`;
- nessun binding per `inherit` o `none_diagnostic`;
- `gate` power ID identico al metric ID;
- family power ID limitato alle cinque famiglie §11.1;
- power plan reale e digest atteso prima di una policy power-governed.

Esito lente 2:

```text
CLEAN
```

## 3. Lente regressione e governance

Verifiche eseguite:

```text
git diff --check                                            PASS
fence Markdown bilanciate                                   PASS
una sola decisione O13F_03, ancora NON FIRMATA              PASS
REV8 SPEC GO = NO                                           PASS
registry non materializzato                                 PASS
power fixture non promossa                                  PASS
inventario semantico minimo presente                        PASS
protected diff Source/CMake/Resources/ml_v2/ml/training     ZERO
diff codice ml_v3                                           ZERO
```

Il worktree contiene soltanto i due nuovi documenti O13F_03 non ancora
committati. Non è stato modificato codice, candidate, registry data, power plan
o dispatcher.

Esito lente 3:

```text
CLEAN
```

## 4. Residui espliciti, non nascosti

Il ballot non chiude ancora:

1. lista completa e spelling dei metric ID;
2. espansione per record family/problem type;
3. classificazione puntuale `mandatory_normative`, `optional_normative` o
   `diagnostic_only`;
4. scelta `own_policy`, `inherit` o `none_diagnostic` per ciascun output;
5. mapping metric×split×template;
6. elenco completo degli strata;
7. power binding effettivi;
8. `n_power`, population membership e digest delle policy ufficiali.

Questi residui non sono un difetto del ballot: sono precisamente i dati che il
successivo registry artifact dovrà materializzare dopo la firma e il recheck.
Il loro stato impedisce correttamente ogni policy ufficiale oggi.

## 5. Next permitted action

```text
1. Commit docs-only immutabile del ballot e di questo report.
2. Verifica SHA e diff del commit.
3. Firma esplicita dell'autorità su O13F_03.
4. Post-signature recheck read-only.
5. Solo dopo: registry data artifact + matrice di tracciabilità.
```

Non sono consentiti prima della firma:

```text
schema/code del registry
lista ID trattata come authority
policy ufficiali
power plan sintetico
dispatcher switch
REV8 SPEC GO
training/runtime/push
```
