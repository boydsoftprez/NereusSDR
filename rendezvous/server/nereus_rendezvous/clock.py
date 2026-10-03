# no-port-check: NereusSDR-original.
"""Time for the service: a real clock, and a manual one the tests drive.

Every timer the service keeps (the handshake and idle timeouts, the
introduction and mailbox lifetimes) and every rate-limit window goes through
a clock, so the conformance sessions move time with {"advanceMs": N} and
never sleep.
"""

from __future__ import annotations

import asyncio
import heapq
import itertools
import time
from typing import Callable, List, Tuple


class TimerHandle:
    def __init__(self) -> None:
        self.cancelled = False
        self._inner = None

    def cancel(self) -> None:
        self.cancelled = True
        if self._inner is not None:
            self._inner.cancel()


class RealClock:
    """Monotonic milliseconds for timers and windows; wall seconds for TURN
    credential expiry."""

    def now_ms(self) -> int:
        return int(time.monotonic() * 1000)

    def wall_seconds(self) -> int:
        return int(time.time())

    def call_later(self, delay_ms: int, callback: Callable[[], None]) -> TimerHandle:
        handle = TimerHandle()

        def fire() -> None:
            if not handle.cancelled:
                callback()

        handle._inner = asyncio.get_running_loop().call_later(delay_ms / 1000.0, fire)
        return handle


class ManualClock:
    """A clock that moves only when advance() is called. Timers fire in
    deadline order, each with now_ms() at its own deadline."""

    def __init__(self, wall_start: int = 1800000000) -> None:
        self._now = 0
        self._wall_start = wall_start
        self._timers: List[Tuple[int, int, TimerHandle, Callable[[], None]]] = []
        self._seq = itertools.count()

    def now_ms(self) -> int:
        return self._now

    def wall_seconds(self) -> int:
        return self._wall_start + self._now // 1000

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
