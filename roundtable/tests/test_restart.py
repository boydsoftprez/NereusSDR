# no-port-check: NereusSDR-original.
"""A restart (wire spec 6.3 "Restart", 6.6, 9.3): rooms active in the last
10 minutes come back with their ids, centers and opening ids, live and
empty, closing 10 minutes after the restart; older ones do not; turns left
open end."""

from __future__ import annotations

from inproc import Harness, presence, slice_
from nereus_roundtable.clock import DEFAULT_WALL_START_MS
from nereus_roundtable.store import Store

MINUTE = 60000


def _first_run(path):
    """Two rooms with a member each; then a clean shutdown at 20 s."""
    store = Store(path)
    h = Harness(store=store)
    a = h.sign_in(address="192.0.2.1")
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    b = h.sign_in(address="192.0.2.2")
    b.send(presence("KG4VCF", "EM73tu", [slice_(0, 7188000, "LSB")]))
    h.advance(10000)
    rooms = {r.id: (r.center, r.opening_id) for r in h.service.book.live_rooms()}
    h.advance(10000)
    end = h.clock.wall_ms()
    h.close()
    store.close()
    return rooms, end


def _second_run(path, wall):
    store = Store(path)
    return Harness(store=store, wall_start_ms=wall), store


def test_rooms_reload_five_minutes_later_with_their_ids(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    rooms, end = _first_run(path)
    assert set(rooms) == {"usb-14212000", "lsb-7188000"}
    h, store = _second_run(path, end + 5 * MINUTE)
    try:
        live = {r.id: (r.center, r.opening_id) for r in h.service.book.live_rooms()}
        assert live == rooms
        assert all(not r.rows for r in h.service.book.live_rooms())
        station = h.sign_in()
        assert [s["id"] for s in station.rooms["rooms"]] == ["lsb-7188000", "usb-14212000"]
        assert all(s["members"] == 0 for s in station.rooms["rooms"])
        # The close timer starts at the restart.
        h.advance(10 * MINUTE - 1)
        assert len(h.service.book.live_rooms()) == 2
        h.advance(1)
        assert h.service.book.live_rooms() == []
    finally:
        h.close()
        store.close()


def test_a_station_rejoining_a_reloaded_room_keeps_it_open(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    rooms, end = _first_run(path)
    h, store = _second_run(path, end + 5 * MINUTE)
    try:
        a = h.sign_in()
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212200)]))
        got = a.take()
        assert [m["type"] for m in got] == ["room"]
        assert got[0]["id"] == "usb-14212000" and got[0]["opening"] == rooms["usb-14212000"][1]
        for _ in range(70):
            h.advance(10000)
            a.send(presence("K4ABC", "EM73", [slice_(0, 14212200)]))
        assert [r.id for r in h.service.book.live_rooms()] == ["usb-14212000"]
    finally:
        h.close()
        store.close()


def test_rooms_eleven_minutes_old_do_not_reload(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    _, end = _first_run(path)
    h, store = _second_run(path, end + 11 * MINUTE)
    try:
        assert h.service.book.live_rooms() == []
        station = h.sign_in()
        assert station.rooms["rooms"] == []
        assert all(r["closed"] is not None for r in store.db.execute("SELECT closed FROM rooms"))
    finally:
        h.close()
        store.close()


def test_a_turn_left_open_by_a_crash_ends_at_the_restart(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    store = Store(path)
    h = Harness(store=store)
    a = h.sign_in()
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    for _ in range(4):
        h.advance(10000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    turn = h.service.book.live("usb-14212000").engine.open[0]
    # No shutdown: the process just stops.
    store.close()
    h.loop.close()
    store = Store(path)
    row = store.db.execute("SELECT end_ms FROM turns WHERE id = ?", (turn.id,)).fetchone()
    assert row["end_ms"] is None
    h2 = Harness(store=store, wall_start_ms=DEFAULT_WALL_START_MS + 2 * MINUTE)
    try:
        row = dict(store.db.execute("SELECT start_ms, end_ms FROM turns WHERE id = ?", (turn.id,)).fetchone())
        # It ends at the room's last recorded activity, the 30 s flush.
        assert row["end_ms"] == DEFAULT_WALL_START_MS + 30000
        assert [r.id for r in h2.service.book.live_rooms()] == ["usb-14212000"]
        # Ids keep increasing after the restart.
        b = h2.sign_in()
        b.send(presence("KG4VCF", "EM73", [slice_(0, 14212000, keyed=True)]))
        new = h2.service.book.live("usb-14212000").engine.open[0]
        assert new.id > turn.id
    finally:
        h2.close()
        store.close()


def test_a_talkers_row_number_survives_a_restart(tmp_path):
    """Two stations sign as K4ABC; the second (nth 2) talks. After a
    restart the closed room's earlier turn still names it the second."""
    from test_read_api import _app, call

    path = str(tmp_path / "rt.sqlite")
    store = Store(path)
    h = Harness(store=store)
    a = h.sign_in(address="192.0.2.1")
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    b = h.sign_in(address="192.0.2.2")
    b.send(presence("K4ABC", "EM73", [slice_(0, 14212010)]))
    h.advance(10000)
    rows = h.service.book.live("usb-14212000").rows
    assert sorted(r.nth for r in rows.values()) == [1, 2]
    b.send(presence("K4ABC", "EM73", [slice_(0, 14212010, keyed=True)]))
    h.advance(5000)
    b.send(presence("K4ABC", "EM73", [slice_(0, 14212010)]))
    h.advance(3000)
    a.send(presence("K4ABC", "EM73", []))
    b.send(presence("K4ABC", "EM73", []))
    h.advance(10 * MINUTE)
    assert h.service.book.live("usb-14212000") is None
    end = h.clock.wall_ms()
    h.close()
    store.close()
    h, store = _second_run(path, end + MINUTE)
    try:
        response, body = call(h, _app(h), "/v1/api/room/usb/14212000")
        assert response.status == 200
        assert [t["talkers"] for t in body["room"]["earlier"]] == [[{"ref": None, "callsign": "K4ABC", "nth": 2}]]
    finally:
        h.close()
        store.close()


def test_a_database_from_before_talker_row_numbers_gains_them(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    Store(path).close()
    import sqlite3

    db = sqlite3.connect(path)
    db.execute("ALTER TABLE turns DROP COLUMN talker_nth")
    db.execute("ALTER TABLE turns DROP COLUMN second_talker_nth")
    db.execute("INSERT INTO turns (opening_id, kind, talker_callsign, start_ms) VALUES (1, 'talking', 'K4ABC', 5)")
    db.commit()
    db.close()
    store = Store(path)
    try:
        assert "talker_nth" in store.columns()["turns"] and "second_talker_nth" in store.columns()["turns"]
        assert tuple(store.db.execute("SELECT talker_nth, second_talker_nth FROM turns").fetchone()) == (1, 1)
    finally:
        store.close()
