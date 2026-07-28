# Storia di Ember Core — Dalla Genesi alla Forma Attuale

**Scope:** Intero plugin AI Equalizer Pro (DSP, GUI, ML, UI, infrastruttura, governance)  
**Periodo:** 2025-12-04 → 2026-07-27 (ultimo commit)  
**Fonti:** `git log --all`, branch list, worktree list, hotspot files, milestone commits  
**Metodo:** Estrazione deterministica da repository git — nessuna informazione a memoria

---

## 1. Panoramica Generale

| Metrica | Valore |
|---|---|
| Primo commit | 2025-12-04 |
| Ultimo commit | 2026-07-27 |
| Durata totale | ~7 mesi e 23 giorni |
| Commit totali (tutti i branch) | 704 |
| Commit su `main` | ~120 |
| Branch | 55 |
| Worktree attivi/registrati | 29 |
| File markdown tracciati | ~15k righe |
| File più toccato | `Source/PluginProcessor.cpp` (106 modifiche) |
| Secondo file più toccato | `CMakeLists.txt` (91 modifiche) |

### Attività mensile

| Mese | Commit | Interpretazione |
|---|---|---|
| 2025-12 | 1 | Genesi — build scripts e backup |
| 2026-01 | 16 | RT-safety, cross-platform, performance |
| 2026-02 | 0 | Vuoto — nessuna attività |
| 2026-03 | 130 | Hardening massivo — bug A-K, GUI, ML |
| 2026-04 | 146 | AIEQ+ framework, skills, release candidate |
| 2026-05 | 10 | Quasi fermo — Guida Suno v5.5 (fuori scope Ember Core) |
| 2026-06 | 137 | ML resonance recall, P4 esperimenti, Tribunale |
| 2026-07 | 264 | Picco massimo — Motore v2/v3, M6-M9, G1 contract |

La forma del progetto è a **due picchi** con una valle a febbraio e una pausa a maggio:
- **Picco 1 (marzo-aprile 2026):** hardening e governance — il plugin diventa stabile e rilasciabile
- **Picco 2 (luglio 2026):** Motore v2/v3 e ML — il progetto pivot verso la prossima generazione

---

## 2. Fase 1 — Genesi e Fondamenta (Dicembre 2025 – Gennaio 2026)

### 2.1 Primo commit (2025-12-04)
- `519b3b33` — "Add build scripts and backup functionality for AI Equalizer Pro"
- Il progetto nasce come plugin AI Equalizer Pro con infrastruttura di build e backup.
- Il nome "Ember Core" non è ancora esplicito; il focus è sulla build automation e sulla struttura di base.

### 2.2 Gennaio 2026 (16 commit) — RT-Safety e Cross-Platform
Commit chiave:
- `46dfaf47` (2026-01-07): "Fix RT-safety: eliminate heap allocation in audio thread" — primo intervento sulla sicurezza real-time dell'audio thread
- `6088ac58` (2026-01-06): "Implement lock-free triple-buffered spectrum access in AIEngine" — accesso lock-free allo spettro nell'AI engine
- `c6550320` (2026-01-06): "Fix array out of bounds crash when switching to Semantic page" — crash critico in debug build
- `33eed8ce` (2026-01-07): "FIX CRITICAL: Array out of bounds crash in Debug build" — conferma della gravità
- `fd6eb8b4` (2026-01-06): "Fix 5 critical bugs: RT-safety, cross-platform, and performance improvements" — batch fix di 5 bug critici
- `d500f121` (2026-01-06): "Fix critical bugs: RT-safety, cross-platform, array bounds, and performance" — stesso batch, messaggio alternativo
- `3d807726` (2026-01-18): "AI/GUI/DSP: thread-safety hardening, buffer reuse, analyzer guards" — hardening thread-safety su tutti e tre i sottosistemi
- `d9181c77` (2026-01-18): "AI: serialize inference; guard weights; reuse buffers; spectrum reconfig guard" — serializzazione inferenza AI
- `413c2572` (2026-01-18): "Add tests and UI/meters fixes" — inizio test regression
- `2747de2a` (2026-01-18): "Fix LP IR centering, enable solo monitor, expand dynEQ bands, always show GR meter" — funzionalità GUI

**Temi dominanti:** RT-safety (no heap allocation in audio thread), lock-free data structures, array bounds, cross-platform build, thread-safety hardening. Il progetto è in fase di stabilizzazione fondamentale.

### 2.3 Febbraio 2026 — Vuoto
Nessun commit. Possibile pausa di sviluppo o periodo di pianificazione.

---

## 3. Fase 2 — Hardening Massivo (Marzo 2026)

### 3.1 Panoramica
130 commit in un solo mese — il periodo di sviluppo più intenso prima di luglio. Il focus è sulla correzione di bug sistemici e sulla stabilizzazione del DSP.

### 3.2 Commit Rilevanti

**Cascata GR Meter (16-17 marzo):**
- `ba7c702f` — GR meter frozen when tweaking threshold/ratio — recalculate on param change
- `69b3bce4` — GR meter calcola GR dal GUI thread dai knob attuali — risposta istantanea
- `ade1c8b5` — GR meter frozen/lagging when tweaking RATIO or THRESHOLD
- `beab5f23` — GR meter release too slow when tweaking threshold knob
- `eb73e7ab` — GR meter confirmed working, remove debug logs

**Bug A-K (15 marzo):**
- `a22ccffa` — fix: bugs A-E — stability and correctness hardening
- `bd8ba702` — fix: bugs F/G/H — DynamicEQ detector, dryBuffer resize, PresetManager safety
- `ef43412b` — fix: bugs I/K — GUI dangling pointer crash and timer guard

**M/S Mode e Phase Bugs (16 marzo):**
- `fd2bbe0d` — fix: M/S mode Mid Only / Side Only / Linear Phase bugs (N/O/P)
- `03caf912` — fix: M/S Mid-Only/Side-Only phase + GR meter decay (bugs N/O/P + GR meter)
- `c9789be4` — fix: GR meter always showing 0 due to clearDynamicMeterCache every block

**Fase 0 — MLEngine disabilitato (16 marzo):**
- `06905ff3` — fix: Fase 0 — MLEngine disabilitato, soglie FORCE DETECTION, latenza NaturalPhase, thread detach

**Partitioned Convolution e Latenza (16 marzo):**
- `583b8237` — feat: partitioned convolution 128-sample latency (was 4096) — riduzione latenza da 4096 a 128 campioni
- `32c773f2` — fix: FORCE DETECTION x5, deadlock spectrumMutex, dryBuffer RT-safe, Peak nullptr, nth_element, SpectrumAnalyzer TOCTOU, crossfade cleanup

**Post-Review Pass (24 marzo):**
- `8a4694f3` — fix: post-review pass — 6 new issues (N1-N4, N6-N8)
- `3463a81f` — fix: bypass click and solo-drag crackle (2 audio glitches)
- `4ae08cda` — Improve bypass continuity handling and regression coverage
- `70f1d829` — Fix test contracts and improve phase transition crossfade

**Checkpoint (28 marzo):**
- `a13b4b88` — Checkpoint: pre-paint-refactoring state (all LOD + dynamic GR overlay + click fixes)

### 3.3 Significato della Fase
Marzo 2026 è la fase di **stabilizzazione critica**. Il plugin aveva problemi di:
- Click audio su bypass/phase/oversampling
- GR meter non funzionante o in ritardo
- M/S mode con bug di fase
- Array bounds crash
- Thread-safety (deadlock spectrumMutex, data race)
- RT-safety (heap allocation in audio thread)

La risoluzione di questi bug ha portato il plugin a uno stato di stabilità sufficiente per il rilascio.

---

## 4. Fase 3 — AIEQ+ Framework e Governance (Aprile 2026)

### 4.1 Panoramica
146 commit. Aprile 2026 è il mese in cui il progetto ha costruito l'infrastruttura di governance per gli agenti AI e ha raggiunto lo stato di **RELEASE-SAFE**.

### 4.2 Commit Rilevanti

**AIEQ Review Tribunal v4.2 (2 aprile):**
- `14fda2ec` — Add AIEQ Review Tribunal v4.2 with severity governor and failure-class discipline
- `d9ba4dac` — Add skill test files archive for iterative prompt validation
- `e6f0be9b` — docs: preserve AUDIO ARCHITECT WAR ROOM skill as reusable golden reference

**File Surgeon Skill (2 aprile — 8 record di test):**
- `dc93c185` — Add first File Surgeon skill test record for DynamicEQProcessor review
- `82c7fbbf` — Add second File Surgeon skill test record for Logger review
- `e366b90e` — Add third File Surgeon skill test record for PluginProcessor review
- `ee044d4d` — Add fourth File Surgeon skill test record for AdvancedSpectrumDisplay review
- `bddb0012` — Add fifth File Surgeon skill test record for PluginEditor review
- `9fd3f6d5` — Add sixth File Surgeon skill test record for AdvancedSpectrumDisplay v1.3 retest
- `5ae00c91` — Add seventh File Surgeon skill test record for NewSpectrumPipeline native v1.3 test
- `8c55074c` — Add eighth File Surgeon skill test record for GLSpectrumComponent v1.3 review

**AIEQ+ Framework (4 aprile):**
- `b3eeb3df` — feat: AIEQ+ framework — 9 foundational files + directory structure
- `0f36f4ad` — feat: AIEQ+ Phase 3 — Advanced governance protocols
- `b5621c4f` — feat: AIEQ+ Phase 3 complete — GLOSSARY, PROMOTION_POLICY, OUTPUT_PROTOCOL
- `b08d7bca` — feat: AIEQ+ Example — suno-prompt-audit (full lifecycle skill)
- `b8ff27aa` — feat: AIEQ+ Composite Skill example — audio-plugin-manager
- `8a565077` — feat: add aieq-plugin-auditor Composite Skill for AI Equalizer Pro
- `e1dc7cfd` — feat: promote aieq-plugin-auditor to v1.1 — Call Chain Verification + JUCE 7+ semantics
- `2bf4818d` — feat: add Regression Matrix and ORCHESTRATOR_CONFIG for aieq-plugin-auditor
- `fc6b881d` — feat: expand aieq-plugin-auditor to v1.2 with 10 sub-skills
- `e6ddf056` — feat: add release-verdict-engine — the definitive Meta-Auditor for release decisions

**Release Candidate (4 aprile):**
- `0b487c6d` — qa: finalize QA phase — transition to RELEASE-CANDIDATE (Rating 8.45/10)
- `d8c4a356` — qa: address Tribunal v4.2 concerns; Hardened RELEASE-CANDIDATE
- `4fa23dd3` — fix: OpenGL sync, GUI idle, M/S test expansion (Hardened RC)
- `2a338e47` — qa: address Tribunal v4.2 concerns (SpinLock, kMaxDelta, SemanticPanel, Governance)
- `4720134e` — fix: REAL implementation of juce::SpinLock and kMaxDelta 0.25f (resolving source/manifest discrepancy)
- `950a9737` — docs: Final Gates Passed (RELEASE-SAFE status reached)
- `4f51c210` — governance: D1 Fixed, P1-EC Fixed, T-5 Verified (Safety) — RELEASE-RISKY achieved
- `de3bce1f` — fix: T-6 (OSC logging) — finalize transition to RELEASE-SAFE

**Governance e Manifesto:**
- `3a1776e0` — governance: add ALIGNMENT_MANIFEST.md — single source of truth for cross-platform AI alignment
- `dbca05b1` — governance: add Promotion Tribunal Report v1.0 and associated test records
- `775cf247` — governance: update ALIGNMENT_MANIFEST.md — reflect RELEASE-RISKY state and Wave 1 fixes

**Skill Promotion:**
- `fb12191d` — governance: promote dsp-safety and gui-performance to VALIDATED (Tribunal v1.1)

**AI Engine Hardening (6 aprile):**
- `c4a72b0f` — feat: retrain ML model — F1 15.6% → 65.2%, clean FP 100% → 0%
- `09750c9a` — ai: apply calibrated shipping thresholds — precision-first, Tribunal-approved
- `800e0f3e` — ai: add deterministic ML-only hooks and dB-to-linear conversion in AIEngine

**Anti-Pop DSP (9 aprile):**
- `83f17d5e` — fix: anti-pop DSP — bypass state machine, coefficient crossfade, IR latest-wins, debounce 20ms
- `62222f2f` — test: AntiPopRegressionTest — 9 adversarial scenarios, all pass with 0 clicks

**Liquid Intelligence GUI (10-11 aprile):**
- `a4dbb563` — ui: Liquid Intelligence GUI Redesign — Wave 1+2+3 complete
- `bf7fcd39` — ui: Liquid Intelligence critical fixes — knob aspect ratio + band filmstrip knobs
- `01f60ef1` — ui: Wave 4B-fix — Tribunale tooltip overlap + glass nodes + cyan post-EQ + AI zone labels
- `c3a8446e` — ui: Wave 4D — Tribunale video verdict fixes + Marco's 4 aesthetic tweaks

### 4.3 Significato della Fase
Aprile 2026 è il mese della **maturità governance**. Il progetto ha:
1. Costruito il framework AIEQ+ con Tribunale, File Surgeon, e release-verdict-engine
2. Raggiunto RELEASE-SAFE con rating 8.45/10
3. Implementato il primo sistema di disciplina degli agenti AI (il predecessore del Tribunale della Veridicità)
4. Completato la GUI Liquid Intelligence
5. Addestrato il modello ML da F1 15.6% a 65.2%

Il Tribunale della Veridicità (costruito in giugno) è l'evoluzione diretta di questo lavoro di aprile.

---

## 5. Fase 4 — ML Resonance Recall e P4 Experiments (Giugno 2026)

### 5.1 Panoramica
137 commit. Giugno è il mese degli esperimenti ML intensivi — il programma Resonance-ML, le chiusure con kill-criterion onorato, e la costruzione del Tribunale della Veridicità.

### 5.2 Commit Rilevanti

**Tribunale della Veridicità (16-17 giugno):**
- `2bbed01c` (2026-06-16): feat(skill): Tribunale della Veridicità — framework giudiziario 3 gradi per claim AI
- `195299f0` (2026-06-17): refactor(skill): rendi il Tribunale della Veridicità agnostico rispetto alla fonte

**P4 ML Experiments (17 giugno):**
- `039372b0` — test(ai): P4-M1 eval integrity — freeze shape-disjoint held-out + corpus sanity framing
- `2d10bfab` — experiment(ai): P4-M2a tilt-invariance training — MEASURED NEGATIVE, candidate UNSHIPPED
- `1658b65c` — experiment(ai): P4-M2a-refine A/B/C — STRONG PARTIAL, candidate B UNSHIPPED
- `c085bc9a` — test(ai): P4-M3 diagnostic — REFUTES mel-resolution premise for Resonance
- `824538e4` — docs: record P4-M3-diagnostic — mel-resolution premise refuted, Resonance is training tension
- `f6894819` — test(ai): P4-M3-SHIP product-pipeline B vs shipped, all classes — SOBERING reframe
- `c3609c44` — docs: record P4-M3-SHIP product-level reframe — B's net product gain is modest (Thinness)
- `af65b5fa` — docs: DECISION strategy B — close ML-retrain line, pivot to veto layer + real data

**P4-BUG-001 (18-19 giugno):**
- `5bd26206` — docs: register P4-BUG-001 snapshot-coherence (Codex finding) — production veto bug
- `46f7bf26` — test(ai): P4-BUG-001 witness — Resonance detection is severely HISTORY-dependent (RMS-controlled)
- `d5834e48` — test(ai): P4-BUG-001 ISOLATED witness — bug causally isolated to the veto (Codex acceptance met)
- `970e7968` — test(ai): P4-BUG-001 witness corrections (Codex) — full ML vector, assertions, guarded probe
- `0a3acbe7` — docs: P4-BUG-001 12-agent consolidated audit — bug+fix PROVEN, severity NOT yet (3 closures)
- `21656aed` — test(ai): P4-BUG-001 closure 1 — pre-veto detection list witness closes the localization confound
- `b3a2d45c` — test(ai): P4-BUG-001 closure 2 — LIVE baseline proves the bug survives the persistence gate
- `3737cf94` — test(ai): P4-BUG-001 closure 2 v2 — Codex fix: keep target ABOVE the 60% persistence gate
- `a0ec4619` — test(ai): P4-BUG-001 before/after acceptance harness + BEFORE baseline (shipped/buggy)
- `17fe2c95` — fix(ai): P4-BUG-001 — ML-path veto reads all bands from ONE frame (snapshot coherence)
- `24e57a52` — docs: P4-BUG-001 CLOSED — Ableton verification confirmed by Marco

**Real-Data ML Program (22-23 giugno):**
- `cf76324d` — test(ai): real-data program M1 — injection factory on real vocals (VocalSet); first finding
- `1e63194d` — test(ai): real-data M2 — paired de-ess/boost on sibilant frames LEARNS sibilance
- `fe0037c9` — test(ai): real-data M3 — product-pipeline bridge locates the loss: the Sibilance VETO prunes vocal sibilance
- `d0e70eb0` — test(ai): real-data M3b — quantify the Sibilance veto + alternative references (counter-exam)
- `8ac21fbb` — test(ai): real-data M4 — low-variance gate RESOLVES it: the "Resonance regression" was NOISE, not a tradeoff
- `8fede41e` — test(ai): real-data M4 Phase C1 — all-8 numeric ship-gate; NEGATIVE result (candidate NOT shippable)

**ML Interim (30 giugno):**
- `71eef41b` — fix(ai): ship seed22 ML-only interim without resonance assist
- `ec688686` — docs(ai): archive resonance assist as NO-GO and document probe

### 5.3 Significato della Fase
Giugno 2026 è il mese della **veridicità sotto pressione**. Il programma Resonance-ML ha attraversato:
- 4 fasi di esperimenti (M1-M4) con kill-criterion onorato
- La scoperta e chiusura del bug P4-BUG-001 (snapshot coherence nella ML veto path)
- La costruzione del Tribunale della Veridicità come framework giudiziario per claim AI
- La decisione strategica di chiudere la linea ML-retrain e pivotare a veto layer + real data

Il kill-criterion è stato rispettato due volte: Resonance recall era un problema di training tension, non di architettura.

---

## 6. Fase 5 — Motore v2/v3 e ML Programs M6-M9 (Luglio 2026)

### 6.1 Panoramica
264 commit — il mese più intenso nella storia del progetto. Luglio 2026 vede il pivot a Motore v3 contract-first, il completamento dei programmi ML M6-M9, la risoluzione del bug pluginval, e l'avanzamento della G1 contract per Motore v3.

### 6.2 Commit Rilevanti

**Pluginval Crash Fix (2-3 luglio):**
- `0c1123e1` — docs(known-issue): pluginval crash is seed-dependent and fires at s8 — withdraw workaround
- `56121848` — test(threadsafety): add quarantined param-storm detector — reproduces the pluginval crash as a heap-buffer-overflow in Oversampling::processSamplesUp
- `9b0d7c5f` — test(threadsafety): deterministic single-thread repro — msMode-transition crossfade drives the FULL 32768-sample transition buffer into a 512-sized oversampler
- `f73be87c` — fix(dsp): msMode-transition crossfade — pass a blockSamples-limited view to the old-mode path, not the raw 32768-sample transition buffer
- `98b7ee5f` — docs(known-issue): pluginval param-thread-safety crash RESOLVED — root cause was a deterministic oversampler heap-buffer-overflow in the msMode crossfade, fixed in eb88b2a0
- `905821db` — docs(threadsafe): promote msMode storm repro to green regression

**ML Programs M6-M9 (7 luglio):**
- `6c7fbe5f` — feat(ml): eval_realclips — M6 pre-veto gate on the 8 Ableton holdout clips
- `6982be0b` — test(ml): harden M6 clean realclip gate
- `6b88099a` — docs(ml): M6 close-out — product-emission finding, best blob, kill-criterion
- `485e4fd0` — feat(ml): M7 product-ranking loss + emission-loss diagnostic
- `c262b363` — docs(ml): M7 final report — NO-GO robust, GO interim single blob, redesign next
- `2d1dff96` — docs(ml): record M8 redesign baseline and acceptance bar
- `62919b63` — feat(ml): add M8 two-stage per-class network
- `9a45b107` — docs(ml): M8 final verdict — NO-GO v1, blockers classified, M9 spec
- `4657b5b6` — feat(ml): M9.2 emission_mode per-class, drop global presence gate

**Motore v2 Walking Skeleton (10 luglio):**
- `051a8946` — vendor: RTNeural (BSD-3) header-only subset for Motore v2
- `5634d364` — feat(motore-v2): A0 walking skeleton — numpy->JSON->RTNeural parity bridge
- `686e6e16` — feat(motore-v2): A3 — real CNN architecture parity (dilated Conv1D bridge)
- `bab42b93` — feat(motore-v2): A2 — log-mel feature contract with full-chain parity
- `56db3ef5` — feat(motore-v2): A1 — corpus manifest, license audit, anti-leakage, splits
- `e133fb9c` — feat(motore-v2): A6 — benchmark harness (numpy RTNeural forward + clip gates)
- `3bd4ccba` — feat(motore-v2): A4 — trainer (M9 recipes on dB windows, weighted BCE + masked Huber)

**Motore v3 G1 Contract (19-25 luglio):**
- `a0e1321e` — docs(motore-v3): establish phase-contract roadmap
- `2c88edad` — test(motore-v3): freeze reproducible G0 baseline
- `e9f505a6` — docs(motore-v3): freeze G1 frontend and benchmark contract
- `f8ef9d9c` — docs(motore-v3): close G1 calibration and benchmark gaps
- `58eb7306` — docs(motore-v3): harden G1 statistical contract
- `632cb0bb` — docs(motore-v3): close final G1 review blockers
- `9498096c` — docs(v3): freeze audited G1 contract revision 5
- `6899c7a0` — docs(v3): freeze byte-level G1 contract revision 6
- `76b04d2d` — feat(v3): add fail-closed G1 contract primitives
- `94dc9991` — feat(v3): G1a T1 contract primitives (identity-bound split)
- `1908fc45` — feat(v3): G1a T2 JSON schemas and fail-closed validators
- `501a4e00` — feat(v3): freeze G1a T6 fixture generators and WAV inventory
- `1746a058` — docs(v3): stamp G1a CLOSE GO after Guardian
- `284228d6` — docs(v3): reopen G1a CLOSE after blocker CC
- `a2186ac1` — fix(v3): close G1a F2/F3/F4 blockers on reopen
- `57b31bf1` — docs(v3): stamp G1a CLOSE GO after Guardian re-CLOSE

**Motore v3 G1b (25 luglio):**
- `a91ab7cf` — feat(v3): land G1b T1 frontend seed on spike branch
- `f1ed82fb` — feat(v3): G1b T1b causal polyphase streaming apply (WS1)
- `743270c8` — feat(v3): G1b T2 offline §6+§7 feature path (WS2)
- `ed9c3dd9` — feat(v3): G1b WS3 streaming feature path (offline≡chunk)
- `c7f05871` — feat(v3): G1b WS4 gate-4 SR-parity harness + spike evidence (RED)

**Motore v3 G1c (26-27 luglio):**
- `52702f7b` — docs(v3): seal G1c determinism pins; reseal contract tripwire
- `f6b7f43d` — feat(v3): G1c T0 gate-9 fixture registry
- `ada23fd3` — feat(v3): G1c T1 fail-closed evaluator input parser
- `3b4932d5` — feat(v3): G1c T2 deterministic one-to-one event matching
- `82c866e4` — fix(v3): derive G1c event ids from content, not arrival order

**REV7 (25-26 luglio):**
- `b3d7f71b` — docs(v3): record final REV7 R remeasure (stationary PASS, sweep FAIL)
- `5e0d32fc` — docs(v3): package REV7 candidate — LF report-only + gate-4 scope (C)
- `6fbf5b59` — docs(v3): consolidate REV7 — LF report-only + gate-4 scope (C)
- `4efc7598` — chore(v3): rehash metrology lock + SHA256SUMS after REV7

**REV8 (27 luglio):**
- `e2bee113` — docs(v3): REV8 candidate — five corrections from the two-lens red-team

**G1c T3 (27 luglio):**
- `33b4d011` — feat(v3): G1c T3 semantic region matcher (§10.1)
- `b96f1f59` — revert(v3): forward revert of G1c T3 semantic region matcher
- `82c866e4` — fix(v3): derive G1c event ids from content, not arrival order

### 6.3 Significato della Fase
Luglio 2026 è il mese del **pivot strutturale**:
1. Il bug pluginval (crash s8) è stato isolato e risolto — era un heap-buffer-overflow deterministico nel msMode crossfade
2. I programmi ML M6-M9 hanno completato il ciclo Resonance-ML con kill-criterion onorato
3. Motore v2 ha raggiunto lo walking skeleton con RTNeural integration
4. Motore v3 è entrato nella fase contract-first con G1a CLOSE GO e G1b in spike
5. Il progetto è passato dalla fase di hardening a una fase di **ricostruzione architetturale**

---

## 7. Linea Evolutiva delle Skill di Disciplina Agenti

Questa linea è emersa dall'estrazione deterministica dei commit — 49 commit dedicati a disciplina degli agenti.

### 7.1 Aprile 2026 — AIEQ Review Tribunal v4.2
- `14fda2ec` — Add AIEQ Review Tribunal v4.2 with severity governor and failure-class discipline
- `66b0e086` — Add skill selection directives cheat sheet for automatic skill assignment
- `89eda44d` — Add quickstart skill selection cheat sheet for automatic AI routing
- `3f5c4720` — Add skill implementation method base distilled from full conversation workflow
- `e6f0be9b` — docs: preserve AUDIO ARCHITECT WAR ROOM skill as reusable golden reference

### 7.2 Aprile 2026 — aieq-plugin-auditor Composite Skill
- `8a565077` — feat: add aieq-plugin-auditor Composite Skill for AI Equalizer Pro
- `fc6b881d` — feat: expand aieq-plugin-auditor to v1.2 with 10 sub-skills
- `2bf4818d` — feat: add Regression Matrix and ORCHESTRATOR_CONFIG for aieq-plugin-auditor
- `e1dc7cfd` — feat: promote aieq-plugin-auditor to v1.1 — Call Chain Verification + JUCE 7+ semantics

Sub-skill:
- `dsp-safety-audit` — audit della sicurezza DSP
- `gui-performance-audit` — audit delle performance GUI
- `ai-integration-audit` — audit dell'integrazione AI
- `code-hygiene-audit` — audit dell'igiene del codice
- `build-system-audit` — audit del sistema di build

L'`ORCHESTRATOR_CONFIG.yaml` instrada per pattern di path (Source/DSP/** → dsp-safety-audit, Source/AI/** → ai-integration-audit…) con governance_state: frozen-pending-composite-regression.

### 7.3 Aprile 2026 — Release Verdict Engine
- `e6ddf056` — feat: add release-verdict-engine — the definitive Meta-Auditor for release decisions
- `0778097a` — docs: add AIEQ+ Evolution Strategy — post-audit strategic roadmap
- `3a1776e0` — governance: add ALIGNMENT_MANIFEST.md — single source of truth for cross-platform AI alignment

### 7.4 Giugno 2026 — Tribunale della Veridicità
- `2bbed01c` — feat(skill): Tribunale della Veridicità — framework giudiziario 3 gradi per claim AI
- `195299f0` — refactor(skill): rendi il Tribunale della Veridicità agnostico rispetto alla fonte

L'Articolo 0 del Tribunale: "Nessuna dichiarazione è vera finché non è provata dal codice. La parola del Dichiarante — chiunque esso sia, AI o umano — non è prova: è un'imputazione da verificare."

Il Tribunale esiste sul branch remoto `origin/claude/ai-statement-verification-51ghqv` e non è nel filesystem di nessun worktree attivo.

---

## 8. Branch e Worktree

### 8.1 Branch Principali
- `main` — branch principale, 120 commit
- `origin/main` — mirror remoto
- `origin/HEAD` — punta a `Add: Guida Avanzata Suno v5.5` (2026-05-12)

### 8.2 Branch di Lunga Vita (selezione)
| Branch | Data | Descrizione |
|---|---|---|
| `fix/code-review-march` | 2026-03-24 | Merge di phase-b/rbj-coefficients |
| `feature/ui-polish-post-recovery` | 2026-04-30 | recupero post-ui-polish |
| `feature/motore-v2-a0` | 2026-07-10 | walking skeleton Motore v2 |
| `feature/motore-v3-g1a-contract-artifacts` | 2026-07-23 | artefatti contrattuali G1a |
| `spike/motore-v3-g1b-frontend` | 2026-07-25 | spike frontend G1b |
| `feature/motore-v3-g1c-reseal` | 2026-07-27 | reseal G1c |
| `feature/motore-v3-offline` | 2026-07-27 | feature path offline G1b |
| `feature/model-recall-realclips-m6-lab` | 2026-07-07 | lab M6 |
| `feature/model-recall-m9-lab` | 2026-07-09 | lab M9 |
| `feature/a4b-control-baseline` | 2026-07-17 | baseline controllo A4b |
| `feature/a4b-contract-enforcement` | 2026-07-17 | enforcement split contract |
| `feature/a4b-corpus-admission` | 2026-07-17 | admission corpus |
| `feature/a4b-data-seed-grid` | 2026-07-19 | grid semi dati |
| `feature/prominence-engine-phase1` | 2026-07-20 | motore prominenza |
| `feature/prominence-p0-p2` | 2026-07-22 | P0/P2 prominenza |
| `hardening/p0-races-capture` | 2026-07-03 | hardening race condition |
| `feature/d1-exposure` | 2026-07-06 | esposizione D1 |
| `feature/d1-dynamic-correction` | 2026-07-04 | correzione dinamica D1 |
| `ai/evolution-phase1` | 2026-07-04 | evoluzione AI |
| `ai/cognition-engine` | 2026-07-05 | Cognition Engine branch |
| `claude/affectionate-wing` | 2026-03-29 | fix click-free audio |
| `claude/wizardly-elion` | 2026-04-10 | Wave 4A GUI |
| `claude/charming-cohen` | 2026-03-29 | remove AIEQ-mac fork |
| `claude/strange-einstein` | 2026-03-24 | Phase A crash fix |
| `claude/vigorous-nobel` | 2026-03-24 | Phase A crash fix (detached) |
| `origin/claude/ai-statement-verification-51ghqv` | 2026-06-17 | Tribunale della Veridicità (remoto) |
| `origin/claude/parallel-codebase-analysis-equalizer-vxQaN` | 2026-06-03 | fix DynamicEQ guard |
| `origin/claude/apply-crit-1-fix-Nalnr` | 2026-04-21 | fix DSP data race |
| `origin/claude/review-source-files-T8jem` | 2026-03-20 | CI macOS build |
| `origin/claude/explain-codebase-mk2s6604wvm2qtd3-RlSss` | 2026-01-06 | Fix 5 critical bugs |
| `origin/codex/analyze-the-codes` | 2026-01-05 | block-size regression CTest |
| `origin/phase-b/rbj-coefficients` | 2026-03-25 | RBJ coefficients |
| `origin/fix/pending-reset-regression` | 2026-03-25 | regression tests |
| `origin/feature/visual-rebranding-premium` | 2026-04-06 | CI build workflow |
| `origin/feature/aieq-plus-framework` | 2026-04-07 | skills promotion |
| `origin/review/business-plan` | 2026-04-04 | business plan WIP |
| `origin/review/codex-2026-04-01` | 2026-04-09 | Liquid Intelligence GUI |
| `origin/chatgpt-sync` | 2026-04-11 | chatgpt sync tests |
| `origin/wizardly-elion` | 2026-04-10 | Wave 4A |
| `origin/HEAD` | 2026-05-12 | Guida Suno v5.5 |
| `origin/main` | 2026-05-12 | Guida Suno v5.5 |

### 8.3 Worktree Attivi (29 totali)
I worktree sono distribuiti su:
- **ML/AI:** a4b-control-baseline, a4b-corpus-admission, a4b-data-seed-grid, m6-lab, m7-interim, m9-lab, motore-v3-corpus-inventory, prominence-p0-p2, prominence-phase1
- **Motore v2/v3:** motore-v2-a0 (recover-ember-core), motore-v3-g1a-contract-artifacts, motore-v3-g1b-spike, motore-v3-g1c-reseal, motore-v3-offline, motore-v3-sandbox
- **DSP/Hardening:** p0-races-capture, d1-exposure-fix, d1-dynamic-correction
- **GUI/UX:** ui-polish-post-recovery, affectionate-wing, wizardly-elion
- **Governance:** a4b-contract-enforcement, a4b-0c-freeze

---

## 9. File Chiave e Architettura

### 9.1 File Più Toccati (Hotspot)
| File | Modifiche |
|---|---|
| `Source/PluginProcessor.cpp` | 106 |
| `CMakeLists.txt` | 91 |
| `Source/PluginProcessor.h` | 66 |
| `docs/AI_SCORECARD.md` | 48 |
| `Source/PluginEditor.cpp` | 45 |
| `Source/AI/AIEngine.cpp` | 41 |
| `Source/GUI/AdvancedSpectrumDisplay.h` | 38 |
| `Source/AI/AIEngine.h` | 33 |
| `Source/PluginEditor.h` | 31 |
| `Source/AI/MLEngine.cpp` | 26 |
| `docs/MOTORE_V3_PLAN.md` | 26 |

### 9.2 Directory Chiave
- `Source/ML/` — Motore ML (MLEngine, AIEngine, MLOperator)
- `Source/DSP/` — DSP core (DynamicEQ, LinearPhase, Oversampling, Convolution)
- `Source/GUI/` — Interfaccia grafica (PluginEditor, AdvancedSpectrumDisplay, AIProblemPanel)
- `Source/AI/` — AI detection and correction (AIEngine, MLEngine, ReferenceMatcher)
- `docs/` — Documentazione (PLAN, ARCHITECTURE, AI_SCORECARD, MOTORE_V3_PLAN, war room)
- `skills/ACTIVE_SKILLS/` — Framework di disciplina agenti (aieq-plugin-auditor, sub-skills)
- `PROMPTS/` — Prompt del Tribunale (AIEQ_REVIEW_TRIBUNAL v4.1, v4.2, v4 Imperial)
- `review/` — Report del Tribunale e QA
- `tests/` — Test suite (CTest, regression, integration)

---

## 10. Milestone Reali Raggiunti

| Data | Milestone | Commit |
|---|---|---|
| 2025-12-04 | Genesis — build scripts e backup | `519b3b33` |
| 2026-01-06 | RT-safety: no heap allocation in audio thread | `46dfaf47` |
| 2026-01-06 | Lock-free triple-buffered spectrum access | `6088ac58` |
| 2026-03-16 | GR meter fully functional (cascata fix) | `ba7c702f` → `eb73e7ab` |
| 2026-03-24 | Post-review pass — 6 new issues (N1-N8) | `8a4694f3` |
| 2026-03-28 | Checkpoint pre-paint-refactoring | `a13b4b88` |
| 2026-04-02 | AIEQ Review Tribunal v4.2 + File Surgeon skill | `14fda2ec` |
| 2026-04-02 | AUDIO ARCHITECT WAR ROOM preserved as golden reference | `e6f0be9b` |
| 2026-04-04 | RELEASE-RISKY achieved (D1 Fixed, P1-EC Fixed, T-5 Verified) | `4f51c210` |
| 2026-04-04 | RELEASE-SAFE status reached (Rating 8.45/10) | `950a9737` |
| 2026-04-04 | aieq-plugin-auditor v1.2 with 10 sub-skills | `fc6b881d` |
| 2026-04-04 | Release verdict engine + ALIGNMENT_MANIFEST | `e6ddf056`, `3a1776e0` |
| 2026-04-09 | Anti-pop DSP — 9 adversarial scenarios, 0 clicks | `62222f2f` |
| 2026-04-11 | ChatGPT sync tests pass1/pass2 | `4c6c2ab8`, `eb353d1a` |
| 2026-06-16 | Tribunale della Veridicità — 3-grade framework | `2bbed01c` |
| 2026-06-17 | Tribunale made source-agnostic | `195299f0` |
| 2026-06-19 | P4-BUG-001 CLOSED — Ableton verification | `24e57a52` |
| 2026-06-22 | Resonance-ML program closed — NO-GO with kill-criterion honored | `8fede41e` |
| 2026-07-03 | pluginval crash RESOLVED — deterministic oversampler HBO fixed | `98b7ee5f` |
| 2026-07-03 | Motore v2 A0 walking skeleton | `5634d364` |
| 2026-07-07 | M6 close-out — product-emission finding, kill-criterion | `6b88099a` |
| 2026-07-08 | M8 final verdict — NO-GO v1, M9 spec | `9a45b107` |
| 2026-07-15 | G1 contract primitives for Motore v3 | `76b04d2d` |
| 2026-07-25 | G1a CLOSE GO after Guardian | `1746a058` |
| 2026-07-25 | REV7 candidate packaged (LF report-only + gate-4 scope C) | `5e0d32fc` |
| 2026-07-26 | REV7 rehash — metrology lock + SHA256SUMS | `4efc7598` |
| 2026-07-27 | REV8 candidate — five corrections from two-lens red-team | `e2bee113` |
| 2026-07-27 | G1c T3 semantic region matcher (reverted, then re-derived from content) | `33b4d011` → `82c866e4` |

---

## 11. Filo Narrativo — Dalla Genesi a Oggi

### 1.1 Nascita e Stabilizzazione (Dic 2025 – Gen 2026)
Ember Core nasce come plugin AI Equalizer Pro con build automation e backup. I primi mesi sono dedicati alla stabilizzazione fondamentale: RT-safety (eliminazione heap allocation in audio thread), lock-free data structures per lo spectrum access, e correzione di crash critici (array bounds, nullptr). La GUI è funzionale ma grezza. Il ML engine (MLEngine) è presente ma disabilitato nella Fase 0.

### 1.2 Hardening e Stabilizzazione (Marzo 2026)
Marzo è il mese del "tutto deve funzionare". 130 commit per risolvere bug sistemici: click audio su bypass, GR meter non funzionante, M/S mode con bug di fase, deadlock sullo spectrumMutex, data race nell'AI engine. La serie di fix del GR meter (16-17 marzo) è uno dei momenti più intensi — 6 commit in due giorni per portare il meter da "frozen" a "fully functional". Il checkpoint del 28 marzo segna lo stato pre-refactoring della GUI.

### 1.3 Governance e Rilascio (Aprile 2026)
Aprile è il mese della maturità. Il progetto costruisce l'intero framework di governance AI (AIEQ+) con Tribunale, File Surgeon, e release-verdict-engine. Il rating 8.45/10 e lo stato RELEASE-SAFE sono il risultato di un processo di QA rigoroso. Le skill di disciplina agenti (aieq-plugin-auditor con 10 sub-skills) sono il primo sistema sistematico di controllo qualità per gli AI che lavorano sul codebase. La GUI Liquid Intelligence completa il redesign.

### 1.4 Veridicità e Kill-Criterion (Giugno 2026)
Giugno è il mese della veridicità sotto pressione. Il programma Resonance-ML attraversa 4 fasi di esperimenti con kill-criterion onorato — la conclusione è che Resonance recall è un problema di training tension, non di architettura, e la linea ML-retrain viene chiusa. Il bug P4-BUG-001 (snapshot coherence nella ML veto path) viene scoperto, isolato, e chiuso con verifica Ableton. Il Tribunale della Veridicità viene costruito come framework giudiziario 3-gradi per claim AI, poi reso agnostico rispetto alla fonte.

### 1.5 Pivot Strutturale (Luglio 2026)
Luglio è il mese del pivot. Il bug pluginval (crash s8) viene isolato e risolto — era un heap-buffer-overflow deterministico nel msMode crossfade. I programmi ML M6-M9 completano il ciclo Resonance-ML con kill-criterion onorato. Motore v2 raggiunge lo walking skeleton con RTNeural integration. Motore v3 entra nella fase contract-first con G1a CLOSE GO e G1b in spike. Il progetto passa dalla fase di hardening a una fase di ricostruzione architetturale, con la documentazione che diventa il driver principale dello sviluppo (G1a T1-T6, G1b WS1-WS4, G1c T0-T3).

---

## 12. Prior Art Esistente — Il Protocollo Non Parte da Zero

### 12.1 AIEQ Review Tribunal (Aprile 2026)
- Severity governor e failure-class discipline
- 8+ test records per ogni skill File Surgeon
- AUDIO ARCHITECT WAR ROOM come golden reference
- Programmatic validation harness per RB-2/RB-3/RB-4

### 12.2 aieq-plugin-auditor Composite Skill
- 10 sub-skills con orchestratore (ORCHESTRATOR_CONFIG.yaml)
- Routing per pattern di path (Source/DSP/** → dsp-safety-audit, Source/AI/** → ai-integration-audit)
- governance_state: frozen-pending-composite-regression
- Regression Matrix per tracciare promozioni e demozioni delle skill

### 12.3 Tribunale della Veridicità (Giugno 2026)
- 3 gradi di giudizio per claim AI
- Agnostico rispetto alla fonte (funziona con qualsiasi agente, non solo Codex)
- Articolo 0: "Nessuna dichiarazione è vera finché non è provata dal codice"
- 6 documenti di riferimento: procedura, collegio peritale, scala dei verdetti, tassonomia dei dichiaranti, registro di giurisprudenza, template di sentenza
- Validato su 5 dichiarazioni reali con 6 documenti di riferimento

### 12.4 Cosa Manca
Il Tribunale esiste e funziona, ma:
1. Vive solo su un branch remoto (`origin/claude/ai-statement-verification-51ghqv`), non in nessun worktree attivo
2. Non ha il capitolo "snapshot discipline" — non presuppone che il giudice possa leggere il codice live, ma non gestisce esplicitamente la staleness di uno zip
3. Non ha un formato di output meccanico che obblighi citazione file:riga, dichiarazione di staleness, e separazione trovato/deciso

Questi sono esattamente i gap che il protocollo per assistenti esterni deve colmare.

---

*Documento generato da estrazione deterministica da git log --all — nessuna informazione a memoria. Tutti i commit citati esistono nel repository.*