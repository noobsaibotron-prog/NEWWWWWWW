# REV8 — ROUND 2.3 — RESTRICTED RED-TEAM PROTOCOL

**Stato:** pacchetto operativo pronto all’esecuzione; non è un verdetto.
**Modalità:** tre lenti indipendenti, read-only, stesso input hash-pinned.
**Target esclusivo:**
`docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md`
**SHA-256 target:**
`8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041`
**HEAD di contesto:** `52757b352b92a7c5c8b13699a8ba5a8c4286c8fd`
**Base scientifica:** `e2bee113c02356c1dff34201f5d4ef002598a23b`
**Freeze Round 2.2:** `95f1f798a2a97aec960eb86faf5968e73d076564`

```text
REV8 SPEC GO = NO
Code review      = fuori scope
Implementation  = fuori scope
Training        = fuori scope
Repository edit = vietato ai tre reviewer
```

---

## 1. Obiettivo ristretto

Il red-team deve stabilire se il signed draft Round 2.3:

1. trasferisce senza drift le 24 decisioni Round 2.2 e le cinque decisioni R23
   firmate;
2. definisce un evaluator deterministico e implementabile senza decisioni
   scientifiche nascoste;
3. impedisce false PASS, leakage, candidate gaming e fallback approssimati;
4. tratta in modo fail-closed N/A, cap, failure e artefatti ancora pendenti;
5. non usa campi candidate-controlled per cambiare grafo, optimum, K6 o gate.

Non è autorizzato un nuovo redesign. Una preferenza alternativa non è un
finding. Un finding deve mostrare almeno una delle condizioni seguenti:

- contraddizione interna;
- divergenza da una decisione firmata;
- input valido con due interpretazioni normative materiali;
- controesempio che cambia matching, metrica o PASS/FAIL;
- percorso candidate-controlled;
- non-determinismo cross-language o dipendenza dall’ordine;
- algoritmo o artefatto richiesto ma semanticamente non definibile;
- failure/N/A che può essere trasformato in PASS;
- requisito che non può essere testato o falsificato come scritto.

Un artefatto esplicitamente `PENDING_*` non è da solo un finding: lo diventa
soltanto se il testo consente PASS prima della sua materializzazione oppure se
la sua interfaccia resta scientificamente ambigua.

---

## 2. Preflight obbligatorio per ogni reviewer

Ogni reviewer DEVE:

1. lavorare nel worktree
   `/Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-rev8-spec-go`;
2. calcolare SHA-256 del target prima della lettura;
3. interrompersi con `INPUT_HASH_MISMATCH` se non coincide;
4. leggere integralmente il target;
5. leggere soltanto come authority/context:
   - `docs/REV8_SCIENTIFIC_AUTHORITY_BALLOT_PRECOMPILED_V2.md`;
   - `docs/REV8_ROUND2_2_DECISION_REGISTER_FINAL_PATCHED.md`;
   - `docs/REV8_BALLOT_V2_COUNTERCHECK_REPORT.md`;
   - `docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_TEMPLATE_FROZEN.md`;
6. non modificare file, non committare, non generare patch;
7. ricalcolare SHA-256 del target alla fine;
8. dichiarare entrambi gli hash nel report.

Il target è byte-authoritative: non applicare formatter, normalizzazione di
newline o rimozione di Markdown hard-break. La conformità dell’input è
determinata dallo SHA-256, non da una riscrittura stilistica.

Il candidate `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` può essere letto
soltanto per individuare drift o contratti preesistenti; non prevale sul ballot
firmato o sul signed draft.

---

## 3. Tassonomia dei finding

### BLOCKER

Consente un false PASS/leakage/gaming materiale, rende il risultato
non-deterministico, contraddice una decisione firmata o rende impossibile
definire l’evaluator.

### MAJOR

Ambiguità materiale e riproducibile che deve essere corretta prima della patch
candidate, ma non dimostra ancora un false PASS completo.

### MINOR

Incoerenza locale o wording imperfetto senza effetto scientifico materiale.

### NOTE

Hardening, test addizionale o chiarimento non necessario alla conformità.

Verdetto consentito:

```text
CLEAN   = nessun BLOCKER o MAJOR
AMEND   = almeno un MAJOR supportato, nessun BLOCKER
BLOCK   = almeno un BLOCKER supportato
```

MINOR e NOTE non impediscono `CLEAN`, ma devono essere chiaramente separati.

---

## 4. Formato obbligatorio di ogni finding

```text
FINDING_ID:
LENS:
SEVERITY: BLOCKER | MAJOR | MINOR | NOTE
TITLE:
TARGET_QUOTE:
TARGET_LINE:
SIGNED_AUTHORITY_AFFECTED:
PRECONDITIONS:
COUNTEREXAMPLE_OR_EXPLOIT:
EXPECTED_NORMATIVE_RESULT:
ACTUAL_AMBIGUOUS_OR_UNSAFE_RESULT:
PASS_FAIL_IMPACT:
REPRODUCTION:
MINIMAL_CORRECTION:
DOWNSTREAM_DECISIONS_REOPENED:
EPISTEMIC_STATUS: PROVED | REPRODUCED | STRONGLY_SUPPORTED | HYPOTHESIS
```

Un finding senza quote/linea, riproduzione o catena d’impatto non può essere
BLOCKER. Se il controesempio è matematico, usare interi, razionali o bit pattern
espliciti. Se è algoritmico, fornire il grafo completo.

---

## 5. Lens A — Optimizer e autorità numerica

### Prompt standalone

```text
Sei il reviewer indipendente OPTIMIZER/NUMERIC del red-team ristretto REV8.
Lavora read-only. Non modificare il repository.

Target:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256 richiesto:
8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041

Leggi integralmente il target e le quattro authority/context elencate nel
protocollo. Verifica l'hash all'inizio e alla fine.

Attacca esclusivamente:

1. N64, bit-to-rational, interi oltre 2^53 e boundary esatto→binary64;
2. source-sample→tick, RNE unico, half-open intervals e frame alignment;
3. boundary table, interval-refined exp2, W_MAX e saturazione/failure;
4. eligibility esatta ai confini 3/10, 1/2 e cube ratio Resonance;
5. K1, K2, K3, K4 additivo/moltiplicativo ed empty matching;
6. exact objective equality, bit-length e serializzazione;
7. definizione di V*, M*, exhaustive oracle A1 ed equivalenza A2;
8. unicità e total order di M_can tramite intera S poi intera D;
9. occurrence ordinal e molteplicità;
10. cap provisional, preflight e failure policy;
11. impossibilità di usare float, greedy o fallback;
12. implementabilità esatta degli envelope richiesti.

Prova a costruire:

- piccoli grafi con 2–4 GT/pred e più optimum K1–K4;
- razionali con denominatori diversi;
- center ratio vicino al confine cube;
- band/temporal IoU esattamente sul bordo;
- payload duplicati con ordinali permutati;
- casi K1=0;
- casi in cui S è uguale ma D differisce;
- casi in cui una riduzione float cambia l'ordine esatto.

Non contestare una soglia perché preferisci un valore diverso: è firmata.
Contesta soltanto ambiguità, incoerenza, non-determinismo o exploit.

Restituisci un solo verdetto CLEAN|AMEND|BLOCK e i finding nel formato
obbligatorio. Se CLEAN, elenca comunque le classi di controesempio tentate.
```

---

## 6. Lens B — Metriche, supporti e significato statistico

### Prompt standalone

```text
Sei il reviewer indipendente METRICS/STATISTICS del red-team ristretto REV8.
Lavora read-only. Non modificare il repository.

Target:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256 richiesto:
8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041

Leggi integralmente il target e le quattro authority/context elencate nel
protocollo. Verifica l'hash all'inizio e alla fine.

Attacca esclusivamente:

1. invarianti TP/FP/FN/P/R/F1 dentro M*;
2. upper envelope severity, onset e offset;
3. guardie K1=0, N/A e PAIRING_ENVELOPE_UNAVAILABLE;
4. center error Resonance, invarianza K4 e cr_log2_rational64;
5. Spearman pooled multi-unità, midrank, supporto, varianza e singleton rho;
6. equality esatta di rho e pubblicazione binary64;
7. G_eligible/G_defined/G_NA e reason code;
8. gerarchia unit→group→macro, peso gruppo=1 e floor;
9. Type-7 p95 e small-support;
10. distinct-confidence thresholds e Average Precision;
11. N_GT=0, K=0, clean safety e macro-AP;
12. boundary esatto→binary64 e sum_pairwise64;
13. qualsiasi modo di usare M_can per una metrica ambiguity-sensitive.

Prova a costruire:

- due matching in M* con severity/onset/offset diversi;
- K1=0 per ogni metrica;
- gruppi misti defined/N/A;
- più unità nello stesso group_id con supporti diseguali;
- Spearman con tie, zero variance, n=9/10 e rho differenti;
- confidence tie con ordine frame/banda differente;
- no GT/no prediction;
- p95 con G=0,1,2 e valori a metà ULP;
- AP con recall ripetuta;
- center product vuoto e non vuoto.

Non proporre metriche alternative. Verifica soltanto che quelle firmate siano
totali, riproducibili, conservative e incapaci di promuovere tramite N/A.

Restituisci un solo verdetto CLEAN|AMEND|BLOCK e i finding nel formato
obbligatorio. Se CLEAN, elenca comunque le fixture tentate.
```

---

## 7. Lens C — Semantica, identità, security e governance

### Prompt standalone

```text
Sei il reviewer indipendente SEMANTICS/SECURITY/GOVERNANCE del red-team
ristretto REV8. Lavora read-only. Non modificare il repository.

Target:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256 richiesto:
8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041

Leggi integralmente il target e le quattro authority/context elencate nel
protocollo. Verifica l'hash all'inizio e alla fine.

Attacca esclusivamente:

1. kind literal, record_family derivata e impossibilità di auto-dichiararla;
2. class matrix semantic/dynamic e complete_types;
3. evaluation_unit_key su tick, provenance source e secondi diagnostici;
4. problem_type vs problem_type_id e partitioning;
5. structural GT key, duplicate/contradictory, annotator/pass scope;
6. Resonance center_band_index strutturale vs center N64 scientifico;
7. Thinness/DullSound direction;
8. scientific/diagnostic key split e campi esclusi;
9. candidate-controlled severity/confidence/actionable/ID/hash;
10. prediction multiset e occurrence ordinal;
11. cross-family/cross-type matching;
12. activation manifest, dispatcher authority, REV7 isolation e no fallback;
13. status PENDING_* e impossibilità di ottenere SPEC GO prima degli artefatti;
14. traceability 24 decisioni + R23_01–R23_05 e assenza di authority drift.

Prova a costruire:

- record_family iniettata o alterata dal candidato;
- kind extra/errato;
- problem_type string/ID discordanti;
- stessi tick con secondi diagnostici differenti;
- stessi center_band_index ma raw center differenti;
- prediction identiche con ordinali mancanti/duplicati/permutati;
- GT uguali fra annotatori/pass diversi;
- severity/confidence variate a geometria identica;
- batch REV7/REV8 misto;
- manifest assente o hash incoerente;
- cross-family edge;
- artifact PENDING ma report che tenta PASS.

Non bloccare perché un artifact PENDING non è ancora materializzato se il testo
impedisce già il GO. Blocca se il pending può essere aggirato o non ha una
interfaccia normativa sufficiente.

Restituisci un solo verdetto CLEAN|AMEND|BLOCK e i finding nel formato
obbligatorio. Se CLEAN, elenca comunque gli exploit tentati.
```

---

## 8. Consolidamento indipendente

Il consolidatore riceve i tre report senza conoscere in anticipo il verdetto
desiderato. Deve:

1. verificare nuovamente hash e linee citate;
2. riprodurre ogni BLOCKER e MAJOR;
3. eliminare duplicati senza fondere catene causali differenti;
4. declassare finding non riproducibili;
5. non usare voto di maggioranza:
   - un BLOCKER riprodotto implica `BLOCK`;
   - almeno un MAJOR riprodotto e nessun BLOCKER implica `AMEND`;
   - altrimenti `CLEAN`;
6. classificare disaccordi non risolti come `UNRESOLVED`, che impedisce CLEAN;
7. emettere una matrice:

```text
finding_id
lens_origin
independent_reproduction
severity_final
decision_ids_affected
minimal_patch
recheck_required
```

Il consolidatore non modifica il target. Può generare un report separato solo
se autorizzato:

```text
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_REPORT.md
```

---

## 9. Definition of Done del red-team

Il gate è completo soltanto quando:

- i tre reviewer hanno verificato lo stesso SHA all’inizio e alla fine;
- ciascuno ha letto integralmente il target;
- ogni lens ha emesso un verdetto;
- tutti i BLOCKER/MAJOR sono stati riprodotti dal consolidatore;
- il target non è cambiato durante l’audit;
- il report finale distingue evidenza formale, riprodotta e ipotesi;
- nessun agente ha modificato repository o target;
- `REV8 SPEC GO` resta NO indipendentemente dal verdetto.

Un risultato `CLEAN` autorizza soltanto la successiva patch document-only del
candidate secondo governance. Non autorizza codice, training, activation o
SPEC GO.
