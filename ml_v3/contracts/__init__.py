"""Motore v3 contract artifacts (G1a).

G1a freezes contracts, constants, canonical serialization, split guards,
coverage floors, the v2-v3 adapter policy and the §13 metrology lock. It
contains no frontend, no evaluator, no model and no training: see
docs/MOTORE_V3_G1_CONTRACT.md §14 for the phase order.

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
from .metrology_lock import (
    METROLOGY_ARTIFACT_ID,
    MetrologyLockError,
    frozen_metrology_lock,
    metrology_lock_sha256,
    validate_metrology_lock_claim,
)
from .schemas import SCHEMA_REGISTRY, schema_for, schema_ids_t2
from .validate import SchemaError, validate, validate_schema_id

__all__ = [
    "ADAPTER_ARTIFACT_ID",
    "CONTRACT_REVISION",
    "HOMOLOGOUS_CLASSES",
    "MASKED_NA_CLASSES",
    "METROLOGY_ARTIFACT_ID",
    "SCHEMA_IDS",
    "SCHEMA_REGISTRY",
    "AdapterError",
    "MetrologyLockError",
    "SchemaError",
    "adapter_mapping_sha256",
    "frozen_adapter_mapping",
    "frozen_metrology_lock",
    "metrology_lock_sha256",
    "schema_for",
    "schema_ids_t2",
    "validate",
    "validate_adapter_mapping_claim",
    "validate_metrology_lock_claim",
    "validate_schema_id",
]
