"""G1b T2 / spike WS3 — offline≡chunk feature bit-identity (mini gate 3).

Schedules: {1, 8193, geometric-32} from metrology lock.
Full fixture-length / 7-schedule matrix deferred to WS4 harness.
Prominence remains unclamped (CC watch); do not "fix" by clamping.
"""
from __future__ import annotations

import unittest

import numpy as np

from ml_v3.contracts.metrology_lock import (
    GEOMETRIC_CHUNK_SCHEDULE,
    HOP_SAMPLES,
    N_LF,
    STREAMING_FLOAT32_FRAME_FIELDS,
    STREAMING_RATIONAL_TIMESTAMP_FIELDS,
    STREAMING_VALIDITY_FIELDS,
    gate_platform_python_label,
    require_gate_platform_python,
)
from ml_v3.frontend.offline_features import (
    StreamingFeatureExtractor,
    extract_chunked_feature_frames,
    extract_offline_feature_frames,
)

# Spike adversarial schedules (plan WS3). Full FIXED_CHUNK_SCHEDULES → official G1b.
SPIKE_FIXED_SCHEDULES: tuple[int, ...] = (1, 8193)

# Meaningful length: first LF frame + several hops (delta history exercised).
N_HOPS: int = 8
N_CANON_TARGET: int = N_LF + N_HOPS * HOP_SAMPLES  # 8192 + 8192 = 16384

# Float32 band / scalar field names (strip [120] suffix from lock labels).
_FLOAT32_KEYS: tuple[str, ...] = tuple(
    name.split("[", 1)[0] for name in STREAMING_FLOAT32_FRAME_FIELDS
)


def _tone(n: int, hz: float, fs: int = 48000, amp: float = 0.25) -> np.ndarray:
    t = np.arange(n, dtype=np.float64) / float(fs)
    return (amp * np.sin(2.0 * np.pi * hz * t)).astype(np.float32)


def _multitone(n: int, fs: int = 48000, amp: float = 0.15) -> np.ndarray:
    """Compact multitone-like adversarial (not full fixture render)."""
    tones = (60.0, 250.0, 1000.0, 3500.0, 8000.0)
    acc = np.zeros(n, dtype=np.float64)
    t = np.arange(n, dtype=np.float64) / float(fs)
    for hz in tones:
        acc += amp * np.sin(2.0 * np.pi * hz * t)
    peak = float(np.max(np.abs(acc)))
    if peak > 0.0:
        acc *= 0.9 / peak
    return acc.astype(np.float32)


def _host_len_for_canonical(fs_in: int, n_canon: int) -> int:
    """Minimum host samples so ceil(n*up/down) >= n_canon."""
    from ml_v3.frontend.resampler_coeffs import resample_ratio

    r = resample_ratio(fs_in)
    if r.identity:
        return int(n_canon)
    # n such that (n*up + down - 1)//down >= n_canon
    # ≈ ceil(n_canon * down / up)
    return int((n_canon * r.down + r.up - 1) // r.up)


def _assert_frame_sequences_bit_identical(
    offline: list[dict],
    chunked: list[dict],
    *,
    ctx: str,
) -> None:
    assert len(offline) == len(chunked), (
        f"{ctx}: frame count offline={len(offline)} chunked={len(chunked)}"
    )
    assert len(offline) >= 1, f"{ctx}: expected ≥1 emitted frame"
    for i, (fa, fb) in enumerate(zip(offline, chunked)):
        for key in STREAMING_RATIONAL_TIMESTAMP_FIELDS:
            assert fa[key] == fb[key], f"{ctx} frame[{i}].{key}: {fa[key]}≠{fb[key]}"
        for key in STREAMING_VALIDITY_FIELDS:
            assert fa[key] == fb[key], f"{ctx} frame[{i}].{key}: {fa[key]}≠{fb[key]}"
        for key in _FLOAT32_KEYS:
            a = np.asarray(fa[key], dtype=np.float32)
            b = np.asarray(fb[key], dtype=np.float32)
            assert a.tobytes() == b.tobytes(), (
                f"{ctx} frame[{i}].{key}: offline≠chunk float32 bytes"
            )


class GatePlatformGuard(unittest.TestCase):
    def test_gate_platform_is_cpython_312_13(self):
        require_gate_platform_python()
        self.assertEqual(gate_platform_python_label(), "CPython 3.12.13")


class OfflineIsSingleChunkStreamingTests(unittest.TestCase):
    def test_offline_matches_chunked_full_buffer_schedule(self):
        require_gate_platform_python()
        n = _host_len_for_canonical(48000, N_CANON_TARGET)
        x = _multitone(n, fs=48000)
        offline = extract_offline_feature_frames(x, 48000)
        # One chunk spanning the entire host buffer.
        one = extract_chunked_feature_frames(x, 48000, schedule=n)
        _assert_frame_sequences_bit_identical(
            offline, one, ctx="offline≡schedule=N"
        )


class OfflineChunkBitIdentityTests(unittest.TestCase):
    """Stop-early gate: feature frames offline ≡ chunked (byte-identical)."""

    def _assert_schedules(self, fs_in: int, x: np.ndarray, *, label: str) -> None:
        require_gate_platform_python()
        offline = extract_offline_feature_frames(x, fs_in)
        self.assertGreaterEqual(len(offline), 2, msg=f"{label}: need ≥2 frames")
        for fixed in SPIKE_FIXED_SCHEDULES:
            chunked = extract_chunked_feature_frames(x, fs_in, fixed)
            _assert_frame_sequences_bit_identical(
                offline, chunked, ctx=f"{label} schedule={fixed}"
            )
        geo = extract_chunked_feature_frames(x, fs_in, GEOMETRIC_CHUNK_SCHEDULE)
        _assert_frame_sequences_bit_identical(
            offline, geo, ctx=f"{label} schedule=geometric-32"
        )

    def test_48k_multitone_spike_schedules(self):
        # Primary meaningful length for {1, 8193, geometric}.
        n = _host_len_for_canonical(48000, N_CANON_TARGET)
        self._assert_schedules(48000, _multitone(n, fs=48000), label="48k/multitone")

    def test_48k_short_sine_spike_schedules(self):
        n = _host_len_for_canonical(48000, N_CANON_TARGET)
        self._assert_schedules(48000, _tone(n, 1000.0, fs=48000), label="48k/sine")

    def test_48k_stereo_spike_schedules(self):
        n = _host_len_for_canonical(48000, N_CANON_TARGET)
        left = _tone(n, 1000.0, fs=48000, amp=0.2)
        right = _tone(n, 250.0, fs=48000, amp=0.15)
        stereo = np.stack([left, right], axis=1)
        self._assert_schedules(48000, stereo, label="48k/stereo")

    def test_96k_multitone_spike_schedules(self):
        # Downsample path; schedule=1 still feasible (short FIR).
        n = _host_len_for_canonical(96000, N_CANON_TARGET)
        self._assert_schedules(96000, _multitone(n, fs=96000), label="96k/multitone")

    def test_44100_multitone_8193_and_geometric(self):
        """44.1k long-FIR: 8193 + geometric on meaningful length.

        schedule=1 at full meaningful length is deferred to WS4 harness
        (polyphase 20481 taps × ~11k host process(1) calls); covered at
        shorter emit-1 length below and at 48k/96k for all three schedules.
        """
        require_gate_platform_python()
        n = _host_len_for_canonical(44100, N_CANON_TARGET)
        x = _multitone(n, fs=44100)
        offline = extract_offline_feature_frames(x, 44100)
        self.assertGreaterEqual(len(offline), 2)
        for sched, name in (
            (8193, "8193"),
            (GEOMETRIC_CHUNK_SCHEDULE, "geometric-32"),
        ):
            chunked = extract_chunked_feature_frames(x, 44100, sched)
            _assert_frame_sequences_bit_identical(
                offline, chunked, ctx=f"44.1k/multitone schedule={name}"
            )

    def test_44100_schedule_1_short_emit(self):
        """schedule=1 on shorter 44.1k slice that still emits ≥1 frame."""
        require_gate_platform_python()
        n = _host_len_for_canonical(44100, N_LF + HOP_SAMPLES)
        x = _tone(n, 1000.0, fs=44100, amp=0.2)
        offline = extract_offline_feature_frames(x, 44100)
        self.assertGreaterEqual(len(offline), 1)
        chunked = extract_chunked_feature_frames(x, 44100, 1)
        _assert_frame_sequences_bit_identical(
            offline, chunked, ctx="44.1k/sine schedule=1 short"
        )


class MultiAssetDeltaResetTests(unittest.TestCase):
    def test_explicit_delta_reset_at_asset_boundary(self):
        """Smoke: reset_delta_history → next valid frame mid_delta all-zero."""
        require_gate_platform_python()
        n = _host_len_for_canonical(48000, N_LF + 3 * HOP_SAMPLES)
        a = _tone(n, 1000.0, amp=0.2)
        b = _tone(n, 500.0, amp=0.2)
        ext = StreamingFeatureExtractor(48000)
        fa = ext.process(a)
        self.assertGreaterEqual(len(fa), 2)
        # Without reset, continuing would carry shape history — reset instead.
        ext.reset_delta_history()
        fb = ext.process(b)
        self.assertGreaterEqual(len(fb), 1)
        # First frame after reset must have zero mid_delta when mid_valid.
        first = fb[0]
        if first["mid_valid"]:
            self.assertTrue(all(v == 0.0 for v in first["mid_delta_db"]))


class InterleavedSilenceProofBTests(unittest.TestCase):
    """Lock also_required (b): silence interleaved between assets.

    Stream = asset_a || silence || asset_b. Silence frames are invalid and
    clear delta history identically on offline monolith vs chunked schedules.
    """

    def _concat_stream(self, fs: int) -> np.ndarray:
        n_a = _host_len_for_canonical(fs, N_LF + 3 * HOP_SAMPLES)
        n_sil = _host_len_for_canonical(fs, N_LF + 2 * HOP_SAMPLES)
        n_b = _host_len_for_canonical(fs, N_LF + 3 * HOP_SAMPLES)
        a = _tone(n_a, 1000.0, fs=fs, amp=0.2)
        sil = np.zeros(n_sil, dtype=np.float32)
        b = _tone(n_b, 500.0, fs=fs, amp=0.2)
        return np.concatenate([a, sil, b])

    def test_48k_interleaved_silence_spike_schedules(self):
        require_gate_platform_python()
        x = self._concat_stream(48000)
        offline = extract_offline_feature_frames(x, 48000)
        self.assertGreaterEqual(len(offline), 3)
        # At least one invalid (silence) frame must appear.
        self.assertTrue(any(not f["valid"] for f in offline))
        for fixed in SPIKE_FIXED_SCHEDULES:
            chunked = extract_chunked_feature_frames(x, 48000, fixed)
            _assert_frame_sequences_bit_identical(
                offline, chunked, ctx=f"proof-b schedule={fixed}"
            )
        geo = extract_chunked_feature_frames(x, 48000, GEOMETRIC_CHUNK_SCHEDULE)
        _assert_frame_sequences_bit_identical(
            offline, geo, ctx="proof-b schedule=geometric-32"
        )

    def test_silence_clears_delta_before_second_asset(self):
        """After interleaved silence, first re-valid mid_delta is all-zero."""
        require_gate_platform_python()
        x = self._concat_stream(48000)
        frames = extract_offline_feature_frames(x, 48000)
        saw_invalid = False
        first_revalid = None
        for f in frames:
            if not f["valid"]:
                saw_invalid = True
                continue
            if saw_invalid and first_revalid is None:
                first_revalid = f
                break
        self.assertIsNotNone(first_revalid)
        assert first_revalid is not None
        self.assertTrue(first_revalid["mid_valid"])
        self.assertTrue(all(v == 0.0 for v in first_revalid["mid_delta_db"]))


class FailClosedStreamingTests(unittest.TestCase):
    def test_layout_change_rejected(self):
        ext = StreamingFeatureExtractor(48000)
        ext.process(_tone(64, 440.0))
        stereo = np.stack([_tone(64, 440.0), _tone(64, 220.0)], axis=1)
        with self.assertRaises(Exception) as ctx:
            ext.process(stereo)
        self.assertIn("layout", str(ctx.exception).lower())


if __name__ == "__main__":
    unittest.main()
