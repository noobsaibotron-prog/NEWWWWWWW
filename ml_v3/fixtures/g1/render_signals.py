"""G1a T6 signal generators — freeze-from fixture-spec v1 only.

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a; fixture_spec_v1.json
digest 513c3baf… (SHA256SUMS trust chain).

Rules (numeric_generation):
  - render DIRECTLY at each gate rate (44100 / 48000 / 96000)
  - compute float64 → cast float32 (round-to-nearest-even); no dither
  - no normalize, no DC removal, no cross-rate resample
  - WAV container: pcm_float32_le, interleaved LRLR for stereo
  - log_sweep active only on [0.5, 1.75] s; outside → 0
"""
from __future__ import annotations

import math
import struct
from pathlib import Path
from typing import Callable, Iterable

import numpy as np

from ml_v3.contracts.canonical import sha256_of_file, sha256sums_text
from ml_v3.contracts.constants import GATE_SAMPLE_RATES
from ml_v3.contracts.fixture_spec import (
    PSEUDO_NOISE_PRNG_SEED,
    SWEEP_F_END_HZ,
    SWEEP_F_START_HZ,
    frozen_fixture_spec,
    sample_count,
    sweep_active_end_seconds,
    sweep_active_start_seconds,
)
from ml_v3.contracts.sha256sums import (
    G1A_SHA256SUMS_RELPATH,
    load_g1a_sha256sums,
    repo_root_from_here,
    verify_g1a_sha256sums,
)

__all__ = [
    "FixtureRenderError",
    "AUDIO_REL_PREFIX",
    "render_asset",
    "iter_asset_specs",
    "write_wav_pcm_float32_le",
    "render_all_to_tree",
    "asset_relpath",
    "update_sha256sums_with_audio",
]

AUDIO_REL_PREFIX = "ml_v3/fixtures/g1/audio"

# wave module: WAVE_FORMAT_IEEE_FLOAT
_WAVE_FORMAT_IEEE_FLOAT = 3


class FixtureRenderError(ValueError):
    """Raised when a fixture render request violates the frozen spec."""


def _require_gate_rate(fs: int) -> None:
    if fs not in GATE_SAMPLE_RATES:
        raise FixtureRenderError(
            f"fs {fs} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")


def _time_axis(fs: int) -> np.ndarray:
    """Return float64 ``t = n / fs`` for ``n = 0 .. N-1``."""
    _require_gate_rate(fs)
    n = sample_count(fs)
    # Exact integer N from fixture_spec; float64 division per time_rule.
    return np.arange(n, dtype=np.float64) / float(fs)


def _cast_f32(x: np.ndarray) -> np.ndarray:
    """Cast float64 render to IEEE754 binary32 (numpy default RTE)."""
    if x.dtype != np.float64:
        raise FixtureRenderError(
            f"compute dtype must be float64 before cast, got {x.dtype}")
    return x.astype(np.float32, copy=False)


def _multitone_f64(
    t: np.ndarray,
    frequencies_hz: Iterable[float],
    amplitude: float,
    phases_rad: Iterable[float],
) -> np.ndarray:
    out = np.zeros(t.shape[0], dtype=np.float64)
    for freq, phase in zip(frequencies_hz, phases_rad, strict=True):
        out += amplitude * np.sin(2.0 * math.pi * float(freq) * t + float(phase))
    return out


def render_multitone(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["multitone"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"multitone not defined at fs={fs}")
    t = _time_axis(fs)
    y = _multitone_f64(
        t,
        spec["frequencies_hz"],
        float(spec["amplitude_peak_each"]),
        spec["phases_rad"],
    )
    return _cast_f32(y)


def render_log_sweep(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["log_sweep"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"log_sweep not defined at fs={fs}")
    t = _time_axis(fs)
    t0 = sweep_active_start_seconds()
    t1 = sweep_active_end_seconds()
    t_active = t1 - t0
    if t_active <= 0.0:
        raise FixtureRenderError("log_sweep active interval empty")
    amp = float(spec["amplitude_peak"])
    ratio = SWEEP_F_END_HZ / SWEEP_F_START_HZ
    ln_ratio = math.log(ratio)
    # Active mask: t in [t_start, t_end] (inclusive endpoints).
    active = (t >= t0) & (t <= t1)
    u = np.zeros_like(t)
    u[active] = (t[active] - t0) / t_active
    # phi(t) = 2π f_start T_active / ln(ratio) * (ratio^u - 1)
    phi = np.zeros_like(t)
    phi[active] = (
        2.0 * math.pi * SWEEP_F_START_HZ * t_active / ln_ratio
        * (np.power(ratio, u[active]) - 1.0)
    )
    y = np.zeros_like(t)
    y[active] = amp * np.sin(phi[active])
    return _cast_f32(y)


def render_transient_burst(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["transient_burst"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"transient_burst not defined at fs={fs}")
    t = _time_axis(fs)
    onset = float(spec["onset_s"])
    peak = float(spec["peak_amplitude"])
    tau = float(spec["tau_s"])
    y = np.zeros_like(t)
    active = t >= onset
    y[active] = peak * np.exp(-(t[active] - onset) / tau)
    return _cast_f32(y)


def render_damped_resonance(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["damped_resonance"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"damped_resonance not defined at fs={fs}")
    t = _time_axis(fs)
    onset = float(spec["onset_s"])
    peak = float(spec["peak_amplitude"])
    tau = float(spec["tau_s"])
    f0 = float(spec["center_hz"])
    phase0 = float(spec["phase_at_onset_rad"])
    y = np.zeros_like(t)
    active = t >= onset
    dt = t[active] - onset
    y[active] = (
        peak * np.exp(-dt / tau)
        * np.sin(2.0 * math.pi * f0 * dt + phase0)
    )
    return _cast_f32(y)


def render_ultrasonic_96k(fs: int) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["ultrasonic_96k"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"ultrasonic_96k not defined at fs={fs}")
    t = _time_axis(fs)
    y = _multitone_f64(
        t,
        spec["frequencies_hz"],
        float(spec["amplitude_peak_each"]),
        spec["phases_rad"],
    )
    return _cast_f32(y)


def _pseudo_noise_phases(seed: int, n_partials: int) -> np.ndarray:
    rng = np.random.Generator(np.random.PCG64(int(seed)))
    phases = np.empty(n_partials, dtype=np.float64)
    for k in range(n_partials):
        phases[k] = 2.0 * math.pi * float(rng.random())
    return phases


def render_pseudo_noise(fs: int, *, seed: int | None = None) -> np.ndarray:
    spec = frozen_fixture_spec()["categories"]["pseudo_noise"]
    if fs not in spec["gate_sample_rates"]:
        raise FixtureRenderError(f"pseudo_noise not defined at fs={fs}")
    t = _time_axis(fs)
    n = int(spec["n_partials"])
    f_lo = float(spec["f_lo_hz"])
    f_hi = float(spec["f_hi_hz"])
    amp = float(spec["amplitude_peak_each"])
    use_seed = PSEUDO_NOISE_PRNG_SEED if seed is None else int(seed)
    phases = _pseudo_noise_phases(use_seed, n)
    y = np.zeros(t.shape[0], dtype=np.float64)
    for k in range(n):
        freq = f_lo + (f_hi - f_lo) * (k + 0.5) / n
        y += amp * np.sin(2.0 * math.pi * freq * t + phases[k])
    return _cast_f32(y)


def render_silence(fs: int) -> np.ndarray:
    silence = frozen_fixture_spec()["categories"]["silence_non_finite"]["silence"]
    if fs not in silence["gate_sample_rates"]:
        raise FixtureRenderError(f"silence not defined at fs={fs}")
    n = sample_count(fs)
    return np.zeros(n, dtype=np.float32)


def render_non_finite(fs: int, pattern_id: str) -> np.ndarray:
    patterns = frozen_fixture_spec()["categories"]["silence_non_finite"][
        "non_finite_patterns"
    ]
    match = next((p for p in patterns if p["id"] == pattern_id), None)
    if match is None:
        raise FixtureRenderError(f"unknown non-finite pattern {pattern_id!r}")
    y = render_silence(fs)
    idx = int(match["sample_index"])
    if idx < 0 or idx >= y.shape[0]:
        raise FixtureRenderError(
            f"non-finite sample_index {idx} out of range for fs={fs}")
    inject = match["inject"]
    if inject == "NaN":
        y[idx] = np.float32(np.nan)
    elif inject == "+Inf":
        y[idx] = np.float32(np.inf)
    elif inject == "-Inf":
        y[idx] = np.float32(-np.inf)
    else:
        raise FixtureRenderError(f"unknown inject token {inject!r}")
    return y


def render_stereo_mid_only(fs: int) -> np.ndarray:
    s = render_multitone(fs).astype(np.float64, copy=False)
    # L=R=s → mid=s, side=0
    return _cast_f32(np.stack([s, s], axis=1))


def render_stereo_side_only(fs: int) -> np.ndarray:
    s = render_multitone(fs).astype(np.float64, copy=False)
    # L=s, R=-s → mid=0, side=s
    return _cast_f32(np.stack([s, -s], axis=1))


def render_stereo_decorrelated(fs: int) -> np.ndarray:
    layout = frozen_fixture_spec()["categories"]["decorrelated_stereo"][
        "layouts"
    ]["decorrelated"]
    left = render_pseudo_noise(
        fs, seed=int(layout["left_prng_seed"])
    ).astype(np.float64, copy=False)
    right = render_pseudo_noise(
        fs, seed=int(layout["right_prng_seed"])
    ).astype(np.float64, copy=False)
    return _cast_f32(np.stack([left, right], axis=1))


RenderFn = Callable[[int], np.ndarray]


def asset_relpath(category: str, stem: str, fs: int) -> str:
    """Repo-root-relative POSIX path for a rendered WAV asset."""
    return f"{AUDIO_REL_PREFIX}/{category}/{stem}_{fs}.wav"


def iter_asset_specs() -> list[tuple[str, str, int, RenderFn]]:
    """Return (category, stem, fs, render_fn) for every fixture-spec asset."""
    specs: list[tuple[str, str, int, RenderFn]] = []
    cats = frozen_fixture_spec()["categories"]

    def add(category: str, stem: str, rates: Iterable[int], fn: RenderFn) -> None:
        for fs in rates:
            specs.append((category, stem, int(fs), fn))

    add("multitone", "multitone", cats["multitone"]["gate_sample_rates"],
        render_multitone)
    add("log_sweep", "log_sweep", cats["log_sweep"]["gate_sample_rates"],
        render_log_sweep)
    add("transient_burst", "transient_burst",
        cats["transient_burst"]["gate_sample_rates"], render_transient_burst)
    add("damped_resonance", "damped_resonance",
        cats["damped_resonance"]["gate_sample_rates"], render_damped_resonance)
    add("ultrasonic_96k", "ultrasonic_96k",
        cats["ultrasonic_96k"]["gate_sample_rates"], render_ultrasonic_96k)
    add("pseudo_noise", "pseudo_noise",
        cats["pseudo_noise"]["gate_sample_rates"], render_pseudo_noise)
    add(
        "decorrelated_stereo", "mid_only",
        cats["decorrelated_stereo"]["gate_sample_rates"],
        render_stereo_mid_only,
    )
    add(
        "decorrelated_stereo", "side_only",
        cats["decorrelated_stereo"]["gate_sample_rates"],
        render_stereo_side_only,
    )
    add(
        "decorrelated_stereo", "decorrelated",
        cats["decorrelated_stereo"]["gate_sample_rates"],
        render_stereo_decorrelated,
    )
    silence_rates = cats["silence_non_finite"]["silence"]["gate_sample_rates"]
    add("silence_non_finite", "silence", silence_rates, render_silence)
    # Non-finite patterns inherit silence gate rates (fail-closed inputs).
    for pattern in cats["silence_non_finite"]["non_finite_patterns"]:
        pid = str(pattern["id"])

        def _make(pattern_id: str) -> RenderFn:
            return lambda fs, _pid=pattern_id: render_non_finite(fs, _pid)

        add("silence_non_finite", pid, silence_rates, _make(pid))

    # Deterministic order by relpath.
    specs.sort(key=lambda item: asset_relpath(item[0], item[1], item[2]))
    return specs


def render_asset(category: str, stem: str, fs: int) -> np.ndarray:
    for cat, st, rate, fn in iter_asset_specs():
        if cat == category and st == stem and rate == fs:
            return fn(fs)
    raise FixtureRenderError(
        f"unknown asset {category}/{stem} at fs={fs}")


def write_wav_pcm_float32_le(
    path: Path,
    samples: np.ndarray,
    sample_rate: int,
) -> None:
    """Write WAV PCM float32 LE (format tag 3), mono or interleaved stereo."""
    path = Path(path)
    if samples.dtype != np.float32:
        raise FixtureRenderError(
            f"WAV payload must be float32, got {samples.dtype}")
    if samples.ndim == 1:
        channels = 1
        pcm = np.ascontiguousarray(samples)
    elif samples.ndim == 2 and samples.shape[1] in (1, 2):
        channels = int(samples.shape[1])
        # C-order (N, C) flatten → interleaved LRLR for stereo.
        pcm = np.ascontiguousarray(samples).reshape(-1)
    else:
        raise FixtureRenderError(
            f"samples shape must be (N,) or (N,1|2), got {samples.shape}")

    n_frames = int(samples.shape[0])
    if n_frames != sample_count(int(sample_rate)):
        raise FixtureRenderError(
            f"frame count {n_frames} != fixture N={sample_count(int(sample_rate))}"
        )
    block_align = channels * 4
    byte_rate = int(sample_rate) * block_align
    data_bytes = pcm.astype("<f4", copy=False).tobytes()
    data_size = len(data_bytes)
    fmt_chunk = struct.pack(
        "<HHIIHH",
        _WAVE_FORMAT_IEEE_FLOAT,
        channels,
        int(sample_rate),
        byte_rate,
        block_align,
        32,
    )
    # 16-byte fmt payload (no extension for IEEE float basic header).
    riff_size = 4 + (8 + 16) + (8 + data_size)
    header = b"".join((
        b"RIFF",
        struct.pack("<I", riff_size),
        b"WAVE",
        b"fmt ",
        struct.pack("<I", 16),
        fmt_chunk,
        b"data",
        struct.pack("<I", data_size),
    ))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + data_bytes)


def render_all_to_tree(root: Path | None = None) -> list[str]:
    """Render every asset under ``ml_v3/fixtures/g1/audio/``; return relpaths."""
    root = repo_root_from_here() if root is None else Path(root)
    written: list[str] = []
    for category, stem, fs, fn in iter_asset_specs():
        rel = asset_relpath(category, stem, fs)
        absolute = root / rel
        samples = fn(fs)
        write_wav_pcm_float32_le(absolute, samples, fs)
        written.append(rel)
    return written


def update_sha256sums_with_audio(root: Path | None = None) -> dict[str, str]:
    """Merge audio digests into SHA256SUMS in canonical path order."""
    root = repo_root_from_here() if root is None else Path(root)
    entries = dict(load_g1a_sha256sums(root))
    for category, stem, fs, _fn in iter_asset_specs():
        rel = asset_relpath(category, stem, fs)
        absolute = root / rel
        if not absolute.is_file():
            raise FixtureRenderError(f"missing audio asset for hash: {rel}")
        entries[rel] = sha256_of_file(absolute)
    text = sha256sums_text(entries)
    out = root / G1A_SHA256SUMS_RELPATH
    out.write_text(text, encoding="utf-8", newline="\n")
    return verify_g1a_sha256sums(root)


def main() -> None:
    written = render_all_to_tree()
    entries = update_sha256sums_with_audio()
    print(f"rendered {len(written)} WAV assets")
    print(f"SHA256SUMS entries: {len(entries)}")


if __name__ == "__main__":
    main()
