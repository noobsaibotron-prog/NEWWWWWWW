"""G1c T1 — evaluator parser: order invariance + one reject per attack vector.

Attack vectors were enumerated per invariant BEFORE writing the tests, so that
"the first attack that fails correctly" does not stand in for coverage:

  admission   A1 unknown schema · A2 missing schema · A3 right schema wrong slot
              A4 fails its T2 validator
  hash        A5 policy hash mismatch · A6 policy of the wrong schema
  run         A7 mixed model_sha256 · A8 mixed frontend hash
              A9 mixed policy hash across predictions
  binding     A10 orphan prediction · A11 orphan annotation · A12 profile drift
  duplicates  A13 duplicate manifest asset_id · A14 duplicate prediction key
              A15 duplicate annotation key
  emptiness   A16 empty manifest · A17 empty predictions
  invariance  A18 manifest permuted · A19 annotations permuted
              A20 predictions permuted · A21 all three permuted
"""
from __future__ import annotations

import copy
import unittest

from ml_v3.benchmark.evaluator_parser import (
    EvaluatorParseError,
    parse_evaluator_inputs,
    parsed_inputs_bytes,
)
from ml_v3.contracts.canonical import sha256_of_obj
from ml_v3.contracts.metrology_lock import gate_platform_python_label
from ml_v3.tests._g1a_t2_fixtures import (
    annotation_clean,
    asset_manifest,
    calibration_policy,
    prediction,
)


def _world(n_assets: int = 3):
    """A small admissible world: n assets, one annotation and one prediction each."""
    policy = calibration_policy()
    digest = sha256_of_obj(policy)
    manifest, annotations, predictions = [], [], []
    for i in range(n_assets):
        asset_id = f"asset-{i:03d}"
        manifest.append(asset_manifest(
            asset_id=asset_id,
            relative_path=f"audio/{asset_id}.wav",
            sha256=f"{i:02d}" * 32,
            group_id=f"fam:grp{i:03d}",
        ))
        annotations.append(annotation_clean(
            asset_id=asset_id, evaluation_unit_id=f"unit-{i:03d}"))
        predictions.append(prediction(
            asset_id=asset_id, calibration_policy_sha256=digest))
    return manifest, annotations, predictions, policy


def _parse(manifest, annotations, predictions, policy):
    return parse_evaluator_inputs(
        manifest_records=manifest,
        annotation_records=annotations,
        prediction_records=predictions,
        calibration_policy=policy,
    )


class HappyPathTests(unittest.TestCase):
    def test_gate_platform(self):
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")

    def test_admits_and_binds_group_id(self):
        parsed = _parse(*_world())
        self.assertEqual(parsed["counts"],
                         {"manifest": 3, "annotations": 3, "predictions": 3})
        # group_id is resolved from the manifest onto records that lack it.
        self.assertEqual(
            [p["group_id"] for p in parsed["predictions"]],
            ["fam:grp000", "fam:grp001", "fam:grp002"])
        self.assertEqual(parsed["run"]["model_sha256"],
                         parsed["predictions"][0]["record"]["model_sha256"])


class OrderInvarianceTests(unittest.TestCase):
    """A18–A21: file order carries no information."""

    def setUp(self):
        self.world = _world(4)
        self.base = parsed_inputs_bytes(_parse(*self.world))

    def _same(self, manifest, annotations, predictions, policy):
        self.assertEqual(
            parsed_inputs_bytes(_parse(manifest, annotations, predictions, policy)),
            self.base)

    def test_manifest_permuted(self):
        m, a, p, pol = self.world
        self._same(list(reversed(m)), a, p, pol)

    def test_annotations_permuted(self):
        m, a, p, pol = self.world
        self._same(m, list(reversed(a)), p, pol)

    def test_predictions_permuted(self):
        m, a, p, pol = self.world
        self._same(m, a, list(reversed(p)), pol)

    def test_all_three_permuted(self):
        m, a, p, pol = self.world
        self._same(list(reversed(m)), list(reversed(a)), list(reversed(p)), pol)

    def test_rotations_are_all_identical(self):
        m, a, p, pol = self.world
        for shift in range(1, len(m)):
            self._same(m[shift:] + m[:shift], a[shift:] + a[:shift],
                       p[shift:] + p[:shift], pol)


class AdmissionRejectTests(unittest.TestCase):
    def test_a1_unknown_schema(self):
        m, a, p, pol = _world()
        m[0] = {**m[0], "schema": "aieq-v3-not-a-schema-1"}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a2_missing_schema(self):
        m, a, p, pol = _world()
        broken = dict(m[0])
        broken.pop("schema")
        m[0] = broken
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a3_right_schema_wrong_slot(self):
        """A valid annotation handed to the prediction slot is still a reject."""
        m, a, p, pol = _world()
        p[0] = annotation_clean(asset_id="asset-000")
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a4_fails_its_schema_validator(self):
        m, a, p, pol = _world()
        p[0] = {**p[0], "tonal_score": [0.5] * 119}   # wrong length
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_non_object_record(self):
        m, a, p, pol = _world()
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, ["not-an-object"], pol)


class HashRejectTests(unittest.TestCase):
    def test_a5_policy_hash_mismatch(self):
        m, a, p, pol = _world()
        p[0] = {**p[0], "calibration_policy_sha256": "ab" * 32}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a6_policy_wrong_schema(self):
        m, a, p, _pol = _world()
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, asset_manifest())

    def test_mutating_the_policy_invalidates_every_prediction(self):
        """Policy content is bound by hash, not by id."""
        m, a, p, pol = _world()
        mutated = copy.deepcopy(pol)
        mutated["semantic_type_thresholds"] = [0.1] * 8
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, mutated)

    def test_coherent_predictions_with_wrong_policy_id_rejected(self):
        """A run coherent with itself must still match the supplied policy."""
        m, a, p, pol = _world()
        p = [{**row, "calibration_policy_id": "wrong-policy-id"} for row in p]
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_coherent_predictions_with_wrong_model_hash_rejected(self):
        m, a, p, pol = _world()
        p = [{**row, "model_sha256": "12" * 32} for row in p]
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_coherent_predictions_with_wrong_frontend_hash_rejected(self):
        m, a, p, pol = _world()
        p = [{**row, "frontend_contract_sha256": "34" * 32} for row in p]
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)


class RunCoherenceRejectTests(unittest.TestCase):
    def test_a7_mixed_model_sha(self):
        m, a, p, pol = _world()
        p[1] = {**p[1], "model_sha256": "cd" * 32}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a8_mixed_frontend_hash(self):
        m, a, p, pol = _world()
        p[1] = {**p[1], "frontend_contract_sha256": "cd" * 32}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a9_mixed_policy_hash(self):
        m, a, p, pol = _world()
        p[1] = {**p[1], "calibration_policy_sha256": "cd" * 32}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)


class BindingRejectTests(unittest.TestCase):
    def test_a10_orphan_prediction(self):
        m, a, p, pol = _world()
        p[0] = {**p[0], "asset_id": "asset-999"}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a11_orphan_annotation(self):
        m, a, p, pol = _world()
        a[0] = {**a[0], "asset_id": "asset-999"}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a12_profile_drift_vs_manifest(self):
        m, a, p, pol = _world()
        p[0] = {**p[0], "profile": "drums"}
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)


class DuplicateRejectTests(unittest.TestCase):
    def test_a13_duplicate_manifest_asset_id(self):
        m, a, p, pol = _world()
        m.append(asset_manifest(asset_id="asset-000", relative_path="dup.wav"))
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a14_duplicate_prediction_key(self):
        m, a, p, pol = _world()
        p.append(copy.deepcopy(p[0]))
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)

    def test_a15_duplicate_annotation_key(self):
        m, a, p, pol = _world()
        a.append(copy.deepcopy(a[0]))
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, p, pol)


class EmptinessRejectTests(unittest.TestCase):
    def test_a16_empty_manifest(self):
        _m, a, p, pol = _world()
        with self.assertRaises(EvaluatorParseError):
            _parse([], a, p, pol)

    def test_a17_empty_predictions_is_not_a_pass(self):
        m, a, _p, pol = _world()
        with self.assertRaises(EvaluatorParseError):
            _parse(m, a, [], pol)


if __name__ == "__main__":
    unittest.main()
