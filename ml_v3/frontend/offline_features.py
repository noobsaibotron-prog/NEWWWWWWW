"""§6+§7 offline feature path (G1b T2 / spike WS2).

Offline-only emit under pinned P1–P7. Streaming/chunked feature path is WS3+.
Does not touch hashed G1a artifacts, CONTRACT, or ship-line.

Pins (a priori; changing after numbers → AMBRA):
  P1  band energy = Σ(w·psd)/Σw
  P2  empty Σw → 0 to sum; after fuse if linear==0 → floor 1e-12; no nearest-bin
  P3  prominence pad mode = reflect
  P4  shape from clamped emitted psd_db
  P5  f64 accumulate; cast each emitted float field to f32 once at write
  P6  invalid channel: psd/shape/level=-120; delta=0; prominence=0
  P7  reason ∈ {silence, level_below_threshold} when valid=false; hard errors
      raise (no frame)
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from math import gcd
from typing import Any

import numpy as np

from ml_v3.contracts.constants import (
    ACCEPTED_SAMPLE_RATES,
    CANONICAL_SAMPLE_RATE,
    GRID_BANDS,
    GRID_MAX_HZ,
    GRID_MIN_HZ,
)
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.contracts.metrology_lock import (
    HOP_SAMPLES,
    N_LF,
    PSD_CLAMP_DB,
    PSD_FLOOR_LINEAR,
)
from ml_v3.frontend.feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    validate_feature_frame_stub,
)
from ml_v3.frontend.resampler import ResamplerError, resample_offline
from ml_v3.frontend.resampler_coeffs import group_delay_rational

__all__ = [
    "OfflineFeatureError",
    "N_MAIN",
    "FUSION_LO_HZ",
    "FUSION_HI_HZ",
    "LEVEL_FLOOR_DBFS",
    "LEVEL_VALID_DBFS",
    "DELTA_CLAMP_DB",
    "P7_REASONS",
    "empty_support_report",
    "extract_offline_feature_frames",
]

N_MAIN: int = 4096
FUSION_LO_HZ: float = 160.0
FUSION_HI_HZ: float = 320.0
LEVEL_FLOOR_DBFS: float = -120.0
LEVEL_VALID_DBFS: float = -100.0
DELTA_CLAMP_DB: tuple[float, float] = (-24.0, 24.0)
PROMINENCE_SIGMA: float = 4.0
PROMINENCE_HALF: int = 16
P7_REASONS: frozenset[str] = frozenset({"silence", "level_below_threshold"})


class OfflineFeatureError(ValueError):
    """Fail-closed hard error: no frame is emitted."""


def _f32(x: float) -> float:
    """P5: cast once at emit."""
    return float(np.float32(x))


def _reduce(num: int, den: int) -> tuple[int, int]:
    if den <= 0:
        raise OfflineFeatureError(f"non-positive denominator: {den}")
    g = gcd(num, den)
    return num // g, den // g


def _hann_periodic(n: int) -> np.ndarray:
    """Periodic Hann: 0.5 - 0.5*cos(2πn/N), n=0..N-1 (§6.2)."""
    if n <= 0:
        raise OfflineFeatureError(f"window length must be > 0, got {n}")
    idx = np.arange(n, dtype=np.float64)
    return 0.5 - 0.5 * np.cos(2.0 * math.pi * idx / float(n))


def _edge_factor(n_fft: int) -> np.ndarray:
    n_bins = n_fft // 2 + 1
    ef = np.full(n_bins, 2.0, dtype=np.float64)
    ef[0] = 1.0
    ef[-1] = 1.0
    return ef


@dataclass(frozen=True)
class _BandWeightTable:
    """Precomputed triangular weights on log2(f) for one FFT size."""

    n_fft: int
    weights: np.ndarray  # (GRID_BANDS, n_bins)
    sum_w: np.ndarray  # (GRID_BANDS,)
    empty_band_indices: tuple[int, ...]
    single_bin_band_indices: tuple[int, ...]


def _virtual_centers(centers: np.ndarray) -> tuple[float, float]:
    """Extreme virtual centres with the same geometric ratio (§6.1)."""
    ratio = (GRID_MAX_HZ / GRID_MIN_HZ) ** (1.0 / float(GRID_BANDS - 1))
    return float(centers[0] / ratio), float(centers[-1] * ratio)


def _build_band_weights(n_fft: int, fs: int = CANONICAL_SAMPLE_RATE) -> _BandWeightTable:
    centers = np.asarray(band_centers_hz(), dtype=np.float64)
    left_v, right_v = _virtual_centers(centers)
    ext = np.empty(GRID_BANDS + 2, dtype=np.float64)
    ext[0] = left_v
    ext[1:-1] = centers
    ext[-1] = right_v
    log_ext = np.log2(ext)

    n_bins = n_fft // 2 + 1
    freqs = np.arange(n_bins, dtype=np.float64) * (float(fs) / float(n_fft))
    # DC and bins > 20 kHz do not contribute (§6.1).
    usable = (freqs > 0.0) & (freqs <= GRID_MAX_HZ)
    log_f = np.empty(n_bins, dtype=np.float64)
    log_f[:] = np.nan
    log_f[usable] = np.log2(freqs[usable])

    w = np.zeros((GRID_BANDS, n_bins), dtype=np.float64)
    for i in range(GRID_BANDS):
        lo = log_ext[i]
        mid = log_ext[i + 1]
        hi = log_ext[i + 2]
        for k in range(n_bins):
            if not usable[k]:
                continue
            x = log_f[k]
            if x <= lo or x >= hi:
                continue
            if x <= mid:
                denom = mid - lo
                if denom > 0.0:
                    w[i, k] = (x - lo) / denom
            else:
                denom = hi - mid
                if denom > 0.0:
                    w[i, k] = (hi - x) / denom

    sum_w = w.sum(axis=1)
    empty = tuple(int(i) for i in range(GRID_BANDS) if sum_w[i] == 0.0)
    single = tuple(
        int(i)
        for i in range(GRID_BANDS)
        if sum_w[i] > 0.0 and int(np.count_nonzero(w[i] > 0.0)) == 1
    )
    return _BandWeightTable(
        n_fft=n_fft,
        weights=w,
        sum_w=sum_w,
        empty_band_indices=empty,
        single_bin_band_indices=single,
    )


_WEIGHTS_MAIN: _BandWeightTable | None = None
_WEIGHTS_LF: _BandWeightTable | None = None
_FUSION_LF: np.ndarray | None = None
_PROMINENCE_KERNEL: np.ndarray | None = None
_HANN_MAIN: np.ndarray | None = None
_HANN_LF: np.ndarray | None = None
_EDGE_MAIN: np.ndarray | None = None
_EDGE_LF: np.ndarray | None = None


def _tables() -> tuple[
    _BandWeightTable,
    _BandWeightTable,
    np.ndarray,
    np.ndarray,
    np.ndarray,
    np.ndarray,
    np.ndarray,
    np.ndarray,
]:
    global _WEIGHTS_MAIN, _WEIGHTS_LF, _FUSION_LF, _PROMINENCE_KERNEL
    global _HANN_MAIN, _HANN_LF, _EDGE_MAIN, _EDGE_LF
    if _WEIGHTS_MAIN is None:
        _WEIGHTS_MAIN = _build_band_weights(N_MAIN)
        _WEIGHTS_LF = _build_band_weights(N_LF)
        centers = np.asarray(band_centers_hz(), dtype=np.float64)
        _FUSION_LF = _fusion_lf_weights(centers)
        j = np.arange(-PROMINENCE_HALF, PROMINENCE_HALF + 1, dtype=np.float64)
        ker = np.exp(-0.5 * (j / PROMINENCE_SIGMA) ** 2)
        _PROMINENCE_KERNEL = ker / ker.sum()
        _HANN_MAIN = _hann_periodic(N_MAIN)
        _HANN_LF = _hann_periodic(N_LF)
        _EDGE_MAIN = _edge_factor(N_MAIN)
        _EDGE_LF = _edge_factor(N_LF)
    assert _WEIGHTS_MAIN is not None and _WEIGHTS_LF is not None
    assert _FUSION_LF is not None and _PROMINENCE_KERNEL is not None
    assert _HANN_MAIN is not None and _HANN_LF is not None
    assert _EDGE_MAIN is not None and _EDGE_LF is not None
    return (
        _WEIGHTS_MAIN,
        _WEIGHTS_LF,
        _FUSION_LF,
        _PROMINENCE_KERNEL,
        _HANN_MAIN,
        _HANN_LF,
        _EDGE_MAIN,
        _EDGE_LF,
    )


def _fusion_lf_weights(centers: np.ndarray) -> np.ndarray:
    """LF raised-cosine weight on log2(f); MAIN = 1 - LF (§6.2)."""
    out = np.empty(GRID_BANDS, dtype=np.float64)
    log_lo = math.log2(FUSION_LO_HZ)
    log_span = math.log2(FUSION_HI_HZ / FUSION_LO_HZ)
    for i, f in enumerate(centers):
        if f <= FUSION_LO_HZ:
            out[i] = 1.0
        elif f >= FUSION_HI_HZ:
            out[i] = 0.0
        else:
            out[i] = 0.5 * (
                1.0 + math.cos(math.pi * (math.log2(float(f)) - log_lo) / log_span)
            )
    return out


def empty_support_report() -> dict[str, Any]:
    """P2 watch-list evidence: empty / single-bin band counts (geometry only)."""
    main, lf, *_ = _tables()
    return {
        "fs_c": CANONICAL_SAMPLE_RATE,
        "n_main": N_MAIN,
        "n_lf": N_LF,
        "main_empty_count": len(main.empty_band_indices),
        "main_empty_indices": list(main.empty_band_indices),
        "main_single_bin_count": len(main.single_bin_band_indices),
        "main_single_bin_indices": list(main.single_bin_band_indices),
        "lf_empty_count": len(lf.empty_band_indices),
        "lf_empty_indices": list(lf.empty_band_indices),
        "lf_single_bin_count": len(lf.single_bin_band_indices),
        "lf_single_bin_indices": list(lf.single_bin_band_indices),
        "note": (
            "Counts are triangular-support geometry at fs_c; not silence. "
            "No v2 nearest-bin fallback (P2)."
        ),
    }


def _onesided_psd(
    x: np.ndarray,
    window: np.ndarray,
    edge: np.ndarray,
    fs: int = CANONICAL_SAMPLE_RATE,
) -> np.ndarray:
    """§6.1 one-sided PSD in linear power (float64)."""
    if x.shape != window.shape:
        raise OfflineFeatureError(
            f"window/x length mismatch: {x.shape} vs {window.shape}"
        )
    # P5: f64 work; numpy FFT on f64.
    framed = window * x
    spec = np.fft.rfft(framed)
    denom = float(fs) * float(np.dot(window, window))
    if denom <= 0.0 or not math.isfinite(denom):
        raise OfflineFeatureError(f"degenerate window energy: {denom!r}")
    mag2 = (spec.real * spec.real) + (spec.imag * spec.imag)
    return edge * mag2 / denom


def _band_energy_linear(psd: np.ndarray, table: _BandWeightTable) -> np.ndarray:
    """P1/P2: true weighted mean; empty Σw → 0 (no nearest-bin)."""
    if psd.shape[0] != table.weights.shape[1]:
        raise OfflineFeatureError("psd / weight bin mismatch")
    energy = np.zeros(GRID_BANDS, dtype=np.float64)
    for i in range(GRID_BANDS):
        sw = float(table.sum_w[i])
        if sw == 0.0:
            energy[i] = 0.0  # P2: empty support contributes 0
            continue
        # P1: Σ(w·psd)/Σw — left-to-right on ascending bin index.
        acc = 0.0
        row = table.weights[i]
        for k in range(row.shape[0]):
            wk = float(row[k])
            if wk != 0.0:
                acc = acc + wk * float(psd[k])
        energy[i] = acc / sw
    return energy


def _fuse_band_energy(main_e: np.ndarray, lf_e: np.ndarray, w_lf: np.ndarray) -> np.ndarray:
    """Power-domain LF/MAIN fusion; P2 floor when fused linear is 0."""
    fused = w_lf * lf_e + (1.0 - w_lf) * main_e
    for i in range(GRID_BANDS):
        if fused[i] == 0.0:
            fused[i] = PSD_FLOOR_LINEAR
        elif fused[i] < PSD_FLOOR_LINEAR:
            fused[i] = PSD_FLOOR_LINEAR
    return fused


def _linear_to_clamped_db(linear: np.ndarray) -> np.ndarray:
    lo, hi = PSD_CLAMP_DB
    out = np.empty(GRID_BANDS, dtype=np.float64)
    for i in range(GRID_BANDS):
        v = float(linear[i])
        if v < PSD_FLOOR_LINEAR:
            v = PSD_FLOOR_LINEAR
        db = 10.0 * math.log10(v)
        if db < lo:
            db = lo
        elif db > hi:
            db = hi
        out[i] = db
    return out


def _shape_from_clamped_psd(psd_db: np.ndarray) -> np.ndarray:
    """P4: shape from clamped emitted psd_db."""
    acc = 0.0
    for i in range(GRID_BANDS):
        acc = acc + (10.0 ** (float(psd_db[i]) / 10.0))
    if acc <= 0.0 or not math.isfinite(acc):
        raise OfflineFeatureError(f"degenerate shape power sum: {acc!r}")
    offset = 10.0 * math.log10(acc)
    return psd_db - offset


def _prominence(shape_db: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    """§7 prominence: shape − gaussian conv; P3 pad=reflect."""
    pad = PROMINENCE_HALF
    # numpy reflect == contract «padding reflect» (not symmetric).
    padded = np.pad(shape_db, pad_width=pad, mode="reflect")
    smooth = np.convolve(padded, kernel, mode="valid")
    if smooth.shape[0] != GRID_BANDS:
        raise OfflineFeatureError(
            f"prominence conv length {smooth.shape[0]} != {GRID_BANDS}"
        )
    return shape_db - smooth


def _level_dbfs(x_main: np.ndarray) -> float:
    """Unwindowed RMS of MAIN window; floor −120 dBFS (§7)."""
    if x_main.size == 0:
        return LEVEL_FLOOR_DBFS
    acc = 0.0
    for i in range(x_main.size):
        v = float(x_main[i])
        acc = acc + v * v
    mean_sq = acc / float(x_main.size)
    if mean_sq <= 0.0 or not math.isfinite(mean_sq):
        return LEVEL_FLOOR_DBFS
    db = 20.0 * math.log10(math.sqrt(mean_sq))
    if db < LEVEL_FLOOR_DBFS:
        return LEVEL_FLOOR_DBFS
    return db


def _p7_reason(mid_level: float, side_level: float, *, mono: bool) -> str:
    """Deterministic disjoint reason when valid=false (P7)."""
    mid_floor = mid_level <= LEVEL_FLOOR_DBFS
    side_floor = side_level <= LEVEL_FLOOR_DBFS
    if mono:
        return "silence" if mid_floor else "level_below_threshold"
    if mid_floor and side_floor:
        return "silence"
    return "level_below_threshold"


def _invalid_channel_vectors() -> dict[str, Any]:
    """P6 floors/zeros for an invalid channel."""
    return {
        "psd_db": np.full(GRID_BANDS, LEVEL_FLOOR_DBFS, dtype=np.float64),
        "shape_db": np.full(GRID_BANDS, LEVEL_FLOOR_DBFS, dtype=np.float64),
        "prominence_db": np.zeros(GRID_BANDS, dtype=np.float64),
        "delta_db": np.zeros(GRID_BANDS, dtype=np.float64),
        "level_dbfs": LEVEL_FLOOR_DBFS,
    }


def _clamp_delta(delta: np.ndarray) -> np.ndarray:
    lo, hi = DELTA_CLAMP_DB
    out = np.empty(GRID_BANDS, dtype=np.float64)
    for i in range(GRID_BANDS):
        v = float(delta[i])
        if v < lo:
            v = lo
        elif v > hi:
            v = hi
        out[i] = v
    return out


def _as_mid_side(audio: np.ndarray) -> tuple[np.ndarray, np.ndarray | None, bool]:
    """Return (mid_f64, side_f64|None, mono). Fail-closed on layout/NaN."""
    if not isinstance(audio, np.ndarray):
        raise OfflineFeatureError(
            f"audio must be numpy.ndarray, got {type(audio).__name__}"
        )
    if audio.ndim == 1:
        x = np.asarray(audio, dtype=np.float64, order="C")
        if x.size == 0:
            raise OfflineFeatureError("zero-length audio rejected")
        if not np.all(np.isfinite(x)):
            raise OfflineFeatureError("non-finite input sample (NaN/Inf) rejected")
        return x, None, True
    if audio.ndim != 2:
        raise OfflineFeatureError(f"audio ndim must be 1 or 2, got {audio.ndim}")

    a = np.asarray(audio, dtype=np.float64, order="C")
    if a.shape[0] == 0 or a.shape[1] == 0:
        raise OfflineFeatureError("zero-length audio rejected")

    # (N, 2) samples-first or (2, N) channels-first.
    if a.shape[1] == 2 and a.shape[0] != 2:
        left, right = a[:, 0], a[:, 1]
    elif a.shape[0] == 2:
        left, right = a[0], a[1]
    elif a.shape[1] == 2 and a.shape[0] == 2:
        # Ambiguous 2×2: treat as samples-first (N=2, ch=2).
        left, right = a[:, 0], a[:, 1]
    else:
        raise OfflineFeatureError(
            f"stereo layout must be (N,2) or (2,N); got shape {a.shape}"
        )

    if left.shape != right.shape:
        raise OfflineFeatureError("left/right length mismatch")
    if not np.all(np.isfinite(left)) or not np.all(np.isfinite(right)):
        raise OfflineFeatureError("non-finite input sample (NaN/Inf) rejected")
    mid = 0.5 * (left + right)
    side = 0.5 * (left - right)
    return mid, side, False


def _source_time_rational(frame_end: int, delay_num: int, delay_den: int) -> tuple[int, int]:
    """source_time = frame_end/fs_c − delay (§5/§7) as reduced rational."""
    # (frame_end * delay_den - delay_num * fs_c) / (fs_c * delay_den)
    num = frame_end * delay_den - delay_num * CANONICAL_SAMPLE_RATE
    den = CANONICAL_SAMPLE_RATE * delay_den
    return _reduce(num, den)


def _emit_vector(vec: np.ndarray) -> list[float]:
    return [_f32(float(v)) for v in vec]


def _channel_features(
    mid_c: np.ndarray,
    frame_end: int,
    *,
    w_main: _BandWeightTable,
    w_lf: _BandWeightTable,
    fusion_lf: np.ndarray,
    prom_ker: np.ndarray,
    hann_main: np.ndarray,
    hann_lf: np.ndarray,
    edge_main: np.ndarray,
    edge_lf: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, float]:
    """Return (psd_db, shape_db, prominence_db, level_dbfs) for one channel."""
    main = mid_c[frame_end - N_MAIN : frame_end]
    lf = mid_c[frame_end - N_LF : frame_end]
    psd_main = _onesided_psd(main, hann_main, edge_main)
    psd_lf = _onesided_psd(lf, hann_lf, edge_lf)
    e_main = _band_energy_linear(psd_main, w_main)
    e_lf = _band_energy_linear(psd_lf, w_lf)
    fused = _fuse_band_energy(e_main, e_lf, fusion_lf)
    psd_db = _linear_to_clamped_db(fused)
    shape = _shape_from_clamped_psd(psd_db)
    prom = _prominence(shape, prom_ker)
    level = _level_dbfs(main)
    return psd_db, shape, prom, level


def extract_offline_feature_frames(
    audio: np.ndarray,
    fs_in: int,
) -> list[dict[str, Any]]:
    """Extract §7 frames offline (monolithic resample + STFT).

    Hard errors raise ``OfflineFeatureError`` / ``ResamplerError`` (no frame).
    Insufficient length → empty list (incomplete frames are not emitted).
    """
    if not isinstance(fs_in, int) or isinstance(fs_in, bool):
        raise OfflineFeatureError(f"fs_in must be int, got {type(fs_in).__name__}")
    if fs_in not in ACCEPTED_SAMPLE_RATES:
        raise OfflineFeatureError(
            f"fs_in {fs_in} rejected; accepted={ACCEPTED_SAMPLE_RATES}"
        )

    mid_h, side_h, mono = _as_mid_side(audio)

    try:
        mid_c = np.asarray(resample_offline(mid_h.astype(np.float32), fs_in), dtype=np.float64)
        if mono:
            side_c = None
        else:
            assert side_h is not None
            side_c = np.asarray(
                resample_offline(side_h.astype(np.float32), fs_in), dtype=np.float64
            )
    except ResamplerError as exc:
        raise OfflineFeatureError(str(exc)) from exc

    n = int(mid_c.size)
    if side_c is not None and int(side_c.size) != n:
        raise OfflineFeatureError("mid/side canonical length mismatch")
    if n < N_LF:
        return []

    (
        w_main,
        w_lf,
        fusion_lf,
        prom_ker,
        hann_main,
        hann_lf,
        edge_main,
        edge_lf,
    ) = _tables()

    delay_num, delay_den = group_delay_rational(fs_in)
    frames: list[dict[str, Any]] = []
    prev_mid_shape: np.ndarray | None = None
    prev_side_shape: np.ndarray | None = None
    frame_index = 0
    frame_end = N_LF

    while frame_end <= n:
        psd_m, shape_m, prom_m, level_m = _channel_features(
            mid_c,
            frame_end,
            w_main=w_main,
            w_lf=w_lf,
            fusion_lf=fusion_lf,
            prom_ker=prom_ker,
            hann_main=hann_main,
            hann_lf=hann_lf,
            edge_main=edge_main,
            edge_lf=edge_lf,
        )
        mid_valid = level_m >= LEVEL_VALID_DBFS

        if mono or side_c is None:
            inv = _invalid_channel_vectors()
            psd_s = inv["psd_db"]
            shape_s = inv["shape_db"]
            prom_s = inv["prominence_db"]
            level_s = float(inv["level_dbfs"])
            side_valid = False
        else:
            psd_s, shape_s, prom_s, level_s = _channel_features(
                side_c,
                frame_end,
                w_main=w_main,
                w_lf=w_lf,
                fusion_lf=fusion_lf,
                prom_ker=prom_ker,
                hann_main=hann_main,
                hann_lf=hann_lf,
                edge_main=edge_main,
                edge_lf=edge_lf,
            )
            side_valid = level_s >= LEVEL_VALID_DBFS

        valid = bool(mid_valid or side_valid)

        # Measured levels (floored) drive P7; P6 may overwrite emitted level.
        if level_m < LEVEL_FLOOR_DBFS:
            level_m = LEVEL_FLOOR_DBFS
        if level_s < LEVEL_FLOOR_DBFS:
            level_s = LEVEL_FLOOR_DBFS
        measured_mid_level = float(level_m)
        measured_side_level = float(level_s)

        if not valid:
            mid_delta = np.zeros(GRID_BANDS, dtype=np.float64)
            side_delta = np.zeros(GRID_BANDS, dtype=np.float64)
            prev_mid_shape = None
            prev_side_shape = None
            reason: str | None = _p7_reason(
                measured_mid_level, measured_side_level, mono=mono
            )
        else:
            reason = None
            if mid_valid:
                if prev_mid_shape is None:
                    mid_delta = np.zeros(GRID_BANDS, dtype=np.float64)
                else:
                    mid_delta = _clamp_delta(shape_m - prev_mid_shape)
                prev_mid_shape = shape_m.copy()
            else:
                mid_delta = np.zeros(GRID_BANDS, dtype=np.float64)
                prev_mid_shape = None
            if side_valid:
                if prev_side_shape is None:
                    side_delta = np.zeros(GRID_BANDS, dtype=np.float64)
                else:
                    side_delta = _clamp_delta(shape_s - prev_side_shape)
                prev_side_shape = shape_s.copy()
            else:
                side_delta = np.zeros(GRID_BANDS, dtype=np.float64)
                prev_side_shape = None

        # P6 overlays for invalid-channel emitted fields (after P7 reason).
        if not mid_valid:
            inv = _invalid_channel_vectors()
            psd_m = inv["psd_db"]
            shape_m = inv["shape_db"]
            prom_m = inv["prominence_db"]
            level_m = float(inv["level_dbfs"])
            mid_delta = inv["delta_db"]
        if not side_valid:
            inv = _invalid_channel_vectors()
            psd_s = inv["psd_db"]
            shape_s = inv["shape_db"]
            prom_s = inv["prominence_db"]
            level_s = float(inv["level_dbfs"])
            side_delta = inv["delta_db"]

        st_num, st_den = _source_time_rational(frame_end, delay_num, delay_den)
        frame = {
            "schema": FEATURE_FRAME_SCHEMA,
            "frame_end_sample": int(frame_end),
            "frame_index": int(frame_index),
            "source_time_num": int(st_num),
            "source_time_den": int(st_den),
            "canonical_sample_rate": CANONICAL_SAMPLE_RATE,
            "mid_psd_db": _emit_vector(psd_m),
            "side_psd_db": _emit_vector(psd_s),
            "mid_shape_db": _emit_vector(shape_m),
            "side_shape_db": _emit_vector(shape_s),
            "mid_prominence_db": _emit_vector(prom_m),
            "side_prominence_db": _emit_vector(prom_s),
            "mid_delta_db": _emit_vector(mid_delta),
            "side_delta_db": _emit_vector(side_delta),
            "mid_level_dbfs": _f32(level_m),
            "side_level_dbfs": _f32(level_s),
            "mid_valid": bool(mid_valid),
            "side_valid": bool(side_valid),
            "valid": bool(valid),
            "reason": reason,
        }
        if reason is not None and reason not in P7_REASONS:
            raise OfflineFeatureError(f"internal P7 reason out of enum: {reason!r}")
        try:
            validate_feature_frame_stub(frame)
        except FeatureFrameError as exc:
            raise OfflineFeatureError(f"emitted frame failed §7 stub: {exc}") from exc

        # Extra range guards (emit contract).
        for name in ("mid_delta_db", "side_delta_db"):
            for v in frame[name]:
                if v < DELTA_CLAMP_DB[0] or v > DELTA_CLAMP_DB[1]:
                    raise OfflineFeatureError(
                        f"{name} out of clamp [{DELTA_CLAMP_DB[0]}, {DELTA_CLAMP_DB[1]}]"
                    )
        for name in ("mid_psd_db", "side_psd_db"):
            for v in frame[name]:
                if v < PSD_CLAMP_DB[0] or v > PSD_CLAMP_DB[1]:
                    raise OfflineFeatureError(f"{name} out of PSD clamp")

        frames.append(frame)
        frame_index += 1
        frame_end += HOP_SAMPLES

    return frames
