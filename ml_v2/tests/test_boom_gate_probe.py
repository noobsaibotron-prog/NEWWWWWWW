"""Deterministic guards for the dataset-only Boom measurement probe."""
from __future__ import annotations

import unittest

import numpy as np

from ml_v2 import dataset_v2 as ds
from ml_v2 import lab_inject as li
from ml_v2.boom_gate_probe import measure_boom_window, qualifies


def shaped_window(*, low_db: float, mid_db: float,
                  other_db: float = -70.0, sample_rate: float = 44100.0
                  ) -> np.ndarray:
    bins = 2049
    bin_hz = sample_rate / ((bins - 1) * 2)
    freqs = np.arange(bins) * bin_hz
    frame = np.full(bins, other_db, dtype=np.float64)
    frame[(freqs >= 40.0) & (freqs <= 150.0)] = low_db
    frame[(freqs > 150.0) & (freqs <= 400.0)] = mid_db
    return np.repeat(frame[None, :], 32, axis=0)


class BoomGateProbeTest(unittest.TestCase):
    def test_measurement_is_scale_invariant(self):
        window = shaped_window(low_db=-55.0, mid_db=-60.0)
        a = measure_boom_window(window, 44100.0, 9.0)
        b = measure_boom_window(window + 12.0, 44100.0, 9.0)
        self.assertAlmostEqual(a.content_relative_db, b.content_relative_db, places=9)
        self.assertAlmostEqual(a.pre_excess_db, b.pre_excess_db, places=9)
        self.assertAlmostEqual(a.post_excess_db, b.post_excess_db, places=9)
        self.assertAlmostEqual(a.delta_excess_db, b.delta_excess_db, places=9)

    def test_gain_increases_measured_excess_without_mutating_input(self):
        window = shaped_window(low_db=-60.0, mid_db=-60.0)
        original = window.copy()
        low = measure_boom_window(window, 44100.0, 6.0)
        high = measure_boom_window(window, 44100.0, 12.0)
        np.testing.assert_array_equal(window, original)
        self.assertGreater(high.post_excess_db, low.post_excess_db)
        self.assertGreater(high.delta_excess_db, low.delta_excess_db)

    def test_gate_rejects_missing_content_and_already_boomy_raw(self):
        missing = measure_boom_window(
            shaped_window(low_db=-95.0, mid_db=-60.0), 44100.0, 9.0)
        already_boomy = measure_boom_window(
            shaped_window(low_db=-35.0, mid_db=-70.0), 44100.0, 9.0)
        valid = measure_boom_window(
            shaped_window(low_db=-55.0, mid_db=-58.0), 44100.0, 9.0)
        gate = dict(content_min_db=3.0, pre_excess_max_db=6.0,
                    post_excess_min_db=6.0, delta_excess_min_db=4.0)
        self.assertFalse(qualifies(missing, **gate))
        self.assertFalse(qualifies(already_boomy, **gate))
        self.assertTrue(qualifies(valid, **gate))

    def test_invalid_inputs_fail_closed(self):
        with self.assertRaises(ValueError):
            measure_boom_window(np.zeros((32, 2049)), 0.0, 9.0)
        with self.assertRaises(ValueError):
            measure_boom_window(np.zeros((32, 2049)), 44100.0, 0.0)
        bad = np.zeros((32, 2049))
        bad[0, 0] = np.nan
        with self.assertRaises(ValueError):
            measure_boom_window(bad, 44100.0, 9.0)

    def test_dataset_gate_is_opt_in_and_control_path_is_exact(self):
        window = shaped_window(low_db=-95.0, mid_db=-60.0)
        spec = next(item for item in li.injections_for() if item.problem == 4)
        default = li.inject_window_db(
            window, 44100.0, np.random.default_rng(42), spec)
        explicit_off = li.inject_window_db(
            window, 44100.0, np.random.default_rng(42), spec,
            measured_boom_gate=False)
        gated = li.inject_window_db(
            window, 44100.0, np.random.default_rng(42), spec,
            measured_boom_gate=True)
        self.assertIsNotNone(default)
        self.assertIsNotNone(explicit_off)
        np.testing.assert_array_equal(default[0], explicit_off[0])
        self.assertEqual(default[1], explicit_off[1])
        self.assertIsNone(gated)

    def test_gate_accepts_frozen_candidate_shape(self):
        window = shaped_window(low_db=-55.0, mid_db=-58.0)
        spec = next(item for item in li.injections_for() if item.problem == 4)
        got = li.inject_window_db(
            window, 44100.0, np.random.default_rng(7), spec,
            measured_boom_gate=True)
        self.assertIsNotNone(got)

    def test_dataset_cache_key_records_gate_state(self):
        control = ds.BuildConfig(split="train", seed=42)
        gated = ds.BuildConfig(split="train", seed=42,
                               measured_boom_gate=True)
        self.assertNotEqual(control.key(), gated.key())


if __name__ == "__main__":
    unittest.main()
