# no-port-check: NereusSDR-original.
"""Rate-limit windows and address groups (wire spec 9.1).

An address group is one IPv4 address (an IPv4-mapped IPv6 address counts as
its IPv4 address) or one IPv6 /64. Connections are counted per group at
connect, before sign-in. Groups live in memory only; nothing here is ever
written to a file or a log (wire spec 9.1, 9.4).
"""

from __future__ import annotations

import collections
import ipaddress
from typing import Deque, Dict, Optional

IPV6_GROUP_BITS = 64


class AddressGroups:
    """A count of open connections per address group."""

    def __init__(self) -> None:
        self._counts: Dict[str, int] = {}

    @staticmethod
    def group(addr: str) -> str:
        """The key an address is counted under. An address that does not
        parse counts under itself, so it is still limited."""
        try:
            ip = ipaddress.ip_address(addr.strip().strip("[]").split("%", 1)[0])
        except ValueError:
            return addr
        if isinstance(ip, ipaddress.IPv6Address):
            if ip.ipv4_mapped is not None:
                return str(ip.ipv4_mapped)
            shift = 128 - IPV6_GROUP_BITS
            return str(ipaddress.IPv6Network(((int(ip) >> shift) << shift, IPV6_GROUP_BITS)))
        return str(ip)

    def count(self, group: str) -> int:
        return self._counts.get(group, 0)

    def add(self, group: str) -> None:
        self._counts[group] = self._counts.get(group, 0) + 1

    def remove(self, group: str) -> None:
        left = self._counts.get(group, 0) - 1
        if left > 0:
            self._counts[group] = left
        else:
            self._counts.pop(group, None)

    def __len__(self) -> int:
        return len(self._counts)


class Window:
    """At most `count` accepted requests per `per_ms` for each key, in a
    sliding, half-open window: a request at time T counts those accepted
    after T - per_ms, so with 1 a second one exactly 1000 ms after the last
    is accepted and one at 999 ms is refused. A refused request is not
    counted. Keys whose requests have all left the window are dropped."""

    def __init__(self, count: int, per_ms: int) -> None:
        self.count = int(count)
        self.per_ms = int(per_ms)
        self._accepted: Dict[str, Deque[int]] = {}

    def _prune(self, key: str, now_ms: int) -> Optional[Deque[int]]:
        times = self._accepted.get(key)
        if times is None:
            return None
        while times and times[0] <= now_ms - self.per_ms:
            times.popleft()
        if not times:
            del self._accepted[key]
            return None
        return times

    def retry_after(self, key: str, now_ms: int) -> int:
        """0 when a request is allowed now, else the milliseconds until the
        oldest accepted one leaves the window (at least 1)."""
        times = self._prune(key, now_ms)
        if times is None or len(times) < self.count:
            return 0
        return max(1, times[0] + self.per_ms - now_ms)

    def allow(self, key: str, now_ms: int) -> bool:
        """Accept and count a request, or refuse it without counting."""
        if self.retry_after(key, now_ms):
            return False
        self._accepted.setdefault(key, collections.deque()).append(now_ms)
        return True

    def sweep(self, now_ms: int) -> None:
        for key in list(self._accepted):
            self._prune(key, now_ms)

    def __len__(self) -> int:
        return len(self._accepted)
