"""Motore v2 — A4 window dataset builder.

Pipeline per window (Manus A4 spec §3, recipes reused from the M6-M9 lab via
ml_v2.lab_inject):

    wav -> rawDb frames [T, 2049]   (ml_v2.feature.frontend_raw_db_frames,
                                     the A2 parity-locked STFT)
        -> [32, 2049] windows (stride, top-energy selection)
        -> injection in the dB domain (lab recipes, ONE injection per window)
        -> log-mel [32, 64] via a VECTORIZED filterbank equivalent to
           ml_v2.feature.extract_mel_bands_from_db (validated at build time)
        -> targets [17] = [8 class | 1 presence | 8 freq(log-normalized)]

Recipes per domain (all deterministic given seed + manifest):
  - vocal:      ml/temporal.py temporal_real — per window: raw neg + random
                injected pos; sib-focused HF windows add: sib pos + de-essed
                neg + RAW natural window as explicit Sibilance negative (M9.5).
  - clean_*:    ml/tier2.py pairing — raw neg + injectable random pos; plus
                M9.3 contrastive axis pairs (CONTRASTIVE_AXIS_PROBLEMS) and
                ring-vs-transient pairs on clean_drums.
  - clean_synth: extra same-file harsh-vs-sibilance axis on HF-rich windows.
  - clean_drums: high-confidence real resonance positives from raw resonant
                percussive windows, never from Ableton judge clips.
  - coloured clean: mild broad EQ variants, all-zero labels, teaching
                colour != problem.
  - hf_negative: all-zero label, oversampled x3 in train (sr >= 32k only).

Split discipline: rows come from MANIFEST_V2.csv (A1-certified); sha256 dedup;
'train' -> training set, 'heldout' -> validation/calibration set. 'test' rows
are NEVER touched here (they feed the A6 benchmark only).
"""
from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote, unquote

import numpy as np

from . import lab_inject as li
from .feature import (FFT_SIZE, N_MELS, extract_mel_bands_from_db,
                      frontend_raw_db_frames)

T_WINDOW = 32
WINDOW_STRIDE = 16
HF_MIN_SAMPLE_RATE = 32000.0
NUM_OUT = 17

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "data" / "MANIFEST_V2.csv"
DATA_ROOT_DEFAULT = Path.home() / "aieq_data"


# ------------------------------------------------------------ fast mel path
class FastMel:
    """Vectorized twin of feature.extract_mel_bands_from_db for one sample
    rate: same integer band edges, same triangular weights, same count quirk
    (zero-weight bins included), same dB mapping. Validated by validate()."""

    def __init__(self, sample_rate: float, num_bins: int = FFT_SIZE // 2 + 1):
        self.sr = float(sample_rate)
        n_mels = N_MELS
        fft_size_quirk = num_bins * 2
        bin_hz = self.sr / fft_size_quirk
        min_mel = li.hz_to_mel(20.0)
        max_mel = li.hz_to_mel(min(20000.0, self.sr * 0.5))
        mel_step = (max_mel - min_mel) / float(n_mels + 1)
        W = np.zeros((n_mels, num_bins), dtype=np.float64)
        count = np.zeros(n_mels, dtype=np.float64)
        for band in range(n_mels):
            hz_low = li.mel_to_hz(min_mel + band * mel_step)
            hz_center = li.mel_to_hz(min_mel + (band + 1) * mel_step)
            hz_high = li.mel_to_hz(min_mel + (band + 2) * mel_step)
            b_lo = min(max(int(hz_low / bin_hz), 0), num_bins - 1)
            b_c = min(max(int(hz_center / bin_hz), 0), num_bins - 1)
            b_hi = min(max(int(hz_high / bin_hz), 0), num_bins - 1)
            for b in range(b_lo, b_hi + 1):
                w = 0.0
                if b < b_c and b_c > b_lo:
                    w = (b - b_lo) / (b_c - b_lo)
                elif b >= b_c and b_hi > b_c:
                    w = (b_hi - b) / (b_hi - b_c)
                W[band, b] = w
                count[band] += 1.0
        self.W = W
        self.count = np.maximum(count, 1.0)

    def __call__(self, win_db: np.ndarray) -> np.ndarray:
        """[W, 2049] dB -> [W, 64] normalized log-mel (float32)."""
        lin = np.power(10.0, np.asarray(win_db, dtype=np.float64) / 20.0)
        energy = (lin @ self.W.T) / self.count
        mel_db = np.maximum(-100.0, 20.0 * np.log10(energy + 1e-10))
        return np.clip(mel_db / -100.0, 0.0, 1.0).astype(np.float32)

    def validate(self, tol: float = 2e-6) -> float:
        """Compare against the reference f32 path on a deterministic frame."""
        rng = np.random.default_rng(7)
        frame = np.clip(rng.normal(-60.0, 15.0, FFT_SIZE // 2 + 1),
                        -120.0, 12.0).astype(np.float32)
        ref = extract_mel_bands_from_db(frame, self.sr)
        fast = self(frame[None, :])[0]
        delta = float(np.max(np.abs(ref.astype(np.float64) - fast)))
        if delta > tol:
            raise AssertionError(f"FastMel diverges from reference: {delta}")
        return delta


_FASTMEL_CACHE: dict[float, FastMel] = {}


def fast_mel_for(sr: float) -> FastMel:
    if sr not in _FASTMEL_CACHE:
        fm = FastMel(sr)
        fm.validate()
        _FASTMEL_CACHE[sr] = fm
    return _FASTMEL_CACHE[sr]


# ------------------------------------------------------------ wav + windows
def load_wav_mono(path: Path) -> tuple[np.ndarray, float]:
    from .eval_v2_benchmark import load_wav_mono as _load
    audio, sr = _load(path)
    return audio, float(sr)


def windows_from_audio(audio: np.ndarray, sr: float,
                       stride: int = WINDOW_STRIDE) -> list[np.ndarray]:
    frames = frontend_raw_db_frames(audio)          # [T, 2049] f32 dB
    if frames.shape[0] < T_WINDOW:
        return []
    return [frames[s:s + T_WINDOW].astype(np.float64)
            for s in range(0, frames.shape[0] - T_WINDOW + 1, stride)]


def top_energy_windows(wins: list[np.ndarray], sr: float, lo: float, hi: float,
                       k: int) -> list[np.ndarray]:
    scored = sorted(wins, key=lambda w: float(li.band_series(w, sr, lo, hi).mean()),
                    reverse=True)
    return scored[:k]


# ------------------------------------------------------------ sample record
@dataclass
class WindowSample:
    mel: np.ndarray          # [32, 64] float32
    target: np.ndarray       # [17] float32
    source: str


def _make_target(problem: int | None, target_hz: float | None,
                 ranges, sib_negative: bool = False) -> np.ndarray:
    t = np.zeros(NUM_OUT, dtype=np.float32)
    if problem is not None:
        t[problem] = 1.0
        t[8] = 1.0                                   # presence
        t[9 + problem] = li.freq_target(problem, float(target_hz), ranges)
    # sib_negative is label-time metadata only (explicit zero on Sibilance is
    # already the default); kept for source bookkeeping.
    return t


def _neg(mel: np.ndarray, source: str) -> WindowSample:
    return WindowSample(mel, np.zeros(NUM_OUT, dtype=np.float32), source)


# ------------------------------------------------------------ manifest rows
# A4b 0b: the loader is FAIL-CLOSED. A manifest row that reaches training with a
# missing/unknown license or no source-group must abort the build, never be
# silently skipped — silent skips are how an unlicensed file ends up in a
# commercial training set.
# "OWNED" = commercially licensed / owned material (the plan admits it), but it
# is only valid WITH a provenance ledger: upstream_license, attribution and
# crosscheck must all be filled in, else the row is rejected.
ALLOWED_LICENSES = {"CC0", "CC-BY", "CC-BY-4.0", "OWNED"}
OWNED_LEDGER_FIELDS = ("upstream_license", "attribution", "crosscheck")
SHA256_HEX = re.compile(r"[0-9a-fA-F]{64}\Z")


def canonical_sha256(value: str, context: str) -> str:
    """Validate and canonicalize a manifest content hash before any dedup gate.

    Treating a digest as an arbitrary non-empty string lets the same content use
    upper-case in one split and lower-case in another, bypassing leakage checks.
    Canonical lower-case keeps the manifest and deep verifier on one identity.
    """
    digest = value.strip()
    if not digest:
        raise ValueError(f"{context}: empty sha256")
    if not SHA256_HEX.fullmatch(digest):
        raise ValueError(f"{context}: sha256 must be exactly 64 hexadecimal characters")
    return digest.lower()


def manifest_rows(split: str, manifest: Path = MANIFEST) -> list[dict]:
    rows, seen = [], set()
    with open(manifest, newline="") as f:
        for lineno, r in enumerate(csv.DictReader(f), start=2):
            if r["split"] != split:
                continue
            if r.get("license_ok", "").strip() != "1":
                raise ValueError(
                    f"{manifest.name}:{lineno}: license_ok != 1 for {r.get('path')}")
            if r.get("license", "").strip() not in ALLOWED_LICENSES:
                raise ValueError(
                    f"{manifest.name}:{lineno}: license {r.get('license')!r} not in "
                    f"whitelist {sorted(ALLOWED_LICENSES)} for {r.get('path')}")
            if r.get("license", "").strip() == "OWNED":
                for fld in OWNED_LEDGER_FIELDS:
                    if not r.get(fld, "").strip() or r.get(fld, "").strip() == "n/a":
                        raise ValueError(
                            f"{manifest.name}:{lineno}: OWNED license requires a "
                            f"non-empty {fld} ledger entry for {r.get('path')}")
            if not r.get("group", "").strip():
                raise ValueError(
                    f"{manifest.name}:{lineno}: empty group for {r.get('path')}")
            r["sha256"] = canonical_sha256(
                r.get("sha256", ""),
                f"{manifest.name}:{lineno} ({r.get('path')})")
            if r["sha256"] in seen:                  # dataloader dedup (A1c)
                continue
            seen.add(r["sha256"])
            rows.append(r)
    return rows


def _gid(row: dict) -> str:
    """INJECTIVE, reversible encoding of the manifest group for embedding as
    the tail ':'-segment of a window source string (groups contain ':', the
    source separator; '/' is our gid/name separator). Percent-encoding with
    safe='' quotes ':', '/', '%', so distinct groups can never collide (a
    plain char substitution could: 'a:b' vs 'a=b') and unquote() recovers the
    raw group for role lookups. The trainer splits calibration vs metric BY
    THIS GID (A4b 0d: group-level isolation, not per-file)."""
    return quote(row["group"].strip(), safe="")


# A4b 0d: contract role assignments (group -> role) for NEW corpora. Roles are
# FIRST-CLASS: a group assigned 'calibration' or 'metric' by the committed
# contract file lands on exactly that side of the heldout split. The hash
# fallback is reserved for the FROZEN legacy_groups baseline (pre-contract
# manifest groups); a new group without a contract role ABORTS the build.
SPLIT_ROLES_PATH = HERE / "data" / "a4b_split_v2.json"
CANONICAL_ROLES = {"train", "calibration", "metric", "g3-external"}
ROLE_TO_SPLIT = {"train": "train", "calibration": "heldout",
                 "metric": "heldout", "g3-external": "test"}
SPLIT_CONTRACT_SCHEMA = "a4b-split-v2"
BATCH_ALLOCATION = "sha256-rank-v1"
SPLIT_ROLE_QUOTAS = {"train": 0.50, "calibration": 0.15,
                     "metric": 0.15, "g3-external": 0.20}
_SMALL_BATCH_ORDER = ("g3-external", "metric", "calibration")
_QUOTA_TIE_ORDER = ("train", "g3-external", "metric", "calibration")
# The first v2 document is a sealed migration point: it contains no admissions.
# A later document must be validated against its committed Git parent instead.
SPLIT_CONTRACT_V2_GENESIS_SHA256 = (
    "d37de0836b3568695bf5c1c78de6011057fa5c9fa0422e17a9f01a09acde0b6b")


def batch_group_ids_sha256(groups: list[str]) -> str:
    """Hash the canonical group list stored in one immutable admission batch."""
    payload = json.dumps(sorted(groups), ensure_ascii=False,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def assign_batch_roles(salt: str, batch_id: str,
                       groups: list[str]) -> dict[str, str]:
    """Derive the only permitted role assignment for an admission batch."""
    if not salt or not batch_id:
        raise ValueError("salt and batch_id must be non-empty")
    if not groups or any(not isinstance(g, str) or not g.strip() for g in groups):
        raise ValueError("groups must be non-empty strings")
    if len(set(groups)) != len(groups):
        raise ValueError("groups must be unique within an admission batch")

    ranked = sorted(
        groups,
        key=lambda group: hashlib.sha256(
            f"{salt}\0{batch_id}\0{group}".encode("utf-8")).hexdigest())
    if len(ranked) < 4:
        return {group: _SMALL_BATCH_ORDER[i] for i, group in enumerate(ranked)}

    counts = {role: 1 for role in CANONICAL_ROLES}
    while sum(counts.values()) < len(ranked):
        role = max(
            CANONICAL_ROLES,
            key=lambda candidate: (
                SPLIT_ROLE_QUOTAS[candidate] * len(ranked) - counts[candidate],
                -_QUOTA_TIE_ORDER.index(candidate)))
        counts[role] += 1

    ordered_roles = []
    for role in ("train", "calibration", "metric", "g3-external"):
        ordered_roles.extend([role] * counts[role])
    return dict(zip(ranked, ordered_roles, strict=True))


def load_split_contract_document(path: Path = SPLIT_ROLES_PATH) -> dict:
    """Load and validate the full, immutable A4b split-contract document."""
    if not path.exists():
        raise FileNotFoundError(f"split contract missing: {path}")
    data = json.loads(path.read_text())
    if data.get("schema") != SPLIT_CONTRACT_SCHEMA:
        raise ValueError(f"{path.name}: unknown schema {data.get('schema')!r}")
    if not isinstance(data.get("salt"), str) or not data["salt"]:
        raise ValueError(f"{path.name}: missing non-empty 'salt'")

    roles = data.get("roles", {})
    if not isinstance(roles, dict):
        raise ValueError(f"{path.name}: 'roles' must be a mapping")
    for g, r in roles.items():
        if not isinstance(g, str) or not g.strip():
            raise ValueError(f"{path.name}: role assignment has an empty group")
        if r not in CANONICAL_ROLES:
            raise ValueError(
                f"{path.name}: unknown role {r!r} for group {g!r} "
                f"(canonical: {sorted(CANONICAL_ROLES)})")

    legacy = data.get("legacy_groups", [])
    if not isinstance(legacy, list):
        raise ValueError(f"{path.name}: 'legacy_groups' must be a list")
    legacy_set = set(legacy)
    if len(legacy_set) != len(legacy) or any(not isinstance(g, str) or not g.strip()
                                             for g in legacy):
        raise ValueError(f"{path.name}: legacy_groups must be unique non-empty strings")
    overlap = sorted(set(roles) & legacy_set)
    if overlap:
        raise ValueError(
            f"{path.name}: groups cannot be both role-assigned and frozen legacy: "
            f"{overlap[:3]}")

    metadata = data.get("group_metadata", {})
    batches = data.get("batches", {})
    if not isinstance(metadata, dict) or not isinstance(batches, dict):
        raise ValueError(f"{path.name}: group_metadata and batches must be mappings")
    if set(metadata) != set(roles):
        raise ValueError(
            f"{path.name}: role assignments and group_metadata must have identical keys")

    for batch_id, batch in batches.items():
        if not isinstance(batch_id, str) or not batch_id or not isinstance(batch, dict):
            raise ValueError(f"{path.name}: invalid admission batch {batch_id!r}")
        for field in ("source_id", "primary_domain"):
            if not isinstance(batch.get(field), str) or not batch[field].strip():
                raise ValueError(f"{path.name}: batch {batch_id!r} missing {field!r}")
        if batch.get("allocation") != BATCH_ALLOCATION:
            raise ValueError(
                f"{path.name}: batch {batch_id!r} must use {BATCH_ALLOCATION!r}")
        groups = batch.get("group_ids")
        if (not isinstance(groups, list) or not groups
                or any(not isinstance(g, str) or not g.strip() for g in groups)
                or len(set(groups)) != len(groups)):
            raise ValueError(f"{path.name}: batch {batch_id!r} has invalid group_ids")
        if batch.get("group_ids_sha256") != batch_group_ids_sha256(groups):
            raise ValueError(f"{path.name}: batch {batch_id!r} group_ids hash mismatch")
        expected_roles = assign_batch_roles(data["salt"], batch_id, groups)
        for group, expected_role in expected_roles.items():
            if roles.get(group) != expected_role:
                raise ValueError(
                    f"{path.name}: batch {batch_id!r} role for group {group!r} "
                    f"must be {expected_role!r} by {BATCH_ALLOCATION}")

    for group, meta in metadata.items():
        if not isinstance(meta, dict):
            raise ValueError(f"{path.name}: group {group!r} metadata must be a mapping")
        for field in ("primary_domain", "admission_batch", "source_id"):
            if not isinstance(meta.get(field), str) or not meta[field].strip():
                raise ValueError(f"{path.name}: group {group!r} metadata missing {field!r}")
        batch_id = meta["admission_batch"]
        batch = batches.get(batch_id)
        if batch is None or group not in batch["group_ids"]:
            raise ValueError(
                f"{path.name}: group {group!r} is not recorded by batch {batch_id!r}")
        if (meta["primary_domain"] != batch["primary_domain"]
                or meta["source_id"] != batch["source_id"]):
            raise ValueError(
                f"{path.name}: group {group!r} metadata disagrees with batch {batch_id!r}")

    for batch_id, batch in batches.items():
        recorded = {group for group, meta in metadata.items()
                    if meta["admission_batch"] == batch_id}
        if recorded != set(batch["group_ids"]):
            raise ValueError(
                f"{path.name}: batch {batch_id!r} group_ids disagree with metadata")
    return data


def load_split_contract(path: Path = SPLIT_ROLES_PATH
                        ) -> tuple[dict[str, str], set[str] | None]:
    """FAIL-CLOSED compatibility view returning (roles, frozen legacy groups)."""
    if not path.exists():
        return {}, None
    data = load_split_contract_document(path)
    return data["roles"], set(data["legacy_groups"])


def validate_split_contract_delta(previous_path: Path,
                                  candidate_path: Path) -> None:
    """Reject any mutation of an admitted group, batch, salt, or legacy baseline."""
    previous = load_split_contract_document(previous_path)
    candidate = load_split_contract_document(candidate_path)
    for field in ("salt", "legacy_groups"):
        if previous[field] != candidate[field]:
            raise ValueError(f"split-contract delta mutates immutable {field!r}")
    for group, role in previous["roles"].items():
        if candidate["roles"].get(group) != role:
            raise ValueError(f"split-contract delta reassigns or removes group {group!r}")
        if candidate["group_metadata"].get(group) != previous["group_metadata"][group]:
            raise ValueError(f"split-contract delta mutates metadata for group {group!r}")
    for batch_id, batch in previous["batches"].items():
        if candidate["batches"].get(batch_id) != batch:
            raise ValueError(f"split-contract delta mutates or removes batch {batch_id!r}")


def split_contract_sha256(path: Path = SPLIT_ROLES_PATH) -> str:
    """Stable provenance identifier for the exact split contract in force."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _validate_committed_contract_bytes(contract_path: Path, head_bytes: bytes,
                                       parent_bytes: bytes | None) -> None:
    """Ensure the checked-out contract is immutable relative to Git history.

    The internal batch rules prevent accidental reassignment, but are not enough
    on their own: a self-consistent rewrite of both an old batch and its roles
    would otherwise pass. The trainer therefore accepts only the exact HEAD
    file and validates every non-genesis revision against HEAD^.
    """
    current = contract_path.read_bytes()
    if current != head_bytes:
        raise ValueError(
            "split contract differs from committed HEAD; commit an admission "
            "only after the append-only preflight")
    if parent_bytes is None:
        if hashlib.sha256(current).hexdigest() != SPLIT_CONTRACT_V2_GENESIS_SHA256:
            raise ValueError(
                "split contract has no committed v2 parent and is not the sealed "
                "v2 genesis document")
        return

    with tempfile.NamedTemporaryFile("wb", suffix=".json", delete=False) as f:
        f.write(parent_bytes)
        parent_path = Path(f.name)
    try:
        validate_split_contract_delta(parent_path, contract_path)
    finally:
        parent_path.unlink(missing_ok=True)


def enforce_committed_split_contract(contract_path: Path = SPLIT_ROLES_PATH) -> None:
    """Fail closed unless the active contract is a committed append-only revision.

    This gate intentionally applies to the trainer rather than generic dataset
    helpers, which accept temporary fixtures in guard tests. It requires a Git
    worktree so a full model candidate can never be trained from an ad-hoc
    contract rewrite.
    """
    contract_path = contract_path.resolve()
    repo_hint = HERE.parent
    try:
        root = Path(subprocess.run(
            ["git", "-C", str(repo_hint), "rev-parse", "--show-toplevel"],
            check=True, text=True, capture_output=True).stdout.strip())
        rel = contract_path.relative_to(root).as_posix()
        head = subprocess.run(
            ["git", "-C", str(root), "show", f"HEAD:{rel}"],
            check=True, capture_output=True).stdout
        parent = subprocess.run(
            ["git", "-C", str(root), "show", f"HEAD^:{rel}"],
            check=False, capture_output=True).stdout
    except (OSError, subprocess.CalledProcessError, ValueError) as e:
        raise RuntimeError(
            "A4b training requires a committed split contract inside a Git worktree") from e
    _validate_committed_contract_bytes(contract_path, head, parent or None)


def validate_split_contract(manifest: Path = MANIFEST,
                            contract_path: Path = SPLIT_ROLES_PATH) -> None:
    """Gate check (corpus integration): every manifest group is either legacy
    or role-assigned, and role<->split are coherent (train->train,
    calibration/metric->heldout, g3-external->test)."""
    roles, legacy = load_split_contract(contract_path)
    if legacy is None:
        raise FileNotFoundError(f"split contract missing: {contract_path}")
    group_splits: dict[str, tuple[str, int]] = {}
    sha_provenance: dict[str, tuple[str, str, str, int]] = {}
    with open(manifest, newline="") as f:
        for lineno, r in enumerate(csv.DictReader(f), start=2):
            g = r["group"].strip()
            split = r["split"].strip()
            sha = canonical_sha256(r["sha256"], f"{manifest.name}:{lineno}")
            if not g:
                raise ValueError(f"{manifest.name}:{lineno}: empty group")
            if split not in {"train", "heldout", "test"}:
                raise ValueError(
                    f"{manifest.name}:{lineno}: unknown split {split!r}")
            role = roles.get(g)
            if role is None:
                if g not in legacy:
                    raise ValueError(
                        f"{manifest.name}:{lineno}: group {g!r} has no contract "
                        f"role and is not in the frozen legacy baseline")
                effective_role = f"legacy:{split}"
            else:
                effective_role = role
                want = ROLE_TO_SPLIT[role]
                if split != want:
                    raise ValueError(
                        f"{manifest.name}:{lineno}: group {g!r} role {role!r} "
                        f"requires split={want!r}, found {split!r}")

            old_split = group_splits.setdefault(g, (split, lineno))
            if old_split[0] != split:
                raise ValueError(
                    f"{manifest.name}:{lineno}: group {g!r} spans splits "
                    f"{old_split[0]!r} (line {old_split[1]}) and {split!r}")

            old_sha = sha_provenance.setdefault(
                sha, (g, split, effective_role, lineno))
            if old_sha[:3] != (g, split, effective_role):
                raise ValueError(
                    f"{manifest.name}:{lineno}: sha256 {sha!r} is reused across "
                    f"group/split/role boundaries; first seen for group {old_sha[0]!r}, "
                    f"split {old_sha[1]!r}, role {old_sha[2]!r} on line {old_sha[3]}")


def heldout_calib_metric_indices(sources: list[str],
                                 roles: dict[str, str] | None = None,
                                 legacy: set[str] | None = None
                                 ) -> tuple[np.ndarray, np.ndarray, dict]:
    """Split heldout windows by manifest source-GROUP (artist/pack/session),
    never by file or row (A4b 0d: files of the same artist must not straddle
    calibration and metric). Source tails are 'gid/name' (see _gid; gid is
    percent-encoded, unquote() recovers the raw group); legacy plain-name
    tails degrade to per-file grouping. Contract roles WIN over the hash
    fallback; a 'train' or 'g3-external' group found in heldout data is a
    contamination and aborts. The hash fallback is allowed ONLY for groups in
    the frozen legacy baseline — an un-roled NEW group aborts. Torch-free on
    purpose: the guard tests exercise this without the training stack."""
    if roles is None:
        roles, legacy = load_split_contract()
    groups: dict[str, list[int]] = {}
    for i, src in enumerate(sources):
        tail = src.rsplit(":", 1)[-1] if ":" in src else src
        group = tail.split("/", 1)[0]
        groups.setdefault(group, []).append(i)

    calib, metric = [], []
    role_forced = 0
    for group, idxs in sorted(groups.items()):
        raw = unquote(group)
        role = roles.get(raw)
        if role in ("train", "g3-external"):
            raise ValueError(
                f"contamination: group {raw!r} has contract role "
                f"{role!r} but appears in heldout data")
        if role == "calibration":
            calib.extend(idxs); role_forced += 1
            continue
        if role == "metric":
            metric.extend(idxs); role_forced += 1
            continue
        if legacy is not None and raw not in legacy:
            raise ValueError(
                f"new group {raw!r} appears in heldout data without a contract "
                f"role (legacy baseline is frozen; assign a role in "
                f"a4b_split_v2.json)")
        h = int(hashlib.sha1(group.encode("utf-8")).hexdigest()[:8], 16)
        (calib if (h % 100) < 50 else metric).extend(idxs)

    if not calib or not metric:
        calib, metric = [], []
        for n, group in enumerate(sorted(groups)):
            (calib if n % 2 == 0 else metric).extend(groups[group])

    if not calib or not metric:
        raise RuntimeError("heldout split needs at least two source groups")

    meta = {"heldout_source_groups": len(groups),
            "heldout_role_forced_groups": role_forced,
            "heldout_calib_windows": len(calib),
            "heldout_metric_windows": len(metric)}
    return (np.asarray(sorted(calib), dtype=np.int64),
            np.asarray(sorted(metric), dtype=np.int64),
            meta)


def verify_manifest_files(split: str, data_root: Path = DATA_ROOT_DEFAULT,
                          manifest: Path = MANIFEST, deep: bool = False,
                          log=print) -> None:
    """A4b 0b deep check: every manifest row's file exists; with deep=True the
    sha256 is re-hashed and must match. Heavy (reads every file) — run as an
    explicit gate step (new-corpus integration, F1), not on every build."""
    for r in manifest_rows(split, manifest):
        p = data_root / r["path"]
        if not p.is_file():
            raise FileNotFoundError(f"manifest row missing on disk: {p}")
        if deep:
            h = hashlib.sha256()
            with open(p, "rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    h.update(chunk)
            if h.hexdigest() != r["sha256"]:
                raise ValueError(f"sha256 mismatch for {p}")
    log(f"  verify_manifest_files({split!r}, deep={deep}): OK")


def _deep_verify_all_splits(manifest: Path, data_root: Path, log=print) -> dict:
    """Verify every admitted audio object, including the external-only split."""
    counts = {}
    for split in ("train", "heldout", "test"):
        verify_manifest_files(split, data_root=data_root, manifest=manifest,
                              deep=True, log=log)
        counts[split] = len(manifest_rows(split, manifest))
    return counts


def validate_admission_preflight(previous_contract: Path, candidate_contract: Path,
                                 manifest: Path = MANIFEST,
                                 data_root: Path = DATA_ROOT_DEFAULT,
                                 log=print) -> dict:
    """Heavy one-shot gate for a proposed corpus admission.

    The caller supplies the previously committed contract explicitly. This makes
    append-only validation, manifest/role validation, and byte-level audio hash
    verification one inseparable operation before a new batch is committed.
    """
    validate_split_contract_delta(previous_contract, candidate_contract)
    validate_split_contract(manifest, candidate_contract)
    counts = _deep_verify_all_splits(manifest, data_root, log=log)
    return {
        "candidate_contract_sha256": split_contract_sha256(candidate_contract),
        "manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
        "verified_rows": counts,
    }


def validate_training_preflight(manifest: Path = MANIFEST,
                                data_root: Path = DATA_ROOT_DEFAULT,
                                contract_path: Path = SPLIT_ROLES_PATH,
                                log=print) -> dict:
    """Mandatory deep gate immediately before any model training.

    Re-hashing the corpus is intentionally expensive. It is still required here:
    model candidates must not be produced from content that changed underneath a
    manifest or from an uncommitted rewrite of the split contract.
    """
    enforce_committed_split_contract(contract_path)
    validate_split_contract(manifest, contract_path)
    counts = _deep_verify_all_splits(manifest, data_root, log=log)
    return {
        "contract_sha256": split_contract_sha256(contract_path),
        "manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
        "verified_rows": counts,
    }


# ------------------------------------------------------------ builders
@dataclass
class BuildConfig:
    split: str = "train"
    seed: int = 42
    clips_per_singer: int = 24
    windows_per_clip: int = 2
    sib_focus: int = 1
    tier2_windows_per_file: int = 2
    hf_negative_repeat: int = 3
    contrastive_per_file: int = 1
    ring_per_file: int = 1
    colored_clean_every: int = 2      # one mild colour negative per N raw-clean windows
    synth_harsh_every: int = 4        # one synth harsh/sib triplet per N clean_synth files
    real_resonance_per_file: int = 10
    measured_boom_gate: bool = False  # F2b opt-in; CONTROL remains byte-identical
    max_vocal_files: int = 0          # 0 = no cap (cap applies AFTER per-singer pick)
    max_tier2_files_per_domain: int = 0

    def key(self) -> str:
        blob = json.dumps(self.__dict__, sort_keys=True).encode()
        return hashlib.sha1(blob).hexdigest()[:12]


def build_windows(cfg: BuildConfig, data_root: Path = DATA_ROOT_DEFAULT,
                  manifest: Path = MANIFEST, log=print) -> list[WindowSample]:
    rng = np.random.default_rng(cfg.seed)
    ranges = li.problem_freq_ranges("product-v2")
    specs = li.injections_for("product-v2")
    rows = manifest_rows(cfg.split, manifest)
    out: list[WindowSample] = []
    clean_seen = 0
    colored_clean = 0
    synth_seen = 0
    synth_hf_seen = 0
    synth_harsh_pairs = 0
    real_resonance = 0
    boom_gate_attempted = 0
    boom_gate_accepted = 0

    def inject(win_db: np.ndarray, sr: float, inj: li.InjectionSpec):
        nonlocal boom_gate_attempted, boom_gate_accepted
        got = li.inject_window_db(
            win_db, sr, rng, inj,
            measured_boom_gate=cfg.measured_boom_gate)
        if cfg.measured_boom_gate and inj.problem == 4:
            boom_gate_attempted += 1
            boom_gate_accepted += int(got is not None)
        return got

    def add_raw_clean(win_db: np.ndarray, sr: float, fm: FastMel,
                      source: str, hf_dead: bool = False) -> None:
        nonlocal clean_seen, colored_clean
        out.append(_neg(fm(win_db), source))
        clean_seen += 1
        if cfg.colored_clean_every <= 0:
            return
        if clean_seen % cfg.colored_clean_every != 0:
            return
        coloured = li.color_window_db(win_db, sr, rng, ranges, hf_dead=hf_dead)
        if coloured is not None:
            if ":" in source:
                prefix, name = source.rsplit(":", 1)
                coloured_source = f"{prefix}+colored:{name}"
            else:
                coloured_source = f"{source}+colored"
            out.append(_neg(fm(coloured), coloured_source))
            colored_clean += 1

    # ---------------- vocal (temporal_real recipe) ----------------
    by_singer: dict[str, list[dict]] = {}
    for r in rows:
        if r["domain"] == "vocal":
            by_singer.setdefault(r["group"].split(":", 1)[1], []).append(r)
    for singer in sorted(by_singer):
        files = sorted(by_singer[singer], key=lambda r: r["path"])
        pick_n = min(cfg.clips_per_singer, len(files))
        pick = [files[i] for i in rng.choice(len(files), size=pick_n,
                                             replace=False)]
        for r in pick:
            try:
                audio, sr = load_wav_mono(data_root / r["path"])
            except Exception:
                continue
            wins = windows_from_audio(audio, sr)
            if not wins:
                continue
            fm = fast_mel_for(sr)
            name = f"{_gid(r)}/{Path(r['path']).name}"
            for w in top_energy_windows(wins, sr, 100.0, 10000.0,
                                        cfg.windows_per_clip):
                add_raw_clean(w, sr, fm, f"vox-raw:{name}",
                              hf_dead=(r.get("hf_dead") == "1"))
                inj = specs[int(rng.integers(0, len(specs)))]
                got = inject(w, sr, inj)
                if got is not None:
                    boosted, target = got
                    out.append(WindowSample(fm(boosted),
                                            _make_target(inj.problem, target, ranges),
                                            f"vox+{li.PROBLEM_NAMES_V2[inj.problem]}:{name}"))
            for w in top_energy_windows(wins, sr, 5000.0, 9000.0, cfg.sib_focus):
                got = inject(w, sr, specs[3])
                if got is not None:
                    boosted, target = got
                    out.append(WindowSample(fm(boosted),
                                            _make_target(3, target, ranges),
                                            f"vox+sibfocus:{name}"))
                deessed = np.stack([li.scale_band_db(
                    f, sr, specs[3].lo_hz, specs[3].hi_hz,
                    -float(rng.uniform(2.0, 6.0))) for f in w])
                out.append(_neg(fm(deessed), f"vox-deessed:{name}"))
                out.append(_neg(fm(w), f"vox-rawsib:{name}"))   # M9.5 round 2
    n_vocal = len(out)
    log(f"  vocal windows: {n_vocal}")

    # ---------------- tier2 (tier2 pairing + M9.3 axes + ring) -----
    by_dom: dict[str, list[dict]] = {}
    for r in rows:
        if r["domain"] != "vocal":
            by_dom.setdefault(r["domain"], []).append(r)
    for dom in sorted(by_dom):
        files = sorted(by_dom[dom], key=lambda r: r["path"])
        if cfg.max_tier2_files_per_domain:
            files = files[: cfg.max_tier2_files_per_domain]
        is_hf = (dom == "hf_negative")
        axis = li.CONTRASTIVE_AXIS_PROBLEMS.get(dom, ())
        for r in files:
            sr_row = float(r["sr"] or 0)
            if is_hf and sr_row and sr_row < HF_MIN_SAMPLE_RATE:
                continue
            try:
                audio, sr = load_wav_mono(data_root / r["path"])
            except Exception:
                continue
            wins = windows_from_audio(audio, sr)
            if not wins:
                continue
            fm = fast_mel_for(sr)
            name = f"{_gid(r)}/{Path(r['path']).name}"
            chosen = top_energy_windows(wins, sr, 100.0, min(10000.0, sr * 0.45),
                                        cfg.tier2_windows_per_file)
            real_res_ids: set[int] = set()
            if dom == "clean_drums" and cfg.real_resonance_per_file > 0:
                candidates_res = top_energy_windows(
                    wins, sr, 120.0, min(5000.0, sr * 0.45),
                    cfg.real_resonance_per_file)
                for w_res in candidates_res:
                    detected = li.detect_real_resonance_window_db(w_res, sr)
                    if detected is None:
                        continue
                    target, _prom = detected
                    out.append(WindowSample(
                        fm(w_res), _make_target(0, target, ranges),
                        f"{dom}+realres:{name}"))
                    real_res_ids.add(id(w_res))
                    real_resonance += 1
            for w in chosen:
                if id(w) in real_res_ids:
                    continue
                mel_raw = fm(w)
                if is_hf:
                    reps = max(1, cfg.hf_negative_repeat) \
                        if cfg.split == "train" else 1
                    for _ in range(reps):
                        out.append(_neg(mel_raw, f"hfneg:{name}"))
                    continue
                add_raw_clean(w, sr, fm, f"{dom}-clean:{name}",
                              hf_dead=(r.get("hf_dead") == "1"))
                candidates = [s for s in specs if li.injectable(s, sr)]
                if candidates:
                    inj = candidates[int(rng.integers(0, len(candidates)))]
                    got = inject(w, sr, inj)
                    if got is not None:
                        boosted, target = got
                        out.append(WindowSample(
                            fm(boosted), _make_target(inj.problem, target, ranges),
                            f"{dom}+{li.PROBLEM_NAMES_V2[inj.problem]}:{name}"))
            if is_hf or not chosen:
                continue
            # M9.3 contrastive axis pairs on the top window
            w0 = chosen[0]
            if dom == "clean_synth":
                synth_seen += 1
                synth_axis = li.synth_harsh_sib_windows_db(w0, sr, rng)
                if synth_axis:
                    synth_hf_seen += 1
                if (cfg.synth_harsh_every > 0 and synth_axis
                        and (synth_hf_seen - 1) % cfg.synth_harsh_every == 0):
                    out.append(_neg(fm(w0), f"{dom}-synthaxisraw:{name}"))
                    for p, boosted, target in synth_axis:
                        out.append(WindowSample(
                            fm(boosted), _make_target(p, target, ranges),
                            f"{dom}+synthaxis{li.PROBLEM_NAMES_V2[p]}:{name}"))
                        synth_harsh_pairs += 1
            for _ in range(cfg.contrastive_per_file):
                for p in axis:
                    spec_p = next(s for s in specs if s.problem == p)
                    if not li.injectable(spec_p, sr):
                        continue
                    got = inject(w0, sr, spec_p)
                    if got is None:
                        continue
                    boosted, target = got
                    out.append(WindowSample(
                        fm(boosted), _make_target(p, target, ranges),
                        f"{dom}+axis{li.PROBLEM_NAMES_V2[p]}:{name}"))
                    out.append(_neg(fm(w0), f"{dom}-axisraw:{name}"))
            # M9.3 ring-vs-transient (clean_drums)
            if dom in li.CONTRASTIVE_RING_FOLDERS:
                for _ in range(cfg.ring_per_file):
                    got = li.ring_window_db(w0, sr, rng, specs[0])
                    if got is not None:
                        boosted, target = got
                        out.append(WindowSample(
                            fm(boosted), _make_target(0, target, ranges),
                            f"{dom}+ring:{name}"))
                        out.append(_neg(fm(w0), f"{dom}-ringraw:{name}"))
    log(f"  tier2 windows: {len(out) - n_vocal}")
    log(f"  colored-clean negatives: {colored_clean} / {clean_seen} raw-clean")
    log(f"  synth harsh/sib axis positives: {synth_harsh_pairs} "
        f"from {synth_hf_seen}/{synth_seen} clean_synth HF-rich files")
    log(f"  real resonance positives: {real_resonance}")
    if cfg.measured_boom_gate:
        log(f"  measured Boom gate: {boom_gate_accepted}/{boom_gate_attempted} "
            "injections accepted")
    return out


def to_arrays(samples: list[WindowSample]
              ) -> tuple[np.ndarray, np.ndarray, list[str]]:
    X = np.stack([s.mel for s in samples]).astype(np.float32)
    Y = np.stack([s.target for s in samples]).astype(np.float32)
    return X, Y, [s.source for s in samples]


# A4b 0a: the cache key must change whenever ANYTHING that shapes the built
# dataset changes — not just BuildConfig. Bug this fixes (proven): caches with
# the same BuildConfig key but different contents, because manifest/recipes had
# changed between builds. The fingerprint hashes the ACTUAL BYTES of the
# manifest and of every pipeline module, plus a schema version; a stale cache
# can then never be loaded as fresh (its filename simply no longer matches).
CACHE_SCHEMA = "a4b-cache-3"   # -3: gid percent-encoded (injective); -2: gid/name tails


def pipeline_fingerprint(manifest: Path = MANIFEST) -> str:
    h = hashlib.sha256()
    h.update(CACHE_SCHEMA.encode())
    for p in (manifest, Path(__file__), HERE / "lab_inject.py",
              HERE / "feature.py"):
        h.update(p.name.encode())
        h.update(p.read_bytes())
    return h.hexdigest()[:16]


def cache_path(cfg: BuildConfig, cache_dir: Path,
               manifest: Path = MANIFEST) -> Path:
    return cache_dir / (f"windows_{cfg.split}_{cfg.key()}"
                        f"_{pipeline_fingerprint(manifest)}.npz")


def _load_cache(cache: Path, log=print):
    """Load a cache npz; on ANY failure treat it as corrupt: delete and return
    None so the caller rebuilds (a half-written or truncated file must never
    poison a run)."""
    try:
        z = np.load(cache, allow_pickle=True)
        return z["X"], z["Y"], list(z["sources"])
    except Exception as e:
        log(f"  [cache] corrupt {cache.name} ({e.__class__.__name__}) — rebuilding")
        cache.unlink(missing_ok=True)
        return None


def build_or_load(cfg: BuildConfig, cache_dir: Path,
                  data_root: Path = DATA_ROOT_DEFAULT, log=print,
                  manifest: Path = MANIFEST,
                  contract_path: Path = SPLIT_ROLES_PATH
                  ) -> tuple[np.ndarray, np.ndarray, list[str]]:
    # This must precede the cache lookup. Otherwise a valid cache can mask an
    # invalid new-corpus assignment and let training continue unchecked.
    validate_split_contract(manifest, contract_path)
    cache_dir.mkdir(parents=True, exist_ok=True)
    cache = cache_path(cfg, cache_dir, manifest)
    if cache.exists():
        got = _load_cache(cache, log)
        if got is not None:
            log(f"  [cache] {cache.name}: X{got[0].shape}")
            return got
    log(f"  building {cfg.split} windows (seed {cfg.seed})…")
    samples = build_windows(cfg, data_root=data_root, manifest=manifest, log=log)
    X, Y, sources = to_arrays(samples)
    # atomic publish with a PER-PROCESS unique temp name: two concurrent
    # trainings building the same cache must not interleave writes into one
    # temp file (a fixed ".tmp" name would); last rename wins, both contents
    # are identical by construction (same fingerprinted inputs).
    fd, tmpname = tempfile.mkstemp(dir=cache_dir,
                                   prefix=cache.stem + ".", suffix=".tmp.npz")
    os.close(fd)
    np.savez_compressed(tmpname, X=X, Y=Y,
                        sources=np.array(sources, dtype=object))
    Path(tmpname).replace(cache)
    log(f"  built X{X.shape} -> {cache.name}")
    return X, Y, sources
