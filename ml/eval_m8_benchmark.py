"""M8 benchmark gate: Ableton real clips + vocal triplet.

Run:
    python3 -m ml.eval_m8_benchmark --blob /tmp/aieq_m8/candidate.bin

This is a lab gate. v3 blobs use the M8 presence+per-class thresholds;
legacy v1/v2 blobs use the product-faithful margin+top-K rule for comparison.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

from . import blob_io
from .dataset import load_wav_mono
from .eval import BASE_THRESHOLDS
from .eval_realclips import DEFAULT_CLIPS, EMISSION_MARGINS, EMISSION_TOPK
from .features import AnalysisPipeline, FEATURE_FNS_ALL, db_frame_to_linear
from .model import NUM_PROBLEMS, problem_names

VOCALS = [
    ("V-SIB", Path("/Users/marco/Desktop/test_voce_SIBILANTE.wav"), "Sibilance"),
    ("V-CLN", Path("/Users/marco/Desktop/test_voce_pulita_femmina.wav"), None),
    ("V-RES", Path("/Users/marco/Desktop/test_voce_RISONANTE.wav"), None),
]

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

MIN_OCC = 0.05
CLEAN_TARGET = 0.05
CLEAN_HARD_MAX = 0.10
SIB_CAP = 0.05


def _features_for_wav(path: Path, feature_version: int) -> np.ndarray:
    audio, sr = load_wav_mono(path)
    frames = AnalysisPipeline(sr).analyze(audio)
    if not frames:
        return np.zeros((0, 64), dtype=np.float64)
    if feature_version == 5:
        # M8 temporal lab: sliding WINDOWS; occupancy becomes a fraction of
        # windows instead of frames (same [0,1] semantics for every gate).
        from .temporal import V5_DIM, features_v5, windows_of
        wins = windows_of(frames)
        if not wins:
            return np.zeros((0, V5_DIM), dtype=np.float64)
        return np.stack([features_v5(w, sr) for w in wins])
    try:
        feature_fn = FEATURE_FNS_ALL[feature_version]
    except KeyError as exc:
        raise SystemExit(f"unsupported feature_version={feature_version}") from exc
    return np.stack([feature_fn(db_frame_to_linear(frame), sr)
                     for frame in frames])


def _read_blob(path: Path):
    try:
        net, feature_version, provenance = blob_io.read_v3(path)
        return net, feature_version, provenance, "m8-v3"
    except ValueError:
        pass
    try:
        net, feature_version, provenance = blob_io.read_v2(path)
        return net, feature_version, provenance, "legacy-v2"
    except ValueError:
        pass
    net = blob_io.read_v1(path)
    return net, 1, {}, "legacy-v1"


def _occupancy(net, kind: str, x: np.ndarray, ui_cap: int) -> dict[int, float]:
    if x.size == 0:
        return {}
    if kind == "m8-v3":
        hits = net.emitted(x, ui_cap=ui_cap)
    else:
        probs = net.forward_problem(x)
        hits = probs > BASE_THRESHOLDS[None, :]
        hits &= (probs - BASE_THRESHOLDS[None, :]) >= EMISSION_MARGINS[None, :]
        order = np.argsort(-probs, axis=1)
        top = np.zeros_like(hits)
        top[np.arange(probs.shape[0])[:, None], order[:, :EMISSION_TOPK]] = True
        hits &= top
    return {c: float(hits[:, c].mean()) for c in range(NUM_PROBLEMS)}


def _detail(names: tuple[str, ...], occ: dict[int, float]) -> str:
    parts = [f"{names[c]} {occ[c]:5.1%}" for c in
             sorted(occ, key=lambda i: -occ[i]) if occ[c] > 0.0]
    return "; ".join(parts) if parts else "(none)"


def _check_clip(num: str, names: tuple[str, ...], occ: dict[int, float]) -> tuple[bool, list[str]]:
    idx = {name: i for i, name in enumerate(names)}
    reasons: list[str] = []

    if num == "01":
        hot = [(names[c], v) for c, v in occ.items() if v > CLEAN_TARGET]
        if hot:
            reasons.extend(f"{name} {value:.1%} > {CLEAN_TARGET:.0%}"
                           for name, value in hot)
        hard = [(names[c], v) for c, v in occ.items() if v > CLEAN_HARD_MAX]
        if hard:
            reasons.extend(f"HARD-CLEAN {name} {value:.1%} > {CLEAN_HARD_MAX:.0%}"
                           for name, value in hard)

    expected = CORE_TARGETS[num]
    if expected:
        got = max((occ.get(idx[name], 0.0) for name in expected if name in idx),
                  default=0.0)
        if got < MIN_OCC:
            reasons.append(f"{'/'.join(expected)} {got:.1%} < {MIN_OCC:.0%}")

    if num == "05" and "Sibilance" in idx:
        sib = occ.get(idx["Sibilance"], 0.0)
        if sib > SIB_CAP:
            reasons.append(f"Sibilance {sib:.1%} > {SIB_CAP:.0%}")

    if num == "07" and {"Thinness", "Harshness"} <= set(idx):
        thin = occ.get(idx["Thinness"], 0.0)
        harsh = occ.get(idx["Harshness"], 0.0)
        if harsh >= thin:
            reasons.append(f"Harshness {harsh:.1%} >= Thinness {thin:.1%}")

    return not reasons, reasons


def _check_vocal(label: str, target: str | None,
                 names: tuple[str, ...], occ: dict[int, float]) -> tuple[bool, list[str]]:
    idx = {name: i for i, name in enumerate(names)}
    reasons: list[str] = []
    sib = occ.get(idx.get("Sibilance", -1), 0.0)
    if target == "Sibilance":
        if sib < MIN_OCC:
            reasons.append(f"Sibilance {sib:.1%} < {MIN_OCC:.0%}")
    else:
        if sib > SIB_CAP:
            reasons.append(f"Sibilance {sib:.1%} > {SIB_CAP:.0%}")
    return not reasons, reasons


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--blob", required=True)
    ap.add_argument("--clips", default=DEFAULT_CLIPS)
    ap.add_argument("--schema", default="")
    ap.add_argument("--ui-cap", type=int, default=3)
    ap.add_argument("--no-fail-exit", action="store_true")
    args = ap.parse_args()

    net, feature_version, provenance, kind = _read_blob(Path(args.blob))
    schema = args.schema or provenance.get("problem_schema", "legacy-v1")
    names = problem_names(schema)
    print(f"feature_version={feature_version}")
    print(f"\n== M8 BENCHMARK (kind={kind}, schema={schema}) ==")
    print(f"blob={args.blob}")

    fails: list[str] = []
    for wav in sorted(Path(args.clips).glob("*.wav")):
        num = wav.name[:2]
        if num not in CORE_TARGETS:
            continue
        x = _features_for_wav(wav, feature_version)
        occ = _occupancy(net, kind, x, args.ui_cap)
        ok, reasons = _check_clip(num, names, occ)
        verdict = "PASS" if ok else "FAIL(" + "; ".join(reasons) + ")"
        if not ok:
            fails.append(num)
        print(f"  {num} [{verdict:>9s}] ({x.shape[0]:3d} fr) {_detail(names, occ)}")

    for label, path, target in VOCALS:
        x = _features_for_wav(path, feature_version)
        occ = _occupancy(net, kind, x, args.ui_cap)
        ok, reasons = _check_vocal(label, target, names, occ)
        verdict = "PASS" if ok else "FAIL(" + "; ".join(reasons) + ")"
        if not ok:
            fails.append(label)
        print(f"  {label} [{verdict:>9s}] ({x.shape[0]:3d} fr) {_detail(names, occ)}")

    print(f"\n  RESULT: {'PASS' if not fails else 'FAIL'}"
          f"  fails={fails if fails else 'none'}")
    return 0 if (not fails or args.no_fail_exit) else 1


if __name__ == "__main__":
    sys.exit(main())
