# REV8 — O-13 policy materialization readiness

## Stato

```text
Branch                         = feature/motore-v3-rev8-spec-go
Base implementation commit     = f277f091
O13F_01 authority              = APPROVATA, FIRMATA, RECHECK CLEAN
Support-floor evaluator         = IMPLEMENTATO, candidate-only
Official support-floor policy   = NON MATERIALIZZABILE OGGI
REV8 dispatcher activation      = NO
REV8 SPEC GO                    = NO
```

Questo report distingue la disponibilita dell'infrastruttura dalla
disponibilita dell'evidenza necessaria per produrre una policy scientifica
attiva. L'assenza dell'artefatto ufficiale non autorizza valori sostitutivi,
floor abbassati o promozione di fixture.

## Evidenza disponibile

Il repository contiene un solo esempio per ciascuna delle tre famiglie di
input necessarie:

```text
ml_v3/fixtures/g1/examples/benchmark_power_plan.json
ml_v3/fixtures/g1/examples/asset_manifest.json
ml_v3/fixtures/g1/examples/annotation.json
```

Digest osservati:

```text
benchmark_power_plan.json
  28a623579e90c9aeee0c3f7f66d02d6d126daed759b230f406b9cc054c2c8a5a

asset_manifest.json
  2b1feade0aae66d6d104e994511f2f482445f819d124a49275b9808708b4af1a

annotation.json
  2692f6cb9b0037200554483c99fdfb80d868091bee710c78501c330ace2aa329
```

Il power plan di esempio dichiara esplicitamente:

```text
pilot_sha256 = aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
support.notes = "fixture"
```

I valori `n_power` presenti servono ai test G1a dello schema. Non provengono
da un pilot reale congelato e non costituiscono autorita scientifica O-13.

## Artefatti mancanti per una policy ufficiale

Prima di produrre un digest di attivazione servono tutti i seguenti elementi:

1. `benchmark_power_plan.json` reale, validato e congelato, derivato soltanto
   da `development_pilot`;
2. manifest/GT ammessi per lo split destinatario;
3. population plan completo per `(metric_id, stratum_id)`, congelato prima di
   leggere le prediction;
4. mapping non ambiguo fra ogni strato power-governed e la riga `gate` o
   `family` del power plan che fornisce `n_power`;
5. digest attesi approvati per power plan, population plan e policy finale.

In assenza di uno solo di questi elementi, l'esito normativo resta:

```text
SUPPORT_POLICY_UNAVAILABLE
metric gate = N/A
PASS = impossibile
```

## Prossima tranche consentita

E consentito implementare, in isolamento REV8 e senza dispatcher switch, un
compiler fail-closed che:

- valida il power plan esistente con lo schema congelato;
- confronta il suo digest con un digest atteso esterno;
- lega ogni strato a una sola riga `gate` o `family`;
- calcola `n_required = max(contract_floor, n_power)`;
- verifica il digest atteso del population plan;
- emette policy canonica e digest senza dichiararli attivi;
- rifiuta binding mancanti, duplicati o ambigui.

Il compiler non deve:

- usare automaticamente gli esempi G1;
- inventare `n_power`;
- derivare il digest atteso dall'input non fidato e chiamarlo authority;
- leggere prediction;
- esportare simboli dal dispatcher REV7;
- produrre REV8 SPEC GO.

## Condizione di chiusura della materializzazione

La policy ufficiale potra essere generata soltanto quando i cinque artefatti
sopra saranno presenti e firmati. Fino ad allora, il risultato di questa
tranche e infrastruttura verificabile, non evidenza acquisita.
