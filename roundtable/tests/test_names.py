# no-port-check: NereusSDR-original.
"""Naming unknown turns (wire spec 5.3.6, 6.9): who may name, the refusals
in their order, one name per station, the counts and `mine` in the views,
the 24 hours, and that a name never moves a turn into any archive."""

from __future__ import annotations

import pytest
from cryptography.hazmat.primitives.asymmetric import ec

from inproc import Harness, presence, reading, slice_
from nereus_roundtable.store import Store

DAY = 86400000


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def _join(harness, callsign, n, hz=None, key=None, at_radio=True):
    s = harness.sign_in(key=key, address=f"192.0.2.{n}")
    s.callsign = callsign
    s.hz = hz or 14212000 + 10 * n
    s.send(presence(callsign, "EM73", [slice_(0, s.hz)], at_radio))
    return s


def _room_of(harness, *callsigns):
    out = [_join(harness, c, i + 1) for i, c in enumerate(callsigns)]
    harness.advance(10000)
    for s in out:
        s.take()
    return out


def _unknown_turn(harness, by):
    """An unknown turn: one present reading, quiet 3 s later. Returns its
    id."""
    by.send(reading(0, -90.0, harness.clock.wall_ms()))
    harness.advance(3000)
    rows = harness.store.db.execute("SELECT id FROM turns WHERE kind = 'unknown' ORDER BY id DESC").fetchall()
    return rows[0][0]


def _name(station, turn_id, name):
    station.send({"type": "name", "turn": turn_id, "name": name})
    return [m for m in station.take() if m["type"] == "error"]


def _code(errors):
    assert len(errors) == 1, errors
    assert errors[0]["about"] == "name"
    return errors[0]["code"]


def _turn_in(got, turn_id):
    turns = [t for m in got if m["type"] == "delta" for t in m["turns"] if t["id"] == turn_id]
    turns += [t for m in got if m["type"] == "room" for t in m["now"] + m["earlier"] if t["id"] == turn_id]
    return turns[-1] if turns else None


def test_every_name_is_kept_with_its_count_and_mine_is_each_station_s_own(harness):
    a, b, c, d = _room_of(harness, "K4ABC", "KG4VCF", "W4XYZ", "N4DEF")
    turn_id = _unknown_turn(harness, b)
    for s in (a, b, c, d):
        s.take()
    assert _name(a, turn_id, "W4XYZ") == []
    assert _name(b, turn_id, "w4xyz") == []
    assert _name(c, turn_id, "W4XYZ") == []
    d.send({"type": "name", "turn": turn_id, "name": "K4ABC"})
    got = d.take()
    assert got and all(m["type"] == "delta" for m in got)
    turn = _turn_in(got, turn_id)
    assert turn["names"] == [{"name": "W4XYZ", "count": 3}, {"name": "K4ABC", "count": 1}]
    assert turn["mine"] == "K4ABC"
    assert _turn_in(a.take(), turn_id)["mine"] == "W4XYZ"
    assert harness.store.name_counts(turn_id) == {"W4XYZ": 3, "K4ABC": 1}
    assert list(harness.store.name_counts(turn_id)) == ["W4XYZ", "K4ABC"]
    # A second name replaces the first.
    assert _name(d, turn_id, "W4XYZ") == []
    assert harness.store.name_counts(turn_id) == {"W4XYZ": 4}
    assert _turn_in(a.take(), turn_id)["names"] == [{"name": "W4XYZ", "count": 4}]
    # The same name again changes nothing anyone is sent.
    assert _name(d, turn_id, "W4XYZ") == [] and a.take() == []
    # A full view carries the names and the viewer's own.
    c.take()
    c.send({"type": "resync", "room": harness.store.db.execute("SELECT room_id FROM rooms").fetchone()[0]})
    views = [m for m in c.take() if m["type"] == "room"]
    assert len(views) == 1 and _turn_in(views, turn_id)["mine"] == "W4XYZ"
    # Ties are by name.
    harness.store.db.execute("DELETE FROM turn_names WHERE turn_id = ?", (turn_id,))
    harness.store.turn_name(turn_id, a.station_id, "W4XYZ", "K4ABC", 1)
    harness.store.turn_name(turn_id, b.station_id, "K4ABC", "KG4VCF", 1)
    assert list(harness.store.name_counts(turn_id)) == ["K4ABC", "W4XYZ"]


def test_a_station_that_joined_after_the_turn_is_refused_not_allowed(harness):
    a, b = _room_of(harness, "K4ABC", "KG4VCF")
    turn_id = _unknown_turn(harness, b)
    e = _join(harness, "W4XYZ", 5)
    harness.advance(10000)
    e.take()
    errors = _name(e, turn_id, "K4ABC")
    assert _code(errors) == "not-allowed"
    assert errors[0]["reason"] == "You can name a turn only if you were on this frequency during it."


def test_a_station_that_became_a_member_while_the_turn_was_open_may_name_it(harness):
    a, b = _room_of(harness, "K4ABC", "KG4VCF")
    e = _join(harness, "W4XYZ", 5)
    harness.advance(8000)
    b.send(reading(0, -90.0, harness.clock.wall_ms()))
    harness.advance(2000)  # E is a member now, the turn still open.
    b.send(reading(0, -90.0, harness.clock.wall_ms()))
    harness.advance(3000)
    turn_id = harness.store.db.execute("SELECT id FROM turns WHERE kind = 'unknown'").fetchone()[0]
    e.take()
    assert _name(e, turn_id, "K4ABC") == []
    # A joining station that is not yet a member is refused not-member.
    f = _join(harness, "N4DEF", 6)
    f.take()
    assert _code(_name(f, turn_id, "K4ABC")) == "not-member"


def test_a_turn_may_be_named_while_it_is_open(harness):
    a, b = _room_of(harness, "K4ABC", "KG4VCF")
    b.send(reading(0, -90.0, harness.clock.wall_ms()))
    turn_id = harness.store.db.execute("SELECT id FROM turns WHERE kind = 'unknown'").fetchone()[0]
    a.take()
    assert _name(a, turn_id, "W4XYZ") == []
    assert harness.store.name_counts(turn_id) == {"W4XYZ": 1}


def test_a_station_that_left_and_is_elsewhere_is_refused_not_member(harness):
    a, b = _room_of(harness, "K4ABC", "KG4VCF")
    turn_id = _unknown_turn(harness, b)
    a.send(presence("K4ABC", "EM73", [slice_(0, 7188000, "LSB")]))
    harness.advance(10000)
    a.take()
    assert _code(_name(a, turn_id, "W4XYZ")) == "not-member"


def test_talking_and_doubled_turns_are_refused_bad_turn_before_membership(harness):
    a, b, c = _room_of(harness, "K4ABC", "KG4VCF", "W4XYZ")
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212010, keyed=True)]))
    b.send(presence("KG4VCF", "EM73", [slice_(0, 14212020, keyed=True)]))
    kinds = dict(harness.store.db.execute("SELECT kind, id FROM turns").fetchall())
    for s in (a, b, c):
        s.take()
    errors = _name(c, kinds["talking"], "K4ABC")
    assert _code(errors) == "bad-turn"
    assert errors[0]["reason"] == "Only a turn with an unknown talker can be named, up to 24 hours after it ends."
    assert _code(_name(c, kinds["doubled"], "K4ABC")) == "bad-turn"
    # A station in another room naming a talking turn hears bad-turn, which
    # comes before not-member.
    d = _join(harness, "N4DEF", 9, hz=7188000)
    d.take()
    assert _code(_name(d, kinds["talking"], "K4ABC")) == "bad-turn"


def test_no_such_turn_away_and_blocked(harness):
    a, b = _room_of(harness, "K4ABC", "KG4VCF")
    turn_id = _unknown_turn(harness, b)
    assert _code(_name(a, turn_id + 100, "W4XYZ")) == "not-found"
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212010)], at_radio=False))
    a.take()
    errors = _name(a, turn_id, "W4XYZ")
    assert _code(errors) == "away"
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212010)]))
    harness.store.db.execute("UPDATE stations SET posting_block_forever = 1 WHERE id = ?", (a.station_id,))
    harness.advance(1000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212010)]))
    a.take()
    assert _code(_name(a, turn_id, "W4XYZ")) == "blocked"


def test_the_eleventh_name_in_a_minute_is_refused_rate(harness):
    a, b = _room_of(harness, "K4ABC", "KG4VCF")
    turn_id = _unknown_turn(harness, b)
    names = ["W4XY%s" % chr(ord("A") + i) for i in range(11)]
    for name in names[:10]:
        assert _name(a, turn_id, name) == []
    errors = _name(a, turn_id, names[10])
    assert _code(errors) == "rate" and errors[0]["retryAfterMs"] >= 1
    assert harness.store.name_counts(turn_id) == {names[9]: 1}


def _alive(harness, stations, ms):
    """Move time while the stations keep talking to the server."""
    while ms > 0:
        step = min(ms, 20000)
        harness.advance(step)
        ms -= step
        for s in stations:
            s.send(presence(s.callsign, "EM73", [slice_(0, s.hz)]))


def test_naming_works_until_24_hours_after_the_turn_in_a_later_opening(harness):
    a, b, c = _room_of(harness, "K4ABC", "KG4VCF", "W4XYZ")
    turn_id = _unknown_turn(harness, b)
    end = harness.store.turn_get(turn_id)["end_ms"]
    # Everyone goes quiet; the rows leave and the room closes.
    harness.advance(DAY - 60000)
    room_id = harness.store.db.execute("SELECT room_id FROM rooms").fetchone()[0]
    assert harness.service.book.live(room_id) is None
    first_opening = harness.store.turn_get(turn_id)["opening_id"]
    for s in (a, b, c):
        s.send(presence(s.callsign, "EM73", [slice_(0, s.hz)]))
    _alive(harness, (a, b, c), end + DAY - harness.clock.wall_ms())
    assert harness.clock.wall_ms() == end + DAY
    assert harness.service.book.live(room_id).opening_id != first_opening
    for s in (a, b, c):
        s.take()
    assert _name(a, turn_id, "N4DEF") == []
    harness.advance(1)
    assert _code(_name(b, turn_id, "N4DEF")) == "bad-turn"
    assert harness.store.name_counts(turn_id) == {"N4DEF": 1}


def test_membership_during_the_turn_is_kept_across_a_restart(tmp_path):
    path = str(tmp_path / "rt.sqlite")
    key_a = ec.generate_private_key(ec.SECP256R1())
    key_e = ec.generate_private_key(ec.SECP256R1())
    store = Store(path)
    h = Harness(store=store)
    a = _join(h, "K4ABC", 1, key=key_a)
    b = _join(h, "KG4VCF", 2)
    h.advance(10000)
    turn_id = _unknown_turn(h, b)
    e = _join(h, "W4XYZ", 5, key=key_e)
    h.advance(10000)
    wall = h.clock.wall_ms()
    h.close()
    store.close()
    store = Store(path)
    h = Harness(store=store, wall_start_ms=wall + 60000)
    try:
        a = _join(h, "K4ABC", 1, key=key_a)
        e = _join(h, "W4XYZ", 5, key=key_e)
        h.advance(10000)
        a.take()
        e.take()
        assert _code(_name(e, turn_id, "K4ABC")) == "not-allowed"
        assert _name(a, turn_id, "W4XYZ") == []
        assert store.name_counts(turn_id) == {"W4XYZ": 1}
    finally:
        h.close()
        store.close()


def test_names_never_touch_sessions_and_recent_names_are_by_base_callsign(harness):
    a, b, c = _room_of(harness, "K4ABC", "KG4VCF", "W4XYZ")
    first = _unknown_turn(harness, b)
    harness.advance(1000)
    second = _unknown_turn(harness, b)
    since = harness.clock.wall_ms()
    assert _name(a, first, "VE3/W4XYZ/P") == []
    harness.advance(1)
    assert _name(b, second, "N4DEF") == []
    harness.advance(1)
    assert _name(c, first, "W4XYZ") == []
    assert harness.store.db.execute("SELECT COUNT(*) FROM sessions").fetchone()[0] == 0
    room_id = harness.store.db.execute("SELECT room_id FROM rooms").fetchone()[0]
    assert harness.store.names_recent(room_id, since) == ["W4XYZ", "N4DEF"]
    assert harness.store.names_recent(room_id, since + 2) == ["W4XYZ"]
    assert harness.store.names_recent("lsb-7188000", since) == []
    # The stored name, and the namer's callsign at the time.
    row = harness.store.db.execute(
        "SELECT name, callsign FROM turn_names WHERE turn_id = ? AND station_id = ?", (first, a.station_id)
    ).fetchone()
    assert tuple(row) == ("VE3/W4XYZ/P", "K4ABC")
