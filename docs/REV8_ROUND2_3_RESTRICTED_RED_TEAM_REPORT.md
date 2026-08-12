# REV8 — ROUND 2.3 — RESTRICTED RED-TEAM REPORT

**Verdetto consolidato:** `BLOCK`
**Target:**
`docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md`
**SHA-256 iniziale e finale:**
`8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041`
**Protocollo:**
`docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_PROTOCOL.md`
**Modalità:** tre lenti indipendenti, read-only, consolidamento senza voto di
maggioranza.
**Data:** 2026-07-29

```text
REV8 SPEC GO = NO
Candidate patch = NON AUTORIZZATA
Implementation  = NON AUTORIZZATA
Training        = NON AUTORIZZATO
Target modified = NO
```

---

## 1. Esiti delle tre lenti

| Lens | Scope | Verdetto | Finding materiali |
|---|---|---:|---:|
| Lens A | Optimizer / Numeric | BLOCK | 2 BLOCKER, 2 MAJOR |
| Lens B | Metrics / Statistics | BLOCK | 1 BLOCKER, 1 MAJOR |
| Lens C | Semantics / Security / Governance | AMEND | 1 MAJOR |

I finding A-001/C-01 e A-002/B-001 sono duplicati indipendenti e sono stati
fusi senza perdere le rispettive catene causali.

Verdetto secondo protocollo:

```text
almeno un BLOCKER riprodotto -> BLOCK
```

---

## 2. Matrice consolidata

| ID | Origine | Riproduzione consolidatore | Severità finale | Decisioni coinvolte |
|---|---|---|---|---|
| RT-001 | A-002, B-001 | Sì, differenza 1 ULP | BLOCKER | R23_03, R23_05, O-13/O-18 |
| RT-002 | A-001, C-01 | Sì, grafo con tie scientifico | BLOCKER | O-20, O-08 |
| RT-003 | B-002 | Sì, macro-mean differisce 1 ULP | MAJOR | R23_05, O-13, O-14b, O-18 |
| RT-004 | A-003 | Sì, prova razionale | MAJOR | O-04a, fixture §14.3 |
| RT-005 | A-004 | Sì, 40000 vs 80000 bit | MAJOR | O-08a, O-09 |

---

## 3. RT-001 — BLOCKER — Center error con due boundary di rounding

### Contraddizione

R23_05 prescrive un unico rounding della statistica trascendentale completa.
La sezione 12.1 prescrive invece:

1. rounding binary64 di `log2(R)`;
2. successiva divisione correttamente arrotondata per K1.

I due procedimenti non sono equivalenti.

### Controesempio valido

Sottografo Resonance con tre soli archi eleggibili:

```text
g1=[0,10),     center=72
p1=[0,10),     center=64

g2=[100,110),  center=100
p2=[100,110),  center=100

g3=[200,210),  center=200
p3=[200,210),  center=200
```

Gli archi cross-pair hanno temporal IoU zero. Il matching è unico:

```text
K1 = 3
R  = (72/64)*1*1 = 9/8
```

L’arco 72/64 è eleggibile:

```text
9^3 = 729 <= 2*8^3 = 1024
```

Risultati:

```text
RN64(log2(9/8)/3)
  = 0x3fad0022f7fa735f

RN64(RN64(log2(9/8))/3)
  = 0x3fad0022f7fa7360
```

La differenza è 1 ULP. Una soglia uguale al primo valore può invertire
PASS/FAIL.

### Correzione minima

Definire un unico operatore:

```text
mean_center_error_oct64(R,K1) =
  correctly_round_binary64(log2(R)/K1)
```

L’interval refinement DEVE operare sull’intera espressione
`log2(R)/K1`. È vietato materializzare un binary64 intermedio per `log2(R)`.

### Autorità da riaprire

```text
R23_03
R23_05
O-13 center-error publication
O-18 golden numeric artifact
```

---

## 4. RT-002 — BLOCKER — D(M) usa campi vietati in K6

### Contraddizione

La sezione 9 stabilisce contemporaneamente:

1. `canonical_item_payload_P` rimuove soltanto schema, ID derivato e
   occurrence ordinal;
2. `D(M)` usa il payload completo restante;
3. `M_can = argmin(S(M),D(M))`;
4. severity, confidence e actionable non possono influire su K6.

Poiché quei tre campi restano nel payload diagnostico, influenzano
necessariamente `D(M)` e quindi `M_can`.

### Controesempio valido

Una GT Muddiness `g` e due prediction `p1,p2`:

```text
stessa evaluation_unit_key
stesso problem_type
stessi tick
stessa banda canonica
stessa eligibility e stesso costo
p1.confidence = N64(0.1)
p2.confidence = N64(0.9)
```

I due matching:

```text
M1={(g,p1)}
M2={(g,p2)}
```

hanno:

```text
V(M1)=V(M2)
S(M1)=S(M2)
D(M1)!=D(M2)
```

Scambiando le confidence cambia il rappresentante canonico, pur mantenendo
invariata la geometria scientifica.

### Impatto

Le metriche ambiguity-sensitive usano correttamente `M*`, quindi non è stato
dimostrato un false PASS diretto. Tuttavia il contratto è non implementabile
come scritto: la formula impone l’influenza dei campi mentre wording e mutation
la vietano. A1 e A2 possono produrre output canonici differenti seguendo due
letture testualmente conformi.

### Correzione minima raccomandata

Separare esplicitamente:

```text
M*       = insieme scientifico degli optimum
S        = chiave/canonicalizzazione scientifica
M_replay = rappresentante esclusivamente diagnostico selezionato da (S,D)
```

Severity, confidence e actionable:

- NON DEVONO influire su eligibility, V*, M*, S, metriche o gate;
- POSSONO influire su D e `M_replay` soltanto per replay/debug;
- non devono essere descritti come input di un K6 scientifico.

Adeguare O-08, O-20 e le mutation. Se si mantiene il nome `M_can`, il testo
DEVE dichiarare senza ambiguità che è un output diagnostico e che il divieto
riguarda tutti gli output scientifici, non D.

### Autorità da riaprire

```text
O-20
O-08
A1/A2 golden e mutation K6
```

---

## 5. RT-003 — MAJOR — Mean e macro-mean non byte-authoritative

### Ambiguità

Il target definisce `sum_pairwise64` e la gerarchia:

```text
observation -> unit mean -> group mean -> macro mean
```

ma non stabilisce se la divisione/pesatura avvenga prima o dopo la riduzione,
né l’ordine canonico completo a ogni livello.

### Controesempio

Tre unità definite:

```text
v1 = N64(0.2) = 0x3fc999999999999a
v2 = N64(0.3) = 0x3fd3333333333333
v3 = N64(1.0) = 0x3ff0000000000000
```

Due implementazioni compatibili con il wording corrente:

```text
A = RN64(sum_pairwise64([v1,v2,v3])/3)
  = 0x3fe0000000000000

B = sum_pairwise64([RN64(v1/3),RN64(v2/3),RN64(v3/3)])
  = 0x3fdfffffffffffff
```

La differenza è 1 ULP e può invertire un gate al confine.

### Correzione minima

Pinnare per ogni livello:

1. chiave e ordine canonico degli input;
2. momento della pesatura;
3. momento della divisione;
4. rounding della divisione;
5. comportamento N=0.

Una possibile norma, da sottoporre all’autorità, è:

```text
mean64(v[0:N]) =
  N/A                         se N=0
  RN64(sum_pairwise64(v)/N)   se N>0
```

Definire separatamente mean pesate, group mean, macro-mean e macro-AP.

### Autorità da riaprire

```text
R23_05
O-13
O-14b macro-AP
O-18 reduction ordering/golden
```

---

## 6. RT-004 — MAJOR — Fixture Resonance esatta sul bordo impossibile

### Contraddizione

La suite richiede, per ogni soglia e classe:

```text
appena dentro
esattamente sul bordo
appena fuori
```

Per Resonance il bordo è:

```text
max(center_GT,center_P)^3 = 2*min(center_GT,center_P)^3
```

I center N64, interpretati esattamente, sono razionali. Il rapporto `q` fra
due center è quindi razionale. Sul bordo servirebbe:

```text
q^3=2
```

Non esiste un razionale con questa proprietà. Se `q=a/b` è ridotto,
`a^3=2b^3` implica prima `a` pari e poi `b` pari, contraddicendo la
coprimalità.

### Impatto

La Definition of Done richiede una fixture che non può esistere nel dominio
normativo.

### Correzione minima

Per la soglia Resonance sostituire “esattamente sul bordo” con:

1. il rapporto N64 valido più vicino conosciuto dal lato interno;
2. il rapporto N64 più vicino conosciuto dal lato esterno;
3. una prova normativa che l’uguaglianza è irraggiungibile nel dominio
   razionale N64;
4. un test diretto del comparatore cubico su razionali sintetici, senza
   pretendere che rappresentino center N64, soltanto se serve coprire il ramo
   equality dell’implementazione.

La soglia firmata non cambia.

---

## 7. RT-005 — MAJOR — Funzione di bit-length non definita

### Ambiguità

Il ceiling candidato usa:

```text
reduced mathematical bit-length <= 65536
```

ma non definisce la funzione.

Per il razionale ridotto:

```text
2^39999 / (2^39999+1)
```

si ottiene:

```text
max(bitlen(n),bitlen(d)) = 40000
bitlen(n)+bitlen(d)      = 80000
```

La prima convenzione ammette il valore, la seconda supera il ceiling.

### Impatto

Il cap è oggi provvisorio e quindi non produce ancora un false PASS. Tuttavia
non può essere benchmarkato o attivato in modo riproducibile senza definire:

- normalizzazione del segno;
- funzione per interi e razionali;
- max vs somma;
- scope per componente/intermedio/sottografo;
- bound conservativo usato dal preflight.

### Correzione minima

Il ballot addendum O-09 DEVE attivare insieme:

1. la funzione formale di bit-length;
2. lo scope;
3. il bound di preflight;
4. la soglia numerica;
5. fixture appena sotto/sul limite/sopra.

---

## 8. Verifiche del consolidatore

Il consolidatore ha riprodotto:

- RT-001 con aritmetica ad alta precisione sul razionale `9/8`;
- RT-002 enumerando i due matching con S identica e D differente;
- RT-003 applicando entrambe le riduzioni ai bit N64 indicati;
- RT-004 tramite prova di irrazionalità di `cube_root(2)`;
- RT-005 applicando le due definizioni usuali di bit-length.

Il target è rimasto byte-identico:

```text
SHA iniziale = 8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041
SHA finale   = 8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041
```

Nessun reviewer ha modificato il repository.

---

## 9. Verdetto e next permitted action

```text
Round 2.3 signed draft = BLOCK
REV8 SPEC GO           = NO
```

**Next permitted action:** micro-amend documentale sul signed draft,
strettamente limitato a RT-001–RT-005, seguito da:

1. nuovo SHA del draft corretto;
2. firma dell’addendum che riapre esclusivamente le decisioni dichiarate;
3. recheck ristretto dei cinque finding sul nuovo SHA;
4. soltanto se tutti risultano chiusi, patch document-only del candidate.

Non è autorizzata alcuna modifica a codice, schema live, runtime o training.
