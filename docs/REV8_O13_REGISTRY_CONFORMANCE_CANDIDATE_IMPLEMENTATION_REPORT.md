# REV8 O13F06 — Registry conformance candidate implementation report

## Stato

```text
Branch                         = feature/motore-v3-rev8-spec-go
Base immutabile               = f43f343171ba86a04e676b950eefb010742ec7a1
Authority                     = O13F06 signed package
Implementation                = candidate-only
Activation / dispatcher       = NON MODIFICATI
Runtime / training / plugin   = NON MODIFICATI
REV8 SPEC GO                  = NO
G1c close / G1 PASS           = NO
Push                          = NO
```

Questo report chiude esclusivamente la tranche candidate-only autorizzata dal
ballot O13F06. Non promuove i fixture di conformance ad artefatti scientifici e
non autorizza l'uso del registry nel percorso attivo.

## File implementati

```text
ml_v3/contracts/registry_conformance_v2.py
SHA-256 20c3239501280f27a96c6d7f935fd5d6da2aae18835467690a9765d5a06b15f1

ml_v3/tests/test_g1c_rev8_registry_conformance_v2.py
SHA-256 34ee8aab3c18eb2e20c254817608716eaf479b2b7d770c072c098c0fab6719e5
```

Il modulo candidate resta intenzionalmente assente da
`ml_v3/contracts/__init__.py`.

## Funzioni materializzate

1. Loader fail-closed del package firmato:
   - manifest SHA-256 esterno pinnato;
   - insieme file esatto;
   - digest di ogni artefatto;
   - JSON strict e byte canonici.
2. Expander deterministico `definition + claim plan + input facts + readiness`.
3. Oracle comparison object-exact e canonical-byte-exact.
4. Validator semantico di definition, claim plan, precondition schema,
   readiness e delle tre istanze.
5. Mutation runner eseguibile per tutte le 31 recipe firmate.
6. Protezione post-load dei companion artifact non direttamente bersagliati
   dalle mutation recipe.

## Oracle positivo

La ricostruzione indipendente riproduce esattamente le tre istanze congelate:

```text
calibration
7ec116138426c3613c3fbb28f9c5805f7e50cdbe9428d36516d6b4996c3af61f

development-metric
587c5a4e4e97f6db392f5a6f1615302a4b5aca5d5a21767c9b369d2e3bc087c3

final-test
75de3d0e6aea3bce596a4ffc9947bfc012af61dd3f2177bb03977e6ef0c77bf3
```

Per ciascuna istanza sono identici sia l'oggetto sia i byte JSON canonici.

## Oracle negativo

```text
Mutation recipe firmate       = 31
Reason code attesi osservati  = 31/31
Mutation accettate            = 0
```

Sono inclusi i reject path per promotion fixture→scientific/official,
dipendenze readiness, completezza delle expansion elettroniche, identità UTF-8
senza normalizzazione Unicode, power binding, precondition selector semantics e
versioni schema.

## Verifiche

Ambiente canonico:

```text
/Users/marco/aieq_data/motore_v3/env/venv/bin/python
CPython 3.12.13
```

Risultati finali:

```text
Test tranche mirati  = 16/16 PASS
Suite ml_v3 completa = 657/657 PASS
Tempo suite completa = 85.093 s
git diff --check     = PASS
```

Il precedente tentativo con CPython 3.14.4 è stato scartato perché il gate
platform richiede correttamente CPython 3.12.13; non è usato come evidenza.

## Counter-check conclusivo

Il counter-check ha verificato:

- nessuna derivazione di digest atteso da input non fidato;
- nessuna discovery implicita di file scientifici;
- provenance hashata anche quando l'ordine dei domini non altera le righe
  espanse;
- conservazione distinta di `e\u0301` ed `é`;
- nessuna modifica in-place del package pristine durante le mutation;
- reason code specifici emessi prima del byte-oracle generico;
- companion artifact respinti se mutati dopo il caricamento;
- assenza di export/activation del modulo candidate.

Esito:

```text
O13F06 candidate implementation = CLEAN
Commit eligibility              = YES
Activation eligibility          = NO
```

## Stop rule

La tranche termina con il commit atomico di questo report, del modulo e dei
test. Ulteriore documentazione O13F06 è vietata salvo un difetto riproducibile
nel codice, un reason-code mismatch, un oracle mismatch o una regressione della
suite. Il prossimo lavoro deve avanzare l'implementazione di G1c, non riaprire
il decision engineering già firmato.
