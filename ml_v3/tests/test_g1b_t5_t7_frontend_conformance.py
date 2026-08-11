"""Closing tests and reject-paths for G1 frontend Gates 5–7."""
from __future__ import annotations

import math
import unittest
from unittest import mock

import numpy as np

import ml_v3.benchmark as benchmark_package
import ml_v3.contracts as contracts_package
from ml_v3.benchmark.frontend_conformance import (
    FrontendConformanceError,
    _alias_threshold,
    _channel_equivalence,
    _eligible_psd_mask,
    _evaluate_gain_case,
    _frame_sequence_failures,
    _max_cell,
    _measure_hann_peak,
    _mono_reference_validity_failures,
    _prominence_eligible_mask,
    _require_gate_platform,
    _run_gate6,
    _strict_threshold,
    _support_failures,
    run_frontend_conformance,
)
from ml_v3.contracts.constants import GRID_BANDS


def _synthetic_frame(
    *,
    frame_index: int = 0,
    frame_end_sample: int = 8192,
    gain_db: float = 0.0,
    corrupt_band: int | None = None,
) -> dict[str, object]:
    psd = np.full(GRID_BANDS, -60.0 + gain_db, dtype=np.float64)
    shape = np.full(GRID_BANDS, -20.0, dtype=np.float64)
    if corrupt_band is not None:
        psd[corrupt_band] += 1.0
        shape[corrupt_band] += 1.0
    zero = np.zeros(GRID_BANDS, dtype=np.float64)
    return {
        "frame_index": frame_index,
        "frame_end_sample": frame_end_sample,
        "source_time_num": frame_end_sample,
        "source_time_den": 48_000,
        "canonical_sample_rate": 48_000,
        "valid": True,
        "reason": None,
        "mid_valid": True,
        "side_valid": False,
        "mid_psd_db": psd.tolist(),
        "mid_shape_db": shape.tolist(),
        "mid_prominence_db": zero.tolist(),
        "mid_delta_db": zero.tolist(),
        "mid_level_dbfs": -24.0 + gain_db,
        "side_psd_db": np.full(GRID_BANDS, -120.0).tolist(),
        "side_shape_db": np.full(GRID_BANDS, -120.0).tolist(),
        "side_prominence_db": zero.tolist(),
        "side_delta_db": zero.tolist(),
        "side_level_dbfs": -120.0,
    }


class Gate5RejectPathTests(unittest.TestCase):
    def test_clamp_boundary_is_excluded_but_interior_is_eligible(self):
        ref = np.full(GRID_BANDS, -60.0)
        var = np.full(GRID_BANDS, -54.0)
        ref[0] = -120.0
        var[1] = 12.0
        mask, reasons = _eligible_psd_mask(ref, var)
        self.assertFalse(mask[0])
        self.assertFalse(mask[1])
        self.assertTrue(np.all(mask[2:]))
        self.assertEqual(reasons["LOWER_CLAMP"], 1)
        self.assertEqual(reasons["UPPER_CLAMP"], 1)

    def test_large_error_cannot_remove_an_eligible_cell(self):
        reference = _synthetic_frame()
        variant = _synthetic_frame(gain_db=6.0, corrupt_band=7)
        result = _evaluate_gain_case(
            fixture_id="synthetic",
            reference_frames=[reference],
            variant_frames=[variant],
            gain_db=6.0,
            transformed_peak=0.5,
        )
        self.assertEqual(result["verdict"], "RED")
        self.assertEqual(result["frame_results"][0]["counts"]["psd"], 120)
        failures = [
            item
            for item in result["failures"]
            if item["reason"] == "GAIN_THRESHOLD_EXCEEDED"
        ]
        self.assertTrue(failures)
        self.assertTrue(
            any(item.get("max_abs_db", 0.0) >= 1.0 for item in failures)
        )

    def test_abbreviated_prominence_neighborhood_fails_closed(self):
        with self.assertRaises(FrontendConformanceError):
            _prominence_eligible_mask(
                np.ones(GRID_BANDS, dtype=bool),
                kernel_half=15,
            )

    def test_support_floor_is_inclusive_and_one_below_fails(self):
        self.assertEqual(
            _support_failures(
                psd_count=108,
                shape_count=108,
                delta_count=108,
                prominence_count=64,
            ),
            [],
        )
        self.assertEqual(
            _support_failures(
                psd_count=107,
                shape_count=108,
                delta_count=108,
                prominence_count=64,
            ),
            ["INSUFFICIENT_ELIGIBLE_SUPPORT"],
        )
        self.assertEqual(
            _support_failures(
                psd_count=108,
                shape_count=108,
                delta_count=108,
                prominence_count=63,
            ),
            ["INSUFFICIENT_ELIGIBLE_SUPPORT"],
        )

    def test_threshold_boundaries_are_exact(self):
        self.assertTrue(_strict_threshold(0.05, 0.05))
        self.assertFalse(
            _strict_threshold(float(np.nextafter(0.05, math.inf)), 0.05)
        )
        self.assertTrue(_alias_threshold(-80.0))
        self.assertFalse(_alias_threshold(float(np.nextafter(-80.0, math.inf))))

    def test_empty_or_all_invalid_sequences_cannot_pass(self):
        empty = _evaluate_gain_case(
            fixture_id="synthetic",
            reference_frames=[],
            variant_frames=[],
            gain_db=6.0,
            transformed_peak=0.5,
        )
        self.assertEqual(empty["verdict"], "RED")
        self.assertTrue(
            any(
                item["reason"] == "EMPTY_FRAME_SEQUENCE"
                for item in empty["failures"]
            )
        )

        reference = _synthetic_frame()
        variant = _synthetic_frame(gain_db=6.0)
        for frame in (reference, variant):
            frame["mid_valid"] = False
            frame["side_valid"] = False
            frame["valid"] = False
            frame["reason"] = "silence"
        invalid = _evaluate_gain_case(
            fixture_id="synthetic",
            reference_frames=[reference],
            variant_frames=[variant],
            gain_db=6.0,
            transformed_peak=0.5,
        )
        self.assertEqual(invalid["verdict"], "RED")
        self.assertTrue(
            any(
                item["reason"] == "REQUIRED_CHANNEL_INVALID"
                for item in invalid["failures"]
            )
        )

    def test_duplicate_and_reversed_frames_fail_closed(self):
        first = _synthetic_frame(frame_index=0, frame_end_sample=8192)
        failures = _frame_sequence_failures(
            [first, dict(first)], label="duplicate"
        )
        self.assertTrue(
            any(item["reason"] == "DUPLICATE_FRAME" for item in failures)
        )
        reversed_frame = _synthetic_frame(frame_index=0, frame_end_sample=7168)
        failures = _frame_sequence_failures(
            [first, reversed_frame], label="reversed"
        )
        self.assertTrue(
            any(item["reason"] == "FRAME_ORDER_INVALID" for item in failures)
        )

    def test_clipping_boundary_is_rejected(self):
        result = _evaluate_gain_case(
            fixture_id="synthetic",
            reference_frames=[_synthetic_frame()],
            variant_frames=[_synthetic_frame(gain_db=6.0)],
            gain_db=6.0,
            transformed_peak=1.0,
        )
        self.assertEqual(result["verdict"], "RED")
        self.assertTrue(
            any(
                item["reason"] == "CLIPPING_OR_FULL_SCALE"
                for item in result["failures"]
            )
        )

    def test_reference_variant_frame_mismatch_fails(self):
        result = _evaluate_gain_case(
            fixture_id="synthetic",
            reference_frames=[_synthetic_frame(frame_index=0)],
            variant_frames=[_synthetic_frame(frame_index=1, gain_db=6.0)],
            gain_db=6.0,
            transformed_peak=0.5,
        )
        self.assertEqual(result["verdict"], "RED")
        self.assertTrue(
            any(
                item["reason"] == "FRAME_ALIGNMENT_MISMATCH"
                for item in result["failures"]
            )
        )

    def test_nan_and_inf_fail_instead_of_becoming_exclusions(self):
        for nonfinite in (math.nan, math.inf):
            with self.subTest(nonfinite=nonfinite):
                reference = _synthetic_frame()
                variant = _synthetic_frame(gain_db=6.0)
                variant["mid_psd_db"][7] = nonfinite
                result = _evaluate_gain_case(
                    fixture_id="synthetic",
                    reference_frames=[reference],
                    variant_frames=[variant],
                    gain_db=6.0,
                    transformed_peak=0.5,
                )
                self.assertEqual(result["verdict"], "RED")
                self.assertTrue(
                    any(
                        item["reason"] == "NONFINITE_VALUE"
                        for item in result["failures"]
                    )
                )

    def test_cell_iteration_permutation_cannot_improve_the_max(self):
        errors = np.linspace(0.0, 0.119, GRID_BANDS, dtype=np.float64)
        eligible = np.ones(GRID_BANDS, dtype=bool)
        expected = float(_max_cell(errors, eligible)["max_abs_db"])
        permutation = np.random.Generator(np.random.PCG64(20260811)).permutation(
            GRID_BANDS
        )
        observed = float(
            _max_cell(errors[permutation], eligible[permutation])["max_abs_db"]
        )
        self.assertEqual(observed, expected)


class Gate6AndGate7RejectPathTests(unittest.TestCase):
    def test_mono_reference_must_keep_side_invalid(self):
        reference = _synthetic_frame()
        reference["side_valid"] = True
        failures = _mono_reference_validity_failures([reference])
        self.assertTrue(failures)
        self.assertEqual(failures[0]["reason"], "VALIDITY_MISMATCH")

    def test_gate6_rejects_mono_reference_with_valid_side(self):
        mono = _synthetic_frame(frame_end_sample=48_000)
        mono["side_valid"] = True
        dual = _synthetic_frame(frame_end_sample=48_000)
        side = _synthetic_frame(frame_end_sample=48_000)
        side["mid_valid"] = False
        side["side_valid"] = True
        for suffix in ("psd_db", "shape_db", "prominence_db", "delta_db"):
            side[f"side_{suffix}"] = list(side[f"mid_{suffix}"])
        side["side_level_dbfs"] = side["mid_level_dbfs"]
        decorrelated = _synthetic_frame(frame_end_sample=48_000)
        decorrelated["side_valid"] = True
        with mock.patch(
            "ml_v3.benchmark.frontend_conformance.extract_offline_feature_frames",
            side_effect=[[mono], [dual], [side], [decorrelated]],
        ):
            result = _run_gate6()
        self.assertEqual(result["verdict"], "RED")
        self.assertEqual(result["cases"]["mono_vs_dual_mono"]["verdict"], "RED")
        self.assertEqual(result["cases"]["side_only_vs_mono"]["verdict"], "RED")

    def test_invalid_reference_channel_cannot_pass_ms_equivalence(self):
        reference = _synthetic_frame()
        candidate = _synthetic_frame()
        reference["mid_valid"] = False
        reference["valid"] = False
        reference["reason"] = "silence"
        result = _channel_equivalence(
            [reference],
            [candidate],
            reference_channel="mid",
            candidate_channel="mid",
        )
        self.assertEqual(result["verdict"], "RED")
        self.assertTrue(
            any(
                item["reason"] == "VALIDITY_MISMATCH"
                for item in result["failures"]
            )
        )

    def test_wrong_full_gate_platform_fails_closed(self):
        with mock.patch(
            "ml_v3.benchmark.frontend_conformance.platform.machine",
            return_value="not-the-frozen-arch",
        ):
            with self.assertRaises(FrontendConformanceError):
                _require_gate_platform()

    def test_20khz_boundary_cannot_hide_a_component_above_threshold(self):
        amplitude = 10.0 ** (-79.997 / 20.0)
        index = np.arange(65_536, dtype=np.float64)
        segment = amplitude * np.sin(
            2.0 * math.pi * 20_000.0 * index / 48_000.0
        )
        measured = _measure_hann_peak(
            segment,
            sample_rate=48_000,
            reference_amplitude=1.0,
        )
        peak_db = float(measured["conservative_peak_db"])
        self.assertGreater(peak_db, -80.0)
        self.assertFalse(_alias_threshold(peak_db))


class FrontendConformanceIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run_frontend_conformance()

    def test_all_three_gates_are_green(self):
        self.assertEqual(self.result["verdict"], "GREEN")
        self.assertEqual(self.result["gates"]["gate5"]["verdict"], "GREEN")
        self.assertEqual(self.result["gates"]["gate6"]["verdict"], "GREEN")
        self.assertEqual(self.result["gates"]["gate7"]["verdict"], "GREEN")

    def test_old_full_grid_comparison_is_explicitly_non_closing_and_fails(self):
        diagnostics = []
        for case in self.result["gates"]["gate5"]["cases"]:
            for frame in case["frame_results"]:
                diagnostic = frame["full_grid_diagnostic"]
                self.assertFalse(diagnostic["closing"])
                diagnostics.extend(
                    float(field["max_abs_db"])
                    for field in diagnostic["fields"].values()
                )
        self.assertGreater(max(diagnostics), 0.05)

    def test_gate5_support_and_clipping_guards_hold_for_official_cases(self):
        for case in self.result["gates"]["gate5"]["cases"]:
            self.assertEqual(case["closing_frame_count"], 78)
            self.assertLess(case["transformed_peak"], 1.0)
            self.assertFalse(case["failures"])
            for frame in case["frame_results"]:
                counts = frame["counts"]
                self.assertGreaterEqual(counts["psd"], 108)
                self.assertGreaterEqual(counts["shape"], 108)
                self.assertGreaterEqual(counts["delta"], 108)
                self.assertGreaterEqual(counts["prominence"], 64)

    def test_gate6_flags_and_equivalence_are_green(self):
        gate = self.result["gates"]["gate6"]
        self.assertEqual(gate["cases"]["mono_vs_dual_mono"]["verdict"], "GREEN")
        self.assertEqual(gate["cases"]["side_only_vs_mono"]["verdict"], "GREEN")
        self.assertEqual(gate["cases"]["decorrelated_stereo"]["verdict"], "GREEN")
        self.assertEqual(gate["cases"]["decorrelated_stereo"]["frame_count"], 78)

    def test_gate7_publishes_observed_peak_with_margin(self):
        gate = self.result["gates"]["gate7"]
        self.assertEqual(gate["analysis_samples"], 65_536)
        self.assertEqual(gate["fft_samples"], 65_536 * 16)
        self.assertGreaterEqual(gate["peak_frequency_hz"], 20.0)
        self.assertLessEqual(gate["peak_frequency_hz"], 20_000.0)
        self.assertLessEqual(gate["peak_db_re_single_input_tone"], -80.0)
        self.assertGreaterEqual(
            gate["useful_start_source_time_seconds"],
            0.2573333333333333,
        )

    def test_lab_symbols_are_not_exported_by_dispatcher_or_contracts(self):
        self.assertNotIn("run_frontend_conformance", benchmark_package.__all__)
        self.assertNotIn("run_frontend_conformance", contracts_package.__all__)
        self.assertFalse(hasattr(contracts_package, "run_frontend_conformance"))


if __name__ == "__main__":
    unittest.main()
