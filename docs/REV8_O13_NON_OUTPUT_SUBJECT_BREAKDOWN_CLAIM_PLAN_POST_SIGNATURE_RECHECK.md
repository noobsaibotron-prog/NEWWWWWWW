# REV8 — O13F_05 — POST-SIGNATURE RECHECK

## Verdetto

```text
Verdetto                              = POST_SIGNATURE_CLEAN
Data                                  = 2026-08-10
Branch                                = feature/motore-v3-rev8-spec-go
Pre-sign freeze commit                = 91da496fe7cad44c0c4bc604aa677fc1c9de7897
Signature commit                      = d56c7f973bae0a7a41865212f77dd43b0b670619
Decisione O13F_05                     = APPROVO
O13F_05 authority                     = SIGNED + POST_SIGNATURE_CLEAN
Registry companion/data package       = AUTORIZZATO COME PROSSIMA TRANCHE
Registry validator/expander code      = NON ANCORA AUTORIZZATO
Official registry instance/policies   = BLOCKED DA INPUT E FIRME REALI
REV8 dispatcher                       = INVARIATO
REV8 SPEC GO                          = NO
```

Il presente report è l'authority di stato successiva al ballot firmato. La
dicitura `PENDING POST-SIGNATURE RECHECK` rimasta nel ballot descrive lo stato
al momento della firma ed è superata esclusivamente dal presente esito
`POST_SIGNATURE_CLEAN`.

## 1. Catena verificata

```text
O13F_04 authority/recheck commit:
1275dbbd29a232dadad1729fa9e5605fba2402ce

O13F_05 pre-sign freeze commit:
91da496fe7cad44c0c4bc604aa677fc1c9de7897

O13F_05 signature commit:
d56c7f973bae0a7a41865212f77dd43b0b670619
```

Digest verificati:

```text
O13F_05 representability audit:
3298d1575420e02b39c1370c980a54401834b94e273bace485e836ecd6644145

O13F_05 ballot pre-firma:
984a1824327e3b57842a937ac8edaf44436a92380ce1f206c01cbbb7c19c2cac

O13F_05 pre-signature counter-check:
424a24ab8fce0260fe21a85dd4270d7b916fe5622bd8adcccffcc232bf820a37

O13F_05 ballot firmato:
64c26017b71e7db13cfe2be0761f0e9941201f9c7f7fd1172a85d855f80e23dc
```

Il digest pre-firma è stato ricalcolato direttamente dal blob del ballot nel
commit `91da496f`; coincide con il valore incorporato nel ballot firmato.

## 2. Integrità della firma

Il diff fra freeze e commit di firma contiene un solo file:

```text
docs/REV8_O13_NON_OUTPUT_SUBJECT_BREAKDOWN_CLAIM_PLAN_BALLOT_DRAFT.md
```

Le sole modifiche sono:

- stato `APPROVO — PENDING POST-SIGNATURE RECHECK`;
- `Decisione O13F_05: APPROVO`;
- `Firma/nome: Marco`;
- `Data: 2026-08-10`;
- SHA-256 del ballot pre-firma;
- commit del freeze pre-firma.

Il numstat del diff è `6 insertions / 6 deletions`. Nessuna decisione
normativa, matrice, schema, formula, stop rule, non-decisione o sequenza è
cambiata durante la firma.

## 3. Recheck sostanziale

| Controllo | Esito |
|---|---|
| firma riferita al corretto commit pre-firma | CLEAN |
| SHA pre-firma incorporato uguale ai byte congelati | CLEAN |
| diff di firma limitato ai sei campi autorizzati | CLEAN |
| audit di rappresentabilità invariato | CLEAN |
| counter-check indipendente 3/3 sui byte finali pre-firma | CLEAN |
| calibration-fit trattato come subject non metrico | CLEAN |
| output metrici vietati sullo split `calibration` | CLEAN |
| dipendenze calibrator D/F hash-bound e fail-closed | CLEAN |
| `electronic_subgenre` distinto da `source_family` | CLEAN |
| denominatore elettronico basato su tutti i gruppi final-test congelati | CLEAN |
| readiness elettronica obbligatoria separata dai breakdown diagnostici | CLEAN |
| breakdown definition e claim-plan in corrispondenza biunivoca | CLEAN |
| breakdown diagnostici non promossi implicitamente a claim | CLEAN |
| claim-plan overlay congelato prima di prediction | CLEAN |
| Holm applicato per p-value osservato, non per ordine ID firmato | CLEAN |
| power design rule e root governing strata espliciti | CLEAN |
| catalogo base congelato a 164 metric definition | CLEAN |
| schema registry definition/instance v2 richiesto nel path O13F_05 | CLEAN |
| O13F_04 preservata salvo supersession esplicite pre-materialization | CLEAN |
| support-floor-policy-3 e dispatcher REV7 invariati | CLEAN |
| official instance/policy ancora bloccate senza input e firma reali | CLEAN |
| REV8 SPEC GO resta NO | CLEAN |

## 4. Test e regressione

```text
Full ml_v3 unittest suite: 641/641 PASS
Runtime suite duration:     81.274 s
git diff --check:           PASS
Markdown fences:            PASS
pre-sign SHA:               MATCH
signed SHA:                 MATERIALIZZATO
protected code diff:        ZERO
runtime/training changes:   ZERO
```

Comando suite:

```text
PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest discover \
  -s ml_v3/tests -t . -p 'test_*.py'
```

Il perimetro protetto confrontato da `1275dbbd` comprende:

```text
Source
CMakeLists.txt
Resources
ml_v2
ml
training
ml_v3
```

Il diff su tale perimetro è zero.

## 5. Stato risultante

```text
O13F_05 authority                    = FIRMATA E POST-SIGNATURE CLEAN
O13F_04 authority                    = INVARIATA SALVO SUPERSESSION ESPLICITE
Registry companion schema            = AUTORIZZATO COME PROSSIMA TRANCHE DOCS/DATA
Registry precondition/subject schema = AUTORIZZATO COME PROSSIMA TRANCHE DOCS/DATA
Registry definition v2 fixture       = AUTORIZZATA COME PROSSIMA TRANCHE DOCS/DATA
Conformance claim plan fixture        = AUTORIZZATO COME PROSSIMA TRANCHE DOCS/DATA
Conformance registry instances        = AUTORIZZATE COME PROSSIMA TRANCHE DOCS/DATA
Registry validator/expander code      = NON ANCORA AUTORIZZATO
Scientific claim plan                 = BLOCKED DA INPUT E FIRMA REALI
Official registry instances           = BLOCKED DA INPUT REALI
Official population/power policies    = BLOCKED DA INPUT E FIRME REALI
REV8 dispatcher                       = INVARIATO
REV8 SPEC GO                          = NO
G1c close / G1 PASS                   = NO
runtime / training / push             = NON AUTORIZZATI
```

## 6. Next permitted action

Preparare in una singola tranche docs/data pre-implementazione, senza codice:

1. companion selector schema;
2. companion precondition/subject schema;
3. registry definition v2 con catalogo e subject definitions;
4. conformance claim plan con gate e power esplicitamente fixture;
5. conformance registry instance positive e negative;
6. matrice completa candidate → metric/scope/split/subject;
7. freeze, counter-check, firma e post-signature recheck del package.

Soltanto dopo il post-signature `CLEAN` di tale package sarà autorizzabile
l'implementazione candidate-only del validator/expander. Claim plan,
population plan, registry instance e policy ufficiali restano bloccati fino
agli input reali G3/G4 e alle rispettive firme.

