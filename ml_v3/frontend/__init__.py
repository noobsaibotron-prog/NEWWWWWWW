"""G1b canonical frontend lab (offline).

Tranche T1: FIR coefficient generator (§5) + V3FeatureFrame schema stub (§7).
No streaming resampler, dual-resolution FFT, training, or Ableton ship.
"""
from __future__ import annotations

from .feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    feature_frame_field_names,
    validate_feature_frame_stub,
)
from .resampler_coeffs import (
    ResamplerCoeffError,
    fir_lowpass_coefficients,
    group_delay_rational,
    resample_ratio,
)

__all__ = [
    "FEATURE_FRAME_SCHEMA",
    "FeatureFrameError",
    "ResamplerCoeffError",
    "feature_frame_field_names",
    "fir_lowpass_coefficients",
    "group_delay_rational",
    "resample_ratio",
    "validate_feature_frame_stub",
]
