# REV8 S6 — Targeted counter-check on the patched normative SHA

**Verdetto byte-level storico:** `CLEAN`

**Verdetto sulla catena normativa corrente:** `BLOCK — S6 NON TRASFERITA A R23C`

**Indipendenza:** verifica Codex singola e riproducibile; **non** equivale al
counter-check indipendente a tre lenti dovuto dalla deroga §6 del ballot S6.

**REV8 SPEC GO:** `NO`

## 1. Scope

Questa verifica controlla la patch document-only applicata al target storico
in §11.4 e verifica separatamente se essa appartenga alla catena normativa
corrente. Non valuta o attiva cap O-09, runtime, training, gate floor,
Spearman general variable-value-marginal o altre righe del ledger REV8.

Artefatti verificati:

```text
Ballot S6 firmato:
docs/REV8_CANDIDATE_S6_SPEARMAN_REASON_CODE_MICRO_AMEND_BALLOT.md
SHA-256 b94c1b2c061db384d2b6a61688def5656b5bcdf75ec05a2904c2b45cd14220bf

Target storico dopo la patch:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256 be8658203e26fae3bc36d020733b6b7ed773d24ed0e78675330f08b459ff56cc

Implementazione candidata:
ml_v3/benchmark/rev8_o09_group_candidate.py
SHA-256 53033d7fcc872420b5420ff4fb825762ddcfb4defa458ffdfdf82f124e3622f6

Test candidate group-level:
ml_v3/tests/test_g1c_rev8_o09_group_candidate.py
SHA-256 c3fa1dd621a491770fd498935826a0d753bd04347292ecde60f24ccd25c02143

Commit patch: 11365cdb514c16794a9fa11ef83e39dd0a2462d9
Parent:       e36d2422b5895abb0721b11f34805090c4ad374a
```

## 2. Integrità e blast radius della patch

Il diff `e36d2422..11365cdb` sul target normativo contiene:

```text
1 hunk
24 righe aggiunte
0 righe rimosse
sezione interessata: §11.4 Spearman
```

Il target normativo non ha ulteriori modifiche fra `11365cdb` e lo snapshot
verificato. Il suo SHA-256 coincide con quello richiesto dal ballot e citato
dal draft A1.

Nessuna modifica S6 ha raggiunto:

- REV7 live;
- dispatcher;
- kernel candidate;
- cap O-09;
- metriche non Spearman;
- schema/runtime/training.

### 2.1 Authority drift rilevato

Il freeze R23C dichiara come artefatto normativo corrente:

```text
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_RC_AMENDED_DRAFT.md
SHA-256 684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb
```

con autorità composta dal ballot R23C firmato e dal report post-firma CLEAN.
Quel target non contiene il patch S6. Il `NORMATIVE_SIGNED_DRAFT` SHA
`be865...` è un predecessore storico, non il target corrente.

Di conseguenza il `CLEAN` di questo report dimostra la correttezza del testo
S6 applicato ai byte storici, ma **non** incorpora S6 nell'autorità R23C. Serve
un ballot di trasferimento che vincoli la stessa policy al target R23C senza
modificarne i byte congelati.

## 3. Conformità clausola per clausola

| Policy firmata | Testo §11.4 | Implementazione/test | Esito |
|---|---|---|---|
| certificato di unicità non disponibile | enumerato esplicitamente | `test_variable_marginal_certificate_unavailable_is_explicit_na` | CLEAN |
| risultato N/A | dichiarato | `GroupStatus.NOT_APPLICABLE` | CLEAN |
| reason code dedicato | `SPEARMAN_CERTIFICATE_UNAVAILABLE` | stesso enum/reason | CLEAN |
| non è PASS | dichiarato | nessun valore certificato | CLEAN |
| non è zero | dichiarato | `value is None` | CLEAN |
| non è runtime failure | dichiarato | distinto da `REJECTED / SOLVER_RUNTIME_FAILURE` | CLEAN |
| non diventa `PAIRING_AMBIGUOUS` senza prova | dichiarato | il test di ambiguità usa un inviluppo non singleton provato | CLEAN |
| blocca un gate Spearman obbligatorio | dichiarato | N/A resta fail-closed | CLEAN |
| non chiude il caso scientifico generale | dichiarato | `ballot_ready=false`; limitazione invariata | CLEAN |

## 4. Test eseguiti

Interprete canonico:

```text
/Users/marco/aieq_data/motore_v3/env/venv/bin/python
CPython 3.12.13
PYTHONDONTWRITEBYTECODE=1
```

Verifica mirata:

```text
python -m unittest \
  ml_v3.tests.test_g1c_rev8_o09_group_candidate.SpearmanTests \
  ml_v3.tests.test_g1c_rev8_o09_group_candidate.RuntimeFailureTests -v

14 test eseguiti
14 PASS
exit code 0
```

Regressione completa dopo la materializzazione dei boundary test A1:

```text
python -m unittest discover -s ml_v3/tests -p 'test_*.py' -q

540 test eseguiti
540 PASS
exit code 0
```

## 5. Limite epistemico

Il verdetto byte-level `CLEAN` vale per la corrispondenza fra policy S6
firmata, patch storico §11.4 e comportamento candidato osservato. Non sana
retroattivamente la deroga di processo e non supera l'authority drift verso
R23C. Una futura revisione indipendente può ancora riaprire S6 come previsto
dalla §6 del ballot.

## 6. Stato risultante

```text
Patch S6 storico §11.4                = TARGETED COUNTER-CHECK CLEAN
S6 nella authority R23C corrente       = ASSENTE; TRANSFER BALLOT RICHIESTO
Counter-check indipendente a tre lenti = DOVUTO, NON ESEGUITO
Spearman variable-value-marginal       = NON CERTIFICATO
Cap O-09                               = NON ATTIVI
Ballot A1                              = BLOCKED PENDING S6 TRANSFER
REV8 SPEC GO                           = NO
```
