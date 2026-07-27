# Motore v3 - Contratto G1 frontend e benchmark

Stato normativo: REVISIONE 8 G1C SCHEMA V2 — 2026-07-27.

Stringa di revisione normativa:

```text
MOTORE_V3_G1_CONTRACT REVISIONE 8 G1C SCHEMA V2
```

Il testo non auto-dichiara il proprio stato operativo. REV8 e attiva soltanto
quando il dispatcher pubblico, il PLAN e il manifest di attivazione sigillato
la indicano insieme come attiva. Finche vive soltanto nel path candidato non
sostituisce `docs/MOTORE_V3_G1_CONTRACT.md`, che resta il contratto vivo REV7.
I moduli REV8 possono essere costruiti e testati soltanto attraverso entrypoint
versionati non esportati dal dispatcher REV7. L'attivazione richiede lo switch
atomico della sezione 14; non esiste uno stato ammesso con norma REV8 e consumer
REV7, o viceversa.

REV8 mantiene invariati i gate metrologici REV7: dominio geometrico `R`,
LF fuori `R` report-only, closing set stazionario `multitone` e
`pseudo_noise`, aggregatore max, soglia 0.25 dB, pubblicazione obbligatoria
fuori `R`, A3 retired e sweep non-stazionario parcheggiato per G1e. Non
autorizza threshold shopping, training, promozione prodotto o test Ableton.

REV8 chiude esclusivamente i contratti evaluator rimasti ambigui:

- schema annotation/prediction/policy v2 senza fallback;
- evaluation-unit index e superfici dense posseduti da un'autorita trusted;
- normalizzazione numerica, identita, duplicati e ordinamenti deterministici;
- matching one-to-one polinomiale separato per eventi e regioni;
- Gate 9 materializzato, eseguibile e sigillato.

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
- frontend lock, evaluation-unit index e run manifest trusted;
- surface admission con authority root separata dal bundle candidato;
- evaluator deterministico per curve tonali e anomalie;
- protocollo di render e confronto competitor;
- report numerico dei gate G1.

G1 non allena modelli, non sceglie l'architettura ML e non dimostra parita con
un competitor. Costruisce il metro con cui queste claim potranno essere
falsificate.

## 2. Perimetro e protezioni

Durante G1 sono ammessi soltanto:

- `docs/MOTORE_V3_G1_CONTRACT.md`;
- `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`;
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
`192000` Hz. I gate obbligatori G1 coprono 44100, 48000 e 96000 Hz;
`88200`, `176400` e `192000` Hz restano sperimentali / report-only fino a un
gate dedicato successivo e non possono sostenere un PASS di sample-rate
parity. Ogni altro sample rate viene rifiutato, non reinterpretato.

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

### 8.2 Identita e assegnazione (§8-minimo byte-level pinnato)

`group_id` rappresenta la piu piccola unita conservativa che racchiude tutte
le dipendenze note: composizione, registrazione/sessione o artista; se questa
identita non e ricostruibile, si usa l'intero pack. Alias della stessa entita
fra sorgenti diverse vengono riconciliati prima dell'ammissione. Ogni gruppo ha
metadati immutabili `group_primary_profile`, `group_primary_domain` e
`source_family`. Stem, mix, versioni, crop, augmentation, injection, render
processati e render competitor ereditano lo stesso gruppo del dry originale.
Mapping di alias, upstream snapshot e regole di inclusione sono versionati
prima dello split; rinominare un gruppo o cambiare profilo/dominio dopo
l'assegnazione invalida il batch.

#### 8.2.1 Canonical JSON (hashabile)

Ogni documento usato per `admission_batch_id`, commitment o artefatti di split
segue le stesse regole degli artefatti numerici di §13: UTF-8 senza BOM;
chiavi ordinate per code point; separatori esatti `,` e `:`; newline LF finale
singola **inclusa nell'hash**; `allow_nan=false`; in lettura rifiuto di
NaN/Infinity/overflow e chiavi duplicate. Si ordinano solo i set dichiarati
order-independent; liste semantiche (curve, score, griglia) restano in ordine.

#### 8.2.2 Identita `group_id` (non testo libero)

- Con upstream stabile: `source_family + ":" + upstream_id`.
- Senza upstream: `source_family + ":pack:" + SHA256(pack_bytes)`, dove
  `pack_bytes` e la concatenazione, per ogni digest SHA-256 **lowercase**
  ordinato per byte ASCII, di `(64 caratteri ASCII + LF)`. Nessun altro
  serializzatore e ammesso. La sequenza `:pack:` e **riservata** alla sola
  forma di fallback generata dal contratto; un `upstream_id` non deve
  produrre ne imitare quella forma.
- Il `group_id` dichiarato deve coincidere con quello ricostruito.
  `upstream_id` e pack entrambi presenti, oppure entrambi assenti quando
  servirebbe l'altro → FAIL.
- U+0000 (NUL) e **vietato** in ogni componente di identita o messaggio HMAC
  (`source_snapshot_id`, `group_id`, `group_primary_domain`, `source_family`,
  `upstream_id`, alias, e la stringa `admission_batch_id`): rifiuto **prima**
  di costruire il MAC. Inoltre `:` e **vietato** in `source_family` e in
  `upstream_id` (oltre al NUL gia vietato).

#### 8.2.3 Roster pre-split

Envelope exact-key, schema `"aieq-v3-admission-roster-1"`:

```text
{
  schema,
  source_snapshot_id,
  source_snapshot_sha256,
  groups: [
    { group_id, group_primary_profile, group_primary_domain, source_family }
  ]
}
```

`groups` ordinati per byte UTF-8 di `group_id`; `additionalProperties`
vietato; il roster **non** porta ruoli ne flag pilot.

`source_snapshot_sha256` = SHA-256 **lowercase hex** (64 caratteri ASCII) di
`canonical_bytes(source_identity_index)`, con
`source_identity_index.schema == "aieq-v3-source-identity-index-1"` e byte
canonici secondo §8.2.1. Nessun altro digest, serializzazione o SHA
free-form e ammesso per quel campo.

`admission_batch_id` = SHA-256 **lowercase hex** (64 caratteri ASCII) dei
byte canonici del roster, **newline finale inclusa**.

#### 8.2.4 Commit-reveal

Prima di vedere il roster il reviewer genera 32 byte raw `salt` e committa

```text
commitment = SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt)
```

Il curatore materializza e committa il roster completo di tutti i gruppi
eleggibili della source snapshot, in ordine canonico, senza ruoli; soltanto
allora il reviewer rivela `salt` e il tool verifica il commitment. Salt con
lunghezza diversa da 32 byte, commitment errato, reveal anticipato o retry
del salt → FAIL.

#### 8.2.5 Ruolo (HMAC, interi esatti)

`admission_batch_id` entra nell'HMAC come **esattamente i 64 byte ASCII
lowercase dell'hex digest**, **mai** i 32 byte raw. Questa scelta e
obbligatoria: la codifica raw-32 vs hex-64 cambia circa il 63.4% dei ruoli.

Messaggio:

```text
HMAC-SHA256(salt,
  b"aieq-v3-role-v1" + NUL
  + admission_batch_id(hex-ASCII) + NUL
  + group_primary_profile + NUL
  + group_primary_domain + NUL
  + source_family + NUL
  + group_id)
```

Sia `value` i primi 8 byte del digest, unsigned big-endian. Confronto
**intero esatto** (nessun float):

- `train` se `value * 20 < 11 * 2^64`;
- `validation` se `value * 20 < 13 * 2^64`;
- `calibration` se `value * 20 < 15 * 2^64`;
- `development-metric` se `value * 20 < 17 * 2^64`;
- altrimenti `final-test`.

(Equivalente alle quote dichiarate 55/10/10/10/15% senza arrotondamento
floating-point.)

#### 8.2.6 Pilot

```text
HMAC-SHA256(salt,
  b"aieq-v3-pilot-v1" + NUL
  + admission_batch_id(hex-ASCII) + NUL
  + group_id)
```

`admission_batch_id` entra come i **stessi 64 byte ASCII hex** del punto 8.2.5
(identico; mai raw-32). Sia `value` tutti i 32 byte unsigned big-endian.
`development_pilot = (role == "development-metric" AND value * 4 < 2^256)`.
Fuori da `development-metric` e sempre false. Il pilot e group-disjoint dal
punto-estimate development dello stesso round; se non raggiunge i supporti
richiesti si ammettono nuovi batch, non si cambia la soglia 0.25
(equivalente intero sopra).

#### 8.2.7 Invarianti e ammissione

- nessun fallback legacy: ogni gruppo e assegnato esplicitamente;
- un `group_id` e un SHA audio in esattamente un ruolo;
- parent e ogni derivato condividono ruolo e `group_id` del parent; parent
  esistente salvo root esplicito; niente cicli parent;
- `asset_id` e `relative_path` unici nel manifest;
- near-duplicate detection e obbligatoria prima di G3, ma non sostituisce
  l'identita di gruppo;
- un batch rivelato non puo essere filtrato/riassegnato dopo aver visto i
  ruoli: ammesso interamente oppure rifiutato solo per regola fail-closed
  preregistrata verificabile senza leggere i ruoli; i suoi gruppi non
  rientrano sotto un altro ID o batch;
- strato di verifica minimo: `(group_primary_profile, group_primary_domain,
  source_family)`; il preflight pubblica conteggi per strato e si ferma se i
  minimi per profilo non sono raggiunti;
- roster, commitment, reveal, batch e assegnazioni materializzati sono
  immutabili e coerenti **intra-batch** nel senso di questo §8-minimo
  (niente rewrite silenzioso degli artefatti gia materializzati dello stesso
  batch); la detection/enforcement cross-batch di retry pregressi, riuso del
  salt, cancellazione di history o ri-ammissione dopo tentativi precedenti
  appartiene a **§8-pieno** (ledger persistente) e **non** e una proprieta
  dimostrata da G1a-minimo;
- un gruppo multi-dominio riceve un solo ruolo globale;
- una sorgente ammessa da v2 viene riammessa esplicitamente nel contratto v3;
  non eredita il fallback A4b;
- input malformato → reject, mai reinterpretazione;
- `final-test` mai usato per scegliere regole, feature o soglie.

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
soltanto nello split `development-metric` ed e assegnato dopo lo split come in
§8.2.6: `HMAC-SHA256(salt, b"aieq-v3-pilot-v1" + NUL +
admission_batch_id(hex-ASCII) + NUL + group_id)`, con `admission_batch_id`
codificato come **64 byte ASCII hex** (identico al MAC di ruolo; mai raw-32),
`value` = digest intero unsigned big-endian a 32 byte, e
`development_pilot` vero solo se `role == "development-metric"` e
`value * 4 < 2^256`. Il pilot e group-disjoint dal punto-estimate
development dello stesso round; se non raggiunge i supporti richiesti si
ammettono nuovi batch, non si cambia la soglia equivalente a 0.25.

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

Ogni record usa `schema = "aieq-v3-annotation-2"` e contiene i 19 campi
top-level della versione 1, con la sola sostituzione dello schema.

Gli otto tipi e ID canonici restano, nell'ordine `0..7`: `Resonance`,
`Harshness`, `Muddiness`, `Sibilance`, `Boominess`, `Thinness`,
`BoxyMidrange`, `DullSound`. Un ID che non concorda con la stringa e invalido.

Ogni semantic region ha esattamente 12 chiavi:

```text
region_id, problem_type, problem_type_id, start_s, end_s,
band_lo_hz, band_hi_hz, center_hz, direction, severity, confidence, actionable
```

La banda e numerica, finita e contenuta in 20-20000 Hz, con
`band_lo_hz < band_hi_hz`. Per Resonance `center_hz` e numerico, finito e
interno alla banda; per gli altri tipi e `null`. Per Thinness e DullSound
`direction` vale `boost` o `cut`; per gli altri tipi e `null`.

Ogni dynamic event ha esattamente 10 chiavi:

```text
event_id, problem_type, problem_type_id, start_s, end_s,
center_hz, width_octaves, severity, confidence, actionable
```

La banda di un evento dinamico non e un campo ma una derivazione normativa.
Si calcolano in binary64 gli estremi grezzi
`raw_lo = center_hz * 2^(-width_octaves/2)` e
`raw_hi = center_hz * 2^(+width_octaves/2)`. La banda dell'evento e la coppia
di indici di banda canonica `(i_lo, i_hi)` ottenuta associando `raw_lo` e
`raw_hi` al centro di banda della sezione 6.1 piu vicino sull'asse `log2(Hz)`,
con tie-break all'indice minore. Vale sempre `i_lo <= i_hi`.

Poiche i centri canonici cadono per costruzione in 20-20000 Hz, la banda
derivata vi cade sempre: nessun controllo di intervallo si applica alla banda
derivata e nulla viene clippato. Il vincolo di intervallo resta sui campi
memorizzati, dove e verificabile esattamente: `center_hz` in 20-20000 Hz e
`width_octaves` maggiore o uguale a zero. Un evento su una sola banda ha
`width_octaves = 0` e `i_lo == i_hi`; e rappresentabile e non degenere.

La derivazione e idempotente e produce lo stesso risultato a tempo di
estrazione e a tempo di matching. Ogni confronto di banda fra eventi, in
qualunque sezione, usa questa definizione e nessun'altra.
Nella stessa evaluation unit, due semantic region dello stesso tipo con
identici `(start_s, end_s, band_lo_hz, band_hi_hz)` o due dynamic event con
semantic payload identico sono duplicati ground-truth invalidi.

Curve e prediction pubbliche sono finite e limitate a `[-9, +9]` dB;
confidence e severity sono in `[0, 1]`; tempi e frequenze devono cadere nel
segmento e in 20-20000 Hz. `clean_for_action = true` impone curva zero,
`tonal_actionable_mask` tutto falso, `global_actionable = false` e nessun
evento actionable; impone inoltre tutti gli otto tipi canonici dentro
`complete_types` ed `explicit_negative_types`. Fuori da questo caso, un tipo
entra in `complete_types` soltanto quando l'intero segmento e stato annotato
esaustivamente per quel tipo; entra in `explicit_negative_types` soltanto se e
completo e gli annotatori ne hanno verificato l'assenza. Le aree fuori dagli
eventi GT valgono come negative soltanto per tipi completi.

Una sola label di presenza senza curva, regione o evento non e una
supervisione tonale valida. G3 definira il processo a due annotatori e
adjudication; G1 congela formato e semantica.

### 9.3 Prediction record

Ogni prediction usa `schema = "aieq-v3-prediction-2"` e ha esattamente:

```text
schema, asset_id, evaluation_unit_id, model_id, model_sha256,
frontend_contract_sha256, calibration_policy_id, calibration_policy_sha256,
profile, tonal_curve_db, tonal_score, tonal_confidence,
segment_start_s, segment_end_s, semantic_bundles, events,
anomaly_score_ref, anomaly_severity_ref, anomaly_valid_ref
```

Ogni semantic bundle ha esattamente 13 chiavi:

```text
bundle_id, occurrence_ordinal, problem_type, problem_type_id, start_s, end_s,
band_lo_hz, band_hi_hz, center_hz, direction, severity, confidence, actionable
```

Ogni prediction event ha esattamente 11 chiavi:

```text
event_id, occurrence_ordinal, problem_type, problem_type_id, start_s, end_s,
center_hz, width_octaves, severity, confidence, actionable
```

Banda, centro e direzione seguono le regole omologhe della sezione 9.2.
`occurrence_ordinal` e un intero non negativo assegnato secondo la sezione 9.6.
Le collezioni prediction sono multiset: nessuna occorrenza viene deduplicata.

La policy usa `schema = "aieq-v3-calibration-policy-2"` e conserva i 17 campi
della versione 1. `prediction_schema_id` vale `aieq-v3-prediction-2` e
`prediction_schema_sha256` uguaglia esattamente
`sha256_of_obj(schema_for("aieq-v3-prediction-2"))`. Ogni modifica a
calibratore, threshold o decision rule cambia SHA-256.

Ogni prediction contiene le tre superfici `anomaly_score_ref`,
`anomaly_severity_ref` e `anomaly_valid_ref`. Le prime due sono riferimenti ad
array raw `<f4` di forma `[num_feature_frames,3,120]`, ordine classi
`Resonance`, `Harshness`, `Sibilance`, finiti in `[0,1]` prima di threshold,
hysteresis, top-k o veto. La terza e il riferimento trusted alla mask
`uint8 [num_feature_frames,120]` posseduta dall'indice della sezione 9.7.

L'asse di classe delle superfici dense ha lunghezza tre e il suo indice non e
`problem_type_id`. La corrispondenza e `Resonance = 0`, `Harshness = 1`,
`Sibilance = 2`, mentre gli stessi tipi hanno `problem_type_id` rispettivamente
`0`, `1` e `3`. I due indici non sono intercambiabili in nessun contesto.

L'evaluator applica la policy alla superficie score e deriva la lista completa
di candidati pre-threshold. Per ogni classe prende i massimi locali positivi
nel vicinato 3x3 tempo-banda: confidence maggiore o uguale a tutti i vicini e
maggiore di almeno un vicino esterno al proprio plateau connesso. Per ogni
plateau conserva soltanto la coordinata frame/banda minima.

In ordine decrescente di confidence, ogni massimo genera la componente
8-neighbour che lo contiene nella mask `confidence >= 0.5 * peak`; massimi
successivi la cui componente contiene gia un massimo conservato vengono
soppressi. Ogni componente produce onset/offset dai frame estremi, banda dagli
estremi di banda, `center_hz = sqrt(band_lo_hz * band_hi_hz)` e
`width_octaves = log2(band_hi_hz / band_lo_hz)`, in binary64. Confidence e
uguale al peak e severity e la media pesata della superficie.

Gli eventi pubblicati alla soglia operativa sono esattamente i candidati con
confidence calibrata almeno pari alla soglia. La PR-AUC ordina la lista
pre-threshold per `(confidence desc, frame, band)` e ripete il matching ai suoi
cut-point. Nessun top-k, floor di confidence o veto puo nascondere un
candidato. L'evaluator ricalcola confidence, bundle, eventi e actionability e
rifiuta ogni differenza.

La `direction` di un bundle Thinness/DullSound non e candidate-controlled.
La policy dichiara
`region_to_bundle.rule_id = signed_band_correction_v1`.
L'evaluator calcola la somma netta dei valori di `tonal_curve_db` i cui centri
di banda canonica ricadono nella banda del bundle Thinness/DullSound, in ordine
crescente di indice di banda, come razionali esatti dei rispettivi bit pattern
binary64 secondo la sezione 10.0 punto 5. Il segno della somma esatta produce
`boost` se positivo e `cut` se negativo. Una somma esattamente nulla non
produce alcun bundle Thinness/DullSound; la sua presenza e FAIL perche viola
l'ipotesi di direzione. Nessun sommatore in virgola mobile e ammesso per questo
ramo: il ramo FAIL discrimina lo zero dal non-zero, e nessun sommatore in
virgola mobile lo fa in modo affidabile.

### 9.4 Versionamento e assenza di fallback

L'entrypoint REV8 accetta soltanto annotation, prediction e calibration policy
v2 coerenti. Versione 1, versione ignota, batch misto, chiave aggiuntiva o
fallback implicito sono FAIL. Asset manifest e admission batch restano
rispettivamente `aieq-v3-asset-manifest-1` e
`aieq-v3-admission-batch-1`.

### 9.5 Normalizzazione numerica

`N64` accetta esclusivamente un valore ammesso da un campo JSON Schema
`number`, mai un booleano. Il token viene convertito a IEEE-754 binary64 con
round-to-nearest, ties-to-even. Overflow, NaN e infinito sono FAIL. Se il
risultato e zero, il bit di segno viene posto a zero.

La forma serializzata e `f64:` seguita dai sedici caratteri esadecimali
minuscoli del bit pattern binary64 big-endian. `100` e `100.0` producono
`f64:4059000000000000`; `-0.0` e `+0.0` producono
`f64:0000000000000000`. I campi `integer`, incluso `occurrence_ordinal`,
restano interi JSON.

I byte canonici normalizzati sono `canonical_bytes()` dopo l'applicazione
ricorsiva di `N64` ai soli campi `number` secondo lo schema v2.

### 9.6 Evaluation unit, duplicati e identificatori

L'identita di un'evaluation unit e la tupla normalizzata
`(evaluation_unit_id, asset_id, profile, segment_start_s, segment_end_s)`.
I tempi usano `N64`; l'ID da solo non identifica un'unita.

La chiave top-level prediction e la evaluation-unit key. La chiave top-level
annotation e `(evaluation_unit_key, annotator_id, pass_id)`. Dopo validazione,
normalizzazione, ricalcolo ID/ordinali e ordinamento delle collezioni:

1. stessa chiave e byte canonici normalizzati identici: deduplicazione con
   report byte-identico alla singola occorrenza;
2. stessa chiave e contenuto diverso: FAIL;
3. bundle/eventi prediction duplicati internamente: mai deduplicati; ogni
   eccedente non matched resta una distinta FP.

Il `semantic_payload` e esattamente:

```text
semantic region:
[kind, unit_key, type, problem_type_id, start, end, band_lo, band_hi, center, direction,
 severity, confidence, actionable]

dynamic event:
[kind, unit_key, type, problem_type_id, start, end, center, width, severity, confidence,
 actionable]

semantic bundle:
[kind, unit_key, type, problem_type_id, start, end, band_lo, band_hi, center, direction,
 severity, confidence, actionable]

prediction event:
[kind, unit_key, type, problem_type_id, start, end, center, width, severity, confidence,
 actionable]
```

Tutti i campi `number` usano `N64`.

Bundle ed eventi prediction con payload identico ricevono ordinali consecutivi
`0..N-1`, indipendenti dall'ordine d'ingresso. La `instance_key` prediction e
`[semantic_payload, occurrence_ordinal]`; per il ground truth e il solo
`semantic_payload`, poiche i duplicati strutturali GT sono invalidi.

L'ID e SHA-256 dei byte canonici di
`["motore-v3-instance-id-v1", instance_key]`. L'evaluator ricalcola ID e
ordinali e rifiuta ogni differenza. Stesso ID con `instance_key` diversa e
FAIL. ID, digest e ordine degli hash non partecipano a eleggibilita, costi o
tie-break.

I due test non coincidono e non devono essere confusi. La sezione 9.2 impone
una regola di coerenza sul ground truth: due semantic region dello stesso tipo
con identici `(start_s, end_s, band_lo_hz, band_hi_hz)` sono invalide anche
quando differiscono per centro, direzione, severity, confidence o
actionability, perche descrivono due volte la stessa porzione di tempo e
frequenza. Il `semantic_payload` e invece il meccanismo di identita, piu largo.
La regola della sezione 9.2 e strettamente piu forte e viene applicata per
prima: un ground truth che la supera non puo produrre collisioni di
`instance_id`, ed e per questo che l'`instance_key` GT non porta ordinale.

### 9.7 Autorita delle evaluation unit

G1 congela gli schema `aieq-v3-frontend-lock-1`,
`aieq-v3-evaluation-unit-index-1` e
`aieq-v3-evaluation-run-manifest-1`.

Il frontend lock contiene esattamente:

```text
schema, lock_id, platform, python_version, numpy_version,
environment_lock_sha256, components
```

`platform` contiene esattamente `os`, `os_version`, `arch` e
`python_implementation`.
`components` e la lista ordinata `(relative_path, sha256)` di:

```text
ml_v3/frontend/feature_frame.py
ml_v3/frontend/offline_features.py
ml_v3/frontend/resampler.py
ml_v3/frontend/resampler_coeffs.py
ml_v3/contracts/grid.py
ml_v3/contracts/constants.py
ml_v3/contracts/metrology_lock.py
ml_v3/environment/requirements.lock
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
```

`ml_v3/contracts/constants.py` resta byte-identico durante lo switch, poiche
contiene anche le costanti frontend sigillate. Revisione e schema ID REV8
vivono in `ml_v3/contracts/constants_v2.py`, fuori dal frontend lock.
`lock_id` e l'hash dei byte canonici normalizzati senza `lock_id`;
`frontend_contract_sha256 = sha256_of_obj(frontend_lock)` sul documento
completo.
All'attivazione il contratto vivo e byte-identico al candidato.

L'evaluation-unit index contiene esattamente:

```text
schema, index_id, contract_revision, role, asset_manifest_sha256,
frontend_contract_sha256, annotation_package_sha256, assets, units
```

`role` e `calibration`, `development-metric` o `final-test`.
`asset_manifest_sha256` e l'hash della lista normalizzata di record
`aieq-v3-asset-manifest-1` validati e ordinati per `asset_id`.
`annotation_package_sha256` e l'hash della lista adjudicated v2 normalizzata
e ordinata per annotation key. `index_id` e l'hash del documento normalizzato
senza `index_id`.

Ogni record `assets` contiene esattamente:

```text
asset_id, asset_sha256, sample_rate, channels, frontend_input_ref
```

`assets` e ordinata per `asset_id` e non ammette duplicati.
`frontend_input_ref` contiene esattamente `relative_path`, `sha256`, `num_host_samples`,
`sample_rate`, `channels`, `dtype`, `layout`; `dtype = float32_le`;
`layout = mono_N` oppure `interleaved_NC`. Il file e raw `<f4`, senza header,
padding o trailing bytes, lungo `N*C*4`, con
`N = num_host_samples` e `C = channels`.

Ogni record `units` contiene esattamente:

```text
evaluation_unit_id, asset_id, profile, segment_start_s, segment_end_s,
expected_T, anomaly_valid_ref
```

`expected_T` e intero `>= 1`. Ogni unita usa il solo input full-asset del
proprio record `assets`. Le unita sono uniche e ordinate per la key
normalizzata. `anomaly_valid_ref` contiene `relative_path`, `sha256`,
`num_feature_frames`, `dtype = uint8`, con path
`surfaces/anomaly_valid/<sha256(canonical_bytes_normalized(unit_key))>.u8`.

Il run manifest contiene:

```text
schema, run_id, contract_revision, role, asset_manifest_sha256,
annotation_package_sha256, evaluation_unit_index_id,
evaluation_unit_index_sha256, frontend_contract_sha256
```

`run_id` e l'hash del documento normalizzato senza `run_id`. Il suo digest
atteso proviene dall'orchestratore pre-run, mai dal candidato. Run manifest,
frontend lock e indice concordano campo per campo.

L'admission trusted verifica uguaglianza esatta fra evaluation-unit key
dell'indice e del package adjudicated per ruolo; membership degli asset;
hash, sample rate e canali fra manifest, assets e frontend input; profilo e
intervallo fra indice e annotation.

### 9.8 Costruzione della mask e ammissione delle superfici

Per ogni asset si crea un estrattore frontend nuovo, eseguito una sola volta
sull'intero `frontend_input_ref`. Sono vietati crop preliminari,
concatenazioni, padding, flush finali e riuso dello stato fra asset. I frame
restano in ordine crescente di `frame_index`.

Per l'unita si selezionano i frame con
`segment_start_s <= source_time < segment_end_s`. Il confronto usa timestamp
razionali G1 e bound binary64 via `as_integer_ratio()`, senza rounding
intermedio. Zero frame e FAIL.

Il frontend G1 espone validita scalare per frame. La mask e
`mask[t,b] = frame[t].valid` per tutte le 120 bande. Una mask per-banda
richiede una revisione successiva.

Score e severity si risolvono sotto `candidate_root`; mask e frontend input
sotto `authority_root`. Ogni path e POSIX relativo canonico: niente assoluti,
`.`, `..`, backslash o symlink esterni. Il loader apre una volta, esegue
`fstat`, legge una volta e usa lo stesso buffer per hash e parse.
Il riferimento mask ripetuto dalla prediction deve essere byte-identico al
riferimento dell'indice; il loader apre comunque soltanto la copia trusted.

Score e severity sono raw C-order `<f4`, forma `[expected_T,3,120]`, finiti in
`[0,1]`, lunghezza `expected_T*3*120*4`. La mask e raw `uint8`, forma
`[expected_T,120]`, valori `{0,1}`, lunghezza `expected_T*120`.
I tre `num_feature_frames` uguagliano `expected_T`. Sotto mask zero, score e
severity delle tre classi hanno bit raw `0x00000000`; `-0.0` e rifiutato.
Le celle invalide sono escluse da estrazione, metriche e denominatori.

## 10. Evaluator deterministico

L'evaluator legge soltanto manifest, annotation record e prediction record
con schema/versione/hash compatibili. L'ordine dei file non puo cambiare i
risultati. Tutte le aggregazioni pubblicano valore globale, macro per profilo,
macro per dominio e CI percentile 95% da 10000 bootstrap a livello `group_id`,
con PCG64 seed 20260719. Il campionamento conserva tutti gli asset figli del
gruppo estratto.

Salvo i rate exposure-aware dichiarati separatamente, ogni metrica primaria
viene calcolata prima per la `evaluation_unit_key` completa della sezione 9.6,
poi mediata dentro `group_id` e
infine macro-mediata con peso uguale fra gruppi. Numero di asset, segmenti,
eventi o celle non aumenta il peso del gruppo. Le micro-medie vengono riportate
solo come diagnostica.

### 10.0 Pin deterministici per aggregazioni e bootstrap

Questi pin chiudono gli operatori necessari a rendere vera la clausola "l'ordine
dei file non puo cambiare i risultati". Valgono per G1c e per ogni evaluator
successivo che produce report confrontabili byte-per-byte.

1. **Ordine canonico dei gruppi.** Prima di qualunque aggregazione, bootstrap o
   ricampionamento, i gruppi vengono ordinati per i byte UTF-8 di `group_id`
   crescente. Ogni record derivato conserva tutti gli asset figli del gruppo
   estratto. L'ordine originale di manifest, filesystem, prediction o annotation
   record non entra mai nello stream RNG o nella riduzione.

2. **Draw del bootstrap.** Il bootstrap a livello `group_id` usa
   `numpy.random.Generator(numpy.random.PCG64(seed)).integers(0, G,
   endpoint=False, dtype=np.int64)` sul vettore dei `G` gruppi gia ordinati. Quando
   servono matrici di indici, gli indici sono generati in shape dichiarata e
   consumati in ordine row-major C; `choice`, shuffle, iterazione su dict/set o
   qualunque operatore equivalente non e ammesso come implementazione normativa.

3. **Percentile e quantile.** Tutti i CI percentile e i quantili bootstrap usano
   Hyndman-Fan type 7, equivalente a NumPy `method="linear"`: ordinare valori
   finiti crescenti, porre `h = (N - 1) * q`, `lo = floor(h)`, `hi = ceil(h)` e
   restituire `x[lo] + (h - lo) * (x[hi] - x[lo])` in binary64. Il CI 95% usa
   esattamente `q = 0.025` e `q = 0.975`; il limite unilaterale usa esattamente
   `q = 1 - alpha_plan`. Valori `NaN` o infiniti fanno fallire il report.

4. **Ordine delle riduzioni annidate.** Tutti gli input numerici entrano nelle
   riduzioni come IEEE-754 binary64 finiti. La media per
   `evaluation_unit_key`, la media dentro `group_id`, la macro-media globale, la
   macro per profilo/dominio e le varianze del piano di potenza usano sempre:
   (a) record ordinati per chiave canonica
   `(metric_id, profile, domain, group_id, evaluation_unit_key,
   frame_or_band_id, decision_key, occurrence_ordinal)` dove i campi mancanti
   valgono
   stringa vuota; (b) prodotti `float64(weight) * float64(value)` calcolati in
   quell'ordine; (c) l'operatore normativo `sum_pairwise64(values)`, non una
   somma "pairwise" generica.

   `sum_pairwise64` e definito cosi: se `N == 0` il chiamante deve produrre
   `N/A` o report FAIL secondo la metrica; se `N == 1` restituisce
   `float64(values[0])`; altrimenti `mid = floor(N/2)` e restituisce
   `float64(sum_pairwise64(values[0:mid]) + sum_pairwise64(values[mid:N]))`.
   Ogni addizione arrotonda a IEEE-754 binary64. Numeratore pesato e denominatore
   dei pesi usano entrambi `sum_pairwise64` sul vettore gia ordinato.

   Le varianze sono a due passate: prima la media con questa regola, poi la
   somma `sum_pairwise64` degli scarti quadratici nello stesso ordine.
   `numpy.sum`, `math.fsum`, Kahan, BLAS parallelo, reduction native su ordine
   container, blocchi interni di libreria e `fast-math` sono vietati per gli
   artefatti di gate anche quando producono differenze numeriche piccole.

5. **Aritmetica del matching.** I valori binary64 delle operazioni scalari che
   precedono le somme, inclusi `log2`, IoU temporale e `band_iou_log2`, sono
   prodotti dalla reference implementation e verificati bit-per-bit da fixture
   normative. Le somme nell'obiettivo del matching sono poi confrontate come
   razionali esatti dei rispettivi bit pattern binary64. Questa aritmetica e
   distinta da `sum_pairwise64`, che resta l'operatore delle metriche finali.

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

Il matching regioni e bipartito one-to-one e si risolve per
`(evaluation_unit_key, problem_type)` con obiettivo lessicografico:

1. massimo numero di match validi;
2. massima somma dell'IoU temporale;
3. minima somma dell'errore dei bordi temporali;
4. minima somma del costo geometrico primario;
5. minima somma del costo geometrico secondario;
6. minima sequenza ordinata delle coppie di `decision_key`.

Per Resonance il costo primario e l'errore del centro in ottave e il
secondario e `1 - band_iou_log2`. Per gli altri tipi il primario e
`1 - band_iou_log2` e il secondario e la distanza in ottave fra i centri
geometrici delle bande.

La `decision_key` normalizzata contiene kind, evaluation-unit key, tipo,
tempi, tutta la geometria letta da eleggibilita/costi, direction, severity,
confidence e actionable. Non contiene ID o hash. Le coppie si ordinano per
`(GT decision_key, prediction decision_key, prediction occurrence_ordinal)`.
L'ordinale interviene soltanto fra prediction integralmente equivalenti per
tutte le metriche.

Le chiavi 1-5 dell'obiettivo sono somme di quantita per-arco e si realizzano
con un algoritmo di matching bipartito a costo minimo di complessita
polinomiale. La chiave 6 seleziona, fra le soluzioni ottime per le chiavi 1-5,
quella lessicograficamente minima secondo le coppie di `decision_key`: e una
proprieta dell'insieme della soluzione e non un peso per arco, e la procedura
deterministica che la realizza va progettata e citata prima che questa sezione
possa chiudere.

Non e ammesso in nessun caso un percorso greedy, un fallback posizionale o una
risoluzione approssimata: un'implementazione che non possa garantire l'ottimo
esatto entro le proprie risorse produce FAIL. Un FAIL e deterministico e
indipendente dall'ordine dei record; un risultato approssimato non lo sarebbe.

### 10.2 Metriche degli eventi

Un evento e matchabile solo se tipo uguale e IoU temporale almeno 0.3. Inoltre:

- Resonance: centro entro un terzo di ottava;
- Harshness/Sibilance: overlap di banda almeno 0.5.

Il matching eventi usa lo stesso obiettivo lessicografico a sei chiavi della
sezione 10.1, applicato a dynamic event e prediction event. La banda e quella
derivata da centro e larghezza secondo la sezione 9.2. ID e hash non
partecipano a matchabilita, costo, tie-break o ordinamento decisionale.

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

Ogni gruppo riceve peso totale uno. Dentro un gruppo, ciascuna
`evaluation_unit_key` unica riceve peso `1 / num_units`; dentro l'unita, il peso
viene diviso uniformemente fra le celle eleggibili. Duplicati, crop e derivati
con la stessa `evaluation_unit_key` vengono deduplicati prima del fit e della
misura. Questo schema di pesi e identico per fit del calibratore, ECE e Brier.

ECE usa 15 bin equal-mass: stable sort per `(confidence, group_id,
evaluation_unit_key, frame_or_band_id)`, poi partizione per peso cumulativo con
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

Il gate G2 rispetto a v2 usa un solo adapter omologo. L'artefatto di mapping e
gia serializzato e hashato da G1a T3; G1c ne e **consumer/validator**, non
owner. In G1c e vietato cambiare classi omologhe, classi `N/A`, soglie seed,
mapping profili, costanti o hash dell'adapter. Se il mapping risulta errato,
G1c si ferma e apre una reseal pre-G1c separata: non lo corregge dentro
l'evaluator. L'adapter copre le sei classi realmente attive in tutti e tre i
candidati G0: `Resonance`,
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
G1c deve avere almeno un test che rifiuta qualunque tentativo di promuovere
`Harshness` o `Sibilance` nel confronto omologo v2, qualunque riordino delle
sei classi e qualunque mismatch fra SHA dichiarato e artifact T3.

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
metrica con SHA-256 del suo ID canonico. La derivazione e:
`metric_seed = uint64be(first8(SHA256(b"20260719" + b"\x00" + metric_id_utf8)))`;
`metric_seed` e passato a `numpy.random.PCG64` come seed intero.

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

`sample_variance` usa il denominatore `n - 1` e la regola di somma della
sezione 10.0. Si fissa sempre la media alternativa a `lambda = 0.25`, senza
usare la media osservata. Per ogni `n >= 149`, 5000 simulazioni PCG64 estraggono
`n` rate di gruppo da `Gamma(shape=1/alpha_nb, scale=0.25*alpha_nb)` e poi
conteggi `Poisson(rate)`; con `alpha_nb = 0` usano direttamente
`Poisson(0.25)`. Ogni simulazione calcola il limite Poisson esatto e, con 2000
resample interni dei gruppi, il quantile `1 - alpha_plan` della media dei
conteggi da un minuto. Entrambi i limiti sono quindi espressi in eventi/minuto.

Pin specifici della simulazione false-events:

5. **Seed interno.** `outer_simulation_index` e zero-based (`0..4999`) e viene
   serializzato come unsigned 64-bit big-endian. Il seed interno e l'intero
   unsigned 64-bit big-endian formato dai primi 8 byte di
   `SHA256(metric_id_utf8 + b"\x00" + outer_index_u64be)`, passato a
   `numpy.random.PCG64`. Codifiche ASCII decimali, little-endian, 32-bit o
   conversioni tramite stringa sono vietate.

6. **Ordine dei draw annidati.** Per ogni `(metric_id, n)` si inizializza un
   solo RNG esterno da `metric_seed` e si iterano gli
   `outer_simulation_index = 0..4999` in ordine crescente. Dentro ciascuna
   simulazione, se `alpha_nb > 0`, si consumano prima tutti i `n` draw Gamma in
   ordine di indice gruppo `0..n-1`; poi si consumano tutti i `n` draw Poisson
   applicati alle rate nello stesso ordine. Se `alpha_nb = 0`, si consumano
   direttamente tutti i `n` draw `Poisson(0.25)` nello stesso ordine. E vietato
   interlacciare Gamma e Poisson per gruppo. I 2000 resample interni usano il
   seed interno sopra, generano una matrice `np.int64` di indici
   `integers(0, n, endpoint=False)` con shape `(2000, n)`, consumata row-major;
   ogni media di riga usa `sum_pairwise64` come definito nella sezione 10.0 e
   il quantile usa il pin type-7 della sezione 10.0.

Si sceglie il primo `n` per cui almeno il 90% delle simulazioni soddisfa
entrambi i limiti `<= 0.5`. Il gate finale usa il limite esatto e 10000
resample per gruppi con il seed evaluator sul pool reale, sempre con i pin
della sezione 10.0. Con zero eventi il limite Poisson simultaneo e
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
frame con timestamp fisico comune, dopo warm-up e prima della coda congelati
come sotto, e sole celle attive secondo il predicato di attivita di gate 4.

### 13.1 Warm-up, coda e porzioni stazionarie (fail-closed)

Warm-up e coda **non** sono parametri liberi del manifest post-generazione.
Sono formule chiuse, congelate in G1a e hashed nel lock; il manifest puo solo
ripetere i valori derivati dalle formule. Modificare warm-up/coda dopo la
generazione delle fixture per restringere la finestra di confronto e FAIL.

Con hop canonico `H = 1024`, `fs_c = 48000`, lunghezza minima LF
`N_LF = 8192` e `resampler_group_delay_seconds` come in §5:

```text
warm_up_seconds = resampler_group_delay_seconds + N_LF / fs_c + K_wu * H / fs_c
coda_seconds = K_coda * H / fs_c
```

con `K_wu = 4` e `K_coda = 4`. La somma e **additiva** (non un `max` fra i
due rami): `N_LF / fs_c` porta al primo frame LF-capable e `K_wu * H / fs_c`
esclude **ulteriori** quattro hop oltre quel punto, cosi shape, prominence e
delta non confrontano il bordo in cui la storia e azzerata e la fusione dual-
resolution e appena diventata disponibile. `K_coda = 4` simmetrizza il
trailing edge senza padding finale (§4.1). I campioni esclusi sono quelli con
`source_time < warm_up_seconds` oppure
`source_time > T_asset - coda_seconds`, dove `T_asset` e la durata fisica
dell'asset alla griglia `source_time`.

Per i confronti cross-SR (44.1 / 48 / 96 kHz) la finestra utile e
l'**intersezione** dei segmenti utili dopo warm-up e prima della coda su
ciascun rate (equivalente a usare il massimo dei warm_up e il massimo delle
code sulla stessa `source_time`); confrontare tratti non comuni e FAIL.

**Porzione stazionaria** (multitone e rumore): e l'intero segmento utile dopo
warm-up e prima della coda (**modalita (a)**, default obbligatorio per i gate
di sample-rate parity su multitone e rumore). Non e ammesso un sottoinsieme
post-hoc. In alternativa (**modalita (b)**), e soltanto se preregistrata in
G1a e hashed nel lock **e** dichiarata diagnostica / non usata per chiudere
il gate 4 di parity, una collezione di finestre che soddisfano tutte:

- durata almeno `T_min = 8 * H / fs_c` (otto hop);
- predicato di stabilita preregistrato: varianza della `mid_psd_db` media
  sulle 120 bande, calcolata hop-per-hop nella finestra, `<= 1.0 dB^2`, e
  max |Δ| hop-to-hop della stessa media `<= 0.5 dB`.

La scelta fra (a) segmento utile intero e (b) collezione di finestre e
fissata in G1a prima della generazione e hashed; non si cambia modalita dopo
aver visto un FAIL. **REV7 (B):** il closing set del gate 4 in G1 e solo
multitone / pseudo-rumore (stazionari) su dominio `R` (§13.2); per chiudere
su questi asset vale solo (a). Tutte le finestre della collezione
preregistrata in (b) entrano nel max |Δ| diagnostico; omettere una finestra
fallita e FAIL. Se nessuna finestra soddisfa il predicato, il report
diagnostico e FAIL (non si allarga `T_min` ne si alza la soglia di varianza
dopo aver visto i numeri).

**Sweep log (`log_sweep`) — REV7 (B):** la fixture resta generata e hashed
in SHA256SUMS. **Non chiude** il gate 4 di sample-rate parity in G1. Le
deviazioni restano misurate e pubblicate come **report-only** (preserve
information; zero costo sulla chiusura). La griglia di checkpoint
preregistrata in G1a (hashed), con almeno i centri critici di §13 (45, 60,
80, 250, 1000, 3500, 8000, 16000, 20000 Hz) piu gli estremi 20 e 20000 Hz
del path, resta il protocollo di misura report-only: per ciascun checkpoint
si confronta il frame il cui `source_time` e piu vicino all'istante in cui
lo sweep attraversa quella frequenza, entro al piu un hop. Omettere la
pubblicazione report-only dello sweep, o ammettere silenziosamente lo sweep
nel max di chiusura del gate 4, e FAIL di report / FAIL di perimetro.
Parity SR non-stazionaria e requisito nominato di **G1e-nonstat**, non di G1c
(proposta `docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_PROPOSAL.md` **PARKED**,
non cancellata). **A3 resta RETIRED**; famiglia ACTIVE chiusa — nessun reopen
via sweep.

Transient burst e risonanza smorzata restano fuori dal confronto dB di
parity spettrale: hanno il gate separato su onset, picco e decadimento gia
previsto al punto 4, per evitare che un arrotondamento frazionario venga
nascosto in un confronto dB non omologo.

### 13.2 Gate obbligatori

Gate obbligatori:

1. **Struttura**: 120 centri, estremi 20/20000 Hz, ordine stretto, timestamp e
   forme esatte.
2. **Determinismo**: due processi puliti producono artefatti byte-identici
   sulla stessa piattaforma e stesso lock.
3. **Streaming ≡ offline**: l'equivalenza non si dimostra con "chunk casuali"
   ad hoc. G1a congela e hasha nel lock:
   - seed PRNG fisso `PCG64(20260719)` (stesso seed dell'evaluator §10);
   - insieme minimo di schedule di chunk size (campioni host di input):
     `1`, `63`, `1024`, `4095`, `8192`, `8193`, piu una schedule geometrica
     casuale `floor(2 ** U)` con `U ~ Uniform[0, 14)` estratta dal PRNG
     preregistrato, di lunghezza almeno 32 chunk, ripetuta identica su ogni
     asset del set di gate.
   Per ogni schedule, l'input a chunk e l'input monolitico offline sullo
   stesso lock/piattaforma devono produrre la stessa sequenza di
   `V3FeatureFrame`: tutti i campi float32 del frame (§7), i timestamp
   razionali (`source_time_num`/`source_time_den`, `frame_end_sample`,
   `frame_index`) e i flag `mid_valid`/`side_valid`/`valid` (e il motivo
   enumerato). Obbligatori inoltre: (a) concatenazione multi-asset con reset
   esplicito della storia `delta_db` al confine asset; (b) silenzio
   intercalato fra asset; (c) identita streaming-vs-offline sullo stesso
   lock e piattaforma. Preferenza e regola di gate sulla **piattaforma di
   gate G1** (OS, arch, stack/numpy dichiarati e hashed nel lock del report):
   artefatti canonici **byte-identici** fra offline e streaming; la
   bit-identita float e **obbligatoria** su quella piattaforma. La tolleranza
   max |Δ| assoluto `<= 1e-6` su ogni float32 (con timestamp razionali
   identici e bit dei flag `valid` identici) e ammessa **solo** per
   piattaforme secondarie **preregistrate** nel lock G1a, resta report-only
   e **non** chiude il gate G1. Dichiarare ad hoc "piattaforma non
   bit-identical" senza allowlist preregistrata → FAIL.
4. **Sample-rate parity**: rispetto al render 48 kHz, su 44.1 e 96 kHz, e
   sulle porzioni definite in §13.1,
   `max_i |x_i(sr) - x_i(48k)| <= 0.25 dB` dove `x` scorrono **ogni**
   elemento del dominio dB dichiarato:
   `mid_psd_db[120]`, `side_psd_db[120]`, `mid_shape_db[120]`,
   `side_shape_db[120]`, `mid_prominence_db[120]`,
   `side_prominence_db[120]`, e gli scalari `mid_level_dbfs` /
   `side_level_dbfs` quando il rispettivo canale e valido.
   Predicato di attivita (nessuna maschera post-hoc): una cella di PSD e
   attiva sse `max(psd_db_ref, psd_db_sr) > -120` (unione: strettamente sopra
   il floor `1e-12` / clamp di §6.1 sul riferimento 48 kHz **oppure** sul
   render sotto test). Cosi un artefatto presente solo a 44.1/96 kHz non
   scompare dal max |Δ|. Shape e prominence della stessa banda ereditano
   l'attivita della PSD del medesimo canale; i vettori di un canale con
   `*_valid == false` restano ignorati come in §7. Questo predicato e del
   solo gate SR; i criteri −100 dBFS/Hz di §10 evaluator restano invariati
   (gate diversi).
   **REV7 (A) — dominio geometrico `R` e report-only ∉R.** Sia `R` l'insieme
   a priori degli indici di banda che soddisfano **entrambi** su ogni path di
   fusione contribuente (fail-closed in crossfade: LF e MAIN devono
   soddisfare): (1) occupancy ENBW del supporto triangolare
   `N_eff(i, p) ≥ N_MIN` con `N_MIN = 2 = ceil(ENBW_Hann)`; (2) separazione
   neighbour-centre `sep_bins(i, p) ≥ SEPARATION_MIN_BINS` con
   `SEPARATION_MIN_BINS = 2` (= main-lobe null-to-null / 2). Il max |Δ| che
   **chiude** il gate 4 scorrono solo celle attive con `i ∈ R` (e gli
   scalari level ammessi come gia nello scope). Le bande `i ∉ R` sono
   **report-only**: non entrano nel max di chiusura; devono comunque essere
   misurate e pubblicate (tabella o equivalente machine-readable con almeno
   asset/portion, SR sotto test, ogni indice `i ∉ R`, field id, `|Δ|` quando
   ammissibile, e max|Δ| report-only su ∉R). Omettere questa pubblicazione, o
   dichiarare PASS di SR-parity "su tutta la griglia 20 Hz–20 kHz" mentre `R`
   esclude la regione bassa, e FAIL di report. Tag ammessi sulle bande
   escluse: `EXCLUDED_GEOMETRY` (o finer: under-resolved vs main-lobe) —
   mai silent PASS. Motivo obbligatorio in linguaggio di chiusura: la griglia
   120 bande e piu fine della separazione neighbour sotto Hann periodico;
   sotto quel limite la parity per-banda non e una claim di chiusura ben
   posta. Vietato: tolleranza dB LF alternativa; shopping di `N_MIN` /
   `SEPARATION_MIN_BINS` contro celle misurate.
   **REV7 (B) — closing set.** In G1 chiudono il gate 4 solo gli asset
   stazionari `multitone` e `pseudo_noise` (porzione §13.1 modalita (a))
   valutati su `R`. `log_sweep` **non chiude** (report-only; §13.1).
   Parity SR non-stazionaria → debito nominato **G1e-nonstat**, fuori da G1c.
   A3 RETIRED / ACTIVE chiusa. Debito G4 (invariato): stabilita cross-SR delle detection sulle
   classi low-end (mud/boom/boxy) — **non** soddisfatta dal solo PASS del
   gate 4 su `R`.
   **Una sola cella attiva in-R fuori soglia → FAIL dell'intero gate.** Non
   si sostituisce il max con media, p95, RMSE o sottoinsieme di bande scelto
   dopo aver visto gli errori. `mid_delta_db` / `side_delta_db` **non**
   entrano in questo gate 0.25 dB (il delta e un derivato temporale gia
   coperto da gain invariance `<= 0.05 dB` e dall'identita streaming≡offline);
   includerli nel max SR sarebbe un dominio diverso e non li si usa per
   mascherare un fallimento su PSD/shape/prominence. Timestamp entro un
   campione canonico. Sui transienti: onset e frame di picco entro un
   campione canonico e tempo di decadimento entro un hop, senza confronto
   ottenuto spostando manualmente i frame. I sample rate 88.2 / 176.4 /
   192 kHz restano sperimentali (§4.1) e non possono chiudere questo gate.
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
   fallimenti attesi; permutare righe o duplicare un record top-level
   normalizzato identico non cambia il report. Superficie anomaly mancante,
   score thresholded e zero
   eventi con esposizione insufficiente devono fallire. Policy assente/hash
   errato, threshold mutato o actionable non riproducibile devono fallire; le
   fixture power devono provare che il gate congiunto non usa la sola Poisson.

   **G1c T0 — registry obbligatoria prima del codice evaluator:** G1c non puo
   implementare parser/matcher/metriche finche non ha committato una registry
   canonica con esattamente queste 14 fixture gate-9 e il loro expected outcome.
   La registry e un artifact G1c, non un file implicito nel test.

   | ID | Expected outcome |
   |---|---|
   | `evaluator_perfect_prediction` | PASS: metriche perfette e report canonico |
   | `evaluator_empty_prediction` | FAIL: FN/recall non perfetto su GT positivo |
   | `evaluator_wrong_class` | FAIL: FP classe errata + FN classe corretta |
   | `evaluator_wrong_frequency` | FAIL: evento non matchabile / errore frequenza oltre tolleranza |
   | `evaluator_inverted_sign` | FAIL: errore di segno su celle tonali attive |
   | `evaluator_duplicate_predictions` | FAIL: matching one-to-one lascia duplicato come FP |
   | `evaluator_row_permutation` | PASS-INVARIANT: report byte-identico alla fixture base |
   | `evaluator_duplicate_evaluation_unit` | PASS-INVARIANT: dedup canonica, stesso report della base |
   | `evaluator_missing_anomaly_surface` | FAIL: superficie dichiarata/necessaria assente |
   | `evaluator_thresholded_score_surface` | FAIL: superficie score pre-threshold non ricostruibile |
   | `evaluator_zero_events_insufficient_exposure` | FAIL: esposizione insufficiente, mai PASS a zero eventi |
   | `evaluator_policy_ref_invalid` | FAIL: policy assente o hash policy errato |
   | `evaluator_policy_mutated_outputs` | FAIL: threshold mutato o actionable non riproducibile |
   | `evaluator_power_joint_false_events` | FAIL se il gate congiunto passa usando solo Poisson o saltando il limite cluster-bootstrap |

   Le 14 fixture sono file schema-v2 eseguibili, non sola prosa nella registry.
   Il runner e il report golden sono obbligatori. Registry, JSON, superfici raw
   e golden entrano tutti in `G1C_SHA256SUMS_REQUIRED`. Un test reject
   aggiuntivo prova stesso ID dichiarato con `instance_key` diversa.

   La fixture authority minima usa gli asset 48 kHz sigillati:
   `multitone [0,0.5)` produce `T=16` e mask di 1920 byte tutta uno;
   `multitone [0.5,1.0)` produce `T=23`; `silence [0,0.5)` produce `T=16`,
   mask tutta zero e score/severity di 23040 byte tutti `+0.0`. Le reject
   fixture coprono traversal, symlink, truncation/trailing bytes, hash errato,
   NaN/Inf/out-of-range, mask `2`, `-0.0` e valore nonzero sotto mask zero per
   ciascuna classe.
10. **Ambiente**: sync del lock con hash, test completi e deep hash di tutte le
   fixture PASS.

La soglia 0.25 dB e hard sul **max** assoluto del dominio di **chiusura**
dichiarato al punto 4 (celle attive con `i ∈ R` sul closing set stazionario).
Non si sostituisce con una media o un p95 per nascondere una banda fuori
contratto. Il max report-only su `i ∉ R` (e su `log_sweep`) non chiude il
gate e non autorizza rilassamenti.

Gli artefatti numerici canonici sono array little-endian `.npy` senza oggetti,
tranne `frontend_input_ref` e le tre superfici della sezione 9.8, che sono raw
con formato e lunghezza gia prescritti. Gli artefatti strutturati sono
JSON UTF-8 **senza BOM**, con chiavi ordinate per code point Unicode,
separatori esatti `,` e `:` (niente spazi), newline LF finale **singola
inclusa nell'hash**, `allow_nan=false` / soli float finiti. In lettura si
rifiutano token `NaN`/`Infinity`/`-Infinity`, overflow tipo `1e400` e chiavi
oggetto duplicate. Si ordinano **solo** i set dichiarati order-independent;
curve, score e griglia 120 bande restano nell'ordine semantico. Archivi
ZIP/NPZ con timestamp non sono usati come prova byte-identica.

## 14. Implementazione e commit atomici dopo il GO

Ordine obbligatorio:

1. **G1a - contract artifacts**: JSON schema di manifest, admission batch,
   annotazione, prediction, calibration policy e piano di potenza; griglia 120
   bande, split roles, commit-reveal, copertura calibration, mapping adapter
   v2-v3, generatori fixture e hash. Nessun frontend ancora.
2. **G1b - canonical frontend**: resampler streaming, dual-resolution
   time-aligned, `V3FeatureFrame` Python e unit test.
3. **G1c - evaluator**: parser fail-closed, matching, metriche, CI group-level,
   validazione dell'adapter omologo v2-v3 gia serializzato da G1a T3 e fixture
   di errore. Il vincolo storico REV7 di non modificare lock/SHA vale durante
   la preparazione candidata; il solo switch atomico REV8 della sezione 14.1
   aggiorna insieme i digest richiesti. G1c non modifica il mapping adapter ne
   la parity SR non-stazionaria.
4. **G1d - competitor protocol harness**: manifest/config/hash e verifica dei
   render; nessun render proprietario nel repository.
5. **G1e - report**: esecuzione completa dei gate, hash degli output e tabella
   PASS/FAIL. La parity SR non-stazionaria vive qui come tranche dedicata
   **G1e-nonstat**: usa la proposta sweep parcheggiata come input, non riapre
   A3/ACTIVE e non cambia il closing set G1 gia consolidato.

### 14.1 Sequenza REV8 candidate-to-active

REV7 resta viva durante tutta la preparazione. Schema, parser, loader,
primitive, T2 e T3 REV8 vivono in moduli v2 esplicitamente versionati, non
esportati dal package attivo e irraggiungibili dal dispatcher REV7.

Ordine obbligatorio:

1. candidato REV8 e digest, senza modificare contratto vivo;
2. primitive `N64`, identity/decision key, aritmetica razionale e test;
3. frontend lock, schema/generatore/validator dell'indice e fixture;
4. surface loader fail-closed e test adversarial;
5. T2 eventi e T3 regioni candidati con oracle;
6. materializzazione ed esecuzione delle 14 fixture Gate 9 v2;
7. commit atomico che cambia insieme contratto vivo, schema/registry v2,
   dispatcher, parser, trusted-index admission, surface loader, T2, T3,
   goldens, tripwire, PLAN e SHA256SUMS.

Il commit di switch produce soltanto `REV8_ACTIVE`. `G1C_REV8_CLOSE` richiede
successivamente entrypoint pubblico v2 verde, EVAL-MVP e tutte le 14 fixture
Gate 9 verdi. Nessuno dei due stati promuove il prodotto.

Il test post-switch sull'entrypoint pubblico deve accettare esclusivamente
annotation/prediction/policy v2, rifiutare v1 e batch misti e dimostrare
l'invocazione effettiva di trusted-index admission, surface loader, T2 e T3.

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
- schema v1, batch misti o consumer REV7 restano raggiungibili
  dall'entrypoint REV8;
- indice, frontend input o mask possono essere sostituiti dal candidato o
  divergono dal run manifest trusted;
- final-test viene letto per scegliere una decisione;
- una dipendenza non e bloccata con hash;
- serve modificare runtime o `ml_v2` per far passare G1.

Rollback prima dello switch: si eliminano soltanto i commit candidati non
promossi. Rollback dopo lo switch: forward-revert atomico dell'intero commit
di attivazione e ritorno al tip REV7 immediatamente precedente; non si torna a
G0 e non si eseguono revert parziali di schema, dispatcher o digest.

## 16. Criterio di approvazione

Questo criterio riguarda il commit document-only del candidato. Il successivo
commit di attivazione segue invece l'inventario atomico della sezione 14.1.

Il reviewer deve verificare questo contratto su commit immutabile e restituire
una sola lista consolidata. Il GO richiede:

- nessuna contraddizione con `docs/MOTORE_V3_PLAN.md`;
- specifiche implementabili senza decisioni aperte nascoste;
- nessun percorso di leakage o contaminazione del final-test;
- metriche separate per bilanciamento tonale e anomalie dinamiche;
- supporti calibration clean/positivi e benchmark group-level falsificabili;
- baseline non omologhe trattate come `N/A`, mai come vittorie artificiali;
- gate abbastanza severi da rendere falsificabili le claim successive;
- diff del commit limitato a questo documento e, se presente per coerenza
  letterale del max |Δ|, alla sola riga G1 di parity in
  `docs/MOTORE_V3_PLAN.md`;
- `docs/MOTORE_V3_G1_CONTRACT.md` REV7 e relativi digest restano byte-identici
  nel commit del candidato;
- nessun path `ml_v3/contracts/`, frontend, fixture, test harness o altro
  codice G1 nello stesso commit document-only.

Fino al GO storico su REVISIONE 6: nessun file G1 di codice, fixture o
ambiente veniva creato. Quella REVISIONE 6 CONSOLIDATA + micro-amend non
costituiva da sola GO a G1a (GO G1a successivo, separato).

**REVISIONE 7 CONSOLIDATA:** non costituisce G1 PASS di prodotto; non
promuove tip ufficiale G1b; non riapre A3/ACTIVE; non ammette lo sweep nel
closing set; non rilassa 0.25 dB / max / `R`. Il rehash coordinato di
metrology lock + SHA256SUMS e obbligatorio nello stesso pacchetto di
consolidamento (mai silent).
