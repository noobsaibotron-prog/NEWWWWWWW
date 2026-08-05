"""REV8 O-13 — support accounting: G_eligible, G_defined, G_NA.

This module is candidate-only and is deliberately absent from
``ml_v3.contracts.__init__``: REV7 remains the live dispatcher until the
atomic activation gate.

§12 requires, for every metric::

    G_eligible
    G_defined
    G_NA = G_eligible \\ G_defined
    reason code per gruppo N/A

with three properties that this module enforces structurally rather than by
convention, because each of them is a way the accounting could be gamed or
quietly corrupted:

* **G_eligible is frozen before predictions are read.**  §12 fixes it from
  manifest, ``complete_types`` and GT.  :func:`account_support` therefore
  takes the frozen set as an input and rejects any outcome naming a unit
  outside it — an evaluator cannot enlarge its own denominator after seeing
  how it scored.
* **Every eligible unit must be accounted for.**  A missing outcome is a
  materialization failure, not an implicit N/A, so a metric cannot silently
  drop the units it did badly on.
* **N/A units are never zero.**  §12 says so explicitly.  This module never
  produces a value for an N/A unit, so no downstream mean can pick one up;
  the defined and N/A populations are returned as disjoint sets.

The group-level reason code is reported as the **canonically ordered set of
distinct unit reasons**, not as a single elected code.  §12 asks for "reason
code per gruppo N/A" but pins no precedence among disagreeing units, and
inventing one would be an unsigned normative choice.  A set is a superset of
whatever selection rule a later amendment fixes, so nothing is lost and
nothing is fabricated.

Gate floors are **not** applied here.  §12 subordinates the macro-mean to
recall and coverage gates with preregistered floors; those floors do not
exist yet, and this module reports support rather than deciding PASS.
"""
from __future__ import annotations

from dataclasses import dataclass

from .normalize_v2 import normalized_canonical_bytes

__all__ = [
    "SUPPORT_AUTHORITY_STATUS",
    "SupportAccounting",
    "SupportError",
    "UnitOutcome",
    "account_support",
]

SUPPORT_AUTHORITY_STATUS = "SUPPORT_ACCOUNTED_GATE_FLOORS_NOT_EVALUATED"


class SupportError(ValueError):
    """Malformed or non-materializable support input."""


@dataclass(frozen=True)
class UnitOutcome:
    """One evaluation unit's verdict for one metric.

    ``reason`` is required exactly when ``defined`` is false: a defined unit
    has nothing to explain, and an N/A unit that explains nothing cannot be
    published under §12.
    """

    group_id: str
    unit_key: tuple[str, ...]
    defined: bool
    reason: str | None = None

    def __post_init__(self) -> None:
        _validate_group_id(self.group_id)
        _validate_unit_key(self.unit_key)
        if not isinstance(self.defined, bool):
            raise SupportError("UnitOutcome.defined must be a bool")
        if self.defined and self.reason is not None:
            raise SupportError(
                "a defined unit must not carry an N/A reason code")
        if not self.defined:
            if not isinstance(self.reason, str) or not self.reason:
                raise SupportError(
                    "an N/A unit must carry a non-empty reason code")
            if "\x00" in self.reason:
                raise SupportError("reason code must be NUL-free")


@dataclass(frozen=True)
class SupportAccounting:
    """Published §12 support for one metric.

    ``g_eligible``/``g_defined``/``g_na`` are canonically ordered group_id
    tuples, not counts, so ``G_NA = G_eligible \\ G_defined`` is checkable by
    a reader rather than asserted.  Counts remain derivable with ``len``.
    """

    g_eligible: tuple[str, ...]
    g_defined: tuple[str, ...]
    g_na: tuple[str, ...]
    na_reasons: tuple[tuple[str, tuple[str, ...]], ...]
    defined_unit_count: int
    na_unit_count: int
    authority_status: str = SUPPORT_AUTHORITY_STATUS


def _validate_group_id(value: object) -> None:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise SupportError("group_id must be a non-empty NUL-free string")


def _validate_unit_key(value: object) -> None:
    if not isinstance(value, tuple) or not value:
        raise SupportError("unit_key must be a non-empty tuple")
    for part in value:
        if not isinstance(part, str) or not part or "\x00" in part:
            raise SupportError(
                "unit_key parts must be non-empty NUL-free strings")


def _group_order(group_id: str) -> bytes:
    """§12 orders groups in the macro reduction by raw UTF-8 group_id bytes."""
    return group_id.encode("utf-8")


def account_support(
    eligible_units: tuple[tuple[str, tuple[str, ...]], ...],
    outcomes: tuple[UnitOutcome, ...],
) -> SupportAccounting:
    """Materialize §12 support from a frozen eligible set and unit outcomes.

    ``eligible_units`` is the frozen ``(group_id, evaluation_unit_key)``
    population, fixed from manifest, ``complete_types`` and GT before any
    prediction is read.  ``outcomes`` must cover it exactly: one outcome per
    eligible unit, no outcome for anything else.

    Raises :class:`SupportError` — never silently degrades — when the two
    populations disagree, because both directions are exactly the failure
    §12's freeze exists to prevent.
    """
    if not isinstance(eligible_units, tuple):
        raise SupportError("eligible_units must be a tuple")
    if not isinstance(outcomes, tuple):
        raise SupportError("outcomes must be a tuple")

    eligible: set[tuple[str, tuple[str, ...]]] = set()
    for entry in eligible_units:
        if not isinstance(entry, tuple) or len(entry) != 2:
            raise SupportError(
                "eligible_units entries must be (group_id, unit_key)")
        group_id, unit_key = entry
        _validate_group_id(group_id)
        _validate_unit_key(unit_key)
        if (group_id, unit_key) in eligible:
            raise SupportError(
                f"duplicate eligible unit {group_id!r}/{unit_key!r}")
        eligible.add((group_id, unit_key))

    seen: set[tuple[str, tuple[str, ...]]] = set()
    defined_groups: set[str] = set()
    reasons_by_group: dict[str, set[str]] = {}
    defined_unit_count = 0
    na_unit_count = 0

    for outcome in outcomes:
        if not isinstance(outcome, UnitOutcome):
            raise SupportError("outcomes must contain UnitOutcome values")
        identity = (outcome.group_id, outcome.unit_key)
        if identity not in eligible:
            raise SupportError(
                "outcome for a unit outside the frozen eligible set: "
                f"{outcome.group_id!r}/{outcome.unit_key!r}")
        if identity in seen:
            raise SupportError(
                f"duplicate outcome for {outcome.group_id!r}/"
                f"{outcome.unit_key!r}")
        seen.add(identity)
        if outcome.defined:
            defined_unit_count += 1
            defined_groups.add(outcome.group_id)
        else:
            na_unit_count += 1
            assert outcome.reason is not None  # enforced in __post_init__
            reasons_by_group.setdefault(outcome.group_id, set()).add(
                outcome.reason)

    missing = eligible - seen
    if missing:
        sample = sorted(f"{group}/{key}" for group, key in missing)[:3]
        raise SupportError(
            f"{len(missing)} eligible unit(s) have no outcome; "
            f"a missing outcome is not an implicit N/A: {sample}")

    g_eligible = tuple(sorted(
        {group_id for group_id, _unit_key in eligible}, key=_group_order))
    g_defined = tuple(sorted(defined_groups, key=_group_order))
    g_na = tuple(sorted(
        set(g_eligible) - defined_groups, key=_group_order))

    na_reasons = tuple(
        (group_id, tuple(sorted(
            reasons_by_group.get(group_id, set()),
            key=normalized_canonical_bytes,
        )))
        for group_id in g_na
    )

    return SupportAccounting(
        g_eligible=g_eligible,
        g_defined=g_defined,
        g_na=g_na,
        na_reasons=na_reasons,
        defined_unit_count=defined_unit_count,
        na_unit_count=na_unit_count,
    )
