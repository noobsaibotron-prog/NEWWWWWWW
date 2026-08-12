"""REV8 O13F06 candidate-only registry conformance falsification tests."""
from __future__ import annotations

import copy
import shutil
import tempfile
import unittest
from pathlib import Path

import ml_v3.contracts as contracts_pkg
from ml_v3.contracts.canonical import canonical_bytes, sha256_of_obj
from ml_v3.contracts.registry_conformance_v2 import (
    AUTHORITY_STATUS,
    CONFORMANCE_FILENAMES,
    PACKAGE_MANIFEST_SHA256,
    RegistryConformanceError,
    apply_conformance_mutation,
    compare_registry_instance_oracle,
    expand_registry_instance,
    load_conformance_package,
    run_conformance_mutations,
    validate_conformance_package,
)

REPO = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = REPO / "docs" / "data" / "rev8" / "o13_registry_conformance_v2"

EXPECTED_INSTANCE_SHA256 = {
    "calibration": (
        "7ec116138426c3613c3fbb28f9c5805f7e50cdbe9428d36516d6b4996c3af61f"
    ),
    "development-metric": (
        "587c5a4e4e97f6db392f5a6f1615302a4b5aca5d5a21767c9b369d2e3bc087c3"
    ),
    "final-test": (
        "75de3d0e6aea3bce596a4ffc9947bfc012af61dd3f2177bb03977e6ef0c77bf3"
    ),
}

INSTANCE_KEY = {
    "calibration": "calibration_instance",
    "development-metric": "development_instance",
    "final-test": "final_instance",
}


class RegistryConformancePackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.package = load_conformance_package(PACKAGE_ROOT)

    def test_signed_package_loads_and_validates(self):
        self.assertEqual(
            self.package["_manifest_sha256"], PACKAGE_MANIFEST_SHA256
        )
        self.assertEqual(self.package["_authority_status"], AUTHORITY_STATUS)
        validate_conformance_package(self.package)

    def test_candidate_module_is_not_activated_by_package_init(self):
        self.assertNotIn("registry_conformance_v2", contracts_pkg.__all__)
        self.assertFalse(hasattr(contracts_pkg, "expand_registry_instance"))

    def test_loader_rejects_manifest_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "package"
            shutil.copytree(PACKAGE_ROOT, root)
            manifest = root / "SHA256SUMS"
            manifest.write_bytes(manifest.read_bytes() + b"\n")
            with self.assertRaisesRegex(
                RegistryConformanceError,
                "CONFORMANCE_MANIFEST_DIGEST_MISMATCH",
            ):
                load_conformance_package(root)

    def test_loader_rejects_extra_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "package"
            shutil.copytree(PACKAGE_ROOT, root)
            (root / "unexpected.json").write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(
                RegistryConformanceError,
                "CONFORMANCE_PACKAGE_FILE_SET_MISMATCH",
            ):
                load_conformance_package(root)

    def test_manifest_covers_every_json_exactly_once(self):
        self.assertEqual(
            set(self.package["_manifest"]), set(CONFORMANCE_FILENAMES.values())
        )

    def test_post_load_companion_artifact_mutation_is_rejected(self):
        mutated = copy.deepcopy(self.package)
        mutated["scope_matrix"]["checksum"] = "0" * 64
        with self.assertRaisesRegex(
            RegistryConformanceError, "REGISTRY_SCOPE_MATRIX_MISMATCH"
        ):
            validate_conformance_package(
                mutated, reference_package=self.package
            )


class RegistryExpansionOracleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.package = load_conformance_package(PACKAGE_ROOT)
        cls.inputs = {
            row["split_role"]: row
            for row in cls.package["input_facts"]["split_inputs"]
        }

    def _expand(self, role: str, split_input: dict | None = None) -> dict:
        return expand_registry_instance(
            self.package["definition"],
            self.package["claim_plan"],
            self.inputs[role] if split_input is None else split_input,
            readiness=None if role == "calibration" else self.package["readiness"],
        )

    def test_definition_plus_claim_reproduce_all_three_instances_bit_exactly(self):
        for role, instance_key in INSTANCE_KEY.items():
            with self.subTest(role=role):
                expanded = self._expand(role)
                frozen = self.package[instance_key]
                compare_registry_instance_oracle(expanded, frozen)
                self.assertEqual(canonical_bytes(expanded), canonical_bytes(frozen))
                self.assertEqual(
                    sha256_of_obj(expanded), EXPECTED_INSTANCE_SHA256[role]
                )

    def test_expansion_is_independent_of_fixture_domain_input_order(self):
        expanded_fields = (
            "expanded_support_templates",
            "expanded_bindings",
            "expanded_calibrator_fit_bindings",
            "expanded_calibrator_dependencies",
            "expanded_benchmark_readiness_bindings",
            "expanded_diagnostic_breakdowns",
            "expanded_metric_stratum_power_bindings",
            "expanded_preconditions",
        )
        for role in INSTANCE_KEY:
            with self.subTest(role=role):
                split_input = copy.deepcopy(self.inputs[role])
                domains = split_input["evaluation_unit_index"]["expansion_domains"]
                domains["profiles"].reverse()
                domains["electronic_subgenres"].reverse()
                for families in domains["source_families_by_problem_type"].values():
                    families.reverse()
                reordered = self._expand(role, split_input)
                frozen = self.package[INSTANCE_KEY[role]]
                for field in expanded_fields:
                    self.assertEqual(reordered[field], frozen[field], field)
                # The input fact itself changed bytes, so provenance must not
                # be normalized away even though expansion order is stable.
                self.assertNotEqual(
                    reordered["evaluation_unit_index_sha256"],
                    frozen["evaluation_unit_index_sha256"],
                )

    def test_unicode_subgenre_identity_is_byte_exact_not_normalized(self):
        rows = self.package["final_instance"]["expanded_diagnostic_breakdowns"]
        first_blueprint = rows[0]["diagnostic_breakdown_blueprint_id"]
        values = {
            row["population_selector"]["electronic_subgenre"]
            for row in rows
            if row["diagnostic_breakdown_blueprint_id"] == first_blueprint
        }
        self.assertIn("e\u0301", values)
        self.assertIn("é", values)
        self.assertIn(None, values)
        self.assertEqual(len(values), 7)

    def test_oracle_rejects_even_semantically_harmless_array_reordering(self):
        actual = copy.deepcopy(self.package["final_instance"])
        actual["expanded_bindings"].reverse()
        with self.assertRaisesRegex(
            RegistryConformanceError, "REGISTRY_INSTANCE_ORACLE_MISMATCH"
        ):
            compare_registry_instance_oracle(
                actual, self.package["final_instance"]
            )

    def test_official_input_projection_is_rejected(self):
        split_input = copy.deepcopy(self.inputs["final-test"])
        split_input["asset_manifest"]["fixture_only"] = False
        with self.assertRaisesRegex(
            RegistryConformanceError, "CONFORMANCE_INPUT_PROMOTION_FORBIDDEN"
        ):
            self._expand("final-test", split_input)


class RegistryMutationOracleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.package = load_conformance_package(PACKAGE_ROOT)
        cls.recipes = cls.package["mutations"]["mutations"]

    def test_signed_oracle_contains_exactly_31_unique_fail_recipes(self):
        self.assertEqual(len(self.recipes), 31)
        self.assertEqual(len({row["mutation_id"] for row in self.recipes}), 31)
        self.assertEqual({row["expected_outcome"] for row in self.recipes}, {"FAIL"})

    def test_all_31_mutations_fail_with_the_signed_reason_code(self):
        results = run_conformance_mutations(self.package)
        self.assertEqual(len(results), 31)
        failures = [result for result in results if not result.passed]
        self.assertEqual(
            failures,
            [],
            msg="; ".join(
                f"{result.mutation_id}: expected "
                f"{result.expected_reason_code}, got {result.observed_reason_code}"
                for result in failures
            ),
        )

    def test_each_mutation_is_rejected_when_executed_individually(self):
        for recipe in self.recipes:
            with self.subTest(mutation_id=recipe["mutation_id"]):
                mutated = apply_conformance_mutation(self.package, recipe)
                with self.assertRaises(RegistryConformanceError) as caught:
                    validate_conformance_package(
                        mutated, reference_package=self.package
                    )
                self.assertEqual(
                    caught.exception.reason_code,
                    recipe["expected_reason_code"],
                )

    def test_mutations_do_not_modify_pristine_package(self):
        pristine_sha = {
            key: sha256_of_obj(self.package[key])
            for key in CONFORMANCE_FILENAMES
        }
        for recipe in self.recipes:
            apply_conformance_mutation(self.package, recipe)
        self.assertEqual(
            pristine_sha,
            {
                key: sha256_of_obj(self.package[key])
                for key in CONFORMANCE_FILENAMES
            },
        )

    def test_unicode_normalization_recipe_has_specific_identity_failure(self):
        recipe = next(
            row for row in self.recipes
            if row["mutation_id"] == "mutation.final_instance.normalize_unicode"
        )
        mutated = apply_conformance_mutation(self.package, recipe)
        with self.assertRaisesRegex(
            RegistryConformanceError, "ELECTRONIC_SUBGENRE_IDENTITY_MISMATCH"
        ):
            validate_conformance_package(mutated, reference_package=self.package)


if __name__ == "__main__":
    unittest.main()
