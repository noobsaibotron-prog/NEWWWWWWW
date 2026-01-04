# 📄 COME LEGGERE I LOG DEBUG

**Versione Debug:** `C:\AIEQ\AI Equalizer Pro.exe` (con file logging)  
**File Log:** `C:\AIEQ\linear_phase_debug.txt`

---

## 🎯 PROCEDURA TEST

### 1. CANCELLA log vecchio (opzionale)
```powershell
Remove-Item C:\AIEQ\linear_phase_debug.txt -ErrorAction SilentlyContinue
```

### 2. AVVIA Standalone
```powershell
cd C:\AIEQ
.\AI` Equalizer` Pro.exe
```

### 3. RIPRODUCI il Bug
1. ✅ Setup audio I/O
2. ✅ Avvia playback
3. ✅ Passa a **"Linear Phase"** mode
4. ✅ Clicca **"Fix All"**
5. ✅ Aspetta ~5 secondi (audio muto)
6. ✅ Chiudi il plugin

### 4. LEGGI il Log
```powershell
Get-Content C:\AIEQ\linear_phase_debug.txt
```

O apri con Notepad:
```powershell
notepad C:\AIEQ\linear_phase_debug.txt
```

---

## 📊 COSA CERCARE NEL LOG

### Log Attesi (Normale):
```
=== NEW SESSION 29/12/2025 01:45:00 ===
LP Debug: irReady=0 currentIR=0 doCrossfade=0
LP Debug: irReady=0 currentIR=0 doCrossfade=0
LP Debug: irReady=1 currentIR=0 doCrossfade=0
LP Process: lp=0x12345678 currentIR=0
```

### Log Bug (Silenzio):
```
=== NEW SESSION 29/12/2025 01:45:00 ===
LP Debug: irReady=1 currentIR=0 doCrossfade=0
LP Process: lp=0x0 currentIR=0
LP Fallback: processor nullptr, using zero-latency EQ
```

O:
```
LP Debug: irReady=0 currentIR=0 doCrossfade=0
(ripete all'infinito, IR non si carica mai)
```

---

## 🔍 INTERPRETAZIONE

### Scenario A: `irReady=0` sempre
- **Causa:** IR builder thread non funziona
- **Fix:** Verifica thread startup

### Scenario B: `irReady=1` ma `lp=0x0`
- **Causa:** Processor non inizializzato correttamente
- **Fix:** Verifica prepareToPlay LinearPhaseProcessor

### Scenario C: Nessun log "LP Debug"
- **Causa:** Non entra mai nel blocco Linear Phase
- **Fix:** Verifica cambio mode parameter

---

## 📝 AFTER TEST

**Mandami il contenuto di:**
```
C:\AIEQ\linear_phase_debug.txt
```

---

**TESTA ORA:** 
1. Avvia `C:\AIEQ\AI Equalizer Pro.exe`
2. Riproduci il bug
3. Leggi `C:\AIEQ\linear_phase_debug.txt`
4. Mandami il contenuto! 📋

