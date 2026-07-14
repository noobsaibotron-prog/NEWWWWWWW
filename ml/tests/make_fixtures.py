"""Generate ml/fixtures/feature_contract.json — the cross-language feature
contract fixture.

Inputs are DETERMINISTIC closed-form spectra (no RNG): mixtures of Gaussian
bumps on tilted baselines, expressed in linear magnitude, at the two shapes
the plugin actually uses (1024 bins ~ fft 2048 synthetic path; 2049 bins ~
fft 4096 analysis path).

The committed JSON holds the inputs and the PYTHON-computed v1/v2 features.
  - ml/tests/test_feature_contract.py asserts the python impl reproduces the
    committed values exactly (self-drift guard).
  - Source/Tests/MLFeatureContractTest.cpp asserts the C++ impl matches within
    1e-4 (float32 vs float64).

Regenerate ONLY when the feature definition changes, in the same commit as
both implementations:
    python3 -m ml.tests.make_fixtures
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from ml.features import features_v1, features_v2

SAMPLE_RATE = 44100.0


def gaussian_bump(freqs: np.ndarray, center: float, sigma: float,
                  amp: float) -> np.ndarray:
    return amp * np.exp(-0.5 * ((freqs - center) / sigma) ** 2)


def tilt(freqs: np.ndarray, db_per_decade: float,
         ref_hz: float = 100.0) -> np.ndarray:
    return 10.0 ** ((db_per_decade
                     * np.log10(np.maximum(freqs, 20.0) / ref_hz)) / 20.0)


def build_cases() -> list[dict]:
    cases = []
    for n_bins, tag in ((1024, "fft2048"), (2049, "fft4096")):
        fft_size = n_bins * 2
        bin_hz = SAMPLE_RATE / fft_size
        freqs = np.arange(n_bins) * bin_hz

        flat = np.full(n_bins, 0.05)
        cases.append((f"{tag}-flat", flat))

        tilted = 0.06 * tilt(freqs, -3.0)
        cases.append((f"{tag}-tilt3", tilted))

        resonant = tilted + gaussian_bump(freqs, 3200.0, 160.0, 0.5)
        cases.append((f"{tag}-res3200", resonant))

        sibilant = tilted + gaussian_bump(freqs, 7000.0, 560.0, 0.4)
        cases.append((f"{tag}-sib7k", sibilant))

        quiet = resonant * 10.0 ** (-24.0 / 20.0)   # -24 dB broadband
        cases.append((f"{tag}-res3200-minus24db", quiet))

        loud = resonant * 10.0 ** (+12.0 / 20.0)    # +12 dB broadband
        cases.append((f"{tag}-res3200-plus12db", loud))

    out = []
    for name, spec in cases:
        spec = np.maximum(spec, 0.0)
        out.append({
            "name": name,
            "sample_rate": SAMPLE_RATE,
            "spectrum": [float(f"{v:.9g}") for v in spec],
            "features_v1": [float(f"{v:.9g}") for v in features_v1(spec, SAMPLE_RATE)],
            "features_v2": [float(f"{v:.9g}") for v in features_v2(spec, SAMPLE_RATE)],
        })
    return out


def main() -> None:
    fixture_path = Path(__file__).resolve().parents[1] / "fixtures" / "feature_contract.json"
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "description": "Cross-language ML feature contract (python == source of truth "
                       "for the committed values; C++ must match within 1e-4)",
        "feature_v2_range_db": 80.0,
        "cases": build_cases(),
    }
    fixture_path.write_text(json.dumps(payload, indent=1))
    print(f"wrote {fixture_path} ({fixture_path.stat().st_size} bytes, "
          f"{len(payload['cases'])} cases)")


if __name__ == "__main__":
    main()
