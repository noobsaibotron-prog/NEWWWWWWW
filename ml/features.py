"""Feature extraction — EXACT port of the C++ inference path.

Mirrors, bit-for-bit at float64 precision (C++ runs float32; the contract test
uses a 1e-5 tolerance to absorb that):

  1. ``OfflineAnalysisPipeline`` (Source/Tests/Support/OfflineAnalysisPipeline.h)
     — the audio → smoothed-dB-spectrum mirror of SpectrumAnalyzer:
     Hann 4096, 50% overlap, mag = |rfft|/fftSize floored at 1e-10,
     dB clamped to [-120, +12], per-bin attack/release smoothing with
     coefficients computed at the 44100-quirk rate (see the C++ header).

  2. ``MLEngine::extractMelBands`` (Source/AI/MLEngine.cpp:489) — 64 triangular
     mel bands (HTK mel), bin-index triangular weights, energy averaged by BIN
     COUNT (not weight sum — replicated quirk), then:
       - feature v1: clamp01(gainToDecibels(e + 1e-10, -100) / -100)
                     (absolute dB map; louder band => LOWER value)
       - feature v2 (65 dims): loudness-invariant spectral shape PLUS an
                     explicit level scalar:
                       feat[0..63] = clamp01(0.5 + (bandDb - mean(bandDb)) / 80)
                       feat[64]    = clamp01(mean(bandDb) / -100)   # v1-style level
                     Shape is invariant to broadband level changes; the level
                     cue survives as ONE explicit input (Clipping/Thinness need
                     it — pure mean-subtraction measurably killed those classes).

Any change to the C++ side must be replicated here in the same commit, and
vice versa. The cross-language contract is enforced by
Source/Tests/MLFeatureContractTest.cpp + ml/tests/test_feature_contract.py
via ml/fixtures/feature_contract.json.
"""

from __future__ import annotations

import numpy as np

MEL_NUM_BANDS = 64
FEATURES_V2_DIM = MEL_NUM_BANDS + 1   # 64 shape bands + 1 level scalar
FFT_ORDER = 12
FFT_SIZE = 1 << FFT_ORDER          # 4096
NUM_BINS = FFT_SIZE // 2 + 1       # 2049
MIN_DECIBELS = -120.0
MAX_DECIBELS = 12.0
ATTACK_MS = 2.0
RELEASE_MS = 50.0

# Feature v2 normalization constants (must match MLEngine.cpp kFeatureV2*)
FEATURE_V2_RANGE_DB = 80.0   # +-40 dB around the frame mean maps to [0, 1]


def hz_to_mel(hz: float) -> float:
    """HTK mel — matches MLEngine::hzToMel."""
    return 2595.0 * np.log10(1.0 + hz / 700.0)


def mel_to_hz(mel: float) -> float:
    return 700.0 * (10.0 ** (mel / 2595.0) - 1.0)


def gain_to_decibels(gain: float, minus_infinity_db: float = -100.0) -> float:
    """juce::Decibels::gainToDecibels."""
    if gain > 0.0:
        return max(minus_infinity_db, 20.0 * np.log10(gain))
    return minus_infinity_db


def extract_mel_bands(spectrum: np.ndarray, sample_rate: float,
                      num_bands: int = MEL_NUM_BANDS) -> np.ndarray:
    """Port of MLEngine::extractMelBands — returns RAW band dB (before the
    feature map), so both v1 and v2 features share this single implementation.

    ``spectrum`` is the LINEAR-magnitude spectrum (len == fftSize/2 bins in the
    C++ synthetic path, or 2049 bins from the analysis pipeline; the C++ code
    derives fftSize from the length either way).
    """
    n_spec = int(spectrum.shape[0])
    band_db = np.full(num_bands, -100.0, dtype=np.float64)
    if n_spec == 0:
        return band_db

    fft_size = n_spec * 2
    bin_hz = float(sample_rate) / float(fft_size)

    min_mel = hz_to_mel(20.0)
    max_mel = hz_to_mel(min(20000.0, float(sample_rate) * 0.5))
    mel_step = (max_mel - min_mel) / float(num_bands + 1)

    for band in range(num_bands):
        mel_low = min_mel + band * mel_step
        mel_center = min_mel + (band + 1) * mel_step
        mel_high = min_mel + (band + 2) * mel_step

        hz_low, hz_center, hz_high = (mel_to_hz(mel_low), mel_to_hz(mel_center),
                                      mel_to_hz(mel_high))

        # C++ does static_cast<int> (truncation) then jlimit clamp
        bin_low = min(max(int(hz_low / bin_hz), 0), n_spec - 1)
        bin_center = min(max(int(hz_center / bin_hz), 0), n_spec - 1)
        bin_high = min(max(int(hz_high / bin_hz), 0), n_spec - 1)

        energy = 0.0
        count = 0
        for b in range(bin_low, bin_high + 1):
            weight = 0.0
            if b < bin_center and bin_center > bin_low:
                weight = float(b - bin_low) / float(bin_center - bin_low)
            elif b >= bin_center and bin_high > bin_center:
                weight = float(bin_high - b) / float(bin_high - bin_center)
            # C++ counts EVERY bin in range (even zero-weight) — replicated
            energy += float(spectrum[b]) * weight
            count += 1

        if count > 0:
            energy /= float(count)

        band_db[band] = gain_to_decibels(energy + 1e-10, -100.0)

    return band_db


def features_v1(spectrum: np.ndarray, sample_rate: float) -> np.ndarray:
    """Shipped v1 feature map: absolute dB per band, inverted polarity."""
    band_db = extract_mel_bands(spectrum, sample_rate)
    return np.clip(band_db / -100.0, 0.0, 1.0)


def features_v2(spectrum: np.ndarray, sample_rate: float) -> np.ndarray:
    """Loudness-invariant v2: per-frame mean-dB subtraction (log-domain CMN)
    on 64 bands + the frame mean itself as an explicit 65th input.

    A broadband level change shifts every bandDb equally -> the shape features
    are unchanged; only feat[64] moves. Spectral SHAPE and LEVEL are factored
    into separate inputs instead of being entangled (v1) or discarded (naive
    mean-subtraction).
    """
    band_db = extract_mel_bands(spectrum, sample_rate)
    mean_db = float(np.mean(band_db))
    shape = np.clip(0.5 + (band_db - mean_db) / FEATURE_V2_RANGE_DB, 0.0, 1.0)
    level = np.clip(mean_db / -100.0, 0.0, 1.0)
    return np.concatenate([shape, [level]])


def features_v3(spectrum: np.ndarray, sample_rate: float) -> np.ndarray:
    """EXPERIMENTAL v3: tilt-AND-level invariant shape + explicit level & slope.

    Per frame: fit a linear trend of bandDb over band index (log-frequency
    axis), subtract it -> detrended shape (64) + level scalar (mean dB) +
    slope scalar (dB per band span, normalized). Hypothesis: kills the
    dark-tilt Boominess FP (slope no longer masquerades as shape) and unmasks
    narrow resonances riding on steep tilts.
    """
    band_db = extract_mel_bands(spectrum, sample_rate)
    n = band_db.shape[0]
    idx = np.arange(n, dtype=np.float64)
    idx_c = idx - idx.mean()
    mean_db = float(band_db.mean())
    slope = float((idx_c @ (band_db - mean_db)) / (idx_c @ idx_c))  # dB per band
    trend = mean_db + slope * idx_c
    shape = np.clip(0.5 + (band_db - trend) / FEATURE_V2_RANGE_DB, 0.0, 1.0)
    level = np.clip(mean_db / -100.0, 0.0, 1.0)
    # slope over the full 64-band span, mapped: +-60 dB span -> [0,1]
    slope_feat = np.clip(0.5 + (slope * n) / 120.0, 0.0, 1.0)
    return np.concatenate([shape, [level, slope_feat]])


def _mel_centers(sample_rate: float, num_bands: int = MEL_NUM_BANDS) -> np.ndarray:
    min_mel = hz_to_mel(20.0)
    max_mel = hz_to_mel(min(20000.0, sample_rate * 0.5))
    mel_step = (max_mel - min_mel) / float(num_bands + 1)
    return np.array([mel_to_hz(min_mel + (i + 1) * mel_step)
                     for i in range(num_bands)], dtype=np.float64)


def _range_mean(values: np.ndarray, centers: np.ndarray,
                lo_hz: float, hi_hz: float, fallback: float) -> float:
    mask = (centers >= lo_hz) & (centers <= hi_hz)
    if not np.any(mask):
        return fallback
    return float(np.mean(values[mask]))


def _range_max(values: np.ndarray, centers: np.ndarray,
               lo_hz: float, hi_hz: float, fallback: float = 0.0) -> float:
    mask = (centers >= lo_hz) & (centers <= hi_hz)
    if not np.any(mask):
        return fallback
    return float(np.max(values[mask]))


def _rel_feature(db_value: float) -> float:
    return float(np.clip(0.5 + db_value / 80.0, 0.0, 1.0))


def _prom_feature(db_value: float) -> float:
    return float(np.clip(db_value / 40.0, 0.0, 1.0))


def features_v4(spectrum: np.ndarray, sample_rate: float) -> np.ndarray:
    """EXPERIMENTAL v4: v3 shape plus explicit musical problem descriptors.

    M8 v1/v3 runs show that the raw 64-band MLP can miss obvious low-end
    differences between a clean drum loop and a boomy kick/tom. v4 keeps the
    detrended 64-band shape + level/slope from v3, then appends compact
    hand-authored descriptors that expose region energy, low-end contrast, and
    local resonance prominence. This is lab-only until mirrored in C++.
    """
    band_db = extract_mel_bands(spectrum, sample_rate)
    n = band_db.shape[0]
    idx = np.arange(n, dtype=np.float64)
    idx_c = idx - idx.mean()
    mean_db = float(band_db.mean())
    slope = float((idx_c @ (band_db - mean_db)) / (idx_c @ idx_c))
    trend = mean_db + slope * idx_c
    residual = band_db - trend
    shape = np.clip(0.5 + residual / FEATURE_V2_RANGE_DB, 0.0, 1.0)
    level = np.clip(mean_db / -100.0, 0.0, 1.0)
    slope_feat = np.clip(0.5 + (slope * n) / 120.0, 0.0, 1.0)

    centers = _mel_centers(sample_rate, n)
    regions = {
        "sub": (35.0, 90.0),
        "boom": (50.0, 150.0),
        "mud": (150.0, 400.0),
        "box": (300.0, 800.0),
        "harsh": (2500.0, 6000.0),
        "sib": (5000.0, 9000.0),
        "air": (9000.0, 16000.0),
    }
    rel = {
        name: _range_mean(band_db, centers, lo, hi, mean_db) - mean_db
        for name, (lo, hi) in regions.items()
    }
    rel_features = [_rel_feature(rel[name]) for name in regions]

    contrast_features = [
        _rel_feature(rel["boom"] - rel["box"]),
        _rel_feature(rel["sub"] - rel["mud"]),
        _rel_feature(rel["mud"] - rel["box"]),
        _rel_feature(rel["box"] - rel["air"]),
    ]
    prom_features = [
        _prom_feature(_range_max(residual, centers, 80.0, 300.0)),
        _prom_feature(_range_max(residual, centers, 300.0, 800.0)),
        _prom_feature(_range_max(residual, centers, 800.0, 5000.0)),
    ]

    return np.concatenate([
        shape,
        [level, slope_feat],
        np.asarray(rel_features, dtype=np.float64),
        np.asarray(contrast_features, dtype=np.float64),
        np.asarray(prom_features, dtype=np.float64),
    ])


FEATURE_FNS_ALL = {1: features_v1, 2: features_v2, 3: features_v3,
                   4: features_v4}


class AnalysisPipeline:
    """Port of aieq_test::OfflineAnalysisPipeline (audio -> smoothed dB frames).

    Replicates the production quirk: smoothing coefficients are computed at
    44100 Hz regardless of the session sample rate.
    """

    def __init__(self, sample_rate: float):
        self.sample_rate = float(sample_rate)
        coeff_sr = 44100.0
        hop = FFT_SIZE / 2.0
        rate = max(10.0, coeff_sr / hop)
        self.attack_coeff = 1.0 - np.exp(-1.0 / (ATTACK_MS * 0.001 * rate))
        self.release_coeff = 1.0 - np.exp(-1.0 / (RELEASE_MS * 0.001 * rate))
        # numpy.hanning == JUCE hann (symmetric, 0.5 - 0.5cos(2pi n/(N-1)))
        self.window = np.hanning(FFT_SIZE)

    def analyze(self, audio: np.ndarray) -> list[np.ndarray]:
        """audio: mono float array (mix channels upstream). Returns a list of
        smoothed dB frames (NUM_BINS each), one per 50%-overlap hop."""
        n = int(audio.shape[0])
        hop = FFT_SIZE // 2
        frames: list[np.ndarray] = []
        if n < FFT_SIZE:
            return frames

        prev_db = np.full(NUM_BINS, MIN_DECIBELS, dtype=np.float64)
        pos = 0
        first = True
        buf = np.zeros(FFT_SIZE, dtype=np.float64)
        overlap = np.zeros(hop, dtype=np.float64)

        while True:
            if first:
                buf[:] = audio[:FFT_SIZE]
                pos = FFT_SIZE
                first = False
            else:
                if pos + hop > n:
                    break
                buf[:hop] = overlap
                buf[hop:] = audio[pos:pos + hop]
                pos += hop
            overlap[:] = buf[hop:]

            windowed = buf * self.window
            mags = np.abs(np.fft.rfft(windowed)) / float(FFT_SIZE)
            mags = np.maximum(mags, 1.0e-10)
            mag_db = np.clip(20.0 * np.log10(mags), MIN_DECIBELS, MAX_DECIBELS)

            rising = mag_db > prev_db
            coeff = np.where(rising, self.attack_coeff, self.release_coeff)
            smoothed = prev_db + coeff * (mag_db - prev_db)
            frames.append(smoothed.copy())
            prev_db = smoothed

        return frames


def db_frame_to_linear(frame_db: np.ndarray) -> np.ndarray:
    """dB -> linear magnitude, matching AIEngine's Decibels::decibelsToGain
    conversion before MLEngine inference (AIEngine.cpp:2886-2888)."""
    return 10.0 ** (frame_db / 20.0)
