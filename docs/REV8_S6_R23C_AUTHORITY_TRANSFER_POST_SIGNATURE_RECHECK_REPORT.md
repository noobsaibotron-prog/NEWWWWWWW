# REV8 S6 — R23C authority-transfer post-signature recheck

**Verdetto:** `3/3 CLEAN`

**S6 nella composizione R23C:** `EFFICACE AL FREEZE DI QUESTO REPORT`

**REV8 SPEC GO:** `NO`

## 1. Snapshot firmato

```text
Signed transfer ballot:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_BALLOT_SIGNED.md
SHA-256 8dab8b7ad0fb25f5263358dd26600d201a036210d808840d6c8e1d07051649c0
Commit 7fe229d0cd1f6e5af629a00cfa5ed79fb3b7b7b1

Unsigned provenance-pinned draft:
SHA-256 5db2f7585e3fdbc1b7fb67599882252636213da60c8f8d9f7a0c95f574ede33a
Commit b1adefbeaf51a4174565ff7a2e85bcf4cab4f3bb
```

Il diff fra draft e ballot firmato è limitato a:

```text
rename DRAFT -> SIGNED
stato: SIGNED — APPROVED — PENDING POST-SIGNATURE RECHECK — NOT EFFECTIVE
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-08-02
```

Target, decisione trasferita, scope, evidence pin e transizioni restano
invariati.

## 2. Authority e provenance verificate

```text
R23C target corrente:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md
SHA-256 684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

R23C signed ballot:
SHA-256 4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

R23C post-signature CLEAN report:
SHA-256 e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157

S6 signed historical snapshot:
SHA-256 b94c1b2c061db384d2b6a61688def5656b5bcdf75ec05a2904c2b45cd14220bf
git object e36d2422:docs/REV8_CANDIDATE_S6_SPEARMAN_REASON_CODE_MICRO_AMEND_BALLOT.md

S6 current document with authority errata:
SHA-256 2904cf07884f1e1876cb3b8ceb6726ff1ab6cbbaba70a3158fae9e37bffd6bcb
Commit 3f992455bd2cc58fb7501b10bb2e1e09f86f8abd

Historical patched SIGNED_DRAFT evidence:
SHA-256 be8658203e26fae3bc36d020733b6b7ed773d24ed0e78675330f08b459ff56cc
git object 11365cdb:docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md

Live restored SIGNED_DRAFT:
SHA-256 8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041
```

Lo snapshot storico firmato, il documento corrente con errata, il patch
storico recuperabile e il file vivo ripristinato sono distinti. Nessun hash
storico viene presentato come hash del corrispondente file vivo.

## 3. Tre lenti post-firma

Tre agenti separati hanno verificato in sola lettura lo stesso commit e lo
stesso SHA. Nessuno ha modificato file.

### 3.1 Semantica e governance

`CLEAN`. Firma completa (`APPROVO`, `Marco`, `2026-08-02`), authority e
provenance coerenti. La firma da sola non produceva efficacia: il ballot
richiedeva esplicitamente questo recheck post-firma. Un `BLOCK` avrebbe
lasciato S6 assente.

### 3.2 Metriche e fail-closed

`CLEAN`. La policy trasferita coincide con S6 §3:

```text
certificate unavailable -> N/A / SPEARMAN_CERTIFICATE_UNAVAILABLE
```

L'esito non è PASS, non è zero, non è runtime failure, non diventa
`PAIRING_AMBIGUOUS` senza prova di non-singleton e impedisce il PASS di un
gate che richiede una Spearman definita.

### 3.3 Optimizer, hash e composizione

`CLEAN`. Commit e SHA firmati corrispondono; target e input sono immutati; la
composizione è append-only e non riscrive i byte del target R23C.

## 4. Test riprodotti

Interprete canonico:

```text
/Users/marco/aieq_data/motore_v3/env/venv/bin/python
CPython 3.12.13
PYTHONDONTWRITEBYTECODE=1
```

```text
PreflightTests + SpearmanTests + RuntimeFailureTests
22 eseguiti
22 PASS
exit code 0

Suite completa ml_v3/tests
540 eseguiti
540 PASS
exit code 0
```

REV7 protetta, `Source/`, `CMakeLists.txt`, `Resources/` e `ml_v2/` hanno
diff zero rispetto al riferimento protetto `7e23f1c3`.

## 5. Composizione risultante

Al freeze immutabile di questo report, l'autorità applicabile diventa:

```text
authority R23C congelata
+ signed S6 transfer ballot SHA 8dab8b7a...
+ questo post-signature report CLEAN
```

L'effetto normativo aggiunge soltanto il quarto esito N/A Spearman. Non
modifica i byte del target R23C e non autorizza cap O-09, enforcement code,
candidate activation, runtime, training o modifiche REV7. Il caso Spearman
general variable-value-marginal resta non certificato.

## 6. Prossimo gate

A1 deve essere retargetizzato sugli SHA finali del transfer firmato e di
questo report, quindi sottoposto a un recheck mirato del nuovo SHA. Il vecchio
counter-check A1 non può essere riutilizzato automaticamente dopo il retarget.

Fino alla successiva firma A1 e al relativo recheck post-firma:

```text
O-09 quattro ceiling per-sottografo = PROVVISORI, NON ATTIVI
17 cap group-level                  = NON AUTORIZZATI
enforcement candidate               = NON MATERIALIZZATO
REV8 SPEC GO                        = NO
runtime/training                    = NON AUTORIZZATI
```
