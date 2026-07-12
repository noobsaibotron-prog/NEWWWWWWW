#!/usr/bin/env python3
"""Motore v2 — A6 benchmark (Manus spec, implementation counter-reviewed).

Evaluates a v2 candidate (RTNeural JSON) against:
  - Ableton Core (8 judge clips)
  - Vocal Triplet (V-SIB / V-CLN / V-RES, Desktop)
  - Electronic Set (30 tier2 clips — drawn from the A1 manifest TEST split,
    so training disjointness is guaranteed by construction, then seed-fixed)

Differences vs the spec's draft script, each verified against the certified
contracts (receipts in m9_handoff_manus/ALIGNMENT_A4_REPORT_CLAUDE.md):
  1. Class order = product-v2 canonical [Resonance, Harshness, Muddiness,
     Sibilance, Boominess, Thinness, BoxyMidrange, DullSound] (C++ enum
     MLEngine.h:34 == lab ml/model.py SCHEMAS) — the draft's order was wrong.
  2. Features via ml_v2.feature (the A2 parity-locked extractor), not a
     re-implementation (draft diverged: sr/2 vs min(20k, sr/2), count quirk).
  3. Model forward via ml_v2.rtneural_numpy (weights=[kernel,bias] list,
     kernel_size/dilation arrays, dense [in][out]) — self-checked at 1e-16
     on the A3 fixture; the draft's parser could not load the real format.
  4. Electronic set from MANIFEST_V2 split=='test' rows only.

Run:
    python3 -m ml_v2.eval_benchmark --model /tmp/aieq_v2/candidate_s42.json \
        [--provenance /tmp/aieq_v2/candidate_s42.provenance.json] \
        [--thresholds 0.5,...x8] [--step 16]
"""
from __future__ import annotations

import argparse
import csv
import json
import struct
import sys
from pathlib import Path

import numpy as np

from .feature import logmel_frames
from .rtneural_numpy import RTNeuralNumpyModel

# ── canonical product-v2 class order (MLEngine.h:34 == ml/model.py SCHEMAS) ──
PROBLEM_NAMES = ("Resonance", "Harshness", "Muddiness", "Sibilance",
                 "Boominess", "Thinness", "BoxyMidrange", "DullSound")
NUM_CLASSES = 8
T_WINDOW = 32

# Benchmark criteria (M8 lineage, per the A6 spec)
MIN_OCC = 0.05
CLEAN_TARGET = 0.05
CLEAN_HARD_MAX = 0.10
SIB_CAP = 0.05
ELECTRONIC_CLEAN_MAX = 0.10

CORE_TARGETS = {
    "01": (),
    "02": ("Boominess", "Muddiness"),
    "03": ("Muddiness", "Boominess"),
    "04": ("BoxyMidrange",),
    "05": ("Harshness",),
    "06": ("DullSound",),
    "07": ("Thinness",),
    "08": ("Resonance",),
}

VOCAL_CLIPS = [
    ("V-SIB", Path.home() / "Desktop" / "test_voce_SIBILANTE.wav", "Sibilance"),
    ("V-CLN", Path.home() / "Desktop" / "test_voce_pulita_femmina.wav", None),
    ("V-RES", Path.home() / "Desktop" / "test_voce_RISONANTE.wav", None),
]

ELECTRONIC_SEED = 42
ELECTRONIC_N = 10
ELECTRONIC_DOMAINS = ("clean_drums", "clean_synth", "clean_bass")

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "data" / "MANIFEST_V2.csv"
DATA_ROOT_DEFAULT = Path.home() / "aieq_data"


# ─────────────────────────────────────────────────────────────── wav loading
def load_wav_mono(path: Path) -> tuple[np.ndarray, int]:
    """RIFF parser: PCM 16/24/32-bit + IEEE float32, downmixed to mono f32."""
    raw = path.read_bytes()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError(f"not a RIFF/WAVE file: {path}")
    pos, fmt = 12, None
    data = b""
    while pos + 8 <= len(raw):
        cid = raw[pos:pos + 4]
        csize = struct.unpack("<I", raw[pos + 4:pos + 8])[0]
        body = raw[pos + 8:pos + 8 + csize]
        if cid == b"fmt ":
            tag, nch, sr, _br, _ba, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE and csize >= 40:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, nch, sr, bits)
        elif cid == b"data":
            data = body
        pos += 8 + csize + (csize & 1)
    if fmt is None:
        raise ValueError(f"no fmt chunk: {path}")
    tag, nch, sr, bits = fmt

    if tag == 3 and bits == 32:
        x = np.frombuffer(data, dtype="<f4").astype(np.float32)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(data, dtype="<i2").astype(np.float32) / 32768.0
    elif tag == 1 and bits == 24:
        b3 = np.frombuffer(data, dtype=np.uint8).reshape(-1, 3)
        val = (b3[:, 0].astype(np.int32) | (b3[:, 1].astype(np.int32) << 8)
               | (b3[:, 2].astype(np.int32) << 16))
        val = np.where(val & 0x800000, val - 0x1000000, val)
        x = val.astype(np.float32) / 8388608.0
    elif tag == 1 and bits == 32:
        x = np.frombuffer(data, dtype="<i4").astype(np.float32) / 2147483648.0
    else:
        raise ValueError(f"unsupported wav format tag={tag} bits={bits}: {path}")

    if nch > 1:
        x = x[: (x.size // nch) * nch].reshape(-1, nch).mean(axis=1)
    return x.astype(np.float32), int(sr)


# ─────────────────────────────────────────────────────────────── occupancy
def windows_of(frames: np.ndarray, step: int) -> np.ndarray:
    n = frames.shape[0]
    if n < T_WINDOW:
        return np.zeros((0, T_WINDOW, frames.shape[1]), dtype=frames.dtype)
    return np.stack([frames[s:s + T_WINDOW]
                     for s in range(0, n - T_WINDOW + 1, step)])


def occupancy(model: RTNeuralNumpyModel, audio: np.ndarray, sr: int,
              thresholds: np.ndarray, step: int) -> tuple[dict[int, float], int]:
    frames = logmel_frames(audio, sr)
    wins = windows_of(frames, step)
    if wins.shape[0] == 0:
        return {c: 0.0 for c in range(NUM_CLASSES)}, 0
    hits = np.zeros(NUM_CLASSES)
    for w in wins:
        logits = model.forward_window(w)
        probs = 1.0 / (1.0 + np.exp(-logits[:NUM_CLASSES]))
        hits += (probs > thresholds).astype(float)
    return {c: float(hits[c] / wins.shape[0]) for c in range(NUM_CLASSES)}, wins.shape[0]


IDX = {name: i for i, name in enumerate(PROBLEM_NAMES)}


def detail(occ: dict[int, float]) -> str:
    parts = [f"{PROBLEM_NAMES[c]} {occ[c]:5.1%}"
             for c in sorted(occ, key=lambda i: -occ[i]) if occ[c] > 0.0]
    return "; ".join(parts) if parts else "(none)"


def check_core(num: str, occ: dict[int, float]) -> list[str]:
    reasons = []
    if num == "01":
        for c, v in occ.items():
            if v > CLEAN_HARD_MAX:
                reasons.append(f"HARD-CLEAN {PROBLEM_NAMES[c]} {v:.1%}")
            elif v > CLEAN_TARGET:
                reasons.append(f"{PROBLEM_NAMES[c]} {v:.1%} > {CLEAN_TARGET:.0%}")
    else:
        expected = CORE_TARGETS.get(num, ())
        if expected:
            got = max(occ[IDX[n]] for n in expected)
            if got < MIN_OCC:
                reasons.append(f"{'/'.join(expected)} {got:.1%} < {MIN_OCC:.0%}")
    if num == "05" and occ[IDX["Sibilance"]] > SIB_CAP:
        reasons.append(f"Sibilance {occ[IDX['Sibilance']]:.1%} > {SIB_CAP:.0%}")
    if num == "07" and occ[IDX["Harshness"]] >= occ[IDX["Thinness"]]:
        reasons.append(f"Harshness {occ[IDX['Harshness']]:.1%} >= "
                       f"Thinness {occ[IDX['Thinness']]:.1%}")
    return reasons


def check_vocal(target: str | None, occ: dict[int, float]) -> list[str]:
    sib = occ[IDX["Sibilance"]]
    if target == "Sibilance":
        return [] if sib >= MIN_OCC else [f"Sibilance {sib:.1%} < {MIN_OCC:.0%}"]
    return [] if sib <= SIB_CAP else [f"Sibilance {sib:.1%} > {SIB_CAP:.0%}"]


def electronic_clips(data_root: Path) -> dict[str, list[Path]]:
    """10 seed-fixed clips per domain from the manifest TEST split only."""
    rows_by_dom: dict[str, list[str]] = {d: [] for d in ELECTRONIC_DOMAINS}
    with open(MANIFEST, newline="") as f:
        for r in csv.DictReader(f):
            if r["domain"] in rows_by_dom and r["split"] == "test":
                rows_by_dom[r["domain"]].append(r["path"])
    out: dict[str, list[Path]] = {}
    rng = np.random.default_rng(ELECTRONIC_SEED)
    for dom, paths in rows_by_dom.items():
        paths = sorted(paths)
        if len(paths) > ELECTRONIC_N:
            idx = sorted(rng.choice(len(paths), size=ELECTRONIC_N, replace=False))
            paths = [paths[i] for i in idx]
        out[dom] = [data_root / p for p in paths]
    return out


# ─────────────────────────────────────────────────────────────── main
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--model", required=True)
    ap.add_argument("--provenance", default=None,
                    help="provenance JSON with class_thresholds (A4.5)")
    ap.add_argument("--thresholds", default=None, help="8 comma-separated floats")
    ap.add_argument("--clips", default=str(Path.home() / "Desktop"
                                           / "AIEQ_Ableton_Test_Clips" / "processed"))
    ap.add_argument("--data-root", default=str(DATA_ROOT_DEFAULT))
    ap.add_argument("--step", type=int, default=16, help="window hop in frames")
    ap.add_argument("--no-fail-exit", action="store_true")
    args = ap.parse_args()

    if args.thresholds:
        thresholds = np.array([float(x) for x in args.thresholds.split(",")])
    elif args.provenance:
        with open(args.provenance) as f:
            prov = json.load(f)
        thresholds = np.array(prov["class_thresholds"], dtype=np.float64)
    else:
        thresholds = np.full(NUM_CLASSES, 0.5)
    assert thresholds.shape == (NUM_CLASSES,)

    model = RTNeuralNumpyModel(args.model)
    print("== V2 BENCHMARK ==")
    print(f"model={args.model}")
    print(f"thresholds={[round(float(t), 3) for t in thresholds]}  step={args.step}")

    fails: list[str] = []

    print("\n[ABLETON CORE]")
    for wav in sorted(Path(args.clips).expanduser().glob("*.wav")):
        num = wav.name[:2]
        if num not in CORE_TARGETS:
            continue
        audio, sr = load_wav_mono(wav)
        occ, n_win = occupancy(model, audio, sr, thresholds, args.step)
        reasons = check_core(num, occ)
        if reasons:
            fails.append(num)
        verdict = "PASS" if not reasons else "FAIL " + "; ".join(reasons)
        print(f"  {num} [{'PASS' if not reasons else 'FAIL':>9s}] ({n_win:3d} win) "
              f"{detail(occ)}" + ("" if not reasons else f"  <<{verdict}>>"))

    print("\n[VOCAL TRIPLET]")
    for label, path, target in VOCAL_CLIPS:
        if not path.exists():
            print(f"  {label} [    SKIP ] (missing: {path})")
            continue
        audio, sr = load_wav_mono(path)
        occ, n_win = occupancy(model, audio, sr, thresholds, args.step)
        reasons = check_vocal(target, occ)
        if reasons:
            fails.append(label)
        print(f"  {label} [{'PASS' if not reasons else 'FAIL':>9s}] ({n_win:3d} win) "
              f"{detail(occ)}" + ("" if not reasons else f"  <<{'; '.join(reasons)}>>"))

    print("\n[ELECTRONIC SET] (manifest TEST split, seed 42)")
    for dom, clips in electronic_clips(Path(args.data_root).expanduser()).items():
        clean_n, reasons = 0, []
        for clip in clips:
            audio, sr = load_wav_mono(clip)
            occ, _ = occupancy(model, audio, sr, thresholds, args.step)
            hot = [(PROBLEM_NAMES[c], v) for c, v in occ.items()
                   if v > ELECTRONIC_CLEAN_MAX]
            if hot:
                reasons += [f"{clip.name}: {n} {v:.1%}" for n, v in hot]
            else:
                clean_n += 1
        ok = clean_n == len(clips)
        if not ok:
            fails.append(dom)
        extra = "" if ok else " (" + "; ".join(reasons[:3]) + (
            f" +{len(reasons)-3} more" if len(reasons) > 3 else "") + ")"
        print(f"  {dom} [{'PASS' if ok else 'FAIL':>9s}] "
              f"({clean_n}/{len(clips)} clean){extra}")

    print(f"\n  RESULT: {'PASS' if not fails else 'FAIL'}  "
          f"fails={fails if fails else 'none'}")
    return 0 if (not fails or args.no_fail_exit) else 1


if __name__ == "__main__":
    sys.exit(main())
