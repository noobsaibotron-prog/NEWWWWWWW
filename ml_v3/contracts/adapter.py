"""Frozen Motore v2↔v3 homologous adapter mapping (G1a T3).

Authority: docs/MOTORE_V3_G1_CONTRACT.md §10.5 @ 6d254d0a.

G1a serializes the mapping, constants and hash already defined in the contract;
it does not choose or modify them (§10.5 final sentence). This module freezes
that packaging for deterministic hashing. It does NOT evaluate macro-F1, run
windows, or implement the G1c homologous evaluator.

Honesty / A15:
  §10.5 states the semantics in prose and does not publish a literal JSON key
  table. Envelope keys below are therefore freeze-from-prose packaging of that
  section (plus the Techno→edm legacy alias already frozen in profiles.py).
  Do not claim a letteral exact-key table in the contract document.
"""
from __future__ import annotations

from copy import deepcopy
from typing import Any

from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import CONTRACT_REVISION, PROBLEM_TYPES
from .profiles import LEGACY_PROFILE_ALIASES

__all__ = [
    "AdapterError",
    "ADAPTER_ARTIFACT_ID",
    "ADAPTER_SECTION",
    "HOMOLOGOUS_CLASSES",
    "MASKED_NA_CLASSES",
    "WINDOW_STEP",
    "MIN_WINDOW_CENTERS",
    "OCCUPANCY_THRESHOLD",
    "PER_CLASS_SUPPORT_FLOORS",
    "frozen_adapter_mapping",
    "adapter_mapping_bytes",
    "adapter_mapping_sha256",
    "validate_adapter_mapping_claim",
    "require_homologous_class",
    "is_masked_na_class",
]

ADAPTER_ARTIFACT_ID = "aieq-v3-adapter-v2-v3-mapping-1"
ADAPTER_SECTION = "10.5"

# §10.5: six classes active in all three G0 candidates; macro-F1 denominator 6.
HOMOLOGOUS_CLASSES: tuple[str, ...] = (
    "Resonance",
    "Muddiness",
    "Boominess",
    "Thinness",
    "BoxyMidrange",
    "DullSound",
)

# §10.5: masked in G0 provenance; remain N/A in the v2 comparison.
MASKED_NA_CLASSES: tuple[str, ...] = ("Harshness", "Sibilance")

WINDOW_STEP: int = 16
MIN_WINDOW_CENTERS: int = 20
OCCUPANCY_THRESHOLD: float = 0.05  # 5%

# Per-class floors on development-metric and final-test (§10.5).
# Consequence: any of the six homologous classes below floor → gate NO-GO.
PER_CLASS_SUPPORT_FLOORS: dict[str, object] = {
    "positive_groups": 30,
    "negative_groups": 30,
    "roles": ("development-metric", "final-test"),
    "insufficient_support_consequence": (
        "insufficient support on any of the six homologous classes "
        "renders the gate NO-GO"
    ),
}

# G2-vs-v2 improvement rule (§10.5). Serialized here; not evaluated in G1a.
_G2_VS_V2_GATES: dict[str, object] = {
    "macro_f1_relative_improvement": 0.10,
    "macro_f1_absolute_when_seed_below": {
        "seed_threshold": 0.10,
        "absolute_delta": 0.10,
    },
    "fp_group_rate_not_worse_than_seed": True,
    "no_cross_seed_composition": True,
}


class AdapterError(ValueError):
    """Raised when an adapter mapping claim is malformed or non-canonical."""


def _assert_partition_complete() -> None:
    """Internal sanity: homologous ∪ masked == eight public types, disjoint."""
    homologous = set(HOMOLOGOUS_CLASSES)
    masked = set(MASKED_NA_CLASSES)
    if homologous & masked:
        raise RuntimeError("homologous and masked_na classes overlap")
    if homologous | masked != set(PROBLEM_TYPES):
        raise RuntimeError("homologous ∪ masked_na must equal PROBLEM_TYPES")
    if len(HOMOLOGOUS_CLASSES) != 6:
        raise RuntimeError("macro-F1 denominator requires exactly six classes")


_assert_partition_complete()


def frozen_adapter_mapping() -> dict[str, Any]:
    """Return a deep copy of the frozen §10.5 adapter mapping artifact.

    Envelope keys are freeze-from-prose packaging (see module docstring).
    """
    return {
        "artifact_id": ADAPTER_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": ADAPTER_SECTION,
        "envelope_authority": (
            "freeze-from-prose packaging of §10.5; contract has no literal "
            "JSON key table for this artifact"
        ),
        "homologous_classes": list(HOMOLOGOUS_CLASSES),
        "macro_f1_denominator": 6,
        "masked_na_classes": list(MASKED_NA_CLASSES),
        "masked_na_policy": (
            "Harshness and Sibilance stay N/A in the v2 comparison; never "
            "treat them as v2 negatives. v3 must clear absolute and G2 gates."
        ),
        "temporal_grid": {
            "window_step": WINDOW_STEP,
            "min_window_centers": MIN_WINDOW_CENTERS,
            "occupancy_threshold": OCCUPANCY_THRESHOLD,
            "short_segment_semantics": (
                "segment with fewer than min_window_centers is N/A for the "
                "homologous adapter; still available to native v3 metrics"
            ),
        },
        "label_at_center": {
            "v2": (
                "one if probability exceeds the provenance threshold of the seed"
            ),
            "v3": (
                "one if the center falls in the temporal support of an "
                "actionable bundle or event of the class; a static bundle "
                "covers the prediction segment"
            ),
            "gt": (
                "one if the center falls in an actionable semantic region or "
                "GT event"
            ),
        },
        "segment_presence": (
            "one for each of the three vectors only when the respective "
            "occupancy on the same grid is at least occupancy_threshold"
        ),
        "per_class_support_floors": {
            "positive_groups": PER_CLASS_SUPPORT_FLOORS["positive_groups"],
            "negative_groups": PER_CLASS_SUPPORT_FLOORS["negative_groups"],
            "roles": list(PER_CLASS_SUPPORT_FLOORS["roles"]),
            "insufficient_support_consequence": (
                PER_CLASS_SUPPORT_FLOORS["insufficient_support_consequence"]
            ),
        },
        "metrics": {
            "macro_f1_classes": 6,
            "false_positive_group_rate_on_clean": True,
            "invent_v2_curves_frequency_severity": False,
        },
        "structural_na_surfaces_v2": [
            "tonal_curves",
            "events",
            "frequency",
            "severity",
        ],
        # Structural absence → N/A; declared-but-invalid → fail-closed, never N/A.
        "na_semantics": (
            "N/A does not enter macro-averages, does not satisfy a gate, and "
            "does not demonstrate improvement; structural absence is N/A, "
            "never zero/infinity/FAIL; a candidate that declares a surface but "
            "emits no valid prediction is fail-closed (FN, schema error or "
            "candidate failure per case), never N/A"
        ),
        "g2_vs_v2_gates": deepcopy(_G2_VS_V2_GATES),
        # Extend the T1 legacy alias; do not invent a second Techno mapping.
        "legacy_profile_aliases": deepcopy(LEGACY_PROFILE_ALIASES),
    }


def adapter_mapping_bytes() -> bytes:
    """Canonical JSON bytes of the frozen mapping (final LF included)."""
    return canonical_bytes(frozen_adapter_mapping())


def adapter_mapping_sha256() -> str:
    """SHA-256 of the canonical frozen mapping artifact."""
    return sha256_of_obj(frozen_adapter_mapping())


def is_masked_na_class(name: str) -> bool:
    return name in MASKED_NA_CLASSES


def require_homologous_class(name: str) -> str:
    """Accept only one of the six homologous classes; reject masked N/A."""
    if name in MASKED_NA_CLASSES:
        raise AdapterError(
            f"{name!r} is masked N/A in the v2 homologous adapter; not a "
            f"homologous class")
    if name not in HOMOLOGOUS_CLASSES:
        raise AdapterError(
            f"non-homologous class {name!r}; allowed: "
            f"{list(HOMOLOGOUS_CLASSES)}")
    return name


def _require_exact_list(claim: dict[str, Any], key: str,
                        expected: list[str]) -> None:
    value = claim.get(key)
    if value != expected:
        raise AdapterError(
            f"adapter mapping claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def _require_exact(claim: dict[str, Any], key: str, expected: object) -> None:
    value = claim.get(key)
    if value != expected:
        raise AdapterError(
            f"adapter mapping claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def validate_adapter_mapping_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed adapter mapping artifact.

    Rejects malformed types, mutated constants, non-canonical class sets,
    and any claim that would treat Harshness/Sibilance as homologous or as
    v2 negatives. Byte-identity with the frozen canonical mapping is required.
    """
    if not isinstance(claim, dict):
        raise AdapterError(
            f"adapter mapping claim must be an object, got {type(claim).__name__}")

    frozen = frozen_adapter_mapping()

    _require_exact(claim, "artifact_id", ADAPTER_ARTIFACT_ID)
    _require_exact(claim, "contract_revision", CONTRACT_REVISION)
    _require_exact(claim, "section", ADAPTER_SECTION)
    _require_exact_list(claim, "homologous_classes", list(HOMOLOGOUS_CLASSES))
    _require_exact(claim, "macro_f1_denominator", 6)
    _require_exact_list(claim, "masked_na_classes", list(MASKED_NA_CLASSES))

    temporal = claim.get("temporal_grid")
    if not isinstance(temporal, dict):
        raise AdapterError("temporal_grid must be an object")
    if temporal.get("window_step") != WINDOW_STEP:
        raise AdapterError(
            f"window_step must be {WINDOW_STEP}, got {temporal.get('window_step')!r}")
    if temporal.get("min_window_centers") != MIN_WINDOW_CENTERS:
        raise AdapterError(
            f"min_window_centers must be {MIN_WINDOW_CENTERS}, got "
            f"{temporal.get('min_window_centers')!r}")
    if temporal.get("occupancy_threshold") != OCCUPANCY_THRESHOLD:
        raise AdapterError(
            f"occupancy_threshold must be {OCCUPANCY_THRESHOLD}, got "
            f"{temporal.get('occupancy_threshold')!r}")

    floors = claim.get("per_class_support_floors")
    if not isinstance(floors, dict):
        raise AdapterError("per_class_support_floors must be an object")
    if floors.get("positive_groups") != 30 or floors.get("negative_groups") != 30:
        raise AdapterError(
            "per_class_support_floors must require 30 positive and 30 negative "
            f"groups, got {floors!r}")
    if floors.get("roles") != ["development-metric", "final-test"]:
        raise AdapterError(
            "per_class_support_floors.roles must be "
            "['development-metric', 'final-test']")
    if floors.get("insufficient_support_consequence") != (
            PER_CLASS_SUPPORT_FLOORS["insufficient_support_consequence"]):
        raise AdapterError(
            "per_class_support_floors.insufficient_support_consequence must "
            "state that insufficient support on any of the six homologous "
            "classes renders the gate NO-GO")

    expected_na = frozen["na_semantics"]
    if claim.get("na_semantics") != expected_na:
        raise AdapterError(
            "na_semantics must include both structural-absence→N/A and "
            "declared-but-invalid→fail-closed (never N/A)")

    aliases = claim.get("legacy_profile_aliases")
    if aliases != LEGACY_PROFILE_ALIASES:
        raise AdapterError(
            "legacy_profile_aliases must match profiles.LEGACY_PROFILE_ALIASES "
            "(Techno→edm only); do not invent a divergent Techno mapping")

    # Reject any claim that silently promotes masked classes into homologous.
    homologous = claim.get("homologous_classes")
    if isinstance(homologous, list):
        for name in MASKED_NA_CLASSES:
            if name in homologous:
                raise AdapterError(
                    f"{name!r} must stay masked N/A; cannot appear in "
                    f"homologous_classes")

    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise AdapterError(f"claim is not canonically serializable: {exc}") from exc

    frozen_digest = sha256_of_obj(frozen)
    if claim_digest != frozen_digest:
        raise AdapterError(
            "adapter mapping claim is not byte-identical to the frozen §10.5 "
            f"mapping (claim_sha256={claim_digest}, "
            f"frozen_sha256={frozen_digest})")

    return claim
