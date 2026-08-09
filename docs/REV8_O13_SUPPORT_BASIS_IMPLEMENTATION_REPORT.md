# REV8 — O-13 — SUPPORT BASIS IMPLEMENTATION REPORT

**Stato:** `IMPLEMENTED_CANDIDATE_ONLY_REGISTRY_PENDING`

Questo report riguarda esclusivamente l'implementazione O13F_02 sul ramo
candidate. Non dichiara O-13 chiuso, non materializza policy ufficiali e non
autorizza REV8, runtime o training.

## 1. Authority consumata

```text
Ballot O13F_02 firmato:
docs/REV8_O13_SUPPORT_BASIS_BALLOT_DRAFT.md

Commit firma:
26f35e753f96ebc48d0453e432e8f2a3f5380687

Post-signature recheck:
docs/REV8_O13_SUPPORT_BASIS_POST_SIGNATURE_RECHECK_REPORT.md

Commit recheck:
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e
```

## 2. Commit implementativo

```text
2b57493917ead1074de8b90da5907790564cc34d
feat(rev8): enforce signed O-13 support basis
```

File modificati:

```text
ml_v3/contracts/support_floor_v2.py
ml_v3/contracts/support_floor_policy_v2.py
ml_v3/tests/test_g1c_rev8_primitives_v2.py
ml_v3/tests/test_g1c_rev8_support_floor_policy_compiler.py
```

Digest post-commit:

```text
support_floor_v2.py
4361976f0a0c20b2f2a6ca6419c7a0cfe58b5fc1f9ddfd7e98114ee34fb556c0

support_floor_policy_v2.py
bfb14257fd85628633425d8e667f5b9e7a016b4b2a72cbe6fbd6c2c3d9371aed
```

## 3. Comportamento materializzato

Lo schema policy candidate è ora:

```text
aieq-v3-rev8-o13-support-floor-policy-3
```

Ogni strato richiede esplicitamente uno dei due letterali:

```text
eligible_and_defined
eligible_only
```

Non esiste default nel tipo di produzione. `defined_only`, campo mancante,
campo extra o schema policy v2 vengono rifiutati.

Per policy `mandatory=true`, il root `all_eligible_groups` deve usare
`eligible_and_defined`. Il compiler copia il basis dal template nella policy;
il digest della policy cambia se il basis cambia e l'evaluator rifiuta una
modifica rispetto all'hash atteso.

Per `eligible_only`:

```text
eligible_minimum_ok = bool
eligible_ceiling_ok = bool
defined_minimum_ok  = null
defined_ceiling_ok  = null
```

I conteggi defined restano pubblicati per audit, ma il lato inattivo non crea
reason code e non partecipa a `support_sufficient`.

Per `eligible_and_defined`, entrambi i lati restano booleani attivi. La
formula `n_required=max(contract_floor,n_power)` è invariata.

## 4. Spearman

La fixture usa `INSUFFICIENT_MATCHED_SUPPORT` come reason code di un gruppo
N/A e verifica che:

- il requisito interno `n>=10` matched pairs cambia l'appartenenza a
  `G_defined`;
- il floor O-13 resta `30 group_id` nell'esempio anomaly;
- `30 eligible / 29 defined` è insufficiente;
- non viene creato un floor unsigned di dieci gruppi.

## 5. Fixture fail-closed

Sono coperti almeno:

- root mandatory `eligible_only` rifiutato;
- basis mancante o sconosciuto rifiutato;
- policy v2 rifiutata senza fallback;
- child corpus `eligible_only` con defined insufficiente ma eligible
  sufficiente;
- stessa fixture mutata a `eligible_and_defined` insufficiente;
- ceiling eligible attivo e ceiling defined inattivo;
- campi inattivi preservati come `None`/`null`;
- alterazione post-compilazione del basis rilevata dall'hash;
- ordine template/popolazioni non autoritativo;
- isolamento dal dispatcher REV7.

## 6. Test

Interprete:

```text
/Users/marco/aieq_data/motore_v3/env/venv/bin/python
PYTHONDONTWRITEBYTECODE=1
```

Risultati:

```text
Support-floor mirati pre-commit: 43/43 PASS
REV8 rilevanti pre-commit:       255/255 PASS
Suite completa pre-commit:       641/641 PASS
REV8 rilevanti post-commit:      255/255 PASS
```

Il commit implementativo contiene soltanto i quattro file elencati. Il diff
verso `Source`, `CMakeLists.txt`, `Resources`, `ml_v2`, `ml` e `training` è
zero. Nessun simbolo support-floor è esportato da
`ml_v3/contracts/__init__.py`.

## 7. Limite residuo dichiarato

Il metric/stratum registry firmato non esiste ancora. Il compiler candidate
riceve template espliciti e la policy risultante è hash-bound, ma non può
dimostrare da solo che una riga `eligible_only` appartenga all'allowlist
scientifica globale finché tale registry non viene materializzato e congelato.

Conseguenze:

```text
schema v3 candidate             = IMPLEMENTATO
policy ufficiali                = NON MATERIALIZZATE
allowlist registry-bound        = PENDING
support-basis activation        = NO
```

Questo limite non apre un fallback nel runtime perché il modulo resta non
esportato e nessuna policy è attiva. È il prossimo gate, non una proprietà
implicitamente soddisfatta da questo commit.

## 8. Prossima azione

Preparare un metric/stratum registry docs/data-only che congeli, per ogni riga:

```text
metric_id
split_role
stratum_id
population_kind
parent_stratum_id
support_basis
contract_floor
power binding
parent-fraction ceiling
mandatory status
```

Il registry dovrà avere schema exact-key, canonical bytes, digest atteso e
fixture che confrontino registry, template e policy. Soltanto dopo quel freeze
potranno essere materializzate policy O-13 ufficiali.

## 9. Stato finale

```text
O13F_02 authority              = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_02 schema/compiler code   = CANDIDATE IMPLEMENTATO
Metric/stratum registry       = NON ANCORA CONGELATO
Official policy materialize   = NO
REV7 dispatcher               = INVARIATO
O-13 close                    = NO
REV8 SPEC GO                  = NO
G1c close / G1 PASS           = NO
Runtime / training / push     = NON AUTORIZZATI
```
