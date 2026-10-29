# no-port-check: NereusSDR-original.
"""Time for the server: a real clock, and a manual one the tests drive.

Every timer the server keeps (the hello timeout now; joining, turns and
room closing later) and every rate-limit window goes through a clock, so the
conformance fixtures move time with {"advanceMs": N} and never sleep.
now_ms() is monotonic and drives timers and windows; wall_ms() is Unix
milliseconds and is what goes on the wire (welcome `time`, reading ages).
"""

from __future__ import annotations

import asyncio
import heapq
import itertools
import time
from typing import Callable, List, Optional, Tuple


class TimerHandle:
    def __init__(self) -> None:
        self.cancelled = False
        self._inner: Optional[asyncio.TimerHandle] = None

    def cancel(self) -> None:
        self.cancelled = True
        if self._inner is not None:
            self._inner.cancel()


class RealClock:
    def now_ms(self) -> int:
        return int(time.monotonic() * 1000)

    def wall_ms(self) -> int:
        return int(time.time() * 1000)

    def call_later(self, delay_ms: int, callback: Callable[[], None]) -> TimerHandle:
        handle = TimerHandle()

        def fire() -> None:
            if not handle.cancelled:
                callback()

        handle._inner = asyncio.get_running_loop().call_later(max(0, delay_ms) / 1000.0, fire)
        return handle


# Wire spec section 11.4: a fixture starts at this Unix ms unless its setup
# names another.
DEFAULT_WALL_START_MS = 1800000000000


class ManualClock:
    """A clock that moves only when advance() is called. Timers due at or
    before the target fire first, in the order they fall due (ties in the
    order they were set), each with now_ms() at its own deadline (wire spec
    6.6, "Moments")."""

    def __init__(self, wall_start_ms: int = DEFAULT_WALL_START_MS) -> None:
        self._now = 0
        self._wall_start = int(wall_start_ms)
        self._timers: List[Tuple[int, int, TimerHandle, Callable[[], None]]] = []
        self._seq = itertools.count()

    def now_ms(self) -> int:
        return self._now

    def wall_ms(self) -> int:
        return self._wall_start + self._now

    def call_later(self, delay_ms: int, callback: Callable[[], None]) -> TimerHandle:
        handle = TimerHandle()
        heapq.heappush(self._timers, (self._now + max(0, int(delay_ms)), next(self._seq), handle, callback))
        return handle

    def advance(self, ms: int) -> None:
        target = self._now + int(ms)
        while self._timers and self._timers[0][0] <= target:
            deadline, _, handle, callback = heapq.heappop(self._timers)
            self._now = max(self._now, deadline)
            if not handle.cancelled:
                callback()
        self._now = target
