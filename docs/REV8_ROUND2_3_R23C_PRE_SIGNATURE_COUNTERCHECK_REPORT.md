# REV8 — ROUND 2.3 — R23C PRE-SIGNATURE COUNTER-CHECK REPORT

**Verdetto consolidato:** `CLEAN`

## 1. Target verificato

```text
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md

SHA-256 iniziale:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

SHA-256 finale:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

Il target non è stato modificato da alcun reviewer.

```text
REV8 SPEC GO = NO
Candidate patch = NON AUTORIZZATA
Implementation  = NON AUTORIZZATA
Training        = NON AUTORIZZATO
```

## 2. Provenance

```text
Predecessore R23A:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011

Ballot R23A firmato:
4db761eeb017cd04595a1ac64943b535809675adefba9fe3d5cf8083336c8666

Recheck report R23A:
0e4a92f1cfa2c11193a3f3dbb1f3f5809b743486509ba469ce5f08477467f44d
```

## 3. Esiti indipendenti

| Lens | Scope | Verdetto finale |
|---|---|---:|
| Lens A | Metrics / macro-AP | CLEAN |
| Lens B | Semantics / metric fields vs replay | CLEAN |
| Lens C | Optimizer / integrazione / governance | CLEAN |

Non è stato usato voto di maggioranza. Prima del giro finale la Lens B aveva
richiesto due qualifiche non scientifiche:

1. consentire i valori esplicitamente replay-only e diagnostici;
2. limitare l’invarianza del fixture agli output scientifici, metriche
   normative e gate.

Le qualifiche sono state applicate e tutte le lenti hanno verificato il nuovo
SHA immutabile.

## 4. RC-001 — chiusura verificata

La norma ora distingue:

```text
sum_pairwise64 = fase di somma interna
mean64         = aggregatore finale di media
macro-AP       = mean64(AP_group64 ordinati per group_id)
```

Golden riprodotto:

```text
AP_group64 = [0.2,0.3,1.0]

sum-only mutation:
1.5 = f64:3ff8000000000000

mean64 normativo:
0.5 = f64:3fe0000000000000
```

§2.3, §12, §13, §14.1 e §14.6 risultano coerenti.

## 5. RC-002 — chiusura verificata

La norma ora separa:

```text
matching scientifico:
eligibility, V*, M*, S, S_can

replay diagnostico:
D, M_replay

usi metrici espliciti:
severity -> §11.2
confidence -> formazione P_t in §13
```

`D` e `M_replay` non selezionano pairing scientifici, envelope, metriche
normative o gate. Possono produrre soltanto valori esplicitamente etichettati
replay-only e diagnostici.

Fixture verificate:

```text
severity_P 0 -> 1:
Q_severity 0 -> 1
V*, M*, S_can invariati

confidence TP>FP -> AP=1
confidence FP>TP -> AP=1/2

tie K6:
V*, M*, S_can invariati
M_replay può cambiare
output scientifici e gate invariati rispetto alla scelta di replay
```

## 6. Esito

```text
RC-001 = CLEAN
RC-002 = CLEAN
```

Il documento è tecnicamente pronto per un ballot esterno R23C che vincoli
l’esatto SHA verificato. Questo esito non costituisce `REV8 SPEC GO` e non
autorizza implementazione o patch del candidate.
