# MEGA — Mappa di occupancy semantica (AI Equalizer Pro)

**Data:** 2026-08-31  
**Albero:** GitHub `noobsaibotron-prog/NEWWWWWWW`, branch `cursor/semantic-intent-map-brainstorming-895c`  
**HEAD di partenza:** `ef67a844` (`origin/main`)  
**Non è** il freeze Ember Core (`/private/tmp/ember-semantic-intent-map`, `exp/semantic-intent-map`). Quei path/branch **non esistono** su questa macchina. L’utente ha chiesto esplicitamente di lavorare sul branch agente attivo.

**Codice di produzione:** nessuno in questo loop. Solo documenti.

**Spec:** `docs/superpowers/brainstorm-2026-08-31-2h/SPEC-semantic-occupancy.md`  
**Piano TDD:** `docs/superpowers/brainstorm-2026-08-31-2h/PLAN-semantic-occupancy.md`

---

## 1. Narrazione del processo (8 slot)

Avvio effettivo (follow-up utente, non le 21:00 di Roma): **2026-08-31 08:02:22 CEST** (`1788156142`). Fine prevista: **10:02:22 CEST** (`1788163342`).

Skill: `loop` e `brainstorming` e `writing-plans` **assenti** su questa Cloud VM. Cadenza con attese (`AwaitShell`). **Nessun** cron 21:00 armato da qui. Assist graph pin **resta parcheggiato**.

| Slot | Ora CEST | Attesa precedente | Decisione | Titolo |
|---|---|---|---|---|
| R1 | 08:04:49 | — | prima proposta | Occupancy 1:N, no-clobber |
| R2 | 08:21:34 | ~16.5 min | evolve | Complementary **non** è del parent; skip in apply |
| R3 | 08:37:47 | ~16.2 min | evolve | Proiettore di tutto lo `SemanticState`; **niente** undo live |
| R4 | 08:54:13 | ~16.4 min | evolve | Persistere **qualità + occupancy**; altrimenti il recall si autodistrugge |
| R5 | 09:13:27 | ~19.2 min | evolve | `project()` puro nei test DSP; **non** bootare `PluginProcessor` |
| R6 | 09:25:03 | ~11.6 min (tagliata) | evolve | Contratto API congelato; reuse `0.148*f` |
| R7 | 09:34:23 | ~9.3 min (tagliata) | evolve | **Non** scansionare 23→0 (oggi forza 24 bande DSP) |
| R8 | 09:40:44 | ~6.3 min (tagliata) | sintesi | Spec + piano TDD + questo MEGA |

Le attese 1–4 sono ~15 minuti come richiesto. Dalla R5 le attese sono state **accorciate** per consegnare spec/piano/MEGA prima del muro delle 2 ore.

### Cosa è stato ucciso

- Solo una legenda UI “Air → Band 7” (colla UX; l’utente l’ha già rifiutata in forma pin).
- Merge in `generateEQFromState` al posto di 1:N (distrugge HighShelf+Peak di Air).
- Piattaforma provenance / `IntentEnvelope` (non esiste in questo tree).
- Assist pin / highlight lista→grafico (già c’è `highlightProblem`; parcheggiato).
- `pushUndoState` su ogni apply (slider + morph 30 Hz vs stack da 20).
- Persistenza **solo** occupancy (dopo recall, Warmth rilascerebbe Air).
- Test che costruiscono `AIEqualizerAudioProcessor` nel target DSP (thread IR+AI nel ctor).
- Parametro APVTS `semanticIntensity` (apparirebbe in automazione host).
- Scan 23→0 copiato dal codice attuale (primo apply semantico → `numActiveBands=24`).
- Applicare complementary in questo slice (Air oggi ruba Clarity).
- Riscrivere `processBlock`.

### Cosa è sopravvissuto

Proiettore di occupancy: qualità primaria → N bande, niente clobber, release a zero, persistenza tripla, crescita di `numActiveBands` dal primo slot libero.

---

## 2. La cosa concreta scelta

**Evoluzione** della feature già presente (`SemanticControlPanel` + `applySemanticAdjustments`), non un plugin nuovo.

Oggi una qualità = **una** banda (`semanticBandAssignments: array<int,N>`). Air è definito come **due** filtri; il secondo vince. Complementary marca `sourceQuality` della qualità ospite (Clarity), non di Air.

Domani: funzione pura `project()` assegna 1..4 bande per qualità primaria, non tocca bande “calde” (`enabled && |gain|≥0.35`), cresce l’EQ da banda 8–9 su un preset a 8 bande, a slider ~0 **rilascia** le bande possedute, e lo stato plugin ricorda slider + mappa + intensity.

---

## 3. Perché questa, non le alternative

- **Legenda-only:** non cambia il suono. Colla. Abbandonata in R1/R2.
- **Merge TODO nel motore:** un solo Peak al posto di shelf+peak. Abbandonata: è una regressione di sound-design scritta nelle `qualityDefinitions`.
- **No-clobber solo su `applySingleCorrection` (AI):** utile ma non è la mappa semantica; il branch e l’automazione puntano al semantic intent. Abbandonata come tema di oggi.
- **Apply discreto (slider = preview):** rompe l’UX live attuale. Abbandonata.
- **Occupancy di sessione senza persist:** con release-on-zero, il recall + un movimento di Warmth **cancella** Air. Abbandonata in R4.

---

## 4. Grounding nel codice (file e gap)

| Pezzo | Dove | Comportamento attuale | Gap |
|---|---|---|---|
| Mappa 1:1 | `PluginProcessor.h:492`, ctor `fill(-1)` `PluginProcessor.cpp:81` | Un int per qualità | Last-write-wins |
| Apply | `applySemanticAdjustments` ~2971 | Claim dal **top** (23), clobber min `|gain|`, bump `numActiveBands` | 24 bande DSP al primo Air |
| Generazione | `generateEQFromState` / `generateEQForQuality` | Più `bands` + complementary `sourceQuality=comp.quality` riga 802 | Air ruba Clarity |
| Air | `SemanticEQEngine.cpp:175–178` | HighShelf 10 kHz + Peak 14 kHz | Solo uno sopravvive |
| Clarity (hitchhike) | `525–528` | 3 peak | Occupancy sbagliata |
| RESET / slider 0 | `resetAllSliders` + `adjustments.empty()` return | EQ resta | Release assente |
| Persistenza | `getStateInformation` ~3221 | APVTS + SlotA–D | Niente `SemanticState` né occupancy |
| Undo | `HistoryManager` `kMaxHistorySize=20` | Solo path AI `2702`, `2843` | Non aggiungere sul path live |
| Test | `CMakeLists.txt:916–931` | Solo DSP | `IntegrationStateTest.cpp` **non** è nel target |
| Ctor Processor | `93–103` | Thread IR + AI | Vietato nei unit test DSP |
| GUI | `SemanticControlPanel.h` slider 0 in ctor | Nessun hydrate | Recall con editor aperto |
| Highlight | `PluginEditor.cpp:48–52` → `highlightProblem` | Già esiste | Non è il pin parcheggiato; **out** |
| `processBlock` | snapshot `numActiveBands` ~3545 | Solo bande attive | **Non modificare**; si alza il param |

**VPA / PluginProcessor:** questo slice **richiede** `PluginProcessor.cpp/.h` (wrapper apply + persist). **Non** richiede `processBlock`. Non c’è VPA/`EffectiveDSPState` in questo tree.

**Ghost identity / persistence n/8 / Phase 1 envelope:** non esistono qui. Non inventarli.

**isDraggingBand mouseUp:** non presente; non toccare WIP inesistente.

---

## 5. Piano + puntatore

Piano TDD a task: **`PLAN-semantic-occupancy.md`**.

Riassunto: (1) test che falliscono + CMake, (2) flag `complementary`, (3) `project()` verde, (4) ValueTree, (5) wrapper Processor, (6) persist, (7) hydrate GUI, (8) suite DSP.

---

## 6. Strategia di test

- **CI di questo repo:** `AIEqualizerPro_Tests` (DSP + occupancy). Mai linkare `PluginProcessor.cpp` / `SemanticEQEngine.cpp` (Torch).
- Casi minimi: Air 8+9, no-clobber banda 1 a +6 dB, release, complementary ignorata, skip se tutto pieno, round-trip ValueTree.
- Persist Processor **non** è in CI finché non esiste un secondo binary (fuori scope).
- Questo loop **non** ha eseguito i test (nessuna implementazione).

---

## 7. Fuori scope

Assist pin; `processBlock`; VPA; install VST3 / SEMANTIC; Envelope 2.0; merge bande nel motore; undo live; parametro APVTS intensity; test full-plugin; complementary apply; colla lista↔grafico.

---

## 8. Primo commit atomico del prossimo agente

```
test(semantic): add failing SemanticOccupancy projector cases
```

Solo: stub `SemanticOccupancy.h/.cpp`, `SemanticOccupancyTest.cpp` che fallisce, file aggiunti a `AIEqualizerPro_Tests`. Niente Processor in quel commit.

---

## 9. Orologio e attese (prova di cadenza)

| Intervallo | Da | A | Durata attesa |
|---|---|---|---|
| Start → R1 write | 08:02:22 | 08:04:49 | lavoro, non attesa |
| R1 → R2 | 08:04:49 | 08:21:34 | **~16.5 min** |
| R2 → R3 | 08:21:34 | 08:37:47 | **~16.2 min** |
| R3 → R4 | 08:37:47 | 08:54:13 | **~16.4 min** |
| R4 → R5 | 08:54:13 | 09:13:27 | **~19.2 min** |
| R5 → R6 | 09:13:27 | 09:25:03 | **~11.6 min** (taglio muro 2h) |
| R6 → R7 | 09:25:03 | 09:34:23 | **~9.3 min** |
| R7 → R8 | 09:34:23 | 09:40:44 | **~6.3 min** |

Cadenza 15 minuti: **onorata nei round 1–4**; round 5–8 accorciati per consegnare il MEGA prima di 10:02 CEST.

**Fine consegna:** 2026-08-31 09:43:03 CEST (`1788162183`), prima del muro 10:02:22.

Copia Downloads: `/home/ubuntu/Downloads/EMBER_BRAINSTORM_2026-08-31.md` (scritta).
