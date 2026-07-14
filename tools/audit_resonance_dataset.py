#!/usr/bin/env python3
"""Report-only Blocco 1 audit for the Resonance dataset factory.

This script does not train a model and does not write weights. It measures
whether a formant-aware resonance injection policy is viable on VocalSet frames:

  - natural raw-frame prominence in the v3/detrended-mel feature space;
  - random post-injection prominence, pre prominence, and delta;
  - the actual pre-filtered Resonance sampler used by ml.dataset.real_dataset;
  - availability of target bands whose raw pre prominence is below P_lo.

Run from the repo root:

  python3 tools/audit_resonance_dataset.py \
      --vocalset /Users/marco/aieq_data/real_audio/vocalset_extracted/FULL \
      --out-json /tmp/aieq_resonance_audit.json \
      --out-csv /tmp/aieq_resonance_audit_samples.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from ml.dataset import (INJECTIONS, AnalysisPipeline, load_wav_mono, singer_of,
                        top_energy_frames, _sample_resonance_injection)
from ml.features import (MEL_NUM_BANDS, db_frame_to_linear, extract_mel_bands,
                         hz_to_mel, mel_to_hz)


FREQ_BINS = [
    (150.0, 300.0),
    (300.0, 700.0),
    (700.0, 1100.0),
    (1100.0, 2500.0),
    (2500.0, 5000.0),
    (5000.0, 8000.0),
]
SIGMA_MULTS = (0.5, 1.0, 2.0)


@dataclass
class FrameRecord:
    frame_db: np.ndarray
    sample_rate: float
    singer: str
    wav_name: str
    centers_hz: np.ndarray
    residual_db: np.ndarray


def mel_centers(sample_rate: float) -> np.ndarray:
    min_mel = hz_to_mel(20.0)
    max_mel = hz_to_mel(min(20000.0, sample_rate * 0.5))
    step = (max_mel - min_mel) / float(MEL_NUM_BANDS + 1)
    return np.array([mel_to_hz(min_mel + (i + 1) * step)
                     for i in range(MEL_NUM_BANDS)], dtype=np.float64)


def detrended_mel_residual(frame_db: np.ndarray, sample_rate: float) -> np.ndarray:
    band_db = extract_mel_bands(db_frame_to_linear(frame_db), sample_rate)
    idx = np.arange(band_db.shape[0], dtype=np.float64)
    idx_c = idx - idx.mean()
    mean_db = float(band_db.mean())
    slope = float((idx_c @ (band_db - mean_db)) / (idx_c @ idx_c))
    trend = mean_db + slope * idx_c
    return band_db - trend


def residual_at(record: FrameRecord, freq_hz: float,
                frame_db: np.ndarray | None = None) -> tuple[float, int]:
    if frame_db is None:
        residual = record.residual_db
    else:
        residual = detrended_mel_residual(frame_db, record.sample_rate)
    idx = int(np.argmin(np.abs(np.log(record.centers_hz / freq_hz))))
    return float(residual[idx]), idx


def inject_resonance(frame_db: np.ndarray, sample_rate: float, freq_hz: float,
                     boost_db: float, sigma_mult: float) -> np.ndarray:
    n_bins = int(frame_db.shape[0])
    fft_size = (n_bins - 1) * 2
    bin_hz = sample_rate / float(fft_size)
    freqs = np.arange(n_bins, dtype=np.float64) * bin_hz
    sigma = max(30.0, freq_hz * 0.05) * sigma_mult
    peak = np.exp(-0.5 * ((freqs - freq_hz) / sigma) ** 2)
    return frame_db + boost_db * peak


def quantiles(values: np.ndarray) -> dict[str, float]:
    if values.size == 0:
        return {}
    qs = np.percentile(values, [0, 5, 25, 50, 75, 95, 99, 100])
    names = ("min", "p05", "p25", "p50", "p75", "p95", "p99", "max")
    return {name: float(value) for name, value in zip(names, qs)}


def collect_frames(args: argparse.Namespace, rng: np.random.Generator) -> list[FrameRecord]:
    vocalset = Path(args.vocalset)
    wavs = sorted(vocalset.rglob("*.wav"))
    by_singer: dict[str, list[Path]] = {}
    for wav in wavs:
        singer = singer_of(wav)
        if singer != "unknown":
            by_singer.setdefault(singer, []).append(wav)

    records: list[FrameRecord] = []
    for singer, paths in sorted(by_singer.items()):
        n_pick = min(args.clips_per_singer, len(paths))
        if n_pick <= 0:
            continue
        for idx in rng.choice(len(paths), size=n_pick, replace=False):
            wav = paths[int(idx)]
            try:
                audio, sample_rate = load_wav_mono(wav)
                frames = AnalysisPipeline(sample_rate).analyze(audio)
            except Exception:
                continue
            for frame_db in top_energy_frames(frames, sample_rate, 100.0, 10000.0,
                                              args.frames_per_clip):
                centers = mel_centers(sample_rate)
                records.append(FrameRecord(
                    frame_db=frame_db,
                    sample_rate=sample_rate,
                    singer=singer,
                    wav_name=wav.name,
                    centers_hz=centers,
                    residual_db=detrended_mel_residual(frame_db, sample_rate),
                ))
    return records


def run_audit(args: argparse.Namespace) -> tuple[dict, list[dict]]:
    rng = np.random.default_rng(args.seed)
    records = collect_frames(args, rng)

    natural_max = []
    natural_random = []
    samples: list[dict] = []
    prefiltered_samples: list[dict] = []

    for record in records:
        freq_mask = ((record.centers_hz >= args.freq_lo)
                     & (record.centers_hz <= args.freq_hi))
        if np.any(freq_mask):
            natural_max.append(float(np.max(record.residual_db[freq_mask])))

        freq_hz = float(args.freq_lo * (args.freq_hi / args.freq_lo) ** rng.uniform())
        natural_random.append(residual_at(record, freq_hz)[0])

        for _ in range(args.injections_per_frame):
            freq_hz = float(args.freq_lo * (args.freq_hi / args.freq_lo) ** rng.uniform())
            boost_db = float(rng.uniform(args.boost_min, args.boost_max))
            sigma_mult = float(rng.choice(SIGMA_MULTS))
            pre_db, band_index = residual_at(record, freq_hz)
            post_frame = inject_resonance(record.frame_db, record.sample_rate,
                                          freq_hz, boost_db, sigma_mult)
            post_db, _ = residual_at(record, freq_hz, post_frame)
            samples.append({
                "mode": "random-target",
                "singer": record.singer,
                "wav": record.wav_name,
                "freq_hz": freq_hz,
                "mel_band_index": band_index,
                "boost_db": boost_db,
                "sigma_mult": sigma_mult,
                "pre_prom_db": pre_db,
                "post_prom_db": post_db,
                "delta_prom_db": post_db - pre_db,
            })

        for _ in range(args.prefiltered_per_frame):
            sampled = _sample_resonance_injection(record.frame_db, record.sample_rate,
                                                  rng, INJECTIONS[0])
            if sampled is None:
                continue
            row = {
                "mode": "prefiltered",
                "singer": record.singer,
                "wav": record.wav_name,
                "freq_hz": sampled.target_freq,
                "mel_band_index": sampled.band_index,
                "boost_db": sampled.boost_db,
                "sigma_mult": sampled.sigma_mult,
                "pre_prom_db": sampled.pre_prom_db,
                "post_prom_db": sampled.post_prom_db,
                "delta_prom_db": sampled.delta_prom_db,
            }
            samples.append(row)
            prefiltered_samples.append(row)

    natural_max_a = np.asarray(natural_max, dtype=np.float64)
    natural_random_a = np.asarray(natural_random, dtype=np.float64)
    random_samples = [s for s in samples if s["mode"] == "random-target"]
    post_a = np.asarray([s["post_prom_db"] for s in random_samples], dtype=np.float64)
    pre_a = np.asarray([s["pre_prom_db"] for s in random_samples], dtype=np.float64)
    delta_a = np.asarray([s["delta_prom_db"] for s in random_samples], dtype=np.float64)
    prefiltered_post_a = np.asarray([s["post_prom_db"] for s in prefiltered_samples],
                                    dtype=np.float64)
    prefiltered_pre_a = np.asarray([s["pre_prom_db"] for s in prefiltered_samples],
                                   dtype=np.float64)
    prefiltered_delta_a = np.asarray([s["delta_prom_db"] for s in prefiltered_samples],
                                     dtype=np.float64)

    thresholds = {}
    for threshold in args.thresholds:
        thresholds[str(threshold)] = {
            "clean_max_over": float(np.mean(natural_max_a >= threshold)),
            "clean_random_over": float(np.mean(natural_random_a >= threshold)),
            "post_keep": float(np.mean(post_a >= threshold)),
        }

    availability = {}
    for p_lo in args.p_lo_values:
        by_bin = {}
        for lo_hz, hi_hz in FREQ_BINS:
            frames_with_candidate = 0
            total_candidates = 0
            total_possible = 0
            for record in records:
                mask = ((record.centers_hz >= lo_hz)
                        & (record.centers_hz < hi_hz))
                possible = int(np.sum(mask))
                if possible == 0:
                    continue
                candidates = int(np.sum(mask & (record.residual_db <= p_lo)))
                frames_with_candidate += int(candidates > 0)
                total_candidates += candidates
                total_possible += possible
            by_bin[f"{int(lo_hz)}-{int(hi_hz)}"] = {
                "frames_with_candidate_rate": (
                    frames_with_candidate / len(records) if records else 0.0),
                "candidate_band_rate": (
                    total_candidates / total_possible if total_possible else 0.0),
                "candidate_bands": total_candidates,
                "possible_bands": total_possible,
            }
        availability[str(p_lo)] = by_bin

    survival = {}
    for p_lo in args.p_lo_values:
        for delta_hi in args.delta_hi_values:
            for p_hi in args.p_hi_values:
                key = f"pre<={p_lo}|delta>={delta_hi}|post>={p_hi}"
                mask = ((pre_a <= p_lo) & (delta_a >= delta_hi) & (post_a >= p_hi))
                survival[key] = float(np.mean(mask))

    prefiltered_by_bin = {}
    for lo_hz, hi_hz in FREQ_BINS:
        in_bin = [s for s in prefiltered_samples
                  if lo_hz <= s["freq_hz"] < hi_hz]
        prefiltered_by_bin[f"{int(lo_hz)}-{int(hi_hz)}"] = {
            "accepted": len(in_bin),
            "accepted_rate_of_all_prefiltered": (
                len(in_bin) / len(prefiltered_samples)
                if prefiltered_samples else 0.0),
        }

    report = {
        "seed": args.seed,
        "vocalset": str(args.vocalset),
        "frames": len(records),
        "clips_per_singer": args.clips_per_singer,
        "frames_per_clip": args.frames_per_clip,
        "injections_per_frame": args.injections_per_frame,
        "prefiltered_per_frame": args.prefiltered_per_frame,
        "natural_max_quantiles": quantiles(natural_max_a),
        "natural_random_quantiles": quantiles(natural_random_a),
        "pre_prom_quantiles": quantiles(pre_a),
        "post_prom_quantiles": quantiles(post_a),
        "delta_prom_quantiles": quantiles(delta_a),
        "prefiltered": {
            "attempts": len(records) * args.prefiltered_per_frame,
            "accepted": len(prefiltered_samples),
            "accepted_rate": (
                len(prefiltered_samples) / (len(records) * args.prefiltered_per_frame)
                if records and args.prefiltered_per_frame else 0.0),
            "pre_prom_quantiles": quantiles(prefiltered_pre_a),
            "post_prom_quantiles": quantiles(prefiltered_post_a),
            "delta_prom_quantiles": quantiles(prefiltered_delta_a),
            "by_freq_bin": prefiltered_by_bin,
        },
        "thresholds": thresholds,
        "availability": availability,
        "survival_random_target": survival,
        "notes": [
            "Prominence is v3-style detrended mel residual.",
            "clean_max_over is deliberately strict: max raw prominence per frame.",
            "Random-target survival is not the proposed factory; it measures why a pre-filter is needed.",
            "Prefiltered metrics call ml.dataset._sample_resonance_injection.",
        ],
    }
    return report, samples


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vocalset", required=True)
    parser.add_argument("--seed", type=int, default=20260703)
    parser.add_argument("--clips-per-singer", type=int, default=12)
    parser.add_argument("--frames-per-clip", type=int, default=2)
    parser.add_argument("--injections-per-frame", type=int, default=3)
    parser.add_argument("--prefiltered-per-frame", type=int, default=3)
    parser.add_argument("--freq-lo", type=float, default=150.0)
    parser.add_argument("--freq-hi", type=float, default=8000.0)
    parser.add_argument("--boost-min", type=float, default=6.0)
    parser.add_argument("--boost-max", type=float, default=18.0)
    parser.add_argument("--thresholds", type=float, nargs="+",
                        default=[2, 4, 6, 8, 10, 12, 14, 16])
    parser.add_argument("--p-lo-values", type=float, nargs="+",
                        default=[0, 2, 4, 6, 8, 10])
    parser.add_argument("--delta-hi-values", type=float, nargs="+",
                        default=[4, 6, 8, 10])
    parser.add_argument("--p-hi-values", type=float, nargs="+",
                        default=[8, 10, 12, 14])
    parser.add_argument("--out-json", default="")
    parser.add_argument("--out-csv", default="")
    args = parser.parse_args()

    report, samples = run_audit(args)

    print(json.dumps(report, indent=2, sort_keys=True))

    if args.out_json:
        Path(args.out_json).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if args.out_csv:
        with Path(args.out_csv).open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(samples[0].keys()) if samples else [])
            if samples:
                writer.writeheader()
                writer.writerows(samples)


if __name__ == "__main__":
    main()
