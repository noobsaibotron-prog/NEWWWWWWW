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

import itertools
import random
import struct
import unittest
from fractions import Fraction

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


if __name__ == "__main__":
    unittest.main()
