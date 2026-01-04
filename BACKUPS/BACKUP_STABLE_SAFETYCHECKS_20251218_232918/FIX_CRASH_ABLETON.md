# Fix Crash Ableton - Safety Checks

## Problema
Il plugin crashava in Ableton quando veniva aperto nella DAW. La causa era che `process()` poteva essere chiamato prima di `prepare()`, o con `currentSampleRate` non ancora inizializzato (0.0), causando:
- Divisione per zero in calcoli di frequenza
- Accesso a coefficienti non inizializzati
- Preparazione di filtri con sample rate invalido

## Fix Implementati

### 1. ParametricEQProcessor::process()
**File**: `Source/DSP/ParametricEQProcessor.cpp`

Aggiunto check di sicurezza all'inizio di `process()`:
```cpp
// CRITICAL: Safety check - if not prepared, just pass through with gain
if (sr <= 0.0 || numSamples <= 0 || bufferChannels <= 0)
{
    const float gain = outputGain.load(std::memory_order_relaxed);
    if (std::abs(gain - 1.0f) > 0.0001f)
        buffer.applyGain(gain);
    return;
}
```

### 2. ParametricEQProcessor::makeCoefficients()
**File**: `Source/DSP/ParametricEQProcessor.cpp`

Aggiunto check per sample rate invalido:
```cpp
// CRITICAL: Safety check - return nullptr if sample rate is invalid
if (sampleRate <= 0.0 || sampleRate > 192000.0)
    return nullptr;
```

### 3. ParametricEQProcessor::addBand()
**File**: `Source/DSP/ParametricEQProcessor.cpp`

Aggiunto check prima di preparare i filtri:
```cpp
// CRITICAL: Safety check - only prepare if sample rate is valid
if (sr > 0.0 && blockSize > 0)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = 1;
    
    band.filterL.prepare(spec);
    band.filterR.prepare(spec);
}
```

### 4. DynamicEQProcessor::process()
**File**: `Source/DSP/DynamicEQProcessor.cpp`

Aggiunto check per sample rate invalido:
```cpp
// CRITICAL: Safety check - if not prepared, just pass through
const double sr = currentSampleRate.load(std::memory_order_relaxed);
if (numSamples == 0 || channels == 0 || sr <= 0.0)
    return;
```

## Comportamento
- Se `process()` viene chiamato prima di `prepare()`, il buffer viene passato attraverso con solo l'output gain applicato (se diverso da 1.0)
- Se `makeCoefficients()` riceve un sample rate invalido, ritorna `nullptr` (già gestito dal check esistente `if (coeffs == nullptr) continue;`)
- Se `addBand()` viene chiamato prima di `prepare()`, i filtri non vengono preparati (verranno preparati quando `prepare()` viene chiamato)

## Test
- ✅ Compilazione Release riuscita
- ⏳ Test in Ableton necessario per verificare che il crash sia risolto

## Note
Questi fix garantiscono che il plugin sia **safe** anche quando viene chiamato in condizioni non ideali (prima di `prepare()`, con sample rate non inizializzato, etc.). Il comportamento è **graceful degradation**: invece di crashare, il plugin passa il segnale attraverso senza processarlo.

