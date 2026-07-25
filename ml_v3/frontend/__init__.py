"""G1b canonical frontend lab (offline + streaming apply).

Tranche T1: FIR coefficient generator (§5) + V3FeatureFrame schema stub (§7).
Tranche T1b / spike WS1: causal polyphase streaming apply (§5) under P5.
No dual-resolution FFT, training, or Ableton ship.
"""
from __future__ import annotations

from .feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    feature_frame_field_names,
    validate_feature_frame_stub,
)
from .resampler import (
    CausalPolyphaseResampler,
    ResamplerError,
    iter_host_chunks,
    resample_chunked,
    resample_offline,
)
from .resampler_coeffs import (
    ResamplerCoeffError,
    fir_lowpass_coefficients,
    group_delay_rational,
    resample_ratio,
)

__all__ = [
    "FEATURE_FRAME_SCHEMA",
    "CausalPolyphaseResampler",
    "FeatureFrameError",
    "ResamplerCoeffError",
    "ResamplerError",
    "feature_frame_field_names",
    "fir_lowpass_coefficients",
    "group_delay_rational",
    "iter_host_chunks",
    "resample_chunked",
    "resample_offline",
    "resample_ratio",
    "validate_feature_frame_stub",
]
