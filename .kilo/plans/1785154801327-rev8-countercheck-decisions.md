# Piano: Decisioni bloccanti C1 e C2 per REV8

## Stato
Documento di lavoro — nessuna modifica al repository.
Data: 2026-07-27
Branch: `feature/motore-v3-g1c-reseal` (non accessibile da questo ambiente)

---

## Decisioni bloccanti

### C1 — Banda derivata evento: Opzione A (continua)

**Decisione**: Snap normativo al centro canonici più vicino; overlap evento resta continuo (`band_iou_log2`).

**Prove incontrovertibili dal REV7 vivo**:

1. **§6.1 (righe 477-488)** definisce 120 centri canonici da 20 a 20000 Hz su asse `log2(f)`, bande triangolari. La griglia è l'unica base di riferimento per qualsiasi banda.
2. **§10.2 (righe 996-1000)** usa `band_iou_log2` continuo per Harshness/Sibilance: "overlap di banda almeno 0.5". Non esiste alcuna nozione di banda discreta a indici interi nel contratto vivo.
3. **§10.1 (righe 989-992)** usa misure continue per tutte le metriche tonali: Resonance (1/3 ottava), Muddiness/Boominess/BoxyMidrange (overlap ≥ 0.5), Thinness/DullSound (stessa regione e direzione).
4. **§9.2 (righe 808-810)** valida bande in Hz (20-20000), non in indici. Il validator è Hz-based.
5. **§9.3 (righe 838-889)** definisce `dynamic_events[]` con campo `center_hz` e `width_octaves` — entrambi float, non indici.

**Perché Opzione B è provata come peggiorativa**:
- Introduce "due nozioni di banda conviventi" (testo piano riga 172), esplicitamente chiamato problema dal piano correzioni.
- Cambierebbe la semantica delle metriche evento da continua a discreta senza una revisione completa di §10.1-10.2.
- Il validator è Hz-based; cambiare a indici richiederebbe modifiche allo schema e al validator.
- Le semantic region mantengono IoU continua (§10.1), creando un'incoerenza interna nel contratto.

**Rischio Opzione A**: Nessuno. È coerente con la griglia e le metriche già definite in REV7.

---

### C2 — Solver matching: Opzione B (FAIL deterministico per limite risorse)

**Decisione**: Accettare il limite di risorse con FAIL; il testo proposto è sufficiente.

**Prove incontrovertibili**:

1. **Comportamento esistente**: Il piano correzioni (riga 96) afferma esplicitamente: "`ml_v3/benchmark/event_matching.py` già FAIL sopra `MAX_SEARCH_NODES=200000`". Questo è il comportamento reale del codice nel branch.
2. **Allineamento testo-codice**: Il testo C2 proposto (righe 83-94) dice: "un'implementazione che non possa garantire l'ottimo esatto entro le proprie risorse produce FAIL. Un FAIL è deterministico e indipendente dall'ordine dei record." Questo è esattamente ciò che il codice fa oggi.
3. **NP-hardness**: L'obiettivo lessicografico di §10.2 (righe 1001-1004) — max match, poi max IoU, poi min errore frequenziale, poi min ID — è un problema di matching bipartito con vincoli multipli. Non esiste un algoritmo polinomiale noto per la versione lessicografica a 6 chiavi.
4. **Opzione A non chiudibile**: Il piano correzioni (riga 88) dice: "la procedura deterministica che la realizza va progettata e citata prima che questa sezione possa chiudere." Nessun algoritmo è stato progettato né citato.

**Perché Opzione A è provata come bloccante**:
- Richiede la progettazione e citazione di un algoritmo deterministico esatto per matching bipartito a 6 chiavi lessicografico.
- Nessun tale algoritmo esiste nel documento né nel codice.
- Chiudere §10.1 con Opzione A senza l'algoritmo violerebbe la richiesta stessa del piano correzioni.

**Rischio Opzione B**: Nessuno. Il codice già implementa FAIL sopra la soglia; il contratto si limita a formalizzare il comportamento esistente.

---

## C1-bis — Schema fix (incluso nella decisione C1)

**Decisione**: Applicare `minimum: 0` al posto di `exclusiveMinimum: 0` su `width_octaves` in `schemas.py` righe 308 e 396.

**Prova**: `exclusiveMinimum: 0` rifiuta `width_octaves = 0`, ma C1 definisce l'evento a banda singola come `width_octaves = 0` e `i_lo == i_hi` — esplicitamente "rappresentabile e non degenere". Quindi `exclusiveMinimum: 0` è un bug nello schema che C1 corregge.

---

## Piano operativo con checkpoint di verifica

### Fase 0 — Verifica pre-autorizzazione (solo lettura)
1. ✅ `ml_v3/benchmark/event_matching.py` — `MAX_SEARCH_NODES = 200_000` (riga 71), `EventMatchingError` alzata a riga 260-262 quando `nodes > MAX_SEARCH_NODES`. C2 Opzione B è già implementata nel codice.
2. ✅ `ml_v3/contracts/schemas.py` — `width_octaves` usa `"exclusiveMinimum": 0` in entrambi gli schema (dynamic event e annotation event). C1-bis (`minimum: 0`) è un bug fix necessario: `exclusiveMinimum: 0` rifiuta `width_octaves = 0`, ma C1 definisce l'evento a banda singola come valido con `width_octaves = 0`.
3. ✅ `ml_v3/contracts/validate.py` — Il validator controlla direzione, banda, severity, confidence — nessun controllo duplicati su `(problem_type, start_s, end_s, band_lo_hz, band_hi_hz)` per regioni GT né su `semantic_payload` per eventi GT. C4 richiede codice nuovo.
4. ✅ `ml_v3/contracts/profiles.py` — `problem_type_id()` mappa 8 tipi (Resonance=0, Harshness=1, Muddiness=2, Sibilance=3, Boominess=4, Thinness=5, BoxyMidrange=6, DullSound=7). `anomaly_class_index()` mappa 3 classi anomalie dense. Gli indici sono diversi: Resonance=0 in entrambi, ma Harshness=1 in `problem_type_id` e 3ª classe anomalia (indice 2), Sibilance=3 in `problem_type_id` e 2ª classe anomalia (indice 1). C5 confermato.
5. ✅ `ml_v3/tests/test_g1c_rev8_primitives_v2.py` — 431 righe, confermato.

### Fase 1 — Modifiche documento (dopo autorizzazione C1 e C2)
1. Applicare C1 (Opzione A) a `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §9.2 righe 544-546
2. Applicare C2 (Opzione B) a `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md` §10.1 riga 969
3. Applicare C3 a §9.3 righe 633-635
4. Applicare C4 dopo §9.6 riga 702
5. Applicare C5 dopo §9.3 riga 604
6. Se C1 Opzione B (non scelta): aggiungere modifica esplicita a §10.2

### Fase 2 — Modifiche codice (dopo Fase 1 approvata)
1. `ml_v3/contracts/schemas.py` righe 308, 396: `exclusiveMinimum: 0` → `minimum: 0`
2. `ml_v3/contracts/validate.py`: aggiungere controllo duplicati GT per C4
3. Se C2 Opzione B (scelta): verificare che `event_matching.py` alzi `EventMatchingError` sopra soglia

### Fase 3 — Test
1. `python -m pytest ml_v3/tests/test_g1c_rev8_primitives_v2.py -v`
2. `python -m pytest ml_v3/tests/test_g1c_t1_evaluator_parser.py -v`
3. Se C4 validator aggiunto: scrivere test duplicati GT
4. Se C1 Opzione B: aggiungere test `band_iou_index`

### Fase 4 — Review
1. Diff solo su `docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md`, `ml_v3/contracts/schemas.py`, `ml_v3/contracts/validate.py`
2. Nessuna modifica a `docs/MOTORE_V3_G1_CONTRACT.md` (REV7 vivo)
3. Nessuna modifica a `Source/`, `CMakeLists.txt`, `ml_v2/`

---

## Constraints

- Sola lettura finché Marco non autorizza C1 e C2
- Nessun commit finché il diff non è approvato
- Nessuna modifica a `docs/MOTORE_V3_G1_CONTRACT.md` (REV7)
- Nessun tocco a C++ (`Source/AI/*`, `Source/PluginProcessor.*`)
- Non mischiare worktree: codice in `motore-v3-g1c-reseal`, non in `motore-v3-offline` o `prominence-phase1`
- Non aprire coding AI/agent/Cursor/Claude su questo branch senza autorizzazione