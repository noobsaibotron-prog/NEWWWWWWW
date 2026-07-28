# Protocollo Assistenti Esterni — Ember Core

**Versione:** 1.0  
**Data:** 2026-07-28  
**Scope:** Assistenti coding free-tier senza accesso live a repo/worktree (Kilo free, Hermes, ChatGPT free, Gemini free, ecc.)  
**Snapshot di riferimento:** Commit `HEAD` al momento della consegna dello zip — ogni output deve dichiarare esplicitamente hash e data.

---

## 1. Principio Fondamentale

> **Un assistente senza esecuzione non può verificare. Deve quindi citare.**

Ogni affermazione fattuale sul codice, sulla storia, sull'architettura deve essere ancorata a **file:riga** dentro lo snapshot consegnato. Niente parafrasi, niente "il progetto fa X", niente memoria. Se non c'è la citazione, l'affermazione è **non verificata** e va trattata come ipotesi.

---

## 2. Dichiarazione di Staleness Obbligatoria

**Ogni risposta deve aprire con:**

```
ANALISI CONTRO SNAPSHOT: <commit-hash-short> (<data-ISO>)
STATO: verificato-contro-snapshot | assunto-senza-verifica | ipotesi-da-validare
PERIMETRO: <elenco file/cartelle esaminati in questa risposta>
```

Esempio:
```
ANALISI CONTRO SNAPSHOT: a1b2c3d (2026-07-28)
STATO: verificato-contro-snapshot
PERIMETRO: Source/ML/ResonanceModel.cpp:1-120, docs/ARCHITECTURE.md:45-89
```

Niente eccezioni. Questo sostituisce la verifica per esecuzione che l'assistente non può fare.

---

## 3. Separazione Meccanica: Trovato vs Deciso

Due colonne, sempre. Mai mescolare.

| Claim (cosa si afferma) | Evidenza (file:riga) | Tipo |
|---|---|---|
| `ResonanceModel usa 8 classi semantiche` | `Source/ML/ResonanceModel.cpp:45-52` | **TROVATO** |
| `Serve aggiungere classe "Transient"` | — | **DECISO** (design choice, richiede conferma umana) |
| `uniqueItems manca su semantic_regions` | `schemas/detection.json:120` (assenza) | **TROVATO** (negativo) |
| `Il fix corretto è aggiungere uniqueItems` | — | **DECISO** (richiede validazione) |

**Regola:** se la colonna "Evidenza" è vuota o dice "—", è una decisione normativa, non un fatto. L'umano deve approvarla.

---

## 4. Profili di Errore per Agente — Contromisure Mirate

### 4.1 Kilo (free) — Scope Creep / Deriva di Perimetro

**Sintomo:** Veloce, capace, ma se gli lasci margine interpreta il brief come "fai tutto ciò che serve" e tocca file fuori scope, cambia architettura, introduce dipendenze.

**Contromisura: Ticket Meccanico Obbligatorio**

Prima di *qualsiasi* azione, Kilo deve ricevere (e confermare) un ticket in questo formato esatto:

```markdown
## TICKET KILO-<NNN>
**Obiettivo atomico:** <una frase, un solo deliverable>
**File CONSENTITI (whitelist):**
- `Source/ML/ResonanceModel.cpp`
- `include/Ember/ML/ResonanceModel.h`
- `tests/ML/test_resonance.cpp`
**File VIETATI (blacklist):**
- `Source/DSP/*`
- `Source/UI/*`
- `CMakeLists.txt` (salvo esplicito ok)
**Criterio di completamento (Definition of Done):**
- [ ] Compila senza warning nuovi
- [ ] Test esistenti passano
- [ ] Nuovo test per il comportamento aggiunto in `tests/ML/test_resonance.cpp`
- [ ] Nessun file fuori whitelist modificato
**Cose esplicitamente VIETATE:**
- Refactoring non richiesto
- Cambi di stile/codifica
- Aggiornamento dipendenze
- Commit/push (solo staging locale)
**Contro-check richiesto prima di consegnare:**
- `git diff --stat` mostra solo file in whitelist
- `grep -r "TODO\|FIXME\|HACK"` sui file toccati = 0 risultati nuovi
```

**Kilo non inizia finché non risponde "TICKET CONFERMATO" ripettando l'elenco sopra.**  
Se durante il lavoro scopre che serve un file fuori whitelist → **si ferma e chiede**, non lo tocca.

---

### 4.2 Hermes (terminale locale, free) — Perdita di Contesto / Fedeltà

**Sintomi misurati stasera:**
1. **Tracciamento sezione sbagliato:** cita riga giusta, sezione wrong (§10.0 vs §9.3) — perde il filo in documenti lunghi
2. **Riscrittura non fedele:** cade qualificatori ("Thinness/DullSound") allargando silentemente scope
3. **Assenza verifica:** afferma esistenza campi JSON senza controllare

**Contromisure: Micro-Task + Diff Obbligatorio**

#### A. Mai "rivedi tutto il documento"
**Vietato:** "rivedi PLAN.md", "controlla ARCHITECTURE.md"  
**Obbligatorio:** "guarda SOLO `PLAN.md:120-145` e dimmi cosa dice su X"

#### B. Riscrittura solo via Diff Riga-per-Riga
**Template obbligatorio per ogni modifica testo/codice:**

```diff
--- a/docs/PLAN.md
+++ b/docs/PLAN.md
@@ -9,7 +9,7 @@
 ## C3: Detection Rules
-### §10.0 Thinness/DullSound Detection
+### §9.3 Thinness/DullSound Detection
```

Se l'output non è un diff unificato, **non viene accettato**. Questo rende visibile ogni cancellazione non spiegata.

#### C. Verifica Puntuale Obbligatoria
Per ogni claim su struttura dati (JSON, schema, API):
```
CLAIM: "uniqueItems esiste su semantic_regions"
VERIFICA: `grep -n "uniqueItems" schemas/detection.json | head -5`
RISULTATO: [output esatto del comando]
```

Hermes deve incollare l'output del grep. Se non lo fa, il claim è **assunto-senza-verifica**.

---

## 5. Checklist Ricezione (Lato Umano)

Prima di accettare *qualsiasi* output da assistente esterno:

- [ ] **Dichiarazione staleness presente?** (hash, data, stato, perimetro)
- [ ] **Tabella Claim/Evidenza/Tipo presente?** (sezione 3)
- [ ] **Citazioni file:riga per ogni fatto?**
- [ ] **Se Kilo: ticket confermato? whitelist rispettata? diff solo whitelist?**
- [ ] **Se Hermes: micro-task? diff unificato? output grep incollato per claim strutturali?**
- [ ] **Nessuna decisione normativa (colonna "DECISO") accettata senza tuo ok esplicito**

Se una casella manca → **rispedisci con richiesta precisa**, non correggere tu.

---

## 6. Formato Consegna Zip (Handoff)

Lo zip consegnato all'assistente esterno deve contenere:

```
/handoff/
├── SNAPSHOT_INFO.txt          # commit hash, data, branch, worktree origine
├── PROTOCOL.md                # questo documento
├── TICKET_TEMPLATE_KILO.md    # sezione 4.1 pronta all'uso
├── MICROTASK_TEMPLATE_HERMES.md # sezione 4.2 pronta all'uso
├── CLAIM_EVIDENCE_TEMPLATE.md # tabella vuota sezione 3
├── REPO_TREE.txt              # `git ls-files` output
├── KEY_FILES/                 # solo i file citati nei ticket/micro-task
│   ├── Source/ML/...
│   ├── schemas/...
│   └── docs/...
└── TEST_BASELINE.txt          # output `ctest --output-on-failure` su snapshot
```

**Regola:** l'assistente esterno **non riceve tutto il repo**. Riceve solo `KEY_FILES/` mirati al ticket/micro-task. Questo forza il perimetro e riduce token/context.

---

## 7. Esempio Completo — Interazione Kilo

**Umano prepara:**
```markdown
## TICKET KILO-042
Obiettivo: Aggiungere validazione uniqueItems su semantic_regions in detection.json
File CONSENTITI: schemas/detection.json, tests/schemas/test_detection.py
File VIETATI: tutto il resto
DoD: schema valido, test passa, nessun altro file toccato
```

**Kilo risponde:**
```
TICKET CONFERMATO. Perimetro: schemas/detection.json, tests/schemas/test_detection.py

ANALISI CONTRO SNAPSHOT: a1b2c3d (2026-07-28)
STATO: verificato-contro-snapshot
PERIMETRO: schemas/detection.json:110-135

| Claim | Evidenza | Tipo |
|---|---|---|
| semantic_regions non ha uniqueItems | schemas/detection.json:115-125 (assenza) | TROVATO |
| Aggiungere uniqueItems: true risolve | — | DECISO (richiede ok) |

Diff proposto:
--- a/schemas/detection.json
+++ b/schemas/detection.json
@@ -112,6 +112,7 @@
       "semantic_regions": {
         "type": "array",
         "items": { "$ref": "#/definitions/semantic_region" },
+        "uniqueItems": true
       },
```

**Umano verifica checklist → approva DECISO → Kilo applica diff → staging only.**

---

## 8. Esempio Completo — Interazione Hermes

**Umano chiede:**
```
MICRO-TASK HERMES-007
Guarda SOLO docs/PLAN.md:9.3 (righe 85-110)
Domanda: Qual è il titolo esatto della sezione C3 e quali qualificatori elenca?
Output richiesto: citazione riga-per-riga + diff se c'è discrepanza con memoria
```

**Hermes risponde:**
```
ANALISI CONTRO SNAPSHOT: a1b2c3d (2026-07-28)
STATO: verificato-contro-snapshot
PERIMETRO: docs/PLAN.md:85-110

Righe 85-110:
85: ## C3: Detection Rules
86: ### §9.3 Thinness/DullSound Detection
87: Qualificatori: Thinness, DullSound
...

Nessuna discrepanza rilevata. Sezione corretta è §9.3, non §10.0.
```

**Umano:** conferma, nessun diff necessario.

---

## 9. Governance

- Questo protocollo è **vincolante** per ogni assistente esterno su Ember Core.
- Modifiche solo con approvazione esplicita umana + versione incrementale.
- Eccezioni documentate in `EXCEPTIONS_LOG.md` con motivazione e scadenza.

---

*Generato da analisi errori reali sessione 2026-07-28. Basato su evidenza misurata, non presupposti.*