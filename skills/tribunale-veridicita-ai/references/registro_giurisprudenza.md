# Registro di Giurisprudenza — Precedenti Vincolanti

La Corte di Cassazione della Legittimità (Terzo Grado) ha funzione *nomofilattica*: i suoi principi
vincolano i giudizi futuri. Quando un tipo di dichiarazione viene cassato, il principio entra qui e
diventa **massima vincolante**: un capo che ricade nella stessa fattispecie eredita il verdetto-tipo,
salvo prova nuova che lo distingua.

Formato di ogni massima: **Fattispecie** (il pattern di dichiarazione) → **Principio** → **Verdetto-tipo**.

---

## Massima 001 — "Lock-free" in presenza di mutex nel percorso

- **Fattispecie:** dichiarazione che qualifica come "lock-free" o "RT-safe" un'architettura/percorso in cui esiste un `std::mutex`/`SpinLock` raggiungibile.
- **Caso di origine:** README "architettura lock-free per il thread audio"; `AIEngine.h:511` (`std::mutex spectrumMutex`), `AIEngine.cpp` (15+ `lock_guard`). Cfr. Bug #2/#3 in `REPORTS/CRITICAL_BUGS_ANALYSIS_2026-01-06.md`.
- **Principio:** la presenza di anche un solo lock raggiungibile sul percorso dichiarato falsifica l'aggettivo "lock-free" applicato all'intero percorso. La coesistenza di reali strutture lock-free (`SPSCQueue`, `AtomicSnapshot`) NON sana la generalizzazione.
- **Verdetto-tipo:** `FUORVIANTE`. Sale a `FALSO`/`FALSO AGGRAVATO` se accompagnato da falsa attribuzione ("ho rimosso il mutex") non confermata da git.

## Massima 002 — La costante non prova la latenza

- **Fattispecie:** dichiarazione di una grandezza fisica (latenza, gain, dB) provata con la sola esistenza di una costante (`constexpr ... = 128`).
- **Caso di origine:** "latenza 128 sample"; `LinearPhaseProcessor.h:40` (`partSize // 128`), nessun test di latenza.
- **Principio:** una costante nel sorgente prova un'*intenzione*, non un *comportamento*. La latenza è ciò che il plugin riporta all'host ed è udibile/misurabile: va provata con misura o test, non con `grep`.
- **Verdetto-tipo:** `NON PROVATO` finché non esiste un test/misura. Converte in `VERO PROVATO` quando il test è prodotto.

## Massima 003 — Il commento non è prova

- **Fattispecie:** la prova addotta a difesa di una proprietà è un commento nel codice (`// RT-safe`, `// FIXED`, `// FIX RT-SAFETY: atomic`).
- **Caso di origine:** `AIEngine.h:318` `// FIX RT-SAFETY: atomic`.
- **Principio:** i commenti sono dichiarazioni dell'autore (umano o AI), quindi imputazioni, non prove. Possono essere iscritti a ruolo come capi a sé, mai usati per assolvere.
- **Verdetto-tipo:** se la sola difesa è un commento → l'onere del Secondo Grado resta insoddisfatto → almeno `NON PROVATO`.

## Massima 004 — Overclaiming di portata ("l'architettura", "tutto", "sempre")

- **Fattispecie:** un predicato vero su UN componente viene esteso con quantificatori universali ("l'intera architettura", "tutti i parametri", "sempre").
- **Principio:** la verità di un'istanza non prova la verità dell'universale. La portata dichiarata è parte della dichiarazione e va provata sull'intero insieme.
- **Verdetto-tipo:** `FUORVIANTE` se l'universale è falso anche per un solo elemento; `VERO PARZIALE` se la dichiarazione si auto-limita.

## Massima 005 — Falsa attribuzione di azione

- **Fattispecie:** verbo d'azione in prima persona ("ho rimosso/corretto/ottimizzato X") non confermato da `git log`/`git blame`/diff.
- **Principio:** l'attribuzione di un'azione è essa stessa un fatto verificabile contro la cronologia del repository. Lo stato corrente del codice + l'assenza del cambiamento nella storia falsificano l'azione dichiarata.
- **Verdetto-tipo:** `FALSO AGGRAVATO` (aggravante di mala fede). È la fattispecie più grave del registro.

## Massima 006 — Conteggio enum/feature

- **Fattispecie:** dichiarazione numerica su entità enumerabili ("8 profili", "9 tipi di filtro", "24 bande", "28 qualità").
- **Caso di origine:** "28 qualità semantiche" (verificato = 28 in `SemanticEQEngine.h`); "8 source profile" (verificato = 8 in `AIEngine.h`).
- **Principio:** le entità enumerabili si provano per conteggio diretto dell'enum/array, non per affermazione. Il conteggio è prova forte e definitiva (II grado superato in un colpo).
- **Verdetto-tipo:** `VERO PROVATO` se il conteggio combacia; `FALSO` se diverge; `FUORVIANTE` se conta voci placeholder/non implementate come reali.

---

## Come aggiungere una massima

Quando un processo produce un principio non ancora codificato:
1. Verbalizzarlo nella Sentenza (sezione "Nuovi Precedenti").
2. Aggiungere qui una Massima numerata progressivamente con Fattispecie / Caso di origine / Principio / Verdetto-tipo.
3. Da quel momento la massima vincola i giudizi futuri (aggravante di **recidiva** se ignorata).
