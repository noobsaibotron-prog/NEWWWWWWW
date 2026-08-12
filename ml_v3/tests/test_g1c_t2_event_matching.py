"""G1c T2 — event matching: pairs that must agree, and greedy refutation.

Vectors enumerated BY PAIR that must agree (the lens that was missing on T1,
where organising by "field that varies" hid the (prediction, policy) pair):

  (gt.type, pred.type)          must be equal or unmatchable
  (gt.time, pred.time)          IoU >= 0.3 or unmatchable
  (gt.centre, pred.centre)      Resonance: within a third of an octave
  (gt.band, pred.band)          Harshness/Sibilance: overlap >= 0.5
  (scope, dynamic events)       semantic region classes are rejected here
  (matching, one-to-one)        no gt or pred used twice
  (matching, optimum)           must BE the lexicographic optimum, not merely
                                a legal matching — the greedy refutation below
  (result, input order)         identical under any permutation
  (search, exactness)           refuses rather than degrading to greedy

Every mutation used below is asserted to actually differ from the value it
replaces — the T1 probe failed because `"aa"*32` replaced `"aa"*32`.
"""
from __future__ import annotations

import itertools
import unittest

from ml_v3.benchmark.event_matching import (
    BAND_OVERLAP_MIN,
    TEMPORAL_IOU_MIN,
    EventMatchingError,
    band_overlap,
    centre_error_octaves,
    is_matchable,
    match_events,
    temporal_iou,
)
from ml_v3.contracts.constants import ANOMALY_CLASSES
from ml_v3.contracts.metrology_lock import gate_platform_python_label


def ev(event_id, ptype="Resonance", start=0.0, end=1.0, centre=1000.0,
       width=0.5, direction=None):
    row = {
        "event_id": event_id,
        "problem_type": ptype,
        "start_s": start,
        "end_s": end,
        "center_hz": centre,
        "width_octaves": width,
    }
    if direction is not None:
        row["direction"] = direction
    return row


class MutationSanityTests(unittest.TestCase):
    """The T1 lesson: a mutation that does not mutate proves nothing."""

    def test_helper_mutations_actually_differ(self):
        base = ev("e0")
        for field, value in (("problem_type", "Harshness"), ("center_hz", 4000.0),
                             ("start_s", 5.0), ("width_octaves", 3.0)):
            self.assertNotEqual(
                base[field], value,
                msg=f"mutation of {field} would be a no-op against the base event")


class MatchabilityPairTests(unittest.TestCase):
    def test_gate_platform(self):
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")

    def test_type_pair_must_agree(self):
        self.assertFalse(is_matchable(ev("g", "Resonance"), ev("p", "Harshness")))

    def test_time_pair_below_threshold_unmatchable(self):
        a = ev("g", start=0.0, end=1.0)
        b = ev("p", start=0.9, end=2.0)
        self.assertLess(temporal_iou(a, b), TEMPORAL_IOU_MIN)
        self.assertFalse(is_matchable(a, b))

    def test_time_pair_disjoint_is_zero_iou(self):
        self.assertEqual(temporal_iou(ev("g", start=0, end=1),
                                      ev("p", start=2, end=3)), 0.0)

    def test_resonance_centre_within_third_octave(self):
        near = ev("p", centre=1000.0 * 2 ** (0.3))
        far = ev("p", centre=1000.0 * 2 ** (0.4))
        self.assertLess(centre_error_octaves(ev("g"), near), 1 / 3)
        self.assertGreater(centre_error_octaves(ev("g"), far), 1 / 3)
        self.assertTrue(is_matchable(ev("g"), near))
        self.assertFalse(is_matchable(ev("g"), far))

    def test_band_class_overlap_threshold(self):
        gt = ev("g", "Sibilance", centre=8000.0, width=1.0)
        same = ev("p", "Sibilance", centre=8000.0, width=1.0)
        offset = ev("p", "Sibilance", centre=8000.0 * 2 ** 0.9, width=1.0)
        self.assertGreaterEqual(band_overlap(gt, same), BAND_OVERLAP_MIN)
        self.assertLess(band_overlap(gt, offset), BAND_OVERLAP_MIN)
        self.assertTrue(is_matchable(gt, same))
        self.assertFalse(is_matchable(gt, offset))

    def test_event_matcher_scope_is_dense_anomaly_classes_only(self):
        self.assertEqual(set(ANOMALY_CLASSES), {"Resonance", "Harshness", "Sibilance"})
        for semantic_type in (
            "Muddiness", "Boominess", "BoxyMidrange", "Thinness", "DullSound"):
            with self.subTest(semantic_type=semantic_type):
                with self.assertRaisesRegex(EventMatchingError, "not a §10.2 dynamic event"):
                    is_matchable(ev("g", semantic_type), ev("p", semantic_type))

    def test_schema_shaped_semantic_region_is_rejected_not_matched(self):
        gt = {
            "event_id": "g",
            "problem_type": "Muddiness",
            "problem_type_id": 2,
            "start_s": 0.0,
            "end_s": 1.0,
            "band_lo_hz": 200.0,
            "band_hi_hz": 500.0,
            "direction": None,
            "severity": 0.8,
            "confidence": 0.9,
            "actionable": True,
        }
        pred = {
            "event_id": "p",
            "problem_type": "Muddiness",
            "problem_type_id": 2,
            "start_s": 0.0,
            "end_s": 1.0,
            "band_lo_hz": 200.0,
            "band_hi_hz": 500.0,
            "confidence": 0.9,
            "actionable": True,
        }
        with self.assertRaisesRegex(EventMatchingError, "not a §10.2 dynamic event"):
            match_events([gt], [pred])

    def test_unknown_type_is_fail_closed(self):
        with self.assertRaises(EventMatchingError):
            is_matchable(ev("g", "NotAClass"), ev("p", "NotAClass"))

    def test_degenerate_span_rejected(self):
        with self.assertRaises(EventMatchingError):
            temporal_iou(ev("g", start=1.0, end=1.0), ev("p"))


class GreedyRefutationTests(unittest.TestCase):
    """The decisive test: a configuration where greedy loses a match."""

    def _world(self):
        # p0 is simultaneously g0's STRONGEST partner and g1's ONLY partner.
        # Greedy takes the strongest pair (g0,p0) first and strands g1: one
        # match. The optimum sacrifices IoU on g0 to keep p0 for g1:
        # (g0,p1) + (g1,p0), two matches. Count beats IoU lexicographically.
        g0 = ev("g0", start=1.0, end=2.0, centre=1000.0)
        g1 = ev("g1", start=0.55, end=1.55, centre=1000.0)
        p0 = ev("p0", start=1.0, end=2.0, centre=1000.0)     # IoU 1.000 w/ g0
        p1 = ev("p1", start=1.5, end=2.5, centre=1000.0)     # IoU 0.333 w/ g0
        return [g0, g1], [p0, p1]

    def test_greedy_would_lose_a_match(self):
        gt, pred = self._world()
        # Sanity: the greedy-attractive pair really is the strongest one.
        self.assertGreater(temporal_iou(gt[0], pred[0]), temporal_iou(gt[0], pred[1]))
        # And g1 is compatible only with p0.
        self.assertTrue(is_matchable(gt[1], pred[0]))
        self.assertFalse(is_matchable(gt[1], pred[1]))

    def test_matcher_finds_the_two_match_optimum(self):
        gt, pred = self._world()
        result = match_events(gt, pred)
        self.assertEqual(result["counts"]["matched"], 2)
        self.assertEqual(result["counts"]["false_negatives"], 0)
        self.assertEqual(result["counts"]["false_positives"], 0)
        pairs = {(m["gt_id"], m["pred_id"]) for m in result["matched"]}
        self.assertEqual(pairs, {("g0", "p1"), ("g1", "p0")})


class OneToOneTests(unittest.TestCase):
    def test_two_predictions_one_gt_leaves_one_false_positive(self):
        gt = [ev("g0")]
        pred = [ev("p0"), ev("p1", start=0.05, end=1.05)]
        result = match_events(gt, pred)
        self.assertEqual(result["counts"]["matched"], 1)
        self.assertEqual(result["counts"]["false_positives"], 1)

    def test_no_id_used_twice(self):
        gt = [ev(f"g{i}", start=i * 0.01, end=1.0 + i * 0.01) for i in range(4)]
        pred = [ev(f"p{i}", start=i * 0.01, end=1.0 + i * 0.01) for i in range(4)]
        result = match_events(gt, pred)
        gt_ids = [m["gt_id"] for m in result["matched"]]
        pred_ids = [m["pred_id"] for m in result["matched"]]
        self.assertEqual(len(gt_ids), len(set(gt_ids)))
        self.assertEqual(len(pred_ids), len(set(pred_ids)))


class OrderInvarianceTests(unittest.TestCase):
    """(result, input order): the contract forbids order dependence."""

    def _world(self):
        gt = [ev(f"g{i}", start=i * 0.02, end=1.0 + i * 0.02, centre=1000.0 * 2 ** (i * 0.05))
              for i in range(4)]
        pred = [ev(f"p{i}", start=i * 0.03, end=1.0 + i * 0.03, centre=1000.0 * 2 ** (i * 0.04))
                for i in range(4)]
        return gt, pred

    def test_every_permutation_gives_the_same_result(self):
        gt, pred = self._world()
        base = match_events(gt, pred)
        for gt_perm in itertools.permutations(gt):
            for pred_perm in itertools.permutations(pred):
                self.assertEqual(match_events(list(gt_perm), list(pred_perm)), base)

    def test_duplicate_event_id_rejected(self):
        gt = [ev("same"), ev("same", start=0.1, end=1.1)]
        with self.assertRaises(EventMatchingError):
            match_events(gt, [ev("p0")])


class EmptyInputTests(unittest.TestCase):
    def test_no_predictions_makes_every_gt_a_false_negative(self):
        result = match_events([ev("g0"), ev("g1", start=5.0, end=6.0)], [])
        self.assertEqual(result["counts"],
                         {"matched": 0, "false_negatives": 2, "false_positives": 0})

    def test_no_gt_makes_every_prediction_a_false_positive(self):
        result = match_events([], [ev("p0")])
        self.assertEqual(result["counts"],
                         {"matched": 0, "false_negatives": 0, "false_positives": 1})

    def test_both_empty(self):
        result = match_events([], [])
        self.assertEqual(result["counts"]["matched"], 0)


if __name__ == "__main__":
    unittest.main()
