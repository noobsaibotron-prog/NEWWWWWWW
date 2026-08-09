"""REV8 O-13 support-floor policy compiler falsification tests."""
from __future__ import annotations

import copy
import unittest

import ml_v3.contracts as contracts_pkg
from ml_v3.contracts.canonical import sha256_of_obj
from ml_v3.contracts.support_floor_policy_v2 import (
    COMPILER_AUTHORITY_STATUS,
    SupportFloorPolicyCompilationError,
    SupportFloorPolicyTemplate,
    compile_support_floor_policy,
)
from ml_v3.contracts.support_floor_v2 import (
    POLICY_SCHEMA,
    StratumPopulation,
    SupportFloorError,
    evaluate_support_floors,
    stratum_population_plan_sha256,
    support_floor_policy_sha256,
)
from ml_v3.contracts.support_v2 import UnitOutcome, account_support
from ml_v3.tests._g1a_t2_fixtures import benchmark_power_plan


class SupportFloorPolicyCompilerTests(unittest.TestCase):
    @staticmethod
    def _groups(count: int) -> tuple[str, ...]:
        return tuple(f"g{index:03}" for index in range(count))

    @classmethod
    def _population(
        cls, count: int, stratum_id: str = "overall"
    ) -> tuple[StratumPopulation, ...]:
        return (StratumPopulation(stratum_id, cls._groups(count)),)

    @staticmethod
    def _population_sha(
        metric_id: str,
        populations: tuple[StratumPopulation, ...],
        split_role: str = "final-test",
    ) -> str:
        return stratum_population_plan_sha256(
            metric_id, split_role, populations
        )

    def test_unpowered_policy_compiles_without_power_artifact(self):
        metric_id = "average_precision:Resonance"
        populations = self._population(30)
        compiled = compile_support_floor_policy(
            (SupportFloorPolicyTemplate(
                "overall", "all_eligible_groups", None, 30
            ),),
            populations,
            metric_id=metric_id,
            split_role="final-test",
            mandatory=True,
            expected_population_plan_sha256=self._population_sha(
                metric_id, populations
            ),
        )
        self.assertEqual(compiled.policy["schema"], POLICY_SCHEMA)
        self.assertIsNone(compiled.power_plan_sha256)
        self.assertEqual(compiled.policy["strata"][0]["n_required"], 30)
        self.assertEqual(
            support_floor_policy_sha256(compiled.policy),
            compiled.policy_sha256,
        )
        self.assertEqual(compiled.authority_status, COMPILER_AUTHORITY_STATUS)

    def test_gate_binding_uses_validated_n_power_and_provenance(self):
        metric_id = "curve_err_rel"
        populations = self._population(40)
        power_plan = benchmark_power_plan()
        power_sha = sha256_of_obj(power_plan)
        compiled = compile_support_floor_policy(
            (SupportFloorPolicyTemplate(
                "overall",
                "all_eligible_groups",
                None,
                30,
                "gate",
                metric_id,
            ),),
            populations,
            metric_id=metric_id,
            split_role="final-test",
            mandatory=True,
            expected_population_plan_sha256=self._population_sha(
                metric_id, populations
            ),
            benchmark_power_plan=power_plan,
            expected_power_plan_sha256=power_sha,
        )
        row = compiled.policy["strata"][0]
        self.assertEqual(row["power_binding_kind"], "gate")
        self.assertEqual(row["power_binding_id"], metric_id)
        self.assertEqual(row["n_power"], 40)
        self.assertEqual(row["n_required"], 40)
        self.assertEqual(compiled.power_plan_sha256, power_sha)

        eligible = tuple(
            (group_id, ("unit", group_id))
            for group_id in populations[0].group_ids
        )
        support = account_support(
            eligible,
            tuple(
                UnitOutcome(group_id, unit_key, True)
                for group_id, unit_key in eligible
            ),
        )
        evaluation = evaluate_support_floors(
            compiled.policy,
            compiled.policy_sha256,
            support,
            populations,
            metric_id=metric_id,
            split_role="final-test",
            power_plan_sha256=power_sha,
        )
        self.assertTrue(evaluation.support_sufficient)
        self.assertEqual(evaluation.strata[0].power_binding_kind, "gate")
        self.assertEqual(evaluation.strata[0].power_binding_id, metric_id)

    def test_family_binding_checks_family_floor(self):
        metric_id = "clean_safety_pool"
        populations = self._population(160)
        power_plan = benchmark_power_plan()
        compiled = compile_support_floor_policy(
            (SupportFloorPolicyTemplate(
                "overall",
                "all_eligible_groups",
                None,
                149,
                "family",
                "clean-safety",
            ),),
            populations,
            metric_id=metric_id,
            split_role="final-test",
            mandatory=True,
            expected_population_plan_sha256=self._population_sha(
                metric_id, populations
            ),
            benchmark_power_plan=power_plan,
            expected_power_plan_sha256=sha256_of_obj(power_plan),
        )
        row = compiled.policy["strata"][0]
        self.assertEqual(row["n_power"], 160)
        self.assertEqual(row["n_required"], 160)

    def test_population_digest_is_external_and_must_match(self):
        metric_id = "average_precision:Resonance"
        populations = self._population(30)
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError,
            "POPULATION_PLAN_HASH_MISMATCH",
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256="0" * 64,
            )

    def test_population_strata_must_match_template_exactly(self):
        metric_id = "average_precision:Resonance"
        populations = self._population(30, "different")
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError,
            "POPULATION_PLAN_TEMPLATE_MISMATCH",
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
            )

    def test_child_population_must_be_subset_of_parent(self):
        metric_id = "average_precision:Resonance"
        populations = (
            StratumPopulation("overall", ("a", "b")),
            StratumPopulation("profile:bass", ("outside",)),
        )
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "not a subset"
        ):
            compile_support_floor_policy(
                (
                    SupportFloorPolicyTemplate(
                        "overall", "all_eligible_groups", None, 2
                    ),
                    SupportFloorPolicyTemplate(
                        "profile:bass", "profile_groups", "overall", 1
                    ),
                ),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
            )

    def test_required_power_plan_cannot_be_missing_or_substituted(self):
        metric_id = "curve_err_rel"
        populations = self._population(40)
        template = (SupportFloorPolicyTemplate(
            "overall", "all_eligible_groups", None, 30, "gate", metric_id
        ),)
        kwargs = {
            "metric_id": metric_id,
            "split_role": "final-test",
            "mandatory": True,
            "expected_population_plan_sha256": self._population_sha(
                metric_id, populations
            ),
        }
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "unavailable"
        ):
            compile_support_floor_policy(template, populations, **kwargs)

        power_plan = benchmark_power_plan()
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "POWER_PLAN_HASH_MISMATCH"
        ):
            compile_support_floor_policy(
                template,
                populations,
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256="0" * 64,
                **kwargs,
            )

    def test_unpowered_policy_rejects_unrelated_power_plan(self):
        metric_id = "average_precision:Resonance"
        populations = self._population(30)
        power_plan = benchmark_power_plan()
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "unrelated power plan"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_power_plan_must_be_frozen(self):
        metric_id = "curve_err_rel"
        populations = self._population(40)
        power_plan = benchmark_power_plan()
        power_plan["result"] = {"status": "draft"}
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "must be frozen"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30,
                    "gate", metric_id,
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_duplicate_gate_binding_is_ambiguous(self):
        metric_id = "curve_err_rel"
        populations = self._population(40)
        power_plan = benchmark_power_plan()
        power_plan["gates"][1]["metric_id"] = metric_id
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "duplicate power gate"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30,
                    "gate", metric_id,
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_bound_n_required_must_match_contract_floor(self):
        metric_id = "curve_err_rel"
        populations = self._population(40)
        power_plan = benchmark_power_plan()
        power_plan["gates"][0]["n_required"] = 41
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "n_required"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30,
                    "gate", metric_id,
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_family_contract_floor_mismatch_is_fatal(self):
        metric_id = "clean_safety_pool"
        populations = self._population(160)
        power_plan = benchmark_power_plan()
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "contractual floor"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 150,
                    "family", "clean-safety",
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_missing_power_binding_id_is_fatal(self):
        metric_id = "missing_metric"
        populations = self._population(40)
        power_plan = benchmark_power_plan()
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "missing gate power binding"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30,
                    "gate", metric_id,
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_gate_binding_cannot_borrow_another_metric_power(self):
        metric_id = "different_metric"
        populations = self._population(40)
        power_plan = benchmark_power_plan()
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "must equal policy metric_id"
        ):
            compile_support_floor_policy(
                (SupportFloorPolicyTemplate(
                    "overall", "all_eligible_groups", None, 30,
                    "gate", "curve_err_rel",
                ),),
                populations,
                metric_id=metric_id,
                split_role="final-test",
                mandatory=True,
                expected_population_plan_sha256=self._population_sha(
                    metric_id, populations
                ),
                benchmark_power_plan=power_plan,
                expected_power_plan_sha256=sha256_of_obj(power_plan),
            )

    def test_template_and_population_order_are_not_authority(self):
        metric_id = "average_precision:Resonance"
        all_groups = self._groups(30)
        child_groups = all_groups[:5]
        root = SupportFloorPolicyTemplate(
            "overall", "all_eligible_groups", None, 30
        )
        child = SupportFloorPolicyTemplate(
            "source:a", "source_family_groups", "overall", 5,
            max_parent_fraction_numerator=1,
            max_parent_fraction_denominator=2,
        )
        populations = (
            StratumPopulation("overall", all_groups),
            StratumPopulation("source:a", child_groups),
        )
        population_sha = self._population_sha(metric_id, populations)
        expected = compile_support_floor_policy(
            (root, child),
            populations,
            metric_id=metric_id,
            split_role="final-test",
            mandatory=True,
            expected_population_plan_sha256=population_sha,
        )
        actual = compile_support_floor_policy(
            (child, root),
            tuple(reversed(populations)),
            metric_id=metric_id,
            split_role="final-test",
            mandatory=True,
            expected_population_plan_sha256=population_sha,
        )
        self.assertEqual(actual, expected)

    def test_binding_fields_are_paired(self):
        with self.assertRaisesRegex(
            SupportFloorPolicyCompilationError, "both present or both absent"
        ):
            SupportFloorPolicyTemplate(
                "overall", "all_eligible_groups", None, 30,
                power_binding_kind="gate",
            )

    def test_v1_policy_without_binding_provenance_is_rejected(self):
        metric_id = "average_precision:Resonance"
        populations = self._population(30)
        compiled = compile_support_floor_policy(
            (SupportFloorPolicyTemplate(
                "overall", "all_eligible_groups", None, 30
            ),),
            populations,
            metric_id=metric_id,
            split_role="final-test",
            mandatory=True,
            expected_population_plan_sha256=self._population_sha(
                metric_id, populations
            ),
        )
        stale = copy.deepcopy(compiled.policy)
        stale["schema"] = "aieq-v3-rev8-o13-support-floor-policy-1"
        del stale["strata"][0]["power_binding_kind"]
        del stale["strata"][0]["power_binding_id"]
        with self.assertRaisesRegex(SupportFloorError, "unexpected.*schema"):
            support_floor_policy_sha256(stale)

    def test_module_is_unreachable_from_rev7_dispatcher(self):
        for symbol in (
            "compile_support_floor_policy",
            "SupportFloorPolicyTemplate",
            "CompiledSupportFloorPolicy",
        ):
            self.assertFalse(hasattr(contracts_pkg, symbol))


if __name__ == "__main__":
    unittest.main()
