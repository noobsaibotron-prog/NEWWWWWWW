"""G1c T0 — canonical gate-9 evaluator fixture registry.

Authority: docs/MOTORE_V3_G1_CONTRACT.md §13.2 gate 9 (REVISIONE 7 + G1c
determinism pins + G1c scope closure).

The contract makes this registry a **precondition** for G1c: parser, matcher
and metrics may not be implemented until exactly these fourteen fixture ids
and their expected outcomes are committed as an artifact — "la registry e un
artifact G1c, non un file implicito nel test".

Why an artifact and not a test list
-----------------------------------
"Producono i fallimenti attesi" is not a specification: each fixture needs its
own expected outcome, pinned before the evaluator exists, so that a later run
cannot quietly redefine what "expected" meant. Serializing the table also makes
it hashable, so a silent edit changes a tracked digest.

Outcome classes (derived from the contract table prefixes, not invented):
  PASS            the fixture must produce a valid report
  FAIL            the fixture must be rejected fail-closed
  PASS_INVARIANT  the fixture must produce a report byte-identical to the base
                  fixture — a stronger claim than PASS, because it also forbids
                  any dependence on row order or on duplicated units.

This module serializes; it does not evaluate. No metric, matcher or parser
lives here.
"""
from __future__ import annotations

from typing import Any

from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import CONTRACT_REVISION

__all__ = [
    "Gate9RegistryError",
    "GATE9_REGISTRY_ARTIFACT_ID",
    "GATE9_REGISTRY_SECTION",
    "GATE9_OUTCOME_CLASSES",
    "GATE9_FIXTURES",
    "gate9_fixture_ids",
    "gate9_outcome",
    "frozen_gate9_registry",
    "gate9_registry_bytes",
    "gate9_registry_sha256",
    "validate_gate9_registry_claim",
]

GATE9_REGISTRY_ARTIFACT_ID = "aieq-v3-gate9-fixture-registry-1"
GATE9_REGISTRY_SECTION = "13.2.gate_9"

GATE9_OUTCOME_CLASSES: tuple[str, ...] = ("PASS", "FAIL", "PASS_INVARIANT")

# Verbatim from the §13.2 gate-9 table, in contract order. The `expected`
# strings are transcribed, not paraphrased: they are the normative text.
GATE9_FIXTURES: tuple[tuple[str, str, str], ...] = (
    ("evaluator_perfect_prediction", "PASS",
     "metriche perfette e report canonico"),
    ("evaluator_empty_prediction", "FAIL",
     "FN/recall non perfetto su GT positivo"),
    ("evaluator_wrong_class", "FAIL",
     "FP classe errata + FN classe corretta"),
    ("evaluator_wrong_frequency", "FAIL",
     "evento non matchabile / errore frequenza oltre tolleranza"),
    ("evaluator_inverted_sign", "FAIL",
     "errore di segno su celle tonali attive"),
    ("evaluator_duplicate_predictions", "FAIL",
     "matching one-to-one lascia duplicato come FP"),
    ("evaluator_row_permutation", "PASS_INVARIANT",
     "report byte-identico alla fixture base"),
    ("evaluator_duplicate_evaluation_unit", "PASS_INVARIANT",
     "dedup canonica, stesso report della base"),
    ("evaluator_missing_anomaly_surface", "FAIL",
     "superficie dichiarata/necessaria assente"),
    ("evaluator_thresholded_score_surface", "FAIL",
     "superficie score pre-threshold non ricostruibile"),
    ("evaluator_zero_events_insufficient_exposure", "FAIL",
     "esposizione insufficiente, mai PASS a zero eventi"),
    ("evaluator_policy_ref_invalid", "FAIL",
     "policy assente o hash policy errato"),
    ("evaluator_policy_mutated_outputs", "FAIL",
     "threshold mutato o actionable non riproducibile"),
    ("evaluator_power_joint_false_events", "FAIL",
     "il gate congiunto passa usando solo Poisson o saltando il limite "
     "cluster-bootstrap"),
)

GATE9_FIXTURE_COUNT = 14


class Gate9RegistryError(ValueError):
    """Raised when a claimed gate-9 registry is malformed or non-canonical."""


def _assert_registry_shape() -> None:
    """Import-time invariants: count, uniqueness, known outcome classes."""
    if len(GATE9_FIXTURES) != GATE9_FIXTURE_COUNT:
        raise RuntimeError(
            f"gate-9 registry must hold exactly {GATE9_FIXTURE_COUNT} fixtures")
    ids = [entry[0] for entry in GATE9_FIXTURES]
    if len(set(ids)) != len(ids):
        raise RuntimeError("duplicate fixture id in gate-9 registry")
    for fixture_id, outcome, expected in GATE9_FIXTURES:
        if outcome not in GATE9_OUTCOME_CLASSES:
            raise RuntimeError(f"unknown outcome class {outcome!r} on {fixture_id!r}")
        if not fixture_id or not expected:
            raise RuntimeError(f"empty id or expected text on {fixture_id!r}")


_assert_registry_shape()


def gate9_fixture_ids() -> tuple[str, ...]:
    """The fourteen fixture ids in contract order."""
    return tuple(entry[0] for entry in GATE9_FIXTURES)


def gate9_outcome(fixture_id: str) -> str:
    """Outcome class for one fixture id. Unknown id is fail-closed."""
    for known_id, outcome, _expected in GATE9_FIXTURES:
        if known_id == fixture_id:
            return outcome
    raise Gate9RegistryError(
        f"unknown gate-9 fixture id {fixture_id!r}; allowed: "
        f"{list(gate9_fixture_ids())}")


def frozen_gate9_registry() -> dict[str, Any]:
    """Deep copy of the frozen gate-9 fixture registry artifact."""
    return {
        "artifact_id": GATE9_REGISTRY_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": GATE9_REGISTRY_SECTION,
        "envelope_authority": (
            "transcription of the §13.2 gate-9 fixture table; ids and expected "
            "text are contract-normative, outcome classes derive from the "
            "table prefixes (PASS / FAIL / PASS-INVARIANT)"
        ),
        "fixture_count": GATE9_FIXTURE_COUNT,
        "outcome_classes": list(GATE9_OUTCOME_CLASSES),
        "pass_invariant_meaning": (
            "report byte-identical to the base fixture; stronger than PASS "
            "because it also forbids dependence on row order or duplicated "
            "evaluation_unit_id"
        ),
        "fixtures": [
            {"id": fixture_id, "outcome": outcome, "expected": expected}
            for fixture_id, outcome, expected in GATE9_FIXTURES
        ],
        "scope": (
            "registry only: this artifact pins ids and expected outcomes. It "
            "does not implement parser, matcher, metrics or the fixtures "
            "themselves, and it does not authorize evaluator code by itself."
        ),
    }


def gate9_registry_bytes() -> bytes:
    """Canonical JSON bytes of the frozen registry (final LF included)."""
    return canonical_bytes(frozen_gate9_registry())


def gate9_registry_sha256() -> str:
    """SHA-256 of the canonical frozen registry artifact."""
    return sha256_of_obj(frozen_gate9_registry())


def validate_gate9_registry_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed gate-9 registry artifact.

    Rejects malformed types, wrong count, renamed or reordered ids, mutated
    expected text and unknown outcome classes. Byte-identity with the frozen
    registry is required: a registry that differs anywhere is not this registry.
    """
    if not isinstance(claim, dict):
        raise Gate9RegistryError(
            f"gate-9 registry claim must be an object, got {type(claim).__name__}")

    frozen = frozen_gate9_registry()

    if claim.get("artifact_id") != GATE9_REGISTRY_ARTIFACT_ID:
        raise Gate9RegistryError(
            f"artifact_id must be {GATE9_REGISTRY_ARTIFACT_ID!r}, "
            f"got {claim.get('artifact_id')!r}")
    if claim.get("contract_revision") != CONTRACT_REVISION:
        raise Gate9RegistryError("contract_revision does not match the frozen constant")
    if claim.get("section") != GATE9_REGISTRY_SECTION:
        raise Gate9RegistryError(
            f"section must be {GATE9_REGISTRY_SECTION!r}, got {claim.get('section')!r}")

    fixtures = claim.get("fixtures")
    if not isinstance(fixtures, list):
        raise Gate9RegistryError("fixtures must be a list")
    if len(fixtures) != GATE9_FIXTURE_COUNT:
        raise Gate9RegistryError(
            f"gate-9 registry must hold exactly {GATE9_FIXTURE_COUNT} fixtures, "
            f"got {len(fixtures)}")
    if claim.get("fixture_count") != GATE9_FIXTURE_COUNT:
        raise Gate9RegistryError(
            f"fixture_count must be {GATE9_FIXTURE_COUNT}")

    seen: set[str] = set()
    for position, row in enumerate(fixtures):
        if not isinstance(row, dict):
            raise Gate9RegistryError(f"fixture row {position} is not an object")
        if set(row) != {"id", "outcome", "expected"}:
            raise Gate9RegistryError(
                f"fixture row {position} must have exactly id/outcome/expected, "
                f"got {sorted(row)}")
        fixture_id = row["id"]
        if not isinstance(fixture_id, str) or not fixture_id:
            raise Gate9RegistryError(f"fixture row {position} has an empty id")
        if fixture_id in seen:
            raise Gate9RegistryError(f"duplicate fixture id {fixture_id!r}")
        seen.add(fixture_id)
        if row["outcome"] not in GATE9_OUTCOME_CLASSES:
            raise Gate9RegistryError(
                f"unknown outcome class {row['outcome']!r} on {fixture_id!r}; "
                f"allowed: {list(GATE9_OUTCOME_CLASSES)}")

    # Contract order is part of the artifact: a reordered registry is a
    # different artifact, because the table is normative as written.
    claimed_ids = [row["id"] for row in fixtures]
    if claimed_ids != list(gate9_fixture_ids()):
        raise Gate9RegistryError(
            "fixture ids differ from the frozen §13.2 gate-9 table "
            "(renamed, dropped, added or reordered)")

    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise Gate9RegistryError(
            f"claim is not canonically serializable: {exc}") from exc
    if claim_digest != sha256_of_obj(frozen):
        raise Gate9RegistryError(
            "gate-9 registry claim is not byte-identical to the frozen "
            f"registry (claim_sha256={claim_digest}, "
            f"frozen_sha256={sha256_of_obj(frozen)})")

    return claim
