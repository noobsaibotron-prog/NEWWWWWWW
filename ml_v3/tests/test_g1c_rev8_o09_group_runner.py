"""Tests for the isolated REV8 O-09 group benchmark runner."""
from __future__ import annotations

from fractions import Fraction
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from ml_v3.benchmark import rev8_o09_group_isolated_bootstrap as bootstrap
from ml_v3.benchmark import run_rev8_o09_group_candidate as runner
from ml_v3.benchmark.rev8_o09_group_candidate import (
    GROUP_CANDIDATE_BALLOT_READY,
    GROUP_CANDIDATE_LIMITATIONS,
    GroupReason,
    GroupResult,
    GroupStatus,
)


class WorkloadTests(unittest.TestCase):
    def test_smoke_is_strict_subset_of_full(self):
        self.assertTrue(set(runner.SMOKE_WORKLOADS) < set(runner.FULL_WORKLOADS))
        self.assertEqual(
            runner._workloads("smoke"), runner.SMOKE_WORKLOADS
        )
        self.assertEqual(runner._workloads("full"), runner.FULL_WORKLOADS)
        with self.assertRaises(ValueError):
            runner._workloads("unknown")

    def test_ap_workloads_certify_and_preserve_atomic_ties(self):
        distinct, _ = runner._measure_call("ap_distinct_diagonal", 8)
        atomic, _ = runner._measure_call("ap_atomic_dense", 8)
        self.assertEqual(distinct.status, GroupStatus.CERTIFIED)
        self.assertEqual(distinct.value.ap, Fraction(1))
        self.assertEqual(len(distinct.value.prefixes), 8)
        self.assertEqual(atomic.status, GroupStatus.CERTIFIED)
        self.assertEqual(atomic.value.ap, Fraction(1))
        self.assertEqual(len(atomic.value.prefixes), 1)
        self.assertEqual(atomic.value.prefixes[0].true_positive, 8)

    def test_coverage_unique_and_ambiguous_paths(self):
        unique, _ = runner._measure_call("coverage_unique", 8)
        ambiguous, _ = runner._measure_call("coverage_ambiguous", 8)
        self.assertEqual(unique.status, GroupStatus.CERTIFIED)
        self.assertEqual(
            unique.value.coverage_minus_group64,
            unique.value.coverage_plus_group64,
        )
        self.assertEqual(ambiguous.status, GroupStatus.CERTIFIED)
        unit = ambiguous.value.units[0]
        self.assertEqual(unit.coverage_minus, Fraction(0))
        self.assertEqual(unit.coverage_plus, Fraction(1))

    def test_spearman_workload_outcomes_are_distinct(self):
        unique, _ = runner._measure_call("spearman_unique", 10)
        fixed, _ = runner._measure_call("spearman_fixed_marginal", 10)
        ambiguous, _ = runner._measure_call("spearman_ambiguous", 10)
        variable, _ = runner._measure_call(
            "spearman_variable_unavailable", 10
        )
        self.assertEqual(unique.status, GroupStatus.CERTIFIED)
        self.assertEqual(unique.value.certificate, "UNIQUE_OPTIMUM")
        self.assertEqual(fixed.status, GroupStatus.CERTIFIED)
        self.assertEqual(
            fixed.value.certificate, "FIXED_MARGINAL_RHO_SINGLETON"
        )
        self.assertEqual(ambiguous.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(ambiguous.reason, GroupReason.PAIRING_AMBIGUOUS)
        self.assertEqual(variable.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(
            variable.reason,
            GroupReason.SPEARMAN_CERTIFICATE_UNAVAILABLE,
        )
        self.assertFalse(variable.witness["ballot_ready"])

    def test_macro_workloads_use_thirty_groups(self):
        ap, _ = runner._measure_call("ap_macro_30", 1)
        coverage, _ = runner._measure_call("coverage_macro_30", 1)
        self.assertEqual(ap.defined_group_count, 30)
        self.assertEqual(coverage.defined_group_count, 30)
        self.assertEqual(coverage.groups_with_na_units, ())

    def test_cap_boundary_workload_contains_accept_and_reject_cases(self):
        result, timing = runner._measure_call("cap_boundaries", 0)
        self.assertGreaterEqual(timing["wall_seconds"], 0.0)
        self.assertGreaterEqual(timing["cpu_seconds"], 0.0)
        by_surface: dict[str, list[dict[str, object]]] = {}
        for row in result["cases"]:
            by_surface.setdefault(row["surface"], []).append(row)
        for surface in (
            "AP_PARTITIONS",
            "COVERAGE_UNITS",
            "SPEARMAN_PARTITIONS",
            "EXACT_SCALAR_BITS",
        ):
            self.assertIn(surface, by_surface)
            statuses = {row["status"] for row in by_surface[surface]}
            self.assertIn(GroupStatus.REJECTED.value, statuses)
        exact = {
            row["value"]: row
            for row in by_surface["EXACT_SCALAR_BITS"]
        }
        self.assertNotEqual(exact[65_535]["status"], "REJECTED")
        self.assertNotEqual(exact[65_536]["status"], "REJECTED")
        self.assertEqual(exact[65_537]["status"], "REJECTED")

    def test_cap_boundary_evidence_preserves_absent_preflight(self):
        """A preflight failure must remain reportable without a fake probe."""
        failure = GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            None,
            witness="TimeoutError",
        )
        with (
            mock.patch.object(
                runner, "evaluate_group_ap", return_value=failure),
            mock.patch.object(
                runner, "evaluate_group_coverage", return_value=failure),
            mock.patch.object(
                runner, "evaluate_group_spearman", return_value=failure),
        ):
            result = runner._measure_cap_boundaries()
        self.assertTrue(result["cases"])
        for row in result["cases"]:
            self.assertEqual(row["status"], GroupStatus.REJECTED.value)
            self.assertEqual(
                row["reason"], GroupReason.SOLVER_RUNTIME_FAILURE.value)
            self.assertIsNone(row["exceeded"])


class EvidenceEncodingTests(unittest.TestCase):
    def test_exact_values_are_serialized_without_decimal_bigints(self):
        encoded = runner._scientific(
            {
                "fraction": Fraction(-3, 7),
                "binary64": 0.5,
                "status": GroupStatus.CERTIFIED,
            }
        )
        self.assertEqual(encoded["fraction"], ["-0x3", "0x7"])
        self.assertEqual(
            encoded["binary64"], {"binary64_hex": "0x1.0000000000000p-1"}
        )
        self.assertEqual(encoded["status"], "CERTIFIED")

    def test_group_result_dataclass_is_serializable(self):
        ap, _ = runner._measure_call("ap_atomic_dense", 2)
        encoded = runner._scientific(ap)
        self.assertEqual(encoded["status"], "CERTIFIED")
        self.assertEqual(encoded["value"]["ap"], ["0x1", "0x1"])

    def test_output_must_be_external_to_repository(self):
        with self.assertRaises(RuntimeError):
            runner._external_output_path(runner.ROOT / "evidence.json")
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "evidence.json"
            self.assertEqual(
                runner._external_output_path(target), target.resolve()
            )


class ProvenanceAndGovernanceTests(unittest.TestCase):
    def test_bootstrap_targets_only_the_group_runner(self):
        self.assertEqual(
            bootstrap.RUNNER_MODULE,
            "ml_v3.benchmark.run_rev8_o09_group_candidate",
        )

    def test_provenance_includes_every_new_group_subject(self):
        required = {
            "ml_v3/benchmark/rev8_o09_group_candidate.py",
            "ml_v3/benchmark/rev8_o09_group_isolated_bootstrap.py",
            "ml_v3/benchmark/run_rev8_o09_group_candidate.py",
            "ml_v3/tests/test_g1c_rev8_o09_group_candidate.py",
            "ml_v3/tests/test_g1c_rev8_o09_group_runner.py",
        }
        self.assertTrue(required.issubset(set(runner.HASHED_PATHS)))
        self.assertEqual(len(runner.HASHED_PATHS), len(set(runner.HASHED_PATHS)))

    def test_runner_cannot_claim_ballot_readiness(self):
        self.assertFalse(GROUP_CANDIDATE_BALLOT_READY)
        self.assertEqual(runner.AUTHORITY_STATUS, (
            "EVIDENCE_ONLY_GROUP_CAPS_NOT_ACTIVE"
        ))
        self.assertIn(
            "GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED",
            GROUP_CANDIDATE_LIMITATIONS,
        )
        self.assertIn(
            "SPEARMAN_RHO64_PUBLICATION_NOT_MATERIALIZED",
            GROUP_CANDIDATE_LIMITATIONS,
        )

    def test_scientific_dataclass_reason_is_not_lost(self):
        ap, _ = runner._measure_call("ap_distinct_diagonal", 2)
        rejected = GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            ap.preflight,
            witness="RuntimeError",
        )
        encoded = runner._scientific(rejected)
        self.assertEqual(encoded["reason"], "SOLVER_RUNTIME_FAILURE")
        self.assertEqual(encoded["witness"], "RuntimeError")


if __name__ == "__main__":
    unittest.main()
