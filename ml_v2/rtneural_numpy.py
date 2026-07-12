"""Motore v2 — numpy forward for RTNeural JSON models (benchmark engine).

Loads the JSON exported by ml_v2.export_rtneural and runs the exact same math
as the C++ RTNeural runtime, vectorized in numpy. Used by the A6 benchmark
(eval_benchmark.py) so Python-side evaluation shares the model contract that
the A3 parity test pinned against the C++.

Format facts this loader relies on (all verified against the vendored parser
`ThirdParty/RTNeural/RTNeural/model_loader.h` and measured in A3):
  - layer["weights"] is a LIST: [kernel, bias]  (there is NO "bias" key);
  - conv1d kernel nesting is [kernel][in][out], taps COMPACT;
  - conv1d "kernel_size"/"dilation" are 1-element ARRAYS (e.g. [5], [2]);
  - dense kernel nesting is [in][out];
  - "activation" is inline on the layer node.

Self-check: `python3 -m ml_v2.rtneural_numpy` loads the committed A3 fixture
(Source/Tests/data/motore_v2_a3) and asserts max|delta| < 1e-6 vs expected.csv.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np


def _activate(x: np.ndarray, act: str) -> np.ndarray:
    if act == "relu":
        return np.maximum(x, 0.0)
    if act == "tanh":
        return np.tanh(x)
    if act == "sigmoid":
        return 1.0 / (1.0 + np.exp(-x))
    if act in ("", None):
        return x
    raise ValueError(f"unsupported activation: {act}")


class RTNeuralNumpyModel:
    """Sequential conv1d/dense model matching RTNeural's streaming semantics.

    forward_window(x[T, in]) computes the full valid causal stack and returns
    the LAST frame's outputs — identical to C++ reset() + T forward() calls
    (the A3 parity contract), but vectorized over time.
    """

    def __init__(self, json_path: str | Path):
        with open(json_path) as f:
            data = json.load(f)
        self.layers: list[tuple] = []
        for node in data["layers"]:
            ltype = node["type"]
            act = node.get("activation", "") or ""
            if ltype == "conv1d":
                kio = np.asarray(node["weights"][0], dtype=np.float64)  # [k][in][out]
                b = np.asarray(node["weights"][1], dtype=np.float64)
                k = int(node["kernel_size"][0])
                d = int(node["dilation"][0])
                assert kio.shape[0] == k, (kio.shape, k)
                self.layers.append(("conv1d", kio, b, k, d, act))
            elif ltype == "dense":
                w_io = np.asarray(node["weights"][0], dtype=np.float64)  # [in][out]
                b = np.asarray(node["weights"][1], dtype=np.float64)
                self.layers.append(("dense", w_io, b, act))
            elif ltype == "activation":
                self.layers.append(("activation", node.get("activation", "")))
            else:
                raise ValueError(f"unsupported layer type: {ltype}")
        self.in_size = int(data["in_shape"][-1])

    @property
    def receptive_field(self) -> int:
        rf = 1
        for layer in self.layers:
            if layer[0] == "conv1d":
                _, _kio, _b, k, d, _ = layer
                rf += (k - 1) * d
        return rf

    def forward_window(self, window: np.ndarray) -> np.ndarray:
        """window: [T, in_size] -> outputs of the LAST valid frame [out]."""
        h = np.asarray(window, dtype=np.float64).T          # [ch, T]
        for layer in self.layers:
            if layer[0] == "conv1d":
                _, kio, b, k, d, act = layer
                in_ch, t_len = h.shape
                t_out = t_len - (k - 1) * d
                if t_out <= 0:
                    raise ValueError(
                        f"window too short: {t_len} frames < receptive field")
                out_ch = kio.shape[2]
                y = np.broadcast_to(b[:, None], (out_ch, t_out)).copy()
                for kk in range(k):                          # w[kk]: [in][out]
                    y += kio[kk].T @ h[:, kk * d : kk * d + t_out]
                h = _activate(y, act)
            elif layer[0] == "dense":
                _, w_io, b, act = layer
                if h.ndim == 2:                              # apply per-frame
                    h = _activate(w_io.T @ h + b[:, None], act)
                else:
                    h = _activate(w_io.T @ h + b, act)
            else:                                            # bare activation
                h = _activate(h, layer[1])
        return h[:, -1] if h.ndim == 2 else h


def _self_check() -> int:
    here = Path(__file__).resolve().parent
    fx = here.parent / "Source" / "Tests" / "data" / "motore_v2_a3"
    model = RTNeuralNumpyModel(fx / "model.json")
    x = np.loadtxt(fx / "input.csv", delimiter=",")
    expected = np.loadtxt(fx / "expected.csv", delimiter=",")
    y = model.forward_window(x)
    delta = float(np.max(np.abs(y - expected)))
    print(f"A3 fixture self-check: max|delta| = {delta:.3e} "
          f"(rf={model.receptive_field}, in={model.in_size})")
    ok = delta < 1e-6
    print("SELF-CHECK", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_self_check())
