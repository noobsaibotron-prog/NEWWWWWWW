# QA PROTOCOL v1.0 — AI Equalizer Pro (AIEQ+)

**Date:** 2026-04-04
**Auditor:** Manus AI
**Version:** 1.0 (Release Candidate Preparation)
**Status:** DRAFTED (Pending Execution)

---

## 1. Obiettivo del Protocollo
L'obiettivo di questo protocollo è validare che i fix apportati (D1, P1, P2, T-6) non abbiano introdotto regressioni e che il plugin sia pronto per il rilascio commerciale in termini di stabilità, sicurezza DSP e coerenza dell'interfaccia.

---

## 2. Test Suites Mandatorie

### 2.1 Suite A: DSP Integrity & Regression (D1, P1)
| Test ID | Descrizione | Criterio di Successo |
|---|---|---|
| **QA-A1** | **D1 Peak Identity** | Errore di fase < -100dB a guadagno 0.0dB. |
| **QA-A2** | **P1 M/S Crossfade** | Nessun clic udibile o spike di ampiezza durante il cambio Stereo ↔ Mid/Side. |
| **QA-A3** | **P1 Dual-Path Sync** | Allineamento temporale perfetto tra i due rami di processamento durante il crossfade. |

### 2.2 Suite B: RT-Safety & Threading (P2, T-6)
| Test ID | Descrizione | Criterio di Successo |
|---|---|---|
| **QA-B1** | **P2 Atomic Stability** | Nessuna data race rilevata tra il thread audio e il thread AI durante il cambio di `correctionMode`. |
| **QA-B2** | **P2 Lazy Apply** | Conferma che `applyProfileThresholds()` non venga mai eseguito nel thread audio. |
| **QA-B3** | **T-6 OSC Silence** | Assenza totale di tentativi di scrittura su disco (Desktop) da parte di `OSCParameterServer`. |

### 2.3 Suite C: Stress Test & Robustness
| Test ID | Descrizione | Criterio di Successo |
|---|---|---|
| **QA-C1** | **Fuzz Block Size** | Stabilità totale con block size variabili (16, 32, 64, ..., 4096 campioni). |
| **QA-C2** | **Sample Rate Agility** | Corretto ricalcolo dei filtri al cambio di sample rate (44.1k ↔ 96k ↔ 192k). |
| **QA-C3** | **Rapid MS Toggle** | Cambio frenetico della modalità M/S (10Hz) senza crash o corruzione dei buffer. |

---

## 3. Metodologia di Esecuzione
1.  **Automated Execution:** Esecuzione delle suite di unit test e integration test tramite CMake/JUCE Unit Test Runner.
2.  **Manual Trace:** Analisi del codice per i pattern RT-safety (P2, T-6).
3.  **Numerical Log:** Cattura dei log numerici durante i crossfade per l'analisi della fase.

---

## 4. Criteri di Accettazione (Pass/Fail)
- **CRITICAL/HIGH:** 0 fallimenti ammessi.
- **MEDIUM:** Massimo 2 fallimenti (con piano di remediation post-rilascio).
- **LOW:** Ammessi fino a 5 fallimenti.

---

## 5. Prossimi Passi
- Esecuzione Fase 2: Regression Suite.
- Esecuzione Fase 3: M/S Stress Test.
