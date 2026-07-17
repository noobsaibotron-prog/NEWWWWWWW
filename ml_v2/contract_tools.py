"""Torch-free tooling for immutable A4b corpus-admission contracts.

This module never edits a contract itself. It validates a proposed delta or
prints a deterministic assignment plan that must be reviewed before inclusion.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from .dataset_v2 import (BATCH_ALLOCATION, DATA_ROOT_DEFAULT, MANIFEST,
                         assign_batch_roles, batch_group_ids_sha256,
                         load_split_contract_document,
                         validate_admission_preflight,
                         validate_split_contract_delta)


def plan_admission_batch(contract_path: Path, batch_spec_path: Path) -> dict:
    """Build, but do not write, a reviewable assignment for a new source batch."""
    contract = load_split_contract_document(contract_path)
    spec = json.loads(batch_spec_path.read_text())
    for field in ("batch_id", "source_id", "primary_domain", "group_ids"):
        if field not in spec:
            raise ValueError(f"batch spec missing {field!r}")
    batch_id = spec["batch_id"]
    source_id = spec["source_id"]
    primary_domain = spec["primary_domain"]
    groups = spec["group_ids"]
    if batch_id in contract["batches"]:
        raise ValueError(f"batch {batch_id!r} already exists")
    occupied = set(contract["roles"]) | set(contract["legacy_groups"])
    reused = sorted(set(groups) & occupied)
    if reused:
        raise ValueError(f"batch contains already-admitted groups: {reused[:3]}")

    assignments = assign_batch_roles(contract["salt"], batch_id, groups)
    ordered_groups = sorted(groups)
    metadata = {
        group: {"primary_domain": primary_domain, "admission_batch": batch_id,
                "source_id": source_id}
        for group in ordered_groups
    }
    batch = {
        "source_id": source_id,
        "primary_domain": primary_domain,
        "allocation": BATCH_ALLOCATION,
        "group_ids": ordered_groups,
        "group_ids_sha256": batch_group_ids_sha256(ordered_groups),
    }
    return {"roles": assignments, "group_metadata": metadata,
            "batch_id": batch_id, "batch": batch}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--validate-delta", action="store_true",
                      help="verify that candidate only appends immutable assignments")
    mode.add_argument("--validate-admission", action="store_true",
                      help="run delta + manifest + deep audio-hash preflight")
    mode.add_argument("--plan-batch", type=Path,
                      help="print a deterministic, review-only batch assignment")
    parser.add_argument("--previous", type=Path,
                        help="committed contract before an admission edit")
    parser.add_argument("--candidate", type=Path,
                        help="candidate contract to validate")
    parser.add_argument("--contract", type=Path,
                        help="current contract used with --plan-batch")
    parser.add_argument("--manifest", type=Path, default=MANIFEST,
                        help="candidate manifest used with --validate-admission")
    parser.add_argument("--data-root", type=Path, default=DATA_ROOT_DEFAULT,
                        help="audio root used with --validate-admission")
    args = parser.parse_args()

    if args.validate_delta:
        if args.previous is None or args.candidate is None:
            parser.error("--validate-delta requires --previous and --candidate")
        validate_split_contract_delta(args.previous, args.candidate)
        print("A4b contract delta: OK")
        return 0

    if args.validate_admission:
        if args.previous is None or args.candidate is None:
            parser.error("--validate-admission requires --previous and --candidate")
        report = validate_admission_preflight(
            args.previous, args.candidate, manifest=args.manifest,
            data_root=args.data_root)
        print("A4b admission preflight: OK")
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0

    if args.contract is None:
        parser.error("--plan-batch requires --contract")
    print(json.dumps(plan_admission_batch(args.contract, args.plan_batch),
                     ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
