from __future__ import annotations

import json
import unittest

from ml_v3.prep.corpus_power_reality import (
    CONTRACT_FLOOR,
    PowerRealityError,
    canonical_json,
    exact_binomial_cdf,
    feasible_thresholds,
    first_clean_actionable_n,
    sensitivity_report,
)


class CorpusPowerRealityTest(unittest.TestCase):
    def test_exact_cdf_has_expected_small_value(self) -> None:
        # Bin(2, 1/50), P(X<=0) = (49/50)^2.
        tail = exact_binomial_cdf(2, 0, 50)
        self.assertEqual(tail.numerator, 49**2)
        self.assertEqual(tail.denominator, 50**2)

    def test_contract_floor_is_enforced(self) -> None:
        with self.assertRaisesRegex(PowerRealityError, "contractual floor"):
            first_clean_actionable_n(1, start_n=CONTRACT_FLOOR - 1)

    def test_invalid_or_duplicate_multiplicity_is_rejected(self) -> None:
        for value in (0, -1, True, 1.0):
            with self.subTest(value=value):
                with self.assertRaises(PowerRealityError):
                    sensitivity_report([value])  # type: ignore[list-item]
        with self.assertRaisesRegex(PowerRealityError, "unique"):
            sensitivity_report([4, 4])

    def test_exact_first_n_for_m1_and_boundary_predecessor(self) -> None:
        result = first_clean_actionable_n(1, start_n=1269, max_n=1271)
        self.assertEqual((result.n, result.k), (1271, 17))
        self.assertEqual(feasible_thresholds(1270, 1), ())
        self.assertTrue(feasible_thresholds(1271, 1))

    def test_exact_boundaries_for_remaining_sensitivity_rows(self) -> None:
        expected = {4: (1870, 24), 6: (2043, 26), 8: (2140, 27), 12: (2313, 29)}
        for m, (n, k) in expected.items():
            with self.subTest(m=m):
                self.assertEqual(feasible_thresholds(n - 1, m), ())
                result = feasible_thresholds(n, m)
                self.assertTrue(result)
                self.assertEqual((result[-1].n, result[-1].k), (n, k))

    def test_report_is_canonical_and_explicitly_non_authoritative(self) -> None:
        report = sensitivity_report([1], start_n=1271, max_n=1271)
        encoded = canonical_json(report)
        self.assertEqual(encoded, canonical_json(json.loads(encoded)))
        self.assertIn("diagnostic-only", report["authority"])
        self.assertNotIn("plan_id", report)
        self.assertNotIn("pilot_sha256", report)
        row = report["rows"][0]
        self.assertEqual(row["comparison_arithmetic"], "exact integer cross-multiplication")
        self.assertNotIn("size_exact", row)
        self.assertNotIn("power_exact", row)

    def test_default_search_result_serializes_without_bigint_conversion(self) -> None:
        result = first_clean_actionable_n(12, start_n=2313, max_n=2313)
        encoded = canonical_json(
            {
                "schema": "test-only",
                "row": result.as_dict(),
            }
        )
        self.assertIn('"n_power_diagnostic":2313', encoded)

    def test_no_solution_in_range_is_fail_closed(self) -> None:
        with self.assertRaisesRegex(PowerRealityError, "no feasible n"):
            first_clean_actionable_n(12, start_n=2300, max_n=2300)


if __name__ == "__main__":
    unittest.main()
