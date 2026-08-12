# REV8 — ROUND 2.3 — R23C FREEZE MANIFEST

## 1. Stato del freeze

```text
Branch:
feature/motore-v3-rev8-spec-go

Parent HEAD:
52757b352b92a7c5c8b13699a8ba5a8c4286c8fd

R23C_01:
APPROVATA, FIRMATA, RECHECK CLEAN

R23C_02:
APPROVATA, FIRMATA, RECHECK CLEAN

Post-signature recheck:
3/3 CLEAN

REV8 SPEC GO:
NO

Candidate patch:
NON AUTORIZZATA

Implementation/runtime/training:
NON AUTORIZZATI
```

Questo manifest congela esclusivamente la catena documentale Round 2.3. Non
costituisce un gate di attivazione e non autorizza modifiche al codice.

## 2. Artefatto normativo corrente

```text
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md
SHA-256:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

I marker R23C `PENDING_EXTERNAL_BALLOT` contenuti nel target descrivono i byte
immutabili precedenti alla firma. La loro approvazione deriva dal ballot
esterno firmato, senza riscrivere il target sottoposto a review.

## 3. Autorità R23C

```text
Ballot firmato:
docs/REV8_ROUND2_3_R23C_CONFIRMATION_BALLOT.md
SHA-256:
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

Counter-check pre-firma:
docs/REV8_ROUND2_3_R23C_PRE_SIGNATURE_COUNTERCHECK_REPORT.md
SHA-256:
b9a157bc50a7b36af7c7cfc36c3e3149d83881b0c4714387f789a63c1b5b577b

Protocollo post-firma:
docs/REV8_ROUND2_3_R23C_POST_SIGNATURE_RECHECK_PROTOCOL.md
SHA-256:
79d59d14481b552050e41106475dcc4130a6c4c66cc6b0827a999d8c73d8b25f

Report post-firma:
docs/REV8_ROUND2_3_R23C_POST_SIGNATURE_RECHECK_REPORT.md
SHA-256:
e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157
Verdetto:
3/3 CLEAN
```

## 4. Catena R23 e R23A

```text
Signed draft R23:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256:
8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041

Protocollo red-team R23:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_PROTOCOL.md
SHA-256:
b5407ff644bcae645016767586caa220e4cb43e83d199dd125d7496898e2e207

Report red-team R23:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_REPORT.md
SHA-256:
6f905589e08740fb12ca8cc446365938f217ef2acc48b958cd9441a8a4051841
Verdetto storico:
BLOCK

Amended draft R23A:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_AMENDED_DRAFT.md
SHA-256:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011

Ballot R23A firmato:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_MICRO_AMEND_BALLOT.md
SHA-256:
4db761eeb017cd04595a1ac64943b535809675adefba9fe3d5cf8083336c8666

Protocollo recheck R23A:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_RECHECK_PROTOCOL.md
SHA-256:
cd44003f983ec185e0381b1820f21a284bb4bc6c87bd6f5917226c6a3ecc0e56

Report recheck R23A:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_RECHECK_REPORT.md
SHA-256:
0e4a92f1cfa2c11193a3f3dbb1f3f5809b743486509ba469ce5f08477467f44d
Verdetto storico:
BLOCK
```

I due verdict storici `BLOCK` sono parte della provenance: hanno prodotto gli
amend R23A e R23C. Non sono il verdetto del target corrente.

## 5. Regola di autorità

L’autorità corrente è la composizione immutabile di:

```text
target R23C
+ ballot R23C firmato
+ report post-firma R23C CLEAN
```

Qualunque divergenza da uno degli SHA elencati invalida il freeze e richiede
un nuovo artefatto, ballot e recheck.

## 6. Passo successivo

Il freeze documentale non produce automaticamente alcun GO.

```text
REV8 SPEC GO = NO
```

L’eventuale candidate patch richiede un gate separato ed esplicito, sul commit
immutabile che contiene questo manifest e tutti gli artefatti elencati.

