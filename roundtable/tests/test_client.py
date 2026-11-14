# no-port-check: NereusSDR-original.
"""A Roundtable station over a real WebSocket, as other software would
write one (design 7.3, D22 to D24; wire spec 5.3).

It signs in with a website key from the key page, or with a station key
as a Core does, and then sends presence, readings and keyed state, posts,
deletes, names turns and reports. It keeps every message the server sent
and the views they build (the band list and each room's members, turns and
messages, wire spec 6.11), so a test can wait for what a screen would show.

This module holds no tests; test_e2e_key_client.py drives it against the
real server.
"""

from __future__ import annotations

import asyncio
import json
import time
from typing import Any, Callable, Dict, List, Optional

from cryptography.hazmat.primitives.asymmetric import ec
from websockets.exceptions import ConnectionClosed

from nereus_roundtable import identity, protocol
from runner import ws_connect

# Wire spec 6.5 and 9.1: presence at least 1100 ms apart, readings at most
# 2 a second per slice; the client leaves a margin on both.
PRESENCE_SPACING_S = 1.1
READING_SPACING_S = 0.6
ANSWERS = ("error", "posted", "notice", "weblinkOk")


def wall_ms() -> int:
    return int(time.time() * 1000)


class RoomView:
    """One room as a receiver holds it: rows by ref, turns and messages by
    id."""

    def __init__(self, full: Dict[str, Any]) -> None:
        self.id = full["id"]
        self.gen = full["gen"]
        self.members: Dict[str, Dict[str, Any]] = {m["ref"]: m for m in full["members"]}
        self.turns: Dict[int, Dict[str, Any]] = {}
        for turn in full["earlier"] + full["now"]:
            self.turns[turn["id"]] = turn
        self.messages: Dict[int, Dict[str, Any]] = {m["id"]: m for m in full["messages"]}

    def apply(self, delta: Dict[str, Any]) -> None:
        if delta["gen"] != self.gen + 1:
            raise AssertionError(f"a gap in {self.id}: {self.gen} then {delta['gen']}")
        self.gen = delta["gen"]
        for member in delta["members"]:
            self.members[member["ref"]] = member
        for ref in delta["left"]:
            self.members.pop(ref, None)
        for turn in delta["turns"]:
            self.turns[turn["id"]] = turn
        for message in delta["messages"]:
            self.messages[message["id"]] = message

    def row(self, callsign: str) -> Optional[Dict[str, Any]]:
        for member in self.members.values():
            if member["callsign"] == callsign:
                return member
        return None

    def own_row(self) -> Optional[Dict[str, Any]]:
        for member in self.members.values():
            if member["you"]:
                return member
        return None

    def turns_of(self, kind: str) -> List[Dict[str, Any]]:
        return sorted((t for t in self.turns.values() if t["kind"] == kind), key=lambda t: t["id"])


class StationClient:
    """One station connection on /v1/station."""

    def __init__(
        self,
        uri: str,
        host: str,
        address: str,
        website_key: Optional[str] = None,
        station_key: Optional[ec.EllipticCurvePrivateKey] = None,
        software: str = "Example Logger",
        software_version: str = "3.1",
    ) -> None:
        if (website_key is None) == (station_key is None):
            raise ValueError("a station signs in with a website key or a station key")
        self.uri = uri
        self.host = host
        self.address = address
        self.website_key = website_key
        self.station_key = station_key
        self.software = software
        self.software_version = software_version
        self.ws: Any = None
        self.welcome: Dict[str, Any] = {}
        self.received: List[Dict[str, Any]] = []
        self.rooms: Dict[str, Dict[str, Any]] = {}
        self.views: Dict[str, RoomView] = {}
        self.closed_code: Optional[int] = None
        self._changed = asyncio.Event()
        self._reader: Optional[asyncio.Task] = None
        self._last_presence = 0.0
        self._last_reading: Dict[int, float] = {}

    # ----------------------------------------------------------- sign-in

    async def connect(self) -> Dict[str, Any]:
        """challenge, hello, then welcome and rooms (wire spec 7.1, 7.2)."""
        self.ws = await ws_connect(self.uri, self.address)
        challenge = json.loads(await asyncio.wait_for(self.ws.recv(), 5))
        assert challenge["type"] == "challenge", challenge
        hello: Dict[str, Any] = {
            "type": "hello",
            "v": 1,
            "software": self.software,
            "softwareVersion": self.software_version,
        }
        if self.website_key is not None:
            hello["key"] = self.website_key
        else:
            assert self.station_key is not None
            nonce = identity.from_b64url(challenge["nonce"])
            spki = identity.spki_of(self.station_key.public_key())
            hello["spki"] = identity.to_b64url(spki)
            hello["sig"] = identity.to_b64url(
                identity.sign_raw(self.station_key, identity.hello_transcript(self.host, nonce))
            )
        await self.ws.send(protocol.encode(hello))
        welcome = json.loads(await asyncio.wait_for(self.ws.recv(), 5))
        assert welcome["type"] == "welcome", welcome
        self.welcome = welcome
        self._reader = asyncio.ensure_future(self._read())
        await self.wait_for(lambda: self.rooms is not None and self.count("rooms") >= 1, "the band list")
        return welcome

    @property
    def station_id(self) -> str:
        return self.welcome["station"]

    async def close(self) -> None:
        if self.ws is not None:
            await self.ws.close()
        if self._reader is not None:
            try:
                await asyncio.wait_for(self._reader, 5)
            except (asyncio.TimeoutError, asyncio.CancelledError):
                self._reader.cancel()

    async def _read(self) -> None:
        try:
            async for text in self.ws:
                message = json.loads(text)
                # What the server sends decodes as its receiver would.
                decoded = protocol.decode(text, "server", "station")
                assert decoded == message, f"keys a station does not know: {text[:200]}"
                self.received.append(message)
                self._view(message)
                self._changed.set()
        except ConnectionClosed as exc:
            self.closed_code = exc.rcvd.code if exc.rcvd is not None else None
        finally:
            if self.closed_code is None and self.ws is not None and self.ws.close_code is not None:
                self.closed_code = self.ws.close_code
            self._changed.set()

    def _view(self, message: Dict[str, Any]) -> None:
        kind = message["type"]
        if kind == "rooms":
            self.rooms = {s["id"]: s for s in message["rooms"]}
        elif kind == "roomsDelta":
            for summary in message["add"] + message["change"]:
                self.rooms[summary["id"]] = summary
            for room_id in message["remove"]:
                self.rooms.pop(room_id, None)
        elif kind == "room":
            self.views[message["id"]] = RoomView(message)
        elif kind == "delta":
            view = self.views.get(message["room"])
            if view is not None:
                view.apply(message)
                if message["closed"]:
                    del self.views[message["room"]]

    # ----------------------------------------------------------- waiting

    def count(self, kind: str) -> int:
        return sum(1 for m in self.received if m["type"] == kind)

    async def wait_for(self, predicate: Callable[[], Any], what: str, timeout: float = 15.0) -> Any:
        """Wait until `predicate()` is true; returns its value."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while True:
            value = predicate()
            if value:
                return value
            remaining = deadline - loop.time()
            if remaining <= 0:
                raise AssertionError(f"timed out after {timeout} s waiting for {what}")
            self._changed.clear()
            try:
                await asyncio.wait_for(self._changed.wait(), min(remaining, 0.25))
            except asyncio.TimeoutError:
                pass

    def answers(self, since: int = 0) -> List[Dict[str, Any]]:
        """The answers to this station's own requests, from received[since:]."""
        return [m for m in self.received[since:] if m["type"] in ANSWERS]

    async def answer(self, since: int, what: str, timeout: float = 5.0) -> Dict[str, Any]:
        """The first answer after received[since]."""
        found = await self.wait_for(lambda: self.answers(since), what, timeout)
        return found[0]

    async def silent_answer(self, since: int) -> List[Dict[str, Any]]:
        """Every answer after received[since], once a request the server
        always answers has come back: a `read` of a room that is not live
        (`error` `not-found` about `read`). Requests answered by nothing on
        success leave only that error."""
        await self.send({"type": "read", "room": "usb-1"})
        await self.wait_for(
            lambda: any(m["type"] == "error" and m["about"] == "read" for m in self.received[since:]),
            "the read marker",
        )
        out = []
        for message in self.answers(since):
            if message["type"] == "error" and message["about"] == "read":
                break
            out.append(message)
        return out

    def view(self, room_id: str) -> Optional[RoomView]:
        return self.views.get(room_id)

    # ----------------------------------------------------------- sending

    async def send(self, message: Dict[str, Any]) -> int:
        """Sends one message; returns len(received) before it, for answer()."""
        mark = len(self.received)
        await self.ws.send(protocol.encode(message))
        return mark

    async def presence(
        self,
        callsign: Optional[str],
        grid: Optional[str],
        slices: List[Dict[str, Any]],
        at_radio: bool = True,
    ) -> int:
        wait = self._last_presence + PRESENCE_SPACING_S - time.monotonic()
        if wait > 0:
            await asyncio.sleep(wait)
        self._last_presence = time.monotonic()
        return await self.send(
            {"type": "presence", "callsign": callsign, "grid": grid, "atRadio": at_radio, "slices": slices}
        )

    async def reading(self, slice_id: int, dbm: float, present: bool) -> int:
        wait = self._last_reading.get(slice_id, 0.0) + READING_SPACING_S - time.monotonic()
        if wait > 0:
            await asyncio.sleep(wait)
        self._last_reading[slice_id] = time.monotonic()
        return await self.send({"type": "reading", "slice": slice_id, "dbm": dbm, "t": wall_ms(), "present": present})

    async def post(self, room: str, text: str, client_id: str) -> Dict[str, Any]:
        """Posts and returns the `posted` answer."""
        mark = await self.send({"type": "post", "room": room, "text": text, "clientId": client_id})
        found = await self.wait_for(
            lambda: [m for m in self.received[mark:] if m["type"] == "posted" and m["clientId"] == client_id],
            f"posted {client_id}",
        )
        return found[0]

    async def delete(self, msg_id: int) -> int:
        return await self.send({"type": "delete", "msg": msg_id})

    async def name(self, turn_id: int, name: str) -> int:
        return await self.send({"type": "name", "turn": turn_id, "name": name})

    async def report_message(self, msg_id: int, reason: Optional[str]) -> int:
        return await self.send({"type": "report", "target": "message", "msg": msg_id, "reason": reason})

    async def report_turn(self, turn_id: int, reason: Optional[str]) -> int:
        return await self.send({"type": "report", "target": "turn", "turn": turn_id, "reason": reason})

    async def report_station(self, room: str, ref: str, reason: Optional[str]) -> int:
        return await self.send({"type": "report", "target": "station", "room": room, "ref": ref, "reason": reason})


def usb_slice(hz: int, keyed: bool = False, slice_id: int = 0) -> Dict[str, Any]:
    return {"id": slice_id, "hz": hz, "mode": "USB", "filterHz": 2700, "keyed": keyed}
