# AI Equalizer Pro V2.1

## Advanced AI-Powered Parametric Equalizer for VST3/AU

**Versione:** 2.1 (basata su analisi ChatGPT + implementazione Claude)

---

## 🆕 Nuove Features V2.1

### 🎛️ Source Profiles
Profili ottimizzati per diverse sorgenti audio:
- **Generic** - Rilevamento bilanciato standard
- **Vocals** - Alta sensibilità sibilanti, focus boxyness 300-400Hz
- **Drums** - Risonanze sub, meno sensibile a harshness (piatti ok)
- **Bass** - Focus su frequenze sub/basse
- **Synth** - Ampio range, focus risonanze
- **Master** - Soglie meno severe per mix completi
- **EDM** - Tollera più bassi e brillantezza

### 🔀 A/B Comparison
- Due slot di memoria per confrontare impostazioni EQ
- Pulsanti A/B per switch istantaneo
- Copia A→B per salvare stato

### 📊 Auto-Gain Intelligente
- Compensazione automatica del volume
- Mantiene livello RMS costante durante l'EQ
- Range limitato ±12 dB per sicurezza

### 📈 Spettro Pre/Post EQ
- Visualizza spettro prima dell'EQ (giallo)
- Overlay spettro dopo EQ (verde)
- Confronto visivo immediato dell'effetto

### 📍 Peak Hold
- Marcatori dei picchi sullo spettro
- Decay configurabile (2 secondi default)
- Identifica facilmente frequenze problematiche

### 🚀 Ottimizzazioni Performance
- **Bypass AI in Export** - AI disabilitata durante render offline
- **Thread Safety** - Mutex per dati condivisi tra audio/GUI
- **Analysis History** - Ultimi 5 snapshot per confronto

---

## Architettura

```
AIEqualizer_V2/
├── Source/
│   ├── PluginProcessor.cpp/h    # Core processor con A/B, Auto-Gain
│   ├── PluginEditor.cpp/h       # Editor principale
│   ├── AI/
│   │   ├── AIEngine.cpp/h       # Motore AI con Source Profiles
│   │   ├── ReferenceMatcher.h   # Reference track matching
│   │   └── UserLearning.h       # Sistema apprendimento utente
│   ├── DSP/
│   │   ├── EQProcessor.cpp/h    # 8-band parametric EQ
│   │   └── SpectrumAnalyzer.cpp/h # FFT con Peak Hold
│   └── GUI/
│       ├── AIControlPanel.h     # Pannello controlli AI
│       ├── AdvancedSpectrumDisplay.h # Spettro Pre/Post
│       ├── InteractiveEQDisplay.h    # Curva EQ interattiva
│       └── ModernLookAndFeel.h  # Stile FabFilter-inspired
├── CMakeLists.txt
└── BUILD_FINAL.bat
```

---

## Compilazione

### Requisiti
- Visual Studio 2022/2025/2026 Community
- JUCE 7.x o 8.x (path in CMakeLists.txt)
- CMake 3.22+

### Build (Windows)

```batch
# Opzione 1: Usa batch file
BUILD_FINAL.bat

# Opzione 2: Manuale
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Output
```
build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3
```

### Installazione
Copia `AI Equalizer Pro.vst3` in:
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `/Library/Audio/Plug-Ins/VST3/`

---

## Problemi Rilevati dall'AI

| Tipo | Range Frequenze | Descrizione |
|------|-----------------|-------------|
| Resonance | 20-20000 Hz | Picchi stretti pronunciati |
| Harshness | 2-6 kHz | Eccesso alte frequenze medio-alte |
| Muddiness | 150-400 Hz | Accumulo basse-medie |
| Boxyness | 300-800 Hz | Suono "scatolato" |
| Sibilance | 5-10 kHz | Eccesso sibilanti |
| Low-End Boom | 30-100 Hz | Accumulo sub/bassi |
| Thin Sound | Lack of 200-600 Hz | Mancanza corpo |
| Dull Sound | Lack of 8-16 kHz | Mancanza brillantezza |

---

## Parametri AI

- **Sensitivity (0-100%)**: Più alto = rileva anche problemi minori
- **Strength (0-100%)**: Intensità correzioni applicate
- **Source Profile**: Adatta soglie al tipo di sorgente

---

## Changelog

### V2.1 (Nov 2025)
- ✅ Source Profiles (Generic, Vocals, Drums, Bass, Synth, Master, EDM)
- ✅ A/B Comparison con memoria
- ✅ Auto-Gain compensazione RMS
- ✅ Spettro Pre/Post EQ overlay
- ✅ Peak Hold su analyzer
- ✅ Bypass AI durante export offline
- ✅ Analysis History (ultimi 5 snapshot)
- ✅ Severity indicators colorati (verde/giallo/rosso)
- ✅ Thread safety migliorata con mutex

### V2.0
- EQ parametrico 8 bande
- Analisi AI real-time
- Reference track matching
- User learning system
- GUI FabFilter-inspired

---

## Licenza

Progetto educativo/personale. JUCE richiede licenza per uso commerciale.

---

## Credits

Sviluppato con assistenza AI (Claude + ChatGPT) per Marco - Sound Designer & Music Producer.

Generi ottimizzati: Industrial, Techno, Jungle, Breakbeat, Dub Techno, Deep House.
