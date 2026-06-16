# Il Collegio Peritale — Consulenti Tecnici d'Ufficio (CTU)

Nessun giudice del Tribunale della Veridicità decide su materia tecnica senza il parere vincolante
del CTU competente. I sette periti ereditano i ruoli ispettivi della *Audio Architect War Room*
(`REPORTS/AUDIO_ARCHITECT_WAR_ROOM_SKILL_GOLD.md`) e li convertono da *revisione* a *verifica di
veridicità*: non cercano il bug migliore da risolvere, ma stabiliscono se una **dichiarazione** è vera.

Per ogni CTU: **mandato**, **domande-tipo**, **standard di prova** (cosa accetta come prova al
Secondo Grado), **bandiere rosse** (segnali di dichiarazione fuorviante per il Terzo Grado).

---

## CTU-1 — Perito RT-Safety

**Mandato:** verificare ogni dichiarazione sulla sicurezza real-time del thread audio (`processBlock`).

**Principio non-negoziabile (legge applicabile):** nel thread audio — nessun `mutex`/`lock`,
nessuna allocazione (`new`, `malloc`, `std::vector` che cresce, `std::string`), nessun I/O,
nessun `SpinLock`, nessuna chiamata bloccante, nessun comportamento non deterministico.

**Domande-tipo:**
- Il percorso dichiarato RT-safe parte davvero da `processBlock`? (seguire la catena)
- Compaiono lock/alloc/IO lungo quel percorso?
- Esiste un `SpinLock` o `std::mutex` raggiungibile dall'audio thread?

**Standard di prova (Secondo Grado):** ispezione del percorso da `processBlock` fino all'effetto,
con assenza dimostrata di costrutti vietati; idealmente un test RT (es. RTSan / harness) verde.

**Bandiere rosse (Terzo Grado → FUORVIANTE/FALSO):** dichiarazione "lock-free"/"RT-safe" con
un `std::mutex`/`SpinLock` raggiungibile (caso storico Bug #2 e Bug #3 in
`REPORTS/CRITICAL_BUGS_ANALYSIS_2026-01-06.md`).

---

## CTU-2 — Perito Threading & Stato

**Mandato:** ownership, sincronizzazione, doppia sorgente di verità, race, semantica di restore.

**Domande-tipo:**
- La comunicazione AI→audio dichiarata "lock-free" usa davvero `SPSCQueue`/`AtomicSnapshot` (`Source/Core/LockFreeStructures.h`) sull'INTERO percorso, o solo su un tratto?
- Chi possiede lo stato condiviso? Ci sono due copie che possono divergere?
- Le scritture sono atomiche con ordering corretto (`memory_order_*`)?

**Standard di prova:** mappa di ownership del dato dichiarato + assenza di accesso concorrente non sincronizzato.

**Bandiere rosse:** "tutto lock-free" quando solo lo spectrum snapshot lo è e `correctionsWriteMutex`/`historyMutex` restano (`AIEngine.h:535,553`). Tipico overclaiming di portata.

---

## CTU-3 — Perito DSP

**Mandato:** filtri, smoothing, latenza, gain matching, denormali, anti-cramping, convoluzione.

**Domande-tipo:**
- La latenza dichiarata (es. "128 sample") è quella riportata all'host via `setLatencySamples`/`getLatencySamples`?
- Il gain del linear phase corrisponde a quello dello zero-latency (claim di parità)?
- L'enum dei tipi di filtro contiene davvero i N tipi dichiarati?

**Standard di prova:** misura (test di regressione gain/latenza, es. `LinearPhaseGainRegressionTest.cpp`) o conteggio diretto dell'enum.

**Bandiere rosse:** una costante `partSize = 128` esiste ma nessun test misura la latenza *effettiva*; un coefficiente di scaling con magic number non giustificato (caso Bug #4).

---

## CTU-4 — Perito Host & Integrazione

**Mandato:** automazione, recall di sessione, parità bounce/render, cambi di block-size e sample-rate.

**Domande-tipo:**
- I parametri dichiarati "automatizzabili" sono in APVTS e raggiungono il DSP?
- Lo stato sopravvive a save/reopen della sessione host?
- Cambi di block-size rompono il comportamento? (esiste `BlockSizeRegressionTest.cpp`, `FuzzBlockSizeTest.cpp`)

**Standard di prova:** test di integrazione stato (`IntegrationStateTest.cpp`) o regressione block-size verdi.

**Bandiere rosse:** "production-grade host integration" senza prova di parità di render o di recall.

---

## CTU-5 — Perito Stato & Preset

**Mandato:** save/load, determinismo del roundtrip, evoluzione schema, recall stabile.

**Domande-tipo:**
- `getStateInformation`/`setStateInformation` serializzano TUTTI i parametri dichiarati persistenti?
- Il roundtrip salva→carica è deterministico (stesso stato in → stesso stato out)?
- Il PresetManager (`Source/Utils/PresetManager.*`) copre il claim?

**Standard di prova:** test di roundtrip o ispezione completa del set serializzato vs set dichiarato.

**Bandiere rosse:** "salva e ripristina tutto lo stato" mentre un sottoinsieme di parametri non è nel ValueTree.

---

## CTU-6 — Perito Performance

**Mandato:** CPU, throughput, rate limiting, costo di riconfigurazione, cadenza di calcolo.

**Domande-tipo:**
- Il "rate limiting a ~30 Hz" dichiarato è davvero attivo, o è il codice commentato (caso Bug #5)?
- "Calcolato ogni 4 blocchi" — il contatore esiste e la guardia è raggiunta?
- L'analisi AI gira su thread dedicato, non sull'audio thread?

**Standard di prova:** ispezione del percorso con la guardia di rate/contatore raggiungibile e attiva; eventuale telemetria.

**Bandiere rosse:** "rate limiting attivo" con il blocco di codice commentato o flag a zero (overclaiming + percorso morto).

---

## CTU-7 — Perito di Prodotto

**Mandato:** giudica le dichiarazioni di maturità ("production-grade", "production-ready", "enterprise").

**Domande-tipo:**
- L'etichetta "production-grade" è sostenuta dai verdetti degli altri sei CTU, o è un'auto-promozione?
- Esistono blocker aperti negli ultimi report (`REPORTS/`) che contraddicono la maturità dichiarata?

**Standard di prova:** un'etichetta di maturità è `VERO PROVATO` solo se nessun CTU ha emesso `FALSO`/`FUORVIANTE` su un claim core nello stesso corpus.

**Bandiere rosse:** "production-grade" coesistente con bug critici aperti documentati negli stessi report del progetto.

---

## Tabella di Assegnazione Rapida

| Parola-chiave nella dichiarazione | CTU competente |
|-----------------------------------|----------------|
| lock-free, RT-safe, zero-allocation, no-mutex, real-time | CTU-1, CTU-2 |
| thread, atomic, race, ownership, SPSC, snapshot | CTU-2 |
| latenza, gain, filtro, slope, dB/oct, convoluzione, denormal | CTU-3 |
| automazione, host, recall, bounce, block-size, sample-rate | CTU-4 |
| preset, save, load, state, roundtrip, serializza | CTU-5 |
| CPU, performance, rate limiting, ogni N blocchi, throughput | CTU-6 |
| production-grade, production-ready, enterprise, premium | CTU-7 |

**Regola di concorso:** se una dichiarazione tocca più materie, si nominano più CTU; il verdetto
finale del capo è il **più severo** tra i pareri (non-compensazione, Art. 8 del Codice di Procedura).
