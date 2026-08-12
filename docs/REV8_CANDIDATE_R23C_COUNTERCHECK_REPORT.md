# REV8 — CANDIDATE R23C PATCH — COUNTER-CHECK REPORT

## 1. Target verificato

```text
Branch:
feature/motore-v3-rev8-spec-go

Commit:
4ca81601aeb2a80376c619f5c5128cf076006fe3

Parent:
5b919c44541ae1248392921ac3d3db48f602f75a

File:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md

SHA-256 iniziale/finale:
d6497a39a6657f70445d163270b4d2eba5cf808775d5a9c582d7f49e7a8cec9b
```

Il worktree è rimasto pulito. Nessun reviewer ha modificato file.

## 2. Esiti

```text
Lens A — schema, geometria e identità = CLEAN
Lens B — optimizer, K6 e metriche     = BLOCK
Lens C — integrazione e governance    = CLEAN

Verdetto consolidato                  = BLOCK
REV8 SPEC GO                          = NO
```

Non è stato usato voto di maggioranza. B-001 è stato scoperto dalla Lens B e
successivamente riprodotto in modo indipendente dalle Lens A e C.

## 3. Controlli CLEAN

Sono risultati coerenti:

- tick interi a 48000 tick/s e secondi soltanto diagnostici;
- boundary table, projection, structural key e `record_family`;
- interi oltre `2^53` senza passaggio attraverso `float`;
- eligibility e obiettivo esatto K1–K4;
- `K5_RESERVED` inattivo;
- `S_can` scientifico e `M_replay` diagnostico;
- envelope severity/onset/offset, Spearman singleton e center error;
- Average Precision a confidence distinte e macro-AP tramite `mean64`;
- diff limitato al candidate, REV7 byte-identica e nessun GO implicito;
- O-02/O-03/O-18 e O-09 ancora bloccanti;
- suite legacy e R23C entrambe obbligatorie.

Golden numerici riprodotti:

```text
mean64([0.2,0.3,1.0]) = f64:3fe0000000000000
sum-only mutation      = f64:3ff8000000000000
R=9/8,K1=3             = f64:3fad0022f7fa735f
double-round mutation  = f64:3fad0022f7fa7360
divide-first mutation  = f64:3fdfffffffffffff
```

## 4. B-001 — actionable coverage non definita su `M*`

Fixture minima:

```text
g  = semantic_region GT actionable=true

pF = semantic_bundle eleggibile
     stessa geometria e stessa scientific_prediction_key di pT
     actionable=false

pT = semantic_bundle eleggibile
     stessa geometria e stessa scientific_prediction_key di pF
     actionable=true

M_F = {(g,pF)}
M_T = {(g,pT)}
```

Poiché actionable è escluso da eligibility, K1–K4 e scientific edge key:

```text
V(M_F) = V(M_T) = V*
S(M_F) = S(M_T) = S_can
M_F, M_T appartengono a M*

coverage(M_F) = 0
coverage(M_T) = 1
```

Il candidate non definisce per coverage un esistenziale, un envelope, un
criterio singleton o un esito N/A. `M_replay` non può risolvere l'ambiguità
perché è replay-only e non può alimentare metriche o gate.

La fixture replicata su almeno 30 gruppi supera il floor di supporto e mantiene
l'ambiguità gate-relevant. Il finding è quindi un blocker, non solo wording.

La clean actionable rate non è affetta: usa esplicitamente l'esistenziale
prediction-level “almeno un bundle actionable” in un gruppo clean e non associa
una prediction a un target GT.

## 5. Condizione di chiusura

Serve una decisione scientifica firmata che definisca la coverage
target-specific sull'intero insieme degli optimum `M*`. Il candidate bloccato
resta immutabile e non è autorizzata alcuna implementazione.

```text
REV8 SPEC GO = NO
```
