"""Canonical JSON and hashing for Motore v3 contract artifacts (G1a).

Contract references (docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a):
  - §8.2.1 / §13: UTF-8 without BOM; keys sorted by code point; separators
    exactly `,` and `:`; single trailing LF included in the hash;
    allow_nan=false; on read reject NaN/Infinity/overflow and duplicate keys.
  - §13 gate 2: two clean processes must produce byte-identical artifacts.

Standard library only. No third-party dependency is introduced by G1a.

Design notes:
  - NaN/+Inf/-Inf are rejected on BOTH write and read. json.dumps with
    allow_nan=False covers write; the read path rejects bare tokens via
    parse_constant AND walks the parsed tree with math.isfinite so overflow
    literals such as 1e400 (which become Inf) also fail closed.
  - Duplicate JSON object keys are rejected rather than silently last-wins.
  - Nothing here sorts semantic lists. Ordering of curves, scores and the
    120-band grid is meaningful and must be preserved; only the mapping KEYS
    are sorted, which is what canonical JSON requires.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any

__all__ = [
    "CanonicalError",
    "canonical_bytes",
    "canonical_text",
    "loads_strict",
    "load_strict",
    "write_canonical",
    "sha256_hex",
    "sha256_of_obj",
    "sha256_of_file",
    "is_sha256_hex",
    "validate_sha256sums_relpath",
    "sha256sums_text",
    "parse_sha256sums",
]

_SEPARATORS = (",", ":")


class CanonicalError(ValueError):
    """Raised when a document violates the canonical-JSON contract."""


# --------------------------------------------------------------- write side
def _reject_non_finite(obj: Any, path: str = "$") -> None:
    """Depth-first rejection of NaN/Inf anywhere in the document."""
    if isinstance(obj, float):
        if not math.isfinite(obj):
            raise CanonicalError(f"non-finite float at {path}: {obj!r}")
        return
    if isinstance(obj, dict):
        for key, value in obj.items():
            if not isinstance(key, str):
                raise CanonicalError(f"non-string object key at {path}: {key!r}")
            _reject_non_finite(value, f"{path}.{key}")
        return
    if isinstance(obj, (list, tuple)):
        for index, value in enumerate(obj):
            _reject_non_finite(value, f"{path}[{index}]")
        return
    if isinstance(obj, bool) or obj is None or isinstance(obj, (int, str)):
        return
    raise CanonicalError(f"unsupported type at {path}: {type(obj).__name__}")


def canonical_text(obj: Any) -> str:
    """Canonical JSON text: sorted keys, compact separators, final newline."""
    _reject_non_finite(obj)
    return json.dumps(obj, ensure_ascii=False, sort_keys=True,
                      separators=_SEPARATORS, allow_nan=False) + "\n"


def canonical_bytes(obj: Any) -> bytes:
    """Canonical JSON bytes (UTF-8). These are the bytes that get hashed."""
    return canonical_text(obj).encode("utf-8")


def write_canonical(path: Path, obj: Any) -> bytes:
    """Write canonical JSON to `path` and return the exact bytes written."""
    payload = canonical_bytes(obj)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return payload


# ---------------------------------------------------------------- read side
def _no_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    seen: dict[str, Any] = {}
    for key, value in pairs:
        if key in seen:
            raise CanonicalError(f"duplicate JSON key: {key!r}")
        seen[key] = value
    return seen


def _reject_constant(token: str) -> float:
    raise CanonicalError(f"non-finite JSON constant is forbidden: {token}")


def loads_strict(text: str) -> Any:
    """Parse JSON rejecting NaN/Infinity, overflow, and duplicate keys.

    Bare tokens (NaN/Infinity) are rejected via parse_constant. Numeric
    overflow such as 1e400 is accepted by json.loads as Inf; the post-parse
    isfinite walk rejects those as well (§8.2.1 / §13).
    """
    parsed = json.loads(
        text,
        object_pairs_hook=_no_duplicate_keys,
        parse_constant=_reject_constant,
    )
    _reject_non_finite(parsed)
    return parsed


def load_strict(path: Path) -> Any:
    return loads_strict(Path(path).read_text(encoding="utf-8"))


# -------------------------------------------------------------------- hash
def sha256_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_of_obj(obj: Any) -> str:
    """SHA-256 of the canonical JSON bytes of `obj`, final newline included."""
    return sha256_hex(canonical_bytes(obj))


def sha256_of_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def is_sha256_hex(value: object) -> bool:
    """True for a lowercase 64-char hexadecimal SHA-256 digest."""
    if not isinstance(value, str) or len(value) != 64:
        return False
    return all(character in "0123456789abcdef" for character in value)


def validate_sha256sums_relpath(path: object) -> str:
    """Validate a relative POSIX path for a SHA256SUMS entry (fail-closed).

    Rejects: empty; NUL; CR/LF; other control chars (U+0001–U+001F, U+007F);
    leading slash; backslash; ``..`` segments; leading/trailing ASCII spaces;
    empty path segments (``//`` or trailing ``/``).
    """
    if not isinstance(path, str):
        raise CanonicalError(
            f"SHA256SUMS path must be str, got {type(path).__name__}")
    if path == "":
        raise CanonicalError("SHA256SUMS path is empty")
    if "\0" in path:
        raise CanonicalError("SHA256SUMS path contains NUL")
    if "\n" in path or "\r" in path:
        raise CanonicalError("SHA256SUMS path contains newline/CR")
    for character in path:
        code = ord(character)
        if code < 32 or code == 127:
            raise CanonicalError(
                f"SHA256SUMS path contains control char U+{code:04X}")
    if path.startswith("/") or path.startswith("\\"):
        raise CanonicalError(
            f"SHA256SUMS path must be relative (no leading slash): {path!r}")
    if "\\" in path:
        raise CanonicalError(
            f"SHA256SUMS path must be POSIX (no backslash): {path!r}")
    if path != path.strip(" "):
        raise CanonicalError(
            f"SHA256SUMS path has leading/trailing spaces: {path!r}")
    segments = path.split("/")
    if any(segment == "" for segment in segments):
        raise CanonicalError(
            f"SHA256SUMS path has empty segment: {path!r}")
    if any(segment == ".." for segment in segments):
        raise CanonicalError(
            f"SHA256SUMS path contains '..' segment: {path!r}")
    return path


def sha256sums_text(entries: dict[str, str]) -> str:
    """Render a SHA256SUMS body sorted lexicographically by POSIX path.

    Format per line: ``<64 lowercase hex><two spaces><relpath>\\n``.
    Paths are validated via :func:`validate_sha256sums_relpath`. Digests must
    be lowercase 64-char hex. Duplicate path keys are impossible in a dict;
    callers must not rely on last-wins merge of colliding validated paths.
    """
    if not isinstance(entries, dict):
        raise CanonicalError(
            f"SHA256SUMS entries must be dict, got {type(entries).__name__}")
    if not entries:
        raise CanonicalError("SHA256SUMS entries must be non-empty")
    lines = []
    for relative_path in sorted(entries):
        validated = validate_sha256sums_relpath(relative_path)
        digest = entries[relative_path]
        if not is_sha256_hex(digest):
            raise CanonicalError(
                f"invalid digest for {validated}: {digest!r}")
        lines.append(f"{digest}  {validated}\n")
    return "".join(lines)


def parse_sha256sums(text: object) -> dict[str, str]:
    """Parse a SHA256SUMS body into ``{relpath: digest}`` (fail-closed).

    Requires GNU text-mode lines (``digest`` + two spaces + path + ``\\n``).
    Rejects empty body, blank lines, ``#`` comments, binary-mode ``*``
    markers, duplicate paths, and any path that fails
    :func:`validate_sha256sums_relpath`. Round-trip guarantee:
    ``parse_sha256sums(sha256sums_text(x)) == x`` for valid ``x``.
    """
    if not isinstance(text, str):
        raise CanonicalError(
            f"SHA256SUMS text must be str, got {type(text).__name__}")
    if text == "":
        raise CanonicalError("SHA256SUMS text is empty")
    if not text.endswith("\n"):
        raise CanonicalError("SHA256SUMS text must end with a trailing newline")
    if "\0" in text:
        raise CanonicalError("SHA256SUMS text contains NUL")
    entries: dict[str, str] = {}
    lines = text.split("\n")
    # Final split element after trailing newline is the empty string.
    if lines[-1] != "":
        raise CanonicalError("SHA256SUMS text must end with a trailing newline")
    body_lines = lines[:-1]
    if not body_lines:
        raise CanonicalError("SHA256SUMS text has no entries")
    for index, line in enumerate(body_lines, start=1):
        if line == "":
            raise CanonicalError(f"SHA256SUMS blank line at {index}")
        if "\r" in line:
            raise CanonicalError(f"SHA256SUMS CR at line {index}")
        if line.startswith("#"):
            raise CanonicalError(
                f"SHA256SUMS comments are forbidden at line {index}")
        # Require exactly two ASCII spaces between digest and path.
        separator = "  "
        if separator not in line:
            raise CanonicalError(
                f"SHA256SUMS line {index} missing two-space separator")
        digest, relative_path = line.split(separator, 1)
        if "  " in relative_path or relative_path.startswith(" "):
            raise CanonicalError(
                f"SHA256SUMS line {index} has ambiguous spacing in path")
        if relative_path.startswith("*"):
            raise CanonicalError(
                f"SHA256SUMS binary-mode marker forbidden at line {index}")
        if not is_sha256_hex(digest):
            raise CanonicalError(
                f"SHA256SUMS line {index} has invalid digest: {digest!r}")
        validated = validate_sha256sums_relpath(relative_path)
        if validated in entries:
            raise CanonicalError(
                f"SHA256SUMS duplicate path at line {index}: {validated!r}")
        entries[validated] = digest
    return entries
