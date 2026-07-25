"""G1b T1b / spike WS1 — causal polyphase apply; offline≡chunk under P5."""
from __future__ import annotations

import unittest

import numpy as np

from ml_v3.contracts.metrology_lock import (
    GEOMETRIC_CHUNK_SCHEDULE,
    gate_platform_python_label,
    require_gate_platform_python,
)
from ml_v3.frontend.resampler import (
    CausalPolyphaseResampler,
    ResamplerError,
    iter_host_chunks,
    resample_chunked,
    resample_offline,
)
from ml_v3.frontend.resampler_coeffs import resample_ratio

# Spike adversarial schedules only (plan §3.1 / WS1). Full lock matrix deferred.
SPIKE_FIXED_SCHEDULES: tuple[int, ...] = (1, 8193)


def _synthetic_host(n: int, seed: int = 7) -> np.ndarray:
    rng = np.random.Generator(np.random.PCG64(seed))
    # float32 host bytes (typical WAV path); resampler promotes to f64 work.
    return rng.standard_normal(n, dtype=np.float64).astype(np.float32)


class GatePlatformGuard(unittest.TestCase):
    def test_gate_platform_is_cpython_312_13(self):
        require_gate_platform_python()
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")


class OutputLengthTests(unittest.TestCase):
    def test_emit_count_matches_ceil_n_up_down(self):
        for fs, n in ((48000, 1000), (44100, 1000), (96000, 1000), (44100, 1)):
            ratio = resample_ratio(fs)
            x = _synthetic_host(n)
            y = resample_offline(x, fs)
            expected = (n * ratio.up + ratio.down - 1) // ratio.down
            self.assertEqual(y.dtype, np.float32)
            self.assertEqual(y.size, expected)

    def test_empty_input(self):
        y = resample_offline(np.zeros(0, dtype=np.float32), 48000)
        self.assertEqual(y.size, 0)
        self.assertEqual(y.dtype, np.float32)


class IdentityPathTests(unittest.TestCase):
    def test_48k_passthrough_cast_at_emit(self):
        x = _synthetic_host(512)
        y = resample_offline(x, 48000)
        self.assertEqual(y.tobytes(), np.asarray(x, dtype=np.float32).tobytes())

    def test_identity_flag(self):
        r = CausalPolyphaseResampler(48000)
        self.assertTrue(r.identity)


class FailClosedTests(unittest.TestCase):
    def test_reject_unsupported_sr(self):
        with self.assertRaises(ResamplerError):
            CausalPolyphaseResampler(32000)

    def test_reject_nan(self):
        x = np.zeros(8, dtype=np.float32)
        x[3] = np.float32("nan")
        with self.assertRaises(ResamplerError):
            resample_offline(x, 48000)

    def test_reject_inf(self):
        x = np.ones(8, dtype=np.float32)
        x[0] = np.float32("inf")
        with self.assertRaises(ResamplerError):
            resample_offline(x, 96000)

    def test_reject_2d(self):
        with self.assertRaises(ResamplerError):
            resample_offline(np.zeros((2, 8), dtype=np.float32), 48000)


class OfflineChunkBitIdentityTests(unittest.TestCase):
    """Stop-early gate: resampler output offline ≡ chunked (byte-identical)."""

    def _assert_schedules(self, fs_in: int, n_host: int) -> None:
        require_gate_platform_python()
        x = _synthetic_host(n_host)
        offline = resample_offline(x, fs_in)
        for fixed in SPIKE_FIXED_SCHEDULES:
            chunked = resample_chunked(x, fs_in, fixed)
            self.assertEqual(
                offline.tobytes(),
                chunked.tobytes(),
                msg=f"fs={fs_in} schedule={fixed} offline≠chunked",
            )
        geo = resample_chunked(x, fs_in, GEOMETRIC_CHUNK_SCHEDULE)
        self.assertEqual(
            offline.tobytes(),
            geo.tobytes(),
            msg=f"fs={fs_in} schedule=geometric-32 offline≠chunked",
        )

    def test_identity_48k_spike_schedules(self):
        # Cover geometric wrap + fixed 8193 with leftover.
        self._assert_schedules(48000, 20_000)

    def test_downsample_96k_spike_schedules(self):
        self._assert_schedules(96000, 20_000)

    def test_upsample_44k1_spike_schedules(self):
        # Long FIR (20481 taps); still must be bit-identical under P5.
        self._assert_schedules(44100, 12_000)

    def test_stateful_process_matches_helper(self):
        fs = 96000
        x = _synthetic_host(4096)
        offline = resample_offline(x, fs)
        r = CausalPolyphaseResampler(fs)
        parts = [r.process(x[s:e]) for s, e in iter_host_chunks(x.size, 1)]
        streamed = np.concatenate(parts)
        self.assertEqual(offline.tobytes(), streamed.tobytes())
        self.assertEqual(r.n_total, x.size)
        self.assertEqual(r.j_emitted, offline.size)


class FormulaSmokeTests(unittest.TestCase):
    def test_manual_single_output_matches_reference_sum(self):
        """One j at 96 kHz: explicit L→R sum vs process() emit."""
        fs = 96000
        ratio = resample_ratio(fs)
        self.assertEqual((ratio.up, ratio.down), (1, 2))
        h = __import__(
            "ml_v3.frontend.resampler_coeffs", fromlist=["fir_lowpass_coefficients"]
        ).fir_lowpass_coefficients(fs)
        m = int(h.size) - 1
        n_host = 400
        x = _synthetic_host(n_host).astype(np.float64)
        j = 10
        up, down = ratio.up, ratio.down
        numer = j * down - m
        n_lo = 0 if numer <= 0 else (numer + up - 1) // up
        n_hi = min(n_host - 1, (j * down) // up)
        acc = 0.0
        for n in range(n_lo, n_hi + 1):
            acc = acc + x[n] * h[j * down - n * up]
        expected = np.float32(acc)
        y = resample_offline(x.astype(np.float32), fs)
        self.assertEqual(expected.tobytes(), y[j].tobytes())


if __name__ == "__main__":
    unittest.main()
