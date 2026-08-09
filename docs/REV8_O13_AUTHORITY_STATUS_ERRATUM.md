# REV8 — O-13 — AUTHORITY STATUS ERRATUM

## Stato

```text
Tipo                       = erratum append-only docs-only
Data                       = 2026-08-09
Base commit                = 6ce33f9a30af23fd7c6b08ad5bcf7a61ac5d113d
Modifica ballot firmati    = NO
Nuova decisione scientifica = NO
REV8 SPEC GO               = NO
```

Questo erratum risolve esclusivamente la lettura dello stato di O13F_01 e
O13F_02. Non modifica i byte, i digest, le decisioni o lo scope dei ballot
firmati.

## 1. Finding

I ballot firmati conservano alcune righe di stato pre-firma:

```text
O13F_01:
SIGNED — APPROVED — PENDING POST-SIGNATURE RECHECK — NOT EFFECTIVE

O13F_02:
Decisione O13F_02 = NON FIRMATA
```

O13F_02 conserva inoltre una stop-rule esplicitamente condizionata da:

```text
Fino alla firma e al recheck
```

seguita dallo stato storico:

```text
O13F_02 implementation = NON AUTORIZZATA
```

Queste righe non sono più lo stato corrente dopo firma, recheck e
implementazione. In O13F_02 l'intestazione `NON FIRMATA` è un residuo
editoriale interno al file firmato.

## 2. Perché i ballot firmati non vengono riscritti

Cambiare ora le righe stale modificherebbe i byte degli artefatti firmati e
invaliderebbe i digest già verificati dai post-signature recheck.

Pertanto è vietato correggere in-place:

```text
docs/REV8_O13_SUPPORT_FLOOR_BINDING_BALLOT_DRAFT.md
docs/REV8_O13_SUPPORT_BASIS_BALLOT_DRAFT.md
```

La correzione è append-only: questo erratum documenta la sequenza temporale e
la regola di precedenza senza alterare l'evidenza originaria.

## 3. O13F_01 — sequenza autoritativa

```text
Pre-sign freeze:
45c49318f8cb8a6b9bbcb8cda29df929e5a5c563

Signed commit:
59bec34856a08aa43cc52bb123e904a44575f4f6

Signed ballot SHA-256:
3b73e9b43e95140bb0e33a938afbedda57a907f49c3f67220d38b923363d0d14

Post-signature recheck commit:
d0b9916cd08aff29dfde03ffe0c8f38c3656a96c

Post-signature recheck SHA-256:
a02229f3c45c76362b37c3d0fa099bbd9faee71a29969214d8b3ec93a18db8a3

Post-signature verdict:
POST_SIGNATURE_CLEAN
```

Il recheck dispone esplicitamente:

```text
O13F_01 authority    = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_01 utilizzabile = SI, per implementazione candidate-only
```

Lo stato `PENDING POST-SIGNATURE RECHECK — NOT EFFECTIVE` nel ballot è quindi
uno snapshot pre-recheck preservato per integrità, non lo stato corrente.

## 4. O13F_02 — sequenza autoritativa

```text
Pre-sign freeze:
087336b62c5cb0d8412ec133991f7ec7d6cc2250

Pre-sign ballot SHA-256:
016ab3fc0f206813a9157dc4d92d60bb90aa5809642fbcfcd2e5a0436eaadcdd

Signed commit:
26f35e753f96ebc48d0453e432e8f2a3f5380687

Signed ballot SHA-256:
4021353cac2f2a1b469b1e889508a7296bbac8e9f656b2b15511261ea6c5fcc7

Firma contenuta nel ballot:
Decisione O13F_02 = APPROVO
Firma/nome         = Marco
Data               = 2026-08-09

Post-signature recheck commit:
1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e

Post-signature recheck SHA-256:
a444f82f3740f37a5ea5e01aea969810e64d883e264fbbdb361ac4243a95ba95

Post-signature verdict:
POST_SIGNATURE_CLEAN
```

Il recheck dispone esplicitamente:

```text
O13F_02 authority    = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_02 utilizzabile = SI, per implementazione candidate-only
```

L'intestazione `Decisione O13F_02 = NON FIRMATA` è quindi classificata:

```text
STALE_PRE_SIGNATURE_STATUS
```

La stop-rule `Fino alla firma e al recheck` è storica e non nega
l'autorizzazione successiva: firma e recheck si sono entrambi verificati.

## 5. Implementazione successiva

L'implementazione O13F_02 è stata materializzata candidate-only in:

```text
2b57493917ead1074de8b90da5907790564cc34d
feat(rev8): enforce signed O-13 support basis
```

Il report successivo:

```text
docs/REV8_O13_SUPPORT_BASIS_IMPLEMENTATION_REPORT.md
SHA-256:
d86e55aacda9860fc77e3d71b5d5c14407f467e55a56fe9161a9a52872827912
```

registra:

```text
O13F_02 authority            = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_02 schema/compiler code = CANDIDATE IMPLEMENTATO
```

L'implementazione non è partita in assenza di autorità: è successiva al commit
di firma e al post-signature recheck.

## 6. Regola di precedenza per la lettura dello stato

Per O13F_01 e O13F_02 lo stato corrente deve essere ricostruito in ordine
temporale append-only:

```text
1. ballot pre-firma immutabile
2. blocco firma nel commit firmato
3. post-signature recheck sul digest firmato
4. eventuale implementation report
5. status/activation report successivi
```

Una riga di intestazione o stop-rule pre-firma non può sovrascrivere eventi
successivi firmati e hash-verificati. Simmetricamente, un report successivo non
può cambiare la decisione scientifica: può soltanto attestare o applicare ciò
che il ballot firmato autorizza.

## 7. Stato corrente corretto

```text
O13F_01 authority              = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_01 implementation        = CANDIDATE-ONLY COMPLETATA
O13F_02 authority             = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_02 implementation        = CANDIDATE-ONLY COMPLETATA
O13F_03                       = PRE-SIGNATURE CLEAN, NON FIRMATA
Metric/stratum registry data  = NON MATERIALIZZATO
Official support policies     = NON MATERIALIZZATE
REV7 dispatcher               = INVARIATO
O-13 close                    = NO
REV8 SPEC GO                  = NO
G1c close / G1 PASS           = NO
runtime / training / push     = NON AUTORIZZATI
```

## 8. Counter-check richiesto

Prima del commit, verificare:

1. digest dei quattro artefatti O13F_01/O13F_02;
2. esistenza delle commit firma/recheck/implementation;
3. nessuna modifica ai ballot firmati;
4. nessuna nuova firma attribuita;
5. nessun cambio a floor, basis, registry o policy;
6. protected diff zero;
7. worktree contenente soltanto questo erratum.
