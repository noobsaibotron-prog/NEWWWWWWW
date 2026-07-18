#!/usr/bin/env python3
"""F2a: paired dB-domain versus waveform EQ renderer probe.

This is a diagnostic, not a training or product path. Both renderers use the
same RBJ peaking-EQ transfer function and differ only in where it is applied:

* renderer 1 adds the steady-state biquad magnitude response to A2 raw-dB
  frames;
* renderer 2 filters the waveform, then runs the complete A2 STFT front-end.

The probe uses committed Round-5 JSON models through the NumPy inference path.
It intentionally has no Torch dependency and never reads Ableton judge clips.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

import numpy as np

from .dataset_v2 import DATA_ROOT_DEFAULT, MANIFEST, FastMel, manifest_rows
from .eval_v2_benchmark import RtNeuralJsonModel, load_wav_mono, sigmoid
from .feature import (FFT_SIZE, RAW_MAX_DB, RAW_MIN_DB,
                      frontend_raw_db_frames)
from .lab_inject import PROBLEM_NAMES_V2
from .model import T_WINDOW

SCHEMA = "aieq-f2a-renderer-probe-v1"
MATERIAL_DOMAINS = ("clean_bass", "clean_synth", "clean_mix")
MATERIAL_SECONDS = 8.0
PINK_SAMPLE_RATE = 44100
PINK_SEED = 20260718
WINDOW_COUNT = 3

# Frozen before the first measurement. These gates answer only whether the
# renderer changes the model input/output enough to justify an F2b ablation.
FEATURE_RMS_MIN = 1.0e-4
FEATURE_CASE_FRACTION_MIN = 0.75
TARGET_PROB_DELTA_MIN = 0.01
MODELS_PER_MATERIAL_MIN = 2
MATERIALS_PASS_MIN = 3


@dataclass(frozen=True)
class EqCase:
    problem: int
    center_hz: float
    q: float
    gain_db: float

    @property
    def name(self) -> str:
        return PROBLEM_NAMES_V2[self.problem]


EQ_CASES = (
    EqCase(0, 1000.0, 8.0, 9.0),
    EqCase(1, 3500.0, 2.0, 8.0),
    EqCase(2, 250.0, 1.0, 8.0),
    EqCase(3, 7000.0, 3.0, 8.0),
    EqCase(4, 90.0, 1.0, 8.0),
    EqCase(5, 180.0, 1.0, -10.0),
    EqCase(6, 500.0, 1.5, 8.0),
    EqCase(7, 9000.0, 1.0, -10.0),
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def rbj_peaking_coefficients(sample_rate: float, center_hz: float, q: float,
                             gain_db: float) -> np.ndarray:
    """Return normalized [b0,b1,b2,a1,a2] RBJ peaking-EQ coefficients."""
    if sample_rate <= 0.0:
        raise ValueError("sample_rate must be positive")
    if not 20.0 <= center_hz < sample_rate * 0.45:
        raise ValueError("center_hz must be in [20 Hz, 0.45 * sample_rate)")
    if q <= 0.0 or not math.isfinite(q):
        raise ValueError("q must be finite and positive")
    if not math.isfinite(gain_db):
        raise ValueError("gain_db must be finite")

    amplitude = 10.0 ** (gain_db / 40.0)
    omega = 2.0 * math.pi * center_hz / sample_rate
    alpha = math.sin(omega) / (2.0 * q)
    cos_omega = math.cos(omega)
    b0 = 1.0 + alpha * amplitude
    b1 = -2.0 * cos_omega
    b2 = 1.0 - alpha * amplitude
    a0 = 1.0 + alpha / amplitude
    a1 = -2.0 * cos_omega
    a2 = 1.0 - alpha / amplitude
    return np.asarray([b0 / a0, b1 / a0, b2 / a0,
                       a1 / a0, a2 / a0], dtype=np.float64)


def apply_biquad(samples: np.ndarray, coefficients: np.ndarray) -> np.ndarray:
    """Deterministic transposed-direct-form-II filter with zero initial state."""
    x = np.asarray(samples, dtype=np.float64)
    b0, b1, b2, a1, a2 = np.asarray(coefficients, dtype=np.float64)
    out = np.empty_like(x)
    state1 = 0.0
    state2 = 0.0
    for index, value in enumerate(x):
        filtered = b0 * value + state1
        state1 = b1 * value - a1 * filtered + state2
        state2 = b2 * value - a2 * filtered
        out[index] = filtered
    if not np.isfinite(out).all():
        raise FloatingPointError("biquad produced non-finite output")
    return out.astype(np.float32)


def biquad_magnitude_db(coefficients: np.ndarray, sample_rate: float,
                        num_bins: int = FFT_SIZE // 2 + 1) -> np.ndarray:
    """Magnitude response at the exact A2 rFFT bin frequencies."""
    b0, b1, b2, a1, a2 = np.asarray(coefficients, dtype=np.float64)
    frequencies = np.arange(num_bins, dtype=np.float64) * sample_rate / FFT_SIZE
    omega = 2.0 * np.pi * frequencies / sample_rate
    z1 = np.exp(-1j * omega)
    numerator = b0 + b1 * z1 + b2 * z1 * z1
    denominator = 1.0 + a1 * z1 + a2 * z1 * z1
    magnitude = np.maximum(np.abs(numerator / denominator), 1.0e-12)
    return (20.0 * np.log10(magnitude)).astype(np.float64)


def render_db_domain(raw_db: np.ndarray, coefficients: np.ndarray,
                     sample_rate: float) -> np.ndarray:
    response = biquad_magnitude_db(coefficients, sample_rate, raw_db.shape[1])
    rendered = np.asarray(raw_db, dtype=np.float64) + response[None, :]
    return np.clip(rendered, RAW_MIN_DB, RAW_MAX_DB).astype(np.float32)


def deterministic_window_starts(num_frames: int,
                                count: int = WINDOW_COUNT) -> list[int]:
    """Interior, evenly-spaced frame starts; never cherry-pick by prediction."""
    max_start = num_frames - T_WINDOW
    if max_start < 0:
        raise ValueError(f"need at least {T_WINDOW} frames, got {num_frames}")
    if count <= 0:
        raise ValueError("window count must be positive")
    if max_start == 0:
        return [0]
    requested = np.rint(np.linspace(0.2, 0.8, count) * max_start).astype(int)
    starts = sorted(set(int(value) for value in requested))
    if len(starts) < min(count, max_start + 1):
        starts = sorted(set(int(value) for value in
                            np.rint(np.linspace(0, max_start, count)).astype(int)))
    return starts


def _pink_noise(seconds: float = MATERIAL_SECONDS,
                sample_rate: int = PINK_SAMPLE_RATE) -> np.ndarray:
    sample_count = int(round(seconds * sample_rate))
    rng = np.random.default_rng(PINK_SEED)
    white = rng.standard_normal(sample_count)
    spectrum = np.fft.rfft(white)
    frequencies = np.fft.rfftfreq(sample_count, 1.0 / sample_rate)
    shaping = 1.0 / np.sqrt(np.maximum(frequencies, 20.0))
    pink = np.fft.irfft(spectrum * shaping, n=sample_count)
    pink -= pink.mean()
    rms = float(np.sqrt(np.mean(pink * pink)))
    if rms <= 0.0:
        raise RuntimeError("pink-noise generator produced silence")
    return np.asarray(pink * (0.1 / rms), dtype=np.float32)


def _center_crop(audio: np.ndarray, sample_rate: int,
                 seconds: float = MATERIAL_SECONDS) -> np.ndarray:
    wanted = int(round(seconds * sample_rate))
    if audio.size < wanted:
        raise ValueError(f"material is shorter than {seconds:.1f} seconds")
    start = (audio.size - wanted) // 2
    return np.asarray(audio[start:start + wanted], dtype=np.float32)


def _select_material_row(rows: Iterable[dict], domain: str,
                         data_root: Path) -> dict:
    eligible = []
    for row in rows:
        path = data_root / row["path"]
        if (row["domain"] == domain and float(row["sr"]) >= 32000.0
                and float(row["duration_s"]) >= MATERIAL_SECONDS):
            rank = hashlib.sha256(
                f"f2a-v1\0{domain}\0{row['group']}\0{row['sha256']}".encode()
            ).hexdigest()
            eligible.append((rank, row, path))
    if not eligible:
        raise FileNotFoundError(
            f"no train {domain} material >= {MATERIAL_SECONDS}s at >=32 kHz")
    _rank, selected, selected_path = min(eligible, key=lambda item: item[0])
    if not selected_path.is_file():
        raise FileNotFoundError(
            f"deterministically selected F2a material is missing: {selected_path}")
    return selected


def load_materials(manifest: Path, data_root: Path) -> list[dict]:
    """Load synthetic pink plus deterministic train-only real materials."""
    train_rows = manifest_rows("train", manifest)
    materials = []

    pink = _pink_noise()
    materials.append({
        "name": "pink",
        "domain": "synthetic",
        "group": "f2a:pink-20260718",
        "path": None,
        "source_sha256": hashlib.sha256(pink.tobytes()).hexdigest(),
        "audio": pink,
        "sample_rate": PINK_SAMPLE_RATE,
    })

    for domain in MATERIAL_DOMAINS:
        row = _select_material_row(train_rows, domain, data_root)
        path = data_root / row["path"]
        actual_sha = sha256_file(path)
        if actual_sha != row["sha256"]:
            raise ValueError(f"manifest hash mismatch for F2a material: {path}")
        audio, sample_rate = load_wav_mono(path)
        if int(sample_rate) != int(row["sr"]):
            raise ValueError(f"manifest sample-rate mismatch for F2a material: {path}")
        cropped = _center_crop(audio, sample_rate)
        materials.append({
            "name": domain,
            "domain": domain,
            "group": row["group"],
            "path": row["path"],
            "source_sha256": actual_sha,
            "audio": cropped,
            "sample_rate": int(sample_rate),
        })
    return materials


def load_models(models_dir: Path) -> list[dict]:
    models = []
    for path in sorted(models_dir.glob("candidate_s*.json")):
        if path.name.endswith(".provenance.json"):
            continue
        match = re.fullmatch(r"candidate_s(\d+)\.json", path.name)
        if match:
            models.append({
                "seed": int(match.group(1)),
                "path": path,
                "sha256": sha256_file(path),
                "model": RtNeuralJsonModel(path),
            })
    models.sort(key=lambda model: model["seed"])
    seeds = [model["seed"] for model in models]
    if seeds != [42, 1337, 2026]:
        raise ValueError(f"expected frozen Round-5 seeds [42,1337,2026], got {seeds}")
    return models


def _probabilities(model: RtNeuralJsonModel, mel_window: np.ndarray) -> np.ndarray:
    return sigmoid(model.forward_window(mel_window)[:8]).astype(np.float64)


def _percentile(values: list[float], q: float) -> float:
    return float(np.percentile(np.asarray(values, dtype=np.float64), q))


def summarize(feature_records: list[dict], model_records: list[dict]) -> dict:
    material_names = sorted({record["material"] for record in feature_records})
    material_summary = {}
    passing_materials = 0
    for material in material_names:
        features = [r for r in feature_records if r["material"] == material]
        models = [r for r in model_records if r["material"] == material]
        feature_fraction = float(np.mean([
            record["feature_rms"] >= FEATURE_RMS_MIN for record in features
        ]))
        seed_summary = {}
        qualifying_models = 0
        for seed in (42, 1337, 2026):
            seed_records = [r for r in models if r["model_seed"] == seed]
            target_abs = [abs(r["target_renderer_delta"]) for r in seed_records]
            non_target = [r["non_target_max_abs_delta"] for r in seed_records]
            median_target = float(np.median(target_abs))
            qualifies = median_target >= TARGET_PROB_DELTA_MIN
            qualifying_models += int(qualifies)
            seed_summary[str(seed)] = {
                "median_abs_target_probability_delta": median_target,
                "p95_abs_target_probability_delta": _percentile(target_abs, 95),
                "median_non_target_max_abs_delta": float(np.median(non_target)),
                "p95_non_target_max_abs_delta": _percentile(non_target, 95),
                "qualifies": qualifies,
            }
        passes = (feature_fraction >= FEATURE_CASE_FRACTION_MIN
                  and qualifying_models >= MODELS_PER_MATERIAL_MIN)
        passing_materials += int(passes)
        feature_rms = [r["feature_rms"] for r in features]
        material_summary[material] = {
            "feature_case_fraction_over_floor": feature_fraction,
            "median_feature_rms": float(np.median(feature_rms)),
            "p95_feature_rms": _percentile(feature_rms, 95),
            "qualifying_models": qualifying_models,
            "models": seed_summary,
            "passes": passes,
        }
    case_summary = {}
    for case in EQ_CASES:
        records = [r for r in model_records if r["case"] == case.name]
        db_effect = [abs(r["target_db_effect"]) for r in records]
        waveform_effect = [abs(r["target_waveform_effect"]) for r in records]
        renderer_delta = [abs(r["target_renderer_delta"]) for r in records]
        reference_effect = max(float(np.median(db_effect)),
                               float(np.median(waveform_effect)), 1.0e-12)
        case_summary[case.name] = {
            "median_abs_target_db_effect": float(np.median(db_effect)),
            "median_abs_target_waveform_effect": float(np.median(waveform_effect)),
            "median_abs_target_renderer_delta": float(np.median(renderer_delta)),
            "renderer_delta_to_injection_effect_ratio": (
                float(np.median(renderer_delta)) / reference_effect),
        }
    return {
        "passing_materials": passing_materials,
        "required_passing_materials": MATERIALS_PASS_MIN,
        "f2a_go": passing_materials >= MATERIALS_PASS_MIN,
        "materials": material_summary,
        "cases": case_summary,
    }


def _portable_path(path: Path) -> str:
    repo_root = Path(__file__).resolve().parent.parent
    resolved = path.resolve()
    try:
        return str(resolved.relative_to(repo_root))
    except ValueError:
        return str(resolved)


def run_probe(manifest: Path, data_root: Path, models_dir: Path) -> dict:
    materials = load_materials(manifest, data_root)
    models = load_models(models_dir)
    feature_records: list[dict] = []
    model_records: list[dict] = []
    material_metadata = []

    for material in materials:
        audio = material["audio"]
        sample_rate = material["sample_rate"]
        raw_frames = frontend_raw_db_frames(audio)
        starts = deterministic_window_starts(raw_frames.shape[0])
        fast_mel = FastMel(sample_rate)
        fast_mel.validate()
        base_mel = fast_mel(raw_frames)
        material_metadata.append({
            key: value for key, value in material.items() if key != "audio"
        } | {"window_starts": starts, "raw_frame_count": int(raw_frames.shape[0])})

        for case in EQ_CASES:
            coefficients = rbj_peaking_coefficients(
                sample_rate, case.center_hz, case.q, case.gain_db)
            db_frames = render_db_domain(raw_frames, coefficients, sample_rate)
            waveform_frames = frontend_raw_db_frames(apply_biquad(audio, coefficients))
            if waveform_frames.shape != raw_frames.shape:
                raise AssertionError("waveform renderer changed A2 frame count")
            db_mel = fast_mel(db_frames)
            waveform_mel = fast_mel(waveform_frames)

            for window_index, start in enumerate(starts):
                stop = start + T_WINDOW
                base_window = base_mel[start:stop]
                db_window = db_mel[start:stop]
                waveform_window = waveform_mel[start:stop]
                difference = waveform_window.astype(np.float64) \
                    - db_window.astype(np.float64)
                abs_difference = np.abs(difference)
                feature_records.append({
                    "material": material["name"],
                    "case": case.name,
                    "problem": case.problem,
                    "window": window_index,
                    "frame_start": start,
                    "feature_rms": float(np.sqrt(np.mean(difference * difference))),
                    "feature_mae": float(np.mean(abs_difference)),
                    "feature_p95_abs": float(np.percentile(abs_difference, 95)),
                    "feature_max_abs": float(abs_difference.max()),
                })

                for frozen in models:
                    base_prob = _probabilities(frozen["model"], base_window)
                    db_prob = _probabilities(frozen["model"], db_window)
                    waveform_prob = _probabilities(frozen["model"], waveform_window)
                    renderer_delta = waveform_prob - db_prob
                    non_target = np.delete(np.abs(renderer_delta), case.problem)
                    model_records.append({
                        "material": material["name"],
                        "case": case.name,
                        "problem": case.problem,
                        "window": window_index,
                        "model_seed": frozen["seed"],
                        "base_probabilities": base_prob.tolist(),
                        "db_probabilities": db_prob.tolist(),
                        "waveform_probabilities": waveform_prob.tolist(),
                        "renderer_probability_delta": renderer_delta.tolist(),
                        "target_db_effect": float(db_prob[case.problem]
                                                  - base_prob[case.problem]),
                        "target_waveform_effect": float(
                            waveform_prob[case.problem] - base_prob[case.problem]),
                        "target_renderer_delta": float(renderer_delta[case.problem]),
                        "non_target_max_abs_delta": float(non_target.max()),
                    })

    summary = summarize(feature_records, model_records)
    return {
        "schema": SCHEMA,
        "decision_scope": (
            "GO permits only a separately gated F2b waveform-renderer ablation; "
            "it is not evidence of model quality or production readiness."
        ),
        "manifest": _portable_path(manifest),
        "manifest_sha256": sha256_file(manifest),
        "models": [{key: value for key, value in model.items()
                    if key != "model" and key != "path"}
                   | {"path": _portable_path(model["path"])} for model in models],
        "materials": material_metadata,
        "cases": [asdict(case) | {"name": case.name} for case in EQ_CASES],
        "gates": {
            "feature_rms_min": FEATURE_RMS_MIN,
            "feature_case_fraction_min": FEATURE_CASE_FRACTION_MIN,
            "target_probability_delta_min": TARGET_PROB_DELTA_MIN,
            "models_per_material_min": MODELS_PER_MATERIAL_MIN,
            "materials_pass_min": MATERIALS_PASS_MIN,
        },
        "feature_records": feature_records,
        "model_records": model_records,
        "summary": summary,
    }


def markdown_report(report: dict, full_report_sha256: str | None = None) -> str:
    lines = [
        "# F2a renderer probe",
        "",
        f"Verdict: **{'GO to F2b ablation' if report['summary']['f2a_go'] else 'NO-GO'}**",
        "",
        report["decision_scope"],
        "",
        "| Material | Feature RMS median | Feature cases over floor | "
        "Qualifying models | Result |",
        "|---|---:|---:|---:|---|",
    ]
    for name, row in sorted(report["summary"]["materials"].items()):
        lines.append(
            f"| {name} | {row['median_feature_rms']:.6f} | "
            f"{100.0 * row['feature_case_fraction_over_floor']:.1f}% | "
            f"{row['qualifying_models']}/3 | {'PASS' if row['passes'] else 'FAIL'} |")
    lines.extend([
        "",
        "| Class | dB effect | Waveform effect | Renderer delta | Delta/effect |",
        "|---|---:|---:|---:|---:|",
    ])
    for name, row in report["summary"]["cases"].items():
        lines.append(
            f"| {name} | {row['median_abs_target_db_effect']:.6f} | "
            f"{row['median_abs_target_waveform_effect']:.6f} | "
            f"{row['median_abs_target_renderer_delta']:.6f} | "
            f"{100.0 * row['renderer_delta_to_injection_effect_ratio']:.2f}% |")
    lines.extend([
        "",
        "A NO-GO here means the waveform renderer does not enter F2b. The "
        "existing dB renderer remains the controlled baseline; other F2b "
        "interventions must be tested independently.",
    ])
    lines.extend(["", "## Frozen inputs", ""])
    for model in report["models"]:
        lines.append(f"- Round 5 seed {model['seed']}: `{model['sha256']}`")
    for material in report["materials"]:
        lines.append(
            f"- {material['name']}: `{material['source_sha256']}` "
            f"({material['group']}, {material['sample_rate']} Hz)")
    lines.extend(["", "The complete per-window feature and probability values "
                  "are in the paired JSON report."])
    if full_report_sha256:
        lines.append(f"Full JSON SHA-256: `{full_report_sha256}`")
    lines.extend([
        "",
        "## Reproduce",
        "",
        "```bash",
        "/Users/marco/aieq_data/env/a4b-venv/bin/python -m ml_v2.renderer_probe \\",
        "  --out /tmp/aieq_v2/f2a_renderer_probe/report.json \\",
        "  --markdown-out ml_v2/reports/F2A_RENDERER_PROBE.md",
        "```",
        "",
    ])
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--data-root", type=Path, default=DATA_ROOT_DEFAULT)
    parser.add_argument("--models-dir", type=Path,
                        default=Path(__file__).resolve().parent / "baselines" / "round5")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--markdown-out", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report = run_probe(args.manifest, args.data_root, args.models_dir)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if args.markdown_out:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(markdown_report(report, sha256_file(args.out)))
    verdict = "GO" if report["summary"]["f2a_go"] else "NO-GO"
    print(f"F2a {verdict}: {report['summary']['passing_materials']}/"
          f"{len(report['summary']['materials'])} materials pass "
          f"({report['summary']['required_passing_materials']} required)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
