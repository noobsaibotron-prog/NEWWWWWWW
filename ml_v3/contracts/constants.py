"""Frozen canonical constants for Motore v3 (G1a).

Every value here is transcribed from docs/MOTORE_V3_G1_CONTRACT.md
(REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a). G1a serializes these; it
does not choose them (contract §10.5: "G1a serializza mapping, costanti e hash
gia definiti qui; non puo sceglierli o modificarli").
"""
from __future__ import annotations

from typing import Final

__all__ = [
    "CONTRACT_REVISION",
    "CONDITIONING_PROFILES", "PROFILE_TO_ID", "ID_TO_PROFILE",
    "PROBLEM_TYPES", "PROBLEM_TYPE_TO_ID", "ID_TO_PROBLEM_TYPE",
    "ANOMALY_CLASSES",
    "SPLIT_ROLES", "ROLE_QUOTA_BOUNDS", "ROLE_INTERVALS_EXACT",
    "BENCHMARK_FAMILIES",
    "ACCEPTED_SAMPLE_RATES", "GATE_SAMPLE_RATES", "CANONICAL_SAMPLE_RATE",
    "TONAL_REGIONS",
    "GRID_BANDS", "GRID_MIN_HZ", "GRID_MAX_HZ",
    "PROFILE_ROLE_FLOORS", "FINAL_TEST_ELECTRONIC_MIN_FRACTION",
    "LICENSE_CLASSES",
    "SPLIT_SALT_BYTES", "COMMITMENT_PREFIX", "ROLE_PREFIX", "PILOT_PREFIX",
    "PILOT_THRESHOLD_NUM", "PILOT_THRESHOLD_DEN",
    "TONAL_CURVE_MIN_DB", "TONAL_CURVE_MAX_DB",
    "SCHEMA_IDS",
]

# Frozen contract identity for this G1a tranche (document commit 6d254d0a).
CONTRACT_REVISION: Final[str] = (
    "MOTORE_V3_G1_CONTRACT REVISIONE 6 CONSOLIDATA + micro-amend @ 6d254d0a"
)

# --- §4.2: the seven host conditioning profiles, ids 0..6
CONDITIONING_PROFILES: Final[tuple[str, ...]] = (
    "generic", "vocals", "drums", "bass", "synth", "master", "edm",
)
PROFILE_TO_ID: Final[dict[str, int]] = {
    name: index for index, name in enumerate(CONDITIONING_PROFILES)
}
ID_TO_PROFILE: Final[dict[int, str]] = {
    index: name for name, index in PROFILE_TO_ID.items()
}

# --- §9.2: the eight public problem types, ids 0..7
PROBLEM_TYPES: Final[tuple[str, ...]] = (
    "Resonance", "Harshness", "Muddiness", "Sibilance",
    "Boominess", "Thinness", "BoxyMidrange", "DullSound",
)
PROBLEM_TYPE_TO_ID: Final[dict[str, int]] = {
    name: index for index, name in enumerate(PROBLEM_TYPES)
}
ID_TO_PROBLEM_TYPE: Final[dict[int, str]] = {
    index: name for name, index in PROBLEM_TYPE_TO_ID.items()
}

# --- §9.3: dense anomaly surface class order
ANOMALY_CLASSES: Final[tuple[str, ...]] = ("Resonance", "Harshness", "Sibilance")

# --- §8.1 / §8.2.5: roles and exact integer quota thresholds
SPLIT_ROLES: Final[tuple[str, ...]] = (
    "train", "validation", "calibration", "development-metric", "final-test",
)
# Declared quotas 55%, 10%, 10%, 10%, 15% -> cumulative interval bounds (docs only).
ROLE_QUOTA_BOUNDS: Final[tuple[float, ...]] = (0.55, 0.65, 0.75, 0.85, 1.0)
# Exact integer comparison per §8.2.5: value * 20 < numerator * 2**64.
# Store (role, numerator) — NEVER pre-floor with // 20 (truncation shifts
# boundaries for numerators that do not divide 2**64 evenly).
ROLE_INTERVALS_EXACT: Final[tuple[tuple[str, int], ...]] = (
    ("train", 11),
    ("validation", 13),
    ("calibration", 15),
    ("development-metric", 17),
    ("final-test", 20),
)

# --- §9.1 / §11.1: the five benchmark families
BENCHMARK_FAMILIES: Final[tuple[str, ...]] = (
    "tonal-controlled", "tonal-natural", "anomaly-natural",
    "clean-safety", "electronic-stratified",
)

# --- §4.1: accepted host sample rates and G1 gate rates
ACCEPTED_SAMPLE_RATES: Final[tuple[int, ...]] = (
    44100, 48000, 88200, 96000, 176400, 192000,
)
GATE_SAMPLE_RATES: Final[tuple[int, ...]] = (44100, 48000, 96000)
CANONICAL_SAMPLE_RATE: Final[int] = 48000

# --- §11.1: canonical tonal regions, last one inclusive
TONAL_REGIONS: Final[tuple[tuple[float, float], ...]] = (
    (20.0, 80.0), (80.0, 200.0), (200.0, 500.0), (500.0, 2000.0),
    (2000.0, 5000.0), (5000.0, 10000.0), (10000.0, 20000.0),
)

# --- §6.1: exactly 120 centres, 20 Hz .. 20000 Hz
GRID_BANDS: Final[int] = 120
GRID_MIN_HZ: Final[float] = 20.0
GRID_MAX_HZ: Final[float] = 20000.0

# --- §8.1: general per-profile split floors
PROFILE_ROLE_FLOORS: Final[dict[str, int]] = {
    "train": 10, "validation": 3, "calibration": 3,
    "development-metric": 3, "final-test": 5,
}
FINAL_TEST_ELECTRONIC_MIN_FRACTION: Final[float] = 0.40

# --- §9.1: allowed licence classes
LICENSE_CLASSES: Final[tuple[str, ...]] = ("CC0", "CC-BY", "OWNED")

# --- §8.2.4 / §8.2.5 / §8.2.6: commit-reveal and HMAC prefixes
SPLIT_SALT_BYTES: Final[int] = 32
COMMITMENT_PREFIX: Final[bytes] = b"aieq-v3-split-salt-v1"
ROLE_PREFIX: Final[bytes] = b"aieq-v3-role-v1"
PILOT_PREFIX: Final[bytes] = b"aieq-v3-pilot-v1"
# pilot true when value * 4 < 2**256 (§8.2.6)
PILOT_THRESHOLD_NUM: Final[int] = 1
PILOT_THRESHOLD_DEN: Final[int] = 4

# --- §9.2: public curves are bounded to [-9, +9] dB
TONAL_CURVE_MIN_DB: Final[float] = -9.0
TONAL_CURVE_MAX_DB: Final[float] = 9.0

# --- schema identifiers frozen for G1a (§8.2.3 + §9 stubs)
SCHEMA_IDS: Final[dict[str, str]] = {
    "asset_manifest": "aieq-v3-asset-manifest-1",
    "admission_batch": "aieq-v3-admission-batch-1",
    "annotation": "aieq-v3-annotation-1",
    "prediction": "aieq-v3-prediction-1",
    "calibration_policy": "aieq-v3-calibration-policy-1",
    "benchmark_power_plan": "aieq-v3-benchmark-power-plan-1",
    "admission_roster": "aieq-v3-admission-roster-1",
    "source_identity_index": "aieq-v3-source-identity-index-1",
    "feature_frame": "aieq-v3-feature-frame-1",
}
