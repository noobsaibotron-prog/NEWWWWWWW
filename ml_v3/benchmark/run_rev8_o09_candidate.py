"""Run the isolated REV8 O-09 kernel benchmark on the frozen gate platform.

The output is evidence, not authority.  In particular, this runner cannot
activate the provisional caps and deliberately reports that group-level
AP/Spearman/B-001 coverage is still pending.

Each timed workload runs in a fresh child process so peak RSS and cold-start
wall/CPU time are not contaminated by earlier workloads.
"""
from __future__ import annotations

import argparse
from fractions import Fraction
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import statistics
import subprocess
import sys
from time import perf_counter, process_time
from typing import Any

from ml_v3.benchmark.rev8_o09_candidate import (
    CandidateGraph,
    CandidateResult,
    ExactEdge,
    a2_exact,
    exact_maximum_cardinality,
    provisional_preflight_probe,
    rational_bit_length,
)
from ml_v3.contracts.canonical import (
    canonical_bytes,
    sha256_of_file,
    write_canonical,
)
from ml_v3.contracts.metrology_lock import (
    frozen_metrology_lock,
    require_gate_platform_python,
)

ROOT = Path(__file__).resolve().parents[2]
PROTECTED_PATHS = (
    "docs/MOTORE_V3_G1_CONTRACT_REV8_CANDIDATE.md",
    "docs/MOTORE_V3_G1_CONTRACT.md",
    "docs/MOTORE_V3_PLAN.md",
)
FROZEN_O_PATHS = (
    "ml_v3/fixtures/rev8/o02_boundary_artifact_v1.json",
    "ml_v3/fixtures/rev8/o03_width_artifact_v1.json",
    "ml_v3/fixtures/rev8/o18_numeric_artifact_v1.json",
    "ml_v3/fixtures/rev8/o02_o03_o18_generator_report_v1.json",
)


def _fraction(value: Fraction | None) -> list[int] | None:
    if value is None:
        return None
    return [value.numerator, value.denominator]


def _result_payload(result: CandidateResult) -> dict[str, Any]:
    payload = {
        "objective": {
            "k1": result.objective.k1,
            "k2": _fraction(result.objective.k2),
            "k3": result.objective.k3,
            "k4": _fraction(result.objective.k4),
            "k4_mode": result.objective.k4_mode,
        },
        "matching": [list(pair) for pair in result.matching],
        "scientific_sequence_hex": [
            value.hex() for value in result.scientific_sequence
        ],
        "diagnostic_sequence_hex": [
            value.hex() for value in result.diagnostic_sequence
        ],
        "severity_upper": _fraction(result.severity_upper),
        "onset_upper_ticks": _fraction(result.onset_upper_ticks),
        "offset_upper_ticks": _fraction(result.offset_upper_ticks),
    }
    return payload


def _edge(
    gt: int,
    prediction: int,
    *,
    n: int,
    kind: str,
) -> ExactEdge:
    delta = abs(gt - prediction)
    diagnostic = f"d:{gt:03}:{prediction:03}".encode()
    if kind == "unique_additive":
        return ExactEdge(
            gt,
            prediction,
            Fraction(n - delta, n),
            delta,
            Fraction(delta, n),
            f"s:{gt:03}:{prediction:03}".encode(),
            diagnostic,
            Fraction((gt * 17 + prediction * 13) % 101, 100),
            delta * 3,
            abs((gt + 1) % n - prediction) * 2,
        )
    if kind == "degenerate_additive":
        return ExactEdge(
            gt,
            prediction,
            Fraction(1),
            0,
            Fraction(0),
            b"same-scientific-key",
            diagnostic,
            Fraction((gt * 17 + prediction * 13) % 101, 100),
            delta,
            abs((gt + 1) % n - prediction),
        )
    if kind == "unique_product":
        low = min(gt + 1, prediction + 1)
        high = max(gt + 1, prediction + 1)
        return ExactEdge(
            gt,
            prediction,
            Fraction(n - delta, n),
            delta,
            Fraction(high, low),
            f"s:{gt:03}:{prediction:03}".encode(),
            diagnostic,
            Fraction((gt * 19 + prediction * 11) % 103, 102),
            delta * 5,
            abs((gt + 2) % n - prediction) * 3,
        )
    raise ValueError(f"unknown workload kind {kind!r}")


def _graph(kind: str, size: int) -> CandidateGraph:
    mode = "product" if kind == "unique_product" else "additive"
    return CandidateGraph(
        size,
        size,
        mode,
        tuple(
            _edge(gt, prediction, n=size, kind=kind)
            for gt in range(size)
            for prediction in range(size)
        ),
    )


def _measure_solver(kind: str, size: int) -> dict[str, Any]:
    graph = _graph(kind, size)
    probe = provisional_preflight_probe(graph)
    wall_start = perf_counter()
    cpu_start = process_time()
    result = a2_exact(graph)
    cpu = process_time() - cpu_start
    wall = perf_counter() - wall_start
    scientific = _result_payload(result)
    return {
        "kind": kind,
        "size": size,
        "gt": graph.gt_count,
        "prediction": graph.prediction_count,
        "eligible_edges": len(graph.edges),
        "preflight": {
            "exact_scalar_bit_bound": probe.exact_scalar_bit_bound,
            "provisional_exceeded": list(probe.provisional_exceeded),
            "authority_status": probe.authority_status,
        },
        "measurement": {
            "wall_seconds": wall,
            "cpu_seconds": cpu,
            # Darwin reports bytes; the platform check makes this unambiguous.
            "peak_rss_bytes": resource.getrusage(
                resource.RUSAGE_SELF).ru_maxrss,
        },
        "scientific_result_sha256": hashlib.sha256(
            canonical_bytes(scientific)).hexdigest(),
        "scientific_result": scientific,
    }


def _measure_ap_prefix(size: int) -> dict[str, Any]:
    """K1-only AP prefix pressure: n distinct confidence thresholds."""
    full = _graph("unique_additive", size)
    by_prediction: dict[int, list[ExactEdge]] = {
        prediction: [] for prediction in range(size)
    }
    for edge in full.edges:
        by_prediction[edge.prediction].append(edge)
    wall_start = perf_counter()
    cpu_start = process_time()
    tp: list[int] = []
    edge_volume = 0
    for threshold_count in range(1, size + 1):
        active = tuple(
            edge
            for prediction in range(threshold_count)
            for edge in by_prediction[prediction]
        )
        edge_volume += len(active)
        prefix = CandidateGraph(
            size, threshold_count, "additive",
            tuple(
                ExactEdge(
                    edge.gt,
                    edge.prediction,
                    edge.k2_iou,
                    edge.k3_tick_error,
                    edge.k4_cost,
                    edge.scientific_key,
                    edge.diagnostic_key,
                    edge.severity_error,
                    edge.onset_error_ticks,
                    edge.offset_error_ticks,
                )
                for edge in active
            ),
        )
        tp.append(exact_maximum_cardinality(prefix))
    cpu = process_time() - cpu_start
    wall = perf_counter() - wall_start
    payload = {
        "tp_by_prefix": tp,
        "thresholds": size,
        "edge_incidence_volume": edge_volume,
    }
    return {
        "kind": "ap_prefix_k1",
        "size": size,
        "measurement": {
            "wall_seconds": wall,
            "cpu_seconds": cpu,
            "peak_rss_bytes": resource.getrusage(
                resource.RUSAGE_SELF).ru_maxrss,
        },
        "scientific_result_sha256": hashlib.sha256(
            canonical_bytes(payload)).hexdigest(),
        "scientific_result": payload,
    }


def _measure_bit_boundaries() -> dict[str, Any]:
    cases = (
        ("under", Fraction(2**32_766, 2**32_767 + 1)),
        ("on", Fraction(2**32_767, 2**32_767 + 1)),
        ("over", Fraction(2**32_767, 2**32_768 + 1)),
    )
    results = []
    for label, value in cases:
        graph = CandidateGraph(
            1, 1, "additive",
            (ExactEdge(
                0, 0, value, 0, Fraction(0), b"s", b"d"),),
        )
        probe = provisional_preflight_probe(graph)
        results.append({
            "label": label,
            "input_rational_bit_length": rational_bit_length(value),
            "bound": probe.exact_scalar_bit_bound,
            "provisional_exceeded": list(probe.provisional_exceeded),
        })
    onset = 2**65_536 - 5
    onset_graph = CandidateGraph(
        1, 1, "additive",
        (ExactEdge(
            0, 0, Fraction(1), 0, Fraction(0), b"s", b"d",
            onset_error_ticks=onset,
        ),),
    )
    onset_probe = provisional_preflight_probe(onset_graph)
    results.append({
        "label": "onset_ms_over",
        "published_rational_bit_length": rational_bit_length(
            Fraction(onset, 48)),
        "bound": onset_probe.exact_scalar_bit_bound,
        "provisional_exceeded": list(onset_probe.provisional_exceeded),
    })
    return {"kind": "exact_bit_boundaries", "cases": results}


def _platform() -> dict[str, Any]:
    python = require_gate_platform_python()
    lock = frozen_metrology_lock()["bit_identity"]["gate_platform"]
    actual = {
        "os": sys.platform,
        "kernel": platform.system(),
        "kernel_release": platform.release(),
        "arch": platform.machine(),
        "os_marketing": platform.mac_ver()[0],
        "python": python,
    }
    if actual["kernel"].lower() != lock["os"]:
        raise RuntimeError(f"gate OS mismatch: {actual} vs {lock}")
    if actual["arch"] != lock["arch"]:
        raise RuntimeError(f"gate arch mismatch: {actual} vs {lock}")
    if actual["os_marketing"] != lock["os_marketing"].removeprefix("macOS "):
        raise RuntimeError(f"gate marketing OS mismatch: {actual} vs {lock}")
    return {"actual": actual, "lock": lock}


def _git(*arguments: str) -> str:
    return subprocess.check_output(
        ("git", *arguments), cwd=ROOT, text=True).strip()


def _worker() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", required=True)
    parser.add_argument("--size", type=int, required=True)
    args = parser.parse_args()
    _platform()
    if args.worker == "ap_prefix_k1":
        result = _measure_ap_prefix(args.size)
    else:
        result = _measure_solver(args.worker, args.size)
    print(json.dumps(result, sort_keys=True, separators=(",", ":")))


def _run_child(kind: str, size: int) -> dict[str, Any]:
    completed = subprocess.run(
        (
            sys.executable,
            "-m",
            "ml_v3.benchmark.run_rev8_o09_candidate",
            "--worker",
            kind,
            "--size",
            str(size),
        ),
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    return json.loads(completed.stdout)


def _quantiles(values: list[float]) -> dict[str, float]:
    ordered = sorted(values)
    return {
        "min": ordered[0],
        "median": statistics.median(ordered),
        "max": ordered[-1],
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--max-size", type=int, default=128,
        choices=(8, 16, 32, 64, 96, 128),
    )
    parser.add_argument("--repeat-small", type=int, default=3)
    parser.add_argument("--repeat-large", type=int, default=1)
    args = parser.parse_args()

    platform_evidence = _platform()
    status = _git("status", "--porcelain")
    if status:
        raise RuntimeError(
            "benchmark requires a clean immutable worktree; status:\n" + status)
    commit = _git("rev-parse", "HEAD")
    sizes = [size for size in (8, 16, 32, 64, 96, 128)
             if size <= args.max_size]
    workloads: list[dict[str, Any]] = []
    for kind in ("unique_additive", "degenerate_additive", "unique_product"):
        for size in sizes:
            repetitions = (
                args.repeat_small if size <= 64 else args.repeat_large)
            runs = [_run_child(kind, size) for _ in range(repetitions)]
            deterministic_hashes = {
                run["scientific_result_sha256"] for run in runs
            }
            if len(deterministic_hashes) != 1:
                raise RuntimeError(
                    f"non-deterministic scientific result for {kind}/{size}")
            workloads.append({
                "kind": kind,
                "size": size,
                "runs": runs,
                "summary": {
                    "wall_seconds": _quantiles([
                        run["measurement"]["wall_seconds"] for run in runs]),
                    "cpu_seconds": _quantiles([
                        run["measurement"]["cpu_seconds"] for run in runs]),
                    "peak_rss_bytes": max(
                        run["measurement"]["peak_rss_bytes"] for run in runs),
                    "scientific_result_sha256": next(iter(
                        deterministic_hashes)),
                },
            })
    for size in sizes:
        repetitions = args.repeat_small if size <= 64 else args.repeat_large
        runs = [_run_child("ap_prefix_k1", size) for _ in range(repetitions)]
        deterministic_hashes = {
            run["scientific_result_sha256"] for run in runs
        }
        if len(deterministic_hashes) != 1:
            raise RuntimeError(f"non-deterministic AP prefix at {size}")
        workloads.append({
            "kind": "ap_prefix_k1",
            "size": size,
            "runs": runs,
            "summary": {
                "wall_seconds": _quantiles([
                    run["measurement"]["wall_seconds"] for run in runs]),
                "cpu_seconds": _quantiles([
                    run["measurement"]["cpu_seconds"] for run in runs]),
                "peak_rss_bytes": max(
                    run["measurement"]["peak_rss_bytes"] for run in runs),
                "scientific_result_sha256": next(iter(deterministic_hashes)),
            },
        })

    protected = {
        path: sha256_of_file(ROOT / path)
        for path in (*PROTECTED_PATHS, *FROZEN_O_PATHS)
        if (ROOT / path).is_file()
    }
    evidence = {
        "schema": "aieq-v3-rev8-o09-candidate-benchmark-1",
        "authority_status": "EVIDENCE_ONLY_CAPS_NOT_ACTIVE",
        "commit": commit,
        "platform": platform_evidence,
        "configuration": {
            "max_size": args.max_size,
            "repeat_small": args.repeat_small,
            "repeat_large": args.repeat_large,
            "sizes": sizes,
        },
        "protected_sha256": protected,
        "bit_boundary_probes": _measure_bit_boundaries(),
        "workloads": workloads,
        "coverage": {
            "per_subgraph_A2": "MEASURED",
            "severity_onset_offset_envelopes": "MEASURED",
            "ap_prefix_K1_pressure": "MEASURED_NOT_FULL_GROUP_AP",
            "b001_group_coverage": "NOT_MEASURED",
            "spearman_group_singleton": "NOT_MEASURED",
            "ballot_ready": False,
            "reason": (
                "R23A_05 group-level AP, Spearman, and B-001 preflight "
                "surfaces require a separate candidate tranche"
            ),
        },
    }
    write_canonical(args.output, evidence)
    print(sha256_of_file(args.output))


if __name__ == "__main__":
    if "--worker" in sys.argv:
        _worker()
    else:
        main()
