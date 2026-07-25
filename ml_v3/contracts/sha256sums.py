"""G1a T5 SHA256SUMS inventory and verify helpers (stdlib only).

Authority: MOTORE_V3_G1_CONTRACT @ 6d254d0a; PLAN debt
``contract_doc_sha256`` → T5 SHA256SUMS.

Self-hash policy
----------------
``ml_v3/fixtures/g1/SHA256SUMS`` does **not** list or hash itself. The
integrity anchor for the sums file is the immutable git commit that contains
it. Adding the sums path to the covered inventory is rejected fail-closed.
"""
from __future__ import annotations

from pathlib import Path

from .canonical import (
    CanonicalError,
    parse_sha256sums,
    sha256_of_file,
    sha256sums_text,
    validate_sha256sums_relpath,
)

__all__ = [
    "G1A_SHA256SUMS_RELPATH",
    "G1A_SHA256SUMS_COVERED",
    "CONTRACT_DOC_SHA256_TRIPWIRE",
    "Sha256SumsError",
    "repo_root_from_here",
    "build_sha256sums_entries",
    "render_g1a_sha256sums",
    "write_g1a_sha256sums",
    "load_g1a_sha256sums",
    "verify_sha256sums_against_tree",
    "verify_g1a_sha256sums",
]

# Repo-root-relative path of the committed sums file (not self-hashed).
G1A_SHA256SUMS_RELPATH = "ml_v3/fixtures/g1/SHA256SUMS"

# Precomputed contract tripwire (PLAN durable debt; freeze @ 6d254d0a).
CONTRACT_DOC_SHA256_TRIPWIRE = (
    "6a6f6d35bbf3fc65d7a01e54620bf4f9649ea77d60c2b3d9e7ea0b72f7f49a86"
)

# Minimum G1a T5+M2 coverage: contract doc + 6 schema goldens + adapter +
# lock + fixture-spec v1 (M2 extends the trust chain; path-canonical order).
G1A_SHA256SUMS_COVERED: tuple[str, ...] = (
    "docs/MOTORE_V3_G1_CONTRACT.md",
    "ml_v3/fixtures/g1/adapter_v2_v3_mapping.json",
    "ml_v3/fixtures/g1/fixture_spec_v1.json",
    "ml_v3/fixtures/g1/metrology_lock.json",
    "ml_v3/fixtures/g1/schemas/admission_batch.json",
    "ml_v3/fixtures/g1/schemas/annotation.json",
    "ml_v3/fixtures/g1/schemas/asset_manifest.json",
    "ml_v3/fixtures/g1/schemas/benchmark_power_plan.json",
    "ml_v3/fixtures/g1/schemas/calibration_policy.json",
    "ml_v3/fixtures/g1/schemas/prediction.json",
)


class Sha256SumsError(ValueError):
    """Raised when SHA256SUMS inventory or tree verification fails."""


def repo_root_from_here() -> Path:
    """Return the repository root assuming this file lives under ml_v3/."""
    # ml_v3/contracts/sha256sums.py → parents[2] == repo root
    return Path(__file__).resolve().parents[2]


def build_sha256sums_entries(
    root: Path,
    relative_paths: tuple[str, ...] | list[str],
    *,
    forbid_self: str | None = G1A_SHA256SUMS_RELPATH,
) -> dict[str, str]:
    """Hash each relative path under ``root`` into a SHA256SUMS mapping."""
    root = Path(root)
    entries: dict[str, str] = {}
    for raw in relative_paths:
        rel = validate_sha256sums_relpath(raw)
        if forbid_self is not None and rel == forbid_self:
            raise Sha256SumsError(
                "SHA256SUMS must not hash itself; anchor is the immutable "
                f"commit containing {forbid_self!r}")
        absolute = root / rel
        if not absolute.is_file():
            raise Sha256SumsError(f"missing file for SHA256SUMS entry: {rel}")
        entries[rel] = sha256_of_file(absolute)
    if len(entries) != len(list(relative_paths)):
        raise Sha256SumsError("SHA256SUMS covered paths contain duplicates")
    return entries


def render_g1a_sha256sums(root: Path | None = None) -> str:
    """Render the G1a T5 SHA256SUMS body from the current tree."""
    root = repo_root_from_here() if root is None else Path(root)
    entries = build_sha256sums_entries(root, G1A_SHA256SUMS_COVERED)
    contract_digest = entries["docs/MOTORE_V3_G1_CONTRACT.md"]
    if contract_digest != CONTRACT_DOC_SHA256_TRIPWIRE:
        raise Sha256SumsError(
            "contract_doc_sha256 tripwire mismatch: "
            f"got {contract_digest}, expected {CONTRACT_DOC_SHA256_TRIPWIRE}")
    return sha256sums_text(entries)


def write_g1a_sha256sums(root: Path | None = None) -> Path:
    """Write ``ml_v3/fixtures/g1/SHA256SUMS`` from the current tree."""
    root = repo_root_from_here() if root is None else Path(root)
    text = render_g1a_sha256sums(root)
    out = root / G1A_SHA256SUMS_RELPATH
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8", newline="\n")
    return out


def load_g1a_sha256sums(root: Path | None = None) -> dict[str, str]:
    """Load and parse the committed G1a SHA256SUMS file."""
    root = repo_root_from_here() if root is None else Path(root)
    path = root / G1A_SHA256SUMS_RELPATH
    if not path.is_file():
        raise Sha256SumsError(f"missing SHA256SUMS at {G1A_SHA256SUMS_RELPATH}")
    return parse_sha256sums(path.read_text(encoding="utf-8"))


def verify_sha256sums_against_tree(
    entries: dict[str, str],
    root: Path,
    *,
    forbid_self: str | None = G1A_SHA256SUMS_RELPATH,
) -> None:
    """Fail-closed: every entry must match ``sha256_of_file`` under root."""
    root = Path(root)
    if not entries:
        raise Sha256SumsError("SHA256SUMS entries empty")
    for rel, claimed in entries.items():
        validated = validate_sha256sums_relpath(rel)
        if forbid_self is not None and validated == forbid_self:
            raise Sha256SumsError(
                "SHA256SUMS must not hash itself; anchor is the immutable "
                f"commit containing {forbid_self!r}")
        absolute = root / validated
        if not absolute.is_file():
            raise Sha256SumsError(f"missing file for SHA256SUMS entry: {validated}")
        actual = sha256_of_file(absolute)
        if actual != claimed:
            raise Sha256SumsError(
                f"digest mismatch for {validated}: "
                f"sums={claimed} tree={actual}")


def verify_g1a_sha256sums(root: Path | None = None) -> dict[str, str]:
    """Verify committed G1a SHA256SUMS against the tree + minimum coverage."""
    root = repo_root_from_here() if root is None else Path(root)
    entries = load_g1a_sha256sums(root)
    missing = [path for path in G1A_SHA256SUMS_COVERED if path not in entries]
    if missing:
        raise Sha256SumsError(
            f"SHA256SUMS missing required coverage: {missing}")
    if G1A_SHA256SUMS_RELPATH in entries:
        raise Sha256SumsError(
            "SHA256SUMS must not hash itself; anchor is the immutable commit "
            f"containing {G1A_SHA256SUMS_RELPATH!r}")
    contract = entries.get("docs/MOTORE_V3_G1_CONTRACT.md")
    if contract != CONTRACT_DOC_SHA256_TRIPWIRE:
        raise Sha256SumsError(
            "contract_doc_sha256 tripwire mismatch: "
            f"got {contract}, expected {CONTRACT_DOC_SHA256_TRIPWIRE}")
    verify_sha256sums_against_tree(entries, root)
    # Round-trip the committed bytes through the canonical renderer.
    try:
        rerendered = sha256sums_text(entries)
    except CanonicalError as exc:
        raise Sha256SumsError(str(exc)) from exc
    committed = (root / G1A_SHA256SUMS_RELPATH).read_text(encoding="utf-8")
    if rerendered != committed:
        raise Sha256SumsError(
            "committed SHA256SUMS is not canonical sha256sums_text(entries)")
    if parse_sha256sums(rerendered) != entries:
        raise Sha256SumsError("SHA256SUMS parse/render round-trip failed")
    return entries
