from __future__ import annotations

import json
import tempfile
import unittest
import wave
from pathlib import Path
from unittest import mock

from ml_v3.prep import corpus_inventory
from ml_v3.prep.corpus_inventory import (
    InventoryError,
    build_inventory,
)


def _write_wav(path: Path, *, frames: int = 32, sample_rate: int = 48000) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sample_rate)
        handle.writeframes(b"\x00\x00" * frames)


def _source_map(root: Path, *, status: str = "quarantine", evidence=None) -> dict:
    return {
        "schema": "aieq-corpus-source-map-1",
        "sources": [
            {
                "attribution": "",
                "evidence_paths": [] if evidence is None else [str(evidence)],
                "rights_basis": "" if status != "verified" else "local evidence",
                "rights_status": status,
                "root": str(root),
                "source_id": "fixture-source",
                "source_url": "",
            }
        ],
    }


class CorpusInventoryTest(unittest.TestCase):
    def test_output_is_deterministic_and_schema_neutral(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "b.wav", frames=64)
            _write_wav(audio / "a.wav", frames=32)
            source_map = base / "sources.json"
            source_map.write_text(
                json.dumps(_source_map(audio), sort_keys=True),
                encoding="utf-8",
            )

            first = build_inventory(source_map, base / "out-1")
            second = build_inventory(source_map, base / "out-2")

            self.assertEqual(first.inventory_sha256, second.inventory_sha256)
            self.assertEqual(first.summary_sha256, second.summary_sha256)
            self.assertEqual(
                (base / "out-1" / "inventory.jsonl").read_bytes(),
                (base / "out-2" / "inventory.jsonl").read_bytes(),
            )
            rows = [
                json.loads(line)
                for line in (base / "out-1" / "inventory.jsonl")
                .read_text(encoding="utf-8")
                .splitlines()
            ]
            self.assertEqual([row["relative_path"] for row in rows], ["a.wav", "b.wav"])
            forbidden = {
                "benchmark_families",
                "labels",
                "profile",
                "source_family",
                "split_role",
                "targets",
            }
            self.assertFalse(forbidden.intersection(rows[0]))
            source_lock = json.loads(
                (base / "out-1" / "sources.lock.json").read_text(encoding="utf-8")
            )
            self.assertNotIn("source_family", source_lock["sources"][0])
            self.assertEqual(rows[0]["audio"]["duration_num"], 1)
            self.assertEqual(rows[0]["audio"]["duration_den"], 1500)

    def test_duplicate_content_is_reported_across_sources(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root_a = base / "a"
            root_b = base / "b"
            _write_wav(root_a / "same.wav")
            root_b.mkdir()
            (root_b / "copy.wav").write_bytes((root_a / "same.wav").read_bytes())
            source_map = base / "sources.json"
            payload = _source_map(root_a)
            second = dict(payload["sources"][0])
            second["root"] = str(root_b)
            second["source_id"] = "fixture-source-b"
            payload["sources"].append(second)
            source_map.write_text(json.dumps(payload), encoding="utf-8")

            result = build_inventory(source_map, base / "out")

            self.assertEqual(result.duplicate_file_count, 1)
            rows = [
                json.loads(line)
                for line in (base / "out" / "inventory.jsonl")
                .read_text(encoding="utf-8")
                .splitlines()
            ]
            self.assertEqual({row["content_group_size"] for row in rows}, {2})
            self.assertEqual(
                [row["canonical_content_ref"] for row in rows],
                [
                    {
                        "relative_path": "same.wav",
                        "source_id": "fixture-source",
                    },
                    {
                        "relative_path": "same.wav",
                        "source_id": "fixture-source",
                    },
                ],
            )

    def test_duplicate_json_keys_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "fixture.wav")
            source_map = base / "sources.json"
            raw = json.dumps(_source_map(audio))
            raw = raw.replace(
                '"source_id": "fixture-source"',
                '"source_id": "shadow", "source_id": "fixture-source"',
            )
            source_map.write_text(raw, encoding="utf-8")

            with self.assertRaisesRegex(InventoryError, "duplicate JSON key: source_id"):
                build_inventory(source_map, base / "out")

    def test_verified_rights_require_hashed_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "fixture.wav")
            source_map = base / "sources.json"
            source_map.write_text(
                json.dumps(_source_map(audio, status="verified")),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(InventoryError, "requires rights_basis"):
                build_inventory(source_map, base / "out")

            evidence = base / "license.txt"
            evidence.write_text("CC0 evidence fixture\n", encoding="utf-8")
            source_map.write_text(
                json.dumps(_source_map(audio, status="verified", evidence=evidence)),
                encoding="utf-8",
            )
            result = build_inventory(source_map, base / "out")
            self.assertEqual(result.error_count, 0)
            lock = json.loads((base / "out" / "sources.lock.json").read_text())
            self.assertEqual(lock["sources"][0]["evidence"][0]["size_bytes"], 21)

    def test_overlapping_source_roots_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            parent = base / "audio"
            child = parent / "child"
            child.mkdir(parents=True)
            payload = _source_map(parent)
            nested = dict(payload["sources"][0])
            nested["root"] = str(child)
            nested["source_id"] = "fixture-source-child"
            payload["sources"].append(nested)
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(payload), encoding="utf-8")

            with self.assertRaisesRegex(InventoryError, "overlapping source roots"):
                build_inventory(source_map, base / "out")

    def test_corrupt_audio_is_recorded_as_no_go(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            audio.mkdir()
            (audio / "broken.wav").write_bytes(b"not a wave")
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")

            result = build_inventory(source_map, base / "out")

            self.assertEqual(result.error_count, 1)
            row = json.loads((base / "out" / "inventory.jsonl").read_text())
            self.assertIsNone(row["audio"])
            self.assertIn("Error", row["error"])

    def test_empty_source_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            audio.mkdir()
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")

            result = build_inventory(source_map, base / "out")

            self.assertEqual(result.empty_source_count, 1)
            summary = json.loads((base / "out" / "summary.json").read_text())
            self.assertEqual(summary["empty_source_ids"], ["fixture-source"])
            self.assertEqual(summary["record_count_by_source"], {"fixture-source": 0})

    def test_audio_symlink_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            external = base / "external.wav"
            _write_wav(external)
            audio.mkdir()
            (audio / "linked.wav").symlink_to(external)
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")

            with self.assertRaisesRegex(InventoryError, "symlink rejected"):
                build_inventory(source_map, base / "out")

    def test_directory_symlink_is_rejected_instead_of_silently_omitted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            external = base / "external"
            _write_wav(audio / "regular.wav")
            _write_wav(external / "linked.wav")
            (audio / "linked-subtree").symlink_to(external, target_is_directory=True)
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")

            with self.assertRaisesRegex(InventoryError, "directory symlink rejected"):
                build_inventory(source_map, base / "out")

    def test_audio_changed_during_scan_is_recorded_as_no_go(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            target = audio / "fixture.wav"
            _write_wav(target)
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")
            original_probe = corpus_inventory._probe_audio

            def probe_then_mutate(path: Path, ffprobe: str | None):
                result = original_probe(path, ffprobe)
                with path.open("ab") as handle:
                    handle.write(b"changed-during-inventory")
                return result

            with mock.patch.object(
                corpus_inventory,
                "_probe_audio",
                side_effect=probe_then_mutate,
            ):
                result = build_inventory(source_map, base / "out")

            self.assertEqual(result.error_count, 1)
            row = json.loads((base / "out" / "inventory.jsonl").read_text())
            self.assertIsNone(row["audio"])
            self.assertIsNone(row["sha256"])
            self.assertIn("audio changed while inventorying", row["error"])

    def test_ffprobe_decimal_duration_is_not_mislabeled_exact(self) -> None:
        completed = mock.Mock(
            returncode=0,
            stderr="",
            stdout=json.dumps(
                {
                    "format": {"duration": "1.234567", "format_name": "mp3"},
                    "streams": [
                        {
                            "channels": 2,
                            "codec_name": "mp3",
                            "sample_rate": "48000",
                        }
                    ],
                }
            ),
        )
        with mock.patch.object(corpus_inventory.subprocess, "run", return_value=completed):
            with self.assertRaisesRegex(InventoryError, "no exact stream duration"):
                corpus_inventory._probe_ffprobe(Path("fixture.mp3"), "ffprobe")

    def test_output_inside_source_root_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "fixture.wav")
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")

            with self.assertRaisesRegex(InventoryError, "outside every source root"):
                build_inventory(source_map, audio / "inventory-output")

    def test_unmanaged_output_entries_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "fixture.wav")
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")
            output = base / "out"
            output.mkdir()
            (output / "stale-unmanaged.json").write_text("{}\n", encoding="utf-8")

            with self.assertRaisesRegex(InventoryError, "unmanaged entries"):
                build_inventory(source_map, output)

    def test_post_write_corruption_is_detected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "fixture.wav")
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")
            original_write = corpus_inventory._atomic_write

            def write_then_corrupt(path: Path, content: bytes) -> None:
                original_write(path, content)
                if path.name == "inventory.jsonl":
                    path.write_bytes(content + b"corrupt")

            with mock.patch.object(
                corpus_inventory,
                "_atomic_write",
                side_effect=write_then_corrupt,
            ):
                with self.assertRaisesRegex(InventoryError, "checksum mismatch"):
                    build_inventory(source_map, base / "out")

    def test_appledouble_metadata_is_counted_and_not_probed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            audio = base / "audio"
            _write_wav(audio / "real.wav")
            (audio / "._real.wav").write_bytes(b"finder metadata")
            source_map = base / "sources.json"
            source_map.write_text(json.dumps(_source_map(audio)), encoding="utf-8")

            result = build_inventory(source_map, base / "out")

            self.assertEqual(result.record_count, 1)
            self.assertEqual(result.error_count, 0)
            self.assertEqual(result.ignored_metadata_file_count, 1)
            summary = json.loads((base / "out" / "summary.json").read_text())
            self.assertEqual(summary["ignored_metadata_file_count"], 1)


if __name__ == "__main__":
    unittest.main()
