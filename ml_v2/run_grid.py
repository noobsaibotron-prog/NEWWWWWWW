"""Small, torch-free helpers for reproducible A4b experiment grids."""
from __future__ import annotations


DEFAULT_DATASET_SEED = 42
_MAX_SEED = (1 << 32) - 2  # validation uses dataset_seed + 1.


def parse_seed_csv(value: str, option: str) -> list[int]:
    """Parse a non-empty, duplicate-free CSV of RNG seeds.

    Keep this outside ``train.py`` so CTest can protect grid identity without
    importing Torch. The upper bound leaves one representable seed for the
    validation build derived from a dataset seed.
    """
    seeds: list[int] = []
    for raw in value.split(","):
        token = raw.strip()
        if not token:
            raise ValueError(f"{option} contains an empty seed")
        try:
            seed = int(token)
        except ValueError as exc:
            raise ValueError(f"{option} contains non-integer seed {token!r}") from exc
        if not 0 <= seed <= _MAX_SEED:
            raise ValueError(f"{option} seed {seed} is outside [0, {_MAX_SEED}]")
        if seed in seeds:
            raise ValueError(f"{option} repeats seed {seed}")
        seeds.append(seed)
    if not seeds:
        raise ValueError(f"{option} must contain at least one seed")
    return seeds


def candidate_stem(model_seed: int, dataset_seed: int,
                   legacy_names: bool) -> str:
    """Return an unambiguous candidate basename for one Cartesian-grid cell."""
    if legacy_names:
        return f"candidate_s{model_seed}"
    return f"candidate_s{model_seed}_d{dataset_seed}"
