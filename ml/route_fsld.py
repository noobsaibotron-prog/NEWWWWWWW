"""Route Freesound Loop Dataset (FSLD/FSL10K) into M6 Tier-2 folders.

Inputs are the extracted Zenodo archive plus expert annotations:
  - extracted/audio/wav/*.wav
  - extracted/fs_analysis/<sound_id>.json      (license/name/author)
  - extracted/fsld_annotations/.../sound-*.json (instrumentation labels)

Commercial-safety policy:
  - accept only CC0 and CC-BY per-file licenses
  - reject BY-NC, Sampling+, unknown/missing licenses
  - reject anything annotated as vocal
  - reject metadata names resembling the Ableton holdout source clips

Routing:
  - percussion-only + high measured HF prominence -> hf_negative
  - percussion-only otherwise -> clean_drums
  - bass-only -> clean_bass
  - melody/chords-only -> clean_synth
  - mixed instrumental loops -> clean_mix
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import wave
from pathlib import Path

import numpy as np


DEFAULT_EXTRACTED = "/Users/marco/aieq_data/downloads/extracted"
DEFAULT_TIER2 = "/Users/marco/aieq_data/real_audio/tier2_train"

LEAK_RE = re.compile(
    r"12.?inch|cs.?1x|arpeggiator|unmuffled|bass.?guitar|drum.?kit",
    re.IGNORECASE,
)

CAPS = {
    "hf_negative": 220,
    "clean_drums": 150,
    "clean_bass": 90,
    "clean_synth": 150,
    "clean_mix": 90,
}


def accepted_license(license_url: str) -> str | None:
    low = (license_url or "").lower()
    if "by-nc" in low or "sampling" in low:
        return None
    if "publicdomain/zero" in low:
        return "CC0"
    if "creativecommons.org/licenses/by/" in low:
        return "CC-BY"
    return None


def sound_id_from_audio(path: Path) -> str | None:
    m = re.match(r"(\d+)_", path.name)
    return m.group(1) if m else None


def load_existing_manifest(path: Path) -> set[str]:
    if not path.exists():
        return set()
    with path.open(newline="") as f:
        return {row.get("file", "") for row in csv.DictReader(f)}


def load_annotations(root: Path) -> dict[str, dict]:
    out = {}
    for path in (root / "fsld_annotations" / "annotations").rglob("sound-*.json"):
        sid = path.stem.split("-", 1)[1]
        try:
            out[sid] = json.loads(path.read_text())
        except Exception:
            continue
    return out


def wav_info(path: Path) -> tuple[int, np.ndarray] | None:
    try:
        with wave.open(str(path), "rb") as wav:
            sr = wav.getframerate()
            channels = wav.getnchannels()
            width = wav.getsampwidth()
            frames = min(wav.getnframes(), sr * 8)
            raw = wav.readframes(frames)
    except Exception:
        return None

    if sr < 32000 or frames < 4096:
        return None
    if width == 2:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        vals = (
            b[:, 0].astype(np.int32)
            | (b[:, 1].astype(np.int32) << 8)
            | (b[:, 2].astype(np.int32) << 16)
        )
        vals = np.where(vals >= 1 << 23, vals - (1 << 24), vals)
        x = vals.astype(np.float64) / float(1 << 23)
    else:
        return None
    if channels > 1:
        x = x.reshape(-1, channels).mean(axis=1)
    return sr, x


def hf_prominence_db(sr: int, x: np.ndarray) -> float:
    n = 4096
    if x.size < n:
        return -120.0
    win = np.hanning(n)
    hops = min(32, max(1, x.size // n))
    acc = np.zeros(n // 2 + 1)
    for i in range(hops):
        start = i * (x.size - n) // max(1, hops - 1)
        seg = x[start:start + n]
        acc += np.abs(np.fft.rfft(seg * win)) ** 2
    psd = acc / float(hops)
    freqs = np.fft.rfftfreq(n, 1.0 / float(sr))

    def band_db(lo: float, hi: float) -> float:
        mask = (freqs >= lo) & (freqs < hi)
        if not mask.any():
            return -120.0
        return float(10.0 * np.log10(psd[mask].mean() + 1e-20))

    return band_db(6000.0, min(14000.0, sr * 0.45)) - band_db(200.0, 2000.0)


def route_folder(inst: dict, hf_prom: float) -> str | None:
    percussion = bool(inst.get("percussion"))
    bass = bool(inst.get("bass"))
    melody = bool(inst.get("melody"))
    chords = bool(inst.get("chords"))
    fx = bool(inst.get("fx"))

    if percussion and not (bass or melody or chords or fx):
        return "hf_negative" if hf_prom > -8.0 else "clean_drums"
    if percussion and not fx:
        return "clean_mix"
    if bass and not (percussion or melody or chords or fx):
        return "clean_bass"
    if (melody or chords) and not (percussion or bass or fx):
        return "clean_synth"
    if (bass or melody or chords or percussion) and not fx:
        return "clean_mix"
    return None


def route(args: argparse.Namespace) -> dict[str, int]:
    extracted = Path(args.extracted)
    tier2 = Path(args.tier2)
    manifest = tier2 / "MANIFEST.csv"
    annotations = load_annotations(extracted)
    existing = load_existing_manifest(manifest)
    rows: list[dict[str, str]] = []
    counts = {k: 0 for k in CAPS}
    counts.update({"rejected": 0, "skipped": 0})

    audio_files = sorted((extracted / "audio" / "wav").glob("*.wav"))
    for audio in audio_files:
        sid = sound_id_from_audio(audio)
        if sid is None:
            counts["rejected"] += 1
            continue
        ann = annotations.get(sid)
        if not ann or ann.get("discard"):
            counts["rejected"] += 1
            continue
        inst = ann.get("instrumentation", {})
        if inst.get("vocal"):
            counts["rejected"] += 1
            continue

        meta_path = extracted / "fs_analysis" / f"{sid}.json"
        if not meta_path.exists():
            counts["rejected"] += 1
            continue
        try:
            meta = json.loads(meta_path.read_text())
        except Exception:
            counts["rejected"] += 1
            continue

        name = str(meta.get("name", audio.name))
        if LEAK_RE.search(name):
            counts["rejected"] += 1
            continue
        license_id = accepted_license(str(meta.get("license", "")))
        if license_id is None:
            counts["rejected"] += 1
            continue

        got = wav_info(audio)
        if got is None:
            counts["rejected"] += 1
            continue
        sr, x = got
        prom = hf_prominence_db(sr, x)
        folder = route_folder(inst, prom)
        if folder is None:
            counts["skipped"] += 1
            continue
        if counts[folder] >= CAPS[folder]:
            counts["skipped"] += 1
            continue

        rel = f"{folder}/fsld_{sid}.wav"
        if rel in existing:
            counts["skipped"] += 1
            continue
        dest = tier2 / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(audio, dest)
        rows.append(
            {
                "file": rel,
                "domain": folder,
                "source_url": f"https://freesound.org/s/{sid}/",
                "license": license_id,
                "attribution": str(meta.get("username", "Freesound")),
                "notes": (
                    f"FSLD loop; {name}; sr={sr}; hfprom={prom:.1f}dB; "
                    "https://zenodo.org/records/3967852"
                ),
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
        + " ".join(f"{k}={counts[k]}" for k in sorted(CAPS))
        + f" rejected={counts['rejected']} skipped={counts['skipped']}"
    )
    return counts


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--extracted", default=DEFAULT_EXTRACTED)
    ap.add_argument("--tier2", default=DEFAULT_TIER2)
    route(ap.parse_args())


if __name__ == "__main__":
    main()
