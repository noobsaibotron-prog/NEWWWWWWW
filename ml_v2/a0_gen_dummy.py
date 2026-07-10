#!/usr/bin/env python3
"""Motore v2 — A0 walking skeleton: generate a dummy [17-output] RTNeural model.

NO torch/keras dependency: numpy defines the model (seed-fixed weights), computes
the reference forward, and writes the JSON in the format RTNeural::json_parser
expects (verified against RTNeural/models/dense.json). The 17 outputs are the
product-v2 concat head [8 classes | 1 presence | 8 freq] as RAW logits — the
per-segment sigmoid lives downstream in the C++ consumer, so A0 validates the
raw bridge only.

Outputs (committed as the parity-test fixture, fully reproducible):
    Source/Tests/data/motore_v2_a0/dummy_model.json
    Source/Tests/data/motore_v2_a0/input.csv
    Source/Tests/data/motore_v2_a0/expected.csv
"""
import json
import os
import numpy as np

SEED = 20260710
IN = 16          # dummy input width (irrelevant to the bridge; small for lightness)
HID = 32
OUT = 17         # concat head [8 classes | 1 presence | 8 freq]

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "Source", "Tests", "data", "motore_v2_a0"))


def main() -> None:
    rng = np.random.default_rng(SEED)
    W1 = (rng.standard_normal((IN, HID)) * 0.3).astype(np.float64)   # [in][hid]
    b1 = (rng.standard_normal(HID) * 0.1).astype(np.float64)
    W2 = (rng.standard_normal((HID, OUT)) * 0.3).astype(np.float64)  # [hid][out]
    b2 = (rng.standard_normal(OUT) * 0.1).astype(np.float64)
    x = (rng.standard_normal(IN) * 1.0).astype(np.float64)

    # Reference forward (ground truth).
    h = np.tanh(x @ W1 + b1)
    y = h @ W2 + b2

    model = {
        "in_shape": [None, IN],
        "layers": [
            {"type": "dense", "activation": "", "shape": [None, HID],
             "weights": [W1.tolist(), b1.tolist()]},
            {"type": "activation", "activation": "tanh", "shape": [None, HID],
             "weights": []},
            {"type": "dense", "activation": "", "shape": [None, OUT],
             "weights": [W2.tolist(), b2.tolist()]},
        ],
    }

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "dummy_model.json"), "w") as f:
        json.dump(model, f)
    np.savetxt(os.path.join(OUT_DIR, "input.csv"), x, delimiter=",")
    np.savetxt(os.path.join(OUT_DIR, "expected.csv"), y, delimiter=",")

    print(f"IN={IN} HID={HID} OUT={OUT} seed={SEED}")
    print(f"wrote fixture to {OUT_DIR}")


if __name__ == "__main__":
    main()
