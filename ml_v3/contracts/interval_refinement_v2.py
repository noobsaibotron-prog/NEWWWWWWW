"""REV8 candidate-only interval refinement for correctly-rounded binary64.

The signed contract permits transcendental quantities only when an interval is
refined until every value in the interval rounds to the same binary64 result.
This module implements that rule on top of the locked ``mpmath==1.3.0``
interval context.  It is offline/control-path code, never audio-thread code.
"""
from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction
from typing import Callable

import mpmath as mp

from .numeric_authority_v2 import NumericAuthorityError, rn64

__all__ = [
    "IntervalRefinementError",
    "RefinementResult",
    "round_log2_fraction",
    "round_power_fraction",
    "round_sqrt_fraction",
]


class IntervalRefinementError(ValueError):
    """Raised when a signed transcendental cannot be certified."""


@dataclass(frozen=True)
class RefinementResult:
    """One correctly-rounded result and its refinement certificate."""

    n64: str
    precision_dps: int
    lower: Fraction
    upper: Fraction


def _mpi_value(value: tuple[int, int, int, int]) -> Fraction:
    sign, mantissa, exponent, _bit_count = value
    numerator = -mantissa if sign else mantissa
    if exponent >= 0:
        return Fraction(numerator << exponent)
    return Fraction(numerator, 1 << (-exponent))


def _bounds(value: object) -> tuple[Fraction, Fraction]:
    try:
        lower_raw, upper_raw = value._mpi_  # type: ignore[attr-defined]
    except AttributeError as exc:
        raise IntervalRefinementError(
            "mpmath interval result has no binary endpoint certificate") from exc
    return _mpi_value(lower_raw), _mpi_value(upper_raw)


def _iv_fraction(value: Fraction) -> object:
    # Construction from numerator and denominator is interval-safe.  At low
    # precision either integer may be outward-rounded, so the quotient remains
    # an enclosure rather than pretending to be exact.
    return mp.iv.mpf(value.numerator) / mp.iv.mpf(value.denominator)


def _refine(
    operation: Callable[[], object],
    *,
    label: str,
    initial_dps: int = 80,
    maximum_dps: int = 1280,
) -> RefinementResult:
    if initial_dps <= 0 or maximum_dps < initial_dps:
        raise IntervalRefinementError("invalid precision range")
    previous_dps = mp.iv.dps
    try:
        dps = initial_dps
        while dps <= maximum_dps:
            mp.iv.dps = dps
            interval = operation()
            lower, upper = _bounds(interval)
            if lower > upper:
                raise IntervalRefinementError(
                    f"{label} produced an inverted interval")
            try:
                low_token = rn64(lower)
                high_token = rn64(upper)
            except NumericAuthorityError as exc:
                raise IntervalRefinementError(
                    f"{label} cannot round to finite binary64") from exc
            if low_token == high_token:
                return RefinementResult(
                    n64=low_token,
                    precision_dps=dps,
                    lower=lower,
                    upper=upper,
                )
            dps *= 2
    finally:
        mp.iv.dps = previous_dps
    raise IntervalRefinementError(
        f"{label} did not converge by {maximum_dps} decimal digits")


def round_power_fraction(
    base: Fraction,
    exponent: Fraction,
    *,
    scale: Fraction = Fraction(1),
    label: str = "power",
) -> RefinementResult:
    """Correctly round ``scale * base**exponent`` for positive ``base``."""
    if base <= 0:
        raise IntervalRefinementError("power base must be positive")

    def operation() -> object:
        return (
            _iv_fraction(scale)
            * mp.iv.power(_iv_fraction(base), _iv_fraction(exponent))
        )

    return _refine(operation, label=label)


def round_sqrt_fraction(
    value: Fraction,
    *,
    label: str = "sqrt",
) -> RefinementResult:
    """Correctly round ``sqrt(value)`` for non-negative exact ``value``."""
    if value < 0:
        raise IntervalRefinementError("sqrt input must be non-negative")
    return _refine(
        lambda: mp.iv.sqrt(_iv_fraction(value)),
        label=label,
    )


def round_log2_fraction(
    value: Fraction,
    *,
    scale: Fraction = Fraction(1),
    label: str = "log2",
) -> RefinementResult:
    """Correctly round ``scale * log2(value)`` for positive exact ``value``."""
    if value <= 0:
        raise IntervalRefinementError("log2 input must be positive")

    def operation() -> object:
        operand = _iv_fraction(value)
        return (
            _iv_fraction(scale)
            * mp.iv.ln(operand)
            / mp.iv.ln(mp.iv.mpf(2))
        )

    return _refine(operation, label=label)
