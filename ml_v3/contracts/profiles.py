"""Conditioning profiles, problem types and genre metadata (G1a).

Contract section 4.2 (lines 107-123): the only conditioning profiles are the
seven exposed by the host APVTS, in canonical id order 0..6; string and id must
agree or the record is rejected.

The contract also records the verified state of the current code: the host
parameter exposes exactly those seven choices (PluginProcessor.cpp:651) and
clamps ids to 0..6 (PluginProcessor.cpp:1849), so AIEngine::SourceProfile::Techno
(AIEngine.h:103-113) is unreachable from the host. Therefore the mapping
`Techno -> edm` is a FUTURE V3 adapter policy for benchmark metadata and is NOT
something the current product code performs. It lives here, in the adapter
layer, and nowhere else.

Subgenres other than the frozen legacy `Techno` alias are NOT auto-mapped:
house, breakbeat and the rest keep their metadata and still require an explicit
canonical source_profile.
"""
from __future__ import annotations

from .constants import (ANOMALY_CLASSES, CONDITIONING_PROFILES, ID_TO_PROFILE,
                        PROBLEM_TYPE_TO_ID, PROBLEM_TYPES, PROFILE_TO_ID)

__all__ = [
    "ProfileError",
    "is_canonical_profile", "profile_id", "profile_name",
    "require_profile_consistency",
    "problem_type_id", "require_problem_type_consistency",
    "anomaly_class_index",
    "LEGACY_PROFILE_ALIASES", "map_legacy_profile",
]


class ProfileError(ValueError):
    """Raised on a non-canonical profile, or a string/id disagreement."""


def is_canonical_profile(name: object) -> bool:
    return isinstance(name, str) and name in PROFILE_TO_ID


def profile_id(name: str) -> int:
    if not is_canonical_profile(name):
        raise ProfileError(
            f"non-canonical source_profile {name!r}; allowed: "
            f"{list(CONDITIONING_PROFILES)}")
    return PROFILE_TO_ID[name]


def profile_name(identifier: int) -> str:
    if not isinstance(identifier, int) or isinstance(identifier, bool) \
            or identifier not in ID_TO_PROFILE:
        raise ProfileError(f"non-canonical profile id {identifier!r}; allowed 0..6")
    return ID_TO_PROFILE[identifier]


def require_profile_consistency(name: str, identifier: int) -> None:
    """Both given: they must agree (contract line 111-112)."""
    expected = profile_id(name)
    if identifier != expected:
        raise ProfileError(
            f"profile string/id mismatch: {name!r} is id {expected}, got {identifier!r}")


def problem_type_id(name: str) -> int:
    if not isinstance(name, str) or name not in PROBLEM_TYPE_TO_ID:
        raise ProfileError(
            f"non-canonical problem type {name!r}; allowed: {list(PROBLEM_TYPES)}")
    return PROBLEM_TYPE_TO_ID[name]


def require_problem_type_consistency(name: str, identifier: int) -> None:
    """Contract line 403: an id disagreeing with the string is invalid."""
    expected = problem_type_id(name)
    if identifier != expected:
        raise ProfileError(
            f"problem type string/id mismatch: {name!r} is id {expected}, "
            f"got {identifier!r}")


def anomaly_class_index(name: str) -> int:
    """Index into the dense anomaly surfaces (contract line 459)."""
    if name not in ANOMALY_CLASSES:
        raise ProfileError(
            f"{name!r} is not a dense anomaly class; allowed: {list(ANOMALY_CLASSES)}")
    return ANOMALY_CLASSES.index(name)


# The ONLY legacy compatibility mapping frozen in G1a. It is an adapter policy
# for benchmark metadata; it does not add a host parameter and does not create
# an eighth profile.
LEGACY_PROFILE_ALIASES: dict[str, dict[str, object]] = {
    "Techno": {
        "source_profile": "edm",
        "source_profile_id": 6,
        "electronic_subgenre": "techno",
    },
}


def map_legacy_profile(legacy_name: str) -> dict[str, object]:
    """Map a legacy/internal metadata profile name to the canonical triple.

    Only `Techno` is mapped. Anything else — including other electronic
    subgenres such as house or breakbeat — is rejected: those keep their
    subgenre metadata and require an explicit canonical source_profile.
    """
    if legacy_name in LEGACY_PROFILE_ALIASES:
        return dict(LEGACY_PROFILE_ALIASES[legacy_name])
    raise ProfileError(
        f"no frozen legacy mapping for {legacy_name!r}; only "
        f"{sorted(LEGACY_PROFILE_ALIASES)} is mapped. Subgenres are metadata "
        f"and require an explicit canonical source_profile.")
