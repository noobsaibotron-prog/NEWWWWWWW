"""REV8 step 2 (§14.1) — N64, identity/decision key, exact arithmetic.

These primitives exist to remove levers a benchmark submitter could otherwise
pull, so the tests are organised by lever rather than by function:

  (token, value)          `100` and `100.0` must not yield different ids
  (zero, sign)            `-0.0` and `+0.0` must not yield different ids
  (id, ordering)          no id or digest may reach a decision key
  (payload, decision key) they differ by exactly one field, on purpose
  (ordinal, input order)  duplicate predictions number 0..N-1 either way
  (sum, term order)       the objective must not depend on accumulation order
  (module, dispatcher)    REV8 code must stay unreachable from the REV7 package

The last one is a contract requirement, not hygiene: §14.1 admits no state with
REV8 norms and REV7 consumers, so a v2 name leaking into `ml_v3.contracts`
would itself be the half-activated state the switch is designed to prevent.

Records here are shaped per the REV8 candidate §9.6, which schema v2 does not
yet register — that lands with the atomic switch, and until then this file is
the only place the shapes are asserted.
"""
from __future__ import annotations

import hashlib
import itertools
import random
import struct
import unittest
from fractions import Fraction
from pathlib import Path

import ml_v3.contracts as contracts_pkg
from ml_v3.contracts.canonical import canonical_bytes
from ml_v3.contracts.exact_arith_v2 import (
    ExactArithError,
    exact,
    exact_sum,
    exact_tuple,
)
from ml_v3.contracts.identity_v2 import (
    INSTANCE_ID_DOMAIN,
    IdentityError,
    annotation_top_level_key,
    assign_occurrence_ordinals,
    decision_key,
    evaluation_unit_key,
    instance_id,
    instance_key,
    semantic_payload,
)
from ml_v3.contracts.metrology_lock import gate_platform_python_label
from ml_v3.contracts.normalize_v2 import (
    N64_PREFIX,
    NormalizationError,
    n64,
    normalized_canonical_bytes,
)
from ml_v3.contracts.numeric_authority_v2 import (
    NumericAuthorityError,
    P95_QUANTILE,
    exact_n64,
    mean64,
    p95_type7_64,
    rn64,
    sum_pairwise64,
)
from ml_v3.contracts.support_v2 import (
    AGGREGATION_AUTHORITY_STATUS,
    HierarchicalMetricAggregation,
    MetricUnitResult,
    SupportAccounting,
    SupportError,
    UnitOutcome,
    aggregate_unit_metric_values,
    account_support,
)
from ml_v3.contracts.support_floor_v2 import (
    O13F_AUTHORITY_STATUS,
    POLICY_REVISION,
    POLICY_SCHEMA,
    SOURCE_CONTRACT_SHA256,
    StratumPopulation,
    SupportFloorError,
    evaluate_support_floors,
    stratum_population_plan_sha256,
    support_floor_policy_sha256,
)


def region(problem_type="Muddiness", start=0.0, end=1.0, lo=200.0, hi=500.0,
           center=None, direction=None, severity=0.8, confidence=0.9,
           actionable=True, **unit):
    """A REV8 §9.6 semantic region / bundle record."""
    record = {
        "problem_type": problem_type,
        "problem_type_id": 2,
        "start_s": start,
        "end_s": end,
        "band_lo_hz": lo,
        "band_hi_hz": hi,
        "center_hz": center,
        "direction": direction,
        "severity": severity,
        "confidence": confidence,
        "actionable": actionable,
    }
    record.update(_unit(**unit))
    return record


def event(problem_type="Resonance", start=0.0, end=1.0, center=1000.0,
          width=0.5, severity=0.8, confidence=0.9, actionable=True, **unit):
    """A REV8 §9.6 dynamic / prediction event record."""
    record = {
        "problem_type": problem_type,
        "problem_type_id": 0,
        "start_s": start,
        "end_s": end,
        "center_hz": center,
        "width_octaves": width,
        "severity": severity,
        "confidence": confidence,
        "actionable": actionable,
    }
    record.update(_unit(**unit))
    return record


def _unit(unit_id="unit-000", asset_id="asset-000", profile="generic",
          seg_start=0.0, seg_end=10.0):
    return {
        "evaluation_unit_id": unit_id,
        "asset_id": asset_id,
        "profile": profile,
        "segment_start_s": seg_start,
        "segment_end_s": seg_end,
    }


class DispatcherIsolationTests(unittest.TestCase):
    """§14.1: REV8 modules are not exported by the active REV7 package."""

    def test_gate_platform(self):
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")

    def test_no_v2_name_is_exported_by_the_rev7_dispatcher(self):
        for name in contracts_pkg.__all__:
            self.assertNotIn("_v2", name, msg=f"{name} leaks REV8 into REV7")

    def test_rev8_primitives_are_not_attributes_of_the_package(self):
        for name in ("n64", "instance_id", "decision_key", "exact_sum",
                     "semantic_payload", "normalized_canonical_bytes"):
            with self.subTest(name=name):
                self.assertFalse(
                    hasattr(contracts_pkg, name),
                    msg=f"{name} is reachable from the REV7 dispatcher")


class N64Tests(unittest.TestCase):
    def test_contract_literal_examples(self):
        """§9.5 prints these two results; they are the specification."""
        self.assertEqual(n64(100), "f64:4059000000000000")
        self.assertEqual(n64(100.0), "f64:4059000000000000")
        self.assertEqual(n64(-0.0), "f64:0000000000000000")
        self.assertEqual(n64(0.0), "f64:0000000000000000")

    def test_shape_is_prefix_plus_sixteen_lowercase_hex(self):
        text = n64(-1234.5)
        self.assertTrue(text.startswith(N64_PREFIX))
        body = text[len(N64_PREFIX):]
        self.assertEqual(len(body), 16)
        self.assertEqual(body, body.lower())
        self.assertEqual(bytes.fromhex(body), struct.pack(">d", -1234.5))

    def test_it_closes_the_canonical_bytes_token_lever(self):
        """The defect N64 exists for: same value, different canonical bytes."""
        self.assertNotEqual(canonical_bytes([100]), canonical_bytes([100.0]))
        self.assertNotEqual(canonical_bytes([-0.0]), canonical_bytes([0.0]))
        self.assertEqual(canonical_bytes([n64(100)]), canonical_bytes([n64(100.0)]))
        self.assertEqual(canonical_bytes([n64(-0.0)]), canonical_bytes([n64(0.0)]))

    def test_booleans_are_not_numbers(self):
        for value in (True, False):
            with self.subTest(value=value):
                with self.assertRaisesRegex(NormalizationError, "boolean"):
                    n64(value)

    def test_non_finite_rejected(self):
        with self.assertRaisesRegex(NormalizationError, "NaN"):
            n64(float("nan"))
        for value in (float("inf"), float("-inf")):
            with self.subTest(value=value):
                with self.assertRaisesRegex(NormalizationError, "infinity"):
                    n64(value)

    def test_integer_too_large_for_binary64_is_overflow_not_silence(self):
        with self.assertRaisesRegex(NormalizationError, "overflow"):
            n64(10 ** 400)

    def test_non_numbers_rejected(self):
        for value in ("1.0", None, [1.0], {"a": 1}):
            with self.subTest(value=value):
                with self.assertRaises(NormalizationError):
                    n64(value)

    def test_distinct_values_stay_distinct(self):
        self.assertNotEqual(n64(1.0), n64(1.0000000000000002))


class NormalizedCanonicalBytesTests(unittest.TestCase):
    def test_raw_float_anywhere_is_refused(self):
        for obj in (1.5, [1.5], {"a": 1.5}, {"a": [{"b": [1.5]}]}):
            with self.subTest(obj=obj):
                with self.assertRaisesRegex(NormalizationError, "un-normalised"):
                    normalized_canonical_bytes(obj)

    def test_the_error_names_the_path(self):
        with self.assertRaisesRegex(NormalizationError, r"\$\.a\[0\]\.b"):
            normalized_canonical_bytes({"a": [{"b": 2.5}]})

    def test_integers_booleans_strings_and_null_pass_through(self):
        obj = {"i": 7, "b": True, "s": "x", "n": None, "f": n64(1.5)}
        self.assertEqual(normalized_canonical_bytes(obj), canonical_bytes(obj))


class EvaluationUnitKeyTests(unittest.TestCase):
    def test_the_id_alone_does_not_identify_a_unit(self):
        """§9.6: same id, different asset, must be a different unit."""
        a = region(asset_id="asset-000")
        b = region(asset_id="asset-999")
        self.assertEqual(a["evaluation_unit_id"], b["evaluation_unit_id"])
        self.assertNotEqual(evaluation_unit_key(a), evaluation_unit_key(b))

    def test_every_component_participates(self):
        base = region()
        for field, value in (("evaluation_unit_id", "unit-999"),
                             ("asset_id", "asset-999"),
                             ("profile", "drums"),
                             ("segment_start_s", 1.0),
                             ("segment_end_s", 11.0)):
            with self.subTest(field=field):
                self.assertNotEqual(base[field], value)
                other = {**base, field: value}
                self.assertNotEqual(evaluation_unit_key(base),
                                    evaluation_unit_key(other))

    def test_segment_times_are_normalised(self):
        integral = region(seg_start=0, seg_end=10)
        floating = region(seg_start=0.0, seg_end=10.0)
        self.assertNotEqual(canonical_bytes([integral["segment_start_s"]]),
                            canonical_bytes([floating["segment_start_s"]]))
        self.assertEqual(evaluation_unit_key(integral),
                         evaluation_unit_key(floating))

    def test_annotation_key_adds_annotator_and_pass(self):
        record = {**region(), "annotator_id": "ann-1", "pass_id": "p1"}
        key = annotation_top_level_key(record)
        self.assertEqual(key[0], evaluation_unit_key(record))
        self.assertEqual(key[1:], ["ann-1", "p1"])

    def test_missing_component_is_fail_closed(self):
        broken = {k: v for k, v in region().items() if k != "profile"}
        with self.assertRaisesRegex(IdentityError, "profile"):
            evaluation_unit_key(broken)


class PayloadAndDecisionKeyTests(unittest.TestCase):
    def test_payload_field_order_matches_the_contract_listing(self):
        payload = semantic_payload("semantic_region", region())
        self.assertEqual(payload[0], "semantic_region")
        self.assertEqual(payload[1], evaluation_unit_key(region()))
        self.assertEqual(payload[2], "Muddiness")
        self.assertEqual(payload[3], 2)                       # problem_type_id
        self.assertEqual(payload[4], n64(0.0))                # start_s
        self.assertEqual(payload[5], n64(1.0))                # end_s
        self.assertEqual(payload[6:8], [n64(200.0), n64(500.0)])
        self.assertIsNone(payload[8])                         # center_hz
        self.assertIsNone(payload[9])                         # direction
        self.assertEqual(payload[10:], [n64(0.8), n64(0.9), True])

    def test_event_payload_carries_centre_and_width(self):
        payload = semantic_payload("dynamic_event", event())
        self.assertEqual(payload[6:8], [n64(1000.0), n64(0.5)])
        self.assertEqual(len(payload), 11)

    def test_decision_key_is_the_payload_minus_problem_type_id(self):
        """The one intended difference between the two keys, pinned."""
        for kind, record in (("semantic_region", region()),
                             ("dynamic_event", event())):
            with self.subTest(kind=kind):
                payload = semantic_payload(kind, record)
                key = decision_key(kind, record)
                self.assertEqual(len(key), len(payload) - 1)
                self.assertEqual(key, payload[:3] + payload[4:])

    def test_decision_key_contains_no_id_and_no_digest(self):
        """§10.1: ids and hashes must not reach an ordering decision."""
        key = decision_key("semantic_bundle", region())
        flat = canonical_bytes(key).decode()
        self.assertNotIn(INSTANCE_ID_DOMAIN, flat)
        ident = instance_id(instance_key("semantic_bundle", region(), 0))
        self.assertNotIn(ident, flat)

    def test_type_id_disagreement_does_not_change_the_decision_key(self):
        a = region()
        b = {**a, "problem_type_id": 5}
        self.assertNotEqual(semantic_payload("semantic_region", a),
                            semantic_payload("semantic_region", b))
        self.assertEqual(decision_key("semantic_region", a),
                         decision_key("semantic_region", b))

    def test_unknown_kind_is_fail_closed(self):
        with self.assertRaisesRegex(IdentityError, "unknown record kind"):
            semantic_payload("not_a_kind", region())

    def test_missing_geometry_is_fail_closed(self):
        broken = {k: v for k, v in region().items() if k != "band_hi_hz"}
        with self.assertRaisesRegex(IdentityError, "band_hi_hz"):
            semantic_payload("semantic_region", broken)

    def test_actionable_must_be_boolean(self):
        with self.assertRaisesRegex(IdentityError, "actionable"):
            semantic_payload("semantic_region", region(actionable=1))


class OccurrenceOrdinalTests(unittest.TestCase):
    def test_identical_payloads_number_consecutively(self):
        records = [region(), region(), region()]
        self.assertEqual(assign_occurrence_ordinals("semantic_bundle", records),
                         [0, 1, 2])

    def test_distinct_payloads_each_start_at_zero(self):
        records = [region(lo=200.0), region(lo=300.0), region(lo=400.0)]
        self.assertEqual(assign_occurrence_ordinals("semantic_bundle", records),
                         [0, 0, 0])

    def test_multiset_of_ordinals_is_independent_of_input_order(self):
        """§9.6: 0..N-1, indipendenti dall'ordine d'ingresso."""
        records = [region(), region(lo=300.0), region(), region()]
        base = sorted(assign_occurrence_ordinals("semantic_bundle", records))
        for permutation in itertools.permutations(records):
            self.assertEqual(
                sorted(assign_occurrence_ordinals("semantic_bundle",
                                                  list(permutation))),
                base)

    def test_ground_truth_kinds_take_no_ordinals(self):
        for kind in ("semantic_region", "dynamic_event"):
            with self.subTest(kind=kind):
                with self.assertRaisesRegex(IdentityError, "ground truth"):
                    assign_occurrence_ordinals(kind, [region()])


class InstanceIdentityTests(unittest.TestCase):
    def test_prediction_key_is_payload_plus_ordinal(self):
        key = instance_key("semantic_bundle", region(), 3)
        self.assertEqual(key, [semantic_payload("semantic_bundle", region()), 3])

    def test_ground_truth_key_is_the_payload_alone(self):
        key = instance_key("semantic_region", region())
        self.assertEqual(key, semantic_payload("semantic_region", region()))

    def test_prediction_without_ordinal_is_fail_closed(self):
        with self.assertRaisesRegex(IdentityError, "occurrence_ordinal"):
            instance_key("semantic_bundle", region())

    def test_ground_truth_with_ordinal_is_fail_closed(self):
        with self.assertRaisesRegex(IdentityError, "takes no occurrence_ordinal"):
            instance_key("semantic_region", region(), 0)

    def test_negative_ordinal_rejected(self):
        with self.assertRaisesRegex(IdentityError, "non-negative"):
            instance_key("semantic_bundle", region(), -1)

    def test_id_is_a_sha256_hex_digest(self):
        ident = instance_id(instance_key("semantic_region", region()))
        self.assertEqual(len(ident), 64)
        self.assertEqual(ident, ident.lower())
        int(ident, 16)

    def test_id_is_stable_and_content_derived(self):
        first = instance_id(instance_key("semantic_region", region()))
        second = instance_id(instance_key("semantic_region", region()))
        self.assertEqual(first, second)
        moved = instance_id(instance_key("semantic_region", region(lo=201.0)))
        self.assertNotEqual(first, moved)

    def test_token_choice_cannot_change_the_id(self):
        """`200` and `200.0` are the same band edge and must be the same record."""
        integral = instance_id(instance_key("semantic_region", region(lo=200)))
        floating = instance_id(instance_key("semantic_region", region(lo=200.0)))
        self.assertEqual(integral, floating)

    def test_ordinal_changes_the_id(self):
        zero = instance_id(instance_key("semantic_bundle", region(), 0))
        one = instance_id(instance_key("semantic_bundle", region(), 1))
        self.assertNotEqual(zero, one)

    def test_domain_separation(self):
        """The id hashes [domain, key], not the bare key."""
        key = instance_key("semantic_region", region())
        import hashlib
        bare = hashlib.sha256(normalized_canonical_bytes(key)).hexdigest()
        self.assertNotEqual(instance_id(key), bare)

    def test_kind_participates_in_identity(self):
        gt = instance_id(instance_key("semantic_region", region()))
        pred = instance_id(instance_key("semantic_bundle", region(), 0))
        self.assertNotEqual(gt, pred)


class ExactArithmeticTests(unittest.TestCase):
    def test_sum_is_independent_of_term_order(self):
        """The property the matching objective needs and floats do not give."""
        values = [1e16, 1.0, -1e16, 0.5]
        expected = exact_sum(values)
        for permutation in itertools.permutations(values):
            self.assertEqual(exact_sum(permutation), expected)
        self.assertEqual(expected, Fraction(3, 2))

    def test_a_plain_left_fold_would_not_have_that_property(self):
        """Why the rule is normative rather than left to the implementer."""
        values = [1e16, 1.0, -1e16, 0.5]
        folds = set()
        for permutation in itertools.permutations(values):
            total = 0.0
            for value in permutation:
                total += value
            folds.add(total)
        self.assertGreater(len(folds), 1)
        self.assertEqual(len({exact_sum(p) for p in itertools.permutations(values)}), 1)

    def test_exact_is_the_true_binary64_value(self):
        self.assertEqual(exact(0.1), Fraction(*(0.1).as_integer_ratio()))
        self.assertNotEqual(exact(0.1), Fraction(1, 10))

    def test_contract_integers_never_pass_through_float(self):
        for value in (2 ** 53 + 1, 2 ** 60 + 1, 10 ** 400):
            with self.subTest(value=value):
                self.assertEqual(exact(value), Fraction(value))

    def test_empty_sum_is_zero(self):
        self.assertEqual(exact_sum([]), Fraction(0))

    def test_comparison_distinguishes_one_ulp(self):
        self.assertLess(exact_sum([1.0]), exact_sum([1.0000000000000002]))

    def test_tuple_preserves_order_for_lexicographic_keys(self):
        self.assertEqual(exact_tuple([2.0, 1.0]), (Fraction(2), Fraction(1)))

    def test_booleans_and_non_finite_rejected(self):
        with self.assertRaisesRegex(ExactArithError, "boolean"):
            exact(True)
        for value in (float("nan"), float("inf")):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ExactArithError, "finite"):
                    exact(value)

    def test_non_numbers_rejected(self):
        with self.assertRaisesRegex(ExactArithError, "only numbers"):
            exact("1.0")

    def test_non_integer_numeric_types_are_rejected(self):
        with self.assertRaisesRegex(ExactArithError, "only numbers"):
            exact(Fraction(1, 3))


class NumericAuthorityTests(unittest.TestCase):
    def test_n64_to_exact_rational_goldens(self):
        self.assertEqual(
            exact_n64("f64:3fc999999999999a"),
            Fraction(3602879701896397, 2 ** 54),
        )
        self.assertEqual(
            exact_n64("f64:3fc9999999999998"),
            Fraction(3602879701896396, 2 ** 54),
        )
        self.assertEqual(
            exact_n64("f64:3fc999999999999a")
            - exact_n64("f64:3fc9999999999998"),
            Fraction(1, 2 ** 54),
        )

    def test_rn64_normalises_zero_and_rejects_overflow(self):
        self.assertEqual(rn64(Fraction(0)), "f64:0000000000000000")
        with self.assertRaisesRegex(NumericAuthorityError, "overflow"):
            rn64(Fraction(10 ** 400))

    def test_pairwise_and_mean_goldens(self):
        values = [n64(0.2), n64(0.3), n64(1.0)]
        self.assertEqual(
            sum_pairwise64(values),
            "f64:3ff8000000000000",
        )
        entries = [(["g2"], values[1]), (["g1"], values[0]),
                   (["g3"], values[2])]
        self.assertEqual(mean64(entries), "f64:3fe0000000000000")

    def test_empty_reductions_are_fail_closed(self):
        with self.assertRaisesRegex(NumericAuthorityError, "N/A or FAIL"):
            sum_pairwise64([])
        self.assertIsNone(mean64([]))

    def test_mean_rejects_an_un_normalised_float_in_its_key(self):
        with self.assertRaisesRegex(NormalizationError, "un-normalised"):
            mean64([(["raw", 1.0], n64(1.0))])

    def test_noncanonical_or_nonfinite_n64_is_rejected(self):
        for token in (
            "f64:8000000000000000",  # negative zero
            "f64:7ff0000000000000",  # +inf
            "f64:7ff8000000000000",  # NaN
            "f64:3FC999999999999A",  # uppercase
        ):
            with self.subTest(token=token):
                with self.assertRaises(NumericAuthorityError):
                    exact_n64(token)


class P95Type7Tests(unittest.TestCase):
    """§12 Type-7 p95, diagnostic only."""

    @staticmethod
    def _n(value):
        return rn64(Fraction(value))

    def test_quantile_is_the_exact_rational_not_binary64(self):
        """The normative decision, pinned so it cannot drift silently.

        §12 writes "q=0.95" without saying which number that is.  The two
        candidates are observably different: binary64(0.95) is strictly below
        19/20, so whenever G-1 is a multiple of 20 it moves h off the integer
        and forces an interpolation where Type-7 must select x[j] outright.
        The authority chose the exact rational on 2026-08-05.
        """
        self.assertEqual(P95_QUANTILE, Fraction(19, 20))
        self.assertNotEqual(
            P95_QUANTILE, Fraction(*(0.95).as_integer_ratio()))

    def test_contract_edge_cases(self):
        self.assertIsNone(p95_type7_64([]))
        self.assertEqual(p95_type7_64([self._n(7)]), self._n(7))

    def test_integral_h_selects_the_element_without_interpolating(self):
        """G-1 multiple of 20 is where the q choice becomes visible.

        With q=19/20 and G=21, h=19 exactly, so the result is x[19] itself.
        Under binary64(0.95) it would interpolate between x[18] and x[19] and
        publish a different binary64 on a tail jump — the case that motivated
        pinning the quantile.
        """
        values = [self._n(0)] * 19 + [self._n(10 ** 6), self._n(10 ** 6)]
        self.assertEqual(p95_type7_64(values), self._n(10 ** 6))
        binary64_quantile = Fraction(*(0.95).as_integer_ratio())
        position = Fraction(len(values) - 1) * binary64_quantile
        self.assertNotEqual(position, position.numerator // position.denominator)

    def test_matches_an_independently_written_exact_oracle(self):
        def oracle(tokens):
            ordered = sorted(exact_n64(token) for token in tokens)
            count = len(ordered)
            if count == 0:
                return None
            if count == 1:
                return rn64(ordered[0])
            position = Fraction(count - 1) * Fraction(19, 20)
            index = position.__floor__()
            gamma = position - index
            if gamma == 0:
                return rn64(ordered[index])
            return rn64(
                (1 - gamma) * ordered[index] + gamma * ordered[index + 1])

        generator = random.Random(20260805)
        for trial in range(200):
            count = generator.randint(1, 60)
            tokens = [
                self._n(Fraction(
                    generator.randint(-10 ** 6, 10 ** 6),
                    generator.randint(1, 997),
                ))
                for _ in range(count)
            ]
            with self.subTest(trial=trial, count=count):
                self.assertEqual(p95_type7_64(tokens), oracle(tokens))

    def test_result_is_independent_of_input_order(self):
        base = [self._n(3), self._n(1), self._n(2), self._n(5), self._n(4)]
        results = {
            p95_type7_64(list(permutation))
            for permutation in itertools.permutations(base)
        }
        self.assertEqual(len(results), 1)

    def test_rejects_malformed_tokens(self):
        with self.assertRaises(NumericAuthorityError):
            p95_type7_64(["f64:zzzzzzzzzzzzzzzz"])


class SupportAccountingTests(unittest.TestCase):
    """§12 G_eligible / G_defined / G_NA."""

    ELIGIBLE = (
        ("g2", ("unit", "a")),
        ("g2", ("unit", "b")),
        ("g1", ("unit", "c")),
        ("g3", ("unit", "d")),
    )
    OUTCOMES = (
        UnitOutcome("g2", ("unit", "a"), True),
        UnitOutcome("g2", ("unit", "b"), False, "INSUFFICIENT_MATCHED_SUPPORT"),
        UnitOutcome("g1", ("unit", "c"), False, "SPEARMAN_UNDEFINED"),
        UnitOutcome("g3", ("unit", "d"), False, "INSUFFICIENT_MATCHED_SUPPORT"),
    )

    def test_g_na_is_exactly_the_set_difference(self):
        result = account_support(self.ELIGIBLE, self.OUTCOMES)
        self.assertEqual(
            set(result.g_na),
            set(result.g_eligible) - set(result.g_defined),
        )
        self.assertFalse(set(result.g_defined) & set(result.g_na))

    def test_one_defined_unit_makes_the_whole_group_defined(self):
        """§12: G_defined counts groups with *at least one* defined unit.

        g2 holds one defined and one N/A unit.  It belongs to G_defined and
        must not also appear in G_NA — an N/A unit does not make its group
        N/A, and double-counting it would inflate both populations.
        """
        result = account_support(self.ELIGIBLE, self.OUTCOMES)
        self.assertIn("g2", result.g_defined)
        self.assertNotIn("g2", result.g_na)
        self.assertEqual(result.g_defined, ("g2",))
        self.assertEqual(result.g_na, ("g1", "g3"))

    def test_outcome_outside_the_frozen_eligible_set_is_rejected(self):
        """The anti-gaming core: the denominator cannot grow after scoring."""
        with self.assertRaisesRegex(SupportError, "outside the frozen"):
            account_support(
                self.ELIGIBLE,
                self.OUTCOMES + (UnitOutcome("g9", ("unit", "x"), True),),
            )

    def test_missing_outcome_is_a_failure_not_an_implicit_na(self):
        """Dropping a unit must not be a silent way to shrink the sample."""
        with self.assertRaisesRegex(SupportError, "no outcome"):
            account_support(self.ELIGIBLE, self.OUTCOMES[:3])

    def test_duplicate_outcome_is_rejected(self):
        with self.assertRaisesRegex(SupportError, "duplicate outcome"):
            account_support(
                self.ELIGIBLE, self.OUTCOMES + (self.OUTCOMES[0],))

    def test_reason_code_is_required_exactly_when_not_defined(self):
        with self.assertRaisesRegex(SupportError, "non-empty reason"):
            UnitOutcome("g", ("unit",), False)
        with self.assertRaisesRegex(SupportError, "must not carry"):
            UnitOutcome("g", ("unit",), True, "SOME_REASON")

    def test_group_reasons_are_a_set_not_an_elected_code(self):
        """§12 pins no precedence among disagreeing units, so none is invented.

        A group whose units fail for different reasons reports both, ordered
        canonically.  A set is a superset of whatever single-code rule a
        later amendment fixes.
        """
        eligible = (("g", ("unit", "a")), ("g", ("unit", "b")))
        outcomes = (
            UnitOutcome("g", ("unit", "a"), False, "SPEARMAN_UNDEFINED"),
            UnitOutcome("g", ("unit", "b"), False, "PAIRING_AMBIGUOUS"),
        )
        result = account_support(eligible, outcomes)
        self.assertEqual(
            result.na_reasons,
            (("g", ("PAIRING_AMBIGUOUS", "SPEARMAN_UNDEFINED")),),
        )

    def test_result_is_independent_of_input_order(self):
        results = {
            account_support(tuple(permutation), self.OUTCOMES).g_na
            for permutation in itertools.permutations(self.ELIGIBLE)
        }
        self.assertEqual(len(results), 1)

    def test_authority_status_declares_floors_unevaluated(self):
        """Support is accounted; §12's gate floors are a separate, absent step."""
        result = account_support(self.ELIGIBLE, self.OUTCOMES)
        self.assertEqual(
            result.authority_status,
            "SUPPORT_ACCOUNTED_GATE_FLOORS_NOT_EVALUATED",
        )

    def test_module_is_unreachable_from_the_rev7_dispatcher(self):
        for symbol in (
            "account_support",
            "aggregate_unit_metric_values",
            "UnitOutcome",
            "MetricUnitResult",
            "SupportAccounting",
            "HierarchicalMetricAggregation",
        ):
            self.assertFalse(
                hasattr(contracts_pkg, symbol),
                f"{symbol} must not be exported by ml_v3.contracts",
            )


class HierarchicalMetricAggregationTests(unittest.TestCase):
    """§12 defined unit -> group -> equal-weight macro reduction."""

    @staticmethod
    def _n(value):
        return rn64(Fraction(value))

    def test_groups_have_equal_weight_not_unit_weight(self):
        """Three units in one group cannot outweigh one independent group."""
        eligible = (
            ("many", ("unit", "a")),
            ("many", ("unit", "b")),
            ("many", ("unit", "c")),
            ("one", ("unit", "d")),
        )
        results = (
            MetricUnitResult("many", ("unit", "a"), self._n(0)),
            MetricUnitResult("many", ("unit", "b"), self._n(0)),
            MetricUnitResult("many", ("unit", "c"), self._n(0)),
            MetricUnitResult("one", ("unit", "d"), self._n(1)),
        )
        aggregate = aggregate_unit_metric_values(eligible, results)
        self.assertEqual(
            aggregate.group_values,
            (("many", self._n(0)), ("one", self._n(1))),
        )
        self.assertEqual(aggregate.macro_mean64, self._n(Fraction(1, 2)))
        self.assertNotEqual(aggregate.macro_mean64, self._n(Fraction(1, 4)))

    def test_na_unit_is_excluded_not_converted_to_zero(self):
        eligible = (
            ("g", ("unit", "defined")),
            ("g", ("unit", "na")),
        )
        results = (
            MetricUnitResult("g", ("unit", "defined"), self._n(1)),
            MetricUnitResult(
                "g", ("unit", "na"), na_reason="PAIRING_ENVELOPE_UNAVAILABLE"
            ),
        )
        aggregate = aggregate_unit_metric_values(eligible, results)
        self.assertEqual(aggregate.group_values, (("g", self._n(1)),))
        self.assertEqual(aggregate.macro_mean64, self._n(1))
        self.assertEqual(aggregate.support.defined_unit_count, 1)
        self.assertEqual(aggregate.support.na_unit_count, 1)

    def test_all_na_is_published_as_no_macro_value(self):
        eligible = (
            ("g1", ("unit", "a")),
            ("g2", ("unit", "b")),
        )
        results = (
            MetricUnitResult("g1", ("unit", "a"), na_reason="NO_SUPPORT"),
            MetricUnitResult("g2", ("unit", "b"), na_reason="NO_SUPPORT"),
        )
        aggregate = aggregate_unit_metric_values(eligible, results)
        self.assertEqual(aggregate.support.g_defined, ())
        self.assertEqual(aggregate.support.g_na, ("g1", "g2"))
        self.assertEqual(aggregate.group_values, ())
        self.assertIsNone(aggregate.macro_mean64)
        self.assertIsNone(aggregate.p95_group64)

    def test_p95_is_diagnostic_over_defined_group_values(self):
        eligible = tuple(
            (f"g{index}", ("unit", str(index))) for index in range(3)
        ) + (("g-na", ("unit", "na")),)
        results = tuple(
            MetricUnitResult(
                f"g{index}", ("unit", str(index)), self._n(index)
            )
            for index in range(3)
        ) + (
            MetricUnitResult("g-na", ("unit", "na"), na_reason="NO_SUPPORT"),
        )
        aggregate = aggregate_unit_metric_values(eligible, results)
        self.assertEqual(
            aggregate.p95_group64,
            p95_type7_64([self._n(0), self._n(1), self._n(2)]),
        )
        self.assertEqual(
            aggregate.authority_status,
            AGGREGATION_AUTHORITY_STATUS,
        )
        self.assertIn("GATE_FLOORS_NOT_EVALUATED", aggregate.authority_status)

    def test_result_is_independent_of_eligible_and_result_order(self):
        eligible = (
            ("g2", ("unit", "b")),
            ("g1", ("unit", "a")),
            ("g2", ("unit", "c")),
        )
        results = (
            MetricUnitResult("g2", ("unit", "c"), self._n(3)),
            MetricUnitResult("g1", ("unit", "a"), self._n(7)),
            MetricUnitResult("g2", ("unit", "b"), self._n(1)),
        )
        expected = aggregate_unit_metric_values(eligible, results)
        for eligible_order in itertools.permutations(eligible):
            for result_order in itertools.permutations(results):
                with self.subTest(
                    eligible=eligible_order,
                    results=result_order,
                ):
                    self.assertEqual(
                        aggregate_unit_metric_values(
                            tuple(eligible_order), tuple(result_order)
                        ),
                        expected,
                    )

    def test_missing_extra_and_duplicate_units_fail_closed(self):
        eligible = (("g", ("unit", "a")),)
        value = MetricUnitResult("g", ("unit", "a"), self._n(1))
        with self.assertRaisesRegex(SupportError, "no outcome"):
            aggregate_unit_metric_values(eligible, ())
        with self.assertRaisesRegex(SupportError, "outside the frozen"):
            aggregate_unit_metric_values(
                eligible,
                (value, MetricUnitResult("x", ("unit", "x"), self._n(1))),
            )
        with self.assertRaisesRegex(SupportError, "duplicate outcome"):
            aggregate_unit_metric_values(eligible, (value, value))

    def test_unit_state_is_an_exact_tagged_union(self):
        with self.assertRaisesRegex(SupportError, "exactly one"):
            MetricUnitResult("g", ("unit",))
        with self.assertRaisesRegex(SupportError, "exactly one"):
            MetricUnitResult(
                "g",
                ("unit",),
                self._n(1),
                "MUST_NOT_EXIST_WITH_VALUE",
            )
        with self.assertRaisesRegex(SupportError, "canonical finite N64"):
            MetricUnitResult("g", ("unit",), "f64:7ff0000000000000")

    def test_non_tuple_results_fail_closed(self):
        with self.assertRaisesRegex(SupportError, "must be a tuple"):
            aggregate_unit_metric_values((), [])


class SupportFloorPolicyTests(unittest.TestCase):
    """O13F_01 hash binding and support-floor blast radius."""

    @staticmethod
    def _groups(count, prefix="g"):
        return tuple(f"{prefix}{index:03}" for index in range(count))

    @classmethod
    def _accounting(cls, eligible_count, defined_count):
        groups = cls._groups(eligible_count)
        eligible = tuple((group, ("unit", group)) for group in groups)
        outcomes = tuple(
            UnitOutcome(
                group,
                ("unit", group),
                index < defined_count,
                None if index < defined_count else "NO_SUPPORT",
            )
            for index, group in enumerate(groups)
        )
        return account_support(eligible, outcomes)

    @staticmethod
    def _stratum(
        stratum_id,
        population_kind,
        contract_floor,
        *,
        parent=None,
        power_required=False,
        n_power=None,
        numerator=None,
        denominator=None,
    ):
        required = max(contract_floor, n_power) if power_required else contract_floor
        return {
            "stratum_id": stratum_id,
            "population_kind": population_kind,
            "parent_stratum_id": parent,
            "contract_floor": contract_floor,
            "power_required": power_required,
            "n_power": n_power,
            "n_required": required,
            "max_parent_fraction_numerator": numerator,
            "max_parent_fraction_denominator": denominator,
        }

    @classmethod
    def _policy(cls, strata, populations, *, power_hash=None):
        return {
            "schema": POLICY_SCHEMA,
            "contract_revision": POLICY_REVISION,
            "source_contract_sha256": SOURCE_CONTRACT_SHA256,
            "metric_id": "average_precision:Resonance",
            "split_role": "final-test",
            "mandatory": True,
            "strata": sorted(strata, key=lambda item: item["stratum_id"].encode("utf-8")),
            "population_plan_sha256": stratum_population_plan_sha256(
                "average_precision:Resonance",
                "final-test",
                tuple(populations),
            ),
            "power_plan_sha256": power_hash,
        }

    @classmethod
    def _evaluate(cls, policy, support, populations):
        return evaluate_support_floors(
            policy,
            support_floor_policy_sha256(policy),
            support,
            tuple(populations),
            metric_id="average_precision:Resonance",
            split_role="final-test",
            power_plan_sha256=policy["power_plan_sha256"],
        )

    def test_source_contract_digest_matches_repository_bytes(self):
        contract = (
            Path(__file__).resolve().parents[2]
            / "docs"
            / "MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md"
        )
        self.assertEqual(
            hashlib.sha256(contract.read_bytes()).hexdigest(),
            SOURCE_CONTRACT_SHA256,
        )

    def test_contract_floor_under_on_over(self):
        for count, sufficient in ((29, False), (30, True), (31, True)):
            with self.subTest(count=count):
                support = self._accounting(count, count)
                populations = (
                    StratumPopulation("overall", support.g_eligible),
                )
                policy = self._policy((
                    self._stratum("overall", "all_eligible_groups", 30),
                ), populations)
                result = self._evaluate(
                    policy,
                    support,
                    populations,
                )
                self.assertEqual(result.support_sufficient, sufficient)
                self.assertEqual(
                    result.status,
                    "SUPPORT_SUFFICIENT" if sufficient else "SUPPORT_INSUFFICIENT",
                )

    def test_defined_floor_is_not_replaced_by_eligible_floor(self):
        support = self._accounting(30, 29)
        populations = (StratumPopulation("overall", support.g_eligible),)
        policy = self._policy((
            self._stratum("overall", "all_eligible_groups", 30),
        ), populations)
        result = self._evaluate(
            policy,
            support,
            populations,
        )
        self.assertFalse(result.support_sufficient)
        self.assertNotIn(
            "SUPPORT_ELIGIBLE_FLOOR_NOT_MET", result.reason_codes
        )
        self.assertIn("SUPPORT_DEFINED_FLOOR_NOT_MET", result.reason_codes)

    def test_power_floor_uses_the_maximum(self):
        power_hash = "1" * 64
        support = self._accounting(44, 44)
        populations = (StratumPopulation("overall", support.g_eligible),)
        policy = self._policy((
            self._stratum(
                "overall",
                "all_eligible_groups",
                30,
                power_required=True,
                n_power=45,
            ),
        ), populations, power_hash=power_hash)
        self.assertEqual(policy["strata"][0]["n_required"], 45)
        result = self._evaluate(
            policy,
            support,
            populations,
        )
        self.assertFalse(result.support_sufficient)

    def test_missing_policy_is_na_and_never_falls_back(self):
        support = self._accounting(149, 149)
        result = evaluate_support_floors(
            None,
            None,
            support,
            (),
            metric_id="average_precision:Resonance",
            split_role="final-test",
        )
        self.assertEqual(result.status, "SUPPORT_POLICY_UNAVAILABLE")
        self.assertFalse(result.support_sufficient)
        self.assertEqual(result.reason_codes, ("SUPPORT_POLICY_UNAVAILABLE",))

    def test_hash_mismatch_and_extra_policy_key_are_fatal(self):
        support = self._accounting(1, 1)
        populations = (StratumPopulation("overall", support.g_eligible),)
        policy = self._policy((
            self._stratum("overall", "all_eligible_groups", 1),
        ), populations)
        with self.assertRaisesRegex(SupportFloorError, "HASH_MISMATCH"):
            evaluate_support_floors(
                policy,
                "0" * 64,
                support,
                populations,
                metric_id=policy["metric_id"],
                split_role=policy["split_role"],
            )
        malformed = dict(policy)
        malformed["candidate_override"] = 0
        with self.assertRaisesRegex(SupportFloorError, "exact-key"):
            support_floor_policy_sha256(malformed)

    def test_power_binding_cannot_be_ignored_or_fabricated(self):
        populations = (StratumPopulation("overall", ()),)
        powered = self._policy((
            self._stratum(
                "overall", "all_eligible_groups", 30,
                power_required=True, n_power=45,
            ),
        ), populations, power_hash="2" * 64)
        powered["strata"][0]["n_required"] = 30
        with self.assertRaisesRegex(SupportFloorError, "must equal max"):
            support_floor_policy_sha256(powered)

        unpowered = self._policy((
            self._stratum("overall", "all_eligible_groups", 30),
        ), populations)
        unpowered["power_plan_sha256"] = "3" * 64
        with self.assertRaisesRegex(SupportFloorError, "must be null"):
            support_floor_policy_sha256(unpowered)

    def test_required_power_plan_must_be_present_and_hash_identical(self):
        power_hash = "4" * 64
        support = self._accounting(45, 45)
        populations = (StratumPopulation("overall", support.g_eligible),)
        policy = self._policy((
            self._stratum(
                "overall", "all_eligible_groups", 30,
                power_required=True, n_power=45,
            ),
        ), populations, power_hash=power_hash)
        policy_hash = support_floor_policy_sha256(policy)

        unavailable = evaluate_support_floors(
            policy,
            policy_hash,
            support,
            populations,
            metric_id=policy["metric_id"],
            split_role=policy["split_role"],
        )
        self.assertEqual(unavailable.status, "SUPPORT_POLICY_UNAVAILABLE")
        self.assertFalse(unavailable.support_sufficient)

        with self.assertRaisesRegex(
            SupportFloorError, "POWER_PLAN_HASH_MISMATCH"
        ):
            evaluate_support_floors(
                policy,
                policy_hash,
                support,
                populations,
                metric_id=policy["metric_id"],
                split_role=policy["split_role"],
                power_plan_sha256="5" * 64,
            )

    def test_profile_floor_is_not_hidden_by_sufficient_total(self):
        overall = self._groups(149)
        profile = overall[:9]
        populations = (
            StratumPopulation("overall", overall),
            StratumPopulation("profile:bass", profile),
        )
        policy = self._policy((
            self._stratum("overall", "all_eligible_groups", 149),
            self._stratum(
                "profile:bass", "profile_groups", 10, parent="overall"
            ),
        ), populations)
        support = self._accounting(149, 149)
        result = self._evaluate(policy, support, populations)
        self.assertFalse(result.support_sufficient)
        self.assertIn("SUPPORT_STRATUM_FLOOR_NOT_MET", result.reason_codes)

    def test_source_family_ceiling_boundary_15_of_30_vs_16_of_30(self):
        overall = self._groups(30)
        support = self._accounting(30, 30)
        for count, sufficient in ((15, True), (16, False)):
            with self.subTest(count=count):
                populations = (
                    StratumPopulation("overall", overall),
                    StratumPopulation("source:dominant", overall[:count]),
                )
                policy = self._policy((
                    self._stratum("overall", "all_eligible_groups", 30),
                    self._stratum(
                        "source:dominant",
                        "source_family_groups",
                        5,
                        parent="overall",
                        numerator=1,
                        denominator=2,
                    ),
                ), populations)
                result = self._evaluate(policy, support, populations)
                self.assertEqual(result.support_sufficient, sufficient)

    def test_populations_must_match_policy_and_frozen_eligible_set(self):
        support = self._accounting(1, 1)
        strata = (
            self._stratum("overall", "all_eligible_groups", 1),
        )
        populations = (StratumPopulation("overall", support.g_eligible),)
        policy = self._policy(strata, populations)
        with self.assertRaisesRegex(
            SupportFloorError, "POPULATION_PLAN_HASH_MISMATCH"
        ):
            self._evaluate(policy, support, ())

        missing_policy = self._policy(strata, ())
        with self.assertRaisesRegex(SupportFloorError, "match policy"):
            self._evaluate(missing_policy, support, ())

        outside = (StratumPopulation("overall", ("outside",)),)
        outside_policy = self._policy(strata, outside)
        with self.assertRaisesRegex(SupportFloorError, "ineligible groups"):
            self._evaluate(outside_policy, support, outside)

    def test_population_and_policy_order_do_not_change_result(self):
        overall = self._groups(30)
        child = overall[:10]
        strata = (
            self._stratum("overall", "all_eligible_groups", 30),
            self._stratum("profile:bass", "profile_groups", 10, parent="overall"),
        )
        support = self._accounting(30, 30)
        populations = (
            StratumPopulation("overall", overall),
            StratumPopulation("profile:bass", child),
        )
        policy = self._policy(strata, populations)
        expected = self._evaluate(policy, support, populations)
        actual = self._evaluate(policy, support, (
            StratumPopulation("profile:bass", tuple(reversed(child))),
            StratumPopulation("overall", tuple(reversed(overall))),
        ))
        self.assertEqual(actual, expected)
        self.assertEqual(actual.authority_status, O13F_AUTHORITY_STATUS)

    def test_population_membership_is_hash_bound_before_predictions(self):
        overall = self._groups(30)
        frozen = (
            StratumPopulation("overall", overall),
            StratumPopulation("profile:bass", overall[:10]),
        )
        policy = self._policy((
            self._stratum("overall", "all_eligible_groups", 30),
            self._stratum("profile:bass", "profile_groups", 10, parent="overall"),
        ), frozen)
        support = self._accounting(30, 30)
        altered = (
            StratumPopulation("overall", overall),
            StratumPopulation("profile:bass", overall[1:11]),
        )
        with self.assertRaisesRegex(
            SupportFloorError, "POPULATION_PLAN_HASH_MISMATCH"
        ):
            self._evaluate(policy, support, altered)

    def test_forged_support_accounting_is_rejected(self):
        support = SupportAccounting(
            g_eligible=("g000",),
            g_defined=("g000",),
            g_na=(),
            na_reasons=(),
            defined_unit_count=0,
            na_unit_count=0,
        )
        populations = (StratumPopulation("overall", support.g_eligible),)
        policy = self._policy((
            self._stratum("overall", "all_eligible_groups", 1),
        ), populations)
        with self.assertRaisesRegex(SupportFloorError, "ACCOUNTING_INVALID"):
            self._evaluate(policy, support, populations)

    def test_module_is_unreachable_from_rev7_dispatcher(self):
        for symbol in (
            "evaluate_support_floors",
            "support_floor_policy_sha256",
            "stratum_population_plan_sha256",
            "SupportFloorEvaluation",
            "StratumPopulation",
        ):
            self.assertFalse(hasattr(contracts_pkg, symbol))


if __name__ == "__main__":
    unittest.main()
