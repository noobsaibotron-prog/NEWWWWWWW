# Boom measured-gate probe

Verdict: **GO TO IMPLEMENT THE FLAGGED ABLATION; TRAINING NOT YET AUTHORIZED**

The probe measured train and heldout material only. It did not read A6 judge
clips, train a model, change a label, or modify the manifest.

## Frozen evidence

- Probe code: `e4993a16`
- Manifest opportunities: 3370 (10110 measurements at 6/9/12 dB)
- Full corpus-probe JSON SHA-256:
  `ff98ce3cc091452fa8df47ee9c9d0ec3a18bd57daebdd26ec78d0c4548673177`
- Exact builder-replay v2 records: 864
- Exact replay JSON SHA-256:
  `7d5ab4df9c1e704455de19c466f4c8994058a36cef6f8de553a02e46298d2cee`
- Frozen directory:
  `/Users/marco/aieq_data/baselines/boom_gate_probe_20260719_e4993a16_v2/`
- `SHA256SUMS` SHA-256:
  `dccce8372cf7c1be4b5cc59478a0dd13935d50d92c79df614a0434e0d46a8472`

The superseded v1 replay used dataset seed 42/1337 directly for heldout. The
trainer actually uses 43/1338. V2 corrects that mapping and its counts match the
flagged builder exactly; the v1 archive remains untouched for audit history.

Three manifest files were shorter than one 32-frame window. They were recorded
explicitly in the JSON; the existing builder also produces no window for them.

## Counter-check findings

The earlier statement that Boom positives were mostly vocal was stale. In the
dataset-seed-42 CONTROL, 68/344 Boom positives are vocal, while 194/344 come
from the clean-bass and clean-drums contrastive axes. The problem is broader:
current Boom positives are admitted without proving that low-end content exists,
that the raw frame is not already strongly boomy, or that the injected result
crosses a measured excess floor.

An absolute dB floor was rejected because it changes with file level. The gate
uses relative measures:

- content = mean(40-150 Hz) - mean(100-10000 Hz);
- Boom excess = mean(40-150 Hz) - mean(150-400 Hz);
- delta = post-injection excess - pre-injection excess.

## Frozen candidate gate

| Condition | Threshold |
|---|---:|
| Relative low-end content | >= 9 dB |
| Raw/pre Boom excess | <= 9 dB |
| Post-injection Boom excess | >= 6 dB |
| Injection delta excess | >= 4 dB |

The choice is intentionally between two bad extremes. A stricter
`pre<=6/post>=9` gate leaves only 48-49 train positives and was rejected as
underpowered. The selected gate preserves a viable, stable count while removing
invisible and already-dominant examples:

| Dataset seed | Split | CONTROL Boom positives | Gate-accepted | Vocal share | Bass/drums axis share |
|---:|---|---:|---:|---:|---:|
| 42 | train | 344 | 114 | 4.4% | 70.2% |
| 42 | heldout | 77 | 27 | 0.0% | 66.7% |
| 1337 | train | 362 | 120 | 5.8% | 65.0% |
| 1337 | heldout | 81 | 30 | 13.3% | 63.3% |

## Implementation contract

1. Add the gate behind a dataset recipe flag whose default is OFF. The default
   CONTROL path must remain bit-identical.
2. Apply the same measured gate to random and contrastive-axis Boom injections;
   do not change gains, shapes, class weights, other classes, or runtime code.
3. Include the flag and all four thresholds in candidate provenance and the
   dataset cache key.
4. Full training is fail-closed if either data realization produces fewer than
   100 train or 20 heldout Boom positives. Quick smoke is exempt from this count.
5. Before any full grid, prove default-OFF CONTROL parity and run one quick
   flag-ON smoke for both dataset seeds.

The full 3x2 ablation qualifies only as an independent Boom lever if clip `02`
passes in at least 4/6 paired runs, median heldout Boominess improves, worst
clean-FP does not increase, and no new paired A6 FAIL appears. Clip `04` remains
an independent blocker; the unchanged final candidate gate still requires both
`02` and `04` green in at least 4/6.
