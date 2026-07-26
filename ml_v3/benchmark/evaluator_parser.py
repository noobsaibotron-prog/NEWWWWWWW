"""G1c T1 — fail-closed evaluator input parser (§10).

Authority: docs/MOTORE_V3_G1_CONTRACT.md §10 opening —

    "L'evaluator legge soltanto manifest, annotation record e prediction record
     con schema/versione/hash compatibili. L'ordine dei file non puo cambiare
     i risultati."

That sentence carries two obligations that this module implements and nothing
else does:

1. **Admission is fail-closed.** Only the three record types, only their own
   schema ids, only after their T2 validator passes, only when the hashes they
   cite actually resolve. Anything else is rejected — never coerced, never
   ignored.
2. **File order is not information.** The parsed result is canonically ordered
   per the §10.0 pin (records sorted by canonical key, group ordering by UTF-8
   `group_id` bytes), so permuting the inputs produces byte-identical output.
   This is asserted, not assumed: the tests compare canonical bytes across
   permutations.

What this module deliberately does NOT do: matching, metrics, CI, power. It
produces the ordered, cross-bound, hash-checked input those steps consume.

Cross-binding
-------------
Prediction and annotation records do not carry `group_id`; the manifest does.
Resolving it here is what makes group-level aggregation possible downstream —
and it is why an orphan record (asset_id absent from the manifest) must be
rejected rather than silently dropped: a dropped record is a silently smaller
denominator.

Run coherence
-------------
One evaluator run is one candidate. Predictions citing different
`model_sha256`, different `frontend_contract_sha256` or different
`calibration_policy_sha256` are not one run, and a report mixing them would be
meaningless. §10 already forbids a policy that is absent or not committed; this
extends the same reasoning to the model and frontend the predictions claim.
"""
from __future__ import annotations

from typing import Any, Iterable, Mapping

from ml_v3.contracts.canonical import canonical_bytes, sha256_of_obj
from ml_v3.contracts.constants import SCHEMA_IDS
from ml_v3.contracts.validate import (
    SchemaError,
    validate_annotation,
    validate_asset_manifest,
    validate_calibration_policy,
    validate_prediction,
)

__all__ = [
    "EvaluatorParseError",
    "PARSED_ARTIFACT_ID",
    "parse_evaluator_inputs",
    "parsed_inputs_bytes",
    "parsed_inputs_sha256",
]

PARSED_ARTIFACT_ID = "aieq-v3-evaluator-parsed-inputs-1"

_MANIFEST = SCHEMA_IDS["asset_manifest"]
_ANNOTATION = SCHEMA_IDS["annotation"]
_PREDICTION = SCHEMA_IDS["prediction"]
_POLICY = SCHEMA_IDS["calibration_policy"]


class EvaluatorParseError(ValueError):
    """Raised on any inadmissible evaluator input. Never self-corrects."""


def _require_schema(record: object, expected: str, slot: str, position: int) -> Mapping[str, Any]:
    """Reject wrong type, missing schema, and right-schema-wrong-slot."""
    if not isinstance(record, Mapping):
        raise EvaluatorParseError(
            f"{slot}[{position}] must be a JSON object, got {type(record).__name__}")
    if "schema" not in record:
        raise EvaluatorParseError(f"{slot}[{position}] has no 'schema' field")
    got = record["schema"]
    if got != expected:
        raise EvaluatorParseError(
            f"{slot}[{position}] carries schema {got!r}; this slot admits only "
            f"{expected!r} (a valid record in the wrong slot is still a reject)")
    return record


def _validated(record: Mapping[str, Any], validator, slot: str, position: int) -> dict[str, Any]:
    try:
        validator(record)
    except SchemaError as exc:
        raise EvaluatorParseError(f"{slot}[{position}] fails its schema: {exc}") from exc
    return dict(record)


def _manifest_index(records: Iterable[object]) -> dict[str, dict[str, Any]]:
    """Asset id → manifest record, rejecting duplicates."""
    index: dict[str, dict[str, Any]] = {}
    for position, raw in enumerate(records):
        record = _validated(
            _require_schema(raw, _MANIFEST, "manifest", position),
            validate_asset_manifest, "manifest", position)
        asset_id = record["asset_id"]
        if asset_id in index:
            raise EvaluatorParseError(
                f"duplicate manifest asset_id {asset_id!r}; the manifest is the "
                "identity index and cannot declare an asset twice")
        index[asset_id] = record
    if not index:
        raise EvaluatorParseError("manifest is empty; nothing to evaluate")
    return index


def _bind_to_manifest(record: Mapping[str, Any], manifest: Mapping[str, dict[str, Any]],
                      slot: str, position: int) -> dict[str, Any]:
    """Resolve group_id from the manifest; reject orphans and profile drift."""
    asset_id = record["asset_id"]
    asset = manifest.get(asset_id)
    if asset is None:
        raise EvaluatorParseError(
            f"{slot}[{position}] refers to asset_id {asset_id!r} absent from the "
            "manifest; an orphan record is rejected, never dropped")
    if record["profile"] != asset["source_profile"]:
        raise EvaluatorParseError(
            f"{slot}[{position}] profile {record['profile']!r} disagrees with "
            f"manifest source_profile {asset['source_profile']!r} for {asset_id!r}")
    return {
        "group_id": asset["group_id"],
        "group_primary_profile": asset["group_primary_profile"],
        "group_primary_domain": asset["group_primary_domain"],
        "split_role": asset["split_role"],
    }


def _annotation_key(record: Mapping[str, Any], bound: Mapping[str, Any]) -> tuple:
    return (
        bound["group_id"].encode("utf-8"),
        record["asset_id"].encode("utf-8"),
        record["evaluation_unit_id"].encode("utf-8"),
        float(record["segment_start_s"]),
        float(record["segment_end_s"]),
        record["annotator_id"].encode("utf-8"),
        record["pass_id"].encode("utf-8"),
    )


def _prediction_key(record: Mapping[str, Any], bound: Mapping[str, Any]) -> tuple:
    return (
        bound["group_id"].encode("utf-8"),
        record["asset_id"].encode("utf-8"),
        float(record["segment_start_s"]),
        float(record["segment_end_s"]),
    )


def _require_run_coherence(predictions: list[dict[str, Any]]) -> dict[str, str]:
    """One run is one candidate: model, frontend and policy hash must agree."""
    fields = ("model_sha256", "frontend_contract_sha256",
              "calibration_policy_sha256", "calibration_policy_id", "model_id")
    run: dict[str, str] = {}
    for field in fields:
        values = {entry["record"][field] for entry in predictions}
        if len(values) != 1:
            raise EvaluatorParseError(
                f"predictions disagree on {field}: {sorted(values)}; one report "
                "cannot mix candidates")
        run[field] = values.pop()
    return run


def parse_evaluator_inputs(
    *,
    manifest_records: Iterable[object],
    annotation_records: Iterable[object],
    prediction_records: Iterable[object],
    calibration_policy: object,
) -> dict[str, Any]:
    """Admit and canonically order evaluator inputs, or fail closed.

    Returns a structure whose canonical bytes are invariant under any
    permutation of the three input sequences.
    """
    manifest = _manifest_index(manifest_records)

    policy = _validated(
        _require_schema(calibration_policy, _POLICY, "calibration_policy", 0),
        validate_calibration_policy, "calibration_policy", 0)
    policy_digest = sha256_of_obj(policy)

    annotations: list[dict[str, Any]] = []
    for position, raw in enumerate(annotation_records):
        record = _validated(
            _require_schema(raw, _ANNOTATION, "annotation", position),
            validate_annotation, "annotation", position)
        bound = _bind_to_manifest(record, manifest, "annotation", position)
        annotations.append({"record": record, "bound": bound})

    predictions: list[dict[str, Any]] = []
    for position, raw in enumerate(prediction_records):
        record = _validated(
            _require_schema(raw, _PREDICTION, "prediction", position),
            validate_prediction, "prediction", position)
        bound = _bind_to_manifest(record, manifest, "prediction", position)
        if record["calibration_policy_sha256"] != policy_digest:
            raise EvaluatorParseError(
                f"prediction[{position}] cites calibration_policy_sha256 "
                f"{record['calibration_policy_sha256']} but the supplied policy "
                f"hashes to {policy_digest}; the evaluator loads the policy by "
                "hash and rejects any difference")
        predictions.append({"record": record, "bound": bound})

    if not predictions:
        raise EvaluatorParseError(
            "no prediction records; an empty prediction set is not a PASS")
    run = _require_run_coherence(predictions)

    # Duplicate detection on the canonical key, after binding.
    for label, rows, key_fn in (
        ("annotation", annotations, _annotation_key),
        ("prediction", predictions, _prediction_key),
    ):
        seen: set[tuple] = set()
        for entry in rows:
            key = key_fn(entry["record"], entry["bound"])
            if key in seen:
                raise EvaluatorParseError(
                    f"duplicate {label} for the same canonical key "
                    f"(asset {entry['record']['asset_id']!r}); duplicates change "
                    "the denominator and are rejected")
            seen.add(key)

    annotations.sort(key=lambda e: _annotation_key(e["record"], e["bound"]))
    predictions.sort(key=lambda e: _prediction_key(e["record"], e["bound"]))
    manifest_sorted = sorted(
        manifest.values(),
        key=lambda r: (r["group_id"].encode("utf-8"), r["asset_id"].encode("utf-8")))

    return {
        "artifact_id": PARSED_ARTIFACT_ID,
        "run": run,
        "calibration_policy_sha256": policy_digest,
        "counts": {
            "manifest": len(manifest_sorted),
            "annotations": len(annotations),
            "predictions": len(predictions),
        },
        "manifest": manifest_sorted,
        "annotations": [
            {**entry["bound"], "record": entry["record"]} for entry in annotations],
        "predictions": [
            {**entry["bound"], "record": entry["record"]} for entry in predictions],
    }


def parsed_inputs_bytes(parsed: Mapping[str, Any]) -> bytes:
    """Canonical JSON bytes of a parsed-input structure."""
    return canonical_bytes(parsed)


def parsed_inputs_sha256(parsed: Mapping[str, Any]) -> str:
    """SHA-256 of the canonical parsed-input structure."""
    return sha256_of_obj(parsed)
