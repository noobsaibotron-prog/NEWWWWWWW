"""Exact, non-authoritative corpus power reality check.

This lab-only helper evaluates the clean-actionable-rate clause without
materialising a benchmark power plan.  It deliberately does not choose the
number or order of primary gates, inspect a pilot, assign split roles, or emit
an ``aieq-v3-benchmark-power-plan-1`` artifact.

For a frozen multiplicity ``m`` the clause is:

* null boundary ``p0 = 0.02``;
* design alternative ``p1 = 0.01``;
* lower-tail rejection ``X <= k``;
* size ``P(X <= k | p0) <= 0.05 / m``;
* power ``P(X <= k | p1) >= 0.90``;
* search every integer ``n`` beginning at the contractual floor 149.

All pass/fail comparisons use integers.  No binary floating-point value can
move a boundary.  Decimal strings are report-only renderings of exact ratios.
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from decimal import Decimal, localcontext
from typing import Iterable, Sequence


CONTRACT_FLOOR = 149
NULL_EVENT_NUMERATOR = 1
NULL_EVENT_DENOMINATOR = 50
ALTERNATIVE_EVENT_NUMERATOR = 1
ALTERNATIVE_EVENT_DENOMINATOR = 100
ALPHA_FAMILY_NUMERATOR = 1
ALPHA_FAMILY_DENOMINATOR_FACTOR = 20
POWER_NUMERATOR = 9
POWER_DENOMINATOR = 10
REPORT_SCHEMA = "aieq-corpus-clean-power-reality-check-1"


class PowerRealityError(ValueError):
    """Raised when diagnostic inputs cannot represent the frozen clause."""


@dataclass(frozen=True)
class ExactTail:
    numerator: int
    denominator: int

    def decimal(self, digits: int = 12) -> str:
        with localcontext() as context:
            context.prec = max(32, digits + 8)
            value = Decimal(self.numerator) / Decimal(self.denominator)
            return format(value, f".{digits}f")


@dataclass(frozen=True)
class CleanPowerResult:
    m: int
    n: int
    k: int
    size: ExactTail
    power: ExactTail

    def as_dict(self) -> dict[str, object]:
        return {
            "alpha_plan_exact": f"1/{ALPHA_FAMILY_DENOMINATOR_FACTOR * self.m}",
            "comparison_arithmetic": "exact integer cross-multiplication",
            "k": self.k,
            "m": self.m,
            "n_power_diagnostic": self.n,
            "power_decimal": self.power.decimal(),
            "power_relation": ">=9/10",
            "size_decimal": self.size.decimal(),
            "size_relation": (
                f"<=1/{ALPHA_FAMILY_DENOMINATOR_FACTOR * self.m}"
            ),
        }


def _require_positive_int(value: object, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise PowerRealityError(f"{name} must be a positive integer")
    return value


def _binomial_term(n: int, k: int, event_denominator: int) -> int:
    """Return the numerator term for p=1/event_denominator."""

    return math.comb(n, k) * (event_denominator - 1) ** (n - k)


def exact_binomial_cdf(n: int, k: int, event_denominator: int) -> ExactTail:
    """Return ``P(X <= k)`` exactly for ``X~Bin(n, 1/event_denominator)``."""

    n = _require_positive_int(n, "n")
    event_denominator = _require_positive_int(
        event_denominator, "event_denominator"
    )
    if event_denominator <= 1:
        raise PowerRealityError("event_denominator must be greater than 1")
    if isinstance(k, bool) or not isinstance(k, int) or not 0 <= k <= n:
        raise PowerRealityError("k must be an integer in [0, n]")
    numerator = sum(
        _binomial_term(n, index, event_denominator)
        for index in range(k + 1)
    )
    return ExactTail(numerator=numerator, denominator=event_denominator**n)


def _passes(m: int, size: ExactTail, power: ExactTail) -> bool:
    size_ok = (
        size.numerator * ALPHA_FAMILY_DENOMINATOR_FACTOR * m
        <= size.denominator * ALPHA_FAMILY_NUMERATOR
    )
    power_ok = (
        power.numerator * POWER_DENOMINATOR
        >= power.denominator * POWER_NUMERATOR
    )
    return size_ok and power_ok


def feasible_thresholds(n: int, m: int) -> tuple[CleanPowerResult, ...]:
    """Return every lower-tail threshold satisfying the exact clause at ``n``."""

    n = _require_positive_int(n, "n")
    m = _require_positive_int(m, "m")
    null_denominator = NULL_EVENT_DENOMINATOR**n
    alternative_denominator = ALTERNATIVE_EVENT_DENOMINATOR**n
    null_numerator = 0
    alternative_numerator = 0
    results: list[CleanPowerResult] = []

    for k in range(n + 1):
        null_numerator += _binomial_term(n, k, NULL_EVENT_DENOMINATOR)
        alternative_numerator += _binomial_term(
            n, k, ALTERNATIVE_EVENT_DENOMINATOR
        )
        size = ExactTail(null_numerator, null_denominator)
        power = ExactTail(alternative_numerator, alternative_denominator)
        if _passes(m, size, power):
            results.append(CleanPowerResult(m=m, n=n, k=k, size=size, power=power))
        if (
            null_numerator * ALPHA_FAMILY_DENOMINATOR_FACTOR * m
            > null_denominator * ALPHA_FAMILY_NUMERATOR
        ):
            break
    return tuple(results)


def first_clean_actionable_n(
    m: int,
    *,
    start_n: int = CONTRACT_FLOOR,
    max_n: int = 10_000,
) -> CleanPowerResult:
    """Find the first integer ``n`` satisfying the frozen analytic clause."""

    m = _require_positive_int(m, "m")
    start_n = _require_positive_int(start_n, "start_n")
    max_n = _require_positive_int(max_n, "max_n")
    if start_n < CONTRACT_FLOOR:
        raise PowerRealityError(
            f"start_n must be at least the contractual floor {CONTRACT_FLOOR}"
        )
    if max_n < start_n:
        raise PowerRealityError("max_n must be greater than or equal to start_n")
    for n in range(start_n, max_n + 1):
        feasible = feasible_thresholds(n, m)
        if feasible:
            return feasible[-1]
    raise PowerRealityError(
        f"no feasible n found for m={m} in [{start_n}, {max_n}]"
    )


def sensitivity_report(
    multiplicities: Iterable[int],
    *,
    start_n: int = CONTRACT_FLOOR,
    max_n: int = 10_000,
) -> dict[str, object]:
    values = tuple(_require_positive_int(value, "m") for value in multiplicities)
    if not values:
        raise PowerRealityError("at least one multiplicity is required")
    if len(set(values)) != len(values):
        raise PowerRealityError("multiplicities must be unique")
    rows = [
        first_clean_actionable_n(m, start_n=start_n, max_n=max_n).as_dict()
        for m in sorted(values)
    ]
    return {
        "authority": "diagnostic-only; does not freeze m or create a power plan",
        "contract_floor": CONTRACT_FLOOR,
        "design_alternative_exact": "1/100",
        "null_boundary_exact": "1/50",
        "power_target_exact": "9/10",
        "rows": rows,
        "schema": REPORT_SCHEMA,
        "search_domain": {"max_n_inclusive": max_n, "start_n_inclusive": start_n},
    }


def canonical_json(report: dict[str, object]) -> str:
    return json.dumps(
        report,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ) + "\n"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--m", nargs="+", required=True, type=int)
    parser.add_argument("--start-n", type=int, default=CONTRACT_FLOOR)
    parser.add_argument("--max-n", type=int, default=10_000)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        report = sensitivity_report(
            arguments.m,
            start_n=arguments.start_n,
            max_n=arguments.max_n,
        )
    except PowerRealityError as error:
        raise SystemExit(f"NO-GO: {error}") from error
    print(canonical_json(report), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
