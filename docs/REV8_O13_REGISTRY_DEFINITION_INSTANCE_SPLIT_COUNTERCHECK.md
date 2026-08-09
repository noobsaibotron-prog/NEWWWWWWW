# REV8 — O13F_04 — PRE-SIGNATURE COUNTER-CHECK

## Verdetto

```text
Verdetto                         = PRE_SIGNATURE_CLEAN
Data                             = 2026-08-10
Branch                           = feature/motore-v3-rev8-spec-go
Base HEAD                        = e7bae25e0fe17dc060d931d250473589664d6968
Decisione O13F_04                = NON FIRMATA
Registry code                    = NON AUTORIZZATO
Official registry instance       = BLOCKED DA INPUT REALI
REV8 dispatcher                  = INVARIATO
REV8 SPEC GO                     = NO
```

Il verdetto approva il ballot esclusivamente come candidato pre-firma. Non
approva metric catalog, companion schema, registry data, validator, policy o
activation.

## 1. Artefatti verificati

```text
docs/REV8_O13F04_REGISTRY_INPUT_READINESS_REPORT.md
SHA-256:
0b4b6b1beab68d5621c08127038ad23db5aad6ec46d695b3e4bcd06540c15cc9

docs/REV8_O13_REGISTRY_DEFINITION_INSTANCE_SPLIT_BALLOT_DRAFT.md
SHA-256 pre-firma:
13764e519ef64d231732d5ad89bfb36727b1479382db8afac383d70010662f69
```

Authority di base verificata:

```text
Candidate REV8 SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_03 signed ballot SHA-256:
1f26519c2fee5a6363e28b09000e7a719343a851f618914e4cbf107206697d13

O13F_03 authority commit:
c3dafbb64cd156d020a8a4566d7adc937acefd06

O13F_03 post-signature recheck commit:
e7bae25e0fe17dc060d931d250473589664d6968
```

## 2. Verifica degli input

La ricerca è stata ripetuta nel repository, nei worktree registrati e sotto:

```text
/Users/marco/aieq_data/motore_v3
```

Non è stata trovata alcuna istanza reale materializzata di:

```text
aieq-v3-evaluation-unit-index-1
```

per `calibration`, `development-metric` o `final-test`. L'unico power plan
presente è la fixture G1a con `pilot_sha256="aa...aa"` e
`support.notes="fixture"`.

Conseguenza verificata:

```text
official registry instance = non materializzabile oggi
official support policy     = non materializzabile oggi
conformance fixture         = utilizzabile soltanto dopo authority dedicata
```

Promuovere esempi G1a, inventare index vuoti o omettere gli input binding
produrrebbe falsa authority ed è correttamente vietato.

## 3. Counter-check architetturale

| Controllo | Esito |
|---|---|
| il riferimento «corpus reale in G3/G4, non blocker G1» punta al candidate §15 | CLEAN |
| O13F_03 resta immutata e viene raffinata append-only | CLEAN |
| definition contiene soltanto identità e regole statiche | CLEAN |
| instance contiene un solo split e digest input hash-bound | CLEAN |
| nessun metric ID, selector concreto, `n_power` o digest reale viene inventato | CLEAN |
| catalogo, blueprint e companion schema restano una unità di freeze | CLEAN |
| espansione source-family/profilo/regione×direzione è completa e deterministica | CLEAN |
| selector concreto resta authority; l'ID hashato non viene interpretato | CLEAN |
| fixture e official split hanno purpose distinti e fail-closed | CLEAN |
| definition e instance richiedono digest attesi esterni | CLEAN |
| committed bytes riordinati, extra o incompleti falliscono | CLEAN |
| `support-floor-policy-3` resta invariata | CLEAN |
| risultati delle precondizioni non sono auto-dichiarati nell'instance | CLEAN |
| risultati precondition vengono hash-bound separatamente nel futuro envelope | CLEAN |
| `POLICY_REVISION` dovrà includere firma/recheck O13F_03 e O13F_04 | CLEAN |
| freeze/counter-check pre-firma non autorizzano codice | CLEAN |
| firma e post-signature recheck precedono il validator candidate-only | CLEAN |
| input reali mancanti producono unavailable/N/A, mai fallback | CLEAN |
| REV7 dispatcher, gate, floor, Holm e power semantics non cambiano | CLEAN |
| REV8 SPEC GO resta NO | CLEAN |

## 4. Tentativi di falsificazione

### 4.1 Fixture promotion

Attacco: usare gli esempi G1a come `official_split`.

Esito: respinto da `instance_purpose`, digest esterno e mutation obbligatoria.

### 4.2 Omissione data-dependent

Attacco: non materializzare una source family dominante oppure una cella a
supporto zero.

Esito: la proiezione esatta dall'index rende la riga mancante un FAIL.

### 4.3 Rigenerazione dopo il risultato

Attacco: ricostruire l'instance dopo aver visto N/A, metriche o gate.

Esito: vietato dal freeze pre-prediction e rilevato dal digest atteso esterno.

### 4.4 Caller-controlled template

Attacco: compilare direttamente una policy da blueprint costruiti dal caller.

Esito: non ufficiale/FAIL senza definition, instance e projection match.

### 4.5 Precondition self-report

Attacco: inserire nell'instance un esito favorevole dichiarato dal producer.

Esito: l'instance contiene soltanto il predicato concreto; l'evidenza viene
calcolata contro il population plan congelato e hash-bound separatamente.

### 4.6 Freeze sufficiente senza firma

Attacco: interpretare il commit pre-firma come autorizzazione al builder.

Esito: stop rule esplicita; servono firma e post-signature `CLEAN`.

## 5. Limiti deliberati

O13F_04 non dimostra ancora:

- completezza del futuro metric catalog;
- correttezza dei companion schema;
- correttezza dei blueprint concreti;
- correttezza del validator/expander;
- disponibilità del corpus e del power plan reali;
- producibilità di una policy ufficiale.

Questi punti restano gate delle tranche successive e non vengono trasformati
in PASS dal presente counter-check.

## 6. Regressione

```text
Full ml_v3 unittest suite: 641/641 PASS
git diff --check:         PASS
Markdown fences:          PASS
candidate SHA:            MATCH
O13F_03 signed SHA:        MATCH
protected code diff:      ZERO
runtime/training changes: ZERO
```

Comando suite:

```text
PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest discover \
  -s ml_v3/tests -t . -p 'test_*.py'
```

## 7. Stato risultante

```text
O13F_04 ballot                    = PRE-SIGNATURE CLEAN, NON FIRMATO
O13F_03 authority                 = INVARIATA
Static registry definition        = NON MATERIALIZZATA
Conformance registry instance     = NON MATERIALIZZATA
Official registry instance        = BLOCKED DA INPUT REALI
Registry validator/expander code  = NON AUTORIZZATO
Official support policies         = BLOCKED
REV8 dispatcher                   = INVARIATO
REV8 SPEC GO                      = NO
```

## 8. Next permitted action

1. congelare atomicamente i due documenti O13F_04 e questo counter-check in un
   commit docs-only;
2. verificare commit scope e SHA del ballot sul commit immutabile;
3. ottenere firma esplicita dell'autorità su O13F_04;
4. eseguire un post-signature recheck separato;
5. soltanto dopo un esito `CLEAN`, preparare la tranche docs/data elencata nel
   ballot §7.

Nessun codice, companion schema, registry definition o fixture è autorizzato
dal solo freeze pre-firma.
