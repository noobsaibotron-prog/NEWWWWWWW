# REV8 — O13F_05 — PRE-SIGNATURE COUNTER-CHECK

## Verdetto

```text
Verdetto                         = PRE_SIGNATURE_CLEAN
Data                             = 2026-08-10
Branch                           = feature/motore-v3-rev8-spec-go
Base HEAD                        = 1275dbbd29a232dadad1729fa9e5605fba2402ce
O13F_04 authority                = SIGNED + POST_SIGNATURE_CLEAN
O13F_05 decision                 = NON FIRMATA
Independent counter-check lenses = 3/3 CLEAN sui byte finali
Full unittest suite              = 641/641 PASS
Protected code diff              = ZERO
REV8 SPEC GO                     = NO
```

Questo report verifica il ballot O13F_05 prima del freeze immutabile. Non
costituisce firma e non autorizza schema, data artifact o codice.

## 1. Artefatti verificati

```text
Representability audit:
docs/REV8_O13F05_REGISTRY_REPRESENTABILITY_AUDIT.md
SHA-256:
3298d1575420e02b39c1370c980a54401834b94e273bace485e836ecd6644145

Ballot O13F_05:
docs/REV8_O13_NON_OUTPUT_SUBJECT_BREAKDOWN_CLAIM_PLAN_BALLOT_DRAFT.md
SHA-256:
984a1824327e3b57842a937ac8edaf44436a92380ce1f206c01cbbb7c19c2cac

Candidate REV8:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_04 signed ballot:
docs/REV8_O13_REGISTRY_DEFINITION_INSTANCE_SPLIT_BALLOT_DRAFT.md
SHA-256:
7202df3db4b461480f9bbdc63d05a82d413e34d4bdab91df2157914686850344
```

## 2. Metodo

Sono state usate tre lenti indipendenti e una verifica locale finale:

1. **metric/status/power lens** — catalogo, split, status, Holm e power design;
2. **population/semantics lens** — calibration, electronic subgenre,
   denominator, readiness e population membership;
3. **schema/optimizer lens** — exact-key, referential graph, canonical order,
   power relocation e cross-instance evidence;
4. **root verification** — confronto integrale dei documenti, hash, regressione
   e protected diff.

Ogni lente ha inizialmente restituito `BLOCK` su problemi concreti. Le
correzioni sono state applicate al ballot, poi ciascuna lente ha riletto i byte
finali e restituito `CLEAN`.

## 3. Finding chiusi

### 3.1 Calibrazione

Chiuso:

- nessuna metrica ECE/Brier/AP inventata sullo split `calibration`;
- quattro subject non metrici per tonal e classi anomaly;
- binding/readiness con esiti READY/NOT_READY/FAIL;
- dependency graph esplicito verso metric binding D/F;
- dependency espansa nello split consumer;
- SHA dell'instance calibration e dei readiness results hash-bound nel
  consumer;
- stessa purpose fixture→fixture e official→official;
- propagazione N/A/no PASS soltanto ai binding dipendenti.

### 3.2 Electronic stratification

Chiuso:

- `electronic_subgenre` distinto da `source_family`;
- dominio `string|null` preservato dal manifest firmato;
- nessuna normalizzazione/alias nascosto;
- group con più valori presente in più breakdown ma contato una sola volta nel
  parent;
- denominator 2/5 uguale a tutti i group ID ammessi e congelati final-test,
  prima di eligibility metric-specific;
- parent readiness subject mandatory che blocca il PASS final-test;
- breakdown population derivata pre-prediction da
  `G_eligible ∩ electronic-stratified ∩ subgenre`;
- `G_defined` proprio calcolato soltanto dopo l'evaluation;
- bijection definition→claim-plan row final-test;
- breakdown diagnostico incapace di gate/claim/power;
- futura claim subordinata a nuova definition e claim plan firmati.

### 3.3 Catalogo

Chiuso:

- matrice esatta di 29 misure semantiche;
- espansione riproducibile a 164 metric definitions;
- scope, record family, problem type, publication status, support mode e split
  congelati;
- ECE/Brier classificati `mandatory_normative/own_policy`;
- p95, coverage_plus, durata e occupancy come soli diagnostici congelati;
- nessun output metric binding su calibration.

### 3.4 Claim plan, Holm e power

Chiuso:

- claim-plan overlay separato dalla definition e dall'instance;
- stessa Holm family D/F, enumerazione firmata e ordinamento operativo per
  p-value crescente con tie-break firmato;
- power-design rule ID preregistrato prima del pilot;
- sole quattro formule candidate §11.2 ammesse;
- gate/family/null power binding non deciso dal power plan osservato;
- ogni primary metric ha root governing strata `gate|family`;
- `n_power` deve entrare in `n_required=max(contract_floor,n_power)`;
- child `null` ammessi soltanto se non governanti e motivati.

### 3.5 Exact-key e canonicalizzazione

Chiuso:

- schema v2 per definition/instance, v1 superseded-before-materialization;
- top-level exact keys completi di definition, claim plan e instance;
- exact keys completi per concrete strata, metric/calibrator/readiness binding,
  dependency e power row;
- power fields rimossi dagli strata e uniti in modo non ambiguo tramite
  template/blueprint/concrete stratum ID;
- ogni array e lista ID con ordine canonico esplicito;
- precondition subject discriminati;
- closure e cardinalità per metriche, calibratori, readiness e breakdown.

## 4. Esiti finali delle tre lenti

```text
countercheck_metrics   = CLEAN
countercheck_semantics = CLEAN
countercheck_optimizer = CLEAN
```

Gli esiti sono riferiti al ballot SHA-256:

```text
984a1824327e3b57842a937ac8edaf44436a92380ce1f206c01cbbb7c19c2cac
```

Non sono trasferibili a byte successivi senza un nuovo controllo di hash/diff.

## 5. Regressione e perimetro

Comando:

```text
PYTHONDONTWRITEBYTECODE=1 \
  /Users/marco/aieq_data/motore_v3/env/venv/bin/python -B -m unittest discover \
  -s ml_v3/tests -t . -p 'test_*.py'
```

Esito:

```text
Ran 641 tests in 84.979s
OK
```

Controlli aggiuntivi:

```text
git diff --check                                  = PASS
Markdown fences audit/ballot                      = PASS
candidate SHA                                     = MATCH
O13F_04 signed SHA                                = MATCH
diff vs base in Source/CMake/Resources/ml_v2/ml/
training/ml_v3                                    = ZERO
new working-tree files                            = 3 docs-only dopo questo report
push                                              = NO
```

## 6. Limiti del verdetto

`PRE_SIGNATURE_CLEAN` significa soltanto che il ballot può essere congelato
e sottoposto a Marco. Non significa:

- O13F_05 firmata;
- package schema/data autorizzato;
- claim plan scientifico deciso;
- official registry instance disponibile;
- validator code autorizzato;
- REV8 SPEC GO;
- G1c close o G1 PASS.

## 7. Next permitted action

```text
commit atomico docs-only del freeze pre-firma
  -> mostrare commit e SHA immutabili
  -> firma esplicita Marco
  -> commit firma limitato ai campi ballot
  -> post-signature recheck
```

