"""Deterministic public-surface fixtures for the 17 REV8 O-09 group caps.

The fixtures are evidence-only.  They materialize cap-1, cap and cap+1 on
the same AP, Coverage and Spearman inputs consumed by the public candidate
evaluators.  Every constituent graph remains within the active per-subgraph
A1 ceilings; only the group aggregate named by a fixture is under test.
Some cap+1 cases necessarily exceed a second group aggregate as a
mathematical consequence (for example AP predictions and local thresholds),
but the named reason must always be present.
"""
from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction
from functools import lru_cache
from typing import Literal

from ml_v3.benchmark.rev8_o09_candidate import CandidateGraph, ExactEdge
from ml_v3.benchmark.rev8_o09_group_candidate import (
    AP_MAX_DISTINCT_THRESHOLDS,
    AP_MAX_LOCAL_THRESHOLDS,
    AP_MAX_PARTITIONS,
    AP_MAX_PREFIX_EDGE_INCIDENCE,
    AP_MAX_TOTAL_EDGES,
    AP_MAX_TOTAL_GT,
    AP_MAX_TOTAL_PREDICTIONS,
    COVERAGE_MAX_PARTITIONS_PER_UNIT,
    COVERAGE_MAX_TOTAL_EDGES,
    COVERAGE_MAX_TOTAL_GT,
    COVERAGE_MAX_TOTAL_PREDICTIONS,
    COVERAGE_MAX_UNITS,
    SPEARMAN_MAX_PARTITIONS,
    SPEARMAN_MAX_SUPPORT,
    SPEARMAN_MAX_TOTAL_EDGES,
    SPEARMAN_MAX_TOTAL_GT,
    SPEARMAN_MAX_TOTAL_PREDICTIONS,
    APPartition,
    CoveragePartition,
    GroupPreflightProbe,
    GroupResult,
    SpearmanPartition,
    evaluate_group_ap,
    evaluate_group_coverage,
    evaluate_group_spearman,
)

Surface = Literal["AP", "COVERAGE", "SPEARMAN"]
BoundaryRelation = Literal["under", "on", "over"]
Partitions = (
    tuple[APPartition, ...]
    | tuple[CoveragePartition, ...]
    | tuple[SpearmanPartition, ...]
)


@dataclass(frozen=True)
class GroupCapSpec:
    fixture_id: str
    surface: Surface
    cap: int
    exceed_tag: str


@dataclass(frozen=True)
class GroupCapFixture:
    spec: GroupCapSpec
    relation: BoundaryRelation
    value: int
    partitions: Partitions


GROUP_CAP_SPECS = (
    GroupCapSpec("AP-01", "AP", AP_MAX_PARTITIONS, "GROUP_AP_PARTITIONS"),
    GroupCapSpec("AP-02", "AP", AP_MAX_TOTAL_GT, "GROUP_AP_GT"),
    GroupCapSpec(
        "AP-03", "AP", AP_MAX_TOTAL_PREDICTIONS,
        "GROUP_AP_PREDICTIONS",
    ),
    GroupCapSpec(
        "AP-04", "AP", AP_MAX_TOTAL_EDGES,
        "GROUP_AP_ELIGIBLE_EDGES",
    ),
    GroupCapSpec(
        "AP-05", "AP", AP_MAX_DISTINCT_THRESHOLDS,
        "GROUP_AP_DISTINCT_THRESHOLDS",
    ),
    GroupCapSpec(
        "AP-06", "AP", AP_MAX_LOCAL_THRESHOLDS,
        "GROUP_AP_LOCAL_THRESHOLDS",
    ),
    GroupCapSpec(
        "AP-07", "AP", AP_MAX_PREFIX_EDGE_INCIDENCE,
        "GROUP_AP_PREFIX_EDGE_INCIDENCE",
    ),
    GroupCapSpec(
        "CV-01", "COVERAGE", COVERAGE_MAX_UNITS,
        "GROUP_COVERAGE_UNITS",
    ),
    GroupCapSpec(
        "CV-02", "COVERAGE", COVERAGE_MAX_PARTITIONS_PER_UNIT,
        "GROUP_COVERAGE_PARTITIONS_PER_UNIT",
    ),
    GroupCapSpec(
        "CV-03", "COVERAGE", COVERAGE_MAX_TOTAL_GT,
        "GROUP_COVERAGE_GT",
    ),
    GroupCapSpec(
        "CV-04", "COVERAGE", COVERAGE_MAX_TOTAL_PREDICTIONS,
        "GROUP_COVERAGE_PREDICTIONS",
    ),
    GroupCapSpec(
        "CV-05", "COVERAGE", COVERAGE_MAX_TOTAL_EDGES,
        "GROUP_COVERAGE_ELIGIBLE_EDGES",
    ),
    GroupCapSpec(
        "SP-01", "SPEARMAN", SPEARMAN_MAX_PARTITIONS,
        "GROUP_SPEARMAN_PARTITIONS",
    ),
    GroupCapSpec(
        "SP-02", "SPEARMAN", SPEARMAN_MAX_TOTAL_GT,
        "GROUP_SPEARMAN_GT",
    ),
    GroupCapSpec(
        "SP-03", "SPEARMAN", SPEARMAN_MAX_TOTAL_PREDICTIONS,
        "GROUP_SPEARMAN_PREDICTIONS",
    ),
    GroupCapSpec(
        "SP-04", "SPEARMAN", SPEARMAN_MAX_TOTAL_EDGES,
        "GROUP_SPEARMAN_ELIGIBLE_EDGES",
    ),
    GroupCapSpec(
        "SP-05", "SPEARMAN", SPEARMAN_MAX_SUPPORT,
        "GROUP_SPEARMAN_SUPPORT",
    ),
)

_SPECS_BY_ID = {spec.fixture_id: spec for spec in GROUP_CAP_SPECS}


def _edge(gt: int, prediction: int) -> ExactEdge:
    pair = f"{gt:03d}:{prediction:03d}".encode("ascii")
    return ExactEdge(
        gt=gt,
        prediction=prediction,
        k2_iou=Fraction(1),
        k3_tick_error=0,
        k4_cost=Fraction(0),
        scientific_key=b"s:" + pair,
        diagnostic_key=b"d:" + pair,
    )


def _graph(
    gt_count: int,
    prediction_count: int,
    edges: tuple[ExactEdge, ...] = (),
) -> CandidateGraph:
    return CandidateGraph(gt_count, prediction_count, "additive", edges)


@lru_cache(maxsize=1)
def _dense_edges_128() -> tuple[ExactEdge, ...]:
    return tuple(
        _edge(gt, prediction)
        for gt in range(128)
        for prediction in range(128)
    )


def _edge_graphs(value: int) -> tuple[CandidateGraph, ...]:
    if value not in (
        AP_MAX_TOTAL_EDGES - 1,
        AP_MAX_TOTAL_EDGES,
        AP_MAX_TOTAL_EDGES + 1,
    ):
        raise ValueError("edge fixture value must be cap-1, cap or cap+1")
    dense = _dense_edges_128()
    first = _graph(128, 128, dense if value >= len(dense) else dense[1:])
    if value <= len(dense):
        return (first,)
    return (first, _graph(1, 1, (_edge(0, 0),)))


def _chunks(total: int, maximum: int = 128) -> tuple[int, ...]:
    quotient, remainder = divmod(total, maximum)
    return (maximum,) * quotient + ((remainder,) if remainder else ())


def _relation_value(spec: GroupCapSpec, relation: BoundaryRelation) -> int:
    return spec.cap + {"under": -1, "on": 0, "over": 1}[relation]


def _ap_shape(
    *,
    gt_total: int = 0,
    prediction_total: int = 0,
) -> tuple[APPartition, ...]:
    sizes = _chunks(gt_total or prediction_total)
    rows: list[APPartition] = []
    offset = 0
    for index, size in enumerate(sizes):
        gt_count = size if gt_total else 0
        prediction_count = size if prediction_total else 0
        confidences = tuple(
            Fraction((offset + local) % 1024 + 1, 2048)
            for local in range(prediction_count)
        )
        rows.append(APPartition(
            (f"ap-shape-{index:03d}",),
            _graph(gt_count, prediction_count),
            confidences,
        ))
        offset += size
    return tuple(rows)


def _ap_distinct(value: int, *, reuse_locally: bool) -> tuple[APPartition, ...]:
    rows: list[APPartition] = []
    offset = 0
    for index, size in enumerate(_chunks(value)):
        confidences = tuple(
            Fraction(
                (local if reuse_locally else offset + local) + 1,
                2048,
            )
            for local in range(size)
        )
        rows.append(APPartition(
            (f"ap-threshold-{index:03d}",),
            _graph(0, size),
            confidences,
        ))
        offset += size
    return tuple(rows)


def _ap_prefix(value: int) -> tuple[APPartition, ...]:
    dense = _dense_edges_128()
    confidences = tuple(Fraction(index + 1, 128) for index in range(128))
    first_edges = dense[1:] if value < AP_MAX_PREFIX_EDGE_INCIDENCE else dense
    rows = [APPartition(
        ("ap-prefix-000",),
        _graph(128, 128, first_edges),
        confidences,
    )]
    if value > AP_MAX_PREFIX_EDGE_INCIDENCE:
        rows.append(APPartition(
            ("ap-prefix-001",),
            _graph(1, 1, (_edge(0, 0),)),
            (Fraction(1),),
        ))
    return tuple(rows)


def _coverage_rows(
    graphs: tuple[CandidateGraph, ...],
    *,
    one_unit: bool = False,
    partitions_per_unit: int = 8,
) -> tuple[CoveragePartition, ...]:
    return tuple(
        CoveragePartition(
            ("coverage-unit",) if one_unit else (
                f"coverage-unit-{index // partitions_per_unit:03d}",
            ),
            (f"coverage-partition-{index:03d}",),
            candidate,
            (False,) * candidate.gt_count,
            (False,) * candidate.prediction_count,
        )
        for index, candidate in enumerate(graphs)
    )


def _coverage_shape(
    *,
    gt_total: int = 0,
    prediction_total: int = 0,
) -> tuple[CoveragePartition, ...]:
    sizes = _chunks(gt_total or prediction_total)
    return _coverage_rows(tuple(
        _graph(size if gt_total else 0, size if prediction_total else 0)
        for size in sizes
    ))


def _spearman_rows(
    graphs: tuple[CandidateGraph, ...],
) -> tuple[SpearmanPartition, ...]:
    return tuple(
        SpearmanPartition(
            (f"spearman-partition-{index:03d}",),
            candidate,
            (Fraction(0),) * candidate.gt_count,
            (Fraction(0),) * candidate.prediction_count,
        )
        for index, candidate in enumerate(graphs)
    )


def _spearman_shape(
    *,
    gt_total: int = 0,
    prediction_total: int = 0,
) -> tuple[SpearmanPartition, ...]:
    sizes = _chunks(gt_total or prediction_total)
    return _spearman_rows(tuple(
        _graph(size if gt_total else 0, size if prediction_total else 0)
        for size in sizes
    ))


def build_group_cap_fixture(
    fixture_id: str,
    relation: BoundaryRelation,
) -> GroupCapFixture:
    """Build one cap-1/cap/cap+1 public evaluator input."""
    try:
        spec = _SPECS_BY_ID[fixture_id]
    except KeyError as error:
        raise ValueError(f"unknown group cap fixture {fixture_id!r}") from error
    value = _relation_value(spec, relation)

    if fixture_id == "AP-01":
        partitions: Partitions = tuple(
            APPartition((f"ap-partition-{index:03d}",), _graph(0, 0), ())
            for index in range(value)
        )
    elif fixture_id == "AP-02":
        partitions = _ap_shape(gt_total=value)
    elif fixture_id == "AP-03":
        partitions = _ap_shape(prediction_total=value)
    elif fixture_id == "AP-04":
        partitions = tuple(
            APPartition((f"ap-edge-{index:03d}",), candidate,
                        (Fraction(1),) * candidate.prediction_count)
            for index, candidate in enumerate(_edge_graphs(value))
        )
    elif fixture_id == "AP-05":
        partitions = _ap_distinct(value, reuse_locally=False)
    elif fixture_id == "AP-06":
        partitions = _ap_distinct(value, reuse_locally=True)
    elif fixture_id == "AP-07":
        partitions = _ap_prefix(value)
    elif fixture_id == "CV-01":
        partitions = _coverage_rows(
            tuple(_graph(0, 0) for _ in range(value)),
            partitions_per_unit=1,
        )
    elif fixture_id == "CV-02":
        partitions = _coverage_rows(
            tuple(_graph(0, 0) for _ in range(value)),
            one_unit=True,
        )
    elif fixture_id == "CV-03":
        partitions = _coverage_shape(gt_total=value)
    elif fixture_id == "CV-04":
        partitions = _coverage_shape(prediction_total=value)
    elif fixture_id == "CV-05":
        partitions = _coverage_rows(_edge_graphs(value), one_unit=True)
    elif fixture_id == "SP-01":
        partitions = _spearman_rows(tuple(_graph(0, 0) for _ in range(value)))
    elif fixture_id == "SP-02":
        partitions = _spearman_shape(gt_total=value)
    elif fixture_id == "SP-03":
        partitions = _spearman_shape(prediction_total=value)
    elif fixture_id == "SP-04":
        partitions = _spearman_rows(_edge_graphs(value))
    elif fixture_id == "SP-05":
        partitions = _spearman_rows(tuple(
            _graph(size, size) for size in _chunks(value)
        ))
    else:  # pragma: no cover - guarded by the frozen registry above.
        raise AssertionError(f"unhandled group cap fixture {fixture_id}")
    return GroupCapFixture(spec, relation, value, partitions)


def evaluate_group_cap_fixture(fixture: GroupCapFixture) -> GroupResult[object]:
    """Evaluate a fixture through its public candidate surface."""
    if fixture.spec.surface == "AP":
        return evaluate_group_ap(fixture.partitions)  # type: ignore[arg-type]
    if fixture.spec.surface == "COVERAGE":
        return evaluate_group_coverage(  # type: ignore[arg-type]
            fixture.partitions)
    return evaluate_group_spearman(fixture.partitions)  # type: ignore[arg-type]


def observed_group_cap_value(
    fixture: GroupCapFixture,
    probe: GroupPreflightProbe,
) -> int:
    """Read the named aggregate from the public evaluator's completed probe."""
    attribute_by_tag = {
        "GROUP_AP_PARTITIONS": "partition_count",
        "GROUP_AP_GT": "total_gt",
        "GROUP_AP_PREDICTIONS": "total_predictions",
        "GROUP_AP_ELIGIBLE_EDGES": "total_eligible_edges",
        "GROUP_AP_DISTINCT_THRESHOLDS": "distinct_thresholds",
        "GROUP_AP_LOCAL_THRESHOLDS": "local_thresholds",
        "GROUP_AP_PREFIX_EDGE_INCIDENCE": "prefix_edge_incidence",
        "GROUP_COVERAGE_UNITS": "unit_count",
        "GROUP_COVERAGE_PARTITIONS_PER_UNIT": (
            "maximum_partitions_per_unit"
        ),
        "GROUP_COVERAGE_GT": "total_gt",
        "GROUP_COVERAGE_PREDICTIONS": "total_predictions",
        "GROUP_COVERAGE_ELIGIBLE_EDGES": "total_eligible_edges",
        "GROUP_SPEARMAN_PARTITIONS": "partition_count",
        "GROUP_SPEARMAN_GT": "total_gt",
        "GROUP_SPEARMAN_PREDICTIONS": "total_predictions",
        "GROUP_SPEARMAN_ELIGIBLE_EDGES": "total_eligible_edges",
        "GROUP_SPEARMAN_SUPPORT": "maximum_matching_support",
    }
    return int(getattr(probe, attribute_by_tag[fixture.spec.exceed_tag]))
