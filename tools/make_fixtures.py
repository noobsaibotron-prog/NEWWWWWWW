#!/usr/bin/env python3
"""
make_fixtures.py — renders the starter AI evaluation corpus (Roadmap v1, P1).

Stdlib-only (wave/struct/math/random): no numpy/scipy dependency, fully
deterministic (fixed seeds), royalty-free by construction (synthesized audio).

Outputs into TestAssets/ai_corpus/ :
  - res3200_pink.wav    pink-ish noise + strong narrow 3.2 kHz resonance (labeled problem)
  - clean_pink.wav      pink-ish noise only (negative control: NO detections expected)
  - clean_dark_tilt.wav darker, low-passed pink (negative control on a warm/dark tilt)
  - manifest.json       labels: type, f0, tolerance_cents, mode, severity, split

Run from the repo root:  python3 tools/make_fixtures.py
"""

import json
import math
import os
import random
import struct
import wave

SR = 48000
DUR_S = 6.0
N = int(SR * DUR_S)
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "TestAssets", "ai_corpus")


def write_wav(path, samples):
    """16-bit PCM mono WAV. samples: float list in [-1, 1]."""
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32767.0))
        w.writeframes(bytes(frames))


def pink_noise(n, seed):
    """Paul Kellet's economy pink filter over seeded white noise (~-3 dB/oct)."""
    rng = random.Random(seed)
    b0 = b1 = b2 = 0.0
    out = []
    for _ in range(n):
        white = rng.uniform(-1.0, 1.0)
        b0 = 0.99765 * b0 + white * 0.0990460
        b1 = 0.96300 * b1 + white * 0.2965164
        b2 = 0.57000 * b2 + white * 1.0526913
        out.append((b0 + b1 + b2 + white * 0.1848) * 0.11)
    return out


def one_pole_lowpass(samples, cutoff_hz):
    """Simple one-pole LP: adds ~-6 dB/oct above cutoff (darker tilt)."""
    a = math.exp(-2.0 * math.pi * cutoff_hz / SR)
    y = 0.0
    out = []
    for s in samples:
        y = (1.0 - a) * s + a * y
        out.append(y)
    return out


def add_resonance(samples, f0, gain):
    """Adds a steady sine at f0 — reads as a narrow resonance peak in the spectrum."""
    return [s + gain * math.sin(2.0 * math.pi * f0 * i / SR)
            for i, s in enumerate(samples)]


def normalize(samples, peak=0.5):
    m = max(abs(s) for s in samples) or 1.0
    return [s * peak / m for s in samples]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    # 1) Pink + 3.2 kHz resonance (the labeled problem clip).
    base = pink_noise(N, seed=101)
    res = add_resonance(normalize(base, 0.35), 3200.0, 0.18)
    write_wav(os.path.join(OUT_DIR, "res3200_pink.wav"), normalize(res, 0.5))

    # 2) Clean pink (negative control).
    clean = normalize(pink_noise(N, seed=202), 0.5)
    write_wav(os.path.join(OUT_DIR, "clean_pink.wav"), clean)

    # 3) Clean dark tilt (negative control on warm/dark material).
    dark = normalize(one_pole_lowpass(pink_noise(N, seed=303), 800.0), 0.5)
    write_wav(os.path.join(OUT_DIR, "clean_dark_tilt.wav"), dark)

    # known_fail: the clip documents a MEASURED gap of the CURRENT detector
    # (P1 baseline). The harness logs it as KNOWN_FAIL instead of hard-failing;
    # fixing the gap (and flipping known_fail back to false) is roadmap work.
    manifest = {
        "version": 1,
        "sample_rate": SR,
        "clips": [
            {
                "file": "res3200_pink.wav",
                "source": "synthesized: pink noise + steady 3.2 kHz sine",
                "split": "eval",
                "expected": "Resonance",
                "known_fail": True,
                "known_fail_reason": ("P1-GAP-001: live path misses the true 3.2 kHz "
                                      "resonance. ML raw probabilities are high "
                                      "(Res~0.997) but over-fire on 4 classes and the "
                                      "decision rule + AIEngine reality-check vetoes "
                                      "flatten everything on real audio. Baseline gap "
                                      "for P2/P4."),
                "problems": [
                    {"type": "Resonance", "f0": 3200.0,
                     "tolerance_cents": 400, "mode": "static", "severity": "strong"}
                ],
            },
            {
                "file": "clean_pink.wav",
                "source": "synthesized: pink noise",
                "split": "eval",
                "expected": "None",
                "known_fail": False,
                "problems": [],
            },
            {
                "file": "clean_dark_tilt.wav",
                "source": "synthesized: low-passed pink (dark tilt)",
                "split": "eval",
                "expected": "None",
                "known_fail": True,
                "known_fail_reason": ("P1-GAP-002: Hybrid at sens 0.2 emits one heuristic "
                                      "resonance near 8.6 kHz (c~0.47) on clean dark "
                                      "material - same HF/tilt fragility family as the "
                                      "known heuristic low-sens FP. Baseline gap."),
                "problems": [],
            },
        ],
    }
    with open(os.path.join(OUT_DIR, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"Wrote 3 fixtures + manifest.json into {os.path.abspath(OUT_DIR)}")


if __name__ == "__main__":
    main()
