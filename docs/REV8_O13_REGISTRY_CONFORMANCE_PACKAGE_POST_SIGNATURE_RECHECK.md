# REV8 — O-13 — REGISTRY CONFORMANCE PACKAGE POST-SIGNATURE RECHECK

## Verdetto

```text
Verdetto                              = POST_SIGNATURE_CLEAN
Data                                  = 2026-08-10
Branch                                = feature/motore-v3-rev8-spec-go
Base authority HEAD                   = f201fa162752857c25fb5114e8ccc74a2798c85f
Package freeze commit                 = 56363b3e7cb8a1ead617bfeb8934568dd0638e09
Signature commit                      = 6ce4abfd1a149fb721b312ae5459dffa3c4bbe1e
Decisione O13F_06                     = APPROVO
O13F_06 authority                     = SIGNED + POST_SIGNATURE_CLEAN
Official registry instance/policies   = NON MATERIALIZZATI
REV8 dispatcher                       = INVARIATO
REV8 SPEC GO                          = NO
```

## 1. Riferimenti firmati

```text
SHA-256 ballot pre-firma:
83af9343bf150aefb47010ffe5af64d9daa6a5cda61a004eeadc9f0618ccbf6d

SHA-256 ballot firmato:
d2f5848b7da7436ad91052bfb7a961125f1687d6aaf160d3a0116f2f1a8bbb10

SHA-256 package SHA256SUMS:
4c6ff97ec04745d641f801add54407513a0dc3cf27cd5ef6225411de06a019d0

SHA-256 materialization report:
79856ef64a12c2fe93f09612bf65b82ef1b827e14c90dc77844b729ea5c07f48

SHA-256 pre-signature counter-check:
31de648fb31ad8ee8ef9fe4d8961464e255052e24814f1b65d626aee1f8180af
```

Il ballot firmato registra:

```text
Decisione O13F_06: APPROVO
Firma/nome: Marco
Data: 2026-08-10
Commit ballot/package pre-firma:
56363b3e7cb8a1ead617bfeb8934568dd0638e09
```

## 2. Atomicità della firma

Il commit di firma ha come parent esatto il freeze commit e modifica un solo
path:

```text
M docs/REV8_O13_REGISTRY_CONFORMANCE_PACKAGE_BALLOT_DRAFT.md
```

La trasformazione è limitata a:

```text
NON FIRMATA -> APPROVO — PENDING POST-SIGNATURE RECHECK
firma/nome  -> Marco
data        -> 2026-08-10
pre-sign ballot SHA -> digest congelato
pre-sign commit     -> freeze commit congelato
```

Nessun JSON, manifest, materialization report o counter-check è cambiato fra
freeze e firma.

## 3. Integrità del package

Sul tree firmato:

```text
package non-ballot diff freeze -> signature = ZERO
SHA256SUMS SHA-256                       = invariato
undici artifact digest                   = invariati
companion schema/definition/claim SHA     = invariati
instance/readiness SHA chain              = invariata
31 mutation oracle                        = invariati
```

La verifica `shasum -a 256 -c SHA256SUMS` resta `PASS (11/11)` e il consumer
check indipendente pre-firma rimane applicabile agli stessi blob immutati.

## 4. Regressione post-firma

```text
Full ml_v3 unittest suite = 641/641 PASS
Runtime                   = 89.668 s
protected code diff       = ZERO
Source/CMake/Resources    = INVARIATI
ml_v2/ml/training         = INVARIATI
REV8 dispatcher           = INVARIATO
worktree pre-report       = CLEAN
push                      = NON ESEGUITO
```

## 5. Scope dell'autorità risultante

O13F_06 rende autoritativi, nel solo path di conformance O-13 v2:

```text
population selector companion schema
registry precondition companion schema
registry definition v2 fixture
conformance claim plan fixture
calibration/development/final conformance instances
calibration readiness fixture
candidate metric/scope/subject trace matrix
31 negative mutation recipe
```

I cinque gate, Holm order e power design del claim plan restano fixture per
coprire i rami del futuro validator. Non sono gate scientifici.

## 6. Cosa diventa autorizzabile

La sola prossima tranche implementativa autorizzabile è candidate-only:

```text
registry definition/claim-plan/instance validator
deterministic registry expander
test eseguibili delle 31 mutation recipe
oracle comparison definition + claim plan -> instance
```

L'implementazione deve:

- consumare soltanto gli schema e gli artefatti firmati O13F_06;
- rifiutare committed bytes non canonici;
- applicare exact-key e hash binding fail-closed;
- mantenere separati fixture input e input ufficiali;
- impedire ogni promozione fixture → scientific/official;
- non introdurre nuovi threshold, gate, reason code o policy;
- restare fuori da dispatcher, activation, runtime e training.

## 7. Blocchi ancora attivi

Restano esplicitamente bloccati:

```text
scientific claim plan
official registry instance
official evaluation-unit/manifest/annotation inputs
official population plan
official power plan
official support-floor policies
report/activation envelope
REV8 dispatcher activation
runtime/training/push
REV8 SPEC GO
G1c close / G1 PASS
```

## 8. Next permitted action

Preparare una singola tranche candidate-only contenente validator, expander,
test delle 31 mutation e oracle comparison. La tranche dovrà essere:

1. limitata a `ml_v3/contracts/` e `ml_v3/tests/`, salvo un report docs-only;
2. priva di artefatti scientifici o officiali;
3. verificata contro i digest O13F_06 congelati;
4. sottoposta a suite completa, mutation test e counter-check indipendente;
5. congelata in commit atomico senza push.

Nessuna altra fase è autorizzata da questo recheck.
