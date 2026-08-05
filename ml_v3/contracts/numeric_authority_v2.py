"""REV8 O-18 — canonical binary64 and published-reduction authority.

This module is a candidate-only implementation of the signed Round 2.3
numeric rules.  It is intentionally absent from ``ml_v3.contracts.__init__``:
REV7 remains the live dispatcher until the atomic activation gate.

Two domains are kept explicit:

* integers and optimizer quantities use exact :class:`fractions.Fraction`;
* published binary64 reductions use the pinned ``sum_pairwise64`` tree and
  round only at the contract boundaries.

The public representation of one binary64 value is the N64 token
``f64:<16 lowercase hexadecimal digits>``.
"""
from __future__ import annotations

import math
import re
import struct
from fractions import Fraction
from typing import Any, Iterable, Literal

from .exact_arith_v2 import exact
from .normalize_v2 import n64, normalized_canonical_bytes

__all__ = [
    "NumericAuthorityError",
    "P95_QUANTILE",
    "exact_n64",
    "float_from_n64",
    "mean64",
    "n64_from_bits",
    "p95_type7_64",
    "rn64",
    "sum_pairwise64",
]

_N64_RE = re.compile(r"f64:([0-9a-f]{16})\Z")
_MEAN_KEY_DOMAINS = frozenset({"canonical", "utf8"})

# §12 writes the Type-7 quantile as "q=0.95" without saying whether that
# denotes the exact rational or the binary64 nearest to it.  They are not the
# same number: binary64(0.95) is 4278419646001971/4503599627370496, strictly
# below 19/20.  The choice is observable, not academic — whenever G-1 is a
# multiple of 20 (G = 21, 41, 61, ...; 49 such G below 1000) the exact
# rational puts h on an integer, so Type-7 selects x[j] with no interpolation,
# while the binary64 sits one epsilon under and interpolates between x[j-1]
# and x[j].  On a distribution with a tail jump — precisely what a p95
# measures — the two publish different binary64 values.
#
# Resolved by the scientific authority (Marco, 2026-08-05): the exact rational.
# §2.3 item 7 requires rational interpolation with a single rounding, and
# admitting binary64(0.95) would inject a rounding *before* the interpolation;
# and Type-7 is defined to land exactly on x[j] when h is integral.
P95_QUANTILE = Fraction(19, 20)


class NumericAuthorityError(ValueError):
    """Raised when an O-18 numeric value is malformed or non-canonical."""


def float_from_n64(token: Any) -> float:
    """Decode one canonical finite N64 token to its binary64 value."""
    if not isinstance(token, str):
        raise NumericAuthorityError(
            f"N64 token must be str, got {type(token).__name__}")
    match = _N64_RE.fullmatch(token)
    if match is None:
        raise NumericAuthorityError(
            "N64 token must be f64: plus 16 lowercase hexadecimal digits")
    value = struct.unpack(">d", bytes.fromhex(match.group(1)))[0]
    if not math.isfinite(value):
        raise NumericAuthorityError("N64 token must encode a finite value")
    if value == 0.0 and token != "f64:0000000000000000":
        raise NumericAuthorityError("negative zero is not canonical N64")
    return value


def n64_from_bits(bits: Any) -> str:
    """Convert canonical ``0x<16 hex>`` artifact bits to an N64 token."""
    if not isinstance(bits, str) or not re.fullmatch(
            r"0x[0-9a-f]{16}", bits):
        raise NumericAuthorityError(
            "binary64 bits must be 0x plus 16 lowercase hexadecimal digits")
    token = "f64:" + bits[2:]
    # Validate finite/canonical-zero semantics before returning it.
    float_from_n64(token)
    return token


def exact_n64(token: Any) -> Fraction:
    """Return exactly the rational represented by a canonical N64 token."""
    return exact(float_from_n64(token))


def rn64(value: Any) -> str:
    """Correctly round an exact integer/fraction to canonical binary64 N64."""
    if isinstance(value, bool):
        raise NumericAuthorityError("RN64 rejects booleans")
    if isinstance(value, int):
        rational = Fraction(value)
    elif isinstance(value, Fraction):
        rational = value
    else:
        raise NumericAuthorityError(
            f"RN64 accepts int or Fraction, got {type(value).__name__}")
    try:
        rounded = float(rational)
    except OverflowError as exc:
        raise NumericAuthorityError("RN64 overflow") from exc
    if not math.isfinite(rounded):
        raise NumericAuthorityError("RN64 produced a non-finite value")
    return n64(rounded)


def _add64(left: str, right: str) -> str:
    """One binary64 addition, rounded once by the CPython binary64 runtime."""
    return n64(float_from_n64(left) + float_from_n64(right))


def sum_pairwise64(values: Iterable[str]) -> str:
    """Pinned pairwise binary64 reduction for published metrics.

    An empty reduction is not a number: the caller owns the metric-specific
    N/A/FAIL decision.
    """
    items = tuple(values)
    if not items:
        raise NumericAuthorityError(
            "sum_pairwise64([]) is metric-specific N/A or FAIL")
    for item in items:
        float_from_n64(item)

    def reduce_span(start: int, end: int) -> str:
        length = end - start
        if length == 1:
            return items[start]
        middle = start + length // 2
        return _add64(
            reduce_span(start, middle),
            reduce_span(middle, end),
        )

    return reduce_span(0, len(items))


def p95_type7_64(values: Iterable[str]) -> str | None:
    """Diagnostic Type-7 p95 over already-publishable binary64 values.

    §12, verbatim, with ``q`` fixed to :data:`P95_QUANTILE`::

        G=0 -> N/A
        G=1 -> x[0]
        G>=2:
          h = (G-1)*q
          j = floor(h)
          gamma = h-j
          p95 = (1-gamma)*x[j] + gamma*x[j+1]

    Per §2.3 item 7 the ordered binary64 inputs are read as exact rationals,
    the interpolation is exact, and the result is rounded exactly once at the
    end.  No intermediate value is materialized as binary64.

    Sorting is by exact numeric value, not by token: two distinct tokens
    cannot denote the same finite binary64, and equal values are
    interchangeable, so the order is total on the published result.

    This statistic is **diagnostic**.  §12 states a future p95 gate requires
    frozen floors and a power analysis, neither of which exists; nothing here
    makes p95 gating.

    Empty input is N/A and returns ``None``, matching :func:`mean64`.
    """
    exact_values = sorted(exact_n64(value) for value in values)
    count = len(exact_values)
    if count == 0:
        return None
    if count == 1:
        return rn64(exact_values[0])
    position = (count - 1) * P95_QUANTILE
    index = position.numerator // position.denominator
    gamma = position - index
    if gamma == 0:
        return rn64(exact_values[index])
    return rn64(
        (1 - gamma) * exact_values[index]
        + gamma * exact_values[index + 1]
    )


def _mean_order_bytes(
    key: Any,
    *,
    key_domain: Literal["canonical", "utf8"],
) -> bytes:
    if key_domain == "canonical":
        return normalized_canonical_bytes(key)
    if key_domain == "utf8":
        if not isinstance(key, str):
            raise NumericAuthorityError(
                "mean64 utf8 keys must be strings")
        return key.encode("utf-8")
    raise NumericAuthorityError(
        f"unknown mean64 key domain: {key_domain!r}")


def mean64(
    entries: Iterable[tuple[Any, str]],
    *,
    key_domain: Literal["canonical", "utf8"] = "canonical",
) -> str | None:
    """Hierarchical mean primitive over already-publishable binary64 values.

    ``canonical`` orders structured normative keys by normalized canonical
    bytes.  ``utf8`` is the separately signed macro-group rule and orders raw
    ``group_id`` bytes, never JSON-escaped string bytes.

    Equal order keys with different values are not total and therefore FAIL
    materialization.  Equal keys with equal values remain order-independent.
    The pairwise sum is rounded at every tree addition; the final exact
    division is rounded once.  Empty input represents N/A and returns
    ``None``.
    """
    if key_domain not in _MEAN_KEY_DOMAINS:
        raise NumericAuthorityError(
            f"unknown mean64 key domain: {key_domain!r}")
    materialized: list[tuple[bytes, str]] = []
    for key, value in entries:
        float_from_n64(value)
        materialized.append((
            _mean_order_bytes(key, key_domain=key_domain),
            value,
        ))
    if not materialized:
        return None
    materialized.sort(key=lambda item: item[0])
    for left, right in zip(materialized, materialized[1:]):
        if left[0] == right[0] and left[1] != right[1]:
            raise NumericAuthorityError(
                "mean64 normative key is not total: equal key has "
                "different values")
    total64 = sum_pairwise64(value for _, value in materialized)
    return rn64(exact_n64(total64) / len(materialized))
