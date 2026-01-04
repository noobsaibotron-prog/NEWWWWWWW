# TensorFlow Lite Setup (Windows x64) – Optional NN Runtime

Questa guida spiega come abilitare l'inferenza NN reale via TFLite. Senza questi passi, il plugin usa il fallback MLEngine (nessun crash).

## Cosa serve
- Header TFLite (cartella `tensorflow/lite/**/*.h`)
- Librerie `tensorflowlite_c.lib` e `tensorflowlite_c.dll` (o anche `tensorflowlite.lib/.dll` se disponibili)
- Definizione del flag `AIEQ_ENABLE_TFLITE`
- Modello `.tflite`: `models/problem_detection.tflite`

## Opzione 1: Prebuilt tflite_c con include
1. Scarica una release tflite_c per Windows x64 che contenga **anche** `include/`.
   - Repo community: https://github.com/tphakala/tflite_c/releases (scegli pacchetto con headers).
2. Copia i file:
   - `include\*` → `C:\AIEQ\ThirdParty\TFLite\include\`
   - `tensorflowlite_c.lib` (+ eventuale `tensorflowlite.lib`) e `.dll` → `C:\AIEQ\ThirdParty\TFLite\lib\`
3. Build settings (Projucer/CMake):
   - Preprocessor: `AIEQ_ENABLE_TFLITE`
   - Header Search Paths: `$(PROJECT_DIR)/ThirdParty/TFLite/include`
   - Library Search Paths: `$(PROJECT_DIR)/ThirdParty/TFLite/lib`
   - Extra Libraries: `tensorflowlite_c.lib` (e/o `tensorflowlite.lib` se presente)
4. Runtime: copia `tensorflowlite_c.dll` accanto al `.vst3` (e all’exe standalone se serve).

## Opzione 2: Header dal sorgente TensorFlow Lite
Se il pacchetto che hai scaricato contiene solo le DLL:
1. Clona (o scarica zip) da https://github.com/tensorflow/tensorflow
2. Copia la cartella `tensorflow/lite/` (tutti gli `.h`) in `C:\AIEQ\ThirdParty\TFLite\include\` mantenendo la struttura.
   - Esempio: `ThirdParty/TFLite/include/tensorflow/lite/interpreter.h`, ecc.
3. Usa le `.lib/.dll` che hai (es. `tensorflowlite_c.lib/.dll`) in `ThirdParty/TFLite/lib/`.
4. Configura come al punto “Build settings” sopra.

## Modello
- Metti `models/problem_detection.tflite` accanto al `.vst3` (cartella `models/` già presente).
- Se manca il modello o il runtime, il plugin logga un warning e resta sul fallback ML.

## Modello di test rapido (Python)
Esegui da `C:\AIEQ` (richiede `pip install tensorflow`):
```python
import tensorflow as tf, os
num_bins = 2048
num_problems = 8
model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(num_bins,)),
    tf.keras.layers.Dense(128, activation='relu'),
    tf.keras.layers.Dense(64, activation='relu'),
    tf.keras.layers.Dense(num_problems, activation='sigmoid'),
])
tflite_model = tf.lite.TFLiteConverter.from_keras_model(model).convert()
os.makedirs("models", exist_ok=True)
with open("models/problem_detection.tflite", "wb") as f:
    f.write(tflite_model)
print("Modello creato in models/problem_detection.tflite")
```
Copiala (cartella `models/`) accanto al `.vst3` prima del test.

## Verifica
- Build Release, copia `.vst3` + `tensorflowlite_c.dll` in `C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\` (o stessa cartella dell’exe).
- Log: “TFLite model loaded …” → NN attiva. Altrimenti warning e fallback ML.

