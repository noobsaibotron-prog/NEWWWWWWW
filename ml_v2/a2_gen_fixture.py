#!/usr/bin/env python3
"""Motore v2 — A2 parity fixture generator.

Synthesizes a deterministic mono test signal (tones at product-relevant
frequencies + a sweep + seeded noise floor), writes it as a float32 WAV, runs
the FULL Python A2 chain (PerceptualFrontEnd STFT replica -> legacy mel from
dB), and writes the expected log-mel frames. The C++ side
(MotoreV2FeatureParityTest) loads the WAV, feeds the REAL PerceptualFrontEnd,
applies aieq::melBandsFromDb, and must match every frame at max|Delta| < 1e-5.

Run:  python3 -m ml_v2.a2_gen_fixture   (from the repo root)
"""
import os
import struct

import numpy as np

from .feature import FFT_SIZE, HOP_SIZE, N_MELS, logmel_frames

SEED = 20260712
SR = 44100
N_FRAMES = 36
N_SAMPLES = FFT_SIZE + (N_FRAMES - 1) * HOP_SIZE      # exactly N_FRAMES frames

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "Source", "Tests", "data",
                                        "motore_v2_a2"))


def synth_signal() -> np.ndarray:
    """Tones (low/mid/presence/sibilance regions) + sweep + noise, float32."""
    rng = np.random.default_rng(SEED)
    t = np.arange(N_SAMPLES, dtype=np.float64) / SR
    x = (0.30 * np.sin(2 * np.pi * 110.0 * t)
         + 0.20 * np.sin(2 * np.pi * 990.0 * t)
         + 0.12 * np.sin(2 * np.pi * 3480.0 * t)
         + 0.08 * np.sin(2 * np.pi * 7040.0 * t))
    # slow sweep 400 -> 2400 Hz across the clip (time-varying content)
    f0, f1 = 400.0, 2400.0
    phase = 2 * np.pi * (f0 * t + (f1 - f0) * t * t / (2 * t[-1]))
    x += 0.10 * np.sin(phase)
    x += rng.normal(0.0, 0.01, N_SAMPLES)              # ~-40 dB noise floor
    return (0.5 * x / np.max(np.abs(x))).astype(np.float32)


def write_wav_f32(path: str, samples: np.ndarray, sr: int) -> None:
    """Minimal RIFF/WAVE writer, format 3 (IEEE float32), mono."""
    data = samples.astype("<f4").tobytes()
    hdr = b"RIFF" + struct.pack("<I", 4 + 26 + 12 + len(data)) + b"WAVE"
    fmt = struct.pack("<IHHIIHHH", 18, 3, 1, sr, sr * 4, 4, 32, 0)
    with open(path, "wb") as f:
        f.write(hdr + b"fmt " + fmt + b"fact" + struct.pack("<II", 4, len(samples))
                + b"data" + struct.pack("<I", len(data)) + data)


def main() -> None:
    x = synth_signal()
    mel = logmel_frames(x, SR)                          # [N_FRAMES, 64]
    assert mel.shape == (N_FRAMES, N_MELS), mel.shape

    os.makedirs(OUT_DIR, exist_ok=True)
    write_wav_f32(os.path.join(OUT_DIR, "input.wav"), x, SR)
    np.savetxt(os.path.join(OUT_DIR, "expected_mel.csv"), mel.astype(np.float64),
               delimiter=",", fmt="%.9e")

    print(f"A2: sr={SR} samples={N_SAMPLES} frames={mel.shape[0]} mels={N_MELS} seed={SEED}")
    print(f"mel range: [{mel.min():.6f}, {mel.max():.6f}]")
    print(f"wrote fixture to {OUT_DIR}")


if __name__ == "__main__":
    main()
