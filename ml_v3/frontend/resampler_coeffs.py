"""§5 FIR low-pass polyphase coefficient generator (G1b T1).

G1a only serialized the generator parameters (`serialize_only`). This module
produces the actual coefficients from the frozen formulas. It does not stream
audio and does not claim sample-rate parity PASS.
"""
from __future__ import annotations

from math import gcd
from typing import NamedTuple

import numpy as np

from ml_v3.contracts.constants import (
    ACCEPTED_SAMPLE_RATES,
    CANONICAL_SAMPLE_RATE,
)
from ml_v3.contracts.metrology_lock import (
    KAISER_BETA,
    RESAMPLER_PASS_HZ,
)

__all__ = [
    "ResamplerCoeffError",
    "ResampleRatio",
    "resample_ratio",
    "group_delay_rational",
    "fir_lowpass_coefficients",
]


class ResamplerCoeffError(ValueError):
    """Fail-closed error for invalid sample rate or degenerate coefficients."""


class ResampleRatio(NamedTuple):
    up: int
    down: int
    num_taps: int | None  # None when identity (up == down == 1)
    identity: bool


def _reduce(num: int, den: int) -> tuple[int, int]:
    if den <= 0:
        raise ResamplerCoeffError(f"non-positive denominator: {den}")
    g = gcd(num, den)
    return num // g, den // g


def resample_ratio(fs_in: int) -> ResampleRatio:
    """Return up/down/num_taps for fs_in → 48000 (§5)."""
    if not isinstance(fs_in, int) or isinstance(fs_in, bool):
        raise ResamplerCoeffError(f"fs_in must be int, got {type(fs_in).__name__}")
    if fs_in not in ACCEPTED_SAMPLE_RATES:
        raise ResamplerCoeffError(
            f"fs_in {fs_in} rejected; accepted={ACCEPTED_SAMPLE_RATES}"
        )
    g = gcd(fs_in, CANONICAL_SAMPLE_RATE)
    up = CANONICAL_SAMPLE_RATE // g
    down = fs_in // g
    if up == 1 and down == 1:
        return ResampleRatio(up=1, down=1, num_taps=None, identity=True)
    num_taps = 128 * max(up, down) + 1
    return ResampleRatio(up=up, down=down, num_taps=num_taps, identity=False)


def group_delay_rational(fs_in: int) -> tuple[int, int]:
    """Group delay as reduced (num, den) seconds (§5).

    Identity → (0, 1). Matches metrology lock for gate rates.
    """
    ratio = resample_ratio(fs_in)
    if ratio.identity:
        return 0, 1
    assert ratio.num_taps is not None
    return _reduce(ratio.num_taps - 1, 2 * ratio.up * fs_in)


def fir_lowpass_coefficients(fs_in: int) -> np.ndarray:
    """Generate causal FIR low-pass coefficients for polyphase phase-zero (§5).

    Returns float64 array length `num_taps`, or length-0 for identity.
    Formulas (contract §5):

        stop_hz = min(fs_in, 48000) / 2
        fc = ((pass_hz + stop_hz) / 2) / (fs_in * up)
        M = num_taps - 1
        h0[n] = 2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0)
        h[n]  = up * h0[n] / sum(h0)

    `numpy.sinc` is sin(pi*x)/(pi*x), matching the contract definition.
    """
    ratio = resample_ratio(fs_in)
    if ratio.identity:
        return np.zeros(0, dtype=np.float64)

    up = ratio.up
    num_taps = ratio.num_taps
    assert num_taps is not None
    m = num_taps - 1
    stop_hz = min(float(fs_in), float(CANONICAL_SAMPLE_RATE)) / 2.0
    pass_hz = float(RESAMPLER_PASS_HZ)
    if not (pass_hz < stop_hz):
        raise ResamplerCoeffError(
            f"degenerate band edges: pass_hz={pass_hz} stop_hz={stop_hz}"
        )
    fc = ((pass_hz + stop_hz) / 2.0) / (float(fs_in) * float(up))
    if not (0.0 < fc < 0.5):
        raise ResamplerCoeffError(f"fc out of open unit interval: {fc}")

    n = np.arange(num_taps, dtype=np.float64)
    # numpy.kaiser(M+1, beta) == window over n=0..M
    window = np.kaiser(num_taps, float(KAISER_BETA)).astype(np.float64, copy=False)
    arg = 2.0 * fc * (n - (m / 2.0))
    h0 = (2.0 * fc) * np.sinc(arg) * window
    s = float(np.sum(h0))
    if not np.isfinite(s) or s == 0.0:
        raise ResamplerCoeffError(f"degenerate h0 sum: {s!r}")
    h = (float(up) * h0) / s
    if not np.all(np.isfinite(h)):
        raise ResamplerCoeffError("non-finite coefficient produced")
    return h
