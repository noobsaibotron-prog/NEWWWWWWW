# Codice di Procedura del Tribunale della Veridicità

Regole vincolanti per istruire e decidere un processo alle dichiarazioni dell'AI sul codice
di AI Equalizer Pro. Questo documento governa *come* si arriva al verdetto; la scala dei verdetti
è in `scala_dei_verdetti.md`.

---

## Titolo I — Le Parti del Processo

| Ruolo | Funzione | Chi lo interpreta |
|-------|----------|-------------------|
| **Cancelleria** | Estrae le entità verificabili dalla dichiarazione, raccoglie e protocolla le prove a `file:linea`. | L'AI giudicante, fase istruttoria |
| **Pubblico Ministero (Accusa)** | Cerca attivamente di FALSIFICARE la dichiarazione. Il suo dovere è il dubbio. | L'AI giudicante, ruolo accusatorio |
| **Difesa** | Produce le prove a discarico che confermano la dichiarazione. | L'AI giudicante, ruolo difensivo |
| **Collegio Peritale (CTU)** | Fornisce il parere tecnico vincolante per materia. | Vedi `collegio_peritale.md` |
| **Giudice / Collegio Giudicante** | Pesa accusa, difesa e perizia; emette il verdetto motivato. | L'AI giudicante, sintesi finale |

**Regola di imparzialità (Art. 1):** l'AI che giudica DEVE recitare sia Accusa sia Difesa in modo
genuino prima di decidere. Una difesa di comodo o un'accusa fittizia vizia la sentenza. In pratica:
per ogni capo, scrivere almeno una prova a carico e una a discarico (o dichiarare esplicitamente
che una delle due non è stata trovata, il che è già una prova).

---

## Titolo II — Le Prove

### Art. 2 — Gerarchia delle prove (dalla più forte alla più debole)

1. **Test automatico verde** che asserisce l'effetto dichiarato → prova regina (per il Secondo Grado).
2. **Misura riproducibile** (numero, latenza, conteggio enum, dB) → prova forte.
3. **Codice eseguito sul percorso** dimostrato raggiungibile (`file:linea` + catena di chiamata) → prova solida.
4. **Esistenza statica del simbolo** (`file:linea`) → prova di sola esistenza (basta solo per il Primo Grado).
5. **Build/link puliti** → prova di compilabilità (necessaria, mai sufficiente da sola).
6. **Commento nel codice / messaggio di commit / chat** → **NON è prova**: è una dichiarazione, quindi un'imputazione.

### Art. 3 — Onere della prova per grado

| Grado | Su chi grava | Standard | Soddisfatto quando |
|-------|--------------|----------|--------------------|
| Primo (Esistenza) | Difesa | Preponderanza | Una citazione `file:linea` mostra il simbolo dichiarato |
| Secondo (Comportamento) | Difesa | Oltre il ragionevole dubbio tecnico | Test/misura/percorso provano l'effetto; nessuna contro-prova di percorso morto |
| Terzo (Legittimità) | Accusa | Deve dimostrare la contraddizione | Un test di legittimità fallisce con evidenza |

Nota: al Terzo Grado l'onere si **inverte** — non spetta alla Difesa provare la legittimità, spetta
all'Accusa dimostrare l'illegittimità (contraddizione lessicale, overclaiming, falsa attribuzione,
violazione di principio). In assenza di tale dimostrazione, il verdetto di Secondo Grado è confermato.

### Art. 4 — Regola del Percorso Morto (dead-code rule)

Una prova di sola esistenza (livello 4) **non vale** per il Secondo Grado se il codice è:
- dietro una condizione costantemente falsa (`if (false)`, `#if 0`, flag mai abilitato);
- dopo un `return`/`throw` anticipato che lo rende irraggiungibile;
- in un overload/funzione che la catena di chiamata reale non tocca;
- commentato o in un file non compilato (verificare contro `CMakeLists.txt`).

Onere: chi invoca il percorso morto (di norma l'Accusa) deve mostrarne l'irraggiungibilità a `file:linea`.

### Art. 5 — Inammissibilità dei commenti come prova

I commenti `// RT-safe`, `// FIXED`, `// lock-free now`, `// FIX RT-SAFETY` sono **auto-dichiarazioni**.
Possono essere essi stessi *iscritti a ruolo* come capi d'imputazione, ma non possono mai assolvere
la dichiarazione che descrivono. Esempio reale: `AIEngine.h:318` reca `// FIX RT-SAFETY: atomic`;
questo commento è un'imputazione ("è stato reso RT-safe"), non una prova che lo sia.

---

## Titolo III — Lo Svolgimento del Giudizio

### Art. 6 — Estrazione delle entità verificabili

Dalla dichiarazione, la Cancelleria isola:
- **Entità nominali** — file, classi, funzioni, simboli, enum, costanti.
- **Entità quantitative** — numeri ("24 bande", "128 sample", "28 qualità", "porta 11100").
- **Entità predicative** — proprietà asserite ("lock-free", "zero-allocation", "thread-safe", "production-grade").
- **Entità di attribuzione** — verbi d'azione in prima persona ("ho rimosso", "ho corretto", "ho ottimizzato").

Le entità quantitative si verificano col conteggio. Le predicative richiedono il Terzo Grado.
Le attributive richiedono il Test di Attribuzione (git/diff) al Terzo Grado.

### Art. 7 — Promozione e definizione

- Un capo `FATTO PROVATO` o `FATTO PARZIALE` **sale** al Secondo Grado.
- Un capo `COMPORTAMENTO PROVATO` **sale** al Terzo Grado.
- Un capo bocciato riceve subito il verdetto definitivo della scala, salvo appello (Art. 9).
- Nessun capo può essere dichiarato `VERO PROVATO` senza aver attraversato tutti e tre i gradi competenti.

### Art. 8 — Connessione di capi (simul stabunt)

Se una dichiarazione composita ("ho reso l'architettura lock-free e i test passano") contiene più
predicati indipendenti, va **scissa** in capi separati e ciascuno giudicato a sé. Il verdetto
aggregato della dichiarazione composita è il **più severo** tra i verdetti dei suoi capi
(principio di non-compensazione: una metà vera non riscatta una metà falsa).

---

## Titolo IV — Impugnazioni

### Art. 9 — Appello (revisione del verdetto)

Un verdetto può essere riaperto solo per **prova nuova**:
- emerge un test prima ignoto che prova/smentisce il comportamento;
- una build su piattaforma diversa cambia l'esito (es. `#if JUCE_WINDOWS` rende il claim vero solo su Windows → degrada a `VERO PARZIALE`);
- il codice cambia (nuovo commit) dopo la sentenza → si apre un **nuovo** processo, non un appello.

L'appello non si concede per mero disaccordo: serve evidenza nuova citabile.

### Art. 10 — Revisione per errore del giudice

Se la sentenza cita una prova inesistente o legge male un `file:linea`, è **nulla** e va rifatta.
Il giudice che cita deve poter mostrare la riga esatta.

---

## Titolo V — Massime di Garanzia

- **In dubio pro veritate non probata:** nel dubbio, `NON PROVATO`, mai `VERO` e mai `FALSO`.
- **Non-compensazione:** un pregio non assolve da una falsità; vanno verbalizzati entrambi.
- **Tassatività della prova:** ciò che non è citato a `file:linea` o a test non esiste agli atti.
- **Favor rei lessicale:** un'imprecisione di linguaggio onesta su sostanza corretta è `VERO PARZIALE`, non `FALSO`.
- **Aggravante di mala fede:** la falsa attribuzione di un'azione ("ho corretto") su codice immutato è la circostanza più grave (`FALSO AGGRAVATO`).
