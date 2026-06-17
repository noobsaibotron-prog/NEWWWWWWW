# Tassonomia dei Dichiaranti

Il Tribunale della Veridicità è **agnostico rispetto alla fonte**: giudica l'affermazione, non chi
la pronuncia. Questo documento cataloga i tipi di **Dichiarante**, perché — pur restando identico il
giudizio — ogni fonte ha *modalità di fallimento ricorrenti* diverse e richiede di **enfatizzare**
test di legittimità diversi. La scala dei verdetti, gli oneri e i tre gradi non cambiano mai.

> **Principio (Art. 0-bis del SKILL):** la reputazione del Dichiarante non è né aggravante né attenuante.
> Un'affermazione vera resta vera anche se la fa una fonte inaffidabile; una falsa resta falsa anche
> se la fa l'autore del codice. La fonte determina *dove cercare le prove* e *cosa sospettare*, mai il verdetto.

---

## Tabella Madre

| # | Dichiarante | Voce/forma tipica | Modalità di fallimento ricorrente | Test di legittimità da enfatizzare | Dove si raccoglie la prova |
|---|-------------|-------------------|-----------------------------------|------------------------------------|----------------------------|
| D1 | **Assistente AI** | "Ho implementato/corretto X", "ora è lock-free" | Allucinazione costruttiva; conferma compiacente ("fatto!"); over-confidence | Attribuzione + Esistenza | trascrizione chat + `git log`/diff |
| D2 | **Sviluppatore umano** | "L'ho sistemato", "gira da me", "è banale" | Memoria selettiva; "works on my machine"; fix non committato | Attribuzione + Comportamento | `git blame`/`git show` + build/test |
| D3 | **Documentazione** (README, `docs/`) | "latenza di soli 128 sample", "fino a 24 bande" | Documentazione aspirazionale; drift (codice cambiato, doc no) | Portata + Comportamento | `README.md:NN`, `docs/*` ↔ codice |
| D4 | **Messaggio di commit** | "fix: rimosso SpinLock dal RT path" | Commit-message optimism; il messaggio promette più del diff | Attribuzione (diff del commit) | `git show <sha>` ↔ codice corrente |
| D5 | **Commento nel codice** | `// RT-safe`, `// FIXED`, `// lock-free now` | Commento obsoleto sopravvissuto al refactor; auto-certificazione | Non-contraddizione (è imputazione, mai prova) | `file:linea` |
| D6 | **Specifica / requisito** | "tutti i parametri devono essere automatizzabili" | Requisito mai implementato; scope dichiarato ≠ scope reso | Portata + Esistenza | spec/issue ↔ APVTS/codice |
| D7 | **Marketing / store / sito** | "production-grade", "zero latency", "AI-powered" | Overclaiming commerciale; aggettivi non falsificabili presi per fatti | Portata + Non-contraddizione | materiale ↔ verdetti CTU |
| D8 | **Bug report / issue** | "il preset perde lo stato del dynamic EQ" | Falso positivo; problema reale ma causa sbagliata; non riproducibile | Comportamento (riproduzione) | issue ↔ test di roundtrip/regressione |

---

## Note operative per dichiarante

### D1 — Assistente AI
Il caso d'uso originario. Sospetto principale: l'AI dichiara `// FIXED` o "fatto" senza che il codice
sia cambiato, oppure descrive un'architettura ideale invece di quella reale. **Sempre** incrociare
con `git log` la frase "ho fatto/rimosso/corretto".

### D2 — Sviluppatore umano
Stessa identica procedura dell'AI: nessuno sconto perché è l'autore. Il "gira da me" non è prova
(manca riproducibilità → Secondo Grado insoddisfatto). Un fix descritto a voce ma non presente nel
repo è `FALSO` (esistenza) o `NON PROVATO` se il commit non è ancora stato fetchato.

### D3 — Documentazione
Fonte più soggetta a **drift**: il codice evolve, la doc resta. Il README di AIEQ Pro è già stato
processato (Sentenza 001): numeri esatti (porte, conteggi enum) reggono, le proprietà architetturali
("lock-free") sono FUORVIANTI. Enfatizzare il Test di Portata.

### D4 — Messaggio di commit
Il messaggio è un'imputazione; il **diff del commit** è la prova. Un commit `fix: rimosso SpinLock`
si verifica con `git show <sha>`: se il `SpinLock` è ancora lì o è stato spostato altrove → `FALSO`
o `FUORVIANTE`. Cfr. Bug #3 (`REPORTS/CRITICAL_BUGS_ANALYSIS_2026-01-06.md`).

### D5 — Commento nel codice
**Mai** prova (Massima 003, `registro_giurisprudenza.md`). Esempio reale: `AIEngine.h:318`
`// FIX RT-SAFETY: atomic` è un'imputazione, non una garanzia. Può essere iscritto a ruolo come capo
a sé. Il commento obsoleto è il fallimento più comune e silenzioso.

### D6 — Specifica / requisito
Si giudica la distanza tra *scope dichiarato* e *scope reso*. "Tutti i parametri automatizzabili" →
contare i parametri APVTS vs il totale dichiarato. È spesso `VERO PARZIALE` (la maggior parte sì, alcuni no).

### D7 — Marketing
Attenzione agli aggettivi **non falsificabili** ("premium", "intelligente"): non sono capi
processabili finché non si traducono in un predicato verificabile. "Zero latency" invece è
falsificabile (→ misura della latenza). "Production-grade" si appoggia al verdetto di CTU-7.

### D8 — Bug report / issue
Qui il Dichiarante afferma un *difetto*, non un pregio: il framework funziona specularmente. La
dichiarazione "X è rotto" è vera se un test la riproduce, `NON PROVATO` se non riproducibile,
`FALSO` se il comportamento atteso è invece dimostrato corretto.

---

## Adattamento del Template di Sentenza

Nel template (`template_sentenza.md`) il campo **Dichiarante** va sempre compilato con il tipo (D1–D8)
e l'identificativo concreto (es. "D3 — README.md:5", "D4 — commit ef67a84", "D1 — chat 2026-06-16").
Questo rende ogni sentenza tracciabile alla fonte senza che la fonte ne influenzi l'esito.
