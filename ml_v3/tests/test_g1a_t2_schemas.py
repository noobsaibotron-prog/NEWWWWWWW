"""G1a T2: six JSON schemas + fail-closed validator (happy + reject paths)."""
from __future__ import annotations

import copy
import unittest
from pathlib import Path

from ml_v3.contracts.canonical import (
    canonical_bytes,
    loads_strict,
    sha256_of_obj,
    write_canonical,
)
from ml_v3.contracts.constants import GRID_BANDS, SCHEMA_IDS, SPLIT_ROLES
from ml_v3.contracts.schemas import (
    SCHEMA_REGISTRY,
    SCHEMA_REGISTRY_RELPATH,
    frozen_schema_registry,
    schema_for,
    schema_ids_t2,
    schema_registry_sha256,
)
from ml_v3.contracts.split import validate_manifest_split_invariants
from ml_v3.contracts.validate import SchemaError, validate, validate_schema_id
from ml_v3.tests._g1a_t2_fixtures import (
    admission_batch,
    admission_batch_mismatched_commitment,
    annotation_clean,
    asset_manifest,
    benchmark_power_plan,
    calibration_policy,
    prediction,
)

# Instance example goldens (not the normative schema surface).
FIXTURE_DIR = Path(__file__).resolve().parents[1] / "fixtures" / "g1" / "examples"
SCHEMA_REGISTRY_FIXTURE = (
    Path(__file__).resolve().parents[1] / "fixtures" / "g1" / "schema_registry_v1.json"
)


class SchemaRegistryTests(unittest.TestCase):
    def test_six_schema_ids_registered(self):
        ids = schema_ids_t2()
        self.assertEqual(len(ids), 6)
        self.assertEqual(set(ids), set(SCHEMA_REGISTRY))
        for schema_id in ids:
            doc = schema_for(schema_id)
            self.assertEqual(doc["$id"], schema_id)
            self.assertIs(doc["additionalProperties"], False)

    def test_schema_ids_match_constants(self):
        expected = {
            SCHEMA_IDS["asset_manifest"],
            SCHEMA_IDS["admission_batch"],
            SCHEMA_IDS["annotation"],
            SCHEMA_IDS["prediction"],
            SCHEMA_IDS["calibration_policy"],
            SCHEMA_IDS["benchmark_power_plan"],
        }
        self.assertEqual(set(schema_ids_t2()), expected)


class HappyPathTests(unittest.TestCase):
    def test_all_six_validate(self):
        docs = [
            asset_manifest(),
            admission_batch(),
            annotation_clean(),
            prediction(),
            calibration_policy(),
            benchmark_power_plan(),
        ]
        for doc in docs:
            with self.subTest(schema=doc["schema"]):
                self.assertEqual(validate(doc), doc["schema"])
                validate_schema_id(doc, doc["schema"])

    def test_manifest_compatible_with_split_invariants(self):
        root = asset_manifest(asset_id="root-1", relative_path="a.wav")
        child = asset_manifest(
            asset_id="child-1",
            relative_path="b.wav",
            sha256="11" * 32,
            parent_asset_id="root-1",
            derivative_kind="crop",
        )
        validate(root)
        validate(child)
        validate_manifest_split_invariants([root, child])

    def test_development_pilot_ok_on_development_metric(self):
        doc = asset_manifest(
            split_role="development-metric", development_pilot=True)
        validate(doc)


class RejectPathManifestTests(unittest.TestCase):
    def test_extra_key_rejected(self):
        doc = asset_manifest()
        doc["extra"] = True
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_missing_key_rejected(self):
        doc = asset_manifest()
        del doc["ledger_id"]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_wrong_schema_id_rejected(self):
        doc = asset_manifest(schema="aieq-v3-asset-manifest-999")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_unknown_split_role_rejected(self):
        doc = asset_manifest(split_role="holdout")
        with self.assertRaises(SchemaError):
            validate(doc)
        self.assertNotIn("holdout", SPLIT_ROLES)

    def test_duplicate_benchmark_family_rejected(self):
        doc = asset_manifest(
            benchmark_families=["tonal-controlled", "tonal-controlled"])
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_pilot_outside_development_rejected(self):
        doc = asset_manifest(split_role="train", development_pilot=True)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_invalid_sha_rejected(self):
        doc = asset_manifest(sha256="ZZ" * 32)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_owned_without_ledger_rejected(self):
        doc = asset_manifest(license_class="OWNED", ledger_id="")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_sample_rate_rejected(self):
        doc = asset_manifest(sample_rate=32000)
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathAdmissionTests(unittest.TestCase):
    def test_roster_sha_must_equal_batch_id(self):
        doc = admission_batch(roster_sha256="ff" * 32)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_bad_status_rejected(self):
        doc = admission_batch(status="pending")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_admitted_requires_salt_reveal(self):
        doc = admission_batch(status="admitted", salt_reveal=None)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_rejected_allows_null_salt_reveal(self):
        doc = admission_batch(status="rejected", salt_reveal=None)
        validate(doc)

    def test_mismatched_salt_commitment_rejected(self):
        """F2: historical dd/ee pair must FAIL commit-reveal (§8.2.4)."""
        doc = admission_batch_mismatched_commitment()
        with self.assertRaises(SchemaError) as ctx:
            validate(doc)
        self.assertIn("salt_commitment", str(ctx.exception))

    def test_rejected_with_mismatched_reveal_rejected(self):
        doc = admission_batch_mismatched_commitment(status="rejected")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_alias_mapping_version_extra_key_rejected(self):
        # Identity-index field must not sneak into admission-batch envelope.
        doc = admission_batch(alias_mapping_version="alias-v1")
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathAnnotationTests(unittest.TestCase):
    def test_curve_length_rejected(self):
        doc = annotation_clean()
        doc["tonal_correction_db"] = [0.0] * (GRID_BANDS - 1)
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_curve_out_of_range_rejected(self):
        doc = annotation_clean(clean_for_action=False)
        curve = [0.0] * GRID_BANDS
        curve[0] = 9.5
        doc["tonal_correction_db"] = curve
        doc["complete_types"] = ["Resonance"]
        doc["explicit_negative_types"] = []
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_clean_for_action_nonzero_curve_rejected(self):
        doc = annotation_clean()
        curve = [0.0] * GRID_BANDS
        curve[3] = 0.1
        doc["tonal_correction_db"] = curve
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_problem_type_id_mismatch_rejected(self):
        doc = annotation_clean(clean_for_action=False)
        doc["complete_types"] = ["Resonance"]
        doc["explicit_negative_types"] = []
        doc["semantic_regions"] = [{
            "problem_type": "Resonance",
            "problem_type_id": 3,  # Sibilance id; mismatch
            "start_s": 0.0,
            "end_s": 1.0,
            "band_lo_hz": 1000.0,
            "band_hi_hz": 2000.0,
            "direction": None,
            "severity": 0.5,
            "confidence": 0.5,
            "actionable": True,
        }]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_thinness_requires_direction(self):
        doc = annotation_clean(clean_for_action=False)
        doc["complete_types"] = ["Thinness"]
        doc["explicit_negative_types"] = []
        doc["semantic_regions"] = [{
            "problem_type": "Thinness",
            "problem_type_id": 5,
            "start_s": 0.0,
            "end_s": 1.0,
            "band_lo_hz": None,
            "band_hi_hz": None,
            "direction": None,
            "severity": 0.5,
            "confidence": 0.5,
            "actionable": True,
        }]
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathPredictionTests(unittest.TestCase):
    def test_missing_anomaly_ref_key_rejected(self):
        doc = prediction()
        del doc["anomaly_score_ref"]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_frame_mismatch_rejected(self):
        doc = prediction()
        doc["anomaly_severity_ref"] = dict(doc["anomaly_severity_ref"])
        doc["anomaly_severity_ref"]["num_feature_frames"] = 99
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_events_without_valid_dtype_rejected(self):
        doc = prediction()
        doc["anomaly_score_ref"] = dict(doc["anomaly_score_ref"])
        doc["anomaly_score_ref"]["dtype"] = "float64"
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathCalibrationTests(unittest.TestCase):
    def test_threshold_length_rejected(self):
        doc = calibration_policy()
        doc["semantic_type_thresholds"] = [0.5] * 7
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_non_monotone_score_map_rejected(self):
        doc = calibration_policy()
        doc["score_to_confidence"] = [
            {"score": 0.0, "confidence": 0.8},
            {"score": 1.0, "confidence": 0.2},
        ]
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_missing_endpoint_knots_rejected(self):
        doc = calibration_policy()
        doc["score_to_confidence"] = [
            {"score": 0.2, "confidence": 0.2},
            {"score": 0.8, "confidence": 0.8},
        ]
        with self.assertRaises(SchemaError):
            validate(doc)


class RejectPathPowerPlanTests(unittest.TestCase):
    def test_unit_file_rejected(self):
        doc = benchmark_power_plan()
        doc = copy.deepcopy(doc)
        doc["families"][0]["unit"] = "file"
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_final_test_source_rejected(self):
        doc = benchmark_power_plan(power_source_role="final-test")
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_final_test_in_support_rejected(self):
        doc = benchmark_power_plan()
        doc = copy.deepcopy(doc)
        doc["support"]["final-test"] = 10
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_n_required_must_be_max(self):
        doc = copy.deepcopy(benchmark_power_plan())
        doc["families"][0]["n_required"] = 1
        with self.assertRaises(SchemaError):
            validate(doc)

    def test_alpha_plan_must_match_m(self):
        doc = benchmark_power_plan(alpha_plan=0.05)
        with self.assertRaises(SchemaError):
            validate(doc)


class FailClosedParseTests(unittest.TestCase):
    def test_nan_token_rejected_before_validate(self):
        text = '{"schema":"x","v":NaN}'
        with self.assertRaises(Exception):
            loads_strict(text)

    def test_unknown_schema_dispatch_rejected(self):
        with self.assertRaises(SchemaError):
            validate({"schema": "aieq-v3-feature-frame-1"})


class GoldenFixtureTests(unittest.TestCase):
    def test_example_raw_equals_canonical_bytes(self):
        """A6: instance-valid is not enough — example on-disk bytes must be
        canonical artifacts (raw == canonical_bytes(doc))."""
        if not FIXTURE_DIR.is_dir():
            self.skipTest("no example fixture dir")
        files = sorted(FIXTURE_DIR.glob("*.json"))
        self.assertGreaterEqual(len(files), 6)
        for path in files:
            with self.subTest(path=path.name):
                raw = path.read_bytes()
                doc = loads_strict(raw.decode("utf-8"))
                self.assertEqual(raw, canonical_bytes(doc))
                validate(doc)


class SchemaRegistrySurfaceTests(unittest.TestCase):
    def test_frozen_registry_matches_fixture(self):
        self.assertEqual(
            SCHEMA_REGISTRY_RELPATH,
            "ml_v3/fixtures/g1/schema_registry_v1.json",
        )
        self.assertTrue(SCHEMA_REGISTRY_FIXTURE.is_file())
        raw = SCHEMA_REGISTRY_FIXTURE.read_bytes()
        doc = loads_strict(raw.decode("utf-8"))
        self.assertEqual(raw, canonical_bytes(doc))
        self.assertEqual(doc, frozen_schema_registry())
        self.assertEqual(sha256_of_obj(doc), schema_registry_sha256())

    def test_mutating_asset_manifest_keys_changes_digest(self):
        """F3: expanding schema surface MUST change a tracked digest."""
        base = frozen_schema_registry()
        digest_before = sha256_of_obj(base)
        mutated = copy.deepcopy(base)
        keys = mutated["key_sets"]["ASSET_MANIFEST_KEYS"]
        self.assertIsInstance(keys, list)
        keys.append("evil_extra_field")
        # Also mutate required[] on the embedded schema dict (surface expand).
        required = mutated["schemas"][SCHEMA_IDS["asset_manifest"]]["required"]
        required.append("evil_extra_field")
        digest_after = sha256_of_obj(mutated)
        self.assertNotEqual(digest_before, digest_after)


def _write_goldens() -> None:
    """Helper for regenerating example + schema-registry fixtures."""
    FIXTURE_DIR.mkdir(parents=True, exist_ok=True)
    mapping = {
        "asset_manifest.json": asset_manifest(),
        "admission_batch.json": admission_batch(),
        "annotation.json": annotation_clean(),
        "prediction.json": prediction(),
        "calibration_policy.json": calibration_policy(),
        "benchmark_power_plan.json": benchmark_power_plan(),
    }
    for name, doc in mapping.items():
        write_canonical(FIXTURE_DIR / name, doc)
    write_canonical(SCHEMA_REGISTRY_FIXTURE, frozen_schema_registry())


if __name__ == "__main__":
    unittest.main()
