"""Tests for the lab-only runtime feasibility harness.

Nothing here proves real-time safety; the isolation tests exercise a
control-flow prototype and are labelled as such. They pin the contract shape
so a later C++ implementation has something falsifiable to match.
"""
from __future__ import annotations

import math
import threading
import unittest
from fractions import Fraction

import torch
from torch import nn

import ml_v3.contracts as contracts_pkg
from ml_v3.contracts.constants import GRID_BANDS
from ml_v3.contracts.metrology_lock import HOP_SAMPLES
from ml_v3.frontend.feature_frame import (
    BAND_VECTOR_FIELDS,
    SCALAR_FLOAT_FIELDS,
)
from ml_v3.runtime_feasibility.benchmark import (
    benchmark_envelope,
    environment,
    percentiles,
)
from ml_v3.runtime_feasibility.envelope import (
    ANALYSIS_HOP_SECONDS,
    FEATURES_PER_FRAME,
    LAB_ENVELOPES,
    RuntimeEnvelope,
    analysis_hop_seconds,
    margin_verdict,
)
from ml_v3.runtime_feasibility.isolation import (
    NEUTRAL_RESULT,
    AudioLikeConsumer,
    BoundedQueue,
    IsolationError,
)
from ml_v3.runtime_feasibility.surrogate import (
    SurrogateError,
    build_surrogate,
    count_parameters,
    make_input,
)


class EnvelopeTests(unittest.TestCase):
    def test_shape_and_cadence_come_from_frozen_artifacts(self):
        """The envelope must not invent the two numbers that drive everything.

        If the feature width or the hop were hardcoded here, a later change to
        the frontend or the metrology lock would silently invalidate every
        measurement while the harness kept reporting green.
        """
        self.assertEqual(
            FEATURES_PER_FRAME,
            len(BAND_VECTOR_FIELDS) * GRID_BANDS + len(SCALAR_FLOAT_FIELDS),
        )
        self.assertEqual(ANALYSIS_HOP_SECONDS, Fraction(HOP_SAMPLES, 48_000))
        self.assertEqual(float(ANALYSIS_HOP_SECONDS) * 1000, 21.333333333333332)

    def test_hop_is_exact_rational_not_float(self):
        self.assertIsInstance(ANALYSIS_HOP_SECONDS, Fraction)
        self.assertEqual(analysis_hop_seconds(48_000), Fraction(1024, 48_000))
        with self.assertRaises(ValueError):
            analysis_hop_seconds(0)

    def test_causal_receptive_field_follows_the_dilated_construction(self):
        envelope = RuntimeEnvelope("probe", "causal_tcn", 64, 8, 4, 3, 8)
        self.assertEqual(
            envelope.receptive_field_frames,
            1 + sum(2 * (2 ** layer) for layer in range(4)),
        )

    def test_margin_traffic_light_boundaries(self):
        hop = float(ANALYSIS_HOP_SECONDS)
        self.assertEqual(margin_verdict(hop * 0.10), "green")
        self.assertEqual(margin_verdict(hop * 0.50), "green")
        self.assertEqual(margin_verdict(hop * 0.60), "amber")
        self.assertEqual(margin_verdict(hop * 0.90), "red")
        with self.assertRaises(ValueError):
            margin_verdict(-1.0)

    def test_rejects_malformed_envelopes(self):
        with self.assertRaises(ValueError):
            RuntimeEnvelope("bad", "causal_tcn", 0, 8, 2, 3, 8)
        with self.assertRaises(ValueError):
            RuntimeEnvelope("bad", "not_a_kind", 8, 8, 2, 3, 8)


class SurrogateTests(unittest.TestCase):
    def test_every_lab_envelope_builds_and_runs(self):
        for envelope in LAB_ENVELOPES:
            with self.subTest(envelope=envelope.name):
                model = build_surrogate(envelope, seed=1)
                sample = make_input(envelope, seed=2)
                with torch.no_grad():
                    output = model(sample)
                self.assertEqual(output.shape[-1], envelope.outputs)
                self.assertGreater(count_parameters(model), 0)
                self.assertTrue(torch.isfinite(output).all())

    def test_input_uses_the_real_frontend_width(self):
        tcn = next(e for e in LAB_ENVELOPES if e.kind == "causal_tcn")
        self.assertEqual(make_input(tcn, seed=1).shape[1], FEATURES_PER_FRAME)
        spectro = next(
            e for e in LAB_ENVELOPES if e.kind == "spectro_temporal")
        sample = make_input(spectro, seed=1)
        self.assertEqual(sample.shape[1], len(BAND_VECTOR_FIELDS))
        self.assertEqual(sample.shape[2], GRID_BANDS)

    def test_causal_tcn_has_no_lookahead(self):
        """The invariant a real-time decision cannot violate.

        Perturbing frame T must change the output at T and leave every earlier
        output bit-identical. A model that fails this is reading the future,
        which is unimplementable in a live plugin no matter how fast it is.
        """
        envelope = next(e for e in LAB_ENVELOPES if e.name == "tcn_worst")
        model = build_surrogate(envelope, seed=1)
        sample = make_input(envelope, seed=2)

        def full_sequence(tensor):
            for pad, conv in zip(model.pads, model.convs):
                tensor = torch.relu(conv(nn.functional.pad(tensor, (pad, 0))))
            return model.head(tensor)

        with torch.no_grad():
            base = full_sequence(sample).clone()
            perturbed = sample.clone()
            perturbed[..., -1] += 1000.0
            after = full_sequence(perturbed)

        self.assertFalse(torch.allclose(base[..., -1], after[..., -1]))
        for index in (-2, -5, -20, 0):
            with self.subTest(frame=index):
                self.assertTrue(
                    torch.allclose(base[..., index], after[..., index]),
                    "an earlier output changed: the model reads the future",
                )

    def test_same_seed_gives_the_same_graph(self):
        envelope = LAB_ENVELOPES[1]
        first = build_surrogate(envelope, seed=7)
        second = build_surrogate(envelope, seed=7)
        sample = make_input(envelope, seed=9)
        with torch.no_grad():
            self.assertTrue(torch.equal(first(sample), second(sample)))

    def test_rejects_unknown_kind(self):
        envelope = RuntimeEnvelope("x", "baseline", 1, 4, 2, 3, 8)
        object.__setattr__(envelope, "kind", "invented")
        with self.assertRaises(SurrogateError):
            build_surrogate(envelope, seed=1)


class BenchmarkTests(unittest.TestCase):
    def test_percentiles_never_invent_an_unobserved_value(self):
        samples = [float(value) for value in range(1, 101)]
        stats = percentiles(samples)
        for key in ("min", "p50", "p95", "p99", "max"):
            self.assertIn(stats[key], samples)
        self.assertEqual(stats["min"], 1.0)
        self.assertEqual(stats["max"], 100.0)
        self.assertLessEqual(stats["p50"], stats["p95"])
        self.assertLessEqual(stats["p95"], stats["p99"])
        with self.assertRaises(ValueError):
            percentiles([])

    def test_environment_records_what_the_numbers_describe(self):
        env = environment()
        for key in ("python", "os", "machine", "torch", "cpu_topology",
                    "build_mode"):
            self.assertIn(key, env)

    def test_benchmark_reports_warmup_separately_and_records_load(self):
        """Warm-up folded into steady state would flatter or spoil the tail.

        Load is recorded because the first sweep of this harness ran at load
        ~100 on 8 cores and its multi-instance numbers measured the machine,
        not the model.
        """
        envelope = next(e for e in LAB_ENVELOPES if e.kind == "baseline")
        previous_threads = torch.get_num_threads()
        result = benchmark_envelope(
            envelope, iterations=20, warmup=5, instances=1, threads=1)
        self.assertEqual(torch.get_num_threads(), previous_threads)
        self.assertEqual(result.warmup_iterations, 5)
        self.assertGreater(result.warmup_seconds, 0.0)
        self.assertEqual(result.iterations, 20)
        self.assertGreater(result.seconds["p99"], 0.0)
        self.assertIn("load_average_before", result.__dict__)
        self.assertIn(result.verdict, {"green", "amber", "red"})

    def test_benchmark_rejects_impossible_configurations(self):
        envelope = LAB_ENVELOPES[0]
        for kwargs in ({"iterations": 0}, {"instances": 0}, {"warmup": -1},
                       {"threads": 0}, {"threads": 1.5}):
            with self.subTest(**kwargs):
                with self.assertRaises(ValueError):
                    benchmark_envelope(envelope, **kwargs)


class IsolationTests(unittest.TestCase):
    """Control-flow prototype; C++/JUCE RT proof still pending."""

    def test_queue_drops_instead_of_growing_or_blocking(self):
        queue = BoundedQueue(3)
        for value in range(3):
            self.assertTrue(queue.push(value))
        self.assertFalse(queue.push(99))
        self.assertEqual(len(queue), 3)
        self.assertEqual(queue.dropped, 1)

    def test_queue_rejects_malformed_capacity(self):
        for capacity in (0, -1, 2.5):
            with self.subTest(capacity=capacity):
                with self.assertRaises(IsolationError):
                    BoundedQueue(capacity)

    def test_consumer_falls_back_to_neutral_when_stale(self):
        """A late result must cost a neutral frame, never a stall."""
        consumer = AudioLikeConsumer(max_stale_frames=2)
        consumer.offer(10, (1.0,) * 8)
        self.assertEqual(consumer.consume(11), (1.0,) * 8)
        self.assertEqual(consumer.consume(20), NEUTRAL_RESULT)
        self.assertEqual(consumer.neutral_fallbacks, 1)

    def test_consumer_is_neutral_before_any_result_exists(self):
        consumer = AudioLikeConsumer(max_stale_frames=4)
        self.assertEqual(consumer.consume(0), NEUTRAL_RESULT)

    def test_non_finite_output_is_rejected_not_clamped(self):
        """Clamping a NaN would hide a broken model behind a plausible number."""
        consumer = AudioLikeConsumer(max_stale_frames=4)
        for bad in (math.nan, math.inf, -math.inf):
            with self.subTest(bad=bad):
                with self.assertRaises(IsolationError):
                    consumer.offer(1, (bad,) + (0.0,) * 7)

    def test_result_width_and_sequence_are_fail_closed(self):
        consumer = AudioLikeConsumer(max_stale_frames=4)
        with self.assertRaises(IsolationError):
            consumer.offer(-1, (0.0,) * 8)
        with self.assertRaises(IsolationError):
            consumer.offer(1, (0.0,) * 7)
        with self.assertRaises(IsolationError):
            consumer.offer(1, ("bad",) + (0.0,) * 7)

    def test_future_result_is_neutral_until_its_frame(self):
        consumer = AudioLikeConsumer(max_stale_frames=4)
        consumer.offer(10, (1.0,) * 8)
        self.assertEqual(consumer.consume(9), NEUTRAL_RESULT)
        self.assertEqual(consumer.consume(10), (1.0,) * 8)

    def test_out_of_order_result_is_rejected(self):
        consumer = AudioLikeConsumer(max_stale_frames=4)
        consumer.offer(5, (1.0,) * 8)
        with self.assertRaises(IsolationError):
            consumer.offer(4, (2.0,) * 8)
        with self.assertRaises(IsolationError):
            consumer.offer(5, (3.0,) * 8)

    def test_dead_worker_does_not_stall_the_audio_like_path(self):
        """The whole point: audio must not care that the worker died."""
        queue = BoundedQueue(8)
        consumer = AudioLikeConsumer(max_stale_frames=2)
        crashed = threading.Event()

        def worker():
            queue.pop()
            crashed.set()
            raise RuntimeError("worker died")

        thread = threading.Thread(target=worker, daemon=True)
        for frame in range(8):
            queue.push(frame)

        # The worker really does die; only its traceback is muted, so the
        # suite output does not advertise an expected crash as a failure.
        previous_hook = threading.excepthook
        threading.excepthook = lambda args: None
        try:
            thread.start()
            thread.join(timeout=5)
        finally:
            threading.excepthook = previous_hook
        self.assertTrue(crashed.is_set())
        self.assertFalse(thread.is_alive())

        for frame in range(100):
            self.assertEqual(consumer.consume(frame), NEUTRAL_RESULT)
        self.assertEqual(consumer.applied, 0)
        self.assertEqual(consumer.neutral_fallbacks, 100)

    def test_slow_worker_bounds_backlog_instead_of_growing_it(self):
        queue = BoundedQueue(4)
        for frame in range(1000):
            queue.push(frame)
        self.assertEqual(len(queue), 4)
        self.assertEqual(queue.dropped, 996)

    def test_concurrent_producers_never_exceed_capacity(self):
        queue = BoundedQueue(50)
        def produce():
            for value in range(200):
                queue.push(value)
        threads = [threading.Thread(target=produce) for _ in range(8)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertLessEqual(len(queue), 50)
        self.assertEqual(len(queue) + queue.dropped, 8 * 200)


class PerimeterTests(unittest.TestCase):
    def test_harness_is_absent_from_the_rev7_dispatcher(self):
        for symbol in ("BoundedQueue", "build_surrogate", "benchmark_envelope",
                       "RuntimeEnvelope"):
            self.assertFalse(
                hasattr(contracts_pkg, symbol),
                f"{symbol} must not be exported by ml_v3.contracts",
            )


if __name__ == "__main__":
    unittest.main()
