"""Plain-Python guards for ml_v2.corpus_admission.

Run: python3 -m ml_v2.tests.test_corpus_admission
"""
from __future__ import annotations

import csv
import hashlib
import json
import tempfile
import wave
from pathlib import Path

from ml_v2.corpus_admission import LEDGER_FIELDS, load_ledger, materialize


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _make_wav(path: Path) -> None:
    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(2)
        stream.setsampwidth(2)
        stream.setframerate(8000)
        stream.writeframes((b"\x00\x00\x10\x00") * 800)


def _row(path: Path, **overrides: str) -> dict[str, str]:
    row = {
        "source_id": "owned", "source_path": path.name,
        "source_sha256": _sha(path), "group": "owned:fixture-01",
        "domain": "clean_synth", "usage": "train_candidate",
        "license": "OWNED", "upstream_license": "fixture-eula-v1",
        "attribution": "fixture owner", "crosscheck": "fixture-receipt-1",
        "source_url": "https://example.test/fixture",
    }
    row.update(overrides)
    return row


def _write_ledger(root: Path, row: dict[str, str]) -> Path:
    path = root / "ledger.csv"
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=LEDGER_FIELDS)
        writer.writeheader()
        writer.writerow(row)
    return path


def _expect_error(action, needle: str) -> None:
    try:
        action()
    except (ValueError, FileNotFoundError) as error:
        assert needle in str(error), error
        return
    raise AssertionError(f"expected error containing {needle!r}")


def test_ledger_rejects_source_hash_drift() -> None:
    root = Path(tempfile.mkdtemp())
    source = root / "fixture.wav"
    _make_wav(source)
    ledger = _write_ledger(root, _row(source, source_sha256="a" * 64))
    _expect_error(lambda: load_ledger(ledger, root), "source_sha256 mismatch")


def test_ledger_rejects_missing_crosscheck() -> None:
    root = Path(tempfile.mkdtemp())
    source = root / "fixture.wav"
    _make_wav(source)
    ledger = _write_ledger(root, _row(source, crosscheck="n/a"))
    _expect_error(lambda: load_ledger(ledger, root), "crosscheck is required")


def test_fma_is_eval_only() -> None:
    root = Path(tempfile.mkdtemp())
    source = root / "fixture.mp3"
    source.write_bytes(b"fixture")
    ledger = _write_ledger(root, _row(
        source, source_id="fma", source_path=source.name,
        source_sha256=_sha(source), license="CC-BY",
        upstream_license="CC-BY-3.0", usage="train_candidate"))
    _expect_error(lambda: load_ledger(ledger, root), "restricted")


def test_ledger_rejects_path_escape() -> None:
    root = Path(tempfile.mkdtemp())
    source = root / "fixture.wav"
    _make_wav(source)
    ledger = _write_ledger(root, _row(source, source_path="../fixture.wav"))
    _expect_error(lambda: load_ledger(ledger, root), "must be relative")


def test_ledger_binds_group_to_source() -> None:
    root = Path(tempfile.mkdtemp())
    source = root / "fixture.wav"
    _make_wav(source)
    ledger = _write_ledger(root, _row(source, group="fma:fixture-01"))
    _expect_error(lambda: load_ledger(ledger, root), "owned:<stable-id>")


def test_materialization_is_pcm_and_idempotent() -> None:
    root = Path(tempfile.mkdtemp())
    source = root / "fixture.wav"
    _make_wav(source)
    ledger = _write_ledger(root, _row(source))
    output = root / "canonical"
    first = materialize(ledger, root, output)
    second = materialize(ledger, root, output)
    assert first == second
    canonical = output / first[0]["canonical_path"]
    with wave.open(str(canonical), "rb") as stream:
        assert (stream.getnchannels(), stream.getsampwidth(), stream.getframerate()) == (1, 2, 44100)
    report = json.loads((output / "admission_report.json").read_text())
    assert report["records"] == first
    assert len(first[0]["ffmpeg_sha256"]) == 64


def main() -> int:
    tests = [value for name, value in sorted(globals().items())
             if name.startswith("test_") and callable(value)]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    print(f"{len(tests)}/{len(tests)} corpus admission tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
