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
from typing import Any, Iterable

from .exact_arith_v2 import exact
from .normalize_v2 import n64, normalized_canonical_bytes

__all__ = [
    "NumericAuthorityError",
    "exact_n64",
    "float_from_n64",
    "mean64",
    "n64_from_bits",
    "rn64",
    "sum_pairwise64",
]

_N64_RE = re.compile(r"f64:([0-9a-f]{16})\Z")


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


def mean64(entries: Iterable[tuple[Any, str]]) -> str | None:
    """Hierarchical mean primitive over already-publishable binary64 values.

    Entries are sorted by canonical bytes of their normative key.  The
    pairwise sum is rounded at every tree addition; the final exact division
    is rounded once.  Empty input represents N/A and returns ``None``.
    """
    materialized: list[tuple[bytes, str]] = []
    for key, value in entries:
        float_from_n64(value)
        materialized.append((normalized_canonical_bytes(key), value))
    if not materialized:
        return None
    materialized.sort(key=lambda item: item[0])
    total64 = sum_pairwise64(value for _, value in materialized)
    return rn64(exact_n64(total64) / len(materialized))
