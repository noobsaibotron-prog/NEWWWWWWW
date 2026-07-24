"""G1a identity / canonical / coverage fail-closed tests."""
from __future__ import annotations

import hashlib
import unittest

from ml_v3.contracts.canonical import CanonicalError, loads_strict, sha256_of_obj
from ml_v3.contracts.constants import CONDITIONING_PROFILES, SCHEMA_IDS
from ml_v3.contracts.coverage import CoverageError, check_calibration_coverage
from ml_v3.contracts.split import (
    SplitError,
    canonical_roster,
    pack_group_id,
    roster_from_identity_index,
    source_snapshot_sha256,
    upstream_group_id,
    validate_identity_index,
    verify_roster_source_snapshot,
)


def _empty_profile_partition() -> dict[str, list[str]]:
    return {profile: [] for profile in CONDITIONING_PROFILES}


def _sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _identity_index() -> dict:
    fam = "fsld"
    up = "track001"
    gid = f"{fam}:{up}"
    digests = sorted([_sha("a"), _sha("b")])
    pack_gid = pack_group_id("packfam", digests)
    return {
        "schema": SCHEMA_IDS["source_identity_index"],
        "source_snapshot_id": "snap-1",
        "alias_mapping_version": "alias-v1",
        "inclusion_rules_version": "incl-v1",
        "groups": [
            {
                "group_id": gid,
                "group_primary_profile": "edm",
                "group_primary_domain": "electronic",
                "source_family": fam,
                "upstream_id": up,
                "pack_audio_sha256": [],
                "aliases": ["alias-a"],
            },
            {
                "group_id": pack_gid,
                "group_primary_profile": "generic",
                "group_primary_domain": "music",
                "source_family": "packfam",
                "upstream_id": None,
                "pack_audio_sha256": digests,
                "aliases": [],
            },
        ],
    }


class ColonForbiddenTests(unittest.TestCase):
    def test_source_family_rejects_colon(self):
        with self.assertRaises(SplitError):
            upstream_group_id("bad:fam", "id1")

    def test_upstream_id_rejects_colon(self):
        with self.assertRaises(SplitError):
            upstream_group_id("fam", "bad:id")

    def test_upstream_must_not_imitate_pack_fallback(self):
        with self.assertRaises(SplitError):
            upstream_group_id("fam", "pack:deadbeef")

    def test_identity_index_rejects_colon_in_family(self):
        index = _identity_index()
        index["groups"][0]["source_family"] = "bad:fam"
        index["groups"][0]["group_id"] = "bad:fam:track001"
        with self.assertRaises(SplitError):
            validate_identity_index(index)


class SourceSnapshotSha256Tests(unittest.TestCase):
    def test_roster_binds_to_canonical_identity_hash(self):
        index = _identity_index()
        # groups must be sorted by utf-8 group_id
        index["groups"] = sorted(
            index["groups"], key=lambda row: row["group_id"].encode("utf-8"))
        expected = sha256_of_obj(index)
        self.assertEqual(source_snapshot_sha256(index), expected)
        roster = roster_from_identity_index(index)
        self.assertEqual(roster["source_snapshot_sha256"], expected)
        self.assertEqual(roster["schema"], SCHEMA_IDS["admission_roster"])
        verify_roster_source_snapshot(roster, index)
        canonical_roster(roster, identity_index=index)

    def test_free_form_snapshot_sha_rejected(self):
        index = _identity_index()
        index["groups"] = sorted(
            index["groups"], key=lambda row: row["group_id"].encode("utf-8"))
        roster = roster_from_identity_index(index)
        tampered = dict(roster)
        tampered["source_snapshot_sha256"] = "ff" * 32
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(tampered, index)
        with self.assertRaises(SplitError):
            canonical_roster(tampered, identity_index=index)

    def test_wrong_identity_schema_rejected(self):
        index = _identity_index()
        index["schema"] = "not-the-contract-schema"
        with self.assertRaises(SplitError):
            source_snapshot_sha256(index)


class CanonicalReadPathTests(unittest.TestCase):
    def test_rejects_nan_infinity_tokens(self):
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": NaN}')
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": Infinity}')
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": -Infinity}')

    def test_rejects_overflow_1e400_as_non_finite(self):
        with self.assertRaises(CanonicalError):
            loads_strict('{"x": 1e400}')
        with self.assertRaises(CanonicalError):
            loads_strict('{"nested": {"y": [1e309]}}')

    def test_rejects_duplicate_keys(self):
        with self.assertRaises(CanonicalError):
            loads_strict('{"a": 1, "a": 2}')

    def test_accepts_finite(self):
        self.assertEqual(loads_strict('{"x": 1.5, "y": 0}\n'), {"x": 1.5, "y": 0})


class CoverageDisjointTests(unittest.TestCase):
    def _empty_anomaly(self) -> dict:
        return {
            klass: {
                "positive_groups": [],
                "negative_groups": [],
                "negative_clean_groups": [],
                "positive_groups_by_family": {},
                "negative_groups_by_profile": {},
            }
            for klass in ("Resonance", "Harshness", "Sibilance")
        }

    def test_positive_negative_overlap_raises(self):
        tonal = {
            "actionable_groups": [],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": [],
            "per_profile_actionable": _empty_profile_partition(),
            "per_profile_clean": _empty_profile_partition(),
        }
        anomaly = self._empty_anomaly()
        anomaly["Resonance"]["positive_groups"] = ["g1", "g2"]
        anomaly["Resonance"]["negative_groups"] = ["g2", "g3"]
        anomaly["Resonance"]["positive_groups_by_family"] = {"f": ["g1", "g2"]}
        anomaly["Resonance"]["negative_groups_by_profile"] = {
            "generic": ["g2", "g3"]}
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, anomaly)

    def test_family_strata_must_match_declared_positives(self):
        tonal = {
            "actionable_groups": [],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": [],
            "per_profile_actionable": _empty_profile_partition(),
            "per_profile_clean": _empty_profile_partition(),
        }
        anomaly = self._empty_anomaly()
        anomaly["Resonance"]["positive_groups"] = ["g1", "g2"]
        anomaly["Resonance"]["positive_groups_by_family"] = {"f": ["g1"]}  # missing g2
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, anomaly)

    def test_tonal_actionable_clean_disjoint(self):
        tonal = {
            "actionable_groups": ["a1"],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": ["a1"],
            "per_profile_actionable": {
                **_empty_profile_partition(),
                "generic": ["a1"],
            },
            "per_profile_clean": {
                **_empty_profile_partition(),
                "generic": ["a1"],
            },
        }
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, self._empty_anomaly())


if __name__ == "__main__":
    unittest.main()
