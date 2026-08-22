# Ember Core Premium — Autorità esecutiva V1

## Metadati e stato

```text
Documento                         = autorità esecutiva canonica
Anchor tecnico auditato           = 78392e872af19b0a35e1525da9650551bacad87f
Baseline esecutiva                = 63203ba3ca4cb3cac179096997be617c069b28ea
Branch                            = feature/ember-core-unified
Hardening DSP                     = GO dopo freeze docs-only
Bake-off harness                  = GO
Render bake-off                   = NO-GO fino ai protocolli firmati
Release commerciale / prezzo €89 = NO-GO fino ai gate finali
```

La baseline esecutiva discende direttamente dall'anchor tecnico. Il delta
fra i due commit aggiunge esclusivamente il gate AI su file reali e la sua
registrazione CMake; non modifica il runtime DSP o il processor del plugin.

Dal commit docs-only che introduce questo documento, il bersaglio torna a
essere il codice. Il testo non deve essere riaperto salvo un blocker
dimostrabile di correttezza, sicurezza real-time, migrazione o latenza.

Autorità tecniche:

- Cytomic TPT-SVF: <https://cytomic.com/technical-papers/>
- ITU-R BS.1770-5: <https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en>
- TDR NOVA GE: <https://www.tokyodawn.net/tdr-nova-ge/>
- Manuale NOVA GE: <https://docs.tokyodawn.net/nova-ge-manual/>

## 1. Domini e sicurezza numerica

```text
fs_host       in [32 kHz, 192 kHz]
oversampling  in {1, 2, 4}
fs_processing = fs_host * oversampling <= 768 kHz
```

Il limite utente è:

\[
20 \le f \le \min(20000, 0.49 f_{\mathrm{host}}).
\]

Designer, TPT e detector usano `fs_processing`. I casi 96 kHz x4 e
192 kHz x4 devono produrre bande attive, coefficienti finiti e nessun
bypass silenzioso.

### 1.1 Surgical e Vintage

Surgical è lineare: usa TPT-SVF e non applica saturazione di banda.
Un'eventuale saturazione Surgical futura deve essere un'opzione separata,
default OFF e vietata nel bake-off core.

Vintage conserva esattamente il percorso seguente:

```cpp
constexpr float drive = 1.2f;
constexpr float invDrive = 1.0f / drive;

const float z = sample * drive;
output = fastTanhApprox(
    std::clamp(z, -64.0f, 64.0f)
) * invDrive;
```

Fast-math è disabilitato. Nel dominio `abs(z) <= 64`, precisione,
costanti e ordine delle operazioni restano invariati.

Failure policy:

- input non finito: output 0, reset e fault counter;
- stato/output non finito con input finito: dry sample, reset e fault;
- limiter con NaN/Inf: output 0 e fault counter.

### 1.2 Limiter globale

\[
y(x)=
\begin{cases}
x,&|x|\le 8\\
\operatorname{sgn}(x)\left[8+32\tanh\left(\frac{|x|-8}{32}\right)\right],&|x|>8.
\end{cases}
\]

- Vintage pre-limiter deve essere bit-identico nel dominio protetto.
- Per `abs(x) <= 1`, l'errore massimo è `3.5e-4`.
- Il residuo RMS deve essere al massimo -72 dBFS soltanto sulle fixture
  audio preregistrate.
- Sopra `abs(x) = 1`, la differenza è una safety change intenzionale.
- Tutti i test lineari vengono eseguiti pre-limiter.

## 2. Filtri

### 2.1 TPT-SVF

\[
g_0=\tan(\pi f/f_{\mathrm{processing}}),\qquad
k_0=1/Q,\qquad
A=10^{G/40}.
\]

| Tipo | \(g\) | \(k\) | \((m_0,m_1,m_2)\) |
|---|---:|---:|---|
| Bell | \(g_0\) | \(k_0/A\) | \((1,k_0A,1)\) |
| Low Shelf | \(g_0/\sqrt A\) | \(k_0\) | \((1,k_0A,A^2)\) |
| High Shelf | \(g_0\sqrt A\) | \(k_0\) | \((A^2,k_0A,1)\) |

\[
\begin{aligned}
d&=1+g(g+k)\\
t_0&=x-ic_2\\
v_0&=t_0/d-(g+k)ic_1/d\\
t_1&=gv_0,\qquad v_1=ic_1+t_1\\
t_2&=gv_1,\qquad v_2=ic_2+t_2\\
ic_1&\leftarrow ic_1+2t_1\\
ic_2&\leftarrow ic_2+2t_2\\
y&=m_0v_0+m_1v_1+m_2v_2.
\end{aligned}
\]

Requisiti:

- `g`, `k`, `d`, `m_i` e `ic_i` finiti;
- `d > 0`;
- coefficienti e stati in `double`;
- cast a `float` soltanto all'uscita;
- `Q` in `[0.1, 10]`;
- gain totale limitato a +/-36 dB;
- Surgical Shelf con `Q = 1/sqrt(2)`; Q host conservato ma inattivo.

### 2.2 Biquad e Jury

Dopo la quantizzazione devono valere:

\[
1+a_1+a_2>10^{-6}
\]

\[
1-a_1+a_2>10^{-6}
\]

\[
1-a_2>10^{-6}.
\]

Numeratore e denominatore devono essere finiti. Una sezione invalida
bypassa l'intera banda.

```text
resonance = 1  -> Butterworth canonico
altro valore   -> Resonant Cascade
```

L'oracolo usa MPFR adattivo fino a un arrotondamento univoco. Se almeno
una risposta è sotto -120 dB, il confronto è complesso lineare.

## 3. Dynamic EQ

### 3.1 Transfer function

```text
ratio rho     in [1, 20]
knee W        in [0, 24] dB
range D       in [0, 48] dB
threshold T   in [-60, 0] dB
```

Con `u = L - T`, per `W = 0`:

\[
\phi_A=\max(u,0),\qquad \phi_B=\min(u,0).
\]

Per `W > 0`:

\[
\phi_A=
\begin{cases}
0&u\le-W/2\\
(u+W/2)^2/(2W)&|u|<W/2\\
u&u\ge W/2
\end{cases}
\]

\[
\phi_B=
\begin{cases}
u&u\le-W/2\\
-(u-W/2)^2/(2W)&|u|<W/2\\
0&u\ge W/2.
\end{cases}
\]

| Action | Trigger | \(\Delta G_{\mathrm{raw}}\) |
|---|---|---|
| Compress | Above | \(-(1-1/\rho)\phi_A\) |
| Compress | Below | \(-(1-1/\rho)\phi_B\) |
| Expand | Above | \((\rho-1)\phi_A\) |
| Expand | Below | \((\rho-1)\phi_B\) |

\[
\Delta G=\operatorname{clamp}(\Delta G_{\mathrm{raw}},-D,+D)
\]

\[
G_s=\operatorname{clamp}(G_{\mathrm{static}},-36,+36)
\]

\[
G_{\mathrm{eff}}=\operatorname{clamp}(G_s+\Delta G,-36,+36)
\]

\[
\Delta G_{\mathrm{applied}}=G_{\mathrm{eff}}-G_s.
\]

```text
dynamicGainDb       = DeltaG_applied
effectiveBandGainDb = G_eff
```

### 3.2 Detector temporale

Peak linked:

\[
q[n]=\max(|L[n]|,|R[n]|).
\]

RMS linked:

\[
q[n]=(L[n]^2+R[n]^2)/2.
\]

Per mono si usa il solo canale. Con `tau` in secondi:

\[
\alpha(\tau)=e^{-1/(\tau f_{\mathrm{processing}})}.
\]

Si usa attack se `q[n] > e[n-1]`, altrimenti release:

\[
e[n]=\alpha_ne[n-1]+(1-\alpha_n)q[n].
\]

\[
L_{\mathrm{Peak}}=20\log_{10}(\max(e,10^{-8}))
\]

\[
L_{\mathrm{RMS}}=10\log_{10}(\max(e,10^{-16})).
\]

Floor pubblicato: -160 dBFS. Non viene applicato un secondo
attack/release smoother al gain.

### 3.3 ABI e slew

```text
DynMode 0 = Off
DynMode 1 = Compress + DynTrigger
DynMode 2 = Expand + DynTrigger
DynMode 3 = Expand + Below, con precedenza su DynTrigger
```

- La nuova UI non genera 3.
- Gate UI genera 2 + Below.
- La copia serializzata può essere canonicalizzata; APVTS vivo no.

Slew:

- frequenza in log2: 100 ottave/s;
- Q in log2: 100 ottave/s;
- gain: 2400 dB/s;
- tipo/CurveMode: crossfade di 128 campioni;
- richieste durante il fade: last-wins dopo il completamento.

## 4. Stato

La root APVTS contiene la proprietà non-host:

```text
stateSchemaVersion = 1
```

- Fresh instance: versione 1, Surgical.
- Proprietà assente: migrazione Legacy.
- Versione 1: load normale.
- Versione maggiore di 1: rifiuto fail-closed, stato corrente invariato.
- Ogni salvataggio scrive versione 1.
- Legacy `DynMode = 3`: Expand + Below.

## 5. Sidechain e gain match

### 5.1 Sidechain

- Stereo: L/R linked.
- M/S soltanto se il main è M/S.
- External mono pilota entrambi i percorsi.
- External stereo usa la stessa matrice M/S.
- `DetectorAvailability` è runtime e non viene serializzato.

```text
ZL           -> fs_host
Natural/HQ   -> sidechain oversamplato identicamente
Linear Phase -> sidechain ritardato di D_main-preDynamic
Lookahead    -> ritarda successivamente soltanto il main
```

L'errore massimo di allineamento è un campione.

### 5.2 K-weighted windowed level

\[
L=-0.691+10\log_{10}\left(\sum_cG_c\frac1N\sum_ny_c[n]^2\right),
\qquad G_c=1.
\]

I coefficienti K sono congelati come bit binary64 in un artefatto
versionato.

\[
G_{\mathrm{target}}=
\operatorname{clamp}(L_{\mathrm{pre}}-L_{\mathrm{post-raw}},-12,+12).
\]

`post-raw` precede gain match, gain manuale e limiter.

Se `L_pre <= -70 LKFS`:

```text
G_target = 0 dB
ritorno con tau = 100 ms
```

Warm-up:

```text
una finestra completa di 400 ms
+ delay di allineamento corrente
```

Tutte le finestre precedenti sono ineligible. L'hop è 100 ms. Il test
offline simula esplicitamente il PDC dichiarato.

## 6. Undo manuale

- mouse-down: snapshot candidato;
- commit soltanto se cambia almeno un parametro;
- mouse-up: commit;
- Escape: restore e discard;
- wheel/tastiera: coalescing 300 ms;
- multi-banda: una sola azione;
- host automation esclusa;
- `beginChangeGesture/endChangeGesture` sempre bilanciati.

## 7. Performance

Prima della misura viene creato e hashato:

```text
EMBER_PERFORMANCE_PROFILE_V1.json
```

### 7.1 Ambiente

```text
MacBookPro18,3
Apple M1 Pro 8-core, 16 GB
macOS 15.5
Apple Clang 17
Release
alimentazione AC
Low Power Mode OFF
```

### 7.2 Fixture

Main:

```text
decorrelated stereo pink noise
PCG64 seed 20260822
RMS -18 dBFS
burst full-band di 100 ms ogni 2 s
```

External sidechain:

```text
decorrelated stereo pink noise indipendente
PCG64 seed 20260823
RMS -18 dBFS
stesso burst schedule
```

Durata: 10 s warm-up + 60 s misura.

### 7.3 Profilo DSP

18 bande statiche:

```text
HP: 30 Hz, resonance 1, 48 dB/oct
LS: 80 Hz, +6 dB, Surgical
14 Bell:
  f = [120,180,270,400,600,900,1350,2000,3000,4500,6700,9000,12000,15000]
  gain alternato +6/-6 dB
  Q ciclico [0.7,2,6]
HS: 12 kHz, -6 dB, Surgical
LP: 18 kHz, resonance 1, 48 dB/oct
```

6 Bell dinamiche:

```text
f = [80,250,1000,3500,8000,14000]
Q = [1,2,4,6,8,10]
gain = 0 dB
Compress Above
threshold = -24 dB
ratio = 4:1
attack = 10 ms
release = 100 ms
knee = 6 dB
range = 12 dB
DetectorSource = ExternalFiltered
SC frequency = frequenza della relativa banda dinamica
SC Q = Q della relativa banda dinamica
channel mode = Stereo linked
```

Matrice:

```text
44.1/48/96 kHz
64/128/512 campioni
ZL/Natural 4x/Linear Phase
1/8 istanze
editor closed/open
```

Editor open: esattamente uno, analyzer alla massima risoluzione e
frequenza selezionabile.

\[
quantum_{\mathrm{seconds}}=block\_size/f_{\mathrm{host}}.
\]

- Tempi monotonic-wall dopo il warm-up.
- p99: elemento `ceil(0.99*N)`, indice 1-based.
- Otto istanze: somma degli otto `processBlock` nello stesso quantum.
- Deadline miss se il tempo aggregato supera il quantum.

PASS:

```text
1 istanza: p99 <= 25% quantum
8 istanze: p99 <= 75% quantum
deadline miss = 0
```

Il baseline registra commit, profile SHA, fixture SHA, binari, JUnit,
CMake, compiler, OS/hardware e test attivi/disabilitati/mancanti.

Release gate:

- ASan/UBSan/TSan;
- zero allocation/mutex/resize nel callback;
- pluginval strictness 10;
- `auval`;
- macOS VST3/AU firmati e notarizzati;
- Windows VST3 validato;
- artifact hash e install test.

## 8. Bake-off preregistrato

Prima dei render devono essere firmati:

```text
EMBER_NOVA_OBJECTIVE_PROTOCOL_V1.json
EMBER_NOVA_HUMAN_PROTOCOL_V1.json
```

### 8.1 Protocollo oggettivo

Per task:

```text
task_id intero
almeno 12 source_group indipendenti
fixture/source hash
target_kind FINITE_GAIN|MUTE
target hash
parameter grid
regioni target/off-target
metrica
hard constraint
crossing/timing
reason code
margine
seed = 20260822 + task_id
```

```text
Ember Semantic OFF
NOVA Smart Operations OFF
```

Esclusioni:

- prima dello sblinding;
- soltanto con reason code preregistrati;
- almeno 12 source group validi dopo le esclusioni;
- altrimenti il task è NO-GO.

Per metriche lower-is-better:

\[
\delta_s=E_s-N_s.
\]

Per metriche higher-is-better:

\[
\delta_s=N_s-E_s.
\]

In entrambi i casi:

\[
\delta_s>0\Rightarrow\text{Ember peggiore}.
\]

Lo statistico bootstrap è:

\[
\operatorname{mean}(\delta_s).
\]

Ogni replica ricampiona gli indici `source_group` e mantiene unita la
coppia Ember/NOVA dello stesso gruppo.

- 100.000 repliche PCG64;
- UCB95: elemento 95.000, indice 1-based;
- ogni hard constraint deve passare su ogni source group valido;
- PASS se tutti gli hard constraint sono verdi e UCB <= margine.

Gain trace:

- render ON/OFF;
- STFT Hann 4096, hop 256;
- T9 usa impulso/cross-correlazione per il sample alignment.

```text
FINITE_GAIN + ON=0 -> errore +infinito e hard FAIL
MUTE               -> residual_energy_dbfs con soglia preregistrata
```

### 8.2 Protocollo umano

Esattamente 12 partecipanti validi:

- esclusi sostituiti prima dello sblinding;
- nessun tredicesimo;
- ordine controbilanciato;
- ascolto cieco;
- level match <= 0.1 LKFS;
- alignment <= 1 campione.

Workflow:

```text
almeno 10/12 più veloci con Ember
rapporto mediano <= 0.80
UCB95(mean(errors_Ember-errors_NOVA)) <= 0.5 errori/10 task
```

Ascolto:

- nessuna preferenza conta come non-Ember;
- Ember almeno 10/12;
- binomiale bilaterale:

\[
p\le0.038574.
\]

## 9. Sequenza e stop-rule

Commit atomici successivi al freeze:

1. baseline;
2. domini e safety;
3. TPT/Jury;
4. detector/dynamics;
5. ABI/state;
6. sidechain/gain match;
7. undo;
8. performance/release;
9. harness;
10. protocolli firmati;
11. render.

Stop-rule:

- failure matematico, RT, di stato o latenza: stop;
- nessuna soglia modificata nella tranche che la misura;
- nessun render prima del freeze dei protocolli;
- €89 resta NO-GO fino a release, bake-off e validazione commerciale.

Assunzioni: host massimo 192 kHz, processing massimo 768 kHz, niente
AAX, niente training ML, Semantic opt-in Beta.
