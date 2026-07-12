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
from dataclasses import dataclass
from pathlib import Path

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
def manifest_rows(split: str, manifest: Path = MANIFEST) -> list[dict]:
    rows, seen = [], set()
    with open(manifest, newline="") as f:
        for r in csv.DictReader(f):
            if r["split"] != split:
                continue
            if r["sha256"] in seen:                  # dataloader dedup (A1c)
                continue
            seen.add(r["sha256"])
            rows.append(r)
    return rows


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
            out.append(_neg(fm(coloured), f"{source}+colored"))
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
            name = Path(r["path"]).name
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
            name = Path(r["path"]).name
            chosen = top_energy_windows(wins, sr, 100.0, min(10000.0, sr * 0.45),
                                        cfg.tier2_windows_per_file)
            for w in chosen:
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
    return out


def to_arrays(samples: list[WindowSample]
              ) -> tuple[np.ndarray, np.ndarray, list[str]]:
    X = np.stack([s.mel for s in samples]).astype(np.float32)
    Y = np.stack([s.target for s in samples]).astype(np.float32)
    return X, Y, [s.source for s in samples]


def build_or_load(cfg: BuildConfig, cache_dir: Path,
                  data_root: Path = DATA_ROOT_DEFAULT, log=print
                  ) -> tuple[np.ndarray, np.ndarray, list[str]]:
    cache_dir.mkdir(parents=True, exist_ok=True)
    cache = cache_dir / f"windows_{cfg.split}_{cfg.key()}.npz"
    if cache.exists():
        z = np.load(cache, allow_pickle=True)
        log(f"  [cache] {cache.name}: X{z['X'].shape}")
        return z["X"], z["Y"], list(z["sources"])
    log(f"  building {cfg.split} windows (seed {cfg.seed})…")
    samples = build_windows(cfg, data_root=data_root, log=log)
    X, Y, sources = to_arrays(samples)
    np.savez_compressed(cache, X=X, Y=Y, sources=np.array(sources, dtype=object))
    log(f"  built X{X.shape} -> {cache.name}")
    return X, Y, sources
