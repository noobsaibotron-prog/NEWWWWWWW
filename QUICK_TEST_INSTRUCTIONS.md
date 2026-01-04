# 🚀 QUICK TEST - LINEAR PHASE DEBUG

**Build Pronto:** `C:\AIEQ\AI Equalizer Pro.exe` (v2.1.5)  
**Log Pulito:** ✅ Cancellato

---

## 🎯 TEST RAPIDO

### 1. Avvia
```powershell
cd C:\AIEQ
.\AI` Equalizer` Pro.exe
```

### 2. Riproduci Bug
- Setup audio I/O
- Passa a **"Linear Phase"**
- Clicca **"Fix All"**
- Aspetta 5 secondi
- Chiudi plugin

### 3. Leggi Log
```powershell
notepad C:\AIEQ\linear_phase_debug.txt
```

---

## 📊 CERCA NEL LOG

### A. EQ Magnitude (Prime righe dopo IR Generated)
```
  bin 0 (0 Hz): mag=??? dB=???
  bin 1 (24 Hz): mag=??? dB=???
  bin 2 (48 Hz): mag=??? dB=???
  bin 3 (72 Hz): mag=??? dB=???
  bin 4 (96 Hz): mag=??? dB=???
EQ Magnitude: numBands=???
```

### B. IR Magnitude
```
IR Generated: maxAbs=??? rms=???
```

---

## 🎯 POSSIBILI RISULTATI

### Caso 1: mag ≈ 1.0 (es. 0.99-1.01)
- **= EQ flat, nessun processing**
- **Causa:** Shadow processor non aggiornato o enabled=false
- **Fix:** Force update shadow processor

### Caso 2: mag ≈ 0.0000001
- **= EQ coefficienti zero/corrotti**
- **Causa:** getMagnitudeForFrequency() bug
- **Fix:** Debug coefficienti

### Caso 3: mag sembra OK (es. 0.5-2.0) ma maxAbs ancora ~1e-08
- **= Problema nella IFFT o windowing**
- **Causa:** Bug nell'IR generation
- **Fix:** Debug IFFT

---

**TESTA ORA E MANDAMI I VALORI!** 🔬

**CERCA:** 
- `mag=` (primi 5 bin)
- `numBands=`
- `maxAbs=`

