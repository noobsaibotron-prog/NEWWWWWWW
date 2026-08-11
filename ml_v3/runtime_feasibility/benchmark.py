"""Steady-state inference benchmark for the lab surrogates.

Reports percentiles, never a bare mean: the question is whether a *late* frame
still fits the hop, and an average hides exactly the tail that would cause a
dropped analysis.

Warm-up is measured and reported separately rather than folded in. The first
iterations pay lazy allocation and kernel selection, so including them would
flatter or spoil the steady-state number depending on iteration count.

Concurrency is measured with real threads, because the interesting failure is
several plugin instances competing for the same cores. Each thread gets its
own model and input — sharing them would measure a different program.
"""
from __future__ import annotations

import json
import os
import platform
import statistics
import subprocess
import sys
import threading
import time
from dataclasses import asdict, dataclass

import torch

from ml_v3.runtime_feasibility.envelope import (
    ANALYSIS_HOP_SECONDS,
    FEATURES_PER_FRAME,
    LAB_ENVELOPES,
    RuntimeEnvelope,
    margin_verdict,
)
from ml_v3.runtime_feasibility.surrogate import (
    build_surrogate,
    count_parameters,
    make_input,
)

__all__ = [
    "BenchmarkResult",
    "benchmark_envelope",
    "environment",
    "percentiles",
    "run_all",
]


def percentiles(samples: list[float]) -> dict[str, float]:
    """p50/p95/p99/max plus min, on an already-collected sample.

    Uses nearest-rank on the sorted sample: with a few thousand iterations the
    interpolation choice is immaterial, and nearest-rank never invents a value
    that was not observed.
    """
    if not samples:
        raise ValueError("no samples")
    ordered = sorted(samples)
    count = len(ordered)

    def at(quantile: float) -> float:
        rank = max(1, min(count, int(-(-quantile * count // 1))))
        return ordered[rank - 1]

    return {
        "min": ordered[0],
        "p50": at(0.50),
        "p95": at(0.95),
        "p99": at(0.99),
        "max": ordered[-1],
        "mean": statistics.fmean(ordered),
    }


def _core_topology() -> dict[str, object]:
    """Physical/performance/efficiency core counts, where the OS exposes them.

    An asymmetric machine matters: a thread landing on an efficiency core is
    several times slower, so a concurrency sweep that ignores the split
    reports contention that is really scheduling.
    """
    topology: dict[str, object] = {}
    for label, key in (
        ("physical", "hw.physicalcpu"),
        ("logical", "hw.logicalcpu"),
        ("performance", "hw.perflevel0.physicalcpu"),
        ("efficiency", "hw.perflevel1.physicalcpu"),
    ):
        try:
            result = subprocess.run(
                ["sysctl", "-n", key], capture_output=True, text=True,
                check=False, timeout=5)
            topology[label] = int(result.stdout.strip())
        except (OSError, ValueError, subprocess.SubprocessError):
            topology[label] = None
    return topology


def load_average() -> float | None:
    """1-minute load. A benchmark that omits it is not reproducible.

    The first sweep of this harness was run at load ~100 on 8 cores and its
    multi-instance numbers measured that load, not the model. Recording it is
    the cheapest way to stop a future reader trusting a contended sample.
    """
    try:
        return round(os.getloadavg()[0], 2)
    except (OSError, AttributeError):
        return None


def environment() -> dict[str, object]:
    """What the numbers describe. They prove nothing about other machines."""
    return {
        "python": sys.version.split()[0],
        "implementation": platform.python_implementation(),
        "os": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "cpu_topology": _core_topology(),
        "torch": torch.__version__,
        "torch_threads": torch.get_num_threads(),
        "torch_interop_threads": torch.get_num_interop_threads(),
        "build_mode": "python_eager_no_jit",
    }


@dataclass(frozen=True)
class BenchmarkResult:
    envelope: str
    kind: str
    instances: int
    parameters: int
    iterations: int
    warmup_iterations: int
    warmup_seconds: float
    seconds: dict[str, float]
    hop_seconds: float
    p99_fraction_of_hop: float
    verdict: str
    input_shape: tuple[int, ...]
    output_shape: tuple[int, ...]
    load_average_before: float | None
    load_average_after: float | None


def _timed_loop(model, sample, iterations: int) -> list[float]:
    out: list[float] = []
    with torch.no_grad():
        for _ in range(iterations):
            start = time.perf_counter()
            model(sample)
            out.append(time.perf_counter() - start)
    return out


def benchmark_envelope(
    envelope: RuntimeEnvelope,
    *,
    iterations: int = 300,
    warmup: int = 40,
    instances: int = 1,
    seed: int = 20260810,
    threads: int = 1,
) -> BenchmarkResult:
    """Measure one envelope at one concurrency level.

    ``threads`` pins intra-op threads so that a concurrency sweep varies only
    the number of instances. Leaving torch free to grab every core would make
    1 instance look artificially good and N instances artificially bad.
    """
    if iterations <= 0 or warmup < 0 or instances <= 0:
        raise ValueError("iterations>0, warmup>=0, instances>0 required")
    torch.set_num_threads(threads)
    load_before = load_average()

    models = [build_surrogate(envelope, seed=seed + i) for i in range(instances)]
    samples = [make_input(envelope, seed=seed + 1000 + i) for i in range(instances)]

    warm_start = time.perf_counter()
    with torch.no_grad():
        for model, sample in zip(models, samples):
            for _ in range(warmup):
                model(sample)
    warm_seconds = time.perf_counter() - warm_start

    collected: list[list[float]] = [[] for _ in range(instances)]
    if instances == 1:
        collected[0] = _timed_loop(models[0], samples[0], iterations)
    else:
        barrier = threading.Barrier(instances)

        def worker(index: int) -> None:
            barrier.wait()
            collected[index] = _timed_loop(
                models[index], samples[index], iterations)

        workers = [
            threading.Thread(target=worker, args=(index,))
            for index in range(instances)
        ]
        for thread in workers:
            thread.start()
        for thread in workers:
            thread.join()

    flat = [value for run in collected for value in run]
    stats = percentiles(flat)
    hop = float(ANALYSIS_HOP_SECONDS)
    with torch.no_grad():
        output_shape = tuple(models[0](samples[0]).shape)

    return BenchmarkResult(
        envelope=envelope.name,
        kind=envelope.kind,
        instances=instances,
        parameters=count_parameters(models[0]),
        iterations=iterations,
        warmup_iterations=warmup,
        warmup_seconds=warm_seconds,
        seconds=stats,
        hop_seconds=hop,
        p99_fraction_of_hop=stats["p99"] / hop,
        verdict=margin_verdict(stats["p99"]),
        input_shape=tuple(samples[0].shape),
        output_shape=output_shape,
        load_average_before=load_before,
        load_average_after=load_average(),
    )


def run_all(
    *,
    iterations: int = 300,
    warmup: int = 40,
    instance_counts: tuple[int, ...] = (1, 4, 8, 16),
) -> dict[str, object]:
    results = [
        asdict(benchmark_envelope(
            envelope, iterations=iterations, warmup=warmup, instances=count))
        for envelope in LAB_ENVELOPES
        for count in instance_counts
    ]
    return {
        "schema": "aieq-v3-runtime-feasibility-benchmark-1",
        "authority_status": "LAB_ONLY_NOT_NORMATIVE_NO_RT_PROOF",
        "environment": environment(),
        "features_per_frame": FEATURES_PER_FRAME,
        "hop_seconds": float(ANALYSIS_HOP_SECONDS),
        "hop_exact": str(ANALYSIS_HOP_SECONDS),
        "results": results,
    }


if __name__ == "__main__":
    print(json.dumps(run_all(), indent=2, sort_keys=True))
