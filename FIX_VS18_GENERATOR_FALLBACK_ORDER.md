# ✅ FIX: VS 18 Generator Fallback - Order of Checks

**Data**: 2025-12-07

## Problema Verificato

### Bug Identificato

**Problema**: L'ordine dei controlli per il fallback del generatore Visual Studio era errato:

1. ❌ **Check 1 (linea 129 originale)**: Verifica se il generatore è Visual Studio ma non c'è installazione trovata
   - Se TRUE → Prova fallback a Ninja/MinGW
   - Se fallisce → FATAL_ERROR

2. ❌ **Check 2 (linea 150 originale)**: Verifica se il generatore è "Visual Studio 18" 
   - Se TRUE → Converte a "Visual Studio 17 2022"

**Scenario del Bug:**
- Utente esegue: `cmake -G "Visual Studio 18 2026" ..`
- Sistema ha: SOLO VS 18 installato (VS 2022 NON installato)
- Risultato:
  1. `VS_INSTALL_FOUND` = FALSE (perché cerca solo VS 2022 in `_VS_HINT_DIRS`)
  2. Check 1 si attiva: generatore è "Visual Studio" ma installazione non trovata
  3. Prova fallback a Ninja/MinGW
  4. Se Ninja/MinGW non disponibile → FATAL_ERROR
  5. Check 2 **NON viene mai eseguito** perché il controllo è già fallito

### Soluzione Applicata

**Riordinato i controlli in `CMakeLists.txt` (linee 129-172):**

#### Nuovo Ordine (CORRETTO):

1. ✅ **PRIMA**: Check per "Visual Studio 18" e conversione a "Visual Studio 17 2022" (linee 134-152)
   - Se generatore è "Visual Studio 18" → Converti a "Visual Studio 17 2022"
   - Re-check se installazione VS esiste (anche VS 18 può essere usata)

2. ✅ **DOPO**: Check se il generatore (ora possibilmente modificato) richiede installazione VS (linee 154-172)
   - Se generatore è Visual Studio ma installazione non trovata → Fallback a Ninja/MinGW

### Codice Modificato

```cmake
# IMPORTANT: Check for VS 18 generator BEFORE checking if VS installation is found.
# If user specifies "Visual Studio 18 2026" but VS 2022 is not installed, we need to
# convert to "Visual Studio 17 2022" first, then check if that installation exists.
# If we check for missing installation first, we'll try Ninja/MinGW fallback before
# the VS 18 -> VS 2022 conversion happens.
if(CMAKE_GENERATOR MATCHES "Visual Studio 18")
    message(WARNING "Generator ${CMAKE_GENERATOR} not registered. Falling back to Visual Studio 17 2022.")
    set(CMAKE_GENERATOR "Visual Studio 17 2022" CACHE INTERNAL "Forced generator fallback" FORCE)
    # After converting to VS 2022, re-check if VS installation exists (in case VS 18 was installed)
    # The _VS_HINT_DIRS already includes VS 2022 paths, so VS_INSTALL_FOUND should already be correct
    # But if VS 18 was the only installation, we need to check if it exists as fallback
    if(NOT VS_INSTALL_FOUND)
        # Check if VS 18 installation exists even though generator was converted
        if(EXISTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC")
            set(VS_INSTALL_FOUND TRUE)
            set(VS_INSTALL_DIR "C:/Program Files/Microsoft Visual Studio/18/Community")
            message(STATUS "Found VS 18 installation (generator converted to VS 2022 for compatibility)")
        elseif(EXISTS "C:/Program Files/Microsoft Visual Studio/2026/Community/VC/Tools/MSVC")
            set(VS_INSTALL_FOUND TRUE)
            set(VS_INSTALL_DIR "C:/Program Files/Microsoft Visual Studio/2026/Community")
            message(STATUS "Found VS 2026 installation (generator converted to VS 2022 for compatibility)")
        endif()
    endif()
endif()

# Now check if the (possibly adjusted) Visual Studio generator has an installation available
if(CMAKE_GENERATOR MATCHES "^Visual Studio" AND NOT VS_INSTALL_FOUND)
    # ... fallback a Ninja/MinGW ...
endif()
```

## Comportamento Finale

### Scenario 1: Utente specifica `-G "Visual Studio 18 2026"`, solo VS 18 installato

**PRIMA (Bug):**
```
1. VS_INSTALL_FOUND = FALSE (cerca solo VS 2022)
2. Check: "Visual Studio" + NOT FOUND → Prova Ninja/MinGW
3. Ninja/MinGW non disponibile → FATAL_ERROR
4. VS 18 conversion check mai eseguito ❌
```

**DOPO (Fix):**
```
1. VS_INSTALL_FOUND = FALSE (cerca solo VS 2022)
2. Check: "Visual Studio 18" → Converti a "Visual Studio 17 2022" ✅
3. Re-check: VS 18 installation trovata → VS_INSTALL_FOUND = TRUE ✅
4. Check: "Visual Studio" + FOUND → Continua normalmente ✅
```

### Scenario 2: Utente specifica `-G "Visual Studio 18 2026"`, VS 2022 installato

**PRIMA e DOPO:**
```
1. VS_INSTALL_FOUND = TRUE (VS 2022 trovato)
2. Check: "Visual Studio 18" → Converti a "Visual Studio 17 2022" ✅
3. Check: "Visual Studio" + FOUND → Continua normalmente ✅
```

### Scenario 3: Utente specifica `-G "Visual Studio 18 2026"`, nessun VS installato

**PRIMA e DOPO:**
```
1. VS_INSTALL_FOUND = FALSE
2. Check: "Visual Studio 18" → Converti a "Visual Studio 17 2022" ✅
3. Re-check: VS 18 non trovato → VS_INSTALL_FOUND rimane FALSE
4. Check: "Visual Studio" + NOT FOUND → Fallback a Ninja/MinGW ✅
5. Se Ninja/MinGW disponibile → Usa fallback
6. Se Ninja/MinGW non disponibile → FATAL_ERROR (comportamento corretto)
```

## Test

Per verificare il fix:

1. **Test con VS 18 installato, VS 2022 non installato:**
   ```bash
   cmake -G "Visual Studio 18 2026" ..
   # Atteso: Generatore convertito a VS 2022, VS 18 installation trovata, build continua
   ```

2. **Test con VS 2022 installato:**
   ```bash
   cmake -G "Visual Studio 18 2026" ..
   # Atteso: Generatore convertito a VS 2022, VS 2022 trovato, build continua
   ```

3. **Test senza VS installato:**
   ```bash
   cmake -G "Visual Studio 18 2026" ..
   # Atteso: Generatore convertito a VS 2022, fallback a Ninja/MinGW o errore appropriato
   ```

## Note

- Il fix garantisce che la conversione VS 18 → VS 2022 avvenga PRIMA del check per installazione mancante
- Se VS 18 è installato ma VS 2022 no, viene rilevata e usata l'installazione VS 18 (generatore convertito per compatibilità)
- Il fallback a Ninja/MinGW avviene solo se davvero non c'è alcuna installazione VS disponibile

---

**Fix applicato**: 2025-12-07  
**File modificato**: `CMakeLists.txt` (linee 129-172)  
**Bug risolto**: L'ordine dei controlli ora garantisce che VS 18 → VS 2022 conversion avvenga prima del check per installazione mancante
