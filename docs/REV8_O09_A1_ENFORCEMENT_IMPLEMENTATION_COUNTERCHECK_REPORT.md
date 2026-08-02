# REV8 O-09 A1 — Enforcement implementation counter-check report

**Verdetto:** `3/3 CLEAN`

**Quattro ceiling A1 per-sottografo:** `MATERIALIZZATI NEL CANDIDATE ISOLATO`

**Cap group-level:** `NON ATTIVI`

**REV8 SPEC GO:** `NO`

## 1. Snapshot verificato

```text
Final implementation commit:
733c5f5c66c9160935b57135094567a50718196a

Intermediate enforcement commit:
b61ac6ad8de4b5ad8ec5fa7969f266902ab0d97d

Intermediate corrective commit:
6d5c12475310a0f79deea38b0b2169b2a8105c08

Signed A1 ballot:
docs/REV8_O09_ACTIVATION_BALLOT_A1_SIGNED.md
SHA-256 bf31ed0e1bbcfa001114808e569a5155427b54eef3b9dd1cd0d80fcd1c184d75

A1 post-signature authority report:
docs/REV8_O09_A1_POST_SIGNATURE_RECHECK_REPORT.md
SHA-256 04f562016facf074b229fe1381aa988cb3495f4b85dcda66d40dd0101856a092
```

Il report post-firma precedente registrava correttamente l'enforcement come
`NON MATERIALIZZATO` al proprio freeze. Questo documento non riscrive quella
storia: registra la tranche implementativa successiva sul commit finale sopra.

## 2. File e hash finali

```text
ml_v3/benchmark/rev8_o09_candidate.py
SHA-256 db8488d15ad3f30d4aa7bb977464081aac24583bbadeeb17a2184a3995348a4e

ml_v3/benchmark/run_rev8_o09_candidate.py
SHA-256 3030e1f1aecab0446e5f2b60b29f5153743e900a231101c4b52a7fd86918314c

ml_v3/tests/test_g1c_rev8_o09_candidate.py
SHA-256 f7e97c5b1f13fed0e55f6e45fbeb9bc4fd4e7d370ada3175f83d28ce0d23e414
```

## 3. Enforcement materializzato

La tranche applica prima di ogni solve i soli quattro ceiling firmati A1:

```text
GT                               <= 128
prediction                       <= 128
eligible edges                   <= 16384
reduced mathematical bit-length  <= 65536
```

Le superfici candidate isolate sono:

```text
evaluate_a1_preflight_fail_closed
evaluate_a2_fail_closed
evaluate_a2_batch_fail_closed
```

Le proprietà verificate sono:

1. preflight A1 eseguito prima di qualsiasi solve;
2. exceed strutturale -> `REJECTED / SOLVER_STRUCTURAL_LIMIT_EXCEEDED`;
3. OOM, timeout, runtime e overflow ->
   `REJECTED / SOLVER_RUNTIME_FAILURE`;
4. modello invalido -> `REJECTED / SOLVER_CONSTRAINT_MODEL_INVALID`;
5. nessun valore scientifico su qualunque rejection;
6. batch: tutti i preflight precedono il primo solve e ogni failure fatale
   elimina i valori parziali;
7. successo tecnico = `EVALUATED`, mai `PASS` o `CERTIFIED`;
8. `a2_exact` resta una primitiva unchecked fuori da `__all__`: è chiamata
   internamente dai due wrapper enforced dopo un preflight A1 riuscito; al di
   fuori di tali wrapper non esistono call-site scientifici non-test;
9. il runner A2 usa esclusivamente la superficie fail-closed;
10. ogni solve K1 del workload AP-prefix passa dallo stesso preflight A1.

## 4. Provenance del runner v5

Il runner emette:

```text
schema = aieq-v3-rev8-o09-candidate-benchmark-5
root authority_status =
  A1_PER_SUBGRAPH_CAPS_ACTIVE_GROUP_CAPS_NOT_ACTIVE

A2/AP-prefix preflight authority_status =
  A1_ACTIVE_ENFORCEMENT
```

Il timer A2 comprende il preflight attivo e il solve esatto. Il payload non
presenta più la vecchia etichetta `EVIDENCE_ONLY_CAPS_NOT_ACTIVE` per il nuovo
schema v5. Il risultato resta evidenza candidate e non costituisce un PASS.

## 5. Counter-check indipendenti

Tre lenti separate hanno letto lo stesso commit finale `733c5f5c...` senza
modificare file.

### 5.1 Metriche e fail-closed

`CLEAN`. Boundary under/on/over, preflight AP-prefix, rejection prima del K1,
assenza di valori parziali e propagazione batch risultano coerenti.

### 5.2 Optimizer e bypass

`CLEAN`. Nessun call-site scientifico in-tree salta A1. Il solver unchecked è
fuori dall'API esportata; runner A2 e AP-prefix usano le superfici enforced.
Formula §2.2, determinismo e wording del timer risultano coerenti.

### 5.3 Semantica, authority e scope

`CLEAN`. Status root/workload/AP-prefix sono coerenti. OOM, timeout e modello
invalido hanno tassonomia fail-closed. REV7 e group candidate restano isolati;
non compare alcun PASS o REV8 SPEC GO implicito.

## 6. Finding chiusi durante il counter-check

| Commit attaccato | Finding | Chiusura nel commit finale |
|---|---|---|
| `b61ac6ad` | eccezioni del preflight fuori dalla traduzione fail-closed | helper A1 centrale con mapping reason/status |
| `b61ac6ad` | `TimeoutError` non classificato | incluso fra i runtime failure fatali |
| `b61ac6ad` | runner A2 chiamava direttamente `a2_exact` | runner migrato a `evaluate_a2_fail_closed` |
| `6d5c1247` | root payload dichiarava ancora cap inattivi | status v5 coerente e distinto dai group cap |
| `6d5c1247` | docstring timer dichiarava solve-only | wording corretto: preflight + solve |
| `6d5c1247` | AP-prefix chiamava K1 senza preflight enforced | preflight A1 prima di ogni prefisso |

## 7. Test finali

```text
Interprete canonico:
/Users/marco/aieq_data/motore_v3/env/venv/bin/python

A1EnforcementTests + BenchmarkRunnerTests:
36/36 PASS, exit code 0

Counter-check mirati indipendenti:
44/44 PASS (risultato riferito dalle lenti read-only)

Suite ml_v3/tests:
558/558 PASS, exit code 0

git diff --check:
PASS

Protected Source/CMake/Resources/ml_v2 diff vs 7e23f1c3:
0
```

## 8. Limiti invariati

```text
17 cap group-level                  = NON ATTIVI
group candidate ballot_ready        = false
Spearman general variable-marginal  = NON CERTIFICATO
G_eligible/G_defined/G_NA e O-13    = NOT_EVALUATED
runtime/training                    = NON AUTORIZZATI
G1c completo                        = NO
REV8 SPEC GO                        = NO
push                                = NON ESEGUITO
```

## 9. Esito della tranche

Sul commit `733c5f5c66c9160935b57135094567a50718196a`, l'enforcement dei
quattro ceiling A1 è materializzato e controverificato nel candidate isolato.
Questo chiude esclusivamente il debito implementativo per-sottografo indicato
dal report post-firma. Non attiva i cap group-level e non autorizza il passaggio
a runtime, training o REV8 SPEC GO.
