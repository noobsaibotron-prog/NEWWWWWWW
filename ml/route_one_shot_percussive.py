"""Route Freesound One-Shot Percussive Sounds into the M6 Tier-2 folders.

This is intentionally conservative for commercial/plugin training:
  - accept only CC0 and CC-BY licenses from the bundled licenses JSON
  - reject BY-NC, Sampling+, unknown/missing licenses
  - put clearly bright percussion (hats/cymbals/rides/crashes) in hf_negative
  - put other drum/percussion one-shots in clean_drums

The script is idempotent. It copies files into tier2_train/ and appends only
new manifest rows.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import wave
from pathlib import Path


DEFAULT_SOURCE = (
    "/Users/marco/aieq_data/downloads/extracted/one_shot_percussive_sounds"
)
DEFAULT_LICENSES = "/Users/marco/aieq_data/downloads/percussive_licenses.txt"
DEFAULT_TIER2 = "/Users/marco/aieq_data/real_audio/tier2_train"
SOURCE_URL = "https://zenodo.org/records/4687854"

# Leakage insurance: never route anything whose metadata resembles the Ableton
# holdout sources. The source dataset should not contain these, but the guard is
# cheap and makes the contract explicit.
LEAK_RE = re.compile(
    r"12.?inch|cs.?1x|arpeggiator|unmuffled|bass.?guitar|drum.?kit",
    re.IGNORECASE,
)

HF_RE = re.compile(
    r"(hi[-_ ]?hat|hihat|hhat|\bhh\b|cymbal|\bcym\b|\bcyn\b|ride|crash|"
    r"splash|china|open.?hat|closed.?hat|semi.?open)",
    re.IGNORECASE,
)

DRUM_RE = re.compile(
    r"(kick|bd\b|bass.?drum|snare|\bsd\b|tom|clap|rim|stick|perc|"
    r"percussion|shak|cow|agog|anvil|drum)",
    re.IGNORECASE,
)


def accepted_license(license_url: str) -> str | None:
    low = (license_url or "").lower()
    if "by-nc" in low or "sampling" in low:
        return None
    if "publicdomain/zero" in low or "/licenses/zero" in low:
        return "CC0"
    if "creativecommons.org/licenses/by/" in low:
        return "CC-BY"
    return None


def valid_wav(path: Path) -> bool:
    try:
        with wave.open(str(path), "rb") as wav:
            return wav.getnframes() >= 512 and wav.getframerate() >= 32000
    except Exception:
        return False


def load_existing_manifest(path: Path) -> set[str]:
    if not path.exists():
        return set()
    with path.open(newline="") as f:
        return {row.get("file", "") for row in csv.DictReader(f)}


def iter_audio(source: Path):
    for wav in sorted(source.rglob("*.wav")):
        sid = wav.stem
        if sid.isdigit():
            yield sid, wav


def classify(name: str) -> str | None:
    if HF_RE.search(name):
        return "hf_negative"
    if DRUM_RE.search(name):
        return "clean_drums"
    return None


def route(args: argparse.Namespace) -> dict[str, int]:
    source = Path(args.source)
    licenses_path = Path(args.licenses)
    tier2 = Path(args.tier2)
    manifest = tier2 / "MANIFEST.csv"

    licenses = json.loads(licenses_path.read_text())
    existing = load_existing_manifest(manifest)
    rows: list[dict[str, str]] = []
    counts = {"hf_negative": 0, "clean_drums": 0, "rejected": 0, "skipped": 0}
    limits = {"hf_negative": args.max_hf, "clean_drums": args.max_drums}

    for sid, src in iter_audio(source):
        meta = licenses.get(sid)
        if not isinstance(meta, dict):
            counts["rejected"] += 1
            continue

        name = str(meta.get("name", src.name))
        if LEAK_RE.search(name):
            counts["rejected"] += 1
            continue

        license_id = accepted_license(str(meta.get("license", "")))
        if license_id is None:
            counts["rejected"] += 1
            continue

        folder = classify(name)
        if folder is None:
            counts["skipped"] += 1
            continue
        if counts[folder] >= limits[folder]:
            counts["skipped"] += 1
            continue

        if not valid_wav(src):
            counts["rejected"] += 1
            continue

        rel = f"{folder}/oneshot_{sid}.wav"
        if rel in existing:
            counts["skipped"] += 1
            continue

        dest = tier2 / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        rows.append(
            {
                "file": rel,
                "domain": folder,
                "source_url": f"https://freesound.org/s/{sid}/",
                "license": license_id,
                "attribution": str(meta.get("username", "Freesound")),
                "notes": f"Freesound One-Shot Percussive Sounds; {name}; {SOURCE_URL}",
            }
        )
        existing.add(rel)
        counts[folder] += 1

    if rows:
        needs_header = not manifest.exists() or manifest.stat().st_size == 0
        with manifest.open("a", newline="") as f:
            writer = csv.DictWriter(
                f,
                fieldnames=[
                    "file",
                    "domain",
                    "source_url",
                    "license",
                    "attribution",
                    "notes",
                ],
            )
            if needs_header:
                writer.writeheader()
            writer.writerows(rows)

    print(
        "routed "
        f"hf_negative={counts['hf_negative']} "
        f"clean_drums={counts['clean_drums']} "
        f"rejected={counts['rejected']} skipped={counts['skipped']}"
    )
    return counts


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", default=DEFAULT_SOURCE)
    ap.add_argument("--licenses", default=DEFAULT_LICENSES)
    ap.add_argument("--tier2", default=DEFAULT_TIER2)
    ap.add_argument("--max-hf", type=int, default=220)
    ap.add_argument("--max-drums", type=int, default=150)
    route(ap.parse_args())


if __name__ == "__main__":
    main()
