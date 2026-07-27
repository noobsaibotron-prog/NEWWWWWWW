"""G1c T3 — semantic region matching (§10.1): pairs that must agree.

Vectors enumerated BY PAIR that must agree, as on T2, plus the two decisions
this tranche had to take against an under-specified contract:

  (gt.type, pred.type)            must be equal or unmatchable
  (gt.time, pred.time)            IoU >= 0.3 or unmatchable
  (gt.centre, pred.centre)        Resonance: derived centres within 1/3 octave
  (gt.band, pred.band)            Mud/Boom/Boxy/Harsh/Sib: overlap >= 0.5
  (gt.region, pred.region)        Thin/Dull: SAME §11.1 CANONICAL REGION —
                                  not band overlap; the two disagree, and
                                  RegionCriterionTests pins both directions
  (gt.direction, pred.direction)  Thin/Dull: GT field vs curve-inferred label
  (scope, eight classes)          §10.1 covers all eight, partitioned
  (schema, fixtures)              fixtures are complete schema records — the
                                  T2 false-green came from hand-shaped ones
  (matching, one-to-one)          no gt or pred used twice
  (matching, optimum)             must BE the lexicographic optimum — the
                                  greedy refutation below
  (result, input order)           identical under permutation of EITHER side
                                  independently, not only of both together
  (search, exactness)             refuses rather than degrading to greedy

Every mutation used below is asserted to actually differ from the value it
replaces — the T1 probe failed because `"aa"*32` replaced `"aa"*32`.
"""
from __future__ import annotations

import itertools
import math
import unittest
from unittest import mock

from ml_v3.benchmark import semantic_region_matching as srm
from ml_v3.benchmark.semantic_region_matching import (
    BAND_OVERLAP_MIN,
    DIRECTION_DEADBAND_DB,
    RESONANCE_CENTRE_TOLERANCE_OCTAVES,
    TEMPORAL_IOU_MIN,
    SemanticRegionMatchingError,
    band_overlap,
    canonical_region_index,
    centre_error_octaves,
    derive_direction,
    is_matchable,
    match_semantic_regions,
    region_centre_hz,
    temporal_iou,
)
from ml_v3.contracts.constants import GRID_BANDS, PROBLEM_TYPES
from ml_v3.contracts.grid import band_centers_hz, region_of_frequency
from ml_v3.contracts.metrology_lock import gate_platform_python_label
from ml_v3.contracts.schema_field_guard import (
    SchemaFieldClaim,
    SchemaFieldClaimError,
    validate_schema_field_claims,
)
from ml_v3.contracts.schemas import SEMANTIC_BUNDLE_KEYS, SEMANTIC_REGION_KEYS


def region(problem_type="Muddiness", start=0.0, end=1.0, lo=200.0, hi=500.0,
           direction=None):
    """A COMPLETE §10.1 semantic region record (all SEMANTIC_REGION_KEYS)."""
    return {
        "problem_type": problem_type,
        "problem_type_id": PROBLEM_TYPES.index(problem_type),
        "start_s": start,
        "end_s": end,
        "band_lo_hz": lo,
        "band_hi_hz": hi,
        "direction": direction,
        "severity": 0.8,
        "confidence": 0.9,
        "actionable": True,
    }


def bundle(problem_type="Muddiness", start=0.0, end=1.0, lo=200.0, hi=500.0):
    """A COMPLETE prediction semantic bundle (all SEMANTIC_BUNDLE_KEYS)."""
    return {
        "problem_type": problem_type,
        "problem_type_id": PROBLEM_TYPES.index(problem_type),
        "start_s": start,
        "end_s": end,
        "band_lo_hz": lo,
        "band_hi_hz": hi,
        "confidence": 0.9,
        "actionable": True,
    }


def flat(value=0.0):
    """A tonal curve that is constant across all 120 bands."""
    return [float(value)] * GRID_BANDS


FLAT = flat(0.0)


class FixtureShapeTests(unittest.TestCase):
    """The T2 lesson: fixtures must be schema records, not convenient dicts."""

    def test_gate_platform(self):
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")

    def test_gt_fixture_is_a_complete_semantic_region_record(self):
        self.assertEqual(frozenset(region()), SEMANTIC_REGION_KEYS)

    def test_pred_fixture_is_a_complete_semantic_bundle_record(self):
        self.assertEqual(frozenset(bundle()), SEMANTIC_BUNDLE_KEYS)

    def test_prediction_bundles_carry_no_direction(self):
        """Why direction must be inferred at all."""
        self.assertNotIn("direction", SEMANTIC_BUNDLE_KEYS)

    def test_semantic_regions_carry_no_centre(self):
        """The §9.2 gap this module works around: 'per Resonance e obbligatorio
        il centro', but the schema has no centre to read."""
        self.assertNotIn("center_hz", SEMANTIC_REGION_KEYS)


class MutationSanityTests(unittest.TestCase):
    def test_helper_mutations_actually_differ(self):
        base = region()
        for field, value in (("problem_type", "Thinness"), ("band_lo_hz", 2000.0),
                             ("band_hi_hz", 5000.0), ("start_s", 5.0),
                             ("direction", "boost")):
            self.assertNotEqual(
                base[field], value,
                msg=f"mutation of {field} would be a no-op against the base region")


class ScopeAndPartitionTests(unittest.TestCase):
    def test_criteria_partition_all_eight_public_classes(self):
        centre = srm._CENTRE_CLASSES
        band = srm._BAND_CLASSES
        region_dir = srm._REGION_DIRECTION_CLASSES
        self.assertEqual(centre | band | region_dir, frozenset(PROBLEM_TYPES))
        self.assertEqual(len(centre) + len(band) + len(region_dir), len(PROBLEM_TYPES))
        self.assertEqual(centre, frozenset({"Resonance"}))
        self.assertEqual(region_dir, frozenset({"Thinness", "DullSound"}))

    def test_every_public_class_is_reachable(self):
        for problem_type in PROBLEM_TYPES:
            with self.subTest(problem_type=problem_type):
                gt = region(problem_type, direction="cut")
                pred = bundle(problem_type)
                self.assertIsInstance(
                    is_matchable(gt, pred, tonal_curve_db=flat(-3.0)), bool)

    def test_unknown_type_is_fail_closed(self):
        gt = {**region(), "problem_type": "NotAClass"}
        with self.assertRaisesRegex(
                SemanticRegionMatchingError, "not a §10.1 semantic region"):
            is_matchable(gt, bundle(), tonal_curve_db=FLAT)

    @staticmethod
    def _dynamic_event():
        return {
            "problem_type": "Resonance",
            "problem_type_id": 0,
            "start_s": 0.0,
            "end_s": 1.0,
            "center_hz": 1000.0,
            "width_octaves": 0.5,
            "severity": 0.8,
            "confidence": 0.9,
            "actionable": True,
        }

    def test_dynamic_event_shape_is_rejected_not_matched(self):
        """A §10.2 record handed to the §10.1 matcher must not silently work.

        The dangerous outcome is not a crash but a *quiet* one: an event has no
        band keys, so treating "absent" like "null" would make it unmatchable
        and drop it into the false-negative count as if the model had missed a
        real region.
        """
        event = self._dynamic_event()
        with self.assertRaisesRegex(
                SemanticRegionMatchingError, "not a §10.1 semantic region"):
            match_semantic_regions([event], [event], tonal_curve_db=FLAT)

    def test_absent_band_key_and_null_band_are_not_the_same_thing(self):
        absent = self._dynamic_event()
        null = region("Resonance", lo=None, hi=None)
        self.assertNotIn("band_lo_hz", absent)
        self.assertIn("band_lo_hz", null)
        with self.assertRaises(SemanticRegionMatchingError):
            is_matchable(absent, bundle("Resonance"), tonal_curve_db=FLAT)
        self.assertFalse(is_matchable(null, bundle("Resonance"), tonal_curve_db=FLAT))

    def test_prediction_of_dynamic_event_shape_is_rejected(self):
        with self.assertRaisesRegex(
                SemanticRegionMatchingError, "not a §10.1 semantic region"):
            match_semantic_regions(
                [region("Resonance")], [self._dynamic_event()], tonal_curve_db=FLAT)

    def test_ground_truth_without_a_direction_key_is_rejected(self):
        no_direction = {k: v for k, v in region("Thinness", lo=520.0, hi=700.0).items()
                        if k != "direction"}
        with self.assertRaisesRegex(
                SemanticRegionMatchingError, "no direction"):
            is_matchable(no_direction, bundle("Thinness", lo=520.0, hi=700.0),
                         tonal_curve_db=flat(-3.0))


class MatchabilityPairTests(unittest.TestCase):
    def test_type_pair_must_agree(self):
        self.assertFalse(is_matchable(
            region("Muddiness"), bundle("Boominess"), tonal_curve_db=FLAT))

    def test_time_pair_below_threshold_unmatchable(self):
        gt = region(start=0.0, end=1.0)
        pred = bundle(start=0.9, end=2.0)
        self.assertLess(temporal_iou(gt, pred), TEMPORAL_IOU_MIN)
        self.assertFalse(is_matchable(gt, pred, tonal_curve_db=FLAT))

    def test_time_pair_disjoint_is_zero_iou(self):
        self.assertEqual(
            temporal_iou(region(start=0.0, end=1.0), bundle(start=2.0, end=3.0)), 0.0)

    def test_degenerate_span_rejected(self):
        with self.assertRaises(SemanticRegionMatchingError):
            temporal_iou(region(start=1.0, end=1.0), bundle())


class ResonanceCentreTests(unittest.TestCase):
    def test_centre_is_the_geometric_mean_of_the_band(self):
        self.assertAlmostEqual(
            region_centre_hz(region(lo=200.0, hi=800.0)), 400.0, places=9)

    def test_centre_within_a_third_of_an_octave(self):
        gt = region("Resonance", lo=1000.0 / math.sqrt(2), hi=1000.0 * math.sqrt(2))
        near = bundle("Resonance", lo=1000.0 * 2 ** 0.3 / math.sqrt(2),
                      hi=1000.0 * 2 ** 0.3 * math.sqrt(2))
        far = bundle("Resonance", lo=1000.0 * 2 ** 0.4 / math.sqrt(2),
                     hi=1000.0 * 2 ** 0.4 * math.sqrt(2))
        self.assertLess(centre_error_octaves(gt, near),
                        RESONANCE_CENTRE_TOLERANCE_OCTAVES)
        self.assertGreater(centre_error_octaves(gt, far),
                           RESONANCE_CENTRE_TOLERANCE_OCTAVES)
        self.assertTrue(is_matchable(gt, near, tonal_curve_db=FLAT))
        self.assertFalse(is_matchable(gt, far, tonal_curve_db=FLAT))

    def test_derived_centre_is_blind_to_band_width(self):
        """Documented consequence of the §9.2 gap, pinned so it cannot surprise.

        With the centre derived on both sides, a one-octave and a one-semitone
        region around the same midpoint are indistinguishable to the Resonance
        criterion. If the contract later supplies a real centre, this test is
        the one that must change.
        """
        narrow = region("Resonance", lo=1000.0 / 2 ** (1 / 24), hi=1000.0 * 2 ** (1 / 24))
        wide = bundle("Resonance", lo=500.0, hi=2000.0)
        self.assertAlmostEqual(centre_error_octaves(narrow, wide), 0.0, places=9)
        self.assertLess(band_overlap(narrow, wide), BAND_OVERLAP_MIN)
        self.assertTrue(is_matchable(narrow, wide, tonal_curve_db=FLAT))


class BandClassTests(unittest.TestCase):
    def test_all_five_band_classes_use_the_overlap_threshold(self):
        for problem_type in sorted(srm._BAND_CLASSES):
            with self.subTest(problem_type=problem_type):
                gt = region(problem_type, lo=200.0, hi=800.0)
                same = bundle(problem_type, lo=200.0, hi=800.0)
                offset = bundle(problem_type, lo=1600.0, hi=6400.0)
                self.assertGreaterEqual(band_overlap(gt, same), BAND_OVERLAP_MIN)
                self.assertLess(band_overlap(gt, offset), BAND_OVERLAP_MIN)
                self.assertTrue(is_matchable(gt, same, tonal_curve_db=FLAT))
                self.assertFalse(is_matchable(gt, offset, tonal_curve_db=FLAT))

    def test_band_classes_ignore_direction(self):
        """Only Thinness/DullSound are directional in §10.1."""
        gt = region("Muddiness", lo=200.0, hi=500.0, direction="boost")
        pred = bundle("Muddiness", lo=200.0, hi=500.0)
        self.assertTrue(is_matchable(gt, pred, tonal_curve_db=flat(-6.0)))
        self.assertTrue(is_matchable(gt, pred, tonal_curve_db=flat(+6.0)))

    def test_disjoint_bands_give_zero_overlap(self):
        self.assertEqual(
            band_overlap(region(lo=100.0, hi=200.0), bundle(lo=400.0, hi=800.0)), 0.0)


class RegionCriterionTests(unittest.TestCase):
    """The decision: "stessa regione" is the §11.1 canonical region.

    Both tests below are chosen so that band overlap and canonical region give
    OPPOSITE answers. They are the evidence that the reading is a real choice
    and not a restatement of the overlap rule.
    """

    def test_high_band_overlap_but_different_canonical_regions_does_not_match(self):
        gt = region("Thinness", lo=150.0, hi=250.0, direction="cut")
        pred = bundle("Thinness", lo=160.0, hi=260.0)
        # Overlap would say yes, loudly.
        self.assertGreater(band_overlap(gt, pred), 0.8)
        self.assertGreaterEqual(band_overlap(gt, pred), BAND_OVERLAP_MIN)
        # The canonical regions straddle the 200 Hz boundary.
        self.assertEqual(canonical_region_index(gt), 1)
        self.assertEqual(canonical_region_index(pred), 2)
        self.assertFalse(is_matchable(gt, pred, tonal_curve_db=flat(-3.0)))

    def test_zero_band_overlap_but_same_canonical_region_matches(self):
        gt = region("DullSound", lo=520.0, hi=700.0, direction="cut")
        pred = bundle("DullSound", lo=1400.0, hi=1900.0)
        self.assertEqual(band_overlap(gt, pred), 0.0)
        self.assertEqual(canonical_region_index(gt), 3)
        self.assertEqual(canonical_region_index(pred), 3)
        self.assertTrue(is_matchable(gt, pred, tonal_curve_db=flat(-3.0)))

    def test_region_is_located_by_the_derived_centre(self):
        gt = region("Thinness", lo=150.0, hi=250.0, direction="cut")
        centre = region_centre_hz(gt)
        self.assertAlmostEqual(centre, math.sqrt(150.0 * 250.0), places=9)
        self.assertEqual(canonical_region_index(gt), region_of_frequency(centre))

    def test_centre_outside_the_grid_is_unmatchable_not_clamped(self):
        gt = region("Thinness", lo=1.0, hi=4.0, direction="cut")     # centre 2 Hz
        pred = bundle("Thinness", lo=1.0, hi=4.0)
        self.assertIsNone(canonical_region_index(gt))
        self.assertFalse(is_matchable(gt, pred, tonal_curve_db=flat(-3.0)))

    def test_same_region_but_opposite_direction_does_not_match(self):
        gt = region("Thinness", lo=520.0, hi=700.0, direction="boost")
        pred = bundle("Thinness", lo=600.0, hi=800.0)
        self.assertEqual(canonical_region_index(gt), canonical_region_index(pred))
        self.assertEqual(derive_direction(pred, flat(-3.0)), "cut")
        self.assertFalse(is_matchable(gt, pred, tonal_curve_db=flat(-3.0)))
        self.assertTrue(is_matchable(gt, pred, tonal_curve_db=flat(+3.0)))


class DirectionDerivationTests(unittest.TestCase):
    def test_boost_cut_and_neutral(self):
        pred = bundle("Thinness", lo=200.0, hi=500.0)
        self.assertEqual(derive_direction(pred, flat(+3.0)), "boost")
        self.assertEqual(derive_direction(pred, flat(-3.0)), "cut")
        self.assertEqual(derive_direction(pred, flat(0.0)), "neutral")

    def test_deadband_edges_are_inclusive_towards_the_direction(self):
        pred = bundle("Thinness", lo=200.0, hi=500.0)
        self.assertEqual(
            derive_direction(pred, flat(DIRECTION_DEADBAND_DB)), "boost")
        self.assertEqual(
            derive_direction(pred, flat(-DIRECTION_DEADBAND_DB)), "cut")
        self.assertEqual(
            derive_direction(pred, flat(DIRECTION_DEADBAND_DB - 1e-9)), "neutral")

    def test_neutral_prediction_matches_no_directional_ground_truth(self):
        """The mechanical recall cost of the dead-band, pinned."""
        pred = bundle("Thinness", lo=520.0, hi=700.0)
        self.assertEqual(derive_direction(pred, flat(0.0)), "neutral")
        for direction in ("boost", "cut"):
            with self.subTest(direction=direction):
                gt = region("Thinness", lo=520.0, hi=700.0, direction=direction)
                self.assertFalse(is_matchable(gt, pred, tonal_curve_db=flat(0.0)))

    def test_direction_is_derived_over_the_prediction_band_only(self):
        """DIRECTION_DOMAIN: a bundle's direction must not depend on the GT."""
        pred = bundle("Thinness", lo=200.0, hi=500.0)
        curve = flat(0.0)
        centres = band_centers_hz()
        for index in range(GRID_BANDS):
            if 200.0 <= centres[index] <= 500.0:
                curve[index] = +4.0
            else:
                curve[index] = -9.0        # outside the bundle: must not count
        self.assertEqual(derive_direction(pred, curve), "boost")

    def test_band_too_narrow_to_contain_a_centre_is_unmatchable(self):
        centres = band_centers_hz()
        lo = centres[60] * 1.001
        hi = centres[61] * 0.999
        self.assertLess(lo, hi)
        self.assertFalse(any(lo <= c <= hi for c in centres))
        pred = bundle("Thinness", lo=lo, hi=hi)
        self.assertIsNone(derive_direction(pred, flat(+6.0)))
        gt = region("Thinness", lo=lo, hi=hi, direction="boost")
        self.assertFalse(is_matchable(gt, pred, tonal_curve_db=flat(+6.0)))

    def test_ground_truth_direction_outside_the_vocabulary_is_fail_closed(self):
        gt = region("Thinness", lo=520.0, hi=700.0, direction="louder")
        with self.assertRaisesRegex(SemanticRegionMatchingError, "vocabulary"):
            is_matchable(gt, bundle("Thinness", lo=520.0, hi=700.0),
                         tonal_curve_db=flat(+3.0))


class NullAndMalformedTests(unittest.TestCase):
    """Schema-legal nulls are unmatchable; present-but-unusable values raise."""

    def test_null_band_on_ground_truth_is_unmatchable(self):
        for problem_type in PROBLEM_TYPES:
            with self.subTest(problem_type=problem_type):
                gt = region(problem_type, lo=None, hi=None, direction="cut")
                self.assertFalse(is_matchable(
                    gt, bundle(problem_type), tonal_curve_db=flat(-3.0)))

    def test_null_band_on_prediction_is_unmatchable(self):
        for problem_type in PROBLEM_TYPES:
            with self.subTest(problem_type=problem_type):
                pred = {**bundle(problem_type), "band_lo_hz": None,
                        "band_hi_hz": None}
                self.assertFalse(is_matchable(
                    region(problem_type, direction="cut"), pred,
                    tonal_curve_db=flat(-3.0)))

    def test_null_direction_on_directional_class_is_unmatchable(self):
        for problem_type in sorted(srm._REGION_DIRECTION_CLASSES):
            with self.subTest(problem_type=problem_type):
                gt = region(problem_type, lo=520.0, hi=700.0, direction=None)
                pred = bundle(problem_type, lo=520.0, hi=700.0)
                self.assertFalse(is_matchable(gt, pred, tonal_curve_db=flat(-3.0)))

    def test_null_band_makes_a_ground_truth_region_a_false_negative(self):
        gt = region("Muddiness", lo=None, hi=None)
        result = match_semantic_regions([gt], [bundle("Muddiness")],
                                        tonal_curve_db=FLAT)
        self.assertEqual(result["counts"],
                         {"matched": 0, "false_negatives": 1, "false_positives": 1})

    def test_inverted_band_raises(self):
        with self.assertRaisesRegex(SemanticRegionMatchingError, "must exceed"):
            band_overlap(region(lo=500.0, hi=200.0), bundle())

    def test_non_positive_band_raises(self):
        with self.assertRaisesRegex(SemanticRegionMatchingError, "must be positive"):
            band_overlap(region(lo=0.0, hi=200.0), bundle())

    def test_non_finite_band_raises(self):
        with self.assertRaises(SemanticRegionMatchingError):
            band_overlap(region(lo=float("nan"), hi=200.0), bundle())

    def test_boolean_is_not_a_number(self):
        with self.assertRaises(SemanticRegionMatchingError):
            band_overlap(region(lo=True, hi=200.0), bundle())


class TonalCurveValidationTests(unittest.TestCase):
    def test_wrong_length_curve_rejected(self):
        with self.assertRaisesRegex(SemanticRegionMatchingError, "120 entries"):
            match_semantic_regions([], [], tonal_curve_db=[0.0] * (GRID_BANDS - 1))

    def test_non_finite_curve_rejected(self):
        curve = flat(0.0)
        curve[7] = float("inf")
        with self.assertRaises(SemanticRegionMatchingError):
            match_semantic_regions([], [], tonal_curve_db=curve)

    def test_non_sequence_curve_rejected(self):
        with self.assertRaisesRegex(SemanticRegionMatchingError, "must be a sequence"):
            match_semantic_regions([], [], tonal_curve_db=None)

    def test_string_is_not_a_curve(self):
        with self.assertRaisesRegex(SemanticRegionMatchingError, "must be a sequence"):
            match_semantic_regions([], [], tonal_curve_db="x" * GRID_BANDS)


class GreedyRefutationTests(unittest.TestCase):
    """A configuration where greedy loses a match."""

    def _world(self):
        # p0 is simultaneously g0's STRONGEST partner and g1's ONLY partner.
        # Greedy takes (g0,p0) first and strands g1: one match. The optimum
        # sacrifices IoU on g0 to keep p0 for g1, for two.
        g0 = region("Muddiness", start=1.0, end=2.0)
        g1 = region("Muddiness", start=0.55, end=1.55)
        p0 = bundle("Muddiness", start=1.0, end=2.0)     # IoU 1.000 with g0
        p1 = bundle("Muddiness", start=1.5, end=2.5)     # IoU 0.333 with g0
        return [g0, g1], [p0, p1]

    def test_greedy_would_lose_a_match(self):
        gt, pred = self._world()
        self.assertGreater(temporal_iou(gt[0], pred[0]), temporal_iou(gt[0], pred[1]))
        self.assertTrue(is_matchable(gt[1], pred[0], tonal_curve_db=FLAT))
        self.assertFalse(is_matchable(gt[1], pred[1], tonal_curve_db=FLAT))

    def test_matcher_finds_the_two_match_optimum(self):
        gt, pred = self._world()
        result = match_semantic_regions(gt, pred, tonal_curve_db=FLAT)
        self.assertEqual(result["counts"],
                         {"matched": 2, "false_negatives": 0, "false_positives": 0})
        # Canonical ids follow content order: gt#000000 is the earlier g1.
        pairs = {(m["gt_id"], m["pred_id"]) for m in result["matched"]}
        self.assertEqual(pairs, {("gt#000000", "pred#000000"),
                                 ("gt#000001", "pred#000001")})


class OneToOneTests(unittest.TestCase):
    def test_two_predictions_one_gt_leaves_one_false_positive(self):
        gt = [region("Muddiness")]
        pred = [bundle("Muddiness"), bundle("Muddiness", start=0.05, end=1.05)]
        result = match_semantic_regions(gt, pred, tonal_curve_db=FLAT)
        self.assertEqual(result["counts"]["matched"], 1)
        self.assertEqual(result["counts"]["false_positives"], 1)

    def test_no_id_used_twice(self):
        gt = [region("Muddiness", start=i * 0.01, end=1.0 + i * 0.01) for i in range(4)]
        pred = [bundle("Muddiness", start=i * 0.01, end=1.0 + i * 0.01)
                for i in range(4)]
        result = match_semantic_regions(gt, pred, tonal_curve_db=FLAT)
        gt_ids = [m["gt_id"] for m in result["matched"]]
        pred_ids = [m["pred_id"] for m in result["matched"]]
        self.assertEqual(len(gt_ids), len(set(gt_ids)))
        self.assertEqual(len(pred_ids), len(set(pred_ids)))


class OrderInvarianceTests(unittest.TestCase):
    """Regions have no id field, so ids are synthesised — from content, not
    arrival order. Permuting ONE side must not change the returned structure."""

    def _world(self):
        gt = [
            region("Muddiness", start=0.0, end=1.0, lo=200.0, hi=500.0),
            region("Boominess", start=0.2, end=1.2, lo=40.0, hi=120.0),
            region("Thinness", start=0.4, end=1.4, lo=520.0, hi=700.0,
                   direction="cut"),
            region("Sibilance", start=0.6, end=1.6, lo=5000.0, hi=9000.0),
        ]
        pred = [
            bundle("Muddiness", start=0.05, end=1.05, lo=210.0, hi=520.0),
            bundle("Boominess", start=0.25, end=1.25, lo=42.0, hi=125.0),
            bundle("Thinness", start=0.45, end=1.45, lo=600.0, hi=800.0),
            bundle("Sibilance", start=0.65, end=1.65, lo=5200.0, hi=9400.0),
        ]
        return gt, pred

    def setUp(self):
        self.gt, self.pred = self._world()
        self.curve = flat(-3.0)
        self.base = match_semantic_regions(
            self.gt, self.pred, tonal_curve_db=self.curve)

    def test_the_world_actually_matches_something(self):
        self.assertGreater(self.base["counts"]["matched"], 0)

    def test_permuting_ground_truth_only(self):
        for permutation in itertools.permutations(self.gt):
            self.assertEqual(
                match_semantic_regions(list(permutation), self.pred,
                                       tonal_curve_db=self.curve),
                self.base)

    def test_permuting_predictions_only(self):
        for permutation in itertools.permutations(self.pred):
            self.assertEqual(
                match_semantic_regions(self.gt, list(permutation),
                                       tonal_curve_db=self.curve),
                self.base)

    def test_permuting_both_sides(self):
        for gt_perm in itertools.permutations(self.gt):
            for pred_perm in itertools.permutations(self.pred):
                self.assertEqual(
                    match_semantic_regions(list(gt_perm), list(pred_perm),
                                           tonal_curve_db=self.curve),
                    self.base)

    def test_identical_duplicate_regions_stay_deterministic(self):
        gt = [region("Muddiness"), region("Muddiness")]
        pred = [bundle("Muddiness")]
        base = match_semantic_regions(gt, pred, tonal_curve_db=FLAT)
        self.assertEqual(
            match_semantic_regions(list(reversed(gt)), pred, tonal_curve_db=FLAT),
            base)
        self.assertEqual(base["counts"],
                         {"matched": 1, "false_negatives": 1, "false_positives": 0})


class EmptyInputTests(unittest.TestCase):
    def test_no_predictions_makes_every_gt_a_false_negative(self):
        result = match_semantic_regions(
            [region("Muddiness"), region("Boominess", start=5.0, end=6.0)],
            [], tonal_curve_db=FLAT)
        self.assertEqual(result["counts"],
                         {"matched": 0, "false_negatives": 2, "false_positives": 0})

    def test_no_gt_makes_every_prediction_a_false_positive(self):
        result = match_semantic_regions([], [bundle("Muddiness")], tonal_curve_db=FLAT)
        self.assertEqual(result["counts"],
                         {"matched": 0, "false_negatives": 0, "false_positives": 1})

    def test_both_empty(self):
        result = match_semantic_regions([], [], tonal_curve_db=FLAT)
        self.assertEqual(result["counts"]["matched"], 0)


class SearchCapTests(unittest.TestCase):
    def test_refuses_rather_than_degrading_to_greedy(self):
        gt = [region("Muddiness", start=i * 0.01, end=1.0 + i * 0.01)
              for i in range(6)]
        pred = [bundle("Muddiness", start=i * 0.01, end=1.0 + i * 0.01)
                for i in range(6)]
        with mock.patch.object(srm, "MAX_SEARCH_NODES", 8):
            with self.assertRaisesRegex(
                    SemanticRegionMatchingError, "refusing to fall"):
                match_semantic_regions(gt, pred, tonal_curve_db=FLAT)

    def test_the_same_world_succeeds_under_the_real_cap(self):
        gt = [region("Muddiness", start=i * 0.01, end=1.0 + i * 0.01)
              for i in range(6)]
        pred = [bundle("Muddiness", start=i * 0.01, end=1.0 + i * 0.01)
                for i in range(6)]
        result = match_semantic_regions(gt, pred, tonal_curve_db=FLAT)
        self.assertEqual(result["counts"]["matched"], 6)


class SchemaFieldClaimTests(unittest.TestCase):
    def test_declared_field_reads_are_schema_valid(self):
        validate_schema_field_claims(srm.SEMANTIC_REGION_MATCHING_SCHEMA_FIELD_CLAIMS)

    def test_matcher_is_bound_to_region_schemas_not_event_shape(self):
        records = {claim.schema_record
                   for claim in srm.SEMANTIC_REGION_MATCHING_SCHEMA_FIELD_CLAIMS}
        self.assertEqual(records, {"semantic_region", "semantic_bundle", "prediction"})
        for claim in srm.SEMANTIC_REGION_MATCHING_SCHEMA_FIELD_CLAIMS:
            self.assertNotIn("center_hz", claim.fields)
            self.assertNotIn("width_octaves", claim.fields)

    def test_guard_would_refute_reading_a_centre_from_a_semantic_region(self):
        """Proof that the §9.2 centre really is absent from the schema."""
        bad_claim = SchemaFieldClaim(
            owner="regression.t3_centre_gap",
            schema_record="semantic_region",
            fields=frozenset({"problem_type", "center_hz"}),
            purpose="reading the §9.2 Resonance centre the schema does not carry",
        )
        with self.assertRaisesRegex(
                SchemaFieldClaimError, "semantic_region lacks fields"):
            validate_schema_field_claims([bad_claim])

    def test_guard_would_refute_reading_a_direction_from_a_bundle(self):
        bad_claim = SchemaFieldClaim(
            owner="regression.t3_direction_gap",
            schema_record="semantic_bundle",
            fields=frozenset({"problem_type", "direction"}),
            purpose="reading a predicted direction the schema does not carry",
        )
        with self.assertRaisesRegex(
                SchemaFieldClaimError, "semantic_bundle lacks fields"):
            validate_schema_field_claims([bad_claim])


if __name__ == "__main__":
    unittest.main()
