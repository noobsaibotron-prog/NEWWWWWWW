# La Scala dei Verdetti

I sei gradi di veridicità con cui il Tribunale chiude ogni capo d'imputazione, più i criteri di
graduazione (aggravanti e attenuanti). Principio guida: **non-compensazione** — un pregio non
cancella una falsità.

---

## I Sei Verdetti

### 🟢 VERO PROVATO
La dichiarazione è pienamente vera e provata.
- **Condizione:** supera tutti e tre i gradi competenti. Esiste (I), funziona come dichiarato (II), è formulata legittimamente (III).
- **Prova minima:** citazione `file:linea` + prova di comportamento (test/misura/percorso) + nessun test di legittimità fallito.
- **Esempio reale:** "OSC su porta UDP 11100, risposte su 11101" → `PluginProcessor.cpp:91`, `OSCParameterServer.h:11,161`. Esiste, è cablato, il numero è esatto.
- **Esempio reale:** "28 qualità semantiche" → `enum class SemanticQuality` con esattamente 28 voci in `SemanticEQEngine.h`. Numero verificato per conteggio.

### 🟢 VERO PARZIALE
Vero, ma su un perimetro più ristretto di quanto la dichiarazione lasci intendere — senza intento ingannevole.
- **Condizione:** il nucleo è provato, ma con una limitazione onesta (piattaforma, configurazione, sottoinsieme).
- **Tipico:** claim vero solo su una piattaforma (`#if JUCE_WINDOWS`), o vero per la maggior parte ma non per tutti i casi dichiarati.
- **Differenza da FUORVIANTE:** qui manca l'overclaiming *ingannevole*; è un'imprecisione di portata in buona fede (favor rei lessicale).
- **Esempio:** "TensorFlow Lite per inferenza ML" → vero, ma il README stesso lo dichiara opzionale e Windows-only; se un claim lo desse per universale sarebbe VERO PARZIALE.

### 🟡 NON PROVATO
Né confermato né smentito agli atti.
- **Condizione:** il fatto esiste (I superato) ma manca la prova di comportamento (II non raggiunto), OPPURE l'istruttoria è incompleta.
- **Regola:** in dubio pro veritate non probata. Non è un'assoluzione né una condanna: è una sospensione del giudizio per carenza di prova.
- **Esempio reale:** "latenza linear phase = 128 sample" → la costante `partSize = 128` esiste (`LinearPhaseProcessor.h:40`) ma nessun test in `Source/Tests/` misura la latenza riportata all'host. Esiste il numero, non la prova dell'effetto.
- **Azione raccomandata:** scrivere il test mancante converte questo verdetto in VERO PROVATO o FALSO.

### 🟠 FUORVIANTE
Vero nella lettera, falso nello spirito.
- **Condizione:** supera I e II su un *tratto*, ma fallisce un test di legittimità al III grado (overclaiming di portata, oppure termine tecnicamente improprio).
- **È il verdetto più importante del framework:** stana le dichiarazioni che "tecnicamente non mentono" ma inducono una conclusione errata.
- **Esempio reale (caso fondativo):** "architettura lock-free per il thread audio". `LockFreeStructures.h` contiene davvero `SPSCQueue`/`AtomicSnapshot`, ma `AIEngine.h:511` mantiene `std::mutex spectrumMutex` e `AIEngine.cpp` usa 15+ `lock_guard`. Vero che *parti* sono lock-free; falso che lo sia *l'architettura*. → Test di Portata fallito.

### 🔴 FALSO
Il codice contraddice la dichiarazione.
- **Condizione:** il fatto è insussistente (I fallito) oppure il comportamento è smentito (II fallito con contro-prova).
- **Esempio costruito:** "il rate limiting AI è attivo" mentre il blocco è commentato → COMPORTAMENTO SMENTITO → FALSO (era lo stato pre-fix del Bug #5).

### ⚫ FALSO AGGRAVATO
Falso con aggravanti che ne segnalano la pericolosità o la mala fede.
- **Condizione:** FALSO **più** almeno un'aggravante grave (vedi sotto), tipicamente la **falsa attribuzione** di un'azione.
- **Esempio costruito:** "ho rimosso il mutex e reso il path lock-free" quando il `std::mutex` è ancora a `AIEngine.h:511` e `git log`/`git blame` non mostrano alcuna rimozione. Falso + auto-attribuzione + contraddizione del principio RT-safety.

---

## Aggravanti (aumentano la severità di un grado)

| Aggravante | Descrizione | Effetto |
|------------|-------------|---------|
| **Falsa attribuzione** | "Ho fatto/corretto/rimosso X" non confermato da git/diff | FALSO → FALSO AGGRAVATO |
| **Violazione di principio** | Contraddice un principio non-negoziabile dell'audio RT (lock/alloc/IO nel thread audio) | FUORVIANTE → FALSO; FALSO → FALSO AGGRAVATO |
| **Recidiva** | Lo stesso tipo di claim è già stato cassato nel registro di giurisprudenza | +1 grado |
| **Auto-certificazione via commento** | La sola prova addotta è un commento `// FIXED`/`// RT-safe` | Inammissibile (Art. 5) → declassa a NON PROVATO o oltre |
| **Critical-impact** | Il claim falso riguarda sicurezza RT, stabilità o perdita dati | +1 grado |

## Attenuanti (riducono la severità di un grado)

| Attenuante | Descrizione | Effetto |
|------------|-------------|---------|
| **Buona fede lessicale** | Imprecisione di linguaggio su sostanza corretta | FUORVIANTE → VERO PARZIALE |
| **Limitazione auto-dichiarata** | La dichiarazione stessa segnala il limite ("opzionale", "Windows-only", "in sviluppo") | FUORVIANTE → VERO PARZIALE |
| **Perimetro ristretto onesto** | Vero su un sottoinsieme senza pretendere il tutto | mantiene VERO PARZIALE invece di scendere |

---

## Criteri di Sentencing (come scegliere il verdetto)

1. **Il fatto esiste?** No → `FALSO` (o `NON PROVATO` se ricerca incompleta). Sì → prosegui.
2. **Funziona come dichiarato?** Smentito → `FALSO`. Non provato → `NON PROVATO`. Provato → prosegui.
3. **Passa i 4 test di legittimità?** (non-contraddizione lessicale, portata, attribuzione, principio)
   - Tutti superati → `VERO PROVATO` (o `VERO PARZIALE` se c'è una limitazione onesta).
   - Fallisce portata/lessico senza mala fede → `FUORVIANTE`.
   - Fallisce attribuzione o principio → applica aggravante → `FALSO`/`FALSO AGGRAVATO`.
4. **Applica aggravanti e attenuanti**, una sola volta ciascuna, partendo dal verdetto base.

## Verdetto Aggregato del Corpus

Per un insieme di dichiarazioni (es. tutto il README), calcolare:
- **Indice di Veridicità** = (n. VERO PROVATO + 0.5·VERO PARZIALE) / n. totale capi.
- **Verdetto di Affidabilità della Fonte:**
  - ≥ 0.85 e nessun FALSO AGGRAVATO → `FONTE ATTENDIBILE`
  - 0.60–0.85 → `FONTE DA VERIFICARE`
  - < 0.60 oppure ≥ 1 FALSO AGGRAVATO → `FONTE INAFFIDABILE`

L'indice non compensa: un singolo `FALSO AGGRAVATO` impedisce `FONTE ATTENDIBILE` a prescindere dalla media.
