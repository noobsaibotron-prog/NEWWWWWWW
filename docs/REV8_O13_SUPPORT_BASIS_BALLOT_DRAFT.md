# REV8 — O-13 — SUPPORT BASIS BALLOT (O13F_02)

## Stato del documento

```text
Tipo                           = micro-ballot normativo docs-only
Base contract                  = MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
Base contract SHA-256          = 398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745
O13F_01 implementation base    = f277f091
Policy compiler base           = d98fc446
Decisione O13F_02              = NON FIRMATA
REV8 dispatcher activation     = NO
REV8 SPEC GO                   = NO
```

Questo ballot non modifica floor numerici, metriche, power analysis o dati.
Definisce soltanto quale popolazione deve soddisfare ciascun floor O-13.

## 1. Finding

O13F_01 ha congelato correttamente:

```text
G_eligible(metric_id, stratum_id)
G_defined(metric_id, stratum_id)
G_NA = G_eligible \ G_defined
```

e il doppio controllo eligible/defined per il supporto metrico. Lo stesso testo
stabilisce però che source-family, profili, regioni, direzioni, minuti clean e
positivi/negativi GT possono essere precondizioni del corpus separate dalla
definibilità della metrica.

Il candidate evaluator attuale applica entrambi i floor a ogni strato. Questa
regola trasforma però una precondizione di composizione del corpus in un floor
non autorizzato sulla definibilità della metrica. Per esempio, il contratto
richiede per ciascuna classe anomaly:

```text
positivi GT                 >= 30 group_id
source family              >= 3
positivi per source family >= 5 group_id
quota massima di una family <= 50%
```

Questi vincoli descrivono `G_eligible` e la sua composizione. Applicare anche
`G_defined >= 5` a ogni source family introdurrebbe una nuova stratificazione
della definibilità che il contratto non richiede. Il supporto definito della
metrica resta invece protetto dal doppio controllo sullo strato metrico.

Il `n>=10` firmato per Spearman NON è un floor di dieci `group_id`: è il numero
di coppie matched necessario dentro ciascun
`(group_id,record_family,problem_type)`. Esso decide se il valore del gruppo
entra in `G_defined`; non deve essere ricodificato come `contract_floor=10`
della policy O-13.

## 2. Decisione proposta O13F_02

Ogni strato della policy O-13 DEVE contenere:

```text
support_basis ∈ {
  "eligible_and_defined",
  "eligible_only"
}
```

Il campo e byte-authoritative, exact-key e incluso nel digest della policy.
Valori mancanti o diversi dai due letterali producono FAIL di
materializzazione. Non esiste default e non si inferisce il basis dal nome
dello strato.

Guardie fail-closed:

1. in una policy `mandatory=true`, lo strato root
   `population_kind="all_eligible_groups"` DEVE usare
   `eligible_and_defined`;
2. `eligible_only` è ammesso soltanto per una riga corpus/GT esplicitamente
   enumerata dal registry metric/stratum firmato;
3. compiler ed evaluator non possono riclassificare uno strato in base ai
   conteggi osservati;
4. policy, template e registry devono concordare esattamente sul basis; ogni
   mismatch di digest o valore è un failure di materializzazione.

### 2.1 `eligible_and_defined`

Si applicano entrambi:

```text
|G_eligible(stratum)| >= n_required
|G_defined(stratum)|  >= n_required
```

Quando esiste un ceiling rispetto al parent, si applicano entrambi:

```text
eligible_child / eligible_parent <= ceiling
defined_child  / defined_parent  <= ceiling
```

Uso: supporto primario di una metrica obbligatoria quando corpus e valori
pubblicabili devono entrambi raggiungere lo stesso floor.

### 2.2 `eligible_only`

Si applica soltanto:

```text
|G_eligible(stratum)| >= n_required
```

e, se presente, soltanto il ceiling eligible. Il conteggio defined viene
materializzato per audit ma non decide questo strato.

Uso: precondizioni di corpus/GT, incluse quando applicabili:

- source-family;
- profili;
- regioni e direzioni tonali;
- pool positivi/negativi GT;
- gruppi con un minuto clean standard eleggibile;
- composizione e copertura delle famiglie benchmark.

`eligible_only` non rende opzionale il supporto definito della metrica: esso
deve apparire in uno strato separato quando richiesto.

### 2.3 `defined_only` è escluso

Per costruzione:

```text
G_defined(stratum) ⊆ G_eligible(stratum)
```

Quindi, a parità di `n_required`, `|G_defined| >= n_required` implica già
`|G_eligible| >= n_required`. Un terzo basis `defined_only` non aggiungerebbe
potere espressivo rispetto a `eligible_and_defined`, ma amplierebbe schema,
fixture e possibilità di configurazione errata. Qualunque floor metrico
defined usa `eligible_and_defined`; eventuali condizioni interne di
pubblicabilità per gruppo determinano l'appartenenza a `G_defined` prima della
valutazione O-13.

## 3. Semantica dei risultati

I campi di esito per strato diventano tri-state:

```text
eligible_minimum_ok : true | false | null
defined_minimum_ok  : true | false | null
eligible_ceiling_ok : true | false | null
defined_ceiling_ok  : true | false | null
```

`null` significa `NOT_APPLICABLE_BY_SUPPORT_BASIS`, non dato mancante.

Matrice normativa:

| `support_basis` | eligible minimum | defined minimum | eligible ceiling | defined ceiling |
|---|---:|---:|---:|---:|
| `eligible_and_defined` | boolean | boolean | boolean | boolean |
| `eligible_only` | boolean | null | boolean | null |

Un ceiling assente produce `true` soltanto sul lato attivo; il lato non attivo
resta `null`.

Reason code:

```text
eligible active e insufficiente -> SUPPORT_ELIGIBLE_FLOOR_NOT_MET
defined active e insufficiente  -> SUPPORT_DEFINED_FLOOR_NOT_MET
strato non-root insufficiente   -> SUPPORT_STRATUM_FLOOR_NOT_MET
```

Un lato `null` non puo generare reason code né contribuire a
`support_sufficient`.

`support_sufficient=true` richiede che tutti e soli i booleani attivi di ogni
strato siano `true`. La presenza di `null` su un lato inattivo non equivale a
successo di quel lato e non può compensare un `false` altrove.

## 4. Power binding

La formula resta invariata:

```text
n_required = max(contract_floor, n_power)
```

Il risultato si applica esclusivamente ai lati attivi secondo
`support_basis`. O13F_02 non autorizza un `n_power` diverso per eligible e
defined. Se una futura metrica richiedera due numerosita power-distinte,
serviranno due strata e due binding espliciti, non un campo implicito.

## 5. Esempio normativo anomaly / Spearman

Per una metrica Spearman obbligatoria di una classe anomaly, la policy deve
poter rappresentare almeno:

```text
stratum anomaly_metric_support:<Class>
  support_basis   = eligible_and_defined
  contract_floor  = 30
  population      = gruppi GT positivi della classe

stratum source_family:<Class>:<Family>
  parent           = anomaly_metric_support:<Class>
  support_basis    = eligible_only
  contract_floor   = 5
  population       = gruppi GT positivi della classe e della family
  parent ceiling   = 1/2
```

Nello strato metrico, `G_defined` contiene soltanto i gruppi per cui rho è
pubblicabile secondo la regola firmata: almeno dieci coppie matched in ogni
matching composto rilevante, varianze non nulle, rho singleton e certificato
disponibile. Il floor O-13 resta `30 group_id`, non `10`.

Gli strata source-family verificano la composizione eleggibile del corpus; non
inventano un requisito di cinque rho definiti per family. I child non possono
contenere gruppi fuori dal parent. Una policy che omette lo strato metrico o
uno strato corpus obbligatorio non è completa.

## 6. Esempi normativi di precondizione corpus

```text
source_family:<family>          -> eligible_only
negative_profile:<profile>      -> eligible_only
clean_profile:<profile>         -> eligible_only
tonal_region_direction:<cell>   -> eligible_only
clean_standard_minute:<Class>   -> eligible_only
```

Il registry metric/stratum successivo deve dichiarare il basis per ogni riga;
questo ballot non decide ancora l'elenco completo degli ID.

## 7. Schema e migrazione candidate-only

Nessuna policy O-13 ufficiale e stata materializzata o attivata. Pertanto:

```text
support-floor-policy-2 = candidate intermedio, non attivo
support-floor-policy-3 = primo schema che include support_basis
```

Lo schema v2 deve essere rifiutato dal compiler/evaluator v3; non esiste
migrazione automatica né default `eligible_and_defined`.

## 8. Fixture e mutation obbligatorie

Prima del commit implementativo devono passare almeno:

1. `eligible_and_defined`: eligible sotto floor -> N/A/no PASS;
2. `eligible_and_defined`: defined sotto floor -> N/A/no PASS;
3. `eligible_only`: defined sotto floor non cambia l'esito dello strato;
4. Spearman 30 eligible / 30 defined -> supporto metrico sufficiente;
5. Spearman 30 eligible / 29 defined -> insufficiente;
6. un gruppo Spearman con 9 coppie matched non entra in `G_defined`; non
   modifica `contract_floor`;
7. source-family `eligible_only`: defined sotto 5 non cambia lo strato corpus;
8. ceiling eligible-only 15/30 PASS, 16/30 FAIL;
9. campo mancante, extra, `defined_only` o altro letterale -> FAIL;
10. mutation che tratta `null` come `true`, `false` o zero -> killed;
11. mutation che applica il lato non attivo -> killed;
12. mutation che converte il supporto interno Spearman `n>=10` in floor di
    dieci gruppi -> killed;
13. policy obbligatoria con root `eligible_only` -> FAIL;
14. `eligible_only` su strato non autorizzato dal registry -> FAIL;
15. mismatch del basis fra registry, template e policy -> FAIL;
16. policy schema v2 -> FAIL, nessun fallback;
17. REV7 dispatcher isolation -> invariata.

## 9. Non-decisioni

O13F_02 non decide:

- metric ID canonici;
- elenco finale degli strata;
- valori `n_power`;
- population membership;
- gate threshold;
- policy di ECE/Brier sotto floor;
- attivazione REV8.

Questi elementi restano governati dal contratto, da O13F_01 o dal successivo
metric/stratum registry.

## 10. Stop rule

Fino alla firma e al recheck:

```text
O13F_02 implementation = NON AUTORIZZATA
metric/stratum registry = NON CONGELABILE
REV8 SPEC GO            = NO
```

## 11. Ballot

```text
Decisione O13F_02: ____________________
Firma/nome:          ____________________
Data:                ____________________
```

Opzioni ammesse:

```text
APPROVO
RESPINGO
RICHIEDO MODIFICHE (con motivazione testuale)
```
