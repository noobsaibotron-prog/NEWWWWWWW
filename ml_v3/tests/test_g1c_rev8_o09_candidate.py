"""Candidate-only O-09 A1/A2 equivalence and preflight falsification."""
from __future__ import annotations

from fractions import Fraction
from importlib.machinery import EXTENSION_SUFFIXES
import itertools
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from ml_v3.benchmark import run_rev8_o09_candidate as runner_module

from ml_v3.benchmark.rev8_o09_candidate import (
    CandidateGraph,
    CandidateGraphError,
    ExactEdge,
    PROVISIONAL_MAX_EXACT_SCALAR_BITS,
    a1_exhaustive,
    a2_exact,
    provisional_preflight_probe,
    rational_bit_length,
)
from ml_v3.benchmark.run_rev8_o09_candidate import (
    _fraction,
    _external_output_path,
    _graph,
    _loaded_module_provenance,
    _measure_ap_prefix,
    _measure_bit_boundaries,
    _measure_solver,
    _run_child,
    _result_payload,
    _source_hashes,
    _tree_hygiene,
)


def edge(
    gt: int,
    prediction: int,
    *,
    k2: Fraction = Fraction(1),
    k3: int = 0,
    k4: Fraction = Fraction(0),
    scientific: bytes | None = None,
    diagnostic: bytes | None = None,
    severity: Fraction = Fraction(0),
    onset: int = 0,
    offset: int = 0,
) -> ExactEdge:
    return ExactEdge(
        gt=gt,
        prediction=prediction,
        k2_iou=k2,
        k3_tick_error=k3,
        k4_cost=k4,
        scientific_key=scientific or f"s:{gt}:{prediction}".encode(),
        diagnostic_key=diagnostic or f"d:{gt}:{prediction}".encode(),
        severity_error=severity,
        onset_error_ticks=onset,
        offset_error_ticks=offset,
    )


def graph(gt: int, prediction: int, edges, mode="additive") -> CandidateGraph:
    return CandidateGraph(gt, prediction, mode, tuple(edges))


class ValidationTests(unittest.TestCase):
    def test_duplicate_pair_rejected(self):
        with self.assertRaisesRegex(CandidateGraphError, "duplicate"):
            graph(1, 1, [edge(0, 0), edge(0, 0)])

    def test_product_k4_requires_ratio_at_least_one(self):
        with self.assertRaisesRegex(CandidateGraphError, "must be >= 1"):
            graph(1, 1, [edge(0, 0, k4=Fraction(1, 2))], "product")

    def test_duplicate_diagnostic_key_rejected_as_non_unique_replay(self):
        with self.assertRaisesRegex(CandidateGraphError, "diagnostic edge key"):
            graph(2, 2, [
                edge(0, 0, diagnostic=b"same"),
                edge(1, 1, diagnostic=b"same"),
            ])

    def test_a1_scope_is_small_and_explicit(self):
        with self.assertRaisesRegex(CandidateGraphError, "A1 oracle domain"):
            a1_exhaustive(graph(4, 1, []))


class A1A2EquivalenceTests(unittest.TestCase):
    def assert_equivalent(self, candidate: CandidateGraph) -> None:
        oracle = a1_exhaustive(candidate)
        runtime = a2_exact(candidate)
        self.assertEqual(runtime.objective, oracle.objective)
        self.assertEqual(runtime.matching, oracle.matching)
        self.assertEqual(
            runtime.scientific_sequence, oracle.scientific_sequence)
        self.assertEqual(
            runtime.diagnostic_sequence, oracle.diagnostic_sequence)
        self.assertEqual(runtime.severity_upper, oracle.severity_upper)
        self.assertEqual(runtime.onset_upper_ticks, oracle.onset_upper_ticks)
        self.assertEqual(runtime.offset_upper_ticks, oracle.offset_upper_ticks)

    def test_empty_graph(self):
        candidate = graph(2, 2, [])
        self.assert_equivalent(candidate)
        result = a2_exact(candidate)
        self.assertEqual(result.objective.k1, 0)
        self.assertIsNone(result.severity_upper)

    def test_greedy_cardinality_trap(self):
        candidate = graph(2, 2, [
            edge(0, 0, k2=Fraction(1)),
            edge(0, 1, k2=Fraction(1, 3)),
            edge(1, 0, k2=Fraction(1, 2)),
        ])
        self.assert_equivalent(candidate)
        self.assertEqual(set(a2_exact(candidate).matching), {(0, 1), (1, 0)})

    def test_exact_fraction_near_tie(self):
        candidate = graph(2, 2, [
            edge(0, 0, k2=Fraction(2**54, 2**54 + 1)),
            edge(1, 1, k2=Fraction(1, 3)),
            edge(0, 1, k2=Fraction(2**54 - 1, 2**54)),
            edge(1, 0, k2=Fraction(1, 3)),
        ])
        self.assert_equivalent(candidate)

    def test_product_k4_resonance(self):
        candidate = graph(2, 2, [
            edge(0, 0, k4=Fraction(3, 2)),
            edge(1, 1, k4=Fraction(4, 3)),
            edge(0, 1, k4=Fraction(5, 4)),
            edge(1, 0, k4=Fraction(8, 5)),
        ], "product")
        self.assert_equivalent(candidate)
        self.assertEqual(a2_exact(candidate).objective.k4, Fraction(2))

    def test_full_S_then_D_not_per_edge_tuple(self):
        # Both perfect matchings tie on V. Their scientific sequences decide
        # first; diagnostic keys deliberately prefer the opposite matching.
        candidate = graph(2, 2, [
            edge(0, 0, scientific=b"a", diagnostic=b"z0"),
            edge(1, 1, scientific=b"d", diagnostic=b"z1"),
            edge(0, 1, scientific=b"b", diagnostic=b"a0"),
            edge(1, 0, scientific=b"c", diagnostic=b"a1"),
        ])
        self.assert_equivalent(candidate)
        self.assertEqual(a2_exact(candidate).scientific_sequence, (b"a", b"d"))

    def test_D_breaks_only_an_identical_S(self):
        candidate = graph(2, 2, [
            edge(0, 0, scientific=b"x", diagnostic=b"b"),
            edge(1, 1, scientific=b"x", diagnostic=b"d"),
            edge(0, 1, scientific=b"x", diagnostic=b"a"),
            edge(1, 0, scientific=b"x", diagnostic=b"c"),
        ])
        self.assert_equivalent(candidate)
        self.assertEqual(set(a2_exact(candidate).matching), {(0, 1), (1, 0)})

    def test_three_upper_envelopes_can_choose_different_optima(self):
        candidate = graph(2, 2, [
            edge(0, 0, scientific=b"x", diagnostic=b"a0",
                 severity=Fraction(9, 10), onset=0, offset=1),
            edge(1, 1, scientific=b"x", diagnostic=b"a1",
                 severity=Fraction(9, 10), onset=0, offset=1),
            edge(0, 1, scientific=b"x", diagnostic=b"b0",
                 severity=Fraction(1, 10), onset=10, offset=0),
            edge(1, 0, scientific=b"x", diagnostic=b"b1",
                 severity=Fraction(1, 10), onset=10, offset=0),
        ])
        self.assert_equivalent(candidate)
        result = a2_exact(candidate)
        self.assertEqual(result.severity_upper, Fraction(9, 10))
        self.assertEqual(result.onset_upper_ticks, Fraction(10))
        self.assertEqual(result.offset_upper_ticks, Fraction(1))

    def test_all_topologies_2x2_with_exact_palette(self):
        pairs = [(0, 0), (0, 1), (1, 0), (1, 1)]
        for mask in range(1 << len(pairs)):
            edges = []
            for rank, pair in enumerate(pairs):
                if mask & (1 << rank):
                    edges.append(edge(
                        *pair,
                        k2=Fraction(rank + 1, rank + 2),
                        k3=(rank * 7) % 5,
                        k4=Fraction(rank + 2, rank + 1),
                        scientific=bytes([97 + (rank % 2)]),
                        diagnostic=bytes([100 + rank]),
                        severity=Fraction(rank, 10),
                        onset=rank,
                        offset=4 - rank,
                    ))
            with self.subTest(mask=mask):
                self.assert_equivalent(graph(2, 2, edges, "product"))

    def test_frozen_all_3x3_topologies_additive_and_product(self):
        """All 512 bipartite topologies, with a fixed adversarial palette."""
        pairs = [(gt, prediction) for gt in range(3) for prediction in range(3)]
        for mode in ("additive", "product"):
            for mask in range(1 << len(pairs)):
                rows = []
                for rank, pair in enumerate(pairs):
                    if not mask & (1 << rank):
                        continue
                    rows.append(edge(
                        *pair,
                        k2=Fraction((rank * 5) % 11 + 1, 12),
                        k3=(rank * 7) % 9,
                        k4=(
                            Fraction((rank * 3) % 7, 7)
                            if mode == "additive"
                            else Fraction(8 + (rank * 3) % 7, 8)
                        ),
                        scientific=bytes([97 + (rank % 3)]),
                        diagnostic=f"d:{rank}".encode(),
                        severity=Fraction((rank * 11) % 13, 13),
                        onset=(rank * 13) % 17,
                        offset=(rank * 17) % 19,
                    ))
                with self.subTest(mode=mode, mask=mask):
                    self.assert_equivalent(graph(3, 3, rows, mode))

    def test_input_and_adjacency_permutations_are_observationally_identical(self):
        rows = [
            edge(0, 0, scientific=b"a", diagnostic=b"4"),
            edge(0, 1, scientific=b"b", diagnostic=b"3"),
            edge(1, 0, scientific=b"b", diagnostic=b"2"),
            edge(1, 1, scientific=b"a", diagnostic=b"1"),
        ]
        expected = a2_exact(graph(2, 2, rows))
        for permutation in itertools.permutations(rows):
            with self.subTest(permutation=permutation):
                self.assertEqual(a2_exact(graph(2, 2, permutation)), expected)


class PreflightTests(unittest.TestCase):
    def test_rational_bit_length_is_reduced_num_plus_den(self):
        value = Fraction(2**39_999, 2**39_999 + 1)
        self.assertEqual(rational_bit_length(value), 80_000)

    def test_probe_is_explicitly_diagnostic_not_active(self):
        probe = provisional_preflight_probe(graph(1, 1, [edge(0, 0)]))
        self.assertEqual(probe.authority_status, "PROVISIONAL_DIAGNOSTIC_ONLY")
        self.assertEqual(probe.provisional_exceeded, ())

    def test_probe_detects_each_provisional_shape_ceiling(self):
        self.assertIn(
            "GT", provisional_preflight_probe(graph(129, 0, [])).provisional_exceeded)
        self.assertIn(
            "PREDICTION",
            provisional_preflight_probe(graph(0, 129, [])).provisional_exceeded)
        many_edges = tuple(
            edge(gt, prediction)
            for gt in range(129)
            for prediction in range(128)
        )
        self.assertIn(
            "ELIGIBLE_EDGES",
            provisional_preflight_probe(
                graph(129, 128, many_edges)).provisional_exceeded,
        )

    def test_probe_detects_large_exact_scalar_bound(self):
        huge = Fraction(2**40_000, 2**40_000 + 1)
        candidate = graph(1, 1, [
            edge(0, 0, k2=huge, severity=huge),
        ])
        probe = provisional_preflight_probe(candidate)
        self.assertGreater(
            probe.exact_scalar_bit_bound, PROVISIONAL_MAX_EXACT_SCALAR_BITS)
        self.assertIn(
            "EXACT_SCALAR_BIT_LENGTH", probe.provisional_exceeded)

    def test_exact_scalar_boundary_65535_65536_65537(self):
        cases = (
            (Fraction(2**32_766, 2**32_767 + 1), 65_535, False),
            (Fraction(2**32_767, 2**32_767 + 1), 65_536, False),
            (Fraction(2**32_767, 2**32_768 + 1), 65_537, True),
        )
        for value, expected, exceeded in cases:
            with self.subTest(expected=expected):
                candidate = graph(1, 1, [edge(0, 0, k2=value)])
                probe = provisional_preflight_probe(candidate)
                self.assertEqual(probe.exact_scalar_bit_bound, expected)
                self.assertEqual(
                    "EXACT_SCALAR_BIT_LENGTH" in probe.provisional_exceeded,
                    exceeded,
                )

    def test_onset_ms_publication_is_inside_preflight_bound_at_k1_one(self):
        # Odd and not divisible by 3, so division by 48 does not reduce.
        onset = 2**65_536 - 5
        candidate = graph(1, 1, [edge(0, 0, onset=onset)])
        probe = provisional_preflight_probe(candidate)
        result = a2_exact(candidate)
        self.assertEqual(result.onset_upper_ticks, onset)
        published_ms = result.onset_upper_ticks / 48
        self.assertEqual(rational_bit_length(published_ms), 65_542)
        self.assertGreaterEqual(
            probe.exact_scalar_bit_bound,
            rational_bit_length(published_ms),
        )
        self.assertIn(
            "EXACT_SCALAR_BIT_LENGTH", probe.provisional_exceeded)


class BenchmarkRunnerTests(unittest.TestCase):
    def test_exact_evidence_uses_hex_strings_beyond_decimal_guard(self):
        huge = Fraction(-(2**65_535 + 1), 2**65_536 + 1)
        encoded = _fraction(huge)
        self.assertEqual(encoded, [
            hex(huge.numerator),
            hex(huge.denominator),
        ])
        self.assertTrue(encoded[0].startswith("-0x"))
        self.assertTrue(encoded[1].startswith("0x"))

    def test_exact_k3_at_boundary_serializes_without_decimal_conversion(self):
        k3 = 2**65_535
        candidate = graph(1, 1, [edge(0, 0, k3=k3)])
        probe = provisional_preflight_probe(candidate)
        self.assertEqual(probe.exact_scalar_bit_bound, 65_536)
        self.assertEqual(probe.provisional_exceeded, ())
        payload = _result_payload(a2_exact(candidate))
        self.assertEqual(payload["objective"]["k3"], hex(k3))

    def test_loaded_modules_are_the_expected_python_sources(self):
        evidence = _loaded_module_provenance()
        self.assertTrue({
            "runner_entrypoint",
            "ml_v3.benchmark.rev8_o09_candidate",
            "ml_v3.contracts.canonical",
            "ml_v3.contracts.metrology_lock",
        }.issubset(evidence))
        for loaded in evidence.values():
            self.assertTrue(loaded["path"].endswith(".py"))
            self.assertEqual(len(loaded["sha256"]), 64)

    def test_provenance_snapshot_is_complete_and_hash_pinned(self):
        hashes = _source_hashes()
        self.assertIn(
            "ml_v3/benchmark/rev8_o09_candidate.py", hashes)
        self.assertIn(
            "ml_v3/benchmark/run_rev8_o09_candidate.py", hashes)
        self.assertIn("ml_v3/fixtures/g1/metrology_lock.json", hashes)
        self.assertTrue(all(len(value) == 64 for value in hashes.values()))

    def test_child_reports_loaded_and_hashed_source_provenance(self):
        run = _run_child("unique_additive", 2)
        worker = run["worker_provenance"]
        self.assertEqual(worker["source_sha256"], _source_hashes())
        parent = _loaded_module_provenance()
        for label in (
            "runner_entrypoint",
            "ml_v3.benchmark.rev8_o09_candidate",
            "ml_v3.contracts.canonical",
            "ml_v3.contracts.metrology_lock",
        ):
            self.assertEqual(worker["loaded_modules"][label], parent[label])

    def test_native_extension_shadow_is_rejected_even_when_git_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "ml_v3"
            package.mkdir()
            shadow = package / f"rev8_o09_candidate{EXTENSION_SUFFIXES[0]}"
            shadow.write_bytes(b"not-a-real-extension")
            with mock.patch.object(runner_module, "ROOT", root):
                with self.assertRaisesRegex(RuntimeError, "native_shadows"):
                    _tree_hygiene(require_clean_git=False)

    def test_external_pycache_prefix_is_rejected(self):
        with mock.patch.object(
                runner_module.sys, "pycache_prefix", "/tmp/evil-pyc"):
            with self.assertRaisesRegex(RuntimeError, "pycache_prefix"):
                _tree_hygiene(require_clean_git=False)

    def test_output_inside_repository_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "outside the repository"):
            _external_output_path(
                runner_module.ROOT / "forbidden-benchmark-output.json")

    def test_output_outside_repository_is_resolved(self):
        with tempfile.TemporaryDirectory() as directory:
            expected = Path(directory).resolve() / "evidence.json"
            self.assertEqual(_external_output_path(expected), expected)

    def test_small_solver_workloads_are_deterministic(self):
        for kind in (
            "unique_additive", "degenerate_additive", "unique_product"):
            with self.subTest(kind=kind):
                first = _measure_solver(kind, 4)
                second = _measure_solver(kind, 4)
                self.assertEqual(
                    first["scientific_result_sha256"],
                    second["scientific_result_sha256"],
                )
                self.assertEqual(
                    first["scientific_result"]["objective"]["k1"], 4)

    def test_ap_prefix_probe_is_complete_and_monotone(self):
        result = _measure_ap_prefix(8)["scientific_result"]
        self.assertEqual(result["thresholds"], 8)
        self.assertEqual(result["tp_by_prefix"], list(range(1, 9)))
        self.assertEqual(result["edge_incidence_volume"], 8 * sum(range(1, 9)))

    def test_bit_boundary_bundle_preserves_under_on_over(self):
        cases = {
            row["label"]: row for row in _measure_bit_boundaries()["cases"]
        }
        self.assertEqual(cases["under"]["bound"], 65_535)
        self.assertEqual(cases["on"]["bound"], 65_536)
        self.assertEqual(cases["over"]["bound"], 65_537)
        self.assertEqual(
            cases["onset_ms_over"]["published_rational_bit_length"], 65_542)
        self.assertEqual(
            cases["onset_ms_over"]["bound"], 65_542)

    def test_dense_shape_and_exact_bit_limit_are_combined(self):
        for kind in (
            "bitstress_additive",
            "bitstress_product",
            "bitstress_degenerate",
        ):
            with self.subTest(kind=kind):
                candidate = _graph(kind, 128)
                probe = provisional_preflight_probe(candidate)
                self.assertEqual(candidate.gt_count, 128)
                self.assertEqual(candidate.prediction_count, 128)
                self.assertEqual(len(candidate.edges), 16_384)
                self.assertEqual(probe.exact_scalar_bit_bound, 65_536)
                self.assertEqual(probe.provisional_exceeded, ())
                if kind == "bitstress_additive":
                    diagonal = (
                        candidate.edges[gt * 128 + gt].k2_iou
                        for gt in range(128)
                    )
                    materialized = sum(diagonal, Fraction(0))
                    self.assertGreaterEqual(
                        rational_bit_length(materialized), 65_000)


if __name__ == "__main__":
    unittest.main()
