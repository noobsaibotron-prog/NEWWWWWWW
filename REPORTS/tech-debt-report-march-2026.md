# Tech Debt Report — AI Equalizer Pro
**Branch:** `fix/code-review-march` | **Data:** Marzo 2026
**Codebase:** ~23.900 LOC (Source/), JUCE 8.0.10, C++20, VST3/AU/Standalone

---

## Executive Summary

Il codebase è in uno stato complessivamente **buono per un progetto di questa complessità** — la ristrutturazione recente ha risolto 16 bug critici di thread safety e il sistema di build è funzionale su entrambe le piattaforme. Tuttavia esistono **5 aree di debito ad alta priorità** che, se non affrontate, possono causare crash in produzione, regressioni silenziose su macOS e stallo nello sviluppo delle feature AI. Il debito totale stimato è di **~18–22 giorni di sviluppo** per eliminarlo completamente; la tranche critica richiede **3–4 giorni**.

**Punteggio complessivo di salute del codice: 6.2 / 10**

| Categoria | Stato | Severità |
|-----------|-------|----------|
| Code Debt | 4 bug aperti + 2 God Object + dead code | 🔴 Alta |
| Architecture Debt | AIEQ-mac fork + TFLite Windows-only + NNW stub | 🔴 Alta |
| Test Debt | macOS CI senza test + 8 moduli senza coverage | 🟡 Media |
| Dependency Debt | JUCE vendored 97MB + TFLite versione opaca | 🟡 Media |
| Documentation Debt | Nessun diagramma architetturale, nessun ML runbook | 🟢 Bassa |

---

## Metodologia di Scoring

Ogni item è valutato su tre assi (1–5):
- **Impact**: quanto rallenta lo sviluppo o degrada l'esperienza utente
- **Risk**: conseguenza del non-fix (crash, data race, feature rotta)
- **Effort**: quanto lavoro richiede il fix (1 = ore, 5 = settimane) — invertito nella formula

```
Priority Score = (Impact + Risk) × (6 − Effort)
```

---

## Debiti Prioritizzati

### 🔴 CRITICO — Fix immediato

---

#### TD-01 — Bug aperti N1 + N2 (race condition audio thread + divisione per zero)
**Tipo:** Code Debt | **Score: 50** | **Effort: 1 giorno**

**N1 — `LinearPhaseProcessor.cpp`:** doppio `activeSlot.load()` non atomico rispetto l'uno all'altro. Il primo load (riga 74) verifica la validità dello slot; il secondo (riga 95, dentro il loop per-sample) recupera il puntatore al `freqDomain`. Tra i due un altro thread può chiamare `setActiveSlot()` → si legge il `freqDomain` di uno slot diverso da quello validato, potenzialmente non inizializzato.

```cpp
// PROBLEMA: due load separati non coerenti
const int currentActive = activeSlot.load(acquire);  // L74
if (!irSlots[currentActive].valid) return;
// ... loop per-sample:
const float* currentIR = irSlots[activeSlot.load(acquire)].freqDomain.data();  // L95 ← diverso!

// FIX: usare currentActive ovunque
const float* currentIR = irSlots[currentActive].freqDomain.data();
```

**N2 — `MLEngine.cpp:255`:** `det.severity = (prob - threshold) / (1.0f - threshold)` produce `+inf`/`NaN` se `adjustThresholdForContext()` restituisce `1.0f`. Il risultato NaN si propaga nelle correzioni AI e può causare comportamenti indefiniti nel motore EQ.

```cpp
// FIX
const float denom = 1.0f - threshold;
det.severity = (denom > 1e-6f) ? (prob - threshold) / denom : 1.0f;
```

**Perché urgente:** entrambi producono comportamento indefinito (UB) in condizioni raggiungibili in produzione. N1 si attiva ogni volta che si carica un IR durante la riproduzione; N2 si attiva con source profile che alzano la threshold a 1.0.

---

#### TD-02 — `AIEQ-mac/` — Fork divergente committata nel repo
**Tipo:** Architecture Debt | **Score: 36** | **Effort: 0.5 giorni**

La cartella `AIEQ-mac/` è una copia quasi-identica dell'intera Source/, con **19 file già divergenti** tra cui `AIEngine.cpp`, `LockFreeStructures.h`, `MLEngine.cpp`. Qualsiasi bug fix su `Source/` non viene automaticamente applicato ad `AIEQ-mac/`, e viceversa. I 16 bug critici fixati nel branch `fix/code-review-march` **non esistono** in `AIEQ-mac/`.

Questo non è un problema di branch — è un problema di repository. L'approccio corretto è usare CMake con target per platform e un unico sorgente, o in alternativa git submodule con override di configurazione.

**Fix:** rimuovere `AIEQ-mac/` e `AIEQ-mac.zip` dal repository, assicurarsi che `CMakeLists.txt` gestisca le differenze macOS tramite `if(APPLE)` block. Il branch `fix/code-review-march` ha già la struttura CMake corretta per macOS (build-mac.yml funziona).

---

#### TD-03 — macOS CI non esegue i test
**Tipo:** Test Debt | **Score: 40** | **Effort: 0.5 giorni**

`build-windows.yml` compila ed esegue `ctest` (DSP tests + AI tests). `build-mac.yml` **compila soltanto**, senza alcun `ctest` step. Qualsiasi regressione su macOS passa inosservata fino al test manuale.

```yaml
# Da aggiungere a build-mac.yml dopo il build step:
- name: Run tests
  run: |
    cmake --build build-mac --config Release --target AIEqualizerPro_Tests
    cmake --build build-mac --config Release --target AIEqualizerPro_AI_Tests
    cd build-mac && ctest --build-config Release --output-on-failure --timeout 60
```

**Nota collaterale:** `build-mac.yml` triggera solo su `push/PR → main`, mentre `build-windows.yml` triggera anche su `fix/**` e `feature/**`. Il branch attuale (`fix/code-review-march`) non ha mai eseguito la build macOS in CI.

---

#### TD-04 — N4 + N5 + N6 — Tre bug aperti di media severità
**Tipo:** Code / Architecture Debt | **Score: 28–35** | **Effort: 1 giorno totale**

Tre bug identificati nella review precedente non ancora risolti da Claude Code:

**N4 — `PartitionedConvolver.h:151–155`:** `chState.resize(numChannels)` dentro `process()` esegue un heap allocation sul thread audio. In DAW con strict RT scheduling può causare xrun o priority inversion.
```cpp
// FIX: sostituire resize con guard + jassertfalse
if (numChannels > chState.size()) { jassertfalse; return; }
// Il resize va fatto solo in prepare()
```

**N5 — `AIControlPanel.h:261,300`:** il timer callback (message thread) chiama `processor.getAIEngine().getDetectedGenre()` e `.getPendingCorrections()` direttamente, senza lock e senza cache atomica. Il thread AI background può modificare lo stato interno di `AIEngine` concorrentemente.

**N6 — `PluginEditor.cpp:118–127`:** nel distruttore, `stopTimer()` è chiamato **dopo** `openGLContext.detach()`. Se il timer scatta nell'intervallo (plausibile su macOS dove il timer ha granularità 15ms), accede a membri già in teardown → crash intermittente alla chiusura.
```cpp
// ORDINE CORRETTO:
stopTimer();              // 1. prima di tutto
for (auto& t : ...) t.join();
openGLContext.detach();
```

---

### 🟡 MEDIO — Pianificare nel prossimo sprint

---

#### TD-05 — TFLite: supporto Windows-only, macOS senza backend ML
**Tipo:** Architecture Debt | **Score: 18** | **Effort: 3 giorni**

`CMakeLists.txt` linka `tensorflowlite_c.lib` solo con `if(WIN32)`. Sulla build macOS `AIEQ_ENABLE_TFLITE` non è mai definita → `NeuralNetworkWrapper` compila con il backend nullo → tutto il layer di inferenza ML è silenziosamente disabilitato su macOS. TFLite distribuisce `libtensorflowlite_c.dylib` per macOS arm64/x86_64 (stessa API C).

**Fix:** aggiungere in `CMakeLists.txt`:
```cmake
if(APPLE AND EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/TFLite/lib/libtensorflowlite_c.dylib")
    target_compile_definitions(AIEqualizerPro PRIVATE AIEQ_ENABLE_TFLITE)
    target_link_libraries(AIEqualizerPro PRIVATE
        "${PROJECT_SOURCE_DIR}/ThirdParty/TFLite/lib/libtensorflowlite_c.dylib")
endif()
```

---

#### TD-06 — 8 moduli senza alcuna copertura di test
**Tipo:** Test Debt | **Score: 16** | **Effort: 5 giorni**

I seguenti moduli non hanno test unit né integration test:

| Modulo | Rischio senza test |
|--------|--------------------|
| `MLEngine` | N2 (div/0) non sarebbe stato trovato in CI |
| `AdaptiveAIEngine` | N8 (maxBin=0 silenzio) non rilevato |
| `PresetManager` | Crash su preset malformato (C16 was caught manually) |
| `CaptureService` | Race condition su sampleRate (C3/C4) non rilevata da test |
| `OnlineLearningSystem` | Checkpoint stubs non testati |
| `ReferenceMatcher` | Zero coverage su feature core |
| `SemanticEQEngine` | 1341 righe senza test |
| `NeuralNetworkWrapper` | Stubs silenti non verificati |

Priorità: MLEngine, PresetManager, CaptureService (impatto più diretto su stabilità).

---

#### TD-07 — ASAN/TSAN assenti dalla CI
**Tipo:** Infrastructure Debt | **Score: 28** | **Effort: 1 giorno**

Tutti i bug di thread safety (C1–C16, N1–N6) sono stati trovati con code review manuale. Nessun sanitizer è configurato nella pipeline. Aggiungere un job dedicato:

```yaml
- name: Build with TSAN
  run: cmake -S . -B build-tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"
       cmake --build build-tsan --target AIEqualizerPro_Tests
       ./build-tsan/Tests/AIEqualizerPro_Tests
```

Su macOS TSAN è supportato nativamente da clang. ASAN può essere aggiunto per il catch delle heap alloc out-of-bound (N4).

---

#### TD-08 — 227 parameter ID come stringhe letterali
**Tipo:** Code Debt | **Score: 20** | **Effort: 1 giorno**

I parameter ID APVTS (es. `"band1Freq"`, `"dynamicThreshold"`, `"sourceProfile"`) sono ripetuti come string literals in almeno 5 file diversi: PluginProcessor.cpp, AIControlPanel.h, DynamicEQPanel.h, BandControlPanel.h, PresetManager.cpp. Un singolo typo (già successo, visibile nel CHANGELOG 2.1.0) può silenziare un parametro senza errori di compilazione.

**Fix:** definire tutte le ID in un header `ParameterIDs.h`:
```cpp
namespace ParameterIDs {
    inline constexpr const char* band1Freq       = "band1Freq";
    inline constexpr const char* dynamicThreshold = "dynamicThreshold";
    // ...
}
```

---

### 🔵 BASSO — Backlog tecnico

---

#### TD-09 — AIEngine God Object (3.790 righe, singola classe)
**Tipo:** Architecture Debt | **Score: 7** | **Effort: 8–10 giorni**

`AIEngine.h/cpp/Advanced.cpp` totalizza 3.790 righe con una singola classe che gestisce: analisi spettrale, peak detection multi-scale, temporal consensus, source profile management, correction generation, correction merging/prioritization, genre detection, multi-track unmasking coordination, APVTS sync e snapshot lock-free. Ha 175+ metodi.

Decomposizione suggerita:
- `SpectrumAnalyzer` (già esiste parzialmente in DSP/)
- `ProblemDetector` — rileva i problemi spettrali
- `CorrectionEngine` — genera e applica le correzioni
- `GenreClassifier` — isolato e testabile separatamente
- `AIEngine` — coordinator leggero che orchestra i sotto-sistemi

Questo non va fatto prima di avere test per tutti i sotto-moduli (TD-06).

---

#### TD-10 — PluginProcessor God Object (4.270 righe)
**Tipo:** Architecture Debt | **Score: 6** | **Effort: 5 giorni**

`PluginProcessor` coordina: DSP pipeline, AI engine, IR build thread, seqlock sync, APVTS, preset management, OSC server, M/S routing, A/B comparison e phase mode switching. Classica violazione SRP in JUCE plugin, difficile da testare in isolamento.

La decomposizione più impattante a breve termine è estrarre `IRBuildCoordinator` (gestione thread di build IR, crossfade, seqlock) che è la parte più critica per la thread safety.

---

#### TD-11 — NeuralNetworkWrapper: 15 stub `return false`
**Tipo:** Code Debt | **Score: 7** | **Effort: 5 giorni**

`NeuralNetworkWrapper.cpp` implementa lo scheletro dell'integrazione con PyTorch (training, fine-tuning, model export) ma quasi tutti i metodi ritornano `false` con `// TODO`. Allo stato attuale il file è documentazione eseguibile, non codice funzionante. L'online learning del plugin non persiste né si aggiorna tra sessioni.

Se la feature non è pianificata nel roadmap a breve, è preferibile rimuovere l'interfaccia pubblica e nascondere gli stub dietro `#ifdef AIEQ_EXPERIMENTAL` per evitare aspettative false.

---

#### TD-12 — JUCE 8.0.10 vendored (97MB in repo)
**Tipo:** Dependency Debt | **Score: 16** | **Effort: 0.5 giorni**

JUCE è copiato direttamente nel repository (97MB). Ogni `git clone` trasporta questa overhead. JUCE supporta ufficialmente l'uso come git submodule.

```bash
git submodule add https://github.com/juce-framework/JUCE.git JUCE
git submodule update --init --recursive
```

Blockers: verificare che `CMakeLists.txt` usi `find_package(JUCE)` o `add_subdirectory(JUCE)` in modo compatibile con entrambi gli approcci.

---

#### TD-13 — Dead code e stubs minori
**Tipo:** Code Debt | **Score: 10–20** | **Effort: 0.5 giorni**

Raccolta di piccole pulizie:
- `pendingIRIndex` (PluginProcessor.h:466): dichiarato e resettato, mai letto → rimuovere
- `maxBin = 0` su silenzio (AdaptiveAIEngine.cpp): aggiungere `if (maxMag == 0.0f) return;` prima del calcolo della frequenza
- `std::random_device` ri-creato per ogni call in `OnlineLearningSystem::sampleFromBuffer()` (L237): promuovere a membro o statico thread_local
- `// TODO: Implement full localization` in SemanticEQEngine.cpp:1303: valutare se è nel roadmap o va rimosso

---

## Roadmap di Remediation

### Fase 1 — Stabilità (questa settimana, ~3 giorni)

| Task | Owner | Giorni |
|------|-------|--------|
| Fix N1 (LinearPhaseProcessor dual load) | Claude Code | 0.5 |
| Fix N2 (MLEngine div/0) | Claude Code | 0.25 |
| Fix N4 (PartitionedConvolver resize) | Claude Code | 0.25 |
| Fix N5 (AIControlPanel timer sync) | Claude Code | 0.5 |
| Fix N6 (PluginEditor dtor order) | Claude Code | 0.25 |
| Aggiungere ctest a build-mac.yml | Dev | 0.25 |
| Rimuovere AIEQ-mac/ dal repo | Dev | 0.5 |

**Deliverable:** zero bug critici aperti, CI verde su entrambe le piattaforme.

---

### Fase 2 — Qualità (prossimo sprint, ~5 giorni)

| Task | Giorni |
|------|--------|
| ASAN/TSAN job in CI (TD-07) | 1 |
| ParameterIDs.h — centralizzare ID APVTS (TD-08) | 1 |
| TFLite macOS support in CMake (TD-05) | 1 |
| Test per MLEngine + PresetManager + CaptureService (TD-06 parziale) | 2 |

**Deliverable:** regressions rilevate automaticamente, TFLite funzionante su macOS.

---

### Fase 3 — Architettura (1–2 mesi, parallelamente alle feature)

| Task | Giorni |
|------|--------|
| Decomposizione AIEngine in sotto-classi (TD-09) | 8–10 |
| Decomposizione PluginProcessor: estrai IRBuildCoordinator (TD-10 parziale) | 2 |
| NeuralNetworkWrapper: implementare o nascondere dietro flag (TD-11) | 3–5 |
| JUCE → git submodule (TD-12) | 0.5 |
| Dead code cleanup (TD-13) | 0.5 |
| Test coverage per tutti gli 8 moduli mancanti (TD-06 completo) | 5 |

---

## Appendice — Inventory Completo dei File

| File | LOC | Test? | Note |
|------|-----|-------|------|
| AIEngine.cpp + .h + Advanced | 3.790 | ✅ (AIEngineTest) | God Object |
| PluginProcessor.cpp + .h | 4.270 | ❌ | God Object |
| SemanticEQEngine.cpp + .h | 1.686 | ❌ | TODO band merging |
| MLEngine.cpp + .h | 1.093 | ❌ | N2 aperto |
| DynamicEQProcessor.cpp + .h | 1.040 | ✅ (DynamicEQRegressionTest) | |
| ParametricEQProcessor.cpp + .h | 1.026 | ✅ (ParametricEQTest) | |
| AdvancedSpectrumDisplay.h | 1.678 | ❌ | GUI-only |
| AIProblemPanel.h | 1.083 | ❌ | GUI-only |
| LinearPhaseProcessor.cpp + .h | ~500 | ✅ (smoke+regression) | N1 aperto |
| AdaptiveAIEngine.cpp + .h | 638 | ❌ | N8 aperto |
| UserLearning.cpp + .h | 630 | ❌ | |
| NeuralNetworkWrapper.cpp + .h | 516 | ❌ | 15 stub |
| LockFreeStructures.h | 518 | ❌ | Thread-safety critica |
| CaptureService.h | 427 | ❌ | |
| PresetManager.cpp | 338 | ❌ | |
| OnlineLearningSystem.cpp + .h | ~400 | ❌ | Checkpoint stub |
| ReferenceMatcher.cpp + .h | ~580 | ❌ | |
| PartitionedConvolver.h | 379 | ✅ (FuzzBlockSizeTest) | N4 aperto |
| PluginEditor.cpp + .h | ~500 | ❌ | N6 aperto |
| AIControlPanel.h | 410 | ❌ | N5 aperto |

---

*Report generato da analisi statica manuale + ispezione file su branch `fix/code-review-march` (marzo 2026). Scoring framework: Priority = (Impact + Risk) × (6 − Effort).*
