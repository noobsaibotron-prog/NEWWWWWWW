"""G1a M2 / T6-prep — frozen fixture-spec v1 happy + reject paths.

Does not generate audio. Spec commit is gated separately (STOP for CC).
"""
from __future__ import annotations

import hashlib
import math
import unittest
from fractions import Fraction
from pathlib import Path

from ml_v3.contracts.canonical import loads_strict, sha256_of_obj, write_canonical
from ml_v3.contracts.constants import (
    CANONICAL_SAMPLE_RATE,
    CONTRACT_REVISION,
    GATE_SAMPLE_RATES,
)
from ml_v3.contracts.fixture_spec import (
    DURATION_DEN,
    DURATION_NUM,
    FIXTURE_SPEC_ARTIFACT_ID,
    MULTITONE_HZ,
    PSEUDO_NOISE_PRNG_SEED,
    SWEEP_T_END_DEN,
    SWEEP_T_END_NUM,
    SWEEP_T_START_DEN,
    SWEEP_T_START_NUM,
    ULTRASONIC_HZ,
    FixtureSpecError,
    _useful_frame_times,
    assert_sweep_checkpoints_reachable,
    common_useful_window,
    duration_seconds,
    fixture_spec_bytes,
    fixture_spec_sha256,
    frozen_fixture_spec,
    nearest_useful_frame_distance,
    sample_count,
    sweep_active_end_seconds,
    sweep_active_start_seconds,
    sweep_crossing_time,
    validate_fixture_spec_claim,
)
from ml_v3.contracts.metrology_lock import (
    HOP_SAMPLES,
    N_LF,
    SWEEP_CHECKPOINT_HZ,
    coda_seconds,
    metrology_lock_sha256,
    resampler_group_delay_rational,
    warm_up_seconds,
)

FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "fixtures" / "g1" / "fixture_spec_v1.json"
)


class FrozenFixtureSpecHappyPathTests(unittest.TestCase):
    def test_duration_preregistered_not_false_formula(self):
        spec = frozen_fixture_spec()
        duration = spec["global_duration"]
        self.assertEqual(duration["kind"], "preregistered_parameter")
        self.assertTrue(duration["not_a_contract_formula"])
        self.assertEqual(
            duration["false_formula_forbidden"], "warm_up + coda + T_min")
        self.assertEqual(duration["duration_num"], DURATION_NUM)
        self.assertEqual(duration["duration_den"], DURATION_DEN)
        self.assertEqual(duration["duration_s"], 2.0)
        self.assertEqual(duration_seconds(), 2.0)
        bound = duration["bound_check"]
        max_bound = (
            bound["max_warm_up_plus_coda_num"]
            / bound["max_warm_up_plus_coda_den"]
        )
        self.assertGreater(duration_seconds(), max_bound)
        # Useful portion long enough for mode (a); not defined by T_min.
        self.assertGreater(bound["useful_portion_hops_at_fs_c"], 32.0)
        self.assertTrue(bound["mode_a_gate_closing"])
        self.assertIn("T_min", bound["rationale"])
        self.assertIn("mode-(b)", bound["rationale"])

    def test_sample_counts_exact_at_gate_rates(self):
        for fs in GATE_SAMPLE_RATES:
            self.assertEqual(sample_count(fs), 2 * fs)
        n_map = frozen_fixture_spec()["global_duration"]["N_at_gate_rates"]
        self.assertEqual(n_map["44100"], 88200)
        self.assertEqual(n_map["48000"], 96000)
        self.assertEqual(n_map["96000"], 192000)

    def test_duration_exceeds_per_rate_wu_plus_coda(self):
        coda = 32 / 375
        for fs in GATE_SAMPLE_RATES:
            self.assertGreater(
                duration_seconds(), warm_up_seconds(fs) + coda)

    def test_numeric_generation_pins(self):
        num = frozen_fixture_spec()["numeric_generation"]
        self.assertEqual(num["compute_dtype"], "float64")
        self.assertEqual(num["artifact_dtype"], "float32")
        self.assertEqual(num["post_render_normalization"], "forbidden")
        self.assertEqual(num["cross_rate_resample"], "forbidden")
        self.assertIn("DIRECTLY", num["render_policy"])
        self.assertEqual(num["file_format"]["container"], "wav")
        self.assertEqual(num["file_format"]["encoding"], "pcm_float32_le")

    def test_pseudo_noise_fully_pinned(self):
        noise = frozen_fixture_spec()["categories"]["pseudo_noise"]
        self.assertEqual(noise["prng_seed"], PSEUDO_NOISE_PRNG_SEED)
        self.assertEqual(PSEUDO_NOISE_PRNG_SEED, 31051986)
        self.assertEqual(noise["n_partials"], 512)
        self.assertEqual(noise["k_range"], [0, 511])
        self.assertIn("PCG64(31051986)", noise["phase_rule"])
        self.assertIn("2*pi*rng.random()", noise["phase_rule"])
        self.assertIn("k=0..511", noise["phase_rule"])
        expected_amp = (10.0 ** (-24.0 / 20.0)) * math.sqrt(2.0 / 512.0)
        self.assertAlmostEqual(noise["amplitude_peak_each"], expected_amp, places=15)
        self.assertIn("no renormalize", noise["waveform"])

    def test_categories_cover_section_13(self):
        cats = frozen_fixture_spec()["categories"]
        self.assertEqual(
            cats["multitone"]["frequencies_hz"], list(MULTITONE_HZ))
        self.assertEqual(
            cats["ultrasonic_96k"]["frequencies_hz"], list(ULTRASONIC_HZ))
        self.assertEqual(
            cats["ultrasonic_96k"]["presentation"], "simultaneous")
        self.assertEqual(
            cats["ultrasonic_96k"]["gate_sample_rates"], [96000])
        sweep = cats["log_sweep"]
        self.assertEqual(sweep["sweep_law"], "exponential_log_chirp")
        active = sweep["active_interval"]
        self.assertEqual(active["t_start_num"], SWEEP_T_START_NUM)
        self.assertEqual(active["t_start_den"], SWEEP_T_START_DEN)
        self.assertEqual(active["t_end_num"], SWEEP_T_END_NUM)
        self.assertEqual(active["t_end_den"], SWEEP_T_END_DEN)
        self.assertEqual(active["t_start_s"], 0.5)
        self.assertEqual(active["t_end_s"], 1.75)
        self.assertEqual(active["outside_active_sample"], 0.0)
        self.assertIn("T_active", sweep["instantaneous_freq_hz"])
        self.assertNotIn(
            "**(t/T), T=duration_s",
            sweep["instantaneous_freq_hz"].replace(" ", ""),
        )
        self.assertIn("peak = −24 dBFS", sweep["amplitude_formula"])
        layouts = cats["decorrelated_stereo"]["layouts"]
        self.assertIn("mid_only", layouts)
        self.assertIn("side_only", layouts)
        self.assertIn("decorrelated", layouts)
        self.assertEqual(
            layouts["decorrelated"]["right_prng_seed"], 31051987)
        patterns = cats["silence_non_finite"]["non_finite_patterns"]
        injects = {p["inject"] for p in patterns}
        self.assertEqual(injects, {"NaN", "+Inf", "-Inf"})

    def test_envelope_authority_honest(self):
        prose = frozen_fixture_spec()["envelope_authority"]
        self.assertIn("freeze-from-prose", prose)
        self.assertIn("NO REV7", prose)
        self.assertIn("no literal JSON key table", prose)
        self.assertTrue(
            frozen_fixture_spec()["no_audio_generators_in_this_artifact"])

    def test_dependencies_bind_metrology_lock(self):
        deps = frozen_fixture_spec()["dependencies"]
        self.assertEqual(deps["contract_revision"], CONTRACT_REVISION)
        self.assertEqual(deps["metrology_lock_sha256"], metrology_lock_sha256())
        self.assertEqual(deps["gate_sample_rates"], list(GATE_SAMPLE_RATES))

    def test_sweep_checkpoints_reachable_within_match_radius(self):
        """M2 blocker guard: every lock checkpoint must be gate-closable."""
        assert_sweep_checkpoints_reachable()
        useful_start, useful_end = common_useful_window()
        hop = HOP_SAMPLES / float(CANONICAL_SAMPLE_RATE)
        t0 = sweep_active_start_seconds()
        t1 = sweep_active_end_seconds()
        self.assertEqual(t0, 0.5)
        self.assertEqual(t1, 1.75)
        self.assertLessEqual(useful_start, t0)
        self.assertLessEqual(t1, useful_end)

        for fs in GATE_SAMPLE_RATES:
            self.assertEqual((t0 * fs) % 1, 0.0)
            self.assertEqual((t1 * fs) % 1, 0.0)
            # Reachability must exercise the true V3FeatureFrame grid.
            self.assertTrue(_useful_frame_times(fs))

        crossings = frozen_fixture_spec()["categories"]["log_sweep"][
            "checkpoint_reachability"]["crossing_time_s"]
        for freq in SWEEP_CHECKPOINT_HZ:
            t_cross = sweep_crossing_time(float(freq))
            self.assertEqual(crossings[str(freq)], t_cross)
            self.assertGreaterEqual(t_cross, useful_start)
            self.assertLessEqual(t_cross, useful_end)
            for fs in GATE_SAMPLE_RATES:
                dist = nearest_useful_frame_distance(t_cross, fs)
                self.assertLessEqual(
                    dist, hop,
                    msg=(
                        f"{freq} Hz @ fs={fs}: nearest frame {dist} s > "
                        f"match radius {hop} s"
                    ),
                )

        # Extremes must not sit on asset edges (the pre-fix failure mode).
        self.assertAlmostEqual(
            sweep_crossing_time(20.0), t0, places=12)
        self.assertAlmostEqual(
            sweep_crossing_time(20000.0), t1, places=12)
        self.assertGreater(sweep_crossing_time(20.0), useful_start)
        self.assertLess(sweep_crossing_time(20000.0), useful_end)

    def test_useful_frame_times_match_contractual_source_time_grid(self):
        """REV6: source_time = frame_end/fs_c − delay; warm-up is exclusion only."""
        end = duration_seconds() - coda_seconds()
        for fs in (44100, 48000, 96000):
            delay_num, delay_den, _ = resampler_group_delay_rational(fs)
            delay = Fraction(delay_num, delay_den)
            wu = warm_up_seconds(fs)
            times = _useful_frame_times(fs)
            self.assertTrue(times, msg=f"empty useful grid at fs={fs}")

            expected: list[float] = []
            frame_end = N_LF
            while True:
                source_time = (
                    Fraction(frame_end, CANONICAL_SAMPLE_RATE) - delay
                )
                t = float(source_time)
                if t > end:
                    break
                if t >= wu:
                    expected.append(t)
                frame_end += HOP_SAMPLES
            self.assertEqual(times, expected)

            # Spot-check every returned time against the closed formula.
            for t in times:
                # Invert: frame_end = round((t + delay) * fs_c)
                frame_end_f = (Fraction.from_float(t) + delay) * CANONICAL_SAMPLE_RATE
                frame_end_i = int(round(float(frame_end_f)))
                self.assertEqual(
                    (frame_end_i - N_LF) % HOP_SAMPLES, 0,
                    msg=f"fs={fs} t={t} not on N_LF+m·H lattice",
                )
                recon = float(
                    Fraction(frame_end_i, CANONICAL_SAMPLE_RATE) - delay
                )
                self.assertAlmostEqual(recon, t, places=12)
                self.assertGreaterEqual(t, wu)
                self.assertLessEqual(t, end)

        # Regression: synthetic warm_up+k·hop lattice ≠ contractual grid
        # at a non-identity rate (delay ≠ 0). Prevents silent reintroduction.
        fs_bug = 44100
        delay_num, delay_den, _ = resampler_group_delay_rational(fs_bug)
        self.assertNotEqual((delay_num, delay_den), (0, 1))
        hop = HOP_SAMPLES / float(CANONICAL_SAMPLE_RATE)
        wu = warm_up_seconds(fs_bug)
        synthetic: list[float] = []
        t = wu
        while t <= end + 1e-12:
            if t <= end:
                synthetic.append(t)
            t += hop
        contractual = _useful_frame_times(fs_bug)
        self.assertNotEqual(
            synthetic, contractual,
            msg="synthetic warm_up-origin grid must differ from §6.2+§5 grid",
        )

    def test_canonical_hash_stable_across_calls(self):
        a = fixture_spec_sha256()
        b = fixture_spec_sha256()
        self.assertEqual(a, b)
        self.assertEqual(len(a), 64)
        self.assertEqual(a, hashlib.sha256(fixture_spec_bytes()).hexdigest())
        self.assertEqual(a, sha256_of_obj(frozen_fixture_spec()))

    def test_validate_accepts_frozen_copy(self):
        out = validate_fixture_spec_claim(frozen_fixture_spec())
        self.assertEqual(out["artifact_id"], FIXTURE_SPEC_ARTIFACT_ID)

    def test_golden_fixture_matches_frozen_bytes(self):
        self.assertTrue(FIXTURE.is_file(), f"missing golden {FIXTURE}")
        on_disk = FIXTURE.read_bytes()
        self.assertEqual(on_disk, fixture_spec_bytes())
        parsed = loads_strict(on_disk.decode("utf-8"))
        validate_fixture_spec_claim(parsed)


class RejectPathTests(unittest.TestCase):
    def test_non_object_claim_rejected(self):
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(["not", "an", "object"])

    def test_duration_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["global_duration"]["duration_s"] = 1.0
        claim["global_duration"]["duration_num"] = 1
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("duration", str(ctx.exception).lower())

    def test_false_duration_formula_allowed_rejected(self):
        claim = frozen_fixture_spec()
        claim["global_duration"]["false_formula_forbidden"] = "none"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_pseudo_noise_seed_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["categories"]["pseudo_noise"]["prng_seed"] = 1
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("prng_seed", str(ctx.exception))

    def test_pseudo_noise_phase_rule_unpin_rejected(self):
        claim = frozen_fixture_spec()
        claim["categories"]["pseudo_noise"]["phase_rule"] = "unspecified"
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("phase_rule", str(ctx.exception))

    def test_post_render_normalize_allowed_rejected(self):
        claim = frozen_fixture_spec()
        claim["numeric_generation"]["post_render_normalization"] = "allowed"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_cross_rate_resample_allowed_rejected(self):
        claim = frozen_fixture_spec()
        claim["numeric_generation"]["cross_rate_resample"] = "allowed"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_sweep_law_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["categories"]["log_sweep"]["sweep_law"] = "linear"
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("sweep_law", str(ctx.exception))

    def test_sweep_active_interval_tamper_rejected(self):
        claim = frozen_fixture_spec()
        # Full-asset sweep (the pre-fix blocker) must not validate.
        claim["categories"]["log_sweep"]["active_interval"]["t_start_s"] = 0.0
        claim["categories"]["log_sweep"]["active_interval"]["t_start_num"] = 0
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        msg = str(ctx.exception).lower()
        self.assertTrue(
            "t_start" in msg or "byte-identical" in msg,
            msg=str(ctx.exception),
        )

    def test_category_removed_rejected(self):
        claim = frozen_fixture_spec()
        del claim["categories"]["transient_burst"]
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("missing", str(ctx.exception))

    def test_metrology_dependency_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["dependencies"]["metrology_lock_sha256"] = "0" * 64
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("metrology_lock_sha256", str(ctx.exception))

    def test_file_format_tamper_rejected(self):
        claim = frozen_fixture_spec()
        claim["numeric_generation"]["file_format"]["encoding"] = "pcm_int16_le"
        with self.assertRaises(FixtureSpecError):
            validate_fixture_spec_claim(claim)

    def test_byte_identity_reject_extra_key(self):
        claim = frozen_fixture_spec()
        claim["extra_freedom"] = True
        with self.assertRaises(FixtureSpecError) as ctx:
            validate_fixture_spec_claim(claim)
        self.assertIn("byte-identical", str(ctx.exception))


def _ensure_golden() -> None:
    """Dev helper: rewrite golden if missing (not used by unittest discovery)."""
    write_canonical(FIXTURE, frozen_fixture_spec())


if __name__ == "__main__":
    unittest.main()
