# AIEQ+ Comprehensive Technical Benchmark Report
**Data**: 04 Aprile 2026
**Autore**: Manus AI (Alta Corte v5.1)
**Stato**: RELEASE-CANDIDATE (Pending Final Verification)

## 1. Executive Summary

Il presente documento fornisce una verifica numerica rigorosa e comparativa di tutte le affermazioni tecniche riguardanti **AIEQ Pro**, confrontandole con i leader di mercato (FabFilter Pro-Q 4, Sonible smart:EQ 4, iZotope Ozone 11, Soundtheory Gullfoss, TDR Nova GE, Kirchhoff-EQ). 

L'obiettivo è garantire **trasparenza assoluta** e **onestà commerciale**, rimuovendo qualsiasi *overclaim* non supportato da dati misurabili.

## 2. Analisi Comparativa dei Parametri Numerici

### 2.1 Latenza (PDC - Plugin Delay Compensation)

La latenza è un parametro critico per l'uso in fase di tracking e live mixing.

| Plugin | Zero Latency Mode | Linear Phase Mode (Max) | Note |
|---|---|---|---|
| **FabFilter Pro-Q 4** | 0 samples (0 ms) | 66.560 samples (~1.509 ms) | Raddoppia con L/R e M/S simultanei [1] |
| **Sonible smart:EQ 4** | 0 samples (0 ms) | ~3.087 samples (~70 ms) | Due modalità distinte [2] |
| **Soundtheory Gullfoss** | ~88 samples (2 ms) | N/A (20 ms standard) | Versione Live ottimizzata [3] |
| **AIEQ Pro** | **NON PRESENTE** | ~220 samples (~5 ms) | Modalità unica [4] |

**Verdetto AIEQ Pro**: Il claim sulla latenza deve essere rivisto. AIEQ Pro *non* offre una modalità Zero Latency, ponendolo in svantaggio rispetto a FabFilter e Sonible per l'uso dal vivo. La latenza di ~5 ms è ottima per il mixing, ma non competitiva per il tracking.

### 2.2 Gestione M/S e Crossfade

AIEQ Pro implementa un crossfade di 1024 campioni per le transizioni M/S.

| Plugin | Approccio M/S | Crossfade Necessario? |
|---|---|---|
| **FabFilter Pro-Q 4** | Per-band stereo placement | NO (Nessun cambio di stato globale) [5] |
| **Kirchhoff-EQ** | Per-band processing | NO [6] |
| **TDR Nova GE** | Parallel dynamic EQ | NO [7] |
| **AIEQ Pro** | **Switch Globale (Stereo ↔ M/S)** | **SÌ (1024 campioni)** |

**Verdetto AIEQ Pro**: Il crossfade di 1024 campioni è una soluzione ingegneristica eccellente a un problema architetturale (il cambio globale di modalità). Tuttavia, i competitor premium usano un design *per-banda* che non richiede crossfade. **Claim "superiore ai competitor" rimosso.**

### 2.3 Architettura GUI e Rendering

La fluidità dell'interfaccia è fondamentale per l'esperienza utente.

| Plugin | Engine Grafico | Note Performance |
|---|---|---|
| **FabFilter Pro-Q 4** | Custom GPU Engine (Non-JUCE) | Eccellente, accelerazione nativa dal 2012 [8] |
| **Sonible smart:EQ 4** | JUCE Software Renderer | Buona, focus sull'AI [9] |
| **AIEQ Pro** | **JUCE OpenGL + SpinLock** | **Ottimizzato con SpinLock, ma a rischio su macOS (OpenGL deprecato)** |

**Verdetto AIEQ Pro**: L'implementazione di `juce::SpinLock` per la sincronizzazione del buffer è tecnicamente valida e risolve problemi di tearing. Tuttavia, l'uso di OpenGL in JUCE è considerato rischioso (deprecato su Mac, problematico su Windows). Il punteggio "8.9/10" è corretto per un plugin JUCE, ma non eguaglia gli engine custom.

### 2.4 Capacità Dynamic EQ

| Parametro | FabFilter Pro-Q 4 | Kirchhoff-EQ | AIEQ Pro |
|---|---|---|---|
| **Max Bande Dinamiche** | 24 | 32 | *Da verificare nel codice* |
| **Dynamic Range** | ±30 dB | Variabile | *Da verificare nel codice* |
| **Auto Attack/Release** | Sì (Program-dependent) | Sì | *Da verificare nel codice* |
| **Spectral Dynamics** | Sì | No | **No** |

**Verdetto AIEQ Pro**: I parametri esatti del Dynamic EQ di AIEQ Pro devono essere estratti dal codice sorgente prima di poter fare affermazioni comparative. FabFilter rimane il benchmark assoluto.

### 2.5 Stabilità e Validazione (pluginval)

`pluginval` è lo standard industriale per testare la stabilità dei plugin [10].

| Livello pluginval | Significato | Stato AIEQ Pro |
|---|---|---|
| **Level 5** | Minimo per compatibilità host (State restoration, Bus layout) | *Da eseguire* |
| **Level 9** | Real-time safety check (Zero allocazioni in audio thread) | *Da eseguire* |
| **Level 10** | Fuzz testing, Extreme blocks, Tutti i test precedenti | **NON TESTATO** |

**Verdetto AIEQ Pro**: Il claim "Zero-Crash" era un **overclaim non verificato**. AIEQ Pro *deve* superare pluginval Level 5 per il rilascio commerciale. Superare il Level 9/10 sarebbe un forte differenziatore tecnico.

### 2.6 Intelligenza Artificiale e Machine Learning

L'uso del termine "AI" nel mercato dei plugin.

| Plugin | Tipo di Tecnologia | Latenza/Inference |
|---|---|---|
| **Gullfoss** | Modello Percettivo Uditivo (1000 update/sec) | 2-20 ms [11] |
| **Sonible smart:EQ 4** | Smart:filter con profili (Machine Learning) | 70 ms (Linear Phase) [12] |
| **iZotope Ozone 11** | Analisi One-Shot (Machine Learning) | N/A (Offline analysis) [13] |
| **AIEQ Pro** | **Atomic Variables per Thread-Safety** | **Nessun Modello ML/DL** |

**Verdetto AIEQ Pro**: AIEQ Pro **NON contiene alcun modello di Machine Learning o Deep Learning**. L'uso del termine "AI" si riferisce a un'architettura thread-safe (Atomics, Lock-free). Il claim "98.5% AI Precision" era un **falso tecnico** ed è stato rimosso. Il nome "AI Equalizer" richiede un disclaimer chiaro per evitare false aspettative.

## 3. Conclusioni e Raccomandazioni per il Lancio

1. **Rimuovere ogni riferimento a Machine Learning**: AIEQ Pro è un eccellente EQ algoritmico con architettura lock-free, non uno strumento di intelligenza artificiale generativa o predittiva.
2. **Evidenziare i veri punti di forza**:
   - Sicurezza real-time garantita da variabili atomiche.
   - Sincronizzazione GUI fluida tramite `juce::SpinLock`.
   - Crossfade M/S a 1024 campioni per transizioni senza click (nel contesto del suo design globale).
3. **Eseguire pluginval**: Il rilascio deve essere subordinato al superamento di pluginval Level 5 (minimo) o Level 10 (ideale).

---

## Riferimenti

[1] FabFilter Pro-Q 4 Processing Mode: https://www.fabfilter.com/help/pro-q/using/processingmode
[2] Sonible smart:EQ 4 Manual: https://sonible.com/wp-content/uploads/2023/12/manual-sonible-smartEQ4_EN.pdf
[3] Soundtheory Gullfoss Specs: https://www.soundtheory.com/gullfoss
[4] AIEQ_DEEP_VERIFICATION_REPORT.md
[5] FabFilter Pro-Q 4 Stereo Options: https://www.fabfilter.com/help/pro-q/using/stereo
[6] Three-Body Technology Kirchhoff-EQ: https://www.threebodytech.com/en/products/kirchhoffeq
[7] TDR Nova GE Manual: https://www.tokyodawn.net/labs/NovaGE/Manual.pdf
[8] FabFilter Forum / News
[9] Sonible smart:EQ 4 Specifications: https://www.sonible.com/smarteq4/
[10] Tracktion pluginval GitHub: https://github.com/Tracktion/pluginval
[11] Gullfoss Operation Manual: https://www.soundtheory.com/static/Gullfoss%20Operation%20Manual.pdf
[12] Sonible smart:EQ 4 Features: https://www.sonible.com/smarteq4/
[13] iZotope Ozone 11 Learn: https://www.izotope.com/en/learn/ai-mastering
