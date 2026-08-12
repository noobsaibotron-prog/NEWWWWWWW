# REV8 O-09 — Group-level readiness audit protocol

**Stato:** `PREPARED — NOT EXECUTED`

**Tipo di tranche:** `READ-ONLY AUDIT`

**Group candidate ballot-ready:** `NO`

**Cap group-level:** `17 CANDIDATE VALUES — NON ATTIVI`

**REV8 SPEC GO:** `NO`

## 1. Scopo

Questo protocollo prepara il controllo di readiness delle superfici group-level
REV8 O-09:

1. Average Precision esatta con tie di confidence atomici;
2. coverage B-001 con envelope `coverage_minus`/`coverage_plus`;
3. Spearman group-level su optimum unici o con marginali dimostrabilmente
   fissi;
4. 17 cap strutturali candidate;
5. semantica fail-closed, provenance e blast radius.

L'audit deve stabilire se esistono evidenze sufficienti per **preparare** un
ballot group-level ristretto. Non attiva cap, non modifica codice e non produce
G1c PASS o REV8 SPEC GO.

## 2. Snapshot e authority da usare

### 2.1 Base Git

```text
Branch:
feature/motore-v3-rev8-spec-go

Audit preparation base HEAD:
33474dff4dc49000a9a4b52755a075117aa1f94e

Protected baseline for Source/CMake/Resources/ml_v2:
7e23f1c3
```

L'audit deve registrare il proprio HEAD effettivo e fallire se il worktree non
è pulito o se il perimetro protetto presenta un diff.

### 2.2 Authority composta applicabile

```text
Living G1 contract REV7 SHA-256:
310d538647d71840c6dd8124f1e24281b758bb6dd3e4776aac4b34e5f658a5b0
File:
docs/MOTORE_V3_G1_CONTRACT.md

R23C immutable target SHA-256:
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
File:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md

S6 signed transfer ballot SHA-256:
8dab8b7ad0fb25f5263358dd26600d201a036210d808840d6c8e1d07051649c0
File:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_BALLOT_SIGNED.md

S6 post-signature CLEAN report SHA-256:
b148e6340dd3c9b46ca0f6787209cc6d2df00a6512e34c5bce164b81b1349c7a
File:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_POST_SIGNATURE_RECHECK_REPORT.md

A1 signed ballot SHA-256:
bf31ed0e1bbcfa001114808e569a5155427b54eef3b9dd1cd0d80fcd1c184d75
File:
docs/REV8_O09_ACTIVATION_BALLOT_A1_SIGNED.md

A1 enforcement implementation report SHA-256:
f0a21177035d5161e89d702d06e8ddd0dbefbedc508cb545991a39da9175e565
File:
docs/REV8_O09_A1_ENFORCEMENT_IMPLEMENTATION_COUNTERCHECK_REPORT.md
```

S6 è efficace per composizione. Il target R23C resta immutato. Le sezioni
storiche §7.1/§9 di `REV8_O09_GROUP_BENCHMARK_REPORT.md` descrivono lo stato
precedente al transfer S6 e non sono authority corrente.

### 2.3 Soggetti group-level vivi

```text
ml_v3/benchmark/rev8_o09_group_candidate.py
SHA-256 53033d7fcc872420b5420ff4fb825762ddcfb4defa458ffdfdf82f124e3622f6

ml_v3/benchmark/run_rev8_o09_group_candidate.py
SHA-256 8fd4e3d5ee31ac37ad1870f9fa182a7c26478bc7627b2da731e3fbe26554433e

ml_v3/tests/test_g1c_rev8_o09_group_candidate.py
SHA-256 c3fa1dd621a491770fd498935826a0d753bd04347292ecde60f24ccd25c02143

ml_v3/tests/test_g1c_rev8_o09_group_runner.py
SHA-256 b8b1398e3804a06a10d0758c79685909a15281d643cd8fd346dd2f9ed0423038

Group benchmark report SHA-256:
ae14e4fbf015143d21cec50459c5b6cbd10d861aca7e7a9aa538da8b21667eab

External full evidence SHA-256:
74e4fc9d1ce47393d9654c5372a5cfc2b80009aa23d7777539d89f49757d7c2a
```

Ogni divergenza da questi byte deve essere dichiarata e sposta l'audit sul
nuovo SHA; non è permesso mescolare risultati ottenuti su snapshot diversi.

## 3. Stato iniziale da non reinterpretare

```text
GROUP_CANDIDATE_BALLOT_READY = False

GROUP_CANDIDATE_LIMITATIONS =
  GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED
  G_ELIGIBLE_G_DEFINED_G_NA_AND_GATE_FLOORS_NOT_EVALUATED
  SPEARMAN_RHO64_PUBLICATION_NOT_MATERIALIZED

authority_status = EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE
```

`GroupStatus.CERTIFIED` certifica soltanto il valore matematico prodotto dalla
singola superficie candidate. Non significa `PASS`, ballot readiness, G1c
CLOSE o REV8 SPEC GO.

## 4. Inventario dei 17 cap candidate

Ogni riga richiede: formula del preflight, unità di conteggio, valore, motivo,
fixture `under/on/over`, reason code, momento del rejection e blast radius.

| ID | Metrica | Cap candidate | Valore |
|---|---|---|---:|
| AP-01 | AP | partitions | 32 |
| AP-02 | AP | total GT | 4096 |
| AP-03 | AP | total predictions | 1024 |
| AP-04 | AP | total eligible edges | 16384 |
| AP-05 | AP | distinct confidence thresholds | 1024 |
| AP-06 | AP | sum of local thresholds | 1024 |
| AP-07 | AP | prefix edge incidence | 1056768 |
| CV-01 | Coverage | units | 32 |
| CV-02 | Coverage | maximum partitions per unit | 8 |
| CV-03 | Coverage | total GT | 8192 |
| CV-04 | Coverage | total predictions | 8192 |
| CV-05 | Coverage | total eligible edges | 16384 |
| SP-01 | Spearman | partitions | 16 |
| SP-02 | Spearman | total GT | 128 |
| SP-03 | Spearman | total predictions | 128 |
| SP-04 | Spearman | total eligible edges | 16384 |
| SP-05 | Spearman | matching support bound | 128 |

Il ceiling A1 `exact scalar bit-length <= 65536` e i limiti A1 dei singoli
sottografi restano authority trasversale già attiva; non vanno ricontati come
ulteriori cap group-level.

## 5. Lenti obbligatorie

Tre lenti indipendenti devono leggere lo stesso SHA. Nessuna lente modifica
file.

### L1 — Metriche e statistica

Verifica:

- AP con distinct-confidence threshold e tie atomici;
- ricalcolo K1 a ogni prefix;
- casi `N_GT=0`, prediction vuote e recall monotona;
- coverage B-001, separabilità esatta ed envelope;
- gerarchia unità → gruppo → macro;
- Spearman pooled, midrank, supporto minimo, varianza zero e singleton;
- confine esatto fra O-09 e O-13;
- nessun N/A trasformato in zero o PASS.

### L2 — Optimizer, costi e fail-closed

Verifica:

- tutti i 17 preflight prima del relativo solve;
- nessun solve dopo un exceed;
- assenza di risultati parziali su failure;
- equivalenza con oracle esaustivo sui grafi piccoli;
- bound su numero di solve e bit-length;
- comportamento OOM, timeout, overflow, runtime e modello invalido;
- scaling del caso `spearman_variable_unavailable`;
- presenza di un cap strutturale deterministico sufficiente, senza usare il
  tempo wall-clock come soglia scientifica.

### L3 — Authority, sicurezza e provenance

Verifica:

- S6 corrente per composizione, non la withdrawal storica;
- A1 attiva anche per ogni sottografo group-level;
- nessuna reachability dal dispatcher REV7/live;
- nessuna esportazione package-level non autorizzata;
- `ballot_ready=false` fino alla chiusura formale;
- evidence esterna e source hash corrispondenti;
- nessun claim `PASS`, `SPEC GO`, runtime o training.

## 6. Ipotesi avversariali da falsificare

Questi punti sono **ipotesi di audit**, non verdetti precompilati.

| ID | Ipotesi |
|---|---|
| H-G01 | `_RUNTIME_FAILURES` group-level non include `TimeoutError`. |
| H-G02 | un'eccezione durante `_group_preflight` può uscire prima della traduzione in `GroupResult(REJECTED, ...)`. |
| H-G03 | il group candidate usa il probe diagnostico storico invece della superficie A1 attiva, creando authority/status incoerenti. |
| H-G04 | le fixture correnti non coprono `under/on/over` separatamente per tutti i 17 cap. |
| H-G05 | un exceed combinato può cambiare reason ordering o lasciare iniziare un solve. |
| H-G06 | `spearman_variable_unavailable/64` richiede un bound strutturale aggiuntivo su classi marginali o numero di solve. |
| H-G07 | la pubblicazione `rho64` non ha ancora algoritmo correctly-rounded, golden e mutation test. |
| H-G08 | una policy Spearman N/A valida è confusa con la certificazione matematica del caso generale. |
| H-G09 | i reducer macro accettano soltanto valori definiti e non materializzano `G_eligible/G_defined/G_NA`. |
| H-G10 | lo storico report group-level contiene conclusioni S6 obsolete che possono essere lette come stato corrente. |
| H-G11 | `GroupStatus.CERTIFIED` può essere consumato erroneamente come PASS da un caller futuro. |
| H-G12 | cap group-level e cap A1 per-sottografo non hanno un unico ordine fail-closed definito. |

## 7. Fixture e test minimi

### 7.1 Matrice cap

Per ciascuno dei 17 cap:

```text
under = cap - 1  -> non REJECTED per quel reason code
on    = cap      -> non REJECTED per quel reason code
over  = cap + 1  -> REJECTED / SOLVER_STRUCTURAL_LIMIT_EXCEEDED
```

Il test deve dimostrare che il solver non è chiamato nel caso `over`.

Sono inoltre obbligatori:

- un caso con più cap superati simultaneamente;
- un caso con cap group-level superato e sottografo A1 valido;
- un caso con sottografo A1 invalido ma cap group-level validi;
- permutazione delle partizioni senza variazione scientifica;
- chiavi duplicate e ordine UTF-8 non canonico;
- empty group e supporto nullo.

### 7.2 Failure injection

Iniettare prima nel preflight e poi nel solve:

```text
MemoryError
TimeoutError
RuntimeError
OverflowError
CandidateGraphError / GroupCandidateError
```

Per ogni superficie AP/Coverage/Spearman verificare:

```text
status = REJECTED
reason = SOLVER_RUNTIME_FAILURE oppure SOLVER_CONSTRAINT_MODEL_INVALID
value  = None
nessun risultato parziale pubblicato
```

### 7.3 Metriche

- AP: oracle esaustivo, tie atomici, no trapezio, macro compound-rounding;
- Coverage: oracle esaustivo, `coverage_minus <= coverage_plus`, unità N/A;
- Spearman: optimum unico, fixed-marginal singleton, pairing ambiguous,
  certificate unavailable, support 9/10/11 e varianza zero;
- determinismo su repliche isolate;
- mutation test che uccida qualunque conversione N/A→0/PASS.

### 7.4 Evidence full

Rieseguire il runner full sul platform lock con almeno tre repliche e output
esterno. Il report deve registrare:

- commit e hash di ogni sorgente caricato;
- invocation completa con `-I -S -B`;
- hash file e payload;
- distribuzione tempo/RSS;
- outcome identico fra repliche;
- costo separato di `spearman_variable_unavailable/64`.

## 8. Confine O-09 / Spearman / O-13

L'audit deve produrre tre verdetti distinti:

```text
O09_GROUP_CAP_BALLOT_READY = YES | NO
SPEARMAN_PUBLICATION_READY = YES | NO
O13_SUPPORT_GATE_READY     = YES | NO
```

Regole:

1. S6 già efficace autorizza il reason code
   `SPEARMAN_CERTIFICATE_UNAVAILABLE`; non certifica automaticamente il caso
   general variable-value-marginal.
2. Se il caso generale resta non certificabile, la policy N/A deve essere
   esplicita, fail-closed e incapace di contribuire a un PASS obbligatorio.
3. `rho64` richiede materializzazione e golden separati.
4. `G_eligible/G_defined/G_NA`, floor e p95 Type-7 appartengono a O-13. Un
   audit O-09 CLEAN non chiude O-13 e non chiude G1c.

## 9. Stop-rule e verdetti

### `READINESS_CLEAN`

Consentito solo se:

- nessun false-PASS path;
- tutti i 17 cap hanno semantica e boundary completi;
- preflight e solve sono fail-closed;
- snapshot/provenance sono univoci;
- policy Spearman e rho64 sono classificate correttamente;
- scope O-13 resta separato;
- le tre lenti concordano sullo stesso SHA.

`READINESS_CLEAN` autorizza soltanto la preparazione di un ballot ristretto.

### `READINESS_BLOCKED`

Obbligatorio se esiste almeno uno fra:

- cap non testabile o non deterministico;
- failure non tradotta;
- solve raggiungibile dopo rejection;
- evidence non riproducibile;
- hash/authority ambigui;
- N/A convertibile in PASS;
- costo non bounded da una misura strutturale deterministica;
- semantica non implementabile.

### Finding non bloccanti

Problemi editoriali, performance non-gating o hardening senza false-PASS
vanno nel durable debt register e non giustificano threshold shopping.

## 10. Deliverable dell'audit

L'esecuzione futura deve produrre esclusivamente:

```text
docs/REV8_O09_GROUP_READINESS_AUDIT_REPORT.md
docs/REV8_O09_GROUP_CAP_MATRIX.md
docs/REV8_O09_GROUP_READINESS_FINDINGS.md   # soltanto se necessario
```

Il report deve contenere:

- HEAD e hash completi;
- esito di ogni lente;
- matrice 17/17;
- test e command line;
- finding con repro;
- tre verdict separati O-09/Spearman/O-13;
- unica next permitted action.

Durante l'audit non sono autorizzati:

```text
modifiche a Python/C++
modifiche a REV7 o R23C
attivazione cap
firma ballot
runtime/training/plugin
push
REV8 SPEC GO
```

## 11. Sequenza dopo l'audit

### Se `READINESS_BLOCKED`

```text
finding report
-> tranche di fix isolata
-> test completi
-> nuovo commit immutabile
-> nuovo audit a tre lenti
```

### Se `READINESS_CLEAN`

```text
ballot group-level ristretto
-> counter-check pre-firma
-> firma dell'autorità
-> recheck post-firma
-> tranche enforcement separata
-> counter-check implementativo
```

Anche dopo questa sequenza:

```text
G1c CLOSE     = NO finché O-13 e gli altri ledger item non sono chiusi
REV8 SPEC GO  = NO
runtime       = invariato
training      = non autorizzato
```
