"""REV8 §10.0 item 5 — exact arithmetic for the matching objective.

Authority: docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md §10.0.

    "Le somme nell'obiettivo del matching sono poi confrontate come razionali
     esatti dei rispettivi bit pattern binary64. Questa aritmetica e distinta da
     `sum_pairwise64`, che resta l'operatore delle metriche finali."

CANDIDATE, NOT ACTIVE. Reachable only by explicit import path; not exported by
`ml_v3.contracts.__init__` until the atomic switch of §14.1.

Two summation operators, and why neither can do the other's job
---------------------------------------------------------------
§10.2 forbids an ordering that depends on record order. A solver that compares
candidate matchings by floating-point sums inherits whatever its summation
operator does with ordering, and the three available operators do not agree:

    values = {1e16, 1.0, -1e16, 0.5}, all 24 permutations, CPython 3.12.13
        sum() builtin      -> {1.5}            (compensated since 3.12)
        math.fsum          -> {1.5}
        plain left fold    -> {0.0, 0.5, 1.0, 1.5, 2.0}

The builtin happens to be stable here, so today's matcher is not visibly broken.
That is not a guarantee to build a contract on: compensation is a CPython
implementation detail, it is still not exact, and it is a third operator again
from the `sum_pairwise64` recursion §10.0 pins for published metrics. An
ordering decision must not depend on which of the three a backend reaches for.

Exact rationals settle it: every binary64 value has an exact rational form, the
sum of rationals is associative and commutative, so the comparison is the same
whatever order the terms arrive in — on any backend, in any version.

`sum_pairwise64` is deliberately absent from this module. It is the published
metrics operator, its rounding is part of the reported number, and §10.0 forbids
substituting anything else for it. Landing the two in one module would invite
exactly the confusion the contract separates them to prevent.
"""
from __future__ import annotations

import math
from fractions import Fraction
from typing import Any, Iterable

__all__ = [
    "ExactArithError",
    "exact",
    "exact_sum",
    "exact_tuple",
]


class ExactArithError(ValueError):
    """Raised on a value that has no exact binary64 rational form."""


def exact(value: Any) -> Fraction:
    """The exact rational value of one binary64 number."""
    if isinstance(value, bool):
        raise ExactArithError("exact() rejects booleans")
    if not isinstance(value, (int, float)):
        raise ExactArithError(
            f"exact() accepts only numbers, got {type(value).__name__}")
    try:
        number = float(value)
    except OverflowError as exc:
        raise ExactArithError(f"overflow converting {value!r} to binary64") from exc
    if not math.isfinite(number):
        raise ExactArithError(f"exact() requires a finite value, got {number!r}")
    return Fraction(*number.as_integer_ratio())


def exact_sum(values: Iterable[Any]) -> Fraction:
    """Sum of binary64 values as an exact rational — independent of term order.

    An empty sum is zero: unlike `sum_pairwise64`, whose N == 0 case §10.0 hands
    back to the caller as `N/A` or FAIL, an objective term over no matched pairs
    is a genuine zero and comparing it is meaningful.
    """
    total = Fraction(0)
    for value in values:
        total += exact(value)
    return total


def exact_tuple(values: Iterable[Any]) -> tuple[Fraction, ...]:
    """Each value as an exact rational, for lexicographic comparison of keys."""
    return tuple(exact(value) for value in values)
