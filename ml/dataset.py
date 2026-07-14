"""Dataset builders: synthetic (port of MLEngine::generateSyntheticDataset)
plus the real-audio VocalSet injection factory (port of the approach in
Source/Tests/RealDataSibilanceTest.cpp, generalized to all classes).

The synthetic generator reproduces the C++ shapes and distributions but does
NOT promise RNG-stream equality with std::mt19937 (numpy Generator differs).
Determinism is guaranteed within Python via fixed seeds. Distribution parity
(tilt band, base level, sigma, prominence model) is what matters for training.

Real-audio pipeline:
  wav -> mono -> AnalysisPipeline (SpectrumAnalyzer mirror) -> smoothed dB
  frame -> select top-energy frames -> inject/attenuate a problem into the dB
  frame (paired positive/negative) -> linear -> features.

Singer-held-out split follows the in-repo convention:
  test singers: female9, female8, male11, male10 (never trained on).
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from .features import (AnalysisPipeline, db_frame_to_linear, extract_mel_bands,
                       features_v1, features_v2, features_v3, features_v4,
                       hz_to_mel,
                       mel_to_hz, MEL_NUM_BANDS)
from .model import NUM_PROBLEMS, PROBLEM_NAMES, problem_names

# ---- synthetic constants ----
# C++ generateSyntheticDataset ships a NARROW tilt band (-4.5..-1.5 dB/decade).
# Measured consequence: pink noise (~-10 dB/decade) is OUT of distribution and
# the v2 shape features fire Boominess/Muddiness on clean pink (corpus probe,
# 2026-07-03). The offline default therefore WIDENS the band to cover real
# program tilts including pink; the C++ generator is left untouched.
TILT_MIN_DB_PER_DECADE = -12.0
TILT_MAX_DB_PER_DECADE = -0.5
TILT_REFERENCE_HZ = 100.0

PROBLEM_FREQ_RANGES = [
    (100.0, 5000.0),    # Resonance
    (2000.0, 8000.0),   # Harshness
    (100.0, 400.0),     # Muddiness
    (5000.0, 12000.0),  # Sibilance
    (40.0, 150.0),      # Boominess
    (80.0, 300.0),      # Thinness (lack of low-mids -> subtractive)
    (300.0, 800.0),     # BoxyMidrange
    (20.0, 20000.0),    # Clipping
]

TEST_SINGERS = ("female9", "female8", "male11", "male10")

# product-v2 swaps slot 7: Clipping -> DullSound (loss of highs). The freq
# range tracks the product's DullSound zone (AIEngine expects 8-16 kHz,
# widened down to 6 kHz where audible dullness onset lives).
DULLSOUND_FREQ_RANGE = (6000.0, 16000.0)


def problem_freq_ranges(schema: str = "legacy-v1") -> list[tuple[float, float]]:
    ranges = list(PROBLEM_FREQ_RANGES)
    if schema == "product-v2":
        ranges[7] = DULLSOUND_FREQ_RANGE
    return ranges

FEATURE_FNS = {1: features_v1, 2: features_v2, 3: features_v3,
               4: features_v4}


@dataclass
class Sample:
    features: np.ndarray                       # [64]
    problem_targets: np.ndarray                # [8] in {0,1}
    freq_targets: np.ndarray                   # [8] normalized log-freq
    source: str = "synthetic"                  # provenance tag


# ------------------------------------------------------------------ synthetic
def _apply_tilt(spectrum: np.ndarray, sample_rate: float, fft_size: int,
                slope_db_per_decade: float) -> None:
    bins = spectrum.shape[0]
    bin_hz = sample_rate / fft_size
    freqs = np.maximum(20.0, np.arange(bins) * bin_hz)
    tilt_gain = 10.0 ** ((slope_db_per_decade
                          * np.log10(freqs / TILT_REFERENCE_HZ)) / 20.0)
    np.multiply(np.maximum(spectrum, 0.0), tilt_gain, out=spectrum)


def _synthetic_spectrum(rng: np.random.Generator, problem: int,
                        sample_rate: float, fft_size: int, target_freq: float,
                        strength: float, relative_prominence: bool,
                        schema: str = "legacy-v1") -> np.ndarray:
    bins = fft_size // 2
    spectrum = np.maximum(0.0, 0.05 + rng.normal(0.0, 0.02, bins))
    _apply_tilt(spectrum, sample_rate, fft_size,
                rng.uniform(TILT_MIN_DB_PER_DECADE, TILT_MAX_DB_PER_DECADE))

    bin_hz = sample_rate / fft_size
    freqs = np.arange(bins) * bin_hz

    if schema == "product-v2" and problem == 7:
        # DullSound: loss of highs — raised-cosine high-shelf CUT that starts
        # at target_freq and stays down to Nyquist (a band peak would be wrong:
        # dullness is a broadband HF deficit, not a local notch).
        cut_db = 8.0 + (np.clip(strength, 0.6, 1.0) - 0.6) / 0.4 * 8.0  # 8..16
        log_f = np.log2(np.maximum(freqs, 1.0))
        win = np.clip((log_f - (np.log2(target_freq) - 0.5)) / 1.0, 0.0, 1.0)
        win = 0.5 - 0.5 * np.cos(np.pi * win)  # 1-octave raised-cosine onset
        return spectrum * 10.0 ** (-cut_db * win / 20.0)

    sigma_hz = max(30.0, target_freq * 0.08)
    peak = np.exp(-0.5 * ((freqs - target_freq) / sigma_hz) ** 2)
    subtractive = (problem == 5)  # Thinness

    if relative_prominence:
        prom_db = 14.0 + (np.clip(strength, 0.6, 1.0) - 0.6) / 0.4 * 8.0
        gain = 10.0 ** ((-prom_db if subtractive else prom_db) / 20.0)
        spectrum = np.maximum(0.0, spectrum * (1.0 + (gain - 1.0) * peak))
    else:
        sign = -1.0 if subtractive else 1.0
        spectrum = np.maximum(0.0, spectrum + sign * strength * peak)

    if problem == 7 and schema == "legacy-v1":  # Clipping
        spectrum = np.clip(spectrum + 0.2, 0.0, 1.2)
    return spectrum


def synthetic_dataset(samples_per_problem: int, feature_version: int,
                      sample_rate: float = 44100.0, fft_size: int = 2048,
                      relative_prominence: bool = True,
                      extra_resonance_positives: int = 0,
                      negative_multiplier: float = 1.5,
                      seed: int = 424242,
                      schema: str = "legacy-v1") -> list[Sample]:
    """Mirrors generateSyntheticDataset: 8N positives + negatives.
    negative_multiplier scales the negative slots relative to the C++ default
    (clean = N * mult, hard = N/2 * mult); 1.0 == C++ composition. The v2
    training default raises it to push the raw clean-FP floor down without
    relying on the plugin's veto stack."""
    feat = FEATURE_FNS[feature_version]
    rng = np.random.default_rng(seed)
    ranges = problem_freq_ranges(schema)
    out: list[Sample] = []

    def add_positive(problem: int):
        lo, hi = ranges[problem]
        f_norm = rng.uniform()
        target = lo * (hi / lo) ** f_norm
        strength = 0.6 + rng.uniform() * 0.4
        spec = _synthetic_spectrum(rng, problem, sample_rate, fft_size,
                                   target, strength, relative_prominence,
                                   schema)
        pt = np.zeros(NUM_PROBLEMS)
        pt[problem] = 1.0
        ft = np.zeros(NUM_PROBLEMS)
        ft[problem] = np.clip(np.log(target / lo) / np.log(hi / lo), 0.0, 1.0)
        out.append(Sample(feat(spec, sample_rate), pt, ft, "synthetic"))

    for p in range(NUM_PROBLEMS):
        for _ in range(samples_per_problem):
            add_positive(p)
    for _ in range(extra_resonance_positives):
        add_positive(0)

    clean = int(samples_per_problem * negative_multiplier)
    hard = int((samples_per_problem // 2) * negative_multiplier)
    for i in range(clean + hard):
        bins = fft_size // 2
        spec = np.maximum(0.0, rng.uniform(0.03, 0.08) + rng.normal(0.0, 0.02, bins))
        _apply_tilt(spec, sample_rate, fft_size,
                    rng.uniform(TILT_MIN_DB_PER_DECADE, TILT_MAX_DB_PER_DECADE))
        if i >= clean + hard // 2:  # weak-bump-on-tilt negatives
            bin_hz = sample_rate / fft_size
            freqs = np.arange(bins) * bin_hz
            target = rng.uniform(100.0, 10000.0)
            sigma = max(30.0, target * 0.08)
            spec += rng.uniform(0.1, 0.3) * np.exp(-0.5 * ((freqs - target) / sigma) ** 2)
        out.append(Sample(feat(spec, sample_rate), np.zeros(NUM_PROBLEMS),
                          np.zeros(NUM_PROBLEMS), "synthetic-negative"))
    return out


# ------------------------------------------------------------------ real audio
def _band_energy_db(frame_db: np.ndarray, sample_rate: float,
                    lo_hz: float, hi_hz: float) -> float:
    n_bins = frame_db.shape[0]
    fft_size = (n_bins - 1) * 2
    bin_hz = sample_rate / fft_size
    lo = max(0, int(lo_hz / bin_hz))
    hi = min(n_bins - 1, int(hi_hz / bin_hz))
    if hi <= lo:
        return -120.0
    return float(np.mean(frame_db[lo:hi + 1]))


def _scale_band_db(frame_db: np.ndarray, sample_rate: float, lo_hz: float,
                   hi_hz: float, delta_db: float,
                   edge_octaves: float = 0.25) -> np.ndarray:
    """Add delta_db to [lo_hz, hi_hz] with raised-cosine octave-domain edges —
    generalization of RealDataSibilanceTest's scaleSibBand injection."""
    n_bins = frame_db.shape[0]
    fft_size = (n_bins - 1) * 2
    bin_hz = sample_rate / fft_size
    freqs = np.maximum(1.0, np.arange(n_bins) * bin_hz)
    log_f = np.log2(freqs)
    lo_l, hi_l = np.log2(lo_hz), np.log2(hi_hz)
    ramp_in = np.clip((log_f - (lo_l - edge_octaves)) / edge_octaves, 0.0, 1.0)
    ramp_out = np.clip(((hi_l + edge_octaves) - log_f) / edge_octaves, 0.0, 1.0)
    win = np.minimum(ramp_in, ramp_out)
    win = 0.5 - 0.5 * np.cos(np.pi * win)  # raised cosine 0..1
    return frame_db + delta_db * win


@dataclass
class InjectionSpec:
    """How to fabricate a labeled positive for one problem class on a real frame."""
    problem: int
    lo_hz: float
    hi_hz: float
    boost_db_range: tuple[float, float]         # positive injection
    subtractive: bool = False                    # Thinness: cut the band instead

    def target_freq(self, rng: np.random.Generator) -> float:
        return float(self.lo_hz * (self.hi_hz / self.lo_hz) ** rng.uniform())


RESONANCE_PRE_PROM_MAX_DB = 6.0
RESONANCE_DELTA_PROM_MIN_DB = 6.0
RESONANCE_POST_PROM_MIN_DB = 10.0
RESONANCE_MAX_TARGET_ATTEMPTS = 160
RESONANCE_TARGET_ATTEMPTS_PER_BIN = 32
RESONANCE_SIGMA_MULTS = (0.5, 1.0, 2.0)
RESONANCE_FREQ_BINS = (
    (150.0, 300.0),
    (300.0, 700.0),
    (700.0, 1100.0),
    (1100.0, 2500.0),
    (2500.0, 5000.0),
)


@dataclass
class ResonanceInjection:
    frame_db: np.ndarray
    target_freq: float
    boost_db: float
    sigma_mult: float
    pre_prom_db: float
    post_prom_db: float
    band_index: int

    @property
    def delta_prom_db(self) -> float:
        return self.post_prom_db - self.pre_prom_db


# Class-specific injection bands (aligned with PROBLEM_FREQ_RANGES, narrowed
# to musically plausible zones on vocal material).
INJECTIONS = [
    InjectionSpec(0, 150.0, 5000.0, (6.0, 18.0)),     # Resonance: narrow peak
    InjectionSpec(1, 2500.0, 6000.0, (6.0, 12.0)),    # Harshness
    InjectionSpec(2, 150.0, 400.0, (6.0, 12.0)),      # Muddiness
    InjectionSpec(3, 5000.0, 9000.0, (6.0, 14.0)),    # Sibilance
    InjectionSpec(4, 50.0, 150.0, (6.0, 12.0)),       # Boominess
    InjectionSpec(5, 100.0, 300.0, (8.0, 14.0), subtractive=True),  # Thinness
    InjectionSpec(6, 300.0, 800.0, (6.0, 12.0)),      # BoxyMidrange
]


def injections_for(schema: str = "legacy-v1") -> list[InjectionSpec]:
    """Injection specs per schema. product-v2 adds DullSound: a subtractive
    band cut whose raised-cosine top edge extends past 16 kHz — effectively a
    high-shelf loss on real material (dullness is broadband HF deficit)."""
    specs = list(INJECTIONS)
    if schema == "product-v2":
        specs.append(InjectionSpec(7, 6000.0, 16000.0, (8.0, 16.0),
                                   subtractive=True))
    return specs


def _mel_centers(sample_rate: float) -> np.ndarray:
    min_mel = hz_to_mel(20.0)
    max_mel = hz_to_mel(min(20000.0, sample_rate * 0.5))
    mel_step = (max_mel - min_mel) / float(MEL_NUM_BANDS + 1)
    return np.array([mel_to_hz(min_mel + (i + 1) * mel_step)
                     for i in range(MEL_NUM_BANDS)], dtype=np.float64)


def _detrended_mel_residual(frame_db: np.ndarray, sample_rate: float) -> np.ndarray:
    """v3-style residual used only for label-time formant awareness."""
    band_db = extract_mel_bands(db_frame_to_linear(frame_db), sample_rate)
    idx = np.arange(band_db.shape[0], dtype=np.float64)
    idx_c = idx - idx.mean()
    mean_db = float(band_db.mean())
    slope = float((idx_c @ (band_db - mean_db)) / (idx_c @ idx_c))
    trend = mean_db + slope * idx_c
    return band_db - trend


def _resonance_prom_at(frame_db: np.ndarray, sample_rate: float,
                       target_freq: float) -> tuple[float, int]:
    centers = _mel_centers(sample_rate)
    residual = _detrended_mel_residual(frame_db, sample_rate)
    band_index = int(np.argmin(np.abs(np.log(centers / target_freq))))
    return float(residual[band_index]), band_index


def _inject_resonance(frame_db: np.ndarray, sample_rate: float,
                      target_freq: float, prom_db: float,
                      sigma_mult: float = 1.0) -> np.ndarray:
    n_bins = frame_db.shape[0]
    fft_size = (n_bins - 1) * 2
    bin_hz = sample_rate / fft_size
    freqs = np.arange(n_bins) * bin_hz
    sigma = max(30.0, target_freq * 0.05) * sigma_mult
    peak = np.exp(-0.5 * ((freqs - target_freq) / sigma) ** 2)
    return frame_db + prom_db * peak


def _sample_resonance_injection(frame_db: np.ndarray, sample_rate: float,
                                rng: np.random.Generator,
                                inj: InjectionSpec) -> ResonanceInjection | None:
    """Choose a resonance target that is not already a strong raw formant.

    The previous factory sampled a random frequency and labeled the injected
    frame positive even if the raw frame already had a stronger formant there.
    That makes positives and raw negatives contradictory. This sampler first
    picks target mel bands whose raw v3/detrended prominence is low, then keeps
    only injections that create a measured post/delta margin.
    """
    centers = _mel_centers(sample_rate)
    residual = _detrended_mel_residual(frame_db, sample_rate)

    available_bins: list[np.ndarray] = []
    for lo_hz, hi_hz in RESONANCE_FREQ_BINS:
        lo = max(lo_hz, inj.lo_hz)
        hi = min(hi_hz, inj.hi_hz)
        if hi <= lo:
            continue
        idx = np.flatnonzero((centers >= lo)
                             & (centers < hi)
                             & (residual <= RESONANCE_PRE_PROM_MAX_DB))
        if idx.size:
            available_bins.append(idx)

    if not available_bins:
        return None

    attempts_left = RESONANCE_MAX_TARGET_ATTEMPTS
    for bin_index in rng.permutation(len(available_bins)):
        band_pool = available_bins[int(bin_index)]
        attempts = min(RESONANCE_TARGET_ATTEMPTS_PER_BIN, attempts_left)
        attempts_left -= attempts
        for _ in range(attempts):
            band_index = int(band_pool[int(rng.integers(0, band_pool.size))])
            target = float(centers[band_index])
            boost = float(rng.uniform(*inj.boost_db_range))
            sigma_mult = float(rng.choice(RESONANCE_SIGMA_MULTS))
            pos_db = _inject_resonance(frame_db, sample_rate, target, boost, sigma_mult)
            post_prom, _ = _resonance_prom_at(pos_db, sample_rate, target)

            candidate = ResonanceInjection(
                frame_db=pos_db,
                target_freq=target,
                boost_db=boost,
                sigma_mult=sigma_mult,
                pre_prom_db=float(residual[band_index]),
                post_prom_db=post_prom,
                band_index=band_index,
            )
            if (candidate.delta_prom_db >= RESONANCE_DELTA_PROM_MIN_DB
                    and candidate.post_prom_db >= RESONANCE_POST_PROM_MIN_DB):
                return candidate
        if attempts_left <= 0:
            break

    return None


def load_wav_mono(path: Path) -> tuple[np.ndarray, float]:
    """Minimal WAV reader (PCM16/24/32, float32) without external deps."""
    import wave
    with wave.open(str(path), "rb") as w:
        sr = w.getframerate()
        n = w.getnframes()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        raw = w.readframes(n)
    if sw == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif sw == 4:
        data = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    elif sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        vals = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8)
                | (b[:, 2].astype(np.int32) << 16))
        vals = np.where(vals >= 1 << 23, vals - (1 << 24), vals)
        data = vals.astype(np.float64) / float(1 << 23)
    else:
        raise ValueError(f"unsupported sample width {sw} in {path}")
    if ch > 1:
        data = data.reshape(-1, ch).mean(axis=1)
    return data, float(sr)


def top_energy_frames(frames_db: list[np.ndarray], sample_rate: float,
                      lo_hz: float, hi_hz: float, k: int) -> list[np.ndarray]:
    scored = sorted(frames_db,
                    key=lambda f: _band_energy_db(f, sample_rate, lo_hz, hi_hz),
                    reverse=True)
    return scored[:k]


def singer_of(path: Path) -> str:
    """VocalSet FULL layout: .../FULL/<singer>/<technique>/<file>.wav"""
    for part in path.parts:
        low = part.lower()
        if low.startswith(("female", "male")):
            return low
    return "unknown"


def real_dataset(vocalset_dir: Path, feature_version: int,
                 frames_per_clip: int = 2, clips_per_singer: int = 12,
                 seed: int = 22,
                 schema: str = "legacy-v1",
                 sib_focus_per_clip: int = 0) -> tuple[list[Sample], list[Sample]]:
    """Returns (train_samples, heldout_samples). Each selected real frame
    yields one PAIRED positive (injected problem) + one negative (the clean
    or de-emphasized frame), per the RealDataSibilanceTest pairing principle.
    """
    feat = FEATURE_FNS[feature_version]
    rng = np.random.default_rng(seed)
    specs = injections_for(schema)
    ranges = problem_freq_ranges(schema)
    names = problem_names(schema)
    wavs = sorted(vocalset_dir.rglob("*.wav"))
    by_singer: dict[str, list[Path]] = {}
    for p in wavs:
        by_singer.setdefault(singer_of(p), []).append(p)

    train: list[Sample] = []
    heldout: list[Sample] = []

    for singer, paths in sorted(by_singer.items()):
        if singer == "unknown":
            continue
        dest = heldout if singer in TEST_SINGERS else train
        pick = [paths[i] for i in
                rng.choice(len(paths), size=min(clips_per_singer, len(paths)),
                           replace=False)]
        for wav_path in pick:
            try:
                audio, sr = load_wav_mono(wav_path)
            except Exception:
                continue
            pipeline = AnalysisPipeline(sr)
            frames = pipeline.analyze(audio)
            if not frames:
                continue
            # M7 sib-focus: the M2-proven selector — the most SIBILANT frames
            # (top 5-9 kHz energy), paired boost-positive vs de-essed negative.
            # This is the targeted recovery for the vocal-sibilance recall the
            # M6 hard-negative pressure crushed; the pairing keeps the axis
            # "excess vs reduced", not "vocal vs not".
            if sib_focus_per_clip > 0:
                sib_inj = INJECTIONS[3]
                for fd in top_energy_frames(frames, sr, 5000.0, 9000.0,
                                            sib_focus_per_clip):
                    delta = float(rng.uniform(*sib_inj.boost_db_range))
                    t = sib_inj.target_freq(rng)
                    pos_db = _scale_band_db(fd, sr, sib_inj.lo_hz,
                                            sib_inj.hi_hz, +delta)
                    neg_db = _scale_band_db(fd, sr, sib_inj.lo_hz,
                                            sib_inj.hi_hz,
                                            -float(rng.uniform(2.0, 6.0)))
                    lo3, hi3 = ranges[3]
                    pt3 = np.zeros(NUM_PROBLEMS); pt3[3] = 1.0
                    ft3 = np.zeros(NUM_PROBLEMS)
                    ft3[3] = np.clip(np.log(max(t, lo3) / lo3)
                                     / np.log(hi3 / lo3), 0.0, 1.0)
                    dest.append(Sample(feat(db_frame_to_linear(pos_db), sr),
                                       pt3, ft3,
                                       f"real+SibFocus:{wav_path.name}"))
                    dest.append(Sample(feat(db_frame_to_linear(neg_db), sr),
                                       np.zeros(NUM_PROBLEMS),
                                       np.zeros(NUM_PROBLEMS),
                                       f"real-deessed:{wav_path.name}"))

            # broadband-energy frame selection (all-class generalization of
            # the 5-9 kHz sibilance selector)
            frames = top_energy_frames(frames, sr, 100.0, 10000.0,
                                       frames_per_clip)
            for frame_db in frames:
                spec_idx = int(rng.integers(0, len(specs)))
                inj = specs[spec_idx]

                if inj.problem == 0:
                    sampled = _sample_resonance_injection(frame_db, sr, rng, inj)
                    neg_db = frame_db.copy()
                    dest.append(Sample(feat(db_frame_to_linear(neg_db), sr),
                                       np.zeros(NUM_PROBLEMS), np.zeros(NUM_PROBLEMS),
                                       f"real-raw-resonance-negative:{wav_path.name}"))
                    if sampled is None:
                        continue
                    pos_db = sampled.frame_db
                    target = sampled.target_freq
                    source_suffix = (
                        f"{wav_path.name};"
                        f"pre={sampled.pre_prom_db:.1f},"
                        f"post={sampled.post_prom_db:.1f},"
                        f"delta={sampled.delta_prom_db:.1f},"
                        f"sigma={sampled.sigma_mult:.1f}"
                    )
                elif inj.subtractive:
                    delta = float(rng.uniform(*inj.boost_db_range))
                    target = inj.target_freq(rng)
                    pos_db = _scale_band_db(frame_db, sr, inj.lo_hz, inj.hi_hz,
                                            -delta)
                    neg_db = _scale_band_db(frame_db, sr, inj.lo_hz, inj.hi_hz,
                                            -float(rng.uniform(2.0, 6.0)))
                    source_suffix = wav_path.name
                else:
                    delta = float(rng.uniform(*inj.boost_db_range))
                    target = inj.target_freq(rng)
                    pos_db = _scale_band_db(frame_db, sr, inj.lo_hz, inj.hi_hz,
                                            +delta)
                    neg_db = _scale_band_db(frame_db, sr, inj.lo_hz, inj.hi_hz,
                                            -float(rng.uniform(2.0, 6.0)))
                    source_suffix = wav_path.name

                lo, hi = ranges[inj.problem]
                pt = np.zeros(NUM_PROBLEMS)
                pt[inj.problem] = 1.0
                ft = np.zeros(NUM_PROBLEMS)
                ft[inj.problem] = np.clip(np.log(max(target, lo) / lo)
                                          / np.log(hi / lo), 0.0, 1.0)

                dest.append(Sample(feat(db_frame_to_linear(pos_db), sr), pt, ft,
                                   f"real+{names[inj.problem]}:{source_suffix}"))
                if inj.problem != 0:
                    dest.append(Sample(feat(db_frame_to_linear(neg_db), sr),
                                       np.zeros(NUM_PROBLEMS), np.zeros(NUM_PROBLEMS),
                                       f"real-clean:{wav_path.name}"))
    return train, heldout


def dataset_hash(samples: list[Sample]) -> str:
    """Stable provenance hash over features + targets."""
    h = hashlib.sha256()
    for s in samples:
        h.update(np.asarray(s.features, dtype="<f4").tobytes())
        h.update(np.asarray(s.problem_targets, dtype="<f4").tobytes())
        h.update(np.asarray(s.freq_targets, dtype="<f4").tobytes())
    return h.hexdigest()[:16]
