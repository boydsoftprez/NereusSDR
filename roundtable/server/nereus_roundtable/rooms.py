# no-port-check: NereusSDR-original.
"""Rooms: placing slices, joining and leaving, opening and closing, and what
each room shows (wire spec 6.1 to 6.4, 6.8, 6.11).

A room is one family and one center frequency. A slice is placed in the
nearest room of its family within the family's half-width (the lower center
on a tie), or forms a new candidate room on its own frequency. A candidate
is invisible until one of its slices becomes a member (10 s of dwell, or at
once on keying); it then opens with a new opening id. A live room closes 10
minutes after its last member left.

One station has at most one row in a room: its counted slice there, the
first of its slices in that room in the order of its latest presence. A row
keeps its ref and nth while it stays; refs are new for every opening.

Live rooms live in memory. Openings, turns and reading tracks are written
to the store; on a restart the rooms active in the last 10 minutes are
reloaded, live and empty, with their ids, centers and opening ids.
"""

from __future__ import annotations

import base64
import logging
import os
from collections import OrderedDict
from typing import Any, Callable, Dict, List, Optional, Set, Tuple

from . import grid as gridlib
from .turns import Reading, Turn, TurnEngine

log = logging.getLogger("nereus_roundtable")

# Wire spec 6.1: mode to family and half-width in Hz.
FAMILIES: Dict[str, Tuple[str, int]] = {
    "LSB": ("lsb", 1500),
    "USB": ("usb", 1500),
    "DIGL": ("digl", 1500),
    "DIGU": ("digu", 1500),
    "AM": ("am", 3000),
    "SAM": ("am", 3000),
    "FM": ("fm", 6000),
    "CWL": ("cw", 300),
    "CWU": ("cw", 300),
    "FDVL": ("fdvl", 1500),
    "FDVU": ("fdvu", 1500),
}
WIDTHS: Dict[str, int] = {family: width for family, width in FAMILIES.values()}
DIGITAL = frozenset({"digl", "digu"})

# Wire spec 6.2, edges included.
BANDS: Tuple[Tuple[str, int, int], ...] = (
    ("2200m", 135700, 137800),
    ("630m", 472000, 479000),
    ("160m", 1800000, 2000000),
    ("80m", 3500000, 4000000),
    ("60m", 5250000, 5450000),
    ("40m", 7000000, 7300000),
    ("30m", 10100000, 10150000),
    ("20m", 14000000, 14350000),
    ("17m", 18068000, 18168000),
    ("15m", 21000000, 21450000),
    ("12m", 24890000, 24990000),
    ("10m", 28000000, 29700000),
    ("6m", 50000000, 54000000),
    ("4m", 70000000, 71000000),
    ("2m", 144000000, 148000000),
    ("1.25m", 222000000, 225000000),
    ("70cm", 420000000, 450000000),
    ("33cm", 902000000, 928000000),
    ("23cm", 1240000000, 1300000000),
)

# Wire spec 9.2.
JOIN_DWELL_MS = 10000
SILENT_LEAVE_MS = 30000
CLOSE_AFTER_MS = 600000
FOLD_AFTER_MS = 7200000
EARLIER_MS = 1800000
FLUSH_MS = 30000
VIEW_MESSAGES = 50
# Wire spec 6.11 "Summary keys": a summary's readings strip and its last
# message's text are cut to these.
SUMMARY_READINGS = 12
SUMMARY_TEXT_CODE_POINTS = 80

REF_BYTES = 5  # 40 bits: 8 base32 characters (4.4)


def band_of(hz: int) -> str:
    for name, lo, hi in BANDS:
        if lo <= hz <= hi:
            return name
    return "other"


def offset_hz(hz: int, center: int) -> int:
    """The slice's frequency less the center, to the nearest 10 Hz, halves
    away from zero (6.8)."""
    d = hz - center
    q = (abs(d) + 5) // 10 * 10
    return q if d >= 0 else -q


def message_object(row: Dict[str, Any], ref: Optional[str] = None) -> Dict[str, Any]:
    """A stored message row as the wire's message object (5.2): `text` only
    while `state` is `shown`."""
    return {
        "id": int(row["id"]),
        "ref": ref,
        "callsign": row["callsign"],
        "nth": int(row["nth"]),
        "t": int(row["t"]),
        "state": row["state"],
        "text": row["text"] if row["state"] == "shown" else None,
        "dbm": row["dbm"],
        "hz": row.get("hz"),
        "family": row.get("family"),
        "sourceRoom": row["room_id"],
    }


class Profile:
    """What a station's rows show about it, from its latest presence."""

    def __init__(
        self,
        callsign: str,
        grid: Optional[str],
        away: bool,
        software: str,
        software_version: str,
        owner: Any = None,
    ) -> None:
        self.callsign = callsign
        self.grid = grid
        self.away = away
        self.software = software
        self.software_version = software_version
        self.owner = owner


class Placement:
    """One slice of one station in one room (live or candidate)."""

    def __init__(self, station_id: str, slice_id: int, hz: int, family: str, keyed: bool, room: "Room", entered: int) -> None:
        self.station_id = station_id
        self.slice_id = slice_id
        self.hz = hz
        self.family = family
        self.keyed = keyed
        self.room = room
        self.entered = entered
        self.member = False
        self.timer: Any = None


class Row:
    """A station's row in one room: its counted slice there."""

    def __init__(self, room: "Room", station_id: str, profile: Profile, placement: Placement, ref: str, nth: int) -> None:
        self.room = room
        self.station_id = station_id
        self.owner = profile.owner
        self.ref = ref
        self.nth = nth
        self.callsign = profile.callsign
        self.grid = profile.grid
        self.away = profile.away
        self.software = profile.software
        self.software_version = profile.software_version
        self.slice_id = placement.slice_id
        self.hz = placement.hz
        self.keyed = placement.keyed
        self.member = False
        self.reading: Optional[Reading] = None
        self.reading_turn: Optional[Turn] = None
        # False once the row has left its room.
        self.present = True
        # False while a helper has stopped counting the station (design
        # 11.7, C21): the row is then in no view, and the turn rules treat
        # it as absent.
        self.counted = True

    @property
    def joining(self) -> bool:
        return not self.member


class RoomDiff:
    """What changed in one room since its last delta (6.11): the changed
    rows with their member objects (`you` false), the rows that left, the
    changed turn objects (`mine` null) with their turns, the new or changed
    message objects, whether the room closed, and whether its summary
    changed."""

    def __init__(
        self,
        members: List[Tuple[Row, Dict[str, Any]]],
        left: List[Row],
        turns: List[Dict[str, Any]],
        closed: bool,
        band: bool,
        turn_of: Optional[List[Turn]] = None,
        messages: Optional[List[Dict[str, Any]]] = None,
    ) -> None:
        self.members = members
        self.left = left
        self.turns = turns
        self.closed = closed
        self.band = band
        self.turn_of = turn_of or []
        self.messages = messages or []

    def turns_for(self, station_id: Optional[str]) -> List[Dict[str, Any]]:
        """The changed turn objects as one station receives them: `mine` is
        its own name for the turn (6.9)."""
        if station_id is None or not self.turn_of:
            return self.turns
        return [dict(obj, mine=turn.given.get(station_id)) for obj, turn in zip(self.turns, self.turn_of)]

    def __bool__(self) -> bool:
        return bool(self.members or self.left or self.turns or self.messages or self.closed)


class _StationEntry:
    def __init__(self, profile: Optional[Profile]) -> None:
        self.profile = profile
        self.placements: Dict[int, Placement] = {}
        self.order: List[int] = []


class Room:
    def __init__(self, book: "RoomBook", family: str, center: int) -> None:
        self.book = book
        self.family = family
        self.center = center
        self.id = f"{family}-{center}"
        self.band = band_of(center)
        self.width = WIDTHS[family]
        self.digital = family in DIGITAL
        self.live = False
        self.opening_id: Optional[int] = None
        self.opened: Optional[int] = None
        self.last = 0
        self.rows: "OrderedDict[str, Row]" = OrderedDict()
        self.placements: Dict[Tuple[str, int], Placement] = {}
        self.refs: Set[str] = set()
        self.close_timer: Any = None
        self.engine = TurnEngine(self, book.clock)
        self.last_turn_start: Optional[int] = None
        # The end of the newest ended turn in this opening (summary `ended`).
        self.last_turn_end: Optional[int] = None
        # Messages posted in the current opening (6.11), the rows that
        # posted them in this process (for each message's `ref`, 6.10), and
        # the message objects changed since the last delta.
        self.message_count = 0
        self.message_rows: Dict[int, Row] = {}
        # The newest stored message of the room id, any opening, for the
        # summary's `lastMessage` (6.11); loaded when the room opens.
        self.last_message: Optional[Dict[str, Any]] = None
        self.pending_messages: "OrderedDict[int, Dict[str, Any]]" = OrderedDict()
        # What the room's viewers last received (6.11).
        self.version = 0
        self.sent_members: Dict[str, Tuple[Row, Dict[str, Any]]] = {}
        self.sent_turns: Dict[int, Dict[str, Any]] = {}
        self.sent_summary: Optional[Dict[str, Any]] = None
        self.closed_now = False

    # ------------------------------------------------------------ turns

    def turn_opened(self, turn: Turn) -> None:
        turn.id = self.book.store.turn_open(self.opening_id or 0, turn.kind, self._talker_pairs(turn), turn.start)
        self._freeze_talker_identity(turn)
        self.book.store.turn_endpoints(turn.id, turn.endpoints)
        # Who may name it later (6.9): every member now, and each row that
        # becomes a member while it is open (_became_member).
        turn.members_during = {row.station_id for row in self.members()}
        self.book.store.turn_members(turn.id, turn.members_during)
        turn.stored_talkers = len(turn.talkers)  # type: ignore[attr-defined]
        self.last = max(self.last, turn.start)
        self.last_turn_start = turn.start
        self.book.touch(self)
        self.book.ensure_tick()

    def turn_changed(self, turn: Turn) -> None:
        self._freeze_talker_identity(turn)
        if getattr(turn, "stored_talkers", 0) != len(turn.talkers):
            self.book.store.turn_talkers(turn.id, self._talker_pairs(turn))
            self.book.store.turn_endpoints(turn.id, turn.endpoints)
            turn.stored_talkers = len(turn.talkers)  # type: ignore[attr-defined]
        self.book.touch(self)

    def turn_ended(self, turn: Turn, dropped: List[Tuple[str, Reading]]) -> None:
        assert turn.end is not None
        self.book.store.turn_end(turn.id, turn.end)
        self.turn_flush(turn)
        # Stored now, and an ended turn is written no more: 30 minutes of
        # readings held per earlier turn was most of the server's memory.
        for track in turn.tracks.values():
            track.release()
        self.book.earlier.setdefault(self.id, []).append(turn)
        self.last_turn_end = turn.end
        gone = {id(reading) for _, reading in dropped}
        for row in self.rows.values():
            if row.reading_turn is turn and id(row.reading) in gone:
                # Stored with no turn after all (6.7).
                row.reading_turn = None
        self.last = max(self.last, self.book.clock.wall_ms())
        self.book.touch(self)

    def turn_flush(self, turn: Turn) -> None:
        rows = []
        for track in turn.tracks.values():
            packed = track.pack(turn.start)
            if packed is None:
                continue
            first, last, dbm, bits = packed
            rows.append(
                (track.station_id, track.callsign, track.offset_hz, track.km, first, last, dbm, bits, track.not_counted, track.away, track.bearing_deg)
            )
        self.book.store.tracks_write(turn.id, rows)

    @staticmethod
    def _freeze_talker_identity(turn: Turn) -> None:
        # Endpoints describe the turn, not a later presence/profile update.
        for row, endpoint in zip(turn.talkers, turn.endpoints):
            endpoint.setdefault("callsign", row.callsign)
            endpoint.setdefault("nth", row.nth)

    @staticmethod
    def _talker_pairs(turn: Turn) -> List[Tuple[str, str, int]]:
        return [(row.station_id, endpoint.get("callsign", row.callsign), endpoint.get("nth", row.nth))
                for row, endpoint in zip(turn.talkers, turn.endpoints)]

    # ------------------------------------------------------------ views

    def members(self) -> List[Row]:
        return [row for row in self.rows.values() if row.member]

    def shown_rows(self) -> List[Row]:
        """The rows views show: every row but those of a station a helper
        stopped counting (wire spec 6.4 "Not counted")."""
        return [row for row in self.rows.values() if row.counted]

    def km_of(self, row: Row) -> Optional[int]:
        """Wire spec 6.8: with exactly one talking turn open, the distance
        from the row's grid to the talker's; otherwise null."""
        talking = self.engine.talking()
        if len(talking) != 1:
            return None
        talker = talking[0].talker
        if talker is None or not talker.present or talker.station_id == row.station_id:
            return None
        return gridlib.distance_km(row.grid, talker.grid)

    def path_of(self, row: Row, turn: Optional[Turn]) -> Tuple[Optional[int], Optional[int]]:
        if turn is None or turn.kind != "talking" or len(turn.endpoints) != 1 or turn.talker.station_id == row.station_id:
            return None, None
        endpoint = turn.endpoints[0]
        bearing = gridlib.initial_bearing_deg(endpoint["grid"], row.grid)
        rounded = int(bearing + 0.5) % 360 if bearing is not None else None
        return gridlib.distance_km(endpoint["grid"], row.grid), rounded

    def station_objects(self) -> List[Dict[str, Any]]:
        """Exact public presence for every slice, independent of RF counting."""
        out = []
        for sid, row in self.rows.items():
            if not row.counted:
                continue
            entry = self.book.stations.get(sid)
            if entry is None:
                continue
            for slice_id in entry.order:
                p = entry.placements.get(slice_id)
                if p is not None and p.room is self:
                    out.append({"ref": row.ref, "slice": p.slice_id, "sourceRoom": self.id,
                                "hz": p.hz, "family": p.family, "callsign": row.callsign,
                                "nth": row.nth, "grid": row.grid, "keyed": p.keyed,
                                "away": row.away, "joining": not p.member})
        return out

    def member_object(self, row: Row, you: bool = False) -> Dict[str, Any]:
        r = row.reading
        reading = None
        if r is not None:
            reading = {
                "dbm": r.dbm,
                "t": r.t,
                "present": r.present,
                "turn": row.reading_turn.id if row.reading_turn is not None else None,
            }
        track = row.reading_turn.tracks.get(row.station_id) if row.reading_turn is not None else None
        return {
            "ref": row.ref,
            "sourceRoom": self.id,
            "slice": row.slice_id,
            "hz": row.hz,
            "bearingDeg": track.bearing_deg if track is not None else None,
            "callsign": row.callsign,
            "nth": row.nth,
            "grid": row.grid,
            "km": track.km if track is not None else self.km_of(row),
            "offsetHz": offset_hz(row.hz, self.center),
            "joining": not row.member,
            "away": row.away,
            "keyed": row.keyed,
            "software": row.software,
            "softwareVersion": row.software_version,
            "reading": reading,
            "you": you,
        }

    def turn_object(self, turn: Turn, mine: Optional[str] = None) -> Dict[str, Any]:
        """The turn as a viewer receives it; `mine` is that viewer's own
        name for it, or None."""
        return {
            "id": turn.id,
            "kind": turn.kind,
            "sourceRoom": self.id,
            "hz": turn.endpoints[0]["hz"] if turn.kind == "talking" and turn.endpoints else self.center,
            "talkers": [
                dict({"ref": t.ref if t.present and t.room is self else None, "callsign": t.callsign, "nth": t.nth}, **endpoint)
                for t, endpoint in zip(turn.talkers, turn.endpoints)
            ],
            "start": turn.start,
            "end": turn.end,
            "ending": turn.ending,
            "names": [dict(n) for n in turn.names],
            "heard": turn.heard(),
            "mine": mine,
        }

    def message_object(self, row: Dict[str, Any]) -> Dict[str, Any]:
        """A stored message as a viewer receives it (5.2, 6.10). `ref` is the
        sender's row ref while the message belongs to the current opening
        and that row is still in the room."""
        sender = self.message_rows.get(int(row["id"]))
        ref = None
        if (
            sender is not None
            and self.live
            and row["opening_id"] == self.opening_id
            and sender.present
            and sender.counted
            and self.rows.get(sender.station_id) is sender
        ):
            ref = sender.ref
        return message_object(row, ref)

    def recent_messages(self) -> List[Dict[str, Any]]:
        """The last 50 messages of the room id, any opening, oldest first
        (5.4.5)."""
        return [self.message_object(r) for r in self.book.store.messages_of_room(self.id, VIEW_MESSAGES)]

    def message_changed(self, row: Dict[str, Any]) -> None:
        """A message was posted, or its state changed: its object goes in
        the room's next delta, and in the summary when it is the newest."""
        self.pending_messages[int(row["id"])] = self.message_object(row)
        if self.last_message is None or int(row["id"]) >= int(self.last_message["id"]):
            self.last_message = row
        self.book.touch(self)

    def load_last_message(self) -> None:
        newest = self.book.store.messages_of_room(self.id, 1)
        self.last_message = newest[-1] if newest else None

    def now_turns(self) -> List[Turn]:
        return sorted(self.engine.open, key=lambda t: (t.start, t.id))

    def earlier_turns(self, now: Optional[int] = None) -> List[Turn]:
        """Turns of this room id that ended in the last 30 minutes, newest
        first (5.4.5)."""
        now = self.book.clock.wall_ms() if now is None else now
        kept = [t for t in self.book.earlier.get(self.id, []) if t.end is not None and t.end >= now - EARLIER_MS]
        self.book.earlier[self.id] = kept
        return sorted(kept, key=lambda t: (t.start, t.id), reverse=True)

    def state(self) -> str:
        kinds = {t.kind for t in self.engine.open}
        for kind in ("talking", "doubled", "unknown"):
            if kind in kinds:
                return kind
        return "quiet"

    def folded(self) -> bool:
        """Wire spec 6.3: every member AWAY, open at least 2 hours, and no
        turn started in the last 2 hours."""
        members = self.members()
        if not members or not all(row.away for row in members) or self.opened is None:
            return False
        now = self.book.clock.wall_ms()
        if now - self.opened < FOLD_AFTER_MS:
            return False
        return self.last_turn_start is None or now - self.last_turn_start >= FOLD_AFTER_MS

    def strip(self, turn: Turn) -> List[float]:
        """Wire spec 6.11: the dBm of each member's latest reading stored
        with the open turn, the turn's talkers left out, nearest first and
        rows with no distance after in callsign order (6.8), at most 12."""
        talkers = {id(row) for row in turn.talkers}
        rows = [
            row
            for row in self.members()
            if row.counted and row.reading is not None and row.reading_turn is turn and id(row) not in talkers
        ]
        keyed = [(self.km_of(row), row) for row in rows]
        keyed.sort(key=lambda kr: (kr[0] is None, kr[0] if kr[0] is not None else 0, kr[1].callsign, kr[1].nth))
        return [row.reading.dbm for _, row in keyed[:SUMMARY_READINGS]]  # type: ignore[union-attr]

    def last_message_line(self) -> Optional[Dict[str, Any]]:
        """Wire spec 6.11: the newest message of the room id, its text cut
        to its first 80 code points."""
        m = self.last_message
        if m is None:
            return None
        shown = m["state"] == "shown"
        return {
            "callsign": m["callsign"],
            "nth": int(m["nth"]),
            "t": int(m["t"]),
            "state": m["state"],
            "text": m["text"][:SUMMARY_TEXT_CODE_POINTS] if shown else None,
        }

    def summary(self, exact: bool = False) -> Dict[str, Any]:
        now = self.now_turns()
        turn = now[0] if now else None
        return {
            "id": self.id,
            "band": self.band,
            "family": self.family,
            "hz": self.center,
            **({"stations": self.station_objects()} if exact else {}),
            "state": self.state(),
            "talkers": [endpoint.get("callsign", row.callsign) for t in now
                        for row, endpoint in zip(t.talkers, t.endpoints)],
            "members": len([row for row in self.members() if row.counted]),
            "messages": self.message_count,
            "last": self.last,
            "folded": self.folded(),
            "start": turn.start if turn is not None else None,
            "heard": turn.heard() if turn is not None else 0,
            "readings": self.strip(turn) if turn is not None else [],
            "lastMessage": self.last_message_line(),
            "ended": self.last_turn_end,
        }

    def diff(self) -> Optional[RoomDiff]:
        """Compare the room with what its viewers last received and remember
        the new state. None for a candidate."""
        if self.closed_now:
            self.closed_now = False
            self.sent_members = {}
            self.sent_turns = {}
            self.sent_summary = None
            self.version += 1
            self.pending_messages.clear()
            if self.live:
                # Closed and opened again in one event: the opening is
                # announced next.
                self.book.touch(self)
            return RoomDiff([], [], [], True, True)
        if not self.live:
            self.pending_messages.clear()
            return None
        current: Dict[str, Tuple[Row, Dict[str, Any]]] = {}
        members: List[Tuple[Row, Dict[str, Any]]] = []
        for row in self.shown_rows():
            obj = self.member_object(row)
            current[row.ref] = (row, obj)
            sent = self.sent_members.get(row.ref)
            if sent is None or sent[1] != obj:
                members.append((row, obj))
        left = [row for ref, (row, _) in self.sent_members.items() if ref not in current]
        turns_now: Dict[int, Dict[str, Any]] = {}
        turns: List[Dict[str, Any]] = []
        turn_of: List[Turn] = []
        for turn in self.now_turns() + list(reversed(self.earlier_turns())):
            obj = self.turn_object(turn)
            turns_now[turn.id] = obj
            if self.sent_turns.get(turn.id) != obj:
                turns.append(obj)
                turn_of.append(turn)
        self.sent_members = current
        self.sent_turns = turns_now
        summary = self.summary()
        band = summary != self.sent_summary
        self.sent_summary = summary
        messages = list(self.pending_messages.values())
        self.pending_messages.clear()
        diff = RoomDiff(members, left, turns, False, band, turn_of, messages)
        if diff:
            self.version += 1
        return diff


class RoomBook:
    """Every room, live and candidate, and every station's placements."""

    def __init__(self, clock: Any, store: Any, random: Callable[[int], bytes] = os.urandom) -> None:
        self.clock = clock
        self.store = store
        self.random = random
        self.rooms: Dict[str, Room] = {}
        self.stations: Dict[str, _StationEntry] = {}
        self.earlier: Dict[str, List[Turn]] = {}
        # Stations a helper stopped counting (design 11.7, C21).
        self.not_counted: Set[str] = set()
        self.touched: "OrderedDict[int, Room]" = OrderedDict()
        self.band_version = 0
        self._tick: Any = None

    # ------------------------------------------------------------ queries

    def generation(self, room_id: str) -> int:
        """How many times the room's view has changed (the server-wide
        counter; each connection numbers its own generations, 6.11)."""
        room = self.rooms.get(room_id)
        return room.version if room is not None else 0

    def rooms_generation(self) -> int:
        """How many times the band list has changed, server-wide."""
        return self.band_version

    def live_rooms(self) -> List[Room]:
        return sorted((r for r in self.rooms.values() if r.live), key=lambda r: (r.center, r.id))

    def live(self, room_id: str) -> Optional[Room]:
        room = self.rooms.get(room_id)
        return room if room is not None and room.live else None

    def members(self, room_id: str) -> List[Row]:
        room = self.rooms.get(room_id)
        return room.members() if room is not None else []

    def rows_of(self, station_id: str) -> List[Row]:
        entry = self.stations.get(station_id)
        if entry is None:
            return []
        out: List[Row] = []
        for p in entry.placements.values():
            row = p.room.rows.get(station_id)
            if row is not None and row not in out:
                out.append(row)
        return out

    def counted_row(self, station_id: str, slice_id: int) -> Optional[Row]:
        """The row whose counted slice this is, or None."""
        entry = self.stations.get(station_id)
        p = entry.placements.get(slice_id) if entry is not None else None
        if p is None:
            return None
        row = p.room.rows.get(station_id)
        return row if row is not None and row.slice_id == slice_id else None

    def has_placements(self, station_id: str) -> bool:
        entry = self.stations.get(station_id)
        return entry is not None and bool(entry.placements)

    # ------------------------------------------------------------ changes

    def touch(self, room: Room) -> None:
        self.touched[id(room)] = room

    def take_touched(self) -> List[Room]:
        rooms = list(self.touched.values())
        self.touched.clear()
        return rooms

    def apply_presence(self, station_id: str, profile: Profile, slices: List[Dict[str, Any]]) -> None:
        """One presence, replacing the last whole (5.3.2, 6.3, 6.4): rows
        leave first, then slices are placed, then keying makes members and
        changes turns."""
        entry = self.stations.get(station_id)
        if entry is None:
            entry = _StationEntry(profile)
            self.stations[station_id] = entry
        entry.profile = profile
        for row in self.rows_of(station_id):
            row.owner = profile.owner
            row.callsign = profile.callsign
            row.grid = profile.grid
            row.away = profile.away
            row.software = profile.software
            row.software_version = profile.software_version
            self.touch(row.room)
        listed = {s["id"]: s for s in slices}
        for slice_id in list(entry.placements):
            p = entry.placements[slice_id]
            s = listed.get(slice_id)
            if s is None or FAMILIES[s["mode"]][0] != p.family or abs(s["hz"] - p.room.center) > p.room.width:
                self.leave(station_id, slice_id)
        entry.order = [s["id"] for s in slices]
        for s in slices:
            p = entry.placements.get(s["id"])
            if p is None:
                self.place(station_id, s)
            else:
                if p.hz != s["hz"] or p.keyed != s["keyed"]:
                    self.touch(p.room)
                p.hz = s["hz"]
                p.keyed = bool(s["keyed"])
        for s in slices:
            p = entry.placements[s["id"]]
            if s["keyed"] and not p.member:
                # Keyed there: a member at once (C2).
                p.member = True
                if p.timer is not None:
                    p.timer.cancel()
                    p.timer = None
        seen: List[Room] = []
        for slice_id in entry.order:
            room = entry.placements[slice_id].room
            if room not in seen:
                seen.append(room)
                self._settle(station_id, room)
        for row in self.rows_of(station_id):
            if row.member and row.counted:
                row.room.engine.on_presence(row, row.keyed)

    def place(self, station_id: str, slice: Dict[str, Any], profile: Optional[Profile] = None) -> str:
        """Place one slice (6.3): the nearest room of its family within the
        family's width, the lower center on a tie, else a new candidate on
        the slice's own frequency. Returns the room id."""
        entry = self.stations.get(station_id)
        if entry is None:
            entry = _StationEntry(profile)
            self.stations[station_id] = entry
        if profile is not None:
            entry.profile = profile
        family = FAMILIES[slice["mode"]][0]
        hz = int(slice["hz"])
        room = self._find(family, hz)
        if room is None:
            room = Room(self, family, hz)
            self.rooms[room.id] = room
            self.ensure_tick()
        slice_id = int(slice["id"])
        p = Placement(station_id, slice_id, hz, family, bool(slice["keyed"]), room, self.clock.now_ms())
        p.timer = self.clock.call_later(JOIN_DWELL_MS, lambda: self._dwell_done(p))
        room.placements[(station_id, slice_id)] = p
        entry.placements[slice_id] = p
        if slice_id not in entry.order:
            entry.order.append(slice_id)
        self._settle(station_id, room)
        return room.id

    def leave(self, station_id: str, slice_id: int) -> None:
        """One slice leaves its room."""
        entry = self.stations.get(station_id)
        p = entry.placements.pop(slice_id, None) if entry is not None else None
        if p is None:
            return
        if p.timer is not None:
            p.timer.cancel()
            p.timer = None
        room = p.room
        room.placements.pop((station_id, slice_id), None)
        self._settle(station_id, room)
        self._drop_if_empty(room)

    def remove_station(self, station_id: str) -> None:
        """Every row of the station leaves: its connection closed or was
        replaced, it was silent 30 s, or it went hidden (6.4)."""
        entry = self.stations.get(station_id)
        if entry is None:
            return
        for slice_id in list(entry.placements):
            self.leave(station_id, slice_id)
        self.stations.pop(station_id, None)

    def rename_station(self, old: str, new: str) -> None:
        """The station's history moved to a new station id (design 7.3,
        C6): what the rooms hold in memory under the old id, in open and
        earlier turns and the rows that posted messages, takes the new one,
        so a later write stores nothing under the old id. The station's
        own rows have already left with its connection."""
        if old in self.not_counted:
            self.not_counted.discard(old)
            self.not_counted.add(new)
        turns: List[Turn] = []
        for room in self.rooms.values():
            turns.extend(room.engine.open)
            for row in room.message_rows.values():
                if row.station_id == old:
                    row.station_id = new
        for earlier in self.earlier.values():
            turns.extend(earlier)
        for turn in turns:
            if old in turn.tracks:
                track = turn.tracks.pop(old)
                track.station_id = new
                turn.tracks[new] = track
            for row in turn.talkers:
                if row.station_id == old:
                    row.station_id = new
            if old in turn.given:
                turn.given[new] = turn.given.pop(old)
            if old in turn.members_during:
                turn.members_during.discard(old)
                turn.members_during.add(new)

    def set_counted(self, station_id: str, counted: bool) -> None:
        """A helper stopped counting the station, or it counts again
        (design 11.7, C21; wire spec 6.4 "Not counted"). While not counted
        its rows are in no view and the turn rules treat it as absent: its
        keyed state opens and holds no turn, and a turn it was talking in
        ends as when a talker leaves. Its readings are still stored with
        the turn open, marked. Counted again, its rows show and its keyed
        state counts from that moment."""
        if counted == (station_id not in self.not_counted):
            return
        if counted:
            self.not_counted.discard(station_id)
        else:
            self.not_counted.add(station_id)
        for row in self.rows_of(station_id):
            room = row.room
            if counted:
                row.counted = True
                if row.member and row.keyed:
                    room.engine.on_presence(row, True)
            else:
                if row.member:
                    room.engine.on_leave(row)
                row.counted = False
                row.reading = None
                row.reading_turn = None
                for turn in room.engine.open:
                    track = turn.tracks.get(station_id)
                    if track is not None:
                        track.not_counted = True
            self.touch(room)

    def reading(self, row: Row, dbm: float, t: int, present: bool) -> Optional[Turn]:
        """An accepted reading from a member's counted slice (6.7)."""
        room = row.room
        wall = self.clock.wall_ms()
        reading = Reading(wall, dbm, t, present)
        if not row.counted:
            # Stored with the turn open, marked, and shown nowhere; it
            # opens no turn and moves no turn rule (C21).
            turn = room.engine.turn_for_reading()
            if turn is not None:
                track = turn.add_reading(row, reading)
                track.not_counted = True
                track.offset_hz = offset_hz(row.hz, room.center)
                track.km, track.bearing_deg = room.path_of(row, turn)
            return turn
        turn = room.engine.on_reading(row, reading)
        row.reading = reading
        row.reading_turn = turn
        if turn is not None:
            track = turn.tracks[row.station_id]
            track.offset_hz = offset_hz(row.hz, room.center)
            track.km, track.bearing_deg = room.path_of(row, turn)
            # Preserve this actual measurement's public identity and path.
            # Later noise-floor samples/profile changes must not rewrite it.
            cutoff = (turn.unkey if turn.ending else
                      room.engine.last_present if turn.kind == "unknown" else None)
            track.remember_reporter(reading, room.member_object(row), cutoff)
        room.last = max(room.last, wall)
        self.touch(room)
        return turn

    # ------------------------------------------------------------ inside

    def _find(self, family: str, hz: int) -> Optional[Room]:
        best: Optional[Room] = None
        best_d = 0
        for room in self.rooms.values():
            if room.family != family:
                continue
            d = abs(hz - room.center)
            if d > room.width:
                continue
            if best is None or d < best_d or (d == best_d and room.center < best.center):
                best = room
                best_d = d
        return best

    def _settle(self, station_id: str, room: Room) -> None:
        """Make the station's row in `room` match its counted slice there."""
        entry = self.stations.get(station_id)
        counted: Optional[Placement] = None
        if entry is not None:
            for slice_id in entry.order:
                p = entry.placements.get(slice_id)
                if p is not None and p.room is room:
                    counted = p
                    break
        row = room.rows.get(station_id)
        if counted is None:
            if row is not None:
                self._row_leaves(row)
            return
        if row is None:
            assert entry is not None and entry.profile is not None
            row = self._row_enters(room, entry.profile, counted)
        else:
            if (row.slice_id, row.hz, row.keyed) != (counted.slice_id, counted.hz, counted.keyed):
                row.slice_id = counted.slice_id
                row.hz = counted.hz
                row.keyed = counted.keyed
                self.touch(room)
        if counted.member and not row.member:
            self._became_member(row)
        elif not counted.member and row.member:
            room.engine.on_leave(row)
            row.member = False
            self.touch(room)
            self._after_member_loss(room)

    def _new_ref(self, room: Room) -> str:
        while True:
            ref = base64.b32encode(self.random(REF_BYTES)).decode("ascii").lower()
            if ref not in room.refs:
                room.refs.add(ref)
                return ref

    def _row_enters(self, room: Room, profile: Profile, p: Placement) -> Row:
        taken = {r.nth for r in room.rows.values() if r.callsign == profile.callsign}
        nth = 1
        while nth in taken:
            nth += 1
        row = Row(room, p.station_id, profile, p, self._new_ref(room), nth)
        row.counted = p.station_id not in self.not_counted
        room.rows[p.station_id] = row
        if room.live:
            room.last = max(room.last, self.clock.wall_ms())
        self.touch(room)
        return row

    def _row_leaves(self, row: Row) -> None:
        room = row.room
        was_member = row.member
        if was_member:
            room.engine.on_leave(row)
        row.present = False
        room.rows.pop(row.station_id, None)
        if room.live:
            room.last = max(room.last, self.clock.wall_ms())
        self.touch(room)
        if was_member:
            self._after_member_loss(room)

    def _after_member_loss(self, room: Room) -> None:
        if not room.live or room.members():
            return
        # The last member left: open turns end now; close in 10 minutes.
        room.engine.end_all()
        if room.opening_id is not None:
            self.store.opening_touch(room.opening_id, self.clock.wall_ms())
        if room.close_timer is None:
            room.close_timer = self.clock.call_later(CLOSE_AFTER_MS, lambda: self._close(room))

    def _became_member(self, row: Row) -> None:
        room = row.room
        row.member = True
        if not room.live:
            self._open(room)
        if room.close_timer is not None:
            room.close_timer.cancel()
            room.close_timer = None
        room.last = max(room.last, self.clock.wall_ms())
        self.touch(room)
        for turn in room.engine.open:
            # A member at some time during the turn (6.9).
            if row.station_id not in turn.members_during:
                turn.members_during.add(row.station_id)
                self.store.turn_members(turn.id, turn.members_during)
        if row.keyed and row.counted:
            room.engine.on_presence(row, True)

    def _dwell_done(self, p: Placement) -> None:
        p.timer = None
        if p.room.placements.get((p.station_id, p.slice_id)) is not p:
            return
        p.member = True
        self._settle(p.station_id, p.room)

    def _open(self, room: Room) -> None:
        now = self.clock.wall_ms()
        room.live = True
        room.opened = now
        room.last = now
        room.last_turn_start = None
        room.last_turn_end = None
        room.message_count = 0
        room.message_rows = {}
        room.load_last_message()
        room.opening_id = self.store.opening_open(room.id, room.band, room.family, room.center, now)
        # Refs are new for every opening (4.4).
        room.refs = set()
        for row in room.rows.values():
            row.ref = self._new_ref(room)
        room.sent_members = {}
        room.sent_turns = {}
        room.sent_summary = None
        self.band_version += 1
        self.touch(room)
        log.info("room %s opened", room.id)

    def _close(self, room: Room) -> None:
        room.close_timer = None
        if not room.live or room.members():
            return
        room.engine.end_all()
        room.live = False
        if room.opening_id is not None:
            self.store.opening_close(room.opening_id, self.clock.wall_ms())
        room.closed_now = True
        self.band_version += 1
        self.touch(room)
        log.info("room %s closed", room.id)
        self._drop_if_empty(room)

    def _drop_if_empty(self, room: Room) -> None:
        if room.live or room.placements or room.rows:
            return
        if self.rooms.get(room.id) is room:
            del self.rooms[room.id]

    # ------------------------------------------------------------ storage

    def ensure_tick(self) -> None:
        if self._tick is None:
            self._tick = self.clock.call_later(FLUSH_MS, self._on_tick)

    def _on_tick(self) -> None:
        self._tick = None
        self.flush()
        self.prune_earlier()
        for room in self.rooms.values():
            if room.live:
                # Folding depends on time alone; the diff sends only change.
                self.touch(room)
        if self.rooms:
            self.ensure_tick()

    def prune_earlier(self) -> None:
        """Drop the earlier turns past 30 minutes (5.4.5) of every room id,
        closed ones too: only a viewed room prunes its own list, so a room
        that closed or moved would otherwise keep its turns for good."""
        cutoff = self.clock.wall_ms() - EARLIER_MS
        for room_id in list(self.earlier):
            kept = [t for t in self.earlier[room_id] if t.end is not None and t.end >= cutoff]
            if kept:
                self.earlier[room_id] = kept
            else:
                del self.earlier[room_id]

    def flush(self) -> None:
        """Write the open turns' tracks and the live rooms' last activity
        (every 30 s, 9.3)."""
        now = self.clock.wall_ms()
        for room in self.rooms.values():
            if not room.live:
                continue
            room.engine.tick()
            if room.opening_id is not None and room.members():
                self.store.opening_touch(room.opening_id, now)

    def reload(self) -> List[Room]:
        """Wire spec 6.3 "Restart": reload the rooms active in the last 10
        minutes as live rooms with no members; each closes 10 minutes after
        the restart unless a member rejoins."""
        now = self.clock.wall_ms()
        loaded: List[Room] = []
        for row in self.store.openings_reload(now, CLOSE_AFTER_MS):
            room = Room(self, row["family"], int(row["center_hz"]))
            room.live = True
            room.opening_id = int(row["opening_id"])
            room.opened = int(row["opened"])
            room.last = int(row["last_active"])
            room.message_count = self.store.message_count(room.opening_id)
            room.load_last_message()
            room.close_timer = self.clock.call_later(CLOSE_AFTER_MS, lambda r=room: self._close(r))
            self.rooms[room.id] = room
            loaded.append(room)
            self.band_version += 1
        if loaded:
            self.ensure_tick()
        return loaded
