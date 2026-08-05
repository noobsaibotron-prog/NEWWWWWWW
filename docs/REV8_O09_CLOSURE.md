# REV8 O-09 — CHIUSURA DEL FILONE

**Stato:** `O-09 CHIUSO` — **`REV8 SPEC GO = NO`** (invariato)

**Commit di chiusura:** `7ddad67274c95129e3eacb0f416f4634973b57c7`
**Data:** 2026-08-05 · **Decisione di:** Marco

## 1. Cosa è chiuso

I quattro ceiling per-sottografo di §10 sono **firmati e attivi**, con
enforcement materializzato e verificato per esecuzione:

```text
GT <= 128 · prediction <= 128 · eligible edges <= 16384 · bit-length <= 65536
GT=129 -> REJECTED / SOLVER_STRUCTURAL_LIMIT_EXCEEDED
```

- ballot A1 firmato + recheck post-firma `3/3 CLEAN`;
- policy S6 firmata e trasferita all'autorità R23C viva, recheck `3/3 CLEAN`;
- evidenza benchmark riproducibile: run indipendente a commit diverso
  riproduce **26/26** hash scientifici dello storico;
- 17 cap group con evidenza di bordo durevole — **51/51** combinazioni
  cap-1/cap/cap+1, rifiuto **pre-solve** provato (`solver_calls == 0`);
- suite **574/574**, perimetro protetto **0-diff**, REV7 mai toccato,
  runtime live invariato (REV8 irraggiungibile dal dispatcher).

Tre difetti fail-open trovati e chiusi lungo il percorso, tutti provati per
iniezione e non per lettura: `TimeoutError` assente dalla tassonomia group
(`2ab1a476`), fase di preflight non protetta (`362acda3`, `e49bbe2b`),
kernel group che non consumava la superficie A1 attiva (`fced92ec`).

## 2. Cosa resta aperto, e a chi appartiene

| Voce | Natura | Titolare |
|---|---|---|
| F-04 — evidenza di falsificazione metriche | copertura di test | O-09, **non bloccante** |
| F-05 — wording storico S6 fuorviante | debito documentale | O-09, **non bloccante** |
| `G_eligible`/`G_defined`/`G_NA`, gate floor, p95 Type-7 | misurazione nuova | **O-13** |
| pubblicazione `rho64`, caso Spearman variable-value-marginal | scienza aperta | **O-12 / fuori scope** |

Nessuna di queste blocca ciò che O-09 doveva produrre. F-05 in particolare è
un documento che spiegherebbe perché un altro documento era impreciso su un
terzo: costo reale, valore nullo.

## 3. Perché si chiude qui, dichiarato

Il filone ha prodotto **35 commit — 23 di sola documentazione, 11 di codice**;
**40 documenti REV8 per 11.449 righe**, contro **5.049 righe** di codice O-09.
Rapporto 2,3 righe di documento per riga di codice.

Ogni giro di verifica trovava qualcosa, ogni ritrovamento generava un fix,
ogni fix un documento, ogni documento un'altra verifica. Il processo
produceva documentazione più in fretta di quanta ne consumasse. Non esiste
un "ultimo finding" che chiuda il filone: si chiude decidendo di chiuderlo.

O-09 è **una riga su ~29** del ledger §15. A questo costo per riga,
`REV8 SPEC GO` non è distante mesi ma anni. La chiusura è una decisione di
allocazione, non una dichiarazione che non resti nulla da trovare.

## 4. Limiti che questa chiusura NON cancella

- **Non è `REV8 SPEC GO`.** Le altre righe del ledger non sono state
  verificate una per una da nessuno.
- **Cinque dei 17 cap group non sono violabili in isolamento** — AP-02, AP-05,
  AP-06, SP-04, SP-05: nessun input li sfora senza sforarne prima un altro
  (stessa struttura del cap-archi per-sottografo, `16384 = 128 × 128`).
  Attivarli darebbe l'illusione di una protezione che non aggiungono.
- **I 17 cap group restano NON ATTIVI**: evidenza sì, autorità normativa no.
- Il ballot S6 fu firmato **senza** counter-check a tre lenti — deroga
  dichiarata nel ballot stesso, mai sanata.
- Il caso `spearman_variable_unavailable` resta **non certificato** e costa
  ~25-29 s contro sotto-4 s dei vicini: policy di costo mai decisa.

## 5. Regola per il prossimo tranche

Niente report intermedi. Commit atomici con la verifica nel messaggio, e
**un solo** documento di chiusura alla fine.

```text
O-09                  = CHIUSO
Cap per-sottografo    = ATTIVI
Cap group (17)        = evidenza sì, NON attivi
REV8 SPEC GO          = NO
Runtime / training    = NON AUTORIZZATI
Push                  = mai eseguito (prerogativa di Marco)
```
