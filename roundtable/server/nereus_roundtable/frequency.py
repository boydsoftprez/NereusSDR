# no-port-check: NereusSDR-original.
# 2026-10-30: J.J. Boyd / KG4VCF, AI-assisted via Codex.
"""Negotiated frequency views; RF rooms remain independent turn owners."""
from . import protocol
from .rooms import band_of, message_object


def context_id(hz, family):
    return f"{family}-{hz}"


def nearby(hz, family, other_hz, other_family):
    return family == other_family and abs(hz - other_hz) <= protocol.CHAT_RADIUS_HZ


def snapshot(service, conn, hz, family):
    members, now, earlier = [], [], []
    for room in service.book.rooms.values():
        if room.family != family:
            continue
        selected_turns = []
        selected_objects = []
        for turn in room.now_turns() + room.earlier_turns():
            obj = room.turn_object(turn, turn.given.get(conn.station_id or ""))
            if nearby(hz, family, obj["hz"], room.family):
                selected_turns.append(turn.id)
                selected_objects.append(turn)
                (now if turn.end is None else earlier).append(obj)
        open_turns = [turn for turn in selected_objects if turn.end is None]
        last = (min(open_turns, key=lambda turn: abs(room.turn_object(turn)["hz"] - hz)) if open_turns
                else max(selected_objects, key=lambda turn: turn.end or turn.start, default=None))
        for row in room.shown_rows():
            member = room.member_object(row, row.station_id == conn.station_id)
            # Current attributed samples keep the identity they measured under.
            # Quiet reads may use the ended turn's last actual sample, but never
            # borrow a different turn's reading or synthesize a departed row.
            attributed = row.reading_turn
            target = attributed if attributed is not None and attributed.id in selected_turns else None
            if target is None and not open_turns and last is not None and attributed is None:
                target = last
            track = target.tracks.get(row.station_id) if target is not None else None
            retained = getattr(track, "reporter_object", None)
            if retained is not None and not track.not_counted and retained["ref"] == row.ref:
                member = dict(retained, you=row.station_id == conn.station_id)
            if (nearby(hz, family, member["hz"], room.family)
                    or member["reading"] is not None and member["reading"]["turn"] in selected_turns):
                members.append(member)
    records = []
    for row in service.store.messages_at(hz, family):
        room = service.book.rooms.get(row["room_id"])
        records.append(room.message_object(row) if room is not None else message_object(row))
    return {"id": context_id(hz, family), "opening": 1, "band": band_of(hz),
            "family": family, "hz": hz, "members": members,
            "now": sorted(now, key=lambda t: (t["start"],t["id"])),
            "earlier": sorted(earlier, key=lambda t: (-t["end"],-t["id"])),
            "messages": records, "view": "frequency"}


def member_key(member):
    return member["sourceRoom"] + "/" + member["ref"]


def changes(old, new):
    previous = {member_key(m): m for m in old["members"]}
    current = {member_key(m): m for m in new["members"]}
    old_turns = {t["id"]: t for t in old["now"] + old["earlier"]}
    old_messages = {m["id"]: m for m in old["messages"]}
    return {"room": new["id"], "view": "frequency", "closed": False,
            "members": [m for key,m in current.items() if previous.get(key) != m],
            "left": [key for key in previous if key not in current],
            "turns": [t for t in new["now"] + new["earlier"] if old_turns.get(t["id"]) != t],
            "messages": [m for m in new["messages"] if old_messages.get(m["id"]) != m]}
