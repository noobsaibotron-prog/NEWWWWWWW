"""G1a T6 — fixture signal generators + committed WAV digests.

Freeze authority: fixture_spec_v1.json (513c3baf…). No G1a CLOSE claim.
"""
from __future__ import annotations

import math
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

from ml_v3.contracts.canonical import sha256_of_file, sha256sums_text
from ml_v3.contracts.constants import GATE_SAMPLE_RATES
from ml_v3.contracts.fixture_spec import (
    fixture_spec_sha256,
    frozen_fixture_spec,
    sample_count,
    sweep_active_end_seconds,
    sweep_active_start_seconds,
)
from ml_v3.contracts.sha256sums import (
    G1A_SHA256SUMS_COVERED,
    G1A_SHA256SUMS_RELPATH,
    Sha256SumsError,
    load_g1a_sha256sums,
    verify_g1a_sha256sums,
)
from ml_v3.fixtures.g1.render_signals import (
    AUDIO_REL_PREFIX,
    asset_relpath,
    iter_asset_specs,
    render_log_sweep,
    render_multitone,
    render_non_finite,
    render_pseudo_noise,
    render_silence,
    render_stereo_decorrelated,
    render_stereo_mid_only,
    render_stereo_side_only,
    write_wav_pcm_float32_le,
)

REPO = Path(__file__).resolve().parents[2]
FIXTURE_SPEC_DIGEST = (
    "513c3baf7aaed8eb1a15f7d2e875a3015479fc2ece0378e75cfceadefe68a6ef"
)


class FixtureSpecAuthorityTests(unittest.TestCase):
    def test_fixture_spec_digest_frozen(self):
        self.assertEqual(fixture_spec_sha256(), FIXTURE_SPEC_DIGEST)


class RenderSemanticsTests(unittest.TestCase):
    def test_direct_rates_no_resample_lengths(self):
        for fs in GATE_SAMPLE_RATES:
            self.assertEqual(render_multitone(fs).shape[0], sample_count(fs))
            self.assertEqual(render_log_sweep(fs).shape[0], sample_count(fs))
            self.assertEqual(render_pseudo_noise(fs).shape[0], sample_count(fs))

    def test_dtype_float32_after_cast(self):
        y = render_multitone(48000)
        self.assertEqual(y.dtype, np.float32)

    def test_log_sweep_active_interval_and_outside_zero(self):
        fs = 48000
        y = render_log_sweep(fs)
        t0 = sweep_active_start_seconds()
        t1 = sweep_active_end_seconds()
        self.assertEqual(t0, 0.5)
        self.assertEqual(t1, 1.75)
        # Sample just before active start must be zero.
        n0 = int(math.floor(t0 * fs)) - 1
        self.assertGreaterEqual(n0, 0)
        self.assertEqual(float(y[n0]), 0.0)
        # First active sample non-zero (phase starts at 0 → sin(0)=0 at exact
        # t_start; next sample must leave zero).
        n_start = int(round(t0 * fs))
        self.assertEqual(float(y[n_start]), 0.0)  # phi(t_start)=0
        self.assertNotEqual(float(y[n_start + 1]), 0.0)
        # Outside after t_end.
        n_after = int(math.floor(t1 * fs)) + 1
        self.assertEqual(float(y[n_after]), 0.0)

    def test_silence_and_non_finite_inject(self):
        z = render_silence(48000)
        self.assertTrue(np.all(z == 0.0))
        nan = render_non_finite(48000, "nan_at_sample_0")
        self.assertTrue(math.isnan(float(nan[0])))
        self.assertTrue(np.all(nan[1:] == 0.0))
        pos = render_non_finite(48000, "pos_inf_at_sample_0")
        self.assertTrue(math.isinf(float(pos[0])) and float(pos[0]) > 0)
        neg = render_non_finite(48000, "neg_inf_at_sample_0")
        self.assertTrue(math.isinf(float(neg[0])) and float(neg[0]) < 0)

    def test_stereo_ms_conventions(self):
        mid = render_stereo_mid_only(48000)
        side = render_stereo_side_only(48000)
        deco = render_stereo_decorrelated(48000)
        self.assertEqual(mid.shape[1], 2)
        # mid-only: L==R
        self.assertTrue(np.array_equal(mid[:, 0], mid[:, 1]))
        # side-only: R==-L
        self.assertTrue(np.array_equal(side[:, 1], -side[:, 0]))
        # decorrelated: channels differ
        self.assertFalse(np.array_equal(deco[:, 0], deco[:, 1]))

    def test_ultrasonic_only_96k(self):
        specs = [
            (cat, stem, fs)
            for cat, stem, fs, _ in iter_asset_specs()
            if cat == "ultrasonic_96k"
        ]
        self.assertEqual(specs, [("ultrasonic_96k", "ultrasonic_96k", 96000)])


def _wav_relpaths_under(root: Path) -> set[str]:
    audio = root / "ml_v3" / "fixtures" / "g1" / "audio"
    return {
        p.relative_to(root).as_posix()
        for p in audio.rglob("*.wav")
        if p.is_file()
    }


def _sha256_map(root: Path, rels: set[str]) -> dict[str, str]:
    return {rel: sha256_of_file(root / rel) for rel in sorted(rels)}


class DeterminismTests(unittest.TestCase):
    def test_two_run_byte_identical_core_renders(self):
        for fs in GATE_SAMPLE_RATES:
            a = render_multitone(fs)
            b = render_multitone(fs)
            self.assertEqual(a.tobytes(), b.tobytes())
            a = render_log_sweep(fs)
            b = render_log_sweep(fs)
            self.assertEqual(a.tobytes(), b.tobytes())
            a = render_pseudo_noise(fs)
            b = render_pseudo_noise(fs)
            self.assertEqual(a.tobytes(), b.tobytes())

    def test_two_run_byte_identical_wav_files(self):
        y = render_multitone(48000)
        with tempfile.TemporaryDirectory() as tmp:
            p1 = Path(tmp) / "a.wav"
            p2 = Path(tmp) / "b.wav"
            write_wav_pcm_float32_le(p1, y, 48000)
            write_wav_pcm_float32_le(p2, y, 48000)
            self.assertEqual(p1.read_bytes(), p2.read_bytes())
            # IEEE float format tag 3
            raw = p1.read_bytes()
            self.assertEqual(struct.unpack_from("<H", raw, 20)[0], 3)

    def test_two_clean_processes_byte_identical_wav_trees(self):
        """§13.2.2 proof for T6 fixture WAV only (not stack-wide G1).

        Two clean OS subprocesses → byte-identical WAV trees, then each of
        the 37 temp_A digests must equal the committed SHA256SUMS inventory.
        Does not call update_sha256sums_with_audio on the temp trees.
        """
        expected = {
            asset_relpath(cat, stem, fs)
            for cat, stem, fs, _fn in iter_asset_specs()
        }
        self.assertEqual(len(expected), 37)
        committed_sums = load_g1a_sha256sums()

        child = (
            "from pathlib import Path\n"
            "import sys\n"
            "from ml_v3.fixtures.g1.render_signals import render_all_to_tree\n"
            "rels = render_all_to_tree(Path(sys.argv[1]))\n"
            "sys.stdout.write('\\n'.join(rels))\n"
        )
        env = dict(os.environ)
        # Ensure repo root import path in a clean process.
        env["PYTHONPATH"] = (
            str(REPO)
            if not env.get("PYTHONPATH")
            else str(REPO) + os.pathsep + env["PYTHONPATH"]
        )

        with tempfile.TemporaryDirectory() as tmp_a, tempfile.TemporaryDirectory() as tmp_b:
            root_a = Path(tmp_a)
            root_b = Path(tmp_b)
            for root in (root_a, root_b):
                subprocess.run(
                    [sys.executable, "-c", child, str(root)],
                    check=True,
                    cwd=str(REPO),
                    env=env,
                    capture_output=True,
                    text=True,
                )

            rels_a = _wav_relpaths_under(root_a)
            rels_b = _wav_relpaths_under(root_b)
            self.assertEqual(len(rels_a), 37)
            self.assertEqual(len(rels_b), 37)
            self.assertEqual(rels_a, expected)
            self.assertEqual(rels_b, expected)

            for rel in sorted(expected):
                self.assertEqual(
                    (root_a / rel).read_bytes(),
                    (root_b / rel).read_bytes(),
                    msg=rel,
                )

            self.assertEqual(
                _sha256_map(root_a, rels_a),
                _sha256_map(root_b, rels_b),
            )

            # Bind two-process renders to the committed SHA256SUMS inventory.
            for rel in sorted(expected):
                self.assertIn(rel, committed_sums, msg=f"missing from SHA256SUMS: {rel}")
                self.assertEqual(
                    sha256_of_file(root_a / rel),
                    committed_sums[rel],
                    msg=rel,
                )


class CommittedInventoryTests(unittest.TestCase):
    def test_all_categories_present_in_inventory(self):
        specs = iter_asset_specs()
        self.assertGreaterEqual(len(specs), 30)
        categories = {cat for cat, _stem, _fs, _fn in specs}
        expected = set(frozen_fixture_spec()["categories"])
        self.assertEqual(categories, expected)

    def test_committed_wavs_match_rerender_and_sums(self):
        entries = verify_g1a_sha256sums()
        specs = iter_asset_specs()
        self.assertEqual(len(specs), 37)
        for category, stem, fs, fn in specs:
            rel = asset_relpath(category, stem, fs)
            absolute = REPO / rel
            self.assertTrue(absolute.is_file(), rel)
            self.assertTrue(rel.startswith(AUDIO_REL_PREFIX + "/"))
            # Byte-identical to a fresh render written through the same writer.
            rendered = fn(fs)
            with tempfile.TemporaryDirectory() as tmp:
                tmp_path = Path(tmp) / "x.wav"
                write_wav_pcm_float32_le(tmp_path, rendered, fs)
                self.assertEqual(
                    absolute.read_bytes(),
                    tmp_path.read_bytes(),
                    msg=rel,
                )
            self.assertEqual(entries[rel], sha256_of_file(absolute))

    def test_verify_rejects_sums_with_audio_stripped(self):
        """Truncated SHA256SUMS (COVERED only) must fail verify."""
        full = load_g1a_sha256sums()
        truncated = {path: full[path] for path in G1A_SHA256SUMS_COVERED}
        self.assertEqual(len(truncated), 11)
        with tempfile.TemporaryDirectory() as tmp:
            troot = Path(tmp)
            for rel in truncated:
                absolute = troot / rel
                absolute.parent.mkdir(parents=True, exist_ok=True)
                absolute.write_bytes((REPO / rel).read_bytes())
            sums_path = troot / G1A_SHA256SUMS_RELPATH
            sums_path.parent.mkdir(parents=True, exist_ok=True)
            sums_path.write_text(sha256sums_text(truncated), encoding="utf-8")
            with self.assertRaises(Sha256SumsError) as ctx:
                verify_g1a_sha256sums(troot)
            self.assertIn("missing required audio inventory", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
