"""Split contract v3: identity, commit-reveal, role and pilot assignment (G1a).

Transcribed from docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a §8 (minimo
byte-level) and §9.1. Nothing here is chosen by G1a.

Byte-level definitions
----------------------
commitment      SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt), salt = 32 raw
                (§8.2.4)
batch id        SHA-256 lowercase hex of canonical roster bytes, trailing LF
                included (§8.2.3)
role            HMAC-SHA256(salt, role message); first 8 bytes BE as value;
                train if value*20 < 11*2**64; validation if < 13*2**64;
                calibration if < 15*2**64; development-metric if < 17*2**64;
                else final-test (§8.2.5). admission_batch_id is the 64 ASCII
                hex bytes — never the raw 32-byte digest.
pilot           HMAC-SHA256(salt, pilot message); all 32 bytes BE as value;
                development_pilot iff role == development-metric AND
                value*4 < 2**256 (§8.2.6). Same hex-64 admission_batch_id.

Identity (§8.2.2)
-----------------
group_id is not free text:
  - with stable upstream: source_family + ":" + upstream_id
  - otherwise pack fallback: source_family + ":pack:" + SHA256(pack_bytes)
    where pack_bytes = concat of (64 ASCII digest + LF) for each lowercase
    SHA-256 sorted by ASCII bytes.
  `:` is forbidden in source_family and upstream_id; `:pack:` is reserved
  for the pack fallback form only. NUL is forbidden in every HMAC component.

source_identity_index schema
----------------------------
Exact-key envelope (additionalProperties forbidden), schema id
`aieq-v3-source-identity-index-1` (SCHEMA_IDS["source_identity_index"]):

  {
    schema,
    source_snapshot_id,
    alias_mapping_version,
    inclusion_rules_version,
    groups: [
      {
        group_id, group_primary_profile, group_primary_domain,
        source_family, upstream_id, pack_audio_sha256, aliases
      }
    ]
  }

`source_snapshot_sha256` on the roster MUST equal
SHA256(canonical_bytes(identity_index)) with that schema; free-form digests
are rejected (§8.2.3 micro-amend).
"""
from __future__ import annotations

import hashlib
import hmac
from typing import Any, Iterable

from .canonical import is_sha256_hex, sha256_of_obj
from .constants import (COMMITMENT_PREFIX, PILOT_PREFIX, PILOT_THRESHOLD_DEN,
                        PILOT_THRESHOLD_NUM, ROLE_INTERVALS_EXACT, ROLE_PREFIX,
                        SCHEMA_IDS, SPLIT_ROLES, SPLIT_SALT_BYTES)
from .profiles import ProfileError, is_canonical_profile

__all__ = [
    "SplitError",
    "reject_nul", "reject_colon", "require_admission_batch_id_hex",
    "pack_group_id", "upstream_group_id", "derive_group_id",
    "validate_identity_index", "source_snapshot_sha256",
    "exact_projection", "roster_from_identity_index", "canonical_roster",
    "verify_roster_source_snapshot", "admission_batch_id",
    "salt_commitment", "verify_commitment",
    "assign_role", "assign_pilot", "assign_batch",
    "validate_manifest_split_invariants",
]

NUL = b"\x00"

# Exact-key sets for aieq-v3-source-identity-index-1 (additionalProperties fail).
_IDENTITY_ROW_KEYS = frozenset({
    "group_id", "group_primary_profile", "group_primary_domain",
    "source_family", "upstream_id", "pack_audio_sha256", "aliases",
})
_IDENTITY_ENVELOPE_KEYS = frozenset({
    "schema", "source_snapshot_id", "alias_mapping_version",
    "inclusion_rules_version", "groups",
})
_ROSTER_ROW_KEYS = ("group_id", "group_primary_profile",
                    "group_primary_domain", "source_family")


class SplitError(ValueError):
    """Raised on any violation of the split contract. Never self-corrects."""


# ------------------------------------------------------------------ hygiene
def reject_nul(value: str, field: str) -> str:
    """NUL is the separator of every commitment/HMAC message: forbid it."""
    if not isinstance(value, str):
        raise SplitError(f"{field} must be a string, got {type(value).__name__}")
    if "\x00" in value:
        raise SplitError(f"{field} contains U+0000, which is the message separator")
    return value


def reject_colon(value: str, field: str) -> str:
    """`:` is forbidden in source_family and upstream_id (§8.2.2)."""
    if ":" in value:
        raise SplitError(
            f"{field} must not contain ':'; reserved for group_id grammar "
            f"(got {value!r})")
    return value


def _require_nonempty(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise SplitError(f"{field} must be a non-empty string")
    return reject_nul(value, field)


def _require_family_or_upstream(value: object, field: str) -> str:
    """Non-empty, no NUL, no colon — identity component hygiene (§8.2.2)."""
    return reject_colon(_require_nonempty(value, field), field)


def require_admission_batch_id_hex(batch_id: str) -> str:
    """admission_batch_id MUST be lowercase hex-64 ASCII before HMAC (§8.2.5)."""
    reject_nul(batch_id, "admission_batch_id")
    if not is_sha256_hex(batch_id):
        raise SplitError(
            "admission_batch_id must be lowercase hex-64 ASCII "
            "(never raw-32 digest bytes)")
    return batch_id


# ----------------------------------------------------------------- identity
def pack_group_id(source_family: str, audio_sha256: Iterable[str]) -> str:
    """`source_family + ":pack:" + SHA256(sorted pack digests + LF)`.

    `:pack:` is reserved for this fallback only (§8.2.2).
    """
    source_family = _require_family_or_upstream(source_family, "source_family")
    digests = list(audio_sha256)
    if not digests:
        raise SplitError("pack group requires a non-empty pack_audio_sha256 list")
    for digest in digests:
        if not is_sha256_hex(digest):
            raise SplitError(f"pack_audio_sha256 entry is not a sha256 hex: {digest!r}")
    if len(set(digests)) != len(digests):
        raise SplitError("pack_audio_sha256 contains duplicates")
    if digests != sorted(digests):
        raise SplitError("pack_audio_sha256 must be sorted by ascii bytes")
    payload = b"".join(digest.encode("ascii") + b"\n" for digest in digests)
    return f"{source_family}:pack:{hashlib.sha256(payload).hexdigest()}"


def upstream_group_id(source_family: str, upstream_id: str) -> str:
    source_family = _require_family_or_upstream(source_family, "source_family")
    upstream_id = _require_family_or_upstream(upstream_id, "upstream_id")
    # upstream_id must not imitate the reserved pack fallback form.
    if upstream_id.startswith("pack:"):
        raise SplitError(
            "upstream_id must not start with 'pack:'; ':pack:' is reserved "
            "for the pack fallback group_id form")
    return f"{source_family}:{upstream_id}"


def derive_group_id(row: dict) -> str:
    """Reconstruct the canonical group_id from an identity-index row."""
    source_family = _require_family_or_upstream(row.get("source_family"), "source_family")
    upstream_id = row.get("upstream_id")
    pack = row.get("pack_audio_sha256")
    if not isinstance(pack, list):
        raise SplitError("pack_audio_sha256 must be a list (possibly empty)")
    if upstream_id is None:
        if not pack:
            raise SplitError(
                "identity is not reconstructible: upstream_id is null and "
                "pack_audio_sha256 is empty")
        return pack_group_id(source_family, pack)
    if pack:
        raise SplitError(
            "ambiguous identity: upstream_id and pack_audio_sha256 both present")
    return upstream_group_id(
        source_family, _require_family_or_upstream(upstream_id, "upstream_id"))


def validate_identity_index(index: Any) -> list[dict]:
    """Fail-closed validation of aieq-v3-source-identity-index-1.

    Exact-key envelope and rows (additionalProperties forbidden). Rejects
    unknown/missing keys, NUL, colon in family/upstream, duplicate group ids,
    duplicate/cross-owned aliases, unsorted lists and any row whose declared
    group_id is not reconstructible from its source.
    """
    if not isinstance(index, dict):
        raise SplitError("identity index must be a JSON object")
    extra = set(index) - _IDENTITY_ENVELOPE_KEYS
    if extra:
        raise SplitError(f"unknown identity index fields: {sorted(extra)}")
    missing = _IDENTITY_ENVELOPE_KEYS - set(index)
    if missing:
        raise SplitError(f"missing identity index fields: {sorted(missing)}")
    if index["schema"] != SCHEMA_IDS["source_identity_index"]:
        raise SplitError(f"unexpected identity index schema: {index['schema']!r}")
    for field in ("source_snapshot_id", "alias_mapping_version",
                  "inclusion_rules_version"):
        _require_nonempty(index[field], field)

    groups = index["groups"]
    if not isinstance(groups, list) or not groups:
        raise SplitError("identity index groups must be a non-empty list")

    seen_groups: set[str] = set()
    alias_owner: dict[str, str] = {}
    rows: list[dict] = []
    for position, row in enumerate(groups):
        if not isinstance(row, dict):
            raise SplitError(f"identity row {position} is not an object")
        extra = set(row) - _IDENTITY_ROW_KEYS
        if extra:
            raise SplitError(f"unknown identity row fields at {position}: {sorted(extra)}")
        missing = _IDENTITY_ROW_KEYS - set(row)
        if missing:
            raise SplitError(f"missing identity row fields at {position}: {sorted(missing)}")

        group_id = _require_nonempty(row["group_id"], "group_id")
        _require_nonempty(row["group_primary_domain"], "group_primary_domain")
        _require_family_or_upstream(row["source_family"], "source_family")
        if row["upstream_id"] is not None:
            _require_family_or_upstream(row["upstream_id"], "upstream_id")
        if not is_canonical_profile(row["group_primary_profile"]):
            raise ProfileError(
                f"non-canonical group_primary_profile at {position}: "
                f"{row['group_primary_profile']!r}")

        derived = derive_group_id(row)
        if derived != group_id:
            raise SplitError(
                f"group_id is not reconstructible at {position}: declared "
                f"{group_id!r}, derived {derived!r}")
        if group_id in seen_groups:
            raise SplitError(f"duplicate group_id in identity index: {group_id!r}")
        seen_groups.add(group_id)

        aliases = row["aliases"]
        if not isinstance(aliases, list):
            raise SplitError(f"aliases must be a list at {position}")
        for alias in aliases:
            _require_nonempty(alias, "alias")
        if len(set(aliases)) != len(aliases):
            raise SplitError(f"duplicate alias inside group {group_id!r}")
        if list(aliases) != sorted(aliases, key=lambda item: item.encode("utf-8")):
            raise SplitError(f"aliases must be sorted by utf-8 bytes in {group_id!r}")
        for alias in aliases:
            owner = alias_owner.get(alias)
            if owner is not None and owner != group_id:
                raise SplitError(
                    f"alias {alias!r} claimed by both {owner!r} and {group_id!r}")
            alias_owner[alias] = group_id
        rows.append(row)

    ordered = sorted(rows, key=lambda item: item["group_id"].encode("utf-8"))
    if [item["group_id"] for item in rows] != [item["group_id"] for item in ordered]:
        raise SplitError("identity index groups must be sorted by utf-8 group_id bytes")
    return rows


def source_snapshot_sha256(identity_index: Any) -> str:
    """SHA-256 of canonical_bytes(identity_index); schema must match (§8.2.3).

    Free-form digests are not admitted: callers must derive this from a
    validated aieq-v3-source-identity-index-1 document.
    """
    validate_identity_index(identity_index)
    return sha256_of_obj(identity_index)


def exact_projection(rows: list[dict]) -> list[dict]:
    """Project validated identity rows onto roster group rows.

    Returns the exact `groups` list required on an admission roster:
    keys `_ROSTER_ROW_KEYS` only, sorted by utf-8 `group_id` bytes.
    Used as: `roster["groups"] == exact_projection(validate_identity_index(index))`.
    """
    if not isinstance(rows, list) or not rows:
        raise SplitError("exact_projection requires a non-empty validated row list")
    return [
        {key: row[key] for key in _ROSTER_ROW_KEYS}
        for row in sorted(rows, key=lambda item: item["group_id"].encode("utf-8"))
    ]


def roster_from_identity_index(index: Any) -> dict:
    """Project the validated identity index onto the pre-split roster.

    Sets source_snapshot_sha256 = SHA256(canonical_bytes(index)) — never a
    free-form digest (§8.2.3).
    """
    rows = validate_identity_index(index)
    return {
        "schema": SCHEMA_IDS["admission_roster"],
        "source_snapshot_id": index["source_snapshot_id"],
        "source_snapshot_sha256": sha256_of_obj(index),
        "groups": exact_projection(rows),
    }


def verify_roster_source_snapshot(roster: Any, identity_index: Any) -> None:
    """Bind roster to identity index: schema hash, snapshot id, full projection.

    Fail-closed checks (all mandatory):
      - identity index schema / reconstructibility via validate_identity_index;
      - source_snapshot_sha256 == SHA256(canonical_bytes(index));
      - source_snapshot_id equality;
      - roster.groups == exact_projection(validate_identity_index(index))
        (exact keys, values, canonical order).
    Free-form digests, removed/added groups, field drift, spurious `:pack:`
    rows and snapshot mismatch all raise SplitError.
    """
    if not isinstance(roster, dict):
        raise SplitError("roster must be a JSON object")
    rows = validate_identity_index(identity_index)
    expected = sha256_of_obj(identity_index)
    claimed = roster.get("source_snapshot_sha256")
    if not is_sha256_hex(claimed):
        raise SplitError("source_snapshot_sha256 is not a sha256 hex digest")
    if not hmac.compare_digest(claimed, expected):
        raise SplitError(
            "source_snapshot_sha256 does not equal "
            "SHA256(canonical_bytes(source_identity_index)); free-form rejected")
    if roster.get("source_snapshot_id") != identity_index.get("source_snapshot_id"):
        raise SplitError("roster source_snapshot_id does not match identity index")
    expected_groups = exact_projection(rows)
    actual_groups = roster.get("groups")
    if actual_groups != expected_groups:
        raise SplitError(
            "roster.groups is not the exact projection of the identity index "
            "(keys, values and canonical group_id order must match)")


def canonical_roster(roster: Any, identity_index: Any | None = None) -> dict:
    """Construction / canonicalize-on-create helper for the pre-split roster.

    Exact-key envelope schema aieq-v3-admission-roster-1. Sorts `groups` by
    utf-8 group_id bytes for newly built artifacts. When `identity_index` is
    supplied, also runs verify_roster_source_snapshot (hash + full projection).

    NOTE (M3): this helper may reorder groups on create. Validation of a
    *committed* artifact that must reject non-canonical on-disk order/bytes
    belongs in a separate fail-closed reader — do not treat sorted output here
    as proof that a stored file was already canonical.
    """
    if not isinstance(roster, dict):
        raise SplitError("roster must be a JSON object")
    allowed = {"schema", "source_snapshot_id", "source_snapshot_sha256", "groups"}
    extra = set(roster) - allowed
    if extra:
        raise SplitError(f"unknown roster fields: {sorted(extra)}")
    missing = allowed - set(roster)
    if missing:
        raise SplitError(f"missing roster fields: {sorted(missing)}")
    if roster["schema"] != SCHEMA_IDS["admission_roster"]:
        raise SplitError(f"unexpected roster schema: {roster['schema']!r}")
    _require_nonempty(roster["source_snapshot_id"], "source_snapshot_id")
    if not is_sha256_hex(roster["source_snapshot_sha256"]):
        raise SplitError("source_snapshot_sha256 is not a sha256 hex digest")

    groups = roster["groups"]
    if not isinstance(groups, list) or not groups:
        raise SplitError("roster groups must be a non-empty list")
    seen: set[str] = set()
    for position, row in enumerate(groups):
        if not isinstance(row, dict):
            raise SplitError(f"roster row {position} is not an object")
        if set(row) != set(_ROSTER_ROW_KEYS):
            raise SplitError(
                f"roster row {position} must have exactly {list(_ROSTER_ROW_KEYS)}, "
                f"got {sorted(row)}")
        if "split_role" in row or "development_pilot" in row:
            raise SplitError("the pre-split roster must not carry roles")
        group_id = _require_nonempty(row["group_id"], "group_id")
        _require_nonempty(row["group_primary_domain"], "group_primary_domain")
        _require_family_or_upstream(row["source_family"], "source_family")
        if not is_canonical_profile(row["group_primary_profile"]):
            raise ProfileError(
                f"non-canonical group_primary_profile at {position}: "
                f"{row['group_primary_profile']!r}")
        if group_id in seen:
            raise SplitError(f"duplicate group_id in roster: {group_id!r}")
        seen.add(group_id)

    ordered = sorted(groups, key=lambda item: item["group_id"].encode("utf-8"))
    normalized = dict(roster)
    normalized["groups"] = ordered
    if identity_index is not None:
        verify_roster_source_snapshot(normalized, identity_index)
    return normalized


def admission_batch_id(roster: Any) -> str:
    """SHA-256 of the canonical JSON bytes of the validated roster envelope."""
    return sha256_of_obj(canonical_roster(roster))


# ------------------------------------------------------------ commit-reveal
def salt_commitment(salt: bytes) -> str:
    if not isinstance(salt, (bytes, bytearray)):
        raise SplitError("salt must be raw bytes")
    if len(salt) != SPLIT_SALT_BYTES:
        raise SplitError(
            f"salt must be exactly {SPLIT_SALT_BYTES} bytes, got {len(salt)}")
    return hashlib.sha256(COMMITMENT_PREFIX + NUL + bytes(salt)).hexdigest()


def verify_commitment(salt: bytes, commitment_hex: str) -> None:
    if not is_sha256_hex(commitment_hex):
        raise SplitError("commitment is not a sha256 hex digest")
    actual = salt_commitment(salt)
    if not hmac.compare_digest(actual, commitment_hex):
        raise SplitError("commitment does not match the revealed salt")


# -------------------------------------------------------- role / pilot draw
def _role_message(batch_id: str, row: dict) -> bytes:
    batch_id = require_admission_batch_id_hex(batch_id)
    return (ROLE_PREFIX + NUL
            + batch_id.encode("ascii") + NUL
            + reject_nul(row["group_primary_profile"], "group_primary_profile").encode("utf-8") + NUL
            + reject_nul(row["group_primary_domain"], "group_primary_domain").encode("utf-8") + NUL
            + reject_colon(reject_nul(row["source_family"], "source_family"),
                           "source_family").encode("utf-8") + NUL
            + reject_nul(row["group_id"], "group_id").encode("utf-8"))


def assign_role(salt: bytes, batch_id: str, row: dict) -> str:
    """Deterministic role draw. Exact integer comparison per §8.2.5.

    Uses value * 20 < numerator * 2**64 — never a pre-floored threshold
    (numerator * 2**64 // 20), which truncates and shifts boundaries.
    """
    if len(salt) != SPLIT_SALT_BYTES:
        raise SplitError(f"salt must be exactly {SPLIT_SALT_BYTES} bytes")
    digest = hmac.new(bytes(salt), _role_message(batch_id, row), hashlib.sha256).digest()
    value = int.from_bytes(digest[:8], "big", signed=False)
    two64 = 2 ** 64
    for role, numerator in ROLE_INTERVALS_EXACT:
        if value * 20 < numerator * two64:
            return role
    # Unreachable: final-test has numerator 20, so value * 20 < 20 * 2**64
    # covers every uint64 value (max value is 2**64 - 1).
    raise SplitError("role assignment exhausted all intervals")  # pragma: no cover


def _require_salt(salt: object) -> bytes:
    if not isinstance(salt, (bytes, bytearray)):
        raise SplitError("salt must be raw bytes")
    if len(salt) != SPLIT_SALT_BYTES:
        raise SplitError(
            f"salt must be exactly {SPLIT_SALT_BYTES} bytes, got {len(salt)}")
    return bytes(salt)


def assign_pilot(salt: bytes, batch_id: str, group_id: str, role: str) -> bool:
    """Pilot flag; only development-metric groups may be true (§8.2.6).

    Fail-closed validation runs BEFORE the non-development early return:
    salt type/length, batch_id hex-64, group_id NUL hygiene, role membership.
    """
    salt = _require_salt(salt)
    batch_id = require_admission_batch_id_hex(batch_id)
    group_id = reject_nul(group_id, "group_id")
    if role not in SPLIT_ROLES:
        raise SplitError(f"unknown split role for pilot: {role!r}")
    if role != "development-metric":
        return False
    message = (PILOT_PREFIX + NUL
               + batch_id.encode("ascii") + NUL
               + group_id.encode("utf-8"))
    digest = hmac.new(salt, message, hashlib.sha256).digest()
    value = int.from_bytes(digest, "big", signed=False)
    # value / 2**256 < 1/4  <=>  value * 4 < 2**256
    return value * PILOT_THRESHOLD_DEN < PILOT_THRESHOLD_NUM * (2 ** 256)


def assign_batch(salt: bytes, roster: Any, commitment_hex: str, *,
                 identity_index: Any) -> dict:
    """Full reveal step: identity-bound roster, then roles and pilots.

    Normative admission path: `identity_index` is a required keyword argument.
    `None` is rejected. Before any role/pilot draw this verifies the commitment
    (§8.2.4), the identity index schema, source_snapshot_id,
    source_snapshot_sha256 and the full roster projection (§8.2.2 / §8.2.3).
    """
    if identity_index is None:
        raise SplitError(
            "identity_index is mandatory on the normative admission path")
    verify_commitment(salt, commitment_hex)
    validated = canonical_roster(roster, identity_index=identity_index)
    batch_id = admission_batch_id(validated)
    assignments = []
    for row in validated["groups"]:
        role = assign_role(salt, batch_id, row)
        assignments.append({
            "group_id": row["group_id"],
            "split_role": role,
            "development_pilot": assign_pilot(salt, batch_id, row["group_id"], role),
        })
    return {"admission_batch_id": batch_id, "assignments": assignments}


# --------------------------------------------------- global split invariants
def validate_manifest_split_invariants(assets: list[dict],
                                       assignments: dict[str, str] | None = None
                                       ) -> None:
    """Global fail-closed checks over a manifest asset list.

    §8.2.7 / §13 gate 8:
      - one group_id in exactly one role;
      - one audio SHA in exactly one role;
      - parent and every derivative share the parent's role;
      - parent_asset_id must exist unless the asset is an explicit root;
      - no parent cycles;
      - asset_id and relative_path unique;
      - roles must match the recorded assignments when supplied.
    """
    if not isinstance(assets, list) or not assets:
        raise SplitError("assets must be a non-empty list")

    group_role: dict[str, str] = {}
    sha_role: dict[str, str] = {}
    by_id: dict[str, dict] = {}
    seen_paths: set[str] = set()

    for asset in assets:
        asset_id = _require_nonempty(asset.get("asset_id"), "asset_id")
        if asset_id in by_id:
            raise SplitError(f"duplicate asset_id: {asset_id!r}")
        by_id[asset_id] = asset

        relative_path = _require_nonempty(asset.get("relative_path"), "relative_path")
        if relative_path in seen_paths:
            raise SplitError(f"duplicate relative_path: {relative_path!r}")
        seen_paths.add(relative_path)

        role = asset.get("split_role")
        if role not in SPLIT_ROLES:
            raise SplitError(f"unknown split_role {role!r} on {asset_id!r}")
        group_id = _require_nonempty(asset.get("group_id"), "group_id")
        digest = asset.get("sha256")
        if not is_sha256_hex(digest):
            raise SplitError(f"invalid sha256 on {asset_id!r}")

        previous = group_role.setdefault(group_id, role)
        if previous != role:
            raise SplitError(
                f"group {group_id!r} appears in two roles: {previous!r} and {role!r}")
        previous_sha_role = sha_role.setdefault(digest, role)
        if previous_sha_role != role:
            raise SplitError(
                f"audio sha {digest} appears in two roles: "
                f"{previous_sha_role!r} and {role!r}")

        if assignments is not None:
            expected = assignments.get(group_id)
            if expected is None:
                raise SplitError(f"group {group_id!r} has no recorded assignment")
            if expected != role:
                raise SplitError(
                    f"group {group_id!r} role {role!r} contradicts the recorded "
                    f"assignment {expected!r}")

        if asset.get("development_pilot") and role != "development-metric":
            raise SplitError(
                f"development_pilot is true on {asset_id!r} outside development-metric")

    for asset_id, asset in by_id.items():
        parent_id = asset.get("parent_asset_id")
        if parent_id is None:
            continue
        if parent_id not in by_id:
            raise SplitError(
                f"parent_asset_id {parent_id!r} of {asset_id!r} does not exist")
        parent = by_id[parent_id]
        if parent["split_role"] != asset["split_role"]:
            raise SplitError(
                f"derivative {asset_id!r} role {asset['split_role']!r} differs from "
                f"parent {parent_id!r} role {parent['split_role']!r}")
        if parent["group_id"] != asset["group_id"]:
            raise SplitError(
                f"derivative {asset_id!r} does not inherit the parent group_id")

        seen_chain = {asset_id}
        cursor = parent_id
        while cursor is not None:
            if cursor in seen_chain:
                raise SplitError(f"parent cycle detected at {cursor!r}")
            seen_chain.add(cursor)
            cursor = by_id[cursor].get("parent_asset_id")
            if cursor is not None and cursor not in by_id:
                raise SplitError(f"parent_asset_id {cursor!r} does not exist")
