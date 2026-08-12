# REV8 — O-13 — SUPPORT BASIS POST-SIGNATURE RECHECK

**Verdetto:** `POST_SIGNATURE_CLEAN`

**Metodo:** recheck Codex mirato, read-only e single-lens sul target firmato.
Non viene presentato come review indipendente multi-agent.

## 1. Target verificato

```text
Ballot:
docs/REV8_O13_SUPPORT_BASIS_BALLOT_DRAFT.md

Commit firmato:
26f35e753f96ebc48d0453e432e8f2a3f5380687

SHA-256 firmato:
4021353cac2f2a1b469b1e889508a7296bbac8e9f656b2b15511261ea6c5fcc7

Target pre-firma dichiarato nel ballot:
commit 087336b62c5cb0d8412ec133991f7ec7d6cc2250
SHA-256 016ab3fc0f206813a9157dc4d92d60bb90aa5809642fbcfcd2e5a0436eaadcdd
```

Il file recuperato direttamente dal commit pre-firma produce esattamente lo
SHA dichiarato. Il commit firmato modifica un solo file: il ballot O13F_02.

## 2. Provenance ricontrollata

```text
MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745

O13F_01 implementation base
f277f091

Support-floor policy compiler base
d98fc446
```

Il testo firmato mantiene `REV8 SPEC GO = NO`, non modifica il candidate e
non attiva alcun dispatcher.

## 3. Firma

```text
Decisione O13F_02: APPROVO
Firma/nome:          Marco
Data:                2026-08-09
```

La firma lega correttamente commit e SHA pre-firma. Non contiene campi vuoti,
`APPROVO CON MODIFICHE` o un riferimento a uno snapshot differente.

## 4. Recheck sostanziale

| Controllo | Esito |
|---|---|
| nessun floor numerico modificato | CLEAN |
| `support_basis` limitato a due letterali | CLEAN |
| root obbligatorio `eligible_and_defined` | CLEAN |
| `eligible_only` limitato a righe corpus/GT firmate | CLEAN |
| `defined_only` escluso come ridondante | CLEAN |
| `G_defined` resta sottoinsieme di `G_eligible` | CLEAN |
| doppio controllo metrico eligible + defined preservato | CLEAN |
| precondizioni corpus non promosse a floor defined | CLEAN |
| supporto interno Spearman `n>=10` non convertito in dieci gruppi | CLEAN |
| source-family 5 e ceiling 1/2 restano vincoli eligible | CLEAN |
| campi di esito inattivi materializzati come `null` | CLEAN |
| `null` non equivale a PASS, false o zero | CLEAN |
| power binding applicato soltanto ai lati attivi | CLEAN |
| policy/template/registry devono concordare sul basis | CLEAN |
| schema v2 senza fallback | CLEAN |
| REV7 e dispatcher invariati | CLEAN |

## 5. Chiarimento Spearman verificato

La authority R23C stabilisce, per ogni
`(group_id,record_family,problem_type)`, che rho è pubblicabile soltanto se il
numero totale di coppie matched è `n>=10`, con varianze non nulle, rho
singleton e certificato disponibile. Questo criterio determina
l'appartenenza del gruppo a `G_defined`.

Il floor O-13 resta espresso in `group_id`. Non esiste nel ballot un nuovo
floor Spearman di dieci gruppi.

## 6. Stop-rule

Non sono emersi:

- bypass del supporto defined per una metrica obbligatoria;
- inferenza post-hoc del basis dai conteggi;
- nuovi metric ID, strata, `n_power` o membership;
- promozione di N/A a valore numerico;
- attivazione REV8, runtime, training o push.

Il recheck certifica il testo firmato, non l'implementazione futura.

## 7. Disposizione

```text
O13F_02 authority             = APPROVATA, FIRMATA, RECHECK CLEAN
O13F_02 utilizzabile          = SI, per implementazione candidate-only
Policy schema v3             = AUTORIZZATO, NON ANCORA IMPLEMENTATO
Metric/stratum registry      = NON ANCORA CONGELATO
REV7 dispatcher              = INVARIATO
O-13 close                   = NO
REV8 SPEC GO                 = NO
G1c close / G1 PASS          = NO
Runtime / training / push    = NON AUTORIZZATI
```

## 8. Prossima azione consentita

Implementare candidate-only:

1. policy schema v3 con `support_basis` exact-key;
2. guardia root obbligatorio e allowlist derivata dal registry/template;
3. risultati tri-state senza coercizione di `null`;
4. compiler, evaluator e digest binding coerenti;
5. fixture e mutation-oriented tests del ballot;
6. suite completa e counter-check del commit immutabile.

La chiusura di questa lista non costituisce automaticamente O-13 close,
materializzazione delle policy ufficiali o `REV8 SPEC GO`.
