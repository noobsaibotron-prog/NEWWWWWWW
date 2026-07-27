"""G1c T3 — deterministic one-to-one semantic region matching (§10.1).

Authority: docs/MOTORE_V3_G1_CONTRACT.md §10.1.

    "Una semantic region e matchabile solo con stesso tipo e IoU temporale
     almeno 0.3. Il matching per classe non usa una frequenza puntuale
     universale:
     - Resonance: centro entro un terzo di ottava;
     - Muddiness, Boominess, BoxyMidrange: overlap di banda almeno 0.5;
     - Thinness, DullSound: stessa regione e stessa direzione spettrale;
     - Harshness e Sibilance: overlap di banda almeno 0.5."

This is NOT §10.2. Dynamic events (`ml_v3.benchmark.event_matching`) carry
`center_hz`/`width_octaves` and cover three classes; semantic regions carry
`band_lo_hz`/`band_hi_hz`/`direction` and cover all eight. The two matchers
share the objective and nothing else — deliberately not one parameterised
module, because the criteria above are per-class and the schemas differ.

Ground truth is `annotation.semantic_regions[]`; prediction is
`prediction.semantic_bundles[]` plus the parent `prediction.tonal_curve_db`.

Reading of "stessa regione" (decided, not assumed)
-------------------------------------------------
§10.1 uses two different words in the same list: "overlap di **banda**" for
five classes and "stessa **regione**" for Thinness/DullSound. §11.1 defines
the canonical tonal regions, already implemented as
`contracts.grid.region_of_frequency`. This module therefore reads "regione"
as the §11.1 canonical region — not as a band-overlap threshold. The two
criteria genuinely diverge: 150-250 Hz vs 160-260 Hz have band IoU 0.811 but
straddle the 200 Hz boundary into regions 1 and 2.

CONTRACT GAPS surfaced here (reported, not silently patched)
------------------------------------------------------------
G1 §9.2 says "per Resonance e obbligatorio il centro", but
`SEMANTIC_REGION_KEYS` has no `center_hz`: the schema does not implement that
obligation. Rather than change a hashed schema, this module derives the centre
geometrically (`REGION_CENTRE_DERIVATION`). Consequence to be aware of when
reading results: with the centre derived on both sides, "centre within a third
of an octave" degenerates into "band midpoints within a third of an octave on
the log2 axis" and is blind to band *width*.

Symmetrically, §9.2 obliges a *direction* for Thinness/DullSound but not a
band — yet the §11.1 region reading needs a band to locate the region. A
§9.2-legal Thinness region with null bands is therefore unmatchable here.

UNPINNED (flagged, not silently chosen)
---------------------------------------
`BAND_OVERLAP_MEASURE`   as in §10.2: IoU on log2, the natural measure on a
                         geometric grid; kept identical to the T2 choice.
`DIRECTION_DEADBAND_DB`  §10.1 needs a predicted direction, but
                         `SEMANTIC_BUNDLE_KEYS` has no `direction` field, so
                         it must be inferred from the tonal curve. The
                         dead-band width is not derived from anything in the
                         contract. Note it is NOT the gate-4 parity threshold
                         despite sharing the value 0.25 dB. Widening it lowers
                         recall mechanically: a bundle inside the dead-band
                         reads "neutral" and can match no directional GT.
`DIRECTION_VOCABULARY`   the schema types `direction` as string-or-null with
                         no enum; boost/cut/neutral is a convention.
`DIRECTION_DOMAIN`       direction is inferred over the *prediction's own*
                         band, not over the GT-prediction band intersection.
                         Ground-truth `direction` is an intrinsic attribute of
                         a region; deriving the predicted counterpart from the
                         pair would make one bundle's direction depend on which
                         GT it is being tested against. This is a deviation
                         from the T3 mandate text and the single constant to
                         flip if the pin lands the other way.

Exactness
---------
Same objective and same refusal as §10.2: exhaustive search with pruning,
lexicographic key (max matches, max sum IoU, min sum centre error, then ids),
and above `MAX_SEARCH_NODES` it raises rather than degrading to a greedy pass
that would reintroduce order dependence on exactly the hardest inputs.

Identity
--------
Semantic regions have no id field in the schema, so ids are synthesised. They
are derived from *content* in canonical order, never from arrival position:
positional ids assigned before sorting would make the returned structure
change under a one-sided permutation of the inputs.

The consequence of having no schema identity: two records that agree on every
field this matcher reads are interchangeable, and their ids may be swapped
between runs that permute the input. Counts, pairings and metrics are
unaffected, but a caller must not use an id to recover one specific original
record. Stable external identity would need an id field in the schema.
"""
from __future__ import annotations

import math
from typing import Any, Iterable, Mapping, Sequence

from ml_v3.contracts.constants import GRID_BANDS, PROBLEM_TYPES
from ml_v3.contracts.grid import band_centers_hz, region_of_frequency
from ml_v3.contracts.schema_field_guard import SchemaFieldClaim

__all__ = [
    "SemanticRegionMatchingError",
    "SEMANTIC_REGION_MATCHING_SCHEMA_FIELD_CLAIMS",
    "TEMPORAL_IOU_MIN",
    "BAND_OVERLAP_MIN",
    "BAND_OVERLAP_MEASURE",
    "RESONANCE_CENTRE_TOLERANCE_OCTAVES",
    "REGION_CENTRE_DERIVATION",
    "REGION_CRITERION",
    "DIRECTION_DEADBAND_DB",
    "DIRECTION_VOCABULARY",
    "DIRECTION_DOMAIN",
    "MAX_SEARCH_NODES",
    "temporal_iou",
    "band_overlap",
    "region_centre_hz",
    "centre_error_octaves",
    "canonical_region_index",
    "derive_direction",
    "is_matchable",
    "match_semantic_regions",
]

TEMPORAL_IOU_MIN = 0.3                            # §10.1
BAND_OVERLAP_MIN = 0.5                            # §10.1
RESONANCE_CENTRE_TOLERANCE_OCTAVES = 1.0 / 3.0    # "un terzo di ottava"
BAND_OVERLAP_MEASURE = "iou_log2"                 # UNPINNED — see docstring
REGION_CENTRE_DERIVATION = "geometric_mean"       # §9.2 gap workaround
REGION_CRITERION = "canonical_tonal_region_11_1"  # reading of "stessa regione"
DIRECTION_DEADBAND_DB = 0.25                      # UNPINNED — see docstring
DIRECTION_VOCABULARY = ("boost", "cut", "neutral")            # UNPINNED
DIRECTION_DOMAIN = "prediction_band"              # UNPINNED — see docstring
MAX_SEARCH_NODES = 200_000

# §10.1 partitions all eight public problem types by criterion.
_CENTRE_CLASSES = frozenset({"Resonance"})
_BAND_CLASSES = frozenset({
    "Muddiness", "Boominess", "BoxyMidrange", "Harshness", "Sibilance"})
_REGION_DIRECTION_CLASSES = frozenset({"Thinness", "DullSound"})
_SEMANTIC_CLASSES = frozenset(PROBLEM_TYPES)

_REGION_FIELDS_READ = frozenset({
    "problem_type",
    "start_s",
    "end_s",
    "band_lo_hz",
    "band_hi_hz",
    "direction",
})
_BUNDLE_FIELDS_READ = frozenset({
    "problem_type",
    "start_s",
    "end_s",
    "band_lo_hz",
    "band_hi_hz",
})

SEMANTIC_REGION_MATCHING_SCHEMA_FIELD_CLAIMS = (
    SchemaFieldClaim(
        owner="ml_v3.benchmark.semantic_region_matching",
        schema_record="semantic_region",
        fields=_REGION_FIELDS_READ,
        purpose="ground-truth §10.1 semantic region matching",
    ),
    SchemaFieldClaim(
        owner="ml_v3.benchmark.semantic_region_matching",
        schema_record="semantic_bundle",
        fields=_BUNDLE_FIELDS_READ,
        purpose="prediction §10.1 semantic region matching",
    ),
    SchemaFieldClaim(
        owner="ml_v3.benchmark.semantic_region_matching",
        schema_record="prediction",
        fields=frozenset({"tonal_curve_db"}),
        purpose="inferring the predicted spectral direction (no bundle field)",
    ),
)


class SemanticRegionMatchingError(ValueError):
    """Raised on malformed regions or on a search that cannot stay exact."""


def _finite(value: object, what: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise SemanticRegionMatchingError(f"{what} must be a finite number")
    number = float(value)
    if not math.isfinite(number):
        raise SemanticRegionMatchingError(f"{what} must be finite, got {number!r}")
    return number


def _problem_type(region: Mapping[str, Any], label: str) -> str:
    value = region.get("problem_type")
    if not isinstance(value, str) or not value:
        raise SemanticRegionMatchingError(
            f"{label}.problem_type must be a non-empty string")
    if value not in _SEMANTIC_CLASSES:
        allowed = ", ".join(sorted(_SEMANTIC_CLASSES))
        raise SemanticRegionMatchingError(
            f"{label}.problem_type {value!r} is not a §10.1 semantic region "
            f"class; allowed: {allowed}")
    return value


def temporal_iou(a: Mapping[str, Any], b: Mapping[str, Any]) -> float:
    """Intersection over union of the two time spans."""
    a0, a1 = _finite(a["start_s"], "start_s"), _finite(a["end_s"], "end_s")
    b0, b1 = _finite(b["start_s"], "start_s"), _finite(b["end_s"], "end_s")
    if a1 <= a0 or b1 <= b0:
        raise SemanticRegionMatchingError("region end_s must be greater than start_s")
    inter = min(a1, b1) - max(a0, b0)
    if inter <= 0.0:
        return 0.0
    union = (a1 - a0) + (b1 - b0) - inter
    return inter / union


def _band_hz(region: Mapping[str, Any]) -> tuple[float, float] | None:
    """Band as an (lo, hi) Hz pair, or None when the schema-legal band is null.

    A *null* band is legal in `SEMANTIC_REGION_KEYS`/`SEMANTIC_BUNDLE_KEYS`, so
    it is not malformed input: it makes the region unmatchable under every
    §10.1 criterion (all of them are frequency criteria). Whether a null band
    also violates the §9.2 per-class obligations is a question for the
    annotation validator, not for the matcher.

    An *absent* key is a different thing entirely: it means the record is not a
    §10.1 region at all — a §10.2 dynamic event, say, which carries `center_hz`
    and `width_octaves` instead. Conflating the two would let a record of the
    wrong shape pass through as "no band, unmatchable" and land silently in the
    false-negative count. That is the T2 false-green in mirror image, so the
    two cases are kept apart: absent raises, null does not.

    Values that are present but unusable — non-numeric, non-positive, inverted
    — are malformed and raise.
    """
    for key in ("band_lo_hz", "band_hi_hz"):
        if key not in region:
            raise SemanticRegionMatchingError(
                f"record has no {key}: not a §10.1 semantic region "
                "(§10.2 dynamic events belong to ml_v3.benchmark.event_matching)")
    low, high = region["band_lo_hz"], region["band_hi_hz"]
    if low is None or high is None:
        return None
    lo = _finite(low, "band_lo_hz")
    hi = _finite(high, "band_hi_hz")
    if lo <= 0.0 or hi <= 0.0:
        raise SemanticRegionMatchingError("band edges must be positive")
    if hi <= lo:
        raise SemanticRegionMatchingError("band_hi_hz must exceed band_lo_hz")
    return (lo, hi)


def band_overlap(a: Mapping[str, Any], b: Mapping[str, Any]) -> float:
    """Band agreement as IoU on the log2-frequency axis (BAND_OVERLAP_MEASURE)."""
    band_a, band_b = _band_hz(a), _band_hz(b)
    if band_a is None or band_b is None:
        return 0.0
    a0, a1 = math.log2(band_a[0]), math.log2(band_a[1])
    b0, b1 = math.log2(band_b[0]), math.log2(band_b[1])
    inter = min(a1, b1) - max(a0, b0)
    if inter <= 0.0:
        return 0.0
    union = (a1 - a0) + (b1 - b0) - inter
    return inter / union


def region_centre_hz(region: Mapping[str, Any]) -> float | None:
    """Centre derived as the geometric mean of the band edges (§9.2 gap).

    `SEMANTIC_REGION_KEYS` has no `center_hz`, so the centre §10.1 asks for on
    Resonance cannot be read — only derived. The geometric mean is the midpoint
    on the log2 axis, which is the axis every other frequency criterion here
    uses. Returns None when the band is absent.
    """
    band = _band_hz(region)
    if band is None:
        return None
    return math.sqrt(band[0] * band[1])


def centre_error_octaves(a: Mapping[str, Any], b: Mapping[str, Any]) -> float | None:
    """Absolute distance between derived centres, in octaves; None if underivable."""
    ca, cb = region_centre_hz(a), region_centre_hz(b)
    if ca is None or cb is None:
        return None
    return abs(math.log2(ca) - math.log2(cb))


def canonical_region_index(region: Mapping[str, Any]) -> int | None:
    """The §11.1 canonical tonal region holding this region's derived centre.

    None when the band is absent or its centre falls outside the 20-20000 Hz
    grid — both make the region criterion unsatisfiable rather than defaulting
    to some edge region.
    """
    centre = region_centre_hz(region)
    if centre is None:
        return None
    return region_of_frequency(centre)


def _direction_label(mean_db: float) -> str:
    if mean_db >= DIRECTION_DEADBAND_DB:
        return "boost"
    if mean_db <= -DIRECTION_DEADBAND_DB:
        return "cut"
    return "neutral"


def derive_direction(
    bundle: Mapping[str, Any],
    tonal_curve_db: Sequence[float],
) -> str | None:
    """Infer a bundle's spectral direction from the parent tonal curve.

    `SEMANTIC_BUNDLE_KEYS` carries no `direction`, so §10.1's "stessa direzione
    spettrale" cannot be evaluated by reading two fields; the predicted side
    must be inferred. The mean is taken over the canonical grid bands whose
    centre lies inside the bundle's own band (DIRECTION_DOMAIN). Returns None —
    unmatchable, never a default direction — when the band is absent or so
    narrow that it contains no band centre.
    """
    curve = _tonal_curve(tonal_curve_db)
    band = _band_hz(bundle)
    if band is None:
        return None
    lo, hi = band
    centres = band_centers_hz()
    inside = [index for index in range(GRID_BANDS) if lo <= centres[index] <= hi]
    if not inside:
        return None
    return _direction_label(sum(curve[index] for index in inside) / len(inside))


def _tonal_curve(tonal_curve_db: Sequence[float]) -> list[float]:
    if isinstance(tonal_curve_db, (str, bytes)) or not isinstance(
            tonal_curve_db, Sequence):
        raise SemanticRegionMatchingError("tonal_curve_db must be a sequence")
    if len(tonal_curve_db) != GRID_BANDS:
        raise SemanticRegionMatchingError(
            f"tonal_curve_db must have {GRID_BANDS} entries, "
            f"got {len(tonal_curve_db)}")
    return [_finite(value, "tonal_curve_db") for value in tonal_curve_db]


def _gt_direction(region: Mapping[str, Any]) -> str | None:
    """Ground-truth direction; absent key means the record is not a region."""
    if "direction" not in region:
        raise SemanticRegionMatchingError(
            "ground-truth record has no direction: not a §10.1 semantic region")
    value = region["direction"]
    if value is None:
        return None
    if not isinstance(value, str) or not value:
        raise SemanticRegionMatchingError(
            "ground-truth direction must be a non-empty string when present")
    if value not in DIRECTION_VOCABULARY:
        allowed = ", ".join(DIRECTION_VOCABULARY)
        raise SemanticRegionMatchingError(
            f"ground-truth direction {value!r} outside the declared "
            f"vocabulary; allowed: {allowed}")
    return value


def is_matchable(
    gt: Mapping[str, Any],
    pred: Mapping[str, Any],
    *,
    tonal_curve_db: Sequence[float],
) -> bool:
    """§10.1 matchability: same type, temporal IoU, then the per-class criterion."""
    gt_type = _problem_type(gt, "ground-truth")
    pred_type = _problem_type(pred, "prediction")
    if gt_type != pred_type:
        return False
    if temporal_iou(gt, pred) < TEMPORAL_IOU_MIN:
        return False
    if gt_type in _CENTRE_CLASSES:
        error = centre_error_octaves(gt, pred)
        return error is not None and error <= RESONANCE_CENTRE_TOLERANCE_OCTAVES
    if gt_type in _BAND_CLASSES:
        return band_overlap(gt, pred) >= BAND_OVERLAP_MIN
    if gt_type in _REGION_DIRECTION_CLASSES:
        gt_region = canonical_region_index(gt)
        pred_region = canonical_region_index(pred)
        if gt_region is None or pred_region is None or gt_region != pred_region:
            return False
        gt_direction = _gt_direction(gt)
        if gt_direction is None:
            return False
        return gt_direction == derive_direction(pred, tonal_curve_db)
    raise SemanticRegionMatchingError(
        f"unknown semantic region problem_type {gt_type!r}")


def _sort_key(region: Mapping[str, Any], label: str) -> tuple:
    """Total order over the fields the matcher reads — content only, no position.

    Every read field participates, so two regions with an equal key are
    interchangeable for matching and the canonical sequence is the same for any
    permutation of the input. Nulls sort last within each field.
    """
    band = _band_hz(region)
    direction = region.get("direction")
    return (
        _problem_type(region, label),
        _finite(region["start_s"], "start_s"),
        _finite(region["end_s"], "end_s"),
        band is None,
        band[0] if band is not None else 0.0,
        band[1] if band is not None else 0.0,
        direction is None,
        direction if isinstance(direction, str) else "",
    )


def _canonical_order(
    regions: Sequence[Mapping[str, Any]],
    label: str,
    prefix: str,
) -> list[tuple[str, Mapping[str, Any]]]:
    """Sort by content, then number: ids that do not depend on arrival order."""
    ordered = sorted(regions, key=lambda region: _sort_key(region, label))
    return [(f"{prefix}#{position:06d}", region)
            for position, region in enumerate(ordered)]


def match_semantic_regions(
    gt_regions: Iterable[Mapping[str, Any]],
    pred_bundles: Iterable[Mapping[str, Any]],
    *,
    tonal_curve_db: Sequence[float],
) -> dict[str, Any]:
    """Exact one-to-one matching under the §10.1 lexicographic objective.

    Returns matched pairs plus unmatched ground-truth (FN) and prediction (FP)
    ids in canonical order. Independent of input order by construction: inputs
    are canonically sorted by content before the search, ids are derived from
    that order, and candidate matchings are compared by a total order.
    """
    curve = _tonal_curve(tonal_curve_db)
    gt = _canonical_order(list(gt_regions), "ground-truth", "gt")
    pred = _canonical_order(list(pred_bundles), "prediction", "pred")

    # Compatibility graph on canonical indices.
    compatible: list[list[int]] = []
    iou: dict[tuple[int, int], float] = {}
    ferr: dict[tuple[int, int], float] = {}
    for gi, (_gid, gregion) in enumerate(gt):
        row: list[int] = []
        for pi, (_pid, pregion) in enumerate(pred):
            if is_matchable(gregion, pregion, tonal_curve_db=curve):
                row.append(pi)
                iou[(gi, pi)] = temporal_iou(gregion, pregion)
                # Matchable pairs always have both bands: every §10.1 criterion
                # needs one, so the derived centre error is never None here.
                error = centre_error_octaves(gregion, pregion)
                if error is None:  # pragma: no cover - unreachable by criterion
                    raise SemanticRegionMatchingError(
                        "matchable pair without a derivable centre error")
                ferr[(gi, pi)] = error
        compatible.append(row)

    best: dict[str, Any] = {"key": None, "pairs": None}
    nodes = 0

    def key_for(pairs: list[tuple[int, int]]) -> tuple:
        # Lexicographic: max count, max sum IoU, min sum centre error, then ids.
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
            raise SemanticRegionMatchingError(
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
        # Leaving this ground-truth region unmatched is always a legal branch.
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
                "problem_type": gt[g][1]["problem_type"],
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
        "region_criterion": REGION_CRITERION,
        "region_centre_derivation": REGION_CENTRE_DERIVATION,
        "direction_domain": DIRECTION_DOMAIN,
        "direction_deadband_db": DIRECTION_DEADBAND_DB,
    }
