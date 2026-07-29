"""REV8 O-02/O-03/O-18 artifacts, projection and falsification tests."""
from __future__ import annotations

import copy
import math
import struct
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path
from typing import Any
from unittest.mock import patch

import ml_v3.contracts as contracts_pkg
import ml_v3.contracts.frequency_geometry_v2 as frequency_geometry
import ml_v3.contracts.numeric_artifact_v2 as numeric_artifact
from ml_v3.contracts.canonical import (
    canonical_bytes,
    load_strict,
    sha256_of_file,
    sha256_of_obj,
    write_canonical,
)
from ml_v3.contracts.exact_arith_v2 import exact
from ml_v3.contracts.frequency_geometry_v2 import (
    BOUNDARY_ARTIFACT_PATH,
    WIDTH_ARTIFACT_PATH,
    FrequencyGeometryError,
    build_boundary_artifact,
    build_width_artifact,
    derive_endpoint_tokens_v1,
    load_boundary_artifact,
    load_width_artifact,
    project_center_width_v1,
    project_interval_n64,
    project_n64,
)
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.contracts.normalize_v2 import n64
from ml_v3.contracts.numeric_artifact_v2 import (
    NUMERIC_ARTIFACT_PATH,
    build_numeric_artifact,
    load_numeric_artifact,
)
from ml_v3.contracts.numeric_authority_v2 import (
    exact_n64,
    float_from_n64,
    mean64,
    n64_from_bits,
)

_REPORT_PATH = (
    Path(__file__).resolve().parents[1]
    / "fixtures"
    / "rev8"
    / "o02_o03_o18_generator_report_v1.json"
)


def _assert_no_float(test: unittest.TestCase, node: Any, path: str = "$") -> None:
    if isinstance(node, float):
        test.fail(f"raw JSON float at {path}")
    if isinstance(node, dict):
        for key, value in node.items():
            _assert_no_float(test, value, f"{path}.{key}")
    elif isinstance(node, list):
        for index, value in enumerate(node):
            _assert_no_float(test, value, f"{path}[{index}]")


def _reseal(artifact: dict[str, Any]) -> None:
    body = dict(artifact)
    del body["artifact_hash"]
    artifact["artifact_hash"] = sha256_of_obj(body)


class ArtifactIntegrityTests(unittest.TestCase):
    def test_candidate_modules_do_not_leak_through_rev7_dispatcher(self):
        for name in (
            "project_center_width_v1",
            "load_boundary_artifact",
            "load_numeric_artifact",
            "mean64",
        ):
            with self.subTest(name=name):
                self.assertFalse(hasattr(contracts_pkg, name))

    def test_o02_o03_o18_load_and_recompute_byte_identically(self):
        boundary = load_boundary_artifact()
        width = load_width_artifact()
        numeric = load_numeric_artifact()

        rebuilt_boundary, _ = build_boundary_artifact()
        self.assertEqual(boundary, rebuilt_boundary)

        rebuilt_width, _ = build_width_artifact(
            sha256_of_file(BOUNDARY_ARTIFACT_PATH))
        self.assertEqual(width, rebuilt_width)

        rebuilt_numeric, _ = build_numeric_artifact()
        self.assertEqual(numeric, rebuilt_numeric)

        for path, artifact in (
            (BOUNDARY_ARTIFACT_PATH, boundary),
            (WIDTH_ARTIFACT_PATH, width),
            (NUMERIC_ARTIFACT_PATH, numeric),
        ):
            with self.subTest(path=path.name):
                self.assertEqual(path.read_bytes(), canonical_bytes(artifact))
                _assert_no_float(self, artifact)

    def test_o02_shapes_and_embedded_hashes(self):
        artifact = load_boundary_artifact()
        centers = artifact["center_binary64_bits"]
        boundaries = artifact["boundary_binary64_bits"]
        self.assertEqual(len(centers), 120)
        self.assertEqual(len(boundaries), 119)
        self.assertEqual(artifact["center_hash"], sha256_of_obj(centers))
        self.assertEqual(artifact["boundary_hash"], sha256_of_obj(boundaries))
        self.assertEqual(centers[0], "0x4034000000000000")  # 20
        self.assertEqual(centers[-1], "0x40d3880000000000")  # 20000

    def test_each_boundary_rounds_the_exact_geometric_mean(self):
        """Independent rational proof around the candidate rounding bin."""
        artifact = load_boundary_artifact()
        centers = [
            exact_n64(n64_from_bits(bits))
            for bits in artifact["center_binary64_bits"]
        ]
        for index, bits in enumerate(artifact["boundary_binary64_bits"]):
            boundary_token = n64_from_bits(bits)
            boundary_float = float_from_n64(boundary_token)
            boundary = exact_n64(boundary_token)
            previous = exact(math.nextafter(boundary_float, -math.inf))
            following = exact(math.nextafter(boundary_float, math.inf))
            lower_midpoint = (previous + boundary) / 2
            upper_midpoint = (boundary + following) / 2
            radicand = centers[index] * centers[index + 1]
            with self.subTest(index=index):
                # The geometric means are irrational here, so strict
                # containment proves the exact value rounds to this boundary.
                self.assertLess(lower_midpoint * lower_midpoint, radicand)
                self.assertLess(radicand, upper_midpoint * upper_midpoint)

    def test_generator_report_is_canonical_sealed_and_bound_to_files(self):
        report = load_strict(_REPORT_PATH)
        self.assertEqual(_REPORT_PATH.read_bytes(), canonical_bytes(report))
        body = dict(report)
        claimed = body.pop("artifact_hash")
        self.assertEqual(claimed, sha256_of_obj(body))
        for name, claim in report["artifacts"].items():
            with self.subTest(name=name):
                path = _REPORT_PATH.parent / name
                self.assertEqual(claim["file_sha256"], sha256_of_file(path))
        diagnostic = report["rev7_legacy_grid_diagnostic"]
        legacy_bits = [
            "0x" + struct.pack(">d", value).hex()
            for value in band_centers_hz()
        ]
        current = load_boundary_artifact()["center_binary64_bits"]
        distances = [
            abs(int(old[2:], 16) - int(new[2:], 16))
            for old, new in zip(legacy_bits, current)
        ]
        self.assertEqual(
            diagnostic["mismatched_center_count"],
            sum(distance != 0 for distance in distances),
        )
        self.assertEqual(
            diagnostic["maximum_positive_bit_order_distance"],
            max(distances),
        )


class ProjectionTests(unittest.TestCase):
    def test_finite_values_saturate_to_first_and_last_band(self):
        self.assertEqual(project_n64(n64(-1.0)), 0)
        self.assertEqual(project_n64(n64(1.0)), 0)
        self.assertEqual(project_n64(n64(30000.0)), 119)

    def test_boundary_tie_maps_lower_and_next_float_maps_upper(self):
        artifact = load_boundary_artifact()
        for index in (0, 17, 59, 100, 118):
            boundary = n64_from_bits(
                artifact["boundary_binary64_bits"][index])
            just_above = n64(math.nextafter(
                float_from_n64(boundary), math.inf))
            with self.subTest(index=index):
                self.assertEqual(project_n64(boundary), index)
                self.assertEqual(project_n64(just_above), index + 1)

    def test_inverted_interval_is_rejected(self):
        with self.assertRaisesRegex(FrequencyGeometryError, "raw_lo > raw_hi"):
            project_interval_n64(n64(100.0), n64(99.0))

    def test_zero_width_is_a_non_degenerate_single_band(self):
        expected = project_n64(n64(1000.0))
        self.assertEqual(
            project_center_width_v1(n64(1000.0), n64(0.0)),
            (expected, expected),
        )

    def test_wmax_is_accepted_and_next_binary64_is_rejected(self):
        width = load_width_artifact()
        w_max = n64_from_bits(width["w_max_binary64_bits"])
        result = project_center_width_v1(n64(1000.0), w_max)
        self.assertEqual(result, (0, 119))
        above = n64(math.nextafter(float_from_n64(w_max), math.inf))
        with self.assertRaisesRegex(FrequencyGeometryError, "exceeds W_MAX"):
            project_center_width_v1(n64(1000.0), above)

    def test_invalid_center_and_width_fail_closed(self):
        for center, width, message in (
            (n64(0.0), n64(0.0), "strictly positive"),
            (n64(-1.0), n64(0.0), "strictly positive"),
            (n64(1000.0), n64(-0.5), "non-negative"),
        ):
            with self.subTest(center=center, width=width):
                with self.assertRaisesRegex(FrequencyGeometryError, message):
                    project_center_width_v1(center, width)

    def test_endpoint_derivation_is_idempotent(self):
        first = derive_endpoint_tokens_v1(n64(1000.0), n64(1.0))
        second = derive_endpoint_tokens_v1(n64(1000.0), n64(1.0))
        self.assertEqual(first, second)
        self.assertEqual(project_interval_n64(first[0], first[1]), (61, 73))

    def test_raw_aliases_share_canonical_band(self):
        base = n64(1000.0)
        adjacent = n64(math.nextafter(1000.0, math.inf))
        self.assertNotEqual(base, adjacent)
        self.assertEqual(
            project_center_width_v1(base, n64(0.0)),
            project_center_width_v1(adjacent, n64(0.0)),
        )


class NumericArtifactTests(unittest.TestCase):
    def test_signed_goldens_and_mutations(self):
        artifact = load_numeric_artifact()
        self.assertEqual(
            artifact["two_ulp_fixture"]["positive_binary64_order_steps"], 2)
        self.assertEqual(
            artifact["sum_pairwise64_goldens"][0]["expected"],
            "f64:3ff8000000000000",
        )
        self.assertEqual(
            artifact["mean64_goldens"][0]["expected"],
            "f64:3fe0000000000000",
        )
        compound = artifact["compound_rounding"]
        self.assertEqual(
            compound["correct_single_round"], "f64:3fad0022f7fa735f")
        self.assertEqual(
            compound["double_round_mutation"], "f64:3fad0022f7fa7360")
        self.assertNotEqual(
            compound["correct_single_round"],
            compound["double_round_mutation"],
        )

    def test_large_integer_goldens_are_exact(self):
        for value in load_numeric_artifact()["exact_integer_goldens"]:
            with self.subTest(value=value):
                self.assertEqual(exact(value), Fraction(value))

    def test_mean64_is_input_order_independent_after_key_sort(self):
        values = [
            (["g1"], n64(0.2)),
            (["g2"], n64(0.3)),
            (["g3"], n64(1.0)),
        ]
        self.assertEqual(mean64(values), mean64(reversed(values)))
        self.assertEqual(mean64(values), "f64:3fe0000000000000")


class ArtifactFalsificationTests(unittest.TestCase):
    def test_resealed_boundary_mutation_is_rejected_by_regeneration(self):
        mutated = copy.deepcopy(load_boundary_artifact())
        current = int(mutated["center_binary64_bits"][1][2:], 16)
        mutated["center_binary64_bits"][1] = f"0x{current + 1:016x}"
        mutated["center_hash"] = sha256_of_obj(
            mutated["center_binary64_bits"])
        _reseal(mutated)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / BOUNDARY_ARTIFACT_PATH.name
            write_canonical(path, mutated)
            frequency_geometry.load_boundary_artifact.cache_clear()
            try:
                with patch.object(
                        frequency_geometry, "BOUNDARY_ARTIFACT_PATH", path):
                    with self.assertRaisesRegex(
                            FrequencyGeometryError,
                            "correctly-rounded regeneration"):
                        frequency_geometry.load_boundary_artifact()
            finally:
                frequency_geometry.load_boundary_artifact.cache_clear()

    def test_resealed_wmax_mutation_is_rejected_by_regeneration(self):
        mutated = copy.deepcopy(load_width_artifact())
        current = int(mutated["w_max_binary64_bits"][2:], 16)
        mutated["w_max_binary64_bits"] = f"0x{current + 1:016x}"
        _reseal(mutated)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / WIDTH_ARTIFACT_PATH.name
            write_canonical(path, mutated)
            frequency_geometry.load_width_artifact.cache_clear()
            try:
                with patch.object(
                        frequency_geometry, "WIDTH_ARTIFACT_PATH", path):
                    with self.assertRaisesRegex(
                            FrequencyGeometryError,
                            "correctly-rounded regeneration"):
                        frequency_geometry.load_width_artifact()
            finally:
                frequency_geometry.load_width_artifact.cache_clear()

    def test_resealed_numeric_mutation_is_rejected_by_recomputation(self):
        mutated = copy.deepcopy(load_numeric_artifact())
        mutated["mean64_goldens"][0]["expected"] = "f64:3fe0000000000001"
        _reseal(mutated)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / NUMERIC_ARTIFACT_PATH.name
            write_canonical(path, mutated)
            numeric_artifact.load_numeric_artifact.cache_clear()
            try:
                with patch.object(
                        numeric_artifact, "NUMERIC_ARTIFACT_PATH", path):
                    with self.assertRaisesRegex(
                            numeric_artifact.NumericArtifactError,
                            "differs from recomputed"):
                        numeric_artifact.load_numeric_artifact()
            finally:
                numeric_artifact.load_numeric_artifact.cache_clear()


if __name__ == "__main__":
    unittest.main()
