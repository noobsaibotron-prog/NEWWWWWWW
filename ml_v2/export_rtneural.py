"""Motore v2 — RTNeural JSON export (recipe VERIFIED against the vendored parser).

IMPORTANT — this recipe was counter-verified against the vendored
`ThirdParty/RTNeural/RTNeural/model_loader.h` and CORRECTS the A3 design spec:

  * conv1d `weights[0]` nesting is **[kernel][in][out]** (keras layout).
    `loadConv1D` (model_loader.h:96-125) reads `layerWeights[i][j][k]` as
    i=kernel, j=in, k=out — with the tap flip `kernel_size-1-i` applied
    INTERNALLY by the loader, so the exporter must NOT flip. Since torch and
    keras share the cross-correlation orientation, torch `[out,in,k]` maps to
    JSON by a pure transpose (2,1,0).
  * Taps stay **COMPACT** (kernel_size entries). Dilation goes in the JSON
    `dilation` field and is handled by the layer's ring buffer; zero-expanding
    to kernel*dilation (the original spec recipe) makes the innermost axis
    longer than out_size and CRASHES the parser (std::out_of_range, measured).
  * dense `weights[0]` nesting is [in][out] → torch `[out,in]` transposed.

Receipts: upstream `models/conv.json` conv layer has weights shape (3, 8, 4) =
(k, in, out); measured spike — correct recipe parity 3.7e-07, spec recipe crash.
"""
from __future__ import annotations

import json

import numpy as np


def conv1d_layer_json(w_out_in_k: np.ndarray, bias: np.ndarray,
                      dilation: int, activation: str = "relu",
                      groups: int = 1) -> dict:
    """Torch-layout conv weights [out, in, kernel] -> RTNeural JSON layer."""
    out_ch, _in_ch, k = w_out_in_k.shape
    kio = np.transpose(w_out_in_k, (2, 1, 0))     # [kernel][in][out], compact
    return {
        "type": "conv1d",
        "activation": activation,
        "shape": [None, None, out_ch],
        "kernel_size": [k],
        "dilation": [dilation],
        "groups": groups,
        "weights": [kio.tolist(), np.asarray(bias).tolist()],
    }


def dense_layer_json(w_out_in: np.ndarray, bias: np.ndarray,
                     activation: str = "") -> dict:
    """Torch-layout dense weights [out, in] -> RTNeural JSON layer."""
    out_f = w_out_in.shape[0]
    return {
        "type": "dense",
        "activation": activation,
        "shape": [None, None, out_f],
        "weights": [np.asarray(w_out_in).T.tolist(), np.asarray(bias).tolist()],
    }


def build_a3_model_json(w: dict) -> dict:
    """A3 stack from a seed_weights()-style dict (see ml_v2.model)."""
    from .model import CONV_SPECS, MEL
    layers = []
    for idx, (_o, _i, _k, d) in enumerate(CONV_SPECS):
        layers.append(conv1d_layer_json(w[f"conv{idx}.w"], w[f"conv{idx}.b"],
                                        dilation=d, activation="relu"))
    layers.append(dense_layer_json(w["dense0.w"], w["dense0.b"], activation="relu"))
    layers.append(dense_layer_json(w["dense1.w"], w["dense1.b"], activation=""))
    return {"in_shape": [None, None, MEL], "layers": layers}


def export_torch_model(model, path: str) -> None:
    """Export a MotoreV2CNN (torch) to an RTNeural JSON file (A4+ path)."""
    import torch.nn as nn
    from .model import CONV_SPECS
    conv_layers = [m for m in model.convs if isinstance(m, nn.Conv1d)]
    dense_layers = [m for m in model.head if isinstance(m, nn.Linear)]
    w = {}
    for idx, conv in enumerate(conv_layers):
        w[f"conv{idx}.w"] = conv.weight.detach().cpu().numpy()
        w[f"conv{idx}.b"] = conv.bias.detach().cpu().numpy()
    for idx, lin in enumerate(dense_layers):
        w[f"dense{idx}.w"] = lin.weight.detach().cpu().numpy()
        w[f"dense{idx}.b"] = lin.bias.detach().cpu().numpy()
    assert len(conv_layers) == len(CONV_SPECS)
    with open(path, "w") as f:
        json.dump(build_a3_model_json(w), f)
