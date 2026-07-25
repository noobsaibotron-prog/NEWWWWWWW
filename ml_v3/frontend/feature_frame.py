"""V3FeatureFrame schema stub (§7) — G1b T1.

Defines the frozen surface and fail-closed structural validation.
Does not extract features, run FFT, or claim streaming≡offline.
"""
from __future__ import annotations

import math
from typing import Any, Mapping

from ml_v3.contracts.constants import (
    CANONICAL_SAMPLE_RATE,
    GRID_BANDS,
    SCHEMA_IDS,
)

__all__ = [
    "FEATURE_FRAME_SCHEMA",
    "FeatureFrameError",
    "BAND_VECTOR_FIELDS",
    "SCALAR_FLOAT_FIELDS",
    "BOOL_FIELDS",
    "INT_FIELDS",
    "feature_frame_field_names",
    "validate_feature_frame_stub",
]

FEATURE_FRAME_SCHEMA: str = SCHEMA_IDS["feature_frame"]

BAND_VECTOR_FIELDS: tuple[str, ...] = (
    "mid_psd_db",
    "side_psd_db",
    "mid_shape_db",
    "side_shape_db",
    "mid_prominence_db",
    "side_prominence_db",
    "mid_delta_db",
    "side_delta_db",
)

SCALAR_FLOAT_FIELDS: tuple[str, ...] = (
    "mid_level_dbfs",
    "side_level_dbfs",
)

BOOL_FIELDS: tuple[str, ...] = (
    "mid_valid",
    "side_valid",
    "valid",
)

INT_FIELDS: tuple[str, ...] = (
    "frame_end_sample",
    "frame_index",
    "source_time_num",
    "source_time_den",
    "canonical_sample_rate",
)


class FeatureFrameError(ValueError):
    """Fail-closed structural / finiteness error for a feature-frame stub."""


def feature_frame_field_names() -> tuple[str, ...]:
    """All top-level keys required by §7 (plus schema)."""
    return (
        "schema",
        *INT_FIELDS,
        *BAND_VECTOR_FIELDS,
        *SCALAR_FLOAT_FIELDS,
        *BOOL_FIELDS,
        "reason",
    )


def _require_finite_float(value: Any, path: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise FeatureFrameError(f"{path} must be a finite number")
    f = float(value)
    if not math.isfinite(f):
        raise FeatureFrameError(f"{path} is non-finite: {value!r}")
    return f


def validate_feature_frame_stub(frame: Mapping[str, Any]) -> dict[str, Any]:
    """Validate a dict against the §7 surface (structure + finiteness only).

    Returns a shallow copy of accepted fields. Does not compute features.
    """
    if not isinstance(frame, Mapping):
        raise FeatureFrameError("frame must be a mapping")

    required = set(feature_frame_field_names())
    keys = set(frame.keys())
    missing = sorted(required - keys)
    if missing:
        raise FeatureFrameError(f"missing fields: {missing}")
    extra = sorted(keys - required)
    if extra:
        raise FeatureFrameError(f"unknown fields: {extra}")

    out: dict[str, Any] = {}
    schema = frame["schema"]
    if schema != FEATURE_FRAME_SCHEMA:
        raise FeatureFrameError(
            f"schema must be {FEATURE_FRAME_SCHEMA!r}, got {schema!r}"
        )
    out["schema"] = schema

    for name in INT_FIELDS:
        value = frame[name]
        if isinstance(value, bool) or not isinstance(value, int):
            raise FeatureFrameError(f"{name} must be int")
        out[name] = value

    if out["canonical_sample_rate"] != CANONICAL_SAMPLE_RATE:
        raise FeatureFrameError(
            f"canonical_sample_rate must be {CANONICAL_SAMPLE_RATE}"
        )
    if out["source_time_den"] <= 0:
        raise FeatureFrameError("source_time_den must be > 0")
    if out["frame_end_sample"] < 0 or out["frame_index"] < 0:
        raise FeatureFrameError("frame_end_sample/frame_index must be >= 0")

    for name in BAND_VECTOR_FIELDS:
        vec = frame[name]
        if not isinstance(vec, (list, tuple)):
            raise FeatureFrameError(f"{name} must be a sequence of length {GRID_BANDS}")
        if len(vec) != GRID_BANDS:
            raise FeatureFrameError(
                f"{name} length {len(vec)} != {GRID_BANDS}"
            )
        out[name] = [_require_finite_float(v, f"{name}[{i}]") for i, v in enumerate(vec)]

    for name in SCALAR_FLOAT_FIELDS:
        out[name] = _require_finite_float(frame[name], name)

    for name in BOOL_FIELDS:
        value = frame[name]
        if not isinstance(value, bool):
            raise FeatureFrameError(f"{name} must be bool")
        out[name] = value

    reason = frame["reason"]
    if out["valid"]:
        if reason is not None:
            raise FeatureFrameError("reason must be null when valid is true")
        out["reason"] = None
    else:
        if not isinstance(reason, str) or not reason:
            raise FeatureFrameError(
                "reason must be a non-empty string when valid is false"
            )
        out["reason"] = reason

    # Mono contract reminder: side_valid false is allowed; do not invent policy.
    if out["valid"] != (out["mid_valid"] or out["side_valid"]):
        raise FeatureFrameError("valid must equal mid_valid OR side_valid")

    return out
