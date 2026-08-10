# REV8 — O13F_04 — POST-SIGNATURE RECHECK

## Verdetto

```text
Verdetto                          = POST_SIGNATURE_CLEAN
Data                              = 2026-08-10
Branch                            = feature/motore-v3-rev8-spec-go
Pre-sign freeze commit            = 019dcdadac0872331a8d885256673cb39ebb0bb0
Signature commit                  = 3d636eff6dc9d4c9891422f5b4b8545a5e785b5b
Decisione O13F_04                 = APPROVO
O13F_04 authority                 = SIGNED + POST_SIGNATURE_CLEAN
Registry validator/expander code  = NON ANCORA AUTORIZZATO
Official registry instance        = BLOCKED DA INPUT REALI
REV8 dispatcher                   = INVARIATO
REV8 SPEC GO                      = NO
```

Il presente report è l'authority di stato successiva al ballot firmato. La
dicitura `PENDING POST-SIGNATURE RECHECK` rimasta nel ballot descrive lo stato
al momento della firma ed è superata esclusivamente da questo esito `CLEAN`.

## 1. Catena verificata

```text
O13F_03 authority commit:
c3dafbb64cd156d020a8a4566d7adc937acefd06

O13F_03 recheck commit:
e7bae25e0fe17dc060d931d250473589664d6968

O13F_04 pre-sign freeze commit:
019dcdadac0872331a8d885256673cb39ebb0bb0

O13F_04 signature commit:
3d636eff6dc9d4c9891422f5b4b8545a5e785b5b
```

Digest verificati:

```text
O13F_04 ballot pre-firma:
13764e519ef64d231732d5ad89bfb36727b1479382db8afac383d70010662f69

O13F_04 ballot firmato:
7202df3db4b461480f9bbdc63d05a82d413e34d4bdab91df2157914686850344

Input readiness report:
0b4b6b1beab68d5621c08127038ad23db5aad6ec46d695b3e4bcd06540c15cc9

Pre-signature counter-check:
cf47f947a099ae914dd12cfe2ffdb1e247267fd8bdddd85b0b1a06e6470e009b
```

## 2. Integrità della firma

Il diff fra freeze e commit di firma contiene soltanto:

- stato `APPROVO — PENDING POST-SIGNATURE RECHECK`;
- `Decisione O13F_04: APPROVO`;
- `Firma/nome: Marco`;
- `Data: 2026-08-10`;
- SHA-256 del ballot pre-firma;
- commit del freeze pre-firma.

Nessuna regola normativa, schema proposto, fixture, stop rule o non-decisione è
cambiata durante la firma.

## 3. Recheck sostanziale

| Controllo | Esito |
|---|---|
| firma riferita al corretto commit pre-firma | CLEAN |
| SHA pre-firma nel ballot uguale ai byte del freeze | CLEAN |
| diff di firma limitato ai sei campi autorizzati | CLEAN |
| O13F_03 preservata come authority di base | CLEAN |
| split definition/instance limitato a packaging e sequencing | CLEAN |
| metriche, floor, gate, Holm e power semantics invariati | CLEAN |
| definition statica priva di membership e risultati | CLEAN |
| instance vincolata a un solo split e input hash-bound | CLEAN |
| fixture non promuovibile a scientific authority | CLEAN |
| precondition predicate separati dai risultati valutati | CLEAN |
| evidence digest delle precondition richiesto nel futuro envelope | CLEAN |
| policy v3 invariata, nessuno schema v4 | CLEAN |
| companion schema e data package ancora da materializzare e firmare | CLEAN |
| validator code non autorizzato prima del freeze docs/data | CLEAN |
| official instance/policy bloccate senza corpus e power plan reali | CLEAN |
| REV7 dispatcher invariato | CLEAN |
| REV8 SPEC GO resta NO | CLEAN |

## 4. Test e regressione

```text
Full ml_v3 unittest suite: 641/641 PASS
git diff --check:         PASS
Markdown fences:          PASS
pre-sign SHA:             MATCH
signed SHA:               MATERIALIZZATO
protected code diff:      ZERO
runtime/training changes: ZERO
```

Comando suite:

```text
PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest discover \
  -s ml_v3/tests -t . -p 'test_*.py'
```

## 5. Stato risultante

```text
O13F_04 authority                  = FIRMATA E POST-SIGNATURE CLEAN
O13F_03 authority                  = INVARIATA
Selector companion schema          = AUTORIZZATO COME PROSSIMA TRANCHE DOCS/DATA
Precondition companion schema      = AUTORIZZATO COME PROSSIMA TRANCHE DOCS/DATA
Static registry definition         = AUTORIZZATA COME PROSSIMA TRANCHE DOCS/DATA
Conformance registry instances     = AUTORIZZATE COME PROSSIMA TRANCHE DOCS/DATA
Registry validator/expander code   = NON ANCORA AUTORIZZATO
Official registry instances        = BLOCKED DA INPUT REALI
Official support policies          = BLOCKED
REV8 dispatcher                    = INVARIATO
REV8 SPEC GO                       = NO
G1c close / G1 PASS                = NO
```

## 6. Next permitted action

Preparare in una singola tranche docs/data, senza codice:

1. selector companion schema exact-key;
2. precondition companion schema exact-key;
3. registry definition completa con metric catalog e blueprint;
4. matrice candidate-clause → metric/scope/split;
5. conformance fixture positive e negative;
6. freeze pre-firma e counter-check del package immutabile;
7. firma esplicita e post-signature recheck.

Soltanto dopo quel secondo recheck `CLEAN` sarà autorizzabile l'implementazione
candidate-only del validator/expander. Instance e policy ufficiali restano
bloccate fino agli input reali G3/G4.
