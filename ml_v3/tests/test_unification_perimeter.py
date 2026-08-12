"""The guarantee that replaces the ship-line 0-diff invariant.

Until the two lines were unified, the lab branch proved it could not touch the
plugin by keeping ``Source/``, ``CMakeLists.txt``, ``ml_v2/``, ``Resources/``
and ``AIEQ-mac/`` byte-identical to G0 ``2c88edad``. That check is meaningless
now and must not be quietly kept as if it still meant something: on this branch
the product is *supposed* to move ahead of G0, and it has — the real-time
snapshot buffer, the Butterworth cascades, the latency plan.

What the frozen diff actually protected was never file equality. It was the
property that a plugin a user installs cannot depend on laboratory code that is
explicitly not ready: ``REV8 SPEC GO = NO``, no trained model, no corpus. That
property is stated directly here, and unlike a frozen hash it stays true while
the product keeps moving.

``ml_v2`` is deliberately absent from the prohibition. The build does reference
it — the A4b guard tests run under ctest and ``Source/AI/MotoreV2Features.h``
exists — and that is long-standing, deliberate and tested. The line drawn is
around ``ml_v3``, the part that has no authority to ship.
"""
from __future__ import annotations

import unittest
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]

#: Everything the plugin build compiles, configures or packages from.
#:
#: ``AIEQ-mac`` is absent on purpose: the old ship-line check named it, but the
#: directory exists on neither parent branch, so that clause was verifying an
#: empty set. A guard listing a path that is not there is worse than no guard —
#: it reads as coverage. :meth:`test_the_product_surface_exists_on_this_branch`
#: keeps this list honest by failing if any entry disappears.
_PRODUCT_SURFACE = ("CMakeLists.txt", "Source", "Resources")

_SUFFIXES = {".cpp", ".h", ".hpp", ".mm", ".txt", ".cmake", ".plist", ".in"}


def _product_files() -> list[Path]:
    files: list[Path] = []
    for entry in _PRODUCT_SURFACE:
        path = _REPO_ROOT / entry
        if path.is_file():
            files.append(path)
        elif path.is_dir():
            files.extend(
                child for child in path.rglob("*")
                if child.is_file() and child.suffix in _SUFFIXES
            )
    return files


class UnificationPerimeterTests(unittest.TestCase):
    def test_the_product_surface_exists_on_this_branch(self):
        """Guard the guard: a typo in a path would make every check vacuous."""
        for entry in _PRODUCT_SURFACE:
            self.assertTrue(
                (_REPO_ROOT / entry).exists(), f"{entry} not found")
        self.assertGreater(len(_product_files()), 100)

    def test_the_plugin_build_does_not_reference_the_lab(self):
        """No product file may name ml_v3 — the shipping path cannot reach it."""
        offenders: list[str] = []
        for path in _product_files():
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if "ml_v3" in text:
                offenders.append(str(path.relative_to(_REPO_ROOT)))
        self.assertEqual(
            offenders, [],
            "the plugin build reached into ml_v3; the lab is not shippable "
            f"(REV8 SPEC GO = NO): {offenders}",
        )

    def test_the_lab_does_not_import_plugin_sources(self):
        """The converse direction, so the boundary is not one-way by luck."""
        this_file = Path(__file__).resolve()
        offenders: list[str] = []
        for path in (_REPO_ROOT / "ml_v3").rglob("*.py"):
            # This module names the forbidden paths in order to forbid them.
            if path.resolve() == this_file:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            for marker in ("Source/AI", "Source/DSP", "Source/GUI"):
                if marker in text:
                    offenders.append(f"{path.relative_to(_REPO_ROOT)}:{marker}")
        self.assertEqual(offenders, [], f"lab reached into plugin: {offenders}")


if __name__ == "__main__":
    unittest.main()
