# no-port-check: NereusSDR-original.
"""The server in one process with no sockets, for tests of rooms, turns,
readings and views, for stations and live readers.

A station here is a real Connection whose transport is never written: the
test feeds it frames through the service's own reading path, one event per
frame as on a socket, and reads what the service queued for it. Time moves
only with a ManualClock, so nothing here sleeps.
"""

from __future__ import annotations

import asyncio
import json
from typing import Any, Dict, List, Optional

from cryptography.hazmat.primitives.asymmetric import ec

from nereus_roundtable import identity, protocol
from nereus_roundtable.clock import DEFAULT_WALL_START_MS, ManualClock
from nereus_roundtable.config import Config
from nereus_roundtable.service import LIVE, STATION, Connection, Service, _Close
from nereus_roundtable.store import Store

HOST = "roundtable.conformance.invalid"


class _NoTransport:
    async def send(self, text: str) -> None:  # pragma: no cover - never written
        raise AssertionError("the in-process harness reads the queue itself")

    async def close(self, code: int) -> None:  # pragma: no cover
        return None


class Station:
    """One station connection: send messages, take what arrived."""

    PATH = STATION

    def __init__(self, harness: "Harness", address: str) -> None:
        self.harness = harness
        self.conn = Connection(harness.service, _NoTransport(), self.PATH, address)
        self.closed: Optional[int] = None
        self.station_id = ""
        harness.service.connections.add(self.conn)
        with harness.service.event():
            harness.service._accept(self.conn)

    def send(self, message: Dict[str, Any]) -> None:
        service = self.harness.service
        if self.conn.closing:
            raise AssertionError("sent on a closed connection")
        with service.event():
            service._on_frame(self.conn, protocol.encode(message))
        service.frames_handled += 1

    def take(self) -> List[Dict[str, Any]]:
        """Every message queued for this station since the last take, each
        checked to decode as its receiver would."""
        out: List[Dict[str, Any]] = []
        queue = self.conn.queue
        while not queue.empty():
            item = queue.get_nowait()
            if isinstance(item, _Close):
                self.closed = item.code
                continue
            text, size = item
            self.conn.queued_bytes -= size
            decoded = protocol.decode(text, "server", self.PATH)
            raw = json.loads(text)
            assert decoded == raw, f"keys the receiver does not know: {text[:200]}"
            out.append(raw)
        return out

    def take_kinds(self) -> List[str]:
        return [m["type"] for m in self.take()]

    def disconnect(self) -> None:
        service = self.harness.service
        service.peer_closes += 1
        service.detach(self.conn)


class LiveReader(Station):
    """One /v1/live connection: nothing is sent to it until it asks."""

    PATH = LIVE


class Harness:
    def __init__(
        self,
        config: Optional[Config] = None,
        store: Optional[Store] = None,
        wall_start_ms: int = DEFAULT_WALL_START_MS,
    ) -> None:
        # asyncio.Queue on Python 3.9 binds the current loop when made.
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.config = config or Config()
        self.config.public_host = HOST
        self.config.ping_interval_ms = 0
        self.clock = ManualClock(wall_start_ms)
        self.store = store if store is not None else Store(":memory:")
        self.service = Service(self.config, self.clock, self.store)

    def close(self) -> None:
        self.service.shutdown()
        asyncio.set_event_loop(None)
        self.loop.close()

    def advance(self, ms: int) -> None:
        self.clock.advance(ms)

    def sign_in(self, key: Optional[ec.EllipticCurvePrivateKey] = None, address: str = "192.0.2.1", frequency_version: Optional[int] = None) -> Station:
        """A station signs in with a station key; welcome and rooms are
        taken."""
        key = key or ec.generate_private_key(ec.SECP256R1())
        station = Station(self, address)
        first = station.take()
        assert first and first[0]["type"] == "challenge", first
        nonce = identity.from_b64url(first[0]["nonce"])
        spki = identity.spki_of(key.public_key())
        sig = identity.sign_raw(key, identity.hello_transcript(HOST, nonce))
        station.send(
            {
                **({"frequencyVersion": frequency_version} if frequency_version is not None else {}),
                "type": "hello",
                "v": 1,
                "software": "test",
                "softwareVersion": "1",
                "spki": identity.to_b64url(spki),
                "sig": identity.to_b64url(sig),
            }
        )
        got = station.take()
        assert [m["type"] for m in got] == ["welcome", "rooms"], got
        station.station_id = got[0]["station"]
        station.welcome = got[0]  # type: ignore[attr-defined]
        station.rooms = got[1]  # type: ignore[attr-defined]
        return station


def live(harness: Harness, address: str = "192.0.2.1") -> LiveReader:
    return LiveReader(harness, address)


def presence(callsign: str, grid: Optional[str], slices: List[Dict[str, Any]], at_radio: bool = True) -> Dict[str, Any]:
    return {"type": "presence", "callsign": callsign, "grid": grid, "atRadio": at_radio, "slices": slices}


def slice_(slice_id: int, hz: int, mode: str = "USB", keyed: bool = False) -> Dict[str, Any]:
    return {"id": slice_id, "hz": hz, "mode": mode, "filterHz": 2700, "keyed": keyed}


def reading(slice_id: int, dbm: float, t: int, present: bool = True) -> Dict[str, Any]:
    return {"type": "reading", "slice": slice_id, "dbm": dbm, "t": t, "present": present}
