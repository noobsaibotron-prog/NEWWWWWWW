"""Falsification tests for the isolated REV8 O-09 group candidate.

These tests exercise only the candidate benchmark surface.  They do not
activate REV8 and do not make the provisional group caps normative.
"""
from __future__ import annotations

from fractions import Fraction
import itertools
import unittest
from unittest import mock

from ml_v3.benchmark import rev8_o09_group_candidate as subject
from ml_v3.benchmark import rev8_o09_candidate as per_subgraph
from ml_v3.benchmark.rev8_o09_candidate import (
    CandidateGraph,
    ExactEdge,
    a1_exhaustive,
)
from ml_v3.benchmark.rev8_o09_group_candidate import (
    APPartition,
    APValue,
    CoveragePartition,
    GroupCandidateError,
    GroupReason,
    GroupStatus,
    RhoIdentity,
    SpearmanAmbiguityWitness,
    SpearmanPartition,
    evaluate_group_ap,
    evaluate_group_coverage,
    evaluate_group_spearman,
    reduce_macro_average_precision_math,
    reduce_macro_coverage_math,
    rho_equal,
)
from ml_v3.contracts.numeric_authority_v2 import exact_n64, mean64, rn64


def edge(
    gt: int,
    prediction: int,
    *,
    k2: Fraction = Fraction(1),
    k3: int = 0,
    k4: Fraction = Fraction(0),
    scientific: bytes | None = None,
    diagnostic: bytes | None = None,
) -> ExactEdge:
    return ExactEdge(
        gt=gt,
        prediction=prediction,
        k2_iou=k2,
        k3_tick_error=k3,
        k4_cost=k4,
        scientific_key=scientific or f"s:{gt}:{prediction}".encode(),
        diagnostic_key=diagnostic or f"d:{gt}:{prediction}".encode(),
    )


def graph(
    gt_count: int,
    prediction_count: int,
    edges: tuple[ExactEdge, ...] | list[ExactEdge],
) -> CandidateGraph:
    return CandidateGraph(
        gt_count,
        prediction_count,
        "additive",
        tuple(edges),
    )


def diagonal_graph(size: int) -> CandidateGraph:
    return graph(size, size, [edge(index, index) for index in range(size)])


def confidence(value: Fraction) -> Fraction:
    """Canonical exact rational represented by the rounded binary64 value."""
    return exact_n64(rn64(value))


class AveragePrecisionTests(unittest.TestCase):
    def test_equal_confidence_predictions_enter_atomically(self):
        result = evaluate_group_ap((
            APPartition(
                ("ap",),
                diagonal_graph(2),
                (Fraction(1, 2), Fraction(1, 2)),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(result.value.ap, Fraction(1))
        self.assertEqual(result.value.ap_group64, rn64(Fraction(1)))
        self.assertEqual(len(result.value.prefixes), 1)
        self.assertEqual(result.value.prefixes[0].true_positive, 2)

    def test_interleaved_partition_thresholds_have_exact_ap(self):
        result = evaluate_group_ap((
            APPartition(
                ("ap-a",),
                graph(1, 2, [edge(0, 1)]),
                (confidence(Fraction(9, 10)), Fraction(1, 2)),
            ),
            APPartition(
                ("ap-b",),
                graph(1, 1, [edge(0, 0)]),
                (confidence(Fraction(7, 10)),),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(result.value.ap, Fraction(7, 12))
        self.assertEqual(
            tuple(prefix.true_positive for prefix in result.value.prefixes),
            (0, 1, 2),
        )
        self.assertEqual(result.preflight.distinct_thresholds, 3)
        self.assertEqual(result.preflight.local_thresholds, 3)
        self.assertEqual(
            result.preflight.maximum_cardinality_solve_bound,
            3,
        )
        self.assertEqual(result.preflight.profile_solve_bound, 0)

    def test_no_gt_is_not_applicable(self):
        result = evaluate_group_ap((
            APPartition(("ap",), graph(0, 1, []), (Fraction(1, 2),)),
        ))
        self.assertEqual(result.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(result.reason, GroupReason.NO_GT)

    def test_gt_with_no_predictions_has_zero_ap(self):
        result = evaluate_group_ap((
            APPartition(("ap",), graph(2, 0, []), ()),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(result.value.ap, Fraction(0))
        self.assertEqual(result.value.prefixes, ())

    def test_partition_cap_is_fail_closed(self):
        partitions = tuple(
            APPartition((f"ap-{index}",), graph(0, 0, []), ())
            for index in range(subject.AP_MAX_PARTITIONS + 1)
        )
        result = evaluate_group_ap(partitions)
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertIn(
            "GROUP_AP_PARTITIONS",
            result.preflight.provisional_exceeded,
        )

    def test_sub_ulp_confidence_cannot_split_one_binary64_tie(self):
        with self.assertRaisesRegex(
            GroupCandidateError,
            "already equal.*canonical binary64",
        ):
            APPartition(
                ("ap",),
                graph(1, 2, [edge(0, 0)]),
                (
                    Fraction(1, 2) + Fraction(1, 2**55),
                    Fraction(1, 2),
                ),
            )

    def test_small_graphs_match_exhaustive_ap_oracle(self):
        pairs = tuple(itertools.product(range(2), repeat=2))
        confidence_cases = tuple(itertools.product(
            (Fraction(1, 2), Fraction(1)),
            repeat=2,
        ))
        for edge_mask in range(1 << len(pairs)):
            candidate = graph(2, 2, [
                edge(gt, prediction)
                for bit, (gt, prediction) in enumerate(pairs)
                if edge_mask & (1 << bit)
            ])
            for confidences in confidence_cases:
                thresholds = sorted(set(confidences), reverse=True)
                oracle = Fraction(0)
                previous_recall = Fraction(0)
                for threshold in thresholds:
                    active = {
                        prediction
                        for prediction, confidence in enumerate(confidences)
                        if confidence >= threshold
                    }
                    prefix = graph(2, 2, [
                        row for row in candidate.edges
                        if row.prediction in active
                    ])
                    tp = a1_exhaustive(prefix).objective.k1
                    precision = Fraction(tp, len(active))
                    recall = Fraction(tp, 2)
                    oracle += (recall - previous_recall) * precision
                    previous_recall = recall
                with self.subTest(
                    edge_mask=edge_mask,
                    confidences=confidences,
                ):
                    result = evaluate_group_ap((
                        APPartition(("ap",), candidate, confidences),
                    ))
                    self.assertEqual(result.value.ap, oracle)

    def test_macro_ap_uses_one_weight_per_unique_group(self):
        first = evaluate_group_ap((
            APPartition(("ap",), diagonal_graph(1), (Fraction(1),)),
        )).value
        second = evaluate_group_ap((
            APPartition(("ap",), graph(1, 0, []), ()),
        )).value
        self.assertEqual(
            reduce_macro_average_precision_math(
                (("z", first), ("a", second)),
            ).value64,
            mean64(
                (("z", first.ap_group64), ("a", second.ap_group64)),
                key_domain="utf8",
            ),
        )
        reduction = reduce_macro_average_precision_math((("only", first),))
        self.assertEqual(reduction.defined_group_count, 1)
        self.assertEqual(
            reduction.authority_status,
            "MATH_ONLY_SUPPORT_FLOOR_NOT_EVALUATED",
        )
        with self.assertRaisesRegex(GroupCandidateError, "unique"):
            reduce_macro_average_precision_math(
                (("same", first), ("same", first)),
            )

    def test_macro_ap_signed_compound_rounding_golden(self):
        values = tuple(
            APValue(value, rn64(value), 1, ())
            for value in (Fraction(1, 5), Fraction(3, 10), Fraction(1))
        )
        result = reduce_macro_average_precision_math((
            ("group-c", values[2]),
            ("group-a", values[0]),
            ("group-b", values[1]),
        ))
        self.assertEqual(result.value64, "f64:3fe0000000000000")
        self.assertNotEqual(result.value64, "f64:3fdfffffffffffff")
        self.assertNotEqual(result.value64, "f64:3ff8000000000000")


class CoverageTests(unittest.TestCase):
    def test_ambiguous_partition_produces_exact_zero_to_one_envelope(self):
        all_edges = [
            edge(gt, prediction)
            for gt in range(2)
            for prediction in range(2)
        ]
        result = evaluate_group_coverage((
            CoveragePartition(
                ("unit",),
                ("part",),
                graph(2, 2, all_edges),
                (True, False),
                (True, False),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        unit = result.value.units[0]
        self.assertEqual(unit.reason, GroupReason.OK)
        self.assertEqual(unit.coverage_minus, Fraction(0))
        self.assertEqual(unit.coverage_plus, Fraction(1))
        self.assertEqual(unit.partition_numerator_bounds, ((0, 1),))

    def test_group_is_equal_weight_mean_of_unit_values(self):
        result = evaluate_group_coverage((
            CoveragePartition(
                ("unit-a",),
                ("part-a",),
                diagonal_graph(1),
                (True,),
                (False,),
            ),
            CoveragePartition(
                ("unit-b",),
                ("part-b",),
                diagonal_graph(3),
                (True, True, True),
                (True, True, True),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(
            result.value.coverage_minus_group64,
            rn64(Fraction(1, 2)),
        )
        self.assertEqual(
            result.value.coverage_plus_group64,
            rn64(Fraction(1, 2)),
        )

    def test_unit_without_actionable_gt_is_reported_and_omitted(self):
        result = evaluate_group_coverage((
            CoveragePartition(
                ("empty",),
                ("part-empty",),
                diagonal_graph(1),
                (False,),
                (True,),
            ),
            CoveragePartition(
                ("valid",),
                ("part-valid",),
                diagonal_graph(1),
                (True,),
                (True,),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        by_key = {unit.unit_key: unit for unit in result.value.units}
        self.assertEqual(
            by_key[("empty",)].reason,
            GroupReason.NO_ACTIONABLE_GT,
        )
        self.assertIsNone(by_key[("empty",)].coverage_minus)
        self.assertEqual(result.value.defined_unit_count, 1)
        self.assertEqual(result.value.na_unit_count, 1)
        self.assertEqual(
            result.value.coverage_minus_group64,
            rn64(Fraction(1)),
        )

    def test_all_units_without_actionable_gt_are_not_applicable(self):
        result = evaluate_group_coverage((
            CoveragePartition(
                ("empty",),
                ("part",),
                diagonal_graph(1),
                (False,),
                (True,),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(result.reason, GroupReason.NO_ACTIONABLE_GT)
        self.assertEqual(result.witness["defined_unit_count"], 0)
        self.assertEqual(result.witness["na_unit_count"], 1)

    def test_partitions_per_unit_cap_is_fail_closed(self):
        partitions = tuple(
            CoveragePartition(
                ("same",),
                (f"part-{index}",),
                graph(0, 0, []),
                (),
                (),
            )
            for index in range(
                subject.COVERAGE_MAX_PARTITIONS_PER_UNIT + 1
            )
        )
        result = evaluate_group_coverage(partitions)
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertEqual(
            result.reason,
            GroupReason.SOLVER_STRUCTURAL_LIMIT_EXCEEDED,
        )
        self.assertIn(
            "GROUP_COVERAGE_PARTITIONS_PER_UNIT",
            result.preflight.provisional_exceeded,
        )

    def test_macro_coverage_rejects_duplicate_group_id(self):
        value = evaluate_group_coverage((
            CoveragePartition(
                ("unit",),
                ("part",),
                diagonal_graph(1),
                (True,),
                (True,),
            ),
        )).value
        self.assertEqual(
            (
                reduce_macro_coverage_math(
                    (("g", value),),
                ).coverage_minus64,
                reduce_macro_coverage_math(
                    (("g", value),),
                ).coverage_plus64,
            ),
            (
                value.coverage_minus_group64,
                value.coverage_plus_group64,
            ),
        )
        with self.assertRaisesRegex(GroupCandidateError, "unique"):
            reduce_macro_coverage_math((("g", value), ("g", value)))

    def test_published_binary64_is_included_in_exact_bit_bound(self):
        result = evaluate_group_coverage((
            CoveragePartition(
                ("unit",),
                ("part",),
                diagonal_graph(3),
                (True, True, True),
                (True, False, False),
            ),
        ))
        output = exact_n64(result.value.coverage_minus_group64)
        self.assertEqual(result.value.units[0].coverage_minus, Fraction(1, 3))
        self.assertGreaterEqual(
            result.preflight.exact_scalar_bit_bound,
            subject.rational_bit_length(output),
        )

    def test_small_graphs_match_exhaustive_coverage_oracle(self):
        pairs = tuple(itertools.product(range(2), repeat=2))
        actions = tuple(itertools.product((False, True), repeat=2))
        for edge_mask in range(1 << len(pairs)):
            candidate = graph(2, 2, [
                edge(gt, prediction)
                for bit, (gt, prediction) in enumerate(pairs)
                if edge_mask & (1 << bit)
            ])
            optimum = a1_exhaustive(candidate).m_star
            self.assertIsNotNone(optimum)
            for actionable_gt in actions:
                denominator = sum(actionable_gt)
                for actionable_prediction in actions:
                    with self.subTest(
                        edge_mask=edge_mask,
                        actionable_gt=actionable_gt,
                        actionable_prediction=actionable_prediction,
                    ):
                        result = evaluate_group_coverage((
                            CoveragePartition(
                                ("unit",),
                                ("part",),
                                candidate,
                                actionable_gt,
                                actionable_prediction,
                            ),
                        ))
                        if denominator == 0:
                            self.assertEqual(
                                result.reason,
                                GroupReason.NO_ACTIONABLE_GT,
                            )
                            continue
                        counts = tuple(
                            sum(
                                actionable_gt[gt]
                                and actionable_prediction[prediction]
                                for gt, prediction in matching
                            )
                            for matching in optimum
                        )
                        unit = result.value.units[0]
                        self.assertEqual(
                            unit.coverage_minus,
                            Fraction(min(counts), denominator),
                        )
                        self.assertEqual(
                            unit.coverage_plus,
                            Fraction(max(counts), denominator),
                        )

    def test_partition_permutation_is_fully_invariant(self):
        rows = (
            CoveragePartition(
                ("unit",),
                ("part-b",),
                diagonal_graph(1),
                (True,),
                (False,),
            ),
            CoveragePartition(
                ("unit",),
                ("part-a",),
                diagonal_graph(1),
                (True,),
                (True,),
            ),
        )
        forward = evaluate_group_coverage(rows)
        reverse = evaluate_group_coverage(tuple(reversed(rows)))
        self.assertEqual(forward, reverse)

    def test_thirty_group_replica_preserves_lower_envelope(self):
        value = evaluate_group_coverage((
            CoveragePartition(
                ("unit",),
                ("part",),
                diagonal_graph(1),
                (True,),
                (True,),
            ),
        )).value
        reduction = reduce_macro_coverage_math(tuple(
            (f"group-{index:02d}", value)
            for index in range(30)
        ))
        self.assertEqual(reduction.defined_group_count, 30)
        self.assertEqual(reduction.coverage_minus64, rn64(Fraction(1)))
        self.assertEqual(reduction.groups_with_na_units, ())


class SpearmanTests(unittest.TestCase):
    def test_unique_optimum_certifies_positive_rho(self):
        values = tuple(Fraction(index) for index in range(10))
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",), diagonal_graph(10), values, values),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(result.value.rho.sign, 1)
        self.assertEqual(result.value.support, 10)
        self.assertEqual(result.value.certificate, "UNIQUE_OPTIMUM")

    def test_support_below_ten_is_not_applicable(self):
        values = tuple(Fraction(index) for index in range(9))
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",), diagonal_graph(9), values, values),
        ))
        self.assertEqual(result.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(
            result.reason,
            GroupReason.INSUFFICIENT_MATCHED_SUPPORT,
        )
        with self.assertRaises(TypeError):
            evaluate_group_spearman(  # type: ignore[call-arg]
                (
                    SpearmanPartition(
                        ("part",),
                        diagonal_graph(2),
                        values[:2],
                        values[:2],
                    ),
                ),
                minimum_support=1,
            )

    def test_zero_variance_is_not_applicable(self):
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",),
                diagonal_graph(10),
                (Fraction(1),) * 10,
                tuple(Fraction(index) for index in range(10)),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(result.reason, GroupReason.SPEARMAN_UNDEFINED)

    def test_perfect_ambiguous_matching_can_change_rho(self):
        all_edges = [
            edge(gt, prediction)
            for gt in range(10)
            for prediction in range(10)
        ]
        ascending = tuple(Fraction(index) for index in range(10))
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",),
                graph(10, 10, all_edges),
                ascending,
                ascending,
            ),
        ))
        self.assertEqual(result.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(result.reason, GroupReason.PAIRING_AMBIGUOUS)
        self.assertIsInstance(result.witness, SpearmanAmbiguityWitness)
        self.assertEqual(result.witness.lower.rho.sign, -1)
        self.assertEqual(result.witness.upper.rho.sign, 1)

    def test_fixed_marginal_tie_can_certify_singleton_rho(self):
        tied_edges = [edge(index, index) for index in range(10)]
        tied_edges.extend((edge(0, 1), edge(1, 0)))
        gt_values = tuple(Fraction(index) for index in range(10))
        prediction_values = (
            Fraction(0),
            Fraction(0),
            *(Fraction(index) for index in range(2, 10)),
        )
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",),
                graph(10, 10, tied_edges),
                gt_values,
                prediction_values,
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(
            result.value.certificate,
            "FIXED_MARGINAL_RHO_SINGLETON",
        )

    def test_rectangular_equal_value_marginal_certifies_singleton(self):
        rectangular_edges = (
            edge(0, 0),
            edge(1, 1),
            edge(0, 1),
            edge(1, 0),
        )
        partitions = [
            SpearmanPartition(
                ("rectangular",),
                graph(2, 3, rectangular_edges),
                (Fraction(0), Fraction(1)),
                (Fraction(0), Fraction(0), Fraction(100)),
            ),
        ]
        for value in range(2, 10):
            partitions.append(SpearmanPartition(
                (f"unique-{value}",),
                diagonal_graph(1),
                (Fraction(value),),
                (Fraction(value),),
            ))
        result = evaluate_group_spearman(tuple(partitions))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(
            result.value.certificate,
            "FIXED_MARGINAL_RHO_SINGLETON",
        )

    def test_variable_vertex_same_value_can_still_certify_singleton(self):
        edges = [edge(index, index) for index in range(10)]
        edges.append(edge(0, 10))
        values = tuple(Fraction(index) for index in range(10))
        prediction_values = (*values, Fraction(0))
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",),
                graph(10, 11, edges),
                values,
                prediction_values,
            ),
        ))
        self.assertEqual(result.status, GroupStatus.CERTIFIED)
        self.assertEqual(
            result.value.certificate,
            "FIXED_MARGINAL_RHO_SINGLETON",
        )

    def test_variable_marginal_certificate_unavailable_is_explicit_na(self):
        edges = [edge(index, index) for index in range(10)]
        edges.append(edge(0, 10))
        values = tuple(Fraction(index) for index in range(10))
        prediction_values = (*values, Fraction(100))
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",),
                graph(10, 11, edges),
                values,
                prediction_values,
            ),
        ))
        self.assertEqual(result.status, GroupStatus.NOT_APPLICABLE)
        self.assertEqual(
            result.reason,
            GroupReason.SPEARMAN_CERTIFICATE_UNAVAILABLE,
        )
        self.assertEqual(
            result.witness["diagnostic"],
            "VARIABLE_MARGINAL_CERTIFICATE_UNAVAILABLE",
        )
        self.assertFalse(result.witness["ballot_ready"])

    def test_external_exact_scalar_over_cap_is_rejected(self):
        huge = Fraction(1 << subject.PROVISIONAL_MAX_EXACT_SCALAR_BITS)
        result = evaluate_group_spearman((
            SpearmanPartition(
                ("part",),
                diagonal_graph(1),
                (huge,),
                (Fraction(0),),
            ),
        ))
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertIn(
            "GROUP_EXACT_SCALAR_BIT_LENGTH",
            result.preflight.provisional_exceeded,
        )

    def test_partition_cap_is_fail_closed(self):
        partitions = tuple(
            SpearmanPartition(
                (f"part-{index}",), graph(0, 0, []), (), ())
            for index in range(subject.SPEARMAN_MAX_PARTITIONS + 1)
        )
        result = evaluate_group_spearman(partitions)
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertIn(
            "GROUP_SPEARMAN_PARTITIONS",
            result.preflight.provisional_exceeded,
        )

    def test_rho_identity_equality_avoids_square_root(self):
        left = RhoIdentity(1, Fraction(1), Fraction(2), Fraction(8))
        same = RhoIdentity(1, Fraction(4), Fraction(8), Fraction(8))
        opposite = RhoIdentity(-1, Fraction(4), Fraction(8), Fraction(8))
        self.assertTrue(rho_equal(left, same))
        self.assertFalse(rho_equal(left, opposite))


class ValidationTests(unittest.TestCase):
    def test_candidate_declares_activation_blockers(self):
        self.assertFalse(subject.GROUP_CANDIDATE_BALLOT_READY)
        self.assertIn(
            "GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED",
            subject.GROUP_CANDIDATE_LIMITATIONS,
        )

    def test_partition_wrappers_are_fail_closed(self):
        with self.assertRaisesRegex(GroupCandidateError, "length"):
            APPartition(("part",), diagonal_graph(1), ())
        with self.assertRaisesRegex(GroupCandidateError, "non-empty"):
            CoveragePartition(
                (), ("part",), diagonal_graph(1), (True,), (True,))
        with self.assertRaisesRegex(GroupCandidateError, "length"):
            SpearmanPartition(
                ("part",), diagonal_graph(1), (), (Fraction(1),))

    def test_duplicate_partition_keys_do_not_inflate_support(self):
        values = tuple(Fraction(index) for index in range(5))
        duplicate = SpearmanPartition(
            ("same",), diagonal_graph(5), values, values)
        with self.assertRaisesRegex(GroupCandidateError, "unique"):
            evaluate_group_spearman((duplicate, duplicate))

    def test_empty_or_nul_keys_are_rejected(self):
        with self.assertRaisesRegex(GroupCandidateError, "NUL-free"):
            CoveragePartition(
                ("",), ("part",), diagonal_graph(1), (True,), (True,))
        with self.assertRaisesRegex(GroupCandidateError, "NUL-free"):
            APPartition(
                ("bad\x00key",), diagonal_graph(1), (Fraction(1),))

    def test_failure_provenance_is_key_addressed_and_order_invariant(self):
        rows = (
            CoveragePartition(
                ("unit",),
                ("part-b",),
                graph(129, 0, []),
                (False,) * 129,
                (),
            ),
            CoveragePartition(
                ("unit",),
                ("part-a",),
                graph(0, 0, []),
                (),
                (),
            ),
        )
        forward = evaluate_group_coverage(rows)
        reverse = evaluate_group_coverage(tuple(reversed(rows)))
        self.assertEqual(forward, reverse)
        self.assertEqual(
            forward.preflight.subgraph_provisional_exceeded,
            ((("part-b",), ("GT",)),),
        )

    def test_macro_input_shape_is_fail_closed(self):
        with self.assertRaisesRegex(GroupCandidateError, "contain"):
            reduce_macro_average_precision_math(
                (("bad",),),  # type: ignore[arg-type]
            )
        with self.assertRaisesRegex(GroupCandidateError, "contain"):
            reduce_macro_coverage_math(
                (("bad",),),  # type: ignore[arg-type]
            )


class PreflightBoundaryTests(unittest.TestCase):
    def test_ap_group_shape_boundaries(self):
        graphs = tuple(graph(0, 0, []) for _ in range(
            subject.AP_MAX_PARTITIONS))
        on = subject._group_preflight(  # type: ignore[attr-defined]
            "AP",
            graphs,
            distinct_thresholds=subject.AP_MAX_DISTINCT_THRESHOLDS,
            local_thresholds=subject.AP_MAX_LOCAL_THRESHOLDS,
            prefix_edge_incidence=subject.AP_MAX_PREFIX_EDGE_INCIDENCE,
        )
        self.assertFalse(on.provisional_exceeded)
        over = subject._group_preflight(  # type: ignore[attr-defined]
            "AP",
            (*graphs, graph(0, 0, [])),
            distinct_thresholds=subject.AP_MAX_DISTINCT_THRESHOLDS + 1,
            local_thresholds=subject.AP_MAX_LOCAL_THRESHOLDS + 1,
            prefix_edge_incidence=subject.AP_MAX_PREFIX_EDGE_INCIDENCE + 1,
        )
        self.assertIn("GROUP_AP_PARTITIONS", over.provisional_exceeded)
        self.assertIn(
            "GROUP_AP_DISTINCT_THRESHOLDS",
            over.provisional_exceeded,
        )
        self.assertIn(
            "GROUP_AP_LOCAL_THRESHOLDS",
            over.provisional_exceeded,
        )
        self.assertIn(
            "GROUP_AP_PREFIX_EDGE_INCIDENCE",
            over.provisional_exceeded,
        )

    def test_coverage_unit_boundaries(self):
        on = subject._group_preflight(  # type: ignore[attr-defined]
            "COVERAGE",
            (),
            unit_count=subject.COVERAGE_MAX_UNITS,
            maximum_partitions_per_unit=(
                subject.COVERAGE_MAX_PARTITIONS_PER_UNIT
            ),
        )
        self.assertFalse(on.provisional_exceeded)
        over = subject._group_preflight(  # type: ignore[attr-defined]
            "COVERAGE",
            (),
            unit_count=subject.COVERAGE_MAX_UNITS + 1,
            maximum_partitions_per_unit=(
                subject.COVERAGE_MAX_PARTITIONS_PER_UNIT + 1
            ),
        )
        self.assertIn("GROUP_COVERAGE_UNITS", over.provisional_exceeded)
        self.assertIn(
            "GROUP_COVERAGE_PARTITIONS_PER_UNIT",
            over.provisional_exceeded,
        )

    def test_spearman_support_boundary(self):
        on = subject._group_preflight(  # type: ignore[attr-defined]
            "SPEARMAN",
            (graph(64, 64, []), graph(64, 64, [])),
        )
        self.assertNotIn(
            "GROUP_SPEARMAN_SUPPORT",
            on.provisional_exceeded,
        )
        over = subject._group_preflight(  # type: ignore[attr-defined]
            "SPEARMAN",
            (graph(65, 65, []), graph(64, 64, [])),
        )
        self.assertIn(
            "GROUP_SPEARMAN_SUPPORT",
            over.provisional_exceeded,
        )

    def test_exact_scalar_bit_boundary(self):
        on = subject._group_preflight(  # type: ignore[attr-defined]
            "SPEARMAN",
            (),
            external_exact_values=(
                Fraction(1 << (
                    subject.PROVISIONAL_MAX_EXACT_SCALAR_BITS - 2
                )),
            ),
        )
        self.assertEqual(
            on.exact_scalar_bit_bound,
            subject.PROVISIONAL_MAX_EXACT_SCALAR_BITS,
        )
        self.assertNotIn(
            "GROUP_EXACT_SCALAR_BIT_LENGTH",
            on.provisional_exceeded,
        )
        over = subject._group_preflight(  # type: ignore[attr-defined]
            "SPEARMAN",
            (),
            external_exact_values=(
                Fraction(1 << (
                    subject.PROVISIONAL_MAX_EXACT_SCALAR_BITS - 1
                )),
            ),
        )
        self.assertIn(
            "GROUP_EXACT_SCALAR_BIT_LENGTH",
            over.provisional_exceeded,
        )


class RuntimeFailureTests(unittest.TestCase):
    def test_ap_runtime_failure_is_fail_closed(self):
        with mock.patch.object(
            subject,
            "exact_maximum_cardinality",
            side_effect=RuntimeError("injected"),
        ):
            result = evaluate_group_ap((
                APPartition(
                    ("part",),
                    diagonal_graph(1),
                    (Fraction(1),),
                ),
            ))
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertEqual(result.reason, GroupReason.SOLVER_RUNTIME_FAILURE)

    def test_coverage_runtime_failure_is_fail_closed(self):
        with mock.patch.object(
            subject,
            "exact_maximum_cardinality",
            side_effect=RuntimeError("injected"),
        ):
            result = evaluate_group_coverage((
                CoveragePartition(
                    ("unit",),
                    ("part",),
                    diagonal_graph(1),
                    (True,),
                    (True,),
                ),
            ))
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertEqual(result.reason, GroupReason.SOLVER_RUNTIME_FAILURE)

    def test_spearman_runtime_failure_is_fail_closed(self):
        values = tuple(Fraction(index) for index in range(10))
        with mock.patch.object(
            subject,
            "exact_maximum_cardinality",
            side_effect=RuntimeError("injected"),
        ):
            result = evaluate_group_spearman((
                SpearmanPartition(
                    ("part",),
                    diagonal_graph(10),
                    values,
                    values,
                ),
            ))
        self.assertEqual(result.status, GroupStatus.REJECTED)
        self.assertEqual(result.reason, GroupReason.SOLVER_RUNTIME_FAILURE)

    def _group_surfaces(self):
        """The three group entry points, each with a minimal valid input."""
        values = tuple(Fraction(index) for index in range(10))
        return (
            (
                "ap",
                lambda: evaluate_group_ap((
                    APPartition(("part",), diagonal_graph(1), (Fraction(1),)),
                )),
            ),
            (
                "coverage",
                lambda: evaluate_group_coverage((
                    CoveragePartition(
                        ("unit",), ("part",), diagonal_graph(1),
                        (True,), (True,),
                    ),
                )),
            ),
            (
                "spearman",
                lambda: evaluate_group_spearman((
                    SpearmanPartition(
                        ("part",), diagonal_graph(10), values, values,
                    ),
                )),
            ),
        )

    def test_every_declared_runtime_failure_is_fail_closed_on_every_surface(self):
        """No member of the taxonomy may escape as a bare exception.

        The named tests above pin RuntimeError only.  TimeoutError was absent
        from the group taxonomy while the A1 enforcement tranche added it to
        the per-subgraph one, so a timeout escaped uncaught on this path —
        exactly how the expensive Spearman variable-marginal case would fail.
        This sweeps the whole declared taxonomy across all three surfaces so a
        future addition cannot be covered on one surface and missed on another.
        """
        for failure in subject._RUNTIME_FAILURES:
            for label, call in self._group_surfaces():
                with self.subTest(failure=failure.__name__, surface=label):
                    with mock.patch.object(
                        subject,
                        "exact_maximum_cardinality",
                        side_effect=failure("injected"),
                    ):
                        result = call()
                    self.assertEqual(result.status, GroupStatus.REJECTED)
                    self.assertEqual(
                        result.reason, GroupReason.SOLVER_RUNTIME_FAILURE)

    def test_group_runtime_taxonomy_matches_the_per_subgraph_authority(self):
        """Structural guard against the two kernels drifting apart again.

        The fail policy names one set of fatal runtime reasons; two kernels
        implementing different sets means one of them is wrong.  Equality is
        asserted as a set so ordering is irrelevant.
        """
        self.assertEqual(
            set(subject._RUNTIME_FAILURES),
            set(per_subgraph._A1_RUNTIME_FAILURES),
        )
        self.assertIn(TimeoutError, subject._RUNTIME_FAILURES)


if __name__ == "__main__":
    unittest.main()
