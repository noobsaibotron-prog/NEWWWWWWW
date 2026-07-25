"""Minimal valid example documents for G1a T2 validators / fixtures.

Instance goldens live under ``ml_v3/fixtures/g1/examples/`` (not the
normative schema surface — that is ``schema_registry_v1.json``).
"""
from __future__ import annotations

from ml_v3.contracts.constants import (
    BENCHMARK_FAMILIES,
    GRID_BANDS,
    PROBLEM_TYPES,
    SCHEMA_IDS,
)
from ml_v3.contracts.split import salt_commitment

_HEX_A = "aa" * 32
_HEX_B = "bb" * 32
_HEX_C = "cc" * 32
_HEX_D = "dd" * 32
_HEX_E = "ee" * 32
# §8.2.4 coherent commit-reveal: reveal = hex(32 raw salt bytes);
# commitment = SHA256(b"aieq-v3-split-salt-v1" + 0x00 + salt).
_SALT_REVEAL = _HEX_E
_SALT_COMMITMENT = salt_commitment(bytes.fromhex(_SALT_REVEAL))
# Historical mismatched pair (dd commitment / ee reveal) — must FAIL.
_MISMATCHED_SALT_COMMITMENT = _HEX_D


def zeros120() -> list[float]:
    return [0.0] * GRID_BANDS


def false120() -> list[bool]:
    return [False] * GRID_BANDS


def half120() -> list[float]:
    return [0.5] * GRID_BANDS


def asset_manifest(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["asset_manifest"],
        "asset_id": "asset-001",
        "relative_path": "audio/asset-001.wav",
        "sha256": _HEX_A,
        "group_id": "fsld:track001",
        "admission_batch_id": _HEX_B,
        "split_role": "calibration",
        "benchmark_families": ["tonal-controlled", "clean-safety"],
        "development_pilot": False,
        "source_profile": "generic",
        "primary_domain": "music",
        "group_primary_profile": "generic",
        "group_primary_domain": "music",
        "source_family": "fsld",
        "electronic_subgenre": None,
        "sample_rate": 48000,
        "channels": 2,
        "duration_s": 12.5,
        "parent_asset_id": None,
        "derivative_kind": None,
        "license_class": "CC0",
        "license_url": "",
        "attribution": "",
        "ledger_id": "",
    }
    doc.update(overrides)
    return doc


def admission_batch(**overrides: object) -> dict:
    # alias_mapping_version intentionally absent: identity-index (§8) field,
    # not part of §9.1 admission-batch freeze-from-prose envelope.
    doc: dict = {
        "schema": SCHEMA_IDS["admission_batch"],
        "admission_batch_id": _HEX_B,
        "source_snapshot_id": "snap-1",
        "source_snapshot_sha256": _HEX_C,
        "inclusion_rules_version": "incl-v1",
        "roster_sha256": _HEX_B,
        "salt_commitment": _SALT_COMMITMENT,
        "salt_reveal": _SALT_REVEAL,
        "roster_commit": "deadbeef",
        "reviewer_id": "reviewer-a",
        "status": "admitted",
    }
    doc.update(overrides)
    return doc


def admission_batch_mismatched_commitment(**overrides: object) -> dict:
    """Legacy dd/ee pair: commitment does not match reveal (§8.2.4 FAIL)."""
    return admission_batch(
        salt_commitment=_MISMATCHED_SALT_COMMITMENT,
        salt_reveal=_SALT_REVEAL,
        **overrides,
    )


def annotation_clean(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["annotation"],
        "asset_id": "asset-001",
        "annotator_id": "ann-1",
        "pass_id": "pass-1",
        "profile": "generic",
        "evaluation_unit_id": "eu-001",
        "segment_start_s": 0.0,
        "segment_end_s": 8.0,
        "tonal_correction_db": zeros120(),
        "tonal_confidence": half120(),
        "tonal_actionable_mask": false120(),
        "semantic_regions": [],
        "dynamic_events": [],
        "complete_types": list(PROBLEM_TYPES),
        "explicit_negative_types": list(PROBLEM_TYPES),
        "global_actionable": False,
        "clean_for_action": True,
        "notes": "",
        "tool_version": "annot-tool-1",
    }
    doc.update(overrides)
    return doc


def prediction(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["prediction"],
        "asset_id": "asset-001",
        "model_id": "model-x",
        "model_sha256": _HEX_A,
        "frontend_contract_sha256": _HEX_B,
        "calibration_policy_id": "calib-1",
        "calibration_policy_sha256": _HEX_C,
        "profile": "generic",
        "tonal_curve_db": zeros120(),
        "tonal_score": half120(),
        "tonal_confidence": half120(),
        "segment_start_s": 0.0,
        "segment_end_s": 8.0,
        "semantic_bundles": [],
        "events": [],
        "anomaly_score_ref": {
            "relative_path": "surfaces/score.npy",
            "sha256": _HEX_D,
            "num_feature_frames": 16,
            "dtype": "float32_le",
        },
        "anomaly_severity_ref": {
            "relative_path": "surfaces/severity.npy",
            "sha256": _HEX_E,
            "num_feature_frames": 16,
            "dtype": "float32_le",
        },
    }
    doc.update(overrides)
    return doc


def calibration_policy(**overrides: object) -> dict:
    doc: dict = {
        "schema": SCHEMA_IDS["calibration_policy"],
        "policy_id": "calib-1",
        "policy_version": "1",
        "model_sha256": _HEX_A,
        "frontend_sha256": _HEX_B,
        "calibration_manifest_sha256": _HEX_C,
        "prediction_schema_id": SCHEMA_IDS["prediction"],
        "prediction_schema_sha256": _HEX_D,
        "tonal_calibrator": {"algorithm": "isotonic", "parameters": {}},
        "anomaly_calibrator": {"algorithm": "platt", "parameters": {"A": 1.0}},
        "tonal_band_thresholds": half120(),
        "semantic_type_thresholds": [0.5] * 8,
        "anomaly_class_thresholds": [0.5, 0.5, 0.5],
        "candidate_extractor_version": "cand-v1",
        "score_to_confidence": [
            {"score": 0.0, "confidence": 0.0},
            {"score": 0.5, "confidence": 0.4},
            {"score": 1.0, "confidence": 1.0},
        ],
        "region_to_bundle": {"rule_id": "r2b-v1", "parameters": {}},
        "threshold_to_actionable": {"rule_id": "t2a-v1", "parameters": {}},
    }
    doc.update(overrides)
    return doc


def benchmark_power_plan(**overrides: object) -> dict:
    m = 2
    families = [
        {
            "family_id": fam,
            "floor_contractual": 149 if fam == "clean-safety" else 30,
            "n_power": 160 if fam == "clean-safety" else 40,
            "n_required": 160 if fam == "clean-safety" else 40,
            "unit": "group_id",
        }
        for fam in BENCHMARK_FAMILIES
    ]
    # fix n_required = max(floor, n_power)
    for row in families:
        row["n_required"] = max(row["floor_contractual"], row["n_power"])
    doc: dict = {
        "schema": SCHEMA_IDS["benchmark_power_plan"],
        "plan_id": "power-1",
        "implementation_version": "power-tool-1",
        "seed_base": 20260719,
        "pilot_sha256": _HEX_A,
        "power_source_role": "development_pilot",
        "m": m,
        "alpha_plan": 0.05 / m,
        "families": families,
        "gates": [
            {
                "metric_id": "curve_err_rel",
                "statistic": "paired_bootstrap",
                "orientation": "greater",
                "min_effect_size": 0.10,
                "n_power": 40,
                "n_required": 40,
                "support": 32,
            },
            {
                "metric_id": "clean_actionable_rate",
                "statistic": "exact_binomial",
                "orientation": "less",
                "min_effect_size": 0.01,
                "n_power": 160,
                "n_required": 160,
                "support": 149,
            },
        ],
        "support": {"pilot_groups": 40, "notes": "fixture"},
        "result": {"status": "frozen", "n_selected": True},
    }
    doc.update(overrides)
    return doc
