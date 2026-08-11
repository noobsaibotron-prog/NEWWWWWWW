"""Controlled, model-free semantic characterization of the frozen V3 frontend.

The probe uses synthetic periodic spectra only. It does not read/write G1
audio assets, train a model, run the evaluator, or alter frontend thresholds.
"""
from __future__ import annotations

import hashlib
import math
from pathlib import Path
from typing import Any, Callable

import numpy as np

from ml_v3.contracts.canonical import canonical_bytes
from ml_v3.contracts.constants import CANONICAL_SAMPLE_RATE
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.contracts.metrology_lock import HOP_SAMPLES
from ml_v3.benchmark.frontend_conformance import _require_gate_platform
from ml_v3.frontend.offline_features import (
    empty_support_report,
    extract_offline_feature_frames,
)

__all__ = ["RepresentationProbeError", "run_representation_probe"]

_SOURCE_COMMIT = "fdb8496556c1d3a8fe8d4b222608c71a30cd4644"
_FS = CANONICAL_SAMPLE_RATE
_BLOCK = HOP_SAMPLES
_BLOCKS = 192
_BASE_RMS = 0.05
_SEED = 20_260_811
_F_MIN = float(_FS) / float(_BLOCK)
_F_MAX = 20_000.0
_PEAK_CENTERS: tuple[float, ...] = (93.75, 250.0, 1_000.0, 4_000.0, 10_000.0)
_PEAK_GAINS: tuple[float, ...] = (3.0, 6.0, 12.0)
_PEAK_SIGMA = 1.0 / 12.0
_BROAD_CASES: tuple[tuple[float, float], ...] = (
    (150.0, 0.45),
    (300.0, 0.45),
    (1_000.0, 0.45),
    (4_000.0, 0.35),
)
_CORRELATION_FLOOR = 0.80
_TEMPORAL_START_HOP = 32
_TEMPORAL_DURATION_HOPS = 16
_TEMPORAL_CENTER = 1_000.0
_TEMPORAL_SIGMA = 0.45
_TEMPORAL_GAIN_DB = 6.0
_DELTA_SETTLED_DB = 0.05
_PROBE_SOURCE = Path(__file__).resolve()
_PROBE_TEST = (
    _PROBE_SOURCE.parents[1] / "tests" / "test_g1b_representation_probe.py"
)

Envelope = Callable[[np.ndarray], np.ndarray]


class RepresentationProbeError(ValueError):
    """Probe construction or frontend output is structurally invalid."""


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _gaussian_envelope(center_hz: float, sigma_oct: float, gain_db: float) -> Envelope:
    if center_hz <= 0.0 or sigma_oct <= 0.0 or not math.isfinite(gain_db):
        raise RepresentationProbeError("invalid gaussian envelope parameters")

    def apply(frequencies: np.ndarray) -> np.ndarray:
        f = np.asarray(frequencies, dtype=np.float64)
        out = np.zeros(f.shape, dtype=np.float64)
        positive = f > 0.0
        distance = np.log2(f[positive] / center_hz)
        out[positive] = gain_db * np.exp(
            -0.5 * (distance / sigma_oct) ** 2
        )
        return out

    return apply


def _low_shelf_envelope(frequencies: np.ndarray) -> np.ndarray:
    """-6 dB below 180 Hz; log-frequency linear transition to 0 at 360 Hz."""
    f = np.asarray(frequencies, dtype=np.float64)
    out = np.zeros(f.shape, dtype=np.float64)
    out[f <= 180.0] = -6.0
    transition = (f > 180.0) & (f < 360.0)
    u = np.log2(f[transition] / 180.0) / math.log2(360.0 / 180.0)
    out[transition] = -6.0 * (1.0 - u)
    return out


def _high_shelf_envelope(frequencies: np.ndarray) -> np.ndarray:
    """0 dB below 5 kHz; log-frequency linear transition to -6 dB at 10 kHz."""
    f = np.asarray(frequencies, dtype=np.float64)
    out = np.zeros(f.shape, dtype=np.float64)
    transition = (f > 5_000.0) & (f < 10_000.0)
    u = np.log2(f[transition] / 5_000.0) / math.log2(2.0)
    out[transition] = -6.0 * u
    out[f >= 10_000.0] = -6.0
    return out


def _base_spectrum() -> tuple[np.ndarray, np.ndarray]:
    """Return deterministic rFFT spectrum and its frequency grid."""
    frequencies = np.fft.rfftfreq(_BLOCK, 1.0 / _FS)
    active = (frequencies >= _F_MIN) & (frequencies <= _F_MAX)
    rng = np.random.Generator(np.random.PCG64(_SEED))
    phases = np.empty(int(np.count_nonzero(active)), dtype=np.float64)
    for index in range(phases.size):
        phases[index] = 2.0 * math.pi * float(rng.random())
    spectrum = np.zeros(frequencies.shape, dtype=np.complex128)
    spectrum[active] = np.exp(1j * phases)
    block = np.fft.irfft(spectrum, n=_BLOCK)
    rms = math.sqrt(float(np.mean(block * block)))
    if rms <= 0.0 or not math.isfinite(rms):
        raise RepresentationProbeError("base block has invalid RMS")
    spectrum *= _BASE_RMS / rms
    return spectrum, frequencies


def _block_from_envelope(
    spectrum: np.ndarray,
    frequencies: np.ndarray,
    envelope: Envelope | None,
) -> np.ndarray:
    transformed = spectrum.copy()
    if envelope is not None:
        envelope_db = np.asarray(envelope(frequencies), dtype=np.float64)
        if envelope_db.shape != frequencies.shape:
            raise RepresentationProbeError("envelope shape mismatch")
        if not np.all(np.isfinite(envelope_db)):
            raise RepresentationProbeError("non-finite envelope")
        transformed *= np.power(10.0, envelope_db / 20.0)
    block = np.fft.irfft(transformed, n=_BLOCK)
    if not np.all(np.isfinite(block)):
        raise RepresentationProbeError("non-finite IFFT block")
    peak = float(np.max(np.abs(block)))
    if peak >= 1.0:
        raise RepresentationProbeError(
            f"probe block reaches/clips full scale: peak={peak}"
        )
    return np.asarray(block, dtype=np.float32)


def _periodic_audio(block: np.ndarray, blocks: int = _BLOCKS) -> np.ndarray:
    if block.shape != (_BLOCK,):
        raise RepresentationProbeError("probe block must have one hop")
    if blocks <= 0:
        raise RepresentationProbeError("block count must be positive")
    return np.tile(block, int(blocks))


def _valid_mid_frames(audio: np.ndarray) -> list[dict[str, Any]]:
    frames = extract_offline_feature_frames(audio, _FS)
    if not frames:
        raise RepresentationProbeError("frontend emitted no frames")
    for frame in frames:
        if not bool(frame["mid_valid"]) or not bool(frame["valid"]):
            raise RepresentationProbeError(
                f"mid channel invalid at frame {frame['frame_index']}"
            )
    return frames


def _mean_field(frames: list[dict[str, Any]], field: str) -> np.ndarray:
    matrix = np.asarray([frame[field] for frame in frames], dtype=np.float64)
    if matrix.ndim != 2 or matrix.shape[1] != 120:
        raise RepresentationProbeError(f"invalid field matrix {field}: {matrix.shape}")
    if not np.all(np.isfinite(matrix)):
        raise RepresentationProbeError(f"non-finite field {field}")
    return np.mean(matrix, axis=0)


def _pearson(a: np.ndarray, b: np.ndarray) -> float:
    x = np.asarray(a, dtype=np.float64)
    y = np.asarray(b, dtype=np.float64)
    if x.shape != y.shape or x.ndim != 1:
        raise RepresentationProbeError("correlation vectors must match")
    if not np.all(np.isfinite(x)) or not np.all(np.isfinite(y)):
        raise RepresentationProbeError("correlation input non-finite")
    xc = x - float(np.mean(x))
    yc = y - float(np.mean(y))
    x_energy = float(np.dot(xc, xc))
    y_energy = float(np.dot(yc, yc))
    if x_energy <= 0.0 or y_energy <= 0.0:
        raise RepresentationProbeError("zero-variance correlation input")
    return float(np.dot(xc, yc) / math.sqrt(x_energy * y_energy))


def _rms(value: np.ndarray) -> float:
    array = np.asarray(value, dtype=np.float64)
    return math.sqrt(float(np.mean(array * array)))


def _target_band(center_hz: float, centers: np.ndarray) -> int:
    return int(np.argmin(np.abs(np.log2(centers / center_hz))))


def _stationary_features(
    spectrum: np.ndarray,
    frequencies: np.ndarray,
    envelope: Envelope | None,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, int, float]:
    block = _block_from_envelope(spectrum, frequencies, envelope)
    audio = _periodic_audio(block)
    frames = _valid_mid_frames(audio)
    return (
        _mean_field(frames, "mid_psd_db"),
        _mean_field(frames, "mid_shape_db"),
        _mean_field(frames, "mid_prominence_db"),
        _mean_field(frames, "mid_delta_db"),
        len(frames),
        float(np.max(np.abs(audio.astype(np.float64)))),
    )


def _run_narrow_peaks(
    spectrum: np.ndarray,
    frequencies: np.ndarray,
    centers: np.ndarray,
    base_shape: np.ndarray,
    base_prominence: np.ndarray,
) -> dict[str, Any]:
    cases: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    for center in _PEAK_CENTERS:
        target = _target_band(center, centers)
        gain_results: list[dict[str, Any]] = []
        target_responses: list[float] = []
        for gain_db in _PEAK_GAINS:
            _psd, shape, prominence, _delta, frame_count, peak = _stationary_features(
                spectrum,
                frequencies,
                _gaussian_envelope(center, _PEAK_SIGMA, gain_db),
            )
            response = prominence - base_prominence
            realized_envelope = _gaussian_envelope(
                center, _PEAK_SIGMA, gain_db
            )(frequencies)
            realized_bin = int(np.argmax(realized_envelope))
            realized_frequency = float(frequencies[realized_bin])
            realized_target = _target_band(realized_frequency, centers)
            target_response = float(response[realized_target])
            target_value = float(prominence[realized_target])
            argmax = int(np.argmax(response))
            localized = abs(argmax - realized_target) <= 2
            positive = target_response > 0.0
            if not positive:
                failures.append(
                    {
                        "reason": "PEAK_PROMINENCE_NOT_POSITIVE",
                        "center_hz": center,
                        "gain_db": gain_db,
                    }
                )
            if not localized:
                failures.append(
                    {
                        "reason": "PEAK_LOCALIZATION_FAILED",
                        "center_hz": center,
                        "gain_db": gain_db,
                        "target_band": realized_target,
                        "argmax_band": argmax,
                    }
                )
            target_responses.append(target_response)
            gain_results.append(
                {
                    "gain_db": gain_db,
                    "nominal_center_hz": center,
                    "realized_peak_bin_hz": realized_frequency,
                    "realized_target_band": realized_target,
                    "target_prominence_db": target_value,
                    "target_response_db": target_response,
                    "argmax_band": argmax,
                    "argmax_center_hz": float(centers[argmax]),
                    "distance_bands": abs(argmax - realized_target),
                    "positive": positive,
                    "localized": localized,
                    "shape_response_at_target_db": float(
                        shape[realized_target] - base_shape[realized_target]
                    ),
                    "frame_count": frame_count,
                    "audio_peak": peak,
                    "excluded_cells": [],
                }
            )
        monotonic = all(
            later > earlier
            for earlier, later in zip(target_responses, target_responses[1:])
        )
        if not monotonic:
            failures.append(
                {
                    "reason": "PEAK_RESPONSE_NOT_STRICTLY_MONOTONIC",
                    "center_hz": center,
                    "responses": target_responses,
                }
            )
        cases.append(
            {
                "center_hz": center,
                "sigma_octaves": _PEAK_SIGMA,
                "target_band": target,
                "target_band_center_hz": float(centers[target]),
                "gains": gain_results,
                "strictly_monotonic": monotonic,
                "verdict": (
                    "GREEN"
                    if monotonic
                    and all(g["positive"] and g["localized"] for g in gain_results)
                    else "RED"
                ),
            }
        )
    return {
        "cases": cases,
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def _run_broad_colorations(
    spectrum: np.ndarray,
    frequencies: np.ndarray,
    centers: np.ndarray,
    base_psd: np.ndarray,
    base_shape: np.ndarray,
    base_prominence: np.ndarray,
) -> dict[str, Any]:
    cases: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    for center, sigma in _BROAD_CASES:
        envelope = _gaussian_envelope(center, sigma, 6.0)
        psd, shape, prominence, _delta, frame_count, peak = _stationary_features(
            spectrum, frequencies, envelope
        )
        expected = envelope(centers)
        shape_response = shape - base_shape
        prominence_response = prominence - base_prominence
        domain = (
            (base_psd > -120.0)
            & (base_psd < 12.0)
            & (psd > -120.0)
            & (psd < 12.0)
        )
        excluded = np.flatnonzero(~domain).astype(int).tolist()
        try:
            correlation = _pearson(expected[domain], shape_response[domain])
        except RepresentationProbeError as exc:
            failures.append(
                {
                    "reason": "BROAD_CORRELATION_UNDEFINED",
                    "center_hz": center,
                    "detail": str(exc),
                }
            )
            correlation = None
        passed = correlation is not None and correlation >= _CORRELATION_FLOOR
        if not passed:
            failures.append(
                {
                    "reason": "BROAD_SHAPE_CORRELATION_FAILED",
                    "center_hz": center,
                    "correlation": correlation,
                }
            )
        ratio = _rms(prominence_response) / max(_rms(shape_response), 1e-300)
        cases.append(
            {
                "center_hz": center,
                "sigma_octaves": sigma,
                "gain_db": 6.0,
                "shape_envelope_pearson": correlation,
                "correlation_floor": _CORRELATION_FLOOR,
                "prominence_to_shape_rms_ratio_diagnostic": ratio,
                "low_end_diagnostic": center <= 300.0,
                "frame_count": frame_count,
                "audio_peak": peak,
                "eligible_cell_count": int(np.count_nonzero(domain)),
                "excluded_cells": excluded,
                "verdict": "GREEN" if passed else "RED",
            }
        )
    return {
        "cases": cases,
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def _run_shelves(
    spectrum: np.ndarray,
    frequencies: np.ndarray,
    centers: np.ndarray,
    base_psd: np.ndarray,
    base_shape: np.ndarray,
) -> dict[str, Any]:
    definitions: tuple[tuple[str, Envelope, np.ndarray], ...] = (
        ("thin_low_shelf", _low_shelf_envelope, centers <= 180.0),
        ("dull_high_shelf", _high_shelf_envelope, centers >= 10_000.0),
    )
    cases: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    for case_id, envelope, affected in definitions:
        psd, shape, _prominence, _delta, frame_count, peak = _stationary_features(
            spectrum, frequencies, envelope
        )
        expected = envelope(centers)
        response = shape - base_shape
        domain = (
            (base_psd > -120.0)
            & (base_psd < 12.0)
            & (psd > -120.0)
            & (psd < 12.0)
        )
        excluded = np.flatnonzero(~domain).astype(int).tolist()
        try:
            correlation = _pearson(expected[domain], response[domain])
        except RepresentationProbeError as exc:
            failures.append(
                {
                    "reason": "SHELF_CORRELATION_UNDEFINED",
                    "case_id": case_id,
                    "detail": str(exc),
                }
            )
            correlation = None
        affected_domain = affected & domain
        affected_values = response[affected_domain]
        sign_correct = bool(
            affected_values.size > 0 and np.all(affected_values < 0.0)
        )
        passed = (
            correlation is not None
            and correlation >= _CORRELATION_FLOOR
            and sign_correct
        )
        if correlation is None or correlation < _CORRELATION_FLOOR:
            failures.append(
                {
                    "reason": "SHELF_SHAPE_CORRELATION_FAILED",
                    "case_id": case_id,
                    "correlation": correlation,
                }
            )
        if not sign_correct:
            failures.append(
                {
                    "reason": "SHELF_SIGN_FAILED",
                    "case_id": case_id,
                    "max_affected_change_db": (
                        float(np.max(affected_values))
                        if affected_values.size
                        else None
                    ),
                }
            )
        cases.append(
            {
                "case_id": case_id,
                "shape_envelope_pearson": correlation,
                "correlation_floor": _CORRELATION_FLOOR,
                "sign_correct_all_affected_bands": sign_correct,
                "affected_band_count": int(np.count_nonzero(affected_domain)),
                "max_affected_change_db": (
                    float(np.max(affected_values))
                    if affected_values.size
                    else None
                ),
                "mean_affected_change_db": (
                    float(np.mean(affected_values))
                    if affected_values.size
                    else None
                ),
                "frame_count": frame_count,
                "audio_peak": peak,
                "eligible_cell_count": int(np.count_nonzero(domain)),
                "excluded_cells": excluded,
                "verdict": "GREEN" if passed else "RED",
            }
        )
    return {
        "cases": cases,
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def _first_settled_frame(
    frames: list[dict[str, Any]],
    *,
    not_before: int,
    deadline: int,
) -> int | None:
    candidates = [
        frame
        for frame in frames
        if not_before <= int(frame["frame_end_sample"]) <= deadline
    ]
    for start_index, frame in enumerate(candidates):
        if all(
            float(
                np.max(
                    np.abs(np.asarray(later["mid_delta_db"], dtype=np.float64))
                )
            )
            <= _DELTA_SETTLED_DB
            for later in candidates[start_index:]
        ):
            return int(frame["frame_end_sample"])
    return None


def _run_temporal(
    spectrum: np.ndarray,
    frequencies: np.ndarray,
    centers: np.ndarray,
) -> dict[str, Any]:
    base_block = _block_from_envelope(spectrum, frequencies, None)
    envelope = _gaussian_envelope(
        _TEMPORAL_CENTER, _TEMPORAL_SIGMA, _TEMPORAL_GAIN_DB
    )
    changed_block = _block_from_envelope(spectrum, frequencies, envelope)
    blocks = [base_block] * _BLOCKS
    for index in range(
        _TEMPORAL_START_HOP,
        _TEMPORAL_START_HOP + _TEMPORAL_DURATION_HOPS,
    ):
        blocks[index] = changed_block
    audio = np.concatenate(blocks)
    frames = _valid_mid_frames(audio)
    start_sample = _TEMPORAL_START_HOP * _BLOCK
    end_sample = (
        _TEMPORAL_START_HOP + _TEMPORAL_DURATION_HOPS
    ) * _BLOCK

    response_frames: list[tuple[int, float, np.ndarray]] = []
    for frame in frames:
        delta = np.asarray(frame["mid_delta_db"], dtype=np.float64)
        response_frames.append(
            (
                int(frame["frame_end_sample"]),
                float(np.max(np.abs(delta))),
                delta,
            )
        )

    early = [
        value
        for frame_end, value, _delta in response_frames
        if frame_end <= start_sample
    ]
    no_early = not early or max(early) <= _DELTA_SETTLED_DB
    onset_candidates = [
        item
        for item in response_frames
        if start_sample < item[0] <= start_sample + _BLOCK
        and item[1] > _DELTA_SETTLED_DB
    ]
    first_onset = onset_candidates[0] if onset_candidates else None

    expected = envelope(centers)
    if first_onset is None:
        onset_correlation = None
        onset_argmax = None
    else:
        onset_correlation = _pearson(expected, first_onset[2])
        onset_argmax = int(np.argmax(first_onset[2]))
    target = _target_band(_TEMPORAL_CENTER, centers)
    localized = onset_argmax is not None and abs(onset_argmax - target) <= 2

    onset_stable = start_sample + 8 * _BLOCK
    onset_deadline = onset_stable + 8 * _BLOCK
    onset_settled = _first_settled_frame(
        frames,
        not_before=onset_stable,
        deadline=min(onset_deadline, end_sample),
    )
    offset_stable = end_sample + 8 * _BLOCK
    offset_deadline = offset_stable + 8 * _BLOCK
    offset_settled = _first_settled_frame(
        frames,
        not_before=offset_stable,
        deadline=offset_deadline,
    )

    failures: list[dict[str, Any]] = []
    if not no_early:
        failures.append(
            {
                "reason": "DELTA_ANTICIPATED_RESPONSE",
                "max_early_delta_db": max(early),
            }
        )
    if first_onset is None:
        failures.append({"reason": "DELTA_ONSET_LATE_OR_MISSING"})
    if onset_correlation is None or onset_correlation < _CORRELATION_FLOOR:
        failures.append(
            {
                "reason": "DELTA_ENVELOPE_CORRELATION_FAILED",
                "correlation": onset_correlation,
            }
        )
    if not localized:
        failures.append(
            {
                "reason": "DELTA_LOCALIZATION_FAILED",
                "target_band": target,
                "argmax_band": onset_argmax,
            }
        )
    if onset_settled is None:
        failures.append({"reason": "DELTA_ONSET_DID_NOT_SETTLE"})
    if offset_settled is None:
        failures.append({"reason": "DELTA_OFFSET_DID_NOT_SETTLE"})

    return {
        "change_start_hop": _TEMPORAL_START_HOP,
        "change_duration_hops": _TEMPORAL_DURATION_HOPS,
        "change_start_sample": start_sample,
        "change_end_sample": end_sample,
        "envelope": {
            "kind": "gaussian",
            "center_hz": _TEMPORAL_CENTER,
            "sigma_octaves": _TEMPORAL_SIGMA,
            "gain_db": _TEMPORAL_GAIN_DB,
        },
        "no_anticipated_response": no_early,
        "first_response_frame_end_sample": (
            first_onset[0] if first_onset is not None else None
        ),
        "first_response_within_one_hop": first_onset is not None,
        "onset_envelope_pearson": onset_correlation,
        "onset_argmax_band": onset_argmax,
        "target_band": target,
        "localized_within_two_bands": localized,
        "onset_stabilization_sample": onset_stable,
        "onset_settled_frame_end_sample": onset_settled,
        "offset_stabilization_sample": offset_stable,
        "offset_settled_frame_end_sample": offset_settled,
        "settled_threshold_db": _DELTA_SETTLED_DB,
        "settle_deadline_hops_after_stabilization": 8,
        "frame_count": len(frames),
        "audio_peak": float(np.max(np.abs(audio.astype(np.float64)))),
        "excluded_cells": [],
        "failures": failures,
        "verdict": "GREEN" if not failures else "RED",
    }


def run_representation_probe() -> dict[str, Any]:
    """Run the canonical V3 representation sanity probe."""
    gate_platform = _require_gate_platform()
    spectrum, frequencies = _base_spectrum()
    centers = np.asarray(band_centers_hz(), dtype=np.float64)
    base_psd, base_shape, base_prominence, base_delta, frame_count, base_peak = (
        _stationary_features(spectrum, frequencies, None)
    )
    base_delta_max = float(np.max(np.abs(base_delta)))
    base_failures: list[dict[str, Any]] = []
    if base_delta_max > _DELTA_SETTLED_DB:
        base_failures.append(
            {
                "reason": "BASE_PERIODIC_DELTA_NONZERO",
                "max_abs_delta_db": base_delta_max,
            }
        )

    narrow = _run_narrow_peaks(
        spectrum, frequencies, centers, base_shape, base_prominence
    )
    broad = _run_broad_colorations(
        spectrum,
        frequencies,
        centers,
        base_psd,
        base_shape,
        base_prominence,
    )
    shelves = _run_shelves(
        spectrum, frequencies, centers, base_psd, base_shape
    )
    temporal = _run_temporal(spectrum, frequencies, centers)
    sections = {
        "base": {
            "frame_count": frame_count,
            "audio_peak": base_peak,
            "max_abs_mean_delta_db": base_delta_max,
            "failures": base_failures,
            "excluded_cells": np.flatnonzero(
                (base_psd <= -120.0) | (base_psd >= 12.0)
            ).astype(int).tolist(),
            "verdict": "GREEN" if not base_failures else "RED",
        },
        "narrow_peaks": narrow,
        "broad_colorations": broad,
        "thin_dull_primitives": shelves,
        "temporal": temporal,
    }
    verdict = (
        "GREEN"
        if all(section["verdict"] == "GREEN" for section in sections.values())
        else "RED"
    )
    support_geometry = empty_support_report()
    structural_empty = list(support_geometry["lf_empty_indices"])
    base_excluded = list(sections["base"]["excluded_cells"])
    inactive_only_for_probe = sorted(set(base_excluded) - set(structural_empty))
    return {
        "schema": "aieq-v3-representation-probe-1",
        "report_id": "V3_REPRESENTATION_PROBE_V1",
        "source_commit": _SOURCE_COMMIT,
        "source_commit_semantics": (
            "committed Gate-5-7 harness parent; exact probe/test bytes are "
            "bound by source_files sha256 in this same atomic evidence commit"
        ),
        "source_files": {
            "probe": {
                "path": "ml_v3/benchmark/representation_probe.py",
                "sha256": _sha256_file(_PROBE_SOURCE),
            },
            "test": {
                "path": "ml_v3/tests/test_g1b_representation_probe.py",
                "sha256": _sha256_file(_PROBE_TEST),
            },
        },
        "environment": gate_platform,
        "parameters": {
            "sample_rate": _FS,
            "block_samples": _BLOCK,
            "block_count": _BLOCKS,
            "base_rms": _BASE_RMS,
            "rfft_bin_min_hz": _F_MIN,
            "rfft_bin_max_hz": _F_MAX,
            "phase_prng": "PCG64",
            "phase_seed": _SEED,
            "transform_domain": "rfft_magnitude_before_ifft",
            "observable_shape_domain": (
                "reference and transformed PSD both strictly inside "
                "(-120,+12) dB; exclusions published"
            ),
            "narrow_localization_reference": (
                "canonical band nearest the realized maximum-gain rFFT bin; "
                "nominal center also published"
            ),
            "prominence_positive_definition": (
                "transformed minus matched-baseline prominence > 0"
            ),
            "peak_centers_hz": list(_PEAK_CENTERS),
            "peak_gains_db": list(_PEAK_GAINS),
            "peak_sigma_octaves": _PEAK_SIGMA,
            "broad_cases_center_sigma": [
                [center, sigma] for center, sigma in _BROAD_CASES
            ],
            "correlation_floor": _CORRELATION_FLOOR,
            "low_shelf": {
                "gain_db": -6.0,
                "flat_below_hz": 180.0,
                "transition_end_hz": 360.0,
                "transition": "linear_in_log2_frequency",
            },
            "high_shelf": {
                "gain_db": -6.0,
                "transition_start_hz": 5_000.0,
                "flat_above_hz": 10_000.0,
                "transition": "linear_in_log2_frequency",
            },
        },
        "results": sections,
        "diagnostics": {
            "low_end_support_debt": {
                "status": "RECORDED_NON_CLOSING_DEBT",
                "closing": False,
                "affects_probe_verdict": False,
                "structurally_unobservable_band_indices": structural_empty,
                "probe_inactive_nonstructural_band_indices": inactive_only_for_probe,
                "base_excluded_band_indices": base_excluded,
                "support_geometry": support_geometry,
                "interpretation": (
                    "LF-empty bands have no triangular FFT support in the "
                    "frozen frontend and cannot be characterized by this "
                    "probe; remaining exclusions are inactive for the "
                    "specific deterministic comb stimulus. This debt is "
                    "published but does not close or relax a gate."
                ),
            }
        },
        "verdict": verdict,
        "scope": {
            "lab_only": True,
            "models_used": False,
            "optimizer_used": False,
            "checkpoint_used": False,
            "g1_assets_modified": False,
            "frontend_modified": False,
            "diagnostic_not_gate": [
                "broad_prominence_to_shape_rms_ratio",
                "low_end_quality",
            ],
            "training_authorized": False,
            "pre_evidence_countercheck": {
                "performed": True,
                "invalid_draft_evidence_committed": False,
                "corrections": [
                    "localization bound to realized 1024-rFFT stimulus bin",
                    "prominence positivity defined relative to matched baseline",
                    "shape correlation/sign restricted to observable PSD cells",
                    "temporal settling requires permanence through deadline",
                ],
                "thresholds_changed": False,
                "frontend_changed": False,
                "fixtures_changed": False,
            },
        },
    }


def _canonical_probe_bytes() -> bytes:
    """Internal deterministic serialization used by tests/evidence tooling."""
    return canonical_bytes(run_representation_probe())
