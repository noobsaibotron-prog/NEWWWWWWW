# REV8 — ROUND 2.2 — DECISION REGISTER FINAL — FROZEN

**Stato globale:**
```
REV8 SPEC GO = NO
G1c close    = NO
G1 PASS      = NO
Training V3  = NO
```
**Base immutabile:** feature/motore-v3-g1c-reseal @ e2bee113 — checkpoint document-only, non modificare
**REV7 vivo:** deve restare intatto — non modificare schemas.py, validate.py, event_matching.py live
**Natura:** registro decisionale autosufficiente, non specifica normativa finale, non patch candidate, non implementazione
**Precedenza:** correzioni finali 2 ULP + A1 non-fatal + K5_RESERVED hanno precedenza su V2 + addendum + master handoff

## 0. Etichette epistemiche

- **FORMALLY PROVED** — deducibile da matematica/testo senza repo
- **REPORTED / REPRODUCIBLE EVIDENCE** — risultato riferito da altro agente, non verificato indipendentemente
- **STRONGLY SUPPORTED** — controesempio convincente ma fixture incompleta nel materiale esterno
- **PROPOSED** — proposta normativa
- **OPEN** — aperto
- **REJECTED** — rigettato

## 1. Correzioni obbligatorie finali applicate

### C7 — 2 ULP, non 1 ULP — FORMALLY PROVED dai bit

```
 0.0                 0x0000000000000000
 1.0                 0x3FF0000000000000
 0.2                 0x3FC999999999999A
-0.2                 0xBFC999999999999A
 1.2                 0x3FF3333333333333
 N64(1.2)-N64(1.0)   0x3FC9999999999998
 0.25                0x3FD0000000000000
 1.25                0x3FF4000000000000
```
0x...99A - 0x...998 = 2 incrementi nel total order dei binary64 positivi. Spacing 2^-55 diff 2^-54 = 2 ULP. d=0.2 REJECTED, d=0.25 STRONGLY SUPPORTED. Fixture: leggere bit N64, convertirli in razionali esatti, differenze con aritmetica razionale. C7-A numeric sensitivity separata da C6-B onset/offset ambiguity.

### A1 — infeasibility interna non fatal

**Non-fatal interni A1:** OPT_SUBPROBLEM_INFEASIBLE, OPT_VALUE_DIFFERS_FROM_GLOBAL_OPTIMUM -> e_i forbidden continua, nessun FAIL report
**Fatali:** SOLVER_STRUCTURAL_LIMIT_EXCEEDED, SOLVER_RUNTIME_FAILURE, SOLVER_CONSTRAINT_MODEL_INVALID — tutti impediscono PASS. Non usare SOLVER_INFEASIBLE_CONSTRAINT_SET generico.

### Formule

- I=max(0,min(b,d)-max(a,c)+1), U=(b-a+1)+(d-c+1)-I, IoU=I/U razionale esatto, soglia 2I>=U, **1-IoU=(U-I)/U**

## 2. Decisioni — 24 ID — 23 nel V2 + O-04a = 24

### O-01 — Temporal Authority — C7 — Blocco A
- **Blocco:** A — spazio matematico
- **Blocker parent:** B1 C7 semantic instability
- **Stato operativo:** UNDECIDED — YELLOW design blocker
- **Stato epistemico:** FORMALLY SHOWN 2 ULP sensitivity, THREAT MODEL TO BE DEMONSTRATED gaming
- **Fatto/problema:** matching temporale deterministico ma sensibile a 2 ULP. GT[0,1] P_A[-0.25,1] P_B[0,1.25] IoU 0.8 K3 0.25 entrambe ma onset/offset divergono. C7-A numeric sensitivity distinta da C6-B onset/offset ambiguity.
- **Alternative:** A) feature frame interi, B) tick sample/hop-derived exact, C) sample index diretti, D) binary64 esatti severi deterministici
- **Raccomandazione:** B — raw annotation -> canonical source-sample tick -> deterministic mapping frontend tick -> matching
- **Dipendenze:** nessuna, radice — parsing binary64 generale sotto O-18/N64 non solo O-01
- **Criterio chiusura:** scelta A/B/C/D + risposte inter-frame, granularità, equivalenza 44.1/48/96, sorgente vs resampled, segment inclusivi/semiaperti, causal alignment + 2 fixture IoU 0.8
- **Owner:** metrologia + frontend
- **Test/fixture:** exhaustive |GT|<=3 |Pred|<=3 diadici 0.25 vs non-diadici, fixture 2 ULP con bit corretti 0x3FC999999999999A vs 0x3FC9999999999998

### O-02 — Boundary Artifact Bit-Exact — Blocco A
- **Blocco:** A
- **Blocker parent:** B5 projection
- **Stato operativo:** DECIDED_PENDING_ARTIFACT
- **Stato epistemico:** PROPOSED — single authority boundary table golden, argmin log-distance solo spiegazione matematica
- **Fatto/problema:** doppia procedura argmin vs boundary pre-arrotondati diverge per rounding
- **Alternative:** A) boundary table golden autoritativa, B) procedura argmin pinnata con boundary solo fixture
- **Raccomandazione:** A
- **Dipendenze:** canonical center grid, grid-version authority, O-03 only where width policy affects raw endpoints — non O-01 — O-01 era residuo copia/incolla, O-02 riguarda proiezione frequenziale non temporale
- **Regola normativa proposta sugli estremi:**
  x non-finite -> FAIL prima di projection
  center_hz non-finite o <=0 -> FAIL
  width_octaves non-finite o <0 -> FAIL (v2 ammette >=0)
  raw_lo > raw_hi -> FAIL
  finite raw fuori griglia -> saturazione banda 0 o 119, non FAIL
  non-finite raw -> FAIL
  band 0 se x <= boundary[0]
  band k se boundary[k-1] < x <= boundary[k] per 1<=k<=118
  band 119 se x > boundary[118]
- **Criterio chiusura:** artefatto 119 valori bit-exact lowercase hex 0x..., ordine crescente, UTF-8 LF, canonical JSON chiavi ordinate no whitespace, center_hash=SHA256(canonical center bit list), boundary_hash=SHA256(canonical boundary bit list), artifact_hash=SHA256(canonical artifact excluding artifact_hash) includes center_hash and boundary_hash, generator interval arithmetic: read center bits exact -> rationals -> sqrt(center_i*center_{i+1}) interval -> increase precision until interval rounds same binary64 -> emit bits, generator report non-normativo con index/input/output/precision/rounding/refinement, + regola estremi reinserita integralmente
- **Owner:** G1b + G1a
- **Test/fixture:** mutation rimuove tie rule deve fallire, H/S canonical equivalence, hash stability, generator correctly-rounded via interval refinement

### O-03 — Width Bound + Aliasing — Blocco A
- **Blocco:** A
- **Blocker parent:** B9
- **Stato operativo:** UNDECIDED su semantic, DECIDED_PENDING_NORMATIVE_WORDING su safety
- **Stato epistemico:** PROPOSED safety, THREAT MODEL gaming per semantic
- **Fatto/problema:** (center1,width1)!=(center2,width2) ma B1=B2 aliasing
- **Alternative:** solo safety vs safety+semantic per classe
- **Raccomandazione:** safety normativo W_MAX≈19.93, semantic diagnostico poi gate
- **Dipendenze:** O-02
- **Criterio chiusura:** W_MAX pinnato + separazione L0/L2/L3/L4 + metamorfica H/S PROPOSED REQUIREMENT B(x)=B(y)=>Eligibility e GeometryCost uguali ∀z
- **Owner:** C1 owner
- **Test/fixture:** aliasing fixture (center,width) diversi stessa banda => eligibility/cost uguali per H/S

### O-04a — Eligible Edge Predicate per classe — Blocco A — CRITICA NUOVA
- **Blocco:** A
- **Blocker parent:** B2/B5
- **Stato operativo:** UNDECIDED — critica
- **Stato epistemico:** OPEN framework mancante
- **Fatto/problema:** O-04..O-07 definiscono obiettivi dopo creazione archi ma non quando arco esiste — senza non esiste grafo bipartito normativamente definito
- **Alternative:** A) larga tutti stessa unità/tipo, B) stretta con soglie hard, C) hard minimale incompatibilità semantica + similarità in K2-K5 — Raccomandazione C
- **Raccomandazione:** C — hard exclusion solo: diversa evaluation_unit_key, diverso problem_type, invalid trusted validity mask, incompatibilità direction semanticamente necessaria, assenza relazione temporale solo se giustificata normativamente. No soglie hard arbitrarie IoU/center distance. Ogni soglia hard richiede motivo scientifico + aritmetica esatta + fixture dentro/bordo/fuori + analisi effetto K1/TP/FP/FN
- **Dipendenze:** O-01 temporal geometry, O-02 band
- **Criterio chiusura:** formula completa per classe senza discrezionalità + per ogni soglia hard motivazione + fixture appena dentro/esatta sul bordo/appena fuori + analisi effetto K1
- **Owner:** C1/C2/C4 + metrologia
- **Test/fixture:** fixture dentro/fuori/bordo per ogni classe

### O-04 — K2 Exact — Somma IoU — Blocco A
- **Blocco:** A
- **Blocker parent:** B2 OPT exact
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** PROPOSED direction
- **Fatto/problema:** K2 massimizza somma IoU, serve aritmetica razionale esatta
- **Alternative:** somma razionale esatta con riduzione canonica vs altre rappresentazioni
- **Raccomandazione:** somma esatta razionale, direction maximize
- **Dipendenze:** O-04a, O-02
- **Criterio chiusura:** definizione K2 per classe, direzione, aritmetica, tipo
- **Owner:** G1c
- **Test/fixture:** fixture I/U esatti [7,7] vs [8,8]=0, [7,7] vs [7,7]=1, 2I>=U threshold, bit complexity bound

### O-05 — K3 Exact Temporal Cost — Blocco A
- **Blocco:** A
- **Blocker parent:** B2
- **Stato operativo:** UNDECIDED — dipende O-01
- **Stato epistemico:** OPEN
- **Fatto/problema:** costo temporale per classe, aritmetica, uguaglianza, direction minimize
- **Alternative:** costo su tick interi vs ms
- **Raccomandazione:** basato su tick canonici O-01
- **Dipendenze:** O-01, O-04a
- **Criterio chiusura:** formula per classe + dipendenza tick
- **Owner:** G1c
- **Test/fixture:** fixture temporale diadica 0.25, fixture 2 ULP sensitivity

### O-06 — K4 Geometric Cost — Blocco A
- **Blocco:** A
- **Blocker parent:** B2
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** OPEN
- **Fatto/problema:** costo geometrico successivo, distinzione H/S banda autoritativa vs Resonance center osservabile
- **Alternative:** H/S solo banda, Resonance center
- **Raccomandazione:** H/S solo banda, Resonance center via O-06a
- **Dipendenze:** O-02, O-04a
- **Criterio chiusura:** formula per classe
- **Owner:** G1c
- **Test/fixture:** H/S canonical equivalence test

### O-06a — Resonance Center-Cost Numeric Authority + Product — Blocco A
- **Blocco:** A
- **Blocker parent:** B2/B5
- **Stato operativo:** UNDECIDED — PREFERRED CANDIDATE pending proof
- **Stato epistemico:** PROPOSED con precondizioni
- **Fatto/problema:** se K4=sum|log2(cGT/cP)| log2 trascendentale non razionale esatto, libm diverse non bit-identiche
- **Alternative:** A) log2 deterministico pinnato + golden, B) lookup fixed-point, C) binary64 reference + golden, D) metrica diversa esatta monotona, E) exact product r_e=max/min Prod r_e argmin sum|log2|=argmin Prod — PREFERRED CANDIDATE E
- **Raccomandazione:** E — per ogni arco r_e=max(cGT,cP)/min(...), K4 minimizza Prod r_e, equivalente somma ottave senza libm. Precondizioni: centri finiti >0, K4 non pesata, cardinalità già fissata K1, centri N64 razionali esatti, confronto esatto, nessun peso reale/non intero, bound bit-length. Non imporre gcd dopo ogni passo, pinnare risultato matematico esatto, libera implementazione: cancellazione incrementale, cross multiplication, big integer, separazione potenze due, riduzione quando necessaria.
- **Dipendenze:** O-04a, O-02
- **Criterio chiusura:** scelta A-E + formula + algoritmo deterministico + cross-lang reproducibility + equality + golden bordi/tie + prova equivalenza per E + bit-complexity proof
- **Owner:** metrologia + G1c
- **Test/fixture:** golden bordi/tie, cross-lang, product vs sum log equivalence, bit-length bound

### O-07 — K5_RESERVED = unused in REV8 — Blocco A
- **Blocco:** A
- **Blocker parent:** B2
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** OPEN — non inventare chiave
- **Fatto/problema:** nomenclatura storica K5 non implica necessità scientifica
- **Alternative:** A) K5 esiste con funzione per-classe esplicita indipendente giustificata non controllabile da candidate, B) K5_RESERVED = unused in REV8, K6 segue K4
- **Raccomandazione:** K5 exists only if it represents an independently justified, candidate-uncontrolled scientific preference. Otherwise K5_RESERVED = unused in REV8. Norm: K5 does not participate in eligibility, optimization, matching equivalence or canonicalization. K6 follows K4.
- **Dipendenze:** O-04..O-06a
- **Criterio chiusura:** decisione A/B + se A definizione esatta per classe
- **Owner:** autorità scientifica
- **Test/fixture:** se B test che K5 non influenza M*, se A test per-classe

### O-08 — OPT(R,F) e A1 — Blocco B
- **Blocco:** B — optimizer
- **Blocker parent:** B2 RED centrale
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** PROPOSED reference candidate A1
- **Fatto/problema:** OPT esatto non definito, A1 pseudocodice non specifica implementabile, infeasibility interna confusa con fatal failure
- **Alternative:** A1 reference V*=OPT(∅,∅) repeated optimization, A2 cost vector optimization candidate
- **Raccomandazione:** A1 reference, A2 optimization con prova equivalenza. Distinzione: OPT_SUBPROBLEM_INFEASIBLE e OPT_VALUE_DIFFERS non-fatal -> forbidden continua, SOLVER_STRUCTURAL_LIMIT_EXCEEDED / RUNTIME_FAILURE / CONSTRAINT_MODEL_INVALID fatal impediscono PASS. Non usare SOLVER_INFEASIBLE_CONSTRAINT_SET generico.
- **Dipendenze:** O-04a, O-04..O-07, O-08a, O-20
- **Criterio chiusura:** 3 livelli: numeric fixture float vs exact, oracle vs exhaustive |GT|<=3, A1 proof con invariante: dopo ogni iterazione esiste almeno un matching in M* compatibile con R,F + prova incidenza lessicograficamente massima
- **Owner:** G1c
- **Test/fixture:** numeric fixture float vs exact, exhaustive oracle, A1 invariant proof

### O-08a — Objective Vector Representation and Equality — Blocco B
- **Blocco:** B
- **Blocker parent:** B2
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** OPEN
- **Fatto/problema:** confronto V_i==V* non definito senza tipo componenti, riduzione canonica, equality exact, bit complexity
- **Alternative:** frazione ridotta vs numeratore/denominatore vs big int non ridotti temporaneamente vs separazione potenze due vs cancellazione incrementale vs cross multiplication
- **Raccomandazione:** Objective components represent exact mathematical values. Equality and ordering are exact. Any internal representation is allowed if observationally equivalent. Canonical reduction is required only at normative serialization boundaries, not necessarily after each arithmetic operation. Bound bit-length provato.
- **Dipendenze:** O-04..O-07
- **Criterio chiusura:** tipo ogni componente, equality exact, riduzione solo a serialization boundaries, limiti crescita, comportamento N/A/infeasible
- **Owner:** G1c
- **Test/fixture:** equality fixture, bit complexity bound, serialization canonical test

### O-09 — FAIL Blast Radius — Blocco B
- **Blocco:** B
- **Blocker parent:** B6
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** PROPOSED con distinzione cap vs runtime
- **Fatto/problema:** non definito se FAIL unità/gruppo/report e se report con FAIL può PASS, memoria non deterministica come soglia
- **Alternative:** FAIL unità vs gruppo vs report
- **Raccomandazione:** cap deterministici preflight: max GT, max prediction, max archi, max bit-length, max chiamate OPT, budget strutturale calcolabile prima. Failure operativi: OOM, eccezione runtime, indisponibilità, corruzione. Cap rilevato prima calcolo parziale. Reason code: SOLVER_STRUCTURAL_LIMIT_EXCEEDED fatal, SOLVER_RUNTIME_FAILURE fatal, SOLVER_CONSTRAINT_MODEL_INVALID fatal, OPT_SUBPROBLEM_INFEASIBLE non-fatal, OPT_VALUE_DIFFERS non-fatal. Qualsiasi fatal in unità che contribuisce a gate impedisce PASS, ammessi report diagnostici parziali non PASS scientifico.
- **Dipendenze:** O-08
- **Criterio chiusura:** tabella blast radius unit/group/report + regola no PASS con fatal + reason code distinti
- **Owner:** G1c
- **Test/fixture:** structural limit detection before partial compute, reason code distinct test

### O-10 — Severity MAE Policy — Blocco C
- **Blocco:** C — statistica
- **Blocker parent:** B3 metric pairing
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** STRONGLY SUPPORTED sensitive fattore 7x
- **Fatto/problema:** severity MAE ambiguity-sensitive GT 0.80 P_A 0.10 MAE 0.70 P_B 0.90 MAE 0.10 K1-K5 identiche
- **Alternative:** envelope [Q_min,Q_max] gate su Q_max vs N/A/FAIL
- **Raccomandazione:** envelope se computabile esattamente additiva probabile altrimenti N/A/FAIL
- **Dipendenze:** O-08
- **Criterio chiusura:** scelta policy + algoritmo esatto + gate behavior + prova computabilità
- **Owner:** metrologia
- **Test/fixture:** severity permutation test among equivalent predictions

### O-11 — Onset/Offset Policy — Blocco C
- **Blocco:** C
- **Blocker parent:** B3/B1
- **Stato operativo:** UNDECIDED — dipende O-01
- **Stato epistemico:** STRONGLY SUPPORTED sensitive con d=0.25 GT[0,1] P_A[-0.25,1] P_B[0,1.25] IoU 0.8 K3 0.25
- **Fatto/problema:** onset 250 vs 0 offset 0 vs 250 con stessi K1-K5
- **Alternative:** envelope vs N/A/FAIL
- **Raccomandazione:** envelope probabile additiva
- **Dipendenze:** O-01, O-08
- **Criterio chiusura:** come O-10
- **Owner:** metrologia
- **Test/fixture:** onset/offset fixture diadica 0.25

### O-12 — Spearman Policy — Blocco C
- **Blocco:** C
- **Blocker parent:** B3
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** STRONGLY SUPPORTED sensitive rho +1/-1 delta 2.0 support 10 fixture incompleta
- **Fatto/problema:** M1 rho +1 MAE 0 M2 rho -1 MAE 0.475 delta 2.0 — REPORTED +1/-1
- **Alternative:** probabilmente N/A/FAIL perché envelope rho su M* non dimostrato computabile
- **Raccomandazione:** pairing ambiguity rilevata -> Spearman N/A, se gate obbligatorio allora FAIL finché non esiste algoritmo esatto envelope
- **Dipendenze:** O-08
- **Criterio chiusura:** scelta N/A/FAIL vs envelope + prova computabilità + fixture completa 10 GT +20 Pred con K1-K5 e prova optimum globale
- **Owner:** metrologia
- **Test/fixture:** Spearman full fixture with geometries, K1-K5, no cross-match, proof both global optimum

### O-13 — Aggregators + Quantile Convention — Blocco C
- **Blocco:** C
- **Blocker parent:** B4 RED centrale
- **Stato operativo:** UNDECIDED — completamente aperto
- **Stato epistemico:** OPEN
- **Fatto/problema:** candidate elenca errore center/onset/offset ma non pinna mean/p95/max, published vs gate vs diagnostic, ordine aggregazione, peso gruppo=1
- **Alternative:** mean/p95/max combinazioni, nearest-rank vs interpolazione, zero/one-based, small-support, tie, N/A threshold, gruppi senza matched
- **Raccomandazione:** definire 5 dimensioni + quantile esatto, rispettare peso gruppo=1
- **Dipendenze:** O-01, O-10..O-12
- **Criterio chiusura:** formule esatte per 3 metriche + quantile method + small-support rule + rispetto peso gruppo=1
- **Owner:** metrologia
- **Test/fixture:** aggregator weight test gruppo pesa 1, quantile convention test small-support

### O-14 — PR-AUC Distinct Confidence Pin — Blocco C
- **Blocco:** C
- **Blocker parent:** B4/B3
- **Stato operativo:** DECIDED_PENDING_NORMATIVE_WORDING
- **Stato epistemico:** PROPOSED — quasi chiudibile
- **Fatto/problema:** §9.3 ordina per conf desc frame band e taglia posizionalmente — con confidence identiche frame/band decide punto PR
- **Alternative:** cut-point su valori distinti vs posizionali
- **Raccomandazione:** P_t={p:conf>=t} con t su valori distinti confidence calibrata, entrata atomica pari merito, ordine solo presentazione
- **Dipendenze:** nessuna
- **Criterio chiusura:** wording normativo esatto + prova invarianza TP/FP/FN
- **Owner:** metrologia
- **Test/fixture:** distinct-confidence fixture tie

### O-14b — PR Metric Integration / Average Precision Convention — Blocco C — NUOVA
- **Blocco:** C
- **Blocker parent:** B4
- **Stato operativo:** UNDECIDED — pin non sufficiente
- **Stato epistemico:** OPEN
- **Fatto/problema:** pin chiude tie ma non definisce area
- **Alternative:** gradini vs trapezoidale, ordine thresholds, punto iniziale/finale, recall duplicate, caso senza GT positivi, senza Pred, macro per gruppo, calibration authority
- **Raccomandazione:** definire gradini vs trapezoidale + casi limite
- **Dipendenze:** O-14
- **Criterio chiusura:** convention completa pin + integrazione
- **Owner:** metrologia
- **Test/fixture:** PR integration fixture per AP/alternative selezionata: no GT, no Pred, duplicate recall, confidence tie e macro a peso gruppo uguale

### O-15 — C4 Structural Keys per classe — Blocco A
- **Blocco:** A
- **Blocker parent:** B7
- **Stato operativo:** UNDECIDED — H/S/Resonance/Thinness/DullSound PROPOSED STRUCTURAL RULES, Muddiness/Boominess/BoxyMidrange NOT YET SPECIFIED
- **Stato epistemico:** PROPOSED vs NOT YET SPECIFIED
- **Fatto/problema:** vecchia regola stessa porzione tempo/frequenza anche se differivano center/direction/severity REJECTED
- **Alternative:** structural target con N64 timing, canonical band, N64 center dove normativo, direction dove normativo, no fuzzy
- **Raccomandazione:** come alternative, definire normalized timing esatto dipende O-01, contradiction fields, relazione C7 aliasing
- **Dipendenze:** O-01, O-02
- **Criterio chiusura:** structural target per ogni classe con N64 timing, canonical band, N64 center dove normativo, direction dove normativo, no fuzzy
- **Owner:** C4 owner + metrologia
- **Test/fixture:** duplicate vs contradictory vs distinct per classe, no fuzzy test

### O-16 — Kind Literals — Blocco D
- **Blocco:** D — authority
- **Blocker parent:** B8.1
- **Stato operativo:** DECIDED_PENDING_TRANSFER_AND_TEST
- **Stato epistemico:** PROPOSED
- **Fatto/problema:** kind entrano in hash instance_id ma solo impliciti
- **Alternative:** lista esatta
- **Raccomandazione:** semantic_region, dynamic_event, semantic_bundle, prediction_event pinnati
- **Dipendenze:** nessuna
- **Criterio chiusura:** lista pinnata + test hash
- **Owner:** G1a
- **Test/fixture:** hash instance_id cambia se kind cambia

### O-17 — Problem Type ID nel Decision Key — Blocco D
- **Blocco:** D
- **Blocker parent:** B8.2
- **Stato operativo:** DECIDED_PENDING_NORMATIVE_WORDING
- **Stato epistemico:** OPEN con ragione plausibile
- **Fatto/problema:** semantic_payload include problem_type_id, vecchia decision_key lo ometteva
- **Alternative:** includere vs omettere se partizionato
- **Raccomandazione:** omettere se matching partizionato per problem type con motivo normativo esplicito
- **Dipendenze:** O-04a partitioning
- **Criterio chiusura:** ragione esplicita scritta
- **Owner:** G1a
- **Test/fixture:** test partitioning rationale

### O-18 — Sum Pairwise64 + N64 Parsing Authority — Blocco D
- **Blocco:** D
- **Blocker parent:** B8.3 + N64 general authority
- **Stato operativo:** DECIDED_PENDING_ARTIFACT_TEST
- **Stato epistemico:** PROPOSED
- **Fatto/problema:** separazione exact rational optimizer vs sum_pairwise64 metriche pubblicate binary64 senza ownership + N64 parsing, bit-to-rational conversion e arithmetic ownership devono avere autorità generale distinta dalla sola temporal authority O-01. Parsing binary64 generale sotto O-18/N64 non solo O-01.
- **Alternative:** ownership esplicita + N64 authority generale
- **Raccomandazione:** exact rational -> optimizer, sum_pairwise64 -> published binary64 metrics + pin round-to-nearest ties-to-even, no fast-math/excess precision nelle operazioni binary64 normative, conversione bit-exact a razionale e golden test
- **Dipendenze:** nessuna per ownership, O-01 dipendenza rimossa per N64 — authority generale
- **Criterio chiusura:** ownership esplicita + tranche §14.1 + golden + mutation tests + pin round-to-nearest ties-to-even, no fast-math/excess precision, conversione bit-exact a razionale e golden test
- **Owner:** metrologia
- **Test/fixture:** golden sum_pairwise64, mutation, 0.2/1.2 bit fixture 0x3FC999999999999A vs 0x3FC9999999999998, signed zero normalization, finite/non-finite admission, cross-language bit-to-rational equivalence

### O-19 — Revision Authority — Blocco D
- **Blocco:** D
- **Blocker parent:** B8.4
- **Stato operativo:** DECIDED_PENDING_TRANSFER_AND_TEST
- **Stato epistemico:** PROPOSED
- **Fatto/problema:** constants.CONTRACT_REVISION resta provenance storica REV7
- **Alternative:** dispatcher/manifest vs constants
- **Raccomandazione:** historical constants -> provenance, dispatcher/activation manifest -> active evaluator authority
- **Dipendenze:** nessuna
- **Criterio chiusura:** autorità dichiarata + test no fallback REV7/REV8
- **Owner:** G1a/G1c
- **Test/fixture:** revision authority test

### O-20 — Decision Key Form — Blocco B/D
- **Blocco:** B/D
- **Blocker parent:** B8.5
- **Stato operativo:** UNDECIDED
- **Stato epistemico:** OPEN
- **Fatto/problema:** dopo separazione L0-L7 vecchia decision_key va rimessa in discussione, probabile split identity/matching/canonical edge key, nessuna metrica candidate-controlled in K6
- **Alternative:** split identity/matching/canonical vs single
- **Raccomandazione:** split identity key / matching key / canonical edge key, nessuna metrica candidate-controlled in K6, occurrence_ordinal solo per copie identiche
- **Dipendenze:** O-04a, O-02, O-01
- **Criterio chiusura:** forma pinnata + nessuna metrica candidate-controlled in K6
- **Owner:** G1a/G1c
- **Test/fixture:** decision key form test, ID/hash/severity/confidence not in K6

## 3. Tabella finale unica — 24 ID — 23+1

| ID | Titolo | Blocco | Blocker | Stato operativo |
|---|---|---|---|---|
| O-01 | Temporal authority C7 2 ULP | A | B1 | UNDECIDED YELLOW |
| O-02 | Boundary artifact bit-exact | A | B5 | DECIDED_PENDING_ARTIFACT |
| O-03 | Width bound + aliasing | A | B9 | UNDECIDED / PENDING wording safety |
| O-04a | Eligible edge predicate | A | B2/B5 | UNDECIDED critica NUOVA |
| O-04 | K2 sum IoU exact | A | B2 | UNDECIDED |
| O-05 | K3 temporal cost | A | B2 | UNDECIDED dipende O-01 |
| O-06 | K4 geometric cost | A | B2 | UNDECIDED |
| O-06a | K4 Resonance numeric authority + product | A | B2/B5 | UNDECIDED PREFERRED E |
| O-07 | K5_RESERVED = unused in REV8 | A | B2 | UNDECIDED |
| O-08 | OPT(R,F) e A1 infeasibility | B | B2 RED | UNDECIDED |
| O-08a | Vector representation equality | B | B2 | UNDECIDED NUOVA |
| O-09 | FAIL blast radius cap vs runtime | B | B6 | UNDECIDED |
| O-10 | Severity MAE policy | C | B3 | UNDECIDED |
| O-11 | Onset/Offset policy | C | B3/B1 | UNDECIDED |
| O-12 | Spearman policy | C | B3 | UNDECIDED |
| O-13 | Aggregators + quantile | C | B4 RED | UNDECIDED |
| O-14 | PR-AUC distinct-conf pin | C | B4/B3 | DECIDED_PENDING_WORDING |
| O-14b | PR metric integration / Average Precision convention | C | B4 | UNDECIDED NUOVA |
| O-15 | C4 structural keys | A | B7 | UNDECIDED PROPOSED vs NOT YET |
| O-16 | Kind literals | D | B8.1 | DECIDED_PENDING_TRANSFER |
| O-17 | problem_type_id in decision_key | D | B8.2 | DECIDED_PENDING_WORDING |
| O-18 | sum_pairwise64 ownership | D | B8.3 | DECIDED_PENDING_ARTIFACT_TEST |
| O-19 | Revision authority | D | B8.4 | DECIDED_PENDING_TRANSFER |
| O-20 | Decision key split | B/D | B8.5 | UNDECIDED |

Conteggio corretto: 23 decision ID nel V2 + O-04a = 24 totali — non 20+4.

## 4. Governance

- modifica autorizzata nel freeze = aggiunta document-only dei tre artefatti Round 2.2 sulla branch dedicata
- codice/runtime/schema/training modificati = NO
- contenuto del candidate REV8 alla base e2bee113 modificato = NO
- base e2bee113 = riferimento immutabile; il freeze è identificato dal commit Git document-only della branch dedicata
- REV8 SPEC GO dichiarato = NO
- Round 2.3 normativo scritto = NO, solo template separato
- Schema/validator/matcher changes = NO
- Training/promozione = NO
- Parsing binary64: round-to-nearest ties-to-even, no excess precision, no fast-math — generale sotto O-18/N64, non solo O-01 — C7-A numeric sensitivity separata da C6-B onset/offset ambiguity
