# no-port-check: NereusSDR-original.
"""Rooms (wire spec 6.1 to 6.4, 6.8, 6.11): placing slices, joining,
leaving and closing, row numbering, and what each connection is sent:
full views, deltas, generations, roomsDelta pacing and resync."""

from __future__ import annotations

import pytest

from inproc import Harness, presence, reading, slice_
from nereus_roundtable import rooms
from nereus_roundtable.clock import ManualClock
from nereus_roundtable.config import Config
from nereus_roundtable.rooms import Profile, RoomBook
from nereus_roundtable.store import Store


@pytest.fixture
def book():
    store = Store(":memory:")
    yield RoomBook(ManualClock(), store)
    store.close()


def _profile(callsign="K4ABC"):
    return Profile(callsign, "EM73", False, "test", "1")


def _place(book, sid, hz, mode="USB", slice_id=0, keyed=False, callsign="K4ABC"):
    return book.place(sid, slice_(slice_id, hz, mode, keyed), _profile(callsign))


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def test_families_are_the_wire_spec_table():
    assert rooms.FAMILIES == {
        "LSB": ("lsb", 1500),
        "USB": ("usb", 1500),
        "DIGL": ("digl", 1500),
        "DIGU": ("digu", 1500),
        "AM": ("am", 3000),
        "SAM": ("am", 3000),
        "FM": ("fm", 6000),
        "CWL": ("cw", 300),
        "CWU": ("cw", 300),
        "FDVL": ("fdvl", 1500),
        "FDVU": ("fdvu", 1500),
    }


def test_opposite_sidebands_never_share_a_room(book):
    assert _place(book, "a", 7200000, "LSB") == "lsb-7200000"
    assert _place(book, "b", 7200000, "USB") == "usb-7200000"


def test_width_decides_sharing(book):
    assert _place(book, "a", 14200000) == "usb-14200000"
    assert _place(book, "b", 14201400) == "usb-14200000"
    assert _place(book, "c", 14201600) == "usb-14201600"


def test_the_edge_of_the_width_is_inside(book):
    assert _place(book, "a", 14200000) == "usb-14200000"
    assert _place(book, "b", 14201500) == "usb-14200000"
    assert _place(book, "c", 14201501) == "usb-14201501"


def test_a_slice_exactly_between_two_centers_joins_the_lower(book):
    assert _place(book, "a", 14200000) == "usb-14200000"
    assert _place(book, "b", 14202000) == "usb-14202000"
    assert _place(book, "c", 14201000) == "usb-14200000"


def test_sam_and_am_share_and_cw_is_narrow(book):
    assert _place(book, "a", 3885000, "AM") == "am-3885000"
    assert _place(book, "b", 3887000, "SAM") == "am-3885000"
    assert _place(book, "c", 7030000, "CWU") == "cw-7030000"
    assert _place(book, "d", 7030301, "CWL") == "cw-7030301"


def test_join_dwell_then_member(book):
    clock = book.clock
    _place(book, "a", 14200000)
    assert book.members("usb-14200000") == []
    clock.advance(9999)
    assert book.members("usb-14200000") == []
    assert book.live("usb-14200000") is None
    clock.advance(1)
    assert [r.station_id for r in book.members("usb-14200000")] == ["a"]
    assert book.live("usb-14200000") is not None


def test_keyed_is_a_member_at_once(book):
    book.apply_presence("a", _profile(), [slice_(0, 14200000, keyed=True)])
    assert [r.station_id for r in book.members("usb-14200000")] == ["a"]


def test_a_candidate_vanishes_when_its_slice_leaves(book):
    _place(book, "a", 14200000)
    book.leave("a", 0)
    assert "usb-14200000" not in book.rooms
    assert book.rooms_generation() == 0


def test_room_closes_ten_minutes_after_its_last_member(book):
    clock = book.clock
    _place(book, "a", 14200000)
    clock.advance(10000)
    gen = book.rooms_generation()
    room_gen = book.generation("usb-14200000")
    book.leave("a", 0)
    clock.advance(599999)
    assert book.live("usb-14200000") is not None
    clock.advance(1)
    assert book.live("usb-14200000") is None
    assert book.rooms_generation() == gen + 1
    room = book.rooms.get("usb-14200000")
    assert room is None or room.version > room_gen


def test_a_member_rejoining_keeps_the_room_and_reopening_reuses_the_id(book):
    clock = book.clock
    _place(book, "a", 14200000)
    clock.advance(10000)
    first = book.live("usb-14200000").opening_id
    book.leave("a", 0)
    clock.advance(300000)
    _place(book, "a", 14200000)
    clock.advance(10000)
    assert book.live("usb-14200000").opening_id == first
    book.leave("a", 0)
    clock.advance(600000)
    assert book.live("usb-14200000") is None
    _place(book, "a", 14200500)
    clock.advance(10000)
    # A new room centered on the slice: no old center to reuse here.
    assert book.live("usb-14200500") is not None
    book.leave("a", 0)
    clock.advance(600000)
    _place(book, "a", 14200000)
    clock.advance(10000)
    again = book.live("usb-14200000")
    assert again is not None and again.opening_id > first


def test_nth_takes_the_lowest_free_number_and_never_renumbers(book):
    for sid in ("a", "b", "c"):
        _place(book, sid, 14200000, callsign="W4XYZ")
    room = book.rooms["usb-14200000"]
    assert [room.rows[s].nth for s in ("a", "b", "c")] == [1, 2, 3]
    book.leave("a", 0)
    assert [room.rows[s].nth for s in ("b", "c")] == [2, 3]
    _place(book, "d", 14200000, callsign="W4XYZ")
    _place(book, "e", 14200000, callsign="W4XYZ")
    assert room.rows["d"].nth == 1 and room.rows["e"].nth == 4
    _place(book, "f", 14200000, callsign="K4ABC")
    assert room.rows["f"].nth == 1


def test_refs_are_eight_base32_characters_and_new_per_opening(book):
    clock = book.clock
    _place(book, "a", 14200000)
    room = book.rooms["usb-14200000"]
    candidate_ref = room.rows["a"].ref
    clock.advance(10000)
    ref = room.rows["a"].ref
    assert len(ref) == 8 and set(ref) <= set("abcdefghijklmnopqrstuvwxyz234567")
    assert ref != candidate_ref


def test_offsets_round_to_ten_hz_halves_away_from_zero():
    assert rooms.offset_hz(14212005, 14212000) == 10
    assert rooms.offset_hz(14211995, 14212000) == -10
    assert rooms.offset_hz(14212004, 14212000) == 0
    assert rooms.offset_hz(14211996, 14212000) == 0
    assert rooms.offset_hz(14212040, 14212000) == 40
    assert rooms.offset_hz(14211900, 14212000) == -100


def test_bands_include_their_edges():
    assert rooms.band_of(7000000) == "40m"
    assert rooms.band_of(7300000) == "40m"
    assert rooms.band_of(7300001) == "other"
    assert rooms.band_of(135700) == "2200m"
    assert rooms.band_of(1300000000) == "23cm"


def test_two_slices_of_one_station_make_one_row(book):
    book.apply_presence("a", _profile(), [slice_(0, 14200000), slice_(1, 14200500)])
    room = book.rooms["usb-14200000"]
    assert list(room.rows) == ["a"] and room.rows["a"].slice_id == 0
    assert book.counted_row("a", 1) is None
    # Listed the other way round, the other slice is counted.
    book.apply_presence("a", _profile(), [slice_(1, 14200500), slice_(0, 14200000)])
    assert room.rows["a"].slice_id == 1


def test_moving_out_of_the_width_leaves_and_places_again(book):
    book.apply_presence("a", _profile(), [slice_(0, 14200000)])
    book.clock.advance(10000)
    book.apply_presence("a", _profile(), [slice_(0, 14201500)])
    assert book.rooms["usb-14200000"].rows["a"].member
    book.apply_presence("a", _profile(), [slice_(0, 14201501)])
    assert "a" not in book.rooms["usb-14200000"].rows
    assert not book.rooms["usb-14201501"].rows["a"].member


# ---------------------------------------------------------------- views


def _join(harness, callsign, hz=14212000, grid="EM73", address="192.0.2.1"):
    station = harness.sign_in(address=address)
    station.send(presence(callsign, grid, [slice_(0, hz)]))
    return station


def test_room_opens_with_rooms_delta_then_room_view(harness):
    a = _join(harness, "K4ABC")
    assert a.take() == []
    harness.advance(10000)
    got = a.take()
    assert [m["type"] for m in got] == ["roomsDelta", "room"]
    assert got[0]["gen"] == 2 and [s["id"] for s in got[0]["add"]] == ["usb-14212000"]
    room = got[1]
    assert room["gen"] == 1 and room["members"][0]["you"] and not room["members"][0]["joining"]


def test_joining_a_live_room_gets_its_view_at_once_then_a_member_delta(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    b = _join(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    got = b.take()
    assert [m["type"] for m in got] == ["room"]
    rows = {m["callsign"]: m for m in got[0]["members"]}
    assert rows["KG4VCF"]["joining"] and rows["KG4VCF"]["you"] and rows["KG4VCF"]["offsetHz"] == 40
    seen = a.take()
    assert [m["type"] for m in seen] == ["delta"]
    assert seen[0]["gen"] == 2 and seen[0]["members"][0]["callsign"] == "KG4VCF"
    assert not seen[0]["members"][0]["you"]
    harness.advance(10000)
    delta = b.take()
    assert [m["type"] for m in delta] == ["roomsDelta", "delta"]
    assert delta[1]["gen"] == 2 and delta[1]["members"][0]["joining"] is False


def test_rooms_delta_is_paced_to_two_seconds(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    b = _join(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    b.take()
    a.take()
    # B becomes a member at 20000: the member count changes; the last band
    # message to A went at 10000, so it goes at once.
    harness.advance(10000)
    assert [m["type"] for m in a.take()] == ["roomsDelta", "delta"]
    # A change 1 s later is held to 2 s after the last roomsDelta.
    harness.advance(1000)
    b.send(reading(0, -91.5, harness.clock.wall_ms(), True))
    assert [m["type"] for m in a.take()] == ["delta"]
    harness.advance(999)
    assert a.take() == []
    harness.advance(1)
    got = a.take()
    assert [m["type"] for m in got] == ["roomsDelta"] and got[0]["gen"] == 4
    assert [s["id"] for s in got[0]["change"]] == ["usb-14212000"]


def test_a_closed_room_sends_closed_then_remove(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    watcher = harness.sign_in(address="192.0.2.2")
    watcher.send({"type": "read", "room": "usb-14212000"})
    assert [m["type"] for m in watcher.take()] == ["room"]
    a.send(presence("K4ABC", "EM73", []))
    got = watcher.take()
    assert [m["type"] for m in got] == ["delta"] and got[0]["left"]
    # The member count's change goes 2 s after the band list it got.
    harness.advance(2000)
    got = watcher.take()
    assert [m["type"] for m in got] == ["roomsDelta"] and got[0]["change"][0]["members"] == 0
    harness.advance(598000)
    got = watcher.take()
    assert [m["type"] for m in got] == ["roomsDelta", "delta"]
    assert got[0]["remove"] == ["usb-14212000"] and got[1]["closed"] is True
    # The view is gone with the room.
    watcher.send({"type": "resync", "room": "usb-14212000"})
    assert watcher.take()[0]["code"] == "not-found"


def test_reads_are_capped_at_four_besides_the_own_room(harness):
    for i in range(5):
        s = _join(harness, f"K{i}ABC", hz=14200000 + 5000 * i, address=f"192.0.2.{i + 1}")
    harness.advance(10000)
    s.take()
    for i in range(4):
        s.send({"type": "read", "room": f"usb-{14200000 + 5000 * i}"})
        assert [m["type"] for m in s.take()] == ["room"]
    # Reading its own room is not counted.
    s.send({"type": "read", "room": "usb-14220000"})
    assert [m["type"] for m in s.take()] == ["room"]
    other = _join(harness, "N4QQQ", hz=14300000, address="192.0.2.9")
    harness.advance(10000)
    s.take()
    s.send({"type": "read", "room": "usb-14300000"})
    got = s.take()
    assert got[0]["type"] == "error" and got[0]["code"] == "not-allowed" and got[0]["about"] == "read"
    s.send({"type": "unread", "room": "usb-14200000"})
    assert s.take() == []
    s.send({"type": "read", "room": "usb-14300000"})
    assert [m["type"] for m in s.take()] == ["room"]
    other.take()


def test_unread_drops_the_view_but_not_the_own_room(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    a.send({"type": "unread", "room": "usb-14212000"})
    assert a.take() == []
    b = _join(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    b.take()
    assert [m["type"] for m in a.take()] == ["delta"]


def test_a_skipped_generation_gets_nothing_until_resync(harness):
    harness.config.outbound_queue_messages = 3
    a = _join(harness, "K4ABC")
    b = _join(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    last = [m["gen"] for m in a.take() if m["type"] in ("room", "delta")][-1]
    b.take()
    t = harness.clock.wall_ms
    # A takes nothing while B's readings fill A's queue: a delta is dropped.
    for i in range(4):
        b.send(reading(0, -91.5 + i, t(), True))
        harness.advance(500)
    queued = [m for m in a.take() if m["type"] == "delta"]
    gens = [m["gen"] for m in queued]
    assert gens == list(range(last + 1, last + 1 + len(gens))) and len(gens) < 4
    # The next delta shows the gap, and then nothing more comes.
    harness.advance(500)
    b.send(reading(0, -80.0, t(), True))
    gap = [m for m in a.take() if m["type"] == "delta"]
    assert len(gap) == 1 and gap[0]["gen"] > gens[-1] + 1
    for _ in range(3):
        harness.advance(1000)
        b.send(reading(0, -79.0, t(), True))
        assert [m for m in a.take() if m["type"] == "delta"] == []
    a.send({"type": "resync", "room": "usb-14212000"})
    got = a.take()
    assert [m["type"] for m in got] == ["room"] and got[0]["gen"] == 1
    harness.advance(1000)
    b.send(reading(0, -78.0, t(), True))
    after = [m for m in a.take() if m["type"] == "delta"]
    assert [m["gen"] for m in after] == [2]


def test_resync_null_starts_the_band_list_again(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    a.send({"type": "resync", "room": None})
    got = a.take()
    assert [m["type"] for m in got] == ["rooms"] and got[0]["gen"] == 1
    assert [s["id"] for s in got[0]["rooms"]] == ["usb-14212000"]


def test_silence_for_thirty_seconds_takes_the_rows_out(harness):
    a = _join(harness, "K4ABC")
    harness.advance(10000)
    a.take()
    harness.advance(19999)
    assert harness.service.book.rows_of(a.station_id)
    harness.advance(1)
    assert harness.service.book.rows_of(a.station_id) == []
    assert not a.conn.closing
    # The next presence places the slice again, joining.
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    rows = harness.service.book.rows_of(a.station_id)
    assert len(rows) == 1 and not rows[0].member


def test_a_closed_connection_takes_its_rows_out(harness):
    a = _join(harness, "K4ABC")
    b = _join(harness, "KG4VCF", hz=14212040, address="192.0.2.2")
    harness.advance(10000)
    a.take()
    b.take()
    b.disconnect()
    got = a.take()
    assert [m["type"] for m in got] == ["delta"] and len(got[0]["left"]) == 1


def test_away_and_software_show_on_the_row(harness):
    a = harness.sign_in()
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)], at_radio=False))
    harness.advance(10000)
    room = [m for m in a.take() if m["type"] == "room"][0]
    row = room["members"][0]
    assert row["away"] is True and row["software"] == "test" and row["softwareVersion"] == "1"


def test_a_core_presence_without_a_callsign_is_refused(harness):
    a = harness.sign_in()
    a.send(presence(None, "EM73", [slice_(0, 14212000)]))
    got = a.take()
    assert got[0]["type"] == "error" and got[0]["code"] == "not-allowed" and got[0]["about"] == "presence"
    assert harness.service.book.rows_of(a.station_id) == []


def test_presence_over_its_limit_is_refused_with_retry(harness):
    a = harness.sign_in()
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    harness.advance(999)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14213000)]))
    got = a.take()
    assert got[0]["code"] == "rate" and got[0]["about"] == "presence" and got[0]["retryAfterMs"] == 1
    harness.advance(1)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14213000)]))
    assert a.take() == []


def test_summary_folds_an_all_away_room_after_two_hours():
    h = Harness(Config())
    try:
        a = h.sign_in()
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)], at_radio=False))
        h.advance(10000)
        room = h.service.book.live("usb-14212000")
        assert room.summary()["folded"] is False
        # A Core repeats its presence every 10 s (6.5).
        for _ in range(719):
            h.advance(10000)
            a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)], at_radio=False))
        h.advance(9999)
        assert room.summary()["folded"] is False
        h.advance(1)
        assert room.summary()["folded"] is True
        assert h.service.book.live("usb-14212000") is room
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)], at_radio=True))
        assert room.summary()["folded"] is False
    finally:
        h.close()


def _summary(station_msgs, room_id="usb-14212000"):
    for m in station_msgs:
        if m["type"] == "roomsDelta":
            for s in m["add"] + m["change"]:
                if s["id"] == room_id:
                    return s
    raise AssertionError(f"no summary of {room_id} in {station_msgs}")


def test_a_summary_carries_the_turn_the_strip_and_the_last_message(harness):
    grids = {"K4ABC": "EM73", "KG4VCF": "FN31", "W4XYZ": None, "N4NEAR": "EM74"}
    stations = {
        c: _join(harness, c, hz=14212000 + 10 * i, grid=g, address=f"192.0.2.{i + 1}")
        for i, (c, g) in enumerate(grids.items())
    }
    watcher = harness.sign_in(address="192.0.2.9")
    harness.advance(10000)
    for s in list(stations.values()) + [watcher]:
        s.take()
    quiet = harness.service.book.live("usb-14212000").summary()
    assert (quiet["start"], quiet["heard"], quiet["readings"], quiet["lastMessage"], quiet["ended"]) == (None, 0, [], None, None)
    a = stations["K4ABC"]
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    start = harness.clock.wall_ms()
    harness.advance(2000)
    got = _summary(watcher.take())
    assert got["state"] == "talking" and got["talkers"] == ["K4ABC"]
    assert (got["start"], got["heard"], got["readings"]) == (start, 0, [])
    now = harness.clock.wall_ms()
    stations["KG4VCF"].send(reading(0, -91.5, now, True))
    stations["W4XYZ"].send(reading(0, -110.0, now, False))
    stations["N4NEAR"].send(reading(0, -80.0, now, True))
    harness.advance(2000)
    got = _summary(watcher.take())
    # Nearest first, no grid last; the talker has no reading of its own turn.
    assert (got["start"], got["heard"], got["readings"]) == (start, 2, [-80.0, -91.5, -110.0])
    text = "x" * 79 + "é\U0001f600" + "y" * 10
    stations["KG4VCF"].send({"type": "post", "room": "usb-14212000", "text": text, "clientId": "c1"})
    msg_id = [m for m in stations["KG4VCF"].take() if m["type"] == "posted"][0]["msg"]
    harness.advance(2000)
    line = _summary(watcher.take())["lastMessage"]
    assert line == {"callsign": "KG4VCF", "nth": 1, "t": line["t"], "state": "shown", "text": "x" * 79 + "é"}
    stations["KG4VCF"].send({"type": "delete", "msg": msg_id})
    harness.advance(2000)
    line = _summary(watcher.take())["lastMessage"]
    assert (line["callsign"], line["state"], line["text"]) == ("KG4VCF", "deleted", None)
    # The turn ends 3 s after the talker unkeys and the last signal: `ended`.
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    harness.advance(10000)
    got = harness.service.book.live("usb-14212000").summary()
    assert got["state"] == "quiet" and got["start"] is None and got["readings"] == [] and got["heard"] == 0
    assert got["ended"] is not None and start < got["ended"] <= harness.clock.wall_ms()


def test_the_strip_holds_at_most_twelve_readings(harness):
    talker = _join(harness, "K4ABC", grid="EM73")
    others = [
        _join(harness, f"K{i}XYZ", hz=14212000 + 10 * (i + 1), grid="EM73", address=f"192.0.2.{i + 2}")
        for i in range(14)
    ]
    harness.advance(10000)
    talker.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    now = harness.clock.wall_ms()
    for i, s in enumerate(others):
        s.send(reading(0, -100.0 + i, now, True))
    summary = harness.service.book.live("usb-14212000").summary()
    assert summary["heard"] == 14
    # Same distance: callsign order, K0XYZ to K11XYZ (string order).
    order = sorted(range(14), key=lambda i: f"K{i}XYZ")[:12]
    assert summary["readings"] == [-100.0 + i for i in order]


def test_one_band_list_build_a_tick_for_six_hundred_stations(harness):
    """Wire spec 6.11 "Pacing": 600 stations in 150 rooms, every one
    sending a reading in each 2 s window: the band list is built once a
    tick and each station gets one roomsDelta a tick."""
    stations = []
    for i in range(600):
        s = harness.sign_in(address=f"192.0.{2 + i // 250}.{1 + i % 250}")
        hz = 14000000 + 5000 * (i // 4)
        keyed = i % 4 == 0
        s.send(presence(f"K{i}A", "EM73", [slice_(0, hz, keyed=keyed)]))
        stations.append((s, keyed))
    harness.advance(10000)
    assert len(harness.service.book.live_rooms()) == 150
    for s, _ in stations:
        s.take()
    service = harness.service
    for tick in range(3):
        builds = service.band_builds
        harness.advance(500)
        now = harness.clock.wall_ms()
        for s, keyed in stations:
            if not keyed:
                s.send(reading(0, -90.0 - tick, now, True))
        harness.advance(1500)
        assert service.band_builds == builds + 1
        for s, _ in stations:
            kinds = [m["type"] for m in s.take()]
            assert kinds.count("roomsDelta") == 1, kinds
