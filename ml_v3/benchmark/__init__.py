"""G1b lab evaluation harnesses (spike / offline). Not ship-line."""
from __future__ import annotations

from .sr_parity import (
    MARGIN_FLOOR_DB,
    SPIKE_ADVERSARIAL_ASSETS,
    SRParityError,
    evaluate_sr_parity_cell,
    run_spike_gate4_matrix,
    verdict_from_max_abs,
)

__all__ = [
    "MARGIN_FLOOR_DB",
    "SPIKE_ADVERSARIAL_ASSETS",
    "SRParityError",
    "evaluate_sr_parity_cell",
    "run_spike_gate4_matrix",
    "verdict_from_max_abs",
]
