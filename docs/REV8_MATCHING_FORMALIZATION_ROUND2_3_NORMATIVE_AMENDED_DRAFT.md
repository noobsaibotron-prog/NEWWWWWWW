# REV8 — MATCHING FORMALIZATION ROUND 2.3 — NORMATIVE AMENDED DRAFT

**Stato del documento:** micro-amend documentale non committato, successivo al
red-team ristretto; decisioni `R23A_01`–`R23A_05` in attesa di firma e recheck.
**Predecessore firmato immutabile:**
`REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md`, SHA-256
`8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041`.
**Autorità scientifica precedente:** ballot e addendum R23 firmati da Marco il
2026-07-29. Le cinque decisioni R23A non sono ancora approvate.
**Base scientifica immutabile:** `e2bee113c02356c1dff34201f5d4ef002598a23b`.
**Freeze Round 2.2:** `95f1f798a2a97aec960eb86faf5968e73d076564`.
**Ballot firmato:** `52757b352b92a7c5c8b13699a8ba5a8c4286c8fd`.
**Precedenza proposta:** dopo firma e recheck, questo documento sostituisce
esclusivamente il predecessor firmato per i cinque finding `RT-001`–`RT-005`.
Continua a trasferire le 18 scelte del ballot, le sei decisioni
`DECIDED_PENDING_*` del Decision Register e le decisioni R23_01–R23_05. Non
modifica il candidate REV8, il codice, gli schema vivi, il runtime o il
training.

```text
REV8 SPEC GO = NO
G1c close    = NO
G1 PASS      = NO
Training V3  = NO
Activation   = NO
```

Le parole **DEVE**, **NON DEVE**, **FAIL** e **N/A** sono normative.
I blocchi marcati `PENDING_*` sono obblighi già decisi ma non ancora
materializzati. Le decisioni `R23_01`–`R23_05` sono state approvate
esplicitamente dall’autorità scientifica e hanno pari forza normativa alle
decisioni trasferite dal ballot.

### Firma addendum Round 2.3

```text
Decisioni approvate:
R23_01, R23_02, R23_03, R23_04, R23_05

Decisione autorità: APPROVO
Firma/nome: Marco
Data: 2026-07-29
```

La firma precedente chiude le cinque discrezionalità individuate durante la
redazione;
non costituisce `REV8 SPEC GO` e non autorizza implementazione o attivazione.

### Firma micro-amend post red-team

```text
Decisioni proposte:
R23A_01 — compound transcendental rounding
R23A_02 — scientific K6 signature vs diagnostic replay
R23A_03 — byte-authoritative mean hierarchy
R23A_04 — unreachable Resonance equality fixture
R23A_05 — exact scalar bit-length semantics

Decisione autorità: PENDING
Firma/nome:
Data:
```

Finché questo blocco non è firmato, il documento è una proposta di micro-amend
e non una nuova autorità normativa. La firma non costituisce `REV8 SPEC GO`;
autorizza soltanto il recheck ristretto di `RT-001`–`RT-005`.

---

## 1. Authority, revision e assenza di fallback

L’autorità della revisione attiva DEVE essere il dispatcher insieme
all’activation manifest. Costanti storiche quali `CONTRACT_REVISION` conservano
soltanto provenance e NON DEVONO selezionare l’evaluator attivo.

L’activation manifest finale DEVE contenere almeno:

```text
active_evaluator_revision
contract_document_sha256
schema_bundle_sha256
boundary_artifact_sha256
numeric_artifact_sha256
dispatcher_sha256
platform_lock_id
```

I digest e l’identificativo definitivo della revisione sono
`PENDING_TRANSFER_O19`. Un manifest assente, sconosciuto, incoerente o misto
REV7/REV8 produce FAIL. Non esiste fallback implicito a REV7.

I kind literal normativi sono esclusivamente:

```text
semantic_region
dynamic_event
semantic_bundle
prediction_event
```

Un kind differente produce FAIL. La lista e i test di hashing sono
`PENDING_TRANSFER_TEST_O16`.

`problem_type_id` resta nel payload semantico e nella provenance per verificare
la corrispondenza con `problem_type`, ma è escluso dalle chiavi scientifiche
perché ogni grafo viene validato e partizionato preventivamente per
`problem_type`. Una discordanza fra stringa e ID produce FAIL. Questo wording
trasferisce O-17.

---

## 2. Autorità numerica canonica

### 2.1 N64

`N64` accetta soltanto valori ammessi da campi JSON Schema `number`, mai
booleani. La conversione usa IEEE-754 binary64, round-to-nearest ties-to-even.
Overflow, NaN e infinito producono FAIL. Lo zero viene normalizzato a `+0`.

La serializzazione canonica è:

```text
f64:<sedici-cifre-esadecimali-minuscole-del-bit-pattern-big-endian>
```

Esempi normativi:

```text
N64(0.0) = f64:0000000000000000
N64(0.2) = f64:3fc999999999999a
N64(1.0) = f64:3ff0000000000000
N64(1.2) = f64:3ff3333333333333
N64(0.25)= f64:3fd0000000000000
N64(1.25)= f64:3ff4000000000000
```

Il valore binary64 `0.2` e il valore rappresentato dai bit della differenza
binary64 `1.2-1.0`, `0x3fc9999999999998`, distano due ULP alla scala di
`0.2`. Il vecchio controesempio temporale con `d=0.2` è rigettato. Il
controesempio di ambiguità onset/offset usa `d=0.25`. La sua fixture v2 usa
tick validi e non negativi:

```text
GT  = [24000,72000)
P_A = [12000,72000)
P_B = [24000,84000)

IoU_t(P_A)=IoU_t(P_B)=4/5
K3(P_A)=K3(P_B)=12000 tick
onset error  = 250 ms / 0 ms
offset error = 0 ms / 250 ms
```

La conversione bit-to-rational DEVE restituire esattamente il razionale
rappresentato dai bit. Gli interi JSON e i tick Python nativi NON DEVONO
passare attraverso `float`; `2^53+1` e `2^60+1` devono rimanere invariati.

### 2.2 Aritmetica dell’optimizer e delle metriche pubblicate

Eligibility, obiettivi, confronto fra optimum ed envelope usano valori
matematici esatti. Equality e ordering sono esatti. Qualunque
rappresentazione interna osservazionalmente equivalente è ammessa; la
riduzione canonica è obbligatoria soltanto ai boundary di serializzazione.

`sum_pairwise64` appartiene esclusivamente alle metriche pubblicate binary64 e
NON all’optimizer. I golden e le mutation sono
`PENDING_ARTIFACT_TEST_O18`. La funzione trasferita dal candidate è:

```text
sum_pairwise64([])       -> il chiamante produce N/A oppure FAIL
sum_pairwise64([x])      -> binary64(x)
sum_pairwise64(values):
  mid = floor(len(values)/2)
  return binary64(
    sum_pairwise64(values[0:mid])
    + sum_pairwise64(values[mid:len(values)])
  )
```

Ogni addizione arrotonda a binary64. Gli input sono ordinati per chiave
canonica prima della riduzione. `numpy.sum`, `math.fsum`, Kahan, BLAS
parallelo, riduzioni native dipendenti dal container e `fast-math` sono
vietati per gli artefatti di gate.

### 2.3 Boundary fra valori esatti e pubblicazione binary64

`DECISIONE_R23_05 — APPROVATA E FIRMATA`

Per evitare che ogni metrica inventi una propria regola di rounding, valgono
le regole seguenti:

1. eligibility, `V*`, envelope e statistiche per-partizione firmate sono
   calcolati nel rispettivo dominio matematico esatto;
2. un razionale esatto viene convertito una sola volta al binary64
   correctly-rounded;
3. un valore algebrico o trascendentale ammesso viene convertito tramite
   interval refinement finché l’intervallo arrotonda a un unico binary64;
   per un’espressione composta l’interval refinement copre l’intera
   espressione matematica, senza binary64 intermedi;
4. le riduzioni annidate fra valori già pubblicabili usano
   `sum_pairwise64` nell’ordine canonico;
5. Average Precision per gruppo è un razionale esatto derivato dai conteggi,
   poi arrotondato una volta; la macro-AP usa `sum_pairwise64`;
6. tick→ms usa il razionale esatto `tick*1000/48000` e un solo rounding;
7. il Type-7 diagnostico interpreta i binary64 ordinati come razionali
   esatti, esegue l’interpolazione razionale e arrotonda una volta.

Golden cross-language devono coprire metà ULP, tie-to-even, small support,
segno zero e valori prossimi ai limiti ammessi.

`R23A_01 — PROPOSTA, FIRMA RICHIESTA`

In particolare, `log2(R)/K1` è una singola espressione trascendentale composta:
arrotondare prima `log2(R)` e dividere successivamente è vietato. La divisione
di `mean64` definita in §12 è invece il boundary esplicito di una riduzione
binary64 già materializzata e non autorizza divisioni per-elemento.

---

## 3. Geometria temporale autoritativa

Tutti i confini temporali normativi dello schema v2 sono interi:

```text
start_tick
end_tick
segment_start_tick
segment_end_tick
```

La griglia autoritativa è 48000 tick/s. Questi valori sono l’unica autorità per
identità, admission, matching e metriche. Eventuali secondi sono diagnostici,
derivati una sola volta come correctly-rounded binary64 di `tick/48000`, e
NON vengono mai riconvertiti in tick.

Gli intervalli sono semiaperti `[start_tick,end_tick)`.
`start_tick >= end_tick` produce FAIL. Ogni intervallo deve essere contenuto
nel segmento autoritativo.

La conversione da un trusted source sample index avviene una sola volta sul
rapporto razionale esatto, con round-to-nearest ties-to-even. Gli eventuali
`source_sample_index` e `source_sample_rate` sono provenance hash-bound, non
una seconda autorità.

La chiave canonica dell’unità migra mantenendo la forma storica e sostituendo
i secondi con i tick:

```text
evaluation_unit_key =
  [evaluation_unit_id,asset_id,profile,
   segment_start_tick,segment_end_tick]
```

I secondi diagnostici non partecipano alla chiave.

Per il feature frame `j`:

```text
tau_j = RNE(frame_end_sample_j - 48000*delay_num/delay_den)
H     = 1024 tick
cell(j) = [tau_j-H, tau_j)
```

Hop, group delay e clipping sono calcolati razionalmente prima dell’unico
rounding. Per una componente dal primo frame `j_lo` all’ultimo `j_hi`:

```text
start_tick = max(segment_start_tick, tau_j_lo-H)
end_tick   = min(segment_end_tick,   tau_j_hi)
```

La migrazione di `evaluation_unit_key` e delle chiavi temporali da secondi N64
a tick interi è `PENDING_IMPLEMENTATION_O01`; gli eventuali secondi restano
soltanto diagnostici.

### 3.1 Temporal IoU

Per `A=[a0,a1)` e `B=[b0,b1)`:

```text
I_t   = max(0, min(a1,b1)-max(a0,b0))
U_t   = (a1-a0)+(b1-b0)-I_t
IoU_t = I_t/U_t
```

`U_t` DEVE essere positivo dopo admission. Tutti i confronti usano gli interi
o il razionale esatto, senza divisione binary64.

---

## 4. Geometria frequenziale canonica

La griglia contiene 120 centri canonici ordinati. Fra centri adiacenti esistono
119 boundary. La singola autorità operativa è una boundary table golden
bit-exact; `argmin` sulla distanza logaritmica è soltanto la spiegazione
matematica.

### 4.1 Artefatto boundary

L’artefatto O-02 è `PENDING_ARTIFACT_O02` e DEVE contenere:

```text
schema
grid_version
center_binary64_bits[120]
boundary_binary64_bits[119]
center_hash
boundary_hash
artifact_hash
generator_provenance
```

I bit sono stringhe `0x` più sedici cifre esadecimali minuscole. L’encoding è
canonical JSON UTF-8 con LF finale, chiavi ordinate e nessun float decimale
per centri o boundary.

```text
center_hash   = SHA256(canonical_bytes(center bit list))
boundary_hash = SHA256(canonical_bytes(boundary bit list))
artifact_hash = SHA256(canonical artifact excluding artifact_hash)
```

Il generatore legge i centri come razionali esatti, calcola
`sqrt(center_i*center_{i+1})` con interval refinement e aumenta la precisione
finché l’intero intervallo arrotonda allo stesso binary64. L’artefatto golden,
non il generatore, è l’autorità.

### 4.2 Proiezione

Per ogni valore finito `x`:

```text
x <= boundary[0]                         -> band 0
boundary[k-1] < x <= boundary[k]         -> band k, 1 <= k <= 118
x > boundary[118]                        -> band 119
```

Il tie sul boundary appartiene alla banda inferiore. Un `x` finito fuori dalla
griglia satura a 0 o 119. Un `x` non finito produce FAIL prima della
proiezione. `raw_lo > raw_hi` produce FAIL.

Quando presente o richiesto dalla classe, `center_hz` deve essere N64 finito e
strettamente positivo. `null` è ammesso soltanto per i campi/classi che lo
schema dichiara esplicitamente. Quando presente, `width_octaves` deve essere
N64 finito in `[0,W_MAX]`, dove:

```text
W_MAX = correctly_rounded_binary64(2*log2(20000/20))
```

Il bit pattern golden di `W_MAX` è `PENDING_ARTIFACT_O03`. Un valore oltre
`W_MAX` produce FAIL. I limiti semantici per classe restano diagnostici fino a
evidenza preregistrata.

L0 conserva center/width raw per provenance. L2 contiene la banda canonica
inclusiva `[i_lo,i_hi]`. L3 applica la validità strutturale della sezione 5.
L4 usa esclusivamente la geometria canonica per le classi band-based.

Per record band-based non-Resonance, se `x` e `y` coincidono in tutti i campi
normativi non frequenziali e differiscono soltanto in center/width raw, allora:

```text
B(x)=B(y)
  => eligibility(x,z)=eligibility(y,z)
  => geometric_cost(x,z)=geometric_cost(y,z)
```

per ogni `z` ammesso.

### 4.3 Derivazione normativa degli endpoint

`DECISIONE_R23_01 — APPROVATA E FIRMATA`

Il calcolo trascendentale di `center*2^(±width/2)` DEVE usare l’operatore
offline `project_center_width_v1`, che:

1. legge center e width dai bit N64 come razionali esatti;
2. calcola entrambi gli endpoint con interval refinement di `exp2`;
3. aumenta la precisione finché ciascun endpoint arrotonda univocamente a
   binary64;
4. applica la boundary table golden ai bit risultanti;
5. materializza e verifica `[i_lo,i_hi]`.

Fino al congelamento dell’operatore, dei golden e del relativo digest, O-02/O-03
non sono attivabili.

---

## 5. Validità strutturale del Ground Truth

Lo scope di deduplicazione GT è una singola:

```text
annotation_top_level_key =
  (evaluation_unit_key, annotator_id, pass_id)
```

Annotatori o pass differenti non sono confrontati come duplicati.

`semantic_region` ammette tutti gli otto problem type. `dynamic_event` ammette
soltanto Resonance, Harshness e Sibilance.

La structural key è:

```text
annotation_top_level_key
kind
problem_type
start_tick
end_tick
i_lo
i_hi
```

Resonance aggiunge `center_band_index` canonico, non il center N64 raw.
Thinness e DullSound aggiungono `direction`. Il payload supervisionale è
esattamente:

```text
[N64(severity), N64(confidence), actionable]
```

Raw center/width/band/timing già proiettati, secondi diagnostici,
`problem_type_id`, ID, schema e provenance non partecipano alla structural
key.

```text
stessa structural key + stessi canonical payload bytes -> DUPLICATE_GT -> FAIL
stessa structural key + payload differente             -> CONTRADICTORY_GT -> FAIL
structural key differente                               -> target distinto
```

Non esiste fuzzy deduplication. La migrazione degli helper di identità
Resonance verso `center_band_index` è `PENDING_IMPLEMENTATION_O15`.

---

## 6. Grafo bipartito ed eligibility

Il grafo è costruito soltanto dopo admission e validity fail-closed. Ogni
sottografo ha scope:

```text
(evaluation_unit_key, record_family, problem_type)
```

`DECISIONE_R23_04 — APPROVATA E FIRMATA`

**Classificazione:** chiusura di sicurezza anti-gaming, non semplice scelta
editoriale o di serializzazione.

Poiché `record_family` entra nelle chiavi scientifiche, la sua enumerazione è
derivata, normativa e non candidate-controlled:

```text
record_family = "semantic_region"
  per semantic_region <-> semantic_bundle

record_family = "dynamic_event"
  per dynamic_event <-> prediction_event
```

`record_family` non è letto dal record: è derivato dai kind già validati. Un
campo omonimo fornito da annotation o prediction NON DEVE influenzare
admission, eligibility, partitioning, chiavi scientifiche o metriche; se lo
schema non lo ammette produce FAIL come chiave extra. In questo modo un
candidato non può auto-dichiarare una famiglia diversa per modificare il grafo
o scavalcare le regole cross-family.

Le sole coppie di famiglie ammesse sono:

```text
semantic_region <-> semantic_bundle     per gli otto problem type
dynamic_event   <-> prediction_event   per Resonance/Harshness/Sibilance
```

Non esistono archi cross-family o cross-type.

Un arco esiste se e solo se tutte le condizioni seguenti sono vere:

1. stessa `evaluation_unit_key`, `record_family` e `problem_type`;
2. record validi e trusted validity mask valida;
3. `IoU_t >= 3/10`, confrontata come `10*I_t >= 3*U_t`;
4. inoltre, per classe:

```text
Resonance:
  max(center_GT,center_P)^3 <= 2*min(center_GT,center_P)^3

Muddiness, Boominess, BoxyMidrange, Harshness, Sibilance:
  band IoU >= 1/2

Thinness, DullSound:
  band IoU >= 1/2
  direction_GT == direction_P
```

La soglia di banda è confrontata come `2*I_b >= U_b`. Center e cubi sono
razionali esatti derivati da N64; center non positivo produce FAIL in
admission.

Queste soglie sono definizioni operative preregistrate di “stesso target”, non
confini percettivi dimostrati. Non possono essere ritoccate dopo aver osservato
risultati del candidato.

---

## 7. Obiettivo scientifico esatto

Un matching `M` è one-to-one e contiene soltanto archi eleggibili.

```text
V(M) = (K1,K2,K3,K4)
direzioni = (max,max,min,min)
```

`K5_RESERVED` è inutilizzato in REV8. Non partecipa a eligibility,
optimization, matching equivalence o canonicalization. K6 segue K4.

### 7.1 K1

```text
K1(M) = |M|
```

Per il matching operativo:

```text
TP = K1
FP = |P|-K1
FN = |GT|-K1
```

### 7.2 K2

```text
K2(M) = sum_{e in M} IoU_t(e)
```

La somma è razionale esatta e viene massimizzata.

### 7.3 K3

```text
K3(M) =
  sum_{(g,p) in M} (
    abs(g.start_tick-p.start_tick)
    + abs(g.end_tick-p.end_tick)
  )
```

K3 è un intero esatto e viene minimizzato.

### 7.4 K4 per classi band-based

Per bande inclusive `[a,b]` e `[c,d]`:

```text
I_b = max(0,min(b,d)-max(a,c)+1)
L_g = b-a+1
L_p = d-c+1
U_b = L_g+L_p-I_b
IoU_b = I_b/U_b
1-IoU_b = (U_b-I_b)/U_b
```

Per tutte le classi non-Resonance:

```text
K4(M) = sum_{e in M} (U_b(e)-I_b(e))/U_b(e)
```

La somma razionale esatta viene minimizzata.

### 7.5 K4 Resonance

Per ogni arco:

```text
r_e = max(center_GT,center_P)/min(center_GT,center_P)
```

Per Resonance:

```text
K4(M) = product_{e in M} r_e
```

Il prodotto razionale positivo esatto viene minimizzato. È matematicamente
equivalente a minimizzare la somma non pesata degli errori assoluti in ottave,
senza `libm`. I centri devono essere finiti e positivi, K1 è già fissato e
non sono ammessi pesi reali/non interi.

### 7.6 Empty matching

```text
K1=0
K2=0
K3=0
K4=1  per Resonance
K4=0  per costi additivi
```

---

## 8. Optimum, oracle A1 e runtime A2

Sia `V*` il migliore vettore lessicografico fra tutti i matching one-to-one
eleggibili:

```text
M* = { M : V(M)=V* }
```

Equality e ordering sono matematicamente esatti. `N/A` non è una componente
dell’obiettivo; infeasibility è un esito separato.

### 8.1 A1 — oracle normativo

A1 enumera esaustivamente tutti i matching sui grafi piccoli del dominio
oracle congelato, calcola `V*`, l’intero `M*`, `S_can` e `M_replay`. A1 è
l’autorità matematica e NON l’algoritmo runtime obbligatorio.

### 8.2 A2 — runtime

A2 può usare qualunque algoritmo esatto purché:

- restituisca gli stessi `V*`, `S_can` e `M_replay` di A1 sull’intero dominio
  oracle;
- superi fixture avversarie e property test;
- calcoli esattamente gli envelope richiesti dalle sezioni 11.2 e 11.3;
- rispetti cap e bit complexity attivati;
- non usi greedy, first-fit, float accumulation o fallback approssimati.

La rappresentazione interna di somme e prodotti è libera se osservazionalmente
equivalente. La canonicalizzazione di valori materializzati deve essere unica.

---

## 9. Firma scientifica K6 e replay diagnostico

`R23A_02 — PROPOSTA, FIRMA RICHIESTA`

K6 materializza una firma scientifica riproducibile di `M*`; non modifica
`V*`, non sceglie il valore di metriche ambiguity-sensitive e non deve
necessariamente identificare un unico matching a livello di oggetti. La scelta
di un matching concreto per replay/debug è un’operazione diagnostica separata.

Entrambi gli endpoint scientifici usano `evaluation_unit_key`.
`prediction_top_level_key = evaluation_unit_key`;
`annotation_top_level_key` resta soltanto diagnostica.

La geometria scientifica ordinata è:

```text
Resonance:
  [null,null,resonance_center_N64,null]

Thinness, DullSound:
  [i_lo,i_hi,null,direction]

altre classi:
  [i_lo,i_hi,null,null]
```

Le chiavi sono:

```text
scientific_gt_key =
  ["gt",evaluation_unit_key,record_family,problem_type,
   start_tick,end_tick,class_geometry]

scientific_prediction_key =
  ["prediction",evaluation_unit_key,record_family,problem_type,
   start_tick,end_tick,class_geometry]

scientific_edge_key =
  [scientific_gt_key,scientific_prediction_key]
```

Escludono severity, confidence, actionable, ID, hash, annotator/pass, secondi e
geometria raw non normativa.

```text
diagnostic_gt_key =
  ["gt-diagnostic",annotation_top_level_key,scientific_gt_key,
   canonical_item_payload_GT]

diagnostic_prediction_key =
  ["prediction-diagnostic",scientific_prediction_key,
   canonical_item_payload_P,occurrence_ordinal]

diagnostic_edge_key =
  [diagnostic_gt_key,diagnostic_prediction_key]
```

`canonical_item_payload_*` è l’exact-key item dello schema v2 dopo N64 e dopo
la sola rimozione di schema, ID derivato e occurrence ordinal.

Per un gruppo di N prediction con payload diagnostico identico, gli ordinali
ammessi sono esattamente il multiset `{0,...,N-1}`, verificato dopo grouping e
indipendentemente dall’ordine d’ingresso. Il GT non usa ordinali.

Gli array sono confrontati mediante canonical bytes in unsigned lexicographic
order. Per ogni `M in M*`:

```text
S(M) = sort(scientific_edge_key(e) for e in M)
D(M) = sort(diagnostic_edge_key(e) for e in M)

S_can    = min_{M in M*} S(M)
M_replay = unique argmin_{M in M*} (S(M),D(M))
```

Il confronto è gerarchico: prima l’intera `S`, poi, soltanto fra `S` identiche,
l’intera `D`. È vietata una tupla per-edge `(scientific,diagnostic)`.

Severity, confidence, actionable, ID e hash non possono influire su
eligibility, `V*`, `M*`, `S`, `S_can`, metriche o gate. Possono comparire
soltanto nel payload diagnostico e influire su `D` e `M_replay`. `M_replay` è
un output esclusivamente diagnostico: non è K6 scientifico e non può essere
usato per attribuire un punteggio.

---

## 10. Cap e failure policy

Il preflight è eseguito per sottografo
`(evaluation_unit_key,record_family,problem_type)` prima del calcolo parziale.

`R23A_05 — PROPOSTA, FIRMA RICHIESTA`

Per un intero matematico `z`:

```text
int_bit_length(0) = 0
int_bit_length(z) = floor(log2(abs(z)))+1, se z != 0
```

Per un razionale canonico ridotto `q=n/d`, con `d>0` e
`gcd(abs(n),d)=1`:

```text
rational_bit_length(q) =
  int_bit_length(n) + int_bit_length(d)
```

Il segno non conta come bit. La misura normativa è applicata al valore
matematico ridotto, non alla rappresentazione interna scelta
dall’implementazione. Per gli interi si usa `int_bit_length`; per i razionali
si usa `rational_bit_length`.

Lo scope comprende il valore matematico di ogni componente esatta di `V`, di
ogni accumulatore esatto di envelope e di ogni altro scalare che il contratto
richiede di confrontare o serializzare, indipendentemente dal fatto che una
specifica implementazione lo materializzi. I temporanei
implementation-specific non sono una soglia normativa; OOM o eccezioni
restano failure operativi.

L’eventuale addendum di attivazione O-09 DEVE fissare atomicamente:

1. il bound conservativo di preflight, derivato dal numero di archi e dai
   bit-length degli input, che domina tutti gli scalari normativi;
2. lo scope esatto per componente, envelope e sottografo;
3. la soglia numerica attivata;
4. fixture appena sotto, sul limite e appena sopra;
5. reason code e blast radius.

Nessuno di questi elementi può essere attivato isolatamente.

Ceiling candidati, non ancora attivi:

```text
GT <= 128
prediction <= 128
eligible edges <= 16384
exact scalar bit-length <= 65536
```

Working space e temporanei sono implementation-specific. A1 exhaustive è
limitato al dominio oracle e non impone un numero di chiamate al runtime.

I numeri sono `PROVISIONAL_PENDING_BENCHMARK_O09`. Non producono PASS/FAIL
finché un ballot addendum, dopo benchmark A2 sul platform lock, non li attiva.
Finché non sono attivati, `REV8 SPEC GO = NO`.

Reason code fatali:

```text
SOLVER_STRUCTURAL_LIMIT_EXCEEDED
SOLVER_RUNTIME_FAILURE
SOLVER_CONSTRAINT_MODEL_INVALID
```

Ogni failure fatale in un sottografo che contribuisce a una metrica di gate
impedisce PASS. Sono ammessi report diagnostici parziali, mai un PASS
scientifico parziale. Non esiste `SOLVER_INFEASIBLE_CONSTRAINT_SET` generico.

---

## 11. Metriche basate sul matching

### 11.1 Invarianti di cardinalità

TP, FP, FN, precision, recall e F1 dipendono da K1 e sono invarianti dentro
`M*`. Precision e recall con denominatore zero seguono le regole fail-closed
del contratto principale; N/A non viene convertito in zero.

### 11.2 Severity MAE conservativa

Per una partizione con `K1>0`:

```text
Q_severity(M) =
  sum_{(g,p) in M} abs(severity_GT-severity_P)/K1

Q_severity_plus = max_{M in M*} Q_severity(M)
```

I valori N64 sono razionali esatti. Il gate usa l’upper envelope.
Lower envelope e valore su `M_replay` sono diagnostici. `K1=0` produce N/A.

Se A2 non calcola l’envelope esattamente entro i cap attivi:

```text
N/A / PAIRING_ENVELOPE_UNAVAILABLE
```

Se la metrica è obbligatoria, N/A impedisce PASS.

### 11.3 Onset e offset conservativi

Per `K1>0`:

```text
Q_on(M)  = sum abs(start_tick_GT-start_tick_P)/K1
Q_off(M) = sum abs(end_tick_GT-end_tick_P)/K1
```

Onset e offset usano upper envelope distinti su `M*`. Il gate usa il worst
case. La conversione tick→ms avviene soltanto dopo il calcolo esatto.
`K1=0` produce N/A. Envelope indisponibile entro i cap produce N/A e, se
obbligatorio, nessun PASS.

### 11.4 Spearman

Spearman è calcolato per
`(group_id,record_family,problem_type)` come Pearson sui midrank, con rank
medio per i tie e pooling delle coppie matched nelle unità complete.

Per il gruppo `g`:

```text
M_g* = CartesianProduct(M_u* for contributing unit/subgraph u)
```

Rho è pubblicabile soltanto se, per ogni matching composto:

1. il numero totale di coppie matched è `n>=10`;
2. entrambi i vettori hanno varianza non zero;
3. l’insieme dei valori matematici esatti di rho è singleton.

Reason code:

```text
almeno un n<10             -> N/A / INSUFFICIENT_MATCHED_SUPPORT
almeno una varianza zero   -> N/A / SPEARMAN_UNDEFINED
rho non singleton          -> N/A / PAIRING_AMBIGUOUS
```

Se Spearman è gate obbligatorio, N/A impedisce PASS. `M_replay` è diagnostico.

### 11.5 Equality e pubblicazione normativa di rho

`DECISIONE_R23_02 — APPROVATA E FIRMATA`

Per evitare una radice non normativa nell’uguaglianza, ogni rho DEVE essere
rappresentato tramite segno e tripla razionale esatta
`(cov^2,var_x,var_y)`. Due rho non nulli con lo stesso segno sono uguali se:

```text
cov_1^2 * var_x_2 * var_y_2
  = cov_2^2 * var_x_1 * var_y_1
```

La pubblicazione binary64 del rho singleton usa interval refinement della
radice finché il risultato arrotonda univocamente.

---

## 12. Aggregazione, supporto e quantili

Per ogni metrica, `G_eligible` è congelato da manifest, `complete_types` e GT
prima di leggere le prediction.

Il report pubblica:

```text
G_eligible
G_defined
G_NA = G_eligible \ G_defined
reason code per gruppo N/A
```

`G_defined` conta i `group_id` con almeno una evaluation unit in cui la metrica
è definita. I floor di gate si applicano a `G_defined` oltre ai floor
GT-positivi preregistrati. Supporto insufficiente per una metrica obbligatoria
produce N/A e impedisce PASS. Le unità N/A non valgono zero.

`R23A_03 — PROPOSTA, FIRMA RICHIESTA`

L’operatore byte-authoritative per una media non pesata è:

```text
mean64(entries):
  N=0 -> N/A
  N>0 ->
    ordered = sort(entries, by=normative_entry_key)
    s = sum_pairwise64(value64(e) for e in ordered)
    return RN64(exact_rational(s)/N)
```

`exact_rational(s)` è il razionale esatto rappresentato dal binary64 `s`.
La divisione avviene una sola volta dopo `sum_pairwise64`. Dividere ciascun
elemento per `N` prima della riduzione è vietato.

Le chiavi di riduzione sono:

```text
osservazioni edge-based -> [scientific_edge_key, metric_value_N64]
altre osservazioni      -> metric-specific normative observation key
unità nel gruppo        -> evaluation_unit_key
gruppi nel macro        -> UTF-8 bytes di group_id
```

Se due entry hanno la stessa chiave e lo stesso valore, il loro ordine è
irrilevante. Ogni metrica che non possiede una chiave normativa totale produce
FAIL di materializzazione, non una riduzione dipendente dall’input order.
Medie pesate non sono autorizzate in REV8; richiedono un amendment separato.

Gerarchia obbligatoria:

1. `mean64` delle osservazioni definite nella evaluation unit;
2. `mean64` dei valori di unità definiti nel gruppo;
3. `mean64` dei valori di gruppo definiti, ciascuno con peso 1.

La macro-mean è la statistica primaria e di gate, subordinata ai gate di recall
e copertura. Le micro-metriche sono soltanto diagnostiche.

Il p95 è diagnostico e usa Type-7 con `q=0.95`. Ordinati
`x[0] <= ... <= x[G-1]`:

```text
G=0 -> N/A
G=1 -> x[0]
G>=2:
  h = (G-1)*q
  j = floor(h)
  gamma = h-j
  p95 = (1-gamma)*x[j] + gamma*x[j+1]
```

Un futuro gate p95 richiede floor e power analysis congelati.

### 12.1 Errore centro Resonance

Per una partizione con `K1>0`, dal prodotto esatto firmato:

```text
R(M) = product r_e
mean_center_error_oct(M) = log2(R(M))/K1
```

Poiché K1 e K4 sono componenti di `V*`, `R(M)` e la mean center error sono
invarianti su `M*`; K6 non può modificarle.

`K1=0` produce N/A. La convenzione `K4=1` (`R(M)=1`) di §7.6 vale soltanto per
il matching vuoto dentro l'obiettivo dell'optimizer; non ridefinisce
`mean_center_error_oct`, che resta un rapporto senza coppie matched su cui
mediare e non "0" per `log2(1)=0`.

`DECISIONE_R23_03 — APPROVATA E FIRMATA; WORDING SUPERATO DA R23A_01`

`R23A_01 — PROPOSTA, FIRMA RICHIESTA`

Per la sola pubblicazione DEVE essere usato:

```text
mean_center_error_oct64(R,K1) =
  correctly_round_binary64(log2(R)/K1)
```

L’interval refinement opera sull’intera espressione `log2(R)/K1` finché
l’intero intervallo arrotonda allo stesso binary64. È vietato materializzare
un binary64 intermedio per `log2(R)`. L’optimizer continua a confrontare il
prodotto razionale e non usa `libm`. Golden e digest dell’operatore sono
obbligatori prima dell’attivazione.

Fixture normativa:

```text
R=9/8, K1=3
mean_center_error_oct64 = f64:3fad0022f7fa735f

f64:3fad0022f7fa7360 è il risultato double-rounded vietato.
```

---

## 13. Average Precision

Average Precision è calcolata per:

```text
(group_id, record_family="dynamic_event", problem_type)
```

Sono incluse esclusivamente unità complete per la classe, GT `dynamic_event` e
prediction `prediction_event` pre-threshold. La confidence è quella calibrata
dalla policy congelata e viene ricalcolata dall’evaluator.

Siano `t_1>...>t_K` i valori binary64 distinti di confidence. A ogni soglia:

```text
P_t = { prediction : confidence >= t }
```

Tutte le prediction con confidence uguale entrano atomicamente; ordine frame o
banda è soltanto presentazione. Il matching normativo viene ricalcolato.

```text
P_k = TP_k/(TP_k+FP_k)
R_k = TP_k/N_GT
R_0 = 0
AP  = sum_k (R_k-R_{k-1})*P_k
```

Si usa la precisione osservata, senza trapezi, precision envelope o endpoint
artificiali.

```text
N_GT>0 e K=0 -> AP=0
N_GT=0       -> N/A; il gruppo contribuisce alla clean safety
```

Per ogni gruppo, l’AP razionale esatta viene arrotondata una volta a
`AP_group64`. La macro-AP usa `mean64` sugli `AP_group64`, ordinati per UTF-8
bytes di `group_id`; include soltanto gruppi GT-positivi eleggibili, peso
gruppo=1 e floor preregistrato. Il nome normativo è `average_precision`. Un
alias legacy `PR-AUC` DEVE dichiarare `convention=average_precision`.

---

## 14. Test oracle, metamorphic e mutation

Prima di patchare il candidate devono esistere almeno:

### 14.1 Numeric e temporal

- N64: signed zero, finite/non-finite, overflow, cross-language bit-to-rational;
- interi `2^53+1` e `2^60+1` senza conversione float;
- fixture 2 ULP `0.2` vs bit della differenza `1.2-1.0`;
- fixture compound-rounding `R=9/8,K1=3`: output
  `f64:3fad0022f7fa735f`, mutation double-rounding
  `f64:3fad0022f7fa7360` uccisa;
- `mean64([0.2,0.3,1.0]) = f64:3fe0000000000000`; mutation divide-first
  `f64:3fdfffffffffffff` uccisa;
- controesempio valido `d=0.25`;
- source-index→tick senza double rounding a 44.1/48/96 kHz;
- segment clipping, group delay e frame-cell half-open.

### 14.2 Projection

- NaN e ±Inf → FAIL;
- finito sotto/sopra griglia → banda 0/119;
- uguaglianza al boundary → banda inferiore;
- `raw_lo>raw_hi` → FAIL;
- `raw_lo==raw_hi` → intervallo di una banda;
- generator correctly-rounded e hash stability;
- equivalenza canonica per classi band-based.

### 14.3 Eligibility

Per ogni soglia e classe la suite usa appena dentro, esattamente sul bordo e
appena fuori soltanto quando l’uguaglianza è raggiungibile nel dominio
ammesso.

`R23A_04 — PROPOSTA, FIRMA RICHIESTA`

Per la soglia center di Resonance l’uguaglianza richiederebbe un rapporto
razionale `q` con `q^3=2`, che non esiste. La fixture normativa usa quindi un
minimo fisso e i due N64 adiacenti che racchiudono il bordo:

```text
min center = f64:408f400000000000  # 1000.0
inside     = f64:4093afaf27b421db  # cube <= 2*min^3
outside    = f64:4093afaf27b421dc  # cube >  2*min^3
```

Il generator report DEVE certificare con interval refinement che i due valori
sono adiacenti e si trovano sui lati opposti di `1000*cuberoot(2)`. La suite
include inoltre la prova normativa dell’irraggiungibilità dell’uguaglianza.
Non è richiesta una fixture N64 “esattamente sul bordo” impossibile.

Includere cross-family, cross-type, direction mismatch, validity invalid e
tentativo di fornire/alterare `record_family` dal record.

### 14.4 Optimizer

- A1 exhaustive su tutti i grafi congelati con `|GT|<=3`, `|P|<=3`;
- A2 uguale ad A1 su `V*`, `S_can`, `M_replay` e ogni output derivato
  richiesto; se A2 materializza `M*`, set equality esatta con A1 nel dominio
  oracle;
- permutazione input, rinomina ID/hash e adjacency invariance;
- float-vs-exact adversarial fixture;
- bit-length bound e preflight: il razionale ridotto
  `2^39999/(2^39999+1)` ha `rational_bit_length=80000`, non 40000;
- envelope severity/onset/offset su tutti gli optimum.

### 14.5 Structural validity e keys

- duplicate, contradictory e distinct per tutte le classi;
- Resonance structural key usa center band, non raw center;
- ordinali prediction esattamente `0..N-1`;
- `S_can` esclude severity, confidence, actionable, ID e hash;
- modificare i campi diagnostici non cambia `S_can`, metriche o gate;
- `M_replay` resta deterministico tramite confronto gerarchico dell’intera S
  prima dell’intera D.

### 14.6 Mutation obbligatorie

Le mutation seguenti DEVONO essere uccise:

- attivare `K5_RESERVED` come obiettivo;
- lasciare che K5 influenzi eligibility, equivalence o K6;
- invertire K3;
- sostituire somme esatte con float;
- usare ID/hash/severity/confidence/actionable in `S` o `S_can`;
- lasciare che `D` o `M_replay` influenzino metriche o gate;
- ignorare o dipendere dall’ordine d’ingresso per `occurrence_ordinal`;
- invertire K6;
- usare greedy/first-fit;
- reintrodurre center/width raw nel costo band-based;
- sostituire IoU esatta con binary64 arrotondata presto;
- tagliare AP posizionalmente dentro un tie di confidence;
- usare `M_replay` per severity/onset/offset/Spearman;
- leggere `record_family` da annotation/prediction invece di derivarlo dai
  kind validati.

---

## 15. Ledger delle materializzazioni e dei gap

| ID | Decisione | Stato dopo la firma | Condizione prima del candidate/GO |
|---|---|---|---|
| O-01 | tick 48 kHz autoritativi | FIRMATA | schema/key migration + exact-int test |
| O-02 | boundary golden | DECISA | artefatto 120/119 + digest + generator report |
| O-03 | W_MAX e aliasing | FIRMATA | bit golden W_MAX + projection operator |
| O-04a | eligibility | FIRMATA | fixture di bordo per classe |
| O-04 | K2 | FIRMATA | exact arithmetic tests |
| O-05 | K3 | FIRMATA | tick integration tests |
| O-06 | K4 band | FIRMATA | exact rational tests |
| O-06a | K4 Resonance product | FIRMATA | A2 equivalence + bit bound |
| O-07 | K5 reserved | FIRMATA | mutation tests |
| O-08 | A1 oracle/A2 runtime | FIRMATA; R23A_02 PENDING | oracle domain + A2 proof su `V*`,`S_can`,`M_replay` |
| O-08a | exact objective semantics | FIRMATA | serialization/cap tests |
| O-09 | fail policy | FIRMATA; R23A_05 PENDING; NUMERI PROVVISORI | bit-length/preflight definition + benchmark + ballot addendum di attivazione |
| O-10 | severity envelope | FIRMATA | exact envelope A2 |
| O-11 | onset/offset envelope | FIRMATA | exact envelope A2 |
| O-12 | Spearman singleton | FIRMATA | exact singleton algorithm + full fixture |
| O-13 | macro mean/p95 | FIRMATA; R23A_01/R23A_03 PENDING | `mean64`, compound-rounding e support/floor fixtures |
| O-14 | distinct-confidence | DECISA | wording e tie tests |
| O-14b | Average Precision | FIRMATA | AP golden |
| O-15 | structural validity | FIRMATA | schema/key migration |
| O-16 | kind literals | DECISA | transfer + hash tests |
| O-17 | problem type ID | DECISA | wording + partition test |
| O-18 | numeric ownership | DECISA; R23A_01/R23A_03/R23A_05 PENDING | `sum_pairwise64`/N64/mean/bit-length artifacts e golden |
| O-19 | revision authority | DECISA | manifest/dispatcher transfer |
| O-20 | K6 keys/order | FIRMATA; R23A_02 PENDING | `S_can`/`M_replay` key, golden e mutation tests |

Decisioni aggiuntive Round 2.3 firmate:

| Decisione | Oggetto | Stato |
|---|---|---|
| R23_01 | endpoint `center,width` via interval-refined exp2 | APPROVATA/FIRMATA |
| R23_02 | equality/pubblicazione esatta Spearman | APPROVATA/FIRMATA |
| R23_03 | wording storico center error; sostituzione proposta da R23A_01 | APPROVATA/FIRMATA; SUPERSEDED_PENDING_R23A_01 |
| R23_04 | literal `record_family` derivati; chiusura anti-gaming | APPROVATA/FIRMATA |
| R23_05 | boundary unico esatto→binary64 per metriche | APPROVATA/FIRMATA |

Micro-amend proposto dopo il red-team ristretto:

| Decisione | Finding | Oggetto | Stato |
|---|---|---|---|
| R23A_01 | RT-001 | un solo rounding su `log2(R)/K1` | PENDING_SIGNATURE |
| R23A_02 | RT-002 | K6=`S_can`; `M_replay` solo diagnostico | PENDING_SIGNATURE |
| R23A_03 | RT-003 | gerarchia `mean64` byte-authoritative | PENDING_SIGNATURE |
| R23A_04 | RT-004 | fixture Resonance senza equality impossibile | PENDING_SIGNATURE |
| R23A_05 | RT-005 | funzione e scope exact bit-length | PENDING_SIGNATURE |

---

## 16. Sequenza di attivazione

1. redazione e revisione iniziale Round 2.3 — COMPLETATA;
2. firma delle cinque decisioni R23 — COMPLETATA;
3. red-team indipendente ristretto al testo consolidato — COMPLETATO, `BLOCK`;
4. micro-amend limitato a RT-001–RT-005 — REDATTO, FIRMA PENDING;
5. firma R23A_01–R23A_05 da parte dell’autorità scientifica;
6. recheck ristretto dei cinque finding sul nuovo SHA;
7. soltanto se CLEAN, patch document-only del candidate REV8;
8. counter-check sul commit candidate immutabile;
9. materializzazione degli artefatti O-02/O-03/O-18 e benchmark O-09;
10. ballot addendum che attiva i cap O-09;
11. eventuale `REV8 SPEC GO`;
12. soltanto dopo, implementazione/trasferimento autorizzati secondo governance.

Le firme del ballot, dell’addendum R23 e dell’eventuale micro-amend R23A non
autorizzano da sole implementazione, attivazione o `REV8 SPEC GO`.
