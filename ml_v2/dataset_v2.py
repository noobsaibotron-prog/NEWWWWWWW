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
            if not r.get("sha256", "").strip():
                raise ValueError(
                    f"{manifest.name}:{lineno}: empty sha256 for {r.get('path')}")
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
SPLIT_ROLES_PATH = HERE / "data" / "a4b_split_v1.json"
CANONICAL_ROLES = {"train", "calibration", "metric", "g3-external"}
ROLE_TO_SPLIT = {"train": "train", "calibration": "heldout",
                 "metric": "heldout", "g3-external": "test"}


def load_split_contract(path: Path = SPLIT_ROLES_PATH
                        ) -> tuple[dict[str, str], set[str] | None]:
    """FAIL-CLOSED contract load. Returns (roles, legacy_groups); legacy is
    None only when the contract file does not exist at all (pure pre-contract
    mode). Unknown schema or non-canonical role values (e.g. the retired
    'train-expansion' spelling) ABORT."""
    if not path.exists():
        return {}, None
    data = json.loads(path.read_text())
    if data.get("schema") != "a4b-split-v1":
        raise ValueError(f"{path.name}: unknown schema {data.get('schema')!r}")
    roles = data.get("roles", {})
    if not isinstance(roles, dict):
        raise ValueError(f"{path.name}: 'roles' must be a mapping")
    for g, r in roles.items():
        if r not in CANONICAL_ROLES:
            raise ValueError(
                f"{path.name}: unknown role {r!r} for group {g!r} "
                f"(canonical: {sorted(CANONICAL_ROLES)})")
    legacy = data.get("legacy_groups", [])
    if not isinstance(legacy, list):
        raise ValueError(f"{path.name}: 'legacy_groups' must be a list")
    return roles, set(legacy)


def validate_split_contract(manifest: Path = MANIFEST,
                            contract_path: Path = SPLIT_ROLES_PATH) -> None:
    """Gate check (corpus integration): every manifest group is either legacy
    or role-assigned, and role<->split are coherent (train->train,
    calibration/metric->heldout, g3-external->test)."""
    roles, legacy = load_split_contract(contract_path)
    if legacy is None:
        raise FileNotFoundError(f"split contract missing: {contract_path}")
    with open(manifest, newline="") as f:
        for lineno, r in enumerate(csv.DictReader(f), start=2):
            g = r["group"].strip()
            role = roles.get(g)
            if role is None:
                if g not in legacy:
                    raise ValueError(
                        f"{manifest.name}:{lineno}: group {g!r} has no contract "
                        f"role and is not in the frozen legacy baseline")
                continue
            want = ROLE_TO_SPLIT[role]
            if r["split"].strip() != want:
                raise ValueError(
                    f"{manifest.name}:{lineno}: group {g!r} role {role!r} "
                    f"requires split={want!r}, found {r['split']!r}")


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
                f"a4b_split_v1.json)")
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
                got = li.inject_window_db(w, sr, rng, inj)
                if got is not None:
                    boosted, target = got
                    out.append(WindowSample(fm(boosted),
                                            _make_target(inj.problem, target, ranges),
                                            f"vox+{li.PROBLEM_NAMES_V2[inj.problem]}:{name}"))
            for w in top_energy_windows(wins, sr, 5000.0, 9000.0, cfg.sib_focus):
                got = li.inject_window_db(w, sr, rng, specs[3])
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
                    got = li.inject_window_db(w, sr, rng, inj)
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
                    got = li.inject_window_db(w0, sr, rng, spec_p)
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
                  data_root: Path = DATA_ROOT_DEFAULT, log=print
                  ) -> tuple[np.ndarray, np.ndarray, list[str]]:
    cache_dir.mkdir(parents=True, exist_ok=True)
    cache = cache_path(cfg, cache_dir)
    if cache.exists():
        got = _load_cache(cache, log)
        if got is not None:
            log(f"  [cache] {cache.name}: X{got[0].shape}")
            return got
    log(f"  building {cfg.split} windows (seed {cfg.seed})…")
    samples = build_windows(cfg, data_root=data_root, log=log)
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
