# REV8 — ROUND 2.3 — R23C POST-SIGNATURE RECHECK REPORT

**Verdetto consolidato:** `CLEAN`

## 1. Target e ballot verificati

```text
Target normativo:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md

SHA-256 iniziale:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

SHA-256 finale:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

Ballot R23C firmato:
docs/REV8_ROUND2_3_R23C_CONFIRMATION_BALLOT.md

SHA-256:
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99
```

Il target e il ballot sono rimasti immutati durante tutte le review.

## 2. Firma verificata

```text
Stato: SIGNED — APPROVED
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-07-29
SHA target firmato:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

`R23C_01` e `R23C_02` sono state approvate atomicamente. Non è stata
registrata alcuna firma parziale.

## 3. Catena di provenance

```text
Predecessore R23A:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011

Ballot R23A firmato:
4db761eeb017cd04595a1ac64943b535809675adefba9fe3d5cf8083336c8666

Recheck report R23A:
0e4a92f1cfa2c11193a3f3dbb1f3f5809b743486509ba469ce5f08477467f44d

Counter-check pre-firma R23C:
b9a157bc50a7b36af7c7cfc36c3e3149d83881b0c4714387f789a63c1b5b577b

Protocollo recheck post-firma:
79d59d14481b552050e41106475dcc4130a6c4c66cc6b0827a999d8c73d8b25f
```

## 4. Esiti indipendenti

| Lens | Ambito | Verdetto | File modificati |
|---|---|---:|---:|
| Lens A | Metrics / macro-AP | CLEAN | nessuno |
| Lens B | Semantics / metric fields vs replay | CLEAN | nessuno |
| Lens C | Optimizer / integrazione / governance | CLEAN | nessuno |

Non è stato usato voto di maggioranza. Il verdetto consolidato è `CLEAN`
perché tutte e tre le lenti hanno restituito `CLEAN` sul medesimo target e
ballot.

## 5. Lens A — macro-AP

La review ha confermato:

```text
sum_pairwise64 = fase di somma interna
mean64         = aggregatore finale con divisione
macro-AP       = mean64(AP_group64 ordinati per group_id UTF-8)
```

Golden riprodotto indipendentemente:

```text
AP_group64 = [0.2, 0.3, 1.0]

sum-only mutation:
1.5 = f64:3ff8000000000000

mean64 normativo:
0.5 = f64:3fe0000000000000
```

La mutation `sum-only` è esplicitamente rifiutata. Non esiste una norma
concorrente che definisca la macro-AP come somma grezza.

## 6. Lens B — metriche e replay

La review ha confermato:

- `severity`, `confidence`, `actionable`, ID e hash non influenzano
  eligibility, `V*`, `M*`, `S` o `S_can`;
- severity alimenta soltanto gli usi metrici esplicitamente firmati;
- confidence alimenta soltanto la formazione di `P_t` e AP come firmato;
- `D` e `M_replay` sono replay-only e diagnostici;
- `D/M_replay` non selezionano pairing, envelope, metriche normative o gate;
- i valori esplicitamente diagnostici possono dipendere da `M_replay`.

Fixture confermate:

```text
severity_P 0 -> 1:
Q_severity 0 -> 1
V*, M*, S_can invariati

confidence TP>FP -> AP=1
confidence FP>TP -> AP=1/2

tie K6:
V*, M*, S_can invariati
M_replay può cambiare
output scientifici, metriche normative e gate invariati
```

Non esiste alcun residuo `M_can`.

## 7. Lens C — integrazione e governance

La review ha confermato:

- R23A_01–R23A_05 restano preservate;
- R23C_01 e R23C_02 sono chiarificazioni, non nuove scelte scientifiche;
- i marker `PENDING` dentro il target descrivono correttamente i byte
  pre-firma; l’autorità successiva deriva dal ballot esterno senza mutare il
  target firmato;
- §§2.3, 9, 11.2, 13 e 14 sono coerenti;
- non esistono dispatch concorrenti della macro-AP;
- nessun GO, codice, candidate patch, runtime o training è stato autorizzato.

## 8. Verdetto

```text
R23C_01 = APPROVATA, FIRMATA, RECHECK CLEAN
R23C_02 = APPROVATA, FIRMATA, RECHECK CLEAN

Post-signature recheck = 3/3 CLEAN
REV8 SPEC GO           = NO
Candidate patch        = NON AUTORIZZATA
Implementation         = NON AUTORIZZATA
Runtime/training       = NON AUTORIZZATI
```

Il ciclo R23C è chiuso. Qualunque passaggio successivo richiede un gate
separato ed esplicito.

