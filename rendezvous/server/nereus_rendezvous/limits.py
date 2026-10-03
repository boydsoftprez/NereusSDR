# no-port-check: NereusSDR-original.
"""Rate limits and address grouping.

Every per-address count and limit is kept per address group: an IPv4
address by itself (an IPv4-mapped IPv6 address counts as its IPv4
address), and any other IPv6 address by its /56 prefix. A /56 is what an
ISP commonly delegates to one household, so one customer can dial from
every address in it; counting by /64 would let a household (or a host that
owns a /56) multiply every limit by up to 256. Rendezvous document section
9.1.
"""

from __future__ import annotations

import collections
import ipaddress
from typing import Deque, Dict, Optional, Tuple


IPV6_GROUP_BITS = 56


def address_group(address: str) -> str:
    """The key an address is counted under. An unparsable address counts
    under itself, so it is still limited."""
    try:
        ip = ipaddress.ip_address(address.split("%", 1)[0])
    except ValueError:
        return address
    if isinstance(ip, ipaddress.IPv6Address):
        if ip.ipv4_mapped is not None:
            return str(ip.ipv4_mapped)
        net = ipaddress.IPv6Network((int(ip) >> (128 - IPV6_GROUP_BITS) << (128 - IPV6_GROUP_BITS), IPV6_GROUP_BITS))
        return str(net)
    return str(ip)


class WindowLimiter:
    """At most `limit` events per `window_ms` for each key, in a sliding
    window. A refused attempt is not counted. Keys whose events have all
    left the window are dropped, so the table stays bounded by the traffic
    of the last window."""

    def __init__(self, limit: int, window_ms: int) -> None:
        self.limit = int(limit)
        self.window_ms = int(window_ms)
        self._events: Dict[str, Deque[int]] = {}

    def _prune(self, key: str, now: int) -> Deque[int]:
        events = self._events.get(key)
        if events is None:
            return collections.deque()
        while events and events[0] <= now - self.window_ms:
            events.popleft()
        if not events:
            del self._events[key]
        return events

    def check(self, key: str, now: int) -> Optional[int]:
        """None when an event is allowed now, else the milliseconds until
        it would be (at least 1)."""
        if self.limit <= 0:
            return self.window_ms
        events = self._prune(key, now)
        if len(events) < self.limit:
            return None
        return max(1, events[0] + self.window_ms - now)

    def record(self, key: str, now: int) -> None:
        self._events.setdefault(key, collections.deque()).append(now)

    def sweep(self, now: int) -> None:
        for key in list(self._events):
            self._prune(key, now)

    def __len__(self) -> int:
        return len(self._events)


def first_allowed(now: int, *checks: Tuple[WindowLimiter, str]) -> Optional[int]:
    """Check several limiters; None when all allow, else the longest wait."""
    wait = None
    for limiter, key in checks:
        w = limiter.check(key, now)
        if w is not None:
            wait = w if wait is None else max(wait, w)
    return wait
