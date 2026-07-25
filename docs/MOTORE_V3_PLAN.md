# Motore v3 - Sviluppo a contratti per fase

## Stato e obiettivo

Questo e il piano canonico del laboratorio Motore v3. Congela obiettivo,
interfacce di prodotto, governance e criteri di successo; l'architettura ML
viene scelta soltanto dopo benchmark, baseline DSP e corpus controfirmati.

### Stato fattuale (aggiornato 2026-07-25) - leggere PRIMA del resto

Il piano descrive un progetto a contratti. Ad oggi, su questo branch:

- **G0: PASS**, freeze RIPRODUCIBILE della baseline NEGATIVA. Non promuove
  alcun modello.
- **G1 contratto**: freeze document-only `6d254d0a` (REV6 consolidata +
  micro-amend). Header/§16 di quel commit restano storicamente "non e GO";
  lo **stato di fase vivente** e nelle righe Governance sotto.
- **Tip accuracy**: codice G1a tip = `75cb6902` (T5). PLAN tip (docs) =
  `37f6ac60` (stato T5 + mandato G1b). Non confondere i due tip.
- **GO G1a**: aperto (2026-07-25) dal reviewer (Marco) sul freeze `6d254d0a`.
- **G1a codice in git** (tip `75cb6902`): T1 `94dc9991` (primitives/split/
  coverage) + T2 `1908fc45` / T2.1 `918b3dde` (JSON schemas, validators,
  golden canonical) + T3 `9aa19295` (adapter v2↔v3 hashed,
  `adapter_mapping_sha256` =
  `6a978c01bcccb85fb7db17ae3c66ee55ebceee82f47dca792e5a2f3a5fb9828f`) +
  T4 `3bfd8aaf` (metrology lock §13 committed;
  `metrology_lock_sha256` =
  `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`) +
  T5 `75cb6902` (SHA256SUMS trust anchor + path hygiene; inventory covers
  contract + adapter + lock + schemas; SHA256SUMS does **not** self-hash —
  trust anchor = this commit).
- **G1a T5**: committed at `75cb6902`. SHA256SUMS is the trust anchor for
  artifact digests beyond lock inline `dependencies`; it does not hash
  itself (anchor = commit `75cb6902`). **M1 T5 catch-up CC: CLEAN**
  (2026-07-25) on `75cb6902`. Hygiene findings that are not semantic
  BLOCKERs → durable debt below; **do not reopen T5 hash**.
- **G1a vs DoD contratto §14 item 1 (G1a)**: T1–T5 hanno atterrato solo le
  **primitives di contratto** — schemas/validators, adapter v2↔v3 hashed,
  metrology lock, SHA256SUMS trust anchor. **Non** chiudono da soli il DoD
  G1a: restano **OPEN (T6)** i **generatori di segnale fixture e i relativi
  hash**, come richiesto dal contratto §14 ("generatori fixture e hash").
  Retract: qualsiasi claim precedente che §14.1 / G1a artifact set sia
  "fully landed" via T1–T5 alone. **Sequencing**: fixture-spec v1
  commit+hash **before** generation; **G1a CLOSE** only after T6 (+ CC /
  Guardian GO) **before** official product G1b. **No G1 PASS**, not
  release-safe.
- **GO G1b (mandato)**: aperto nel PLAN tip `37f6ac60` (2026-07-25) dal
  reviewer (Marco) con mandato: **REV7 only if implementation demonstrates
  falsifiable impossibility** (not inconvenience); one redteam + one
  independent CC per tranche; stop-rule semantica sotto (non etichetta
  di severity). **Product G1b not started** — mandato aperto ≠ lavoro
  prodotto iniziato. Uncommitted `ml_v3/frontend/` = **spike /
  feasibility probe** (not gate proof; non tip ufficiale G1b) fino a
  M3+M4.
- **Authority hierarchy (Motore-v3)**:
  - GLOBAL / RELEASE AUTHORITY → `ALIGNMENT_MANIFEST.md`
  - MOTORE V3 LAB STATE AUTHORITY → `docs/MOTORE_V3_PLAN.md`
  - FROZEN G1 TECHNICAL AUTHORITY → `docs/MOTORE_V3_G1_CONTRACT.md` @
    `6d254d0a`
  Per task Motore-v3, ordine di lettura: PLAN → frozen phase contract →
  ALIGNMENT_MANIFEST per vincoli globali / ship.
- **Stop-rule (unica, semantica)**: **BLOCKER** (indipendentemente da
  etichetta CRITICAL/HIGH/MED/LOW) se abilita: false PASS; final-test
  leakage; split leakage; hash/binding ambiguity; canonical
  nondeterminism; post-freeze gate-domain mutation; non-implementability
  del contratto. Tutto il resto → durable debt list. Le etichette di
  severity descrivono gravita; la semantica decide se il gate puo
  procedere.
- **Debt list (durable)** — non-BLOCKER; do not reopen T5 hash:
  1. `contract_doc_sha256` tripwire (also in T5 SHA256SUMS; T5/T6 chain).
     Precomputed (contract @ `6d254d0a`):
     `6a6f6d35bbf3fc65d7a01e54620bf4f9649ea77d60c2b3d9e7ea0b72f7f49a86`.
     Tripwire against silent contract edits — not a G1a/G1b close criterion.
  2. **T6** fixture signal generators + hashes **OPEN** (contratto §14 G1a);
     fixture-spec v1 commit+hash precedes generation.
  3. **From M1 T5 CC** (hygiene, non-CRITICAL):
     (a) `.` path segments accepted in SHA256SUMS paths;
     (b) only ASCII space stripped — NBSP / unicode WS accepted;
     (c) commit-anchor SHA declared in PLAN/docstring, not a machine-checked
     constant in `sha256sums.py`.
- **Ancora assente / in corso**: G1a T6 (fixture generators/hash; after
  fixture-spec v1); product G1b only after G1a CLOSE (mandato open, prodotto
  non avviato; uncommitted `ml_v3/frontend/` = spike/feasibility probe, not
  gate proof); modello V3, runtime V3, UI V3, build Ableton V3, training V3.
  Ship-line (`Source/`, CMake, `Resources/`, `AIEQ-mac`) **0-diff** vs freeze
  G0 `2c88edad`. Eventuale G1b prodotto potra toccare `ml_v3/frontend/`
  (e solo minimum `Source/` se contract-authorized later) — **still no
  silent ship of V3 to Ableton** without later gates (G6+).
- **Nessun training V3 e autorizzato** oltre i limiti di fase.
- Il CONTROL Motore v2/A4b resta **NO-GO**; nessun modello e promosso.
- Il laboratorio prominence v2 vive su **branch separati** e NON e integrato in
  questo branch (vedi sezione "Stato prominence v2").

Le sezioni G1c–G8 seguenti restano criteri FUTURI finche la fase corrente non
riceve GO di chiusura. Non confondere "mandato G1b aperto nel PLAN" con
"product G1b started", "G1 PASS" o "release-safe".

Obiettivo finale: non inferiorita misurata rispetto a smart:EQ 4 sul
bilanciamento tonale e rispetto a soothe, Equator e Gullfoss sulle anomalie
dinamiche, con particolare peso a techno, house e breakbeat.

Motore v2 resta una baseline sperimentale riproducibile. Non modificare ship
line, plugin installato, APVTS, preset, `Resources/Models/ml_weights.bin`,
`feature/a4b-data-seed-grid` o `feature/unified-exp-clean`.

## Governance

- **Authority hierarchy (Motore-v3)**: GLOBAL / RELEASE →
  `ALIGNMENT_MANIFEST.md`; LAB STATE → questo PLAN; FROZEN G1 TECHNICAL →
  `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a`. Ordine di lettura task
  Motore-v3: PLAN → frozen phase contract → ALIGNMENT_MANIFEST (global /
  ship).
- **Stop-rule (unica, semantica)**: BLOCKER (a prescindere da
  CRITICAL/HIGH/MED/LOW) se abilita false PASS, final-test leakage, split
  leakage, hash/binding ambiguity, canonical nondeterminism, post-freeze
  gate-domain mutation, o non-implementability del contratto; altrimenti
  durable debt list. Severity = gravita; semantica = proceed/stop.
- Branch offline: `feature/motore-v3-offline`, basato su
  `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Integrazione futura: branch nuovo da
  `feature/unified-exp-clean@68ca31b43f5e523f63d70ce58a6cbf255760f82e`.
- Un solo implementatore scrive sul branch; il reviewer esamina un commit
  immutabile e restituisce un'unica lista consolidata di finding.
- Ogni fase definisce input e hash, file ammessi, output e schema, comandi,
  metriche, gate, stop condition e rollback.
- Ogni fase produce commit atomici e report numerico. La fase successiva non
  parte senza counter-check e GO esplicito.
- GO G1a — reviewer (Marco) su `6d254d0a` → fase aperta (2026-07-25).
  T1: `94dc9991`. T2: `1908fc45`. T2.1: `918b3dde`. T3: `9aa19295`.
  T4: `3bfd8aaf` (metrology lock;
  `metrology_lock_sha256` =
  `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`).
  T5: `75cb6902` (tip codice G1a; SHA256SUMS trust anchor). PLAN tip docs:
  `37f6ac60`. Stop-rule: vedi bullet semantico sopra (no automatic
  HIGH/MED→debt). T1–T5 = contract primitives only; **M1 T5 catch-up CC:
  CLEAN** on `75cb6902`; **T6 OPEN** (generatori fixture + hash per §14;
  fixture-spec v1 commit+hash before generation). **G1a CLOSE** after T6 +
  CC / Guardian GO, before official product G1b; no G1 PASS.
- GO G1b — mandato aperto in PLAN `37f6ac60` (2026-07-25) con REV7:
  amend solo se l'implementazione dimostra **impossibilita falsificabile**
  (non inconvenienza); un redteam + un CC indipendente per tranche;
  stop-rule semantica (stesso bullet sopra). **Product G1b not started**;
  uncommitted `ml_v3/frontend/` = spike / feasibility probe (not gate
  proof) until M3+M4. Nessun ship Ableton.
- Dati, cache e modelli restano in `~/aieq_data/motore_v3/`; nel repository
  entrano soltanto codice, manifest, lock, hash, contratti e report.
- Massimo tre round completi di training. Non si compensano fallimenti offline
  con threshold o routing runtime ad hoc.

## G0 - Freeze e riproduzione

- Tag annotato runtime:
  `checkpoint/motore-v2-runtime-5c9cb329-2026-07-19` su
  `5c9cb3290f87b62a339c6b2c49645b2b25524712`.
- Tag annotato scientifico:
  `checkpoint/motore-v2-a4b-final-88e70dd0-2026-07-19` su
  `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Congelare un ambiente v3 separato con versione Python completa, piattaforma,
  pacchetti e hash delle distribuzioni.
- Verificare in profondita gli hash audio di `train`, `heldout` e `test`, poi
  gli artefatti storici in `ml_v2/baselines/a4b_control/SHA256SUMS`.
- Il checkpoint corrente include la fixture test-only `fsld:46593`, ammessa
  dopo i primi report e responsabile del passaggio controllato da 834 a 835
  righe test. Il riferimento comportamentale corrente e il CONTROL 3x2
  `a4b_control_grid_20260718_85fa55ec`, non i tre report pre-fixture.
- Verificare l'hash del `SHA256SUMS` della griglia, rieseguire i candidati
  dataset-seed 42 per model seed 42, 1337 e 2026, normalizzare soltanto la riga
  `model=` e confrontare gli output con `eval_s*_d42.txt`.
- Il risultato atteso e A4b `NO-GO 0/3`: G0 passa se lo riproduce esattamente,
  non se il modello diventa verde.

## G1 - Frontend e benchmark

- Ricampionare il solo percorso di analisi a 48 kHz, fuori dal callback audio,
  prima dell'estrazione delle feature.
- Congelare 120 bande logaritmiche con centri fisici 20 Hz-20 kHz e durate
  temporali identiche a ogni sample rate.
- Usare i sette profili utente esistenti. Stato VERIFICATO sul codice corrente:
  il parametro host APVTS espone esattamente sette scelte - `Generic`, `Vocals`,
  `Drums`, `Bass`, `Synth`, `Master`, `EDM` (`Source/PluginProcessor.cpp:651`);
  `AIEngine::SourceProfile` possiede anche `Techno` con soglie proprie
  (`Source/AI/AIEngine.h:103-113`), ma il parametro host lo rende irraggiungibile
  perche clampa gli ID a `0..6` (`Source/PluginProcessor.cpp:1849`).
  La mappatura `Techno -> edm` e quindi una FUTURA policy dell'adapter V3 per i
  metadata di benchmark, NON il comportamento del codice attuale; APVTS resta
  invariato e non si aggiungono parametri host.
- Congelare split globalmente group-disjoint: `train`, `validation`,
  `calibration`, `development-metric`, `final-test`.
- Congelare evaluator, schema annotazioni, metriche e protocollo dei render
  competitor prima di qualunque training.
- Gate: determinismo, overlap zero, gain invariance e parity 44.1/48/96 kHz
  con max |Δ| ≤ 0.25 dB.

## G2 - Baseline deterministica

- Implementare profile matching tonale e detector tempo-frequenza per
  Resonance, Sibilance e Harshness senza ML.
- Produrre una curva tonale continua su 120 bande e al massimo otto filtri AI
  statici globali.
- Il fitting e azionabile soltanto con RMSE percettivamente pesato entro
  0.5 dB nelle regioni attive e p95 entro 1.5 dB; altrimenti e report-only.
- Gate: almeno 10% di miglioramento rispetto a v2 nelle metriche primarie,
  senza regressioni clean.

## G3 - Corpus e target reali

- Ammettere soltanto CC0, CC-BY o materiale OWNED con ledger completo e
  verifica hash fail-closed.
- Minimo per profilo: 500 sorgenti, 5 ore e 30 gruppi indipendenti; almeno il
  40% del test finale deve essere elettronica.
- Sintetici: 1-3 degradazioni simultanee fino a +/-12 dB; il target correttivo
  sicuro e esplicitamente limitato a +/-9 dB.
- Naturali: due annotazioni indipendenti e adjudication di curva correttiva a
  120 bande, regioni problematiche e actionability.
- Le sole etichette "problema presente" non sono supervisione sufficiente per
  la curva tonale.

## G4 - Selezione e training

- Prima del full training congelare un contratto di bake-off tra baseline DSP,
  TCN causale e modello spettro-temporale compatto.
- Tutti i candidati condividono input e output di G1; il contratto G4 definisce
  forme tensoriali, causalita, pooling e budget compute.
- Scegliere il modello piu semplice che supera G2 di almeno il 10% rispettando
  integralmente la clean safety.
- Training finale fattoriale: tre model seed per due dataset seed. Un model
  seed e verde soltanto se passa con entrambi i dataset seed; ne servono due.
- Confidence tonale: calibrazione su clean e positivi, massimizzando macro-F1
  con meno del 2% di clean actionable oltre 1 dB.
- Anomaly Engine: calibrazione separata con massimo 0.5 falsi eventi/minuto.
- Dopo tre round senza GO, resta il motore deterministico.

## G5-G8 - Validazione e prodotto

- G5: render ciechi loudness-matched per Marco. E un veto su artefatti e
  workflow, non una prova di non inferiorita.
- G6: integrazione suggestions-only dalla linea EXP pulita, esclusivamente con
  commit v3 controfirmati.
- G7: processing dinamico mediante `DynamicCorrectionEngine` esistente,
  default OFF e rollback immediato.
- G8: panel formale; numerosita stabilita con power analysis dal pilot, minimo
  12 ma non assunto sufficiente.
- Non inferiorita: CI 95% clusterizzata per ascoltatore e sorgente, con limite
  inferiore non peggiore di -10 punti percentuali.

## Interfacce e UX congelate

- Tipi pubblici futuri: `V3FeatureFrame`, `V3AnalysisSnapshot`,
  `V3TonalSuggestion`, `V3DynamicEvent`, `V3SuggestionBundle`.
- Ogni bundle contiene ID stabile, `ProblemType`, confidence, actionability,
  motivo di rifiuto e filtri statici oppure un evento dinamico.
- I filtri AI statici sono globalmente al massimo otto e appartengono a un solo
  bundle. FIX ALL deduplica e non sovrascrive bande manuali significative.
- Pannelli AI e Semantic, barre e curve ambra, Capture, FIX singolo, FIX ALL e
  undo/redo restano invariati.
- La diagnosi valuta la risposta prevista dopo EQ manuale e semantica, evitando
  di duplicare intenzioni come Warmth o Air.
- `AIEQ_ENABLE_MOTORE_V3` **non esiste nel codice corrente** (verificato: zero
  occorrenze in `Source/` e `CMakeLists.txt`). VERRA INTRODOTTO in una futura
  fase di integrazione, **OFF di default** nelle build normali. Il comportamento
  qui descritto e il contratto previsto per quel flag futuro, non lo stato
  attuale: nell'EXP, modello valido e `aiEnabled` attiveranno i suggerimenti;
  `dynamicCorrections` controllera soltanto l'audio dinamico. Anche i tipi
  pubblici `V3FeatureFrame`, `V3AnalysisSnapshot`, `V3TonalSuggestion`,
  `V3DynamicEvent` e `V3SuggestionBundle` sono nomi di contratto FUTURI: oggi
  esistono solo in questi documenti, non nel codice.
- Modello assente, corrotto o incompatibile: fallback completo alle euristiche
  correnti. Nessun nuovo parametro host.

## Gate finali e assunzioni

- G6 richiede parity Python/C++, build v3/no-v3, quattro runner, CTest con
  `--no-tests=error`, ASan/TSan, pluginval s8+s10 e Ableton.
- Installazione soltanto come `AI Equalizer Pro v3 EXP`; il plugin originale
  non viene toccato.
- Smoke locali su M1 Pro; cloud autorizzato per tranche soltanto dopo G2/G3.
- **STIME DI PROGRAMMA NON VALIDATE, non garanzie**: closed beta indicativamente
  4-6 mesi; prova credibile di parita top-tier indicativamente 6-12 mesi. Sono
  proiezioni di pianificazione, prive di evidenza sperimentale a supporto e
  soggette a revisione a ogni gate; non vanno citate come impegni.
- Release production NO-GO fino a G8 verde e zero P0 aperti.

## Stato prominence v2 (laboratorio separato, NON integrato qui)

Registrato per evitare che risultati di un altro branch vengano letti come
progressi del V3. Il laboratorio prominence vive su branch separati
(`feature/prominence-engine-phase1`, `feature/prominence-p0-p2`) e **non e
integrato in `feature/motore-v3-offline`**.

- **P0: PASS**, ma esclusivamente come probe DETERMINISTICO e riproducibile
  (due run bit-identiche). Non e una prova di qualita del motore.
- **P1: NO-GO** contro i criteri congelati, con questi numeri:
  - 100 Hz: monotonicita FALSA; dinamica `0.000395` < `0.03`;
    separazione picco/valle `0.000876` < `0.05`;
  - 2000 Hz: dinamica `0.019241` < `0.03`;
    separazione picco/valle `0.008332` < `0.05`.
- **P2 e P2-bis sono guardie** del valutatore e dell'hash di default: proteggono
  da errori di misura e da regressioni silenziose, NON sono prove di qualita.
- In questo branch **non e autorizzato nulla** di V3b, V4a, V4b o P0-bis.
- Il futuro V3 **non eredita automaticamente** l'encoding legacy invertito ne le
  sue scelte di finestra: la rappresentazione V3 va definita nei suoi contratti,
  non ereditata dal laboratorio v2.

Debt confermati del vecchio laboratorio prominence, che **non devono diventare
semantiche del futuro V3**:

1. Python applica `smoothingOctaves` dalla config, mentre il C++ prepara le
   `windowSizes` con il default;
2. `maxWidthBands` nel detector e in realta misurato in **bin FFT grezzi**, non
   in bande.
