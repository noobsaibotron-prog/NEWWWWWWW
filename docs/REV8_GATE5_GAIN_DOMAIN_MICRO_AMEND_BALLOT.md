# REV8 — GATE 5 GAIN-DOMAIN MICRO-AMEND BALLOT

**Stato:** SIGNED — APPROVED

## 1. Oggetto e autorita

Questo ballot modifica esclusivamente il dominio scientificamente eleggibile
del Gate 5 — gain invariance. Non modifica il frontend, le fixture G1
congelate, la soglia numerica, i Gate 4/6/7, l'evaluator, il plugin o il
training.

    Target candidate:
    docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md

    SHA-256 target candidate:
    398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

    Base commit:
    9c62f314

    Decisione scientifica:
    dominio eleggibile fail-closed per il Gate 5

Il Gate 5 originale richiede un confronto full-grid. Tale confronto resta
obbligatorio come diagnostica, ma non puo chiudere il gate sulle celle ferme
al floor o al ceiling: una traslazione di gain non puo essere osservata dove
il frontend ha gia applicato un clamp. Il presente ballot definisce a priori
il dominio sul quale la proprieta di gain invariance e matematicamente
osservabile.

## 2. Fixture di chiusura

Il Gate 5 usa esclusivamente:

    pseudo_noise mono, 48 kHz
    decorrelated_stereo, 48 kHz

Per entrambe:

    attenuazione preliminare esatta = -1.0 dB
    gain variants                  = -12, -6, +6, +12 dB

L'attenuazione preliminare appartiene alla fixture del gate e viene applicata
prima della variante. Qualsiasi campione della variante con:

    abs(x) >= 1.0

rende il caso invalido e produce FAIL; non e ammesso clipping, limiting,
normalizzazione adattiva o esclusione del solo campione.

## 3. Accoppiamento dei frame e validita

Reference e variante sono accoppiati soltanto quando coincidono esattamente:

    frame_index
    frame_end_sample

Frame mancanti, duplicati, riordinati o timestamp differenti producono FAIL.
Per ogni canale logico confrontato, una differenza fra i flag di validita di
reference e variante produce FAIL.

Un canale non valido in entrambi i render non fornisce celle scientifiche e
viene pubblicato con:

    CHANNEL_INVALID

Non puo essere usato per soddisfare il supporto minimo.

## 4. Dominio eleggibile PSD e shape

Per ogni frame, canale e banda canonica i, la cella PSD/shape e eleggibile se
e solo se i due valori PSD emessi, reference e variante, sono entrambi finiti
e strettamente interni a:

    (-120.0, +12.0) dB

Quindi:

    psd_ref <= -120.0 oppure psd_variant <= -120.0
      -> esclusione LOWER_CLAMP

    psd_ref >= +12.0 oppure psd_variant >= +12.0
      -> esclusione UPPER_CLAMP

    valore non finito
      -> FAIL, non esclusione silenziosa

Shape usa esattamente la stessa maschera di eleggibilita della PSD. Non e
ammesso eliminare una cella perche il suo errore shape, PSD o delta e grande.

## 5. Dominio eleggibile prominence

La prominence della banda i e eleggibile se e solo se tutte le 33 celle PSD
del kernel riflesso i-16 ... i+16 sono eleggibili secondo il §4.

La reflection e quella del frontend congelato. Per una griglia di lunghezza
n=120, ogni indice j fuori dominio viene riflesso ripetutamente con:

    j < 0  -> j = -j
    j >= n -> j = 2*n - 2 - j

finche 0 <= j < n.

Se anche una sola cella del vicinato non e eleggibile:

    PROMINENCE_NEIGHBOR_INELIGIBLE

Il kernel non puo essere abbreviato, ritagliato, sostituito con un vicinato
parziale o ricostruito dopo aver osservato i risultati.

## 6. Dominio eleggibile delta

Per un frame successivo al primo frame valido/reset, delta alla banda i e
eleggibile se e solo se la cella shape corrente e la cella shape del frame
valido precedente sono entrambe eleggibili.

In caso contrario:

    DELTA_HISTORY_INELIGIBLE

Il primo frame valido dopo start/reset conserva la norma congelata:

    delta_db[i] = 0

e viene verificato come tale. Non e ammesso saltare arbitrariamente frame
intermedi per ricostruire una storia eleggibile favorevole.

## 7. Metriche, soglia e supporto minimo

Tutte le celle eleggibili entrano nel massimo. Sono vietati sampling, p95,
media, RMSE, selezione post-hoc o esclusione guidata dall'errore.

Per ogni gain variant, frame valido e canale valido:

    shape:      max abs(variant - reference)                    <= 0.05 dB
    prominence: max abs(variant - reference)                    <= 0.05 dB
    delta:      max abs(variant - reference)                    <= 0.05 dB
    PSD:        max abs((variant - reference) - applied_gain)   <= 0.05 dB
    level:      abs((variant - reference) - applied_gain)       <= 0.05 dB

applied_gain e uno dei quattro gain variant, non include l'attenuazione
preliminare comune a reference e variante.

Supporto minimo per ciascun frame/canale valido:

    PSD eligible cells        >= 108
    shape eligible cells      >= 108
    delta eligible cells      >= 108
    prominence eligible cells >= 64

Il mancato raggiungimento di un singolo floor produce:

    INSUFFICIENT_ELIGIBLE_SUPPORT

e FAIL dell'intero Gate 5. Ridurre il dominio, alterare i clamp o riclassificare
una cella eleggibile come esclusa per ottenere PASS e vietato.

## 8. Reporting obbligatorio

Il report machine-readable pubblica, per fixture, gain, frame e canale:

- numero totale ed eleggibile di celle per campo;
- massimo errore e posizione del massimo per ogni campo;
- conteggio di ogni reason code;
- frame index, frame end sample e flag di validita;
- peak assoluto del segnale trasformato;
- verdetto per caso e verdetto complessivo.

Reason code minimi:

    LOWER_CLAMP
    UPPER_CLAMP
    PROMINENCE_NEIGHBOR_INELIGIBLE
    DELTA_HISTORY_INELIGIBLE
    CHANNEL_INVALID
    INSUFFICIENT_ELIGIBLE_SUPPORT
    NONFINITE_VALUE
    FRAME_ALIGNMENT_MISMATCH
    VALIDITY_MISMATCH
    CLIPPING_OR_FULL_SCALE

Il confronto full-grid resta obbligatorio come diagnostica separata. Deve
pubblicare i propri massimi e spiegare esplicitamente che le celle clampate
rendono la traslazione full-grid non osservabile. Un PASS full-grid non e
richiesto; un suo risultato non puo sostituire o rilassare il gate sul dominio
eleggibile.

## 9. Reject-path e mutation obbligatorie

I test devono falsificare almeno:

    vecchio confronto full-grid -> mostra il fallimento sulle celle clampate
    rimozione di una cella eleggibile con errore grande -> FAIL
    kernel prominence di 32 o meno celle -> FAIL
    supporto 107 PSD/shape/delta o 63 prominence -> FAIL
    valore esattamente -120.0 o +12.0 -> escluso, non eleggibile
    errore esattamente 0.05 dB -> PASS
    errore immediatamente sopra 0.05 dB -> FAIL
    frame/timestamp/validity mismatch -> FAIL
    abs(x) esattamente 1.0 -> FAIL
    NaN o Inf -> FAIL

La suite deve inoltre provare che aggiungere una cella valida al dominio non
puo migliorare il massimo tramite la rimozione di altre celle e che una
permutazione dell'ordine di iterazione non cambia il verdetto.

## 10. Scope della firma

La firma:

- approva soltanto il dominio eleggibile del Gate 5 definito sopra;
- autorizza un harness lab-only run_frontend_conformance() -> dict e i test
  dei Gate 5–7;
- non modifica Gate 4, Gate 6 o Gate 7;
- non autorizza modifiche al frontend, alle fixture congelate o alle soglie;
- non esporta nuovi simboli da ml_v3.contracts;
- non autorizza training, plugin, runtime prodotto o G2;
- non costituisce REV8 SPEC GO ne G1 PASS.

Il codice del gate deve vivere in:

    ml_v3/benchmark/frontend_conformance.py

e resta un entrypoint lab-only.

## 11. Firma dell'autorita

    Decisione complessiva: APPROVO
    Firma/nome: Marco
    Data: 2026-08-11

    Base dell'autorizzazione:
    approvazione esplicita del piano "chiusura frontend G1 e probe semantico V3"
    nel task Codex corrente, inclusa la scelta "dominio eleggibile".

## 12. Stato dopo la firma

    Gate 5 eligible-domain policy = FIRMATA
    Harness Gate 5-7              = AUTORIZZATO, NON ANCORA VALIDATO
    Representation probe          = AUTORIZZATO SOLO DOPO GREEN DEI GATE 5-7
    REV8 SPEC GO                  = NO
    G1 PASS                       = NO
    Training/plugin/runtime       = NON AUTORIZZATI
