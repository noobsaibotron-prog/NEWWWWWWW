"""§5 causal polyphase resampler — streaming apply (G1b T1b / spike WS1).

Implements contract phase-zero FIR:
    y[j] = sum_n x[n] * h[j*down - n*up]
with out-of-range h indices treated as 0. For N input samples emit
    0 <= j < ceil(N*up/down)
with no trailing filter flush. State is kept across blocks.

P5 (pinned a priori): accumulate FIR work in float64; cast each emitted
sample to float32 once at emit; left-to-right association on ascending n
(frozen index order). Offline monolith is one process() of the full buffer;
chunked schedules share the same per-j path.
"""
from __future__ import annotations

from typing import Iterable, Sequence

import numpy as np

from ml_v3.frontend.resampler_coeffs import (
    ResamplerCoeffError,
    fir_lowpass_coefficients,
    resample_ratio,
)

__all__ = [
    "ResamplerError",
    "CausalPolyphaseResampler",
    "resample_offline",
    "resample_chunked",
    "iter_host_chunks",
]


class ResamplerError(ResamplerCoeffError):
    """Fail-closed error for streaming apply (invalid SR / non-finite input)."""


def _as_float64_1d(x: np.ndarray) -> np.ndarray:
    if not isinstance(x, np.ndarray):
        raise ResamplerError(f"input must be numpy.ndarray, got {type(x).__name__}")
    if x.ndim != 1:
        raise ResamplerError(f"input must be 1-D host samples, got shape {x.shape}")
    if x.size == 0:
        return np.zeros(0, dtype=np.float64)
    if not np.all(np.isfinite(x)):
        raise ResamplerError("non-finite input sample (NaN/Inf) rejected")
    return np.asarray(x, dtype=np.float64, order="C")


def iter_host_chunks(
    n_samples: int,
    schedule: int | Sequence[int],
) -> Iterable[tuple[int, int]]:
    """Yield successive (start, end) host-sample slices for a frozen schedule.

    Fixed schedule: constant chunk size (last chunk may be short).
    Sequence schedule (e.g. geometric-32): cycle sizes until input exhausted.
    """
    if n_samples < 0:
        raise ResamplerError(f"n_samples must be >= 0, got {n_samples}")
    if n_samples == 0:
        return
    if isinstance(schedule, int):
        if schedule <= 0:
            raise ResamplerError(f"fixed chunk size must be > 0, got {schedule}")
        start = 0
        while start < n_samples:
            end = min(n_samples, start + schedule)
            yield start, end
            start = end
        return
    if not schedule:
        raise ResamplerError("chunk schedule sequence must be non-empty")
    sizes = [int(s) for s in schedule]
    if any(s <= 0 for s in sizes):
        raise ResamplerError(f"chunk sizes must be > 0, got {sizes!r}")
    start = 0
    idx = 0
    while start < n_samples:
        end = min(n_samples, start + sizes[idx % len(sizes)])
        yield start, end
        start = end
        idx += 1


class CausalPolyphaseResampler:
    """Stateful causal polyphase resampler fs_in → 48000 Hz (§5)."""

    def __init__(self, fs_in: int) -> None:
        try:
            self._ratio = resample_ratio(fs_in)
            self._h = fir_lowpass_coefficients(fs_in)
        except ResamplerCoeffError as exc:
            # Unified fail-closed surface for apply callers.
            raise ResamplerError(str(exc)) from exc
        self._fs_in = fs_in
        self._n_total = 0
        self._j_emitted = 0
        # Absolute host index of _buf[0]; samples held in float64.
        self._buf_start = 0
        self._buf = np.zeros(0, dtype=np.float64)

    @property
    def fs_in(self) -> int:
        return self._fs_in

    @property
    def identity(self) -> bool:
        return self._ratio.identity

    @property
    def n_total(self) -> int:
        return self._n_total

    @property
    def j_emitted(self) -> int:
        return self._j_emitted

    def reset(self) -> None:
        self._n_total = 0
        self._j_emitted = 0
        self._buf_start = 0
        self._buf = np.zeros(0, dtype=np.float64)

    def process(self, x: np.ndarray) -> np.ndarray:
        """Consume one host block; return newly emitted float32 samples."""
        block = _as_float64_1d(x)
        if block.size == 0:
            return np.zeros(0, dtype=np.float32)

        if self._ratio.identity:
            return self._process_identity(block)
        return self._process_polyphase(block)

    def _process_identity(self, block: np.ndarray) -> np.ndarray:
        # Delay 0 passthrough; P5 cast once at emit.
        out = np.empty(block.size, dtype=np.float32)
        for i in range(block.size):
            out[i] = np.float32(block[i])
        self._n_total += int(block.size)
        self._j_emitted += int(block.size)
        return out

    def _process_polyphase(self, block: np.ndarray) -> np.ndarray:
        up = self._ratio.up
        down = self._ratio.down
        h = self._h
        m = int(h.size) - 1
        n_prev = self._n_total
        n_new = n_prev + int(block.size)
        j_prev = self._j_emitted
        # ceil(n_new * up / down)
        j_new = (n_new * up + down - 1) // down
        n_emit = j_new - j_prev
        if n_emit < 0:
            raise ResamplerError("internal emit cursor moved backwards")

        if self._buf.size == 0:
            self._buf = block.copy()
            self._buf_start = n_prev
        else:
            self._buf = np.concatenate((self._buf, block))

        out = np.empty(n_emit, dtype=np.float32)
        buf = self._buf
        buf_start = self._buf_start

        for k, j in enumerate(range(j_prev, j_new)):
            # n in [ceil((j*down - M)/up), floor(j*down/up)] ∩ [0, n_new)
            numer = j * down - m
            if numer <= 0:
                n_lo = 0
            else:
                n_lo = (numer + up - 1) // up
            n_hi = (j * down) // up
            if n_hi >= n_new:
                n_hi = n_new - 1
            if n_lo < 0:
                n_lo = 0

            acc = 0.0  # float64; left-to-right on ascending n (P5)
            if n_lo <= n_hi:
                for n in range(n_lo, n_hi + 1):
                    h_idx = j * down - n * up
                    # Guaranteed in [0, M] by n_lo/n_hi construction.
                    acc = acc + buf[n - buf_start] * h[h_idx]
            out[k] = np.float32(acc)

        self._n_total = n_new
        self._j_emitted = j_new
        self._trim_history(up=up, down=down, m=m)
        return out

    def _trim_history(self, *, up: int, down: int, m: int) -> None:
        """Drop host samples that can no longer contribute to future outputs."""
        next_j = self._j_emitted
        numer = next_j * down - m
        if numer <= 0:
            keep_from = 0
        else:
            keep_from = (numer + up - 1) // up
        if keep_from < 0:
            keep_from = 0
        if keep_from <= self._buf_start:
            return
        rel = keep_from - self._buf_start
        if rel >= self._buf.size:
            self._buf = np.zeros(0, dtype=np.float64)
            self._buf_start = keep_from
            return
        self._buf = self._buf[rel:].copy()
        self._buf_start = keep_from


def resample_offline(x: np.ndarray, fs_in: int) -> np.ndarray:
    """Monolithic offline apply (single process of the full host buffer)."""
    return CausalPolyphaseResampler(fs_in).process(x)


def resample_chunked(
    x: np.ndarray,
    fs_in: int,
    schedule: int | Sequence[int],
) -> np.ndarray:
    """Chunked streaming apply under a frozen host-sample schedule."""
    x64 = _as_float64_1d(x)
    resampler = CausalPolyphaseResampler(fs_in)
    parts: list[np.ndarray] = []
    for start, end in iter_host_chunks(int(x64.size), schedule):
        parts.append(resampler.process(x64[start:end]))
    if not parts:
        return np.zeros(0, dtype=np.float32)
    return np.concatenate(parts)
