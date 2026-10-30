# no-port-check: NereusSDR-original.
# 2026-10-30: Frequency-first regressions. J.J. Boyd / KG4VCF, AI-assisted via Codex.
import sqlite3

import pytest

from inproc import Harness, live, presence, reading, slice_
from nereus_roundtable import protocol
from nereus_roundtable.store import Store
from webclient import Browser, body_json

HZ = 14212000

@pytest.fixture
def h():
    harness = Harness()
    yield harness
    harness.close()


def station(h, call="K4ABC", hz=HZ, mode="USB", keyed=False, grid="EM73", address="192.0.2.1"):
    s = h.sign_in(address=address, frequency_version=1)
    assert s.welcome["frequencyVersion"] == 1
    s.send(presence(call, grid, [slice_(0, hz, mode, keyed)]))
    return s


def watch(h, hz=HZ, family="usb"):
    r = live(h, "198.51.100.1")
    r.send({"type": "watchBand", "band": "all", "frequencyVersion": 1})
    assert r.take()[0]["frequencyVersion"] == 1
    r.send({"type": "watchAt", "hz": hz, "family": family})
    assert r.take()[0]["view"] == "frequency"
    return r


def post(s, hz=HZ, family="usb", text="Hello", client="c1"):
    s.send({"type": "postAt", "slice": 0, "hz": hz, "family": family, "text": text, "clientId": client})
    return next(m for m in s.take() if m["type"] == "posted")


def messages(r):
    return [m for packet in r.take() if packet["type"] == "delta" and packet.get("view") == "frequency" for m in packet["messages"]]


@pytest.mark.parametrize("offset,mode,included", [(-5001,"USB",False),(-5000,"USB",True),(5000,"USB",True),(5001,"USB",False),(0,"LSB",False)])
def test_pairwise_chat_across_rooms_and_boundary(h, offset, mode, included):
    s = station(h, hz=HZ+offset, mode=mode)
    r = watch(h)
    assert post(s,HZ+offset,"usb" if mode=="USB" else "lsb")["ok"]
    got = messages(r)
    assert bool(got) == included
    if included:
        assert (got[0]["hz"],got[0]["family"]) == (HZ+offset,"usb")


def test_chain_is_not_transitive_and_history_queries_cross_rooms(h):
    a = station(h,hz=HZ)
    b = station(h,"W4XYZ",HZ+5000,address="192.0.2.2")
    c = station(h,"N4XYZ",HZ+10000,address="192.0.2.3")
    for s,hz in [(a,HZ),(b,HZ+5000),(c,HZ+10000)]:
        assert post(s,hz)["ok"]
    a.send({"type":"readAt","hz":HZ,"family":"usb"})
    full = next(m for m in a.take() if m.get("view")=="frequency")
    assert [m["hz"] for m in full["messages"]] == [HZ,HZ+5000]


def test_stale_context_and_absent_slice_refused_without_room_dwell(h):
    s = station(h)
    assert post(s,HZ+1)["ok"] is False
    assert post(s,family="lsb")["ok"] is False
    assert h.store.db.execute("SELECT count(*) FROM messages").fetchone()[0] == 0
    assert post(s)["ok"] is True  # No ten-second room membership required.
    assert h.store.message_get(1)["hz"] == HZ


def test_unnegotiated_new_kind_closes_and_legacy_frames_unchanged(h):
    s = h.sign_in()
    assert "frequencyVersion" not in s.welcome
    s.send({"type":"readAt","hz":HZ,"family":"usb"})
    s.take()
    assert s.closed == 4002


def test_directory_lists_every_slice_exactly_and_stop_counting_hides_all(h):
    s = station(h,hz=HZ+7)
    h.advance(1100)
    s.send(presence("K4ABC","EM73",[slice_(0,HZ+7),slice_(1,HZ+19)]))
    h.advance(10000)
    r = live(h,"198.51.100.1")
    r.send({"type":"watchBand","band":"all","frequencyVersion":1})
    full = r.take()[0]
    people = [p for room in full["rooms"] for p in room["stations"]]
    assert [(p["slice"],p["hz"]) for p in people] == [(0,HZ+7),(1,HZ+19)]
    assert all(p["sourceRoom"] and "station" not in p for p in people)
    with h.service.event():
        h.service.book.set_counted(s.station_id,False)
    h.advance(2000)
    r.send({"type":"watchBand","band":"all","frequencyVersion":1})
    assert not [p for room in r.take()[-1]["rooms"] for p in room["stations"]]


def test_delete_updates_frequency_watch_and_resync_retains_tombstone(h):
    s = station(h)
    r = watch(h)
    msg = post(s)["msg"]
    assert len(messages(r)) == 1
    s.send({"type":"delete","msg":msg})
    assert messages(r)[0]["state"] == "deleted"
    r.send({"type":"resync","room":f"usb-{HZ}","view":"frequency"})
    full = r.take()[-1]
    assert full["view"] == "frequency" and full["gen"] == 1
    assert full["messages"][0]["state"] == "deleted"


def test_hidden_station_cannot_post_and_its_directory_disappears(h):
    s = station(h)
    r = watch(h)
    s.send({"type":"hidden"})
    assert post(s)["code"] == "hidden"
    r.send({"type":"watchAt","hz":HZ,"family":"usb"})
    assert not r.take()[-1]["members"]


def test_last_talker_path_survives_unkey_and_departure(h):
    a = station(h,keyed=True)
    b = station(h,"W4XYZ",HZ+10,grid="FN20",address="192.0.2.2")
    h.advance(10000)
    r = watch(h)
    b.send(reading(0,-81,h.clock.wall_ms()))
    rows = [row for packet in r.take() if packet["type"]=="delta" for row in packet["members"] if row["callsign"]=="W4XYZ"]
    assert rows[-1]["bearingDeg"] is not None
    before = rows[-1]["bearingDeg"]
    h.advance(1100)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ)]))
    h.advance(3000)
    a.disconnect()
    r.send({"type":"watchAt","hz":HZ,"family":"usb"})
    full = r.take()[-1]
    row = next(m for m in full["members"] if m["callsign"]=="W4XYZ")
    assert row["bearingDeg"] == before and row["reading"]["turn"] in [t["id"] for t in full["earlier"]]


def test_legacy_message_columns_upgrade_blank_and_new_context_survives_reopen(tmp_path):
    path = str(tmp_path/"rt.sqlite")
    s = Store(path)
    old = s.message_add(1,"usb-14212000","s","K4ABC",1,0,"legacy",None)
    s.close()
    db = sqlite3.connect(path)
    db.execute("DROP INDEX messages_by_frequency")
    db.execute("ALTER TABLE messages DROP COLUMN hz")
    db.execute("ALTER TABLE messages DROP COLUMN family")
    db.close()
    s = Store(path)
    assert s.message_get(old)["hz"] is None and s.message_get(old)["text"] == "legacy"
    msg = s.message_add(1,"usb-14212000","s","K4ABC",1,1,"exact",None,hz=HZ+7,family="usb")
    s.close()
    s = Store(path)
    assert (s.message_get(msg)["hz"],s.message_get(msg)["family"]) == (HZ+7,"usb")
    s.close()


def test_owner_follow_requires_cookie_csrf_and_actual_presence(h):
    s = station(h)
    browser = Browser(h)
    assert browser.get("/v1/me/follow").status == 401
    cookie = browser.link_cookie(s)
    follow = body_json(browser.get("/v1/me/follow",cookie))
    assert follow["slices"][0]["hz"] == HZ
    assert browser.post("/v1/me/postAt",{"slice":"0","hz":str(HZ),"family":"usb","text":"Hi","clientId":"web"},cookie).status == 403
    csrf = body_json(browser.get("/v1/me",cookie))["csrf"]
    form = {"csrf":csrf,"slice":"0","hz":str(HZ),"family":"usb","text":"Hi","clientId":"web"}
    response = browser.post("/v1/me/postAt",form,cookie)
    assert response.status == 200 and body_json(response)["ok"]
    form["hz"] = str(HZ+5000)
    assert browser.post("/v1/me/postAt",form,cookie).status == 409


def test_directory_publishes_candidate_presence_without_rf_membership(h):
    s = station(h,hz=HZ+7)
    assert not h.service.book.live_rooms()
    r = live(h,"198.51.100.1")
    r.send({"type":"watchBand","band":"all","frequencyVersion":1})
    people = r.take()[0]["rooms"][0]["stations"]
    assert people[0]["hz"] == HZ+7 and people[0]["joining"]
    legacy = h.sign_in(address="192.0.2.2")
    assert legacy.rooms["rooms"] == []


@pytest.mark.parametrize("kind,body,sender",[
    ("postAt",{"slice":0,"hz":HZ,"family":"usb","text":"Hi","clientId":"c"},"station"),
    ("readAt",{"hz":HZ,"family":"usb"},"station"),
    ("unreadAt",{"hz":HZ,"family":"usb"},"station"),
    ("watchAt",{"hz":HZ,"family":"usb"},"live"),
    ("unwatchAt",{"hz":HZ,"family":"usb"},"live"),
])
def test_extension_kinds_validate_required_exact_context(kind,body,sender):
    raw = dict(body,type=kind)
    assert protocol.decode_object(raw,sender,"server") == raw
    for key in body:
        invalid = dict(raw)
        del invalid[key]
        with pytest.raises(protocol.ProtocolError):
            protocol.decode_object(invalid,sender,"server")
    for invalid_hz in (0,HZ+0.5,True,"14212000"):
        with pytest.raises(protocol.ProtocolError):
            protocol.decode_object(dict(raw,hz=invalid_hz),sender,"server")


def test_release_and_resync_distinguish_frequency_from_identical_rf_id(h):
    s = station(h)
    h.advance(10000)
    r = watch(h)
    r.send({"type":"watch","room":f"usb-{HZ}"})
    assert r.take()[-1].get("view") is None
    r.send({"type":"unwatchAt","hz":HZ,"family":"usb"})
    assert not r.conn.frequency_views and f"usb-{HZ}" in r.conn.views
    r.send({"type":"watchAt","hz":HZ,"family":"usb"})
    r.take()
    r.send({"type":"unwatch","room":f"usb-{HZ}"})
    assert r.conn.frequency_views and not r.conn.views
    r.send({"type":"resync","room":f"usb-{HZ}","view":"frequency"})
    assert r.take()[-1]["view"] == "frequency"


def test_frequency_view_limit_and_unread_release(h):
    s = station(h)
    for i in range(h.config.reads):
        s.send({"type":"readAt","hz":HZ+i,"family":"usb"})
        s.take()
    s.send({"type":"readAt","hz":HZ+20,"family":"usb"})
    assert s.take()[-1]["type"] == "error"
    s.send({"type":"unreadAt","hz":HZ,"family":"usb"})
    s.send({"type":"readAt","hz":HZ+20,"family":"usb"})
    assert s.take()[-1]["view"] == "frequency"


def test_frequency_deltas_scope_left_refs_to_the_source_room(h):
    s = station(h)
    r = watch(h)
    s.disconnect()
    packet = next(m for m in r.take() if m.get("view")=="frequency")
    assert packet["left"] and packet["left"][0].startswith("usb-") and "/" in packet["left"][0]
    invalid = dict(packet,left=["abcdefgh"])
    with pytest.raises(protocol.ProtocolError):
        protocol.decode_object(invalid,"server","live")


def test_moderation_expiry_and_mentions_keep_frequency_context(h):
    s = station(h)
    recipient = station(h,"W4XYZ",HZ+5000,address="192.0.2.2")
    r = watch(h)
    ident = post(s,text="W4XYZ good signal")["msg"]
    mention = next(m for m in recipient.take() if m["type"]=="mention")
    assert mention["message"]["hz"] == HZ
    messages(r)
    with h.service.event():
        assert h.store.message_remove(ident,"helper")
        h.service.message_changed(ident)
    assert messages(r)[0]["state"] == "removed"
    with h.service.event():
        assert h.store.message_restore(ident,"W4XYZ good signal")
        h.service.message_changed(ident)
    assert messages(r)[0]["state"] == "shown"
    with h.service.event():
        h.store.messages_expire(h.clock.wall_ms()+1)
    assert messages(r)[0]["state"] == "expired"


def test_public_frequency_api_browses_persisted_context_without_tuning(h):
    s = station(h)
    assert post(s)["ok"]
    s.disconnect()
    browser = Browser(h)
    result = body_json(browser.get(f"/v1/api/frequency/usb/{HZ}"))
    assert result["frequencyVersion"] == 1 and result["room"]["messages"][0]["hz"] == HZ
    assert browser.get("/v1/api/frequency/usb/0").status == 400


def test_directed_bearing_is_saved_in_archive_and_missing_grid_stays_blank(h):
    a = station(h,keyed=True)
    b = station(h,"W4XYZ",HZ+10,grid="FN20",address="192.0.2.2")
    h.advance(10000)
    b.send(reading(0,-80,h.clock.wall_ms()))
    turn = h.service.book.live(f"usb-{HZ}").engine.open[0]
    expected = turn.tracks[b.station_id].bearing_deg
    h.advance(1100)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ)]))
    h.advance(3000)
    track = h.store.tracks_of(turn.id)[0]
    assert track["bearing_deg"] == expected
    browser = Browser(h)
    cookie = browser.link_cookie(a)
    session = h.store.sessions_of(a.station_id)[0]
    detail = body_json(browser.get(f"/v1/me/session/{session['id']}",cookie))
    assert detail["readers"][0]["bearingDeg"] == expected


@pytest.mark.parametrize("grid",[None,"EM73"])
def test_missing_or_coincident_grid_never_invents_bearing(h, grid):
    a = station(h,keyed=True)
    b = station(h,"W4XYZ",HZ+10,grid=grid,address="192.0.2.2")
    h.advance(10000)
    r = watch(h)
    b.send(reading(0,-80,h.clock.wall_ms()))
    row = next(row for packet in r.take() if packet.get("view")=="frequency" for row in packet.get("members",[]) if row["callsign"]=="W4XYZ")
    assert row["bearingDeg"] is None
    if grid is None:
        assert row["km"] is None


def test_posting_secondary_slice_does_not_attach_primary_slice_meter(h):
    a = station(h,keyed=True)
    b = station(h,"W4XYZ",HZ+10,address="192.0.2.2")
    h.advance(10000)
    b.send(presence("W4XYZ","EM73",[slice_(0,HZ+10),slice_(1,HZ+20)]))
    b.send(reading(0,-81,h.clock.wall_ms()))
    b.take()
    b.send({"type":"postAt","slice":1,"hz":HZ+20,"family":"usb","text":"secondary","clientId":"secondary"})
    answer = next(m for m in b.take() if m["type"]=="posted")
    stored = h.store.message_get(answer["msg"])
    assert stored["hz"] == HZ+20 and stored["dbm"] is None


@pytest.mark.parametrize("missing", ["slice", "hz", "family", "text", "clientId"])
def test_web_post_requires_every_context_and_message_field(h, missing):
    s = station(h)
    browser = Browser(h)
    cookie = browser.link_cookie(s)
    csrf = body_json(browser.get("/v1/me", cookie))["csrf"]
    form = {"csrf":csrf,"slice":"0","hz":str(HZ),"family":"usb","text":"Hi","clientId":"web"}
    del form[missing]
    assert browser.post("/v1/me/postAt",form,cookie).status == 400
    assert h.store.db.execute("SELECT count(*) FROM messages").fetchone()[0] == 0


def test_legacy_connections_do_not_enter_frequency_recompute_registry(h):
    old = h.sign_in()
    new = station(h,address="192.0.2.2")
    assert old.conn not in h.service.frequency_connections
    assert new.conn in h.service.frequency_connections
    new.disconnect()
    assert not h.service.frequency_connections
