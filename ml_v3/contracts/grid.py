"""Canonical 120-band physical grid (G1a).

Contract section 6.1, lines 187-191:

    center[i] = 20 * (20000 / 20) ** (i / 119), i = 0..119

exactly 120 inclusive centres from 20 Hz to 20000 Hz. G1a freezes the centres
only. Triangular supports, PSD, FFT and any frontend behaviour belong to G1b
(contract section 14, line 931).
"""
from __future__ import annotations

from .constants import GRID_BANDS, GRID_MAX_HZ, GRID_MIN_HZ, TONAL_REGIONS

__all__ = ["band_centers_hz", "region_of_frequency", "region_index_of_band"]


def band_centers_hz() -> list[float]:
    """The 120 canonical band centres, strictly increasing, ends exact."""
    ratio = GRID_MAX_HZ / GRID_MIN_HZ
    last = GRID_BANDS - 1
    centers = [GRID_MIN_HZ * (ratio ** (index / last)) for index in range(GRID_BANDS)]
    # Pin the endpoints to the exact contract values: the closed form already
    # yields them, but floating-point pow must never be allowed to drift the
    # two values the structure gate checks literally (contract gate 1).
    centers[0] = GRID_MIN_HZ
    centers[last] = GRID_MAX_HZ
    return centers


def region_of_frequency(frequency_hz: float) -> int | None:
    """Index of the canonical tonal region containing `frequency_hz`.

    Regions are left-inclusive and right-exclusive except the last, which is
    inclusive on both ends (contract section 11.1, lines 710-712).
    """
    last_index = len(TONAL_REGIONS) - 1
    for index, (low, high) in enumerate(TONAL_REGIONS):
        if index == last_index:
            if low <= frequency_hz <= high:
                return index
        elif low <= frequency_hz < high:
            return index
    return None


def region_index_of_band(band_index: int) -> int | None:
    """Canonical tonal region of a band, addressed by its centre frequency."""
    if not 0 <= band_index < GRID_BANDS:
        raise IndexError(f"band index out of range: {band_index}")
    return region_of_frequency(band_centers_hz()[band_index])
