"""G1b T2 / spike WS2 — offline §6+§7 feature path under P1–P7."""
from __future__ import annotations

import unittest

import numpy as np

from ml_v3.contracts.constants import CANONICAL_SAMPLE_RATE, GRID_BANDS
from ml_v3.contracts.metrology_lock import (
    HOP_SAMPLES,
    N_LF,
    PSD_CLAMP_DB,
    PSD_FLOOR_LINEAR,
    gate_platform_python_label,
    require_gate_platform_python,
)
from ml_v3.frontend.feature_frame import validate_feature_frame_stub
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.frontend.offline_features import (
    DELTA_CLAMP_DB,
    LEVEL_FLOOR_DBFS,
    N_MAIN,
    OfflineFeatureError,
    P7_REASONS,
    _band_energy_linear,
    _build_band_weights,
    _fuse_band_energy,
    _fusion_lf_weights,
    _shape_from_clamped_psd,
    _tables,
    empty_support_report,
    extract_offline_feature_frames,
)


def _tone(n: int, hz: float, fs: int = 48000, amp: float = 0.25) -> np.ndarray:
    t = np.arange(n, dtype=np.float64) / float(fs)
    return (amp * np.sin(2.0 * np.pi * hz * t)).astype(np.float32)


def _silence(n: int) -> np.ndarray:
    return np.zeros(n, dtype=np.float32)


class GatePlatformGuard(unittest.TestCase):
    def test_gate_platform_is_cpython_312_13(self):
        require_gate_platform_python()
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")


class P2GeometryTests(unittest.TestCase):
    def test_empty_support_report_counts(self):
        rep = empty_support_report()
        self.assertEqual(rep["fs_c"], CANONICAL_SAMPLE_RATE)
        self.assertEqual(rep["n_main"], N_MAIN)
        self.assertEqual(rep["n_lf"], N_LF)
        # Exact geometry (matches plan P2 ballpark and WS2 evidence note).
        self.assertEqual(rep["main_empty_count"], 14)
        self.assertEqual(rep["main_single_bin_count"], 21)
        self.assertEqual(rep["lf_empty_count"], 6)
        self.assertEqual(rep["lf_single_bin_count"], 17)
        # Empty bands must have Σw==0; single-bin exactly one positive weight.
        main, lf, *_ = _tables()
        for i in main.empty_band_indices:
            self.assertEqual(float(main.sum_w[i]), 0.0)
        for i in main.single_bin_band_indices:
            self.assertEqual(int(np.count_nonzero(main.weights[i] > 0.0)), 1)
        for i in lf.empty_band_indices:
            self.assertEqual(float(lf.sum_w[i]), 0.0)
        for i in lf.single_bin_band_indices:
            self.assertEqual(int(np.count_nonzero(lf.weights[i] > 0.0)), 1)

    def test_p1_weighted_mean_and_p2_empty_floor(self):
        table = _build_band_weights(N_MAIN)
        n_bins = N_MAIN // 2 + 1
        psd = np.ones(n_bins, dtype=np.float64)
        e = _band_energy_linear(psd, table)
        for i in range(GRID_BANDS):
            if table.sum_w[i] == 0.0:
                self.assertEqual(e[i], 0.0)  # P2 empty → 0 before fuse
            else:
                # Constant PSD → weighted mean equals that constant (P1).
                self.assertAlmostEqual(e[i], 1.0, places=12)
        # After fuse with zeros on both paths → linear floor.
        z = np.zeros(GRID_BANDS, dtype=np.float64)
        fused = _fuse_band_energy(z, z, np.ones(GRID_BANDS, dtype=np.float64))
        self.assertTrue(np.all(fused == PSD_FLOOR_LINEAR))

    def test_no_nearest_bin_on_empty(self):
        """Adversarial: empty-support band must not pick a neighbour bin."""
        table = _build_band_weights(N_MAIN)
        if not table.empty_band_indices:
            self.skipTest("no empty MAIN bands in geometry")
        n_bins = N_MAIN // 2 + 1
        psd = np.zeros(n_bins, dtype=np.float64)
        # Huge energy in all bins — empty band must still contribute 0.
        psd[:] = 1e6
        e = _band_energy_linear(psd, table)
        for i in table.empty_band_indices:
            self.assertEqual(e[i], 0.0)


class P4ShapeTests(unittest.TestCase):
    def test_shape_from_clamped_psd(self):
        psd = np.full(GRID_BANDS, -60.0, dtype=np.float64)
        psd[10] = -40.0
        # Clamp surface (already in range).
        shape = _shape_from_clamped_psd(psd)
        lin = np.sum(10.0 ** (psd / 10.0))
        offset = 10.0 * np.log10(lin)
        expected = psd - offset
        self.assertTrue(np.allclose(shape, expected, rtol=0.0, atol=1e-12))
        # Shape power sums to ~0 dB reference (sum of 10**(shape/10) == 1).
        self.assertAlmostEqual(float(np.sum(10.0 ** (shape / 10.0))), 1.0, places=10)


class FailClosedTests(unittest.TestCase):
    def test_reject_unsupported_sr(self):
        with self.assertRaises(OfflineFeatureError):
            extract_offline_feature_frames(_tone(9000, 1000.0), 32000)

    def test_reject_nan(self):
        x = _tone(9000, 440.0)
        x[100] = np.float32("nan")
        with self.assertRaises(OfflineFeatureError):
            extract_offline_feature_frames(x, 48000)

    def test_reject_inf(self):
        x = _tone(9000, 440.0)
        x[0] = np.float32("inf")
        with self.assertRaises(OfflineFeatureError):
            extract_offline_feature_frames(x, 48000)

    def test_reject_zero_channels_empty(self):
        with self.assertRaises(OfflineFeatureError):
            extract_offline_feature_frames(np.zeros((0,), dtype=np.float32), 48000)

    def test_reject_too_many_channels(self):
        x = np.zeros((100, 3), dtype=np.float32)
        with self.assertRaises(OfflineFeatureError):
            extract_offline_feature_frames(x, 48000)

    def test_insufficient_samples_emits_no_frame(self):
        frames = extract_offline_feature_frames(_tone(N_LF - 1, 1000.0), 48000)
        self.assertEqual(frames, [])


class OfflineEmitTests(unittest.TestCase):
    def test_finite_frames_schema_and_ranges(self):
        # Long enough for a few hops after first LF-capable frame.
        n = N_LF + 4 * HOP_SAMPLES
        frames = extract_offline_feature_frames(_tone(n, 1000.0, amp=0.2), 48000)
        self.assertGreaterEqual(len(frames), 5)
        for i, fr in enumerate(frames):
            validate_feature_frame_stub(fr)
            self.assertEqual(fr["frame_index"], i)
            self.assertEqual(fr["frame_end_sample"], N_LF + i * HOP_SAMPLES)
            self.assertTrue(fr["mid_valid"])
            self.assertFalse(fr["side_valid"])  # mono
            self.assertTrue(fr["valid"])
            self.assertIsNone(fr["reason"])
            for name in (
                "mid_psd_db",
                "side_psd_db",
                "mid_shape_db",
                "side_shape_db",
                "mid_prominence_db",
                "side_prominence_db",
                "mid_delta_db",
                "side_delta_db",
            ):
                self.assertEqual(len(fr[name]), GRID_BANDS)
                self.assertTrue(all(math_isfinite(v) for v in fr[name]))
            for v in fr["mid_psd_db"]:
                self.assertGreaterEqual(v, PSD_CLAMP_DB[0])
                self.assertLessEqual(v, PSD_CLAMP_DB[1])
            for v in fr["mid_delta_db"]:
                self.assertGreaterEqual(v, DELTA_CLAMP_DB[0])
                self.assertLessEqual(v, DELTA_CLAMP_DB[1])
            # P6 invalid side
            self.assertTrue(all(v == LEVEL_FLOOR_DBFS for v in fr["side_psd_db"]))
            self.assertTrue(all(v == 0.0 for v in fr["side_delta_db"]))
            self.assertTrue(all(v == 0.0 for v in fr["side_prominence_db"]))
            self.assertEqual(fr["side_level_dbfs"], LEVEL_FLOOR_DBFS)

    def test_lf_main_share_frame_end_sample(self):
        """Dual-res: every emitted frame is LF-capable (end >= N_LF, hop lattice)."""
        frames = extract_offline_feature_frames(
            _tone(N_LF + 2 * HOP_SAMPLES, 250.0), 48000
        )
        self.assertGreaterEqual(len(frames), 3)
        for fr in frames:
            self.assertGreaterEqual(fr["frame_end_sample"], N_LF)
            self.assertEqual((fr["frame_end_sample"] - N_LF) % HOP_SAMPLES, 0)

    def test_first_valid_delta_zero_then_history(self):
        frames = extract_offline_feature_frames(
            _tone(N_LF + 2 * HOP_SAMPLES, 1000.0, amp=0.2), 48000
        )
        self.assertTrue(all(v == 0.0 for v in frames[0]["mid_delta_db"]))
        # Later frames may be non-zero; still inside clamp.
        for fr in frames[1:]:
            for v in fr["mid_delta_db"]:
                self.assertGreaterEqual(v, DELTA_CLAMP_DB[0])
                self.assertLessEqual(v, DELTA_CLAMP_DB[1])

    def test_silence_reason_p7(self):
        frames = extract_offline_feature_frames(
            _silence(N_LF + HOP_SAMPLES), 48000
        )
        self.assertGreaterEqual(len(frames), 2)
        for fr in frames:
            self.assertFalse(fr["valid"])
            self.assertFalse(fr["mid_valid"])
            self.assertEqual(fr["reason"], "silence")
            self.assertIn(fr["reason"], P7_REASONS)
            self.assertEqual(fr["mid_level_dbfs"], LEVEL_FLOOR_DBFS)

    def test_level_below_threshold_reason_p7(self):
        # RMS ~ amp/sqrt(2); choose amp so level in (-120, -100).
        # 20*log10(amp/sqrt(2)) ≈ -110 → amp ≈ 10**(-110/20)*sqrt(2) ≈ 4.47e-6
        amp = 4.5e-6
        frames = extract_offline_feature_frames(
            _tone(N_LF + HOP_SAMPLES, 1000.0, amp=amp), 48000
        )
        self.assertGreaterEqual(len(frames), 1)
        fr = frames[0]
        self.assertFalse(fr["valid"])
        self.assertEqual(fr["reason"], "level_below_threshold")
        # Emitted invalid level is P6 floor; reason used measured pre-overwrite.
        self.assertEqual(fr["mid_level_dbfs"], LEVEL_FLOOR_DBFS)

    def test_stereo_dual_mono_side_invalid(self):
        n = N_LF + HOP_SAMPLES
        mono = _tone(n, 1000.0, amp=0.2)
        stereo = np.stack([mono, mono], axis=1)
        frames = extract_offline_feature_frames(stereo, 48000)
        self.assertGreaterEqual(len(frames), 2)
        fr = frames[0]
        self.assertTrue(fr["mid_valid"])
        self.assertFalse(fr["side_valid"])
        self.assertTrue(fr["valid"])
        self.assertIsNone(fr["reason"])

    def test_determinism_same_process(self):
        x = _tone(N_LF + HOP_SAMPLES, 440.0, amp=0.15)
        a = extract_offline_feature_frames(x, 48000)
        b = extract_offline_feature_frames(x, 48000)
        self.assertEqual(len(a), len(b))
        for fa, fb in zip(a, b):
            for key in fa:
                if isinstance(fa[key], list):
                    self.assertEqual(
                        np.asarray(fa[key], dtype=np.float32).tobytes(),
                        np.asarray(fb[key], dtype=np.float32).tobytes(),
                        msg=key,
                    )
                else:
                    self.assertEqual(fa[key], fb[key], msg=key)

    def test_resample_44k_emits_finite(self):
        # Host 44.1k → canonical via WS1 resampler; offline feature path only.
        n_host = int(0.25 * 44100)  # ~0.25 s
        frames = extract_offline_feature_frames(_tone(n_host, 1000.0, fs=44100), 44100)
        self.assertGreaterEqual(len(frames), 1)
        for fr in frames:
            validate_feature_frame_stub(fr)
            self.assertTrue(all(math_isfinite(v) for v in fr["mid_psd_db"]))


def math_isfinite(v: float) -> bool:
    return bool(np.isfinite(v))


class FusionWeightTests(unittest.TestCase):
    def test_fusion_endpoints(self):
        centers = np.asarray(band_centers_hz(), dtype=np.float64)
        w = _fusion_lf_weights(centers)
        for i, f in enumerate(centers):
            if f <= 160.0:
                self.assertEqual(w[i], 1.0)
            elif f >= 320.0:
                self.assertEqual(w[i], 0.0)
            else:
                self.assertGreater(w[i], 0.0)
                self.assertLess(w[i], 1.0)


if __name__ == "__main__":
    unittest.main()
