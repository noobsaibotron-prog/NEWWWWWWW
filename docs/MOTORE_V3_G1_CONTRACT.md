# Motore v3 - Contratto G1 frontend e benchmark

Stato: PROPOSTA IMMUTABILE PER COUNTER-CHECK, REVISIONE 4. Questo documento non autorizza
ancora l'implementazione. G1 parte soltanto dopo il GO del reviewer sul commit
che contiene esclusivamente questo file.

Allineamento all'audit 2026-07-23: **nulla di questo contratto e implementato**.
Non esistono frontend V3, modello V3, runtime V3, UI V3 o build Ableton V3;
nessun training V3 e autorizzato; `AIEQ_ENABLE_MOTORE_V3` non esiste ancora nel
codice e verra introdotto OFF di default in una futura fase di integrazione.
G0 e PASS soltanto come freeze riproducibile della baseline NEGATIVA (il CONTROL
Motore v2/A4b resta NO-GO). **Tutti i gate elencati qui sotto sono criteri
FUTURI da soddisfare: nessuno e PASS, e nessuno va rilassato.**

## 1. Scopo e risultato atteso

G1 congela il sistema di misura usato da tutte le fasi successive. Deve
eliminare tre ambiguita che hanno reso il Motore v2 difficile da addestrare e
da valutare:

1. la durata delle feature non deve cambiare con il sample rate dell'host;
2. colore tonale largo, picchi locali e variazioni temporali non devono essere
   compressi nello stesso singolo canale;
3. training, calibrazione, selezione del round e test finale non devono
   condividere gruppi o responsabilita.

Output G1:

- frontend Python deterministico e streaming-equivalent;
- fixture sintetiche congelate con hash;
- split contract v3 fail-closed;
- schema di annotazione e schema dei risultati;
- evaluator deterministico per curve tonali e anomalie;
- protocollo di render e confronto competitor;
- report numerico dei gate G1.

G1 non allena modelli, non sceglie l'architettura ML e non dimostra parita con
un competitor. Costruisce il metro con cui queste claim potranno essere
falsificate.

## 2. Perimetro e protezioni

Durante G1 sono ammessi soltanto:

- `docs/MOTORE_V3_G1_CONTRACT.md`;
- `ml_v3/frontend/`;
- `ml_v3/benchmark/`;
- `ml_v3/contracts/`;
- `ml_v3/fixtures/g1/` per manifest, generatori e piccoli artefatti test;
- `ml_v3/tests/`;
- `ml_v3/environment/` solo per aggiornare il lock con dipendenze necessarie;
- `ml_v3/reports/G1_FRONTEND_BENCHMARK_REPORT.md`.

Sono vietati:

- `Source/`, `CMakeLists.txt`, `Resources/`, `ml_v2/` e `AIEQ-mac/`;
- pesi, training, threshold appresi o routing runtime;
- APVTS, preset, GUI, pannelli AI/Semantic e plugin installati;
- modifiche ai branch o tag protetti di G0;
- uso del `final-test` per scegliere feature, soglie o implementazione.

Tutti gli artefatti grandi restano sotto `~/aieq_data/motore_v3/g1/`. Nel
repository entrano solo fixture piccole, manifest, contratti, hash e report.

## 3. Riferimenti autorevoli e debiti da non ereditare

- Base scientifica: `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Freeze G0: `2c88edad489c02b01d97b0d46cb7c206e33937bb`.
- Il test v2 a 44.1 kHz e un testimone utile del metodo di parity, non una
  specifica v3.
- `PerceptualFrontEnd` corrente e diagnostics-only. Il suo LF usa l'ultimo
  frame 8192 disponibile, non allineato al frame 4096, e le durate cambiano
  col sample rate. G1 non puo replicare questi due comportamenti.
- Il contratto A4b separa calibration e metric per group, ma contiene ruoli
  legacy e solo quattro destinazioni. G1 parte con un nuovo contratto a cinque
  ruoli e nessun fallback legacy.
- L'evaluator A6 basato su occupancy e singoli hit resta storico. Nessun
  criterio G1 puo essere soddisfatto da un solo hit o da una sola clip.

## 4. Contratto di ingresso

### 4.1 Audio

Il frontend accetta array float32 mono o stereo interleaving-independent ai
sample rate host comuni `44100`, `48000`, `88200`, `96000`, `176400` e
`192000` Hz. I gate obbligatori G1 coprono 44100, 48000 e 96000 Hz; gli altri
tre sono supportati e report-only fino a un gate dedicato successivo. Ogni
altro sample rate viene rifiutato, non reinterpretato.

Regole fail-closed:

- zero canali, piu di due canali, sample rate non valido, NaN o Inf: errore;
- mono: `mid = input`, `side` non valido;
- stereo: `mid = (left + right) / 2`, `side = (left - right) / 2`;
- nessun limiter, normalizzatore o dither implicito;
- nessun padding finale; lo stato iniziale e zero e i frame incompleti non
  vengono emessi;
- ogni output conserva `asset_id`, `group_id`, sample rate originale e hash
  SHA-256 dell'audio sorgente.

### 4.2 Profili

I soli profili condizionanti sono i sette esposti oggi dall'APVTS:

`generic`, `vocals`, `drums`, `bass`, `synth`, `master`, `edm`.

Questo e anche l'ordine canonico degli ID `0..6`; stringa e ID devono
concordare o il record viene rifiutato.

Stato VERIFICATO sul codice corrente: queste sette scelte sono esattamente
quelle esposte dal parametro host APVTS (`Source/PluginProcessor.cpp:651`), e il
parametro clampa gli ID a `0..6` (`Source/PluginProcessor.cpp:1849`).
L'enum interno `AIEngine::SourceProfile` contiene ANCHE `Techno` con soglie
proprie (`Source/AI/AIEngine.h:103-113`), ma il clamp lo rende irraggiungibile
dall'host: **oggi nessun conditioning `Techno -> edm` avviene nel codice**.
La mappatura `Techno -> edm` e una FUTURA policy dell'adapter V3 per i metadata
di benchmark. Techno, house, breakbeat e altri sottogeneri restano metadati di
benchmark separati e obbligatori quando noti; non aggiungono un parametro host e
non cambiano il numero dei profili.

## 5. Ricampionamento canonico

L'analisi avviene sempre a 48000 Hz. Il ricampionamento e parte del contratto,
non una pre-elaborazione libera del chiamante.

Per un input `fs_in`, ridurre la frazione:

```text
g = gcd(fs_in, 48000)
up = 48000 / g
down = fs_in / g
```

Se `up == down == 1`, il ricampionatore e l'identita, con ritardo zero. Negli
altri casi la reference Python usa un FIR low-pass polyphase causale con:

- numero tap `128 * max(up, down) + 1`;
- finestra Kaiser `beta = 9.0`;
- passband richiesta fino a 20000 Hz e stopband da
  `min(fs_in, 48000) / 2`;
- cutoff al punto medio fra i due estremi, espresso in cicli/campione alla
  frequenza intermedia `fs_in * up`;
- guadagno dei coefficienti moltiplicato per `up`;
- stato streaming conservato tra blocchi;
- ritardo di gruppo dichiarato nei metadati, mai eliminato con look-ahead;
- nessun padding o riflessione ai bordi.

Con `pass_hz = 20000`, `stop_hz = min(fs_in, 48000)/2`,
`fc = ((pass_hz + stop_hz)/2) / (fs_in*up)` e `M = num_taps - 1`, i
coefficienti sono:

```text
h0[n] = 2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0)
h[n]  = up * h0[n] / sum(h0), n = 0..M
```

`sinc(x) = sin(pi*x)/(pi*x)`. La fase polyphase e zero: l'output `j` e
`sum_n input[n] * h[j*down - n*up]`, considerando zero gli indici di `h`
fuori `[0, M]`. Per `N` campioni di input si emettono gli indici
`0 <= j < ceil(N*up/down)`; alla fine non si emette la coda ulteriore del
filtro.

Il ritardo fisico e la frazione esatta
`(num_taps - 1) / (2 * up * fs_in)` secondi. Ogni frame espone sia l'indice
intero nel flusso canonico sia il tempo sorgente razionale:

```text
source_time = frame_end_sample / 48000 - resampler_group_delay_seconds
```

Numeratore e denominatore vengono conservati come interi nei metadati. I test
fra sample rate si allineano su `source_time`, non sull'indice di output grezzo.

Il generatore dei coefficienti, la versione della libreria numerica e gli hash
delle fixture entrano nel lock G1. La futura implementazione C++ puo usare una
struttura diversa, ma deve riprodurre le feature entro i gate di parity; non
puo cambiare il comportamento osservabile.

## 6. Griglia e temporizzazione

### 6.1 Griglia fisica

Esistono esattamente 120 centri, inclusivi, da 20 a 20000 Hz:

```text
center[i] = 20 * (20000 / 20) ** (i / 119), i = 0..119
```

Le bande sono triangolari sull'asse `log2(f)`. Il supporto interno della banda
`i` va dal centro precedente al centro successivo; per le due bande estreme si
usa un centro virtuale ottenuto con lo stesso rapporto geometrico. DC e bin
oltre 20000 Hz non contribuiscono.

Per ogni FFT si calcola PSD one-sided in potenza:

```text
psd[k] = edge_factor[k] * abs(FFT(window * x)[k])**2
         / (48000 * sum(window**2))
```

`edge_factor` vale 1 a DC/Nyquist e 2 negli altri bin. L'energia di banda e la
media pesata lineare della PSD; conversione in dB soltanto dopo media e fusione.
Il floor e `1e-12`, poi clamp a `[-120, +12]` dBFS/Hz.

### 6.2 Due risoluzioni, stesso timestamp

- percorso MAIN: FFT 4096;
- percorso LF: FFT 8192;
- finestra Hann periodica `0.5 - 0.5*cos(2*pi*n/N)`;
- hop comune 1024 campioni canonici, cioe 21.333333 ms;
- finestre causali right-aligned allo stesso `frame_end_sample` esclusivo;
- primo frame soltanto quando sono disponibili 8192 campioni reali;
- timestamp in campioni canonici e secondi razionali, non in float.

La fusione avviene in potenza:

- LF puro fino a 160 Hz;
- MAIN puro da 320 Hz;
- crossfade raised-cosine su `log2(f)` fra 160 e 320 Hz.

Nel crossfade il peso LF e
`0.5 * (1 + cos(pi * log2(f/160) / log2(320/160)))`; il peso MAIN e il suo
complemento.

Non e consentito riusare l'ultimo frame LF. Per ogni frame MAIN deve esistere
un frame LF con lo stesso `frame_end_sample`.

## 7. V3FeatureFrame

G1 congela una superficie fisica, non il tensore del modello. G4 potra
selezionare o impilare questi campi senza cambiarne il significato.

Ogni frame contiene:

- `schema = "aieq-v3-feature-frame-1"`;
- `frame_end_sample`, `frame_index`, `source_time_num`, `source_time_den`,
  `canonical_sample_rate = 48000`;
- `mid_psd_db[120]`;
- `side_psd_db[120]`, `mid_valid` e `side_valid`;
- `mid_shape_db[120]` e `side_shape_db[120]`;
- `mid_prominence_db[120]` e `side_prominence_db[120]`;
- `mid_delta_db[120]` e `side_delta_db[120]`;
- `mid_level_dbfs`, `side_level_dbfs`;
- `valid` e un motivo enumerato quando falso.

Definizioni:

- `shape_db`: PSD normalizzata sottraendo
  `10*log10(sum(10**(psd_db/10)))` sulle 120 bande;
- `prominence_db`: `mid_shape_db` meno la sua convoluzione gaussiana su asse
  log, sigma 4 bande, kernel `exp(-0.5*(j/4)**2)` per `j=-16..16`, normalizzato
  a somma uno, con padding reflect;
- `delta_db`: shape corrente meno shape precedente, clamp `[-24, +24]` dB;
  sul primo frame valido e un vettore di zeri; un asset boundary o un frame
  globalmente non valido azzera la storia, quindi il successivo valido riparte
  da zero;
- `level_dbfs`: RMS time-domain non finestrato della finestra MAIN, floor
  -120 dBFS;
- `mid_valid`/`side_valid` richiedono livello del rispettivo canale almeno
  -100 dBFS; `valid` e il loro OR. Per input mono `side_valid` e sempre falso.
  I vettori di un canale non valido sono al floor e vengono ignorati. Un frame
  globalmente non valido non puo generare suggerimenti o eventi.

Non sono parte di G1: lunghezza della sequenza, pooling, receptive field del
modello, hidden size, logits, threshold o confidence calibration.

## 8. Split contract v3

### 8.1 Ruoli

Ogni gruppo ha esattamente uno dei cinque ruoli globali:

- `train`: aggiornamento dei pesi;
- `validation`: early stopping e scelta iperparametri interna a un round;
- `calibration`: soglie e calibrazione, dopo il freeze dei pesi;
- `development-metric`: scelta fra round e accettazione G2/G4;
- `final-test`: verifica sigillata del candidato congelato e benchmark
  competitor; mai usato per decisioni precedenti.

Quota attesa per strato: 55%, 10%, 10%, 10%, 15% nello stesso ordine. Le
quote non autorizzano un corpus insufficiente: G3 deve avere per ogni profilo
almeno 10 gruppi train, 3 validation, 3 calibration, 3 development-metric e 5
final-test. Almeno il 40% del `final-test` complessivo deve essere elettronica.
Questi sono floor generali di split, non sufficienti per calibrare o validare
una famiglia: i supporti piu severi delle sezioni 10.4 e 11 prevalgono.

### 8.2 Identita e assegnazione

`group_id` rappresenta la piu piccola unita conservativa che racchiude tutte
le dipendenze note: composizione, registrazione/sessione o artista; se questa
identita non e ricostruibile, si usa l'intero pack. Alias della stessa entita
fra sorgenti diverse vengono riconciliati prima dell'ammissione. Ogni gruppo ha
metadati immutabili `group_primary_profile`, `group_primary_domain` e
`source_family`. Stem, mix, versioni, crop, augmentation, injection, render
processati e render competitor ereditano lo stesso gruppo del dry originale.

`group_id` non e testo libero. Se la sorgente pubblica un ID stabile di
composizione, sessione o artista, l'ID canonico e
`source_family + ":" + upstream_id`. Se manca, l'intero pack forma un solo
gruppo con ID `source_family + ":pack:" + SHA256(sorted_audio_sha256)`. Mapping
di alias, upstream snapshot e regole di inclusione sono versionati prima dello
split; rinominare un gruppo o cambiare profilo/dominio dopo l'assegnazione
invalida il batch.

Regole:

- nessun fallback legacy: ogni gruppo e assegnato esplicitamente;
- nessun gruppo o SHA audio puo comparire in due ruoli;
- near-duplicate detection e obbligatoria prima di G3, ma non sostituisce
  l'identita di gruppo;
- ogni admission batch usa un protocollo commit-reveal: prima di vedere il
  roster il reviewer genera 32 byte casuali `salt` e committa
  `SHA256("aieq-v3-split-salt-v1" + NUL + salt)`; il curatore materializza e
  committa il roster completo di tutti i gruppi eleggibili della source
  snapshot, in ordine canonico, senza ruoli; soltanto allora il reviewer rivela
  `salt` e il tool verifica il commitment;
- `admission_batch_id` e lo SHA-256 dei byte canonici del roster pre-split. Il
  ruolo usa i primi 8 byte unsigned big-endian di
  `HMAC-SHA256(salt, "aieq-v3-role-v1" + NUL + admission_batch_id + NUL +
  group_primary_profile + NUL + group_primary_domain + NUL + source_family +
  NUL + group_id)`, divisi per `2**64`, con intervalli `[0,.55)`, `[.55,.65)`,
  `[.65,.75)`, `[.75,.85)`, `[.85,1)` nell'ordine dichiarato;
- un batch rivelato non puo essere filtrato dopo aver visto i ruoli: viene
  ammesso interamente oppure resta registrato come rifiutato soltanto per una
  regola fail-closed preregistrata e verificabile senza leggere i ruoli; i suoi
  gruppi non possono rientrare sotto un altro ID o batch;
- strato di verifica minimo: `(group_primary_profile, group_primary_domain,
  source_family)`; il preflight pubblica conteggi per strato e si ferma se i
  minimi per profilo non sono raggiunti;
- roster, commitment, reveal, batch e assegnazioni sono append-only; tentativi
  multipli, salt riutilizzati, subset di source snapshot o metadati mutati
  bloccano il preflight;
- un gruppo multi-dominio riceve un solo ruolo globale;
- una sorgente ammessa da v2 viene riammessa esplicitamente nel contratto v3;
  non eredita il fallback A4b.

Audio e annotazioni `final-test` vivono sotto una root separata. I loader di
training, validation, calibration e development rifiutano quel ruolo anche se
il path viene passato esplicitamente. L'evaluator finale richiede commit,
contratto e hash candidato gia congelati e registra l'apertura nel report.

Il trainer futuro deve rifiutare un contratto non committato, una modifica
retroattiva, un gruppo senza ruolo o un asset il cui hash non corrisponde.

## 9. Manifest e annotazioni

### 9.1 Asset manifest

Campi obbligatori:

```text
schema, asset_id, relative_path, sha256, group_id, admission_batch_id,
split_role, benchmark_families, development_pilot,
source_profile, primary_domain, group_primary_profile, group_primary_domain,
source_family, electronic_subgenre,
sample_rate, channels, duration_s, parent_asset_id, derivative_kind,
license_class, license_url, attribution, ledger_id
```

`benchmark_families` e una lista ordinata senza duplicati presa esclusivamente
da `tonal-controlled`, `tonal-natural`, `anomaly-natural`, `clean-safety` ed
`electronic-stratified`. `development_pilot` e booleano, puo essere vero
soltanto nello split `development-metric` ed e assegnato dopo lo split con
`HMAC-SHA256(salt, "aieq-v3-pilot-v1" + NUL + admission_batch_id + NUL +
group_id)`, interpretato unsigned big-endian e diviso per `2**256`, minore di
0.25. Il pilot e group-disjoint dal punto-estimate
development dello stesso round; se non raggiunge i supporti richiesti si
ammettono nuovi batch, non si cambia la soglia 0.25.

La membership benchmark viene congelata dopo l'adjudication ma prima di
eseguire qualunque candidato sul ruolo. Dipende soltanto da provenance e
annotazioni, mai da prediction o metrica; modificarla dopo il freeze invalida
manifest e report.

Ogni batch possiede inoltre un record `aieq-v3-admission-batch-1` con source
snapshot, regole di inclusione, roster SHA-256, salt commitment/reveal, commit
del roster, reviewer e stato admitted/rejected. Il manifest viene rifiutato se
questo record manca o non ricostruisce esattamente ruoli e pilot flag.

Licenze ammesse: CC0, CC-BY o OWNED con ledger completo. Campo mancante,
licenza sconosciuta o hash errato bloccano il preflight.

### 9.2 Annotation record

Ogni record usa `schema = "aieq-v3-annotation-1"` e contiene:

- `asset_id`, `annotator_id`, `pass_id`, `profile`,
  `evaluation_unit_id`, `segment_start_s`, `segment_end_s`;
- `tonal_correction_db[120]`: EQ correttiva desiderata; segno positivo =
  boost, negativo = cut;
- `tonal_confidence[120]` e `tonal_actionable_mask[120]`;
- `semantic_regions[]` per gli otto tipi pubblici correnti;
- `dynamic_events[]` per Resonance, Harshness e Sibilance;
- `complete_types[]`, `explicit_negative_types[]`, `global_actionable`,
  `clean_for_action`, note e versione tool.

Gli otto tipi e ID canonici restano, nell'ordine `0..7`: `Resonance`,
`Harshness`, `Muddiness`, `Sibilance`, `Boominess`, `Thinness`,
`BoxyMidrange`, `DullSound`. Un ID che non concorda con la stringa e invalido.

Ogni semantic region contiene tipo, inizio/fine, banda inferiore/superiore,
direzione, severity, confidence e actionability. Ogni evento dinamico contiene
tipo, inizio/fine, centro, larghezza in ottave, severity, confidence e
actionability.

Per Thinness e DullSound la direzione spettrale e obbligatoria; per Muddiness,
Boominess e BoxyMidrange e obbligatoria una banda; per Resonance e obbligatorio
il centro. Una sola label di presenza senza curva, regione o evento non e una
supervisione tonale valida.

Curve e prediction pubbliche sono finite e limitate a `[-9, +9]` dB;
confidence e severity sono in `[0, 1]`; tempi e frequenze devono cadere nel
segmento e in 20-20000 Hz. `clean_for_action = true` impone curva zero,
`tonal_actionable_mask` tutto falso, `global_actionable = false` e nessun
evento actionable; impone inoltre tutti gli otto tipi canonici dentro
`complete_types` ed `explicit_negative_types`. Fuori da questo caso, un tipo
entra in `complete_types` soltanto quando l'intero segmento e stato annotato
esaustivamente per quel tipo; entra in `explicit_negative_types` soltanto se e
completo e gli annotatori ne hanno verificato l'assenza. Le aree fuori dagli
eventi GT valgono come negative soltanto per tipi completi. Un suono colorato
ma intenzionalmente corretto e clean non viene trasformato in hard-negative di
un problema diverso.

`evaluation_unit_id` e congelato prima di eseguire i sistemi. Segmenti che
derivano dallo stesso intervallo annotato condividono l'ID e possono contribuire
una sola volta; dividere, duplicare o sovrapporre un segmento dopo il freeze
invalida il record.

G3 definira processo a due annotatori e adjudication. G1 congela formato e
semantica, non inventa annotazioni reali.

### 9.3 Prediction record

Ogni prediction usa `schema = "aieq-v3-prediction-1"` e contiene `asset_id`,
`model_id`, hash del modello e del frontend contract, `calibration_policy_id`,
SHA-256 della policy, profilo, curva tonale a 120 bande, `tonal_score[120]`
pre-calibrazione, `tonal_confidence[120]` calibrata, `segment_start_s`,
`segment_end_s`, bundle semantici e lista eventi. Ogni bundle ed evento porta
confidence e `actionable`.

La policy usa `schema = "aieq-v3-calibration-policy-1"` e contiene hash di
modello, frontend, calibration manifest e prediction schema; ID/versione,
algoritmo e parametri completi dei calibratori tonal e anomaly;
`tonal_band_thresholds[120]`, `semantic_type_thresholds[8]` e
`anomaly_class_thresholds[3]`; versione dell'estrattore di candidati e regole
deterministiche score-to-confidence, region-to-bundle e threshold-to-actionable.
Ogni mapping score-to-confidence e monotono non decrescente e definito anche
agli estremi zero e uno.
Viene fittata soltanto su `calibration` dopo il freeze dei pesi e committata
prima di aprire `development-metric`. Ogni modifica a calibratore, threshold o
decision rule cambia SHA-256.

Ogni prediction contiene anche `anomaly_score_ref` e
`anomaly_severity_ref`, riferimenti con SHA-256 ad array little-endian float32
di forma `[num_feature_frames, 3, 120]`, ordine classi `Resonance`, `Harshness`,
`Sibilance`, allineati ai frame G1. Sono superfici dense finite in `[0,1]`
prima di threshold, hysteresis, top-k o veto. Frame/bande invalidi sono marcati
da una mask separata e non possono essere omessi; una lista eventi senza queste
superfici e schema invalido.

L'evaluator applica prima la policy alla superficie score e deriva una lista
completa di candidati dalla confidence calibrata, prima della soglia. Per ogni
classe prende i massimi locali positivi nel vicinato 3x3 tempo-banda: confidence
maggiore o uguale a tutti i vicini e maggiore di almeno un vicino esterno al
proprio plateau connesso. Per ogni plateau conserva soltanto la coordinata
frame/banda minima.
In ordine decrescente di confidence, ogni massimo genera la componente 8-neighbour
che lo contiene nella mask `confidence >= 0.5 * peak`; massimi successivi la cui
componente contiene gia un massimo conservato vengono soppressi. Ogni componente
produce onset/offset dai frame estremi, banda dagli estremi di banda, centro
come media geometrica dei centri pesata dalle confidence, confidence uguale al peak
e severity come media pesata della superficie severity.

Gli eventi pubblicati alla soglia operativa sono esattamente i candidati con
confidence calibrata almeno pari alla soglia; devono coincidere con questa
estrazione o la prediction e invalida. La PR-AUC ordina l'intera lista
pre-threshold per `(confidence desc, frame, band)` e ripete il matching ai suoi
cut-point. Nessun top-k, floor di confidence o veto puo nascondere un candidato.
L'evaluator carica la policy per hash, ricalcola confidence, bundle, eventi e
flag `actionable` dai valori pre-calibrazione e rifiuta qualunque differenza con
la prediction. Una policy assente, non committata o fittata su un ruolo diverso
da `calibration` invalida il report.

## 10. Evaluator deterministico

L'evaluator legge soltanto manifest, annotation record e prediction record
con schema/versione/hash compatibili. L'ordine dei file non puo cambiare i
risultati. Tutte le aggregazioni pubblicano valore globale, macro per profilo,
macro per dominio e CI percentile 95% da 10000 bootstrap a livello `group_id`,
con PCG64 seed 20260719. Il campionamento conserva tutti gli asset figli del
gruppo estratto.

Salvo i rate exposure-aware dichiarati separatamente, ogni metrica primaria
viene calcolata prima per `evaluation_unit_id`, poi mediata dentro `group_id` e
infine macro-mediata con peso uguale fra gruppi. Numero di asset, segmenti,
eventi o celle non aumenta il peso del gruppo. Le micro-medie vengono riportate
solo come diagnostica.

### 10.1 Metriche tonali

Per una prediction `p[120]` e target `t[120]`, i pesi sono:

```text
w[i] = annotation_confidence[i] * tonal_actionable_mask[i]
       * audible_mask[i]
```

`audible_mask[i]` vale 1 quando la mediana temporale della PSD dry della banda
nel segmento annotato e almeno -100 dBFS/Hz e non oltre 60 dB sotto il massimo
del segmento; altrimenti 0.

Metriche obbligatorie:

- MAE e RMSE pesate della curva;
- p95 dell'errore assoluto sulle celle attive;
- miglioramento residuo:
  `1 - RMSE(t - p) / max(RMSE(t), 1e-6)`;
- errore di segno sulle celle con `abs(t) >= 1 dB`;
- clean actionable rate: quota di gruppi `clean_for_action` con almeno un
  bundle marcato actionable e oltre 1 dB in qualunque regione udibile;
- copertura: quota di target actionable per cui il motore emette un bundle
  azionabile;
- MAE della severity `[0,1]` sui bundle semantici matched e Spearman rho se il
  supporto e almeno 10; sotto quel supporto rho e `N/A`.

Un record senza celle attive ha metriche di curva `N/A`, non zero; viene usato
per clean safety. Ogni macro-media pubblica anche il supporto ed esclude gli
`N/A` senza convertirli in PASS.

Una semantic region e matchabile solo con stesso tipo e IoU temporale almeno
0.3. Il matching per classe non usa una frequenza puntuale universale:

- Resonance: centro entro un terzo di ottava;
- Muddiness, Boominess, BoxyMidrange: overlap di banda almeno 0.5;
- Thinness, DullSound: stessa regione e stessa direzione spettrale;
- Harshness e Sibilance: overlap di banda almeno 0.5.

### 10.2 Metriche degli eventi

Un evento e matchabile solo se tipo uguale e IoU temporale almeno 0.3. Inoltre:

- Resonance: centro entro un terzo di ottava;
- Harshness/Sibilance: overlap di banda almeno 0.5.

Il matching e bipartito one-to-one con obiettivo lessicografico deterministico:
massimo numero di match validi, poi massima somma IoU, poi minima somma
dell'errore frequenziale in ottave, poi ordine crescente degli ID evento. Non
e ammesso un greedy dipendente dall'ordine dei record.
Metriche obbligatorie:

- precision, recall, F1 e area precision-recall per classe, macro a peso uguale
  sui gruppi con annotazione completa; un gruppo senza GT positivo ha PR-AUC
  `N/A` e contribuisce invece alla clean safety;
- errore centro in ottave per Resonance;
- errore onset e offset in millisecondi;
- MAE della severity `[0,1]` sugli eventi matched e Spearman rho con supporto
  almeno 10;
- falsi eventi al minuto su gruppi clean;
- durata e occupancy soltanto come diagnostica, mai come gate primario.

Le metriche evento vengono prima calcolate per gruppo usando le superfici dense
pre-threshold della sezione 9.3 e poi macro-mediate con peso uguale. Segmenti,
eventi o durate aggiuntive dello stesso gruppo non aumentano il peso del gruppo.
Si riportano anche le micro-metriche come diagnostica, ma non possono promuovere
una classe.

Nessun singolo file, singolo hit o threshold scelto sullo stesso split puo far
passare una classe.

### 10.3 Calibrazione delle confidence

Tonal e anomaly hanno calibratori, report e parametri separati. I calibratori
si fittano solo su `calibration`; ECE, Brier e PR-AUC vengono pubblicati su
`development-metric` e poi, una sola volta, su `final-test`.

Per la confidence tonale, ogni cella udibile e un esempio binario actionable/
non-actionable. Per anomaly, ogni cella valida delle superfici dense e un
esempio: target uno dentro un evento GT del tipo e zero fuori, ma soltanto
quando il tipo e in `complete_types`. I ground-truth mancati restano FN nelle
metriche evento.

Ogni gruppo riceve peso totale uno. Dentro un gruppo, ciascun
`evaluation_unit_id` unico riceve peso `1 / num_units`; dentro l'unita, il peso
viene diviso uniformemente fra le celle eleggibili. Duplicati, crop e derivati
con lo stesso `evaluation_unit_id` vengono deduplicati prima del fit e della
misura. Questo schema di pesi e identico per fit del calibratore, ECE e Brier.

ECE usa 15 bin equal-mass: stable sort per `(confidence, group_id,
evaluation_unit_id, frame_or_band_id)`, poi partizione per peso cumulativo con
deviazione minima da `1/15`. Ogni bin usa accuratezza e confidence pesate; ECE
e la somma `mass_bin * abs(accuracy_bin - mean_confidence_bin)`. Brier e la
media pesata di `(confidence - target)**2`. Bin vuoti non vengono creati e
supporto zero e `N/A`, mai PASS.

### 10.4 Copertura minima per calibrazione e misura

L'unita indipendente di supporto e sempre `group_id`. Celle, eventi, crop,
derivati e asset multipli dello stesso gruppo aumentano il numero di esempi ma
non il supporto indipendente. I conteggi vengono pubblicati prima di fittare
qualunque calibratore. La stessa annotazione non puo essere usata come positivo
e negativo per la stessa famiglia.

La calibrazione globale e ammessa soltanto con questa copertura minima:

| Famiglia | Positivi indipendenti | Negativi indipendenti | Copertura obbligatoria |
|---|---:|---:|---|
| Tonale | almeno 50 gruppi con almeno una cella `tonal_actionable_mask` vera e target non zero, di cui almeno 20 con cella positiva e 20 con cella negativa | almeno 50 gruppi `clean_for_action` | almeno 3 actionable e 3 clean per ciascuno dei sette profili |
| Resonance | almeno 30 gruppi con evento GT actionable | almeno 50 gruppi senza evento GT della classe, di cui almeno 30 clean | positivi da almeno 3 `source_family`, almeno 5 gruppi ciascuna e nessuna oltre il 50%; negativi almeno 3 per profilo |
| Harshness | almeno 30 gruppi con evento GT actionable | almeno 50 gruppi senza evento GT della classe, di cui almeno 30 clean | positivi da almeno 3 `source_family`, almeno 5 gruppi ciascuna e nessuna oltre il 50%; negativi almeno 3 per profilo |
| Sibilance | almeno 30 gruppi con evento GT actionable | almeno 50 gruppi senza evento GT della classe, di cui almeno 30 clean | positivi da almeno 3 `source_family`, almeno 5 gruppi ciascuna e nessuna oltre il 50%; negativi almeno 3 per profilo |

Un gruppo multi-label puo contribuire al supporto positivo di piu classi, ma
una sola volta per classe. Un gruppo negativo per una classe puo contenere
un'altra anomalia soltanto se l'annotazione esclude esplicitamente la classe in
esame; l'assenza di annotazione non vale come negativo. Le direzioni positive e
negative della curva sono sotto-strati dello stesso supporto actionable: non
valgono come gruppi negativi e un gruppo si conta una sola volta nel totale 50.

I calibratori restano globali salvo prova contraria in G4. Una soglia o un
calibratore specifico per profilo, dominio o sottogenere e vietato se quello
strato non contiene almeno 30 gruppi positivi e 30 negativi nello split
`calibration`. Se il supporto manca, la classe o lo strato e `N/A` e non puo
contribuire a un GO; non si accorpano split e non si abbassano i minimi.

Su `development-metric` e `final-test` il supporto minimo dipende dalla metrica:
curve e coverage tonali richiedono almeno 30 gruppi actionable; clean actionable
rate almeno 149 gruppi clean; ogni classe anomaly almeno 30 gruppi positivi e
l'intero pool clean-safety da almeno 149 gruppi/minuti della sezione 11. Si
applicano inoltre i floor della sezione 11 e la sua power analysis.
Supporto inferiore produce `N/A` e blocca ogni claim o gate che dipende da quella
metrica. ECE/Brier possono essere riportati anche con supporto maggiore di zero,
ma non valgono come prova di calibrazione finche questi minimi non sono
raggiunti.

### 10.5 Comparabilita delle baseline e semantica `N/A`

Un sistema viene confrontato soltanto sulle superfici che produce realmente
con lo stesso schema, oppure tramite un adapter congelato e controfirmato prima
di aprire `development-metric`. L'assenza strutturale di curve tonali, eventi,
frequenza o severity nel Motore v2 vale `N/A`, non zero, infinito o FAIL.

`N/A` non entra in macro-medie, non soddisfa un gate e non dimostra un
miglioramento. Le claim relative a v2 usano soltanto metriche omologhe o un
surrogato preregistrato; le nuove capacita v3 vengono valutate in assoluto e
contro la baseline deterministica G2. Se invece un candidato dichiara una
superficie ma non emette una prediction valida, l'esito e fail-closed: FN,
errore di schema o fallimento del candidato secondo il caso, mai `N/A`.

Il gate G2 rispetto a v2 usa un solo adapter omologo, congelato in G1c, sulle
sei classi realmente attive in tutti e tre i candidati G0: `Resonance`,
`Muddiness`, `Boominess`, `Thinness`, `BoxyMidrange`, `DullSound`.
`Harshness` e `Sibilance` sono mascherate nelle provenance G0 e restano `N/A`
nel confronto v2; v3 le deve superare con i gate assoluti e contro G2, mai
trattandole come negativi v2. Il denominatore della macro-F1 e sempre sei; una
delle sei classi con supporto insufficiente rende il gate NO-GO.

Per ogni segmento completo con almeno 20 finestre v2, l'adapter usa i centri
fisici delle finestre prodotte dal frontend G0 con `window_step=16` come griglia
temporale comune. A ogni centro:

- v2 vale uno se la probabilita supera la soglia della provenance del seed;
- v3 vale uno se il centro cade nel supporto temporale di un bundle o evento
  `actionable` della classe; un bundle statico copre il segmento prediction;
- GT vale uno se il centro cade in una semantic region o evento GT actionable.

La presenza di segmento e uno per ciascuno dei tre vettori soltanto quando la
rispettiva occupancy sulla stessa griglia e almeno 5%. Un segmento con meno di
20 centri e `N/A` per l'adapter omologo, ma resta disponibile alle metriche v3
native. Ogni classe richiede separatamente almeno 30 gruppi GT positivi e 30
negativi sia su development sia su final-test.

L'adapter calcola macro-F1 a sei classi e false-positive group rate sui gruppi
clean, senza inventare curve, frequenze o severity per v2. Ognuno dei tre
candidati G0 viene valutato separatamente con model, provenance, mask e soglie
congelati. G2 deve superare ciascun seed: macro-F1 almeno 10% relativo, oppure
almeno +0.10 assoluto quando il seed v2 e sotto 0.10, e false-positive group
rate non superiore allo stesso seed. Non e ammesso comporre una baseline con la
metrica migliore di un seed e la safety migliore di un altro. G1a serializza
mapping, costanti e hash gia definiti qui; non puo sceglierli o modificarli.

## 11. Matrice benchmark e potenza statistica

### 11.1 Famiglie congelate

Il benchmark non e un unico pool e non usa il numero di file come prova di
indipendenza. Ogni asset appartiene a una o piu famiglie dichiarate nel
manifest, ma il supporto statistico e sempre contato per `group_id`.

| Famiglia | Contenuto | Metriche primarie | Copertura minima prima della power analysis |
|---|---|---|---|
| `tonal-controlled` | dry group-disjoint con 1-3 trasformazioni note, includendo boost, cut, shelf, bell e interazioni | RMSE/p95 della curva, miglioramento residuo, errore di segno | almeno 5 parent group per profilo; ogni regione tonale e direzione compare in almeno 10 gruppi |
| `tonal-natural` | materiale reale clean e materiale reale con curva actionable annotata e adjudicata | RMSE/p95, coverage, clean actionable rate, severity | almeno 5 clean e 5 actionable group per profilo |
| `anomaly-natural` | Resonance, Harshness e Sibilance reali con intervallo, banda e severity annotati | PR-AUC, F1, errore frequenziale/temporale, severity, falsi/min | almeno 30 positive group per classe; i negativi usano il pool clean-safety completo |
| `clean-safety` | materiale reale intenzionalmente corretto, incluso materiale colorato ma non problematico | clean actionable rate e falsi eventi/min | almeno 149 group totali e 10 per profilo, ciascuno con almeno un minuto clean eleggibile per i tre tipi anomaly; nessun negativo implicito |
| `electronic-stratified` | vista trasversale delle quattro famiglie per techno, house, breakbeat e altri sottogeneri dichiarati | stesse metriche della famiglia madre, riportate per sottogenere | almeno il 40% del final-test complessivo; nessuna claim di sottogenere senza supporto determinato dalla power analysis |

I derivati controlled ereditano il gruppo del dry e non moltiplicano il
supporto. Il set `tonal-controlled` usa parent group sigillati e una griglia di
parametri preregistrata. La partizione di holdout opera su celle del piano
fattoriale `(sequenza trasformazioni, regione, direzione, gain, Q)`, non soltanto
su seed o asset: nessuna cella final-test compare nel generatore di training e
almeno una composizione multi-trasformazione completa e riservata al benchmark.
`tonal-natural` e obbligatorio: il sintetico da solo non puo dimostrare
trasferimento al reale.

Le regioni tonali canoniche, inclusive a sinistra ed esclusive a destra salvo
l'ultima, sono `[20,80)`, `[80,200)`, `[200,500)`, `[500,2000)`, `[2000,5000)`,
`[5000,10000)` e `[10000,20000]` Hz. La griglia controlled deve coprire boost e
cut in ciascuna regione; un asset con piu trasformazioni resta una sola unita
del parent group.

Il sotto-set competitor di G5/G8 e una selezione preregistrata dalle famiglie
`tonal-natural`, `anomaly-natural` e `clean-safety`; non costituisce una sesta
famiglia e non puo cambiare i loro conteggi dopo l'apertura.

### 11.2 Piano di potenza preregistrato

La numerosita finale di ogni famiglia e strato e:

```text
n_required = max(floor_contrattuale, n_power)
```

`n_power` viene stimato soltanto dal sottoinsieme `development_pilot`, mai da
`final-test`, con unita di ricampionamento `group_id` e test paired quando i
sistemi condividono la sorgente. Il pilot non viene usato per il point-estimate
del gate development dello stesso round e deve avere almeno 30 gruppi paired
per una metrica continua/macro e almeno 15 gruppi GT positivi per ogni classe
anomaly; per stimare l'overdispersione false-events servono almeno 30 gruppi
clean con un minuto eleggibile per classe. Supporto inferiore richiede nuovi
batch, non una diversa selezione.

La decisione finale controlla alpha family-wise 0.05 con Holm su tutti i gate
primari dello stesso report. Per dimensionare il campione si usa invece il
conservativo `alpha_plan = 0.05 / m`, dove `m` e il numero congelato di gate
primari: e il piu piccolo livello possibile nella procedura Holm. La potenza
minima e 0.90. Numero e ordine dei gate sono congelati prima del calcolo. Il
piano registra seed, pilot SHA-256, statistico, orientamento, effect size minimo,
`m`, `alpha_plan`, supporto, risultato e versione dell'implementazione.

L'effetto minimo di interesse e preregistrato per metrica:

- errore di curva comparabile: riduzione relativa almeno 10% rispetto alla
  baseline pertinente, senza regressione clean;
- macro-F1 o PR-AUC comparabile: aumento relativo almeno 10%, oppure +0.10
  assoluto quando la baseline e inferiore a 0.10;
- clean actionable rate: ipotesi nulla `p >= 0.02`, alternativa di progetto
  `p = 0.01`, limite superiore esatto unilaterale sotto 0.02;
- falsi eventi/min: ipotesi nulla `lambda >= 0.5`, alternativa di progetto
  `lambda = 0.25`, limite superiore Poisson esatto e limite cluster-bootstrap
  entrambi non oltre 0.5;
- parity e invariance: restano gate deterministici, non sono sostituiti da una
  power analysis;
- G8: numerosita ascoltatori e sorgenti resta determinata dal pilot G5 e dal
  modello clusterizzato definito nel piano principale.

Per metriche continue e macro-F1/PR-AUC, il tool usa 10000 simulazioni PCG64. A
ogni numerosita candidata ricampiona con replacement i record completi di
gruppo paired, ricalcola lo statistico, centra la distribuzione bootstrap e la
trasla esattamente dell'effetto minimo dichiarato; non usa la media favorevole
osservata come alternativa. La potenza e la quota di simulazioni che supera il
critico unilaterale sotto la distribuzione centrata nulla ad `alpha_plan`. Si
sceglie il primo `n` con potenza almeno 0.90; seed base `20260719`, derivato per
metrica con SHA-256 del suo ID canonico.

Per clean actionable rate, `n_power` e il primo `n` per cui il test binomiale
esatto unilaterale di `p >= 0.02` ha size non oltre `alpha_plan` e potenza almeno
0.90 a `p = 0.01`; resta comunque il floor di 149 gruppi.

Il tempo clean eleggibile e l'unione degli intervalli coperti da frame G1 validi
nei segmenti `clean_for_action` con il tipo in `explicit_negative_types`.
Silenzio, frame invalidi, warm-up, overlap e derivati duplicati non entrano nel
denominatore. Per ciascun gruppo e classe si usa esattamente il primo minuto
eleggibile in ordine temporale canonico; un gruppo con meno di un minuto non
entra nel pool false-events. Quindi `n` gruppi producono esattamente `n` minuti
indipendenti e nessun asset lungo domina l'esposizione.

La potenza false-events e congiunta. Dal pilot si costruisce per ogni classe il
vettore di conteggi sui minuti standard. Si stima soltanto l'overdispersione con

```text
mu_pilot = mean(counts)
alpha_nb = max(0, (sample_variance(counts) - mu_pilot)
                  / max(mu_pilot**2, 1e-12))
```

`sample_variance` usa il denominatore `n - 1`. Si fissa sempre la media
alternativa a `lambda = 0.25`, senza usare la media osservata. Per ogni
`n >= 149`, 5000 simulazioni PCG64 estraggono `n` rate di
gruppo da `Gamma(shape=1/alpha_nb, scale=0.25*alpha_nb)` e poi conteggi
`Poisson(rate)`; con `alpha_nb = 0` usano direttamente `Poisson(0.25)`. Ogni
simulazione calcola il limite Poisson esatto e, con 2000 resample interni dei
gruppi, il quantile `1 - alpha_plan` della media dei conteggi da un minuto.
Entrambi i limiti sono quindi espressi in eventi/minuto. Il seed interno e
derivato dai primi 8 byte di
`SHA256(metric_id + NUL + outer_simulation_index)`. Si sceglie il primo `n` per
cui almeno il 90% delle simulazioni soddisfa entrambi i limiti `<= 0.5`. Il gate
finale usa il limite esatto e 10000 resample per gruppi con il seed evaluator
sul pool reale. Con zero eventi il limite Poisson simultaneo e
`-log(alpha_plan) / n`, mai zero. La dimensione del pool clean-safety e il
massimo fra 149, `n_power` binomiale e i tre `n_power` false-events; lo stesso
pool deve soddisfare tutti e quattro i gate.

Il risultato viene congelato in `benchmark_power_plan.json` con SHA-256 prima
di aprire `final-test`. Se il corpus disponibile non raggiunge `n_required` o
l'esposizione richiesta, la famiglia e NO-GO: non si riduce l'effetto, non si
contano crop come gruppi e non si trasferiscono asset da altri ruoli. Tutti i
risultati pubblicano supporto, intervallo di confidenza e numero di gruppi
esclusi con motivo. Non e ammesso scegliere a posteriori il metodo che produce
il campione minore.

## 12. Protocollo competitor

Il protocollo viene congelato in G1 ma usato per claim soltanto in G5/G8.

G5 usa gruppi dichiarati in anticipo dentro `development-metric`: il risultato
e un veto su artefatti/workflow e puo causare una nuova iterazione, quindi non
ha valore di final-test. G8 usa gruppi `final-test` mai aperti prima. I due set
non condividono group, dry o derivati.

- dry e target final-test sono sigillati prima dei render;
- stessa sorgente, stessa regione e stesso obiettivo per tutti i sistemi;
- versione plugin, modalita, profilo, quality mode e preset iniziale sono
  registrati; niente ritocco manuale non dichiarato;
- render float32 48 kHz, latency-compensated, senza clipping;
- loudness matching entro 0.1 LU sul segmento valutato, con guadagno applicato
  registrato;
- nomi randomizzati e metadati rimossi per l'ascolto cieco;
- output competitor vietato a train, validation e calibration; nel sotto-set
  G5 di development-metric puo produrre soltanto GO/NO-GO e report qualitativo,
  mai target, feature, threshold o supervisione;
- hash SHA-256 per dry, render, configurazione e tabella di randomizzazione.

Il confronto numerico usa l'effetto applicato, non il nome del problema:

- curva statica: mediana temporale di
  `mid_psd_db(processed) - mid_psd_db(dry)` sulle celle allineate e udibili;
- azione dinamica: la stessa differenza frame-per-frame sulla griglia G1;
- artefatti: overshoot, pumping, variazione stereo e loudness residuo.

Il loudness matching usa ITU-R BS.1770-4 con gating integrato sul segmento
comune. Dry e render vengono prima allineati per latenza e poi troncati alla
stessa durata; nessun time-stretch e ammesso.

I render competitor final-test restano sigillati fino a G8. La non inferiorita
umana resta G8; G1 verifica soltanto che il protocollo sia riproducibile e non
contaminante.

## 13. Fixture e gate G1

Le fixture sono generate da formule, non registrate a mano:

- multitone ai centri critici 45, 60, 80, 250, 1000, 3500, 8000, 16000 e
  20000 Hz;
- sweep log 20-20000 Hz;
- pseudo-rumore continuo band-limited: somma di 512 sinusoidi con frequenze
  `20 + (20000-20)*(k+0.5)/512`, fase PCG64 seed 31051986 e ampiezza RMS
  complessiva -24 dBFS; ogni ampiezza di picco vale
  `10**(-24/20) * sqrt(2/512)` e il segnale e valutato analiticamente ai tre
  sample rate;
- transient burst e risonanza smorzata;
- a 96 kHz, toni ultrasonici a 28000, 32000 e 40000 Hz;
- stereo mid-only, side-only e decorrelato;
- silenzio e input non finiti per i gate fail-closed.

Ogni segnale continuo viene renderizzato direttamente a 44.1, 48 e 96 kHz;
non si crea una variante ricampionando un'altra variante. I confronti usano
frame con timestamp fisico comune, dopo warm-up, e sole celle attive sopra il
floor dichiarato.

La parity spettrale usa le porzioni stazionarie di multitone, sweep e rumore,
escludendo soltanto warm-up e coda dichiarati nel manifest. Transient burst e
risonanza smorzata non vengono esclusi: hanno un gate separato su onset, picco
e decadimento, per evitare che un arrotondamento frazionario venga nascosto in
un confronto dB non omologo.

Gate obbligatori:

1. **Struttura**: 120 centri, estremi 20/20000 Hz, ordine stretto, timestamp e
   forme esatte.
2. **Determinismo**: due processi puliti producono artefatti byte-identici
   sulla stessa piattaforma e stesso lock.
3. **Streaming**: chunk casuali e input monolitico producono gli stessi frame,
   max delta `1e-6`.
4. **Sample-rate parity**: rispetto al render 48 kHz, max delta assoluto sulle
   feature dB attive nelle porzioni spettrali dichiarate a 44.1 e 96 kHz
   `<= 0.25 dB`; timestamp entro un campione canonico. Sui transienti: onset e
   frame di picco entro un campione canonico e tempo di decadimento entro un
   hop, senza confronto ottenuto spostando manualmente i frame.
5. **Gain invariance**: a -12, -6, +6 e +12 dB senza clipping, shape,
   prominence e delta hanno max delta `<= 0.05 dB`; PSD e level traslano del
   gain applicato con errore `<= 0.05 dB`.
6. **M/S**: mono e stereo dual-mono hanno mid equivalente `<= 0.05 dB`; side
   dual-mono resta non valido e non genera azioni; stereo anti-fase ha mid non
   valido ma side e frame globale validi.
7. **Anti-alias**: i toni ultrasonici a 96 kHz non producono componenti alias
   fra 20 e 20000 Hz oltre -80 dB rispetto al tono di ingresso.
8. **Split**: zero overlap di group, SHA e parent fra tutti i ruoli; ogni
   derivato eredita il ruolo del parent; nessun fallback. Commitment errato,
   reveal anticipato, ID rinominato, roster parziale, retry del salt o filtro
   post-role devono fallire.
9. **Evaluator**: fixture perfetta produce metriche perfette; prediction vuota,
   classe errata, frequenza errata, segno invertito e duplicati producono i
   fallimenti attesi; permutare righe o duplicare un `evaluation_unit_id` non
   cambia il report. Superficie anomaly mancante, score thresholded e zero
   eventi con esposizione insufficiente devono fallire. Policy assente/hash
   errato, threshold mutato o actionable non riproducibile devono fallire; le
   fixture power devono provare che il gate congiunto non usa la sola Poisson.
10. **Ambiente**: sync del lock con hash, test completi e deep hash di tutte le
   fixture PASS.

La soglia 0.25 dB e hard. Non si sostituisce con una media o un p95 per
nascondere una banda fuori contratto.

Gli artefatti numerici canonici sono array little-endian `.npy` senza oggetti
e JSON UTF-8 con chiavi ordinate, separatori compatti, newline finale e float
finiti. Archivi ZIP/NPZ con timestamp non sono usati come prova byte-identica.

## 14. Implementazione e commit atomici dopo il GO

Ordine obbligatorio:

1. **G1a - contract artifacts**: JSON schema di manifest, admission batch,
   annotazione, prediction, calibration policy e piano di potenza; griglia 120
   bande, split roles, commit-reveal, copertura calibration, mapping adapter
   v2-v3, generatori fixture e hash. Nessun frontend ancora.
2. **G1b - canonical frontend**: resampler streaming, dual-resolution
   time-aligned, `V3FeatureFrame` Python e unit test.
3. **G1c - evaluator**: parser fail-closed, matching, metriche, CI group-level,
   adapter omologo v2-v3 e fixture di errore.
4. **G1d - competitor protocol harness**: manifest/config/hash e verifica dei
   render; nessun render proprietario nel repository.
5. **G1e - report**: esecuzione completa dei gate, hash degli output e tabella
   PASS/FAIL.

Dopo ogni commit:

```bash
git diff --cached --check
python -m compileall -q ml_v3
python -m unittest discover -s ml_v3/tests -p 'test_*.py'
git diff --name-only 2c88edad -- Source ml_v2 CMakeLists.txt Resources AIEQ-mac
```

L'ultimo comando deve produrre output vuoto. I comandi specifici delle fixture
saranno definiti da G1a e poi riportati senza abbreviazioni nel report.

## 15. Stop condition e rollback

G1 e NO-GO se si verifica uno solo dei seguenti casi:

- sample-rate parity o streaming parity non raggiunti senza rilassare il gate;
- LF e MAIN non condividono lo stesso timestamp;
- uno split permette fallback, overlap o riassegnazione retroattiva;
- il preflight G1 accetta una fixture calibration, development o final-test che
  non raggiunge supporto o esposizione richiesti. La disponibilita del corpus
  reale viene invece verificata in G3/G4 e non blocca l'implementazione G1;
- il piano di potenza usa file/crop come unita indipendenti, legge final-test o
  viene modificato dopo la sua apertura;
- fit o metriche cambiano duplicando celle, segmenti o derivati nello stesso
  gruppo;
- un sistema puo omettere score anomaly pre-threshold senza invalidare lo
  schema;
- calibratore, soglia o decision policy possono cambiare senza cambiare hash o
  senza invalidare prediction e report;
- il dimensionamento false-events garantisce potenza soltanto per uno dei due
  limiti richiesti dal gate congiunto;
- una metrica dipende dall'ordine dei file o usa lo split che calibra;
- final-test viene letto per scegliere una decisione;
- una dipendenza non e bloccata con hash;
- serve modificare runtime o `ml_v2` per far passare G1.

Rollback: si elimina il branch/commit G1 non promosso e si torna al commit G0
`2c88edad`. Nessun altro branch richiede ripristino.

## 16. Criterio di approvazione

Il reviewer deve verificare questo contratto su commit immutabile e restituire
una sola lista consolidata. Il GO richiede:

- nessuna contraddizione con `docs/MOTORE_V3_PLAN.md`;
- specifiche implementabili senza decisioni aperte nascoste;
- nessun percorso di leakage o contaminazione del final-test;
- metriche separate per bilanciamento tonale e anomalie dinamiche;
- supporti calibration clean/positivi e benchmark group-level falsificabili;
- baseline non omologhe trattate come `N/A`, mai come vittorie artificiali;
- gate abbastanza severi da rendere falsificabili le claim successive;
- diff del commit limitato a questo documento.

Fino a quel GO: nessun file G1 di codice, fixture o ambiente viene creato.
