# REV8 — O-13 — REGISTRY CONFORMANCE PACKAGE COUNTER-CHECK

## Verdetto

```text
Verdetto                              = PRE_SIGNATURE_CLEAN
Data                                  = 2026-08-10
Branch                                = feature/motore-v3-rev8-spec-go
Base HEAD                             = f201fa162752857c25fb5114e8ccc74a2798c85f
Package authority                     = NON ANCORA EFFICACE
Decisione O13F_06                     = NON FIRMATA
Registry validator/expander code      = NON AUTORIZZATO
Official registry instance/policies   = NON MATERIALIZZATI
REV8 SPEC GO                          = NO
```

Il counter-check non autorizza il package. Stabilisce soltanto che i byte
docs/data sottoposti al ballot O13F_06 sono internamente coerenti e pronti per
un freeze immutabile pre-firma.

## 1. Snapshot controllato

```text
SHA-256 SHA256SUMS:
4c6ff97ec04745d641f801add54407513a0dc3cf27cd5ef6225411de06a019d0

SHA-256 materialization report:
79856ef64a12c2fe93f09612bf65b82ef1b827e14c90dc77844b729ea5c07f48

SHA-256 ballot pre-firma:
83af9343bf150aefb47010ffe5af64d9daa6a5cda61a004eeadc9f0618ccbf6d
```

Il manifest elenca undici e soltanto undici JSON canonici in:

```text
docs/data/rev8/o13_registry_conformance_v2/
```

Il ballot resta `NON FIRMATA`; i campi commit/SHA della sezione firma restano
`PENDING FREEZE` fino alla creazione del commit atomico.

## 2. Authority e provenance

Sono stati verificati:

```text
candidate contract SHA-256 =
398aea26daa6d48324f9df7e9dda54c38b332fb4fb230563d5c26172875745

O13F_01 authority/recheck =
59bec34856a08aa43cc52bb123e904a44575f4f6 /
d0b9916cd08aff29dfde03ffe0c8f38c3656a96c

O13F_02 authority/recheck =
26f35e753f96ebc48d0453e432e8f2a3f5380687 /
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e

O13F_03 authority/recheck =
c3dafbb64cd156d020a8a4566d7adc937acefd06 /
e7bae25e0fe17dc060d931d250473589664d6968

O13F_04 authority/recheck =
3d636eff6dc9d4c9891422f5b4b8545a5e785b5b /
1275dbbd29a232dadad1729fa9e5605fba2402ce

O13F_05 authority/recheck =
d56c7f973bae0a7a41865212f77dd43b0b670619 /
f201fa162752857c25fb5114e8ccc74a2798c85f
```

Tutti i dieci commit sono presenti come commit object. Il file candidate
presente nel tree ha il digest dichiarato. O13F_05 risulta `APPROVO`, firmata
da Marco il 2026-08-10 e seguita da report `POST_SIGNATURE_CLEAN`.

## 3. Metodo indipendente

Il consumer check è stato eseguito sui file materializzati senza importare il
generatore usato per produrli. Ha:

1. letto UTF-8 e JSON dai file reali;
2. ricostruito i canonical bytes con key sort, separatori compatti e singolo
   LF finale;
3. ricalcolato SHA-256 e verificato tutte le entry di `SHA256SUMS`;
4. seguito i binding schema → definition → claim plan → instance → readiness;
5. ricostruito cardinalità, identità, proiezioni e relazioni parent/child;
6. controllato gli input fixture hash-bound e le espansioni split-specifiche;
7. verificato l'oracle negativo e i reason code attesi.

Lo script di controllo è temporaneo, esterno al repository e non fa parte
dell'authority.

## 4. Risultati byte-level e strutturali

```text
canonical JSON UTF-8 + singolo LF                    PASS (11/11)
SHA256SUMS                                           PASS (11/11)
companion schema hash binding                       PASS (2/2)
claim plan -> definition SHA                        PASS
instance -> definition/claim plan SHA               PASS (3/3)
readiness -> calibration instance SHA               PASS
D/F -> upstream calibration/readiness SHA           PASS (2/2)
fixture input facts -> instance SHA fields          PASS (3/3 split)
metric ID/definition tuple uniqueness               PASS (164/164)
mandatory own-policy closure                        PASS (139/139)
diagnostic no-binding closure                       PASS (25/25)
metric binding projection D/F                       PASS (278/278)
metric-stratum power rows                           PASS (350/350)
calibrator dependency rows                          PASS (268/268)
definition/claim breakdown bijection                PASS (153/153)
final electronic expansion                          PASS (1071/1071)
electronic frozen-parent denominator                PASS (4/10 = 2/5)
mutation IDs / expected FAIL outcomes               PASS (31/31)
```

Sono inoltre presenti e byte-distinti:

```text
null
trance
e + U+0301
U+00E9
```

Lo stesso gruppo può essere membro di più breakdown ma viene contato una sola
volta nel parent congelato.

## 5. Finding trovati e chiusi prima del freeze

### F-01 — promozioni fixture non coperte dall'oracle

Il report e il ballot richiedevano il rifiuto di:

```text
conformance claim plan -> scientific claim plan
conformance instance   -> official registry instance
```

ma il primo mutation manifest non conteneva le due recipe esplicite. Sono
state aggiunte con reason code distinti:

```text
CONFORMANCE_CLAIM_PLAN_PROMOTION_FORBIDDEN
CONFORMANCE_INPUT_PROMOTION_FORBIDDEN
```

Esito: `CLOSED_BEFORE_FREEZE`.

### F-02 — selector relazionale sottospecificato

Le precondition `minimum_qualifying_partitions` selezionano la popolazione
aggregata e la partizionano tramite `partition_key=source_family`; non sono una
precondition concreta per ogni singola source family. Il primo companion
schema non dichiarava esplicitamente che anche i nested selector della
precondition concreta conservano gli exact-key del selector template.

Il companion schema ora congela:

```text
blueprint_nested_selector_key_mode = template_exact_keys
concrete_nested_selector_key_mode  = template_exact_keys
expanded_partition_value_in_nested_selector = forbidden
```

È stata aggiunta la mutation:

```text
mutation.precondition_schema.concrete_selector_mode
-> PRECONDITION_RELATIONAL_SELECTOR_MODE_MISMATCH
```

La modifica rende implementabile l'intento già firmato O13F_03/O13F_05 senza
aggiungere threshold, predicate kind o scelta scientifica.

Esito: `CLOSED_BEFORE_FREEZE`.

### F-03 — parent selector non ricostruibile nella prima bozza

Il controllo di proiezione exact-key ha rilevato, durante la materializzazione
e prima dello snapshot qui controllato, selector child incompleti per regioni
tonali, minuti clean e adapter. I child finali conservano exact-key e valori
del parent e il consumer check ricostruisce tutte le proiezioni.

Esito: `CLOSED_BEFORE_FREEZE`.

## 6. Regressione e perimetro

```text
Full ml_v3 unittest suite = 641/641 PASS
Runtime                   = 79.242 s
git diff --check          = PASS
protected code diff       = ZERO
Source/CMake/Resources    = INVARIATI
ml_v2/ml/training         = INVARIATI
REV8 dispatcher           = INVARIATO
push                      = NON ESEGUITO
```

I file temporanei di generazione/controllo non appartengono al repository e
devono essere rimossi prima dell'handoff.

## 7. Limiti deliberati confermati

Il package è esclusivamente un oracle di conformance. Non contiene e non può
autorizzare:

```text
scientific claim plan
official registry instance
official population/power plan
official support policies
prediction o risultati scientifici
validator/expander implementation
report/activation envelope
REV8 dispatcher activation
runtime/training/push
REV8 SPEC GO
G1c close / G1 PASS
```

I cinque gate, Holm order e power design del claim plan sono fixture di
copertura dei rami futuri del validator, non decisioni scientifiche.

## 8. Decisione del counter-check

```text
Package bytes                     = PRE_SIGNATURE_CLEAN
Materialization report            = COERENTE
O13F_06 ballot                     = COERENTE, NON FIRMATA
Atomic docs/data freeze            = AUTORIZZABILE COME NEXT ACTION
Registry validator/expander code   = ANCORA NON AUTORIZZATO
Official artifacts/policies        = BLOCKED
REV8 SPEC GO                       = NO
```

Next permitted action:

1. creare un commit atomico contenente soltanto package JSON/manifest,
   materialization report, ballot e questo counter-check;
2. verificare tree, hash e protected diff sul commit immutabile;
3. richiedere la firma esplicita di Marco su commit e SHA-256 del ballot;
4. eseguire un post-signature recheck separato;
5. soltanto dopo un esito `POST_SIGNATURE_CLEAN`, valutare l'autorizzazione
   candidate-only del registry validator/expander.
