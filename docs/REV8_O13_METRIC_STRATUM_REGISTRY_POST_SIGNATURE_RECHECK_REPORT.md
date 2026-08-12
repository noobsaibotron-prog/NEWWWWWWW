# REV8 — O13F_03 — POST-SIGNATURE RECHECK

**Verdetto:** `POST_SIGNATURE_CLEAN`

**Metodo:** recheck mirato sul commit firmato e confronto byte/semantico con lo
snapshot pre-firma autorizzato. Non viene presentato come review multi-agent.

## 1. Target verificato

```text
Ballot:
docs/REV8_O13_METRIC_STRATUM_REGISTRY_BALLOT_DRAFT.md

Commit pre-firma:
5120b5728c781e10a898335e0f0c9bbc72485944

SHA-256 pre-firma:
23a31daa99a78510c6b666a94749f590853faeb38064aee6cf06a40c99bddfa5

Commit firmato:
c3dafbb64cd156d020a8a4566d7adc937acefd06

SHA-256 firmato:
1f26519c2fee5a6363e28b09000e7a719343a851f618914e4cbf107206697d13
```

Il file estratto direttamente dal commit pre-firma produce esattamente lo SHA
dichiarato nel ballot firmato. Il commit di firma modifica un solo file: il
ballot O13F_03.

## 2. Firma verificata

```text
Decisione O13F_03: APPROVO
Firma/nome:          Marco
Data:                2026-08-10
```

Non sono presenti placeholder, `APPROVO CON MODIFICHE` o riferimenti a un
commit/SHA differente.

## 3. Confronto pre-firma → firma

Le sole differenze sono:

1. stato documento da `NON FIRMATA` a
   `APPROVO — PENDING POST-SIGNATURE RECHECK`;
2. stop-rule aggiornata per richiedere il recheck `CLEAN` successivo alla
   firma;
3. decisione, nome, data, SHA e commit pre-firma materializzati.

Normalizzando esclusivamente queste zone, il testo pre-firma e quello firmato
sono identici. Non è cambiata alcuna decisione scientifica, exact-key,
precondizione, regola di supporto, floor, selector, scope o stop-rule
sostanziale.

## 4. Provenance e authority

```text
Candidate SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_01 current/signed SHA-256:
3b73e9b43e95140bb0e33a938afbedda57a907f49c3f67220d38b923363d0d14

O13F_02 current/signed SHA-256:
4021353cac2f2a1b469b1e889508a7296bbac8e9f656b2b15511261ea6c5fcc7

Amended pre-signature counter-check SHA-256:
ed80096f5fdd576d56ff7b40cf4eb4570e333a23864a084077dc743e4e064087
```

Candidate, O13F_01 e O13F_02 sono invariati. Gli ultimi due ballot correnti
sono byte-identici ai rispettivi commit di firma.

## 5. Recheck sostanziale

| Controllo | Esito |
|---|---|
| support inheritance assente | CLEAN |
| ogni metrica normativa mantiene policy e `G_defined` propri | CLEAN |
| TP/FP/FN, precision/recall/F1 restano mandatory con policy propria | CLEAN |
| `definition_key` + `evaluation_scope_id` separano identità e scope | CLEAN |
| selector exact-key richiesto, nessun parsing dello `stratum_id` | CLEAN |
| split input legato a index, manifest, annotation e frontend lock | CLEAN |
| source-family complete per il ceiling | CLEAN |
| minimo tre family×cinque non irrigidito a tutte le family | CLEAN |
| electronic-stratified 2/5 rappresentato come precondizione relazionale | CLEAN |
| policy v3 invariata, nessuna policy v4 | CLEAN |
| direct caller templates esclusi dal path ufficiale | CLEAN |
| registry digest richiesto nel futuro report/activation envelope | CLEAN |
| metric ID/data/power plan non materializzati dal ballot | CLEAN |
| p95, coverage_plus, duration/occupancy restano diagnostici | CLEAN |
| supporto interno Spearman non convertito in floor di gruppi | CLEAN |
| REV8 SPEC GO resta NO | CLEAN |

## 6. Test e regressione

```text
Full ml_v3 unittest suite: 641/641 PASS
git diff --check:         PASS
Markdown fences:          PASS
protected code diff:      ZERO
signed commit scope:      un solo file Markdown
worktree pre-report:      CLEAN
```

Comando suite:

```text
PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest discover \
  -s ml_v3/tests -t . -p 'test_*.py' -q
```

## 7. Stato risultante

```text
O13F_03 architecture authority = FIRMATA E POST-SIGNATURE CLEAN
Registry schema/code           = NON ANCORA AUTORIZZATO
Registry data artifact         = NON MATERIALIZZATO
Official policies              = NON MATERIALIZZATE
REV8 dispatcher                = INVARIATO
REV8 SPEC GO                   = NO
G1c close / G1 PASS            = NO
```

L'authority O13F_03 è ora consumabile dal prossimo freeze documentale. Non
autorizza un builder a inventare selector, precondition schema o metric ID.

## 8. Next permitted action

Preparare, senza attivazione e senza policy ufficiali:

1. selector companion schema exact-key;
2. registry-precondition companion schema exact-key;
3. catalogo completo dei metric ID e `definition_key`;
4. matrice di tracciabilità candidate-clause → metric/scope/split;
5. primo registry data artifact con binding agli input congelati.

I cinque elementi devono formare una nuova unità docs/data di freeze,
counter-check e firma. Soltanto dopo quel freeze sarà autorizzabile
l'implementazione del registry path ufficiale.
