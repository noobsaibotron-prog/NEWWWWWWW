"""G1c T0 — gate-9 fixture registry: happy path, reject paths, golden bytes."""
from __future__ import annotations

import copy
import re
import unittest
from pathlib import Path

from ml_v3.contracts.canonical import canonical_bytes, loads_strict, sha256_of_obj
from ml_v3.contracts.gate9_registry import (
    GATE9_FIXTURE_COUNT,
    GATE9_OUTCOME_CLASSES,
    GATE9_REGISTRY_ARTIFACT_ID,
    Gate9RegistryError,
    frozen_gate9_registry,
    gate9_fixture_ids,
    gate9_outcome,
    gate9_registry_bytes,
    gate9_registry_sha256,
    validate_gate9_registry_claim,
)
from ml_v3.contracts.metrology_lock import gate_platform_python_label

REPO = Path(__file__).resolve().parents[2]
GOLDEN = REPO / "ml_v3" / "fixtures" / "g1" / "gate9_registry_v1.json"
CONTRACT = REPO / "docs" / "MOTORE_V3_G1_CONTRACT.md"


class Gate9RegistryShapeTests(unittest.TestCase):
    def test_gate_platform(self):
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")

    def test_exactly_fourteen_unique_ids(self):
        ids = gate9_fixture_ids()
        self.assertEqual(len(ids), GATE9_FIXTURE_COUNT)
        self.assertEqual(len(ids), 14)
        self.assertEqual(len(set(ids)), 14)

    def test_outcome_classes_and_partition(self):
        outcomes = [gate9_outcome(i) for i in gate9_fixture_ids()]
        for outcome in outcomes:
            self.assertIn(outcome, GATE9_OUTCOME_CLASSES)
        # One PASS, two PASS_INVARIANT (order/dedup invariance), eleven FAIL.
        self.assertEqual(outcomes.count("PASS"), 1)
        self.assertEqual(outcomes.count("PASS_INVARIANT"), 2)
        self.assertEqual(outcomes.count("FAIL"), 11)

    def test_invariance_fixtures_are_the_expected_two(self):
        invariant = [
            i for i in gate9_fixture_ids() if gate9_outcome(i) == "PASS_INVARIANT"]
        self.assertEqual(
            invariant,
            ["evaluator_row_permutation", "evaluator_duplicate_evaluation_unit"],
        )

    def test_unknown_id_is_fail_closed(self):
        with self.assertRaises(Gate9RegistryError):
            gate9_outcome("evaluator_does_not_exist")

    def test_digest_stable_across_calls(self):
        self.assertEqual(gate9_registry_sha256(), gate9_registry_sha256())
        self.assertEqual(len(gate9_registry_sha256()), 64)


class Gate9RegistryContractBindingTests(unittest.TestCase):
    """The registry must mirror §13.2 gate 9, not a private list."""

    def test_every_id_appears_in_the_contract_table(self):
        text = CONTRACT.read_text(encoding="utf-8")
        for fixture_id in gate9_fixture_ids():
            self.assertIn(
                f"`{fixture_id}`", text,
                msg=f"{fixture_id} is not in the contract gate-9 table")

    def test_contract_table_declares_no_other_evaluator_fixture(self):
        text = CONTRACT.read_text(encoding="utf-8")
        found = set(re.findall(r"`(evaluator_[a-z0-9_]+)`", text))
        self.assertEqual(
            found, set(gate9_fixture_ids()),
            msg="contract and registry disagree on the gate-9 fixture set")


class Gate9RegistryGoldenTests(unittest.TestCase):
    def test_golden_matches_frozen_bytes(self):
        self.assertTrue(GOLDEN.is_file(), f"missing golden {GOLDEN}")
        raw = GOLDEN.read_bytes()
        self.assertEqual(raw, gate9_registry_bytes())
        doc = loads_strict(raw.decode("utf-8"))
        self.assertEqual(raw, canonical_bytes(doc))
        self.assertEqual(doc, frozen_gate9_registry())
        self.assertEqual(sha256_of_obj(doc), gate9_registry_sha256())
        validate_gate9_registry_claim(doc)


class Gate9RegistryRejectTests(unittest.TestCase):
    def test_non_object_rejected(self):
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(["not", "an", "object"])

    def test_dropped_fixture_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixtures"] = claim["fixtures"][:-1]
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_added_fixture_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixtures"].append(
            {"id": "evaluator_sneaky", "outcome": "PASS", "expected": "x"})
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_reordered_fixtures_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixtures"][0], claim["fixtures"][1] = (
            claim["fixtures"][1], claim["fixtures"][0])
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_renamed_id_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixtures"][0]["id"] = "evaluator_perfect_prediction_v2"
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_flipping_fail_to_pass_rejected(self):
        """The attack this registry exists to stop."""
        claim = copy.deepcopy(frozen_gate9_registry())
        for row in claim["fixtures"]:
            if row["id"] == "evaluator_empty_prediction":
                row["outcome"] = "PASS"
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_unknown_outcome_class_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixtures"][0]["outcome"] = "REPORT_ONLY"
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_mutated_expected_text_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixtures"][1]["expected"] = "qualcosa di piu permissivo"
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_wrong_fixture_count_field_rejected(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["fixture_count"] = 13
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_extra_top_level_key_breaks_byte_identity(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["invented_relaxation"] = True
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)

    def test_artifact_id_pinned(self):
        claim = copy.deepcopy(frozen_gate9_registry())
        claim["artifact_id"] = GATE9_REGISTRY_ARTIFACT_ID + "-x"
        with self.assertRaises(Gate9RegistryError):
            validate_gate9_registry_claim(claim)


if __name__ == "__main__":
    unittest.main()
