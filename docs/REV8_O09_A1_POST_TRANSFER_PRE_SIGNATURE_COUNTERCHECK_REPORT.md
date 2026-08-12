# REV8 O-09 A1 — Post-transfer pre-signature counter-check

**Verdetto:** `3/3 CLEAN`

**A1:** `SIGNABLE AFTER FREEZE OF THIS REPORT`

**REV8 SPEC GO:** `NO`

## 1. Snapshot verificato

```text
O-09 A1 narrow ballot draft:
docs/REV8_O09_ACTIVATION_BALLOT_A1_DRAFT.md
SHA-256 a9373336fdd0a93ad9cb5499465e8cebcd24cb45fdca89aa4cbfa6e6ea440049
Commit ca9b437b1fcd110819e41de6d9996f3359233843

S6 signed transfer ballot:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_BALLOT_SIGNED.md
SHA-256 8dab8b7ad0fb25f5263358dd26600d201a036210d808840d6c8e1d07051649c0
Commit 7fe229d0cd1f6e5af629a00cfa5ed79fb3b7b7b1

S6 transfer post-signature CLEAN report:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_POST_SIGNATURE_RECHECK_REPORT.md
SHA-256 b148e6340dd3c9b46ca0f6787209cc6d2df00a6512e34c5bce164b81b1349c7a
Commit 88315f0f82a96a8906fe6a382c8f679ab4f73304
```

Il transfer S6 è efficace nella composizione R23C. Il target R23C SHA
`684d8fd7...` resta immutato.

## 2. Metodo

Tre agenti separati hanno verificato in sola lettura lo stesso SHA A1 con
lenti metriche, optimizer e semantica/governance. Nessuno ha modificato file.

## 3. Finding chiusi nel retarget

| Finding | Correzione | Esito |
|---|---|---|
| transfer S6 puntava a draft non efficace | pin del ballot firmato `8dab...` e report CLEAN `b148...` | CLOSED |
| report group-level vivo associato allo SHA storico | corrente `ae14...@3f992455` distinto da storico `508906...@ea8e22b2` | CLOSED |
| target S6 firmato confuso col post-patch storico | target `8f5857...` distinto da evidence `be865...@11365cdb` | CLOSED |
| firma descritta come sufficiente all'attivazione | efficacia soltanto dopo recheck post-firma `CLEAN` congelato | CLOSED |
| patch in-place implicita sul target R23C | composizione esterna; target immutato | CLOSED |

## 4. Esiti delle tre lenti

### 4.1 Metriche e fail-closed

`CLEAN`. Formula, metriche, scope, reason code e pin S6 sono coerenti. Gli
otto test permanenti A1 passano. I 17 cap group-level, Spearman generale,
enforcement candidate e gate O-13 restano fuori scope.

### 4.2 Optimizer e aritmetica

`CLEAN`. La §2.2 resta equivalente al probe evidence-only. La pubblicazione
usa `K1`; `L(Z)` è usato solo nel bound conservativo. Target e composizione
sono hash-pinned e non richiedono una riscrittura del target congelato.

### 4.3 Semantica e governance

`CLEAN`. Provenance corrente/storica è separata. `APPROVO` richiede transfer
S6 efficace e deroga tecnica §8.1 `SI`, ma resta non efficace fino al recheck
post-firma CLEAN. `RESPINGO`, deroga `NO` e `APPROVO CON MODIFICHE` non
attivano nulla.

## 5. Test

```text
Interprete: CPython 3.12.13 canonico
PreflightTests: 8/8 PASS
Suite congelata ml_v3/tests: 540/540 PASS
REV7 / Source / CMake / Resources / ml_v2 protected diff: 0
```

Le modifiche successive all'ultimo full run sono esclusivamente documentali.

## 6. Decisione di gate

Al freeze di questo report, il requisito §8 del ballot è soddisfatto senza
deroga di processo. A1 può essere presentato alla firma sullo SHA
`a9373336...`.

La firma dovrà registrare esplicitamente:

```text
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-08-02
Deroga tecnica di scope §8.1: SI
```

La firma non sarà ancora efficace. Servirà un nuovo report post-firma
`3/3 CLEAN`; un `BLOCK` lascerà i quattro ceiling provvisori e inattivi.
