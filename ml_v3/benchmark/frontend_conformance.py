"""Lab-only G1 frontend conformance harness for Gates 5, 6 and 7.

Authority for Gate 5:
docs/REV8_GATE5_GAIN_DOMAIN_MICRO_AMEND_BALLOT.md.

This module is deliberately outside ml_v3.contracts. It measures the frozen
frontend and official G1 fixtures; it does not alter feature extraction,
fixture generation, thresholds, the plugin, or any model/training path.
"""
from __future__ import annotations

import math
import platform
import subprocess
from collections import Counter
from pathlib import Path
from typing import Any

import numpy as np

from ml_v3.contracts.constants import CANONICAL_SAMPLE_RATE, GRID_BANDS
from ml_v3.contracts.fixture_spec import duration_seconds, frozen_fixture_spec
from ml_v3.contracts.metrology_lock import (
    ANTI_ALIAS_MAX_DB_RE_TONE,
    GAIN_INVARIANCE_MAX_ABS_DB,
    MS_MID_EQUIVALENCE_MAX_ABS_DB,
    PSD_CLAMP_DB,
    coda_seconds,
    frozen_metrology_lock,
    gate_platform_python_label,
    require_gate_platform_python,
    resampler_group_delay_rational,
    warm_up_seconds,
)
from ml_v3.fixtures.g1.render_signals import (
    render_multitone,
    render_pseudo_noise,
    render_stereo_decorrelated,
    render_stereo_mid_only,
    render_stereo_side_only,
    render_ultrasonic_96k,
)
from ml_v3.frontend.offline_features import (
    PROMINENCE_HALF,
    extract_offline_feature_frames,
)
from ml_v3.frontend.resampler import resample_offline

__all__ = ["FrontendConformanceError", "run_frontend_conformance"]

_REPO_ROOT = Path(__file__).resolve().parents[2]
_BALLOT = "docs/REV8_GATE5_GAIN_DOMAIN_MICRO_AMEND_BALLOT.md"
_BASE_ATTENUATION_DB = -1.0
_GAIN_VARIANTS_DB: tuple[float, ...] = (-12.0, -6.0, 6.0, 12.0)
_MIN_PSD_SHAPE_DELTA = 108
_MIN_PROMINENCE = 64
_ANTI_ALIAS_N = 65_536
_ANTI_ALIAS_ZERO_PAD = 16
_GAIN_FIXTURE_FS = 48_000

_VECTOR_SUFFIXES: tuple[str, ...] = (
    "psd_db",
    "shape_db",
    "prominence_db",
    "delta_db",
)


class FrontendConformanceError(ValueError):
    """The harness input or implementation violates the signed gate."""


def _git_head() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=_REPO_ROOT,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "UNKNOWN"


def _require_gate_platform() -> dict[str, str]:
    """Fail closed unless the complete gate-platform lock matches."""
    try:
        require_gate_platform_python()
    except ValueError as exc:
        raise FrontendConformanceError(str(exc)) from exc
    expected = frozen_metrology_lock()["bit_identity"]["gate_platform"]
    actual = {
        "os": platform.system().lower(),
        "arch": platform.machine(),
        "python": gate_platform_python_label(),
        "numpy": np.__version__,
    }
    mismatches = {
        key: {"expected": str(expected[key]), "actual": actual[key]}
        for key in actual
        if actual[key] != str(expected[key])
    }
    if mismatches:
        raise FrontendConformanceError(
            f"gate platform mismatch: {mismatches}"
        )
    return actual


def _gain_linear(db: float) -> float:
    return 10.0 ** (float(db) / 20.0)


def _strict_threshold(value: float, threshold: float) -> bool:
    """Gate convention: exact boundary passes, immediately above fails."""
    return bool(math.isfinite(value) and value <= threshold)


def _alias_threshold(value_db: float) -> bool:
    """Anti-alias convention: exact -80 dB passes; a larger value fails."""
    return bool(math.isfinite(value_db) and value_db <= ANTI_ALIAS_MAX_DB_RE_TONE)


def _reflect_index(index: int, length: int = GRID_BANDS) -> int:
    if length <= 1:
        raise FrontendConformanceError("reflect length must be > 1")
    j = int(index)
    while j < 0 or j >= length:
        if j < 0:
            j = -j
        else:
            j = 2 * length - 2 - j
    return j


def _finite_vector(value: Any, *, path: str) -> np.ndarray:
    arr = np.asarray(value, dtype=np.float64)
    if arr.shape != (GRID_BANDS,):
        raise FrontendConformanceError(
            f"{path} shape {arr.shape} != ({GRID_BANDS},)"
        )
    if not np.all(np.isfinite(arr)):
        raise FrontendConformanceError(f"{path} contains NaN/Inf")
    return arr


def _eligible_psd_mask(
    psd_reference: np.ndarray,
    psd_variant: np.ndarray,
) -> tuple[np.ndarray, Counter[str]]:
    """Return the only Gate-5 mask permitted by the signed clamp domain."""
    ref = _finite_vector(psd_reference, path="psd_reference")
    var = _finite_vector(psd_variant, path="psd_variant")
    lo, hi = PSD_CLAMP_DB
    lower = (ref <= lo) | (var <= lo)
    upper = (~lower) & ((ref >= hi) | (var >= hi))
    eligible = (~lower) & (~upper)
    reasons: Counter[str] = Counter()
    reasons["LOWER_CLAMP"] = int(np.count_nonzero(lower))
    reasons["UPPER_CLAMP"] = int(np.count_nonzero(upper))
    return eligible, reasons


def _prominence_eligible_mask(
    psd_eligible: np.ndarray,
    *,
    kernel_half: int = PROMINENCE_HALF,
) -> np.ndarray:
    """33-cell reflect dependency closure; abbreviated kernels are forbidden."""
    if kernel_half != PROMINENCE_HALF or (2 * kernel_half + 1) != 33:
        raise FrontendConformanceError(
            "prominence eligibility must use the complete 33-cell "
            "reflect kernel"
        )
    mask = np.asarray(psd_eligible, dtype=bool)
    if mask.shape != (GRID_BANDS,):
        raise FrontendConformanceError("PSD eligibility mask must have 120 cells")
    out = np.ones(GRID_BANDS, dtype=bool)
    for i in range(GRID_BANDS):
        for offset in range(-kernel_half, kernel_half + 1):
            if not bool(mask[_reflect_index(i + offset)]):
                out[i] = False
                break
    return out


def _support_failures(
    *,
    psd_count: int,
    shape_count: int,
    delta_count: int,
    prominence_count: int,
) -> list[str]:
    failures: list[str] = []
    if (
        psd_count < _MIN_PSD_SHAPE_DELTA
        or shape_count < _MIN_PSD_SHAPE_DELTA
        or delta_count < _MIN_PSD_SHAPE_DELTA
        or prominence_count < _MIN_PROMINENCE
    ):
        failures.append("INSUFFICIENT_ELIGIBLE_SUPPORT")
    return failures


def _max_cell(
    errors: np.ndarray,
    eligible: np.ndarray,
) -> dict[str, int | float | None]:
    indices = np.flatnonzero(eligible)
    if indices.size == 0:
        return {"max_abs_db": None, "band_index": None}
    local = int(np.argmax(errors[indices]))
    band = int(indices[local])
    return {"max_abs_db": float(errors[band]), "band_index": band}


def _full_grid_diagnostic(
    reference: dict[str, Any],
    variant: dict[str, Any],
    channel: str,
    gain_db: float,
) -> dict[str, Any]:
    fields: dict[str, Any] = {}
    for suffix in _VECTOR_SUFFIXES:
        name = f"{channel}_{suffix}"
        ref = _finite_vector(reference[name], path=f"reference.{name}")
        var = _finite_vector(variant[name], path=f"variant.{name}")
        expected = gain_db if suffix == "psd_db" else 0.0
        errors = np.abs((var - ref) - expected)
        peak = int(np.argmax(errors))
        fields[name] = {
            "max_abs_db": float(errors[peak]),
            "band_index": peak,
        }
    level_name = f"{channel}_level_dbfs"
    fields[level_name] = {
        "max_abs_db": abs(
            (float(variant[level_name]) - float(reference[level_name])) - gain_db
        ),
        "band_index": None,
    }
    return {
        "closing": False,
        "reason": (
            "diagnostic only: clamped cells make full-grid gain translation "
            "unobservable and cannot constitute PASS"
        ),
        "fields": fields,
    }


def _frame_key(frame: dict[str, Any]) -> tuple[int, int]:
    return int(frame["frame_index"]), int(frame["frame_end_sample"])


def _channel_valid(frame: dict[str, Any], channel: str) -> bool:
    return bool(frame[f"{channel}_valid"])


def _frame_sequence_failures(
    frames: list[dict[str, Any]],
    *,
    label: str,
) -> list[dict[str, Any]]:
    failures: list[dict[str, Any]] = []
    if not frames:
        return [{"reason": "EMPTY_FRAME_SEQUENCE", "sequence": label}]
    seen: set[tuple[int, int]] = set()
    previous: tuple[int, int] | None = None
    for position, frame in enumerate(frames):
        key = _frame_key(frame)
        if key in seen:
            failures.append(
                {
                    "reason": "DUPLICATE_FRAME",
                    "sequence": label,
                    "position": position,
                    "frame_key": list(key),
                }
            )
        seen.add(key)
        if previous is not None and not (
            key[0] == previous[0] + 1 and key[1] > previous[1]
        ):
            failures.append(
                {
                    "reason": "FRAME_ORDER_INVALID",
                    "sequence": label,
                    "position": position,
                    "previous_key": list(previous),
                    "frame_key": list(key),
                }
            )
        previous = key
    return failures


def _source_time_seconds(frame: dict[str, Any]) -> float:
    denominator = int(frame["source_time_den"])
    if denominator <= 0:
        raise FrontendConformanceError("source_time_den must be positive")
    return float(frame["source_time_num"]) / float(denominator)


def _useful_frame_keys(
    frames: list[dict[str, Any]],
    *,
    sample_rate: int,
) -> set[tuple[int, int]]:
    """Closing source-time domain for one non-cross-SR fixture."""
    start = warm_up_seconds(sample_rate)
    end = duration_seconds() - coda_seconds()
    if end <= start:
        raise FrontendConformanceError("empty useful source-time interval")
    keys = {
        _frame_key(frame)
        for frame in frames
        if start <= _source_time_seconds(frame) <= end
    }
    if not keys:
        raise FrontendConformanceError("no frames in useful source-time interval")
    return keys


def _evaluate_gain_case(
    *,
    fixture_id: str,
    reference_frames: list[dict[str, Any]],
    variant_frames: list[dict[str, Any]],
    gain_db: float,
    transformed_peak: float,
    closing_frame_keys: set[tuple[int, int]] | None = None,
    required_channels: tuple[str, ...] = ("mid",),
) -> dict[str, Any]:
    failures: list[dict[str, Any]] = []
    reason_totals: Counter[str] = Counter()
    frame_results: list[dict[str, Any]] = []
    maxima: dict[str, dict[str, Any]] = {}

    failures.extend(
        _frame_sequence_failures(reference_frames, label="reference")
    )
    failures.extend(_frame_sequence_failures(variant_frames, label="variant"))

    if not math.isfinite(transformed_peak) or transformed_peak >= 1.0:
        failures.append(
            {
                "reason": "CLIPPING_OR_FULL_SCALE",
                "transformed_peak": transformed_peak,
            }
        )
    if len(reference_frames) != len(variant_frames):
        failures.append(
            {
                "reason": "FRAME_ALIGNMENT_MISMATCH",
                "reference_count": len(reference_frames),
                "variant_count": len(variant_frames),
            }
        )

    reference_keys = {_frame_key(frame) for frame in reference_frames}
    variant_keys = {_frame_key(frame) for frame in variant_frames}
    closing_keys = (
        reference_keys.copy()
        if closing_frame_keys is None
        else set(closing_frame_keys)
    )
    if not closing_keys or not closing_keys.issubset(reference_keys & variant_keys):
        failures.append(
            {
                "reason": "USEFUL_FRAME_SET_MISMATCH",
                "closing_count": len(closing_keys),
                "reference_count": len(reference_keys),
                "variant_count": len(variant_keys),
            }
        )
    if any(channel not in {"mid", "side"} for channel in required_channels):
        raise FrontendConformanceError("unknown required logical channel")
    supported_closing_frames: Counter[str] = Counter()

    previous_masks: dict[str, np.ndarray | None] = {"mid": None, "side": None}
    for pair_index, (reference, variant) in enumerate(
        zip(reference_frames, variant_frames, strict=False)
    ):
        if _frame_key(reference) != _frame_key(variant):
            failures.append(
                {
                    "reason": "FRAME_ALIGNMENT_MISMATCH",
                    "pair_index": pair_index,
                    "reference_key": list(_frame_key(reference)),
                    "variant_key": list(_frame_key(variant)),
                }
            )
            continue

        closing = _frame_key(reference) in closing_keys

        for channel in ("mid", "side"):
            ref_valid = _channel_valid(reference, channel)
            var_valid = _channel_valid(variant, channel)
            if ref_valid != var_valid:
                if not closing:
                    previous_masks[channel] = None
                    continue
                failures.append(
                    {
                        "reason": "VALIDITY_MISMATCH",
                        "frame_index": int(reference["frame_index"]),
                        "channel": channel,
                    }
                )
                previous_masks[channel] = None
                continue
            if not ref_valid:
                if closing:
                    reason_totals["CHANNEL_INVALID"] += GRID_BANDS
                previous_masks[channel] = None
                continue

            try:
                psd_ref = _finite_vector(
                    reference[f"{channel}_psd_db"],
                    path=f"reference.{channel}_psd_db",
                )
                psd_var = _finite_vector(
                    variant[f"{channel}_psd_db"],
                    path=f"variant.{channel}_psd_db",
                )
                shape_ref = _finite_vector(
                    reference[f"{channel}_shape_db"],
                    path=f"reference.{channel}_shape_db",
                )
                shape_var = _finite_vector(
                    variant[f"{channel}_shape_db"],
                    path=f"variant.{channel}_shape_db",
                )
                prom_ref = _finite_vector(
                    reference[f"{channel}_prominence_db"],
                    path=f"reference.{channel}_prominence_db",
                )
                prom_var = _finite_vector(
                    variant[f"{channel}_prominence_db"],
                    path=f"variant.{channel}_prominence_db",
                )
                delta_ref = _finite_vector(
                    reference[f"{channel}_delta_db"],
                    path=f"reference.{channel}_delta_db",
                )
                delta_var = _finite_vector(
                    variant[f"{channel}_delta_db"],
                    path=f"variant.{channel}_delta_db",
                )
                level_ref = float(reference[f"{channel}_level_dbfs"])
                level_var = float(variant[f"{channel}_level_dbfs"])
                if not math.isfinite(level_ref) or not math.isfinite(level_var):
                    raise FrontendConformanceError("non-finite level")
            except FrontendConformanceError as exc:
                if closing:
                    failures.append(
                        {
                            "reason": "NONFINITE_VALUE",
                            "frame_index": int(reference["frame_index"]),
                            "channel": channel,
                            "detail": str(exc),
                        }
                    )
                previous_masks[channel] = None
                continue

            psd_mask, exclusion_reasons = _eligible_psd_mask(psd_ref, psd_var)
            prom_mask = _prominence_eligible_mask(psd_mask)
            if closing:
                reason_totals.update(exclusion_reasons)
                reason_totals["PROMINENCE_NEIGHBOR_INELIGIBLE"] += int(
                    GRID_BANDS - np.count_nonzero(prom_mask)
                )

            previous = previous_masks[channel]
            if previous is None:
                delta_mask = psd_mask.copy()
                if closing and (
                    np.any(delta_ref != 0.0) or np.any(delta_var != 0.0)
                ):
                    failures.append(
                        {
                            "reason": "FIRST_VALID_DELTA_NONZERO",
                            "frame_index": int(reference["frame_index"]),
                            "channel": channel,
                        }
                    )
            else:
                delta_mask = psd_mask & previous
                if closing:
                    reason_totals["DELTA_HISTORY_INELIGIBLE"] += int(
                        GRID_BANDS - np.count_nonzero(delta_mask)
                    )
            previous_masks[channel] = psd_mask.copy()

            if not closing:
                continue

            supported_closing_frames[channel] += 1

            counts = {
                "psd": int(np.count_nonzero(psd_mask)),
                "shape": int(np.count_nonzero(psd_mask)),
                "prominence": int(np.count_nonzero(prom_mask)),
                "delta": int(np.count_nonzero(delta_mask)),
            }
            local_support_failures = _support_failures(
                psd_count=counts["psd"],
                shape_count=counts["shape"],
                delta_count=counts["delta"],
                prominence_count=counts["prominence"],
            )
            for reason in local_support_failures:
                failures.append(
                    {
                        "reason": reason,
                        "frame_index": int(reference["frame_index"]),
                        "channel": channel,
                        "counts": counts,
                    }
                )
                reason_totals[reason] += 1

            field_results = {
                "psd_db": _max_cell(np.abs((psd_var - psd_ref) - gain_db), psd_mask),
                "shape_db": _max_cell(np.abs(shape_var - shape_ref), psd_mask),
                "prominence_db": _max_cell(
                    np.abs(prom_var - prom_ref), prom_mask
                ),
                "delta_db": _max_cell(np.abs(delta_var - delta_ref), delta_mask),
                "level_dbfs": {
                    "max_abs_db": abs((level_var - level_ref) - gain_db),
                    "band_index": None,
                },
            }
            for field_name, field_result in field_results.items():
                value = field_result["max_abs_db"]
                if value is None or not _strict_threshold(
                    float(value), GAIN_INVARIANCE_MAX_ABS_DB
                ):
                    failures.append(
                        {
                            "reason": "GAIN_THRESHOLD_EXCEEDED",
                            "frame_index": int(reference["frame_index"]),
                            "channel": channel,
                            "field": field_name,
                            "max_abs_db": value,
                        }
                    )
                global_key = f"{channel}_{field_name}"
                current = maxima.get(global_key)
                if value is not None and (
                    current is None or float(value) > float(current["max_abs_db"])
                ):
                    maxima[global_key] = {
                        "max_abs_db": float(value),
                        "frame_index": int(reference["frame_index"]),
                        "frame_end_sample": int(reference["frame_end_sample"]),
                        "band_index": field_result["band_index"],
                    }

            frame_results.append(
                {
                    "frame_index": int(reference["frame_index"]),
                    "frame_end_sample": int(reference["frame_end_sample"]),
                    "channel": channel,
                    "counts": counts,
                    "fields": field_results,
                    "full_grid_diagnostic": _full_grid_diagnostic(
                        reference, variant, channel, gain_db
                    ),
                }
            )

    for channel in required_channels:
        if supported_closing_frames[channel] != len(closing_keys):
            failures.append(
                {
                    "reason": "REQUIRED_CHANNEL_INVALID",
                    "channel": channel,
                    "supported_closing_frames": supported_closing_frames[channel],
                    "required_closing_frames": len(closing_keys),
                }
            )

    return {
        "fixture_id": fixture_id,
        "gain_db": gain_db,
        "transformed_peak": transformed_peak,
        "threshold_db": GAIN_INVARIANCE_MAX_ABS_DB,
        "support_floor": {
            "psd_shape_delta": _MIN_PSD_SHAPE_DELTA,
            "prominence": _MIN_PROMINENCE,
        },
        "closing_frame_count": len(closing_keys),
        "reason_counts": dict(sorted(reason_totals.items())),
        "maxima": maxima,
        "frame_results": frame_results,
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def _run_gate5() -> dict[str, Any]:
    fixtures = (
        ("pseudo_noise", render_pseudo_noise(_GAIN_FIXTURE_FS), ("mid",)),
        (
            "decorrelated_stereo",
            render_stereo_decorrelated(_GAIN_FIXTURE_FS),
            ("mid", "side"),
        ),
    )
    cases: list[dict[str, Any]] = []
    base_scale = _gain_linear(_BASE_ATTENUATION_DB)
    for fixture_id, original, required_channels in fixtures:
        reference_audio = np.asarray(
            np.asarray(original, dtype=np.float64) * base_scale,
            dtype=np.float32,
        )
        reference_frames = extract_offline_feature_frames(
            reference_audio, _GAIN_FIXTURE_FS
        )
        closing_keys = _useful_frame_keys(
            reference_frames, sample_rate=_GAIN_FIXTURE_FS
        )
        for gain_db in _GAIN_VARIANTS_DB:
            variant_audio = np.asarray(
                np.asarray(reference_audio, dtype=np.float64)
                * _gain_linear(gain_db),
                dtype=np.float32,
            )
            peak = float(np.max(np.abs(np.asarray(variant_audio, dtype=np.float64))))
            variant_frames = extract_offline_feature_frames(
                variant_audio, _GAIN_FIXTURE_FS
            )
            cases.append(
                _evaluate_gain_case(
                    fixture_id=fixture_id,
                    reference_frames=reference_frames,
                    variant_frames=variant_frames,
                    gain_db=gain_db,
                    transformed_peak=peak,
                    closing_frame_keys=closing_keys,
                    required_channels=required_channels,
                )
            )
    return {
        "gate_id": 5,
        "name": "gain_invariance_eligible_domain",
        "fixture_sample_rate": _GAIN_FIXTURE_FS,
        "base_attenuation_db": _BASE_ATTENUATION_DB,
        "gain_variants_db": list(_GAIN_VARIANTS_DB),
        "cases": cases,
        "verdict": "GREEN" if all(c["verdict"] == "GREEN" for c in cases) else "RED",
    }


def _same_timestamps(
    reference: list[dict[str, Any]],
    candidate: list[dict[str, Any]],
) -> bool:
    if len(reference) != len(candidate):
        return False
    keys = (
        "frame_index",
        "frame_end_sample",
        "source_time_num",
        "source_time_den",
        "canonical_sample_rate",
    )
    return all(
        all(a[key] == b[key] for key in keys)
        for a, b in zip(reference, candidate, strict=True)
    )


def _channel_equivalence(
    reference: list[dict[str, Any]],
    candidate: list[dict[str, Any]],
    *,
    reference_channel: str,
    candidate_channel: str,
) -> dict[str, Any]:
    maxima: dict[str, float] = {}
    failures = _frame_sequence_failures(reference, label="reference")
    failures.extend(_frame_sequence_failures(candidate, label="candidate"))
    if failures:
        return {"maxima": maxima, "failures": failures, "verdict": "RED"}
    if not _same_timestamps(reference, candidate):
        failures.append({"reason": "FRAME_ALIGNMENT_MISMATCH"})
        return {"maxima": maxima, "failures": failures, "verdict": "RED"}

    for a, b in zip(reference, candidate, strict=True):
        if not (
            bool(a["valid"])
            and bool(b["valid"])
            and _channel_valid(a, reference_channel)
            and _channel_valid(b, candidate_channel)
        ):
            failures.append(
                {
                    "reason": "VALIDITY_MISMATCH",
                    "frame_index": int(a["frame_index"]),
                    "reference_channel": reference_channel,
                    "candidate_channel": candidate_channel,
                }
            )
            continue
        for suffix in _VECTOR_SUFFIXES:
            av = _finite_vector(
                a[f"{reference_channel}_{suffix}"],
                path=f"reference.{reference_channel}_{suffix}",
            )
            bv = _finite_vector(
                b[f"{candidate_channel}_{suffix}"],
                path=f"candidate.{candidate_channel}_{suffix}",
            )
            value = float(np.max(np.abs(bv - av)))
            key = f"{candidate_channel}_{suffix}"
            maxima[key] = max(maxima.get(key, 0.0), value)
            if not _strict_threshold(value, MS_MID_EQUIVALENCE_MAX_ABS_DB):
                failures.append(
                    {
                        "reason": "MS_THRESHOLD_EXCEEDED",
                        "frame_index": int(a["frame_index"]),
                        "field": key,
                        "max_abs_db": value,
                    }
                )
        level_error = abs(
            float(b[f"{candidate_channel}_level_dbfs"])
            - float(a[f"{reference_channel}_level_dbfs"])
        )
        level_key = f"{candidate_channel}_level_dbfs"
        maxima[level_key] = max(maxima.get(level_key, 0.0), level_error)
        if not _strict_threshold(level_error, MS_MID_EQUIVALENCE_MAX_ABS_DB):
            failures.append(
                {
                    "reason": "MS_THRESHOLD_EXCEEDED",
                    "frame_index": int(a["frame_index"]),
                    "field": level_key,
                    "max_abs_db": level_error,
                }
            )
    return {
        "maxima": maxima,
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def _mono_reference_validity_failures(
    frames: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    failures: list[dict[str, Any]] = []
    for frame in frames:
        if not (
            bool(frame["mid_valid"])
            and not bool(frame["side_valid"])
            and bool(frame["valid"])
        ):
            failures.append(
                {
                    "reason": "VALIDITY_MISMATCH",
                    "frame_index": int(frame["frame_index"]),
                    "expected": "mono mid valid, side invalid, frame valid",
                }
            )
    return failures


def _run_gate6() -> dict[str, Any]:
    fs = CANONICAL_SAMPLE_RATE
    mono = extract_offline_feature_frames(render_multitone(fs), fs)
    dual_mono = extract_offline_feature_frames(render_stereo_mid_only(fs), fs)
    side_only = extract_offline_feature_frames(render_stereo_side_only(fs), fs)
    decorrelated = extract_offline_feature_frames(
        render_stereo_decorrelated(fs), fs
    )
    mono_keys = _useful_frame_keys(mono, sample_rate=fs)
    dual_keys = _useful_frame_keys(dual_mono, sample_rate=fs)
    side_keys = _useful_frame_keys(side_only, sample_rate=fs)
    decorrelated_keys = _useful_frame_keys(decorrelated, sample_rate=fs)
    mono = [f for f in mono if _frame_key(f) in mono_keys]
    dual_mono = [
        f
        for f in dual_mono
        if _frame_key(f) in dual_keys
    ]
    side_only = [
        f
        for f in side_only
        if _frame_key(f) in side_keys
    ]
    decorrelated = [
        f
        for f in decorrelated
        if _frame_key(f) in decorrelated_keys
    ]
    mono_validity_failures = _mono_reference_validity_failures(mono)

    dual = _channel_equivalence(
        mono,
        dual_mono,
        reference_channel="mid",
        candidate_channel="mid",
    )
    dual["failures"].extend(mono_validity_failures)
    for frame in dual_mono:
        if (
            not bool(frame["mid_valid"])
            or bool(frame["side_valid"])
            or not bool(frame["valid"])
        ):
            dual["failures"].append(
                {
                    "reason": "VALIDITY_MISMATCH",
                    "frame_index": int(frame["frame_index"]),
                }
            )
    dual["verdict"] = "GREEN" if not dual["failures"] else "RED"

    side = _channel_equivalence(
        mono,
        side_only,
        reference_channel="mid",
        candidate_channel="side",
    )
    side["failures"].extend(mono_validity_failures)
    for frame in side_only:
        if (
            bool(frame["mid_valid"])
            or not bool(frame["side_valid"])
            or not bool(frame["valid"])
        ):
            side["failures"].append(
                {
                    "reason": "VALIDITY_MISMATCH",
                    "frame_index": int(frame["frame_index"]),
                }
            )
    side["verdict"] = "GREEN" if not side["failures"] else "RED"

    decorrelated_failures: list[dict[str, Any]] = []
    decorrelated_failures.extend(
        _frame_sequence_failures(decorrelated, label="decorrelated")
    )
    for frame in decorrelated:
        if not (
            bool(frame["mid_valid"])
            and bool(frame["side_valid"])
            and bool(frame["valid"])
        ):
            decorrelated_failures.append(
                {
                    "reason": "VALIDITY_MISMATCH",
                    "frame_index": int(frame["frame_index"]),
                }
            )
    decorrelated_case = {
        "frame_count": len(decorrelated),
        "failures": decorrelated_failures,
        "verdict": "GREEN" if not decorrelated_failures else "RED",
    }
    cases = {
        "mono_vs_dual_mono": dual,
        "side_only_vs_mono": side,
        "decorrelated_stereo": decorrelated_case,
    }
    return {
        "gate_id": 6,
        "name": "mid_side",
        "sample_rate": fs,
        "threshold_db": MS_MID_EQUIVALENCE_MAX_ABS_DB,
        "cases": cases,
        "verdict": (
            "GREEN"
            if all(case["verdict"] == "GREEN" for case in cases.values())
            else "RED"
        ),
    }


def _periodic_hann(length: int) -> np.ndarray:
    index = np.arange(length, dtype=np.float64)
    return 0.5 - 0.5 * np.cos(2.0 * math.pi * index / float(length))


def _hann_scalloping_compensation_db(length: int, zero_pad: int) -> float:
    """Worst residual loss at half a zero-padded FFT bin."""
    if length <= 0 or zero_pad <= 0:
        raise FrontendConformanceError("invalid Hann/zero-pad length")
    window = _periodic_hann(length)
    offset_original_bins = 0.5 / float(zero_pad)
    index = np.arange(length, dtype=np.float64)
    phasor = np.exp(
        -2j * math.pi * offset_original_bins * index / float(length)
    )
    response = abs(np.dot(window, phasor)) / float(np.sum(window))
    if response <= 0.0 or response > 1.0 or not math.isfinite(response):
        raise FrontendConformanceError("invalid Hann scalloping response")
    return -20.0 * math.log10(response)


def _measure_hann_peak(
    segment: np.ndarray,
    *,
    sample_rate: int,
    reference_amplitude: float,
    zero_pad: int = _ANTI_ALIAS_ZERO_PAD,
) -> dict[str, float | int]:
    """Conservative in-band peak estimate with bounded scalloping loss."""
    x = np.asarray(segment, dtype=np.float64)
    if x.shape != (_ANTI_ALIAS_N,) or not np.all(np.isfinite(x)):
        raise FrontendConformanceError("anti-alias segment invalid")
    if reference_amplitude <= 0.0 or not math.isfinite(reference_amplitude):
        raise FrontendConformanceError("reference amplitude invalid")
    window = _periodic_hann(_ANTI_ALIAS_N)
    fft_length = _ANTI_ALIAS_N * int(zero_pad)
    spectrum = np.fft.rfft(x * window, n=fft_length)
    amplitude = 2.0 * np.abs(spectrum) / float(np.sum(window))
    frequencies = np.fft.rfftfreq(fft_length, 1.0 / float(sample_rate))
    active = (frequencies >= 20.0) & (frequencies <= 20_000.0)
    if not np.any(active):
        raise FrontendConformanceError("anti-alias FFT has no in-band bins")
    active_indices = np.flatnonzero(active)
    peak_index = int(active_indices[int(np.argmax(amplitude[active_indices]))])
    peak_amplitude = float(amplitude[peak_index])
    peak_frequency = float(frequencies[peak_index])

    # The in-band interval is closed. Evaluate both exact endpoints so the
    # nearest sampled frequency remains at most half a zero-padded bin away,
    # including the nominal 28 kHz -> 20 kHz alias boundary.
    sample_index = np.arange(_ANTI_ALIAS_N, dtype=np.float64)
    for boundary_hz in (20.0, 20_000.0):
        phasor = np.exp(
            -2j * math.pi * boundary_hz * sample_index / float(sample_rate)
        )
        boundary_amplitude = float(
            2.0 * abs(np.dot(x * window, phasor)) / float(np.sum(window))
        )
        if boundary_amplitude > peak_amplitude:
            peak_amplitude = boundary_amplitude
            peak_frequency = boundary_hz
    raw_db = (
        -999.0
        if peak_amplitude <= 0.0
        else 20.0 * math.log10(peak_amplitude / reference_amplitude)
    )
    compensation = _hann_scalloping_compensation_db(_ANTI_ALIAS_N, zero_pad)
    return {
        "fft_samples": fft_length,
        "peak_frequency_hz": peak_frequency,
        "peak_amplitude": peak_amplitude,
        "raw_peak_db": raw_db,
        "scalloping_compensation_db": compensation,
        "conservative_peak_db": raw_db + compensation,
    }


def _run_gate7() -> dict[str, Any]:
    fs_in = 96_000
    source = render_ultrasonic_96k(fs_in)
    resampled = resample_offline(source, fs_in)
    delay_num, delay_den, _ = resampler_group_delay_rational(fs_in)
    delay_seconds = float(delay_num) / float(delay_den)
    start = int(
        math.ceil(
            (warm_up_seconds(fs_in) + delay_seconds)
            * CANONICAL_SAMPLE_RATE
        )
    )
    end = start + _ANTI_ALIAS_N
    source_duration_s = float(source.shape[0]) / float(fs_in)
    useful_end = int(
        math.floor(
            (source_duration_s - coda_seconds() + delay_seconds)
            * CANONICAL_SAMPLE_RATE
        )
    )
    failures: list[dict[str, Any]] = []
    if end > useful_end or end > int(resampled.shape[0]):
        failures.append(
            {
                "reason": "INSUFFICIENT_USEFUL_SAMPLES",
                "start_sample": start,
                "end_sample": end,
                "useful_end_sample": useful_end,
                "resampled_length": int(resampled.shape[0]),
            }
        )
        return {
            "gate_id": 7,
            "name": "anti_alias",
            "failures": failures,
            "verdict": "RED",
        }

    segment = np.asarray(resampled[start:end], dtype=np.float64)
    input_tone_amplitude = float(
        frozen_fixture_spec()["categories"]["ultrasonic_96k"][
            "amplitude_peak_each"
        ]
    )
    if input_tone_amplitude <= 0.0 or not math.isfinite(input_tone_amplitude):
        raise FrontendConformanceError("invalid official ultrasonic amplitude")
    peak = _measure_hann_peak(
        segment,
        sample_rate=CANONICAL_SAMPLE_RATE,
        reference_amplitude=input_tone_amplitude,
    )
    peak_db = float(peak["conservative_peak_db"])
    if not _alias_threshold(peak_db):
        failures.append(
            {
                "reason": "ANTI_ALIAS_THRESHOLD_EXCEEDED",
                "peak_db_re_single_input_tone": peak_db,
            }
        )
    return {
        "gate_id": 7,
        "name": "anti_alias",
        "input_sample_rate": fs_in,
        "canonical_sample_rate": CANONICAL_SAMPLE_RATE,
        "window": "periodic_hann",
        "analysis_samples": _ANTI_ALIAS_N,
        "zero_pad_factor": _ANTI_ALIAS_ZERO_PAD,
        "fft_samples": int(peak["fft_samples"]),
        "useful_start_sample": start,
        "useful_end_sample_exclusive": end,
        "useful_start_source_time_seconds": (
            float(start) / float(CANONICAL_SAMPLE_RATE) - delay_seconds
        ),
        "input_single_tone_amplitude": input_tone_amplitude,
        "peak_frequency_hz": float(peak["peak_frequency_hz"]),
        "peak_amplitude": float(peak["peak_amplitude"]),
        "raw_peak_db_re_single_input_tone": float(peak["raw_peak_db"]),
        "scalloping_compensation_db": float(
            peak["scalloping_compensation_db"]
        ),
        "peak_db_re_single_input_tone": peak_db,
        "threshold_db": ANTI_ALIAS_MAX_DB_RE_TONE,
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def run_frontend_conformance() -> dict[str, Any]:
    """Run closing numerical checks for frontend Gates 5, 6 and 7.

    The returned dictionary is JSON-safe. RED is a scientific result, not an
    exception. Structural harness violations raise FrontendConformanceError.
    """
    gate_platform = _require_gate_platform()
    gate5 = _run_gate5()
    gate6 = _run_gate6()
    gate7 = _run_gate7()
    gates = {"gate5": gate5, "gate6": gate6, "gate7": gate7}
    overall = (
        "GREEN"
        if all(gate["verdict"] == "GREEN" for gate in gates.values())
        else "RED"
    )
    return {
        "schema": "aieq-v3-frontend-conformance-1",
        "authority_ballot": _BALLOT,
        "source_commit": _git_head(),
        "environment": gate_platform,
        "gates": gates,
        "verdict": overall,
        "scope": {
            "lab_only": True,
            "contracts_exported": False,
            "frontend_modified": False,
            "fixtures_modified": False,
            "thresholds_modified": False,
            "training_authorized": False,
        },
    }
