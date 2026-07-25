#!/usr/bin/env python3
"""Run G1b spike WS4 evidence (gate-4 + streaming also-required notes).

Gate platform only. Writes:
  ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.json
  ml_v3/reports/G1B_SPIKE_WS4_EVIDENCE.md

≠ G1 PASS. ≠ CONTRACT amend.
"""
from __future__ import annotations

import json
import time
from pathlib import Path

from ml_v3.benchmark.sr_parity import run_spike_gate4_matrix
from ml_v3.contracts.metrology_lock import (
    FIXED_CHUNK_SCHEDULES,
    GEOMETRIC_CHUNK_SCHEDULE,
)

REPORTS = Path(__file__).resolve().parents[1] / "reports"
JSON_PATH = REPORTS / "G1B_SPIKE_WS4_EVIDENCE.json"
MD_PATH = REPORTS / "G1B_SPIKE_WS4_EVIDENCE.md"

SPIKE_SCHEDULES_NOTE = {
    "spike_schedules": [1, 8193, "geometric-32"],
    "geometric_chunks_host_samples": list(GEOMETRIC_CHUNK_SCHEDULE),
    "full_fixed_chunk_schedules_deferred": list(FIXED_CHUNK_SCHEDULES),
    "deferral": (
        "Full product matrix {1,63,1024,4095,8192,8193}+geometric deferred "
        "to official G1b. Spike covers {1,8193,geometric-32}. "
        "schedule=1 on full-length 44.1 kHz T6 assets remains harness-heavy "
        "(long FIR); covered on short emit slices + 48k/96k full paths in tests."
    ),
}


def _md(matrix: dict, extra: dict) -> str:
    lines: list[str] = []
    lines.append("# G1b Spike WS4 Evidence — Gate 4 SR-parity + proof (b)")
    lines.append("")
    lines.append("**Status:** SPIKE EVIDENCE — ≠ G1 PASS — ≠ G1b tip — ≠ CONTRACT amend")
    lines.append(f"**Overall gate-4 verdict:** `{matrix['overall_verdict']}`")
    lines.append(f"**Overall max|Δ|:** {matrix['overall_max_abs_db']:.6f} dB "
                 f"(threshold {matrix['threshold_db']} dB; "
                 f"GREEN margin floor {matrix['margin_floor_db']} dB)")
    lines.append("")
    lines.append("## Platform")
    lines.append("")
    for k, v in matrix["platform"].items():
        lines.append(f"- `{k}`: `{v}`")
    lines.append("")
    lines.append("## Useful window (warm-up ∩ coda)")
    lines.append("")
    lines.append(f"- `[{matrix['useful_window'][0]}, {matrix['useful_window'][1]}]` s")
    lines.append("")
    lines.append("## Per-cell results")
    lines.append("")
    lines.append("| asset | sr | mode | max\\|Δ\\| dB | margin | verdict | peak field/band | P2 watch |")
    lines.append("|-------|----|------|------------|--------|---------|-----------------|----------|")
    for c in matrix["cells"]:
        peak = c["peak"]
        if peak is None:
            peak_s = "—"
        else:
            band = peak["band"]
            hz = peak["hz"]
            peak_s = f"{peak['field']}[{band}]"
            if hz is not None:
                peak_s += f"@{hz:.1f}Hz"
        p2 = "HIT" if c["p2_watch_hit"] else "—"
        lines.append(
            f"| {c['asset']} | {c['fs_sr']} | {c['mode']} | "
            f"{c['max_abs_db']:.6f} | {c['margin_db']:.6f} | "
            f"**{c['verdict']}** | {peak_s} | {p2} |"
        )
    lines.append("")
    lines.append("## Peak localization detail")
    lines.append("")
    for c in matrix["cells"]:
        peak = c["peak"]
        if peak is None:
            continue
        lines.append(
            f"- **{c['asset']}@{c['fs_sr']}**: `{peak['field']}` band={peak['band']} "
            f"hz={peak['hz']} |Δ|={peak['abs_delta_db']:.6f} "
            f"psd_ref={peak['psd_ref']} psd_sr={peak['psd_sr']} "
            f"t_ref={peak['t_ref']:.6f} t_sr={peak['t_sr']:.6f} "
            f"meta={peak['meta']} "
            f"p2_main_sb={peak['p2_main_single_bin']} "
            f"p2_lf_sb={peak['p2_lf_single_bin']}"
        )
        for n in c.get("notes") or []:
            lines.append(f"  - note: {n}")
    lines.append("")
    lines.append("## P2 support geometry (preregistered watch)")
    lines.append("")
    p2 = matrix["p2_support"]
    lines.append(f"- MAIN empty={p2['main_empty_count']} single-bin={p2['main_single_bin_count']}")
    lines.append(f"- LF empty={p2['lf_empty_count']} single-bin={p2['lf_single_bin_count']}")
    lines.append(f"- MAIN single-bin indices: `{p2['main_single_bin_indices']}`")
    lines.append(f"- LF single-bin indices: `{p2['lf_single_bin_indices']}`")
    lines.append("")
    lines.append("## Pin necessity check")
    lines.append("")
    lines.append(f"- changed_to_pass: `{matrix['pin_necessity']['changed_to_pass']}`")
    lines.append(f"- {matrix['pin_necessity']['note']}")
    lines.append("")
    lines.append("## Streaming schedules (spike)")
    lines.append("")
    lines.append(f"- Spike schedules: `{extra['streaming']['spike_schedules']}`")
    lines.append(f"- Deferred full fixed set: `{extra['streaming']['full_fixed_chunk_schedules_deferred']}`")
    lines.append(f"- {extra['streaming']['deferral']}")
    lines.append("")
    lines.append("## Proof (b) interleaved silence")
    lines.append("")
    pb = extra["proof_b"]
    lines.append(f"- Status: **{pb['status']}**")
    lines.append(f"- {pb['detail']}")
    lines.append("")
    lines.append("## Diagnostic (non-gating)")
    lines.append("")
    lines.append(
        "Tone-peak / sweep-peak bands are far tighter than the full §13.2 domain "
        "max|Δ|. Failures concentrate on near-floor active cells "
        "(activity predicate `max(psd_ref,psd_sr) > -120`), P2 single-bin MAIN "
        "bands, and sweep skirt bands away from the instantaneous peak. "
        "Prominence left **unclamped**. No threshold relaxation applied."
    )
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    for n in matrix["notes"]:
        lines.append(f"- {n}")
    lines.append("")
    lines.append("## Handoff")
    lines.append("")
    lines.append("- `ember-contract-guardian`: counter-check tip; REV7 only if RED write-up accepted")
    lines.append("- `ember-parity-lab`: re-verify digests / max|Δ| / margin tables on gate venv")
    lines.append("- `ember-metrology-redteam`: attack false-PASS (window, activity, invalid ignore, platform)")
    lines.append("")
    lines.append("≠ G1 PASS. ≠ Ableton readiness. CONTRACT @ freeze unchanged.")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    t0 = time.time()
    matrix = run_spike_gate4_matrix()
    dt = time.time() - t0
    # Proof (b) status is owned by unit tests; record expectation here.
    extra = {
        "streaming": SPIKE_SCHEDULES_NOTE,
        "proof_b": {
            "status": "COVERED_BY_TESTS",
            "detail": (
                "ml_v3/tests/test_g1b_t2_streaming_features.py::"
                "InterleavedSilenceProofBTests — asset||silence||asset "
                "offline≡chunk on spike schedules {1,8193,geometric-32}; "
                "silence naturally clears delta history (also_required b). "
                "Proof (a) explicit reset remains in MultiAssetDeltaResetTests."
            ),
        },
        "elapsed_s": dt,
        "claim": "SPIKE_EVIDENCE_ONLY",
    }
    payload = {
        "artifact": "G1B_SPIKE_WS4_EVIDENCE",
        "phase": "G1b-SPIKE-WS4",
        "gate4": matrix.to_dict(),
        **extra,
    }
    REPORTS.mkdir(parents=True, exist_ok=True)
    JSON_PATH.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    MD_PATH.write_text(_md(payload["gate4"], extra))
    print(f"Wrote {JSON_PATH}")
    print(f"Wrote {MD_PATH}")
    print(f"VERDICT={matrix.overall_verdict} max|Δ|={matrix.overall_max_abs_db:.6f} elapsed={dt:.1f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
