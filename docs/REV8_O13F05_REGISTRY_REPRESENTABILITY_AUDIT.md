# REV8 — O-13 — REGISTRY REPRESENTABILITY AUDIT (O13F_05)

## Verdetto

```text
Verdetto                               = BLOCK_BEFORE_REGISTRY_MATERIALIZATION
Data                                   = 2026-08-10
Branch                                 = feature/motore-v3-rev8-spec-go
Base HEAD                              = 1275dbbd29a232dadad1729fa9e5605fba2402ce
O13F_04 authority                      = SIGNED + POST_SIGNATURE_CLEAN
Static registry definition             = NON MATERIALIZZATA
Official split registry instance       = NON MATERIALIZZABILE OGGI
Companion schema / conformance package = NON MATERIALIZZATI
Registry implementation                = NON AUTORIZZATA
REV8 SPEC GO                           = NO
```

## 1. Scopo

Questo audit verifica se l'authority firmata O13F_01–04 consente di
materializzare senza nuove scelte scientifiche:

```text
selector companion schema
precondition companion schema
metric catalog
support/precondition blueprint
conformance registry instance
```

La verifica ha confrontato integralmente:

```text
docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md
docs/REV8_O13_METRIC_STRATUM_REGISTRY_BALLOT_DRAFT.md
docs/REV8_O13_REGISTRY_DEFINITION_INSTANCE_SPLIT_BALLOT_DRAFT.md
```

con le strutture G1a già presenti. Nessun file di codice, schema, fixture,
candidate, dispatcher o authority firmata è stato modificato.

## 2. Finding A — readiness del fit calibration senza output legale

Il candidate §10.3 stabilisce:

```text
i calibratori si fittano soltanto su calibration
ECE, Brier e Average Precision si pubblicano soltanto su
development-metric e final-test
```

Il candidate §10.4 impone inoltre floor specifici da verificare prima del fit:

- supporto tonal actionable/clean e direzioni positive/negative;
- copertura dei sette profili;
- supporto positivo/negativo per Resonance, Harshness e Sibilance;
- source-family minima e ceiling sui positivi anomaly;
- copertura negativa per profilo.

O13F_03 lega invece ogni precondizione a un binding
`(metric_id,split_role)` nello stesso split e vieta output inventati. Non
esiste alcun `metric_id` pubblicabile su `calibration` cui legare questi
vincoli.

Sono state falsificate le alternative seguenti:

```text
ECE/Brier/AP su calibration
  -> viola §10.3: non sono output pubblicati su calibration

binding calibration verso una metrica development/final
  -> viola il same-split binding O13F_03

nuova metrica "calibration readiness"
  -> inventa un output scientifico non presente nel candidate

precondizione senza subject
  -> non lega deterministicamente la conseguenza al calibratore interessato
```

Conclusione: serve un subject non-metrico di fit/readiness. Il subject produce
un esito di ammissibilità, non una misura scientifica.

## 3. Finding B — `electronic_subgenre` non è `source_family`

Il candidate §9.1 contiene due campi distinti:

```text
source_family
electronic_subgenre
```

Il candidate §11.1 richiede la vista `electronic-stratified` per:

```text
techno
house
breakbeat
ogni altro sottogenere dichiarato
```

con le metriche della famiglia madre riportate per sottogenere. O13F_03 e
O13F_04 enumerano `source_family_groups` e
`for_each_source_family_in_parent`, ma non una popolazione o expansion mode
per sottogenere elettronico.

Riutilizzare `source_family` è semanticamente falso e può aggregare o separare
gruppi in modo diverso dal manifest. Hardcodare soltanto tre sottogeneri viola
la clausola «altri sottogeneri dichiarati». Lasciare il valore in testo libero
senza identità canonica rende byte-divergenti gli stessi breakdown.

Conclusione: serve una population registry-only distinta e un'espansione
completa sui sottogeneri osservati nel parent elettronico.

## 4. Finding C — report per sottogenere non equivale a claim

Il candidate richiede che le metriche siano riportate per sottogenere, ma
vieta ogni claim di sottogenere senza supporto determinato dalla power
analysis.

Creare automaticamente un nuovo `metric_id` normativo per ogni valore
data-dependent di `electronic_subgenre` produrrebbe un namespace e una Holm
family dipendenti dal corpus. Riutilizzare il `metric_id` parent come se il
sottogenere avesse la stessa policy permetterebbe support inheritance, vietata
da O13F_03.

La rappresentazione sicura è un breakdown diagnostico hash-bound:

```text
base metric identity invariata
concrete electronic-subgenre selector esplicito
support accounting proprio
nessun gate
nessuna claim
nessun power binding
```

Una futura claim richiede prima delle prediction un claim plan firmato, un
scope e un `metric_id` distinti, oltre al relativo power binding.

## 5. Finding D — il binding di potenza non è ancora scegliibile

O13F_03 congela i tipi:

```text
null
gate
family
```

ma lascia esplicitamente non decisi:

```text
lista finale dei metric_id
gate primari nella procedura Holm
riga esatta metric x split x stratum
```

Il candidate §11.2 richiede che numero e ordine dei gate primari siano
congelati prima del calcolo di potenza. Non autorizza il builder a classificare
silenziosamente tutte le metriche come gate, tutte come family-bound o alcune
come diagnostiche.

Perciò una registry definition che includesse già binding e primary-gate set
costringerebbe il builder a prendere decisioni scientifiche ancora aperte.
Rinviare queste scelte a un power plan osservato sarebbe invece troppo tardi e
potenzialmente data-dependent.

Conclusione: serve un claim-plan overlay statico, firmato dopo il catalogo ma
prima del pilot e delle prediction. Il power plan reale ne consuma gli ID; non
li crea.

## 6. Inventario semantico di controllo

La lettura letterale delle §§10.1–10.5 produce, prima dei breakdown elettronici:

```text
29 misure semantiche
164 metric definition candidate dopo espansione per scope,
record_family e problem_type
```

L'interpretazione usata dall'audit è:

- kernel obbligatorio §10.2 su `semantic_region` per tutti gli otto tipi;
- kernel obbligatorio applicabile su `dynamic_event` per Resonance,
  Harshness e Sibilance;
- durata e occupancy come diagnostici distinti;
- ECE/Brier tonali globali e anomaly class-specific sulle superfici dense,
  senza attribuire loro `record_family=dynamic_event`;
- nessun output metric binding sullo split `calibration`;
- `development-metric` e `final-test` come split di pubblicazione.

Il conteggio 164 è un checksum semantico per la futura matrice, non assegna
ancora spelling o ID autoritativi. Ogni scostamento deve essere spiegato
riga-per-riga nel freeze del catalogo.

## 7. Conseguenza operativa

Le regole scientifiche O13F_04 restano authority, ma i suoi schema artifact
v1, mai materializzati, devono essere superseded-before-materialization. Il
package autorizzato dal suo §7 non può essere materializzato correttamente
senza un amendment firmato che definisca:

1. subject non-metrici di calibration fit;
2. breakdown elettronici registry-only;
3. popolazione ed espansione di `electronic_subgenre`;
4. separazione fra registry definition e claim-plan overlay;
5. conseguenze fail-closed e digest binding.

Non è richiesta una modifica del candidate e non è consentita una revisione di
`support-floor-policy-3`. La supersessione riguarda esclusivamente gli
exact-key schema O13F_04 v1 non materializzati, non le sue guardie contro
fixture, fallback, digest auto-dichiarati o input reali mancanti.

## 8. Next permitted action

```text
Preparare O13F_05 docs-only
  -> counter-check pre-firma
  -> freeze immutabile
  -> firma esplicita Marco
  -> post-signature recheck

Solo dopo:
  companion schema + catalog/definition + claim-plan conformance

Restano vietati:
  validator code
  official instance
  official power plan
  prediction/runtime/training
  dispatcher activation
  push
```
