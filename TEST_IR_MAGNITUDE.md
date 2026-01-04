# 🔬 TEST IR MAGNITUDE - DIAGNOSTICA FINALE

**Build:** v2.1.3 (IR Magnitude Debug)  
**Location:** `C:\AIEQ\AI Equalizer Pro.exe`

---

## 🧪 PROCEDURA TEST

### 1. Prepara Test
```powershell
cd C:\AIEQ
Remove-Item linear_phase_debug.txt -ErrorAction SilentlyContinue
```

### 2. Avvia e Riproduci Bug
```powershell
.\AI` Equalizer` Pro.exe
```

- Setup audio
- Passa a "Linear Phase"
- Clicca "Fix All"
- Aspetta 10 secondi
- Chiudi plugin

### 3. Leggi Log
```powershell
notepad C:\AIEQ\linear_phase_debug.txt
```

---

## 📊 COSA CERCARE

### ✅ IR Normale (audio OK)
```
IR Generated: maxAbs=0.5 rms=0.05 backIdx=0
IR Generated: maxAbs=0.8 rms=0.08 backIdx=1
```
**maxAbs dovrebbe essere 0.1-2.0**  
**rms dovrebbe essere 0.01-0.5**

### 🔴 IR Silenzioso (BUG!)
```
IR Generated: maxAbs=0.0001 rms=0.00001 backIdx=0
IR Generated: maxAbs=0 rms=0 backIdx=1
```
**maxAbs vicino a 0 = IR praticamente zero = SILENZIO!**

### 🔴 IR Troppo Forte (Possibile distorsione)
```
IR Generated: maxAbs=10.0 rms=5.0 backIdx=0
```
**maxAbs > 5.0 = Clipping possibile**

---

## 🎯 DIAGNOSI PREVISTA

**Se maxAbs < 0.001:** 
- Problema: IR troppo attenuato
- Causa probabile: Double windowing o gain compensation errata
- Fix: Rimuovere/aggiustare hannGainComp

**Se maxAbs = 0:**
- Problema: IR completamente zero
- Causa: EQ magnitude response è flat (gain=0 ovunque)
- Fix: Verificare eqProcessorForIR coefficienti

**Se maxAbs normale ma audio muto:**
- Problema: Convolution non funziona
- Causa: Bug in JUCE Convolution o prepare() non chiamato
- Fix: Reset convolution o richiama prepare()

---

## 📝 REPORT RICHIESTO

Dopo il test, mandami:
1. Le righe `IR Generated:` dal log
2. Conferma se audio è ancora muto

Con questi dati identifico la causa esatta!

---

**TEST BUILD: `C:\AIEQ\AI Equalizer Pro.exe`**  
**LOG: `C:\AIEQ\linear_phase_debug.txt`**

**TESTA E MANDAMI I VALORI DI `maxAbs` e `rms`!** 🔬

