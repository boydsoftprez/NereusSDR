# no-port-check: NereusSDR-original.
"""Nothing public carries a station id, a key id or an address (wire spec
3.2, 4.4, 10.1; design 12.1).

Every turn fixture is played again with live readers watching every band
and every room, and with the read API asked for every room, its messages
and every callsign after each step; a scenario adds website-key stations,
messages, mentions, names, a closed room and a hidden station. Every value
and key of every live message and API answer is walked; the test fails if
one equals a station id, a key id, a public key, a website key or an
address any connection used, or holds a station id."""

from __future__ import annotations

import json
from typing import Any, Dict, Iterable, List, Set
from urllib.parse import parse_qs

import pytest

from cryptography.hazmat.primitives.asymmetric import ec

from inproc import Harness, LiveReader, Station, presence, reading, slice_
from nereus_roundtable import identity
from nereus_roundtable.http import App, Request, parse_head, split_path
from runner import load_fixture, load_manifest
from test_turns import Generations, TurnRunner

TURNS = [f for f in load_manifest()["fixtures"] if f["kind"] == "turns"]
LIVE_ADDRESS = "198.51.100.{}"
API_ADDRESS = "203.0.113.77"


class PublicRecorder:
    """Live readers on every band and room, the read API on everything,
    and every public value they were given."""

    def __init__(self, harness: Harness, callsigns: Iterable[str]) -> None:
        self.harness = harness
        harness.config.api_requests_per_minute = 1000000
        self.app = App(harness.service, harness.store, harness.config)
        self.callsigns = sorted(set(callsigns))
        self.readers: List[LiveReader] = []
        self.gens: Dict[int, Generations] = {}
        self.rooms_seen: Set[str] = set()
        self.public: List[Any] = []
        self.addresses: Set[str] = {API_ADDRESS}
        band = self._reader()
        band.send({"type": "watchBand", "band": "all"})
        for band_name in ("20m", "40m"):
            other = self._reader()
            other.send({"type": "watchBand", "band": band_name})

    def _reader(self) -> LiveReader:
        address = LIVE_ADDRESS.format(len(self.readers) + 1)
        self.addresses.add(address)
        reader = LiveReader(self.harness, address)
        self.readers.append(reader)
        self.gens[id(reader)] = Generations()
        return reader

    def _api(self, target: str) -> None:
        head = f"GET {target} HTTP/1.1\r\nHost: x\r\n\r\n".encode("ascii")
        method, path, query, headers = parse_head(head)
        req = Request(method, path, split_path(path) or [], parse_qs(query, keep_blank_values=True), headers, b"", API_ADDRESS)
        response = self.harness.loop.run_until_complete(self.app.answer(req))
        assert response.status in (200, 404), (target, response.status, response.body[:200])
        self.public.append(json.loads(response.body))

    def drain(self) -> None:
        book = self.harness.service.book
        watched = {room for r in self.readers for room in r.conn.watched}
        for room in book.live_rooms():
            self.rooms_seen.add(room.id)
            if room.id in watched:
                continue
            reader = next((r for r in self.readers if len(r.conn.watched) < 4), None) or self._reader()
            reader.send({"type": "watch", "room": room.id})
        for reader in self.readers:
            for msg in reader.take():
                assert msg["type"] != "error", msg
                self.gens[id(reader)].see(msg)
                self.public.append(msg)
        self._api("/v1/api/rooms")
        for room_id in sorted(self.rooms_seen):
            family, hz = room_id.split("-")
            self._api(f"/v1/api/room/{family}/{hz}")
            self._api(f"/v1/api/room/{family}/{hz}/messages")
        for callsign in self.callsigns:
            self._api("/v1/api/callsign/" + callsign.replace("/", "%2F"))


def secrets_of(harness: Harness, extra: Iterable[str] = ()) -> Set[str]:
    out = set(extra)
    for conn in list(harness.service.connections) + list(harness.service.stations.values()):
        if conn.station_id:
            out.add(conn.station_id)
    return out


def walk(value: Any, forbidden: Set[str], ids: Set[str], where: str = "") -> None:
    if isinstance(value, dict):
        for key, item in value.items():
            walk(key, forbidden, ids, where + "/key")
            walk(item, forbidden, ids, f"{where}/{key}")
    elif isinstance(value, list):
        for i, item in enumerate(value):
            walk(item, forbidden, ids, f"{where}[{i}]")
    elif isinstance(value, str):
        assert value not in forbidden, f"{where} is {value!r}"
        for sid in ids:
            assert sid not in value, f"{where} holds a station id: {value!r}"


class PublicTurnRunner(TurnRunner):
    def __init__(self, fixture: Dict[str, Any], name: str) -> None:
        super().__init__(fixture, name)
        self.recorder = PublicRecorder(self.harness, [e["callsign"] for e in self.setup.values()])
        self.ids = {s.station_id for s in self.stations.values()}
        self.addresses = {f"192.0.2.{i + 1}" for i in range(len(self.stations))}

    def drain(self) -> None:
        super().drain()
        self.recorder.drain()

    def run(self) -> None:
        try:
            super().run()
        finally:
            forbidden = self.ids | self.addresses | self.recorder.addresses
            assert self.recorder.public, "nothing public was recorded"
            walk(self.recorder.public, forbidden, self.ids, self.name)


@pytest.mark.parametrize("entry", TURNS, ids=[f["id"] for f in TURNS])
def test_turn_fixtures_publish_no_station_id_or_address(entry):
    runner = PublicTurnRunner(load_fixture(entry["file"]), entry["id"])
    runner.run()
    kinds = {m.get("type") for m in runner.recorder.public}
    assert {"rooms", "room", "delta"} <= kinds


def _key_station(harness: Harness, callsign: str, grid: str, address: str):
    raw = identity.new_key()
    station_id = identity.key_id(raw)
    now = harness.clock.wall_ms()
    harness.store.station_upsert(station_id, "key", callsign, grid, now)
    harness.store.key_add(identity.key_hash(raw), station_id, now)
    station = Station(harness, address)
    station.take()
    station.send({"type": "hello", "v": 1, "software": "Example Logger", "softwareVersion": "3.1", "key": identity.to_b64url(raw)})
    got = station.take()
    assert [m["type"] for m in got] == ["welcome", "rooms"] and got[0]["kind"] == "key"
    station.station_id = station_id
    return station, identity.to_b64url(raw)


def test_a_full_scenario_publishes_no_station_id_key_or_address():
    h = Harness()
    try:
        recorder = PublicRecorder(h, ["K4ABC", "W4XYZ", "VE3/KG4VCF/P", "KG4VCF"])
        keys = [ec.generate_private_key(ec.SECP256R1()) for _ in range(3)]
        spkis = {identity.to_b64url(identity.spki_of(k.public_key())) for k in keys}
        a = h.sign_in(keys[0], address="192.0.2.1")
        b, b_key = _key_station(h, "W4XYZ", "FN20", "2001:db8:5:6::7")
        c = h.sign_in(keys[1], address="::ffff:192.0.2.9")
        d = h.sign_in(keys[2], address="192.0.2.10")
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
        b.send(presence(None, "FN20", [slice_(0, 14212040)]))
        c.send(presence("VE3/KG4VCF/P", "FN03", [slice_(0, 7188000, "LSB"), slice_(1, 14212080)]))
        d.send(presence("KG4VCF", None, [slice_(0, 14212100)]))
        recorder.drain()
        h.advance(10000)
        recorder.drain()
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
        h.advance(1000)
        b.send(reading(0, -91.5, h.clock.wall_ms(), True))
        c.send(reading(1, -101.0, h.clock.wall_ms(), True))
        b.send({"type": "post", "room": "usb-14212000", "text": "K4ABC good copy, KG4VCF are you there?", "clientId": "c1"})
        recorder.drain()
        h.advance(2000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
        h.advance(4000)
        b.send(reading(0, -95.0, h.clock.wall_ms(), True))
        recorder.drain()
        h.advance(4000)
        unknown = [t for t in h.service.book.earlier.get("usb-14212000", []) if t.kind == "unknown"]
        if unknown:
            d.send({"type": "name", "turn": unknown[0].id, "name": "N0CALL"})
        d.send({"type": "post", "room": "usb-14212000", "text": "here", "clientId": "c2"})
        d.send({"type": "delete", "msg": 2})
        recorder.drain()
        c.send({"type": "hidden"})
        recorder.drain()
        for s, callsign in ((a, "K4ABC"), (b, None), (d, "KG4VCF")):
            s.send(presence(callsign, None, []))
        recorder.drain()
        h.advance(600000)
        recorder.drain()
        assert h.service.book.live_rooms() == []
        ids = {a.station_id, b.station_id, c.station_id, d.station_id}
        addresses = {"192.0.2.1", "2001:db8:5:6::7", "::ffff:192.0.2.9", "192.0.2.9", "192.0.2.10", "2001:db8:5:6::/64"}
        forbidden = ids | addresses | recorder.addresses | {b_key} | spkis
        walk(recorder.public, forbidden, ids, "scenario")
        kinds = {m.get("type") for m in recorder.public}
        assert {"rooms", "roomsDelta", "room", "delta"} <= kinds
        texts = json.dumps(recorder.public, separators=(",", ":"))
        assert "good copy" in texts and '"live":false' in texts and "VE3/KG4VCF/P" in texts
    finally:
        h.close()


def test_the_walk_catches_a_station_id():
    with pytest.raises(AssertionError):
        walk([{"type": "delta", "x": "ref-abcdefghijklmnopqrstuvwxyz"}], set(), {"abcdefghijklmnopqrstuvwxyz"})
    with pytest.raises(AssertionError):
        walk({"rows": [{"addr": "192.0.2.1"}]}, {"192.0.2.1"}, set())
    walk({"rows": [{"callsign": "K4ABC"}]}, {"192.0.2.1"}, {"abcdefghijklmnopqrstuvwxyz"})
