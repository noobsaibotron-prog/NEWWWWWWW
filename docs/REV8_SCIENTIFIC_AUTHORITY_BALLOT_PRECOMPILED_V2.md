# REV8 — SCIENTIFIC AUTHORITY BALLOT V2 — PRECOMPILATO — NON FIRMATO

**Stato:** REV8 SPEC GO = NO — G1c close = NO — Training V3 = NO
**Base scientifica:** e2bee113 immutabile document-only
**Freeze Round 2.2 di riferimento:** 95f1f798a2a97aec960eb86faf5968e73d076564
**Relazione con V1:** questa V2 sostituisce esclusivamente le raccomandazioni precompilate del ballot V1; non altera il Decision Register o il template congelati
**Scopo:** contiene soltanto decisioni UNDECIDED che richiedono scelta umana/scientifica. Le scelte sono precompilate come raccomandazioni Codex/Muse, ma restano prive di autorità finché Marco non firma o respinge ogni riga. Non chiedere nuova firma per DECIDED_PENDING_* salvo dipendenza reale
**Priorità:** Blocco A -> B -> C — non decidere metrica prima di grafo e costi
**Revisione V2:** conversione temporale senza double rounding; A1 exhaustive come autorità/oracle e A2 come runtime equivalente; cap O-09 separati dalla prova di fattibilità; p95 diagnostico finché non powered; structural key Resonance su center-band canonica; K6 scientifico separato dalla serializzazione diagnostica; Average Precision definita esplicitamente

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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** B2 — tutti i confini temporali normativi dello schema v2 sono materializzati come interi `start_tick`, `end_tick`, `segment_start_tick` e `segment_end_tick` sulla griglia 48000 Hz. Sono l’unica autorità per identità, ammissione, matching e metriche; eventuali secondi sono diagnostici derivati come correctly-rounded binary64 di `tick/48000` e non vengono mai riconvertiti in tick. Intervalli `[start_tick,end_tick)`; `start_tick >= end_tick` → FAIL. La conversione da un trusted source sample index avviene una sola volta sul rapporto razionale, con round-to-nearest ties-to-even, prima della serializzazione; gli eventuali `source_sample_index` e `source_sample_rate` sono provenance hash-bound e non una seconda autorità. Per un feature frame `j`, `τ_j=RNE(frame_end_sample_j−48000·delay_num/delay_den)` in tick, con `H=1024`; la cella temporale del frame è `[τ_j−H,τ_j)`. Per una componente con primo frame `j_lo` e ultimo frame `j_hi`, `start_tick=max(segment_start_tick,τ_j_lo−H)` ed `end_tick=min(segment_end_tick,τ_j_hi)`. Hop, group delay e clipping sono calcolati razionalmente prima dell’unico rounding indicato.
- **Motivazione V2:** Lo schema corrente conserva soltanto secondi e rende non osservabile l’origine del tempo. A 96 kHz, `source_index=5` produce 2 tick col rapporto razionale ma 3 tick dopo serializzazione N64: materializzare i tick elimina concretamente questo doppio percorso.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** `width_octaves` deve essere N64 finito in `[0,W_MAX]`, con `W_MAX = 2·log2(20000/20)` materializzato come golden binary64 correctly-rounded; oltre il bound → FAIL. Il bound non elimina l’aliasing entro intervallo. L0 conserva center/width raw per provenance; L2 usa `[i_lo,i_hi]` canonico; L3 applica O-15; L4 usa esclusivamente la geometria canonica per le classi band-based. Per ogni classe band-based non-Resonance e per record `x,y` identici in tutti i campi normativi non frequenziali ma differenti soltanto in center/width raw, `B(x)=B(y)` implica eligibility e costo geometrico identici contro ogni `z`. I limiti semantici restano diagnostici fino a evidenza preregistrata.
- **Motivazione V2:** Separa il safety bound dalla reale semantica dell’aliasing, che non viene resa iniettiva da W_MAX.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Il grafo viene costruito solo dopo admission/validity fail-closed. Sono ammesse esclusivamente le famiglie `semantic_region ↔ semantic_bundle` per gli otto problem type e `dynamic_event ↔ prediction_event` per Resonance, Harshness e Sibilance; nessun arco cross-family. Dentro la stessa `(evaluation_unit_key, record_family, problem_type)`, temporal IoU ≥ 3/10. Resonance richiede `max(center)^3 ≤ 2·min(center)^3`; Muddiness, Boominess, BoxyMidrange, Harshness e Sibilance richiedono band IoU ≥ 1/2; Thinness e DullSound richiedono band IoU ≥ 1/2 e direction uguale. Confronti esatti e fixture appena dentro, sul bordo e appena fuori. Le soglie sono adottate come definizione operativa preregistrata di “stesso target”, non come confini percettivi empiricamente dimostrati, e non possono essere ritoccate dopo l’osservazione dei risultati. `TP=K1`, `FP=|P|−K1`, `FN=|GT|−K1`; rimuovere un arco pivotal può ridurre K1/TP di uno e aumentare FP e FN di uno.
- **Motivazione V2:** Rende esplicita, firmabile e non adattabile post hoc la convenzione operativa del candidate, compreso il suo effetto discontinuo su K1/TP/FP/FN, e impedisce cross-match fra regioni semantiche ed eventi dinamici.
- **Firma:**
- **Data:**

### O-04 — K2 Exact
- **ID:** O-04
- **Decisione:** K2 è la somma razionale esatta della temporal IoU
- **Alternative:** somma razionale esatta con riduzione canonica vs altre
- **Raccomandazione Muse:** somma esatta razionale, maximize
- **Rischio:** bit complexity
- **Impatto:** blocca OPT
- **Dipendenze:** O-01,O-04a,O-02
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per intervalli temporali semiaperti: `I_t=max(0,min(end_GT,end_P)-max(start_GT,start_P))`, `U_t=(end_GT-start_GT)+(end_P-start_P)-I_t`, `IoU_t=I_t/U_t`. K2 massimizza la somma razionale esatta di `IoU_t` sugli archi matched. Nessun arrotondamento binary64 intermedio.
- **Motivazione V2:** Preserva l’ordine dell’obiettivo indipendentemente da permutazione e backend.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** K3 minimizza Σ(|start_tick_GT−start_tick_P|+|end_tick_GT−end_tick_P|) in tick interi esatti.
- **Motivazione V2:** È coerente con O-01 e rinvia la conversione in millisecondi alla sola pubblicazione.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per bande inclusive intere: `I_b=max(0,min(hi_GT,hi_P)-max(lo_GT,lo_P)+1)`, `L_GT=hi_GT-lo_GT+1`, `L_P=hi_P-lo_P+1`, `U_b=L_GT+L_P-I_b`, `IoU_b=I_b/U_b`; eligibility band-based usa `2·I_b ≥ U_b`. Per Resonance K4 usa O-06a. Per tutte le altre classi K4 minimizza `Σ((U_b-I_b)/U_b)`.
- **Motivazione V2:** Usa la geometria effettivamente osservabile dal frontend e non reintroduce center/width raw per classi band-based.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** E+ — per Resonance K4 minimizza il prodotto razionale esatto Π max(center_GT,center_P)/min(center_GT,center_P), matematicamente equivalente alla somma degli errori assoluti in ottave. K4 è un obiettivo moltiplicativo in un gruppo ordinato di razionali positivi, non una falsa somma additiva. A2 deve dimostrare confronto esatto, equivalenza con l’autorità A1 e bit complexity bounded; nessuna libm normativa.
- **Motivazione V2:** Conserva la semantica in ottave e la riproducibilità cross-language, ma rende esplicito il requisito algoritmico spesso nascosto dalla trasformazione logaritmica.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** B — K5_RESERVED = unused in REV8. K5 non partecipa a eligibility, optimization, matching equivalence o canonicalization; K6 segue K4.
- **Motivazione V2:** Evita un obiettivo artificiale che restringerebbe M* e potrebbe nascondere l’ambiguità delle metriche.
- **Firma:**
- **Data:**

### O-15 — C4 Structural Keys
- **ID:** O-15
- **Decisione:** tassonomia duplicati/contraddizioni per tutte le classi
- **Alternative:** structural target con N64 timing, canonical band, N64 center dove normativo, direction dove normativo, no fuzzy
- **Raccomandazione Muse:** H/S/Resonance/Thinness/DullSound PROPOSED STRUCTURAL RULES, Muddiness/Boominess/BoxyMidrange NOT YET SPECIFIED
- **Rischio:** vecchia regola REJECTED
- **Impatto:** GT validity
- **Dipendenze:** O-01,O-02,O-03,O-04a
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Scope GT: una singola `annotation_top_level_key=(evaluation_unit_key,annotator_id,pass_id)`; record di annotatori/pass diversi non sono duplicati fra loro. Matrice kind/class: `semantic_region` ammette gli otto tipi, `dynamic_event` soltanto Resonance/Harshness/Sibilance. Structural key: top-level key, kind, problem_type, start_tick, end_tick e `[i_lo,i_hi]`; Resonance aggiunge `center_band_index` canonico, non raw center N64; Thinness/DullSound aggiungono direction. Il payload supervisionale è esattamente `[N64(severity),N64(confidence),actionable]`. Raw center/width/band/timing già proiettati, secondi diagnostici, `problem_type_id`, ID, schema e provenance non vi partecipano. Stessa structural key + stessi byte canonici del payload → DUPLICATE_GT; stessa key + payload diverso → CONTRADICTORY_GT; entrambi → FAIL. Key diverse restano distinte, senza fuzzy deduplication.
- **Motivazione V2:** Il raw center N64 renderebbe artificialmente distinti target che il frontend rappresenta nella stessa cella; la banda centrale canonica lega la validità GT alla risoluzione realmente osservabile.
- **Firma:**
- **Data:**

---

## Blocco B — Optimizer

### O-08 — Exact Optimum, A1 Exhaustive Oracle e A2 Runtime
- **ID:** O-08
- **Decisione:** definizione esatta di `V=(K1,K2,K3,K4)`, insieme degli optimum, canonicalizzazione S/D post-optimum e separazione fra oracle A1 e runtime A2; K5_RESERVED non è una componente
- **Alternative:** A1 exhaustive reference sui grafi piccoli; A2 exact runtime con prova di equivalenza
- **Raccomandazione Muse:** definire direttamente `M*` e il rappresentante canonico tramite il comparatore gerarchico O-20. A1 enumera esaustivamente i matching sui grafi piccoli ed è l’oracle; A2 può usare qualunque algoritmo esatto soltanto dopo equivalenza contro A1.
- **Rischio:** confondere l’ottimizzazione scientifica con la canonicalizzazione può produrre un optimum diverso o un tie-break candidate-controlled
- **Impatto:** blocca solver
- **Dipendenze:** O-04a,O-04..O-07,O-08a,O-20
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per ogni sottografo `(evaluation_unit_key,record_family,problem_type)`, `V(M)=(K1,K2,K3,K4)` con direzioni `(max,max,min,min)`; K5_RESERVED non è una componente. Sull’empty matching: K1=K2=K3=0, K4=1 per il prodotto Resonance e K4=0 per i costi additivi. `V*` è il miglior vettore lessicografico fra tutti i matching one-to-one eleggibili e `M*={M:V(M)=V*}`. O-20 definisce `M_can=unique argmin_{M∈M*}(S(M),D(M))` mediante confronto gerarchico delle due sequenze. A1 enumera esaustivamente tutti i matching sui grafi piccoli, calcola `V*`, `M*` e `M_can`, ed è l’oracle normativo. A2 può usare un algoritmo diverso sul runtime completo, ma deve restituire gli stessi `V*` e `M_can` di A1 su tutto il dominio oracle e superare fixture/adversarial property test. Structural limit, runtime failure e constraint model invalid sono fatali; non esiste un generico fallback approssimato.
- **Motivazione V2:** Separa l’autorità matematica dall’algoritmo production ed elimina il vecchio require/forbid, che non realizzava correttamente la canonicalizzazione gerarchica S/D.
- **Firma:**
- **Data:**

### O-08a — Objective Vector Representation and Equality — implementation-neutral
- **ID:** O-08a — NUOVA
- **Decisione:** tipo delle componenti K1-K4, equality exact, rappresentazione di somme/prodotti razionali, riduzione canonica, bit complexity e confronto `V_i==V*`; K5_RESERVED è escluso
- **Alternative:** frazione ridotta vs numeratore/denominatore vs big int non ridotti temporaneamente vs separazione potenze due vs cancellazione incrementale vs cross multiplication
- **Raccomandazione Muse:** Objective components represent exact mathematical values. Equality and ordering are exact. Any internal representation is allowed if observationally equivalent. Canonical reduction is required only at normative serialization boundaries, not necessarily after each arithmetic operation. Bound bit-length provato.
- **Rischio:** denominatori somma IoU crescono
- **Impatto:** blocca confronto optimum
- **Dipendenze:** O-04..O-07
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Le componenti dell’obiettivo rappresentano valori matematici esatti; equality e ordering sono esatti. Qualunque rappresentazione interna osservazionalmente equivalente è ammessa; la riduzione canonica è obbligatoria solo ai boundary di serializzazione. Bit-length e operazioni devono rispettare cap strutturali dimostrati.
- **Motivazione V2:** Definisce la semantica senza imporre gcd dopo ogni operazione o una struttura big-integer specifica.
- **Firma:**
- **Data:**

### O-20 — Canonical Edge Key + Order
- **ID:** O-20
- **Decisione:** forma esatta canonical edge key, total order archi per A1, nessuna metrica candidate-controlled in K6
- **Alternative:** varie forme key
- **Raccomandazione Muse:** key senza severity/confidence/ID/hash, solo geometria canonica + occurrence_ordinal per copie identiche non deduplica
- **Rischio:** K6 usa severity -> false PASS
- **Impatto:** canonicalizzazione
- **Dipendenze:** O-01,O-02,O-03,O-04a,O-04,O-05,O-06,O-06a,O-07
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Entrambe le scientific endpoint key usano esclusivamente `evaluation_unit_key`; `prediction_top_level_key` è definita uguale a `evaluation_unit_key`, mentre `annotation_top_level_key` resta soltanto diagnostica. La geometria scientifica è class-dependent e ordinata come `[i_lo_or_null,i_hi_or_null,resonance_center_N64_or_null,direction_or_null]`: Resonance usa `[null,null,center_N64,null]`; Thinness/DullSound usano `[i_lo,i_hi,null,direction]`; ogni altra classe usa `[i_lo,i_hi,null,null]`. Quindi `scientific_gt_key=["gt",evaluation_unit_key,record_family,problem_type,start_tick,end_tick,class_geometry]` e `scientific_prediction_key=["prediction",evaluation_unit_key,record_family,problem_type,start_tick,end_tick,class_geometry]`. Escludono severity, confidence, actionable, ID, hash, annotator/pass, secondi e geometria raw non normativa. `scientific_edge_key(e)=[scientific_gt_key(g),scientific_prediction_key(p)]`. `diagnostic_gt_key=["gt-diagnostic",annotation_top_level_key,scientific_gt_key,canonical_item_payload_GT]`; `diagnostic_prediction_key=["prediction-diagnostic",scientific_prediction_key,canonical_item_payload_P,occurrence_ordinal]`, dove `canonical_item_payload_*` è l’exact-key item dello schema v2 dopo N64 e rimozione esclusiva di schema, ID derivato e occurrence ordinal; `diagnostic_edge_key(e)=[diagnostic_gt_key(g),diagnostic_prediction_key(p)]`. Gli array vengono confrontati mediante canonical bytes in unsigned lexicographic order. Per ogni gruppo di prediction con payload diagnostico identico e molteplicità N, gli ordinali ammessi sono esattamente il multiset `{0,…,N−1}`, verificato dopo grouping e indipendentemente dall’ordine d’ingresso; il GT non porta ordinale. Per `M∈M*`, `S(M)=sort(scientific_edge_key(e):e∈M)` e `D(M)=sort(diagnostic_edge_key(e):e∈M)`; si minimizza gerarchicamente prima l’intera S e soltanto fra S identiche l’intera D. Non usare una singola tupla per-edge `(scientific,diagnostic)`. `M_can=unique argmin_{M∈M*}(S(M),D(M))`. Nessuna metrica ambiguity-sensitive o gate può usare `M_can`: deve usare invarianza o envelope sull’intero `M*`.
- **Motivazione V2:** Il confronto gerarchico fra sequenze produce un total order riproducibile senza consentire ai campi diagnostici della prima edge di prevalere sulla geometria scientifica delle edge successive.
- **Firma:**
- **Data:**

### O-09 — FAIL Blast Radius — cap vs runtime
- **ID:** O-09
- **Decisione:** blast radius e distinzione limiti normativi deterministici vs failure operativi
- **Alternative:** FAIL unità vs gruppo vs report
- **Raccomandazione Muse:** cap deterministici preflight: max GT, max prediction, max archi, max bit-length e budget strutturale calcolabile prima. Failure operativi: OOM, eccezione, indisponibilità, corruzione. Cap rilevato prima del calcolo parziale. `SOLVER_STRUCTURAL_LIMIT_EXCEEDED`, `SOLVER_RUNTIME_FAILURE` e `SOLVER_CONSTRAINT_MODEL_INVALID` sono fatali. Qualsiasi fatal in un sottografo che contribuisce a gate impedisce PASS.
- **Rischio:** memoria come soglia non deterministica se non preflight
- **Impatto:** gate validity
- **Dipendenze:** O-08
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Firmare ora la policy fail-closed e lo scope per sottografo `(evaluation_unit_key,record_family,problem_type)`, non ancora i numeri. Ceiling candidati: GT≤128, prediction≤128, archi≤16384 e bit-length≤65536 della rappresentazione matematica ridotta; working-space e temporanei sono implementation-specific. Il preflight usa un bound conservativo derivato dagli input. A1 exhaustive è limitato al dominio oracle fissato dalle fixture e non impone un numero di chiamate OPT al runtime. I valori restano PROVISIONAL/DECIDED_PENDING_BENCHMARK e non producono PASS/FAIL finché un ballot addendum non li attiva dopo benchmark A2 sul platform lock. Solo un cap attivato superato → SOLVER_STRUCTURAL_LIMIT_EXCEEDED; ogni failure fatale → nessun PASS. Finché i cap non sono attivati, REV8 SPEC GO resta NO.
- **Motivazione V2:** Evita di firmare come fattibili numeri non ancora misurati, mantenendo però una policy di ammissione e blast radius completamente fail-closed.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per ogni metrica, `G_eligible` è congelato da manifest, complete_types e GT prima di leggere le prediction; pubblicare `G_eligible`, `G_defined`, `G_NA=G_eligible\\G_defined` e reason code. `G_defined` conta i group_id con almeno una unità in cui la metrica è definita; i floor di gate si applicano a G_defined oltre ai floor GT-positivi preregistrati. Floor mancante per una metrica obbligatoria → N/A e nessun PASS; le unità N/A non valgono zero. Gerarchia: mean delle osservazioni definite per unità → mean delle unità definite nel gruppo → macro-mean dei gruppi definiti, peso gruppo=1. La macro-mean è la statistica primaria e di gate, subordinata ai gate recall/cobertura. P95 diagnostico Type-7 con q=0.95: G=0 → N/A, G=1 → unico valore, G≥2 usa `h=(G−1)q` e interpolazione lineare. Un futuro gate p95 richiede floor e power analysis congelati.
- **Motivazione V2:** Preserva l’indipendenza dei gruppi senza lasciare che un percentile interpolato e poco supportato decida prematuramente il gate.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per ogni partizione con K1>0, `Q_severity(M)=Σ|severity_GT−severity_P|/K1` sui razionali esatti N64 e `Q_severity^+ := max_{M∈M*} Q_severity(M)`. K1=0 → N/A. Lower envelope e valore di `M_can` sono diagnostici. Envelope non computabile esattamente entro O-09 → N/A/PAIRING_ENVELOPE_UNAVAILABLE; se obbligatoria, nessun PASS. Aggregazione successiva secondo O-13.
- **Motivazione V2:** Impedisce a K6 di scegliere il pairing scientificamente più favorevole.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per ogni partizione con K1>0, `Q_on(M)=Σ|start_tick_GT-start_tick_P|/K1` e `Q_off(M)=Σ|end_tick_GT-end_tick_P|/K1`; usare gli upper envelope distinti `max_{M∈M*}`. K1=0 → N/A. Il gate usa il worst case; tick→ms avviene soltanto dopo il calcolo esatto. Envelope non disponibile entro O-09 → N/A e, se obbligatorio, nessun PASS. Aggregazione secondo O-13.
- **Motivazione V2:** Il controesempio d=0.25 mostra che K3 uguale non rende invarianti onset e offset separati.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Calcolare Spearman per `(group_id,record_family,problem_type)` come Pearson sui midrank, con rank medio per tie, pooling delle coppie matched nelle unità complete del gruppo. Per il gruppo `g`, `𝓜_g^* := ×_{u∈U_g} M_u^*`, prodotto cartesiano degli insiemi optimum dei sottografi/unità che contribuiscono; `rho(m)` usa le coppie pooled del matching composto `m`. Rho è pubblicabile soltanto se, per ogni `m∈𝓜_g^*`, il numero totale di coppie matched è `n≥10`, entrambi i vettori hanno varianza non zero e l’insieme dei valori matematici esatti `{rho(m):m∈𝓜_g^*}` è singleton. Se almeno un matching ha n<10 → N/A/INSUFFICIENT_MATCHED_SUPPORT; se almeno un matching produce varianza zero → N/A/SPEARMAN_UNDEFINED; altrimenti, se i valori non sono singleton → N/A/PAIRING_AMBIGUOUS. Macro-aggregazione e floor sui group_id definiti seguono O-13. Se Spearman è gate obbligatorio, N/A impedisce PASS; `M_can` è solo diagnostico.
- **Motivazione V2:** Non assume l’esistenza di un algoritmo esatto per l’envelope di rho.
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
- **Scelta precompilata V2 — DA FIRMARE O RESPINGERE:** Per ogni `(group_id,record_family="dynamic_event",problem_type)` usare esclusivamente `dynamic_event ↔ prediction_event` e prediction pre-threshold delle unità in cui la classe appartiene a `complete_types`, con confidence calibrata e ricalcolata dalla policy congelata. Siano `t_1>…>t_K` i valori binary64 distinti; a ogni `t_k` includere atomicamente tutte le prediction con `confidence≥t_k` e ricalcolare il matching normativo. `P_k=TP_k/(TP_k+FP_k)`, `R_k=TP_k/N_GT`, `R_0=0`; Average Precision `AP=Σ_k(R_k−R_{k-1})·P_k`, usando la precisione osservata, senza trapezi, precision envelope o endpoint artificiale. `N_GT>0,K=0` → AP=0; `N_GT=0` → N/A e clean safety. Macro-AP sui soli gruppi GT-positivi eleggibili, peso gruppo=1, con supporto/floor preregistrato. Nome normativo `metric_name=average_precision`; ogni alias legacy PR-AUC dichiara `convention=average_precision`.
- **Motivazione V2:** Definisce completamente endpoint, tie, integrazione e aggregazione, evitando di confondere AP con altre aree sotto la curva precision-recall.
- **Firma:**
- **Data:**

---

## Firma, counter-check e governance

- Stato counter-check: primo giro 3/3 `BLOCK`; secondo giro 3/3 `BLOCK` con residui circoscritti; dopo integrazione e conferma finale ultra-mirata, 3/3 `CLEAN` sul testo corrente.
- Ogni scelta resta una raccomandazione finché la relativa firma non è presente.
- Una modifica riapre la decisione e tutti i dipendenti downstream transitivi dichiarati; non riapre automaticamente le dipendenze upstream già soddisfatte.
- Modifiche cross-cutting a schema, identità, temporal authority, eligibility, objective vector o K6 richiedono un nuovo counter-check consolidato anche se la dipendenza non era stata elencata correttamente.
- Ogni riga firmata deve registrare esplicitamente `APPROVO` oppure `RESPINGO`; in caso di rifiuto è obbligatorio il wording sostitutivo.
- Il ballot firmato non costituisce REV8 SPEC GO e non autorizza implementazione, training o attivazione.
- `ml_v3/benchmark/event_matching.py` e `ml_v3/contracts/exact_arith_v2.py` correnti non costituiscono prova A2/REV8: usano ancora semantiche float/ID o conversioni che perdono interi oltre 2^53. Restano candidate isolate e non possono sostenere PASS.
