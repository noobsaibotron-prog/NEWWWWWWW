"""Motore v3 contract artifacts (G1a).

G1a freezes contracts, constants, canonical serialization, split guards,
coverage floors and the v2-v3 adapter policy. It contains no frontend, no
evaluator, no model and no training: see docs/MOTORE_V3_G1_CONTRACT.md
§14 for the phase order.

Authority: MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend
@ 6d254d0a. Draft modules here are migrated to that freeze.
"""
from .adapter import (
    ADAPTER_ARTIFACT_ID,
    HOMOLOGOUS_CLASSES,
    MASKED_NA_CLASSES,
    AdapterError,
    adapter_mapping_sha256,
    frozen_adapter_mapping,
    validate_adapter_mapping_claim,
)
from .constants import CONTRACT_REVISION, SCHEMA_IDS
from .schemas import SCHEMA_REGISTRY, schema_for, schema_ids_t2
from .validate import SchemaError, validate, validate_schema_id

__all__ = [
    "ADAPTER_ARTIFACT_ID",
    "CONTRACT_REVISION",
    "HOMOLOGOUS_CLASSES",
    "MASKED_NA_CLASSES",
    "SCHEMA_IDS",
    "SCHEMA_REGISTRY",
    "AdapterError",
    "SchemaError",
    "adapter_mapping_sha256",
    "frozen_adapter_mapping",
    "schema_for",
    "schema_ids_t2",
    "validate",
    "validate_adapter_mapping_claim",
    "validate_schema_id",
]
