"""G1a T1 gate reject-path tests (H1/H2/M1/M2 + HMAC ceil boundaries)."""
from __future__ import annotations

import hashlib
import hmac
import unittest

from ml_v3.contracts.constants import (
    CONDITIONING_PROFILES,
    ROLE_INTERVALS_EXACT,
    SCHEMA_IDS,
)
from ml_v3.contracts.coverage import CoverageError, check_calibration_coverage
from ml_v3.contracts.split import (
    SplitError,
    assign_batch,
    assign_pilot,
    assign_role,
    exact_projection,
    pack_group_id,
    roster_from_identity_index,
    salt_commitment,
    validate_identity_index,
    verify_roster_source_snapshot,
)


def _sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _sorted_identity_index() -> dict:
    fam = "fsld"
    up = "track001"
    gid = f"{fam}:{up}"
    digests = sorted([_sha("a"), _sha("b")])
    pack_gid = pack_group_id("packfam", digests)
    index = {
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
    index["groups"] = sorted(
        index["groups"], key=lambda row: row["group_id"].encode("utf-8"))
    return index


def _empty_profile_partition() -> dict[str, list[str]]:
    return {profile: [] for profile in CONDITIONING_PROFILES}


def _empty_anomaly() -> dict:
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


SALT = b"\x22" * 32
BATCH_HEX = "cd" * 32


class H1IdentityMandatoryTests(unittest.TestCase):
    def test_assign_batch_without_identity_keyword_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        commitment = salt_commitment(SALT)
        with self.assertRaises(TypeError):
            assign_batch(SALT, roster, commitment)  # type: ignore[call-arg]

    def test_assign_batch_identity_none_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        commitment = salt_commitment(SALT)
        with self.assertRaises(SplitError):
            assign_batch(SALT, roster, commitment, identity_index=None)

    def test_free_form_snapshot_hex_rejected_on_normative_path(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        tampered = dict(roster)
        tampered["source_snapshot_sha256"] = "ab" * 32
        commitment = salt_commitment(SALT)
        with self.assertRaises(SplitError):
            assign_batch(SALT, tampered, commitment, identity_index=index)


class H2FullProjectionTests(unittest.TestCase):
    def test_full_projection_pass(self):
        index = _sorted_identity_index()
        rows = validate_identity_index(index)
        roster = roster_from_identity_index(index)
        self.assertEqual(roster["groups"], exact_projection(rows))
        verify_roster_source_snapshot(roster, index)
        batch = assign_batch(
            SALT, roster, salt_commitment(SALT), identity_index=index)
        self.assertEqual(len(batch["assignments"]), len(roster["groups"]))

    def test_minus_group_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        roster["groups"] = list(roster["groups"][:-1])
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_plus_group_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        extra = {
            "group_id": "extrafam:extra1",
            "group_primary_profile": "bass",
            "group_primary_domain": "music",
            "source_family": "extrafam",
        }
        roster["groups"] = list(roster["groups"]) + [extra]
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_projected_field_changed_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        groups = [dict(row) for row in roster["groups"]]
        groups[0]["group_primary_domain"] = "tampered-domain"
        roster["groups"] = groups
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_spurious_pack_group_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        digests = sorted([_sha("x"), _sha("y")])
        spurious_gid = pack_group_id("otherpack", digests)
        roster["groups"] = list(roster["groups"]) + [{
            "group_id": spurious_gid,
            "group_primary_profile": "drums",
            "group_primary_domain": "music",
            "source_family": "otherpack",
        }]
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)

    def test_different_snapshot_id_fails(self):
        index = _sorted_identity_index()
        roster = roster_from_identity_index(index)
        roster = dict(roster)
        roster["source_snapshot_id"] = "snap-OTHER"
        with self.assertRaises(SplitError):
            verify_roster_source_snapshot(roster, index)


class M1TonalProfilePartitionTests(unittest.TestCase):
    def _tonal_base(self) -> dict:
        return {
            "actionable_groups": ["a1", "a2"],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": ["c1"],
            "per_profile_actionable": {
                **_empty_profile_partition(),
                "generic": ["a1"],
                "edm": ["a2"],
            },
            "per_profile_clean": {
                **_empty_profile_partition(),
                "vocals": ["c1"],
            },
        }

    def test_same_group_id_two_profiles_fails(self):
        tonal = self._tonal_base()
        tonal["per_profile_actionable"]["generic"] = ["a1"]
        tonal["per_profile_actionable"]["edm"] = ["a1", "a2"]
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, _empty_anomaly())

    def test_missing_profile_fails(self):
        tonal = self._tonal_base()
        del tonal["per_profile_actionable"]["bass"]
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, _empty_anomaly())

    def test_extra_non_canonical_profile_fails(self):
        tonal = self._tonal_base()
        tonal["per_profile_actionable"]["techno"] = []
        with self.assertRaises(CoverageError):
            check_calibration_coverage(tonal, _empty_anomaly())

    def test_valid_exact_partition_reaches_floors(self):
        # Empty declared sets with exact empty profile keys is structurally valid.
        tonal = {
            "actionable_groups": [],
            "actionable_positive_cell_groups": [],
            "actionable_negative_cell_groups": [],
            "clean_groups": [],
            "per_profile_actionable": _empty_profile_partition(),
            "per_profile_clean": _empty_profile_partition(),
        }
        report = check_calibration_coverage(tonal, _empty_anomaly())
        self.assertFalse(report.ok)  # floors N/A, but no CoverageError


class M2AssignPilotFailClosedTests(unittest.TestCase):
    def test_salt_lengths_fail_before_role_shortcut(self):
        for length in (31, 33, 1):
            with self.subTest(length=length):
                with self.assertRaises(SplitError):
                    assign_pilot(b"\x00" * length, BATCH_HEX, "fam:g1", "train")

    def test_invalid_batch_hex_fails_even_when_role_not_development(self):
        with self.assertRaises(SplitError):
            assign_pilot(SALT, "zz" * 32, "fam:g1", "train")
        with self.assertRaises(SplitError):
            assign_pilot(SALT, "AB" * 32, "fam:g1", "final-test")

    def test_nul_group_id_fails_before_shortcut(self):
        with self.assertRaises(SplitError):
            assign_pilot(SALT, BATCH_HEX, "fam:\x00g1", "train")

    def test_unknown_role_fails(self):
        with self.assertRaises(SplitError):
            assign_pilot(SALT, BATCH_HEX, "fam:g1", "not-a-role")

    def test_non_development_returns_false_after_validation(self):
        self.assertFalse(assign_pilot(SALT, BATCH_HEX, "fam:g1", "train"))


class HmacCeilBoundaryTests(unittest.TestCase):
    def test_ceil_boundaries_for_role_numerators(self):
        """For n in (11,13,15,17), T = ceil(n * 2**64 / 20); probe T-1 and T."""
        two64 = 2 ** 64
        row = {
            "group_id": "fam:up1",
            "group_primary_profile": "generic",
            "group_primary_domain": "music",
            "source_family": "fam",
        }
        original_new = hmac.new

        roles = [name for name, _ in ROLE_INTERVALS_EXACT]
        for role, numerator in ROLE_INTERVALS_EXACT[:-1]:
            # T = ceil(n * 2**64 / 20) via integer arithmetic (no float).
            threshold = (numerator * two64 + 19) // 20
            next_role_name = roles[roles.index(role) + 1]

            for value, expected in (
                (threshold - 1, role),
                (threshold, next_role_name),
            ):
                def _fake_new(key, msg=None, digestmod=None, *, _v=value):  # noqa: ANN001
                    class _H:
                        def digest(self_inner):
                            return _v.to_bytes(8, "big") + b"\x00" * 24
                    return _H()

                hmac.new = _fake_new  # type: ignore[assignment]
                try:
                    got = assign_role(SALT, BATCH_HEX, row)
                finally:
                    hmac.new = original_new  # type: ignore[assignment]
                self.assertEqual(
                    got, expected,
                    f"n={numerator} value={value} expected={expected} got={got}")


if __name__ == "__main__":
    unittest.main()
