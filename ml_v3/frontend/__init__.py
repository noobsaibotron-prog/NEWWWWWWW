"""G1b canonical frontend lab (offline + streaming apply).

Tranche T1: FIR coefficient generator (§5) + V3FeatureFrame schema stub (§7).
Tranche T1b / spike WS1: causal polyphase streaming apply (§5) under P5.
Tranche T2 / spike WS2: offline §6+§7 feature path under P1–P7.
No training or Ableton ship.
"""
from __future__ import annotations

from .feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    feature_frame_field_names,
    validate_feature_frame_stub,
)
from .offline_features import (
    OfflineFeatureError,
    empty_support_report,
    extract_offline_feature_frames,
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
    "OfflineFeatureError",
    "ResamplerCoeffError",
    "ResamplerError",
    "empty_support_report",
    "extract_offline_feature_frames",
    "feature_frame_field_names",
    "fir_lowpass_coefficients",
    "group_delay_rational",
    "iter_host_chunks",
    "resample_chunked",
    "resample_offline",
    "resample_ratio",
    "validate_feature_frame_stub",
]
