# no-port-check: NereusSDR-original.
"""Turns, worked out per room on the server's own clock (wire spec 6.6,
6.7), and the reading tracks stored with them (6.7, 9.3).

The engine is told when a member's keyed state changes, when a member's
reading arrives and when a member's row leaves; it keeps the room's open
turns and tells its room when a turn opens, changes or ends. A reading's
own time is used only for its age; every rule here runs on arrival times.

Timers go through the clock, so "Moments" (6.6) holds: timers due at or
before a moment fire, in the order they fall due, before any message read
at that moment.
"""

from __future__ import annotations

import logging
import math
from collections import OrderedDict
from typing import Any, Dict, List, Optional, Set, Tuple

log = logging.getLogger("nereus_roundtable")

# Wire spec 9.2.
CONTINUE_MS = 3000  # same-talker continuation (C4)
QUIET_MS = 3000  # an unknown turn ends 3 s after the last present reading
LONG_KEY_MS = 600000  # a key held 10 unbroken minutes is treated as unkeyed (C21)

# Wire spec 6.7: dBm + 160 in 1 dB steps, so -160 to +94 dBm; 255 is "no
# reading that second".
DBM_OFFSET = 160
DBM_BYTE_MAX = 254
NO_READING = 255


def dbm_byte(dbm: float) -> int:
    """One second's byte: dBm + 160 rounded to the nearest whole dB (halves
    up), clamped to 0..254."""
    value = int(math.floor(float(dbm) + DBM_OFFSET + 0.5))
    return max(0, min(DBM_BYTE_MAX, value))


class Reading:
    """One accepted reading: `arrival` is the server's wall clock when it
    was read, `t` the time it carries (already replaced by its arrival when
    it was stamped too far off, 6.7)."""

    __slots__ = ("arrival", "dbm", "t", "present")

    def __init__(self, arrival: int, dbm: float, t: int, present: bool) -> None:
        self.arrival = arrival
        self.dbm = dbm
        self.t = t
        self.present = present


class Track:
    """One station's readings stored with one turn. Packed, it is one byte a
    second (dbm_byte, or NO_READING) and one signal-present bit a second,
    bit i of the turn's slot i in byte i // 8 at bit i % 8. Slot i is the
    second [start + 1000 i, start + 1000 (i + 1)) of the turn; when two
    readings fall in one slot the later one is kept. A track spans from its
    first reading's slot to its last's."""

    def __init__(self, station_id: str, callsign: str) -> None:
        self.station_id = station_id
        self.callsign = callsign
        self.readings: List[Reading] = []
        self.offset_hz: Optional[int] = None
        self.km: Optional[int] = None
        self.not_counted = False
        # The reader was AWAY at its latest reading (design 11.6).
        self.away = False

    def add(self, reading: Reading) -> None:
        self.readings.append(reading)

    def drop_after(self, end: int) -> List[Reading]:
        """Drop the readings that arrived after `end` (6.7); returns them."""
        kept = [r for r in self.readings if r.arrival <= end]
        dropped = [r for r in self.readings if r.arrival > end]
        self.readings = kept
        return dropped

    @property
    def count(self) -> int:
        return len(self.readings)

    @property
    def latest(self) -> Optional[Reading]:
        return self.readings[-1] if self.readings else None

    def pack(self, start: int) -> Optional[Tuple[int, int, bytes, bytes]]:
        """(first slot, last slot, dBm bytes, present bits) relative to the
        turn's start, or None with no readings."""
        if not self.readings:
            return None
        slots: Dict[int, Reading] = {}
        for r in self.readings:
            slots[max(0, (r.arrival - start) // 1000)] = r
        first = min(slots)
        last = max(slots)
        n = last - first + 1
        dbm = bytearray([NO_READING] * n)
        bits = bytearray((n + 7) // 8)
        for slot, r in slots.items():
            i = slot - first
            dbm[i] = dbm_byte(r.dbm)
            if r.present:
                bits[i // 8] |= 1 << (i % 8)
        return first, last, bytes(dbm), bytes(bits)


class Turn:
    """One turn. `talkers` holds the talkers' rows (the room's Row objects,
    which keep their callsign and nth after they leave); `end` is None while
    the turn is open. Times are the server's wall clock in Unix ms."""

    def __init__(self, kind: str, talkers: List[Any], start: int) -> None:
        self.id = 0
        self.kind = kind
        self.talkers = list(talkers)
        self.start = start
        self.end: Optional[int] = None
        self.ending = False
        self.unkey: Optional[int] = None
        self.timer: Any = None
        self.tracks: "OrderedDict[str, Track]" = OrderedDict()
        # Naming (wire spec 6.9): each naming station's name, the counts
        # the views show, and the stations that were members of the room
        # at some time during the turn (who may name it).
        self.given: Dict[str, str] = {}
        self.names: List[Dict[str, Any]] = []
        self.members_during: Set[str] = set()

    def set_name(self, station_id: str, name: str) -> bool:
        """A station's name for the turn, replacing its earlier one. True
        when the counts changed."""
        if self.given.get(station_id) == name:
            return False
        self.given[station_id] = name
        counts: Dict[str, int] = {}
        for given in self.given.values():
            counts[given] = counts.get(given, 0) + 1
        # Most first, then by name (6.9).
        self.names = [{"name": n, "count": c} for n, c in sorted(counts.items(), key=lambda nc: (-nc[1], nc[0]))]
        return True

    @property
    def talker(self) -> Any:
        return self.talkers[0] if self.talkers else None

    def add_reading(self, row: Any, reading: Reading) -> Track:
        track = self.tracks.get(row.station_id)
        if track is None:
            track = Track(row.station_id, row.callsign)
            self.tracks[row.station_id] = track
        track.callsign = row.callsign
        track.away = bool(getattr(row, "away", False))
        track.add(reading)
        return track

    def readings_by_station(self) -> Dict[str, int]:
        return {sid: tr.count for sid, tr in self.tracks.items() if tr.count}

    def heard(self) -> int:
        """Stations whose latest reading stored with the turn has present
        true; always 0 for a doubled turn (6.8)."""
        if self.kind == "doubled":
            return 0
        # A track a helper stopped counting is stored, never shown (C21).
        return sum(
            1 for tr in self.tracks.values() if not tr.not_counted and tr.latest is not None and tr.latest.present
        )

    def cancel_timer(self) -> None:
        if self.timer is not None:
            self.timer.cancel()
            self.timer = None


class TurnEngine:
    """The turn rules of one room (wire spec 6.6).

    `room` provides `digital` (bool) and the hooks turn_opened(turn),
    turn_changed(turn), turn_ended(turn, dropped) and turn_flush(turn), where
    dropped is a list of (row station id, Reading) dropped at the end.
    Members are the room's Row objects; the engine never sees a joining row.
    """

    def __init__(self, room: Any, clock: Any) -> None:
        self.room = room
        self.clock = clock
        self.digital = bool(getattr(room, "digital", False))
        # Effective keyed members in keying order, each with its long-key
        # timer (6.6 "Keyed members").
        self.keyed: "OrderedDict[Any, Any]" = OrderedDict()
        # Members whose key ran 10 minutes: unkeyed until a presence
        # reports them unkeyed.
        self.suppressed: set = set()
        self.open: List[Turn] = []
        self.last_present: Optional[int] = None  # L, wall ms
        self.quiet_timer: Any = None

    # ------------------------------------------------------------ inputs

    def on_presence(self, row: Any, keyed: bool) -> None:
        """A member's counted slice reported keyed or not."""
        if keyed:
            if row in self.suppressed or row in self.keyed:
                return
            self.keyed[row] = self.clock.call_later(LONG_KEY_MS, lambda: self._long_key(row))
            self._key(row)
            return
        self.suppressed.discard(row)
        handle = self.keyed.pop(row, None)
        if handle is None:
            return
        handle.cancel()
        self._unkey(row, leaving=False)

    def on_reading(self, row: Any, reading: Reading) -> Optional[Turn]:
        """A member's accepted reading. Returns the turn it is stored with,
        or None."""
        now = reading.arrival
        if reading.present:
            self.last_present = now
        if reading.present and not self.keyed and not self.open:
            # Unknown: no member keyed and no turn open (6.6).
            self._open("unknown", [], now)
        turn = self._turn_for_reading()
        if turn is not None:
            turn.add_reading(row, reading)
            self.room.turn_changed(turn)
        unknown = self._unknown()
        if unknown is not None and reading.present:
            self._arm_quiet()
        return turn

    def on_leave(self, row: Any) -> None:
        """A member's row leaves the room for any reason: it is unkeyed at
        that moment with no 3-second wait (6.6 "A talker leaving")."""
        self.suppressed.discard(row)
        handle = self.keyed.pop(row, None)
        if handle is not None:
            handle.cancel()
            self._unkey(row, leaving=True)
            return
        for turn in list(self.open):
            if turn.kind == "talking" and turn.talker is row and turn.ending:
                self._end(turn, turn.unkey)

    def end_all(self) -> None:
        """The room's last member left: its open turns end now, no wait."""
        now = self.clock.wall_ms()
        for turn in list(self.open):
            self._end(turn, turn.unkey if turn.ending and turn.unkey is not None else now)
        for handle in self.keyed.values():
            handle.cancel()
        self.keyed.clear()
        self.suppressed.clear()

    def tick(self) -> None:
        """Write the open turns' tracks (every 30 s, 9.3)."""
        for turn in self.open:
            self.room.turn_flush(turn)

    # ------------------------------------------------------------ state

    def is_keyed(self, row: Any) -> bool:
        return row in self.keyed

    def talking(self) -> List[Turn]:
        return [t for t in self.open if t.kind == "talking"]

    def _unknown(self) -> Optional[Turn]:
        for t in self.open:
            if t.kind == "unknown":
                return t
        return None

    def _talking_of(self, row: Any) -> Optional[Turn]:
        for t in self.open:
            if t.kind == "talking" and t.talker is row:
                return t
        return None

    def turn_for_reading(self) -> Optional[Turn]:
        """The turn a reading arriving now is stored with (6.7), without
        any turn rule running: for a station a helper stopped counting."""
        return self._turn_for_reading()

    def _turn_for_reading(self) -> Optional[Turn]:
        """Wire spec 6.7: in other families the one open turn; in digital
        ones the turn of the only keyed member, none with two or more keyed,
        and with none keyed the one open turn if there is exactly one."""
        if not self.digital:
            return self.open[0] if self.open else None
        keyed = list(self.keyed)
        if len(keyed) == 1:
            return self._talking_of(keyed[0])
        if len(keyed) >= 2:
            return None
        return self.open[0] if len(self.open) == 1 else None

    # ------------------------------------------------------------ rules

    def _now(self) -> int:
        return self.clock.wall_ms()

    def _open(self, kind: str, talkers: List[Any], start: int) -> Turn:
        turn = Turn(kind, talkers, start)
        self.open.append(turn)
        self.room.turn_opened(turn)
        return turn

    def _end(self, turn: Turn, end: int) -> None:
        if turn not in self.open:
            return
        turn.cancel_timer()
        turn.end = end
        turn.ending = False
        self.open.remove(turn)
        if turn.kind == "unknown":
            self._cancel_quiet()
        dropped: List[Tuple[str, Reading]] = []
        for sid, track in turn.tracks.items():
            for reading in track.drop_after(end):
                dropped.append((sid, reading))
        self.room.turn_ended(turn, dropped)

    def _key(self, row: Any) -> None:
        now = self._now()
        if self.digital:
            self._key_digital(row, now)
            return
        turn = self.open[0] if self.open else None
        if turn is not None and turn.kind == "unknown":
            # Talking: an open unknown turn ends at that moment.
            self._end(turn, now)
            turn = None
        if turn is not None and turn.kind == "talking":
            if turn.talker is row and turn.ending:
                # The same talker keyed again within 3 s: the turn goes on.
                turn.cancel_timer()
                turn.ending = False
                turn.unkey = None
                self.room.turn_changed(turn)
                return
            # Doubled: another member keys while one is keyed or ending; the
            # open talking turn ends at that moment, or at its unkey.
            self._end(turn, turn.unkey if turn.ending and turn.unkey is not None else now)
            turn = None
        if turn is not None and turn.kind == "doubled":
            if row not in turn.talkers:
                turn.talkers.append(row)
                self.room.turn_changed(turn)
            return
        keyed = list(self.keyed)
        if len(keyed) == 1:
            self._open("talking", keyed, now)
        elif len(keyed) >= 2:
            self._open("doubled", keyed, now)

    def _key_digital(self, row: Any, now: int) -> None:
        unknown = self._unknown()
        if unknown is not None:
            self._end(unknown, now)
        turn = self._talking_of(row)
        if turn is not None and turn.ending:
            turn.cancel_timer()
            turn.ending = False
            turn.unkey = None
            self.room.turn_changed(turn)
            return
        if turn is None:
            self._open("talking", [row], now)

    def _unkey(self, row: Any, leaving: bool) -> None:
        now = self._now()
        turn = self._talking_of(row)
        if turn is not None:
            if leaving:
                self._end(turn, turn.unkey if turn.ending and turn.unkey is not None else now)
                return
            if not turn.ending:
                turn.ending = True
                turn.unkey = now
                turn.timer = self.clock.call_later(CONTINUE_MS, lambda: self._ending_done(turn))
                self.room.turn_changed(turn)
            return
        if self.digital:
            return
        turn = self.open[0] if self.open else None
        if turn is not None and turn.kind == "doubled" and row in turn.talkers:
            remaining = list(self.keyed)
            if len(remaining) == 1:
                # One member left keyed: the double ends and a talking turn
                # for that member opens at the same moment (D10).
                self._end(turn, now)
                self._open("talking", remaining, now)
            elif not remaining:
                self._end(turn, now)

    def _ending_done(self, turn: Turn) -> None:
        turn.timer = None
        if turn.ending and turn.unkey is not None:
            self._end(turn, turn.unkey)

    def _long_key(self, row: Any) -> None:
        if row not in self.keyed:
            return
        log.info("a long key in %s: treated as unkeyed", getattr(self.room, "id", "a room"))
        del self.keyed[row]
        self.suppressed.add(row)
        self._unkey(row, leaving=False)

    def _arm_quiet(self) -> None:
        self._cancel_quiet()
        self.quiet_timer = self.clock.call_later(QUIET_MS, self._quiet)

    def _cancel_quiet(self) -> None:
        if self.quiet_timer is not None:
            self.quiet_timer.cancel()
            self.quiet_timer = None

    def _quiet(self) -> None:
        """Quiet: no member keyed and no present reading for 3 s; the
        unknown turn's end is L, the last present reading's arrival."""
        self.quiet_timer = None
        unknown = self._unknown()
        if unknown is None or self.keyed:
            return
        end = self.last_present if self.last_present is not None else self._now()
        self._end(unknown, max(unknown.start, end))
