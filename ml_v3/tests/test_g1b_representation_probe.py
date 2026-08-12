"""Falsification tests for the model-free V3 representation probe."""
from __future__ import annotations

import unittest
from pathlib import Path

import numpy as np

import ml_v3.benchmark as benchmark_package
import ml_v3.contracts as contracts_package
from ml_v3.benchmark.representation_probe import (
    _base_spectrum,
    run_representation_probe,
)
from ml_v3.contracts.canonical import canonical_bytes


_REPORT = (
    Path(__file__).resolve().parents[1]
    / "reports"
    / "V3_REPRESENTATION_PROBE_V1.json"
)


class RepresentationProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run_representation_probe()

    def test_base_spectrum_is_byte_deterministic(self):
        spectrum_a, frequencies_a = _base_spectrum()
        spectrum_b, frequencies_b = _base_spectrum()
        self.assertEqual(spectrum_a.tobytes(), spectrum_b.tobytes())
        self.assertEqual(frequencies_a.tobytes(), frequencies_b.tobytes())

    def test_canonical_report_is_byte_identical_to_committed_evidence(self):
        self.assertTrue(_REPORT.is_file())
        self.assertEqual(canonical_bytes(self.result), _REPORT.read_bytes())

    def test_all_preregistered_probe_sections_are_green(self):
        self.assertEqual(self.result["verdict"], "GREEN")
        for section in self.result["results"].values():
            self.assertEqual(section["verdict"], "GREEN")

    def test_peak_response_is_positive_monotonic_and_localized(self):
        cases = {
            float(case["center_hz"]): case
            for case in self.result["results"]["narrow_peaks"]["cases"]
        }
        for case in cases.values():
            self.assertTrue(case["strictly_monotonic"])
            self.assertEqual(case["verdict"], "GREEN")
            responses = [gain["target_response_db"] for gain in case["gains"]]
            self.assertTrue(all(value > 0.0 for value in responses))
            self.assertTrue(
                all(later > earlier for earlier, later in zip(responses, responses[1:]))
            )
            self.assertTrue(all(gain["distance_bands"] <= 2 for gain in case["gains"]))

        # 250 Hz is not an exact 1024-rFFT bin. The evidence must publish both
        # nominal and realized geometry rather than silently pretending it is.
        self.assertEqual(cases[250.0]["target_band"], 44)
        self.assertTrue(
            all(
                gain["realized_peak_bin_hz"] == 234.375
                for gain in cases[250.0]["gains"]
            )
        )

    def test_broad_shape_correlations_clear_the_preregistered_floor(self):
        for case in self.result["results"]["broad_colorations"]["cases"]:
            self.assertGreaterEqual(case["shape_envelope_pearson"], 0.80)
            self.assertEqual(case["verdict"], "GREEN")

    def test_thin_and_dull_shelves_pass_on_observable_cells(self):
        cases = {
            case["case_id"]: case
            for case in self.result["results"]["thin_dull_primitives"]["cases"]
        }
        for case in cases.values():
            self.assertGreaterEqual(case["shape_envelope_pearson"], 0.80)
            self.assertTrue(case["sign_correct_all_affected_bands"])
            self.assertGreater(case["affected_band_count"], 0)
            self.assertEqual(case["verdict"], "GREEN")
        self.assertTrue(cases["thin_low_shelf"]["excluded_cells"])

    def test_low_end_debt_is_explicit_complete_and_non_closing(self):
        diagnostic = self.result["diagnostics"]["low_end_support_debt"]
        self.assertEqual(diagnostic["status"], "RECORDED_NON_CLOSING_DEBT")
        self.assertFalse(diagnostic["closing"])
        self.assertFalse(diagnostic["affects_probe_verdict"])
        self.assertEqual(
            diagnostic["structurally_unobservable_band_indices"],
            [0, 1, 4, 5, 8, 11],
        )
        classified = set(diagnostic["structurally_unobservable_band_indices"])
        classified.update(diagnostic["probe_inactive_nonstructural_band_indices"])
        self.assertEqual(classified, set(diagnostic["base_excluded_band_indices"]))

    def test_temporal_response_is_causal_localized_and_settles(self):
        result = self.result["results"]["temporal"]
        self.assertTrue(result["no_anticipated_response"])
        self.assertTrue(result["first_response_within_one_hop"])
        self.assertGreaterEqual(result["onset_envelope_pearson"], 0.80)
        self.assertTrue(result["localized_within_two_bands"])
        self.assertIsNotNone(result["onset_settled_frame_end_sample"])
        self.assertIsNotNone(result["offset_settled_frame_end_sample"])
        self.assertLessEqual(
            result["onset_settled_frame_end_sample"],
            result["onset_stabilization_sample"] + 8 * 1024,
        )
        self.assertLessEqual(
            result["offset_settled_frame_end_sample"],
            result["offset_stabilization_sample"] + 8 * 1024,
        )
        self.assertEqual(result["verdict"], "GREEN")

    def test_probe_is_lab_only_and_not_exported(self):
        self.assertNotIn("run_representation_probe", benchmark_package.__all__)
        self.assertNotIn("run_representation_probe", contracts_package.__all__)
        self.assertFalse(hasattr(contracts_package, "run_representation_probe"))


if __name__ == "__main__":
    unittest.main()
