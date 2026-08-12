"""REV8 O-13 / O13F_01+O13F_02 — hash-bound support-floor evaluation.

This module is candidate-only and deliberately absent from
``ml_v3.contracts.__init__``.  It consumes the support accounting produced by
``support_v2`` and answers one question only: is the preregistered independent
support sufficient for the metric's own gate to run?

``support_basis`` keeps corpus-only preconditions from becoming unsigned
defined-result floors while preserving both eligible and defined checks for
mandatory metric support.  ``SUPPORT_SUFFICIENT`` is not a metric PASS.  The
metric threshold, recall, coverage and clean-safety gates remain separate
consumers.
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any, Mapping

from .canonical import sha256_of_obj
from .normalize_v2 import normalized_canonical_bytes
from .support_v2 import SUPPORT_AUTHORITY_STATUS, SupportAccounting

__all__ = [
    "O13F_AUTHORITY_STATUS",
    "POLICY_REVISION",
    "POLICY_SCHEMA",
    "POPULATION_PLAN_SCHEMA",
    "SOURCE_CONTRACT_SHA256",
    "SUPPORT_BASIS_ELIGIBLE_AND_DEFINED",
    "SUPPORT_BASIS_ELIGIBLE_ONLY",
    "StratumFloorResult",
    "StratumPopulation",
    "SupportFloorError",
    "SupportFloorEvaluation",
    "evaluate_support_floors",
    "stratum_population_plan_sha256",
    "support_floor_policy_sha256",
]

POLICY_SCHEMA = "aieq-v3-rev8-o13-support-floor-policy-3"
POPULATION_PLAN_SCHEMA = "aieq-v3-rev8-o13-stratum-population-plan-1"
SOURCE_CONTRACT_SHA256 = (
    "398aea26daa6d48324f9df7e9dda54c38b332fb4fbfb230563d5c26172875745"
)
POLICY_REVISION = (
    "REV8-O13F-01@59bec34856a08aa43cc52bb123e904a44575f4f6"
    "+recheck@d0b9916cd08aff29dfde03ffe0c8f38c3656a96c"
    "+O13F-02@26f35e753f96ebc48d0453e432e8f2a3f5380687"
    "+recheck@1974f2f57a5bfa12b98c3aa6640ca58e2b825d5e"
)
O13F_AUTHORITY_STATUS = (
    "SUPPORT_FLOORS_AND_BASIS_EVALUATED_METRIC_GATE_NOT_EVALUATED"
)

SUPPORT_BASIS_ELIGIBLE_AND_DEFINED = "eligible_and_defined"
SUPPORT_BASIS_ELIGIBLE_ONLY = "eligible_only"

_POLICY_KEYS = frozenset({
    "schema",
    "contract_revision",
    "source_contract_sha256",
    "metric_id",
    "split_role",
    "mandatory",
    "strata",
    "population_plan_sha256",
    "power_plan_sha256",
})
_STRATUM_KEYS = frozenset({
    "stratum_id",
    "population_kind",
    "parent_stratum_id",
    "support_basis",
    "contract_floor",
    "power_required",
    "power_binding_kind",
    "power_binding_id",
    "n_power",
    "n_required",
    "max_parent_fraction_numerator",
    "max_parent_fraction_denominator",
})
_SUPPORT_BASES = frozenset({
    SUPPORT_BASIS_ELIGIBLE_AND_DEFINED,
    SUPPORT_BASIS_ELIGIBLE_ONLY,
})
_SPLIT_ROLES = frozenset({"calibration", "development-metric", "final-test"})
_POPULATION_KINDS = frozenset({
    "all_eligible_groups",
    "gt_positive_groups",
    "gt_negative_groups",
    "clean_groups",
    "clean_standard_minute_groups",
    "profile_groups",
    "source_family_groups",
    "tonal_region_direction_groups",
    "paired_groups",
})
_SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")

_REASON_ORDER = {
    "SUPPORT_POLICY_UNAVAILABLE": 0,
    "SUPPORT_ELIGIBLE_FLOOR_NOT_MET": 1,
    "SUPPORT_DEFINED_FLOOR_NOT_MET": 2,
    "SUPPORT_STRATUM_FLOOR_NOT_MET": 3,
}


class SupportFloorError(ValueError):
    """Fatal policy, hash or support-accounting materialization failure."""


@dataclass(frozen=True)
class _StratumRequirement:
    stratum_id: str
    population_kind: str
    parent_stratum_id: str | None
    support_basis: str
    contract_floor: int
    power_required: bool
    power_binding_kind: str | None
    power_binding_id: str | None
    n_power: int | None
    n_required: int
    max_parent_fraction_numerator: int | None
    max_parent_fraction_denominator: int | None


@dataclass(frozen=True)
class StratumPopulation:
    """Frozen group population for one policy stratum."""

    stratum_id: str
    group_ids: tuple[str, ...]

    def __post_init__(self) -> None:
        _require_text(self.stratum_id, "stratum_id")
        if not isinstance(self.group_ids, tuple):
            raise SupportFloorError("StratumPopulation.group_ids must be a tuple")
        for group_id in self.group_ids:
            _require_text(group_id, "group_id")
        if len(self.group_ids) != len(set(self.group_ids)):
            raise SupportFloorError(
                f"duplicate group_id in stratum {self.stratum_id!r}"
            )


@dataclass(frozen=True)
class StratumFloorResult:
    stratum_id: str
    population_kind: str
    parent_stratum_id: str | None
    support_basis: str
    contract_floor: int
    power_required: bool
    power_binding_kind: str | None
    power_binding_id: str | None
    n_power: int | None
    eligible_count: int
    defined_count: int
    n_required: int
    eligible_minimum_ok: bool | None
    defined_minimum_ok: bool | None
    eligible_ceiling_ok: bool | None
    defined_ceiling_ok: bool | None


@dataclass(frozen=True)
class SupportFloorEvaluation:
    """O13F support result, explicitly distinct from the metric gate."""

    metric_id: str
    split_role: str
    status: str
    support_sufficient: bool
    reason_codes: tuple[str, ...]
    policy_sha256: str | None
    population_plan_sha256: str | None
    strata: tuple[StratumFloorResult, ...]
    authority_status: str


def _require_text(value: object, label: str) -> str:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise SupportFloorError(f"{label} must be a non-empty NUL-free string")
    return value


def _require_sha256(value: object, label: str) -> str:
    if not isinstance(value, str) or _SHA256_RE.fullmatch(value) is None:
        raise SupportFloorError(f"{label} must be lowercase SHA-256 hex-64")
    return value


def _require_int(value: object, label: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise SupportFloorError(f"{label} must be an integer >= {minimum}")
    return value


def _exact_keys(value: Mapping[str, Any], expected: frozenset[str], label: str) -> None:
    actual = frozenset(value)
    if actual != expected:
        raise SupportFloorError(
            f"{label} exact-key mismatch: missing={sorted(expected-actual)}, "
            f"extra={sorted(actual-expected)}"
        )


def _parse_stratum(value: object, index: int) -> _StratumRequirement:
    label = f"strata[{index}]"
    if not isinstance(value, Mapping):
        raise SupportFloorError(f"{label} must be an object")
    _exact_keys(value, _STRATUM_KEYS, label)
    stratum_id = _require_text(value["stratum_id"], f"{label}.stratum_id")
    population_kind = _require_text(
        value["population_kind"], f"{label}.population_kind"
    )
    if population_kind not in _POPULATION_KINDS:
        raise SupportFloorError(f"{label}.population_kind is not canonical")
    parent = value["parent_stratum_id"]
    if parent is not None:
        parent = _require_text(parent, f"{label}.parent_stratum_id")
    support_basis = _require_text(
        value["support_basis"], f"{label}.support_basis"
    )
    if support_basis not in _SUPPORT_BASES:
        raise SupportFloorError(f"{label}.support_basis is not canonical")

    contract_floor = _require_int(
        value["contract_floor"], f"{label}.contract_floor"
    )
    power_required = value["power_required"]
    if not isinstance(power_required, bool):
        raise SupportFloorError(f"{label}.power_required must be a bool")
    power_binding_kind = value["power_binding_kind"]
    power_binding_id = value["power_binding_id"]
    n_power = value["n_power"]
    n_required = _require_int(value["n_required"], f"{label}.n_required")
    if power_required:
        if power_binding_kind not in ("gate", "family"):
            raise SupportFloorError(
                f"{label}.power_binding_kind must be gate or family"
            )
        power_binding_id = _require_text(
            power_binding_id, f"{label}.power_binding_id"
        )
        n_power = _require_int(n_power, f"{label}.n_power", minimum=1)
        if n_required != max(contract_floor, n_power):
            raise SupportFloorError(
                f"{label}.n_required must equal max(contract_floor, n_power)"
            )
    else:
        if power_binding_kind is not None or power_binding_id is not None:
            raise SupportFloorError(
                f"{label} unpowered stratum must not carry a power binding"
            )
        if n_power is not None:
            raise SupportFloorError(
                f"{label}.n_power must be null when power is not required"
            )
        if n_required != contract_floor:
            raise SupportFloorError(
                f"{label}.n_required must equal contract_floor without power"
            )

    numerator = value["max_parent_fraction_numerator"]
    denominator = value["max_parent_fraction_denominator"]
    if (numerator is None) != (denominator is None):
        raise SupportFloorError(
            f"{label} parent-fraction numerator and denominator must be paired"
        )
    if numerator is not None:
        numerator = _require_int(numerator, f"{label}.fraction_numerator")
        denominator = _require_int(
            denominator, f"{label}.fraction_denominator", minimum=1
        )
        if parent is None:
            raise SupportFloorError(
                f"{label} parent-fraction ceiling requires a parent stratum"
            )
        if numerator > denominator:
            raise SupportFloorError(
                f"{label} parent-fraction ceiling must be <= 1"
            )

    return _StratumRequirement(
        stratum_id,
        population_kind,
        parent,
        support_basis,
        contract_floor,
        power_required,
        power_binding_kind,
        power_binding_id,
        n_power,
        n_required,
        numerator,
        denominator,
    )


def _validate_policy(
    policy: object,
    expected_sha256: object,
) -> tuple[Mapping[str, Any], tuple[_StratumRequirement, ...], str]:
    expected = _require_sha256(expected_sha256, "expected_policy_sha256")
    if not isinstance(policy, Mapping):
        raise SupportFloorError("support floor policy must be an object")
    _exact_keys(policy, _POLICY_KEYS, "support floor policy")
    actual_sha256 = sha256_of_obj(policy)
    if actual_sha256 != expected:
        raise SupportFloorError("SUPPORT_POLICY_HASH_MISMATCH")
    if policy["schema"] != POLICY_SCHEMA:
        raise SupportFloorError("unexpected support floor policy schema")
    if policy["contract_revision"] != POLICY_REVISION:
        raise SupportFloorError("unexpected support floor policy revision")
    source_contract_sha256 = _require_sha256(
        policy["source_contract_sha256"], "source_contract_sha256"
    )
    if source_contract_sha256 != SOURCE_CONTRACT_SHA256:
        raise SupportFloorError("support policy source contract mismatch")
    _require_text(policy["metric_id"], "metric_id")
    split_role = _require_text(policy["split_role"], "split_role")
    if split_role not in _SPLIT_ROLES:
        raise SupportFloorError("support floor split_role is not canonical")
    if not isinstance(policy["mandatory"], bool):
        raise SupportFloorError("mandatory must be a bool")
    raw_strata = policy["strata"]
    if not isinstance(raw_strata, list) or not raw_strata:
        raise SupportFloorError("strata must be a non-empty list")
    strata = tuple(_parse_stratum(item, index) for index, item in enumerate(raw_strata))
    ids = tuple(item.stratum_id for item in strata)
    if len(ids) != len(set(ids)):
        raise SupportFloorError("support policy stratum_id values must be unique")
    if ids != tuple(sorted(ids, key=lambda item: item.encode("utf-8"))):
        raise SupportFloorError("support policy strata must be in UTF-8 order")
    by_id = {item.stratum_id: item for item in strata}
    roots = [
        item for item in strata
        if item.population_kind == "all_eligible_groups"
    ]
    if len(roots) != 1 or roots[0].parent_stratum_id is not None:
        raise SupportFloorError(
            "policy requires exactly one root all_eligible_groups stratum"
        )
    if (
        policy["mandatory"]
        and roots[0].support_basis != SUPPORT_BASIS_ELIGIBLE_AND_DEFINED
    ):
        raise SupportFloorError(
            "mandatory policy root must use eligible_and_defined"
        )
    for item in strata:
        parent = item.parent_stratum_id
        if parent is not None and (parent not in by_id or parent == item.stratum_id):
            raise SupportFloorError(
                f"invalid parent for stratum {item.stratum_id!r}"
            )
        seen = {item.stratum_id}
        while parent is not None:
            if parent in seen:
                raise SupportFloorError("support policy stratum cycle")
            seen.add(parent)
            parent = by_id[parent].parent_stratum_id

    any_power = any(item.power_required for item in strata)
    _require_sha256(policy["population_plan_sha256"], "population_plan_sha256")
    power_hash = policy["power_plan_sha256"]
    if any_power:
        _require_sha256(power_hash, "power_plan_sha256")
    elif power_hash is not None:
        raise SupportFloorError(
            "power_plan_sha256 must be null when no stratum requires power"
        )
    return policy, strata, actual_sha256


def support_floor_policy_sha256(policy: object) -> str:
    """Hash a structurally valid policy without declaring it authoritative."""
    if not isinstance(policy, Mapping):
        raise SupportFloorError("support floor policy must be an object")
    digest = sha256_of_obj(policy)
    _validate_policy(policy, digest)
    return digest


def _canonical_population_plan(
    metric_id: object,
    split_role: object,
    populations: object,
) -> dict[str, Any]:
    metric = _require_text(metric_id, "metric_id")
    role = _require_text(split_role, "split_role")
    if role not in _SPLIT_ROLES:
        raise SupportFloorError("support floor split_role is not canonical")
    if not isinstance(populations, tuple):
        raise SupportFloorError("populations must be a tuple")
    if any(not isinstance(item, StratumPopulation) for item in populations):
        raise SupportFloorError("populations must contain StratumPopulation")
    by_id: dict[str, StratumPopulation] = {}
    for population in populations:
        if population.stratum_id in by_id:
            raise SupportFloorError("duplicate stratum population")
        by_id[population.stratum_id] = population
    return {
        "schema": POPULATION_PLAN_SCHEMA,
        "metric_id": metric,
        "split_role": role,
        "strata": [
            {
                "stratum_id": stratum_id,
                "group_ids": sorted(
                    by_id[stratum_id].group_ids,
                    key=lambda item: item.encode("utf-8"),
                ),
            }
            for stratum_id in sorted(by_id, key=lambda item: item.encode("utf-8"))
        ],
    }


def stratum_population_plan_sha256(
    metric_id: object,
    split_role: object,
    populations: object,
) -> str:
    """Hash canonical pre-prediction stratum memberships.

    A policy digest binds this digest transitively.  Reordering the same
    frozen memberships is harmless; adding, removing or reassigning any
    group changes the digest and is a materialization failure.
    """
    return sha256_of_obj(
        _canonical_population_plan(metric_id, split_role, populations)
    )


def _validate_accounting(value: object) -> SupportAccounting:
    if not isinstance(value, SupportAccounting):
        raise SupportFloorError("support must be SupportAccounting")
    if value.authority_status != SUPPORT_AUTHORITY_STATUS:
        raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
    for label, population in (
        ("g_eligible", value.g_eligible),
        ("g_defined", value.g_defined),
        ("g_na", value.g_na),
    ):
        if not isinstance(population, tuple):
            raise SupportFloorError(f"{label} must be a tuple")
        for group_id in population:
            _require_text(group_id, f"{label} group_id")
        if len(population) != len(set(population)):
            raise SupportFloorError(f"{label} contains duplicate group_id")
        if population != tuple(sorted(population, key=lambda item: item.encode("utf-8"))):
            raise SupportFloorError(f"{label} is not in canonical UTF-8 order")
    eligible = set(value.g_eligible)
    defined = set(value.g_defined)
    na = set(value.g_na)
    if not defined <= eligible or na != eligible - defined or defined & na:
        raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
    if not isinstance(value.na_reasons, tuple):
        raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
    validated_na_reasons: list[str] = []
    for entry in value.na_reasons:
        if not isinstance(entry, tuple) or len(entry) != 2:
            raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
        group_id, reasons = entry
        _require_text(group_id, "na_reasons group_id")
        if not isinstance(reasons, tuple) or not reasons:
            raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
        for reason in reasons:
            _require_text(reason, "na_reasons reason")
        if len(reasons) != len(set(reasons)) or reasons != tuple(sorted(
            reasons, key=normalized_canonical_bytes
        )):
            raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
        validated_na_reasons.append(group_id)
    if tuple(validated_na_reasons) != value.g_na:
        raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
    for label, count, minimum in (
        ("defined_unit_count", value.defined_unit_count, len(value.g_defined)),
        ("na_unit_count", value.na_unit_count, len(value.g_na)),
    ):
        if isinstance(count, bool) or not isinstance(count, int) or count < minimum:
            raise SupportFloorError(
                f"SUPPORT_ACCOUNTING_INVALID: {label}"
            )
    if value.defined_unit_count + value.na_unit_count < len(value.g_eligible):
        raise SupportFloorError("SUPPORT_ACCOUNTING_INVALID")
    return value


def _fraction_ok(child: int, parent: int, numerator: int | None, denominator: int | None) -> bool:
    if numerator is None or denominator is None:
        return True
    return child * denominator <= parent * numerator


def evaluate_support_floors(
    policy: object | None,
    expected_policy_sha256: str | None,
    support: SupportAccounting,
    populations: tuple[StratumPopulation, ...],
    *,
    metric_id: str,
    split_role: str,
    power_plan_sha256: str | None = None,
) -> SupportFloorEvaluation:
    """Apply a frozen O13F policy without evaluating the metric threshold."""
    metric_id = _require_text(metric_id, "metric_id")
    split_role = _require_text(split_role, "split_role")
    if split_role not in _SPLIT_ROLES:
        raise SupportFloorError("support floor split_role is not canonical")
    _validate_accounting(support)
    if policy is None or expected_policy_sha256 is None:
        return SupportFloorEvaluation(
            metric_id,
            split_role,
            "SUPPORT_POLICY_UNAVAILABLE",
            False,
            ("SUPPORT_POLICY_UNAVAILABLE",),
            None,
            None,
            (),
            "SUPPORT_POLICY_UNAVAILABLE_NO_PASS",
        )

    policy, requirements, policy_sha256 = _validate_policy(
        policy, expected_policy_sha256
    )
    if policy["metric_id"] != metric_id or policy["split_role"] != split_role:
        raise SupportFloorError("support policy metric/split binding mismatch")
    expected_power_plan_sha256 = policy["power_plan_sha256"]
    if expected_power_plan_sha256 is not None and power_plan_sha256 is None:
        return SupportFloorEvaluation(
            metric_id,
            split_role,
            "SUPPORT_POLICY_UNAVAILABLE",
            False,
            ("SUPPORT_POLICY_UNAVAILABLE",),
            policy_sha256,
            None,
            (),
            "SUPPORT_POLICY_UNAVAILABLE_NO_PASS",
        )
    if expected_power_plan_sha256 is None and power_plan_sha256 is not None:
        raise SupportFloorError("unexpected power plan for unpowered policy")
    if power_plan_sha256 is not None:
        actual_power_plan_sha256 = _require_sha256(
            power_plan_sha256, "power_plan_sha256"
        )
        if actual_power_plan_sha256 != expected_power_plan_sha256:
            raise SupportFloorError("SUPPORT_POWER_PLAN_HASH_MISMATCH")
    population_plan_sha256 = stratum_population_plan_sha256(
        metric_id, split_role, populations
    )
    if population_plan_sha256 != policy["population_plan_sha256"]:
        raise SupportFloorError("SUPPORT_POPULATION_PLAN_HASH_MISMATCH")
    population_by_id: dict[str, StratumPopulation] = {}
    for population in populations:
        if population.stratum_id in population_by_id:
            raise SupportFloorError("duplicate stratum population")
        population_by_id[population.stratum_id] = population
    required_ids = {item.stratum_id for item in requirements}
    if set(population_by_id) != required_ids:
        raise SupportFloorError("stratum populations do not match policy exactly")

    eligible_groups = set(support.g_eligible)
    defined_groups = set(support.g_defined)
    population_sets: dict[str, set[str]] = {}
    requirement_by_id = {item.stratum_id: item for item in requirements}
    for requirement in requirements:
        group_set = set(population_by_id[requirement.stratum_id].group_ids)
        if not group_set <= eligible_groups:
            raise SupportFloorError(
                f"stratum {requirement.stratum_id!r} contains ineligible groups"
            )
        if requirement.population_kind == "all_eligible_groups" and group_set != eligible_groups:
            raise SupportFloorError(
                "all_eligible_groups population must equal G_eligible"
            )
        population_sets[requirement.stratum_id] = group_set
    for requirement in requirements:
        parent = requirement.parent_stratum_id
        if parent is not None and not population_sets[requirement.stratum_id] <= population_sets[parent]:
            raise SupportFloorError(
                f"stratum {requirement.stratum_id!r} is not a subset of its parent"
            )

    results: list[StratumFloorResult] = []
    reasons: set[str] = set()
    root_id = next(
        item.stratum_id for item in requirements
        if item.population_kind == "all_eligible_groups"
    )
    for requirement in requirements:
        groups = population_sets[requirement.stratum_id]
        defined = groups & defined_groups
        eligible_minimum_ok: bool | None = (
            len(groups) >= requirement.n_required
        )
        defined_is_active = (
            requirement.support_basis == SUPPORT_BASIS_ELIGIBLE_AND_DEFINED
        )
        defined_minimum_ok: bool | None = (
            len(defined) >= requirement.n_required
            if defined_is_active else None
        )
        eligible_ceiling_ok: bool | None = True
        defined_ceiling_ok: bool | None = True if defined_is_active else None
        if requirement.parent_stratum_id is not None:
            parent_groups = population_sets[requirement.parent_stratum_id]
            parent_defined = parent_groups & defined_groups
            eligible_ceiling_ok = _fraction_ok(
                len(groups),
                len(parent_groups),
                requirement.max_parent_fraction_numerator,
                requirement.max_parent_fraction_denominator,
            )
            if defined_is_active:
                defined_ceiling_ok = _fraction_ok(
                    len(defined),
                    len(parent_defined),
                    requirement.max_parent_fraction_numerator,
                    requirement.max_parent_fraction_denominator,
                )
        if not eligible_minimum_ok:
            reasons.add("SUPPORT_ELIGIBLE_FLOOR_NOT_MET")
        if defined_minimum_ok is False:
            reasons.add("SUPPORT_DEFINED_FLOOR_NOT_MET")
        active_checks = (
            eligible_minimum_ok,
            eligible_ceiling_ok,
            *(
                (defined_minimum_ok, defined_ceiling_ok)
                if defined_is_active else ()
            ),
        )
        if requirement.stratum_id != root_id and not all(active_checks):
            reasons.add("SUPPORT_STRATUM_FLOOR_NOT_MET")
        results.append(StratumFloorResult(
            stratum_id=requirement.stratum_id,
            population_kind=requirement.population_kind,
            parent_stratum_id=requirement.parent_stratum_id,
            support_basis=requirement.support_basis,
            contract_floor=requirement.contract_floor,
            power_required=requirement.power_required,
            power_binding_kind=requirement.power_binding_kind,
            power_binding_id=requirement.power_binding_id,
            n_power=requirement.n_power,
            eligible_count=len(groups),
            defined_count=len(defined),
            n_required=requirement.n_required,
            eligible_minimum_ok=eligible_minimum_ok,
            defined_minimum_ok=defined_minimum_ok,
            eligible_ceiling_ok=eligible_ceiling_ok,
            defined_ceiling_ok=defined_ceiling_ok,
        ))

    ordered_reasons = tuple(sorted(reasons, key=_REASON_ORDER.__getitem__))
    sufficient = not ordered_reasons
    return SupportFloorEvaluation(
        metric_id,
        split_role,
        "SUPPORT_SUFFICIENT" if sufficient else "SUPPORT_INSUFFICIENT",
        sufficient,
        ordered_reasons,
        policy_sha256,
        population_plan_sha256,
        tuple(results),
        O13F_AUTHORITY_STATUS,
    )
