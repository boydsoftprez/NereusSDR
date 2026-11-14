# no-port-check: NereusSDR-original.
"""A browser for the in-process server: requests straight into the HTTP
app's dispatch (no sockets), with the web routes added, and helpers that
write archive history into the store directly.

Time moves only with the harness's ManualClock.
"""

from __future__ import annotations

import json
from typing import Any, Dict, List, Optional, Sequence, Tuple
from urllib.parse import parse_qs, urlencode

from inproc import Harness, Station
from nereus_roundtable import identity, protocol
from nereus_roundtable.http import App, Request, Response, split_path
from nereus_roundtable.moderation import Moderation
from nereus_roundtable.rooms import band_of
from nereus_roundtable.web import Web

FORM = "application/x-www-form-urlencoded"
ADDRESS = "198.51.100.7"
_UNSET = object()


class Browser:
    def __init__(self, harness: Harness) -> None:
        self.harness = harness
        self.app = App(harness.service, harness.store, harness.config)
        self.web = Web(harness.service, harness.store, harness.config, self.app)
        self.moderation = Moderation(self.web, self.app)
        self.origin = f"https://{harness.config.public_host}"

    def request(
        self,
        method: str,
        target: str,
        headers: Optional[Dict[str, Any]] = None,
        body: bytes = b"",
        address: str = ADDRESS,
    ) -> Response:
        path, _, query = target.partition("?")
        hdrs: Dict[str, List[str]] = {}
        for name, value in (headers or {}).items():
            values = value if isinstance(value, list) else [value]
            hdrs.setdefault(name.lower(), []).extend(values)
        request = Request(
            method, path, split_path(path) or [], parse_qs(query, keep_blank_values=True), hdrs, body, address
        )
        return self.harness.loop.run_until_complete(self.app.answer(request))

    def get(self, target: str, cookie: Optional[str] = None) -> Response:
        return self.request("GET", target, {"Cookie": cookie} if cookie else None)

    def post(
        self,
        target: str,
        form: Dict[str, str],
        cookie: Optional[str] = None,
        origin: Any = _UNSET,
        address: str = ADDRESS,
        content_type: str = FORM,
    ) -> Response:
        headers: Dict[str, Any] = {"Content-Type": content_type}
        if origin is _UNSET:
            headers["Origin"] = self.origin
        elif origin is not None:
            headers["Origin"] = origin
        if cookie:
            headers["Cookie"] = cookie
        return self.request("POST", target, headers, urlencode(form).encode("utf-8"), address)

    # ----------------------------------------------------------- sessions

    def cookie_from(self, response: Response) -> str:
        """`rt_session=<token>` from a response's Set-Cookie."""
        value = response.head("Set-Cookie")
        assert value is not None, response.status
        return value.split(";", 1)[0]

    def csrf(self, cookie: str) -> str:
        me = self.get("/v1/me", cookie)
        assert me.status == 200, me.body
        return _json(me)["csrf"]

    def new_key(self, callsign: str = "W4XYZ", grid: str = "EM73", address: str = ADDRESS) -> str:
        r = self.post("/key/new", {"callsign": callsign, "grid": grid}, address=address)
        assert r.status == 200, r.body
        return _json(r)["key"]

    def key_cookie(self, key: str) -> str:
        r = self.post("/key/signin", {"key": key})
        assert r.status == 303, r.body
        return self.cookie_from(r)

    def link_cookie(self, station: Station) -> str:
        """Sign the browser in through the station's `weblink`."""
        url = weblink(station)
        r = self.get(url[len(self.origin) :])
        assert r.status == 303, r.body
        return self.cookie_from(r)


def _json(response: Response) -> Any:
    return json.loads(response.body.decode("utf-8"))


body_json = _json


def weblink(station: Station) -> str:
    station.send({"type": "weblink"})
    got = [m for m in station.take() if m["type"] == "weblinkOk"]
    assert len(got) == 1, got
    return got[0]["url"]


def sign_in_key(harness: Harness, key_text: str, address: str = "192.0.2.50") -> Station:
    """A station signs in with a website key; welcome and rooms are taken
    (or the close is recorded)."""
    station = Station(harness, address)
    first = station.take()
    assert first and first[0]["type"] == "challenge", first
    station.send({"type": "hello", "v": 1, "software": "test", "softwareVersion": "1", "key": key_text})
    got = station.take()
    if station.closed is None:
        assert [m["type"] for m in got] == ["welcome", "rooms"], got
        station.station_id = got[0]["station"]
    return station


# ----------------------------------------------------------- history


def pack(present: Sequence[bool], dbm: float = -90.0) -> Tuple[bytes, bytes]:
    """A track of one reading a second with these present flags."""
    n = len(present)
    dbm_bytes = bytes([int(dbm) + 160] * n)
    bits = bytearray((n + 7) // 8)
    for i, p in enumerate(present):
        if p:
            bits[i // 8] |= 1 << (i % 8)
    return dbm_bytes, bytes(bits)


def opening(harness: Harness, room_id: str = "usb-14212000") -> int:
    family, hz = protocol.room_id_center(room_id)
    return harness.store.opening_open(room_id, band_of(hz), family, hz, harness.clock.wall_ms())


def stored_turn(
    harness: Harness,
    opening_id: int,
    kind: str,
    talkers: List[Tuple[str, str]],
    start: int,
    end: int,
    readers: Sequence[Tuple[str, str, Sequence[bool]]] = (),
    away: Sequence[str] = (),
    km: Optional[int] = 500,
) -> int:
    """A turn written to the store as the rooms write one: opened, its
    tracks, then ended (which builds sessions)."""
    store = harness.store
    turn_id = store.turn_open(opening_id, kind, talkers, start)
    rows = []
    for sid, callsign, present in readers:
        dbm, bits = pack(present)
        rows.append((sid, callsign, 0, km, 0, len(present) - 1, dbm, bits, False, sid in away))
    store.tracks_write(turn_id, rows)
    store.turn_end(turn_id, end)
    return turn_id


def station_id_of(name: str) -> str:
    """A made-up station id for history written straight to the store."""
    return identity.key_id(name.encode("ascii").ljust(32, b"."))
