"""M7 Phase-3 diagnostic: WHY does a class fail to be product-emitted?

For every real clip (01-08) and the vocal triplet, per class:
  - raw probability p50/p90/p99 over frames
  - product-emitted occupancy (threshold + margin + top-2, the MLEngine rule)
  - top-2 presence (fraction of frames where the class ranks top-2 by raw prob)
  - LOSS ATTRIBUTION on frames where raw > threshold but NOT emitted:
      lost@margin  : (prob - thr) < margin
      lost@rank    : cleared margin but not in the frame's top-2
  - lost@threshold: frames where raw <= threshold at all (recall gap)

This separates "data/loss problem" (raw never rises: lost@threshold) from
"competition problem" (raw fine, loses top-2: lost@rank) from "softness"
(sits within the margin band: lost@margin) — each points at a DIFFERENT lever.

Usage:
    python3 -m ml.diag_emission --blob /tmp/aieq_m6/sweep_pw12_h4_sn8.bin
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

from . import blob_io
from .dataset import load_wav_mono
from .eval import BASE_THRESHOLDS
from .eval_realclips import DEFAULT_CLIPS, EMISSION_MARGINS, EMISSION_TOPK
from .features import AnalysisPipeline, db_frame_to_linear, features_v1
from .model import problem_names

VOCALS = [
    ("V-SIB  test_voce_SIBILANTE", "/Users/marco/Desktop/test_voce_SIBILANTE.wav", "Sibilance"),
    ("V-CLN  test_voce_pulita", "/Users/marco/Desktop/test_voce_pulita_femmina.wav", None),
    ("V-RES  test_voce_RISONANTE", "/Users/marco/Desktop/test_voce_RISONANTE.wav", None),
]

CLIP_TARGET = {"01": None, "02": "Boominess", "03": "Muddiness",
               "04": "BoxyMidrange", "05": "Harshness", "06": "DullSound",
               "07": "Thinness", "08": "Resonance"}


def analyze(net, wav: Path):
    audio, sr = load_wav_mono(wav)
    frames = AnalysisPipeline(sr).analyze(audio)
    x = np.stack([features_v1(db_frame_to_linear(f), sr) for f in frames])
    return net.forward_problem(x)


def report(probs: np.ndarray, names, label: str, target: str | None) -> None:
    n = probs.shape[0]
    thr = BASE_THRESHOLDS[None, :]
    over_thr = probs > thr
    over_margin = over_thr & ((probs - thr) >= EMISSION_MARGINS[None, :])
    order = np.argsort(-probs, axis=1)
    top = np.zeros_like(over_thr)
    top[np.arange(n)[:, None], order[:, :EMISSION_TOPK]] = True
    emitted = over_margin & top

    print(f"\n== {label}  ({n} frames"
          + (f", target={target}" if target else ", target=none") + ") ==")
    print("  class         | p50   p90   p99  | top2% | emit% | lost@thr  @margin  @rank")
    for c, name in enumerate(names):
        p = probs[:, c]
        p50, p90, p99 = np.percentile(p, (50, 90, 99))
        if p90 < 0.05 and name != target:
            continue  # quiet class, not the target -> skip row
        n_thr = int(over_thr[:, c].sum())
        lost_margin = int((over_thr[:, c] & ~over_margin[:, c]).sum())
        lost_rank = int((over_margin[:, c] & ~top[:, c]).sum())
        mark = " <== TARGET" if name == target else ""
        print(f"  {name:<13} | {p50:.2f}  {p50 if False else p90:.2f}  {p99:.2f} "
              f"| {top[:, c].mean():5.1%} | {emitted[:, c].mean():5.1%} "
              f"| {(n - n_thr):4d}     {lost_margin:4d}     {lost_rank:4d}{mark}")
    # top-2 composition: which classes hog the two emission slots
    flat = order[:, :EMISSION_TOPK].ravel()
    counts = np.bincount(flat, minlength=len(names))
    hog = sorted(((counts[c], names[c]) for c in range(len(names))), reverse=True)
    hogs = ", ".join(f"{nm} {ct / (n * EMISSION_TOPK):.0%}" for ct, nm in hog[:4] if ct)
    print(f"  top-2 slots held by: {hogs}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--blob", required=True)
    ap.add_argument("--clips", default=DEFAULT_CLIPS)
    args = ap.parse_args()

    net, fv, prov = blob_io.read_v2(Path(args.blob))
    schema = prov.get("problem_schema", "legacy-v1")
    names = problem_names(schema)
    print(f"blob={args.blob}\nschema={schema} feature_v{fv} seed={prov.get('seed')}")

    for wav in sorted(Path(args.clips).glob("*.wav")):
        num = wav.name[:2]
        if num not in CLIP_TARGET:
            continue
        report(analyze(net, wav), names, f"clip {num} {wav.name[3:35]}",
               CLIP_TARGET[num])
    for label, path, target in VOCALS:
        report(analyze(net, Path(path)), names, label, target)


if __name__ == "__main__":
    main()
