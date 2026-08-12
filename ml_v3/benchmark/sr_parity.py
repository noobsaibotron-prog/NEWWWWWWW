"""Gate-4 sample-rate parity harness (G1b spike WS4).

REV6 / lock packaging:
  - adversarial assets: multitone, pseudo_noise, log_sweep
  - rates: 44.1 / 48 (ref) / 96
  - useful window: common warm-up∩coda intersection
  - alignment: nearest source_time (tie: frame_index, frame_end_sample)
  - activity: max(psd_ref, psd_sr) > -120; shape/prominence inherit
  - invalid_channel_vectors_ignored
  - aggregator: max |Δ|; threshold 0.25 dB; empty active set → FAIL
  - sweep: checkpoint frames only
  - inputs: T6 render_* float32 arrays (no WAV loader)

Margin packaging (spike evidence; ≠ CONTRACT amend):
  GREEN requires max|Δ| ≤ 0.25 **and** margin ≥ MARGIN_FLOOR_DB (0.05).
  Pass under threshold but margin < floor → AMBRA.
  max|Δ| > 0.25 → RED (feasibility).

Does not mutate hashed G1a artifacts. Does not claim G1 PASS.
"""
from __future__ import annotations

import platform
import sys
from dataclasses import asdict, dataclass, field
from typing import Any, Callable, Iterable, Literal

import numpy as np

from ml_v3.contracts.constants import CANONICAL_SAMPLE_RATE
from ml_v3.contracts.fixture_spec import (
    assert_sweep_checkpoints_reachable,
    common_useful_window,
    sweep_crossing_time,
)
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.contracts.metrology_lock import (
    ACTIVITY_FLOOR_DB,
    GATE_PLATFORM_PYTHON,
    SR_PARITY_MAX_ABS_DB,
    SWEEP_CHECKPOINT_HZ,
    frozen_metrology_lock,
    gate_platform_python_label,
    require_gate_platform_python,
)
from ml_v3.fixtures.g1.render_signals import (
    render_log_sweep,
    render_multitone,
    render_pseudo_noise,
)
from ml_v3.frontend.offline_features import (
    empty_support_report,
    extract_offline_feature_frames,
)

__all__ = [
    "MARGIN_FLOOR_DB",
    "SPIKE_ADVERSARIAL_ASSETS",
    "SRParityError",
    "PeakCell",
    "CellResult",
    "MatrixResult",
    "source_time_seconds",
    "filter_useful_frames",
    "nearest_frame",
    "verdict_from_max_abs",
    "evaluate_sr_parity_cell",
    "run_spike_gate4_matrix",
    "require_gate_platform_full",
]

# A priori spike packaging (plan §4 example: 0.24 dB → AMBRA).
MARGIN_FLOOR_DB: float = 0.05

REF_HZ: int = CANONICAL_SAMPLE_RATE
COMPARE_HZ: tuple[int, ...] = (44100, 96000)

SPIKE_ADVERSARIAL_ASSETS: tuple[str, ...] = (
    "multitone",
    "pseudo_noise",
    "log_sweep",
)

_RENDERERS: dict[str, Callable[[int], np.ndarray]] = {
    "multitone": render_multitone,
    "pseudo_noise": render_pseudo_noise,
    "log_sweep": render_log_sweep,
}

_BAND_FIELDS: tuple[str, ...] = (
    "mid_psd_db",
    "side_psd_db",
    "mid_shape_db",
    "side_shape_db",
    "mid_prominence_db",
    "side_prominence_db",
)
_SCALAR_FIELDS: tuple[str, ...] = (
    "mid_level_dbfs",
    "side_level_dbfs",
)
_CHANNEL_OF = {
    "mid_psd_db": "mid",
    "side_psd_db": "side",
    "mid_shape_db": "mid",
    "side_shape_db": "side",
    "mid_prominence_db": "mid",
    "side_prominence_db": "side",
    "mid_level_dbfs": "mid",
    "side_level_dbfs": "side",
}
_PSD_OF = {"mid": "mid_psd_db", "side": "side_psd_db"}
_VALID_OF = {"mid": "mid_valid", "side": "side_valid"}

Verdict = Literal["GREEN", "AMBRA", "RED", "INCONCLUSIVE"]


class SRParityError(ValueError):
    """Fail-closed harness error (wrong platform / empty useful / packaging)."""


@dataclass(frozen=True)
class PeakCell:
    field: str
    band: int
    hz: float | None
    abs_delta_db: float
    psd_ref: float | None
    psd_sr: float | None
    t_ref: float
    t_sr: float
    p2_main_single_bin: bool
    p2_lf_single_bin: bool
    meta: dict[str, Any] = field(default_factory=dict)


@dataclass
class CellResult:
    asset: str
    fs_sr: int
    fs_ref: int
    mode: str
    max_abs_db: float
    margin_db: float
    verdict: Verdict
    n_pairs_compared: int
    n_active_cells: int
    n_skipped_invalid_channel: int
    n_skipped_empty_activity: int
    peak: PeakCell | None
    p2_watch_hit: bool
    notes: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        d = asdict(self)
        return d


@dataclass
class MatrixResult:
    platform: dict[str, str]
    threshold_db: float
    margin_floor_db: float
    useful_window: tuple[float, float]
    cells: list[CellResult]
    overall_max_abs_db: float
    overall_verdict: Verdict
    p2_support: dict[str, Any]
    pin_necessity: dict[str, Any]
    notes: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            "platform": self.platform,
            "threshold_db": self.threshold_db,
            "margin_floor_db": self.margin_floor_db,
            "useful_window": list(self.useful_window),
            "cells": [c.to_dict() for c in self.cells],
            "overall_max_abs_db": self.overall_max_abs_db,
            "overall_verdict": self.overall_verdict,
            "p2_support": self.p2_support,
            "pin_necessity": self.pin_necessity,
            "notes": list(self.notes),
        }


def source_time_seconds(frame: dict[str, Any]) -> float:
    return float(frame["source_time_num"]) / float(frame["source_time_den"])


def filter_useful_frames(
    frames: Iterable[dict[str, Any]],
    *,
    window: tuple[float, float] | None = None,
) -> list[dict[str, Any]]:
    start, end = common_useful_window() if window is None else window
    if end <= start:
        raise SRParityError(
            f"empty useful window: start={start} end={end} → FAIL "
            "(empty_useful_segment_is_fail)"
        )
    out = [f for f in frames if start <= source_time_seconds(f) <= end]
    if not out:
        raise SRParityError(
            "no frames inside common useful window → FAIL "
            "(empty_useful_segment_is_fail)"
        )
    return out


def nearest_frame(
    frames: list[dict[str, Any]],
    t: float,
) -> dict[str, Any]:
    """Nearest source_time; tie-break smaller frame_index, then frame_end_sample."""
    if not frames:
        raise SRParityError("nearest_frame on empty list")
    best = frames[0]
    best_d = abs(source_time_seconds(best) - t)
    for f in frames[1:]:
        d = abs(source_time_seconds(f) - t)
        if d < best_d - 1e-18:
            best, best_d = f, d
        elif abs(d - best_d) <= 1e-18:
            if (
                f["frame_index"] < best["frame_index"]
                or (
                    f["frame_index"] == best["frame_index"]
                    and f["frame_end_sample"] < best["frame_end_sample"]
                )
            ):
                best, best_d = f, d
    return best


def verdict_from_max_abs(
    max_abs_db: float,
    *,
    threshold_db: float = SR_PARITY_MAX_ABS_DB,
    margin_floor_db: float = MARGIN_FLOOR_DB,
) -> Verdict:
    if not np.isfinite(max_abs_db):
        return "INCONCLUSIVE"
    margin = threshold_db - max_abs_db
    if max_abs_db > threshold_db:
        return "RED"
    if margin < margin_floor_db:
        return "AMBRA"
    return "GREEN"


def require_gate_platform_full() -> dict[str, str]:
    """Fail-closed: python + numpy + os/arch must match lock gate_platform."""
    require_gate_platform_python()
    lock = frozen_metrology_lock()["bit_identity"]["gate_platform"]
    actual = {
        "os": platform.system().lower(),
        "arch": platform.machine(),
        "python": gate_platform_python_label(),
        "numpy": __import__("numpy").__version__,
    }
    expected = {
        "os": str(lock["os"]),
        "arch": str(lock["arch"]),
        "python": str(lock["python"]),
        "numpy": str(lock["numpy"]),
    }
    # darwin marketing label is informational; compare kernel family.
    if actual["os"] != expected["os"]:
        raise SRParityError(
            f"os mismatch: actual={actual['os']} expected={expected['os']}"
        )
    if actual["arch"] != expected["arch"]:
        raise SRParityError(
            f"arch mismatch: actual={actual['arch']} expected={expected['arch']}"
        )
    if actual["python"] != expected["python"]:
        raise SRParityError(
            f"python mismatch: actual={actual['python']} "
            f"expected={expected['python']}"
        )
    if actual["numpy"] != expected["numpy"]:
        raise SRParityError(
            f"numpy mismatch: actual={actual['numpy']} "
            f"expected={expected['numpy']}"
        )
    if expected["python"] != GATE_PLATFORM_PYTHON:
        raise SRParityError("lock gate_platform.python drifted from constant")
    return {
        **actual,
        "interpreter": sys.executable,
    }


def _channel_active_mask(
    ref: dict[str, Any],
    sr: dict[str, Any],
    channel: str,
) -> np.ndarray | None:
    """Return 120-bool activity mask, or None if channel ignored (invalid)."""
    v_ref = bool(ref[_VALID_OF[channel]])
    v_sr = bool(sr[_VALID_OF[channel]])
    if not (v_ref and v_sr):
        return None  # invalid_channel_vectors_ignored
    psd_ref = np.asarray(ref[_PSD_OF[channel]], dtype=np.float64)
    psd_sr = np.asarray(sr[_PSD_OF[channel]], dtype=np.float64)
    if psd_ref.shape != (120,) or psd_sr.shape != (120,):
        raise SRParityError(f"{channel} psd shape must be (120,)")
    return np.maximum(psd_ref, psd_sr) > ACTIVITY_FLOOR_DB


def evaluate_sr_parity_cell(
    frames_ref: list[dict[str, Any]],
    frames_sr: list[dict[str, Any]],
    *,
    asset: str,
    fs_sr: int,
    mode: Literal["full", "sweep_checkpoints"] = "full",
    fs_ref: int = REF_HZ,
    p2_main_single: set[int] | None = None,
    p2_lf_single: set[int] | None = None,
) -> CellResult:
    """Compare one (asset, sr) cell against 48 kHz reference frames."""
    useful_ref = filter_useful_frames(frames_ref)
    useful_sr = filter_useful_frames(frames_sr)
    centers = list(band_centers_hz())
    if p2_main_single is None or p2_lf_single is None:
        rep = empty_support_report()
        p2_main_single = set(rep["main_single_bin_indices"])
        p2_lf_single = set(rep["lf_single_bin_indices"])

    if mode == "full":
        pairs: list[tuple[dict[str, Any], dict[str, Any], dict[str, Any]]] = [
            (fr, nearest_frame(useful_sr, source_time_seconds(fr)), {"t": source_time_seconds(fr)})
            for fr in useful_ref
        ]
    elif mode == "sweep_checkpoints":
        assert_sweep_checkpoints_reachable()
        pairs = []
        for freq in SWEEP_CHECKPOINT_HZ:
            t_cross = sweep_crossing_time(float(freq))
            pairs.append(
                (
                    nearest_frame(useful_ref, t_cross),
                    nearest_frame(useful_sr, t_cross),
                    {"checkpoint_hz": int(freq), "t_cross": float(t_cross)},
                )
            )
    else:
        raise SRParityError(f"unknown mode: {mode!r}")

    max_abs = -1.0
    peak: PeakCell | None = None
    n_active = 0
    n_pairs = 0
    n_skip_invalid = 0
    n_skip_empty = 0
    notes: list[str] = []

    def consider(
        field: str,
        band: int,
        delta: float,
        *,
        psd_ref: float | None,
        psd_sr: float | None,
        t_ref: float,
        t_sr: float,
        meta: dict[str, Any],
    ) -> None:
        nonlocal max_abs, peak, n_active
        n_active += 1
        ad = abs(float(delta))
        if ad > max_abs:
            max_abs = ad
            hz = None if band < 0 else float(centers[band])
            peak = PeakCell(
                field=field,
                band=band,
                hz=hz,
                abs_delta_db=ad,
                psd_ref=psd_ref,
                psd_sr=psd_sr,
                t_ref=t_ref,
                t_sr=t_sr,
                p2_main_single_bin=(band in p2_main_single) if band >= 0 else False,
                p2_lf_single_bin=(band in p2_lf_single) if band >= 0 else False,
                meta=dict(meta),
            )

    for ref, sr, meta in pairs:
        t_ref = source_time_seconds(ref)
        t_sr = source_time_seconds(sr)
        masks: dict[str, np.ndarray | None] = {
            "mid": _channel_active_mask(ref, sr, "mid"),
            "side": _channel_active_mask(ref, sr, "side"),
        }
        if masks["mid"] is None:
            n_skip_invalid += 1
        if masks["side"] is None:
            n_skip_invalid += 1

        any_mask = False
        any_active = False
        for ch, mask in masks.items():
            if mask is None:
                continue
            any_mask = True
            if bool(mask.any()):
                any_active = True
        if not any_mask:
            # both channels invalid on this pair — ignore pair
            continue
        if not any_active:
            n_skip_empty += 1
            continue

        n_pairs += 1
        for field_name in _BAND_FIELDS:
            ch = _CHANNEL_OF[field_name]
            mask = masks[ch]
            if mask is None:
                continue
            a = np.asarray(ref[field_name], dtype=np.float64)
            b = np.asarray(sr[field_name], dtype=np.float64)
            psd_a = np.asarray(ref[_PSD_OF[ch]], dtype=np.float64)
            psd_b = np.asarray(sr[_PSD_OF[ch]], dtype=np.float64)
            for i in np.where(mask)[0]:
                consider(
                    field_name,
                    int(i),
                    float(a[i] - b[i]),
                    psd_ref=float(psd_a[i]),
                    psd_sr=float(psd_b[i]),
                    t_ref=t_ref,
                    t_sr=t_sr,
                    meta=meta,
                )
        for field_name in _SCALAR_FIELDS:
            ch = _CHANNEL_OF[field_name]
            mask = masks[ch]
            if mask is None:
                continue
            # level: compare when channel valid on both (already in mask path)
            consider(
                field_name,
                -1,
                float(ref[field_name]) - float(sr[field_name]),
                psd_ref=None,
                psd_sr=None,
                t_ref=t_ref,
                t_sr=t_sr,
                meta=meta,
            )

    if n_active == 0 or max_abs < 0.0:
        raise SRParityError(
            f"empty active-cell set for {asset}@{fs_sr} mode={mode} → FAIL "
            f"(vacuous_pass_forbidden; pairs={n_pairs} "
            f"skip_invalid≈{n_skip_invalid} skip_empty_act={n_skip_empty})"
        )

    verdict = verdict_from_max_abs(max_abs)
    p2_hit = bool(
        peak is not None
        and (peak.p2_main_single_bin or peak.p2_lf_single_bin)
    )
    if p2_hit:
        notes.append(
            f"P2 watch-list HIT: max|Δ| on "
            f"{'MAIN' if peak and peak.p2_main_single_bin else ''}"
            f"{'+' if peak and peak.p2_main_single_bin and peak.p2_lf_single_bin else ''}"
            f"{'LF' if peak and peak.p2_lf_single_bin else ''} "
            f"single-bin band {peak.band if peak else '?'}"
        )
    return CellResult(
        asset=asset,
        fs_sr=fs_sr,
        fs_ref=fs_ref,
        mode=mode,
        max_abs_db=float(max_abs),
        margin_db=float(SR_PARITY_MAX_ABS_DB - max_abs),
        verdict=verdict,
        n_pairs_compared=n_pairs,
        n_active_cells=n_active,
        n_skipped_invalid_channel=n_skip_invalid,
        n_skipped_empty_activity=n_skip_empty,
        peak=peak,
        p2_watch_hit=p2_hit,
        notes=notes,
    )


def _extract_cached(
    cache: dict[tuple[str, int], list[dict[str, Any]]],
    asset: str,
    fs: int,
) -> list[dict[str, Any]]:
    key = (asset, fs)
    if key not in cache:
        render = _RENDERERS[asset]
        audio = render(fs)
        cache[key] = extract_offline_feature_frames(audio, fs)
    return cache[key]


def run_spike_gate4_matrix(
    *,
    assets: tuple[str, ...] = SPIKE_ADVERSARIAL_ASSETS,
    compare_hz: tuple[int, ...] = COMPARE_HZ,
) -> MatrixResult:
    """Run adversarial gate-4 matrix on the gate platform (fail-closed)."""
    platform_info = require_gate_platform_full()
    assert_sweep_checkpoints_reachable()
    useful = common_useful_window()
    p2 = empty_support_report()
    p2_main = set(p2["main_single_bin_indices"])
    p2_lf = set(p2["lf_single_bin_indices"])

    cache: dict[tuple[str, int], list[dict[str, Any]]] = {}
    cells: list[CellResult] = []
    notes: list[str] = [
        "Spike adversarial subset only; transient/damped onset out of scope.",
        "Full 7-schedule streaming product matrix deferred to official G1b.",
        "Prominence unclamped (P3 reflect); no post-hoc prominence clamp.",
        f"Margin floor for GREEN packaging: {MARGIN_FLOOR_DB} dB "
        f"(threshold {SR_PARITY_MAX_ABS_DB} dB).",
    ]

    for asset in assets:
        if asset not in _RENDERERS:
            raise SRParityError(f"unknown adversarial asset: {asset!r}")
        mode: Literal["full", "sweep_checkpoints"] = (
            "sweep_checkpoints" if asset == "log_sweep" else "full"
        )
        frames_ref = _extract_cached(cache, asset, REF_HZ)
        for fs in compare_hz:
            frames_sr = _extract_cached(cache, asset, fs)
            cell = evaluate_sr_parity_cell(
                frames_ref,
                frames_sr,
                asset=asset,
                fs_sr=fs,
                mode=mode,
                p2_main_single=p2_main,
                p2_lf_single=p2_lf,
            )
            cells.append(cell)

    overall_max = max(c.max_abs_db for c in cells)
    # Worst cell dominates; any RED → overall RED.
    if any(c.verdict == "RED" for c in cells):
        overall = "RED"
    elif any(c.verdict == "AMBRA" for c in cells):
        overall = "AMBRA"
    elif any(c.verdict == "INCONCLUSIVE" for c in cells):
        overall = "INCONCLUSIVE"
    else:
        overall = verdict_from_max_abs(overall_max)

    pin_necessity = {
        "pins": ["P1", "P2", "P3", "P4", "P5", "P6", "P7"],
        "changed_to_pass": False,
        "note": (
            "P1–P7 remain the a-priori pins from G1B_SPIKE_PLAN.md; "
            "this run did not rewrite pins to chase a pass. "
            "RED under pinned P* is a REV7 candidate, not silent pin mutation."
        ),
    }

    return MatrixResult(
        platform=platform_info,
        threshold_db=SR_PARITY_MAX_ABS_DB,
        margin_floor_db=MARGIN_FLOOR_DB,
        useful_window=useful,
        cells=cells,
        overall_max_abs_db=float(overall_max),
        overall_verdict=overall,  # type: ignore[arg-type]
        p2_support=p2,
        pin_necessity=pin_necessity,
        notes=notes,
    )
