"""Schema-field guard for evaluator modules.

Any evaluator code that reads record fields must declare the schema key-set it
expects. Tests validate those declarations against ``schemas.py`` so a module
cannot silently use fields from the wrong record shape.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Final, Iterable

from .schemas import (
    ADMISSION_BATCH_KEYS,
    ANNOTATION_KEYS,
    ASSET_MANIFEST_KEYS,
    BENCHMARK_POWER_PLAN_KEYS,
    CALIBRATION_POLICY_KEYS,
    DYNAMIC_EVENT_KEYS,
    PREDICTION_EVENT_KEYS,
    PREDICTION_KEYS,
    SEMANTIC_BUNDLE_KEYS,
    SEMANTIC_REGION_KEYS,
)

__all__ = [
    "SchemaFieldClaim",
    "SchemaFieldClaimError",
    "SCHEMA_FIELD_KEYSETS",
    "fields_for_schema_record",
    "validate_schema_field_claims",
]


SCHEMA_FIELD_KEYSETS: Final[dict[str, frozenset[str]]] = {
    "asset_manifest": ASSET_MANIFEST_KEYS,
    "admission_batch": ADMISSION_BATCH_KEYS,
    "annotation": ANNOTATION_KEYS,
    "semantic_region": SEMANTIC_REGION_KEYS,
    "dynamic_event": DYNAMIC_EVENT_KEYS,
    "prediction": PREDICTION_KEYS,
    "semantic_bundle": SEMANTIC_BUNDLE_KEYS,
    "prediction_event": PREDICTION_EVENT_KEYS,
    "calibration_policy": CALIBRATION_POLICY_KEYS,
    "benchmark_power_plan": BENCHMARK_POWER_PLAN_KEYS,
}


@dataclass(frozen=True)
class SchemaFieldClaim:
    """A module's explicit claim that it reads fields from one schema record."""

    owner: str
    schema_record: str
    fields: frozenset[str]
    purpose: str


class SchemaFieldClaimError(ValueError):
    """Raised when a module claims fields outside the declared schema."""


def fields_for_schema_record(schema_record: str) -> frozenset[str]:
    """Return the normative key-set for a schema record label."""

    try:
        return SCHEMA_FIELD_KEYSETS[schema_record]
    except KeyError as exc:
        allowed = ", ".join(sorted(SCHEMA_FIELD_KEYSETS))
        raise SchemaFieldClaimError(
            f"unknown schema record {schema_record!r}; allowed: {allowed}") from exc


def validate_schema_field_claims(claims: Iterable[SchemaFieldClaim]) -> None:
    """Fail if any declared field is absent from its schema key-set."""

    errors: list[str] = []
    for claim in claims:
        if not claim.owner:
            errors.append("claim owner must be non-empty")
        if not claim.fields:
            errors.append(f"{claim.owner}: fields must be non-empty")
            continue
        allowed = fields_for_schema_record(claim.schema_record)
        missing = claim.fields - allowed
        if missing:
            errors.append(
                f"{claim.owner}: {claim.schema_record} lacks fields "
                f"{sorted(missing)} for {claim.purpose}")
    if errors:
        raise SchemaFieldClaimError("; ".join(errors))
