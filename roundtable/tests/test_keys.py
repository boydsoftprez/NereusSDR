# no-port-check: NereusSDR-original.
"""The key page's form actions (design 7.3, C6, D26; wire spec 4.2, 9.1):
issuing a website key, signing in with it, its profile, and "Move my
history to a new key". The refusals come first: Origin, the key limit, a
key that is not a good key, and a request that names a callsign in place
of a key."""

from __future__ import annotations

import pytest

from inproc import Harness, presence, slice_
from nereus_roundtable import identity
from webclient import Browser, body_json, opening, sign_in_key, stored_turn

DAY = 86400000


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


@pytest.fixture
def browser(harness):
    return Browser(harness)


def _count(harness, table):
    return harness.store.db.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]


# ----------------------------------------------------------- refusals


@pytest.mark.parametrize("path", ["/key/new", "/key/signin", "/key/profile", "/key/move"])
@pytest.mark.parametrize(
    "origin",
    [
        None,
        "https://evil.example",
        "http://roundtable.conformance.invalid",
        "null",
        ["https://roundtable.conformance.invalid", "https://roundtable.conformance.invalid"],
    ],
)
def test_every_key_action_needs_the_site_origin(harness, browser, path, origin):
    key = browser.new_key()
    keys = _count(harness, "keys")
    r = browser.post(path, {"callsign": "K4ABC", "grid": "EM73", "key": key}, origin=origin)
    assert r.status == 403, (path, origin, r.status)
    assert body_json(r)["error"] == "forbidden"
    assert r.head("Set-Cookie") is None
    assert _count(harness, "keys") == keys
    assert _count(harness, "web_sessions") == 0
    assert harness.store.station_get(identity.key_id(identity.from_b64url(key)))["callsign"] == "W4XYZ"


def test_a_fourth_key_in_a_day_from_one_group_is_refused(harness, browser):
    for _ in range(3):
        browser.new_key(address="198.51.100.7")
    r = browser.post("/key/new", {"callsign": "W4XYZ", "grid": "EM73"}, address="198.51.100.7")
    assert r.status == 429
    assert body_json(r)["error"] == "rate"
    assert _count(harness, "keys") == 3
    # The same group by an IPv4-mapped address, and the IPv6 /64.
    assert browser.post("/key/new", {"callsign": "W4XYZ"}, address="::ffff:198.51.100.7").status == 429
    for n in range(3):
        browser.new_key(address=f"2001:db8:1:2::{n + 1}")
    assert browser.post("/key/new", {"callsign": "W4XYZ"}, address="2001:db8:1:2::99").status == 429
    # Another group is not held, and the window is a day.
    browser.new_key(address="198.51.100.8")
    harness.advance(DAY - 1)
    assert browser.post("/key/new", {"callsign": "W4XYZ"}, address="198.51.100.7").status == 429
    harness.advance(1)
    browser.new_key(address="198.51.100.7")


def test_a_refused_key_request_is_not_counted(harness, browser):
    for _ in range(5):
        assert browser.post("/key/new", {"callsign": "nope"}).status == 400
    for _ in range(3):
        browser.new_key()
    assert _count(harness, "keys") == 3


@pytest.mark.parametrize(
    "form",
    [
        {"callsign": ""},
        {"callsign": "W4XYZ/"},
        {"callsign": "ABC"},
        {"callsign": "W4XYZ", "grid": "EM7"},
        {"callsign": "W4XYZ", "grid": "EM73tu12"},
    ],
)
def test_key_new_refuses_a_bad_callsign_or_grid(harness, browser, form):
    assert browser.post("/key/new", form).status == 400
    assert _count(harness, "keys") == 0


def test_a_form_must_be_a_form(harness, browser):
    r = browser.post("/key/new", {"callsign": "W4XYZ"}, content_type="application/json")
    assert r.status == 400
    r = browser.request(
        "POST",
        "/key/new",
        {"Origin": browser.origin, "Content-Type": "application/x-www-form-urlencoded"},
        b"callsign=W4XYZ&callsign=K4ABC",
    )
    assert r.status == 400
    assert _count(harness, "keys") == 0
    assert browser.get("/key/new").status == 405


@pytest.mark.parametrize("path", ["/key/signin", "/key/profile", "/key/move"])
def test_a_key_action_refuses_a_key_that_is_not_good(harness, browser, path):
    good = browser.new_key()
    revoked = browser.new_key(callsign="K4ABC")
    harness.store.key_revoke(identity.key_hash(identity.from_b64url(revoked)), 1)
    for key in ("", "short", identity.to_b64url(identity.new_key()), revoked, good + "A"):
        r = browser.post(path, {"key": key, "callsign": "N4DEF", "grid": "FN20"})
        assert r.status == 403, (path, key)
        assert body_json(r)["error"] == "bad-key"
    assert _count(harness, "web_sessions") == 0
    assert _count(harness, "keys") == 2
    assert harness.store.station_get(identity.key_id(identity.from_b64url(good)))["callsign"] == "W4XYZ"


def test_a_move_or_profile_naming_a_callsign_in_place_of_a_key_does_nothing(harness, browser):
    key = browser.new_key(callsign="W4XYZ")
    sid = identity.key_id(identity.from_b64url(key))
    oid = opening(harness)
    stored_turn(harness, oid, "talking", [(sid, "W4XYZ")], 1000, 5000)
    before = {t: _count(harness, t) for t in ("keys", "sessions", "turns", "stations")}
    for path in ("/key/move", "/key/profile"):
        r = browser.post(path, {"callsign": "W4XYZ", "grid": "EM73"})
        assert r.status == 403
        r = browser.post(path, {"callsign": "W4XYZ", "key": "W4XYZ"})
        assert r.status == 403
    assert {t: _count(harness, t) for t in before} == before
    assert harness.store.key_lookup(identity.key_hash(identity.from_b64url(key)))["revoked"] is None
    assert harness.store.db.execute("SELECT station_id FROM sessions").fetchall()[0][0] == sid


# ----------------------------------------------------------- flows


def test_a_key_works_once_issued_and_only_its_hash_is_stored(harness, browser):
    r = browser.post("/key/new", {"callsign": "w4xyz", "grid": "em73TU"})
    assert r.status == 200
    assert r.head("Cache-Control") == "no-store"
    assert r.head("Access-Control-Allow-Origin") is None
    got = body_json(r)
    assert got["callsign"] == "W4XYZ" and got["grid"] == "EM73tu"
    raw = identity.b64url_of_length(got["key"], 32)
    assert raw is not None and len(got["key"]) == 43
    stored = harness.store.db.execute("SELECT key_hash, station_id FROM keys").fetchall()
    assert [(bytes(h), s) for h, s in stored] == [(identity.key_hash(raw), identity.key_id(raw))]
    # Nothing in the database holds the key's text or raw bytes.
    for table in harness.store.columns():
        for row in harness.store.db.execute(f"SELECT * FROM {table}").fetchall():
            for value in row:
                assert value != got["key"] and value != raw
    station = sign_in_key(harness, got["key"])
    assert station.closed is None
    assert station.station_id == identity.key_id(raw)
    assert harness.store.station_get(station.station_id)["callsign"] == "W4XYZ"


def test_key_signin_sets_the_session_cookie(harness, browser):
    key = browser.new_key()
    r = browser.post("/key/signin", {"key": key})
    assert r.status == 303
    assert r.head("Location") == "/archive"
    cookie = r.head("Set-Cookie")
    name, _, rest = cookie.partition("=")
    token, _, attributes = rest.partition("; ")
    assert name == "rt_session" and identity.b64url_of_length(token, 32) is not None
    assert attributes == "Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=2592000"
    # Only the cookie's hash is stored.
    hashes = [bytes(r[0]) for r in harness.store.db.execute("SELECT token_hash FROM web_sessions")]
    assert hashes == [identity.key_hash(identity.from_b64url(token))]
    me = browser.get("/v1/me", browser.cookie_from(r))
    assert me.status == 200
    assert body_json(me)["station"] == {"callsign": "W4XYZ", "grid": "EM73", "kind": "key"}


def test_key_profile_sets_the_callsign_and_grid(harness, browser):
    key = browser.new_key()
    sid = identity.key_id(identity.from_b64url(key))
    r = browser.post("/key/profile", {"key": key, "callsign": "k4abc/p", "grid": ""})
    assert r.status == 200 and body_json(r) == {"callsign": "K4ABC/P", "grid": None}
    row = harness.store.station_get(sid)
    assert (row["callsign"], row["grid"]) == ("K4ABC/P", None)
    station = sign_in_key(harness, key)
    station.send(presence("IGNORED1", "FN20", [slice_(0, 14212000)]))
    harness.advance(10000)
    assert harness.service.book.live("usb-14212000").rows[sid].callsign == "K4ABC/P"


def _scan_for(harness, value):
    """Every (table, column) whose stored value holds `value`."""
    out = []
    for table, columns in harness.store.columns().items():
        for row in harness.store.db.execute(f"SELECT * FROM {table}").fetchall():
            for column, cell in zip(columns, row):
                if isinstance(cell, str) and value in cell:
                    out.append((table, column))
    return sorted(set(out))


def test_key_move_moves_everything_revokes_the_old_key_and_closes_its_socket(harness, browser):
    old_key = browser.new_key(callsign="W4XYZ", grid="EM73")
    old_id = identity.key_id(identity.from_b64url(old_key))
    store = harness.store
    # History: a talking turn with a reader, its reading of another's turn,
    # a name, a message, a report, mentions seen and a web session.
    other = "q" * 26
    oid = opening(harness)
    talk = stored_turn(harness, oid, "talking", [(old_id, "W4XYZ")], 1000, 60000, [(other, "K4ABC", [True] * 30)])
    heard = stored_turn(harness, oid, "talking", [(other, "K4ABC")], 70000, 90000, [(old_id, "W4XYZ", [True] * 10)])
    unknown = stored_turn(harness, oid, "unknown", [], 100000, 110000)
    store.turn_members(unknown, {old_id, other})
    store.turn_name(unknown, old_id, "N4DEF", "W4XYZ", 120000)
    store.message_add(oid, "usb-14212000", old_id, "W4XYZ", 1, 130000, "hello", None, ["K4ABC"])
    store.report_add("turn", unknown, None, old_id, 140000)
    store.mention_seen(old_id, 150000)
    cookie = browser.key_cookie(old_key)
    # The old key's station is connected and in a room.
    station = sign_in_key(harness, old_key)
    station.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    harness.advance(10000)
    station.take()
    assert harness.service.book.live("usb-14212000").rows.get(old_id) is not None

    r = browser.post("/key/move", {"key": old_key})
    assert r.status == 200
    new_key = body_json(r)["key"]
    new_raw = identity.b64url_of_length(new_key, 32)
    new_id = identity.key_id(new_raw)
    assert new_id != old_id

    # The old socket closed with 4004 and no message; its row left.
    assert station.take() == []
    assert station.closed == 4004
    assert old_id not in harness.service.stations
    assert harness.service.book.live("usb-14212000").rows.get(old_id) is None
    # The old key is revoked and refuses a sign-in with 4004.
    assert store.key_lookup(identity.key_hash(identity.from_b64url(old_key)))["revoked"] is not None
    assert sign_in_key(harness, old_key).closed == 4004
    # Nothing is left under the old id but its revoked key and the log of
    # the move itself.
    assert _scan_for(harness, old_id) == [("keys", "station_id"), ("review_actions", "helper_id"), ("review_actions", "target")]
    log = store.db.execute("SELECT helper_id, action, target, detail FROM review_actions").fetchall()
    assert [tuple(r) for r in log] == [(old_id, "move-history", old_id, new_id)]
    # Everything is under the new id.
    assert store.station_get(new_id)["callsign"] == "W4XYZ"
    assert store.station_get(new_id)["mentions_seen"] == 150000
    assert [s["turn_ids"] for s in store.sessions_of(new_id)] == [str(talk)]
    assert store.turn_get(talk)["talker_id"] == new_id
    assert [r["station_id"] for r in store.tracks_of(heard)] == [new_id]
    assert store.turn_names_of(unknown) == {new_id: "N4DEF"}
    assert store.member_ids(store.turn_get(unknown)) == {new_id, other}
    assert store.db.execute("SELECT station_id FROM messages").fetchall()[0][0] == new_id
    assert store.db.execute("SELECT reporter_id FROM reports").fetchall()[0][0] == new_id
    # The old browser session ended with the old id.
    assert browser.get("/v1/me", cookie).status == 401
    # The new key signs in to the moved history.
    moved = sign_in_key(harness, new_key, address="192.0.2.51")
    assert moved.closed is None and moved.station_id == new_id
    cookie = browser.key_cookie(new_key)
    sessions = body_json(browser.get("/v1/me/sessions", cookie))["sessions"]
    assert [s["reported"] for s in sessions] == [1]
    # The old key cannot move again.
    assert browser.post("/key/move", {"key": old_key}).status == 403


@pytest.mark.parametrize("failure", ["database", "gone"])
def test_a_key_move_that_does_not_commit_tells_the_station_to_try_later(harness, browser, monkeypatch, failure):
    import sqlite3

    old_key = browser.new_key(callsign="W4XYZ")
    old_hash = identity.key_hash(identity.from_b64url(old_key))
    station = sign_in_key(harness, old_key)
    station.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    harness.advance(10000)
    station.take()

    def fail(*args, **kwargs):
        if failure == "database":
            raise sqlite3.OperationalError("disk I/O error")
        return None

    monkeypatch.setattr(harness.store, "key_move", fail)
    r = browser.post("/key/move", {"key": old_key})
    assert r.status == (503 if failure == "database" else 403)
    # Never told its key was revoked: 1013, so it comes back with it.
    assert station.take() == []
    assert station.closed == 1013
    assert harness.store.key_lookup(old_hash)["revoked"] is None
    monkeypatch.undo()
    again = sign_in_key(harness, old_key, address="192.0.2.51")
    assert again.closed is None


def test_key_move_renames_what_open_turns_hold_in_memory(harness, browser):
    """A reading the old station made of another station's open turn is
    flushed later under the new id, never the old."""
    from inproc import reading

    old_key = browser.new_key(callsign="W4XYZ")
    old_id = identity.key_id(identity.from_b64url(old_key))
    talker = harness.sign_in(address="192.0.2.9")
    talker.send(presence("K4ABC", "FN20", [slice_(0, 14212000)]))
    station = sign_in_key(harness, old_key)
    station.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    harness.advance(10000)
    talker.send(presence("K4ABC", "FN20", [slice_(0, 14212000, keyed=True)]))
    harness.advance(1000)
    station.send(reading(0, -80.0, harness.clock.wall_ms()))
    harness.advance(1000)
    station.send(reading(0, -81.0, harness.clock.wall_ms()))
    r = browser.post("/key/move", {"key": old_key})
    assert r.status == 200
    new_id = identity.key_id(identity.b64url_of_length(body_json(r)["key"], 32))
    harness.advance(31000)
    talker.send(presence("K4ABC", "FN20", [slice_(0, 14212000)]))
    harness.advance(4000)
    assert harness.store.db.execute("SELECT COUNT(*) FROM turns WHERE end_ms IS NULL").fetchone()[0] == 0
    stations = {r[0] for r in harness.store.db.execute("SELECT station_id FROM turn_readings")}
    assert stations == {new_id}
    assert _scan_for(harness, old_id) == [("keys", "station_id"), ("review_actions", "helper_id"), ("review_actions", "target")]
