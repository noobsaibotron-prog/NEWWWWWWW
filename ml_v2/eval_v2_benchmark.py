#!/usr/bin/env python3
"""Motore v2 — A6 benchmark scaffold for RTNeural JSON candidates.

Evaluates a Motore v2 JSON model against:
  - the 8 Ableton judge clips;
  - the vocal triplet/quartet probes on the Desktop;
  - a deterministic clean electronic set from A1 MANIFEST_V2 test split.

This script is intentionally Python-only and does not touch Resources/Models,
runtime C++, APVTS, presets, or the shipped MLP. It is useful before A4 exists:
run it against the A3 random fixture to verify the evaluator plumbing; run it
against /tmp/aieq_v2 candidates once A4 starts producing trained JSON.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import struct
import sys
from pathlib import Path

import numpy as np

from .feature import N_MELS, logmel_frames
from .model import OUT, T_WINDOW

PROBLEM_NAMES = (
    "Resonance", "Harshness", "Muddiness", "Sibilance",
    "Boominess", "Thinness", "BoxyMidrange", "DullSound",
)
NAME_TO_IDX = {name: i for i, name in enumerate(PROBLEM_NAMES)}

DEFAULT_DATA_ROOT = Path.home() / "aieq_data"
DEFAULT_CLIPS = Path.home() / "Desktop" / "AIEQ_Ableton_Test_Clips" / "processed"
DEFAULT_MANIFEST = Path(__file__).resolve().parent / "data" / "MANIFEST_V2.csv"

MIN_OCC = 0.05
CLEAN_TARGET = 0.05
CLEAN_HARD_MAX = 0.10
SIB_CAP = 0.05
ELECTRONIC_CLEAN_MAX = 0.10
NEUTRAL_EXCESS_DB = 6.0
NEUTRAL_MAX_HOT_FRAC = 0.50

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

VOCAL_PROBES = (
    ("V-SIB", Path.home() / "Desktop" / "test_voce_SIBILANTE.wav", "Sibilance"),
    ("V-CLN-F", Path.home() / "Desktop" / "test_voce_pulita_femmina.wav", None),
    ("V-CLN-M", Path.home() / "Desktop" / "test_voce_pulita_maschio.wav", None),
    ("V-RES", Path.home() / "Desktop" / "test_voce_RISONANTE.wav", None),
)


def sigmoid(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=np.float64)
    return np.where(x >= 0.0, 1.0 / (1.0 + np.exp(-x)),
                    np.exp(x) / (1.0 + np.exp(x)))


def _read_wav_chunks(path: Path) -> tuple[int, int, int, int, bytes]:
    with open(path, "rb") as f:
        header = f.read(12)
        if len(header) < 12 or header[:4] != b"RIFF" or header[8:12] != b"WAVE":
            raise ValueError(f"not a RIFF/WAVE file: {path}")
        fmt_tag = channels = sr = bits = 0
        data = b""
        while True:
            chunk = f.read(8)
            if len(chunk) < 8:
                break
            cid, size = chunk[:4], struct.unpack("<I", chunk[4:])[0]
            payload = f.read(size)
            if size & 1:
                f.read(1)
            if cid == b"fmt ":
                fmt_tag, channels, sr, _byte_rate, _block_align, bits = struct.unpack(
                    "<HHIIHH", payload[:16])
                if fmt_tag == 0xFFFE and size >= 40:
                    fmt_tag = struct.unpack("<H", payload[24:26])[0]
            elif cid == b"data":
                data = payload
        if not data or not sr or not channels or not bits:
            raise ValueError(f"incomplete WAV chunks: {path}")
        return fmt_tag, channels, sr, bits, data


def load_wav_mono(path: Path) -> tuple[np.ndarray, int]:
    """Load PCM/float WAV into mono float32 [-1, 1-ish]."""
    fmt_tag, channels, sr, bits, data = _read_wav_chunks(path)
    if fmt_tag == 3 and bits == 32:
        arr = np.frombuffer(data, dtype="<f4").astype(np.float32)
    elif fmt_tag == 1 and bits == 16:
        arr = (np.frombuffer(data, dtype="<i2").astype(np.float32) / 32768.0)
    elif fmt_tag == 1 and bits == 24:
        raw = np.frombuffer(data, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        vals = raw[:, 0] | (raw[:, 1] << 8) | (raw[:, 2] << 16)
        vals = np.where(vals & 0x800000, vals - 0x1000000, vals)
        arr = vals.astype(np.float32) / 8388608.0
    elif fmt_tag == 1 and bits == 32:
        arr = (np.frombuffer(data, dtype="<i4").astype(np.float32) / 2147483648.0)
    else:
        raise ValueError(f"unsupported WAV format tag={fmt_tag} bits={bits}: {path}")
    if channels > 1:
        arr = arr.reshape(-1, channels).mean(axis=1)
    return np.asarray(arr, dtype=np.float32), sr


class RtNeuralJsonModel:
    def __init__(self, path: Path):
        data = json.loads(path.read_text())
        self.layers = data["layers"]

    def forward_window(self, window_tm: np.ndarray) -> np.ndarray:
        h = np.asarray(window_tm, dtype=np.float32).T  # [channels, time]
        vec: np.ndarray | None = None
        for layer in self.layers:
            typ = layer["type"].lower()
            weights, bias = layer["weights"]
            act = layer.get("activation", "")
            if typ == "conv1d":
                w = np.asarray(weights, dtype=np.float32)  # [kernel, in, out]
                b = np.asarray(bias, dtype=np.float32)
                k = int(layer["kernel_size"][0])
                d = int(layer.get("dilation", [1])[0])
                out_len = h.shape[1] - (k - 1) * d
                if out_len <= 0:
                    raise ValueError("window is shorter than model receptive field")
                y = np.empty((w.shape[2], out_len), dtype=np.float32)
                for t in range(out_len):
                    acc = b.copy()
                    for kk in range(k):
                        acc += h[:, t + kk * d] @ w[kk]
                    y[:, t] = acc
                h = np.maximum(y, 0.0) if act.lower() == "relu" else y
                vec = None
            elif typ == "dense":
                w = np.asarray(weights, dtype=np.float32)  # [in, out]
                b = np.asarray(bias, dtype=np.float32)
                if vec is None:
                    vec = h[:, -1]
                vec = vec @ w + b
                if act.lower() == "relu":
                    vec = np.maximum(vec, 0.0)
            else:
                raise ValueError(f"unsupported RTNeural layer: {typ}")
        if vec is None or vec.shape[0] != OUT:
            raise ValueError(f"model output shape mismatch: {None if vec is None else vec.shape}")
        return vec.astype(np.float32)


def windows_for_wav(path: Path, step: int) -> np.ndarray:
    audio, sr = load_wav_mono(path)
    frames = logmel_frames(audio, sr)
    if frames.shape[0] < T_WINDOW:
        return np.zeros((0, T_WINDOW, N_MELS), dtype=np.float32)
    return np.stack([frames[i:i + T_WINDOW]
                     for i in range(0, frames.shape[0] - T_WINDOW + 1, step)]).astype(np.float32)


def occupancy(model: RtNeuralJsonModel, path: Path, thresholds: np.ndarray,
              step: int) -> tuple[dict[str, float], int]:
    wins = windows_for_wav(path, step)
    if wins.shape[0] == 0:
        return {name: 0.0 for name in PROBLEM_NAMES}, 0
    logits = np.stack([model.forward_window(w) for w in wins])
    probs = sigmoid(logits[:, :len(PROBLEM_NAMES)])
    hits = probs > thresholds[None, :]
    return {name: float(hits[:, i].mean()) for i, name in enumerate(PROBLEM_NAMES)}, wins.shape[0]


def detail(occ: dict[str, float]) -> str:
    hot = [(name, value) for name, value in occ.items() if value > 0.0]
    hot.sort(key=lambda item: -item[1])
    return "; ".join(f"{name} {value:5.1%}" for name, value in hot) if hot else "(none)"


def check_core(num: str, occ: dict[str, float]) -> tuple[bool, list[str]]:
    reasons: list[str] = []
    if num == "01":
        for name, value in occ.items():
            if value > CLEAN_TARGET:
                reasons.append(f"{name} {value:.1%} > {CLEAN_TARGET:.0%}")
            if value > CLEAN_HARD_MAX:
                reasons.append(f"HARD-CLEAN {name} {value:.1%} > {CLEAN_HARD_MAX:.0%}")
    expected = CORE_TARGETS[num]
    if expected:
        got = max(occ.get(name, 0.0) for name in expected)
        if got < MIN_OCC:
            reasons.append(f"{'/'.join(expected)} {got:.1%} < {MIN_OCC:.0%}")
    if num == "05" and occ.get("Sibilance", 0.0) > SIB_CAP:
        reasons.append(f"Sibilance {occ['Sibilance']:.1%} > {SIB_CAP:.0%}")
    if num == "07" and occ.get("Harshness", 0.0) >= occ.get("Thinness", 0.0):
        reasons.append(f"Harshness {occ.get('Harshness', 0.0):.1%} >= "
                       f"Thinness {occ.get('Thinness', 0.0):.1%}")
    return not reasons, reasons


def check_vocal(target: str | None, occ: dict[str, float]) -> tuple[bool, list[str]]:
    sib = occ.get("Sibilance", 0.0)
    if target == "Sibilance":
        return sib >= MIN_OCC, ([] if sib >= MIN_OCC else [f"Sibilance {sib:.1%} < {MIN_OCC:.0%}"])
    return sib <= SIB_CAP, ([] if sib <= SIB_CAP else [f"Sibilance {sib:.1%} > {SIB_CAP:.0%}"])


def neutrality_reason(path: Path) -> str | None:
    audio, sr = load_wav_mono(path)
    frames = logmel_frames(audio, sr)
    if frames.shape[0] == 0:
        return "no analysable frames"
    mel_db = -100.0 * frames.astype(np.float64)
    worst: tuple[int, float] | None = None
    for b in range(3, mel_db.shape[1] - 3):
        neighbours = np.concatenate([mel_db[:, b - 3:b],
                                     mel_db[:, b + 1:b + 4]], axis=1)
        excess = mel_db[:, b] - neighbours.mean(axis=1)
        hot_frac = float(np.mean(excess > NEUTRAL_EXCESS_DB))
        if worst is None or hot_frac > worst[1]:
            worst = (b, hot_frac)
        if hot_frac > NEUTRAL_MAX_HOT_FRAC:
            return (f"mel band {b} > adjacent mean by {NEUTRAL_EXCESS_DB:.0f} dB "
                    f"for {hot_frac:.1%} frames")
    return None


def select_electronic(manifest: Path, data_root: Path, per_domain: int
                      ) -> tuple[dict[str, list[Path]],
                                 dict[str, list[Path]],
                                 dict[str, list[tuple[Path, str]]]]:
    domains = ("clean_drums", "clean_synth", "clean_bass", "clean_mix", "hf_negative")
    raw: dict[str, list[Path]] = {}
    neutral: dict[str, list[Path]] = {}
    excluded: dict[str, list[tuple[Path, str]]] = {}
    rows = list(csv.DictReader(manifest.open(newline="")))
    for domain in domains:
        picks = [r for r in rows if r["domain"] == domain
                 and r["split"] == "test" and r["hf_dead"] == "0"]
        picks.sort(key=lambda r: r["sha256"])
        raw[domain] = [data_root / r["path"] for r in picks[:per_domain]]
        neutral[domain] = []
        excluded[domain] = []
        for r in picks:
            path = data_root / r["path"]
            reason = neutrality_reason(path)
            if reason is None:
                neutral[domain].append(path)
                if len(neutral[domain]) >= per_domain:
                    break
            else:
                excluded[domain].append((path, reason))
    return raw, neutral, excluded


def load_thresholds(args: argparse.Namespace) -> np.ndarray:
    if args.thresholds:
        vals = [float(x) for x in args.thresholds.split(",")]
        if len(vals) != len(PROBLEM_NAMES):
            raise SystemExit("--thresholds must contain 8 comma-separated floats")
        return np.asarray(vals, dtype=np.float64)
    if args.metadata:
        meta = json.loads(Path(args.metadata).read_text())
        vals = meta.get("class_thresholds")
        if vals is not None and len(vals) == len(PROBLEM_NAMES):
            return np.asarray(vals, dtype=np.float64)
    return np.full(len(PROBLEM_NAMES), 0.5, dtype=np.float64)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--model", required=True, help="RTNeural JSON model")
    ap.add_argument("--metadata", default="", help="optional provenance JSON with class_thresholds")
    ap.add_argument("--thresholds", default="", help="8 comma-separated thresholds; overrides metadata")
    ap.add_argument("--clips", default=str(DEFAULT_CLIPS))
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    ap.add_argument("--data-root", default=str(DEFAULT_DATA_ROOT))
    ap.add_argument("--window-step", type=int, default=16)
    ap.add_argument("--electronic-per-domain", type=int, default=10)
    ap.add_argument("--no-fail-exit", action="store_true")
    args = ap.parse_args()

    model = RtNeuralJsonModel(Path(args.model))
    thresholds = load_thresholds(args)
    fails: list[str] = []

    print("\n== V2 BENCHMARK ==")
    print(f"model={args.model}")
    print("thresholds=" + ",".join(f"{x:.3f}" for x in thresholds))

    print("\n[ABLETON CORE]")
    for wav in sorted(Path(args.clips).glob("*.wav")):
        num = wav.name[:2]
        if num not in CORE_TARGETS:
            continue
        occ, n = occupancy(model, wav, thresholds, args.window_step)
        ok, reasons = check_core(num, occ)
        if not ok:
            fails.append(num)
        verdict = "PASS" if ok else "FAIL(" + "; ".join(reasons) + ")"
        print(f"  {num} [{verdict:>9s}] ({n:4d} win) {detail(occ)}")

    print("\n[VOCAL PROBES]")
    for label, wav, target in VOCAL_PROBES:
        if not wav.exists():
            print(f"  {label} [  MISSING] {wav}")
            fails.append(label)
            continue
        occ, n = occupancy(model, wav, thresholds, args.window_step)
        ok, reasons = check_vocal(target, occ)
        if not ok:
            fails.append(label)
        verdict = "PASS" if ok else "FAIL(" + "; ".join(reasons) + ")"
        print(f"  {label} [{verdict:>9s}] ({n:4d} win) {detail(occ)}")

    print("\n[ELECTRONIC RAW SET - diagnostic only]")
    electronic_raw, electronic_neutral, electronic_excluded = select_electronic(
        Path(args.manifest), Path(args.data_root), args.electronic_per_domain)
    for domain, paths in electronic_raw.items():
        bad: list[str] = []
        for path in paths:
            occ, _n = occupancy(model, path, thresholds, args.window_step)
            hot = [(name, value) for name, value in occ.items()
                   if value > ELECTRONIC_CLEAN_MAX]
            if hot:
                hot.sort(key=lambda item: -item[1])
                bad.append(f"{path.name} {hot[0][0]} {hot[0][1]:.1%}")
        verdict = "PASS" if not bad else "REPORT(" + "; ".join(bad[:5]) + ")"
        print(f"  {domain:<12s} [{verdict:>9s}] ({len(paths)} files)")

    print("\n[ELECTRONIC NEUTRAL SET - gate]")
    for domain, excluded in electronic_excluded.items():
        for path, reason in excluded:
            print(f"  exclude {domain:<12s} {path.name}: {reason}")

    for domain, paths in electronic_neutral.items():
        if len(paths) < args.electronic_per_domain:
            fails.append(domain)
            print(f"  {domain:<12s} [FAIL(insufficient neutral files "
                  f"{len(paths)}/{args.electronic_per_domain})]")
            continue
        bad: list[str] = []
        for path in paths:
            occ, _n = occupancy(model, path, thresholds, args.window_step)
            hot = [(name, value) for name, value in occ.items()
                   if value > ELECTRONIC_CLEAN_MAX]
            if hot:
                hot.sort(key=lambda item: -item[1])
                bad.append(f"{path.name} {hot[0][0]} {hot[0][1]:.1%}")
        clean_count = len(paths) - len(bad)
        if clean_count < 7:
            fails.append(domain)
        verdict = "PASS" if clean_count >= 7 else \
            "FAIL(" + "; ".join(bad[:5]) + ")"
        print(f"  {domain:<12s} [{verdict:>9s}] "
              f"({clean_count}/{len(paths)} clean files)")

    print(f"\n  RESULT: {'PASS' if not fails else 'FAIL'}"
          f"  fails={fails if fails else 'none'}")
    return 0 if (not fails or args.no_fail_exit) else 1


if __name__ == "__main__":
    sys.exit(main())
