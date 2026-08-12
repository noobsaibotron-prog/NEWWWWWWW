# REV8 — ROUND 2.3 — R23C POST-SIGNATURE RECHECK PROTOCOL

## 1. Scopo

Questo protocollo disciplina il controllo ristretto successivo alla firma del
ballot R23C. Non autorizza modifiche, implementazione, candidate patch,
training o `REV8 SPEC GO`.

## 2. Target immutabile

```text
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md

SHA-256 atteso:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

Il recheck DEVE fermarsi con `BLOCK` se il digest non coincide.

## 3. Ballot richiesto

```text
docs/REV8_ROUND2_3_R23C_CONFIRMATION_BALLOT.md
```

Il ballot DEVE:

- essere firmato `APPROVO`;
- riportare nome, data e SHA del target;
- approvare atomicamente `R23C_01` e `R23C_02`;
- non contenere modifiche sostanziali successive al counter-check pre-firma.

Il reviewer DEVE calcolare e registrare lo SHA-256 del ballot firmato.

## 4. Lenti indipendenti

### Lens A — macro-AP

Verificare:

1. `sum_pairwise64` è soltanto la somma interna;
2. `mean64` è l’aggregatore finale;
3. macro-AP usa `mean64(AP_group64)` nell’ordine normativo;
4. il golden `[0.2,0.3,1.0]` produce `0.5`;
5. la mutation `sum-only = 1.5` viene rifiutata;
6. non esiste un testo concorrente che definisca macro-AP come somma grezza.

### Lens B — metric fields e replay

Verificare:

1. severity/confidence/actionable/ID/hash non influenzano il matching
   scientifico;
2. severity può influenzare soltanto le metriche esplicitamente firmate;
3. confidence può influenzare `P_t` e AP come esplicitamente firmato;
4. `D` e `M_replay` sono replay-only;
5. replay non alimenta output scientifici, metriche normative o gate;
6. i valori esplicitamente diagnostici possono dipendere dal replay senza
   violare la norma.

### Lens C — integrazione e governance

Verificare:

1. nessuna decisione R23A è stata alterata;
2. le due decisioni R23C sono soltanto chiarificazioni;
3. target, predecessor, report e ballot sono legati ai digest corretti;
4. non è comparso alcun GO implicito;
5. non è autorizzato alcun codice, candidate patch o training;
6. non esistono contraddizioni fra §§2.3, 9, 11.2, 13 e 14.

## 5. Regola di decisione

Non si usa voto di maggioranza:

```text
3 CLEAN             -> CLEAN
almeno un AMEND     -> AMEND
almeno un BLOCK     -> BLOCK
digest non conforme -> BLOCK
```

Ogni lente deve fornire:

- verdetto;
- evidenza puntuale;
- eventuale blocker minimo riproducibile;
- SHA iniziale e finale del target;
- dichiarazione di non aver modificato i file.

## 6. Output richiesto

Creare:

```text
docs/REV8_ROUND2_3_R23C_POST_SIGNATURE_RECHECK_REPORT.md
```

Il report deve contenere:

- SHA del target;
- SHA del ballot firmato;
- SHA del report pre-firma;
- esito di ciascuna lente;
- evidenza dei due golden R23C;
- verdetto consolidato;
- stato governance finale.

Anche con verdetto `CLEAN`:

```text
REV8 SPEC GO = NO
```

Qualunque passaggio successivo richiede un gate separato ed esplicito.

