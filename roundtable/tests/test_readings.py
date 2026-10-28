# no-port-check: NereusSDR-original.
"""Readings (wire spec 5.3.3, 6.5 to 6.8): the refusals in their order,
the rate limit, clock skew, the counted slice, what a row shows, and the
tracks stored with a turn (one byte and one present bit a second)."""

from __future__ import annotations

import pytest

from inproc import Harness, presence, reading, slice_
from nereus_roundtable import grid, turns
from nereus_roundtable.turns import Reading, Track


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def _member(harness, callsign, hz=14212000, grid_="EM73", address="192.0.2.1", slices=None):
    s = harness.sign_in(address=address)
    s.send(presence(callsign, grid_, slices or [slice_(0, hz)]))
    return s


def _deltas(station):
    return [m for m in station.take() if m["type"] == "delta"]


def _row(delta, callsign):
    for m in delta["members"]:
        if m["callsign"] == callsign:
            return m
    raise AssertionError(f"no row for {callsign} in {delta}")


def test_a_reading_before_any_presence_is_an_unknown_slice(harness):
    a = harness.sign_in()
    a.send(reading(0, -91.5, harness.clock.wall_ms()))
    got = a.take()
    assert got[0]["code"] == "unknown-slice" and got[0]["about"] == "reading"


def test_a_reading_for_an_unlisted_slice_is_an_unknown_slice(harness):
    a = _member(harness, "K4ABC")
    a.send(reading(1, -91.5, harness.clock.wall_ms()))
    assert a.take()[0]["code"] == "unknown-slice"


def test_a_reading_on_a_keyed_slice_is_refused(harness):
    a = harness.sign_in()
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    a.take()
    a.send(reading(0, -91.5, harness.clock.wall_ms()))
    got = a.take()
    assert got[0]["code"] == "slice-keyed" and got[0]["about"] == "reading"


def test_a_reading_from_a_joining_slice_is_dropped_silently(harness):
    a = _member(harness, "K4ABC")
    b = _member(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    a.take()
    b.take()
    c = _member(harness, "W4XYZ", hz=14211900, address="192.0.2.3")
    c.take()
    a.take()
    c.send(reading(0, -91.5, harness.clock.wall_ms()))
    assert c.take() == [] and a.take() == []
    assert harness.service.book.counted_row(c.station_id, 0).reading is None


def test_readings_over_two_a_second_are_dropped_with_one_rate_error_a_minute(harness):
    a = _member(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    now = harness.clock.wall_ms
    a.send(reading(0, -91.0, now()))
    a.send(reading(0, -92.0, now()))
    a.take()
    a.send(reading(0, -93.0, now()))
    got = a.take()
    assert [m["type"] for m in got] == ["error"]
    assert got[0]["code"] == "rate" and got[0]["about"] == "reading" and got[0]["retryAfterMs"] == 1000
    row = harness.service.book.counted_row(a.station_id, 0)
    assert row.reading.dbm == -92.0
    # More over the limit in the same minute: dropped with no error.
    for _ in range(5):
        harness.advance(1000)
        for dbm in (-90.0, -89.0, -88.0):
            a.send(reading(0, dbm, now()))
        assert [m for m in a.take() if m["type"] == "error"] == []
        assert row.reading.dbm == -89.0
    for _ in range(11):
        # Its presence every 5 s keeps its rows in the room (6.4).
        harness.advance(5000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    a.take()
    for dbm in (-90.0, -89.0, -88.0):
        a.send(reading(0, dbm, now()))
    assert [m["code"] for m in a.take() if m["type"] == "error"] == ["rate"]


def test_the_limit_is_per_slice(harness):
    a = _member(harness, "K4ABC", slices=[slice_(0, 14212000), slice_(1, 7188000, "LSB")])
    harness.advance(10000)
    a.take()
    now = harness.clock.wall_ms()
    for s in (0, 0, 1, 1):
        a.send(reading(s, -91.0, now))
    assert [m for m in a.take() if m["type"] == "error"] == []


def test_a_reading_stamped_more_than_five_seconds_off_takes_its_arrival(harness):
    a = _member(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    row = harness.service.book.counted_row(a.station_id, 0)
    now = harness.clock.wall_ms()
    a.send(reading(0, -91.0, now - 6000))
    assert row.reading.t == now
    harness.advance(1000)
    now = harness.clock.wall_ms()
    a.send(reading(0, -91.0, now + 6000))
    assert row.reading.t == now
    harness.advance(1000)
    now = harness.clock.wall_ms()
    a.send(reading(0, -91.0, now - 4000))
    assert row.reading.t == now - 4000
    harness.advance(1000)
    now = harness.clock.wall_ms()
    a.send(reading(0, -91.0, now + 5000))
    assert row.reading.t == now + 5000


def test_only_the_first_listed_slice_in_a_room_produces_readings(harness):
    a = _member(harness, "K4ABC", slices=[slice_(0, 14212000), slice_(1, 14212500)])
    harness.advance(10000)
    a.take()
    row = harness.service.book.counted_row(a.station_id, 0)
    now = harness.clock.wall_ms()
    a.send(reading(1, -80.0, now))
    assert a.take() == [] and row.reading is None
    a.send(reading(0, -91.0, now))
    assert row.reading.dbm == -91.0
    assert [m["type"] for m in a.take()] == ["delta"]


def test_a_row_shows_its_reading_its_turn_and_the_distance_to_the_talker(harness):
    a = _member(harness, "K4ABC", grid_="EM73")
    b = _member(harness, "KG4VCF", hz=14212040, grid_="FN31", address="192.0.2.2")
    c = _member(harness, "W4XYZ", hz=14211900, grid_=None, address="192.0.2.3")
    harness.advance(10000)
    for s in (a, b, c):
        s.take()
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    delta = _deltas(b)[0]
    a.take()
    c.take()
    turn = delta["turns"][0]
    assert turn["kind"] == "talking" and turn["talkers"][0]["callsign"] == "K4ABC"
    assert _row(delta, "KG4VCF")["km"] == grid.distance_km("FN31", "EM73")
    assert _row(delta, "K4ABC")["km"] is None
    # W4XYZ has no grid, so its km stays null and its row is not resent.
    assert "W4XYZ" not in [m["callsign"] for m in delta["members"]]
    harness.advance(1000)
    now = harness.clock.wall_ms()
    b.send(reading(0, -91.5, now, True))
    delta = _deltas(a)[0]
    assert _row(delta, "KG4VCF")["reading"] == {"dbm": -91.5, "t": now, "present": True, "turn": turn["id"]}
    assert delta["turns"][0]["heard"] == 1
    c.send(reading(0, -110.0, now, False))
    delta = _deltas(a)[0]
    # A reading with present false leaves heard at 1: the turn is not resent.
    assert delta["turns"] == []
    assert _row(delta, "W4XYZ")["reading"]["present"] is False


def test_dbm_bytes():
    assert turns.dbm_byte(-160) == 0
    assert turns.dbm_byte(-73) == 87
    assert turns.dbm_byte(94) == 254
    assert turns.dbm_byte(-91.5) == 69
    assert turns.dbm_byte(-91.4) == 69
    assert turns.dbm_byte(-91.6) == 68
    assert turns.NO_READING == 255


def test_a_missing_second_is_255_and_present_bits_follow_the_seconds():
    track = Track("a", "K4ABC")
    start = 1800000000000
    track.add(Reading(start + 0, -73, start, True))
    track.add(Reading(start + 2000, -160, start, False))
    track.add(Reading(start + 9500, 94, start, True))
    first, last, dbm, bits = track.pack(start)
    assert (first, last) == (0, 9)
    assert dbm == bytes([87, 255, 0, 255, 255, 255, 255, 255, 255, 254])
    assert bits == bytes([0b00000001, 0b00000010])


def test_a_two_minute_turn_of_one_station_stores_120_and_15_bytes(harness):
    a = _member(harness, "K4ABC")
    b = _member(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    turn = harness.service.book.live("usb-14212000").engine.open[0]
    store = harness.service.store
    for i in range(120):
        if i % 10 == 0:
            a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
        b.send(reading(0, -91.0, harness.clock.wall_ms(), i % 2 == 0))
        if i == 25:
            # Flushed every 30 s while the turn is open: at 30 s the turn
            # had the 20 seconds from 10 s.
            rows = store.tracks_of(turn.id)
            assert len(rows) == 1 and len(rows[0]["dbm_track"]) == 20
        harness.advance(1000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    harness.advance(3000)
    assert turn.end is not None
    rows = store.tracks_of(turn.id)
    assert len(rows) == 1
    row = rows[0]
    assert len(row["dbm_track"]) == 120 and len(row["present_track"]) == 15
    assert len(row["dbm_track"]) + len(row["present_track"]) == 135
    assert row["callsign"] == "KG4VCF" and row["offset_hz"] == 40
    assert row["km"] == 0 and (row["first_second"], row["last_second"]) == (0, 119)
    assert row["present_track"][0] == 0b01010101
    stored = store.turns_of(store.opening_get(1)["opening_id"])
    assert stored[0]["kind"] == "talking" and stored[0]["end_ms"] == turn.end
