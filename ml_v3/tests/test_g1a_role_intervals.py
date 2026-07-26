"""G1a boundary tests: ROLE_INTERVALS exact integer thresholds (§8.2.5)."""
from __future__ import annotations

import hashlib
import hmac
import unittest

from ml_v3.contracts.constants import (CONTRACT_REVISION, ROLE_INTERVALS_EXACT,
                                       ROLE_PREFIX, SCHEMA_IDS, SPLIT_ROLES)
from ml_v3.contracts.split import SplitError, assign_role, require_admission_batch_id_hex


NUL = b"\x00"
SALT = b"\x11" * 32
BATCH_HEX = "ab" * 32  # valid lowercase hex-64


def _role_for_value(value: int) -> str:
    """Reference oracle: value * 20 < n * 2**64."""
    two64 = 2 ** 64
    for role, numerator in ROLE_INTERVALS_EXACT:
        if value * 20 < numerator * two64:
            return role
    raise AssertionError("unreachable")


def _floor_bug_role(value: int) -> str:
    """Legacy buggy comparison that used // 20 truncation."""
    for role, numerator in ROLE_INTERVALS_EXACT:
        upper = numerator * (2 ** 64) // 20
        if value < upper:
            return role
    return SPLIT_ROLES[-1]


class RoleIntervalExactTests(unittest.TestCase):
    def test_contract_revision_is_rev7_consolidated(self):
        self.assertIn("REVISIONE 7 CONSOLIDATA", CONTRACT_REVISION)
        self.assertIn("6fbf5b59", CONTRACT_REVISION)
        self.assertIn("gate-4 scope C", CONTRACT_REVISION)
        self.assertNotIn("REVISIONE 5", CONTRACT_REVISION)

    def test_schema_ids_include_identity_and_roster(self):
        self.assertEqual(SCHEMA_IDS["source_identity_index"],
                         "aieq-v3-source-identity-index-1")
        self.assertEqual(SCHEMA_IDS["admission_roster"],
                         "aieq-v3-admission-roster-1")

    def test_intervals_store_numerators_not_floored_thresholds(self):
        # Must be (role, numerator) — never pre-divided by 20.
        self.assertEqual(
            ROLE_INTERVALS_EXACT,
            (
                ("train", 11),
                ("validation", 13),
                ("calibration", 15),
                ("development-metric", 17),
                ("final-test", 20),
            ),
        )

    def test_floor_truncation_diverges_at_train_boundary(self):
        # For n=11, floored = 11*2**64 // 20 leaves a gap of 16.
        floored = 11 * (2 ** 64) // 20
        # value == floored: buggy path excludes train; exact path includes train.
        self.assertEqual(_floor_bug_role(floored), "validation")
        self.assertEqual(_role_for_value(floored), "train")
        # Last value still in train under exact rule:
        last_train = (11 * (2 ** 64) - 1) // 20
        self.assertEqual(_role_for_value(last_train), "train")
        self.assertEqual(_role_for_value(last_train + 1), "validation")

    def test_assign_role_matches_exact_formula_via_forged_hmac(self):
        """Forge salt/message is hard; instead unit-test the comparison formula
        through a monkeypatch of hmac digest first 8 bytes."""
        row = {
            "group_id": "fam:up1",
            "group_primary_profile": "generic",
            "group_primary_domain": "music",
            "source_family": "fam",
        }
        # Probe values that sit in the truncation gap for train (n=11).
        floored = 11 * (2 ** 64) // 20
        probes = [
            0,
            floored - 1,
            floored,
            floored + 15,  # still train under exact; validation under floor bug
            (11 * (2 ** 64)) // 20 + 16,  # first validation under exact? check
            (13 * (2 ** 64) - 1) // 20,
            (13 * (2 ** 64)) // 20,
            (2 ** 64) - 1,
        ]
        original_new = hmac.new

        for value in probes:
            expected = _role_for_value(value)

            def _fake_new(key, msg=None, digestmod=None):  # noqa: ANN001
                class _H:
                    def digest(self_inner):
                        return value.to_bytes(8, "big") + b"\x00" * 24
                return _H()

            hmac.new = _fake_new  # type: ignore[assignment]
            try:
                got = assign_role(SALT, BATCH_HEX, row)
            finally:
                hmac.new = original_new  # type: ignore[assignment]
            self.assertEqual(got, expected, f"value={value}")

        # Explicitly assert the known train-boundary gap between floor bug and exact.
        self.assertNotEqual(_floor_bug_role(floored), _role_for_value(floored))

    def test_admission_batch_id_must_be_hex64(self):
        require_admission_batch_id_hex(BATCH_HEX)
        with self.assertRaises(SplitError):
            require_admission_batch_id_hex("not-hex")
        with self.assertRaises(SplitError):
            require_admission_batch_id_hex("AB" * 32)  # uppercase rejected
        with self.assertRaises(SplitError):
            require_admission_batch_id_hex("\x00" + "ab" * 31)


class HmacHexAsciiTests(unittest.TestCase):
    def test_role_and_pilot_messages_embed_hex64_ascii_not_raw32(self):
        from ml_v3.contracts.split import assign_pilot

        row = {
            "group_id": "fam:up1",
            "group_primary_profile": "generic",
            "group_primary_domain": "music",
            "source_family": "fam",
        }
        captured: list[bytes] = []
        original_new = hmac.new

        def _spy(key, msg=None, digestmod=None):  # noqa: ANN001
            captured.append(msg)
            return original_new(key, msg, digestmod)

        hmac.new = _spy  # type: ignore[assignment]
        try:
            assign_role(SALT, BATCH_HEX, row)
            assign_pilot(SALT, BATCH_HEX, row["group_id"], "development-metric")
        finally:
            hmac.new = original_new  # type: ignore[assignment]

        self.assertEqual(len(captured), 2)
        role_msg, pilot_msg = captured
        # Role: prefix + NUL + hex64 + NUL + ...
        self.assertTrue(role_msg.startswith(ROLE_PREFIX + NUL + BATCH_HEX.encode("ascii") + NUL))
        self.assertNotIn(bytes.fromhex(BATCH_HEX), role_msg)  # raw-32 must NOT appear
        self.assertIn(BATCH_HEX.encode("ascii"), role_msg)
        self.assertIn(BATCH_HEX.encode("ascii"), pilot_msg)
        # Reject non-hex batch id before MAC
        with self.assertRaises(SplitError):
            assign_role(SALT, "zz" * 32, row)


if __name__ == "__main__":
    unittest.main()
