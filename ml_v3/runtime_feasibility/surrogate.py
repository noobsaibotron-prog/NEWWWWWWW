"""Worst-case surrogate models with random weights. Lab-only.

These are *shape* probes, not architectures. Their weights are random and
their outputs meaningless; the only thing being measured is what a model of
this size costs per analysis frame. Sizing follows
:mod:`ml_v3.runtime_feasibility.envelope`, which derives the feature shape and
the cadence from frozen artifacts and marks the topologies as lab-only.

Two deliberate worst-case choices, both stated so a reader does not mistake
them for the intended design:

* the causal TCN recomputes its whole context window on every hop instead of
  caching per-layer state. A streaming implementation would cache, so real
  cost would be lower; measuring the recompute keeps the number an upper
  bound rather than an optimistic one.
* channels and layers sit above anything proposed, so a green result carries
  slack instead of sitting on the boundary.
"""
from __future__ import annotations

import torch
from torch import nn

from ml_v3.contracts.constants import GRID_BANDS
from ml_v3.frontend.feature_frame import BAND_VECTOR_FIELDS
from ml_v3.runtime_feasibility.envelope import (
    FEATURES_PER_FRAME,
    RuntimeEnvelope,
)

__all__ = [
    "SurrogateError",
    "build_surrogate",
    "count_parameters",
    "make_input",
]

_BAND_FIELDS = len(BAND_VECTOR_FIELDS)


class SurrogateError(ValueError):
    """Malformed surrogate request."""


class _CausalTCN(nn.Module):
    """Dilated causal stack over [batch, features, time].

    Causality is enforced by left-padding each convolution by
    ``(kernel-1)*dilation`` and discarding the trailing samples, so no output
    frame ever depends on a future frame. A non-causal model would leak
    lookahead into a real-time decision, which is the failure this shape is
    meant to price.
    """

    def __init__(self, envelope: RuntimeEnvelope) -> None:
        super().__init__()
        self.pads: list[int] = []
        layers: list[nn.Module] = []
        in_channels = FEATURES_PER_FRAME
        for index in range(envelope.layers):
            dilation = 2 ** index
            layers.append(nn.Conv1d(
                in_channels, envelope.channels, envelope.kernel,
                dilation=dilation))
            self.pads.append((envelope.kernel - 1) * dilation)
            in_channels = envelope.channels
        self.convs = nn.ModuleList(layers)
        self.head = nn.Conv1d(envelope.channels, envelope.outputs, 1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        for pad, conv in zip(self.pads, self.convs):
            x = torch.relu(conv(nn.functional.pad(x, (pad, 0))))
        return self.head(x)[..., -1]


class _SpectroTemporal(nn.Module):
    """2-D stack over [batch, band_fields, bands, time].

    Keeps the band axis structured instead of flattening it, which is the
    point of a spectro-temporal candidate: a 2-D kernel can see a shape across
    neighbouring bands that a flattened vector destroys.
    """

    def __init__(self, envelope: RuntimeEnvelope) -> None:
        super().__init__()
        layers: list[nn.Module] = []
        in_channels = _BAND_FIELDS
        for _ in range(envelope.layers):
            layers.append(nn.Conv2d(
                in_channels, envelope.channels, (envelope.kernel, envelope.kernel),
                padding=(envelope.kernel // 2, 0)))
            in_channels = envelope.channels
        self.convs = nn.ModuleList(layers)
        self.head = nn.Linear(envelope.channels, envelope.outputs)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        for conv in self.convs:
            x = torch.relu(conv(x))
        return self.head(x.mean(dim=(2, 3)))


class _BaselineMLP(nn.Module):
    """Minimal control. If even this is slow, the measurement is wrong."""

    def __init__(self, envelope: RuntimeEnvelope) -> None:
        super().__init__()
        blocks: list[nn.Module] = [
            nn.Linear(FEATURES_PER_FRAME, envelope.channels), nn.ReLU()]
        for _ in range(envelope.layers - 1):
            blocks += [nn.Linear(envelope.channels, envelope.channels), nn.ReLU()]
        blocks.append(nn.Linear(envelope.channels, envelope.outputs))
        self.net = nn.Sequential(*blocks)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def build_surrogate(envelope: RuntimeEnvelope, *, seed: int) -> nn.Module:
    """Deterministic random-weight surrogate for one envelope.

    ``seed`` makes the weights reproducible so a repeated benchmark measures
    the same graph, not a different random one.
    """
    if not isinstance(seed, int):
        raise SurrogateError("seed must be an int")
    torch.manual_seed(seed)
    if envelope.kind == "causal_tcn":
        model: nn.Module = _CausalTCN(envelope)
    elif envelope.kind == "spectro_temporal":
        model = _SpectroTemporal(envelope)
    elif envelope.kind == "baseline":
        model = _BaselineMLP(envelope)
    else:
        raise SurrogateError(f"unknown envelope kind {envelope.kind!r}")
    return model.eval()


def make_input(envelope: RuntimeEnvelope, *, seed: int) -> torch.Tensor:
    """Input tensor with the shape the frontend actually produces."""
    generator = torch.Generator().manual_seed(seed)
    if envelope.kind == "causal_tcn":
        shape = (1, FEATURES_PER_FRAME, envelope.context_frames)
    elif envelope.kind == "spectro_temporal":
        shape = (1, _BAND_FIELDS, GRID_BANDS, envelope.context_frames)
    elif envelope.kind == "baseline":
        shape = (1, FEATURES_PER_FRAME)
    else:
        raise SurrogateError(f"unknown envelope kind {envelope.kind!r}")
    return torch.randn(*shape, generator=generator)


def count_parameters(model: nn.Module) -> int:
    return sum(parameter.numel() for parameter in model.parameters())
