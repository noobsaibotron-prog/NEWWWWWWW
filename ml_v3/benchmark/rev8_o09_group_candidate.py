"""Candidate-only group-level subjects for the REV8 O-09 benchmark.

This module is deliberately not exported and is not reachable from the live
REV7 evaluator.  It extends the isolated per-subgraph candidate kernel with
the three group-level surfaces that the first O-09 raw benchmark explicitly
did not measure:

* exact group Average Precision with atomic confidence ties;
* the separable B-001 ``coverage_minus``/``coverage_plus`` envelope; and
* exact Spearman singleton certificates for unique/fixed-value-marginal
  optima.

The Spearman implementation is intentionally incomplete and fail-closed.  It
certifies a unique optimum directly, and ambiguous optima only when matched GT
and prediction value multiplicities are proved fixed (perfect matching is the
cheap special case).  Truly variable value marginals return the explicit
``SPEARMAN_CERTIFICATE_UNAVAILABLE`` N/A result rather than being guessed.
Consequently this module is evidence-only and is not ballot-ready.

All limits and preflight results remain diagnostic candidate evidence until a
separate O-09 authority ballot activates cap, scope, fixtures, reason code and
blast radius atomically.
"""
from __future__ import annotations

from dataclasses import dataclass, replace
from enum import Enum
from fractions import Fraction
from typing import Generic, Literal, TypeVar

from ml_v3.benchmark.rev8_o09_candidate import (
    PROVISIONAL_MAX_EXACT_SCALAR_BITS,
    CandidateGraph,
    CandidateGraphError,
    exact_maximum_cardinality,
    provisional_preflight_probe,
    rational_bit_length,
    _solve_profile,
    _solve_profile_details,
)
from ml_v3.contracts.numeric_authority_v2 import (
    NumericAuthorityError,
    exact_n64,
    mean64,
    rn64,
)
from ml_v3.contracts.normalize_v2 import normalized_canonical_bytes

__all__ = [
    "APPartition",
    "APValue",
    "CoveragePartition",
    "CoverageUnitValue",
    "CoverageValue",
    "GroupCandidateError",
    "GROUP_CANDIDATE_BALLOT_READY",
    "GROUP_CANDIDATE_LIMITATIONS",
    "GroupPreflightProbe",
    "GroupReason",
    "GroupResult",
    "GroupStatus",
    "MacroAPReduction",
    "MacroCoverageReduction",
    "RhoIdentity",
    "SpearmanAmbiguityWitness",
    "SpearmanPartition",
    "SpearmanValue",
    "evaluate_group_ap",
    "evaluate_group_coverage",
    "evaluate_group_spearman",
    "reduce_macro_average_precision_math",
    "reduce_macro_coverage_math",
    "rho_equal",
]

Metric = Literal["AP", "COVERAGE", "SPEARMAN"]
T = TypeVar("T")

GROUP_CANDIDATE_BALLOT_READY = False
GROUP_CANDIDATE_LIMITATIONS = (
    "GENERAL_VARIABLE_VALUE_MARGINAL_SPEARMAN_NOT_CERTIFIED",
    "G_ELIGIBLE_G_DEFINED_G_NA_AND_GATE_FLOORS_NOT_EVALUATED",
    "SPEARMAN_RHO64_PUBLICATION_NOT_MATERIALIZED",
)

AP_MAX_PARTITIONS = 32
AP_MAX_TOTAL_GT = 4_096
AP_MAX_TOTAL_PREDICTIONS = 1_024
AP_MAX_TOTAL_EDGES = 16_384
AP_MAX_DISTINCT_THRESHOLDS = 1_024
AP_MAX_LOCAL_THRESHOLDS = 1_024
AP_MAX_PREFIX_EDGE_INCIDENCE = 1_056_768

COVERAGE_MAX_UNITS = 32
COVERAGE_MAX_PARTITIONS_PER_UNIT = 8
COVERAGE_MAX_TOTAL_GT = 8_192
COVERAGE_MAX_TOTAL_PREDICTIONS = 8_192
COVERAGE_MAX_TOTAL_EDGES = 16_384

SPEARMAN_MAX_PARTITIONS = 16
SPEARMAN_MAX_TOTAL_GT = 128
SPEARMAN_MAX_TOTAL_PREDICTIONS = 128
SPEARMAN_MAX_TOTAL_EDGES = 16_384
SPEARMAN_MAX_SUPPORT = 128

# Every finite binary64 interpreted as an exact rational is safely below this
# reduced numerator-plus-denominator bit bound.  It covers RN64/mean64
# materialization even when a small exact input such as 1/3 expands to a
# binary64 rational with a long power-of-two denominator.
BINARY64_EXACT_RATIONAL_BIT_BOUND = 2_100


class GroupCandidateError(ValueError):
    """Malformed abstract group input."""


class GroupStatus(str, Enum):
    CERTIFIED = "CERTIFIED"
    NOT_APPLICABLE = "NOT_APPLICABLE"
    REJECTED = "REJECTED"


class GroupReason(str, Enum):
    OK = "OK"
    NO_GT = "NO_GT"
    NO_ACTIONABLE_GT = "NO_ACTIONABLE_GT"
    INSUFFICIENT_MATCHED_SUPPORT = "INSUFFICIENT_MATCHED_SUPPORT"
    SPEARMAN_UNDEFINED = "SPEARMAN_UNDEFINED"
    PAIRING_AMBIGUOUS = "PAIRING_AMBIGUOUS"
    PAIRING_ENVELOPE_UNAVAILABLE = "PAIRING_ENVELOPE_UNAVAILABLE"
    SPEARMAN_CERTIFICATE_UNAVAILABLE = "SPEARMAN_CERTIFICATE_UNAVAILABLE"
    SOLVER_STRUCTURAL_LIMIT_EXCEEDED = "SOLVER_STRUCTURAL_LIMIT_EXCEEDED"
    SOLVER_RUNTIME_FAILURE = "SOLVER_RUNTIME_FAILURE"
    SOLVER_CONSTRAINT_MODEL_INVALID = "SOLVER_CONSTRAINT_MODEL_INVALID"


@dataclass(frozen=True)
class GroupPreflightProbe:
    metric: Metric
    partition_count: int
    total_gt: int
    total_predictions: int
    total_eligible_edges: int
    distinct_thresholds: int
    local_thresholds: int
    prefix_edge_incidence: int
    unit_count: int
    maximum_partitions_per_unit: int
    maximum_matching_support: int
    maximum_cardinality_solve_bound: int
    profile_solve_bound: int
    spearman_marginal_value_classes: int
    exact_scalar_bit_bound: int
    subgraph_provisional_exceeded: tuple[
        tuple[tuple[str, ...], tuple[str, ...]], ...
    ]
    provisional_exceeded: tuple[str, ...]
    authority_status: str = "PROVISIONAL_GROUP_DIAGNOSTIC_ONLY"


@dataclass(frozen=True)
class GroupResult(Generic[T]):
    status: GroupStatus
    reason: GroupReason
    value: T | None
    preflight: GroupPreflightProbe
    witness: object | None = None


def _fraction_tuple(values: object, size: int, label: str) -> tuple[Fraction, ...]:
    if not isinstance(values, tuple) or len(values) != size:
        raise GroupCandidateError(f"{label} must be a tuple of length {size}")
    for value in values:
        if not isinstance(value, Fraction):
            raise GroupCandidateError(f"{label} values must be Fraction")
    return values


def _bool_tuple(values: object, size: int, label: str) -> tuple[bool, ...]:
    if not isinstance(values, tuple) or len(values) != size:
        raise GroupCandidateError(f"{label} must be a tuple of length {size}")
    if any(type(value) is not bool for value in values):
        raise GroupCandidateError(f"{label} values must be bool")
    return values


def _key_tuple(values: object, label: str) -> tuple[str, ...]:
    if (
        not isinstance(values, tuple)
        or not values
        or any(
            not isinstance(value, str)
            or not value
            or "\x00" in value
            for value in values
        )
    ):
        raise GroupCandidateError(
            f"{label} must be a non-empty tuple of non-empty NUL-free strings")
    return values


@dataclass(frozen=True)
class APPartition:
    partition_key: tuple[str, ...]
    graph: CandidateGraph
    prediction_confidence: tuple[Fraction, ...]

    def __post_init__(self) -> None:
        _key_tuple(self.partition_key, "APPartition.partition_key")
        if not isinstance(self.graph, CandidateGraph):
            raise GroupCandidateError("APPartition.graph must be CandidateGraph")
        values = _fraction_tuple(
            self.prediction_confidence,
            self.graph.prediction_count,
            "prediction_confidence",
        )
        if any(value < 0 or value > 1 for value in values):
            raise GroupCandidateError("prediction confidence must be in [0,1]")
        for value in values:
            try:
                normalized = exact_n64(rn64(value))
            except NumericAuthorityError as error:
                raise GroupCandidateError(
                    "prediction confidence must be canonical finite binary64"
                ) from error
            if normalized != value:
                raise GroupCandidateError(
                    "prediction confidence Fraction must already equal its "
                    "canonical binary64 value")


@dataclass(frozen=True)
class CoveragePartition:
    unit_key: tuple[str, ...]
    partition_key: tuple[str, ...]
    graph: CandidateGraph
    actionable_gt: tuple[bool, ...]
    actionable_prediction: tuple[bool, ...]

    def __post_init__(self) -> None:
        _key_tuple(self.unit_key, "CoveragePartition.unit_key")
        _key_tuple(self.partition_key, "CoveragePartition.partition_key")
        if not isinstance(self.graph, CandidateGraph):
            raise GroupCandidateError(
                "CoveragePartition.graph must be CandidateGraph")
        _bool_tuple(self.actionable_gt, self.graph.gt_count, "actionable_gt")
        _bool_tuple(
            self.actionable_prediction,
            self.graph.prediction_count,
            "actionable_prediction",
        )


@dataclass(frozen=True)
class SpearmanPartition:
    partition_key: tuple[str, ...]
    graph: CandidateGraph
    gt_values: tuple[Fraction, ...]
    prediction_values: tuple[Fraction, ...]

    def __post_init__(self) -> None:
        _key_tuple(self.partition_key, "SpearmanPartition.partition_key")
        if not isinstance(self.graph, CandidateGraph):
            raise GroupCandidateError(
                "SpearmanPartition.graph must be CandidateGraph")
        _fraction_tuple(self.gt_values, self.graph.gt_count, "gt_values")
        _fraction_tuple(
            self.prediction_values,
            self.graph.prediction_count,
            "prediction_values",
        )


@dataclass(frozen=True)
class APPrefix:
    threshold: Fraction
    true_positive: int
    false_positive: int
    precision: Fraction
    recall: Fraction


@dataclass(frozen=True)
class APValue:
    ap: Fraction
    ap_group64: str
    gt_count: int
    prefixes: tuple[APPrefix, ...]


@dataclass(frozen=True)
class CoverageUnitValue:
    unit_key: tuple[str, ...]
    reason: GroupReason
    coverage_minus: Fraction | None
    coverage_plus: Fraction | None
    coverage_minus64: str | None
    coverage_plus64: str | None
    actionable_gt_count: int
    partition_numerator_bounds: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class CoverageValue:
    coverage_minus_group64: str
    coverage_plus_group64: str
    defined_unit_count: int
    na_unit_count: int
    units: tuple[CoverageUnitValue, ...]


@dataclass(frozen=True)
class MacroAPReduction:
    """Math-only reduction; support/floor authority remains with the caller."""

    value64: str | None
    defined_group_count: int
    group_ids: tuple[str, ...]
    authority_status: str = "MATH_ONLY_SUPPORT_FLOOR_NOT_EVALUATED"


@dataclass(frozen=True)
class MacroCoverageReduction:
    """Math-only B-001 reductions; never a standalone gate decision."""

    coverage_minus64: str | None
    coverage_plus64: str | None
    defined_group_count: int
    group_ids: tuple[str, ...]
    groups_with_na_units: tuple[str, ...]
    authority_status: str = "MATH_ONLY_SUPPORT_FLOOR_NOT_EVALUATED"


@dataclass(frozen=True)
class RhoIdentity:
    """Exact identity for rho without requiring an irrational square root."""

    sign: int
    covariance_squared: Fraction
    variance_x: Fraction
    variance_y: Fraction

    def __post_init__(self) -> None:
        if self.sign not in (-1, 0, 1):
            raise GroupCandidateError("rho sign must be -1, 0, or 1")
        if self.covariance_squared < 0:
            raise GroupCandidateError("squared covariance must be non-negative")
        if self.variance_x <= 0 or self.variance_y <= 0:
            raise GroupCandidateError("rho variances must be positive")
        if (self.sign == 0) != (self.covariance_squared == 0):
            raise GroupCandidateError("zero rho sign/covariance mismatch")


@dataclass(frozen=True)
class SpearmanValue:
    rho: RhoIdentity
    support: int
    matching_by_partition: tuple[tuple[tuple[int, int], ...], ...]
    certificate: str


@dataclass(frozen=True)
class SpearmanAmbiguityWitness:
    lower: SpearmanValue
    upper: SpearmanValue


def rho_equal(left: RhoIdentity, right: RhoIdentity) -> bool:
    """R23 exact equality of two non-materialized Spearman coefficients."""
    if left.sign != right.sign:
        return False
    if left.sign == 0:
        return True
    return (
        left.covariance_squared * right.variance_x * right.variance_y
        == right.covariance_squared * left.variance_x * left.variance_y
    )


def _ceil_log2_positive(value: int) -> int:
    return 0 if value <= 1 else (value - 1).bit_length()


def _group_preflight(
    metric: Metric,
    graphs: tuple[CandidateGraph, ...],
    *,
    distinct_thresholds: int = 0,
    local_thresholds: int = 0,
    prefix_edge_incidence: int = 0,
    unit_count: int = 0,
    maximum_partitions_per_unit: int = 0,
    spearman_marginal_value_classes: int = 0,
    partition_keys: tuple[tuple[str, ...], ...] | None = None,
    external_exact_values: tuple[Fraction, ...] = (),
) -> GroupPreflightProbe:
    if partition_keys is None:
        partition_keys = tuple(
            (f"diagnostic-index-{index}",)
            for index in range(len(graphs))
        )
    if len(partition_keys) != len(graphs):
        raise GroupCandidateError(
            "partition_keys must align exactly with graphs")
    probes = tuple(provisional_preflight_probe(graph) for graph in graphs)
    total_gt = sum(graph.gt_count for graph in graphs)
    total_predictions = sum(graph.prediction_count for graph in graphs)
    total_edges = sum(len(graph.edges) for graph in graphs)
    support = sum(
        min(graph.gt_count, graph.prediction_count) for graph in graphs)
    subgraph_exceeded = tuple(
        (partition_keys[index], probe.provisional_exceeded)
        for index, probe in enumerate(probes)
        if probe.provisional_exceeded
    )
    maximum_subgraph_bound = max(
        (probe.exact_scalar_bit_bound for probe in probes), default=2)
    if metric == "AP":
        # For
        #   AP = sum_k (TP_k-TP_{k-1})*TP_k/(N_GT*P_active_k),
        # a common-denominator construction over T thresholds is bounded by
        # (2T-1)D + A + ceil(log2(T)), where
        # A=2*bitlen(N_GT) and D=bitlen(N_GT)+bitlen(P_total).
        thresholds = max(1, distinct_thresholds)
        numerator_bits = 2 * max(1, total_gt).bit_length()
        denominator_bits = (
            max(1, total_gt).bit_length()
            + max(1, total_predictions).bit_length()
        )
        group_bound = (
            (2 * thresholds - 1) * denominator_bits
            + numerator_bits
            + _ceil_log2_positive(thresholds)
        )
        # Each partition is solved only at its own distinct confidence
        # states.  Global thresholds reuse the most recent local state.
        cardinality_solve_bound = local_thresholds
        profile_solve_bound = 0
    elif metric == "COVERAGE":
        group_bound = 2 * max(1, total_gt).bit_length()
        cardinality_solve_bound = 2 * len(graphs)
        profile_solve_bound = 2 * len(graphs)
    else:
        # Midranks have denominator at most two.  This dominates covariance,
        # variance, squared covariance and the cross-products used by
        # ``rho_equal`` for a support of at most ``support``.
        group_bound = 16 * max(1, support).bit_length() + 32
        # Per partition: one objective solve; two extrema for the covariance.
        # Each distinct GT/prediction value class may require two additional
        # exact extrema to certify that its matched multiplicity is fixed.
        cardinality_solve_bound = (
            3 * len(graphs) + 2 * spearman_marginal_value_classes
        )
        profile_solve_bound = cardinality_solve_bound
    external_bound = max(
        (rational_bit_length(value) for value in external_exact_values),
        default=2,
    )
    exact_bound = max(
        maximum_subgraph_bound,
        group_bound,
        external_bound,
        BINARY64_EXACT_RATIONAL_BIT_BOUND,
    )
    exceeded = [
        (
            "SUBGRAPH_"
            + normalized_canonical_bytes(key).hex()
            + "_"
            + reason
        )
        for key, reasons in subgraph_exceeded
        for reason in reasons
    ]
    if metric == "AP":
        if len(graphs) > AP_MAX_PARTITIONS:
            exceeded.append("GROUP_AP_PARTITIONS")
        if total_gt > AP_MAX_TOTAL_GT:
            exceeded.append("GROUP_AP_GT")
        if total_predictions > AP_MAX_TOTAL_PREDICTIONS:
            exceeded.append("GROUP_AP_PREDICTIONS")
        if total_edges > AP_MAX_TOTAL_EDGES:
            exceeded.append("GROUP_AP_ELIGIBLE_EDGES")
        if distinct_thresholds > AP_MAX_DISTINCT_THRESHOLDS:
            exceeded.append("GROUP_AP_DISTINCT_THRESHOLDS")
        if local_thresholds > AP_MAX_LOCAL_THRESHOLDS:
            exceeded.append("GROUP_AP_LOCAL_THRESHOLDS")
        if prefix_edge_incidence > AP_MAX_PREFIX_EDGE_INCIDENCE:
            exceeded.append("GROUP_AP_PREFIX_EDGE_INCIDENCE")
    elif metric == "COVERAGE":
        if unit_count > COVERAGE_MAX_UNITS:
            exceeded.append("GROUP_COVERAGE_UNITS")
        if maximum_partitions_per_unit > COVERAGE_MAX_PARTITIONS_PER_UNIT:
            exceeded.append("GROUP_COVERAGE_PARTITIONS_PER_UNIT")
        if total_gt > COVERAGE_MAX_TOTAL_GT:
            exceeded.append("GROUP_COVERAGE_GT")
        if total_predictions > COVERAGE_MAX_TOTAL_PREDICTIONS:
            exceeded.append("GROUP_COVERAGE_PREDICTIONS")
        if total_edges > COVERAGE_MAX_TOTAL_EDGES:
            exceeded.append("GROUP_COVERAGE_ELIGIBLE_EDGES")
    else:
        if len(graphs) > SPEARMAN_MAX_PARTITIONS:
            exceeded.append("GROUP_SPEARMAN_PARTITIONS")
        if total_gt > SPEARMAN_MAX_TOTAL_GT:
            exceeded.append("GROUP_SPEARMAN_GT")
        if total_predictions > SPEARMAN_MAX_TOTAL_PREDICTIONS:
            exceeded.append("GROUP_SPEARMAN_PREDICTIONS")
        if total_edges > SPEARMAN_MAX_TOTAL_EDGES:
            exceeded.append("GROUP_SPEARMAN_ELIGIBLE_EDGES")
        if support > SPEARMAN_MAX_SUPPORT:
            exceeded.append("GROUP_SPEARMAN_SUPPORT")
    if exact_bound > PROVISIONAL_MAX_EXACT_SCALAR_BITS:
        exceeded.append("GROUP_EXACT_SCALAR_BIT_LENGTH")
    return GroupPreflightProbe(
        metric=metric,
        partition_count=len(graphs),
        total_gt=total_gt,
        total_predictions=total_predictions,
        total_eligible_edges=total_edges,
        distinct_thresholds=distinct_thresholds,
        local_thresholds=local_thresholds,
        prefix_edge_incidence=prefix_edge_incidence,
        unit_count=unit_count,
        maximum_partitions_per_unit=maximum_partitions_per_unit,
        maximum_matching_support=support,
        maximum_cardinality_solve_bound=cardinality_solve_bound,
        profile_solve_bound=profile_solve_bound,
        spearman_marginal_value_classes=spearman_marginal_value_classes,
        exact_scalar_bit_bound=exact_bound,
        subgraph_provisional_exceeded=subgraph_exceeded,
        provisional_exceeded=tuple(exceeded),
    )


def _preflight_rejection(probe: GroupPreflightProbe) -> GroupResult[object]:
    return GroupResult(
        GroupStatus.REJECTED,
        GroupReason.SOLVER_STRUCTURAL_LIMIT_EXCEEDED,
        None,
        probe,
        witness=probe.provisional_exceeded,
    )


def _ensure_unique_partition_keys(
    keys: tuple[tuple[str, ...], ...],
) -> None:
    if len(keys) != len(set(keys)):
        raise GroupCandidateError("partition_key values must be unique")


_RUNTIME_FAILURES = (MemoryError, RuntimeError, OverflowError)


def evaluate_group_ap(
    partitions: tuple[APPartition, ...],
) -> GroupResult[APValue]:
    """Compute exact group AP, inserting equal-confidence predictions at once."""
    if not isinstance(partitions, tuple) or any(
            not isinstance(partition, APPartition) for partition in partitions):
        raise GroupCandidateError("partitions must be a tuple of APPartition")
    _ensure_unique_partition_keys(
        tuple(partition.partition_key for partition in partitions))
    partitions = tuple(sorted(
        partitions,
        key=lambda partition: normalized_canonical_bytes(
            partition.partition_key),
    ))
    thresholds = tuple(sorted(
        {
            confidence
            for partition in partitions
            for confidence in partition.prediction_confidence
        },
        reverse=True,
    ))
    graphs = tuple(partition.graph for partition in partitions)
    local_thresholds = sum(
        len(set(partition.prediction_confidence))
        for partition in partitions
    )
    prefix_edge_incidence = 0
    for partition in partitions:
        for threshold in set(partition.prediction_confidence):
            prefix_edge_incidence += sum(
                partition.prediction_confidence[edge.prediction] >= threshold
                for edge in partition.graph.edges
            )
    probe = _group_preflight(
        "AP",
        graphs,
        distinct_thresholds=len(thresholds),
        local_thresholds=local_thresholds,
        prefix_edge_incidence=prefix_edge_incidence,
        partition_keys=tuple(
            partition.partition_key for partition in partitions),
        external_exact_values=tuple(
            confidence
            for partition in partitions
            for confidence in partition.prediction_confidence
        ),
    )
    if probe.provisional_exceeded:
        return _preflight_rejection(probe)  # type: ignore[return-value]
    gt_count = sum(graph.gt_count for graph in graphs)
    if gt_count == 0:
        return GroupResult(
            GroupStatus.NOT_APPLICABLE,
            GroupReason.NO_GT,
            None,
            probe,
        )
    previous_recall = Fraction(0)
    ap = Fraction(0)
    prefixes: list[APPrefix] = []
    local_prefixes: list[dict[Fraction, tuple[int, int]]] = []
    try:
        for partition in partitions:
            graph = partition.graph
            states: dict[Fraction, tuple[int, int]] = {}
            for threshold in sorted(
                    set(partition.prediction_confidence), reverse=True):
                active = {
                    index
                    for index, confidence in enumerate(
                        partition.prediction_confidence)
                    if confidence >= threshold
                }
                prefix = CandidateGraph(
                    graph.gt_count,
                    graph.prediction_count,
                    graph.k4_mode,
                    tuple(
                        edge for edge in graph.edges
                        if edge.prediction in active
                    ),
                )
                states[threshold] = (
                    len(active),
                    exact_maximum_cardinality(prefix),
                )
            local_prefixes.append(states)
    except CandidateGraphError as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            probe,
            witness=str(error),
        )
    except _RUNTIME_FAILURES as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            probe,
            witness=type(error).__name__,
        )
    current_states = [(0, 0)] * len(partitions)
    for threshold in thresholds:
        for index, states in enumerate(local_prefixes):
            if threshold in states:
                current_states[index] = states[threshold]
        active_predictions = sum(state[0] for state in current_states)
        tp = sum(state[1] for state in current_states)
        fp = active_predictions - tp
        precision = Fraction(tp, active_predictions)
        recall = Fraction(tp, gt_count)
        if recall < previous_recall:
            return GroupResult(
                GroupStatus.REJECTED,
                GroupReason.SOLVER_CONSTRAINT_MODEL_INVALID,
                None,
                probe,
                witness="AP K1 prefix is not monotone",
            )
        ap += (recall - previous_recall) * precision
        prefixes.append(APPrefix(
            threshold, tp, fp, precision, recall))
        previous_recall = recall
    return GroupResult(
        GroupStatus.CERTIFIED,
        GroupReason.OK,
        APValue(ap, rn64(ap), gt_count, tuple(prefixes)),
        probe,
    )


def reduce_macro_average_precision_math(
    groups: tuple[tuple[str, APValue], ...],
) -> MacroAPReduction:
    """Reduce defined AP groups only; does not evaluate support or gate floors."""
    if not isinstance(groups, tuple) or any(
        not isinstance(item, tuple) or len(item) != 2
        for item in groups
    ):
        raise GroupCandidateError(
            "groups must contain (group_id, APValue) tuples")
    if any(
        not isinstance(item[0], str)
        or not item[0]
        or "\x00" in item[0]
        or not isinstance(item[1], APValue)
        for item in groups
    ):
        raise GroupCandidateError(
            "groups must contain (group_id, APValue) tuples")
    group_ids = tuple(item[0] for item in groups)
    if len(group_ids) != len(set(group_ids)):
        raise GroupCandidateError("macro AP group_id values must be unique")
    ordered_ids = tuple(sorted(group_ids, key=lambda value: value.encode("utf-8")))
    return MacroAPReduction(
        mean64(
            (
                (group_id, value.ap_group64)
                for group_id, value in groups
            ),
            key_domain="utf8",
        ),
        len(group_ids),
        ordered_ids,
    )


def reduce_macro_coverage_math(
    groups: tuple[tuple[str, CoverageValue], ...],
) -> MacroCoverageReduction:
    """Reduce defined B-001 groups; does not evaluate support or gate floors."""
    if not isinstance(groups, tuple) or any(
        not isinstance(item, tuple) or len(item) != 2
        for item in groups
    ):
        raise GroupCandidateError(
            "groups must contain (group_id, CoverageValue) tuples")
    if any(
        not isinstance(item[0], str)
        or not item[0]
        or "\x00" in item[0]
        or not isinstance(item[1], CoverageValue)
        for item in groups
    ):
        raise GroupCandidateError(
            "groups must contain (group_id, CoverageValue) tuples")
    group_ids = tuple(item[0] for item in groups)
    if len(group_ids) != len(set(group_ids)):
        raise GroupCandidateError(
            "macro coverage group_id values must be unique")
    ordered_ids = tuple(sorted(group_ids, key=lambda value: value.encode("utf-8")))
    return MacroCoverageReduction(
        mean64(
            (
                (group_id, value.coverage_minus_group64)
                for group_id, value in groups
            ),
            key_domain="utf8",
        ),
        mean64(
            (
                (group_id, value.coverage_plus_group64)
                for group_id, value in groups
            ),
            key_domain="utf8",
        ),
        len(group_ids),
        ordered_ids,
        tuple(sorted(
            (
                group_id
                for group_id, value in groups
                if value.na_unit_count
            ),
            key=lambda value: value.encode("utf-8"),
        )),
    )


def _weighted_graph(
    graph: CandidateGraph,
    weights: dict[tuple[int, int], Fraction],
    *,
    complement: bool,
) -> CandidateGraph:
    ceiling = max(weights.values(), default=Fraction(0))
    return CandidateGraph(
        graph.gt_count,
        graph.prediction_count,
        graph.k4_mode,
        tuple(
            replace(
                edge,
                severity_error=(
                    ceiling - weights[(edge.gt, edge.prediction)]
                    if complement
                    else weights[(edge.gt, edge.prediction)]
                ),
            )
            for edge in graph.edges
        ),
    )


def _extreme_weight(
    graph: CandidateGraph,
    weights: dict[tuple[int, int], Fraction],
    *,
    maximize: bool,
) -> tuple[Fraction, tuple[tuple[int, int], ...]]:
    cardinality = exact_maximum_cardinality(graph)
    weighted = _weighted_graph(graph, weights, complement=not maximize)
    matching = _solve_profile(weighted, cardinality, "severity")
    return (
        sum((weights[pair] for pair in matching), Fraction(0)),
        matching,
    )


def evaluate_group_coverage(
    partitions: tuple[CoveragePartition, ...],
) -> GroupResult[CoverageValue]:
    """Compute B-001 per unit, then its pinned hierarchical group mean."""
    if not isinstance(partitions, tuple) or any(
            not isinstance(partition, CoveragePartition)
            for partition in partitions):
        raise GroupCandidateError(
            "partitions must be a tuple of CoveragePartition")
    _ensure_unique_partition_keys(
        tuple(partition.partition_key for partition in partitions))
    partitions = tuple(sorted(
        partitions,
        key=lambda partition: normalized_canonical_bytes(
            partition.partition_key),
    ))
    graphs = tuple(partition.graph for partition in partitions)
    by_unit: dict[tuple[str, ...], list[CoveragePartition]] = {}
    for partition in partitions:
        by_unit.setdefault(partition.unit_key, []).append(partition)
    probe = _group_preflight(
        "COVERAGE",
        graphs,
        unit_count=len(by_unit),
        maximum_partitions_per_unit=max(
            (len(rows) for rows in by_unit.values()),
            default=0,
        ),
        partition_keys=tuple(
            partition.partition_key for partition in partitions),
    )
    if probe.provisional_exceeded:
        return _preflight_rejection(probe)  # type: ignore[return-value]
    if not any(
        any(partition.actionable_gt) for partition in partitions
    ):
        return GroupResult(
            GroupStatus.NOT_APPLICABLE,
            GroupReason.NO_ACTIONABLE_GT,
            None,
            probe,
            witness={
                "unit_keys": tuple(sorted(
                    by_unit,
                    key=normalized_canonical_bytes,
                )),
                "defined_unit_count": 0,
                "na_unit_count": len(by_unit),
            },
        )
    units: list[CoverageUnitValue] = []
    try:
        for unit_key in sorted(by_unit, key=normalized_canonical_bytes):
            rows = sorted(
                by_unit[unit_key],
                key=lambda row: normalized_canonical_bytes(
                    row.partition_key),
            )
            denominator = sum(
                sum(partition.actionable_gt) for partition in rows)
            if denominator == 0:
                units.append(CoverageUnitValue(
                    unit_key,
                    GroupReason.NO_ACTIONABLE_GT,
                    None,
                    None,
                    None,
                    None,
                    0,
                    (),
                ))
                continue
            lower_total = 0
            upper_total = 0
            bounds: list[tuple[int, int]] = []
            for partition in rows:
                weights = {
                    (edge.gt, edge.prediction): Fraction(int(
                        partition.actionable_gt[edge.gt]
                        and partition.actionable_prediction[edge.prediction]
                    ))
                    for edge in partition.graph.edges
                }
                lower, _lower_matching = _extreme_weight(
                    partition.graph, weights, maximize=False)
                upper, _upper_matching = _extreme_weight(
                    partition.graph, weights, maximize=True)
                if lower.denominator != 1 or upper.denominator != 1:
                    raise CandidateGraphError(
                        "coverage envelope did not remain integral")
                lower_int = lower.numerator
                upper_int = upper.numerator
                if not 0 <= lower_int <= upper_int:
                    raise CandidateGraphError(
                        "invalid coverage envelope order")
                lower_total += lower_int
                upper_total += upper_int
                bounds.append((lower_int, upper_int))
            coverage_minus = Fraction(lower_total, denominator)
            coverage_plus = Fraction(upper_total, denominator)
            units.append(CoverageUnitValue(
                unit_key,
                GroupReason.OK,
                coverage_minus,
                coverage_plus,
                rn64(coverage_minus),
                rn64(coverage_plus),
                denominator,
                tuple(bounds),
            ))
    except CandidateGraphError as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            probe,
            witness=str(error),
        )
    except _RUNTIME_FAILURES as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            probe,
            witness=type(error).__name__,
        )
    minus_group64 = mean64(
        (
            (unit.unit_key, unit.coverage_minus64)
            for unit in units
            if unit.coverage_minus64 is not None
        ),
        key_domain="canonical",
    )
    plus_group64 = mean64(
        (
            (unit.unit_key, unit.coverage_plus64)
            for unit in units
            if unit.coverage_plus64 is not None
        ),
        key_domain="canonical",
    )
    if minus_group64 is None or plus_group64 is None:
        raise CandidateGraphError(
            "actionable coverage unexpectedly produced no unit value")
    return GroupResult(
        GroupStatus.CERTIFIED,
        GroupReason.OK,
        CoverageValue(
            minus_group64,
            plus_group64,
            sum(unit.reason == GroupReason.OK for unit in units),
            sum(
                unit.reason == GroupReason.NO_ACTIONABLE_GT
                for unit in units
            ),
            tuple(units),
        ),
        probe,
    )


def _midranks(values: tuple[Fraction, ...]) -> tuple[Fraction, ...]:
    ordered = sorted(range(len(values)), key=lambda index: (values[index], index))
    ranks = [Fraction(0)] * len(values)
    start = 0
    while start < len(ordered):
        end = start + 1
        while end < len(ordered) and values[ordered[end]] == values[ordered[start]]:
            end += 1
        rank = Fraction((start + 1) + end, 2)
        for position in range(start, end):
            ranks[ordered[position]] = rank
        start = end
    return tuple(ranks)


def _rho_identity(
    pairs: tuple[tuple[Fraction, Fraction], ...],
) -> RhoIdentity | None:
    x = tuple(pair[0] for pair in pairs)
    y = tuple(pair[1] for pair in pairs)
    rank_x = _midranks(x)
    rank_y = _midranks(y)
    n = len(pairs)
    sum_x = sum(rank_x, Fraction(0))
    sum_y = sum(rank_y, Fraction(0))
    covariance = (
        n * sum((a * b for a, b in zip(rank_x, rank_y)), Fraction(0))
        - sum_x * sum_y
    )
    variance_x = (
        n * sum((value * value for value in rank_x), Fraction(0))
        - sum_x * sum_x
    )
    variance_y = (
        n * sum((value * value for value in rank_y), Fraction(0))
        - sum_y * sum_y
    )
    if variance_x == 0 or variance_y == 0:
        return None
    sign = (covariance > 0) - (covariance < 0)
    return RhoIdentity(
        sign,
        covariance * covariance,
        variance_x,
        variance_y,
    )


def _pairs_for_matchings(
    partitions: tuple[SpearmanPartition, ...],
    matchings: tuple[tuple[tuple[int, int], ...], ...],
) -> tuple[tuple[Fraction, Fraction], ...]:
    return tuple(
        (
            partition.gt_values[gt],
            partition.prediction_values[prediction],
        )
        for partition, matching in zip(partitions, matchings)
        for gt, prediction in matching
    )


def _value_marginal_is_fixed(
    partition: SpearmanPartition,
    *,
    axis: Literal["gt", "prediction"],
) -> bool:
    """Prove that every V-optimum selects the same value multiplicities."""
    values = (
        partition.gt_values
        if axis == "gt"
        else partition.prediction_values
    )
    for value in set(values):
        weights = {
            (edge.gt, edge.prediction): Fraction(int(
                (
                    partition.gt_values[edge.gt]
                    if axis == "gt"
                    else partition.prediction_values[edge.prediction]
                ) == value
            ))
            for edge in partition.graph.edges
        }
        lower, _ = _extreme_weight(
            partition.graph, weights, maximize=False)
        upper, _ = _extreme_weight(
            partition.graph, weights, maximize=True)
        if lower != upper:
            return False
    return True


def _ranks_by_value(
    values: tuple[Fraction, ...],
) -> dict[Fraction, Fraction]:
    ranks = _midranks(values)
    result: dict[Fraction, Fraction] = {}
    for value, rank in zip(values, ranks):
        previous = result.setdefault(value, rank)
        if previous != rank:
            raise CandidateGraphError("equal values received unequal midranks")
    return result


def evaluate_group_spearman(
    partitions: tuple[SpearmanPartition, ...],
) -> GroupResult[SpearmanValue]:
    """Certify one exact rho across all composed per-partition optima."""
    if not isinstance(partitions, tuple) or any(
            not isinstance(partition, SpearmanPartition)
            for partition in partitions):
        raise GroupCandidateError(
            "partitions must be a tuple of SpearmanPartition")
    _ensure_unique_partition_keys(
        tuple(partition.partition_key for partition in partitions))
    partitions = tuple(sorted(
        partitions,
        key=lambda partition: normalized_canonical_bytes(
            partition.partition_key),
    ))
    graphs = tuple(partition.graph for partition in partitions)
    marginal_value_classes = sum(
        len(set(partition.gt_values))
        + len(set(partition.prediction_values))
        for partition in partitions
    )
    probe = _group_preflight(
        "SPEARMAN",
        graphs,
        spearman_marginal_value_classes=marginal_value_classes,
        partition_keys=tuple(
            partition.partition_key for partition in partitions),
        external_exact_values=tuple(
            value
            for partition in partitions
            for value in (*partition.gt_values, *partition.prediction_values)
        ),
    )
    if probe.provisional_exceeded:
        return _preflight_rejection(probe)  # type: ignore[return-value]
    canonical: list[tuple[tuple[int, int], ...]] = []
    ambiguous: list[bool] = []
    try:
        for graph in graphs:
            cardinality = exact_maximum_cardinality(graph)
            matching, has_alternative = _solve_profile_details(
                graph, cardinality, "objective")
            canonical.append(matching)
            ambiguous.append(has_alternative)
    except CandidateGraphError as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            probe,
            witness=str(error),
        )
    except _RUNTIME_FAILURES as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            probe,
            witness=type(error).__name__,
        )
    support = sum(len(matching) for matching in canonical)
    if support < 10:
        return GroupResult(
            GroupStatus.NOT_APPLICABLE,
            GroupReason.INSUFFICIENT_MATCHED_SUPPORT,
            None,
            probe,
            witness={"support": support, "minimum_support": 10},
        )
    canonical_tuple = tuple(canonical)
    if not any(ambiguous):
        rho = _rho_identity(_pairs_for_matchings(partitions, canonical_tuple))
        if rho is None:
            return GroupResult(
                GroupStatus.NOT_APPLICABLE,
                GroupReason.SPEARMAN_UNDEFINED,
                None,
                probe,
                witness={"support": support},
            )
        return GroupResult(
            GroupStatus.CERTIFIED,
            GroupReason.OK,
            SpearmanValue(
                rho, support, canonical_tuple, "UNIQUE_OPTIMUM"),
            probe,
        )
    # A unique optimum fixes membership.  An ambiguous perfect matching fixes
    # it cheaply.  Rectangular/partial optima are still certifiable when exact
    # envelope probes prove that every distinct GT and prediction value has a
    # fixed matched multiplicity across M*.
    try:
        fixed = all(
            not is_ambiguous
            or len(matching) == graph.gt_count == graph.prediction_count
            or (
                _value_marginal_is_fixed(partition, axis="gt")
                and _value_marginal_is_fixed(
                    partition, axis="prediction")
            )
            for partition, graph, matching, is_ambiguous
            in zip(partitions, graphs, canonical, ambiguous)
        )
    except CandidateGraphError as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            probe,
            witness=str(error),
        )
    except _RUNTIME_FAILURES as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            probe,
            witness=type(error).__name__,
        )
    if not fixed:
        return GroupResult(
            GroupStatus.NOT_APPLICABLE,
            GroupReason.SPEARMAN_CERTIFICATE_UNAVAILABLE,
            None,
            probe,
            witness={
                "diagnostic": "VARIABLE_MARGINAL_CERTIFICATE_UNAVAILABLE",
                "ballot_ready": False,
            },
        )

    # Fixed matched marginals make all rank vectors and both variances
    # invariant.  The product sum, and therefore covariance, is separable over
    # partitions and can be minimized/maximized with exact envelope solves.
    selected_gt = tuple(
        tuple(sorted(gt for gt, _prediction in matching))
        for matching in canonical
    )
    selected_prediction = tuple(
        tuple(sorted(prediction for _gt, prediction in matching))
        for matching in canonical
    )
    all_gt_values = tuple(
        partitions[index].gt_values[gt]
        for index in range(len(partitions))
        for gt in selected_gt[index]
    )
    all_prediction_values = tuple(
        partitions[index].prediction_values[prediction]
        for index in range(len(partitions))
        for prediction in selected_prediction[index]
    )
    gt_ranks = _ranks_by_value(all_gt_values)
    prediction_ranks = _ranks_by_value(all_prediction_values)

    lower_matchings: list[tuple[tuple[int, int], ...]] = []
    upper_matchings: list[tuple[tuple[int, int], ...]] = []
    try:
        for partition in partitions:
            weights = {
                (edge.gt, edge.prediction): (
                    gt_ranks.get(
                        partition.gt_values[edge.gt], Fraction(0))
                    * prediction_ranks.get(
                        partition.prediction_values[edge.prediction],
                        Fraction(0),
                    )
                )
                for edge in partition.graph.edges
            }
            _lower, lower_matching = _extreme_weight(
                partition.graph, weights, maximize=False)
            _upper, upper_matching = _extreme_weight(
                partition.graph, weights, maximize=True)
            lower_matchings.append(lower_matching)
            upper_matchings.append(upper_matching)
    except (CandidateGraphError, KeyError) as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            probe,
            witness=str(error),
        )
    except _RUNTIME_FAILURES as error:
        return GroupResult(
            GroupStatus.REJECTED,
            GroupReason.SOLVER_RUNTIME_FAILURE,
            None,
            probe,
            witness=type(error).__name__,
        )
    lower_tuple = tuple(lower_matchings)
    upper_tuple = tuple(upper_matchings)
    lower_rho = _rho_identity(_pairs_for_matchings(partitions, lower_tuple))
    upper_rho = _rho_identity(_pairs_for_matchings(partitions, upper_tuple))
    if lower_rho is None or upper_rho is None:
        return GroupResult(
            GroupStatus.NOT_APPLICABLE,
            GroupReason.SPEARMAN_UNDEFINED,
            None,
            probe,
            witness={"support": support},
        )
    lower_value = SpearmanValue(
        lower_rho, support, lower_tuple, "FIXED_MARGINAL_EXTREME")
    upper_value = SpearmanValue(
        upper_rho, support, upper_tuple, "FIXED_MARGINAL_EXTREME")
    if not rho_equal(lower_rho, upper_rho):
        return GroupResult(
            GroupStatus.NOT_APPLICABLE,
            GroupReason.PAIRING_AMBIGUOUS,
            None,
            probe,
            witness=SpearmanAmbiguityWitness(lower_value, upper_value),
        )
    return GroupResult(
        GroupStatus.CERTIFIED,
        GroupReason.OK,
        SpearmanValue(
            lower_rho, support, lower_tuple,
            "FIXED_MARGINAL_RHO_SINGLETON",
        ),
        probe,
    )
