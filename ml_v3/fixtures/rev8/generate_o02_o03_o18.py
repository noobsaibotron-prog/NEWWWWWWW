"""Generate the signed-decision artifacts for REV8 O-02/O-03/O-18.

Run from the repository root with the locked G1 environment:

    python -m ml_v3.fixtures.rev8.generate_o02_o03_o18

The generated JSON files are canonical UTF-8 with one trailing LF.  Re-running
the command must be byte-identical.
"""
from __future__ import annotations

import struct
from pathlib import Path

from ml_v3.contracts.canonical import (
    sha256_of_file,
    sha256_of_obj,
    write_canonical,
)
from ml_v3.contracts.frequency_geometry_v2 import (
    BOUNDARY_ARTIFACT_PATH,
    WIDTH_ARTIFACT_PATH,
    build_boundary_artifact,
    build_width_artifact,
)
from ml_v3.contracts.numeric_artifact_v2 import (
    NUMERIC_ARTIFACT_PATH,
    build_numeric_artifact,
)
from ml_v3.contracts.grid import band_centers_hz

REPORT_PATH = Path(__file__).resolve().parent / "o02_o03_o18_generator_report_v1.json"


def _seal(payload: dict[str, object]) -> dict[str, object]:
    result = dict(payload)
    result["artifact_hash"] = sha256_of_obj(result)
    return result


def generate() -> dict[str, object]:
    boundary, boundary_report = build_boundary_artifact()
    write_canonical(BOUNDARY_ARTIFACT_PATH, boundary)
    boundary_file_sha = sha256_of_file(BOUNDARY_ARTIFACT_PATH)

    width, width_report = build_width_artifact(boundary_file_sha)
    write_canonical(WIDTH_ARTIFACT_PATH, width)

    numeric, numeric_report = build_numeric_artifact()
    write_canonical(NUMERIC_ARTIFACT_PATH, numeric)

    legacy_bits = [
        "0x" + struct.pack(">d", value).hex()
        for value in band_centers_hz()
    ]
    center_steps = [
        abs(int(legacy[2:], 16) - int(current[2:], 16))
        for legacy, current in zip(
            legacy_bits, boundary["center_binary64_bits"])
    ]

    report = _seal({
        "schema": "aieq-v3-o02-o03-o18-generator-report-1",
        "generator": "ml_v3.fixtures.rev8.generate_o02_o03_o18",
        "artifacts": {
            BOUNDARY_ARTIFACT_PATH.name: {
                "file_sha256": sha256_of_file(BOUNDARY_ARTIFACT_PATH),
                "embedded_artifact_hash": boundary["artifact_hash"],
            },
            WIDTH_ARTIFACT_PATH.name: {
                "file_sha256": sha256_of_file(WIDTH_ARTIFACT_PATH),
                "embedded_artifact_hash": width["artifact_hash"],
            },
            NUMERIC_ARTIFACT_PATH.name: {
                "file_sha256": sha256_of_file(NUMERIC_ARTIFACT_PATH),
                "embedded_artifact_hash": numeric["artifact_hash"],
            },
        },
        "certificates": {
            "o02": boundary_report,
            "o03": width_report,
            "o18": numeric_report,
        },
        "rev7_legacy_grid_diagnostic": {
            "mismatched_center_count": sum(
                step != 0 for step in center_steps),
            "maximum_positive_bit_order_distance": max(center_steps),
            "normative_effect": (
                "none: REV7 stays live; O-02 bits are REV8 authority"
            ),
        },
    })
    write_canonical(REPORT_PATH, report)
    return report


if __name__ == "__main__":
    generate()
