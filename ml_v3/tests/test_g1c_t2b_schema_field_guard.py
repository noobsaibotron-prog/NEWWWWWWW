"""G1c T2b — evaluator field reads must match the contract schemas.

This is the mechanical guard added after the T2 false-green: tests must not
prove evaluator code against hand-shaped fixtures only. If a module reads fields
from a record type, it declares those fields and this test validates them
against the normative key-sets in ``contracts/schemas.py``.
"""
from __future__ import annotations

import unittest

from ml_v3.benchmark.evaluator_parser import EVALUATOR_PARSER_SCHEMA_FIELD_CLAIMS
from ml_v3.benchmark.event_matching import EVENT_MATCHING_SCHEMA_FIELD_CLAIMS
from ml_v3.contracts.metrology_lock import gate_platform_python_label
from ml_v3.contracts.schema_field_guard import (
    SchemaFieldClaim,
    SchemaFieldClaimError,
    fields_for_schema_record,
    validate_schema_field_claims,
)


class SchemaFieldGuardTests(unittest.TestCase):
    def test_gate_platform(self):
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")

    def test_current_evaluator_modules_declare_schema_valid_field_reads(self):
        validate_schema_field_claims(EVALUATOR_PARSER_SCHEMA_FIELD_CLAIMS)
        validate_schema_field_claims(EVENT_MATCHING_SCHEMA_FIELD_CLAIMS)

    def test_event_matcher_is_bound_to_event_schemas_not_semantic_region_shape(self):
        records = {claim.schema_record for claim in EVENT_MATCHING_SCHEMA_FIELD_CLAIMS}
        self.assertEqual(records, {"dynamic_event", "prediction_event"})
        for claim in EVENT_MATCHING_SCHEMA_FIELD_CLAIMS:
            self.assertNotIn("band_lo_hz", claim.fields)
            self.assertNotIn("band_hi_hz", claim.fields)
            self.assertNotIn("direction", claim.fields)

    def test_guard_refutes_original_t2_band_field_mismatch(self):
        bad_claim = SchemaFieldClaim(
            owner="regression.original_t2",
            schema_record="dynamic_event",
            fields=frozenset({"problem_type", "band_lo_hz", "band_hi_hz"}),
            purpose="mistakenly treating §10.2 events as §10.1 regions",
        )
        with self.assertRaisesRegex(
                SchemaFieldClaimError, "dynamic_event lacks fields"):
            validate_schema_field_claims([bad_claim])

    def test_guard_refutes_prediction_semantic_bundle_direction_mismatch(self):
        bad_claim = SchemaFieldClaim(
            owner="regression.original_t2",
            schema_record="semantic_bundle",
            fields=frozenset({"problem_type", "direction"}),
            purpose="mistakenly requiring prediction bundle direction",
        )
        with self.assertRaisesRegex(
                SchemaFieldClaimError, "semantic_bundle lacks fields"):
            validate_schema_field_claims([bad_claim])

    def test_unknown_schema_record_is_fail_closed(self):
        with self.assertRaisesRegex(SchemaFieldClaimError, "unknown schema record"):
            fields_for_schema_record("not_a_schema_record")


if __name__ == "__main__":
    unittest.main()
