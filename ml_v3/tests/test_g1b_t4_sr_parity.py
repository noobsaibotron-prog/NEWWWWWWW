"""G1b T4 / spike WS4 — gate-4 SR-parity harness (fail-closed).

Full adversarial matrix evidence is produced by
``ml_v3/benchmark/run_ws4_evidence.py`` (slow). Tests here guard platform,
window, activity empty-set, invalid-channel ignore, and verdict packaging.
"""
from __future__ import annotations

import unittest

import numpy as np

from ml_v3.benchmark.sr_parity import (
    MARGIN_FLOOR_DB,
    SRParityError,
    evaluate_sr_parity_cell,
    filter_useful_frames,
    nearest_frame,
    require_gate_platform_full,
    source_time_seconds,
    verdict_from_max_abs,
)
from ml_v3.contracts.fixture_spec import common_useful_window
from ml_v3.contracts.metrology_lock import (
    ACTIVITY_FLOOR_DB,
    SR_PARITY_MAX_ABS_DB,
    gate_platform_python_label,
    require_gate_platform_python,
)
from ml_v3.fixtures.g1.render_signals import render_multitone
from ml_v3.frontend.offline_features import extract_offline_feature_frames


def _stub_frame(
    *,
    t_num: int,
    t_den: int,
    frame_index: int,
    frame_end: int,
    mid_psd: np.ndarray,
    mid_valid: bool = True,
    level: float = -20.0,
) -> dict:
    z = np.full(120, -120.0, dtype=np.float32)
    shape = mid_psd.astype(np.float32).copy()
    return {
        "source_time_num": t_num,
        "source_time_den": t_den,
        "frame_index": frame_index,
        "frame_end_sample": frame_end,
        "mid_valid": mid_valid,
        "side_valid": False,
        "valid": mid_valid,
        "mid_psd_db": mid_psd.astype(np.float32),
        "side_psd_db": z.copy(),
        "mid_shape_db": shape,
        "side_shape_db": z.copy(),
        "mid_prominence_db": np.zeros(120, dtype=np.float32),
        "side_prominence_db": np.zeros(120, dtype=np.float32),
        "mid_level_dbfs": np.float32(level if mid_valid else -120.0),
        "side_level_dbfs": np.float32(-120.0),
    }


class GatePlatformTests(unittest.TestCase):
    def test_require_gate_platform_full(self):
        info = require_gate_platform_full()
        self.assertEqual(info["python"], "CPython 3.12.13")
        self.assertEqual(info["numpy"], "2.5.1")
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")
        require_gate_platform_python()


class VerdictPackagingTests(unittest.TestCase):
    def test_green_ambra_red_boundaries(self):
        self.assertEqual(verdict_from_max_abs(0.10), "GREEN")
        # Example from plan: 0.24 with floor 0.05 → AMBRA
        self.assertEqual(verdict_from_max_abs(0.24), "AMBRA")
        self.assertEqual(
            verdict_from_max_abs(SR_PARITY_MAX_ABS_DB - MARGIN_FLOOR_DB),
            "AMBRA",
        )
        self.assertEqual(
            verdict_from_max_abs(SR_PARITY_MAX_ABS_DB - MARGIN_FLOOR_DB - 1e-9),
            "GREEN",
        )
        self.assertEqual(verdict_from_max_abs(0.2500001), "RED")
        self.assertEqual(verdict_from_max_abs(SR_PARITY_MAX_ABS_DB), "AMBRA")


class HarnessUnitTests(unittest.TestCase):
    def test_nearest_tie_break_smaller_frame_index(self):
        # Same |Δt|: prefer smaller frame_index.
        a = _stub_frame(
            t_num=1, t_den=1, frame_index=2, frame_end=100,
            mid_psd=np.full(120, -50.0),
        )
        b = _stub_frame(
            t_num=1, t_den=1, frame_index=1, frame_end=200,
            mid_psd=np.full(120, -50.0),
        )
        # Both at t=1.0; target 1.0 → tie → smaller index
        got = nearest_frame([a, b], 1.0)
        self.assertEqual(got["frame_index"], 1)

    def test_empty_active_set_fails(self):
        # Both at floor → activity false → FAIL
        psd = np.full(120, ACTIVITY_FLOOR_DB, dtype=np.float64)
        # Place inside common useful window via synthetic times near 1.0s
        ref = [_stub_frame(
            t_num=1, t_den=1, frame_index=0, frame_end=8192, mid_psd=psd,
        )]
        sr = [_stub_frame(
            t_num=1, t_den=1, frame_index=0, frame_end=8192, mid_psd=psd,
        )]
        start, end = common_useful_window()
        self.assertTrue(start <= 1.0 <= end)
        with self.assertRaises(SRParityError) as ctx:
            evaluate_sr_parity_cell(
                ref, sr, asset="stub", fs_sr=44100, mode="full",
            )
        self.assertIn("empty active-cell", str(ctx.exception).lower())

    def test_invalid_channel_ignored_does_not_compare_floor_vs_live(self):
        live = np.full(120, -40.0, dtype=np.float64)
        live[10] = -30.0
        floor = np.full(120, -120.0, dtype=np.float64)
        ref = [_stub_frame(
            t_num=1, t_den=1, frame_index=0, frame_end=8192,
            mid_psd=floor, mid_valid=False, level=-120.0,
        )]
        sr = [_stub_frame(
            t_num=1, t_den=1, frame_index=0, frame_end=8192,
            mid_psd=live, mid_valid=True, level=-25.0,
        )]
        with self.assertRaises(SRParityError) as ctx:
            # only pair is invalid-on-ref → no active cells
            evaluate_sr_parity_cell(
                ref, sr, asset="stub", fs_sr=44100, mode="full",
            )
        self.assertIn("empty active-cell", str(ctx.exception).lower())

    def test_filter_useful_rejects_empty(self):
        with self.assertRaises(SRParityError):
            filter_useful_frames([])


class SmokeLiveExtractTests(unittest.TestCase):
    """Short live smoke: harness runs on multitone useful frames (not full matrix)."""

    def test_multitone_cell_returns_finite_max(self):
        require_gate_platform_python()
        # Use a truncated extract by filtering — full 2s still OK (~10s).
        ref = extract_offline_feature_frames(render_multitone(48000), 48000)
        sr = extract_offline_feature_frames(render_multitone(44100), 44100)
        cell = evaluate_sr_parity_cell(
            ref, sr, asset="multitone", fs_sr=44100, mode="full",
        )
        self.assertTrue(np.isfinite(cell.max_abs_db))
        self.assertGreater(cell.n_active_cells, 0)
        self.assertIn(cell.verdict, ("GREEN", "AMBRA", "RED"))
        # Spike probe expectation under current tip: RED on full domain.
        self.assertEqual(cell.verdict, "RED")
        self.assertGreater(cell.max_abs_db, SR_PARITY_MAX_ABS_DB)
        # Localization must include source times inside useful window.
        start, end = common_useful_window()
        self.assertIsNotNone(cell.peak)
        assert cell.peak is not None
        self.assertGreaterEqual(cell.peak.t_ref, start)
        self.assertLessEqual(cell.peak.t_ref, end)
        _ = source_time_seconds(ref[0])


if __name__ == "__main__":
    unittest.main()
