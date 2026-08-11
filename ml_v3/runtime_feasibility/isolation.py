"""Control-flow prototype of the audio/worker split. Lab-only.

    producer (audio-like, bounded work)
      -> bounded preallocated queue
      -> analysis worker (frontend + inference)
      -> bounded result queue
      -> consumer (audio-like, bounded work)

**This is a control-flow prototype; C++/JUCE RT proof still pending.** Python
cannot demonstrate real-time safety: it allocates, it has a GIL, and its
scheduler is not the host's. What it *can* demonstrate is the shape of the
contract — that the audio-like side never waits on the worker, that queues are
bounded, that overflow is counted rather than grown, and that a slow, broken
or dead worker degrades to neutral instead of stalling or crashing.

The invariant that matters is not "inference is fast enough". It is "audio
does not care how fast inference is". A model that misses its deadline should
cost a stale suggestion, never a dropout.
"""
from __future__ import annotations

import math
import threading
from collections import deque
from dataclasses import dataclass, field

__all__ = [
    "AudioLikeConsumer",
    "BoundedQueue",
    "IsolationError",
    "NEUTRAL_RESULT",
    "WorkerStats",
]


class IsolationError(RuntimeError):
    """Structural violation of the isolation contract."""


#: What the audio side applies when it has nothing fresh and trustworthy.
NEUTRAL_RESULT: tuple[float, ...] = (0.0,) * 8


class BoundedQueue:
    """Fixed-capacity queue that drops instead of growing or blocking.

    An unbounded queue converts a slow worker into unbounded memory growth and
    unbounded staleness; a blocking queue converts it into an audio stall.
    Both are worse than dropping. Capacity never causes ``push`` to wait and
    the queue never grows beyond its bound. The Python prototype still uses a
    short mutex around deque access, so it is not a proof of lock-freedom or
    real-time safety; that remains a requirement for the future C++ path.
    """

    def __init__(self, capacity: int) -> None:
        if not isinstance(capacity, int) or capacity <= 0:
            raise IsolationError("capacity must be a positive int")
        self._capacity = capacity
        self._items: deque = deque()
        self._lock = threading.Lock()
        self.dropped = 0

    @property
    def capacity(self) -> int:
        return self._capacity

    def __len__(self) -> int:
        with self._lock:
            return len(self._items)

    def push(self, item: object) -> bool:
        """Enqueue if there is room; capacity overflow is counted and dropped."""
        with self._lock:
            if len(self._items) >= self._capacity:
                self.dropped += 1
                return False
            self._items.append(item)
            return True

    def pop(self) -> object | None:
        with self._lock:
            return self._items.popleft() if self._items else None


@dataclass
class WorkerStats:
    processed: int = 0
    failed: int = 0
    rejected_nonfinite: int = 0
    rejected_out_of_order: int = 0
    last_sequence: int = -1


@dataclass
class AudioLikeConsumer:
    """The audio-side half of the contract.

    Bounded work by construction: output width is fixed, it looks at the newest
    available result, applies a staleness rule, and returns. It never waits,
    retries, or asks whether the worker is healthy — a consumer that inspects
    worker state is a consumer that can be made to block by it.
    """

    max_stale_frames: int
    _latest: tuple[int, tuple[float, ...]] | None = field(
        default=None, repr=False)
    neutral_fallbacks: int = 0
    applied: int = 0

    def __post_init__(self) -> None:
        if not isinstance(self.max_stale_frames, int) or self.max_stale_frames < 0:
            raise IsolationError("max_stale_frames must be a non-negative int")

    def offer(self, sequence: int, values: tuple[float, ...]) -> None:
        """Accept a worker result, rejecting anything unusable.

        Non-finite output is dropped rather than clamped: a NaN that reaches a
        filter coefficient is an audible failure, and silently substituting a
        number would hide a broken model.
        """
        if not isinstance(sequence, int) or sequence < 0:
            raise IsolationError("sequence must be a non-negative int")
        if not isinstance(values, tuple) or len(values) != len(NEUTRAL_RESULT):
            raise IsolationError("result must have the fixed output width")
        try:
            finite = all(math.isfinite(value) for value in values)
        except TypeError as error:
            raise IsolationError("result values must be numeric") from error
        if not finite:
            raise IsolationError("non-finite result rejected")
        if self._latest is not None and sequence <= self._latest[0]:
            raise IsolationError("out-of-order result rejected")
        self._latest = (sequence, values)

    def consume(self, current_frame: int) -> tuple[float, ...]:
        """Return what audio should apply at ``current_frame``. Bounded work."""
        if not isinstance(current_frame, int) or current_frame < 0:
            raise IsolationError("current_frame must be a non-negative int")
        if self._latest is None:
            self.neutral_fallbacks += 1
            return NEUTRAL_RESULT
        sequence, values = self._latest
        if current_frame < sequence:
            self.neutral_fallbacks += 1
            return NEUTRAL_RESULT
        if current_frame - sequence > self.max_stale_frames:
            self.neutral_fallbacks += 1
            return NEUTRAL_RESULT
        self.applied += 1
        return values
