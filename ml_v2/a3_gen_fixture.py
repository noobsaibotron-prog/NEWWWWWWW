#!/usr/bin/env python3
"""Motore v2 — A3 parity fixture generator.

Builds the REAL A3 architecture with seed-fixed numpy weights, computes the
ground-truth [17] logits of the LAST valid frame for a 32x64 log-mel-shaped
random window, and writes the RTNeural JSON + fixture CSVs consumed by
Source/Tests/MotoreV2ParityTest.cpp.

Ground truth = the numpy reference (torch semantics). When torch is importable
the SAME weights are loaded into the real MotoreV2CNN and cross-checked against
the numpy reference (< 1e-9 in float64) — two independent implementations
witnessing the same numbers. The committed fixture is byte-identical either way.

Run:  python3 -m ml_v2.a3_gen_fixture   (from the repo root)
"""
import json
import os

import numpy as np

from .export_rtneural import build_a3_model_json
from .model import HAS_TORCH, MEL, OUT, T_WINDOW, numpy_forward_last, seed_weights

SEED = 20260711

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "Source", "Tests", "data",
                                        "motore_v2_a3"))


def main() -> None:
    w = seed_weights(SEED)
    rng = np.random.default_rng(SEED + 1)
    x = rng.standard_normal((T_WINDOW, MEL))          # [T, MEL] window

    y = numpy_forward_last(w, x)                      # [17] ground truth

    if HAS_TORCH:
        import torch
        from .model import MotoreV2CNN, load_seed_weights_into_torch
        net = MotoreV2CNN()
        load_seed_weights_into_torch(net, w)
        with torch.no_grad():
            yt = net(torch.from_numpy(x.T[None]))[0, :, -1].numpy()
        delta = float(np.max(np.abs(yt - y)))
        assert delta < 1e-9, f"torch vs numpy reference diverge: {delta}"
        print(f"torch cross-check OK (max|delta| = {delta:.3e})")
    else:
        print("torch not importable here: numpy reference only (fixture identical)")

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "model.json"), "w") as f:
        json.dump(build_a3_model_json(w), f)
    np.savetxt(os.path.join(OUT_DIR, "input.csv"), x, delimiter=",")
    np.savetxt(os.path.join(OUT_DIR, "expected.csv"), y, delimiter=",")

    n_params = sum(v.size for v in w.values())
    print(f"A3: MEL={MEL} T={T_WINDOW} OUT={OUT} seed={SEED} params={n_params}")
    print(f"wrote fixture to {OUT_DIR}")


if __name__ == "__main__":
    main()
