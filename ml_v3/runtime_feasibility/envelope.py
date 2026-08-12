"""Lab-only runtime envelopes for the future V3 model.

Everything here is either **derived** from a frozen artifact or **declared
lab-only**. The distinction is carried in the code, not in prose, because the
whole point of this harness is to avoid inventing a budget that later gets
mistaken for a signed one.

Derived from frozen artifacts, not chosen here:

* ``GRID_BANDS`` (120) — ``ml_v3.contracts.constants``;
* the per-frame feature shape — ``ml_v3.frontend.feature_frame``: eight band
  vectors plus two scalars, so 8*120+2 = 962 floats;
* ``HOP_SAMPLES`` (1024) and the canonical 48 kHz rate — the frozen metrology
  lock, giving one analysis every 1024/48000 s = 21.333 ms.

Declared lab-only, because no signed authority fixes them yet:

* the topologies. G4 of ``MOTORE_V3_PLAN.md`` names the bake-off candidates —
  a DSP baseline, a causal TCN, and a compact spectro-temporal model — and
  says the G4 contract will define "forme tensoriali, causalita, pooling e
  budget compute". That contract does not exist, so the sizes below are
  worst-case probes, not proposals;
* the amber/red margin thresholds, which stay diagnostic;
* the number of concurrent instances swept.

No ML backend is declared for V3 anywhere in the plan or the candidate
contract, and no target hardware is declared. Measurements therefore describe
the machine that ran them and prove nothing about a JUCE C++ runtime.
"""
from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction

from ml_v3.contracts.constants import GRID_BANDS
from ml_v3.contracts.metrology_lock import HOP_SAMPLES
from ml_v3.frontend.feature_frame import (
    BAND_VECTOR_FIELDS,
    SCALAR_FLOAT_FIELDS,
)

__all__ = [
    "ANALYSIS_HOP_SECONDS",
    "CANONICAL_SAMPLE_RATE",
    "FEATURES_PER_FRAME",
    "LAB_ENVELOPES",
    "MARGIN_AMBER",
    "MARGIN_RED",
    "RuntimeEnvelope",
    "analysis_hop_seconds",
    "margin_verdict",
]

CANONICAL_SAMPLE_RATE = 48_000

#: Floats the frontend hands to a model for one analysis frame.
FEATURES_PER_FRAME = len(BAND_VECTOR_FIELDS) * GRID_BANDS + len(
    SCALAR_FLOAT_FIELDS)

#: Exact seconds between two analysis frames, from the frozen hop.
ANALYSIS_HOP_SECONDS = Fraction(HOP_SAMPLES, CANONICAL_SAMPLE_RATE)

# Diagnostic only. The prompt that requested this harness supplies them as a
# starting traffic light and says explicitly that appearing there does not make
# them product norms. They stay diagnostic until an authority fixes a budget.
MARGIN_AMBER = Fraction(1, 2)
MARGIN_RED = Fraction(3, 4)


def analysis_hop_seconds(sample_rate: int = CANONICAL_SAMPLE_RATE) -> Fraction:
    """Exact hop interval at a given rate.

    The frontend canonicalizes to 48 kHz, so the model cadence does not change
    with the host rate; this exists to make that invariance testable rather
    than assumed.
    """
    if not isinstance(sample_rate, int) or sample_rate <= 0:
        raise ValueError("sample_rate must be a positive int")
    return Fraction(HOP_SAMPLES, sample_rate)


@dataclass(frozen=True)
class RuntimeEnvelope:
    """One worst-case shape to measure. Lab-only, never a proposal."""

    name: str
    kind: str
    context_frames: int
    channels: int
    layers: int
    kernel: int
    outputs: int

    def __post_init__(self) -> None:
        for field in ("context_frames", "channels", "layers", "kernel",
                      "outputs"):
            value = getattr(self, field)
            if not isinstance(value, int) or value <= 0:
                raise ValueError(f"{field} must be a positive int")
        if self.kind not in ("causal_tcn", "spectro_temporal", "baseline"):
            raise ValueError(f"unknown envelope kind {self.kind!r}")

    @property
    def receptive_field_frames(self) -> int:
        """Frames a causal stack of dilated layers can see.

        Dilations double per layer, the usual causal-TCN construction:
        1 + sum over layers of (kernel-1)*2**layer.
        """
        if self.kind != "causal_tcn":
            return self.context_frames
        return 1 + sum(
            (self.kernel - 1) * (2 ** layer) for layer in range(self.layers))

    @property
    def receptive_field_seconds(self) -> Fraction:
        return self.receptive_field_frames * ANALYSIS_HOP_SECONDS


#: Worst-case probes, not architectures. Sized deliberately above what anyone
#: has proposed so a green result carries slack rather than sitting on the
#: boundary.
LAB_ENVELOPES: tuple[RuntimeEnvelope, ...] = (
    RuntimeEnvelope("baseline_min", "baseline", 1, 16, 2, 3, 8),
    RuntimeEnvelope("tcn_small", "causal_tcn", 32, 32, 4, 3, 8),
    RuntimeEnvelope("tcn_worst", "causal_tcn", 64, 64, 6, 3, 8),
    RuntimeEnvelope("spectro_worst", "spectro_temporal", 64, 64, 4, 3, 8),
)


def margin_verdict(p99_seconds: float) -> str:
    """Diagnostic traffic light against the frozen hop.

    Returns ``green``/``amber``/``red``. This is a lab signal: no authority has
    fixed a budget, so a green here is evidence, not permission.
    """
    if p99_seconds < 0:
        raise ValueError("p99_seconds must be non-negative")
    used = Fraction(p99_seconds).limit_denominator(10 ** 12) / ANALYSIS_HOP_SECONDS
    if used <= MARGIN_AMBER:
        return "green"
    if used <= MARGIN_RED:
        return "amber"
    return "red"
