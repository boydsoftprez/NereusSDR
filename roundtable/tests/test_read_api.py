# no-port-check: NereusSDR-original.
"""The read API (wire spec 10): rooms, one room live or closed, older
messages, a callsign's rows, its errors and its 60 a minute per address
group. Requests go through the app's own parser and dispatch in one
process with a ManualClock; test_http.py covers the sockets."""

from __future__ import annotations

import json
import sqlite3
from typing import Any, Dict, Optional, Tuple

import pytest

from inproc import Harness, presence, reading, slice_
from nereus_roundtable import protocol
from nereus_roundtable.http import App, Request, Response, parse_head, split_path
from nereus_roundtable.service import Service
from nereus_roundtable.store import Store
from urllib.parse import parse_qs

MINUTE = 60000


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def _app(harness):
    return App(harness.service, harness.store, harness.config)


def call(harness, app, target, method="GET", address="192.0.2.50") -> Tuple[Response, Optional[Dict[str, Any]]]:
    head = f"{method} {target} HTTP/1.1\r\nHost: roundtable.conformance.invalid\r\n\r\n".encode("ascii")
    m, path, query, headers = parse_head(head)
    request = Request(m, path, split_path(path) or [], parse_qs(query, keep_blank_values=True), headers, b"", address)
    response = harness.loop.run_until_complete(app.answer(request))
    body = json.loads(response.body) if response.body else None
    return response, body


def _join(harness, callsign, hz=14212000, grid="EM73", address="192.0.2.1", mode="USB"):
    station = harness.sign_in(address=address)
    station.send(presence(callsign, grid, [slice_(0, hz, mode)]))
    return station


def test_rooms_lists_every_live_room_in_frequency_order_with_time(harness):
    app = _app(harness)
    _join(harness, "K4ABC", hz=14250000)
    _join(harness, "KG4VCF", hz=7188000, mode="LSB", address="192.0.2.2")
    _join(harness, "W4XYZ", hz=14212000, address="192.0.2.3")
    harness.advance(10000)
    response, body = call(harness, app, "/v1/api/rooms")
    assert response.status == 200
    assert response.head("Content-Type") == "application/json; charset=utf-8"
    assert response.head("Cache-Control") == "no-store"
    assert response.head("Access-Control-Allow-Origin") == "*"
    assert body["time"] == harness.clock.wall_ms()
    assert [r["id"] for r in body["rooms"]] == ["lsb-7188000", "usb-14212000", "usb-14250000"]
    for summary in body["rooms"]:
        protocol.SUMMARY(summary, "summary")
    _, body = call(harness, app, "/v1/api/rooms?band=20m")
    assert [r["id"] for r in body["rooms"]] == ["usb-14212000", "usb-14250000"]
    _, body = call(harness, app, "/v1/api/rooms?band=other")
    assert body["rooms"] == []
    for bad in ("/v1/api/rooms?band=21m", "/v1/api/rooms?band=", "/v1/api/rooms?band=20m&band=40m"):
        response, body = call(harness, app, bad)
        assert response.status == 400 and body == {"error": "bad-request", "reason": "That request could not be read."}


def test_one_live_room_is_its_view_as_a_live_reader_gets_it(harness):
    app = _app(harness)
    a = _join(harness, "K4ABC")
    b = _join(harness, "W4XYZ", hz=14212040, grid="FN20", address="192.0.2.2")
    harness.advance(10000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    harness.advance(1000)
    b.send(reading(0, -91.5, harness.clock.wall_ms(), True))
    b.send({"type": "post", "room": "usb-14212000", "text": "good copy", "clientId": "c1"})
    response, body = call(harness, app, "/v1/api/room/usb/14212000")
    assert response.status == 200
    assert body["live"] is True and body["closed"] is None and body["time"] == harness.clock.wall_ms()
    room = body["room"]
    assert "type" not in room and "gen" not in room
    assert room["id"] == "usb-14212000" and room["band"] == "20m" and room["family"] == "usb"
    assert room["hz"] == 14212000 and room["opening"] == harness.service.book.live("usb-14212000").opening_id
    assert [m["callsign"] for m in room["members"]] == ["K4ABC", "W4XYZ"]
    assert all(m["you"] is False for m in room["members"])
    assert room["now"][0]["kind"] == "talking" and room["now"][0]["mine"] is None
    assert [m["text"] for m in room["messages"]] == ["good copy"]
    # It decodes as a live reader's `room` would.
    protocol.decode_object(dict(room, type="room", gen=1), "server", "live")


def test_a_closed_room_answers_its_latest_opening_and_never_404_once_it_existed(harness):
    app = _app(harness)
    a = _join(harness, "K4ABC")
    b = _join(harness, "W4XYZ", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    opening = harness.service.book.live("usb-14212000").opening_id
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    harness.advance(5000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    harness.advance(3000)
    b.send({"type": "post", "room": "usb-14212000", "text": "73", "clientId": "c1"})
    a.send(presence("K4ABC", "EM73", []))
    b.send(presence("W4XYZ", "EM73", []))
    harness.advance(600000)
    assert harness.service.book.live("usb-14212000") is None
    closed_at = harness.clock.wall_ms()
    response, body = call(harness, app, "/v1/api/room/usb/14212000")
    assert response.status == 200
    assert body["live"] is False and body["closed"] == closed_at
    room = body["room"]
    assert room["opening"] == opening and room["members"] == [] and room["now"] == []
    # The talking turn ended about 10 minutes before the close: in the window.
    assert [t["kind"] for t in room["earlier"]] == ["talking"]
    assert room["earlier"][0]["talkers"] == [{"ref": None, "callsign": "K4ABC", "nth": 1}]
    assert [m["text"] for m in room["messages"]] == ["73"] and room["messages"][0]["ref"] is None
    protocol.decode_object(dict(room, type="room", gen=1), "server", "live")
    # An hour later it answers the same: the window is the 30 minutes
    # before the close, not before now.
    harness.advance(3600000)
    response, later = call(harness, app, "/v1/api/room/usb/14212000")
    assert response.status == 200 and later["room"] == room and later["closed"] == closed_at


def test_a_closed_room_after_a_restart_is_rebuilt_from_the_store(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    store = Store(path)
    h = Harness(store=store)
    try:
        a = _join(h, "K4ABC")
        b = _join(h, "W4XYZ", hz=14212040, address="192.0.2.2")
        h.advance(10000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
        h.advance(1000)
        b.send(reading(0, -91.5, h.clock.wall_ms(), True))
        h.advance(4000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
        h.advance(3000)
        a.send(presence("K4ABC", "EM73", []))
        b.send(presence("W4XYZ", "EM73", []))
        h.advance(600000)
        before = json.loads(call(h, _app(h), "/v1/api/room/usb/14212000")[0].body)
        # A new service over the same file holds no turns in memory.
        fresh = Service(h.config, h.clock, store)
        app = App(fresh, store, h.config)
        response, after = call(h, app, "/v1/api/room/usb/14212000")
        assert response.status == 200
        assert after["room"]["earlier"] == before["room"]["earlier"]
        turn = after["room"]["earlier"][0]
        assert turn["kind"] == "talking" and turn["heard"] == 1 and turn["ending"] is False
    finally:
        h.close()
        store.close()


def test_unknown_and_malformed_rooms(harness):
    app = _app(harness)
    response, body = call(harness, app, "/v1/api/room/usb/14212000")
    assert response.status == 404 and body == {"error": "not-found", "reason": "That room, turn or message is not here."}
    for bad in ("/v1/api/room/xyz/14212000", "/v1/api/room/usb/0", "/v1/api/room/usb/014212000", "/v1/api/room/USB/14212000", "/v1/api/room/usb/1e6"):
        response, body = call(harness, app, bad)
        assert response.status == 400 and body["error"] == "bad-request", bad
    for missing in ("/v1/api/room/usb", "/v1/api/room/usb/14212000/other", "/v1/api/nothing", "/v1/api/rooms/"):
        response, body = call(harness, app, missing)
        assert response.status == 404 and body["error"] == "not-found", missing


def test_older_messages_newest_first_before_an_id_and_a_limit(harness):
    app = _app(harness)
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    ids = []
    for i in range(5):
        a.send({"type": "post", "room": "usb-14212000", "text": f"m{i}", "clientId": f"c{i}"})
        ids.append(next(m["msg"] for m in a.take() if m["type"] == "posted"))
        harness.advance(5000)
    _, body = call(harness, app, "/v1/api/room/usb/14212000/messages")
    assert [m["text"] for m in body["messages"]] == ["m4", "m3", "m2", "m1", "m0"]
    assert body["time"] == harness.clock.wall_ms()
    _, body = call(harness, app, f"/v1/api/room/usb/14212000/messages?before={ids[3]}&limit=2")
    assert [m["id"] for m in body["messages"]] == [ids[2], ids[1]]
    # The sender's row is still there: its ref goes with each message.
    assert all(m["ref"] is not None for m in body["messages"])
    for bad in ("?before=x", "?limit=0", "?limit=51", "?before=-1", "?limit=2.0", "?before=1&before=2"):
        response, body = call(harness, app, "/v1/api/room/usb/14212000/messages" + bad)
        assert response.status == 400, bad
    response, _ = call(harness, app, "/v1/api/room/usb/7000000/messages")
    assert response.status == 404
    response, _ = call(harness, app, "/v1/api/room/usb/0/messages")
    assert response.status == 400


def test_a_callsign_used_by_two_stations_gives_two_flat_rows(harness):
    app = _app(harness)
    _join(harness, "KG4VCF", hz=14212000)
    _join(harness, "KG4VCF", hz=7188000, mode="LSB", address="192.0.2.2")
    _join(harness, "VE3/KG4VCF/P", hz=14212040, address="192.0.2.3")
    _join(harness, "W4XYZ", hz=14212080, address="192.0.2.4")
    harness.advance(10000)
    response, body = call(harness, app, "/v1/api/callsign/KG4VCF")
    assert response.status == 200 and body["callsign"] == "KG4VCF"
    refs = {
        (row.room.id, row.callsign): row.ref
        for room in harness.service.book.live_rooms()
        for row in room.rows.values()
    }
    want = sorted(
        [
            {"callsign": "KG4VCF", "room": "lsb-7188000", "ref": refs[("lsb-7188000", "KG4VCF")]},
            {"callsign": "KG4VCF", "room": "usb-14212000", "ref": refs[("usb-14212000", "KG4VCF")]},
            {"callsign": "VE3/KG4VCF/P", "room": "usb-14212000", "ref": refs[("usb-14212000", "VE3/KG4VCF/P")]},
        ],
        key=lambda r: (r["room"], r["ref"]),
    )
    assert body["rows"] == want
    assert all(set(r) == {"callsign", "room", "ref"} for r in body["rows"])
    # The prefixed callsign, its slashes as %2F, matches on the base.
    _, encoded = call(harness, app, "/v1/api/callsign/ve3%2Fkg4vcf%2Fp")
    assert encoded["callsign"] == "VE3/KG4VCF/P" and encoded["rows"] == want
    response, _ = call(harness, app, "/v1/api/callsign/VE3/KG4VCF")
    assert response.status == 404
    for bad in ("/v1/api/callsign/KG-4VCF", "/v1/api/callsign/AB", "/v1/api/callsign/%2FKG4VCF"):
        response, body = call(harness, app, bad)
        assert response.status == 400, bad
    _, body = call(harness, app, "/v1/api/callsign/N0BODY")
    assert body["rows"] == []


def test_joining_rows_are_listed_too(harness):
    app = _app(harness)
    _join(harness, "K4ABC")
    harness.advance(10000)
    _join(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    _, body = call(harness, app, "/v1/api/callsign/KG4VCF")
    assert [r["room"] for r in body["rows"]] == ["usb-14212000"]


def test_sixty_requests_a_minute_per_address_group(harness):
    app = _app(harness)
    for _ in range(60):
        assert call(harness, app, "/v1/api/rooms", address="203.0.113.5")[0].status == 200
    harness.advance(1000)
    response, body = call(harness, app, "/v1/api/rooms", address="203.0.113.5")
    assert response.status == 429
    assert body == {"error": "rate", "reason": "Too many at once. Try again in a moment."}
    assert response.head("Retry-After") == "59"
    # The same /64 is the same group; another group is not held.
    assert call(harness, app, "/v1/api/rooms", address="203.0.113.6")[0].status == 200
    for _ in range(60):
        call(harness, app, "/v1/api/rooms", address="2001:db8:1:2::1")
    assert call(harness, app, "/v1/api/rooms", address="2001:db8:1:2:ffff::9")[0].status == 429
    harness.advance(MINUTE - 1000 - 1)
    assert call(harness, app, "/v1/api/rooms", address="203.0.113.5")[0].status == 429
    harness.advance(1)
    assert call(harness, app, "/v1/api/rooms", address="203.0.113.5")[0].status == 200


def test_only_get_and_head(harness):
    app = _app(harness)
    for method in ("POST", "PUT", "DELETE", "OPTIONS"):
        response, body = call(harness, app, "/v1/api/rooms", method=method)
        assert response.status == 405 and body == {"error": "not-allowed", "reason": "Only GET and HEAD are supported."}
        assert response.head("Allow") == "GET, HEAD"
    response, body = call(harness, app, "/v1/api/rooms", method="HEAD")
    assert response.status == 200 and json.loads(response.body)["rooms"] == []


def test_a_database_that_cannot_answer_is_503(harness):
    app = _app(harness)
    harness.store.db.close()
    response, body = call(harness, app, "/v1/api/room/usb/14212000")
    assert response.status == 503 and body == {"error": "server", "reason": "Roundtable can't answer right now."}
    harness.store.db = sqlite3.connect(":memory:")
