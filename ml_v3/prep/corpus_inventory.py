"""Build a deterministic, schema-neutral inventory of local audio corpora.

This module deliberately stops before corpus admission. It records immutable
file facts and source-rights evidence, but never assigns V3 split roles,
benchmark families, labels, profiles, or model targets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import stat
import subprocess
import tempfile
import wave
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
from typing import Any, Iterable, Sequence


SOURCE_MAP_SCHEMA = "aieq-corpus-source-map-1"
SOURCE_LOCK_SCHEMA = "aieq-corpus-source-lock-0"
INVENTORY_RECORD_SCHEMA = "aieq-corpus-inventory-record-1"
INVENTORY_SUMMARY_SCHEMA = "aieq-corpus-inventory-summary-0"
TOOL_VERSION = "0.2.0"

RIGHTS_STATUSES = frozenset({"verified", "quarantine", "unknown"})
AUDIO_EXTENSIONS = frozenset({".wav", ".wave", ".flac", ".mp3", ".aif", ".aiff", ".ogg"})
HASH_CHUNK_BYTES = 1024 * 1024
OUTPUT_FILENAMES = frozenset(
    {"inventory.jsonl", "sources.lock.json", "summary.json", "SHA256SUMS"}
)


class InventoryError(RuntimeError):
    """Raised when inventory inputs violate a fail-closed precondition."""


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise InventoryError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_json_constant(value: str) -> None:
    raise InventoryError(f"non-finite JSON constant is forbidden: {value}")


@dataclass(frozen=True)
class SourceSpec:
    source_id: str
    root: Path
    source_url: str
    rights_status: str
    rights_basis: str
    attribution: str
    evidence_paths: tuple[Path, ...]


@dataclass(frozen=True)
class InventoryResult:
    record_count: int
    error_count: int
    empty_source_count: int
    ignored_metadata_file_count: int
    duplicate_file_count: int
    inventory_sha256: str
    summary_sha256: str
    source_lock_sha256: str
    output_dir: Path


def _canonical_json_bytes(value: Any) -> bytes:
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
        + b"\n"
    )


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(HASH_CHUNK_BYTES):
            digest.update(chunk)
    return digest.hexdigest()


def _require_nonempty_string(data: dict[str, Any], key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        raise InventoryError(f"{key} must be a non-empty string")
    return value


def _require_string(data: dict[str, Any], key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str):
        raise InventoryError(f"{key} must be a string")
    return value


def _read_source_map(path: Path) -> tuple[str, tuple[SourceSpec, ...]]:
    try:
        raw = path.read_bytes()
        data = json.loads(
            raw,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_json_constant,
        )
    except (OSError, json.JSONDecodeError) as exc:
        raise InventoryError(f"cannot read source map {path}: {exc}") from exc

    if not isinstance(data, dict) or data.get("schema") != SOURCE_MAP_SCHEMA:
        raise InventoryError(f"source map schema must be {SOURCE_MAP_SCHEMA}")
    if set(data) != {"schema", "sources"}:
        raise InventoryError("source map keys must be exactly: schema, sources")

    rows = data["sources"]
    if not isinstance(rows, list) or not rows:
        raise InventoryError("sources must be a non-empty array")

    expected_keys = {
        "attribution",
        "evidence_paths",
        "rights_basis",
        "rights_status",
        "root",
        "source_id",
        "source_url",
    }
    specs: list[SourceSpec] = []
    seen_ids: set[str] = set()
    seen_roots: set[Path] = set()

    for index, row in enumerate(rows):
        if not isinstance(row, dict) or set(row) != expected_keys:
            raise InventoryError(
                f"sources[{index}] keys must be exactly: {', '.join(sorted(expected_keys))}"
            )

        source_id = _require_nonempty_string(row, "source_id")
        if source_id in seen_ids:
            raise InventoryError(f"duplicate source_id: {source_id}")
        seen_ids.add(source_id)

        root_text = _require_nonempty_string(row, "root")
        root = Path(root_text).expanduser()
        if root.is_symlink():
            raise InventoryError(f"source root must not be a symlink: {root}")
        root = root.resolve()
        if not root.is_dir():
            raise InventoryError(f"source root is not a directory: {root}")
        if root in seen_roots:
            raise InventoryError(f"duplicate source root: {root}")
        for other_root in seen_roots:
            if root in other_root.parents or other_root in root.parents:
                raise InventoryError(
                    f"overlapping source roots are forbidden: {root} and {other_root}"
                )
        seen_roots.add(root)

        rights_status = _require_nonempty_string(row, "rights_status")
        if rights_status not in RIGHTS_STATUSES:
            raise InventoryError(
                f"invalid rights_status {rights_status!r}; expected one of "
                f"{sorted(RIGHTS_STATUSES)}"
            )

        evidence_rows = row["evidence_paths"]
        if not isinstance(evidence_rows, list) or any(
            not isinstance(item, str) or not item.strip() for item in evidence_rows
        ):
            raise InventoryError(f"sources[{index}].evidence_paths must be an array of paths")

        evidence_paths: list[Path] = []
        for evidence_text in evidence_rows:
            evidence = Path(evidence_text).expanduser()
            if evidence.is_symlink():
                raise InventoryError(f"rights evidence must not be a symlink: {evidence}")
            evidence = evidence.resolve()
            if not evidence.is_file():
                raise InventoryError(f"rights evidence is not a file: {evidence}")
            evidence_paths.append(evidence)

        rights_basis = _require_string(row, "rights_basis")
        source_url = _require_string(row, "source_url")
        attribution = _require_string(row, "attribution")
        if rights_status == "verified" and (not rights_basis.strip() or not evidence_paths):
            raise InventoryError(
                f"verified source {source_id!r} requires rights_basis and evidence_paths"
            )

        specs.append(
            SourceSpec(
                source_id=source_id,
                root=root,
                source_url=source_url,
                rights_status=rights_status,
                rights_basis=rights_basis,
                attribution=attribution,
                evidence_paths=tuple(evidence_paths),
            )
        )

    return hashlib.sha256(raw).hexdigest(), tuple(sorted(specs, key=lambda item: item.source_id))


def _raise_walk_error(error: OSError) -> None:
    raise InventoryError(f"cannot traverse source tree: {error}") from error


def _audio_paths(source: SourceSpec) -> tuple[list[Path], int]:
    paths: list[Path] = []
    ignored_metadata_file_count = 0
    for dirpath, dirnames, filenames in os.walk(
        source.root,
        followlinks=False,
        onerror=_raise_walk_error,
    ):
        dirnames.sort()
        filenames.sort()
        directory = Path(dirpath)

        # A linked directory can silently hide or import an arbitrary audio
        # subtree because os.walk correctly refuses to follow it.  Ignoring it
        # would nevertheless make the claimed source snapshot incomplete, so
        # reject the source instead of treating the link as an empty folder.
        for dirname in dirnames:
            candidate = directory / dirname
            if candidate.is_symlink():
                raise InventoryError(f"directory symlink rejected: {candidate}")

        for filename in filenames:
            path = directory / filename
            if path.suffix.lower() not in AUDIO_EXTENSIONS:
                continue
            if path.name.startswith("._"):
                ignored_metadata_file_count += 1
                continue
            if path.is_symlink():
                raise InventoryError(f"audio symlink rejected: {path}")
            if path.is_file():
                paths.append(path)
    return (
        sorted(paths, key=lambda item: item.relative_to(source.root).as_posix()),
        ignored_metadata_file_count,
    )


def _fraction_fields(value: Fraction) -> dict[str, int]:
    return {
        "duration_num": value.numerator,
        "duration_den": value.denominator,
    }


def _probe_wave(path: Path) -> dict[str, Any]:
    with wave.open(str(path), "rb") as handle:
        frames = handle.getnframes()
        sample_rate = handle.getframerate()
        if frames <= 0 or sample_rate <= 0:
            raise InventoryError("WAV has no positive-duration audio")
        return {
            "channels": handle.getnchannels(),
            "codec": f"pcm_s{handle.getsampwidth() * 8}",
            "container": "wav",
            "frames": frames,
            "probe_backend": "python-wave",
            "sample_rate": sample_rate,
            **_fraction_fields(Fraction(frames, sample_rate)),
        }


def _probe_ffprobe(path: Path, ffprobe: str) -> dict[str, Any]:
    command = [
        ffprobe,
        "-v",
        "error",
        "-select_streams",
        "a:0",
        "-show_entries",
        "stream=channels,codec_name,sample_rate,duration_ts,time_base:format=format_name,duration",
        "-of",
        "json",
        str(path),
    ]
    completed = subprocess.run(command, capture_output=True, check=False, text=True)
    if completed.returncode != 0:
        detail = completed.stderr.strip() or f"ffprobe exited {completed.returncode}"
        raise InventoryError(detail)

    try:
        payload = json.loads(completed.stdout)
        streams = payload["streams"]
        stream = streams[0]
        channels = int(stream["channels"])
        sample_rate = int(stream["sample_rate"])
    except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as exc:
        raise InventoryError(f"ffprobe returned incomplete audio metadata: {exc}") from exc

    if channels <= 0 or sample_rate <= 0:
        raise InventoryError("ffprobe returned invalid channels or sample rate")

    duration: Fraction
    frames: int | None = None
    if stream.get("duration_ts") is None or stream.get("time_base") is None:
        # A decimal container duration is only ffprobe's formatted estimate;
        # wrapping that text in Fraction would make the estimate rational, not
        # make it an exact stream duration.  Inventory only authoritative ticks.
        raise InventoryError("ffprobe returned no exact stream duration")
    try:
        duration = Fraction(int(stream["duration_ts"])) * Fraction(stream["time_base"])
    except (TypeError, ValueError, ZeroDivisionError) as exc:
        raise InventoryError(f"invalid ffprobe duration_ts/time_base: {exc}") from exc
    frame_fraction = duration * sample_rate
    if frame_fraction.denominator == 1:
        frames = frame_fraction.numerator

    if duration <= 0:
        raise InventoryError("audio has no positive duration")

    result: dict[str, Any] = {
        "channels": channels,
        "codec": str(stream.get("codec_name", "unknown")),
        "container": str(payload.get("format", {}).get("format_name", "unknown")),
        "frames": frames,
        "probe_backend": "ffprobe",
        "sample_rate": sample_rate,
        **_fraction_fields(duration),
    }
    return result


def _probe_audio(path: Path, ffprobe: str | None) -> dict[str, Any]:
    if path.suffix.lower() in {".wav", ".wave"}:
        return _probe_wave(path)
    if ffprobe is None:
        raise InventoryError("ffprobe is required for non-WAV audio")
    return _probe_ffprobe(path, ffprobe)


def _source_lock(source_map_sha256: str, sources: Sequence[SourceSpec]) -> dict[str, Any]:
    rows: list[dict[str, Any]] = []
    for source in sources:
        evidence: list[dict[str, Any]] = []
        for path in sorted(source.evidence_paths, key=lambda item: str(item)):
            before = _file_fingerprint(path)
            digest = _sha256_file(path)
            after = _file_fingerprint(path)
            if after != before:
                raise InventoryError(f"rights evidence changed while hashing: {path}")
            evidence.append(
                {
                    "path": str(path),
                    "sha256": digest,
                    "size_bytes": before[3],
                }
            )
        rows.append(
            {
                "attribution": source.attribution,
                "evidence": evidence,
                "rights_basis": source.rights_basis,
                "rights_status": source.rights_status,
                "root": str(source.root),
                "source_id": source.source_id,
                "source_url": source.source_url,
            }
        )
    return {
        "schema": SOURCE_LOCK_SCHEMA,
        "source_map_sha256": source_map_sha256,
        "sources": rows,
    }


def _ffprobe_version(ffprobe: str | None, *, used: bool) -> str | None:
    if not used:
        return None
    if ffprobe is None:
        return "unavailable"
    completed = subprocess.run(
        [ffprobe, "-version"],
        capture_output=True,
        check=False,
        text=True,
    )
    if completed.returncode != 0:
        return f"unreadable-exit-{completed.returncode}"
    lines = completed.stdout.splitlines()
    return lines[0].strip() if lines else "unreadable-empty-output"


def _atomic_write(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def _jsonl_bytes(rows: Iterable[dict[str, Any]]) -> bytes:
    return b"".join(_canonical_json_bytes(row) for row in rows)


def _file_fingerprint(path: Path) -> tuple[int, int, int, int, int, int]:
    """Return fields that must remain stable while a file is inventoried."""

    details = path.stat(follow_symlinks=False)
    if not stat.S_ISREG(details.st_mode):
        raise InventoryError(f"audio path is not a regular file: {path}")
    return (
        details.st_dev,
        details.st_ino,
        details.st_mode,
        details.st_size,
        details.st_mtime_ns,
        details.st_ctime_ns,
    )


def _validate_output_directory(output_dir: Path, sources: Sequence[SourceSpec]) -> None:
    if output_dir.is_symlink():
        raise InventoryError(f"output directory must not be a symlink: {output_dir}")
    if output_dir.exists() and not output_dir.is_dir():
        raise InventoryError(f"output path is not a directory: {output_dir}")
    for source in sources:
        if output_dir == source.root or source.root in output_dir.parents:
            raise InventoryError(
                f"output directory must be outside every source root: {output_dir}"
            )
    if output_dir.exists():
        unexpected = sorted(
            path.name for path in output_dir.iterdir() if path.name not in OUTPUT_FILENAMES
        )
        if unexpected:
            raise InventoryError(
                "output directory contains unmanaged entries: " + ", ".join(unexpected)
            )


def _verify_written_outputs(
    output_dir: Path,
    *,
    inventory_sha256: str,
    source_lock_sha256: str,
    summary_sha256: str,
    sums: bytes,
) -> None:
    expected = {
        "inventory.jsonl": inventory_sha256,
        "sources.lock.json": source_lock_sha256,
        "summary.json": summary_sha256,
    }
    for name, digest in expected.items():
        if _sha256_file(output_dir / name) != digest:
            raise InventoryError(f"post-write checksum mismatch: {name}")
    if (output_dir / "SHA256SUMS").read_bytes() != sums:
        raise InventoryError("post-write checksum manifest mismatch")


def build_inventory(
    source_map_path: Path,
    output_dir: Path,
    *,
    ffprobe_path: str | None = None,
) -> InventoryResult:
    """Build the inventory and return its deterministic summary."""

    source_map_path = source_map_path.expanduser().resolve()
    output_dir = output_dir.expanduser().absolute()
    if output_dir.is_symlink():
        raise InventoryError(f"output directory must not be a symlink: {output_dir}")
    output_dir = output_dir.resolve()
    source_map_sha256, sources = _read_source_map(source_map_path)
    _validate_output_directory(output_dir, sources)

    if ffprobe_path is None:
        ffprobe_path = shutil.which("ffprobe")

    records: list[dict[str, Any]] = []
    ignored_metadata_file_count = 0
    used_ffprobe = False
    for source in sources:
        paths, source_ignored_count = _audio_paths(source)
        ignored_metadata_file_count += source_ignored_count
        for path in paths:
            used_ffprobe = used_ffprobe or path.suffix.lower() not in {".wav", ".wave"}
            relative_path = path.relative_to(source.root).as_posix()
            record: dict[str, Any] = {
                "audio": None,
                "error": None,
                "extension": path.suffix.lower(),
                "relative_path": relative_path,
                "schema": INVENTORY_RECORD_SCHEMA,
                "sha256": None,
                "size_bytes": None,
                "source_id": source.source_id,
            }
            try:
                before = _file_fingerprint(path)
                record["size_bytes"] = before[3]
                record["sha256"] = _sha256_file(path)
                record["audio"] = _probe_audio(path, ffprobe_path)
                after = _file_fingerprint(path)
                if after != before:
                    record["sha256"] = None
                    record["size_bytes"] = None
                    raise InventoryError(f"audio changed while inventorying: {path}")
            except (InventoryError, OSError, wave.Error) as exc:
                record["audio"] = None
                record["error"] = f"{type(exc).__name__}: {exc}"
            records.append(record)

    records.sort(key=lambda row: (row["source_id"], row["relative_path"]))

    content_refs: dict[str, list[tuple[str, str]]] = {}
    for row in records:
        if row["sha256"] is None:
            continue
        ref = (row["source_id"], row["relative_path"])
        content_refs.setdefault(row["sha256"], []).append(ref)
    for refs in content_refs.values():
        refs.sort()

    for row in records:
        if row["sha256"] is None:
            row["canonical_content_ref"] = None
            row["content_group_size"] = 0
            continue
        refs = content_refs[row["sha256"]]
        row["canonical_content_ref"] = {
            "relative_path": refs[0][1],
            "source_id": refs[0][0],
        }
        row["content_group_size"] = len(refs)

    source_lock = _source_lock(source_map_sha256, sources)
    inventory_bytes = _jsonl_bytes(records)
    inventory_sha256 = hashlib.sha256(inventory_bytes).hexdigest()
    source_lock_bytes = _canonical_json_bytes(source_lock)
    source_lock_sha256 = hashlib.sha256(source_lock_bytes).hexdigest()

    error_count = sum(row["error"] is not None for row in records)
    duplicate_file_count = sum(len(refs) - 1 for refs in content_refs.values())
    duplicate_group_count = sum(len(refs) > 1 for refs in content_refs.values())
    cross_source_groups = [
        refs for refs in content_refs.values() if len({ref[0] for ref in refs}) > 1
    ]
    within_source_groups = [
        refs
        for refs in content_refs.values()
        if len(refs) > 1 and len({ref[0] for ref in refs}) == 1
    ]
    record_count_by_source = {
        source.source_id: sum(row["source_id"] == source.source_id for row in records)
        for source in sources
    }
    empty_source_ids = sorted(
        source_id for source_id, count in record_count_by_source.items() if count == 0
    )
    summary = {
        "cross_source_duplicate_file_count": sum(
            len(refs) for refs in cross_source_groups
        ),
        "cross_source_duplicate_group_count": len(cross_source_groups),
        "duplicate_file_count": duplicate_file_count,
        "duplicate_group_count": duplicate_group_count,
        "empty_source_ids": empty_source_ids,
        "error_count": error_count,
        "ffprobe_version": _ffprobe_version(ffprobe_path, used=used_ffprobe),
        "ignored_metadata_file_count": ignored_metadata_file_count,
        "inventory_sha256": inventory_sha256,
        "record_count": len(records),
        "record_count_by_source": record_count_by_source,
        "rights_status_counts": {
            status: sum(source.rights_status == status for source in sources)
            for status in sorted(RIGHTS_STATUSES)
        },
        "schema": INVENTORY_SUMMARY_SCHEMA,
        "source_count": len(sources),
        "source_lock_sha256": source_lock_sha256,
        "source_map_sha256": source_map_sha256,
        "tool_version": TOOL_VERSION,
        "within_source_duplicate_file_count": sum(
            len(refs) for refs in within_source_groups
        ),
        "within_source_duplicate_group_count": len(within_source_groups),
    }
    summary_bytes = _canonical_json_bytes(summary)
    summary_sha256 = hashlib.sha256(summary_bytes).hexdigest()

    _atomic_write(output_dir / "inventory.jsonl", inventory_bytes)
    _atomic_write(output_dir / "sources.lock.json", source_lock_bytes)
    _atomic_write(output_dir / "summary.json", summary_bytes)
    sums = (
        f"{inventory_sha256}  inventory.jsonl\n"
        f"{source_lock_sha256}  sources.lock.json\n"
        f"{summary_sha256}  summary.json\n"
    ).encode("ascii")
    _atomic_write(output_dir / "SHA256SUMS", sums)
    _verify_written_outputs(
        output_dir,
        inventory_sha256=inventory_sha256,
        source_lock_sha256=source_lock_sha256,
        summary_sha256=summary_sha256,
        sums=sums,
    )

    return InventoryResult(
        record_count=len(records),
        error_count=error_count,
        empty_source_count=len(empty_source_ids),
        ignored_metadata_file_count=ignored_metadata_file_count,
        duplicate_file_count=duplicate_file_count,
        inventory_sha256=inventory_sha256,
        summary_sha256=summary_sha256,
        source_lock_sha256=source_lock_sha256,
        output_dir=output_dir,
    )


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-map", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument(
        "--ffprobe",
        default=None,
        help="ffprobe executable; defaults to PATH lookup",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        result = build_inventory(args.source_map, args.out, ffprobe_path=args.ffprobe)
    except InventoryError as exc:
        print(f"ERROR: {exc}")
        return 2

    print(
        f"inventory records={result.record_count} errors={result.error_count} "
        f"empty_sources={result.empty_source_count} "
        f"ignored_metadata={result.ignored_metadata_file_count} "
        f"duplicate_files={result.duplicate_file_count}"
    )
    print(f"output={result.output_dir}")
    if result.error_count or result.empty_source_count:
        print("NO-GO: audio probe errors or configured empty sources remain")
        return 3
    print("PASS: schema-neutral corpus inventory complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
