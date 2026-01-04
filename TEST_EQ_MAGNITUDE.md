# 🔬 TEST EQ MAGNITUDE - DIAGNOSTICA AVANZATA

**Build:** v2.1.5 (EQ Magnitude Debug)  
**Location:** `C:\AIEQ\AI Equalizer Pro.exe`

---

## 🎯 NUOVO TEST

```powershell
cd C:\AIEQ
Remove-Item linear_phase_debug.txt
.\AI` Equalizer` Pro.exe
```

### Scenario Test:
1. ✅ Passa a "Linear Phase"
2. ✅ Clicca "Fix All"
3. ✅ Aspetta 5 secondi (silenzio)
4. ✅ Chiudi plugin

### Leggi Log:
```powershell
notepad C:\AIEQ\linear_phase_debug.txt
```

---

## 📊 CERCA NEL LOG

### 1. IR Magnitude
```
IR Generated: maxAbs=??? rms=???
```

### 2. EQ Magnitude Response (NUOVO!)
```
  bin 0 (0 Hz): mag=??? dB=???
  bin 1 (24 Hz): mag=??? dB=???
  bin 2 (48 Hz): mag=??? dB=???
EQ Magnitude: numBands=???
```

**Se mag ≈ 1.0 per tutti i bin** → EQ è flat → IR sarà quasi delta

**Se mag ≈ 0.0000001** → EQ coefficienti sono zero/corrotti → IR silenzioso

---

## 📝 MANDAMI

1. I valori `mag=` per i primi 5 bin
2. Il valore `numBands=`
3. Conferma se audio è ancora muto

---

**TEST: `C:\AIEQ\AI Equalizer Pro.exe`**

**MANDAMI I VALORI DI `mag=` E `numBands=`!** 🔬

