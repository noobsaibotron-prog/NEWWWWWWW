# SENTENZA DI VERIDICITÀ N. 001/2026

**Tribunale della Veridicità — AIEQ Veritas Court**
**Data:** 2026-06-16
**Corpus processato:** `README.md` — dichiarazioni di prodotto su AI Equalizer Pro
**Fonte delle dichiarazioni:** README.md (dichiarazioni "ufficiali" generate con assistenza AI)
**Giudice estensore:** Collegio Giudicante del Tribunale della Veridicità
**Framework applicato:** `skills/tribunale-veridicita-ai/` v1.0

Questa è la sentenza inaugurale che valida il framework su dichiarazioni reali. Cinque capi
selezionati per coprire l'intera scala dei verdetti.

---

## Dispositivo (sintesi dei verdetti)

| # | Capo d'imputazione (dichiarazione) | CTU | I° | II° | III° | VERDETTO |
|---|-----------------------------------|-----|----|----|------|----------|
| 1 | "Server OSC integrato (porta UDP 11100, risposte su 11101)" | CTU-4 | ✅ | ✅ | ✅ | 🟢 VERO PROVATO |
| 2 | "28 qualità semantiche supportate" | CTU-3 | ✅ | ✅ | ✅ | 🟢 VERO PROVATO |
| 3 | "8 profili ottimizzati per diverse sorgenti" | CTU-3 | ✅ | ✅ | ✅ | 🟢 VERO PROVATO |
| 4 | "latenza di soli 128 sample anziché 4096" | CTU-3 | ✅ | ⚠️ | — | 🟡 NON PROVATO |
| 5 | "architettura lock-free per il thread audio" | CTU-1, CTU-2 | ✅ | ⚠️ | ❌ | 🟠 FUORVIANTE |

**Indice di Veridicità del corpus:** (3 VERO PROVATO + 0.5·0 VERO PARZIALE) / 5 = **0.60**
**Verdetto di Affidabilità della Fonte:** **FONTE DA VERIFICARE** (0.60, nessun FALSO AGGRAVATO)

---

## Motivazione

### Capo N.1 — "Server OSC integrato (porta UDP 11100, risposte su 11101)"
- **Fonte:** `README.md:74-75`
- **CTU competente:** CTU-4 (Host & Integrazione)
- **Primo Grado (Esistenza):** `FATTO PROVATO` — `Source/PluginProcessor.cpp:91` costruisce `OSCParameterServer(apvts, 11100)`; la classe esiste in `Source/Core/OSCParameterServer.h:23`.
- **Secondo Grado (Comportamento):** `COMPORTAMENTO PROVATO` — `OSCParameterServer.h:10-11` documenta il listening su 11100 e `:161` invia la risposta sulla porta 11101. Il server è istanziato e cablato nel processore, non è codice morto.
- **Terzo Grado (Legittimità):** `CONFERMATO` — conteggio numerico esatto (Massima 006). Nessun overclaiming: i numeri 11100/11101 combaciano alla cifra.
- **Prove a discarico (Difesa):** `PluginProcessor.cpp:91`, `OSCParameterServer.h:11,161`.
- **Prove a carico (Accusa):** nessuna trovata; il numero di porta e di risposta corrispondono.
- **VERDETTO: 🟢 VERO PROVATO** — la dichiarazione è esatta alla cifra.

### Capo N.2 — "28 qualità semantiche supportate"
- **Fonte:** `README.md:47`
- **CTU competente:** CTU-3 (DSP / semantica)
- **Primo Grado:** `FATTO PROVATO` — `enum class SemanticQuality` esiste in `Source/AI/SemanticEQEngine.h:38`.
- **Secondo Grado:** `COMPORTAMENTO PROVATO` — conteggio diretto dell'enum: esattamente 28 voci (Air, Brilliance, Presence, Sizzle, Warmth, Body, Thickness, Richness, Punch, Bite, Snap, Attack, Smoothness, Softness, Darkness, Gentleness, Weight, Foundation, Rumble, Tightness, Clarity, Definition, Focus, Width, Vintage, Modern, Aggressive, Smooth). `numQualities` è derivato da `NumQualities` (`:85`).
- **Terzo Grado:** `CONFERMATO` — Massima 006 (conteggio enum): il numero combacia esattamente.
- **Prove a discarico:** `SemanticEQEngine.h:38-85`.
- **VERDETTO: 🟢 VERO PROVATO** — 28 dichiarate, 28 nell'enum.

### Capo N.3 — "8 profili ottimizzati per diverse sorgenti: Generic, Vocals, Drums, Bass, Synth, Master, EDM e Techno"
- **Fonte:** `README.md:43`
- **CTU competente:** CTU-3
- **Primo Grado:** `FATTO PROVATO` — `enum class SourceProfile` in `Source/AI/AIEngine.h:75`.
- **Secondo Grado:** `COMPORTAMENTO PROVATO` — conteggio diretto: esattamente 8 voci (Generic, Vocals, Drums, Bass, Synth, Master, EDM, Techno), nello stesso ordine e con gli stessi nomi dichiarati. `setSourceProfile`/`getSourceProfile` (`:153-154`) li espongono al runtime.
- **Terzo Grado:** `CONFERMATO` — Massima 006. Da notare: un commento interno a `AIEngine.h:23` elenca solo 6 profili; tuttavia l'enum (fonte di verità) ne conta 8. Il commento è inammissibile come prova (Massima 003) e non inficia il claim, che è corretto.
- **Prove a discarico:** `AIEngine.h:75-84`.
- **VERDETTO: 🟢 VERO PROVATO** — 8 dichiarati, 8 nell'enum, nomi corrispondenti.

### Capo N.4 — "latenza di soli 128 sample anziché 4096 del classico OLA"
- **Fonte:** `README.md:27`
- **CTU competente:** CTU-3 (DSP)
- **Primo Grado:** `FATTO PROVATO` — `Source/DSP/LinearPhaseProcessor.h:40` definisce `static constexpr size_t partSize = PartitionedConvolver::partSize; // 128`. La costante esiste.
- **Secondo Grado:** `COMPORTAMENTO NON PROVATO` — applicata la **Massima 002** (la costante non prova la latenza). La directory `Source/Tests/` contiene `LinearPhaseGainRegressionTest.cpp` e `LinearPhaseIRSmokeTest.cpp`, ma **nessun test misura la latenza riportata all'host** (`getLatencySamples`/`setLatencySamples`). L'esistenza della costante prova un'*intenzione di design*, non l'*effetto udibile/misurabile*.
- **Terzo Grado:** non raggiunto (manca la prova di comportamento).
- **Prove a discarico:** `LinearPhaseProcessor.h:40` (esistenza).
- **Prove a carico:** assenza di un test di latenza in `Source/Tests/`.
- **VERDETTO: 🟡 NON PROVATO** — il numero esiste nel sorgente ma manca la prova che la latenza effettiva riportata all'host sia 128.
- **Conversione possibile:** scrivere `LinearPhaseLatencyTest.cpp` che asserisca la latenza riportata = 128 converte questo verdetto in VERO PROVATO.

### Capo N.5 — "architettura lock-free per il thread audio" (+ "I parametri sono lock-free")
- **Fonte:** `README.md:5`, `README.md:19`
- **CTU competente:** CTU-1 (RT-Safety), CTU-2 (Threading & Stato)
- **Primo Grado:** `FATTO PROVATO` — le strutture lock-free esistono davvero: `Source/Core/LockFreeStructures.h:128` (`SPSCQueue`, lock-free SPSC), `:35` (`AtomicSnapshot`, triple-buffer atomico).
- **Secondo Grado:** `COMPORTAMENTO PROVATO SOLO PARZIALMENTE` — i parametri EQ usano version counter atomici e lo spectrum ha un percorso a snapshot atomico; ma `Source/AI/AIEngine.cpp:206` acquisisce `std::lock_guard<std::mutex> lock(spectrumMutex)`, e l'AIEngine usa 15+ `lock_guard` (`AIEngine.cpp:546,562,586,607,650,791,…`).
- **Terzo Grado (Legittimità):** `CASSATO` — falliti due test di legittimità:
  - **Test di Non-Contraddizione lessicale (Massima 001):** `AIEngine.h:511` mantiene `mutable std::mutex spectrumMutex`; `:535` `correctionsWriteMutex`; `:553` `historyMutex`. L'aggettivo "lock-free" è incompatibile con un mutex raggiungibile sul percorso AI↔dato condiviso.
  - **Test di Portata / overclaiming (Massima 004):** il claim estende a "l'architettura" e "i parametri" una proprietà vera solo su *alcuni* percorsi.
- **Prove a carico (Accusa):** `AIEngine.h:511,535,553`; `AIEngine.cpp:206,546,650`.
- **Prove a discarico (Difesa):** `LockFreeStructures.h:35,128` — le strutture esistono e parte del path è genuinamente lock-free.
- **Attenuante applicata:** buona fede tecnica (no falsa attribuzione: il README non dichiara "ho rimosso il mutex"; se lo facesse, scatterebbe la Massima 005 → FALSO AGGRAVATO).
- **Aggravante NON applicata:** nessuna falsa attribuzione di azione riscontrata nel README.
- **VERDETTO: 🟠 FUORVIANTE** — vero che esistono percorsi lock-free e strutture dedicate; falso che lo sia *l'architettura* del thread audio nel suo complesso. Coerente con il caso storico Bug #2 (`REPORTS/CRITICAL_BUGS_ANALYSIS_2026-01-06.md`).

---

## Nuovi Precedenti (per il Registro di Giurisprudenza)

Nessuna massima nuova: i cinque capi ricadono tutti in massime già codificate
(001, 002, 003, 004, 006). Il registro `references/registro_giurisprudenza.md` è confermato adeguato
al corpus README. La Massima 006 (conteggio enum) si è dimostrata la più produttiva (3 verdetti su 5).

---

## Giudizio di Veridicità Aggregato

Il README di AI Equalizer Pro è una **FONTE DA VERIFICARE** (indice 0.60). Le dichiarazioni
**numeriche e enumerabili** (porte OSC, conteggio qualità, conteggio profili) sono accurate alla
cifra e reggono tutti e tre i gradi: dove l'AI ha potuto contare, ha detto il vero. Le debolezze
emergono su due fronti: (a) le dichiarazioni di **grandezze fisiche** (latenza 128) restano
intenzioni di design non ancora provate da test — `NON PROVATO`, non falso; (b) le dichiarazioni di
**proprietà architetturali** ("lock-free per il thread audio") soffrono di overclaiming di portata —
veri su singoli percorsi, generalizzati all'intero sistema mentre tre mutex restano nell'AIEngine.

Non si riscontra mala fede: nessuna falsa attribuzione di azione, nessun FALSO AGGRAVATO. Il profilo
è quello di una documentazione *ottimista ma non mendace*, che andrebbe calibrata sulla sostanza.

---

## Azioni Raccomandate (non vincolanti)

1. **Capo 4 → convertire in VERO PROVATO:** scrivere `Source/Tests/LinearPhaseLatencyTest.cpp` che asserisca `getLatencySamples() == 128` in modalità Linear Phase.
2. **Capo 5 → correggere il README:** sostituire "architettura lock-free per il thread audio" con una formulazione di portata onesta, es. *"percorso parametri e spectrum lock-free (SPSC + snapshot atomico); l'AIEngine usa ancora mutex su scritture non-RT"*. In alternativa, completare la migrazione lock-free di `AIEngine` (rimuovere i tre mutex) per rendere il claim VERO PROVATO.
3. **Igiene documentale:** allineare il commento `AIEngine.h:23` (elenca 6 profili) all'enum reale (8), per evitare futuri capi d'imputazione sui commenti.
