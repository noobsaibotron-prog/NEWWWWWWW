"""Self-drift guard: the python feature implementation must reproduce the
committed fixture exactly (it generated it). Run:
    python3 -m ml.tests.test_feature_contract
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

from ml.features import features_v1, features_v2


def main() -> int:
    fixture_path = Path(__file__).resolve().parents[1] / "fixtures" / "feature_contract.json"
    payload = json.loads(fixture_path.read_text())
    failures = 0
    for case in payload["cases"]:
        spec = np.asarray(case["spectrum"], dtype=np.float64)
        sr = case["sample_rate"]
        for version, fn in ((1, features_v1), (2, features_v2)):
            got = fn(spec, sr)
            want = np.asarray(case[f"features_v{version}"], dtype=np.float64)
            if got.shape != want.shape or not np.allclose(got, want, atol=1e-7):
                worst = float(np.max(np.abs(got - want))) if got.shape == want.shape else float("nan")
                print(f"FAIL {case['name']} v{version}: max|diff|={worst:.3e}")
                failures += 1
    n = len(payload["cases"]) * 2
    print(f"{n - failures}/{n} contract checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
