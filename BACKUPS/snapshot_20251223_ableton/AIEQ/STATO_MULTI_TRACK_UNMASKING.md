# STATO MULTI-TRACK UNMASKING

## ❌ NON È SEMPRE ATTIVO

### Stato Default:
```cpp
bool enableMultiTrackUnmasking = false;  // ← DISABILITATO di default
```

### Controlli di Attivazione:

1. **Flag in AIEngine**: `enableMultiTrackUnmasking` deve essere `true`
2. **Oggetto inizializzato**: `multiTrackUnmasking` deve esistere (sempre inizializzato)
3. **Flag interno**: `MultiTrackUnmasking::isEnabled` (default: `true`)

### Codice di Controllo:
```cpp
void AIEngine::updateMultiTrackAnalysis(...)
{
    if (!enableMultiTrackUnmasking || !multiTrackUnmasking)  // ← Controllo flag
        return;  // ← Esce se disabilitato
    
    // ... esegue analisi solo se abilitato
}
```

---

## 🔧 COME ABILITARLO

### Metodo 1: Abilitazione Manuale
```cpp
// Nel PluginProcessor o dove serve
aiEngine.enableMultiTrackUnmasking = true;

// Poi registra le tracce
aiEngine.multiTrackUnmasking->registerTrack("track1", "Kick");
aiEngine.multiTrackUnmasking->registerTrack("track2", "Bass");
```

### Metodo 2: Abilitazione Automatica (se vuoi)
```cpp
// Nel costruttore AIEngine o in prepare()
// Se vuoi che sia sempre attivo, cambia default:
bool enableMultiTrackUnmasking = true;  // ← Cambia default
```

### Metodo 3: Abilitazione Condizionale
```cpp
// Abilita solo in contesto multi-traccia
void setupForMultiTrackMode(bool isMultiTrack)
{
    aiEngine.enableMultiTrackUnmasking = isMultiTrack;
    
    if (isMultiTrack)
    {
        // Registra tracce
        aiEngine.multiTrackUnmasking->registerTrack("kick", "Kick");
        // ...
    }
}
```

---

## ⚡ IMPATTO PERFORMANCE

### Quando DISABILITATO (default):
- ✅ **Zero overhead**: Nessuna analisi eseguita
- ✅ **Nessun uso memoria**: Oggetto esiste ma non fa nulla
- ✅ **Nessun calcolo**: Controllo flag → return immediato

### Quando ABILITATO:
- ⚠️ **Overhead minimo**: Solo se tracce registrate
- ⚠️ **Memoria**: ~1KB per traccia registrata
- ⚠️ **CPU**: Analisi O(n²) per n tracce (solo quando chiamato)

---

## 💡 RACCOMANDAZIONI

### Abilita quando:
- ✅ Stai mixando più tracce simultaneamente
- ✅ Hai problemi di mascheramento frequenziale
- ✅ Vuoi correzioni automatiche cross-track

### Lascia disabilitato quando:
- ✅ Processing single-track
- ✅ Non hai problemi di mascheramento
- ✅ Vuoi massima performance

---

## 🔄 COMPORTAMENTO ATTUALE

**Default**: ❌ **DISABILITATO**

**Motivo**: 
- Non tutti gli utenti hanno contesto multi-traccia
- Evita overhead inutile in uso single-track
- Utente deve abilitare esplicitamente quando serve

**Vantaggi**:
- Zero overhead di default
- Controllo esplicito da parte dell'utente
- Performance ottimale per uso standard

---

## 🛠️ SE VUOI CAMBIARE IL DEFAULT

Se vuoi che sia **sempre attivo** di default, modifica:

**File**: `Source/AI/AIEngine.h` (linea ~434)

```cpp
// PRIMA:
bool enableMultiTrackUnmasking = false;

// DOPO:
bool enableMultiTrackUnmasking = true;  // ← Sempre attivo
```

**Nota**: Questo aumenterà leggermente l'overhead anche in uso single-track, ma sarà sempre disponibile.

