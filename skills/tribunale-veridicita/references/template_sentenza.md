# Template della Sentenza

Struttura canonica della sentenza emessa dal Tribunale della Veridicità. Copiare in
`REPORTS/SENTENZA_VERIDICITA_YYYY-MM-DD.md` e compilare. Ogni affermazione del giudice deve essere
ancorata a `file:linea` o a un test.

---

```markdown
# SENTENZA DI VERIDICITÀ N. <progressivo>/<anno>

**Tribunale della Veridicità — AIEQ Veritas Court**
**Data:** YYYY-MM-DD
**Corpus processato:** <es. README.md §"Funzionalità Principali"> / <chat del gg/mm> / <commit range>
**Dichiarante:** <tipo D1–D8 + identificativo concreto, es. "D3 — README.md:5" | "D4 — commit ef67a84" | "D2 — sviluppatore, review del gg/mm">
**Giudice estensore:** <chi giudica: IA che esegue la skill o revisore umano>

---

## Dispositivo (sintesi dei verdetti)

| # | Capo d'imputazione (dichiarazione) | CTU | I° | II° | III° | VERDETTO |
|---|-----------------------------------|-----|----|----|------|----------|
| 1 | "<testo letterale>" | CTU-x | ✅ | ✅ | ✅ | 🟢 VERO PROVATO |
| 2 | "<testo letterale>" | CTU-y | ✅ | ⚠️ | — | 🟡 NON PROVATO |
| 3 | "<testo letterale>" | CTU-z | ✅ | ✅ | ❌ | 🟠 FUORVIANTE |

**Indice di Veridicità del corpus:** <valore> → **<FONTE ATTENDIBILE | DA VERIFICARE | INAFFIDABILE>**

---

## Motivazione (per ciascun capo)

### Capo N.1 — "<dichiarazione letterale>"
- **Fonte:** <file:linea del README / messaggio chat>
- **CTU competente:** CTU-x (<materia>)
- **Primo Grado (Esistenza):** <verdetto> — prova: `path/file.cpp:NN` (<cosa mostra>)
- **Secondo Grado (Comportamento):** <verdetto> — prova: `Source/Tests/XTest.cpp` / misura / percorso da `processBlock`
- **Terzo Grado (Legittimità):** <verdetto> — test applicati: [non-contraddizione | portata | attribuzione | principio]; esito.
- **Prove a carico (Accusa):** <citazioni>
- **Prove a discarico (Difesa):** <citazioni>
- **Aggravanti/Attenuanti:** <eventuali>
- **VERDETTO:** <verdetto della scala> — <motivazione di una riga>

(ripetere per ogni capo)

---

## Nuovi Precedenti (per il Registro di Giurisprudenza)

- <eventuale nuova massima emersa, da riportare in `registro_giurisprudenza.md`>

---

## Giudizio di Veridicità Aggregato

<2-4 frasi: la fonte è attendibile? Quali pattern di dichiarazione ricorrono? Raccomandazioni —
es. "convertire i NON PROVATO scrivendo i test mancanti", "correggere il claim FUORVIANTE nel README">

---

## Azioni Raccomandate (non vincolanti)

1. <es. riformulare il claim X nel README da "lock-free" a "snapshot atomico + SPSC sul percorso AI→audio">
2. <es. scrivere `LinearPhaseLatencyTest.cpp` per provare la latenza 128>
```

---

## Esempio Lavorato (estratto)

> **Capo N.3 — "architettura lock-free per il thread audio"**
> - Fonte: `README.md:5`
> - CTU: CTU-1 (RT-Safety), CTU-2 (Threading)
> - I° Esistenza: `FATTO PROVATO` — `Source/Core/LockFreeStructures.h:128` definisce `SPSCQueue`; `:35` `AtomicSnapshot`. Le strutture lock-free esistono davvero.
> - II° Comportamento: `COMPORTAMENTO PROVATO (parziale)` — lo spectrum snapshot usa l'atomic snapshot; ma `AIEngine.cpp:206` acquisisce `std::lock_guard<std::mutex> lock(spectrumMutex)`.
> - III° Legittimità: `CASSATO` — Test di Portata (Massima 004) e Non-Contraddizione (Massima 001) falliti: `AIEngine.h:511` mantiene `std::mutex spectrumMutex`; `:535` `correctionsWriteMutex`; `:553` `historyMutex`. L'aggettivo "lock-free" esteso all'"architettura" è generalizzazione oltre l'evidenza.
> - Prove a carico: `AIEngine.h:511,535,553`; `AIEngine.cpp:206,546,650`.
> - Prove a discarico: `LockFreeStructures.h:35,128`.
> - Attenuante: buona fede tecnica (le strutture esistono e parte del path è genuinamente lock-free).
> - **VERDETTO: 🟠 FUORVIANTE** — vero che esistono percorsi lock-free, falso che lo sia l'architettura.
