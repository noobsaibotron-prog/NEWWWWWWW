"""Offline training entry point.

Usage (from the repo root):
    python3 -m ml.train --out Resources/Models/ml_weights_v2.bin \
        --feature-version 2 --epochs 40 --samples-per-problem 96 \
        [--vocalset /Users/marco/aieq_data/real_audio/vocalset_extracted/FULL] \
        [--init-from Resources/Models/ml_weights.bin]

Produces a v2 blob (metadata + embedded checksum + provenance) and prints the
held-out metrics table. Deterministic for fixed seeds.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import subprocess
from pathlib import Path

import numpy as np

from . import blob_io
from .dataset import (Sample, dataset_hash, real_dataset, synthetic_dataset)
from .eval import BASE_THRESHOLDS, evaluate, format_report
from .model import EQNet, NUM_PROBLEMS, TwoStageEQNet, problem_names


M8_DEFAULT_MARGIN_LOSS_WEIGHT = 0.5


def _git_rev() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=Path(__file__).parent, text=True).strip()
    except Exception:
        return "unknown"


def _to_arrays(samples: list[Sample]):
    x = np.stack([s.features for s in samples])
    yp = np.stack([s.problem_targets for s in samples])
    yf = np.stack([s.freq_targets for s in samples])
    return x, yp, yf


def _logit(p: np.ndarray | float) -> np.ndarray | float:
    clipped = np.clip(p, 1e-6, 1.0 - 1e-6)
    return np.log(clipped / (1.0 - clipped))


def _calibrate_negative_bias(net: EQNet, samples: list[Sample], schema: str,
                             target: float, thinness_target: float
                             ) -> dict:
    if not samples:
        return {}
    if not (0.0 < target < 1.0):
        raise SystemExit("--calibrate-negative-fp-target must be between 0 and 1")
    if thinness_target <= 0.0:
        thinness_target = target
    if not (0.0 < thinness_target < 1.0):
        raise SystemExit("--calibrate-thinness-negative-fp-target must be between 0 and 1")

    x = np.stack([s.features for s in samples])
    y = np.stack([s.problem_targets for s in samples])
    probs = net.forward_problem(x)
    names = problem_names(schema)

    class_targets = np.full(NUM_PROBLEMS, target, dtype=np.float64)
    class_targets[5] = thinness_target
    deltas = np.zeros(NUM_PROBLEMS, dtype=np.float64)

    for c, threshold in enumerate(BASE_THRESHOLDS):
        neg = probs[y[:, c] < 0.5, c]
        if neg.size == 0:
            continue
        q = float(np.quantile(neg, 1.0 - class_targets[c]))
        deltas[c] = min(0.0, float(_logit(threshold) - _logit(q)))

    net.p3.b += deltas
    return {
        "negative_fp_target": target,
        "thinness_negative_fp_target": thinness_target,
        "samples": len(samples),
        "deltas": {names[i]: float(deltas[i]) for i in range(NUM_PROBLEMS)},
    }


def _evaluate_m8_emission(net: TwoStageEQNet, samples: list[Sample],
                          schema: str, note: str, ui_cap: int) -> dict:
    if not samples:
        return {"note": note, "macro_f1": 0.0, "clean_fp_rate": 0.0,
                "per_class": {}, "n": 0, "clean_n": 0}
    x = np.stack([s.features for s in samples])
    y = np.stack([s.problem_targets for s in samples])
    pred = net.emitted(x, ui_cap=ui_cap)
    truth = y > 0.5
    names = problem_names(schema)
    per_class = {}
    f1s = []
    for c in range(NUM_PROBLEMS):
        tp = int(np.sum(pred[:, c] & truth[:, c]))
        fp = int(np.sum(pred[:, c] & ~truth[:, c]))
        fn = int(np.sum(~pred[:, c] & truth[:, c]))
        support = int(truth[:, c].sum())
        precision = tp / (tp + fp) if tp + fp else 0.0
        recall = tp / (tp + fn) if tp + fn else 0.0
        f1 = (2 * precision * recall / (precision + recall)
              if precision + recall else 0.0)
        per_class[names[c]] = {
            "precision": precision, "recall": recall, "f1": f1,
            "support": support, "fp": fp,
        }
        if support > 0:
            f1s.append(f1)
    clean_mask = ~truth.any(axis=1)
    clean_fp = (float(pred[clean_mask].any(axis=1).mean())
                if clean_mask.any() else 0.0)
    return {
        "note": note,
        "n": len(samples),
        "macro_f1": float(np.mean(f1s)) if f1s else 0.0,
        "clean_fp_rate": clean_fp,
        "clean_n": int(clean_mask.sum()),
        "per_class": per_class,
    }


def _calibrate_m8_thresholds(net: TwoStageEQNet, samples: list[Sample],
                             schema: str, target_fp: float) -> dict:
    if not samples:
        return {}
    if not (0.0 < target_fp < 1.0):
        raise SystemExit("--m8-threshold-target-fp must be between 0 and 1")

    x = np.stack([s.features for s in samples])
    y = np.stack([s.problem_targets for s in samples])
    presence, probs = net.forward(x)
    clean = y.max(axis=1) < 0.5
    truth = y > 0.5

    if clean.any():
        q = float(np.quantile(presence[clean], 1.0 - target_fp))
        net.presence_threshold = min(0.95, max(0.50, q))

    thresholds = np.zeros(NUM_PROBLEMS, dtype=np.float64)
    names = problem_names(schema)
    for c in range(NUM_PROBLEMS):
        neg = probs[~truth[:, c], c]
        if neg.size:
            q = float(np.quantile(neg, 1.0 - target_fp))
            thresholds[c] = min(0.95, max(0.30, q))
        else:
            thresholds[c] = 0.50
    net.class_thresholds = thresholds
    return {
        "target_fp": target_fp,
        "samples": len(samples),
        "presence_threshold": float(net.presence_threshold),
        "class_thresholds": {names[i]: float(thresholds[i])
                             for i in range(NUM_PROBLEMS)},
    }


def train(args: argparse.Namespace) -> Path:
    rng = np.random.default_rng(args.seed)

    m8_margin_loss_weight = args.m8_margin_loss_weight
    if args.architecture == "m8-two-stage" and args.ranking_loss_weight > 0.0:
        if args.m8_margin_loss_weight == M8_DEFAULT_MARGIN_LOSS_WEIGHT:
            m8_margin_loss_weight = args.ranking_loss_weight
            print("M8: using --ranking-loss-weight as compatibility alias for "
                  f"--m8-margin-loss-weight ({m8_margin_loss_weight})")
        else:
            print("M8: ignoring legacy --ranking-loss-weight because "
                  "--m8-margin-loss-weight is explicit")

    print(f"== dataset (feature v{args.feature_version}, schema {args.problem_schema}) ==")
    if args.feature_version == 5:
        # M8 temporal lab: WINDOW-level samples (ml/temporal.py). v5 requires
        # the m8 architecture; the legacy path never sees these builders.
        if args.architecture != "m8-two-stage":
            raise SystemExit("--feature-version 5 requires --architecture m8-two-stage")
        from .temporal import (temporal_contrastive, temporal_real,
                               temporal_synthetic, temporal_tier2)
        synth = temporal_synthetic(args.samples_per_problem, seed=args.seed,
                                   schema=args.problem_schema,
                                   negative_multiplier=args.negative_multiplier)
        print(f"synthetic (windows): {len(synth)}")
        real_train, real_heldout = [], []
        if args.vocalset and Path(args.vocalset).is_dir():
            real_train, real_heldout = temporal_real(
                Path(args.vocalset), seed=args.seed, schema=args.problem_schema,
                clips_per_singer=args.clips_per_singer,
                sib_focus=args.sib_focus_per_clip)
            print(f"real train: {len(real_train)}   real held-out: {len(real_heldout)}")
        tier2_train_samples, tier2_heldout = [], []
        if args.tier2_real and Path(args.tier2_real).is_dir():
            tier2_train_samples, tier2_heldout = temporal_tier2(
                Path(args.tier2_real), seed=args.seed, schema=args.problem_schema,
                windows_per_file=args.tier2_frames_per_file,
                hf_negative_repeat=args.hf_negative_repeat)
            print(f"tier2 train: {len(tier2_train_samples)}   "
                  f"tier2 held-out: {len(tier2_heldout)}")
        contrastive_train, contrastive_heldout = [], []
        if args.contrastive_root and Path(args.contrastive_root).is_dir():
            contrastive_train, contrastive_heldout = temporal_contrastive(
                Path(args.contrastive_root), seed=args.seed,
                schema=args.problem_schema,
                windows_per_file=args.contrastive_windows_per_file,
                pair_repeat=args.contrastive_pair_repeat,
                # M9.5 round 2: feed the vocal set so the contrastive
                # generator adds the vocal-clean-vs-sibilant axis (raw natural
                # vocal frames as explicit Sibilance negatives).
                vocalset_dir=(Path(args.vocalset)
                              if args.vocalset and Path(args.vocalset).is_dir()
                              else None))
            print(f"contrastive train: {len(contrastive_train)}   "
                  f"contrastive held-out: {len(contrastive_heldout)}")
        val_samples = temporal_synthetic(32, seed=987654,
                                         schema=args.problem_schema)
    else:
        synth = synthetic_dataset(args.samples_per_problem, args.feature_version,
                                  relative_prominence=True,
                                  extra_resonance_positives=args.extra_resonance,
                                  negative_multiplier=args.negative_multiplier,
                                  seed=args.seed,
                                  schema=args.problem_schema)
        print(f"synthetic: {len(synth)}")

        real_train, real_heldout = [], []
        if args.vocalset:
            vs = Path(args.vocalset)
            if vs.is_dir():
                real_train, real_heldout = real_dataset(vs, args.feature_version,
                                                        frames_per_clip=args.frames_per_clip,
                                                        clips_per_singer=args.clips_per_singer,
                                                        seed=args.seed,
                                                        schema=args.problem_schema,
                                                        sib_focus_per_clip=args.sib_focus_per_clip)
                print(f"real train: {len(real_train)}   real held-out: {len(real_heldout)}")
            else:
                print(f"WARNING: vocalset dir not found: {vs} — synthetic-only run")

        tier2_train_samples, tier2_heldout = [], []
        if args.tier2_real:
            from .tier2 import tier2_dataset
            t2 = Path(args.tier2_real)
            if t2.is_dir():
                tier2_train_samples, tier2_heldout = tier2_dataset(
                    t2, args.feature_version,
                    frames_per_file=args.tier2_frames_per_file,
                    hf_negative_repeat=args.hf_negative_repeat,
                    thinness_tonal_only=args.thinness_tonal_only,
                    thinness_bass_repeat=args.thinness_bass_repeat,
                    seed=args.seed, schema=args.problem_schema)
                print(f"tier2 train: {len(tier2_train_samples)}   "
                      f"tier2 held-out: {len(tier2_heldout)}")
            else:
                print(f"WARNING: tier2 dir not found: {t2} — skipping")

        val_samples = synthetic_dataset(32, args.feature_version,
                                        relative_prominence=True, seed=987654,
                                        schema=args.problem_schema)
        contrastive_train, contrastive_heldout = [], []

    train_samples = (synth + real_train + tier2_train_samples
                     + contrastive_train)
    # deterministic shuffle
    order = rng.permutation(len(train_samples))
    train_samples = [train_samples[i] for i in order]

    x, yp, yf = _to_arrays(train_samples)
    print(f"train {x.shape[0]}  val {len(val_samples)}  held-out {len(real_heldout)}")

    # ---- model ----
    input_dim = int(x.shape[1])
    if args.architecture == "m8-two-stage":
        if args.init_from:
            raise SystemExit("--init-from is legacy-only; M8 v3 starts from scratch")
        trunk = tuple(int(h) for h in args.m8_trunk.split(","))
        if len(trunk) != 2:
            raise SystemExit("--m8-trunk must be 'h1,h2' (e.g. 192,96)")
        net = TwoStageEQNet(seed=args.seed, input_dim=input_dim, trunk=trunk)
        # M9.2b: activate the emission semantics BEFORE calibration/eval —
        # the M9.2 infrastructure defaulted to "legacy" and nothing set it,
        # so every trained blob stayed presence-gated (the measured M8 round-6
        # blindness on static problems, e.g. clip 05). Serialized with the
        # blob via write_v3 provenance; calibration is mode-independent
        # (head-output quantiles) so with per-class mode target_fp becomes
        # the HONEST per-class FP on the calibration heldout.
        net.emission_mode = args.emission_mode
        print(f"emission mode: {net.emission_mode}")
    elif args.init_from:
        net = blob_io.read_v1(Path(args.init_from))
        if net.input_dim != input_dim:
            raise SystemExit(f"--init-from blob has input_dim {net.input_dim}, "
                             f"features produce {input_dim} — incompatible")
        print(f"initialized from v1 blob: {args.init_from}")
    else:
        hidden = tuple(int(h) for h in args.hidden.split(","))
        if len(hidden) != 2:
            raise SystemExit("--hidden must be 'h1,h2' (e.g. 256,128)")
        net = EQNet(seed=args.seed, input_dim=input_dim, hidden=hidden)

    # class weights: boost historically weak or M6-target classes.
    class_weights = np.ones(NUM_PROBLEMS)
    class_weights[0] = args.resonance_weight
    class_weights[1] = args.harshness_weight
    class_weights[3] = args.sibilance_weight
    class_weights[5] = args.thinness_weight
    # slot 7 = DullSound under product-v2: needs positive push to outrank the
    # co-firing BoxyMidrange under the product top-2 emission rule.
    class_weights[7] = args.dullsound_weight
    # Some classes need extra specificity. Resonance ships with a very low
    # plugin threshold (0.20), and M6 measured that Sibilance learned "HF energy"
    # without non-vocal-HF negative pressure.
    neg_class_weights = np.ones(NUM_PROBLEMS)
    neg_class_weights[0] = args.resonance_neg_weight
    neg_class_weights[3] = args.sibilance_neg_weight
    # M7 round 3: targeted clean-floor pressure for the classes that leak on
    # clean material once their positives are boosted (Thinness via
    # bass-repeat, Boominess via ranking-loss competitor dynamics).
    neg_class_weights[4] = args.boominess_neg_weight
    neg_class_weights[5] = args.thinness_neg_weight
    # M9.5 round 2: BoxyMidrange explodes on clean drums under per-class
    # emission; push its negative gradient to respect the raw window.
    neg_class_weights[6] = args.boxy_neg_weight

    # M7: product operating point per class (threshold + emission margin +
    # slack) — the ranking loss pushes positive targets above THIS, because
    # this is what MLEngine::detectProblems actually requires to emit.
    emission_targets = None
    if args.ranking_loss_weight > 0.0:
        from .eval import BASE_THRESHOLDS
        from .eval_realclips import EMISSION_MARGINS
        emission_targets = (BASE_THRESHOLDS + EMISSION_MARGINS
                            + args.emission_slack)
        print(f"ranking loss ON: weight {args.ranking_loss_weight}, "
              f"rank-margin {args.rank_margin}, slack {args.emission_slack}")

    best_val = -1.0
    best_state = None
    n = x.shape[0]
    for epoch in range(args.epochs):
        # cosine LR decay stabilizes the tail (constant Adam LR oscillated
        # between over- and under-firing epochs on this tiny net)
        lr = args.lr_min + 0.5 * (args.lr - args.lr_min) * (
            1.0 + np.cos(np.pi * epoch / max(1, args.epochs - 1)))
        order = rng.permutation(n)
        losses = []
        for start in range(0, n, args.batch_size):
            idx = order[start:start + args.batch_size]
            if args.architecture == "m8-two-stage":
                stats = net.train_batch(
                    x[idx], yp[idx], yf[idx], lr=lr,
                    presence_weight=args.presence_loss_weight,
                    class_weight=args.class_loss_weight,
                    freq_loss_weight=args.freq_loss_weight,
                    class_pos_weight=args.pos_weight,
                    class_weights=class_weights,
                    neg_class_weights=neg_class_weights,
                    margin_weight=m8_margin_loss_weight,
                    margin_target=args.m8_margin_target)
            else:
                stats = net.train_batch(x[idx], yp[idx], yf[idx], lr=lr,
                                        fp_weight=args.fp_weight,
                                        pos_weight=args.pos_weight,
                                        class_weights=class_weights,
                                        neg_class_weights=neg_class_weights,
                                        ranking_weight=args.ranking_loss_weight,
                                        emission_targets=emission_targets,
                                        rank_margin=args.rank_margin)
            losses.append(stats["bce"])
        val_metrics = evaluate(net, val_samples, feature_note="val",
                               schema=args.problem_schema)
        macro_f1 = val_metrics["macro_f1"]
        clean_fp = val_metrics["clean_fp_rate"]
        # model selection: maximize F1 with a tolerance band on clean-FP.
        # (A hard -4*FP penalty made an all-negative early epoch with FP=0
        # beat every useful later epoch — measured. 5% raw-model clean-FP is
        # acceptable because the plugin adds margin+topK+veto gating on top;
        # beyond the band the penalty is steep.)
        score = macro_f1 - 4.0 * max(0.0, clean_fp - 0.05)
        if score > best_val:
            best_val = score
            if args.architecture == "m8-two-stage":
                best_state = [(d.w.copy(), d.b.copy()) for d in net.layers()]
            else:
                best_state = [(d.w.copy(), d.b.copy())
                              for d in (net.p1, net.p2, net.p3, net.f1, net.f2)]
        print(f"epoch {epoch + 1:3d}/{args.epochs}  lr {lr:.2e}  "
              f"bce {np.mean(losses):.4f}  "
              f"val macroF1 {macro_f1:.3f}  cleanFP {clean_fp:.3f}")

    if best_state is not None:
        layers = net.layers() if args.architecture == "m8-two-stage" \
            else (net.p1, net.p2, net.p3, net.f1, net.f2)
        for d, (w, b) in zip(layers, best_state):
            d.w, d.b = w, b

    calibration = {}
    if args.architecture == "m8-two-stage":
        if args.calibrate_per_class_thresholds:
            calibration_samples = (val_samples + real_heldout + tier2_heldout
                                   + contrastive_heldout)
            calibration = _calibrate_m8_thresholds(
                net, calibration_samples, args.problem_schema,
                args.m8_threshold_target_fp)
            print("\n== M8 per-class threshold calibration ==")
            print(f"samples={calibration.get('samples', 0)}  "
                  f"target_fp={calibration.get('target_fp', 0.0):.3f}  "
                  f"presence={calibration.get('presence_threshold', 0.0):.3f}")
            for name, threshold in calibration.get("class_thresholds", {}).items():
                print(f"  {name:<13} threshold {threshold:.3f}")
    elif args.calibrate_negative_fp_target > 0.0:
        calibration_samples = (val_samples + real_heldout + tier2_heldout
                               + contrastive_heldout)
        calibration = _calibrate_negative_bias(
            net, calibration_samples, args.problem_schema,
            args.calibrate_negative_fp_target,
            args.calibrate_thinness_negative_fp_target)
        print("\n== post-hoc negative bias calibration ==")
        print(f"samples={calibration.get('samples', 0)}  "
              f"target={calibration.get('negative_fp_target', 0.0):.3f}  "
              f"thinness={calibration.get('thinness_negative_fp_target', 0.0):.3f}")
        for name, delta in calibration.get("deltas", {}).items():
            if abs(delta) > 1e-9:
                print(f"  {name:<13} bias {delta:+.4f}")

    # ---- final evaluation ----
    print("\n== validation ==")
    print(format_report(evaluate(net, val_samples, feature_note="val",
                                 schema=args.problem_schema)))
    if args.architecture == "m8-two-stage":
        print("\n== validation (M8 emission) ==")
        print(format_report(_evaluate_m8_emission(
            net, val_samples, args.problem_schema, "val-emission",
            args.m8_ui_cap)))
    if real_heldout:
        print("\n== HELD-OUT singers (never trained) ==")
        print(format_report(evaluate(net, real_heldout, feature_note="held-out",
                                     schema=args.problem_schema)))
        if args.architecture == "m8-two-stage":
            print("\n== HELD-OUT singers (M8 emission) ==")
            print(format_report(_evaluate_m8_emission(
                net, real_heldout, args.problem_schema, "held-out-emission",
                args.m8_ui_cap)))
    if tier2_heldout:
        print("\n== TIER-2 HELD-OUT files (never trained) ==")
        print(format_report(evaluate(net, tier2_heldout, feature_note="tier2",
                                     schema=args.problem_schema)))
        if args.architecture == "m8-two-stage":
            print("\n== TIER-2 HELD-OUT files (M8 emission) ==")
            print(format_report(_evaluate_m8_emission(
                net, tier2_heldout, args.problem_schema, "tier2-emission",
                args.m8_ui_cap)))

    # ---- export ----
    provenance = {
        "schema": "mleq-v3" if args.architecture == "m8-two-stage" else "mleq-v2",
        "architecture": args.architecture,
        "problem_schema": args.problem_schema,
        "feature_version": args.feature_version,
        "date": _dt.datetime.now(_dt.timezone.utc).isoformat(timespec="seconds"),
        "git": _git_rev(),
        "seed": args.seed,
        "epochs": args.epochs,
        "lr": args.lr,
        "batch_size": args.batch_size,
        "train_samples": len(train_samples),
        "synthetic_per_problem": args.samples_per_problem,
        "real_train": len(real_train),
        "real_heldout": len(real_heldout),
        "tier2_train": len(tier2_train_samples),
        "tier2_heldout": len(tier2_heldout),
        "contrastive_train": len(contrastive_train),
        "contrastive_heldout": len(contrastive_heldout),
        "thinness_tonal_only": bool(args.thinness_tonal_only),
        "thinness_bass_repeat": args.thinness_bass_repeat,
        "dataset_hash": dataset_hash(train_samples),
        "class_weights": {"Resonance": args.resonance_weight,
                          "Harshness": args.harshness_weight,
                          "Sibilance": args.sibilance_weight,
                          "Thinness": args.thinness_weight,
                          "DullSound": args.dullsound_weight},
        "ranking_loss": {"weight": args.ranking_loss_weight,
                         "rank_margin": args.rank_margin,
                         "emission_slack": args.emission_slack},
        "hidden": list(getattr(net, "hidden", ())),
        "m8_trunk": list(getattr(net, "trunk", ())),
        "m8_presence_threshold": float(getattr(net, "presence_threshold", 0.0)),
        "m8_class_thresholds": [float(v) for v in getattr(net, "class_thresholds", [])],
        "m8_ui_cap": args.m8_ui_cap,
        "m8_threshold_calibration": calibration if args.architecture == "m8-two-stage" else {},
        "m8_loss": {"presence_weight": args.presence_loss_weight,
                    "class_weight": args.class_loss_weight,
                    "freq_loss_weight": args.freq_loss_weight,
                    "margin_weight": m8_margin_loss_weight,
                    "margin_target": args.m8_margin_target},
        "neg_class_weights": {"Resonance": args.resonance_neg_weight,
                              "Sibilance": args.sibilance_neg_weight,
                              "Boominess": args.boominess_neg_weight,
                              "Thinness": args.thinness_neg_weight},
        "posthoc_negative_bias_calibration": calibration if args.architecture != "m8-two-stage" else {},
    }
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    if args.architecture == "m8-two-stage":
        blob_io.write_v3(net, out, args.feature_version, provenance)
    else:
        blob_io.write_v2(net, out, args.feature_version, provenance)
    size = out.stat().st_size
    check = blob_io.fnv1a64(out.read_bytes()[:-8])
    print(f"\nwrote {out}  ({size} bytes, payload fnv1a64 {check:016x})")

    if args.also_v1:
        if args.architecture == "m8-two-stage":
            raise SystemExit("--also-v1 impossible for M8 two-stage blobs")
        if net.hidden != (128, 64):
            raise SystemExit("--also-v1 impossible: v1 layout is fixed at "
                             "128/64 hidden — non-default nets need the v2 loader")
        v1_out = out.with_name(out.stem + "_v1compat.bin")
        blob_io.write_v1(net, v1_out)
        print(f"wrote v1-compat blob {v1_out} ({v1_out.stat().st_size} bytes) — "
              "NOTE: only valid if feature_version == 1")
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--architecture", default="legacy",
                    choices=("legacy", "m8-two-stage"),
                    help="legacy = EQNet v2/v1 path; m8-two-stage = lab-only "
                         "presence gate + independent class heads, exported as v3")
    ap.add_argument("--out", required=True)
    ap.add_argument("--feature-version", type=int, default=2,
                    choices=(1, 2, 3, 4, 5))
    ap.add_argument("--epochs", type=int, default=40)
    ap.add_argument("--batch-size", type=int, default=32)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--lr-min", type=float, default=5e-5)
    ap.add_argument("--seed", type=int, default=22)
    ap.add_argument("--samples-per-problem", type=int, default=96)
    ap.add_argument("--extra-resonance", type=int, default=64)
    ap.add_argument("--negative-multiplier", type=float, default=3.0)
    ap.add_argument("--fp-weight", type=float, default=1.5)
    ap.add_argument("--pos-weight", type=float, default=6.0)
    ap.add_argument("--resonance-weight", type=float, default=1.0)
    ap.add_argument("--resonance-neg-weight", type=float, default=3.0)
    ap.add_argument("--harshness-weight", type=float, default=1.0)
    ap.add_argument("--sibilance-weight", type=float, default=1.0)
    ap.add_argument("--sibilance-neg-weight", type=float, default=1.0)
    ap.add_argument("--thinness-weight", type=float, default=1.3)
    ap.add_argument("--dullsound-weight", type=float, default=1.0,
                    help="positive weight for slot 7 (DullSound in product-v2)")
    ap.add_argument("--thinness-neg-weight", type=float, default=1.0)
    ap.add_argument("--boominess-neg-weight", type=float, default=1.0)
    ap.add_argument("--boxy-neg-weight", type=float, default=1.0,
                    help="M9.5 round 2: BoxyMidrange negative-cell weight "
                         "(clean-drum precision under per-class emission)")
    ap.add_argument("--sib-focus-per-clip", type=int, default=0,
                    help="M7: per vocal clip, add N sibilance-focused paired "
                         "positives (top 5-9kHz frames, boost vs de-ess — the "
                         "M2 method) to recover true vocal sibilance recall")
    ap.add_argument("--emission-mode", default="legacy",
                    choices=("legacy", "per-class"),
                    help="M9.2b (m8-two-stage only): emission semantics baked "
                         "into the blob. legacy = presence gate + class "
                         "thresholds (M8 behaviour); per-class = calibrated "
                         "class thresholds only (drops the global presence "
                         "gate that blinded static problems, M8 round-6)")
    ap.add_argument("--hidden", default="128,64",
                    help="problem-net hidden sizes 'h1,h2'. 128,64 = shipped. "
                         "Non-default sizes need the v2 loader (no --also-v1).")
    ap.add_argument("--ranking-loss-weight", type=float, default=0.0,
                    help="M7 product-aware shaping: emission hinge (target "
                         "above threshold+margin+slack) + top-2 rank hinge. "
                         "0 = off (byte-identical training)")
    ap.add_argument("--rank-margin", type=float, default=0.05)
    ap.add_argument("--emission-slack", type=float, default=0.05)
    ap.add_argument("--m8-trunk", default="192,96",
                    help="M8 shared trunk hidden sizes 'h1,h2'")
    ap.add_argument("--presence-loss-weight", type=float, default=1.0)
    ap.add_argument("--class-loss-weight", type=float, default=1.0)
    ap.add_argument("--freq-loss-weight", type=float, default=0.5)
    ap.add_argument("--m8-margin-loss-weight", type=float,
                    default=M8_DEFAULT_MARGIN_LOSS_WEIGHT)
    ap.add_argument("--m8-margin-target", type=float, default=0.70)
    ap.add_argument("--m8-ui-cap", type=int, default=3,
                    help="post-threshold display cap for M8 emission eval")
    ap.add_argument("--calibrate-per-class-thresholds", action="store_true",
                    help="M8: calibrate presence/class thresholds on held-out "
                         "negatives before writing the v3 blob")
    ap.add_argument("--m8-threshold-target-fp", type=float, default=0.03,
                    help="M8 calibration target FP for negative cells")
    ap.add_argument("--vocalset", default="")
    ap.add_argument("--frames-per-clip", type=int, default=2)
    ap.add_argument("--clips-per-singer", type=int, default=12)
    ap.add_argument("--problem-schema", default="legacy-v1",
                    choices=("legacy-v1", "product-v2"),
                    help="slot-7 semantics: legacy-v1=Clipping (shipped), "
                         "product-v2=DullSound (full product class coverage)")
    ap.add_argument("--tier2-real", default="",
                    help="tier2_train/ root (clean_* + hf_negative folders)")
    ap.add_argument("--tier2-frames-per-file", type=int, default=3)
    ap.add_argument("--hf-negative-repeat", type=int, default=3,
                    help="oversample factor for hf_negative frames "
                         "(the Sibilance hard negatives)")
    ap.add_argument("--thinness-tonal-only", action="store_true",
                    help="do not synthesize Thinness positives from clean_drums "
                         "Tier-2 sources")
    ap.add_argument("--thinness-bass-repeat", type=int, default=0,
                    help="extra Thinness positives per clean_bass Tier-2 frame")
    ap.add_argument("--calibrate-negative-fp-target", type=float, default=0.0,
                    help="post-training per-class bias calibration target over "
                         "negative validation cells; disabled at 0")
    ap.add_argument("--calibrate-thinness-negative-fp-target", type=float,
                    default=0.0,
                    help="override the negative calibration target for Thinness")
    ap.add_argument("--init-from", default="")
    ap.add_argument("--also-v1", action="store_true",
                    help="additionally export a v1-format blob (genre zeroed)")
    ap.add_argument("--contrastive-root", default="",
                    help="M9.3 (v5 only): tier2_train/ root for targeted "
                         "contrastive pairs per confusion axis (usually the "
                         "same dir as --tier2-real)")
    ap.add_argument("--contrastive-windows-per-file", type=int, default=3,
                    help="M9.3: top-energy windows per file for pairs")
    ap.add_argument("--contrastive-pair-repeat", type=int, default=1,
                    help="M9.3: repeat factor for each (raw+positives) block "
                         "to scale counts without changing the balance")
    train(ap.parse_args())


if __name__ == "__main__":
    main()
