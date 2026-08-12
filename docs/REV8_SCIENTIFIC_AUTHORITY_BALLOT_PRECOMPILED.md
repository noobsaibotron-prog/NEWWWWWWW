# REV8 — SCIENTIFIC AUTHORITY BALLOT — PRECOMPILATO — NON FIRMATO

**Stato:** REV8 SPEC GO = NO — G1c close = NO — Training V3 = NO
**Base:** e2bee113 immutabile document-only
**Scopo:** contiene soltanto decisioni UNDECIDED che richiedono scelta umana/scientifica. Le scelte sono precompilate come raccomandazioni Codex/Muse, ma restano prive di autorità finché Marco non firma o respinge ogni riga. Non chiedere nuova firma per DECIDED_PENDING_* salvo dipendenza reale
**Priorità:** Blocco A -> B -> C — non decidere metrica prima di grafo e costi
**Correzioni applicate:** K5_RESERVED = unused in REV8 (non constant), O-02 dipendenza corretta non O-01, O-08a implementation-neutral, O-20 dipende da O-01/O-02, O-14b distingue esplicitamente Average Precision da una generica PR-AUC

---

## Blocco A — Spazio matematico

### O-01 — Temporal Authority — C7 — 2 ULP
- **ID:** O-01
- **Decisione:** tempo autoritativo prima matching? 0.2=0x3FC999999999999A vs N64(1.2)-N64(1.0)=0x3FC9999999999998 distanza 2 ULP FORMALLY PROVED, fixture razionali esatti non sottrazione runtime, C7-A numeric sensitivity distinta da C6-B onset/offset ambiguity
- **Alternative:** A) feature frame interi, B) tick sample/hop-derived exact, C) sample index diretti, D) binary64 esatti severi deterministici
- **Raccomandazione Muse:** B
- **Rischio:** mapping sorgente->frontend deterministico complesso
- **Impatto se rinviata:** blocca O-04a,O-05,O-11,O-13,O-15
- **Dipendenze:** nessuna, radice — parsing binary64 generale sotto O-18/N64 non solo O-01
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** B — tempo canonico in tick interi da 1/48000 s; conversione N64×48000 con round-to-nearest, ties-to-even; intervalli [start_tick,end_tick) semiaperti; durata nulla → FAIL.
- **Motivazione precompilata:** Elimina la sensibilità rappresentazionale binary64 dal matching mantenendo una granularità più fine del feature frame e una base comune fra 44.1/48/96 kHz.
- **Firma/nome:**
- **Data:**

### O-03 — Width Bound + Aliasing
- **ID:** O-03
- **Decisione:** W_MAX safety + semantic width bound + aliasing
- **Alternative:** solo safety vs safety+semantic per classe
- **Raccomandazione Muse:** safety normativo W_MAX≈19.93, semantic diagnostico poi gate
- **Rischio:** semantic arbitrario presto, solo safety lascia aliasing
- **Impatto:** blocca identità/duplicate semantics
- **Dipendenze:** O-02
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Safety bound normativo W_MAX = 2·log2(20000/20), materializzato come golden binary64 correctly-rounded; limiti semantici per classe inizialmente diagnostici e non usati come gate.
- **Motivazione precompilata:** Chiude overflow e aliasing patologico senza introdurre prematuramente soglie percettive non validate.
- **Firma:**
- **Data:**

### O-04a — Eligible Edge Predicate — CRITICA
- **ID:** O-04a — NUOVA
- **Decisione:** quando esiste arco GT-pred? Formula senza discrezionalità
- **Alternative:** A) larga tutti stessa unità/tipo, B) stretta con soglie hard, C) hard minimale incompatibilità semantica + similarità in K2-K5 — Raccomandazione C
- **Raccomandazione Muse:** C — hard exclusion solo: diversa evaluation_unit_key, diverso problem_type, invalid validity mask, incompatibilità direction semanticamente necessaria, assenza relazione temporale solo se giustificata normativamente. No soglie hard arbitrarie IoU/center distance. Ogni soglia hard richiede motivo + fixture dentro/bordo/fuori + analisi effetto K1
- **Rischio:** B modifica K1 direttamente gamificabile
- **Impatto:** grafo non definito, blocca tutto B e C
- **Dipendenze:** O-01,O-02
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Predicato per classe: stessa evaluation_unit_key e problem_type, record/validity mask validi; temporal IoU ≥ 3/10 esatto; Resonance con max(center)^3 ≤ 2·min(center)^3; altre classi con band IoU ≥ 1/2 esatto; Thinness/DullSound richiedono direction compatibile.
- **Motivazione precompilata:** Rende il grafo deterministico con soglie razionali/algebriche verificabili. Round 2.3 deve motivare scientificamente ogni hard threshold e fornire fixture appena dentro, sul bordo e appena fuori, perché queste soglie modificano K1.
- **Firma:**
- **Data:**

### O-04 — K2 Exact
- **ID:** O-04
- **Decisione:** definizione esatta K2 somma IoU I/U razionale esatto 2I>=U
- **Alternative:** somma razionale esatta con riduzione canonica vs altre
- **Raccomandazione Muse:** somma esatta razionale, maximize
- **Rischio:** bit complexity
- **Impatto:** blocca OPT
- **Dipendenze:** O-04a,O-02
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** K2 massimizza la somma razionale esatta della temporal IoU sugli archi matched; nessun arrotondamento binary64 intermedio. La condizione 2I≥U resta il test esatto della soglia band-IoU, non la definizione di K2.
- **Motivazione precompilata:** Conserva l’ordinamento lessicografico senza dipendenza dall’ordine delle somme floating-point.
- **Firma:**
- **Data:**

### O-05 — K3 Exact Temporal Cost
- **ID:** O-05
- **Decisione:** costo temporale per classe
- **Alternative:** tick interi vs ms
- **Raccomandazione Muse:** basato su tick canonici O-01
- **Rischio:** dipende O-01
- **Impatto:** blocca OPT
- **Dipendenze:** O-01,O-04a
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** K3 minimizza Σ(|start_tick_GT−start_tick_P|+|end_tick_GT−end_tick_P|) usando tick interi esatti.
- **Motivazione precompilata:** È coerente con O-01 e separa il costo di timing dalla conversione diagnostica finale in millisecondi.
- **Firma:**
- **Data:**

### O-06 — K4 Geometric Cost
- **ID:** O-06
- **Decisione:** costo geometrico successivo H/S banda autoritativa vs Resonance center
- **Alternative:** H/S solo banda vs center
- **Raccomandazione Muse:** H/S solo banda, Resonance via O-06a
- **Rischio:** aliasing se center in H/S
- **Impatto:** blocca OPT
- **Dipendenze:** O-02,O-04a
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Per Resonance K4 usa O-06a; per le altre classi minimizza Σ((U−I)/U) sulle bande canoniche.
- **Motivazione precompilata:** Evita center/width raw nelle classi la cui geometria autoritativa è l’intervallo di bande canoniche.
- **Firma:**
- **Data:**

### O-06a — Resonance Center-Cost Numeric Authority + Product — PREFERRED E
- **ID:** O-06a
- **Decisione:** come calcolare |log2(cGT/cP)| esatto cross-lang
- **Alternative:** A) log2 pinnato + golden, B) lookup fixed-point, C) binary64 reference + golden, D) metrica diversa esatta monotona, E) exact product r_e=max/min Prod r_e argmin sum|log2|=argmin Prod — PREFERRED CANDIDATE E
- **Raccomandazione Muse:** E con precondizioni: centri finiti >0, K4 non pesata, cardinalità fissata K1, centri N64 razionali esatti, confronto esatto, nessun peso reale/non intero, bound bit-length, implementazione libera senza gcd obbligatorio ogni passo — Objective components represent exact mathematical values, canonical reduction only at serialization boundaries
- **Rischio:** bit complexity prodotto
- **Impatto:** blocker reale OPT
- **Dipendenze:** O-04a,O-02
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** E — minimizzare il prodotto esatto Π max(center_GT,center_P)/min(center_GT,center_P), equivalente a minimizzare Σ|log2(center_GT/center_P)|, senza libm normativa.
- **Motivazione precompilata:** Valida con centri finiti e positivi, K4 non pesata, cardinalità già fissata da K1, confronto razionale esatto e bound di bit-length dimostrato.
- **Firma:**
- **Data:**

### O-07 — K5_RESERVED = unused in REV8
- **ID:** O-07
- **Decisione:** esiste K5?
- **Alternative:** A) K5 esiste con funzione per-classe esplicita indipendente giustificata non controllabile, B) K5_RESERVED = unused in REV8, K6 segue K4
- **Raccomandazione Muse:** K5 exists only if independently justified candidate-uncontrolled preference. Otherwise K5_RESERVED = unused in REV8. Norm: K5 does not participate in eligibility, optimization, matching equivalence or canonicalization. K6 follows K4.
- **Rischio:** K5 inventata riduce M* nasconde ambiguità
- **Impatto:** altera M*
- **Dipendenze:** O-04..O-06a
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** B — K5_RESERVED = unused in REV8. K5 non partecipa a eligibility, optimization, matching equivalence o canonicalization; K6 segue K4.
- **Motivazione precompilata:** Non introduce un obiettivo artificiale capace di restringere M* e nascondere l’ambiguità delle metriche.
- **Firma:**
- **Data:**

### O-15 — C4 Structural Keys
- **ID:** O-15
- **Decisione:** tassonomia duplicati/contraddizioni per tutte le classi
- **Alternative:** structural target con N64 timing, canonical band, N64 center dove normativo, direction dove normativo, no fuzzy
- **Raccomandazione Muse:** H/S/Resonance/Thinness/DullSound PROPOSED STRUCTURAL RULES, Muddiness/Boominess/BoxyMidrange NOT YET SPECIFIED
- **Rischio:** vecchia regola REJECTED
- **Impatto:** GT validity
- **Dipendenze:** O-01,O-02
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Structural target = evaluation unit, kind, problem type, start/end tick canonici, banda canonica, center N64 per Resonance e direction per Thinness/DullSound. Stessa structural key + payload identico → DUPLICATE_GT; stessa key + payload incompatibile → CONTRADICTORY_GT; entrambi → FAIL GT validity.
- **Motivazione precompilata:** Fissa una tassonomia esatta senza fuzzy deduplication; Round 2.3 deve completare i campi per tutte le otto classi.
- **Firma:**
- **Data:**

---

## Blocco B — Optimizer

### O-08 — OPT(R,F) e A1 — infeasibility non fatal
- **ID:** O-08
- **Decisione:** definizione esatta OPT(K1..K5), comportamento infeasible, total order archi, invariante A1
- **Alternative:** A1 reference V*=OPT(∅,∅) repeated optimization, A2 cost vector optimization
- **Raccomandazione Muse:** A1 reference, A2 optimization con prova equivalenza. Distinzione: OPT_SUBPROBLEM_INFEASIBLE e OPT_VALUE_DIFFERS non-fatal -> forbidden continua, SOLVER_STRUCTURAL_LIMIT_EXCEEDED / RUNTIME_FAILURE / CONSTRAINT_MODEL_INVALID fatal impediscono PASS. Non usare SOLVER_INFEASIBLE_CONSTRAINT_SET generico.
- **Rischio:** senza invariante A1 non prova optimum globale
- **Impatto:** blocca solver
- **Dipendenze:** O-04a,O-04..O-07,O-08a,O-20
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** A1 come algoritmo reference: calcola V*=OPT(∅,∅), visita le edge class in ordine canonico e mantiene un arco solo se il sottoproblema preserva V*. OPT_SUBPROBLEM_INFEASIBLE e OPT_VALUE_DIFFERS_FROM_GLOBAL_OPTIMUM sono non-fatali e vietano l’arco; limiti strutturali, runtime failure o constraint model invalid sono fatali. A2 ammesso solo con prova di equivalenza.
- **Motivazione precompilata:** Separa la canonicalizzazione deterministica dalla ricerca dell’optimum e non trasforma la normale infeasibility di un sottoproblema A1 in un FAIL scientifico.
- **Firma:**
- **Data:**

### O-08a — Objective Vector Representation and Equality — implementation-neutral
- **ID:** O-08a — NUOVA
- **Decisione:** tipo componenti K1-K5, equality exact, rappresentazione somme razionali, riduzione canonica, bit complexity, confronto V_i==V*
- **Alternative:** frazione ridotta vs numeratore/denominatore vs big int non ridotti temporaneamente vs separazione potenze due vs cancellazione incrementale vs cross multiplication
- **Raccomandazione Muse:** Objective components represent exact mathematical values. Equality and ordering are exact. Any internal representation is allowed if observationally equivalent. Canonical reduction is required only at normative serialization boundaries, not necessarily after each arithmetic operation. Bound bit-length provato.
- **Rischio:** denominatori somma IoU crescono
- **Impatto:** blocca confronto optimum
- **Dipendenze:** O-04..O-07
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Le componenti dell’obiettivo rappresentano valori matematici esatti; equality e ordering sono esatti. Qualunque rappresentazione interna osservazionalmente equivalente è ammessa; riduzione canonica solo ai boundary di serializzazione, con bit-length bounded.
- **Motivazione precompilata:** Specifica la semantica senza imporre gcd dopo ogni operazione o una particolare struttura big-integer.
- **Firma:**
- **Data:**

### O-20 — Canonical Edge Key + Order
- **ID:** O-20
- **Decisione:** forma esatta canonical edge key, total order archi per A1, nessuna metrica candidate-controlled in K6
- **Alternative:** varie forme key
- **Raccomandazione Muse:** key senza severity/confidence/ID/hash, solo geometria canonica + occurrence_ordinal per copie identiche non deduplica
- **Rischio:** K6 usa severity -> false PASS
- **Impatto:** canonicalizzazione
- **Dipendenze:** O-04a, O-02, O-01 — O-01 per eventuali componenti temporali canonici della edge key, O-02 per quelli frequenziali
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** K6 ordina edge class strutturali, non ID concreti; esclude severity, confidence, actionable, ID e hash; preserva la molteplicità tramite occurrence_ordinal; il mapping alle istanze concrete è diagnostico.
- **Motivazione precompilata:** Impedisce a dati candidate-controlled di determinare il pairing scientifico. Round 2.3 deve provare unicità del report canonico e replay.
- **Firma:**
- **Data:**

### O-09 — FAIL Blast Radius — cap vs runtime
- **ID:** O-09
- **Decisione:** blast radius e distinzione limiti normativi deterministici vs failure operativi
- **Alternative:** FAIL unità vs gruppo vs report
- **Raccomandazione Muse:** cap deterministici preflight: max GT, max prediction, max archi, max bit-length, max chiamate OPT, budget strutturale calcolabile prima. Failure operativi: OOM, eccezione, indisponibilità, corruzione. Cap rilevato prima calcolo parziale. Reason code: SOLVER_STRUCTURAL_LIMIT_EXCEEDED fatal, SOLVER_RUNTIME_FAILURE fatal, SOLVER_CONSTRAINT_MODEL_INVALID fatal, OPT_SUBPROBLEM_INFEASIBLE non-fatal, OPT_VALUE_DIFFERS non-fatal. Qualsiasi fatal in unità che contribuisce a gate impedisce PASS.
- **Rischio:** memoria come soglia non deterministica se non preflight
- **Impatto:** gate validity
- **Dipendenze:** O-08
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Safety cap normativi per partizione: 128 GT, 128 prediction, 16.384 archi, 16.385 chiamate OPT e 65.536 bit per componente. Preflight prima del calcolo; superamento → SOLVER_STRUCTURAL_LIMIT_EXCEEDED. OOM/runtime/model-invalid → report senza PASS. I cap devono essere confermati da prova di fattibilità/benchmark prima dello SPEC GO.
- **Motivazione precompilata:** Rende il limite riproducibile e distinto dalle failure dipendenti dall’implementazione, senza spacciare i numeri come evidenza scientifica già acquisita.
- **Firma:**
- **Data:**

---

## Blocco C — Significato Statistico

### O-13 — Aggregators + Quantile Convention
- **ID:** O-13
- **Decisione:** per ogni metrica centro/onset/offset: published, gate, diagnostic, intra-group, inter-group macro peso gruppo=1 + p95 method
- **Alternative:** mean/p95/max combinazioni, nearest-rank vs interpolazione, zero/one-based, small-support, tie, N/A threshold, gruppi senza matched
- **Raccomandazione Muse:** definire 5 dimensioni + quantile esatto, rispettare peso gruppo=1
- **Rischio:** p95 non unico cross-impl
- **Impatto:** gate statistico
- **Dipendenze:** O-01,O-10..O-12
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Per-unit mean → mean dentro ciascun group_id → macro mean con peso gruppo=1. Pubblicare p95 Type-7 sui valori di gruppo; micro e max solo diagnostici. Il gate usa esclusivamente la statistica preregistrata; supporto insufficiente → N/A e nessun PASS.
- **Motivazione precompilata:** Preserva il peso uguale dei gruppi e fissa percentile, small-support e blast radius.
- **Firma:**
- **Data:**

### O-10 — Severity MAE Policy
- **ID:** O-10
- **Decisione:** ambiguity policy severity MAE STRONGLY SUPPORTED sensitive fattore 7x
- **Alternative:** envelope Q_max gate vs N/A/FAIL
- **Raccomandazione Muse:** envelope se computabile esattamente additiva probabile altrimenti N/A/FAIL
- **Rischio:** envelope non computabile
- **Impatto:** gate
- **Dipendenze:** O-08
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Severity MAE usa l’upper envelope esatto su M*. Lower bound e valore del matching K6 sono diagnostici. Se l’envelope non è computabile esattamente entro i cap O-09, la metrica è N/A e non può contribuire a un PASS.
- **Motivazione precompilata:** Evita che K6 scelga arbitrariamente il punteggio più favorevole.
- **Firma:**
- **Data:**

### O-11 — Onset/Offset Policy
- **ID:** O-11
- **Decisione:** onset/offset STRONGLY SUPPORTED sensitive con d=0.25
- **Alternative:** envelope vs N/A/FAIL
- **Raccomandazione Muse:** envelope probabile additiva
- **Rischio:** dipende O-01
- **Impatto:** gate
- **Dipendenze:** O-01,O-08
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Onset e offset hanno envelope esatti distinti su M* e il gate usa il worst case; la conversione tick→ms avviene solo dopo l’aggregazione esatta. Impossibilità di calcolo esatto entro O-09 → N/A, nessun PASS.
- **Motivazione precompilata:** Il controesempio d=0.25 dimostra che una somma temporale K3 uguale non rende invarianti onset e offset separati.
- **Firma:**
- **Data:**

### O-12 — Spearman Policy
- **ID:** O-12
- **Decisione:** Spearman STRONGLY SUPPORTED sensitive rho +1/-1 delta 2.0 fixture incompleta
- **Alternative:** probabilmente N/A/FAIL perché envelope rho su M* non dimostrato computabile
- **Raccomandazione Muse:** pairing ambiguity -> N/A, se gate obbligatorio allora FAIL finché non esiste algoritmo esatto envelope
- **Rischio:** envelope rho complesso
- **Impatto:** gate
- **Dipendenze:** O-08
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Spearman è pubblicabile soltanto con pairing univoco o invarianza dimostrata su M*. Altrimenti → N/A/PAIRING_AMBIGUOUS; se è gate obbligatorio, N/A impedisce PASS.
- **Motivazione precompilata:** Non assume computabile un envelope esatto di rho che non è stato dimostrato.
- **Firma:**
- **Data:**

### O-14b — PR Metric Integration / Average Precision Convention
- **ID:** O-14b — NUOVA
- **Decisione:** dopo distinct-conf pin, come integrare curva
- **Alternative:** gradini vs trapezoidale, ordine thresholds, punto iniziale/finale, recall duplicate, caso senza GT positivi, senza Pred, macro per gruppo, calibration authority
- **Raccomandazione Muse:** definire gradini vs trapezoidale + casi limite
- **Rischio:** area diversa con interpolazione diversa
- **Impatto:** metrica precision-recall; la scelta precompilata la identifica esplicitamente come Average Precision
- **Dipendenze:** O-14
- **Scelta precompilata Codex/Muse — DA FIRMARE O RESPINGERE:** Adottare Average Precision (AP) a gradini sui valori distinti di confidence: tutti i tie entrano insieme; nessuna interpolazione trapezoidale; GT positivi senza prediction → 0; nessun GT positivo → N/A e contribuisce alla clean safety. Calcolare AP per `group_id` con annotazione completa e pubblicare la macro-media a peso gruppo uguale. Ogni riferimento legacy “PR-AUC” deve dichiarare esplicitamente la convenzione AP e pubblicare `metric_name = average_precision`.
- **Motivazione precompilata:** Elimina cut-point posizionali e l’ambiguità terminologica tra AP e altre aree sotto la curva precision-recall, preservando il peso unitario dei gruppi già richiesto dal candidate.
- **Firma:**
- **Data:**

---

## Firma e governance

- Ogni scelta resta una raccomandazione finché la relativa firma non è presente.
- Una modifica riapre soltanto la decisione e le sue dipendenze dichiarate.
- Il ballot firmato non costituisce REV8 SPEC GO e non autorizza implementazione, training o attivazione.
