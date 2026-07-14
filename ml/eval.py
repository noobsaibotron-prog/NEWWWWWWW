"""Evaluation: per-class precision/recall/F1 + clean-FP rate.

Decision rule mirrors the plugin default operating point: per-class base
thresholds from MLEngine.cpp (sensitivity 0.5 => scale 1.0, no margin/top-K —
this measures the RAW model quality; the plugin's extra gating only ever
reduces detections, so clean-FP measured here is an upper bound of shipped FP).
"""

from __future__ import annotations

import numpy as np

from .dataset import Sample
from .model import EQNet, NUM_PROBLEMS, PROBLEM_NAMES, problem_names

BASE_THRESHOLDS = np.array([0.20, 0.50, 0.30, 0.25, 0.30, 0.50, 0.30, 0.40])


def evaluate(net: EQNet, samples: list[Sample], feature_note: str = "",
             schema: str = "legacy-v1") -> dict:
    if not samples:
        return {"note": feature_note, "macro_f1": 0.0, "clean_fp_rate": 0.0,
                "per_class": {}, "n": 0}
    x = np.stack([s.features for s in samples])
    y = np.stack([s.problem_targets for s in samples])
    probs = net.forward_problem(x)
    pred = probs > BASE_THRESHOLDS[None, :]
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
    clean_fp_rate = (float(pred[clean_mask].any(axis=1).mean())
                     if clean_mask.any() else 0.0)

    return {
        "note": feature_note,
        "n": len(samples),
        "macro_f1": float(np.mean(f1s)) if f1s else 0.0,
        "clean_fp_rate": clean_fp_rate,
        "clean_n": int(clean_mask.sum()),
        "per_class": per_class,
    }


def format_report(m: dict) -> str:
    lines = [f"n={m['n']}  macro-F1={m['macro_f1']:.3f}  "
             f"clean-FP={m['clean_fp_rate']:.3f} (on {m.get('clean_n', 0)} clean)"]
    for name, s in m["per_class"].items():
        if s["support"] == 0 and s["fp"] == 0:
            continue
        lines.append(f"  {name:<13} P {s['precision']:.2f}  R {s['recall']:.2f}  "
                     f"F1 {s['f1']:.2f}  (support {s['support']}, fp {s['fp']})")
    return "\n".join(lines)
