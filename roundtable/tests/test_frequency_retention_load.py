# no-port-check: NereusSDR-original.
# 2026-10-30: retained readings and scoped fanout, J.J. Boyd / KG4VCF, Codex.
import gc
import weakref

import pytest

from inproc import Harness, live, presence, reading, slice_
from nereus_roundtable import frequency
from nereus_roundtable.rooms import Room
from test_frequency import station, watch, post, messages, HZ

@pytest.fixture
def h():
    harness=Harness()
    yield harness
    harness.close()


def full(h,reader=None):
    r=reader or live(h,"198.51.100.2")
    if not reader:
        r.send({"type":"watchBand","band":"all","frequencyVersion":1});r.take()
    r.send({"type":"watchAt","hz":HZ,"family":"usb"})
    return next(m for m in reversed(r.take()) if m.get("view")=="frequency" and m["type"]=="room")


def talking(h):
    a=station(h,keyed=True)
    b=station(h,"W4XYZ",HZ+10,grid="FN20",address="192.0.2.2")
    h.advance(10000)
    b.send(reading(0,-81,h.clock.wall_ms()))
    r=watch(h)
    row=next(m for m in full(h,r)["members"] if m["callsign"]=="W4XYZ")
    return a,b,r,row


def quiet(h,a,b):
    h.advance(1100)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ)]))
    h.advance(3000)
    b.send(reading(0,-125,h.clock.wall_ms(),False))
    assert h.service.book.rows_of(b.station_id)[0].reading_turn is None


def test_continuing_quiet_meter_preserves_original_report_and_fresh_view(h):
    a,b,r,original=talking(h)
    quiet(h,a,b)
    for _ in range(3):
        h.advance(1000);b.send(reading(0,-129,h.clock.wall_ms(),False))
    for view in (full(h,r),full(h)):
        row=next(m for m in view["members"] if m["callsign"]=="W4XYZ")
        assert row["reading"]==original["reading"]
        assert (row["km"],row["bearingDeg"])==(original["km"],original["bearingDeg"])
        assert row["reading"]["turn"] in [t["id"] for t in view["earlier"]]


def test_changed_current_identity_does_not_rewrite_retained_measurement(h):
    a,b,r,original=talking(h)
    quiet(h,a,b)
    h.advance(1100)
    a.send(presence("K4ABC/P","FN31",[slice_(0,HZ)]))
    b.send(presence("W4XYZ/P","FM18",[slice_(0,HZ+10)]))
    h.advance(1000);b.send(reading(0,-130,h.clock.wall_ms(),False))
    view=full(h,r)
    retained=next(m for m in view["members"] if m["ref"]==original["ref"])
    assert retained["callsign"]=="W4XYZ" and retained["grid"]=="FN20"
    assert retained["reading"]==original["reading"]
    assert retained["bearingDeg"]==original["bearingDeg"]
    turn=next(t for t in view["earlier"] if t["id"]==original["reading"]["turn"])
    assert turn["talkers"][0]["callsign"]=="K4ABC"
    r.send({"type":"watchBand","band":"all","frequencyVersion":1})
    directory=next(m for m in reversed(r.take()) if m["type"]=="rooms")
    assert "W4XYZ/P" in [p["callsign"] for room in directory["rooms"] for p in room["stations"]]


def test_departed_or_suppressed_reporter_is_not_invented_from_old_track(h):
    a,b,r,_=talking(h)
    quiet(h,a,b)
    with h.service.event():h.service.book.set_counted(b.station_id,False)
    assert all(m["callsign"]!="W4XYZ" for m in full(h,r)["members"])
    b.disconnect()
    assert all(m["callsign"]!="W4XYZ" for m in full(h,r)["members"])


def test_current_unknown_reading_keeps_its_own_turn_and_blank_path(h):
    a,b,r,old=talking(h)
    quiet(h,a,b)
    h.advance(1000);b.send(reading(0,-89,h.clock.wall_ms(),True))
    view=full(h,r)
    unknown=next(t for t in view["now"] if t["kind"]=="unknown")
    row=next(m for m in view["members"] if m["ref"]==old["ref"])
    assert row["reading"]["turn"]==unknown["id"] and row["reading"]["dbm"]==-89
    assert row["km"] is None and row["bearingDeg"] is None


def counters(monkeypatch):
    calls={"snapshot":[],"exact":[]}
    original=frequency.snapshot
    def snapshot(service,conn,hz,family):
        calls["snapshot"].append((hz,family))
        return original(service,conn,hz,family)
    monkeypatch.setattr(frequency,"snapshot",snapshot)
    summary=Room.summary
    def summarise(room,exact=False):
        if exact:calls["exact"].append(room.id)
        return summary(room,exact)
    monkeypatch.setattr(Room,"summary",summarise)
    return calls


def test_unrelated_readings_do_not_snapshot_selected_view_or_rebuild_directory_each_time(h,monkeypatch):
    near=station(h,keyed=True)
    reporter=station(h,"W4XYZ",HZ+10,address="192.0.2.2")
    far=station(h,"N4FAR",HZ+1000000,keyed=True,address="192.0.2.3")
    other=station(h,"N5FAR",HZ+1000010,address="192.0.2.4")
    h.advance(10000)
    r=watch(h)
    calls=counters(monkeypatch)
    packets=[]
    for i in range(8):
        h.advance(1000)
        other.send(reading(0,-80-i,h.clock.wall_ms()))
        packets+=r.take()
    assert calls["snapshot"]==[]
    assert len(calls["exact"])<=5,calls
    assert len([m for m in packets if m["type"]=="roomsDelta"])<=5
    assert not [m for m in packets if m.get("view")=="frequency"]
    calls["snapshot"].clear()
    reporter.send(reading(0,-84.5,h.clock.wall_ms()))
    relevant=[m for m in r.take() if m.get("view")=="frequency"]
    assert relevant and calls["snapshot"]==[(HZ,"usb")]
    assert any(row["reading"] and row["reading"]["dbm"]==-84.5 for row in relevant[-1]["members"])


def test_identity_tuning_and_hidden_directory_removal_are_immediate(h,monkeypatch):
    a=station(h)
    r=watch(h)
    calls=counters(monkeypatch)
    h.advance(1100)
    a.send(presence("K4ABC/P","FN20",[slice_(0,HZ+7)]))
    packets=r.take()
    directory=next(m for m in packets if m["type"]=="roomsDelta")
    people=[p for room in directory["add"]+directory["change"] for p in room["stations"]]
    assert any(p["hz"]==HZ+7 and p["callsign"]=="K4ABC/P" for p in people)
    assert len(calls["exact"])<=1,calls
    a.send({"type":"hidden"})
    assert any(m["type"]=="roomsDelta" for m in r.take())
    assert not full(h,r)["members"]


def test_keyed_only_changes_update_relevant_view_without_global_directory_rebuild(h,monkeypatch):
    a=station(h)
    far=station(h,"N4FAR",HZ+1000000,address="192.0.2.2")
    h.advance(10000)
    # Establish a normal directory sample before the keyed-only change;
    # the next change is genuinely inside the existing two-second window.
    far.send(reading(0,-120,h.clock.wall_ms(),False))
    r=watch(h)
    calls=counters(monkeypatch)
    h.advance(1100)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ,keyed=True)]))
    assert any(m.get("view")=="frequency" and m["turns"] for m in r.take())
    assert not calls["exact"],calls


@pytest.mark.parametrize("action",["remove","restore","expire","archive"])
def test_direct_store_message_changes_propagate_without_live_rf_room(h,monkeypatch,action):
    a=station(h)
    ident=post(a)["msg"]
    a.disconnect()
    r=watch(h)
    if action=="restore":
        with h.service.event():h.store.message_remove(ident,"helper")
        r.take()
    calls=counters(monkeypatch)
    with h.service.event():
        if action=="remove":h.store.message_remove(ident,"helper")
        elif action=="restore":h.store.message_restore(ident,"Hello")
        elif action=="expire":h.store.messages_expire(h.clock.wall_ms()+1)
        else:h.store.delete_archive(a.station_id)
    expected={"remove":"removed","restore":"shown","expire":"expired","archive":"deleted"}[action]
    assert any(m["id"]==ident and m["state"]==expected for m in messages(r))
    assert calls["snapshot"]==[(HZ,"usb")]


def test_unrelated_sql_and_rolled_back_messages_do_not_invalidate_frequency_view(h,monkeypatch):
    a=station(h)
    ident=post(a)["msg"]
    r=watch(h)
    calls=counters(monkeypatch)
    with h.service.event():
        h.store.station_upsert(a.station_id,"core","K4ABC","FN20",h.clock.wall_ms())
        oid=h.store.opening_open("usb-15212000","20m","usb",HZ+1000000,h.clock.wall_ms())
        h.store.turn_open(oid,"talking",[(a.station_id,"K4ABC",1)],h.clock.wall_ms())
        with pytest.raises(RuntimeError):
            with h.store.transaction():
                h.store.message_remove(ident,"helper")
                raise RuntimeError("rollback")
    assert h.store.message_get(ident)["state"]=="shown"
    assert calls["snapshot"]==[] and not messages(r)


def test_service_shutdown_does_not_leave_a_store_listener_cycle():
    h=Harness();store=h.store;reference=weakref.ref(h.service)
    h.close();del h;gc.collect()
    assert reference() is None
    store.close()


def test_live_profile_change_waits_for_a_new_actual_measurement(h):
    a,b,r,original=talking(h)
    h.advance(1100)
    b.send(presence("W4XYZ/P","FM18",[slice_(0,HZ+10)]))
    before=next(m for m in full(h,r)["members"] if m["ref"]==original["ref"])
    assert before["callsign"]=="W4XYZ" and before["grid"]=="FN20"
    assert before["reading"]==original["reading"]
    b.send(reading(0,-85,h.clock.wall_ms()))
    after=next(m for m in full(h,r)["members"] if m["ref"]==original["ref"])
    assert after["callsign"]=="W4XYZ/P" and after["grid"]=="FM18"
    assert after["reading"]["dbm"]==-85 and after["reading"]["t"]>original["reading"]["t"]
    assert after["km"]!=original["km"]


def test_doubled_sample_stays_combined_with_blank_path_and_no_archive_credit(h):
    a=station(h,keyed=True)
    b=station(h,"W4XYZ",HZ+10,address="192.0.2.2")
    c=station(h,"N4XYZ",HZ+20,grid="FN20",address="192.0.2.3")
    h.advance(10000)
    b.send(presence("W4XYZ","EM73",[slice_(0,HZ+10,keyed=True)]))
    c.send(reading(0,-65,h.clock.wall_ms()))
    view=full(h)
    turn=next(t for t in view["now"] if t["kind"]=="doubled")
    member=next(m for m in view["members"] if m["callsign"]=="N4XYZ")
    assert member["reading"]["turn"]==turn["id"] and member["reading"]["dbm"]==-65
    assert member["km"] is None and member["bearingDeg"] is None
    assert all(str(turn["id"]) not in session["turn_ids"].split() for session in h.store.db.execute("SELECT turn_ids FROM sessions"))


def test_keyed_station_cannot_publish_a_self_reading_or_hidden_report(h):
    a,b,r,original=talking(h)
    a.send(reading(0,-10,h.clock.wall_ms()))
    assert any(m["type"]=="error" and m["code"]=="slice-keyed" for m in a.take())
    assert next(m for m in full(h,r)["members"] if m["callsign"]=="K4ABC")["reading"] is None
    b.send({"type":"hidden"})
    assert all(m["ref"]!=original["ref"] for m in full(h,r)["members"])


def test_live_summary_talker_matches_frozen_turn_identity_after_profile_change(h):
    a,b,r,original=talking(h)
    h.advance(1100)
    a.send(presence("K4ABC/P","FN31",[slice_(0,HZ,keyed=True)]))
    room=h.service.book.live(f"usb-{HZ}")
    assert room.turn_object(room.engine.open[0])["talkers"][0]["callsign"]=="K4ABC"
    assert room.summary()["talkers"]==["K4ABC"]
    assert room.station_objects()[0]["callsign"]=="K4ABC/P"


@pytest.mark.parametrize("change_identity", [False, True])
def test_ending_interval_trim_restores_actual_retained_report_snapshot(h, change_identity):
    a,b,r,original=talking(h)
    h.advance(1100)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ)]))
    h.advance(1000)
    if change_identity:
        b.send(presence("W4XYZ/P","FM18",[slice_(0,HZ+10)]))
    b.send(reading(0,-125,h.clock.wall_ms(),False))
    assert h.service.book.rows_of(b.station_id)[0].reading_turn.ending
    h.advance(500)
    b.send(reading(0,-127,h.clock.wall_ms(),False))
    # A still-open continuation shows current RF; only the eventual backdated
    # end trims those samples, then restores the retained measurement.
    active=next(m for m in full(h,r)["members"] if m["ref"]==original["ref"])
    assert active["reading"]["dbm"]==-127
    h.advance(1500)
    for view in (full(h,r),full(h)):
        row=next(m for m in view["members"] if m["ref"]==original["ref"])
        assert row["reading"]==original["reading"]
        assert (row["callsign"],row["grid"],row["km"],row["bearingDeg"])==(
            original["callsign"],original["grid"],original["km"],original["bearingDeg"])
    track=h.service.book.rows_of(a.station_id)[0].room.earlier_turns()[0].tracks[b.station_id]
    assert (track.callsign,track.km,track.bearing_deg)==(
        original["callsign"],original["km"],original["bearingDeg"])


def test_ending_interval_with_no_retained_reading_does_not_invent_report(h):
    a=station(h,keyed=True)
    b=station(h,"W4XYZ",HZ+10,grid="FN20",address="192.0.2.2")
    h.advance(10000)
    a.send(presence("K4ABC","EM73",[slice_(0,HZ)]))
    h.advance(1000)
    b.send(reading(0,-125,h.clock.wall_ms(),False))
    h.advance(2000)
    for view in (full(h),full(h)):
        row=next(m for m in view["members"] if m["callsign"]=="W4XYZ")
        assert row["reading"] is None or row["reading"]["turn"] is None
        assert row["km"] is None and row["bearingDeg"] is None


def test_unknown_quiet_trim_restores_last_retained_sample(h):
    b=station(h,"W4XYZ",HZ,grid="FN20")
    h.advance(10000)
    b.send(reading(0,-81,h.clock.wall_ms(),True))
    original=next(m for m in full(h)["members"] if m["callsign"]=="W4XYZ")
    h.advance(1000)
    b.send(reading(0,-125,h.clock.wall_ms(),False))
    h.advance(2000)
    row=next(m for m in full(h)["members"] if m["callsign"]=="W4XYZ")
    assert row["reading"]==original["reading"]
    assert row["km"] is None and row["bearingDeg"] is None
