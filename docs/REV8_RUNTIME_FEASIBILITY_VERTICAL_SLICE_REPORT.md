# REV8 — Runtime feasibility vertical slice

**Status:** `LAB_ONLY_NOT_NORMATIVE_NO_RT_PROOF` · **`REV8 SPEC GO = NO`**

## 1. Snapshot verificato

```text
branch      feature/motore-v3-rev8-spec-go
base commit a8019e6ce9e37a7060e252d3bfc7ef3376566391
worktree    pulito prima della tranche
baseline    657/657 PASS, misurata prima di toccare qualunque file
dopo        681/681 PASS
ship-line   Source/ ml_v2/ CMakeLists.txt Resources/ AIEQ-mac/ = 0-diff vs G0
AGENTS.md   assente
```

Due condizioni di stop-rule risultano **entrambe non soddisfatte** e sono state
gestite come il mandato prevede, non aggirate:

- **nessun backend ML dichiarato** per V3 (né `MOTORE_V3_PLAN.md` né il
  candidate contract nominano RTNeural/ONNX/TFLite);
- **nessun target hardware dichiarato**.

Le architetture invece **non sono state inventate**: G4 del piano nomina già la
baseline DSP, il TCN causale e il modello spettro-temporale compatto, e demanda
a un futuro contratto G4 «forme tensoriali, causalita, pooling e budget
compute». Quel contratto non esiste, quindi le taglie qui misurate sono sonde
worst-case, non proposte.

## 2. Scope e file

Perimetro isolato, come imposto in assenza di autorizzazione ship-line:

```text
ml_v3/runtime_feasibility/__init__.py     package lab-only
ml_v3/runtime_feasibility/envelope.py     envelope derivati/dichiarati
ml_v3/runtime_feasibility/surrogate.py    modelli worst-case a pesi casuali
ml_v3/runtime_feasibility/benchmark.py    percentili, concorrenza, ambiente
ml_v3/runtime_feasibility/isolation.py    prototipo di control-flow
ml_v3/tests/test_g1c_runtime_feasibility.py   24 test
```

Nessuna modifica a `ml_v3/contracts/__init__.py`; il pacchetto resta invisibile
al dispatcher REV7 (pinnato da test).

## 3. Cosa è derivato e cosa è dichiarato

Derivato da artefatti congelati, **non scelto qui**:

| Grandezza | Valore | Fonte |
|---|---|---|
| bande | 120 | `contracts/constants.py` |
| feature per frame | **962** = 8×120 + 2 | `frontend/feature_frame.py` |
| hop | 1024 campioni | metrology lock |
| cadenza analisi | **8/375 s = 21,333 ms** | hop / 48 kHz |

Dichiarato lab-only: le topologie, il semaforo di margine (50 %/75 %), il
numero di istanze. Il semaforo resta **diagnostico**: comparire in un prompt non
lo rende norma di prodotto.

## 4. Misure

Apple M1 Pro, 6 core performance + 2 efficiency, macOS Darwin 24.5.0 arm64,
CPython 3.12.13, torch 2.13.0 eager senza JIT, 1 thread intra-op per istanza,
250 iterazioni, 40 di warm-up escluse dalla steady-state.

| Envelope | par. | 1 ist. | 4 ist. | 8 ist. | 16 ist. |
|---|---:|---:|---:|---:|---:|
| `baseline_min` | 15.816 | 0,2 % | 3,3 % | 8,1 % | 17,4 % |
| `tcn_small` | 101.960 | 1,7 % | 4,2 % | 14,8 % | 34,6 % |
| **`tcn_worst`** | **247.048** | **4,4 %** | **6,3 %** | **19,9 %** | **49,6 %** |
| `spectro_worst` | 115.976 | 38,3 % | **106,4 %** | **250,0 %** | **1212,1 %** |

Percentuali = p99 sull'hop. `tcn_worst` ha 127 frame di campo recettivo, cioè
2,71 s di contesto — limite superiore generoso, il Motore v2 ne usava ~750 ms.

**Il numero di parametri non predice il costo.** `spectro_worst` ha **meno**
parametri di `tcn_worst` (116k contro 247k) e costa **9× di più** a istanza
singola: la convoluzione 2-D su 8×120×64 domina.

### Un errore di misura, corretto

Il primo sweep è stato eseguito con load average **~100** su 8 core e i suoi
numeri multiistanza misuravano quella contesa, non il modello — `tcn_small` a 4
istanze risultava 18,8 ms contro gli 0,9 ms reali. Il benchmark ora **registra
il load average** in ogni riga proprio per impedire che un campione contenduto
venga riletto come dato. Anche le misure qui riportate sono a load ~11, non a
macchina scarica: i verdi hanno quindi margine reale, i rossi potrebbero essere
meno rossi.

## 5. Test

24 test mirati, suite completa 681/681. Coprono:

- forma e cadenza derivate dagli artefatti, non hardcoded (se il frontend o il
  lock cambiassero, il test cade invece di restare verde su numeri obsoleti);
- **assenza di lookahead** nel TCN: perturbare il frame T cambia l'output a T e
  lascia bit-identici tutti i precedenti;
- determinismo a parità di seed;
- percentili che non inventano valori non osservati;
- warm-up separato e load registrato;
- coda bounded, drop contati, nessun blocco;
- NaN/Inf **respinti, non clampati**;
- risultati fuori ordine respinti;
- worker morto: 100 frame consecutivi in neutral, zero applicazioni;
- worker lento: backlog fermo a capienza, 996 drop contati su 1000;
- 8 produttori concorrenti non superano mai la capienza;
- assenza dal dispatcher REV7.

Mutation test sulle tre garanzie di isolamento: coda che cresce → 3 fallimenti,
NaN clampato → 1, staleness ignorata → 1.

## 6. Classificazione

```text
baseline_min    FEASIBLE (lab)
tcn_small       FEASIBLE (lab)
tcn_worst       FEASIBLE (lab)  <- worst-case TCN verde fino a 16 istanze
spectro_worst   FEASIBLE_WITH_REDUCED_ENVELOPE (lab)
                rosso da 4 istanze; a 1 istanza gia' al 38% dell'hop

GO di prodotto  INCONCLUSIVE
```

Il GO di prodotto **non può** essere altro: nessun target hardware normativo
esiste, quindi le misure descrivono una macchina, non il requisito.

## 7. Non dimostrato

- qualità audio, e nulla qui la riguarda;
- comportamento in Ableton o in qualunque host: **mai eseguito**;
- RT-safety C++/JUCE — questo è un *control-flow prototype; C++/JUCE RT proof
  still pending*. Python alloca, ha un GIL e non usa lo scheduler dell'host;
- prestazioni su hardware diverso dall'M1 Pro misurato;
- che un modello addestrato somigli a questi surrogati: hanno pesi casuali;
- comportamento oltre 16 istanze;
- che 250 iterazioni catturino la coda vera di una sessione lunga.

## 8. Prossima azione minima

Ridurre `spectro_worst` finché non regge 8 istanze, **oppure** decidere che il
bake-off G4 lo ammette solo in taglia ridotta — e in entrambi i casi scriverlo
nel contratto G4, che è dove il budget compute deve vivere.

Il TCN causale worst-case non richiede azione: regge con margine anche
sovradimensionato.
