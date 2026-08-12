"""REV8 §9.6 / §10.1 — evaluation-unit key, instance identity, decision key.

Authority: docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md §9.6 and §10.1.

    §9.6 "L'identita di un'evaluation unit e la tupla normalizzata
          `(evaluation_unit_id, asset_id, profile, segment_start_s,
          segment_end_s)`. I tempi usano `N64`; l'ID da solo non identifica
          un'unita."
    §9.6 "L'ID e SHA-256 dei byte canonici di
          `["motore-v3-instance-id-v1", instance_key]`. […] ID, digest e ordine
          degli hash non partecipano a eleggibilita, costi o tie-break."
    §10.1 "La `decision_key` normalizzata contiene kind, evaluation-unit key,
          tipo, tempi, tutta la geometria letta da eleggibilita/costi,
          direction, severity, confidence e actionable. Non contiene ID o hash."

CANDIDATE, NOT ACTIVE. Reachable only by explicit import path; not exported by
`ml_v3.contracts.__init__` until the atomic switch of §14.1.

Two keys, deliberately not one
------------------------------
`semantic_payload` and `decision_key` carry almost the same fields. They differ
by exactly one: the payload includes `problem_type_id`, the decision key does
not, because §10.1 enumerates the decision key's contents and that field is not
among them — it is redundant with `problem_type`, which a validator requires it
to agree with, and it is read by no eligibility rule or cost. They are built by
one function with a flag so the two can never drift apart silently, and
`test_g1c_rev8_primitives_v2` pins the difference to that single field.

Identity is derived, never chosen
---------------------------------
§10.2's lexicographic objective used to terminate on record ids. If a submitter
could choose an id, it could choose the winner among otherwise equivalent
matchings. Here the id is a function of the content, so "choosing the id" means
changing the geometry — which changes matchability and cost. The lever is gone
rather than watched. Ordering, correspondingly, is done on the decision key and
never on an id or a digest.
"""
from __future__ import annotations

import hashlib
from typing import Any, Iterable, Mapping, Sequence

from .normalize_v2 import n64, normalized_canonical_bytes

__all__ = [
    "IdentityError",
    "INSTANCE_ID_DOMAIN",
    "REGION_KINDS",
    "EVENT_KINDS",
    "PREDICTION_KINDS",
    "evaluation_unit_key",
    "annotation_top_level_key",
    "semantic_payload",
    "decision_key",
    "assign_occurrence_ordinals",
    "instance_key",
    "instance_id",
]

INSTANCE_ID_DOMAIN = "motore-v3-instance-id-v1"

# §9.6 names four record kinds; the labels match the schema-record vocabulary
# already used by `schema_field_guard.SCHEMA_FIELD_KEYSETS`.
REGION_KINDS = ("semantic_region", "semantic_bundle")
EVENT_KINDS = ("dynamic_event", "prediction_event")
PREDICTION_KINDS = ("semantic_bundle", "prediction_event")
_ALL_KINDS = REGION_KINDS + EVENT_KINDS

_UNIT_KEY_FIELDS = ("evaluation_unit_id", "asset_id", "profile")


class IdentityError(ValueError):
    """Raised on a record identity that cannot be derived fail-closed."""


def _require(record: Mapping[str, Any], field: str, label: str) -> Any:
    if field not in record:
        raise IdentityError(f"{label} has no {field}")
    return record[field]


def _text(record: Mapping[str, Any], field: str, label: str) -> str:
    value = _require(record, field, label)
    if not isinstance(value, str) or not value:
        raise IdentityError(f"{label}.{field} must be a non-empty string")
    return value


def _kind(kind: str) -> str:
    if kind not in _ALL_KINDS:
        raise IdentityError(
            f"unknown record kind {kind!r}; allowed: {', '.join(_ALL_KINDS)}")
    return kind


def evaluation_unit_key(record: Mapping[str, Any]) -> list[Any]:
    """§9.6 evaluation-unit identity: the whole tuple, times normalised."""
    label = "record"
    key = [_text(record, field, label) for field in _UNIT_KEY_FIELDS]
    key.append(n64(_require(record, "segment_start_s", label)))
    key.append(n64(_require(record, "segment_end_s", label)))
    return key


def annotation_top_level_key(record: Mapping[str, Any]) -> list[Any]:
    """§9.6: the annotation key is the unit key plus annotator and pass."""
    return [
        evaluation_unit_key(record),
        _text(record, "annotator_id", "annotation"),
        _text(record, "pass_id", "annotation"),
    ]


def _geometry(kind: str, record: Mapping[str, Any], label: str) -> list[Any]:
    if kind in REGION_KINDS:
        return [
            n64(_require(record, "band_lo_hz", label)),
            n64(_require(record, "band_hi_hz", label)),
            _center(record, label),
            _direction(record, label),
        ]
    return [
        n64(_require(record, "center_hz", label)),
        n64(_require(record, "width_octaves", label)),
    ]


def _center(record: Mapping[str, Any], label: str) -> Any:
    """`center_hz` is a key on every region and bundle; it is null off Resonance."""
    value = _require(record, "center_hz", label)
    return None if value is None else n64(value)


def _direction(record: Mapping[str, Any], label: str) -> Any:
    value = _require(record, "direction", label)
    if value is None:
        return None
    if not isinstance(value, str) or not value:
        raise IdentityError(f"{label}.direction must be a non-empty string or null")
    return value


def _payload(kind: str, record: Mapping[str, Any], *, with_type_id: bool) -> list[Any]:
    kind = _kind(kind)
    label = kind
    body: list[Any] = [kind, evaluation_unit_key(record),
                       _text(record, "problem_type", label)]
    if with_type_id:
        type_id = _require(record, "problem_type_id", label)
        if isinstance(type_id, bool) or not isinstance(type_id, int):
            raise IdentityError(f"{label}.problem_type_id must be an integer")
        body.append(type_id)
    body.append(n64(_require(record, "start_s", label)))
    body.append(n64(_require(record, "end_s", label)))
    body.extend(_geometry(kind, record, label))
    body.append(n64(_require(record, "severity", label)))
    body.append(n64(_require(record, "confidence", label)))
    actionable = _require(record, "actionable", label)
    if not isinstance(actionable, bool):
        raise IdentityError(f"{label}.actionable must be a boolean")
    body.append(actionable)
    return body


def semantic_payload(kind: str, record: Mapping[str, Any]) -> list[Any]:
    """§9.6 `semantic_payload`, in the field order the contract prints."""
    return _payload(kind, record, with_type_id=True)


def decision_key(kind: str, record: Mapping[str, Any]) -> list[Any]:
    """§10.1 decision key: the payload without `problem_type_id`, no ids, no hashes."""
    return _payload(kind, record, with_type_id=False)


def assign_occurrence_ordinals(
    kind: str,
    records: Iterable[Mapping[str, Any]],
) -> list[int]:
    """§9.6 ordinals: identical payloads get 0..N-1, independent of input order.

    Returned positionally against `records`. Ground-truth kinds are rejected:
    §9.6 gives ordinals to prediction bundles and events only, because a
    structurally duplicated ground-truth record is invalid rather than counted.
    """
    kind = _kind(kind)
    if kind not in PREDICTION_KINDS:
        raise IdentityError(
            f"occurrence ordinals belong to prediction records; {kind!r} is ground truth")
    seen: dict[bytes, int] = {}
    ordinals: list[int] = []
    for record in records:
        digest = normalized_canonical_bytes(semantic_payload(kind, record))
        ordinal = seen.get(digest, 0)
        seen[digest] = ordinal + 1
        ordinals.append(ordinal)
    return ordinals


def instance_key(
    kind: str,
    record: Mapping[str, Any],
    occurrence_ordinal: int | None = None,
) -> list[Any]:
    """§9.6 instance key: payload plus ordinal for predictions, payload alone for GT."""
    kind = _kind(kind)
    payload = semantic_payload(kind, record)
    if kind in PREDICTION_KINDS:
        if isinstance(occurrence_ordinal, bool) or not isinstance(
                occurrence_ordinal, int):
            raise IdentityError(
                f"{kind} instance key needs an integer occurrence_ordinal")
        if occurrence_ordinal < 0:
            raise IdentityError("occurrence_ordinal must be non-negative")
        return [payload, occurrence_ordinal]
    if occurrence_ordinal is not None:
        raise IdentityError(
            f"{kind} is ground truth and takes no occurrence_ordinal")
    return payload


def instance_id(key: Sequence[Any]) -> str:
    """§9.6: SHA-256 of the canonical bytes of `[domain, instance_key]`."""
    return hashlib.sha256(
        normalized_canonical_bytes([INSTANCE_ID_DOMAIN, key])).hexdigest()
