#!/usr/bin/env python3
"""Motore v2 — A1: corpus manifest, license audit, anti-leakage, splits.

Scope (Marco, 2026-07-12): report + committed manifest ONLY. No training, no
model, no runtime. A4 training does not start until this audit is green.

What it does, deterministically (no RNG):
  1. MANIFEST_V2.csv — every corpus file (VocalSet + tier2_train) with sha256,
     RIFF-parsed sr/channels/bits/duration, domain, group id, split, license.
  2. License audit — normalizes every claim; anything NC/ND/unknown is a
     VIOLATION. For freesound-sourced files (fsld_<id> / one-shot percussive)
     the manifest claim is CROSS-CHECKED against the upstream per-id records
     (FSL10K metadata.json, percussive_licenses.txt).
  3. Anti-leakage — sha256 duplicates across splits = FAIL; the 8 Ableton
     judge clips (AIEQ_Ableton_Test_Clips) must have ZERO hash overlap with
     the corpus = else FAIL.
  4. Splits — vocal: singer-disjoint, test = lab TEST_SINGERS (female8/9,
     male10/11, unchanged for cross-program comparability), calibration
     heldout = CALIB_SINGERS (female7, male9); tier2: group-disjoint
     (BabySlakh stems+mix share the track group) via sha1(group) mod 100 →
     70/15/15 train/heldout/test.

Exit code: 0 = green, 1 = violations found (report says which).

Run:  python3 -m ml_v2.a1_corpus   (from the repo root)
"""
from __future__ import annotations

import csv
import hashlib
import json
import os
import struct
import sys
from pathlib import Path

DATA_ROOT = Path(os.environ.get("AIEQ_DATA_ROOT", str(Path.home() / "aieq_data")))
VOCALSET_DIR = DATA_ROOT / "real_audio" / "vocalset_extracted" / "FULL"
TIER2_DIR = DATA_ROOT / "real_audio" / "tier2_train"
FSL_METADATA = DATA_ROOT / "downloads" / "extracted" / "metadata.json"
PERC_LICENSES = DATA_ROOT / "downloads" / "percussive_licenses.txt"
JUDGE_DIR = Path(os.environ.get("AIEQ_JUDGE_DIR",
                                str(Path.home() / "Desktop" / "AIEQ_Ableton_Test_Clips")))

HERE = Path(__file__).resolve().parent
OUT_DIR = HERE / "data"
MANIFEST_OUT = OUT_DIR / "MANIFEST_V2.csv"
REPORT_OUT = OUT_DIR / "A1_REPORT.md"

# Lab split (ml/dataset.py TEST_SINGERS) — kept IDENTICAL so v2 test results
# stay comparable with the M6-M9 benchmarks.
TEST_SINGERS = ("female9", "female8", "male11", "male10")
# Calibration heldout (threshold calibration ONLY — never trained on, never
# used as the judge set).
CALIB_SINGERS = ("female7", "male9")

COMMERCIAL_OK = {"CC0", "CC-BY", "CC-BY-3.0", "CC-BY-4.0"}


# ------------------------------------------------------------------ helpers
def norm_license(raw: str) -> str:
    """Normalize a license string/URL to a short form."""
    s = raw.strip().rstrip("/").lower()
    if not s:
        return "UNKNOWN"
    if "zero" in s or s.endswith("publicdomain") or "cc0" in s:
        return "CC0"
    if "by-nc-nd" in s:
        return "CC-BY-NC-ND"
    if "by-nc-sa" in s:
        return "CC-BY-NC-SA"
    if "by-nc" in s:
        return "CC-BY-NC"
    if "by-nd" in s:
        return "CC-BY-ND"
    if "by-sa" in s:
        return "CC-BY-SA"
    if "by/4.0" in s or s == "cc-by-4.0":
        return "CC-BY-4.0"
    if "by/3.0" in s or s == "cc-by-3.0":
        return "CC-BY-3.0"
    if "by" in s and "cc" in s or "licenses/by" in s:
        return "CC-BY"
    return raw.strip().upper()


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def riff_info(path: Path) -> dict:
    """Parse RIFF fmt/data chunks without decoding audio."""
    info = {"sr": 0, "channels": 0, "bits": 0, "duration_s": 0.0, "fmt": 0}
    with open(path, "rb") as f:
        hdr = f.read(12)
        if len(hdr) < 12 or hdr[:4] != b"RIFF" or hdr[8:12] != b"WAVE":
            return info
        data_size = None
        while True:
            ch = f.read(8)
            if len(ch) < 8:
                break
            cid, csize = ch[:4], struct.unpack("<I", ch[4:])[0]
            if cid == b"fmt ":
                fmt = f.read(csize)
                (tag, nch, sr, _br, _ba, bits) = struct.unpack("<HHIIHH", fmt[:16])
                if tag == 0xFFFE and csize >= 40:      # WAVE_FORMAT_EXTENSIBLE
                    tag = struct.unpack("<H", fmt[24:26])[0]
                info.update(fmt=tag, channels=nch, sr=sr, bits=bits)
            elif cid == b"data":
                data_size = csize
                f.seek(csize + (csize & 1), 1)
            else:
                f.seek(csize + (csize & 1), 1)
        if data_size and info["sr"] and info["channels"] and info["bits"]:
            bytes_per_s = info["sr"] * info["channels"] * (info["bits"] // 8)
            if bytes_per_s:
                info["duration_s"] = data_size / bytes_per_s
    return info


def singer_of(path: Path) -> str:
    for part in path.parts:
        low = part.lower()
        if low.startswith(("female", "male")):
            return low
    return "unknown"


def tier2_group(rel: str) -> str:
    """Anti-leakage group: BabySlakh stems+mix of the SAME track share it."""
    name = Path(rel).name
    if name.startswith("babyslakh_"):
        return "slakh:" + name.split("_")[1]          # TrackXXXXX
    if name.startswith("fsld_"):
        return "fsld:" + name.split("_")[1].split(".")[0]
    return "file:" + name.rsplit(".", 1)[0]


def tier2_split(group: str) -> str:
    p = int(hashlib.sha1(group.encode()).hexdigest(), 16) % 100
    return "train" if p < 70 else ("heldout" if p < 85 else "test")


def freesound_id(rel: str, source_url: str) -> str | None:
    name = Path(rel).name
    if name.startswith("fsld_"):
        return name.split("_")[1].split(".")[0]
    if "freesound.org/s/" in source_url:
        return source_url.rstrip("/").rsplit("/", 1)[-1]
    return None


# ------------------------------------------------------------------ main
def main() -> int:
    rows: list[dict] = []
    problems: list[str] = []

    # --- VocalSet ---
    voc_files = sorted(p for p in VOCALSET_DIR.rglob("*.wav")
                       if not p.name.startswith("._"))
    for p in voc_files:
        singer = singer_of(p)
        split = ("test" if singer in TEST_SINGERS
                 else "heldout" if singer in CALIB_SINGERS else "train")
        info = riff_info(p)
        rows.append({
            "path": str(p.relative_to(DATA_ROOT)), "domain": "vocal",
            "group": "singer:" + singer, "split": split,
            "sr": info["sr"], "channels": info["channels"], "bits": info["bits"],
            "duration_s": round(info["duration_s"], 3),
            "sha256": sha256_of(p),
            "license": "CC-BY-4.0", "license_ok": 1,
            "source": "VocalSet (zenodo.org/records/1193957)",
            "hf_dead": 1 if info["sr"] and info["sr"] <= 22050 else 0,
            "license_crosscheck": "n/a",
        })

    # --- tier2_train via its per-file MANIFEST ---
    fsl_meta = json.loads(FSL_METADATA.read_text()) if FSL_METADATA.exists() else {}
    perc_meta = json.loads(PERC_LICENSES.read_text()) if PERC_LICENSES.exists() else {}
    xcheck = {"checked": 0, "match": 0, "mismatch": 0, "no_record": 0}

    with open(TIER2_DIR / "MANIFEST.csv", newline="") as f:
        for r in csv.DictReader(f):
            p = TIER2_DIR / r["file"]
            if not p.exists():
                problems.append(f"MANIFEST references missing file: {r['file']}")
                continue
            lic = norm_license(r["license"])
            ok = lic in COMMERCIAL_OK
            if not ok:
                problems.append(f"NON-COMMERCIAL/unknown license {lic}: {r['file']}")

            xc = "n/a"
            fsid = freesound_id(r["file"], r.get("source_url", ""))
            if fsid:
                rec = fsl_meta.get(fsid) or perc_meta.get(fsid)
                if rec is None:
                    xcheck["no_record"] += 1
                    xc = "no-upstream-record"
                else:
                    up = norm_license(rec["license"])
                    xcheck["checked"] += 1
                    if up in COMMERCIAL_OK:
                        xcheck["match"] += 1
                        xc = f"upstream={up}:OK"
                    else:
                        xcheck["mismatch"] += 1
                        xc = f"upstream={up}:VIOLATION"
                        problems.append(
                            f"UPSTREAM license mismatch {r['file']}: manifest={lic} "
                            f"upstream={up} (freesound {fsid})")

            group = tier2_group(r["file"])
            info = riff_info(p)
            rows.append({
                "path": str(p.relative_to(DATA_ROOT)), "domain": r["domain"],
                "group": group, "split": tier2_split(group),
                "sr": info["sr"], "channels": info["channels"], "bits": info["bits"],
                "duration_s": round(info["duration_s"], 3),
                "sha256": sha256_of(p),
                "license": lic, "license_ok": 1 if ok else 0,
                "source": r.get("source_url", ""),
                "hf_dead": 1 if info["sr"] and info["sr"] <= 22050 else 0,
                "license_crosscheck": xc,
            })

    # --- anti-leakage: intra-corpus duplicates ---
    by_hash: dict[str, list[dict]] = {}
    for row in rows:
        by_hash.setdefault(row["sha256"], []).append(row)
    dup_same_split, dup_cross_split = [], []
    for h, group_rows in by_hash.items():
        if len(group_rows) > 1:
            splits = {g["split"] for g in group_rows}
            entry = f"{h[:12]}…: " + " | ".join(g["path"] for g in group_rows)
            (dup_cross_split if len(splits) > 1 else dup_same_split).append(entry)
    for e in dup_cross_split:
        problems.append(f"DUPLICATE ACROSS SPLITS (leakage): {e}")

    # --- anti-leakage: judge clips must be OUTSIDE the corpus ---
    judge_files = sorted(JUDGE_DIR.rglob("*.wav")) if JUDGE_DIR.exists() else []
    judge_overlap = []
    corpus_hashes = set(by_hash)
    for jp in judge_files:
        if sha256_of(jp) in corpus_hashes:
            judge_overlap.append(str(jp))
            problems.append(f"JUDGE CLIP inside corpus (leakage): {jp}")

    # --- write manifest ---
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    fields = ["path", "domain", "group", "split", "sr", "channels", "bits",
              "duration_s", "sha256", "license", "license_ok", "source",
              "hf_dead", "license_crosscheck"]
    with open(MANIFEST_OUT, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)

    # --- report ---
    def table(keyfun) -> dict:
        agg: dict = {}
        for row in rows:
            k = keyfun(row)
            a = agg.setdefault(k, {"n": 0, "dur": 0.0})
            a["n"] += 1
            a["dur"] += row["duration_s"]
        return dict(sorted(agg.items()))

    dom_split = table(lambda r: (r["domain"], r["split"]))
    lic_tab = table(lambda r: r["license"])
    sr_tab = table(lambda r: r["sr"])

    lines = ["# A1 — Corpus v2: audit report", ""]
    lines.append(f"Files: **{len(rows)}** · durata totale: "
                 f"**{sum(r['duration_s'] for r in rows)/3600.0:.2f} h** · "
                 f"root: `{DATA_ROOT}`")
    lines.append("")
    lines.append("## Domini × split (n file / minuti)")
    lines.append("")
    lines.append("| domain | split | n | min |")
    lines.append("|---|---|---|---|")
    for (dom, split), a in dom_split.items():
        lines.append(f"| {dom} | {split} | {a['n']} | {a['dur']/60.0:.1f} |")
    lines.append("")
    lines.append("## Licenze")
    lines.append("")
    lines.append("| license | n | commercial-ok |")
    lines.append("|---|---|---|")
    for lic, a in lic_tab.items():
        lines.append(f"| {lic} | {a['n']} | {'sì' if lic in COMMERCIAL_OK else '**NO**'} |")
    lines.append("")
    lines.append(f"Cross-check upstream (freesound): {xcheck['checked']} verificati, "
                 f"{xcheck['match']} match, **{xcheck['mismatch']} mismatch**, "
                 f"{xcheck['no_record']} senza record locale.")
    lines.append("")
    lines.append("## Sample rate")
    lines.append("")
    lines.append("| sr | n | nota |")
    lines.append("|---|---|---|")
    for sr, a in sr_tab.items():
        note = "**HF morto sopra Nyquist/2 — vedi design A4**" if sr <= 22050 else ""
        lines.append(f"| {sr} | {a['n']} | {note} |")
    lines.append("")
    lines.append("## Anti-leakage")
    lines.append("")
    lines.append(f"- Duplicati sha256 CROSS-split: **{len(dup_cross_split)}**"
                 + (" ← FAIL" if dup_cross_split else " ✅"))
    lines.append(f"- Duplicati sha256 same-split (warning): {len(dup_same_split)}")
    for e in dup_same_split[:10]:
        lines.append(f"    - {e}")
    lines.append(f"- Clip giudice ({len(judge_files)} wav in `{JUDGE_DIR.name}`) "
                 f"dentro il corpus: **{len(judge_overlap)}**"
                 + (" ← FAIL" if judge_overlap else " ✅"))
    lines.append("")
    lines.append("## Split policy (deterministica, zero RNG)")
    lines.append("")
    lines.append(f"- vocal: singer-disjoint. test = {TEST_SINGERS} (INVARIATO dal lab "
                 f"M6-M9), heldout-calibrazione = {CALIB_SINGERS}, train = restanti.")
    lines.append("- tier2: group-disjoint (stems+mix stesso track = stesso gruppo), "
                 "sha1(group) mod 100 → <70 train, <85 heldout, resto test.")
    lines.append("")
    if problems:
        lines.append("## ⚠️ VIOLAZIONI")
        lines.append("")
        for p in problems:
            lines.append(f"- {p}")
        lines.append("")
        lines.append("**ESITO: ROSSO** — risolvere prima di A4.")
    else:
        lines.append("**ESITO: VERDE** — nessuna violazione. Corpus certificato per A4.")
    REPORT_OUT.write_text("\n".join(lines) + "\n")

    print(f"manifest: {MANIFEST_OUT} ({len(rows)} righe)")
    print(f"report:   {REPORT_OUT}")
    print("ESITO:", "ROSSO" if problems else "VERDE",
          f"({len(problems)} violazioni)" if problems else "")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
