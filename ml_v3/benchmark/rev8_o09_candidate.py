"""REV8 O-09 candidate-only exact matching benchmark kernel.

This module is deliberately *not* exported by :mod:`ml_v3.benchmark` and is
not reachable from the live REV7 evaluator.  Its only purpose before
``REV8 SPEC GO`` is to provide the A1/A2 subject needed by the signed O-09
benchmark and ballot sequence.

The input is an already-normalized abstract bipartite graph.  Raw schemas,
identity, tick conversion, eligibility, class semantics, and dispatcher
activation are outside this module's authority.

A1 exhaustively enumerates all matchings for the frozen small oracle domain.
A2 computes the same exact objective without enumerating ``M*``:

* maximum cardinality is found by Hopcroft--Karp;
* a fixed-cardinality exact min-cost flow operates in a lexicographically
  ordered abelian group;
* additive K4 uses rational addition, Resonance K4 uses rational
  multiplication/division;
* K6's complete sorted S then D sequences are encoded by exact base-(K+1)
  count vectors.  The integer encoding is an implementation temporary; the
  observable authority remains the materialized S/D byte sequences.
* severity, onset, and offset upper envelopes are independent exact solves
  whose prefix remains the complete V objective.

No float, greedy, first-fit, approximation, or fallback path exists here.

In the separate post-A1 enforcement tranche,
``evaluate_a2_fail_closed`` and
``evaluate_a2_batch_fail_closed`` are the isolated enforcement entry points
for the four active per-subgraph ceilings.  They remain candidate-only: this
module is still not exported to, or reachable from, the live REV7 evaluator.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, replace
from enum import Enum
from fractions import Fraction
import heapq
from typing import Iterable, Literal, Sequence

__all__ = [
    "A1_ORACLE_MAX_SIDE",
    "PROVISIONAL_MAX_EDGES",
    "PROVISIONAL_MAX_EXACT_SCALAR_BITS",
    "PROVISIONAL_MAX_GT",
    "PROVISIONAL_MAX_PREDICTIONS",
    "CandidateGraph",
    "CandidateGraphError",
    "CandidateBatchEvaluation",
    "CandidateEvaluation",
    "CandidateEvaluationReason",
    "CandidateEvaluationStatus",
    "CandidateResult",
    "ExactEdge",
    "Objective",
    "PreflightProbe",
    "a1_exhaustive",
    "evaluate_a2_batch_fail_closed",
    "evaluate_a2_fail_closed",
    "exact_maximum_cardinality",
    "provisional_preflight_probe",
    "rational_bit_length",
]

A1_ORACLE_MAX_SIDE = 3
PROVISIONAL_MAX_GT = 128
PROVISIONAL_MAX_PREDICTIONS = 128
PROVISIONAL_MAX_EDGES = 16_384
PROVISIONAL_MAX_EXACT_SCALAR_BITS = 65_536

K4Mode = Literal["additive", "product"]
Profile = Literal["objective", "canonical", "severity", "onset", "offset"]


class CandidateGraphError(ValueError):
    """Malformed abstract graph or an internally inconsistent exact solve."""


def _is_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _fraction(value: object, label: str) -> Fraction:
    if not isinstance(value, Fraction):
        raise CandidateGraphError(f"{label} must be fractions.Fraction")
    return value


@dataclass(frozen=True)
class ExactEdge:
    """One eligible, already-normalized GT/prediction edge."""

    gt: int
    prediction: int
    k2_iou: Fraction
    k3_tick_error: int
    k4_cost: Fraction
    scientific_key: bytes
    diagnostic_key: bytes
    severity_error: Fraction = Fraction(0)
    onset_error_ticks: int = 0
    offset_error_ticks: int = 0

    def validate(self, gt_count: int, prediction_count: int,
                 k4_mode: K4Mode) -> None:
        if not _is_int(self.gt) or not 0 <= self.gt < gt_count:
            raise CandidateGraphError(f"edge.gt outside [0,{gt_count}): {self.gt!r}")
        if not _is_int(self.prediction) or not 0 <= self.prediction < prediction_count:
            raise CandidateGraphError(
                f"edge.prediction outside [0,{prediction_count}): "
                f"{self.prediction!r}")
        k2 = _fraction(self.k2_iou, "edge.k2_iou")
        if not Fraction(0) <= k2 <= Fraction(1):
            raise CandidateGraphError("edge.k2_iou must be in [0,1]")
        if not _is_int(self.k3_tick_error) or self.k3_tick_error < 0:
            raise CandidateGraphError("edge.k3_tick_error must be a non-negative int")
        k4 = _fraction(self.k4_cost, "edge.k4_cost")
        if k4_mode == "additive" and k4 < 0:
            raise CandidateGraphError("additive edge.k4_cost must be non-negative")
        if k4_mode == "product" and k4 < 1:
            raise CandidateGraphError("product edge.k4_cost must be >= 1")
        if not isinstance(self.scientific_key, bytes):
            raise CandidateGraphError("edge.scientific_key must be bytes")
        if not isinstance(self.diagnostic_key, bytes):
            raise CandidateGraphError("edge.diagnostic_key must be bytes")
        if not self.scientific_key:
            raise CandidateGraphError("edge.scientific_key must be non-empty")
        if not self.diagnostic_key:
            raise CandidateGraphError("edge.diagnostic_key must be non-empty")
        severity = _fraction(self.severity_error, "edge.severity_error")
        if severity < 0:
            raise CandidateGraphError("edge.severity_error must be non-negative")
        for value, label in (
            (self.onset_error_ticks, "edge.onset_error_ticks"),
            (self.offset_error_ticks, "edge.offset_error_ticks"),
        ):
            if not _is_int(value) or value < 0:
                raise CandidateGraphError(f"{label} must be a non-negative int")


@dataclass(frozen=True)
class CandidateGraph:
    """Abstract per-subgraph input for the isolated O-09 benchmark."""

    gt_count: int
    prediction_count: int
    k4_mode: K4Mode
    edges: tuple[ExactEdge, ...]

    def __post_init__(self) -> None:
        if not _is_int(self.gt_count) or self.gt_count < 0:
            raise CandidateGraphError("gt_count must be a non-negative int")
        if not _is_int(self.prediction_count) or self.prediction_count < 0:
            raise CandidateGraphError(
                "prediction_count must be a non-negative int")
        if self.k4_mode not in ("additive", "product"):
            raise CandidateGraphError("k4_mode must be 'additive' or 'product'")
        if not isinstance(self.edges, tuple):
            raise CandidateGraphError("edges must be a tuple")
        seen_pairs: set[tuple[int, int]] = set()
        seen_diagnostic_keys: set[bytes] = set()
        for edge in self.edges:
            if not isinstance(edge, ExactEdge):
                raise CandidateGraphError("edges must contain ExactEdge values")
            edge.validate(self.gt_count, self.prediction_count, self.k4_mode)
            pair = (edge.gt, edge.prediction)
            if pair in seen_pairs:
                raise CandidateGraphError(f"duplicate eligible edge {pair}")
            seen_pairs.add(pair)
            # R23C diagnostic_edge_key contains the complete endpoint payload
            # plus the validated occurrence ordinal.  It is therefore unique
            # per eligible edge.  Without this invariant two different
            # matchings can have identical S and D, contradicting the signed
            # "unique argmin" authority for M_replay.
            if edge.diagnostic_key in seen_diagnostic_keys:
                raise CandidateGraphError(
                    "duplicate diagnostic edge key makes M_replay non-unique")
            seen_diagnostic_keys.add(edge.diagnostic_key)


@dataclass(frozen=True)
class Objective:
    """Exact V=(K1,K2,K3,K4), with K5_RESERVED absent."""

    k1: int
    k2: Fraction
    k3: int
    k4: Fraction
    k4_mode: K4Mode

    def rank_key(self) -> tuple[object, ...]:
        """Ascending Python tuple whose minimum is the normative optimum."""
        return (-self.k1, -self.k2, self.k3, self.k4)


@dataclass(frozen=True)
class CandidateResult:
    objective: Objective
    matching: tuple[tuple[int, int], ...]
    scientific_sequence: tuple[bytes, ...]
    diagnostic_sequence: tuple[bytes, ...]
    severity_upper: Fraction | None
    onset_upper_ticks: Fraction | None
    offset_upper_ticks: Fraction | None
    m_star: tuple[tuple[tuple[int, int], ...], ...] | None


class CandidateEvaluationStatus(str, Enum):
    """Fail-closed status for the isolated A1 enforcement surface."""

    EVALUATED = "EVALUATED"
    REJECTED = "REJECTED"


class CandidateEvaluationReason(str, Enum):
    """Normative A1 outcome reason for a per-subgraph or batch evaluation."""

    OK = "OK"
    SOLVER_STRUCTURAL_LIMIT_EXCEEDED = "SOLVER_STRUCTURAL_LIMIT_EXCEEDED"
    SOLVER_RUNTIME_FAILURE = "SOLVER_RUNTIME_FAILURE"
    SOLVER_CONSTRAINT_MODEL_INVALID = "SOLVER_CONSTRAINT_MODEL_INVALID"


@dataclass(frozen=True)
class CandidateEvaluation:
    """One fail-closed per-subgraph result.

    ``value`` is absent for every rejection.  This prevents callers from
    accidentally publishing a partial solve alongside a fatal status.
    """

    status: CandidateEvaluationStatus
    reason: CandidateEvaluationReason
    value: CandidateResult | None
    preflight: "PreflightProbe | None"
    witness: object | None = None


@dataclass(frozen=True)
class CandidateBatchEvaluation:
    """Fail-closed aggregate over gate-contributing subgraphs."""

    status: CandidateEvaluationStatus
    reason: CandidateEvaluationReason
    values: tuple[CandidateResult, ...] | None
    preflights: tuple["PreflightProbe", ...] | None
    witness: object | None = None


def _edge_map(graph: CandidateGraph) -> dict[tuple[int, int], ExactEdge]:
    return {(edge.gt, edge.prediction): edge for edge in graph.edges}


def _objective(graph: CandidateGraph,
               matching: Sequence[tuple[int, int]]) -> Objective:
    edges = _edge_map(graph)
    selected = [edges[pair] for pair in matching]
    k4 = (Fraction(1) if graph.k4_mode == "product" else Fraction(0))
    for edge in selected:
        if graph.k4_mode == "product":
            k4 *= edge.k4_cost
        else:
            k4 += edge.k4_cost
    return Objective(
        k1=len(selected),
        k2=sum((edge.k2_iou for edge in selected), Fraction(0)),
        k3=sum(edge.k3_tick_error for edge in selected),
        k4=k4,
        k4_mode=graph.k4_mode,
    )


def _sequences(graph: CandidateGraph,
               matching: Sequence[tuple[int, int]]
               ) -> tuple[tuple[bytes, ...], tuple[bytes, ...]]:
    edges = _edge_map(graph)
    selected = [edges[pair] for pair in matching]
    return (
        tuple(sorted(edge.scientific_key for edge in selected)),
        tuple(sorted(edge.diagnostic_key for edge in selected)),
    )


def _metric_sums(graph: CandidateGraph,
                 matching: Sequence[tuple[int, int]]
                 ) -> tuple[Fraction, int, int]:
    edges = _edge_map(graph)
    selected = [edges[pair] for pair in matching]
    return (
        sum((edge.severity_error for edge in selected), Fraction(0)),
        sum(edge.onset_error_ticks for edge in selected),
        sum(edge.offset_error_ticks for edge in selected),
    )


def _all_matchings(graph: CandidateGraph
                   ) -> Iterable[tuple[tuple[int, int], ...]]:
    by_gt: list[list[int]] = [[] for _ in range(graph.gt_count)]
    for edge in graph.edges:
        by_gt[edge.gt].append(edge.prediction)
    for row in by_gt:
        row.sort()

    pairs: list[tuple[int, int]] = []
    used: set[int] = set()

    def visit(gt: int) -> Iterable[tuple[tuple[int, int], ...]]:
        if gt == graph.gt_count:
            yield tuple(pairs)
            return
        for prediction in by_gt[gt]:
            if prediction in used:
                continue
            used.add(prediction)
            pairs.append((gt, prediction))
            yield from visit(gt + 1)
            pairs.pop()
            used.remove(prediction)
        yield from visit(gt + 1)

    yield from visit(0)


def a1_exhaustive(graph: CandidateGraph) -> CandidateResult:
    """Normative exhaustive oracle on the frozen |GT|,|P|<=3 domain."""
    if (graph.gt_count > A1_ORACLE_MAX_SIDE
            or graph.prediction_count > A1_ORACLE_MAX_SIDE):
        raise CandidateGraphError(
            f"A1 oracle domain is <= {A1_ORACLE_MAX_SIDE} per side")
    candidates = list(_all_matchings(graph))
    objectives = [_objective(graph, matching) for matching in candidates]
    best_rank = min(objective.rank_key() for objective in objectives)
    optimum = [
        matching for matching, objective in zip(candidates, objectives)
        if objective.rank_key() == best_rank
    ]
    optimum.sort()
    canonical = min(
        optimum,
        key=lambda matching: _sequences(graph, matching),
    )
    objective = _objective(graph, canonical)
    scientific, diagnostic = _sequences(graph, canonical)
    if objective.k1 == 0:
        severity_upper = onset_upper = offset_upper = None
    else:
        sums = [_metric_sums(graph, matching) for matching in optimum]
        severity_upper = max(value[0] for value in sums) / objective.k1
        onset_upper = Fraction(max(value[1] for value in sums), objective.k1)
        offset_upper = Fraction(max(value[2] for value in sums), objective.k1)
    return CandidateResult(
        objective=objective,
        matching=canonical,
        scientific_sequence=scientific,
        diagnostic_sequence=diagnostic,
        severity_upper=severity_upper,
        onset_upper_ticks=onset_upper,
        offset_upper_ticks=offset_upper,
        m_star=tuple(optimum),
    )


@dataclass(frozen=True)
class _Cost:
    """Element of the ordered abelian group used by exact min-cost flow."""

    neg_k2: Fraction
    k3: int
    k4: Fraction
    extras: tuple[Fraction | int, ...]
    k4_mode: K4Mode

    @classmethod
    def zero(cls, k4_mode: K4Mode, extra_count: int) -> "_Cost":
        return cls(
            Fraction(0),
            0,
            Fraction(1) if k4_mode == "product" else Fraction(0),
            (0,) * extra_count,
            k4_mode,
        )

    def __add__(self, other: "_Cost") -> "_Cost":
        self._compatible(other)
        k4 = (self.k4 * other.k4 if self.k4_mode == "product"
              else self.k4 + other.k4)
        return _Cost(
            self.neg_k2 + other.neg_k2,
            self.k3 + other.k3,
            k4,
            tuple(a + b for a, b in zip(self.extras, other.extras)),
            self.k4_mode,
        )

    def __neg__(self) -> "_Cost":
        k4 = (Fraction(1, 1) / self.k4 if self.k4_mode == "product"
              else -self.k4)
        return _Cost(
            -self.neg_k2,
            -self.k3,
            k4,
            tuple(-value for value in self.extras),
            self.k4_mode,
        )

    def __sub__(self, other: "_Cost") -> "_Cost":
        return self + (-other)

    def _key(self) -> tuple[object, ...]:
        return (self.neg_k2, self.k3, self.k4, *self.extras)

    def __lt__(self, other: "_Cost") -> bool:
        self._compatible(other)
        return self._key() < other._key()

    def _compatible(self, other: "_Cost") -> None:
        if (self.k4_mode != other.k4_mode
                or len(self.extras) != len(other.extras)):
            raise CandidateGraphError("incompatible exact cost domains")


@dataclass
class _Arc:
    to: int
    reverse: int
    capacity: int
    cost: _Cost
    pair: tuple[int, int] | None = None


def _hopcroft_karp_size(graph: CandidateGraph) -> int:
    adjacency: list[list[int]] = [[] for _ in range(graph.gt_count)]
    for edge in graph.edges:
        adjacency[edge.gt].append(edge.prediction)
    for row in adjacency:
        row.sort()
    pair_gt = [-1] * graph.gt_count
    pair_pred = [-1] * graph.prediction_count
    distance = [0] * graph.gt_count
    infinity = graph.gt_count + graph.prediction_count + 1

    def bfs() -> bool:
        queue: deque[int] = deque()
        shortest = infinity
        for gt in range(graph.gt_count):
            if pair_gt[gt] == -1:
                distance[gt] = 0
                queue.append(gt)
            else:
                distance[gt] = infinity
        while queue:
            gt = queue.popleft()
            if distance[gt] >= shortest:
                continue
            for prediction in adjacency[gt]:
                mate = pair_pred[prediction]
                if mate == -1:
                    shortest = distance[gt] + 1
                elif distance[mate] == infinity:
                    distance[mate] = distance[gt] + 1
                    queue.append(mate)
        return shortest != infinity

    def dfs(gt: int) -> bool:
        for prediction in adjacency[gt]:
            mate = pair_pred[prediction]
            if mate == -1 or (
                    distance[mate] == distance[gt] + 1 and dfs(mate)):
                pair_gt[gt] = prediction
                pair_pred[prediction] = gt
                return True
        distance[gt] = infinity
        return False

    matching = 0
    while bfs():
        for gt in range(graph.gt_count):
            if pair_gt[gt] == -1 and dfs(gt):
                matching += 1
    return matching


def exact_maximum_cardinality(graph: CandidateGraph) -> int:
    """Exact K1 for AP prefix probes; no secondary objective is consulted."""
    return _hopcroft_karp_size(graph)


def _canonical_weights(keys: Iterable[bytes], cardinality: int
                       ) -> dict[bytes, int]:
    unique = sorted(set(keys))
    if not unique:
        return {}
    base = cardinality + 1
    return {
        key: -(base ** (len(unique) - rank - 1))
        for rank, key in enumerate(unique)
    }


def _edge_cost(edge: ExactEdge, graph: CandidateGraph, profile: Profile,
               scientific_weights: dict[bytes, int],
               diagnostic_weights: dict[bytes, int]) -> _Cost:
    if profile == "objective":
        extras = ()
    elif profile == "canonical":
        extras: tuple[Fraction | int, ...] = (
            scientific_weights[edge.scientific_key],
            diagnostic_weights[edge.diagnostic_key],
        )
    elif profile == "severity":
        extras = (-edge.severity_error,)
    elif profile == "onset":
        extras = (-edge.onset_error_ticks,)
    elif profile == "offset":
        extras = (-edge.offset_error_ticks,)
    else:  # pragma: no cover - Literal plus internal caller
        raise CandidateGraphError(f"unknown solve profile {profile!r}")
    return _Cost(
        -edge.k2_iou,
        edge.k3_tick_error,
        edge.k4_cost,
        extras,
        graph.k4_mode,
    )


def _add_arc(network: list[list[_Arc]], source: int, target: int,
             cost: _Cost, pair: tuple[int, int] | None = None) -> None:
    forward = _Arc(target, len(network[target]), 1, cost, pair)
    reverse = _Arc(source, len(network[source]), 0, -cost, None)
    network[source].append(forward)
    network[target].append(reverse)


def _initial_potentials(network: list[list[_Arc]], source: int,
                        zero: _Cost) -> list[_Cost]:
    """Bellman--Ford once; subsequent augmentations use exact Dijkstra."""
    count = len(network)
    distance: list[_Cost | None] = [None] * count
    distance[source] = zero
    for _ in range(count - 1):
        changed = False
        for node, arcs in enumerate(network):
            current = distance[node]
            if current is None:
                continue
            for arc in arcs:
                if arc.capacity == 0:
                    continue
                candidate = current + arc.cost
                if distance[arc.to] is None or candidate < distance[arc.to]:
                    distance[arc.to] = candidate
                    changed = True
        if not changed:
            break
    # A reachable negative cycle at the zero-flow state would mean that the
    # abstract cost model is not an ordered assignment model.
    for node, arcs in enumerate(network):
        current = distance[node]
        if current is None:
            continue
        for arc in arcs:
            if arc.capacity == 0:
                continue
            candidate = current + arc.cost
            if distance[arc.to] is not None and candidate < distance[arc.to]:
                raise CandidateGraphError("negative cycle in initial cost model")
    return [value if value is not None else zero for value in distance]


def _has_alternative_optimum(
    network: list[list[_Arc]],
    potential: list[_Cost],
    zero: _Cost,
) -> bool:
    """Whether the final residual graph has a non-trivial zero-cost cycle.

    For a minimum fixed-cardinality flow all residual reduced costs are
    non-negative.  A distinct flow with the same exact objective exists iff a
    residual circulation of zero cost exists; such a circulation is composed
    only of zero-reduced-cost arcs.  A forward pair arc inside a non-trivial
    SCC witnesses an alternating cycle and therefore another matching.
    """
    adjacency: list[list[int]] = [[] for _ in network]
    reverse_adjacency: list[list[int]] = [[] for _ in network]
    zero_pair_arcs: list[tuple[int, int]] = []
    for node, arcs in enumerate(network):
        for arc in arcs:
            if arc.capacity == 0:
                continue
            reduced = arc.cost + potential[node] - potential[arc.to]
            if reduced == zero:
                adjacency[node].append(arc.to)
                reverse_adjacency[arc.to].append(node)
                if arc.pair is not None:
                    zero_pair_arcs.append((node, arc.to))

    visited = [False] * len(network)
    finish: list[int] = []

    def first(node: int) -> None:
        visited[node] = True
        for target in adjacency[node]:
            if not visited[target]:
                first(target)
        finish.append(node)

    for node in range(len(network)):
        if not visited[node]:
            first(node)

    component = [-1] * len(network)

    def second(node: int, label: int) -> None:
        component[node] = label
        for target in reverse_adjacency[node]:
            if component[target] == -1:
                second(target, label)

    label = 0
    for node in reversed(finish):
        if component[node] == -1:
            second(node, label)
            label += 1
    return any(
        source != target and component[source] == component[target]
        for source, target in zero_pair_arcs
    )


def _solve_profile_details(
    graph: CandidateGraph,
    cardinality: int,
    profile: Profile,
) -> tuple[tuple[tuple[int, int], ...], bool]:
    if cardinality == 0:
        return (), False
    if profile == "canonical":
        scientific_weights = _canonical_weights(
            (edge.scientific_key for edge in graph.edges), cardinality)
        diagnostic_weights = _canonical_weights(
            (edge.diagnostic_key for edge in graph.edges), cardinality)
    else:
        scientific_weights = {}
        diagnostic_weights = {}
    extra_count = 2 if profile == "canonical" else (0 if profile == "objective" else 1)
    zero = _Cost.zero(graph.k4_mode, extra_count)

    source = 0
    gt_offset = 1
    prediction_offset = gt_offset + graph.gt_count
    sink = prediction_offset + graph.prediction_count
    network: list[list[_Arc]] = [[] for _ in range(sink + 1)]
    for gt in range(graph.gt_count):
        _add_arc(network, source, gt_offset + gt, zero)
    for prediction in range(graph.prediction_count):
        _add_arc(network, prediction_offset + prediction, sink, zero)
    pair_arcs: dict[tuple[int, int], _Arc] = {}
    for edge in sorted(graph.edges, key=lambda item: (item.gt, item.prediction)):
        node = gt_offset + edge.gt
        _add_arc(
            network,
            node,
            prediction_offset + edge.prediction,
            _edge_cost(
                edge, graph, profile, scientific_weights, diagnostic_weights),
            (edge.gt, edge.prediction),
        )
        pair_arcs[(edge.gt, edge.prediction)] = network[node][-1]

    potential = _initial_potentials(network, source, zero)
    for _ in range(cardinality):
        distance: list[_Cost | None] = [None] * len(network)
        previous: list[tuple[int, int] | None] = [None] * len(network)
        distance[source] = zero
        serial = 0
        heap: list[tuple[_Cost, int, int]] = [(zero, serial, source)]
        while heap:
            current, _serial, node = heapq.heappop(heap)
            if distance[node] is None or current != distance[node]:
                continue
            for arc_index, arc in enumerate(network[node]):
                if arc.capacity == 0:
                    continue
                reduced = arc.cost + potential[node] - potential[arc.to]
                if reduced < zero:
                    raise CandidateGraphError(
                        "negative reduced cost; exact potential invariant failed")
                candidate = current + reduced
                if distance[arc.to] is None or candidate < distance[arc.to]:
                    distance[arc.to] = candidate
                    previous[arc.to] = (node, arc_index)
                    serial += 1
                    heapq.heappush(heap, (candidate, serial, arc.to))
        if distance[sink] is None:
            raise CandidateGraphError(
                "max-cardinality result is inconsistent with residual network")
        for node, value in enumerate(distance):
            if value is not None:
                potential[node] = potential[node] + value
        node = sink
        while node != source:
            step = previous[node]
            if step is None:
                raise CandidateGraphError("broken augmenting-path predecessor")
            parent, arc_index = step
            arc = network[parent][arc_index]
            arc.capacity -= 1
            network[node][arc.reverse].capacity += 1
            node = parent

    matching = tuple(sorted(
        pair for pair, arc in pair_arcs.items() if arc.capacity == 0
    ))
    return matching, _has_alternative_optimum(network, potential, zero)


def _solve_profile(graph: CandidateGraph, cardinality: int, profile: Profile
                   ) -> tuple[tuple[int, int], ...]:
    return _solve_profile_details(graph, cardinality, profile)[0]


def a2_exact(graph: CandidateGraph) -> CandidateResult:
    """Unchecked exact solver primitive used by oracle and unit tests.

    Scientific evaluation callers must use :func:`evaluate_a2_fail_closed` or
    :func:`evaluate_a2_batch_fail_closed`; this primitive does not enforce the
    signed A1 structural ceilings and is intentionally absent from
    :data:`__all__`.
    """
    cardinality = _hopcroft_karp_size(graph)
    objective_match, ambiguous = _solve_profile_details(
        graph, cardinality, "objective")
    objective = _objective(graph, objective_match)
    if objective.k1 != cardinality:
        raise CandidateGraphError("A2 returned the wrong matching cardinality")
    canonical = (
        _solve_profile(graph, cardinality, "canonical")
        if ambiguous else objective_match
    )
    if _objective(graph, canonical) != objective:
        raise CandidateGraphError("canonical solve changed the V optimum")
    scientific, diagnostic = _sequences(graph, canonical)
    if cardinality == 0:
        severity_upper = onset_upper = offset_upper = None
    elif not ambiguous:
        metric_sums = _metric_sums(graph, canonical)
        severity_upper = metric_sums[0] / cardinality
        onset_upper = Fraction(metric_sums[1], cardinality)
        offset_upper = Fraction(metric_sums[2], cardinality)
    else:
        severity_match = _solve_profile(graph, cardinality, "severity")
        onset_match = _solve_profile(graph, cardinality, "onset")
        offset_match = _solve_profile(graph, cardinality, "offset")
        for label, matching in (
            ("severity", severity_match),
            ("onset", onset_match),
            ("offset", offset_match),
        ):
            if _objective(graph, matching) != objective:
                raise CandidateGraphError(
                    f"{label} envelope solve changed the V optimum")
        severity_sums = _metric_sums(graph, severity_match)
        onset_sums = _metric_sums(graph, onset_match)
        offset_sums = _metric_sums(graph, offset_match)
        severity_upper = severity_sums[0] / cardinality
        onset_upper = Fraction(onset_sums[1], cardinality)
        offset_upper = Fraction(offset_sums[2], cardinality)
    return CandidateResult(
        objective=objective,
        matching=canonical,
        scientific_sequence=scientific,
        diagnostic_sequence=diagnostic,
        severity_upper=severity_upper,
        onset_upper_ticks=onset_upper,
        offset_upper_ticks=offset_upper,
        m_star=None,
    )


def rational_bit_length(value: Fraction) -> int:
    """R23A_05 exact reduced mathematical bit-length."""
    value = _fraction(value, "rational_bit_length(value)")
    return abs(value.numerator).bit_length() + value.denominator.bit_length()


@dataclass(frozen=True)
class PreflightProbe:
    gt_count: int
    prediction_count: int
    eligible_edges: int
    exact_scalar_bit_bound: int
    provisional_exceeded: tuple[str, ...]
    authority_status: str = "PROVISIONAL_DIAGNOSTIC_ONLY"


def _ceil_log2_positive(value: int) -> int:
    if value <= 1:
        return 0
    return (value - 1).bit_length()


def provisional_preflight_probe(graph: CandidateGraph) -> PreflightProbe:
    """Input-derived subgraph bound; diagnostic until the O-09 ballot.

    This intentionally does not claim to cover group-level AP or Spearman.
    It bounds exact per-subgraph V components plus severity/onset/offset
    accumulators and their division by K.  Its deliberately conservative
    fraction-sum formula dominates an unreduced common-denominator sum.
    """
    k = max(1, min(graph.gt_count, graph.prediction_count))

    def sum_bound(values: Sequence[Fraction], *, divide_by_k: bool) -> int:
        """Bound a reduced sum using the k largest denominator exponents.

        For q_i=n_i/d_i, a common-denominator construction has denominator
        bounded by the sum of the selected denominator bit lengths.  Its
        numerator is bounded by the largest
        ``A_i + sum(D_j, j!=i)`` term plus ``ceil(log2(k))`` for adding at
        most k non-negative terms.  Reduction can only make the value smaller.
        """
        if not values:
            return 2  # reduced zero is 0/1
        limit = min(k, len(values))
        ranked_denominators = sorted(
            (
                (value.denominator.bit_length(), index)
                for index, value in enumerate(values)
            ),
            reverse=True,
        )
        denominator_rank = {
            original_index: rank
            for rank, (_bits, original_index) in enumerate(ranked_denominators)
        }
        denominator_prefix = [0]
        for bits, _index in ranked_denominators:
            denominator_prefix.append(denominator_prefix[-1] + bits)
        top_denominator_sum = denominator_prefix[limit]
        largest_term = 1
        for index, value in enumerate(values):
            other_count = min(limit - 1, len(values) - 1)
            rank = denominator_rank[index]
            if other_count == 0:
                other_sum = 0
            elif rank >= other_count:
                other_sum = denominator_prefix[other_count]
            else:
                # The excluded item is inside the prefix: take one extra and
                # remove that exact item.
                other_sum = (
                    denominator_prefix[other_count + 1]
                    - value.denominator.bit_length()
                )
            largest_term = max(
                largest_term,
                abs(value.numerator).bit_length()
                + other_sum,
            )
        bound = (
            top_denominator_sum
            + largest_term
            + _ceil_log2_positive(limit)
        )
        if divide_by_k and limit > 1:
            bound += _ceil_log2_positive(limit)
        return bound

    def product_bound(values: Sequence[Fraction]) -> int:
        if not values:
            return 2  # reduced multiplicative identity 1/1
        limit = min(k, len(values))
        numerator = sum(sorted(
            (abs(value.numerator).bit_length() for value in values),
            reverse=True,
        )[:limit])
        denominator = sum(sorted(
            (value.denominator.bit_length() for value in values),
            reverse=True,
        )[:limit])
        return numerator + denominator

    def integer_sum_bound(values: Sequence[int], *, divide_by_k: bool) -> int:
        if not values:
            return 1
        limit = min(k, len(values))
        bound = max(abs(value).bit_length() for value in values)
        bound += _ceil_log2_positive(limit)
        if divide_by_k:
            # The published onset/offset scalar is not merely sum/K ticks:
            # R23_05 converts it exactly to milliseconds as
            # sum/(K*48).  The reduced value cannot exceed the unreduced
            # numerator + denominator bit bound.  This also matters at K=1,
            # where an integer q/1 already has a one-bit denominator and the
            # ms boundary contributes denominator 48.
            bound += (limit * 48).bit_length()
        return bound

    k2_values = [edge.k2_iou for edge in graph.edges]
    k4_values = [edge.k4_cost for edge in graph.edges]
    severity_values = [edge.severity_error for edge in graph.edges]
    k3_values = [edge.k3_tick_error for edge in graph.edges]
    onset_values = [edge.onset_error_ticks for edge in graph.edges]
    offset_values = [edge.offset_error_ticks for edge in graph.edges]
    exact_scalar_bound = max(
        sum_bound(k2_values, divide_by_k=False),
        (
            product_bound(k4_values)
            if graph.k4_mode == "product"
            else sum_bound(k4_values, divide_by_k=False)
        ),
        sum_bound(severity_values, divide_by_k=True),
        integer_sum_bound(k3_values, divide_by_k=False),
        integer_sum_bound(onset_values, divide_by_k=True),
        integer_sum_bound(offset_values, divide_by_k=True),
        max(
            (rational_bit_length(value)
             for value in (*k2_values, *k4_values, *severity_values)),
            default=2,
        ),
    )
    exceeded: list[str] = []
    if graph.gt_count > PROVISIONAL_MAX_GT:
        exceeded.append("GT")
    if graph.prediction_count > PROVISIONAL_MAX_PREDICTIONS:
        exceeded.append("PREDICTION")
    if len(graph.edges) > PROVISIONAL_MAX_EDGES:
        exceeded.append("ELIGIBLE_EDGES")
    if exact_scalar_bound > PROVISIONAL_MAX_EXACT_SCALAR_BITS:
        exceeded.append("EXACT_SCALAR_BIT_LENGTH")
    return PreflightProbe(
        gt_count=graph.gt_count,
        prediction_count=graph.prediction_count,
        eligible_edges=len(graph.edges),
        exact_scalar_bit_bound=exact_scalar_bound,
        provisional_exceeded=tuple(exceeded),
    )


_A1_RUNTIME_FAILURES = (
    MemoryError,
    RuntimeError,
    OverflowError,
    TimeoutError,
)
_A1_ACTIVE_AUTHORITY_STATUS = "A1_ACTIVE_ENFORCEMENT"


def evaluate_a2_fail_closed(graph: CandidateGraph) -> CandidateEvaluation:
    """Apply signed A1 limits before invoking the exact solver.

    The probe retains its historical evidence-only field names so old
    benchmark artifacts remain replayable.  This wrapper is the separate
    enforcement surface implemented in a separate post-A1 tranche: any active
    per-subgraph ceiling exceed is fatal and no solve is attempted.
    """
    if not isinstance(graph, CandidateGraph):
        raise CandidateGraphError("graph must be CandidateGraph")
    try:
        probe = replace(
            provisional_preflight_probe(graph),
            authority_status=_A1_ACTIVE_AUTHORITY_STATUS,
        )
    except CandidateGraphError as error:
        return CandidateEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            None,
            witness=str(error),
        )
    except _A1_RUNTIME_FAILURES as error:
        return CandidateEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_RUNTIME_FAILURE,
            None,
            None,
            witness=type(error).__name__,
        )
    if probe.provisional_exceeded:
        return CandidateEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_STRUCTURAL_LIMIT_EXCEEDED,
            None,
            probe,
            witness=probe.provisional_exceeded,
        )
    try:
        value = a2_exact(graph)
    except CandidateGraphError as error:
        return CandidateEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            probe,
            witness=str(error),
        )
    except _A1_RUNTIME_FAILURES as error:
        return CandidateEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_RUNTIME_FAILURE,
            None,
            probe,
            witness=type(error).__name__,
        )
    return CandidateEvaluation(
        CandidateEvaluationStatus.EVALUATED,
        CandidateEvaluationReason.OK,
        value,
        probe,
    )


def evaluate_a2_batch_fail_closed(
    graphs: tuple[CandidateGraph, ...],
) -> CandidateBatchEvaluation:
    """Evaluate gate-contributing subgraphs with fatal batch propagation.

    Every graph is preflighted before the first solve.  A structural exceed in
    any member therefore prevents all solving.  Runtime or constraint failure
    during the solve phase discards every accumulated value and returns no
    partial scientific result.
    """
    if not isinstance(graphs, tuple) or any(
            not isinstance(graph, CandidateGraph) for graph in graphs):
        raise CandidateGraphError("graphs must be a tuple of CandidateGraph")
    try:
        preflights = tuple(
            replace(
                provisional_preflight_probe(graph),
                authority_status=_A1_ACTIVE_AUTHORITY_STATUS,
            )
            for graph in graphs
        )
    except CandidateGraphError as error:
        return CandidateBatchEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_CONSTRAINT_MODEL_INVALID,
            None,
            None,
            witness=str(error),
        )
    except _A1_RUNTIME_FAILURES as error:
        return CandidateBatchEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_RUNTIME_FAILURE,
            None,
            None,
            witness=type(error).__name__,
        )
    structural = tuple(
        (index, probe.provisional_exceeded)
        for index, probe in enumerate(preflights)
        if probe.provisional_exceeded
    )
    if structural:
        return CandidateBatchEvaluation(
            CandidateEvaluationStatus.REJECTED,
            CandidateEvaluationReason.SOLVER_STRUCTURAL_LIMIT_EXCEEDED,
            None,
            preflights,
            witness=structural,
        )
    values: list[CandidateResult] = []
    for index, graph in enumerate(graphs):
        try:
            values.append(a2_exact(graph))
        except CandidateGraphError as error:
            return CandidateBatchEvaluation(
                CandidateEvaluationStatus.REJECTED,
                CandidateEvaluationReason.SOLVER_CONSTRAINT_MODEL_INVALID,
                None,
                preflights,
                witness={"graph_index": index, "detail": str(error)},
            )
        except _A1_RUNTIME_FAILURES as error:
            return CandidateBatchEvaluation(
                CandidateEvaluationStatus.REJECTED,
                CandidateEvaluationReason.SOLVER_RUNTIME_FAILURE,
                None,
                preflights,
                witness={
                    "graph_index": index,
                    "exception": type(error).__name__,
                },
            )
    return CandidateBatchEvaluation(
        CandidateEvaluationStatus.EVALUATED,
        CandidateEvaluationReason.OK,
        tuple(values),
        preflights,
    )
