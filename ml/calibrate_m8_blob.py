"""Post-hoc threshold calibration for M8 lab blobs.

This does not train the network. It reads an existing v3 blob, rebuilds the
same kind of held-out calibration set used by ml.train, updates only the M8
presence/class thresholds, and writes a new v3 blob. Benchmark clips must not
be used here.
"""

from __future__ import annotations

import argparse
import datetime as _dt
from pathlib import Path

from . import blob_io
from .dataset import Sample, real_dataset, synthetic_dataset
from .model import M8_ARCH, problem_names
from .tier2 import tier2_dataset
from .train import _calibrate_m8_thresholds, _evaluate_m8_emission


def _print_calibration(calibration: dict, schema: str) -> None:
    print("\n== M8 per-class threshold calibration ==")
    print(f"samples={calibration.get('samples', 0)}  "
          f"target_fp={calibration.get('target_fp', 0.0):.3f}  "
          f"presence={calibration.get('presence_threshold', 0.0):.3f}")
    thresholds = calibration.get("class_thresholds", {})
    for name in problem_names(schema):
        print(f"  {name:<13} threshold {float(thresholds.get(name, 0.0)):.3f}")


def _print_emission(metrics: dict) -> None:
    print("\n== calibration emission ==")
    print(f"n={metrics['n']}  macro-F1={metrics['macro_f1']:.3f}  "
          f"clean-FP={metrics['clean_fp_rate']:.3f} "
          f"(on {metrics['clean_n']} clean)")
    for name, m in metrics["per_class"].items():
        print(f"  {name:<13} P {m['precision']:.2f}  R {m['recall']:.2f}  "
              f"F1 {m['f1']:.2f}  (support {m['support']}, fp {m['fp']})")


def _calibration_samples(args: argparse.Namespace,
                         feature_version: int,
                         schema: str,
                         seed: int) -> list[Sample]:
    if feature_version == 5:
        # M8 temporal lab: window-level calibration sources (same disjointness:
        # synthetic val seed + held-out singers/files, never benchmark clips).
        from .temporal import temporal_real, temporal_synthetic, temporal_tier2
        samples = temporal_synthetic(32, seed=987654, schema=schema)
        if args.vocalset and Path(args.vocalset).is_dir():
            _, hold = temporal_real(Path(args.vocalset), seed=seed, schema=schema)
            print(f"real held-out calibration: {len(hold)}")
            samples += hold
        if args.tier2_real and Path(args.tier2_real).is_dir():
            _, hold = temporal_tier2(Path(args.tier2_real), seed=seed, schema=schema)
            print(f"tier2 held-out calibration: {len(hold)}")
            samples += hold
        return samples

    samples = synthetic_dataset(32, feature_version,
                                relative_prominence=True,
                                seed=987654,
                                schema=schema)

    if args.vocalset:
        root = Path(args.vocalset)
        if not root.is_dir():
            raise SystemExit(f"--vocalset not found: {root}")
        _, heldout = real_dataset(root, feature_version,
                                  frames_per_clip=args.frames_per_clip,
                                  clips_per_singer=args.clips_per_singer,
                                  seed=seed,
                                  schema=schema,
                                  sib_focus_per_clip=args.sib_focus_per_clip)
        print(f"real held-out calibration: {len(heldout)}")
        samples += heldout

    if args.tier2_real:
        root = Path(args.tier2_real)
        if not root.is_dir():
            raise SystemExit(f"--tier2-real not found: {root}")
        _, heldout = tier2_dataset(root, feature_version,
                                   frames_per_file=args.tier2_frames_per_file,
                                   hf_negative_repeat=args.hf_negative_repeat,
                                   thinness_tonal_only=args.thinness_tonal_only,
                                   thinness_bass_repeat=args.thinness_bass_repeat,
                                   seed=seed,
                                   schema=schema)
        print(f"tier2 held-out calibration: {len(heldout)}")
        samples += heldout

    return samples


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--blob", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--target-fp", type=float, required=True,
                    help="negative-cell FP target for M8 threshold calibration")
    ap.add_argument("--problem-schema", default="",
                    choices=("", "legacy-v1", "product-v2"),
                    help="default: read from blob provenance")
    ap.add_argument("--seed", type=int, default=-1,
                    help="default: read from blob provenance or use 22")
    ap.add_argument("--vocalset", default="")
    ap.add_argument("--frames-per-clip", type=int, default=3)
    ap.add_argument("--clips-per-singer", type=int, default=16)
    ap.add_argument("--sib-focus-per-clip", type=int, default=2)
    ap.add_argument("--tier2-real", default="")
    ap.add_argument("--tier2-frames-per-file", type=int, default=3)
    ap.add_argument("--hf-negative-repeat", type=int, default=4)
    ap.add_argument("--thinness-tonal-only", action="store_true")
    ap.add_argument("--thinness-bass-repeat", type=int, default=0)
    ap.add_argument("--m8-ui-cap", type=int, default=3)
    args = ap.parse_args()

    net, feature_version, provenance = blob_io.read_v3(Path(args.blob))
    if provenance.get("architecture") != M8_ARCH:
        raise SystemExit(f"not an M8 lab blob: {args.blob}")

    schema = args.problem_schema or provenance.get("problem_schema", "product-v2")
    seed = args.seed if args.seed >= 0 else int(provenance.get("seed", 22))

    samples = _calibration_samples(args, feature_version, schema, seed)
    print(f"total calibration samples: {len(samples)}")
    calibration = _calibrate_m8_thresholds(net, samples, schema,
                                           target_fp=args.target_fp)
    _print_calibration(calibration, schema)

    metrics = _evaluate_m8_emission(net, samples, schema, "calibration",
                                    ui_cap=args.m8_ui_cap)
    _print_emission(metrics)

    updated = dict(provenance)
    updated["m8_threshold_calibration"] = calibration
    updated["m8_presence_threshold"] = float(net.presence_threshold)
    updated["m8_class_thresholds"] = [float(v) for v in net.class_thresholds]
    updated["posthoc_m8_threshold_calibration"] = {
        "date": _dt.datetime.now(_dt.timezone.utc).isoformat(timespec="seconds"),
        "source_blob": str(Path(args.blob)),
        "target_fp": args.target_fp,
        "samples": len(samples),
        "feature_version": feature_version,
        "problem_schema": schema,
        "seed": seed,
        "vocalset": str(Path(args.vocalset)) if args.vocalset else "",
        "tier2_real": str(Path(args.tier2_real)) if args.tier2_real else "",
        "frames_per_clip": args.frames_per_clip,
        "clips_per_singer": args.clips_per_singer,
        "sib_focus_per_clip": args.sib_focus_per_clip,
        "tier2_frames_per_file": args.tier2_frames_per_file,
        "hf_negative_repeat": args.hf_negative_repeat,
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    blob_io.write_v3(net, out, feature_version, updated)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
