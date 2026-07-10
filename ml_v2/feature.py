"""Motore v2 — A2 feature extraction, bit-faithful Python replica of the C++.

Replicates, in float32 where the C++ is float32:

1. The PerceptualFrontEnd main STFT path (PerceptualFrontEnd.cpp::processOneFrame):
   FFT 4096 / hop 2048 (50% overlap, priming consumes a full 4096), JUCE Hann
   window (symmetric, cos(2*pi*i/(size-1)), normalise=true: factor size/sum with
   a SEQUENTIAL float32 accumulation), mag = |X_k| / 4096, floor 1e-10,
   rawDb = clamp(-120, +12, 20*log10(mag)).

2. The legacy mel filterbank (MLEngine::extractMelBands) adapted to a dB-domain
   input frame, matching the gated C++ variant `aieq::melBandsFromDb`
   (Source/AI/MotoreV2Features.h). Quirks preserved on purpose:
     - fftSize = numBins*2 (2049 bins -> 4098, NOT 4096) for the binHz mapping;
     - `count` counts EVERY bin in [binLow..binHigh], including zero-weight ones;
     - output is dB mapped to [0,1] INVERTED: gainToDecibels(e+1e-10,-100)/-100
       (0 dB -> 0.0, -100 dB -> 1.0).
   dB -> linear per bin is an explicit 10^(dB/20) (NOT Decibels::decibelsToGain,
   whose -100 dB cutoff would zero the -120 dB clamped floor bins).

NO librosa anywhere: any library filterbank/window differs subtly and breaks
the parity contract (A2 design decision #2).
"""
from __future__ import annotations

import numpy as np

FFT_SIZE = 4096
HOP_SIZE = 2048
NUM_BINS = FFT_SIZE // 2 + 1     # 2049
RAW_MIN_DB = -120.0
RAW_MAX_DB = 12.0
N_MELS = 64

_f32 = np.float32


def juce_hann_window(size: int = FFT_SIZE) -> np.ndarray:
    """JUCE WindowingFunction<float>(size, hann, normalise=true), bit-faithful.

    ncos<float>(2,i,size) = cosf(float(2*i) * pi_f / float(size-1));
    samples[i] = float(0.5 - 0.5*cos2)  (double math, cast to float);
    normalise: factor = float(size)/sum with SEQUENTIAL float32 accumulation.
    """
    i = np.arange(size, dtype=np.float64)
    arg = (i * 2.0).astype(_f32) * _f32(np.pi) / _f32(size - 1)
    cos2 = np.cos(arg.astype(_f32)).astype(_f32)
    w = (0.5 - 0.5 * cos2.astype(np.float64)).astype(_f32)

    acc = _f32(0.0)
    for v in w:                       # sequential f32 sum, same order as C++
        acc = _f32(acc + v)
    factor = _f32(_f32(size) / acc)
    return (w * factor).astype(_f32)


def frontend_raw_db_frames(samples: np.ndarray) -> np.ndarray:
    """PerceptualFrontEnd main-path rawDb frames for a mono float32 buffer.

    The C++ streaming (priming + overlap ring) is arithmetically identical to
    frame k = samples[k*HOP : k*HOP + FFT_SIZE]; frames complete only when the
    full 4096 are available. Returns [numFrames, NUM_BINS] float32 dB.
    """
    x = np.asarray(samples, dtype=_f32)
    if x.size < FFT_SIZE:
        return np.zeros((0, NUM_BINS), dtype=_f32)
    n_frames = (x.size - FFT_SIZE) // HOP_SIZE + 1
    win = juce_hann_window()
    out = np.empty((n_frames, NUM_BINS), dtype=_f32)
    for f in range(n_frames):
        frame = x[f * HOP_SIZE : f * HOP_SIZE + FFT_SIZE]
        y = (frame * win).astype(_f32)
        spec = np.fft.rfft(y)                       # f32 in -> complex64
        mag = (np.abs(spec).astype(_f32) / _f32(FFT_SIZE)).astype(_f32)
        mag = np.maximum(mag, _f32(1.0e-10))
        db = (_f32(20.0) * np.log10(mag)).astype(_f32)
        out[f] = np.clip(db, _f32(RAW_MIN_DB), _f32(RAW_MAX_DB))
    return out


def _hz_to_mel(hz: np.float32) -> np.float32:
    return _f32(_f32(2595.0) * _f32(np.log10(_f32(1.0) + _f32(hz) / _f32(700.0))))


def _mel_to_hz(mel: np.float32) -> np.float32:
    return _f32(_f32(700.0) * (_f32(10.0) ** (_f32(mel) / _f32(2595.0)) - _f32(1.0)))


def extract_mel_bands_from_db(raw_db: np.ndarray, sample_rate: float,
                              n_mels: int = N_MELS) -> np.ndarray:
    """Legacy extractMelBands math on a dB-domain frame (2049 bins) — the
    Python twin of aieq::melBandsFromDb. Returns [n_mels] float32 in [0,1]."""
    spectrum = (_f32(10.0) ** (np.asarray(raw_db, dtype=_f32) / _f32(20.0))).astype(_f32)
    num_bins = spectrum.size

    fft_size = num_bins * 2                                   # legacy quirk: 4098
    bin_hz = _f32(_f32(sample_rate) / _f32(fft_size))

    min_mel = _hz_to_mel(_f32(20.0))
    max_mel = _hz_to_mel(_f32(min(20000.0, sample_rate * 0.5)))
    mel_step = _f32((max_mel - min_mel) / _f32(n_mels + 1))

    mel = np.zeros(n_mels, dtype=_f32)
    for band in range(n_mels):
        mel_low = _f32(min_mel + _f32(band) * mel_step)
        mel_center = _f32(min_mel + _f32(band + 1) * mel_step)
        mel_high = _f32(min_mel + _f32(band + 2) * mel_step)

        bin_low = int(_f32(_mel_to_hz(mel_low) / bin_hz))     # C++ int() truncation
        bin_center = int(_f32(_mel_to_hz(mel_center) / bin_hz))
        bin_high = int(_f32(_mel_to_hz(mel_high) / bin_hz))

        bin_low = min(max(bin_low, 0), num_bins - 1)
        bin_center = min(max(bin_center, 0), num_bins - 1)
        bin_high = min(max(bin_high, 0), num_bins - 1)

        energy = _f32(0.0)
        count = 0
        for b in range(bin_low, bin_high + 1):
            weight = _f32(0.0)
            if b < bin_center and bin_center > bin_low:
                weight = _f32(_f32(b - bin_low) / _f32(bin_center - bin_low))
            elif b >= bin_center and bin_high > bin_center:
                weight = _f32(_f32(bin_high - b) / _f32(bin_high - bin_center))
            energy = _f32(energy + spectrum[b] * weight)
            count += 1

        if count > 0:
            energy = _f32(energy / _f32(count))

        # juce::Decibels::gainToDecibels(energy + 1e-10, -100): arg always > 0
        g = _f32(energy + _f32(1.0e-10))
        db = max(_f32(-100.0), _f32(_f32(20.0) * _f32(np.log10(g))))
        mel[band] = min(max(_f32(db / _f32(-100.0)), _f32(0.0)), _f32(1.0))
    return mel


def logmel_frames(samples: np.ndarray, sample_rate: float,
                  n_mels: int = N_MELS) -> np.ndarray:
    """Full A2 chain: mono audio -> [numFrames, n_mels] float32 log-mel."""
    raw = frontend_raw_db_frames(samples)
    return np.stack([extract_mel_bands_from_db(fr, sample_rate, n_mels)
                     for fr in raw]) if raw.shape[0] else np.zeros((0, n_mels), dtype=_f32)
