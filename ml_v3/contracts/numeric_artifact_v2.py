"""REV8 O-18 — frozen numeric authority artifact.

The artifact pins the byte-level examples that distinguish exact optimizer
arithmetic from published binary64 reductions.  It is candidate-only and is
not exported by the live REV7 dispatcher.
"""
from __future__ import annotations

import re
from fractions import Fraction
from functools import lru_cache
from pathlib import Path
from typing import Any, Mapping

from .canonical import canonical_bytes, load_strict, sha256_of_obj
from .exact_arith_v2 import exact
from .interval_refinement_v2 import round_log2_fraction
from .normalize_v2 import n64
from .numeric_authority_v2 import (
    exact_n64,
    mean64,
    rn64,
    sum_pairwise64,
)

__all__ = [
    "NUMERIC_ARTIFACT_PATH",
    "NumericArtifactError",
    "build_numeric_artifact",
    "load_numeric_artifact",
]

_REV8_FIXTURE_DIR = Path(__file__).resolve().parents[1] / "fixtures" / "rev8"
NUMERIC_ARTIFACT_PATH = _REV8_FIXTURE_DIR / "o18_numeric_artifact_v1.json"
_SCHEMA = "aieq-v3-o18-numeric-artifact-1"
_VERSION = "rev8-o18-r23c-1"
_KEYS = frozenset({
    "artifact_hash",
    "compound_rounding",
    "exact_integer_goldens",
    "generator_provenance",
    "mean64_goldens",
    "n64_goldens",
    "numeric_version",
    "reduction_edge_goldens",
    "rounding_goldens",
    "schema",
    "sum_pairwise64_goldens",
    "two_ulp_fixture",
    "validation_goldens",
})


class NumericArtifactError(ValueError):
    """Raised when the O-18 artifact is malformed or inconsistent."""


def _seal(payload: Mapping[str, Any]) -> dict[str, Any]:
    result = dict(payload)
    result["artifact_hash"] = sha256_of_obj(result)
    return result


def build_numeric_artifact() -> tuple[dict[str, Any], dict[str, Any]]:
    """Build all O-18 goldens from the signed operators."""
    values = [n64(0.2), n64(0.3), n64(1.0)]
    pairwise = sum_pairwise64(values)
    entries = [
        {"key": ["g2"], "value": values[1]},
        {"key": ["g1"], "value": values[0]},
        {"key": ["g3"], "value": values[2]},
    ]
    mean = mean64((entry["key"], entry["value"]) for entry in entries)
    if mean is None:
        raise NumericArtifactError("non-empty mean unexpectedly produced N/A")

    utf8_entries = [
        {"key": "\n", "value": n64(1.0)},
        {"key": "!", "value": n64(1e16)},
        {"key": "a", "value": n64(-1e16)},
    ]
    utf8_mean = mean64(
        ((entry["key"], entry["value"]) for entry in utf8_entries),
        key_domain="utf8",
    )
    if utf8_mean is None:
        raise NumericArtifactError(
            "non-empty UTF-8 mean unexpectedly produced N/A")

    compound = round_log2_fraction(
        Fraction(9, 8),
        scale=Fraction(1, 3),
        label="O-18 compound log2(9/8)/3",
    )
    base = round_log2_fraction(
        Fraction(9, 8),
        label="O-18 double-round mutation base",
    )
    double_round_mutation = rn64(exact_n64(base.n64) / 3)

    left = "f64:3fc999999999999a"
    right = "f64:3fc9999999999998"
    difference = exact_n64(left) - exact_n64(right)
    one = "f64:3ff0000000000000"
    one_next = "f64:3ff0000000000001"
    one_next_even = "f64:3ff0000000000002"
    midpoint_even_down = (
        exact_n64(one) + exact_n64(one_next)
    ) / 2
    midpoint_even_up = (
        exact_n64(one_next) + exact_n64(one_next_even)
    ) / 2
    minimum_subnormal = "f64:0000000000000001"
    maximum_finite = "f64:7fefffffffffffff"

    artifact = _seal({
        "schema": _SCHEMA,
        "numeric_version": _VERSION,
        "n64_goldens": [
            {"source": "0.0", "token": n64(0.0)},
            {"source": "-0.0", "token": n64(-0.0)},
            {"source": "0.2", "token": n64(0.2)},
            {"source": "1.2", "token": n64(1.2)},
        ],
        "exact_integer_goldens": [
            2 ** 53 + 1,
            2 ** 60 + 1,
        ],
        "two_ulp_fixture": {
            "left": left,
            "right": right,
            "positive_binary64_order_steps": 2,
            "difference_numerator": difference.numerator,
            "difference_denominator": difference.denominator,
        },
        "sum_pairwise64_goldens": [{
            "inputs": values,
            "expected": pairwise,
        }],
        "mean64_goldens": [{
            "entries": entries,
            "key_domain": "canonical",
            "expected": mean,
        }, {
            "entries": utf8_entries,
            "key_domain": "utf8",
            "expected": utf8_mean,
        }],
        "reduction_edge_goldens": {
            "sum_empty": "CALLER_NA_OR_FAIL",
            "mean_empty": None,
            "sum_singleton": {
                "input": one_next,
                "expected": one_next,
            },
            "mean_singleton": {
                "key": ["singleton"],
                "input": one_next,
                "expected": one_next,
            },
        },
        "rounding_goldens": {
            "half_ulp_tie_even_down": {
                "numerator": midpoint_even_down.numerator,
                "denominator": midpoint_even_down.denominator,
                "expected": one,
            },
            "half_ulp_tie_even_up": {
                "numerator": midpoint_even_up.numerator,
                "denominator": midpoint_even_up.denominator,
                "expected": one_next_even,
            },
            "half_minimum_subnormal_tie_to_zero": {
                "numerator": 1,
                "denominator": 2 ** 1075,
                "expected": "f64:0000000000000000",
            },
            "minimum_positive_subnormal": {
                "input": minimum_subnormal,
                "expected": minimum_subnormal,
            },
            "maximum_finite": {
                "input": maximum_finite,
                "expected": maximum_finite,
            },
        },
        "validation_goldens": {
            "equal_key_equal_value": "ALLOWED",
            "equal_key_different_value": "FAIL_NON_TOTAL_KEY",
            "macro_group_order": "RAW_UTF8_BYTES",
        },
        "compound_rounding": {
            "ratio_numerator": 9,
            "ratio_denominator": 8,
            "divisor": 3,
            "correct_single_round": compound.n64,
            "double_round_mutation": double_round_mutation,
        },
        "generator_provenance": {
            "implementation": "ml_v3.contracts.numeric_artifact_v2",
            "rounding": "round-to-nearest-ties-to-even",
            "transcendental": (
                "mpmath-iv-interval-refinement-until-one-binary64"
            ),
        },
    })
    report = {
        "compound_precision_dps": compound.precision_dps,
        "compound_lower_fraction": (
            f"{compound.lower.numerator}/{compound.lower.denominator}"
        ),
        "compound_upper_fraction": (
            f"{compound.upper.numerator}/{compound.upper.denominator}"
        ),
        "double_round_base_precision_dps": base.precision_dps,
    }
    return artifact, report


def _validate_seal(payload: dict[str, Any]) -> None:
    claimed = payload["artifact_hash"]
    if not isinstance(claimed, str) or not re.fullmatch(r"[0-9a-f]{64}", claimed):
        raise NumericArtifactError("invalid O-18 artifact_hash")
    body = dict(payload)
    del body["artifact_hash"]
    if claimed != sha256_of_obj(body):
        raise NumericArtifactError("O-18 artifact_hash mismatch")


@lru_cache(maxsize=1)
def load_numeric_artifact() -> dict[str, Any]:
    """Load, byte-check and recompute every frozen O-18 golden."""
    try:
        payload = load_strict(NUMERIC_ARTIFACT_PATH)
    except (OSError, ValueError) as exc:
        raise NumericArtifactError("cannot load O-18 artifact") from exc
    if not isinstance(payload, dict) or frozenset(payload) != _KEYS:
        raise NumericArtifactError("O-18 exact-key mismatch")
    if NUMERIC_ARTIFACT_PATH.read_bytes() != canonical_bytes(payload):
        raise NumericArtifactError("O-18 artifact is not canonical JSON")
    if payload["schema"] != _SCHEMA or payload["numeric_version"] != _VERSION:
        raise NumericArtifactError("O-18 schema/version mismatch")
    _validate_seal(payload)

    expected, _report = build_numeric_artifact()
    if payload != expected:
        raise NumericArtifactError("O-18 artifact differs from recomputed goldens")

    for value in payload["exact_integer_goldens"]:
        if exact(value) != Fraction(value):
            raise NumericArtifactError("O-18 exact integer path lost precision")
    return payload
