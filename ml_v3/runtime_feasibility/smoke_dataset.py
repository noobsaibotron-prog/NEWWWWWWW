"""Deterministic smoke dataset built from real frontend features. Lab-only.

The task is **not** the real problem. It is signal-type classification over
frames the real frontend produced from formula-rendered fixtures — multitone,
log sweep and pseudo-noise — which have genuinely different spectra, so the
task is learnable without being the Muddiness/Resonance problem V3 actually
has to solve. Two further fixtures were measured and excluded; see
:data:`SPARSE_CLASSES_EXCLUDED` for the counts and the reason.

Why real features rather than random tensors: a smoke test on noise proves the
tensors have the right shape, which is the least interesting thing that could
break. Real frames carry the true dynamic range, the true correlation between
the eight band vectors, the clamps and the floors — so a training loop that
silently depends on well-conditioned input fails here instead of failing later
on the corpus.

No WAV is read: :mod:`ml_v3.fixtures.g1.render_signals` renders from formula,
so the dataset is reproducible from source with no fixture on disk.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import torch

from ml_v3.contracts.constants import GRID_BANDS
from ml_v3.fixtures.g1.render_signals import (
    render_damped_resonance,
    render_log_sweep,
    render_multitone,
    render_pseudo_noise,
    render_transient_burst,
)
from ml_v3.frontend.feature_frame import (
    BAND_VECTOR_FIELDS,
    SCALAR_FLOAT_FIELDS,
)
from ml_v3.frontend.offline_features import extract_offline_feature_frames
from ml_v3.runtime_feasibility.envelope import FEATURES_PER_FRAME

__all__ = [
    "SMOKE_CLASSES",
    "SPARSE_CLASSES_EXCLUDED",
    "SmokeDataset",
    "SmokeDatasetError",
    "build_smoke_dataset",
    "frame_to_vector",
]

#: Lab-only classes. Renderers with visibly different spectra, so the smoke
#: task is learnable; this is not the V3 problem statement.
#:
#: ``transient_burst`` and ``damped_resonance`` are deliberately absent.
#: Measured on the 2 s fixtures at 48 kHz, valid frames per class are:
#: multitone 86/86, pseudo_noise 86/86, log_sweep 63/86, damped_resonance
#: 25/86, transient_burst 8/86. The two sparse ones are not a frontend defect
#: — a burst is mostly silence between hits and a damped resonance decays
#: below the validity floor, so the frontend is right to mark those frames
#: invalid. They simply cannot supply a 32-frame window, and padding them
#: with floor values would train the model on silence labelled as signal.
SMOKE_CLASSES: tuple[tuple[str, object], ...] = (
    ("multitone", render_multitone),
    ("log_sweep", render_log_sweep),
    ("pseudo_noise", render_pseudo_noise),
)

#: Kept importable so a future longer-fixture variant can reinstate them
#: without rediscovering why they were dropped.
SPARSE_CLASSES_EXCLUDED: tuple[tuple[str, object], ...] = (
    ("transient_burst", render_transient_burst),
    ("damped_resonance", render_damped_resonance),
)

_CANONICAL_RATE = 48_000


class SmokeDatasetError(RuntimeError):
    """The dataset could not be materialized."""


@dataclass(frozen=True)
class SmokeDataset:
    """Windows of real feature frames with a class label per window.

    ``features`` is ``[N, FEATURES_PER_FRAME, context]`` — the layout the
    causal TCN surrogate consumes, so training and the runtime benchmark
    exercise the same tensor shape rather than two that merely look alike.
    """

    features: torch.Tensor
    labels: torch.Tensor
    class_names: tuple[str, ...]
    context_frames: int

    def __post_init__(self) -> None:
        if self.features.ndim != 3:
            raise SmokeDatasetError("features must be [N, features, context]")
        if self.features.shape[1] != FEATURES_PER_FRAME:
            raise SmokeDatasetError(
                f"expected {FEATURES_PER_FRAME} features per frame, "
                f"got {self.features.shape[1]}")
        if self.features.shape[0] != self.labels.shape[0]:
            raise SmokeDatasetError("features and labels disagree on N")
        if self.features.shape[2] != self.context_frames:
            raise SmokeDatasetError("context_frames disagrees with tensor")

    def __len__(self) -> int:
        return int(self.features.shape[0])

    def split(self, *, holdout: int) -> tuple["SmokeDataset", "SmokeDataset"]:
        """Deterministic index split. **Leaks when windows overlap.**

        With ``stride < context_frames`` a held-out window shares frames with
        training windows, so accuracy measured this way is not generalization.
        It is still useful — it answers "did the optimizer move the weights at
        all" — but it must never be reported as held-out performance. Use
        :meth:`disjoint_split` for a number that means something.
        """
        if not isinstance(holdout, int) or not 0 < holdout < len(self):
            raise SmokeDatasetError("holdout must be inside (0, len)")
        cut = len(self) - holdout
        return (
            SmokeDataset(self.features[:cut], self.labels[:cut],
                         self.class_names, self.context_frames),
            SmokeDataset(self.features[cut:], self.labels[cut:],
                         self.class_names, self.context_frames),
        )


def disjoint_split(
    *,
    context_frames: int,
    seed: int = 20260810,
    stride: int = 1,
    holdout_fraction: float = 0.3,
) -> tuple[SmokeDataset, SmokeDataset]:
    """Train/holdout sharing **no source frame**, per class.

    Each class's frame matrix is cut in two along time, and a gap of
    ``context_frames`` is discarded at the seam so no window can straddle it.
    Without that gap a window ending one frame past the cut still contains
    ``context_frames - 1`` training frames, which is the leak that makes a
    naive split report perfect accuracy.

    Cutting per class rather than globally keeps both sides populated for
    every label; a global time cut would hand entire classes to one side.
    """
    if not isinstance(context_frames, int) or context_frames <= 0:
        raise SmokeDatasetError("context_frames must be a positive int")
    if not 0.0 < holdout_fraction < 1.0:
        raise SmokeDatasetError("holdout_fraction must be inside (0,1)")

    train_windows: list[np.ndarray] = []
    train_labels: list[int] = []
    hold_windows: list[np.ndarray] = []
    hold_labels: list[int] = []

    for index, (_name, renderer) in enumerate(SMOKE_CLASSES):
        matrix = _frames_for(renderer, seed=seed + index)
        total = matrix.shape[1]
        cut = int(total * (1.0 - holdout_fraction))
        left = matrix[:, :cut]
        right = matrix[:, cut + context_frames:]
        for source, windows, labels in (
            (left, train_windows, train_labels),
            (right, hold_windows, hold_labels),
        ):
            span = source.shape[1]
            if span < context_frames:
                raise SmokeDatasetError(
                    f"class {index} cannot supply a {context_frames}-frame "
                    f"window on one side of a disjoint split (has {span})")
            for start in range(0, span - context_frames + 1, stride):
                windows.append(source[:, start:start + context_frames])
                labels.append(index)

    def _pack(windows: list[np.ndarray], labels: list[int], salt: int
              ) -> SmokeDataset:
        stacked = torch.from_numpy(np.stack(windows)).float()
        targets = torch.tensor(labels, dtype=torch.long)
        order = torch.randperm(
            stacked.shape[0],
            generator=torch.Generator().manual_seed(seed + salt))
        return SmokeDataset(
            features=stacked[order],
            labels=targets[order],
            class_names=tuple(name for name, _ in SMOKE_CLASSES),
            context_frames=context_frames,
        )

    return _pack(train_windows, train_labels, 0), _pack(
        hold_windows, hold_labels, 1)


def frame_to_vector(frame: dict) -> np.ndarray:
    """One frontend frame to the flat float vector a model consumes.

    Order is fixed by ``BAND_VECTOR_FIELDS`` then ``SCALAR_FLOAT_FIELDS``, the
    same declaration the envelope counts, so the width cannot drift apart from
    the frontend without a test noticing.
    """
    parts = [np.asarray(frame[name], dtype=np.float32)
             for name in BAND_VECTOR_FIELDS]
    for name in parts:
        if name.shape != (GRID_BANDS,):
            raise SmokeDatasetError(
                f"band vector must have {GRID_BANDS} entries")
    scalars = np.asarray(
        [frame[name] for name in SCALAR_FLOAT_FIELDS], dtype=np.float32)
    return np.concatenate(parts + [scalars])


def _frames_for(renderer, *, seed: int) -> np.ndarray:
    """Real frames for one class, as [features, time]."""
    try:
        audio = renderer(_CANONICAL_RATE, seed=seed)
    except TypeError:
        audio = renderer(_CANONICAL_RATE)
    frames = extract_offline_feature_frames(audio, _CANONICAL_RATE)
    usable = [frame for frame in frames if frame["valid"]]
    if not usable:
        raise SmokeDatasetError("renderer produced no valid frame")
    return np.stack([frame_to_vector(frame) for frame in usable], axis=1)


def build_smoke_dataset(
    *,
    context_frames: int,
    seed: int = 20260810,
    stride: int = 1,
) -> SmokeDataset:
    """Materialize the dataset deterministically.

    Windows are cut per class and then shuffled once with a seeded generator,
    so class order cannot leak into the batch order while the result stays
    reproducible.
    """
    if not isinstance(context_frames, int) or context_frames <= 0:
        raise SmokeDatasetError("context_frames must be a positive int")
    if not isinstance(stride, int) or stride <= 0:
        raise SmokeDatasetError("stride must be a positive int")

    windows: list[np.ndarray] = []
    labels: list[int] = []
    for index, (_name, renderer) in enumerate(SMOKE_CLASSES):
        matrix = _frames_for(renderer, seed=seed + index)
        total = matrix.shape[1]
        if total < context_frames:
            raise SmokeDatasetError(
                f"class {index} has {total} frames, needs {context_frames}")
        for start in range(0, total - context_frames + 1, stride):
            windows.append(matrix[:, start:start + context_frames])
            labels.append(index)

    stacked = torch.from_numpy(np.stack(windows)).float()
    targets = torch.tensor(labels, dtype=torch.long)
    order = torch.randperm(
        stacked.shape[0], generator=torch.Generator().manual_seed(seed))
    return SmokeDataset(
        features=stacked[order],
        labels=targets[order],
        class_names=tuple(name for name, _ in SMOKE_CLASSES),
        context_frames=context_frames,
    )
