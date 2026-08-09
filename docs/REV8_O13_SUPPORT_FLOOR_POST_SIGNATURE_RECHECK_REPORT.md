# REV8 — O-13 — SUPPORT FLOOR POST-SIGNATURE RECHECK

**Verdetto:** `POST_SIGNATURE_CLEAN`

**Metodo:** recheck Codex mirato, read-only e single-lens sul target firmato.
Non viene presentato come review indipendente multi-agent.

## 1. Target verificato

```text
Ballot:
docs/REV8_O13_SUPPORT_FLOOR_BINDING_BALLOT_DRAFT.md

Commit firmato:
59bec34856a08aa43cc52bb123e904a44575f4f6

SHA-256 firmato:
3b73e9b43e95140bb0e33a938afbedda57a907f49c3f67220d38b923363d0d14

Target pre-firma dichiarato nel ballot:
commit 45c49318f8cb8a6b9bbcb8cda29df929e5a5c563
SHA-256 f26de4f266a37b40007c8effebbc6dc048c07906b98a1d4fdda4e5eaeb81271d
```

Il file recuperato direttamente dal commit pre-firma produce esattamente lo
SHA dichiarato. Il commit firmato modifica un solo file: il ballot O13F_01.

## 2. Provenance ricontrollata

Gli artefatti referenziati dal ballot conservano i digest dichiarati:

```text
MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
398aea26daa6d48324f9df7e9dda54c38b332fb4fb230563d5c26172875745

REV8_SCIENTIFIC_AUTHORITY_BALLOT_PRECOMPILED_V2.md
cb799a076346ff1e33b74b50e5ceb2901e536c4fc0623fcc54ae5a5fa9c8afe0

REV8_ROUND2_3_R23C_CONFIRMATION_BALLOT.md
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

REV8_ROUND2_3_R23C_POST_SIGNATURE_RECHECK_REPORT.md
e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157

support_v2.py al momento della firma
036178d7cff4e61c0c83c96c63c5923d2335a2d13f19dca9ff038a0db86246b0
```

## 3. Firma

Il target firmato contiene:

```text
Decisione O13F_01: APPROVO
Firma/nome:        Marco
Data:              2026-08-09
```

La firma lega correttamente commit e SHA pre-firma. Non contiene
`APPROVO CON MODIFICHE`, campi vuoti o target differenti.

## 4. Recheck sostanziale

| Controllo | Esito |
|---|---|
| Nessun floor numerico nuovo | CLEAN |
| `n_required=max(contract_floor,n_power)` | CLEAN |
| `G_eligible` congelato prima delle prediction | CLEAN |
| `G_defined` sottoinsieme completo, non selezionato | CLEAN |
| doppio controllo eligible + defined | CLEAN |
| strata profilo/source-family/regione/direzione preservati | CLEAN |
| unique `group_id`, non unità/asset | CLEAN |
| N/A mai convertito in zero | CLEAN |
| valori sotto floor normativamente N/A | CLEAN |
| eccezione diagnostica limitata a ECE/Brier già autorizzata | CLEAN |
| p95 ancora diagnostico | CLEAN |
| policy hash-pinned prima delle prediction | CLEAN |
| power plan mancante senza fallback | CLEAN |
| mismatch policy/hash fatale | CLEAN |
| fixture under/on/over richieste | CLEAN |
| cap source-family 50% con boundary 15/30 e 16/30 | CLEAN |
| nessuna modifica a candidate/dispatcher/runtime/training | CLEAN |

## 5. Stop-rule

Non sono emersi:

- nuovi numeri o preferenze scientifiche non firmate;
- percorsi che permettono di usare `G_eligible` al posto di `G_defined`;
- fallback quando manca il power plan;
- possibilità di contare asset o unità come gruppi indipendenti;
- promozione del p95 diagnostico a gate;
- autorizzazione implicita a REV8, G1c, runtime o training.

Il recheck non certifica l'implementazione: al momento del controllo la floor
policy non è ancora materializzata nel codice.

## 6. Disposizione

```text
O13F_01 authority              = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_01 utilizzabile           = SI, per implementazione candidate-only
Support/p95/hierarchy code     = CANDIDATE-ONLY
Floor policy implementation   = NON ANCORA MATERIALIZZATA
REV7 dispatcher               = INVARIATO
O-13 close                    = NO
REV8 SPEC GO                  = NO
G1c close / G1 PASS           = NO
Runtime / training / push     = NON AUTORIZZATI
```

## 7. Prossima azione consentita

Implementare, senza export dal dispatcher REV7:

1. schema exact-key della policy O13F_01;
2. verifica del digest atteso;
3. calcolo `n_required`;
4. valutazione eligible/defined/strata;
5. reason code e blast radius firmati;
6. fixture under/on/over e mutation-oriented;
7. suite completa sul platform lock;
8. counter-check sul commit candidate immutabile.

La chiusura di questa lista non costituisce automaticamente O-13 close o
`REV8 SPEC GO`.
