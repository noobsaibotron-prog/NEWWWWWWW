#!/usr/bin/env python3
"""Measure whether a Boom injection is visible and label-safe.

This is a dataset-only diagnostic. It never writes the manifest, changes the
training builder, or loads a model. The output is used to freeze one measured
Boom gate before an ablation is implemented.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

import numpy as np

from . import dataset_v2 as ds
from . import lab_inject as li

SCHEMA = "aieq-boom-gate-probe-v1"
DOMAINS = ("vocal", "clean_bass", "clean_drums", "clean_synth", "clean_mix")
GAINS_DB = (6.0, 9.0, 12.0)
QUANTILES = (0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0)

# Diagnostic grid only. No row becomes a training threshold automatically.
CONTENT_MIN_GRID = (0.0, 3.0, 6.0, 9.0, 12.0)
PRE_EXCESS_MAX_GRID = (0.0, 3.0, 6.0, 9.0)
POST_EXCESS_MIN_GRID = (3.0, 6.0, 9.0, 12.0)
DELTA_EXCESS_MIN_DB = 4.0


@dataclass(frozen=True)
class BoomMeasurement:
    split: str
    domain: str
    group: str
    path: str
    window_index: int
    sample_rate: float
    gain_db: float
    raw_low_db: float
    raw_wide_db: float
    content_relative_db: float
    pre_excess_db: float
    post_excess_db: float
    delta_excess_db: float


def measure_boom_window(win_db: np.ndarray, sample_rate: float, gain_db: float,
                        *, split: str = "synthetic", domain: str = "synthetic",
                        group: str = "synthetic", path: str = "synthetic",
                        window_index: int = 0) -> BoomMeasurement:
    """Return scale-invariant content and Boom-excess measurements."""
    window = np.asarray(win_db, dtype=np.float64)
    if window.ndim != 2 or window.shape[1] < 3:
        raise ValueError("win_db must be a [frames, frequency-bins] array")
    if not np.isfinite(window).all():
        raise ValueError("win_db contains non-finite values")
    if sample_rate <= 0.0 or not np.isfinite(sample_rate):
        raise ValueError("sample_rate must be finite and positive")
    if gain_db <= 0.0 or not np.isfinite(gain_db):
        raise ValueError("gain_db must be finite and positive")

    wide_hi = min(10000.0, sample_rate * 0.45)
    if wide_hi <= 150.0:
        raise ValueError("sample_rate is too low for the Boom probe")

    raw_low = float(li.band_series(window, sample_rate, 40.0, 150.0).mean())
    raw_ref = float(li.band_series(window, sample_rate, 150.0, 400.0).mean())
    raw_wide = float(li.band_series(window, sample_rate, 100.0, wide_hi).mean())
    boosted = np.stack([
        li.scale_band_db(frame, sample_rate, 50.0, 150.0, gain_db)
        for frame in window
    ])
    post_low = float(li.band_series(boosted, sample_rate, 40.0, 150.0).mean())
    post_ref = float(li.band_series(boosted, sample_rate, 150.0, 400.0).mean())
    pre_excess = raw_low - raw_ref
    post_excess = post_low - post_ref

    return BoomMeasurement(
        split=split,
        domain=domain,
        group=group,
        path=path,
        window_index=window_index,
        sample_rate=float(sample_rate),
        gain_db=float(gain_db),
        raw_low_db=raw_low,
        raw_wide_db=raw_wide,
        content_relative_db=raw_low - raw_wide,
        pre_excess_db=pre_excess,
        post_excess_db=post_excess,
        delta_excess_db=post_excess - pre_excess,
    )


def qualifies(measurement: BoomMeasurement, *, content_min_db: float,
              pre_excess_max_db: float, post_excess_min_db: float,
              delta_excess_min_db: float = DELTA_EXCESS_MIN_DB) -> bool:
    """Apply one explicit candidate gate to a measurement."""
    return (
        measurement.content_relative_db >= content_min_db
        and measurement.pre_excess_db <= pre_excess_max_db
        and measurement.post_excess_db >= post_excess_min_db
        and measurement.delta_excess_db >= delta_excess_min_db
    )


def _ranked_rows(rows: Iterable[dict], split: str, domain: str,
                 max_files: int) -> list[dict]:
    selected = [row for row in rows if row["domain"] == domain]
    selected.sort(key=lambda row: hashlib.sha256(
        f"{split}\0{domain}\0{row['group']}\0{row['path']}".encode()
    ).hexdigest())
    return selected if max_files <= 0 else selected[:max_files]


def collect_measurements(*, manifest: Path, data_root: Path,
                         splits: tuple[str, ...], domains: tuple[str, ...],
                         gains_db: tuple[float, ...], windows_per_file: int,
                         max_files_per_domain: int,
                         log=print) -> tuple[list[BoomMeasurement], list[dict]]:
    if windows_per_file <= 0:
        raise ValueError("windows_per_file must be positive")
    measurements: list[BoomMeasurement] = []
    skipped: list[dict] = []

    for split in splits:
        rows = ds.manifest_rows(split, manifest)
        for domain in domains:
            selected = _ranked_rows(rows, split, domain, max_files_per_domain)
            log(f"  {split}/{domain}: {len(selected)} files")
            for row in selected:
                audio, sample_rate = ds.load_wav_mono(data_root / row["path"])
                windows = ds.windows_from_audio(audio, sample_rate)
                if not windows:
                    skipped.append({"split": split, "domain": domain,
                                    "group": row["group"], "path": row["path"],
                                    "reason": "shorter-than-window"})
                    continue
                chosen = ds.top_energy_windows(
                    windows, sample_rate, 100.0,
                    min(10000.0, sample_rate * 0.45), windows_per_file)
                for window_index, window in enumerate(chosen):
                    for gain_db in gains_db:
                        measurements.append(measure_boom_window(
                            window, sample_rate, gain_db,
                            split=split, domain=domain, group=row["group"],
                            path=row["path"], window_index=window_index))
    return measurements, skipped


def _quantile_map(values: list[float]) -> dict[str, float]:
    if not values:
        return {}
    result = np.quantile(np.asarray(values, dtype=np.float64), QUANTILES)
    return {f"q{int(q * 100):02d}": float(value)
            for q, value in zip(QUANTILES, result)}


def summarize(measurements: list[BoomMeasurement], skipped: list[dict]) -> dict:
    groups: dict[tuple[str, str, float], list[BoomMeasurement]] = {}
    for measurement in measurements:
        groups.setdefault((measurement.split, measurement.domain,
                           measurement.gain_db), []).append(measurement)

    distributions = []
    for (split, domain, gain_db), records in sorted(groups.items()):
        distributions.append({
            "split": split,
            "domain": domain,
            "gain_db": gain_db,
            "count": len(records),
            "content_relative_db": _quantile_map(
                [record.content_relative_db for record in records]),
            "pre_excess_db": _quantile_map(
                [record.pre_excess_db for record in records]),
            "post_excess_db": _quantile_map(
                [record.post_excess_db for record in records]),
            "delta_excess_db": _quantile_map(
                [record.delta_excess_db for record in records]),
        })

    grid = []
    for gain_db in GAINS_DB:
        gain_records = [record for record in measurements
                        if record.gain_db == gain_db]
        for content_min in CONTENT_MIN_GRID:
            for pre_max in PRE_EXCESS_MAX_GRID:
                for post_min in POST_EXCESS_MIN_GRID:
                    accepted = [record for record in gain_records if qualifies(
                        record, content_min_db=content_min,
                        pre_excess_max_db=pre_max,
                        post_excess_min_db=post_min)]
                    by_split = {
                        split: sum(record.split == split for record in accepted)
                        for split in sorted({record.split for record in gain_records})
                    }
                    grid.append({
                        "gain_db": gain_db,
                        "content_min_db": content_min,
                        "pre_excess_max_db": pre_max,
                        "post_excess_min_db": post_min,
                        "delta_excess_min_db": DELTA_EXCESS_MIN_DB,
                        "accepted": len(accepted),
                        "total": len(gain_records),
                        "accepted_by_split": by_split,
                    })

    return {
        "schema": SCHEMA,
        "measurement_count": len(measurements),
        "opportunity_count": len(measurements) // max(1, len(GAINS_DB)),
        "gains_db": list(GAINS_DB),
        "distributions": distributions,
        "candidate_grid": grid,
        "skipped": skipped,
        "records": [asdict(record) for record in measurements],
    }


def markdown_report(report: dict, full_report_sha256: str) -> str:
    lines = [
        "# Boom gate dataset probe",
        "",
        "Verdict: **MEASUREMENT ONLY - NO TRAINING AUTHORIZED**",
        "",
        f"- Opportunities: {report['opportunity_count']}",
        f"- Measurements: {report['measurement_count']}",
        f"- Skipped files: {len(report['skipped'])}",
        f"- Full JSON SHA-256: `{full_report_sha256}`",
        "",
        "The probe uses only manifest train/heldout material. A6 judge clips are not",
        "read, scored, or used to choose thresholds.",
        "",
        "## Median measurements",
        "",
        "| Split | Domain | Gain | N | Content rel. | Pre excess | Post excess | Delta |",
        "|---|---|---:|---:|---:|---:|---:|---:|",
    ]
    for row in report["distributions"]:
        lines.append(
            f"| {row['split']} | {row['domain']} | {row['gain_db']:.0f} dB | "
            f"{row['count']} | {row['content_relative_db']['q50']:.2f} | "
            f"{row['pre_excess_db']['q50']:.2f} | "
            f"{row['post_excess_db']['q50']:.2f} | "
            f"{row['delta_excess_db']['q50']:.2f} |")
    lines += [
        "",
        "Candidate-grid counts and all per-window measurements are stored in the",
        "paired JSON. No grid row is selected automatically.",
        "",
    ]
    return "\n".join(lines)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ds.MANIFEST)
    parser.add_argument("--data-root", type=Path, default=ds.DATA_ROOT_DEFAULT)
    parser.add_argument("--splits", default="train,heldout")
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--windows-per-file", type=int, default=1)
    parser.add_argument("--max-files-per-domain", type=int, default=0)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--markdown-out", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    splits = tuple(item.strip() for item in args.splits.split(",") if item.strip())
    domains = tuple(item.strip() for item in args.domains.split(",") if item.strip())
    unknown = sorted(set(domains) - set(DOMAINS))
    if unknown:
        raise ValueError(f"unknown domains: {unknown}")
    measurements, skipped = collect_measurements(
        manifest=args.manifest, data_root=args.data_root, splits=splits,
        domains=domains, gains_db=GAINS_DB,
        windows_per_file=args.windows_per_file,
        max_files_per_domain=args.max_files_per_domain)
    report = summarize(measurements, skipped)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(report, indent=2, sort_keys=True) + "\n"
    args.out.write_text(payload)
    digest = hashlib.sha256(payload.encode()).hexdigest()
    args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_out.write_text(markdown_report(report, digest))
    print(f"Boom probe: {report['opportunity_count']} opportunities, "
          f"{len(skipped)} skipped")
    print(f"JSON SHA-256: {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
