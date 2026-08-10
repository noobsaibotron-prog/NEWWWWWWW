# REV8 — O-13 — REGISTRY CONFORMANCE PACKAGE BALLOT (O13F_06)

## Stato del documento

```text
Tipo                                = package ballot docs/data pre-firma
Data preparazione                   = 2026-08-10
Branch                              = feature/motore-v3-rev8-spec-go
Base commit                         = f201fa162752857c25fb5114e8ccc74a2798c85f
Decisione O13F_06                   = APPROVO — PENDING POST-SIGNATURE RECHECK
Package purpose                     = CONFORMANCE_ONLY_NOT_SCIENTIFIC_AUTHORITY
Registry validator/expander code    = NON AUTORIZZATO
Official registry instance/policies = NON MATERIALIZZATI
REV8 dispatcher activation          = NO
REV8 SPEC GO                        = NO
```

## 1. Package sottoposto alla decisione

```text
Directory:
docs/data/rev8/o13_registry_conformance_v2/

SHA-256 SHA256SUMS:
4c6ff97ec04745d641f801add54407513a0dc3cf27cd5ef6225411de06a019d0

Materialization report:
docs/REV8_O13_REGISTRY_CONFORMANCE_PACKAGE_REPORT.md
```

La decisione riguarda esattamente gli undici JSON elencati in `SHA256SUMS`,
il manifest SHA e il report. Un file extra, mancante o con digest diverso non
appartiene al package approvato.

## 2. Decisione O13F_06-A — companion schema

Si approvano come schema di conformance O-13 v2:

```text
aieq-v3-rev8-o13-population-selector-schema-2
aieq-v3-rev8-o13-registry-precondition-schema-2
```

Gli exact-key, i domini e le expansion injection materializzati sono
autoritativi per la successiva implementazione candidate-only. Nessun parser
può inferire semantica dagli ID o riordinare committed bytes non canonici.

Le precondition relazionali usano gli exact-key del selector template anche
nella forma concreta: selezionano il parent/child aggregato e applicano
`partition_key` in fase di valutazione, senza iniettare nel selector il valore
della singola partizione.

## 3. Decisione O13F_06-B — catalogo e definition v2

Si approvano:

```text
164 metric definition
139 mandatory_normative / own_policy
25 diagnostic_only / none_diagnostic
4 calibrator-fit subject non metrici
1 benchmark-readiness subject
153 diagnostic breakdown definition elettroniche
support-template blueprint materializzati
```

Lo spelling di `metric_id`, `definition_key`, scope, record/type expansion e
status diventa authority O-13 soltanto dopo firma e post-signature recheck del
presente ballot.

## 4. Decisione O13F_06-C — claim-plan fixture

Si approva il claim plan esclusivamente come `conformance_fixture`.

I cinque gate, il loro ordine Holm e i power design sono scelti per coprire i
rami del validator; non sono gate scientifici e non possono essere copiati o
promossi nel primo `scientific_claim_plan` senza una firma separata
pre-pilot/pre-prediction.

La closure di 278 metric binding, 350 metric-stratum power row, 268
calibrator dependency, quattro fit binding, un benchmark-readiness binding e
153 breakdown row è l'oracle della proiezione.

## 5. Decisione O13F_06-D — input e instance fixture

Le tre instance positive e le projection input sono approvate soltanto come
conformance oracle.

Regola fail-closed:

```text
conformance projection + conformance_fixture -> ammesso nei soli test
conformance projection + official_split      -> FAIL
conformance claim plan promosso a scientific -> FAIL
fixture digest usato in policy/report/gate   -> FAIL
```

Le projection non sostituiscono gli input reali richiesti da O13F_04 e non
testano trusted admission, audio o mask.

## 6. Decisione O13F_06-E — breakdown elettronici

Si approvano come oracle:

- exact equality di `electronic_subgenre` senza normalizzazione;
- espansione di tutti i valori osservati, incluso `null`;
- membership sovrapposta nei breakdown ma conteggio unico nel parent;
- denominatore su tutti i gruppi final-test congelati;
- boundary fixture esatto `5*4 == 2*10`;
- 153 × 7 = 1071 breakdown concreti;
- nessun gate, claim, Holm o power derivato dai breakdown.

## 7. Decisione O13F_06-F — mutation oracle e tracciabilità

Si approvano:

- 31 mutation recipe fail-closed, incluse le due promozioni vietate
  `conformance_fixture -> scientific/official` e il selector-mode
  relazionale;
- matrice completa candidate → metric/scope/split/subject;
- motivazione root governing / child non-governing per i gate fixture;
- hash binding fra schema, definition, claim plan, instance e readiness.

Il mutation manifest non sostituisce i futuri test eseguibili: ne congela gli
input e gli esiti attesi.

## 8. Conseguenza dopo firma e recheck

Solo dopo `POST_SIGNATURE_CLEAN` O13F_06 diventa autorizzabile:

```text
implementazione candidate-only del registry validator/expander
test executable delle 31 mutation recipe
oracle comparison definition + claim plan -> instance
```

Restano bloccati:

```text
scientific claim plan
official registry instance
official population/power plan
official support policies
report/activation envelope
REV8 dispatcher
runtime/training/push
REV8 SPEC GO
G1c close / G1 PASS
```

## 9. Stop rule

Fino al post-signature recheck:

```text
package authority                  = NON EFFICACE
registry validator/expander code   = NON AUTORIZZATO
official registry/policies         = NON MATERIALIZZABILI
REV8 SPEC GO                       = NO
```

## 10. Ballot

```text
Decisione O13F_06: APPROVO
Firma/nome: Marco
Data: 2026-08-10

SHA-256 ballot pre-firma:
83af9343bf150aefb47010ffe5af64d9daa6a5cda61a004eeadc9f0618ccbf6d

Commit ballot/package pre-firma:
56363b3e7cb8a1ead617bfeb8934568dd0638e09
```

Opzioni ammesse:

```text
APPROVO
RESPINGO
RICHIEDO MODIFICHE (con motivazione testuale)
```
