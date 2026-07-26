"""G1c T2 — deterministic one-to-one event matching (§10.2).

Authority: docs/MOTORE_V3_G1_CONTRACT.md §10.2.

    "Il matching e bipartito one-to-one con obiettivo lessicografico
     deterministico: massimo numero di match validi, poi massima somma IoU,
     poi minima somma dell'errore frequenziale in ottave, poi ordine crescente
     degli ID evento. Non e ammesso un greedy dipendente dall'ordine dei
     record."

That last sentence is the whole difficulty. A greedy matcher that takes the
best-looking pair first is not merely suboptimal — it returns *different*
answers for different input orders, which would make precision and recall a
function of file order. So this module solves the objective exactly and
compares candidate matchings by a total order, never by arrival order.

Exactness, and what happens when it is not affordable
-----------------------------------------------------
The search is exhaustive with pruning over ground-truth items in canonical
order. For the sizes §10.2 deals with (events inside one evaluation segment)
that is cheap. Above `MAX_SEARCH_NODES` it raises instead of degrading to a
greedy pass: a silent fallback would reintroduce exactly the order dependence
the contract forbids, and would do it precisely on the hardest inputs.

Matchability (§10.2)
--------------------
This matcher is only for dense dynamic events: Resonance, Harshness and
Sibilance. Semantic regions use a different schema (`band_lo_hz`/`band_hi_hz`)
and must be matched by a separate evaluator path. Same problem type and
temporal IoU >= 0.3 are necessary for every event class. On
top of that, per class:
  Resonance                              centre within one third of an octave
  Harshness, Sibilance                   band overlap >= 0.5

UNPINNED (flagged, not silently chosen)
---------------------------------------
"overlap di banda almeno 0.5" does not say overlap *of what over what*:
intersection-over-union and intersection-over-smaller give different answers.
This module uses IoU on the log2-frequency interval — the natural measure on a
geometric band grid — and exposes it as ``BAND_OVERLAP_MEASURE`` so the choice
is visible and can be pinned in the contract. If the pin lands differently,
this constant is the single place to change.
"""
from __future__ import annotations

import math
from typing import Any, Iterable, Mapping, Sequence

from ml_v3.contracts.constants import ANOMALY_CLASSES

__all__ = [
    "EventMatchingError",
    "TEMPORAL_IOU_MIN",
    "BAND_OVERLAP_MIN",
    "BAND_OVERLAP_MEASURE",
    "RESONANCE_CENTRE_TOLERANCE_OCTAVES",
    "MAX_SEARCH_NODES",
    "temporal_iou",
    "band_overlap",
    "centre_error_octaves",
    "is_matchable",
    "match_events",
]

TEMPORAL_IOU_MIN = 0.3                     # §10.2
BAND_OVERLAP_MIN = 0.5                     # §10.2
RESONANCE_CENTRE_TOLERANCE_OCTAVES = 1.0 / 3.0   # "un terzo di ottava"
BAND_OVERLAP_MEASURE = "iou_log2"          # see UNPINNED note in the docstring
MAX_SEARCH_NODES = 200_000

_EVENT_CLASSES = frozenset(ANOMALY_CLASSES)
_CENTRE_CLASSES = frozenset({"Resonance"})
_BAND_CLASSES = frozenset({"Harshness", "Sibilance"})


class EventMatchingError(ValueError):
    """Raised on malformed events or on a search that cannot stay exact."""


def _finite(value: object, what: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise EventMatchingError(f"{what} must be a finite number")
    number = float(value)
    if not math.isfinite(number):
        raise EventMatchingError(f"{what} must be finite, got {number!r}")
    return number


def _problem_type(event: Mapping[str, Any], label: str) -> str:
    value = event.get("problem_type")
    if not isinstance(value, str) or not value:
        raise EventMatchingError(f"{label}.problem_type must be a non-empty string")
    if value not in _EVENT_CLASSES:
        allowed = ", ".join(sorted(_EVENT_CLASSES))
        raise EventMatchingError(
            f"{label}.problem_type {value!r} is not a §10.2 dynamic event "
            f"class; allowed: {allowed}")
    return value


def temporal_iou(a: Mapping[str, Any], b: Mapping[str, Any]) -> float:
    """Intersection over union of the two time spans."""
    a0, a1 = _finite(a["start_s"], "start_s"), _finite(a["end_s"], "end_s")
    b0, b1 = _finite(b["start_s"], "start_s"), _finite(b["end_s"], "end_s")
    if a1 <= a0 or b1 <= b0:
        raise EventMatchingError("event end_s must be greater than start_s")
    inter = min(a1, b1) - max(a0, b0)
    if inter <= 0.0:
        return 0.0
    union = (a1 - a0) + (b1 - b0) - inter
    return inter / union


def _band_log2(event: Mapping[str, Any]) -> tuple[float, float]:
    """Event band as a log2-frequency interval from centre and width."""
    centre = _finite(event["center_hz"], "center_hz")
    width = _finite(event["width_octaves"], "width_octaves")
    if centre <= 0.0:
        raise EventMatchingError("center_hz must be positive")
    if width <= 0.0:
        raise EventMatchingError("width_octaves must be positive")
    half = width / 2.0
    mid = math.log2(centre)
    return (mid - half, mid + half)


def band_overlap(a: Mapping[str, Any], b: Mapping[str, Any]) -> float:
    """Band agreement as IoU on the log2-frequency axis (see BAND_OVERLAP_MEASURE)."""
    a0, a1 = _band_log2(a)
    b0, b1 = _band_log2(b)
    inter = min(a1, b1) - max(a0, b0)
    if inter <= 0.0:
        return 0.0
    union = (a1 - a0) + (b1 - b0) - inter
    return inter / union


def centre_error_octaves(a: Mapping[str, Any], b: Mapping[str, Any]) -> float:
    """Absolute distance between centres, in octaves."""
    ca = _finite(a["center_hz"], "center_hz")
    cb = _finite(b["center_hz"], "center_hz")
    if ca <= 0.0 or cb <= 0.0:
        raise EventMatchingError("center_hz must be positive")
    return abs(math.log2(ca) - math.log2(cb))


def is_matchable(gt: Mapping[str, Any], pred: Mapping[str, Any]) -> bool:
    """§10.2 matchability: same event type, temporal IoU, per-class criterion."""
    gt_type = _problem_type(gt, "ground-truth")
    pred_type = _problem_type(pred, "prediction")
    if gt_type != pred_type:
        return False
    if temporal_iou(gt, pred) < TEMPORAL_IOU_MIN:
        return False
    if gt_type in _CENTRE_CLASSES:
        return centre_error_octaves(gt, pred) <= RESONANCE_CENTRE_TOLERANCE_OCTAVES
    if gt_type in _BAND_CLASSES:
        return band_overlap(gt, pred) >= BAND_OVERLAP_MIN
    raise EventMatchingError(f"unknown event problem_type {gt_type!r}")


def _freq_error(gt: Mapping[str, Any], pred: Mapping[str, Any]) -> float:
    """Frequency error in octaves, used by the third lexicographic objective."""
    return centre_error_octaves(gt, pred)


def _event_id(event: Mapping[str, Any], fallback: int) -> str:
    """Stable id for the final tie-break; falls back to canonical position."""
    value = event.get("event_id")
    if value is None:
        return f"#{fallback:06d}"
    if not isinstance(value, str) or not value:
        raise EventMatchingError("event_id must be a non-empty string when present")
    return value


def _canonical_order(events: Sequence[Mapping[str, Any]], label: str
                     ) -> list[tuple[str, int, Mapping[str, Any]]]:
    """Sort events into canonical order so the search never sees arrival order."""
    decorated = [
        (_event_id(event, index), index, event) for index, event in enumerate(events)]
    ids = [entry[0] for entry in decorated]
    if len(set(ids)) != len(ids):
        raise EventMatchingError(f"duplicate event_id among {label} events")
    decorated.sort(key=lambda entry: (
        _problem_type(entry[2], label),
        float(entry[2]["start_s"]),
        float(entry[2]["end_s"]),
        entry[0],
    ))
    return decorated


def match_events(
    gt_events: Iterable[Mapping[str, Any]],
    pred_events: Iterable[Mapping[str, Any]],
) -> dict[str, Any]:
    """Exact one-to-one matching under the §10.2 lexicographic objective.

    Returns matched pairs plus the unmatched ground-truth (FN) and prediction
    (FP) ids, all in canonical order. Independent of input order by
    construction: inputs are canonically sorted before the search, and
    candidate matchings are compared by a total order.
    """
    gt = _canonical_order(list(gt_events), "ground-truth")
    pred = _canonical_order(list(pred_events), "prediction")

    # Compatibility graph on canonical indices.
    compatible: list[list[int]] = []
    iou: dict[tuple[int, int], float] = {}
    ferr: dict[tuple[int, int], float] = {}
    for gi, (_gid, _gpos, gevent) in enumerate(gt):
        row: list[int] = []
        for pi, (_pid, _ppos, pevent) in enumerate(pred):
            if is_matchable(gevent, pevent):
                row.append(pi)
                iou[(gi, pi)] = temporal_iou(gevent, pevent)
                ferr[(gi, pi)] = _freq_error(gevent, pevent)
        compatible.append(row)

    best: dict[str, Any] = {"key": None, "pairs": None}
    nodes = 0

    def key_for(pairs: list[tuple[int, int]]) -> tuple:
        # Lexicographic: max count, max sum IoU, min sum freq error, then ids.
        return (
            -len(pairs),
            -sum(iou[pair] for pair in pairs),
            sum(ferr[pair] for pair in pairs),
            tuple(sorted((gt[g][0], pred[p][0]) for g, p in pairs)),
        )

    def search(gi: int, used: set[int], pairs: list[tuple[int, int]]) -> None:
        nonlocal nodes, best
        nodes += 1
        if nodes > MAX_SEARCH_NODES:
            raise EventMatchingError(
                f"exact matching exceeded {MAX_SEARCH_NODES} search nodes "
                f"({len(gt)} GT x {len(pred)} predictions); refusing to fall "
                "back to an order-dependent greedy pass")
        if gi == len(gt):
            candidate = key_for(pairs)
            if best["key"] is None or candidate < best["key"]:
                best = {"key": candidate, "pairs": list(pairs)}
            return
        # Prune: even matching everything left cannot beat the best count.
        if best["key"] is not None:
            optimistic = len(pairs) + (len(gt) - gi)
            if optimistic < -best["key"][0]:
                return
        for pi in compatible[gi]:
            if pi in used:
                continue
            used.add(pi)
            pairs.append((gi, pi))
            search(gi + 1, used, pairs)
            pairs.pop()
            used.remove(pi)
        # Leaving this ground-truth event unmatched is always a legal branch.
        search(gi + 1, used, pairs)

    search(0, set(), [])
    pairs = best["pairs"] or []

    matched_gt = {g for g, _p in pairs}
    matched_pred = {p for _g, p in pairs}
    return {
        "matched": [
            {
                "gt_id": gt[g][0],
                "pred_id": pred[p][0],
                "problem_type": gt[g][2]["problem_type"],
                "temporal_iou": iou[(g, p)],
                "centre_error_octaves": ferr[(g, p)],
            }
            for g, p in sorted(pairs, key=lambda pair: (gt[pair[0]][0], pred[pair[1]][0]))
        ],
        "false_negatives": [gt[g][0] for g in range(len(gt)) if g not in matched_gt],
        "false_positives": [pred[p][0] for p in range(len(pred)) if p not in matched_pred],
        "counts": {
            "matched": len(pairs),
            "false_negatives": len(gt) - len(matched_gt),
            "false_positives": len(pred) - len(matched_pred),
        },
        "band_overlap_measure": BAND_OVERLAP_MEASURE,
    }
