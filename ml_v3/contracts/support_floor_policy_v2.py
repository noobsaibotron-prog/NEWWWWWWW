"""REV8 O-13 candidate-only support-floor policy compiler.

The compiler turns three independently frozen inputs into one evaluable policy:

* contract-derived stratum templates;
* a pre-prediction population plan;
* an optional, validated development-pilot power plan.

It never discovers files, derives an expected digest from an untrusted input,
or activates the result.  A caller must provide the externally frozen digest
for every artifact that influences the policy.  The module remains absent from
``ml_v3.contracts.__init__`` until the atomic REV8 switch.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Mapping

from .canonical import is_sha256_hex, sha256_of_obj
from .support_floor_v2 import (
    POLICY_REVISION,
    POLICY_SCHEMA,
    SOURCE_CONTRACT_SHA256,
    SUPPORT_BASIS_ELIGIBLE_AND_DEFINED,
    SUPPORT_BASIS_ELIGIBLE_ONLY,
    StratumPopulation,
    SupportFloorError,
    stratum_population_plan_sha256,
    support_floor_policy_sha256,
)
from .validate import SchemaError, validate_benchmark_power_plan

__all__ = [
    "COMPILER_AUTHORITY_STATUS",
    "CompiledSupportFloorPolicy",
    "SupportFloorPolicyCompilationError",
    "SupportFloorPolicyTemplate",
    "compile_support_floor_policy",
]

COMPILER_AUTHORITY_STATUS = (
    "SUPPORT_BASIS_POLICY_V3_COMPILED_CANDIDATE_ONLY_NOT_ACTIVE"
)

_SUPPORT_BASES = frozenset({
    SUPPORT_BASIS_ELIGIBLE_AND_DEFINED,
    SUPPORT_BASIS_ELIGIBLE_ONLY,
})


class SupportFloorPolicyCompilationError(ValueError):
    """A frozen input is missing, ambiguous or inconsistent."""


def _require_text(value: object, label: str) -> str:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise SupportFloorPolicyCompilationError(
            f"{label} must be a non-empty NUL-free string"
        )
    return value


def _require_sha256(value: object, label: str) -> str:
    if not is_sha256_hex(value):
        raise SupportFloorPolicyCompilationError(
            f"{label} must be lowercase SHA-256 hex-64"
        )
    assert isinstance(value, str)
    return value


def _require_nonnegative_int(value: object, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise SupportFloorPolicyCompilationError(
            f"{label} must be a non-negative integer"
        )
    return value


@dataclass(frozen=True)
class SupportFloorPolicyTemplate:
    """Contract-derived requirement before power and population binding."""

    stratum_id: str
    population_kind: str
    parent_stratum_id: str | None
    contract_floor: int
    support_basis: str
    power_binding_kind: str | None = None
    power_binding_id: str | None = None
    max_parent_fraction_numerator: int | None = None
    max_parent_fraction_denominator: int | None = None

    def __post_init__(self) -> None:
        _require_text(self.stratum_id, "stratum_id")
        _require_text(self.population_kind, "population_kind")
        if self.parent_stratum_id is not None:
            _require_text(self.parent_stratum_id, "parent_stratum_id")
        _require_nonnegative_int(self.contract_floor, "contract_floor")
        _require_text(self.support_basis, "support_basis")
        if self.support_basis not in _SUPPORT_BASES:
            raise SupportFloorPolicyCompilationError(
                "support_basis is not canonical"
            )

        kind = self.power_binding_kind
        binding_id = self.power_binding_id
        if (kind is None) != (binding_id is None):
            raise SupportFloorPolicyCompilationError(
                "power binding kind and id must be both present or both absent"
            )
        if kind is not None:
            if kind not in ("gate", "family"):
                raise SupportFloorPolicyCompilationError(
                    "power_binding_kind must be gate or family"
                )
            _require_text(binding_id, "power_binding_id")

        numerator = self.max_parent_fraction_numerator
        denominator = self.max_parent_fraction_denominator
        if (numerator is None) != (denominator is None):
            raise SupportFloorPolicyCompilationError(
                "parent-fraction numerator and denominator must be paired"
            )
        if numerator is not None:
            numerator = _require_nonnegative_int(
                numerator, "max_parent_fraction_numerator"
            )
            denominator = _require_nonnegative_int(
                denominator, "max_parent_fraction_denominator"
            )
            if denominator == 0 or numerator > denominator:
                raise SupportFloorPolicyCompilationError(
                    "parent-fraction ceiling must satisfy 0 <= numerator <= "
                    "denominator and denominator > 0"
                )
            if self.parent_stratum_id is None:
                raise SupportFloorPolicyCompilationError(
                    "parent-fraction ceiling requires a parent stratum"
                )


@dataclass(frozen=True)
class CompiledSupportFloorPolicy:
    """Canonical policy materialization; never an activation decision."""

    policy: dict[str, Any]
    policy_sha256: str
    population_plan_sha256: str
    power_plan_sha256: str | None
    authority_status: str = COMPILER_AUTHORITY_STATUS


def _validated_power_rows(
    power_plan: object,
    expected_power_plan_sha256: object,
) -> tuple[Mapping[str, Any], str, dict[str, Mapping[str, Any]], dict[str, Mapping[str, Any]]]:
    if not isinstance(power_plan, Mapping):
        raise SupportFloorPolicyCompilationError(
            "required benchmark power plan is unavailable"
        )
    expected = _require_sha256(
        expected_power_plan_sha256, "expected_power_plan_sha256"
    )
    try:
        validate_benchmark_power_plan(power_plan)
    except SchemaError as exc:
        raise SupportFloorPolicyCompilationError(
            "benchmark power plan schema validation failed"
        ) from exc
    actual = sha256_of_obj(power_plan)
    if actual != expected:
        raise SupportFloorPolicyCompilationError(
            "SUPPORT_POWER_PLAN_HASH_MISMATCH"
        )
    result = power_plan["result"]
    if not isinstance(result, Mapping) or result.get("status") != "frozen":
        raise SupportFloorPolicyCompilationError(
            "benchmark power plan result.status must be frozen"
        )

    families: dict[str, Mapping[str, Any]] = {}
    for row in power_plan["families"]:
        family_id = row["family_id"]
        if family_id in families:
            raise SupportFloorPolicyCompilationError(
                f"duplicate power family binding {family_id!r}"
            )
        families[family_id] = row

    gates: dict[str, Mapping[str, Any]] = {}
    for row in power_plan["gates"]:
        metric_id = row["metric_id"]
        if metric_id in gates:
            raise SupportFloorPolicyCompilationError(
                f"duplicate power gate binding {metric_id!r}"
            )
        gates[metric_id] = row
    return power_plan, actual, families, gates


def _power_for_template(
    template: SupportFloorPolicyTemplate,
    families: Mapping[str, Mapping[str, Any]],
    gates: Mapping[str, Mapping[str, Any]],
) -> int | None:
    kind = template.power_binding_kind
    binding_id = template.power_binding_id
    if kind is None:
        return None
    assert binding_id is not None
    rows = gates if kind == "gate" else families
    row = rows.get(binding_id)
    if row is None:
        raise SupportFloorPolicyCompilationError(
            f"missing {kind} power binding {binding_id!r}"
        )
    if kind == "family" and row["floor_contractual"] != template.contract_floor:
        raise SupportFloorPolicyCompilationError(
            f"family {binding_id!r} contractual floor does not match template"
        )
    n_power = row["n_power"]
    expected_required = max(template.contract_floor, n_power)
    if row["n_required"] != expected_required:
        raise SupportFloorPolicyCompilationError(
            f"{kind} {binding_id!r} n_required does not match the bound "
            "contract floor"
        )
    return n_power


def _validate_population_shape(
    templates: tuple[SupportFloorPolicyTemplate, ...],
    populations: tuple[StratumPopulation, ...],
) -> None:
    template_by_id = {item.stratum_id: item for item in templates}
    population_by_id = {item.stratum_id: item for item in populations}
    if set(population_by_id) != set(template_by_id):
        raise SupportFloorPolicyCompilationError(
            "SUPPORT_POPULATION_PLAN_TEMPLATE_MISMATCH"
        )
    population_sets = {
        stratum_id: set(item.group_ids)
        for stratum_id, item in population_by_id.items()
    }
    for stratum_id, template in template_by_id.items():
        parent = template.parent_stratum_id
        if parent is not None:
            if parent not in population_sets:
                raise SupportFloorPolicyCompilationError(
                    f"missing parent population {parent!r}"
                )
            if not population_sets[stratum_id] <= population_sets[parent]:
                raise SupportFloorPolicyCompilationError(
                    f"stratum {stratum_id!r} is not a subset of parent "
                    f"{parent!r}"
                )


def compile_support_floor_policy(
    templates: tuple[SupportFloorPolicyTemplate, ...],
    populations: tuple[StratumPopulation, ...],
    *,
    metric_id: str,
    split_role: str,
    mandatory: bool,
    expected_population_plan_sha256: str,
    benchmark_power_plan: Mapping[str, Any] | None = None,
    expected_power_plan_sha256: str | None = None,
) -> CompiledSupportFloorPolicy:
    """Compile one hash-bound policy without activating or writing it."""
    metric = _require_text(metric_id, "metric_id")
    role = _require_text(split_role, "split_role")
    if not isinstance(mandatory, bool):
        raise SupportFloorPolicyCompilationError("mandatory must be a bool")
    if not isinstance(templates, tuple) or not templates:
        raise SupportFloorPolicyCompilationError(
            "templates must be a non-empty tuple"
        )
    if any(not isinstance(item, SupportFloorPolicyTemplate) for item in templates):
        raise SupportFloorPolicyCompilationError(
            "templates must contain SupportFloorPolicyTemplate"
        )
    ids = tuple(item.stratum_id for item in templates)
    if len(ids) != len(set(ids)):
        raise SupportFloorPolicyCompilationError(
            "support-floor template stratum_id values must be unique"
        )
    roots = tuple(
        item for item in templates
        if item.population_kind == "all_eligible_groups"
    )
    if len(roots) != 1 or roots[0].parent_stratum_id is not None:
        raise SupportFloorPolicyCompilationError(
            "templates require exactly one root all_eligible_groups stratum"
        )
    if (
        mandatory
        and roots[0].support_basis != SUPPORT_BASIS_ELIGIBLE_AND_DEFINED
    ):
        raise SupportFloorPolicyCompilationError(
            "mandatory policy root must use eligible_and_defined"
        )

    expected_population = _require_sha256(
        expected_population_plan_sha256,
        "expected_population_plan_sha256",
    )
    try:
        actual_population = stratum_population_plan_sha256(
            metric, role, populations
        )
    except SupportFloorError as exc:
        raise SupportFloorPolicyCompilationError(
            "population plan validation failed"
        ) from exc
    if actual_population != expected_population:
        raise SupportFloorPolicyCompilationError(
            "SUPPORT_POPULATION_PLAN_HASH_MISMATCH"
        )
    _validate_population_shape(templates, populations)

    power_required = any(
        item.power_binding_kind is not None for item in templates
    )
    for template in templates:
        if (
            template.power_binding_kind == "gate"
            and template.power_binding_id != metric
        ):
            raise SupportFloorPolicyCompilationError(
                "gate power binding id must equal policy metric_id"
            )
    power_sha256: str | None = None
    families: dict[str, Mapping[str, Any]] = {}
    gates: dict[str, Mapping[str, Any]] = {}
    if power_required:
        _plan, power_sha256, families, gates = _validated_power_rows(
            benchmark_power_plan,
            expected_power_plan_sha256,
        )
    elif benchmark_power_plan is not None or expected_power_plan_sha256 is not None:
        raise SupportFloorPolicyCompilationError(
            "unpowered policy must not accept an unrelated power plan"
        )

    strata: list[dict[str, Any]] = []
    for template in sorted(
        templates, key=lambda item: item.stratum_id.encode("utf-8")
    ):
        n_power = _power_for_template(template, families, gates)
        strata.append({
            "stratum_id": template.stratum_id,
            "population_kind": template.population_kind,
            "parent_stratum_id": template.parent_stratum_id,
            "support_basis": template.support_basis,
            "contract_floor": template.contract_floor,
            "power_required": n_power is not None,
            "power_binding_kind": template.power_binding_kind,
            "power_binding_id": template.power_binding_id,
            "n_power": n_power,
            "n_required": max(template.contract_floor, n_power)
            if n_power is not None else template.contract_floor,
            "max_parent_fraction_numerator": (
                template.max_parent_fraction_numerator
            ),
            "max_parent_fraction_denominator": (
                template.max_parent_fraction_denominator
            ),
        })

    policy: dict[str, Any] = {
        "schema": POLICY_SCHEMA,
        "contract_revision": POLICY_REVISION,
        "source_contract_sha256": SOURCE_CONTRACT_SHA256,
        "metric_id": metric,
        "split_role": role,
        "mandatory": mandatory,
        "strata": strata,
        "population_plan_sha256": actual_population,
        "power_plan_sha256": power_sha256,
    }
    try:
        policy_sha256 = support_floor_policy_sha256(policy)
    except SupportFloorError as exc:
        raise SupportFloorPolicyCompilationError(
            "compiled support-floor policy validation failed"
        ) from exc
    return CompiledSupportFloorPolicy(
        policy,
        policy_sha256,
        actual_population,
        power_sha256,
    )
