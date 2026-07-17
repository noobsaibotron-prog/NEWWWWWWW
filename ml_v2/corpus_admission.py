"""Fail-closed canonicalisation for prospective A4b corpus admissions.

This tool intentionally does *not* edit MANIFEST_V2.csv or a4b_split_v2.json.
It converts a reviewed external source ledger into deterministic PCM WAV files
and an immutable materialisation report. A later, separate admission commit
must review those records, assign contract roles, and pass the deep preflight.

The source policy is deliberately narrow:
  - FMA full mixes are evaluation-only; they must never become raw-clean
    training negatives by accident.
  - Slakh can be a training candidate or an evaluation fixture.
  - OWNED material needs a complete commercial provenance ledger.

Run:
  python3 -m ml_v2.corpus_admission \
    --ledger /absolute/path/batch.csv \
    --input-root /absolute/path/quarantine \
    --output-root /absolute/path/canonical_batch
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import wave
from dataclasses import dataclass
from pathlib import Path

from .dataset_v2 import ALLOWED_LICENSES, canonical_sha256


LEDGER_FIELDS = (
    "source_id", "source_path", "source_sha256", "group", "domain", "usage",
    "license", "upstream_license", "attribution", "crosscheck", "source_url",
)
ALLOWED_DOMAINS = {
    "clean_bass", "clean_drums", "clean_mix", "clean_synth", "hf_negative",
}
SOURCE_POLICIES = {
    "fma": {"suffixes": {".mp3"}, "usages": {"eval_only"}},
    "slakh": {"suffixes": {".flac", ".wav"},
              "usages": {"eval_only", "train_candidate"}},
    "owned": {"suffixes": {".flac", ".mp3", ".wav"},
              "usages": {"eval_only", "train_candidate"}},
}
_SAFE_GROUP = re.compile(r"[a-z0-9][a-z0-9._-]*:[a-z0-9][a-z0-9._-]*\Z")


@dataclass(frozen=True)
class LedgerRow:
    source_id: str
    source_path: Path
    source_sha256: str
    group: str
    domain: str
    usage: str
    license: str
    upstream_license: str
    attribution: str
    crosscheck: str
    source_url: str


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _required(value: str, label: str, row_number: int) -> str:
    value = value.strip()
    if not value or value.lower() == "n/a":
        raise ValueError(f"ledger row {row_number}: {label} is required")
    return value


def _safe_source_path(input_root: Path, value: str, row_number: int) -> Path:
    candidate = Path(_required(value, "source_path", row_number))
    if candidate.is_absolute() or ".." in candidate.parts:
        raise ValueError(f"ledger row {row_number}: source_path must be relative")
    root = input_root.resolve()
    resolved = (root / candidate).resolve()
    try:
        resolved.relative_to(root)
    except ValueError as exc:
        raise ValueError(
            f"ledger row {row_number}: source_path escapes input_root") from exc
    if not resolved.is_file():
        raise FileNotFoundError(f"ledger row {row_number}: source file missing: {resolved}")
    return resolved


def _validate_row(raw: dict[str, str], input_root: Path, row_number: int) -> LedgerRow:
    missing = [field for field in LEDGER_FIELDS if field not in raw]
    if missing:
        raise ValueError(f"ledger missing required columns: {', '.join(missing)}")

    source_id = _required(raw["source_id"], "source_id", row_number).lower()
    if source_id not in SOURCE_POLICIES:
        raise ValueError(f"ledger row {row_number}: unsupported source_id {source_id!r}")
    source_path = _safe_source_path(input_root, raw["source_path"], row_number)
    policy = SOURCE_POLICIES[source_id]
    if source_path.suffix.lower() not in policy["suffixes"]:
        raise ValueError(
            f"ledger row {row_number}: {source_id} does not admit "
            f"{source_path.suffix.lower()!r} input")

    usage = _required(raw["usage"], "usage", row_number)
    if usage not in policy["usages"]:
        raise ValueError(
            f"ledger row {row_number}: {source_id} is restricted to "
            f"{sorted(policy['usages'])}, not {usage!r}")
    domain = _required(raw["domain"], "domain", row_number)
    if domain not in ALLOWED_DOMAINS:
        raise ValueError(f"ledger row {row_number}: unsupported domain {domain!r}")

    license_name = _required(raw["license"], "license", row_number)
    if license_name not in ALLOWED_LICENSES:
        raise ValueError(
            f"ledger row {row_number}: license {license_name!r} is not approved")
    if source_id == "owned" and license_name != "OWNED":
        raise ValueError(f"ledger row {row_number}: owned source must use OWNED license")
    if source_id != "owned" and license_name == "OWNED":
        raise ValueError(f"ledger row {row_number}: OWNED is valid only for owned source")

    source_sha = canonical_sha256(
        raw["source_sha256"], f"ledger row {row_number} ({source_path.name})")
    actual_sha = sha256_file(source_path)
    if actual_sha != source_sha:
        raise ValueError(
            f"ledger row {row_number}: source_sha256 mismatch for {source_path.name}")

    group = _required(raw["group"], "group", row_number)
    if not _SAFE_GROUP.fullmatch(group) or not group.startswith(source_id + ":"):
        raise ValueError(
            f"ledger row {row_number}: group must be a lowercase "
            f"{source_id}:<stable-id> identifier")

    return LedgerRow(
        source_id=source_id,
        source_path=source_path,
        source_sha256=source_sha,
        group=group,
        domain=domain,
        usage=usage,
        license=license_name,
        upstream_license=_required(raw["upstream_license"], "upstream_license", row_number),
        attribution=_required(raw["attribution"], "attribution", row_number),
        crosscheck=_required(raw["crosscheck"], "crosscheck", row_number),
        source_url=_required(raw["source_url"], "source_url", row_number),
    )


def load_ledger(ledger_path: Path, input_root: Path) -> list[LedgerRow]:
    """Validate every row before decoding any source file."""
    with ledger_path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None:
            raise ValueError("ledger has no header")
        missing = [field for field in LEDGER_FIELDS if field not in reader.fieldnames]
        if missing:
            raise ValueError(f"ledger missing required columns: {', '.join(missing)}")
        rows = [_validate_row(raw, input_root, line)
                for line, raw in enumerate(reader, start=2)]
    if not rows:
        raise ValueError("ledger contains no rows")
    duplicate_hashes = {row.source_sha256 for row in rows}
    if len(duplicate_hashes) != len(rows):
        raise ValueError("ledger reuses a source_sha256; duplicate content is not admitted")
    return sorted(rows, key=lambda row: (row.source_id, row.source_sha256))


def _ffmpeg_banner(ffmpeg: str) -> str:
    completed = subprocess.run([ffmpeg, "-version"], check=True, text=True,
                               capture_output=True)
    return completed.stdout.splitlines()[0].strip()


def _validate_canonical_wav(path: Path) -> dict[str, int]:
    with wave.open(str(path), "rb") as stream:
        info = {
            "sample_rate": stream.getframerate(),
            "channels": stream.getnchannels(),
            "sample_width_bytes": stream.getsampwidth(),
            "frames": stream.getnframes(),
            "compression": stream.getcomptype(),
        }
    expected = {"sample_rate": 44100, "channels": 1,
                "sample_width_bytes": 2, "compression": "NONE"}
    for key, value in expected.items():
        if info[key] != value:
            raise ValueError(f"canonical WAV {path.name}: {key}={info[key]!r}, expected {value!r}")
    if info["frames"] <= 0:
        raise ValueError(f"canonical WAV {path.name}: no audio frames")
    return info


def _decode_to_pcm(ffmpeg: str, source: Path, destination: Path) -> None:
    completed = subprocess.run(
        [ffmpeg, "-nostdin", "-v", "error", "-y", "-i", str(source),
         "-map", "0:a:0", "-vn", "-ac", "1", "-ar", "44100",
         "-c:a", "pcm_s16le", "-map_metadata", "-1",
         "-fflags", "+bitexact", "-flags:a", "+bitexact", str(destination)],
        text=True, capture_output=True)
    if completed.returncode != 0:
        detail = completed.stderr.strip() or "no ffmpeg error text"
        raise RuntimeError(f"ffmpeg failed for {source.name}: {detail}")


def materialize(ledger_path: Path, input_root: Path, output_root: Path,
                ffmpeg_binary: str = "ffmpeg") -> list[dict]:
    """Canonicalise a validated ledger and atomically write its report.

    Existing output is accepted only if a fresh deterministic decode has the
    same byte hash. This makes reruns idempotent and makes stale/corrupt
    canonical audio a hard error rather than a silent cache hit.
    """
    rows = load_ledger(ledger_path, input_root)
    ffmpeg = shutil.which(ffmpeg_binary)
    if ffmpeg is None:
        raise FileNotFoundError(f"ffmpeg executable not found: {ffmpeg_binary}")
    ffmpeg_version = _ffmpeg_banner(ffmpeg)
    ffmpeg_sha256 = sha256_file(Path(ffmpeg).resolve())
    output_root.mkdir(parents=True, exist_ok=True)
    records: list[dict] = []

    for row in rows:
        target_dir = output_root / row.source_id
        target_dir.mkdir(parents=True, exist_ok=True)
        target = target_dir / f"{row.source_sha256}.wav"
        fd, temp_name = tempfile.mkstemp(prefix=".decode-", suffix=".wav", dir=target_dir)
        os.close(fd)
        temporary = Path(temp_name)
        try:
            _decode_to_pcm(ffmpeg, row.source_path, temporary)
            wav_info = _validate_canonical_wav(temporary)
            canonical_sha = sha256_file(temporary)
            if target.exists():
                _validate_canonical_wav(target)
                if sha256_file(target) != canonical_sha:
                    raise ValueError(
                        f"canonical output collision or corruption: {target}")
                temporary.unlink()
            else:
                os.replace(temporary, target)
            records.append({
                "source_id": row.source_id,
                "source_path": row.source_path.relative_to(input_root.resolve()).as_posix(),
                "source_sha256": row.source_sha256,
                "canonical_path": target.relative_to(output_root).as_posix(),
                "canonical_sha256": canonical_sha,
                "group": row.group,
                "domain": row.domain,
                "usage": row.usage,
                "license": row.license,
                "upstream_license": row.upstream_license,
                "attribution": row.attribution,
                "crosscheck": row.crosscheck,
                "source_url": row.source_url,
                "ffmpeg": ffmpeg_version,
                "ffmpeg_sha256": ffmpeg_sha256,
                **wav_info,
            })
        finally:
            temporary.unlink(missing_ok=True)

    report = {
        "schema": "a4b-corpus-materialisation-v1",
        "ledger_sha256": sha256_file(ledger_path),
        "records": records,
    }
    report_path = output_root / "admission_report.json"
    with tempfile.NamedTemporaryFile("w", suffix=".json", dir=output_root,
                                     delete=False) as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, sort_keys=True)
        stream.write("\n")
        temporary_report = Path(stream.name)
    os.replace(temporary_report, report_path)
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", type=Path, required=True)
    parser.add_argument("--input-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    records = materialize(args.ledger, args.input_root, args.output_root,
                          args.ffmpeg)
    print(json.dumps({"materialized_records": len(records),
                      "report": str(args.output_root / "admission_report.json")},
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
