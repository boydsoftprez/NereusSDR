# no-port-check: NereusSDR-original.
"""Live readers (wire spec 3.2, 5.5, 6.11, 9.1): the watched band list and
watched rooms, sent as a station gets them with every `you` false and every
`mine` null; the watch cap; resync; and the live socket caps. In one
process with a ManualClock, so nothing sleeps."""

from __future__ import annotations

import pytest

from inproc import Harness, live, presence, reading, slice_
from nereus_roundtable import protocol

WATCH_REASON = "You can watch up to 4 frequencies at once."
NOT_FOUND_REASON = "That room, turn or message is not here."


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def _join(harness, callsign, hz=14212000, grid="EM73", address="192.0.2.1", mode="USB"):
    station = harness.sign_in(address=address)
    station.send(presence(callsign, grid, [slice_(0, hz, mode)]))
    return station


def _band(harness, band, address="198.51.100.1"):
    reader = live(harness, address)
    assert reader.take() == [], "a live reader is sent nothing until it asks"
    reader.send({"type": "watchBand", "band": band})
    got = reader.take()
    assert [m["type"] for m in got] == ["rooms"] and got[0]["gen"] == 1
    return reader, got[0]


def test_a_band_watcher_sees_a_room_open_within_one_delta_and_another_band_does_not(harness):
    twenty, _ = _band(harness, "20m")
    forty, _ = _band(harness, "40m", "198.51.100.2")
    every, _ = _band(harness, "all", "198.51.100.3")
    _join(harness, "K4ABC", hz=14212000)
    assert twenty.take() == [] and forty.take() == []
    harness.advance(10000)
    got = twenty.take()
    assert [m["type"] for m in got] == ["roomsDelta"]
    assert got[0]["gen"] == 2 and [s["id"] for s in got[0]["add"]] == ["usb-14212000"]
    assert got[0]["add"][0]["band"] == "20m" and got[0]["change"] == [] and got[0]["remove"] == []
    assert [s["id"] for s in every.take()[0]["add"]] == ["usb-14212000"]
    assert forty.take() == []
    # A 40 m room reaches the 40 m watcher only.
    _join(harness, "KG4VCF", hz=7188000, mode="LSB", address="192.0.2.2")
    harness.advance(10000)
    got = forty.take()
    assert [m["type"] for m in got] == ["roomsDelta"] and [s["id"] for s in got[0]["add"]] == ["lsb-7188000"]
    assert twenty.take() == []


def test_watch_band_lists_only_that_band_in_frequency_order_and_a_new_one_replaces_it(harness):
    _join(harness, "K4ABC", hz=14250000)
    _join(harness, "W4XYZ", hz=14212000, address="192.0.2.2")
    _join(harness, "KG4VCF", hz=7188000, mode="LSB", address="192.0.2.3")
    harness.advance(10000)
    reader, rooms = _band(harness, "20m")
    assert [s["id"] for s in rooms["rooms"]] == ["usb-14212000", "usb-14250000"]
    reader.send({"type": "watchBand", "band": "40m"})
    got = reader.take()
    assert [m["type"] for m in got] == ["rooms"] and got[0]["gen"] == 1
    assert [s["id"] for s in got[0]["rooms"]] == ["lsb-7188000"]
    # A 20 m change no longer reaches it.
    harness.advance(5000)
    _join(harness, "N0CALL1", hz=14300000, address="192.0.2.4")
    harness.advance(10000)
    assert reader.take() == []
    reader.send({"type": "watchBand", "band": "all"})
    assert len(reader.take()[0]["rooms"]) == 4
    reader.send({"type": "watchBand", "band": "other"})
    assert reader.take()[0]["rooms"] == []


def test_a_watched_room_is_the_stations_view_with_no_row_its_own(harness):
    a = _join(harness, "K4ABC", grid="EM73")
    b = _join(harness, "W4XYZ", grid="FN20", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    a.take()
    b.take()
    reader = live(harness, "198.51.100.1")
    reader.send({"type": "watch", "room": "usb-14212000"})
    got = reader.take()
    assert [m["type"] for m in got] == ["room"]
    view = got[0]
    assert view["gen"] == 1 and view["id"] == "usb-14212000" and view["band"] == "20m"
    assert [m["callsign"] for m in view["members"]] == ["K4ABC", "W4XYZ"]
    assert all(m["you"] is False for m in view["members"])
    # A keys: a talking turn, and B's reading of it, each a delta.
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    got = reader.take()
    assert [m["type"] for m in got] == ["delta"] and got[0]["gen"] == 2
    turn = got[0]["turns"][0]
    assert turn["kind"] == "talking" and turn["mine"] is None
    assert all(m["you"] is False for m in got[0]["members"])
    harness.advance(1000)
    b.send(reading(0, -91.5, harness.clock.wall_ms(), True))
    got = reader.take()
    assert [m["type"] for m in got] == ["delta"] and got[0]["gen"] == 3
    row = got[0]["members"][0]
    assert row["callsign"] == "W4XYZ" and row["reading"]["dbm"] == -91.5 and row["you"] is False
    # Station-only kinds never reach it.
    b.send({"type": "post", "room": "usb-14212000", "text": "K4ABC good signal", "clientId": "c1"})
    got = reader.take()
    assert [m["type"] for m in got] == ["delta"] and got[0]["messages"][0]["text"] == "K4ABC good signal"


def test_a_named_turn_shows_its_names_but_no_mine_to_a_live_reader(harness):
    a = _join(harness, "K4ABC")
    b = _join(harness, "W4XYZ", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    reader = live(harness, "198.51.100.1")
    reader.send({"type": "watch", "room": "usb-14212000"})
    reader.take()
    a.send(reading(0, -80.0, harness.clock.wall_ms(), True))
    harness.advance(4000)
    turn_id = harness.service.book.earlier["usb-14212000"][0].id
    reader.take()
    b.take()
    b.send({"type": "name", "turn": turn_id, "name": "N0CALL"})
    mine = [m for m in b.take() if m["type"] == "delta"]
    assert mine and mine[0]["turns"][0]["mine"] == "N0CALL"
    got = reader.take()
    assert [m["type"] for m in got] == ["delta"]
    turn = got[0]["turns"][0]
    assert turn["names"] == [{"name": "N0CALL", "count": 1}] and turn["mine"] is None


def test_a_fifth_watch_is_refused_and_unwatch_frees_a_place(harness):
    for i in range(5):
        _join(harness, f"K{i}ABC", hz=14200000 + 5000 * i, address=f"192.0.2.{i + 1}")
    harness.advance(10000)
    reader = live(harness, "198.51.100.1")
    for i in range(4):
        reader.send({"type": "watch", "room": f"usb-{14200000 + 5000 * i}"})
        assert [m["type"] for m in reader.take()] == ["room"]
    reader.send({"type": "watch", "room": "usb-14220000"})
    assert reader.take() == [
        {"type": "error", "code": "not-allowed", "reason": WATCH_REASON, "about": "watch", "retryAfterMs": 0}
    ]
    # Watching one already watched again is not a fifth: its view comes again.
    reader.send({"type": "watch", "room": "usb-14200000"})
    got = reader.take()
    assert [m["type"] for m in got] == ["room"] and got[0]["gen"] == 1
    reader.send({"type": "unwatch", "room": "usb-14200000"})
    assert reader.take() == []
    reader.send({"type": "watch", "room": "usb-14220000"})
    assert [m["type"] for m in reader.take()] == ["room"]


def test_an_unwatched_room_sends_nothing_more(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    reader = live(harness, "198.51.100.1")
    reader.send({"type": "watch", "room": "usb-14212000"})
    reader.take()
    reader.send({"type": "unwatch", "room": "usb-14212000"})
    # Unwatching what is not watched is answered by nothing too.
    reader.send({"type": "unwatch", "room": "usb-7000000"})
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    assert reader.take() == []


def test_watch_of_no_live_room_is_not_found(harness):
    _join(harness, "K4ABC")
    reader = live(harness, "198.51.100.1")
    # A candidate has no view (6.3).
    reader.send({"type": "watch", "room": "usb-14212000"})
    assert reader.take() == [
        {"type": "error", "code": "not-found", "reason": NOT_FOUND_REASON, "about": "watch", "retryAfterMs": 0}
    ]


def test_resync_answers_the_watched_band_or_room_and_not_found_otherwise(harness):
    _join(harness, "K4ABC")
    harness.advance(10000)
    reader = live(harness, "198.51.100.1")
    reader.send({"type": "resync", "room": None})
    assert reader.take()[0]["code"] == "not-found"
    reader.send({"type": "resync", "room": "usb-14212000"})
    got = reader.take()
    assert got[0]["code"] == "not-found" and got[0]["about"] == "resync"
    reader.send({"type": "watchBand", "band": "20m"})
    reader.take()
    reader.send({"type": "watch", "room": "usb-14212000"})
    reader.take()
    reader.send({"type": "resync", "room": None})
    got = reader.take()
    assert [m["type"] for m in got] == ["rooms"] and got[0]["gen"] == 1 and len(got[0]["rooms"]) == 1
    reader.send({"type": "resync", "room": "usb-14212000"})
    got = reader.take()
    assert [m["type"] for m in got] == ["room"] and got[0]["gen"] == 1


def test_a_closing_room_ends_the_watch_and_leaves_the_band_list(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    reader, _ = _band(harness, "20m")
    reader.send({"type": "watch", "room": "usb-14212000"})
    reader.take()
    a.send(presence("K4ABC", "EM73", []))
    got = reader.take()
    assert [m["type"] for m in got] == ["delta"] and got[0]["left"]
    harness.advance(2000)
    got = reader.take()
    assert [m["type"] for m in got] == ["roomsDelta"] and got[0]["change"][0]["members"] == 0
    harness.advance(598000)
    got = reader.take()
    assert [m["type"] for m in got] == ["roomsDelta", "delta"]
    assert got[0]["remove"] == ["usb-14212000"] and got[1]["closed"] is True
    assert reader.conn not in harness.service._viewers.get("usb-14212000", set())
    # Its watch place is free again and the room is gone.
    reader.send({"type": "resync", "room": "usb-14212000"})
    assert reader.take()[0]["code"] == "not-found"
    assert reader.conn.watched == set()


def test_band_changes_to_a_live_reader_are_paced_to_two_seconds(harness):
    _join(harness, "K4ABC")
    harness.advance(10000)
    reader, _ = _band(harness, "20m")
    # B becomes a member less than 2 s after the band list went.
    harness.advance(1000)
    b = harness.sign_in(address="192.0.2.2")
    b.send(presence("W4XYZ", "EM73", [slice_(0, 14212040, keyed=True)]))
    assert not [m for m in b.take() if m["type"] == "error"]
    assert reader.take() == []
    harness.advance(999)
    assert reader.take() == []
    harness.advance(1)
    got = reader.take()
    assert [m["type"] for m in got] == ["roomsDelta"] and got[0]["gen"] == 2
    assert got[0]["change"][0]["members"] == 2 and got[0]["change"][0]["state"] == "talking"


def test_a_live_reader_sees_a_hidden_stations_row_leave(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    reader = live(harness, "198.51.100.1")
    reader.send({"type": "watch", "room": "usb-14212000"})
    ref = reader.take()[0]["members"][0]["ref"]
    a.send({"type": "hidden"})
    got = reader.take()
    assert [m["type"] for m in got] == ["delta"] and got[0]["left"] == [ref]


def test_a_fifth_live_socket_from_one_group_and_the_501st_in_all_are_refused(harness):
    readers = [live(harness, "203.0.113.9") for _ in range(4)]
    assert all(not r.conn.closing for r in readers)
    fifth = live(harness, "203.0.113.9")
    assert fifth.take() == [
        {
            "type": "error",
            "code": "rate",
            "reason": "Too many connections from this network. Try again shortly.",
            "about": None,
            "retryAfterMs": 5000,
        }
    ]
    assert fifth.closed == protocol.CLOSE_TRY_LATER
    # 4 from each of 124 more groups: 500 in all with the first 4.
    for group in range(124):
        for _ in range(4):
            reader = live(harness, f"2001:db8:{group:x}::1")
            assert not reader.conn.closing
    assert harness.service.live_count == 500
    busy = live(harness, "198.51.100.200")
    assert busy.take() == [
        {"type": "error", "code": "rate", "reason": "Roundtable is busy. Try again shortly.", "about": None, "retryAfterMs": 5000}
    ]
    assert busy.closed == protocol.CLOSE_TRY_LATER
    # One leaving makes room for one more.
    readers[0].disconnect()
    assert not live(harness, "198.51.100.200").conn.closing


def test_a_live_reader_leaving_drops_its_views(harness):
    _join(harness, "K4ABC")
    harness.advance(10000)
    reader, _ = _band(harness, "all")
    reader.send({"type": "watch", "room": "usb-14212000"})
    reader.take()
    reader.disconnect()
    assert reader.conn not in harness.service._viewers.get("usb-14212000", set())
    assert harness.service.live_count == 0
