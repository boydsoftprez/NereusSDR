# no-port-check: NereusSDR-original.
"""The review list (design 7.5, 11.7, 12.2, C16, C21, D16, D26): who may
open it, Remove with its 10-minute Undo, Block, Stop counting and its live
effects, Leave, two helpers on one report, helper changes by station id,
and the log. Refusals first. Time moves only with the ManualClock."""

from __future__ import annotations

import pytest

from inproc import HOST, Harness, Station, presence, reading, slice_
from nereus_roundtable import identity
from webclient import Browser, body_json

ROOM = "usb-14212000"
UNDO_MS = 600000


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


@pytest.fixture
def browser(harness):
    return Browser(harness)


def _station(harness, callsign, hz=14212000, address="192.0.2.1", keyed=False, key=None):
    s = harness.sign_in(key=key, address=address)
    s.callsign = callsign
    s.send(presence(callsign, "EM73", [slice_(0, hz, keyed=keyed)]))
    return s


def _members(harness, *callsigns):
    """Stations in one room, members after the 10 s dwell."""
    out = [_station(harness, c, hz=14212000 + 10 * i, address=f"192.0.2.{i + 1}") for i, c in enumerate(callsigns)]
    harness.advance(10000)
    for s in out:
        s.take()
    return out


def _wait(harness, ms, *stations):
    """Advance `ms` while the stations keep sending readings, so none goes
    silent (6.4) and leaves its room."""
    step = 20000
    while ms > 0:
        n = min(step, ms)
        harness.advance(n)
        ms -= n
        for s in stations:
            s.send(reading(0, -90.0, harness.clock.wall_ms(), False))


class Helper:
    """A helper's browser: its station signs in, is listed in the config,
    and opens the review list through a sign-in link."""

    def __init__(self, harness, browser, address, config=True):
        self.harness = harness
        self.browser = browser
        self.station = harness.sign_in(address=address)
        if config:
            harness.config.helpers.append(self.station.station_id)
        self.cookie = browser.link_cookie(self.station)
        self.csrf = browser.csrf(self.cookie)
        self.id = self.station.station_id

    def get(self, target):
        return self.browser.get(target, self.cookie)

    def post(self, target, **form):
        return self.browser.post(target, dict(form, csrf=self.csrf), self.cookie)

    def entries(self):
        r = self.get("/v1/review/reports")
        assert r.status == 200, r.body
        return body_json(r)["reports"]


def _post(station, text, client_id="c1"):
    station.send({"type": "post", "room": ROOM, "text": text, "clientId": client_id})
    got = [m for m in station.take() if m["type"] == "posted"]
    assert len(got) == 1 and got[0]["ok"], got
    return got[0]["msg"]


def _ref(harness, station):
    return harness.service.book.live(ROOM).rows[station.station_id].ref


def _notices(station):
    return [(m["code"], m["until"]) for m in station.take() if m["type"] == "notice"]


def _messages_seen(got):
    return [m for msg in got if msg["type"] == "delta" for m in msg["messages"]]


def _report_message(harness, reporter, msg_id, reason="spam"):
    reporter.send({"type": "report", "target": "message", "msg": msg_id, "reason": reason})
    assert [m for m in reporter.take() if m["type"] == "error"] == []


def _report_station(harness, reporter, station):
    reporter.send({"type": "report", "target": "station", "room": ROOM, "ref": _ref(harness, station), "reason": "falseReadings"})
    assert [m for m in reporter.take() if m["type"] == "error"] == []


# ----------------------------------------------------------- refusals

ROUTES_GET = ("/v1/review/reports", "/v1/review/log", "/v1/review/stations?callsign=K4ABC")
ROUTES_POST = (
    "/v1/review/remove",
    "/v1/review/block",
    "/v1/review/stop-counting",
    "/v1/review/leave",
    "/v1/review/undo",
    "/v1/review/helpers/add",
    "/v1/review/helpers/remove",
)


def test_a_non_helper_gets_403_on_every_review_route(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    msg_id = _post(a, "buy my amplifier")
    _report_message(harness, b, msg_id)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    # b is signed in on the web but is no helper.
    cookie = browser.link_cookie(b)
    csrf = browser.csrf(cookie)
    form = {"csrf": csrf, "report": str(entry["id"]), "version": str(entry["version"]), "until": "forever",
            "action": "1", "station": b.station_id}
    for target in ROUTES_GET:
        assert browser.get(target, cookie).status == 403, target
        assert browser.get(target).status == 401, target
    for target in ROUTES_POST:
        r = browser.post(target, form, cookie)
        assert r.status == 403 and body_json(r)["error"] == "not-helper", target
        assert browser.post(target, form).status == 401, target
        # A helper's own request still needs its Origin and its CSRF token.
        assert browser.post(target, dict(form, csrf=h.csrf), h.cookie, origin="https://evil.example").status == 403
        assert browser.post(target, dict(form, csrf=csrf), h.cookie).status == 403
    # Nothing changed.
    assert harness.store.message_get(msg_id)["state"] == "shown"
    assert not harness.store.station_blocked(a.station_id, harness.clock.wall_ms())
    assert not harness.service.is_helper(b.station_id)
    assert [e["version"] for e in h.entries()] == [1]


def test_an_action_needs_a_report_and_its_version(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    msg_id = _post(a, "buy my amplifier")
    _report_message(harness, b, msg_id)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    assert h.post("/v1/review/remove", report=str(entry["id"])).status == 400
    assert h.post("/v1/review/remove", report="99", version="1").status == 404
    r = h.post("/v1/review/remove", report=str(entry["id"]), version="2")
    assert r.status == 409 and body_json(r)["action"] is None
    # Block needs a time in the future or for good.
    for until in ("", "0", str(harness.clock.wall_ms()), "soon"):
        r = h.post("/v1/review/block", report=str(entry["id"]), version="1", until=until)
        assert r.status == 400 and body_json(r)["error"] == "bad-until", until
    assert harness.store.message_get(msg_id)["state"] == "shown"


# ----------------------------------------------------------- remove


def test_remove_clears_the_text_logs_it_and_undo_within_ten_minutes_restores_it(harness, browser):
    a, b, c = _members(harness, "K4ABC", "KG4VCF", "W4XYZ")
    msg_id = _post(a, "buy my amplifier")
    b.take()
    c.take()
    _report_message(harness, b, msg_id, "spam")
    _report_message(harness, c, msg_id, "offTopic")
    h = Helper(harness, browser, "192.0.2.9")
    entries = h.entries()
    assert len(entries) == 1
    entry = entries[0]
    assert (entry["target"], entry["room"], entry["count"], entry["version"]) == ("message", ROOM, 2, 1)
    assert [r["reason"] for r in entry["reports"]] == ["spam", "offTopic"]
    assert [r["callsign"] for r in entry["reports"]] == ["KG4VCF", "W4XYZ"]
    assert entry["message"]["text"] == "buy my amplifier"
    assert entry["station"]["id"] == a.station_id

    r = h.post("/v1/review/remove", report=str(entry["id"]), version="1")
    assert r.status == 200, r.body
    answer = body_json(r)
    assert answer["report"] == {"id": entry["id"], "version": 2, "closed": True}
    row = harness.store.message_get(msg_id)
    assert (row["text"], row["state"], row["removed_by"]) == (None, "removed", h.id)
    # The row, its sender, time and reading stay.
    assert (row["station_id"], row["callsign"]) == (a.station_id, "K4ABC")
    # The room shows "Message removed": state removed, no text.
    got = b.take()
    assert [(m["id"], m["state"], m["text"]) for m in _messages_seen(got)] == [(msg_id, "removed", None)]
    assert [(m["code"], m["text"]) for m in got if m["type"] == "notice"] == [
        ("report-closed", "A report you made has been looked at.")
    ]
    assert _notices(c) == [("report-closed", None)]
    # The station reported is told nothing, and never who reported.
    assert [m for m in a.take() if m["type"] == "notice"] == []
    assert h.entries() == []
    # The log has the helper and the time.
    log = body_json(h.get("/v1/review/log"))["actions"]
    assert [(e["action"], e["helper"], e["helperCallsign"], e["t"], e["target"]) for e in log] == [
        ("remove", h.id, None, harness.clock.wall_ms(), str(msg_id))
    ]

    _wait(harness, UNDO_MS - 1, a, b, c)
    b.take()
    r = h.post("/v1/review/undo", action=str(answer["action"]["id"]), version="2")
    assert r.status == 200, r.body
    assert body_json(r)["report"] == {"id": entry["id"], "version": 3, "closed": False}
    row = harness.store.message_get(msg_id)
    assert (row["text"], row["state"], row["removed_by"]) == ("buy my amplifier", "shown", None)
    assert [(m["state"], m["text"]) for m in _messages_seen(b.take())] == [("shown", "buy my amplifier")]
    # The entry is open again, and the log shows both, newest first.
    assert [e["version"] for e in h.entries()] == [3]
    log = body_json(h.get("/v1/review/log"))["actions"]
    assert [e["action"] for e in log] == ["undo", "remove"]
    assert log[1]["undone"] == harness.clock.wall_ms()


def test_undo_at_ten_minutes_and_one_second_is_refused_and_the_text_is_gone(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    msg_id = _post(a, "buy my amplifier")
    _report_message(harness, b, msg_id)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    answer = body_json(h.post("/v1/review/remove", report=str(entry["id"]), version="1"))
    action_id = answer["action"]["id"]
    # The text is kept for the Undo only.
    assert harness.store.action_get(action_id)["undo_text"] == "buy my amplifier"
    harness.advance(UNDO_MS)
    assert harness.store.action_get(action_id)["undo_text"] == "buy my amplifier"
    harness.advance(1000)
    r = h.post("/v1/review/undo", action=str(action_id), version="2")
    assert r.status == 409 and body_json(r)["error"] == "too-late"
    assert harness.store.message_get(msg_id)["text"] is None
    assert harness.store.action_get(action_id)["undo_text"] is None
    count = harness.store.db.execute("SELECT COUNT(*) FROM review_actions WHERE undo_text IS NOT NULL").fetchone()[0]
    assert count == 0


def test_two_helpers_on_one_report_the_second_gets_409_with_the_first_action(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    msg_id = _post(a, "buy my amplifier")
    _report_message(harness, b, msg_id)
    first = Helper(harness, browser, "192.0.2.9")
    second = Helper(harness, browser, "192.0.2.10")
    seen_first = first.entries()[0]
    seen_second = second.entries()[0]
    assert seen_first["version"] == seen_second["version"] == 1
    assert first.post("/v1/review/leave", report=str(seen_first["id"]), version="1").status == 200
    r = second.post("/v1/review/remove", report=str(seen_second["id"]), version="1")
    assert r.status == 409
    got = body_json(r)
    assert (got["error"], got["version"]) == ("changed", 2)
    assert (got["action"]["action"], got["action"]["helper"]) == ("leave", first.id)
    # Leave changed nothing; the second helper's remove did not happen.
    assert harness.store.message_get(msg_id)["state"] == "shown"
    # Undo of an action that is no longer the latest is refused too.
    blocked = second.post("/v1/review/block", report=str(seen_second["id"]), version="2", until="forever")
    assert blocked.status == 200
    leave_id = body_json(first.get("/v1/review/log"))["actions"][-1]["id"]
    r = first.post("/v1/review/undo", action=str(leave_id), version="3")
    assert r.status == 409 and body_json(r)["action"]["action"] == "block"


# ----------------------------------------------------------- block


def test_block_refuses_posts_and_names_with_a_notice_and_readings_still_count(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    msg_id = _post(a, "buy my amplifier")
    b.take()
    _report_message(harness, b, msg_id)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    until = harness.clock.wall_ms() + 3600000
    r = h.post("/v1/review/block", report=str(entry["id"]), version="1", until=str(until))
    assert r.status == 200, r.body
    assert _notices(a) == [("blocked", until)]
    assert body_json(h.get("/v1/review/log"))["actions"][0]["detail"] == {
        "until": until, "forever": False, "prior": {"until": None, "forever": False}
    }
    a.send({"type": "post", "room": ROOM, "text": "again", "clientId": "c2"})
    got = [m for m in a.take() if m["type"] == "posted"]
    assert [(m["ok"], m["code"]) for m in got] == [(False, "blocked")]
    # A name is refused blocked too: an unknown turn a heard.
    b.send(reading(0, -80.0, harness.clock.wall_ms(), True))
    harness.advance(4000)
    turn_id = harness.store.db.execute("SELECT id FROM turns WHERE kind = 'unknown'").fetchone()[0]
    a.take()
    a.send({"type": "name", "turn": turn_id, "name": "W1AW"})
    assert [(m["type"], m["code"]) for m in a.take() if m["type"] == "error"] == [("error", "blocked")]
    # Its readings are still accepted and shown.
    a.send(reading(0, -71.0, harness.clock.wall_ms(), False))
    assert [m for m in a.take() if m["type"] == "error"] == []
    shown = [m for msg in b.take() if msg["type"] == "delta" for m in msg["members"] if m["callsign"] == "K4ABC"]
    assert shown and shown[-1]["reading"]["dbm"] == -71.0
    # It ends when it says.
    _wait(harness, 3600000, a, b)
    _post(a, "back again", "c3")


def test_the_blocked_notice_follows_welcome_and_rooms_at_sign_in(harness, browser):
    from cryptography.hazmat.primitives.asymmetric import ec

    key = ec.generate_private_key(ec.SECP256R1())
    a = _station(harness, "K4ABC", key=key)
    b = _station(harness, "KG4VCF", hz=14212010, address="192.0.2.2")
    harness.advance(10000)
    a.take()
    b.take()
    _report_station(harness, b, a)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    assert h.post("/v1/review/block", report=str(entry["id"]), version="1", until="forever").status == 200
    a.disconnect()
    again = Station(harness, "192.0.2.1")
    nonce = identity.from_b64url(again.take()[0]["nonce"])
    spki = identity.spki_of(key.public_key())
    again.send({
        "type": "hello", "v": 1, "software": "test", "softwareVersion": "1",
        "spki": identity.to_b64url(spki), "sig": identity.to_b64url(identity.sign_raw(key, identity.hello_transcript(HOST, nonce))),
    })
    got = again.take()
    assert [m["type"] for m in got] == ["welcome", "rooms", "notice"]
    assert (got[2]["code"], got[2]["until"]) == ("blocked", None)


# ----------------------------------------------------------- stop counting


def test_stop_counting_hides_the_station_live_and_its_keying_opens_no_turn(harness, browser):
    a, b, c = _members(harness, "K4ABC", "KG4VCF", "W4XYZ")
    room = harness.service.book.live(ROOM)
    a_ref = _ref(harness, a)
    _report_station(harness, b, a)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    r = h.post("/v1/review/stop-counting", report=str(entry["id"]), version="1", until="forever")
    assert r.status == 200, r.body
    assert _notices(a) == [("not-counted", None)]
    # Its row leaves every view; the room counts 2 members.
    got = b.take()
    assert [d["left"] for d in got if d["type"] == "delta"] == [[a_ref]]
    assert room.summary()["members"] == 2
    assert [row.callsign for row in room.shown_rows()] == ["KG4VCF", "W4XYZ"]
    # A keyed claim from it opens no turn.
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    assert room.engine.open == []
    assert harness.store.db.execute("SELECT COUNT(*) FROM turns").fetchone()[0] == 0
    harness.advance(1000)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    # Its present readings open no unknown turn either.
    a.send(reading(0, -70.0, harness.clock.wall_ms(), True))
    assert [m for m in a.take() if m["type"] == "error"] == []
    assert room.engine.open == []
    harness.advance(1000)
    assert [m for m in b.take() if m["type"] == "delta"] == []
    # During c's turn its readings are stored, marked, and credit nothing.
    c.send(presence("W4XYZ", "EM73", [slice_(0, 14212020, keyed=True)]))
    harness.advance(1000)
    a.send(reading(0, -70.0, harness.clock.wall_ms(), True))
    b.send(reading(0, -75.0, harness.clock.wall_ms(), True))
    turn = room.engine.open[0]
    assert turn.heard() == 1
    harness.advance(1000)
    seen = [m for msg in c.take() if msg["type"] == "delta" for m in msg["members"]]
    assert seen and all(m["callsign"] != "K4ABC" for m in seen)
    for s in (a, b):
        s.take()
    c.send(presence("W4XYZ", "EM73", [slice_(0, 14212020)]))
    harness.advance(3000)
    rows = harness.store.db.execute(
        "SELECT station_id, not_counted FROM turn_readings WHERE turn_id = ? ORDER BY station_id", (turn.id,)
    ).fetchall()
    assert {r[0]: r[1] for r in rows} == {a.station_id: 1, b.station_id: 0}
    # The archive and the read API leave it out.
    api = browser.get(f"/v1/api/room/usb/14212000")
    assert api.status == 200 and "K4ABC" not in [m["callsign"] for m in body_json(api)["room"]["members"]]

    # Undo lifts it: the row shows again and its keying counts.
    action = body_json(h.get("/v1/review/log"))["actions"][0]
    assert h.post("/v1/review/undo", action=str(action["id"]), version="2").status == 200
    assert not harness.store.station_not_counted(a.station_id, harness.clock.wall_ms())
    assert [row.callsign for row in room.shown_rows()] == ["K4ABC", "KG4VCF", "W4XYZ"]
    assert room.summary()["members"] == 3
    shown = [m for msg in b.take() if msg["type"] == "delta" for m in msg["members"]]
    assert [m["ref"] for m in shown if m["callsign"] == "K4ABC"] == [a_ref]
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    assert [(t.kind, t.talker.callsign) for t in room.engine.open] == [("talking", "K4ABC")]


def test_stop_counting_ends_the_turn_it_was_talking_in_and_a_time_ends_it(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    room = harness.service.book.live(ROOM)
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    assert [t.talker.callsign for t in room.engine.open] == ["K4ABC"]
    _report_station(harness, b, a)
    h = Helper(harness, browser, "192.0.2.9")
    entry = h.entries()[0]
    until = harness.clock.wall_ms() + 60000
    r = h.post("/v1/review/stop-counting", report=str(entry["id"]), version="1", until=str(until))
    assert r.status == 200, r.body
    assert _notices(a) == [("not-counted", until)]
    assert room.engine.open == []
    ended = harness.store.db.execute("SELECT kind, end_ms FROM turns").fetchall()
    assert [(k, e) for k, e in ended] == [("talking", harness.clock.wall_ms())]
    # Still keyed, still not counted: no turn.
    for step in (20000, 20000, 19999):
        _wait(harness, step, b)
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000, keyed=True)]))
    assert room.engine.open == [] and a.station_id not in [r.station_id for r in room.shown_rows()]
    # At its time it counts again, keyed: a new turn opens.
    harness.advance(1)
    assert a.station_id in [r.station_id for r in room.shown_rows()]
    assert [t.talker.callsign for t in room.engine.open] == ["K4ABC"]


# ----------------------------------------------------------- helpers


def test_helpers_are_added_and_removed_by_station_id_only_and_each_change_is_logged(harness, browser):
    a, b = _members(harness, "K4ABC", "KG4VCF")
    h = Helper(harness, browser, "192.0.2.9")
    # Picking: station ids with their callsigns.
    r = h.get("/v1/review/stations?callsign=kg4vcf/p")
    assert r.status == 200
    assert [(s["id"], s["callsign"], s["helper"]) for s in body_json(r)["stations"]] == [(b.station_id, "KG4VCF", False)]
    assert h.get("/v1/review/stations").status == 400
    # Never by callsign.
    for station in ("KG4VCF", b.station_id.upper(), b.station_id[:-1], ""):
        assert h.post("/v1/review/helpers/add", station=station).status == 400, station
    unknown = identity.key_id(b"x" * 32)
    assert h.post("/v1/review/helpers/add", station=unknown).status == 404
    r = h.post("/v1/review/helpers/add", station=b.station_id)
    assert r.status == 200 and body_json(r) == {"changed": True, "helper": True}
    assert body_json(h.post("/v1/review/helpers/add", station=b.station_id))["changed"] is False
    # The new helper opens the review list with its own session.
    cookie = browser.link_cookie(b)
    assert browser.get("/v1/review/reports", cookie).status == 200
    # The helper set in the config can't be removed here.
    r = h.post("/v1/review/helpers/remove", station=h.id)
    assert r.status == 400 and body_json(r)["error"] == "config-helper"
    r = h.post("/v1/review/helpers/remove", station=b.station_id)
    assert r.status == 200 and body_json(r) == {"changed": True, "helper": False}
    assert browser.get("/v1/review/reports", cookie).status == 403
    got = body_json(h.get("/v1/review/log"))
    assert [(e["action"], e["helper"], e["target"]) for e in got["actions"]] == [
        ("helper-remove", h.id, b.station_id),
        ("helper-add", h.id, b.station_id),
    ]
    assert [(x["id"], x["config"]) for x in got["helpers"]] == [(h.id, True)]


def test_reports_of_one_item_are_one_entry_oldest_first_with_the_count_older_than_a_day(harness, browser):
    a, b, c = _members(harness, "K4ABC", "KG4VCF", "W4XYZ")
    old = _post(a, "first")
    _report_message(harness, b, old, None)
    harness.advance(86400001)
    for s in (a, b, c):
        s.send(presence(s.callsign, "EM73", [slice_(0, 14212000 + 10 * ["K4ABC", "KG4VCF", "W4XYZ"].index(s.callsign))]))
    harness.advance(10000)
    new = _post(a, "second", "c2")
    _report_message(harness, c, new, "abuse")
    _report_message(harness, b, new, "abuse")
    h = Helper(harness, browser, "192.0.2.9")
    got = body_json(h.get("/v1/review/reports"))
    assert got["olderThanDay"] == 1
    assert [(e["message"]["id"], e["count"]) for e in got["reports"]] == [(old, 1), (new, 2)]
    assert got["reports"][0]["reports"][0]["reason"] is None
