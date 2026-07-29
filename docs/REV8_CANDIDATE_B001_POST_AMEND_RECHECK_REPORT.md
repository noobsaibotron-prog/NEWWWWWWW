# REV8 — CANDIDATE B-001 — POST-AMEND RECHECK REPORT

## 1. Target immutabile

```text
Commit:
ea29a7ae3cf2fdf61a7e8dc2e1944099bb926143

Parent:
da930615

Candidate:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md

Candidate SHA-256 iniziale/finale:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745
```

Il commit target modifica esclusivamente il candidate REV8. Il worktree è
rimasto pulito durante il recheck e nessun reviewer ha modificato file.

## 2. Provenance dell'AMEND

Il precedente counter-check sul commit:

```text
3653ba28e402a5ca92391d1f033d74909e4390b7
```

è registrato in:

```text
docs/REV8_CANDIDATE_B001_TARGETED_COUNTERCHECK_REPORT.md
SHA-256:
36973330326f8ad68e491589eaf07babe381b41a69e221e15ed54336798e804e

Report commit:
da930615
```

Il report aveva emesso `AMEND` per:

```text
A-001  replica su almeno 30 group_id assente dalla suite candidate
A-002  mutation coverage N/A -> zero/PASS assente dalla lista obbligatoria
A-003  overlay R23C/B-001 contraddittorio sulla coverage
```

## 3. Patch verificata

Il commit target:

```text
ea29a7ae3cf2fdf61a7e8dc2e1944099bb926143
```

chiude esclusivamente i tre amend:

- aggiunge la replica su almeno 30 `group_id` con lo stesso lower envelope
  gate-relevant;
- aggiunge la mutation obbligatoria che converte la coverage N/A in zero o
  PASS;
- assegna R23C al proprio scope e il ballot B-001 esclusivamente alla coverage
  target-specific conservativa, rimuovendo coverage dalle clausole storiche
  non ridefinite.

La formula B-001, le decisioni scientifiche e gli altri gate non sono stati
modificati.

## 4. Lenti indipendenti

```text
Lens A — semantica e authority       = CLEAN
Lens B — test e anti-gaming          = CLEAN
Lens C — integrazione e governance   = CLEAN

Verdetto consolidato                 = 3/3 CLEAN
```

Non è stato usato voto di maggioranza: ciascuna lente ha verificato il proprio
perimetro e nessuna ha emesso `AMEND` o `BLOCK`.

### Lens A

Ha confermato:

- `G_A(u)`, `M_u*`, `covered(g,M)` e `C_u(M)` invariati;
- `coverage_minus` unica metrica scientifica e di gate;
- `coverage_plus` diagnostica;
- `NO_ACTIONABLE_GT` e `PAIRING_ENVELOPE_UNAVAILABLE` fail-closed;
- divieto di massimo, `M_replay` ed esistenziale prediction-level;
- single rounding e gerarchia `mean64` invariati;
- authority R23C/B-001 ora disgiunte per scope.

### Lens B

Ha confermato:

- trasferimento integrale del property test su almeno 30 `group_id`;
- trasferimento integrale della mutation N/A→zero/PASS;
- nessun indebolimento delle fixture e mutation preesistenti;
- nessun rientro di `M_replay`, massimo, esistenziale o `actionable` nel
  matching scientifico o nel gate.

### Lens C

Ha confermato:

- chiusura di A-001, A-002 e A-003;
- scope del commit limitato al solo candidate;
- worktree pulito;
- nessuna modifica a codice, schema live, test, runtime o PLAN;
- nessun GO implicito;
- assenza di residui contraddittori materiali.

## 5. Verifiche di integrità

```text
REV7:
docs/MOTORE_V3_G1_CONTRACT.md
SHA-256:
310d538647d71840c6dd8124f1e24281b758bb6dd3e4776aac4b34e5f658a5b0

PLAN SHA-256:
8c45a01dbb207e8163f9b3d0613f4547c3fbd5f22680de0e74ba4785f62def77

Candidate SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745
```

REV7 e PLAN sono invariati. Non è stato eseguito alcun push.

## 6. Verdetto

```text
B-001 transfer nel candidate    = CLEAN
Round documentale B-001         = CHIUSO
Candidate REV8                  = RECERTIFIED per questo perimetro
REV8 SPEC GO                    = NO
Implementation/training         = NON AUTORIZZATI
```

Il `3/3 CLEAN` chiude il counter-check mirato B-001. Non attiva REV8 e non
sostituisce i gate successivi: materializzazione degli artefatti O-02/O-03/O-18,
benchmark e ballot dei cap O-09, integrazione completa, recheck del pacchetto e
SPEC GO restano separati.
