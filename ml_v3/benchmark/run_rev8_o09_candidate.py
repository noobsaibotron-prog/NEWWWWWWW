"""Run the isolated REV8 O-09 kernel benchmark on the frozen gate platform.

The output is evidence, not authority.  In particular, this runner cannot
activate the provisional caps and deliberately reports that group-level
AP/Spearman/B-001 coverage is still pending.

Each timed workload runs in a fresh child process.  The wall/CPU timer covers
the exact solve only.  Peak RSS is the worker high-water mark through result
materialization, including imports, graph construction, preflight, and solve;
later evidence hashing/JSON encoding is outside that sample.
"""
from __future__ import annotations

import argparse
from fractions import Fraction
import hashlib
from importlib.machinery import EXTENSION_SUFFIXES, SourceFileLoader
import json
from math import gcd
import os
from pathlib import Path
import platform
import resource
import statistics
import subprocess
import sys
from time import perf_counter, process_time
from typing import Any

import numpy as np

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
PROVENANCE_PATHS = (
    "ml_v3/fixtures/g1/metrology_lock.json",
    "ml_v3/environment/requirements.lock",
    "ml_v3/benchmark/rev8_o09_candidate.py",
    "ml_v3/benchmark/run_rev8_o09_candidate.py",
    "ml_v3/tests/test_g1c_rev8_o09_candidate.py",
)
HASHED_PATHS = (*PROTECTED_PATHS, *FROZEN_O_PATHS, *PROVENANCE_PATHS)


def _pairwise_coprime_256bit_denominators() -> tuple[int, ...]:
    """Return 128 deterministic pairwise-coprime, 256-bit odd integers."""
    values: list[int] = []
    candidate = 2**255 + 1
    while len(values) < 128:
        if all(gcd(candidate, prior) == 1 for prior in values):
            values.append(candidate)
        candidate += 2
    return tuple(values)


_COPRIME_DENOMINATORS_256 = _pairwise_coprime_256bit_denominators()


def _fraction(value: Fraction | None) -> list[str] | None:
    """Serialize an exact rational without decimal bigint conversion.

    The 65,536-bit O-09 boundary exceeds Python's defensive decimal-digit
    conversion limit.  Signed lowercase hexadecimal strings are exact,
    language-neutral, compact, and avoid changing that process-wide guard.
    """
    if value is None:
        return None
    return [hex(value.numerator), hex(value.denominator)]


def _exact_integer(value: int) -> str:
    """Serialize an unbounded exact scalar integer as signed lowercase hex."""
    return hex(value)


def _result_payload(result: CandidateResult) -> dict[str, Any]:
    payload = {
        "objective": {
            "k1": result.objective.k1,
            "k2": _fraction(result.objective.k2),
            "k3": _exact_integer(result.objective.k3),
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
    if kind == "bitstress_additive":
        # At n=128 the signed conservative sum bound is exactly 65,536:
        # 128*D + (A + 127*D) + ceil(log2(128))
        # with A=249 numerator bits and D=256 denominator bits.
        # A perfect matching selects one value per GT row.  The 128 row
        # denominators are pairwise coprime, so the reduced K2 sum really
        # materializes a near-65k-bit numerator/denominator instead of merely
        # reaching the conservative preflight formula with repeated q values.
        near_limit = Fraction(
            2**248,
            _COPRIME_DENOMINATORS_256[gt],
        )
        return ExactEdge(
            gt,
            prediction,
            near_limit,
            delta,
            Fraction(0),
            f"s:{gt:03}:{prediction:03}".encode(),
            diagnostic,
            Fraction(0),
            delta,
            delta,
        )
    if kind == "bitstress_product":
        # Every ratio has 256 numerator + 256 denominator bits.  A product
        # of 128 selected edges therefore reaches exactly 65,536 bits before
        # any reduction; numerator/denominator are coprime odd integers.
        ratio = Fraction(2**255 + 3, 2**255 + 1)
        return ExactEdge(
            gt,
            prediction,
            Fraction(1),
            delta,
            ratio,
            f"s:{gt:03}:{prediction:03}".encode(),
            diagnostic,
            Fraction(0),
            delta,
            delta,
        )
    if kind == "bitstress_degenerate":
        # At n=128 this single ambiguous workload simultaneously exercises:
        # - all 16,384 eligible edges;
        # - K6 over a non-singleton V optimum;
        # - the three exact ambiguity envelopes;
        # - a K3 sum bound of exactly 65,536 bits;
        # - a severity mean bound of exactly 65,536 bits; and
        # - onset/offset publication through exact /48 at exactly 65,536 bits.
        #
        # severity: D=256, A=242, K=128
        #   128*D + (A + 127*D) + ceil(log2(K)) + ceil(log2(K))
        #   = 65,536.
        # tick metrics: B=65,516, K=128
        #   B + ceil(log2(K)) + bit_length(K*48) = 65,536.
        # K3: B=65,529, K=128
        #   B + ceil(log2(K)) = 65,536.
        severity_near_limit = Fraction(2**241, 2**255 + 1)
        tick_near_limit = 2**65_515
        k3_near_limit = 2**65_528
        return ExactEdge(
            gt,
            prediction,
            Fraction(1),
            k3_near_limit,
            Fraction(0),
            b"same-scientific-key",
            diagnostic,
            severity_near_limit,
            tick_near_limit,
            tick_near_limit,
        )
    raise ValueError(f"unknown workload kind {kind!r}")


def _graph(kind: str, size: int) -> CandidateGraph:
    mode = (
        "product"
        if kind in ("unique_product", "bitstress_product")
        else "additive"
    )
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
    if not sys.dont_write_bytecode:
        raise RuntimeError(
            "O-09 evidence requires PYTHONDONTWRITEBYTECODE=1")
    python = require_gate_platform_python()
    lock = frozen_metrology_lock()["bit_identity"]["gate_platform"]
    actual = {
        "os": sys.platform,
        "kernel": platform.system(),
        "kernel_release": platform.release(),
        "arch": platform.machine(),
        "os_marketing": platform.mac_ver()[0],
        "python": python,
        "numpy": np.__version__,
    }
    if actual["kernel"].lower() != lock["os"]:
        raise RuntimeError(f"gate OS mismatch: {actual} vs {lock}")
    if actual["arch"] != lock["arch"]:
        raise RuntimeError(f"gate arch mismatch: {actual} vs {lock}")
    if actual["os_marketing"] != lock["os_marketing"].removeprefix("macOS "):
        raise RuntimeError(f"gate marketing OS mismatch: {actual} vs {lock}")
    if actual["numpy"] != lock["numpy"]:
        raise RuntimeError(f"gate NumPy mismatch: {actual} vs {lock}")
    return {"actual": actual, "lock": lock}


def _source_hashes() -> dict[str, str]:
    hashes: dict[str, str] = {}
    for relative in HASHED_PATHS:
        path = ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"required O-09 provenance file missing: {relative}")
        hashes[relative] = sha256_of_file(path)
    return hashes


def _loaded_module_provenance() -> dict[str, dict[str, str]]:
    """Hash every loaded project Python module plus the ``-m`` entrypoint."""
    evidence: dict[str, dict[str, str]] = {}

    def record(label: str, module: object) -> None:
        module_file = getattr(module, "__file__", None)
        if module_file is None:
            raise RuntimeError(f"loaded module has no source path: {label}")
        actual = Path(module_file).resolve()
        try:
            relative = str(actual.relative_to(ROOT))
        except ValueError as error:
            raise RuntimeError(
                f"project module loaded outside repository for {label}: "
                f"{actual}") from error
        if actual.suffix != ".py":
            raise RuntimeError(
                f"non-source project module loaded for {label}: {actual}")
        loader = getattr(module, "__loader__", None)
        spec = getattr(module, "__spec__", None)
        origin = getattr(spec, "origin", None)
        if not isinstance(loader, SourceFileLoader):
            raise RuntimeError(
                f"non-source loader for project module {label}: {loader!r}")
        if origin is None or Path(origin).resolve() != actual:
            raise RuntimeError(
                f"module spec origin mismatch for {label}: {origin!r}")
        cached = getattr(module, "__cached__", None)
        cached_path = Path(cached).resolve() if cached is not None else None
        if cached_path is not None and cached_path.exists():
            raise RuntimeError(
                f"project module bytecode cache exists for {label}: "
                f"{cached_path}")
        evidence[label] = {
            "path": relative,
            "sha256": sha256_of_file(actual),
            "loader": "SourceFileLoader",
            "spec_origin": relative,
            "cached_path": (
                str(cached_path) if cached_path is not None else "NONE"),
            "cached_exists": False,
        }

    record("runner_entrypoint", sys.modules[__name__])
    for module_name, module in sorted(sys.modules.items()):
        if module_name != "ml_v3" and not module_name.startswith("ml_v3."):
            continue
        module_file = getattr(module, "__file__", None)
        if module_file is not None:
            record(module_name, module)
    return evidence


def _tree_hygiene(*, require_clean_git: bool) -> None:
    if sys.pycache_prefix is not None:
        raise RuntimeError(
            "benchmark forbids sys.pycache_prefix, including external caches")
    for variable in ("PYTHONPYCACHEPREFIX", "PYTHONPATH", "PYTHONHOME"):
        if os.environ.get(variable):
            raise RuntimeError(
                f"benchmark forbids import-affecting environment {variable}")
    caches = sorted(
        str(path.relative_to(ROOT))
        for path in (ROOT / "ml_v3").rglob("__pycache__")
    )
    pyc_files = sorted(
        str(path.relative_to(ROOT))
        for path in (ROOT / "ml_v3").rglob("*.pyc")
    )
    native_shadows = sorted(
        str(path.relative_to(ROOT))
        for path in (ROOT / "ml_v3").rglob("*")
        if path.is_file() and any(
            path.name.endswith(suffix) for suffix in EXTENSION_SUFFIXES)
    )
    if caches or pyc_files or native_shadows:
        raise RuntimeError(
            "benchmark requires an archive-like Python source tree; "
            f"caches={caches[:5]}, pyc={pyc_files[:5]}, "
            f"native_shadows={native_shadows[:5]}")
    if require_clean_git:
        status = _git("status", "--porcelain")
        if status:
            raise RuntimeError(
                "benchmark requires a clean immutable source worktree; "
                f"status:\n{status}")


def _external_output_path(path: Path) -> Path:
    resolved = path.expanduser().resolve()
    try:
        resolved.relative_to(ROOT.resolve())
    except ValueError:
        return resolved
    raise RuntimeError(
        f"O-09 benchmark output must be outside the repository: {resolved}")


def _git(*arguments: str) -> str:
    return subprocess.check_output(
        ("git", *arguments), cwd=ROOT, text=True).strip()


def _worker() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", required=True)
    parser.add_argument("--size", type=int, required=True)
    args = parser.parse_args()
    _platform()
    _tree_hygiene(require_clean_git=False)
    source_before = _source_hashes()
    loaded_modules_before = _loaded_module_provenance()
    if args.worker == "ap_prefix_k1":
        result = _measure_ap_prefix(args.size)
    else:
        result = _measure_solver(args.worker, args.size)
    source_after = _source_hashes()
    loaded_modules_after = _loaded_module_provenance()
    if source_before != source_after:
        raise RuntimeError(
            "O-09 provenance files changed while a worker was executing")
    if loaded_modules_before != loaded_modules_after:
        raise RuntimeError(
            "loaded project modules changed while a worker was executing")
    result["worker_provenance"] = {
        "source_sha256": source_after,
        "loaded_modules": loaded_modules_after,
    }
    print(json.dumps(result, sort_keys=True, separators=(",", ":")))


def _run_child(kind: str, size: int) -> dict[str, Any]:
    child_environment = os.environ.copy()
    for variable in ("PYTHONPYCACHEPREFIX", "PYTHONPATH", "PYTHONHOME"):
        child_environment.pop(variable, None)
    child_environment["PYTHONDONTWRITEBYTECODE"] = "1"
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
        env=child_environment,
        check=False,
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"O-09 worker failed for {kind}/{size} "
            f"(exit {completed.returncode}):\n{completed.stderr}")
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
    output_path = _external_output_path(args.output)

    platform_evidence = _platform()
    _tree_hygiene(require_clean_git=True)
    source_hashes_start = _source_hashes()
    loaded_modules_parent_start = _loaded_module_provenance()
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
    if args.max_size == 128:
        for kind in (
            "bitstress_additive",
            "bitstress_product",
            "bitstress_degenerate",
        ):
            runs = [
                _run_child(kind, 128) for _ in range(args.repeat_large)
            ]
            deterministic_hashes = {
                run["scientific_result_sha256"] for run in runs
            }
            if len(deterministic_hashes) != 1:
                raise RuntimeError(
                    f"non-deterministic scientific result for {kind}/128")
            workloads.append({
                "kind": kind,
                "size": 128,
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

    _tree_hygiene(require_clean_git=True)
    source_hashes_end = _source_hashes()
    loaded_modules_parent_end = _loaded_module_provenance()
    if source_hashes_start != source_hashes_end:
        raise RuntimeError(
            "O-09 provenance files changed during the benchmark run")
    if loaded_modules_parent_start != loaded_modules_parent_end:
        raise RuntimeError(
            "loaded project modules changed during the benchmark run")
    for row in workloads:
        for run in row["runs"]:
            worker = run.get("worker_provenance")
            if worker is None:
                raise RuntimeError("worker omitted provenance evidence")
            if worker["source_sha256"] != source_hashes_start:
                raise RuntimeError(
                    "worker source hashes do not match the parent snapshot")
            if worker["loaded_modules"] != loaded_modules_parent_end:
                raise RuntimeError(
                    "worker loaded-module evidence differs from the parent")
    evidence = {
        "schema": "aieq-v3-rev8-o09-candidate-benchmark-3",
        "authority_status": "EVIDENCE_ONLY_CAPS_NOT_ACTIVE",
        "commit": commit,
        "platform": platform_evidence,
        "configuration": {
            "invocation": [
                sys.executable,
                "-m",
                "ml_v3.benchmark.run_rev8_o09_candidate",
                "--output",
                str(output_path),
                "--max-size",
                str(args.max_size),
                "--repeat-small",
                str(args.repeat_small),
                "--repeat-large",
                str(args.repeat_large),
            ],
            "python_dont_write_bytecode": sys.dont_write_bytecode,
            "external_output_required": True,
            "atomic_publish_after_final_hygiene": True,
            "exact_scalar_integer_encoding": "signed_lowercase_hex_strings",
            "max_size": args.max_size,
            "repeat_small": args.repeat_small,
            "repeat_large": args.repeat_large,
            "sizes": sizes,
        },
        "protected_sha256": source_hashes_end,
        "loaded_modules": loaded_modules_parent_end,
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
    evidence["payload_sha256"] = hashlib.sha256(
        canonical_bytes(evidence)).hexdigest()
    temporary_output = output_path.with_name(
        f".{output_path.name}.{os.getpid()}.tmp")
    if temporary_output.exists():
        raise RuntimeError(
            f"refusing to overwrite stale benchmark temp: {temporary_output}")
    try:
        write_canonical(temporary_output, evidence)
        _tree_hygiene(require_clean_git=True)
        if _source_hashes() != source_hashes_end:
            raise RuntimeError(
                "O-09 provenance files changed during evidence publication")
        if _loaded_module_provenance() != loaded_modules_parent_end:
            raise RuntimeError(
                "loaded project modules changed during evidence publication")
        os.replace(temporary_output, output_path)
    finally:
        temporary_output.unlink(missing_ok=True)
    print(sha256_of_file(output_path))


if __name__ == "__main__":
    if "--worker" in sys.argv:
        _worker()
    else:
        main()
