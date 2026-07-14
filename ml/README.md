# ml/ — offline training pipeline (MLEQ)

Offline (out-of-plugin) training for the MLEngine problem/frequency networks.
The plugin stays **inference-only**; this pipeline produces the weight blobs
it ships.

Pure numpy — no torch/tensorflow dependency. The network is small enough
(~19k parameters without the dropped genreNet) that numpy Adam trains it in
seconds; keeping the dependency surface tiny makes the pipeline runnable on
any dev machine (`pip install -r ml/requirements.txt`).

## Modules

| File | Role |
|------|------|
| `features.py` | EXACT port of the C++ inference feature path (SpectrumAnalyzer mirror + `extractMelBands`), feature v1 (absolute dB) and v2 (loudness-invariant frame-mean normalization) |
| `model.py` | problemNet + freqNet in numpy (same shapes/layout as `MLEngine::DenseLayer`), Adam, BCE + masked-Huber losses. genreNet intentionally absent |
| `dataset.py` | synthetic generator (port of `generateSyntheticDataset`) + VocalSet real-audio injection factory with singer-held-out split (`female9/female8/male11/male10`) |
| `blob_io.py` | v1 blob read/write (byte-compatible with shipped format) + v2 blob (shape metadata, provenance JSON, embedded FNV-1a checksum) |
| `train.py` | training entry point, deterministic, exports v2 blob |
| `eval.py` | per-class P/R/F1 + clean-FP rate at the plugin's base thresholds |
| `tests/` | feature-contract test against C++-generated fixtures |

## Cross-language contract

`Source/Tests/MLFeatureContractTest.cpp` writes `ml/fixtures/feature_contract.json`
(inputs + expected features from the C++ implementation). `ml/tests/test_feature_contract.py`
asserts the Python port matches within 1e-4 (float32 vs float64). **Any change
to the feature path on either side must keep this green in the same commit.**

## Typical runs

Synthetic-only smoke (fast):

    python3 -m ml.train --out /tmp/mleq_v2_synth.bin --feature-version 2 \
        --epochs 20 --samples-per-problem 64

Full run with VocalSet real audio:

    python3 -m ml.train --out Resources/Models/ml_weights_v2.bin \
        --feature-version 2 --epochs 40 --samples-per-problem 96 \
        --vocalset /Users/marco/aieq_data/real_audio/vocalset_extracted/FULL

The exported blob embeds provenance (git rev, dataset hash, metrics config) and
an FNV-1a integrity checksum verified by the C++ v2 loader at load time.
