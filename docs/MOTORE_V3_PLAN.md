# Motore v3 - Sviluppo a contratti per fase

## Stato e obiettivo

Questo e il piano canonico del laboratorio Motore v3. Congela obiettivo,
interfacce di prodotto, governance e criteri di successo; l'architettura ML
viene scelta soltanto dopo benchmark, baseline DSP e corpus controfirmati.

Obiettivo finale: non inferiorita misurata rispetto a smart:EQ 4 sul
bilanciamento tonale e rispetto a soothe, Equator e Gullfoss sulle anomalie
dinamiche, con particolare peso a techno, house e breakbeat.

Motore v2 resta una baseline sperimentale riproducibile. Non modificare ship
line, plugin installato, APVTS, preset, `Resources/Models/ml_weights.bin`,
`feature/a4b-data-seed-grid` o `feature/unified-exp-clean`.

## Governance

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
- Usare i sette profili utente esistenti; `SourceProfile::Techno` viene
  condizionato come EDM senza modificare APVTS.
- Congelare split globalmente group-disjoint: `train`, `validation`,
  `calibration`, `development-metric`, `final-test`.
- Congelare evaluator, schema annotazioni, metriche e protocollo dei render
  competitor prima di qualunque training.
- Gate: determinismo, overlap zero, gain invariance e parity 44.1/48/96 kHz
  entro 0.25 dB.

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
- `AIEQ_ENABLE_MOTORE_V3` resta OFF nelle build normali. Nell'EXP, modello
  valido e `aiEnabled` attivano i suggerimenti; `dynamicCorrections` controlla
  soltanto l'audio dinamico.
- Modello assente, corrotto o incompatibile: fallback completo alle euristiche
  correnti. Nessun nuovo parametro host.

## Gate finali e assunzioni

- G6 richiede parity Python/C++, build v3/no-v3, quattro runner, CTest con
  `--no-tests=error`, ASan/TSan, pluginval s8+s10 e Ableton.
- Installazione soltanto come `AI Equalizer Pro v3 EXP`; il plugin originale
  non viene toccato.
- Smoke locali su M1 Pro; cloud autorizzato per tranche soltanto dopo G2/G3.
- Closed beta stimata in 4-6 mesi; prova credibile di parita top-tier in
  6-12 mesi.
- Release production NO-GO fino a G8 verde e zero P0 aperti.
