# no-port-check: NereusSDR-original.
# 2026-10-30: authenticated web actions, J.J. Boyd / KG4VCF, AI-assisted via Codex.
import pytest
from inproc import Harness, presence, slice_, live
from webclient import Browser, body_json

HZ=14212000

@pytest.fixture
def context():
    h=Harness()
    a=h.sign_in(frequency_version=1)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ)]))
    b=Browser(h)
    cookie=b.link_cookie(a)
    csrf=body_json(b.get("/v1/me",cookie))["csrf"]
    yield h,a,b,cookie,csrf
    h.close()


def add(h,s,text="Hello"):
    s.send({"type":"postAt","slice":0,"hz":HZ,"family":"usb","text":text,"clientId":"c1"})
    return next(m["msg"] for m in s.take() if m["type"]=="posted")


@pytest.mark.parametrize("route,field",[("delete","msg"),("mentionsSeen","t")])
def test_web_actions_require_session_origin_and_csrf(context,route,field):
    h,a,b,cookie,csrf=context
    value=add(h,a) if field=="msg" else h.clock.wall_ms()
    form={"csrf":csrf,field:str(value)}
    assert b.post("/v1/me/"+route,form).status==401
    assert b.post("/v1/me/"+route,dict(form,csrf="wrong"),cookie).status==403
    assert b.post("/v1/me/"+route,form,cookie,origin="https://attacker.invalid").status==403


def test_delete_uses_authenticated_owner_and_updates_frequency_tombstone(context):
    h,a,b,cookie,csrf=context
    other=h.sign_in(address="192.0.2.2",frequency_version=1)
    other.send(presence("K4ABC","FN20",[slice_(0,HZ)]))  # same callsign is not ownership
    other_id=add(h,other)
    own=add(h,a)
    r=live(h,"198.51.100.1")
    r.send({"type":"watchBand","band":"all","frequencyVersion":1});r.take()
    r.send({"type":"watchAt","hz":HZ,"family":"usb"});r.take()
    assert b.post("/v1/me/delete",{"csrf":csrf,"msg":str(other_id)},cookie).status==409
    assert h.store.message_get(other_id)["state"]=="shown"
    assert b.post("/v1/me/delete",{"csrf":csrf,"msg":str(own)},cookie).status==200
    changes=[m for packet in r.take() if packet.get("view")=="frequency" for m in packet.get("messages",[])]
    assert any(m["id"]==own and m["state"]=="deleted" for m in changes)


def test_owner_can_delete_and_mark_seen_after_radio_disconnect(context):
    h,a,b,cookie,csrf=context
    ident=add(h,a)
    a.disconnect()
    assert b.post("/v1/me/delete",{"csrf":csrf,"msg":str(ident)},cookie).status==200
    assert b.post("/v1/me/mentionsSeen",{"csrf":csrf,"t":str(h.clock.wall_ms())},cookie).status==200


def test_mentions_seen_is_bounded_and_monotonic(context):
    h,a,b,cookie,csrf=context
    now=h.clock.wall_ms()
    for invalid in (-1,now+1,"nan",""):
        assert b.post("/v1/me/mentionsSeen",{"csrf":csrf,"t":str(invalid)},cookie).status==400
    assert b.post("/v1/me/mentionsSeen",{"csrf":csrf,"t":str(now)},cookie).status==200
    assert b.post("/v1/me/mentionsSeen",{"csrf":csrf,"t":str(now-100)},cookie).status==200
    assert h.store.station_get(a.station_id)["mentions_seen"]==now


def test_follow_owns_ids_by_identity_not_callsign_and_caps_result(context):
    h,a,b,cookie,csrf=context
    own=add(h,a)
    other=h.sign_in(address="192.0.2.2",frequency_version=1)
    other.send(presence("K4ABC","FN20",[slice_(0,HZ)]))
    other_id=add(h,other)
    got=body_json(b.get("/v1/me/follow",cookie))
    assert own in got["ownMessageIds"] and other_id not in got["ownMessageIds"]
    for i in range(501):
        h.store.message_add(0,"usb-14212000",a.station_id,"K4ABC",1,i,"stored",None,hz=HZ,family="usb")
    got=body_json(b.get("/v1/me/follow",cookie))
    assert len(got["ownMessageIds"])==500
    assert a.station_id not in str(got)
