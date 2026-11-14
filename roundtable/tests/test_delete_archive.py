# no-port-check: NereusSDR-original.
"""Delete my archive (design 13, C9; wire spec 5.3.13, 5.4.10): what goes
(the station's sessions, its talking turns and every reading stored with
them, its message texts) and what stays (its readings of other talkers,
under the callsign stored with them). Refusals first. Row ids are never
reused after a delete."""

from __future__ import annotations

import pytest

from inproc import Harness, presence, reading, slice_
from webclient import Browser, body_json, opening, stored_turn

ROOM = "usb-14212000"
CONFIRM = "delete my archive"


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


@pytest.fixture
def browser(harness):
    return Browser(harness)


def _count(harness, sql, *args):
    return harness.store.db.execute(sql, args).fetchone()[0]


def _history(harness):
    """A talks (C and B read it), B talks (A reads it), and A, B and C post."""
    a = harness.sign_in(address="192.0.2.1")
    b = harness.sign_in(address="192.0.2.2")
    c = harness.sign_in(address="192.0.2.3")
    oid = opening(harness)
    store = harness.store
    a_turn = stored_turn(
        harness, oid, "talking", [(a.station_id, "W4XYZ")], 1000, 11000,
        [(b.station_id, "K4ABC", [True] * 10), (c.station_id, "N0CALL", [True] * 10)],
    )
    store.turn_name(a_turn, c.station_id, "W4XYZ", "N0CALL", 2000)
    b_turn = stored_turn(
        harness, oid, "talking", [(b.station_id, "K4ABC")], 20000, 30000, [(a.station_id, "W4XYZ", [True] * 10)]
    )
    mine = [
        store.message_add(oid, ROOM, a.station_id, "W4XYZ", 1, 5000, "hello KG4VCF", -90.0, ["KG4VCF"]),
        store.message_add(oid, ROOM, a.station_id, "W4XYZ", 1, 25000, "good copy", -88.0),
    ]
    theirs = store.message_add(oid, ROOM, b.station_id, "K4ABC", 2, 6000, "hi W4XYZ", None, ["W4XYZ"])
    return a, b, c, a_turn, b_turn, mine, theirs


# ----------------------------------------------------------- refusals


def test_delete_over_the_web_needs_origin_csrf_and_the_words(harness, browser):
    a, *_ = _history(harness)
    cookie = browser.link_cookie(a)
    csrf = browser.csrf(cookie)
    assert browser.post("/archive/delete", {"confirm": CONFIRM}, cookie).status == 403
    assert browser.post("/archive/delete", {"csrf": csrf, "confirm": CONFIRM}, cookie, origin="https://evil.example").status == 403
    assert browser.post("/archive/delete", {"csrf": csrf, "confirm": CONFIRM}).status == 401
    for words in ("", "Delete my archive", "delete my archive ", "delete"):
        r = browser.post("/archive/delete", {"csrf": csrf, "confirm": words}, cookie)
        assert r.status == 400 and body_json(r)["error"] == "not-allowed", words
    r = browser.post("/archive/delete", {"csrf": csrf}, cookie)
    assert r.status == 400
    assert len(harness.store.sessions_of(a.station_id)) == 1
    assert _count(harness, "SELECT COUNT(*) FROM messages WHERE station_id = ? AND text IS NOT NULL", a.station_id) == 2


def test_delete_from_the_app_needs_the_words(harness):
    a, *_ = _history(harness)
    a.send({"type": "deleteArchive", "confirm": "delete everything"})
    got = a.take()
    assert [(m["type"], m.get("code")) for m in got] == [("error", "not-allowed")]
    assert len(harness.store.sessions_of(a.station_id)) == 1


# ----------------------------------------------------------- what goes


def test_delete_my_archive_over_the_web(harness, browser):
    a, b, c, a_turn, b_turn, mine, theirs = _history(harness)
    cookie = browser.link_cookie(a)
    r = browser.post("/archive/delete", {"csrf": browser.csrf(cookie), "confirm": CONFIRM}, cookie)
    assert r.status == 200
    assert body_json(r)["deleted"] is True
    store = harness.store
    # Its sessions, its talking turns and others' readings and names of them.
    assert store.sessions_of(a.station_id) == []
    assert store.turn_get(a_turn) is None
    assert _count(harness, "SELECT COUNT(*) FROM turn_readings WHERE turn_id = ?", a_turn) == 0
    assert _count(harness, "SELECT COUNT(*) FROM turn_names WHERE turn_id = ?", a_turn) == 0
    # Its own reading of another's turn stays under its stored callsign.
    (kept,) = store.tracks_of(b_turn)
    assert (kept["station_id"], kept["callsign"]) == (a.station_id, "W4XYZ")
    assert len(store.sessions_of(b.station_id)) == 1
    # Its message texts are gone, the rows stay; others' messages stay.
    for mid in mine:
        row = store.message_get(mid)
        assert (row["text"], row["state"], row["mentions"], row["callsign"]) == (None, "deleted", "", "W4XYZ")
    assert store.message_get(theirs)["text"] == "hi W4XYZ"
    # Signed in still, with nothing in the archive.
    assert body_json(browser.get("/v1/me/sessions", cookie))["sessions"] == []
    # The browser session was not what was deleted.
    assert browser.get("/v1/me", cookie).status == 200


def test_delete_my_archive_from_the_app(harness):
    a, b, c, a_turn, b_turn, mine, theirs = _history(harness)
    a.send({"type": "deleteArchive", "confirm": CONFIRM})
    (notice,) = a.take()
    assert notice["type"] == "notice" and notice["code"] == "archive-deleted" and notice["until"] is None
    assert harness.store.sessions_of(a.station_id) == []
    assert harness.store.turn_get(a_turn) is None
    assert [harness.store.message_get(m)["text"] for m in mine] == [None, None]
    assert harness.store.turn_get(b_turn) is not None


def test_a_removed_message_keeps_its_state(harness):
    a, b, c, a_turn, b_turn, mine, theirs = _history(harness)
    harness.store.db.execute("UPDATE messages SET state = 'removed' WHERE id = ?", (mine[0],))
    harness.service.delete_archive(a.station_id)
    assert harness.store.message_get(mine[0])["state"] == "removed"
    assert harness.store.message_get(mine[0])["text"] is None


def test_the_live_rooms_show_the_cleared_messages(harness):
    a = harness.sign_in(address="192.0.2.1")
    a.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    b = harness.sign_in(address="192.0.2.2")
    b.send(presence("K4ABC", "FN20", [slice_(0, 14212040)]))
    harness.advance(10000)
    for s in (a, b):
        s.take()
    a.send({"type": "post", "room": ROOM, "text": "hello there", "clientId": "c1"})
    posted = [m for m in a.take() if m["type"] == "posted"]
    msg_id = posted[0]["msg"]
    b.take()
    a.send({"type": "deleteArchive", "confirm": CONFIRM})
    harness.advance(1000)
    seen = [m for msg in b.take() if msg["type"] == "delta" for m in msg["messages"]]
    assert [(m["id"], m["state"], m["text"]) for m in seen] == [(msg_id, "deleted", None)]


def test_an_open_talking_turn_is_kept_and_stored_as_new(harness):
    a = harness.sign_in(address="192.0.2.1")
    a.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    b = harness.sign_in(address="192.0.2.2")
    b.send(presence("K4ABC", "FN20", [slice_(0, 14212040)]))
    harness.advance(10000)
    a.send(presence("W4XYZ", "EM73", [slice_(0, 14212000, keyed=True)]))
    harness.advance(1000)
    b.send(reading(0, -80.0, harness.clock.wall_ms()))
    open_id = harness.store.db.execute("SELECT id FROM turns WHERE end_ms IS NULL").fetchone()[0]
    harness.service.delete_archive(a.station_id)
    assert harness.store.turn_get(open_id) is not None
    harness.advance(1000)
    a.send(presence("W4XYZ", "EM73", [slice_(0, 14212000)]))
    harness.advance(4000)
    (s,) = harness.store.sessions_of(a.station_id)
    assert harness.store.session_turn_ids(s) == [open_id]


# ----------------------------------------------------------- row ids


def test_row_ids_are_never_reused_after_a_delete(harness):
    a, b, c, a_turn, b_turn, mine, theirs = _history(harness)
    store = harness.store
    oid = opening(harness)
    # A's turn becomes the newest turn and its session the newest session.
    newest_turn = stored_turn(harness, oid, "talking", [(a.station_id, "W4XYZ")], 40 * 60000, 41 * 60000)
    newest_session = max(int(s["id"]) for s in store.sessions_of(a.station_id))
    assert newest_session == _count(harness, "SELECT MAX(id) FROM sessions")
    assert newest_turn == _count(harness, "SELECT MAX(id) FROM turns")
    harness.service.delete_archive(a.station_id)
    assert _count(harness, "SELECT MAX(id) FROM turns") == b_turn
    after = stored_turn(harness, oid, "talking", [(a.station_id, "W4XYZ")], 50 * 60000, 51 * 60000)
    assert after > newest_turn
    (session,) = store.sessions_of(a.station_id)
    assert int(session["id"]) > newest_session
    # The other tables with their own ids: the newest row deleted, the
    # next one gets a fresh id.
    for table, insert in (
        ("rooms", lambda: opening(harness)),
        ("messages", lambda: store.message_add(oid, ROOM, b.station_id, "K4ABC", 2, 1, "x", None)),
    ):
        first = insert()
        key = "opening_id" if table == "rooms" else "id"
        store.db.execute(f"DELETE FROM {table} WHERE {key} = ?", (first,))
        assert insert() > first, table
