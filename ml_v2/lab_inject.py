"""Motore v2 — injection primitives PORTED VERBATIM from the M6-M9 lab.

Provenance: worktree m6-lab, branch feature/model-recall-m9-lab @ bdcac804
(ml/dataset.py, ml/tier2.py, ml/temporal.py, ml/features.py). Vendored here
(rather than imported cross-worktree) so the committed trainer is reproducible
regardless of what branch the lab worktree has checked out. Semantics are
intentionally IDENTICAL — these recipes carry measured knowledge (M2 pairing,
M6 hf-negatives, M9.3 confusion axes + ring post-check, M9.5 vocal sib axis)
and per the A4 spec must be reused, not reinvented.

Everything here operates on dB-domain spectra frames [2049] or windows
[W, 2049] — exactly the PerceptualFrontEnd rawDb frames of the A2 chain.
Feature extraction (log-mel) happens AFTER injection, in dataset_v2.py.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

NUM_PROBLEMS = 8
MEL_NUM_BANDS = 64
SYNTH_HF_MIN_DB = -60.0  # A2 rawDb is FFT-scaled; this is audible HF content.

# F2b measured Boom gate. These values are frozen by
# ml_v2/reports/BOOM_GATE_PROBE.md and remain opt-in at the dataset level.
BOOM_CONTENT_MIN_DB = 9.0
BOOM_PRE_EXCESS_MAX_DB = 9.0
BOOM_POST_EXCESS_MIN_DB = 6.0
BOOM_DELTA_EXCESS_MIN_DB = 4.0

# ---- ml/dataset.py: class bands (product-v2 order: Res, Harsh, Mud, Sib,
# Boom, Thin, Boxy, Dull) --------------------------------------------------
PROBLEM_FREQ_RANGES = [
    (100.0, 5000.0),    # Resonance
    (2000.0, 8000.0),   # Harshness
    (100.0, 400.0),     # Muddiness
    (5000.0, 12000.0),  # Sibilance
    (40.0, 150.0),      # Boominess
    (80.0, 300.0),      # Thinness (lack of low-mids -> subtractive)
    (300.0, 800.0),     # BoxyMidrange
    (20.0, 20000.0),    # Clipping (legacy slot 7)
]
DULLSOUND_FREQ_RANGE = (6000.0, 16000.0)

PROBLEM_NAMES_V2 = ("Resonance", "Harshness", "Muddiness", "Sibilance",
                    "Boominess", "Thinness", "BoxyMidrange", "DullSound")


def problem_freq_ranges(schema: str = "product-v2") -> list[tuple[float, float]]:
    ranges = list(PROBLEM_FREQ_RANGES)
    if schema == "product-v2":
        ranges[7] = DULLSOUND_FREQ_RANGE
    return ranges


@dataclass
class InjectionSpec:
    problem: int
    lo_hz: float
    hi_hz: float
    boost_db_range: tuple[float, float]
    subtractive: bool = False

    def target_freq(self, rng: np.random.Generator) -> float:
        return float(self.lo_hz * (self.hi_hz / self.lo_hz) ** rng.uniform())


INJECTIONS = [
    InjectionSpec(0, 150.0, 5000.0, (6.0, 18.0)),
    InjectionSpec(1, 2500.0, 6000.0, (6.0, 12.0)),
    InjectionSpec(2, 150.0, 400.0, (6.0, 12.0)),
    InjectionSpec(3, 5000.0, 9000.0, (6.0, 14.0)),
    InjectionSpec(4, 50.0, 150.0, (6.0, 12.0)),
    InjectionSpec(5, 100.0, 300.0, (8.0, 14.0), subtractive=True),
    InjectionSpec(6, 300.0, 800.0, (6.0, 12.0)),
]


def injections_for(schema: str = "product-v2") -> list[InjectionSpec]:
    specs = list(INJECTIONS)
    if schema == "product-v2":
        specs.append(InjectionSpec(7, 6000.0, 16000.0, (8.0, 16.0),
                                   subtractive=True))
    return specs


def injectable(inj: InjectionSpec, sr: float) -> bool:
    """ml/tier2.py: honest only if the band is inside the file's bandwidth
    (also self-handles 16 kHz BabySlakh: HF classes are never injected)."""
    return inj.hi_hz <= sr * 0.45


# ---- ml/dataset.py: dB-domain band scaling -------------------------------
def scale_band_db(frame_db: np.ndarray, sample_rate: float, lo_hz: float,
                  hi_hz: float, delta_db: float,
                  edge_octaves: float = 0.25) -> np.ndarray:
    n_bins = frame_db.shape[0]
    fft_size = (n_bins - 1) * 2
    bin_hz = sample_rate / fft_size
    freqs = np.maximum(1.0, np.arange(n_bins) * bin_hz)
    log_f = np.log2(freqs)
    lo_l, hi_l = np.log2(lo_hz), np.log2(hi_hz)
    ramp_in = np.clip((log_f - (lo_l - edge_octaves)) / edge_octaves, 0.0, 1.0)
    ramp_out = np.clip(((hi_l + edge_octaves) - log_f) / edge_octaves, 0.0, 1.0)
    win = np.minimum(ramp_in, ramp_out)
    win = 0.5 - 0.5 * np.cos(np.pi * win)
    return frame_db + delta_db * win


def color_window_db(win_db: np.ndarray, sr: float, rng: np.random.Generator,
                    ranges: list[tuple[float, float]],
                    hf_dead: bool = False,
                    max_hz: float = 6000.0,
                    gain_db_range: tuple[float, float] = (2.0, 4.0),
                    q_range: tuple[float, float] = (0.5, 2.0)
                    ) -> np.ndarray | None:
    """Mild broad EQ colour for all-zero hard negatives.

    Q is implemented explicitly as bandwidth = center / Q. These negatives
    teach colour != problem, so the band is intentionally wide and low gain.
    """
    nyq_limit = min(sr * 0.45, max_hz)
    hard_hi = min(nyq_limit, 8000.0) if hf_dead else nyq_limit
    eligible: list[tuple[float, float]] = []
    for lo, hi in ranges:
        hi_c = min(float(hi), hard_hi)
        lo_c = max(float(lo), 20.0)
        if hi_c > lo_c * 1.05:
            eligible.append((lo_c, hi_c))
    if not eligible:
        return None

    lo, hi = eligible[int(rng.integers(0, len(eligible)))]
    center = float(lo * (hi / lo) ** rng.uniform())
    q = float(rng.uniform(*q_range))
    bandwidth = max(20.0, center / max(q, 1.0e-6))
    band_lo = max(lo, center - 0.5 * bandwidth, 20.0)
    band_hi = min(hi, center + 0.5 * bandwidth, hard_hi)
    if band_hi <= band_lo * 1.05:
        band_lo, band_hi = lo, hi
    gain = float(rng.uniform(*gain_db_range))
    return np.stack([
        scale_band_db(f, sr, band_lo, band_hi, gain, edge_octaves=0.5)
        for f in win_db
    ])


def q_band_window_db(win_db: np.ndarray, sr: float, center_hz: float,
                     q: float, gain_db: float,
                     edge_octaves: float = 0.25) -> np.ndarray | None:
    bandwidth = max(20.0, center_hz / max(q, 1.0e-6))
    lo = max(20.0, center_hz - 0.5 * bandwidth)
    hi = min(sr * 0.45, center_hz + 0.5 * bandwidth)
    if hi <= lo * 1.05:
        return None
    return np.stack([
        scale_band_db(f, sr, lo, hi, gain_db, edge_octaves=edge_octaves)
        for f in win_db
    ])


def synth_harsh_sib_windows_db(win_db: np.ndarray, sr: float,
                               rng: np.random.Generator
                               ) -> list[tuple[int, np.ndarray, float]]:
    """Same-file synth axis: raw HF synth vs harsh 2-4 kHz vs sib 6-12 kHz."""
    if float(band_series(win_db, sr, 4000.0, min(12000.0, sr * 0.45)).max()) <= SYNTH_HF_MIN_DB:
        return []

    out: list[tuple[int, np.ndarray, float]] = []
    harsh_hi = min(4000.0, sr * 0.45)
    if harsh_hi > 2000.0 * 1.05:
        center = float(2000.0 * (harsh_hi / 2000.0) ** rng.uniform())
        boosted = q_band_window_db(win_db, sr, center,
                                   float(rng.uniform(1.0, 2.0)),
                                   float(rng.uniform(6.0, 10.0)))
        if boosted is not None:
            out.append((1, boosted, center))

    sib_hi = min(12000.0, sr * 0.45)
    if sib_hi > 6000.0 * 1.05:
        center = float(6000.0 * (sib_hi / 6000.0) ** rng.uniform())
        boosted = q_band_window_db(win_db, sr, center,
                                   float(rng.uniform(2.0, 4.0)),
                                   float(rng.uniform(6.0, 10.0)))
        if boosted is not None:
            out.append((3, boosted, center))
    return out


def band_series(win_db: np.ndarray, sr: float, lo: float, hi: float) -> np.ndarray:
    """ml/temporal.py: mean dB in [lo,hi] per frame -> [W]."""
    n_bins = win_db.shape[1]
    fft_size = (n_bins - 1) * 2
    bin_hz = sr / fft_size
    a = max(1, int(lo / bin_hz))
    b = min(n_bins - 1, int(hi / bin_hz))
    if b <= a:
        return np.full(win_db.shape[0], -100.0)
    return win_db[:, a:b + 1].mean(axis=1)


@dataclass(frozen=True)
class BoomInjectionMeasurement:
    content_relative_db: float
    pre_excess_db: float
    post_excess_db: float
    delta_excess_db: float


def measure_boom_injection(raw_db: np.ndarray, boosted_db: np.ndarray,
                           sample_rate: float) -> BoomInjectionMeasurement:
    """Measure low-end content and local excess without an absolute level gate."""
    raw = np.asarray(raw_db, dtype=np.float64)
    boosted = np.asarray(boosted_db, dtype=np.float64)
    if raw.ndim != 2 or raw.shape != boosted.shape or raw.shape[1] < 3:
        raise ValueError("raw_db and boosted_db must be matching [frames, bins] arrays")
    if not np.isfinite(raw).all() or not np.isfinite(boosted).all():
        raise ValueError("Boom measurement received non-finite values")
    if sample_rate <= 0.0 or not np.isfinite(sample_rate):
        raise ValueError("sample_rate must be finite and positive")
    wide_hi = min(10000.0, sample_rate * 0.45)
    if wide_hi <= 150.0:
        raise ValueError("sample_rate is too low for the Boom measurement")

    raw_low = float(band_series(raw, sample_rate, 40.0, 150.0).mean())
    raw_ref = float(band_series(raw, sample_rate, 150.0, 400.0).mean())
    raw_wide = float(band_series(raw, sample_rate, 100.0, wide_hi).mean())
    post_low = float(band_series(boosted, sample_rate, 40.0, 150.0).mean())
    post_ref = float(band_series(boosted, sample_rate, 150.0, 400.0).mean())
    pre_excess = raw_low - raw_ref
    post_excess = post_low - post_ref
    return BoomInjectionMeasurement(
        content_relative_db=raw_low - raw_wide,
        pre_excess_db=pre_excess,
        post_excess_db=post_excess,
        delta_excess_db=post_excess - pre_excess,
    )


def boom_injection_qualifies(measurement: BoomInjectionMeasurement) -> bool:
    return (
        measurement.content_relative_db >= BOOM_CONTENT_MIN_DB
        and measurement.pre_excess_db <= BOOM_PRE_EXCESS_MAX_DB
        and measurement.post_excess_db >= BOOM_POST_EXCESS_MIN_DB
        and measurement.delta_excess_db >= BOOM_DELTA_EXCESS_MIN_DB
    )


# ---- ml/features.py: legacy mel in RAW dB (for the resonance residual) ----
def hz_to_mel(hz: float) -> float:
    return 2595.0 * np.log10(1.0 + hz / 700.0)


def mel_to_hz(mel: float) -> float:
    return 700.0 * (10.0 ** (mel / 2595.0) - 1.0)


def db_frame_to_linear(frame_db: np.ndarray) -> np.ndarray:
    return 10.0 ** (frame_db / 20.0)


def extract_mel_bands_db(spectrum_linear: np.ndarray, sample_rate: float,
                         num_bands: int = MEL_NUM_BANDS) -> np.ndarray:
    """ml/features.py extract_mel_bands: raw band dB (float64, label-time only)."""
    n_spec = int(spectrum_linear.shape[0])
    band_db = np.full(num_bands, -100.0, dtype=np.float64)
    if n_spec == 0:
        return band_db
    fft_size = n_spec * 2
    bin_hz = float(sample_rate) / float(fft_size)
    min_mel = hz_to_mel(20.0)
    max_mel = hz_to_mel(min(20000.0, float(sample_rate) * 0.5))
    mel_step = (max_mel - min_mel) / float(num_bands + 1)
    for band in range(num_bands):
        hz_low = mel_to_hz(min_mel + band * mel_step)
        hz_center = mel_to_hz(min_mel + (band + 1) * mel_step)
        hz_high = mel_to_hz(min_mel + (band + 2) * mel_step)
        bin_low = min(max(int(hz_low / bin_hz), 0), n_spec - 1)
        bin_center = min(max(int(hz_center / bin_hz), 0), n_spec - 1)
        bin_high = min(max(int(hz_high / bin_hz), 0), n_spec - 1)
        energy, count = 0.0, 0
        for b in range(bin_low, bin_high + 1):
            weight = 0.0
            if b < bin_center and bin_center > bin_low:
                weight = (b - bin_low) / (bin_center - bin_low)
            elif b >= bin_center and bin_high > bin_center:
                weight = (bin_high - b) / (bin_high - bin_center)
            energy += spectrum_linear[b] * weight
            count += 1
        if count > 0:
            energy /= count
        band_db[band] = max(-100.0, 20.0 * np.log10(energy + 1e-10))
    return band_db


def mel_centers(sample_rate: float) -> np.ndarray:
    min_mel = hz_to_mel(20.0)
    max_mel = hz_to_mel(min(20000.0, sample_rate * 0.5))
    mel_step = (max_mel - min_mel) / float(MEL_NUM_BANDS + 1)
    return np.array([mel_to_hz(min_mel + (i + 1) * mel_step)
                     for i in range(MEL_NUM_BANDS)], dtype=np.float64)


def detrended_mel_residual(frame_db: np.ndarray, sample_rate: float) -> np.ndarray:
    """ml/dataset.py: v3-style residual, label-time formant awareness."""
    band_db = extract_mel_bands_db(db_frame_to_linear(frame_db), sample_rate)
    idx = np.arange(band_db.shape[0], dtype=np.float64)
    idx_c = idx - idx.mean()
    mean_db = float(band_db.mean())
    slope = float((idx_c @ (band_db - mean_db)) / (idx_c @ idx_c))
    trend = mean_db + slope * idx_c
    return band_db - trend


# ---- ml/dataset.py: measured resonance injection -------------------------
RESONANCE_PRE_PROM_MAX_DB = 6.0
RESONANCE_DELTA_PROM_MIN_DB = 6.0
RESONANCE_POST_PROM_MIN_DB = 10.0
RESONANCE_MAX_TARGET_ATTEMPTS = 160
RESONANCE_TARGET_ATTEMPTS_PER_BIN = 32
RESONANCE_SIGMA_MULTS = (0.5, 1.0, 2.0)
RESONANCE_FREQ_BINS = (
    (150.0, 300.0), (300.0, 700.0), (700.0, 1100.0),
    (1100.0, 2500.0), (2500.0, 5000.0),
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


def _resonance_prom_at(frame_db: np.ndarray, sample_rate: float,
                       target_freq: float) -> tuple[float, int]:
    centers = mel_centers(sample_rate)
    residual = detrended_mel_residual(frame_db, sample_rate)
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


def sample_resonance_injection(frame_db: np.ndarray, sample_rate: float,
                               rng: np.random.Generator,
                               inj: InjectionSpec) -> ResonanceInjection | None:
    """ml/dataset.py _sample_resonance_injection: pick a target that is not
    already a strong raw formant; keep only injections with measured
    post/delta prominence margin."""
    centers = mel_centers(sample_rate)
    residual = detrended_mel_residual(frame_db, sample_rate)

    available_bins: list[np.ndarray] = []
    for lo_hz, hi_hz in RESONANCE_FREQ_BINS:
        lo = max(lo_hz, inj.lo_hz)
        hi = min(hi_hz, inj.hi_hz)
        if hi <= lo:
            continue
        idx = np.flatnonzero((centers >= lo) & (centers < hi)
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
            pos_db = _inject_resonance(frame_db, sample_rate, target, boost,
                                       sigma_mult)
            post_prom, _ = _resonance_prom_at(pos_db, sample_rate, target)
            cand = ResonanceInjection(pos_db, target, boost, sigma_mult,
                                      float(residual[band_index]), post_prom,
                                      band_index)
            if (cand.delta_prom_db >= RESONANCE_DELTA_PROM_MIN_DB
                    and cand.post_prom_db >= RESONANCE_POST_PROM_MIN_DB):
                return cand
        if attempts_left <= 0:
            break
    return None


# ---- ml/temporal.py: M9.3 axes + ring persistence post-check --------------
CONTRASTIVE_AXIS_PROBLEMS = {
    "clean_drums": (6, 2, 4, 7),   # Boxy, Mud, Boom, Dull
    "clean_synth": (1, 5),         # Harsh, Thin
    "clean_bass":  (2, 4, 5),      # Mud, Boom, Thin
    "clean_mix":   (6, 2),         # Boxy, Mud
}
CONTRASTIVE_RING_FOLDERS = ("clean_drums",)
RING_MAX_ATTEMPTS = 6
RING_DECAY_RANGE = (0.35, 0.65)

_PERS_RES_DB = 6.0
_PERS_LO_HZ, _PERS_HI_HZ = 100.0, 5000.0


def _mel_residuals_window(win_db: np.ndarray, sr: float) -> np.ndarray:
    return np.stack([detrended_mel_residual(f, sr) for f in win_db])


def persistence_scores(win_db: np.ndarray, sr: float) -> tuple[float, float]:
    """(persFrac, persRes) of ml/temporal.py temporal_descriptors[7:9] —
    the measured post-check pair the ring injection must dominate."""
    res = _mel_residuals_window(win_db, sr)
    centers = mel_centers(sr)
    zone = (centers >= _PERS_LO_HZ) & (centers <= _PERS_HI_HZ)
    pers = (res[:, zone] > _PERS_RES_DB).mean(axis=0)
    mean_res = res[:, zone].mean(axis=0)
    score = pers * np.maximum(mean_res, 0.0)
    if not score.size:
        return 0.0, 0.0
    b = int(np.argmax(score))
    return float(pers[b]), float(mean_res[b]) / 10.0


def _longest_true_run(mask: np.ndarray) -> int:
    best = cur = 0
    for v in mask:
        cur = cur + 1 if bool(v) else 0
        best = max(best, cur)
    return best


def detect_real_resonance_window_db(win_db: np.ndarray, sr: float
                                    ) -> tuple[float, float] | None:
    """High-confidence real resonance detector for train-time labels only.

    Criteria: narrow residual peak, >=6 dB local prominence, >=5 consecutive
    frames, 120 Hz-5 kHz, with per-frame detrending to reject broad tilt.
    Returns (target_freq_hz, mean_prominence_db) when accepted.
    """
    residual = _mel_residuals_window(win_db, sr)
    centers = mel_centers(sr)
    zone = np.flatnonzero((centers >= 120.0) & (centers <= 5000.0))
    best: tuple[float, float, int] | None = None
    for b in zone:
        if b < 3 or b + 3 >= residual.shape[1]:
            continue
        side = np.concatenate([residual[:, b - 3:b - 1],
                               residual[:, b + 2:b + 4]], axis=1)
        side_mean = side.mean(axis=1)
        prom = residual[:, b] - side_mean
        peak = residual[:, b]
        immediate = np.stack([residual[:, b - 1], residual[:, b + 1]], axis=1)
        outer = np.stack([residual[:, b - 2], residual[:, b + 2]], axis=1)
        # Width guard: at most one immediate neighbour may be close to the peak,
        # and the outer neighbours must already have dropped.
        too_wide = ((immediate >= (peak[:, None] - 3.0)).sum(axis=1) > 1) \
            | (outer.max(axis=1) >= (peak - 3.0))
        hit = (prom >= 6.0) & ~too_wide
        run = _longest_true_run(hit)
        if run < 5:
            continue
        mean_prom = float(prom[hit].mean())
        if best is None or mean_prom > best[1]:
            best = (float(centers[b]), mean_prom, run)
    if best is None:
        return None
    return best[0], best[1]


def ring_window_db(win_db: np.ndarray, sr: float, rng: np.random.Generator,
                   spec: InjectionSpec) -> tuple[np.ndarray, float] | None:
    """ml/temporal.py _ring_window, dB-domain half: decaying narrow-band boost
    kept only if its persistence measurably dominates the raw window.
    Returns (boosted_window_db, target_freq) or None."""
    assert spec.problem == 0
    mid = win_db[win_db.shape[0] // 2]
    w = win_db.shape[0]
    raw_frac, raw_res = persistence_scores(win_db, sr)
    for _ in range(RING_MAX_ATTEMPTS):
        sampled = sample_resonance_injection(mid, sr, rng, spec)
        if sampled is None:
            return None
        target = sampled.target_freq
        delta_start = float(sampled.boost_db)
        delta_end = delta_start * float(rng.uniform(*RING_DECAY_RANGE))
        deltas = np.linspace(delta_start, delta_end, w)
        boosted = np.stack([
            scale_band_db(win_db[t], sr, target * 0.94, target * 1.06,
                          float(deltas[t]), edge_octaves=0.15)
            for t in range(w)])
        ring_frac, ring_res = persistence_scores(boosted, sr)
        if ring_frac >= raw_frac and ring_res > raw_res:
            return boosted, target
    return None


def inject_window_db(win_db: np.ndarray, sr: float, rng: np.random.Generator,
                     inj: InjectionSpec, measured_boom_gate: bool = False
                     ) -> tuple[np.ndarray, float] | None:
    """ml/temporal.py _inject_window, dB-domain half: ONE injection applied to
    EVERY frame (persistent signature). Returns (boosted_db, target_freq)."""
    if inj.problem == 0:
        mid = win_db[win_db.shape[0] // 2]
        sampled = sample_resonance_injection(mid, sr, rng, inj)
        if sampled is None:
            return None
        target = sampled.target_freq
        boosted = np.stack([
            scale_band_db(f, sr, target * 0.94, target * 1.06,
                          sampled.boost_db, edge_octaves=0.15)
            for f in win_db])
        return boosted, target
    delta = float(rng.uniform(*inj.boost_db_range))
    target = inj.target_freq(rng)
    sign = -1.0 if inj.subtractive else 1.0
    boosted = np.stack([scale_band_db(f, sr, inj.lo_hz, inj.hi_hz, sign * delta)
                        for f in win_db])
    if (measured_boom_gate and inj.problem == 4
            and not boom_injection_qualifies(
                measure_boom_injection(win_db, boosted, sr))):
        return None
    return boosted, target


def freq_target(problem: int, target_hz: float, ranges) -> float:
    """ml/tier2.py normalization: log position of the target in the class band."""
    lo, hi = ranges[problem]
    return float(np.clip(np.log(max(target_hz, lo) / lo) / np.log(hi / lo),
                         0.0, 1.0))
