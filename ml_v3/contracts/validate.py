"""Fail-closed validators for Motore v3 G1a T2 JSON schemas.

Stdlib-only. Malformed documents raise SchemaError; nothing is reinterpreted
or coerced. Enforces G1a T2 freeze-from-prose envelopes derived from
docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a §9 / §11.2 (except asset-manifest
keys, which §9.1 tables letterally) and T1
``validate_manifest_split_invariants`` field expectations.
"""
from __future__ import annotations

import math
from typing import Any, Iterable, Mapping, Sequence

from .canonical import is_sha256_hex
from .constants import (
    ACCEPTED_SAMPLE_RATES,
    ANOMALY_CLASSES,
    BENCHMARK_FAMILIES,
    GRID_BANDS,
    GRID_MAX_HZ,
    GRID_MIN_HZ,
    LICENSE_CLASSES,
    PROBLEM_TYPE_TO_ID,
    PROBLEM_TYPES,
    SCHEMA_IDS,
    SPLIT_ROLES,
    TONAL_CURVE_MAX_DB,
    TONAL_CURVE_MIN_DB,
)
from .profiles import ProfileError, is_canonical_profile, require_problem_type_consistency
from .schemas import (
    ADMISSION_BATCH_KEYS,
    ANOMALY_REF_KEYS,
    ANNOTATION_KEYS,
    ASSET_MANIFEST_KEYS,
    BENCHMARK_POWER_PLAN_KEYS,
    CALIBRATION_POLICY_KEYS,
    CALIBRATOR_KEYS,
    DYNAMIC_EVENT_KEYS,
    POWER_FAMILY_KEYS,
    POWER_GATE_KEYS,
    PREDICTION_EVENT_KEYS,
    PREDICTION_KEYS,
    SCHEMA_REGISTRY,
    SCORE_KNOT_KEYS,
    SEMANTIC_BUNDLE_KEYS,
    SEMANTIC_REGION_KEYS,
)

__all__ = [
    "SchemaError",
    "validate",
    "validate_schema_id",
    "validate_asset_manifest",
    "validate_admission_batch",
    "validate_annotation",
    "validate_prediction",
    "validate_calibration_policy",
    "validate_benchmark_power_plan",
]


class SchemaError(ValueError):
    """Raised when a document violates a G1a T2 schema. Never self-corrects."""


# ------------------------------------------------------------------ helpers
def _require_mapping(doc: object, label: str) -> Mapping[str, Any]:
    if not isinstance(doc, Mapping):
        raise SchemaError(f"{label} must be a JSON object, got {type(doc).__name__}")
    return doc


def _exact_keys(doc: Mapping[str, Any], allowed: frozenset[str], label: str) -> None:
    keys = set(doc.keys())
    missing = allowed - keys
    extra = keys - allowed
    if missing:
        raise SchemaError(f"{label}: missing required keys: {sorted(missing)}")
    if extra:
        raise SchemaError(
            f"{label}: additionalProperties forbidden; unexpected keys: "
            f"{sorted(extra)}")


def _require_str(doc: Mapping[str, Any], key: str, *, nonempty: bool = True) -> str:
    value = doc[key]
    if not isinstance(value, str):
        raise SchemaError(f"{key} must be a string, got {type(value).__name__}")
    if nonempty and not value:
        raise SchemaError(f"{key} must be a non-empty string")
    if "\x00" in value:
        raise SchemaError(f"{key} contains U+0000")
    return value


def _require_bool(doc: Mapping[str, Any], key: str) -> bool:
    value = doc[key]
    if not isinstance(value, bool):
        raise SchemaError(f"{key} must be a boolean, got {type(value).__name__}")
    return value


def _require_int(doc: Mapping[str, Any], key: str, *, minimum: int | None = None
                 ) -> int:
    value = doc[key]
    if isinstance(value, bool) or not isinstance(value, int):
        raise SchemaError(f"{key} must be an integer, got {type(value).__name__}")
    if minimum is not None and value < minimum:
        raise SchemaError(f"{key} must be >= {minimum}, got {value}")
    return value


def _require_finite_number(value: object, path: str, *,
                           minimum: float | None = None,
                           maximum: float | None = None,
                           exclusive_minimum: float | None = None) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise SchemaError(f"{path} must be a finite number, got {type(value).__name__}")
    number = float(value)
    if not math.isfinite(number):
        raise SchemaError(f"{path} must be finite, got {number!r}")
    if minimum is not None and number < minimum:
        raise SchemaError(f"{path} must be >= {minimum}, got {number}")
    if maximum is not None and number > maximum:
        raise SchemaError(f"{path} must be <= {maximum}, got {number}")
    if exclusive_minimum is not None and number <= exclusive_minimum:
        raise SchemaError(f"{path} must be > {exclusive_minimum}, got {number}")
    return number


def _require_sha256(doc: Mapping[str, Any], key: str) -> str:
    value = _require_str(doc, key)
    if not is_sha256_hex(value):
        raise SchemaError(f"{key} must be lowercase hex-64 SHA-256, got {value!r}")
    return value


def _require_list(doc: Mapping[str, Any], key: str) -> list[Any]:
    value = doc[key]
    if not isinstance(value, list):
        raise SchemaError(f"{key} must be a list, got {type(value).__name__}")
    return value


def _require_float_vector(doc: Mapping[str, Any], key: str, length: int, *,
                          minimum: float | None = None,
                          maximum: float | None = None) -> list[float]:
    values = _require_list(doc, key)
    if len(values) != length:
        raise SchemaError(f"{key} must have length {length}, got {len(values)}")
    out: list[float] = []
    for index, item in enumerate(values):
        out.append(_require_finite_number(
            item, f"{key}[{index}]", minimum=minimum, maximum=maximum))
    return out


def _require_bool_vector(doc: Mapping[str, Any], key: str, length: int) -> list[bool]:
    values = _require_list(doc, key)
    if len(values) != length:
        raise SchemaError(f"{key} must have length {length}, got {len(values)}")
    out: list[bool] = []
    for index, item in enumerate(values):
        if not isinstance(item, bool):
            raise SchemaError(
                f"{key}[{index}] must be boolean, got {type(item).__name__}")
        out.append(item)
    return out


def _require_unique_enum_list(values: Sequence[Any], allowed: Iterable[str],
                              path: str) -> list[str]:
    allowed_set = set(allowed)
    if not isinstance(values, list):
        raise SchemaError(f"{path} must be a list")
    seen: set[str] = set()
    out: list[str] = []
    for index, item in enumerate(values):
        if not isinstance(item, str):
            raise SchemaError(f"{path}[{index}] must be a string")
        if item not in allowed_set:
            raise SchemaError(f"{path}[{index}] unknown value {item!r}")
        if item in seen:
            raise SchemaError(f"{path} contains duplicate {item!r}")
        seen.add(item)
        out.append(item)
    return out


def _require_profile(name: object, path: str) -> str:
    if not is_canonical_profile(name):
        raise SchemaError(f"{path} non-canonical profile {name!r}")
    return str(name)


def _hz_in_grid(value: float, path: str) -> float:
    return _require_finite_number(
        value, path, minimum=GRID_MIN_HZ, maximum=GRID_MAX_HZ)


def _check_segment(start: float, end: float, path: str) -> None:
    if end <= start:
        raise SchemaError(f"{path}: segment_end_s must be > segment_start_s")


def _check_time_in_segment(t0: float, t1: float, seg0: float, seg1: float,
                           path: str) -> None:
    if t0 < seg0 or t1 > seg1 or t1 <= t0:
        raise SchemaError(
            f"{path}: times [{t0}, {t1}] must fall inside segment "
            f"[{seg0}, {seg1}] with end > start")


# ----------------------------------------------------------- asset manifest
def validate_asset_manifest(doc: object) -> None:
    data = _require_mapping(doc, "asset_manifest")
    _exact_keys(data, ASSET_MANIFEST_KEYS, "asset_manifest")
    if data["schema"] != SCHEMA_IDS["asset_manifest"]:
        raise SchemaError(
            f"unexpected asset_manifest schema: {data['schema']!r}")
    _require_str(data, "asset_id")
    _require_str(data, "relative_path")
    _require_sha256(data, "sha256")
    _require_str(data, "group_id")
    _require_sha256(data, "admission_batch_id")
    role = _require_str(data, "split_role")
    if role not in SPLIT_ROLES:
        raise SchemaError(f"unknown split_role {role!r}")
    families = _require_unique_enum_list(
        _require_list(data, "benchmark_families"), BENCHMARK_FAMILIES,
        "benchmark_families")
    del families  # uniqueness/membership already checked
    pilot = _require_bool(data, "development_pilot")
    if pilot and role != "development-metric":
        raise SchemaError(
            "development_pilot may be true only when split_role is "
            "'development-metric'")
    _require_profile(data["source_profile"], "source_profile")
    _require_str(data, "primary_domain")
    _require_profile(data["group_primary_profile"], "group_primary_profile")
    _require_str(data, "group_primary_domain")
    _require_str(data, "source_family")
    sub = data["electronic_subgenre"]
    if sub is not None and (not isinstance(sub, str) or "\x00" in sub):
        raise SchemaError("electronic_subgenre must be string or null")
    rate = _require_int(data, "sample_rate")
    if rate not in ACCEPTED_SAMPLE_RATES:
        raise SchemaError(f"sample_rate {rate} not in ACCEPTED_SAMPLE_RATES")
    _require_int(data, "channels", minimum=1)
    _require_finite_number(data["duration_s"], "duration_s", exclusive_minimum=0.0)
    parent = data["parent_asset_id"]
    if parent is not None and (not isinstance(parent, str) or not parent):
        raise SchemaError("parent_asset_id must be non-empty string or null")
    kind = data["derivative_kind"]
    if kind is not None and (not isinstance(kind, str) or "\x00" in kind):
        raise SchemaError("derivative_kind must be string or null")
    if parent is None and kind is not None:
        raise SchemaError("root asset must have derivative_kind null")
    if parent is not None and kind is None:
        raise SchemaError("derivative asset requires non-null derivative_kind")
    license_class = _require_str(data, "license_class")
    if license_class not in LICENSE_CLASSES:
        raise SchemaError(f"unknown license_class {license_class!r}")
    _require_str(data, "license_url", nonempty=False)
    _require_str(data, "attribution", nonempty=False)
    ledger = _require_str(data, "ledger_id", nonempty=False)
    if license_class == "OWNED" and not ledger:
        raise SchemaError("OWNED license_class requires non-empty ledger_id")


# ---------------------------------------------------------- admission batch
def validate_admission_batch(doc: object) -> None:
    data = _require_mapping(doc, "admission_batch")
    _exact_keys(data, ADMISSION_BATCH_KEYS, "admission_batch")
    if data["schema"] != SCHEMA_IDS["admission_batch"]:
        raise SchemaError(
            f"unexpected admission_batch schema: {data['schema']!r}")
    batch_id = _require_sha256(data, "admission_batch_id")
    _require_str(data, "source_snapshot_id")
    _require_sha256(data, "source_snapshot_sha256")
    _require_str(data, "inclusion_rules_version")
    roster = _require_sha256(data, "roster_sha256")
    if roster != batch_id:
        raise SchemaError(
            "roster_sha256 must equal admission_batch_id "
            "(SHA-256 of canonical roster bytes)")
    _require_sha256(data, "salt_commitment")
    reveal = data["salt_reveal"]
    if reveal is not None:
        if not isinstance(reveal, str) or not is_sha256_hex(reveal):
            raise SchemaError(
                "salt_reveal must be null or lowercase hex of 32 raw salt bytes")
    _require_str(data, "roster_commit")
    _require_str(data, "reviewer_id")
    status = _require_str(data, "status")
    if status not in ("admitted", "rejected"):
        raise SchemaError(f"status must be admitted|rejected, got {status!r}")


# --------------------------------------------------------------- annotation
def _validate_semantic_region(region: object, index: int, seg0: float,
                              seg1: float) -> None:
    path = f"semantic_regions[{index}]"
    data = _require_mapping(region, path)
    _exact_keys(data, SEMANTIC_REGION_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in PROBLEM_TYPE_TO_ID:
        raise SchemaError(f"{path}.problem_type unknown {ptype!r}")
    pid = _require_int(data, "problem_type_id")
    try:
        require_problem_type_consistency(ptype, pid)
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    band_lo = data["band_lo_hz"]
    band_hi = data["band_hi_hz"]
    if band_lo is not None:
        band_lo = _hz_in_grid(
            _require_finite_number(band_lo, f"{path}.band_lo_hz"),
            f"{path}.band_lo_hz")
    if band_hi is not None:
        band_hi = _hz_in_grid(
            _require_finite_number(band_hi, f"{path}.band_hi_hz"),
            f"{path}.band_hi_hz")
    if (band_lo is None) != (band_hi is None):
        raise SchemaError(f"{path}: band_lo_hz/band_hi_hz must both be set or null")
    if band_lo is not None and band_hi is not None and band_hi <= band_lo:
        raise SchemaError(f"{path}: band_hi_hz must be > band_lo_hz")
    direction = data["direction"]
    if direction is not None and not isinstance(direction, str):
        raise SchemaError(f"{path}.direction must be string or null")
    if ptype in ("Thinness", "DullSound") and not direction:
        raise SchemaError(f"{path}: direction required for {ptype}")
    if ptype in ("Muddiness", "Boominess", "BoxyMidrange") and band_lo is None:
        raise SchemaError(f"{path}: band required for {ptype}")
    _require_finite_number(data["severity"], f"{path}.severity",
                           minimum=0.0, maximum=1.0)
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def _validate_dynamic_event(event: object, index: int, seg0: float,
                            seg1: float) -> None:
    path = f"dynamic_events[{index}]"
    data = _require_mapping(event, path)
    _exact_keys(data, DYNAMIC_EVENT_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in ANOMALY_CLASSES:
        raise SchemaError(
            f"{path}.problem_type must be one of {list(ANOMALY_CLASSES)}")
    pid = _require_int(data, "problem_type_id")
    try:
        require_problem_type_consistency(ptype, pid)
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    _hz_in_grid(
        _require_finite_number(data["center_hz"], f"{path}.center_hz"),
        f"{path}.center_hz")
    _require_finite_number(data["width_octaves"], f"{path}.width_octaves",
                           exclusive_minimum=0.0)
    _require_finite_number(data["severity"], f"{path}.severity",
                           minimum=0.0, maximum=1.0)
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def validate_annotation(doc: object) -> None:
    data = _require_mapping(doc, "annotation")
    _exact_keys(data, ANNOTATION_KEYS, "annotation")
    if data["schema"] != SCHEMA_IDS["annotation"]:
        raise SchemaError(f"unexpected annotation schema: {data['schema']!r}")
    _require_str(data, "asset_id")
    _require_str(data, "annotator_id")
    _require_str(data, "pass_id")
    _require_profile(data["profile"], "profile")
    _require_str(data, "evaluation_unit_id")
    seg0 = _require_finite_number(data["segment_start_s"], "segment_start_s",
                                  minimum=0.0)
    seg1 = _require_finite_number(data["segment_end_s"], "segment_end_s",
                                  exclusive_minimum=0.0)
    _check_segment(seg0, seg1, "annotation")
    curve = _require_float_vector(
        data, "tonal_correction_db", GRID_BANDS,
        minimum=TONAL_CURVE_MIN_DB, maximum=TONAL_CURVE_MAX_DB)
    _require_float_vector(data, "tonal_confidence", GRID_BANDS,
                          minimum=0.0, maximum=1.0)
    mask = _require_bool_vector(data, "tonal_actionable_mask", GRID_BANDS)
    regions = _require_list(data, "semantic_regions")
    events = _require_list(data, "dynamic_events")
    for index, region in enumerate(regions):
        _validate_semantic_region(region, index, seg0, seg1)
    for index, event in enumerate(events):
        _validate_dynamic_event(event, index, seg0, seg1)
    complete = _require_unique_enum_list(
        _require_list(data, "complete_types"), PROBLEM_TYPES, "complete_types")
    explicit_neg = _require_unique_enum_list(
        _require_list(data, "explicit_negative_types"), PROBLEM_TYPES,
        "explicit_negative_types")
    for name in explicit_neg:
        if name not in complete:
            raise SchemaError(
                f"explicit_negative_types entry {name!r} must also be in "
                "complete_types")
    global_actionable = _require_bool(data, "global_actionable")
    clean = _require_bool(data, "clean_for_action")
    _require_str(data, "notes", nonempty=False)
    _require_str(data, "tool_version")
    if clean:
        if any(abs(v) > 0.0 for v in curve):
            raise SchemaError("clean_for_action requires zero tonal_correction_db")
        if any(mask):
            raise SchemaError("clean_for_action requires tonal_actionable_mask all false")
        if global_actionable:
            raise SchemaError("clean_for_action requires global_actionable false")
        if any(bool(ev.get("actionable")) for ev in events):
            raise SchemaError("clean_for_action forbids actionable dynamic_events")
        if set(complete) != set(PROBLEM_TYPES) or set(explicit_neg) != set(PROBLEM_TYPES):
            raise SchemaError(
                "clean_for_action requires all eight types in complete_types "
                "and explicit_negative_types")


# --------------------------------------------------------------- prediction
def _validate_anomaly_ref(ref: object, path: str) -> None:
    data = _require_mapping(ref, path)
    _exact_keys(data, ANOMALY_REF_KEYS, path)
    _require_str(data, "relative_path")
    _require_sha256(data, "sha256")
    _require_int(data, "num_feature_frames", minimum=1)
    if data["dtype"] != "float32_le":
        raise SchemaError(f"{path}.dtype must be 'float32_le'")


def _validate_prediction_bundle(bundle: object, index: int, seg0: float,
                                seg1: float) -> None:
    path = f"semantic_bundles[{index}]"
    data = _require_mapping(bundle, path)
    _exact_keys(data, SEMANTIC_BUNDLE_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in PROBLEM_TYPE_TO_ID:
        raise SchemaError(f"{path}.problem_type unknown {ptype!r}")
    try:
        require_problem_type_consistency(ptype, _require_int(data, "problem_type_id"))
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    for key in ("band_lo_hz", "band_hi_hz"):
        if data[key] is not None:
            _hz_in_grid(_require_finite_number(data[key], f"{path}.{key}"),
                        f"{path}.{key}")
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def _validate_prediction_event(event: object, index: int, seg0: float,
                               seg1: float) -> None:
    path = f"events[{index}]"
    data = _require_mapping(event, path)
    _exact_keys(data, PREDICTION_EVENT_KEYS, path)
    ptype = _require_str(data, "problem_type")
    if ptype not in ANOMALY_CLASSES:
        raise SchemaError(
            f"{path}.problem_type must be one of {list(ANOMALY_CLASSES)}")
    try:
        require_problem_type_consistency(ptype, _require_int(data, "problem_type_id"))
    except ProfileError as exc:
        raise SchemaError(str(exc)) from exc
    start = _require_finite_number(data["start_s"], f"{path}.start_s")
    end = _require_finite_number(data["end_s"], f"{path}.end_s")
    _check_time_in_segment(start, end, seg0, seg1, path)
    _hz_in_grid(_require_finite_number(data["center_hz"], f"{path}.center_hz"),
                f"{path}.center_hz")
    _require_finite_number(data["width_octaves"], f"{path}.width_octaves",
                           exclusive_minimum=0.0)
    _require_finite_number(data["severity"], f"{path}.severity",
                           minimum=0.0, maximum=1.0)
    _require_finite_number(data["confidence"], f"{path}.confidence",
                           minimum=0.0, maximum=1.0)
    _require_bool(data, "actionable")


def validate_prediction(doc: object) -> None:
    data = _require_mapping(doc, "prediction")
    _exact_keys(data, PREDICTION_KEYS, "prediction")
    if data["schema"] != SCHEMA_IDS["prediction"]:
        raise SchemaError(f"unexpected prediction schema: {data['schema']!r}")
    _require_str(data, "asset_id")
    _require_str(data, "model_id")
    _require_sha256(data, "model_sha256")
    _require_sha256(data, "frontend_contract_sha256")
    _require_str(data, "calibration_policy_id")
    _require_sha256(data, "calibration_policy_sha256")
    _require_profile(data["profile"], "profile")
    _require_float_vector(
        data, "tonal_curve_db", GRID_BANDS,
        minimum=TONAL_CURVE_MIN_DB, maximum=TONAL_CURVE_MAX_DB)
    _require_float_vector(data, "tonal_score", GRID_BANDS, minimum=0.0, maximum=1.0)
    _require_float_vector(data, "tonal_confidence", GRID_BANDS,
                          minimum=0.0, maximum=1.0)
    seg0 = _require_finite_number(data["segment_start_s"], "segment_start_s",
                                  minimum=0.0)
    seg1 = _require_finite_number(data["segment_end_s"], "segment_end_s",
                                  exclusive_minimum=0.0)
    _check_segment(seg0, seg1, "prediction")
    bundles = _require_list(data, "semantic_bundles")
    events = _require_list(data, "events")
    for index, bundle in enumerate(bundles):
        _validate_prediction_bundle(bundle, index, seg0, seg1)
    for index, event in enumerate(events):
        _validate_prediction_event(event, index, seg0, seg1)
    # Contract: event list without dense anomaly surfaces is schema-invalid.
    # Refs are required keys; validate shape metadata.
    _validate_anomaly_ref(data["anomaly_score_ref"], "anomaly_score_ref")
    _validate_anomaly_ref(data["anomaly_severity_ref"], "anomaly_severity_ref")
    score_frames = data["anomaly_score_ref"]["num_feature_frames"]
    sev_frames = data["anomaly_severity_ref"]["num_feature_frames"]
    if score_frames != sev_frames:
        raise SchemaError(
            "anomaly_score_ref and anomaly_severity_ref num_feature_frames "
            "must match")


# ------------------------------------------------------ calibration policy
def _validate_calibrator(cal: object, path: str) -> None:
    data = _require_mapping(cal, path)
    _exact_keys(data, CALIBRATOR_KEYS, path)
    _require_str(data, "algorithm")
    params = data["parameters"]
    if not isinstance(params, Mapping):
        raise SchemaError(f"{path}.parameters must be an object")


def _validate_rule_object(rule: object, path: str) -> None:
    data = _require_mapping(rule, path)
    allowed = frozenset({"rule_id", "parameters"})
    _exact_keys(data, allowed, path)
    _require_str(data, "rule_id")
    if not isinstance(data["parameters"], Mapping):
        raise SchemaError(f"{path}.parameters must be an object")


def _validate_score_to_confidence(knots: object) -> None:
    if not isinstance(knots, list) or len(knots) < 2:
        raise SchemaError("score_to_confidence must be a list with >= 2 knots")
    scores: list[float] = []
    confidences: list[float] = []
    for index, knot in enumerate(knots):
        path = f"score_to_confidence[{index}]"
        data = _require_mapping(knot, path)
        _exact_keys(data, SCORE_KNOT_KEYS, path)
        score = _require_finite_number(data["score"], f"{path}.score",
                                       minimum=0.0, maximum=1.0)
        conf = _require_finite_number(data["confidence"], f"{path}.confidence",
                                      minimum=0.0, maximum=1.0)
        scores.append(score)
        confidences.append(conf)
    if 0.0 not in scores or 1.0 not in scores:
        raise SchemaError("score_to_confidence must define knots at score 0 and 1")
    for index in range(1, len(scores)):
        if scores[index] <= scores[index - 1]:
            raise SchemaError(
                "score_to_confidence scores must be strictly increasing")
        if confidences[index] + 1e-15 < confidences[index - 1]:
            raise SchemaError(
                "score_to_confidence must be monotone non-decreasing")


def validate_calibration_policy(doc: object) -> None:
    data = _require_mapping(doc, "calibration_policy")
    _exact_keys(data, CALIBRATION_POLICY_KEYS, "calibration_policy")
    if data["schema"] != SCHEMA_IDS["calibration_policy"]:
        raise SchemaError(
            f"unexpected calibration_policy schema: {data['schema']!r}")
    _require_str(data, "policy_id")
    _require_str(data, "policy_version")
    _require_sha256(data, "model_sha256")
    _require_sha256(data, "frontend_sha256")
    _require_sha256(data, "calibration_manifest_sha256")
    if data["prediction_schema_id"] != SCHEMA_IDS["prediction"]:
        raise SchemaError(
            f"prediction_schema_id must be {SCHEMA_IDS['prediction']!r}")
    _require_sha256(data, "prediction_schema_sha256")
    _validate_calibrator(data["tonal_calibrator"], "tonal_calibrator")
    _validate_calibrator(data["anomaly_calibrator"], "anomaly_calibrator")
    _require_float_vector(data, "tonal_band_thresholds", GRID_BANDS,
                          minimum=0.0, maximum=1.0)
    sem = _require_float_vector(data, "semantic_type_thresholds", 8,
                                minimum=0.0, maximum=1.0)
    anom = _require_float_vector(data, "anomaly_class_thresholds", 3,
                                 minimum=0.0, maximum=1.0)
    del sem, anom
    _require_str(data, "candidate_extractor_version")
    _validate_score_to_confidence(data["score_to_confidence"])
    _validate_rule_object(data["region_to_bundle"], "region_to_bundle")
    _validate_rule_object(data["threshold_to_actionable"], "threshold_to_actionable")


# ---------------------------------------------------- benchmark power plan
def _forbid_final_test_leak(obj: object, path: str = "$") -> None:
    """Reject any string leaf equal to final-test / final_test as power source."""
    if isinstance(obj, str):
        lowered = obj.lower().replace("_", "-")
        if lowered == "final-test":
            raise SchemaError(
                f"{path}: power plan must not reference final-test as input")
        return
    if isinstance(obj, Mapping):
        for key, value in obj.items():
            key_path = f"{path}.{key}" if isinstance(key, str) else path
            if isinstance(key, str):
                key_norm = key.lower().replace("_", "-")
                if key_norm in ("final-test", "finaltest"):
                    raise SchemaError(
                        f"{path}: power plan must not contain final-test key")
            _forbid_final_test_leak(value, key_path)
        return
    if isinstance(obj, list):
        for index, value in enumerate(obj):
            _forbid_final_test_leak(value, f"{path}[{index}]")


def validate_benchmark_power_plan(doc: object) -> None:
    data = _require_mapping(doc, "benchmark_power_plan")
    _exact_keys(data, BENCHMARK_POWER_PLAN_KEYS, "benchmark_power_plan")
    if data["schema"] != SCHEMA_IDS["benchmark_power_plan"]:
        raise SchemaError(
            f"unexpected benchmark_power_plan schema: {data['schema']!r}")
    _require_str(data, "plan_id")
    _require_str(data, "implementation_version")
    _require_int(data, "seed_base")
    _require_sha256(data, "pilot_sha256")
    source = _require_str(data, "power_source_role")
    if source != "development_pilot":
        raise SchemaError(
            "power_source_role must be 'development_pilot' "
            "(never final-test)")
    m_value = _require_int(data, "m", minimum=1)
    alpha = _require_finite_number(data["alpha_plan"], "alpha_plan",
                                   exclusive_minimum=0.0, maximum=1.0)
    expected = 0.05 / m_value
    if abs(alpha - expected) > 1e-12:
        raise SchemaError(
            f"alpha_plan must equal 0.05/m ({expected}), got {alpha}")
    families = _require_list(data, "families")
    if not families:
        raise SchemaError("families must be non-empty")
    seen_families: set[str] = set()
    for index, family in enumerate(families):
        path = f"families[{index}]"
        row = _require_mapping(family, path)
        _exact_keys(row, POWER_FAMILY_KEYS, path)
        fam = _require_str(row, "family_id")
        if fam not in BENCHMARK_FAMILIES:
            raise SchemaError(f"{path}.family_id unknown {fam!r}")
        if fam in seen_families:
            raise SchemaError(f"duplicate family_id {fam!r}")
        seen_families.add(fam)
        floor = _require_int(row, "floor_contractual", minimum=1)
        n_power = _require_int(row, "n_power", minimum=1)
        n_required = _require_int(row, "n_required", minimum=1)
        if n_required != max(floor, n_power):
            raise SchemaError(
                f"{path}.n_required must equal max(floor_contractual, n_power)")
        if row["unit"] != "group_id":
            raise SchemaError(
                f"{path}.unit must be 'group_id' (file/crop forbidden)")
    gates = _require_list(data, "gates")
    if not gates:
        raise SchemaError("gates must be non-empty")
    if len(gates) != m_value:
        raise SchemaError(f"gates length must equal m ({m_value})")
    for index, gate in enumerate(gates):
        path = f"gates[{index}]"
        row = _require_mapping(gate, path)
        _exact_keys(row, POWER_GATE_KEYS, path)
        _require_str(row, "metric_id")
        _require_str(row, "statistic")
        orientation = _require_str(row, "orientation")
        if orientation not in ("greater", "less", "two-sided"):
            raise SchemaError(f"{path}.orientation invalid {orientation!r}")
        _require_finite_number(row["min_effect_size"], f"{path}.min_effect_size")
        _require_int(row, "n_power", minimum=1)
        _require_int(row, "n_required", minimum=1)
        _require_int(row, "support", minimum=0)
    if not isinstance(data["support"], Mapping):
        raise SchemaError("support must be an object")
    if not isinstance(data["result"], Mapping):
        raise SchemaError("result must be an object")
    _forbid_final_test_leak(data["support"], "support")
    _forbid_final_test_leak(data["result"], "result")


# --------------------------------------------------------------- dispatch
_VALIDATORS = {
    SCHEMA_IDS["asset_manifest"]: validate_asset_manifest,
    SCHEMA_IDS["admission_batch"]: validate_admission_batch,
    SCHEMA_IDS["annotation"]: validate_annotation,
    SCHEMA_IDS["prediction"]: validate_prediction,
    SCHEMA_IDS["calibration_policy"]: validate_calibration_policy,
    SCHEMA_IDS["benchmark_power_plan"]: validate_benchmark_power_plan,
}


def validate(document: object) -> str:
    """Validate a document by its ``schema`` field. Returns the schema id."""
    data = _require_mapping(document, "document")
    if "schema" not in data:
        raise SchemaError("document missing 'schema' field")
    schema_id = data["schema"]
    if not isinstance(schema_id, str):
        raise SchemaError("schema must be a string")
    validator = _VALIDATORS.get(schema_id)
    if validator is None:
        raise SchemaError(
            f"unsupported or unknown schema id {schema_id!r}; "
            f"T2 registry has: {sorted(SCHEMA_REGISTRY)}")
    validator(data)
    return schema_id


def validate_schema_id(document: object, expected_schema_id: str) -> None:
    """Validate and require ``document['schema'] == expected_schema_id``."""
    got = validate(document)
    if got != expected_schema_id:
        raise SchemaError(
            f"expected schema {expected_schema_id!r}, got {got!r}")
