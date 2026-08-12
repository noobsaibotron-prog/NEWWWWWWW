# REV8 — CANDIDATE B-001 — COVERAGE MICRO-AMEND BALLOT

**Stato:** `SIGNED — APPROVED`

## 1. Oggetto

Questo ballot chiude esclusivamente B-001 del counter-check sul candidate:

```text
Target candidate:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md

Commit target:
4ca81601aeb2a80376c619f5c5128cf076006fe3

SHA-256 target:
d6497a39a6657f70445d163270b4d2eba5cf808775d5a9c582d7f49e7a8cec9b

Counter-check:
docs/REV8_CANDIDATE_R23C_COUNTERCHECK_REPORT.md

SHA-256 counter-check:
98c799b7a52e4e414675e2f6704536d5030ef32ee88e05ab1bcc51ec9ed9bb83

Verdetto:
BLOCK — B-001
```

La decisione non modifica R23C, K1–K4, `M*`, K6, `S_can`, `M_replay`, le
metriche anomaly o la clean actionable rate. Definisce soltanto la coverage
target-specific che consuma `actionable` dopo il matching.

## 2. Decisione B-001

Per ogni evaluation unit `u`, siano `q` tutte le partizioni:

```text
(evaluation_unit_key=u,
 record_family="semantic_region",
 problem_type)
```

e sia:

```text
M_u* = CartesianProduct(M_q* per tutte le partizioni q di u)
```

Sia `G_A(u)` l'insieme delle semantic region GT strutturalmente valide
dell'unità `u` con `actionable=true`.

Per `M in M_u*`:

```text
covered(g,M) = 1
  se esiste una semantic_bundle prediction p tale che:
    (g,p) appartiene a M
    e actionable(p)=true
  altrimenti 0

C_u(M) =
  sum_{g in G_A(u)} covered(g,M) / |G_A(u)|
```

La metrica scientifica primaria e di gate è:

```text
coverage_minus(u) = min_{M in M_u*} C_u(M)
```

La coverage usa quindi il lower envelope conservativo su tutti i matching
scientificamente ottimi.

## 3. Semantica N/A e diagnostica

```text
|G_A(u)| = 0
  -> N/A / NO_ACTIONABLE_GT

lower envelope non calcolabile esattamente entro i cap attivi
  -> N/A / PAIRING_ENVELOPE_UNAVAILABLE

coverage_plus(u) = max_{M in M_u*} C_u(M)
  -> diagnostica soltanto

coverage calcolata su M_replay
  -> vietata per metriche normative e gate
```

Se la coverage è obbligatoria, ciascun N/A sopra impedisce PASS. Supporto ed
esclusioni N/A devono essere pubblicati.

## 4. Aggregazione

`C_u(M)` è un razionale esatto. `coverage_minus(u)` viene arrotondata una sola
volta a binary64 al boundary di pubblicazione. Le riduzioni successive seguono
la gerarchia `mean64` già firmata:

```text
evaluation unit -> group_id -> macro fra gruppi
```

Ogni gruppo riceve peso totale uno. La sola somma grezza non è una media.

## 5. Separazione scientifica

La decisione non permette ad actionable di retroagire su:

```text
eligibility
V*
M*
S
S_can
```

Actionable è consumato soltanto dalla formula coverage firmata sopra e dalle
metriche tonali preesistenti che lo dichiarano esplicitamente.

`D` e `M_replay` restano replay-only. ID, hash, severity e confidence non
possono scegliere il valore di coverage.

## 6. Motivazione

Il lower envelope:

- impedisce a un matching diagnostico favorevole di produrre un false PASS;
- conserva il vincolo one-to-one;
- impedisce che una singola prediction actionable copra artificialmente più
  target tramite un esistenziale sugli archi;
- resta indipendente da `D`, `M_replay`, ID e hash;
- è coerente con gli envelope conservativi già usati per severity, onset e
  offset.

## 7. Fixture e mutation obbligatorie

```text
unique actionable match
  -> coverage_minus = 1

only non-actionable match
  -> coverage_minus = 0

ambiguous pF/pT con stesso V* e S_can
  -> coverage_minus = 0
  -> coverage_plus = 1, diagnostica

|G_A| = 0
  -> N/A / NO_ACTIONABLE_GT

envelope oltre cap
  -> N/A / PAIRING_ENVELOPE_UNAVAILABLE
  -> nessun PASS
```

Property test obbligatori:

```text
permutazione input                    -> invariance
rinomina ID/hash                      -> invariance
modifica del solo M_replay            -> nessun effetto scientifico
replica su almeno 30 group_id         -> stesso lower envelope gate-relevant
```

Mutation da uccidere:

```text
usare max_{M in M*} C_u(M) come gate
usare coverage(M_replay)
usare un esistenziale prediction-level che ignora il matching one-to-one
permettere ad actionable di influenzare eligibility o K1–K4
convertire N/A in zero o PASS
```

## 8. Scope della firma

La firma:

- approva atomicamente la decisione B-001 e le relative fixture;
- autorizza una patch document-only del candidate su un nuovo commit;
- richiede un counter-check mirato sul nuovo SHA;
- non autorizza codice, artefatti O-02/O-03/O-18, benchmark O-09, runtime o
  training;
- non costituisce `REV8 SPEC GO`.

## 9. Firma dell'autorità

```text
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-07-29

Policy firmata:
coverage_minus = min_{M in M*} coverage(M)
```

## 10. Stato dopo la firma

```text
B-001 coverage policy      = FIRMATA
Candidate patch            = AUTORIZZATA, NON ANCORA APPLICATA
Counter-check nuovo SHA    = OBBLIGATORIO
REV8 SPEC GO               = NO
Implementation/training    = NON AUTORIZZATI
```
