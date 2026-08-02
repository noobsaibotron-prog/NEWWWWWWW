# REV8 O-09 A1 — Post-signature recheck report

**Verdetto:** `3/3 CLEAN`

**O-09 quattro ceiling per-sottografo:** `NORMATIVAMENTE ATTIVI AL FREEZE DI QUESTO REPORT`

**Enforcement candidate:** `NON MATERIALIZZATO`

**REV8 SPEC GO:** `NO`

## 1. Snapshot firmato

```text
Signed A1 ballot:
docs/REV8_O09_ACTIVATION_BALLOT_A1_SIGNED.md
SHA-256 bf31ed0e1bbcfa001114808e569a5155427b54eef3b9dd1cd0d80fcd1c184d75
Commit 5e85c96362d4ccdd8a7932c21690651cfbd87f58

Pre-signature A1 report:
docs/REV8_O09_A1_POST_TRANSFER_PRE_SIGNATURE_COUNTERCHECK_REPORT.md
SHA-256 1da135ae77442f0471c2a3968af921e8f3162ac7705de6084f828505b2324748
Commit 829c0d29f89b2ee348270533cdfcf5a8cf4f0a97

Unsigned retargeted draft:
SHA-256 a9373336fdd0a93ad9cb5499465e8cebcd24cb45fdca89aa4cbfa6e6ea440049
Commit ca9b437b1fcd110819e41de6d9996f3359233843
```

La firma registra:

```text
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-08-02
Deroga tecnica di scope §8.1: SI
Deroga di processo: nessuna
```

## 2. Authority composta verificata

```text
R23C target immutabile SHA-256
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

R23C ballot SHA-256
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

R23C post-signature report SHA-256
e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157

S6 signed transfer SHA-256
8dab8b7ad0fb25f5263358dd26600d201a036210d808840d6c8e1d07051649c0

S6 transfer post-signature CLEAN report SHA-256
b148e6340dd3c9b46ca0f6787209cc6d2df00a6512e34c5bce164b81b1349c7a
```

Nessun byte del target R23C è stato riscritto. L'attivazione vive nella
composizione append-only degli artefatti firmati e dei rispettivi report
post-firma CLEAN.

## 3. Tre lenti post-firma

Tre agenti separati hanno verificato in sola lettura commit e SHA firmati.
Nessuno ha modificato file.

### 3.1 Metriche e fail-closed

`CLEAN`. Firma, deroga tecnica, policy, scope e semantica fail-closed sono
coerenti. Tutti i pin coincidono. I ceiling attivati sono soltanto:

```text
GT                              <= 128
prediction                      <= 128
eligible edges                  <= 16384
reduced mathematical bit-length <= 65536
```

### 3.2 Optimizer e aritmetica

`CLEAN`. La formula conservativa §2.2 e il ceiling sono invariati rispetto al
draft verificato. Firma e rename non modificano gli input scientifici o la
composizione d'autorità.

### 3.3 Semantica e governance

`CLEAN`. La firma corrisponde esattamente al gate pre-firma. Il ballot aveva
stato correttamente `NOT EFFECTIVE` fino a questo report. Nessuna deroga di
processo è stata usata. Scope e transizioni restano fail-closed.

## 4. Test riprodotti

```text
Interprete canonico: CPython 3.12.13
PreflightTests + SpearmanTests + RuntimeFailureTests: 22/22 PASS
Suite ml_v3/tests: 540/540 PASS
exit code 0
Protected REV7/Source/CMake/Resources/ml_v2 diff: 0
```

## 5. Effetto normativo risultante

Al freeze immutabile di questo report, l'autorità applicabile diventa:

```text
R23C congelata
+ S6 signed transfer
+ S6 transfer post-signature CLEAN report
+ A1 signed ballot
+ questo A1 post-signature CLEAN report
```

I quattro ceiling per-sottografo sono ora normativamente attivi. Il target
R23C resta immutato.

## 6. Limite operativo esplicito

L'attivazione normativa **non** materializza automaticamente l'enforcement nel
candidate corrente. Il probe esistente resta evidence-only:

```text
provisional_preflight_probe -> etichette diagnostiche
a2_exact()                  -> non consuma provisional_exceeded
status/reason               -> non emessi dal path candidate
fatal blast radius          -> non implementato
```

Di conseguenza nessun evaluator candidate può ancora dichiararsi conforme ad
A1 o produrre un PASS REV8. Serve una tranche separata che implementi:

1. preflight prima di qualsiasi solve/calcolo parziale;
2. `SOLVER_STRUCTURAL_LIMIT_EXCEEDED` sui quattro ceiling;
3. fatal blast radius per ogni sottografo che contribuisce a un gate;
4. reject-path test under/on/over e propagation test;
5. counter-check indipendente sul nuovo commit.

## 7. Fuori scope invariato

```text
17 cap group-level                  = NON ATTIVI
Spearman general variable-marginal  = NON CERTIFICATO
G_eligible/G_defined/G_NA e O-13    = NOT_EVALUATED
enforcement candidate               = NON MATERIALIZZATO
runtime/training                    = NON AUTORIZZATI
REV8 SPEC GO                        = NO
```
