"""Tests for the lab-only smoke training pipeline.

The task is signal-type classification on formula-rendered fixtures. It is
learnable and verifiable, and it is **not** the V3 problem: nothing here says
anything about Muddiness, Resonance or audio quality, and no checkpoint
produced here is a candidate.
"""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import torch

import ml_v3.contracts as contracts_pkg
from ml_v3.runtime_feasibility.envelope import LAB_ENVELOPES
from ml_v3.runtime_feasibility.smoke_dataset import (
    SMOKE_CLASSES,
    SPARSE_CLASSES_EXCLUDED,
    SmokeDataset,
    SmokeDatasetError,
    build_smoke_dataset,
    disjoint_split,
    frame_to_vector,
)
from ml_v3.runtime_feasibility.envelope import FEATURES_PER_FRAME
from ml_v3.runtime_feasibility.smoke_training import (
    SmokeTrainingError,
    envelope_for_dataset,
    evaluate_accuracy,
    load_checkpoint,
    majority_class_rate,
    train_smoke_model,
)

CONTEXT = 16
HOLDOUT_FRACTION = 0.6


class DatasetTests(unittest.TestCase):
    def test_windows_carry_the_real_frontend_width(self):
        dataset = build_smoke_dataset(context_frames=CONTEXT, stride=4)
        self.assertEqual(dataset.features.shape[1], FEATURES_PER_FRAME)
        self.assertEqual(dataset.features.shape[2], CONTEXT)
        self.assertFalse(torch.isnan(dataset.features).any())
        self.assertFalse(torch.isinf(dataset.features).any())

    def test_dataset_is_reproducible_from_source(self):
        """No WAV is read, so a rebuild must be bit-identical."""
        first = build_smoke_dataset(context_frames=CONTEXT, stride=4)
        second = build_smoke_dataset(context_frames=CONTEXT, stride=4)
        self.assertTrue(torch.equal(first.features, second.features))
        self.assertTrue(torch.equal(first.labels, second.labels))

    def test_every_class_is_represented(self):
        dataset = build_smoke_dataset(context_frames=CONTEXT, stride=4)
        self.assertEqual(len(dataset.class_names), len(SMOKE_CLASSES))
        self.assertEqual(
            set(dataset.labels.tolist()), set(range(len(SMOKE_CLASSES))))

    def test_sparse_fixtures_are_excluded_on_purpose(self):
        """Documented exclusion, not an accident.

        transient_burst and damped_resonance are mostly silence or decay below
        the validity floor, so the frontend correctly marks their frames
        invalid and they cannot fill a window. Keeping them importable means
        a longer-fixture variant can reinstate them without rediscovering why.
        """
        excluded = {name for name, _ in SPARSE_CLASSES_EXCLUDED}
        included = {name for name, _ in SMOKE_CLASSES}
        self.assertEqual(excluded, {"transient_burst", "damped_resonance"})
        self.assertFalse(excluded & included)

    def test_disjoint_split_shares_no_source_frame(self):
        """The split that makes a held-out number mean something.

        A naive index split over stride-1 windows leaks: a held-out window
        overlaps training windows by context-1 frames. This split cuts each
        class along time and discards a context-sized gap at the seam, so no
        window can straddle it.
        """
        train, holdout = disjoint_split(
            context_frames=CONTEXT, stride=1,
            holdout_fraction=HOLDOUT_FRACTION)
        self.assertGreater(len(train), 0)
        self.assertGreater(len(holdout), 0)
        train_rows = {tuple(row.flatten().tolist()) for row in train.features}
        holdout_rows = {
            tuple(row.flatten().tolist()) for row in holdout.features}
        self.assertFalse(train_rows & holdout_rows)

    def test_disjoint_split_reports_when_fixtures_are_too_short(self):
        """A 2 s fixture cannot hold out a long context; say so, do not pad.

        Each class needs roughly 3x context frames — train, gap, holdout — so
        the 63-frame class caps an honest disjoint context at about 21.
        """
        with self.assertRaises(SmokeDatasetError):
            disjoint_split(context_frames=32, stride=1, holdout_fraction=0.35)

    def test_disjoint_split_rejects_invalid_stride(self):
        for stride in (0, -1, 1.5):
            with self.subTest(stride=stride):
                with self.assertRaises(SmokeDatasetError):
                    disjoint_split(context_frames=CONTEXT, stride=stride)

    def test_rejects_malformed_requests(self):
        for kwargs in ({"context_frames": 0}, {"context_frames": 10_000}):
            with self.subTest(**kwargs):
                with self.assertRaises(SmokeDatasetError):
                    build_smoke_dataset(**kwargs)

    def test_frame_vector_rejects_a_wrong_width_band(self):
        dataset_frame = {name: [0.0] * 3 for name in ("mid_psd_db",)}
        with self.assertRaises((SmokeDatasetError, KeyError)):
            frame_to_vector(dataset_frame)


class TrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.train_set, cls.holdout_set = disjoint_split(
            context_frames=CONTEXT, stride=1,
            holdout_fraction=HOLDOUT_FRACTION)
        base = next(e for e in LAB_ENVELOPES if e.name == "tcn_small")
        cls.envelope = envelope_for_dataset(base, cls.train_set)

    def test_envelope_output_width_matches_the_task(self):
        """Eight logits for a three-class task would train dead outputs."""
        self.assertEqual(self.envelope.outputs, len(self.train_set.class_names))
        self.assertEqual(self.envelope.context_frames, CONTEXT)

    def test_majority_rate_is_the_honest_baseline(self):
        labels = torch.tensor([0, 0, 0, 1, 2])
        self.assertAlmostEqual(majority_class_rate(labels), 0.6)
        with self.assertRaises(SmokeTrainingError):
            majority_class_rate(torch.tensor([], dtype=torch.long))

    def test_end_to_end_training_closes_the_pipeline(self):
        """forward -> loss -> backward -> step -> checkpoint -> reload.

        The assertions are the falsifiable part: loss must fall, held-out
        accuracy must beat the majority class by a margin, and the reloaded
        checkpoint must reproduce the logits exactly. A loop that merely runs
        proves nothing — it could be optimizing nothing.
        """
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory) / "smoke.pt"
            report = train_smoke_model(
                self.envelope, self.train_set,
                holdout_set=self.holdout_set,
                epochs=30, batch_size=8, checkpoint_path=checkpoint)
            self.assertLess(report.final_loss, report.initial_loss)
            self.assertTrue(report.checkpoint_reload_exact)
            self.assertGreaterEqual(report.beats_majority_by, 0.15)

            restored = load_checkpoint(
                checkpoint,
                self.envelope,
                expected_class_names=self.train_set.class_names,
            )
            accuracy = evaluate_accuracy(restored, self.holdout_set)
            majority = majority_class_rate(self.holdout_set.labels)
            self.assertGreater(
                accuracy, majority,
                "a genuinely disjoint holdout must still beat the majority")

    def test_training_raises_instead_of_reporting_a_hollow_success(self):
        """An impossible margin must fail loudly, not return a nice report."""
        with self.assertRaises(SmokeTrainingError):
            train_smoke_model(
                self.envelope, self.train_set,
                holdout_set=self.holdout_set,
                epochs=1, batch_size=8,
                min_margin_over_majority=0.99)

    def test_rejects_impossible_hyperparameters(self):
        for kwargs in ({"epochs": 0}, {"batch_size": 0},
                       {"learning_rate": 0.0}):
            with self.subTest(**kwargs):
                with self.assertRaises(SmokeTrainingError):
                    train_smoke_model(
                        self.envelope, self.train_set,
                        holdout_set=self.holdout_set, **kwargs)

    def test_checkpoint_refuses_a_graph_it_does_not_match(self):
        """strict=True: a partial load would run with random weights."""
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory) / "smoke.pt"
            train_smoke_model(
                self.envelope, self.train_set,
                holdout_set=self.holdout_set,
                epochs=2, batch_size=8, checkpoint_path=checkpoint,
                min_margin_over_majority=-1.0)
            wider = envelope_for_dataset(
                next(e for e in LAB_ENVELOPES if e.name == "tcn_worst"),
                self.train_set)
            with self.assertRaises(SmokeTrainingError):
                load_checkpoint(checkpoint, wider)

    def test_checkpoint_binds_class_identity_not_only_tensor_shape(self):
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory) / "smoke.pt"
            train_smoke_model(
                self.envelope, self.train_set,
                holdout_set=self.holdout_set,
                epochs=2, batch_size=8, checkpoint_path=checkpoint,
                min_margin_over_majority=-1.0)
            reversed_classes = tuple(reversed(self.train_set.class_names))
            with self.assertRaisesRegex(
                    SmokeTrainingError, "class order mismatch"):
                load_checkpoint(
                    checkpoint,
                    self.envelope,
                    expected_class_names=reversed_classes,
                )

    def test_training_rejects_incompatible_holdout(self):
        malformed = SmokeDataset(
            features=self.holdout_set.features[..., :-1],
            labels=self.holdout_set.labels,
            class_names=self.holdout_set.class_names,
            context_frames=self.holdout_set.context_frames - 1,
        )
        with self.assertRaisesRegex(SmokeTrainingError, "context differs"):
            train_smoke_model(
                self.envelope, self.train_set,
                holdout_set=malformed,
                epochs=1, batch_size=8,
                min_margin_over_majority=-1.0,
            )


class PerimeterTests(unittest.TestCase):
    def test_smoke_training_is_absent_from_the_rev7_dispatcher(self):
        for symbol in ("train_smoke_model", "build_smoke_dataset",
                       "SmokeDataset"):
            self.assertFalse(
                hasattr(contracts_pkg, symbol),
                f"{symbol} must not be exported by ml_v3.contracts",
            )


if __name__ == "__main__":
    unittest.main()
