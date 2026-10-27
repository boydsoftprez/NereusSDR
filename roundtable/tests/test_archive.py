# no-port-check: NereusSDR-original.
"""The archive (design 4, 11.6, 11.7, C18, D10; wire spec 7.9): talking
turns grouped into sessions by the 10 minute gap, WATCHED from the
signal-present bits, AWAY, the hour histogram, and what never appears:
doubled and unknown turns, whatever their names, and readings a helper
stopped counting."""

from __future__ import annotations

import pytest

from inproc import Harness, presence, reading, slice_
from nereus_roundtable.rooms import band_of
from webclient import Browser, body_json, opening, stored_turn

MINUTE = 60000
HOUR = 3600000
DAY = 86400000
ROOM = "usb-14212000"


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


@pytest.fixture
def browser(harness):
    return Browser(harness)


def _talk(harness, oid, station, start, end, readers=(), away=(), callsign="W4XYZ"):
    return stored_turn(harness, oid, "talking", [(station.station_id, callsign)], start, end, readers, away)


def _sessions(browser, cookie):
    r = browser.get("/v1/me/sessions", cookie)
    assert r.status == 200
    return body_json(r)["sessions"]


def _detail(browser, cookie, session_id):
    r = browser.get(f"/v1/me/session/{session_id}", cookie)
    assert r.status == 200
    return body_json(r)


# ----------------------------------------------------------- refusals


def test_the_archive_needs_a_signed_in_browser(harness, browser):
    a = harness.sign_in()
    _talk(harness, opening(harness), a, 1000, 5000)
    (s,) = harness.store.sessions_of(a.station_id)
    for path in ("/v1/me", "/v1/me/sessions", f"/v1/me/session/{s['id']}", "/v1/me/histogram"):
        r = browser.get(path)
        assert r.status == 401 and body_json(r)["error"] == "signed-out", path


@pytest.mark.parametrize("query", ["days=0", "days=x", "days=36501", "days=1&days=2", "band=11m", "band="])
def test_a_histogram_query_that_cannot_be_read_is_refused(harness, browser, query):
    cookie = browser.link_cookie(harness.sign_in())
    r = browser.get(f"/v1/me/histogram?{query}", cookie)
    assert r.status == 400 and body_json(r)["error"] == "bad-request"


def test_doubled_turns_never_appear_and_nor_do_their_readings(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    b = harness.sign_in(address="192.0.2.2")
    c = harness.sign_in(address="192.0.2.3")
    oid = opening(harness)
    talking = _talk(harness, oid, a, 1000, 11000, [(c.station_id, "K4ABC", [True] * 10)])
    doubled = stored_turn(
        harness, oid, "doubled", [(a.station_id, "W4XYZ"), (b.station_id, "KG4VCF")], 12000, 32000,
        [(c.station_id, "K4ABC", [True] * 20)],
    )
    (session,) = harness.store.sessions_of(a.station_id)
    assert harness.store.session_turn_ids(session) == [talking]
    # Even a session row naming the doubled turn shows only talking turns.
    harness.store.db.execute(
        "UPDATE sessions SET turn_ids = ?, end_ms = ? WHERE id = ?", (f"{talking} {doubled}", 32000, session["id"])
    )
    cookie = browser.link_cookie(a)
    detail = _detail(browser, cookie, session["id"])
    assert [t["id"] for t in detail["turns"]] == [talking]
    (reader,) = detail["readers"]
    assert reader["watched"] == 10
    assert [t["turn"] for t in reader["tracks"]] == [talking]
    assert detail["session"]["turns"] == 1
    assert harness.store.sessions_of(b.station_id) == []


def test_a_named_unknown_turn_never_appears(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    c = harness.sign_in(address="192.0.2.3")
    oid = opening(harness)
    unknown = stored_turn(harness, oid, "unknown", [(a.station_id, "W4XYZ")], 1000, 21000, [(c.station_id, "K4ABC", [True] * 20)])
    # A reader names the unknown turn as this station.
    harness.store.turn_name(unknown, c.station_id, "W4XYZ", "K4ABC", 2000)
    assert harness.store.sessions_of(a.station_id) == []
    cookie = browser.link_cookie(a)
    assert _sessions(browser, cookie) == []
    assert body_json(browser.get("/v1/me/histogram", cookie))["hours"] == [0] * 24
    # A talking turn next to it shows alone.
    talking = _talk(harness, oid, a, 22000, 30000)
    (s,) = _sessions(browser, cookie)
    assert [t["id"] for t in _detail(browser, cookie, s["id"])["turns"]] == [talking]


def test_readings_a_helper_stopped_counting_are_left_out(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    c = harness.sign_in(address="192.0.2.3")
    d = harness.sign_in(address="192.0.2.4")
    oid = opening(harness)
    turn = _talk(harness, oid, a, 1000, 11000, [(c.station_id, "K4ABC", [True] * 10), (d.station_id, "N0CALL", [True] * 10)])
    harness.store.db.execute("UPDATE turn_readings SET not_counted = 1 WHERE turn_id = ? AND station_id = ?", (turn, d.station_id))
    cookie = browser.link_cookie(a)
    (s,) = _sessions(browser, cookie)
    assert s["reported"] == 1
    assert [r["callsign"] for r in _detail(browser, cookie, s["id"])["readers"]] == ["K4ABC"]


def test_a_station_s_own_readings_are_not_its_readers(harness, browser):
    a = harness.sign_in()
    oid = opening(harness)
    _talk(harness, oid, a, 1000, 11000, [(a.station_id, "W4XYZ", [True] * 10)])
    cookie = browser.link_cookie(a)
    (s,) = _sessions(browser, cookie)
    assert s["reported"] == 0
    assert _detail(browser, cookie, s["id"])["readers"] == []


# ----------------------------------------------------------- sessions


def test_three_turns_at_minutes_4_6_and_20_make_two_sessions(harness, browser):
    a = harness.sign_in()
    oid = opening(harness)
    t1 = _talk(harness, oid, a, 4 * MINUTE, 4 * MINUTE + 30000)
    t2 = _talk(harness, oid, a, 6 * MINUTE, 6 * MINUTE + 30000)
    t3 = _talk(harness, oid, a, 20 * MINUTE, 20 * MINUTE + 30000)
    sessions = _sessions(browser, browser.link_cookie(a))
    assert len(sessions) == 2
    newest, oldest = sessions
    assert (oldest["start"], oldest["end"], oldest["turns"]) == (4 * MINUTE, 6 * MINUTE + 30000, 2)
    assert (newest["start"], newest["end"], newest["turns"]) == (20 * MINUTE, 20 * MINUTE + 30000, 1)
    stored = sorted(harness.store.session_turn_ids(s) for s in harness.store.sessions_of(a.station_id))
    assert stored == [[t1, t2], [t3]]


def test_gaps_of_4_6_and_20_minutes_make_two_sessions(harness, browser):
    a = harness.sign_in()
    oid = opening(harness)
    start = 0
    ids = []
    for gap in (None, 4, 6, 20):
        start = start if gap is None else start + gap * MINUTE
        ids.append(_talk(harness, oid, a, start, start + 20000))
        start += 20000
    stored = sorted(harness.store.session_turn_ids(s) for s in harness.store.sessions_of(a.station_id))
    assert stored == [ids[:3], ids[3:]]
    assert [s["turns"] for s in _sessions(browser, browser.link_cookie(a))] == [1, 3]


def test_the_gap_is_under_10_minutes_from_the_session_s_end(harness):
    a = harness.sign_in()
    oid = opening(harness)
    _talk(harness, oid, a, 0, 10000)
    _talk(harness, oid, a, 10000 + 10 * MINUTE - 1, 10000 + 10 * MINUTE + 5000)
    assert len(harness.store.sessions_of(a.station_id)) == 1
    end = 10000 + 10 * MINUTE + 5000
    _talk(harness, oid, a, end + 10 * MINUTE, end + 10 * MINUTE + 5000)
    assert len(harness.store.sessions_of(a.station_id)) == 2


def test_a_session_is_one_room_id(harness):
    a = harness.sign_in()
    _talk(harness, opening(harness, "usb-14212000"), a, 0, 10000)
    _talk(harness, opening(harness, "lsb-7188000"), a, 20000, 30000)
    _talk(harness, opening(harness, "usb-14212000"), a, 40000, 50000)
    rooms = sorted((s["room_id"], len(harness.store.session_turn_ids(s))) for s in harness.store.sessions_of(a.station_id))
    assert rooms == [("lsb-7188000", 1), ("usb-14212000", 2)]


def test_turns_a_restart_ends_join_sessions(harness):
    a = harness.sign_in()
    oid = opening(harness)
    store = harness.store
    now = harness.clock.wall_ms()
    turn = store.turn_open(oid, "talking", [(a.station_id, "W4XYZ")], now + 1000)
    store.opening_touch(oid, now + 9000)
    store.openings_reload(now + 60000, 1800000)
    (s,) = store.sessions_of(a.station_id)
    assert store.session_turn_ids(s) == [turn]
    assert (s["start_ms"], s["end_ms"]) == (now + 1000, now + 9000)
    # A second reload adds nothing twice.
    store.openings_reload(now + 60000, 1800000)
    assert [store.session_turn_ids(x) for x in store.sessions_of(a.station_id)] == [[turn]]


def test_a_reader_present_for_50_of_120_seconds_watched_50(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    c = harness.sign_in(address="192.0.2.3")
    d = harness.sign_in(address="192.0.2.4")
    present = [i % 12 < 5 for i in range(120)]
    assert sum(present) == 50
    turn = _talk(harness, opening(harness), a, 1000, 121000, [(c.station_id, "K4ABC", present), (d.station_id, "N0CALL", [True] * 60)])
    cookie = browser.link_cookie(a)
    (s,) = _sessions(browser, cookie)
    assert s["reported"] == 2
    detail = _detail(browser, cookie, s["id"])
    assert [(r["callsign"], r["watched"]) for r in detail["readers"]] == [("N0CALL", 60), ("K4ABC", 50)]
    reader = detail["readers"][1]
    assert reader["km"] == 500 and reader["offsetHz"] == 0 and reader["away"] is False
    (track,) = reader["tracks"]
    assert (track["turn"], track["first"], track["last"]) == (turn, 0, 119)
    assert track["present"] == present
    assert track["dbm"] == [-90] * 120


def test_watched_adds_up_across_the_session_s_turns_and_away_is_kept(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    c = harness.sign_in(address="192.0.2.3")
    oid = opening(harness)
    _talk(harness, oid, a, 0, 30000, [(c.station_id, "K4ABC", [True] * 30)])
    _talk(harness, oid, a, 60000, 90000, [(c.station_id, "K4ABC", [True] * 20 + [False] * 10)], away=[c.station_id])
    cookie = browser.link_cookie(a)
    (s,) = _sessions(browser, cookie)
    (reader,) = _detail(browser, cookie, s["id"])["readers"]
    assert reader["watched"] == 50
    assert reader["away"] is True
    assert [t["away"] for t in reader["tracks"]] == [False, True]


def test_the_session_s_messages_are_the_room_s_in_its_time(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    c = harness.sign_in(address="192.0.2.3")
    oid = opening(harness)
    store = harness.store
    store.message_add(oid, ROOM, c.station_id, "K4ABC", 1, 500, "before", -90.0)
    inside = store.message_add(oid, ROOM, c.station_id, "K4ABC", 1, 2000, "good copy", -90.0)
    mine = store.message_add(oid, ROOM, a.station_id, "W4XYZ", 2, 3000, "thanks", None)
    store.message_add(oid, "lsb-7188000", c.station_id, "K4ABC", 1, 2500, "elsewhere", None)
    _talk(harness, oid, a, 1000, 10000)
    cookie = browser.link_cookie(a)
    (s,) = _sessions(browser, cookie)
    messages = _detail(browser, cookie, s["id"])["messages"]
    assert [(m["id"], m["text"], m["mine"]) for m in messages] == [(inside, "good copy", False), (mine, "thanks", True)]


def test_sessions_are_newest_first_with_their_band(harness, browser):
    a = harness.sign_in()
    _talk(harness, opening(harness, "usb-14212000"), a, 0, 10000)
    _talk(harness, opening(harness, "lsb-7188000"), a, 20000, 30000)
    got = _sessions(browser, browser.link_cookie(a))
    assert [(s["room"], s["family"], s["hz"], s["band"]) for s in got] == [
        ("lsb-7188000", "lsb", 7188000, band_of(7188000)),
        ("usb-14212000", "usb", 14212000, band_of(14212000)),
    ]


# ----------------------------------------------------------- histogram


def test_the_histogram_counts_stations_that_heard_the_station_by_hour(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    c = harness.sign_in(address="192.0.2.3")
    d = harness.sign_in(address="192.0.2.4")
    e = harness.sign_in(address="192.0.2.5")
    now = harness.clock.wall_ms()
    day = now - now % DAY - DAY
    oid = opening(harness)
    # Hour 5: two stations heard it (one in two turns), one never did.
    _talk(harness, oid, a, day + 5 * HOUR, day + 5 * HOUR + 10000, [(c.station_id, "K4ABC", [True] * 10), (e.station_id, "N0E", [False] * 10)])
    _talk(harness, oid, a, day + 5 * HOUR + 60000, day + 5 * HOUR + 70000, [(c.station_id, "K4ABC", [True] * 10), (d.station_id, "N0D", [True] * 10)])
    # Hour 17, on another band.
    _talk(harness, opening(harness, "lsb-7188000"), a, day + 17 * HOUR, day + 17 * HOUR + 10000, [(d.station_id, "N0D", [True] * 10)])
    # Hour 9, long ago.
    _talk(harness, oid, a, day - 40 * DAY + 9 * HOUR, day - 40 * DAY + 9 * HOUR + 10000, [(c.station_id, "K4ABC", [True] * 10)])
    cookie = browser.link_cookie(a)

    def hours(query=""):
        r = browser.get("/v1/me/histogram" + query, cookie)
        assert r.status == 200
        return body_json(r)

    every = hours()
    assert (every["days"], every["band"]) == (None, None)
    assert {h: n for h, n in enumerate(every["hours"]) if n} == {5: 2, 9: 1, 17: 1}
    month = hours("?days=30")
    assert month["days"] == 30
    assert {h: n for h, n in enumerate(month["hours"]) if n} == {5: 2, 17: 1}
    band = band_of(14212000)
    twenty = hours(f"?days=30&band={band}")
    assert twenty["band"] == band
    assert {h: n for h, n in enumerate(twenty["hours"]) if n} == {5: 2}


# ----------------------------------------------------------- end to end


def test_a_talker_s_turn_reaches_its_archive_through_the_rooms(harness, browser):
    talker = harness.sign_in(address="192.0.2.1")
    talker.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    reader = harness.sign_in(address="192.0.2.2")
    reader.send(presence("K4ABC", "FN20", [slice_(0, 14212040)]))
    harness.advance(10000)
    talker.send(presence("W4XYZ", "EM73", [slice_(0, 14212000, keyed=True)]))
    for _ in range(4):
        harness.advance(1000)
        reader.send(reading(0, -80.0, harness.clock.wall_ms()))
    harness.advance(1000)
    talker.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    harness.advance(4000)
    for s in (talker, reader):
        s.take()
    (row,) = harness.store.sessions_of(talker.station_id)
    cookie = browser.link_cookie(talker)
    (s,) = _sessions(browser, cookie)
    assert s["id"] == row["id"] and s["room"] == ROOM and s["turns"] == 1 and s["reported"] == 1
    (r,) = _detail(browser, cookie, s["id"])["readers"]
    assert r["callsign"] == "K4ABC"
    assert r["watched"] >= 1
    assert r["offsetHz"] == 40
    # The reader has no talking turn of its own.
    assert _sessions(browser, browser.link_cookie(reader)) == []
