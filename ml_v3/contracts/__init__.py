"""Motore v3 contract artifacts (G1a).

G1a freezes contracts, constants, canonical serialization, split guards,
coverage floors, the v2-v3 adapter policy, the §13 metrology lock and the
§13 fixture-spec (M2; generators are T6). It contains no frontend, no
evaluator, no model and no training: see docs/MOTORE_V3_G1_CONTRACT.md
§14 for the phase order.

Authority: MOTORE_V3_G1_CONTRACT REVISIONE 7 CONSOLIDATA @ 6fbf5b59
(ancestor REV6 @ 6d254d0a).
"""
from .gate9_registry import (
    GATE9_FIXTURES,
    Gate9RegistryError,
    frozen_gate9_registry,
    gate9_fixture_ids,
    gate9_outcome,
    gate9_registry_sha256,
    validate_gate9_registry_claim,
)
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
from .fixture_spec import (
    FIXTURE_SPEC_ARTIFACT_ID,
    FixtureSpecError,
    fixture_spec_sha256,
    frozen_fixture_spec,
    validate_fixture_spec_claim,
)
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
    "FIXTURE_SPEC_ARTIFACT_ID",
    "GATE9_FIXTURES",
    "HOMOLOGOUS_CLASSES",
    "MASKED_NA_CLASSES",
    "METROLOGY_ARTIFACT_ID",
    "SCHEMA_IDS",
    "SCHEMA_REGISTRY",
    "AdapterError",
    "FixtureSpecError",
    "Gate9RegistryError",
    "MetrologyLockError",
    "SchemaError",
    "adapter_mapping_sha256",
    "fixture_spec_sha256",
    "frozen_adapter_mapping",
    "frozen_fixture_spec",
    "frozen_gate9_registry",
    "frozen_metrology_lock",
    "gate9_fixture_ids",
    "gate9_outcome",
    "gate9_registry_sha256",
    "metrology_lock_sha256",
    "schema_for",
    "schema_ids_t2",
    "validate",
    "validate_adapter_mapping_claim",
    "validate_fixture_spec_claim",
    "validate_gate9_registry_claim",
    "validate_metrology_lock_claim",
    "validate_schema_id",
]
