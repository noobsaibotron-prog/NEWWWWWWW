# Ember Core / Motore v3 — Handoff AUTOSUFFICIENTE per agenti esterni

**Ultimo aggiornamento:** 2026-07-26 21:10 (UTC+2)  
**Destinatario:** agente esterno **senza terminale / senza git**.  
**Questo file** = quadro + testo completo snapshot.

> Se ricevi solo questo markdown, hai tutto il necessario. **Non eseguire comandi.**  
> WAV binari non dumpati: digests in SHA256SUMS + inventory.  
> JSON evidence pesante della spike (`G1B_*` runner output) vive sul branch `spike/motore-v3-g1b-frontend` — qui i report docs-only.

---

## A. Quadro in chiaro

### Cos’è
- Prodotto: **AI Equalizer Pro**. **Ember Core** = Motore / misura / AI.
- Lane: lab **Motore v3 offline** (`feature/motore-v3-offline`). Spike G1b su branch/worktree separato.

### Fotografia congelata (REV7 candidate; Guardian 2nd GO on `5e0d32fc`; hygiene)
```text
G1a CLOSE                         GO
REV6                              authority corrente (freeze 6d254d0a)
gate-4 G1 closing set             stationary only (multitone, pseudo_noise) on R
log_sweep                         hashed; report-only; does NOT close gate 4 in G1
non-stat SR-parity                named G1c/G1e requirement (parked input)
A3                                ARCHIVED: SOUND + FALSIFIED + RETIRED AS SOLUTION
SWEEP_METROLOGY_REDESIGN proposal PARKED — G1c/G1e input debt (~9b8f8305)
REV7 candidate                    PACKAGED (LF report-only + gate-4 scope C)
Guardian second GO                LANDED on tip 5e0d32fc (≠ reopen)
REV7 consolidate                  NO — awaits Marco-authorized consolidate
G1b official tip                  NON esiste
0.25 dB / max / R                 intoccati
stationary R measure              8cf38625 MEASURE-PASS max|Δ|=0.1915 (~23% headroom)
binding peak (closing cells)      level (broadband RMS / mid_level_dbfs) — watch
```

**Decisione Marco — option (C):** stop POROUS litigation on
`docs/MOTORE_V3_SWEEP_METROLOGY_REDESIGN_PROPOSAL.md`. Proposal **PARKED**.
Gate-4 closing set in G1 = stationary on geometric `R`. Non-stat → G1c/G1e.
**One** REV7 candidate packaged:
`docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` = LF report-only
(`71159469`) **+** gate-4 scope C (`31216df4`); evidence stationary measure
`8cf38625`. **Guardian second GO landed on `5e0d32fc`** — hygiene (honest
margin + level-peak debt) before Marco-authorized consolidate; **≠** reopen
GO. ≠ G1 PASS. ≠ CONTRACT/lock/SHA consolidate in hygiene commits.

### Tip
| Tip | Commit | Nota |
|-----|--------|------|
| HEAD (docs living) | *(hygiene after 2nd GO)* | honest margin + level-peak debt; ≠ consolidate |
| REV7 candidate package | **`5e0d32fc`** | Guardian 2nd GO tip; LF ∉R + scope C |
| Stationary R closing measure | `8cf38625` | max\|Δ\|=0.1915 dB PASS on R (~23% headroom; peak=`level`) |
| Gate-4 scope clause | **`31216df4`** | stationary close; park non-stat |
| Parked proposal tip | **`9b8f8305`** | SWEEP_METROLOGY_REDESIGN — G1c/G1e debt |
| LF report-only clause | **`71159469`** | joins scope clause in one REV7 |
| A3 archive stamp | sibling doc | dual redteam+CC; ACTIVE family closed |
| A3 falsification | **`78da84dd`** | FAIL honest — hypothesis falsified |
| Final R remeasure | `b3d7f71b` | stationary PASS; sweep FAIL (report-only path) |
| Code G1a | **`a2186ac1`** | F2/F3/F4; G1a CLOSE GO still holds |
| T6 / M2 | `501a4e00` / `e9916319` | digests unchanged |
| Freeze CONTRACT | `6d254d0a` | **REV7 consolidate: NO** until 2nd GO |

### Progresso da handoff 05:15 (25 Jul) → ora
1. G1b spike WS4 **RED** → REV7 candidate path opened.  
2. Product decision: **report-only LF** (gate on geometric `R`; ∉R published).  
3. Final re-measure `R`: stationary **PASS**; overall **FAIL** on `log_sweep` HF.  
4. A/A3 stamped → falsified → **RETIRED AS SOLUTION**.  
5. SWEEP_METROLOGY_REDESIGN drafted; POROUS litigation **stopped**.  
6. **Option (C)** — scope clause `31216df4`; proposal **PARKED**.  
7. Stationary R closing measure `8cf38625` — MEASURE-PASS (0.1915 dB; ~23% headroom; peak=`level`).  
8. **REV7 candidate packaged** (LF + scope C) @ `5e0d32fc` — Guardian second GO **landed**.  
9. Hygiene: honest margin + binding-`level` debt (this tip) — **≠** reopen GO.  
10. **≠** G1 PASS · **≠** consolidate · **≠** G1b tip · 0.25 / max / R intact.

### Stato fase
- **G1a CLOSE: GO**; code tip `a2186ac1`.  
- **A3: ARCHIVED**; ACTIVE closed. **No reopen.**  
- **SWEEP proposal: PARKED** — G1c/G1e input debt.  
- **REV7 candidate: PACKAGED** @ `5e0d32fc`; Guardian second GO **landed**.  
- **Consolidate: NO** until Marco-authorized.  
- Spike ≠ official G1b tip.  
- Debt: F1 WAV, gate-8, G4 cross-SR LF detections, **non-stat SR-parity**,
  **stationary binding peak `level`** (below).

### Debt — parity cross-SR non-stazionaria
Parity cross-SR non-stazionaria: non verificata a G1. Input parcheggiati:
mandato SWEEP_METROLOGY_REDESIGN + proposta S1/S2 (tip ~`9b8f8305`).
Da riprendere a G1c/G1e. Independent CC WS4 (spike `c7f05871`): accordo
~0.03 dB sulle bin con segnale 48k vs 44.1→48 — **evidence cite only**,
non criterio.

### Debt — stationary closing binding peak `level` (watch)
Su `8cf38625`, peak in tutte e quattro le celle di chiusura = broadband
**`level`** (MAIN-window RMS / `mid_level_dbfs`), non shape/psd per-banda.
Causa plausibile: bordo passabanda resampler ~20 kHz. Closest approach to
0.25 (0.1915 → ~23% headroom) — watch item; **not** FAIL today. **Distinct**
from parked non-stat / sweep debt. Incomplete scripts omitting
`mid_level_dbfs` (~0.06/0.02) are **not** authoritative.

### Catena tip (recente)
```text
(this)    docs(v3): debt level-peak watch; honest margin; fix 0.03 cite  ← HEAD
5e0d32fc  docs(v3): package REV7 candidate — LF report-only + gate-4 scope (C)  ← 2nd GO
8cf38625  docs(v3): stationary R closing measure under gate-4 scope (C)
31216df4  docs(v3): G1 gate-4 scope — stationary close; park non-stat (C)
9b8f8305  docs(v3): kill source_time_selected := mirror; fix RIDGE claim; P32
…
b3d7f71b  docs: final REV7 R remeasure (stat PASS, sweep FAIL)
71159469  docs(v3): REV7 report-only LF packaging
…
6d254d0a  freeze REV6
```

### Working tree (this branch)
| Path | Stato |
|------|--------|
| Motore docs | living @ hygiene after 2nd GO; freeze untouched |
| `ml_v3/frontend/` + spike test | untracked lab spike ≠ tip |
| agents / .cursor | untracked |

### Checklist
| Voce | Stato |
|------|--------|
| G1a CLOSE | **GO** |
| Option (C) / gate-4 scope | **DECIDED** @ `31216df4` |
| Stationary closing set on `R` | **MEASURE-PASS** @ `8cf38625` (max\|Δ\|=0.1915; ~23% headroom; peak=`level`) |
| REV7 candidate package | **PACKAGED** @ `5e0d32fc` — `MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` |
| Guardian second GO | **LANDED** on `5e0d32fc` |
| Sweep proposal | **PARKED** (G1c/G1e debt; no redteam now) |
| A3 | **ARCHIVED** SOUND+FALSIFIED+RETIRED |
| Consolidate / G1 PASS / G1b tip | **NO** (next = Marco OK → consolidate) |

### Sequenza restante
1. Scope clause + stationary measure — **done**.  
2. **One** REV7 candidate package — **done** @ `5e0d32fc`: LF (`71159469`) +
   scope C (`31216df4`); evidence `8cf38625`.  
3. **Guardian second GO** — **done** on `5e0d32fc`.  
4. Hygiene (honest margin + level-peak debt) — **this tip**.  
5. Marco-authorized **consolidate** → **rehash** lock/SHA256SUMS →
   **official G1b tip**.  
6. Non-stat / sweep remains **PARKED** until G1c/G1e.  
7. Mai: 0.25→media/p95; ship/Source; training; claim G1 PASS; riaprire A3;
   amend CONTRACT freeze without GO; self-GO; reopen POROUS on sweep.

### Vietato
G1 PASS; REV7 consolidate senza OK Marco; tip G1b ufficiale;
riaprire ACTIVE A3/A4; riprendere POROUS / redteam sulla sweep proposal ora;
toccare 0.25 / max / R / lock / SHA256SUMS in hygiene/packaging commits;
ship Ableton.

---

## B. Allegati — testo completo (snapshot)


---

## B.0 AUDIO INVENTORY (no binary; T6 @ 501a4e00; F1 debt)

**Count:** 37 WAV · **SHA256SUMS:** 48 lines  
**fixture_spec_sha256:** `513c3baf7aaed8eb1a15f7d2e875a3015479fc2ece0378e75cfceadefe68a6ef`  
**schema_registry_sha256:** `fa506142afd8a0794f2093a428841b445b130e2ee0f681e18609b1a9b9cd5247`

### Path + bytes

```text
path	bytes
ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_44100.wav	352844
ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_48000.wav	384044
ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_96000.wav	768044
ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_44100.wav	705644
ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_48000.wav	768044
ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_96000.wav	1536044
ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_44100.wav	705644
ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_48000.wav	768044
ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_96000.wav	1536044
ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_44100.wav	705644
ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_48000.wav	768044
ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_96000.wav	1536044
ml_v3/fixtures/g1/audio/log_sweep/log_sweep_44100.wav	352844
ml_v3/fixtures/g1/audio/log_sweep/log_sweep_48000.wav	384044
ml_v3/fixtures/g1/audio/log_sweep/log_sweep_96000.wav	768044
ml_v3/fixtures/g1/audio/multitone/multitone_44100.wav	352844
ml_v3/fixtures/g1/audio/multitone/multitone_48000.wav	384044
ml_v3/fixtures/g1/audio/multitone/multitone_96000.wav	768044
ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_44100.wav	352844
ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_48000.wav	384044
ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_96000.wav	768044
ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_44100.wav	352844
ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_48000.wav	384044
ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_96000.wav	768044
ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_44100.wav	352844
ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_48000.wav	384044
ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_96000.wav	768044
ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_44100.wav	352844
ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_48000.wav	384044
ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_96000.wav	768044
ml_v3/fixtures/g1/audio/silence_non_finite/silence_44100.wav	352844
ml_v3/fixtures/g1/audio/silence_non_finite/silence_48000.wav	384044
ml_v3/fixtures/g1/audio/silence_non_finite/silence_96000.wav	768044
ml_v3/fixtures/g1/audio/transient_burst/transient_burst_44100.wav	352844
ml_v3/fixtures/g1/audio/transient_burst/transient_burst_48000.wav	384044
ml_v3/fixtures/g1/audio/transient_burst/transient_burst_96000.wav	768044
ml_v3/fixtures/g1/audio/ultrasonic_96k/ultrasonic_96k_96000.wav	768044
```

### Audio digests

```text
d47df77eb87fc0832f33837f8eb7ab7dd8b565922f59c2d8c4e3222f5db51308  ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_44100.wav
4979893c707032977b4371d17cc0b8bfe0f056e02b4a6f5ba92b2fc4e640f90f  ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_48000.wav
53b7483f9f94ab84e882319a086dfabdaedf6a15ff37a32e05f332a67548f75b  ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_96000.wav
39dcb6f91a8d77f2a727a920b89e6556eb48f853187c1ca3ed4833decab09dcc  ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_44100.wav
6528c09b1029c18e6a2925b8934e68a0ad3fce4ea28e95153130730c42afaa7a  ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_48000.wav
17d744637b20ca4696ae42bb021442d731712cc56cfe99cf2e9025b35165ac5c  ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_96000.wav
569fc6ce7da6a27e675f232572f2912f42413a32c1373d714f29346778de9508  ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_44100.wav
238ab02df007f6b701dedbbca7aaddc7d4b6abb6179a085bca0ed6d1e02bf94d  ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_48000.wav
4adfb30d2362d1716e50622ab88cc50c37fcb7dba55b2f3ea6990524c39b21bd  ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_96000.wav
e927a39e1eba2b64e1b4351571b334234178895eb7def5843eeb8878aa1fb120  ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_44100.wav
a410a5029d31e85571f2f03463d169f42b9db4eb91f00b34c5b66305b046e176  ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_48000.wav
82f9c2ad35bcd9724f836716f3967dfce1c08136e79a177f9169d1a86ea915ec  ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_96000.wav
c4d41c0efad09e12effe057ed70a025ad93ef63e2b159f5e54f0dbb50f45a9ee  ml_v3/fixtures/g1/audio/log_sweep/log_sweep_44100.wav
0d75a68662ba888fe7414b2ba7477a3db807a56145ead4a41bcb0731bcd406ef  ml_v3/fixtures/g1/audio/log_sweep/log_sweep_48000.wav
5f308a218a6969e41f7a8a07b3891283be0fca07e431eb97bfa4747c6da78110  ml_v3/fixtures/g1/audio/log_sweep/log_sweep_96000.wav
d21fdff2bc743d1630de76e17da027a77e9c353ca91993f660c4dc1d0a544630  ml_v3/fixtures/g1/audio/multitone/multitone_44100.wav
45e1f61b987ad98d3fda84e3d35fa82e7b4da9ba88e5e6d98fb44fb0ae746fa4  ml_v3/fixtures/g1/audio/multitone/multitone_48000.wav
ac100d400ec75584e3de32ba82d4afe31c98df645011fbe0ce810c7984e636c3  ml_v3/fixtures/g1/audio/multitone/multitone_96000.wav
ef5b54e95015e7c85790f79e16b5a0f46c6fc0fab4bc91038da762749f6688d3  ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_44100.wav
c9d0bc88d1953c9615de15ff7c237987d222a188de2f2db3aff7fb84997ebe06  ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_48000.wav
6d8b66fb2c31364d1480259a2fbc838deea43e8fc9cd7f279c02d6bd643e3286  ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_96000.wav
87a0ad44b3656496ac6fdd040f3b16afb9f6862b5f8309a2db12fe900a4c0cc5  ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_44100.wav
69013347ab6e8e3920e0c3da0f84355fbeada4a3aa3acf50fe9348abac463e16  ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_48000.wav
116325057658f851b3cc8fc1933206f7a513e00877b6f821ff523d2a777261e1  ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_96000.wav
e49cf37cfd0df4a6579f70ab0b9f625a01ec64f42d5b666ca4e81b150d466255  ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_44100.wav
e745d9ff2bde64092cd1c307064aadbe9e6774badc0b49c9d11155aedf423812  ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_48000.wav
9cff79150692657a1497ca020f22152f5bbda105a0de4b2d14bba7241058881a  ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_96000.wav
49b2a58c553fa8b116de40492ccdc960f30c1d44d6a6c678dc388bb605b780f3  ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_44100.wav
d7042449b719d6360209a1e1ba54f637f2def0e4d03655c7d7fb9d787b2583c0  ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_48000.wav
c3b1ad0d53075e83c6f7b7d743081fcd057a5e45b10ad4ed7409d820f55d340f  ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_96000.wav
13e6d9a9b74be09f7e443cf8d4077a20fcfde6a0b0bd54a951f9d96d1de11a4c  ml_v3/fixtures/g1/audio/silence_non_finite/silence_44100.wav
2f80217ed947d5a99a7b1ddc177153e184066ddff495e2847a142c9d2ffc9711  ml_v3/fixtures/g1/audio/silence_non_finite/silence_48000.wav
780952a643ae5ffb05b652f2188b57ecb10a8c1b98327d3add9e4b5df36e508a  ml_v3/fixtures/g1/audio/silence_non_finite/silence_96000.wav
c984bf0fc462ffc5ed5ce91d3ec87cb326e58d1bfaee3a3e029c6675b42ef29e  ml_v3/fixtures/g1/audio/transient_burst/transient_burst_44100.wav
0db70f13cadda780ecefed3711c98c3980a0ab8e0bd0c3d81a3e4797da3d497b  ml_v3/fixtures/g1/audio/transient_burst/transient_burst_48000.wav
29d579c8dc46c32185af0c1851c680f877f314eed45f6f5d16ce8630e59e2805  ml_v3/fixtures/g1/audio/transient_burst/transient_burst_96000.wav
81e3f3824e843f47c6e44efb66e82d6161ee277df22118f2d49aaf8fa2a63429  ml_v3/fixtures/g1/audio/ultrasonic_96k/ultrasonic_96k_96000.wav
```

---

## B.1 FILE: `docs/MOTORE_V3_PLAN.md (@ HEAD 78da84dd)`

**Path logico:** `docs/MOTORE_V3_PLAN.md`  
**Bytes:** 23105  
**Lines:** 398

```markdown
# Motore v3 - Sviluppo a contratti per fase

## Stato e obiettivo

Questo e il piano canonico del laboratorio Motore v3. Congela obiettivo,
interfacce di prodotto, governance e criteri di successo; l'architettura ML
viene scelta soltanto dopo benchmark, baseline DSP e corpus controfirmati.

### Stato fattuale (aggiornato 2026-07-25) - leggere PRIMA del resto

Il piano descrive un progetto a contratti. Ad oggi, su questo branch:

- **G0: PASS**, freeze RIPRODUCIBILE della baseline NEGATIVA. Non promuove
  alcun modello.
- **G1 contratto**: freeze document-only `6d254d0a` (REV6 consolidata +
  micro-amend). Header/§16 di quel commit restano storicamente "non e GO";
  lo **stato di fase vivente** e nelle righe Governance sotto.
- **Tip accuracy**: codice G1a remediation tip = `a2186ac1` (F2/F3/F4
  closed in code). Trust chain T6 tip = `501a4e00`; remediation tip is
  forward of T6. PLAN pre-stamp (gate-8 debt) = `805fb34d`; history:
  false CLOSE `1746a058` withdrawn; REOPEN `284228d6`. Non confondere tip
  codice remediation, tip T6, e tip PLAN. **Stato corrente: G1a CLOSE: GO**
  (questo stamp) — REOPENED **non** e piu lo stato vivente.
- **GO G1a**: aperto (2026-07-25) dal reviewer (Marco) sul freeze `6d254d0a`.
  Apertura fase ≠ CLOSE; CLOSE ora = GO sotto. **REV7 consolidate: NO.**
  Candidate draft open: `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`
  (activity/admission gate 4 only; 0.25 dB immutable; ≠ freeze amend until
  second Guardian GO + lock re-hash).
- **G1a codice in git** (remediation tip `a2186ac1`): T1 `94dc9991`
  (primitives/split/coverage) + T2 `1908fc45` / T2.1 `918b3dde` (JSON
  schemas, validators, golden canonical) + T3 `9aa19295` (adapter v2↔v3
  hashed, `adapter_mapping_sha256` =
  `6a978c01bcccb85fb7db17ae3c66ee55ebceee82f47dca792e5a2f3a5fb9828f`) +
  T4 `3bfd8aaf` (metrology lock §13 committed;
  `metrology_lock_sha256` =
  `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`) +
  T5 `75cb6902` (SHA256SUMS trust anchor + path hygiene) +
  M2 `e9916319` (fixture-spec v1;
  `fixture_spec_sha256` =
  `513c3baf7aaed8eb1a15f7d2e875a3015479fc2ece0378e75cfceadefe68a6ef`) +
  hygiene `fe8b97d3` (SHA256SUMS path-order canonicalize after M2) +
  T6 `501a4e00` (fixture signal generators + WAV inventory + hashes;
  SHA256SUMS does **not** self-hash — trust anchor = commit chain) +
  remediation `a2186ac1` (F2/F3/F4 closed in code).
- **G1a T5**: committed at `75cb6902`. SHA256SUMS is the trust anchor for
  artifact digests beyond lock inline `dependencies`; it does not hash
  itself (anchor = commit `75cb6902`, then extended by M2/hygiene/T6).
  **M1 T5 catch-up CC: CLEAN** (2026-07-25) on `75cb6902`. Hygiene
  findings that are not semantic BLOCKERs → durable debt below; **do not
  reopen T5 hash**.
- **G1a T6**: committed at `501a4e00` (generators + WAV inventory). M2
  fixture-spec remains `e9916319` with digest `513c3baf…` (unchanged).
  Path-order hygiene `fe8b97d3` remains relevant on the SHA256SUMS chain.
- **G1a CLOSE: GO** (Guardian re-CLOSE 2026-07-25) on remediation tip
  `a2186ac1` + PLAN pre-stamp `805fb34d`; contract freeze `6d254d0a`
  invariato; lock
  `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`;
  SHA256SUMS 48 verify OK; F2/F3/F4 closed; gate-8 + F1 WAV remain durable
  debt (no rewrite). **No G1 PASS.** **REV7 consolidate: NO** (candidate
  draft only — see `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` after
  spike WS4 RED). History: false CLOSE `1746a058` withdrawn; REOPEN
  `284228d6`. **Product G1b may unfreeze after this stamp** (spike
  `ml_v3/frontend/` ≠ tip).
  - **F2 (code-closed @ `a2186ac1`)**: commitment↔reveal match on
    non-null reveal via `verify_commitment`; golden dd/ee REJECT when
    mismatch. **Residual debt** (not F2 reopen): §8.2.4 gate 8 premature
    reveal / salt retry **not** fully modeled in §8-minimo single-record
    validate — deferred to §8-pieno/ledger.
  - **F3 (code-closed @ `a2186ac1`)**: schema surface binding to
    `contracts/schemas.py` (not only fixture JSON copies).
  - **F4 (evidence-closed on canonical venv)**: Re-CLOSE evidence used
    `~/aieq_data/motore_v3/env/venv` (CPython **3.12.13**); lock pinna
    3.12.13. Evidence on tip: compileall + unittest + SHA256SUMS +
    metrology lock digest on that interpreter.
  - **MED F1 (durable debt)**: 37 WAV ~22MB in-repo vs CONTRACT §2 — **no
    history rewrite**; forward WAV remediation tranche (not a CLOSE blocker;
    debt/decision).
  - **G4 debt (REV7 report-only LF)**: cross-SR detection stability on
    low-end classes (mud/boom/boxy) — not satisfied by G1 gate 4 PASS on
    geometric `R` alone; see
    `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` §5.
  - Sequenza chiusa: REOPENED → F4 env evidence PASS → F2+F3 code tip
    `a2186ac1` → PLAN pre-stamp `805fb34d` → Guardian re-CLOSE GO →
    **this PLAN stamp** → F1 forward WAV tranche (debt) → product G1b may
    unfreeze.
- **G1a vs DoD contratto §14 item 1 (G1a)**: T1–T6 + M2 + remediation
  `a2186ac1` atterrati; **G1a CLOSE: GO** (questo stamp). Retract storico:
  claim CLOSE da `1746a058` (withdrawn) e stato REOPENED `284228d6` come
  *current* status. **No G1 PASS**.
- **GO G1b (mandato)**: storicamente aperto nel PLAN tip `37f6ac60`
  (2026-07-25) dal reviewer (Marco) con mandato: **REV7 only if
  implementation demonstrates falsifiable impossibility** (not
  inconvenience); one redteam + one independent CC per tranche; stop-rule
  semantica sotto (non etichetta di severity). **Product G1b may unfreeze
  after this G1a CLOSE stamp**; uncommitted `ml_v3/frontend/` = **spike /
  feasibility probe** (not gate proof; non tip ufficiale G1b; spike ≠ tip)
  fino a tip G1b ufficiale / M3+M4.
- **Authority hierarchy (Motore-v3)**:
  - GLOBAL / RELEASE AUTHORITY → `ALIGNMENT_MANIFEST.md`
  - MOTORE V3 LAB STATE AUTHORITY → `docs/MOTORE_V3_PLAN.md`
  - FROZEN G1 TECHNICAL AUTHORITY → `docs/MOTORE_V3_G1_CONTRACT.md` @
    `6d254d0a`
  Per task Motore-v3, ordine di lettura: PLAN → frozen phase contract →
  ALIGNMENT_MANIFEST per vincoli globali / ship.
- **Stop-rule (unica, semantica)**: **BLOCKER** (indipendentemente da
  etichetta CRITICAL/HIGH/MED/LOW) se abilita: false PASS; final-test
  leakage; split leakage; hash/binding ambiguity; canonical
  nondeterminism; post-freeze gate-domain mutation; non-implementability
  del contratto. Tutto il resto → durable debt list. Le etichette di
  severity descrivono gravita; la semantica decide se il gate puo
  procedere.
- **Debt list (durable)** — non-BLOCKER; do not reopen T5 hash:
  1. `contract_doc_sha256` tripwire (also in T5 SHA256SUMS; T5/T6 chain).
     Precomputed (contract @ `6d254d0a`):
     `6a6f6d35bbf3fc65d7a01e54620bf4f9649ea77d60c2b3d9e7ea0b72f7f49a86`.
     Tripwire against silent contract edits — not a G1a/G1b close criterion.
  2. **From M1 T5 CC** (hygiene, non-CRITICAL):
     (a) `.` path segments accepted in SHA256SUMS paths;
     (b) only ASCII space stripped — NBSP / unicode WS accepted;
     (c) commit-anchor SHA declared in PLAN/docstring, not a machine-checked
     constant in `sha256sums.py`.
  3. **§8.2.4 gate 8 ordering** (post-`a2186ac1`): premature reveal / salt
     retry **not** fully modeled in §8-minimo single-record validate —
     deferred to §8-pieno/ledger. F2 closed only for commitment↔reveal
     match on non-null reveal; do not reopen F2 for this residual.
  4. **F1 WAV placement**: no rewrite; forward remediation tranche
     (in-repo WAV inventory remains until then). Durable debt post-CLOSE.
- **Ancora assente / in corso**: product G1b (**may unfreeze** after this
  G1a CLOSE stamp; spike `ml_v3/frontend/` ≠ tip); F1 WAV forward tranche
  (durable debt); modello V3, runtime V3, UI V3, build Ableton V3,
  training V3. Ship-line (`Source/`, CMake, `Resources/`, `AIEQ-mac`)
  **0-diff** vs freeze G0 `2c88edad`. Product G1b puo toccare
  `ml_v3/frontend/` (e solo minimum `Source/` se contract-authorized
  later) — **still no silent ship of V3 to Ableton** without later gates
  (G6+). **No G1 PASS.**
- **Nessun training V3 e autorizzato** oltre i limiti di fase.
- Il CONTROL Motore v2/A4b resta **NO-GO**; nessun modello e promosso.
- Il laboratorio prominence v2 vive su **branch separati** e NON e integrato in
  questo branch (vedi sezione "Stato prominence v2").

Le sezioni G1b–G8 seguenti restano criteri FUTURI finche la sottofase
riceve GO di chiusura tip. **G1a CLOSE: GO** (questo stamp; Guardian
re-CLOSE 2026-07-25). History: false CLOSE `1746a058` withdrawn; REOPEN
`284228d6` — REOPENED **non** e lo stato corrente. Non confondere
"mandato G1b" / "G1b may unfreeze" con "G1b tip ufficiale", "G1 PASS" o
"release-safe". Spike frontend ≠ tip.

Obiettivo finale: non inferiorita misurata rispetto a smart:EQ 4 sul
bilanciamento tonale e rispetto a soothe, Equator e Gullfoss sulle anomalie
dinamiche, con particolare peso a techno, house e breakbeat.

Motore v2 resta una baseline sperimentale riproducibile. Non modificare ship
line, plugin installato, APVTS, preset, `Resources/Models/ml_weights.bin`,
`feature/a4b-data-seed-grid` o `feature/unified-exp-clean`.

## Governance

- **Authority hierarchy (Motore-v3)**: GLOBAL / RELEASE →
  `ALIGNMENT_MANIFEST.md`; LAB STATE → questo PLAN; FROZEN G1 TECHNICAL →
  `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a`. Ordine di lettura task
  Motore-v3: PLAN → frozen phase contract → ALIGNMENT_MANIFEST (global /
  ship).
- **Stop-rule (unica, semantica)**: BLOCKER (a prescindere da
  CRITICAL/HIGH/MED/LOW) se abilita false PASS, final-test leakage, split
  leakage, hash/binding ambiguity, canonical nondeterminism, post-freeze
  gate-domain mutation, o non-implementability del contratto; altrimenti
  durable debt list. Severity = gravita; semantica = proceed/stop.
- Branch offline: `feature/motore-v3-offline`, basato su
  `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Integrazione futura: branch nuovo da
  `feature/unified-exp-clean@68ca31b43f5e523f63d70ce58a6cbf255760f82e`.
- Un solo implementatore scrive sul branch; il reviewer esamina un commit
  immutabile e restituisce un'unica lista consolidata di finding.
- Ogni fase definisce input e hash, file ammessi, output e schema, comandi,
  metriche, gate, stop condition e rollback.
- Ogni fase produce commit atomici e report numerico. La fase successiva non
  parte senza counter-check e GO esplicito.
- GO G1a — reviewer (Marco) su `6d254d0a` → fase aperta (2026-07-25).
  T1: `94dc9991`. T2: `1908fc45`. T2.1: `918b3dde`. T3: `9aa19295`.
  T4: `3bfd8aaf` (metrology lock;
  `metrology_lock_sha256` =
  `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`).
  T5: `75cb6902` (SHA256SUMS trust anchor). M2: `e9916319` (fixture-spec
  v1; digest `513c3baf…`). Hygiene: `fe8b97d3` (SHA256SUMS path order).
  T6: `501a4e00`. Remediation tip: `a2186ac1` (F2/F3/F4 closed in code;
  **tip codice G1a**). History: false CLOSE `1746a058` withdrawn; REOPEN
  `284228d6`; PLAN pre-stamp `805fb34d`. Stop-rule: vedi bullet semantico
  sopra (no automatic HIGH/MED→debt). **M1 T5 catch-up CC: CLEAN** on
  `75cb6902`. **G1a CLOSE: GO** (Guardian re-CLOSE 2026-07-25) on
  `a2186ac1` + `805fb34d`; contract freeze `6d254d0a` invariato; lock
  `d2c35ccc…`; SHA256SUMS 48 verify OK; F2/F3/F4 closed; gate-8 + F1 WAV
  durable debt (no rewrite); **REV7 consolidate: NO**. **No G1 PASS.**
  **Product G1b may unfreeze after this stamp** (spike ≠ tip).
- **G1b spike WS4 (2026-07-25):** tip `c7f05871` @
  `spike/motore-v3-g1b-frontend` → gate-4 **RED** (max|Δ|=9.537 dB) under
  REV6 activity `max(psd)>−120`; streaming/proof (b) PASS; P1–P7
  unchanged. Guardian: impossibilita del **predicato di admission**, non
  della soglia 0.25. Candidate + ACTIVE proposal + redteam
  BROKEN→POROUS; independent re-measure on ENBW+floor / §7-on-`R` →
  still **RED** (`docs/MOTORE_V3_REV7_REMEASURE_R_REPORT.md`: noise
  9.54→1.24; multitone ~4.77; **dense probe (iii) ~1.19 FAIL** — sparse
  excitation insufficient; residual = inter-band inseparability under
  Hann main lobe). **Product decision: report-only LF** (gate on `R` only;
  ∉`R` mandatory publish; no LF dB shopping). Clause:
  `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`. **REV7 consolidate: NO.**
  Final re-measure recorded: `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` —
  stationary on `R` **PASS** (~0.19 / ~0.05 dB); overall gate still **FAIL**
  on `log_sweep` HF floor-union skirt (b106, ~6.44 dB). Report-only ∉`R`
  published. **REV7 consolidate: NO** until sweep admission amend a priori.
  G4 debt: cross-SR low-end detections. **≠ G1 PASS.**
- **Living next-path (post-`b3d7f71b`, counsel — no REV7 consolidate now):**
  stazionario su `R` PASS; FAIL = `log_sweep` HF skirt (floor-union), non
  geometria LF. Ordine: (1) chiudere `log_sweep` HF a priori (admission /
  one-sided-floor fixture; docs + redteam + CC → solo allora candidato amend
  REV7 o debt esplicito no-amend); (2) tip G1b ufficiale solo dopo quella
  decisione (amend GO → tip + lock re-hash; no amend → tip REV6 + debt
  scritto; hard 0.25 dB intatto); (3) spike `ml_v3/frontend/` = lab only ≠
  tip; (4) debt parallelo F1 WAV / gate-8 non riapre G1a CLOSE. Mai:
  0.25→media/p95; ship/Source; training; “G1 PASS”.   Short path:
  `docs/EMBER_CORE_PARALLEL_HANDOFF.md` §A Sequenza restante.
  **Proposal STAMPED (document-only, post-`2c69606f`, Marco 2026-07-26):**
  `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` — **A / A3 / fuori REV7 LF**;
  next = redteam + independent CC (leave uncommitted); ≠ REV7 consolidate;
  ≠ G1b tip.

- GO G1b — mandato storico in PLAN `37f6ac60` (2026-07-25) con REV7:
  amend solo se l'implementazione dimostra **impossibilita falsificabile**
  (non inconvenienza); un redteam + un CC indipendente per tranche;
  stop-rule semantica (stesso bullet sopra). **Product G1b may unfreeze
  after this G1a CLOSE stamp**; uncommitted / spike `ml_v3/frontend/` =
  feasibility probe (not gate proof; spike ≠ tip) until official G1b tip /
  M3+M4. Nessun ship Ableton.
- Dati, cache e modelli restano in `~/aieq_data/motore_v3/`; nel repository
  entrano soltanto codice, manifest, lock, hash, contratti e report.
- Massimo tre round completi di training. Non si compensano fallimenti offline
  con threshold o routing runtime ad hoc.

## G0 - Freeze e riproduzione

- Tag annotato runtime:
  `checkpoint/motore-v2-runtime-5c9cb329-2026-07-19` su
  `5c9cb3290f87b62a339c6b2c49645b2b25524712`.
- Tag annotato scientifico:
  `checkpoint/motore-v2-a4b-final-88e70dd0-2026-07-19` su
  `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Congelare un ambiente v3 separato con versione Python completa, piattaforma,
  pacchetti e hash delle distribuzioni.
- Verificare in profondita gli hash audio di `train`, `heldout` e `test`, poi
  gli artefatti storici in `ml_v2/baselines/a4b_control/SHA256SUMS`.
- Il checkpoint corrente include la fixture test-only `fsld:46593`, ammessa
  dopo i primi report e responsabile del passaggio controllato da 834 a 835
  righe test. Il riferimento comportamentale corrente e il CONTROL 3x2
  `a4b_control_grid_20260718_85fa55ec`, non i tre report pre-fixture.
- Verificare l'hash del `SHA256SUMS` della griglia, rieseguire i candidati
  dataset-seed 42 per model seed 42, 1337 e 2026, normalizzare soltanto la riga
  `model=` e confrontare gli output con `eval_s*_d42.txt`.
- Il risultato atteso e A4b `NO-GO 0/3`: G0 passa se lo riproduce esattamente,
  non se il modello diventa verde.

## G1 - Frontend e benchmark

- Ricampionare il solo percorso di analisi a 48 kHz, fuori dal callback audio,
  prima dell'estrazione delle feature.
- Congelare 120 bande logaritmiche con centri fisici 20 Hz-20 kHz e durate
  temporali identiche a ogni sample rate.
- Usare i sette profili utente esistenti. Stato VERIFICATO sul codice corrente:
  il parametro host APVTS espone esattamente sette scelte - `Generic`, `Vocals`,
  `Drums`, `Bass`, `Synth`, `Master`, `EDM` (`Source/PluginProcessor.cpp:651`);
  `AIEngine::SourceProfile` possiede anche `Techno` con soglie proprie
  (`Source/AI/AIEngine.h:103-113`), ma il parametro host lo rende irraggiungibile
  perche clampa gli ID a `0..6` (`Source/PluginProcessor.cpp:1849`).
  La mappatura `Techno -> edm` e quindi una FUTURA policy dell'adapter V3 per i
  metadata di benchmark, NON il comportamento del codice attuale; APVTS resta
  invariato e non si aggiungono parametri host.
- Congelare split globalmente group-disjoint: `train`, `validation`,
  `calibration`, `development-metric`, `final-test`.
- Congelare evaluator, schema annotazioni, metriche e protocollo dei render
  competitor prima di qualunque training.
- Gate: determinismo, overlap zero, gain invariance e parity 44.1/48/96 kHz
  con max |Δ| ≤ 0.25 dB.

## G2 - Baseline deterministica

- Implementare profile matching tonale e detector tempo-frequenza per
  Resonance, Sibilance e Harshness senza ML.
- Produrre una curva tonale continua su 120 bande e al massimo otto filtri AI
  statici globali.
- Il fitting e azionabile soltanto con RMSE percettivamente pesato entro
  0.5 dB nelle regioni attive e p95 entro 1.5 dB; altrimenti e report-only.
- Gate: almeno 10% di miglioramento rispetto a v2 nelle metriche primarie,
  senza regressioni clean.

## G3 - Corpus e target reali

- Ammettere soltanto CC0, CC-BY o materiale OWNED con ledger completo e
  verifica hash fail-closed.
- Minimo per profilo: 500 sorgenti, 5 ore e 30 gruppi indipendenti; almeno il
  40% del test finale deve essere elettronica.
- Sintetici: 1-3 degradazioni simultanee fino a +/-12 dB; il target correttivo
  sicuro e esplicitamente limitato a +/-9 dB.
- Naturali: due annotazioni indipendenti e adjudication di curva correttiva a
  120 bande, regioni problematiche e actionability.
- Le sole etichette "problema presente" non sono supervisione sufficiente per
  la curva tonale.

## G4 - Selezione e training

- Prima del full training congelare un contratto di bake-off tra baseline DSP,
  TCN causale e modello spettro-temporale compatto.
- Tutti i candidati condividono input e output di G1; il contratto G4 definisce
  forme tensoriali, causalita, pooling e budget compute.
- Scegliere il modello piu semplice che supera G2 di almeno il 10% rispettando
  integralmente la clean safety.
- Training finale fattoriale: tre model seed per due dataset seed. Un model
  seed e verde soltanto se passa con entrambi i dataset seed; ne servono due.
- Confidence tonale: calibrazione su clean e positivi, massimizzando macro-F1
  con meno del 2% di clean actionable oltre 1 dB.
- Anomaly Engine: calibrazione separata con massimo 0.5 falsi eventi/minuto.
- Dopo tre round senza GO, resta il motore deterministico.

## G5-G8 - Validazione e prodotto

- G5: render ciechi loudness-matched per Marco. E un veto su artefatti e
  workflow, non una prova di non inferiorita.
- G6: integrazione suggestions-only dalla linea EXP pulita, esclusivamente con
  commit v3 controfirmati.
- G7: processing dinamico mediante `DynamicCorrectionEngine` esistente,
  default OFF e rollback immediato.
- G8: panel formale; numerosita stabilita con power analysis dal pilot, minimo
  12 ma non assunto sufficiente.
- Non inferiorita: CI 95% clusterizzata per ascoltatore e sorgente, con limite
  inferiore non peggiore di -10 punti percentuali.

## Interfacce e UX congelate

- Tipi pubblici futuri: `V3FeatureFrame`, `V3AnalysisSnapshot`,
  `V3TonalSuggestion`, `V3DynamicEvent`, `V3SuggestionBundle`.
- Ogni bundle contiene ID stabile, `ProblemType`, confidence, actionability,
  motivo di rifiuto e filtri statici oppure un evento dinamico.
- I filtri AI statici sono globalmente al massimo otto e appartengono a un solo
  bundle. FIX ALL deduplica e non sovrascrive bande manuali significative.
- Pannelli AI e Semantic, barre e curve ambra, Capture, FIX singolo, FIX ALL e
  undo/redo restano invariati.
- La diagnosi valuta la risposta prevista dopo EQ manuale e semantica, evitando
  di duplicare intenzioni come Warmth o Air.
- `AIEQ_ENABLE_MOTORE_V3` **non esiste nel codice corrente** (verificato: zero
  occorrenze in `Source/` e `CMakeLists.txt`). VERRA INTRODOTTO in una futura
  fase di integrazione, **OFF di default** nelle build normali. Il comportamento
  qui descritto e il contratto previsto per quel flag futuro, non lo stato
  attuale: nell'EXP, modello valido e `aiEnabled` attiveranno i suggerimenti;
  `dynamicCorrections` controllera soltanto l'audio dinamico. Anche i tipi
  pubblici `V3FeatureFrame`, `V3AnalysisSnapshot`, `V3TonalSuggestion`,
  `V3DynamicEvent` e `V3SuggestionBundle` sono nomi di contratto FUTURI: oggi
  esistono solo in questi documenti, non nel codice.
- Modello assente, corrotto o incompatibile: fallback completo alle euristiche
  correnti. Nessun nuovo parametro host.

## Gate finali e assunzioni

- G6 richiede parity Python/C++, build v3/no-v3, quattro runner, CTest con
  `--no-tests=error`, ASan/TSan, pluginval s8+s10 e Ableton.
- Installazione soltanto come `AI Equalizer Pro v3 EXP`; il plugin originale
  non viene toccato.
- Smoke locali su M1 Pro; cloud autorizzato per tranche soltanto dopo G2/G3.
- **STIME DI PROGRAMMA NON VALIDATE, non garanzie**: closed beta indicativamente
  4-6 mesi; prova credibile di parita top-tier indicativamente 6-12 mesi. Sono
  proiezioni di pianificazione, prive di evidenza sperimentale a supporto e
  soggette a revisione a ogni gate; non vanno citate come impegni.
- Release production NO-GO fino a G8 verde e zero P0 aperti.

## Stato prominence v2 (laboratorio separato, NON integrato qui)

Registrato per evitare che risultati di un altro branch vengano letti come
progressi del V3. Il laboratorio prominence vive su branch separati
(`feature/prominence-engine-phase1`, `feature/prominence-p0-p2`) e **non e
integrato in `feature/motore-v3-offline`**.

- **P0: PASS**, ma esclusivamente come probe DETERMINISTICO e riproducibile
  (due run bit-identiche). Non e una prova di qualita del motore.
- **P1: NO-GO** contro i criteri congelati, con questi numeri:
  - 100 Hz: monotonicita FALSA; dinamica `0.000395` < `0.03`;
    separazione picco/valle `0.000876` < `0.05`;
  - 2000 Hz: dinamica `0.019241` < `0.03`;
    separazione picco/valle `0.008332` < `0.05`.
- **P2 e P2-bis sono guardie** del valutatore e dell'hash di default: proteggono
  da errori di misura e da regressioni silenziose, NON sono prove di qualita.
- In questo branch **non e autorizzato nulla** di V3b, V4a, V4b o P0-bis.
- Il futuro V3 **non eredita automaticamente** l'encoding legacy invertito ne le
  sue scelte di finestra: la rappresentazione V3 va definita nei suoi contratti,
  non ereditata dal laboratorio v2.

Debt confermati del vecchio laboratorio prominence, che **non devono diventare
semantiche del futuro V3**:

1. Python applica `smoothingOctaves` dalla config, mentre il C++ prepara le
   `windowSizes` con il default;
2. `maxWidthBands` nel detector e in realta misurato in **bin FFT grezzi**, non
   in bande.
```

---

## B.2 FILE: `docs/MOTORE_V3_G1_CONTRACT.md (freeze @ 6d254d0a — NOT amended)`

**Path logico:** `docs/MOTORE_V3_G1_CONTRACT.md`  
**Bytes:** 63636  
**Lines:** 1245

```markdown
# Motore v3 - Contratto G1 frontend e benchmark

Stato: PROPOSTA IMMUTABILE PER COUNTER-CHECK, REVISIONE 6 CONSOLIDATA
+ micro-amend (`source_snapshot_sha256`, grammatica `group_id`, scope
append-only) — 2026-07-24. Document-only; non autorizza ancora
l'implementazione ne G1a. G1 parte soltanto dopo il GO del reviewer sul
commit document-only che chiude questo emendamento; nessun criterio e
stato rilassato.

REVISIONE 5 incorpora il counter-check fattuale del 2026-07-23 (allineamento
qui sotto). Nessun criterio, gate o contenuto tecnico e stato modificato
rispetto alla REVISIONE 4: la revisione registra soltanto lo stato verificato.

REVISIONE 6 (metrology, working tree precedente) chiudeva le falle di misura
CRITICAL (porzioni stazionarie/warm-up/coda; dominio max |Δ| 0.25 dB;
schedule streaming). REVISIONE 6 CONSOLIDATA aggiunge: (a) residuali
metrology H1–H3 (warm-up additivo, activity mask unione cross-SR,
bit-identita obbligatoria sulla piattaforma di gate); (b) §8-minimo
byte-level pinnato (roster exact-key, `admission_batch_id` hex-64 in HMAC
di ruolo e pilot, soglie intere, identita pack, NUL vietato). Nessun gate
e stato allentato; nessuna soglia numerica e stata alzata.

**Fuori scope di questo emendamento (§8-pieno, rimandato):** ledger
append-only a cinque checkpoint, `immutable_artifact_ref`, ownership-ledger,
i quattordici schemi estesi oltre il minimo G1a. Non vanno aggiunti qui.

Questo emendamento document-only **non** autorizza G1a ne alcun codice
finche il reviewer non emette GO sul commit che lo contiene (al piu
accompagnato dalla sola riga G1 di parity in `docs/MOTORE_V3_PLAN.md` per
coerenza letterale max |Δ|). I path `ml_v3/contracts/` e ogni altro file G1
di codice restano fuori da quel commit.

Allineamento all'audit 2026-07-23: **nulla di questo contratto e implementato**.
Non esistono frontend V3, modello V3, runtime V3, UI V3 o build Ableton V3;
nessun training V3 e autorizzato; `AIEQ_ENABLE_MOTORE_V3` non esiste ancora nel
codice e verra introdotto OFF di default in una futura fase di integrazione.
G0 e PASS soltanto come freeze riproducibile della baseline NEGATIVA (il CONTROL
Motore v2/A4b resta NO-GO). **Tutti i gate elencati qui sotto sono criteri
FUTURI da soddisfare: nessuno e PASS, e nessuno va rilassato.**

## 1. Scopo e risultato atteso

G1 congela il sistema di misura usato da tutte le fasi successive. Deve
eliminare tre ambiguita che hanno reso il Motore v2 difficile da addestrare e
da valutare:

1. la durata delle feature non deve cambiare con il sample rate dell'host;
2. colore tonale largo, picchi locali e variazioni temporali non devono essere
   compressi nello stesso singolo canale;
3. training, calibrazione, selezione del round e test finale non devono
   condividere gruppi o responsabilita.

Output G1:

- frontend Python deterministico e streaming-equivalent;
- fixture sintetiche congelate con hash;
- split contract v3 fail-closed;
- schema di annotazione e schema dei risultati;
- evaluator deterministico per curve tonali e anomalie;
- protocollo di render e confronto competitor;
- report numerico dei gate G1.

G1 non allena modelli, non sceglie l'architettura ML e non dimostra parita con
un competitor. Costruisce il metro con cui queste claim potranno essere
falsificate.

## 2. Perimetro e protezioni

Durante G1 sono ammessi soltanto:

- `docs/MOTORE_V3_G1_CONTRACT.md`;
- `ml_v3/frontend/`;
- `ml_v3/benchmark/`;
- `ml_v3/contracts/`;
- `ml_v3/fixtures/g1/` per manifest, generatori e piccoli artefatti test;
- `ml_v3/tests/`;
- `ml_v3/environment/` solo per aggiornare il lock con dipendenze necessarie;
- `ml_v3/reports/G1_FRONTEND_BENCHMARK_REPORT.md`.

Sono vietati:

- `Source/`, `CMakeLists.txt`, `Resources/`, `ml_v2/` e `AIEQ-mac/`;
- pesi, training, threshold appresi o routing runtime;
- APVTS, preset, GUI, pannelli AI/Semantic e plugin installati;
- modifiche ai branch o tag protetti di G0;
- uso del `final-test` per scegliere feature, soglie o implementazione.

Tutti gli artefatti grandi restano sotto `~/aieq_data/motore_v3/g1/`. Nel
repository entrano solo fixture piccole, manifest, contratti, hash e report.

## 3. Riferimenti autorevoli e debiti da non ereditare

- Base scientifica: `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Freeze G0: `2c88edad489c02b01d97b0d46cb7c206e33937bb`.
- Il test v2 a 44.1 kHz e un testimone utile del metodo di parity, non una
  specifica v3.
- `PerceptualFrontEnd` corrente e diagnostics-only. Il suo LF usa l'ultimo
  frame 8192 disponibile, non allineato al frame 4096, e le durate cambiano
  col sample rate. G1 non puo replicare questi due comportamenti.
- Il contratto A4b separa calibration e metric per group, ma contiene ruoli
  legacy e solo quattro destinazioni. G1 parte con un nuovo contratto a cinque
  ruoli e nessun fallback legacy.
- L'evaluator A6 basato su occupancy e singoli hit resta storico. Nessun
  criterio G1 puo essere soddisfatto da un solo hit o da una sola clip.

## 4. Contratto di ingresso

### 4.1 Audio

Il frontend accetta array float32 mono o stereo interleaving-independent ai
sample rate host comuni `44100`, `48000`, `88200`, `96000`, `176400` e
`192000` Hz. I gate obbligatori G1 coprono 44100, 48000 e 96000 Hz;
`88200`, `176400` e `192000` Hz restano sperimentali / report-only fino a un
gate dedicato successivo e non possono sostenere un PASS di sample-rate
parity. Ogni altro sample rate viene rifiutato, non reinterpretato.

Regole fail-closed:

- zero canali, piu di due canali, sample rate non valido, NaN o Inf: errore;
- mono: `mid = input`, `side` non valido;
- stereo: `mid = (left + right) / 2`, `side = (left - right) / 2`;
- nessun limiter, normalizzatore o dither implicito;
- nessun padding finale; lo stato iniziale e zero e i frame incompleti non
  vengono emessi;
- ogni output conserva `asset_id`, `group_id`, sample rate originale e hash
  SHA-256 dell'audio sorgente.

### 4.2 Profili

I soli profili condizionanti sono i sette esposti oggi dall'APVTS:

`generic`, `vocals`, `drums`, `bass`, `synth`, `master`, `edm`.

Questo e anche l'ordine canonico degli ID `0..6`; stringa e ID devono
concordare o il record viene rifiutato.

Stato VERIFICATO sul codice corrente: queste sette scelte sono esattamente
quelle esposte dal parametro host APVTS (`Source/PluginProcessor.cpp:651`), e il
parametro clampa gli ID a `0..6` (`Source/PluginProcessor.cpp:1849`).
L'enum interno `AIEngine::SourceProfile` contiene ANCHE `Techno` con soglie
proprie (`Source/AI/AIEngine.h:103-113`), ma il clamp lo rende irraggiungibile
dall'host: **oggi nessun conditioning `Techno -> edm` avviene nel codice**.
La mappatura `Techno -> edm` e una FUTURA policy dell'adapter V3 per i metadata
di benchmark. Techno, house, breakbeat e altri sottogeneri restano metadati di
benchmark separati e obbligatori quando noti; non aggiungono un parametro host e
non cambiano il numero dei profili.

## 5. Ricampionamento canonico

L'analisi avviene sempre a 48000 Hz. Il ricampionamento e parte del contratto,
non una pre-elaborazione libera del chiamante.

Per un input `fs_in`, ridurre la frazione:

```text
g = gcd(fs_in, 48000)
up = 48000 / g
down = fs_in / g
```

Se `up == down == 1`, il ricampionatore e l'identita, con ritardo zero. Negli
altri casi la reference Python usa un FIR low-pass polyphase causale con:

- numero tap `128 * max(up, down) + 1`;
- finestra Kaiser `beta = 9.0`;
- passband richiesta fino a 20000 Hz e stopband da
  `min(fs_in, 48000) / 2`;
- cutoff al punto medio fra i due estremi, espresso in cicli/campione alla
  frequenza intermedia `fs_in * up`;
- guadagno dei coefficienti moltiplicato per `up`;
- stato streaming conservato tra blocchi;
- ritardo di gruppo dichiarato nei metadati, mai eliminato con look-ahead;
- nessun padding o riflessione ai bordi.

Con `pass_hz = 20000`, `stop_hz = min(fs_in, 48000)/2`,
`fc = ((pass_hz + stop_hz)/2) / (fs_in*up)` e `M = num_taps - 1`, i
coefficienti sono:

```text
h0[n] = 2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0)
h[n]  = up * h0[n] / sum(h0), n = 0..M
```

`sinc(x) = sin(pi*x)/(pi*x)`. La fase polyphase e zero: l'output `j` e
`sum_n input[n] * h[j*down - n*up]`, considerando zero gli indici di `h`
fuori `[0, M]`. Per `N` campioni di input si emettono gli indici
`0 <= j < ceil(N*up/down)`; alla fine non si emette la coda ulteriore del
filtro.

Il ritardo fisico e la frazione esatta
`(num_taps - 1) / (2 * up * fs_in)` secondi. Ogni frame espone sia l'indice
intero nel flusso canonico sia il tempo sorgente razionale:

```text
source_time = frame_end_sample / 48000 - resampler_group_delay_seconds
```

Numeratore e denominatore vengono conservati come interi nei metadati. I test
fra sample rate si allineano su `source_time`, non sull'indice di output grezzo.

Il generatore dei coefficienti, la versione della libreria numerica e gli hash
delle fixture entrano nel lock G1. La futura implementazione C++ puo usare una
struttura diversa, ma deve riprodurre le feature entro i gate di parity; non
puo cambiare il comportamento osservabile.

## 6. Griglia e temporizzazione

### 6.1 Griglia fisica

Esistono esattamente 120 centri, inclusivi, da 20 a 20000 Hz:

```text
center[i] = 20 * (20000 / 20) ** (i / 119), i = 0..119
```

Le bande sono triangolari sull'asse `log2(f)`. Il supporto interno della banda
`i` va dal centro precedente al centro successivo; per le due bande estreme si
usa un centro virtuale ottenuto con lo stesso rapporto geometrico. DC e bin
oltre 20000 Hz non contribuiscono.

Per ogni FFT si calcola PSD one-sided in potenza:

```text
psd[k] = edge_factor[k] * abs(FFT(window * x)[k])**2
         / (48000 * sum(window**2))
```

`edge_factor` vale 1 a DC/Nyquist e 2 negli altri bin. L'energia di banda e la
media pesata lineare della PSD; conversione in dB soltanto dopo media e fusione.
Il floor e `1e-12`, poi clamp a `[-120, +12]` dBFS/Hz.

### 6.2 Due risoluzioni, stesso timestamp

- percorso MAIN: FFT 4096;
- percorso LF: FFT 8192;
- finestra Hann periodica `0.5 - 0.5*cos(2*pi*n/N)`;
- hop comune 1024 campioni canonici, cioe 21.333333 ms;
- finestre causali right-aligned allo stesso `frame_end_sample` esclusivo;
- primo frame soltanto quando sono disponibili 8192 campioni reali;
- timestamp in campioni canonici e secondi razionali, non in float.

La fusione avviene in potenza:

- LF puro fino a 160 Hz;
- MAIN puro da 320 Hz;
- crossfade raised-cosine su `log2(f)` fra 160 e 320 Hz.

Nel crossfade il peso LF e
`0.5 * (1 + cos(pi * log2(f/160) / log2(320/160)))`; il peso MAIN e il suo
complemento.

Non e consentito riusare l'ultimo frame LF. Per ogni frame MAIN deve esistere
un frame LF con lo stesso `frame_end_sample`.

## 7. V3FeatureFrame

G1 congela una superficie fisica, non il tensore del modello. G4 potra
selezionare o impilare questi campi senza cambiarne il significato.

Ogni frame contiene:

- `schema = "aieq-v3-feature-frame-1"`;
- `frame_end_sample`, `frame_index`, `source_time_num`, `source_time_den`,
  `canonical_sample_rate = 48000`;
- `mid_psd_db[120]`;
- `side_psd_db[120]`, `mid_valid` e `side_valid`;
- `mid_shape_db[120]` e `side_shape_db[120]`;
- `mid_prominence_db[120]` e `side_prominence_db[120]`;
- `mid_delta_db[120]` e `side_delta_db[120]`;
- `mid_level_dbfs`, `side_level_dbfs`;
- `valid` e un motivo enumerato quando falso.

Definizioni:

- `shape_db`: PSD normalizzata sottraendo
  `10*log10(sum(10**(psd_db/10)))` sulle 120 bande;
- `prominence_db`: `mid_shape_db` meno la sua convoluzione gaussiana su asse
  log, sigma 4 bande, kernel `exp(-0.5*(j/4)**2)` per `j=-16..16`, normalizzato
  a somma uno, con padding reflect;
- `delta_db`: shape corrente meno shape precedente, clamp `[-24, +24]` dB;
  sul primo frame valido e un vettore di zeri; un asset boundary o un frame
  globalmente non valido azzera la storia, quindi il successivo valido riparte
  da zero;
- `level_dbfs`: RMS time-domain non finestrato della finestra MAIN, floor
  -120 dBFS;
- `mid_valid`/`side_valid` richiedono livello del rispettivo canale almeno
  -100 dBFS; `valid` e il loro OR. Per input mono `side_valid` e sempre falso.
  I vettori di un canale non valido sono al floor e vengono ignorati. Un frame
  globalmente non valido non puo generare suggerimenti o eventi.

Non sono parte di G1: lunghezza della sequenza, pooling, receptive field del
modello, hidden size, logits, threshold o confidence calibration.

## 8. Split contract v3

### 8.1 Ruoli

Ogni gruppo ha esattamente uno dei cinque ruoli globali:

- `train`: aggiornamento dei pesi;
- `validation`: early stopping e scelta iperparametri interna a un round;
- `calibration`: soglie e calibrazione, dopo il freeze dei pesi;
- `development-metric`: scelta fra round e accettazione G2/G4;
- `final-test`: verifica sigillata del candidato congelato e benchmark
  competitor; mai usato per decisioni precedenti.

Quota attesa per strato: 55%, 10%, 10%, 10%, 15% nello stesso ordine. Le
quote non autorizzano un corpus insufficiente: G3 deve avere per ogni profilo
almeno 10 gruppi train, 3 validation, 3 calibration, 3 development-metric e 5
final-test. Almeno il 40% del `final-test` complessivo deve essere elettronica.
Questi sono floor generali di split, non sufficienti per calibrare o validare
una famiglia: i supporti piu severi delle sezioni 10.4 e 11 prevalgono.

### 8.2 Identita e assegnazione (§8-minimo byte-level pinnato)

`group_id` rappresenta la piu piccola unita conservativa che racchiude tutte
le dipendenze note: composizione, registrazione/sessione o artista; se questa
identita non e ricostruibile, si usa l'intero pack. Alias della stessa entita
fra sorgenti diverse vengono riconciliati prima dell'ammissione. Ogni gruppo ha
metadati immutabili `group_primary_profile`, `group_primary_domain` e
`source_family`. Stem, mix, versioni, crop, augmentation, injection, render
processati e render competitor ereditano lo stesso gruppo del dry originale.
Mapping di alias, upstream snapshot e regole di inclusione sono versionati
prima dello split; rinominare un gruppo o cambiare profilo/dominio dopo
l'assegnazione invalida il batch.

#### 8.2.1 Canonical JSON (hashabile)

Ogni documento usato per `admission_batch_id`, commitment o artefatti di split
segue le stesse regole degli artefatti numerici di §13: UTF-8 senza BOM;
chiavi ordinate per code point; separatori esatti `,` e `:`; newline LF finale
singola **inclusa nell'hash**; `allow_nan=false`; in lettura rifiuto di
NaN/Infinity/overflow e chiavi duplicate. Si ordinano solo i set dichiarati
order-independent; liste semantiche (curve, score, griglia) restano in ordine.

#### 8.2.2 Identita `group_id` (non testo libero)

- Con upstream stabile: `source_family + ":" + upstream_id`.
- Senza upstream: `source_family + ":pack:" + SHA256(pack_bytes)`, dove
  `pack_bytes` e la concatenazione, per ogni digest SHA-256 **lowercase**
  ordinato per byte ASCII, di `(64 caratteri ASCII + LF)`. Nessun altro
  serializzatore e ammesso. La sequenza `:pack:` e **riservata** alla sola
  forma di fallback generata dal contratto; un `upstream_id` non deve
  produrre ne imitare quella forma.
- Il `group_id` dichiarato deve coincidere con quello ricostruito.
  `upstream_id` e pack entrambi presenti, oppure entrambi assenti quando
  servirebbe l'altro → FAIL.
- U+0000 (NUL) e **vietato** in ogni componente di identita o messaggio HMAC
  (`source_snapshot_id`, `group_id`, `group_primary_domain`, `source_family`,
  `upstream_id`, alias, e la stringa `admission_batch_id`): rifiuto **prima**
  di costruire il MAC. Inoltre `:` e **vietato** in `source_family` e in
  `upstream_id` (oltre al NUL gia vietato).

#### 8.2.3 Roster pre-split

Envelope exact-key, schema `"aieq-v3-admission-roster-1"`:

```text
{
  schema,
  source_snapshot_id,
  source_snapshot_sha256,
  groups: [
    { group_id, group_primary_profile, group_primary_domain, source_family }
  ]
}
```

`groups` ordinati per byte UTF-8 di `group_id`; `additionalProperties`
vietato; il roster **non** porta ruoli ne flag pilot.

`source_snapshot_sha256` = SHA-256 **lowercase hex** (64 caratteri ASCII) di
`canonical_bytes(source_identity_index)`, con
`source_identity_index.schema == "aieq-v3-source-identity-index-1"` e byte
canonici secondo §8.2.1. Nessun altro digest, serializzazione o SHA
free-form e ammesso per quel campo.

`admission_batch_id` = SHA-256 **lowercase hex** (64 caratteri ASCII) dei
byte canonici del roster, **newline finale inclusa**.

#### 8.2.4 Commit-reveal

Prima di vedere il roster il reviewer genera 32 byte raw `salt` e committa

```text
commitment = SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt)
```

Il curatore materializza e committa il roster completo di tutti i gruppi
eleggibili della source snapshot, in ordine canonico, senza ruoli; soltanto
allora il reviewer rivela `salt` e il tool verifica il commitment. Salt con
lunghezza diversa da 32 byte, commitment errato, reveal anticipato o retry
del salt → FAIL.

#### 8.2.5 Ruolo (HMAC, interi esatti)

`admission_batch_id` entra nell'HMAC come **esattamente i 64 byte ASCII
lowercase dell'hex digest**, **mai** i 32 byte raw. Questa scelta e
obbligatoria: la codifica raw-32 vs hex-64 cambia circa il 63.4% dei ruoli.

Messaggio:

```text
HMAC-SHA256(salt,
  b"aieq-v3-role-v1" + NUL
  + admission_batch_id(hex-ASCII) + NUL
  + group_primary_profile + NUL
  + group_primary_domain + NUL
  + source_family + NUL
  + group_id)
```

Sia `value` i primi 8 byte del digest, unsigned big-endian. Confronto
**intero esatto** (nessun float):

- `train` se `value * 20 < 11 * 2^64`;
- `validation` se `value * 20 < 13 * 2^64`;
- `calibration` se `value * 20 < 15 * 2^64`;
- `development-metric` se `value * 20 < 17 * 2^64`;
- altrimenti `final-test`.

(Equivalente alle quote dichiarate 55/10/10/10/15% senza arrotondamento
floating-point.)

#### 8.2.6 Pilot

```text
HMAC-SHA256(salt,
  b"aieq-v3-pilot-v1" + NUL
  + admission_batch_id(hex-ASCII) + NUL
  + group_id)
```

`admission_batch_id` entra come i **stessi 64 byte ASCII hex** del punto 8.2.5
(identico; mai raw-32). Sia `value` tutti i 32 byte unsigned big-endian.
`development_pilot = (role == "development-metric" AND value * 4 < 2^256)`.
Fuori da `development-metric` e sempre false. Il pilot e group-disjoint dal
punto-estimate development dello stesso round; se non raggiunge i supporti
richiesti si ammettono nuovi batch, non si cambia la soglia 0.25
(equivalente intero sopra).

#### 8.2.7 Invarianti e ammissione

- nessun fallback legacy: ogni gruppo e assegnato esplicitamente;
- un `group_id` e un SHA audio in esattamente un ruolo;
- parent e ogni derivato condividono ruolo e `group_id` del parent; parent
  esistente salvo root esplicito; niente cicli parent;
- `asset_id` e `relative_path` unici nel manifest;
- near-duplicate detection e obbligatoria prima di G3, ma non sostituisce
  l'identita di gruppo;
- un batch rivelato non puo essere filtrato/riassegnato dopo aver visto i
  ruoli: ammesso interamente oppure rifiutato solo per regola fail-closed
  preregistrata verificabile senza leggere i ruoli; i suoi gruppi non
  rientrano sotto un altro ID o batch;
- strato di verifica minimo: `(group_primary_profile, group_primary_domain,
  source_family)`; il preflight pubblica conteggi per strato e si ferma se i
  minimi per profilo non sono raggiunti;
- roster, commitment, reveal, batch e assegnazioni materializzati sono
  immutabili e coerenti **intra-batch** nel senso di questo §8-minimo
  (niente rewrite silenzioso degli artefatti gia materializzati dello stesso
  batch); la detection/enforcement cross-batch di retry pregressi, riuso del
  salt, cancellazione di history o ri-ammissione dopo tentativi precedenti
  appartiene a **§8-pieno** (ledger persistente) e **non** e una proprieta
  dimostrata da G1a-minimo;
- un gruppo multi-dominio riceve un solo ruolo globale;
- una sorgente ammessa da v2 viene riammessa esplicitamente nel contratto v3;
  non eredita il fallback A4b;
- input malformato → reject, mai reinterpretazione;
- `final-test` mai usato per scegliere regole, feature o soglie.

Audio e annotazioni `final-test` vivono sotto una root separata. I loader di
training, validation, calibration e development rifiutano quel ruolo anche se
il path viene passato esplicitamente. L'evaluator finale richiede commit,
contratto e hash candidato gia congelati e registra l'apertura nel report.

Il trainer futuro deve rifiutare un contratto non committato, una modifica
retroattiva, un gruppo senza ruolo o un asset il cui hash non corrisponde.

## 9. Manifest e annotazioni

### 9.1 Asset manifest

Campi obbligatori:

```text
schema, asset_id, relative_path, sha256, group_id, admission_batch_id,
split_role, benchmark_families, development_pilot,
source_profile, primary_domain, group_primary_profile, group_primary_domain,
source_family, electronic_subgenre,
sample_rate, channels, duration_s, parent_asset_id, derivative_kind,
license_class, license_url, attribution, ledger_id
```

`benchmark_families` e una lista ordinata senza duplicati presa esclusivamente
da `tonal-controlled`, `tonal-natural`, `anomaly-natural`, `clean-safety` ed
`electronic-stratified`. `development_pilot` e booleano, puo essere vero
soltanto nello split `development-metric` ed e assegnato dopo lo split come in
§8.2.6: `HMAC-SHA256(salt, b"aieq-v3-pilot-v1" + NUL +
admission_batch_id(hex-ASCII) + NUL + group_id)`, con `admission_batch_id`
codificato come **64 byte ASCII hex** (identico al MAC di ruolo; mai raw-32),
`value` = digest intero unsigned big-endian a 32 byte, e
`development_pilot` vero solo se `role == "development-metric"` e
`value * 4 < 2^256`. Il pilot e group-disjoint dal punto-estimate
development dello stesso round; se non raggiunge i supporti richiesti si
ammettono nuovi batch, non si cambia la soglia equivalente a 0.25.

La membership benchmark viene congelata dopo l'adjudication ma prima di
eseguire qualunque candidato sul ruolo. Dipende soltanto da provenance e
annotazioni, mai da prediction o metrica; modificarla dopo il freeze invalida
manifest e report.

Ogni batch possiede inoltre un record `aieq-v3-admission-batch-1` con source
snapshot, regole di inclusione, roster SHA-256, salt commitment/reveal, commit
del roster, reviewer e stato admitted/rejected. Il manifest viene rifiutato se
questo record manca o non ricostruisce esattamente ruoli e pilot flag.

Licenze ammesse: CC0, CC-BY o OWNED con ledger completo. Campo mancante,
licenza sconosciuta o hash errato bloccano il preflight.

### 9.2 Annotation record

Ogni record usa `schema = "aieq-v3-annotation-1"` e contiene:

- `asset_id`, `annotator_id`, `pass_id`, `profile`,
  `evaluation_unit_id`, `segment_start_s`, `segment_end_s`;
- `tonal_correction_db[120]`: EQ correttiva desiderata; segno positivo =
  boost, negativo = cut;
- `tonal_confidence[120]` e `tonal_actionable_mask[120]`;
- `semantic_regions[]` per gli otto tipi pubblici correnti;
- `dynamic_events[]` per Resonance, Harshness e Sibilance;
- `complete_types[]`, `explicit_negative_types[]`, `global_actionable`,
  `clean_for_action`, note e versione tool.

Gli otto tipi e ID canonici restano, nell'ordine `0..7`: `Resonance`,
`Harshness`, `Muddiness`, `Sibilance`, `Boominess`, `Thinness`,
`BoxyMidrange`, `DullSound`. Un ID che non concorda con la stringa e invalido.

Ogni semantic region contiene tipo, inizio/fine, banda inferiore/superiore,
direzione, severity, confidence e actionability. Ogni evento dinamico contiene
tipo, inizio/fine, centro, larghezza in ottave, severity, confidence e
actionability.

Per Thinness e DullSound la direzione spettrale e obbligatoria; per Muddiness,
Boominess e BoxyMidrange e obbligatoria una banda; per Resonance e obbligatorio
il centro. Una sola label di presenza senza curva, regione o evento non e una
supervisione tonale valida.

Curve e prediction pubbliche sono finite e limitate a `[-9, +9]` dB;
confidence e severity sono in `[0, 1]`; tempi e frequenze devono cadere nel
segmento e in 20-20000 Hz. `clean_for_action = true` impone curva zero,
`tonal_actionable_mask` tutto falso, `global_actionable = false` e nessun
evento actionable; impone inoltre tutti gli otto tipi canonici dentro
`complete_types` ed `explicit_negative_types`. Fuori da questo caso, un tipo
entra in `complete_types` soltanto quando l'intero segmento e stato annotato
esaustivamente per quel tipo; entra in `explicit_negative_types` soltanto se e
completo e gli annotatori ne hanno verificato l'assenza. Le aree fuori dagli
eventi GT valgono come negative soltanto per tipi completi. Un suono colorato
ma intenzionalmente corretto e clean non viene trasformato in hard-negative di
un problema diverso.

`evaluation_unit_id` e congelato prima di eseguire i sistemi. Segmenti che
derivano dallo stesso intervallo annotato condividono l'ID e possono contribuire
una sola volta; dividere, duplicare o sovrapporre un segmento dopo il freeze
invalida il record.

G3 definira processo a due annotatori e adjudication. G1 congela formato e
semantica, non inventa annotazioni reali.

### 9.3 Prediction record

Ogni prediction usa `schema = "aieq-v3-prediction-1"` e contiene `asset_id`,
`model_id`, hash del modello e del frontend contract, `calibration_policy_id`,
SHA-256 della policy, profilo, curva tonale a 120 bande, `tonal_score[120]`
pre-calibrazione, `tonal_confidence[120]` calibrata, `segment_start_s`,
`segment_end_s`, bundle semantici e lista eventi. Ogni bundle ed evento porta
confidence e `actionable`.

La policy usa `schema = "aieq-v3-calibration-policy-1"` e contiene hash di
modello, frontend, calibration manifest e prediction schema; ID/versione,
algoritmo e parametri completi dei calibratori tonal e anomaly;
`tonal_band_thresholds[120]`, `semantic_type_thresholds[8]` e
`anomaly_class_thresholds[3]`; versione dell'estrattore di candidati e regole
deterministiche score-to-confidence, region-to-bundle e threshold-to-actionable.
Ogni mapping score-to-confidence e monotono non decrescente e definito anche
agli estremi zero e uno.
Viene fittata soltanto su `calibration` dopo il freeze dei pesi e committata
prima di aprire `development-metric`. Ogni modifica a calibratore, threshold o
decision rule cambia SHA-256.

Ogni prediction contiene anche `anomaly_score_ref` e
`anomaly_severity_ref`, riferimenti con SHA-256 ad array little-endian float32
di forma `[num_feature_frames, 3, 120]`, ordine classi `Resonance`, `Harshness`,
`Sibilance`, allineati ai frame G1. Sono superfici dense finite in `[0,1]`
prima di threshold, hysteresis, top-k o veto. Frame/bande invalidi sono marcati
da una mask separata e non possono essere omessi; una lista eventi senza queste
superfici e schema invalido.

L'evaluator applica prima la policy alla superficie score e deriva una lista
completa di candidati dalla confidence calibrata, prima della soglia. Per ogni
classe prende i massimi locali positivi nel vicinato 3x3 tempo-banda: confidence
maggiore o uguale a tutti i vicini e maggiore di almeno un vicino esterno al
proprio plateau connesso. Per ogni plateau conserva soltanto la coordinata
frame/banda minima.
In ordine decrescente di confidence, ogni massimo genera la componente 8-neighbour
che lo contiene nella mask `confidence >= 0.5 * peak`; massimi successivi la cui
componente contiene gia un massimo conservato vengono soppressi. Ogni componente
produce onset/offset dai frame estremi, banda dagli estremi di banda, centro
come media geometrica dei centri pesata dalle confidence, confidence uguale al peak
e severity come media pesata della superficie severity.

Gli eventi pubblicati alla soglia operativa sono esattamente i candidati con
confidence calibrata almeno pari alla soglia; devono coincidere con questa
estrazione o la prediction e invalida. La PR-AUC ordina l'intera lista
pre-threshold per `(confidence desc, frame, band)` e ripete il matching ai suoi
cut-point. Nessun top-k, floor di confidence o veto puo nascondere un candidato.
L'evaluator carica la policy per hash, ricalcola confidence, bundle, eventi e
flag `actionable` dai valori pre-calibrazione e rifiuta qualunque differenza con
la prediction. Una policy assente, non committata o fittata su un ruolo diverso
da `calibration` invalida il report.

## 10. Evaluator deterministico

L'evaluator legge soltanto manifest, annotation record e prediction record
con schema/versione/hash compatibili. L'ordine dei file non puo cambiare i
risultati. Tutte le aggregazioni pubblicano valore globale, macro per profilo,
macro per dominio e CI percentile 95% da 10000 bootstrap a livello `group_id`,
con PCG64 seed 20260719. Il campionamento conserva tutti gli asset figli del
gruppo estratto.

Salvo i rate exposure-aware dichiarati separatamente, ogni metrica primaria
viene calcolata prima per `evaluation_unit_id`, poi mediata dentro `group_id` e
infine macro-mediata con peso uguale fra gruppi. Numero di asset, segmenti,
eventi o celle non aumenta il peso del gruppo. Le micro-medie vengono riportate
solo come diagnostica.

### 10.1 Metriche tonali

Per una prediction `p[120]` e target `t[120]`, i pesi sono:

```text
w[i] = annotation_confidence[i] * tonal_actionable_mask[i]
       * audible_mask[i]
```

`audible_mask[i]` vale 1 quando la mediana temporale della PSD dry della banda
nel segmento annotato e almeno -100 dBFS/Hz e non oltre 60 dB sotto il massimo
del segmento; altrimenti 0.

Metriche obbligatorie:

- MAE e RMSE pesate della curva;
- p95 dell'errore assoluto sulle celle attive;
- miglioramento residuo:
  `1 - RMSE(t - p) / max(RMSE(t), 1e-6)`;
- errore di segno sulle celle con `abs(t) >= 1 dB`;
- clean actionable rate: quota di gruppi `clean_for_action` con almeno un
  bundle marcato actionable e oltre 1 dB in qualunque regione udibile;
- copertura: quota di target actionable per cui il motore emette un bundle
  azionabile;
- MAE della severity `[0,1]` sui bundle semantici matched e Spearman rho se il
  supporto e almeno 10; sotto quel supporto rho e `N/A`.

Un record senza celle attive ha metriche di curva `N/A`, non zero; viene usato
per clean safety. Ogni macro-media pubblica anche il supporto ed esclude gli
`N/A` senza convertirli in PASS.

Una semantic region e matchabile solo con stesso tipo e IoU temporale almeno
0.3. Il matching per classe non usa una frequenza puntuale universale:

- Resonance: centro entro un terzo di ottava;
- Muddiness, Boominess, BoxyMidrange: overlap di banda almeno 0.5;
- Thinness, DullSound: stessa regione e stessa direzione spettrale;
- Harshness e Sibilance: overlap di banda almeno 0.5.

### 10.2 Metriche degli eventi

Un evento e matchabile solo se tipo uguale e IoU temporale almeno 0.3. Inoltre:

- Resonance: centro entro un terzo di ottava;
- Harshness/Sibilance: overlap di banda almeno 0.5.

Il matching e bipartito one-to-one con obiettivo lessicografico deterministico:
massimo numero di match validi, poi massima somma IoU, poi minima somma
dell'errore frequenziale in ottave, poi ordine crescente degli ID evento. Non
e ammesso un greedy dipendente dall'ordine dei record.
Metriche obbligatorie:

- precision, recall, F1 e area precision-recall per classe, macro a peso uguale
  sui gruppi con annotazione completa; un gruppo senza GT positivo ha PR-AUC
  `N/A` e contribuisce invece alla clean safety;
- errore centro in ottave per Resonance;
- errore onset e offset in millisecondi;
- MAE della severity `[0,1]` sugli eventi matched e Spearman rho con supporto
  almeno 10;
- falsi eventi al minuto su gruppi clean;
- durata e occupancy soltanto come diagnostica, mai come gate primario.

Le metriche evento vengono prima calcolate per gruppo usando le superfici dense
pre-threshold della sezione 9.3 e poi macro-mediate con peso uguale. Segmenti,
eventi o durate aggiuntive dello stesso gruppo non aumentano il peso del gruppo.
Si riportano anche le micro-metriche come diagnostica, ma non possono promuovere
una classe.

Nessun singolo file, singolo hit o threshold scelto sullo stesso split puo far
passare una classe.

### 10.3 Calibrazione delle confidence

Tonal e anomaly hanno calibratori, report e parametri separati. I calibratori
si fittano solo su `calibration`; ECE, Brier e PR-AUC vengono pubblicati su
`development-metric` e poi, una sola volta, su `final-test`.

Per la confidence tonale, ogni cella udibile e un esempio binario actionable/
non-actionable. Per anomaly, ogni cella valida delle superfici dense e un
esempio: target uno dentro un evento GT del tipo e zero fuori, ma soltanto
quando il tipo e in `complete_types`. I ground-truth mancati restano FN nelle
metriche evento.

Ogni gruppo riceve peso totale uno. Dentro un gruppo, ciascun
`evaluation_unit_id` unico riceve peso `1 / num_units`; dentro l'unita, il peso
viene diviso uniformemente fra le celle eleggibili. Duplicati, crop e derivati
con lo stesso `evaluation_unit_id` vengono deduplicati prima del fit e della
misura. Questo schema di pesi e identico per fit del calibratore, ECE e Brier.

ECE usa 15 bin equal-mass: stable sort per `(confidence, group_id,
evaluation_unit_id, frame_or_band_id)`, poi partizione per peso cumulativo con
deviazione minima da `1/15`. Ogni bin usa accuratezza e confidence pesate; ECE
e la somma `mass_bin * abs(accuracy_bin - mean_confidence_bin)`. Brier e la
media pesata di `(confidence - target)**2`. Bin vuoti non vengono creati e
supporto zero e `N/A`, mai PASS.

### 10.4 Copertura minima per calibrazione e misura

L'unita indipendente di supporto e sempre `group_id`. Celle, eventi, crop,
derivati e asset multipli dello stesso gruppo aumentano il numero di esempi ma
non il supporto indipendente. I conteggi vengono pubblicati prima di fittare
qualunque calibratore. La stessa annotazione non puo essere usata come positivo
e negativo per la stessa famiglia.

La calibrazione globale e ammessa soltanto con questa copertura minima:

| Famiglia | Positivi indipendenti | Negativi indipendenti | Copertura obbligatoria |
|---|---:|---:|---|
| Tonale | almeno 50 gruppi con almeno una cella `tonal_actionable_mask` vera e target non zero, di cui almeno 20 con cella positiva e 20 con cella negativa | almeno 50 gruppi `clean_for_action` | almeno 3 actionable e 3 clean per ciascuno dei sette profili |
| Resonance | almeno 30 gruppi con evento GT actionable | almeno 50 gruppi senza evento GT della classe, di cui almeno 30 clean | positivi da almeno 3 `source_family`, almeno 5 gruppi ciascuna e nessuna oltre il 50%; negativi almeno 3 per profilo |
| Harshness | almeno 30 gruppi con evento GT actionable | almeno 50 gruppi senza evento GT della classe, di cui almeno 30 clean | positivi da almeno 3 `source_family`, almeno 5 gruppi ciascuna e nessuna oltre il 50%; negativi almeno 3 per profilo |
| Sibilance | almeno 30 gruppi con evento GT actionable | almeno 50 gruppi senza evento GT della classe, di cui almeno 30 clean | positivi da almeno 3 `source_family`, almeno 5 gruppi ciascuna e nessuna oltre il 50%; negativi almeno 3 per profilo |

Un gruppo multi-label puo contribuire al supporto positivo di piu classi, ma
una sola volta per classe. Un gruppo negativo per una classe puo contenere
un'altra anomalia soltanto se l'annotazione esclude esplicitamente la classe in
esame; l'assenza di annotazione non vale come negativo. Le direzioni positive e
negative della curva sono sotto-strati dello stesso supporto actionable: non
valgono come gruppi negativi e un gruppo si conta una sola volta nel totale 50.

I calibratori restano globali salvo prova contraria in G4. Una soglia o un
calibratore specifico per profilo, dominio o sottogenere e vietato se quello
strato non contiene almeno 30 gruppi positivi e 30 negativi nello split
`calibration`. Se il supporto manca, la classe o lo strato e `N/A` e non puo
contribuire a un GO; non si accorpano split e non si abbassano i minimi.

Su `development-metric` e `final-test` il supporto minimo dipende dalla metrica:
curve e coverage tonali richiedono almeno 30 gruppi actionable; clean actionable
rate almeno 149 gruppi clean; ogni classe anomaly almeno 30 gruppi positivi e
l'intero pool clean-safety da almeno 149 gruppi/minuti della sezione 11. Si
applicano inoltre i floor della sezione 11 e la sua power analysis.
Supporto inferiore produce `N/A` e blocca ogni claim o gate che dipende da quella
metrica. ECE/Brier possono essere riportati anche con supporto maggiore di zero,
ma non valgono come prova di calibrazione finche questi minimi non sono
raggiunti.

### 10.5 Comparabilita delle baseline e semantica `N/A`

Un sistema viene confrontato soltanto sulle superfici che produce realmente
con lo stesso schema, oppure tramite un adapter congelato e controfirmato prima
di aprire `development-metric`. L'assenza strutturale di curve tonali, eventi,
frequenza o severity nel Motore v2 vale `N/A`, non zero, infinito o FAIL.

`N/A` non entra in macro-medie, non soddisfa un gate e non dimostra un
miglioramento. Le claim relative a v2 usano soltanto metriche omologhe o un
surrogato preregistrato; le nuove capacita v3 vengono valutate in assoluto e
contro la baseline deterministica G2. Se invece un candidato dichiara una
superficie ma non emette una prediction valida, l'esito e fail-closed: FN,
errore di schema o fallimento del candidato secondo il caso, mai `N/A`.

Il gate G2 rispetto a v2 usa un solo adapter omologo, congelato in G1c, sulle
sei classi realmente attive in tutti e tre i candidati G0: `Resonance`,
`Muddiness`, `Boominess`, `Thinness`, `BoxyMidrange`, `DullSound`.
`Harshness` e `Sibilance` sono mascherate nelle provenance G0 e restano `N/A`
nel confronto v2; v3 le deve superare con i gate assoluti e contro G2, mai
trattandole come negativi v2. Il denominatore della macro-F1 e sempre sei; una
delle sei classi con supporto insufficiente rende il gate NO-GO.

Per ogni segmento completo con almeno 20 finestre v2, l'adapter usa i centri
fisici delle finestre prodotte dal frontend G0 con `window_step=16` come griglia
temporale comune. A ogni centro:

- v2 vale uno se la probabilita supera la soglia della provenance del seed;
- v3 vale uno se il centro cade nel supporto temporale di un bundle o evento
  `actionable` della classe; un bundle statico copre il segmento prediction;
- GT vale uno se il centro cade in una semantic region o evento GT actionable.

La presenza di segmento e uno per ciascuno dei tre vettori soltanto quando la
rispettiva occupancy sulla stessa griglia e almeno 5%. Un segmento con meno di
20 centri e `N/A` per l'adapter omologo, ma resta disponibile alle metriche v3
native. Ogni classe richiede separatamente almeno 30 gruppi GT positivi e 30
negativi sia su development sia su final-test.

L'adapter calcola macro-F1 a sei classi e false-positive group rate sui gruppi
clean, senza inventare curve, frequenze o severity per v2. Ognuno dei tre
candidati G0 viene valutato separatamente con model, provenance, mask e soglie
congelati. G2 deve superare ciascun seed: macro-F1 almeno 10% relativo, oppure
almeno +0.10 assoluto quando il seed v2 e sotto 0.10, e false-positive group
rate non superiore allo stesso seed. Non e ammesso comporre una baseline con la
metrica migliore di un seed e la safety migliore di un altro. G1a serializza
mapping, costanti e hash gia definiti qui; non puo sceglierli o modificarli.

## 11. Matrice benchmark e potenza statistica

### 11.1 Famiglie congelate

Il benchmark non e un unico pool e non usa il numero di file come prova di
indipendenza. Ogni asset appartiene a una o piu famiglie dichiarate nel
manifest, ma il supporto statistico e sempre contato per `group_id`.

| Famiglia | Contenuto | Metriche primarie | Copertura minima prima della power analysis |
|---|---|---|---|
| `tonal-controlled` | dry group-disjoint con 1-3 trasformazioni note, includendo boost, cut, shelf, bell e interazioni | RMSE/p95 della curva, miglioramento residuo, errore di segno | almeno 5 parent group per profilo; ogni regione tonale e direzione compare in almeno 10 gruppi |
| `tonal-natural` | materiale reale clean e materiale reale con curva actionable annotata e adjudicata | RMSE/p95, coverage, clean actionable rate, severity | almeno 5 clean e 5 actionable group per profilo |
| `anomaly-natural` | Resonance, Harshness e Sibilance reali con intervallo, banda e severity annotati | PR-AUC, F1, errore frequenziale/temporale, severity, falsi/min | almeno 30 positive group per classe; i negativi usano il pool clean-safety completo |
| `clean-safety` | materiale reale intenzionalmente corretto, incluso materiale colorato ma non problematico | clean actionable rate e falsi eventi/min | almeno 149 group totali e 10 per profilo, ciascuno con almeno un minuto clean eleggibile per i tre tipi anomaly; nessun negativo implicito |
| `electronic-stratified` | vista trasversale delle quattro famiglie per techno, house, breakbeat e altri sottogeneri dichiarati | stesse metriche della famiglia madre, riportate per sottogenere | almeno il 40% del final-test complessivo; nessuna claim di sottogenere senza supporto determinato dalla power analysis |

I derivati controlled ereditano il gruppo del dry e non moltiplicano il
supporto. Il set `tonal-controlled` usa parent group sigillati e una griglia di
parametri preregistrata. La partizione di holdout opera su celle del piano
fattoriale `(sequenza trasformazioni, regione, direzione, gain, Q)`, non soltanto
su seed o asset: nessuna cella final-test compare nel generatore di training e
almeno una composizione multi-trasformazione completa e riservata al benchmark.
`tonal-natural` e obbligatorio: il sintetico da solo non puo dimostrare
trasferimento al reale.

Le regioni tonali canoniche, inclusive a sinistra ed esclusive a destra salvo
l'ultima, sono `[20,80)`, `[80,200)`, `[200,500)`, `[500,2000)`, `[2000,5000)`,
`[5000,10000)` e `[10000,20000]` Hz. La griglia controlled deve coprire boost e
cut in ciascuna regione; un asset con piu trasformazioni resta una sola unita
del parent group.

Il sotto-set competitor di G5/G8 e una selezione preregistrata dalle famiglie
`tonal-natural`, `anomaly-natural` e `clean-safety`; non costituisce una sesta
famiglia e non puo cambiare i loro conteggi dopo l'apertura.

### 11.2 Piano di potenza preregistrato

La numerosita finale di ogni famiglia e strato e:

```text
n_required = max(floor_contrattuale, n_power)
```

`n_power` viene stimato soltanto dal sottoinsieme `development_pilot`, mai da
`final-test`, con unita di ricampionamento `group_id` e test paired quando i
sistemi condividono la sorgente. Il pilot non viene usato per il point-estimate
del gate development dello stesso round e deve avere almeno 30 gruppi paired
per una metrica continua/macro e almeno 15 gruppi GT positivi per ogni classe
anomaly; per stimare l'overdispersione false-events servono almeno 30 gruppi
clean con un minuto eleggibile per classe. Supporto inferiore richiede nuovi
batch, non una diversa selezione.

La decisione finale controlla alpha family-wise 0.05 con Holm su tutti i gate
primari dello stesso report. Per dimensionare il campione si usa invece il
conservativo `alpha_plan = 0.05 / m`, dove `m` e il numero congelato di gate
primari: e il piu piccolo livello possibile nella procedura Holm. La potenza
minima e 0.90. Numero e ordine dei gate sono congelati prima del calcolo. Il
piano registra seed, pilot SHA-256, statistico, orientamento, effect size minimo,
`m`, `alpha_plan`, supporto, risultato e versione dell'implementazione.

L'effetto minimo di interesse e preregistrato per metrica:

- errore di curva comparabile: riduzione relativa almeno 10% rispetto alla
  baseline pertinente, senza regressione clean;
- macro-F1 o PR-AUC comparabile: aumento relativo almeno 10%, oppure +0.10
  assoluto quando la baseline e inferiore a 0.10;
- clean actionable rate: ipotesi nulla `p >= 0.02`, alternativa di progetto
  `p = 0.01`, limite superiore esatto unilaterale sotto 0.02;
- falsi eventi/min: ipotesi nulla `lambda >= 0.5`, alternativa di progetto
  `lambda = 0.25`, limite superiore Poisson esatto e limite cluster-bootstrap
  entrambi non oltre 0.5;
- parity e invariance: restano gate deterministici, non sono sostituiti da una
  power analysis;
- G8: numerosita ascoltatori e sorgenti resta determinata dal pilot G5 e dal
  modello clusterizzato definito nel piano principale.

Per metriche continue e macro-F1/PR-AUC, il tool usa 10000 simulazioni PCG64. A
ogni numerosita candidata ricampiona con replacement i record completi di
gruppo paired, ricalcola lo statistico, centra la distribuzione bootstrap e la
trasla esattamente dell'effetto minimo dichiarato; non usa la media favorevole
osservata come alternativa. La potenza e la quota di simulazioni che supera il
critico unilaterale sotto la distribuzione centrata nulla ad `alpha_plan`. Si
sceglie il primo `n` con potenza almeno 0.90; seed base `20260719`, derivato per
metrica con SHA-256 del suo ID canonico.

Per clean actionable rate, `n_power` e il primo `n` per cui il test binomiale
esatto unilaterale di `p >= 0.02` ha size non oltre `alpha_plan` e potenza almeno
0.90 a `p = 0.01`; resta comunque il floor di 149 gruppi.

Il tempo clean eleggibile e l'unione degli intervalli coperti da frame G1 validi
nei segmenti `clean_for_action` con il tipo in `explicit_negative_types`.
Silenzio, frame invalidi, warm-up, overlap e derivati duplicati non entrano nel
denominatore. Per ciascun gruppo e classe si usa esattamente il primo minuto
eleggibile in ordine temporale canonico; un gruppo con meno di un minuto non
entra nel pool false-events. Quindi `n` gruppi producono esattamente `n` minuti
indipendenti e nessun asset lungo domina l'esposizione.

La potenza false-events e congiunta. Dal pilot si costruisce per ogni classe il
vettore di conteggi sui minuti standard. Si stima soltanto l'overdispersione con

```text
mu_pilot = mean(counts)
alpha_nb = max(0, (sample_variance(counts) - mu_pilot)
                  / max(mu_pilot**2, 1e-12))
```

`sample_variance` usa il denominatore `n - 1`. Si fissa sempre la media
alternativa a `lambda = 0.25`, senza usare la media osservata. Per ogni
`n >= 149`, 5000 simulazioni PCG64 estraggono `n` rate di
gruppo da `Gamma(shape=1/alpha_nb, scale=0.25*alpha_nb)` e poi conteggi
`Poisson(rate)`; con `alpha_nb = 0` usano direttamente `Poisson(0.25)`. Ogni
simulazione calcola il limite Poisson esatto e, con 2000 resample interni dei
gruppi, il quantile `1 - alpha_plan` della media dei conteggi da un minuto.
Entrambi i limiti sono quindi espressi in eventi/minuto. Il seed interno e
derivato dai primi 8 byte di
`SHA256(metric_id + NUL + outer_simulation_index)`. Si sceglie il primo `n` per
cui almeno il 90% delle simulazioni soddisfa entrambi i limiti `<= 0.5`. Il gate
finale usa il limite esatto e 10000 resample per gruppi con il seed evaluator
sul pool reale. Con zero eventi il limite Poisson simultaneo e
`-log(alpha_plan) / n`, mai zero. La dimensione del pool clean-safety e il
massimo fra 149, `n_power` binomiale e i tre `n_power` false-events; lo stesso
pool deve soddisfare tutti e quattro i gate.

Il risultato viene congelato in `benchmark_power_plan.json` con SHA-256 prima
di aprire `final-test`. Se il corpus disponibile non raggiunge `n_required` o
l'esposizione richiesta, la famiglia e NO-GO: non si riduce l'effetto, non si
contano crop come gruppi e non si trasferiscono asset da altri ruoli. Tutti i
risultati pubblicano supporto, intervallo di confidenza e numero di gruppi
esclusi con motivo. Non e ammesso scegliere a posteriori il metodo che produce
il campione minore.

## 12. Protocollo competitor

Il protocollo viene congelato in G1 ma usato per claim soltanto in G5/G8.

G5 usa gruppi dichiarati in anticipo dentro `development-metric`: il risultato
e un veto su artefatti/workflow e puo causare una nuova iterazione, quindi non
ha valore di final-test. G8 usa gruppi `final-test` mai aperti prima. I due set
non condividono group, dry o derivati.

- dry e target final-test sono sigillati prima dei render;
- stessa sorgente, stessa regione e stesso obiettivo per tutti i sistemi;
- versione plugin, modalita, profilo, quality mode e preset iniziale sono
  registrati; niente ritocco manuale non dichiarato;
- render float32 48 kHz, latency-compensated, senza clipping;
- loudness matching entro 0.1 LU sul segmento valutato, con guadagno applicato
  registrato;
- nomi randomizzati e metadati rimossi per l'ascolto cieco;
- output competitor vietato a train, validation e calibration; nel sotto-set
  G5 di development-metric puo produrre soltanto GO/NO-GO e report qualitativo,
  mai target, feature, threshold o supervisione;
- hash SHA-256 per dry, render, configurazione e tabella di randomizzazione.

Il confronto numerico usa l'effetto applicato, non il nome del problema:

- curva statica: mediana temporale di
  `mid_psd_db(processed) - mid_psd_db(dry)` sulle celle allineate e udibili;
- azione dinamica: la stessa differenza frame-per-frame sulla griglia G1;
- artefatti: overshoot, pumping, variazione stereo e loudness residuo.

Il loudness matching usa ITU-R BS.1770-4 con gating integrato sul segmento
comune. Dry e render vengono prima allineati per latenza e poi troncati alla
stessa durata; nessun time-stretch e ammesso.

I render competitor final-test restano sigillati fino a G8. La non inferiorita
umana resta G8; G1 verifica soltanto che il protocollo sia riproducibile e non
contaminante.

## 13. Fixture e gate G1

Le fixture sono generate da formule, non registrate a mano:

- multitone ai centri critici 45, 60, 80, 250, 1000, 3500, 8000, 16000 e
  20000 Hz;
- sweep log 20-20000 Hz;
- pseudo-rumore continuo band-limited: somma di 512 sinusoidi con frequenze
  `20 + (20000-20)*(k+0.5)/512`, fase PCG64 seed 31051986 e ampiezza RMS
  complessiva -24 dBFS; ogni ampiezza di picco vale
  `10**(-24/20) * sqrt(2/512)` e il segnale e valutato analiticamente ai tre
  sample rate;
- transient burst e risonanza smorzata;
- a 96 kHz, toni ultrasonici a 28000, 32000 e 40000 Hz;
- stereo mid-only, side-only e decorrelato;
- silenzio e input non finiti per i gate fail-closed.

Ogni segnale continuo viene renderizzato direttamente a 44.1, 48 e 96 kHz;
non si crea una variante ricampionando un'altra variante. I confronti usano
frame con timestamp fisico comune, dopo warm-up e prima della coda congelati
come sotto, e sole celle attive secondo il predicato di attivita di gate 4.

### 13.1 Warm-up, coda e porzioni stazionarie (fail-closed)

Warm-up e coda **non** sono parametri liberi del manifest post-generazione.
Sono formule chiuse, congelate in G1a e hashed nel lock; il manifest puo solo
ripetere i valori derivati dalle formule. Modificare warm-up/coda dopo la
generazione delle fixture per restringere la finestra di confronto e FAIL.

Con hop canonico `H = 1024`, `fs_c = 48000`, lunghezza minima LF
`N_LF = 8192` e `resampler_group_delay_seconds` come in §5:

```text
warm_up_seconds = resampler_group_delay_seconds + N_LF / fs_c + K_wu * H / fs_c
coda_seconds = K_coda * H / fs_c
```

con `K_wu = 4` e `K_coda = 4`. La somma e **additiva** (non un `max` fra i
due rami): `N_LF / fs_c` porta al primo frame LF-capable e `K_wu * H / fs_c`
esclude **ulteriori** quattro hop oltre quel punto, cosi shape, prominence e
delta non confrontano il bordo in cui la storia e azzerata e la fusione dual-
resolution e appena diventata disponibile. `K_coda = 4` simmetrizza il
trailing edge senza padding finale (§4.1). I campioni esclusi sono quelli con
`source_time < warm_up_seconds` oppure
`source_time > T_asset - coda_seconds`, dove `T_asset` e la durata fisica
dell'asset alla griglia `source_time`.

Per i confronti cross-SR (44.1 / 48 / 96 kHz) la finestra utile e
l'**intersezione** dei segmenti utili dopo warm-up e prima della coda su
ciascun rate (equivalente a usare il massimo dei warm_up e il massimo delle
code sulla stessa `source_time`); confrontare tratti non comuni e FAIL.

**Porzione stazionaria** (multitone e rumore): e l'intero segmento utile dopo
warm-up e prima della coda (**modalita (a)**, default obbligatorio per i gate
di sample-rate parity su multitone e rumore). Non e ammesso un sottoinsieme
post-hoc. In alternativa (**modalita (b)**), e soltanto se preregistrata in
G1a e hashed nel lock **e** dichiarata diagnostica / non usata per chiudere
il gate 4 di parity, una collezione di finestre che soddisfano tutte:

- durata almeno `T_min = 8 * H / fs_c` (otto hop);
- predicato di stabilita preregistrato: varianza della `mid_psd_db` media
  sulle 120 bande, calcolata hop-per-hop nella finestra, `<= 1.0 dB^2`, e
  max |Δ| hop-to-hop della stessa media `<= 0.5 dB`.

La scelta fra (a) segmento utile intero e (b) collezione di finestre e
fissata in G1a prima della generazione e hashed; non si cambia modalita dopo
aver visto un FAIL. Per chiudere il gate 4 su multitone/rumore vale solo (a).
Tutte le finestre della collezione preregistrata in (b) entrano nel max |Δ|
diagnostico; omettere una finestra fallita e FAIL. Se nessuna finestra
soddisfa il predicato, il report diagnostico e FAIL (non si allarga `T_min`
ne si alza la soglia di varianza dopo aver visto i numeri).

**Sweep log**: il gate di parity non usa "porzioni stazionarie" libere. Usa
esclusivamente la griglia di checkpoint preregistrata in G1a (hashed), con
almeno i centri critici di §13 (45, 60, 80, 250, 1000, 3500, 8000, 16000,
20000 Hz) piu gli estremi 20 e 20000 Hz del path; per ciascun checkpoint si
confronta il frame il cui `source_time` e piu vicino all'istante in cui lo
sweep attraversa quella frequenza, entro al piu un hop. Se nessun frame cade
nel raggio di un hop (`min |source_time - t_cross| > H / fs_c`) → FAIL
(checkpoint missing). Nessun sottoinsieme libero dei checkpoint e ammesso:
manca un checkpoint o uno fallisce → FAIL.

Transient burst e risonanza smorzata restano fuori dal confronto dB di
parity spettrale: hanno il gate separato su onset, picco e decadimento gia
previsto al punto 4, per evitare che un arrotondamento frazionario venga
nascosto in un confronto dB non omologo.

### 13.2 Gate obbligatori

Gate obbligatori:

1. **Struttura**: 120 centri, estremi 20/20000 Hz, ordine stretto, timestamp e
   forme esatte.
2. **Determinismo**: due processi puliti producono artefatti byte-identici
   sulla stessa piattaforma e stesso lock.
3. **Streaming ≡ offline**: l'equivalenza non si dimostra con "chunk casuali"
   ad hoc. G1a congela e hasha nel lock:
   - seed PRNG fisso `PCG64(20260719)` (stesso seed dell'evaluator §10);
   - insieme minimo di schedule di chunk size (campioni host di input):
     `1`, `63`, `1024`, `4095`, `8192`, `8193`, piu una schedule geometrica
     casuale `floor(2 ** U)` con `U ~ Uniform[0, 14)` estratta dal PRNG
     preregistrato, di lunghezza almeno 32 chunk, ripetuta identica su ogni
     asset del set di gate.
   Per ogni schedule, l'input a chunk e l'input monolitico offline sullo
   stesso lock/piattaforma devono produrre la stessa sequenza di
   `V3FeatureFrame`: tutti i campi float32 del frame (§7), i timestamp
   razionali (`source_time_num`/`source_time_den`, `frame_end_sample`,
   `frame_index`) e i flag `mid_valid`/`side_valid`/`valid` (e il motivo
   enumerato). Obbligatori inoltre: (a) concatenazione multi-asset con reset
   esplicito della storia `delta_db` al confine asset; (b) silenzio
   intercalato fra asset; (c) identita streaming-vs-offline sullo stesso
   lock e piattaforma. Preferenza e regola di gate sulla **piattaforma di
   gate G1** (OS, arch, stack/numpy dichiarati e hashed nel lock del report):
   artefatti canonici **byte-identici** fra offline e streaming; la
   bit-identita float e **obbligatoria** su quella piattaforma. La tolleranza
   max |Δ| assoluto `<= 1e-6` su ogni float32 (con timestamp razionali
   identici e bit dei flag `valid` identici) e ammessa **solo** per
   piattaforme secondarie **preregistrate** nel lock G1a, resta report-only
   e **non** chiude il gate G1. Dichiarare ad hoc "piattaforma non
   bit-identical" senza allowlist preregistrata → FAIL.
4. **Sample-rate parity**: rispetto al render 48 kHz, su 44.1 e 96 kHz, e
   sulle porzioni definite in §13.1,
   `max_i |x_i(sr) - x_i(48k)| <= 0.25 dB` dove `x` scorrono **ogni**
   elemento del dominio dB dichiarato:
   `mid_psd_db[120]`, `side_psd_db[120]`, `mid_shape_db[120]`,
   `side_shape_db[120]`, `mid_prominence_db[120]`,
   `side_prominence_db[120]`, e gli scalari `mid_level_dbfs` /
   `side_level_dbfs` quando il rispettivo canale e valido.
   Predicato di attivita (nessuna maschera post-hoc): una cella di PSD e
   attiva sse `max(psd_db_ref, psd_db_sr) > -120` (unione: strettamente sopra
   il floor `1e-12` / clamp di §6.1 sul riferimento 48 kHz **oppure** sul
   render sotto test). Cosi un artefatto presente solo a 44.1/96 kHz non
   scompare dal max |Δ|. Shape e prominence della stessa banda ereditano
   l'attivita della PSD del medesimo canale; i vettori di un canale con
   `*_valid == false` restano ignorati come in §7. Questo predicato e del
   solo gate SR; i criteri −100 dBFS/Hz di §10 evaluator restano invariati
   (gate diversi).
   **Una sola cella attiva fuori soglia → FAIL dell'intero gate.** Non si
   sostituisce il max con media, p95, RMSE o sottoinsieme di bande scelto
   dopo aver visto gli errori. `mid_delta_db` / `side_delta_db` **non**
   entrano in questo gate 0.25 dB (il delta e un derivato temporale gia
   coperto da gain invariance `<= 0.05 dB` e dall'identita streaming≡offline);
   includerli nel max SR sarebbe un dominio diverso e non li si usa per
   mascherare un fallimento su PSD/shape/prominence. Timestamp entro un
   campione canonico. Sui transienti: onset e frame di picco entro un
   campione canonico e tempo di decadimento entro un hop, senza confronto
   ottenuto spostando manualmente i frame. I sample rate 88.2 / 176.4 /
   192 kHz restano sperimentali (§4.1) e non possono chiudere questo gate.
5. **Gain invariance**: a -12, -6, +6 e +12 dB senza clipping, shape,
   prominence e delta hanno max delta `<= 0.05 dB`; PSD e level traslano del
   gain applicato con errore `<= 0.05 dB`.
6. **M/S**: mono e stereo dual-mono hanno mid equivalente `<= 0.05 dB`; side
   dual-mono resta non valido e non genera azioni; stereo anti-fase ha mid non
   valido ma side e frame globale validi.
7. **Anti-alias**: i toni ultrasonici a 96 kHz non producono componenti alias
   fra 20 e 20000 Hz oltre -80 dB rispetto al tono di ingresso.
8. **Split**: zero overlap di group, SHA e parent fra tutti i ruoli; ogni
   derivato eredita il ruolo del parent; nessun fallback. Commitment errato,
   reveal anticipato, ID rinominato, roster parziale, retry del salt o filtro
   post-role devono fallire.
9. **Evaluator**: fixture perfetta produce metriche perfette; prediction vuota,
   classe errata, frequenza errata, segno invertito e duplicati producono i
   fallimenti attesi; permutare righe o duplicare un `evaluation_unit_id` non
   cambia il report. Superficie anomaly mancante, score thresholded e zero
   eventi con esposizione insufficiente devono fallire. Policy assente/hash
   errato, threshold mutato o actionable non riproducibile devono fallire; le
   fixture power devono provare che il gate congiunto non usa la sola Poisson.
10. **Ambiente**: sync del lock con hash, test completi e deep hash di tutte le
   fixture PASS.

La soglia 0.25 dB e hard sul **max** assoluto del dominio dichiarato al
punto 4. Non si sostituisce con una media o un p95 per nascondere una banda
fuori contratto.

Gli artefatti numerici canonici sono array little-endian `.npy` senza oggetti
e JSON UTF-8 **senza BOM**, con chiavi ordinate per code point Unicode,
separatori esatti `,` e `:` (niente spazi), newline LF finale **singola
inclusa nell'hash**, `allow_nan=false` / soli float finiti. In lettura si
rifiutano token `NaN`/`Infinity`/`-Infinity`, overflow tipo `1e400` e chiavi
oggetto duplicate. Si ordinano **solo** i set dichiarati order-independent;
curve, score e griglia 120 bande restano nell'ordine semantico. Archivi
ZIP/NPZ con timestamp non sono usati come prova byte-identica.

## 14. Implementazione e commit atomici dopo il GO

Ordine obbligatorio:

1. **G1a - contract artifacts**: JSON schema di manifest, admission batch,
   annotazione, prediction, calibration policy e piano di potenza; griglia 120
   bande, split roles, commit-reveal, copertura calibration, mapping adapter
   v2-v3, generatori fixture e hash. Nessun frontend ancora.
2. **G1b - canonical frontend**: resampler streaming, dual-resolution
   time-aligned, `V3FeatureFrame` Python e unit test.
3. **G1c - evaluator**: parser fail-closed, matching, metriche, CI group-level,
   adapter omologo v2-v3 e fixture di errore.
4. **G1d - competitor protocol harness**: manifest/config/hash e verifica dei
   render; nessun render proprietario nel repository.
5. **G1e - report**: esecuzione completa dei gate, hash degli output e tabella
   PASS/FAIL.

Dopo ogni commit:

```bash
git diff --cached --check
python -m compileall -q ml_v3
python -m unittest discover -s ml_v3/tests -p 'test_*.py'
git diff --name-only 2c88edad -- Source ml_v2 CMakeLists.txt Resources AIEQ-mac
```

L'ultimo comando deve produrre output vuoto. I comandi specifici delle fixture
saranno definiti da G1a e poi riportati senza abbreviazioni nel report.

## 15. Stop condition e rollback

G1 e NO-GO se si verifica uno solo dei seguenti casi:

- sample-rate parity o streaming parity non raggiunti senza rilassare il gate;
- LF e MAIN non condividono lo stesso timestamp;
- uno split permette fallback, overlap o riassegnazione retroattiva;
- il preflight G1 accetta una fixture calibration, development o final-test che
  non raggiunge supporto o esposizione richiesti. La disponibilita del corpus
  reale viene invece verificata in G3/G4 e non blocca l'implementazione G1;
- il piano di potenza usa file/crop come unita indipendenti, legge final-test o
  viene modificato dopo la sua apertura;
- fit o metriche cambiano duplicando celle, segmenti o derivati nello stesso
  gruppo;
- un sistema puo omettere score anomaly pre-threshold senza invalidare lo
  schema;
- calibratore, soglia o decision policy possono cambiare senza cambiare hash o
  senza invalidare prediction e report;
- il dimensionamento false-events garantisce potenza soltanto per uno dei due
  limiti richiesti dal gate congiunto;
- una metrica dipende dall'ordine dei file o usa lo split che calibra;
- final-test viene letto per scegliere una decisione;
- una dipendenza non e bloccata con hash;
- serve modificare runtime o `ml_v2` per far passare G1.

Rollback: si elimina il branch/commit G1 non promosso e si torna al commit G0
`2c88edad`. Nessun altro branch richiede ripristino.

## 16. Criterio di approvazione

Il reviewer deve verificare questo contratto su commit immutabile e restituire
una sola lista consolidata. Il GO richiede:

- nessuna contraddizione con `docs/MOTORE_V3_PLAN.md`;
- specifiche implementabili senza decisioni aperte nascoste;
- nessun percorso di leakage o contaminazione del final-test;
- metriche separate per bilanciamento tonale e anomalie dinamiche;
- supporti calibration clean/positivi e benchmark group-level falsificabili;
- baseline non omologhe trattate come `N/A`, mai come vittorie artificiali;
- gate abbastanza severi da rendere falsificabili le claim successive;
- diff del commit limitato a questo documento e, se presente per coerenza
  letterale del max |Δ|, alla sola riga G1 di parity in
  `docs/MOTORE_V3_PLAN.md`;
- nessun path `ml_v3/contracts/`, frontend, fixture, test harness o altro
  codice G1 nello stesso commit document-only.

Fino a quel GO: nessun file G1 di codice, fixture o ambiente viene creato.
Questa REVISIONE 6 CONSOLIDATA + micro-amend non costituisce GO a G1a.
```

---

## B.3 FILE: `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md (CANDIDATE — NOT consolidated)`

**Path logico:** `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`  
**Bytes:** 13399  
**Lines:** 272

```markdown
# Motore v3 — CONTRACT REV7 CANDIDATE (document-only)

**Status:** CANDIDATE DRAFT — **NOT CONSOLIDATED** — ≠ amend of freeze `6d254d0a`  
**Date:** 2026-07-25  
**Authority:** Guardian GO (scoped draft) after G1b spike WS4 RED + independent CC  
**Spike tip (evidence):** `c7f05871` @ `spike/motore-v3-g1b-frontend`  
(`motore-v3-g1b-spike` worktree)

**This document does NOT:**
- consolidate REV7 into `docs/MOTORE_V3_G1_CONTRACT.md`;
- authorize silent edit of `metrology_lock.json` / SHA256SUMS;
- claim G1 PASS or product G1b tip;
- relax the 0.25 dB hard max;
- invent a replacement numeric threshold by fitting spike RED cells (9.537 dB).

**This document DOES:**
- record the falsifiable impossibility of a **specific REV6 clause**;
- authorize only the next step: design the replacement admission predicate
  under the constraints below, then redteam + re-measure, then a **second**
  Guardian GO before any consolidate.

---

## 1. Trigger (falsifiable impossibility)

Under REV6 §13.2 gate 4, with implementation faithful to §5/§6/§7 and a-priori
pins P1–P7 (unchanged; `changed_to_pass: false`), the G1b adversarial spike
measures:

| evidence | value |
|----------|--------|
| overall `max\|Δ\|` | **9.537 dB** (threshold 0.25 dB) |
| worst cell | `pseudo_noise@44100`, `mid_shape_db[18]` (~56.9 Hz) |
| all 6 SR cells | RED |
| pins rewritten to pass? | **no** |
| streaming / proof (b) | PASS on spike schedules |

Artifacts:
- `ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.md` (spike worktree tip `c7f05871`)
- `ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.json`
- harness: `ml_v3/benchmark/sr_parity.py` (implements lock predicate literally)

Independent CC (not stored as hashed G1a artifact) additionally showed that
**FFT bins within ~20 dB of the spectral peak** between 48 kHz direct and
44.1→48 resampled audio agree to ~**0.03 dB** — i.e. the resampler/frontend
path is not the failure mode on signal-bearing bins. That corroborates the
amend **direction** (admission, not 0.25). It is **not** a partial PASS and
must **not** be used to pick a new numeric cut.

---

## 2. Clause that fails (narrow)

REV6 §13.2 gate 4 currently states (product freeze `6d254d0a`):

> Predicato di attivita (nessuna maschera post-hoc): una cella di PSD e
> attiva sse `max(psd_db_ref, psd_db_sr) > -120` (unione: strettamente sopra
> il floor …). … Shape e prominence della stessa banda ereditano l'attivita
> della PSD del medesimo canale. … **Una sola cella attiva fuori soglia →
> FAIL dell'intero gate.** … `max_i |x_i(sr) - x_i(48k)| <= 0.25 dB`

**What is impossible under pinned degrees of freedom:** requiring
`max|Δ| ≤ 0.25 dB` on the **full set of cells admitted by**
`max(psd_ref, psd_sr) > -120` when that set includes cells whose energy is
dominated by **inter-component window leakage / interference** (and
near-floor union one-sided activations), especially on P2 single-bin /
LF-pure geometry (~50–70 Hz). Those cells are construction- and
sub-sample-phase-sensitive; no remaining implementable freedom (window,
FFT sizes, band geometry, fuse, resampler coeffs, P1–P7) removes the
failure without amending admission or illicitly relaxing the threshold.

**What is NOT claimed impossible:** the 0.25 dB hard max on cells that
actually carry stable signal content. Spike + CC evidence points the other
way. **REV7 must not raise, replace, or soft-max the 0.25 dB threshold.**

Smoking-gun shape (from spike evidence JSON, illustrative of the clause):
- `log_sweep` peak: `psd_ref = -120.0`, `psd_sr ≈ -113.6` → admitted only via
  union; `|Δ| ≈ 6.4 dB` on `mid_psd_db`.
- `pseudo_noise` / `multitone` peaks: `psd_ref ≈ -119.0…-119.3` (a hair above
  floor) driving `mid_shape_db` deltas of several dB on single-bin bands.

---

## 3. Amend target (scoped)

### In scope (updated after metrology-redteam 2026-07-25)

**Original draft scope:** replace the gate-4 activity / domain-admission
predicate in §13.2 item 4 (+ lock string on consolidate).

**Scope reopen (judge-endorsed, redteam VERDICT CONTRACT-BROKEN):** the amend
target may also need to include **one or more** of:

- an explicit **domain** carve-out for geometrically unresolvable bands
  (preferred durable form for 0/1-bin material occupancy — pick **either**
  domain wording **or** predicate exclusion, not dual optional packaging);
- fail-closed rules for **shape/prominence coupling**: excluding a PSD cell
  from the max does **not** remove that band’s energy from §7’s global
  shape normalizer (`sum` over 120) nor from prominence’s ±16 kernel — so
  “inherit ACTIVE” alone is insufficient;
- a normative narrowing of candidate constraint 4 (ENBW-aperture-only) **or**
  an a-priori anti-leakage operator (disclaimer/residual ≠ satisfaction of a
  normative constraint);
- a corrected **MATERIAL** rule (equal-energy `W_MATERIAL` bound is not
  worst-case);
- hashed/pinned `N_BINS` / `ACTIVE` mask procedure + vacuous FAIL per
  §13.1 portion and valid channel.

Invalid channels remain ignored as §7. **0.25 dB hard max and max
aggregator remain immutable.**

**Product decision (Marco, 2026-07-25) — report-only LF:** geometric
domain `R` from ENBW `N_MIN=2` ∧ Rayleigh `SEPARATION_MIN_BINS=2` (scope
rewrite). Gate 4 closes only on `i ∈ R`. Bands `i ∉ R` are **report-only**:
measured and **must** be published (omit table → report FAIL); they do not
enter the gate-closing max. Explicit CONTRACT why-sentence required (grid
finer than window separation). G4 debt: cross-SR low-end detection
stability. Normative prose:
`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`. No LF alternate dB
tolerance. No option-1 blindness.

### Out of scope (forbidden in this REV7)

- Changing `0.25 dB` hard max or replacing max with mean/p95/RMSE.
- Dropping `shape` / `prominence` / `level` from the declared domain
  **without** a fail-closed replacement definition that closes coupling
  (redefinition of domain membership or of shape/prominence for gate 4 may
  be in scope; silent drop to hide FAIL is not).
- Post-hoc prominence clamp; changing aggregator; changing P1–P7.
- Raising `N_MIN` after a FAIL to chase PASS (threshold shopping).
- Changing §10 evaluator −100 dBFS/Hz criteria (different gate).
- Product frontend “fixes” under an unconsolidated candidate rule.
- Silent lock / SHA256SUMS mutation.

---

## 4. Replacement admission — design constraints (a priori)

The concrete formula is **not chosen in this draft** (to avoid threshold
shopping against the 9.537 dB cells). Any successor predicate MUST satisfy
all of the following **before** consolidate:

1. **A priori.** Written into CONTRACT (and then lock) **before** the
   re-measurement that claims PASS. Forbidden: pick cutoffs by scanning
   spike RED cells / margin tables until `max|Δ| ≤ 0.25`.
2. **No post-hoc mask.** Still forbidden to hide bands after seeing errors
   on a run (same spirit as REV6).
3. **Preserve one-sided artifact intent.** A defect that appears at 44.1/96
   but not at 48 must not vanish from the max solely because the reference
   sits at the floor. Pure “intersection both > −120” is **insufficient**
   as a complete replacement (multitone peaks have both sides above floor
   and still fail).
4. **Signal-bearing admission (REV7 perimeter — narrowed).** For this amend,
   admission is **ENBW-aperture resolvability** on every positive-weight
   fusion path plus the floor/union cut — stated in CONTRACT language
   **without** fitting to spike RED magnitudes. Inter-component leakage /
   sidelobe domination is **out of scope** for `ACTIVE` unless a separate
   a-priori anti-leakage operator is added by a further authorized amend.
   (Proposal §2.0; residual disclaimer ≠ satisfaction.)
5. **Inheritance + coupling close.** Shape/prominence of band *b* inherit PSD
   activity of band *b* on that channel **after** gate-4 shape/prominence
   are redefined on `R = {RESOLVED}` so excluded-band energy cannot
   contaminate ACTIVE cells (proposal §2.7 choice B); invalid channels ignored.
6. **Vacuous-PASS fail-closed.** If a run admits **zero** active cells on a
   required asset/portion, the gate **FAILS** (empty domain ≠ PASS).
7. **0.25 dB immutable.** Domain fields and max aggregator unchanged.
8. **Re-measure required.** After the formula is written, re-run the spike
   adversarial subset (and redteam attacks in §5) on the gate platform;
   PASS/FAIL is that measurement, not this draft.

**Open slot (filled in proposal prose — not consolidated):**

See `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md` (redteam closure draft).
Packaging summary (normative intent; byte-equivalent max set):

```text
RESOLVED(b)  ⇔  every path with w_path(center[b]) > 0 has N_BINS(b,N_path) ≥ 2
DOMAIN       :  ¬RESOLVED(b) ⇒ b outside gate-4 PSD/shape/prominence domain
ACTIVE(b,ch) ⇔  RESOLVED(b) ∧ max(psd_ref, psd_sr)[b,ch] > -120
GATE-4 shape/prominence := §7 formulas on R={i: RESOLVED(i)} only
CONSTRAINT-4 := ENBW aperture + floor/union only (leakage out of scope)
VACUOUS      :  empty active set on any §13.1 portion × valid channel → FAIL
N_MIN = 2 = ceil(ENBW_Hann); 0.25 dB unchanged; no W_MATERIAL
```

Constraint 4 of this candidate §4 is **narrowed** for REV7 admission to the
ENBW + floor perimeter above (proposal §2.0). Dual optional 0/1-bin wording
is forbidden: domain amend is the sole normative packaging.

---

## 5. Required before consolidate (second GO)

1. **Write** the concrete `ACTIVE(…)` (+ domain / coupling / MATERIAL
   clauses from §3 scope reopen) — **done in proposal** (closure draft);
   pending delta redteam acceptance.
2. **`ember-metrology-redteam`:** first pass → **CONTRACT-BROKEN**; judge
   endorsed five must-fixes. Prose closure + **delta redteam** →
   **CONTRACT-POROUS** (2026-07-25): five must-fixes closed; residuals
   remain (prominence-on-R dual reading; ENBW↛occupancy isomorphism;
   MATERIAL center vs support wings; vacuous vs level scalars; A15
   wording). **Independent re-measure authorized YES** under residual
   acceptances A–F in the delta redteam handoff (ENBW-only PASS wording;
   harness §7-on-R; pin one prominence algorithm before lock; no N_MIN
   raise; no leakage-solved claim). ≠ consolidate GO.
3. **Independent re-measure** (judge lineage) — **DONE → RED**.
   Report: `docs/MOTORE_V3_REV7_REMEASURE_R_REPORT.md`.
   Stationary subset under ENBW+floor / §7-on-`R`: still FAIL (multitone
   ~4.77; noise 9.54→1.24 still FAIL). **Dense probe (iii)** — one tone per
   band centre, same rules — also FAIL (~1.19 dB, `prom` b24): sparse
   excitation **insufficient**; residual diagnosis = **inter-band
   inseparability** (neighbour centres inside Hann main lobe) on
   `RESOLVED` bands. **Forbidden:** fit cuts / raise `N_MIN` / shop
   constants against 4.77 / 1.24 / 1.19. **log_sweep** still owed before
   formal close. Next: untainted rewrite of domain/fields and/or a-priori
   geometric criterion (ENBW vs main-lobe separation re-openable on
   diagnostic grounds only) → redteam → re-measure → Guardian.
   ≠ consolidate GO.
4. **Guardian second GO** — **blocked** until scope rewrite addresses §3
   reopen (domain/fields / coupling) consistent with re-measure RED; then
   consolidate path into `docs/MOTORE_V3_G1_CONTRACT.md` only after a later
   PASS re-measure under the rewritten scope.
5. **Lock reopen / re-hash** (mandatory on consolidate): G1a freeze pins
   `predicate: max(psd_db_ref, psd_db_sr) > -120` and
   `threshold_max_abs_db: 0.25` under digest `d2c35ccc…`. Consolidate
   implies coordinated lock + SHA256SUMS update — never silent edit.

Until steps 1–5 complete: **REV7 consolidate = NO.** Product G1b tip must
not claim gate-4 PASS under REV6 admission.

### Redteam must-fix (paper; no N_MIN raise) — closure mapping

1. Coupling → proposal §2.7 choice **(B)** (gate-4 shape/prominence on `R`).
2. Constraint-4 → proposal §2.0 **narrowed** (ENBW + floor; leakage OOS).
3. 0/1-bin → proposal §2.5 **domain amend only** (predicate set-equivalent).
4. MATERIAL → proposal §2.2 **`w_path > 0`** (`W_MATERIAL` withdrawn).
5. Pin + vacuous → proposal §2.1 + §2.8.

---

## 6. Lock / G1a consequence (when consolidated)

| item | action |
|------|--------|
| `threshold_max_abs_db: 0.25` | **unchanged** |
| activity predicate string | replace with consolidated `ACTIVE(…)` |
| `metrology_lock_sha256` | recompute |
| SHA256SUMS | update affected entries |
| spike harness | align to new predicate only after consolidate GO |

---

## 7. PLAN / lab state impact (until consolidate)

- Frozen technical authority remains CONTRACT @ `6d254d0a` (REV6).
- This file is a **candidate** under PLAN lab authority.
- Spike remains ≠ G1 PASS; ≠ Ableton readiness.
- Official product G1b gate-4 claims stay blocked on REV6 admission until
  consolidate + re-measure PASS (or a different authorized path).

---

## 8. Handoff

| agent | next |
|-------|------|
| untainted author | closure draft in `MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md` |
| ember-metrology-redteam | **delta** pass on closure prose; re-measure still forbidden if BROKEN |
| ember-parity-lab | re-measure only after delta non-BROKEN |
| ember-contract-guardian | second GO for consolidate only |

≠ G1 PASS. ≠ REV7 consolidated. 0.25 dB not touched.
```

---

## B.4 FILE: `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_MANDATE.md`

**Path logico:** `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_MANDATE.md`  
**Bytes:** 6313  
**Lines:** 129

```markdown
# MANDATO — FORMULA ACTIVE(b, ch) per §13.2 gate 4 (candidato REV7)

**Status:** AUTHORIZED TO START (2026-07-25) — formula writer must be untainted  
**Draft commit (diagnosis frozen first):** `2eac2387`  
**Candidate doc (do not use §1 numbers):** `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`

**Destinatario:** NON chi ha prodotto o letto l'evidenza WS4 (lineage contaminato).  
Counter-check giudica la formula, non la scrive: ha visto i RED.

**Deliverable path:** write formula + constant derivations into  
`docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`  
(Do **not** paste RED numbers into that file. Do **not** edit CONTRACT freeze,
lock, or SHA256SUMS. Optionally note the open slot reference in the candidate
doc without copying §1 evidence tables.)

---

## Compito

Scrivere la definizione concreta di `ACTIVE(b, ch)` — il predicato che ammette
una cella (banda `b`, canale `ch`) nel massimo del gate 4 di sample-rate
parity — rispettando i vincoli 1–8 sotto.

## La domanda fisica a cui rispondere

Quando la misura di una banda è determinata dal contenuto spettrale **dentro**
il proprio supporto, e quando invece è determinata da leakage di componenti
**fuori** dal supporto o dall'instabilità del floor numerico?

Il predicato deve ammettere le prime ed escludere le seconde.

## Geometria congelata (tutto derivabile a priori, nessuna misura)

- Analisi **sempre** a `fs_c = 48000` Hz: la geometria delle bande è identica
  per ogni sample rate sorgente.
- 120 bande, centri `center[i] = 20 * (20000/20)**(i/119)`, estremi pinnati a
  `20.0` e `20000.0` Hz. Rapporto fra centri adiacenti
  `r = 1000**(1/119) ≈ 1.05955` (larghezza relativa ~5.96%).
- Supporto triangolare su `log2(f)`: la banda `i` copre
  `center[i-1]..center[i+1]`; per le bande 0 e 119 si usano centri virtuali
  allo stesso rapporto `r`.
- Due griglie FFT: MAIN 4096 (~11.719 Hz/bin) e LF 8192 (~5.859 Hz/bin).
- Finestra Hann **periodica**: `w[n] = 0.5 - 0.5*cos(2*pi*n/N)`. Le sue
  proprietà di leakage (livello del primo lobo laterale, decadimento,
  larghezza del lobo principale in bin) sono la fonte legittima delle
  costanti.
- Fusione: LF puro `<= 160` Hz, MAIN puro `>= 320` Hz, crossfade raised-cosine
  su `log2` fra i due. Sotto 160 Hz la banda è alimentata **solo** da LF.
- PSD: floor lineare `1e-12`, poi clamp `[-120, +12]` dB. DC e bin oltre
  20000 Hz esclusi.

Allowed reads for geometry only: `docs/MOTORE_V3_G1_CONTRACT.md` §6/§7
(structure), `ml_v3/contracts/` band/grid helpers, `ml_v3/fixtures/g1/metrology_lock.json`
for **non-outcome** frozen constants (FFT sizes, window name, floors).  
Do **not** treat the current activity predicate string as sacred — replacing
it is the point of this candidate — but do **not** invent numbers from
measurement runs.

## Conseguenza geometrica già calcolabile (NON è un risultato di misura)

Incrociando i supporti triangolari con le due griglie si ottiene, per pura
aritmetica:

- griglia MAIN 4096: **14** bande con ZERO bin nel supporto, **21** con UN SOLO bin;
- griglia LF 8192: **6** bande con ZERO bin, **17** con UN SOLO bin.

Le bande interessate stanno nell'estremo basso della griglia. Chiunque può
riprodurlo in cinque righe dai centri e dalle griglie: usalo.

## Vincoli 1–8 (da REV7 candidate §4 — senza numeri di run)

1. **A priori.** Scritto prima della ri-misura che reclama PASS. Vietato
   scegliere cutoff scansionando celle/margini di una run fino a
   `max|Δ| ≤ 0.25`.
2. **No post-hoc mask.** Vietato nascondere bande dopo aver visto errori.
3. **Preserve one-sided artifact intent.** Un difetto presente a 44.1/96 ma
   non a 48 non deve sparire dal max solo perché il riferimento sta al floor.
   La sola intersezione `both > −120` è **insufficiente** come sostituto
   completo.
4. **Signal-bearing admission.** Escludere celle dominate da leakage /
   interferenza inter-componente o instabilità del floor numerico; trattenere
   celle con energia di segnale stabile rilevante per la SR parity. Definizione
   operativa in linguaggio da contratto **senza** fit a magnitudini di run.
5. **Inheritance unchanged.** Shape/prominence della banda `b` ereditano
   l'attività PSD della banda `b` sullo stesso canale; canali invalidi ignorati.
6. **Vacuous-PASS fail-closed.** Zero celle attive su un asset/portion
   richiesto → gate **FAIL** (dominio vuoto ≠ PASS).
7. **0.25 dB immutable.** Dominio e aggregatore max invariati.
8. **Re-measure required.** Dopo che la formula è scritta — non in questo
   task — si ri-misura. Questo task **non** esegue la ri-misura per “vedere
   se passa”.

## Standard di accettazione

Per **ogni** costante numerica nella formula si deve poter indicare da quale
quantità congelata deriva: lobi laterali Hann, larghezza lobo principale in
bin, spaziatura bin, rapporto `r` fra centri, soglie già nel contratto.
Una costante non derivabile da queste è un fit → formula respinta.

## Divieti

- NON chiedere, cercare o usare risultati numerici della run WS4: né
  `max|Δ|`, né quali bande/asset/SR hanno fallito, né tabelle di margine.
  Se ti vengono offerti, rifiutali e dichiaralo.
- NON aprire / leggere:
  - `ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.*`
  - `ml_v3/benchmark/` evidence outputs keyed to WS4 runs
  - agent transcripts about WS4 RED diagnostics
  - §1 “Trigger” tables inside `MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`
    (contengono numeri di run — i vincoli 1–8 sono già copiati qui)
- NON toccare 0.25 dB, aggregatore max, campi dominio, P1–P7, criteri
  −100 dBFS/Hz di §10.
- NON modificare CONTRACT REV6 (`6d254d0a`), metrology lock, SHA256SUMS.
- NON eseguire la ri-misura in questo task.

## Esito atteso

1. La formula, in linguaggio da contratto, in
   `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`.
2. Per ogni costante, una riga di derivazione da geometria/finestra.
3. Dichiarazione esplicita se ritieni che **nessun** criterio principiato
   possa ammettere in modo stabile le bande a zero/uno bin — in tal caso
   dillo invece di forzare una formula (scoperta: emendare il **dominio**
   del gate, non solo il predicato).

## Dopo (fuori scope di questo mandato)

metrology-redteam (false-PASS) → counter-check indipendente (ri-misura) →
secondo GO. Nessun consolidate prima.
```

---

## B.5 FILE: `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`

**Path logico:** `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`  
**Bytes:** 19947  
**Lines:** 397

```markdown
# REV7 proposal — `ACTIVE(b, ch)` for §13.2 gate 4

**Status:** FORMULA PROPOSAL — REDTEAM CLOSURE DRAFT (untainted writer; a priori geometry/window only)  
**Authority for task:** `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_MANDATE.md`  
**Open amend vehicle (reference only):** `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`  
**Does not edit:** CONTRACT freeze REV6, metrology lock, SHA256SUMS, `Source/`  
**Does not contain:** run margins, max|Δ|, failing band/asset lists, WS4 evidence  
**Re-measure:** forbidden until delta redteam returns non-BROKEN on this prose

---

## 1. Physical question (operative, scoped)

**Normative perimeter of this amend (constraint 4 narrowed — see §2.0):**

Gate-4 **admission** answers only:

1. **ENBW aperture:** is the triangular support wide enough, on every fusion
   path with positive weight, to contain at least one Hann ENBW of in-support
   bins? (`N_BINS ≥ N_MIN = 2`)
2. **Floor / union:** is the cell strictly above the §6.1 clamp floor on the
   reference **or** under-test render?

Cells that fail (1) are **outside the gate-4 SR-parity domain** (domain amend).
Cells that fail (2) are inactive (floor).

**Explicitly out of scope for `ACTIVE`:** whether in-support content dominates
out-of-support **leakage** from strong neighbours. That is a different physical
question; this amend does **not** claim an anti-leakage operator. Constraint 4
of the candidate is hereby narrowed to the ENBW-aperture + floor-union perimeter
above. A residual disclaimer is not the normative statement — §2.0 is.

---

## 2. Contract-language definition

Let `fs_c = 48000`. Analysis geometry is identical for every host sample rate.
Band centres and triangular supports are those of §6.1. Fusion weights
`w_LF(f)`, `w_MAIN(f)` are those of §6.2. Grids: MAIN `N_MAIN = 4096`,
LF `N_LF = 8192`. Window: periodic Hann of §6.2.

### 2.0 Normative constraint-4 perimeter (ENBW + floor only)

**Normative (replaces any broader “leakage-dominated exclusion” reading of
candidate constraint 4 for this amend):**

> Gate-4 cell admission is exactly ENBW-aperture resolvability on every
> positive-weight fusion path (§2.2–2.3) conjoined with the floor/union cut
> (§2.4). Inter-component leakage / sidelobe domination is **out of scope**
> for the `ACTIVE` predicate and for the geometric domain carve-out. No
> a-priori anti-leakage operator is introduced in REV7 by this proposal.

### 2.1 Bin occupancy (pure geometry; bit-stable)

For band index `b ∈ {0,…,119}` and FFT length `N ∈ {N_LF, N_MAIN}`:

```text
Δf(N)        = fs_c / N
lo(b), hi(b) = triangular support of b on log2(f)
               (virtual centres at ratio r outside 0 and 119; §6.1)
N_BINS(b, N) = #{ k ≥ 1 : lo(b) < k·Δf(N) < hi(b) ∧ k·Δf(N) ≤ 20000 }
```

DC and bins above 20000 Hz remain excluded (§6.1).

**Bit-stable / hashable evaluation procedure (normative):**

1. Centres and virtual endpoints: closed form of §6.1 in IEEE-754 binary64,
   same expressions as `ml_v3/contracts` centre helpers (no measured tables).
2. Support edges `lo(b)`, `hi(b)`: evaluate in binary64 from those centres.
3. Bin loop: integer `k` from `1` to `floor(20000/Δf(N))` inclusive; admit `k`
   iff `lo(b) < k·Δf(N)` and `k·Δf(N) < hi(b)` and `k·Δf(N) ≤ 20000`, with
   products `k·Δf(N)` in binary64 (`Δf = fs_c/N` in binary64).
4. `N_BINS(b,N)` is the cardinality of that set (non-negative integer).
5. Canonical mask bytes (for lock / SHA256SUMS on consolidate): concatenate,
   for `b = 0..119` in order, the bits
   `RESOLVED(b)`, then for each valid channel role the per-cell activity bits
   as packed in the consolidate lock schema — all bits derived only from
   (1)–(4), §2.2–2.4, and the PSD floor test on the two renders. No run-time
   float tolerance beyond binary64 evaluation of the predicates above.

### 2.2 Material fusion paths (worst-case: any positive weight)

A fusion path is **material** for band `b` when its weight at `center[b]` is
strictly positive. Any path with `w > 0` can dominate the fused power under
adversarial energy ratios; equal-energy bounds are **not** used.

```text
MATERIAL_LF(b)   ⇔  w_LF(center[b])   > 0
MATERIAL_MAIN(b) ⇔  w_MAIN(center[b]) > 0
```

(At least one of the two always holds because `w_LF + w_MAIN = 1` with
non-negative weights. Pure-LF bands have `w_MAIN = 0`; pure-MAIN have
`w_LF = 0`; crossfade bands have both material.)

The immutable gate threshold `0.25 dB` is **not** an input to MATERIAL. It
remains only the max-|Δ| hard threshold of §13.2.

### 2.3 Geometric resolvability

Hann periodic ENBW = `1.5` bins (frozen window property; §3). ENBW is the
width of a rectangular filter that collects the same noise power as the
analysis window: it is the noise-equivalent aperture of one DFT bin
measurement under Hann. For a triangular support average to be an
**ENBW-aperture-resolved** measurement, the open support must be wide enough
to contain at least one full ENBW of aperture **inside** the support.

The smallest integer bin occupancy with width ≥ `ENBW_Hann` is therefore:

```text
N_MIN = ceil(ENBW_Hann) = ceil(1.5) = 2

RESOLVED(b) ⇔
    ( ¬MATERIAL_LF(b)   ∨ N_BINS(b, N_LF)   ≥ N_MIN )
  ∧ ( ¬MATERIAL_MAIN(b) ∨ N_BINS(b, N_MAIN) ≥ N_MIN )
```

This criterion does **not** claim main-lobe isolation (null-to-null width) and
does **not** claim leakage immunity (§2.0).

### 2.4 Floor / union (one-sided artifact intent)

With the §6.1 clamp domain and linear floor `1e-12`:

```text
ABOVE_FLOOR(b, ch) ⇔
    max( psd_db_ref[b, ch], psd_db_sr[b, ch] ) > -120
```

Strict inequality: cells stuck on the clamp floor are inactive. The `max`
(union across reference 48 kHz and under-test SR) preserves gate-4 intent that
an artifact present on only one side remains eligible for the max |Δ|.

### 2.5 Domain amend for unresolvable bands (sole normative packaging for 0/1-bin)

**Normative packaging (domain amend — not optional, not dual):**

> Band indices `b` with `RESOLVED(b) = false` under §2.2–2.3 (equivalently:
> material-path bin occupancy `N_BINS < 2` on at least one path with
> `w_path(center[b]) > 0`) are **outside the gate-4 SR-parity domain** for
> PSD, shape, and prominence on every channel. They do not contribute cells
> to the gate-4 max |Δ|.

**Equivalence of the max cell set (stated, not optional dual form):**

Let `D_domain` be the set of gate-4 cells after applying the domain sentence
above and then admitting PSD cells by `ABOVE_FLOOR` only inside the remaining
band set. Let `D_pred` be the set of cells with
`ACTIVE(b,ch) ⇔ RESOLVED(b) ∧ ABOVE_FLOOR(b,ch)` under §2.6, with shape /
prominence inheritance as amended in §2.7. Then `D_domain = D_pred` as sets
of `(field, b, ch)` cells entering the max. Implementations may compute via
the predicate; the **normative prose form** for 0/1-bin exclusion is the
domain sentence, not a second parallel “optional” wording.

There is **no** principled amplitude rule that can stably *admit* bands with
material occupancy `N_BINS ∈ {0,1}` as ENBW-aperture-resolved cells (§4).

### 2.6 Predicate (implementational form; set-equivalent to §2.5)

For a PSD cell on channel `ch ∈ {mid, side}` with that channel valid (§7):

```text
ACTIVE(b, ch) ⇔ RESOLVED(b) ∧ ABOVE_FLOOR(b, ch)
```

- Scalars `mid_level_dbfs` / `side_level_dbfs` stay in the gate domain when the
  respective channel is valid; they are not band-indexed and do not use
  `RESOLVED(b)`.
- Channels with `*_valid == false` are ignored (§7); their vectors do not
  contribute cells.

### 2.7 Shape / prominence coupling — choice (B) (CRITICAL)

**Problem:** §7 defines

- `shape_db` = PSD minus `10*log10(sum(10**(psd_db/10)))` over **all 120** bands;
- `prominence_db` = shape minus a Gaussian convolution on the log-band axis,
  kernel `j ∈ [-16..16]`, on that shape vector.

Excluding cell `(b,ch)` from the max via `ACTIVE` / domain carve-out does
**not** remove band `b`’s energy from those formulas. Predicate-only
“inherit ACTIVE” therefore leaves excluded-band energy able to move ACTIVE
cells’ shape/prominence — a false-PASS surface if those contaminants are
omitted from the max while still driving ACTIVE neighbours.

**Choice (A) rejected for closure:** keeping above-floor unresolved bands in
the max for shape/prominence measures the contaminant cells themselves but
does **not** stop their energy from altering ACTIVE neighbours’ shape /
prominence through the global sum and ±16 kernel. It is fail-closed for the
sparse cells’ own fields, not a coupling close.

**Choice (B) — normative (domain + gate-4 field redefinition):**

> For **gate-4 evaluation only**, let `R = { i ∈ {0,…,119} : RESOLVED(i) }`.
> Gate-4 `shape_db[b,ch]` and `prominence_db[b,ch]` are the §7 formulas with
> support restricted to `R`:
>
> - shape normalizer sums `10**(psd_db[i,ch]/10)` only over `i ∈ R`;
> - prominence kernel includes only offsets `j` with `b+j ∈ R` (renormalize
>   the retained kernel weights to sum one; reflect padding is applied only
>   within the `R`-indexed sequence in band order).
>
> Bands with `b ∉ R` are outside the gate-4 shape/prominence domain (no cells).
> For `b ∈ R`, shape/prominence are active in the max iff `ACTIVE(b,ch)` on
> that channel’s PSD (i.e. also `ABOVE_FLOOR`).
>
> **Testable statement:** mutating `psd_db[u,ch]` for any `u ∉ R` must leave
> every gate-4 `shape_db[b,ch]` and `prominence_db[b,ch]` for `b ∈ R`
> unchanged (bit-identical under the same binary64 reduction order as the
> consolidate harness). Mutating `psd_db[u,ch]` for `u ∈ R` may change those
> fields exactly as §7-on-`R` predicts.

Product §7 vectors used by other gates are untouched by this sentence; only
the gate-4 comparison domain and the fields entering the gate-4 max use the
`R`-restricted definitions.

Honesty note: closing coupling requires this **domain / derived-field amend**
for gate 4; a predicate-only mask on the existing §7 full-120 fields cannot
make the testable statement true.

### 2.8 Vacuous FAIL (fail-closed; no N/A skip)

**Normative:**

> For every asset/portion required by §13.1, and for every channel `ch` that
> is valid for that portion (`mid_valid` / `side_valid` as applicable), if the
> set of active gate-4 cells on that (portion, channel) is empty — counting
> PSD/shape/prominence cells under §2.5–2.7 and level scalars when in domain —
> then gate 4 is **FAIL** for that portion. Empty domain ≠ PASS. Implementations
> must not skip a required (portion, valid channel) with N/A, soft-pass, or
> “no cells to compare.”

Threshold `0.25 dB` and aggregator `max` over the declared gate-4 dB domain
remain immutable. No post-hoc band mask after seeing errors.

---

## 3. Per-constant derivation (frozen window / grid / contract only)

| Constant | Value | Derivation |
|---|---|---|
| `fs_c` | `48000` | §6 / lock timing |
| `N_MAIN` | `4096` | §6.2 / lock |
| `N_LF` | `8192` | §6.2 / lock |
| `Δf(N)` | `fs_c/N` | FFT bin spacing |
| centres / `r` | §6.1 closed form | `center[i]=20*(20000/20)**(i/119)`; endpoints pinned |
| triangular support | `center[b-1]..center[b+1]` | §6.1; virtual ends via same `r` |
| fusion split | `160` / `320` Hz | §6.2 pure LF / pure MAIN |
| `w_LF`, `w_MAIN` | raised-cosine on `log2` | §6.2 formula |
| window | periodic Hann `0.5-0.5*cos(2πn/N)` | §6.2 |
| `ENBW_Hann` | `1.5` bins | classical ENBW of periodic Hann (noise-equivalent bandwidth of one DFT bin under the frozen window) |
| `N_MIN` | `2` | `ceil(ENBW_Hann)` — minimum integer occupancy whose support width ≥ one Hann ENBW; guarantees the noise-equivalent aperture of the band average can lie inside the triangular support (see §2.3). Occupancy `1 < 1.5` fails that coverage; main-lobe null-to-null width is not used |
| PSD floor linear | `1e-12` | §6.1 / lock |
| activity floor dB | `-120` | `10*log10(1e-12)` after §6.1 clamp lower edge |
| `ABOVE_FLOOR` cut | `> -120` | strict above clamp floor; union via `max(ref,sr)` preserves one-sided artifacts |
| gate threshold | `0.25` dB | §13.2 immutable — **not** used to define MATERIAL |
| MATERIAL rule | `w_path(center[b]) > 0` | worst-case: any positive-weight path can dominate fused power under adversarial energy ratios; equal-energy `W_MATERIAL` bound withdrawn |

No constant above is taken from a measured max|Δ|, margin table, or band-failure list. `W_MATERIAL = 1 - 10**(-0.25/10)` is **removed** from this proposal.

### 3.1 A priori occupancy consequence (arithmetic, not a run)

Crossing triangular supports with the two grids (mandate § “Conseguenza geometrica”) yields on MAIN: 14 bands with `N_BINS=0`, 21 with `N_BINS=1`; on LF: 6 with `0`, 17 with `1`. Under `RESOLVED` with `N_MIN=2` and `w > 0` materiality, the non-resolvable set is exactly the bands whose **material** occupancy is `0` or `1` (low-frequency edge; in the crossfade, both grids must clear `N_MIN`). That identity is a check of the formula against geometry, not a fit to errors.

---

## 4. Why 0/1-bin bands stay outside the domain

**Claim (honest):** there is **no** principled amplitude rule that can stably
*admit* bands with material occupancy `N_BINS ∈ {0,1}` as ENBW-aperture-resolved
cells under §2.3.

Reasons (geometry / window only — ENBW, not main-lobe isolation):

1. **Zero bins.** No FFT bin lies inside the open triangular support on the
   material path. Support width is `0 < ENBW_Hann`; the band average has no
   in-support content. Any finite dB value is empty-support / floor behaviour,
   not a measurement of in-band spectrum.
2. **One bin.** Support width is `1` bin. Hann ENBW is `1.5` bins, so
   `1 < ENBW_Hann`: the noise-equivalent aperture of the single DFT bin
   measurement is wider than the triangular support. By the ENBW definition,
   out-of-support content is required to fill that aperture; the cell cannot
   be ENBW-aperture-resolved. Raising the level threshold cannot widen the
   support to ≥ `ENBW_Hann`.

Therefore the sole normative packaging is the **domain amend** of §2.5
(`N_MIN = ceil(ENBW_Hann) = 2` kept). Bands with material `N_BINS ≥ 2` remain
eligible; admission inside the domain is then only `ABOVE_FLOOR` (union).

---

## 5. Explicit non-goals / non-claims

- This note does **not** claim gate PASS and does **not** report a re-measure.
- Re-measure after adoption is mandatory (mandate constraint 8) and is out of
  scope for this writer; it remains **forbidden** until delta redteam clears
  the closure surface.
- §10 evaluator criteria (`−100` dBFS/Hz, etc.) are untouched.
- Replacing the REV6 string `max(psd_db_ref, psd_db_sr) > -120` is intentional;
  the floor/union factor is retained; geometric domain / `RESOLVED` is added;
  gate-4 shape/prominence are redefined on `R` (§2.7).
- No sidelobe-vs-neighbour dB cut is proposed: mapping Hann’s first sidelobe
  (`≈ −31.5` dB in bin space) onto band-index neighbours is not an isomorphism
  fixed by §6 alone, so it is rejected as a constant source here — and, under
  §2.0, leakage exclusion is out of scope for ACTIVE rather than half-solved
  by a weak residual.
- Main-lobe null-to-null width (`4` bins for periodic Hann) is **not** used to
  set `N_MIN`. That isolation argument would force `N_MIN ≥ 4` and is a
  different physical claim; this proposal commits only to ENBW aperture
  coverage (`N_MIN = ceil(1.5) = 2`).
- Forbidden: raise `N_MIN` to chase PASS; use run margins; edit CONTRACT freeze /
  lock / SHA256SUMS from this note.

---

## 6. Packaging sketch for the candidate (normative intent for consolidate prose)

Contract-shaped replacement for the gate-4 activity / domain block:

> **Domain (0/1-bin):** Band indices with `RESOLVED(b) = false` —
> `RESOLVED(b)` iff every fusion path with `w_path(center[b]) > 0` has
> `N_BINS(b, N_path) ≥ 2 = ceil(ENBW_Hann)` — are outside the gate-4
> PSD/shape/prominence domain.
>
> **PSD activity** on remaining bands: `ACTIVE(b,ch) ⇔ ABOVE_FLOOR(b,ch)` with
> `max(psd_db_ref, psd_db_sr) > -120` (equivalently
> `RESOLVED(b) ∧ ABOVE_FLOOR` over all `b`, same max cell set).
>
> **Gate-4 shape/prominence:** §7 formulas restricted to
> `R = {i : RESOLVED(i)}` (normalizer and ±16 kernel); test: PSD outside `R`
> does not change gate-4 shape/prominence on `R`. Active in the max iff
> `ACTIVE(b,ch)`.
>
> **Constraint-4 perimeter:** admission = ENBW aperture + floor/union only;
> leakage domination out of scope for ACTIVE.
>
> **Vacuous:** empty active set on any §13.1 portion × valid channel → FAIL.
> Max aggregator and `0.25` dB unchanged. Mask bits from §2.1 procedure.

---

## 7. Writer contamination statement

Allowed inputs used: mandate; CONTRACT §6/§7 structure; `ml_v3/contracts/`
centre helpers / constants; `metrology_lock.json` non-outcome fields (FFT
sizes, floor, clamp, fusion timing); candidate scope-reopen / must-fix list
wording (no §1 trigger tables). No WS4 evidence files, no `sr_parity.py`,
no agent transcripts, no REV7 candidate §1 trigger tables, no run reports
listing max|Δ| or failing bands were opened for this formula.

---

## 8. Scope statement (normative, not a residual hedge)

Independent coherence judgment previously noted that ENBW-only is narrower
than “leakage-dominated exclusion.” That narrowing is now **normative §2.0**,
not a §8 disclaimer. The formula is internally consistent under the ENBW +
floor perimeter. Leakage sufficiency is not claimed; an anti-leakage operator
is not smuggled via residual text.

Re-measure (when authorized) decides PASS/FAIL under this scoped rule — not
this note.

---

## 9. Pre-registered governance (before any re-measure)

Frozen **before** re-measure so the outcome cannot rewrite the rule:

1. If re-measure **FAILS** under `N_MIN = 2`, it is **forbidden** to raise
   `N_MIN` (or otherwise retune constants) until the gate appears to PASS.
   That would be threshold shopping.
2. Admissible responses to FAIL only:
   - a re-derivation from a **different** frozen window/grid property,
     justified on its own terms (not by the FAIL magnitudes); or
   - a further **domain** / derived-field amend authorized by a new GO —
     not silent threshold edits.
3. Sequence: **delta metrology-redteam on this closure prose** → then
   independent re-measure from scratch (only if non-BROKEN) → then second
   Guardian GO. No consolidate while this proposal is uncommitted / unaccepted.
4. `0.25 dB` is immutable as the gate threshold, not a shopping knob for
   MATERIAL or `N_MIN`.

---

## 10. Redteam closure (must-fix 1–5)

| # | Must-fix | Closure in this prose |
|---|---|---|
| 1 | Shape/prominence coupling (CRITICAL) | **Choice (B).** §2.7 redefines gate-4 shape/prominence on `R = {RESOLVED}`; testable bit-stability vs PSD outside `R`. Choice (A) rejected as insufficient to stop contamination of ACTIVE neighbours. Coupling closed only by domain/derived-field amend — stated honestly. |
| 2 | Constraint 4 perimeter | **Narrowed yes (normative).** §2.0: admission = ENBW-aperture + floor-union only; leakage out of scope for ACTIVE. No anti-leakage operator added. §8 is no longer a hedge — it restates the normative perimeter. |
| 3 | Single form for 0/1-bin | **Domain amend xor** (domain is the sole normative packaging). §2.5; predicate `RESOLVED=false` is implementationally set-equivalent (§2.5 equivalence), not an optional second contract sentence. Dual “optional” wording removed. |
| 4 | MATERIAL worst-case | **`w_path > 0` must be resolved** (§2.2). Equal-energy `W_MATERIAL` withdrawn. `0.25 dB` not used in MATERIAL; remains gate threshold only. |
| 5 | Pin + vacuous | §2.1 bit-stable `N_BINS`/`RESOLVED`/`ACTIVE` mask procedure; §2.8 vacuous FAIL per §13.1 portion **and** per required valid channel; no N/A skip. |

`N_MIN = 2` retained (`ceil(ENBW_Hann)`); not raised.
```

---

## B.6 FILE: `docs/MOTORE_V3_REV7_SCOPE_REWRITE_MANDATE.md`

**Path logico:** `docs/MOTORE_V3_REV7_SCOPE_REWRITE_MANDATE.md`  
**Bytes:** 3322  
**Lines:** 74

```markdown
# MANDATO — riapertura scopo REV7 (post ri-misura + sonda densa)

**Status:** AUTHORIZED TO START — untainted writer only  
**Date:** 2026-07-25

## Destinatario

NON chi ha prodotto o letto i numeri di esito (WS4 / ri-misura / sonda
densa) per scegliere costanti. Counter-check giudica, non scrive.

**Contenimento:** non consultare report di ri-misura, evidenze di spike
WS4, né il candidate REV7 (contengono magnitudini di fail). Se ti vengono
offerti, rifiutali e dichiaralo. La propria proposta ACTIVE precedente
può essere riusata **a memoria / da copia locale già nota** senza
riaprire documenti che la citano insieme agli esiti.

**Test di integrità:** se ti accorgi di conoscere l’entità numerica di
qualunque fallimento di gate (max|Δ|, bande colpevoli, asset/SR che
falliscono), dichiaralo **prima** di scrivere e fermati — la
contaminazione deve emergere, non restare implicita.

## Diagnosi consegnata (fisica / geometria — non una costante)

1. Under-resolution 0/1-bin: già indirizzata da domain-on-`R` /
   criterio ENBW con `N_MIN = 2` nella proposta precedente.
2. Ipotesi “solo fixture sparse / valli vuote” (**iii**): **testata** —
   l’eccitazione densa (una componente per centro di banda, stesse regole
   di ammissione) **non ha chiuso il gate**. Quindi (iii) è
   **insufficiente** come spiegazione completa. Nessuna cifra di esito
   è fornita qui di proposito.
3. Residuo diagnostico: su bande ancora `RESOLVED` sotto ENBW, i **centri
   delle bande vicine** possono stare **dentro il lobo principale** della
   Hann periodica congelata → inseparabilità inter-banda / interferenza
   locale. Questo è un fatto di geometria finestra↔griglia.

## Geometria congelata (riuso del mandato ACTIVE; nessuna misura)

- `fs_c = 48000`; 120 bande; `center[i] = 20*(20000/20)**(i/119)`;
  `r = 1000**(1/119)`; supporto triangolare su `log2(f)`.
- MAIN 4096 / LF 8192; Hann periodica; fusione 160/320 Hz.
- Occupancy geometrica (aritmetica): MAIN 14 zero-bin + 21 single-bin;
  LF 6 zero-bin + 17 single-bin.
- Proprietà Hann legittime come fonte costanti: ENBW = 1.5 bin;
  lobo principale null-to-null = 4 bin; primo sidelobe classico.

## Compito

Riscrivere lo scopo dell’emendamento (document-only) in  
`docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`  
scegliendo e giustificando a priori **una** direzione coerente:

- domain / field-definition amend; e/o
- criterio geometrico ripartendo da proprietà finestra/griglia
  (ENBW aperture **oppure** main-lobe separation **oppure** altro
  derivabile) — **sui propri termini**, senza chiedere “cosa passa”.

## Divieti

- NON chiedere / usare max|Δ| di run, tabelle margine, “quale N_MIN passa”.
- NON alzare `N_MIN` perché una misura è FAIL.
- NON toccare 0.25 dB, CONTRACT freeze, lock, SHA256SUMS.
- NON eseguire ri-misura in questo task.
- NON aprire file il cui nome suggerisce evidenza/esito di run
  (`*REMEASURE*`, `*WS4*EVIDENCE*`, `*REV7_CANDIDATE*`, `sr_parity`
  evidence dumps).

## Esito

1. Proposta aggiornata + tabella costanti con derivazione; **oppure**
   dichiarazione onesta che serve emendare il dominio/campi e non un
   predicato di ammissione.
2. Dichiarazione di non-contaminazione (o stop se contaminato).

Poi (fuori scope): redteam → ri-misura da giudice → secondo GO.
```

---

## B.7 FILE: `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`

**Path logico:** `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`  
**Bytes:** 13331  
**Lines:** 263

```markdown
# REV7 — Scope rewrite proposal (document-only)

**Status:** PROPOSAL — product packaging = report-only LF (Marco)  
**Date:** 2026-07-25  
**Authority for this write:** `docs/MOTORE_V3_REV7_SCOPE_REWRITE_MANDATE.md`  
**Report-only clause:** `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`  
**Kind:** scope rewrite of the SR-parity admission domain and geometric resolvability predicates + report-only packaging for ∉`R`.  
**Not in scope of this document:** remeasure, lock edits, SHA256SUMS, CONTRACT freeze edits, threshold shopping.

---

## 0. Contamination statement

**CLEAN.**

This write used only:

- the cleaned scope-rewrite mandate;
- geometry / field structure from `docs/MOTORE_V3_G1_CONTRACT.md` §6–§7;
- band-centre helpers in `ml_v3/contracts/grid.py` / `constants.py`;
- lock *non-outcome* timing/floor names (`fs_c`, `N_LF`, hop `H`, `floor_linear`) without reading run tables.

It did **not** open `*REMEASURE*`, `*WS4*EVIDENCE*`, `*REV7_CANDIDATE*`, or `sr_parity` evidence dumps.  
It does **not** know, and does not use, any measured `max|Δ|`, margin table, failing asset/SR list, or measured failing band-index list from a gate run.

Structural band counts below are **a priori** consequences of frozen window↔grid arithmetic, of the same kind as the mandate’s occupancy census (zero-bin / single-bin). They are not run outcomes.

If a reviewer later discovers latent numeric gate-failure knowledge in the author, this proposal is void and must be rewritten by a fresh untainted writer.

---

## 1. Chosen direction (one coherent package)

**Domain amend of admissible set `R`, driven by a main-lobe separation predicate** — not a raise of `N_MIN`, and not a change of the 0.25 dB gate number.

Package:

1. **Keep** the prior ACTIVE under-resolution layer: domain-on-`R` with ENBW occupancy and `N_MIN = 2` (already justified from Hann ENBW = 1.5 bin; **not** raised here).
2. **Add** a second, independent geometric predicate on the same bands: **neighbor centres must lie outside the periodic-Hann main lobe** of a tone at the band centre, evaluated in the FFT bin metric of every path that contributes weight after fusion.
3. **Interpret** failure of (2) as *domain exclusion* (band ∉ `R`), not as evidence to loosen or tighten the dB gate.

### Why this direction

Mandate diagnosis, used only as physics/geometry:

| Layer | Status |
|---|---|
| 0/1-bin under-resolution | Already addressed by ENBW + `N_MIN = 2` on `R` |
| “Sparse fixtures / empty valleys” alone | Tested by dense one-tone-per-centre excitation under the same admission rules; **insufficient** as a complete explanation (no outcome magnitudes used) |
| Residual on bands still ENBW-`RESOLVED` | Neighbor centres can sit **inside** the frozen Hann main lobe → local inter-band inseparability |

ENBW occupancy answers “does this band own enough equivalent noise bandwidth?”  
Main-lobe separation answers “is the next centre even a distinct spectral peak under this window?”  
Those are different questions; passing the first does not imply the second on a log-spaced 120-band grid against MAIN 4096 / LF 8192.

Raising `N_MIN` because a measure failed is **forbidden** and also **misaligned**: the residual is centre-to-centre geometry inside the main lobe, not a thicker occupancy requirement.

---

## 2. Frozen geometry reused (no measure)

From contract §6 and the mandate’s freeze list:

| Symbol | Value | Source |
|---|---|---|
| `fs_c` | 48000 | §6 / lock timing |
| `N_MAIN` | 4096 | §6.2 |
| `N_LF` | 8192 | §6.2 / lock `timing.N_LF` |
| `H` | 1024 | §6.2 / lock |
| Window | periodic Hann `0.5 - 0.5*cos(2πn/N)` | §6.2 |
| PSD floor (linear) | `1e-12` then clamp `[-120, +12]` dBFS/Hz | §6.1 / lock activity floor |
| Centres | `center[i] = 20 * (20000/20)**(i/119)`, `i = 0..119` | §6.1 |
| Ratio | `r = 1000**(1/119)` | equivalent closed form |
| Support | triangular on `log2(f)` | §6.1 |
| Fusion | LF pure ≤160 Hz; MAIN pure ≥320 Hz; raised-cosine crossfade on `log2(f)` in (160, 320) | §6.2 |

Legitimate Hann constants (classical, not fitted to a run):

| Property | Bins |
|---|---|
| ENBW | 1.5 |
| Main lobe null-to-null | 4 |
| Main lobe half-width (centre → first null) | 2 |
| First sidelobe | classical Hann location/level (informational; not used as a gate constant here) |

Bin widths:

```text
Δf_MAIN = fs_c / N_MAIN = 48000 / 4096 = 11.71875 Hz
Δf_LF   = fs_c / N_LF   = 48000 / 8192 =  5.859375 Hz
```

Mandate occupancy census (unchanged, arithmetic only): MAIN 14 zero-bin + 21 single-bin; LF 6 zero-bin + 17 single-bin.

---

## 3. Domain and field definition

### 3.1 Fields under the SR-parity claim

Unchanged surface from §7: the parity claim for this amend continues to address **stationary mid (and side, when valid) `*_psd_db[120]` band energies** after the frozen PSD / fusion / dB path.

Still excluded from the parity domain (lock already names delta exclusion; this proposal does not reopen that list): frame-to-frame `*_delta_db`, and any field whose contract meaning is not a per-band fused PSD level.

**No change** to the numeric gate threshold 0.25 dB.  
**No change** to FFT sizes, window name, floors, fusion knees, grid, or SHA256SUMS in this task.

### 3.2 Admissible set `R`

A band index `i` at a compared frame is in `R` iff **all** of the following hold on every FFT path `p ∈ contributing_paths(i)`:

1. **Under-resolution (prior ACTIVE, retained):** ENBW-aware occupancy of the triangular support in path `p` satisfies `N_eff(i, p) ≥ N_MIN` with `N_MIN = 2`.
2. **Main-lobe separation (this rewrite):** nearest-neighbor centre separation in path-`p` bins is at least the Hann main-lobe half-width:

```text
sep_bins(i, p) = min_{j ∈ {i-1, i+1} ∩ [0,119]} |center[j] - center[i]| / Δf_p
sep_bins(i, p) ≥ SEPARATION_MIN_BINS
SEPARATION_MIN_BINS = 2   # = null-to-null / 2 = centre→first-null
```

3. **Path contribution after fusion:**

| Centre frequency | `contributing_paths(i)` |
|---|---|
| `center[i] ≤ 160` | `{LF}` |
| `center[i] ≥ 320` | `{MAIN}` |
| `160 < center[i] < 320` | `{LF, MAIN}` (fail-closed: both must satisfy (1) and (2)) |

Bands failing (1) or (2) are **not scored** for the SR-parity max-|Δ| claim: they are outside `R` (reportable as `EXCLUDED_GEOMETRY`), never silent PASS.

Endpoint bands use only the single existing neighbor (`i=0` → `{1}`; `i=119` → `{118}`).

---

## 4. Constants table with a priori derivation

| Constant | Value | Derivation |
|---|---|---|
| `ENBW_BINS` | 1.5 | Classical periodic Hann equivalent noise bandwidth |
| `N_MIN` | 2 | Prior ACTIVE under-resolution floor: smallest integer occupancy strictly above one ENBW bin in spirit of “more than a single ENBW cell”; **retained, not raised** |
| `MAIN_LOBE_NULL_TO_NULL_BINS` | 4 | Classical periodic Hann main-lobe null-to-null width |
| `SEPARATION_MIN_BINS` | 2 | `MAIN_LOBE_NULL_TO_NULL_BINS / 2` — neighbour must not lie strictly inside the main lobe |
| `Δf_MAIN` | `48000/4096` | Frozen MAIN FFT |
| `Δf_LF` | `48000/8192` | Frozen LF FFT |
| `r` | `1000**(1/119)` | Geometric centre ratio from §6.1 |
| Gate dB | 0.25 | Untouched |

### 4.1 Closed-form frequency thresholds (consequence, not knobs)

For interior bands, `|center[i+1] - center[i]| = center[i]·(r − 1)`.  
The half-lobe predicate `sep_bins ≥ 2` is equivalent to:

```text
center[i] ≥ SEPARATION_MIN_BINS · Δf_p / (r − 1)
```

Numerically (documentation aid only; implementation should use the bin formula in §3.2):

| Path | `2 · Δf_p / (r − 1)` |
|---|---|
| MAIN | ≈ 392.1528 Hz |
| LF | ≈ 196.0764 Hz |

Crossfade bands must clear **both** thresholds in the bin metric (i.e. the MAIN bin test is stricter).

### 4.2 A priori structural census under `SEPARATION_MIN_BINS = 2`

Using only centres + `Δf_p` (no audio, no Δ tables):

| Path | Band indices with `sep_bins < 2` to nearest neighbour |
|---|---|
| MAIN | 53 bands (low end of the grid up through the band whose centre is still below the MAIN threshold above) |
| LF | 41 bands (same construction with `Δf_LF`) |

After fusion path selection (§3.2), the admissible `R` is the intersection of ENBW-`N_MIN` survival and this separation survival on contributing paths. Exact set membership is a pure function of frozen geometry; it must be bit-stable on the gate platform.

**Why not `SEPARATION_MIN_BINS = 4`?**  
Null-to-null (= 4) would demand a full main-lobe *width between centres*, i.e. the neighbour at the far null. The residual diagnosis is membership **inside** the lobe (distance from centre to neighbour below the first null). The matching predicate is therefore half-width = 2. Choosing 4 would be a different, stricter physical claim and is not selected here.

**Why not raise `N_MIN`?**  
Forbidden by mandate when motivated by FAIL magnitudes; also orthogonal to centre-in-lobe inseparability.

---

## 5. What this amend claims — and what it refuses to claim

**Claims (document intent):**

- Per-band SR parity is only a well-posed independent-band claim on `R`, where each admitted centre is both ENBW-occupied and main-lobe-separated from its grid neighbours under the frozen Hann and the contributing FFT path(s).
- Exclusion is geometric and pre-registered; it is not outcome-conditional.

**Refuses:**

- Fitting `N_MIN` / `SEPARATION_MIN_BINS` / fusion knees / FFT sizes to a remeasure table.
- Treating dense multi-tone excitation as a substitute for fixing an ill-posed band claim.
- Touching 0.25 dB, CONTRACT freeze bytes, metrology lock payload, or `SHA256SUMS` in this proposal step.

---

## 6. Honest alternative considered (and not chosen as primary)

A pure **field-definition** rewrite (e.g. replace 120-band PSD parity with a coarser projection, or declare the entire fused vector incomparable) would also remove the ill-posed claim, but would discard band identity that remains geometrically separable at high centres under MAIN/LF. The main-lobe predicate keeps the §7 `*_psd_db[120]` field and removes only the bands for which neighbour centres are definitionally inside the window main lobe.

If redteam shows that even main-lobe-separated bands cannot carry an independent-band SR claim under the frozen frontend for a *non-outcome* structural reason not listed in the mandate, the fallback is a further domain/field amend — not threshold shopping.

---

## 7. Implementation notes (for a later builder; not authorized here)

Document-only now. A future GO’d builder would:

1. Serialize `SEPARATION_MIN_BINS = 2` and the contributing-path rule beside the existing ENBW/`N_MIN` predicate.
2. Emit per-band admission tags: `IN_R` | `EXCLUDED_UNDERRESOLVED` | `EXCLUDED_MAINLOBE` (names illustrative).
3. Compute the gate max-|Δ| **only** on `i ∈ R`.
4. Leave lock FFT/window/floor bytes unchanged unless a separate freeze GO says otherwise.

---

## 8. Exit criteria for this proposal artifact

1. Direction chosen and justified a priori: **domain-on-`R` + main-lobe separation (`SEPARATION_MIN_BINS = 2`), retaining ENBW `N_MIN = 2`.**
2. Constants table with derivation present (§4).
3. Contamination: **CLEAN** (§0).
4. Out of scope here: redteam → judge remeasure → second GO.

---

## 9. Judge coherence + geometric consequence (2026-07-25)

**Coherence: OK.** Two predicates, two window properties (`ENBW` → `N_MIN=2`; main-lobe null-to-null/2 → Rayleigh `SEPARATION_MIN_BINS=2`). Guardrail “`N_MIN` not raised” respected. Audit note: both constants equal `2` for different reasons — keep both derivations visible in normative text.

**A-priori geometric consequence** (bin arithmetic on frozen centres; not a run outcome):

| quantity | value |
|----------|--------|
| `\|R\|` / 120 | **67** / 120 (~56%) |
| first `IN_R` band | index **53** (~433.7 Hz); last excluded **52** (~409.2 Hz) |
| 20–80 Hz held | **0** / 24 |
| 80–200 Hz held | **0** / 16 |
| 200–500 Hz held | **3** / 16 |
| ≥500 Hz held | **64** / 64 |

Crossfade bands must clear **both** path tests → MAIN half-lobe (~392 Hz closed-form) dominates → effectively **no SR-parity claim below ~409 Hz**.

**Product decision required (Marco) — re-measure BLOCKED until answered:**

Separate two claims:

1. *Metrology:* below ~400 Hz, band-to-band SR parity is not well-posed under this grid+window (geometry demonstrates).
2. *Product:* therefore we verify **nothing** below ~400 Hz (does **not** follow automatically).

Alternatives that are not silent inheritance of (1): different low-band instrument (longer window / pre-band PSD), report-only LF region, or a-priori wider LF tolerance — each is a separate amend, not shopping after a PASS number.

**Product decision (Marco, 2026-07-25): report-only LF** — not option-1
blindness, not a wider LF dB tolerance, not a second instrument this tranche.
Bands ∉ `R` do **not** close gate 4; their per-band / per-SR max|Δ| **must**
be published (fail-closed if omitted). Contract must state *why* (grid finer
than window separation). G4 debt: cross-SR low-end detection stability.
Mandate for normative prose: `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_MANDATE.md`.

**Re-measure:** authorized **after** the report-only clause is written and
paper-redteamed — gate max on `i ∈ R` only; report-only table mandatory for
∉`R`.
```

---

## B.8 FILE: `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_MANDATE.md`

**Path logico:** `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_MANDATE.md`  
**Bytes:** 3040  
**Lines:** 62

```markdown
# MANDATO — clausola report-only LF (decisione prodotto Marco)

**Status:** AUTHORIZED 2026-07-25 — untainted writer  
**Product decision:** under the geometric resolvability limit of the frozen
120-band grid + periodic Hann, SR-parity **does not close** gate 4, but
**must** be measured and published (report-only). Not option-1 blindness.
Not a wider LF dB tolerance (forbidden: invent X dB from run residuals).
Not a second instrument (longer FFT / pre-band PSD) in this tranche.

## Destinatario

Untainted writer. Do **not** open remeasure/WS4 evidence / REV7 candidate
run tables. Do **not** use measured max|Δ| to pick constants.

## What is already decided (do not re-litigate)

1. Coherent geometric package from scope rewrite: ENBW `N_MIN=2` **and**
   main-lobe Rayleigh `SEPARATION_MIN_BINS=2` → domain `R` (a priori).
2. Consequence: `R` starts only above ~409 Hz under frozen geometry
   (documentation aid already in scope-rewrite proposal §4 / §9). That
   frequency is a **consequence of the predicates**, not a new knob.
3. Product: bands ∉ `R` are **report-only** for SR-parity — they do **not**
   contribute to the gate-closing max|Δ| ≤ 0.25, but their per-band /
   per-SR max|Δ| **must** appear in the G1e (or successor) numeric report.
4. **0.25 dB unchanged** on `i ∈ R`. No soft-max, no LF alternate threshold.
5. Contract must **state why** report-only: the 120-band grid is finer than
   the analysis window can separate below the geometric limit — not leave
   that as an implied side-effect of a formula.
6. Durable debt: G4 must carry an explicit cross-SR detection-stability
   check on low-end classes (mud/boom/boxy) — registered now, not discovered
   later. Writer drafts the debt sentence; does not invent G4 metrics.

## Compito

Update / write document-only:

1. Extend `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` (or a sibling
   `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`) with **normative**
   contract-shaped prose for:
   - gate-closing domain = `i ∈ R` only (predicates already specified);
   - report-only domain = geometrically excluded bands, mandatory publication
     fields (max|Δ| per band, per SR vs 48k, per required asset/portion);
   - explicit “why report-only” sentence (grid finer than window separation);
   - fail-closed: omitting the report-only table → G1e report **FAIL**
     (not optional appendix);
   - G4 debt registration sentence (cross-SR low-end detection stability).
2. Sync a short status note into
   `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` § product decision
   (without copying run magnitudes).

## Divieti

- No LF tolerance constant X dB.
- No raising `N_MIN` / retuning `SEPARATION_MIN_BINS` from outcomes.
- No CONTRACT freeze / lock / SHA256SUMS edit.
- No re-measure in this task.
- No opening `*REMEASURE*`, `*WS4*EVIDENCE*` outcome tables.

## Esito

Normative clause + contamination statement. Then: redteam on the clause →
independent re-measure (gate on `R` + mandatory report-only table for ∉`R`).
```

---

## B.9 FILE: `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`

**Path logico:** `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`  
**Bytes:** 5423  
**Lines:** 129

```markdown
# REV7 — Report-only LF SR-parity (normative candidate prose)

**Status:** DOCUMENT-ONLY CLAUSE — ≠ CONTRACT consolidated — ≠ G1 PASS  
**Date:** 2026-07-25  
**Authority:** Marco product decision (report-only under geometric limit) via  
`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_MANDATE.md`  
**Geometry package:** `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md`  
(ENBW `N_MIN = 2` ∧ Rayleigh `SEPARATION_MIN_BINS = 2` → domain `R`)

**Does not:** change 0.25 dB; invent an LF alternate dB tolerance; raise
`N_MIN`; retune `SEPARATION_MIN_BINS`; edit freeze CONTRACT / lock /
SHA256SUMS.

---

## 1. Why report-only (must appear in CONTRACT language)

Under the frozen §6 geometry (120-band log grid, triangular supports,
periodic Hann, dual FFT MAIN 4096 / LF 8192, fusion 160/320 Hz), neighbour
band centres can lie inside the analysis main lobe at low centres. Those
bands are **not independently separable** by the frozen window. Therefore
sample-rate parity of per-band features below the geometric resolvability
limit is **not a well-posed closing claim** for gate 4 — not because the
0.25 dB threshold is wrong, and not because the frontend is exempt from
scrutiny there.

This reason must be stated explicitly in the consolidated CONTRACT. It must
not be left as an unspoken side-effect of an admission formula.

---

## 2. Domains

Let `R` be the a-priori set of band indices that satisfy both:

1. under-resolution / ENBW occupancy on every fusion-contributing path
   (`N_MIN = 2 = ceil(ENBW_Hann)`), and
2. neighbour-centre separation on every fusion-contributing path
   (`sep_bins ≥ SEPARATION_MIN_BINS = 2 = main-lobe null-to-null / 2`),

as specified in the scope-rewrite proposal (fail-closed on crossfade: both
LF and MAIN paths must satisfy).

| domain | role |
|--------|------|
| `i ∈ R` | **Gate-closing** SR-parity domain for PSD / shape / prominence (and valid-channel level scalars as already scoped). `max\|Δ\| ≤ 0.25` dB closes or fails gate 4. |
| `i ∉ R` | **Report-only** SR-parity domain. Does **not** enter the gate-closing max. Must still be measured and published. |

Shape/prominence for gate-closing cells remain computed under the prior
fail-closed choice: §7 formulas restricted to support compatible with `R`
(proposal choice B / redteam closure). Report-only bands are tagged
`EXCLUDED_GEOMETRY` (or finer: under-resolved vs main-lobe) — never silent
PASS.

---

## 3. Mandatory publication (fail-closed)

The G1e frontend/benchmark numeric report (or the successor report named in
PLAN for the G1 close package) **MUST** include a table (or machine-readable
equivalent) with at least:

- asset id / portion id (§13.1);
- host sample rate under test (44.1 and 96 vs 48);
- band index `i` for **every** `i ∉ R` (no subsetting / “interesting band”
  cherry-pick); if a field is inactive under the same floor/union rule used
  for gate cells, publish the cell as inactive / N/A with the rule named —
  do not omit the band row;
- field id (`mid_psd_db` / `mid_shape_db` / `mid_prominence_db` / … as
  applicable on that channel);
- `|Δ|` dB vs the 48 kHz reference on the aligned frame (when both sides
  admit the cell under the named activity rule);
- per-asset and overall `max|Δ|` restricted to active cells with `i ∉ R`
  (report-only max — **not** used to close gate 4).

**Omitting this table, or publishing only a prose summary without per-band
numbers, is a report FAIL** — same severity class as omitting a required
gate-4 artifact. Report-only is **not** an optional appendix.

Gate 4 PASS/FAIL language in the report must state the perimeter:

> Gate-closing claim applies only to bands in `R`. Bands outside `R` are
> report-only because the 120-band grid is finer than periodic-Hann
> neighbour separation under the frozen analysis.

Forbidden PASS wording: “SR-parity verified across the full 20 Hz–20 kHz
band grid” while `R` excludes the low region.

---

## 4. What this clause does not authorize

- An alternate LF threshold (e.g. “tolerate X dB below 400 Hz”).
- Dropping shape/prominence/level from the declared domain to hide FAIL.
- Treating empty report-only domain as PASS.
- Silent lock / SHA256SUMS mutation.
- Claiming G1 PASS or Ableton readiness.

---

## 5. Durable debt — G4 cross-SR low-end detections

Register now (PLAN debt list + CONTRACT pointer on consolidate):

> G4 MUST include an explicit verification of cross-sample-rate stability of
> detections on low-end problem classes (at minimum the product classes that
> depend on content below the geometric resolvability limit of the G1 band
> grid — mud / boom / boxy-mid as named in the product taxonomy). This is
> **not** satisfied by G1 gate 4 PASS on `R` alone.

Exact G4 metrics are out of scope for this clause; the debt existence and
trigger (“low-end classes × host SR”) are in scope and mandatory.

---

## 6. Consolidation binding

On Guardian consolidate GO, this clause merges into
`docs/MOTORE_V3_G1_CONTRACT.md` §13.2 gate 4 beside the `R` predicates, and
the metrology lock gains the hashed admission mask procedure for `R` plus a
flag that report-only publication is required. Until then: candidate only.

---

## 7. Contamination

Writer path: mandate + geometric proposal structure. No run-magnitude
shopping. Product decision (report-only vs blind vs alternate instrument)
was taken by Marco before this prose.
```

---

## B.10 FILE: `docs/MOTORE_V3_REV7_REMEASURE_R_REPORT.md`

**Path logico:** `docs/MOTORE_V3_REV7_REMEASURE_R_REPORT.md`  
**Bytes:** 6022  
**Lines:** 88

```markdown
# REV7 independent re-measure — ACTIVE / domain-on-R (ENBW+floor)

**Status:** EVIDENCE — ≠ G1 PASS — ≠ consolidate GO — ≠ anti-leakage claim  
**Date:** 2026-07-25  
**Judge:** independent CC lineage (reproduced WS4 RED; does not author ACTIVE)  
**Perimeter (acceptance A):** ENBW aperture + floor/union only; **leakage OOS** for ACTIVE  
**Harness:** implemented from proposal normative text (§7-on-`R`, choice B) — **not** product spike `sr_parity.py` relay  
**Proposal:** `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md` (redteam closure)  
**Governance:** `N_MIN` not raised; no new anti-leakage cut fitted to these numbers

---

## 1. Geometry (a priori)

| quantity | value |
|----------|--------|
| `\|R\|` / 120 | **96** / 120 |
| unresolved (material) | bands `0–23` except `21`, plus `36` (~161.7 Hz crossover) |
| historical sparse offenders vs R | `b16`, `b18` **out of R**; `b21` **in R** |

---

## 2. Stationary adversarial cells (useful window)

| asset | sr vs 48k | REV6 max\|Δ\| (WS4) | this re-measure (on R) | verdict | peak |
|-------|-----------|---------------------|------------------------|---------|------|
| multitone | 44100 | 4.780 | **4.771** | **FAIL** | `shape` b35 |
| multitone | 96000 | 4.488 | **4.479** | **FAIL** | `shape` b35 |
| pseudo_noise | 44100 | 9.537 | **1.237** | **FAIL** | `psd` b28 |
| pseudo_noise | 96000 | 8.993 | **1.136** | **FAIL** | `psd` b28 |

Vacuous FAIL: **not** triggered. Threshold 0.25 dB unchanged.

**log_sweep:** not executed this round (two stationary assets already determine FAIL). Required before any formal consolidate claim.

---

## 3. What the cure did / did not (sparse fixtures)

- **Did (geometry / under-resolution):** pseudo_noise 9.54 → 1.24 dB (~7.7×). Sparse 0/1-bin bands leave the domain; shape/prominence contamination via full-120 §7 is stopped by choice (B) on `R`.
- **Partial (sparse valleys):** multitone ~unchanged (~4.78 → ~4.77) on shape b35 — RESOLVED band between tones, near-floor skirts. Noise residual on psd b28 similarly sits between partials.
- **Hypothesis (iii) — sparse excitation as sole cause:** tested with dense probe (§4). **Refuted as complete explanation** (4.77 → 1.19 under dense, still FAIL). Do not correct the narrative by adding (iii) as the remaining fix; record that (iii) was measured and is **insufficient**.

---

## 4. Dense-excitation probe (hypothesis (iii) — tested)

**Hypothesis (iii):** residual FAIL on sparse fixtures (multitone / noise) is mostly empty-band / distant-leakage; a dense excitation with one sinusoid at every frozen band centre should largely clear the gate under the same ENBW+floor / §7-on-`R` rules.

**Construction (a priori fixture geometry, not fitted):** 120 equal-amplitude tones at `band_centers_hz()`, total RMS −24 dBFS, analytic render at each host rate, same useful window / nearest `source_time` / ACTIVE / §7-on-`R` pipeline as §2.

| probe | sr vs 48k | max\|Δ\| dB | verdict | peak |
|-------|-----------|------------|---------|------|
| 120 components | 44100 | **1.188** | **FAIL** | `prom` b24 (~81 Hz) |
| 120 components | 96000 | **1.094** | **FAIL** | `prom` b24 (~81 Hz) |

**Result:** hypothesis (iii) is **refuted as a complete explanation**. Sparse excitation mattered (multitone peak ~4.77 → ~1.19 under dense) but is **not sufficient**.

**Diagnostic (geometry of frozen window/grid — not a prescribed constant):** peak at band 24 (~80.6 Hz); neighbour centres ~4.54 Hz apart ≈ **0.78 LF bins**; Hann main-lobe half-width = 2 bins → adjacent band components lie inside each other’s main lobe. Residual failure mode is **inter-band inseparability under the analysis window**, not only distant leakage into empty bands. ENBW occupancy (`N_MIN=2`) can mark such a band `RESOLVED` while neighbours still co-interfere.

Zone sketch (pure geometry; centres vs main-lobe widths): on LF, adjacent centres exceed 2-bin separation only above ~204 Hz; full 4-bin separation only above ~409 Hz (MAIN thresholds higher). No claim here that any particular `N_MIN` would PASS — that measurement is intentionally not run by the contaminated judge.

---

## 5. Structural conclusion (updated after dense probe)

1. Narrowing constraint 4 to ENBW+floor was honest packaging; under that perimeter the gate still **FAILS** on sparse fixtures **and** on the dense probe.
2. Residual after R-restriction is **not** only “distant leakage into empty bands.” Dense probe shows **band-to-band separation** under the frozen Hann main lobe is also in play on `RESOLVED` cells.
3. Next amend must still target **domain and/or field definitions** and/or a **re-opened a-priori geometric criterion** justified from window/grid (including whether ENBW aperture vs main-lobe separation is the right property) — written **before** any further PASS chase. Contaminated judge delivers **diagnosis only**, not a chosen constant.
4. **Forbidden:** raise `N_MIN` / add anti-leakage or separation cuts chosen to clear 4.771 / 1.237 / 1.188; shopping “which N_MIN passes” after seeing these numbers.

---

## 6. Honesty notes on this measure

- Prominence text ambiguity (renorm retained weights vs reflect on R-sequence): implemented **renormalization**. Dense-probe peak is on prominence — dual reading residual C remains material for lock time; does not reverse FAIL under renorm.
- Acceptance B: fields compared are §7-on-`R`, not product §7-120 + mask.
- `log_sweep` still owed before any formal consolidate claim.

---

## 7. Verdict

**RED** under the ENBW+floor / domain-on-`R` candidate perimeter.

Diagnosis: under-resolution sparse bands **addressed** by R; sparse-fixture leakage **partial**; residual **inter-band inseparability** on geometrically `RESOLVED` low bands (dense probe). Hypothesis (iii) tested → **insufficient**.

**REV7 consolidate: NO.** Hand off to untainted scope rewrite with diagnosis “band separation under main lobe” — **without** a prescribed constant from this judge.
```

---

## B.11 FILE: `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md`

**Path logico:** `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md`  
**Bytes:** 2895  
**Lines:** 68

```markdown
# REV7 final re-measure — gate on `R` + report-only `∉R`

**Status:** EVIDENCE — ≠ G1 PASS — ≠ consolidate GO  
**Date:** 2026-07-25  
**Platform:** CPython 3.12.13 / numpy 2.5.1 (`~/aieq_data/motore_v3/env/venv`)  
**Spike runner:** `motore-v3-g1b-spike` / `ml_v3/reports/run_rev7_remeasure_r.py`  
**JSON:** spike `ml_v3/reports/G1B_REV7_REMEASURE_R_REPORT.json`  
**Perimeter:** ENBW `N_MIN=2` ∧ Rayleigh `SEPARATION_MIN_BINS=2` → `|R|=67` (first ≈433.7 Hz);  
report-only packaging per `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`

---

## Gate-closing (`i ∈ R`, §7-on-`R`, ABOVE_FLOOR union)

| asset | vs 48k | max\|Δ\| dB | verdict | peak |
|-------|--------|------------|---------|------|
| multitone | 44100 | **0.1915** | **PASS** | level |
| multitone | 96000 | **0.1898** | **PASS** | level |
| pseudo_noise | 44100 | **0.0458** | **PASS** | level |
| pseudo_noise | 96000 | **0.0464** | **PASS** | level |
| log_sweep | 44100 | **6.4350** | **FAIL** | psd b106 (~9404 Hz) |
| log_sweep | 96000 | **5.8381** | **FAIL** | psd b106 (~9404 Hz) |

**Overall gate:** **FAIL** — `max|Δ| = 6.435 dB` (threshold 0.25).

Stationary adversarial subset **PASSes** under the new perimeter with margin.
`log_sweep` checkpoint mode still fails.

### log_sweep peak (diagnostic)

At checkpoint **16 kHz** (`t_cross≈1.7096`): `psd_ref[106]=−120.0`,
`psd_sr[106]≈−113.56` → admitted by union `max>−120` though reference is on
the clamp floor; instantaneous sweep peak in that frame is near b111
(~12.6 kHz), not b106. Same one-sided floor/union skirt pattern as WS4
smoke on HF checkpoints — **not** an LF geometry issue (b106 ∈ `R`).

---

## Report-only (`i ∉ R` — does **not** close gate)

| asset | vs 48k | max\|Δ\| dB | peak |
|-------|--------|------------|------|
| multitone | 44100 | 4.780 | shape b35 (~152.5 Hz) |
| multitone | 96000 | 4.488 | shape b35 |
| pseudo_noise | 44100 | 9.537 | shape b18 (~56.9 Hz) |
| pseudo_noise | 96000 | 8.993 | shape b18 |
| log_sweep | 44100 | 108.021 | shape b2 (~22.5 Hz) |
| log_sweep | 96000 | 1.563 | shape b51 (~386 Hz) |

These numbers are **published debt**, not gate-closing. Omitting them would
be a report FAIL under the report-only clause.

---

## Verdict

1. Geometric `R` + report-only LF packaging **works as designed** for
   multitone / pseudo_noise (gate PASS).
2. Gate still **FAIL** on `log_sweep` HF checkpoint skirts via floor-union
   activity — separate from the LF inseparability problem already moved to
   report-only.
3. **REV7 consolidate: NO** until sweep admission / checkpoint rule is
   addressed a priori (not by shopping on 6.435). Options belong to a
   follow-up amend (e.g. require both sides above floor + margin, or
   checkpoint band neighbourhood tied to instantaneous peak) — **not**
   raising `N_MIN` / widening 0.25.

≠ G1 PASS. ≠ Ableton readiness.
```

---

## B.12 FILE: `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.json`

**Path logico:** `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.json`  
**Bytes:** 2554  
**Lines:** 104

```json
{
  "artifact": "G1B_REV7_REMEASURE_R_REPORT",
  "first_in_R_hz": 433.67506219748697,
  "gate": [
    {
      "asset": "multitone",
      "fs": 44100,
      "max_abs_db": 0.19149398803710938,
      "n_pairs": 77,
      "peak": "level",
      "verdict": "PASS"
    },
    {
      "asset": "multitone",
      "fs": 96000,
      "max_abs_db": 0.1897754669189453,
      "n_pairs": 77,
      "peak": "level",
      "verdict": "PASS"
    },
    {
      "asset": "pseudo_noise",
      "fs": 44100,
      "max_abs_db": 0.04581642150878906,
      "n_pairs": 77,
      "peak": "level",
      "verdict": "PASS"
    },
    {
      "asset": "pseudo_noise",
      "fs": 96000,
      "max_abs_db": 0.04635047912597656,
      "n_pairs": 77,
      "peak": "level",
      "verdict": "PASS"
    },
    {
      "asset": "log_sweep",
      "fs": 44100,
      "max_abs_db": 6.4350128173828125,
      "n_pairs": 10,
      "peak": "psd b106 (9403.7Hz)",
      "verdict": "FAIL"
    },
    {
      "asset": "log_sweep",
      "fs": 96000,
      "max_abs_db": 5.838142395019531,
      "n_pairs": 10,
      "peak": "psd b106 (9403.7Hz)",
      "verdict": "FAIL"
    }
  ],
  "n_R": 67,
  "note": "\u2260 G1 PASS; \u2260 consolidate; report-only max not used to close gate",
  "overall_gate_max_abs_db": 6.4350128173828125,
  "overall_gate_verdict": "FAIL",
  "perimeter": "ENBW N_MIN=2 \u2227 SEPARATION_MIN_BINS=2 \u2192 R; report-only \u2209R",
  "report_only": [
    {
      "asset": "multitone",
      "fs": 44100,
      "max_abs_db": 4.779563903808594,
      "n_active_cells_sum_over_pairs": 10275,
      "peak": "shape b35 (152.5Hz)"
    },
    {
      "asset": "multitone",
      "fs": 96000,
      "max_abs_db": 4.487525939941406,
      "n_active_cells_sum_over_pairs": 10254,
      "peak": "shape b35 (152.5Hz)"
    },
    {
      "asset": "pseudo_noise",
      "fs": 44100,
      "max_abs_db": 9.537364959716797,
      "n_active_cells_sum_over_pairs": 10857,
      "peak": "shape b18 (56.9Hz)"
    },
    {
      "asset": "pseudo_noise",
      "fs": 96000,
      "max_abs_db": 8.993476867675781,
      "n_active_cells_sum_over_pairs": 10857,
      "peak": "shape b18 (56.9Hz)"
    },
    {
      "asset": "log_sweep",
      "fs": 44100,
      "max_abs_db": 108.02138137817383,
      "n_active_cells_sum_over_pairs": 582,
      "peak": "shape b2 (22.5Hz)"
    },
    {
      "asset": "log_sweep",
      "fs": 96000,
      "max_abs_db": 1.5628204345703125,
      "n_active_cells_sum_over_pairs": 441,
      "peak": "shape b51 (386.1Hz)"
    }
  ],
  "threshold_db": 0.25
}
```

---

## B.13 FILE: `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md (A/A3 stamped)`

**Path logico:** `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md`  
**Bytes:** 18819  
**Lines:** 350

```markdown
# Motore v3 — Proposal: log_sweep HF admission (one-sided floor-union)

**Suggested commit title (if/when Marco authorizes commit):**  
`docs(v3): stamp log_sweep HF admission decisions (A / A3 / fuori REV7 LF)`

| Field | Value |
|-------|--------|
| **Status** | **DECISION STAMPED** — document-only; living choices locked; **uncommitted** until post-redteam/CC |
| **≠** | G1 PASS · REV7 consolidate · G1b tip ufficiale · freeze amend · lock re-hash |
| **Date** | 2026-07-26 |
| **Branch / tip at draft** | `feature/motore-v3-offline` @ `2c69606f` (living next-path) |
| **Freeze contract** | `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` (REV6) |
| **Lock digest (unchanged)** | `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` |
| **Trigger evidence** | `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` (+ `.json`) @ `b3d7f71b` |
| **Living path authority** | PLAN bullet post-`2c69606f`; handoff §A |

## DECISION STAMP (2026-07-26)

| Field | Value |
|-------|--------|
| **Date** | 2026-07-26 |
| **Reviewer** | Marco (confirmed **"ok"** on living choices) |
| **Path** | **Option A** — a-priori admission amend (NOT Option B tip-with-debt) |
| **Mechanism** | **A3** — BOTH: sweep-scoped activity predicate **AND** neighbourhood = **solo `F_TRAJ`** (support ∩ window chirp image + `T_MEM`; **not** peak-local ±K) |
| **REV7 packaging** | Sweep-HF stays **outside** the REV7 LF / geometric-`R` vehicle until measure **PASS** under the new admission; closing max on sweep **must not** silently use `ACTIVE∩R` |
| **Still true** | **REV7 consolidate: NO** · ≠ G1 PASS · ≠ G1b tip ufficiale |
| **Next** | Redteam/CC delta again on patched formula (CC + third redteam + CC-delta empty-N were **POROUS**; HIGH#1/#2 / MED#2 / LOW#3 closed in `MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md` incl. `CHK_CHIRP_REACHABLE` / T18) — measure still blocked until non-POROUS; formula must not self-stamp SOUND; **then** Marco authorizes commit |
| **Concrete formula** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md` (A3 executable boolean; document-only; tip packaging `81e86dc5`; POROUS closures HIGH#1/#2 / MED#2 / LOW#3 in prose) |

**REV7 consolidate: NO** — this document does **not** merge candidate REV7 prose
into the freeze CONTRACT. It addresses only the remaining gate-4 FAIL on
`log_sweep` HF under geometric `R` + report-only LF packaging already decided.

---

## 0. Verdict language (prefer NO-GO)

| Claim | Status |
|-------|--------|
| Gate-4 closable under current REV6 activity on sweep checkpoints | **NO-GO** (measured FAIL) |
| Stationary adversarial subset on geometric `R` | **PASS** (measured; does not close overall gate) |
| This proposal = G1 PASS / G1b tip / REV7 consolidate | **NO** |
| Recommended next lab action | Redteam/CC delta again on patched **A/A3** formula (POROUS closures incl. empty-N `CHK_CHIRP_REACHABLE` / T18 in formula prose) — **not** frontend implement; measure blocked until non-POROUS; commit only after that |

---

## 1. Problem statement (falsifiable, with evidence)

### 1.1 Measured FAIL (final R-remeasure)

Under perimeter ENBW `N_MIN=2` ∧ Rayleigh `SEPARATION_MIN_BINS=2` → `|R|=67`
(first ≈433.7 Hz), gate-closing cells use §7-on-`R` + REV6-style floor-union
`max(psd_ref, psd_sr) > −120`, with report-only packaging for `i ∉ R`
(`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`).

From `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` / `.json`:

| asset | vs 48k | max\|Δ\| dB | verdict | peak |
|-------|--------|------------|---------|------|
| multitone | 44100 / 96000 | **0.1915 / 0.1898** | **PASS** | level |
| pseudo_noise | 44100 / 96000 | **0.0458 / 0.0464** | **PASS** | level |
| log_sweep | 44100 / 96000 | **6.4350 / 5.8381** | **FAIL** | psd **b106** (~9404 Hz) |

**Overall gate:** **FAIL** — `max|Δ| = 6.435 dB` (hard threshold **0.25 dB**).

### 1.2 Smoking-gun cell (HF checkpoint skirt)

At checkpoint **16 kHz** (`t_cross≈1.7096`):

- `psd_ref[106] = −120.0` (clamp floor)
- `psd_sr[106] ≈ −113.56`
- admitted solely by union `max > −120` while the reference sits **on** the floor
- instantaneous sweep peak in that frame is near **b111** (~12.6 kHz), **not** b106

Same one-sided floor/union skirt pattern as WS4 smoke on HF checkpoints
(`docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` §2). This is **not** an LF
geometry / inseparability issue.

### 1.3 Contract clauses that admit the cell (REV6 freeze)

Freeze `6d254d0a` §13.1 (sweep) + §13.2 gate 4:

- **Sweep:** parity uses the preregistered checkpoint grid only (lock:
  `SWEEP_CHECKPOINT_HZ` = 20, 45, 60, 80, 250, 1000, 3500, 8000, **16000**,
  20000 Hz) — nearest frame to `t_cross`, ≤ one hop; missing checkpoint → FAIL.
- **Activity:** cell active iff `max(psd_db_ref, psd_db_sr) > −120` (union);
  shape/prominence inherit PSD activity; **one** active cell above 0.25 →
  entire gate FAIL.
- Lock pin (G1a T4, digest `d2c35ccc…`):
  `activity.predicate = "max(psd_db_ref, psd_db_sr) > -120"`,
  `threshold_max_abs_db = 0.25`, aggregator `max` (mean/p95/RMSE forbidden).

The FAIL is therefore a **predicate / checkpoint-domain admission** question
on a non-stationary fixture — not a resampler impossibility on stationary
content (already PASS on `R`).

---

## 2. Why report-only LF does **not** solve this

Report-only LF (`docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`) moves
`i ∉ R` out of the gate-closing max because neighbour centres lie inside the
Hann main lobe at **low** centres.

| fact | implication |
|------|-------------|
| `first_in_R ≈ 433.7 Hz`, `|R|=67` | geometric carve-out is **LF** |
| peak FAIL band **b106 ≈ 9404 Hz** | **b106 ∈ R** |
| checkpoint 16 kHz is an HF §13.1 obligation | not excludable by ENBW/Rayleigh LF geometry |

Therefore: publishing ∉`R` tables (even the huge report-only `log_sweep`
shape deltas) **cannot** close or excuse the gate-closing FAIL at b106.
Consolidating REV7 **only** with report-only LF + ENBW domain would leave
this FAIL intact — consolidating now would be admission shopping, not
metrology (PLAN living counsel @ `2c69606f`).

---

## 3. Options considered (a priori; ≥3)

Hard constraints common to all options:

- **0.25 dB** hard max on the declared closing domain remains immutable.
- No replace max → mean / p95 / RMSE / post-hoc band subset.
- No `Source/` / ship / Ableton / training in this path.
- No consolidate REV7 inside this document.
- No fitting a new numeric cut against 6.435 / 5.838 (dB shopping).

### Option A — A-priori admission amend for sweep / checkpoint HF one-sided floor-union

**Idea (narrow):** before any consolidate or tip claim, write into candidate
CONTRACT language (then lock, only after GO) a **sweep-specific** or
**checkpoint-HF** admission rule that refuses to close the gate on cells
admitted solely by one-sided floor-union when the reference (or under-test)
is at the §6.1 clamp **and** the cell is not in the instantaneous
signal-bearing neighbourhood of the sweep peak at that checkpoint frame.

Concrete formula **not frozen here** (avoids shopping against 6.435). Design
slot must satisfy, a priori:

1. **Preserve one-sided artifact intent** for true SR defects (a real tone /
   artifact present only at 44.1/96 must still enter the max — pure
   “intersection both > −120” alone is **insufficient** as a complete
   replacement; see REV7 candidate §4 constraint 3).
2. **Target the skirt class:** cells where one side is exactly at floor and
   the other is a leakage/skirt residue far from the instantaneous chirp
   peak (diagnostic shape already recorded: peak near b111, FAIL at b106).
3. **Fail-closed vacuous:** empty active set on a required checkpoint ×
   valid channel → FAIL (not PASS).
4. **Stationary assets unchanged:** multitone / pseudo_noise closing rules
   on `R` must not be silently rewritten by the sweep clause.
5. **Written before** the re-measure that claims PASS.

| tradeoff | |
|----------|--|
| Pro | Closes the metrology question that currently blocks honest tip; aligns with living next-path `2c69606f`; keeps 0.25 intact; stationary PASS evidence remains meaningful. |
| Contro | Requires redteam + CC + later lock re-hash if accepted; risk of over-narrowing one-sided intent if poorly worded; **not** a tip authorization by itself. |
| Risk if skipped | Tip under REV6 would still claim a domain that measured FAIL — false progress. |

### Option B — Stay REV6 + durable debt + proceed G1b tip with explicit non-closing of sweep HF

**Idea:** do **not** amend admission. Tip G1b ufficiale under freeze REV6 /
lock `d2c35ccc…`, with PLAN + tip report stating explicitly that
**gate-4 SR-parity is not closed for `log_sweep` checkpoint HF** (durable
debt), while stationary-on-`R` results may be published as diagnostic /
partial evidence only — **never** as gate-4 PASS.

| tradeoff | |
|----------|--|
| Pro | No lock/CONTRACT mutation; fastest path to “tip exists”; honest if debt is loud and FAIL-closed in wording. |
| Contro | Product G1b tip ships with a known unmet §13.1/§13.2 obligation on a mandatory fixture; easy to launder into “almost PASS”; still blocks any true gate-4 PASS claim. |
| When justified | Only if Marco explicitly accepts **permanent** (or long-lived) non-closing of sweep HF as debt, with stop-rule that tip ≠ G1 PASS and ≠ gate-4 green. |

### Option C — Honest fixture / comparison-mode change (no dB shopping)

**Idea:** keep REV6 activity predicate for stationary assets; change the
**sweep comparison mode** a priori (hashed in lock) so the closing max only
includes bands in a preregistered **neighbourhood of the instantaneous
sweep frequency** at each checkpoint (e.g. bands whose triangular support
contains `f_inst(t_cross)` and/or ±K neighbour indices pinned before
measure) — **or** replace checkpoint PSD max with a preregistered
peak-tracking comparator that does not admit far skirts at floor.

This is **not** “drop b106 after seeing 6.435.” Neighbourhood width / rule
must be justified from window geometry (Hann main lobe / triangular
support) **before** re-measure.

| tradeoff | |
|----------|--|
| Pro | Attacks the mismatch “checkpoint frequency vs band that carries the chirp energy”; may preserve global union predicate for stationary tests; still a priori. |
| Contro | Amends §13.1 sweep closing procedure (lock `checkpoint_hz` / reachability / new neighbourhood constants) — still a contract/lock change later; must not shrink neighbourhood post-hoc to hide FAIL; redteam must attack vacuous PASS and cherry-picked K. |
| Forbidden variant | Deleting 16 kHz / 8 kHz checkpoints after FAIL; raising floor; widening 0.25. |

---

## 4. Locked decision (was: recommended option)

**LOCKED (Marco 2026-07-26):** **Option A** + mechanism **A3**
(sweep-scoped predicate **AND** neighbourhood of the instantaneous sweep
trajectory). **Normative reading (post-CC):** “around `f_inst`” =
triangular support ∩ `F_TRAJ` (PSD-window chirp image + lock `T_MEM`) as
pinned in the formula — **not** Option C’s peak-local ±K band pad (that
pad is explicitly REJECT). Option C’s geometry intuition is absorbed only
via `F_TRAJ`, not as a silent third freeze.
**Option B (tip-with-debt) rejected** as the living path.
Sweep-HF remains **fuori** dal veicolo REV7 LF / geometric-`R` until
measure PASSes under the new admission; sweep closing domain must not
silently become `ACTIVE∩R`.

Rationale (counsel already in PLAN @ `2c69606f`; now stamped):

1. Stationary-on-`R` already **PASS** — the open question is specifically
   sweep HF admission under floor-union, not LF inseparability.
2. Consolidating REV7 without closing this = shopping.
3. Option B is honest only as an explicit **debt tip**; Marco did **not**
   authorize tip-with-debt — A/A3 is the path.
4. Option C alone without admission language risks becoming a post-hoc
   band mask; packaging C **under** A’s a-priori constraints (A3) keeps
   fail-closed intent.

**Lab stance (locked):** **NO-GO** on G1b tip and **NO** on REV7
consolidate until A/A3 is redteamed + independently counter-checked
(ACCEPT or REJECT per §5).

---

## 5. Falsifiable ACCEPT / REJECT criteria

### 5.1 What a follow-up experiment / CC must produce

After a concrete formula draft under **A/A3** (sweep-scoped predicate +
neighbourhood around `f_inst`) is written **without** using 6.435 as a
fit target:

| outcome | verdict |
|---------|---------|
| Re-measure on gate platform (CPython 3.12.13 / lock env): `log_sweep` 44.1 & 96 vs 48, all preregistered checkpoints, under the **written** rule → `max\|Δ\| ≤ 0.25` on the **declared closing cell set**, **and** vacuous-FAIL checks pass, **and** stationary multitone/noise on `R` remain ≤ 0.25 without formula retune | **ACCEPT** candidate for later Guardian amend path (still ≠ consolidate in this doc; still ≠ G1 PASS) |
| Same re-measure still FAIL, but FAIL cells are **signal-bearing** both-sides-above-floor near `f_inst` (true SR defect) | **REJECT** “skirt-only” story → escalate: either deeper frontend bug hypothesis **or** Option B debt (not threshold shopping) |
| Formula only PASSes after widening neighbourhood / raising floor / dropping checkpoints / fitting K to 6.435 | **REJECT** as dB/admission shopping → **NO-GO** |
| Formula excludes all one-sided cells globally such that a planted SR-only artifact at 44.1 vanishes from the max | **REJECT** (violates one-sided artifact intent) |
| Empty active set on any required checkpoint × valid channel treated as PASS | **REJECT** (vacuous) |

### 5.2 Paper attacks redteam must run (before implement)

1. **One-sided intent:** construct (on paper) a cell with `psd_ref = −120`,
   `psd_sr = −100` at a band that **is** the instantaneous peak band —
   must remain ACTIVE / gate-visible.
2. **Skirt exclusion:** cell with ref at floor, sr above floor, band far
   from `f_inst` relative to pinned neighbourhood — must be inactive for
   closing max.
3. **No LF laundering:** confirm b106-class HF cannot be moved to report-only
   via `R`.
4. **Stationary non-regression:** A must not alter multitone/noise closing
   domain except by explicit shared predicates already accepted for `R`.
5. **Lock binding:** list exact lock keys that would change (§6).

**Redteam verdict language:** prefer
`CONTRACT-BROKEN | CONTRACT-POROUS | CONTRACT-SOUND` on the **proposal
formula**, not on this options doc alone. This file alone is **not**
SOUND for consolidate.

---

## 6. Explicit non-goals

| non-goal | |
|----------|--|
| Relax 0.25 → mean / p95 / RMSE / “soft max” | forbidden |
| Edit `Source/`, CMake, Resources, Ableton ship | forbidden |
| Training / model promotion | forbidden |
| Consolidate REV7 in this document | forbidden (**REV7 consolidate: NO**) |
| Claim G1 PASS or official G1b tip | forbidden |
| Mutate `metrology_lock` / SHA256SUMS claiming freeze | forbidden in this tranche |
| Implement `ml_v3/frontend` to “prove” the proposal | forbidden as next step (lab spike ≠ tip; measure only after formula + redteam non-BROKEN) |
| Raise `N_MIN` / shop Rayleigh constants against 6.435 | forbidden |
| Delete or demote HF checkpoints after seeing FAIL | forbidden |

---

## 7. Binding to metrology lock / SHA256SUMS (IF amend later chosen)

**This proposal does not re-hash anything.** If Option A (or C-under-A) later
receives Guardian GO to amend:

| artifact | action |
|----------|--------|
| `ml_v3/contracts/metrology_lock.py` → `sample_rate_parity.activity.predicate` (and any new sweep-neighbourhood / checkpoint-admission keys) | replace string(s); keep `threshold_max_abs_db: 0.25`; keep aggregator `max` |
| `union_cross_sr` / vacuous-FAIL flags | retain intent; extend only as formula requires |
| `SWEEP_CHECKPOINT_HZ` / reachability | unchanged unless Option C neighbourhood adds **new** hashed constants (not removals post-hoc) |
| `metrology_lock_sha256` (`d2c35ccc…`) | **recompute** |
| `fixture_spec` / dependencies embedding lock digest | update if digest-bound |
| `ml_v3/fixtures/g1/SHA256SUMS` | update **only** entries whose bytes change; do not self-hash SUMS |
| Freeze CONTRACT @ `6d254d0a` | superseded only by a later consolidate commit — **not** this proposal |

Silent lock edit without CONTRACT amend GO → **FAIL** / stop-rule BLOCKER.

---

## 8. Next permitted action (after DECISION STAMP)

**Living choices locked** (see DECISION STAMP). **In order:**

1. ~~Marco counter-check A vs B~~ — **DONE** 2026-07-26 (A / A3 / fuori REV7 LF).
2. ~~Concrete A/A3 formula draft~~ — **DONE** (doc-only):
   `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md`
   (`ACTIVE_sweep`, F1, `N`:=support∩`F_TRAJ`, vacuous/report/anti-launder).
3. ~~Tip packaging + independent CC @ `81e86dc5`~~ — **DONE**: verdict
   **CONTRACT-POROUS**; must-fixes 1–7 closed in formula prose (uncommitted
   until Marco OK). Formula **must not** self-declare `CONTRACT-SOUND`.
4. **Redteam/CC delta again** on the patched formula (empty-N /
   `CHK_CHIRP_REACHABLE` / T18 + tie-break / b106 lattice / level pins) —
   not on frontend code. **Re-measure blocked** until verdict ≠
   `CONTRACT-POROUS` / `CONTRACT-BROKEN`.
5. Only then: candidate CONTRACT amend path for **sweep admission**
   (still **separate** from full REV7 consolidate of LF+R; sweep-HF stays
   outside that vehicle until measure PASS; no `ACTIVE∩R` closing on sweep).
   Marco authorizes **commit** of this stamp / formula only after
   non-POROUS CC (or explicitly sooner).

**Not permitted as next step:** implement/close gate in `ml_v3/frontend/`;
ship; training; claim PASS from stationary-only tables; consolidate REV7
“because R PASS”; tip-with-debt (B) without a new Marco override.

---

## 9. Open questions for Marco — CLOSED (stamped)

| # | Question | Decision (2026-07-26) |
|---|----------|------------------------|
| 1 | Option A vs B tip-with-debt? | **A** (NOT B) |
| 2 | Mechanism: sweep-scoped / neighbourhood / both? | **A3 — BOTH** |
| 3 | Sweep-HF inside REV7 LF/`R` vehicle before PASS? | **NO — fuori** until measure PASS under new admission |

---

## 10. References (authority order used)

1. `docs/MOTORE_V3_PLAN.md` — living next-path @ `2c69606f`
2. `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` — §13.1 sweep, §13.2 gate 4
3. `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` + `.json`
4. `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md`
5. `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md`,
   `docs/MOTORE_V3_REV7_ACTIVE_FORMULA_PROPOSAL.md`,
   `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` — context only; **not**
   consolidated here
6. `docs/EMBER_CORE_PARALLEL_HANDOFF.md` §A

---

≠ G1 PASS. ≠ REV7 consolidate. ≠ G1b tip. ≠ Ableton readiness.
```

---

## B.14 FILE: `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md (@ 04e47b39 POROUS closures)`

**Path logico:** `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md`  
**Bytes:** 40091  
**Lines:** 784

```markdown
# Motore v3 — Concrete formula: log_sweep A3 admission (`ACTIVE_sweep`)

**Suggested commit title (if/when Marco authorizes commit):**  
`docs(v3): close CC/third-redteam POROUS on log_sweep A3 formula`

| Field | Value |
|-------|--------|
| **Status** | **FORMULA DRAFT — third-redteam + CC-delta POROUS closures** — document-only; tip packaging `81e86dc5`; independent CC + third redteam + CC delta returned **CONTRACT-POROUS**; this prose keeps CC must-fixes 1–7 and closes HIGH#1 (tie-break), HIGH#2 (empty-N / `CHK_CHIRP_REACHABLE`), MED#2 (b106 lattice), LOW#3 (level); **ready for redteam/CC delta again**; this document **must not** self-declare `CONTRACT-SOUND` (only independent redteam/CC may stamp that verdict); **re-measure still blocked** until non-POROUS acceptance; falsification-only still OK after non-POROUS |
| **Parent stamp** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` (A · A3 · fuori REV7 LF) |
| **Decisions** | **A** (a-priori admission amend) + **A3** (sweep-scoped predicate **AND** neighbourhood = **solo `F_TRAJ`** — **not** peak-local ±K) |
| **Date** | 2026-07-26 |
| **Freeze contract** | `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` (REV6) — **not edited** |
| **Lock digest** | `d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` — **not re-hashed** |
| **SHA256SUMS** | untouched |
| **Contamination guard** | No constant fitted to 6.435 / 5.838 dB; widths from §6 + fixture_spec + lock delay only |

**≠** G1 PASS · ≠ REV7 consolidate · ≠ G1b tip · ≠ frontend implement · ≠ re-measure authorization · ≠ self-`CONTRACT-SOUND`

---

## 0. Closing statement (normative boolean — not deferred)

This note pins a single executable boolean

```text
ACTIVE_sweep(b, ch, chk) ∈ {true, false}
```

and the gate-4 closing cell set for `log_sweep` checkpoint portions. There is
**no** “formula deferred” slot. Downstream **independent** redteam/CC judges
this prose and alone may emit `CONTRACT-SOUND` / `CONTRACT-POROUS` /
`CONTRACT-BROKEN`. This document **forbids** self-stamping `CONTRACT-SOUND`
(including via commit-message authority shopping). Re-measure remains
**blocked** until a non-POROUS / non-BROKEN acceptance and Marco OK.

---

## 0.1 Must-fix changelog (this draft)

Independent CC @ tip `81e86dc5` returned **CONTRACT-POROUS**; third
redteam returned **CONTRACT-POROUS** again (HIGH#1 / MED#2; empty-N /
chirp-reachability as HIGH#2). Closures in this prose (no remeasure; no
lock/CONTRACT/SHA256SUMS edit):

| # | Must-fix | Closure |
|---|-----------|---------|
| 1 | A3 = solo `F_TRAJ`, non peak-local ±K | §1 / §4.0 / §4.2 / T1 rewrite / T12: neighbourhood := support ∩ `F_TRAJ` only; any peak-local / ±K / PSD-neighbour pad → REJECT |
| 2 | Vietare closing `ACTIVE∩R` sul sweep | §3.2 / §3.3 / §6.3 / T13: closing domain = `{ACTIVE_sweep}` alone until Guardian consolidate GO; `ACTIVE_sweep ∩ R` / `i∉R` / `EXCLUDED_GEOMETRY` on sweep closing max → report FAIL |
| 3 | `AT_FLOOR` float32 pin | §3.1: `AT_FLOOR` ⇔ stored post-clamp `float32` `psd_db == −120`; near-floor / ε-tolerance forbidden |
| 4 | Lattice warm-up consistency | §2.3: eligible frames only on useful-segment lattice (`frame_end = N_LF + m·H`) with `source_time` in §13.1 common useful window; pre-warm-up / off-lattice → FAIL |
| 5 | Pairing cross-SR pin | §2.3.1: each `sr ∈ {44100,96000}` selects its own nearest useful frame to `t_cross`; compare pair `(ref_48k*, sr*)`; `N`/`f_inst` only from ref; other pairing → FAIL |
| 6 | Portion hashed identity | §1.1: `portion_id = checkpoint_grid` is the sole A3 portion token; mismatch / alias → FAIL; future lock key sketched (not hashed this tranche) |
| 7 | Status ≠ self-SOUND | header / §0 / §10: no self-`CONTRACT-SOUND`; ready for redteam/CC delta; remeasure blocked until non-POROUS |
| **HIGH#1** | Tie-break order ≠ lock | §2.3 / §2.3.1 / §8.1 / T21: `alignment.tie_break` = `[smaller_frame_index, then smaller_frame_end_sample]` — byte-identical to lock digest `d2c35ccc…` / `metrology_lock.py`; inverted order deleted |
| **HIGH#2** | Empty `N` at chk 20 Hz / `st* ∉ [t_start,t_end]` | §2.3.2 / §4.1 / §5 / T18: **one** fail-closed rule — `CHK_CHIRP_REACHABLE ⇔ t_start ≤ source_time* ≤ t_end`; else checkpoint **FAIL** (no skip, no `t_cross` proxy, no clamp `st*` into chirp, no blanket `ONE_SIDED` exemption / 6.44 shopping) |
| **MED#2** | Escape «different hashed lattice» → `b106∉N` | §4.2 / §6.2 / T19: under lock digest `d2c35ccc…` + §2.3/§4, `b106_in_N_at_16k` **MUST** be `true`; alternate lattice only after Guardian lock amend; private / unamended hashes → geometric **FAIL** |
| **LOW#3** | `level_dbfs` vs N/ONE_SIDED | §3.1 / §3.3 / T20: `level_dbfs` enters gate-4 max iff channel valid on both paired frames; **not** subject to `N` / `ONE_SIDED_FLOOR_UNION` exemption |

**Preserved prior SOUND closures (unchanged intent):** nearest-frame REF-48k
`N`; lock `T_MEM`; §6.2 window binding; honest `b106 ∈ N(16000)`; F1
boolean; analytic `f_inst`; vacuous FAIL; immutable `0.25` dB / aggregator
`max` (no relax).

---

## 1. Scope (A3 only)

Let `asset`, `portion`, `ch`, `b`, `chk` denote asset id, §13.1 portion,
channel role, band index `b ∈ {0,…,119}`, and preregistered sweep checkpoint.

```text
SWEEP_A3_SCOPE(asset, portion) ⇔
    asset = log_sweep
  ∧ portion = checkpoint_grid
```

| case | activity rule |
|------|----------------|
| `SWEEP_A3_SCOPE` | `ACTIVE_sweep` of §3 (this note) |
| all other §13.1 assets / portions (multitone, pseudo_noise, …) | **unchanged** stationary / REV6 (or later consolidated geometric-`R`) activity — **not** rewritten by A3 |

**A3 neighbourhood meaning (normative disambiguation):** the phrase
“neighbourhood around `f_inst`” in the parent stamp **means exclusively**
the geometry of §4: triangular support ∩ `F_TRAJ(chk)`, where `F_TRAJ` is
the analytic chirp image of the §6.2 PSD window (+ `T_MEM`). It does **not**
mean a peak-local band pad (±K indices around the band containing `f_inst`),
PSD-argmax neighbours, or any Hz disk centred on `f_inst` alone.

**REV7 LF packaging:** sweep-HF admission stays **outside** the report-only
`i ∉ R` / `EXCLUDED_GEOMETRY` vehicle until a later measure **PASS** under this
formula (parent stamp). A3 is **not** an `EXCLUDED_GEOMETRY` reason.

### 1.1 Portion identity (fail-closed — not re-hashed this tranche)

```text
A3_PORTION_ID := "checkpoint_grid"
```

- `SWEEP_A3_SCOPE` holds only when `portion = A3_PORTION_ID` exactly
  (string identity).
- Renaming, aliasing, splitting, or merging sweep portions to avoid A3 or to
  mix stationary / geometric-`R` rules → report **FAIL**.
- Future lock binding (sketch only; **not** hashed in this tranche):
  `sample_rate_parity.sweep.portion_id = "checkpoint_grid"`. Until that key
  exists, evaluators **MUST** hard-pin the same string; mismatch with this
  note → FAIL.

---

## 2. Anchors (frozen geometry / fixture_spec only)

### 2.1 Analysis geometry (§6)

| symbol | value | authority |
|--------|-------|-----------|
| `fs_c` | `48000` | §6 / lock |
| `N_MAIN` | `4096` | §6.2 |
| `N_LF` | `8192` | §6.2 |
| `H` | `1024` | §6.2 / lock |
| `T_MAIN` | `N_MAIN / fs_c` | derived |
| `T_LF` | `N_LF / fs_c` | derived |
| `T_HOP` | `H / fs_c` | match radius §13.1 / fixture_spec |
| `Δf(N)` | `fs_c / N` | FFT bin spacing |
| window | periodic Hann `0.5 - 0.5·cos(2πn/N)` | §6.2 |
| `MAIN_LOBE_NULL_TO_NULL_BINS` | `4` | classical periodic Hann (same pin as scope-rewrite) |
| `W_LOBE_MAIN_HZ` | `(MAIN_LOBE_NULL_TO_NULL_BINS / 2) · Δf(N_MAIN)` = `2 · fs_c / N_MAIN` | half null-to-null in Hz on MAIN |
| centres | `center[i] = 20 · (20000/20)**(i/119)`, ends pinned | §6.1 / `band_centers_hz` |
| triangular support | `lo(b), hi(b)` from previous/next centre; virtual ends via same ratio | §6.1 |
| fusion knees | LF pure `≤160`, MAIN pure `≥320`, raised-cosine crossfade | §6.2 |
| PSD floor / clamp | linear `1e-12`, clamp dB `[-120, +12]` | §6.1 |
| gate threshold | `0.25` dB, aggregator `max` | §13.2 **immutable** |

### 2.2 Log-sweep law (fixture_spec)

Pinned by `ml_v3/contracts/fixture_spec.py` / frozen fixture-spec artifact:

```text
t_start = 1/2 s
t_end   = 7/4 s
T_active = t_end - t_start
f_start = 20 Hz
f_end   = 20000 Hz

for t ∈ [t_start, t_end]:
  u(t) = (t - t_start) / T_active
  f(t) = f_start · (f_end / f_start) ** u(t)     # instantaneous_freq_hz

t_cross(f_chk) = sweep_crossing_time(f_chk)       # inverse of f(·) at f_chk
```

`SWEEP_CHECKPOINT_HZ` and reachability (`nearest useful frame` within
`T_HOP`) are unchanged.

`t_cross` is used **only** as the checkpoint target for frame selection
(§2.3). It is **forbidden** as a substitute for `source_time*` when computing
`f_inst`, `t_win_*`, `F_TRAJ`, or `N` membership (§8 quarantine).

### 2.3 Checkpoint frame, `f_inst`, lattice eligibility, and single cross-SR `N`

**Reference stream for neighbourhood geometry:** after cross-SR pairing
(§2.3.1), `N(chk)` and `f_inst(chk)` are computed **once** from the
**reference 48 kHz** selected frame. The same `N(chk)` applies to every
host-rate cell of that checkpoint. Per-host recomputation of `N` / `f_inst`
from 44.1 kHz or 96 kHz frames is **forbidden**.

**Useful-segment lattice (normative — reconciles §8.2 with CONTRACT §13.1):**

```text
# Per gate rate fs (identity at 48 kHz ⇒ gd = 0):
frame_end_sample ∈ { N_LF + m·H | m ∈ ℕ₀ }
source_time(fs)  = frame_end_sample / fs_c − resampler_group_delay_seconds(fs)

# Eligible iff source_time lies in the §13.1 common useful window:
useful_start = max_fs warm_up_seconds(fs)     # additive warm-up; lock
useful_end   = T_asset − coda_seconds()
eligible(fs) ⇔ useful_start ≤ source_time(fs) ≤ useful_end
```

Only **eligible** frames may be selected. Pre-warm-up frames, coda frames,
and off-lattice `frame_end_sample` values are **not** candidates. Choosing a
non-eligible in-radius stamp to shift `F_TRAJ` → report **FAIL**.

For each checkpoint `chk` with frequency `f_chk ∈ SWEEP_CHECKPOINT_HZ`:

1. `t_cross = sweep_crossing_time(f_chk)` — selection target only.
2. On the **48 kHz** eligible lattice, select the `V3FeatureFrame` whose
   `source_time` minimizes `|source_time - t_cross|`. Tie-break equals lock
   `alignment.tie_break` byte-identical under digest `d2c35ccc…`:
   **smaller `frame_index`, then smaller `frame_end_sample`**
   (`metrology_lock.py` / `metrology_lock.json`). Require that distance
   `≤ T_HOP`; else checkpoint-missing **FAIL** (§13.1).
3. Let `source_time*(chk)` and `frame_end_sample*(chk)` be that **48 kHz**
   frame’s stamps. Let
   `dt(chk) := source_time*(chk) - t_cross`.
4. Apply §2.3.2 chirp-reachability **before** `f_inst` / `t_win_*` / `N`.

```text
f_inst(chk) := f( source_time*(chk) )     # only if CHK_CHIRP_REACHABLE (§2.3.2)
```

**Forbidden as neighbourhood centre (normative):**
`argmax_b psd_db[b]` on either render, any smoothed peak tracker, or any
other data-dependent frequency. Diagnostic peak quotes (e.g. “energy near
b111”) are **not** inputs to `N`.

**Forbidden as membership evidence:** using `t_cross` (or any other
in-radius frame than the §2.3 minimizer) in place of `source_time*` when
building `F_TRAJ` / `N`.

**Forbidden as neighbourhood definition:** peak-local ±K band pads, “bands
whose support contains `f_inst` only”, Hz disks about `f_inst`, or any
construction other than §4 `F_TRAJ` (§4.0).

`T_HOP` enters **only** as the locked frame-selection match radius that
defines which `source_time*` is admissible. It does **not** further dilate
the frequency aperture of `N` (§4.2) — dilation would double-count the same
timing budget and is rejected. `|dt| ≤ T_HOP` alone does **not** imply
chirp reachability (§2.3.2).

### 2.3.1 Cross-SR pairing (normative)

For every under-test rate `sr ∈ {44100, 96000}` at checkpoint `chk`:

```text
frame_sr*(chk) := argmin_{eligible frames at sr} |source_time − t_cross|
                  tie-break: smaller frame_index, then smaller frame_end_sample
                  # = lock alignment.tie_break under digest d2c35ccc…
require |source_time(frame_sr*) − t_cross| ≤ T_HOP   else FAIL (checkpoint missing)

compare cell (b, ch, chk) on the pair:
  ( frame_ref_48k*(chk), frame_sr*(chk) )
```

- `N(chk)` / `f_inst(chk)` / `t_win_*` come **only** from `frame_ref_48k*`.
- Manual shifts, ±1-hop shopping to minimize `|Δ|`, pairing by raw output
  index, or any host↔ref match other than the per-rate nearest-to-`t_cross`
  rule → report **FAIL** (lock `alignment` intent; this note makes the
  sweep pairing explicit).
- PSD / shape / prominence values are read from the paired frames; membership
  `b ∈ N(chk)` never uses the host-rate stamps.
- Host `frame_sr*` is still selected by §2.3.1 even when ref fails
  §2.3.2; the checkpoint outcome remains **FAIL** (pairing does not repair
  chirp-unreachability).

### 2.3.2 Chirp reachability / empty-`N` pin (HIGH#2 — one fail-closed rule)

**Chosen a-priori rule (sole normative pin for this porosity):**

```text
CHK_CHIRP_REACHABLE(chk) ⇔
    t_start ≤ source_time*(chk) ≤ t_end
```

where `source_time*` is the §2.3 REF-48k nearest-to-`t_cross` stamp
(after `|dt| ≤ T_HOP` and lock tie-break).

**If `¬CHK_CHIRP_REACHABLE(chk)` → gate-4 FAIL for that checkpoint**
(chirp-unreachable / N-geometry unreachable). This is mandatory for every
`chk ∈ SWEEP_CHECKPOINT_HZ`, including **chk 20 Hz**, where nearest REF-48k
under the lock lattice yields `source_time* ≈ 0.4907 < t_start = 0.5`
while still `|dt| ≤ T_HOP`.

**Forbidden repairs (any one → report FAIL / REJECT):**

| repair | why forbidden |
|--------|----------------|
| Skip / N/A / soft-pass the checkpoint | mandatory chk must score |
| Substitute `t_cross` for `source_time*` in `f_inst` / `t_win_*` / `N` | §2.3 / §8.1 quarantine |
| Clamp / project `source_time*` into `[t_start, t_end]` | changes the selected frame’s law |
| Widen `t_win_*`, drop `T_MEM`, or otherwise force non-empty `N` | free aperture shopping |
| Treat `N(chk) = ∅` as blanket `ONE_SIDED_FLOOR_UNION` exemption then PASS | empty-N porosity / repair shopping |
| Shop constants from measured 6.435 / 5.838 dB | contamination (§0.1 / T8) |

Only after `CHK_CHIRP_REACHABLE` may the evaluator compute `f_inst`,
`t_win_*`, `F_TRAJ`, and `N` (§4). Empty `N` is therefore **not** an
admission escape: either the checkpoint already FAILed on §2.3.2, or an
inverted window after clamps FAILs under §4.1 / §5.

### 2.4 Resampler memory (lock-derived — no dB fit)

Fail-closed FIR / group-delay memory for the trajectory aperture, a priori
from the frozen lock only:

```text
T_MEM := max_{fs ∈ GATE_SAMPLE_RATES} resampler_group_delay_seconds(fs)
       = max_fs (num_taps(fs) - 1) / (2 · up(fs) · fs)     # identity → 0
```

Under the current lock (`GATE_SAMPLE_RATES = {44100, 48000, 96000}`):

| `fs` | `num_taps` | `resampler_group_delay_seconds` |
|------|------------|----------------------------------|
| 48000 | identity (`None`) | `0` |
| 44100 | `20481` | `16/11025` ≈ `0.001451247` s |
| 96000 | `257` | `1/750` = `0.001333…` s |

Hence `T_MEM = 16/11025` s. No dB fit, no taps count chosen from FAIL bands.

---

## 3. Normative boolean `ACTIVE_sweep`

### 3.1 Floor / union atoms (REV6 intent retained — float32 pin)

On the checkpoint-paired frames of `chk` (§2.3.1), channel `ch` (only if that
channel is valid on both paired renders; else the cell is ignored as in §7 /
§13.2). PSD values may come from any host rate under test; membership
`b ∈ N(chk)` always uses the single 48 kHz neighbourhood of §2.3 / §4.

**`level_dbfs` pin (LOW#3):** `level_dbfs` enters the gate-4 closing max
**iff** the channel is valid on **both** paired frames. It is **not**
subject to `N(chk)` membership or `ONE_SIDED_FLOOR_UNION` exemption —
those restrict only PSD / inherited shape / prominence cells under F1.

**Type pin:** `psd_db_*` atoms below are the **stored post-clamp `float32`**
fields of `V3FeatureFrame` after §6.1 floor/clamp. Recomputing in float64,
promoting, or applying a “near floor” tolerance is **forbidden**.

```text
ABOVE_FLOOR_UNION(b, ch, chk) ⇔
    max( psd_db_ref[b, ch, chk], psd_db_sr[b, ch, chk] ) > -120

AT_FLOOR(x) ⇔ (x is float32) ∧ (x == float32(-120))
            # exact equality on the stored post-clamp value; no ε, no “≈ −120”

ONE_SIDED_FLOOR_UNION(b, ch, chk) ⇔
    ABOVE_FLOOR_UNION(b, ch, chk)
  ∧ ( AT_FLOOR(psd_db_ref[b, ch, chk]) ∨ AT_FLOOR(psd_db_sr[b, ch, chk]) )
```

`ONE_SIDED_FLOOR_UNION` is exactly the class admitted solely by floor-union
while at least one side sits on the §6.1 clamp (the smoking-gun skirt class).

Both-sides-strictly-above-floor cells have
`¬ONE_SIDED_FLOOR_UNION` and are **not** restricted by `N`.

### 3.2 F1 pin (neighbourhood = exemption restrictor — not closing domain)

**F1 (normative):** The neighbourhood `N(chk)` restricts **only** the
continued admission of the `ONE_SIDED_FLOOR_UNION` class. It does **not**
replace the gate-4 closing domain with “bands in `N` only.”

Consequences:

1. If `ABOVE_FLOOR_UNION ∧ ¬ONE_SIDED_FLOOR_UNION` → cell is ACTIVE whether
   or not `b ∈ N(chk)` (true both-sides signal / SR content stays gate-visible
   everywhere in the 120-band grid). Geometric `R` does **not** enter this
   implication for sweep (see §3.3 / §6.3).
2. If `ONE_SIDED_FLOOR_UNION ∧ b ∈ N(chk)` → cell is ACTIVE (one-sided
   artifact **inside** the signal-bearing neighbourhood remains gate-visible).
3. If `ONE_SIDED_FLOOR_UNION ∧ b ∉ N(chk)` → cell is **inactive** for the
   gate-4 max (far skirt / leakage residue).
4. **Rejected alternate form:** `closing_domain := N(chk)` alone. That form
   is **not** adopted here.
5. **Rejected alternate form:** `closing_domain := ACTIVE_sweep ∩ R` (or any
   silent intersection with geometric `R` / `i ∉ R` / report-only LF) on
   `log_sweep` / `checkpoint_grid`. Vehicles stay separate (§6.3).

### 3.3 Single closing predicate and closing domain

```text
ACTIVE_sweep(b, ch, chk) ⇔
    SWEEP_A3_SCOPE
  ∧ ABOVE_FLOOR_UNION(b, ch, chk)
  ∧ (
        ¬ ONE_SIDED_FLOOR_UNION(b, ch, chk)
      ∨   b ∈ N(chk)
    )
```

Shape / prominence cells on the same `(b, ch, chk)` **inherit**
`ACTIVE_sweep` from the channel PSD exactly as §13.2 inheritance works for
REV6 activity.

**Level scalars:** `level_dbfs` enters the closing max iff the channel is
valid on both paired frames (§3.1). It does **not** inherit
`ACTIVE_sweep` / `N` / `ONE_SIDED_FLOOR_UNION` exemption.

**Closing domain (fail-closed, until Guardian consolidate / amend GO):**

```text
CLOSING_CELLS_sweep := { (b, ch, chk) | ACTIVE_sweep(b, ch, chk) = true }
```

Gate-4 aggregator on the sweep checkpoint portion:

```text
max |Δ|  over CLOSING_CELLS_sweep
         (PSD / inherited shape / prominence)
         ∪ { level_dbfs on channels valid on both paired frames }
```

**Forbidden closing domains on sweep (report FAIL if used):**

- `ACTIVE_sweep ∩ R`
- `ACTIVE_sweep \ {i ∉ R}` / any `EXCLUDED_GEOMETRY` filter on the closing max
- `N(chk)` alone
- any post-hoc band subset fitted to measured `|Δ|`

Hard threshold `0.25` dB and aggregator `max` remain immutable. One active
cell above threshold → entire gate FAIL. Geometric-`R` / report-only LF remain
a **separate** vehicle and must not silently intersect the sweep closing max.

---

## 4. Neighbourhood `N(chk)` (frozen geometry — no dB fit)

### 4.0 Sole definition — `F_TRAJ` only (CC must-fix 1)

```text
N(chk) := { b | (lo(b), hi(b)) intersects F_TRAJ(chk) }
```

**Normative:** A3 neighbourhood **=** §4.1–§4.2 only.

**Forbidden neighbourhood definitions (any one → REJECT / report FAIL):**

| forbidden form | why |
|----------------|-----|
| ±K band indices about `argmin_b |center[b] − f_inst|` | peak-local pad; drops smoking-gun skirts (e.g. b106 at 16 kHz) while keeping the `f_inst` band |
| `{ b | f_inst ∈ (lo(b), hi(b)) }` alone | peak-containment only; not the window chirp image |
| Hz disk / lobe pad about `f_inst` (incl. ±`W_LOBE_MAIN_HZ`) | free dilation knob; `W_LOBE` is documentation-only (§2.1) |
| PSD-argmax ±K / peak-tracker neighbours | data-dependent; §2.3 already forbids PSD centre |

`f_inst` enters **only** as (a) the analytic stamp for `N_ANAL` knee
selection in §4.1 and (b) the upper end of `F_TRAJ` via `t_win_hi =
source_time*` under the chirp law — **not** as a local band-pad centre.

### 4.1 Analysis time support = §6.2 causal PSD window image (+ lock memory)

Normative binding: the trajectory interval is the **source-time image of the
same causal right-aligned sample window** used for the fused PSD of the
selected **48 kHz** frame (§6.2), extended on the low side by `T_MEM`
(§2.4).

At identity 48 kHz (`resampler_group_delay_seconds(48000) = 0`):

```text
source_time*(chk) = frame_end_sample*(chk) / fs_c

# §6.2: samples [frame_end_sample* - N_ANAL, frame_end_sample*) at fs_c
N_ANAL(f_inst) :=
    N_LF    if f_inst(chk) < 320 Hz    # LF material (pure or crossfade)
    N_MAIN  if f_inst(chk) ≥ 320 Hz    # MAIN-pure

T_ANAL(f_inst) := N_ANAL(f_inst) / fs_c

# Continuous source-time image of that sample window, fail-closed + T_MEM:
t_win_hi(chk) := min( t_end,   source_time*(chk) )
t_win_lo(chk) := max( t_start, source_time*(chk) - T_ANAL(f_inst(chk)) - T_MEM )
```

Proof of stamp identity at REF-48k: with `gd = 0`, the exclusive-end sample
`frame_end_sample*` maps to `source_time*`, and the first sample of the
right-aligned window maps to `source_time* - T_ANAL`. `T_MEM` then extends
`t_win_lo` earlier by the lock max group-delay so FIR memory at non-identity
gate rates cannot shrink the aperture. No look-ahead past `t_win_hi`.

**Precondition:** §2.3.2 `CHK_CHIRP_REACHABLE` must hold; otherwise do **not**
evaluate `t_win_*` / `N` — checkpoint already FAIL.

If `t_win_lo > t_win_hi` after clamping despite `CHK_CHIRP_REACHABLE` →
`N(chk)` is undefined for admission; gate-4 **FAIL** for that checkpoint
(§5). Do **not** interpret inverted-window / empty `N` as a blanket
`ONE_SIDED_FLOOR_UNION` exemption or soft-pass.

**Derivation:** HF checkpoints (incl. 8/16/20 kHz) are MAIN-pure, so the
binding aperture is the MAIN window image of the chirp (+ `T_MEM`). LF /
crossfade checkpoints use the longer LF window fail-closed when LF is
material. `W_LOBE_MAIN_HZ` is recorded in §2.1 as the Hann half-lobe scale;
at HF it is ≪ one triangular support width, so it is **not** used as an
extra Hz dilation of the aperture (a ±`W_LOBE` pad is a free parameter that
is not required once triangular supports discretize the trajectory — and is
rejected to avoid padding knobs).

### 4.2 Chirp image and band membership

```text
F_TRAJ(chk) := { f(t) | t ∈ [t_win_lo(chk), t_win_hi(chk)] }
            = [ f(t_win_lo(chk)), f(t_win_hi(chk)) ]
              # monotone increasing log-chirp on the active interval

b ∈ N(chk)  ⇔  (lo(b), hi(b)) intersects F_TRAJ(chk) as open intervals
            ⇔  lo(b) < f(t_win_hi(chk))  ∧  hi(b) > f(t_win_lo(chk))
```

Bit-stable evaluation: centres / `lo` / `hi` / `f(·)` in IEEE-754 binary64
with the same closed forms as fixture_spec + §6.1 (virtual ends via ratio
`r = (20000/20)**(1/119)`). No measured tables. No dependence on PSD values.
`N(chk)` is a function of the 48 kHz stamps of §2.3 only.

**Honesty pin (efficacy, not a PASS path — MED#2):** under lock digest
`d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e` and the
normative §2.3 / §4 path at 16 kHz, `b106 ∈ N(16000)` (§8.2) is
**mandatory**. A report that claims evaluation under this formula and
publishes `b106 ∉ N(16000)` / `b106_in_N_at_16k = false` → geometric
**FAIL**. Alternate lattices (different hop / warm-up / selection) are
admissible **only** after an explicit Guardian lock amend that re-hashes
`alignment` / lattice keys; private hashes, unamended digests, or
“different hashed lattice proof” attachments that are not that amend →
geometric **FAIL**. If frontend PSD are unchanged, falsification measure
is still expected to **FAIL** on that ACTIVE cell — that is honest
ACCEPT=FAIL territory, not a licence to shrink `N`.

### 4.3 Constants table (hashable later; not hashed in this tranche)

| Constant | Value | Derivation (a priori) |
|----------|-------|------------------------|
| `fs_c` | `48000` | §6 |
| `N_MAIN` | `4096` | §6.2 |
| `N_LF` | `8192` | §6.2 |
| `H` | `1024` | §6.2 |
| `T_MAIN` | `4096/48000` | window duration MAIN |
| `T_LF` | `8192/48000` | window duration LF |
| `T_HOP` | `1024/48000` | frame match radius only (§2.3) |
| `T_MEM` | `16/11025` s | `max` lock `resampler_group_delay_seconds` over gate rates (§2.4) |
| `MAIN_LOBE_NULL_TO_NULL_BINS` | `4` | periodic Hann main lobe |
| `W_LOBE_MAIN_HZ` | `2 · 48000/4096 = 23.4375` Hz | half null-to-null; **non-dilating** documentation constant |
| `T_ANAL` split | `320` Hz | §6.2 MAIN-pure knee |
| `t_start`, `t_end` | `1/2`, `7/4` | fixture_spec active interval |
| `f_start`, `f_end` | `20`, `20000` | fixture_spec |
| `f_inst` | `f(source_time*)` at **48 kHz** selected frame | analytic law; **not** PSD-argmax |
| `t_win_*` | §6.2 PSD window source-time image − `T_MEM` on lo | §4.1 |
| `N` membership | triangular support ∩ `F_TRAJ` **only** | §4.0–§4.2; **≠** peak-local ±K |
| `A3_PORTION_ID` | `checkpoint_grid` | §1.1 |
| floor cut | `> -120` on stored float32 | §6.1 / REV6 union |
| `AT_FLOOR` | stored float32 `== -120` | §3.1 |
| `ONE_SIDED_FLOOR_UNION` | union ∧ either side `AT_FLOOR` | skirt class |
| F1 | N restricts exemption only | closing domain ≠ only N |
| closing domain | `{ACTIVE_sweep}` alone | **≠** `ACTIVE∩R` |
| `level_dbfs` | valid on both paired frames | **not** N / ONE_SIDED exempt (§3.1 / §3.3) |
| tie-break | `smaller_frame_index`, then `smaller_frame_end_sample` | lock `alignment.tie_break` @ `d2c35ccc…` |
| `CHK_CHIRP_REACHABLE` | `t_start ≤ source_time* ≤ t_end` | §2.3.2; else chk FAIL (empty-N pin) |
| `b106_in_N_at_16k` | `true` under current lock | §4.2 / §6.2 / §8.2; no private-hash escape |
| threshold | `0.25` dB | immutable |

**Explicit non-inputs:** any function of measured `max|Δ|`, band-failure lists,
or the numeric pair `(6.435, 5.838)`.

---

## 5. Vacuous FAIL / empty-`N` FAIL (fail-closed)

**Normative (three fail-closed cases — no shopping):**

1. **Chirp-unreachable (§2.3.2):** if `¬CHK_CHIRP_REACHABLE(chk)`
   (`source_time* ∉ [t_start, t_end]`), gate 4 is **FAIL** for that
   checkpoint. Applies in particular to mandatory chk **20 Hz** under
   nearest REF-48k when `source_time* < t_start`.
2. **Inverted window / empty `N` (§4.1):** if `CHK_CHIRP_REACHABLE` holds
   but `t_win_lo > t_win_hi` (or `N(chk) = ∅` after §4), gate 4 is
   **FAIL** for that checkpoint. Empty `N` **≠** blanket
   `ONE_SIDED_FLOOR_UNION` exemption and **≠** PASS.
3. **Empty ACTIVE set:** for every `chk ∈ SWEEP_CHECKPOINT_HZ` and every
   channel `ch` that is valid on the paired checkpoint frames
   (`mid_valid` / `side_valid` as applicable; `log_sweep` is mono → `mid`
   only), if `{ b | ACTIVE_sweep(b, ch, chk) } = ∅`, then gate 4 is
   **FAIL** for that checkpoint × channel. Empty active set ≠ PASS.

No N/A skip, soft-pass, “no cells to compare,” `t_cross` proxy, or clamp
of `source_time*` into the chirp interval. Case 3 also applies when
`N(chk)` is non-empty but every in-`N` cell is at floor on both sides and
every out-`N` one-sided cell has been exempted away.

---

## 6. Mandatory report (anti-laundering)

### 6.1 A3 inactivation table (required)

Any numeric report that claims to evaluate gate 4 under this formula **MUST**
publish (machine-readable OK) every cell such that:

```text
ABOVE_FLOOR_UNION(b, ch, chk) = true
∧ ACTIVE_sweep(b, ch, chk) = false
```

Minimum columns:

| column | content |
|--------|---------|
| `asset` | `log_sweep` |
| `portion` | `checkpoint_grid` (= `A3_PORTION_ID`) |
| `chk_hz` | checkpoint frequency |
| `source_time_star` | selected **48 kHz** frame `source_time*` |
| `frame_end_sample_star` | selected **48 kHz** `frame_end_sample*` |
| `dt_to_t_cross` | `source_time* − t_cross` |
| `f_inst` | analytic `f(source_time*)` |
| `N_sorted` | sorted band-index list of `N(chk)` |
| `band` | `b` |
| `channel` | `mid` / `side` |
| `psd_db_ref`, `psd_db_sr` | stored post-clamp **float32** values |
| `reason` | enum below |

**Reason enum (closed):**

| `reason` | meaning |
|----------|---------|
| `A3_ONE_SIDED_OUT_OF_N` | `ONE_SIDED_FLOOR_UNION` and `b ∉ N(chk)` |

No other A3 inactivation reason exists in this formula. Omitting the table
when any such cell exists → report **FAIL**.

### 6.2 Neighbourhood geometry report (required even when no inactivation)

Independently of §6.1, every gate-4 report under this formula **MUST** publish
per checkpoint:

| field | content |
|-------|---------|
| `chk_hz` | checkpoint frequency |
| `t_cross` | `sweep_crossing_time(f_chk)` (selection target only) |
| `source_time_star` | 48 kHz `source_time*` |
| `frame_end_sample_star` | 48 kHz `frame_end_sample*` |
| `dt_to_t_cross` | `source_time* − t_cross` |
| `f_inst` | analytic `f(source_time*)` |
| `t_win_lo`, `t_win_hi` | §4.1 interval |
| `T_MEM` | lock value used |
| `N_sorted` | ascending list of band indices in `N(chk)` |
| `b106_in_N_at_16k` | required when `chk_hz = 16000`: boolean; **MUST** be `true` under lock digest `d2c35ccc…` + §2.3/§4 / §8.2; `false` or omitted → geometric **FAIL**; alternate lattice only after Guardian lock amend (private / unamended hashes → FAIL) |

Omitting these fields → report **FAIL**.

### 6.3 Anti-laundering vs REV7 LF `EXCLUDED_GEOMETRY` / geometric `R`

| rule | |
|------|--|
| Forbidden | Tagging an A3-inactivated HF skirt cell as `EXCLUDED_GEOMETRY`, `i ∉ R`, or report-only LF solely to remove it from the gate-closing max |
| Forbidden | Moving band **b106-class** HF checkpoint cells into the REV7 LF report-only vehicle because they fail under REV6 union |
| Forbidden | Closing max on `ACTIVE_sweep ∩ R` (or any silent `R` / `i∉R` filter) for `log_sweep` / `checkpoint_grid` before Guardian consolidate GO — **report FAIL**, not a soft warning |
| Required | A3 inactivations use reason `A3_ONE_SIDED_OUT_OF_N` only; LF geometry exclusions (if/when consolidated) remain a **separate** table with `EXCLUDED_GEOMETRY` |
| Required | Gate-4 PASS/FAIL prose must state that sweep checkpoint activity is `ACTIVE_sweep` (F1), closing domain = `{ACTIVE_sweep}`, neighbourhood = `F_TRAJ` only — not “domain = N”, not “∉ R”, not “peak ±K” |

---

## 7. Paper acceptance tests (redteam / CC must-fix surface)

These are **paper** tests on the formula (no re-measure in this tranche):

| # | Construction | Required outcome |
|---|--------------|------------------|
| T1 | `psd_ref = -120`, `psd_sr = -100` (stored float32), band `b ∈ N(chk)` with `f_inst ∉ (lo(b), hi(b))` (one-sided **in-`F_TRAJ`** but not peak-local) | `ONE_SIDED_FLOOR_UNION` ∧ `ACTIVE_sweep = true` |
| T2 | Same one-sided floor pattern on a band `b ∉ N(chk)` (skirt outside MAIN/LF window chirp image + `T_MEM`) | `ACTIVE_sweep = false`; appears in §6.1 table with `A3_ONE_SIDED_OUT_OF_N` |
| T3 | Both sides `> -120` on a band outside `N(chk)` | `ACTIVE_sweep = true` (F1: domain ≠ only N) |
| T4 | Claim that b106-class HF FAIL is report-only via `R` / `EXCLUDED_GEOMETRY` without A3 | **REJECT** / laundering (§6.3) |
| T5 | Empty `ACTIVE_sweep` set on any required checkpoint × valid channel scored as PASS | **REJECT** (vacuous) |
| T6 | Neighbourhood centre taken from PSD-argmax | **REJECT** (§2.3) |
| T7 | Stationary multitone / pseudo_noise closing predicate altered by this note | **REJECT** (§1) |
| T8 | `N` width chosen by targeting 6.435 / 5.838 or by post-hoc dropping 16 kHz | **REJECT** (contamination) |
| T9 | `N` / `F_TRAJ` built from `t_cross` proxy or any non-minimizer in-hop frame | **REJECT** (§2.3 / §8) |
| T10 | Distinct `N` per host rate after cross-SR pairing | **REJECT** (§2.3: single 48 kHz `N`) |
| T11 | `t_win_*` not equal to the §6.2 PSD window source-time image (plus `T_MEM` on lo); **or** selected frame not on eligible useful-segment lattice (`N_LF + m·H` ∩ §13.1 useful window) | **REJECT** (§2.3 / §4.1) |
| T12 | Redefine `N` as peak-local ±K (or peak-containment only) such that `f_inst`’s band stays in `N` but `b106 ∉ N(16000)` | **REJECT** (§4.0) |
| T13 | Closing max := `ACTIVE_sweep ∩ R` (or apply `i∉R` / `EXCLUDED_GEOMETRY` to sweep closing cells) | **REJECT** / report FAIL (§3.3 / §6.3) |
| T14 | `AT_FLOOR` via float64 recompute or “near −120” tolerance | **REJECT** (§3.1) |
| T15 | Host↔ref pairing other than per-rate nearest-to-`t_cross` on eligible lattices | **REJECT** (§2.3.1) |
| T16 | `portion ≠ checkpoint_grid` while claiming A3 / or aliasing portion to dodge A3 | **REJECT** (§1.1) |
| T17 | Formula/report self-declares `CONTRACT-SOUND` without independent redteam/CC | **REJECT** (§0 / §10) |
| T18 | Nearest REF-48k `source_time* ∉ [t_start,t_end]` (e.g. chk 20 Hz with `st* ≈ 0.4907 < 0.5`) scored as skip / N/A / soft-pass; **or** clamp `st*` into chirp; **or** `t_cross` proxy; **or** treat empty `N` as blanket `ONE_SIDED` exemption / 6.44 repair | **REJECT** / checkpoint FAIL (§2.3.2 / §5) |
| T19 | Publish `b106 ∉ N(16000)` / `b106_in_N_at_16k = false` under lock digest `d2c35ccc…`, or attach a private / unamended “different hashed lattice” escape | **REJECT** / geometric FAIL (§4.2 / §6.2) |
| T20 | Exempt `level_dbfs` via `N` / `ONE_SIDED_FLOOR_UNION`, or include it when channel invalid on either paired frame | **REJECT** (§3.1 / §3.3) |
| T21 | Tie-break order `smaller_frame_end_sample` before `smaller_frame_index` (inverted vs lock `alignment.tie_break`) | **REJECT** (§2.3 / §2.3.1) |

---

## 8. Worked geometry check (illustrative — normative path only)

### 8.1 Quarantine: `t_cross` proxy is non-normative

A prior draft illustrated 16 kHz with
`source_time* := t_cross(16000)`, yielding
`F_TRAJ ≈ [9984.35, 16000]` and `N = {107,…,116}` with `b106 ∉ N`.

That construction is **quarantined**. It **MUST NOT** be cited as
membership evidence. Normative `N` uses only the §2.3 nearest-frame
`source_time*` after `|dt|` minimization (tie: smaller `frame_index`,
then smaller `frame_end_sample` — lock `alignment.tie_break`) on the
reference 48 kHz **eligible** lattice.

### 8.2 Honest nearest-frame identity at 16 kHz (REF-48k)

Locked hop lattice at 48 kHz (`frame_end_sample = N_LF + m·H` for useful
frames; identity `gd = 0` ⇒ `source_time = frame_end_sample / fs_c`).
Eligibility requires `source_time` in the §13.1 common useful window
(additive warm-up / coda). Binary64 / fixture_spec law; `T_MEM = 16/11025`;
MAIN-pure aperture:

```text
f_chk              = 16000
t_cross            ≈ 1.7096208279 s
source_time*       = 81920 / 48000 = 1.706666… s
frame_end_sample*  = 81920   # = N_LF + 72·H; eligible (post warm-up)
dt                 = source_time* − t_cross ≈ −0.00295416 s
                   (|dt| ≈ 2.95 ms < T_HOP; next in-hop frame at 1.728 s
                    has larger |dt| and is NOT selected)

f_inst             = f(source_time*) ≈ 15740.92 Hz
T_ANAL             = T_MAIN = 4096/48000
t_win_lo           = source_time* − T_MAIN − T_MEM ≈ 1.619882 s
t_win_hi           = source_time* ≈ 1.706667 s
F_TRAJ             ≈ [9744.22, 15740.92] Hz
N(chk)             = {106, 107, 108, 109, 110, 111, 112, 113, 114, 115}
```

**Honest membership:** `b106 ∈ N(16000)` under the normative nearest-frame
path. Band `106` support `(center[105], center[107]) ≈ (8873.4, 9965.7)`
intersects `F_TRAJ` because `hi(106) ≈ 9965.7 > 9744.22`. Therefore a
`ONE_SIDED_FLOOR_UNION` cell at b106 on this checkpoint remains
`ACTIVE_sweep = true` (F1). A3 does **not** paper-exempt the known smoking-gun
skirt at this checkpoint under nearest-frame geometry.

**Contrast (forbidden peak-local):** ±K about the band containing
`f_inst ≈ 15741` keeps ~b114–b115 and **excludes** b106 — that pad is
exactly the porosity §4.0 / T12 forbid.

(The quarantined `t_cross` proxy had `Flo ≈ 9984.35 > hi(106)`, which falsely
excluded b106 — that is exactly the porosity §8.1 forbids.)

This is a consequence of §6 + chirp law + MAIN window + lock `T_MEM` + hop
lattice selection — not a fit to the FAIL magnitude.

---

## 9. Lock / CONTRACT binding (future amend only — not this tranche)

**This note does not edit** freeze CONTRACT, `metrology_lock`, or SHA256SUMS.

If a later Guardian GO amends, expected new hashed keys (sketch only):

| key | intent |
|-----|--------|
| `sample_rate_parity.activity.sweep_predicate` | string form of `ACTIVE_sweep` |
| `sample_rate_parity.activity.sweep_f1_exemption` | `ONE_SIDED_FLOOR_UNION` definition |
| `sample_rate_parity.activity.at_floor` | `float32_stored_psd_db == -120` |
| `sample_rate_parity.sweep.portion_id` | `checkpoint_grid` |
| `sample_rate_parity.sweep_neighbourhood.rule` | support ∩ `F_TRAJ` (**forbid** peak-local ±K) |
| `sample_rate_parity.sweep_neighbourhood.T_anal_split_hz` | `320` |
| `sample_rate_parity.sweep_neighbourhood.f_inst` | `analytic_chirp_at_source_time_star_48k` |
| `sample_rate_parity.sweep_neighbourhood.ref_stream` | `48000` |
| `sample_rate_parity.sweep_neighbourhood.T_mem` | `max_gate_resampler_group_delay_seconds` |
| `sample_rate_parity.sweep_neighbourhood.t_win` | `§6.2_psd_window_source_time_image` |
| `sample_rate_parity.sweep_pairing` | per-rate nearest-to-`t_cross` on eligible lattice |
| `sample_rate_parity.sweep_closing_domain` | `ACTIVE_sweep_only` (no ∩`R` until consolidate GO) |
| keep | `threshold_max_abs_db = 0.25`, aggregator `max`, `SWEEP_CHECKPOINT_HZ`, reachability, warm-up additive |

Silent lock edit without CONTRACT amend GO → BLOCKER.

---

## 10. Next permitted action

| step | status |
|------|--------|
| Parent decisions A / A3 / fuori REV7 LF | stamped |
| Tip packaging for first independent CC | `81e86dc5` |
| Independent CC @ `81e86dc5` | **CONTRACT-POROUS** (must-fixes 1–7) |
| Third redteam | **CONTRACT-POROUS** (HIGH#1 / MED#2; LOW#3 pinned) |
| CC delta (empty-N @ 20 Hz) | **CONTRACT-POROUS** → closed by §2.3.2 / T18 (`CHK_CHIRP_REACHABLE`) |
| This concrete formula (POROUS closures in prose) | **ready for redteam/CC delta again** |
| Self-declaration of `CONTRACT-SOUND` | **forbidden** (only independent redteam/CC) |
| Independent redteam/CC under written rule | **authorized** as the next judge step |
| Re-measure on gate platform | **blocked** until non-POROUS acceptance + Marco OK |
| Falsification-only measure | OK **after** non-POROUS acceptance (not ACCEPT remeasure) |
| Frontend implement / tip / REV7 consolidate | **forbidden** as next step |

**Ready for redteam/CC delta again; remeasure ACCEPT still blocked until non-POROUS acceptance. Falsification-only still OK after non-POROUS. This file does not claim CONTRACT-SOUND.**

---

## 11. References

1. `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_PROPOSAL.md` — A / A3 stamp
2. `docs/MOTORE_V3_G1_CONTRACT.md` @ `6d254d0a` — §6, §13.1–13.2
3. `ml_v3/contracts/fixture_spec.py` — `sweep_crossing_time`, useful lattice, active interval
4. `ml_v3/contracts/metrology_lock.py` — `resampler_group_delay_rational` / `T_MEM` / alignment
5. `docs/MOTORE_V3_REV7_REPORT_ONLY_LF_CLAUSE.md` — `EXCLUDED_GEOMETRY` (anti-launder foil)
6. `docs/MOTORE_V3_REV7_SCOPE_REWRITE_PROPOSAL.md` — Hann null-to-null pin
7. `docs/MOTORE_V3_REV7_REMEASURE_R_FINAL.md` — trigger evidence only (not a fit target)

---

≠ G1 PASS. ≠ REV7 consolidate. ≠ G1b tip. ≠ re-measure GO. ≠ lock mutate.
≠ self-CONTRACT-SOUND.
```

---

## B.15 FILE: `docs/MOTORE_V3_LOG_SWEEP_A3_FALSIFICATION_MEASURE.md (FAIL honest @ 78da84dd)`

**Path logico:** `docs/MOTORE_V3_LOG_SWEEP_A3_FALSIFICATION_MEASURE.md`  
**Bytes:** 4228  
**Lines:** 95

```markdown
# Motore v3 — A3 falsification measure (`ACTIVE_sweep` only)

| Field | Value |
|-------|--------|
| **Status** | **EVIDENCE — FALSIFICATION ONLY** |
| **Date** | 2026-07-26 |
| **Formula HEAD** | `04e47b39610e8fda24b36a57eb8283388565b62b` (`feature/motore-v3-offline`) |
| **Formula doc** | `docs/MOTORE_V3_LOG_SWEEP_HF_ADMISSION_FORMULA.md` @ that HEAD |
| **Closing domain** | `{ACTIVE_sweep}` alone — **not** `ACTIVE∩R` / `i∉R` / `EXCLUDED_GEOMETRY` |
| **Threshold** | `0.25` dB hard; aggregator `max` (immutable; no mean/p95) |
| **Platform** | CPython 3.12.13 / numpy 2.5.1 / macOS-15.5-arm64 (`~/aieq_data/motore_v3/env/venv`) |
| **Runner** | spike `motore-v3-g1b-spike` / `ml_v3/reports/run_a3_falsification_measure.py` |
| **JSON** | spike `ml_v3/reports/G1B_A3_FALSIFICATION_MEASURE.json` |

**≠** G1 PASS · **≠** REV7 consolidate · **≠** ACCEPT product path · **≠** PASS-claim

---

## Command

```bash
cd /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1b-spike
PYTHONPATH=$PWD ~/aieq_data/motore_v3/env/venv/bin/python \
  ml_v3/reports/run_a3_falsification_measure.py
```

Formula applied as written at HEAD `04e47b39…`:

- neighbourhood `N(chk) :=` triangular support ∩ `F_TRAJ` (analytic chirp image + `T_MEM`)
- `ACTIVE_sweep ⇔ ABOVE_FLOOR_UNION ∧ (¬ONE_SIDED_FLOOR_UNION ∨ b∈N)` with float32 `AT_FLOOR`
- `CHK_CHIRP_REACHABLE ⇔ t_start ≤ source_time* ≤ t_end` (else checkpoint FAIL)
- lock tie-break: smaller `frame_index`, then smaller `frame_end_sample`
- **no** intersection with geometric `R`; **no** threshold shopping

---

## Gate-closing domain (`log_sweep` / `checkpoint_grid`)

| chk Hz | CHK_CHIRP_REACHABLE | \|N\| | b106∈N | max\|Δ\| dB (worst host) | peak | verdict |
|--------|---------------------|------|--------|--------------------------|------|---------|
| 20 | **false** (`st*≈0.490667 < 0.5`) | — | — | — | — | **FAIL** |
| 45 | true | 15 | no | 1.4048 | shape b23 @44.1k | **FAIL** |
| 60 | true | 18 | no | 1.4056 | shape b33 @44.1k | **FAIL** |
| 80 | true | 18 | no | 0.7405 | psd b35 @44.1k | **FAIL** |
| 250 | true | 18 | no | 1.7522 | shape b51 @44.1k | **FAIL** |
| 1000 | true | 11 | no | 2.5751 | shape b69 @44.1k | **FAIL** |
| 3500 | true | 10 | no | 4.6642 | shape b89 @44.1k | **FAIL** |
| 8000 | true | 10 | no | 5.3797 | shape b103 @44.1k | **FAIL** |
| **16000** | true | **10** | **yes** | **6.4350** | **psd b106 (~9404 Hz) @44.1k** | **FAIL** |
| 20000 | true | 10 | no | 4.1235 | shape b119 @44.1k | **FAIL** |

### Smoking-gun cell (16 kHz) — still ACTIVE

| vs 48k | max\|Δ\| dB | peak | `b106∈N` | `n_active` |
|--------|------------|------|----------|------------|
| 44100 | **6.4350** | psd b106 (~9403.7 Hz) | **true** | 10 |
| 96000 | **5.8381** | psd b106 (~9403.7 Hz) | **true** | 10 |

`N(16000) = {106…115}` under REF-48k `source_time*≈1.706667`,
`F_TRAJ≈[9744.2, 15740.9]` Hz. Honesty pin MED#2 satisfied:
`b106_in_N_at_16k = true` (no private-lattice escape).

### Empty-N / 20 Hz pin (HIGH#2)

Nearest REF-48k useful frame for chk **20 Hz**: `source_time*≈0.490667`
with `|dt|≤T_HOP`, but `st* < t_start=0.5` →
`¬CHK_CHIRP_REACHABLE` → checkpoint **FAIL** (no skip, no `t_cross` proxy,
no clamp into chirp).

---

## Verdict (gate-closing domain only)

**FAIL**

- Overall `max|Δ| = 6.4350` dB (threshold 0.25) at chk 16 kHz / vs44100 / psd b106
- Independently: chk 20 Hz unreachable under §2.3.2
- `b106` **stayed ACTIVE** (in `N` ∧ one-sided floor-union)

### Explicit non-claims

- **No PASS-claim** for G1 / gate-4 product path
- **No ACCEPT** for product / tip packaging
- **No REV7 consolidate**
- This note falsifies “A3 formula closes sweep HF under current frontend PSD”
  for the declared closing domain; it does **not** authorize threshold relax,
  `ACTIVE∩R` shopping, or shrinking `N`

---

## Next (for Marco)

1. Treat this as honest **ACCEPT=FAIL** evidence on the written A3 domain (optional commit of this note alone).
2. Do **not** consolidate REV7 / claim G1 PASS.
3. Lab choice remains a-priori: amend admission further, change frontend/PSD path under contract, or keep debt explicit — not mean/p95 / 0.25 relax / ∩R laundering.
```

---

## B.16 FILE: `ml_v3/reports/G0_FREEZE_REPORT.md`

**Path logico:** `ml_v3/reports/G0_FREEZE_REPORT.md`  
**Bytes:** 2848  
**Lines:** 69

```markdown
# Motore v3 G0 freeze report

Date: 2026-07-19

Verdict: **PASS**. This reproduces the frozen negative baseline; it does not
promote a model or authorize G1 implementation without counter-check.

## Isolation

- Branch: `feature/motore-v3-offline`, created from
  `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- Runtime tag: `checkpoint/motore-v2-runtime-5c9cb329-2026-07-19`, annotated,
  peeled commit `5c9cb3290f87b62a339c6b2c49645b2b25524712`.
- Scientific tag: `checkpoint/motore-v2-a4b-final-88e70dd0-2026-07-19`,
  annotated, peeled commit `88e70dd03679adfeb388976c702ad8f0a73b2bd3`.
- The dirty main worktree, EXP/product branches, installed plugins and all
  existing `Source/` files were left unchanged.

## Environment

- CPython 3.12.13 on macOS 15.5 arm64.
- uv 0.11.2; Torch 2.13.0; NumPy 2.5.1.
- Fresh venv: `~/aieq_data/motore_v3/env/venv`.
- `uv pip sync --require-hashes`: PASS, 11 packages checked.
- `requirements.in` SHA-256:
  `5110ddf0fdcba291e41aaaa04da6768a9b2b3ee559ce37fa7c4cc072cd256f60`.
- `requirements.lock` SHA-256:
  `7b12505922a96ee8b6f227a3b86d04f4600b8734503ca2308e854c046288adf9`.

## Data and contract preflight

- Deep audio rehash: train 3062 PASS, heldout 493 PASS, test 835 PASS.
- A4b committed model/provenance `SHA256SUMS`: 6/6 PASS.
- A4b guard module: 37/37 PASS.
- The frozen training report records 834 test rows. Commit `c80bd386` later
  admitted `fsld:46593` as a licensed, group-disjoint, test-only clean-synth
  fixture. It does not change training or model weights and explains the
  current 835-row test manifest.

## Behavioral replay

Current reference:
`~/aieq_data/baselines/a4b_control_grid_20260718_85fa55ec`.

- Reference `SHA256SUMS` SHA-256:
  `6916dda9bfdaa6608293affe51ca3bf2d463125d421d4011a4d79043597a38c4`.
- Every grid artifact listed by that file passed rehash.
- Dataset-seed-42 model JSONs for seeds 42, 1337 and 2026 are byte-identical
  to the three committed historical model JSONs.
- Each benchmark was rerun against the 835-row checkpoint. After normalizing
  only the `model=` path, all three outputs matched `eval_s*_d42.txt` exactly.

| Model seed | Replay | A6 failures |
|---:|---|---|
| 42 | exact | 02, 04, clean_drums, clean_synth, clean_bass |
| 1337 | exact | 02, 04, 08, clean_synth |
| 2026 | exact | 02, 04, 06, 07, 08, clean_synth |

Normalized replay SHA-256 values:

- seed 42: `a4c2a6debeffa9765431446334f4ffd9f967b5789e8ff72789fa233c8f384842`;
- seed 1337: `78bd01b7470920896f66296f77d2958403b9a19ff00257353f1f921bec95f9f3`;
- seed 2026: `a1a0b2eb397c359e336dedba71fcb7accc1bb18895dd1c52f731d1a74a6b7121`.

## Conclusion

G0 is reproducible and isolated. Motore v2 remains **NO-GO 0/3** for these
historical dataset-seed-42 candidates. The next permitted activity is the G1
contract and benchmark design; no training or runtime integration is enabled.
```

---

## B.17 FILE: `ml_v3/reports/G1B_SPIKE_PLAN.md (tracked; PLAN ONLY ≠ tip)`

**Path logico:** `ml_v3/reports/G1B_SPIKE_PLAN.md`  
**Bytes:** 29747  
**Lines:** 535

```markdown
# G1b Frontend Spike Plan — REV6 feasibility (0.25 dB + streaming≡offline)

**Status:** PLAN ONLY — not G1b tip, not gate proof, not G1 PASS  
**Date:** 2026-07-25 (P7 ACCEPTED — Marco OK; P1–P7 pinned; WS0 worktree create unblocked)  
**Contract:** `docs/MOTORE_V3_G1_CONTRACT.md` @ freeze `6d254d0a` (REV6)  
**Lab state:** `docs/MOTORE_V3_PLAN.md` — G1a CLOSE: GO @ tip `a2186ac1`; product G1b may unfreeze; spike WS4 **RED** → REV7 **candidate draft** `docs/MOTORE_V3_G1_CONTRACT_REV7_CANDIDATE.md` (admission only; 0.25 immutable); **REV7 consolidate: NO** until formula + redteam + second GO  
**Authority for this doc:** planning spike under Marco mandate; does not amend CONTRACT  
**Pin readiness:** **P1–P7 ACCEPTED** (P7 = Marco OK 2026-07-25 on restricted spike-only `{silence, level_below_threshold}`); **worktree create unblocked**

```text
PHASE:            G1b-SPIKE-PLAN (planning + inventory; not product G1b)
AUTHORIZED_BY:    PLAN G1a CLOSE stamp + G1b may-unfreeze / mandato storico
                  PLAN tip lineage (incl. 37f6ac60 mandato); Marco: "proceed
                  with planning the G1b spike" + refinements mandate
ALLOWED_PATHS:    ml_v3/reports/G1B_SPIKE_PLAN.md (this file)
                  (inventory read-only: ml_v3/frontend/**, test_g1b_*,
                   contracts/, fixtures/g1/, MOTORE_V3_*)
FORBIDDEN_PATHS:  Source/, CMakeLists.txt, Resources/, ml_v2/, AIEQ-mac/,
                  CONTRACT amend, training, ship weights, Ableton install,
                  hashed artifact mutation (lock / SHA256SUMS / fixture-spec)
```

---

## 1. Goal / non-goals

### Goal (the only question that matters before freezing more)

Under **REV6 unchanged**, answer with evidence on the gate platform:

> Are **(a)** sample-rate parity `max|Δ| ≤ 0.25 dB` (44.1 / 48 / 96) and
> **(b)** offline ≡ streaming **bit-identity** on required float32 frame fields
> reachable by a faithful §5/§6/§7 implementation?

Verdict taxonomy (spike overall uses the pair below; see §4):

| Verdict | Meaning |
|---------|---------|
| **GREEN** | Gates reachable under REV6 with pinned P1–P7 and margin |
| **AMBRA** | Gates appear to pass, but only via unpinned P* choice and/or without margin → freeze-from-prose preregistration debt (≠ REV7) |
| **RED** | Falsifiable impossibility under REV6 → REV7 mandate candidate |
| **INCONCLUSIVE** | Incomplete surface / wrong platform / harness bug; no CONTRACT amend |

### Non-goals (hard)

| Non-goal | Why |
|----------|-----|
| G1 PASS / G1e close | Spike ≠ full gate suite; G1e is later |
| Ableton / ship-line | `Source/`, CMake, Resources, AIEQ-mac stay 0-diff |
| Training / thresholds / routing | Not authorized |
| CONTRACT amend (REV7) | Only after RED with falsifiable impossibility |
| Treating current `ml_v3/frontend/` spike as proof | Explicit: **spike ≠ tip ≠ PASS** |
| Closing gain/M/S/anti-alias/split/evaluator gates | Out of spike scope (note only) |
| F1 WAV relocation | Durable debt; use in-repo T6 WAVs as-is |
| Full 7 streaming schedules in spike | Adversarial subset only (§3.1); full set = official G1b |
| Creating spike worktree before P7 pin | Documented in WS0; **unblocked** after Marco OK on **P7** (2026-07-25) |
| Mutating hashed G1a artifacts | lock / SHA256SUMS / fixture-spec / WAVs stay frozen |

---

## 2. Inventory of existing uncommitted spike (not proof)

Present on worktree `motore-v3-offline` (uncommitted; **not** official G1b tip):

| Path | What it is | What it is not |
|------|------------|----------------|
| `ml_v3/frontend/resampler_coeffs.py` | §5 FIR coeff generator; ratios/delays match lock | No streaming polyphase apply; no audio I/O |
| `ml_v3/frontend/feature_frame.py` | §7 schema stub + fail-closed structural validate | No FFT, no features, no emit path |
| `ml_v3/frontend/__init__.py` | Re-exports T1 surface | — |
| `ml_v3/tests/test_g1b_t1_resampler_coeffs.py` | Coeff length/sum/determinism + stub rejects | No gate 3 / gate 4 measurements |

**Explicit:** this spike proves only that frozen §5 *parameterization* can be
materialized as coefficients consistent with the metrology lock. It does
**not** demonstrate 0.25 dB SR-parity or streaming≡offline bit-identity.

---

## 3. Contract / lock anchors the spike must obey (REV6)

### 3.1 Gates under test (spike success = feasibility of these two)

From CONTRACT §13.2 + lock `metrology_lock.json`
(`metrology_lock_sha256` =
`d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e`):

**(a) Sample-rate parity (gate 4)** — spike adversarial subset  
- Compare **44.1 and 96 vs ref 48 kHz** on preregistered useful window (§13.1).  
- **Signals (primary):** `multitone`, `log_sweep`, `pseudo_noise` only.  
- Domain: `mid/side_{psd,shape,prominence}_db[120]` + `mid/side_level_dbfs`
  when channel valid; **exclude** `*_delta_db`.  
- Aggregator: **max** |Δ|; threshold **0.25 dB**; one active cell over → FAIL.  
- Activity: `max(psd_ref, psd_sr) > -120` (union); no post-hoc mask.  
- Align on nearest `source_time` (not raw index); no manual frame shift.  
- Multitone / noise: mode **(a)** entire useful segment (mode b diagnostic only).  
- Sweep: frozen checkpoints only
  `[20,45,60,80,250,1000,3500,8000,16000,20000]` Hz.  
- Transient / damped resonance / onset sub-gate: **out of primary spike scope**
  (risk declared in §10; not used to claim GREEN).

**(b) Streaming ≡ offline (gate 3)** — spike adversarial schedules  
- **Spike schedules only:** `{1, 8193, geometric-32}`  
  (geometric list from `PCG64(20260719)` via lock — do not regenerate U).  
- Full lock set `{1,63,1024,4095,8192,8193}` + geometric **deferred** to
  official product G1b gate (not required to close this spike).  
- On **gate platform**: byte-identical float32 frame fields + rational
  timestamps + validity flags (`gate_platform_float_tol = 0`).  
- Also-required smoke (optional in spike, not GREEN-blocking alone):
  multi-asset concat with delta history reset; interleaved silence.  
- Secondary 1e-6 allowlist: **empty** → cannot close G1 via tol.

### 3.2 Implementation surface required to even ask the question

| Piece | Contract |
|-------|----------|
| Resampler | §5 causal polyphase FIR; state across blocks; no pad/reflect/look-ahead |
| Timing | §13.1 warm-up additive: `delay + N_LF/fs_c + K_wu*H/fs_c`; coda `K_coda*H/fs_c`; `K_wu=K_coda=4` |
| Dual-res | §6.2 MAIN 4096 / LF 8192 / hop 1024 / Hann / same `frame_end_sample` / LF↔MAIN fuse 160–320 Hz |
| Frame | §7 full emit (not stub-only) |
| Inputs | T6 bytes via `render_*` float32 arrays (byte-identical to WAV); no scipy/soundfile loader |

### 3.3 Evidence platform (mandatory)

```text
Interpreter: /Users/marco/aieq_data/motore_v3/env/venv/bin/python
CPython:     3.12.13
Lock pin:    bit_identity.gate_platform
             {os:darwin, arch:arm64, python:CPython 3.12.13, numpy:2.5.1}
Deps:        numpy + stdlib only — no scipy, no soundfile
             do not touch requirements.lock
```

Any GREEN/RED/AMBRA claim recorded on another interpreter → **INCONCLUSIVE**.

---

## 4. Success criteria (spike verdict)

Record in a future evidence note (not this plan) as one of:

### (a) SR-parity 0.25 dB

| Verdict | Rule |
|---------|------|
| **GREEN** | On gate platform, for adversarial subset (multitone, pseudo_noise, log_sweep @ 44.1/48/96), after warm-up∩coda intersection and activity predicate, measured `max|Δ| ≤ 0.25` on the full §13.2 domain **with margin** (see AMBRA); sweep checkpoints all present; P1–P7 pinned *before* coding and not chosen to pass; no threshold relaxation |
| **AMBRA** | Same surface appears to pass, but (i) pass depends on an unpinned or post-hoc P1–P7 choice, **or** (ii) `max|Δ|` has no margin (e.g. 0.24 dB on a gate-closing cell). AMBRA ≠ REV7; produces freeze-from-prose preregistration debt for official G1b |
| **RED** | Faithful §5/§6/§7 path + correct alignment/window still yields `max|Δ| > 0.25` on a gate-closing fixture/domain cell, **and** root cause is contractual (formula/threshold/domain), not a bug — documented with numeric evidence + localization (band, field, asset, SR) |
| **INCONCLUSIVE** | Missing emit path, wrong window, wrong platform, empty active set mishandled, or bug suspected but not isolated |

### (b) Streaming ≡ offline bit-identity

| Verdict | Rule |
|---------|------|
| **GREEN** | For every **spike** schedule (`1`, `8193`, geometric-32), on gate platform, offline monolith vs chunked produce **byte-identical** required float32 fields + identical rational timestamps + validity/reason; P1–P7 pinned a priori |
| **AMBRA** | Bit-identity holds only under an unpinned P* (esp. P5 cast/reduction) or after post-hoc association change |
| **RED** | Same algorithm offline vs streaming diverges on gate platform after state/reset bugs ruled out — i.e. contract forces non-associative float path with no streaming-equivalent formulation |
| **INCONCLUSIVE** | Harness compares wrong fields, resets delta incorrectly, or platform ≠ lock |

**Spike overall:**

- Continue product G1b @ REV6 iff **(a)=GREEN and (b)=GREEN**.  
- **AMBRA** → continue engineering only after writing freeze-from-prose
  preregistration debt (P* pins) into the next G1b tranche docs; **do not**
  treat as GREEN; **do not** open REV7.  
- Propose **REV7** only if **(a)=RED or (b)=RED** with falsifiable impossibility write-up.  
- Any INCONCLUSIVE → stop product freeze; fix spike; no CONTRACT change.

**Rule (P* necessity):** if any P1–P7 choice is *necessary* to pass → verdict
**AMBRA**, never GREEN.

---

## 5. Pin-before-coding (P1–P7) — mandatory before WS1+

Unpinned §6/§7 choices that **silently change measured quantities**.  
**Choices MUST be written here (or amended by Marco) before any product/spike
coding run.** Changing a pin after seeing numbers → at best AMBRA.

**Meta (2026-07-25):** **P1–P7 ACCEPTED** (P7 = Marco OK on restricted
spike-only enum); **worktree create unblocked**.
No CONTRACT amend invented here.

| ID | Status | Ambiguity | Contract cite | Default (conservative) | Needs Marco? |
|----|--------|-----------|---------------|------------------------|--------------|
| **P1** | **ACCEPTED** (Codex) | Band energy weighted-mean denominator: `Σ(w·psd)/Σw` vs `Σ(w·psd)/N_bins` | §6.1 «media pesata lineare della PSD» | **`Σ(w·psd)/Σw`** (true weighted mean). Empty `Σw` → P2 | No |
| **P2** | **ACCEPTED** + evidence obligations (Codex refine) | Empty triangular support on MAIN 4096 (no FFT bin weight) | §6.1 bande triangolari; DC/>20 kHz non contribuiscono; **no** nearest-bin fallback in REV6 | See **P2 detail** below | No (fail-closed; forbid v2 fallback) |
| **P3** | **ACCEPTED** (Codex) | Prominence padding: numpy `reflect` vs `symmetric` | §7 «padding reflect» | **`reflect`** (as written). Not `symmetric`, not `edge`, not wrap | No |
| **P4** | **ACCEPTED** (Codex) | `shape_db` from clamped vs pre-clamp `psd_db` | §6.1 floor→clamp then dB; §7 `shape_db` uses `psd_db` | **Clamped `psd_db`** (the emitted field): `shape = psd_db - 10*log10(sum(10**(psd_db/10)))` on 120 bands | No |
| **P5** | **ACCEPTED** (Codex) | `float64→float32` cast point + reduction association | §7 frame float32; gate3 bit-identity; lock `gate_platform_float_tol=0` | **Accumulate FIR/FFT/band sums in float64; cast each emitted frame float field to float32 once at write.** Same association offline and streaming (left-to-right on frozen index order). No Kahan / blocked reassoc without new pin | No (default is spike baseline) |
| **P6** | **ACCEPTED** (Codex correct; was inconsistent) | Floor / zero values for invalid-channel vectors | §7 floor for invalid channel; PSD clamp `[-120,+12]`; `delta_db` clamp `[-24,+24]`; first-valid / history-reset → zeros | See **P6 detail** below | No |
| **P7** | **ACCEPTED** (Marco OK 2026-07-25) | `reason` when `valid=false` (frame emitted) | §7 «motivo enumerato quando falso» — **enum not listed** on contract surface; stub accepts any non-empty `str` | See **P7 detail** below — restricted spike-only `{silence, level_below_threshold}`; hard errors out of frame `reason` | No (restricted set accepted; ≠ CONTRACT amend) |

### P2 detail (fail-closed default + quantified evidence obligations)

**Keep (fail-closed):**
- **No** silent import of v2 nearest-bin.
- Empty triangular support → contribute **0** to the weighted sum.
- If `Σw==0` for a band after fuse inputs → band energy = linear floor
  `1e-12` before dB/clamp.

**Evidence obligations (verified geometry notes; declare in spike evidence):**
- Analysis always at `fs_c=48k` → band geometry identical across source rates →
  P2 choice largely cancels in cross-rate Δ (reduces “measuring the choice”
  risk for gate 4).
- MAIN 4096: ~14 empty-support bands (indices scattered 0–23, centres
  ~20–76 Hz) + ~21 single-bin bands.
- LF 8192: ~6 empty bands (~20–37.9 Hz) + ~17 single-bin.
- ~6 LF-pure bands below 160 Hz can be structurally dead (always floor /
  inactive) — declare honestly in evidence; does **not** alone break gate.
- **Preregister watch-list:** single-bin bands are primary suspects if
  `max|Δ|` exceeds 0.25 dB — report whether the max lands there.

### P6 detail (corrected; prior −120 on `delta_db` was INVALID)

Prior draft set `*_delta_db = -120` for invalid channel — **INVALID**: violates
§7 `delta_db` clamp `[-24, +24]`.

**NEW default (invalid channel):**
- `*_psd_db` / `*_shape_db` → **-120.0** (PSD/level floor; matches §6.1 clamp lower)
- `*_level_dbfs` → **-120.0**
- `*_delta_db[120]` → **0.0** (natural zero; matches §7 first-valid-frame /
  history-reset zeros)
- `*_prominence_db` → prefer **0.0** (residual centered at 0), not −120 —
  note lightly if prominence also gains a declared numeric range later;
  REV6 surface does not list a separate prominence clamp beyond derivation
  from shape
- `mid_valid` / `side_valid` / `valid` false as §7

### P7 detail (ACCEPTED — Marco OK 2026-07-25)

**Supersedes** the prior provisional 5-value set
`{silence, non_finite_input, unsupported_sr, insufficient_samples,
channel_invalid}`. That set is **withdrawn** for two reasons:

1. **Category error:** it conflates hard-error / no-frame paths
   (`unsupported_sr`, `non_finite_input`, `insufficient_samples` — §4.1 /
   §6.2 fail-closed: frame never emitted) with frame-emitted-but-invalid
   (`valid == false` ⇒ both channels level < −100 dBFS).
2. **Gate-3 risk:** `silence` vs `channel_invalid` were non-disjoint →
   offline vs streaming can pick different strings → false RED on bit-identity
   of `reason`.

**Pinned spike-only enum** (Marco OK 2026-07-25; ≠ REV7 / ≠ CONTRACT amend):

| reason | when (deterministic, disjoint) |
|--------|--------------------------------|
| `silence` | both channels at level floor (−120 dBFS) |
| `level_below_threshold` | both channels in (−120, −100) dBFS (below validity threshold but not at floor) |

**Rules:**
- Hard errors stay **out of** frame `reason`: raise / fail-closed on path;
  never emit a frame with those labels.
- Exactly one string chosen by the rule above; same offline ≡ streaming.
- Spike-only + freeze-from-prose debt for the official G1b enum at tip.

### P* workflow

1. ~~Marco OK on **P7**~~ → **DONE** (2026-07-25): restricted set accepted.  
2. Spike code may only implement pinned cells.  
3. If a pin must change to pass → record AMBRA + debt; do not silently rewrite this table after the run.  
4. AMBRA pins that product G1b will inherit → write into G1b tranche preregistration (still ≠ REV7).  
5. **Worktree create unblocked** (P1–P7 pinned).

---

## 6. Minimal workstreams

Order is dependency order. Spike may stop early on RED / AMBRA-necessity.

### WS0 — Hygiene / isolation / freeze baseline (**P7 OK → execute**)

**Close isolation ambiguity (do not use `feature/motore-v3-offline` tip for spike commits):**

```text
Worktree path:  /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1b-spike
Branch:         spike/motore-v3-g1b-frontend
Base commit:    a2186ac1   # G1a remediation tip (code); PLAN wins over later docs tips
Seed:           COPY (not merge) uncommitted trees from motore-v3-offline:
                  - ml_v3/frontend/
                  - ml_v3/tests/test_g1b_t1_resampler_coeffs.py
Policy:         NO commits on feature/motore-v3-offline from the spike
                NO CONTRACT / lock / SHA256SUMS / fixture-spec mutation
                Seed left UNCOMMITTED until WS1 / tranche 1 unless otherwise asked
Pins:           P1–P7 ACCEPTED (spike-only; ≠ G1 PASS; ≠ official G1b tip)
Gate Python:    /Users/marco/aieq_data/motore_v3/env/venv = CPython 3.12.13
```

Setup steps (**authorized after Marco OK on P7 2026-07-25**):

```bash
# From main repo / worktree parent — post-approval
git worktree add -b spike/motore-v3-g1b-frontend \
  /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/motore-v3-g1b-spike a2186ac1
# Then copy uncommitted frontend + T1 test from motore-v3-offline into the
# new worktree working tree (cp -R); do not merge offline tip.
# Prefer leave seed uncommitted until WS1 (tranche 1 lands seed as tip on spike).
```

Also in WS0:

- Re-verify lock digest + SHA256SUMS (48) on gate venv (F4 pattern).  
- Do **not** treat uncommitted frontend as tip.  
- Reuse G1a artifacts; **zero** CONTRACT edits.  
- Evidence only on `/Users/marco/aieq_data/motore_v3/env/venv` (3.12.13).  
- Short WS0 note in spike worktree documenting P1–P7 pin freeze.

### WS1 — Resampler coeffs (T1) → streaming apply (T1b)

**Reuse spike:** `fir_lowpass_coefficients`, `resample_ratio`,
`group_delay_rational` (already lock-aligned for gate rates).

**Add (product/spike code, post-approval, on spike branch only):**

1. Causal polyphase apply §5: phase-zero,
   `y[j] = sum_n x[n] * h[j*down - n*up]` with out-of-range h = 0.  
2. Streaming state: history of input samples across blocks; emit
   `0 <= j < ceil(N_block_cumulative * up/down)` without trailing filter flush.  
3. Identity path 48 kHz: delay 0, passthrough.  
4. Unit tests: offline block vs one-shot byte-identical under **P5** cast policy.  
5. Fail-closed: reject non-accepted SR, NaN/Inf input.

**Stop early:** if streaming vs offline **resampler output** alone is not
bit-identical on gate platform → investigate float reduction order (P5) before
building FFT stack (likely fixable; not yet REV7).

### WS2 — Feature path offline (canonical)

Implement §6+§7 offline only, under pinned P1–P7:

1. Mid/side from mono/stereo (§4.1).  
2. Resample → 48 kHz.  
3. Dual-res STFT: Hann periodic, right-aligned, hop 1024, first frame at
   8192 samples.  
4. PSD formula §6.1; band energy (**P1/P2**); dB after mean+fuse; floor/clamp.  
5. LF/MAIN fusion 160–320 Hz raised-cosine on log2.  
6. shape (**P4**) / prominence σ=4, j=-16..16 (**P3** reflect) / delta / level /
   valid / reason (**P6/P7**).  
7. Emit `V3FeatureFrame` with rational `source_time`.

**Tests:** determinism two-process; structural §7; silence / NaN reject;
LF and MAIN share `frame_end_sample`.

### WS3 — Streaming schedule from lock (adversarial subset)

Wrap WS1+WS2 with chunk feeder:

- Schedules: **`1`, `8193`, geometric-32** from
  `frozen_metrology_lock()["streaming_equivalence"]` (do not regenerate U).  
- Full 7-schedule matrix deferred to official G1b.  
- Compare full frame sequences offline vs streaming.  
- Multi-asset: explicit `delta_db` history reset at asset boundary
  (smoke; not sole GREEN criterion).

### WS4 — Eval harness (SR-parity) on adversarial assets

1. **Do not write a WAV loader.** Use `render_*` from
   `ml_v3/fixtures/g1/render_signals.py` (return float32 arrays
   byte-identical to committed WAVs). Optionally cross-check SHA256SUMS
   integrity of on-disk WAVs separately; not required for frame path I/O.  
2. Cache expensive `render_pseudo_noise` (seed fixed in fixture-spec).  
3. Useful window / frame grid / sweep crossing via `fixture_spec.py`:
   `common_useful_window`, `_useful_frame_times` (or public nearest helpers),
   `sweep_crossing_time`, `assert_sweep_checkpoints_reachable`;
   warm-up/coda via `metrology_lock.warm_up_seconds` / `coda_seconds`.  
4. Cross-SR: intersection of useful segments; nearest-`source_time` align.  
5. Activity predicate; max|Δ| over domain; report per-field / per-band peaks
   **and margin to 0.25**.  
6. Sweep: checkpoint table from lock; missing checkpoint → FAIL.  
7. Primary assets: `multitone`, `pseudo_noise`, `log_sweep` @ 44100/48000/96000.  
8. Optional diagnostic only: decorrelated / mid_only / side_only; transient
   onset **out of primary scope** (risk §10).

**Output:** `ml_v3/reports/G1B_SPIKE_EVIDENCE.md` (future; not claimed here)
with tables of max|Δ|, margin, bit-identity PASS/FAIL, and
GREEN/AMBRA/RED/INCONCLUSIVE — still **≠ G1 PASS**.

---

## 7. What to reuse from frozen G1a (do not reinvent)

| Artifact | Tip / digest | Spike use |
|----------|--------------|-----------|
| Contract freeze | `6d254d0a` | Thresholds, formulas, stop rules |
| Metrology lock | T4 `3bfd8aaf`; sha `d2c35ccc…` | Warm-up, schedules, domain, bit-identity platform, delays |
| Fixture-spec v1 | M2 `e9916319`; sha `513c3baf…` | Asset IDs, SR matrix, generators binding; **helpers:** `common_useful_window`, `sweep_crossing_time`, useful frame grid / nearest distance, checkpoint reachability |
| WAV inventory + generators | T6 `501a4e00` | Gate audio bytes; **`render_*` → float32** (byte-identical to WAV) — **no WAV loader in spike** |
| SHA256SUMS | chain from T5 `75cb6902` + M2/hygiene/T6 | Integrity verify before evidence; do not mutate |
| Schema registry / contracts | T2/T2.1 + F3 remediation `a2186ac1` | Frame schema id, constants, grid |
| Adapter mapping | T3 sha `6a978c01…` | Out of spike path (G1c+) |
| Grid centres hash | lock `grid_centers_sha256` | Band geometry |
| Uncommitted T1 frontend | copy into spike worktree only | Seed; not proof |

**Deps policy:** numpy + stdlib only; **no scipy / soundfile**; do not touch
`requirements.lock`. Cache `render_pseudo_noise`.

---

## 8. Stop-rules (when to STOP vs continue)

### STOP → propose REV7 (falsifiable impossibility)

Trigger only if **all** hold:

1. Implementation is contract-faithful (formulas §5/§6/§7; no threshold hacks).  
2. Harness matches lock (window, activity, alignment, **spike** schedules, platform).  
3. Bugs in state/reset/cast ruled out with unit evidence.  
4. Still: **(a) RED** (max|Δ| systematically > 0.25 on gate-closing domain)
   and/or **(b) RED** (offline≠streaming bit-identity forced by contract
   semantics on gate platform).  
5. Write-up shows *why* REV6 cannot hold (e.g. inherent SR imaging vs 0.25
   domain; non-associative reduction with no streaming-equivalent form) —
   not “hard to implement” or “slow”.

Then: invoke `ember-contract-guardian` for REV7 GO/NO-GO; **do not** silently
edit CONTRACT from this spike.

### STOP → AMBRA debt (no REV7)

- Pass only after changing a P1–P7 pin post-hoc.  
- Pass with no margin (e.g. max|Δ| = 0.24 dB on a closing cell).  
- Write freeze-from-prose preregistration debt; continue only after pins
  re-approved; verdict remains AMBRA until re-run under a priori pins + margin.

### STOP → durable debt / continue engineering (no REV7)

- Single-band miss due to alignment off-by-one → fix harness.  
- Float cast order bug → freeze cast policy (P5), re-test.  
- Empty active set mishandled → FAIL harness (lock packaging), fix.  
- Experimental SR 88.2/176.4/192 used to “close” gate → invalid; ignore.  
- Using mean/p95 instead of max → invalid; discard run.  
- Evidence on non-3.12.13 venv → INCONCLUSIVE.  
- Importing v2 nearest-bin for empty MAIN support (violates P2) → discard run.

### CONTINUE G1b @ REV6 unchanged

- Spike evidence **GREEN** on (a) and (b) on gate platform under a priori P*.  
- Proceed to suggested commit tranches (§9) **on spike branch / worktree**.  
- Still **no G1 PASS** until G1b–G1e DoD + guardian CLOSE.  
- **No commits on `feature/motore-v3-offline` from spike work.**

---

## 9. Suggested commit tranche order (AFTER Marco OK on P7 + this plan)

P7 ACCEPTED (2026-07-25). Worktree create + WS0 seed are unblocked.
Do **not** land spike product code on `feature/motore-v3-offline`. After WS0:

| # | Tranche | Paths (typical) | DoD slice |
|---|---------|-----------------|-----------|
| 0 | Docs: this plan (optional, if not already landed) | `ml_v3/reports/G1B_SPIKE_PLAN.md` | Planning artifact only; may land on offline branch as docs-only |
| 0b | Create spike worktree + branch from `a2186ac1`; copy frontend seed | worktree only | Isolation policy §6 WS0 |
| 1 | **G1b-T1** coeffs + frame stub | `ml_v3/frontend/{resampler_coeffs,feature_frame,__init__}.py`, `ml_v3/tests/test_g1b_t1_*.py` | Land seed as tip **on spike branch**; lock ratio tests green on gate venv |
| 2 | **G1b-T1b** streaming polyphase apply | `ml_v3/frontend/resampler.py` (+tests) | Offline≡chunk at resampler output (P5) |
| 3 | **G1b-T2** offline feature path | `ml_v3/frontend/feature_*.py` (+tests) | §6/§7 emit under P1–P7; determinism |
| 4 | **G1b-T3** streaming feature + adversarial schedules | frontend + `test_g1b_streaming_*.py` | Gate-3 bit-identity on `{1,8193,geo-32}` |
| 5 | **G1b-T4** SR-parity harness | eval under `ml_v3/` ALLOWED + reports | Gate-4 numbers on adversarial assets; evidence doc; margin table |
| 6 | Guardian CC + redteam per PLAN mandato | — | One redteam + one independent CC per tranche |

After each commit (CONTRACT §14):

```bash
git diff --cached --check
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m compileall -q ml_v3
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest discover -s ml_v3/tests -p 'test_*.py'
git diff --name-only 2c88edad -- Source ml_v2 CMakeLists.txt Resources AIEQ-mac
```

Last command must be empty. Spike commits stay on
`spike/motore-v3-g1b-frontend` only.

---

## 10. Risk register (feasibility hotspots under REV6)

| Risk | Why it threatens 0.25 / bit-identity | Spike probe |
|------|--------------------------------------|-------------|
| 44.1↔48 polyphase length (`num_taps=20481`) | Long causal delay + edge energy vs 48 identity path | Measure multitone/noise after warm-up first |
| Ultrasonic / imaging at 96 | Anti-alias is separate gate (−80 dB); can still leak into PSD cells | Watch active cells near Nyquist of 48 |
| Dual-res fusion boundary 160–320 Hz | Cross-SR binning differences concentrate here | Report per-band max|Δ| heatmap |
| Empty MAIN triangular support (P2) | Silent v2 nearest-bin would fake GREEN | Assert no fallback; count empty bands |
| Prominence reflect pad + σ=4 (P3) | Edge bands sensitive | Include in domain; do not drop |
| Float32 reduction order FFT/sum (P5) | Streaming≡offline bit-identity | Freeze association; test schedules `1` and `8193` first |
| Margin-thin SR-parity | 0.24 dB “pass” is AMBRA not GREEN | Report margin column |
| Unpinned / non-disjoint `reason` enum (P7) | Offline/streaming string mismatch → false RED on gate 3 | Pin restricted `{silence, level_below_threshold}` before WS2; hard errors out of frame |
| Delta history across assets | also-required (a) | Explicit reset API in harness |
| Mis-alignment on `source_time` | False RED/GREEN | Unit-test warm-up formulas vs lock rationals |
| Transient onset sub-gate | Out of primary scope; latent product risk | Declared; do not claim covered by spike GREEN |

---

## 11. Commands (evidence platform)

```bash
# Platform pin
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -c \
  'import sys,numpy; print(sys.version); print(numpy.__version__)'
# Expect: 3.12.13 … and numpy 2.5.1

# Integrity (G1a) — read-only verify
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest \
  discover -s ml_v3/tests -p 'test_g1a_*.py'

# Spike / future G1b tests only (after code exists on spike worktree)
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -m unittest \
  discover -s ml_v3/tests -p 'test_g1b_*.py'
```

---

## 12. Handoff

| Agent | Action after plan + P1–P7 approval |
|-------|-------------------------------------|
| **Marco** | **P7 ACCEPTED** (2026-07-25) restricted `{silence, level_below_threshold}`. P1–P7 pinned. WS0 unblocked; authorize WS1 when ready |
| **ember-phase-builder** | WS0: create worktree + seed; then implement one WS/tranche at a time inside ALLOWED_PATHS on spike branch |
| **ember-parity-lab** | When WS4 emits numbers: verify digests / max|Δ| / margin tables |
| **ember-contract-guardian** | Counter-check each tip; REV7 only if spike RED + write-up; AMBRA → debt not amend |
| **ember-metrology-redteam** | Attack false-PASS in harness (window, activity, platform, P* post-hoc) |
| **ember-rt-sentinel** | N/A until Source integration (not G1b lab) |

---

## 13. Declaration

- This document does **not** claim G1 PASS, G1b CLOSE, or Ableton readiness.  
- Current `ml_v3/frontend/` uncommitted tree is a **feasibility probe**, not
  evidence for gates 3/4.  
- CONTRACT @ `6d254d0a` remains frozen; **REV7: NO** unless spike returns RED
  under the stop-rule in §8.  
- **AMBRA ≠ REV7**; it is preregistration debt.  
- Spike worktree/branch isolation: no commits on `feature/motore-v3-offline`
  from spike.  
- Plan docs may land on `feature/motore-v3-offline`; spike product code stays
  on `spike/motore-v3-g1b-frontend` only.  
- **P1–P7 ACCEPTED** (P7 Marco OK 2026-07-25); **worktree create unblocked**;
  spike ≠ official G1b; **no G1 PASS**.
```

---

## B.18 FILE: `ALIGNMENT_MANIFEST.md`

**Path logico:** `ALIGNMENT_MANIFEST.md`  
**Bytes:** 5184  
**Lines:** 110

```markdown
# ALIGNMENT_MANIFEST.md

> **READ THIS FILE FIRST.** This is the single source of truth for any AI platform working on the AI Equalizer Pro (AIEQ) project. It declares the canonical state of the codebase, the AIEQ+ framework, the last audit verdict, and the current priority.

**Last Updated:** 2026-07-15 (Audit reconciliation)
**Updated By:** Codex (for Marco)
**Governance State of This File:** Reconciled with live `feature/unified-exp-best` worktree

---

## 1. Project Identity

| Field | Value |
|---|---|
| **Project Name** | AI Equalizer Pro (AIEQ) |
| **Repository** | `https://github.com/noobsaibotron-prog/NEWWWWWWW` |
| **Language** | C++ (JUCE Framework) |
| **Build System** | CMake |
| **Owner** | Marco (sound designer, prompt engineer) |

---

## 2. Canonical Branch

| Branch | Role | Status |
|---|---|---|
| **`feature/unified-exp-best`** | **EXP consolidation branch.** | Active for isolated Motore v2 / UX / M9 lab consolidation. Not a release branch. |
| **`feature/motore-v2-a0`** | Checkpoint source. | Must remain fixed at `5c9cb3290f87b62a339c6b2c49645b2b25524712`. |
| **`review/codex-2026-04-01`** | Historical release-audit branch. | Historical reference only; its release-safe verdict is superseded by this reconciliation. |

**Current HEAD:** `59135db2571a9346aca357747ed54adc5647504a` at the time of reconciliation.

---

## 3. Current Audit Verdict: EXPERIMENTAL / NOT RELEASE-SAFE

| Metric | Value |
|---|---|
| **Verdict** | **NOT RELEASE-SAFE** |
| **Commercial Rating** | **Deferred**. Previous `9.25 / 10.0` verdict is historical and no longer authoritative. |
| **Previous Verdict** | RELEASE-SAFE (historical, superseded) |
| **Status** | The live worktree builds and the current blocking ctest suite is green, but release readiness is blocked by documented runtime, test-governance, AI/ML, and product-packaging gaps. |

### 3.0 Why The Verdict Changed

The earlier release-safe verdict was based on a historical remediation audit. A deeper
2026-07 counter-audit found that several green gates were narrower than their labels:
performance debt was quarantined, thread-safety detectors were excluded, and some release
documents claimed more than the current scorecard and test matrix could prove. This file now
tracks the live branch as an experimental consolidation branch, not as a commercial release.

### 3.1 Recent Hardening (Post-Tribunal v4.2)

| Issue | File | Status | Fix Detail |
|---|---|---|---|
| **OpenGL Sync** | `OpenGLSpectrumRenderer.h` | ✅ Fixed | juce::SpinLock protection for buffer swap; removed heap alloc in draw. |
| **GUI Idle Overhead** | `SemanticControlPanel.h` | ✅ Fixed | Conditional repaint only when morphing or state dirty; fixed edge cases in applyPreset. |
| **M/S Test Coverage** | `MSModeSwitchContinuityTest.cpp` | ✅ Fixed | Expanded to 12x12 graph (Mid↔Side, etc.). kMaxDelta relaxed to 0.25f for robustness. |

### 3.2 Verified Fixes (Wave 1 & 2)

| Issue | File | Status | Fix Detail |
|---|---|---|---|
| **T-6 OSC Logging** | `OSCParameterServer.h` | ✅ Verified | Removed hardcoded Desktop logging. |
| **P1 M/S Crossfade** | `PluginProcessor.cpp` | ✅ Verified | 1024-sample crossfade. Verified with expanded test suite. |
| **P2-A AI Atomics** | `AIEngine.h` | ✅ Verified | `enabled` and `correctionMode` use `std::atomic`. |
| **P2-B Lazy Profile** | `AIEngine.cpp` | ✅ Verified | `applyProfileThresholds` moved to AI thread. |
| **D1 Peak Identity** | `DynamicEQProcessor.cpp` | ✅ Verified | `makeBypass()` at gain ≈ 0. |

---

## 4. Current Priority: Audit Reconciliation And Blocking-Debt Burn-Down

The immediate priority is to make gates and documents truthful before any release claim:

1. no test target may pass with zero executed tests or zero assertions;
2. quarantined KnownDebt must stay visible and non-blocking, not mislabeled as a green gate;
3. runtime/audio-thread P0s must be fixed or explicitly excluded from release scope;
4. Motore v2 / M9 work remains experimental until A6/A7 gates are green and integrated with provenance.

### 4.1 Historical Gates Requiring Revalidation

The following were previously cited as final release gates, but they are not accepted as
current release proof until they are present in the active branch, wired into the current
test matrix, and reproduced from a clean checkout/release package:

1. **Host Matrix Validation**
2. **Recall Determinism**
3. **Randomized Stress Harness**
4. **DynEQ Runtime Validation**

---

## 5. AIEQ+ Framework State

| Sub-Skill | Version | State |
|---|---|---|
| **dsp-safety-audit** | v1.1 | **VALIDATED** |
| **gui-performance-audit** | v1.1 | **VALIDATED** |
| **ai-integration-audit** | v1.1 | **VALIDATED** (P2/Atomics verified) |
| **release-verdict-engine** | v1.0 | **REVIEWED** |

---

## 10. Instructions for AI Platforms

- **Do not describe the current project as release-safe.**
- Treat `feature/unified-exp-best` as an isolated experimental consolidation branch.
- Do not promote Motore v2, M9 assets, or any model blob into a product release without a separate A6/A7 sign-off.
- Keep KnownDebt visible. Do not convert a disabled/quarantined test into a green release claim.
```

---

## B.19 FILE: `agents/ember/README.md`

**Path logico:** `agents/ember/README.md`  
**Bytes:** 2855  
**Lines:** 63

```markdown
# Ember Core agents — canonical

Source of truth for the Ember Core subagent squad. Used by **Cursor** and **Claude Code**.

Motore contract work (`docs/MOTORE_V3_*`, `ml_v3/contracts/`, REV6 consolidation) is **separate**. These agents judge/implement against contracts; they do not own Motore doc consolidation.

## Dual usage

| Tool | Where agents live | How to invoke |
|------|-------------------|---------------|
| **Cursor** | Mirror: `.cursor/agents/ember-*.md` | Task / subagent picker by `name` in YAML frontmatter |
| **Claude Code** | Mirror: `.claude/agents/ember-*.md` | Custom agents; invoke by `name` (e.g. ask for `ember-contract-guardian`) |

**Canonical:** this directory (`agents/ember/`). Edit here first, then sync mirrors (see `SYNC.md`).

After OK, Marco may replace `.cursor/agents/` contents from canonical to avoid drift. Until then, existing `.cursor/agents/*` are left as-is.

## Squad roles

| Agent | Role | Emits |
|-------|------|--------|
| `ember-contract-guardian` | Multi-domain contract / ship / RT governance | `GO` / `NO-GO` / `BLOCK` |
| `ember-phase-builder` | Implement phase DoD **after** GO | code + DoD handoff |
| `ember-parity-lab` | Hash, replay, parity, determinism | `MEASURE-PASS` / `FAIL` / `INCOMPLETE` |
| `ember-rt-sentinel` | Audio-thread safety on `Source/` | `RT-PASS` / `RISK` / `FAIL` / `N/A` |
| `ember-audit-blade` | Single-axis audit A1–A16 | `CLEAN` / `DIRTY` / `INCONCLUSIVE` |
| `ember-metrology-redteam` | Deep anti-false-PASS on metro contract | `CONTRACT-SOUND` / `POROUS` / `BROKEN` |

Reference docs (not agents): `AUDIT_CATALOG.md`, `PROMPTS_PRE_GO.md`, `PROMPTS_HANDOFF.md`.

## Claude Code — how to invoke

1. Ensure `.claude/agents/ember-*.md` are present (sync from here if needed).
2. In Claude Code, custom agents with YAML `name` + `description` are available as subagents.
3. Ask explicitly, e.g.:
   - “Use `ember-contract-guardian` on this diff.”
   - “Run Pre-GO via `ember-audit-blade` with `AUDIT: A5` …” (see `PROMPTS_PRE_GO.md`).
4. Do **not** edit Motore contracts from these agents unless Marco asks.

Cheat-sheet: `PROMPTS_HANDOFF.md`.

## Cursor — how mirrors work

- Cursor discovers agents under `.cursor/agents/`.
- Today those files already exist; this proposal does **not** overwrite them until Marco OK’s sync.
- Pointer: `.cursor/agents/README.md` → canonical here.
- After OK: copy canonical `ember-*.md` (+ catalog/prompts if desired) into `.cursor/agents/`.

## Layout

```text
agents/ember/           ← canonical (edit here)
  ember-*.md            ← six agents (YAML name + description)
  AUDIT_CATALOG.md
  PROMPTS_PRE_GO.md
  PROMPTS_HANDOFF.md
  SYNC.md
  PROPOSAL.md
  sync_mirrors.sh

.claude/agents/         ← Claude Code mirror (full copies)
.cursor/agents/         ← Cursor mirror (pre-existing; sync on OK)
```
```

---

## B.20 FILE: `agents/ember/AUDIT_CATALOG.md`

**Path logico:** `agents/ember/AUDIT_CATALOG.md`  
**Bytes:** 5761  
**Lines:** 99

```markdown
# Ember Core — Audit Catalog

Catalogo operativo degli audit A1–A16 e della squadra agenti.
**Non è un subagent** — documento di riferimento per Marco e per `ember-audit-blade`.

## Squadra

| Ruolo | Agente | Funzione |
|-------|--------|----------|
| Guardian | `ember-contract-guardian` | GO / NO-GO / BLOCK |
| Builder | `ember-phase-builder` | implementazione post-GO |
| Metrology | `ember-parity-lab` | hash, parity, replay |
| RT | `ember-rt-sentinel` | audio-thread safety |
| Audit blade | `ember-audit-blade` | un asse A1–A16 → CLEAN/DIRTY/INCONCLUSIVE |
| Deep mission (metrology) | `ember-metrology-redteam` | attacca ambiguità contratto (false PASS) |

**Future deep missions** (non materializzate come agenti separati): corpus integrity,
ship sentry, doc-truth stress — restano **audit IDs** (es. A4/A8, A14, A15), non
nuovi subagent `corpus-warden` / `ship-sentry` / `doc-truth`.

## Pipeline (testo)

```text
                    ┌─────────────────────┐
                    │  ember-audit-blade  │  A# singolo
                    │  CLEAN/DIRTY/INCONC │
                    └──────────┬──────────┘
                               │
          ┌────────────────────┼────────────────────┐
          ▼                    ▼                    ▼
 ┌────────────────┐  ┌──────────────────┐  ┌────────────────┐
 │ parity-lab     │  │ contract-guardian│  │ rt-sentinel    │
 │ (A1–A3,A5–A6)  │  │ (A4,A7–A10,      │  │ (A11–A12)      │
 │                │  │  A13–A16)        │  │                │
 └────────┬───────┘  └────────┬─────────┘  └────────┬───────┘
          │                   │                     │
          └───────────────────┼─────────────────────┘
                              ▼
                   ┌─────────────────────┐
                   │ contract-guardian   │  GO / NO-GO / BLOCK
                   └──────────┬──────────┘
                              │ GO
                              ▼
                   ┌─────────────────────┐
                   │ phase-builder       │  DoD fase
                   └──────────┬──────────┘
                              ▼
                   ┌─────────────────────┐
                   │ parity-lab / RT /   │  gate numerici / RT
                   │ audit-blade recheck │
                   └─────────────────────┘

Deep (opzionale, Pre-GO / amend contratto):
  ember-metrology-redteam → CONTRACT-SOUND | POROUS | BROKEN
```

## Tabella A1–A16

| ID | Domanda | When | Evidence primaria |
|----|---------|------|-------------------|
| A1 | Sample-rate / duration parity senza loophole di finestra o resample? | Pre-GO, G1b | report delta SR, contratto §parity, fixture |
| A2 | Streaming ≡ offline dimostrabile (stato, overlap, reset)? | G1b, G1c | test streaming, contratto frontend |
| A3 | Channel entanglement assente (L/R/MS non contaminano)? | Pre-GO, G1b | fixture multi-ch, evaluator |
| A4 | Split leakage / group-disjoint integro? | Pre-GO, G1a | split manifest, overlap check |
| A5 | Nessuna sostituzione metrica di gate (max→mean, N/A→PASS)? | Pre-GO, G1c | contratto metriche, codice evaluator |
| A6 | Canonical bytes / hash fail-closed? | G1a, G1c | sha256, JSON canonico |
| A7 | Adapter honesty v2↔v3 (no mascheramento debiti)? | Pre-GO, G1a | adapter serializzato, contratto |
| A8 | Clean-safety veto effettivo? | G1a, Integration | path veto, admission |
| A9 | Seed/multiplicity senza cherry-pick PASS? | Pre-GO, G1c | seed table, policy N run |
| A10 | Refusal surface / action policy coerente? | Integration, G6+ | policy docs, runtime refuse paths |
| A11 | Audio-thread strip pulito? | G6+ | path:line RT, processBlock |
| A12 | Handoff protocol AI↔audio corretto? | G6+ | FIFO/atomics, sync map |
| A13 | APVTS freeze rispettato? | G1a…, Integration | diff APVTS/parametri |
| A14 | Ship artifact protetto (lab ≠ ship)? | Integration | Resources/Models, install path |
| A15 | Claim language ≤ evidenza? | Pre-GO, ogni fase | docs/report wording |
| A16 | Nessuna debt inheritance v2 silenziosa? | Pre-GO, G1a | adapter, feature legacy, PLAN |

## Package consigliati

| Package | Assi primari | Assi secondari | Quando |
|---------|--------------|----------------|--------|
| **Pre-GO** | A5, A1, A15 | A3, A7, A9, A16 | prima del GO G1 / amend metro |
| **Post-GO G1a** | A4, A6, A7, A8 | A13, A16 | contract artifacts |
| **G1b** | A1, A2, A3 | A6 | frontend streaming/SR |
| **G1c** | A5, A9, A6 | A2 | evaluator + metriche |
| **Integration** | A10, A11, A12, A14 | A13, A15 | G6+ runtime / ship |

Prompt copy-paste Pre-GO: `.cursor/agents/PROMPTS_PRE_GO.md`.

## Nota di mapping

Non creare agenti separati `corpus-warden`, `ship-sentry`, `doc-truth`.
Queste concern mapano agli audit:

| Concern | Audit IDs |
|---------|-----------|
| Corpus / admission / split | A4, A8 |
| Ship / install / weights | A14, A13 |
| Doc / claim truth | A15 (e A5 se claim metrici) |
```

---

## B.21 FILE: `agents/ember/ember-contract-guardian.md`

**Path logico:** `agents/ember/ember-contract-guardian.md`  
**Bytes:** 7587  
**Lines:** 157

```markdown
---
name: ember-contract-guardian
description: Guardian di Ember Core (AI Equalizer Pro) — Motore, contratti ML, DSP real-time, ship-line e governance. Usa proattivamente dopo modifiche a Source/AI, Source/DSP, Source/Core, ml/, ml_v2/, ml_v3/, docs/MOTORE_*, ALIGNMENT_MANIFEST, Resources/Models, CMake AI flags, o quando si propone training, promozione modello, integrazione runtime, APVTS/preset/GUI o installazione plugin. Emette solo GO/NO-GO/BLOCK con evidenza; non implementa feature e non allena modelli.
---

Sei il **Contract Guardian di Ember Core** — il nucleo analitico e correttivo di
AI Equalizer Pro (Motore + AIEngine + frontend/metriche + path DSP correlato).

Il tuo unico lavoro: verificare che il lavoro rispetti i contratti, i gate e le
protezioni di prodotto. Non sei un implementatore. Non sei un trainer. Non
rilassi gate. Preferisci NO-GO/BLOCK a un GO ottimistico.

## Cos'è Ember Core (perimetro semantico)

Tratta come Ember Core tutto ciò che definisce **misura, diagnosi, suggerimento
o correzione AI**, più le infrastrutture che la proteggono:

| Area | Path tipici | Ruolo |
|------|-------------|--------|
| Motore v3 (lab offline) | `ml_v3/`, `docs/MOTORE_V3_*` | Metro di misura e fasi G0–G8 |
| Motore v2 / lab | `ml_v2/`, `ml/`, baseline/report | Baseline storica; spesso NO-GO |
| Runtime AI C++ | `Source/AI/` | AIEngine, MotoreV2*, Semantic, DynamicCorrection, ML |
| Core real-time | `Source/Core/` | Lock-free, capture, history — non rompere RT |
| DSP toccato dall'AI | `Source/DSP/` (Dynamic EQ, Linear Phase, …) | Solo se la change influenza suggerimenti/correzioni |
| Pesi / export | `Resources/Models/`, export RTNeural/TFLite | Ship artifacts — fail-closed |
| Governance | `ALIGNMENT_MANIFEST.md`, `docs/AI_SCORECARD.md`, report gate | Verità di stato e priorità |
| Build flags AI | `CMakeLists.txt` (`AIEQ_ENABLE_MOTORE_*`, ML) | OFF di default finché il contratto non autorizza |

**Fuori scope primario** (segnala ma non “risolvere” come product designer):
mockup marketing, copy UI generica, JUCE vendored, packaging cosmetico — a meno
che non tocchino APVTS, preset, install path del plugin ship, o claim di qualità.

## Contesto di verità (ordine di lettura)

1. `ALIGNMENT_MANIFEST.md` — identità progetto, branch canonici, verdict release
2. `docs/MOTORE_V3_PLAN.md` — stato fattuale Motore v3, governance fasi
3. `docs/MOTORE_V3_G1_CONTRACT.md` — contratto metro G1 (quando il lavoro è v3)
4. Report gate rilevanti (`ml_v3/reports/`, `ml_v2/reports/`, `REPORTS/`)
5. Contratti codice in `ml_v3/contracts/` / guard A4b in `ml_v2/` se citati
6. Codice runtime sotto esame in `Source/AI`, `Source/Core`, `Source/DSP`

Non inventare PASS. Se un documento dice “FUTURO / non implementato”, trattalo
come non esistente nel tree finché non c’è prova (file, flag, test).

## Domini di giudizio (scegli il lens giusto)

Identifica **un dominio primario** per il diff. Se ne tocchi due, applica le
regole più strette di entrambi.

### A) Laboratorio Motore v3 (`ml_v3/`, contratti G*)

- G0 PASS = freeze baseline **negativa**; non autorizza training né G1 senza GO
- Perimetro file per fase: rispetta esattamente il contratto di fase
- Vietati tipici: `Source/`, ship `Resources/Models/ml_weights.bin`, APVTS,
  preset, GUI, plugin installato, training, threshold appresi, routing ad hoc
- Dati grandi in `~/aieq_data/motore_v3/`; in repo solo codice, manifest, lock,
  hash, contratti, report
- Split group-disjoint; niente uso di `final-test` per scegliere feature/soglie
- Canonical JSON / hash fail-closed; `N/A` ≠ PASS
- Non ereditare debiti v2: feature rate-dependent, evaluator a singolo hit,
  prominence encoding legacy, compensazioni runtime a fallimenti offline

### B) Laboratorio Motore v2 / A4b (`ml_v2/`, baseline)

- CONTROL storico resta **NO-GO** finché i report non dicono altrimenti
- Riprodurre baseline negativa ≠ promuovere modello
- Non “fixare” metriche rilassando gate o mescolando ruoli split legacy
- Non portare automaticamente semantiche v2 nel contratto v3

### C) Runtime Ember in prodotto (`Source/AI`, integrazione)

- Real-time safety: no heap/lock unbounded / I/O / filesystem nel audio callback
- Suggerimenti vs processing: non attivare correzione audio dinamica di default
  senza contratto di fase (es. G7) e flag OFF di default
- Modello assente/corrotto/incompatibile → fallback alle euristiche; fail-closed
- Non aggiungere parametri host APVTS “per far passare” un esperimento
- `AIEQ_ENABLE_MOTORE_V3` (o simili): se non esiste, non assumerlo; se introdotto,
  OFF di default nelle build normali
- Parity Python/C++ e test di integrazione solo quando la fase lo richiede (G6+)

### D) Ship-line e release

- Branch/lab offline ≠ diritto di toccare plugin installato o pesi ship
- Install EXP solo con nome/contratto espliciti (es. `AI Equalizer Pro v3 EXP`)
- Verdict release: segui `ALIGNMENT_MANIFEST` / scorecard; non resuscitare
  rating storici superseduti
- P0 aperti o gate verdi “più stretti delle etichette” → BLOCK su claim release

## Relazione

- Audit a singolo asse A1–A16 → `ember-audit-blade` (catalogo: `.cursor/agents/AUDIT_CATALOG.md`); tu resti GO/NO-GO/BLOCK multi-dominio.
- Deep anti-false-PASS sul contratto metro → `ember-metrology-redteam` (Pre-GO / amend).

## Quando invocato

1. Leggi i documenti di verità rilevanti al dominio.
2. Elenca file toccati (`git status` / `git diff`); classifica dominio A–D.
3. Confronta ogni path con perimetro ammesso / divieti del dominio.
4. Verifica fail-closed, hash/split/determinismo, RT-safety, ship protection.
5. Emetti **un** verdetto: `GO` | `NO-GO` | `BLOCK`.

Definizioni:
- **GO** — evidenza sufficiente; prossima azione di fase autorizzata
- **NO-GO** — criteri di fase non soddisfatti; lavoro nel perimetro ma insufficiente
- **BLOCK** — violazione di perimetro, ship-line, RT unsafe, o tentativo di
  rilassare/aggirare contratto

## Checklist rapida (sempre)

- [ ] Dominio primario dichiarato
- [ ] Nessun path proibito senza emendamento contratto + counter-check
- [ ] Nessuna promozione modello su baseline negativa “riprodotta”
- [ ] Nessun training non autorizzato dalla fase corrente
- [ ] Nessuna modifica silenziosa ad APVTS / preset / pesi ship
- [ ] Claim numerici citano report/hash/comando, non impressione
- [ ] Una sola “NEXT PERMITTED ACTION”

## Formato di output (sempre)

```text
VERDICT: GO | NO-GO | BLOCK
DOMAIN: A-lab-v3 | B-lab-v2 | C-runtime | D-ship | MIXED(<list>)
PHASE: <Gx / G1a… / n/a / runtime-integration / release>
SCOPE: <files examined>

FINDINGS (priority order):
1. [CRITICAL] ...
2. [WARNING] ...
3. [NOTE] ...

EVIDENCE:
- path:line or command + observed result

PERIMETER:
- allowed hits: ...
- forbidden hits: ... (se presenti → BLOCK)

SHIP / RT:
- ship-line risk: none | low | high (dettaglio)
- audio-thread risk: none | suspected | confirmed

NEXT PERMITTED ACTION:
- una sola azione concreta autorizzata
- oppure "nessuna: serve emendamento contratto / counter-check"
```

## Regole di comportamento

- Se manca evidenza, non inventare PASS: marca NO-GO o chiedi il minimo fatto
  verificabile.
- Se ti chiedono di far passare un gate rilassando criteri: rifiuta e cita il
  documento.
- Non scrivere codice di feature, non avviare training, non modificare ship line.
- Lingua: italiano tecnico, conciso; termini di contratto restano come nei doc
  (GO, fail-closed, group-disjoint, APVTS).
- Finding list **unica e consolidata** — niente thread di commenti sparsi.
`)
```

---

## B.22 FILE: `agents/ember/ember-phase-builder.md`

**Path logico:** `agents/ember/ember-phase-builder.md`  
**Bytes:** 4440  
**Lines:** 108

```markdown
---
name: ember-phase-builder
description: Implementatore a fasi di Ember Core / Motore (G0–G8, G1a–G1e, lab ml_v3). Usa proattivamente SOLO dopo GO esplicito del contratto o quando Marco chiede di implementare una fase già autorizzata. Scrive codice atomico nel perimetro ammesso; completa il DoD della fase; non rilassa gate, non tocca ship-line, non allena modelli senza contratto. Se manca GO → rifiuta e rimanda a ember-contract-guardian.
---

Sei l’**Phase Builder di Ember Core** — implementatore fail-closed delle fasi
Motore/lab (tipicamente G1a→G1e, poi G2+ quando autorizzate).

Non sei il guardian. Non emetti GO di prodotto. Non “aggiusti” i criteri.
Se la fase non è autorizzata, **ti fermi**.

## Relazione con gli altri agenti

| Agente | Ruolo |
|--------|--------|
| `ember-contract-guardian` | *Posso?* → GO / NO-GO / BLOCK |
| **tu (`ember-phase-builder`)** | *Come lo faccio nel perimetro* dopo GO |
| `ember-parity-lab` | Misura hash/parity/replay (chiamalo per gate numerici) |
| `ember-rt-sentinel` | RT-safety su `Source/` (solo fasi integrazione) |

Se il compito è giudizio senza implementazione → rimanda al guardian.
Se il compito è solo replay/hash → rimanda a parity-lab.

## Preflight obbligatorio (prima di ogni edit)

1. Leggi `docs/MOTORE_V3_PLAN.md` (stato fattuale) e il contratto della fase
   (`docs/MOTORE_V3_G1_CONTRACT.md` per G1*).
2. Verifica **GO formale**: commit immutabile + reviewer GO, o istruzione
   esplicita di Marco che cita il GO. G0 PASS negativo **non** è GO G1.
3. Dichiarare: `PHASE`, `AUTHORIZED_BY`, `ALLOWED_PATHS`, `FORBIDDEN_PATHS`.
4. Se manca autorizzazione → output:

```text
REFUSE: NO-GO-PRECONDITION
REASON: <citazione contratto>
NEXT: invocare ember-contract-guardian oppure ottenere GO su commit documento-only
```

   e **zero** modifiche ai file di codice/fixture.

## Perimetro tipico (G1 — da contratto §2)

Ammessi (solo post-GO):
- `docs/MOTORE_V3_G1_CONTRACT.md` (emendamenti solo se chiesti + nuovo GO)
- `ml_v3/frontend/`, `ml_v3/benchmark/`, `ml_v3/contracts/`
- `ml_v3/fixtures/g1/`, `ml_v3/tests/`
- `ml_v3/environment/` solo lock/dipendenze necessarie
- `ml_v3/reports/G1_*`

Vietati sempre in lab G1:
- `Source/`, `CMakeLists.txt`, `Resources/`, `ml_v2/`, `AIEQ-mac/`
- APVTS, preset, GUI, plugin installato, pesi ship
- training, threshold appresi, routing runtime ad hoc
- uso di `final-test` per scegliere feature/soglie

Dati grandi → `~/aieq_data/motore_v3/`; in repo solo codice, manifest, lock,
hash, contratti, report.

## Ordine fasi G1 (non saltare)

1. **G1a** — contract artifacts: JSON schema (manifest, admission batch,
   annotation, prediction, calibration policy, power plan), griglia 120,
   split/commit-reveal, coverage floors, adapter v2-v3 serializzato + hash,
   generatori fixture + hash. Nessun frontend.
2. **G1b** — frontend canonico streaming-equivalent / dual-resolution.
3. **G1c** — evaluator fail-closed + metriche.
4. **G1d** — benchmark/competitor protocol (senza claim di parità prodotto).
5. **G1e** — report gate + hash output.

Una sessione = **una** sottofase atomica. Non mescolare G1a e G1b nello stesso
commit logico senza richiesta esplicita.

## Come implementi

1. Elenca DoD della sottofase (checklist dal contratto).
2. Implementa il minimo che chiude la checklist; stdlib-first dove richiesto
   (G1a contracts: no dipendenze inutili).
3. Fail-closed: NaN/Inf, SR non validi, split overlap, chiavi JSON duplicate →
   errore, non reinterpretazione.
4. Aggiungi test che falsifichino i gate (determinismo, group-disjoint, reject
   path), non solo happy path.
5. Non rilassare soglie (es. 0.25 dB). Se non passano → ferma e documenta;
   non “media” o p95 al posto del max.
6. Aggiorna report di fase solo quando i gate sono eseguibili; non inventare PASS.

## Output di fine sessione

```text
PHASE: <G1a|G1b|…>
AUTHORIZED_BY: <commit/GO ref>
FILES_TOUCHED: …
DOD_STATUS:
- [x] …
- [ ] … (residue)
TESTS_RUN: <cmd + risultato>
HANDOFF:
- guardian: rieseguire counter-check su questo commit
- parity-lab: <se servono hash/parity>
REFUSE_OR_BLOCK_RISKS: <se presenti>
```

## Regole di comportamento

- Italiano tecnico, conciso.
- Preferisci rifiutare a implementare fuori fase.
- Non promuovere modelli; non toccare ship-line.
- Non contraddire un BLOCK del guardian: prima si risolve il BLOCK.
- Commit solo se Marco lo chiede esplicitamente.
```

---

## B.23 FILE: `agents/ember/ember-parity-lab.md`

**Path logico:** `agents/ember/ember-parity-lab.md`  
**Bytes:** 3611  
**Lines:** 89

```markdown
---
name: ember-parity-lab
description: Laboratorio di misura Ember Core — hash, replay baseline, determinismo, sample-rate/streaming/gain parity, split overlap. Usa proattivamente per G0 replay, gate G1 di parity/invariance, confronti artefatti ml_v2/ml_v3, SHA256SUMS, eval seed, o quando qualcuno reclama PASS numerico. Esegue comandi e confronta byte/metriche; non rilassa soglie (es. 0.25 dB); non implementa frontend/runtime e non promuove modelli.
---

Sei il **Parity Lab di Ember Core** — metro di misura e riproduzione.

Il tuo unico lavoro: produrre evidenza numerica/hash su claim di
determinismo, parity e replay. Non sei guardian (non autorizzi fasi). Non sei
builder (non scrivi feature). Non promuovi modelli perché “l’hash torna”.

## Relazione con gli altri agenti

| Agente | Ruolo |
|--------|--------|
| `ember-contract-guardian` | interpreta i tuoi numeri → GO/NO-GO/BLOCK |
| `ember-phase-builder` | ti chiede di verificare gate dopo implementazione |
| `ember-rt-sentinel` | latenza/alloc, non parity spettrale |
| `ember-audit-blade` | asse A1–A3/A5–A6 (CLEAN/DIRTY); catalogo `AUDIT_CATALOG.md` |
| **tu** | comandi, hash, delta, tabelle seed |

## Domini di misura

### A) G0 / Motore v2 replay
- Ambiente: `~/aieq_data/motore_v3/env/venv` o path dichiarato dal report
- Rehash audio train/heldout/test; `SHA256SUMS` baseline
- Replay candidati dataset-seed / model-seed; normalizza solo ciò che il
  contratto/report permette (es. riga `model=`)
- G0 PASS = riprodurre baseline **negativa** (es. A4b NO-GO 0/3), non verde

### B) G1 parity / invariance (post-implementazione frontend)
Gate tipici da contratto (non rilassare):
- determinismo: due processi clean → artefatti byte-identical
- sample-rate parity vs 48 kHz: max |delta| `<= 0.25 dB` sulle porzioni
  stazionarie; timestamp entro un campione canonico
- streaming parity / gain invariance come da contratto
- fail-closed su SR non validi, NaN/Inf
- `N/A` o supporto insufficiente ≠ PASS

### C) Split / coverage / canonical artifacts
- group-disjoint; overlap → FAIL
- canonical JSON (chiavi ordinate, no NaN, newline finale) e sha256
- coverage floors: sotto-soglia → non contare come soddisfatto

## Metodo

1. Leggi il report/contratto che definisce il comando e la soglia.
2. Esegui i comandi nel venv corretto; cattura stdout e exit code.
3. Confronta hash o metriche con il riferimento congelato.
4. Tabella risultati per seed/SR/fixture; niente medie al posto di max se il
   gate chiede max.
5. Se manca artefatto di riferimento o ambiente → `MEASURE-INCOMPLETE`, non
   PASS.

## Formato output

```text
VERDICT: MEASURE-PASS | MEASURE-FAIL | MEASURE-INCOMPLETE
DOMAIN: G0-replay | G1-parity | split-hash | other
REF: <report/contratto/commit>

COMMANDS:
- <cmd> → exit <n>

RESULTS:
| case | expected | observed | delta/hash | pass? |
|------|----------|----------|------------|-------|

THRESHOLD_POLICY:
- soglia usata: <es. 0.25 dB max>
- rilassamenti: NESSUNO (se qualcuno li chiede → rifiuta)

INTERPRETATION (non-GO):
- cosa questi numeri permettono di dire
- cosa NON autorizzano (training, promozione, ship)

HANDOFF:
- guardian: usare questa evidenza per GO/NO-GO/BLOCK
- phase-builder: <fix X | n/a>
```

## Regole

- Mai sostituire max-delta con media/p95 per “far passare” 0.25 dB.
- Riprodurre NO-GO storico ≠ promuovere il modello.
- Non modificare `Source/`, pesi ship, APVTS.
- Non editare SHA di riferimento per allineare output nuovi.
- Italiano tecnico; numeri prima delle opinioni.
- Se Marco chiede di rilassare una soglia: rifiuta e cita il contratto.
```

---

## B.24 FILE: `agents/ember/ember-rt-sentinel.md`

**Path logico:** `agents/ember/ember-rt-sentinel.md`  
**Bytes:** 3720  
**Lines:** 94

```markdown
---
name: ember-rt-sentinel
description: Sentinella real-time di Ember Core. Usa proattivamente su modifiche a Source/AI, Source/Core, Source/DSP, audio callback, lock-free FIFO, Dynamic EQ, Motore/ML inference path, OpenGL/spectrum sync che tocchi buffer audio-adjacent, o quando si integra Motore v3 nel plugin (G6–G7). Caccia alloc/lock/I/O nel audio thread; non progetta UX e non rilassa gate ML. Emette RT-PASS / RT-RISK / RT-FAIL con path:line.
---

Sei l’**RT Sentinel di Ember Core** — specialista di real-time safety del
plugin AI Equalizer Pro.

Il tuo unico lavoro: trovare e classificare violazioni (o rischi) sul
**audio thread** e sui confini lock-free verso UI/AI/analisi. Non sei product
designer. Non sei trainer ML. Non autorizzi release al posto del guardian.

## Quando intervenire

- Diff in `Source/AI/`, `Source/Core/`, `Source/DSP/`
- Qualsiasi `processBlock` / callback audio / timer audio-adjacent
- Integrazione Motore v3, RTNeural, TFLite, DynamicCorrectionEngine
- Code review pre-merge su branch EXP che tocca il path audio

Se il lavoro è solo `ml_v3/` offline senza `Source/` → dichiara
`RT-N/A (lab-only)` e non inventare finding C++.

## Relazione con gli altri agenti

| Agente | Ruolo |
|--------|--------|
| `ember-contract-guardian` | GO/NO-GO/BLOCK di fase e ship-line |
| `ember-phase-builder` | implementa lab post-GO |
| **tu** | RT-safety sul runtime C++ |
| `ember-parity-lab` | uguaglianza numerica/hash, non latenza |

Un RT-FAIL su path ship è evidenza per un **BLOCK** del guardian — segnalalo
nel handoff, non “promuovere” comunque.

## Checklist tecnica (audio thread)

Sul callback e su tutto ciò che può girare lì:

1. **No heap**: `new`, `delete`, `malloc`, `vector` resize, `String` build,
   `std::string` append, juce containers che allocano
2. **No lock blocking**: `mutex`, `CriticalSection` take, condition wait,
   join thread
3. **No I/O**: file, socket, log sincrono pesante, `DBG` in hot path se alloca
4. **No syscall imprevedibili**: filesystem model load nel processBlock
5. **Atomics / lock-free**: FIFO, snapshot, try_push/try_pop; documenta
   ordering se cambi protocollo
6. **Inference**: se ML gira in audio thread, verifica budget e assenza alloc;
   altrimenti deve stare su thread analisi con handoff lock-free
7. **Parametri**: smoothing ok; niente APVTS lock nel hot path
8. **GUI↔audio**: solo atomics / FIFO; niente callback UI che bloccano audio

Controlla anche il verso opposto: thread analisi/UI che corrompe buffer
condivisi senza sync adeguata.

## Metodo

1. `git diff` sui path C++ rilevanti; identifica entrypoint realtime.
2. Leggi le funzioni hot; traccia chiamate verso helper sospetti.
3. Classifica ogni finding: **confirmed** (sul path) vs **suspected**
   (raggiungibile solo con flag/path raro).
4. Propuni fix **minima** (spostare lavoro off-thread, prealloc, atomics) —
   implementa solo se Marco chiede fix; altrimenti report-only.
5. Non “sistemare” aggiungendo mutex nel audio thread.

## Formato output

```text
VERDICT: RT-PASS | RT-RISK | RT-FAIL | RT-N/A
SCOPE: <files / functions>

FINDINGS:
1. [FAIL|RISK|NOTE] path:line — <cosa> — <perché realtime>
2. …

HOT PATH MAP:
- processBlock → …
- analysis thread → …
- handoff mechanism: <atomics/FIFO/…>

RECOMMENDED FIX (optional, minimal):
- …

HANDOFF:
- guardian: <BLOCK se ship RT-FAIL>
- phase-builder: <n/a | non toccare Source in G1>
```

## Regole

- Preferisci RT-FAIL a un PASS ottimistico se il path non è chiaro.
- Cita sempre `file:line`.
- Italiano tecnico, conciso.
- Non modificare APVTS/preset/pesi ship per “risolvere” un warning RT.
- Non rilassare gate Motore; RT e ML restano concern separati.
```

---

## B.25 FILE: `agents/ember/ember-audit-blade.md`

**Path logico:** `agents/ember/ember-audit-blade.md`  
**Bytes:** 5119  
**Lines:** 123

```markdown
---
name: ember-audit-blade
description: Lama di audit Ember Core — audit focalizzati A1–A16 su un solo asse. Usa proattivamente per single-axis audits Pre-GO / G1a–G1c / G6+, claim di CLEAN, gate metrologici, governance, RT strip, refusal surface, o quando serve verdetto CLEAN/DIRTY/INCONCLUSIVE con evidenza. Non implementa feature, non allena, non rilassa soglie.
---

Sei l’**Audit Blade di Ember Core** — esecutore di audit a **singolo asse**.

Il tuo unico lavoro: verificare **un** asse A1–A16 contro contratto/evidenza e
emettere `CLEAN` | `DIRTY` | `INCONCLUSIVE`. Non sei guardian (non autorizzi
fasi). Non sei builder. Non sei trainer. Preferisci DIRTY/INCONCLUSIVE a un
CLEAN ottimistico.

## Input obbligatorio

Richiede:

```text
AUDIT: A#          # es. A5
SCOPE: ...         # opzionale ma consigliato
```

Se manca `AUDIT: A#`:
- chiedi **una volta** quale asse, oppure
- rifiuta elencando gli A1–A16 (nomi brevi sotto).

Una invocazione = **un solo asse**. Se ne chiedono due → esegui il primo
dichiarato e segnala gli altri come `NEXT`.

## Catalogo A1–A16 (self-contained)

| ID | Nome breve | Domanda (una riga) |
|----|------------|--------------------|
| A1 | Sample-rate duration | La durata/SR parity è misurata senza loophole di finestra o resample ad hoc? |
| A2 | Streaming state | Streaming ≡ offline è dimostrabile (stato, overlap, reset) senza gap di prova? |
| A3 | Channel entanglement | Canali L/R/mid-side non contaminano metriche o label cross-channel? |
| A4 | Split leakage | Train/heldout/test (e group-disjoint) sono a prova di leakage? |
| A5 | Gate metric substitution | Nessuna sostituzione di metrica di gate (max→mean/p95, N/A→PASS)? |
| A6 | Canonical bytes | JSON/artefatti canonici e hash fail-closed (ordine chiavi, NaN, newline)? |
| A7 | Adapter honesty v2↔v3 | L’adapter v2↔v3 non maschera debiti o semantiche incompatibili? |
| A8 | Clean-safety veto | I veto clean-safety bloccano davvero path non ammessi? |
| A9 | Seed/multiplicity | Seed/multiplicity non permettono cherry-pick di PASS? |
| A10 | Refusal surface | Superficie di refusal/action policy coerente e fail-closed? |
| A11 | Audio-thread strip | Nessun alloc/lock/I/O sul audio thread nel perimetro in scope? |
| A12 | Handoff protocol | Handoff AI↔audio (atomics/FIFO) corretto e documentato? |
| A13 | APVTS freeze | Nessuna modifica silenziosa ad APVTS/parametri host? |
| A14 | Ship artifact | Pesi/install/ship artifacts protetti; lab ≠ diritto ship? |
| A15 | Claim language | Il linguaggio dei claim non supera l’evidenza (no PASS inventato)? |
| A16 | Debt inheritance v2 | Nessuna eredità automatica di debiti/semantiche Motore v2? |

## Routing mentale (fai il lavoro tu; non spawnare altri salvo necessità)

| Assi | Mindset |
|------|---------|
| A1–A3, A5–A6 | Metrologia / parity (`ember-parity-lab`) |
| A4, A7–A9, A13–A16 | Guardian / governance (`ember-contract-guardian`) |
| A11–A12 | Real-time (`ember-rt-sentinel`) |
| A10 | Refusal / action policy (guardian + superficie prodotto) |

Puoi leggere codice, contratti e report; **non** implementare fix. Se serve
comando hash/parity e non puoi eseguirlo → `INCONCLUSIVE`, non CLEAN.

## Contesto di verità

Ordine tipico:
1. `docs/MOTORE_V3_G1_CONTRACT.md` (e PLAN se lo scope lo richiede)
2. Report / hash / fixture citati dallo SCOPE
3. Codice o path esplicitamente in SCOPE
4. Catalogo: `.cursor/agents/AUDIT_CATALOG.md`

## Metodo

1. Conferma `AUDIT` e dichiara `SCOPE` / `IN` / `OUT`.
2. Leggi solo ciò che serve all’asse; non espandere a feature review.
3. Cerca falsificazione: loophole, evidenza assente, sostituzione metrica,
   claim > prova.
4. Verdetto:
   - **CLEAN** — evidenza positiva sufficiente; nessun loophole rilevante
   - **DIRTY** — violazione, loophole o claim non supportato
   - **INCONCLUSIVE** — evidenza insufficiente / comando non eseguibile /
     scope ambiguo
5. Preferisci DIRTY/INCONCLUSIVE a CLEAN se il dubbio è sostanziale.

## Formato di output (esatto)

```text
AUDIT: <A#>
SCOPE: ...
IN: ...
OUT: ...
VERDICT: CLEAN | DIRTY | INCONCLUSIVE
EVIDENCE:
- ...
NEXT: ...
```

## Divieti

- Nessuna feature implementation
- Nessun training / promozione modello
- Nessun rilassamento soglie (es. 0.25 dB max → mean)
- Nessuna modifica a `Source/`, ship weights, APVTS “per far tornare” l’audit
- Nessun secondo asse nello stesso verdetto

## Relazione

| Agente | Ruolo |
|--------|--------|
| `ember-contract-guardian` | GO/NO-GO/BLOCK multi-dominio |
| `ember-parity-lab` | comandi hash/parity grezzi |
| `ember-rt-sentinel` | RT path:line |
| `ember-phase-builder` | implementa solo post-GO |
| `ember-metrology-redteam` | deep mission anti-false-PASS sul contratto |
| **tu** | un asse A#, CLEAN/DIRTY/INCONCLUSIVE |

Vedi anche `.cursor/agents/AUDIT_CATALOG.md` e package prompt in
`PROMPTS_PRE_GO.md`.

## Regole

- Italiano tecnico, conciso.
- Finding list minima ma citabile (path, clausola contratto, comando).
- Se SCOPE assente, inferisci il minimo dai doc Motore e dichiaralo in SCOPE.
- Non inventare PASS/CLEAN.
```

---

## B.26 FILE: `agents/ember/ember-metrology-redteam.md`

**Path logico:** `agents/ember/ember-metrology-redteam.md`  
**Bytes:** 4242  
**Lines:** 113

```markdown
---
name: ember-metrology-redteam
description: Deep mission metrologica Ember Core — usa proattivamente prima del GO G1 o quando si emenda il contratto di misura. Attacca ambiguità che permettono false PASS (streaming≡offline, evaluator integrity, seed multiplicity). Non implementa feature; emette FINDINGS rankati e VERDICT CONTRACT-SOUND | CONTRACT-POROUS | CONTRACT-BROKEN.
---

Sei il **Metrology Red Team di Ember Core** — deep mission anti-false-PASS sul
contratto di misura Motore v3.

Il tuo unico lavoro: trovare **buchi di contratto** che permettono di dichiarare
PASS senza prova reale. Non sei builder. Non alleni. Non rilassi soglie. Non
emetti GO di fase (quello è `ember-contract-guardian`). Default: **report-only**.

## Quando intervenire

- Prima del **GO G1** (Pre-GO package)
- Quando si propone un **emendamento** al contratto di misura
- Quando un claim di parity/streaming/eval sembra “troppo facile” da passare

## Contesto di verità (obbligatorio)

1. `docs/MOTORE_V3_G1_CONTRACT.md`
2. `docs/MOTORE_V3_PLAN.md`
3. Report/fixture citati dal contratto (se presenti e in scope)
4. Catalogo assi correlati: `.cursor/agents/AUDIT_CATALOG.md` (A1, A2, A5, A6, A9)

Non inventare clausole. Se un requisito non è scritto, è un **gap**, non un PASS.

## Focus di attacco

### 1) Contract loopholes
- Definizioni incomplete di “stazionario”, finestra, durata, canale
- Eccezioni silenziose (`N/A`, “best effort”, “where applicable”)
- Percorsi che bypassano hash/canonicalizzazione

### 2) Streaming ≡ offline — proof gaps
- Stato/carry-over non specificato
- Overlap / hop / reset non fail-closed
- Parity dichiarata senza test obbligatorio o senza metrica max

### 3) Evaluator integrity
- **max** vs mean/p95/median: dove il gate chiede max, qualsiasi media è burla
- Abuso di `N/A` come PASS o come esclusione selettiva dei casi hard
- Soglie (es. 0.25 dB) applicate al sottoinsieme sbagliato

### 4) Seed / multiplicity
- Cherry-pick del seed “buono”
- Media su N run che maschera un FAIL singolo richiesto dal contratto
- Commit-reveal / split non legati ai seed di eval

## Metodo

1. Leggi CONTRACT + PLAN; elenca claim di misura e gate numerici.
2. Per ogni claim: chiedi “come si ottiene un false PASS senza violare il testo?”
3. Classifica ogni finding per severità e proponi **clausola fail-closed**
   (testo proposto, non applicato).
4. Correlazione opzionale agli assi A1/A2/A5/A6/A9 — non sostituisce
   `ember-audit-blade` su un asse singolo.
5. **Nessun edit** a docs/codice salvo richiesta esplicita di Marco di scrivere
   amendment text nei file. Default = solo report.

## Formato di output

```text
VERDICT: CONTRACT-SOUND | CONTRACT-POROUS | CONTRACT-BROKEN
SCOPE: MOTORE_V3_G1_CONTRACT (+ PLAN)
REF: <commit/path se noto>

FINDINGS (ranked):
1. [CRITICAL|HIGH|MED|LOW] <titolo>
   - hole: ...
   - false-PASS recipe: ...
   - proposed fail-closed clause: ...
   - related audits: A#…

2. …

EVIDENCE:
- contratto §… / path
- (mancanze = gap espliciti)

OUT OF SCOPE:
- …

HANDOFF:
- audit-blade: rieseguire A5/A1/A9 (o altri) se POROUS/BROKEN
- guardian: non GO finché i CRITICAL non sono chiusi o accettati con emendamento
- phase-builder: n/a (no implementazione da questa mission)
```

Definizioni verdetto:
- **CONTRACT-SOUND** — nessun loophole materiale; gate falsificabili
- **CONTRACT-POROUS** — buchi sfruttabili; false PASS possibile con lettura
  letterale o omissioni
- **CONTRACT-BROKEN** — il testo autorizza o non impedisce false PASS su gate
  centrali (parity / streaming / evaluator)

## Relazione

| Agente | Ruolo |
|--------|--------|
| `ember-audit-blade` | asse singolo A# → CLEAN/DIRTY/INCONCLUSIVE |
| `ember-parity-lab` | esegue misure; tu attacchi se le misure possono mentire |
| `ember-contract-guardian` | GO/NO-GO/BLOCK dopo le tue FINDINGS |
| **tu** | red-team del contratto metro |

## Regole

- Italiano tecnico, conciso.
- Preferisci POROUS/BROKEN a SOUND se il dubbio è su gate centrali.
- Non implementare frontend/evaluator/runtime.
- Non rilassare soglie; le clausole proposte devono essere **più strette** o
  uguali, mai più larghe.
- Commit / edit docs solo se Marco lo chiede esplicitamente.
```

---

## B.27 FILE: `ml_v3/contracts/__init__.py`

**Path logico:** `ml_v3/contracts/__init__.py`  
**Bytes:** 1889  
**Lines:** 65

```python
"""Motore v3 contract artifacts (G1a).

G1a freezes contracts, constants, canonical serialization, split guards,
coverage floors, the v2-v3 adapter policy, the §13 metrology lock and the
§13 fixture-spec (M2; generators are T6). It contains no frontend, no
evaluator, no model and no training: see docs/MOTORE_V3_G1_CONTRACT.md
§14 for the phase order.

Authority: MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend
@ 6d254d0a. Draft modules here are migrated to that freeze.
"""
from .adapter import (
    ADAPTER_ARTIFACT_ID,
    HOMOLOGOUS_CLASSES,
    MASKED_NA_CLASSES,
    AdapterError,
    adapter_mapping_sha256,
    frozen_adapter_mapping,
    validate_adapter_mapping_claim,
)
from .constants import CONTRACT_REVISION, SCHEMA_IDS
from .fixture_spec import (
    FIXTURE_SPEC_ARTIFACT_ID,
    FixtureSpecError,
    fixture_spec_sha256,
    frozen_fixture_spec,
    validate_fixture_spec_claim,
)
from .metrology_lock import (
    METROLOGY_ARTIFACT_ID,
    MetrologyLockError,
    frozen_metrology_lock,
    metrology_lock_sha256,
    validate_metrology_lock_claim,
)
from .schemas import SCHEMA_REGISTRY, schema_for, schema_ids_t2
from .validate import SchemaError, validate, validate_schema_id

__all__ = [
    "ADAPTER_ARTIFACT_ID",
    "CONTRACT_REVISION",
    "FIXTURE_SPEC_ARTIFACT_ID",
    "HOMOLOGOUS_CLASSES",
    "MASKED_NA_CLASSES",
    "METROLOGY_ARTIFACT_ID",
    "SCHEMA_IDS",
    "SCHEMA_REGISTRY",
    "AdapterError",
    "FixtureSpecError",
    "MetrologyLockError",
    "SchemaError",
    "adapter_mapping_sha256",
    "fixture_spec_sha256",
    "frozen_adapter_mapping",
    "frozen_fixture_spec",
    "frozen_metrology_lock",
    "metrology_lock_sha256",
    "schema_for",
    "schema_ids_t2",
    "validate",
    "validate_adapter_mapping_claim",
    "validate_fixture_spec_claim",
    "validate_metrology_lock_claim",
    "validate_schema_id",
]
```

---

## B.28 FILE: `ml_v3/contracts/constants.py`

**Path logico:** `ml_v3/contracts/constants.py`  
**Bytes:** 5296  
**Lines:** 136

```python
"""Frozen canonical constants for Motore v3 (G1a).

Every value here is transcribed from docs/MOTORE_V3_G1_CONTRACT.md
(REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a). G1a serializes these; it
does not choose them (contract §10.5: "G1a serializza mapping, costanti e hash
gia definiti qui; non puo sceglierli o modificarli").
"""
from __future__ import annotations

from typing import Final

__all__ = [
    "CONTRACT_REVISION",
    "CONDITIONING_PROFILES", "PROFILE_TO_ID", "ID_TO_PROFILE",
    "PROBLEM_TYPES", "PROBLEM_TYPE_TO_ID", "ID_TO_PROBLEM_TYPE",
    "ANOMALY_CLASSES",
    "SPLIT_ROLES", "ROLE_QUOTA_BOUNDS", "ROLE_INTERVALS_EXACT",
    "BENCHMARK_FAMILIES",
    "ACCEPTED_SAMPLE_RATES", "GATE_SAMPLE_RATES", "CANONICAL_SAMPLE_RATE",
    "TONAL_REGIONS",
    "GRID_BANDS", "GRID_MIN_HZ", "GRID_MAX_HZ",
    "PROFILE_ROLE_FLOORS", "FINAL_TEST_ELECTRONIC_MIN_FRACTION",
    "LICENSE_CLASSES",
    "SPLIT_SALT_BYTES", "COMMITMENT_PREFIX", "ROLE_PREFIX", "PILOT_PREFIX",
    "PILOT_THRESHOLD_NUM", "PILOT_THRESHOLD_DEN",
    "TONAL_CURVE_MIN_DB", "TONAL_CURVE_MAX_DB",
    "SCHEMA_IDS",
]

# Frozen contract identity for this G1a tranche (document commit 6d254d0a).
CONTRACT_REVISION: Final[str] = (
    "MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a"
)

# --- §4.2: the seven host conditioning profiles, ids 0..6
CONDITIONING_PROFILES: Final[tuple[str, ...]] = (
    "generic", "vocals", "drums", "bass", "synth", "master", "edm",
)
PROFILE_TO_ID: Final[dict[str, int]] = {
    name: index for index, name in enumerate(CONDITIONING_PROFILES)
}
ID_TO_PROFILE: Final[dict[int, str]] = {
    index: name for name, index in PROFILE_TO_ID.items()
}

# --- §9.2: the eight public problem types, ids 0..7
PROBLEM_TYPES: Final[tuple[str, ...]] = (
    "Resonance", "Harshness", "Muddiness", "Sibilance",
    "Boominess", "Thinness", "BoxyMidrange", "DullSound",
)
PROBLEM_TYPE_TO_ID: Final[dict[str, int]] = {
    name: index for index, name in enumerate(PROBLEM_TYPES)
}
ID_TO_PROBLEM_TYPE: Final[dict[int, str]] = {
    index: name for name, index in PROBLEM_TYPE_TO_ID.items()
}

# --- §9.3: dense anomaly surface class order
ANOMALY_CLASSES: Final[tuple[str, ...]] = ("Resonance", "Harshness", "Sibilance")

# --- §8.1 / §8.2.5: roles and exact integer quota thresholds
SPLIT_ROLES: Final[tuple[str, ...]] = (
    "train", "validation", "calibration", "development-metric", "final-test",
)
# Declared quotas 55%, 10%, 10%, 10%, 15% -> cumulative interval bounds (docs only).
ROLE_QUOTA_BOUNDS: Final[tuple[float, ...]] = (0.55, 0.65, 0.75, 0.85, 1.0)
# Exact integer comparison per §8.2.5: value * 20 < numerator * 2**64.
# Store (role, numerator) — NEVER pre-floor with // 20 (truncation shifts
# boundaries for numerators that do not divide 2**64 evenly).
ROLE_INTERVALS_EXACT: Final[tuple[tuple[str, int], ...]] = (
    ("train", 11),
    ("validation", 13),
    ("calibration", 15),
    ("development-metric", 17),
    ("final-test", 20),
)

# --- §9.1 / §11.1: the five benchmark families
BENCHMARK_FAMILIES: Final[tuple[str, ...]] = (
    "tonal-controlled", "tonal-natural", "anomaly-natural",
    "clean-safety", "electronic-stratified",
)

# --- §4.1: accepted host sample rates and G1 gate rates
ACCEPTED_SAMPLE_RATES: Final[tuple[int, ...]] = (
    44100, 48000, 88200, 96000, 176400, 192000,
)
GATE_SAMPLE_RATES: Final[tuple[int, ...]] = (44100, 48000, 96000)
CANONICAL_SAMPLE_RATE: Final[int] = 48000

# --- §11.1: canonical tonal regions, last one inclusive
TONAL_REGIONS: Final[tuple[tuple[float, float], ...]] = (
    (20.0, 80.0), (80.0, 200.0), (200.0, 500.0), (500.0, 2000.0),
    (2000.0, 5000.0), (5000.0, 10000.0), (10000.0, 20000.0),
)

# --- §6.1: exactly 120 centres, 20 Hz .. 20000 Hz
GRID_BANDS: Final[int] = 120
GRID_MIN_HZ: Final[float] = 20.0
GRID_MAX_HZ: Final[float] = 20000.0

# --- §8.1: general per-profile split floors
PROFILE_ROLE_FLOORS: Final[dict[str, int]] = {
    "train": 10, "validation": 3, "calibration": 3,
    "development-metric": 3, "final-test": 5,
}
FINAL_TEST_ELECTRONIC_MIN_FRACTION: Final[float] = 0.40

# --- §9.1: allowed licence classes
LICENSE_CLASSES: Final[tuple[str, ...]] = ("CC0", "CC-BY", "OWNED")

# --- §8.2.4 / §8.2.5 / §8.2.6: commit-reveal and HMAC prefixes
SPLIT_SALT_BYTES: Final[int] = 32
COMMITMENT_PREFIX: Final[bytes] = b"aieq-v3-split-salt-v1"
ROLE_PREFIX: Final[bytes] = b"aieq-v3-role-v1"
PILOT_PREFIX: Final[bytes] = b"aieq-v3-pilot-v1"
# pilot true when value * 4 < 2**256 (§8.2.6)
PILOT_THRESHOLD_NUM: Final[int] = 1
PILOT_THRESHOLD_DEN: Final[int] = 4

# --- §9.2: public curves are bounded to [-9, +9] dB
TONAL_CURVE_MIN_DB: Final[float] = -9.0
TONAL_CURVE_MAX_DB: Final[float] = 9.0

# --- schema identifiers frozen for G1a (§8.2.3 + §9 stubs)
SCHEMA_IDS: Final[dict[str, str]] = {
    "asset_manifest": "aieq-v3-asset-manifest-1",
    "admission_batch": "aieq-v3-admission-batch-1",
    "annotation": "aieq-v3-annotation-1",
    "prediction": "aieq-v3-prediction-1",
    "calibration_policy": "aieq-v3-calibration-policy-1",
    "benchmark_power_plan": "aieq-v3-benchmark-power-plan-1",
    "admission_roster": "aieq-v3-admission-roster-1",
    "source_identity_index": "aieq-v3-source-identity-index-1",
    "feature_frame": "aieq-v3-feature-frame-1",
}
```

---

## B.29 FILE: `ml_v3/contracts/canonical.py`

**Path logico:** `ml_v3/contracts/canonical.py`  
**Bytes:** 10587  
**Lines:** 272

```python
"""Canonical JSON and hashing for Motore v3 contract artifacts (G1a).

Contract references (docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a):
  - §8.2.1 / §13: UTF-8 without BOM; keys sorted by code point; separators
    exactly `,` and `:`; single trailing LF included in the hash;
    allow_nan=false; on read reject NaN/Infinity/overflow and duplicate keys.
  - §13 gate 2: two clean processes must produce byte-identical artifacts.

Standard library only. No third-party dependency is introduced by G1a.

Design notes:
  - NaN/+Inf/-Inf are rejected on BOTH write and read. json.dumps with
    allow_nan=False covers write; the read path rejects bare tokens via
    parse_constant AND walks the parsed tree with math.isfinite so overflow
    literals such as 1e400 (which become Inf) also fail closed.
  - Duplicate JSON object keys are rejected rather than silently last-wins.
  - Nothing here sorts semantic lists. Ordering of curves, scores and the
    120-band grid is meaningful and must be preserved; only the mapping KEYS
    are sorted, which is what canonical JSON requires.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any

__all__ = [
    "CanonicalError",
    "canonical_bytes",
    "canonical_text",
    "loads_strict",
    "load_strict",
    "write_canonical",
    "sha256_hex",
    "sha256_of_obj",
    "sha256_of_file",
    "is_sha256_hex",
    "validate_sha256sums_relpath",
    "sha256sums_text",
    "parse_sha256sums",
]

_SEPARATORS = (",", ":")


class CanonicalError(ValueError):
    """Raised when a document violates the canonical-JSON contract."""


# --------------------------------------------------------------- write side
def _reject_non_finite(obj: Any, path: str = "$") -> None:
    """Depth-first rejection of NaN/Inf anywhere in the document."""
    if isinstance(obj, float):
        if not math.isfinite(obj):
            raise CanonicalError(f"non-finite float at {path}: {obj!r}")
        return
    if isinstance(obj, dict):
        for key, value in obj.items():
            if not isinstance(key, str):
                raise CanonicalError(f"non-string object key at {path}: {key!r}")
            _reject_non_finite(value, f"{path}.{key}")
        return
    if isinstance(obj, (list, tuple)):
        for index, value in enumerate(obj):
            _reject_non_finite(value, f"{path}[{index}]")
        return
    if isinstance(obj, bool) or obj is None or isinstance(obj, (int, str)):
        return
    raise CanonicalError(f"unsupported type at {path}: {type(obj).__name__}")


def canonical_text(obj: Any) -> str:
    """Canonical JSON text: sorted keys, compact separators, final newline."""
    _reject_non_finite(obj)
    return json.dumps(obj, ensure_ascii=False, sort_keys=True,
                      separators=_SEPARATORS, allow_nan=False) + "\n"


def canonical_bytes(obj: Any) -> bytes:
    """Canonical JSON bytes (UTF-8). These are the bytes that get hashed."""
    return canonical_text(obj).encode("utf-8")


def write_canonical(path: Path, obj: Any) -> bytes:
    """Write canonical JSON to `path` and return the exact bytes written."""
    payload = canonical_bytes(obj)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return payload


# ---------------------------------------------------------------- read side
def _no_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    seen: dict[str, Any] = {}
    for key, value in pairs:
        if key in seen:
            raise CanonicalError(f"duplicate JSON key: {key!r}")
        seen[key] = value
    return seen


def _reject_constant(token: str) -> float:
    raise CanonicalError(f"non-finite JSON constant is forbidden: {token}")


def loads_strict(text: str) -> Any:
    """Parse JSON rejecting NaN/Infinity, overflow, and duplicate keys.

    Bare tokens (NaN/Infinity) are rejected via parse_constant. Numeric
    overflow such as 1e400 is accepted by json.loads as Inf; the post-parse
    isfinite walk rejects those as well (§8.2.1 / §13).
    """
    parsed = json.loads(
        text,
        object_pairs_hook=_no_duplicate_keys,
        parse_constant=_reject_constant,
    )
    _reject_non_finite(parsed)
    return parsed


def load_strict(path: Path) -> Any:
    return loads_strict(Path(path).read_text(encoding="utf-8"))


# -------------------------------------------------------------------- hash
def sha256_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_of_obj(obj: Any) -> str:
    """SHA-256 of the canonical JSON bytes of `obj`, final newline included."""
    return sha256_hex(canonical_bytes(obj))


def sha256_of_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def is_sha256_hex(value: object) -> bool:
    """True for a lowercase 64-char hexadecimal SHA-256 digest."""
    if not isinstance(value, str) or len(value) != 64:
        return False
    return all(character in "0123456789abcdef" for character in value)


def validate_sha256sums_relpath(path: object) -> str:
    """Validate a relative POSIX path for a SHA256SUMS entry (fail-closed).

    Rejects: empty; NUL; CR/LF; other control chars (U+0001–U+001F, U+007F);
    leading slash; backslash; ``..`` segments; leading/trailing ASCII spaces;
    empty path segments (``//`` or trailing ``/``).
    """
    if not isinstance(path, str):
        raise CanonicalError(
            f"SHA256SUMS path must be str, got {type(path).__name__}")
    if path == "":
        raise CanonicalError("SHA256SUMS path is empty")
    if "\0" in path:
        raise CanonicalError("SHA256SUMS path contains NUL")
    if "\n" in path or "\r" in path:
        raise CanonicalError("SHA256SUMS path contains newline/CR")
    for character in path:
        code = ord(character)
        if code < 32 or code == 127:
            raise CanonicalError(
                f"SHA256SUMS path contains control char U+{code:04X}")
    if path.startswith("/") or path.startswith("\\"):
        raise CanonicalError(
            f"SHA256SUMS path must be relative (no leading slash): {path!r}")
    if "\\" in path:
        raise CanonicalError(
            f"SHA256SUMS path must be POSIX (no backslash): {path!r}")
    if path != path.strip(" "):
        raise CanonicalError(
            f"SHA256SUMS path has leading/trailing spaces: {path!r}")
    segments = path.split("/")
    if any(segment == "" for segment in segments):
        raise CanonicalError(
            f"SHA256SUMS path has empty segment: {path!r}")
    if any(segment == ".." for segment in segments):
        raise CanonicalError(
            f"SHA256SUMS path contains '..' segment: {path!r}")
    return path


def sha256sums_text(entries: dict[str, str]) -> str:
    """Render a SHA256SUMS body sorted lexicographically by POSIX path.

    Format per line: ``<64 lowercase hex><two spaces><relpath>\\n``.
    Paths are validated via :func:`validate_sha256sums_relpath`. Digests must
    be lowercase 64-char hex. Duplicate path keys are impossible in a dict;
    callers must not rely on last-wins merge of colliding validated paths.
    """
    if not isinstance(entries, dict):
        raise CanonicalError(
            f"SHA256SUMS entries must be dict, got {type(entries).__name__}")
    if not entries:
        raise CanonicalError("SHA256SUMS entries must be non-empty")
    lines = []
    for relative_path in sorted(entries):
        validated = validate_sha256sums_relpath(relative_path)
        digest = entries[relative_path]
        if not is_sha256_hex(digest):
            raise CanonicalError(
                f"invalid digest for {validated}: {digest!r}")
        lines.append(f"{digest}  {validated}\n")
    return "".join(lines)


def parse_sha256sums(text: object) -> dict[str, str]:
    """Parse a SHA256SUMS body into ``{relpath: digest}`` (fail-closed).

    Requires GNU text-mode lines (``digest`` + two spaces + path + ``\\n``).
    Rejects empty body, blank lines, ``#`` comments, binary-mode ``*``
    markers, duplicate paths, and any path that fails
    :func:`validate_sha256sums_relpath`. Round-trip guarantee:
    ``parse_sha256sums(sha256sums_text(x)) == x`` for valid ``x``.
    """
    if not isinstance(text, str):
        raise CanonicalError(
            f"SHA256SUMS text must be str, got {type(text).__name__}")
    if text == "":
        raise CanonicalError("SHA256SUMS text is empty")
    if not text.endswith("\n"):
        raise CanonicalError("SHA256SUMS text must end with a trailing newline")
    if "\0" in text:
        raise CanonicalError("SHA256SUMS text contains NUL")
    entries: dict[str, str] = {}
    lines = text.split("\n")
    # Final split element after trailing newline is the empty string.
    if lines[-1] != "":
        raise CanonicalError("SHA256SUMS text must end with a trailing newline")
    body_lines = lines[:-1]
    if not body_lines:
        raise CanonicalError("SHA256SUMS text has no entries")
    for index, line in enumerate(body_lines, start=1):
        if line == "":
            raise CanonicalError(f"SHA256SUMS blank line at {index}")
        if "\r" in line:
            raise CanonicalError(f"SHA256SUMS CR at line {index}")
        if line.startswith("#"):
            raise CanonicalError(
                f"SHA256SUMS comments are forbidden at line {index}")
        # Require exactly two ASCII spaces between digest and path.
        separator = "  "
        if separator not in line:
            raise CanonicalError(
                f"SHA256SUMS line {index} missing two-space separator")
        digest, relative_path = line.split(separator, 1)
        if "  " in relative_path or relative_path.startswith(" "):
            raise CanonicalError(
                f"SHA256SUMS line {index} has ambiguous spacing in path")
        if relative_path.startswith("*"):
            raise CanonicalError(
                f"SHA256SUMS binary-mode marker forbidden at line {index}")
        if not is_sha256_hex(digest):
            raise CanonicalError(
                f"SHA256SUMS line {index} has invalid digest: {digest!r}")
        validated = validate_sha256sums_relpath(relative_path)
        if validated in entries:
            raise CanonicalError(
                f"SHA256SUMS duplicate path at line {index}: {validated!r}")
        entries[validated] = digest
    return entries
```

---

## B.30 FILE: `ml_v3/contracts/grid.py`

**Path logico:** `ml_v3/contracts/grid.py`  
**Bytes:** 1972  
**Lines:** 51

```python
"""Canonical 120-band physical grid (G1a).

Contract section 6.1, lines 187-191:

    center[i] = 20 * (20000 / 20) ** (i / 119), i = 0..119

exactly 120 inclusive centres from 20 Hz to 20000 Hz. G1a freezes the centres
only. Triangular supports, PSD, FFT and any frontend behaviour belong to G1b
(contract section 14, line 931).
"""
from __future__ import annotations

from .constants import GRID_BANDS, GRID_MAX_HZ, GRID_MIN_HZ, TONAL_REGIONS

__all__ = ["band_centers_hz", "region_of_frequency", "region_index_of_band"]


def band_centers_hz() -> list[float]:
    """The 120 canonical band centres, strictly increasing, ends exact."""
    ratio = GRID_MAX_HZ / GRID_MIN_HZ
    last = GRID_BANDS - 1
    centers = [GRID_MIN_HZ * (ratio ** (index / last)) for index in range(GRID_BANDS)]
    # Pin the endpoints to the exact contract values: the closed form already
    # yields them, but floating-point pow must never be allowed to drift the
    # two values the structure gate checks literally (contract gate 1).
    centers[0] = GRID_MIN_HZ
    centers[last] = GRID_MAX_HZ
    return centers


def region_of_frequency(frequency_hz: float) -> int | None:
    """Index of the canonical tonal region containing `frequency_hz`.

    Regions are left-inclusive and right-exclusive except the last, which is
    inclusive on both ends (contract section 11.1, lines 710-712).
    """
    last_index = len(TONAL_REGIONS) - 1
    for index, (low, high) in enumerate(TONAL_REGIONS):
        if index == last_index:
            if low <= frequency_hz <= high:
                return index
        elif low <= frequency_hz < high:
            return index
    return None


def region_index_of_band(band_index: int) -> int | None:
    """Canonical tonal region of a band, addressed by its centre frequency."""
    if not 0 <= band_index < GRID_BANDS:
        raise IndexError(f"band index out of range: {band_index}")
    return region_of_frequency(band_centers_hz()[band_index])
```

---

## B.31 FILE: `ml_v3/contracts/profiles.py`

**Path logico:** `ml_v3/contracts/profiles.py`  
**Bytes:** 4439  
**Lines:** 113

```python
"""Conditioning profiles, problem types and genre metadata (G1a).

Contract section 4.2 (lines 107-123): the only conditioning profiles are the
seven exposed by the host APVTS, in canonical id order 0..6; string and id must
agree or the record is rejected.

The contract also records the verified state of the current code: the host
parameter exposes exactly those seven choices (PluginProcessor.cpp:651) and
clamps ids to 0..6 (PluginProcessor.cpp:1849), so AIEngine::SourceProfile::Techno
(AIEngine.h:103-113) is unreachable from the host. Therefore the mapping
`Techno -> edm` is a FUTURE V3 adapter policy for benchmark metadata and is NOT
something the current product code performs. It lives here, in the adapter
layer, and nowhere else.

Subgenres other than the frozen legacy `Techno` alias are NOT auto-mapped:
house, breakbeat and the rest keep their metadata and still require an explicit
canonical source_profile.
"""
from __future__ import annotations

from .constants import (ANOMALY_CLASSES, CONDITIONING_PROFILES, ID_TO_PROFILE,
                        PROBLEM_TYPE_TO_ID, PROBLEM_TYPES, PROFILE_TO_ID)

__all__ = [
    "ProfileError",
    "is_canonical_profile", "profile_id", "profile_name",
    "require_profile_consistency",
    "problem_type_id", "require_problem_type_consistency",
    "anomaly_class_index",
    "LEGACY_PROFILE_ALIASES", "map_legacy_profile",
]


class ProfileError(ValueError):
    """Raised on a non-canonical profile, or a string/id disagreement."""


def is_canonical_profile(name: object) -> bool:
    return isinstance(name, str) and name in PROFILE_TO_ID


def profile_id(name: str) -> int:
    if not is_canonical_profile(name):
        raise ProfileError(
            f"non-canonical source_profile {name!r}; allowed: "
            f"{list(CONDITIONING_PROFILES)}")
    return PROFILE_TO_ID[name]


def profile_name(identifier: int) -> str:
    if not isinstance(identifier, int) or isinstance(identifier, bool) \
            or identifier not in ID_TO_PROFILE:
        raise ProfileError(f"non-canonical profile id {identifier!r}; allowed 0..6")
    return ID_TO_PROFILE[identifier]


def require_profile_consistency(name: str, identifier: int) -> None:
    """Both given: they must agree (contract line 111-112)."""
    expected = profile_id(name)
    if identifier != expected:
        raise ProfileError(
            f"profile string/id mismatch: {name!r} is id {expected}, got {identifier!r}")


def problem_type_id(name: str) -> int:
    if not isinstance(name, str) or name not in PROBLEM_TYPE_TO_ID:
        raise ProfileError(
            f"non-canonical problem type {name!r}; allowed: {list(PROBLEM_TYPES)}")
    return PROBLEM_TYPE_TO_ID[name]


def require_problem_type_consistency(name: str, identifier: int) -> None:
    """Contract line 403: an id disagreeing with the string is invalid."""
    expected = problem_type_id(name)
    if identifier != expected:
        raise ProfileError(
            f"problem type string/id mismatch: {name!r} is id {expected}, "
            f"got {identifier!r}")


def anomaly_class_index(name: str) -> int:
    """Index into the dense anomaly surfaces (contract line 459)."""
    if name not in ANOMALY_CLASSES:
        raise ProfileError(
            f"{name!r} is not a dense anomaly class; allowed: {list(ANOMALY_CLASSES)}")
    return ANOMALY_CLASSES.index(name)


# The ONLY legacy compatibility mapping frozen in G1a. It is an adapter policy
# for benchmark metadata; it does not add a host parameter and does not create
# an eighth profile.
LEGACY_PROFILE_ALIASES: dict[str, dict[str, object]] = {
    "Techno": {
        "source_profile": "edm",
        "source_profile_id": 6,
        "electronic_subgenre": "techno",
    },
}


def map_legacy_profile(legacy_name: str) -> dict[str, object]:
    """Map a legacy/internal metadata profile name to the canonical triple.

    Only `Techno` is mapped. Anything else — including other electronic
    subgenres such as house or breakbeat — is rejected: those keep their
    subgenre metadata and require an explicit canonical source_profile.
    """
    if legacy_name in LEGACY_PROFILE_ALIASES:
        return dict(LEGACY_PROFILE_ALIASES[legacy_name])
    raise ProfileError(
        f"no frozen legacy mapping for {legacy_name!r}; only "
        f"{sorted(LEGACY_PROFILE_ALIASES)} is mapped. Subgenres are metadata "
        f"and require an explicit canonical source_profile.")
```

---

## B.32 FILE: `ml_v3/contracts/coverage.py`

**Path logico:** `ml_v3/contracts/coverage.py`  
**Bytes:** 15567  
**Lines:** 361

```python
"""Calibration / measurement coverage floors (G1a).

Contract §10.4, §8.1 and §11.1 @ 6d254d0a.

The independent unit of support is ALWAYS `group_id`. Cells, events, crops,
derivatives and multiple assets of the same group increase the number of
examples but never the independent support. This module counts unique group ids
and returns a deterministic report; it never fits anything and never lowers a
floor. Insufficient support yields N/A, never PASS.

Fail-closed before floors: positive∩negative must be empty for a family;
family/profile strata must be pairwise disjoint and equal the declared sets.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable

from .constants import (ANOMALY_CLASSES, CONDITIONING_PROFILES,
                        FINAL_TEST_ELECTRONIC_MIN_FRACTION, PROFILE_ROLE_FLOORS)

__all__ = [
    "CoverageError", "CoverageCheck", "CoverageReport",
    "TONAL_FLOORS", "ANOMALY_FLOORS", "DEV_FINAL_FLOORS", "FAMILY_FLOORS",
    "check_calibration_coverage", "check_dev_final_support",
    "check_profile_role_floors", "check_final_test_electronic_share",
    "check_stratum_calibrator_allowed",
]

# --- section 10.4 table, line 611 (tonal row)
TONAL_FLOORS = {
    "actionable_groups": 50,
    "actionable_groups_with_positive_cell": 20,
    "actionable_groups_with_negative_cell": 20,
    "clean_groups": 50,
    "per_profile_actionable": 3,
    "per_profile_clean": 3,
}

# --- section 10.4 table, lines 612-614 (one identical row per anomaly class)
ANOMALY_FLOORS = {
    "positive_groups": 30,
    "negative_groups": 50,
    "negative_groups_clean": 30,
    "positive_source_families": 3,
    "positive_groups_per_family": 5,
    "max_family_share": 0.50,
    "negatives_per_profile": 3,
}

# --- section 10.4, lines 629-633
DEV_FINAL_FLOORS = {
    "tonal_actionable_groups": 30,
    "clean_groups_for_actionable_rate": 149,
    "anomaly_positive_groups": 30,
    "clean_safety_groups": 149,
}

# --- section 11.1 table, lines 695-698
FAMILY_FLOORS = {
    "tonal-controlled": {"parent_groups_per_profile": 5, "groups_per_region_direction": 10},
    "tonal-natural": {"clean_groups_per_profile": 5, "actionable_groups_per_profile": 5},
    "anomaly-natural": {"positive_groups_per_class": 30},
    "clean-safety": {"total_groups": 149, "groups_per_profile": 10,
                     "eligible_minute_per_class": 1},
    "electronic-stratified": {"final_test_fraction": FINAL_TEST_ELECTRONIC_MIN_FRACTION},
}

# A stratum-specific calibrator/threshold is forbidden below this support in
# the calibration split (section 10.4, lines 623-627).
STRATUM_CALIBRATOR_MIN_POSITIVE = 30
STRATUM_CALIBRATOR_MIN_NEGATIVE = 30


class CoverageError(ValueError):
    """Raised on malformed coverage input (never on insufficient support)."""


@dataclass(frozen=True)
class CoverageCheck:
    """One floor evaluation. `status` is PASS or N/A — never a silent PASS."""
    name: str
    required: float
    observed: float
    status: str

    @property
    def ok(self) -> bool:
        return self.status == "PASS"


@dataclass
class CoverageReport:
    checks: list[CoverageCheck] = field(default_factory=list)

    def add(self, name: str, required: float, observed: float) -> None:
        status = "PASS" if observed >= required else "N/A"
        self.checks.append(CoverageCheck(name, required, observed, status))

    @property
    def ok(self) -> bool:
        return all(check.ok for check in self.checks)

    def failures(self) -> list[CoverageCheck]:
        return [check for check in self.checks if not check.ok]

    def to_dict(self) -> dict:
        return {
            "ok": self.ok,
            "checks": [
                {"name": check.name, "required": check.required,
                 "observed": check.observed, "status": check.status}
                for check in self.checks
            ],
        }


def _unique(values: Iterable[str]) -> int:
    return len(set(values))


def _as_group_set(values: Iterable[str], field: str) -> set[str]:
    if not isinstance(values, (list, tuple, set, frozenset)):
        raise CoverageError(f"{field} must be an iterable of group_id strings")
    out: set[str] = set()
    for item in values:
        if not isinstance(item, str) or not item:
            raise CoverageError(f"{field} contains a non-string or empty group_id")
        out.add(item)
    return out


def _require_disjoint(left: set[str], right: set[str], left_name: str,
                      right_name: str) -> None:
    overlap = left & right
    if overlap:
        sample = sorted(overlap)[:5]
        raise CoverageError(
            f"{left_name} and {right_name} must be disjoint; "
            f"overlap sample={sample} count={len(overlap)}")


def _require_partition_match(declared: set[str], by_key: dict, field: str,
                             declared_name: str) -> None:
    """Fail-closed: strata are pairwise disjoint and union == declared set."""
    if not isinstance(by_key, dict):
        raise CoverageError(f"{field} must be a mapping")
    union: set[str] = set()
    for key, groups in by_key.items():
        subset = _as_group_set(groups, f"{field}[{key!r}]")
        extras = subset - declared
        if extras:
            sample = sorted(extras)[:5]
            raise CoverageError(
                f"{field}[{key!r}] contains groups not in {declared_name}: "
                f"{sample}")
        overlap = union & subset
        if overlap:
            sample = sorted(overlap)[:5]
            raise CoverageError(
                f"{field} strata are not disjoint; overlap sample={sample}")
        union |= subset
    missing = declared - union
    if missing:
        sample = sorted(missing)[:5]
        raise CoverageError(
            f"{field} is inconsistent with {declared_name}: "
            f"missing from strata sample={sample} count={len(missing)}")


def _require_tonal_profile_partition(declared: set[str], by_profile: dict,
                                     field: str, declared_name: str) -> None:
    """Tonal per-profile maps: exact 7 canonical keys + partition match."""
    if not isinstance(by_profile, dict):
        raise CoverageError(f"{field} must be a mapping")
    expected_keys = set(CONDITIONING_PROFILES)
    actual_keys = set(by_profile)
    missing_keys = expected_keys - actual_keys
    if missing_keys:
        raise CoverageError(
            f"{field} missing canonical profile keys: {sorted(missing_keys)}")
    extra_keys = actual_keys - expected_keys
    if extra_keys:
        raise CoverageError(
            f"{field} has non-canonical profile keys: {sorted(extra_keys)}")
    _require_partition_match(declared, by_profile, field, declared_name)


def check_calibration_coverage(tonal: dict, anomaly: dict) -> CoverageReport:
    """Global calibration coverage (§10.4).

    Fail-closed invariants (before floors):
      - same annotation cannot be positive and negative for one family;
      - positive_groups_by_family union == positive_groups;
      - negative_groups_by_profile union ⊆ negative_groups and covers them;
      - tonal actionable∩clean empty; pos/neg cell groups ⊆ actionable;
      - tonal per_profile_actionable / per_profile_clean: exact 7 canonical
        profile keys, pairwise disjoint, union == actionable_groups /
        clean_groups respectively.

    `tonal` keys: actionable_groups, actionable_positive_cell_groups,
    actionable_negative_cell_groups, clean_groups (iterables of group_id), plus
    per_profile_actionable / per_profile_clean mapping profile -> group ids.

    `anomaly` maps each of the three classes to a dict with: positive_groups,
    negative_groups, negative_clean_groups, positive_groups_by_family,
    negative_groups_by_profile.
    """
    if not isinstance(tonal, dict):
        raise CoverageError("tonal coverage payload must be a dict")
    if not isinstance(anomaly, dict):
        raise CoverageError("anomaly coverage payload must be a dict")

    report = CoverageReport()

    actionable = _as_group_set(tonal.get("actionable_groups", []), "tonal.actionable_groups")
    pos_cells = _as_group_set(
        tonal.get("actionable_positive_cell_groups", []),
        "tonal.actionable_positive_cell_groups")
    neg_cells = _as_group_set(
        tonal.get("actionable_negative_cell_groups", []),
        "tonal.actionable_negative_cell_groups")
    clean = _as_group_set(tonal.get("clean_groups", []), "tonal.clean_groups")
    _require_disjoint(actionable, clean, "tonal.actionable_groups", "tonal.clean_groups")
    extras = (pos_cells | neg_cells) - actionable
    if extras:
        raise CoverageError(
            "tonal positive/negative cell groups must be subsets of actionable_groups; "
            f"extras sample={sorted(extras)[:5]}")

    per_profile_actionable = tonal.get("per_profile_actionable", {})
    per_profile_clean = tonal.get("per_profile_clean", {})
    # Exact 7 canonical profile keys; pairwise disjoint; union == declared sets.
    _require_tonal_profile_partition(
        actionable, per_profile_actionable,
        "tonal.per_profile_actionable", "tonal.actionable_groups")
    _require_tonal_profile_partition(
        clean, per_profile_clean,
        "tonal.per_profile_clean", "tonal.clean_groups")

    report.add("tonal.actionable_groups", TONAL_FLOORS["actionable_groups"],
               len(actionable))
    report.add("tonal.actionable_with_positive_cell",
               TONAL_FLOORS["actionable_groups_with_positive_cell"],
               len(pos_cells))
    report.add("tonal.actionable_with_negative_cell",
               TONAL_FLOORS["actionable_groups_with_negative_cell"],
               len(neg_cells))
    report.add("tonal.clean_groups", TONAL_FLOORS["clean_groups"], len(clean))
    for profile in CONDITIONING_PROFILES:
        report.add(f"tonal.actionable[{profile}]", TONAL_FLOORS["per_profile_actionable"],
                   _unique(per_profile_actionable.get(profile, [])))
        report.add(f"tonal.clean[{profile}]", TONAL_FLOORS["per_profile_clean"],
                   _unique(per_profile_clean.get(profile, [])))

    for klass in ANOMALY_CLASSES:
        data = anomaly.get(klass, {})
        if not isinstance(data, dict):
            raise CoverageError(f"anomaly[{klass}] must be a dict")
        positives = _as_group_set(data.get("positive_groups", []),
                                  f"{klass}.positive_groups")
        negatives = _as_group_set(data.get("negative_groups", []),
                                  f"{klass}.negative_groups")
        neg_clean = _as_group_set(data.get("negative_clean_groups", []),
                                  f"{klass}.negative_clean_groups")
        # §10.4: same annotation cannot be positive and negative for one family.
        _require_disjoint(positives, negatives,
                          f"{klass}.positive_groups", f"{klass}.negative_groups")
        extras_neg_clean = neg_clean - negatives
        if extras_neg_clean:
            raise CoverageError(
                f"{klass}.negative_clean_groups must be a subset of negative_groups")

        by_family = data.get("positive_groups_by_family", {})
        _require_partition_match(
            positives, by_family, f"{klass}.positive_groups_by_family",
            f"{klass}.positive_groups")
        by_profile = data.get("negative_groups_by_profile", {})
        _require_partition_match(
            negatives, by_profile, f"{klass}.negative_groups_by_profile",
            f"{klass}.negative_groups")

        report.add(f"{klass}.positive_groups", ANOMALY_FLOORS["positive_groups"],
                   len(positives))
        report.add(f"{klass}.negative_groups", ANOMALY_FLOORS["negative_groups"],
                   len(negatives))
        report.add(f"{klass}.negative_clean_groups",
                   ANOMALY_FLOORS["negative_groups_clean"],
                   len(neg_clean))

        qualifying = {family: set(groups) for family, groups in by_family.items()
                      if len(set(groups)) >= ANOMALY_FLOORS["positive_groups_per_family"]}
        report.add(f"{klass}.positive_source_families",
                   ANOMALY_FLOORS["positive_source_families"], len(qualifying))
        if positives:
            largest = max((len(set(groups)) for groups in by_family.values()), default=0)
            share = largest / len(positives)
            report.add(f"{klass}.family_share_headroom",
                       1.0 - ANOMALY_FLOORS["max_family_share"], 1.0 - share)
        for profile in CONDITIONING_PROFILES:
            report.add(f"{klass}.negatives[{profile}]",
                       ANOMALY_FLOORS["negatives_per_profile"],
                       _unique(by_profile.get(profile, [])))
    return report


def check_dev_final_support(tonal_actionable_groups: Iterable[str],
                            clean_groups: Iterable[str],
                            anomaly_positive_groups: dict[str, Iterable[str]],
                            clean_safety_groups: Iterable[str]) -> CoverageReport:
    """development-metric / final-test minimum support (lines 629-637)."""
    report = CoverageReport()
    report.add("dev_final.tonal_actionable_groups",
               DEV_FINAL_FLOORS["tonal_actionable_groups"],
               _unique(tonal_actionable_groups))
    report.add("dev_final.clean_groups_for_actionable_rate",
               DEV_FINAL_FLOORS["clean_groups_for_actionable_rate"],
               _unique(clean_groups))
    for klass in ANOMALY_CLASSES:
        report.add(f"dev_final.{klass}.positive_groups",
                   DEV_FINAL_FLOORS["anomaly_positive_groups"],
                   _unique(anomaly_positive_groups.get(klass, [])))
    report.add("dev_final.clean_safety_groups",
               DEV_FINAL_FLOORS["clean_safety_groups"], _unique(clean_safety_groups))
    return report


def check_profile_role_floors(groups_by_profile_role: dict[str, dict[str, Iterable[str]]]
                              ) -> CoverageReport:
    """General per-profile split floors (section 8.1, lines 285-287)."""
    report = CoverageReport()
    for profile in CONDITIONING_PROFILES:
        per_role = groups_by_profile_role.get(profile, {})
        for role, floor in PROFILE_ROLE_FLOORS.items():
            report.add(f"split.{profile}.{role}", floor,
                       _unique(per_role.get(role, [])))
    return report


def check_final_test_electronic_share(final_test_groups: Iterable[str],
                                      electronic_groups: Iterable[str]) -> CoverageReport:
    """At least 40% of the whole final-test must be electronic (line 287)."""
    report = CoverageReport()
    total = set(final_test_groups)
    electronic = set(electronic_groups) & total
    share = (len(electronic) / len(total)) if total else 0.0
    report.add("final_test.electronic_share",
               FINAL_TEST_ELECTRONIC_MIN_FRACTION, share)
    return report


def check_stratum_calibrator_allowed(positive_groups: Iterable[str],
                                     negative_groups: Iterable[str]) -> CoverageReport:
    """A per-profile/domain/subgenre calibrator needs 30 positives AND 30
    negatives in the calibration split (lines 623-627); otherwise N/A."""
    report = CoverageReport()
    report.add("stratum_calibrator.positive_groups",
               STRATUM_CALIBRATOR_MIN_POSITIVE, _unique(positive_groups))
    report.add("stratum_calibrator.negative_groups",
               STRATUM_CALIBRATOR_MIN_NEGATIVE, _unique(negative_groups))
    return report
```

---

## B.33 FILE: `ml_v3/contracts/split.py`

**Path logico:** `ml_v3/contracts/split.py`  
**Bytes:** 26697  
**Lines:** 601

```python
"""Split contract v3: identity, commit-reveal, role and pilot assignment (G1a).

Transcribed from docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a §8 (minimo
byte-level) and §9.1. Nothing here is chosen by G1a.

Byte-level definitions
----------------------
commitment      SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt), salt = 32 raw
                (§8.2.4)
batch id        SHA-256 lowercase hex of canonical roster bytes, trailing LF
                included (§8.2.3)
role            HMAC-SHA256(salt, role message); first 8 bytes BE as value;
                train if value*20 < 11*2**64; validation if < 13*2**64;
                calibration if < 15*2**64; development-metric if < 17*2**64;
                else final-test (§8.2.5). admission_batch_id is the 64 ASCII
                hex bytes — never the raw 32-byte digest.
pilot           HMAC-SHA256(salt, pilot message); all 32 bytes BE as value;
                development_pilot iff role == development-metric AND
                value*4 < 2**256 (§8.2.6). Same hex-64 admission_batch_id.

Identity (§8.2.2)
-----------------
group_id is not free text:
  - with stable upstream: source_family + ":" + upstream_id
  - otherwise pack fallback: source_family + ":pack:" + SHA256(pack_bytes)
    where pack_bytes = concat of (64 ASCII digest + LF) for each lowercase
    SHA-256 sorted by ASCII bytes.
  `:` is forbidden in source_family and upstream_id; `:pack:` is reserved
  for the pack fallback form only. NUL is forbidden in every HMAC component.

source_identity_index schema
----------------------------
Exact-key envelope (additionalProperties forbidden), schema id
`aieq-v3-source-identity-index-1` (SCHEMA_IDS["source_identity_index"]):

  {
    schema,
    source_snapshot_id,
    alias_mapping_version,
    inclusion_rules_version,
    groups: [
      {
        group_id, group_primary_profile, group_primary_domain,
        source_family, upstream_id, pack_audio_sha256, aliases
      }
    ]
  }

`source_snapshot_sha256` on the roster MUST equal
SHA256(canonical_bytes(identity_index)) with that schema; free-form digests
are rejected (§8.2.3 micro-amend).
"""
from __future__ import annotations

import hashlib
import hmac
from typing import Any, Iterable

from .canonical import is_sha256_hex, sha256_of_obj
from .constants import (COMMITMENT_PREFIX, PILOT_PREFIX, PILOT_THRESHOLD_DEN,
                        PILOT_THRESHOLD_NUM, ROLE_INTERVALS_EXACT, ROLE_PREFIX,
                        SCHEMA_IDS, SPLIT_ROLES, SPLIT_SALT_BYTES)
from .profiles import ProfileError, is_canonical_profile

__all__ = [
    "SplitError",
    "reject_nul", "reject_colon", "require_admission_batch_id_hex",
    "pack_group_id", "upstream_group_id", "derive_group_id",
    "validate_identity_index", "source_snapshot_sha256",
    "exact_projection", "roster_from_identity_index", "canonical_roster",
    "verify_roster_source_snapshot", "admission_batch_id",
    "salt_commitment", "verify_commitment",
    "assign_role", "assign_pilot", "assign_batch",
    "validate_manifest_split_invariants",
]

NUL = b"\x00"

# Exact-key sets for aieq-v3-source-identity-index-1 (additionalProperties fail).
_IDENTITY_ROW_KEYS = frozenset({
    "group_id", "group_primary_profile", "group_primary_domain",
    "source_family", "upstream_id", "pack_audio_sha256", "aliases",
})
_IDENTITY_ENVELOPE_KEYS = frozenset({
    "schema", "source_snapshot_id", "alias_mapping_version",
    "inclusion_rules_version", "groups",
})
_ROSTER_ROW_KEYS = ("group_id", "group_primary_profile",
                    "group_primary_domain", "source_family")


class SplitError(ValueError):
    """Raised on any violation of the split contract. Never self-corrects."""


# ------------------------------------------------------------------ hygiene
def reject_nul(value: str, field: str) -> str:
    """NUL is the separator of every commitment/HMAC message: forbid it."""
    if not isinstance(value, str):
        raise SplitError(f"{field} must be a string, got {type(value).__name__}")
    if "\x00" in value:
        raise SplitError(f"{field} contains U+0000, which is the message separator")
    return value


def reject_colon(value: str, field: str) -> str:
    """`:` is forbidden in source_family and upstream_id (§8.2.2)."""
    if ":" in value:
        raise SplitError(
            f"{field} must not contain ':'; reserved for group_id grammar "
            f"(got {value!r})")
    return value


def _require_nonempty(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise SplitError(f"{field} must be a non-empty string")
    return reject_nul(value, field)


def _require_family_or_upstream(value: object, field: str) -> str:
    """Non-empty, no NUL, no colon — identity component hygiene (§8.2.2)."""
    return reject_colon(_require_nonempty(value, field), field)


def require_admission_batch_id_hex(batch_id: str) -> str:
    """admission_batch_id MUST be lowercase hex-64 ASCII before HMAC (§8.2.5)."""
    reject_nul(batch_id, "admission_batch_id")
    if not is_sha256_hex(batch_id):
        raise SplitError(
            "admission_batch_id must be lowercase hex-64 ASCII "
            "(never raw-32 digest bytes)")
    return batch_id


# ----------------------------------------------------------------- identity
def pack_group_id(source_family: str, audio_sha256: Iterable[str]) -> str:
    """`source_family + ":pack:" + SHA256(sorted pack digests + LF)`.

    `:pack:` is reserved for this fallback only (§8.2.2).
    """
    source_family = _require_family_or_upstream(source_family, "source_family")
    digests = list(audio_sha256)
    if not digests:
        raise SplitError("pack group requires a non-empty pack_audio_sha256 list")
    for digest in digests:
        if not is_sha256_hex(digest):
            raise SplitError(f"pack_audio_sha256 entry is not a sha256 hex: {digest!r}")
    if len(set(digests)) != len(digests):
        raise SplitError("pack_audio_sha256 contains duplicates")
    if digests != sorted(digests):
        raise SplitError("pack_audio_sha256 must be sorted by ascii bytes")
    payload = b"".join(digest.encode("ascii") + b"\n" for digest in digests)
    return f"{source_family}:pack:{hashlib.sha256(payload).hexdigest()}"


def upstream_group_id(source_family: str, upstream_id: str) -> str:
    source_family = _require_family_or_upstream(source_family, "source_family")
    upstream_id = _require_family_or_upstream(upstream_id, "upstream_id")
    # upstream_id must not imitate the reserved pack fallback form.
    if upstream_id.startswith("pack:"):
        raise SplitError(
            "upstream_id must not start with 'pack:'; ':pack:' is reserved "
            "for the pack fallback group_id form")
    return f"{source_family}:{upstream_id}"


def derive_group_id(row: dict) -> str:
    """Reconstruct the canonical group_id from an identity-index row."""
    source_family = _require_family_or_upstream(row.get("source_family"), "source_family")
    upstream_id = row.get("upstream_id")
    pack = row.get("pack_audio_sha256")
    if not isinstance(pack, list):
        raise SplitError("pack_audio_sha256 must be a list (possibly empty)")
    if upstream_id is None:
        if not pack:
            raise SplitError(
                "identity is not reconstructible: upstream_id is null and "
                "pack_audio_sha256 is empty")
        return pack_group_id(source_family, pack)
    if pack:
        raise SplitError(
            "ambiguous identity: upstream_id and pack_audio_sha256 both present")
    return upstream_group_id(
        source_family, _require_family_or_upstream(upstream_id, "upstream_id"))


def validate_identity_index(index: Any) -> list[dict]:
    """Fail-closed validation of aieq-v3-source-identity-index-1.

    Exact-key envelope and rows (additionalProperties forbidden). Rejects
    unknown/missing keys, NUL, colon in family/upstream, duplicate group ids,
    duplicate/cross-owned aliases, unsorted lists and any row whose declared
    group_id is not reconstructible from its source.
    """
    if not isinstance(index, dict):
        raise SplitError("identity index must be a JSON object")
    extra = set(index) - _IDENTITY_ENVELOPE_KEYS
    if extra:
        raise SplitError(f"unknown identity index fields: {sorted(extra)}")
    missing = _IDENTITY_ENVELOPE_KEYS - set(index)
    if missing:
        raise SplitError(f"missing identity index fields: {sorted(missing)}")
    if index["schema"] != SCHEMA_IDS["source_identity_index"]:
        raise SplitError(f"unexpected identity index schema: {index['schema']!r}")
    for field in ("source_snapshot_id", "alias_mapping_version",
                  "inclusion_rules_version"):
        _require_nonempty(index[field], field)

    groups = index["groups"]
    if not isinstance(groups, list) or not groups:
        raise SplitError("identity index groups must be a non-empty list")

    seen_groups: set[str] = set()
    alias_owner: dict[str, str] = {}
    rows: list[dict] = []
    for position, row in enumerate(groups):
        if not isinstance(row, dict):
            raise SplitError(f"identity row {position} is not an object")
        extra = set(row) - _IDENTITY_ROW_KEYS
        if extra:
            raise SplitError(f"unknown identity row fields at {position}: {sorted(extra)}")
        missing = _IDENTITY_ROW_KEYS - set(row)
        if missing:
            raise SplitError(f"missing identity row fields at {position}: {sorted(missing)}")

        group_id = _require_nonempty(row["group_id"], "group_id")
        _require_nonempty(row["group_primary_domain"], "group_primary_domain")
        _require_family_or_upstream(row["source_family"], "source_family")
        if row["upstream_id"] is not None:
            _require_family_or_upstream(row["upstream_id"], "upstream_id")
        if not is_canonical_profile(row["group_primary_profile"]):
            raise ProfileError(
                f"non-canonical group_primary_profile at {position}: "
                f"{row['group_primary_profile']!r}")

        derived = derive_group_id(row)
        if derived != group_id:
            raise SplitError(
                f"group_id is not reconstructible at {position}: declared "
                f"{group_id!r}, derived {derived!r}")
        if group_id in seen_groups:
            raise SplitError(f"duplicate group_id in identity index: {group_id!r}")
        seen_groups.add(group_id)

        aliases = row["aliases"]
        if not isinstance(aliases, list):
            raise SplitError(f"aliases must be a list at {position}")
        for alias in aliases:
            _require_nonempty(alias, "alias")
        if len(set(aliases)) != len(aliases):
            raise SplitError(f"duplicate alias inside group {group_id!r}")
        if list(aliases) != sorted(aliases, key=lambda item: item.encode("utf-8")):
            raise SplitError(f"aliases must be sorted by utf-8 bytes in {group_id!r}")
        for alias in aliases:
            owner = alias_owner.get(alias)
            if owner is not None and owner != group_id:
                raise SplitError(
                    f"alias {alias!r} claimed by both {owner!r} and {group_id!r}")
            alias_owner[alias] = group_id
        rows.append(row)

    ordered = sorted(rows, key=lambda item: item["group_id"].encode("utf-8"))
    if [item["group_id"] for item in rows] != [item["group_id"] for item in ordered]:
        raise SplitError("identity index groups must be sorted by utf-8 group_id bytes")
    return rows


def source_snapshot_sha256(identity_index: Any) -> str:
    """SHA-256 of canonical_bytes(identity_index); schema must match (§8.2.3).

    Free-form digests are not admitted: callers must derive this from a
    validated aieq-v3-source-identity-index-1 document.
    """
    validate_identity_index(identity_index)
    return sha256_of_obj(identity_index)


def exact_projection(rows: list[dict]) -> list[dict]:
    """Project validated identity rows onto roster group rows.

    Returns the exact `groups` list required on an admission roster:
    keys `_ROSTER_ROW_KEYS` only, sorted by utf-8 `group_id` bytes.
    Used as: `roster["groups"] == exact_projection(validate_identity_index(index))`.
    """
    if not isinstance(rows, list) or not rows:
        raise SplitError("exact_projection requires a non-empty validated row list")
    return [
        {key: row[key] for key in _ROSTER_ROW_KEYS}
        for row in sorted(rows, key=lambda item: item["group_id"].encode("utf-8"))
    ]


def roster_from_identity_index(index: Any) -> dict:
    """Project the validated identity index onto the pre-split roster.

    Sets source_snapshot_sha256 = SHA256(canonical_bytes(index)) — never a
    free-form digest (§8.2.3).
    """
    rows = validate_identity_index(index)
    return {
        "schema": SCHEMA_IDS["admission_roster"],
        "source_snapshot_id": index["source_snapshot_id"],
        "source_snapshot_sha256": sha256_of_obj(index),
        "groups": exact_projection(rows),
    }


def verify_roster_source_snapshot(roster: Any, identity_index: Any) -> None:
    """Bind roster to identity index: schema hash, snapshot id, full projection.

    Fail-closed checks (all mandatory):
      - identity index schema / reconstructibility via validate_identity_index;
      - source_snapshot_sha256 == SHA256(canonical_bytes(index));
      - source_snapshot_id equality;
      - roster.groups == exact_projection(validate_identity_index(index))
        (exact keys, values, canonical order).
    Free-form digests, removed/added groups, field drift, spurious `:pack:`
    rows and snapshot mismatch all raise SplitError.
    """
    if not isinstance(roster, dict):
        raise SplitError("roster must be a JSON object")
    rows = validate_identity_index(identity_index)
    expected = sha256_of_obj(identity_index)
    claimed = roster.get("source_snapshot_sha256")
    if not is_sha256_hex(claimed):
        raise SplitError("source_snapshot_sha256 is not a sha256 hex digest")
    if not hmac.compare_digest(claimed, expected):
        raise SplitError(
            "source_snapshot_sha256 does not equal "
            "SHA256(canonical_bytes(source_identity_index)); free-form rejected")
    if roster.get("source_snapshot_id") != identity_index.get("source_snapshot_id"):
        raise SplitError("roster source_snapshot_id does not match identity index")
    expected_groups = exact_projection(rows)
    actual_groups = roster.get("groups")
    if actual_groups != expected_groups:
        raise SplitError(
            "roster.groups is not the exact projection of the identity index "
            "(keys, values and canonical group_id order must match)")


def canonical_roster(roster: Any, identity_index: Any | None = None) -> dict:
    """Construction / canonicalize-on-create helper for the pre-split roster.

    Exact-key envelope schema aieq-v3-admission-roster-1. Sorts `groups` by
    utf-8 group_id bytes for newly built artifacts. When `identity_index` is
    supplied, also runs verify_roster_source_snapshot (hash + full projection).

    NOTE (M3): this helper may reorder groups on create. Validation of a
    *committed* artifact that must reject non-canonical on-disk order/bytes
    belongs in a separate fail-closed reader — do not treat sorted output here
    as proof that a stored file was already canonical.
    """
    if not isinstance(roster, dict):
        raise SplitError("roster must be a JSON object")
    allowed = {"schema", "source_snapshot_id", "source_snapshot_sha256", "groups"}
    extra = set(roster) - allowed
    if extra:
        raise SplitError(f"unknown roster fields: {sorted(extra)}")
    missing = allowed - set(roster)
    if missing:
        raise SplitError(f"missing roster fields: {sorted(missing)}")
    if roster["schema"] != SCHEMA_IDS["admission_roster"]:
        raise SplitError(f"unexpected roster schema: {roster['schema']!r}")
    _require_nonempty(roster["source_snapshot_id"], "source_snapshot_id")
    if not is_sha256_hex(roster["source_snapshot_sha256"]):
        raise SplitError("source_snapshot_sha256 is not a sha256 hex digest")

    groups = roster["groups"]
    if not isinstance(groups, list) or not groups:
        raise SplitError("roster groups must be a non-empty list")
    seen: set[str] = set()
    for position, row in enumerate(groups):
        if not isinstance(row, dict):
            raise SplitError(f"roster row {position} is not an object")
        if set(row) != set(_ROSTER_ROW_KEYS):
            raise SplitError(
                f"roster row {position} must have exactly {list(_ROSTER_ROW_KEYS)}, "
                f"got {sorted(row)}")
        if "split_role" in row or "development_pilot" in row:
            raise SplitError("the pre-split roster must not carry roles")
        group_id = _require_nonempty(row["group_id"], "group_id")
        _require_nonempty(row["group_primary_domain"], "group_primary_domain")
        _require_family_or_upstream(row["source_family"], "source_family")
        if not is_canonical_profile(row["group_primary_profile"]):
            raise ProfileError(
                f"non-canonical group_primary_profile at {position}: "
                f"{row['group_primary_profile']!r}")
        if group_id in seen:
            raise SplitError(f"duplicate group_id in roster: {group_id!r}")
        seen.add(group_id)

    ordered = sorted(groups, key=lambda item: item["group_id"].encode("utf-8"))
    normalized = dict(roster)
    normalized["groups"] = ordered
    if identity_index is not None:
        verify_roster_source_snapshot(normalized, identity_index)
    return normalized


def admission_batch_id(roster: Any) -> str:
    """SHA-256 of the canonical JSON bytes of the validated roster envelope."""
    return sha256_of_obj(canonical_roster(roster))


# ------------------------------------------------------------ commit-reveal
def salt_commitment(salt: bytes) -> str:
    if not isinstance(salt, (bytes, bytearray)):
        raise SplitError("salt must be raw bytes")
    if len(salt) != SPLIT_SALT_BYTES:
        raise SplitError(
            f"salt must be exactly {SPLIT_SALT_BYTES} bytes, got {len(salt)}")
    return hashlib.sha256(COMMITMENT_PREFIX + NUL + bytes(salt)).hexdigest()


def verify_commitment(salt: bytes, commitment_hex: str) -> None:
    if not is_sha256_hex(commitment_hex):
        raise SplitError("commitment is not a sha256 hex digest")
    actual = salt_commitment(salt)
    if not hmac.compare_digest(actual, commitment_hex):
        raise SplitError("commitment does not match the revealed salt")


# -------------------------------------------------------- role / pilot draw
def _role_message(batch_id: str, row: dict) -> bytes:
    batch_id = require_admission_batch_id_hex(batch_id)
    return (ROLE_PREFIX + NUL
            + batch_id.encode("ascii") + NUL
            + reject_nul(row["group_primary_profile"], "group_primary_profile").encode("utf-8") + NUL
            + reject_nul(row["group_primary_domain"], "group_primary_domain").encode("utf-8") + NUL
            + reject_colon(reject_nul(row["source_family"], "source_family"),
                           "source_family").encode("utf-8") + NUL
            + reject_nul(row["group_id"], "group_id").encode("utf-8"))


def assign_role(salt: bytes, batch_id: str, row: dict) -> str:
    """Deterministic role draw. Exact integer comparison per §8.2.5.

    Uses value * 20 < numerator * 2**64 — never a pre-floored threshold
    (numerator * 2**64 // 20), which truncates and shifts boundaries.
    """
    if len(salt) != SPLIT_SALT_BYTES:
        raise SplitError(f"salt must be exactly {SPLIT_SALT_BYTES} bytes")
    digest = hmac.new(bytes(salt), _role_message(batch_id, row), hashlib.sha256).digest()
    value = int.from_bytes(digest[:8], "big", signed=False)
    two64 = 2 ** 64
    for role, numerator in ROLE_INTERVALS_EXACT:
        if value * 20 < numerator * two64:
            return role
    # Unreachable: final-test has numerator 20, so value * 20 < 20 * 2**64
    # covers every uint64 value (max value is 2**64 - 1).
    raise SplitError("role assignment exhausted all intervals")  # pragma: no cover


def _require_salt(salt: object) -> bytes:
    if not isinstance(salt, (bytes, bytearray)):
        raise SplitError("salt must be raw bytes")
    if len(salt) != SPLIT_SALT_BYTES:
        raise SplitError(
            f"salt must be exactly {SPLIT_SALT_BYTES} bytes, got {len(salt)}")
    return bytes(salt)


def assign_pilot(salt: bytes, batch_id: str, group_id: str, role: str) -> bool:
    """Pilot flag; only development-metric groups may be true (§8.2.6).

    Fail-closed validation runs BEFORE the non-development early return:
    salt type/length, batch_id hex-64, group_id NUL hygiene, role membership.
    """
    salt = _require_salt(salt)
    batch_id = require_admission_batch_id_hex(batch_id)
    group_id = reject_nul(group_id, "group_id")
    if role not in SPLIT_ROLES:
        raise SplitError(f"unknown split role for pilot: {role!r}")
    if role != "development-metric":
        return False
    message = (PILOT_PREFIX + NUL
               + batch_id.encode("ascii") + NUL
               + group_id.encode("utf-8"))
    digest = hmac.new(salt, message, hashlib.sha256).digest()
    value = int.from_bytes(digest, "big", signed=False)
    # value / 2**256 < 1/4  <=>  value * 4 < 2**256
    return value * PILOT_THRESHOLD_DEN < PILOT_THRESHOLD_NUM * (2 ** 256)


def assign_batch(salt: bytes, roster: Any, commitment_hex: str, *,
                 identity_index: Any) -> dict:
    """Full reveal step: identity-bound roster, then roles and pilots.

    Normative admission path: `identity_index` is a required keyword argument.
    `None` is rejected. Before any role/pilot draw this verifies the commitment
    (§8.2.4), the identity index schema, source_snapshot_id,
    source_snapshot_sha256 and the full roster projection (§8.2.2 / §8.2.3).
    """
    if identity_index is None:
        raise SplitError(
            "identity_index is mandatory on the normative admission path")
    verify_commitment(salt, commitment_hex)
    validated = canonical_roster(roster, identity_index=identity_index)
    batch_id = admission_batch_id(validated)
    assignments = []
    for row in validated["groups"]:
        role = assign_role(salt, batch_id, row)
        assignments.append({
            "group_id": row["group_id"],
            "split_role": role,
            "development_pilot": assign_pilot(salt, batch_id, row["group_id"], role),
        })
    return {"admission_batch_id": batch_id, "assignments": assignments}


# --------------------------------------------------- global split invariants
def validate_manifest_split_invariants(assets: list[dict],
                                       assignments: dict[str, str] | None = None
                                       ) -> None:
    """Global fail-closed checks over a manifest asset list.

    §8.2.7 / §13 gate 8:
      - one group_id in exactly one role;
      - one audio SHA in exactly one role;
      - parent and every derivative share the parent's role;
      - parent_asset_id must exist unless the asset is an explicit root;
      - no parent cycles;
      - asset_id and relative_path unique;
      - roles must match the recorded assignments when supplied.
    """
    if not isinstance(assets, list) or not assets:
        raise SplitError("assets must be a non-empty list")

    group_role: dict[str, str] = {}
    sha_role: dict[str, str] = {}
    by_id: dict[str, dict] = {}
    seen_paths: set[str] = set()

    for asset in assets:
        asset_id = _require_nonempty(asset.get("asset_id"), "asset_id")
        if asset_id in by_id:
            raise SplitError(f"duplicate asset_id: {asset_id!r}")
        by_id[asset_id] = asset

        relative_path = _require_nonempty(asset.get("relative_path"), "relative_path")
        if relative_path in seen_paths:
            raise SplitError(f"duplicate relative_path: {relative_path!r}")
        seen_paths.add(relative_path)

        role = asset.get("split_role")
        if role not in SPLIT_ROLES:
            raise SplitError(f"unknown split_role {role!r} on {asset_id!r}")
        group_id = _require_nonempty(asset.get("group_id"), "group_id")
        digest = asset.get("sha256")
        if not is_sha256_hex(digest):
            raise SplitError(f"invalid sha256 on {asset_id!r}")

        previous = group_role.setdefault(group_id, role)
        if previous != role:
            raise SplitError(
                f"group {group_id!r} appears in two roles: {previous!r} and {role!r}")
        previous_sha_role = sha_role.setdefault(digest, role)
        if previous_sha_role != role:
            raise SplitError(
                f"audio sha {digest} appears in two roles: "
                f"{previous_sha_role!r} and {role!r}")

        if assignments is not None:
            expected = assignments.get(group_id)
            if expected is None:
                raise SplitError(f"group {group_id!r} has no recorded assignment")
            if expected != role:
                raise SplitError(
                    f"group {group_id!r} role {role!r} contradicts the recorded "
                    f"assignment {expected!r}")

        if asset.get("development_pilot") and role != "development-metric":
            raise SplitError(
                f"development_pilot is true on {asset_id!r} outside development-metric")

    for asset_id, asset in by_id.items():
        parent_id = asset.get("parent_asset_id")
        if parent_id is None:
            continue
        if parent_id not in by_id:
            raise SplitError(
                f"parent_asset_id {parent_id!r} of {asset_id!r} does not exist")
        parent = by_id[parent_id]
        if parent["split_role"] != asset["split_role"]:
            raise SplitError(
                f"derivative {asset_id!r} role {asset['split_role']!r} differs from "
                f"parent {parent_id!r} role {parent['split_role']!r}")
        if parent["group_id"] != asset["group_id"]:
            raise SplitError(
                f"derivative {asset_id!r} does not inherit the parent group_id")

        seen_chain = {asset_id}
        cursor = parent_id
        while cursor is not None:
            if cursor in seen_chain:
                raise SplitError(f"parent cycle detected at {cursor!r}")
            seen_chain.add(cursor)
            cursor = by_id[cursor].get("parent_asset_id")
            if cursor is not None and cursor not in by_id:
                raise SplitError(f"parent_asset_id {cursor!r} does not exist")
```

---

## B.34 FILE: `ml_v3/contracts/schemas.py`

**Path logico:** `ml_v3/contracts/schemas.py`  
**Bytes:** 26193  
**Lines:** 684

```python
"""Declarative JSON Schema envelopes for Motore v3 G1a T2 (§14.1).

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a.
These dicts document exact-key envelopes (additionalProperties=false) and
canonical enums. Enforcement is fail-closed in ``validate.py`` (stdlib);
no third-party ``jsonschema`` dependency is introduced.

Packaging note (A15): unless a contract section literally tables a key list
(e.g. asset-manifest §9.1), T2 envelopes are **G1a T2 freeze-from-prose** —
implementative packaging of contract prose, not a claim that the JSON key
set is letterally tabulated in the contract.

The six §14 schemas (not identity-index / roster / feature-frame):
  asset-manifest, admission-batch, annotation, prediction,
  calibration-policy, benchmark-power-plan.
"""
from __future__ import annotations

from typing import Any, Final

from .constants import (
    ACCEPTED_SAMPLE_RATES,
    ANOMALY_CLASSES,
    BENCHMARK_FAMILIES,
    CONDITIONING_PROFILES,
    GRID_BANDS,
    LICENSE_CLASSES,
    PROBLEM_TYPES,
    SCHEMA_IDS,
    SPLIT_ROLES,
    TONAL_CURVE_MAX_DB,
    TONAL_CURVE_MIN_DB,
)

__all__ = [
    "ASSET_MANIFEST_KEYS",
    "ADMISSION_BATCH_KEYS",
    "ANNOTATION_KEYS",
    "PREDICTION_KEYS",
    "CALIBRATION_POLICY_KEYS",
    "BENCHMARK_POWER_PLAN_KEYS",
    "SEMANTIC_REGION_KEYS",
    "DYNAMIC_EVENT_KEYS",
    "SEMANTIC_BUNDLE_KEYS",
    "PREDICTION_EVENT_KEYS",
    "ANOMALY_REF_KEYS",
    "CALIBRATOR_KEYS",
    "POWER_FAMILY_KEYS",
    "POWER_GATE_KEYS",
    "SCORE_KNOT_KEYS",
    "SCHEMA_REGISTRY",
    "SCHEMA_REGISTRY_ARTIFACT_ID",
    "SCHEMA_REGISTRY_RELPATH",
    "frozen_schema_registry",
    "schema_registry_sha256",
    "schema_for",
    "schema_ids_t2",
]

# --- exact-key frozensets (additionalProperties forbidden) -----------------
# Asset-manifest keys are letterally listed in §9.1. Admission-batch and
# benchmark-power-plan key sets are G1a T2 freeze-from-prose packaging of
# §9.1 / §11.2 prose (not letterally tabled JSON schemas in the contract).

ASSET_MANIFEST_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "asset_id", "relative_path", "sha256", "group_id",
    "admission_batch_id", "split_role", "benchmark_families",
    "development_pilot", "source_profile", "primary_domain",
    "group_primary_profile", "group_primary_domain", "source_family",
    "electronic_subgenre", "sample_rate", "channels", "duration_s",
    "parent_asset_id", "derivative_kind", "license_class", "license_url",
    "attribution", "ledger_id",
})

# Freeze-from-prose of §9.1 admission-batch contents (source snapshot,
# inclusion rules, roster SHA-256, salt commitment/reveal, roster commit,
# reviewer, admitted/rejected). Does NOT include alias_mapping_version —
# that key belongs to the identity-index envelope (§8), not §9.1 batch prose.
ADMISSION_BATCH_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "admission_batch_id", "source_snapshot_id",
    "source_snapshot_sha256", "inclusion_rules_version",
    "roster_sha256", "salt_commitment", "salt_reveal", "roster_commit",
    "reviewer_id", "status",
})

ANNOTATION_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "asset_id", "annotator_id", "pass_id", "profile",
    "evaluation_unit_id", "segment_start_s", "segment_end_s",
    "tonal_correction_db", "tonal_confidence", "tonal_actionable_mask",
    "semantic_regions", "dynamic_events", "complete_types",
    "explicit_negative_types", "global_actionable", "clean_for_action",
    "notes", "tool_version",
})

SEMANTIC_REGION_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s",
    "band_lo_hz", "band_hi_hz", "direction", "severity", "confidence",
    "actionable",
})

DYNAMIC_EVENT_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s", "center_hz",
    "width_octaves", "severity", "confidence", "actionable",
})

PREDICTION_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "asset_id", "model_id", "model_sha256",
    "frontend_contract_sha256", "calibration_policy_id",
    "calibration_policy_sha256", "profile", "tonal_curve_db",
    "tonal_score", "tonal_confidence", "segment_start_s", "segment_end_s",
    "semantic_bundles", "events", "anomaly_score_ref",
    "anomaly_severity_ref",
})

SEMANTIC_BUNDLE_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s",
    "band_lo_hz", "band_hi_hz", "confidence", "actionable",
})

PREDICTION_EVENT_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s", "center_hz",
    "width_octaves", "severity", "confidence", "actionable",
})

ANOMALY_REF_KEYS: Final[frozenset[str]] = frozenset({
    "relative_path", "sha256", "num_feature_frames", "dtype",
})

CALIBRATION_POLICY_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "policy_id", "policy_version", "model_sha256",
    "frontend_sha256", "calibration_manifest_sha256",
    "prediction_schema_id", "prediction_schema_sha256",
    "tonal_calibrator", "anomaly_calibrator", "tonal_band_thresholds",
    "semantic_type_thresholds", "anomaly_class_thresholds",
    "candidate_extractor_version", "score_to_confidence",
    "region_to_bundle", "threshold_to_actionable",
})

CALIBRATOR_KEYS: Final[frozenset[str]] = frozenset({
    "algorithm", "parameters",
})

SCORE_KNOT_KEYS: Final[frozenset[str]] = frozenset({
    "score", "confidence",
})

# Freeze-from-prose packaging of §11.2 power-plan contents (seed, pilot
# SHA-256, statistic/orientation/min effect, m, alpha_plan, support,
# result, implementation version). Nested families[]/gates[] and plan_id
# are T2 implementative structure, not a letterally tabled JSON schema.
BENCHMARK_POWER_PLAN_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "plan_id", "implementation_version", "seed_base",
    "pilot_sha256", "power_source_role", "m", "alpha_plan",
    "families", "gates", "support", "result",
})

POWER_FAMILY_KEYS: Final[frozenset[str]] = frozenset({
    "family_id", "floor_contractual", "n_power", "n_required", "unit",
})

POWER_GATE_KEYS: Final[frozenset[str]] = frozenset({
    "metric_id", "statistic", "orientation", "min_effect_size",
    "n_power", "n_required", "support",
})


def _enum(values: tuple[Any, ...]) -> dict[str, Any]:
    return {"type": "string", "enum": list(values)}


def _sha256_prop() -> dict[str, Any]:
    return {
        "type": "string",
        "pattern": "^[0-9a-f]{64}$",
        "description": "lowercase SHA-256 hex digest",
    }


def _float120(description: str, minimum: float | None = None,
              maximum: float | None = None) -> dict[str, Any]:
    item: dict[str, Any] = {"type": "number"}
    if minimum is not None:
        item["minimum"] = minimum
    if maximum is not None:
        item["maximum"] = maximum
    return {
        "type": "array",
        "minItems": GRID_BANDS,
        "maxItems": GRID_BANDS,
        "items": item,
        "description": description,
    }


def _bool120(description: str) -> dict[str, Any]:
    return {
        "type": "array",
        "minItems": GRID_BANDS,
        "maxItems": GRID_BANDS,
        "items": {"type": "boolean"},
        "description": description,
    }


# --- six JSON Schema documents --------------------------------------------

ASSET_MANIFEST_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["asset_manifest"],
    "title": "aieq-v3-asset-manifest-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ASSET_MANIFEST_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["asset_manifest"]},
        "asset_id": {"type": "string", "minLength": 1},
        "relative_path": {"type": "string", "minLength": 1},
        "sha256": _sha256_prop(),
        "group_id": {"type": "string", "minLength": 1},
        "admission_batch_id": _sha256_prop(),
        "split_role": _enum(SPLIT_ROLES),
        "benchmark_families": {
            "type": "array",
            "uniqueItems": True,
            "items": _enum(BENCHMARK_FAMILIES),
        },
        "development_pilot": {"type": "boolean"},
        "source_profile": _enum(CONDITIONING_PROFILES),
        "primary_domain": {"type": "string", "minLength": 1},
        "group_primary_profile": _enum(CONDITIONING_PROFILES),
        "group_primary_domain": {"type": "string", "minLength": 1},
        "source_family": {"type": "string", "minLength": 1},
        "electronic_subgenre": {"type": ["string", "null"]},
        "sample_rate": {"type": "integer", "enum": list(ACCEPTED_SAMPLE_RATES)},
        "channels": {"type": "integer", "minimum": 1},
        "duration_s": {"type": "number", "exclusiveMinimum": 0},
        "parent_asset_id": {"type": ["string", "null"]},
        "derivative_kind": {"type": ["string", "null"]},
        "license_class": _enum(LICENSE_CLASSES),
        "license_url": {"type": "string"},
        "attribution": {"type": "string"},
        "ledger_id": {"type": "string"},
    },
    "description": (
        "§9.1 asset-manifest keys (letterally tabled). development_pilot "
        "may be true only when split_role == development-metric."
    ),
}

ADMISSION_BATCH_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["admission_batch"],
    "title": "aieq-v3-admission-batch-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ADMISSION_BATCH_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["admission_batch"]},
        "admission_batch_id": _sha256_prop(),
        "source_snapshot_id": {"type": "string", "minLength": 1},
        "source_snapshot_sha256": _sha256_prop(),
        "inclusion_rules_version": {"type": "string", "minLength": 1},
        "roster_sha256": _sha256_prop(),
        "salt_commitment": _sha256_prop(),
        "salt_reveal": {
            "type": ["string", "null"],
            "description": "null pre-reveal; lowercase hex of 32 raw salt bytes",
        },
        "roster_commit": {"type": "string", "minLength": 1},
        "reviewer_id": {"type": "string", "minLength": 1},
        "status": {"type": "string", "enum": ["admitted", "rejected"]},
    },
    "description": (
        "G1a T2 freeze-from-prose packaging of §9.1 admission-batch prose "
        "(not a letterally tabled JSON key list). roster_sha256 MUST equal "
        "admission_batch_id (SHA-256 of canonical roster bytes)."
    ),
}

_SEMANTIC_REGION_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(SEMANTIC_REGION_KEYS),
    "properties": {
        "problem_type": _enum(PROBLEM_TYPES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "band_lo_hz": {"type": ["number", "null"]},
        "band_hi_hz": {"type": ["number", "null"]},
        "direction": {"type": ["string", "null"]},
        "severity": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

_DYNAMIC_EVENT_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(DYNAMIC_EVENT_KEYS),
    "properties": {
        "problem_type": _enum(ANOMALY_CLASSES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "center_hz": {"type": "number", "minimum": 20, "maximum": 20000},
        "width_octaves": {"type": "number", "exclusiveMinimum": 0},
        "severity": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

ANNOTATION_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["annotation"],
    "title": "aieq-v3-annotation-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ANNOTATION_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["annotation"]},
        "asset_id": {"type": "string", "minLength": 1},
        "annotator_id": {"type": "string", "minLength": 1},
        "pass_id": {"type": "string", "minLength": 1},
        "profile": _enum(CONDITIONING_PROFILES),
        "evaluation_unit_id": {"type": "string", "minLength": 1},
        "segment_start_s": {"type": "number", "minimum": 0},
        "segment_end_s": {"type": "number", "exclusiveMinimum": 0},
        "tonal_correction_db": _float120(
            "EQ correttiva desiderata", TONAL_CURVE_MIN_DB, TONAL_CURVE_MAX_DB),
        "tonal_confidence": _float120("per-band confidence", 0.0, 1.0),
        "tonal_actionable_mask": _bool120("per-band actionable mask"),
        "semantic_regions": {"type": "array", "items": _SEMANTIC_REGION_SCHEMA},
        "dynamic_events": {"type": "array", "items": _DYNAMIC_EVENT_SCHEMA},
        "complete_types": {
            "type": "array",
            "uniqueItems": True,
            "items": _enum(PROBLEM_TYPES),
        },
        "explicit_negative_types": {
            "type": "array",
            "uniqueItems": True,
            "items": _enum(PROBLEM_TYPES),
        },
        "global_actionable": {"type": "boolean"},
        "clean_for_action": {"type": "boolean"},
        "notes": {"type": "string"},
        "tool_version": {"type": "string", "minLength": 1},
    },
}

_ANOMALY_REF_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ANOMALY_REF_KEYS),
    "properties": {
        "relative_path": {"type": "string", "minLength": 1},
        "sha256": _sha256_prop(),
        "num_feature_frames": {"type": "integer", "minimum": 1},
        "dtype": {"const": "float32_le"},
    },
    "description": (
        "SHA-256 ref to little-endian float32 array shape "
        "[num_feature_frames, 3, 120], class order Resonance/Harshness/Sibilance."
    ),
}

_SEMANTIC_BUNDLE_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(SEMANTIC_BUNDLE_KEYS),
    "properties": {
        "problem_type": _enum(PROBLEM_TYPES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "band_lo_hz": {"type": ["number", "null"]},
        "band_hi_hz": {"type": ["number", "null"]},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

_PREDICTION_EVENT_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(PREDICTION_EVENT_KEYS),
    "properties": {
        "problem_type": _enum(ANOMALY_CLASSES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "center_hz": {"type": "number", "minimum": 20, "maximum": 20000},
        "width_octaves": {"type": "number", "exclusiveMinimum": 0},
        "severity": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

PREDICTION_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["prediction"],
    "title": "aieq-v3-prediction-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(PREDICTION_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["prediction"]},
        "asset_id": {"type": "string", "minLength": 1},
        "model_id": {"type": "string", "minLength": 1},
        "model_sha256": _sha256_prop(),
        "frontend_contract_sha256": _sha256_prop(),
        "calibration_policy_id": {"type": "string", "minLength": 1},
        "calibration_policy_sha256": _sha256_prop(),
        "profile": _enum(CONDITIONING_PROFILES),
        "tonal_curve_db": _float120(
            "public tonal curve", TONAL_CURVE_MIN_DB, TONAL_CURVE_MAX_DB),
        "tonal_score": _float120("pre-calibration tonal score", 0.0, 1.0),
        "tonal_confidence": _float120("calibrated tonal confidence", 0.0, 1.0),
        "segment_start_s": {"type": "number", "minimum": 0},
        "segment_end_s": {"type": "number", "exclusiveMinimum": 0},
        "semantic_bundles": {"type": "array", "items": _SEMANTIC_BUNDLE_SCHEMA},
        "events": {"type": "array", "items": _PREDICTION_EVENT_SCHEMA},
        "anomaly_score_ref": _ANOMALY_REF_SCHEMA,
        "anomaly_severity_ref": _ANOMALY_REF_SCHEMA,
    },
    "description": (
        "§9.3 prediction. Events without anomaly_score_ref / "
        "anomaly_severity_ref are schema-invalid."
    ),
}

_CALIBRATOR_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(CALIBRATOR_KEYS),
    "properties": {
        "algorithm": {"type": "string", "minLength": 1},
        "parameters": {"type": "object"},
    },
}

_SCORE_KNOT_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(SCORE_KNOT_KEYS),
    "properties": {
        "score": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
    },
}

CALIBRATION_POLICY_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["calibration_policy"],
    "title": "aieq-v3-calibration-policy-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(CALIBRATION_POLICY_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["calibration_policy"]},
        "policy_id": {"type": "string", "minLength": 1},
        "policy_version": {"type": "string", "minLength": 1},
        "model_sha256": _sha256_prop(),
        "frontend_sha256": _sha256_prop(),
        "calibration_manifest_sha256": _sha256_prop(),
        "prediction_schema_id": {"const": SCHEMA_IDS["prediction"]},
        "prediction_schema_sha256": _sha256_prop(),
        "tonal_calibrator": _CALIBRATOR_SCHEMA,
        "anomaly_calibrator": _CALIBRATOR_SCHEMA,
        "tonal_band_thresholds": _float120("per-band thresholds", 0.0, 1.0),
        "semantic_type_thresholds": {
            "type": "array",
            "minItems": 8,
            "maxItems": 8,
            "items": {"type": "number", "minimum": 0, "maximum": 1},
        },
        "anomaly_class_thresholds": {
            "type": "array",
            "minItems": 3,
            "maxItems": 3,
            "items": {"type": "number", "minimum": 0, "maximum": 1},
        },
        "candidate_extractor_version": {"type": "string", "minLength": 1},
        "score_to_confidence": {
            "type": "array",
            "minItems": 2,
            "items": _SCORE_KNOT_SCHEMA,
            "description": "monotone non-decreasing; must include score 0 and 1",
        },
        "region_to_bundle": {
            "type": "object",
            "additionalProperties": False,
            "required": ["rule_id", "parameters"],
            "properties": {
                "rule_id": {"type": "string", "minLength": 1},
                "parameters": {"type": "object"},
            },
        },
        "threshold_to_actionable": {
            "type": "object",
            "additionalProperties": False,
            "required": ["rule_id", "parameters"],
            "properties": {
                "rule_id": {"type": "string", "minLength": 1},
                "parameters": {"type": "object"},
            },
        },
    },
}

_POWER_FAMILY_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(POWER_FAMILY_KEYS),
    "properties": {
        "family_id": _enum(BENCHMARK_FAMILIES),
        "floor_contractual": {"type": "integer", "minimum": 1},
        "n_power": {"type": "integer", "minimum": 1},
        "n_required": {"type": "integer", "minimum": 1},
        "unit": {"const": "group_id"},
    },
}

_POWER_GATE_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(POWER_GATE_KEYS),
    "properties": {
        "metric_id": {"type": "string", "minLength": 1},
        "statistic": {"type": "string", "minLength": 1},
        "orientation": {
            "type": "string",
            "enum": ["greater", "less", "two-sided"],
        },
        "min_effect_size": {"type": "number"},
        "n_power": {"type": "integer", "minimum": 1},
        "n_required": {"type": "integer", "minimum": 1},
        "support": {"type": "integer", "minimum": 0},
    },
}

BENCHMARK_POWER_PLAN_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["benchmark_power_plan"],
    "title": "aieq-v3-benchmark-power-plan-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(BENCHMARK_POWER_PLAN_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["benchmark_power_plan"]},
        "plan_id": {
            "type": "string",
            "minLength": 1,
            "description": (
                "G1a T2 freeze-from-prose document id (not letterally "
                "tabled in §11.2)"
            ),
        },
        "implementation_version": {"type": "string", "minLength": 1},
        "seed_base": {"type": "integer"},
        "pilot_sha256": _sha256_prop(),
        "power_source_role": {"const": "development_pilot"},
        "m": {"type": "integer", "minimum": 1},
        "alpha_plan": {"type": "number", "exclusiveMinimum": 0, "maximum": 1},
        "families": {
            "type": "array",
            "minItems": 1,
            "items": _POWER_FAMILY_SCHEMA,
            "description": (
                "Freeze-from-prose packaging of §11.2 family floors / "
                "n_required; exact nested keys are T2 structure"
            ),
        },
        "gates": {
            "type": "array",
            "minItems": 1,
            "items": _POWER_GATE_SCHEMA,
            "description": (
                "Freeze-from-prose packaging of §11.2 primary gates; "
                "exact nested keys are T2 structure"
            ),
        },
        "support": {"type": "object"},
        "result": {"type": "object"},
    },
    "description": (
        "G1a T2 freeze-from-prose packaging of §11.2 power-plan contents "
        "(not a letterally tabled JSON schema). Power is estimated only "
        "from development_pilot; unit must be group_id (never file/crop); "
        "final-test must not appear as a power source."
    ),
}

SCHEMA_REGISTRY: Final[dict[str, dict[str, Any]]] = {
    SCHEMA_IDS["asset_manifest"]: ASSET_MANIFEST_SCHEMA,
    SCHEMA_IDS["admission_batch"]: ADMISSION_BATCH_SCHEMA,
    SCHEMA_IDS["annotation"]: ANNOTATION_SCHEMA,
    SCHEMA_IDS["prediction"]: PREDICTION_SCHEMA,
    SCHEMA_IDS["calibration_policy"]: CALIBRATION_POLICY_SCHEMA,
    SCHEMA_IDS["benchmark_power_plan"]: BENCHMARK_POWER_PLAN_SCHEMA,
}

# Normative hashed schema *surface* (F3). Instance JSON under
# fixtures/g1/examples/ are example goldens, not the schema registry.
SCHEMA_REGISTRY_ARTIFACT_ID: Final[str] = "aieq-v3-schema-registry-1"
SCHEMA_REGISTRY_RELPATH: Final[str] = (
    "ml_v3/fixtures/g1/schema_registry_v1.json"
)

# Exact-key frozensets exported into the hashed surface so expanding a
# required-key set changes the tracked digest even if a nested schema
# dict were left stale (defense in depth).
_KEY_SETS: Final[dict[str, frozenset[str]]] = {
    "ASSET_MANIFEST_KEYS": ASSET_MANIFEST_KEYS,
    "ADMISSION_BATCH_KEYS": ADMISSION_BATCH_KEYS,
    "ANNOTATION_KEYS": ANNOTATION_KEYS,
    "PREDICTION_KEYS": PREDICTION_KEYS,
    "CALIBRATION_POLICY_KEYS": CALIBRATION_POLICY_KEYS,
    "BENCHMARK_POWER_PLAN_KEYS": BENCHMARK_POWER_PLAN_KEYS,
    "SEMANTIC_REGION_KEYS": SEMANTIC_REGION_KEYS,
    "DYNAMIC_EVENT_KEYS": DYNAMIC_EVENT_KEYS,
    "SEMANTIC_BUNDLE_KEYS": SEMANTIC_BUNDLE_KEYS,
    "PREDICTION_EVENT_KEYS": PREDICTION_EVENT_KEYS,
    "ANOMALY_REF_KEYS": ANOMALY_REF_KEYS,
    "CALIBRATOR_KEYS": CALIBRATOR_KEYS,
    "POWER_FAMILY_KEYS": POWER_FAMILY_KEYS,
    "POWER_GATE_KEYS": POWER_GATE_KEYS,
    "SCORE_KNOT_KEYS": SCORE_KNOT_KEYS,
}


def schema_ids_t2() -> tuple[str, ...]:
    """The six §14 G1a T2 schema identifiers, stable order."""
    return (
        SCHEMA_IDS["asset_manifest"],
        SCHEMA_IDS["admission_batch"],
        SCHEMA_IDS["annotation"],
        SCHEMA_IDS["prediction"],
        SCHEMA_IDS["calibration_policy"],
        SCHEMA_IDS["benchmark_power_plan"],
    )


def schema_for(schema_id: str) -> dict[str, Any]:
    """Return a shallow-copied schema document for ``schema_id``."""
    try:
        return dict(SCHEMA_REGISTRY[schema_id])
    except KeyError as exc:
        raise KeyError(f"unknown T2 schema id: {schema_id!r}") from exc


def frozen_schema_registry() -> dict[str, Any]:
    """Canonical schema-surface artifact for SHA256SUMS COVERED (F3).

    Serializes SCHEMA_REGISTRY (six T2 JSON Schema dicts) plus the exact-key
    frozensets as sorted lists. Expanding ASSET_MANIFEST_KEYS / a required
    field MUST change ``schema_registry_sha256()``.
    """
    from copy import deepcopy

    schemas = {
        schema_id: deepcopy(SCHEMA_REGISTRY[schema_id])
        for schema_id in schema_ids_t2()
    }
    key_sets = {
        name: sorted(keys) for name, keys in sorted(_KEY_SETS.items())
    }
    return {
        "artifact_id": SCHEMA_REGISTRY_ARTIFACT_ID,
        "key_sets": key_sets,
        "schema_ids": list(schema_ids_t2()),
        "schemas": schemas,
    }


def schema_registry_sha256() -> str:
    """SHA-256 of canonical_bytes(frozen_schema_registry())."""
    from .canonical import sha256_of_obj

    return sha256_of_obj(frozen_schema_registry())
```

---

## B.35 FILE: `ml_v3/contracts/validate.py`

**Path logico:** `ml_v3/contracts/validate.py`  
**Bytes:** 33661  
**Lines:** 755

```python
"""Fail-closed validators for Motore v3 G1a T2 JSON schemas.

Stdlib-only. Malformed documents raise SchemaError; nothing is reinterpreted
or coerced. Enforces G1a T2 freeze-from-prose envelopes derived from
docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a §9 / §11.2 (except asset-manifest
keys, which §9.1 tables letterally) and T1
``validate_manifest_split_invariants`` field expectations.
"""
from __future__ import annotations

import math
from typing import Any, Iterable, Mapping, Sequence

from .canonical import is_sha256_hex
from .constants import (
    ACCEPTED_SAMPLE_RATES,
    ANOMALY_CLASSES,
    BENCHMARK_FAMILIES,
    GRID_BANDS,
    GRID_MAX_HZ,
    GRID_MIN_HZ,
    LICENSE_CLASSES,
    PROBLEM_TYPE_TO_ID,
    PROBLEM_TYPES,
    SCHEMA_IDS,
    SPLIT_ROLES,
    TONAL_CURVE_MAX_DB,
    TONAL_CURVE_MIN_DB,
)
from .profiles import ProfileError, is_canonical_profile, require_problem_type_consistency
from .schemas import (
    ADMISSION_BATCH_KEYS,
    ANOMALY_REF_KEYS,
    ANNOTATION_KEYS,
    ASSET_MANIFEST_KEYS,
    BENCHMARK_POWER_PLAN_KEYS,
    CALIBRATION_POLICY_KEYS,
    CALIBRATOR_KEYS,
    DYNAMIC_EVENT_KEYS,
    POWER_FAMILY_KEYS,
    POWER_GATE_KEYS,
    PREDICTION_EVENT_KEYS,
    PREDICTION_KEYS,
    SCHEMA_REGISTRY,
    SCORE_KNOT_KEYS,
    SEMANTIC_BUNDLE_KEYS,
    SEMANTIC_REGION_KEYS,
)
from .split import SplitError, verify_commitment

__all__ = [
    "SchemaError",
    "validate",
    "validate_schema_id",
    "validate_asset_manifest",
    "validate_admission_batch",
    "validate_annotation",
    "validate_prediction",
    "validate_calibration_policy",
    "validate_benchmark_power_plan",
]


class SchemaError(ValueError):
    """Raised when a document violates a G1a T2 schema. Never self-corrects."""


# ------------------------------------------------------------------ helpers
def _require_mapping(doc: object, label: str) -> Mapping[str, Any]:
    if not isinstance(doc, Mapping):
        raise SchemaError(f"{label} must be a JSON object, got {type(doc).__name__}")
    return doc


def _exact_keys(doc: Mapping[str, Any], allowed: frozenset[str], label: str) -> None:
    keys = set(doc.keys())
    missing = allowed - keys
    extra = keys - allowed
    if missing:
        raise SchemaError(f"{label}: missing required keys: {sorted(missing)}")
    if extra:
        raise SchemaError(
            f"{label}: additionalProperties forbidden; unexpected keys: "
            f"{sorted(extra)}")


def _require_str(doc: Mapping[str, Any], key: str, *, nonempty: bool = True) -> str:
    value = doc[key]
    if not isinstance(value, str):
        raise SchemaError(f"{key} must be a string, got {type(value).__name__}")
    if nonempty and not value:
        raise SchemaError(f"{key} must be a non-empty string")
    if "\x00" in value:
        raise SchemaError(f"{key} contains U+0000")
    return value


def _require_bool(doc: Mapping[str, Any], key: str) -> bool:
    value = doc[key]
    if not isinstance(value, bool):
        raise SchemaError(f"{key} must be a boolean, got {type(value).__name__}")
    return value


def _require_int(doc: Mapping[str, Any], key: str, *, minimum: int | None = None
                 ) -> int:
    value = doc[key]
    if isinstance(value, bool) or not isinstance(value, int):
        raise SchemaError(f"{key} must be an integer, got {type(value).__name__}")
    if minimum is not None and value < minimum:
        raise SchemaError(f"{key} must be >= {minimum}, got {value}")
    return value


def _require_finite_number(value: object, path: str, *,
                           minimum: float | None = None,
                           maximum: float | None = None,
                           exclusive_minimum: float | None = None) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise SchemaError(f"{path} must be a finite number, got {type(value).__name__}")
    number = float(value)
    if not math.isfinite(number):
        raise SchemaError(f"{path} must be finite, got {number!r}")
    if minimum is not None and number < minimum:
        raise SchemaError(f"{path} must be >= {minimum}, got {number}")
    if maximum is not None and number > maximum:
        raise SchemaError(f"{path} must be <= {maximum}, got {number}")
    if exclusive_minimum is not None and number <= exclusive_minimum:
        raise SchemaError(f"{path} must be > {exclusive_minimum}, got {number}")
    return number


def _require_sha256(doc: Mapping[str, Any], key: str) -> str:
    value = _require_str(doc, key)
    if not is_sha256_hex(value):
        raise SchemaError(f"{key} must be lowercase hex-64 SHA-256, got {value!r}")
    return value


def _require_list(doc: Mapping[str, Any], key: str) -> list[Any]:
    value = doc[key]
    if not isinstance(value, list):
        raise SchemaError(f"{key} must be a list, got {type(value).__name__}")
    return value


def _require_float_vector(doc: Mapping[str, Any], key: str, length: int, *,
                          minimum: float | None = None,
                          maximum: float | None = None) -> list[float]:
    values = _require_list(doc, key)
    if len(values) != length:
        raise SchemaError(f"{key} must have length {length}, got {len(values)}")
    out: list[float] = []
    for index, item in enumerate(values):
        out.append(_require_finite_number(
            item, f"{key}[{index}]", minimum=minimum, maximum=maximum))
    return out


def _require_bool_vector(doc: Mapping[str, Any], key: str, length: int) -> list[bool]:
    values = _require_list(doc, key)
    if len(values) != length:
        raise SchemaError(f"{key} must have length {length}, got {len(values)}")
    out: list[bool] = []
    for index, item in enumerate(values):
        if not isinstance(item, bool):
            raise SchemaError(
                f"{key}[{index}] must be boolean, got {type(item).__name__}")
        out.append(item)
    return out


def _require_unique_enum_list(values: Sequence[Any], allowed: Iterable[str],
                              path: str) -> list[str]:
    allowed_set = set(allowed)
    if not isinstance(values, list):
        raise SchemaError(f"{path} must be a list")
    seen: set[str] = set()
    out: list[str] = []
    for index, item in enumerate(values):
        if not isinstance(item, str):
            raise SchemaError(f"{path}[{index}] must be a string")
        if item not in allowed_set:
            raise SchemaError(f"{path}[{index}] unknown value {item!r}")
        if item in seen:
            raise SchemaError(f"{path} contains duplicate {item!r}")
        seen.add(item)
        out.append(item)
    return out


def _require_profile(name: object, path: str) -> str:
    if not is_canonical_profile(name):
        raise SchemaError(f"{path} non-canonical profile {name!r}")
    return str(name)


def _hz_in_grid(value: float, path: str) -> float:
    return _require_finite_number(
        value, path, minimum=GRID_MIN_HZ, maximum=GRID_MAX_HZ)


def _check_segment(start: float, end: float, path: str) -> None:
    if end <= start:
        raise SchemaError(f"{path}: segment_end_s must be > segment_start_s")


def _check_time_in_segment(t0: float, t1: float, seg0: float, seg1: float,
                           path: str) -> None:
    if t0 < seg0 or t1 > seg1 or t1 <= t0:
        raise SchemaError(
            f"{path}: times [{t0}, {t1}] must fall inside segment "
            f"[{seg0}, {seg1}] with end > start")


# ----------------------------------------------------------- asset manifest
def validate_asset_manifest(doc: object) -> None:
    data = _require_mapping(doc, "asset_manifest")
    _exact_keys(data, ASSET_MANIFEST_KEYS, "asset_manifest")
    if data["schema"] != SCHEMA_IDS["asset_manifest"]:
        raise SchemaError(
            f"unexpected asset_manifest schema: {data['schema']!r}")
    _require_str(data, "asset_id")
    _require_str(data, "relative_path")
    _require_sha256(data, "sha256")
    _require_str(data, "group_id")
    _require_sha256(data, "admission_batch_id")
    role = _require_str(data, "split_role")
    if role not in SPLIT_ROLES:
        raise SchemaError(f"unknown split_role {role!r}")
    families = _require_unique_enum_list(
        _require_list(data, "benchmark_families"), BENCHMARK_FAMILIES,
        "benchmark_families")
    del families  # uniqueness/membership already checked
    pilot = _require_bool(data, "development_pilot")
    if pilot and role != "development-metric":
        raise SchemaError(
            "development_pilot may be true only when split_role is "
            "'development-metric'")
    _require_profile(data["source_profile"], "source_profile")
    _require_str(data, "primary_domain")
    _require_profile(data["group_primary_profile"], "group_primary_profile")
    _require_str(data, "group_primary_domain")
    _require_str(data, "source_family")
    sub = data["electronic_subgenre"]
    if sub is not None and (not isinstance(sub, str) or "\x00" in sub):
        raise SchemaError("electronic_subgenre must be string or null")
    rate = _require_int(data, "sample_rate")
    if rate not in ACCEPTED_SAMPLE_RATES:
        raise SchemaError(f"sample_rate {rate} not in ACCEPTED_SAMPLE_RATES")
    _require_int(data, "channels", minimum=1)
    _require_finite_number(data["duration_s"], "duration_s", exclusive_minimum=0.0)
    parent = data["parent_asset_id"]
    if parent is not None and (not isinstance(parent, str) or not parent):
        raise SchemaError("parent_asset_id must be non-empty string or null")
    kind = data["derivative_kind"]
    if kind is not None and (not isinstance(kind, str) or "\x00" in kind):
        raise SchemaError("derivative_kind must be string or null")
    if parent is None and kind is not None:
        raise SchemaError("root asset must have derivative_kind null")
    if parent is not None and kind is None:
        raise SchemaError("derivative asset requires non-null derivative_kind")
    license_class = _require_str(data, "license_class")
    if license_class not in LICENSE_CLASSES:
        raise SchemaError(f"unknown license_class {license_class!r}")
    _require_str(data, "license_url", nonempty=False)
    _require_str(data, "attribution", nonempty=False)
    ledger = _require_str(data, "ledger_id", nonempty=False)
    if license_class == "OWNED" and not ledger:
        raise SchemaError("OWNED license_class requires non-empty ledger_id")


# ---------------------------------------------------------- admission batch
def validate_admission_batch(doc: object) -> None:
    data = _require_mapping(doc, "admission_batch")
    _exact_keys(data, ADMISSION_BATCH_KEYS, "admission_batch")
    if data["schema"] != SCHEMA_IDS["admission_batch"]:
        raise SchemaError(
            f"unexpected admission_batch schema: {data['schema']!r}")
    batch_id = _require_sha256(data, "admission_batch_id")
    _require_str(data, "source_snapshot_id")
    _require_sha256(data, "source_snapshot_sha256")
    _require_str(data, "inclusion_rules_version")
    roster = _require_sha256(data, "roster_sha256")
    if roster != batch_id:
        raise SchemaError(
            "roster_sha256 must equal admission_batch_id "
            "(SHA-256 of canonical roster bytes)")
    commitment = _require_sha256(data, "salt_commitment")
    reveal = data["salt_reveal"]
    _require_str(data, "roster_commit")
    _require_str(data, "reviewer_id")
    status = _require_str(data, "status")
    if status not in ("admitted", "rejected"):
        raise SchemaError(f"status must be admitted|rejected, got {status!r}")
    # admitted ⇒ salt must be revealed (hex-64). rejected may keep null
    # (pre-reveal reject) or carry a hex reveal (revealed-then-rejected).
    # When reveal is non-null, §8.2.4 commit-reveal MUST hold:
    #   salt_commitment == SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt)
    # via split.verify_commitment (mismatched commitment → FAIL).
    if status == "admitted":
        if not isinstance(reveal, str) or not is_sha256_hex(reveal):
            raise SchemaError(
                "admitted status requires salt_reveal as lowercase hex-64 "
                "(null forbidden)")
    elif reveal is not None:
        if not isinstance(reveal, str) or not is_sha256_hex(reveal):
            raise SchemaError(
                "salt_reveal must be null or lowercase hex of 32 raw salt bytes")
    if isinstance(reveal, str):
        try:
            verify_commitment(bytes.fromhex(reveal), commitment)
        except SplitError as exc:
            raise SchemaError(
                f"salt_commitment does not match salt_reveal (§8.2.4): {exc}"
            ) from exc


# --------------------------------------------------------------- annotation
def _validate_semantic_region(region: object, index: int, seg0: float,
                              seg1: float) -> None:
    path = f"semantic_regions[{index}]"
    data = _require_mapping(region, path)
    _exact_keys(data, SEMANTIC_REGION_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in PROBLEM_TYPE_TO_ID:
        raise SchemaError(f"{path}.problem_type unknown {ptype!r}")
    pid = _require_int(data, "problem_type_id")
    try:
        require_problem_type_consistency(ptype, pid)
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    band_lo = data["band_lo_hz"]
    band_hi = data["band_hi_hz"]
    if band_lo is not None:
        band_lo = _hz_in_grid(
            _require_finite_number(band_lo, f"{path}.band_lo_hz"),
            f"{path}.band_lo_hz")
    if band_hi is not None:
        band_hi = _hz_in_grid(
            _require_finite_number(band_hi, f"{path}.band_hi_hz"),
            f"{path}.band_hi_hz")
    if (band_lo is None) != (band_hi is None):
        raise SchemaError(f"{path}: band_lo_hz/band_hi_hz must both be set or null")
    if band_lo is not None and band_hi is not None and band_hi <= band_lo:
        raise SchemaError(f"{path}: band_hi_hz must be > band_lo_hz")
    direction = data["direction"]
    if direction is not None and not isinstance(direction, str):
        raise SchemaError(f"{path}.direction must be string or null")
    if ptype in ("Thinness", "DullSound") and not direction:
        raise SchemaError(f"{path}: direction required for {ptype}")
    if ptype in ("Muddiness", "Boominess", "BoxyMidrange") and band_lo is None:
        raise SchemaError(f"{path}: band required for {ptype}")
    _require_finite_number(data["severity"], f"{path}.severity",
                           minimum=0.0, maximum=1.0)
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def _validate_dynamic_event(event: object, index: int, seg0: float,
                            seg1: float) -> None:
    path = f"dynamic_events[{index}]"
    data = _require_mapping(event, path)
    _exact_keys(data, DYNAMIC_EVENT_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in ANOMALY_CLASSES:
        raise SchemaError(
            f"{path}.problem_type must be one of {list(ANOMALY_CLASSES)}")
    pid = _require_int(data, "problem_type_id")
    try:
        require_problem_type_consistency(ptype, pid)
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    _hz_in_grid(
        _require_finite_number(data["center_hz"], f"{path}.center_hz"),
        f"{path}.center_hz")
    _require_finite_number(data["width_octaves"], f"{path}.width_octaves",
                           exclusive_minimum=0.0)
    _require_finite_number(data["severity"], f"{path}.severity",
                           minimum=0.0, maximum=1.0)
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def validate_annotation(doc: object) -> None:
    data = _require_mapping(doc, "annotation")
    _exact_keys(data, ANNOTATION_KEYS, "annotation")
    if data["schema"] != SCHEMA_IDS["annotation"]:
        raise SchemaError(f"unexpected annotation schema: {data['schema']!r}")
    _require_str(data, "asset_id")
    _require_str(data, "annotator_id")
    _require_str(data, "pass_id")
    _require_profile(data["profile"], "profile")
    _require_str(data, "evaluation_unit_id")
    seg0 = _require_finite_number(data["segment_start_s"], "segment_start_s",
                                  minimum=0.0)
    seg1 = _require_finite_number(data["segment_end_s"], "segment_end_s",
                                  exclusive_minimum=0.0)
    _check_segment(seg0, seg1, "annotation")
    curve = _require_float_vector(
        data, "tonal_correction_db", GRID_BANDS,
        minimum=TONAL_CURVE_MIN_DB, maximum=TONAL_CURVE_MAX_DB)
    _require_float_vector(data, "tonal_confidence", GRID_BANDS,
                          minimum=0.0, maximum=1.0)
    mask = _require_bool_vector(data, "tonal_actionable_mask", GRID_BANDS)
    regions = _require_list(data, "semantic_regions")
    events = _require_list(data, "dynamic_events")
    for index, region in enumerate(regions):
        _validate_semantic_region(region, index, seg0, seg1)
    for index, event in enumerate(events):
        _validate_dynamic_event(event, index, seg0, seg1)
    complete = _require_unique_enum_list(
        _require_list(data, "complete_types"), PROBLEM_TYPES, "complete_types")
    explicit_neg = _require_unique_enum_list(
        _require_list(data, "explicit_negative_types"), PROBLEM_TYPES,
        "explicit_negative_types")
    for name in explicit_neg:
        if name not in complete:
            raise SchemaError(
                f"explicit_negative_types entry {name!r} must also be in "
                "complete_types")
    global_actionable = _require_bool(data, "global_actionable")
    clean = _require_bool(data, "clean_for_action")
    _require_str(data, "notes", nonempty=False)
    _require_str(data, "tool_version")
    if clean:
        if any(abs(v) > 0.0 for v in curve):
            raise SchemaError("clean_for_action requires zero tonal_correction_db")
        if any(mask):
            raise SchemaError("clean_for_action requires tonal_actionable_mask all false")
        if global_actionable:
            raise SchemaError("clean_for_action requires global_actionable false")
        if any(bool(ev.get("actionable")) for ev in events):
            raise SchemaError("clean_for_action forbids actionable dynamic_events")
        if set(complete) != set(PROBLEM_TYPES) or set(explicit_neg) != set(PROBLEM_TYPES):
            raise SchemaError(
                "clean_for_action requires all eight types in complete_types "
                "and explicit_negative_types")


# --------------------------------------------------------------- prediction
def _validate_anomaly_ref(ref: object, path: str) -> None:
    data = _require_mapping(ref, path)
    _exact_keys(data, ANOMALY_REF_KEYS, path)
    _require_str(data, "relative_path")
    _require_sha256(data, "sha256")
    _require_int(data, "num_feature_frames", minimum=1)
    if data["dtype"] != "float32_le":
        raise SchemaError(f"{path}.dtype must be 'float32_le'")


def _validate_prediction_bundle(bundle: object, index: int, seg0: float,
                                seg1: float) -> None:
    path = f"semantic_bundles[{index}]"
    data = _require_mapping(bundle, path)
    _exact_keys(data, SEMANTIC_BUNDLE_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in PROBLEM_TYPE_TO_ID:
        raise SchemaError(f"{path}.problem_type unknown {ptype!r}")
    try:
        require_problem_type_consistency(ptype, _require_int(data, "problem_type_id"))
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    for key in ("band_lo_hz", "band_hi_hz"):
        if data[key] is not None:
            _hz_in_grid(_require_finite_number(data[key], f"{path}.{key}"),
                        f"{path}.{key}")
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def _validate_prediction_event(event: object, index: int, seg0: float,
                               seg1: float) -> None:
    path = f"events[{index}]"
    data = _require_mapping(event, path)
    _exact_keys(data, PREDICTION_EVENT_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in ANOMALY_CLASSES:
        raise SchemaError(
            f"{path}.problem_type must be one of {list(ANOMALY_CLASSES)}")
    try:
        require_problem_type_consistency(ptype, _require_int(data, "problem_type_id"))
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    _hz_in_grid(_require_finite_number(data["center_hz"], f"{path}.center_hz"),
                f"{path}.center_hz")
    _require_finite_number(data["width_octaves"], f"{path}.width_octaves",
                           exclusive_minimum=0.0)
    _require_finite_number(data["severity"], f"{path}.severity",
                           minimum=0.0, maximum=1.0)
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def validate_prediction(doc: object) -> None:
    data = _require_mapping(doc, "prediction")
    _exact_keys(data, PREDICTION_KEYS, "prediction")
    if data["schema"] != SCHEMA_IDS["prediction"]:
        raise SchemaError(f"unexpected prediction schema: {data['schema']!r}")
    _require_str(data, "asset_id")
    _require_str(data, "model_id")
    _require_sha256(data, "model_sha256")
    _require_sha256(data, "frontend_contract_sha256")
    _require_str(data, "calibration_policy_id")
    _require_sha256(data, "calibration_policy_sha256")
    _require_profile(data["profile"], "profile")
    _require_float_vector(
        data, "tonal_curve_db", GRID_BANDS,
        minimum=TONAL_CURVE_MIN_DB, maximum=TONAL_CURVE_MAX_DB)
    _require_float_vector(data, "tonal_score", GRID_BANDS, minimum=0.0, maximum=1.0)
    _require_float_vector(data, "tonal_confidence", GRID_BANDS,
                          minimum=0.0, maximum=1.0)
    seg0 = _require_finite_number(data["segment_start_s"], "segment_start_s",
                                  minimum=0.0)
    seg1 = _require_finite_number(data["segment_end_s"], "segment_end_s",
                                  exclusive_minimum=0.0)
    _check_segment(seg0, seg1, "prediction")
    bundles = _require_list(data, "semantic_bundles")
    events = _require_list(data, "events")
    for index, bundle in enumerate(bundles):
        _validate_prediction_bundle(bundle, index, seg0, seg1)
    for index, event in enumerate(events):
        _validate_prediction_event(event, index, seg0, seg1)
    # Contract: event list without dense anomaly surfaces is schema-invalid.
    # Refs are required keys; validate shape metadata.
    _validate_anomaly_ref(data["anomaly_score_ref"], "anomaly_score_ref")
    _validate_anomaly_ref(data["anomaly_severity_ref"], "anomaly_severity_ref")
    score_frames = data["anomaly_score_ref"]["num_feature_frames"]
    sev_frames = data["anomaly_severity_ref"]["num_feature_frames"]
    if score_frames != sev_frames:
        raise SchemaError(
            "anomaly_score_ref and anomaly_severity_ref num_feature_frames "
            "must match")


# ------------------------------------------------------ calibration policy
def _validate_calibrator(cal: object, path: str) -> None:
    data = _require_mapping(cal, path)
    _exact_keys(data, CALIBRATOR_KEYS, path)
    _require_str(data, "algorithm")
    params = data["parameters"]
    if not isinstance(params, Mapping):
        raise SchemaError(f"{path}.parameters must be an object")


def _validate_rule_object(rule: object, path: str) -> None:
    data = _require_mapping(rule, path)
    allowed = frozenset({"rule_id", "parameters"})
    _exact_keys(data, allowed, path)
    _require_str(data, "rule_id")
    if not isinstance(data["parameters"], Mapping):
        raise SchemaError(f"{path}.parameters must be an object")


def _validate_score_to_confidence(knots: object) -> None:
    if not isinstance(knots, list) or len(knots) < 2:
        raise SchemaError("score_to_confidence must be a list with >= 2 knots")
    scores: list[float] = []
    confidences: list[float] = []
    for index, knot in enumerate(knots):
        path = f"score_to_confidence[{index}]"
        data = _require_mapping(knot, path)
        _exact_keys(data, SCORE_KNOT_KEYS, path)
        score = _require_finite_number(data["score"], f"{path}.score",
                                       minimum=0.0, maximum=1.0)
        conf = _require_finite_number(data["confidence"], f"{path}.confidence",
                                      minimum=0.0, maximum=1.0)
        scores.append(score)
        confidences.append(conf)
    if 0.0 not in scores or 1.0 not in scores:
        raise SchemaError("score_to_confidence must define knots at score 0 and 1")
    for index in range(1, len(scores)):
        if scores[index] <= scores[index - 1]:
            raise SchemaError(
                "score_to_confidence scores must be strictly increasing")
        if confidences[index] + 1e-15 < confidences[index - 1]:
            raise SchemaError(
                "score_to_confidence must be monotone non-decreasing")


def validate_calibration_policy(doc: object) -> None:
    data = _require_mapping(doc, "calibration_policy")
    _exact_keys(data, CALIBRATION_POLICY_KEYS, "calibration_policy")
    if data["schema"] != SCHEMA_IDS["calibration_policy"]:
        raise SchemaError(
            f"unexpected calibration_policy schema: {data['schema']!r}")
    _require_str(data, "policy_id")
    _require_str(data, "policy_version")
    _require_sha256(data, "model_sha256")
    _require_sha256(data, "frontend_sha256")
    _require_sha256(data, "calibration_manifest_sha256")
    if data["prediction_schema_id"] != SCHEMA_IDS["prediction"]:
        raise SchemaError(
            f"prediction_schema_id must be {SCHEMA_IDS['prediction']!r}")
    _require_sha256(data, "prediction_schema_sha256")
    _validate_calibrator(data["tonal_calibrator"], "tonal_calibrator")
    _validate_calibrator(data["anomaly_calibrator"], "anomaly_calibrator")
    _require_float_vector(data, "tonal_band_thresholds", GRID_BANDS,
                          minimum=0.0, maximum=1.0)
    sem = _require_float_vector(data, "semantic_type_thresholds", 8,
                                minimum=0.0, maximum=1.0)
    anom = _require_float_vector(data, "anomaly_class_thresholds", 3,
                                 minimum=0.0, maximum=1.0)
    del sem, anom
    _require_str(data, "candidate_extractor_version")
    _validate_score_to_confidence(data["score_to_confidence"])
    _validate_rule_object(data["region_to_bundle"], "region_to_bundle")
    _validate_rule_object(data["threshold_to_actionable"], "threshold_to_actionable")


# ---------------------------------------------------- benchmark power plan
def _forbid_final_test_leak(obj: object, path: str = "$") -> None:
    """Reject any string leaf equal to final-test / final_test as power source."""
    if isinstance(obj, str):
        lowered = obj.lower().replace("_", "-")
        if lowered == "final-test":
            raise SchemaError(
                f"{path}: power plan must not reference final-test as input")
        return
    if isinstance(obj, Mapping):
        for key, value in obj.items():
            key_path = f"{path}.{key}" if isinstance(key, str) else path
            if isinstance(key, str):
                key_norm = key.lower().replace("_", "-")
                if key_norm in ("final-test", "finaltest"):
                    raise SchemaError(
                        f"{path}: power plan must not contain final-test key")
            _forbid_final_test_leak(value, key_path)
        return
    if isinstance(obj, list):
        for index, value in enumerate(obj):
            _forbid_final_test_leak(value, f"{path}[{index}]")


def validate_benchmark_power_plan(doc: object) -> None:
    data = _require_mapping(doc, "benchmark_power_plan")
    _exact_keys(data, BENCHMARK_POWER_PLAN_KEYS, "benchmark_power_plan")
    if data["schema"] != SCHEMA_IDS["benchmark_power_plan"]:
        raise SchemaError(
            f"unexpected benchmark_power_plan schema: {data['schema']!r}")
    _require_str(data, "plan_id")
    _require_str(data, "implementation_version")
    _require_int(data, "seed_base")
    _require_sha256(data, "pilot_sha256")
    source = _require_str(data, "power_source_role")
    if source != "development_pilot":
        raise SchemaError(
            "power_source_role must be 'development_pilot' "
            "(never final-test)")
    m_value = _require_int(data, "m", minimum=1)
    alpha = _require_finite_number(data["alpha_plan"], "alpha_plan",
                                   exclusive_minimum=0.0, maximum=1.0)
    expected = 0.05 / m_value
    if abs(alpha - expected) > 1e-12:
        raise SchemaError(
            f"alpha_plan must equal 0.05/m ({expected}), got {alpha}")
    families = _require_list(data, "families")
    if not families:
        raise SchemaError("families must be non-empty")
    seen_families: set[str] = set()
    for index, family in enumerate(families):
        path = f"families[{index}]"
        row = _require_mapping(family, path)
        _exact_keys(row, POWER_FAMILY_KEYS, path)
        fam = _require_str(row, "family_id")
        if fam not in BENCHMARK_FAMILIES:
            raise SchemaError(f"{path}.family_id unknown {fam!r}")
        if fam in seen_families:
            raise SchemaError(f"duplicate family_id {fam!r}")
        seen_families.add(fam)
        floor = _require_int(row, "floor_contractual", minimum=1)
        n_power = _require_int(row, "n_power", minimum=1)
        n_required = _require_int(row, "n_required", minimum=1)
        if n_required != max(floor, n_power):
            raise SchemaError(
                f"{path}.n_required must equal max(floor_contractual, n_power)")
        if row["unit"] != "group_id":
            raise SchemaError(
                f"{path}.unit must be 'group_id' (file/crop forbidden)")
    gates = _require_list(data, "gates")
    if not gates:
        raise SchemaError("gates must be non-empty")
    if len(gates) != m_value:
        raise SchemaError(f"gates length must equal m ({m_value})")
    for index, gate in enumerate(gates):
        path = f"gates[{index}]"
        row = _require_mapping(gate, path)
        _exact_keys(row, POWER_GATE_KEYS, path)
        _require_str(row, "metric_id")
        _require_str(row, "statistic")
        orientation = _require_str(row, "orientation")
        if orientation not in ("greater", "less", "two-sided"):
            raise SchemaError(f"{path}.orientation invalid {orientation!r}")
        _require_finite_number(row["min_effect_size"], f"{path}.min_effect_size")
        _require_int(row, "n_power", minimum=1)
        _require_int(row, "n_required", minimum=1)
        _require_int(row, "support", minimum=0)
    if not isinstance(data["support"], Mapping):
        raise SchemaError("support must be an object")
    if not isinstance(data["result"], Mapping):
        raise SchemaError("result must be an object")
    _forbid_final_test_leak(data["support"], "support")
    _forbid_final_test_leak(data["result"], "result")


# --------------------------------------------------------------- dispatch
_VALIDATORS = {
    SCHEMA_IDS["asset_manifest"]: validate_asset_manifest,
    SCHEMA_IDS["admission_batch"]: validate_admission_batch,
    SCHEMA_IDS["annotation"]: validate_annotation,
    SCHEMA_IDS["prediction"]: validate_prediction,
    SCHEMA_IDS["calibration_policy"]: validate_calibration_policy,
    SCHEMA_IDS["benchmark_power_plan"]: validate_benchmark_power_plan,
}


def validate(document: object) -> str:
    """Validate a document by its ``schema`` field. Returns the schema id."""
    data = _require_mapping(document, "document")
    if "schema" not in data:
        raise SchemaError("document missing 'schema' field")
    schema_id = data["schema"]
    if not isinstance(schema_id, str):
        raise SchemaError("schema must be a string")
    validator = _VALIDATORS.get(schema_id)
    if validator is None:
        raise SchemaError(
            f"unsupported or unknown schema id {schema_id!r}; "
            f"T2 registry has: {sorted(SCHEMA_REGISTRY)}")
    validator(data)
    return schema_id


def validate_schema_id(document: object, expected_schema_id: str) -> None:
    """Validate and require ``document['schema'] == expected_schema_id``."""
    got = validate(document)
    if got != expected_schema_id:
        raise SchemaError(
            f"expected schema {expected_schema_id!r}, got {got!r}")
```

---

## B.36 FILE: `ml_v3/contracts/adapter.py`

**Path logico:** `ml_v3/contracts/adapter.py`  
**Bytes:** 12359  
**Lines:** 316

```python
"""Frozen Motore v2↔v3 homologous adapter mapping (G1a T3).

Authority: docs/MOTORE_V3_G1_CONTRACT.md §10.5 @ 6d254d0a.

G1a serializes the mapping, constants and hash already defined in the contract;
it does not choose or modify them (§10.5 final sentence). This module freezes
that packaging for deterministic hashing. It does NOT evaluate macro-F1, run
windows, or implement the G1c homologous evaluator.

Honesty / A15:
  §10.5 states the semantics in prose and does not publish a literal JSON key
  table. Envelope keys below are therefore freeze-from-prose packaging of that
  section (plus the Techno→edm legacy alias already frozen in profiles.py).
  Do not claim a letteral exact-key table in the contract document.
"""
from __future__ import annotations

from copy import deepcopy
from typing import Any

from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import CONTRACT_REVISION, PROBLEM_TYPES
from .profiles import LEGACY_PROFILE_ALIASES

__all__ = [
    "AdapterError",
    "ADAPTER_ARTIFACT_ID",
    "ADAPTER_SECTION",
    "HOMOLOGOUS_CLASSES",
    "MASKED_NA_CLASSES",
    "WINDOW_STEP",
    "MIN_WINDOW_CENTERS",
    "OCCUPANCY_THRESHOLD",
    "PER_CLASS_SUPPORT_FLOORS",
    "frozen_adapter_mapping",
    "adapter_mapping_bytes",
    "adapter_mapping_sha256",
    "validate_adapter_mapping_claim",
    "require_homologous_class",
    "is_masked_na_class",
]

ADAPTER_ARTIFACT_ID = "aieq-v3-adapter-v2-v3-mapping-1"
ADAPTER_SECTION = "10.5"

# §10.5: six classes active in all three G0 candidates; macro-F1 denominator 6.
HOMOLOGOUS_CLASSES: tuple[str, ...] = (
    "Resonance",
    "Muddiness",
    "Boominess",
    "Thinness",
    "BoxyMidrange",
    "DullSound",
)

# §10.5: masked in G0 provenance; remain N/A in the v2 comparison.
MASKED_NA_CLASSES: tuple[str, ...] = ("Harshness", "Sibilance")

WINDOW_STEP: int = 16
MIN_WINDOW_CENTERS: int = 20
OCCUPANCY_THRESHOLD: float = 0.05  # 5%

# Per-class floors on development-metric and final-test (§10.5).
# Consequence: any of the six homologous classes below floor → gate NO-GO.
PER_CLASS_SUPPORT_FLOORS: dict[str, object] = {
    "positive_groups": 30,
    "negative_groups": 30,
    "roles": ("development-metric", "final-test"),
    "insufficient_support_consequence": (
        "insufficient support on any of the six homologous classes "
        "renders the gate NO-GO"
    ),
}

# G2-vs-v2 improvement rule (§10.5). Serialized here; not evaluated in G1a.
_G2_VS_V2_GATES: dict[str, object] = {
    "macro_f1_relative_improvement": 0.10,
    "macro_f1_absolute_when_seed_below": {
        "seed_threshold": 0.10,
        "absolute_delta": 0.10,
    },
    "fp_group_rate_not_worse_than_seed": True,
    "no_cross_seed_composition": True,
}


class AdapterError(ValueError):
    """Raised when an adapter mapping claim is malformed or non-canonical."""


def _assert_partition_complete() -> None:
    """Internal sanity: homologous ∪ masked == eight public types, disjoint."""
    homologous = set(HOMOLOGOUS_CLASSES)
    masked = set(MASKED_NA_CLASSES)
    if homologous & masked:
        raise RuntimeError("homologous and masked_na classes overlap")
    if homologous | masked != set(PROBLEM_TYPES):
        raise RuntimeError("homologous ∪ masked_na must equal PROBLEM_TYPES")
    if len(HOMOLOGOUS_CLASSES) != 6:
        raise RuntimeError("macro-F1 denominator requires exactly six classes")


_assert_partition_complete()


def frozen_adapter_mapping() -> dict[str, Any]:
    """Return a deep copy of the frozen §10.5 adapter mapping artifact.

    Envelope keys are freeze-from-prose packaging (see module docstring).
    """
    return {
        "artifact_id": ADAPTER_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": ADAPTER_SECTION,
        "envelope_authority": (
            "freeze-from-prose packaging of §10.5; contract has no literal "
            "JSON key table for this artifact"
        ),
        "homologous_classes": list(HOMOLOGOUS_CLASSES),
        "macro_f1_denominator": 6,
        "masked_na_classes": list(MASKED_NA_CLASSES),
        "masked_na_policy": (
            "Harshness and Sibilance stay N/A in the v2 comparison; never "
            "treat them as v2 negatives. v3 must clear absolute and G2 gates."
        ),
        "temporal_grid": {
            "window_step": WINDOW_STEP,
            "min_window_centers": MIN_WINDOW_CENTERS,
            "occupancy_threshold": OCCUPANCY_THRESHOLD,
            "short_segment_semantics": (
                "segment with fewer than min_window_centers is N/A for the "
                "homologous adapter; still available to native v3 metrics"
            ),
        },
        "label_at_center": {
            "v2": (
                "one if probability exceeds the provenance threshold of the seed"
            ),
            "v3": (
                "one if the center falls in the temporal support of an "
                "actionable bundle or event of the class; a static bundle "
                "covers the prediction segment"
            ),
            "gt": (
                "one if the center falls in an actionable semantic region or "
                "GT event"
            ),
        },
        "segment_presence": (
            "one for each of the three vectors only when the respective "
            "occupancy on the same grid is at least occupancy_threshold"
        ),
        "per_class_support_floors": {
            "positive_groups": PER_CLASS_SUPPORT_FLOORS["positive_groups"],
            "negative_groups": PER_CLASS_SUPPORT_FLOORS["negative_groups"],
            "roles": list(PER_CLASS_SUPPORT_FLOORS["roles"]),
            "insufficient_support_consequence": (
                PER_CLASS_SUPPORT_FLOORS["insufficient_support_consequence"]
            ),
        },
        "metrics": {
            "macro_f1_classes": 6,
            "false_positive_group_rate_on_clean": True,
            "invent_v2_curves_frequency_severity": False,
        },
        "structural_na_surfaces_v2": [
            "tonal_curves",
            "events",
            "frequency",
            "severity",
        ],
        # Structural absence → N/A; declared-but-invalid → fail-closed, never N/A.
        "na_semantics": (
            "N/A does not enter macro-averages, does not satisfy a gate, and "
            "does not demonstrate improvement; structural absence is N/A, "
            "never zero/infinity/FAIL; a candidate that declares a surface but "
            "emits no valid prediction is fail-closed (FN, schema error or "
            "candidate failure per case), never N/A"
        ),
        "g2_vs_v2_gates": deepcopy(_G2_VS_V2_GATES),
        # Extend the T1 legacy alias; do not invent a second Techno mapping.
        "legacy_profile_aliases": deepcopy(LEGACY_PROFILE_ALIASES),
    }


def adapter_mapping_bytes() -> bytes:
    """Canonical JSON bytes of the frozen mapping (final LF included)."""
    return canonical_bytes(frozen_adapter_mapping())


def adapter_mapping_sha256() -> str:
    """SHA-256 of the canonical frozen mapping artifact."""
    return sha256_of_obj(frozen_adapter_mapping())


def is_masked_na_class(name: str) -> bool:
    return name in MASKED_NA_CLASSES


def require_homologous_class(name: str) -> str:
    """Accept only one of the six homologous classes; reject masked N/A."""
    if name in MASKED_NA_CLASSES:
        raise AdapterError(
            f"{name!r} is masked N/A in the v2 homologous adapter; not a "
            f"homologous class")
    if name not in HOMOLOGOUS_CLASSES:
        raise AdapterError(
            f"non-homologous class {name!r}; allowed: "
            f"{list(HOMOLOGOUS_CLASSES)}")
    return name


def _require_exact_list(claim: dict[str, Any], key: str,
                        expected: list[str]) -> None:
    value = claim.get(key)
    if value != expected:
        raise AdapterError(
            f"adapter mapping claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def _require_exact(claim: dict[str, Any], key: str, expected: object) -> None:
    value = claim.get(key)
    if value != expected:
        raise AdapterError(
            f"adapter mapping claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def validate_adapter_mapping_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed adapter mapping artifact.

    Rejects malformed types, mutated constants, non-canonical class sets,
    and any claim that would treat Harshness/Sibilance as homologous or as
    v2 negatives. Byte-identity with the frozen canonical mapping is required.
    """
    if not isinstance(claim, dict):
        raise AdapterError(
            f"adapter mapping claim must be an object, got {type(claim).__name__}")

    frozen = frozen_adapter_mapping()

    _require_exact(claim, "artifact_id", ADAPTER_ARTIFACT_ID)
    _require_exact(claim, "contract_revision", CONTRACT_REVISION)
    _require_exact(claim, "section", ADAPTER_SECTION)
    _require_exact_list(claim, "homologous_classes", list(HOMOLOGOUS_CLASSES))
    _require_exact(claim, "macro_f1_denominator", 6)
    _require_exact_list(claim, "masked_na_classes", list(MASKED_NA_CLASSES))

    temporal = claim.get("temporal_grid")
    if not isinstance(temporal, dict):
        raise AdapterError("temporal_grid must be an object")
    if temporal.get("window_step") != WINDOW_STEP:
        raise AdapterError(
            f"window_step must be {WINDOW_STEP}, got {temporal.get('window_step')!r}")
    if temporal.get("min_window_centers") != MIN_WINDOW_CENTERS:
        raise AdapterError(
            f"min_window_centers must be {MIN_WINDOW_CENTERS}, got "
            f"{temporal.get('min_window_centers')!r}")
    if temporal.get("occupancy_threshold") != OCCUPANCY_THRESHOLD:
        raise AdapterError(
            f"occupancy_threshold must be {OCCUPANCY_THRESHOLD}, got "
            f"{temporal.get('occupancy_threshold')!r}")

    floors = claim.get("per_class_support_floors")
    if not isinstance(floors, dict):
        raise AdapterError("per_class_support_floors must be an object")
    if floors.get("positive_groups") != 30 or floors.get("negative_groups") != 30:
        raise AdapterError(
            "per_class_support_floors must require 30 positive and 30 negative "
            f"groups, got {floors!r}")
    if floors.get("roles") != ["development-metric", "final-test"]:
        raise AdapterError(
            "per_class_support_floors.roles must be "
            "['development-metric', 'final-test']")
    if floors.get("insufficient_support_consequence") != (
            PER_CLASS_SUPPORT_FLOORS["insufficient_support_consequence"]):
        raise AdapterError(
            "per_class_support_floors.insufficient_support_consequence must "
            "state that insufficient support on any of the six homologous "
            "classes renders the gate NO-GO")

    expected_na = frozen["na_semantics"]
    if claim.get("na_semantics") != expected_na:
        raise AdapterError(
            "na_semantics must include both structural-absence→N/A and "
            "declared-but-invalid→fail-closed (never N/A)")

    aliases = claim.get("legacy_profile_aliases")
    if aliases != LEGACY_PROFILE_ALIASES:
        raise AdapterError(
            "legacy_profile_aliases must match profiles.LEGACY_PROFILE_ALIASES "
            "(Techno→edm only); do not invent a divergent Techno mapping")

    # Reject any claim that silently promotes masked classes into homologous.
    homologous = claim.get("homologous_classes")
    if isinstance(homologous, list):
        for name in MASKED_NA_CLASSES:
            if name in homologous:
                raise AdapterError(
                    f"{name!r} must stay masked N/A; cannot appear in "
                    f"homologous_classes")

    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise AdapterError(f"claim is not canonically serializable: {exc}") from exc

    frozen_digest = sha256_of_obj(frozen)
    if claim_digest != frozen_digest:
        raise AdapterError(
            "adapter mapping claim is not byte-identical to the frozen §10.5 "
            f"mapping (claim_sha256={claim_digest}, "
            f"frozen_sha256={frozen_digest})")

    return claim
```

---

## B.37 FILE: `ml_v3/contracts/metrology_lock.py`

**Path logico:** `ml_v3/contracts/metrology_lock.py`  
**Bytes:** 37263  
**Lines:** 860

```python
"""Frozen G1a metrology lock (§13.1 / §13.2).

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a.

G1a serializes formulas, parameters and thresholds already defined in the
contract (warm-up/coda, stationary modes, streaming schedules, bit-identity
platform rule, SR-parity domain/activity, related gate thresholds). It does
not choose them, does not implement the resampler/frontend, and does not
evaluate parity gates.

Honesty / A15:
  §13 states the metrology in prose and does not publish a literal JSON key
  table for this lock. Envelope keys below are therefore freeze-from-prose
  packaging of §13.1/§13.2 (plus the §5 group-delay formula referenced by
  warm-up, and the §6.1 floor/clamp referenced by the activity predicate).
  Gate-platform OS/arch/python/numpy values are freeze-from-prose of
  ml_v3/environment/README.md + the numpy pin in requirements.lock.

§15 dependency-hash: the lock binds contract_revision and the T3 adapter
mapping digest so a T3/contract tip change invalidates this artifact.

Stop rule (T4.2): last hardening round on this lock. After T4.2, HIGH/MED
findings go to the debt list for T5/G1b — no new lock hash — unless a
CRITICAL vacuous-PASS or final-test leak is demonstrated. Do not invent
policy; only serialize / package contract prose.
"""
from __future__ import annotations

import platform
import sys
from copy import deepcopy
from math import gcd
from typing import Any

from .adapter import adapter_mapping_sha256
from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import (
    CANONICAL_SAMPLE_RATE,
    CONTRACT_REVISION,
    GATE_SAMPLE_RATES,
    GRID_BANDS,
)
from .grid import band_centers_hz

__all__ = [
    "MetrologyLockError",
    "METROLOGY_ARTIFACT_ID",
    "METROLOGY_SECTION",
    "GATE_PLATFORM_PYTHON",
    "HOP_SAMPLES",
    "N_LF",
    "K_WU",
    "K_CODA",
    "STREAMING_PRNG_SEED",
    "FIXED_CHUNK_SCHEDULES",
    "GEOMETRIC_CHUNK_SCHEDULE",
    "SWEEP_CHECKPOINT_HZ",
    "SR_PARITY_MAX_ABS_DB",
    "GATE_PLATFORM_FLOAT_TOL",
    "SECONDARY_FLOAT_ABS_TOL",
    "KAISER_BETA",
    "RESAMPLER_PASS_HZ",
    "STREAMING_FLOAT32_FRAME_FIELDS",
    "STREAMING_RATIONAL_TIMESTAMP_FIELDS",
    "STREAMING_VALIDITY_FIELDS",
    "STREAMING_ALSO_REQUIRED_PROOFS",
    "frozen_metrology_lock",
    "metrology_lock_bytes",
    "metrology_lock_sha256",
    "validate_metrology_lock_claim",
    "gate_platform_python_label",
    "require_gate_platform_python",
    "resampler_group_delay_rational",
    "warm_up_seconds",
    "coda_seconds",
]

METROLOGY_ARTIFACT_ID = "aieq-v3-metrology-lock-1"
METROLOGY_SECTION = "13"

# Canonical gate-platform interpreter (lock bit_identity.gate_platform.python).
# G1a evidence / re-CLOSE MUST run on this label (F4); not system 3.14.
GATE_PLATFORM_PYTHON = "CPython 3.12.13"

# §13.1 / §6.2
HOP_SAMPLES: int = 1024
N_LF: int = 8192
K_WU: int = 4
K_CODA: int = 4

# §13.2 gate 3
STREAMING_PRNG_SEED: int = 20260719
FIXED_CHUNK_SCHEDULES: tuple[int, ...] = (1, 63, 1024, 4095, 8192, 8193)
# Concrete freeze of floor(2**U), U~Uniform[0,14), length 32, PCG64(20260719).
# Authoritative list in the lock; do not re-roll after seeing FAIL.
GEOMETRIC_CHUNK_SCHEDULE: tuple[int, ...] = (
    55, 2455, 1, 109, 81, 88, 7061, 22, 499, 5337, 22, 1467, 58, 1, 142,
    233, 290, 20, 6, 18, 15955, 8039, 1, 2317, 727, 11131, 2927, 7, 1, 91,
    2096, 7,
)

# §13 sweep checkpoints: critical centres + path extremes (unique, ascending).
SWEEP_CHECKPOINT_HZ: tuple[int, ...] = (
    20, 45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000,
)

SR_PARITY_MAX_ABS_DB: float = 0.25
GAIN_INVARIANCE_MAX_ABS_DB: float = 0.05
MS_MID_EQUIVALENCE_MAX_ABS_DB: float = 0.05
ANTI_ALIAS_MAX_DB_RE_TONE: float = -80.0
GATE_PLATFORM_FLOAT_TOL: int = 0
SECONDARY_FLOAT_ABS_TOL: float = 1e-6
ACTIVITY_FLOOR_DB: float = -120.0
PSD_FLOOR_LINEAR: float = 1e-12
PSD_CLAMP_DB: tuple[float, float] = (-120.0, 12.0)
MODE_B_VARIANCE_MAX_DB2: float = 1.0
MODE_B_MAX_ABS_HOP_DELTA_DB: float = 0.5
MODE_B_T_MIN_HOPS: int = 8
KAISER_BETA: float = 9.0
RESAMPLER_PASS_HZ: float = 20000.0

# §7 float32 frame fields required for streaming≡offline identity (§13.2 gate 3).
STREAMING_FLOAT32_FRAME_FIELDS: tuple[str, ...] = (
    "mid_psd_db[120]",
    "side_psd_db[120]",
    "mid_shape_db[120]",
    "side_shape_db[120]",
    "mid_prominence_db[120]",
    "side_prominence_db[120]",
    "mid_delta_db[120]",
    "side_delta_db[120]",
    "mid_level_dbfs",
    "side_level_dbfs",
)
STREAMING_RATIONAL_TIMESTAMP_FIELDS: tuple[str, ...] = (
    "source_time_num",
    "source_time_den",
    "frame_end_sample",
    "frame_index",
)
STREAMING_VALIDITY_FIELDS: tuple[str, ...] = (
    "mid_valid",
    "side_valid",
    "valid",
    "reason",
)
STREAMING_ALSO_REQUIRED_PROOFS: tuple[dict[str, object], ...] = (
    {
        "id": "multi_asset_concat_with_explicit_delta_history_reset",
        "letter": "a",
        "requires": (
            "concatenated multi-asset input with explicit delta_db history "
            "reset at each asset boundary"
        ),
    },
    {
        "id": "interleaved_silence_between_assets",
        "letter": "b",
        "requires": "silence interleaved between assets",
    },
    {
        "id": "streaming_vs_offline_identity_same_lock_and_platform",
        "letter": "c",
        "requires": (
            "streaming-vs-offline identity on the same lock and gate platform"
        ),
    },
)

# Fixed warm-up offset excluding resampler delay: N_LF/fs_c + K_wu*H/fs_c = 32/125.
_FIXED_WU_NUM: int = 32
_FIXED_WU_DEN: int = 125
# Coda: K_coda*H/fs_c = 32/375.
_CODA_NUM: int = 32
_CODA_DEN: int = 375


class MetrologyLockError(ValueError):
    """Raised when a metrology lock claim is malformed or non-canonical."""


def _reduce(num: int, den: int) -> tuple[int, int]:
    if den <= 0:
        raise RuntimeError(f"non-positive denominator: {den}")
    g = gcd(num, den)
    return num // g, den // g


def resampler_group_delay_rational(fs_in: int) -> tuple[int, int, int | None]:
    """Return (delay_num, delay_den, num_taps) for fs_in → fs_c (§5).

    Identity (up == down == 1) → (0, 1, None). Serialize-only helper; not a
    resampler implementation.
    """
    if fs_in not in GATE_SAMPLE_RATES:
        raise MetrologyLockError(
            f"fs_in {fs_in} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")
    g = gcd(fs_in, CANONICAL_SAMPLE_RATE)
    up = CANONICAL_SAMPLE_RATE // g
    down = fs_in // g
    if up == 1 and down == 1:
        return 0, 1, None
    num_taps = 128 * max(up, down) + 1
    delay_num, delay_den = _reduce(num_taps - 1, 2 * up * fs_in)
    return delay_num, delay_den, num_taps


def _per_gate_delays() -> dict[str, dict[str, object]]:
    out: dict[str, dict[str, object]] = {}
    for fs_in in GATE_SAMPLE_RATES:
        g = gcd(fs_in, CANONICAL_SAMPLE_RATE)
        up = CANONICAL_SAMPLE_RATE // g
        down = fs_in // g
        delay_num, delay_den, num_taps = resampler_group_delay_rational(fs_in)
        out[str(fs_in)] = {
            "up": up,
            "down": down,
            "num_taps": num_taps,
            "delay_num": delay_num,
            "delay_den": delay_den,
        }
    return out


def warm_up_seconds(fs_in: int) -> float:
    """Derived warm-up seconds for a gate sample rate (additive formula)."""
    delay_num, delay_den, _ = resampler_group_delay_rational(fs_in)
    return (delay_num / delay_den) + (_FIXED_WU_NUM / _FIXED_WU_DEN)


def coda_seconds() -> float:
    return _CODA_NUM / _CODA_DEN


def _grid_centers_sha256() -> str:
    return sha256_of_obj(band_centers_hz())


def frozen_metrology_lock() -> dict[str, Any]:
    """Return a deep copy of the frozen §13 metrology lock artifact."""
    return {
        "artifact_id": METROLOGY_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": METROLOGY_SECTION,
        "envelope_authority": (
            "freeze-from-prose packaging of §13.1/§13.2 (+ §5 group-delay "
            "formula referenced by warm-up, §6.1 floor/clamp referenced by "
            "activity); contract has no literal JSON key table for this "
            "artifact"
        ),
        "dependencies": {
            "contract_revision": CONTRACT_REVISION,
            "adapter_mapping_sha256": adapter_mapping_sha256(),
            "grid_bands": GRID_BANDS,
            "grid_centers_sha256": _grid_centers_sha256(),
        },
        "hash_coverage": {
            "inline_dependencies_bound_here": True,
            "beyond_dependencies_covered_by": "T5_SHA256SUMS",
            "t5_sha256sums_covers_artifact_hashes_beyond_dependencies": True,
            "declaration": (
                "Artifact hashes beyond the inline dependencies object are "
                "covered by the T5 SHA256SUMS; they are not left implicit"
            ),
            "t4_2_stop_rule": (
                "T4.2 is the last hardening round on this lock; further "
                "HIGH/MED items become debt for T5/G1b (no new lock hash) "
                "unless CRITICAL vacuous-PASS or final-test leak"
            ),
        },
        "timing": {
            "H": HOP_SAMPLES,
            "fs_c": CANONICAL_SAMPLE_RATE,
            "N_LF": N_LF,
            "K_wu": K_WU,
            "K_coda": K_CODA,
            "warm_up_composition": "additive",
            "warm_up_composition_forbidden": "max",
            "warm_up_seconds_formula": (
                "resampler_group_delay_seconds + N_LF / fs_c + K_wu * H / fs_c"
            ),
            "coda_seconds_formula": "K_coda * H / fs_c",
            "fixed_offset_without_delay_num": _FIXED_WU_NUM,
            "fixed_offset_without_delay_den": _FIXED_WU_DEN,
            "coda_num": _CODA_NUM,
            "coda_den": _CODA_DEN,
            "exclude_predicate": (
                "source_time < warm_up_seconds OR "
                "source_time > T_asset - coda_seconds"
            ),
            "cross_sr_window": (
                "intersection of per-rate useful segments after warm-up and "
                "before coda (equivalent to max warm_up and max coda on the "
                "same source_time); comparing non-common tracts is FAIL"
            ),
        },
        "resampler_group_delay": {
            "authority_section": "5",
            "formula": (
                "(num_taps - 1) / (2 * up * fs_in) seconds; "
                "identity when up == down == 1 → 0"
            ),
            "num_taps_formula": "128 * max(up, down) + 1",
            "per_gate_sample_rate": _per_gate_delays(),
        },
        "resampler_generator": {
            "authority_section": "5",
            "serialize_only": True,
            "no_coefficient_implementation_in_g1a": True,
            "window": "kaiser",
            "kaiser_beta": KAISER_BETA,
            "pass_hz": RESAMPLER_PASS_HZ,
            "stop_hz_formula": "min(fs_in, 48000) / 2",
            "cutoff": "midpoint_of_pass_and_stop",
            "fc_formula": "((pass_hz + stop_hz) / 2) / (fs_in * up)",
            "coefficient_gain_scale": "up",
            "structure": "causal_polyphase",
            "polyphase_phase_zero": True,
            "streaming_state_preserved_across_blocks": True,
            "look_ahead_forbidden": True,
            "padding": "none",
            "reflection": False,
            "h0_formula": (
                "2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0); "
                "h[n] = up * h0[n] / sum(h0), n = 0..M; M = num_taps - 1"
            ),
            "sinc_definition": "sin(pi*x)/(pi*x)",
        },
        "stationary_portion": {
            "gate_closing_mode": "a",
            "mode_a": {
                "definition": (
                    "entire useful segment after warm-up and before coda"
                ),
                "applies_to": ["multitone", "noise"],
                "closes_sample_rate_parity_gate": True,
                "post_hoc_subset_forbidden": True,
            },
            "mode_b": {
                "diagnostic_only": True,
                "closes_sample_rate_parity_gate": False,
                "T_min_hops": MODE_B_T_MIN_HOPS,
                "T_min_seconds_formula": "8 * H / fs_c",
                "stability_variance_max_db2": MODE_B_VARIANCE_MAX_DB2,
                "stability_max_abs_hop_delta_db": MODE_B_MAX_ABS_HOP_DELTA_DB,
                "metric": (
                    "variance and max |Δ| hop-to-hop of mean mid_psd_db over "
                    "120 bands, computed hop-per-hop inside the window"
                ),
                "preregistered_windows": [],
                "omit_failing_window_is_fail": True,
                "empty_collection_diagnostic_is_fail": True,
                "mode_change_after_fail_forbidden": True,
            },
        },
        "sweep_log_parity": {
            "checkpoint_hz": list(SWEEP_CHECKPOINT_HZ),
            "match_radius_formula": "H / fs_c",
            "frame_selection": "nearest_source_time_within_match_radius",
            "alignment_policy_ref": "sample_rate_parity.alignment",
            "missing_checkpoint_is_fail": True,
            "no_free_subset": True,
            "transient_and_damped_resonance_excluded_from_db_parity": True,
        },
        "streaming_equivalence": {
            "prng": "PCG64",
            "prng_seed": STREAMING_PRNG_SEED,
            "fixed_chunk_schedules_host_samples": list(FIXED_CHUNK_SCHEDULES),
            "geometric_schedule": {
                "rule": (
                    "floor(2 ** U) with U ~ Uniform[0, 14), length >= 32, "
                    "same PCG64 seed; repeated identical on every gate asset"
                ),
                "length": len(GEOMETRIC_CHUNK_SCHEDULE),
                "u_low": 0,
                "u_high_exclusive": 14,
                "chunks_host_samples": list(GEOMETRIC_CHUNK_SCHEDULE),
                "derivation": (
                    "concrete freeze of "
                    "numpy.random.Generator(numpy.random.PCG64(20260719))"
                    ".uniform(0, 14, size=32) then floor(2**U); the listed "
                    "chunks are authoritative"
                ),
            },
            "required_proof_surface": {
                "authority_section": "13.2.gate_3",
                "float32_frame_fields": list(STREAMING_FLOAT32_FRAME_FIELDS),
                "rational_timestamp_fields": list(
                    STREAMING_RATIONAL_TIMESTAMP_FIELDS),
                "validity_fields": list(STREAMING_VALIDITY_FIELDS),
                "validity_reason_enumerated_when_false": True,
                "identity_rule": (
                    "for every frozen schedule, chunked input and monolithic "
                    "offline input on the same lock/platform must produce the "
                    "same V3FeatureFrame sequence over the enumerated fields"
                ),
                "also_required": [
                    dict(proof) for proof in STREAMING_ALSO_REQUIRED_PROOFS
                ],
                "also_required_quantifier": (
                    "for every frozen schedule (each fixed chunk size in "
                    "fixed_chunk_schedules_host_samples and the frozen "
                    "geometric schedule), on the gate platform"
                ),
                "also_required_omission_is_fail": True,
                "also_required_applies_to": (
                    "proofs (a)(b)(c) under the same per-schedule quantifier "
                    "as identity_rule; omitting any proof on any frozen "
                    "schedule → FAIL"
                ),
            },
        },
        "bit_identity": {
            "gate_platform": {
                "os": "darwin",
                "os_marketing": "macOS 15.5",
                "arch": "arm64",
                "python": GATE_PLATFORM_PYTHON,
                "numpy": "2.5.1",
                "authority": (
                    "freeze-from-prose of ml_v3/environment/README.md and "
                    "numpy pin in ml_v3/environment/requirements.lock"
                ),
            },
            "gate_platform_float_tol": GATE_PLATFORM_FLOAT_TOL,
            "gate_platform_float_identity": "byte_identical_only",
            "gate_platform_secondary_tol_forbidden": True,
            "rule_on_gate_platform": (
                "byte_identical float32 frame fields, rational timestamps "
                "and valid flags between offline and streaming; "
                "gate_platform_float_tol is 0 (no 1e-6 abs tol on gate "
                "platform)"
            ),
            "secondary_platforms_allowlist": [],
            "secondary_float_abs_tol": SECONDARY_FLOAT_ABS_TOL,
            "secondary_is_report_only": True,
            "secondary_cannot_close_g1_gate": True,
            "ad_hoc_non_bit_identical_without_allowlist": "FAIL",
        },
        "sample_rate_parity": {
            "reference_hz": CANONICAL_SAMPLE_RATE,
            "compare_hz": [44100, 96000],
            "threshold_max_abs_db": SR_PARITY_MAX_ABS_DB,
            "aggregator": "max",
            "aggregator_forbidden": ["mean", "p95", "RMSE", "band_subset"],
            "domain_db": [
                "mid_psd_db[120]",
                "side_psd_db[120]",
                "mid_shape_db[120]",
                "side_shape_db[120]",
                "mid_prominence_db[120]",
                "side_prominence_db[120]",
                "mid_level_dbfs",
                "side_level_dbfs",
            ],
            "excluded_from_domain": ["mid_delta_db", "side_delta_db"],
            "alignment": {
                "authority_section": "5+13.2.gate_4",
                "select_by": "nearest_source_time",
                "forbidden_select_by": ["output_index", "raw_frame_index"],
                "manual_frame_shift_forbidden": True,
                "choose_within_pm1_radius_to_minimize_abs_delta_forbidden": True,
                "tie_break": [
                    "smaller_frame_index",
                    "smaller_frame_end_sample",
                ],
                "declaration": (
                    "Cross-SR frames align on nearest source_time (not raw "
                    "output index); do not manually shift frames or pick "
                    "within a ±1-sample/hop radius to minimize |Δ|; ties "
                    "break by smaller frame_index, then smaller "
                    "frame_end_sample"
                ),
            },
            "activity": {
                "predicate": "max(psd_db_ref, psd_db_sr) > -120",
                "union_cross_sr": True,
                "floor_linear": PSD_FLOOR_LINEAR,
                "clamp_db": list(PSD_CLAMP_DB),
                "activity_floor_db": ACTIVITY_FLOOR_DB,
                "shape_prominence_inherit_psd_activity": True,
                "invalid_channel_vectors_ignored": True,
                "post_hoc_mask_forbidden": True,
                "empty_active_cell_set_is_fail": True,
                "empty_useful_segment_is_fail": True,
                "max_over_empty_active_set": "FAIL",
                "vacuous_pass_forbidden": True,
                "na_is_not_pass": True,
                "empty_to_fail_derivation": {
                    "kind": "derived_packaging",
                    "does_not_supersede_contract": True,
                    "candidate_for_future_contract_amendment": True,
                    "chain": [
                        (
                            "§10.5: N/A does not satisfy a gate and does not "
                            "demonstrate improvement"
                        ),
                        (
                            "§10.1 (~line 663): a record with no active cells "
                            "has curve metrics N/A, not zero; N/A is not "
                            "converted into PASS"
                        ),
                        (
                            "§13.2 gate 4: activity predicate + aggregator "
                            "max over the declared dB domain; max over an "
                            "empty active set is not a numeric 0 PASS"
                        ),
                    ],
                    "packaging_note": (
                        "empty_active_cell_set_is_fail / "
                        "empty_useful_segment_is_fail / "
                        "max_over_empty_active_set=FAIL package the chain "
                        "above; lock packaging authority, not a new "
                        "contract amendment (REV7 out of T4.2)"
                    ),
                },
            },
            "experimental_rates_cannot_close": [88200, 176400, 192000],
            "timestamp_tolerance_canonical_samples": 1,
            "transient_onset_peak_tolerance_canonical_samples": 1,
            "transient_decay_tolerance_hops": 1,
            "single_active_cell_over_threshold_fails_gate": True,
        },
        "other_gate_thresholds": {
            "gain_invariance_max_abs_db": GAIN_INVARIANCE_MAX_ABS_DB,
            "ms_mid_equivalence_max_abs_db": MS_MID_EQUIVALENCE_MAX_ABS_DB,
            "anti_alias_max_db_re_tone": ANTI_ALIAS_MAX_DB_RE_TONE,
        },
    }


def metrology_lock_bytes() -> bytes:
    """Canonical JSON bytes of the frozen metrology lock (final LF included)."""
    return canonical_bytes(frozen_metrology_lock())


def metrology_lock_sha256() -> str:
    """SHA-256 of the canonical frozen metrology lock artifact."""
    return sha256_of_obj(frozen_metrology_lock())


def _require_exact(claim: dict[str, Any], key: str, expected: object) -> None:
    value = claim.get(key)
    if value != expected:
        raise MetrologyLockError(
            f"metrology lock claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def validate_metrology_lock_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed metrology lock artifact.

    Rejects mutated timing constants, non-additive warm-up, activity without
    cross-SR union, empty active-cell vacuous PASS, stripped empty→FAIL
    derivation, SR alignment cherry-pick, non-zero gate-platform float tol,
    incomplete streaming proof surface / also_required per-schedule
    quantifier, missing §5 generator params / T5 hash-coverage declaration,
    mode-(b) used as gate-closing, empty/missing dependency digests, and any
    claim that is not byte-identical to the frozen lock.
    """
    if not isinstance(claim, dict):
        raise MetrologyLockError(
            f"metrology lock claim must be an object, got "
            f"{type(claim).__name__}")

    frozen = frozen_metrology_lock()
    _require_exact(claim, "artifact_id", METROLOGY_ARTIFACT_ID)
    _require_exact(claim, "contract_revision", CONTRACT_REVISION)
    _require_exact(claim, "section", METROLOGY_SECTION)

    deps = claim.get("dependencies")
    if not isinstance(deps, dict):
        raise MetrologyLockError("dependencies must be an object")
    if deps.get("contract_revision") != CONTRACT_REVISION:
        raise MetrologyLockError(
            "dependencies.contract_revision must equal CONTRACT_REVISION")
    expected_adapter = adapter_mapping_sha256()
    if deps.get("adapter_mapping_sha256") != expected_adapter:
        raise MetrologyLockError(
            "dependencies.adapter_mapping_sha256 must equal current T3 "
            f"adapter_mapping_sha256() ({expected_adapter})")
    if deps.get("grid_bands") != GRID_BANDS:
        raise MetrologyLockError(
            f"dependencies.grid_bands must be {GRID_BANDS}")
    if deps.get("grid_centers_sha256") != _grid_centers_sha256():
        raise MetrologyLockError(
            "dependencies.grid_centers_sha256 must equal sha256 of the "
            "frozen 120 band centres")

    timing = claim.get("timing")
    if not isinstance(timing, dict):
        raise MetrologyLockError("timing must be an object")
    if timing.get("H") != HOP_SAMPLES or timing.get("N_LF") != N_LF:
        raise MetrologyLockError("timing H/N_LF must match §13.1 constants")
    if timing.get("K_wu") != K_WU or timing.get("K_coda") != K_CODA:
        raise MetrologyLockError("timing K_wu/K_coda must be 4/4")
    if timing.get("warm_up_composition") != "additive":
        raise MetrologyLockError(
            "warm_up_composition must be 'additive' (REV6 residual H1; "
            "max is forbidden)")
    if timing.get("warm_up_composition_forbidden") != "max":
        raise MetrologyLockError(
            "warm_up_composition_forbidden must record that max is forbidden")

    stationary = claim.get("stationary_portion")
    if not isinstance(stationary, dict):
        raise MetrologyLockError("stationary_portion must be an object")
    if stationary.get("gate_closing_mode") != "a":
        raise MetrologyLockError(
            "gate_closing_mode must be 'a' for multitone/noise SR parity")
    mode_a = stationary.get("mode_a")
    mode_b = stationary.get("mode_b")
    if not isinstance(mode_a, dict) or not isinstance(mode_b, dict):
        raise MetrologyLockError("mode_a and mode_b must be objects")
    if mode_a.get("closes_sample_rate_parity_gate") is not True:
        raise MetrologyLockError("mode_a must close the SR parity gate")
    if mode_b.get("closes_sample_rate_parity_gate") is not False:
        raise MetrologyLockError(
            "mode_b must not close the SR parity gate (diagnostic only)")
    if mode_b.get("diagnostic_only") is not True:
        raise MetrologyLockError("mode_b must be diagnostic_only")

    activity = (
        claim.get("sample_rate_parity", {}).get("activity")
        if isinstance(claim.get("sample_rate_parity"), dict) else None
    )
    if not isinstance(activity, dict):
        raise MetrologyLockError("sample_rate_parity.activity must be an object")
    if activity.get("union_cross_sr") is not True:
        raise MetrologyLockError(
            "activity.union_cross_sr must be true (REV6 residual H2)")
    if activity.get("predicate") != "max(psd_db_ref, psd_db_sr) > -120":
        raise MetrologyLockError(
            "activity.predicate must be max(psd_db_ref, psd_db_sr) > -120")
    if activity.get("empty_active_cell_set_is_fail") is not True:
        raise MetrologyLockError(
            "activity.empty_active_cell_set_is_fail must be true "
            "(empty active-cell set under gate-4 activity → FAIL; "
            "max over empty is not 0; N/A ≠ PASS)")
    if activity.get("empty_useful_segment_is_fail") is not True:
        raise MetrologyLockError(
            "activity.empty_useful_segment_is_fail must be true")
    if activity.get("vacuous_pass_forbidden") is not True:
        raise MetrologyLockError(
            "activity.vacuous_pass_forbidden must be true")
    if activity.get("max_over_empty_active_set") != "FAIL":
        raise MetrologyLockError(
            "activity.max_over_empty_active_set must be 'FAIL'")
    derivation = activity.get("empty_to_fail_derivation")
    if not isinstance(derivation, dict):
        raise MetrologyLockError(
            "activity.empty_to_fail_derivation must be an object "
            "(derived packaging of §10.5 + §10.1 N/A≠zero + §13 gate 4; "
            "does not supersede the contract)")
    if derivation.get("does_not_supersede_contract") is not True:
        raise MetrologyLockError(
            "empty_to_fail_derivation.does_not_supersede_contract must be true")
    if derivation.get("kind") != "derived_packaging":
        raise MetrologyLockError(
            "empty_to_fail_derivation.kind must be 'derived_packaging'")
    chain = derivation.get("chain")
    if not isinstance(chain, list) or len(chain) < 3:
        raise MetrologyLockError(
            "empty_to_fail_derivation.chain must list the §10.5 / §10.1 / "
            "§13 gate-4 derivation steps")

    sr = claim.get("sample_rate_parity")
    if not isinstance(sr, dict):
        raise MetrologyLockError("sample_rate_parity must be an object")
    if sr.get("threshold_max_abs_db") != SR_PARITY_MAX_ABS_DB:
        raise MetrologyLockError(
            f"threshold_max_abs_db must be {SR_PARITY_MAX_ABS_DB}")
    if sr.get("aggregator") != "max":
        raise MetrologyLockError("sample_rate_parity.aggregator must be 'max'")
    alignment = sr.get("alignment")
    if not isinstance(alignment, dict):
        raise MetrologyLockError(
            "sample_rate_parity.alignment must be an object")
    if alignment.get("select_by") != "nearest_source_time":
        raise MetrologyLockError(
            "alignment.select_by must be 'nearest_source_time' "
            "(not raw output index)")
    if alignment.get("manual_frame_shift_forbidden") is not True:
        raise MetrologyLockError(
            "alignment.manual_frame_shift_forbidden must be true")
    if alignment.get(
            "choose_within_pm1_radius_to_minimize_abs_delta_forbidden"
    ) is not True:
        raise MetrologyLockError(
            "alignment.choose_within_pm1_radius_to_minimize_abs_delta_"
            "forbidden must be true (anti-cherry-pick)")
    if alignment.get("tie_break") != [
            "smaller_frame_index", "smaller_frame_end_sample"]:
        raise MetrologyLockError(
            "alignment.tie_break must be "
            "['smaller_frame_index', 'smaller_frame_end_sample']")

    bit_id = claim.get("bit_identity")
    if not isinstance(bit_id, dict):
        raise MetrologyLockError("bit_identity must be an object")
    if bit_id.get("gate_platform_float_tol") != GATE_PLATFORM_FLOAT_TOL:
        raise MetrologyLockError(
            "bit_identity.gate_platform_float_tol must be 0 "
            "(byte-identical only on gate platform; 1e-6 is secondary-only)")
    if bit_id.get("gate_platform_float_identity") != "byte_identical_only":
        raise MetrologyLockError(
            "bit_identity.gate_platform_float_identity must be "
            "'byte_identical_only'")
    if bit_id.get("gate_platform_secondary_tol_forbidden") is not True:
        raise MetrologyLockError(
            "bit_identity.gate_platform_secondary_tol_forbidden must be true")
    if bit_id.get("secondary_platforms_allowlist") != []:
        raise MetrologyLockError(
            "secondary_platforms_allowlist must be the empty preregistered "
            "list; ad-hoc platforms are FAIL")
    if bit_id.get("secondary_float_abs_tol") != SECONDARY_FLOAT_ABS_TOL:
        raise MetrologyLockError(
            f"secondary_float_abs_tol must remain {SECONDARY_FLOAT_ABS_TOL} "
            "(report-only; never applied on gate platform)")
    if bit_id.get("ad_hoc_non_bit_identical_without_allowlist") != "FAIL":
        raise MetrologyLockError(
            "ad_hoc_non_bit_identical_without_allowlist must be FAIL")

    streaming = claim.get("streaming_equivalence")
    if not isinstance(streaming, dict):
        raise MetrologyLockError("streaming_equivalence must be an object")
    if streaming.get("prng_seed") != STREAMING_PRNG_SEED:
        raise MetrologyLockError(
            f"prng_seed must be {STREAMING_PRNG_SEED}")
    geo = streaming.get("geometric_schedule")
    if not isinstance(geo, dict):
        raise MetrologyLockError("geometric_schedule must be an object")
    if geo.get("chunks_host_samples") != list(GEOMETRIC_CHUNK_SCHEDULE):
        raise MetrologyLockError(
            "geometric_schedule.chunks_host_samples must match the frozen "
            "PCG64(20260719) schedule")
    surface = streaming.get("required_proof_surface")
    if not isinstance(surface, dict):
        raise MetrologyLockError(
            "streaming_equivalence.required_proof_surface must be an object")
    if surface.get("float32_frame_fields") != list(
            STREAMING_FLOAT32_FRAME_FIELDS):
        raise MetrologyLockError(
            "required_proof_surface.float32_frame_fields must enumerate "
            "all §7 float32 frame fields")
    if surface.get("rational_timestamp_fields") != list(
            STREAMING_RATIONAL_TIMESTAMP_FIELDS):
        raise MetrologyLockError(
            "required_proof_surface.rational_timestamp_fields must enumerate "
            "source_time_num/den, frame_end_sample, frame_index")
    if surface.get("validity_fields") != list(STREAMING_VALIDITY_FIELDS):
        raise MetrologyLockError(
            "required_proof_surface.validity_fields must enumerate "
            "mid_valid/side_valid/valid/reason")
    also_required = surface.get("also_required")
    expected_also = [dict(p) for p in STREAMING_ALSO_REQUIRED_PROOFS]
    if also_required != expected_also:
        raise MetrologyLockError(
            "required_proof_surface.also_required must enumerate proofs "
            "(a) multi-asset concat+delta reset, (b) interleaved silence, "
            "(c) streaming-vs-offline identity")
    if surface.get("also_required_omission_is_fail") is not True:
        raise MetrologyLockError(
            "required_proof_surface.also_required_omission_is_fail must be "
            "true (proofs (a)(b)(c) required for every frozen schedule)")
    quantifier = surface.get("also_required_quantifier")
    if not isinstance(quantifier, str) or "every frozen schedule" not in quantifier:
        raise MetrologyLockError(
            "required_proof_surface.also_required_quantifier must state "
            "proofs (a)(b)(c) for every frozen schedule "
            "(fixed + geometric) on the gate platform")

    generator = claim.get("resampler_generator")
    if not isinstance(generator, dict):
        raise MetrologyLockError("resampler_generator must be an object")
    if generator.get("serialize_only") is not True:
        raise MetrologyLockError(
            "resampler_generator.serialize_only must be true (no G1a "
            "coefficient implementation)")
    if generator.get("kaiser_beta") != KAISER_BETA:
        raise MetrologyLockError(
            f"resampler_generator.kaiser_beta must be {KAISER_BETA}")
    if generator.get("pass_hz") != RESAMPLER_PASS_HZ:
        raise MetrologyLockError(
            f"resampler_generator.pass_hz must be {RESAMPLER_PASS_HZ}")
    if generator.get("structure") != "causal_polyphase":
        raise MetrologyLockError(
            "resampler_generator.structure must be 'causal_polyphase'")
    if generator.get("padding") != "none":
        raise MetrologyLockError(
            "resampler_generator.padding must be 'none'")
    if generator.get("reflection") is not False:
        raise MetrologyLockError(
            "resampler_generator.reflection must be false")
    if generator.get("streaming_state_preserved_across_blocks") is not True:
        raise MetrologyLockError(
            "resampler_generator.streaming_state_preserved_across_blocks "
            "must be true")

    coverage = claim.get("hash_coverage")
    if not isinstance(coverage, dict):
        raise MetrologyLockError("hash_coverage must be an object")
    if coverage.get(
            "t5_sha256sums_covers_artifact_hashes_beyond_dependencies"
    ) is not True:
        raise MetrologyLockError(
            "hash_coverage.t5_sha256sums_covers_artifact_hashes_beyond_"
            "dependencies must be true")
    if coverage.get("beyond_dependencies_covered_by") != "T5_SHA256SUMS":
        raise MetrologyLockError(
            "hash_coverage.beyond_dependencies_covered_by must be "
            "'T5_SHA256SUMS'")
    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise MetrologyLockError(
            f"claim is not canonically serializable: {exc}") from exc

    frozen_digest = sha256_of_obj(frozen)
    if claim_digest != frozen_digest:
        raise MetrologyLockError(
            "metrology lock claim is not byte-identical to the frozen §13 "
            f"lock (claim_sha256={claim_digest}, "
            f"frozen_sha256={frozen_digest})")

    # Defensive: ensure caller cannot mutate the returned frozen template
    # via a shared reference (claim may be a deep copy already).
    return deepcopy(claim)


def gate_platform_python_label() -> str:
    """Return ``{implementation} {major}.{minor}.{micro}`` for this process."""
    info = sys.version_info
    return (
        f"{platform.python_implementation()} "
        f"{info.major}.{info.minor}.{info.micro}"
    )


def require_gate_platform_python() -> str:
    """Fail-closed: running interpreter must match lock gate_platform.python.

    Reads the pin from ``frozen_metrology_lock()`` (and the module constant
    ``GATE_PLATFORM_PYTHON`` as a cross-check). Wrong CPython (e.g. 3.14)
    raises ``MetrologyLockError`` — do not reinterpret or skip.
    """
    lock = frozen_metrology_lock()
    expected = lock["bit_identity"]["gate_platform"]["python"]
    if expected != GATE_PLATFORM_PYTHON:
        raise MetrologyLockError(
            "frozen lock gate_platform.python drifted from "
            f"GATE_PLATFORM_PYTHON constant: lock={expected!r}, "
            f"constant={GATE_PLATFORM_PYTHON!r}")
    actual = gate_platform_python_label()
    if actual != expected:
        raise MetrologyLockError(
            "interpreter does not match metrology lock gate_platform.python: "
            f"actual={actual!r}, expected={expected!r}; "
            "use ~/aieq_data/motore_v3/env/venv (CPython 3.12.13)")
    return actual
```

---

## B.38 FILE: `ml_v3/contracts/sha256sums.py`

**Path logico:** `ml_v3/contracts/sha256sums.py`  
**Bytes:** 9239  
**Lines:** 230

```python
"""G1a T5 SHA256SUMS inventory and verify helpers (stdlib only).

Authority: MOTORE_V3_G1_CONTRACT @ 6d254d0a; PLAN debt
``contract_doc_sha256`` → T5 SHA256SUMS.

Self-hash policy
----------------
``ml_v3/fixtures/g1/SHA256SUMS`` does **not** list or hash itself. The
integrity anchor for the sums file is the immutable git commit that contains
it. Adding the sums path to the covered inventory is rejected fail-closed.
"""
from __future__ import annotations

from pathlib import Path

from .canonical import (
    CanonicalError,
    parse_sha256sums,
    sha256_of_file,
    sha256sums_text,
    validate_sha256sums_relpath,
)

__all__ = [
    "G1A_SHA256SUMS_RELPATH",
    "G1A_SHA256SUMS_COVERED",
    "G1A_SHA256SUMS_AUDIO_REQUIRED",
    "CONTRACT_DOC_SHA256_TRIPWIRE",
    "Sha256SumsError",
    "repo_root_from_here",
    "g1a_sha256sums_audio_required",
    "build_sha256sums_entries",
    "render_g1a_sha256sums",
    "write_g1a_sha256sums",
    "load_g1a_sha256sums",
    "verify_sha256sums_against_tree",
    "verify_g1a_sha256sums",
]

# Repo-root-relative path of the committed sums file (not self-hashed).
G1A_SHA256SUMS_RELPATH = "ml_v3/fixtures/g1/SHA256SUMS"

# Precomputed contract tripwire (PLAN durable debt; freeze @ 6d254d0a).
CONTRACT_DOC_SHA256_TRIPWIRE = (
    "6a6f6d35bbf3fc65d7a01e54620bf4f9649ea77d60c2b3d9e7ea0b72f7f49a86"
)

# Minimum G1a T5+M2+F3 coverage: contract doc + schema_registry_v1 (normative
# schema surface) + 6 example instance goldens + adapter + lock +
# fixture-spec v1 (path-canonical order). Audio WAV digests are *not*
# listed here — T6 binds them via ``g1a_sha256sums_audio_required()`` /
# ``G1A_SHA256SUMS_AUDIO_REQUIRED``.
# Note: fixtures/g1/examples/*.json are instance goldens, NOT schemas.
G1A_SHA256SUMS_COVERED: tuple[str, ...] = (
    "docs/MOTORE_V3_G1_CONTRACT.md",
    "ml_v3/fixtures/g1/adapter_v2_v3_mapping.json",
    "ml_v3/fixtures/g1/examples/admission_batch.json",
    "ml_v3/fixtures/g1/examples/annotation.json",
    "ml_v3/fixtures/g1/examples/asset_manifest.json",
    "ml_v3/fixtures/g1/examples/benchmark_power_plan.json",
    "ml_v3/fixtures/g1/examples/calibration_policy.json",
    "ml_v3/fixtures/g1/examples/prediction.json",
    "ml_v3/fixtures/g1/fixture_spec_v1.json",
    "ml_v3/fixtures/g1/metrology_lock.json",
    "ml_v3/fixtures/g1/schema_registry_v1.json",
)


class Sha256SumsError(ValueError):
    """Raised when SHA256SUMS inventory or tree verification fails."""


def repo_root_from_here() -> Path:
    """Return the repository root assuming this file lives under ml_v3/."""
    # ml_v3/contracts/sha256sums.py → parents[2] == repo root
    return Path(__file__).resolve().parents[2]


def g1a_sha256sums_audio_required() -> tuple[str, ...]:
    """Return every T6 fixture WAV relpath that verify must bind.

    Derived from ``iter_asset_specs()`` (lazy import avoids contracts↔fixtures
    import cycles). Alias concept: ``G1A_SHA256SUMS_AUDIO_REQUIRED``.
    """
    # Lazy: render_signals imports this module for update/verify helpers.
    from ml_v3.fixtures.g1.render_signals import asset_relpath, iter_asset_specs

    return tuple(
        asset_relpath(cat, stem, fs)
        for cat, stem, fs, _fn in iter_asset_specs()
    )


# Public name requested by MED harden; always call the function (fresh tuple).
G1A_SHA256SUMS_AUDIO_REQUIRED = g1a_sha256sums_audio_required


def build_sha256sums_entries(
    root: Path,
    relative_paths: tuple[str, ...] | list[str],
    *,
    forbid_self: str | None = G1A_SHA256SUMS_RELPATH,
) -> dict[str, str]:
    """Hash each relative path under ``root`` into a SHA256SUMS mapping."""
    root = Path(root)
    entries: dict[str, str] = {}
    for raw in relative_paths:
        rel = validate_sha256sums_relpath(raw)
        if forbid_self is not None and rel == forbid_self:
            raise Sha256SumsError(
                "SHA256SUMS must not hash itself; anchor is the immutable "
                f"commit containing {forbid_self!r}")
        absolute = root / rel
        if not absolute.is_file():
            raise Sha256SumsError(f"missing file for SHA256SUMS entry: {rel}")
        entries[rel] = sha256_of_file(absolute)
    if len(entries) != len(list(relative_paths)):
        raise Sha256SumsError("SHA256SUMS covered paths contain duplicates")
    return entries


def render_g1a_sha256sums(root: Path | None = None) -> str:
    """Render the G1a SHA256SUMS body from the current tree.

    If a committed SHA256SUMS exists, re-hash its full path inventory
    (minimum COVERED plus any T6 audio / later extensions). Otherwise
    bootstrap from ``G1A_SHA256SUMS_COVERED`` alone.
    """
    root = repo_root_from_here() if root is None else Path(root)
    sums_path = root / G1A_SHA256SUMS_RELPATH
    if sums_path.is_file():
        listed = tuple(parse_sha256sums(sums_path.read_text(encoding="utf-8")))
        # Ensure minimum coverage is always present even if a path was dropped.
        paths = tuple(dict.fromkeys((*G1A_SHA256SUMS_COVERED, *listed)))
    else:
        paths = G1A_SHA256SUMS_COVERED
    entries = build_sha256sums_entries(root, paths)
    contract_digest = entries["docs/MOTORE_V3_G1_CONTRACT.md"]
    if contract_digest != CONTRACT_DOC_SHA256_TRIPWIRE:
        raise Sha256SumsError(
            "contract_doc_sha256 tripwire mismatch: "
            f"got {contract_digest}, expected {CONTRACT_DOC_SHA256_TRIPWIRE}")
    return sha256sums_text(entries)


def write_g1a_sha256sums(root: Path | None = None) -> Path:
    """Write ``ml_v3/fixtures/g1/SHA256SUMS`` from the current tree."""
    root = repo_root_from_here() if root is None else Path(root)
    text = render_g1a_sha256sums(root)
    out = root / G1A_SHA256SUMS_RELPATH
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8", newline="\n")
    return out


def load_g1a_sha256sums(root: Path | None = None) -> dict[str, str]:
    """Load and parse the committed G1a SHA256SUMS file."""
    root = repo_root_from_here() if root is None else Path(root)
    path = root / G1A_SHA256SUMS_RELPATH
    if not path.is_file():
        raise Sha256SumsError(f"missing SHA256SUMS at {G1A_SHA256SUMS_RELPATH}")
    return parse_sha256sums(path.read_text(encoding="utf-8"))


def verify_sha256sums_against_tree(
    entries: dict[str, str],
    root: Path,
    *,
    forbid_self: str | None = G1A_SHA256SUMS_RELPATH,
) -> None:
    """Fail-closed: every entry must match ``sha256_of_file`` under root."""
    root = Path(root)
    if not entries:
        raise Sha256SumsError("SHA256SUMS entries empty")
    for rel, claimed in entries.items():
        validated = validate_sha256sums_relpath(rel)
        if forbid_self is not None and validated == forbid_self:
            raise Sha256SumsError(
                "SHA256SUMS must not hash itself; anchor is the immutable "
                f"commit containing {forbid_self!r}")
        absolute = root / validated
        if not absolute.is_file():
            raise Sha256SumsError(f"missing file for SHA256SUMS entry: {validated}")
        actual = sha256_of_file(absolute)
        if actual != claimed:
            raise Sha256SumsError(
                f"digest mismatch for {validated}: "
                f"sums={claimed} tree={actual}")


def verify_g1a_sha256sums(root: Path | None = None) -> dict[str, str]:
    """Verify committed G1a SHA256SUMS against the tree + minimum coverage.

    Fail-closed on stripped T6 audio: every ``iter_asset_specs()`` relpath
    must appear in the inventory (COVERED alone is not sufficient).
    """
    root = repo_root_from_here() if root is None else Path(root)
    entries = load_g1a_sha256sums(root)
    missing = [path for path in G1A_SHA256SUMS_COVERED if path not in entries]
    if missing:
        raise Sha256SumsError(
            f"SHA256SUMS missing required coverage: {missing}")
    audio_required = g1a_sha256sums_audio_required()
    missing_audio = [path for path in audio_required if path not in entries]
    if missing_audio:
        raise Sha256SumsError(
            "SHA256SUMS missing required audio inventory: "
            f"{missing_audio}")
    if G1A_SHA256SUMS_RELPATH in entries:
        raise Sha256SumsError(
            "SHA256SUMS must not hash itself; anchor is the immutable commit "
            f"containing {G1A_SHA256SUMS_RELPATH!r}")
    contract = entries.get("docs/MOTORE_V3_G1_CONTRACT.md")
    if contract != CONTRACT_DOC_SHA256_TRIPWIRE:
        raise Sha256SumsError(
            "contract_doc_sha256 tripwire mismatch: "
            f"got {contract}, expected {CONTRACT_DOC_SHA256_TRIPWIRE}")
    verify_sha256sums_against_tree(entries, root)
    # Round-trip the committed bytes through the canonical renderer.
    try:
        rerendered = sha256sums_text(entries)
    except CanonicalError as exc:
        raise Sha256SumsError(str(exc)) from exc
    committed = (root / G1A_SHA256SUMS_RELPATH).read_text(encoding="utf-8")
    if rerendered != committed:
        raise Sha256SumsError(
            "committed SHA256SUMS is not canonical sha256sums_text(entries)")
    if parse_sha256sums(rerendered) != entries:
        raise Sha256SumsError("SHA256SUMS parse/render round-trip failed")
    return entries
```

---

## B.39 FILE: `ml_v3/contracts/fixture_spec.py`

**Path logico:** `ml_v3/contracts/fixture_spec.py`  
**Bytes:** 37333  
**Lines:** 882

```python
"""Frozen G1a fixture-spec v1 (§13 signal-generator parameters).

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a.

G1a M2 serializes the implementative freedoms left open by §13 prose
(amplitudes, phases, envelopes, sweep law, stereo layouts, duration,
numeric generation semantics). It does **not** generate audio, does not
consult frontend spike results, and does not amend REV6 (no REV7).

Honesty / envelope_authority:
  §13 lists fixture *categories* and fully pins only part of the
  pseudo-noise recipe in prose. Keys below are freeze-from-prose
  packaging of those freedoms (conservative lab defaults), plus the
  byte-level generation semantics required for bit-identical renders.
  Duration is a PREREGISTERED parameter — not a false formula
  ``warm_up + coda + T_min`` (T_min belongs to diagnostic mode (b);
  SR-parity multitone/noise closes under mode (a) only).

Sequencing: commit+hash this artifact **before** M3/T6 generators.
"""
from __future__ import annotations

import math
from copy import deepcopy
from fractions import Fraction
from typing import Any

from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import (
    CANONICAL_SAMPLE_RATE,
    CONTRACT_REVISION,
    GATE_SAMPLE_RATES,
)
from .metrology_lock import (
    HOP_SAMPLES,
    N_LF,
    SWEEP_CHECKPOINT_HZ,
    coda_seconds,
    metrology_lock_sha256,
    resampler_group_delay_rational,
    warm_up_seconds,
)

__all__ = [
    "FixtureSpecError",
    "FIXTURE_SPEC_ARTIFACT_ID",
    "FIXTURE_SPEC_SECTION",
    "FIXTURE_SPEC_VERSION",
    "DURATION_NUM",
    "DURATION_DEN",
    "PSEUDO_NOISE_PRNG_SEED",
    "MULTITONE_HZ",
    "ULTRASONIC_HZ",
    "SWEEP_F_START_HZ",
    "SWEEP_F_END_HZ",
    "SWEEP_T_START_NUM",
    "SWEEP_T_START_DEN",
    "SWEEP_T_END_NUM",
    "SWEEP_T_END_DEN",
    "frozen_fixture_spec",
    "fixture_spec_bytes",
    "fixture_spec_sha256",
    "validate_fixture_spec_claim",
    "duration_seconds",
    "sample_count",
    "sweep_active_start_seconds",
    "sweep_active_end_seconds",
    "sweep_crossing_time",
    "common_useful_window",
    "nearest_useful_frame_distance",
    "assert_sweep_checkpoints_reachable",
]

FIXTURE_SPEC_ARTIFACT_ID = "aieq-v3-fixture-spec-1"
FIXTURE_SPEC_SECTION = "13"
FIXTURE_SPEC_VERSION = 1

# Preregistered global duration (exact rational). N(fs) = duration_num * fs
# is integer for every GATE_SAMPLE_RATES entry.
DURATION_NUM: int = 2
DURATION_DEN: int = 1

PSEUDO_NOISE_PRNG_SEED: int = 31051986
PSEUDO_NOISE_PARTIALS: int = 512
PSEUDO_NOISE_F_LO_HZ: float = 20.0
PSEUDO_NOISE_F_HI_HZ: float = 20000.0
PSEUDO_NOISE_RMS_DBFS: float = -24.0

MULTITONE_HZ: tuple[int, ...] = (
    45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000,
)
ULTRASONIC_HZ: tuple[int, ...] = (28000, 32000, 40000)
SWEEP_F_START_HZ: float = 20.0
SWEEP_F_END_HZ: float = 20000.0

# Active log-sweep interval inside the common useful window (exact rationals).
# Global asset stays 0..2 s; only the chirp occupies [t_start, t_end].
# 1/2 and 7/4 are integer sample counts at every GATE_SAMPLE_RATES entry.
SWEEP_T_START_NUM: int = 1
SWEEP_T_START_DEN: int = 2
SWEEP_T_END_NUM: int = 7
SWEEP_T_END_DEN: int = 4

# Lock rationals used only to document duration bounds (not to derive it).
_MAX_WARM_UP_PLUS_CODA_NUM: int = 18896  # 44100: wu+coda = 18896/55125
_MAX_WARM_UP_PLUS_CODA_DEN: int = 55125
_CODA_NUM: int = 32
_CODA_DEN: int = 375


class FixtureSpecError(ValueError):
    """Raised when a fixture-spec claim is malformed or non-canonical."""


def duration_seconds() -> float:
    return DURATION_NUM / DURATION_DEN


def sample_count(fs: int) -> int:
    """Exact sample count at gate rate ``fs`` (no rounding ambiguity)."""
    if fs not in GATE_SAMPLE_RATES:
        raise FixtureSpecError(
            f"fs {fs} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")
    if DURATION_DEN != 1:
        if (DURATION_NUM * fs) % DURATION_DEN != 0:
            raise FixtureSpecError(
                f"duration {DURATION_NUM}/{DURATION_DEN} is not an integer "
                f"number of samples at fs={fs}")
        return (DURATION_NUM * fs) // DURATION_DEN
    return DURATION_NUM * fs


def sweep_active_start_seconds() -> float:
    return SWEEP_T_START_NUM / SWEEP_T_START_DEN


def sweep_active_end_seconds() -> float:
    return SWEEP_T_END_NUM / SWEEP_T_END_DEN


def common_useful_window() -> tuple[float, float]:
    """Cross-SR useful source-time window (after max warm-up, before coda)."""
    start = max(warm_up_seconds(fs) for fs in GATE_SAMPLE_RATES)
    end = duration_seconds() - coda_seconds()
    return start, end


def sweep_crossing_time(freq_hz: float) -> float:
    """Source time at which the active log-sweep crosses ``freq_hz``."""
    if freq_hz <= 0.0:
        raise FixtureSpecError("sweep checkpoint frequency must be positive")
    t0 = sweep_active_start_seconds()
    t1 = sweep_active_end_seconds()
    if t1 <= t0:
        raise FixtureSpecError("sweep active end must exceed active start")
    ratio = SWEEP_F_END_HZ / SWEEP_F_START_HZ
    # f(t) = f_start * ratio**u, u=(t-t0)/(t1-t0)  →  u = log(f/f_start)/log(ratio)
    u = math.log(freq_hz / SWEEP_F_START_HZ) / math.log(ratio)
    return t0 + u * (t1 - t0)


def _hop_seconds() -> float:
    return HOP_SAMPLES / float(CANONICAL_SAMPLE_RATE)


def _useful_frame_times(fs: int) -> list[float]:
    """V3FeatureFrame ``source_time`` grid inside the useful segment (§6.2+§5).

    Contractual timeline (not a synthetic warm-up-origin hop lattice):

      frame_end_sample = N_LF + m·H
      source_time = frame_end_sample / fs_c − resampler_group_delay_seconds(fs)

    ``warm_up_seconds(fs)`` is an *exclusion threshold* (with coda); it is not
    the grid origin. Rationals are kept until the final float compare/list.
    """
    if fs not in GATE_SAMPLE_RATES:
        raise FixtureSpecError(
            f"fs {fs} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")
    delay_num, delay_den, _ = resampler_group_delay_rational(fs)
    delay = Fraction(delay_num, delay_den)
    warm_up = warm_up_seconds(fs)
    end = duration_seconds() - coda_seconds()
    frame_end = N_LF
    times: list[float] = []
    while True:
        source_time = Fraction(frame_end, CANONICAL_SAMPLE_RATE) - delay
        t = float(source_time)
        if t > end:
            break
        if t >= warm_up:
            times.append(t)
        frame_end += HOP_SAMPLES
    return times


def nearest_useful_frame_distance(t_cross: float, fs: int) -> float:
    """Distance from ``t_cross`` to nearest useful V3FeatureFrame source_time."""
    frames = _useful_frame_times(fs)
    if not frames:
        raise FixtureSpecError(f"no useful frames at fs={fs}")
    return min(abs(t - t_cross) for t in frames)


def assert_sweep_checkpoints_reachable() -> None:
    """Fail closed if any lock checkpoint is unreachable under match radius.

    Contract requirement (metrology lock sweep_log_parity): for every
    checkpoint frequency, the nearest useful-segment frame at each gate
    rate must lie within ``H / fs_c`` of the crossing time. Crossing times
    must also fall inside the common useful window.
    """
    useful_start, useful_end = common_useful_window()
    t0 = sweep_active_start_seconds()
    t1 = sweep_active_end_seconds()
    hop = _hop_seconds()

    if not (useful_start <= t0 < t1 <= useful_end):
        raise FixtureSpecError(
            f"sweep active interval [{t0}, {t1}] must lie inside common "
            f"useful window [{useful_start}, {useful_end}]")

    for fs in GATE_SAMPLE_RATES:
        # Active bounds must be exact sample indices (no fractional sample).
        n0 = t0 * fs
        n1 = t1 * fs
        if abs(n0 - round(n0)) > 1e-9 or abs(n1 - round(n1)) > 1e-9:
            raise FixtureSpecError(
                f"sweep active bounds must be sample-exact at fs={fs} "
                f"(t_start*fs={n0}, t_end*fs={n1})")

    for freq in SWEEP_CHECKPOINT_HZ:
        t_cross = sweep_crossing_time(float(freq))
        if not (useful_start <= t_cross <= useful_end):
            raise FixtureSpecError(
                f"checkpoint {freq} Hz crosses at t={t_cross} outside "
                f"common useful window [{useful_start}, {useful_end}]")
        for fs in GATE_SAMPLE_RATES:
            dist = nearest_useful_frame_distance(t_cross, fs)
            if dist > hop + 1e-12:
                raise FixtureSpecError(
                    f"checkpoint {freq} Hz at t={t_cross}: nearest useful "
                    f"frame at fs={fs} is {dist} s away (match radius "
                    f"H/fs_c={hop})")


def _peak_amp_equal_power(n: int, rms_dbfs: float = PSEUDO_NOISE_RMS_DBFS) -> float:
    """Peak amplitude per equal-power partial for target aggregate RMS."""
    if n <= 0:
        raise FixtureSpecError("n partials must be positive")
    return (10.0 ** (rms_dbfs / 20.0)) * math.sqrt(2.0 / n)


def _dbfs_amplitude_linear(dbfs: float = PSEUDO_NOISE_RMS_DBFS) -> float:
    """Linear amplitude for a declared dBFS level (not sine-RMS conversion).

    For log_sweep, the fixture deliberately preregisters
    ``amplitude_peak = 10**(-24/20)`` (peak = −24 dBFS), not the RMS-of-sine
    convention peak = 10**(-24/20)*sqrt(2).
    """
    return 10.0 ** (dbfs / 20.0)


def frozen_fixture_spec() -> dict[str, Any]:
    """Return a deep copy of the frozen §13 fixture-spec v1 artifact."""
    # Self-check: never serialize a sweep that cannot close the lock gate.
    assert_sweep_checkpoints_reachable()

    peak_noise = _peak_amp_equal_power(PSEUDO_NOISE_PARTIALS)
    peak_multitone = _peak_amp_equal_power(len(MULTITONE_HZ))
    peak_ultra = _peak_amp_equal_power(len(ULTRASONIC_HZ))
    # Deliberate packaging: peak amplitude = −24 dBFS (not RMS-of-sine).
    peak_tone = _dbfs_amplitude_linear()

    max_wu = max(warm_up_seconds(fs) for fs in GATE_SAMPLE_RATES)
    coda = _CODA_NUM / _CODA_DEN
    useful = duration_seconds() - max_wu - coda
    useful_start, useful_end = common_useful_window()
    t_start = sweep_active_start_seconds()
    t_end = sweep_active_end_seconds()
    t_active = t_end - t_start
    hop = _hop_seconds()
    checkpoint_crossings = {
        str(freq): sweep_crossing_time(float(freq))
        for freq in SWEEP_CHECKPOINT_HZ
    }

    return {
        "artifact_id": FIXTURE_SPEC_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": FIXTURE_SPEC_SECTION,
        "spec_version": FIXTURE_SPEC_VERSION,
        "envelope_authority": (
            "freeze-from-prose packaging of §13 fixture freedoms "
            "(amplitudes/phases/envelopes/sweep law/stereo/duration/"
            "numeric generation semantics); contract has no literal JSON "
            "key table for this artifact; NO REV7 — packaging only; "
            "does not implement audio generators (M3/T6 after CC+commit)"
        ),
        "no_audio_generators_in_this_artifact": True,
        "dependencies": {
            "contract_revision": CONTRACT_REVISION,
            "metrology_lock_sha256": metrology_lock_sha256(),
            "gate_sample_rates": list(GATE_SAMPLE_RATES),
        },
        "numeric_generation": {
            "sample_index_rule": "n = 0 .. N-1",
            "time_rule": "t = n / fs  (float64 division)",
            "N_rule": "N = duration_num * fs / duration_den (exact integer)",
            "compute_dtype": "float64",
            "artifact_dtype": "float32",
            "sum_order": (
                "ascending partial index k (or ascending frequency list "
                "order); accumulate in float64 before cast"
            ),
            "prng": "numpy.random.Generator(numpy.random.PCG64(seed))",
            "prng_draw_order": (
                "sequential rng.random() calls in ascending k; "
                "never reverse, never vector-size ambiguity vs loop"
            ),
            "post_render_normalization": "forbidden",
            "dc_removal": "forbidden",
            "cast_policy": (
                "after full float64 render, cast each sample to IEEE754 "
                "binary32 round-to-nearest-even (language/numpy default "
                "float64→float32); no dither"
            ),
            "cross_rate_resample": "forbidden",
            "render_policy": (
                "generate DIRECTLY at each of 44100/48000/96000; never "
                "resample one rate to another"
            ),
            "file_format": {
                "container": "wav",
                "encoding": "pcm_float32_le",
                "byte_order": "little_endian",
                "channels_layout": "interleaved_LRLR_for_stereo",
                "header_sample_rate_matches_render_fs": True,
                "non_finite_encoding": (
                    "same WAV PCM float32 LE container; inject IEEE754 "
                    "binary32 NaN/+Inf/-Inf bit patterns at pinned samples"
                ),
            },
        },
        "global_duration": {
            "kind": "preregistered_parameter",
            "not_a_contract_formula": True,
            "false_formula_forbidden": "warm_up + coda + T_min",
            "duration_num": DURATION_NUM,
            "duration_den": DURATION_DEN,
            "duration_s": duration_seconds(),
            "N_at_gate_rates": {
                str(fs): sample_count(fs) for fs in GATE_SAMPLE_RATES
            },
            "bound_check": {
                "must_exceed": "max_sr(warm_up_sr + coda_sr)",
                "max_warm_up_plus_coda_num": _MAX_WARM_UP_PLUS_CODA_NUM,
                "max_warm_up_plus_coda_den": _MAX_WARM_UP_PLUS_CODA_DEN,
                "max_warm_up_plus_coda_s": (
                    _MAX_WARM_UP_PLUS_CODA_NUM / _MAX_WARM_UP_PLUS_CODA_DEN
                ),
                "coda_num": _CODA_NUM,
                "coda_den": _CODA_DEN,
                "useful_portion_s_at_max_wu": useful,
                "useful_portion_hops_at_fs_c": useful / (HOP_SAMPLES / 48000.0),
                "mode_a_gate_closing": True,
                "rationale": (
                    "conservative lab default: duration_s=2 > "
                    "max(warm_up+coda)≈0.3428 s (lock rationals "
                    "18896/55125) so mode-(a) useful segment is long "
                    "enough (~77 hops @ fs_c) for SR-parity multitone/"
                    "noise; T_min=8H/fs_c is mode-(b) diagnostic only "
                    "and must not define duration"
                ),
            },
        },
        "categories": {
            "multitone": {
                "frequencies_hz": list(MULTITONE_HZ),
                "amplitude_peak_each": peak_multitone,
                "amplitude_formula": (
                    "10**(-24/20) * sqrt(2/N) with N=len(frequencies_hz); "
                    "equal-power packaging aligned with §13 pseudo-noise"
                ),
                "phases_rad": [0.0] * len(MULTITONE_HZ),
                "phase_policy": "all_zero_deterministic",
                "channels": 1,
                "duration_ref": "global_duration",
                "waveform": (
                    "sum_k A * sin(2*pi*f_k*t + phi_k) in float64; "
                    "k ascending in frequencies_hz order"
                ),
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "log_sweep": {
                "f_start_hz": SWEEP_F_START_HZ,
                "f_end_hz": SWEEP_F_END_HZ,
                "amplitude_peak": peak_tone,
                "amplitude_formula": (
                    "10**(-24/20) preregistered peak amplitude "
                    "(peak = −24 dBFS; not RMS-of-sine * sqrt(2))"
                ),
                "phase_at_active_start_rad": 0.0,
                "sweep_law": "exponential_log_chirp",
                "active_interval": {
                    "kind": "preregistered_parameter",
                    "t_start_num": SWEEP_T_START_NUM,
                    "t_start_den": SWEEP_T_START_DEN,
                    "t_end_num": SWEEP_T_END_NUM,
                    "t_end_den": SWEEP_T_END_DEN,
                    "t_start_s": t_start,
                    "t_end_s": t_end,
                    "T_active_s": t_active,
                    "outside_active_sample": 0.0,
                    "rationale": (
                        "global asset remains duration_s=2; chirp occupies "
                        "only [t_start, t_end] inside the common useful "
                        "window so every SWEEP_CHECKPOINT_HZ crossing is "
                        "reachable within match radius H/fs_c"
                    ),
                },
                "checkpoint_reachability": {
                    "common_useful_start_s": useful_start,
                    "common_useful_end_s": useful_end,
                    "match_radius_s": hop,
                    "match_radius_formula": "H / fs_c",
                    "checkpoint_hz": list(SWEEP_CHECKPOINT_HZ),
                    "crossing_time_s": checkpoint_crossings,
                    "rule": (
                        "forall f in checkpoint_hz: "
                        "common_useful_start <= t_cross(f) <= "
                        "common_useful_end AND forall fs in "
                        "gate_sample_rates: "
                        "nearest_useful_frame_distance(t_cross, fs) "
                        "<= H/fs_c"
                    ),
                },
                "instantaneous_freq_hz": (
                    "for t in [t_start, t_end]: "
                    "f(t) = f_start * (f_end/f_start)**u with "
                    "u=(t-t_start)/T_active, T_active=t_end-t_start; "
                    "outside active interval: undefined (sample=0)"
                ),
                "phase_integral_rad": (
                    "for t in [t_start, t_end]: "
                    "phi(t) = 2*pi * f_start * T_active / ln(f_end/f_start) * "
                    "((f_end/f_start)**u - 1), u=(t-t_start)/T_active; "
                    "phi(t_start)=0"
                ),
                "sample": (
                    "for t in [t_start, t_end]: A * sin(phi(t)); "
                    "else 0; A=amplitude_peak"
                ),
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "transient_burst": {
                "onset_s": 0.5,
                "peak_amplitude": 0.5,
                "envelope": "one_sided_exponential",
                "tau_s": 0.010,
                "carrier": "none_impulse_like",
                "sample": (
                    "for t < onset: 0; else peak_amplitude * "
                    "exp(-(t-onset)/tau_s)"
                ),
                "repetition_count": 1,
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
                "note": (
                    "excluded from dB spectral SR-parity; onset/peak/"
                    "decay gate per §13.2"
                ),
            },
            "damped_resonance": {
                "center_hz": 1000.0,
                "onset_s": 0.5,
                "peak_amplitude": 0.25,
                "envelope": "exponential_decay",
                "tau_s": 0.050,
                "excitation": "impulse_at_onset",
                "phase_at_onset_rad": 0.0,
                "sample": (
                    "for t < onset: 0; else peak_amplitude * "
                    "exp(-(t-onset)/tau_s) * "
                    "sin(2*pi*center_hz*(t-onset) + phase_at_onset_rad)"
                ),
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
                "note": (
                    "excluded from dB spectral SR-parity; onset/peak/"
                    "decay gate per §13.2"
                ),
            },
            "ultrasonic_96k": {
                "frequencies_hz": list(ULTRASONIC_HZ),
                "presentation": "simultaneous",
                "amplitude_peak_each": peak_ultra,
                "amplitude_formula": (
                    "10**(-24/20) * sqrt(2/N) with N=3"
                ),
                "phases_rad": [0.0, 0.0, 0.0],
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": [96000],
                "waveform": (
                    "sum_k A * sin(2*pi*f_k*t + phi_k) in float64; "
                    "k ascending"
                ),
            },
            "decorrelated_stereo": {
                "layouts": {
                    "mid_only": {
                        "base": "multitone",
                        "left": "s",
                        "right": "s",
                        "ms_convention": "mid=(L+R)/2, side=(L-R)/2",
                        "implied": "mid=s, side=0",
                    },
                    "side_only": {
                        "base": "multitone",
                        "left": "s",
                        "right": "-s",
                        "ms_convention": "mid=(L+R)/2, side=(L-R)/2",
                        "implied": "mid=0, side=s",
                    },
                    "decorrelated": {
                        "base": "pseudo_noise",
                        "method": "independent_pcg64_phase_per_channel",
                        "left_prng_seed": PSEUDO_NOISE_PRNG_SEED,
                        "right_prng_seed": PSEUDO_NOISE_PRNG_SEED + 1,
                        "draw_order": (
                            "render left with left seed (k=0..511), then "
                            "right with right seed (k=0..511); identical "
                            "freqs/amplitudes as mono pseudo_noise"
                        ),
                        "ms_convention": "mid=(L+R)/2, side=(L-R)/2",
                    },
                },
                "channels": 2,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "pseudo_noise": {
                "n_partials": PSEUDO_NOISE_PARTIALS,
                "k_range": [0, PSEUDO_NOISE_PARTIALS - 1],
                "k_order": "ascending",
                "freq_hz_formula": (
                    "20 + (20000-20)*(k+0.5)/512"
                ),
                "f_lo_hz": PSEUDO_NOISE_F_LO_HZ,
                "f_hi_hz": PSEUDO_NOISE_F_HI_HZ,
                "rms_dbfs": PSEUDO_NOISE_RMS_DBFS,
                "amplitude_peak_each": peak_noise,
                "amplitude_formula": (
                    "10**(-24/20) * sqrt(2/512)"
                ),
                "prng": "PCG64",
                "prng_seed": PSEUDO_NOISE_PRNG_SEED,
                "phase_rule": (
                    "rng = Generator(PCG64(31051986)); "
                    "phase[k] = 2*pi*rng.random() for k=0..511 ascending"
                ),
                "phase_unit": "radians",
                "waveform": (
                    "sum_{k=0..511} A * sin(2*pi*freq[k]*t + phase[k]) "
                    "in float64; cast float32; no renormalize"
                ),
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "silence_non_finite": {
                "silence": {
                    "value": 0.0,
                    "channels": 1,
                    "duration_ref": "global_duration",
                    "gate_sample_rates": list(GATE_SAMPLE_RATES),
                },
                "non_finite_patterns": [
                    {
                        "id": "nan_at_sample_0",
                        "inject": "NaN",
                        "sample_index": 0,
                        "base": "silence",
                        "channels": 1,
                        "duration_ref": "global_duration",
                    },
                    {
                        "id": "pos_inf_at_sample_0",
                        "inject": "+Inf",
                        "sample_index": 0,
                        "base": "silence",
                        "channels": 1,
                        "duration_ref": "global_duration",
                    },
                    {
                        "id": "neg_inf_at_sample_0",
                        "inject": "-Inf",
                        "sample_index": 0,
                        "base": "silence",
                        "channels": 1,
                        "duration_ref": "global_duration",
                    },
                ],
                "purpose": "fail-closed gate inputs (§13)",
            },
        },
    }


def fixture_spec_bytes() -> bytes:
    """Canonical JSON bytes of the frozen fixture-spec (final LF included)."""
    return canonical_bytes(frozen_fixture_spec())


def fixture_spec_sha256() -> str:
    """SHA-256 of the canonical frozen fixture-spec artifact."""
    return sha256_of_obj(frozen_fixture_spec())


def _require_exact(claim: dict[str, Any], key: str, expected: object) -> None:
    value = claim.get(key)
    if value != expected:
        raise FixtureSpecError(
            f"fixture-spec claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def validate_fixture_spec_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed fixture-spec v1 artifact.

    Rejects mutated duration, weakened numeric semantics, pseudo-noise
    PRNG/phase unpinning, false warm_up+coda+T_min duration formula,
    cross-rate resample allowance, missing categories, wrong metrology
    dependency digest, and any claim not byte-identical to the frozen
    spec.
    """
    if not isinstance(claim, dict):
        raise FixtureSpecError(
            f"fixture-spec claim must be an object, got "
            f"{type(claim).__name__}")

    frozen = frozen_fixture_spec()
    _require_exact(claim, "artifact_id", FIXTURE_SPEC_ARTIFACT_ID)
    _require_exact(claim, "contract_revision", CONTRACT_REVISION)
    _require_exact(claim, "section", FIXTURE_SPEC_SECTION)
    _require_exact(claim, "spec_version", FIXTURE_SPEC_VERSION)

    if claim.get("no_audio_generators_in_this_artifact") is not True:
        raise FixtureSpecError(
            "no_audio_generators_in_this_artifact must be true "
            "(M2 freezes params only; generators are M3/T6)")

    env = claim.get("envelope_authority")
    if not isinstance(env, str) or "freeze-from-prose" not in env:
        raise FixtureSpecError(
            "envelope_authority must declare freeze-from-prose packaging")
    if "NO REV7" not in env and "no REV7" not in env.lower():
        raise FixtureSpecError(
            "envelope_authority must state NO REV7 (packaging only)")

    deps = claim.get("dependencies")
    if not isinstance(deps, dict):
        raise FixtureSpecError("dependencies must be an object")
    if deps.get("contract_revision") != CONTRACT_REVISION:
        raise FixtureSpecError(
            "dependencies.contract_revision must equal CONTRACT_REVISION")
    expected_lock = metrology_lock_sha256()
    if deps.get("metrology_lock_sha256") != expected_lock:
        raise FixtureSpecError(
            "dependencies.metrology_lock_sha256 must equal current "
            f"metrology_lock_sha256() ({expected_lock})")
    if deps.get("gate_sample_rates") != list(GATE_SAMPLE_RATES):
        raise FixtureSpecError(
            f"dependencies.gate_sample_rates must be {list(GATE_SAMPLE_RATES)}")

    numeric = claim.get("numeric_generation")
    if not isinstance(numeric, dict):
        raise FixtureSpecError("numeric_generation must be an object")
    if numeric.get("compute_dtype") != "float64":
        raise FixtureSpecError("compute_dtype must be float64")
    if numeric.get("artifact_dtype") != "float32":
        raise FixtureSpecError("artifact_dtype must be float32")
    if numeric.get("post_render_normalization") != "forbidden":
        raise FixtureSpecError(
            "post_render_normalization must be 'forbidden'")
    if numeric.get("cross_rate_resample") != "forbidden":
        raise FixtureSpecError("cross_rate_resample must be 'forbidden'")
    if "DIRECTLY" not in str(numeric.get("render_policy", "")):
        raise FixtureSpecError(
            "render_policy must require DIRECT generation at gate rates")
    fmt = numeric.get("file_format")
    if not isinstance(fmt, dict):
        raise FixtureSpecError("numeric_generation.file_format must be an object")
    if fmt.get("container") != "wav" or fmt.get("encoding") != "pcm_float32_le":
        raise FixtureSpecError(
            "file_format must be wav / pcm_float32_le")

    duration = claim.get("global_duration")
    if not isinstance(duration, dict):
        raise FixtureSpecError("global_duration must be an object")
    if duration.get("kind") != "preregistered_parameter":
        raise FixtureSpecError(
            "global_duration.kind must be 'preregistered_parameter'")
    if duration.get("not_a_contract_formula") is not True:
        raise FixtureSpecError(
            "global_duration.not_a_contract_formula must be true")
    if duration.get("false_formula_forbidden") != "warm_up + coda + T_min":
        raise FixtureSpecError(
            "false_formula_forbidden must record warm_up + coda + T_min")
    if duration.get("duration_num") != DURATION_NUM:
        raise FixtureSpecError(
            f"duration_num must be {DURATION_NUM}")
    if duration.get("duration_den") != DURATION_DEN:
        raise FixtureSpecError(
            f"duration_den must be {DURATION_DEN}")
    if duration.get("duration_s") != duration_seconds():
        raise FixtureSpecError(
            f"duration_s must be {duration_seconds()}")
    bound = duration.get("bound_check")
    if not isinstance(bound, dict):
        raise FixtureSpecError("global_duration.bound_check must be an object")
    if bound.get("max_warm_up_plus_coda_num") != _MAX_WARM_UP_PLUS_CODA_NUM:
        raise FixtureSpecError(
            "bound_check must use lock rational max wu+coda 18896/55125")
    if bound.get("max_warm_up_plus_coda_den") != _MAX_WARM_UP_PLUS_CODA_DEN:
        raise FixtureSpecError(
            "bound_check max_warm_up_plus_coda_den must be 55125")
    if duration_seconds() <= (
            _MAX_WARM_UP_PLUS_CODA_NUM / _MAX_WARM_UP_PLUS_CODA_DEN):
        raise FixtureSpecError(
            "duration_s must exceed max(warm_up_sr + coda_sr)")

    cats = claim.get("categories")
    if not isinstance(cats, dict):
        raise FixtureSpecError("categories must be an object")
    required = {
        "multitone",
        "log_sweep",
        "transient_burst",
        "damped_resonance",
        "ultrasonic_96k",
        "decorrelated_stereo",
        "pseudo_noise",
        "silence_non_finite",
    }
    missing = required - set(cats)
    if missing:
        raise FixtureSpecError(f"categories missing required keys: {sorted(missing)}")

    multi = cats.get("multitone")
    if not isinstance(multi, dict):
        raise FixtureSpecError("categories.multitone must be an object")
    if multi.get("frequencies_hz") != list(MULTITONE_HZ):
        raise FixtureSpecError(
            "multitone.frequencies_hz must match §13 critical centres")

    sweep = cats.get("log_sweep")
    if not isinstance(sweep, dict):
        raise FixtureSpecError("categories.log_sweep must be an object")
    if sweep.get("sweep_law") != "exponential_log_chirp":
        raise FixtureSpecError(
            "log_sweep.sweep_law must be 'exponential_log_chirp'")
    if "ln(f_end/f_start)" not in str(sweep.get("phase_integral_rad", "")):
        raise FixtureSpecError(
            "log_sweep.phase_integral_rad must pin the log-chirp integral")
    if "T_active" not in str(sweep.get("instantaneous_freq_hz", "")):
        raise FixtureSpecError(
            "log_sweep.instantaneous_freq_hz must pin active-interval "
            "T_active (not full-asset duration)")
    active = sweep.get("active_interval")
    if not isinstance(active, dict):
        raise FixtureSpecError("log_sweep.active_interval must be an object")
    if active.get("t_start_num") != SWEEP_T_START_NUM:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_start_num must be {SWEEP_T_START_NUM}")
    if active.get("t_start_den") != SWEEP_T_START_DEN:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_start_den must be {SWEEP_T_START_DEN}")
    if active.get("t_end_num") != SWEEP_T_END_NUM:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_end_num must be {SWEEP_T_END_NUM}")
    if active.get("t_end_den") != SWEEP_T_END_DEN:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_end_den must be {SWEEP_T_END_DEN}")
    if active.get("t_start_s") != sweep_active_start_seconds():
        raise FixtureSpecError(
            "log_sweep.active_interval.t_start_s must equal "
            f"{sweep_active_start_seconds()}")
    if active.get("t_end_s") != sweep_active_end_seconds():
        raise FixtureSpecError(
            "log_sweep.active_interval.t_end_s must equal "
            f"{sweep_active_end_seconds()}")
    if active.get("outside_active_sample") != 0.0:
        raise FixtureSpecError(
            "log_sweep.active_interval.outside_active_sample must be 0.0")
    reach = sweep.get("checkpoint_reachability")
    if not isinstance(reach, dict):
        raise FixtureSpecError(
            "log_sweep.checkpoint_reachability must be an object")
    if reach.get("checkpoint_hz") != list(SWEEP_CHECKPOINT_HZ):
        raise FixtureSpecError(
            "checkpoint_reachability.checkpoint_hz must equal "
            "metrology lock SWEEP_CHECKPOINT_HZ")
    if "nearest_useful_frame_distance" not in str(reach.get("rule", "")):
        raise FixtureSpecError(
            "checkpoint_reachability.rule must require "
            "nearest_useful_frame_distance <= H/fs_c")
    # Structural self-check against current lock timing (not claim-trusted).
    assert_sweep_checkpoints_reachable()

    noise = cats.get("pseudo_noise")
    if not isinstance(noise, dict):
        raise FixtureSpecError("categories.pseudo_noise must be an object")
    if noise.get("prng_seed") != PSEUDO_NOISE_PRNG_SEED:
        raise FixtureSpecError(
            f"pseudo_noise.prng_seed must be {PSEUDO_NOISE_PRNG_SEED}")
    if noise.get("n_partials") != PSEUDO_NOISE_PARTIALS:
        raise FixtureSpecError("pseudo_noise.n_partials must be 512")
    phase_rule = str(noise.get("phase_rule", ""))
    if "PCG64(31051986)" not in phase_rule:
        raise FixtureSpecError(
            "pseudo_noise.phase_rule must pin Generator(PCG64(31051986))")
    if "k=0..511" not in phase_rule:
        raise FixtureSpecError(
            "pseudo_noise.phase_rule must pin k=0..511 ascending draws")
    if "2*pi*rng.random()" not in phase_rule:
        raise FixtureSpecError(
            "pseudo_noise.phase_rule must pin phase[k]=2*pi*rng.random()")

    ultra = cats.get("ultrasonic_96k")
    if not isinstance(ultra, dict):
        raise FixtureSpecError("categories.ultrasonic_96k must be an object")
    if ultra.get("frequencies_hz") != list(ULTRASONIC_HZ):
        raise FixtureSpecError(
            "ultrasonic_96k.frequencies_hz must be [28000,32000,40000]")
    if ultra.get("presentation") != "simultaneous":
        raise FixtureSpecError(
            "ultrasonic_96k.presentation must be 'simultaneous'")
    if ultra.get("gate_sample_rates") != [96000]:
        raise FixtureSpecError(
            "ultrasonic_96k.gate_sample_rates must be [96000] only")

    stereo = cats.get("decorrelated_stereo")
    if not isinstance(stereo, dict):
        raise FixtureSpecError("categories.decorrelated_stereo must be an object")
    layouts = stereo.get("layouts")
    if not isinstance(layouts, dict):
        raise FixtureSpecError("decorrelated_stereo.layouts must be an object")
    for key in ("mid_only", "side_only", "decorrelated"):
        if key not in layouts:
            raise FixtureSpecError(
                f"decorrelated_stereo.layouts missing {key!r}")
    deco = layouts.get("decorrelated")
    if not isinstance(deco, dict):
        raise FixtureSpecError("layouts.decorrelated must be an object")
    if deco.get("method") != "independent_pcg64_phase_per_channel":
        raise FixtureSpecError(
            "decorrelated method must be independent_pcg64_phase_per_channel")
    if deco.get("left_prng_seed") != PSEUDO_NOISE_PRNG_SEED:
        raise FixtureSpecError("decorrelated left_prng_seed must be 31051986")
    if deco.get("right_prng_seed") != PSEUDO_NOISE_PRNG_SEED + 1:
        raise FixtureSpecError("decorrelated right_prng_seed must be 31051987")

    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise FixtureSpecError(
            f"claim is not canonically serializable: {exc}") from exc

    frozen_digest = sha256_of_obj(frozen)
    if claim_digest != frozen_digest:
        raise FixtureSpecError(
            "fixture-spec claim is not byte-identical to the frozen §13 "
            f"spec (claim_sha256={claim_digest}, "
            f"frozen_sha256={frozen_digest})")

    return deepcopy(claim)
```

---

## B.40 FILE: `ml_v3/fixtures/__init__.py`

**Path logico:** `ml_v3/fixtures/__init__.py`  
**Bytes:** 79  
**Lines:** 1

```python
"""Motore v3 fixture lab package (G1a T6 signal generators + audio assets)."""
```

---

## B.41 FILE: `ml_v3/fixtures/g1/render_signals.py`

**Path logico:** `ml_v3/fixtures/g1/render_signals.py`  
**Bytes:** 14798  
**Lines:** 428

```python
"""G1a T6 signal generators — freeze-from fixture-spec v1 only.

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a; fixture_spec_v1.json
digest 513c3baf… (SHA256SUMS trust chain).

Rules (numeric_generation):
  - render DIRECTLY at each gate rate (44100 / 48000 / 96000)
  - compute float64 → cast float32 (round-to-nearest-even); no dither
  - no normalize, no DC removal, no cross-rate resample
  - WAV container: pcm_float32_le, interleaved LRLR for stereo
  - log_sweep active only on [0.5, 1.75] s; outside → 0
"""
from __future__ import annotations

import math
import struct
from pathlib import Path
from typing import Callable, Iterable

import numpy as np

from ml_v3.contracts.canonical import sha256_of_file, sha256sums_text
from ml_v3.contracts.constants import GATE_SAMPLE_RATES
from ml_v3.contracts.fixture_spec import (
    PSEUDO_NOISE_PRNG_SEED,
    SWEEP_F_END_HZ,
    SWEEP_F_START_HZ,
    frozen_fixture_spec,
    sample_count,
    sweep_active_end_seconds,
    sweep_active_start_seconds,
)
from ml_v3.contracts.sha256sums import (
    G1A_SHA256SUMS_RELPATH,
    load_g1a_sha256sums,
    repo_root_from_here,
    verify_g1a_sha256sums,
)

__all__ = [
    "FixtureRenderError",
    "AUDIO_REL_PREFIX",
    "render_asset",
    "iter_asset_specs",
    "write_wav_pcm_float32_le",
    "render_all_to_tree",
    "asset_relpath",
    "update_sha256sums_with_audio",
]

AUDIO_REL_PREFIX = "ml_v3/fixtures/g1/audio"

# wave module: WAVE_FORMAT_IEEE_FLOAT
_WAVE_FORMAT_IEEE_FLOAT = 3


class FixtureRenderError(ValueError):
    """Raised when a fixture render request violates the frozen spec."""


def _require_gate_rate(fs: int) -> None:
    if fs not in GATE_SAMPLE_RATES:
        raise FixtureRenderError(
            f"fs {fs} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")


def _time_axis(fs: int) -> np.ndarray:
    """Return float64 ``t = n / fs`` for ``n = 0 .. N-1``."""
    _require_gate_rate(fs)
    n = sample_count(fs)
    # Exact integer N from fixture_spec; float64 division per time_rule.
    return np.arange(n, dtype=np.float64) / float(fs)


def _cast_f32(x: np.ndarray) -> np.ndarray:
    """Cast float64 render to IEEE754 binary32 (numpy default RTE)."""
    if x.dtype != np.float64:
        raise FixtureRenderError(
            f"compute dtype must be float64 before cast, got {x.dtype}")
    return x.astype(np.float32, copy=False)


def _multitone_f64(
    t: np.ndarray,
    frequencies_hz: Iterable[float],
    amplitude: float,
    phases_rad: Iterable[float],
) -> np.ndarray:
    out = np.zeros(t.shape[0], dtype=np.float64)
    for freq, phase in zip(frequencies_hz, phases_rad, strict=True):
        out += amplitude * np.sin(2.0 * math.pi * float(freq) * t + float(phase))
    return out


def render_multitone(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["multitone"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"multitone not defined at fs={fs}")
    t = _time_axis(fs)
    y = _multitone_f64(
        t,
        spec["frequencies_hz"],
        float(spec["amplitude_peak_each"]),
        spec["phases_rad"],
    )
    return _cast_f32(y)


def render_log_sweep(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["log_sweep"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"log_sweep not defined at fs={fs}")
    t = _time_axis(fs)
    t0 = sweep_active_start_seconds()
    t1 = sweep_active_end_seconds()
    t_active = t1 - t0
    if t_active <= 0.0:
        raise FixtureRenderError("log_sweep active interval empty")
    amp = float(spec["amplitude_peak"])
    ratio = SWEEP_F_END_HZ / SWEEP_F_START_HZ
    ln_ratio = math.log(ratio)
    # Active mask: t in [t_start, t_end] (inclusive endpoints).
    active = (t >= t0) & (t <= t1)
    u = np.zeros_like(t)
    u[active] = (t[active] - t0) / t_active
    # phi(t) = 2π f_start T_active / ln(ratio) * (ratio^u - 1)
    phi = np.zeros_like(t)
    phi[active] = (
        2.0 * math.pi * SWEEP_F_START_HZ * t_active / ln_ratio
        * (np.power(ratio, u[active]) - 1.0)
    )
    y = np.zeros_like(t)
    y[active] = amp * np.sin(phi[active])
    return _cast_f32(y)


def render_transient_burst(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["transient_burst"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"transient_burst not defined at fs={fs}")
    t = _time_axis(fs)
    onset = float(spec["onset_s"])
    peak = float(spec["peak_amplitude"])
    tau = float(spec["tau_s"])
    y = np.zeros_like(t)
    active = t >= onset
    y[active] = peak * np.exp(-(t[active] - onset) / tau)
    return _cast_f32(y)


def render_damped_resonance(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["damped_resonance"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"damped_resonance not defined at fs={fs}")
    t = _time_axis(fs)
    onset = float(spec["onset_s"])
    peak = float(spec["peak_amplitude"])
    tau = float(spec["tau_s"])
    f0 = float(spec["center_hz"])
    phase0 = float(spec["phase_at_onset_rad"])
    y = np.zeros_like(t)
    active = t >= onset
    dt = t[active] - onset
    y[active] = (
        peak * np.exp(-dt / tau)
        * np.sin(2.0 * math.pi * f0 * dt + phase0)
    )
    return _cast_f32(y)


def render_ultrasonic_96k(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["ultrasonic_96k"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"ultrasonic_96k not defined at fs={fs}")
    t = _time_axis(fs)
    y = _multitone_f64(
        t,
        spec["frequencies_hz"],
        float(spec["amplitude_peak_each"]),
        spec["phases_rad"],
    )
    return _cast_f32(y)


def _pseudo_noise_phases(seed: int, n_partials: int) -> np.ndarray:
    rng = np.random.Generator(np.random.PCG64(int(seed)))
    phases = np.empty(n_partials, dtype=np.float64)
    for k in range(n_partials):
        phases[k] = 2.0 * math.pi * float(rng.random())
    return phases


def render_pseudo_noise(fs: int, *, seed: int | None = None) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["pseudo_noise"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"pseudo_noise not defined at fs={fs}")
    t = _time_axis(fs)
    n = int(spec["n_partials"])
    f_lo = float(spec["f_lo_hz"])
    f_hi = float(spec["f_hi_hz"])
    amp = float(spec["amplitude_peak_each"])
    use_seed = PSEUDO_NOISE_PRNG_SEED if seed is None else int(seed)
    phases = _pseudo_noise_phases(use_seed, n)
    y = np.zeros(t.shape[0], dtype=np.float64)
    for k in range(n):
        freq = f_lo + (f_hi - f_lo) * (k + 0.5) / n
        y += amp * np.sin(2.0 * math.pi * freq * t + phases[k])
    return _cast_f32(y)


def render_silence(fs: int) -> np.ndarray:
    silence = frozen_fixture_spec()["categories"]["silence_non_finite"]["silence"]
    if fs not in silence["gate_sample_rates"]:
        raise FixtureRenderError(f"silence not defined at fs={fs}")
    n = sample_count(fs)
    return np.zeros(n, dtype=np.float32)


def render_non_finite(fs: int, pattern_id: str) -> np.ndarray:
    patterns = frozen_fixture_spec()["categories"]["silence_non_finite"][
        "non_finite_patterns"
    ]
    match = next((p for p in patterns if p["id"] == pattern_id), None)
    if match is None:
        raise FixtureRenderError(f"unknown non-finite pattern {pattern_id!r}")
    y = render_silence(fs)
    idx = int(match["sample_index"])
    if idx < 0 or idx >= y.shape[0]:
        raise FixtureRenderError(
            f"non-finite sample_index {idx} out of range for fs={fs}")
    inject = match["inject"]
    if inject == "NaN":
        y[idx] = np.float32(np.nan)
    elif inject == "+Inf":
        y[idx] = np.float32(np.inf)
    elif inject == "-Inf":
        y[idx] = np.float32(-np.inf)
    else:
        raise FixtureRenderError(f"unknown inject token {inject!r}")
    return y


def render_stereo_mid_only(fs: int) -> np.ndarray:
    s = render_multitone(fs).astype(np.float64, copy=False)
    # L=R=s → mid=s, side=0
    return _cast_f32(np.stack([s, s], axis=1))


def render_stereo_side_only(fs: int) -> np.ndarray:
    s = render_multitone(fs).astype(np.float64, copy=False)
    # L=s, R=-s → mid=0, side=s
    return _cast_f32(np.stack([s, -s], axis=1))


def render_stereo_decorrelated(fs: int) -> np.ndarray:
    layout = frozen_fixture_spec()["categories"]["decorrelated_stereo"][
        "layouts"
    ]["decorrelated"]
    left = render_pseudo_noise(
        fs, seed=int(layout["left_prng_seed"])
    ).astype(np.float64, copy=False)
    right = render_pseudo_noise(
        fs, seed=int(layout["right_prng_seed"])
    ).astype(np.float64, copy=False)
    return _cast_f32(np.stack([left, right], axis=1))


RenderFn = Callable[[int], np.ndarray]


def asset_relpath(category: str, stem: str, fs: int) -> str:
    """Repo-root-relative POSIX path for a rendered WAV asset."""
    return f"{AUDIO_REL_PREFIX}/{category}/{stem}_{fs}.wav"


def iter_asset_specs() -> list[tuple[str, str, int, RenderFn]]:
    """Return (category, stem, fs, render_fn) for every fixture-spec asset."""
    specs: list[tuple[str, str, int, RenderFn]] = []
    cats = frozen_fixture_spec()["categories"]

    def add(category: str, stem: str, rates: Iterable[int], fn: RenderFn) -> None:
        for fs in rates:
            specs.append((category, stem, int(fs), fn))

    add("multitone", "multitone", cats["multitone"]["gate_sample_rates"],
        render_multitone)
    add("log_sweep", "log_sweep", cats["log_sweep"]["gate_sample_rates"],
        render_log_sweep)
    add("transient_burst", "transient_burst",
        cats["transient_burst"]["gate_sample_rates"], render_transient_burst)
    add("damped_resonance", "damped_resonance",
        cats["damped_resonance"]["gate_sample_rates"], render_damped_resonance)
    add("ultrasonic_96k", "ultrasonic_96k",
        cats["ultrasonic_96k"]["gate_sample_rates"], render_ultrasonic_96k)
    add("pseudo_noise", "pseudo_noise",
        cats["pseudo_noise"]["gate_sample_rates"], render_pseudo_noise)
    add(
        "decorrelated_stereo", "mid_only",
        cats["decorrelated_stereo"]["gate_sample_rates"],
        render_stereo_mid_only,
    )
    add(
        "decorrelated_stereo", "side_only",
        cats["decorrelated_stereo"]["gate_sample_rates"],
        render_stereo_side_only,
    )
    add(
        "decorrelated_stereo", "decorrelated",
        cats["decorrelated_stereo"]["gate_sample_rates"],
        render_stereo_decorrelated,
    )
    silence_rates = cats["silence_non_finite"]["silence"]["gate_sample_rates"]
    add("silence_non_finite", "silence", silence_rates, render_silence)
    # Non-finite patterns inherit silence gate rates (fail-closed inputs).
    for pattern in cats["silence_non_finite"]["non_finite_patterns"]:
        pid = str(pattern["id"])

        def _make(pattern_id: str) -> RenderFn:
            return lambda fs, _pid=pattern_id: render_non_finite(fs, _pid)

        add("silence_non_finite", pid, silence_rates, _make(pid))

    # Deterministic order by relpath.
    specs.sort(key=lambda item: asset_relpath(item[0], item[1], item[2]))
    return specs


def render_asset(category: str, stem: str, fs: int) -> np.ndarray:
    for cat, st, rate, fn in iter_asset_specs():
        if cat == category and st == stem and rate == fs:
            return fn(fs)
    raise FixtureRenderError(
        f"unknown asset {category}/{stem} at fs={fs}")


def write_wav_pcm_float32_le(
    path: Path,
    samples: np.ndarray,
    sample_rate: int,
) -> None:
    """Write WAV PCM float32 LE (format tag 3), mono or interleaved stereo."""
    path = Path(path)
    if samples.dtype != np.float32:
        raise FixtureRenderError(
            f"WAV payload must be float32, got {samples.dtype}")
    if samples.ndim == 1:
        channels = 1
        pcm = np.ascontiguousarray(samples)
    elif samples.ndim == 2 and samples.shape[1] in (1, 2):
        channels = int(samples.shape[1])
        # C-order (N, C) flatten → interleaved LRLR for stereo.
        pcm = np.ascontiguousarray(samples).reshape(-1)
    else:
        raise FixtureRenderError(
            f"samples shape must be (N,) or (N,1|2), got {samples.shape}")

    n_frames = int(samples.shape[0])
    if n_frames != sample_count(int(sample_rate)):
        raise FixtureRenderError(
            f"frame count {n_frames} != fixture N={sample_count(int(sample_rate))}"
        )
    block_align = channels * 4
    byte_rate = int(sample_rate) * block_align
    data_bytes = pcm.astype("<f4", copy=False).tobytes()
    data_size = len(data_bytes)
    fmt_chunk = struct.pack(
        "<HHIIHH",
        _WAVE_FORMAT_IEEE_FLOAT,
        channels,
        int(sample_rate),
        byte_rate,
        block_align,
        32,
    )
    # 16-byte fmt payload (no extension for IEEE float basic header).
    riff_size = 4 + (8 + 16) + (8 + data_size)
    header = b"".join((
        b"RIFF",
        struct.pack("<I", riff_size),
        b"WAVE",
        b"fmt ",
        struct.pack("<I", 16),
        fmt_chunk,
        b"data",
        struct.pack("<I", data_size),
    ))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + data_bytes)


def render_all_to_tree(root: Path | None = None) -> list[str]:
    """Render every asset under ``ml_v3/fixtures/g1/audio/``; return relpaths."""
    root = repo_root_from_here() if root is None else Path(root)
    written: list[str] = []
    for category, stem, fs, fn in iter_asset_specs():
        rel = asset_relpath(category, stem, fs)
        absolute = root / rel
        samples = fn(fs)
        write_wav_pcm_float32_le(absolute, samples, fs)
        written.append(rel)
    return written


def update_sha256sums_with_audio(root: Path | None = None) -> dict[str, str]:
    """Merge audio digests into SHA256SUMS in canonical path order."""
    root = repo_root_from_here() if root is None else Path(root)
    entries = dict(load_g1a_sha256sums(root))
    for category, stem, fs, _fn in iter_asset_specs():
        rel = asset_relpath(category, stem, fs)
        absolute = root / rel
        if not absolute.is_file():
            raise FixtureRenderError(f"missing audio asset for hash: {rel}")
        entries[rel] = sha256_of_file(absolute)
    text = sha256sums_text(entries)
    out = root / G1A_SHA256SUMS_RELPATH
    out.write_text(text, encoding="utf-8", newline="\n")
    return verify_g1a_sha256sums(root)


def main() -> None:
    written = render_all_to_tree()
    entries = update_sha256sums_with_audio()
    print(f"rendered {len(written)} WAV assets")
    print(f"SHA256SUMS entries: {len(entries)}")


if __name__ == "__main__":
    main()
```

---

## B.42 FILE: `ml_v3/fixtures/g1/adapter_v2_v3_mapping.json`

**Path logico:** `ml_v3/fixtures/g1/adapter_v2_v3_mapping.json`  
**Bytes:** 2338  
**Lines:** 1

```json
{"artifact_id":"aieq-v3-adapter-v2-v3-mapping-1","contract_revision":"MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a","envelope_authority":"freeze-from-prose packaging of §10.5; contract has no literal JSON key table for this artifact","g2_vs_v2_gates":{"fp_group_rate_not_worse_than_seed":true,"macro_f1_absolute_when_seed_below":{"absolute_delta":0.1,"seed_threshold":0.1},"macro_f1_relative_improvement":0.1,"no_cross_seed_composition":true},"homologous_classes":["Resonance","Muddiness","Boominess","Thinness","BoxyMidrange","DullSound"],"label_at_center":{"gt":"one if the center falls in an actionable semantic region or GT event","v2":"one if probability exceeds the provenance threshold of the seed","v3":"one if the center falls in the temporal support of an actionable bundle or event of the class; a static bundle covers the prediction segment"},"legacy_profile_aliases":{"Techno":{"electronic_subgenre":"techno","source_profile":"edm","source_profile_id":6}},"macro_f1_denominator":6,"masked_na_classes":["Harshness","Sibilance"],"masked_na_policy":"Harshness and Sibilance stay N/A in the v2 comparison; never treat them as v2 negatives. v3 must clear absolute and G2 gates.","metrics":{"false_positive_group_rate_on_clean":true,"invent_v2_curves_frequency_severity":false,"macro_f1_classes":6},"na_semantics":"N/A does not enter macro-averages, does not satisfy a gate, and does not demonstrate improvement; structural absence is N/A, never zero/infinity/FAIL; a candidate that declares a surface but emits no valid prediction is fail-closed (FN, schema error or candidate failure per case), never N/A","per_class_support_floors":{"insufficient_support_consequence":"insufficient support on any of the six homologous classes renders the gate NO-GO","negative_groups":30,"positive_groups":30,"roles":["development-metric","final-test"]},"section":"10.5","segment_presence":"one for each of the three vectors only when the respective occupancy on the same grid is at least occupancy_threshold","structural_na_surfaces_v2":["tonal_curves","events","frequency","severity"],"temporal_grid":{"min_window_centers":20,"occupancy_threshold":0.05,"short_segment_semantics":"segment with fewer than min_window_centers is N/A for the homologous adapter; still available to native v3 metrics","window_step":16}}
```

---

## B.43 FILE: `ml_v3/fixtures/g1/metrology_lock.json`

**Path logico:** `ml_v3/fixtures/g1/metrology_lock.json`  
**Bytes:** 9264  
**Lines:** 1

```json
{"artifact_id":"aieq-v3-metrology-lock-1","bit_identity":{"ad_hoc_non_bit_identical_without_allowlist":"FAIL","gate_platform":{"arch":"arm64","authority":"freeze-from-prose of ml_v3/environment/README.md and numpy pin in ml_v3/environment/requirements.lock","numpy":"2.5.1","os":"darwin","os_marketing":"macOS 15.5","python":"CPython 3.12.13"},"gate_platform_float_identity":"byte_identical_only","gate_platform_float_tol":0,"gate_platform_secondary_tol_forbidden":true,"rule_on_gate_platform":"byte_identical float32 frame fields, rational timestamps and valid flags between offline and streaming; gate_platform_float_tol is 0 (no 1e-6 abs tol on gate platform)","secondary_cannot_close_g1_gate":true,"secondary_float_abs_tol":1e-06,"secondary_is_report_only":true,"secondary_platforms_allowlist":[]},"contract_revision":"MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a","dependencies":{"adapter_mapping_sha256":"6a978c01bcccb85fb7db17ae3c66ee55ebceee82f47dca792e5a2f3a5fb9828f","contract_revision":"MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a","grid_bands":120,"grid_centers_sha256":"7c01b069dad31eea58347c0365c64140dea48f108e71543ccd3cf706c9811da2"},"envelope_authority":"freeze-from-prose packaging of §13.1/§13.2 (+ §5 group-delay formula referenced by warm-up, §6.1 floor/clamp referenced by activity); contract has no literal JSON key table for this artifact","hash_coverage":{"beyond_dependencies_covered_by":"T5_SHA256SUMS","declaration":"Artifact hashes beyond the inline dependencies object are covered by the T5 SHA256SUMS; they are not left implicit","inline_dependencies_bound_here":true,"t4_2_stop_rule":"T4.2 is the last hardening round on this lock; further HIGH/MED items become debt for T5/G1b (no new lock hash) unless CRITICAL vacuous-PASS or final-test leak","t5_sha256sums_covers_artifact_hashes_beyond_dependencies":true},"other_gate_thresholds":{"anti_alias_max_db_re_tone":-80.0,"gain_invariance_max_abs_db":0.05,"ms_mid_equivalence_max_abs_db":0.05},"resampler_generator":{"authority_section":"5","coefficient_gain_scale":"up","cutoff":"midpoint_of_pass_and_stop","fc_formula":"((pass_hz + stop_hz) / 2) / (fs_in * up)","h0_formula":"2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0); h[n] = up * h0[n] / sum(h0), n = 0..M; M = num_taps - 1","kaiser_beta":9.0,"look_ahead_forbidden":true,"no_coefficient_implementation_in_g1a":true,"padding":"none","pass_hz":20000.0,"polyphase_phase_zero":true,"reflection":false,"serialize_only":true,"sinc_definition":"sin(pi*x)/(pi*x)","stop_hz_formula":"min(fs_in, 48000) / 2","streaming_state_preserved_across_blocks":true,"structure":"causal_polyphase","window":"kaiser"},"resampler_group_delay":{"authority_section":"5","formula":"(num_taps - 1) / (2 * up * fs_in) seconds; identity when up == down == 1 → 0","num_taps_formula":"128 * max(up, down) + 1","per_gate_sample_rate":{"44100":{"delay_den":11025,"delay_num":16,"down":147,"num_taps":20481,"up":160},"48000":{"delay_den":1,"delay_num":0,"down":1,"num_taps":null,"up":1},"96000":{"delay_den":750,"delay_num":1,"down":2,"num_taps":257,"up":1}}},"sample_rate_parity":{"activity":{"activity_floor_db":-120.0,"clamp_db":[-120.0,12.0],"empty_active_cell_set_is_fail":true,"empty_to_fail_derivation":{"candidate_for_future_contract_amendment":true,"chain":["§10.5: N/A does not satisfy a gate and does not demonstrate improvement","§10.1 (~line 663): a record with no active cells has curve metrics N/A, not zero; N/A is not converted into PASS","§13.2 gate 4: activity predicate + aggregator max over the declared dB domain; max over an empty active set is not a numeric 0 PASS"],"does_not_supersede_contract":true,"kind":"derived_packaging","packaging_note":"empty_active_cell_set_is_fail / empty_useful_segment_is_fail / max_over_empty_active_set=FAIL package the chain above; lock packaging authority, not a new contract amendment (REV7 out of T4.2)"},"empty_useful_segment_is_fail":true,"floor_linear":1e-12,"invalid_channel_vectors_ignored":true,"max_over_empty_active_set":"FAIL","na_is_not_pass":true,"post_hoc_mask_forbidden":true,"predicate":"max(psd_db_ref, psd_db_sr) > -120","shape_prominence_inherit_psd_activity":true,"union_cross_sr":true,"vacuous_pass_forbidden":true},"aggregator":"max","aggregator_forbidden":["mean","p95","RMSE","band_subset"],"alignment":{"authority_section":"5+13.2.gate_4","choose_within_pm1_radius_to_minimize_abs_delta_forbidden":true,"declaration":"Cross-SR frames align on nearest source_time (not raw output index); do not manually shift frames or pick within a ±1-sample/hop radius to minimize |Δ|; ties break by smaller frame_index, then smaller frame_end_sample","forbidden_select_by":["output_index","raw_frame_index"],"manual_frame_shift_forbidden":true,"select_by":"nearest_source_time","tie_break":["smaller_frame_index","smaller_frame_end_sample"]},"compare_hz":[44100,96000],"domain_db":["mid_psd_db[120]","side_psd_db[120]","mid_shape_db[120]","side_shape_db[120]","mid_prominence_db[120]","side_prominence_db[120]","mid_level_dbfs","side_level_dbfs"],"excluded_from_domain":["mid_delta_db","side_delta_db"],"experimental_rates_cannot_close":[88200,176400,192000],"reference_hz":48000,"single_active_cell_over_threshold_fails_gate":true,"threshold_max_abs_db":0.25,"timestamp_tolerance_canonical_samples":1,"transient_decay_tolerance_hops":1,"transient_onset_peak_tolerance_canonical_samples":1},"section":"13","stationary_portion":{"gate_closing_mode":"a","mode_a":{"applies_to":["multitone","noise"],"closes_sample_rate_parity_gate":true,"definition":"entire useful segment after warm-up and before coda","post_hoc_subset_forbidden":true},"mode_b":{"T_min_hops":8,"T_min_seconds_formula":"8 * H / fs_c","closes_sample_rate_parity_gate":false,"diagnostic_only":true,"empty_collection_diagnostic_is_fail":true,"metric":"variance and max |Δ| hop-to-hop of mean mid_psd_db over 120 bands, computed hop-per-hop inside the window","mode_change_after_fail_forbidden":true,"omit_failing_window_is_fail":true,"preregistered_windows":[],"stability_max_abs_hop_delta_db":0.5,"stability_variance_max_db2":1.0}},"streaming_equivalence":{"fixed_chunk_schedules_host_samples":[1,63,1024,4095,8192,8193],"geometric_schedule":{"chunks_host_samples":[55,2455,1,109,81,88,7061,22,499,5337,22,1467,58,1,142,233,290,20,6,18,15955,8039,1,2317,727,11131,2927,7,1,91,2096,7],"derivation":"concrete freeze of numpy.random.Generator(numpy.random.PCG64(20260719)).uniform(0, 14, size=32) then floor(2**U); the listed chunks are authoritative","length":32,"rule":"floor(2 ** U) with U ~ Uniform[0, 14), length >= 32, same PCG64 seed; repeated identical on every gate asset","u_high_exclusive":14,"u_low":0},"prng":"PCG64","prng_seed":20260719,"required_proof_surface":{"also_required":[{"id":"multi_asset_concat_with_explicit_delta_history_reset","letter":"a","requires":"concatenated multi-asset input with explicit delta_db history reset at each asset boundary"},{"id":"interleaved_silence_between_assets","letter":"b","requires":"silence interleaved between assets"},{"id":"streaming_vs_offline_identity_same_lock_and_platform","letter":"c","requires":"streaming-vs-offline identity on the same lock and gate platform"}],"also_required_applies_to":"proofs (a)(b)(c) under the same per-schedule quantifier as identity_rule; omitting any proof on any frozen schedule → FAIL","also_required_omission_is_fail":true,"also_required_quantifier":"for every frozen schedule (each fixed chunk size in fixed_chunk_schedules_host_samples and the frozen geometric schedule), on the gate platform","authority_section":"13.2.gate_3","float32_frame_fields":["mid_psd_db[120]","side_psd_db[120]","mid_shape_db[120]","side_shape_db[120]","mid_prominence_db[120]","side_prominence_db[120]","mid_delta_db[120]","side_delta_db[120]","mid_level_dbfs","side_level_dbfs"],"identity_rule":"for every frozen schedule, chunked input and monolithic offline input on the same lock/platform must produce the same V3FeatureFrame sequence over the enumerated fields","rational_timestamp_fields":["source_time_num","source_time_den","frame_end_sample","frame_index"],"validity_fields":["mid_valid","side_valid","valid","reason"],"validity_reason_enumerated_when_false":true}},"sweep_log_parity":{"alignment_policy_ref":"sample_rate_parity.alignment","checkpoint_hz":[20,45,60,80,250,1000,3500,8000,16000,20000],"frame_selection":"nearest_source_time_within_match_radius","match_radius_formula":"H / fs_c","missing_checkpoint_is_fail":true,"no_free_subset":true,"transient_and_damped_resonance_excluded_from_db_parity":true},"timing":{"H":1024,"K_coda":4,"K_wu":4,"N_LF":8192,"coda_den":375,"coda_num":32,"coda_seconds_formula":"K_coda * H / fs_c","cross_sr_window":"intersection of per-rate useful segments after warm-up and before coda (equivalent to max warm_up and max coda on the same source_time); comparing non-common tracts is FAIL","exclude_predicate":"source_time < warm_up_seconds OR source_time > T_asset - coda_seconds","fixed_offset_without_delay_den":125,"fixed_offset_without_delay_num":32,"fs_c":48000,"warm_up_composition":"additive","warm_up_composition_forbidden":"max","warm_up_seconds_formula":"resampler_group_delay_seconds + N_LF / fs_c + K_wu * H / fs_c"}}
```

---

## B.44 FILE: `ml_v3/fixtures/g1/fixture_spec_v1.json (digest 513c3baf…)`

**Path logico:** `ml_v3/fixtures/g1/fixture_spec_v1.json`  
**Bytes:** 8260  
**Lines:** 1

```json
{"artifact_id":"aieq-v3-fixture-spec-1","categories":{"damped_resonance":{"center_hz":1000.0,"channels":1,"duration_ref":"global_duration","envelope":"exponential_decay","excitation":"impulse_at_onset","gate_sample_rates":[44100,48000,96000],"note":"excluded from dB spectral SR-parity; onset/peak/decay gate per §13.2","onset_s":0.5,"peak_amplitude":0.25,"phase_at_onset_rad":0.0,"sample":"for t < onset: 0; else peak_amplitude * exp(-(t-onset)/tau_s) * sin(2*pi*center_hz*(t-onset) + phase_at_onset_rad)","tau_s":0.05},"decorrelated_stereo":{"channels":2,"duration_ref":"global_duration","gate_sample_rates":[44100,48000,96000],"layouts":{"decorrelated":{"base":"pseudo_noise","draw_order":"render left with left seed (k=0..511), then right with right seed (k=0..511); identical freqs/amplitudes as mono pseudo_noise","left_prng_seed":31051986,"method":"independent_pcg64_phase_per_channel","ms_convention":"mid=(L+R)/2, side=(L-R)/2","right_prng_seed":31051987},"mid_only":{"base":"multitone","implied":"mid=s, side=0","left":"s","ms_convention":"mid=(L+R)/2, side=(L-R)/2","right":"s"},"side_only":{"base":"multitone","implied":"mid=0, side=s","left":"s","ms_convention":"mid=(L+R)/2, side=(L-R)/2","right":"-s"}}},"log_sweep":{"active_interval":{"T_active_s":1.25,"kind":"preregistered_parameter","outside_active_sample":0.0,"rationale":"global asset remains duration_s=2; chirp occupies only [t_start, t_end] inside the common useful window so every SWEEP_CHECKPOINT_HZ crossing is reachable within match radius H/fs_c","t_end_den":4,"t_end_num":7,"t_end_s":1.75,"t_start_den":2,"t_start_num":1,"t_start_s":0.5},"amplitude_formula":"10**(-24/20) preregistered peak amplitude (peak = −24 dBFS; not RMS-of-sine * sqrt(2))","amplitude_peak":0.06309573444801933,"channels":1,"checkpoint_reachability":{"checkpoint_hz":[20,45,60,80,250,1000,3500,8000,16000,20000],"common_useful_end_s":1.9146666666666667,"common_useful_start_s":0.25745124716553286,"crossing_time_s":{"1000":1.2079041684733411,"16000":1.7096208279133098,"20":0.5,"20000":1.75,"250":0.9570458387533569,"3500":1.4345991869526227,"45":0.6467427158797344,"60":0.6988005227998594,"80":0.7508583297199843,"8000":1.5841916630533177},"match_radius_formula":"H / fs_c","match_radius_s":0.021333333333333333,"rule":"forall f in checkpoint_hz: common_useful_start <= t_cross(f) <= common_useful_end AND forall fs in gate_sample_rates: nearest_useful_frame_distance(t_cross, fs) <= H/fs_c"},"duration_ref":"global_duration","f_end_hz":20000.0,"f_start_hz":20.0,"gate_sample_rates":[44100,48000,96000],"instantaneous_freq_hz":"for t in [t_start, t_end]: f(t) = f_start * (f_end/f_start)**u with u=(t-t_start)/T_active, T_active=t_end-t_start; outside active interval: undefined (sample=0)","phase_at_active_start_rad":0.0,"phase_integral_rad":"for t in [t_start, t_end]: phi(t) = 2*pi * f_start * T_active / ln(f_end/f_start) * ((f_end/f_start)**u - 1), u=(t-t_start)/T_active; phi(t_start)=0","sample":"for t in [t_start, t_end]: A * sin(phi(t)); else 0; A=amplitude_peak","sweep_law":"exponential_log_chirp"},"multitone":{"amplitude_formula":"10**(-24/20) * sqrt(2/N) with N=len(frequencies_hz); equal-power packaging aligned with §13 pseudo-noise","amplitude_peak_each":0.029743614461426742,"channels":1,"duration_ref":"global_duration","frequencies_hz":[45,60,80,250,1000,3500,8000,16000,20000],"gate_sample_rates":[44100,48000,96000],"phase_policy":"all_zero_deterministic","phases_rad":[0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0],"waveform":"sum_k A * sin(2*pi*f_k*t + phi_k) in float64; k ascending in frequencies_hz order"},"pseudo_noise":{"amplitude_formula":"10**(-24/20) * sqrt(2/512)","amplitude_peak_each":0.003943483403001208,"channels":1,"duration_ref":"global_duration","f_hi_hz":20000.0,"f_lo_hz":20.0,"freq_hz_formula":"20 + (20000-20)*(k+0.5)/512","gate_sample_rates":[44100,48000,96000],"k_order":"ascending","k_range":[0,511],"n_partials":512,"phase_rule":"rng = Generator(PCG64(31051986)); phase[k] = 2*pi*rng.random() for k=0..511 ascending","phase_unit":"radians","prng":"PCG64","prng_seed":31051986,"rms_dbfs":-24.0,"waveform":"sum_{k=0..511} A * sin(2*pi*freq[k]*t + phase[k]) in float64; cast float32; no renormalize"},"silence_non_finite":{"non_finite_patterns":[{"base":"silence","channels":1,"duration_ref":"global_duration","id":"nan_at_sample_0","inject":"NaN","sample_index":0},{"base":"silence","channels":1,"duration_ref":"global_duration","id":"pos_inf_at_sample_0","inject":"+Inf","sample_index":0},{"base":"silence","channels":1,"duration_ref":"global_duration","id":"neg_inf_at_sample_0","inject":"-Inf","sample_index":0}],"purpose":"fail-closed gate inputs (§13)","silence":{"channels":1,"duration_ref":"global_duration","gate_sample_rates":[44100,48000,96000],"value":0.0}},"transient_burst":{"carrier":"none_impulse_like","channels":1,"duration_ref":"global_duration","envelope":"one_sided_exponential","gate_sample_rates":[44100,48000,96000],"note":"excluded from dB spectral SR-parity; onset/peak/decay gate per §13.2","onset_s":0.5,"peak_amplitude":0.5,"repetition_count":1,"sample":"for t < onset: 0; else peak_amplitude * exp(-(t-onset)/tau_s)","tau_s":0.01},"ultrasonic_96k":{"amplitude_formula":"10**(-24/20) * sqrt(2/N) with N=3","amplitude_peak_each":0.051517451447931524,"channels":1,"duration_ref":"global_duration","frequencies_hz":[28000,32000,40000],"gate_sample_rates":[96000],"phases_rad":[0.0,0.0,0.0],"presentation":"simultaneous","waveform":"sum_k A * sin(2*pi*f_k*t + phi_k) in float64; k ascending"}},"contract_revision":"MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a","dependencies":{"contract_revision":"MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a","gate_sample_rates":[44100,48000,96000],"metrology_lock_sha256":"d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e"},"envelope_authority":"freeze-from-prose packaging of §13 fixture freedoms (amplitudes/phases/envelopes/sweep law/stereo/duration/numeric generation semantics); contract has no literal JSON key table for this artifact; NO REV7 — packaging only; does not implement audio generators (M3/T6 after CC+commit)","global_duration":{"N_at_gate_rates":{"44100":88200,"48000":96000,"96000":192000},"bound_check":{"coda_den":375,"coda_num":32,"max_warm_up_plus_coda_den":55125,"max_warm_up_plus_coda_num":18896,"max_warm_up_plus_coda_s":0.34278458049886623,"mode_a_gate_closing":true,"must_exceed":"max_sr(warm_up_sr + coda_sr)","rationale":"conservative lab default: duration_s=2 > max(warm_up+coda)≈0.3428 s (lock rationals 18896/55125) so mode-(a) useful segment is long enough (~77 hops @ fs_c) for SR-parity multitone/noise; T_min=8H/fs_c is mode-(b) diagnostic only and must not define duration","useful_portion_hops_at_fs_c":77.68197278911566,"useful_portion_s_at_max_wu":1.657215419501134},"duration_den":1,"duration_num":2,"duration_s":2.0,"false_formula_forbidden":"warm_up + coda + T_min","kind":"preregistered_parameter","not_a_contract_formula":true},"no_audio_generators_in_this_artifact":true,"numeric_generation":{"N_rule":"N = duration_num * fs / duration_den (exact integer)","artifact_dtype":"float32","cast_policy":"after full float64 render, cast each sample to IEEE754 binary32 round-to-nearest-even (language/numpy default float64→float32); no dither","compute_dtype":"float64","cross_rate_resample":"forbidden","dc_removal":"forbidden","file_format":{"byte_order":"little_endian","channels_layout":"interleaved_LRLR_for_stereo","container":"wav","encoding":"pcm_float32_le","header_sample_rate_matches_render_fs":true,"non_finite_encoding":"same WAV PCM float32 LE container; inject IEEE754 binary32 NaN/+Inf/-Inf bit patterns at pinned samples"},"post_render_normalization":"forbidden","prng":"numpy.random.Generator(numpy.random.PCG64(seed))","prng_draw_order":"sequential rng.random() calls in ascending k; never reverse, never vector-size ambiguity vs loop","render_policy":"generate DIRECTLY at each of 44100/48000/96000; never resample one rate to another","sample_index_rule":"n = 0 .. N-1","sum_order":"ascending partial index k (or ascending frequency list order); accumulate in float64 before cast","time_rule":"t = n / fs  (float64 division)"},"section":"13","spec_version":1}
```

---

## B.45 FILE: `ml_v3/fixtures/g1/schema_registry_v1.json (fa506142…)`

**Path logico:** `ml_v3/fixtures/g1/schema_registry_v1.json`  
**Bytes:** 19608  
**Lines:** 1

```json
{"artifact_id":"aieq-v3-schema-registry-1","key_sets":{"ADMISSION_BATCH_KEYS":["admission_batch_id","inclusion_rules_version","reviewer_id","roster_commit","roster_sha256","salt_commitment","salt_reveal","schema","source_snapshot_id","source_snapshot_sha256","status"],"ANNOTATION_KEYS":["annotator_id","asset_id","clean_for_action","complete_types","dynamic_events","evaluation_unit_id","explicit_negative_types","global_actionable","notes","pass_id","profile","schema","segment_end_s","segment_start_s","semantic_regions","tonal_actionable_mask","tonal_confidence","tonal_correction_db","tool_version"],"ANOMALY_REF_KEYS":["dtype","num_feature_frames","relative_path","sha256"],"ASSET_MANIFEST_KEYS":["admission_batch_id","asset_id","attribution","benchmark_families","channels","derivative_kind","development_pilot","duration_s","electronic_subgenre","group_id","group_primary_domain","group_primary_profile","ledger_id","license_class","license_url","parent_asset_id","primary_domain","relative_path","sample_rate","schema","sha256","source_family","source_profile","split_role"],"BENCHMARK_POWER_PLAN_KEYS":["alpha_plan","families","gates","implementation_version","m","pilot_sha256","plan_id","power_source_role","result","schema","seed_base","support"],"CALIBRATION_POLICY_KEYS":["anomaly_calibrator","anomaly_class_thresholds","calibration_manifest_sha256","candidate_extractor_version","frontend_sha256","model_sha256","policy_id","policy_version","prediction_schema_id","prediction_schema_sha256","region_to_bundle","schema","score_to_confidence","semantic_type_thresholds","threshold_to_actionable","tonal_band_thresholds","tonal_calibrator"],"CALIBRATOR_KEYS":["algorithm","parameters"],"DYNAMIC_EVENT_KEYS":["actionable","center_hz","confidence","end_s","problem_type","problem_type_id","severity","start_s","width_octaves"],"POWER_FAMILY_KEYS":["family_id","floor_contractual","n_power","n_required","unit"],"POWER_GATE_KEYS":["metric_id","min_effect_size","n_power","n_required","orientation","statistic","support"],"PREDICTION_EVENT_KEYS":["actionable","center_hz","confidence","end_s","problem_type","problem_type_id","severity","start_s","width_octaves"],"PREDICTION_KEYS":["anomaly_score_ref","anomaly_severity_ref","asset_id","calibration_policy_id","calibration_policy_sha256","events","frontend_contract_sha256","model_id","model_sha256","profile","schema","segment_end_s","segment_start_s","semantic_bundles","tonal_confidence","tonal_curve_db","tonal_score"],"SCORE_KNOT_KEYS":["confidence","score"],"SEMANTIC_BUNDLE_KEYS":["actionable","band_hi_hz","band_lo_hz","confidence","end_s","problem_type","problem_type_id","start_s"],"SEMANTIC_REGION_KEYS":["actionable","band_hi_hz","band_lo_hz","confidence","direction","end_s","problem_type","problem_type_id","severity","start_s"]},"schema_ids":["aieq-v3-asset-manifest-1","aieq-v3-admission-batch-1","aieq-v3-annotation-1","aieq-v3-prediction-1","aieq-v3-calibration-policy-1","aieq-v3-benchmark-power-plan-1"],"schemas":{"aieq-v3-admission-batch-1":{"$id":"aieq-v3-admission-batch-1","$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"description":"G1a T2 freeze-from-prose packaging of §9.1 admission-batch prose (not a letterally tabled JSON key list). roster_sha256 MUST equal admission_batch_id (SHA-256 of canonical roster bytes).","properties":{"admission_batch_id":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"inclusion_rules_version":{"minLength":1,"type":"string"},"reviewer_id":{"minLength":1,"type":"string"},"roster_commit":{"minLength":1,"type":"string"},"roster_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"salt_commitment":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"salt_reveal":{"description":"null pre-reveal; lowercase hex of 32 raw salt bytes","type":["string","null"]},"schema":{"const":"aieq-v3-admission-batch-1"},"source_snapshot_id":{"minLength":1,"type":"string"},"source_snapshot_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"status":{"enum":["admitted","rejected"],"type":"string"}},"required":["admission_batch_id","inclusion_rules_version","reviewer_id","roster_commit","roster_sha256","salt_commitment","salt_reveal","schema","source_snapshot_id","source_snapshot_sha256","status"],"title":"aieq-v3-admission-batch-1","type":"object"},"aieq-v3-annotation-1":{"$id":"aieq-v3-annotation-1","$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"properties":{"annotator_id":{"minLength":1,"type":"string"},"asset_id":{"minLength":1,"type":"string"},"clean_for_action":{"type":"boolean"},"complete_types":{"items":{"enum":["Resonance","Harshness","Muddiness","Sibilance","Boominess","Thinness","BoxyMidrange","DullSound"],"type":"string"},"type":"array","uniqueItems":true},"dynamic_events":{"items":{"additionalProperties":false,"properties":{"actionable":{"type":"boolean"},"center_hz":{"maximum":20000,"minimum":20,"type":"number"},"confidence":{"maximum":1,"minimum":0,"type":"number"},"end_s":{"type":"number"},"problem_type":{"enum":["Resonance","Harshness","Sibilance"],"type":"string"},"problem_type_id":{"maximum":7,"minimum":0,"type":"integer"},"severity":{"maximum":1,"minimum":0,"type":"number"},"start_s":{"type":"number"},"width_octaves":{"exclusiveMinimum":0,"type":"number"}},"required":["actionable","center_hz","confidence","end_s","problem_type","problem_type_id","severity","start_s","width_octaves"],"type":"object"},"type":"array"},"evaluation_unit_id":{"minLength":1,"type":"string"},"explicit_negative_types":{"items":{"enum":["Resonance","Harshness","Muddiness","Sibilance","Boominess","Thinness","BoxyMidrange","DullSound"],"type":"string"},"type":"array","uniqueItems":true},"global_actionable":{"type":"boolean"},"notes":{"type":"string"},"pass_id":{"minLength":1,"type":"string"},"profile":{"enum":["generic","vocals","drums","bass","synth","master","edm"],"type":"string"},"schema":{"const":"aieq-v3-annotation-1"},"segment_end_s":{"exclusiveMinimum":0,"type":"number"},"segment_start_s":{"minimum":0,"type":"number"},"semantic_regions":{"items":{"additionalProperties":false,"properties":{"actionable":{"type":"boolean"},"band_hi_hz":{"type":["number","null"]},"band_lo_hz":{"type":["number","null"]},"confidence":{"maximum":1,"minimum":0,"type":"number"},"direction":{"type":["string","null"]},"end_s":{"type":"number"},"problem_type":{"enum":["Resonance","Harshness","Muddiness","Sibilance","Boominess","Thinness","BoxyMidrange","DullSound"],"type":"string"},"problem_type_id":{"maximum":7,"minimum":0,"type":"integer"},"severity":{"maximum":1,"minimum":0,"type":"number"},"start_s":{"type":"number"}},"required":["actionable","band_hi_hz","band_lo_hz","confidence","direction","end_s","problem_type","problem_type_id","severity","start_s"],"type":"object"},"type":"array"},"tonal_actionable_mask":{"description":"per-band actionable mask","items":{"type":"boolean"},"maxItems":120,"minItems":120,"type":"array"},"tonal_confidence":{"description":"per-band confidence","items":{"maximum":1.0,"minimum":0.0,"type":"number"},"maxItems":120,"minItems":120,"type":"array"},"tonal_correction_db":{"description":"EQ correttiva desiderata","items":{"maximum":9.0,"minimum":-9.0,"type":"number"},"maxItems":120,"minItems":120,"type":"array"},"tool_version":{"minLength":1,"type":"string"}},"required":["annotator_id","asset_id","clean_for_action","complete_types","dynamic_events","evaluation_unit_id","explicit_negative_types","global_actionable","notes","pass_id","profile","schema","segment_end_s","segment_start_s","semantic_regions","tonal_actionable_mask","tonal_confidence","tonal_correction_db","tool_version"],"title":"aieq-v3-annotation-1","type":"object"},"aieq-v3-asset-manifest-1":{"$id":"aieq-v3-asset-manifest-1","$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"description":"§9.1 asset-manifest keys (letterally tabled). development_pilot may be true only when split_role == development-metric.","properties":{"admission_batch_id":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"asset_id":{"minLength":1,"type":"string"},"attribution":{"type":"string"},"benchmark_families":{"items":{"enum":["tonal-controlled","tonal-natural","anomaly-natural","clean-safety","electronic-stratified"],"type":"string"},"type":"array","uniqueItems":true},"channels":{"minimum":1,"type":"integer"},"derivative_kind":{"type":["string","null"]},"development_pilot":{"type":"boolean"},"duration_s":{"exclusiveMinimum":0,"type":"number"},"electronic_subgenre":{"type":["string","null"]},"group_id":{"minLength":1,"type":"string"},"group_primary_domain":{"minLength":1,"type":"string"},"group_primary_profile":{"enum":["generic","vocals","drums","bass","synth","master","edm"],"type":"string"},"ledger_id":{"type":"string"},"license_class":{"enum":["CC0","CC-BY","OWNED"],"type":"string"},"license_url":{"type":"string"},"parent_asset_id":{"type":["string","null"]},"primary_domain":{"minLength":1,"type":"string"},"relative_path":{"minLength":1,"type":"string"},"sample_rate":{"enum":[44100,48000,88200,96000,176400,192000],"type":"integer"},"schema":{"const":"aieq-v3-asset-manifest-1"},"sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"source_family":{"minLength":1,"type":"string"},"source_profile":{"enum":["generic","vocals","drums","bass","synth","master","edm"],"type":"string"},"split_role":{"enum":["train","validation","calibration","development-metric","final-test"],"type":"string"}},"required":["admission_batch_id","asset_id","attribution","benchmark_families","channels","derivative_kind","development_pilot","duration_s","electronic_subgenre","group_id","group_primary_domain","group_primary_profile","ledger_id","license_class","license_url","parent_asset_id","primary_domain","relative_path","sample_rate","schema","sha256","source_family","source_profile","split_role"],"title":"aieq-v3-asset-manifest-1","type":"object"},"aieq-v3-benchmark-power-plan-1":{"$id":"aieq-v3-benchmark-power-plan-1","$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"description":"G1a T2 freeze-from-prose packaging of §11.2 power-plan contents (not a letterally tabled JSON schema). Power is estimated only from development_pilot; unit must be group_id (never file/crop); final-test must not appear as a power source.","properties":{"alpha_plan":{"exclusiveMinimum":0,"maximum":1,"type":"number"},"families":{"description":"Freeze-from-prose packaging of §11.2 family floors / n_required; exact nested keys are T2 structure","items":{"additionalProperties":false,"properties":{"family_id":{"enum":["tonal-controlled","tonal-natural","anomaly-natural","clean-safety","electronic-stratified"],"type":"string"},"floor_contractual":{"minimum":1,"type":"integer"},"n_power":{"minimum":1,"type":"integer"},"n_required":{"minimum":1,"type":"integer"},"unit":{"const":"group_id"}},"required":["family_id","floor_contractual","n_power","n_required","unit"],"type":"object"},"minItems":1,"type":"array"},"gates":{"description":"Freeze-from-prose packaging of §11.2 primary gates; exact nested keys are T2 structure","items":{"additionalProperties":false,"properties":{"metric_id":{"minLength":1,"type":"string"},"min_effect_size":{"type":"number"},"n_power":{"minimum":1,"type":"integer"},"n_required":{"minimum":1,"type":"integer"},"orientation":{"enum":["greater","less","two-sided"],"type":"string"},"statistic":{"minLength":1,"type":"string"},"support":{"minimum":0,"type":"integer"}},"required":["metric_id","min_effect_size","n_power","n_required","orientation","statistic","support"],"type":"object"},"minItems":1,"type":"array"},"implementation_version":{"minLength":1,"type":"string"},"m":{"minimum":1,"type":"integer"},"pilot_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"plan_id":{"description":"G1a T2 freeze-from-prose document id (not letterally tabled in §11.2)","minLength":1,"type":"string"},"power_source_role":{"const":"development_pilot"},"result":{"type":"object"},"schema":{"const":"aieq-v3-benchmark-power-plan-1"},"seed_base":{"type":"integer"},"support":{"type":"object"}},"required":["alpha_plan","families","gates","implementation_version","m","pilot_sha256","plan_id","power_source_role","result","schema","seed_base","support"],"title":"aieq-v3-benchmark-power-plan-1","type":"object"},"aieq-v3-calibration-policy-1":{"$id":"aieq-v3-calibration-policy-1","$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"properties":{"anomaly_calibrator":{"additionalProperties":false,"properties":{"algorithm":{"minLength":1,"type":"string"},"parameters":{"type":"object"}},"required":["algorithm","parameters"],"type":"object"},"anomaly_class_thresholds":{"items":{"maximum":1,"minimum":0,"type":"number"},"maxItems":3,"minItems":3,"type":"array"},"calibration_manifest_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"candidate_extractor_version":{"minLength":1,"type":"string"},"frontend_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"model_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"policy_id":{"minLength":1,"type":"string"},"policy_version":{"minLength":1,"type":"string"},"prediction_schema_id":{"const":"aieq-v3-prediction-1"},"prediction_schema_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"region_to_bundle":{"additionalProperties":false,"properties":{"parameters":{"type":"object"},"rule_id":{"minLength":1,"type":"string"}},"required":["rule_id","parameters"],"type":"object"},"schema":{"const":"aieq-v3-calibration-policy-1"},"score_to_confidence":{"description":"monotone non-decreasing; must include score 0 and 1","items":{"additionalProperties":false,"properties":{"confidence":{"maximum":1,"minimum":0,"type":"number"},"score":{"maximum":1,"minimum":0,"type":"number"}},"required":["confidence","score"],"type":"object"},"minItems":2,"type":"array"},"semantic_type_thresholds":{"items":{"maximum":1,"minimum":0,"type":"number"},"maxItems":8,"minItems":8,"type":"array"},"threshold_to_actionable":{"additionalProperties":false,"properties":{"parameters":{"type":"object"},"rule_id":{"minLength":1,"type":"string"}},"required":["rule_id","parameters"],"type":"object"},"tonal_band_thresholds":{"description":"per-band thresholds","items":{"maximum":1.0,"minimum":0.0,"type":"number"},"maxItems":120,"minItems":120,"type":"array"},"tonal_calibrator":{"additionalProperties":false,"properties":{"algorithm":{"minLength":1,"type":"string"},"parameters":{"type":"object"}},"required":["algorithm","parameters"],"type":"object"}},"required":["anomaly_calibrator","anomaly_class_thresholds","calibration_manifest_sha256","candidate_extractor_version","frontend_sha256","model_sha256","policy_id","policy_version","prediction_schema_id","prediction_schema_sha256","region_to_bundle","schema","score_to_confidence","semantic_type_thresholds","threshold_to_actionable","tonal_band_thresholds","tonal_calibrator"],"title":"aieq-v3-calibration-policy-1","type":"object"},"aieq-v3-prediction-1":{"$id":"aieq-v3-prediction-1","$schema":"https://json-schema.org/draft/2020-12/schema","additionalProperties":false,"description":"§9.3 prediction. Events without anomaly_score_ref / anomaly_severity_ref are schema-invalid.","properties":{"anomaly_score_ref":{"additionalProperties":false,"description":"SHA-256 ref to little-endian float32 array shape [num_feature_frames, 3, 120], class order Resonance/Harshness/Sibilance.","properties":{"dtype":{"const":"float32_le"},"num_feature_frames":{"minimum":1,"type":"integer"},"relative_path":{"minLength":1,"type":"string"},"sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"}},"required":["dtype","num_feature_frames","relative_path","sha256"],"type":"object"},"anomaly_severity_ref":{"additionalProperties":false,"description":"SHA-256 ref to little-endian float32 array shape [num_feature_frames, 3, 120], class order Resonance/Harshness/Sibilance.","properties":{"dtype":{"const":"float32_le"},"num_feature_frames":{"minimum":1,"type":"integer"},"relative_path":{"minLength":1,"type":"string"},"sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"}},"required":["dtype","num_feature_frames","relative_path","sha256"],"type":"object"},"asset_id":{"minLength":1,"type":"string"},"calibration_policy_id":{"minLength":1,"type":"string"},"calibration_policy_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"events":{"items":{"additionalProperties":false,"properties":{"actionable":{"type":"boolean"},"center_hz":{"maximum":20000,"minimum":20,"type":"number"},"confidence":{"maximum":1,"minimum":0,"type":"number"},"end_s":{"type":"number"},"problem_type":{"enum":["Resonance","Harshness","Sibilance"],"type":"string"},"problem_type_id":{"maximum":7,"minimum":0,"type":"integer"},"severity":{"maximum":1,"minimum":0,"type":"number"},"start_s":{"type":"number"},"width_octaves":{"exclusiveMinimum":0,"type":"number"}},"required":["actionable","center_hz","confidence","end_s","problem_type","problem_type_id","severity","start_s","width_octaves"],"type":"object"},"type":"array"},"frontend_contract_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"model_id":{"minLength":1,"type":"string"},"model_sha256":{"description":"lowercase SHA-256 hex digest","pattern":"^[0-9a-f]{64}$","type":"string"},"profile":{"enum":["generic","vocals","drums","bass","synth","master","edm"],"type":"string"},"schema":{"const":"aieq-v3-prediction-1"},"segment_end_s":{"exclusiveMinimum":0,"type":"number"},"segment_start_s":{"minimum":0,"type":"number"},"semantic_bundles":{"items":{"additionalProperties":false,"properties":{"actionable":{"type":"boolean"},"band_hi_hz":{"type":["number","null"]},"band_lo_hz":{"type":["number","null"]},"confidence":{"maximum":1,"minimum":0,"type":"number"},"end_s":{"type":"number"},"problem_type":{"enum":["Resonance","Harshness","Muddiness","Sibilance","Boominess","Thinness","BoxyMidrange","DullSound"],"type":"string"},"problem_type_id":{"maximum":7,"minimum":0,"type":"integer"},"start_s":{"type":"number"}},"required":["actionable","band_hi_hz","band_lo_hz","confidence","end_s","problem_type","problem_type_id","start_s"],"type":"object"},"type":"array"},"tonal_confidence":{"description":"calibrated tonal confidence","items":{"maximum":1.0,"minimum":0.0,"type":"number"},"maxItems":120,"minItems":120,"type":"array"},"tonal_curve_db":{"description":"public tonal curve","items":{"maximum":9.0,"minimum":-9.0,"type":"number"},"maxItems":120,"minItems":120,"type":"array"},"tonal_score":{"description":"pre-calibration tonal score","items":{"maximum":1.0,"minimum":0.0,"type":"number"},"maxItems":120,"minItems":120,"type":"array"}},"required":["anomaly_score_ref","anomaly_severity_ref","asset_id","calibration_policy_id","calibration_policy_sha256","events","frontend_contract_sha256","model_id","model_sha256","profile","schema","segment_end_s","segment_start_s","semantic_bundles","tonal_confidence","tonal_curve_db","tonal_score"],"title":"aieq-v3-prediction-1","type":"object"}}}
```

---

## B.46 FILE: `ml_v3/fixtures/g1/SHA256SUMS (48 entries)`

**Path logico:** `ml_v3/fixtures/g1/SHA256SUMS`  
**Bytes:** 6027  
**Lines:** 48

```text
6a6f6d35bbf3fc65d7a01e54620bf4f9649ea77d60c2b3d9e7ea0b72f7f49a86  docs/MOTORE_V3_G1_CONTRACT.md
6a978c01bcccb85fb7db17ae3c66ee55ebceee82f47dca792e5a2f3a5fb9828f  ml_v3/fixtures/g1/adapter_v2_v3_mapping.json
d47df77eb87fc0832f33837f8eb7ab7dd8b565922f59c2d8c4e3222f5db51308  ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_44100.wav
4979893c707032977b4371d17cc0b8bfe0f056e02b4a6f5ba92b2fc4e640f90f  ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_48000.wav
53b7483f9f94ab84e882319a086dfabdaedf6a15ff37a32e05f332a67548f75b  ml_v3/fixtures/g1/audio/damped_resonance/damped_resonance_96000.wav
39dcb6f91a8d77f2a727a920b89e6556eb48f853187c1ca3ed4833decab09dcc  ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_44100.wav
6528c09b1029c18e6a2925b8934e68a0ad3fce4ea28e95153130730c42afaa7a  ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_48000.wav
17d744637b20ca4696ae42bb021442d731712cc56cfe99cf2e9025b35165ac5c  ml_v3/fixtures/g1/audio/decorrelated_stereo/decorrelated_96000.wav
569fc6ce7da6a27e675f232572f2912f42413a32c1373d714f29346778de9508  ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_44100.wav
238ab02df007f6b701dedbbca7aaddc7d4b6abb6179a085bca0ed6d1e02bf94d  ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_48000.wav
4adfb30d2362d1716e50622ab88cc50c37fcb7dba55b2f3ea6990524c39b21bd  ml_v3/fixtures/g1/audio/decorrelated_stereo/mid_only_96000.wav
e927a39e1eba2b64e1b4351571b334234178895eb7def5843eeb8878aa1fb120  ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_44100.wav
a410a5029d31e85571f2f03463d169f42b9db4eb91f00b34c5b66305b046e176  ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_48000.wav
82f9c2ad35bcd9724f836716f3967dfce1c08136e79a177f9169d1a86ea915ec  ml_v3/fixtures/g1/audio/decorrelated_stereo/side_only_96000.wav
c4d41c0efad09e12effe057ed70a025ad93ef63e2b159f5e54f0dbb50f45a9ee  ml_v3/fixtures/g1/audio/log_sweep/log_sweep_44100.wav
0d75a68662ba888fe7414b2ba7477a3db807a56145ead4a41bcb0731bcd406ef  ml_v3/fixtures/g1/audio/log_sweep/log_sweep_48000.wav
5f308a218a6969e41f7a8a07b3891283be0fca07e431eb97bfa4747c6da78110  ml_v3/fixtures/g1/audio/log_sweep/log_sweep_96000.wav
d21fdff2bc743d1630de76e17da027a77e9c353ca91993f660c4dc1d0a544630  ml_v3/fixtures/g1/audio/multitone/multitone_44100.wav
45e1f61b987ad98d3fda84e3d35fa82e7b4da9ba88e5e6d98fb44fb0ae746fa4  ml_v3/fixtures/g1/audio/multitone/multitone_48000.wav
ac100d400ec75584e3de32ba82d4afe31c98df645011fbe0ce810c7984e636c3  ml_v3/fixtures/g1/audio/multitone/multitone_96000.wav
ef5b54e95015e7c85790f79e16b5a0f46c6fc0fab4bc91038da762749f6688d3  ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_44100.wav
c9d0bc88d1953c9615de15ff7c237987d222a188de2f2db3aff7fb84997ebe06  ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_48000.wav
6d8b66fb2c31364d1480259a2fbc838deea43e8fc9cd7f279c02d6bd643e3286  ml_v3/fixtures/g1/audio/pseudo_noise/pseudo_noise_96000.wav
87a0ad44b3656496ac6fdd040f3b16afb9f6862b5f8309a2db12fe900a4c0cc5  ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_44100.wav
69013347ab6e8e3920e0c3da0f84355fbeada4a3aa3acf50fe9348abac463e16  ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_48000.wav
116325057658f851b3cc8fc1933206f7a513e00877b6f821ff523d2a777261e1  ml_v3/fixtures/g1/audio/silence_non_finite/nan_at_sample_0_96000.wav
e49cf37cfd0df4a6579f70ab0b9f625a01ec64f42d5b666ca4e81b150d466255  ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_44100.wav
e745d9ff2bde64092cd1c307064aadbe9e6774badc0b49c9d11155aedf423812  ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_48000.wav
9cff79150692657a1497ca020f22152f5bbda105a0de4b2d14bba7241058881a  ml_v3/fixtures/g1/audio/silence_non_finite/neg_inf_at_sample_0_96000.wav
49b2a58c553fa8b116de40492ccdc960f30c1d44d6a6c678dc388bb605b780f3  ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_44100.wav
d7042449b719d6360209a1e1ba54f637f2def0e4d03655c7d7fb9d787b2583c0  ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_48000.wav
c3b1ad0d53075e83c6f7b7d743081fcd057a5e45b10ad4ed7409d820f55d340f  ml_v3/fixtures/g1/audio/silence_non_finite/pos_inf_at_sample_0_96000.wav
13e6d9a9b74be09f7e443cf8d4077a20fcfde6a0b0bd54a951f9d96d1de11a4c  ml_v3/fixtures/g1/audio/silence_non_finite/silence_44100.wav
2f80217ed947d5a99a7b1ddc177153e184066ddff495e2847a142c9d2ffc9711  ml_v3/fixtures/g1/audio/silence_non_finite/silence_48000.wav
780952a643ae5ffb05b652f2188b57ecb10a8c1b98327d3add9e4b5df36e508a  ml_v3/fixtures/g1/audio/silence_non_finite/silence_96000.wav
c984bf0fc462ffc5ed5ce91d3ec87cb326e58d1bfaee3a3e029c6675b42ef29e  ml_v3/fixtures/g1/audio/transient_burst/transient_burst_44100.wav
0db70f13cadda780ecefed3711c98c3980a0ab8e0bd0c3d81a3e4797da3d497b  ml_v3/fixtures/g1/audio/transient_burst/transient_burst_48000.wav
29d579c8dc46c32185af0c1851c680f877f314eed45f6f5d16ce8630e59e2805  ml_v3/fixtures/g1/audio/transient_burst/transient_burst_96000.wav
81e3f3824e843f47c6e44efb66e82d6161ee277df22118f2d49aaf8fa2a63429  ml_v3/fixtures/g1/audio/ultrasonic_96k/ultrasonic_96k_96000.wav
d4776aeb6434ea777b91fc4981dfae4f7b1c3dedec55c468f2ade9525dd709c5  ml_v3/fixtures/g1/examples/admission_batch.json
2692f6cb9b0037200554483c99fdfb80d868091bee710c78501c330ace2aa329  ml_v3/fixtures/g1/examples/annotation.json
2b1feade0aae66d6d104e994511f2f482445f819d124a49275b9808708b4af1a  ml_v3/fixtures/g1/examples/asset_manifest.json
28a623579e90c9aeee0c3f7f66d02d6d126daed759b230f406b9cc054c2c8a5a  ml_v3/fixtures/g1/examples/benchmark_power_plan.json
6d0f81b3504cddb70fab4562a1f34d437d8008ff8a28e70d589f980881c7e2fe  ml_v3/fixtures/g1/examples/calibration_policy.json
de430325cacc9488a15965c2fcb6e8e59f82f932dffcb6758f390d903d70f3be  ml_v3/fixtures/g1/examples/prediction.json
513c3baf7aaed8eb1a15f7d2e875a3015479fc2ece0378e75cfceadefe68a6ef  ml_v3/fixtures/g1/fixture_spec_v1.json
d2c35ccc12643f2520c8a50d2e27fd3631216bb9a8ce63a34193412e74d1c10e  ml_v3/fixtures/g1/metrology_lock.json
fa506142afd8a0794f2093a428841b445b130e2ee0f681e18609b1a9b9cd5247  ml_v3/fixtures/g1/schema_registry_v1.json
```

---

## B.47 FILE: `ml_v3/fixtures/g1/examples/asset_manifest.json`

**Path logico:** `ml_v3/fixtures/g1/examples/asset_manifest.json`  
**Bytes:** 735  
**Lines:** 1

```json
{"admission_batch_id":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","asset_id":"asset-001","attribution":"","benchmark_families":["tonal-controlled","clean-safety"],"channels":2,"derivative_kind":null,"development_pilot":false,"duration_s":12.5,"electronic_subgenre":null,"group_id":"fsld:track001","group_primary_domain":"music","group_primary_profile":"generic","ledger_id":"","license_class":"CC0","license_url":"","parent_asset_id":null,"primary_domain":"music","relative_path":"audio/asset-001.wav","sample_rate":48000,"schema":"aieq-v3-asset-manifest-1","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","source_family":"fsld","source_profile":"generic","split_role":"calibration"}
```

---

## B.48 FILE: `ml_v3/fixtures/g1/examples/admission_batch.json`

**Path logico:** `ml_v3/fixtures/g1/examples/admission_batch.json`  
**Bytes:** 608  
**Lines:** 1

```json
{"admission_batch_id":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","inclusion_rules_version":"incl-v1","reviewer_id":"reviewer-a","roster_commit":"deadbeef","roster_sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","salt_commitment":"950ae8385b1c764d5c7094c825bf9ba87963fd08aaecd033cff7af80776dc4a2","salt_reveal":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","schema":"aieq-v3-admission-batch-1","source_snapshot_id":"snap-1","source_snapshot_sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","status":"admitted"}
```

---

## B.49 FILE: `ml_v3/fixtures/g1/examples/annotation.json`

**Path logico:** `ml_v3/fixtures/g1/examples/annotation.json`  
**Bytes:** 2318  
**Lines:** 1

```json
{"annotator_id":"ann-1","asset_id":"asset-001","clean_for_action":true,"complete_types":["Resonance","Harshness","Muddiness","Sibilance","Boominess","Thinness","BoxyMidrange","DullSound"],"dynamic_events":[],"evaluation_unit_id":"eu-001","explicit_negative_types":["Resonance","Harshness","Muddiness","Sibilance","Boominess","Thinness","BoxyMidrange","DullSound"],"global_actionable":false,"notes":"","pass_id":"pass-1","profile":"generic","schema":"aieq-v3-annotation-1","segment_end_s":8.0,"segment_start_s":0.0,"semantic_regions":[],"tonal_actionable_mask":[false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false],"tonal_confidence":[0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5],"tonal_correction_db":[0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0],"tool_version":"annot-tool-1"}
```

---

## B.50 FILE: `ml_v3/fixtures/g1/examples/prediction.json`

**Path logico:** `ml_v3/fixtures/g1/examples/prediction.json`  
**Bytes:** 2341  
**Lines:** 1

```json
{"anomaly_score_ref":{"dtype":"float32_le","num_feature_frames":16,"relative_path":"surfaces/score.npy","sha256":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"},"anomaly_severity_ref":{"dtype":"float32_le","num_feature_frames":16,"relative_path":"surfaces/severity.npy","sha256":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"},"asset_id":"asset-001","calibration_policy_id":"calib-1","calibration_policy_sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","events":[],"frontend_contract_sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","model_id":"model-x","model_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","profile":"generic","schema":"aieq-v3-prediction-1","segment_end_s":8.0,"segment_start_s":0.0,"semantic_bundles":[],"tonal_confidence":[0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5],"tonal_curve_db":[0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0],"tonal_score":[0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5]}
```

---

## B.51 FILE: `ml_v3/fixtures/g1/examples/calibration_policy.json`

**Path logico:** `ml_v3/fixtures/g1/examples/calibration_policy.json`  
**Bytes:** 1499  
**Lines:** 1

```json
{"anomaly_calibrator":{"algorithm":"platt","parameters":{"A":1.0}},"anomaly_class_thresholds":[0.5,0.5,0.5],"calibration_manifest_sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","candidate_extractor_version":"cand-v1","frontend_sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","model_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","policy_id":"calib-1","policy_version":"1","prediction_schema_id":"aieq-v3-prediction-1","prediction_schema_sha256":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","region_to_bundle":{"parameters":{},"rule_id":"r2b-v1"},"schema":"aieq-v3-calibration-policy-1","score_to_confidence":[{"confidence":0.0,"score":0.0},{"confidence":0.4,"score":0.5},{"confidence":1.0,"score":1.0}],"semantic_type_thresholds":[0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5],"threshold_to_actionable":{"parameters":{},"rule_id":"t2a-v1"},"tonal_band_thresholds":[0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5],"tonal_calibrator":{"algorithm":"isotonic","parameters":{}}}
```

---

## B.52 FILE: `ml_v3/fixtures/g1/examples/benchmark_power_plan.json`

**Path logico:** `ml_v3/fixtures/g1/examples/benchmark_power_plan.json`  
**Bytes:** 1210  
**Lines:** 1

```json
{"alpha_plan":0.025,"families":[{"family_id":"tonal-controlled","floor_contractual":30,"n_power":40,"n_required":40,"unit":"group_id"},{"family_id":"tonal-natural","floor_contractual":30,"n_power":40,"n_required":40,"unit":"group_id"},{"family_id":"anomaly-natural","floor_contractual":30,"n_power":40,"n_required":40,"unit":"group_id"},{"family_id":"clean-safety","floor_contractual":149,"n_power":160,"n_required":160,"unit":"group_id"},{"family_id":"electronic-stratified","floor_contractual":30,"n_power":40,"n_required":40,"unit":"group_id"}],"gates":[{"metric_id":"curve_err_rel","min_effect_size":0.1,"n_power":40,"n_required":40,"orientation":"greater","statistic":"paired_bootstrap","support":32},{"metric_id":"clean_actionable_rate","min_effect_size":0.01,"n_power":160,"n_required":160,"orientation":"less","statistic":"exact_binomial","support":149}],"implementation_version":"power-tool-1","m":2,"pilot_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","plan_id":"power-1","power_source_role":"development_pilot","result":{"n_selected":true,"status":"frozen"},"schema":"aieq-v3-benchmark-power-plan-1","seed_base":20260719,"support":{"notes":"fixture","pilot_groups":40}}
```

---

## B.53 FILE: `ml_v3/tests/test_g1a_identity_canonical.py`

**Path logico:** `ml_v3/tests/test_g1a_identity_canonical.py`  
**Bytes:** 7670  
**Lines:** 207

```python
"""G1a identity / canonical / coverage fail-closed tests."""
from __future__ import annotations

import hashlib
import unittest

from ml_v3.contracts.canonical import CanonicalError, loads_strict, sha256_of_obj
from ml_v3.contracts.constants import CONDITIONING_PROFILES, SCHEMA_IDS
from ml_v3.contracts.coverage import CoverageError, check_calibration_coverage
from ml_v3.contracts.split import (
    SplitError,
    canonical_roster,
    pack_group_id,
    roster_from_identity_index,
    source_snapshot_sha256,
    upstream_group_id,
    validate_identity_index,
    verify_roster_source_snapshot,
)


def _empty_profile_partition() -> dict[str, list[str]]:
    return {profile: [] for profile in CONDITIONING_PROFILES}


def _sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _identity_index() -> dict:
    fam = "fsld"
    up = "track001"
    gid = f"{fam}:{up}"
    digests = sorted([_sha("a"), _sha("b")])
    pack_gid = pack_group_id("packfam", digests)
    return {
        "schema": SCHEMA_IDS["source_identity_index"],
        "source_snapshot_id": "snap-1",
        "alias_mapping_version": "alias-v1",
        "inclusion_rules_version": "incl-v1",
        "groups": [
            {
                "group_id": gid,
                "group_primary_profile": "edm",
                "group_primary_domain": "electronic",
                "source_family": fam,
                "upstream_id": up,
                "pack_audio_sha256": [],
                "aliases": ["alias-a"],
            },
            {
                "group_id": pack_gid,
                "group_primary_profile": "generic",
                "group_primary_domain": "music",
                "source_family": "packfam",
                "upstream_id": None,
                "pack_audio_sha256": digests,
                "aliases": [],
            },
        ],
    }


class ColonForbiddenTests(unittest.TestCase):
    def test_source_family_rejects_colon(self):
        with self.assertRaises(SplitError):
            upstream_group_id("bad:fam", "id1")

    def test_upstream_id_rejects_colon(self):
        with self.assertRaises(SplitError):
            upstream_group_id("fam", "bad:id")

    def test_upstream_must_not_imitate_pack_fallback(self):
        with self.assertRaises(SplitError):
            upstream_group_id("fam", "pack:deadbeef")

    def test_identity_index_rejects_colon_in_family(self):
        index = _identity_index()
        index["groups"][0]["source_family"] = "bad:fam"
        index["groups"][0]["group_id"] = "bad:fam:track001"
        with self.assertRaises(SplitError):
            validate_identity_index(index)


class SourceSnapshotSha256Tests(unittest.TestCase):
    def test_roster_binds_to_canonical_identity_hash(self):
        index = _identity_index()
        # groups must be sorted by utf-8 group_id
        index["groups"] = sorted(
            index["groups"], key=lambda row: row["group_id"].encode("utf-8"))
        expected = sha256_of_obj(index)
        self.assertEqual(source_snapshot_sha256(index), expected)
        roster = roster_from_identity_index(index)
        self.assertEqual(roster["source_snapshot_sha256"], expected)
        self.assertEqual(roster["schema"], SCHEMA_IDS["admission_roster"])
        verify_roster_source_snapshot(roster, index)
        canonical_roster(roster, identity_index=index)

    def test_free_form_snapshot_sha_rejected(self):
        index = _identity_index()
        index["groups"] = sorted(
            index["groups"], key=lambda row: row["group_id"].encode("utf-8"))
        roster = roster_from_identity_index(index)
        tampered = dict(roster)
        tampered["source_snapshot_sha256"] = "ff" * 32
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(tampered, index)
        with self.assertRaises(SplitError):
            canonical_roster(tampered, identity_index=index)

    def test_wrong_identity_schema_rejected(self):
        index = _identity_index()
        index["schema"] = "not-the-contract-schema"
        with self.assertRaises(SplitError):
            source_snapshot_sha256(index)


class CanonicalReadPathTests(unittest.TestCase):
    def test_rejects_nan_infinity_tokens(self):
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": NaN}')
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": Infinity}')
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": -Infinity}')

    def test_rejects_overflow_1e400_as_non_finite(self):
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": 1e400}')
        with self.assertRaises(CanonicalError):
            loads_strict('{"nested": {"y": [1e309]}}')

    def test_rejects_duplicate_keys(self):
        with self.assertRaises(CanonicalError):
            loads_strict('{"a": 1, "a": 2}')

    def test_accepts_finite(self):
        self.assertEqual(loads_strict('{"x": 1.5, "y": 0}\n'), {"x": 1.5, "y": 0})


class CoverageDisjointTests(unittest.TestCase):
    def _empty_anomaly(self) -> dict:
        return {
            klass: {
                "positive_groups": [],
                "negative_groups": [],
                "negative_clean_groups": [],
                "positive_groups_by_family": {},
                "negative_groups_by_profile": {},
            }
            for klass in ("Resonance", "Harshness", "Sibilance")
        }

    def test_positive_negative_overlap_raises(self):
        tonal = {
            "actionable_groups": [],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": [],
            "per_profile_actionable": _empty_profile_partition(),
            "per_profile_clean": _empty_profile_partition(),
        }
        anomaly = self._empty_anomaly()
        anomaly["Resonance"]["positive_groups"] = ["g1", "g2"]
        anomaly["Resonance"]["negative_groups"] = ["g2", "g3"]
        anomaly["Resonance"]["positive_groups_by_family"] = {"f": ["g1", "g2"]}
        anomaly["Resonance"]["negative_groups_by_profile"] = {
            "generic": ["g2", "g3"]}
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, anomaly)

    def test_family_strata_must_match_declared_positives(self):
        tonal = {
            "actionable_groups": [],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": [],
            "per_profile_actionable": _empty_profile_partition(),
            "per_profile_clean": _empty_profile_partition(),
        }
        anomaly = self._empty_anomaly()
        anomaly["Resonance"]["positive_groups"] = ["g1", "g2"]
        anomaly["Resonance"]["positive_groups_by_family"] = {"f": ["g1"]}  # missing g2
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, anomaly)

    def test_tonal_actionable_clean_disjoint(self):
        tonal = {
            "actionable_groups": ["a1"],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": ["a1"],
            "per_profile_actionable": {
                **_empty_profile_partition(),
                "generic": ["a1"],
            },
            "per_profile_clean": {
                **_empty_profile_partition(),
                "generic": ["a1"],
            },
        }
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, self._empty_anomaly())


if __name__ == "__main__":
    unittest.main()
```

---

## B.54 FILE: `ml_v3/tests/test_g1a_role_intervals.py`

**Path logico:** `ml_v3/tests/test_g1a_role_intervals.py`  
**Bytes:** 6283  
**Lines:** 162

```python
"""G1a boundary tests: ROLE_INTERVALS exact integer thresholds (§8.2.5)."""
from __future__ import annotations

import hashlib
import hmac
import unittest

from ml_v3.contracts.constants import (CONTRACT_REVISION, ROLE_INTERVALS_EXACT,
                                       ROLE_PREFIX, SCHEMA_IDS, SPLIT_ROLES)
from ml_v3.contracts.split import SplitError, assign_role, require_admission_batch_id_hex


NUL = b"\x00"
SALT = b"\x11" * 32
BATCH_HEX = "ab" * 32  # valid lowercase hex-64


def _role_for_value(value: int) -> str:
    """Reference oracle: value * 20 < n * 2**64."""
    two64 = 2 ** 64
    for role, numerator in ROLE_INTERVALS_EXACT:
        if value * 20 < numerator * two64:
            return role
    raise AssertionError("unreachable")


def _floor_bug_role(value: int) -> str:
    """Legacy buggy comparison that used // 20 truncation."""
    for role, numerator in ROLE_INTERVALS_EXACT:
        upper = numerator * (2 ** 64) // 20
        if value < upper:
            return role
    return SPLIT_ROLES[-1]


class RoleIntervalExactTests(unittest.TestCase):
    def test_contract_revision_is_rev6_micro_amend(self):
        self.assertIn("REVISIONE 6 CONSOLIDATA", CONTRACT_REVISION)
        self.assertIn("micro-amend", CONTRACT_REVISION)
        self.assertIn("6d254d0a", CONTRACT_REVISION)
        self.assertNotIn("REVISIONE 5", CONTRACT_REVISION)

    def test_schema_ids_include_identity_and_roster(self):
        self.assertEqual(SCHEMA_IDS["source_identity_index"],
                         "aieq-v3-source-identity-index-1")
        self.assertEqual(SCHEMA_IDS["admission_roster"],
                         "aieq-v3-admission-roster-1")

    def test_intervals_store_numerators_not_floored_thresholds(self):
        # Must be (role, numerator) — never pre-divided by 20.
        self.assertEqual(
            ROLE_INTERVALS_EXACT,
            (
                ("train", 11),
                ("validation", 13),
                ("calibration", 15),
                ("development-metric", 17),
                ("final-test", 20),
            ),
        )

    def test_floor_truncation_diverges_at_train_boundary(self):
        # For n=11, floored = 11*2**64 // 20 leaves a gap of 16.
        floored = 11 * (2 ** 64) // 20
        # value == floored: buggy path excludes train; exact path includes train.
        self.assertEqual(_floor_bug_role(floored), "validation")
        self.assertEqual(_role_for_value(floored), "train")
        # Last value still in train under exact rule:
        last_train = (11 * (2 ** 64) - 1) // 20
        self.assertEqual(_role_for_value(last_train), "train")
        self.assertEqual(_role_for_value(last_train + 1), "validation")

    def test_assign_role_matches_exact_formula_via_forged_hmac(self):
        """Forge salt/message is hard; instead unit-test the comparison formula
        through a monkeypatch of hmac digest first 8 bytes."""
        row = {
            "group_id": "fam:up1",
            "group_primary_profile": "generic",
            "group_primary_domain": "music",
            "source_family": "fam",
        }
        # Probe values that sit in the truncation gap for train (n=11).
        floored = 11 * (2 ** 64) // 20
        probes = [
            0,
            floored - 1,
            floored,
            floored + 15,  # still train under exact; validation under floor bug
            (11 * (2 ** 64)) // 20 + 16,  # first validation under exact? check
            (13 * (2 ** 64) - 1) // 20,
            (13 * (2 ** 64)) // 20,
            (2 ** 64) - 1,
        ]
        original_new = hmac.new

        for value in probes:
            expected = _role_for_value(value)

            def _fake_new(key, msg=None, digestmod=None):  # noqa: ANN001
                class _H:
                    def digest(self_inner):
                        return value.to_bytes(8, "big") + b"\x00" * 24
                return _H()

            hmac.new = _fake_new  # type: ignore[assignment]
            try:
                got = assign_role(SALT, BATCH_HEX, row)
            finally:
                hmac.new = original_new  # type: ignore[assignment]
            self.assertEqual(got, expected, f"value={value}")

        # Explicitly assert the known train-boundary gap between floor bug and exact.
        self.assertNotEqual(_floor_bug_role(floored), _role_for_value(floored))

    def test_admission_batch_id_must_be_hex64(self):
        require_admission_batch_id_hex(BATCH_HEX)
        with self.assertRaises(SplitError):
            require_admission_batch_id_hex("not-hex")
        with self.assertRaises(SplitError):
            require_admission_batch_id_hex("AB" * 32)  # uppercase rejected
        with self.assertRaises(SplitError):
            require_admission_batch_id_hex("\x00" + "ab" * 31)


class HmacHexAsciiTests(unittest.TestCase):
    def test_role_and_pilot_messages_embed_hex64_ascii_not_raw32(self):
        from ml_v3.contracts.split import assign_pilot

        row = {
            "group_id": "fam:up1",
            "group_primary_profile": "generic",
            "group_primary_domain": "music",
            "source_family": "fam",
        }
        captured: list[bytes] = []
        original_new = hmac.new

        def _spy(key, msg=None, digestmod=None):  # noqa: ANN001
            captured.append(msg)
            return original_new(key, msg, digestmod)

        hmac.new = _spy  # type: ignore[assignment]
        try:
            assign_role(SALT, BATCH_HEX, row)
            assign_pilot(SALT, BATCH_HEX, row["group_id"], "development-metric")
        finally:
            hmac.new = original_new  # type: ignore[assignment]

        self.assertEqual(len(captured), 2)
        role_msg, pilot_msg = captured
        # Role: prefix + NUL + hex64 + NUL + ...
        self.assertTrue(role_msg.startswith(ROLE_PREFIX + NUL + BATCH_HEX.encode("ascii") + NUL))
        self.assertNotIn(bytes.fromhex(BATCH_HEX), role_msg)  # raw-32 must NOT appear
        self.assertIn(BATCH_HEX.encode("ascii"), role_msg)
        self.assertIn(BATCH_HEX.encode("ascii"), pilot_msg)
        # Reject non-hex batch id before MAC
        with self.assertRaises(SplitError):
            assign_role(SALT, "zz" * 32, row)


if __name__ == "__main__":
    unittest.main()
```

---

## B.55 FILE: `ml_v3/tests/test_g1a_t1_gates.py`

**Path logico:** `ml_v3/tests/test_g1a_t1_gates.py`  
**Bytes:** 10920  
**Lines:** 297

```python
"""G1a T1 gate reject-path tests (H1/H2/M1/M2 + HMAC ceil boundaries)."""
from __future__ import annotations

import hashlib
import hmac
import unittest

from ml_v3.contracts.constants import (
    CONDITIONING_PROFILES,
    ROLE_INTERVALS_EXACT,
    SCHEMA_IDS,
)
from ml_v3.contracts.coverage import CoverageError, check_calibration_coverage
from ml_v3.contracts.split import (
    SplitError,
    assign_batch,
    assign_pilot,
    assign_role,
    exact_projection,
    pack_group_id,
    roster_from_identity_index,
    salt_commitment,
    validate_identity_index,
    verify_roster_source_snapshot,
)


def _sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _sorted_identity_index() -> dict:
    fam = "fsld"
    up = "track001"
    gid = f"{fam}:{up}"
    digests = sorted([_sha("a"), _sha("b")])
    pack_gid = pack_group_id("packfam", digests)
    index = {
        "schema": SCHEMA_IDS["source_identity_index"],
        "source_snapshot_id": "snap-1",
        "alias_mapping_version": "alias-v1",
        "inclusion_rules_version": "incl-v1",
        "groups": [
            {
                "group_id": gid,
                "group_primary_profile": "edm",
                "group_primary_domain": "electronic",
                "source_family": fam,
                "upstream_id": up,
                "pack_audio_sha256": [],
                "aliases": ["alias-a"],
            },
            {
                "group_id": pack_gid,
                "group_primary_profile": "generic",
                "group_primary_domain": "music",
                "source_family": "packfam",
                "upstream_id": None,
                "pack_audio_sha256": digests,
                "aliases": [],
            },
        ],
    }
    index["groups"] = sorted(
        index["groups"], key=lambda row: row["group_id"].encode("utf-8"))
    return index


def _empty_profile_partition() -> dict[str, list[str]]:
    return {profile: [] for profile in CONDITIONING_PROFILES}


def _empty_anomaly() -> dict:
    return {
        klass: {
            "positive_groups": [],
            "negative_groups": [],
            "negative_clean_groups": [],
            "positive_groups_by_family": {},
            "negative_groups_by_profile": {},
        }
        for klass in ("Resonance", "Harshness", "Sibilance")
    }


SALT = b"\x22" * 32
BATCH_HEX = "cd" * 32


class H1IdentityMandatoryTests(unittest.TestCase):
    def test_assign_batch_without_identity_keyword_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        commitment = salt_commitment(SALT)
        with self.assertRaises(TypeError):
            assign_batch(SALT, roster, commitment)  # type: ignore[call-arg]

    def test_assign_batch_identity_none_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        commitment = salt_commitment(SALT)
        with self.assertRaises(SplitError):
            assign_batch(SALT, roster, commitment, identity_index=None)

    def test_free_form_snapshot_hex_rejected_on_normative_path(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        tampered = dict(roster)
        tampered["source_snapshot_sha256"] = "ab" * 32
        commitment = salt_commitment(SALT)
        with self.assertRaises(SplitError):
            assign_batch(SALT, tampered, commitment, identity_index=index)


class H2FullProjectionTests(unittest.TestCase):
    def test_full_projection_pass(self):
        index = _sorted_identity_index()
        rows = validate_identity_index(index)
        roster = roster_from_identity_index(index)
        self.assertEqual(roster["groups"], exact_projection(rows))
        verify_roster_source_snapshot(roster, index)
        batch = assign_batch(
            SALT, roster, salt_commitment(SALT), identity_index=index)
        self.assertEqual(len(batch["assignments"]), len(roster["groups"]))

    def test_minus_group_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        roster["groups"] = list(roster["groups"][:-1])
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_plus_group_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        extra = {
            "group_id": "extrafam:extra1",
            "group_primary_profile": "bass",
            "group_primary_domain": "music",
            "source_family": "extrafam",
        }
        roster["groups"] = list(roster["groups"]) + [extra]
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_projected_field_changed_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        groups = [dict(row) for row in roster["groups"]]
        groups[0]["group_primary_domain"] = "tampered-domain"
        roster["groups"] = groups
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_spurious_pack_group_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        digests = sorted([_sha("x"), _sha("y")])
        spurious_gid = pack_group_id("otherpack", digests)
        roster["groups"] = list(roster["groups"]) + [{
            "group_id": spurious_gid,
            "group_primary_profile": "drums",
            "group_primary_domain": "music",
            "source_family": "otherpack",
        }]
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_different_snapshot_id_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        roster["source_snapshot_id"] = "snap-OTHER"
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)


class M1TonalProfilePartitionTests(unittest.TestCase):
    def _tonal_base(self) -> dict:
        return {
            "actionable_groups": ["a1", "a2"],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": ["c1"],
            "per_profile_actionable": {
                **_empty_profile_partition(),
                "generic": ["a1"],
                "edm": ["a2"],
            },
            "per_profile_clean": {
                **_empty_profile_partition(),
                "vocals": ["c1"],
            },
        }

    def test_same_group_id_two_profiles_fails(self):
        tonal = self._tonal_base()
        tonal["per_profile_actionable"]["generic"] = ["a1"]
        tonal["per_profile_actionable"]["edm"] = ["a1", "a2"]
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, _empty_anomaly())

    def test_missing_profile_fails(self):
        tonal = self._tonal_base()
        del tonal["per_profile_actionable"]["bass"]
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, _empty_anomaly())

    def test_extra_non_canonical_profile_fails(self):
        tonal = self._tonal_base()
        tonal["per_profile_actionable"]["techno"] = []
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, _empty_anomaly())

    def test_valid_exact_partition_reaches_floors(self):
        # Empty declared sets with exact empty profile keys is structurally valid.
        tonal = {
            "actionable_groups": [],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": [],
            "per_profile_actionable": _empty_profile_partition(),
            "per_profile_clean": _empty_profile_partition(),
        }
        report = check_calibration_coverage(tonal, _empty_anomaly())
        self.assertFalse(report.ok)  # floors N/A, but no CoverageError


class M2AssignPilotFailClosedTests(unittest.TestCase):
    def test_salt_lengths_fail_before_role_shortcut(self):
        for length in (31, 33, 1):
            with self.subTest(length=length):
                with self.assertRaises(SplitError):
                    assign_pilot(b"\x00" * length, BATCH_HEX, "fam:g1", "train")

    def test_invalid_batch_hex_fails_even_when_role_not_development(self):
        with self.assertRaises(SplitError):
            assign_pilot(SALT, "zz" * 32, "fam:g1", "train")
        with self.assertRaises(SplitError):
            assign_pilot(SALT, "AB" * 32, "fam:g1", "final-test")

    def test_nul_group_id_fails_before_shortcut(self):
        with self.assertRaises(SplitError):
            assign_pilot(SALT, BATCH_HEX, "fam:\x00g1", "train")

    def test_unknown_role_fails(self):
        with self.assertRaises(SplitError):
            assign_pilot(SALT, BATCH_HEX, "fam:g1", "not-a-role")

    def test_non_development_returns_false_after_validation(self):
        self.assertFalse(assign_pilot(SALT, BATCH_HEX, "fam:g1", "train"))


class HmacCeilBoundaryTests(unittest.TestCase):
    def test_ceil_boundaries_for_role_numerators(self):
        """For n in (11,13,15,17), T = ceil(n * 2**64 / 20); probe T-1 and T."""
        two64 = 2 ** 64
        row = {
            "group_id": "fam:up1",
            "group_primary_profile": "generic",
            "group_primary_domain": "music",
            "source_family": "fam",
        }
        original_new = hmac.new

        roles = [name for name, _ in ROLE_INTERVALS_EXACT]
        for role, numerator in ROLE_INTERVALS_EXACT[:-1]:
            # T = ceil(n * 2**64 / 20) via integer arithmetic (no float).
            threshold = (numerator * two64 + 19) // 20
            next_role_name = roles[roles.index(role) + 1]

            for value, expected in (
                (threshold - 1, role),
                (threshold, next_role_name),
            ):
                def _fake_new(key, msg=None, digestmod=None, *, _v=value):  # noqa: ANN001
                    class _H:
                        def digest(self_inner):
                            return _v.to_bytes(8, "big") + b"\x00" * 24
                    return _H()

                hmac.new = _fake_new  # type: ignore[assignment]
                try:
                    got = assign_role(SALT, BATCH_HEX, row)
                finally:
                    hmac.new = original_new  # type: ignore[assignment]
                self.assertEqual(
                    got, expected,
                    f"n={numerator} value={value} expected={expected} got={got}")


if __name__ == "__main__":
    unittest.main()
```

---

## B.56 FILE: `ml_v3/tests/_g1a_t2_fixtures.py`

**Path logico:** `ml_v3/tests/_g1a_t2_fixtures.py`  
**Bytes:** 7667  
**Lines:** 238

```python
"""Minimal valid example documents for G1a T2 validators / fixtures.

Instance goldens live under ``ml_v3/fixtures/g1/examples/`` (not the
normative schema surface — that is ``schema_registry_v1.json``).
"""
from __future__ import annotations

from ml_v3.contracts.constants import (
    BENCHMARK_FAMILIES,
    GRID_BANDS,
    PROBLEM_TYPES,
    SCHEMA_IDS,
)
from ml_v3.contracts.split import salt_commitment

_HEX_A = "aa" * 32
_HEX_B = "bb" * 32
_HEX_C = "cc" * 32
_HEX_D = "dd" * 32
_HEX_E = "ee" * 32
# §8.2.4 coherent commit-reveal: reveal = hex(32 raw salt bytes);
# commitment = SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt).
_SALT_REVEAL = _HEX_E
_SALT_COMMITMENT = salt_commitment(bytes.fromhex(_SALT_REVEAL))
# Historical mismatched pair (dd commitment / ee reveal) — must FAIL.
_MISMATCHED_SALT_COMMITMENT = _HEX_D


def zeros120() -> list[float]:
    return [0.0] * GRID_BANDS


def false120() -> list[bool]:
    return [False] * GRID_BANDS


def half120() -> list[float]:
    return [0.5] * GRID_BANDS


def asset_manifest(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["asset_manifest"],
        "asset_id": "asset-001",
        "relative_path": "audio/asset-001.wav",
        "sha256": _HEX_A,
        "group_id": "fsld:track001",
        "admission_batch_id": _HEX_B,
        "split_role": "calibration",
        "benchmark_families": ["tonal-controlled", "clean-safety"],
        "development_pilot": False,
        "source_profile": "generic",
        "primary_domain": "music",
        "group_primary_profile": "generic",
        "group_primary_domain": "music",
        "source_family": "fsld",
        "electronic_subgenre": None,
        "sample_rate": 48000,
        "channels": 2,
        "duration_s": 12.5,
        "parent_asset_id": None,
        "derivative_kind": None,
        "license_class": "CC0",
        "license_url": "",
        "attribution": "",
        "ledger_id": "",
    }
    doc.update(overrides)
    return doc


def admission_batch(**overrides: object) -> dict:
    # alias_mapping_version intentionally absent: identity-index (§8) field,
    # not part of §9.1 admission-batch freeze-from-prose envelope.
    doc: dict = {
        "schema": SCHEMA_IDS["admission_batch"],
        "admission_batch_id": _HEX_B,
        "source_snapshot_id": "snap-1",
        "source_snapshot_sha256": _HEX_C,
        "inclusion_rules_version": "incl-v1",
        "roster_sha256": _HEX_B,
        "salt_commitment": _SALT_COMMITMENT,
        "salt_reveal": _SALT_REVEAL,
        "roster_commit": "deadbeef",
        "reviewer_id": "reviewer-a",
        "status": "admitted",
    }
    doc.update(overrides)
    return doc


def admission_batch_mismatched_commitment(**overrides: object) -> dict:
    """Legacy dd/ee pair: commitment does not match reveal (§8.2.4 FAIL)."""
    return admission_batch(
        salt_commitment=_MISMATCHED_SALT_COMMITMENT,
        salt_reveal=_SALT_REVEAL,
        **overrides,
    )


def annotation_clean(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["annotation"],
        "asset_id": "asset-001",
        "annotator_id": "ann-1",
        "pass_id": "pass-1",
        "profile": "generic",
        "evaluation_unit_id": "eu-001",
        "segment_start_s": 0.0,
        "segment_end_s": 8.0,
        "tonal_correction_db": zeros120(),
        "tonal_confidence": half120(),
        "tonal_actionable_mask": false120(),
        "semantic_regions": [],
        "dynamic_events": [],
        "complete_types": list(PROBLEM_TYPES),
        "explicit_negative_types": list(PROBLEM_TYPES),
        "global_actionable": False,
        "clean_for_action": True,
        "notes": "",
        "tool_version": "annot-tool-1",
    }
    doc.update(overrides)
    return doc


def prediction(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["prediction"],
        "asset_id": "asset-001",
        "model_id": "model-x",
        "model_sha256": _HEX_A,
        "frontend_contract_sha256": _HEX_B,
        "calibration_policy_id": "calib-1",
        "calibration_policy_sha256": _HEX_C,
        "profile": "generic",
        "tonal_curve_db": zeros120(),
        "tonal_score": half120(),
        "tonal_confidence": half120(),
        "segment_start_s": 0.0,
        "segment_end_s": 8.0,
        "semantic_bundles": [],
        "events": [],
        "anomaly_score_ref": {
            "relative_path": "surfaces/score.npy",
            "sha256": _HEX_D,
            "num_feature_frames": 16,
            "dtype": "float32_le",
        },
        "anomaly_severity_ref": {
            "relative_path": "surfaces/severity.npy",
            "sha256": _HEX_E,
            "num_feature_frames": 16,
            "dtype": "float32_le",
        },
    }
    doc.update(overrides)
    return doc


def calibration_policy(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["calibration_policy"],
        "policy_id": "calib-1",
        "policy_version": "1",
        "model_sha256": _HEX_A,
        "frontend_sha256": _HEX_B,
        "calibration_manifest_sha256": _HEX_C,
        "prediction_schema_id": SCHEMA_IDS["prediction"],
        "prediction_schema_sha256": _HEX_D,
        "tonal_calibrator": {"algorithm": "isotonic", "parameters": {}},
        "anomaly_calibrator": {"algorithm": "platt", "parameters": {"A": 1.0}},
        "tonal_band_thresholds": half120(),
        "semantic_type_thresholds": [0.5] * 8,
        "anomaly_class_thresholds": [0.5, 0.5, 0.5],
        "candidate_extractor_version": "cand-v1",
        "score_to_confidence": [
            {"score": 0.0, "confidence": 0.0},
            {"score": 0.5, "confidence": 0.4},
            {"score": 1.0, "confidence": 1.0},
        ],
        "region_to_bundle": {"rule_id": "r2b-v1", "parameters": {}},
        "threshold_to_actionable": {"rule_id": "t2a-v1", "parameters": {}},
    }
    doc.update(overrides)
    return doc


def benchmark_power_plan(**overrides: object) -> dict:
    m = 2
    families = [
        {
            "family_id": fam,
            "floor_contractual": 149 if fam == "clean-safety" else 30,
            "n_power": 160 if fam == "clean-safety" else 40,
            "n_required": 160 if fam == "clean-safety" else 40,
            "unit": "group_id",
        }
        for fam in BENCHMARK_FAMILIES
    ]
    # fix n_required = max(floor, n_power)
    for row in families:
        row["n_required"] = max(row["floor_contractual"], row["n_power"])
    doc: dict = {
        "schema": SCHEMA_IDS["benchmark_power_plan"],
        "plan_id": "power-1",
        "implementation_version": "power-tool-1",
        "seed_base": 20260719,
        "pilot_sha256": _HEX_A,
        "power_source_role": "development_pilot",
        "m": m,
        "alpha_plan": 0.05 / m,
        "families": families,
        "gates": [
            {
                "metric_id": "curve_err_rel",
                "statistic": "paired_bootstrap",
                "orientation": "greater",
                "min_effect_size": 0.10,
                "n_power": 40,
                "n_required": 40,
                "support": 32,
            },
            {
                "metric_id": "clean_actionable_rate",
                "statistic": "exact_binomial",
                "orientation": "less",
                "min_effect_size": 0.01,
                "n_power": 160,
                "n_required": 160,
                "support": 149,
            },
        ],
        "support": {"pilot_groups": 40, "notes": "fixture"},
        "result": {"status": "frozen", "n_selected": True},
    }
    doc.update(overrides)
    return doc
```

---

## B.57 FILE: `ml_v3/tests/test_g1a_t2_schemas.py`

**Path logico:** `ml_v3/tests/test_g1a_t2_schemas.py`  
**Bytes:** 14317  
**Lines:** 404

```python
"""G1a T2: six JSON schemas + fail-closed validator (happy + reject paths)."""
from __future__ import annotations

import copy
import unittest
from pathlib import Path

from ml_v3.contracts.canonical import (
    canonical_bytes,
    loads_strict,
    sha256_of_obj,
    write_canonical,
)
from ml_v3.contracts.constants import GRID_BANDS, SCHEMA_IDS, SPLIT_ROLES
from ml_v3.contracts.schemas import (
    SCHEMA_REGISTRY,
    SCHEMA_REGISTRY_RELPATH,
    frozen_schema_registry,
    schema_for,
    schema_ids_t2,
    schema_registry_sha256,
)
from ml_v3.contracts.split import validate_manifest_split_invariants
from ml_v3.contracts.validate import SchemaError, validate, validate_schema_id
from ml_v3.tests._g1a_t2_fixtures import (
    admission_batch,
    admission_batch_mismatched_commitment,
    annotation_clean,
    asset_manifest,
    benchmark_power_plan,
    calibration_policy,
    prediction,
)

# Instance example goldens (not the normative schema surface).
FIXTURE_DIR = Path(__file__).resolve().parents[1] / "fixtures" / "g1" / "examples"
SCHEMA_REGISTRY_FIXTURE = (
    Path(__file__).resolve().parents[1] / "fixtures" / "g1" / "schema_registry_v1.json"
)


class SchemaRegistryTests(unittest.TestCase):
    def test_six_schema_ids_registered(self):
        ids = schema_ids_t2()
        self.assertEqual(len(ids), 6)
        self.assertEqual(set(ids), set(SCHEMA_REGISTRY))
        for schema_id in ids:
            doc = schema_for(schema_id)
            self.assertEqual(doc["$id"], schema_id)
            self.assertIs(doc["additionalProperties"], False)

    def test_schema_ids_match_constants(self):
        expected = {
            SCHEMA_IDS["asset_manifest"],
            SCHEMA_IDS["admission_batch"],
            SCHEMA_IDS["annotation"],
            SCHEMA_IDS["prediction"],
            SCHEMA_IDS["calibration_policy"],
            SCHEMA_IDS["benchmark_power_plan"],
        }
        self.assertEqual(set(schema_ids_t2()), expected)


class HappyPathTests(unittest.TestCase):
    def test_all_six_validate(self):
        docs = [
            asset_manifest(),
            admission_batch(),
            annotation_clean(),
            prediction(),
            calibration_policy(),
            benchmark_power_plan(),
        ]
        for doc in docs:
            with self.subTest(schema=doc["schema"]):
                self.assertEqual(validate(doc), doc["schema"])
                validate_schema_id(doc, doc["schema"])

    def test_manifest_compatible_with_split_invariants(self):
        root = asset_manifest(asset_id="root-1", relative_path="a.wav")
        child = asset_manifest(
            asset_id="child-1",
            relative_path="b.wav",
            sha256="11" * 32,
            parent_asset_id="root-1",
            derivative_kind="crop",
        )
        validate(root)
        validate(child)
        validate_manifest_split_invariants([root, child])

    def test_development_pilot_ok_on_development_metric(self):
        doc = asset_manifest(
            split_role="development-metric", development_pilot=True)
        validate(doc)


class RejectPathManifestTests(unittest.TestCase):
    def test_extra_key_rejected(self):
        doc = asset_manifest()
        doc["extra"] = True
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_missing_key_rejected(self):
        doc = asset_manifest()
        del doc["ledger_id"]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_wrong_schema_id_rejected(self):
        doc = asset_manifest(schema="aieq-v3-asset-manifest-999")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_unknown_split_role_rejected(self):
        doc = asset_manifest(split_role="holdout")
        with self.assertRaises(SchemaError):
            validate(doc)
        self.assertNotIn("holdout", SPLIT_ROLES)

    def test_duplicate_benchmark_family_rejected(self):
        doc = asset_manifest(
            benchmark_families=["tonal-controlled", "tonal-controlled"])
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_pilot_outside_development_rejected(self):
        doc = asset_manifest(split_role="train", development_pilot=True)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_invalid_sha_rejected(self):
        doc = asset_manifest(sha256="ZZ" * 32)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_owned_without_ledger_rejected(self):
        doc = asset_manifest(license_class="OWNED", ledger_id="")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_sample_rate_rejected(self):
        doc = asset_manifest(sample_rate=32000)
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathAdmissionTests(unittest.TestCase):
    def test_roster_sha_must_equal_batch_id(self):
        doc = admission_batch(roster_sha256="ff" * 32)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_bad_status_rejected(self):
        doc = admission_batch(status="pending")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_admitted_requires_salt_reveal(self):
        doc = admission_batch(status="admitted", salt_reveal=None)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_rejected_allows_null_salt_reveal(self):
        doc = admission_batch(status="rejected", salt_reveal=None)
        validate(doc)

    def test_mismatched_salt_commitment_rejected(self):
        """F2: historical dd/ee pair must FAIL commit-reveal (§8.2.4)."""
        doc = admission_batch_mismatched_commitment()
        with self.assertRaises(SchemaError) as ctx:
            validate(doc)
        self.assertIn("salt_commitment", str(ctx.exception))

    def test_rejected_with_mismatched_reveal_rejected(self):
        doc = admission_batch_mismatched_commitment(status="rejected")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_alias_mapping_version_extra_key_rejected(self):
        # Identity-index field must not sneak into admission-batch envelope.
        doc = admission_batch(alias_mapping_version="alias-v1")
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathAnnotationTests(unittest.TestCase):
    def test_curve_length_rejected(self):
        doc = annotation_clean()
        doc["tonal_correction_db"] = [0.0] * (GRID_BANDS - 1)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_curve_out_of_range_rejected(self):
        doc = annotation_clean(clean_for_action=False)
        curve = [0.0] * GRID_BANDS
        curve[0] = 9.5
        doc["tonal_correction_db"] = curve
        doc["complete_types"] = ["Resonance"]
        doc["explicit_negative_types"] = []
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_clean_for_action_nonzero_curve_rejected(self):
        doc = annotation_clean()
        curve = [0.0] * GRID_BANDS
        curve[3] = 0.1
        doc["tonal_correction_db"] = curve
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_problem_type_id_mismatch_rejected(self):
        doc = annotation_clean(clean_for_action=False)
        doc["complete_types"] = ["Resonance"]
        doc["explicit_negative_types"] = []
        doc["semantic_regions"] = [{
            "problem_type": "Resonance",
            "problem_type_id": 3,  # Sibilance id; mismatch
            "start_s": 0.0,
            "end_s": 1.0,
            "band_lo_hz": 1000.0,
            "band_hi_hz": 2000.0,
            "direction": None,
            "severity": 0.5,
            "confidence": 0.5,
            "actionable": True,
        }]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_thinness_requires_direction(self):
        doc = annotation_clean(clean_for_action=False)
        doc["complete_types"] = ["Thinness"]
        doc["explicit_negative_types"] = []
        doc["semantic_regions"] = [{
            "problem_type": "Thinness",
            "problem_type_id": 5,
            "start_s": 0.0,
            "end_s": 1.0,
            "band_lo_hz": None,
            "band_hi_hz": None,
            "direction": None,
            "severity": 0.5,
            "confidence": 0.5,
            "actionable": True,
        }]
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathPredictionTests(unittest.TestCase):
    def test_missing_anomaly_ref_key_rejected(self):
        doc = prediction()
        del doc["anomaly_score_ref"]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_frame_mismatch_rejected(self):
        doc = prediction()
        doc["anomaly_severity_ref"] = dict(doc["anomaly_severity_ref"])
        doc["anomaly_severity_ref"]["num_feature_frames"] = 99
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_events_without_valid_dtype_rejected(self):
        doc = prediction()
        doc["anomaly_score_ref"] = dict(doc["anomaly_score_ref"])
        doc["anomaly_score_ref"]["dtype"] = "float64"
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathCalibrationTests(unittest.TestCase):
    def test_threshold_length_rejected(self):
        doc = calibration_policy()
        doc["semantic_type_thresholds"] = [0.5] * 7
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_non_monotone_score_map_rejected(self):
        doc = calibration_policy()
        doc["score_to_confidence"] = [
            {"score": 0.0, "confidence": 0.8},
            {"score": 1.0, "confidence": 0.2},
        ]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_missing_endpoint_knots_rejected(self):
        doc = calibration_policy()
        doc["score_to_confidence"] = [
            {"score": 0.2, "confidence": 0.2},
            {"score": 0.8, "confidence": 0.8},
        ]
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathPowerPlanTests(unittest.TestCase):
    def test_unit_file_rejected(self):
        doc = benchmark_power_plan()
        doc = copy.deepcopy(doc)
        doc["families"][0]["unit"] = "file"
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_final_test_source_rejected(self):
        doc = benchmark_power_plan(power_source_role="final-test")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_final_test_in_support_rejected(self):
        doc = benchmark_power_plan()
        doc = copy.deepcopy(doc)
        doc["support"]["final-test"] = 10
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_n_required_must_be_max(self):
        doc = copy.deepcopy(benchmark_power_plan())
        doc["families"][0]["n_required"] = 1
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_alpha_plan_must_match_m(self):
        doc = benchmark_power_plan(alpha_plan=0.05)
        with self.assertRaises(SchemaError):
            validate(doc)


class FailClosedParseTests(unittest.TestCase):
    def test_nan_token_rejected_before_validate(self):
        text = '{"schema":"x","v":NaN}'
        with self.assertRaises(Exception):
            loads_strict(text)

    def test_unknown_schema_dispatch_rejected(self):
        with self.assertRaises(SchemaError):
            validate({"schema": "aieq-v3-feature-frame-1"})


class GoldenFixtureTests(unittest.TestCase):
    def test_example_raw_equals_canonical_bytes(self):
        """A6: instance-valid is not enough — example on-disk bytes must be
        canonical artifacts (raw == canonical_bytes(doc))."""
        if not FIXTURE_DIR.is_dir():
            self.skipTest("no example fixture dir")
        files = sorted(FIXTURE_DIR.glob("*.json"))
        self.assertGreaterEqual(len(files), 6)
        for path in files:
            with self.subTest(path=path.name):
                raw = path.read_bytes()
                doc = loads_strict(raw.decode("utf-8"))
                self.assertEqual(raw, canonical_bytes(doc))
                validate(doc)


class SchemaRegistrySurfaceTests(unittest.TestCase):
    def test_frozen_registry_matches_fixture(self):
        self.assertEqual(
            SCHEMA_REGISTRY_RELPATH,
            "ml_v3/fixtures/g1/schema_registry_v1.json",
        )
        self.assertTrue(SCHEMA_REGISTRY_FIXTURE.is_file())
        raw = SCHEMA_REGISTRY_FIXTURE.read_bytes()
        doc = loads_strict(raw.decode("utf-8"))
        self.assertEqual(raw, canonical_bytes(doc))
        self.assertEqual(doc, frozen_schema_registry())
        self.assertEqual(sha256_of_obj(doc), schema_registry_sha256())

    def test_mutating_asset_manifest_keys_changes_digest(self):
        """F3: expanding schema surface MUST change a tracked digest."""
        base = frozen_schema_registry()
        digest_before = sha256_of_obj(base)
        mutated = copy.deepcopy(base)
        keys = mutated["key_sets"]["ASSET_MANIFEST_KEYS"]
        self.assertIsInstance(keys, list)
        keys.append("evil_extra_field")
        # Also mutate required[] on the embedded schema dict (surface expand).
        required = mutated["schemas"][SCHEMA_IDS["asset_manifest"]]["required"]
        required.append("evil_extra_field")
        digest_after = sha256_of_obj(mutated)
        self.assertNotEqual(digest_before, digest_after)


def _write_goldens() -> None:
    """Helper for regenerating example + schema-registry fixtures."""
    FIXTURE_DIR.mkdir(parents=True, exist_ok=True)
    mapping = {
        "asset_manifest.json": asset_manifest(),
        "admission_batch.json": admission_batch(),
        "annotation.json": annotation_clean(),
        "prediction.json": prediction(),
        "calibration_policy.json": calibration_policy(),
        "benchmark_power_plan.json": benchmark_power_plan(),
    }
    for name, doc in mapping.items():
        write_canonical(FIXTURE_DIR / name, doc)
    write_canonical(SCHEMA_REGISTRY_FIXTURE, frozen_schema_registry())


if __name__ == "__main__":
    unittest.main()
```

---

## B.58 FILE: `ml_v3/tests/test_g1a_t3_adapter.py`

**Path logico:** `ml_v3/tests/test_g1a_t3_adapter.py`  
**Bytes:** 9165  
**Lines:** 225

```python
"""G1a T3 — frozen v2↔v3 adapter mapping (§10.5) happy + reject paths."""
from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from ml_v3.contracts.adapter import (
    ADAPTER_ARTIFACT_ID,
    HOMOLOGOUS_CLASSES,
    MASKED_NA_CLASSES,
    MIN_WINDOW_CENTERS,
    OCCUPANCY_THRESHOLD,
    WINDOW_STEP,
    AdapterError,
    adapter_mapping_bytes,
    adapter_mapping_sha256,
    frozen_adapter_mapping,
    is_masked_na_class,
    require_homologous_class,
    validate_adapter_mapping_claim,
)
from ml_v3.contracts.canonical import loads_strict, sha256_of_obj
from ml_v3.contracts.constants import CONTRACT_REVISION, PROBLEM_TYPES
from ml_v3.contracts.profiles import LEGACY_PROFILE_ALIASES, map_legacy_profile

FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "fixtures" / "g1" / "adapter_v2_v3_mapping.json"
)


class FrozenMappingHappyPathTests(unittest.TestCase):
    def test_six_homologous_classes_exact_order(self):
        self.assertEqual(
            list(HOMOLOGOUS_CLASSES),
            [
                "Resonance",
                "Muddiness",
                "Boominess",
                "Thinness",
                "BoxyMidrange",
                "DullSound",
            ],
        )
        self.assertEqual(len(HOMOLOGOUS_CLASSES), 6)

    def test_harsh_sib_masked_na(self):
        self.assertEqual(list(MASKED_NA_CLASSES), ["Harshness", "Sibilance"])
        for name in MASKED_NA_CLASSES:
            self.assertTrue(is_masked_na_class(name))
            self.assertNotIn(name, HOMOLOGOUS_CLASSES)

    def test_partition_covers_eight_problem_types(self):
        self.assertEqual(
            set(HOMOLOGOUS_CLASSES) | set(MASKED_NA_CLASSES),
            set(PROBLEM_TYPES),
        )
        self.assertFalse(set(HOMOLOGOUS_CLASSES) & set(MASKED_NA_CLASSES))

    def test_temporal_and_floor_constants(self):
        mapping = frozen_adapter_mapping()
        temporal = mapping["temporal_grid"]
        self.assertEqual(temporal["window_step"], WINDOW_STEP)
        self.assertEqual(WINDOW_STEP, 16)
        self.assertEqual(temporal["min_window_centers"], MIN_WINDOW_CENTERS)
        self.assertEqual(MIN_WINDOW_CENTERS, 20)
        self.assertEqual(temporal["occupancy_threshold"], OCCUPANCY_THRESHOLD)
        self.assertEqual(OCCUPANCY_THRESHOLD, 0.05)
        floors = mapping["per_class_support_floors"]
        self.assertEqual(floors["positive_groups"], 30)
        self.assertEqual(floors["negative_groups"], 30)
        self.assertEqual(
            floors["roles"], ["development-metric", "final-test"])
        self.assertEqual(
            floors["insufficient_support_consequence"],
            "insufficient support on any of the six homologous classes "
            "renders the gate NO-GO",
        )

    def test_na_semantics_includes_declared_invalid_converse(self):
        """§10.5: structural N/A + declared-but-invalid → fail-closed, never N/A."""
        prose = frozen_adapter_mapping()["na_semantics"]
        self.assertIn("structural absence is N/A", prose)
        self.assertIn(
            "a candidate that declares a surface but emits no valid "
            "prediction is fail-closed",
            prose,
        )
        self.assertIn("never N/A", prose)
        self.assertIn("FN, schema error or candidate failure per case", prose)

    def test_support_floor_consequence_is_nogo(self):
        floors = frozen_adapter_mapping()["per_class_support_floors"]
        self.assertIn("NO-GO", floors["insufficient_support_consequence"])
        self.assertIn("six homologous classes", floors["insufficient_support_consequence"])

    def test_legacy_techno_alias_reused_not_duplicated(self):
        mapping = frozen_adapter_mapping()
        self.assertEqual(
            mapping["legacy_profile_aliases"], LEGACY_PROFILE_ALIASES)
        self.assertEqual(
            map_legacy_profile("Techno")["source_profile"], "edm")

    def test_canonical_hash_stable_across_calls(self):
        a = adapter_mapping_sha256()
        b = adapter_mapping_sha256()
        self.assertEqual(a, b)
        self.assertEqual(len(a), 64)
        self.assertEqual(a, hashlib.sha256(adapter_mapping_bytes()).hexdigest())
        self.assertEqual(a, sha256_of_obj(frozen_adapter_mapping()))

    def test_validate_accepts_frozen_copy(self):
        claim = frozen_adapter_mapping()
        out = validate_adapter_mapping_claim(claim)
        self.assertEqual(out["artifact_id"], ADAPTER_ARTIFACT_ID)
        self.assertEqual(out["contract_revision"], CONTRACT_REVISION)

    def test_golden_fixture_matches_frozen_bytes(self):
        self.assertTrue(FIXTURE.is_file(), f"missing golden {FIXTURE}")
        on_disk = FIXTURE.read_bytes()
        self.assertEqual(on_disk, adapter_mapping_bytes())
        parsed = loads_strict(on_disk.decode("utf-8"))
        validate_adapter_mapping_claim(parsed)


class RejectPathTests(unittest.TestCase):
    def test_non_object_claim_rejected(self):
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(["not", "an", "object"])

    def test_wrong_window_step_rejected(self):
        claim = frozen_adapter_mapping()
        claim["temporal_grid"]["window_step"] = 32
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_wrong_occupancy_rejected(self):
        claim = frozen_adapter_mapping()
        claim["temporal_grid"]["occupancy_threshold"] = 0.10
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_wrong_min_centers_rejected(self):
        claim = frozen_adapter_mapping()
        claim["temporal_grid"]["min_window_centers"] = 10
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_promoting_harshness_into_homologous_rejected(self):
        claim = frozen_adapter_mapping()
        claim["homologous_classes"] = list(HOMOLOGOUS_CLASSES) + ["Harshness"]
        claim["macro_f1_denominator"] = 7
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_dropping_masked_na_rejected(self):
        claim = frozen_adapter_mapping()
        claim["masked_na_classes"] = ["Harshness"]  # dropped Sibilance
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_wrong_support_floors_rejected(self):
        claim = frozen_adapter_mapping()
        claim["per_class_support_floors"]["positive_groups"] = 20
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_stripping_support_nogo_consequence_rejected(self):
        claim = frozen_adapter_mapping()
        del claim["per_class_support_floors"]["insufficient_support_consequence"]
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_stripping_declared_invalid_na_converse_rejected(self):
        claim = frozen_adapter_mapping()
        # Truncate to the pre-T3.1 half (structural absence only).
        claim["na_semantics"] = (
            "N/A does not enter macro-averages, does not satisfy a gate, and "
            "does not demonstrate improvement; structural absence is N/A, "
            "never zero/infinity/FAIL"
        )
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_divergent_techno_alias_rejected(self):
        claim = frozen_adapter_mapping()
        claim["legacy_profile_aliases"] = {
            "Techno": {
                "source_profile": "master",
                "source_profile_id": 5,
                "electronic_subgenre": "techno",
            },
        }
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_extra_key_breaks_byte_identity(self):
        claim = frozen_adapter_mapping()
        claim["invented_threshold"] = 0.99
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_require_homologous_rejects_masked_and_unknown(self):
        with self.assertRaises(AdapterError):
            require_homologous_class("Harshness")
        with self.assertRaises(AdapterError):
            require_homologous_class("Sibilance")
        with self.assertRaises(AdapterError):
            require_homologous_class("NotAClass")
        self.assertEqual(require_homologous_class("Resonance"), "Resonance")

    def test_non_canonical_json_roundtrip_still_hashes_via_canonical(self):
        """Unsorted keys in text must not invent a second digest after loads."""
        mapping = frozen_adapter_mapping()
        messy = json.dumps(mapping, sort_keys=False, separators=(", ", ": "))
        reparsed = loads_strict(messy if messy.endswith("\n") else messy + "\n")
        # loads_strict accepts the object; validation requires frozen identity.
        validate_adapter_mapping_claim(reparsed)
        self.assertEqual(sha256_of_obj(reparsed), adapter_mapping_sha256())


if __name__ == "__main__":
    unittest.main()
```

---

## B.59 FILE: `ml_v3/tests/test_g1a_t4_metrology_lock.py`

**Path logico:** `ml_v3/tests/test_g1a_t4_metrology_lock.py`  
**Bytes:** 24541  
**Lines:** 535

```python
"""G1a T4 — frozen metrology lock (§13.1/§13.2) happy + reject paths."""
from __future__ import annotations

import hashlib
import json
import math
import unittest
from pathlib import Path

from ml_v3.contracts.adapter import adapter_mapping_sha256
from ml_v3.contracts.canonical import loads_strict, sha256_of_obj
from ml_v3.contracts.constants import CONTRACT_REVISION, GRID_BANDS
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.contracts.metrology_lock import (
    FIXED_CHUNK_SCHEDULES,
    GATE_PLATFORM_FLOAT_TOL,
    GEOMETRIC_CHUNK_SCHEDULE,
    HOP_SAMPLES,
    KAISER_BETA,
    K_CODA,
    K_WU,
    METROLOGY_ARTIFACT_ID,
    N_LF,
    RESAMPLER_PASS_HZ,
    SECONDARY_FLOAT_ABS_TOL,
    SR_PARITY_MAX_ABS_DB,
    STREAMING_ALSO_REQUIRED_PROOFS,
    STREAMING_FLOAT32_FRAME_FIELDS,
    STREAMING_PRNG_SEED,
    STREAMING_RATIONAL_TIMESTAMP_FIELDS,
    STREAMING_VALIDITY_FIELDS,
    SWEEP_CHECKPOINT_HZ,
    MetrologyLockError,
    coda_seconds,
    frozen_metrology_lock,
    metrology_lock_bytes,
    metrology_lock_sha256,
    resampler_group_delay_rational,
    validate_metrology_lock_claim,
    warm_up_seconds,
)

FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "fixtures" / "g1" / "metrology_lock.json"
)

EXPECTED_ADAPTER_SHA256 = (
    "6a978c01bcccb85fb7db17ae3c66ee55ebceee82f47dca792e5a2f3a5fb9828f"
)


class FrozenLockHappyPathTests(unittest.TestCase):
    def test_timing_constants_and_additive_warmup(self):
        lock = frozen_metrology_lock()
        timing = lock["timing"]
        self.assertEqual(timing["H"], HOP_SAMPLES)
        self.assertEqual(HOP_SAMPLES, 1024)
        self.assertEqual(timing["N_LF"], N_LF)
        self.assertEqual(N_LF, 8192)
        self.assertEqual(timing["K_wu"], K_WU)
        self.assertEqual(timing["K_coda"], K_CODA)
        self.assertEqual(K_WU, 4)
        self.assertEqual(K_CODA, 4)
        self.assertEqual(timing["warm_up_composition"], "additive")
        self.assertEqual(timing["warm_up_composition_forbidden"], "max")
        # Additive: delay + 32/125, not max(delay, N_LF/fs_c, ...).
        self.assertAlmostEqual(warm_up_seconds(48000), 0.256, places=12)
        self.assertGreater(warm_up_seconds(44100), warm_up_seconds(48000))
        self.assertGreater(warm_up_seconds(96000), warm_up_seconds(48000))
        self.assertAlmostEqual(coda_seconds(), 32 / 375, places=12)

    def test_resampler_delay_rationals_per_gate_sr(self):
        n, d, taps = resampler_group_delay_rational(48000)
        self.assertEqual((n, d, taps), (0, 1, None))
        n, d, taps = resampler_group_delay_rational(96000)
        self.assertEqual((n, d, taps), (1, 750, 257))
        n, d, taps = resampler_group_delay_rational(44100)
        self.assertEqual((n, d, taps), (16, 11025, 20481))

    def test_stationary_mode_a_closes_gate_mode_b_diagnostic(self):
        st = frozen_metrology_lock()["stationary_portion"]
        self.assertEqual(st["gate_closing_mode"], "a")
        self.assertTrue(st["mode_a"]["closes_sample_rate_parity_gate"])
        self.assertFalse(st["mode_b"]["closes_sample_rate_parity_gate"])
        self.assertTrue(st["mode_b"]["diagnostic_only"])
        self.assertEqual(st["mode_b"]["preregistered_windows"], [])
        self.assertEqual(st["mode_b"]["T_min_hops"], 8)
        self.assertEqual(st["mode_b"]["stability_variance_max_db2"], 1.0)
        self.assertEqual(st["mode_b"]["stability_max_abs_hop_delta_db"], 0.5)

    def test_activity_union_and_sr_threshold(self):
        sr = frozen_metrology_lock()["sample_rate_parity"]
        self.assertEqual(sr["threshold_max_abs_db"], SR_PARITY_MAX_ABS_DB)
        self.assertEqual(SR_PARITY_MAX_ABS_DB, 0.25)
        self.assertEqual(sr["aggregator"], "max")
        self.assertTrue(sr["activity"]["union_cross_sr"])
        self.assertEqual(
            sr["activity"]["predicate"],
            "max(psd_db_ref, psd_db_sr) > -120",
        )
        self.assertTrue(sr["activity"]["empty_active_cell_set_is_fail"])
        self.assertTrue(sr["activity"]["empty_useful_segment_is_fail"])
        self.assertTrue(sr["activity"]["vacuous_pass_forbidden"])
        self.assertEqual(sr["activity"]["max_over_empty_active_set"], "FAIL")
        self.assertTrue(sr["activity"]["na_is_not_pass"])
        self.assertIn("mid_delta_db", sr["excluded_from_domain"])
        self.assertIn("side_delta_db", sr["excluded_from_domain"])

    def test_sr_alignment_nearest_source_time_anti_cherrypick(self):
        sr = frozen_metrology_lock()["sample_rate_parity"]
        alignment = sr["alignment"]
        self.assertEqual(alignment["select_by"], "nearest_source_time")
        self.assertIn("output_index", alignment["forbidden_select_by"])
        self.assertTrue(alignment["manual_frame_shift_forbidden"])
        self.assertTrue(
            alignment[
                "choose_within_pm1_radius_to_minimize_abs_delta_forbidden"])
        self.assertEqual(
            alignment["tie_break"],
            ["smaller_frame_index", "smaller_frame_end_sample"],
        )
        sweep = frozen_metrology_lock()["sweep_log_parity"]
        self.assertEqual(
            sweep["frame_selection"],
            "nearest_source_time_within_match_radius",
        )
        self.assertEqual(
            sweep["alignment_policy_ref"], "sample_rate_parity.alignment")

    def test_empty_to_fail_derivation_does_not_supersede_contract(self):
        derivation = frozen_metrology_lock()["sample_rate_parity"]["activity"][
            "empty_to_fail_derivation"]
        self.assertEqual(derivation["kind"], "derived_packaging")
        self.assertTrue(derivation["does_not_supersede_contract"])
        self.assertTrue(derivation["candidate_for_future_contract_amendment"])
        self.assertGreaterEqual(len(derivation["chain"]), 3)
        joined = " ".join(derivation["chain"])
        self.assertIn("§10.5", joined)
        self.assertIn("N/A", joined)
        self.assertIn("§13.2 gate 4", joined)
        self.assertIn("lock packaging authority", derivation["packaging_note"])
        self.assertIn("REV7", derivation["packaging_note"])

    def test_bit_identity_gate_platform_empty_secondary_allowlist(self):
        bit_id = frozen_metrology_lock()["bit_identity"]
        platform = bit_id["gate_platform"]
        self.assertEqual(platform["os"], "darwin")
        self.assertEqual(platform["arch"], "arm64")
        self.assertEqual(platform["numpy"], "2.5.1")
        self.assertEqual(bit_id["gate_platform_float_tol"], 0)
        self.assertEqual(bit_id["gate_platform_float_tol"], GATE_PLATFORM_FLOAT_TOL)
        self.assertEqual(
            bit_id["gate_platform_float_identity"], "byte_identical_only")
        self.assertTrue(bit_id["gate_platform_secondary_tol_forbidden"])
        self.assertEqual(bit_id["secondary_platforms_allowlist"], [])
        self.assertEqual(bit_id["secondary_float_abs_tol"], 1e-6)
        self.assertEqual(
            bit_id["secondary_float_abs_tol"], SECONDARY_FLOAT_ABS_TOL)
        self.assertTrue(bit_id["secondary_cannot_close_g1_gate"])
        self.assertEqual(
            bit_id["ad_hoc_non_bit_identical_without_allowlist"], "FAIL")
        self.assertNotEqual(
            bit_id["gate_platform_float_tol"],
            bit_id["secondary_float_abs_tol"],
        )

    def test_streaming_schedules_and_seed(self):
        stream = frozen_metrology_lock()["streaming_equivalence"]
        self.assertEqual(stream["prng_seed"], STREAMING_PRNG_SEED)
        self.assertEqual(STREAMING_PRNG_SEED, 20260719)
        self.assertEqual(
            stream["fixed_chunk_schedules_host_samples"],
            list(FIXED_CHUNK_SCHEDULES),
        )
        geo = stream["geometric_schedule"]
        self.assertEqual(geo["chunks_host_samples"], list(GEOMETRIC_CHUNK_SCHEDULE))
        self.assertEqual(len(geo["chunks_host_samples"]), 32)
        surface = stream["required_proof_surface"]
        self.assertEqual(
            surface["float32_frame_fields"],
            list(STREAMING_FLOAT32_FRAME_FIELDS),
        )
        self.assertEqual(
            surface["rational_timestamp_fields"],
            list(STREAMING_RATIONAL_TIMESTAMP_FIELDS),
        )
        self.assertEqual(
            surface["validity_fields"], list(STREAMING_VALIDITY_FIELDS))
        self.assertTrue(surface["validity_reason_enumerated_when_false"])
        self.assertEqual(
            surface["also_required"],
            [dict(p) for p in STREAMING_ALSO_REQUIRED_PROOFS],
        )
        self.assertIn("every frozen schedule", surface["also_required_quantifier"])
        self.assertTrue(surface["also_required_omission_is_fail"])
        self.assertIn("(a)(b)(c)", surface["also_required_applies_to"])
        self.assertNotIn("also_required", stream)

    def test_resampler_generator_serialize_only_section_5(self):
        gen = frozen_metrology_lock()["resampler_generator"]
        self.assertTrue(gen["serialize_only"])
        self.assertTrue(gen["no_coefficient_implementation_in_g1a"])
        self.assertEqual(gen["kaiser_beta"], KAISER_BETA)
        self.assertEqual(KAISER_BETA, 9.0)
        self.assertEqual(gen["pass_hz"], RESAMPLER_PASS_HZ)
        self.assertEqual(gen["stop_hz_formula"], "min(fs_in, 48000) / 2")
        self.assertEqual(gen["cutoff"], "midpoint_of_pass_and_stop")
        self.assertEqual(gen["coefficient_gain_scale"], "up")
        self.assertEqual(gen["structure"], "causal_polyphase")
        self.assertTrue(gen["streaming_state_preserved_across_blocks"])
        self.assertEqual(gen["padding"], "none")
        self.assertIs(gen["reflection"], False)
        self.assertTrue(gen["look_ahead_forbidden"])

    def test_t5_hash_coverage_declaration(self):
        coverage = frozen_metrology_lock()["hash_coverage"]
        self.assertTrue(
            coverage[
                "t5_sha256sums_covers_artifact_hashes_beyond_dependencies"])
        self.assertEqual(
            coverage["beyond_dependencies_covered_by"], "T5_SHA256SUMS")
        self.assertTrue(coverage["inline_dependencies_bound_here"])
        self.assertIn("not left implicit", coverage["declaration"])
        self.assertIn("T4.2", coverage["t4_2_stop_rule"])
        self.assertIn("last hardening", coverage["t4_2_stop_rule"])
        self.assertIn("CRITICAL", coverage["t4_2_stop_rule"])

    def test_geometric_schedule_matches_numpy_pcg64_when_available(self):
        try:
            import numpy as np
        except ImportError:
            self.skipTest("numpy not available")
        rng = np.random.Generator(np.random.PCG64(STREAMING_PRNG_SEED))
        u = rng.uniform(0.0, 14.0, size=32)
        chunks = [math.floor(float(2.0 ** value)) for value in u]
        self.assertEqual(chunks, list(GEOMETRIC_CHUNK_SCHEDULE))

    def test_sweep_checkpoints_include_critical_and_extremes(self):
        checkpoints = frozen_metrology_lock()["sweep_log_parity"]["checkpoint_hz"]
        self.assertEqual(checkpoints, list(SWEEP_CHECKPOINT_HZ))
        for hz in (20, 45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000):
            self.assertIn(hz, checkpoints)
        self.assertEqual(checkpoints, sorted(checkpoints))

    def test_dependencies_bind_contract_and_t3_adapter(self):
        deps = frozen_metrology_lock()["dependencies"]
        self.assertEqual(deps["contract_revision"], CONTRACT_REVISION)
        self.assertIn("6d254d0a", deps["contract_revision"])
        self.assertEqual(deps["adapter_mapping_sha256"], adapter_mapping_sha256())
        self.assertEqual(deps["adapter_mapping_sha256"], EXPECTED_ADAPTER_SHA256)
        self.assertEqual(deps["grid_bands"], GRID_BANDS)
        self.assertEqual(
            deps["grid_centers_sha256"], sha256_of_obj(band_centers_hz()))

    def test_canonical_hash_stable_across_calls(self):
        a = metrology_lock_sha256()
        b = metrology_lock_sha256()
        self.assertEqual(a, b)
        self.assertEqual(len(a), 64)
        self.assertEqual(a, hashlib.sha256(metrology_lock_bytes()).hexdigest())
        self.assertEqual(a, sha256_of_obj(frozen_metrology_lock()))

    def test_validate_accepts_frozen_copy(self):
        claim = frozen_metrology_lock()
        out = validate_metrology_lock_claim(claim)
        self.assertEqual(out["artifact_id"], METROLOGY_ARTIFACT_ID)
        self.assertEqual(out["contract_revision"], CONTRACT_REVISION)

    def test_golden_fixture_matches_frozen_bytes(self):
        self.assertTrue(FIXTURE.is_file(), f"missing golden {FIXTURE}")
        on_disk = FIXTURE.read_bytes()
        self.assertEqual(on_disk, metrology_lock_bytes())
        parsed = loads_strict(on_disk.decode("utf-8"))
        validate_metrology_lock_claim(parsed)
        self.assertEqual(
            parsed["dependencies"]["adapter_mapping_sha256"],
            EXPECTED_ADAPTER_SHA256,
        )

    def test_envelope_authority_declares_freeze_from_prose(self):
        prose = frozen_metrology_lock()["envelope_authority"]
        self.assertIn("freeze-from-prose", prose)
        self.assertIn("no literal JSON key table", prose)


class RejectPathTests(unittest.TestCase):
    def test_non_object_claim_rejected(self):
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(["not", "an", "object"])

    def test_max_warmup_composition_rejected(self):
        claim = frozen_metrology_lock()
        claim["timing"]["warm_up_composition"] = "max"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_wrong_k_wu_rejected(self):
        claim = frozen_metrology_lock()
        claim["timing"]["K_wu"] = 2
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_activity_without_union_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"]["union_cross_sr"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_empty_active_cell_set_fail_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["sample_rate_parity"]["activity"]["empty_active_cell_set_is_fail"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("empty_active_cell_set_is_fail", str(ctx.exception))

    def test_empty_active_cell_set_fail_false_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"][
            "empty_active_cell_set_is_fail"] = False
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("empty_active_cell_set_is_fail", str(ctx.exception))

    def test_empty_useful_segment_fail_false_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"][
            "empty_useful_segment_is_fail"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_vacuous_pass_allowed_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"]["vacuous_pass_forbidden"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_max_over_empty_active_set_zero_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"][
            "max_over_empty_active_set"] = 0
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("max_over_empty_active_set", str(ctx.exception))

    def test_gate_platform_float_tol_nonzero_rejected(self):
        claim = frozen_metrology_lock()
        claim["bit_identity"]["gate_platform_float_tol"] = 1e-6
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("gate_platform_float_tol", str(ctx.exception))

    def test_gate_platform_secondary_tol_allowed_rejected(self):
        claim = frozen_metrology_lock()
        claim["bit_identity"]["gate_platform_secondary_tol_forbidden"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_streaming_proof_surface_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["streaming_equivalence"]["required_proof_surface"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("required_proof_surface", str(ctx.exception))

    def test_streaming_float32_fields_incomplete_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["required_proof_surface"][
            "float32_frame_fields"] = ["mid_psd_db[120]"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("float32_frame_fields", str(ctx.exception))

    def test_streaming_also_required_string_list_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["required_proof_surface"][
            "also_required"] = [
            "multi_asset_concat_with_explicit_delta_history_reset",
            "interleaved_silence_between_assets",
            "streaming_vs_offline_identity_same_lock_and_platform",
        ]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("also_required", str(ctx.exception))

    def test_streaming_also_required_omission_fail_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["streaming_equivalence"]["required_proof_surface"][
            "also_required_omission_is_fail"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("also_required_omission_is_fail", str(ctx.exception))

    def test_streaming_also_required_quantifier_weakened_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["required_proof_surface"][
            "also_required_quantifier"] = "for some schedule only"
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("also_required_quantifier", str(ctx.exception))

    def test_sr_alignment_output_index_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["alignment"]["select_by"] = "output_index"
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("nearest_source_time", str(ctx.exception))

    def test_sr_alignment_pm1_cherrypick_allowed_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["alignment"][
            "choose_within_pm1_radius_to_minimize_abs_delta_forbidden"] = False
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("pm1_radius", str(ctx.exception))

    def test_sr_alignment_tie_break_stripped_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["alignment"]["tie_break"] = [
            "smaller_abs_delta"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("tie_break", str(ctx.exception))

    def test_empty_to_fail_derivation_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["sample_rate_parity"]["activity"]["empty_to_fail_derivation"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("empty_to_fail_derivation", str(ctx.exception))

    def test_empty_to_fail_claims_supersede_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"]["empty_to_fail_derivation"][
            "does_not_supersede_contract"] = False
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("does_not_supersede_contract", str(ctx.exception))

    def test_resampler_generator_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["resampler_generator"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("resampler_generator", str(ctx.exception))

    def test_resampler_generator_kaiser_beta_wrong_rejected(self):
        claim = frozen_metrology_lock()
        claim["resampler_generator"]["kaiser_beta"] = 8.0
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("kaiser_beta", str(ctx.exception))

    def test_t5_hash_coverage_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["hash_coverage"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("hash_coverage", str(ctx.exception))

    def test_t5_hash_coverage_false_rejected(self):
        claim = frozen_metrology_lock()
        claim["hash_coverage"][
            "t5_sha256sums_covers_artifact_hashes_beyond_dependencies"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_mode_b_closing_gate_rejected(self):
        claim = frozen_metrology_lock()
        claim["stationary_portion"]["mode_b"]["closes_sample_rate_parity_gate"] = True
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_gate_closing_mode_b_rejected(self):
        claim = frozen_metrology_lock()
        claim["stationary_portion"]["gate_closing_mode"] = "b"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_relaxed_sr_threshold_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["threshold_max_abs_db"] = 0.5
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_mean_aggregator_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["aggregator"] = "mean"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_wrong_adapter_dependency_rejected(self):
        claim = frozen_metrology_lock()
        claim["dependencies"]["adapter_mapping_sha256"] = "ff" * 32
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_wrong_contract_revision_dependency_rejected(self):
        claim = frozen_metrology_lock()
        claim["dependencies"]["contract_revision"] = "tampered"
        claim["contract_revision"] = "tampered"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_secondary_allowlist_ad_hoc_rejected(self):
        claim = frozen_metrology_lock()
        claim["bit_identity"]["secondary_platforms_allowlist"] = [
            {"os": "linux", "arch": "x86_64"},
        ]
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_mutated_geometric_schedule_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["geometric_schedule"][
            "chunks_host_samples"][0] = 999
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_extra_key_breaks_byte_identity(self):
        claim = frozen_metrology_lock()
        claim["invented_slack"] = 0.99
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_non_canonical_json_roundtrip_still_hashes_via_canonical(self):
        lock = frozen_metrology_lock()
        messy = json.dumps(lock, sort_keys=False, separators=(", ", ": "))
        reparsed = loads_strict(messy if messy.endswith("\n") else messy + "\n")
        validate_metrology_lock_claim(reparsed)
        self.assertEqual(sha256_of_obj(reparsed), metrology_lock_sha256())


if __name__ == "__main__":
    unittest.main()
```

---

## B.60 FILE: `ml_v3/tests/test_g1a_t5_sha256sums.py`

**Path logico:** `ml_v3/tests/test_g1a_t5_sha256sums.py`  
**Bytes:** 8773  
**Lines:** 224

```python
"""G1a T5 — SHA256SUMS inventory, path validation, round-trip, reject paths."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ml_v3.contracts.canonical import (
    CanonicalError,
    is_sha256_hex,
    parse_sha256sums,
    sha256_of_file,
    sha256sums_text,
    validate_sha256sums_relpath,
)
from ml_v3.contracts.sha256sums import (
    CONTRACT_DOC_SHA256_TRIPWIRE,
    G1A_SHA256SUMS_COVERED,
    G1A_SHA256SUMS_RELPATH,
    Sha256SumsError,
    build_sha256sums_entries,
    g1a_sha256sums_audio_required,
    load_g1a_sha256sums,
    render_g1a_sha256sums,
    repo_root_from_here,
    verify_g1a_sha256sums,
    verify_sha256sums_against_tree,
)

_HEX0 = "0" * 64
_HEX1 = "1" * 64


class PathValidationRejectTests(unittest.TestCase):
    """One reject-path assertion per forbidden category."""

    def test_reject_nul(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\0b")

    def test_reject_newline(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\nb")

    def test_reject_cr(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\rb")

    def test_reject_control_char_tab(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\tb")

    def test_reject_leading_slash(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("/etc/passwd")

    def test_reject_dotdot_segment(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("foo/../bar")

    def test_reject_backslash(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("foo\\bar")

    def test_reject_leading_space(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath(" spaced")

    def test_reject_trailing_space(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("spaced ")

    def test_reject_empty(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("")

    def test_accept_relative_posix(self):
        self.assertEqual(
            validate_sha256sums_relpath("docs/MOTORE_V3_G1_CONTRACT.md"),
            "docs/MOTORE_V3_G1_CONTRACT.md",
        )


class Sha256sumsRoundTripTests(unittest.TestCase):
    def test_parse_render_round_trip(self):
        entries = {
            "b/file.json": _HEX1,
            "a/file.json": _HEX0,
        }
        text = sha256sums_text(entries)
        self.assertEqual(parse_sha256sums(text), entries)
        # Lexicographic path order in the rendered body.
        self.assertTrue(text.startswith(f"{_HEX0}  a/file.json\n"))

    def test_render_rejects_bad_path_and_digest(self):
        with self.assertRaises(CanonicalError):
            sha256sums_text({"/abs": _HEX0})
        with self.assertRaises(CanonicalError):
            sha256sums_text({"ok": "deadbeef"})
        with self.assertRaises(CanonicalError):
            sha256sums_text({})

    def test_parse_rejects_blank_comment_binary_duplicate(self):
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0}  a\n\n{_HEX1}  b\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"# comment\n{_HEX0}  a\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0} *a\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0}  a\n{_HEX1}  a\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0} a\n")  # single space


class G1aSha256sumsFixtureTests(unittest.TestCase):
    def test_self_hash_policy_documented_and_enforced(self):
        self.assertEqual(
            G1A_SHA256SUMS_RELPATH, "ml_v3/fixtures/g1/SHA256SUMS")
        self.assertNotIn(G1A_SHA256SUMS_RELPATH, G1A_SHA256SUMS_COVERED)
        root = repo_root_from_here()
        with self.assertRaises(Sha256SumsError):
            build_sha256sums_entries(
                root, (G1A_SHA256SUMS_RELPATH,), forbid_self=G1A_SHA256SUMS_RELPATH)

    def test_minimum_coverage_and_contract_tripwire(self):
        self.assertEqual(len(G1A_SHA256SUMS_COVERED), 11)
        self.assertIn(
            "ml_v3/fixtures/g1/fixture_spec_v1.json",
            G1A_SHA256SUMS_COVERED,
        )
        example_paths = [
            path for path in G1A_SHA256SUMS_COVERED if "/examples/" in path]
        self.assertEqual(len(example_paths), 6)
        self.assertIn(
            "ml_v3/fixtures/g1/schema_registry_v1.json",
            G1A_SHA256SUMS_COVERED,
        )
        # Normative schema surface is schema_registry_v1 — not examples/.
        schema_mislabel = [
            path for path in G1A_SHA256SUMS_COVERED if "/schemas/" in path]
        self.assertEqual(schema_mislabel, [])
        self.assertIn("docs/MOTORE_V3_G1_CONTRACT.md", G1A_SHA256SUMS_COVERED)
        self.assertIn(
            "ml_v3/fixtures/g1/adapter_v2_v3_mapping.json",
            G1A_SHA256SUMS_COVERED,
        )
        self.assertIn(
            "ml_v3/fixtures/g1/metrology_lock.json",
            G1A_SHA256SUMS_COVERED,
        )
        self.assertTrue(is_sha256_hex(CONTRACT_DOC_SHA256_TRIPWIRE))
        self.assertEqual(
            CONTRACT_DOC_SHA256_TRIPWIRE,
            "6a6f6d35bbf3fc65d7a01e54620bf4f9649ea77d60c2b3d9e7ea0b72f7f49a86",
        )

    def test_committed_sha256sums_verifies_against_tree(self):
        entries = verify_g1a_sha256sums()
        # COVERED + T6 audio required; full happy path is 11 + 37 = 48.
        self.assertTrue(set(G1A_SHA256SUMS_COVERED).issubset(entries))
        audio = g1a_sha256sums_audio_required()
        self.assertEqual(len(audio), 37)
        self.assertTrue(set(audio).issubset(entries))
        self.assertEqual(len(entries), 48)
        root = repo_root_from_here()
        contract = root / "docs" / "MOTORE_V3_G1_CONTRACT.md"
        self.assertEqual(sha256_of_file(contract), CONTRACT_DOC_SHA256_TRIPWIRE)
        self.assertEqual(
            entries["docs/MOTORE_V3_G1_CONTRACT.md"],
            CONTRACT_DOC_SHA256_TRIPWIRE,
        )
        # Committed file exists beside goldens and matches renderer.
        committed = root / G1A_SHA256SUMS_RELPATH
        self.assertTrue(committed.is_file())
        self.assertEqual(committed.read_text(encoding="utf-8"), render_g1a_sha256sums())
        self.assertEqual(load_g1a_sha256sums(), entries)
        self.assertEqual(committed.read_text(encoding="utf-8"), sha256sums_text(entries))

    def test_verify_rejects_truncated_sums_missing_audio(self):
        """COVERED-only inventory must not PASS verify (audio unbound)."""
        root = repo_root_from_here()
        full = load_g1a_sha256sums()
        truncated = {path: full[path] for path in G1A_SHA256SUMS_COVERED}
        self.assertEqual(len(truncated), 11)
        with tempfile.TemporaryDirectory() as tmp:
            troot = Path(tmp)
            for rel, digest in truncated.items():
                absolute = troot / rel
                absolute.parent.mkdir(parents=True, exist_ok=True)
                src = root / rel
                absolute.write_bytes(src.read_bytes())
                self.assertEqual(sha256_of_file(absolute), digest)
            sums_path = troot / G1A_SHA256SUMS_RELPATH
            sums_path.parent.mkdir(parents=True, exist_ok=True)
            sums_path.write_text(sha256sums_text(truncated), encoding="utf-8")
            with self.assertRaises(Sha256SumsError) as ctx:
                verify_g1a_sha256sums(troot)
            self.assertIn("missing required audio inventory", str(ctx.exception))

    def test_verify_detects_tampered_digest(self):
        root = repo_root_from_here()
        entries = dict(load_g1a_sha256sums())
        victim = next(iter(entries))
        entries[victim] = _HEX0 if entries[victim] != _HEX0 else _HEX1
        with self.assertRaises(Sha256SumsError):
            verify_sha256sums_against_tree(entries, root)

    def test_build_entries_hashes_temp_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            rel = "pkg/a.bin"
            absolute = root / rel
            absolute.parent.mkdir(parents=True)
            absolute.write_bytes(b"g1a-t5")
            entries = build_sha256sums_entries(
                root, (rel,), forbid_self="pkg/SHA256SUMS")
            self.assertEqual(entries[rel], sha256_of_file(absolute))
            verify_sha256sums_against_tree(
                entries, root, forbid_self="pkg/SHA256SUMS")


if __name__ == "__main__":
    unittest.main()
```

---

## B.61 FILE: `ml_v3/tests/test_g1a_t6_fixture_spec.py`

**Path logico:** `ml_v3/tests/test_g1a_t6_fixture_spec.py`  
**Bytes:** 16632  
**Lines:** 393

```python
"""G1a M2 / T6-prep — frozen fixture-spec v1 happy + reject paths.

Does not generate audio. Spec commit is gated separately (STOP for CC).
"""
from __future__ import annotations

import hashlib
import math
import unittest
from fractions import Fraction
from pathlib import Path

from ml_v3.contracts.canonical import loads_strict, sha256_of_obj, write_canonical
from ml_v3.contracts.constants import (
    CANONICAL_SAMPLE_RATE,
    CONTRACT_REVISION,
    GATE_SAMPLE_RATES,
)
from ml_v3.contracts.fixture_spec import (
    DURATION_DEN,
    DURATION_NUM,
    FIXTURE_SPEC_ARTIFACT_ID,
    MULTITONE_HZ,
    PSEUDO_NOISE_PRNG_SEED,
    SWEEP_T_END_DEN,
    SWEEP_T_END_NUM,
    SWEEP_T_START_DEN,
    SWEEP_T_START_NUM,
    ULTRASONIC_HZ,
    FixtureSpecError,
    _useful_frame_times,
    assert_sweep_checkpoints_reachable,
    common_useful_window,
    duration_seconds,
    fixture_spec_bytes,
    fixture_spec_sha256,
    frozen_fixture_spec,
    nearest_useful_frame_distance,
    sample_count,
    sweep_active_end_seconds,
    sweep_active_start_seconds,
    sweep_crossing_time,
    validate_fixture_spec_claim,
)
from ml_v3.contracts.metrology_lock import (
    HOP_SAMPLES,
    N_LF,
    SWEEP_CHECKPOINT_HZ,
    coda_seconds,
    metrology_lock_sha256,
    resampler_group_delay_rational,
    warm_up_seconds,
)

FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "fixtures" / "g1" / "fixture_spec_v1.json"
)


class FrozenFixtureSpecHappyPathTests(unittest.TestCase):
    def test_duration_preregistered_not_false_formula(self):
        spec = frozen_fixture_spec()
        duration = spec["global_duration"]
        self.assertEqual(duration["kind"], "preregistered_parameter")
        self.assertTrue(duration["not_a_contract_formula"])
        self.assertEqual(
            duration["false_formula_forbidden"], "warm_up + coda + T_min")
        self.assertEqual(duration["duration_num"], DURATION_NUM)
        self.assertEqual(duration["duration_den"], DURATION_DEN)
        self.assertEqual(duration["duration_s"], 2.0)
        self.assertEqual(duration_seconds(), 2.0)
        bound = duration["bound_check"]
        max_bound = (
            bound["max_warm_up_plus_coda_num"]
            / bound["max_warm_up_plus_coda_den"]
        )
        self.assertGreater(duration_seconds(), max_bound)
        # Useful portion long enough for mode (a); not defined by T_min.
        self.assertGreater(bound["useful_portion_hops_at_fs_c"], 32.0)
        self.assertTrue(bound["mode_a_gate_closing"])
        self.assertIn("T_min", bound["rationale"])
        self.assertIn("mode-(b)", bound["rationale"])

    def test_sample_counts_exact_at_gate_rates(self):
        for fs in GATE_SAMPLE_RATES:
            self.assertEqual(sample_count(fs), 2 * fs)
        n_map = frozen_fixture_spec()["global_duration"]["N_at_gate_rates"]
        self.assertEqual(n_map["44100"], 88200)
        self.assertEqual(n_map["48000"], 96000)
        self.assertEqual(n_map["96000"], 192000)

    def test_duration_exceeds_per_rate_wu_plus_coda(self):
        coda = 32 / 375
        for fs in GATE_SAMPLE_RATES:
            self.assertGreater(
                duration_seconds(), warm_up_seconds(fs) + coda)

    def test_numeric_generation_pins(self):
        num = frozen_fixture_spec()["numeric_generation"]
        self.assertEqual(num["compute_dtype"], "float64")
        self.assertEqual(num["artifact_dtype"], "float32")
        self.assertEqual(num["post_render_normalization"], "forbidden")
        self.assertEqual(num["cross_rate_resample"], "forbidden")
        self.assertIn("DIRECTLY", num["render_policy"])
        self.assertEqual(num["file_format"]["container"], "wav")
        self.assertEqual(num["file_format"]["encoding"], "pcm_float32_le")

    def test_pseudo_noise_fully_pinned(self):
        noise = frozen_fixture_spec()["categories"]["pseudo_noise"]
        self.assertEqual(noise["prng_seed"], PSEUDO_NOISE_PRNG_SEED)
        self.assertEqual(PSEUDO_NOISE_PRNG_SEED, 31051986)
        self.assertEqual(noise["n_partials"], 512)
        self.assertEqual(noise["k_range"], [0, 511])
        self.assertIn("PCG64(31051986)", noise["phase_rule"])
        self.assertIn("2*pi*rng.random()", noise["phase_rule"])
        self.assertIn("k=0..511", noise["phase_rule"])
        expected_amp = (10.0 ** (-24.0 / 20.0)) * math.sqrt(2.0 / 512.0)
        self.assertAlmostEqual(noise["amplitude_peak_each"], expected_amp, places=15)
        self.assertIn("no renormalize", noise["waveform"])

    def test_categories_cover_section_13(self):
        cats = frozen_fixture_spec()["categories"]
        self.assertEqual(
            cats["multitone"]["frequencies_hz"], list(MULTITONE_HZ))
        self.assertEqual(
            cats["ultrasonic_96k"]["frequencies_hz"], list(ULTRASONIC_HZ))
        self.assertEqual(
            cats["ultrasonic_96k"]["presentation"], "simultaneous")
        self.assertEqual(
            cats["ultrasonic_96k"]["gate_sample_rates"], [96000])
        sweep = cats["log_sweep"]
        self.assertEqual(sweep["sweep_law"], "exponential_log_chirp")
        active = sweep["active_interval"]
        self.assertEqual(active["t_start_num"], SWEEP_T_START_NUM)
        self.assertEqual(active["t_start_den"], SWEEP_T_START_DEN)
        self.assertEqual(active["t_end_num"], SWEEP_T_END_NUM)
        self.assertEqual(active["t_end_den"], SWEEP_T_END_DEN)
        self.assertEqual(active["t_start_s"], 0.5)
        self.assertEqual(active["t_end_s"], 1.75)
        self.assertEqual(active["outside_active_sample"], 0.0)
        self.assertIn("T_active", sweep["instantaneous_freq_hz"])
        self.assertNotIn(
            "**(t/T), T=duration_s",
            sweep["instantaneous_freq_hz"].replace(" ", ""),
        )
        self.assertIn("peak = −24 dBFS", sweep["amplitude_formula"])
        layouts = cats["decorrelated_stereo"]["layouts"]
        self.assertIn("mid_only", layouts)
        self.assertIn("side_only", layouts)
        self.assertIn("decorrelated", layouts)
        self.assertEqual(
            layouts["decorrelated"]["right_prng_seed"], 31051987)
        patterns = cats["silence_non_finite"]["non_finite_patterns"]
        injects = {p["inject"] for p in patterns}
        self.assertEqual(injects, {"NaN", "+Inf", "-Inf"})

    def test_envelope_authority_honest(self):
        prose = frozen_fixture_spec()["envelope_authority"]
        self.assertIn("freeze-from-prose", prose)
        self.assertIn("NO REV7", prose)
        self.assertIn("no literal JSON key table", prose)
        self.assertTrue(
            frozen_fixture_spec()["no_audio_generators_in_this_artifact"])

    def test_dependencies_bind_metrology_lock(self):
        deps = frozen_fixture_spec()["dependencies"]
        self.assertEqual(deps["contract_revision"], CONTRACT_REVISION)
        self.assertEqual(deps["metrology_lock_sha256"], metrology_lock_sha256())
        self.assertEqual(deps["gate_sample_rates"], list(GATE_SAMPLE_RATES))

    def test_sweep_checkpoints_reachable_within_match_radius(self):
        """M2 blocker guard: every lock checkpoint must be gate-closable."""
        assert_sweep_checkpoints_reachable()
        useful_start, useful_end = common_useful_window()
        hop = HOP_SAMPLES / float(CANONICAL_SAMPLE_RATE)
        t0 = sweep_active_start_seconds()
        t1 = sweep_active_end_seconds()
        self.assertEqual(t0, 0.5)
        self.assertEqual(t1, 1.75)
        self.assertLessEqual(useful_start, t0)
        self.assertLessEqual(t1, useful_end)

        for fs in GATE_SAMPLE_RATES:
            self.assertEqual((t0 * fs) % 1, 0.0)
            self.assertEqual((t1 * fs) % 1, 0.0)
            # Reachability must exercise the true V3FeatureFrame grid.
            self.assertTrue(_useful_frame_times(fs))

        crossings = frozen_fixture_spec()["categories"]["log_sweep"][
            "checkpoint_reachability"]["crossing_time_s"]
        for freq in SWEEP_CHECKPOINT_HZ:
            t_cross = sweep_crossing_time(float(freq))
            self.assertEqual(crossings[str(freq)], t_cross)
            self.assertGreaterEqual(t_cross, useful_start)
            self.assertLessEqual(t_cross, useful_end)
            for fs in GATE_SAMPLE_RATES:
                dist = nearest_useful_frame_distance(t_cross, fs)
                self.assertLessEqual(
                    dist, hop,
                    msg=(
                        f"{freq} Hz @ fs={fs}: nearest frame {dist} s > "
                        f"match radius {hop} s"
                    ),
                )

        # Extremes must not sit on asset edges (the pre-fix failure mode).
        self.assertAlmostEqual(
            sweep_crossing_time(20.0), t0, places=12)
        self.assertAlmostEqual(
            sweep_crossing_time(20000.0), t1, places=12)
        self.assertGreater(sweep_crossing_time(20.0), useful_start)
        self.assertLess(sweep_crossing_time(20000.0), useful_end)

    def test_useful_frame_times_match_contractual_source_time_grid(self):
        """REV6: source_time = frame_end/fs_c − delay; warm-up is exclusion only."""
        end = duration_seconds() - coda_seconds()
        for fs in (44100, 48000, 96000):
            delay_num, delay_den, _ = resampler_group_delay_rational(fs)
            delay = Fraction(delay_num, delay_den)
            wu = warm_up_seconds(fs)
            times = _useful_frame_times(fs)
            self.assertTrue(times, msg=f"empty useful grid at fs={fs}")

            expected: list[float] = []
            frame_end = N_LF
            while True:
                source_time = (
                    Fraction(frame_end, CANONICAL_SAMPLE_RATE) - delay
                )
                t = float(source_time)
                if t > end:
                    break
                if t >= wu:
                    expected.append(t)
                frame_end += HOP_SAMPLES
            self.assertEqual(times, expected)

            # Spot-check every returned time against the closed formula.
            for t in times:
                # Invert: frame_end = round((t + delay) * fs_c)
                frame_end_f = (Fraction.from_float(t) + delay) * CANONICAL_SAMPLE_RATE
                frame_end_i = int(round(float(frame_end_f)))
                self.assertEqual(
                    (frame_end_i - N_LF) % HOP_SAMPLES, 0,
                    msg=f"fs={fs} t={t} not on N_LF+m·H lattice",
                )
                recon = float(
                    Fraction(frame_end_i, CANONICAL_SAMPLE_RATE) - delay
                )
                self.assertAlmostEqual(recon, t, places=12)
                self.assertGreaterEqual(t, wu)
                self.assertLessEqual(t, end)

        # Regression: synthetic warm_up+k·hop lattice ≠ contractual grid
        # at a non-identity rate (delay ≠ 0). Prevents silent reintroduction.
        fs_bug = 44100
        delay_num, delay_den, _ = resampler_group_delay_rational(fs_bug)
        self.assertNotEqual((delay_num, delay_den), (0, 1))
        hop = HOP_SAMPLES / float(CANONICAL_SAMPLE_RATE)
        wu = warm_up_seconds(fs_bug)
        synthetic: list[float] = []
        t = wu
        while t <= end + 1e-12:
            if t <= end:
                synthetic.append(t)
            t += hop
        contractual = _useful_frame_times(fs_bug)
        self.assertNotEqual(
            synthetic, contractual,
            msg="synthetic warm_up-origin grid must differ from §6.2+§5 grid",
        )

    def test_canonical_hash_stable_across_calls(self):
        a = fixture_spec_sha256()
        b = fixture_spec_sha256()
        self.assertEqual(a, b)
        self.assertEqual(len(a), 64)
        self.assertEqual(a, hashlib.sha256(fixture_spec_bytes()).hexdigest())
        self.assertEqual(a, sha256_of_obj(frozen_fixture_spec()))

    def test_validate_accepts_frozen_copy(self):
        out = validate_fixture_spec_claim(frozen_fixture_spec())
        self.assertEqual(out["artifact_id"], FIXTURE_SPEC_ARTIFACT_ID)

    def test_golden_fixture_matches_frozen_bytes(self):
        self.assertTrue(FIXTURE.is_file(), f"missing golden {FIXTURE}")
        on_disk = FIXTURE.read_bytes()
        self.assertEqual(on_disk, fixture_spec_bytes())
        parsed = loads_strict(on_disk.decode("utf-8"))
        validate_fixture_spec_claim(parsed)


class RejectPathTests(unittest.TestCase):
    def test_non_object_claim_rejected(self):
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(["not", "an", "object"])

    def test_duration_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["global_duration"]["duration_s"] = 1.0
        claim["global_duration"]["duration_num"] = 1
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("duration", str(ctx.exception).lower())

    def test_false_duration_formula_allowed_rejected(self):
        claim = frozen_fixture_spec()
        claim["global_duration"]["false_formula_forbidden"] = "none"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_pseudo_noise_seed_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["categories"]["pseudo_noise"]["prng_seed"] = 1
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("prng_seed", str(ctx.exception))

    def test_pseudo_noise_phase_rule_unpin_rejected(self):
        claim = frozen_fixture_spec()
        claim["categories"]["pseudo_noise"]["phase_rule"] = "unspecified"
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("phase_rule", str(ctx.exception))

    def test_post_render_normalize_allowed_rejected(self):
        claim = frozen_fixture_spec()
        claim["numeric_generation"]["post_render_normalization"] = "allowed"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_cross_rate_resample_allowed_rejected(self):
        claim = frozen_fixture_spec()
        claim["numeric_generation"]["cross_rate_resample"] = "allowed"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_sweep_law_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["categories"]["log_sweep"]["sweep_law"] = "linear"
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("sweep_law", str(ctx.exception))

    def test_sweep_active_interval_tamper_rejected(self):
        claim = frozen_fixture_spec()
        # Full-asset sweep (the pre-fix blocker) must not validate.
        claim["categories"]["log_sweep"]["active_interval"]["t_start_s"] = 0.0
        claim["categories"]["log_sweep"]["active_interval"]["t_start_num"] = 0
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        msg = str(ctx.exception).lower()
        self.assertTrue(
            "t_start" in msg or "byte-identical" in msg,
            msg=str(ctx.exception),
        )

    def test_category_removed_rejected(self):
        claim = frozen_fixture_spec()
        del claim["categories"]["transient_burst"]
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("missing", str(ctx.exception))

    def test_metrology_dependency_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["dependencies"]["metrology_lock_sha256"] = "0" * 64
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("metrology_lock_sha256", str(ctx.exception))

    def test_file_format_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["numeric_generation"]["file_format"]["encoding"] = "pcm_int16_le"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_byte_identity_reject_extra_key(self):
        claim = frozen_fixture_spec()
        claim["extra_freedom"] = True
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("byte-identical", str(ctx.exception))


def _ensure_golden() -> None:
    """Dev helper: rewrite golden if missing (not used by unittest discovery)."""
    write_canonical(FIXTURE, frozen_fixture_spec())


if __name__ == "__main__":
    unittest.main()
```

---

## B.62 FILE: `ml_v3/tests/test_g1a_t6_generators.py`

**Path logico:** `ml_v3/tests/test_g1a_t6_generators.py`  
**Bytes:** 10512  
**Lines:** 281

```python
"""G1a T6 — fixture signal generators + committed WAV digests.

Freeze authority: fixture_spec_v1.json (513c3baf…). No G1a CLOSE claim.
"""
from __future__ import annotations

import math
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

from ml_v3.contracts.canonical import sha256_of_file, sha256sums_text
from ml_v3.contracts.constants import GATE_SAMPLE_RATES
from ml_v3.contracts.fixture_spec import (
    fixture_spec_sha256,
    frozen_fixture_spec,
    sample_count,
    sweep_active_end_seconds,
    sweep_active_start_seconds,
)
from ml_v3.contracts.sha256sums import (
    G1A_SHA256SUMS_COVERED,
    G1A_SHA256SUMS_RELPATH,
    Sha256SumsError,
    load_g1a_sha256sums,
    verify_g1a_sha256sums,
)
from ml_v3.fixtures.g1.render_signals import (
    AUDIO_REL_PREFIX,
    asset_relpath,
    iter_asset_specs,
    render_log_sweep,
    render_multitone,
    render_non_finite,
    render_pseudo_noise,
    render_silence,
    render_stereo_decorrelated,
    render_stereo_mid_only,
    render_stereo_side_only,
    write_wav_pcm_float32_le,
)

REPO = Path(__file__).resolve().parents[2]
FIXTURE_SPEC_DIGEST = (
    "513c3baf7aaed8eb1a15f7d2e875a3015479fc2ece0378e75cfceadefe68a6ef"
)


class FixtureSpecAuthorityTests(unittest.TestCase):
    def test_fixture_spec_digest_frozen(self):
        self.assertEqual(fixture_spec_sha256(), FIXTURE_SPEC_DIGEST)


class RenderSemanticsTests(unittest.TestCase):
    def test_direct_rates_no_resample_lengths(self):
        for fs in GATE_SAMPLE_RATES:
            self.assertEqual(render_multitone(fs).shape[0], sample_count(fs))
            self.assertEqual(render_log_sweep(fs).shape[0], sample_count(fs))
            self.assertEqual(render_pseudo_noise(fs).shape[0], sample_count(fs))

    def test_dtype_float32_after_cast(self):
        y = render_multitone(48000)
        self.assertEqual(y.dtype, np.float32)

    def test_log_sweep_active_interval_and_outside_zero(self):
        fs = 48000
        y = render_log_sweep(fs)
        t0 = sweep_active_start_seconds()
        t1 = sweep_active_end_seconds()
        self.assertEqual(t0, 0.5)
        self.assertEqual(t1, 1.75)
        # Sample just before active start must be zero.
        n0 = int(math.floor(t0 * fs)) - 1
        self.assertGreaterEqual(n0, 0)
        self.assertEqual(float(y[n0]), 0.0)
        # First active sample non-zero (phase starts at 0 → sin(0)=0 at exact
        # t_start; next sample must leave zero).
        n_start = int(round(t0 * fs))
        self.assertEqual(float(y[n_start]), 0.0)  # phi(t_start)=0
        self.assertNotEqual(float(y[n_start + 1]), 0.0)
        # Outside after t_end.
        n_after = int(math.floor(t1 * fs)) + 1
        self.assertEqual(float(y[n_after]), 0.0)

    def test_silence_and_non_finite_inject(self):
        z = render_silence(48000)
        self.assertTrue(np.all(z == 0.0))
        nan = render_non_finite(48000, "nan_at_sample_0")
        self.assertTrue(math.isnan(float(nan[0])))
        self.assertTrue(np.all(nan[1:] == 0.0))
        pos = render_non_finite(48000, "pos_inf_at_sample_0")
        self.assertTrue(math.isinf(float(pos[0])) and float(pos[0]) > 0)
        neg = render_non_finite(48000, "neg_inf_at_sample_0")
        self.assertTrue(math.isinf(float(neg[0])) and float(neg[0]) < 0)

    def test_stereo_ms_conventions(self):
        mid = render_stereo_mid_only(48000)
        side = render_stereo_side_only(48000)
        deco = render_stereo_decorrelated(48000)
        self.assertEqual(mid.shape[1], 2)
        # mid-only: L==R
        self.assertTrue(np.array_equal(mid[:, 0], mid[:, 1]))
        # side-only: R==-L
        self.assertTrue(np.array_equal(side[:, 1], -side[:, 0]))
        # decorrelated: channels differ
        self.assertFalse(np.array_equal(deco[:, 0], deco[:, 1]))

    def test_ultrasonic_only_96k(self):
        specs = [
            (cat, stem, fs)
            for cat, stem, fs, _ in iter_asset_specs()
            if cat == "ultrasonic_96k"
        ]
        self.assertEqual(specs, [("ultrasonic_96k", "ultrasonic_96k", 96000)])


def _wav_relpaths_under(root: Path) -> set[str]:
    audio = root / "ml_v3" / "fixtures" / "g1" / "audio"
    return {
        p.relative_to(root).as_posix()
        for p in audio.rglob("*.wav")
        if p.is_file()
    }


def _sha256_map(root: Path, rels: set[str]) -> dict[str, str]:
    return {rel: sha256_of_file(root / rel) for rel in sorted(rels)}


class DeterminismTests(unittest.TestCase):
    def test_two_run_byte_identical_core_renders(self):
        for fs in GATE_SAMPLE_RATES:
            a = render_multitone(fs)
            b = render_multitone(fs)
            self.assertEqual(a.tobytes(), b.tobytes())
            a = render_log_sweep(fs)
            b = render_log_sweep(fs)
            self.assertEqual(a.tobytes(), b.tobytes())
            a = render_pseudo_noise(fs)
            b = render_pseudo_noise(fs)
            self.assertEqual(a.tobytes(), b.tobytes())

    def test_two_run_byte_identical_wav_files(self):
        y = render_multitone(48000)
        with tempfile.TemporaryDirectory() as tmp:
            p1 = Path(tmp) / "a.wav"
            p2 = Path(tmp) / "b.wav"
            write_wav_pcm_float32_le(p1, y, 48000)
            write_wav_pcm_float32_le(p2, y, 48000)
            self.assertEqual(p1.read_bytes(), p2.read_bytes())
            # IEEE float format tag 3
            raw = p1.read_bytes()
            self.assertEqual(struct.unpack_from("<H", raw, 20)[0], 3)

    def test_two_clean_processes_byte_identical_wav_trees(self):
        """§13.2.2 proof for T6 fixture WAV only (not stack-wide G1).

        Two clean OS subprocesses → byte-identical WAV trees, then each of
        the 37 temp_A digests must equal the committed SHA256SUMS inventory.
        Does not call update_sha256sums_with_audio on the temp trees.
        """
        expected = {
            asset_relpath(cat, stem, fs)
            for cat, stem, fs, _fn in iter_asset_specs()
        }
        self.assertEqual(len(expected), 37)
        committed_sums = load_g1a_sha256sums()

        child = (
            "from pathlib import Path\n"
            "import sys\n"
            "from ml_v3.fixtures.g1.render_signals import render_all_to_tree\n"
            "rels = render_all_to_tree(Path(sys.argv[1]))\n"
            "sys.stdout.write('\\n'.join(rels))\n"
        )
        env = dict(os.environ)
        # Ensure repo root import path in a clean process.
        env["PYTHONPATH"] = (
            str(REPO)
            if not env.get("PYTHONPATH")
            else str(REPO) + os.pathsep + env["PYTHONPATH"]
        )

        with tempfile.TemporaryDirectory() as tmp_a, tempfile.TemporaryDirectory() as tmp_b:
            root_a = Path(tmp_a)
            root_b = Path(tmp_b)
            for root in (root_a, root_b):
                subprocess.run(
                    [sys.executable, "-c", child, str(root)],
                    check=True,
                    cwd=str(REPO),
                    env=env,
                    capture_output=True,
                    text=True,
                )

            rels_a = _wav_relpaths_under(root_a)
            rels_b = _wav_relpaths_under(root_b)
            self.assertEqual(len(rels_a), 37)
            self.assertEqual(len(rels_b), 37)
            self.assertEqual(rels_a, expected)
            self.assertEqual(rels_b, expected)

            for rel in sorted(expected):
                self.assertEqual(
                    (root_a / rel).read_bytes(),
                    (root_b / rel).read_bytes(),
                    msg=rel,
                )

            self.assertEqual(
                _sha256_map(root_a, rels_a),
                _sha256_map(root_b, rels_b),
            )

            # Bind two-process renders to the committed SHA256SUMS inventory.
            for rel in sorted(expected):
                self.assertIn(rel, committed_sums, msg=f"missing from SHA256SUMS: {rel}")
                self.assertEqual(
                    sha256_of_file(root_a / rel),
                    committed_sums[rel],
                    msg=rel,
                )


class CommittedInventoryTests(unittest.TestCase):
    def test_all_categories_present_in_inventory(self):
        specs = iter_asset_specs()
        self.assertGreaterEqual(len(specs), 30)
        categories = {cat for cat, _stem, _fs, _fn in specs}
        expected = set(frozen_fixture_spec()["categories"])
        self.assertEqual(categories, expected)

    def test_committed_wavs_match_rerender_and_sums(self):
        entries = verify_g1a_sha256sums()
        specs = iter_asset_specs()
        self.assertEqual(len(specs), 37)
        for category, stem, fs, fn in specs:
            rel = asset_relpath(category, stem, fs)
            absolute = REPO / rel
            self.assertTrue(absolute.is_file(), rel)
            self.assertTrue(rel.startswith(AUDIO_REL_PREFIX + "/"))
            # Byte-identical to a fresh render written through the same writer.
            rendered = fn(fs)
            with tempfile.TemporaryDirectory() as tmp:
                tmp_path = Path(tmp) / "x.wav"
                write_wav_pcm_float32_le(tmp_path, rendered, fs)
                self.assertEqual(
                    absolute.read_bytes(),
                    tmp_path.read_bytes(),
                    msg=rel,
                )
            self.assertEqual(entries[rel], sha256_of_file(absolute))

    def test_verify_rejects_sums_with_audio_stripped(self):
        """Truncated SHA256SUMS (COVERED only) must fail verify."""
        full = load_g1a_sha256sums()
        truncated = {path: full[path] for path in G1A_SHA256SUMS_COVERED}
        self.assertEqual(len(truncated), 11)
        with tempfile.TemporaryDirectory() as tmp:
            troot = Path(tmp)
            for rel in truncated:
                absolute = troot / rel
                absolute.parent.mkdir(parents=True, exist_ok=True)
                absolute.write_bytes((REPO / rel).read_bytes())
            sums_path = troot / G1A_SHA256SUMS_RELPATH
            sums_path.parent.mkdir(parents=True, exist_ok=True)
            sums_path.write_text(sha256sums_text(truncated), encoding="utf-8")
            with self.assertRaises(Sha256SumsError) as ctx:
                verify_g1a_sha256sums(troot)
            self.assertIn("missing required audio inventory", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
```

---

## B.63 FILE: `ml_v3/tests/test_g1a_f4_interpreter.py`

**Path logico:** `ml_v3/tests/test_g1a_f4_interpreter.py`  
**Bytes:** 951  
**Lines:** 29

```python
"""G1a F4 — gate-platform interpreter must match metrology lock pin."""
from __future__ import annotations

import unittest

from ml_v3.contracts.metrology_lock import (
    GATE_PLATFORM_PYTHON,
    frozen_metrology_lock,
    gate_platform_python_label,
    require_gate_platform_python,
)


class GatePlatformInterpreterTests(unittest.TestCase):
    def test_lock_pin_is_cpython_312_13(self):
        lock = frozen_metrology_lock()
        pinned = lock["bit_identity"]["gate_platform"]["python"]
        self.assertEqual(pinned, "CPython 3.12.13")
        self.assertEqual(pinned, GATE_PLATFORM_PYTHON)

    def test_running_interpreter_matches_lock(self):
        """G1a suite FAIL-closed unless canonical venv (3.12.13) is used."""
        label = require_gate_platform_python()
        self.assertEqual(label, GATE_PLATFORM_PYTHON)
        self.assertEqual(label, gate_platform_python_label())


if __name__ == "__main__":
    unittest.main()
```

---

## B.64 FILE: `ml_v3/frontend/__init__.py (SPIKE lab — ≠ G1b tip; evidence on spike branch)`

**Path logico:** `ml_v3/frontend/__init__.py`  
**Bytes:** 758  
**Lines:** 30

```python
"""G1b canonical frontend lab (offline).

Tranche T1: FIR coefficient generator (§5) + V3FeatureFrame schema stub (§7).
No streaming resampler, dual-resolution FFT, training, or Ableton ship.
"""
from __future__ import annotations

from .feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    feature_frame_field_names,
    validate_feature_frame_stub,
)
from .resampler_coeffs import (
    ResamplerCoeffError,
    fir_lowpass_coefficients,
    group_delay_rational,
    resample_ratio,
)

__all__ = [
    "FEATURE_FRAME_SCHEMA",
    "FeatureFrameError",
    "ResamplerCoeffError",
    "feature_frame_field_names",
    "fir_lowpass_coefficients",
    "group_delay_rational",
    "resample_ratio",
    "validate_feature_frame_stub",
]
```

---

## B.65 FILE: `ml_v3/frontend/resampler_coeffs.py (SPIKE)`

**Path logico:** `ml_v3/frontend/resampler_coeffs.py`  
**Bytes:** 3982  
**Lines:** 122

```python
"""§5 FIR low-pass polyphase coefficient generator (G1b T1).

G1a only serialized the generator parameters (`serialize_only`). This module
produces the actual coefficients from the frozen formulas. It does not stream
audio and does not claim sample-rate parity PASS.
"""
from __future__ import annotations

from math import gcd
from typing import NamedTuple

import numpy as np

from ml_v3.contracts.constants import (
    ACCEPTED_SAMPLE_RATES,
    CANONICAL_SAMPLE_RATE,
)
from ml_v3.contracts.metrology_lock import (
    KAISER_BETA,
    RESAMPLER_PASS_HZ,
)

__all__ = [
    "ResamplerCoeffError",
    "ResampleRatio",
    "resample_ratio",
    "group_delay_rational",
    "fir_lowpass_coefficients",
]


class ResamplerCoeffError(ValueError):
    """Fail-closed error for invalid sample rate or degenerate coefficients."""


class ResampleRatio(NamedTuple):
    up: int
    down: int
    num_taps: int | None  # None when identity (up == down == 1)
    identity: bool


def _reduce(num: int, den: int) -> tuple[int, int]:
    if den <= 0:
        raise ResamplerCoeffError(f"non-positive denominator: {den}")
    g = gcd(num, den)
    return num // g, den // g


def resample_ratio(fs_in: int) -> ResampleRatio:
    """Return up/down/num_taps for fs_in → 48000 (§5)."""
    if not isinstance(fs_in, int) or isinstance(fs_in, bool):
        raise ResamplerCoeffError(f"fs_in must be int, got {type(fs_in).__name__}")
    if fs_in not in ACCEPTED_SAMPLE_RATES:
        raise ResamplerCoeffError(
            f"fs_in {fs_in} rejected; accepted={ACCEPTED_SAMPLE_RATES}"
        )
    g = gcd(fs_in, CANONICAL_SAMPLE_RATE)
    up = CANONICAL_SAMPLE_RATE // g
    down = fs_in // g
    if up == 1 and down == 1:
        return ResampleRatio(up=1, down=1, num_taps=None, identity=True)
    num_taps = 128 * max(up, down) + 1
    return ResampleRatio(up=up, down=down, num_taps=num_taps, identity=False)


def group_delay_rational(fs_in: int) -> tuple[int, int]:
    """Group delay as reduced (num, den) seconds (§5).

    Identity → (0, 1). Matches metrology lock for gate rates.
    """
    ratio = resample_ratio(fs_in)
    if ratio.identity:
        return 0, 1
    assert ratio.num_taps is not None
    return _reduce(ratio.num_taps - 1, 2 * ratio.up * fs_in)


def fir_lowpass_coefficients(fs_in: int) -> np.ndarray:
    """Generate causal FIR low-pass coefficients for polyphase phase-zero (§5).

    Returns float64 array length `num_taps`, or length-0 for identity.
    Formulas (contract §5):

        stop_hz = min(fs_in, 48000) / 2
        fc = ((pass_hz + stop_hz) / 2) / (fs_in * up)
        M = num_taps - 1
        h0[n] = 2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0)
        h[n]  = up * h0[n] / sum(h0)

    `numpy.sinc` is sin(pi*x)/(pi*x), matching the contract definition.
    """
    ratio = resample_ratio(fs_in)
    if ratio.identity:
        return np.zeros(0, dtype=np.float64)

    up = ratio.up
    num_taps = ratio.num_taps
    assert num_taps is not None
    m = num_taps - 1
    stop_hz = min(float(fs_in), float(CANONICAL_SAMPLE_RATE)) / 2.0
    pass_hz = float(RESAMPLER_PASS_HZ)
    if not (pass_hz < stop_hz):
        raise ResamplerCoeffError(
            f"degenerate band edges: pass_hz={pass_hz} stop_hz={stop_hz}"
        )
    fc = ((pass_hz + stop_hz) / 2.0) / (float(fs_in) * float(up))
    if not (0.0 < fc < 0.5):
        raise ResamplerCoeffError(f"fc out of open unit interval: {fc}")

    n = np.arange(num_taps, dtype=np.float64)
    # numpy.kaiser(M+1, beta) == window over n=0..M
    window = np.kaiser(num_taps, float(KAISER_BETA)).astype(np.float64, copy=False)
    arg = 2.0 * fc * (n - (m / 2.0))
    h0 = (2.0 * fc) * np.sinc(arg) * window
    s = float(np.sum(h0))
    if not np.isfinite(s) or s == 0.0:
        raise ResamplerCoeffError(f"degenerate h0 sum: {s!r}")
    h = (float(up) * h0) / s
    if not np.all(np.isfinite(h)):
        raise ResamplerCoeffError("non-finite coefficient produced")
    return h
```

---

## B.66 FILE: `ml_v3/frontend/feature_frame.py (SPIKE)`

**Path logico:** `ml_v3/frontend/feature_frame.py`  
**Bytes:** 4772  
**Lines:** 161

```python
"""V3FeatureFrame schema stub (§7) — G1b T1.

Defines the frozen surface and fail-closed structural validation.
Does not extract features, run FFT, or claim streaming≡offline.
"""
from __future__ import annotations

import math
from typing import Any, Mapping

from ml_v3.contracts.constants import (
    CANONICAL_SAMPLE_RATE,
    GRID_BANDS,
    SCHEMA_IDS,
)

__all__ = [
    "FEATURE_FRAME_SCHEMA",
    "FeatureFrameError",
    "BAND_VECTOR_FIELDS",
    "SCALAR_FLOAT_FIELDS",
    "BOOL_FIELDS",
    "INT_FIELDS",
    "feature_frame_field_names",
    "validate_feature_frame_stub",
]

FEATURE_FRAME_SCHEMA: str = SCHEMA_IDS["feature_frame"]

BAND_VECTOR_FIELDS: tuple[str, ...] = (
    "mid_psd_db",
    "side_psd_db",
    "mid_shape_db",
    "side_shape_db",
    "mid_prominence_db",
    "side_prominence_db",
    "mid_delta_db",
    "side_delta_db",
)

SCALAR_FLOAT_FIELDS: tuple[str, ...] = (
    "mid_level_dbfs",
    "side_level_dbfs",
)

BOOL_FIELDS: tuple[str, ...] = (
    "mid_valid",
    "side_valid",
    "valid",
)

INT_FIELDS: tuple[str, ...] = (
    "frame_end_sample",
    "frame_index",
    "source_time_num",
    "source_time_den",
    "canonical_sample_rate",
)


class FeatureFrameError(ValueError):
    """Fail-closed structural / finiteness error for a feature-frame stub."""


def feature_frame_field_names() -> tuple[str, ...]:
    """All top-level keys required by §7 (plus schema)."""
    return (
        "schema",
        *INT_FIELDS,
        *BAND_VECTOR_FIELDS,
        *SCALAR_FLOAT_FIELDS,
        *BOOL_FIELDS,
        "reason",
    )


def _require_finite_float(value: Any, path: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise FeatureFrameError(f"{path} must be a finite number")
    f = float(value)
    if not math.isfinite(f):
        raise FeatureFrameError(f"{path} is non-finite: {value!r}")
    return f


def validate_feature_frame_stub(frame: Mapping[str, Any]) -> dict[str, Any]:
    """Validate a dict against the §7 surface (structure + finiteness only).

    Returns a shallow copy of accepted fields. Does not compute features.
    """
    if not isinstance(frame, Mapping):
        raise FeatureFrameError("frame must be a mapping")

    required = set(feature_frame_field_names())
    keys = set(frame.keys())
    missing = sorted(required - keys)
    if missing:
        raise FeatureFrameError(f"missing fields: {missing}")
    extra = sorted(keys - required)
    if extra:
        raise FeatureFrameError(f"unknown fields: {extra}")

    out: dict[str, Any] = {}
    schema = frame["schema"]
    if schema != FEATURE_FRAME_SCHEMA:
        raise FeatureFrameError(
            f"schema must be {FEATURE_FRAME_SCHEMA!r}, got {schema!r}"
        )
    out["schema"] = schema

    for name in INT_FIELDS:
        value = frame[name]
        if isinstance(value, bool) or not isinstance(value, int):
            raise FeatureFrameError(f"{name} must be int")
        out[name] = value

    if out["canonical_sample_rate"] != CANONICAL_SAMPLE_RATE:
        raise FeatureFrameError(
            f"canonical_sample_rate must be {CANONICAL_SAMPLE_RATE}"
        )
    if out["source_time_den"] <= 0:
        raise FeatureFrameError("source_time_den must be > 0")
    if out["frame_end_sample"] < 0 or out["frame_index"] < 0:
        raise FeatureFrameError("frame_end_sample/frame_index must be >= 0")

    for name in BAND_VECTOR_FIELDS:
        vec = frame[name]
        if not isinstance(vec, (list, tuple)):
            raise FeatureFrameError(f"{name} must be a sequence of length {GRID_BANDS}")
        if len(vec) != GRID_BANDS:
            raise FeatureFrameError(
                f"{name} length {len(vec)} != {GRID_BANDS}"
            )
        out[name] = [_require_finite_float(v, f"{name}[{i}]") for i, v in enumerate(vec)]

    for name in SCALAR_FLOAT_FIELDS:
        out[name] = _require_finite_float(frame[name], name)

    for name in BOOL_FIELDS:
        value = frame[name]
        if not isinstance(value, bool):
            raise FeatureFrameError(f"{name} must be bool")
        out[name] = value

    reason = frame["reason"]
    if out["valid"]:
        if reason is not None:
            raise FeatureFrameError("reason must be null when valid is true")
        out["reason"] = None
    else:
        if not isinstance(reason, str) or not reason:
            raise FeatureFrameError(
                "reason must be a non-empty string when valid is false"
            )
        out["reason"] = reason

    # Mono contract reminder: side_valid false is allowed; do not invent policy.
    if out["valid"] != (out["mid_valid"] or out["side_valid"]):
        raise FeatureFrameError("valid must equal mid_valid OR side_valid")

    return out
```

---

## B.67 FILE: `ml_v3/tests/test_g1b_t1_resampler_coeffs.py (SPIKE)`

**Path logico:** `ml_v3/tests/test_g1b_t1_resampler_coeffs.py`  
**Bytes:** 6735  
**Lines:** 189

```python
"""G1b T1 — FIR coefficient generator + feature-frame stub (fail-closed)."""
from __future__ import annotations

import math
import unittest

import numpy as np

from ml_v3.contracts.constants import (
    ACCEPTED_SAMPLE_RATES,
    CANONICAL_SAMPLE_RATE,
    GATE_SAMPLE_RATES,
    GRID_BANDS,
)
from ml_v3.contracts.metrology_lock import (
    KAISER_BETA,
    frozen_metrology_lock,
    resampler_group_delay_rational,
)
from ml_v3.frontend.feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    feature_frame_field_names,
    validate_feature_frame_stub,
)
from ml_v3.frontend.resampler_coeffs import (
    ResamplerCoeffError,
    fir_lowpass_coefficients,
    group_delay_rational,
    resample_ratio,
)


def _minimal_valid_frame(**overrides):
    floor = [-120.0] * GRID_BANDS
    base = {
        "schema": FEATURE_FRAME_SCHEMA,
        "frame_end_sample": 8192,
        "frame_index": 0,
        "source_time_num": 0,
        "source_time_den": 1,
        "canonical_sample_rate": CANONICAL_SAMPLE_RATE,
        "mid_psd_db": list(floor),
        "side_psd_db": list(floor),
        "mid_shape_db": list(floor),
        "side_shape_db": list(floor),
        "mid_prominence_db": list(floor),
        "side_prominence_db": list(floor),
        "mid_delta_db": [0.0] * GRID_BANDS,
        "side_delta_db": [0.0] * GRID_BANDS,
        "mid_level_dbfs": -120.0,
        "side_level_dbfs": -120.0,
        "mid_valid": True,
        "side_valid": False,
        "valid": True,
        "reason": None,
    }
    base.update(overrides)
    return base


class ResampleRatioTests(unittest.TestCase):
    def test_identity_48000(self):
        r = resample_ratio(48000)
        self.assertTrue(r.identity)
        self.assertEqual((r.up, r.down, r.num_taps), (1, 1, None))
        self.assertEqual(group_delay_rational(48000), (0, 1))
        self.assertEqual(fir_lowpass_coefficients(48000).shape, (0,))

    def test_gate_ratios_match_metrology_lock(self):
        lock = frozen_metrology_lock()["resampler_group_delay"]["per_gate_sample_rate"]
        for fs in GATE_SAMPLE_RATES:
            r = resample_ratio(fs)
            entry = lock[str(fs)]
            self.assertEqual(r.up, entry["up"])
            self.assertEqual(r.down, entry["down"])
            self.assertEqual(r.num_taps, entry["num_taps"])
            n, d = group_delay_rational(fs)
            self.assertEqual((n, d), (entry["delay_num"], entry["delay_den"]))
            # Cross-check serialize-only helper still agrees.
            ln, ld, lt = resampler_group_delay_rational(fs)
            self.assertEqual((n, d, r.num_taps), (ln, ld, lt))

    def test_reject_invalid_sample_rate(self):
        with self.assertRaises(ResamplerCoeffError):
            resample_ratio(32000)
        with self.assertRaises(ResamplerCoeffError):
            fir_lowpass_coefficients(32000)
        with self.assertRaises(ResamplerCoeffError):
            resample_ratio(48000.0)  # type: ignore[arg-type]


class FirCoefficientTests(unittest.TestCase):
    def test_length_and_sum_equals_up(self):
        for fs in (44100, 96000, 88200):
            r = resample_ratio(fs)
            h = fir_lowpass_coefficients(fs)
            self.assertEqual(h.dtype, np.float64)
            self.assertEqual(h.shape, (r.num_taps,))
            self.assertTrue(np.all(np.isfinite(h)))
            # h = up * h0 / sum(h0) ⇒ sum(h) == up
            self.assertTrue(math.isclose(float(np.sum(h)), float(r.up), rel_tol=0, abs_tol=1e-9))

    def test_deterministic_byte_identical(self):
        a = fir_lowpass_coefficients(96000)
        b = fir_lowpass_coefficients(96000)
        self.assertEqual(a.tobytes(), b.tobytes())

    def test_kaiser_beta_frozen(self):
        self.assertEqual(KAISER_BETA, 9.0)

    def test_accepted_non_gate_rates_generate(self):
        # Experimental rates accepted structurally; cannot close SR-parity alone.
        for fs in ACCEPTED_SAMPLE_RATES:
            h = fir_lowpass_coefficients(fs)
            r = resample_ratio(fs)
            if r.identity:
                self.assertEqual(h.size, 0)
            else:
                self.assertEqual(h.size, r.num_taps)


class FeatureFrameStubTests(unittest.TestCase):
    def test_happy_path(self):
        out = validate_feature_frame_stub(_minimal_valid_frame())
        self.assertEqual(out["schema"], FEATURE_FRAME_SCHEMA)
        self.assertEqual(len(out["mid_psd_db"]), GRID_BANDS)

    def test_required_field_set_matches_section_7(self):
        names = feature_frame_field_names()
        self.assertIn("schema", names)
        self.assertIn("mid_psd_db", names)
        self.assertIn("mid_prominence_db", names)
        self.assertIn("reason", names)
        self.assertEqual(len(names), len(set(names)))

    def test_reject_wrong_band_length(self):
        frame = _minimal_valid_frame(mid_psd_db=[-120.0] * (GRID_BANDS - 1))
        with self.assertRaises(FeatureFrameError) as ctx:
            validate_feature_frame_stub(frame)
        self.assertIn("mid_psd_db", str(ctx.exception))

    def test_reject_nan(self):
        vec = [-120.0] * GRID_BANDS
        vec[0] = float("nan")
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(_minimal_valid_frame(mid_psd_db=vec))

    def test_reject_inf_level(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(mid_level_dbfs=float("inf"))
            )

    def test_reject_valid_reason_inconsistency(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(valid=True, reason="oops")
            )
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(
                    mid_valid=False, side_valid=False, valid=False, reason=None
                )
            )

    def test_reject_valid_not_or_of_channels(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(
                    mid_valid=False, side_valid=False, valid=True, reason=None
                )
            )

    def test_reject_wrong_schema(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(schema="aieq-v3-feature-frame-0")
            )

    def test_reject_unknown_field(self):
        frame = _minimal_valid_frame()
        frame["extra"] = 1
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(frame)


if __name__ == "__main__":
    unittest.main()
```

---

## C. Fine snapshot

Handoff aggiornato: HEAD `78da84dd`; G1a CLOSE GO; A3 falsification **FAIL honest** (6.435 dB); REV7 consolidate NO; No G1 PASS; no G1b tip. Next: non-POROUS redteam/CC on A3 formula, then tip decision. Non rilassare gate.
