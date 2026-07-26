"""G1a T4 — frozen metrology lock (§13.1/§13.2) happy + reject paths."""
from __future__ import annotations

import hashlib
import json
import math
import unittest
from pathlib import Path

from ml_v3.contracts.adapter import adapter_mapping_sha256
from ml_v3.contracts.canonical import loads_strict, sha256_of_obj
from ml_v3.contracts.constants import CONTRACT_REVISION, GRID_BANDS
from ml_v3.contracts.grid import band_centers_hz
from ml_v3.contracts.metrology_lock import (
    FIXED_CHUNK_SCHEDULES,
    GATE_PLATFORM_FLOAT_TOL,
    GEOMETRIC_CHUNK_SCHEDULE,
    HOP_SAMPLES,
    KAISER_BETA,
    K_CODA,
    K_WU,
    METROLOGY_ARTIFACT_ID,
    N_LF,
    RESAMPLER_PASS_HZ,
    SECONDARY_FLOAT_ABS_TOL,
    SR_PARITY_MAX_ABS_DB,
    STREAMING_ALSO_REQUIRED_PROOFS,
    STREAMING_FLOAT32_FRAME_FIELDS,
    STREAMING_PRNG_SEED,
    STREAMING_RATIONAL_TIMESTAMP_FIELDS,
    STREAMING_VALIDITY_FIELDS,
    SWEEP_CHECKPOINT_HZ,
    MetrologyLockError,
    coda_seconds,
    frozen_metrology_lock,
    metrology_lock_bytes,
    metrology_lock_sha256,
    resampler_group_delay_rational,
    validate_metrology_lock_claim,
    warm_up_seconds,
)

FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "fixtures" / "g1" / "metrology_lock.json"
)

EXPECTED_ADAPTER_SHA256 = (
    "606fae2908b3a41d34581b84f1e6060272839e85005039bfb2cde60e934ac616"
)


class FrozenLockHappyPathTests(unittest.TestCase):
    def test_timing_constants_and_additive_warmup(self):
        lock = frozen_metrology_lock()
        timing = lock["timing"]
        self.assertEqual(timing["H"], HOP_SAMPLES)
        self.assertEqual(HOP_SAMPLES, 1024)
        self.assertEqual(timing["N_LF"], N_LF)
        self.assertEqual(N_LF, 8192)
        self.assertEqual(timing["K_wu"], K_WU)
        self.assertEqual(timing["K_coda"], K_CODA)
        self.assertEqual(K_WU, 4)
        self.assertEqual(K_CODA, 4)
        self.assertEqual(timing["warm_up_composition"], "additive")
        self.assertEqual(timing["warm_up_composition_forbidden"], "max")
        # Additive: delay + 32/125, not max(delay, N_LF/fs_c, ...).
        self.assertAlmostEqual(warm_up_seconds(48000), 0.256, places=12)
        self.assertGreater(warm_up_seconds(44100), warm_up_seconds(48000))
        self.assertGreater(warm_up_seconds(96000), warm_up_seconds(48000))
        self.assertAlmostEqual(coda_seconds(), 32 / 375, places=12)

    def test_resampler_delay_rationals_per_gate_sr(self):
        n, d, taps = resampler_group_delay_rational(48000)
        self.assertEqual((n, d, taps), (0, 1, None))
        n, d, taps = resampler_group_delay_rational(96000)
        self.assertEqual((n, d, taps), (1, 750, 257))
        n, d, taps = resampler_group_delay_rational(44100)
        self.assertEqual((n, d, taps), (16, 11025, 20481))

    def test_stationary_mode_a_closes_gate_mode_b_diagnostic(self):
        st = frozen_metrology_lock()["stationary_portion"]
        self.assertEqual(st["gate_closing_mode"], "a")
        self.assertTrue(st["mode_a"]["closes_sample_rate_parity_gate"])
        self.assertFalse(st["mode_b"]["closes_sample_rate_parity_gate"])
        self.assertTrue(st["mode_b"]["diagnostic_only"])
        self.assertEqual(st["mode_b"]["preregistered_windows"], [])
        self.assertEqual(st["mode_b"]["T_min_hops"], 8)
        self.assertEqual(st["mode_b"]["stability_variance_max_db2"], 1.0)
        self.assertEqual(st["mode_b"]["stability_max_abs_hop_delta_db"], 0.5)

    def test_activity_union_and_sr_threshold(self):
        sr = frozen_metrology_lock()["sample_rate_parity"]
        self.assertEqual(sr["threshold_max_abs_db"], SR_PARITY_MAX_ABS_DB)
        self.assertEqual(SR_PARITY_MAX_ABS_DB, 0.25)
        self.assertEqual(sr["aggregator"], "max")
        self.assertTrue(sr["activity"]["union_cross_sr"])
        self.assertEqual(
            sr["activity"]["predicate"],
            "max(psd_db_ref, psd_db_sr) > -120",
        )
        self.assertTrue(sr["activity"]["empty_active_cell_set_is_fail"])
        self.assertTrue(sr["activity"]["empty_useful_segment_is_fail"])
        self.assertTrue(sr["activity"]["vacuous_pass_forbidden"])
        self.assertEqual(sr["activity"]["max_over_empty_active_set"], "FAIL")
        self.assertTrue(sr["activity"]["na_is_not_pass"])
        self.assertIn("mid_delta_db", sr["excluded_from_domain"])
        self.assertIn("side_delta_db", sr["excluded_from_domain"])

    def test_sr_alignment_nearest_source_time_anti_cherrypick(self):
        sr = frozen_metrology_lock()["sample_rate_parity"]
        alignment = sr["alignment"]
        self.assertEqual(alignment["select_by"], "nearest_source_time")
        self.assertIn("output_index", alignment["forbidden_select_by"])
        self.assertTrue(alignment["manual_frame_shift_forbidden"])
        self.assertTrue(
            alignment[
                "choose_within_pm1_radius_to_minimize_abs_delta_forbidden"])
        self.assertEqual(
            alignment["tie_break"],
            ["smaller_frame_index", "smaller_frame_end_sample"],
        )
        sweep = frozen_metrology_lock()["sweep_log_parity"]
        self.assertEqual(
            sweep["frame_selection"],
            "nearest_source_time_within_match_radius",
        )
        self.assertEqual(
            sweep["alignment_policy_ref"], "sample_rate_parity.alignment")

    def test_empty_to_fail_derivation_does_not_supersede_contract(self):
        derivation = frozen_metrology_lock()["sample_rate_parity"]["activity"][
            "empty_to_fail_derivation"]
        self.assertEqual(derivation["kind"], "derived_packaging")
        self.assertTrue(derivation["does_not_supersede_contract"])
        self.assertFalse(derivation["candidate_for_future_contract_amendment"])
        self.assertGreaterEqual(len(derivation["chain"]), 3)
        joined = " ".join(derivation["chain"])
        self.assertIn("§10.5", joined)
        self.assertIn("N/A", joined)
        self.assertIn("§13.2 gate 4", joined)
        self.assertIn("lock packaging authority", derivation["packaging_note"])
        self.assertIn("REV7", derivation["packaging_note"])

    def test_bit_identity_gate_platform_empty_secondary_allowlist(self):
        bit_id = frozen_metrology_lock()["bit_identity"]
        platform = bit_id["gate_platform"]
        self.assertEqual(platform["os"], "darwin")
        self.assertEqual(platform["arch"], "arm64")
        self.assertEqual(platform["numpy"], "2.5.1")
        self.assertEqual(bit_id["gate_platform_float_tol"], 0)
        self.assertEqual(bit_id["gate_platform_float_tol"], GATE_PLATFORM_FLOAT_TOL)
        self.assertEqual(
            bit_id["gate_platform_float_identity"], "byte_identical_only")
        self.assertTrue(bit_id["gate_platform_secondary_tol_forbidden"])
        self.assertEqual(bit_id["secondary_platforms_allowlist"], [])
        self.assertEqual(bit_id["secondary_float_abs_tol"], 1e-6)
        self.assertEqual(
            bit_id["secondary_float_abs_tol"], SECONDARY_FLOAT_ABS_TOL)
        self.assertTrue(bit_id["secondary_cannot_close_g1_gate"])
        self.assertEqual(
            bit_id["ad_hoc_non_bit_identical_without_allowlist"], "FAIL")
        self.assertNotEqual(
            bit_id["gate_platform_float_tol"],
            bit_id["secondary_float_abs_tol"],
        )

    def test_streaming_schedules_and_seed(self):
        stream = frozen_metrology_lock()["streaming_equivalence"]
        self.assertEqual(stream["prng_seed"], STREAMING_PRNG_SEED)
        self.assertEqual(STREAMING_PRNG_SEED, 20260719)
        self.assertEqual(
            stream["fixed_chunk_schedules_host_samples"],
            list(FIXED_CHUNK_SCHEDULES),
        )
        geo = stream["geometric_schedule"]
        self.assertEqual(geo["chunks_host_samples"], list(GEOMETRIC_CHUNK_SCHEDULE))
        self.assertEqual(len(geo["chunks_host_samples"]), 32)
        surface = stream["required_proof_surface"]
        self.assertEqual(
            surface["float32_frame_fields"],
            list(STREAMING_FLOAT32_FRAME_FIELDS),
        )
        self.assertEqual(
            surface["rational_timestamp_fields"],
            list(STREAMING_RATIONAL_TIMESTAMP_FIELDS),
        )
        self.assertEqual(
            surface["validity_fields"], list(STREAMING_VALIDITY_FIELDS))
        self.assertTrue(surface["validity_reason_enumerated_when_false"])
        self.assertEqual(
            surface["also_required"],
            [dict(p) for p in STREAMING_ALSO_REQUIRED_PROOFS],
        )
        self.assertIn("every frozen schedule", surface["also_required_quantifier"])
        self.assertTrue(surface["also_required_omission_is_fail"])
        self.assertIn("(a)(b)(c)", surface["also_required_applies_to"])
        self.assertNotIn("also_required", stream)

    def test_resampler_generator_serialize_only_section_5(self):
        gen = frozen_metrology_lock()["resampler_generator"]
        self.assertTrue(gen["serialize_only"])
        self.assertTrue(gen["no_coefficient_implementation_in_g1a"])
        self.assertEqual(gen["kaiser_beta"], KAISER_BETA)
        self.assertEqual(KAISER_BETA, 9.0)
        self.assertEqual(gen["pass_hz"], RESAMPLER_PASS_HZ)
        self.assertEqual(gen["stop_hz_formula"], "min(fs_in, 48000) / 2")
        self.assertEqual(gen["cutoff"], "midpoint_of_pass_and_stop")
        self.assertEqual(gen["coefficient_gain_scale"], "up")
        self.assertEqual(gen["structure"], "causal_polyphase")
        self.assertTrue(gen["streaming_state_preserved_across_blocks"])
        self.assertEqual(gen["padding"], "none")
        self.assertIs(gen["reflection"], False)
        self.assertTrue(gen["look_ahead_forbidden"])

    def test_t5_hash_coverage_declaration(self):
        coverage = frozen_metrology_lock()["hash_coverage"]
        self.assertTrue(
            coverage[
                "t5_sha256sums_covers_artifact_hashes_beyond_dependencies"])
        self.assertEqual(
            coverage["beyond_dependencies_covered_by"], "T5_SHA256SUMS")
        self.assertTrue(coverage["inline_dependencies_bound_here"])
        self.assertIn("not left implicit", coverage["declaration"])
        self.assertIn("T4.2", coverage["t4_2_stop_rule"])
        self.assertIn("last hardening", coverage["t4_2_stop_rule"])
        self.assertIn("CRITICAL", coverage["t4_2_stop_rule"])

    def test_geometric_schedule_matches_numpy_pcg64_when_available(self):
        try:
            import numpy as np
        except ImportError:
            self.skipTest("numpy not available")
        rng = np.random.Generator(np.random.PCG64(STREAMING_PRNG_SEED))
        u = rng.uniform(0.0, 14.0, size=32)
        chunks = [math.floor(float(2.0 ** value)) for value in u]
        self.assertEqual(chunks, list(GEOMETRIC_CHUNK_SCHEDULE))

    def test_sweep_checkpoints_include_critical_and_extremes(self):
        checkpoints = frozen_metrology_lock()["sweep_log_parity"]["checkpoint_hz"]
        self.assertEqual(checkpoints, list(SWEEP_CHECKPOINT_HZ))
        for hz in (20, 45, 60, 80, 250, 1000, 3500, 8000, 16000, 20000):
            self.assertIn(hz, checkpoints)
        self.assertEqual(checkpoints, sorted(checkpoints))

    def test_dependencies_bind_contract_and_t3_adapter(self):
        deps = frozen_metrology_lock()["dependencies"]
        self.assertEqual(deps["contract_revision"], CONTRACT_REVISION)
        self.assertIn("6fbf5b59", deps["contract_revision"])
        self.assertIn("REVISIONE 7 CONSOLIDATA", deps["contract_revision"])
        self.assertEqual(deps["adapter_mapping_sha256"], adapter_mapping_sha256())
        self.assertEqual(deps["adapter_mapping_sha256"], EXPECTED_ADAPTER_SHA256)
        self.assertEqual(deps["grid_bands"], GRID_BANDS)
        self.assertEqual(
            deps["grid_centers_sha256"], sha256_of_obj(band_centers_hz()))

    def test_canonical_hash_stable_across_calls(self):
        a = metrology_lock_sha256()
        b = metrology_lock_sha256()
        self.assertEqual(a, b)
        self.assertEqual(len(a), 64)
        self.assertEqual(a, hashlib.sha256(metrology_lock_bytes()).hexdigest())
        self.assertEqual(a, sha256_of_obj(frozen_metrology_lock()))

    def test_validate_accepts_frozen_copy(self):
        claim = frozen_metrology_lock()
        out = validate_metrology_lock_claim(claim)
        self.assertEqual(out["artifact_id"], METROLOGY_ARTIFACT_ID)
        self.assertEqual(out["contract_revision"], CONTRACT_REVISION)

    def test_golden_fixture_matches_frozen_bytes(self):
        self.assertTrue(FIXTURE.is_file(), f"missing golden {FIXTURE}")
        on_disk = FIXTURE.read_bytes()
        self.assertEqual(on_disk, metrology_lock_bytes())
        parsed = loads_strict(on_disk.decode("utf-8"))
        validate_metrology_lock_claim(parsed)
        self.assertEqual(
            parsed["dependencies"]["adapter_mapping_sha256"],
            EXPECTED_ADAPTER_SHA256,
        )

    def test_envelope_authority_declares_freeze_from_prose(self):
        prose = frozen_metrology_lock()["envelope_authority"]
        self.assertIn("freeze-from-prose", prose)
        self.assertIn("no literal JSON key table", prose)


class RejectPathTests(unittest.TestCase):
    def test_non_object_claim_rejected(self):
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(["not", "an", "object"])

    def test_max_warmup_composition_rejected(self):
        claim = frozen_metrology_lock()
        claim["timing"]["warm_up_composition"] = "max"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_wrong_k_wu_rejected(self):
        claim = frozen_metrology_lock()
        claim["timing"]["K_wu"] = 2
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_activity_without_union_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"]["union_cross_sr"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_empty_active_cell_set_fail_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["sample_rate_parity"]["activity"]["empty_active_cell_set_is_fail"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("empty_active_cell_set_is_fail", str(ctx.exception))

    def test_empty_active_cell_set_fail_false_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"][
            "empty_active_cell_set_is_fail"] = False
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("empty_active_cell_set_is_fail", str(ctx.exception))

    def test_empty_useful_segment_fail_false_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"][
            "empty_useful_segment_is_fail"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_vacuous_pass_allowed_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"]["vacuous_pass_forbidden"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_max_over_empty_active_set_zero_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"][
            "max_over_empty_active_set"] = 0
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("max_over_empty_active_set", str(ctx.exception))

    def test_gate_platform_float_tol_nonzero_rejected(self):
        claim = frozen_metrology_lock()
        claim["bit_identity"]["gate_platform_float_tol"] = 1e-6
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("gate_platform_float_tol", str(ctx.exception))

    def test_gate_platform_secondary_tol_allowed_rejected(self):
        claim = frozen_metrology_lock()
        claim["bit_identity"]["gate_platform_secondary_tol_forbidden"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_streaming_proof_surface_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["streaming_equivalence"]["required_proof_surface"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("required_proof_surface", str(ctx.exception))

    def test_streaming_float32_fields_incomplete_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["required_proof_surface"][
            "float32_frame_fields"] = ["mid_psd_db[120]"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("float32_frame_fields", str(ctx.exception))

    def test_streaming_also_required_string_list_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["required_proof_surface"][
            "also_required"] = [
            "multi_asset_concat_with_explicit_delta_history_reset",
            "interleaved_silence_between_assets",
            "streaming_vs_offline_identity_same_lock_and_platform",
        ]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("also_required", str(ctx.exception))

    def test_streaming_also_required_omission_fail_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["streaming_equivalence"]["required_proof_surface"][
            "also_required_omission_is_fail"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("also_required_omission_is_fail", str(ctx.exception))

    def test_streaming_also_required_quantifier_weakened_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["required_proof_surface"][
            "also_required_quantifier"] = "for some schedule only"
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("also_required_quantifier", str(ctx.exception))

    def test_sr_alignment_output_index_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["alignment"]["select_by"] = "output_index"
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("nearest_source_time", str(ctx.exception))

    def test_sr_alignment_pm1_cherrypick_allowed_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["alignment"][
            "choose_within_pm1_radius_to_minimize_abs_delta_forbidden"] = False
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("pm1_radius", str(ctx.exception))

    def test_sr_alignment_tie_break_stripped_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["alignment"]["tie_break"] = [
            "smaller_abs_delta"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("tie_break", str(ctx.exception))

    def test_empty_to_fail_derivation_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["sample_rate_parity"]["activity"]["empty_to_fail_derivation"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("empty_to_fail_derivation", str(ctx.exception))

    def test_empty_to_fail_claims_supersede_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["activity"]["empty_to_fail_derivation"][
            "does_not_supersede_contract"] = False
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("does_not_supersede_contract", str(ctx.exception))

    def test_resampler_generator_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["resampler_generator"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("resampler_generator", str(ctx.exception))

    def test_resampler_generator_kaiser_beta_wrong_rejected(self):
        claim = frozen_metrology_lock()
        claim["resampler_generator"]["kaiser_beta"] = 8.0
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("kaiser_beta", str(ctx.exception))

    def test_t5_hash_coverage_stripped_rejected(self):
        claim = frozen_metrology_lock()
        del claim["hash_coverage"]
        with self.assertRaises(MetrologyLockError) as ctx:
            validate_metrology_lock_claim(claim)
        self.assertIn("hash_coverage", str(ctx.exception))

    def test_t5_hash_coverage_false_rejected(self):
        claim = frozen_metrology_lock()
        claim["hash_coverage"][
            "t5_sha256sums_covers_artifact_hashes_beyond_dependencies"] = False
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_mode_b_closing_gate_rejected(self):
        claim = frozen_metrology_lock()
        claim["stationary_portion"]["mode_b"]["closes_sample_rate_parity_gate"] = True
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_gate_closing_mode_b_rejected(self):
        claim = frozen_metrology_lock()
        claim["stationary_portion"]["gate_closing_mode"] = "b"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_relaxed_sr_threshold_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["threshold_max_abs_db"] = 0.5
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_mean_aggregator_rejected(self):
        claim = frozen_metrology_lock()
        claim["sample_rate_parity"]["aggregator"] = "mean"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_wrong_adapter_dependency_rejected(self):
        claim = frozen_metrology_lock()
        claim["dependencies"]["adapter_mapping_sha256"] = "ff" * 32
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_wrong_contract_revision_dependency_rejected(self):
        claim = frozen_metrology_lock()
        claim["dependencies"]["contract_revision"] = "tampered"
        claim["contract_revision"] = "tampered"
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_secondary_allowlist_ad_hoc_rejected(self):
        claim = frozen_metrology_lock()
        claim["bit_identity"]["secondary_platforms_allowlist"] = [
            {"os": "linux", "arch": "x86_64"},
        ]
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_mutated_geometric_schedule_rejected(self):
        claim = frozen_metrology_lock()
        claim["streaming_equivalence"]["geometric_schedule"][
            "chunks_host_samples"][0] = 999
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_extra_key_breaks_byte_identity(self):
        claim = frozen_metrology_lock()
        claim["invented_slack"] = 0.99
        with self.assertRaises(MetrologyLockError):
            validate_metrology_lock_claim(claim)

    def test_non_canonical_json_roundtrip_still_hashes_via_canonical(self):
        lock = frozen_metrology_lock()
        messy = json.dumps(lock, sort_keys=False, separators=(", ", ": "))
        reparsed = loads_strict(messy if messy.endswith("\n") else messy + "\n")
        validate_metrology_lock_claim(reparsed)
        self.assertEqual(sha256_of_obj(reparsed), metrology_lock_sha256())


if __name__ == "__main__":
    unittest.main()
