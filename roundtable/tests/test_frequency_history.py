# no-port-check: NereusSDR-original.
# 2026-10-30: bounded retained-message status, J.J. Boyd / KG4VCF, Codex.
import pytest

from inproc import Harness, live, presence, slice_
from nereus_roundtable import protocol
from nereus_roundtable.store import Store
from test_frequency import HZ, station, watch, post


@pytest.fixture
def h():
    harness=Harness()
    yield harness
    harness.close()


def history(reader, ids, client="status-1"):
    reader.send({"type":"history","clientId":client,"ids":ids})
    return next(m for m in reader.take() if m["type"]=="history")


@pytest.mark.parametrize("receiver",["station","live"])
def test_history_follows_received_id_after_retune_and_tombstones(h,receiver):
    sender=station(h)
    ident=post(sender,text="Received before retuning")["msg"]
    reader=station(h,"W4XYZ",HZ+500000,address="192.0.2.2") if receiver=="station" else watch(h,HZ+500000)
    for action,expected in ((None,"shown"),("remove","removed"),("restore","shown"),("delete","deleted")):
        with h.service.event():
            if action=="remove":h.store.message_remove(ident,"helper")
            elif action=="restore":h.store.message_restore(ident,"Received before retuning")
            elif action=="delete":h.store.message_delete(ident,sender.station_id)
        answer=history(reader,[ident,99999],"correlation-"+expected)
        assert answer=={"type":"history","clientId":"correlation-"+expected,"states":[
            {"id":ident,"state":expected,"text":"Received before retuning" if expected=="shown" else None},
            {"id":99999,"state":"expired","text":None}]}


def test_history_observes_other_store_expiry_without_rf_touch(tmp_path):
    path=str(tmp_path/"history.sqlite")
    store=Store(path)
    harness=Harness(store=store)
    other=Store(path)
    try:
        sender=station(harness)
        ident=post(sender)["msg"]
        reader=watch(harness,HZ+500000)
        before=harness.service.band_builds
        other.messages_expire(harness.clock.wall_ms()+1)
        assert history(reader,[ident])["states"]==[{"id":ident,"state":"expired","text":None}]
        assert harness.service.band_builds==before
    finally:
        other.close();harness.close();store.close()


def test_history_only_returns_requested_status_and_no_identity_or_discovery(h):
    sender=station(h)
    wanted=post(sender,text="Wanted",client="wanted")["msg"]
    secret=post(sender,text="Not requested",client="not-requested")["msg"]
    for i in range(60):
        h.store.message_add(0,f"usb-{HZ}",sender.station_id,"K4ABC",1,h.clock.wall_ms(),"Newer unrelated text",None,hz=HZ,family="usb")
    reader=watch(h,HZ+500000)
    writes=h.store.db.total_changes
    builds=h.service.band_builds
    views=dict(reader.conn.frequency_views)
    assert history(reader,[wanted])["states"]==[{"id":wanted,"state":"shown","text":"Wanted"}]
    assert history(reader,[])["states"]==[]
    assert h.store.db.total_changes==writes and h.service.band_builds==builds
    assert reader.conn.frequency_views==views and secret!=wanted


@pytest.mark.parametrize("ids",[[True],[1.0],[-1],[0],["1"],[1,1],list(range(1,202)),None,{}])
@pytest.mark.parametrize("sender",["station","live"])
def test_history_rejects_bad_or_unbounded_ids(ids,sender):
    with pytest.raises(protocol.ProtocolError):
        protocol.decode_object({"type":"history","clientId":"known","ids":ids},sender,"server")


@pytest.mark.parametrize("sender",["station","live"])
def test_history_accepts_two_hundred_and_empty_ids(sender):
    for ids in ([],list(range(1,201))):
        msg={"type":"history","clientId":"known","ids":ids}
        assert protocol.decode_object(msg,sender,"server")==msg


@pytest.mark.parametrize("kind",["station","live"])
def test_history_requires_frequency_negotiation_and_keeps_legacy_tables(h,kind):
    reader=h.sign_in() if kind=="station" else live(h)
    reader.send({"type":"history","clientId":"old","ids":[]})
    reader.take()
    assert reader.closed==4002
    assert "history" not in protocol.TABLES[(kind,"server")]
    assert "history" not in protocol.TABLES[("server",kind)]


def test_history_read_only_away_and_hidden_existing_policy(h):
    reader=station(h)
    h.advance(1100)
    reader.send(presence("K4ABC","EM73",[slice_(0,HZ)],at_radio=False))
    assert history(reader,[99])["states"]==[{"id":99,"state":"expired","text":None}]
    reader.send({"type":"hidden"});reader.take()
    reader.send({"type":"history","clientId":"hidden","ids":[]})
    response=reader.take()[-1]
    assert response["type"]=="error" and response["code"]=="hidden"
