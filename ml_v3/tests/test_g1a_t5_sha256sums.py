"""G1a T5 — SHA256SUMS inventory, path validation, round-trip, reject paths."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ml_v3.contracts.canonical import (
    CanonicalError,
    is_sha256_hex,
    parse_sha256sums,
    sha256_of_file,
    sha256sums_text,
    validate_sha256sums_relpath,
)
from ml_v3.contracts.sha256sums import (
    CONTRACT_DOC_SHA256_TRIPWIRE,
    G1A_SHA256SUMS_COVERED,
    G1A_SHA256SUMS_RELPATH,
    Sha256SumsError,
    build_sha256sums_entries,
    g1a_sha256sums_audio_required,
    load_g1a_sha256sums,
    render_g1a_sha256sums,
    repo_root_from_here,
    verify_g1a_sha256sums,
    verify_sha256sums_against_tree,
)

_HEX0 = "0" * 64
_HEX1 = "1" * 64


class PathValidationRejectTests(unittest.TestCase):
    """One reject-path assertion per forbidden category."""

    def test_reject_nul(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\0b")

    def test_reject_newline(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\nb")

    def test_reject_cr(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\rb")

    def test_reject_control_char_tab(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("a\tb")

    def test_reject_leading_slash(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("/etc/passwd")

    def test_reject_dotdot_segment(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("foo/../bar")

    def test_reject_backslash(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("foo\\bar")

    def test_reject_leading_space(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath(" spaced")

    def test_reject_trailing_space(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("spaced ")

    def test_reject_empty(self):
        with self.assertRaises(CanonicalError):
            validate_sha256sums_relpath("")

    def test_accept_relative_posix(self):
        self.assertEqual(
            validate_sha256sums_relpath("docs/MOTORE_V3_G1_CONTRACT.md"),
            "docs/MOTORE_V3_G1_CONTRACT.md",
        )


class Sha256sumsRoundTripTests(unittest.TestCase):
    def test_parse_render_round_trip(self):
        entries = {
            "b/file.json": _HEX1,
            "a/file.json": _HEX0,
        }
        text = sha256sums_text(entries)
        self.assertEqual(parse_sha256sums(text), entries)
        # Lexicographic path order in the rendered body.
        self.assertTrue(text.startswith(f"{_HEX0}  a/file.json\n"))

    def test_render_rejects_bad_path_and_digest(self):
        with self.assertRaises(CanonicalError):
            sha256sums_text({"/abs": _HEX0})
        with self.assertRaises(CanonicalError):
            sha256sums_text({"ok": "deadbeef"})
        with self.assertRaises(CanonicalError):
            sha256sums_text({})

    def test_parse_rejects_blank_comment_binary_duplicate(self):
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0}  a\n\n{_HEX1}  b\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"# comment\n{_HEX0}  a\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0} *a\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0}  a\n{_HEX1}  a\n")
        with self.assertRaises(CanonicalError):
            parse_sha256sums(f"{_HEX0} a\n")  # single space


class G1aSha256sumsFixtureTests(unittest.TestCase):
    def test_self_hash_policy_documented_and_enforced(self):
        self.assertEqual(
            G1A_SHA256SUMS_RELPATH, "ml_v3/fixtures/g1/SHA256SUMS")
        self.assertNotIn(G1A_SHA256SUMS_RELPATH, G1A_SHA256SUMS_COVERED)
        root = repo_root_from_here()
        with self.assertRaises(Sha256SumsError):
            build_sha256sums_entries(
                root, (G1A_SHA256SUMS_RELPATH,), forbid_self=G1A_SHA256SUMS_RELPATH)

    def test_minimum_coverage_and_contract_tripwire(self):
        self.assertEqual(len(G1A_SHA256SUMS_COVERED), 11)
        self.assertIn(
            "ml_v3/fixtures/g1/fixture_spec_v1.json",
            G1A_SHA256SUMS_COVERED,
        )
        example_paths = [
            path for path in G1A_SHA256SUMS_COVERED if "/examples/" in path]
        self.assertEqual(len(example_paths), 6)
        self.assertIn(
            "ml_v3/fixtures/g1/schema_registry_v1.json",
            G1A_SHA256SUMS_COVERED,
        )
        # Normative schema surface is schema_registry_v1 — not examples/.
        schema_mislabel = [
            path for path in G1A_SHA256SUMS_COVERED if "/schemas/" in path]
        self.assertEqual(schema_mislabel, [])
        self.assertIn("docs/MOTORE_V3_G1_CONTRACT.md", G1A_SHA256SUMS_COVERED)
        self.assertIn(
            "ml_v3/fixtures/g1/adapter_v2_v3_mapping.json",
            G1A_SHA256SUMS_COVERED,
        )
        self.assertIn(
            "ml_v3/fixtures/g1/metrology_lock.json",
            G1A_SHA256SUMS_COVERED,
        )
        self.assertTrue(is_sha256_hex(CONTRACT_DOC_SHA256_TRIPWIRE))
        # Reseal @ REV7 + G1c determinism pins (§10.0 / §11.2) + scope
        # closure. Independent literal: updating the module constant alone must
        # not make this pass.
        self.assertEqual(
            CONTRACT_DOC_SHA256_TRIPWIRE,
            "310d538647d71840c6dd8124f1e24281b758bb6dd3e4776aac4b34e5f658a5b0",
        )

    def test_committed_sha256sums_verifies_against_tree(self):
        entries = verify_g1a_sha256sums()
        # COVERED + T6 audio + G1c required; happy path is 11 + 37 + 1 = 49.
        self.assertTrue(set(G1A_SHA256SUMS_COVERED).issubset(entries))
        audio = g1a_sha256sums_audio_required()
        self.assertEqual(len(audio), 37)
        self.assertTrue(set(audio).issubset(entries))
        self.assertEqual(len(entries), 49)
        root = repo_root_from_here()
        contract = root / "docs" / "MOTORE_V3_G1_CONTRACT.md"
        self.assertEqual(sha256_of_file(contract), CONTRACT_DOC_SHA256_TRIPWIRE)
        self.assertEqual(
            entries["docs/MOTORE_V3_G1_CONTRACT.md"],
            CONTRACT_DOC_SHA256_TRIPWIRE,
        )
        # Committed file exists beside goldens and matches renderer.
        committed = root / G1A_SHA256SUMS_RELPATH
        self.assertTrue(committed.is_file())
        self.assertEqual(committed.read_text(encoding="utf-8"), render_g1a_sha256sums())
        self.assertEqual(load_g1a_sha256sums(), entries)
        self.assertEqual(committed.read_text(encoding="utf-8"), sha256sums_text(entries))

    def test_verify_rejects_truncated_sums_missing_audio(self):
        """COVERED-only inventory must not PASS verify (audio unbound)."""
        root = repo_root_from_here()
        full = load_g1a_sha256sums()
        truncated = {path: full[path] for path in G1A_SHA256SUMS_COVERED}
        self.assertEqual(len(truncated), 11)
        with tempfile.TemporaryDirectory() as tmp:
            troot = Path(tmp)
            for rel, digest in truncated.items():
                absolute = troot / rel
                absolute.parent.mkdir(parents=True, exist_ok=True)
                src = root / rel
                absolute.write_bytes(src.read_bytes())
                self.assertEqual(sha256_of_file(absolute), digest)
            sums_path = troot / G1A_SHA256SUMS_RELPATH
            sums_path.parent.mkdir(parents=True, exist_ok=True)
            sums_path.write_text(sha256sums_text(truncated), encoding="utf-8")
            with self.assertRaises(Sha256SumsError) as ctx:
                verify_g1a_sha256sums(troot)
            self.assertIn("missing required audio inventory", str(ctx.exception))

    def test_verify_detects_tampered_digest(self):
        root = repo_root_from_here()
        entries = dict(load_g1a_sha256sums())
        victim = next(iter(entries))
        entries[victim] = _HEX0 if entries[victim] != _HEX0 else _HEX1
        with self.assertRaises(Sha256SumsError):
            verify_sha256sums_against_tree(entries, root)

    def test_build_entries_hashes_temp_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            rel = "pkg/a.bin"
            absolute = root / rel
            absolute.parent.mkdir(parents=True)
            absolute.write_bytes(b"g1a-t5")
            entries = build_sha256sums_entries(
                root, (rel,), forbid_self="pkg/SHA256SUMS")
            self.assertEqual(entries[rel], sha256_of_file(absolute))
            verify_sha256sums_against_tree(
                entries, root, forbid_self="pkg/SHA256SUMS")


if __name__ == "__main__":
    unittest.main()
