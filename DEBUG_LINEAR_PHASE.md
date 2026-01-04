# 🔍 DEBUG LINEAR PHASE SILENCE - ISTRUZIONI

**Versione Debug:** `C:\AIEQ\AI Equalizer Pro.exe` (01:35+)  
**Con logging attivo**

---

## 🧪 PROCEDURA TEST CON DEBUG

### 1. Avvia Standalone con Console di Debug

```powershell
cd C:\AIEQ
.\AI` Equalizer` Pro.exe
```

**IMPORTANTE:** Tieni aperto il terminale per vedere i log!

### 2. Setup Audio
- Configura Input/Output
- Avvia playback

### 3. Scenario Test
1. ✅ Verifica audio funziona in "Zero Latency"
2. ✅ Passa a **"Linear Phase"**
3. ✅ Clicca **"Fix All"** (applica correzioni AI)
4. ✅ **Osserva i log nel terminale!**

### 4. Log Attesi

Se funziona:
```
LP Debug: irReady=0 currentIR=0 doCrossfade=0
LP Debug: irReady=1 currentIR=0 doCrossfade=0
LP Process: lp=0x... currentIR=0
```

Se bug:
```
LP Debug: irReady=1 currentIR=0 doCrossfade=0
LP Process: lp=0x0 currentIR=0
LP Fallback: processor nullptr, using zero-latency EQ
```

---

## 📊 COSA CERCARE

### Caso 1: irReady = FALSE
- ✅ Dovrebbe usare fallback zero-latency (righe 1214-1220)
- Audio dovrebbe esserci

### Caso 2: irReady = TRUE ma lp = nullptr
- ✅ Dovrebbe usare fallback zero-latency (righe 1286-1294)
- Audio dovrebbe esserci

### Caso 3: irReady = TRUE e lp != nullptr
- ✅ Usa Linear Phase processor (riga 1281)
- Audio dovrebbe esserci MA potrebbe essere problema nell'IR

---

## 🔍 ANALISI POSSIBILI CAUSE

### Ipotesi A: IR Builder Non Parte
- `irBuilderThread` non si avvia
- `triggerEQCurveUpdate()` non funziona
- `irBuildEvent` non viene segnalato

### Ipotesi B: IR Vuoto/Corrotto
- IR viene generato ma è silenzioso (coefficienti zero)
- `loadImpulseResponse()` fallisce silenziosamente
- Convoluzione con IR vuoto = silenzio

### Ipotesi C: Parametri EQ Tutti Zero
- Quando applica "Fix All", potrebbe azzerare tutti i gain
- IR generato con EQ flat = audio passa ma senza processing

---

## 🛠️ WORKAROUND TEMPORANEO

Se il bug persiste:

```cpp
// Opzione nucleare: Disabilita completamente Linear Phase
if (mode == PhaseMode::LinearPhase)
{
    // Force fallback to zero-latency
    eqProcessor.process(buffer);
    if (dynEqEnabledLocal) {
        dynamicEQProcessor.process(buffer);
    }
}
// Skip tutto il resto del blocco Linear Phase
```

Questo garantisce audio sempre presente.

---

## 📝 REPORT LOG RICHIESTO

Dopo il test, dimmi:
1. Cosa dicono i log `LP Debug:`?
2. Cosa dice `LP Process:` (lp pointer)?
3. Vedi `LP Fallback:` quando silenzio?

Con queste info posso identificare la causa esatta!

---

**TESTA ORA: `C:\AIEQ\AI Equalizer Pro.exe` (con debug)**  
**Mandami i log!** 🔍

