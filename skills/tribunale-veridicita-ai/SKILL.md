---
name: tribunale-veridicita-ai
description: >
  Framework giudiziario a tre gradi per valutare la VERIDICITÀ delle dichiarazioni dell'AI
  sulla programmazione del plugin AI Equalizer Pro (C++/JUCE). Usa questa skill quando un
  assistente AI (Claude, ChatGPT o altri) afferma di aver implementato, corretto, ottimizzato
  o garantito qualcosa nel codice — "ho reso l'architettura lock-free", "la latenza è 128 sample",
  "i test passano", "ho rimosso il bug" — e vuoi un giudizio rigoroso, provato e non compiacente
  su quanto quell'affermazione corrisponda al codice reale.
  Tre tribunali in cascata: Fatti (esistenza), Comportamento (funzione), Legittimità (coerenza).
  Non usare per: scrivere nuovo codice, fare design UI, o valutare gusto/opinioni soggettive.
---

# ⚖️ Il Tribunale della Veridicità — AIEQ Veritas Court

> **AIEQ+ Metadata**
> - Category: `verification-framework`
> - Domain (primary): Verifica forense delle dichiarazioni AI su codice C++/JUCE (plugin audio real-time)
> - Domain (excluded): Generazione di codice, design UI/UX, valutazioni soggettive, debugging in autonomia
> - Version: `1.0`
> - Governance State: `Validated` (validata su 5 dichiarazioni reali del README di AI Equalizer Pro — vedi `REPORTS/SENTENZA_VERIDICITA_2026-06-16.md`)
> - Lingua di processo: Italiano

Questa skill trasforma l'IA in un **collegio giudicante** che processa le dichiarazioni di un'altra
AI (o di sé stessa) riguardo alla programmazione di AI Equalizer Pro. Non si fida della parola:
ogni affermazione diventa un **capo d'imputazione** che deve sopravvivere a **tre gradi di giudizio**,
ciascuno con un onere della prova più severo del precedente. Una dichiarazione è "vera" solo quando
ha superato tutti i tribunali competenti, con prove citate a `file:linea`.

## Principio Fondamentale (Articolo 0)

> **Nessuna dichiarazione dell'AI è vera finché non è provata dal codice. La parola dell'AI non è prova: è un'imputazione da verificare.**

Il framework esiste per fermare due fallimenti tipici dell'assistenza AI:

1. **L'allucinazione costruttiva** — l'AI dichiara di aver implementato qualcosa che nel codice non esiste, esiste a metà, o esiste ma è disattivato.
2. **La dichiarazione fuorviante** — l'AI usa termini tecnici ("lock-free", "RT-safe", "zero-allocation", "i test passano") la cui forma contraddice la sostanza del codice (es. dichiara lock-free mentre un `std::mutex` è ancora nel percorso audio).

Il caso storico fondativo di questo tribunale è documentato in `REPORTS/CRITICAL_BUGS_ANALYSIS_2026-01-06.md`, Bug #2: una "falsa architettura lock-free". Questo framework nasce per impedire che simili dichiarazioni vengano accettate senza processo.

## Azioni Proibite (Forbidden Actions)

- Emettere un verdetto senza almeno una citazione `file:linea` come prova materiale.
- Dichiarare "VERO" una qualsiasi affermazione fermandosi al Primo Grado (l'esistenza del codice non prova che funzioni).
- Confondere la **presenza** di un simbolo con il suo **uso effettivo** nel percorso dichiarato (un mutex dichiarato e mai usato è diverso da un mutex usato nel thread audio).
- Trattare un commento nel codice (`// RT-safe`, `// FIXED`) come prova: i commenti sono dichiarazioni dell'AI, quindi imputazioni, non prove.
- Accettare "i test passano" senza identificare QUALE test copre la dichiarazione e cosa misura davvero.
- Emettere verdetti compiacenti: in caso di dubbio si emette `NON PROVATO`, mai `VERO`.
- Punire una dichiarazione sostanzialmente corretta per imprecisione lessicale minore senza graduarla (esistono i verdetti intermedi).

## I Tre Tribunali (Gradi di Giudizio)

L'ordinamento è ispirato ai tre gradi di giudizio dell'ordinamento italiano. Ogni grado risponde a una domanda diversa e applica un **onere della prova** crescente. Una dichiarazione promossa in un grado **sale** al successivo; una bocciata riceve subito il verdetto, salvo appello.

| Grado | Tribunale | Domanda | Onere della prova | Tipo di prova |
|-------|-----------|---------|-------------------|---------------|
| **I** | **Tribunale dei Fatti** (Esistenza) | Il codice dichiarato *esiste* nel repository? | Preponderanza dell'evidenza | Statica: simbolo/funzione/file a `file:linea` |
| **II** | **Corte d'Appello del Comportamento** (Funzione) | Il codice fa *davvero* ciò che è dichiarato? | Oltre il ragionevole dubbio tecnico | Dinamica: build, test, misura, esecuzione del percorso |
| **III** | **Corte di Cassazione della Legittimità** (Coerenza) | La dichiarazione è *formulata legittimamente*, senza contraddire i principi che invoca? | Legittimità piena, non contraddittorietà | Logica: confronto forma del claim ↔ sostanza del codice ↔ principi d'ingegneria |

### Primo Grado — Tribunale dei Fatti (Esistenza)

**Compito:** accertare la realtà materiale. Stabilisce se ciò che l'AI dice di aver scritto esiste davvero.

**Cosa NON decide:** se funziona, se è corretto, se è ben fatto. Solo: *c'è o non c'è.*

**Procedura:**
1. La **Cancelleria** estrae dalla dichiarazione le entità verificabili (file, classe, funzione, simbolo, costante, numero).
2. Il **Pubblico Ministero (Accusa)** cerca le contro-prove: il simbolo *non* esiste, esiste altrove, è commentato, è in un file morto.
3. La **Difesa** produce la citazione `file:linea` che dimostra l'esistenza.
4. Il **Giudice dei Fatti** confronta e verbalizza.

**Verdetti possibili al Primo Grado:**
- `FATTO PROVATO` → sale al Secondo Grado.
- `FATTO PARZIALE` → esiste parte di ciò che è dichiarato; sale con riserva, annotando il deficit.
- `FATTO INSUSSISTENTE` → il codice non esiste. Processo chiuso: verdetto finale `FALSO` (o `NON PROVATO` se la ricerca è incompleta).

> Esempio reale (vedi sentenza): claim "OSC su porta 11100, risposte su 11101" → `PluginProcessor.cpp:91` + `OSCParameterServer.h:11` → **FATTO PROVATO**.

### Secondo Grado — Corte d'Appello del Comportamento (Funzione)

**Compito:** riesaminare il fatto provato alla luce del *comportamento*. L'esistenza del codice non basta: bisogna provare che produca l'effetto dichiarato.

**Cosa NON decide:** se la dichiarazione è formulata in modo onesto (questo è il Terzo Grado). Decide: *fa ciò che dice di fare?*

**Mezzi di prova ammessi (in ordine di forza):**
1. **Test automatico** che esercita il percorso e asserisce l'effetto (`Source/Tests/*`).
2. **Misura** diretta (latenza dichiarata, gain, conteggio, throughput).
3. **Ispezione del percorso di esecuzione** — si segue la chiamata dall'ingresso all'effetto, dimostrando che il codice è raggiunto e non aggirato (no early-return, no `#if 0`, no flag disattivato).
4. **Build pulita** che dimostra che il codice compila ed è linkato (necessaria ma non sufficiente).

**Regola del percorso morto:** se il codice esiste ma è irraggiungibile (dietro un `if (false)`, una feature flag spenta, un `return` anticipato, un overload mai chiamato), il comportamento dichiarato **non è prodotto** → la Corte riforma il verdetto di Primo Grado.

**Verdetti possibili al Secondo Grado:**
- `COMPORTAMENTO PROVATO` → sale al Terzo Grado.
- `COMPORTAMENTO NON PROVATO` → esiste ma manca la prova che funzioni (es. nessun test misura la latenza dichiarata). Verdetto finale tendenziale: `NON PROVATO`.
- `COMPORTAMENTO SMENTITO` → la prova dimostra che NON fa ciò che è dichiarato. Verdetto finale: `FALSO` o `FUORVIANTE`.

> Esempio reale: claim "latenza linear phase = 128 sample" → la costante esiste (`LinearPhaseProcessor.h:40`, Primo Grado superato) ma **nessun test in `Source/Tests/` misura la latenza riportata all'host** → **COMPORTAMENTO NON PROVATO**.

### Terzo Grado — Corte di Cassazione della Legittimità (Coerenza)

**Compito:** non rigiudica i fatti. Giudica la **legittimità della dichiarazione**: la forma del claim è coerente con la sostanza del codice e con i principi d'ingegneria che il claim stesso invoca? È la corte che stana le dichiarazioni *tecnicamente fuorvianti* anche quando il codice "in qualche modo funziona".

**Test di legittimità (un fallimento = cassazione):**
1. **Test di non-contraddizione lessicale** — il termine tecnico usato è compatibile col codice? ("lock-free" è incompatibile con un `std::mutex` nel percorso dichiarato; "zero-allocation" con un `std::vector` che cresce in `processBlock`).
2. **Test di portata (overclaiming)** — la dichiarazione generalizza oltre l'evidenza? ("l'architettura è lock-free" quando solo *un* percorso lo è).
3. **Test di attribuzione** — l'AI si attribuisce un'azione ("ho rimosso", "ho corretto") che il codice/git non conferma?
4. **Test di principio** — il claim rispetta i Principi Non-Negoziabili dell'audio real-time (nessun mutex/IO/allocazione/lock nel thread audio — vedi `REPORTS/AUDIO_ARCHITECT_WAR_ROOM_SKILL_GOLD.md`)?

**Funzione nomofilattica (la giurisprudenza):** la Cassazione fissa il **precedente vincolante**. Una volta che un tipo di dichiarazione è cassato (es. "lock-free in presenza di mutex nel path → FUORVIANTE"), il principio entra nel `registro_giurisprudenza.md` e vincola i giudizi futuri.

**Verdetti possibili al Terzo Grado:** confermano o riformano in `VERO PROVATO`, `VERO PARZIALE`, `FUORVIANTE`, `FALSO`, o `FALSO AGGRAVATO` (vedi scala dei verdetti).

> Esempio reale: claim README "architettura lock-free per il thread audio" + "parametri lock-free". `Source/Core/LockFreeStructures.h` contiene davvero `SPSCQueue`/`AtomicSnapshot` (sostanza reale), MA `AIEngine.h:511` dichiara ancora `std::mutex spectrumMutex` e `AIEngine.cpp` usa 15+ `lock_guard`. Il Primo e Secondo Grado provano che *parti* del sistema sono lock-free; la Cassazione applica il Test di Portata e di Non-Contraddizione → **FUORVIANTE** (vero in parte, generalizzato oltre l'evidenza).

## Il Collegio Peritale (Consulenti Tecnici d'Ufficio)

Nessun giudice decide su materia tecnica senza il parere di un perito. I CTU sono i sette ispettori
ereditati dalla War Room (`REPORTS/AUDIO_ARCHITECT_WAR_ROOM_SKILL_GOLD.md`), ciascuno competente per
materia. Il dettaglio di mandato, domande-tipo e standard di prova è in `references/collegio_peritale.md`.

| CTU | Materia di competenza | Esempio di dichiarazione su cui è chiamato |
|-----|----------------------|---------------------------------------------|
| Perito RT-Safety | Sicurezza real-time del thread audio | "Nessuna allocazione/lock nel processBlock" |
| Perito Threading & Stato | Ownership, sincronizzazione, race | "Comunicazione lock-free tra AI e audio" |
| Perito DSP | Filtri, smoothing, latenza, gain, denormali | "Latenza 128 sample", "anti-cramping a 48 dB/oct" |
| Perito Host & Integrazione | Automazione, recall, bounce parity | "I parametri sono automatizzabili dall'host" |
| Perito Stato & Preset | Save/load, determinismo del roundtrip | "Il preset salva e ripristina tutto lo stato" |
| Perito Performance | CPU, throughput, rate limiting | "Rate limiting a 30 Hz", "calcolato ogni 4 blocchi" |
| Perito di Prodotto | Maturità per release commerciale | "Production-grade", "production-ready" |

## Procedura Canonica (Workflow Operativo)

Per processare un set di dichiarazioni, seguire le sei fasi. Il dettaglio procedurale completo
(regole sulle prove, oneri, ammissibilità) è in `references/codice_di_procedura.md`.

### Fase 1 — Iscrizione a Ruolo (Capi d'Imputazione)
Raccogliere le dichiarazioni da processare e numerarle. Le fonti tipiche:
- Il `README.md` (le dichiarazioni "ufficiali" sul prodotto).
- I messaggi di chat dell'AI ("ho fatto X", "ora è Y").
- I commenti nel codice che asseriscono proprietà (`// RT-safe`, `// FIXED`, `// lock-free`).
- I messaggi di commit git ("fix:", "feat:").

Ogni dichiarazione diventa un **Capo d'Imputazione** con: testo letterale, fonte, CTU competente.

### Fase 2 — Istruttoria (Raccolta Prove)
La Cancelleria raccoglie le prove a `file:linea` con gli strumenti del repo (Grep/Glob/Read, build, test). Ogni prova è etichettata: **a carico** (smentisce) o **a discarico** (conferma).

### Fase 3 — Primo Grado (Esistenza)
Per ogni capo: esiste? Verdetto di Primo Grado. I `FATTO INSUSSISTENTE` chiudono qui.

### Fase 4 — Secondo Grado (Comportamento)
Per i capi promossi: funziona come dichiarato? Build/test/misura/percorso. Verdetto di Secondo Grado.

### Fase 5 — Terzo Grado (Legittimità)
Per i capi promossi: la formulazione è legittima? Applicare i quattro test di legittimità e la giurisprudenza esistente. Verdetto definitivo.

### Fase 6 — Sentenza
Emettere la **Sentenza** secondo `references/template_sentenza.md`: dispositivo (verdetto per capo), motivazione (prove citate), eventuali nuovi precedenti per il registro, e un **Giudizio di Veridicità Aggregato** sul corpus di dichiarazioni.

## La Scala dei Verdetti

Sei gradi di veridicità (dettaglio, criteri e aggravanti/attenuanti in `references/scala_dei_verdetti.md`):

| Verdetto | Significato | Condizione |
|----------|-------------|------------|
| 🟢 `VERO PROVATO` | Pienamente vero e provato | Supera tutti e tre i gradi |
| 🟢 `VERO PARZIALE` | Vero ma incompleto/limitato | Vero su un perimetro più ristretto di quanto dichiarato, senza inganno |
| 🟡 `NON PROVATO` | Né confermato né smentito | Esiste ma manca prova di comportamento; oppure ricerca insufficiente |
| 🟠 `FUORVIANTE` | Vero nella lettera, falso nello spirito | Generalizza oltre l'evidenza o usa un termine tecnicamente improprio |
| 🔴 `FALSO` | Smentito dal codice | Il codice contraddice la dichiarazione |
| ⚫ `FALSO AGGRAVATO` | Falso + contraddizione di principio + auto-attribuzione | Es. "ho reso lock-free" mentre il mutex è nel path audio e git non mostra la rimozione |

## Standard di Tono

- Severo ma onesto; non compensatorio (un grande pregio non assolve da una falsità).
- Tecnicamente esplicito: ogni affermazione del giudice è ancorata a `file:linea` o a un test.
- Distingue il **fatto provato** dalla **forte inferenza** e dal **non provato**.
- Garantista: nel dubbio, `NON PROVATO`, mai `VERO`. Ma anche: nessuna assoluzione per dichiarazioni fuorvianti solo perché "il codice in qualche modo gira".
- Orientato alla **fiducia di prodotto**: lo scopo non è umiliare l'AI, ma produrre un giudizio che sopravviva al contatto con host reali, DAW reali e utenti paganti.

## Riferimenti Bundle

- `references/codice_di_procedura.md` — Codice di Procedura: regole su prove, oneri, ammissibilità, ricusazione, appello.
- `references/collegio_peritale.md` — I sette CTU: mandato, domande-tipo, standard di prova per materia.
- `references/scala_dei_verdetti.md` — Scala dei verdetti, aggravanti, attenuanti, criteri di sentencing.
- `references/registro_giurisprudenza.md` — Registro dei precedenti vincolanti, già popolato con casi reali di AIEQ Pro.
- `references/template_sentenza.md` — Template della Sentenza + esempio lavorato.

## Companion File

- `REPORTS/SENTENZA_VERIDICITA_YYYY-MM-DD.md` — la sentenza emessa su un dato corpus di dichiarazioni.

## Criteri di Promozione (a v2.0)

La skill sale a v2.0 quando:
- È stata applicata a un corpus di ≥ 20 dichiarazioni reali con sentenza motivata.
- Almeno un nuovo precedente vincolante è stato aggiunto al registro a partire da un caso non previsto.
- Un giudizio di Secondo Grado è stato chiuso con prova da test automatico *scritto ad hoc* (non solo test preesistenti).
