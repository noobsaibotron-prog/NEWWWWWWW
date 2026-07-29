# REV8 — ROUND 2.3 — RESTRICTED RED-TEAM RECHECK PROTOCOL

**Stato:** `READY_AFTER_R23A_SIGNATURE`

## 1. Target

Il recheck parte soltanto dopo la firma del ballot
`REV8_ROUND2_3_RESTRICTED_RED_TEAM_MICRO_AMEND_BALLOT.md`.

```text
Target atteso:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_AMENDED_DRAFT.md

SHA pre-firma proposto:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011
```

Se la firma modifica il target, il reviewer usa il nuovo SHA dichiarato nel
ballot. Ogni reviewer registra SHA iniziale e finale; una differenza produce
`BLOCK / TARGET_MUTATED`.

## 2. Scope

Il recheck è read-only e limitato a:

```text
RT-001 / R23A_01
RT-002 / R23A_02
RT-003 / R23A_03
RT-004 / R23A_04
RT-005 / R23A_05
```

Non sono autorizzati:

- nuovi redesign non causati dai cinque amendment;
- modifica di codice, schema, candidate, runtime o training;
- `REV8 SPEC GO`;
- giudizio per maggioranza.

Un singolo blocker riprodotto mantiene `BLOCK`.

## 3. Check obbligatori

### Lens A — Numeric / Optimizer

1. Riprodurre:

   ```text
   R=9/8, K1=3
   single-round = f64:3fad0022f7fa735f
   double-round = f64:3fad0022f7fa7360
   ```

2. Verificare che nessun wording autorizzi un binary64 intermedio di
   `log2(R)`.
3. Verificare `mean64`:

   ```text
   sum-then-divide = f64:3fe0000000000000
   divide-then-sum = f64:3fdfffffffffffff
   ```

4. Verificare definizione, scope e stato provvisorio di bit-length:

   ```text
   rational_bit_length(2^39999/(2^39999+1)) = 80000
   ```

### Lens B — Semantics / K6

1. Costruire un tie scientifico con una GT e due prediction geometricamente
   identiche ma confidence differenti.
2. Verificare:

   ```text
   V*, M*, S_can invariati
   M_replay può cambiare
   metriche e gate invariati
   ```

3. Cercare ogni residuo di `M_can` o qualunque uso di `D` nei risultati
   scientifici.
4. Verificare che A1/A2 e mutation usino coerentemente
   `V*`, `S_can`, `M_replay`.

### Lens C — Fixtures / Governance

1. Leggere i tre bit pattern Resonance come N64 esatti e verificare:

   ```text
   inside_bits + 1 = outside_bits
   inside^3 < 2*min^3
   outside^3 > 2*min^3
   ```

2. Verificare la prova che `q^3=2` non ha soluzione razionale.
3. Verificare che il ceiling O-09 resti inattivo e possa essere attivato
   soltanto insieme a bound, scope, soglia, fixture e blast radius.
4. Verificare che predecessor firmato, report e ballot siano referenziati con
   SHA corretti e che `REV8 SPEC GO = NO` resti esplicito.

## 4. Verdetto

Ogni lens emette:

```text
CLEAN
AMEND
BLOCK
```

Il consolidatore:

- fonde duplicati mantenendo le catene causali;
- riproduce ogni finding materiale;
- non usa voto di maggioranza;
- emette `CLEAN` soltanto se tutti i cinque finding sono chiusi e nessuna
  correzione introduce un blocker concreto nel medesimo scope.

## 5. Next permitted action

Solo con recheck `CLEAN` sullo SHA firmato:

```text
patch document-only del candidate REV8
```

Restano non autorizzati implementazione, schema live, runtime, training e
`REV8 SPEC GO`.
