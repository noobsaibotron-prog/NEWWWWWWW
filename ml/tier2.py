"""Tier-2 dataset builder (M6): clean NON-VOCAL material + injection positives
+ HF hard negatives for Sibilance.

Folder layout (see ~/aieq_data/real_audio/tier2_train/README.txt):
    clean_drums/   clean_bass/   clean_synth/   clean_mix/   hf_negative/

Principles (M6 plan, agreed 2026-07-07):
  - POSITIVES BY INJECTION: every positive is manufactured from a clean frame
    with a controlled band boost/cut (perfect free labels — the proven M2
    method). No hand-labeled "problem audio" required.
  - hf_negative/ frames are EXPLICIT all-zero-label negatives, oversampled
    (hf_negative_repeat) so the loss actually feels them: this is the measured
    fix for the model's "HF energy => Sibilance" label defect.
  - LEAKAGE BAN: the 8 Ableton holdout clips and their sources/ parents must
    never be in these folders (enforced by provenance/manifest, not by code —
    this module just reads what is there).
  - Held-out split: every k-th file per folder (deterministic) is reserved for
    the tier2 held-out eval, never trained.

Sample-rate note: frames are analyzed at each file's native rate. 16 kHz
material (e.g. BabySlakh) carries no content above 8 kHz, so it is
automatically EXCLUDED from HF-dependent duties: hf_negative/ files below
32 kHz are skipped, and injection specs whose band needs content above
Nyquist*0.9 are not applied to such files.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from .dataset import (FEATURE_FNS, InjectionSpec, Sample, _sample_resonance_injection,
                      _scale_band_db, injections_for, load_wav_mono,
                      problem_freq_ranges, top_energy_frames)
from .features import AnalysisPipeline, db_frame_to_linear
from .model import NUM_PROBLEMS, problem_names

TIER2_FOLDERS = ("clean_drums", "clean_bass", "clean_synth", "clean_mix",
                 "hf_negative")
HELDOUT_EVERY = 5          # every 5th file per folder -> held-out
HF_MIN_SAMPLE_RATE = 32000.0   # below this there is no real HF content


def _frames_for_file(path: Path, frames_per_file: int
                     ) -> tuple[list[np.ndarray], float] | None:
    try:
        audio, sr = load_wav_mono(path)
    except Exception:
        return None
    if audio.size < 4096:
        return None
    frames = AnalysisPipeline(sr).analyze(audio)
    if not frames:
        return None
    # broadband-energy selection (same selector real_dataset uses)
    frames = top_energy_frames(frames, sr, 100.0, min(10000.0, sr * 0.45),
                               frames_per_file)
    return frames, sr


def _injectable(inj: InjectionSpec, sr: float) -> bool:
    """An injection is honest only if its band is inside the file's bandwidth."""
    return inj.hi_hz <= sr * 0.45


def _injected_sample(frame_db: np.ndarray, sr: float, rng: np.random.Generator,
                     inj: InjectionSpec, feat, ranges, names: tuple[str, ...],
                     source: str) -> Sample | None:
    if inj.problem == 0:
        sampled = _sample_resonance_injection(frame_db, sr, rng, inj)
        if sampled is None:
            return None
        pos_db = sampled.frame_db
        target = sampled.target_freq
    else:
        delta = float(rng.uniform(*inj.boost_db_range))
        target = inj.target_freq(rng)
        sign = -1.0 if inj.subtractive else 1.0
        pos_db = _scale_band_db(frame_db, sr, inj.lo_hz, inj.hi_hz,
                                sign * delta)

    lo, hi = ranges[inj.problem]
    pt = np.zeros(NUM_PROBLEMS)
    pt[inj.problem] = 1.0
    ft = np.zeros(NUM_PROBLEMS)
    ft[inj.problem] = np.clip(np.log(max(target, lo) / lo)
                              / np.log(hi / lo), 0.0, 1.0)
    return Sample(feat(db_frame_to_linear(pos_db), sr), pt, ft,
                  f"tier2+{names[inj.problem]}:{source}")


def tier2_dataset(root: Path, feature_version: int,
                  frames_per_file: int = 3,
                  hf_negative_repeat: int = 3,
                  thinness_tonal_only: bool = False,
                  thinness_bass_repeat: int = 0,
                  seed: int = 22,
                  schema: str = "legacy-v1"
                  ) -> tuple[list[Sample], list[Sample]]:
    """Returns (train, heldout). Deterministic for a fixed seed + folder state."""
    feat = FEATURE_FNS[feature_version]
    rng = np.random.default_rng(seed)
    specs = injections_for(schema)
    ranges = problem_freq_ranges(schema)
    names = problem_names(schema)

    train: list[Sample] = []
    heldout: list[Sample] = []

    for folder in TIER2_FOLDERS:
        d = root / folder
        if not d.is_dir():
            continue
        files = sorted(p for p in d.rglob("*.wav") if p.is_file())
        for i, path in enumerate(files):
            got = _frames_for_file(path, frames_per_file)
            if got is None:
                continue
            frames, sr = got
            dest = heldout if (i % HELDOUT_EVERY == HELDOUT_EVERY - 1) else train
            is_hf_negative = (folder == "hf_negative")

            if is_hf_negative and sr < HF_MIN_SAMPLE_RATE:
                continue  # no real HF content -> useless as an HF negative

            for frame_db in frames:
                x_raw = feat(db_frame_to_linear(frame_db), sr)

                if is_hf_negative:
                    # THE Sibilance hard negative: HF-rich, label all-zero.
                    # Oversampled so ~10k loss cells actually move the class.
                    reps = max(1, hf_negative_repeat) if dest is train else 1
                    for _ in range(reps):
                        dest.append(Sample(x_raw, np.zeros(NUM_PROBLEMS),
                                           np.zeros(NUM_PROBLEMS),
                                           f"tier2-hfneg:{path.name}"))
                    continue

                # clean_* folders: one raw negative + one injected positive
                dest.append(Sample(x_raw, np.zeros(NUM_PROBLEMS),
                                   np.zeros(NUM_PROBLEMS),
                                   f"tier2-clean:{folder}/{path.name}"))

                thin_specs = [s for s in specs if s.problem == 5
                              and _injectable(s, sr)]
                if folder == "clean_bass" and thin_specs:
                    for _ in range(max(0, thinness_bass_repeat)):
                        injected = _injected_sample(
                            frame_db, sr, rng, thin_specs[0], feat, ranges,
                            names, f"{folder}/{path.name}")
                        if injected is not None:
                            dest.append(injected)

                candidates = [s for s in specs if _injectable(s, sr)]
                if thinness_tonal_only and folder == "clean_drums":
                    candidates = [s for s in candidates if s.problem != 5]
                if not candidates:
                    continue
                inj = candidates[int(rng.integers(0, len(candidates)))]

                injected = _injected_sample(frame_db, sr, rng, inj, feat,
                                            ranges, names,
                                            f"{folder}/{path.name}")
                if injected is not None:
                    dest.append(injected)

    return train, heldout
