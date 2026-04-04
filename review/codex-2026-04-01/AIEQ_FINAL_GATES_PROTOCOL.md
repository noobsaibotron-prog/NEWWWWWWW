# AIEQ+ Final Gates Protocol: Verso lo stato RELEASE-SAFE

**Data:** 2026-04-04
**Autore:** Manus AI
**Progetto:** AI Equalizer Pro (AIEQ)
**Obiettivo:** Definizione dei criteri di superamento per i 4 gate mandatori.

---

## 1. Introduzione

Per passare dallo stato **RELEASE-CANDIDATE (Verified)** allo stato **RELEASE-SAFE**, AI Equalizer Pro deve superare quattro test di validazione esterna e di stress-test automatizzato. Questo protocollo definisce le procedure, i criteri di successo e gli artefatti richiesti per ogni gate.

---

## 2. Gate 1: Host Matrix Validation (HMV)

L'obiettivo è garantire la stabilità del plugin nelle principali DAW commerciali, verificando il corretto caricamento, la stabilità del processamento audio e la gestione delle risorse GUI.

| DAW | OS | Versione | Criteri di Successo |
|---|---|---|---|
| **Reaper** | Windows/macOS | v7.x | Caricamento VST3 istantaneo, zero glitch nel bypass, corretta automazione parametri. |
| **Ableton Live** | Windows/macOS | v11/12 | Stabilità durante il plugin-scan, gestione corretta dei buffer variabili. |
| **Logic Pro** | macOS (AU) | v11.x | Superamento test di validazione AU (auval), stabilità in modalità M/S. |
| **Cubase** | Windows/macOS | v13.x | Corretto rendering offline, stabilità del rendering OpenGL nel pannello. |
| **Pro Tools** | Windows/macOS | v2024.x | Validazione formato AAX, stabilità durante il commit/freeze delle tracce. |

**Artefatto Richiesto:** `HMV_MATRIX_REPORT.yaml` con log di validazione per ogni host.

---

## 3. Gate 2: Recall Determinism (RD)

Verifica che il salvataggio e il caricamento dello stato (XML/Binary) producano risultati identici, senza perdite di dati o derive nei parametri semantici.

*   **Procedura:**
    1.  Impostare uno stato complesso (AI attivo, morphing parziale, parametri M/S asimmetrici).
    2.  Salvare lo stato tramite `getStateInformation()`.
    3.  Modificare i parametri.
    4.  Ricaricare lo stato tramite `setStateInformation()`.
    5.  Confrontare i coefficienti dei filtri e lo stato dell'AIEngine.
*   **Criterio di Successo:** Delta numerico = 0 per tutti i parametri APVTS; coerenza totale dello stato semantico.

---

## 4. Gate 3: Randomized Stress Harness (RSH)

Test di robustezza contro condizioni operative estreme e variabili, simulando comportamenti anomali dell'host o dell'utente.

*   **Test Case:**
    1.  **Buffer Switch:** Cambio rapido della dimensione del buffer (64 -> 1024 -> 128 campioni) durante il playback.
    2.  **Sample Rate Warp:** Cambio della frequenza di campionamento (44.1k -> 96k) durante il processamento.
    3.  **Rapid Parameter Storm:** Automazione di 100+ parametri contemporaneamente a velocità audio.
    4.  **OpenGL Resize Stress:** Ridimensionamento continuo della finestra della GUI per 60 secondi.
*   **Criterio di Successo:** Zero crash, zero audio dropouts (oltre il tempo di ricalcolo), zero deadlock (SpinLock validation).

---

## 5. Gate 4: DynEQ Runtime Validation (DRV)

Validazione del processore dinamico (Dynamic EQ) in tempo reale, focalizzandosi sulla stabilità del lookahead e sulla precisione dell'inviluppo.

*   **Test Case:**
    1.  **Attack/Release Precision:** Verifica dei tempi di intervento rispetto ai valori impostati.
    2.  **Lookahead Consistency:** Assenza di sfasamenti temporali tra il segnale di sidechain e il segnale processato.
    3.  **High Gain Stress:** Stabilità dei filtri IIR con gain estremi (-24dB / +24dB) e Q elevati.
*   **Criterio di Successo:** Errore di inviluppo < 0.1dB; stabilità dei filtri verificata matematicamente (Z-plane stability).

---

## 6. Conclusioni

Il superamento di questi 4 gate porterà il rating commerciale a **9.0+** e sbloccherà lo stato **RELEASE-SAFE**. Gli script per i gate 2 e 3 verranno sviluppati nella fase successiva di questo piano.
