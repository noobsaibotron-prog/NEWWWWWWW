"""Declarative JSON Schema envelopes for Motore v3 G1a T2 (§14.1).

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a.
These dicts document exact-key envelopes (additionalProperties=false) and
canonical enums. Enforcement is fail-closed in ``validate.py`` (stdlib);
no third-party ``jsonschema`` dependency is introduced.

Packaging note (A15): unless a contract section literally tables a key list
(e.g. asset-manifest §9.1), T2 envelopes are **G1a T2 freeze-from-prose** —
implementative packaging of contract prose, not a claim that the JSON key
set is letterally tabulated in the contract.

The six §14 schemas (not identity-index / roster / feature-frame):
  asset-manifest, admission-batch, annotation, prediction,
  calibration-policy, benchmark-power-plan.
"""
from __future__ import annotations

from typing import Any, Final

from .constants import (
    ACCEPTED_SAMPLE_RATES,
    ANOMALY_CLASSES,
    BENCHMARK_FAMILIES,
    CONDITIONING_PROFILES,
    GRID_BANDS,
    LICENSE_CLASSES,
    PROBLEM_TYPES,
    SCHEMA_IDS,
    SPLIT_ROLES,
    TONAL_CURVE_MAX_DB,
    TONAL_CURVE_MIN_DB,
)

__all__ = [
    "ASSET_MANIFEST_KEYS",
    "ADMISSION_BATCH_KEYS",
    "ANNOTATION_KEYS",
    "PREDICTION_KEYS",
    "CALIBRATION_POLICY_KEYS",
    "BENCHMARK_POWER_PLAN_KEYS",
    "SEMANTIC_REGION_KEYS",
    "DYNAMIC_EVENT_KEYS",
    "SEMANTIC_BUNDLE_KEYS",
    "PREDICTION_EVENT_KEYS",
    "ANOMALY_REF_KEYS",
    "CALIBRATOR_KEYS",
    "POWER_FAMILY_KEYS",
    "POWER_GATE_KEYS",
    "SCORE_KNOT_KEYS",
    "SCHEMA_REGISTRY",
    "SCHEMA_REGISTRY_ARTIFACT_ID",
    "SCHEMA_REGISTRY_RELPATH",
    "frozen_schema_registry",
    "schema_registry_sha256",
    "schema_for",
    "schema_ids_t2",
]

# --- exact-key frozensets (additionalProperties forbidden) -----------------
# Asset-manifest keys are letterally listed in §9.1. Admission-batch and
# benchmark-power-plan key sets are G1a T2 freeze-from-prose packaging of
# §9.1 / §11.2 prose (not letterally tabled JSON schemas in the contract).

ASSET_MANIFEST_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "asset_id", "relative_path", "sha256", "group_id",
    "admission_batch_id", "split_role", "benchmark_families",
    "development_pilot", "source_profile", "primary_domain",
    "group_primary_profile", "group_primary_domain", "source_family",
    "electronic_subgenre", "sample_rate", "channels", "duration_s",
    "parent_asset_id", "derivative_kind", "license_class", "license_url",
    "attribution", "ledger_id",
})

# Freeze-from-prose of §9.1 admission-batch contents (source snapshot,
# inclusion rules, roster SHA-256, salt commitment/reveal, roster commit,
# reviewer, admitted/rejected). Does NOT include alias_mapping_version —
# that key belongs to the identity-index envelope (§8), not §9.1 batch prose.
ADMISSION_BATCH_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "admission_batch_id", "source_snapshot_id",
    "source_snapshot_sha256", "inclusion_rules_version",
    "roster_sha256", "salt_commitment", "salt_reveal", "roster_commit",
    "reviewer_id", "status",
})

ANNOTATION_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "asset_id", "annotator_id", "pass_id", "profile",
    "evaluation_unit_id", "segment_start_s", "segment_end_s",
    "tonal_correction_db", "tonal_confidence", "tonal_actionable_mask",
    "semantic_regions", "dynamic_events", "complete_types",
    "explicit_negative_types", "global_actionable", "clean_for_action",
    "notes", "tool_version",
})

SEMANTIC_REGION_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s",
    "band_lo_hz", "band_hi_hz", "direction", "severity", "confidence",
    "actionable",
})

DYNAMIC_EVENT_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s", "center_hz",
    "width_octaves", "severity", "confidence", "actionable",
})

PREDICTION_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "asset_id", "model_id", "model_sha256",
    "frontend_contract_sha256", "calibration_policy_id",
    "calibration_policy_sha256", "profile", "tonal_curve_db",
    "tonal_score", "tonal_confidence", "segment_start_s", "segment_end_s",
    "semantic_bundles", "events", "anomaly_score_ref",
    "anomaly_severity_ref",
})

SEMANTIC_BUNDLE_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s",
    "band_lo_hz", "band_hi_hz", "confidence", "actionable",
})

PREDICTION_EVENT_KEYS: Final[frozenset[str]] = frozenset({
    "problem_type", "problem_type_id", "start_s", "end_s", "center_hz",
    "width_octaves", "severity", "confidence", "actionable",
})

ANOMALY_REF_KEYS: Final[frozenset[str]] = frozenset({
    "relative_path", "sha256", "num_feature_frames", "dtype",
})

CALIBRATION_POLICY_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "policy_id", "policy_version", "model_sha256",
    "frontend_sha256", "calibration_manifest_sha256",
    "prediction_schema_id", "prediction_schema_sha256",
    "tonal_calibrator", "anomaly_calibrator", "tonal_band_thresholds",
    "semantic_type_thresholds", "anomaly_class_thresholds",
    "candidate_extractor_version", "score_to_confidence",
    "region_to_bundle", "threshold_to_actionable",
})

CALIBRATOR_KEYS: Final[frozenset[str]] = frozenset({
    "algorithm", "parameters",
})

SCORE_KNOT_KEYS: Final[frozenset[str]] = frozenset({
    "score", "confidence",
})

# Freeze-from-prose packaging of §11.2 power-plan contents (seed, pilot
# SHA-256, statistic/orientation/min effect, m, alpha_plan, support,
# result, implementation version). Nested families[]/gates[] and plan_id
# are T2 implementative structure, not a letterally tabled JSON schema.
BENCHMARK_POWER_PLAN_KEYS: Final[frozenset[str]] = frozenset({
    "schema", "plan_id", "implementation_version", "seed_base",
    "pilot_sha256", "power_source_role", "m", "alpha_plan",
    "families", "gates", "support", "result",
})

POWER_FAMILY_KEYS: Final[frozenset[str]] = frozenset({
    "family_id", "floor_contractual", "n_power", "n_required", "unit",
})

POWER_GATE_KEYS: Final[frozenset[str]] = frozenset({
    "metric_id", "statistic", "orientation", "min_effect_size",
    "n_power", "n_required", "support",
})


def _enum(values: tuple[Any, ...]) -> dict[str, Any]:
    return {"type": "string", "enum": list(values)}


def _sha256_prop() -> dict[str, Any]:
    return {
        "type": "string",
        "pattern": "^[0-9a-f]{64}$",
        "description": "lowercase SHA-256 hex digest",
    }


def _float120(description: str, minimum: float | None = None,
              maximum: float | None = None) -> dict[str, Any]:
    item: dict[str, Any] = {"type": "number"}
    if minimum is not None:
        item["minimum"] = minimum
    if maximum is not None:
        item["maximum"] = maximum
    return {
        "type": "array",
        "minItems": GRID_BANDS,
        "maxItems": GRID_BANDS,
        "items": item,
        "description": description,
    }


def _bool120(description: str) -> dict[str, Any]:
    return {
        "type": "array",
        "minItems": GRID_BANDS,
        "maxItems": GRID_BANDS,
        "items": {"type": "boolean"},
        "description": description,
    }


# --- six JSON Schema documents --------------------------------------------

ASSET_MANIFEST_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["asset_manifest"],
    "title": "aieq-v3-asset-manifest-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ASSET_MANIFEST_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["asset_manifest"]},
        "asset_id": {"type": "string", "minLength": 1},
        "relative_path": {"type": "string", "minLength": 1},
        "sha256": _sha256_prop(),
        "group_id": {"type": "string", "minLength": 1},
        "admission_batch_id": _sha256_prop(),
        "split_role": _enum(SPLIT_ROLES),
        "benchmark_families": {
            "type": "array",
            "uniqueItems": True,
            "items": _enum(BENCHMARK_FAMILIES),
        },
        "development_pilot": {"type": "boolean"},
        "source_profile": _enum(CONDITIONING_PROFILES),
        "primary_domain": {"type": "string", "minLength": 1},
        "group_primary_profile": _enum(CONDITIONING_PROFILES),
        "group_primary_domain": {"type": "string", "minLength": 1},
        "source_family": {"type": "string", "minLength": 1},
        "electronic_subgenre": {"type": ["string", "null"]},
        "sample_rate": {"type": "integer", "enum": list(ACCEPTED_SAMPLE_RATES)},
        "channels": {"type": "integer", "minimum": 1},
        "duration_s": {"type": "number", "exclusiveMinimum": 0},
        "parent_asset_id": {"type": ["string", "null"]},
        "derivative_kind": {"type": ["string", "null"]},
        "license_class": _enum(LICENSE_CLASSES),
        "license_url": {"type": "string"},
        "attribution": {"type": "string"},
        "ledger_id": {"type": "string"},
    },
    "description": (
        "§9.1 asset-manifest keys (letterally tabled). development_pilot "
        "may be true only when split_role == development-metric."
    ),
}

ADMISSION_BATCH_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["admission_batch"],
    "title": "aieq-v3-admission-batch-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ADMISSION_BATCH_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["admission_batch"]},
        "admission_batch_id": _sha256_prop(),
        "source_snapshot_id": {"type": "string", "minLength": 1},
        "source_snapshot_sha256": _sha256_prop(),
        "inclusion_rules_version": {"type": "string", "minLength": 1},
        "roster_sha256": _sha256_prop(),
        "salt_commitment": _sha256_prop(),
        "salt_reveal": {
            "type": ["string", "null"],
            "description": "null pre-reveal; lowercase hex of 32 raw salt bytes",
        },
        "roster_commit": {"type": "string", "minLength": 1},
        "reviewer_id": {"type": "string", "minLength": 1},
        "status": {"type": "string", "enum": ["admitted", "rejected"]},
    },
    "description": (
        "G1a T2 freeze-from-prose packaging of §9.1 admission-batch prose "
        "(not a letterally tabled JSON key list). roster_sha256 MUST equal "
        "admission_batch_id (SHA-256 of canonical roster bytes)."
    ),
}

_SEMANTIC_REGION_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(SEMANTIC_REGION_KEYS),
    "properties": {
        "problem_type": _enum(PROBLEM_TYPES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "band_lo_hz": {"type": ["number", "null"]},
        "band_hi_hz": {"type": ["number", "null"]},
        "direction": {"type": ["string", "null"]},
        "severity": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

_DYNAMIC_EVENT_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(DYNAMIC_EVENT_KEYS),
    "properties": {
        "problem_type": _enum(ANOMALY_CLASSES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "center_hz": {"type": "number", "minimum": 20, "maximum": 20000},
        "width_octaves": {"type": "number", "exclusiveMinimum": 0},
        "severity": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

ANNOTATION_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["annotation"],
    "title": "aieq-v3-annotation-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ANNOTATION_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["annotation"]},
        "asset_id": {"type": "string", "minLength": 1},
        "annotator_id": {"type": "string", "minLength": 1},
        "pass_id": {"type": "string", "minLength": 1},
        "profile": _enum(CONDITIONING_PROFILES),
        "evaluation_unit_id": {"type": "string", "minLength": 1},
        "segment_start_s": {"type": "number", "minimum": 0},
        "segment_end_s": {"type": "number", "exclusiveMinimum": 0},
        "tonal_correction_db": _float120(
            "EQ correttiva desiderata", TONAL_CURVE_MIN_DB, TONAL_CURVE_MAX_DB),
        "tonal_confidence": _float120("per-band confidence", 0.0, 1.0),
        "tonal_actionable_mask": _bool120("per-band actionable mask"),
        "semantic_regions": {"type": "array", "items": _SEMANTIC_REGION_SCHEMA},
        "dynamic_events": {"type": "array", "items": _DYNAMIC_EVENT_SCHEMA},
        "complete_types": {
            "type": "array",
            "uniqueItems": True,
            "items": _enum(PROBLEM_TYPES),
        },
        "explicit_negative_types": {
            "type": "array",
            "uniqueItems": True,
            "items": _enum(PROBLEM_TYPES),
        },
        "global_actionable": {"type": "boolean"},
        "clean_for_action": {"type": "boolean"},
        "notes": {"type": "string"},
        "tool_version": {"type": "string", "minLength": 1},
    },
}

_ANOMALY_REF_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(ANOMALY_REF_KEYS),
    "properties": {
        "relative_path": {"type": "string", "minLength": 1},
        "sha256": _sha256_prop(),
        "num_feature_frames": {"type": "integer", "minimum": 1},
        "dtype": {"const": "float32_le"},
    },
    "description": (
        "SHA-256 ref to little-endian float32 array shape "
        "[num_feature_frames, 3, 120], class order Resonance/Harshness/Sibilance."
    ),
}

_SEMANTIC_BUNDLE_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(SEMANTIC_BUNDLE_KEYS),
    "properties": {
        "problem_type": _enum(PROBLEM_TYPES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "band_lo_hz": {"type": ["number", "null"]},
        "band_hi_hz": {"type": ["number", "null"]},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

_PREDICTION_EVENT_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(PREDICTION_EVENT_KEYS),
    "properties": {
        "problem_type": _enum(ANOMALY_CLASSES),
        "problem_type_id": {"type": "integer", "minimum": 0, "maximum": 7},
        "start_s": {"type": "number"},
        "end_s": {"type": "number"},
        "center_hz": {"type": "number", "minimum": 20, "maximum": 20000},
        "width_octaves": {"type": "number", "exclusiveMinimum": 0},
        "severity": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "actionable": {"type": "boolean"},
    },
}

PREDICTION_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["prediction"],
    "title": "aieq-v3-prediction-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(PREDICTION_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["prediction"]},
        "asset_id": {"type": "string", "minLength": 1},
        "model_id": {"type": "string", "minLength": 1},
        "model_sha256": _sha256_prop(),
        "frontend_contract_sha256": _sha256_prop(),
        "calibration_policy_id": {"type": "string", "minLength": 1},
        "calibration_policy_sha256": _sha256_prop(),
        "profile": _enum(CONDITIONING_PROFILES),
        "tonal_curve_db": _float120(
            "public tonal curve", TONAL_CURVE_MIN_DB, TONAL_CURVE_MAX_DB),
        "tonal_score": _float120("pre-calibration tonal score", 0.0, 1.0),
        "tonal_confidence": _float120("calibrated tonal confidence", 0.0, 1.0),
        "segment_start_s": {"type": "number", "minimum": 0},
        "segment_end_s": {"type": "number", "exclusiveMinimum": 0},
        "semantic_bundles": {"type": "array", "items": _SEMANTIC_BUNDLE_SCHEMA},
        "events": {"type": "array", "items": _PREDICTION_EVENT_SCHEMA},
        "anomaly_score_ref": _ANOMALY_REF_SCHEMA,
        "anomaly_severity_ref": _ANOMALY_REF_SCHEMA,
    },
    "description": (
        "§9.3 prediction. Events without anomaly_score_ref / "
        "anomaly_severity_ref are schema-invalid."
    ),
}

_CALIBRATOR_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(CALIBRATOR_KEYS),
    "properties": {
        "algorithm": {"type": "string", "minLength": 1},
        "parameters": {"type": "object"},
    },
}

_SCORE_KNOT_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(SCORE_KNOT_KEYS),
    "properties": {
        "score": {"type": "number", "minimum": 0, "maximum": 1},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
    },
}

CALIBRATION_POLICY_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["calibration_policy"],
    "title": "aieq-v3-calibration-policy-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(CALIBRATION_POLICY_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["calibration_policy"]},
        "policy_id": {"type": "string", "minLength": 1},
        "policy_version": {"type": "string", "minLength": 1},
        "model_sha256": _sha256_prop(),
        "frontend_sha256": _sha256_prop(),
        "calibration_manifest_sha256": _sha256_prop(),
        "prediction_schema_id": {"const": SCHEMA_IDS["prediction"]},
        "prediction_schema_sha256": _sha256_prop(),
        "tonal_calibrator": _CALIBRATOR_SCHEMA,
        "anomaly_calibrator": _CALIBRATOR_SCHEMA,
        "tonal_band_thresholds": _float120("per-band thresholds", 0.0, 1.0),
        "semantic_type_thresholds": {
            "type": "array",
            "minItems": 8,
            "maxItems": 8,
            "items": {"type": "number", "minimum": 0, "maximum": 1},
        },
        "anomaly_class_thresholds": {
            "type": "array",
            "minItems": 3,
            "maxItems": 3,
            "items": {"type": "number", "minimum": 0, "maximum": 1},
        },
        "candidate_extractor_version": {"type": "string", "minLength": 1},
        "score_to_confidence": {
            "type": "array",
            "minItems": 2,
            "items": _SCORE_KNOT_SCHEMA,
            "description": "monotone non-decreasing; must include score 0 and 1",
        },
        "region_to_bundle": {
            "type": "object",
            "additionalProperties": False,
            "required": ["rule_id", "parameters"],
            "properties": {
                "rule_id": {"type": "string", "minLength": 1},
                "parameters": {"type": "object"},
            },
        },
        "threshold_to_actionable": {
            "type": "object",
            "additionalProperties": False,
            "required": ["rule_id", "parameters"],
            "properties": {
                "rule_id": {"type": "string", "minLength": 1},
                "parameters": {"type": "object"},
            },
        },
    },
}

_POWER_FAMILY_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(POWER_FAMILY_KEYS),
    "properties": {
        "family_id": _enum(BENCHMARK_FAMILIES),
        "floor_contractual": {"type": "integer", "minimum": 1},
        "n_power": {"type": "integer", "minimum": 1},
        "n_required": {"type": "integer", "minimum": 1},
        "unit": {"const": "group_id"},
    },
}

_POWER_GATE_SCHEMA: Final[dict[str, Any]] = {
    "type": "object",
    "additionalProperties": False,
    "required": sorted(POWER_GATE_KEYS),
    "properties": {
        "metric_id": {"type": "string", "minLength": 1},
        "statistic": {"type": "string", "minLength": 1},
        "orientation": {
            "type": "string",
            "enum": ["greater", "less", "two-sided"],
        },
        "min_effect_size": {"type": "number"},
        "n_power": {"type": "integer", "minimum": 1},
        "n_required": {"type": "integer", "minimum": 1},
        "support": {"type": "integer", "minimum": 0},
    },
}

BENCHMARK_POWER_PLAN_SCHEMA: Final[dict[str, Any]] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": SCHEMA_IDS["benchmark_power_plan"],
    "title": "aieq-v3-benchmark-power-plan-1",
    "type": "object",
    "additionalProperties": False,
    "required": sorted(BENCHMARK_POWER_PLAN_KEYS),
    "properties": {
        "schema": {"const": SCHEMA_IDS["benchmark_power_plan"]},
        "plan_id": {
            "type": "string",
            "minLength": 1,
            "description": (
                "G1a T2 freeze-from-prose document id (not letterally "
                "tabled in §11.2)"
            ),
        },
        "implementation_version": {"type": "string", "minLength": 1},
        "seed_base": {"type": "integer"},
        "pilot_sha256": _sha256_prop(),
        "power_source_role": {"const": "development_pilot"},
        "m": {"type": "integer", "minimum": 1},
        "alpha_plan": {"type": "number", "exclusiveMinimum": 0, "maximum": 1},
        "families": {
            "type": "array",
            "minItems": 1,
            "items": _POWER_FAMILY_SCHEMA,
            "description": (
                "Freeze-from-prose packaging of §11.2 family floors / "
                "n_required; exact nested keys are T2 structure"
            ),
        },
        "gates": {
            "type": "array",
            "minItems": 1,
            "items": _POWER_GATE_SCHEMA,
            "description": (
                "Freeze-from-prose packaging of §11.2 primary gates; "
                "exact nested keys are T2 structure"
            ),
        },
        "support": {"type": "object"},
        "result": {"type": "object"},
    },
    "description": (
        "G1a T2 freeze-from-prose packaging of §11.2 power-plan contents "
        "(not a letterally tabled JSON schema). Power is estimated only "
        "from development_pilot; unit must be group_id (never file/crop); "
        "final-test must not appear as a power source."
    ),
}

SCHEMA_REGISTRY: Final[dict[str, dict[str, Any]]] = {
    SCHEMA_IDS["asset_manifest"]: ASSET_MANIFEST_SCHEMA,
    SCHEMA_IDS["admission_batch"]: ADMISSION_BATCH_SCHEMA,
    SCHEMA_IDS["annotation"]: ANNOTATION_SCHEMA,
    SCHEMA_IDS["prediction"]: PREDICTION_SCHEMA,
    SCHEMA_IDS["calibration_policy"]: CALIBRATION_POLICY_SCHEMA,
    SCHEMA_IDS["benchmark_power_plan"]: BENCHMARK_POWER_PLAN_SCHEMA,
}

# Normative hashed schema *surface* (F3). Instance JSON under
# fixtures/g1/examples/ are example goldens, not the schema registry.
SCHEMA_REGISTRY_ARTIFACT_ID: Final[str] = "aieq-v3-schema-registry-1"
SCHEMA_REGISTRY_RELPATH: Final[str] = (
    "ml_v3/fixtures/g1/schema_registry_v1.json"
)

# Exact-key frozensets exported into the hashed surface so expanding a
# required-key set changes the tracked digest even if a nested schema
# dict were left stale (defense in depth).
_KEY_SETS: Final[dict[str, frozenset[str]]] = {
    "ASSET_MANIFEST_KEYS": ASSET_MANIFEST_KEYS,
    "ADMISSION_BATCH_KEYS": ADMISSION_BATCH_KEYS,
    "ANNOTATION_KEYS": ANNOTATION_KEYS,
    "PREDICTION_KEYS": PREDICTION_KEYS,
    "CALIBRATION_POLICY_KEYS": CALIBRATION_POLICY_KEYS,
    "BENCHMARK_POWER_PLAN_KEYS": BENCHMARK_POWER_PLAN_KEYS,
    "SEMANTIC_REGION_KEYS": SEMANTIC_REGION_KEYS,
    "DYNAMIC_EVENT_KEYS": DYNAMIC_EVENT_KEYS,
    "SEMANTIC_BUNDLE_KEYS": SEMANTIC_BUNDLE_KEYS,
    "PREDICTION_EVENT_KEYS": PREDICTION_EVENT_KEYS,
    "ANOMALY_REF_KEYS": ANOMALY_REF_KEYS,
    "CALIBRATOR_KEYS": CALIBRATOR_KEYS,
    "POWER_FAMILY_KEYS": POWER_FAMILY_KEYS,
    "POWER_GATE_KEYS": POWER_GATE_KEYS,
    "SCORE_KNOT_KEYS": SCORE_KNOT_KEYS,
}


def schema_ids_t2() -> tuple[str, ...]:
    """The six §14 G1a T2 schema identifiers, stable order."""
    return (
        SCHEMA_IDS["asset_manifest"],
        SCHEMA_IDS["admission_batch"],
        SCHEMA_IDS["annotation"],
        SCHEMA_IDS["prediction"],
        SCHEMA_IDS["calibration_policy"],
        SCHEMA_IDS["benchmark_power_plan"],
    )


def schema_for(schema_id: str) -> dict[str, Any]:
    """Return a shallow-copied schema document for ``schema_id``."""
    try:
        return dict(SCHEMA_REGISTRY[schema_id])
    except KeyError as exc:
        raise KeyError(f"unknown T2 schema id: {schema_id!r}") from exc


def frozen_schema_registry() -> dict[str, Any]:
    """Canonical schema-surface artifact for SHA256SUMS COVERED (F3).

    Serializes SCHEMA_REGISTRY (six T2 JSON Schema dicts) plus the exact-key
    frozensets as sorted lists. Expanding ASSET_MANIFEST_KEYS / a required
    field MUST change ``schema_registry_sha256()``.
    """
    from copy import deepcopy

    schemas = {
        schema_id: deepcopy(SCHEMA_REGISTRY[schema_id])
        for schema_id in schema_ids_t2()
    }
    key_sets = {
        name: sorted(keys) for name, keys in sorted(_KEY_SETS.items())
    }
    return {
        "artifact_id": SCHEMA_REGISTRY_ARTIFACT_ID,
        "key_sets": key_sets,
        "schema_ids": list(schema_ids_t2()),
        "schemas": schemas,
    }


def schema_registry_sha256() -> str:
    """SHA-256 of canonical_bytes(frozen_schema_registry())."""
    from .canonical import sha256_of_obj

    return sha256_of_obj(frozen_schema_registry())
