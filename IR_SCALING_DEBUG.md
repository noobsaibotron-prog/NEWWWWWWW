# 🔬 IR SCALING DEBUG - v3.2.0

**Build:** `C:\AIEQ\AI Equalizer Pro IR-SCALE.exe`  
**Data:** 2025-12-29 03:50

---

## 🎯 PROBLEMA IDENTIFICATO

**Il fix avgMag compensation NON ha funzionato:**

```
avgMag = 0.91  → compensation = 1.1x
IR: 3e-08 * 1.1 = 3.3e-08  ← ANCORA TROPPO PICCOLO!
```

**3e-08 è 7 ORDINI DI GRANDEZZA troppo piccolo!**

---

## 🔴 NUOVA IPOTESI

**Forse JUCE Convolution ha un threshold interno:**
- Se `maxAbs < 1e-06`, forse scarta/tronca l'IR?
- Oppure c'è un bug nel loader?

---

## ✅ FIX APPLICATO: IR SCALING AGGRESSIVO

**Se `maxAbs < 1e-04`, scala l'IR a target 0.1:**

```cpp
constexpr float minReasonableIR = 1e-04f;  // Threshold
constexpr float targetIR = 0.1f;           // Target per scaling
const float irScale = (maxAbs > 0.0f && maxAbs < minReasonableIR) ? (targetIR / maxAbs) : 1.0f;

// Scala IR prima di caricare
for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
    scaledIR[n] = irBuf[n] * irScale;

back->loadImpulseResponse(scaledIR, sr);
```

**Effetto:**
- IR con maxAbs=3e-08 → Scala 3333x → maxAbs=0.1
- IR con maxAbs=0.5 → Nessun scaling
- IR con maxAbs=1.0 → Nessun scaling

---

## 🧪 TEST

```powershell
cd C:\AIEQ
Remove-Item linear_phase_debug.txt
.\AI` Equalizer` Pro` IR-SCALE.exe
```

**Scenario:**
1. Passa a "Linear Phase"
2. Clicca "Fix All" (oppure muovi qualsiasi banda)
3. **VERIFICA AUDIO!**

**Check log:**
```powershell
notepad C:\AIEQ\linear_phase_debug.txt | Select-String "SCALING"
```

**Dovrebbe vedere:**
```
SCALING IR: 3333x (maxAbs too small: 3e-08)
IR BEFORE LOAD: maxAbs=0.1 [dopo scaling]
```

---

## 🎯 SE QUESTO FUNZIONA

**Allora il problema è confermato:** JUCE Convolution (o il nostro IR builder) ha problemi con IR molto piccoli!

**Causa root:** Non è un problema di normalization flags, ma di **scala assoluta dell'IR!**

---

## 🚀 TEST IMMEDIATO!

**Lancia:** `C:\AIEQ\AI Equalizer Pro IR-SCALE.exe`

**QUESTO DOVREBBE FINALMENTE RISOLVERE!** 🎯

