# REV8 — GATE 5 — POST-SIGNATURE RECHECK E ERRATUM PROCEDURALE

## Verdetto

```text
Verdetto                       = POST_SIGNATURE_CLEAN_WITH_PROCEDURAL_ERRATUM
Tipo                           = erratum append-only + recheck retroattivo
Data                           = 2026-08-12
Branch                         = feature/motore-v3-rev8-spec-go
Ballot oggetto                 = docs/REV8_GATE5_GAIN_DOMAIN_MICRO_AMEND_BALLOT.md
Signature commit               = c400ac25407cf1d4112b78d9c9f953fcaf23cd75
Modifica ballot firmati        = NO
Nuova decisione scientifica    = NO
Nuova firma attribuita         = NO
REV8 SPEC GO                   = NO
G1 PASS                        = NO
```

Questo documento fa due cose distinte e non le confonde: registra
un'**anomalia procedurale** nella firma del ballot Gate 5, e fornisce il
**post-signature recheck** che quel ballot non ha mai avuto. Non modifica i
byte, i digest, la decisione o lo scope del ballot firmato.

## 1. Finding — l'anomalia procedurale

Il ballot Gate 5 è l'unico della catena REV8 recente a divergere dalla
disciplina freeze → firma → recheck. Rilevato per ispezione di `git`, non
per lettura del documento:

```text
commit c400ac25  1 file changed, 243 insertions(+), 0 deletions(-)
```

Il file è stato **creato e firmato nello stesso commit**. Ne conseguono tre
assenze:

```text
commit di freeze pre-firma      = NON ESISTE
countercheck pre-firma          = NON ESISTE
post-signature recheck          = NON ESISTE (prima di questo documento)
```

Per confronto, i quattro ballot O13F_03…O13F_06 hanno tutti e tre gli stadi,
e i loro pin SHA sono stati ricalcolati e verificati combacianti.

Il blocco firma del ballot dichiara inoltre come base dell'autorizzazione:

```text
approvazione esplicita del piano "chiusura frontend G1 e probe semantico V3"
nel task Codex corrente, inclusa la scelta "dominio eleggibile".
```

Non è una firma apposta su un documento congelato di cui verificare il
digest: è l'approvazione di un piano data in conversazione, poi trascritta
in blocco firma. È una forma di autorizzazione diversa da quella usata per
ogni altro ballot della catena.

## 2. Ciò che questo erratum NON può fare

Un erratum registra fatti; non ne fabbrica. In particolare:

```text
lo SHA-256 pre-firma del ballot Gate 5 NON ESISTE e non è ricostruibile
```

Nessun commit contiene una versione non firmata di quel documento, quindi
non c'è alcun digest pre-firma da pinnare, né ora né in futuro. Questo
documento **non crea retroattivamente un freeze** e non deve essere letto
come se lo facesse. Il countercheck pre-firma resta mancante in modo
definitivo: ciò che segue è una verifica *a posteriori*, non un sostituto
di quella che avrebbe dovuto precedere la firma.

Il ballot firmato non viene riscritto: cambiarne i byte invaliderebbe il
digest e la traccia storica. La correzione è append-only.

## 3. Conferma dell'autorità

```text
Data conferma            = 2026-08-12
Confermato da            = Marco
Oggetto della conferma   = approvazione del piano "chiusura frontend G1 e
                           probe semantico V3", inclusa la scelta del
                           dominio eleggibile del Gate 5
Natura                   = conferma RETROATTIVA di un'approvazione data
                           in conversazione il 2026-08-11
```

La conferma è stata richiesta esplicitamente in sede di counter-check, dopo
che l'anomalia procedurale era stata rilevata e presentata insieme al testo
integrale del ballot. Non costituisce una nuova decisione scientifica né una
nuova firma: attesta che la decisione registrata il 2026-08-11 corrisponde
alla volontà dell'autorità.

Conseguenza: l'harness Gate 5-7 e il representation probe a valle sono
**autorizzati**.

## 4. Recheck retroattivo — verifica eseguita

Verifica per esecuzione su interprete canonico CPython 3.12.13,
numpy 2.5.1, torch 2.13.0.

### 4.1 Integrità del documento firmato

```text
SHA-256 ballot firmato:
1f371cd427d99a00a0abb0a6e2b63666dc19ca427b651f9bdf1b63a4cf56d705

Immutabile da c400ac25 a HEAD          = SI
Target candidate dichiarato:
398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745
Target candidate a HEAD                = INVARIATO
```

### 4.2 Sostanza della decisione

La decisione restringe il Gate 5 alle celle dove la gain invariance è
osservabile, escludendo quelle al clamp. Misurato:

```text
soglia del gate                        = 0.05 dB
errore massimo su griglia PIENA        = 12.0000 dB  (240x la soglia)
celle eleggibili mantenute             = 109/120 PSD/shape/delta, 70/120 prominence
verdetto Gate 5 / 6 / 7                = GREEN
```

Il massimo full-grid coincide **esattamente** con il ceiling del clamp
`+12.0 dB`: il fallimento non è un difetto del frontend ma la saturazione,
e su una cella satura una traslazione di gain non è osservabile. La
restrizione è quindi giustificata da un fenomeno misurato.

### 4.3 Il verde è guadagnato, non strutturale

Mutation test sul dominio e sui floor:

```text
dominio allargato a tutte le 120 celle -> Gate 5 RED
floor supporto PSD 108 -> 120          -> Gate 5 RED
floor prominence 64 -> 120             -> Gate 5 RED
```

Togliendo la restrizione il gate cade davvero, e i floor impediscono che il
dominio si assottigli fino a diventare vacuo passando comunque.

### 4.4 Copertura delle falsificazioni obbligatorie di §9

Dodici requisiti su dodici coperti da test eseguibili. La proprietà «rimuovere
celle non può migliorare il massimo» è stata verificata per mutazione:
`_max_cell` alterato per scartare silenziosamente la cella con errore massimo
— la manipolazione che §7 vieta — produce il fallimento di
`test_large_error_cannot_remove_an_eligible_cell`.

### 4.5 Perimetro

```text
run_frontend_conformance esportato da ml_v3.contracts   = NO
run_representation_probe esportato da ml_v3.contracts   = NO
ship-line 0-diff vs G0 2c88edad                         = SI
frontend / fixture G1 congelate modificate              = NO
```

## 5. Rimedio materializzato

L'anomalia procedurale aveva una conseguenza nel codice: l'harness era
legato alla propria autorità da **etichette, non da verifiche**. Il modulo
nominava il ballot senza verificarne il digest, e le costanti che
implementano valori normativi firmati non erano pinnate da alcun test. Il
caso limite: con un gain variant a `0 dB` la variante coincide con la
reference, ogni confronto è soddisfatto e il Gate 5 riporta GREEN senza
provare alcuna invarianza.

```text
commit rimedio:
cade91b1b04d8e1328d9500dc25cebe849ace426
test(g1b): pin Gate 5 harness to its signed authority
```

Tre test aggiunti, mutation test 5/5 uccise:

```text
gain variants -> (0.0,)                 UCCISA
floor PSD 108 -> 107                    UCCISA
floor prominence 64 -> 63               UCCISA
attenuazione -1.0 -> -2.0               UCCISA
ballot firmato alterato di un byte      UCCISA
```

L'ultimo test dà al ballot Gate 5 la sola protezione di integrità che può
ancora ricevere: da questo commit in avanti il documento non può cambiare in
silenzio. Non sostituisce il freeze mancante — lo dichiara §2.

```text
suite completa = 735/735 PASS
```

## 6. Stato corrente corretto

```text
Gate 5 eligible-domain policy   = FIRMATA, CONFERMATA, RECHECK CLEAN
Freeze pre-firma                = ASSENTE E NON RICOSTRUIBILE
Countercheck pre-firma          = ASSENTE (definitivamente)
Integrità ballot                = PINNATA DA TEST (cade91b1)
Harness Gate 5-7                = AUTORIZZATO E VALIDATO (GREEN)
Representation probe            = AUTORIZZATO E VALIDATO (GREEN)
Costanti normative              = PINNATE AI VALORI FIRMATI
REV7 dispatcher                 = INVARIATO
REV8 SPEC GO                    = NO
G1 PASS                         = NO
Training / plugin / runtime     = NON AUTORIZZATI
push                            = NON AUTORIZZATO
```

## 7. Regola per il futuro

Un ballot che crea e firma nello stesso commit non è verificabile per
digest, perché non esiste uno stato pre-firma da confrontare. La disciplina
freeze → countercheck → firma → recheck non è burocrazia: è ciò che rende
possibile il controllo indipendente. Dove salta, il massimo ottenibile è una
verifica a posteriori della sostanza — utile, ma strutturalmente più debole,
perché non può più distinguere ciò che l'autorità ha approvato da ciò che è
stato scritto dopo.

## 8. Counter-check richiesto su questo documento

Prima di considerarlo acquisito, verificare:

1. che il ballot Gate 5 sia byte-invariato e il suo SHA-256 sia quello citato;
2. che questo documento non attribuisca alcuna nuova firma;
3. che non introduca decisioni scientifiche, soglie, reason code o policy;
4. che i commit citati esistano e abbiano il contenuto dichiarato;
5. che la suite sia verde e la ship-line a diff zero;
6. che nessun simbolo lab risulti esportato da `ml_v3.contracts`.
