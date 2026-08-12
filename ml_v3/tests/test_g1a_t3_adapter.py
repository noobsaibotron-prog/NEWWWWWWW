"""G1a T3 — frozen v2↔v3 adapter mapping (§10.5) happy + reject paths."""
from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from ml_v3.contracts.adapter import (
    ADAPTER_ARTIFACT_ID,
    HOMOLOGOUS_CLASSES,
    MASKED_NA_CLASSES,
    MIN_WINDOW_CENTERS,
    OCCUPANCY_THRESHOLD,
    WINDOW_STEP,
    AdapterError,
    adapter_mapping_bytes,
    adapter_mapping_sha256,
    frozen_adapter_mapping,
    is_masked_na_class,
    require_homologous_class,
    validate_adapter_mapping_claim,
)
from ml_v3.contracts.canonical import loads_strict, sha256_of_obj
from ml_v3.contracts.constants import CONTRACT_REVISION, PROBLEM_TYPES
from ml_v3.contracts.profiles import LEGACY_PROFILE_ALIASES, map_legacy_profile

FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "fixtures" / "g1" / "adapter_v2_v3_mapping.json"
)


class FrozenMappingHappyPathTests(unittest.TestCase):
    def test_six_homologous_classes_exact_order(self):
        self.assertEqual(
            list(HOMOLOGOUS_CLASSES),
            [
                "Resonance",
                "Muddiness",
                "Boominess",
                "Thinness",
                "BoxyMidrange",
                "DullSound",
            ],
        )
        self.assertEqual(len(HOMOLOGOUS_CLASSES), 6)

    def test_harsh_sib_masked_na(self):
        self.assertEqual(list(MASKED_NA_CLASSES), ["Harshness", "Sibilance"])
        for name in MASKED_NA_CLASSES:
            self.assertTrue(is_masked_na_class(name))
            self.assertNotIn(name, HOMOLOGOUS_CLASSES)

    def test_partition_covers_eight_problem_types(self):
        self.assertEqual(
            set(HOMOLOGOUS_CLASSES) | set(MASKED_NA_CLASSES),
            set(PROBLEM_TYPES),
        )
        self.assertFalse(set(HOMOLOGOUS_CLASSES) & set(MASKED_NA_CLASSES))

    def test_temporal_and_floor_constants(self):
        mapping = frozen_adapter_mapping()
        temporal = mapping["temporal_grid"]
        self.assertEqual(temporal["window_step"], WINDOW_STEP)
        self.assertEqual(WINDOW_STEP, 16)
        self.assertEqual(temporal["min_window_centers"], MIN_WINDOW_CENTERS)
        self.assertEqual(MIN_WINDOW_CENTERS, 20)
        self.assertEqual(temporal["occupancy_threshold"], OCCUPANCY_THRESHOLD)
        self.assertEqual(OCCUPANCY_THRESHOLD, 0.05)
        floors = mapping["per_class_support_floors"]
        self.assertEqual(floors["positive_groups"], 30)
        self.assertEqual(floors["negative_groups"], 30)
        self.assertEqual(
            floors["roles"], ["development-metric", "final-test"])
        self.assertEqual(
            floors["insufficient_support_consequence"],
            "insufficient support on any of the six homologous classes "
            "renders the gate NO-GO",
        )

    def test_na_semantics_includes_declared_invalid_converse(self):
        """§10.5: structural N/A + declared-but-invalid → fail-closed, never N/A."""
        prose = frozen_adapter_mapping()["na_semantics"]
        self.assertIn("structural absence is N/A", prose)
        self.assertIn(
            "a candidate that declares a surface but emits no valid "
            "prediction is fail-closed",
            prose,
        )
        self.assertIn("never N/A", prose)
        self.assertIn("FN, schema error or candidate failure per case", prose)

    def test_support_floor_consequence_is_nogo(self):
        floors = frozen_adapter_mapping()["per_class_support_floors"]
        self.assertIn("NO-GO", floors["insufficient_support_consequence"])
        self.assertIn("six homologous classes", floors["insufficient_support_consequence"])

    def test_legacy_techno_alias_reused_not_duplicated(self):
        mapping = frozen_adapter_mapping()
        self.assertEqual(
            mapping["legacy_profile_aliases"], LEGACY_PROFILE_ALIASES)
        self.assertEqual(
            map_legacy_profile("Techno")["source_profile"], "edm")

    def test_canonical_hash_stable_across_calls(self):
        a = adapter_mapping_sha256()
        b = adapter_mapping_sha256()
        self.assertEqual(a, b)
        self.assertEqual(len(a), 64)
        self.assertEqual(a, hashlib.sha256(adapter_mapping_bytes()).hexdigest())
        self.assertEqual(a, sha256_of_obj(frozen_adapter_mapping()))

    def test_validate_accepts_frozen_copy(self):
        claim = frozen_adapter_mapping()
        out = validate_adapter_mapping_claim(claim)
        self.assertEqual(out["artifact_id"], ADAPTER_ARTIFACT_ID)
        self.assertEqual(out["contract_revision"], CONTRACT_REVISION)

    def test_golden_fixture_matches_frozen_bytes(self):
        self.assertTrue(FIXTURE.is_file(), f"missing golden {FIXTURE}")
        on_disk = FIXTURE.read_bytes()
        self.assertEqual(on_disk, adapter_mapping_bytes())
        parsed = loads_strict(on_disk.decode("utf-8"))
        validate_adapter_mapping_claim(parsed)


class RejectPathTests(unittest.TestCase):
    def test_non_object_claim_rejected(self):
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(["not", "an", "object"])

    def test_wrong_window_step_rejected(self):
        claim = frozen_adapter_mapping()
        claim["temporal_grid"]["window_step"] = 32
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_wrong_occupancy_rejected(self):
        claim = frozen_adapter_mapping()
        claim["temporal_grid"]["occupancy_threshold"] = 0.10
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_wrong_min_centers_rejected(self):
        claim = frozen_adapter_mapping()
        claim["temporal_grid"]["min_window_centers"] = 10
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_promoting_harshness_into_homologous_rejected(self):
        claim = frozen_adapter_mapping()
        claim["homologous_classes"] = list(HOMOLOGOUS_CLASSES) + ["Harshness"]
        claim["macro_f1_denominator"] = 7
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_dropping_masked_na_rejected(self):
        claim = frozen_adapter_mapping()
        claim["masked_na_classes"] = ["Harshness"]  # dropped Sibilance
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_wrong_support_floors_rejected(self):
        claim = frozen_adapter_mapping()
        claim["per_class_support_floors"]["positive_groups"] = 20
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_stripping_support_nogo_consequence_rejected(self):
        claim = frozen_adapter_mapping()
        del claim["per_class_support_floors"]["insufficient_support_consequence"]
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_stripping_declared_invalid_na_converse_rejected(self):
        claim = frozen_adapter_mapping()
        # Truncate to the pre-T3.1 half (structural absence only).
        claim["na_semantics"] = (
            "N/A does not enter macro-averages, does not satisfy a gate, and "
            "does not demonstrate improvement; structural absence is N/A, "
            "never zero/infinity/FAIL"
        )
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_divergent_techno_alias_rejected(self):
        claim = frozen_adapter_mapping()
        claim["legacy_profile_aliases"] = {
            "Techno": {
                "source_profile": "master",
                "source_profile_id": 5,
                "electronic_subgenre": "techno",
            },
        }
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_extra_key_breaks_byte_identity(self):
        claim = frozen_adapter_mapping()
        claim["invented_threshold"] = 0.99
        with self.assertRaises(AdapterError):
            validate_adapter_mapping_claim(claim)

    def test_require_homologous_rejects_masked_and_unknown(self):
        with self.assertRaises(AdapterError):
            require_homologous_class("Harshness")
        with self.assertRaises(AdapterError):
            require_homologous_class("Sibilance")
        with self.assertRaises(AdapterError):
            require_homologous_class("NotAClass")
        self.assertEqual(require_homologous_class("Resonance"), "Resonance")

    def test_non_canonical_json_roundtrip_still_hashes_via_canonical(self):
        """Unsorted keys in text must not invent a second digest after loads."""
        mapping = frozen_adapter_mapping()
        messy = json.dumps(mapping, sort_keys=False, separators=(", ", ": "))
        reparsed = loads_strict(messy if messy.endswith("\n") else messy + "\n")
        # loads_strict accepts the object; validation requires frozen identity.
        validate_adapter_mapping_claim(reparsed)
        self.assertEqual(sha256_of_obj(reparsed), adapter_mapping_sha256())


if __name__ == "__main__":
    unittest.main()
