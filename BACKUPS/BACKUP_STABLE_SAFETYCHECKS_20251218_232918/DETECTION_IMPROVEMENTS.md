# MIGLIORAMENTI AFFIDABILITÀ DETECTION - PROPOSTA

## PROBLEMI ATTUALI

1. **Soglie troppo basse** (0.5dB) → Troppi falsi positivi
2. **Mancanza validazione statistica** → Non usa outlier detection
3. **Mancanza consensus multi-frame** → Non richiede coerenza temporale
4. **Mancanza analisi armonica** → Non distingue picchi legittimi da problemi
5. **Mancanza validazione contestuale** → Non considera il contesto (es. 60Hz in kick è normale)
6. **Cross-validation troppo semplice** → Solo 3 metodi base

## PROPOSTE CONCRETE

### 1. STATISTICAL OUTLIER DETECTION (Z-Score)
- Calcolare Z-score per ogni picco rispetto alla distribuzione locale
- Solo picchi con Z-score > 2.5 sono considerati outlier (problemi reali)
- Elimina falsi positivi da variazioni normali

### 2. MULTI-FRAME CONSENSUS
- Richiedere che un problema appaia in almeno 3-5 frame consecutivi
- Pattern deve essere coerente (frequenza ±1%, magnitudine ±2dB)
- Elimina transienti e artefatti

### 3. HARMONIC ANALYSIS
- Verificare se un picco è armonico (f0, 2f0, 3f0...) o spurio
- Picchi armonici = legittimi (non problemi)
- Picchi spuri = problemi reali
- Usa FFT per trovare fondamentale

### 4. CONTEXTUAL VALIDATION
- Validare in base al SourceProfile:
  - Kick: picchi 40-80Hz sono normali
  - Vocals: picchi 200-400Hz possono essere formanti
  - Synth: picchi armonici sono normali
- Whitelist di frequenze "normali" per profilo

### 5. SPECTRAL COHERENCE ANALYSIS
- Analizzare pattern spettrali caratteristici:
  - Resonance: picco stretto e persistente
  - Harshness: energia diffusa in range 2-8kHz
  - Muddiness: accumulo energia 150-400Hz
- Pattern matching invece di soglie fisse

### 6. DYNAMIC RANGE NORMALIZATION
- Normalizzare rispetto al dynamic range del segnale
- Segnale con DR alto: soglie più alte
- Segnale con DR basso: soglie più basse
- Più robusto a variazioni di livello

### 7. MULTI-METHOD VOTING
- Usare 5-7 metodi diversi:
  1. Z-score outlier detection
  2. Percentile-based threshold
  3. Cross-validation (esistente)
  4. Temporal stability (esistente)
  5. Harmonic analysis
  6. Contextual validation
  7. Spectral coherence
- Richiedere consensus (almeno 4/7 metodi concordano)

### 8. PHASE ANALYSIS (opzionale, avanzato)
- Analizzare anche la fase per alcuni problemi
- Resonance: fase caratteristica
- Richiede FFT complesso (più costoso)

## IMPLEMENTAZIONE PRIORITARIA

**FASE 1 (Immediata - Alta priorità):**
1. Z-Score Outlier Detection
2. Multi-frame Consensus (rafforzare)
3. Contextual Validation

**FASE 2 (Media priorità):**
4. Harmonic Analysis
5. Spectral Coherence Analysis
6. Dynamic Range Normalization

**FASE 3 (Avanzato):**
7. Multi-method Voting
8. Phase Analysis

## RISULTATO ATTESO

- **Riduzione falsi positivi**: 70-80%
- **Mantenimento veri positivi**: >90%
- **Affidabilità complessiva**: 85-95%


