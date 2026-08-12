# REV8 — O-13 — SUPPORT FLOOR BINDING BALLOT (O13F_01)

**Stato:** `SIGNED — APPROVED — PENDING POST-SIGNATURE RECHECK — NOT EFFECTIVE`

Questo ballot non introduce nuovi floor numerici. Congela esclusivamente il
dispatch fra i floor già presenti nel contratto, il piano di potenza
preregistrato e le popolazioni O-13 `G_eligible` / `G_defined` / `G_NA`.

La firma, da sola, non attiva codice, non chiude O-13, non produce
`REV8 SPEC GO` e non autorizza runtime o training. Dopo la firma sono ancora
obbligatori un recheck read-only sullo SHA firmato, l'implementazione
candidate-only, fixture under/on/over e un counter-check sul commit
immutabile.

## 1. Target e catena di autorità

```text
Branch:
feature/motore-v3-rev8-spec-go

Base commit del draft:
15fd717389c064e64632b8dacd7af48e4cdc830d

Candidate REV8:
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
SHA-256:
398aea26daa6d48324f9df7e9dda54c38b332fb4fb230563d5c26172875745

Ballot scientifico O-13 già firmato:
docs/REV8_SCIENTIFIC_AUTHORITY_BALLOT_PRECOMPILED_V2.md
SHA-256:
cb799a076346ff1e33b74b50e5ceb2901e536c4fc0623fcc54ae5a5fa9c8afe0

Chiarimento R23C firmato:
docs/REV8_ROUND2_3_R23C_CONFIRMATION_BALLOT.md
SHA-256:
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

Recheck R23C CLEAN:
docs/REV8_ROUND2_3_R23C_POST_SIGNATURE_RECHECK_REPORT.md
SHA-256:
e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157

Implementazione O-13 corrente, non attiva:
ml_v3/contracts/support_v2.py
SHA-256:
036178d7cff4e61c0c83c96c63c5923d2335a2d13f19dca9ff038a0db86246b0
```

## 2. Problema residuo

L'autorità corrente ha già stabilito che:

1. `G_eligible` è congelato da manifest, `complete_types` e GT prima di
   leggere le prediction;
2. `G_defined` contiene i `group_id` con almeno una evaluation unit in cui la
   metrica è definita;
3. `G_NA = G_eligible \ G_defined` e le unità N/A non valgono zero;
4. i floor di gate si applicano a `G_defined` oltre ai floor GT-positivi;
5. supporto insufficiente per una metrica obbligatoria produce N/A e impedisce
   PASS;
6. la numerosità finale è
   `n_required = max(floor_contrattuale, n_power)`;
7. il p95 O-13 è diagnostico; un suo eventuale gate futuro richiede floor e
   power analysis separati.

I floor contrattuali esistono già, ma manca un binding byte-authoritative che
stabilisca quale popolazione verifica ciascun floor, come entra `n_power`, come
si trattano gli strata e quale artefatto impedisce di cambiare policy dopo aver
letto le prediction.

Senza questo binding, due evaluator conformi in apparenza potrebbero usare:

- `G_eligible` al posto di `G_defined`;
- un solo conteggio globale ignorando profili o source family;
- il floor contrattuale ignorando un `n_power` maggiore;
- un nuovo floor scelto dopo il risultato;
- N/A come zero oppure come gruppo silenziosamente rimosso;
- il p95 diagnostico come gate non autorizzato.

## 3. Floor già congelati — nessun numero nuovo

Questa sezione è una trascrizione operativa delle soglie già presenti nel
candidate; non le sostituisce.

### 3.1 Calibration

| Popolazione | Floor già congelato |
|---|---:|
| Tonale actionable | 50 gruppi; almeno 20 con cella positiva e 20 con cella negativa |
| Tonale clean | 50 gruppi `clean_for_action` |
| Tonale per profilo | almeno 3 actionable e 3 clean per ciascuno dei 7 profili |
| Ogni classe anomaly positiva | 30 gruppi con evento GT actionable |
| Ogni classe anomaly negativa | 50 gruppi senza evento GT della classe, almeno 30 clean |
| Anomaly source family | almeno 3 family, almeno 5 positivi ciascuna, nessuna oltre il 50% |
| Anomaly negativi per profilo | almeno 3 per ciascuno dei 7 profili |
| Calibratore specifico di strato | almeno 30 positivi e 30 negativi nello strato |

### 3.2 Development-metric e final-test

| Popolazione | Floor già congelato |
|---|---:|
| Curve e coverage tonali actionable | 30 gruppi |
| Clean actionable rate | 149 gruppi clean |
| Ogni classe anomaly positiva | 30 gruppi |
| Pool clean-safety anomaly | 149 gruppi e 149 minuti standard, 10 gruppi per profilo |
| Adapter omologo, per classe | 30 gruppi GT positivi e 30 negativi |

Restano inoltre obbligatori gli strata della matrice §11.1:

```text
tonal-controlled:
  >=5 parent group per profilo
  ogni regione tonale e direzione in >=10 gruppi

tonal-natural:
  >=5 clean e >=5 actionable group per profilo

anomaly-natural:
  >=30 positive group per classe

clean-safety:
  >=149 gruppi totali
  >=10 per profilo
  >=1 minuto clean eleggibile standard per gruppo e classe
```

### 3.3 Power plan

Per ogni metrica o strato governato dalla power analysis:

```text
n_required(metric_id, stratum_id)
  = max(floor_contrattuale(metric_id, stratum_id),
        n_power(metric_id, stratum_id))
```

`n_power` deriva soltanto dal `development_pilot` e dal piano congelato. Non
può essere ricalcolato da development-metric o final-test e non può essere
sostituito da un numero osservato più favorevole.

## 4. Decisione proposta O13F_01

### 4.1 Popolazioni per metrica e strato

Per ogni coppia normativa `(metric_id, stratum_id)` il report materializza:

```text
G_eligible(metric_id, stratum_id)
G_defined(metric_id, stratum_id)
G_NA(metric_id, stratum_id)
  = G_eligible(metric_id, stratum_id)
    \ G_defined(metric_id, stratum_id)
```

Regole:

1. `G_eligible` è congelato prima delle prediction.
2. `G_defined` è un sottoinsieme di `G_eligible` e viene derivato dagli esiti
   completi, mai selezionato per raggiungere il floor.
3. Ogni gruppo eleggibile deve avere un esito definito oppure N/A con reason
   code; un esito mancante è FAIL di materializzazione.
4. Asset, segmenti, crop, eventi, celle e unità multiple non aumentano il
   supporto indipendente: ogni `group_id` conta al massimo uno nello strato.
5. Un gruppo può contribuire a più classi soltanto nei casi multi-label già
   ammessi dal contratto; conta una volta per classe.

### 4.2 Doppio controllo obbligatorio

Per una metrica obbligatoria e ciascuno dei suoi strata richiesti, il supporto
è sufficiente soltanto se sono vere entrambe:

```text
eligible_support_ok:
  |G_eligible(metric_id, stratum_id)| >= n_required(metric_id, stratum_id)

defined_support_ok:
  |G_defined(metric_id, stratum_id)| >= n_required(metric_id, stratum_id)
```

Il primo controllo dimostra che il corpus/GT possiede il supporto
preregistrato. Il secondo impedisce che una metrica venga dichiarata valida su
un sottoinsieme piccolo e favorevole dopo che pairing ambiguity, varianza nulla,
supporto matched insufficiente o altri N/A hanno eliminato gruppi.

Per i floor che non dipendono dalla definibilità della metrica ma descrivono
una precondizione del corpus — per esempio source-family, profili, regione,
direzione, minuti clean e positivi/negativi GT — il conteggio resta una
precondizione separata e obbligatoria. Non viene sostituito dal solo totale
`G_defined`.

### 4.3 Esiti

```text
SUPPORT_SUFFICIENT
  tutti i floor eleggibili, definiti e di strato sono soddisfatti
  -> la metrica può essere valutata dal proprio gate
  -> NON significa PASS della metrica

SUPPORT_ELIGIBLE_FLOOR_NOT_MET
  almeno un floor su G_eligible / GT / corpus non è soddisfatto
  -> valore di gate N/A
  -> PASS impossibile

SUPPORT_DEFINED_FLOOR_NOT_MET
  corpus sufficiente ma G_defined sotto n_required
  -> valore di gate N/A
  -> PASS impossibile

SUPPORT_STRATUM_FLOOR_NOT_MET
  totale sufficiente ma uno strato obbligatorio è insufficiente
  -> valore di gate N/A
  -> PASS impossibile

SUPPORT_POLICY_UNAVAILABLE
  manca una policy o un power plan richiesto
  -> valore di gate N/A
  -> PASS impossibile

SUPPORT_POLICY_HASH_MISMATCH
  policy presente ma non coincide con l'hash atteso
  -> FAIL di materializzazione dell'intero report

SUPPORT_ACCOUNTING_INVALID
  popolazioni mancanti, sovrapposte, extra o incoerenti
  -> FAIL di materializzazione dell'intero report
```

Sotto floor, il valore normativo della metrica è N/A. ECE e Brier conservano
la sola eccezione diagnostica già autorizzata dal candidate quando il supporto
è maggiore di zero; per le altre metriche nessun valore numerico sotto floor
viene pubblicato, salvo che un'autorità preesistente lo classifichi
esplicitamente come diagnostico. Un diagnostico autorizzato non entra nel campo
normativo di gate, non soddisfa una claim e non viene usato per scegliere una
policy successiva.

### 4.4 p95, ECE e Brier

- Il p95 Type-7 O-13 resta diagnostico e usa soltanto i valori dei gruppi
  definiti. Non ha un floor indipendente e non può diventare gate tramite
  questo ballot.
- ECE e Brier possono essere riportati con supporto maggiore di zero come già
  previsto dal contratto, ma non dimostrano calibrazione e non soddisfano il
  gate finché tutti i floor applicabili non sono rispettati.
- N/A non viene mai convertito in zero, infinito o osservazione numerica.

## 5. Policy artifact byte-authoritative

La policy applicata da un evaluator deve essere materializzata prima di leggere
le prediction e deve avere exact-key schema almeno equivalente a:

```text
schema
contract_revision
source_contract_sha256
metric_id
split_role
mandatory
strata[]:
  stratum_id
  population_kind
  contract_floor
  power_required
  n_power
  n_required
power_plan_sha256
```

Vincoli:

1. interi non negativi, mai binary64;
2. `n_required == max(contract_floor, n_power)` quando `power_required=true`;
3. quando `power_required=false`, `n_power == null` e
   `n_required == contract_floor`;
4. `power_plan_sha256` obbligatorio quando almeno uno strato richiede power;
5. `metric_id`, `split_role`, `stratum_id` e `population_kind` appartengono a
   enumerazioni congelate, non candidate-controlled;
6. chiavi extra, duplicate, mancanti o non canoniche producono FAIL;
7. il digest è SHA-256 lowercase hex-64 dei canonical bytes dell'artefatto;
8. admission/activation deve confrontarlo con un digest atteso congelato; un
   artefatto auto-consistente ma non atteso non è autorità.

La policy non può essere generata, sostituita o ricalcolata dopo aver letto
prediction o risultati di gate.

## 6. Fixture obbligatorie prima dell'efficacia

Per ogni tipo di floor e per ogni strato canonico devono esistere fixture
durature:

```text
n_required - 1 -> insufficiente, N/A, nessun PASS
n_required     -> supporto sufficiente, gate ancora da valutare
n_required + 1 -> supporto sufficiente, gate ancora da valutare
```

La suite deve inoltre provare:

1. `G_eligible` sufficiente ma `G_defined` sotto floor -> N/A;
2. `G_defined` sufficiente in totale ma un profilo/source-family/regione sotto
   floor -> N/A;
3. `n_power > contract_floor` -> si applica `n_power`;
4. `n_power < contract_floor` -> si applica il floor contrattuale;
5. power plan mancante quando richiesto -> nessun fallback;
6. policy hash errato -> FAIL di report;
7. unità/asset duplicati nello stesso gruppo non aumentano il supporto;
8. N/A non entra in macro-mean né p95;
9. permutare manifest, unità, risultati o strata non cambia i canonical bytes;
10. il p95 diagnostico non può essere promosso a gate;
11. sotto floor il valore normativo è N/A; soltanto ECE/Brier possono
    conservare il reporting diagnostico già autorizzato con supporto >0;
12. nessun codice REV8 è esportato dal dispatcher REV7 prima dello switch
    atomico.

Le fixture sui vincoli non riducibili a un semplice minimo includono inoltre:

```text
30 positivi anomaly, 3 source family, almeno 5 ciascuna:
  concentrazione massima 15/30 -> ammessa (50%)
  concentrazione massima 16/30 -> insufficiente (>50%)

source family:
  2 family -> insufficiente
  3 family -> la numerosità di ciascuna deve essere verificata separatamente

clean-safety:
  149 group_id ma un profilo a 9 -> insufficiente
  149 group_id con tutti i 7 profili >=10 -> il solo supporto è sufficiente
```

Mutation obbligatoriamente uccise:

```text
use G_eligible instead of G_defined
lower n_required after observing predictions
ignore n_power
ignore one mandatory stratum
count units/assets instead of unique group_id
turn N/A into zero or publish an unauthorized below-floor numeric value
accept a self-hashed but unexpected policy
fall back when the power plan is missing
let diagnostic p95 satisfy a gate
```

## 7. Scope e blast radius

O13F_01:

- non modifica i floor numerici esistenti;
- non decide la soglia di una metrica;
- non attiva il p95 come gate;
- non chiude O-12, O-14 o la power analysis;
- non riapre O-09;
- non modifica il candidate congelato;
- non autorizza export dal dispatcher, training, runtime o Ableton;
- non costituisce `REV8 SPEC GO`, `G1c close` o `G1 PASS`.

Un failure di supporto per una metrica obbligatoria impedisce PASS alla claim o
al gate dipendente. Un mismatch di policy/hash o una contabilità strutturalmente
invalida rende non materializzabile l'intero report.

## 8. Raccomandazione

**Raccomandazione: `APPROVO`.**

Motivo: il testo non sceglie nuovi numeri. Rende operativi e fail-closed i
floor già congelati, impedisce il passaggio post-hoc da `G_defined` a una
popolazione più favorevole e vincola l'eventuale `n_power` al proprio artefatto
preregistrato.

L'alternativa `RESPINGO` lascia il problema attuale: i conteggi O-13 sono
materializzati, ma nessun evaluator può applicare i floor senza scegliere un
dispatch non firmato.

## 9. Firma dell'autorità

Firma registrata dopo il freeze docs-only e il counter-check sul commit
immutabile del draft.

```text
Decisione O13F_01:     APPROVO
Firma/nome:            Marco
Data:                  2026-08-09

SHA-256 ballot pre-firma verificato:
f26de4f266a37b40007c8effebbc6dc048c07906b98a1d4fdda4e5eaeb81271d

Commit ballot pre-firma verificato:
45c49318f8cb8a6b9bbcb8cda29df929e5a5c563
```

`APPROVO CON MODIFICHE` non rende efficace la decisione: richiede un nuovo
draft, un nuovo SHA, un nuovo counter-check e una firma finale.

## 10. Stato dopo un'eventuale firma

Anche dopo `APPROVO`:

```text
O13F_01 authority              = FIRMATA, NON EFFICACE PENDING RECHECK
Support/p95/hierarchy code     = CANDIDATE-ONLY
Floor policy implementation   = NON MATERIALIZZATA
REV7 dispatcher               = INVARIATO
O-13 close                    = NO
REV8 SPEC GO                  = NO
G1c close / G1 PASS           = NO
Runtime / training / push     = NON AUTORIZZATI
```

Soltanto un recheck post-firma `CLEAN`, seguito da implementazione e fixture
sul commit candidate immutabile, può rendere questa authority consumabile dal
successivo gate di attivazione.
