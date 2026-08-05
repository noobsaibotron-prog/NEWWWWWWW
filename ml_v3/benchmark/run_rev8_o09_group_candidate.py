"""Run isolated evidence-only benchmarks for REV8 O-09 group surfaces.

This runner measures the candidate AP, B-001 coverage and Spearman group
primitives.  Every subgraph consumes the signed active A1 preflight; the 17
group-level caps remain evidence-only and cannot close O-09.  In particular,
general variable-value-marginal Spearman, published ``rho64``,
``G_eligible``/``G_defined``/``G_NA`` and gate floors remain explicit
activation blockers.

Every timed workload runs in a fresh ``-I -S -B`` child process.  The timer
covers the group evaluation (or macro math reduction), while peak RSS covers
the complete worker through scientific-result materialization.
"""
from __future__ import annotations

import argparse
from dataclasses import fields, is_dataclass
from enum import Enum
from fractions import Fraction
import hashlib
from importlib.machinery import EXTENSION_SUFFIXES, SourceFileLoader
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

import numpy as np

from ml_v3.benchmark import rev8_o09_group_candidate as group_subject
from ml_v3.benchmark.rev8_o09_candidate import CandidateGraph, ExactEdge
from ml_v3.benchmark.rev8_o09_group_cap_fixtures import (
    GROUP_CAP_SPECS,
    build_group_cap_fixture,
    observed_group_cap_value,
)
from ml_v3.benchmark.rev8_o09_group_candidate import (
    AP_MAX_DISTINCT_THRESHOLDS,
    BINARY64_EXACT_RATIONAL_BIT_BOUND,
    COVERAGE_MAX_PARTITIONS_PER_UNIT,
    GROUP_CANDIDATE_BALLOT_READY,
    GROUP_CANDIDATE_LIMITATIONS,
    PROVISIONAL_MAX_EXACT_SCALAR_BITS,
    _GROUP_A1_EVIDENCE_AUTHORITY_STATUS,
    APPartition,
    CoveragePartition,
    GroupReason,
    GroupResult,
    GroupStatus,
    SpearmanPartition,
    evaluate_group_ap,
    evaluate_group_coverage,
    evaluate_group_spearman,
    reduce_macro_average_precision_math,
    reduce_macro_coverage_math,
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
from ml_v3.contracts.numeric_authority_v2 import exact_n64, rn64
from ml_v3.contracts.normalize_v2 import normalized_canonical_bytes

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_SCHEMA = "aieq-v3-rev8-o09-group-candidate-benchmark-3"
AUTHORITY_STATUS = _GROUP_A1_EVIDENCE_AUTHORITY_STATUS
BOOTSTRAP_PATH = (
    ROOT / "ml_v3/benchmark/rev8_o09_group_isolated_bootstrap.py"
)
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
    "ml_v3/benchmark/rev8_o09_group_candidate.py",
    "ml_v3/benchmark/rev8_o09_group_cap_fixtures.py",
    "ml_v3/benchmark/rev8_o09_group_isolated_bootstrap.py",
    "ml_v3/benchmark/run_rev8_o09_group_candidate.py",
    "ml_v3/tests/test_g1c_rev8_o09_candidate.py",
    "ml_v3/tests/test_g1c_rev8_o09_group_candidate.py",
    "ml_v3/tests/test_g1c_rev8_o09_group_runner.py",
    "ml_v3/contracts/numeric_authority_v2.py",
    "ml_v3/contracts/normalize_v2.py",
)
HASHED_PATHS = (*PROTECTED_PATHS, *FROZEN_O_PATHS, *PROVENANCE_PATHS)

FULL_WORKLOADS: tuple[tuple[str, int], ...] = (
    ("ap_distinct_diagonal", 16),
    ("ap_distinct_diagonal", 64),
    ("ap_distinct_diagonal", 128),
    ("ap_atomic_dense", 8),
    ("ap_atomic_dense", 16),
    ("ap_atomic_dense", 64),
    ("ap_atomic_dense", 128),
    ("coverage_unique", 16),
    ("coverage_unique", 64),
    ("coverage_unique", 128),
    ("coverage_ambiguous", 8),
    ("coverage_ambiguous", 16),
    ("coverage_ambiguous", 32),
    ("spearman_unique", 10),
    ("spearman_unique", 64),
    ("spearman_unique", 128),
    ("spearman_fixed_marginal", 10),
    ("spearman_fixed_marginal", 64),
    ("spearman_fixed_marginal", 128),
    ("spearman_ambiguous", 10),
    ("spearman_ambiguous", 16),
    ("spearman_variable_unavailable", 10),
    ("spearman_variable_unavailable", 64),
    ("ap_macro_30", 1),
    ("coverage_macro_30", 1),
    ("cap_boundaries", 0),
)
SMOKE_WORKLOADS: tuple[tuple[str, int], ...] = (
    ("ap_distinct_diagonal", 16),
    ("ap_atomic_dense", 8),
    ("coverage_unique", 16),
    ("coverage_ambiguous", 8),
    ("spearman_unique", 10),
    ("spearman_fixed_marginal", 10),
    ("spearman_ambiguous", 10),
    ("spearman_variable_unavailable", 10),
    ("ap_macro_30", 1),
    ("coverage_macro_30", 1),
    ("cap_boundaries", 0),
)


def _edge(
    gt: int,
    prediction: int,
    *,
    scientific: bytes | None = None,
    diagnostic: bytes | None = None,
) -> ExactEdge:
    return ExactEdge(
        gt,
        prediction,
        Fraction(1),
        0,
        Fraction(0),
        scientific or f"s:{gt:04}:{prediction:04}".encode(),
        diagnostic or f"d:{gt:04}:{prediction:04}".encode(),
    )


def _graph(
    gt_count: int,
    prediction_count: int,
    edges: tuple[ExactEdge, ...] | list[ExactEdge],
) -> CandidateGraph:
    return CandidateGraph(
        gt_count, prediction_count, "additive", tuple(edges)
    )


def _diagonal_graph(size: int) -> CandidateGraph:
    return _graph(size, size, [_edge(index, index) for index in range(size)])


def _dense_graph(size: int) -> CandidateGraph:
    return _graph(
        size,
        size,
        [
            _edge(
                gt,
                prediction,
                scientific=b"same-scientific-key",
            )
            for gt in range(size)
            for prediction in range(size)
        ],
    )


def _confidence(index: int, size: int) -> Fraction:
    return exact_n64(rn64(Fraction(size - index, size)))


def _scientific(value: object) -> object:
    if isinstance(value, Fraction):
        return [hex(value.numerator), hex(value.denominator)]
    if isinstance(value, float):
        return {"binary64_hex": value.hex()}
    if isinstance(value, Enum):
        return value.value
    if is_dataclass(value):
        return {
            field.name: _scientific(getattr(value, field.name))
            for field in fields(value)
        }
    if isinstance(value, tuple):
        return [_scientific(item) for item in value]
    if isinstance(value, list):
        return [_scientific(item) for item in value]
    if isinstance(value, dict):
        return {
            str(key): _scientific(item)
            for key, item in sorted(
                value.items(), key=lambda item: str(item[0]).encode("utf-8")
            )
        }
    if isinstance(value, (str, int, bool)) or value is None:
        return value
    raise TypeError(f"unsupported scientific evidence type: {type(value)!r}")


def _measure_call(kind: str, size: int) -> tuple[object, dict[str, Any]]:
    if kind == "ap_distinct_diagonal":
        subject = (
            APPartition(
                ("ap-distinct",),
                _diagonal_graph(size),
                tuple(_confidence(index, size) for index in range(size)),
            ),
        )
        call = lambda: evaluate_group_ap(subject)
    elif kind == "ap_atomic_dense":
        subject = (
            APPartition(
                ("ap-atomic",),
                _dense_graph(size),
                (Fraction(1, 2),) * size,
            ),
        )
        call = lambda: evaluate_group_ap(subject)
    elif kind == "coverage_unique":
        subject = (
            CoveragePartition(
                ("unit",),
                ("coverage-unique",),
                _diagonal_graph(size),
                (True,) * size,
                tuple(index % 2 == 0 for index in range(size)),
            ),
        )
        call = lambda: evaluate_group_coverage(subject)
    elif kind == "coverage_ambiguous":
        subject = (
            CoveragePartition(
                ("unit",),
                ("coverage-ambiguous",),
                _dense_graph(size),
                tuple(index < size // 2 for index in range(size)),
                tuple(index < size // 2 for index in range(size)),
            ),
        )
        call = lambda: evaluate_group_coverage(subject)
    elif kind == "spearman_unique":
        values = tuple(Fraction(index) for index in range(size))
        subject = (
            SpearmanPartition(
                ("spearman-unique",),
                _diagonal_graph(size),
                values,
                values,
            ),
        )
        call = lambda: evaluate_group_spearman(subject)
    elif kind == "spearman_fixed_marginal":
        edges = [_edge(index, index) for index in range(size)]
        for index in range(0, size - 1, 2):
            edges.extend((_edge(index, index + 1), _edge(index + 1, index)))
        gt_values = tuple(Fraction(index) for index in range(size))
        prediction_values = tuple(
            Fraction(index - (index % 2)) for index in range(size)
        )
        subject = (
            SpearmanPartition(
                ("spearman-fixed",),
                _graph(size, size, edges),
                gt_values,
                prediction_values,
            ),
        )
        call = lambda: evaluate_group_spearman(subject)
    elif kind == "spearman_ambiguous":
        values = tuple(Fraction(index) for index in range(size))
        subject = (
            SpearmanPartition(
                ("spearman-ambiguous",),
                _dense_graph(size),
                values,
                values,
            ),
        )
        call = lambda: evaluate_group_spearman(subject)
    elif kind == "spearman_variable_unavailable":
        values = tuple(Fraction(index) for index in range(size))
        edges = [_edge(index, index) for index in range(size)]
        edges.append(_edge(0, size))
        subject = (
            SpearmanPartition(
                ("spearman-variable",),
                _graph(size, size + 1, edges),
                values,
                (*values, Fraction(size * 100)),
            ),
        )
        call = lambda: evaluate_group_spearman(subject)
    elif kind == "ap_macro_30":
        one = evaluate_group_ap(
            (
                APPartition(
                    ("macro-ap",), _diagonal_graph(3), (Fraction(1, 2),) * 3
                ),
            )
        )
        if one.status is not GroupStatus.CERTIFIED or one.value is None:
            raise RuntimeError("AP macro seed did not certify")
        subject = tuple(
            (f"group-{index:02}", one.value) for index in range(30)
        )
        call = lambda: reduce_macro_average_precision_math(subject)
    elif kind == "coverage_macro_30":
        one = evaluate_group_coverage(
            (
                CoveragePartition(
                    ("macro-unit",),
                    ("macro-coverage",),
                    _diagonal_graph(3),
                    (True, True, True),
                    (True, False, True),
                ),
            )
        )
        if one.status is not GroupStatus.CERTIFIED or one.value is None:
            raise RuntimeError("coverage macro seed did not certify")
        subject = tuple(
            (f"group-{index:02}", one.value) for index in range(30)
        )
        call = lambda: reduce_macro_coverage_math(subject)
    elif kind == "cap_boundaries":
        call = _measure_cap_boundaries
    else:
        raise ValueError(f"unknown group workload {kind!r}")

    wall_start = perf_counter()
    cpu_start = process_time()
    result = call()
    cpu_seconds = process_time() - cpu_start
    wall_seconds = perf_counter() - wall_start
    return result, {
        "wall_seconds": wall_seconds,
        "cpu_seconds": cpu_seconds,
    }


def _preflight_exceeded_for_evidence(
    result: GroupResult[Any],
) -> list[str] | None:
    """Serialize structural exceedances without fabricating a failed probe."""
    if result.preflight is None:
        return None
    return list(result.preflight.provisional_exceeded)


def _measure_cap_boundaries() -> dict[str, object]:
    cases: list[dict[str, object]] = []
    relation_offsets = (("under", -1), ("on", 0), ("over", 1))
    for spec in GROUP_CAP_SPECS:
        for relation, offset in relation_offsets:
            fixture = build_group_cap_fixture(
                spec.fixture_id, relation)  # type: ignore[arg-type]
            solve_calls: list[str] = []

            def cardinality(*_args: object, **_kwargs: object) -> int:
                solve_calls.append("exact_maximum_cardinality")
                return 0

            def profile(
                *_args: object,
                **_kwargs: object,
            ) -> tuple[tuple[int, int], ...]:
                solve_calls.append("_solve_profile")
                return ()

            def profile_details(
                *_args: object,
                **_kwargs: object,
            ) -> tuple[tuple[tuple[int, int], ...], bool]:
                solve_calls.append("_solve_profile_details")
                return (), False

            originals = (
                group_subject.exact_maximum_cardinality,
                group_subject._solve_profile,  # type: ignore[attr-defined]
                group_subject._solve_profile_details,  # type: ignore[attr-defined]
            )
            try:
                group_subject.exact_maximum_cardinality = cardinality
                group_subject._solve_profile = profile  # type: ignore[attr-defined]
                group_subject._solve_profile_details = (  # type: ignore[attr-defined]
                    profile_details
                )
                if spec.surface == "AP":
                    result = evaluate_group_ap(  # type: ignore[arg-type]
                        fixture.partitions)
                elif spec.surface == "COVERAGE":
                    result = evaluate_group_coverage(  # type: ignore[arg-type]
                        fixture.partitions)
                else:
                    result = evaluate_group_spearman(  # type: ignore[arg-type]
                        fixture.partitions)
            finally:
                group_subject.exact_maximum_cardinality = originals[0]
                group_subject._solve_profile = originals[1]  # type: ignore[attr-defined]
                group_subject._solve_profile_details = (  # type: ignore[attr-defined]
                    originals[2]
                )
            expected = spec.cap + offset
            observed = (
                observed_group_cap_value(fixture, result.preflight)
                if result.preflight is not None
                else None
            )
            target_exceeded = bool(
                result.preflight is not None
                and spec.exceed_tag
                in result.preflight.provisional_exceeded
            )
            if result.preflight is not None:
                if (
                    observed != expected
                    or target_exceeded != (relation == "over")
                ):
                    raise RuntimeError(
                        f"cap fixture {spec.fixture_id}/{relation} mismatch: "
                        f"observed={observed}, expected={expected}, "
                        f"target_exceeded={target_exceeded}"
                    )
                if relation == "over" and (
                    result.status is not GroupStatus.REJECTED
                    or result.reason
                    is not GroupReason.SOLVER_STRUCTURAL_LIMIT_EXCEEDED
                    or solve_calls
                ):
                    raise RuntimeError(
                        f"cap fixture {spec.fixture_id}/over did not reject "
                        f"pre-solve: status={result.status.value}, "
                        f"reason={result.reason.value}, calls={solve_calls}"
                    )
            cases.append(
                {
                    "cap_id": spec.fixture_id,
                    "surface": spec.surface,
                    "relation": relation,
                    "tag": spec.exceed_tag,
                    "cap": spec.cap,
                    "value": observed,
                    "status": result.status.value,
                    "reason": result.reason.value,
                    "target_exceeded": target_exceeded,
                    "exceeded": _preflight_exceeded_for_evidence(result),
                    "solver_calls": solve_calls,
                    "solver_not_called": not solve_calls,
                }
            )
    for bits in (
        PROVISIONAL_MAX_EXACT_SCALAR_BITS - 1,
        PROVISIONAL_MAX_EXACT_SCALAR_BITS,
        PROVISIONAL_MAX_EXACT_SCALAR_BITS + 1,
    ):
        # ``rational_bit_length`` counts numerator and denominator.  For the
        # integer 2**(bits-2), those contributions are (bits-1) + 1.
        value = Fraction(1 << (bits - 2))
        graph = _graph(1, 1, (_edge(0, 0),))
        result = evaluate_group_spearman(
            (
                SpearmanPartition(
                    ("scalar-boundary",), graph, (value,), (Fraction(0),)
                ),
            )
        )
        cases.append(
            {
                "cap_id": "TRANSVERSE-EXACT-SCALAR",
                "surface": "EXACT_SCALAR_BITS",
                "relation": (
                    "under" if bits < PROVISIONAL_MAX_EXACT_SCALAR_BITS
                    else "on" if bits == PROVISIONAL_MAX_EXACT_SCALAR_BITS
                    else "over"
                ),
                "tag": "GROUP_EXACT_SCALAR_BIT_LENGTH",
                "cap": PROVISIONAL_MAX_EXACT_SCALAR_BITS,
                "value": bits,
                "status": result.status.value,
                "reason": result.reason.value,
                "target_exceeded": bool(
                    result.preflight is not None
                    and "GROUP_EXACT_SCALAR_BIT_LENGTH"
                    in result.preflight.provisional_exceeded
                ),
                "exceeded": _preflight_exceeded_for_evidence(result),
            }
        )
    return {
        "cases": cases,
        "group_cap_matrix": {
            "row_count": len(GROUP_CAP_SPECS),
            "boundary_case_count": 3 * len(GROUP_CAP_SPECS),
            "durable_under_on_over": True,
            "durable_no_solve_over": True,
        },
        "declared_binary64_exact_rational_bound": (
            BINARY64_EXACT_RATIONAL_BIT_BOUND
        ),
        "ap_max_distinct_thresholds": AP_MAX_DISTINCT_THRESHOLDS,
        "coverage_max_partitions_per_unit": (
            COVERAGE_MAX_PARTITIONS_PER_UNIT
        ),
    }


def _peak_rss_bytes() -> int:
    value = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return int(value if sys.platform == "darwin" else value * 1024)


def _platform() -> dict[str, Any]:
    flags = sys.flags
    if not (
        flags.isolated
        and flags.no_site
        and flags.ignore_environment
        and flags.safe_path
        and sys.dont_write_bytecode
    ):
        raise RuntimeError(
            "O-09 group evidence requires isolated startup with -I -S -B"
        )
    original_argv = tuple(sys.orig_argv)
    if (
        len(original_argv) < 5
        or Path(original_argv[0]).resolve() != Path(sys.executable).resolve()
        or original_argv[1:4] != ("-I", "-S", "-B")
        or Path(original_argv[4]).resolve() != BOOTSTRAP_PATH.resolve()
    ):
        raise RuntimeError(
            "O-09 group evidence requires the exact isolated bootstrap "
            f"invocation; sys.orig_argv={original_argv!r}"
        )
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
    return {
        "actual": actual,
        "lock": lock,
        "startup": {
            "orig_argv": list(original_argv),
            "bootstrap_path": str(BOOTSTRAP_PATH),
            "isolated": bool(flags.isolated),
            "no_site": bool(flags.no_site),
            "ignore_environment": bool(flags.ignore_environment),
            "safe_path": bool(flags.safe_path),
            "dont_write_bytecode": bool(sys.dont_write_bytecode),
        },
    }


def _source_hashes() -> dict[str, str]:
    result: dict[str, str] = {}
    for relative in HASHED_PATHS:
        path = ROOT / relative
        if not path.is_file():
            raise RuntimeError(
                f"required O-09 group provenance file missing: {relative}"
            )
        result[relative] = sha256_of_file(path)
    return result


def _loaded_module_provenance() -> dict[str, dict[str, object]]:
    evidence: dict[str, dict[str, object]] = {}

    def record(label: str, module: object) -> None:
        module_file = getattr(module, "__file__", None)
        if module_file is None:
            raise RuntimeError(f"loaded module has no source path: {label}")
        actual = Path(module_file).resolve()
        try:
            relative = str(actual.relative_to(ROOT))
        except ValueError as error:
            raise RuntimeError(
                f"project module loaded outside repository: {actual}"
            ) from error
        if actual.suffix != ".py":
            raise RuntimeError(
                f"non-source project module loaded for {label}: {actual}"
            )
        loader = getattr(module, "__loader__", None)
        spec = getattr(module, "__spec__", None)
        origin = getattr(spec, "origin", None)
        if not isinstance(loader, SourceFileLoader):
            raise RuntimeError(
                f"non-source loader for project module {label}: {loader!r}"
            )
        if origin is None or Path(origin).resolve() != actual:
            raise RuntimeError(
                f"module spec origin mismatch for {label}: {origin!r}"
            )
        cached = getattr(module, "__cached__", None)
        cached_path = Path(cached).resolve() if cached is not None else None
        if cached_path is not None and cached_path.exists():
            raise RuntimeError(
                f"project bytecode cache exists for {label}: {cached_path}"
            )
        evidence[label] = {
            "path": relative,
            "sha256": sha256_of_file(actual),
            "loader": "SourceFileLoader",
            "spec_origin": relative,
            "cached_path": (
                str(cached_path) if cached_path is not None else "NONE"
            ),
            "cached_exists": False,
        }

    record("runner_entrypoint", sys.modules[__name__])
    for module_name, module in sorted(sys.modules.items()):
        if module_name != "ml_v3" and not module_name.startswith("ml_v3."):
            continue
        if getattr(module, "__file__", None) is not None:
            record(module_name, module)
    return evidence


def _git(*arguments: str) -> str:
    return subprocess.check_output(
        ("git", *arguments), cwd=ROOT, text=True
    ).strip()


def _tree_hygiene(*, require_clean_git: bool) -> None:
    if sys.pycache_prefix is not None:
        raise RuntimeError("group benchmark forbids sys.pycache_prefix")
    for variable in ("PYTHONPYCACHEPREFIX", "PYTHONPATH", "PYTHONHOME"):
        if os.environ.get(variable):
            raise RuntimeError(
                f"group benchmark forbids import environment {variable}"
            )
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
        if path.is_file()
        and any(path.name.endswith(suffix) for suffix in EXTENSION_SUFFIXES)
    )
    if caches or pyc_files or native_shadows:
        raise RuntimeError(
            "group benchmark requires an archive-like source tree; "
            f"caches={caches[:5]}, pyc={pyc_files[:5]}, "
            f"native_shadows={native_shadows[:5]}"
        )
    if require_clean_git:
        status = _git("status", "--porcelain")
        if status:
            raise RuntimeError(
                "group benchmark requires a clean immutable worktree; "
                f"status:\n{status}"
            )


def _external_output_path(path: Path) -> Path:
    resolved = path.expanduser().resolve()
    try:
        resolved.relative_to(ROOT.resolve())
    except ValueError:
        return resolved
    raise RuntimeError(
        f"O-09 group output must be outside the repository: {resolved}"
    )


def _worker() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", required=True)
    parser.add_argument("--size", type=int, required=True)
    args = parser.parse_args()
    platform_evidence = _platform()
    _tree_hygiene(require_clean_git=False)
    source_before = _source_hashes()
    modules_before = _loaded_module_provenance()
    result, measurement = _measure_call(args.worker, args.size)
    scientific = _scientific(result)
    measurement["peak_rss_bytes"] = _peak_rss_bytes()
    source_after = _source_hashes()
    modules_after = _loaded_module_provenance()
    if source_before != source_after:
        raise RuntimeError("group provenance changed during worker")
    if modules_before != modules_after:
        raise RuntimeError("loaded modules changed during worker")
    payload = {
        "kind": args.worker,
        "size": args.size,
        "measurement": measurement,
        "scientific_result": scientific,
        "scientific_result_sha256": hashlib.sha256(
            canonical_bytes(scientific)
        ).hexdigest(),
        "worker_provenance": {
            "platform": platform_evidence,
            "source_sha256": source_after,
            "loaded_modules": modules_after,
        },
    }
    print(json.dumps(payload, sort_keys=True, separators=(",", ":")))


def _run_child(kind: str, size: int) -> dict[str, Any]:
    child_environment = os.environ.copy()
    for variable in ("PYTHONPYCACHEPREFIX", "PYTHONPATH", "PYTHONHOME"):
        child_environment.pop(variable, None)
    child_environment["PYTHONDONTWRITEBYTECODE"] = "1"
    completed = subprocess.run(
        (
            sys.executable,
            "-I",
            "-S",
            "-B",
            str(BOOTSTRAP_PATH),
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
            f"group worker failed for {kind}/{size} "
            f"(exit {completed.returncode}):\n{completed.stderr}"
        )
    return json.loads(completed.stdout)


def _quantiles(values: list[float]) -> dict[str, float]:
    ordered = sorted(values)
    return {
        "min": ordered[0],
        "median": statistics.median(ordered),
        "max": ordered[-1],
    }


def _workloads(profile_name: str) -> tuple[tuple[str, int], ...]:
    if profile_name == "smoke":
        return SMOKE_WORKLOADS
    if profile_name == "full":
        return FULL_WORKLOADS
    raise ValueError(f"unknown profile {profile_name!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", choices=("smoke", "full"), default="full")
    parser.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()
    if args.repeat < 1:
        raise SystemExit("--repeat must be >= 1")
    output_path = _external_output_path(args.output)

    platform_evidence = _platform()
    _tree_hygiene(require_clean_git=True)
    source_start = _source_hashes()
    modules_start = _loaded_module_provenance()
    commit = _git("rev-parse", "HEAD")
    rows: list[dict[str, object]] = []
    for kind, size in _workloads(args.profile):
        runs = [_run_child(kind, size) for _ in range(args.repeat)]
        result_hashes = {
            run["scientific_result_sha256"] for run in runs
        }
        if len(result_hashes) != 1:
            raise RuntimeError(
                f"non-deterministic group result for {kind}/{size}"
            )
        rows.append(
            {
                "kind": kind,
                "size": size,
                "runs": runs,
                "summary": {
                    "wall_seconds": _quantiles(
                        [
                            run["measurement"]["wall_seconds"]
                            for run in runs
                        ]
                    ),
                    "cpu_seconds": _quantiles(
                        [
                            run["measurement"]["cpu_seconds"]
                            for run in runs
                        ]
                    ),
                    "peak_rss_bytes": max(
                        run["measurement"]["peak_rss_bytes"] for run in runs
                    ),
                    "scientific_result_sha256": next(iter(result_hashes)),
                },
            }
        )

    _tree_hygiene(require_clean_git=True)
    source_end = _source_hashes()
    modules_end = _loaded_module_provenance()
    if source_start != source_end:
        raise RuntimeError("group provenance changed during benchmark")
    if modules_start != modules_end:
        raise RuntimeError("parent loaded modules changed during benchmark")
    for row in rows:
        for run in row["runs"]:
            worker = run["worker_provenance"]
            if worker["source_sha256"] != source_end:
                raise RuntimeError("worker source hashes differ from parent")
            if worker["loaded_modules"] != modules_end:
                raise RuntimeError("worker modules differ from parent")

    evidence = {
        "schema": EVIDENCE_SCHEMA,
        "authority_status": AUTHORITY_STATUS,
        "commit": commit,
        "platform": platform_evidence,
        "configuration": {
            "profile": args.profile,
            "repeat": args.repeat,
            "workloads": [list(item) for item in _workloads(args.profile)],
            "external_output_required": True,
            "fresh_isolated_worker_per_run": True,
            "atomic_publish_after_final_hygiene": True,
        },
        "protected_sha256": source_end,
        "loaded_modules": modules_end,
        "workloads": rows,
        "coverage": {
            "group_ap": "MEASURED",
            "b001_group_coverage": "MEASURED",
            "spearman_unique": "MEASURED",
            "spearman_fixed_value_marginal": "MEASURED",
            "spearman_ambiguity_detection": "MEASURED",
            "general_variable_value_marginal_spearman": "NOT_CERTIFIED",
            "spearman_rho64_publication": "NOT_MATERIALIZED",
            "G_eligible_G_defined_G_NA_and_gate_floors": "NOT_EVALUATED",
            "ballot_ready": GROUP_CANDIDATE_BALLOT_READY,
            "limitations": list(GROUP_CANDIDATE_LIMITATIONS),
        },
    }
    evidence["payload_sha256"] = hashlib.sha256(
        canonical_bytes(evidence)
    ).hexdigest()
    temporary = output_path.with_name(
        f".{output_path.name}.{os.getpid()}.tmp"
    )
    if temporary.exists():
        raise RuntimeError(f"stale output temp exists: {temporary}")
    try:
        write_canonical(temporary, evidence)
        _tree_hygiene(require_clean_git=True)
        if _source_hashes() != source_end:
            raise RuntimeError("group provenance changed during publication")
        if _loaded_module_provenance() != modules_end:
            raise RuntimeError("group modules changed during publication")
        os.replace(temporary, output_path)
    finally:
        temporary.unlink(missing_ok=True)
    print(sha256_of_file(output_path))


if __name__ == "__main__":
    if "--worker" in sys.argv:
        _worker()
    else:
        main()
