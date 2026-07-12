#!/usr/bin/env python3
"""Motore v2 — A4 trainer (Manus spec: CLAUDE_A4_DESIGN_SPEC.md).

Loss = weighted class BCE + presence BCE + MASKED Smooth-L1 on freq (§2).
Optimizer AdamW, cosine 1e-3 -> 5e-5, batch 32, 40-50 epochs (§5).
Model selection on validation: macro_f1 - 4*max(0, clean_fp - 0.05) (§5).
Calibration (A4.5): per-class negative-quantile thresholds @ target_fp=0.05,
clamped [0.30, 0.95]; presence on all-clean windows, clamped [0.50, 0.95].
Export (§6): RTNeural JSON + provenance -> /tmp/aieq_v2/candidate_s<seed>.json
(NEVER touches Resources/Models/ml_weights.bin). 3 seeds mandatory (§7).

Round-1 choices documented for the counter-check:
  - dataset seed FIXED at 42 for every model seed (vary the INIT, not the
    data — the M8 lesson: per-class swings from data resampling mask init
    instability); validation/calibration built from split='heldout' seed 43.
  - loss head weights: class 1.0, presence 0.3, freq 0.5.
  - class pos_weight = clamp(sqrt(n_neg/n_pos), 1, 6) per class (spec §2
    "pesi per classe" — square root keeps rare-class gradients sane).

Run:  python3 -m ml_v2.train --seeds 42,1337,2026
      python3 -m ml_v2.train --quick          (smoke: tiny caps, 6 epochs)
"""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F

from .dataset_v2 import BuildConfig, build_or_load
from .export_rtneural import export_torch_model
from .model import MotoreV2CNN
from .lab_inject import PROBLEM_NAMES_V2

NUM_CLASSES = 8
TARGET_FP = 0.05


# ------------------------------------------------------------ metrics
def evaluate(model: torch.nn.Module, X: torch.Tensor, Y: torch.Tensor,
             batch: int = 256) -> dict:
    model.eval()
    probs_all = []
    with torch.no_grad():
        for i in range(0, X.shape[0], batch):
            logits = model(X[i:i + batch].transpose(1, 2))[:, :, -1]
            probs_all.append(torch.sigmoid(logits[:, :NUM_CLASSES]).cpu())
    probs = torch.cat(probs_all).numpy()
    y = Y[:, :NUM_CLASSES].cpu().numpy()

    pred = probs > 0.5
    f1s = []
    for c in range(NUM_CLASSES):
        tp = float(np.sum(pred[:, c] & (y[:, c] > 0.5)))
        fp = float(np.sum(pred[:, c] & (y[:, c] <= 0.5)))
        fn = float(np.sum(~pred[:, c] & (y[:, c] > 0.5)))
        prec = tp / (tp + fp) if tp + fp else 0.0
        rec = tp / (tp + fn) if tp + fn else 0.0
        f1s.append(2 * prec * rec / (prec + rec) if prec + rec else 0.0)
    macro_f1 = float(np.mean(f1s))

    clean = np.where(y.sum(axis=1) == 0)[0]
    clean_fp = float(np.mean(pred[clean].any(axis=1))) if clean.size else 0.0
    selection = macro_f1 - 4.0 * max(0.0, clean_fp - TARGET_FP)
    return {"macro_f1": macro_f1, "clean_fp": clean_fp,
            "selection": selection, "per_class_f1": [round(v, 3) for v in f1s],
            "probs": probs}


def calibrate(probs: np.ndarray, Y: np.ndarray) -> tuple[list[float], float]:
    """A4.5: per-class 1-target_fp quantile on NEGATIVES, clamp [0.30, 0.95];
    presence quantile on all-clean windows, clamp [0.50, 0.95]."""
    y = Y[:, :NUM_CLASSES]
    ths = []
    for c in range(NUM_CLASSES):
        neg = probs[y[:, c] <= 0.5, c]
        th = float(np.quantile(neg, 1.0 - TARGET_FP)) if neg.size else 0.5
        ths.append(float(np.clip(th, 0.30, 0.95)))
    clean = probs[y.sum(axis=1) == 0]
    pres = float(np.quantile(clean.max(axis=1), 1.0 - TARGET_FP)) \
        if clean.size else 0.5
    return ths, float(np.clip(pres, 0.50, 0.95))


# ------------------------------------------------------------ training
def train_one_seed(seed: int, Xtr, Ytr, Xva, Yva, device: str,
                   epochs: int, batch_size: int, lr: float, min_lr: float,
                   log=print) -> tuple[MotoreV2CNN, dict]:
    torch.manual_seed(seed)
    np.random.seed(seed)
    model = MotoreV2CNN(dtype=torch.float32).to(device)

    n_pos = Ytr[:, :NUM_CLASSES].sum(dim=0)
    n_neg = Ytr.shape[0] - n_pos
    pos_w = torch.clamp(torch.sqrt(n_neg / torch.clamp(n_pos, min=1.0)),
                        1.0, 6.0).to(device)
    pw_str = ", ".join(f"{PROBLEM_NAMES_V2[c][:4]}={float(pos_w[c]):.2f}"
                       for c in range(NUM_CLASSES))
    log(f"  pos_weight: {pw_str}")

    opt = torch.optim.AdamW(model.parameters(), lr=lr)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs,
                                                       eta_min=min_lr)
    n = Xtr.shape[0]
    best = {"selection": -1e9}
    best_state = None
    t0 = time.time()
    for epoch in range(epochs):
        model.train()
        perm = torch.randperm(n)
        tot = 0.0
        for i in range(0, n, batch_size):
            idx = perm[i:i + batch_size]
            xb = Xtr[idx].transpose(1, 2)              # [B, 64, 32]
            yb = Ytr[idx]
            logits = model(xb)[:, :, -1]               # [B, 17] last frame
            zc, zp, zf = (logits[:, :NUM_CLASSES], logits[:, NUM_CLASSES],
                          logits[:, NUM_CLASSES + 1:])
            yc = yb[:, :NUM_CLASSES]
            w = yc * pos_w + (1.0 - yc)                # per-element weights
            loss_c = F.binary_cross_entropy_with_logits(zc, yc, weight=w)
            loss_p = F.binary_cross_entropy_with_logits(zp, yb[:, NUM_CLASSES])
            mask = yc
            if mask.sum() > 0:
                loss_f = (F.smooth_l1_loss(torch.sigmoid(zf),
                                           yb[:, NUM_CLASSES + 1:],
                                           reduction="none") * mask
                          ).sum() / mask.sum()
            else:
                loss_f = torch.zeros((), device=xb.device)
            loss = loss_c + 0.3 * loss_p + 0.5 * loss_f
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += float(loss.detach()) * len(idx)
        sched.step()
        m = evaluate(model, Xva, Yva)
        marker = ""
        if m["selection"] > best["selection"]:
            best = {k: v for k, v in m.items() if k != "probs"}
            best["epoch"] = epoch
            best_state = {k: v.detach().cpu().clone()
                          for k, v in model.state_dict().items()}
            marker = "  <-- best"
        if epoch % 5 == 0 or marker:
            log(f"  ep {epoch:3d}  loss {tot / n:.4f}  "
                f"f1 {m['macro_f1']:.3f}  cleanFP {m['clean_fp']:.3f}  "
                f"sel {m['selection']:.3f}{marker}")
    log(f"  seed {seed}: best ep {best['epoch']} sel {best['selection']:.3f} "
        f"({time.time() - t0:.0f}s)")
    model.load_state_dict(best_state)
    return model, best


def export_candidate(model: MotoreV2CNN, seed: int, best: dict,
                     thresholds: list[float], presence_th: float,
                     out_dir: Path, meta: dict, log=print) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    model_cpu = model.to("cpu")
    json_path = out_dir / f"candidate_s{seed}.json"
    export_torch_model(model_cpu, str(json_path))

    # Export self-check with the REAL weights (A3 caveat): torch forward vs
    # the numpy JSON loader on a random window must agree in f32.
    from .eval_v2_benchmark import RtNeuralJsonModel
    rng = np.random.default_rng(9)
    win = rng.uniform(0.0, 1.0, (32, 64)).astype(np.float32)
    with torch.no_grad():
        yt = model_cpu(torch.from_numpy(win.T[None]))[0, :, -1].numpy()
    yn = RtNeuralJsonModel(json_path).forward_window(win)
    delta = float(np.max(np.abs(yt - np.asarray(yn, dtype=np.float32))))
    assert delta < 1e-4, f"export self-check failed: {delta}"
    log(f"  export self-check (trained weights): max|delta| = {delta:.2e}")

    prov = {
        "schema": "motore-v2-a4", "seed": seed,
        "class_order": list(PROBLEM_NAMES_V2),
        "class_thresholds": thresholds, "presence_threshold": presence_th,
        "target_fp": TARGET_FP, "best": best, "export_selfcheck_delta": delta,
        **meta,
    }
    prov_path = out_dir / f"candidate_s{seed}.provenance.json"
    prov_path.write_text(json.dumps(prov, indent=1))
    return json_path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--seeds", default="42,1337,2026")
    ap.add_argument("--epochs", type=int, default=45)
    ap.add_argument("--batch-size", type=int, default=32)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--min-lr", type=float, default=5e-5)
    ap.add_argument("--out", default="/tmp/aieq_v2")
    ap.add_argument("--cache", default="/tmp/aieq_v2/cache")
    ap.add_argument("--device", default=None)
    ap.add_argument("--quick", action="store_true",
                    help="smoke run: tiny dataset caps + 6 epochs")
    args = ap.parse_args()

    device = args.device or ("mps" if torch.backends.mps.is_available()
                             else "cpu")
    print(f"device={device}  torch={torch.__version__}")

    tr_cfg = BuildConfig(split="train", seed=42)
    va_cfg = BuildConfig(split="heldout", seed=43, clips_per_singer=16,
                         hf_negative_repeat=1)
    epochs = args.epochs
    if args.quick:
        tr_cfg = BuildConfig(split="train", seed=42, clips_per_singer=3,
                             windows_per_clip=1, tier2_windows_per_file=1,
                             contrastive_per_file=0, ring_per_file=0,
                             max_tier2_files_per_domain=10)
        va_cfg = BuildConfig(split="heldout", seed=43, clips_per_singer=3,
                             windows_per_clip=1, tier2_windows_per_file=1,
                             contrastive_per_file=0, ring_per_file=0,
                             hf_negative_repeat=1,
                             max_tier2_files_per_domain=10)
        epochs = 6

    cache = Path(args.cache)
    print("train set:")
    Xtr, Ytr, src_tr = build_or_load(tr_cfg, cache)
    print("val/calib set:")
    Xva, Yva, _ = build_or_load(va_cfg, cache)
    pos_frac = Ytr[:, :NUM_CLASSES].sum(axis=0) / len(Ytr)
    print(f"train {Xtr.shape}, val {Xva.shape}")
    print("train pos fraction per class:",
          {PROBLEM_NAMES_V2[c][:4]: round(float(pos_frac[c]), 3)
           for c in range(NUM_CLASSES)})

    Xtr_t = torch.from_numpy(Xtr).to(device)
    Ytr_t = torch.from_numpy(Ytr).to(device)
    Xva_t = torch.from_numpy(Xva).to(device)
    Yva_t = torch.from_numpy(Yva).to(device)

    meta = {"epochs": epochs, "batch_size": args.batch_size, "lr": args.lr,
            "min_lr": args.min_lr, "train_windows": int(Xtr.shape[0]),
            "val_windows": int(Xva.shape[0]),
            "dataset_train_key": tr_cfg.key(), "dataset_val_key": va_cfg.key(),
            "loss_weights": {"class": 1.0, "presence": 0.3, "freq": 0.5}}

    results = {}
    for seed in [int(s) for s in args.seeds.split(",")]:
        print(f"\n== seed {seed} ==")
        model, best = train_one_seed(seed, Xtr_t, Ytr_t, Xva_t, Yva_t, device,
                                     epochs, args.batch_size, args.lr,
                                     args.min_lr)
        final = evaluate(model.to(device), Xva_t, Yva_t)
        ths, pres = calibrate(final["probs"], Yva)
        path = export_candidate(model, seed, best, ths, pres,
                                Path(args.out), meta)
        print(f"  thresholds: "
              f"{[f'{PROBLEM_NAMES_V2[c][:4]}={ths[c]:.2f}' for c in range(8)]}")
        print(f"  exported -> {path}")
        results[seed] = best

    print("\n== SUMMARY ==")
    for seed, b in results.items():
        print(f"  seed {seed}: sel {b['selection']:.3f}  f1 {b['macro_f1']:.3f}"
              f"  cleanFP {b['clean_fp']:.3f}  ep {b['epoch']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
