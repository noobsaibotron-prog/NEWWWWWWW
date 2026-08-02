# REV8 S6 — R23C authority-transfer ballot

**Stato:** `DRAFT — PENDING SIGNATURE — NO AUTHORITY YET`

**REV8 SPEC GO:** `NO`

## 1. Motivo del trasferimento

Il ballot S6 firmato il 2026-07-31 ha approvato il reason code
`SPEARMAN_CERTIFICATE_UNAVAILABLE`, ma ha indicato come target il precedente
`NORMATIVE_SIGNED_DRAFT`. La catena congelata R23C dichiara invece corrente la
composizione:

```text
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md
SHA-256 684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

+ ballot R23C firmato
SHA-256 4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

+ report post-firma R23C CLEAN
SHA-256 e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157
```

Il target R23C corrente non contiene S6. Il patch storico applicato al vecchio
signed draft, pur corretto nei propri byte, non modifica la catena corrente.

## 2. Decisione trasferita

Questo ballot non sceglie una nuova policy. Trasferisce alla composizione R23C
la decisione S6 già firmata, senza modificare il target immutabile:

```text
Nel caso Spearman in cui la procedura esatta richiesta non disponga di un
certificato sufficiente a dimostrare l'unicità del valore su M*, il risultato
è N/A con reason code SPEARMAN_CERTIFICATE_UNAVAILABLE.

Questo esito:
- non è PASS;
- non è zero;
- non è una failure runtime;
- non può essere convertito in PAIRING_AMBIGUOUS senza prova di non-singleton;
- impedisce il PASS di qualunque gate che richieda una Spearman definita.
```

La composizione risultante, soltanto dopo firma **e recheck post-firma CLEAN**,
sarà:

```text
authority R23C congelata
+ questo ballot S6 transfer firmato
+ report post-firma S6 transfer CLEAN
```

I marker e i byte del target R23C restano invariati.
Un recheck `BLOCK` lascia S6 assente dalla composizione corrente; la sola firma
non è sufficiente a renderla efficace.

## 3. Scope

Il trasferimento:

- aggiunge soltanto il quarto esito N/A all'autorità Spearman §11.4;
- non chiude il caso scientifico general variable-value-marginal;
- non attiva cap O-09, codice, candidate, runtime o training;
- non valuta gate floor o supporto group-level;
- non costituisce `REV8 SPEC GO`;
- non sana retroattivamente la deroga di processo del primo ballot S6.

## 4. Evidenza

```text
Snapshot S6 firmato storico:
SHA-256 b94c1b2c061db384d2b6a61688def5656b5bcdf75ec05a2904c2b45cd14220bf
Commit e36d2422b5895abb0721b11f34805090c4ad374a
Recuperabile con: git show e36d2422:docs/REV8_CANDIDATE_S6_SPEARMAN_REASON_CODE_MICRO_AMEND_BALLOT.md

Documento S6 corrente con errata di authority:
docs/REV8_CANDIDATE_S6_SPEARMAN_REASON_CODE_MICRO_AMEND_BALLOT.md
SHA-256 2904cf07884f1e1876cb3b8ceb6726ff1ab6cbbaba70a3158fae9e37bffd6bcb
Commit errata 3f992455bd2cc58fb7501b10bb2e1e09f86f8abd

Patch storico verificato — NON PIU' NEL WORKTREE, solo in git history:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256 be8658203e26fae3bc36d020733b6b7ed773d24ed0e78675330f08b459ff56cc
Recuperabile con: git show 11365cdb:docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
Motivo: quel patch aveva rotto il pin congelato del freeze manifest R23C
(8f5857a8...), ripristinato in ba17a949. Il file nel worktree e' tornato a
8f5857a8... e NON contiene S6. Verificare questo SHA contro il file vivo
fallirebbe: e' atteso, non un'anomalia.

Implementazione candidata:
ml_v3/benchmark/rev8_o09_group_candidate.py
SHA-256 53033d7fcc872420b5420ff4fb825762ddcfb4defa458ffdfdf82f124e3622f6

Targeted report:
docs/REV8_S6_TARGETED_COUNTERCHECK_REPORT.md
SHA-256 88d24d1522e3bfb0a012b19b1b8edf2ff4087ff9e3534c82aa21aa946acd2570

Evidence freeze commit:
9a16692bfee32396213f7487a80ef94f99f50076
```

## 5. Firma dell'autorità

```text
Decisione complessiva: [ DA COMPILARE — APPROVO / RESPINGO ]
Firma/nome:            [ DA COMPILARE ]
Data:                  [ DA COMPILARE ]
SHA target R23C:       684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

## 6. Stato dopo la firma (se APPROVO)

```text
S6 nella composizione R23C            = FIRMATA, NON EFFICACE PENDING RECHECK
Target R23C                           = IMMUTATO
Counter-check nuovo ballot/SHA        = OBBLIGATORIO
Spearman variable-value-marginal      = NON CERTIFICATO
Cap O-09                              = NON ATTIVI
REV8 SPEC GO                          = NO
Runtime/training                      = NON AUTORIZZATI
```
