# no-port-check: NereusSDR-original.
"""Messages (wire spec 5.3.4, 5.3.5, 5.3.7, 5.3.14, 5.4.7, 6.10): posting
and its refusals in order, the message a room's viewers get, the last 50
in a room view, deleting, reports onto the review list, and going hidden."""

from __future__ import annotations

import logging
import sqlite3

import pytest

from inproc import Harness, presence, reading, slice_
from nereus_roundtable.config import Config

ROOM = "usb-14212000"


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def _config(**limits):
    config = Config()
    for key, value in limits.items():
        setattr(config, key, value)
    return config


def _station(harness, callsign, hz=14212000, address="192.0.2.1", at_radio=True, keyed=False):
    s = harness.sign_in(address=address)
    s.callsign = callsign
    s.send(presence(callsign, "EM73", [slice_(0, hz, keyed=keyed)], at_radio))
    return s


def _members(harness, *callsigns):
    """Stations in one room, members after the 10 s dwell."""
    out = [_station(harness, c, hz=14212000 + 10 * i, address=f"192.0.2.{i + 1}") for i, c in enumerate(callsigns)]
    harness.advance(10000)
    for s in out:
        s.take()
    return out


def _post(station, text, client_id="c1", room=ROOM):
    station.send({"type": "post", "room": room, "text": text, "clientId": client_id})


def _answer(station):
    """The `posted` among what arrived (deltas and band-list changes from
    the same moment may come with it)."""
    got = [m for m in station.take() if m["type"] == "posted"]
    assert len(got) == 1, got
    return got[0]


def _ref(harness, station, room=ROOM):
    return harness.service.book.live(room).rows[station.station_id].ref


def _messages(got):
    return [m for msg in got if msg["type"] == "delta" for m in msg["messages"]]


def test_a_post_reaches_every_viewer_with_the_callsign_and_reading_at_the_time(harness):
    a, b, c = _members(harness, "K4ABC", "KG4VCF", "W4XYZ")
    # A talks; B's reading is stored with A's turn.
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    b.send(reading(0, -91.5, harness.clock.wall_ms()))
    for s in (a, b, c):
        s.take()
    _post(b, "Good copy here, **S7**.")
    got_b = b.take()
    assert got_b[0]["type"] == "posted" and got_b[0]["ok"] is True and got_b[0]["clientId"] == "c1"
    msg_id = got_b[0]["msg"]
    expected = {
        "id": msg_id,
        "ref": _ref(harness, b),
        "callsign": "KG4VCF",
        "nth": 1,
        "t": harness.clock.wall_ms(),
        "state": "shown",
        "text": "Good copy here, **S7**.",
        "dbm": -91.5,
    }
    # The sender's own view gets it too, after `posted`.
    assert _messages(got_b[1:]) == [expected]
    for s in (a, c):
        got = s.take()
        assert [m["type"] for m in got] == ["delta"]
        assert got[0]["messages"] == [expected] and got[0]["members"] == [] and got[0]["turns"] == []
    row = harness.store.message_get(msg_id)
    assert (row["callsign"], row["nth"], row["dbm"], row["opening_id"]) == (
        "KG4VCF",
        1,
        -91.5,
        harness.service.book.live(ROOM).opening_id,
    )
    # The stored callsign is the one at the time: a later callsign change
    # does not touch it.
    b.send(presence("KG4VCF/P", "EM73", [slice_(0, 14212010)]))
    d = harness.sign_in(address="192.0.2.9")
    d.send({"type": "read", "room": ROOM})
    view = [m for m in d.take() if m["type"] == "room"][0]
    assert [(m["callsign"], m["text"]) for m in view["messages"]] == [("KG4VCF", "Good copy here, **S7**.")]


def test_the_reading_is_null_once_its_turn_has_ended_or_with_no_reading(harness):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    _post(b, "no reading yet")
    assert _messages(b.take())[0]["dbm"] is None
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    b.send(reading(0, -80.0, harness.clock.wall_ms()))
    harness.advance(1000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    harness.advance(3000)
    b.take()
    _post(b, "after the turn", "c2")
    assert _messages(b.take())[0]["dbm"] is None


def test_the_seventh_post_in_a_minute_is_refused_rate_and_not_counted(harness):
    (a,) = _members(harness, "K4ABC")
    for i in range(6):
        _post(a, f"m{i}", f"c{i}")
        assert _answer(a)["ok"] is True
        harness.advance(1000)
    _post(a, "m6", "c6")
    assert _answer(a) == (
        {
            "type": "posted",
            "clientId": "c6",
            "ok": False,
            "msg": None,
            "code": "rate",
            "reason": "Too many at once. Try again in a moment.",
        }
    )
    # The first post (at 10000 ms) leaves the window at 70000 ms. The
    # station keeps talking to the server so its row stays.
    for _ in range(2):
        harness.advance(20000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    harness.advance(70000 - harness.clock.now_ms() - 1)
    a.take()
    _post(a, "m7", "c7")
    assert _answer(a)["code"] == "rate"
    harness.advance(1)
    _post(a, "m8", "c8")
    assert _answer(a)["ok"] is True


def test_the_sixty_first_post_in_an_hour_is_refused_rate():
    h = Harness(_config(messages_per_minute=1000))
    try:
        (a,) = _members(h, "K4ABC")
        for i in range(60):
            _post(a, f"m{i}", f"c{i}")
            assert _answer(a)["ok"] is True
            h.advance(20000)
        _post(a, "late", "late")
        assert _answer(a)["code"] == "rate"
        # The first one leaves the hour at 10000 + 3600000 ms.
        h.advance(10000 + 3600000 - h.clock.now_ms())
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
        h.advance(10000)
        a.take()
        _post(a, "now", "now")
        assert _answer(a)["ok"] is True
    finally:
        h.close()


def test_too_long_and_nothing_to_send(harness):
    (a,) = _members(harness, "K4ABC")
    _post(a, "x" * 500, "ok")
    assert _answer(a)["ok"] is True
    _post(a, "x" * 501, "long")
    assert a.take()[0] == {
        "type": "posted",
        "clientId": "long",
        "ok": False,
        "msg": None,
        "code": "too-long",
        "reason": "A message can be up to 500 characters and 10 lines.",
    }
    _post(a, "\n".join("abcdefghijk"), "lines")
    assert _answer(a)["code"] == "too-long"
    _post(a, " ​\r\n ", "empty")
    got = a.take()[0]
    assert (got["code"], got["reason"]) == ("not-allowed", "There is nothing to send.")


def test_an_away_member_and_a_non_member_are_refused(harness):
    a = _station(harness, "K4ABC", at_radio=False)
    harness.advance(10000)
    a.take()
    _post(a, "hello")
    assert _answer(a)["code"] == "away"
    # Joining, in another room, and a room that does not exist.
    b = _station(harness, "KG4VCF", hz=14212050, address="192.0.2.2")
    b.take()
    _post(b, "hello")
    assert _answer(b)["code"] == "not-member"
    c = _station(harness, "W4XYZ", hz=7188000, address="192.0.2.3")
    harness.advance(10000)
    c.take()
    _post(c, "hello")
    assert _answer(c)["code"] == "not-member"
    _post(c, "hello", room="usb-21300000")
    assert _answer(c)["code"] == "not-member"
    # An AWAY station's refusal comes after not-member: a post to a room
    # it is not in is not-member.
    _post(a, "hello", room="lsb-7188000")
    assert _answer(a)["code"] == "not-member"


def test_a_blocked_station_is_refused_after_away_and_before_rate(harness):
    (a,) = _members(harness, "K4ABC")
    harness.store.db.execute("UPDATE stations SET posting_block_forever = 1 WHERE id = ?", (a.station_id,))
    _post(a, "hello")
    got = a.take()[0]
    assert (got["code"], got["reason"]) == ("blocked", "This station has been blocked from posting and naming turns.")
    harness.store.db.execute(
        "UPDATE stations SET posting_block_forever = 0, posting_block_until = ? WHERE id = ?",
        (harness.clock.wall_ms() + 1, a.station_id),
    )
    _post(a, "hello")
    assert _answer(a)["code"] == "blocked"
    harness.advance(1)
    _post(a, "hello")
    assert _answer(a)["ok"] is True


def test_a_database_that_cannot_save_answers_server(harness, monkeypatch):
    (a,) = _members(harness, "K4ABC")

    def fail(*args, **kwargs):
        raise sqlite3.OperationalError("disk I/O error")

    monkeypatch.setattr(harness.store, "message_add", fail)
    _post(a, "hello")
    got = a.take()
    assert [(m["type"], m.get("code")) for m in got] == [("posted", "server")]
    assert got[0]["reason"] == "Roundtable can't save messages right now."


def test_a_room_view_carries_the_last_fifty_messages_of_the_room_id_oldest_first():
    h = Harness(_config(messages_per_minute=1000, messages_per_hour=1000))
    try:
        a, b = _members(h, "K4ABC", "KG4VCF")
        ids = []
        for i in range(55):
            _post(a, f"m{i}", f"c{i}")
            ids.append(_answer(a)["msg"])
        assert ids == sorted(ids) and len(set(ids)) == 55
        c = h.sign_in(address="192.0.2.9")
        c.send({"type": "read", "room": ROOM})
        view = [m for m in c.take() if m["type"] == "room"][0]
        assert [m["id"] for m in view["messages"]] == ids[5:]
        assert view["messages"][-1]["ref"] == _ref(h, a)
        summary = h.service.book.live(ROOM).summary()
        assert summary["messages"] == 55
        # All members leave; the room closes and opens again on the same id.
        a.disconnect()
        b.disconnect()
        h.advance(600000)
        assert h.service.book.live(ROOM) is None
        d = _station(h, "W4XYZ", address="192.0.2.4")
        h.advance(10000)
        got = d.take()
        view = [m for m in got if m["type"] == "room"][0]
        # Messages of every opening, but none of the old ones has a row.
        assert [m["id"] for m in view["messages"]] == ids[5:]
        assert {m["ref"] for m in view["messages"]} == {None}
        assert h.service.book.live(ROOM).summary()["messages"] == 0
    finally:
        h.close()


def test_a_message_ref_is_null_once_its_sender_has_left(harness):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    _post(a, "hello")
    a.take()
    a.send(presence("K4ABC", "EM73", [slice_(0, 7188000, "LSB")]))
    b.send({"type": "resync", "room": ROOM})
    view = [m for m in b.take() if m["type"] == "room"][0]
    assert view["messages"][0]["ref"] is None


def test_delete_clears_the_text_and_keeps_the_row(harness):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    _post(a, "oops")
    msg_id = a.take()[0]["msg"]
    b.take()
    # Another station's message, and no such message.
    b.send({"type": "delete", "msg": msg_id})
    got = b.take()
    assert [(m["type"], m["code"], m["reason"], m["about"]) for m in got] == [
        ("error", "not-allowed", "You can delete only your own messages.", "delete")
    ]
    b.send({"type": "delete", "msg": msg_id + 100})
    assert b.take()[0]["code"] == "not-found"
    # The sender, after leaving the room.
    a.send(presence("K4ABC", "EM73", [slice_(0, 7188000, "LSB")]))
    a.take()
    b.take()
    a.send({"type": "delete", "msg": msg_id})
    assert a.take() == []
    deleted = _messages(b.take())
    assert len(deleted) == 1
    assert (deleted[0]["id"], deleted[0]["state"], deleted[0]["text"]) == (msg_id, "deleted", None)
    row = harness.store.message_get(msg_id)
    assert (row["state"], row["text"], row["callsign"]) == ("deleted", None, "K4ABC")
    # Deleting it again does nothing.
    a.send({"type": "delete", "msg": msg_id})
    assert a.take() == [] and b.take() == []


def _reports(harness):
    return [dict(r) for r in harness.store.db.execute("SELECT * FROM reports ORDER BY id")]


def test_reports_go_on_the_review_list_and_a_second_counts_once(harness):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    _post(a, "spam spam")
    msg_id = a.take()[0]["msg"]
    b.take()
    b.send({"type": "report", "target": "message", "msg": msg_id, "reason": "spam"})
    b.send({"type": "report", "target": "message", "msg": msg_id, "reason": "abuse"})
    b.send({"type": "report", "target": "station", "room": ROOM, "ref": _ref(harness, a), "reason": None})
    assert b.take() == [] and a.take() == []
    rows = _reports(harness)
    assert [(r["target"], r["message_id"], r["reason"], r["reporter_id"]) for r in rows[:1]] == [
        ("message", msg_id, "spam", b.station_id)
    ]
    assert len(rows) == 2
    assert (rows[1]["room_id"], rows[1]["row_ref"], rows[1]["reported_station_id"], rows[1]["reason"]) == (
        ROOM,
        _ref(harness, a),
        a.station_id,
        None,
    )
    for bad in (
        {"type": "report", "target": "message", "msg": msg_id + 50, "reason": None},
        {"type": "report", "target": "turn", "turn": 999, "reason": "wrongName"},
        {"type": "report", "target": "station", "room": ROOM, "ref": "aaaaaaaa", "reason": None},
        {"type": "report", "target": "station", "room": "lsb-7188000", "ref": _ref(harness, a), "reason": None},
    ):
        b.send(bad)
        got = b.take()
        assert [(m["code"], m["about"]) for m in got] == [("not-found", "report")], bad


def test_the_eleventh_report_in_an_hour_is_refused_rate(harness):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    ids = []
    for i in range(11):
        _post(a, f"m{i}", f"c{i}")
        ids.append(_answer(a)["msg"])
        harness.advance(10000)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
        b.send(presence("KG4VCF", "EM73", [slice_(0, 14212010)]))
    b.take()
    for msg_id in ids[:10]:
        b.send({"type": "report", "target": "message", "msg": msg_id, "reason": None})
    assert b.take() == []
    b.send({"type": "report", "target": "message", "msg": ids[10], "reason": None})
    got = b.take()
    assert [(m["code"], m["about"]) for m in got] == [("rate", "report")] and got[0]["retryAfterMs"] >= 1
    assert len(_reports(harness)) == 10


def test_hidden_ends_presence_at_once_and_refuses_what_follows(harness):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    b_ref = _ref(harness, b)
    # B is talking when it goes hidden.
    b.send(presence("KG4VCF", "EM73", [slice_(0, 14212010, keyed=True)]))
    a.take()
    b.take()
    harness.advance(500)
    b.send({"type": "hidden"})
    assert b.take() == []
    got = a.take()
    assert [m["type"] for m in got] == ["delta"]
    assert got[0]["left"] == [b_ref]
    turn = got[0]["turns"][0]
    assert (turn["kind"], turn["end"], turn["ending"]) == ("talking", harness.clock.wall_ms(), False)
    assert harness.service.book.rows_of(b.station_id) == []
    # Every station kind but weblink and deleteArchive is refused.
    for msg in (
        reading(0, -90.0, harness.clock.wall_ms()),
        presence("KG4VCF", "EM73", [slice_(0, 14212010)]),
        {"type": "read", "room": ROOM},
        {"type": "mentionsSeen", "t": 1},
        {"type": "name", "turn": 1, "name": "W4XYZ"},
        {"type": "delete", "msg": 1},
        {"type": "hidden"},
    ):
        b.send(msg)
        got = b.take()
        assert [(m["type"], m["code"], m["about"]) for m in got] == [("error", "hidden", msg["type"])], msg
        assert got[0]["reason"] == "This station is hidden, so nothing more is taken from it."
    _post(b, "hello")
    got = b.take()
    assert [(m["type"], m["code"], m["ok"]) for m in got] == [("posted", "hidden", False)]
    # Nothing else reaches it, the band list included.
    harness.advance(60000)
    _post(a, "anyone?")
    assert b.take() == [] and harness.service.book.rows_of(b.station_id) == []


def test_logs_hold_no_message_text_and_no_whole_station_id(harness, caplog):
    caplog.set_level(logging.DEBUG, logger="nereus_roundtable")
    a, b = _members(harness, "K4ABC", "KG4VCF")
    _post(a, "Secretword in the message")
    msg_id = a.take()[0]["msg"]
    a.send({"type": "delete", "msg": msg_id})
    b.send({"type": "hidden"})
    text = caplog.text
    assert "Secretword" not in text and "K4ABC" not in text
    for s in (a, b):
        assert s.station_id not in text
