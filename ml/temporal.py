"""M8 temporal lab (feature v5): WINDOW-level features + dataset builders.

WHY (measured, Round 1-5): the three residual benchmark failures are TEMPORAL
signatures that no per-frame feature can express —
  02 kick boom : clean drum loops and boomy kicks have near-identical per-frame
                 low-end; the difference is that boom SUSTAINS between hits
                 while a clean kick's low-end decays fast.
  08 resonance : a ringing tom keeps ONE narrow band high across frames while
                 the rest of the spectrum moves; per-frame prominence alone
                 also matches static formants/synth partials.
  01 clean     : the per-frame ambiguity above is exactly what leaks Boom/Thin
                 onto the clean loop.

feature v5 (dim 64+10): features_v1 of the window's CENTER frame + 10 window
descriptors (sustain, level-invariant band excess, spectral flux, narrow-band
persistence). Injections are applied to EVERY frame of a window, so injected
positives carry the sustained/persistent signature by construction, while raw
windows of the same files are the transient negatives.

Lab-only: none of this touches feature v1-v4, the legacy builders, or any
product path. Blob v3 stores feature_version=5; the benchmark/eval tools
dispatch on it.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from .dataset import (INJECTIONS, InjectionSpec, Sample,
                      _sample_resonance_injection, _scale_band_db,
                      _synthetic_spectrum, injections_for, load_wav_mono,
                      problem_freq_ranges, singer_of, TEST_SINGERS)
from .features import (AnalysisPipeline, db_frame_to_linear, extract_mel_bands,
                       features_v1, hz_to_mel, mel_to_hz, MEL_NUM_BANDS)
from .model import NUM_PROBLEMS, problem_names

V5_WINDOW = 12          # frames per window (~0.35 s at the pipeline hop)
V5_STRIDE = 4           # sliding stride for eval / window harvesting
V5_EXTRA = 10           # temporal descriptor count
V5_DIM = 64 + V5_EXTRA

_PERS_RES_DB = 6.0      # residual level that counts as "peak present"
_PERS_LO_HZ, _PERS_HI_HZ = 100.0, 5000.0

# ---- M9.3 contrastive pair generator constants ----
# temporal_descriptors layout: index 7 = persFrac (narrow-peak persistence
# fraction), index 8 = persRes (mean residual /10)
_D_PERSFRAC, _D_PERSRES = 7, 8
RING_MAX_ATTEMPTS = 6            # measured post-check resampling budget
RING_DECAY_RANGE = (0.35, 0.65)  # ring decays to 35-65% of initial boost

# Axis -> problem indices (product-v2 schema): 0 Resonance, 1 Harshness,
# 2 Muddiness, 3 Sibilance, 4 Boominess, 5 Thinness, 6 BoxyMidrange,
# 7 DullSound. Targets from the M8 verdict's confusion axes.
CONTRASTIVE_AXIS_PROBLEMS = {
    "clean_drums": (6, 2, 4, 7),   # Boxy, Mud, Boom, Dull  (A1: 01 precision)
    "clean_synth": (1, 5),         # Harsh, Thin            (A4: 07 dominance)
    "clean_bass":  (2, 4, 5),      # Mud, Boom, Thin        (low-end support)
    "clean_mix":   (6, 2),         # Boxy, Mud              (mix-level support)
}
CONTRASTIVE_RING_FOLDERS = ("clean_drums",)   # A3: ring-vs-transient pairs


def _band_series(win_db: np.ndarray, sr: float, lo: float, hi: float) -> np.ndarray:
    """Mean dB in [lo,hi] per frame -> [W]."""
    n_bins = win_db.shape[1]
    fft_size = (n_bins - 1) * 2
    bin_hz = sr / fft_size
    a = max(1, int(lo / bin_hz))
    b = min(n_bins - 1, int(hi / bin_hz))
    if b <= a:
        return np.full(win_db.shape[0], -100.0)
    return win_db[:, a:b + 1].mean(axis=1)


def _mel_residuals(win_db: np.ndarray, sr: float) -> np.ndarray:
    """Detrended mel residual per frame -> [W, MEL_NUM_BANDS] (v3-style)."""
    out = np.empty((win_db.shape[0], MEL_NUM_BANDS))
    idx = np.arange(MEL_NUM_BANDS, dtype=np.float64)
    idx_c = idx - idx.mean()
    denom = float(idx_c @ idx_c)
    for t in range(win_db.shape[0]):
        band_db = extract_mel_bands(db_frame_to_linear(win_db[t]), sr)
        mean_db = float(band_db.mean())
        slope = float((idx_c @ (band_db - mean_db)) / denom)
        out[t] = band_db - (mean_db + slope * idx_c)
    return out


def _mel_centers(sr: float) -> np.ndarray:
    lo, hi = hz_to_mel(20.0), hz_to_mel(min(20000.0, sr * 0.5))
    step = (hi - lo) / (MEL_NUM_BANDS + 1)
    return np.array([mel_to_hz(lo + (i + 1) * step) for i in range(MEL_NUM_BANDS)])


def temporal_descriptors(win_db: np.ndarray, sr: float) -> np.ndarray:
    """[W, bins] dB window -> [V5_EXTRA] descriptors (all ~O(1) scaled)."""
    broad = _band_series(win_db, sr, 100.0, min(10000.0, sr * 0.45))
    boom = _band_series(win_db, sr, 40.0, 150.0)
    mud = _band_series(win_db, sr, 150.0, 400.0)
    hf = _band_series(win_db, sr, 5000.0, min(12000.0, sr * 0.45))

    def rel(series: np.ndarray) -> float:
        return float((series - broad).mean()) / 10.0

    def sustain(series: np.ndarray) -> float:
        # 0 = perfectly sustained (mean==peak); more negative = transient hits
        return float(series.mean() - series.max()) / 10.0

    flux = float(np.abs(np.diff(win_db, axis=0)).mean()) / 10.0

    res = _mel_residuals(win_db, sr)                       # [W, bands]
    centers = _mel_centers(sr)
    zone = (centers >= _PERS_LO_HZ) & (centers <= _PERS_HI_HZ)
    pers = (res[:, zone] > _PERS_RES_DB).mean(axis=0)      # fraction per band
    mean_res = res[:, zone].mean(axis=0)
    score = pers * np.maximum(mean_res, 0.0)
    b = int(np.argmax(score)) if score.size else 0
    zone_centers = centers[zone]
    pos = (np.log(zone_centers[b] / _PERS_LO_HZ)
           / np.log(_PERS_HI_HZ / _PERS_LO_HZ)) if score.size else 0.0

    return np.array([
        rel(boom), sustain(boom),
        rel(mud), sustain(mud),
        rel(hf), sustain(hf),
        flux,
        float(pers[b]) if score.size else 0.0,   # narrow-peak persistence
        float(mean_res[b]) / 10.0 if score.size else 0.0,
        float(pos),
    ])


def features_v5(win_db: np.ndarray, sr: float) -> np.ndarray:
    center = win_db[win_db.shape[0] // 2]
    return np.concatenate([features_v1(db_frame_to_linear(center), sr),
                           temporal_descriptors(win_db, sr)])


def windows_of(frames: list[np.ndarray], window: int = V5_WINDOW,
               stride: int = V5_STRIDE) -> list[np.ndarray]:
    if len(frames) < window:
        return []
    arr = np.stack(frames)
    return [arr[s:s + window] for s in range(0, len(frames) - window + 1, stride)]


def _top_energy_windows(wins: list[np.ndarray], sr: float, lo: float, hi: float,
                        k: int) -> list[np.ndarray]:
    scored = sorted(wins, key=lambda w: float(_band_series(w, sr, lo, hi).mean()),
                    reverse=True)
    return scored[:k]


def _labelled(win_feat: np.ndarray, problem: int, target: float,
              ranges, source: str) -> Sample:
    pt = np.zeros(NUM_PROBLEMS); pt[problem] = 1.0
    ft = np.zeros(NUM_PROBLEMS)
    lo, hi = ranges[problem]
    ft[problem] = np.clip(np.log(max(target, lo) / lo) / np.log(hi / lo), 0, 1)
    return Sample(win_feat, pt, ft, source)


def _ring_window(win_db: np.ndarray, sr: float, rng: np.random.Generator,
                 ranges, source: str, spec: InjectionSpec | None = None,
                 ) -> Sample | None:
    """Resonance RING pair: narrow-band boost that decays across the window.

    A real tom ring keeps ONE narrow band elevated while it slowly decays;
    uniform-boost injection teaches persistence but not decay. This injection
    samples a resonance target on the center frame (same guardrails as
    _sample_resonance_injection), then applies a per-frame delta that decays
    from delta_start to delta_end across the window.

    Measured post-check (same philosophy as _sample_resonance_injection):
    the v5 persistence descriptors track the ARGMAX band in the 100-5000 Hz
    zone; on dense drum loops an already-persistent band (e.g. the kick
    fundamental) can win the argmax in both raw and ring, in which case the
    pair carries no descriptor-level signal. We recompute the descriptors
    after injection and only keep a ring whose persistence measurably
    dominates the raw window, resampling up to RING_MAX_ATTEMPTS times.
    """
    if spec is None:
        spec = injections_for("product-v2")[0]
    assert spec.problem == 0
    mid = win_db[win_db.shape[0] // 2]
    w = win_db.shape[0]
    raw_desc = temporal_descriptors(win_db, sr)
    for _ in range(RING_MAX_ATTEMPTS):
        sampled = _sample_resonance_injection(mid, sr, rng, spec)
        if sampled is None:
            return None
        target = sampled.target_freq
        delta_start = float(sampled.boost_db)
        delta_end = delta_start * float(rng.uniform(*RING_DECAY_RANGE))
        deltas = np.linspace(delta_start, delta_end, w)
        boosted = np.stack([
            _scale_band_db(win_db[t], sr, target * 0.94, target * 1.06,
                           float(deltas[t]), edge_octaves=0.15)
            for t in range(w)])
        ring_desc = temporal_descriptors(boosted, sr)
        if (ring_desc[_D_PERSFRAC] >= raw_desc[_D_PERSFRAC]
                and ring_desc[_D_PERSRES] > raw_desc[_D_PERSRES]):
            return _labelled(features_v5(boosted, sr), 0, target, ranges,
                             f"t5+RingDecay:{source}")
    return None


def _inject_window(win_db: np.ndarray, sr: float, rng: np.random.Generator,
                   inj: InjectionSpec, ranges, names,
                   source: str) -> Sample | None:
    """Apply ONE injection to EVERY frame of the window (persistent signature)."""
    if inj.problem == 0:
        mid = win_db[win_db.shape[0] // 2]
        sampled = _sample_resonance_injection(mid, sr, rng, inj)
        if sampled is None:
            return None
        target = sampled.target_freq
        boosted = np.stack([
            _scale_band_db(f, sr, target * 0.94, target * 1.06, sampled.boost_db,
                           edge_octaves=0.15) for f in win_db])
        return _labelled(features_v5(boosted, sr), 0, target, ranges,
                         f"t5+Resonance:{source}")
    delta = float(rng.uniform(*inj.boost_db_range))
    target = inj.target_freq(rng)
    sign = -1.0 if inj.subtractive else 1.0
    boosted = np.stack([_scale_band_db(f, sr, inj.lo_hz, inj.hi_hz, sign * delta)
                        for f in win_db])
    return _labelled(features_v5(boosted, sr), inj.problem, target, ranges,
                     f"t5+{names[inj.problem]}:{source}")


def _neg(win_db: np.ndarray, sr: float, source: str) -> Sample:
    return Sample(features_v5(win_db, sr), np.zeros(NUM_PROBLEMS),
                  np.zeros(NUM_PROBLEMS), source)


# ------------------------------------------------------------------ builders
def temporal_synthetic(samples_per_problem: int, seed: int,
                       schema: str = "product-v2",
                       negative_multiplier: float = 3.0) -> list[Sample]:
    """Synthetic WINDOWS: the same spectrum with small per-frame jitter
    (problems persist by construction); clean windows likewise."""
    rng = np.random.default_rng(seed)
    ranges = problem_freq_ranges(schema)
    out: list[Sample] = []
    sr, fft = 44100.0, 2048

    def spec_to_db(spec: np.ndarray) -> np.ndarray:
        return 20.0 * np.log10(np.maximum(spec, 1e-6))

    def jitter_window(base: np.ndarray) -> np.ndarray:
        return np.stack([spec_to_db(np.maximum(
            base * (1.0 + rng.normal(0.0, 0.05, base.shape)), 0.0))
            for _ in range(V5_WINDOW)])

    for p in range(NUM_PROBLEMS):
        lo, hi = ranges[p]
        for _ in range(samples_per_problem):
            target = lo * (hi / lo) ** rng.uniform()
            strength = 0.6 + rng.uniform() * 0.4
            base = _synthetic_spectrum(rng, p, sr, fft, target, strength,
                                       True, schema)
            win = jitter_window(base)
            out.append(_labelled(features_v5(win, sr), p, target, ranges,
                                 "t5-synth"))
    n_neg = int(samples_per_problem * negative_multiplier)
    for _ in range(n_neg):
        bins = fft // 2
        base = np.maximum(0.0, rng.uniform(0.03, 0.08)
                          + rng.normal(0.0, 0.02, bins))
        from .dataset import _apply_tilt, TILT_MIN_DB_PER_DECADE, TILT_MAX_DB_PER_DECADE
        _apply_tilt(base, sr, fft,
                    rng.uniform(TILT_MIN_DB_PER_DECADE, TILT_MAX_DB_PER_DECADE))
        out.append(_neg(jitter_window(base), sr, "t5-synth-clean"))
    return out


def temporal_real(vocalset_dir: Path, seed: int, schema: str = "product-v2",
                  clips_per_singer: int = 8, windows_per_clip: int = 2,
                  sib_focus: int = 1) -> tuple[list[Sample], list[Sample]]:
    rng = np.random.default_rng(seed)
    ranges = problem_freq_ranges(schema)
    names = problem_names(schema)
    specs = injections_for(schema)
    by: dict[str, list[Path]] = {}
    for p in sorted(vocalset_dir.rglob("*.wav")):
        s = singer_of(p)
        if s != "unknown":
            by.setdefault(s, []).append(p)
    train: list[Sample] = []
    heldout: list[Sample] = []
    for s in sorted(by):
        dest = heldout if s in TEST_SINGERS else train
        pick = [by[s][i] for i in rng.choice(
            len(by[s]), size=min(clips_per_singer, len(by[s])), replace=False)]
        for path in pick:
            try:
                audio, sr = load_wav_mono(path)
            except Exception:
                continue
            frames = AnalysisPipeline(sr).analyze(audio)
            wins = windows_of(frames)
            if not wins:
                continue
            chosen = _top_energy_windows(wins, sr, 100.0, 10000.0,
                                         windows_per_clip)
            for w in chosen:
                dest.append(_neg(w, sr, f"t5-vox-raw:{path.name}"))
                inj = specs[int(rng.integers(0, len(specs)))]
                got = _inject_window(w, sr, rng, inj, ranges, names, path.name)
                if got is not None:
                    dest.append(got)
            for w in _top_energy_windows(wins, sr, 5000.0, 9000.0, sib_focus):
                got = _inject_window(w, sr, rng, specs[3], ranges, names,
                                     f"sibfocus:{path.name}")
                if got is not None:
                    dest.append(got)
                deessed = np.stack([_scale_band_db(
                    f, sr, specs[3].lo_hz, specs[3].hi_hz,
                    -float(rng.uniform(2.0, 6.0))) for f in w])
                dest.append(_neg(deessed, sr, f"t5-deessed:{path.name}"))
                # M9.5 round 2: the de-essed window teaches "strong sib vs
                # attenuated sib" but NEVER "sib vs normal voice" — the model
                # never sees the RAW natural vocal frame as a Sibilance
                # negative, so it fires Sibilance on any voice (measured:
                # V-CLN raw >0.5 = 73%). Add the raw window as an explicit
                # all-zero negative on the same sib-focused frame.
                dest.append(_neg(w, sr, f"t5-vox-rawsib:{path.name}"))
    return train, heldout


def temporal_tier2(root: Path, seed: int, schema: str = "product-v2",
                   windows_per_file: int = 2, hf_negative_repeat: int = 3
                   ) -> tuple[list[Sample], list[Sample]]:
    from .tier2 import TIER2_FOLDERS, HELDOUT_EVERY, HF_MIN_SAMPLE_RATE
    rng = np.random.default_rng(seed)
    ranges = problem_freq_ranges(schema)
    names = problem_names(schema)
    specs = injections_for(schema)
    train: list[Sample] = []
    heldout: list[Sample] = []
    for folder in TIER2_FOLDERS:
        d = root / folder
        if not d.is_dir():
            continue
        files = sorted(p for p in d.rglob("*.wav") if p.is_file())
        for i, path in enumerate(files):
            try:
                audio, sr = load_wav_mono(path)
            except Exception:
                continue
            if audio.size < 4096:
                continue
            frames = AnalysisPipeline(sr).analyze(audio)
            wins = windows_of(frames)
            if not wins:
                continue
            dest = heldout if (i % HELDOUT_EVERY == HELDOUT_EVERY - 1) else train
            is_hf = (folder == "hf_negative")
            if is_hf and sr < HF_MIN_SAMPLE_RATE:
                continue
            chosen = _top_energy_windows(wins, sr, 100.0,
                                         min(10000.0, sr * 0.45),
                                         windows_per_file)
            for w in chosen:
                if is_hf:
                    reps = max(1, hf_negative_repeat) if dest is train else 1
                    neg = _neg(w, sr, f"t5-hfneg:{path.name}")
                    for _ in range(reps):
                        dest.append(neg)
                    continue
                dest.append(_neg(w, sr, f"t5-clean:{folder}/{path.name}"))
                usable = [s for s in specs if s.hi_hz <= sr * 0.45]
                if not usable:
                    continue
                inj = usable[int(rng.integers(0, len(usable)))]
                got = _inject_window(w, sr, rng, inj, ranges, names,
                                     f"{folder}/{path.name}")
                if got is not None:
                    dest.append(got)
    return train, heldout


def temporal_contrastive(root: Path, seed: int, schema: str = "product-v2",
                         windows_per_file: int = 3,
                         pair_repeat: int = 1,
                         vocalset_dir: Path | None = None,
                         vocal_windows_per_clip: int = 2,
                         clips_per_singer: int = 8,
                         ) -> tuple[list[Sample], list[Sample]]:
    """M9.3: targeted same-file contrastive pairs per confusion axis.

    From the M8 verdict, point 2 of the M9 spec: "Data: scale CONTRASTIVE
    WINDOW PAIRS per confusion axis (same-file pairs: raw vs injected, at
    5-10x current counts)".

    Key differences vs temporal_tier2:
      - injections are TARGETED per axis (CONTRASTIVE_AXIS_PROBLEMS), not one
        random spec per window
      - each chosen window emits the raw negative once and MULTIPLE injected
        positives (one per axis-relevant problem) -> same-file pairs at scale
      - RING injection for Resonance (asse 08): narrow-band boost that DECAYS
        across the window, teaching that a decaying ring is still Resonance
        while the raw percussive window is not
      - sr-aware: specs whose band exceeds sr*0.45 are skipped (as in tier2)
      - heldout: same "every 5th file" rule as tier2 (non-leaky by file)
      - AppleDouble files (._*) are excluded from the file COUNT so the
        heldout rule stays stable across macOS/rsync copies

    `pair_repeat` repeats the whole (raw + positives) block per window to
    scale counts without changing the pos/neg balance.
    """
    from .tier2 import HELDOUT_EVERY
    rng = np.random.default_rng(seed)
    ranges = problem_freq_ranges(schema)
    names = problem_names(schema)
    specs = {s.problem: s for s in injections_for(schema)}
    train: list[Sample] = []
    heldout: list[Sample] = []

    for folder, problems in CONTRASTIVE_AXIS_PROBLEMS.items():
        d = root / folder
        if not d.is_dir():
            continue
        files = sorted(p for p in d.rglob("*.wav")
                       if p.is_file() and not p.name.startswith("._"))
        for i, path in enumerate(files):
            try:
                audio, sr = load_wav_mono(path)
            except Exception:
                continue
            if audio.size < 4096:
                continue
            frames = AnalysisPipeline(sr).analyze(audio)
            wins = windows_of(frames)
            if not wins:
                continue
            dest = heldout if (i % HELDOUT_EVERY == HELDOUT_EVERY - 1) else train
            chosen = _top_energy_windows(wins, sr, 100.0,
                                         min(10000.0, sr * 0.45),
                                         windows_per_file)
            for w in chosen:
                for _ in range(max(1, pair_repeat)):
                    dest.append(_neg(w, sr, f"t5c-raw:{folder}/{path.name}"))
                    for prob in problems:
                        spec = specs.get(prob)
                        if spec is None or spec.hi_hz > sr * 0.45:
                            continue
                        got = _inject_window(w, sr, rng, spec, ranges, names,
                                             f"c:{folder}/{path.name}")
                        if got is not None:
                            dest.append(got)
                    if folder in CONTRASTIVE_RING_FOLDERS:
                        got = _ring_window(w, sr, rng, ranges,
                                           f"{folder}/{path.name}",
                                           spec=specs.get(0))
                        if got is not None:
                            dest.append(got)

    # M9.5 round 2: the VOCAL confusion axis (V-CLN/V-RES false Sibilance).
    # temporal_real's sib_focus never gave the model a RAW-natural vocal frame
    # as a Sibilance negative (only the de-essed one), so it learned "voice =
    # sibilance". Same-file pairs on sib-focused vocal windows: raw natural =
    # negative (Sibilance NOT present), Sibilance-injected = positive. Singer
    # split via TEST_SINGERS keeps it non-leaky (same rule as temporal_real).
    if vocalset_dir is not None and Path(vocalset_dir).is_dir():
        by: dict[str, list[Path]] = {}
        for p in sorted(Path(vocalset_dir).rglob("*.wav")):
            s = singer_of(p)
            if s != "unknown":
                by.setdefault(s, []).append(p)
        sib_spec = specs.get(3)
        for singer in sorted(by):
            dst = heldout if singer in TEST_SINGERS else train
            pick = [by[singer][k] for k in rng.choice(
                len(by[singer]),
                size=min(clips_per_singer, len(by[singer])), replace=False)]
            for path in pick:
                try:
                    audio, sr = load_wav_mono(path)
                except Exception:
                    continue
                if audio.size < 4096 or sib_spec is None or sib_spec.hi_hz > sr * 0.45:
                    continue
                wins = windows_of(AnalysisPipeline(sr).analyze(audio))
                if not wins:
                    continue
                for w in _top_energy_windows(wins, sr, 5000.0, 9000.0,
                                             vocal_windows_per_clip):
                    for _ in range(max(1, pair_repeat)):
                        dst.append(_neg(w, sr, f"t5cv-raw:{path.name}"))
                        got = _inject_window(w, sr, rng, sib_spec, ranges,
                                             names, f"cv:{path.name}")
                        if got is not None:
                            dst.append(got)
    return train, heldout
