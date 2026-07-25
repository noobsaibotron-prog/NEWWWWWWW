"""G1a F4 — gate-platform interpreter must match metrology lock pin."""
from __future__ import annotations

import unittest

from ml_v3.contracts.metrology_lock import (
    GATE_PLATFORM_PYTHON,
    frozen_metrology_lock,
    gate_platform_python_label,
    require_gate_platform_python,
)


class GatePlatformInterpreterTests(unittest.TestCase):
    def test_lock_pin_is_cpython_312_13(self):
        lock = frozen_metrology_lock()
        pinned = lock["bit_identity"]["gate_platform"]["python"]
        self.assertEqual(pinned, "CPython 3.12.13")
        self.assertEqual(pinned, GATE_PLATFORM_PYTHON)

    def test_running_interpreter_matches_lock(self):
        """G1a suite FAIL-closed unless canonical venv (3.12.13) is used."""
        label = require_gate_platform_python()
        self.assertEqual(label, GATE_PLATFORM_PYTHON)
        self.assertEqual(label, gate_platform_python_label())


if __name__ == "__main__":
    unittest.main()
