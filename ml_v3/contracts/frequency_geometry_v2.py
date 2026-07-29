"""REV8 O-02/O-03 — bit-exact frequency geometry and projection.

The live REV7 grid remains untouched.  This module is imported explicitly by
REV8 tests and tools only.  Its golden artifacts live under
``ml_v3/fixtures/rev8`` and are validated before use.
"""
from __future__ import annotations

import re
from fractions import Fraction
from functools import lru_cache
from pathlib import Path
from typing import Any, Mapping

from .canonical import (
    canonical_bytes,
    load_strict,
    sha256_of_file,
    sha256_of_obj,
)
from .interval_refinement_v2 import (
    IntervalRefinementError,
    round_log2_fraction,
    round_power_fraction,
    round_sqrt_fraction,
)
from .normalize_v2 import n64
from .numeric_authority_v2 import (
    NumericAuthorityError,
    exact_n64,
    n64_from_bits,
)

__all__ = [
    "BOUNDARY_ARTIFACT_PATH",
    "WIDTH_ARTIFACT_PATH",
    "FrequencyGeometryError",
    "build_boundary_artifact",
    "build_width_artifact",
    "derive_endpoint_tokens_v1",
    "load_boundary_artifact",
    "load_width_artifact",
    "project_center_width_v1",
    "project_interval_n64",
    "project_n64",
]

_REV8_FIXTURE_DIR = Path(__file__).resolve().parents[1] / "fixtures" / "rev8"
BOUNDARY_ARTIFACT_PATH = _REV8_FIXTURE_DIR / "o02_boundary_artifact_v1.json"
WIDTH_ARTIFACT_PATH = _REV8_FIXTURE_DIR / "o03_width_artifact_v1.json"

_BOUNDARY_SCHEMA = "aieq-v3-o02-boundary-artifact-1"
_WIDTH_SCHEMA = "aieq-v3-o03-width-artifact-1"
_GRID_VERSION = "aieq-v3-log-grid-120-rev8-1"
_PROJECTOR_VERSION = "project_center_width_v1"
_BITS_RE = re.compile(r"0x[0-9a-f]{16}\Z")

_BOUNDARY_KEYS = frozenset({
    "artifact_hash",
    "boundary_binary64_bits",
    "boundary_hash",
    "center_binary64_bits",
    "center_hash",
    "generator_provenance",
    "grid_version",
    "schema",
})
_WIDTH_KEYS = frozenset({
    "artifact_hash",
    "boundary_artifact_sha256",
    "generator_provenance",
    "projector_version",
    "schema",
    "w_max_binary64_bits",
})


class FrequencyGeometryError(ValueError):
    """Raised when an O-02/O-03 artifact or projection is invalid."""


def _bits(token: str) -> str:
    if not isinstance(token, str) or not token.startswith("f64:"):
        raise FrequencyGeometryError("expected canonical N64 token")
    return "0x" + token[4:]


def _static_provenance() -> dict[str, Any]:
    import mpmath

    return {
        "algorithm": "mpmath-iv-interval-refinement-until-one-binary64",
        "implementation": "ml_v3.contracts.frequency_geometry_v2",
        "initial_precision_dps": 80,
        "maximum_precision_dps": 1280,
        "mpmath_version": mpmath.__version__,
        "rounding": "round-to-nearest-ties-to-even",
    }


def _seal(payload: Mapping[str, Any]) -> dict[str, Any]:
    result = dict(payload)
    if "artifact_hash" in result:
        raise FrequencyGeometryError("unsealed payload contains artifact_hash")
    result["artifact_hash"] = sha256_of_obj(result)
    return result


def _generate_centers() -> tuple[list[str], list[int]]:
    centers: list[str] = []
    precisions: list[int] = []
    for index in range(120):
        if index == 0:
            result = n64(20.0)
            precision = 0
        elif index == 119:
            result = n64(20000.0)
            precision = 0
        else:
            refined = round_power_fraction(
                Fraction(1000),
                Fraction(index, 119),
                scale=Fraction(20),
                label=f"center[{index}]",
            )
            result = refined.n64
            precision = refined.precision_dps
        centers.append(result)
        precisions.append(precision)
    return centers, precisions


def _generate_boundaries(
    center_tokens: list[str],
) -> tuple[list[str], list[int]]:
    boundaries: list[str] = []
    precisions: list[int] = []
    for index, (left, right) in enumerate(
            zip(center_tokens, center_tokens[1:])):
        refined = round_sqrt_fraction(
            exact_n64(left) * exact_n64(right),
            label=f"boundary[{index}]",
        )
        boundaries.append(refined.n64)
        precisions.append(refined.precision_dps)
    return boundaries, precisions


def build_boundary_artifact() -> tuple[dict[str, Any], dict[str, Any]]:
    """Generate the O-02 artifact and its interval-refinement report."""
    centers, center_precisions = _generate_centers()
    boundaries, boundary_precisions = _generate_boundaries(centers)
    center_bits = [_bits(token) for token in centers]
    boundary_bits = [_bits(token) for token in boundaries]
    artifact = _seal({
        "schema": _BOUNDARY_SCHEMA,
        "grid_version": _GRID_VERSION,
        "center_binary64_bits": center_bits,
        "boundary_binary64_bits": boundary_bits,
        "center_hash": sha256_of_obj(center_bits),
        "boundary_hash": sha256_of_obj(boundary_bits),
        "generator_provenance": _static_provenance(),
    })
    report = {
        "center_precision_dps": center_precisions,
        "boundary_precision_dps": boundary_precisions,
        "maximum_center_precision_dps": max(center_precisions),
        "maximum_boundary_precision_dps": max(boundary_precisions),
    }
    return artifact, report


def build_width_artifact(
    boundary_artifact_sha256: str,
) -> tuple[dict[str, Any], dict[str, Any]]:
    """Generate the O-03 W_MAX artifact and certificate."""
    if not re.fullmatch(r"[0-9a-f]{64}", boundary_artifact_sha256):
        raise FrequencyGeometryError("invalid boundary artifact SHA-256")
    refined = round_log2_fraction(
        Fraction(1000),
        scale=Fraction(2),
        label="W_MAX",
    )
    artifact = _seal({
        "schema": _WIDTH_SCHEMA,
        "boundary_artifact_sha256": boundary_artifact_sha256,
        "w_max_binary64_bits": _bits(refined.n64),
        "projector_version": _PROJECTOR_VERSION,
        "generator_provenance": _static_provenance(),
    })
    report = {
        "w_max_precision_dps": refined.precision_dps,
        "w_max_lower_fraction": (
            f"{refined.lower.numerator}/{refined.lower.denominator}"
        ),
        "w_max_upper_fraction": (
            f"{refined.upper.numerator}/{refined.upper.denominator}"
        ),
    }
    return artifact, report


def _validate_canonical_file(path: Path, payload: Any) -> None:
    if path.read_bytes() != canonical_bytes(payload):
        raise FrequencyGeometryError(f"{path.name} is not canonical JSON")


def _require_exact_keys(
    payload: Any,
    expected: frozenset[str],
    *,
    label: str,
) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise FrequencyGeometryError(f"{label} must be an object")
    actual = frozenset(payload)
    if actual != expected:
        raise FrequencyGeometryError(
            f"{label} exact-key mismatch: missing={sorted(expected-actual)}, "
            f"extra={sorted(actual-expected)}")
    return payload


def _validate_bits_list(
    value: Any,
    *,
    length: int,
    label: str,
) -> list[str]:
    if not isinstance(value, list) or len(value) != length:
        raise FrequencyGeometryError(f"{label} must contain {length} entries")
    for index, bits in enumerate(value):
        if not isinstance(bits, str) or _BITS_RE.fullmatch(bits) is None:
            raise FrequencyGeometryError(f"{label}[{index}] has invalid bits")
        try:
            n64_from_bits(bits)
        except NumericAuthorityError as exc:
            raise FrequencyGeometryError(
                f"{label}[{index}] is not finite canonical binary64") from exc
    return value


def _validate_seal(payload: dict[str, Any], *, label: str) -> None:
    claimed = payload["artifact_hash"]
    if not isinstance(claimed, str) or not re.fullmatch(r"[0-9a-f]{64}", claimed):
        raise FrequencyGeometryError(f"{label}.artifact_hash is invalid")
    body = dict(payload)
    del body["artifact_hash"]
    actual = sha256_of_obj(body)
    if claimed != actual:
        raise FrequencyGeometryError(
            f"{label}.artifact_hash mismatch: {claimed} != {actual}")


@lru_cache(maxsize=1)
def load_boundary_artifact() -> dict[str, Any]:
    """Load and fully validate the frozen O-02 artifact."""
    try:
        payload = load_strict(BOUNDARY_ARTIFACT_PATH)
    except (OSError, ValueError) as exc:
        raise FrequencyGeometryError("cannot load O-02 artifact") from exc
    artifact = _require_exact_keys(
        payload, _BOUNDARY_KEYS, label="O-02 artifact")
    _validate_canonical_file(BOUNDARY_ARTIFACT_PATH, artifact)
    if artifact["schema"] != _BOUNDARY_SCHEMA:
        raise FrequencyGeometryError("O-02 schema mismatch")
    if artifact["grid_version"] != _GRID_VERSION:
        raise FrequencyGeometryError("O-02 grid version mismatch")
    centers = _validate_bits_list(
        artifact["center_binary64_bits"], length=120, label="centers")
    boundaries = _validate_bits_list(
        artifact["boundary_binary64_bits"], length=119, label="boundaries")
    if artifact["center_hash"] != sha256_of_obj(centers):
        raise FrequencyGeometryError("O-02 center_hash mismatch")
    if artifact["boundary_hash"] != sha256_of_obj(boundaries):
        raise FrequencyGeometryError("O-02 boundary_hash mismatch")
    center_values = [exact_n64(n64_from_bits(bits)) for bits in centers]
    boundary_values = [exact_n64(n64_from_bits(bits)) for bits in boundaries]
    if center_values[0] != 20 or center_values[-1] != 20000:
        raise FrequencyGeometryError("O-02 endpoint centers mismatch")
    if any(left >= right for left, right in zip(
            center_values, center_values[1:])):
        raise FrequencyGeometryError("O-02 centers are not strictly increasing")
    if any(left >= right for left, right in zip(
            boundary_values, boundary_values[1:])):
        raise FrequencyGeometryError(
            "O-02 boundaries are not strictly increasing")
    for index, boundary in enumerate(boundary_values):
        if not center_values[index] < boundary < center_values[index + 1]:
            raise FrequencyGeometryError(
                f"O-02 boundary[{index}] does not separate its centers")
    _validate_seal(artifact, label="O-02")
    expected, _report = build_boundary_artifact()
    if artifact != expected:
        raise FrequencyGeometryError(
            "O-02 artifact differs from correctly-rounded regeneration")
    return artifact


@lru_cache(maxsize=1)
def load_width_artifact() -> dict[str, Any]:
    """Load and fully validate the frozen O-03 artifact."""
    try:
        payload = load_strict(WIDTH_ARTIFACT_PATH)
    except (OSError, ValueError) as exc:
        raise FrequencyGeometryError("cannot load O-03 artifact") from exc
    artifact = _require_exact_keys(
        payload, _WIDTH_KEYS, label="O-03 artifact")
    _validate_canonical_file(WIDTH_ARTIFACT_PATH, artifact)
    if artifact["schema"] != _WIDTH_SCHEMA:
        raise FrequencyGeometryError("O-03 schema mismatch")
    if artifact["projector_version"] != _PROJECTOR_VERSION:
        raise FrequencyGeometryError("O-03 projector version mismatch")
    if artifact["boundary_artifact_sha256"] != sha256_of_file(
            BOUNDARY_ARTIFACT_PATH):
        raise FrequencyGeometryError("O-03 boundary artifact binding mismatch")
    bits = artifact["w_max_binary64_bits"]
    if not isinstance(bits, str) or _BITS_RE.fullmatch(bits) is None:
        raise FrequencyGeometryError("O-03 W_MAX bits are invalid")
    if exact_n64(n64_from_bits(bits)) <= 0:
        raise FrequencyGeometryError("O-03 W_MAX must be positive")
    _validate_seal(artifact, label="O-03")
    expected, _report = build_width_artifact(
        sha256_of_file(BOUNDARY_ARTIFACT_PATH))
    if artifact != expected:
        raise FrequencyGeometryError(
            "O-03 artifact differs from correctly-rounded regeneration")
    return artifact


def _boundary_tokens() -> tuple[str, ...]:
    artifact = load_boundary_artifact()
    return tuple(n64_from_bits(bits) for bits in artifact[
        "boundary_binary64_bits"])


def project_n64(token: str) -> int:
    """Project one finite N64 frequency to band 0..119.

    Equality with a boundary maps to the lower band.  Finite values outside
    the center range saturate naturally through the first/last comparison.
    """
    value = exact_n64(token)
    boundaries = tuple(exact_n64(item) for item in _boundary_tokens())
    for index, boundary in enumerate(boundaries):
        if value <= boundary:
            return index
    return 119


def project_interval_n64(low: str, high: str) -> tuple[int, int]:
    """Project an inclusive finite binary64 endpoint interval."""
    if exact_n64(low) > exact_n64(high):
        raise FrequencyGeometryError("raw_lo > raw_hi")
    return project_n64(low), project_n64(high)


def _w_max_token() -> str:
    return n64_from_bits(load_width_artifact()["w_max_binary64_bits"])


def derive_endpoint_tokens_v1(
    center: str,
    width: str,
) -> tuple[str, str, tuple[int, int]]:
    """Derive correctly-rounded endpoint tokens for center/width N64."""
    center_value = exact_n64(center)
    width_value = exact_n64(width)
    if center_value <= 0:
        raise FrequencyGeometryError("center_hz must be strictly positive")
    if width_value < 0:
        raise FrequencyGeometryError("width_octaves must be non-negative")
    if width_value > exact_n64(_w_max_token()):
        raise FrequencyGeometryError("width_octaves exceeds W_MAX")
    try:
        low = round_power_fraction(
            Fraction(2),
            -width_value / 2,
            scale=center_value,
            label="project_center_width_v1.raw_lo",
        )
        high = round_power_fraction(
            Fraction(2),
            width_value / 2,
            scale=center_value,
            label="project_center_width_v1.raw_hi",
        )
    except IntervalRefinementError as exc:
        raise FrequencyGeometryError(
            "center/width endpoints are not finite binary64") from exc
    if exact_n64(low.n64) > exact_n64(high.n64):
        raise FrequencyGeometryError("raw_lo > raw_hi after rounding")
    return low.n64, high.n64, (
        low.precision_dps,
        high.precision_dps,
    )


def project_center_width_v1(center: str, width: str) -> tuple[int, int]:
    """Return the inclusive canonical band interval for one event."""
    low, high, _precisions = derive_endpoint_tokens_v1(center, width)
    return project_interval_n64(low, high)
