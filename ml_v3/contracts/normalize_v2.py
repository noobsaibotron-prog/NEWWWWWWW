"""REV8 §9.5 — numeric normalisation `N64`.

Authority: docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md §9.5.

    "`N64` accetta esclusivamente un valore ammesso da un campo JSON Schema
     `number`, mai un booleano. Il token viene convertito a IEEE-754 binary64
     con round-to-nearest, ties-to-even. Overflow, NaN e infinito sono FAIL.
     Se il risultato e zero, il bit di segno viene posto a zero.

     La forma serializzata e `f64:` seguita dai sedici caratteri esadecimali
     minuscoli del bit pattern binary64 big-endian."

CANDIDATE, NOT ACTIVE. The live contract is REV7. This module is reachable only
by explicit import path; `ml_v3.contracts.__init__` does not export it, and must
not, until the atomic switch of §14.1.

Why a normalisation exists at all
---------------------------------
Identity, deduplication and ordering all hash canonical bytes. Canonical JSON
alone is not enough for that job: it distinguishes tokens that denote the same
binary64 value. Measured on this repo's `canonical_bytes`:

    canonical_bytes([100])    -> b'[100]\\n'
    canonical_bytes([100.0])  -> b'[100.0]\\n'
    canonical_bytes([-0.0])   -> b'[-0.0]\\n'
    canonical_bytes([0.0])    -> b'[0.0]\\n'

A submitter writing `100` instead of `100.0` for the same band edge would have
obtained a different record id. N64 removes that lever by hashing the value, not
the token.
"""
from __future__ import annotations

import math
import struct
from typing import Any

from .canonical import canonical_bytes

__all__ = [
    "N64_PREFIX",
    "NormalizationError",
    "n64",
    "normalized_canonical_bytes",
]

N64_PREFIX = "f64:"


class NormalizationError(ValueError):
    """Raised on a value N64 refuses to normalise."""


def n64(value: Any) -> str:
    """Normalise one JSON `number` to its `f64:<16 hex>` canonical form."""
    # A JSON boolean is not a number, and in Python it would silently pass an
    # isinstance(int) check — so it is rejected before anything else.
    if isinstance(value, bool):
        raise NormalizationError("N64 rejects booleans; a JSON boolean is not a number")
    if not isinstance(value, (int, float)):
        raise NormalizationError(
            f"N64 accepts only JSON numbers, got {type(value).__name__}")
    try:
        number = float(value)
    except OverflowError as exc:
        raise NormalizationError(
            f"N64 overflow converting {value!r} to binary64") from exc
    if math.isnan(number):
        raise NormalizationError("N64 rejects NaN")
    if math.isinf(number):
        raise NormalizationError("N64 rejects infinity")
    if number == 0.0:
        # Not a no-op: -0.0 == 0.0 is true, so this clears the sign bit and
        # makes the two zeros share one serialisation, as §9.5 requires.
        number = 0.0
    return N64_PREFIX + struct.pack(">d", number).hex()


def _reject_raw_numbers(node: Any, path: str) -> None:
    """Refuse any float left un-normalised anywhere in a structure.

    Integers are allowed: §9.5 keeps `integer` fields — `occurrence_ordinal`
    among them — as JSON integers. A float, by contrast, can only be a `number`
    field that skipped N64, and hashing it would reintroduce the token
    sensitivity N64 exists to remove. Fail-closed beats hashing it quietly.
    """
    if isinstance(node, bool):
        return
    if isinstance(node, float):
        raise NormalizationError(
            f"un-normalised float at {path}: apply n64() before hashing")
    if isinstance(node, dict):
        for key, child in node.items():
            _reject_raw_numbers(child, f"{path}.{key}")
    elif isinstance(node, (list, tuple)):
        for index, child in enumerate(node):
            _reject_raw_numbers(child, f"{path}[{index}]")


def normalized_canonical_bytes(obj: Any) -> bytes:
    """Canonical bytes of a structure whose `number` fields are already N64.

    §9.5: "I byte canonici normalizzati sono `canonical_bytes()` dopo
    l'applicazione ricorsiva di `N64` ai soli campi `number`". The guard makes
    that precondition checkable instead of assumed.
    """
    _reject_raw_numbers(obj, "$")
    return canonical_bytes(obj)
