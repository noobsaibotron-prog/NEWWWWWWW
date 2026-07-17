# Baseline Round5 (hybrid-core, 2026-07-13) — riferimento STORICO

Candidati + provenance + eval dei 3 seed (42/1337/2026). Il candidato spedito
nell'EXP e' `candidate_s42.json` (sha 3e1fd372...).

**PROVENANCE LEGACY INCOMPLETA** (A4b 0c): NON registra il weight_decay
effettivo (= default AdamW 1e-2, train.py:244 senza parametro), l'hash del
manifest, gli hash delle ricette di iniezione, ne' le versioni ambiente.
Mask = [3] (solo Sibilance). Dataset seed = 42 fisso.

Le cache esatte e TUTTI gli artefatti /tmp/aieq_v2 dell'epoca sono congelati in
`~/aieq_data/baselines/tmp_aieq_v2_freeze_20260717/` (fuori repo, ~120MB).

Uso nel programma A4b (piano 2026-07-17):
- questo e' il **Round5 LEGACY REPLAY** target: riprodurlo richiede mask=[3],
  wd=1e-2 esplicito, dataset seed 42, le cache congelate qui sopra.
- le ABLAZIONI A4b si confrontano invece con **A4b CONTROL** (mask=[1,3],
  pipeline nuova con fingerprint 0a + loader 0b, nessun intervento) — NON con
  questo baseline.
