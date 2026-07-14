"""M6 real-clip gate: pre-veto model occupancy on the 8 Ableton HOLDOUT clips.

These clips (and their sources/) are EVAL-ONLY — never training material.
This measures the RAW model (pre-veto, pre-persistence): the C++ runtime adds
protective gating on top, which only ever REDUCES detections, so a class that
fails here cannot appear in the product.

Gate (M6 v2, agreed 2026-07-07). occupancy = fraction of frames where the
class prob exceeds its base threshold (sensitivity 0.5 => scale 1.0):
  01 clean drum : every class <= 10%        (hard clean floor)
  02 kick boomy : Boominess or Muddiness >= 5%
  03 bass muddy : Muddiness or Boominess >= 5%
  04 tom boxy   : BoxyMidrange >= 5%
  05 harsh synth: Harshness >= 5%  AND  Sibilance <= 10%
  06 dull pad   : DullSound >= 5%           (product-v2 only)
  07 thin bass  : Thinness >= 5%   AND  Harshness occupancy < Thinness
  08 reso tom   : REPORT-ONLY (expected-fail ML — R1-R4/RA1 receipts)

Usage:
    python3 -m ml.eval_realclips --blob /tmp/aieq_m6/candidate.bin \
        [--clips /Users/marco/Desktop/AIEQ_Ableton_Test_Clips/processed]
Reads v2 blobs (schema from provenance) or v1 blobs (--schema flag then).
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

from . import blob_io
from .dataset import load_wav_mono
from .eval import BASE_THRESHOLDS
from .features import AnalysisPipeline, db_frame_to_linear, features_v1
from .model import EQNet, NUM_PROBLEMS, problem_names

DEFAULT_CLIPS = "/Users/marco/Desktop/AIEQ_Ableton_Test_Clips/processed"

# PRODUCT-FAITHFUL emission (MLEngine::detectProblems @ sensitivity 0.5,
# GenreType::Unknown): a class is EMITTED on a frame only if
#   prob > threshold  AND  (prob - threshold) >= margin  AND  top-K rank by
# raw prob (K=2 at default sensitivity), then capped to K detections.
# The raw mode (prob > threshold) measures model recall only — it is NOT what
# the plugin emits; the C++ cross-check (2026-07-07) proved a raw-gate pass
# can collapse to near-zero through this rule when classes co-fire.
EMISSION_MARGINS = np.array([0.02, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10])
EMISSION_TOPK = 2

# clip-number -> (expected classes (any-of), hard sibilance cap or None)
GATE = {
    "01": ((), 0.10),                       # clean: quiet; Sib <= 10%
    "02": (("Boominess", "Muddiness"), None),
    "03": (("Muddiness", "Boominess"), None),
    "04": (("BoxyMidrange",), None),
    "05": (("Harshness",), 0.10),           # AND Sib <= 10%
    "06": (("DullSound",), None),           # product-v2 only
    "07": (("Thinness",), None),            # AND Harshness < Thinness
    "08": (("Resonance",), None),           # REPORT-ONLY
}
MIN_OCC = 0.05
CLEAN_MAX_OCC = 0.10


def clip_occupancy(net: EQNet, wav: Path,
                   emission: str = "product") -> tuple[dict[int, float], int]:
    audio, sr = load_wav_mono(wav)
    frames = AnalysisPipeline(sr).analyze(audio)
    if not frames:
        return {}, 0
    x = np.stack([features_v1(db_frame_to_linear(f), sr) for f in frames])
    probs = net.forward_problem(x)
    hits = probs > BASE_THRESHOLDS[None, :]
    if emission == "product":
        hits &= (probs - BASE_THRESHOLDS[None, :]) >= EMISSION_MARGINS[None, :]
        order = np.argsort(-probs, axis=1)
        top = np.zeros_like(hits)
        rows = np.arange(probs.shape[0])[:, None]
        top[rows, order[:, :EMISSION_TOPK]] = True
        hits &= top
    return {c: float(hits[:, c].mean()) for c in range(NUM_PROBLEMS)}, len(frames)


def run_gate(net: EQNet, schema: str, clips_dir: Path,
             feature_version: int, emission: str = "product",
             quiet: bool = False) -> dict:
    if feature_version != 1:
        print(f"WARNING: blob feature_version={feature_version}; the C++ "
              "runtime computes v1 features — realclip numbers are only "
              "product-faithful for v1 blobs.")
    names = problem_names(schema)
    name_to_idx = {n: i for i, n in enumerate(names)}

    rows = []
    passes, fails = [], []
    for wav in sorted(clips_dir.glob("*.wav")):
        num = wav.name[:2]
        if num not in GATE:
            continue
        occ, n = clip_occupancy(net, wav, emission)
        if not occ:
            continue
        expected, sib_cap = GATE[num]
        parts = [f"{names[c]} {occ[c]:5.1%}" for c in
                 sorted(occ, key=lambda c: -occ[c]) if occ[c] > 0.0]
        detail = "; ".join(parts) if parts else "(none)"

        verdict = "report-only"
        if num == "08":
            pass  # expected-fail ML, never gates M6
        else:
            ok = True
            why = []
            if num == "01":
                hot = [(names[c], occ[c]) for c in range(len(names))
                       if occ[c] > CLEAN_MAX_OCC]
                if hot:
                    ok = False
                    why.extend(f"{name} {value:.1%} > {CLEAN_MAX_OCC:.0%}"
                               for name, value in hot)
            if expected:
                got = max((occ[name_to_idx[e]] for e in expected
                           if e in name_to_idx), default=0.0)
                if got < MIN_OCC:
                    ok = False
                    why.append(f"{'/'.join(expected)} {got:.1%} < {MIN_OCC:.0%}")
            if sib_cap is not None and "Sibilance" in name_to_idx:
                sib = occ[name_to_idx["Sibilance"]]
                if sib > sib_cap:
                    ok = False
                    why.append(f"Sibilance {sib:.1%} > {sib_cap:.0%}")
            if num == "07":
                thin = occ.get(name_to_idx.get("Thinness", -1), 0.0)
                harsh = occ.get(name_to_idx.get("Harshness", -1), 0.0)
                if harsh >= thin:
                    ok = False
                    why.append(f"Harshness {harsh:.1%} >= Thinness {thin:.1%}")
            verdict = "PASS" if ok else "FAIL(" + "; ".join(why) + ")"
            (passes if ok else fails).append(num)

        rows.append((num, wav.name, n, detail, verdict))

    gated = [r for r in rows if r[0] != "08"]
    if not quiet:
        print(f"\n== M6 REAL-CLIP GATE (emission={emission}, schema {schema}) ==")
        for num, name, n, detail, verdict in rows:
            print(f"  {num} [{verdict:>9s}] ({n:3d} fr) {detail}")
        print(f"\n  RESULT: {len(passes)}/{len(gated)} gated clips PASS "
              f"(08 report-only). FAIL: {fails if fails else 'none'}")
    return {"pass": passes, "fail": fails, "rows": rows, "n_gated": len(gated)}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--blob", required=True)
    ap.add_argument("--clips", default=DEFAULT_CLIPS)
    ap.add_argument("--schema", default="",
                    help="required for v1 blobs; v2 blobs carry it in provenance")
    ap.add_argument("--emission", default="product", choices=("product", "raw"),
                    help="product = MLEngine margin+top-2 replica (default); "
                         "raw = prob>threshold (model recall only)")
    args = ap.parse_args()

    path = Path(args.blob)
    try:
        net, feature_version, provenance = blob_io.read_v2(path)
        schema = args.schema or provenance.get("problem_schema", "legacy-v1")
    except ValueError:
        net = blob_io.read_v1(path)
        feature_version = 1
        schema = args.schema or "legacy-v1"
    run_gate(net, schema, Path(args.clips), feature_version, args.emission)


if __name__ == "__main__":
    main()
