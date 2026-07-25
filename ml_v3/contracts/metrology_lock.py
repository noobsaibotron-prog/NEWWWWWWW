"""Frozen G1a metrology lock (§13.1 / §13.2).

Authority: docs/MOTORE_V3_G1_CONTRACT.md @ 6d254d0a.

G1a serializes formulas, parameters and thresholds already defined in the
contract (warm-up/coda, stationary modes, streaming schedules, bit-identity
platform rule, SR-parity domain/activity, related gate thresholds). It does
not choose them, does not implement the resampler/frontend, and does not
evaluate parity gates.

Honesty / A15:
  §13 states the metrology in prose and does not publish a literal JSON key
  table for this lock. Envelope keys below are therefore freeze-from-prose
  packaging of §13.1/§13.2 (plus the §5 group-delay formula referenced by
  warm-up, and the §6.1 floor/clamp referenced by the activity predicate).
  Gate-platform OS/arch/python/numpy values are freeze-from-prose of
  ml_v3/environment/README.md + the numpy pin in requirements.lock.

§15 dependency-hash: the lock binds contract_revision and the T3 adapter
mapping digest so a T3/contract tip change invalidates this artifact.

Stop rule (T4.2): last hardening round on this lock. After T4.2, HIGH/MED
findings go to the debt list for T5/G1b — no new lock hash — unless a
CRITICAL vacuous-PASS or final-test leak is demonstrated. Do not invent
policy; only serialize / package contract prose.
"""
from __future__ import annotations

import platform
import sys
from copy import deepcopy
from math import gcd
from typing import Any

from .adapter import adapter_mapping_sha256
from .canonical import CanonicalError, canonical_bytes, sha256_of_obj
from .constants import (
    CANONICAL_SAMPLE_RATE,
    CONTRACT_REVISION,
    GATE_SAMPLE_RATES,
    GRID_BANDS,
)
from .grid import band_centers_hz

__all__ = [
    "MetrologyLockError",
    "METROLOGY_ARTIFACT_ID",
    "METROLOGY_SECTION",
    "GATE_PLATFORM_PYTHON",
    "HOP_SAMPLES",
    "N_LF",
    "K_WU",
    "K_CODA",
    "STREAMING_PRNG_SEED",
    "FIXED_CHUNK_SCHEDULES",
    "GEOMETRIC_CHUNK_SCHEDULE",
    "SWEEP_CHECKPOINT_HZ",
    "SR_PARITY_MAX_ABS_DB",
    "GATE_PLATFORM_FLOAT_TOL",
    "SECONDARY_FLOAT_ABS_TOL",
    "KAISER_BETA",
    "RESAMPLER_PASS_HZ",
    "STREAMING_FLOAT32_FRAME_FIELDS",
    "STREAMING_RATIONAL_TIMESTAMP_FIELDS",
    "STREAMING_VALIDITY_FIELDS",
    "STREAMING_ALSO_REQUIRED_PROOFS",
    "frozen_metrology_lock",
    "metrology_lock_bytes",
    "metrology_lock_sha256",
    "validate_metrology_lock_claim",
    "gate_platform_python_label",
    "require_gate_platform_python",
    "resampler_group_delay_rational",
    "warm_up_seconds",
    "coda_seconds",
]

METROLOGY_ARTIFACT_ID = "aieq-v3-metrology-lock-1"
METROLOGY_SECTION = "13"

# Canonical gate-platform interpreter (lock bit_identity.gate_platform.python).
# G1a evidence / re-CLOSE MUST run on this label (F4); not system 3.14.
GATE_PLATFORM_PYTHON = "CPython 3.12.13"

# §13.1 / §6.2
HOP_SAMPLES: int = 1024
N_LF: int = 8192
K_WU: int = 4
K_CODA: int = 4

# §13.2 gate 3
STREAMING_PRNG_SEED: int = 20260719
FIXED_CHUNK_SCHEDULES: tuple[int, ...] = (1, 63, 1024, 4095, 8192, 8193)
# Concrete freeze of floor(2**U), U~Uniform[0,14), length 32, PCG64(20260719).
# Authoritative list in the lock; do not re-roll after seeing FAIL.
GEOMETRIC_CHUNK_SCHEDULE: tuple[int, ...] = (
    55, 2455, 1, 109, 81, 88, 7061, 22, 499, 5337, 22, 1467, 58, 1, 142,
    233, 290, 20, 6, 18, 15955, 8039, 1, 2317, 727, 11131, 2927, 7, 1, 91,
    2096, 7,
)

# §13 sweep checkpoints: critical centres + path extremes (unique, ascending).
SWEEP_CHECKPOINT_HZ: tuple[int, ...] = (
    20, 45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000,
)

SR_PARITY_MAX_ABS_DB: float = 0.25
GAIN_INVARIANCE_MAX_ABS_DB: float = 0.05
MS_MID_EQUIVALENCE_MAX_ABS_DB: float = 0.05
ANTI_ALIAS_MAX_DB_RE_TONE: float = -80.0
GATE_PLATFORM_FLOAT_TOL: int = 0
SECONDARY_FLOAT_ABS_TOL: float = 1e-6
ACTIVITY_FLOOR_DB: float = -120.0
PSD_FLOOR_LINEAR: float = 1e-12
PSD_CLAMP_DB: tuple[float, float] = (-120.0, 12.0)
MODE_B_VARIANCE_MAX_DB2: float = 1.0
MODE_B_MAX_ABS_HOP_DELTA_DB: float = 0.5
MODE_B_T_MIN_HOPS: int = 8
KAISER_BETA: float = 9.0
RESAMPLER_PASS_HZ: float = 20000.0

# §7 float32 frame fields required for streaming≡offline identity (§13.2 gate 3).
STREAMING_FLOAT32_FRAME_FIELDS: tuple[str, ...] = (
    "mid_psd_db[120]",
    "side_psd_db[120]",
    "mid_shape_db[120]",
    "side_shape_db[120]",
    "mid_prominence_db[120]",
    "side_prominence_db[120]",
    "mid_delta_db[120]",
    "side_delta_db[120]",
    "mid_level_dbfs",
    "side_level_dbfs",
)
STREAMING_RATIONAL_TIMESTAMP_FIELDS: tuple[str, ...] = (
    "source_time_num",
    "source_time_den",
    "frame_end_sample",
    "frame_index",
)
STREAMING_VALIDITY_FIELDS: tuple[str, ...] = (
    "mid_valid",
    "side_valid",
    "valid",
    "reason",
)
STREAMING_ALSO_REQUIRED_PROOFS: tuple[dict[str, object], ...] = (
    {
        "id": "multi_asset_concat_with_explicit_delta_history_reset",
        "letter": "a",
        "requires": (
            "concatenated multi-asset input with explicit delta_db history "
            "reset at each asset boundary"
        ),
    },
    {
        "id": "interleaved_silence_between_assets",
        "letter": "b",
        "requires": "silence interleaved between assets",
    },
    {
        "id": "streaming_vs_offline_identity_same_lock_and_platform",
        "letter": "c",
        "requires": (
            "streaming-vs-offline identity on the same lock and gate platform"
        ),
    },
)

# Fixed warm-up offset excluding resampler delay: N_LF/fs_c + K_wu*H/fs_c = 32/125.
_FIXED_WU_NUM: int = 32
_FIXED_WU_DEN: int = 125
# Coda: K_coda*H/fs_c = 32/375.
_CODA_NUM: int = 32
_CODA_DEN: int = 375


class MetrologyLockError(ValueError):
    """Raised when a metrology lock claim is malformed or non-canonical."""


def _reduce(num: int, den: int) -> tuple[int, int]:
    if den <= 0:
        raise RuntimeError(f"non-positive denominator: {den}")
    g = gcd(num, den)
    return num // g, den // g


def resampler_group_delay_rational(fs_in: int) -> tuple[int, int, int | None]:
    """Return (delay_num, delay_den, num_taps) for fs_in → fs_c (§5).

    Identity (up == down == 1) → (0, 1, None). Serialize-only helper; not a
    resampler implementation.
    """
    if fs_in not in GATE_SAMPLE_RATES:
        raise MetrologyLockError(
            f"fs_in {fs_in} is not a G1 gate sample rate {GATE_SAMPLE_RATES}")
    g = gcd(fs_in, CANONICAL_SAMPLE_RATE)
    up = CANONICAL_SAMPLE_RATE // g
    down = fs_in // g
    if up == 1 and down == 1:
        return 0, 1, None
    num_taps = 128 * max(up, down) + 1
    delay_num, delay_den = _reduce(num_taps - 1, 2 * up * fs_in)
    return delay_num, delay_den, num_taps


def _per_gate_delays() -> dict[str, dict[str, object]]:
    out: dict[str, dict[str, object]] = {}
    for fs_in in GATE_SAMPLE_RATES:
        g = gcd(fs_in, CANONICAL_SAMPLE_RATE)
        up = CANONICAL_SAMPLE_RATE // g
        down = fs_in // g
        delay_num, delay_den, num_taps = resampler_group_delay_rational(fs_in)
        out[str(fs_in)] = {
            "up": up,
            "down": down,
            "num_taps": num_taps,
            "delay_num": delay_num,
            "delay_den": delay_den,
        }
    return out


def warm_up_seconds(fs_in: int) -> float:
    """Derived warm-up seconds for a gate sample rate (additive formula)."""
    delay_num, delay_den, _ = resampler_group_delay_rational(fs_in)
    return (delay_num / delay_den) + (_FIXED_WU_NUM / _FIXED_WU_DEN)


def coda_seconds() -> float:
    return _CODA_NUM / _CODA_DEN


def _grid_centers_sha256() -> str:
    return sha256_of_obj(band_centers_hz())


def frozen_metrology_lock() -> dict[str, Any]:
    """Return a deep copy of the frozen §13 metrology lock artifact."""
    return {
        "artifact_id": METROLOGY_ARTIFACT_ID,
        "contract_revision": CONTRACT_REVISION,
        "section": METROLOGY_SECTION,
        "envelope_authority": (
            "freeze-from-prose packaging of §13.1/§13.2 (+ §5 group-delay "
            "formula referenced by warm-up, §6.1 floor/clamp referenced by "
            "activity); contract has no literal JSON key table for this "
            "artifact"
        ),
        "dependencies": {
            "contract_revision": CONTRACT_REVISION,
            "adapter_mapping_sha256": adapter_mapping_sha256(),
            "grid_bands": GRID_BANDS,
            "grid_centers_sha256": _grid_centers_sha256(),
        },
        "hash_coverage": {
            "inline_dependencies_bound_here": True,
            "beyond_dependencies_covered_by": "T5_SHA256SUMS",
            "t5_sha256sums_covers_artifact_hashes_beyond_dependencies": True,
            "declaration": (
                "Artifact hashes beyond the inline dependencies object are "
                "covered by the T5 SHA256SUMS; they are not left implicit"
            ),
            "t4_2_stop_rule": (
                "T4.2 is the last hardening round on this lock; further "
                "HIGH/MED items become debt for T5/G1b (no new lock hash) "
                "unless CRITICAL vacuous-PASS or final-test leak"
            ),
        },
        "timing": {
            "H": HOP_SAMPLES,
            "fs_c": CANONICAL_SAMPLE_RATE,
            "N_LF": N_LF,
            "K_wu": K_WU,
            "K_coda": K_CODA,
            "warm_up_composition": "additive",
            "warm_up_composition_forbidden": "max",
            "warm_up_seconds_formula": (
                "resampler_group_delay_seconds + N_LF / fs_c + K_wu * H / fs_c"
            ),
            "coda_seconds_formula": "K_coda * H / fs_c",
            "fixed_offset_without_delay_num": _FIXED_WU_NUM,
            "fixed_offset_without_delay_den": _FIXED_WU_DEN,
            "coda_num": _CODA_NUM,
            "coda_den": _CODA_DEN,
            "exclude_predicate": (
                "source_time < warm_up_seconds OR "
                "source_time > T_asset - coda_seconds"
            ),
            "cross_sr_window": (
                "intersection of per-rate useful segments after warm-up and "
                "before coda (equivalent to max warm_up and max coda on the "
                "same source_time); comparing non-common tracts is FAIL"
            ),
        },
        "resampler_group_delay": {
            "authority_section": "5",
            "formula": (
                "(num_taps - 1) / (2 * up * fs_in) seconds; "
                "identity when up == down == 1 → 0"
            ),
            "num_taps_formula": "128 * max(up, down) + 1",
            "per_gate_sample_rate": _per_gate_delays(),
        },
        "resampler_generator": {
            "authority_section": "5",
            "serialize_only": True,
            "no_coefficient_implementation_in_g1a": True,
            "window": "kaiser",
            "kaiser_beta": KAISER_BETA,
            "pass_hz": RESAMPLER_PASS_HZ,
            "stop_hz_formula": "min(fs_in, 48000) / 2",
            "cutoff": "midpoint_of_pass_and_stop",
            "fc_formula": "((pass_hz + stop_hz) / 2) / (fs_in * up)",
            "coefficient_gain_scale": "up",
            "structure": "causal_polyphase",
            "polyphase_phase_zero": True,
            "streaming_state_preserved_across_blocks": True,
            "look_ahead_forbidden": True,
            "padding": "none",
            "reflection": False,
            "h0_formula": (
                "2*fc*sinc(2*fc*(n - M/2)) * kaiser(n, M, beta=9.0); "
                "h[n] = up * h0[n] / sum(h0), n = 0..M; M = num_taps - 1"
            ),
            "sinc_definition": "sin(pi*x)/(pi*x)",
        },
        "stationary_portion": {
            "gate_closing_mode": "a",
            "mode_a": {
                "definition": (
                    "entire useful segment after warm-up and before coda"
                ),
                "applies_to": ["multitone", "noise"],
                "closes_sample_rate_parity_gate": True,
                "post_hoc_subset_forbidden": True,
            },
            "mode_b": {
                "diagnostic_only": True,
                "closes_sample_rate_parity_gate": False,
                "T_min_hops": MODE_B_T_MIN_HOPS,
                "T_min_seconds_formula": "8 * H / fs_c",
                "stability_variance_max_db2": MODE_B_VARIANCE_MAX_DB2,
                "stability_max_abs_hop_delta_db": MODE_B_MAX_ABS_HOP_DELTA_DB,
                "metric": (
                    "variance and max |Δ| hop-to-hop of mean mid_psd_db over "
                    "120 bands, computed hop-per-hop inside the window"
                ),
                "preregistered_windows": [],
                "omit_failing_window_is_fail": True,
                "empty_collection_diagnostic_is_fail": True,
                "mode_change_after_fail_forbidden": True,
            },
        },
        "sweep_log_parity": {
            "checkpoint_hz": list(SWEEP_CHECKPOINT_HZ),
            "match_radius_formula": "H / fs_c",
            "frame_selection": "nearest_source_time_within_match_radius",
            "alignment_policy_ref": "sample_rate_parity.alignment",
            "missing_checkpoint_is_fail": True,
            "no_free_subset": True,
            "transient_and_damped_resonance_excluded_from_db_parity": True,
        },
        "streaming_equivalence": {
            "prng": "PCG64",
            "prng_seed": STREAMING_PRNG_SEED,
            "fixed_chunk_schedules_host_samples": list(FIXED_CHUNK_SCHEDULES),
            "geometric_schedule": {
                "rule": (
                    "floor(2 ** U) with U ~ Uniform[0, 14), length >= 32, "
                    "same PCG64 seed; repeated identical on every gate asset"
                ),
                "length": len(GEOMETRIC_CHUNK_SCHEDULE),
                "u_low": 0,
                "u_high_exclusive": 14,
                "chunks_host_samples": list(GEOMETRIC_CHUNK_SCHEDULE),
                "derivation": (
                    "concrete freeze of "
                    "numpy.random.Generator(numpy.random.PCG64(20260719))"
                    ".uniform(0, 14, size=32) then floor(2**U); the listed "
                    "chunks are authoritative"
                ),
            },
            "required_proof_surface": {
                "authority_section": "13.2.gate_3",
                "float32_frame_fields": list(STREAMING_FLOAT32_FRAME_FIELDS),
                "rational_timestamp_fields": list(
                    STREAMING_RATIONAL_TIMESTAMP_FIELDS),
                "validity_fields": list(STREAMING_VALIDITY_FIELDS),
                "validity_reason_enumerated_when_false": True,
                "identity_rule": (
                    "for every frozen schedule, chunked input and monolithic "
                    "offline input on the same lock/platform must produce the "
                    "same V3FeatureFrame sequence over the enumerated fields"
                ),
                "also_required": [
                    dict(proof) for proof in STREAMING_ALSO_REQUIRED_PROOFS
                ],
                "also_required_quantifier": (
                    "for every frozen schedule (each fixed chunk size in "
                    "fixed_chunk_schedules_host_samples and the frozen "
                    "geometric schedule), on the gate platform"
                ),
                "also_required_omission_is_fail": True,
                "also_required_applies_to": (
                    "proofs (a)(b)(c) under the same per-schedule quantifier "
                    "as identity_rule; omitting any proof on any frozen "
                    "schedule → FAIL"
                ),
            },
        },
        "bit_identity": {
            "gate_platform": {
                "os": "darwin",
                "os_marketing": "macOS 15.5",
                "arch": "arm64",
                "python": GATE_PLATFORM_PYTHON,
                "numpy": "2.5.1",
                "authority": (
                    "freeze-from-prose of ml_v3/environment/README.md and "
                    "numpy pin in ml_v3/environment/requirements.lock"
                ),
            },
            "gate_platform_float_tol": GATE_PLATFORM_FLOAT_TOL,
            "gate_platform_float_identity": "byte_identical_only",
            "gate_platform_secondary_tol_forbidden": True,
            "rule_on_gate_platform": (
                "byte_identical float32 frame fields, rational timestamps "
                "and valid flags between offline and streaming; "
                "gate_platform_float_tol is 0 (no 1e-6 abs tol on gate "
                "platform)"
            ),
            "secondary_platforms_allowlist": [],
            "secondary_float_abs_tol": SECONDARY_FLOAT_ABS_TOL,
            "secondary_is_report_only": True,
            "secondary_cannot_close_g1_gate": True,
            "ad_hoc_non_bit_identical_without_allowlist": "FAIL",
        },
        "sample_rate_parity": {
            "reference_hz": CANONICAL_SAMPLE_RATE,
            "compare_hz": [44100, 96000],
            "threshold_max_abs_db": SR_PARITY_MAX_ABS_DB,
            "aggregator": "max",
            "aggregator_forbidden": ["mean", "p95", "RMSE", "band_subset"],
            "domain_db": [
                "mid_psd_db[120]",
                "side_psd_db[120]",
                "mid_shape_db[120]",
                "side_shape_db[120]",
                "mid_prominence_db[120]",
                "side_prominence_db[120]",
                "mid_level_dbfs",
                "side_level_dbfs",
            ],
            "excluded_from_domain": ["mid_delta_db", "side_delta_db"],
            "alignment": {
                "authority_section": "5+13.2.gate_4",
                "select_by": "nearest_source_time",
                "forbidden_select_by": ["output_index", "raw_frame_index"],
                "manual_frame_shift_forbidden": True,
                "choose_within_pm1_radius_to_minimize_abs_delta_forbidden": True,
                "tie_break": [
                    "smaller_frame_index",
                    "smaller_frame_end_sample",
                ],
                "declaration": (
                    "Cross-SR frames align on nearest source_time (not raw "
                    "output index); do not manually shift frames or pick "
                    "within a ±1-sample/hop radius to minimize |Δ|; ties "
                    "break by smaller frame_index, then smaller "
                    "frame_end_sample"
                ),
            },
            "activity": {
                "predicate": "max(psd_db_ref, psd_db_sr) > -120",
                "union_cross_sr": True,
                "floor_linear": PSD_FLOOR_LINEAR,
                "clamp_db": list(PSD_CLAMP_DB),
                "activity_floor_db": ACTIVITY_FLOOR_DB,
                "shape_prominence_inherit_psd_activity": True,
                "invalid_channel_vectors_ignored": True,
                "post_hoc_mask_forbidden": True,
                "empty_active_cell_set_is_fail": True,
                "empty_useful_segment_is_fail": True,
                "max_over_empty_active_set": "FAIL",
                "vacuous_pass_forbidden": True,
                "na_is_not_pass": True,
                "empty_to_fail_derivation": {
                    "kind": "derived_packaging",
                    "does_not_supersede_contract": True,
                    "candidate_for_future_contract_amendment": True,
                    "chain": [
                        (
                            "§10.5: N/A does not satisfy a gate and does not "
                            "demonstrate improvement"
                        ),
                        (
                            "§10.1 (~line 663): a record with no active cells "
                            "has curve metrics N/A, not zero; N/A is not "
                            "converted into PASS"
                        ),
                        (
                            "§13.2 gate 4: activity predicate + aggregator "
                            "max over the declared dB domain; max over an "
                            "empty active set is not a numeric 0 PASS"
                        ),
                    ],
                    "packaging_note": (
                        "empty_active_cell_set_is_fail / "
                        "empty_useful_segment_is_fail / "
                        "max_over_empty_active_set=FAIL package the chain "
                        "above; lock packaging authority, not a new "
                        "contract amendment (REV7 out of T4.2)"
                    ),
                },
            },
            "experimental_rates_cannot_close": [88200, 176400, 192000],
            "timestamp_tolerance_canonical_samples": 1,
            "transient_onset_peak_tolerance_canonical_samples": 1,
            "transient_decay_tolerance_hops": 1,
            "single_active_cell_over_threshold_fails_gate": True,
        },
        "other_gate_thresholds": {
            "gain_invariance_max_abs_db": GAIN_INVARIANCE_MAX_ABS_DB,
            "ms_mid_equivalence_max_abs_db": MS_MID_EQUIVALENCE_MAX_ABS_DB,
            "anti_alias_max_db_re_tone": ANTI_ALIAS_MAX_DB_RE_TONE,
        },
    }


def metrology_lock_bytes() -> bytes:
    """Canonical JSON bytes of the frozen metrology lock (final LF included)."""
    return canonical_bytes(frozen_metrology_lock())


def metrology_lock_sha256() -> str:
    """SHA-256 of the canonical frozen metrology lock artifact."""
    return sha256_of_obj(frozen_metrology_lock())


def _require_exact(claim: dict[str, Any], key: str, expected: object) -> None:
    value = claim.get(key)
    if value != expected:
        raise MetrologyLockError(
            f"metrology lock claim {key!r} must equal {expected!r}, "
            f"got {value!r}")


def validate_metrology_lock_claim(claim: object) -> dict[str, Any]:
    """Fail-closed validation of a claimed metrology lock artifact.

    Rejects mutated timing constants, non-additive warm-up, activity without
    cross-SR union, empty active-cell vacuous PASS, stripped empty→FAIL
    derivation, SR alignment cherry-pick, non-zero gate-platform float tol,
    incomplete streaming proof surface / also_required per-schedule
    quantifier, missing §5 generator params / T5 hash-coverage declaration,
    mode-(b) used as gate-closing, empty/missing dependency digests, and any
    claim that is not byte-identical to the frozen lock.
    """
    if not isinstance(claim, dict):
        raise MetrologyLockError(
            f"metrology lock claim must be an object, got "
            f"{type(claim).__name__}")

    frozen = frozen_metrology_lock()
    _require_exact(claim, "artifact_id", METROLOGY_ARTIFACT_ID)
    _require_exact(claim, "contract_revision", CONTRACT_REVISION)
    _require_exact(claim, "section", METROLOGY_SECTION)

    deps = claim.get("dependencies")
    if not isinstance(deps, dict):
        raise MetrologyLockError("dependencies must be an object")
    if deps.get("contract_revision") != CONTRACT_REVISION:
        raise MetrologyLockError(
            "dependencies.contract_revision must equal CONTRACT_REVISION")
    expected_adapter = adapter_mapping_sha256()
    if deps.get("adapter_mapping_sha256") != expected_adapter:
        raise MetrologyLockError(
            "dependencies.adapter_mapping_sha256 must equal current T3 "
            f"adapter_mapping_sha256() ({expected_adapter})")
    if deps.get("grid_bands") != GRID_BANDS:
        raise MetrologyLockError(
            f"dependencies.grid_bands must be {GRID_BANDS}")
    if deps.get("grid_centers_sha256") != _grid_centers_sha256():
        raise MetrologyLockError(
            "dependencies.grid_centers_sha256 must equal sha256 of the "
            "frozen 120 band centres")

    timing = claim.get("timing")
    if not isinstance(timing, dict):
        raise MetrologyLockError("timing must be an object")
    if timing.get("H") != HOP_SAMPLES or timing.get("N_LF") != N_LF:
        raise MetrologyLockError("timing H/N_LF must match §13.1 constants")
    if timing.get("K_wu") != K_WU or timing.get("K_coda") != K_CODA:
        raise MetrologyLockError("timing K_wu/K_coda must be 4/4")
    if timing.get("warm_up_composition") != "additive":
        raise MetrologyLockError(
            "warm_up_composition must be 'additive' (REV6 residual H1; "
            "max is forbidden)")
    if timing.get("warm_up_composition_forbidden") != "max":
        raise MetrologyLockError(
            "warm_up_composition_forbidden must record that max is forbidden")

    stationary = claim.get("stationary_portion")
    if not isinstance(stationary, dict):
        raise MetrologyLockError("stationary_portion must be an object")
    if stationary.get("gate_closing_mode") != "a":
        raise MetrologyLockError(
            "gate_closing_mode must be 'a' for multitone/noise SR parity")
    mode_a = stationary.get("mode_a")
    mode_b = stationary.get("mode_b")
    if not isinstance(mode_a, dict) or not isinstance(mode_b, dict):
        raise MetrologyLockError("mode_a and mode_b must be objects")
    if mode_a.get("closes_sample_rate_parity_gate") is not True:
        raise MetrologyLockError("mode_a must close the SR parity gate")
    if mode_b.get("closes_sample_rate_parity_gate") is not False:
        raise MetrologyLockError(
            "mode_b must not close the SR parity gate (diagnostic only)")
    if mode_b.get("diagnostic_only") is not True:
        raise MetrologyLockError("mode_b must be diagnostic_only")

    activity = (
        claim.get("sample_rate_parity", {}).get("activity")
        if isinstance(claim.get("sample_rate_parity"), dict) else None
    )
    if not isinstance(activity, dict):
        raise MetrologyLockError("sample_rate_parity.activity must be an object")
    if activity.get("union_cross_sr") is not True:
        raise MetrologyLockError(
            "activity.union_cross_sr must be true (REV6 residual H2)")
    if activity.get("predicate") != "max(psd_db_ref, psd_db_sr) > -120":
        raise MetrologyLockError(
            "activity.predicate must be max(psd_db_ref, psd_db_sr) > -120")
    if activity.get("empty_active_cell_set_is_fail") is not True:
        raise MetrologyLockError(
            "activity.empty_active_cell_set_is_fail must be true "
            "(empty active-cell set under gate-4 activity → FAIL; "
            "max over empty is not 0; N/A ≠ PASS)")
    if activity.get("empty_useful_segment_is_fail") is not True:
        raise MetrologyLockError(
            "activity.empty_useful_segment_is_fail must be true")
    if activity.get("vacuous_pass_forbidden") is not True:
        raise MetrologyLockError(
            "activity.vacuous_pass_forbidden must be true")
    if activity.get("max_over_empty_active_set") != "FAIL":
        raise MetrologyLockError(
            "activity.max_over_empty_active_set must be 'FAIL'")
    derivation = activity.get("empty_to_fail_derivation")
    if not isinstance(derivation, dict):
        raise MetrologyLockError(
            "activity.empty_to_fail_derivation must be an object "
            "(derived packaging of §10.5 + §10.1 N/A≠zero + §13 gate 4; "
            "does not supersede the contract)")
    if derivation.get("does_not_supersede_contract") is not True:
        raise MetrologyLockError(
            "empty_to_fail_derivation.does_not_supersede_contract must be true")
    if derivation.get("kind") != "derived_packaging":
        raise MetrologyLockError(
            "empty_to_fail_derivation.kind must be 'derived_packaging'")
    chain = derivation.get("chain")
    if not isinstance(chain, list) or len(chain) < 3:
        raise MetrologyLockError(
            "empty_to_fail_derivation.chain must list the §10.5 / §10.1 / "
            "§13 gate-4 derivation steps")

    sr = claim.get("sample_rate_parity")
    if not isinstance(sr, dict):
        raise MetrologyLockError("sample_rate_parity must be an object")
    if sr.get("threshold_max_abs_db") != SR_PARITY_MAX_ABS_DB:
        raise MetrologyLockError(
            f"threshold_max_abs_db must be {SR_PARITY_MAX_ABS_DB}")
    if sr.get("aggregator") != "max":
        raise MetrologyLockError("sample_rate_parity.aggregator must be 'max'")
    alignment = sr.get("alignment")
    if not isinstance(alignment, dict):
        raise MetrologyLockError(
            "sample_rate_parity.alignment must be an object")
    if alignment.get("select_by") != "nearest_source_time":
        raise MetrologyLockError(
            "alignment.select_by must be 'nearest_source_time' "
            "(not raw output index)")
    if alignment.get("manual_frame_shift_forbidden") is not True:
        raise MetrologyLockError(
            "alignment.manual_frame_shift_forbidden must be true")
    if alignment.get(
            "choose_within_pm1_radius_to_minimize_abs_delta_forbidden"
    ) is not True:
        raise MetrologyLockError(
            "alignment.choose_within_pm1_radius_to_minimize_abs_delta_"
            "forbidden must be true (anti-cherry-pick)")
    if alignment.get("tie_break") != [
            "smaller_frame_index", "smaller_frame_end_sample"]:
        raise MetrologyLockError(
            "alignment.tie_break must be "
            "['smaller_frame_index', 'smaller_frame_end_sample']")

    bit_id = claim.get("bit_identity")
    if not isinstance(bit_id, dict):
        raise MetrologyLockError("bit_identity must be an object")
    if bit_id.get("gate_platform_float_tol") != GATE_PLATFORM_FLOAT_TOL:
        raise MetrologyLockError(
            "bit_identity.gate_platform_float_tol must be 0 "
            "(byte-identical only on gate platform; 1e-6 is secondary-only)")
    if bit_id.get("gate_platform_float_identity") != "byte_identical_only":
        raise MetrologyLockError(
            "bit_identity.gate_platform_float_identity must be "
            "'byte_identical_only'")
    if bit_id.get("gate_platform_secondary_tol_forbidden") is not True:
        raise MetrologyLockError(
            "bit_identity.gate_platform_secondary_tol_forbidden must be true")
    if bit_id.get("secondary_platforms_allowlist") != []:
        raise MetrologyLockError(
            "secondary_platforms_allowlist must be the empty preregistered "
            "list; ad-hoc platforms are FAIL")
    if bit_id.get("secondary_float_abs_tol") != SECONDARY_FLOAT_ABS_TOL:
        raise MetrologyLockError(
            f"secondary_float_abs_tol must remain {SECONDARY_FLOAT_ABS_TOL} "
            "(report-only; never applied on gate platform)")
    if bit_id.get("ad_hoc_non_bit_identical_without_allowlist") != "FAIL":
        raise MetrologyLockError(
            "ad_hoc_non_bit_identical_without_allowlist must be FAIL")

    streaming = claim.get("streaming_equivalence")
    if not isinstance(streaming, dict):
        raise MetrologyLockError("streaming_equivalence must be an object")
    if streaming.get("prng_seed") != STREAMING_PRNG_SEED:
        raise MetrologyLockError(
            f"prng_seed must be {STREAMING_PRNG_SEED}")
    geo = streaming.get("geometric_schedule")
    if not isinstance(geo, dict):
        raise MetrologyLockError("geometric_schedule must be an object")
    if geo.get("chunks_host_samples") != list(GEOMETRIC_CHUNK_SCHEDULE):
        raise MetrologyLockError(
            "geometric_schedule.chunks_host_samples must match the frozen "
            "PCG64(20260719) schedule")
    surface = streaming.get("required_proof_surface")
    if not isinstance(surface, dict):
        raise MetrologyLockError(
            "streaming_equivalence.required_proof_surface must be an object")
    if surface.get("float32_frame_fields") != list(
            STREAMING_FLOAT32_FRAME_FIELDS):
        raise MetrologyLockError(
            "required_proof_surface.float32_frame_fields must enumerate "
            "all §7 float32 frame fields")
    if surface.get("rational_timestamp_fields") != list(
            STREAMING_RATIONAL_TIMESTAMP_FIELDS):
        raise MetrologyLockError(
            "required_proof_surface.rational_timestamp_fields must enumerate "
            "source_time_num/den, frame_end_sample, frame_index")
    if surface.get("validity_fields") != list(STREAMING_VALIDITY_FIELDS):
        raise MetrologyLockError(
            "required_proof_surface.validity_fields must enumerate "
            "mid_valid/side_valid/valid/reason")
    also_required = surface.get("also_required")
    expected_also = [dict(p) for p in STREAMING_ALSO_REQUIRED_PROOFS]
    if also_required != expected_also:
        raise MetrologyLockError(
            "required_proof_surface.also_required must enumerate proofs "
            "(a) multi-asset concat+delta reset, (b) interleaved silence, "
            "(c) streaming-vs-offline identity")
    if surface.get("also_required_omission_is_fail") is not True:
        raise MetrologyLockError(
            "required_proof_surface.also_required_omission_is_fail must be "
            "true (proofs (a)(b)(c) required for every frozen schedule)")
    quantifier = surface.get("also_required_quantifier")
    if not isinstance(quantifier, str) or "every frozen schedule" not in quantifier:
        raise MetrologyLockError(
            "required_proof_surface.also_required_quantifier must state "
            "proofs (a)(b)(c) for every frozen schedule "
            "(fixed + geometric) on the gate platform")

    generator = claim.get("resampler_generator")
    if not isinstance(generator, dict):
        raise MetrologyLockError("resampler_generator must be an object")
    if generator.get("serialize_only") is not True:
        raise MetrologyLockError(
            "resampler_generator.serialize_only must be true (no G1a "
            "coefficient implementation)")
    if generator.get("kaiser_beta") != KAISER_BETA:
        raise MetrologyLockError(
            f"resampler_generator.kaiser_beta must be {KAISER_BETA}")
    if generator.get("pass_hz") != RESAMPLER_PASS_HZ:
        raise MetrologyLockError(
            f"resampler_generator.pass_hz must be {RESAMPLER_PASS_HZ}")
    if generator.get("structure") != "causal_polyphase":
        raise MetrologyLockError(
            "resampler_generator.structure must be 'causal_polyphase'")
    if generator.get("padding") != "none":
        raise MetrologyLockError(
            "resampler_generator.padding must be 'none'")
    if generator.get("reflection") is not False:
        raise MetrologyLockError(
            "resampler_generator.reflection must be false")
    if generator.get("streaming_state_preserved_across_blocks") is not True:
        raise MetrologyLockError(
            "resampler_generator.streaming_state_preserved_across_blocks "
            "must be true")

    coverage = claim.get("hash_coverage")
    if not isinstance(coverage, dict):
        raise MetrologyLockError("hash_coverage must be an object")
    if coverage.get(
            "t5_sha256sums_covers_artifact_hashes_beyond_dependencies"
    ) is not True:
        raise MetrologyLockError(
            "hash_coverage.t5_sha256sums_covers_artifact_hashes_beyond_"
            "dependencies must be true")
    if coverage.get("beyond_dependencies_covered_by") != "T5_SHA256SUMS":
        raise MetrologyLockError(
            "hash_coverage.beyond_dependencies_covered_by must be "
            "'T5_SHA256SUMS'")
    try:
        claim_digest = sha256_of_obj(claim)
    except CanonicalError as exc:
        raise MetrologyLockError(
            f"claim is not canonically serializable: {exc}") from exc

    frozen_digest = sha256_of_obj(frozen)
    if claim_digest != frozen_digest:
        raise MetrologyLockError(
            "metrology lock claim is not byte-identical to the frozen §13 "
            f"lock (claim_sha256={claim_digest}, "
            f"frozen_sha256={frozen_digest})")

    # Defensive: ensure caller cannot mutate the returned frozen template
    # via a shared reference (claim may be a deep copy already).
    return deepcopy(claim)


def gate_platform_python_label() -> str:
    """Return ``{implementation} {major}.{minor}.{micro}`` for this process."""
    info = sys.version_info
    return (
        f"{platform.python_implementation()} "
        f"{info.major}.{info.minor}.{info.micro}"
    )


def require_gate_platform_python() -> str:
    """Fail-closed: running interpreter must match lock gate_platform.python.

    Reads the pin from ``frozen_metrology_lock()`` (and the module constant
    ``GATE_PLATFORM_PYTHON`` as a cross-check). Wrong CPython (e.g. 3.14)
    raises ``MetrologyLockError`` — do not reinterpret or skip.
    """
    lock = frozen_metrology_lock()
    expected = lock["bit_identity"]["gate_platform"]["python"]
    if expected != GATE_PLATFORM_PYTHON:
        raise MetrologyLockError(
            "frozen lock gate_platform.python drifted from "
            f"GATE_PLATFORM_PYTHON constant: lock={expected!r}, "
            f"constant={GATE_PLATFORM_PYTHON!r}")
    actual = gate_platform_python_label()
    if actual != expected:
        raise MetrologyLockError(
            "interpreter does not match metrology lock gate_platform.python: "
            f"actual={actual!r}, expected={expected!r}; "
            "use ~/aieq_data/motore_v3/env/venv (CPython 3.12.13)")
    return actual
