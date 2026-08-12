"""Frozen G1a fixture-spec v1 (§13 signal-generator parameters).

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a.

G1a M2 serializes the implementative freedoms left open by §13 prose
(amplitudes, phases, envelopes, sweep law, stereo layouts, duration,
numeric generation semantics). It does **not** generate audio, does not
consult frontend spike results, and does not amend REV6 (no REV7).

Honesty / envelope_authority:
  §13 lists fixture *categories* and fully pins only part of the
  pseudo-noise recipe in prose. Keys below are freeze-from-prose
  packaging of those freedoms (conservative lab defaults), plus the
  byte-level generation semantics required for bit-identical renders.
  Duration is a PREREGISTERED parameter — not a false formula
  ``warm_up + coda + T_min`` (T_min belongs to diagnostic mode (b);
  SR-parity multitone/noise closes under mode (a) only).

Sequencing: commit+hash this artifact **before** M3/T6 generators.
"""
from __future__ import annotations

import math
from copy import deepcopy
from fractions import Fraction
from typing import Any

from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import (
    CANONICAL_SAMPLE_RATE,
    CONTRACT_REVISION,
    GATE_SAMPLE_RATES,
)
from .metrology_lock import (
    HOP_SAMPLES,
    N_LF,
    SWEEP_CHECKPOINT_HZ,
    coda_seconds,
    metrology_lock_sha256,
    resampler_group_delay_rational,
    warm_up_seconds,
)

__all__ = [
    "FixtureSpecError",
    "FIXTURE_SPEC_ARTIFACT_ID",
    "FIXTURE_SPEC_SECTION",
    "FIXTURE_SPEC_VERSION",
    "DURATION_NUM",
    "DURATION_DEN",
    "PSEUDO_NOISE_PRNG_SEED",
    "MULTITONE_HZ",
    "ULTRASONIC_HZ",
    "SWEEP_F_START_HZ",
    "SWEEP_F_END_HZ",
    "SWEEP_T_START_NUM",
    "SWEEP_T_START_DEN",
    "SWEEP_T_END_NUM",
    "SWEEP_T_END_DEN",
    "frozen_fixture_spec",
    "fixture_spec_bytes",
    "fixture_spec_sha256",
    "validate_fixture_spec_claim",
    "duration_seconds",
    "sample_count",
    "sweep_active_start_seconds",
    "sweep_active_end_seconds",
    "sweep_crossing_time",
    "common_useful_window",
    "nearest_useful_frame_distance",
    "assert_sweep_checkpoints_reachable",
]

FIXTURE_SPEC_ARTIFACT_ID = "aieq-v3-fixture-spec-1"
FIXTURE_SPEC_SECTION = "13"
FIXTURE_SPEC_VERSION = 1

# Preregistered global duration (exact rational). N(fs) = duration_num * fs
# is integer for every GATE_SAMPLE_RATES entry.
DURATION_NUM: int = 2
DURATION_DEN: int = 1

PSEUDO_NOISE_PRNG_SEED: int = 31051986
PSEUDO_NOISE_PARTIALS: int = 512
PSEUDO_NOISE_F_LO_HZ: float = 20.0
PSEUDO_NOISE_F_HI_HZ: float = 20000.0
PSEUDO_NOISE_RMS_DBFS: float = -24.0

MULTITONE_HZ: tuple[int, ...] = (
    45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000,
)
ULTRASONIC_HZ: tuple[int, ...] = (28000, 32000, 40000)
SWEEP_F_START_HZ: float = 20.0
SWEEP_F_END_HZ: float = 20000.0

# Active log-sweep interval inside the common useful window (exact rationals).
# Global asset stays 0..2 s; only the chirp occupies [t_start, t_end].
# 1/2 and 7/4 are integer sample counts at every GATE_SAMPLE_RATES entry.
SWEEP_T_START_NUM: int = 1
SWEEP_T_START_DEN: int = 2
SWEEP_T_END_NUM: int = 7
SWEEP_T_END_DEN: int = 4

# Lock rationals used only to document duration bounds (not to derive it).
_MAX_WARM_UP_PLUS_CODA_NUM: int = 18896  # 44100: wu+coda = 18896/55125
_MAX_WARM_UP_PLUS_CODA_DEN: int = 55125
_CODA_NUM: int = 32
_CODA_DEN: int = 375


class FixtureSpecError(ValueError):
    """Raised when a fixture-spec claim is malformed or non-canonical."""


def duration_seconds() -> float:
    return DURATION_NUM / DURATION_DEN


def sample_count(fs: int) -> int:
    """Exact sample count at gate rate ``fs`` (no rounding ambiguity)."""
    if fs not in GATE_SAMPLE_RATES:
        raise FixtureSpecError(
            f"fs {fs} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")
    if DURATION_DEN != 1:
        if (DURATION_NUM * fs) % DURATION_DEN != 0:
            raise FixtureSpecError(
                f"duration {DURATION_NUM}/{DURATION_DEN} is not an integer "
                f"number of samples at fs={fs}")
        return (DURATION_NUM * fs) // DURATION_DEN
    return DURATION_NUM * fs


def sweep_active_start_seconds() -> float:
    return SWEEP_T_START_NUM / SWEEP_T_START_DEN


def sweep_active_end_seconds() -> float:
    return SWEEP_T_END_NUM / SWEEP_T_END_DEN


def common_useful_window() -> tuple[float, float]:
    """Cross-SR useful source-time window (after max warm-up, before coda)."""
    start = max(warm_up_seconds(fs) for fs in GATE_SAMPLE_RATES)
    end = duration_seconds() - coda_seconds()
    return start, end


def sweep_crossing_time(freq_hz: float) -> float:
    """Source time at which the active log-sweep crosses ``freq_hz``."""
    if freq_hz <= 0.0:
        raise FixtureSpecError("sweep checkpoint frequency must be positive")
    t0 = sweep_active_start_seconds()
    t1 = sweep_active_end_seconds()
    if t1 <= t0:
        raise FixtureSpecError("sweep active end must exceed active start")
    ratio = SWEEP_F_END_HZ / SWEEP_F_START_HZ
    # f(t) = f_start * ratio**u, u=(t-t0)/(t1-t0)  →  u = log(f/f_start)/log(ratio)
    u = math.log(freq_hz / SWEEP_F_START_HZ) / math.log(ratio)
    return t0 + u * (t1 - t0)


def _hop_seconds() -> float:
    return HOP_SAMPLES / float(CANONICAL_SAMPLE_RATE)


def _useful_frame_times(fs: int) -> list[float]:
    """V3FeatureFrame ``source_time`` grid inside the useful segment (§6.2+§5).

    Contractual timeline (not a synthetic warm-up-origin hop lattice):

      frame_end_sample = N_LF + m·H
      source_time = frame_end_sample / fs_c − resampler_group_delay_seconds(fs)

    ``warm_up_seconds(fs)`` is an *exclusion threshold* (with coda); it is not
    the grid origin. Rationals are kept until the final float compare/list.
    """
    if fs not in GATE_SAMPLE_RATES:
        raise FixtureSpecError(
            f"fs {fs} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")
    delay_num, delay_den, _ = resampler_group_delay_rational(fs)
    delay = Fraction(delay_num, delay_den)
    warm_up = warm_up_seconds(fs)
    end = duration_seconds() - coda_seconds()
    frame_end = N_LF
    times: list[float] = []
    while True:
        source_time = Fraction(frame_end, CANONICAL_SAMPLE_RATE) - delay
        t = float(source_time)
        if t > end:
            break
        if t >= warm_up:
            times.append(t)
        frame_end += HOP_SAMPLES
    return times


def nearest_useful_frame_distance(t_cross: float, fs: int) -> float:
    """Distance from ``t_cross`` to nearest useful V3FeatureFrame source_time."""
    frames = _useful_frame_times(fs)
    if not frames:
        raise FixtureSpecError(f"no useful frames at fs={fs}")
    return min(abs(t - t_cross) for t in frames)


def assert_sweep_checkpoints_reachable() -> None:
    """Fail closed if any lock checkpoint is unreachable under match radius.

    Contract requirement (metrology lock sweep_log_parity): for every
    checkpoint frequency, the nearest useful-segment frame at each gate
    rate must lie within ``H / fs_c`` of the crossing time. Crossing times
    must also fall inside the common useful window.
    """
    useful_start, useful_end = common_useful_window()
    t0 = sweep_active_start_seconds()
    t1 = sweep_active_end_seconds()
    hop = _hop_seconds()

    if not (useful_start <= t0 < t1 <= useful_end):
        raise FixtureSpecError(
            f"sweep active interval [{t0}, {t1}] must lie inside common "
            f"useful window [{useful_start}, {useful_end}]")

    for fs in GATE_SAMPLE_RATES:
        # Active bounds must be exact sample indices (no fractional sample).
        n0 = t0 * fs
        n1 = t1 * fs
        if abs(n0 - round(n0)) > 1e-9 or abs(n1 - round(n1)) > 1e-9:
            raise FixtureSpecError(
                f"sweep active bounds must be sample-exact at fs={fs} "
                f"(t_start*fs={n0}, t_end*fs={n1})")

    for freq in SWEEP_CHECKPOINT_HZ:
        t_cross = sweep_crossing_time(float(freq))
        if not (useful_start <= t_cross <= useful_end):
            raise FixtureSpecError(
                f"checkpoint {freq} Hz crosses at t={t_cross} outside "
                f"common useful window [{useful_start}, {useful_end}]")
        for fs in GATE_SAMPLE_RATES:
            dist = nearest_useful_frame_distance(t_cross, fs)
            if dist > hop + 1e-12:
                raise FixtureSpecError(
                    f"checkpoint {freq} Hz at t={t_cross}: nearest useful "
                    f"frame at fs={fs} is {dist} s away (match radius "
                    f"H/fs_c={hop})")


def _peak_amp_equal_power(n: int, rms_dbfs: float = PSEUDO_NOISE_RMS_DBFS) -> float:
    """Peak amplitude per equal-power partial for target aggregate RMS."""
    if n <= 0:
        raise FixtureSpecError("n partials must be positive")
    return (10.0 ** (rms_dbfs / 20.0)) * math.sqrt(2.0 / n)


def _dbfs_amplitude_linear(dbfs: float = PSEUDO_NOISE_RMS_DBFS) -> float:
    """Linear amplitude for a declared dBFS level (not sine-RMS conversion).

    For log_sweep, the fixture deliberately preregisters
    ``amplitude_peak = 10**(-24/20)`` (peak = −24 dBFS), not the RMS-of-sine
    convention peak = 10**(-24/20)*sqrt(2).
    """
    return 10.0 ** (dbfs / 20.0)


def frozen_fixture_spec() -> dict[str, Any]:
    """Return a deep copy of the frozen §13 fixture-spec v1 artifact."""
    # Self-check: never serialize a sweep that cannot close the lock gate.
    assert_sweep_checkpoints_reachable()

    peak_noise = _peak_amp_equal_power(PSEUDO_NOISE_PARTIALS)
    peak_multitone = _peak_amp_equal_power(len(MULTITONE_HZ))
    peak_ultra = _peak_amp_equal_power(len(ULTRASONIC_HZ))
    # Deliberate packaging: peak amplitude = −24 dBFS (not RMS-of-sine).
    peak_tone = _dbfs_amplitude_linear()

    max_wu = max(warm_up_seconds(fs) for fs in GATE_SAMPLE_RATES)
    coda = _CODA_NUM / _CODA_DEN
    useful = duration_seconds() - max_wu - coda
    useful_start, useful_end = common_useful_window()
    t_start = sweep_active_start_seconds()
    t_end = sweep_active_end_seconds()
    t_active = t_end - t_start
    hop = _hop_seconds()
    checkpoint_crossings = {
        str(freq): sweep_crossing_time(float(freq))
        for freq in SWEEP_CHECKPOINT_HZ
    }

    return {
        "artifact_id": FIXTURE_SPEC_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": FIXTURE_SPEC_SECTION,
        "spec_version": FIXTURE_SPEC_VERSION,
        "envelope_authority": (
            "freeze-from-prose packaging of §13 fixture freedoms "
            "(amplitudes/phases/envelopes/sweep law/stereo/duration/"
            "numeric generation semantics); contract has no literal JSON "
            "key table for this artifact; NO REV7 — packaging only; "
            "does not implement audio generators (M3/T6 after CC+commit)"
        ),
        "no_audio_generators_in_this_artifact": True,
        "dependencies": {
            "contract_revision": CONTRACT_REVISION,
            "metrology_lock_sha256": metrology_lock_sha256(),
            "gate_sample_rates": list(GATE_SAMPLE_RATES),
        },
        "numeric_generation": {
            "sample_index_rule": "n = 0 .. N-1",
            "time_rule": "t = n / fs  (float64 division)",
            "N_rule": "N = duration_num * fs / duration_den (exact integer)",
            "compute_dtype": "float64",
            "artifact_dtype": "float32",
            "sum_order": (
                "ascending partial index k (or ascending frequency list "
                "order); accumulate in float64 before cast"
            ),
            "prng": "numpy.random.Generator(numpy.random.PCG64(seed))",
            "prng_draw_order": (
                "sequential rng.random() calls in ascending k; "
                "never reverse, never vector-size ambiguity vs loop"
            ),
            "post_render_normalization": "forbidden",
            "dc_removal": "forbidden",
            "cast_policy": (
                "after full float64 render, cast each sample to IEEE754 "
                "binary32 round-to-nearest-even (language/numpy default "
                "float64→float32); no dither"
            ),
            "cross_rate_resample": "forbidden",
            "render_policy": (
                "generate DIRECTLY at each of 44100/48000/96000; never "
                "resample one rate to another"
            ),
            "file_format": {
                "container": "wav",
                "encoding": "pcm_float32_le",
                "byte_order": "little_endian",
                "channels_layout": "interleaved_LRLR_for_stereo",
                "header_sample_rate_matches_render_fs": True,
                "non_finite_encoding": (
                    "same WAV PCM float32 LE container; inject IEEE754 "
                    "binary32 NaN/+Inf/-Inf bit patterns at pinned samples"
                ),
            },
        },
        "global_duration": {
            "kind": "preregistered_parameter",
            "not_a_contract_formula": True,
            "false_formula_forbidden": "warm_up + coda + T_min",
            "duration_num": DURATION_NUM,
            "duration_den": DURATION_DEN,
            "duration_s": duration_seconds(),
            "N_at_gate_rates": {
                str(fs): sample_count(fs) for fs in GATE_SAMPLE_RATES
            },
            "bound_check": {
                "must_exceed": "max_sr(warm_up_sr + coda_sr)",
                "max_warm_up_plus_coda_num": _MAX_WARM_UP_PLUS_CODA_NUM,
                "max_warm_up_plus_coda_den": _MAX_WARM_UP_PLUS_CODA_DEN,
                "max_warm_up_plus_coda_s": (
                    _MAX_WARM_UP_PLUS_CODA_NUM / _MAX_WARM_UP_PLUS_CODA_DEN
                ),
                "coda_num": _CODA_NUM,
                "coda_den": _CODA_DEN,
                "useful_portion_s_at_max_wu": useful,
                "useful_portion_hops_at_fs_c": useful / (HOP_SAMPLES / 48000.0),
                "mode_a_gate_closing": True,
                "rationale": (
                    "conservative lab default: duration_s=2 > "
                    "max(warm_up+coda)≈0.3428 s (lock rationals "
                    "18896/55125) so mode-(a) useful segment is long "
                    "enough (~77 hops @ fs_c) for SR-parity multitone/"
                    "noise; T_min=8H/fs_c is mode-(b) diagnostic only "
                    "and must not define duration"
                ),
            },
        },
        "categories": {
            "multitone": {
                "frequencies_hz": list(MULTITONE_HZ),
                "amplitude_peak_each": peak_multitone,
                "amplitude_formula": (
                    "10**(-24/20) * sqrt(2/N) with N=len(frequencies_hz); "
                    "equal-power packaging aligned with §13 pseudo-noise"
                ),
                "phases_rad": [0.0] * len(MULTITONE_HZ),
                "phase_policy": "all_zero_deterministic",
                "channels": 1,
                "duration_ref": "global_duration",
                "waveform": (
                    "sum_k A * sin(2*pi*f_k*t + phi_k) in float64; "
                    "k ascending in frequencies_hz order"
                ),
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "log_sweep": {
                "f_start_hz": SWEEP_F_START_HZ,
                "f_end_hz": SWEEP_F_END_HZ,
                "amplitude_peak": peak_tone,
                "amplitude_formula": (
                    "10**(-24/20) preregistered peak amplitude "
                    "(peak = −24 dBFS; not RMS-of-sine * sqrt(2))"
                ),
                "phase_at_active_start_rad": 0.0,
                "sweep_law": "exponential_log_chirp",
                "active_interval": {
                    "kind": "preregistered_parameter",
                    "t_start_num": SWEEP_T_START_NUM,
                    "t_start_den": SWEEP_T_START_DEN,
                    "t_end_num": SWEEP_T_END_NUM,
                    "t_end_den": SWEEP_T_END_DEN,
                    "t_start_s": t_start,
                    "t_end_s": t_end,
                    "T_active_s": t_active,
                    "outside_active_sample": 0.0,
                    "rationale": (
                        "global asset remains duration_s=2; chirp occupies "
                        "only [t_start, t_end] inside the common useful "
                        "window so every SWEEP_CHECKPOINT_HZ crossing is "
                        "reachable within match radius H/fs_c"
                    ),
                },
                "checkpoint_reachability": {
                    "common_useful_start_s": useful_start,
                    "common_useful_end_s": useful_end,
                    "match_radius_s": hop,
                    "match_radius_formula": "H / fs_c",
                    "checkpoint_hz": list(SWEEP_CHECKPOINT_HZ),
                    "crossing_time_s": checkpoint_crossings,
                    "rule": (
                        "forall f in checkpoint_hz: "
                        "common_useful_start <= t_cross(f) <= "
                        "common_useful_end AND forall fs in "
                        "gate_sample_rates: "
                        "nearest_useful_frame_distance(t_cross, fs) "
                        "<= H/fs_c"
                    ),
                },
                "instantaneous_freq_hz": (
                    "for t in [t_start, t_end]: "
                    "f(t) = f_start * (f_end/f_start)**u with "
                    "u=(t-t_start)/T_active, T_active=t_end-t_start; "
                    "outside active interval: undefined (sample=0)"
                ),
                "phase_integral_rad": (
                    "for t in [t_start, t_end]: "
                    "phi(t) = 2*pi * f_start * T_active / ln(f_end/f_start) * "
                    "((f_end/f_start)**u - 1), u=(t-t_start)/T_active; "
                    "phi(t_start)=0"
                ),
                "sample": (
                    "for t in [t_start, t_end]: A * sin(phi(t)); "
                    "else 0; A=amplitude_peak"
                ),
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "transient_burst": {
                "onset_s": 0.5,
                "peak_amplitude": 0.5,
                "envelope": "one_sided_exponential",
                "tau_s": 0.010,
                "carrier": "none_impulse_like",
                "sample": (
                    "for t < onset: 0; else peak_amplitude * "
                    "exp(-(t-onset)/tau_s)"
                ),
                "repetition_count": 1,
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
                "note": (
                    "excluded from dB spectral SR-parity; onset/peak/"
                    "decay gate per §13.2"
                ),
            },
            "damped_resonance": {
                "center_hz": 1000.0,
                "onset_s": 0.5,
                "peak_amplitude": 0.25,
                "envelope": "exponential_decay",
                "tau_s": 0.050,
                "excitation": "impulse_at_onset",
                "phase_at_onset_rad": 0.0,
                "sample": (
                    "for t < onset: 0; else peak_amplitude * "
                    "exp(-(t-onset)/tau_s) * "
                    "sin(2*pi*center_hz*(t-onset) + phase_at_onset_rad)"
                ),
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
                "note": (
                    "excluded from dB spectral SR-parity; onset/peak/"
                    "decay gate per §13.2"
                ),
            },
            "ultrasonic_96k": {
                "frequencies_hz": list(ULTRASONIC_HZ),
                "presentation": "simultaneous",
                "amplitude_peak_each": peak_ultra,
                "amplitude_formula": (
                    "10**(-24/20) * sqrt(2/N) with N=3"
                ),
                "phases_rad": [0.0, 0.0, 0.0],
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": [96000],
                "waveform": (
                    "sum_k A * sin(2*pi*f_k*t + phi_k) in float64; "
                    "k ascending"
                ),
            },
            "decorrelated_stereo": {
                "layouts": {
                    "mid_only": {
                        "base": "multitone",
                        "left": "s",
                        "right": "s",
                        "ms_convention": "mid=(L+R)/2, side=(L-R)/2",
                        "implied": "mid=s, side=0",
                    },
                    "side_only": {
                        "base": "multitone",
                        "left": "s",
                        "right": "-s",
                        "ms_convention": "mid=(L+R)/2, side=(L-R)/2",
                        "implied": "mid=0, side=s",
                    },
                    "decorrelated": {
                        "base": "pseudo_noise",
                        "method": "independent_pcg64_phase_per_channel",
                        "left_prng_seed": PSEUDO_NOISE_PRNG_SEED,
                        "right_prng_seed": PSEUDO_NOISE_PRNG_SEED + 1,
                        "draw_order": (
                            "render left with left seed (k=0..511), then "
                            "right with right seed (k=0..511); identical "
                            "freqs/amplitudes as mono pseudo_noise"
                        ),
                        "ms_convention": "mid=(L+R)/2, side=(L-R)/2",
                    },
                },
                "channels": 2,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "pseudo_noise": {
                "n_partials": PSEUDO_NOISE_PARTIALS,
                "k_range": [0, PSEUDO_NOISE_PARTIALS - 1],
                "k_order": "ascending",
                "freq_hz_formula": (
                    "20 + (20000-20)*(k+0.5)/512"
                ),
                "f_lo_hz": PSEUDO_NOISE_F_LO_HZ,
                "f_hi_hz": PSEUDO_NOISE_F_HI_HZ,
                "rms_dbfs": PSEUDO_NOISE_RMS_DBFS,
                "amplitude_peak_each": peak_noise,
                "amplitude_formula": (
                    "10**(-24/20) * sqrt(2/512)"
                ),
                "prng": "PCG64",
                "prng_seed": PSEUDO_NOISE_PRNG_SEED,
                "phase_rule": (
                    "rng = Generator(PCG64(31051986)); "
                    "phase[k] = 2*pi*rng.random() for k=0..511 ascending"
                ),
                "phase_unit": "radians",
                "waveform": (
                    "sum_{k=0..511} A * sin(2*pi*freq[k]*t + phase[k]) "
                    "in float64; cast float32; no renormalize"
                ),
                "channels": 1,
                "duration_ref": "global_duration",
                "gate_sample_rates": list(GATE_SAMPLE_RATES),
            },
            "silence_non_finite": {
                "silence": {
                    "value": 0.0,
                    "channels": 1,
                    "duration_ref": "global_duration",
                    "gate_sample_rates": list(GATE_SAMPLE_RATES),
                },
                "non_finite_patterns": [
                    {
                        "id": "nan_at_sample_0",
                        "inject": "NaN",
                        "sample_index": 0,
                        "base": "silence",
                        "channels": 1,
                        "duration_ref": "global_duration",
                    },
                    {
                        "id": "pos_inf_at_sample_0",
                        "inject": "+Inf",
                        "sample_index": 0,
                        "base": "silence",
                        "channels": 1,
                        "duration_ref": "global_duration",
                    },
                    {
                        "id": "neg_inf_at_sample_0",
                        "inject": "-Inf",
                        "sample_index": 0,
                        "base": "silence",
                        "channels": 1,
                        "duration_ref": "global_duration",
                    },
                ],
                "purpose": "fail-closed gate inputs (§13)",
            },
        },
    }


def fixture_spec_bytes() -> bytes:
    """Canonical JSON bytes of the frozen fixture-spec (final LF included)."""
    return canonical_bytes(frozen_fixture_spec())


def fixture_spec_sha256() -> str:
    """SHA-256 of the canonical frozen fixture-spec artifact."""
    return sha256_of_obj(frozen_fixture_spec())


def _require_exact(claim: dict[str, Any], key: str, expected: object) -> None:
    value = claim.get(key)
    if value != expected:
        raise FixtureSpecError(
            f"fixture-spec claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def validate_fixture_spec_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed fixture-spec v1 artifact.

    Rejects mutated duration, weakened numeric semantics, pseudo-noise
    PRNG/phase unpinning, false warm_up+coda+T_min duration formula,
    cross-rate resample allowance, missing categories, wrong metrology
    dependency digest, and any claim not byte-identical to the frozen
    spec.
    """
    if not isinstance(claim, dict):
        raise FixtureSpecError(
            f"fixture-spec claim must be an object, got "
            f"{type(claim).__name__}")

    frozen = frozen_fixture_spec()
    _require_exact(claim, "artifact_id", FIXTURE_SPEC_ARTIFACT_ID)
    _require_exact(claim, "contract_revision", CONTRACT_REVISION)
    _require_exact(claim, "section", FIXTURE_SPEC_SECTION)
    _require_exact(claim, "spec_version", FIXTURE_SPEC_VERSION)

    if claim.get("no_audio_generators_in_this_artifact") is not True:
        raise FixtureSpecError(
            "no_audio_generators_in_this_artifact must be true "
            "(M2 freezes params only; generators are M3/T6)")

    env = claim.get("envelope_authority")
    if not isinstance(env, str) or "freeze-from-prose" not in env:
        raise FixtureSpecError(
            "envelope_authority must declare freeze-from-prose packaging")
    if "NO REV7" not in env and "no REV7" not in env.lower():
        raise FixtureSpecError(
            "envelope_authority must state NO REV7 (packaging only)")

    deps = claim.get("dependencies")
    if not isinstance(deps, dict):
        raise FixtureSpecError("dependencies must be an object")
    if deps.get("contract_revision") != CONTRACT_REVISION:
        raise FixtureSpecError(
            "dependencies.contract_revision must equal CONTRACT_REVISION")
    expected_lock = metrology_lock_sha256()
    if deps.get("metrology_lock_sha256") != expected_lock:
        raise FixtureSpecError(
            "dependencies.metrology_lock_sha256 must equal current "
            f"metrology_lock_sha256() ({expected_lock})")
    if deps.get("gate_sample_rates") != list(GATE_SAMPLE_RATES):
        raise FixtureSpecError(
            f"dependencies.gate_sample_rates must be {list(GATE_SAMPLE_RATES)}")

    numeric = claim.get("numeric_generation")
    if not isinstance(numeric, dict):
        raise FixtureSpecError("numeric_generation must be an object")
    if numeric.get("compute_dtype") != "float64":
        raise FixtureSpecError("compute_dtype must be float64")
    if numeric.get("artifact_dtype") != "float32":
        raise FixtureSpecError("artifact_dtype must be float32")
    if numeric.get("post_render_normalization") != "forbidden":
        raise FixtureSpecError(
            "post_render_normalization must be 'forbidden'")
    if numeric.get("cross_rate_resample") != "forbidden":
        raise FixtureSpecError("cross_rate_resample must be 'forbidden'")
    if "DIRECTLY" not in str(numeric.get("render_policy", "")):
        raise FixtureSpecError(
            "render_policy must require DIRECT generation at gate rates")
    fmt = numeric.get("file_format")
    if not isinstance(fmt, dict):
        raise FixtureSpecError("numeric_generation.file_format must be an object")
    if fmt.get("container") != "wav" or fmt.get("encoding") != "pcm_float32_le":
        raise FixtureSpecError(
            "file_format must be wav / pcm_float32_le")

    duration = claim.get("global_duration")
    if not isinstance(duration, dict):
        raise FixtureSpecError("global_duration must be an object")
    if duration.get("kind") != "preregistered_parameter":
        raise FixtureSpecError(
            "global_duration.kind must be 'preregistered_parameter'")
    if duration.get("not_a_contract_formula") is not True:
        raise FixtureSpecError(
            "global_duration.not_a_contract_formula must be true")
    if duration.get("false_formula_forbidden") != "warm_up + coda + T_min":
        raise FixtureSpecError(
            "false_formula_forbidden must record warm_up + coda + T_min")
    if duration.get("duration_num") != DURATION_NUM:
        raise FixtureSpecError(
            f"duration_num must be {DURATION_NUM}")
    if duration.get("duration_den") != DURATION_DEN:
        raise FixtureSpecError(
            f"duration_den must be {DURATION_DEN}")
    if duration.get("duration_s") != duration_seconds():
        raise FixtureSpecError(
            f"duration_s must be {duration_seconds()}")
    bound = duration.get("bound_check")
    if not isinstance(bound, dict):
        raise FixtureSpecError("global_duration.bound_check must be an object")
    if bound.get("max_warm_up_plus_coda_num") != _MAX_WARM_UP_PLUS_CODA_NUM:
        raise FixtureSpecError(
            "bound_check must use lock rational max wu+coda 18896/55125")
    if bound.get("max_warm_up_plus_coda_den") != _MAX_WARM_UP_PLUS_CODA_DEN:
        raise FixtureSpecError(
            "bound_check max_warm_up_plus_coda_den must be 55125")
    if duration_seconds() <= (
            _MAX_WARM_UP_PLUS_CODA_NUM / _MAX_WARM_UP_PLUS_CODA_DEN):
        raise FixtureSpecError(
            "duration_s must exceed max(warm_up_sr + coda_sr)")

    cats = claim.get("categories")
    if not isinstance(cats, dict):
        raise FixtureSpecError("categories must be an object")
    required = {
        "multitone",
        "log_sweep",
        "transient_burst",
        "damped_resonance",
        "ultrasonic_96k",
        "decorrelated_stereo",
        "pseudo_noise",
        "silence_non_finite",
    }
    missing = required - set(cats)
    if missing:
        raise FixtureSpecError(f"categories missing required keys: {sorted(missing)}")

    multi = cats.get("multitone")
    if not isinstance(multi, dict):
        raise FixtureSpecError("categories.multitone must be an object")
    if multi.get("frequencies_hz") != list(MULTITONE_HZ):
        raise FixtureSpecError(
            "multitone.frequencies_hz must match §13 critical centres")

    sweep = cats.get("log_sweep")
    if not isinstance(sweep, dict):
        raise FixtureSpecError("categories.log_sweep must be an object")
    if sweep.get("sweep_law") != "exponential_log_chirp":
        raise FixtureSpecError(
            "log_sweep.sweep_law must be 'exponential_log_chirp'")
    if "ln(f_end/f_start)" not in str(sweep.get("phase_integral_rad", "")):
        raise FixtureSpecError(
            "log_sweep.phase_integral_rad must pin the log-chirp integral")
    if "T_active" not in str(sweep.get("instantaneous_freq_hz", "")):
        raise FixtureSpecError(
            "log_sweep.instantaneous_freq_hz must pin active-interval "
            "T_active (not full-asset duration)")
    active = sweep.get("active_interval")
    if not isinstance(active, dict):
        raise FixtureSpecError("log_sweep.active_interval must be an object")
    if active.get("t_start_num") != SWEEP_T_START_NUM:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_start_num must be {SWEEP_T_START_NUM}")
    if active.get("t_start_den") != SWEEP_T_START_DEN:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_start_den must be {SWEEP_T_START_DEN}")
    if active.get("t_end_num") != SWEEP_T_END_NUM:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_end_num must be {SWEEP_T_END_NUM}")
    if active.get("t_end_den") != SWEEP_T_END_DEN:
        raise FixtureSpecError(
            f"log_sweep.active_interval.t_end_den must be {SWEEP_T_END_DEN}")
    if active.get("t_start_s") != sweep_active_start_seconds():
        raise FixtureSpecError(
            "log_sweep.active_interval.t_start_s must equal "
            f"{sweep_active_start_seconds()}")
    if active.get("t_end_s") != sweep_active_end_seconds():
        raise FixtureSpecError(
            "log_sweep.active_interval.t_end_s must equal "
            f"{sweep_active_end_seconds()}")
    if active.get("outside_active_sample") != 0.0:
        raise FixtureSpecError(
            "log_sweep.active_interval.outside_active_sample must be 0.0")
    reach = sweep.get("checkpoint_reachability")
    if not isinstance(reach, dict):
        raise FixtureSpecError(
            "log_sweep.checkpoint_reachability must be an object")
    if reach.get("checkpoint_hz") != list(SWEEP_CHECKPOINT_HZ):
        raise FixtureSpecError(
            "checkpoint_reachability.checkpoint_hz must equal "
            "metrology lock SWEEP_CHECKPOINT_HZ")
    if "nearest_useful_frame_distance" not in str(reach.get("rule", "")):
        raise FixtureSpecError(
            "checkpoint_reachability.rule must require "
            "nearest_useful_frame_distance <= H/fs_c")
    # Structural self-check against current lock timing (not claim-trusted).
    assert_sweep_checkpoints_reachable()

    noise = cats.get("pseudo_noise")
    if not isinstance(noise, dict):
        raise FixtureSpecError("categories.pseudo_noise must be an object")
    if noise.get("prng_seed") != PSEUDO_NOISE_PRNG_SEED:
        raise FixtureSpecError(
            f"pseudo_noise.prng_seed must be {PSEUDO_NOISE_PRNG_SEED}")
    if noise.get("n_partials") != PSEUDO_NOISE_PARTIALS:
        raise FixtureSpecError("pseudo_noise.n_partials must be 512")
    phase_rule = str(noise.get("phase_rule", ""))
    if "PCG64(31051986)" not in phase_rule:
        raise FixtureSpecError(
            "pseudo_noise.phase_rule must pin Generator(PCG64(31051986))")
    if "k=0..511" not in phase_rule:
        raise FixtureSpecError(
            "pseudo_noise.phase_rule must pin k=0..511 ascending draws")
    if "2*pi*rng.random()" not in phase_rule:
        raise FixtureSpecError(
            "pseudo_noise.phase_rule must pin phase[k]=2*pi*rng.random()")

    ultra = cats.get("ultrasonic_96k")
    if not isinstance(ultra, dict):
        raise FixtureSpecError("categories.ultrasonic_96k must be an object")
    if ultra.get("frequencies_hz") != list(ULTRASONIC_HZ):
        raise FixtureSpecError(
            "ultrasonic_96k.frequencies_hz must be [28000,32000,40000]")
    if ultra.get("presentation") != "simultaneous":
        raise FixtureSpecError(
            "ultrasonic_96k.presentation must be 'simultaneous'")
    if ultra.get("gate_sample_rates") != [96000]:
        raise FixtureSpecError(
            "ultrasonic_96k.gate_sample_rates must be [96000] only")

    stereo = cats.get("decorrelated_stereo")
    if not isinstance(stereo, dict):
        raise FixtureSpecError("categories.decorrelated_stereo must be an object")
    layouts = stereo.get("layouts")
    if not isinstance(layouts, dict):
        raise FixtureSpecError("decorrelated_stereo.layouts must be an object")
    for key in ("mid_only", "side_only", "decorrelated"):
        if key not in layouts:
            raise FixtureSpecError(
                f"decorrelated_stereo.layouts missing {key!r}")
    deco = layouts.get("decorrelated")
    if not isinstance(deco, dict):
        raise FixtureSpecError("layouts.decorrelated must be an object")
    if deco.get("method") != "independent_pcg64_phase_per_channel":
        raise FixtureSpecError(
            "decorrelated method must be independent_pcg64_phase_per_channel")
    if deco.get("left_prng_seed") != PSEUDO_NOISE_PRNG_SEED:
        raise FixtureSpecError("decorrelated left_prng_seed must be 31051986")
    if deco.get("right_prng_seed") != PSEUDO_NOISE_PRNG_SEED + 1:
        raise FixtureSpecError("decorrelated right_prng_seed must be 31051987")

    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise FixtureSpecError(
            f"claim is not canonically serializable: {exc}") from exc

    frozen_digest = sha256_of_obj(frozen)
    if claim_digest != frozen_digest:
        raise FixtureSpecError(
            "fixture-spec claim is not byte-identical to the frozen §13 "
            f"spec (claim_sha256={claim_digest}, "
            f"frozen_sha256={frozen_digest})")

    return deepcopy(claim)
