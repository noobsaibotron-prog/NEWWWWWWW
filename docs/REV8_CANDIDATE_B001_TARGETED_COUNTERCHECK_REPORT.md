# REV8 — CANDIDATE B-001 — TARGETED COUNTER-CHECK REPORT

## 1. Target

```text
Commit:
3653ba28e402a5ca92391d1f033d74909e4390b7

Parent:
d9ef603f55cdc092c6e4ac3620b771a65daeeabc

Candidate SHA-256 iniziale/finale:
7b4e57abe681e9a19757f924b569b130d3153972e50a0b78c562af66040035ba
```

Nessun reviewer ha modificato file. Il worktree è rimasto pulito.

## 2. Verdetto

```text
Lens A — definizione matematica e aggregazione = CLEAN
Lens B — falsificazione e anti-gaming          = CLEAN
Lens C — integrazione e governance             = AMEND

Verdetto consolidato                            = AMEND
REV8 SPEC GO                                    = NO
```

Non è stato usato voto di maggioranza.

## 3. Policy e fixture confermate

Sono state riprodotte:

```text
unique actionable match       -> minus=1, plus=1
only non-actionable match     -> minus=0, plus=0
ambiguous pF/pT               -> minus=0, plus=1
no actionable GT              -> N/A / NO_ACTIONABLE_GT
2 GT, 1 actionable prediction -> minus=plus=1/2
2 partizioni, una ambigua      -> minus=1/2, plus=1
```

Sono confermati:

- `G_A(u)`, `M_u*`, `covered(g,M)` e `C_u(M)` esatti;
- `coverage_minus` sola metrica primaria e di gate;
- `coverage_plus` diagnostica;
- indipendenza da `M_replay`, ID e hash;
- vincolo one-to-one;
- N/A fail-closed;
- single rounding e gerarchia `mean64`.

## 4. Amend richiesti

### A-001 — replica su almeno 30 `group_id`

Il ballot firmato richiede:

```text
replica su almeno 30 group_id
-> stesso lower envelope gate-relevant
```

La suite candidate trasferisce gli altri property test ma omette questa
replica. Deve essere aggiunta alla fixture B-001.

### A-002 — mutation N/A→zero/PASS

Il ballot richiede di uccidere:

```text
convertire N/A in zero o PASS
```

La stop condition vieta già il comportamento, ma l'elenco delle mutation
obbligatorie non lo include. Deve essere aggiunto esplicitamente.

### A-003 — overlay delle authority

§9.0 dichiara contemporaneamente:

- R23C come “unica authority”;
- ballot B-001 come authority della coverage;
- coverage fra le parti “non ridefinite”;
- R23C come sola precedenza in caso di conflitto.

La formula B-001 resta chiara, ma il wording di authority è stale. Deve essere
corretto dichiarando la composizione `R23C + ballot B-001`, rimuovendo coverage
dalle parti non ridefinite e assegnando a ciascuna authority la precedenza nel
proprio scope.

## 5. Classificazione

I tre amend:

- non cambiano la policy scientifica firmata;
- non richiedono un nuovo ballot;
- non autorizzano codice o artefatti;
- richiedono un nuovo candidate commit document-only;
- richiedono un recheck mirato sul nuovo SHA.

```text
REV8 SPEC GO = NO
```
