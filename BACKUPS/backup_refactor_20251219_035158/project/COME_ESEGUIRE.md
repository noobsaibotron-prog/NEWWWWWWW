# 🚀 COME ESEGUIRE IL BUILD - Guida Passo-Passo

## Metodo 1: Usando il Prompt dei Comandi (CMD)

### Passo 1: Apri il Prompt dei Comandi
- Premi `Windows + R`
- Digita `cmd` e premi Invio
- Oppure cerca "Prompt dei comandi" nel menu Start

### Passo 2: Naviga nella directory del progetto
Copia e incolla questo comando:

```batch
cd /d C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
```

### Passo 3: Verifica di essere nella directory corretta
```batch
dir
```

Dovresti vedere i file: `BUILD_FINAL.bat`, `CMakeLists.txt`, `Source`, `JUCE`, ecc.

### Passo 4: Esegui il build
```batch
BUILD_FINAL.bat
```

---

## Metodo 2: Usando PowerShell

### Passo 1: Apri PowerShell
- Premi `Windows + X`
- Seleziona "Windows PowerShell" o "Terminale"
- Oppure cerca "PowerShell" nel menu Start

### Passo 2: Naviga nella directory del progetto
```powershell
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
```

### Passo 3: Verifica di essere nella directory corretta
```powershell
Get-ChildItem
```

### Passo 4: Esegui il build
```powershell
.\BUILD_FINAL.bat
```

---

## Metodo 3: Usando Esplora File (Il Più Semplice!)

### Passo 1: Apri Esplora File
- Premi `Windows + E`
- Naviga a: `C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2`

### Passo 2: Apri il Prompt dalla directory
- Nella barra degli indirizzi di Esplora File, clicca e digita: `cmd`
- Premi Invio
- Il Prompt si aprirà già nella directory corretta!

### Passo 3: Esegui il build
```batch
BUILD_FINAL.bat
```

---

## Metodo 4: Doppio Click (Windows Explorer)

### Passo 1: Apri Esplora File
Naviga a: `C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2`

### Passo 2: Doppio click su BUILD_FINAL.bat
- Trova il file `BUILD_FINAL.bat`
- Fai doppio click
- Si aprirà una finestra che esegue il build automaticamente

---

## ⚡ Comando Rapido (Copia e Incolla)

### Per CMD:
```batch
cd /d C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2 && BUILD_FINAL.bat
```

### Per PowerShell:
```powershell
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2; .\BUILD_FINAL.bat
```

---

## ✅ Verifica che Funzioni

Dopo aver eseguito `BUILD_FINAL.bat`, dovresti vedere:

```
===========================================
  AI EQUALIZER PRO V2 - Build Script (improved)
===========================================

[INFO] Found JUCE in project directory
[INFO] Using JUCE from: C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2\JUCE
...
```

Se vedi questo, tutto sta funzionando correttamente! 🎉

---

## 🆘 Problemi Comuni

### "Il comando non è riconosciuto"
**Causa**: Non sei nella directory corretta
**Soluzione**: Esegui prima `cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2`

### "File non trovato"
**Causa**: Il percorso potrebbe essere diverso
**Soluzione**: Verifica il percorso con `dir` o `Get-ChildItem`

### "Accesso negato"
**Causa**: Potresti aver bisogno dei permessi di amministratore
**Soluzione**: Apri CMD/PowerShell come Amministratore (tasto destro → "Esegui come amministratore")

---

## 📝 Note Importanti

1. **Non serve specificare il percorso di JUCE** - Lo script lo trova automaticamente!
2. **Il build può richiedere alcuni minuti** - Sii paziente
3. **Assicurati di avere Visual Studio installato** con il workload C++
4. **CMake verrà trovato automaticamente** se installato o incluso in Visual Studio

---

## 🎯 Il Metodo Più Veloce

1. Apri Esplora File (`Windows + E`)
2. Vai a: `C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2`
3. Nella barra degli indirizzi, digita: `cmd` e premi Invio
4. Digita: `BUILD_FINAL.bat` e premi Invio
5. Aspetta che finisca! ⏳

