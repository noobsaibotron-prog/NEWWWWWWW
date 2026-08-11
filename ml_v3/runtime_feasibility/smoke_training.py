"""End-to-end smoke training. Lab-only, never promotable.

Proves the *training* half of the pipeline the way the benchmark proved the
inference half: frontend features -> model -> loss -> backward -> optimizer ->
checkpoint -> reload -> identical inference. If any link is broken, it breaks
here on three formula-rendered fixtures instead of after months of corpus
work.

What this deliberately does **not** claim: that the model learned anything
about audio quality. The task is signal-type classification, chosen because it
is learnable and verifiable, and a model that scores well on it says nothing
about Muddiness or Resonance. No checkpoint produced here is a V3 candidate.

The success criterion is falsifiable and stated up front: training accuracy
must beat the majority-class rate by a stated margin, and a reloaded
checkpoint must reproduce the pre-save logits bit-for-bit. A loop that merely
runs without raising proves nothing — it could be optimizing nothing at all.
"""
from __future__ import annotations


from dataclasses import dataclass, field
from pathlib import Path

import torch
from torch import nn

from ml_v3.runtime_feasibility.envelope import RuntimeEnvelope
from ml_v3.runtime_feasibility.smoke_dataset import SmokeDataset
from ml_v3.runtime_feasibility.surrogate import build_surrogate

__all__ = [
    "SmokeTrainingError",
    "envelope_for_dataset",
    "TrainingReport",
    "evaluate_accuracy",
    "load_checkpoint",
    "majority_class_rate",
    "save_checkpoint",
    "train_smoke_model",
]


class SmokeTrainingError(RuntimeError):
    """The smoke training could not run or did not meet its criterion."""


@dataclass(frozen=True)
class TrainingReport:
    epochs: int
    batch_size: int
    learning_rate: float
    initial_loss: float
    final_loss: float
    losses: tuple[float, ...]
    train_accuracy: float
    holdout_accuracy: float
    majority_rate: float
    parameters: int
    checkpoint_reload_exact: bool
    seed: int
    extra: dict = field(default_factory=dict)

    @property
    def beats_majority_by(self) -> float:
        return self.holdout_accuracy - self.majority_rate


def majority_class_rate(labels: torch.Tensor) -> float:
    """Accuracy of always predicting the most common class.

    The honest baseline. A three-class task with an unbalanced split can look
    like 60% "accuracy" from a model that learned nothing, so every accuracy
    below is reported against this rather than against 1/3.
    """
    if labels.numel() == 0:
        raise SmokeTrainingError("no labels")
    counts = torch.bincount(labels)
    return float(counts.max().item() / labels.numel())


def evaluate_accuracy(model: nn.Module, dataset: SmokeDataset) -> float:
    model.eval()
    with torch.no_grad():
        predicted = model(dataset.features).argmax(dim=-1)
    return float((predicted == dataset.labels).float().mean().item())


def save_checkpoint(model: nn.Module, path: Path, *, meta: dict) -> None:
    """Persist weights plus the metadata needed to rebuild the same graph.

    Storing the envelope alongside the tensors is what makes a reload
    verifiable: weights without their shape are only replayable by code that
    already happens to agree, which is exactly the coupling that breaks
    silently later.
    """
    torch.save({"state_dict": model.state_dict(), "meta": meta}, path)


def load_checkpoint(path: Path, envelope: RuntimeEnvelope) -> nn.Module:
    """Rebuild the graph from the envelope and load the saved tensors.

    ``strict=True`` on purpose: a checkpoint whose keys do not match the graph
    exactly is a mismatch, and letting torch fill the gap would produce a model
    that runs while carrying partly random weights.
    """
    payload = torch.load(path, weights_only=False)
    if "state_dict" not in payload:
        raise SmokeTrainingError("checkpoint has no state_dict")
    model = build_surrogate(envelope, seed=0)
    model.load_state_dict(payload["state_dict"], strict=True)
    model.eval()
    return model


def train_smoke_model(
    envelope: RuntimeEnvelope,
    dataset: SmokeDataset,
    *,
    epochs: int = 60,
    batch_size: int = 16,
    learning_rate: float = 1e-3,
    holdout: int = 20,
    seed: int = 20260810,
    checkpoint_path: Path | None = None,
    min_margin_over_majority: float = 0.15,
) -> TrainingReport:
    """Run the loop and assert it actually optimized something.

    ``min_margin_over_majority`` is the falsifiable part: if the held-out
    accuracy does not beat the majority-class rate by this margin, the
    function raises rather than returning a report that reads like success.
    """
    if epochs <= 0 or batch_size <= 0 or learning_rate <= 0:
        raise SmokeTrainingError("epochs, batch_size, learning_rate must be > 0")

    torch.manual_seed(seed)
    train_set, holdout_set = dataset.split(holdout=holdout)

    model = build_surrogate(envelope, seed=seed)
    model.train()
    optimizer = torch.optim.Adam(model.parameters(), lr=learning_rate)
    criterion = nn.CrossEntropyLoss()

    generator = torch.Generator().manual_seed(seed)
    losses: list[float] = []
    for _epoch in range(epochs):
        order = torch.randperm(len(train_set), generator=generator)
        epoch_loss = 0.0
        batches = 0
        for start in range(0, len(train_set), batch_size):
            index = order[start:start + batch_size]
            optimizer.zero_grad(set_to_none=True)
            logits = model(train_set.features[index])
            loss = criterion(logits, train_set.labels[index])
            loss.backward()
            optimizer.step()
            epoch_loss += float(loss.item())
            batches += 1
        losses.append(epoch_loss / max(1, batches))

    train_accuracy = evaluate_accuracy(model, train_set)
    holdout_accuracy = evaluate_accuracy(model, holdout_set)
    majority = majority_class_rate(holdout_set.labels)

    reload_exact = True
    if checkpoint_path is not None:
        model.eval()
        with torch.no_grad():
            before = model(holdout_set.features).clone()
        save_checkpoint(model, checkpoint_path, meta={
            "envelope": envelope.name,
            "classes": list(dataset.class_names),
            "context_frames": dataset.context_frames,
        })
        restored = load_checkpoint(checkpoint_path, envelope)
        with torch.no_grad():
            after = restored(holdout_set.features)
        reload_exact = bool(torch.equal(before, after))

    report = TrainingReport(
        epochs=epochs,
        batch_size=batch_size,
        learning_rate=learning_rate,
        initial_loss=losses[0],
        final_loss=losses[-1],
        losses=tuple(losses),
        train_accuracy=train_accuracy,
        holdout_accuracy=holdout_accuracy,
        majority_rate=majority,
        parameters=sum(p.numel() for p in model.parameters()),
        checkpoint_reload_exact=reload_exact,
        seed=seed,
        extra={"classes": list(dataset.class_names),
               "train_windows": len(train_set),
               "holdout_windows": len(holdout_set)},
    )

    if report.final_loss >= report.initial_loss:
        raise SmokeTrainingError(
            f"loss did not decrease: {report.initial_loss:.4f} -> "
            f"{report.final_loss:.4f}")
    if report.beats_majority_by < min_margin_over_majority:
        raise SmokeTrainingError(
            f"holdout accuracy {holdout_accuracy:.3f} does not beat majority "
            f"{majority:.3f} by {min_margin_over_majority:.2f}")
    if not reload_exact:
        raise SmokeTrainingError("reloaded checkpoint changed the logits")
    return report


def envelope_for_dataset(
    base: RuntimeEnvelope,
    dataset: SmokeDataset,
) -> RuntimeEnvelope:
    """Same topology, output width matched to the task.

    The benchmark envelopes emit eight logits because that is the worst-case
    output the plan implies. A three-class smoke task needs three, and
    silently slicing the extra five would train a head whose unused outputs
    still receive gradient — a subtle way to make the loop look healthier than
    the model is.
    """
    return RuntimeEnvelope(
        name=f"{base.name}_smoke",
        kind=base.kind,
        context_frames=dataset.context_frames,
        channels=base.channels,
        layers=base.layers,
        kernel=base.kernel,
        outputs=len(dataset.class_names),
    )
