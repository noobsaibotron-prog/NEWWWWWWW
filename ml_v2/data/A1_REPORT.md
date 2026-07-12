# A1 — Corpus v2: audit report

Files: **4392** · durata totale: **16.35 h** · root: `/Users/marco/aieq_data`

## Domini × split (n file / minuti)

| domain | split | n | min |
|---|---|---|---|
| clean_bass | heldout | 19 | 14.2 |
| clean_bass | test | 17 | 8.3 |
| clean_bass | train | 73 | 69.9 |
| clean_drums | heldout | 27 | 14.8 |
| clean_drums | test | 19 | 7.4 |
| clean_drums | train | 124 | 82.7 |
| clean_mix | heldout | 23 | 15.5 |
| clean_mix | test | 15 | 6.6 |
| clean_mix | train | 72 | 81.3 |
| clean_synth | heldout | 24 | 17.3 |
| clean_synth | test | 21 | 9.8 |
| clean_synth | train | 125 | 90.7 |
| hf_negative | heldout | 35 | 6.0 |
| hf_negative | test | 38 | 4.4 |
| hf_negative | train | 147 | 24.4 |
| vocal | heldout | 365 | 51.1 |
| vocal | test | 724 | 95.8 |
| vocal | train | 2524 | 380.7 |

## Licenze

| license | n | commercial-ok |
|---|---|---|
| CC-BY | 357 | sì |
| CC-BY-4.0 | 3692 | sì |
| CC0 | 343 | sì |

Cross-check upstream (freesound per-id): **700 commercial-ok** (di cui 343 exact-match stringa/versione e 357 commercial-ok con versione diversa — es. manifest CC-BY vs upstream CC-BY-3.0), **0 non-commercial/violazioni**. I 79 file BabySlakh non sono su freesound: licenza CC-BY-4.0 dalla fonte primaria (Slakh/Zenodo), nessun cross-check per-id applicabile.

## Sample rate

| sr | n | nota |
|---|---|---|
| 16000 | 79 | **HF morto sopra Nyquist/2 — vedi design A4** |
| 44100 | 4313 |  |

## Anti-leakage

- Duplicati sha256 CROSS-split: **0** ✅
- Duplicati sha256 same-split (warning): 3
    - 4554c7002a5b…: real_audio/vocalset_extracted/FULL/female2/scales/slow_piano/f2_scales_f_slow_piano_u(1).wav | real_audio/vocalset_extracted/FULL/female2/scales/slow_piano/f2_scales_f_slow_piano_u.wav
    - 3ea60e668c22…: real_audio/vocalset_extracted/FULL/female2/scales/straight/f2_scales_straight_u(1).wav | real_audio/vocalset_extracted/FULL/female2/scales/straight/f2_scales_straight_u.wav
    - a261df2163a5…: real_audio/vocalset_extracted/FULL/female2/scales/vibrato/f2_scales_vibrato_a(1).wav | real_audio/vocalset_extracted/FULL/female2/scales/vibrato/f2_scales_vibrato_a.wav
- Clip giudice (8 wav in `AIEQ_Ableton_Test_Clips`) + vocal holdout (6 `test_voce_*.wav`) dentro il corpus: **4** ← FAIL

## Split policy (deterministica, zero RNG)

- vocal: singer-disjoint. test = ('female9', 'female8', 'male11', 'male10') (INVARIATO dal lab M6-M9), heldout-calibrazione = ('female7', 'male9'), train = restanti.
- tier2: group-disjoint (stems+mix stesso track = stesso gruppo), sha1(group) mod 100 → <70 train, <85 heldout, resto test.

## ⚠️ VIOLAZIONI

- JUDGE/HOLDOUT CLIP inside corpus (leakage): /Users/marco/Desktop/test_voce_pulita_femmina 2.wav
- JUDGE/HOLDOUT CLIP inside corpus (leakage): /Users/marco/Desktop/test_voce_pulita_femmina.wav
- JUDGE/HOLDOUT CLIP inside corpus (leakage): /Users/marco/Desktop/test_voce_pulita_maschio 2.wav
- JUDGE/HOLDOUT CLIP inside corpus (leakage): /Users/marco/Desktop/test_voce_pulita_maschio.wav

**ESITO: ROSSO** — risolvere prima di A4.
