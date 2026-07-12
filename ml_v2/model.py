"""Motore v2 — A3 architecture (Manus design, 2026-07-10).

3x dilated causal Conv1D (valid padding) + 2x Dense, concat head [17] =
[8 classes | 1 presence | 8 freq] as RAW logits (per-segment sigmoid lives in
the C++ consumer). ~50.5k params, receptive field 17 frames (~390 ms @ 23 ms hop).

The torch class is the A4 training vehicle. For A3 parity the fixture generator
loads seed-fixed numpy weights into it, so the committed fixture is reproducible
bit-for-bit regardless of torch's own RNG.
"""
from __future__ import annotations

import numpy as np

try:
    import torch
    import torch.nn as nn
    HAS_TORCH = True
except ImportError:          # fixture generation falls back to the numpy reference
    HAS_TORCH = False

MEL = 64          # log-mel bins per frame (feature A2)
T_WINDOW = 32     # frames per analysis window (~750 ms @ 23 ms hop)
OUT = 17          # [8 classes | 1 presence | 8 freq]

# (out_ch, in_ch, kernel, dilation) per conv stage — receptive field 5+4+8 = 17
CONV_SPECS = [(64, MEL, 5, 1), (64, 64, 3, 2), (64, 64, 3, 4)]
DENSE_SPECS = [(64, 64), (OUT, 64)]


if HAS_TORCH:
    class MotoreV2CNN(nn.Module):
        """A3 CNN. Input [batch, MEL, T] -> output [batch, OUT, T_valid].

        padding='valid' everywhere: RTNeural streams sample-by-sample causally,
        and the parity contract compares torch's LAST valid frame with
        RTNeural's output after T forward() calls.

        dtype: float64 by default (bit-reproducible A3 fixture); the A4
        trainer constructs with float32 (MPS has no float64 support).
        """

        def __init__(self, dtype: "torch.dtype" = None) -> None:
            super().__init__()
            dtype = dtype or torch.float64
            convs = []
            for out_ch, in_ch, k, d in CONV_SPECS:
                convs.append(nn.Conv1d(in_ch, out_ch, k, dilation=d,
                                       padding=0, dtype=dtype))
                convs.append(nn.ReLU())
            self.convs = nn.Sequential(*convs)
            self.head = nn.Sequential(
                nn.Linear(DENSE_SPECS[0][1], DENSE_SPECS[0][0], dtype=dtype),
                nn.ReLU(),
                nn.Linear(DENSE_SPECS[1][1], DENSE_SPECS[1][0], dtype=dtype),
            )

        def forward(self, x: "torch.Tensor") -> "torch.Tensor":
            h = self.convs(x)                     # [B, 64, T_valid]
            h = h.transpose(1, 2)                 # [B, T_valid, 64]
            return self.head(h).transpose(1, 2)   # [B, OUT, T_valid]


def seed_weights(seed: int) -> dict:
    """Deterministic numpy weights in TORCH layout (conv [out,in,k], dense [out,in])."""
    rng = np.random.default_rng(seed)
    w = {}
    for idx, (o, i, k, _d) in enumerate(CONV_SPECS):
        w[f"conv{idx}.w"] = (rng.standard_normal((o, i, k)) / np.sqrt(i * k))
        w[f"conv{idx}.b"] = rng.standard_normal(o) * 0.05
    for idx, (o, i) in enumerate(DENSE_SPECS):
        w[f"dense{idx}.w"] = (rng.standard_normal((o, i)) / np.sqrt(i))
        w[f"dense{idx}.b"] = rng.standard_normal(o) * 0.05
    return w


def load_seed_weights_into_torch(model: "MotoreV2CNN", w: dict) -> None:
    conv_layers = [m for m in model.convs if isinstance(m, nn.Conv1d)]
    dense_layers = [m for m in model.head if isinstance(m, nn.Linear)]
    with torch.no_grad():
        for idx, conv in enumerate(conv_layers):
            conv.weight.copy_(torch.from_numpy(w[f"conv{idx}.w"]))
            conv.bias.copy_(torch.from_numpy(w[f"conv{idx}.b"]))
        for idx, lin in enumerate(dense_layers):
            lin.weight.copy_(torch.from_numpy(w[f"dense{idx}.w"]))
            lin.bias.copy_(torch.from_numpy(w[f"dense{idx}.b"]))


def numpy_forward_last(w: dict, x_tm: np.ndarray) -> np.ndarray:
    """Independent numpy reference (torch semantics: cross-correlation, valid,
    dilated). x_tm: [T, MEL] -> [OUT] logits of the LAST valid frame."""
    h = x_tm.T                                             # [ch, T]
    for idx, (_o, _i, k, d) in enumerate(CONV_SPECS):
        wt, b = w[f"conv{idx}.w"], w[f"conv{idx}.b"]
        t_out = h.shape[1] - (k - 1) * d
        y = np.empty((wt.shape[0], t_out))
        for m in range(t_out):
            acc = b.copy()
            for kk in range(k):
                acc = acc + wt[:, :, kk] @ h[:, m + kk * d]
            y[:, m] = acc
        h = np.maximum(y, 0.0)
    last = h[:, -1]
    z = np.maximum(w["dense0.w"] @ last + w["dense0.b"], 0.0)
    return w["dense1.w"] @ z + w["dense1.b"]
