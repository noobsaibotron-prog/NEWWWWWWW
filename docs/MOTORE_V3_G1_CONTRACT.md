# Motore v3 - Contratto G1 frontend e benchmark

Stato: PROPOSTA IMMUTABILE PER COUNTER-CHECK. Questo documento non autorizza
ancora l'implementazione. G1 parte soltanto dopo il GO del reviewer sul commit
che contiene esclusivamente questo file.

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

L'enum interno `SourceProfile::Techno` usa il conditioning `edm`. Techno,
house, breakbeat e altri sottogeneri restano metadati di benchmark separati e
obbligatori quando noti; non aggiungono un parametro host e non cambiano il
numero dei profili.

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

### 8.2 Identita e assegnazione

`group_id` rappresenta la piu piccola unita conservativa che racchiude tutte
le dipendenze note: composizione, registrazione/sessione o artista; se questa
identita non e ricostruibile, si usa l'intero pack. Alias della stessa entita
fra sorgenti diverse vengono riconciliati prima dell'ammissione. Ogni gruppo ha
metadati immutabili `group_primary_profile`, `group_primary_domain` e
`source_family`. Stem, mix, versioni, crop, augmentation, injection, render
processati e render competitor ereditano lo stesso gruppo del dry originale.

Regole:

- nessun fallback legacy: ogni gruppo e assegnato esplicitamente;
- nessun gruppo o SHA audio puo comparire in due ruoli;
- near-duplicate detection e obbligatoria prima di G3, ma non sostituisce
  l'identita di gruppo;
- assegnazione deterministica `sha256-threshold-v1`: interpretare come intero
  unsigned big-endian i primi 8 byte di
  `SHA256(salt + NUL + group_primary_profile + NUL + group_primary_domain +
  NUL + source_family + NUL + group_id)`, dividerlo per `2**64` e assegnare gli
  intervalli `[0,.55)`, `[.55,.65)`, `[.65,.75)`, `[.75,.85)`, `[.85,1)` ai
  cinque ruoli nell'ordine dichiarato;
- strato di verifica minimo: `(group_primary_profile, group_primary_domain,
  source_family)`; il preflight pubblica conteggi per strato e si ferma se i
  minimi per profilo non sono raggiunti;
- batch e assegnazioni append-only; salt, ruoli e metadati ammessi non sono
  modificabili;
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
schema, asset_id, relative_path, sha256, group_id, split_role,
source_profile, primary_domain, group_primary_profile, group_primary_domain,
source_family, electronic_subgenre,
sample_rate, channels, duration_s, parent_asset_id, derivative_kind,
license_class, license_url, attribution, ledger_id
```

Licenze ammesse: CC0, CC-BY o OWNED con ledger completo. Campo mancante,
licenza sconosciuta o hash errato bloccano il preflight.

### 9.2 Annotation record

Ogni record usa `schema = "aieq-v3-annotation-1"` e contiene:

- `asset_id`, `annotator_id`, `pass_id`, `profile`,
  `segment_start_s`, `segment_end_s`;
- `tonal_correction_db[120]`: EQ correttiva desiderata; segno positivo =
  boost, negativo = cut;
- `tonal_confidence[120]` e `tonal_actionable_mask[120]`;
- `semantic_regions[]` per gli otto tipi pubblici correnti;
- `dynamic_events[]` per Resonance, Harshness e Sibilance;
- `global_actionable`, `clean_for_action`, note e versione tool.

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
evento actionable. Un suono colorato ma intenzionalmente corretto e clean:
non viene trasformato in hard-negative di un problema diverso.

G3 definira processo a due annotatori e adjudication. G1 congela formato e
semantica, non inventa annotazioni reali.

### 9.3 Prediction record

Ogni prediction usa `schema = "aieq-v3-prediction-1"` e contiene `asset_id`,
`model_id`, hash del modello e del frontend contract, profilo, curva tonale a
120 bande, confidence a 120 bande, `segment_start_s`, `segment_end_s`, bundle
semantici e lista eventi. Ogni bundle ed evento porta confidence e
`actionable`; l'evaluator non ricostruisce questi campi da una soglia nascosta.

## 10. Evaluator deterministico

L'evaluator legge soltanto manifest, annotation record e prediction record
con schema/versione/hash compatibili. L'ordine dei file non puo cambiare i
risultati. Tutte le aggregazioni pubblicano valore globale, macro per profilo,
macro per dominio e CI percentile 95% da 10000 bootstrap a livello `group_id`,
con PCG64 seed 20260719. Il campionamento conserva tutti gli asset figli del
gruppo estratto.

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

- precision, recall, F1 e area precision-recall per classe;
- errore centro in ottave per Resonance;
- errore onset e offset in millisecondi;
- MAE della severity `[0,1]` sugli eventi matched e Spearman rho con supporto
  almeno 10;
- falsi eventi al minuto su gruppi clean;
- durata e occupancy soltanto come diagnostica, mai come gate primario.

Nessun singolo file, singolo hit o threshold scelto sullo stesso split puo far
passare una classe.

### 10.3 Calibrazione delle confidence

Tonal e anomaly hanno calibratori, report e parametri separati. I calibratori
si fittano solo su `calibration`; ECE, Brier e PR-AUC vengono pubblicati su
`development-metric` e poi, una sola volta, su `final-test`.

Per la confidence tonale, ogni cella udibile e un esempio binario actionable/
non-actionable. Per anomaly, ogni evento predetto e corretto se entra nel
matching one-to-one; i ground-truth mancati restano FN nelle metriche PR.

ECE usa 15 bin equal-count: stable sort per `(confidence, asset_id, event_or_
band_id)`, partizione con differenza di cardinalita al massimo uno, poi
`sum_bin(n_bin/N * abs(accuracy_bin - mean_confidence_bin))`. Brier e la media
di `(confidence - target)**2`. Bin vuoti non vengono creati e supporto zero e
`N/A`, mai PASS.

## 11. Protocollo competitor

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

## 12. Fixture e gate G1

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
   derivato eredita il ruolo del parent; nessun fallback.
9. **Evaluator**: fixture perfetta produce metriche perfette; prediction vuota,
   classe errata, frequenza errata, segno invertito e duplicati producono i
   fallimenti attesi; permutare righe non cambia il report.
10. **Ambiente**: sync del lock con hash, test completi e deep hash di tutte le
   fixture PASS.

La soglia 0.25 dB e hard. Non si sostituisce con una media o un p95 per
nascondere una banda fuori contratto.

Gli artefatti numerici canonici sono array little-endian `.npy` senza oggetti
e JSON UTF-8 con chiavi ordinate, separatori compatti, newline finale e float
finiti. Archivi ZIP/NPZ con timestamp non sono usati come prova byte-identica.

## 13. Implementazione e commit atomici dopo il GO

Ordine obbligatorio:

1. **G1a - contract artifacts**: JSON schema, griglia 120 bande, split roles,
   generatori fixture e hash. Nessun frontend ancora.
2. **G1b - canonical frontend**: resampler streaming, dual-resolution
   time-aligned, `V3FeatureFrame` Python e unit test.
3. **G1c - evaluator**: parser fail-closed, matching, metriche, CI group-level e
   fixture di errore.
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

## 14. Stop condition e rollback

G1 e NO-GO se si verifica uno solo dei seguenti casi:

- sample-rate parity o streaming parity non raggiunti senza rilassare il gate;
- LF e MAIN non condividono lo stesso timestamp;
- uno split permette fallback, overlap o riassegnazione retroattiva;
- una metrica dipende dall'ordine dei file o usa lo split che calibra;
- final-test viene letto per scegliere una decisione;
- una dipendenza non e bloccata con hash;
- serve modificare runtime o `ml_v2` per far passare G1.

Rollback: si elimina il branch/commit G1 non promosso e si torna al commit G0
`2c88edad`. Nessun altro branch richiede ripristino.

## 15. Criterio di approvazione

Il reviewer deve verificare questo contratto su commit immutabile e restituire
una sola lista consolidata. Il GO richiede:

- nessuna contraddizione con `docs/MOTORE_V3_PLAN.md`;
- specifiche implementabili senza decisioni aperte nascoste;
- nessun percorso di leakage o contaminazione del final-test;
- metriche separate per bilanciamento tonale e anomalie dinamiche;
- gate abbastanza severi da rendere falsificabili le claim successive;
- diff del commit limitato a questo documento.

Fino a quel GO: nessun file G1 di codice, fixture o ambiente viene creato.
