"""REV8 O-13 candidate-only registry conformance validator and expander.

This module implements the candidate-only tranche authorized by O13F06.  It
does not activate a registry, discover scientific inputs, or promote the
conformance fixtures to official artifacts.  The signed package manifest is
pinned externally and every generated instance is derived deterministically
from the registry definition, claim plan, fixture-only input facts and the
calibration-readiness fixture.

The validator deliberately reports semantic reason codes before checking the
final byte oracle.  Otherwise all negative fixtures would collapse to a
generic digest mismatch and the signed 31-recipe mutation oracle would not be
executable.
"""
from __future__ import annotations

import copy
import unicodedata
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping, MutableMapping, Sequence

from .canonical import (
    canonical_bytes,
    is_sha256_hex,
    load_strict,
    parse_sha256sums,
    sha256_of_file,
    sha256_of_obj,
)

__all__ = [
    "AUTHORITY_STATUS",
    "CONFORMANCE_FILENAMES",
    "PACKAGE_MANIFEST_SHA256",
    "ConformanceMutationResult",
    "RegistryConformanceError",
    "apply_conformance_mutation",
    "compare_registry_instance_oracle",
    "expand_registry_instance",
    "load_conformance_package",
    "run_conformance_mutations",
    "validate_conformance_package",
]


AUTHORITY_STATUS = "O13F06_REGISTRY_CONFORMANCE_CANDIDATE_ONLY_NOT_ACTIVE"
PACKAGE_MANIFEST_SHA256 = (
    "4c6ff97ec04745d641f801add54407513a0dc3cf27cd5ef6225411de06a019d0"
)

CONFORMANCE_FILENAMES: dict[str, str] = {
    "readiness": "calibration_readiness_results_v1.json",
    "scope_matrix": "candidate_metric_scope_subject_matrix_v1.json",
    "input_facts": "conformance_input_facts_v1.json",
    "population_selector_schema": "population_selector_schema_v2.json",
    "claim_plan": "registry_claim_plan_conformance_v1.json",
    "mutations": "registry_conformance_mutations_v1.json",
    "definition": "registry_definition_v2.json",
    "calibration_instance": "registry_instance_calibration_v2.json",
    "development_instance": "registry_instance_development_metric_v2.json",
    "final_instance": "registry_instance_final_test_v2.json",
    "precondition_schema": "registry_precondition_schema_v2.json",
}
_MANIFEST_FILENAME = "SHA256SUMS"
_EXPECTED_FILES = frozenset((*CONFORMANCE_FILENAMES.values(), _MANIFEST_FILENAME))

_SCHEMAS = {
    "definition": "aieq-v3-rev8-o13-registry-definition-2",
    "claim_plan": "aieq-v3-rev8-o13-registry-claim-plan-1",
    "instance": "aieq-v3-rev8-o13-registry-instance-2",
    "precondition_schema": "aieq-v3-rev8-o13-registry-precondition-schema-2",
    "readiness": "aieq-v3-rev8-o13-readiness-results-conformance-1",
    "mutations": "aieq-v3-rev8-o13-registry-conformance-mutations-1",
    "input_facts": "aieq-v3-rev8-o13-conformance-input-facts-1",
}


class RegistryConformanceError(ValueError):
    """Fail-closed conformance error with a stable scientific reason code."""

    def __init__(self, reason_code: str, detail: str = "") -> None:
        self.reason_code = reason_code
        self.detail = detail
        message = reason_code if not detail else f"{reason_code}: {detail}"
        super().__init__(message)


@dataclass(frozen=True)
class ConformanceMutationResult:
    mutation_id: str
    expected_reason_code: str
    observed_reason_code: str
    passed: bool


def _fail(reason_code: str, detail: str = "") -> None:
    raise RegistryConformanceError(reason_code, detail)


def _as_mapping(value: object, label: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        _fail("REGISTRY_SHAPE_INVALID", f"{label} must be an object")
    return value


def _as_list(value: object, label: str) -> list[Any]:
    if not isinstance(value, list):
        _fail("REGISTRY_SHAPE_INVALID", f"{label} must be an array")
    return value


def _utf8(value: str) -> bytes:
    return value.encode("utf-8")


def _require_schema(document: Mapping[str, Any], expected: str) -> None:
    if document.get("schema") != expected:
        _fail(
            "REGISTRY_SCHEMA_VERSION_REJECTED",
            f"expected {expected!r}, got {document.get('schema')!r}",
        )


def _require_conformance_purpose(value: object, reason_code: str) -> None:
    if value != "conformance_fixture":
        _fail(reason_code, "fixture-only artifact promotion is forbidden")


def load_conformance_package(
    package_root: Path,
    *,
    expected_manifest_sha256: str = PACKAGE_MANIFEST_SHA256,
) -> dict[str, Any]:
    """Load the exact signed package and reject byte or membership drift."""

    root = Path(package_root)
    if not root.is_dir():
        _fail("CONFORMANCE_PACKAGE_MISSING", str(root))
    if not is_sha256_hex(expected_manifest_sha256):
        _fail("CONFORMANCE_MANIFEST_AUTHORITY_INVALID")

    actual_files = frozenset(path.name for path in root.iterdir() if path.is_file())
    if actual_files != _EXPECTED_FILES:
        _fail(
            "CONFORMANCE_PACKAGE_FILE_SET_MISMATCH",
            f"missing={sorted(_EXPECTED_FILES-actual_files)!r}, "
            f"extra={sorted(actual_files-_EXPECTED_FILES)!r}",
        )

    manifest_path = root / _MANIFEST_FILENAME
    if sha256_of_file(manifest_path) != expected_manifest_sha256:
        _fail("CONFORMANCE_MANIFEST_DIGEST_MISMATCH")
    manifest_text = manifest_path.read_text(encoding="utf-8")
    try:
        manifest = parse_sha256sums(manifest_text)
    except ValueError as exc:
        raise RegistryConformanceError(
            "CONFORMANCE_MANIFEST_INVALID", str(exc)
        ) from exc
    if frozenset(manifest) != frozenset(CONFORMANCE_FILENAMES.values()):
        _fail("CONFORMANCE_MANIFEST_FILE_SET_MISMATCH")

    package: dict[str, Any] = {}
    for key, filename in CONFORMANCE_FILENAMES.items():
        path = root / filename
        if sha256_of_file(path) != manifest[filename]:
            _fail("CONFORMANCE_ARTIFACT_DIGEST_MISMATCH", filename)
        try:
            document = load_strict(path)
        except ValueError as exc:
            raise RegistryConformanceError(
                "CONFORMANCE_ARTIFACT_JSON_INVALID", filename
            ) from exc
        if path.read_bytes() != canonical_bytes(document):
            _fail("CONFORMANCE_ARTIFACT_NONCANONICAL", filename)
        package[key] = document
    package["_manifest"] = manifest
    package["_manifest_sha256"] = expected_manifest_sha256
    package["_authority_status"] = AUTHORITY_STATUS
    return package


def _expanded_selector_rows(
    blueprint: Mapping[str, Any],
    domains: Mapping[str, Any],
) -> list[dict[str, Any]]:
    base = dict(_as_mapping(
        blueprint["population_selector_template"],
        "population_selector_template",
    ))
    mode = blueprint["expansion_mode"]
    if mode == "single":
        return [base]
    if mode == "for_each_profile":
        return [dict(base, profile=value) for value in domains["profiles"]]
    if mode == "for_each_source_family_in_parent":
        problem_type = base.get("problem_type")
        families = domains["source_families_by_problem_type"].get(problem_type)
        if not isinstance(families, list) or not families:
            _fail(
                "SOURCE_FAMILY_EXPANSION_DOMAIN_MISSING",
                str(problem_type),
            )
        return [dict(base, source_family=value) for value in families]
    if mode == "for_each_tonal_region_direction":
        return [
            dict(base, tonal_region_index=index, direction=direction)
            for index in range(7)
            for direction in ("boost", "cut")
        ]
    _fail("REGISTRY_EXPANSION_MODE_UNSUPPORTED", str(mode))
    raise AssertionError("unreachable")


def _expand_support_templates(
    definition: Mapping[str, Any],
    claim_plan: Mapping[str, Any],
    split_role: str,
    domains: Mapping[str, Any],
) -> list[dict[str, Any]]:
    references = {
        row["support_template_blueprint_id"]
        for row in claim_plan["metric_binding_blueprints"]
        if row["split_role"] == split_role
    }
    references.update(
        row["support_template_blueprint_id"]
        for row in claim_plan["calibrator_fit_binding_blueprints"]
        if row["split_role"] == split_role
    )

    result: list[dict[str, Any]] = []
    seen_templates: set[str] = set()
    for template in definition["support_template_blueprints"]:
        template_id = template["support_template_blueprint_id"]
        if template_id not in references:
            continue
        if template_id in seen_templates:
            _fail("SUPPORT_TEMPLATE_DUPLICATE", template_id)
        seen_templates.add(template_id)
        strata: list[dict[str, Any]] = []
        for blueprint in template["stratum_blueprints"]:
            mode = blueprint["expansion_mode"]
            for selector in _expanded_selector_rows(blueprint, domains):
                stratum_id = blueprint["stratum_blueprint_id"]
                if mode != "single":
                    stratum_id += ".h" + sha256_of_obj(selector)
                strata.append({
                    "contract_floor": blueprint["contract_floor"],
                    "max_parent_fraction_denominator":
                        blueprint["max_parent_fraction_denominator"],
                    "max_parent_fraction_numerator":
                        blueprint["max_parent_fraction_numerator"],
                    "parent_stratum_id": blueprint["parent_blueprint_id"],
                    "population_kind": blueprint["population_kind"],
                    "population_selector": selector,
                    "stratum_blueprint_id": blueprint["stratum_blueprint_id"],
                    "stratum_id": stratum_id,
                    "support_basis": blueprint["support_basis"],
                })
        strata.sort(key=lambda row: _utf8(row["stratum_id"]))
        result.append({"strata": strata, "support_template_id": template_id})

    if seen_templates != references:
        _fail("SUPPORT_TEMPLATE_REFERENCE_UNRESOLVED")
    result.sort(key=lambda row: _utf8(row["support_template_id"]))
    return result


def _expand_metric_bindings(
    claim_plan: Mapping[str, Any], split_role: str
) -> list[dict[str, Any]]:
    rows = [{
        "binding_id": row["binding_id"],
        "mandatory": row["mandatory"],
        "metric_id": row["metric_id"],
        "precondition_ids": list(row["precondition_blueprint_ids"]),
        "split_role": row["split_role"],
        "support_template_id": row["support_template_blueprint_id"],
    } for row in claim_plan["metric_binding_blueprints"]
        if row["split_role"] == split_role]
    rows.sort(key=lambda row: _utf8(row["binding_id"]))
    return rows


def _expand_calibrator_fit_bindings(
    claim_plan: Mapping[str, Any], split_role: str
) -> list[dict[str, Any]]:
    rows = [{
        "binding_id": row["binding_id"],
        "calibrator_fit_subject_id": row["calibrator_fit_subject_id"],
        "precondition_ids": list(row["precondition_blueprint_ids"]),
        "split_role": row["split_role"],
        "support_template_id": row["support_template_blueprint_id"],
    } for row in claim_plan["calibrator_fit_binding_blueprints"]
        if row["split_role"] == split_role]
    rows.sort(key=lambda row: _utf8(row["binding_id"]))
    return rows


def _expand_dependencies(
    claim_plan: Mapping[str, Any],
    metric_bindings: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    binding_ids = {row["binding_id"] for row in metric_bindings}
    rows = [dict(row) for row in claim_plan["calibrator_dependency_blueprints"]
            if row["metric_binding_id"] in binding_ids]
    rows.sort(key=lambda row: (
        _utf8(row["metric_binding_id"]),
        _utf8(row["calibrator_fit_binding_id"]),
    ))
    return rows


def _expand_readiness_bindings(
    claim_plan: Mapping[str, Any], split_role: str
) -> list[dict[str, Any]]:
    rows = [{
        "benchmark_readiness_subject_id": row["benchmark_readiness_subject_id"],
        "binding_id": row["binding_id"],
        "precondition_ids": list(row["precondition_blueprint_ids"]),
        "split_role": row["split_role"],
    } for row in claim_plan["benchmark_readiness_binding_blueprints"]
        if row["split_role"] == split_role]
    rows.sort(key=lambda row: _utf8(row["binding_id"]))
    return rows


def _expand_breakdowns(
    definition: Mapping[str, Any],
    claim_plan: Mapping[str, Any],
    split_role: str,
    domains: Mapping[str, Any],
) -> list[dict[str, Any]]:
    definitions = {
        row["diagnostic_breakdown_blueprint_id"]: row
        for row in definition["diagnostic_breakdown_blueprint_definitions"]
    }
    rows: list[dict[str, Any]] = []
    for blueprint in claim_plan["diagnostic_breakdown_blueprints"]:
        if blueprint["split_role"] != split_role:
            continue
        blueprint_id = blueprint["diagnostic_breakdown_blueprint_id"]
        definition_row = definitions.get(blueprint_id)
        if definition_row is None:
            _fail("BREAKDOWN_BIJECTION_MISMATCH", blueprint_id)
        if definition_row["expansion_mode"] != (
            "for_each_electronic_subgenre_in_parent"
        ):
            _fail("REGISTRY_EXPANSION_MODE_UNSUPPORTED", blueprint_id)
        for subgenre in domains["electronic_subgenres"]:
            selector = {"electronic_subgenre": subgenre}
            rows.append({
                "base_metric_id": definition_row["base_metric_id"],
                "benchmark_readiness_binding_ids":
                    list(blueprint["benchmark_readiness_binding_ids"]),
                "breakdown_evaluation_scope_id":
                    definition_row["breakdown_evaluation_scope_id"],
                "diagnostic_breakdown_blueprint_id": blueprint_id,
                "diagnostic_breakdown_id":
                    blueprint_id + ".h" + sha256_of_obj(selector),
                "parent_evaluation_scope_id":
                    definition_row["parent_evaluation_scope_id"],
                "population_kind": definition_row["population_kind"],
                "population_selector": selector,
                "precondition_ids": list(blueprint["precondition_blueprint_ids"]),
                "split_role": split_role,
            })
    rows.sort(key=lambda row: _utf8(row["diagnostic_breakdown_id"]))
    return rows


def _expand_power_bindings(
    claim_plan: Mapping[str, Any],
    split_role: str,
    support_templates: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    concrete: dict[tuple[str, str], list[str]] = {}
    for template in support_templates:
        for stratum in template["strata"]:
            concrete.setdefault((
                template["support_template_id"],
                stratum["stratum_blueprint_id"],
            ), []).append(stratum["stratum_id"])

    rows: list[dict[str, Any]] = []
    for blueprint in claim_plan["metric_stratum_power_binding_blueprints"]:
        if blueprint["split_role"] != split_role:
            continue
        key = (
            blueprint["support_template_blueprint_id"],
            blueprint["stratum_blueprint_id"],
        )
        if key not in concrete:
            _fail("POWER_BINDING_STRATUM_UNRESOLVED", repr(key))
        for stratum_id in concrete[key]:
            rows.append({
                "metric_id": blueprint["metric_id"],
                "power_binding_id": blueprint["power_binding_id"],
                "power_binding_kind": blueprint["power_binding_kind"],
                "split_role": blueprint["split_role"],
                "stratum_blueprint_id": blueprint["stratum_blueprint_id"],
                "stratum_id": stratum_id,
                "support_template_id":
                    blueprint["support_template_blueprint_id"],
            })
    rows.sort(key=lambda row: (
        _utf8(row["metric_id"]), _utf8(row["stratum_id"])
    ))
    return rows


def _expand_preconditions(
    claim_plan: Mapping[str, Any], split_role: str
) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for blueprint in claim_plan["precondition_blueprints"]:
        if blueprint["split_role"] != split_role:
            continue
        row = {
            key: copy.deepcopy(value)
            for key, value in blueprint.items()
            if key not in (
                "child_population_selector_template",
                "parent_population_selector_template",
            )
        }
        row["child_population_selector"] = copy.deepcopy(
            blueprint["child_population_selector_template"]
        )
        row["parent_population_selector"] = copy.deepcopy(
            blueprint["parent_population_selector_template"]
        )
        row["precondition_id"] = blueprint["precondition_blueprint_id"]
        rows.append(row)
    rows.sort(key=lambda row: _utf8(row["precondition_id"]))
    return rows


def expand_registry_instance(
    definition: Mapping[str, Any],
    claim_plan: Mapping[str, Any],
    split_input: Mapping[str, Any],
    *,
    readiness: Mapping[str, Any] | None,
) -> dict[str, Any]:
    """Expand one deterministic fixture-only registry instance."""

    role = split_input["split_role"]
    if role not in ("calibration", "development-metric", "final-test"):
        _fail("OUTPUT_SPLIT_ROLE_FORBIDDEN", str(role))
    if split_input["evaluation_unit_index"].get("fixture_only") is not True:
        _fail("CONFORMANCE_INPUT_PROMOTION_FORBIDDEN")
    if split_input["asset_manifest"].get("fixture_only") is not True:
        _fail("CONFORMANCE_INPUT_PROMOTION_FORBIDDEN")
    if split_input["annotation_package"].get("fixture_only") is not True:
        _fail("CONFORMANCE_INPUT_PROMOTION_FORBIDDEN")
    if split_input["frontend_contract"].get("fixture_only") is not True:
        _fail("CONFORMANCE_INPUT_PROMOTION_FORBIDDEN")

    domains = split_input["evaluation_unit_index"]["expansion_domains"]
    support_templates = _expand_support_templates(
        definition, claim_plan, role, domains
    )
    metric_bindings = _expand_metric_bindings(claim_plan, role)
    if role == "calibration":
        upstream: list[dict[str, Any]] = []
    else:
        if readiness is None:
            _fail("UPSTREAM_READINESS_DEPENDENCY_REQUIRED", role)
        upstream = [{
            "readiness_results_sha256": sha256_of_obj(readiness),
            "registry_instance_sha256": readiness["registry_instance_sha256"],
            "source_split_role": "calibration",
        }]

    return {
        "annotation_package_sha256":
            sha256_of_obj(split_input["annotation_package"]),
        "asset_manifest_sha256": sha256_of_obj(split_input["asset_manifest"]),
        "evaluation_unit_index_id":
            split_input["evaluation_unit_index"]["index_id"],
        "evaluation_unit_index_sha256":
            sha256_of_obj(split_input["evaluation_unit_index"]),
        "expanded_benchmark_readiness_bindings":
            _expand_readiness_bindings(claim_plan, role),
        "expanded_bindings": metric_bindings,
        "expanded_calibrator_dependencies":
            _expand_dependencies(claim_plan, metric_bindings),
        "expanded_calibrator_fit_bindings":
            _expand_calibrator_fit_bindings(claim_plan, role),
        "expanded_diagnostic_breakdowns":
            _expand_breakdowns(definition, claim_plan, role, domains),
        "expanded_metric_stratum_power_bindings":
            _expand_power_bindings(claim_plan, role, support_templates),
        "expanded_preconditions": _expand_preconditions(claim_plan, role),
        "expanded_support_templates": support_templates,
        "frontend_contract_sha256":
            sha256_of_obj(split_input["frontend_contract"]),
        "instance_purpose": "conformance_fixture",
        "registry_claim_plan_sha256": sha256_of_obj(claim_plan),
        "registry_definition_sha256": sha256_of_obj(definition),
        "schema": _SCHEMAS["instance"],
        "split_role": role,
        "upstream_readiness_dependencies": upstream,
    }


def compare_registry_instance_oracle(
    actual: Mapping[str, Any], expected: Mapping[str, Any]
) -> None:
    """Require exact object and canonical-byte identity for an instance."""

    if actual != expected or canonical_bytes(actual) != canonical_bytes(expected):
        _fail("REGISTRY_INSTANCE_ORACLE_MISMATCH")


def _validate_definition(
    definition: Mapping[str, Any], reference: Mapping[str, Any]
) -> None:
    _require_schema(definition, _SCHEMAS["definition"])
    metrics = _as_list(definition.get("metric_definitions"), "metric_definitions")
    expected_metrics = reference["metric_definitions"]
    metric_ids = [row.get("metric_id") for row in metrics if isinstance(row, Mapping)]
    expected_ids = [row["metric_id"] for row in expected_metrics]
    if len(metrics) != len(expected_metrics) or set(metric_ids) != set(expected_ids):
        _fail("METRIC_CATALOG_MISMATCH")
    for row in metrics:
        if not isinstance(row, Mapping):
            _fail("METRIC_CATALOG_MISMATCH")
        if "calibration" in row.get("allowed_split_roles", []):
            _fail("OUTPUT_SPLIT_ROLE_FORBIDDEN", str(row.get("metric_id")))

    breakdowns = _as_list(
        definition.get("diagnostic_breakdown_blueprint_definitions"),
        "diagnostic_breakdown_blueprint_definitions",
    )
    for row in breakdowns:
        if not isinstance(row, Mapping) or (
            row.get("publication_status") != "diagnostic_only"
            or row.get("claim_status") != "forbidden_without_signed_claim_plan"
        ):
            _fail("BREAKDOWN_PROMOTION_FORBIDDEN")

    expected_subjects = {
        row["calibrator_fit_subject_id"]
        for row in reference["calibrator_fit_subject_definitions"]
    }
    actual_subjects = {
        row.get("calibrator_fit_subject_id")
        for row in definition.get("calibrator_fit_subject_definitions", [])
        if isinstance(row, Mapping)
    }
    if actual_subjects != expected_subjects:
        _fail("CALIBRATOR_SUBJECT_SET_MISMATCH")
    if definition != reference:
        _fail("REGISTRY_DEFINITION_MISMATCH")


def _validate_claim_plan(
    claim: Mapping[str, Any],
    reference: Mapping[str, Any],
    definition: Mapping[str, Any],
) -> None:
    _require_schema(claim, _SCHEMAS["claim_plan"])
    _require_conformance_purpose(
        claim.get("claim_plan_purpose"),
        "CONFORMANCE_CLAIM_PLAN_PROMOTION_FORBIDDEN",
    )
    if claim.get("registry_definition_sha256") != sha256_of_obj(definition):
        _fail("REGISTRY_DEFINITION_DIGEST_MISMATCH")

    diagnostic_ids = {
        row["metric_id"]
        for row in definition["metric_definitions"]
        if row["publication_status"] == "diagnostic_only"
    }
    for row in claim.get("metric_binding_blueprints", []):
        if isinstance(row, Mapping) and row.get("metric_id") in diagnostic_ids:
            _fail("DIAGNOSTIC_METRIC_BINDING_FORBIDDEN")

    for row in claim.get("diagnostic_breakdown_blueprints", []):
        if not isinstance(row, Mapping) or (
            row.get("power_binding_kind") is not None
            or row.get("power_binding_id") is not None
        ):
            _fail("BREAKDOWN_POWER_FORBIDDEN")

    def ids(key: str, field: str, document: Mapping[str, Any]) -> list[Any]:
        return [row.get(field) for row in document.get(key, [])
                if isinstance(row, Mapping)]

    if set(ids("diagnostic_breakdown_blueprints",
               "diagnostic_breakdown_blueprint_id", claim)) != set(ids(
                   "diagnostic_breakdown_blueprints",
                   "diagnostic_breakdown_blueprint_id", reference)):
        _fail("BREAKDOWN_BIJECTION_MISMATCH")

    actual_dependencies = {
        (row.get("metric_binding_id"), row.get("calibrator_fit_binding_id"))
        for row in claim.get("calibrator_dependency_blueprints", [])
        if isinstance(row, Mapping)
    }
    expected_dependencies = {
        (row["metric_binding_id"], row["calibrator_fit_binding_id"])
        for row in reference["calibrator_dependency_blueprints"]
    }
    if len(actual_dependencies) < len(expected_dependencies):
        _fail("CALIBRATOR_DEPENDENCY_GRAPH_MISMATCH")
    if actual_dependencies != expected_dependencies:
        _fail("CALIBRATOR_DEPENDENCY_CLASS_MISMATCH")

    if set(claim.get("holm_primary_gate_metric_ids", [])) != set(
        reference["holm_primary_gate_metric_ids"]
    ):
        _fail("HOLM_GATE_SET_MISMATCH")

    actual_power = claim.get("power_design_blueprints", [])
    expected_power = reference["power_design_blueprints"]
    if len(actual_power) > len(expected_power):
        _fail("POWER_DESIGN_NONPRIMARY_FORBIDDEN")
    if actual_power != expected_power:
        _fail("POWER_DESIGN_SET_MISMATCH")

    subjects = set(ids("metric_binding_blueprints", "binding_id", claim))
    subjects.update(ids("calibrator_fit_binding_blueprints", "binding_id", claim))
    subjects.update(ids(
        "benchmark_readiness_binding_blueprints", "binding_id", claim
    ))
    for row in claim.get("precondition_blueprints", []):
        if not isinstance(row, Mapping) or row.get("binding_subject_id") not in subjects:
            _fail("PRECONDITION_SUBJECT_UNRESOLVED")

    expected_power_rows = {
        (row["metric_id"], row["split_role"],
         row["support_template_blueprint_id"], row["stratum_blueprint_id"]): row
        for row in reference["metric_stratum_power_binding_blueprints"]
    }
    for row in claim.get("metric_stratum_power_binding_blueprints", []):
        if not isinstance(row, Mapping):
            _fail("PRIMARY_ROOT_POWER_BINDING_REQUIRED")
        key = (
            row.get("metric_id"), row.get("split_role"),
            row.get("support_template_blueprint_id"),
            row.get("stratum_blueprint_id"),
        )
        expected = expected_power_rows.get(key)
        if expected is not None and expected["power_binding_kind"] is not None:
            if row.get("power_binding_kind") is None or row.get("power_binding_id") is None:
                _fail("PRIMARY_ROOT_POWER_BINDING_REQUIRED")

    if claim != reference:
        _fail("REGISTRY_CLAIM_PLAN_MISMATCH")


def _validate_precondition_schema(
    document: Mapping[str, Any], reference: Mapping[str, Any]
) -> None:
    _require_schema(document, _SCHEMAS["precondition_schema"])
    if document.get("relational_selector_semantics") != reference.get(
        "relational_selector_semantics"
    ):
        _fail("PRECONDITION_RELATIONAL_SELECTOR_MODE_MISMATCH")
    if document != reference:
        _fail("REGISTRY_PRECONDITION_SCHEMA_MISMATCH")


def _validate_readiness(
    readiness: Mapping[str, Any],
    reference: Mapping[str, Any],
    calibration_instance: Mapping[str, Any],
    definition: Mapping[str, Any],
    claim: Mapping[str, Any],
) -> None:
    _require_schema(readiness, _SCHEMAS["readiness"])
    _require_conformance_purpose(
        readiness.get("claim_plan_purpose"),
        "CONFORMANCE_INPUT_PROMOTION_FORBIDDEN",
    )
    for row in readiness.get("results", []):
        if not isinstance(row, Mapping) or row.get("status") != "READY":
            _fail("READINESS_STATUS_INVALID")
    if readiness.get("registry_instance_sha256") != sha256_of_obj(
        calibration_instance
    ):
        _fail("READINESS_INSTANCE_DIGEST_MISMATCH")
    if (
        readiness.get("registry_definition_sha256") != sha256_of_obj(definition)
        or readiness.get("registry_claim_plan_sha256") != sha256_of_obj(claim)
    ):
        _fail("UPSTREAM_PURPOSE_OR_DIGEST_MISMATCH")
    if readiness != reference:
        _fail("READINESS_ORACLE_MISMATCH")


def _validate_final_breakdown_identity(
    instance: Mapping[str, Any], split_input: Mapping[str, Any]
) -> None:
    observed = split_input["evaluation_unit_index"]["expansion_domains"][
        "electronic_subgenres"
    ]
    rows = instance.get("expanded_diagnostic_breakdowns", [])
    if not isinstance(rows, list):
        _fail("ELECTRONIC_SUBGENRE_EXPANSION_MISMATCH")
    per_blueprint: dict[str, list[Any]] = {}
    for row in rows:
        if not isinstance(row, Mapping):
            _fail("ELECTRONIC_SUBGENRE_EXPANSION_MISMATCH")
        selector = row.get("population_selector")
        if not isinstance(selector, Mapping):
            _fail("ELECTRONIC_SUBGENRE_EXPANSION_MISMATCH")
        if row.get("parent_defined_support_reused") is True:
            _fail("BREAKDOWN_SUPPORT_INHERITANCE_FORBIDDEN")
        per_blueprint.setdefault(
            str(row.get("diagnostic_breakdown_blueprint_id")), []
        ).append(selector.get("electronic_subgenre"))

    for values in per_blueprint.values():
        if len(values) != len(observed):
            _fail("ELECTRONIC_SUBGENRE_EXPANSION_MISMATCH")
        if values == observed:
            continue
        # Array order is by expanded ID, not observed-domain order.  Compare
        # byte-exact multiplicities while detecting a normalization collapse.
        def identity(value: Any) -> tuple[str, bytes]:
            if value is None:
                return ("null", b"")
            return ("str", str(value).encode("utf-8"))

        expected_multiset = sorted((identity(v) for v in observed))
        actual_multiset = sorted((identity(v) for v in values))
        if expected_multiset != actual_multiset:
            expected_nfc = sorted(
                (None if v is None else unicodedata.normalize("NFC", v)
                 for v in observed),
                key=lambda v: b"" if v is None else v.encode("utf-8"),
            )
            actual_nfc = sorted(
                (None if v is None else unicodedata.normalize("NFC", str(v))
                 for v in values),
                key=lambda v: b"" if v is None else v.encode("utf-8"),
            )
            if expected_nfc == actual_nfc:
                _fail("ELECTRONIC_SUBGENRE_IDENTITY_MISMATCH")
            _fail("ELECTRONIC_SUBGENRE_EXPANSION_MISMATCH")


def _validate_instance_semantics(
    instance: Mapping[str, Any],
    role: str,
    split_input: Mapping[str, Any],
    readiness: Mapping[str, Any],
    calibration_instance: Mapping[str, Any],
) -> None:
    _require_schema(instance, _SCHEMAS["instance"])
    _require_conformance_purpose(
        instance.get("instance_purpose"),
        "CONFORMANCE_INPUT_PROMOTION_FORBIDDEN",
    )
    if instance.get("split_role") != role:
        _fail("OUTPUT_SPLIT_ROLE_FORBIDDEN")
    upstream = instance.get("upstream_readiness_dependencies")
    if role == "calibration":
        if upstream != []:
            _fail("CALIBRATION_UPSTREAM_DEPENDENCY_FORBIDDEN")
    else:
        if not isinstance(upstream, list) or len(upstream) != 1:
            _fail("UPSTREAM_READINESS_DEPENDENCY_REQUIRED")
        expected = {
            "readiness_results_sha256": sha256_of_obj(readiness),
            "registry_instance_sha256": sha256_of_obj(calibration_instance),
            "source_split_role": "calibration",
        }
        if upstream[0] != expected:
            _fail("UPSTREAM_PURPOSE_OR_DIGEST_MISMATCH")

    if role == "final-test":
        if not instance.get("expanded_benchmark_readiness_bindings"):
            _fail("BENCHMARK_READINESS_BINDING_REQUIRED")
        _validate_final_breakdown_identity(instance, split_input)
        preconditions = instance.get("expanded_preconditions", [])
        if preconditions:
            parent = preconditions[0].get("parent_population_selector", {})
            if parent.get("support_partition") != "frozen_final_test":
                _fail("ELECTRONIC_PARENT_DENOMINATOR_MISMATCH")


def _split_inputs(input_facts: Mapping[str, Any]) -> dict[str, Mapping[str, Any]]:
    _require_schema(input_facts, _SCHEMAS["input_facts"])
    _require_conformance_purpose(
        input_facts.get("purpose"), "CONFORMANCE_INPUT_PROMOTION_FORBIDDEN"
    )
    result: dict[str, Mapping[str, Any]] = {}
    for row in input_facts.get("split_inputs", []):
        if not isinstance(row, Mapping):
            _fail("CONFORMANCE_INPUT_FACTS_INVALID")
        role = row.get("split_role")
        if role in result or role not in (
            "calibration", "development-metric", "final-test"
        ):
            _fail("CONFORMANCE_INPUT_FACTS_INVALID")
        result[str(role)] = row
    if set(result) != {"calibration", "development-metric", "final-test"}:
        _fail("CONFORMANCE_INPUT_FACTS_INVALID")
    return result


def validate_conformance_package(
    package: Mapping[str, Any],
    *,
    reference_package: Mapping[str, Any] | None = None,
) -> None:
    """Validate one package or mutation against the signed reference package."""

    reference = package if reference_package is None else reference_package
    definition = _as_mapping(package.get("definition"), "definition")
    claim = _as_mapping(package.get("claim_plan"), "claim_plan")
    precondition_schema = _as_mapping(
        package.get("precondition_schema"), "precondition_schema"
    )
    readiness = _as_mapping(package.get("readiness"), "readiness")
    input_facts = _as_mapping(package.get("input_facts"), "input_facts")

    reference_definition = _as_mapping(reference["definition"], "reference definition")
    reference_claim = _as_mapping(reference["claim_plan"], "reference claim")
    reference_precondition = _as_mapping(
        reference["precondition_schema"], "reference precondition schema"
    )
    reference_readiness = _as_mapping(reference["readiness"], "reference readiness")

    _validate_definition(definition, reference_definition)
    _validate_precondition_schema(precondition_schema, reference_precondition)
    _validate_claim_plan(claim, reference_claim, definition)
    split_inputs = _split_inputs(input_facts)

    calibration_instance = _as_mapping(
        package.get("calibration_instance"), "calibration instance"
    )
    _validate_instance_semantics(
        calibration_instance,
        "calibration",
        split_inputs["calibration"],
        readiness,
        calibration_instance,
    )
    expected_calibration = expand_registry_instance(
        definition, claim, split_inputs["calibration"], readiness=None
    )
    compare_registry_instance_oracle(calibration_instance, expected_calibration)

    _validate_readiness(
        readiness,
        reference_readiness,
        calibration_instance,
        definition,
        claim,
    )

    for role, key in (
        ("development-metric", "development_instance"),
        ("final-test", "final_instance"),
    ):
        instance = _as_mapping(package.get(key), key)
        _validate_instance_semantics(
            instance,
            role,
            split_inputs[role],
            readiness,
            calibration_instance,
        )
        expected = expand_registry_instance(
            definition, claim, split_inputs[role], readiness=readiness
        )
        compare_registry_instance_oracle(instance, expected)

    # Companion artifacts are part of the signed package even when the 31
    # recipes do not target them.  Do not let an in-memory caller bypass the
    # loader's manifest check by mutating one after load.
    for key, reason_code in (
        ("population_selector_schema",
         "REGISTRY_POPULATION_SELECTOR_SCHEMA_MISMATCH"),
        ("scope_matrix", "REGISTRY_SCOPE_MATRIX_MISMATCH"),
        ("input_facts", "CONFORMANCE_INPUT_FACTS_MISMATCH"),
        ("mutations", "MUTATION_ORACLE_SET_MISMATCH"),
    ):
        if package.get(key) != reference.get(key):
            _fail(reason_code)


def _pointer_parent(document: Any, pointer: str) -> tuple[Any, str]:
    if not pointer.startswith("/"):
        _fail("MUTATION_RECIPE_INVALID", pointer)
    tokens = pointer[1:].split("/")
    current = document
    for raw in tokens[:-1]:
        token = raw.replace("~1", "/").replace("~0", "~")
        if isinstance(current, list):
            current = current[int(token)]
        elif isinstance(current, MutableMapping):
            current = current[token]
        else:
            _fail("MUTATION_RECIPE_INVALID", pointer)
    return current, tokens[-1].replace("~1", "/").replace("~0", "~")


def _standard_mutation(document: Any, recipe: Mapping[str, Any]) -> None:
    parent, token = _pointer_parent(document, recipe["json_pointer"])
    operation = recipe["operation"]
    if operation == "replace":
        if isinstance(parent, list):
            parent[int(token)] = copy.deepcopy(recipe["value"])
        else:
            parent[token] = copy.deepcopy(recipe["value"])
        return
    if operation == "remove":
        if isinstance(parent, list):
            del parent[int(token)]
        else:
            del parent[token]
        return
    if operation == "append":
        if token != "-" or not isinstance(parent, list):
            _fail("MUTATION_RECIPE_INVALID", recipe["mutation_id"])
        parent.append(copy.deepcopy(recipe["value"]))
        return
    _fail("MUTATION_RECIPE_INVALID", str(operation))


def _apply_custom_mutation(
    document: MutableMapping[str, Any], recipe: Mapping[str, Any]
) -> None:
    operation = recipe["operation"]
    if operation == "append_diagnostic_binding":
        first = copy.deepcopy(document["metric_binding_blueprints"][0])
        first["binding_id"] = "binding.metric.diagnostic.forbidden.final_test"
        # Diagnostic metric IDs are absent from the claim plan by design.
        first["metric_id"] = (
            "matching.duration.dynamic_event.harshness.anomaly_natural"
        )
        first["split_role"] = "final-test"
        document["metric_binding_blueprints"].append(first)
        return
    if operation == "replace_matching_root_power_with_null":
        for row in document["metric_stratum_power_binding_blueprints"]:
            if (
                row["power_binding_kind"] is not None
                and row["stratum_blueprint_id"].endswith(".root")
            ):
                row["power_binding_kind"] = None
                row["power_binding_id"] = None
                return
        _fail("MUTATION_RECIPE_INVALID", recipe["mutation_id"])
    if operation == "append_unobserved_subgenre":
        source = copy.deepcopy(document["expanded_diagnostic_breakdowns"][0])
        selector = {"electronic_subgenre": recipe["value"]}
        source["population_selector"] = selector
        source["diagnostic_breakdown_id"] = (
            source["diagnostic_breakdown_blueprint_id"]
            + ".h" + sha256_of_obj(selector)
        )
        document["expanded_diagnostic_breakdowns"].append(source)
        document["expanded_diagnostic_breakdowns"].sort(
            key=lambda row: _utf8(row["diagnostic_breakdown_id"])
        )
        return
    if operation == "mark_parent_defined_support_reused":
        document["expanded_diagnostic_breakdowns"][0][
            "parent_defined_support_reused"
        ] = True
        return
    if operation == "merge_unicode_equivalent_selectors":
        for row in document["expanded_diagnostic_breakdowns"]:
            selector = row["population_selector"]
            if selector.get("electronic_subgenre") == "e\u0301":
                selector["electronic_subgenre"] = "é"
                row["diagnostic_breakdown_id"] = (
                    row["diagnostic_breakdown_blueprint_id"]
                    + ".h" + sha256_of_obj(selector)
                )
        document["expanded_diagnostic_breakdowns"].sort(
            key=lambda row: _utf8(row["diagnostic_breakdown_id"])
        )
        return
    if operation == "remove_one_observed_subgenre":
        rows = document["expanded_diagnostic_breakdowns"]
        target = rows[0]["population_selector"]["electronic_subgenre"]
        document["expanded_diagnostic_breakdowns"] = [
            row for row in rows
            if row["population_selector"]["electronic_subgenre"] != target
        ]
        return
    _fail("MUTATION_RECIPE_INVALID", str(operation))


def apply_conformance_mutation(
    package: Mapping[str, Any], recipe: Mapping[str, Any]
) -> dict[str, Any]:
    """Deep-copy a package and apply one signed mutation recipe."""

    mutated = copy.deepcopy(dict(package))
    target_digest = recipe.get("target_artifact_sha256")
    target_key = None
    for key in CONFORMANCE_FILENAMES:
        if key.startswith("_"):
            continue
        value = package.get(key)
        if isinstance(value, Mapping) and sha256_of_obj(value) == target_digest:
            target_key = key
            break
    if target_key is None:
        _fail("MUTATION_TARGET_UNRESOLVED", str(target_digest))
    document = mutated[target_key]
    operation = recipe.get("operation")
    if operation in ("replace", "remove", "append"):
        _standard_mutation(document, recipe)
    else:
        if not isinstance(document, MutableMapping):
            _fail("MUTATION_RECIPE_INVALID", str(recipe.get("mutation_id")))
        _apply_custom_mutation(document, recipe)
    return mutated


def run_conformance_mutations(
    package: Mapping[str, Any]
) -> tuple[ConformanceMutationResult, ...]:
    """Execute all signed negative recipes against the pristine package."""

    manifest = _as_mapping(package.get("mutations"), "mutation manifest")
    _require_schema(manifest, _SCHEMAS["mutations"])
    recipes = manifest.get("mutations")
    if not isinstance(recipes, list) or len(recipes) != 31:
        _fail("MUTATION_ORACLE_SET_MISMATCH")
    ids = [row.get("mutation_id") for row in recipes if isinstance(row, Mapping)]
    if len(ids) != 31 or len(set(ids)) != 31:
        _fail("MUTATION_ORACLE_SET_MISMATCH")

    results: list[ConformanceMutationResult] = []
    for recipe in recipes:
        if recipe.get("expected_outcome") != "FAIL":
            _fail("MUTATION_RECIPE_INVALID", str(recipe.get("mutation_id")))
        expected = recipe["expected_reason_code"]
        mutated = apply_conformance_mutation(package, recipe)
        try:
            validate_conformance_package(
                mutated, reference_package=package
            )
        except RegistryConformanceError as exc:
            observed = exc.reason_code
        else:
            observed = "PASS"
        results.append(ConformanceMutationResult(
            mutation_id=recipe["mutation_id"],
            expected_reason_code=expected,
            observed_reason_code=observed,
            passed=(observed == expected),
        ))
    return tuple(results)
