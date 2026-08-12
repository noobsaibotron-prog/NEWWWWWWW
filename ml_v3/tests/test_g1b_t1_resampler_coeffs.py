"""G1b T1 — FIR coefficient generator + feature-frame stub (fail-closed)."""
from __future__ import annotations

import math
import unittest

import numpy as np

from ml_v3.contracts.constants import (
    ACCEPTED_SAMPLE_RATES,
    CANONICAL_SAMPLE_RATE,
    GATE_SAMPLE_RATES,
    GRID_BANDS,
)
from ml_v3.contracts.metrology_lock import (
    KAISER_BETA,
    frozen_metrology_lock,
    resampler_group_delay_rational,
)
from ml_v3.frontend.feature_frame import (
    FEATURE_FRAME_SCHEMA,
    FeatureFrameError,
    feature_frame_field_names,
    validate_feature_frame_stub,
)
from ml_v3.frontend.resampler_coeffs import (
    ResamplerCoeffError,
    fir_lowpass_coefficients,
    group_delay_rational,
    resample_ratio,
)


def _minimal_valid_frame(**overrides):
    floor = [-120.0] * GRID_BANDS
    base = {
        "schema": FEATURE_FRAME_SCHEMA,
        "frame_end_sample": 8192,
        "frame_index": 0,
        "source_time_num": 0,
        "source_time_den": 1,
        "canonical_sample_rate": CANONICAL_SAMPLE_RATE,
        "mid_psd_db": list(floor),
        "side_psd_db": list(floor),
        "mid_shape_db": list(floor),
        "side_shape_db": list(floor),
        "mid_prominence_db": list(floor),
        "side_prominence_db": list(floor),
        "mid_delta_db": [0.0] * GRID_BANDS,
        "side_delta_db": [0.0] * GRID_BANDS,
        "mid_level_dbfs": -120.0,
        "side_level_dbfs": -120.0,
        "mid_valid": True,
        "side_valid": False,
        "valid": True,
        "reason": None,
    }
    base.update(overrides)
    return base


class ResampleRatioTests(unittest.TestCase):
    def test_identity_48000(self):
        r = resample_ratio(48000)
        self.assertTrue(r.identity)
        self.assertEqual((r.up, r.down, r.num_taps), (1, 1, None))
        self.assertEqual(group_delay_rational(48000), (0, 1))
        self.assertEqual(fir_lowpass_coefficients(48000).shape, (0,))

    def test_gate_ratios_match_metrology_lock(self):
        lock = frozen_metrology_lock()["resampler_group_delay"]["per_gate_sample_rate"]
        for fs in GATE_SAMPLE_RATES:
            r = resample_ratio(fs)
            entry = lock[str(fs)]
            self.assertEqual(r.up, entry["up"])
            self.assertEqual(r.down, entry["down"])
            self.assertEqual(r.num_taps, entry["num_taps"])
            n, d = group_delay_rational(fs)
            self.assertEqual((n, d), (entry["delay_num"], entry["delay_den"]))
            # Cross-check serialize-only helper still agrees.
            ln, ld, lt = resampler_group_delay_rational(fs)
            self.assertEqual((n, d, r.num_taps), (ln, ld, lt))

    def test_reject_invalid_sample_rate(self):
        with self.assertRaises(ResamplerCoeffError):
            resample_ratio(32000)
        with self.assertRaises(ResamplerCoeffError):
            fir_lowpass_coefficients(32000)
        with self.assertRaises(ResamplerCoeffError):
            resample_ratio(48000.0)  # type: ignore[arg-type]


class FirCoefficientTests(unittest.TestCase):
    def test_length_and_sum_equals_up(self):
        for fs in (44100, 96000, 88200):
            r = resample_ratio(fs)
            h = fir_lowpass_coefficients(fs)
            self.assertEqual(h.dtype, np.float64)
            self.assertEqual(h.shape, (r.num_taps,))
            self.assertTrue(np.all(np.isfinite(h)))
            # h = up * h0 / sum(h0) ⇒ sum(h) == up
            self.assertTrue(math.isclose(float(np.sum(h)), float(r.up), rel_tol=0, abs_tol=1e-9))

    def test_deterministic_byte_identical(self):
        a = fir_lowpass_coefficients(96000)
        b = fir_lowpass_coefficients(96000)
        self.assertEqual(a.tobytes(), b.tobytes())

    def test_kaiser_beta_frozen(self):
        self.assertEqual(KAISER_BETA, 9.0)

    def test_accepted_non_gate_rates_generate(self):
        # Experimental rates accepted structurally; cannot close SR-parity alone.
        for fs in ACCEPTED_SAMPLE_RATES:
            h = fir_lowpass_coefficients(fs)
            r = resample_ratio(fs)
            if r.identity:
                self.assertEqual(h.size, 0)
            else:
                self.assertEqual(h.size, r.num_taps)


class FeatureFrameStubTests(unittest.TestCase):
    def test_happy_path(self):
        out = validate_feature_frame_stub(_minimal_valid_frame())
        self.assertEqual(out["schema"], FEATURE_FRAME_SCHEMA)
        self.assertEqual(len(out["mid_psd_db"]), GRID_BANDS)

    def test_required_field_set_matches_section_7(self):
        names = feature_frame_field_names()
        self.assertIn("schema", names)
        self.assertIn("mid_psd_db", names)
        self.assertIn("mid_prominence_db", names)
        self.assertIn("reason", names)
        self.assertEqual(len(names), len(set(names)))

    def test_reject_wrong_band_length(self):
        frame = _minimal_valid_frame(mid_psd_db=[-120.0] * (GRID_BANDS - 1))
        with self.assertRaises(FeatureFrameError) as ctx:
            validate_feature_frame_stub(frame)
        self.assertIn("mid_psd_db", str(ctx.exception))

    def test_reject_nan(self):
        vec = [-120.0] * GRID_BANDS
        vec[0] = float("nan")
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(_minimal_valid_frame(mid_psd_db=vec))

    def test_reject_inf_level(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(mid_level_dbfs=float("inf"))
            )

    def test_reject_valid_reason_inconsistency(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(valid=True, reason="oops")
            )
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(
                    mid_valid=False, side_valid=False, valid=False, reason=None
                )
            )

    def test_reject_valid_not_or_of_channels(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(
                    mid_valid=False, side_valid=False, valid=True, reason=None
                )
            )

    def test_reject_wrong_schema(self):
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(
                _minimal_valid_frame(schema="aieq-v3-feature-frame-0")
            )

    def test_reject_unknown_field(self):
        frame = _minimal_valid_frame()
        frame["extra"] = 1
        with self.assertRaises(FeatureFrameError):
            validate_feature_frame_stub(frame)


if __name__ == "__main__":
    unittest.main()
