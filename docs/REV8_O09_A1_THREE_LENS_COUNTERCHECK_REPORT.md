# REV8 O-09 A1 — Three-lens targeted counter-check

**Verdetto contenutistico:** `CLEAN`

**Verdetto operativo:** `BLOCK — S6→R23C TRANSFER NON ANCORA EFFICACE`

**REV8 SPEC GO:** `NO`

## 1. Snapshot verificato

```text
A1 narrow ballot:
docs/REV8_O09_ACTIVATION_BALLOT_A1_DRAFT.md
SHA-256 ffe2eca914279e71a51b9e101d1d36234355ec991ae9afb0c19ae4005b2b6c78

S6 R23C transfer draft:
docs/REV8_S6_R23C_AUTHORITY_TRANSFER_BALLOT_DRAFT.md
SHA-256 321c0bb0d6cf770d3d612b362bba4427b06f17a36d7913a6d2fedd8856b3f491
commit bdb51ae4801be5c0a3a135c5b95194ed600f2a57

S6 historical targeted report:
docs/REV8_S6_TARGETED_COUNTERCHECK_REPORT.md
SHA-256 88d24d1522e3bfb0a012b19b1b8edf2ff4087ff9e3534c82aa21aa946acd2570

A1 permanent tests:
ml_v3/tests/test_g1c_rev8_o09_candidate.py
SHA-256 4d43c15d20a169786d78f6d51057432989b22adbd5468df0662f9ad028259970
evidence freeze commit 9a16692bfee32396213f7487a80ef94f99f50076
```

Authority R23C invocata e verificata:

```text
target RC SHA-256
684d8fd7ebfff4a9924b1105771acb9eb92d2bb9f97fd8295a60df77bfb9f0cb

ballot R23C firmato SHA-256
4c21b552883b67e4e61b3b03d94a8e041ec00aebec858f968c572b302d9f0b99

report R23C post-firma CLEAN SHA-256
e2223fa357d76b162508ba724ac36a529a9c5a1dc57dd894d5c39689ab070157
```

## 2. Metodo e indipendenza

Tre agenti di controverifica separati hanno lavorato in sola lettura sullo
stesso snapshot, con lenti diverse. Nessuno dei tre ha modificato file:

1. **metriche/fail-closed:** formula pubblicata, reason code, fixture,
   conteggi e assenza di overclaim;
2. **optimizer/aritmetica:** equivalenza fra §2.2 e probe, authority chain,
   bound `K1`/`L(Z)` e pin crittografici;
3. **semantica/governance:** transizioni di firma, scope, stato S6 storico vs
   corrente e claim sul ledger.

È indipendenza di campionamento e di lente dentro lo stesso workflow, non una
certificazione esterna organizzativa.

## 3. Finding emersi e risolti prima del freeze

| Finding | Lente | Correzione | Stato |
|---|---|---|---|
| target S6 storico confuso con authority R23C corrente | optimizer | introdotto transfer ballot separato; target RC immutato | CLOSED |
| formula di bit-length non interamente materializzata nel ballot | optimizer | aggiunta §2.2 esatta e test di equivalenza indipendente | CLOSED |
| pubblicazione tick→ms descritta con `K` invece di `K1` | optimizer | `sum/(K1*48)`; `L(Z)*48` soltanto bound conservativo | CLOSED |
| cap group-level contati 16 anziché 17 | metriche | corretto inventario 7 AP + 5 Coverage + 5 Spearman | CLOSED |
| enforcement candidate descritto come già attivo | metriche | distinto obbligo normativo futuro da probe evidence-only | CLOSED |
| transizioni di firma/deroghe ambigue | semantica | firma fail-closed; modifiche richiedono nuovo snapshot e nuova firma | CLOSED |
| conteggio stale «altre 28 righe» | semantica | rimosso il conteggio; restante ledger dichiarato non verificato | CLOSED |
| counter-check descritto come non eseguito | tutte | completate e materializzate le tre lenti; rimossa deroga di processo | CLOSED |

## 4. Risultati delle tre lenti

### 4.1 Metriche / fail-closed

`CLEAN` sul contenuto. Formula, metriche, reason code, scope, fixture e
transizioni sono coerenti. `540/540 PASS` è riportato correttamente. Nessun
cap group-level, Spearman generale, enforcement candidate o SPEC GO viene
dichiarato attivo.

### 4.2 Optimizer / aritmetica

`CLEAN` sul disegno tecnico. La formula §2.2 coincide con
`provisional_preflight_probe`; un confronto indipendente deterministico e i
test permanenti verificano l'equivalenza. La pubblicazione usa `K1`; il bound
usa correttamente `L(Z)` poiché `K1 <= L(Z)`. Authority e hash sono coerenti.

### 4.3 Semantica / governance

`CLEAN` dopo la rimozione del conteggio ledger stale. Le transizioni sono
fail-closed: il transfer non firmato non ha autorità; la firma del transfer da
sola non basta; serve il suo recheck post-firma `CLEAN`. A1 resta non firmabile
fino a quel momento. Nessun GO implicito.

## 5. Test riprodotti

Interprete canonico:

```text
/Users/marco/aieq_data/motore_v3/env/venv/bin/python
CPython 3.12.13
PYTHONDONTWRITEBYTECODE=1
```

Mirati:

```text
PreflightTests + SpearmanTests + RuntimeFailureTests
22 eseguiti
22 PASS
exit code 0
```

Suite completa:

```text
python -m unittest discover -s ml_v3/tests -p 'test_*.py' -q
540 eseguiti
540 PASS
exit code 0
```

## 6. Limite e prossimo gate

Il `CLEAN` contenutistico non rende A1 firmabile. La sequenza obbligatoria è:

```text
firma del transfer S6→R23C
→ freeze SHA del ballot firmato
→ recheck post-firma CLEAN
→ composizione R23C + transfer efficace
→ retarget/freeze A1 sugli SHA finali
→ solo allora eventuale firma A1
```

Un recheck `BLOCK` del transfer lascia S6 assente dalla authority corrente e
mantiene A1 `NOT SIGNABLE`. I 17 cap group-level, enforcement candidate,
runtime, training e `REV8 SPEC GO` restano fuori scope e non autorizzati.
