# no-port-check: NereusSDR-original.
"""The history database: one SQLite file in WAL mode (design 6.3, wire spec
9.3).

Live rooms are held in memory; this file holds what lasts: stations, website
key hashes, room openings, turns and their reading tracks, names, messages,
reports and the review log, helpers, archive sessions, web sign-in sessions
and one-time links. No table has a column for an address, and nothing here
ever stores one (wire spec 9.3, 12): the key-issuance count per address
group lives in memory only. A website key is stored only as SHA-256 of its
32 raw bytes.

Every integer id is AUTOINCREMENT, so an id is never given twice, even
after Delete my archive removes the newest turns and sessions (wire spec
4.4). SQLite keeps the counters in its own sqlite_sequence table.

Archive sessions (design 4, C18) are built here as each talking turn ends:
a station's consecutive talking turns in one room id with gaps under 10
minutes are one session. Unknown and doubled turns never make one,
whatever their names.

The review list (design 11.7) groups the open reports of one item (a
message, a turn, or a station whatever room it was reported from) into one
entry, named by its first report's id (`reports.item`) and carrying a
version that every helper action and undo raises. A removed message's text
is kept in its review_actions row only while its Undo lasts (10 minutes),
then cleared.

The schema is created whole on first open; schema_version names it so a
later version can move an older file forward.
"""

from __future__ import annotations

import json
import sqlite3
from contextlib import contextmanager
from typing import Any, Dict, Iterable, Iterator, List, Optional, Set, Tuple

from . import messages

SCHEMA_VERSION = 1

# Design 4 and C18, wire spec 9.2: talking turns of one station in one room
# with gaps under 10 minutes are one archive session.
SESSION_GAP_MS = 600000

# Times are Unix milliseconds. A "forever" posting block or "not counted"
# mark is the *_forever flag; *_until holds a time when it ends.
SCHEMA = """
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS stations (
    id TEXT PRIMARY KEY,
    kind TEXT NOT NULL CHECK (kind IN ('core', 'key')),
    callsign TEXT,
    grid TEXT,
    first_seen INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    posting_block_until INTEGER,
    posting_block_forever INTEGER NOT NULL DEFAULT 0,
    not_counted_until INTEGER,
    not_counted_forever INTEGER NOT NULL DEFAULT 0,
    mentions_seen INTEGER
);
CREATE TABLE IF NOT EXISTS keys (
    key_hash BLOB PRIMARY KEY,
    station_id TEXT NOT NULL,
    created INTEGER NOT NULL,
    revoked INTEGER
);
CREATE INDEX IF NOT EXISTS keys_by_station ON keys (station_id);
CREATE TABLE IF NOT EXISTS rooms (
    opening_id INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id TEXT NOT NULL,
    band TEXT NOT NULL,
    family TEXT NOT NULL,
    center_hz INTEGER NOT NULL,
    opened INTEGER NOT NULL,
    last_active INTEGER NOT NULL,
    closed INTEGER
);
CREATE INDEX IF NOT EXISTS rooms_by_room_id ON rooms (room_id, opening_id);
CREATE TABLE IF NOT EXISTS turns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    opening_id INTEGER NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('talking', 'unknown', 'doubled')),
    talker_id TEXT,
    talker_callsign TEXT,
    second_talker_id TEXT,
    second_talker_callsign TEXT,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER,
    member_ids TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS turns_by_opening ON turns (opening_id, start_ms);
CREATE INDEX IF NOT EXISTS turns_by_talker ON turns (talker_id, start_ms);
CREATE TABLE IF NOT EXISTS turn_readings (
    turn_id INTEGER NOT NULL,
    station_id TEXT NOT NULL,
    callsign TEXT NOT NULL,
    offset_hz INTEGER,
    km INTEGER,
    first_second INTEGER NOT NULL,
    last_second INTEGER NOT NULL,
    dbm_track BLOB NOT NULL,
    present_track BLOB NOT NULL,
    not_counted INTEGER NOT NULL DEFAULT 0,
    away INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (turn_id, station_id)
);
CREATE INDEX IF NOT EXISTS turn_readings_by_station ON turn_readings (station_id);
CREATE TABLE IF NOT EXISTS turn_names (
    turn_id INTEGER NOT NULL,
    station_id TEXT NOT NULL,
    callsign TEXT,
    name TEXT NOT NULL,
    t INTEGER NOT NULL,
    PRIMARY KEY (turn_id, station_id)
);
CREATE INDEX IF NOT EXISTS turn_names_by_time ON turn_names (t);
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    opening_id INTEGER NOT NULL,
    room_id TEXT NOT NULL,
    station_id TEXT NOT NULL,
    callsign TEXT NOT NULL,
    nth INTEGER NOT NULL,
    t INTEGER NOT NULL,
    text TEXT,
    state TEXT NOT NULL CHECK (state IN ('shown', 'removed', 'deleted', 'expired')),
    dbm REAL,
    removed_by TEXT,
    mentions TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS messages_by_room ON messages (room_id, id);
CREATE INDEX IF NOT EXISTS messages_by_station ON messages (station_id);
CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target TEXT NOT NULL CHECK (target IN ('message', 'turn', 'station')),
    message_id INTEGER,
    turn_id INTEGER,
    room_id TEXT,
    row_ref TEXT,
    reported_station_id TEXT,
    reason TEXT,
    reporter_id TEXT NOT NULL,
    t INTEGER NOT NULL,
    closed INTEGER,
    closed_by TEXT,
    item INTEGER,
    version INTEGER NOT NULL DEFAULT 1,
    closed_action INTEGER
);
CREATE INDEX IF NOT EXISTS reports_by_reporter ON reports (reporter_id, target);
CREATE INDEX IF NOT EXISTS reports_by_item ON reports (item);
CREATE TABLE IF NOT EXISTS review_actions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    helper_id TEXT NOT NULL,
    action TEXT NOT NULL,
    target TEXT NOT NULL,
    detail TEXT,
    t INTEGER NOT NULL,
    undone INTEGER,
    report INTEGER,
    undo_text TEXT
);
CREATE INDEX IF NOT EXISTS review_actions_by_report ON review_actions (report, id);
CREATE TABLE IF NOT EXISTS helpers (
    station_id TEXT PRIMARY KEY,
    added INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id TEXT NOT NULL,
    room_id TEXT NOT NULL,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER NOT NULL,
    turn_ids TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS sessions_by_station ON sessions (station_id, start_ms);
CREATE INDEX IF NOT EXISTS sessions_by_room ON sessions (station_id, room_id, end_ms);
CREATE TABLE IF NOT EXISTS web_sessions (
    token_hash BLOB PRIMARY KEY,
    station_id TEXT NOT NULL,
    csrf_token TEXT NOT NULL,
    created INTEGER NOT NULL,
    expires INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS web_sessions_by_station ON web_sessions (station_id);
CREATE TABLE IF NOT EXISTS weblinks (
    token_hash BLOB PRIMARY KEY,
    station_id TEXT NOT NULL,
    created INTEGER NOT NULL,
    expires INTEGER NOT NULL,
    used INTEGER
);
"""

TABLES = (
    "stations",
    "keys",
    "rooms",
    "turns",
    "turn_readings",
    "turn_names",
    "messages",
    "reports",
    "review_actions",
    "helpers",
    "sessions",
    "web_sessions",
    "weblinks",
    "schema_version",
)


class StoreError(Exception):
    pass


class Store:
    def __init__(self, path: str) -> None:
        self.path = path
        # Autocommit; a change that touches several rows opens its own
        # transaction.
        self.db = sqlite3.connect(path, isolation_level=None)
        self.db.row_factory = sqlite3.Row
        self.journal_mode = str(self.db.execute("PRAGMA journal_mode=WAL").fetchone()[0]).lower()
        self.db.execute("PRAGMA synchronous=NORMAL")
        self._create()

    def _create(self) -> None:
        self.db.executescript(SCHEMA)
        row = self.db.execute("SELECT version FROM schema_version").fetchone()
        if row is None:
            self.db.execute("INSERT INTO schema_version (version) VALUES (?)", (SCHEMA_VERSION,))
        elif row["version"] != SCHEMA_VERSION:
            raise StoreError(f"the database is schema version {row['version']}, not {SCHEMA_VERSION}")

    def close(self) -> None:
        self.db.close()

    @contextmanager
    def transaction(self) -> Iterator[None]:
        """One transaction, or a part of the one already open."""
        if self.db.in_transaction:
            yield
            return
        self.db.execute("BEGIN")
        try:
            yield
        except BaseException:
            self.db.execute("ROLLBACK")
            raise
        self.db.execute("COMMIT")

    # ------------------------------------------------------------- stations

    def station_upsert(self, station_id: str, kind: str, callsign: Optional[str], grid: Optional[str], now: int) -> None:
        """Add a station, or mark it seen. A callsign or grid of None keeps
        the one stored: a Core signs in with none and sends its own in
        `presence`."""
        self.db.execute(
            """
            INSERT INTO stations (id, kind, callsign, grid, first_seen, last_seen)
            VALUES (?, ?, ?, ?, ?, ?)
            ON CONFLICT (id) DO UPDATE SET
                kind = excluded.kind,
                callsign = COALESCE(excluded.callsign, stations.callsign),
                grid = COALESCE(excluded.grid, stations.grid),
                last_seen = excluded.last_seen
            """,
            (station_id, kind, callsign, grid, now, now),
        )

    def station_get(self, station_id: str) -> Optional[Dict[str, Any]]:
        row = self.db.execute("SELECT * FROM stations WHERE id = ?", (station_id,)).fetchone()
        return dict(row) if row is not None else None

    # ------------------------------------------------------------- keys

    def key_add(self, key_hash: bytes, station_id: str, now: int) -> None:
        self.db.execute(
            "INSERT INTO keys (key_hash, station_id, created, revoked) VALUES (?, ?, ?, NULL)",
            (bytes(key_hash), station_id, now),
        )

    def key_lookup(self, key_hash: bytes) -> Optional[Dict[str, Any]]:
        """The key's row: station_id, created and revoked (None while it is
        good), or None for a key never issued."""
        row = self.db.execute(
            "SELECT station_id, created, revoked FROM keys WHERE key_hash = ?", (bytes(key_hash),)
        ).fetchone()
        return dict(row) if row is not None else None

    def key_revoke(self, key_hash: bytes, now: int) -> bool:
        """Revoke a key. False when there is no such key or it was already
        revoked."""
        cursor = self.db.execute(
            "UPDATE keys SET revoked = ? WHERE key_hash = ? AND revoked IS NULL", (now, bytes(key_hash))
        )
        return cursor.rowcount == 1

    # ------------------------------------------------------------- rooms

    def opening_open(self, room_id: str, band: str, family: str, center_hz: int, now: int) -> int:
        """A room opens: a new opening, whose id is returned."""
        cursor = self.db.execute(
            "INSERT INTO rooms (room_id, band, family, center_hz, opened, last_active, closed)"
            " VALUES (?, ?, ?, ?, ?, ?, NULL)",
            (room_id, band, family, int(center_hz), now, now),
        )
        return int(cursor.lastrowid)

    def opening_touch(self, opening_id: int, now: int) -> None:
        """The opening was active at `now`: the restart rule's clock."""
        self.db.execute(
            "UPDATE rooms SET last_active = MAX(last_active, ?) WHERE opening_id = ?", (now, opening_id)
        )

    def opening_close(self, opening_id: int, now: int) -> None:
        self.db.execute("UPDATE rooms SET closed = ? WHERE opening_id = ? AND closed IS NULL", (now, opening_id))

    def openings_reload(self, now: int, window_ms: int) -> List[Dict[str, Any]]:
        """Wire spec 6.3 "Restart": the openings still open and active in
        the last `window_ms`, in opening order. Older ones still open are
        closed at their last activity plus the window, and every turn left
        open in any opening ends at the opening's last activity (open turns
        end at a restart)."""
        with self.transaction():
            ended = [
                int(r[0])
                for r in self.db.execute("SELECT id FROM turns WHERE end_ms IS NULL AND kind = 'talking' ORDER BY id")
            ]
            self.db.execute(
                """
                UPDATE turns SET end_ms = MAX(start_ms,
                    (SELECT last_active FROM rooms WHERE rooms.opening_id = turns.opening_id))
                WHERE end_ms IS NULL
                """
            )
            self.db.execute(
                "UPDATE rooms SET closed = last_active + ? WHERE closed IS NULL AND last_active < ?",
                (window_ms, now - window_ms),
            )
            for turn_id in ended:
                self._session_add(turn_id)
            rows = self.db.execute(
                "SELECT * FROM rooms WHERE closed IS NULL ORDER BY opening_id"
            ).fetchall()
        return [dict(r) for r in rows]

    def opening_get(self, opening_id: int) -> Optional[Dict[str, Any]]:
        row = self.db.execute("SELECT * FROM rooms WHERE opening_id = ?", (opening_id,)).fetchone()
        return dict(row) if row is not None else None

    def opening_latest(self, room_id: str) -> Optional[Dict[str, Any]]:
        """The room id's most recent opening, or None for a room id that has
        never opened (wire spec 10.3)."""
        row = self.db.execute(
            "SELECT * FROM rooms WHERE room_id = ? ORDER BY opening_id DESC LIMIT 1", (room_id,)
        ).fetchone()
        return dict(row) if row is not None else None

    # ------------------------------------------------------------- turns

    def turn_open(self, opening_id: int, kind: str, talkers: List[Tuple[str, str]], start: int) -> int:
        """A turn opens; its id is returned. `talkers` is (station id,
        callsign) in keying order; the first two are stored."""
        first = talkers[0] if talkers else (None, None)
        second = talkers[1] if len(talkers) > 1 else (None, None)
        cursor = self.db.execute(
            "INSERT INTO turns (opening_id, kind, talker_id, talker_callsign, second_talker_id,"
            " second_talker_callsign, start_ms, end_ms) VALUES (?, ?, ?, ?, ?, ?, ?, NULL)",
            (opening_id, kind, first[0], first[1], second[0], second[1], start),
        )
        return int(cursor.lastrowid)

    def turn_talkers(self, turn_id: int, talkers: List[Tuple[str, str]]) -> None:
        first = talkers[0] if talkers else (None, None)
        second = talkers[1] if len(talkers) > 1 else (None, None)
        self.db.execute(
            "UPDATE turns SET talker_id = ?, talker_callsign = ?, second_talker_id = ?,"
            " second_talker_callsign = ? WHERE id = ?",
            (first[0], first[1], second[0], second[1], turn_id),
        )

    def turn_end(self, turn_id: int, end: int) -> None:
        """A turn ends; a talking turn joins its talker's archive session."""
        with self.transaction():
            self.db.execute("UPDATE turns SET end_ms = ? WHERE id = ?", (end, turn_id))
            self._session_add(turn_id)

    def turns_of(self, opening_id: int) -> List[Dict[str, Any]]:
        rows = self.db.execute(
            "SELECT * FROM turns WHERE opening_id = ? ORDER BY start_ms, id", (opening_id,)
        ).fetchall()
        return [dict(r) for r in rows]

    def tracks_write(self, turn_id: int, tracks: List[Tuple[Any, ...]]) -> None:
        """Write (or rewrite) one turn's tracks: each is (station id,
        callsign, offset Hz, km, first second, last second, dBm bytes,
        present bits, not counted) and, optionally, AWAY at the reader's
        latest reading (design 11.6)."""
        if not tracks:
            return
        with self.transaction():
            for entry in tracks:
                sid, callsign, offset, km, first, last, dbm, bits, not_counted = entry[:9]
                away = bool(entry[9]) if len(entry) > 9 else False
                self.db.execute(
                    "INSERT OR REPLACE INTO turn_readings (turn_id, station_id, callsign, offset_hz, km,"
                    " first_second, last_second, dbm_track, present_track, not_counted, away)"
                    " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (
                        turn_id,
                        sid,
                        callsign,
                        offset,
                        km,
                        first,
                        last,
                        bytes(dbm),
                        bytes(bits),
                        1 if not_counted else 0,
                        1 if away else 0,
                    ),
                )

    def tracks_of(self, turn_id: int) -> List[Dict[str, Any]]:
        rows = self.db.execute(
            "SELECT * FROM turn_readings WHERE turn_id = ? ORDER BY station_id", (turn_id,)
        ).fetchall()
        return [dict(r) for r in rows]

    def turn_get(self, turn_id: int) -> Optional[Dict[str, Any]]:
        """One turn with its opening's room id, or None."""
        row = self.db.execute(
            "SELECT turns.*, rooms.room_id AS room_id FROM turns"
            " JOIN rooms ON rooms.opening_id = turns.opening_id WHERE turns.id = ?",
            (turn_id,),
        ).fetchone()
        return dict(row) if row is not None else None

    def turn_members(self, turn_id: int, station_ids: Iterable[str]) -> None:
        """The stations that were members of the turn's room at some time
        during it (wire spec 6.9), kept for naming."""
        self.db.execute(
            "UPDATE turns SET member_ids = ? WHERE id = ?", (" ".join(sorted(station_ids)), turn_id)
        )

    @staticmethod
    def member_ids(turn: Dict[str, Any]) -> Set[str]:
        return set((turn.get("member_ids") or "").split())

    # ------------------------------------------------------------- names

    def turn_name(self, turn_id: int, station_id: str, name: str, callsign: Optional[str], t: int) -> None:
        """A station names a turn: one name per station, a later one
        replacing the earlier (wire spec 5.3.6). Nothing here touches
        sessions: a name never puts a turn in an archive (6.9)."""
        self.db.execute(
            "INSERT OR REPLACE INTO turn_names (turn_id, station_id, callsign, name, t) VALUES (?, ?, ?, ?, ?)",
            (turn_id, station_id, callsign, name, t),
        )

    def turn_names_of(self, turn_id: int) -> Dict[str, str]:
        """Each naming station's name for the turn."""
        rows = self.db.execute("SELECT station_id, name FROM turn_names WHERE turn_id = ?", (turn_id,)).fetchall()
        return {r["station_id"]: r["name"] for r in rows}

    def name_counts(self, turn_id: int) -> Dict[str, int]:
        """Each distinct name with how many stations gave it, most first,
        then by name (6.9)."""
        rows = self.db.execute(
            "SELECT name, COUNT(*) AS n FROM turn_names WHERE turn_id = ? GROUP BY name ORDER BY n DESC, name",
            (turn_id,),
        ).fetchall()
        return {r["name"]: int(r["n"]) for r in rows}

    def names_recent(self, room_id: str, since: int) -> List[str]:
        """The base callsigns of the names given at or after `since` to
        turns of any opening of the room id, newest first, each once (6.9:
        what a screen offers)."""
        rows = self.db.execute(
            "SELECT turn_names.name AS name, MAX(turn_names.t) AS last FROM turn_names"
            " JOIN turns ON turns.id = turn_names.turn_id"
            " JOIN rooms ON rooms.opening_id = turns.opening_id"
            " WHERE rooms.room_id = ? AND turn_names.t >= ? GROUP BY turn_names.name ORDER BY last DESC, name",
            (room_id, since),
        ).fetchall()
        out: List[str] = []
        for r in rows:
            base = messages.base_callsign(r["name"])
            if base not in out:
                out.append(base)
        return out

    # ------------------------------------------------------------- messages

    def message_add(
        self,
        opening_id: int,
        room_id: str,
        station_id: str,
        callsign: str,
        nth: int,
        t: int,
        text: str,
        dbm: Optional[float],
        mentions: Iterable[str] = (),
    ) -> int:
        """Store a posted message with the sender's callsign and row number
        at the time and its reading of the talker (or None); returns its
        id. `mentions` are the base callsigns of its words, kept so a
        station's next `welcome` can list the messages naming it."""
        words = list(mentions)
        cursor = self.db.execute(
            "INSERT INTO messages (opening_id, room_id, station_id, callsign, nth, t, text, state, dbm,"
            " removed_by, mentions) VALUES (?, ?, ?, ?, ?, ?, ?, 'shown', ?, NULL, ?)",
            (
                opening_id,
                room_id,
                station_id,
                callsign,
                nth,
                t,
                text,
                dbm,
                (" " + " ".join(words) + " ") if words else "",
            ),
        )
        return int(cursor.lastrowid)

    def message_get(self, message_id: int) -> Optional[Dict[str, Any]]:
        row = self.db.execute("SELECT * FROM messages WHERE id = ?", (message_id,)).fetchone()
        return dict(row) if row is not None else None

    def message_delete(self, message_id: int, by_station: str) -> str:
        """The sender deletes its own message (wire spec 5.3.5): the text
        goes, `state` becomes `deleted`, the row stays. Returns `deleted`,
        `unchanged` (already without its text), `not-found` or
        `not-allowed` (another station's message)."""
        row = self.message_get(message_id)
        if row is None:
            return "not-found"
        if row["station_id"] != by_station:
            return "not-allowed"
        if row["state"] != "shown":
            return "unchanged"
        self.db.execute("UPDATE messages SET text = NULL, state = 'deleted' WHERE id = ?", (message_id,))
        return "deleted"

    def messages_of_room(self, room_id: str, limit: int = 50, before: Optional[int] = None) -> List[Dict[str, Any]]:
        """The newest `limit` messages of every opening of the room id, with
        ids under `before` when given, oldest first."""
        if before is None:
            rows = self.db.execute(
                "SELECT * FROM messages WHERE room_id = ? ORDER BY id DESC LIMIT ?", (room_id, limit)
            ).fetchall()
        else:
            rows = self.db.execute(
                "SELECT * FROM messages WHERE room_id = ? AND id < ? ORDER BY id DESC LIMIT ?",
                (room_id, before, limit),
            ).fetchall()
        return [dict(r) for r in reversed(rows)]

    def message_count(self, opening_id: int) -> int:
        row = self.db.execute("SELECT COUNT(*) FROM messages WHERE opening_id = ?", (opening_id,)).fetchone()
        return int(row[0])

    # ------------------------------------------------------------- mentions

    def mention_seen(self, station_id: str, up_to_ms: int) -> None:
        """Every mention of the station in a message posted at or before
        `up_to_ms` is seen (wire spec 5.3.11). The mark never moves back."""
        self.db.execute(
            "UPDATE stations SET mentions_seen = MAX(COALESCE(mentions_seen, -1), ?) WHERE id = ?",
            (up_to_ms, station_id),
        )

    def unread_mentions(self, station_id: str, limit: int = 50) -> List[Dict[str, Any]]:
        """The shown messages of other stations naming the station's current
        base callsign, posted after its last `mentionsSeen`: at most the
        newest `limit`, oldest first (wire spec 5.4.2)."""
        station = self.station_get(station_id)
        if station is None or not station["callsign"]:
            return []
        base = messages.base_callsign(station["callsign"])
        seen = station["mentions_seen"]
        rows = self.db.execute(
            "SELECT * FROM messages WHERE state = 'shown' AND station_id != ? AND t > ?"
            " AND instr(mentions, ?) > 0 ORDER BY id DESC LIMIT ?",
            (station_id, -1 if seen is None else seen, f" {base} ", limit),
        ).fetchall()
        return [dict(r) for r in reversed(rows)]

    # ------------------------------------------------------------- reports

    def report_add(
        self,
        kind: str,
        target: Any,
        reason: Optional[str],
        by: str,
        now: int,
        station: Optional[str] = None,
    ) -> bool:
        """Put an item on the review list (wire spec 5.3.7). `target` is a
        message id, a turn id, or (room id, row ref) for a station, whose
        station id is `station`. A second open report of one item by one
        station counts once: False, and nothing is added."""
        message_id = target if kind == "message" else None
        turn_id = target if kind == "turn" else None
        room_id, ref = target if kind == "station" else (None, None)
        if kind == "station":
            match = "room_id = ? AND row_ref = ?"
            args: Tuple[Any, ...] = (room_id, ref)
        elif kind == "message":
            match = "message_id = ?"
            args = (message_id,)
        else:
            match = "turn_id = ?"
            args = (turn_id,)
        found = self.db.execute(
            f"SELECT 1 FROM reports WHERE reporter_id = ? AND target = ? AND closed IS NULL AND {match}",
            (by, kind) + args,
        ).fetchone()
        if found is not None:
            return False
        # Reports of one item while it is open are one entry on the review
        # list (design 11.7): a message or a turn by its id, a station by
        # its station id whatever room it was reported from.
        if kind == "station":
            item_match = "reported_station_id = ?"
            item_args: Tuple[Any, ...] = (station,)
        else:
            item_match, item_args = match, args
        with self.transaction():
            open_item = self.db.execute(
                f"SELECT item, version FROM reports WHERE target = ? AND closed IS NULL AND {item_match}"
                " ORDER BY id LIMIT 1",
                (kind,) + item_args,
            ).fetchone()
            cursor = self.db.execute(
                "INSERT INTO reports (target, message_id, turn_id, room_id, row_ref, reported_station_id, reason,"
                " reporter_id, t, closed, closed_by, item, version) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, NULL, ?, ?)",
                (
                    kind,
                    message_id,
                    turn_id,
                    room_id,
                    ref,
                    station,
                    reason,
                    by,
                    now,
                    open_item["item"] if open_item is not None else None,
                    open_item["version"] if open_item is not None else 1,
                ),
            )
            if open_item is None:
                self.db.execute("UPDATE reports SET item = id WHERE id = ?", (cursor.lastrowid,))
        return True

    # ------------------------------------------------------------- review

    def review_item(self, item_id: int) -> Optional[Dict[str, Any]]:
        """One entry of the review list: every report of one item, oldest
        first, with the entry's version and whether it is closed."""
        rows = [dict(r) for r in self.db.execute("SELECT * FROM reports WHERE item = ? ORDER BY id", (item_id,))]
        if not rows:
            return None
        first = rows[0]
        return {
            "id": int(item_id),
            "target": first["target"],
            "message_id": first["message_id"],
            "turn_id": first["turn_id"],
            "room_id": first["room_id"],
            "row_ref": first["row_ref"],
            "station": first["reported_station_id"],
            "version": int(first["version"]),
            "closed": first["closed"],
            "closed_by": first["closed_by"],
            "closed_action": first["closed_action"],
            "first": int(first["t"]),
            "reports": rows,
        }

    def review_open(self) -> List[Dict[str, Any]]:
        """The open entries of the review list, oldest first (design 11.7)."""
        ids = [
            int(r[0])
            for r in self.db.execute(
                "SELECT item, MIN(t) AS first FROM reports WHERE closed IS NULL GROUP BY item ORDER BY first, item"
            )
        ]
        out = []
        for item_id in ids:
            item = self.review_item(item_id)
            if item is not None:
                out.append(item)
        return out

    def review_bump(self, item_id: int, version: int) -> None:
        self.db.execute("UPDATE reports SET version = ? WHERE item = ?", (version, item_id))

    def review_close(self, item_id: int, by: str, action_id: int, now: int) -> bool:
        """Close the entry's open reports; True when any was open."""
        cursor = self.db.execute(
            "UPDATE reports SET closed = ?, closed_by = ?, closed_action = ? WHERE item = ? AND closed IS NULL",
            (now, by, action_id, item_id),
        )
        return cursor.rowcount > 0

    def review_reopen(self, item_id: int, action_id: int) -> None:
        """Undo: the reports the action closed are open again."""
        self.db.execute(
            "UPDATE reports SET closed = NULL, closed_by = NULL, closed_action = NULL WHERE item = ? AND closed_action = ?",
            (item_id, action_id),
        )

    def action_add(
        self,
        helper_id: str,
        action: str,
        target: str,
        detail: Dict[str, Any],
        t: int,
        report: Optional[int] = None,
        undo_text: Optional[str] = None,
    ) -> int:
        """One line of the helper log (design 11.7); its id."""
        cursor = self.db.execute(
            "INSERT INTO review_actions (helper_id, action, target, detail, t, undone, report, undo_text)"
            " VALUES (?, ?, ?, ?, ?, NULL, ?, ?)",
            (helper_id, action, target, json.dumps(detail, sort_keys=True), t, report, undo_text),
        )
        return int(cursor.lastrowid)

    def action_get(self, action_id: int) -> Optional[Dict[str, Any]]:
        row = self.db.execute("SELECT * FROM review_actions WHERE id = ?", (action_id,)).fetchone()
        return dict(row) if row is not None else None

    def action_latest(self, item_id: int) -> Optional[Dict[str, Any]]:
        """The newest action taken on a review entry, an undo included."""
        row = self.db.execute(
            "SELECT * FROM review_actions WHERE report = ? ORDER BY id DESC LIMIT 1", (item_id,)
        ).fetchone()
        return dict(row) if row is not None else None

    def action_undone(self, action_id: int, now: int) -> None:
        self.db.execute(
            "UPDATE review_actions SET undone = ?, undo_text = NULL WHERE id = ?", (now, action_id)
        )

    def actions_log(self, limit: int = 200) -> List[Dict[str, Any]]:
        """The helper log, newest first: every helper action and every
        helper change. A history move by its own key is not a helper's."""
        rows = self.db.execute(
            "SELECT * FROM review_actions WHERE action != 'move-history' ORDER BY id DESC LIMIT ?", (limit,)
        ).fetchall()
        return [dict(r) for r in rows]

    def undo_purge(self, before: int) -> int:
        """A removed message's text is kept only while its Undo lasts:
        clear what was kept for actions taken before `before`."""
        cursor = self.db.execute(
            "UPDATE review_actions SET undo_text = NULL WHERE undo_text IS NOT NULL AND t < ?", (before,)
        )
        return cursor.rowcount

    def message_remove(self, message_id: int, helper_id: str) -> Optional[str]:
        """A helper removes a shown message (design 11.7): its text goes,
        `state` becomes `removed`. Returns the text it had, or None when it
        had none to remove."""
        row = self.message_get(message_id)
        if row is None or row["state"] != "shown" or row["text"] is None:
            return None
        self.db.execute(
            "UPDATE messages SET text = NULL, state = 'removed', removed_by = ? WHERE id = ?", (helper_id, message_id)
        )
        return str(row["text"])

    def message_restore(self, message_id: int, text: str) -> bool:
        """Undo of a removal: the text is shown again."""
        cursor = self.db.execute(
            "UPDATE messages SET text = ?, state = 'shown', removed_by = NULL WHERE id = ? AND state = 'removed'",
            (text, message_id),
        )
        return cursor.rowcount == 1

    # ------------------------------------------------------------- helpers

    def helpers_list(self) -> List[Dict[str, Any]]:
        return [dict(r) for r in self.db.execute("SELECT * FROM helpers ORDER BY added, station_id")]

    def helper_is(self, station_id: str) -> bool:
        return self.db.execute("SELECT 1 FROM helpers WHERE station_id = ?", (station_id,)).fetchone() is not None

    def helper_add(self, station_id: str, now: int) -> bool:
        cursor = self.db.execute("INSERT OR IGNORE INTO helpers (station_id, added) VALUES (?, ?)", (station_id, now))
        return cursor.rowcount == 1

    def helper_remove(self, station_id: str) -> bool:
        return self.db.execute("DELETE FROM helpers WHERE station_id = ?", (station_id,)).rowcount == 1

    def stations_by_callsign(self, base: str, limit: int = 50) -> List[Dict[str, Any]]:
        """The stations whose stored callsign has this base callsign (wire
        spec 4.5), most recently seen first."""
        rows = self.db.execute(
            "SELECT * FROM stations WHERE callsign IS NOT NULL AND instr(upper(callsign), ?) > 0"
            " ORDER BY last_seen DESC, id",
            (base.upper(),),
        ).fetchall()
        out = [dict(r) for r in rows if messages.base_callsign(r["callsign"]) == base]
        return out[:limit]

    # ------------------------------------------------------------- retention

    def messages_expire(self, before: int) -> List[Tuple[int, str]]:
        """Design 6.4: every shown message posted before `before` loses its
        text (`state` `expired`); the row, its sender, time and reading
        stay. Returns (id, room id) of each."""
        with self.transaction():
            rows = [
                (int(r["id"]), str(r["room_id"]))
                for r in self.db.execute("SELECT id, room_id FROM messages WHERE state = 'shown' AND t < ?", (before,))
            ]
            self.db.execute(
                "UPDATE messages SET text = NULL, mentions = '', state = 'expired' WHERE state = 'shown' AND t < ?",
                (before,),
            )
        return rows

    def row_counts(self) -> Dict[str, int]:
        """Every table's row count, for the snapshot check."""
        return {name: int(self.db.execute(f"SELECT COUNT(*) FROM {name}").fetchone()[0]) for name in TABLES}

    # ------------------------------------------------------------- blocks

    def station_blocked(self, station_id: str, now: int) -> bool:
        """Whether a helper has blocked the station from posting and naming
        turns (design 11.7) at `now`."""
        row = self.station_get(station_id)
        if row is None:
            return False
        until = row["posting_block_until"]
        return bool(row["posting_block_forever"]) or (until is not None and until > now)

    def station_not_counted(self, station_id: str, now: int) -> bool:
        """Whether a helper has stopped counting the station's readings and
        keyed state (design 11.7, C21) at `now`."""
        row = self.station_get(station_id)
        if row is None:
            return False
        until = row["not_counted_until"]
        return bool(row["not_counted_forever"]) or (until is not None and until > now)

    def station_block_set(self, station_id: str, until: Optional[int], forever: bool) -> None:
        """Block the station from posting and naming turns until a time,
        or for good; (None, False) lifts it."""
        self.db.execute(
            "UPDATE stations SET posting_block_until = ?, posting_block_forever = ? WHERE id = ?",
            (until, 1 if forever else 0, station_id),
        )

    def station_not_counted_set(self, station_id: str, until: Optional[int], forever: bool) -> None:
        """Stop counting the station until a time, or for good; (None,
        False) counts it again."""
        self.db.execute(
            "UPDATE stations SET not_counted_until = ?, not_counted_forever = ? WHERE id = ?",
            (until, 1 if forever else 0, station_id),
        )

    # ------------------------------------------------------------- profile

    def station_profile(self, station_id: str, callsign: str, grid: Optional[str]) -> bool:
        """A key station's callsign and grid, set on the key page (design
        7.3). False when there is no such station."""
        cursor = self.db.execute("UPDATE stations SET callsign = ?, grid = ? WHERE id = ?", (callsign, grid, station_id))
        return cursor.rowcount == 1

    # ------------------------------------------------------------- web

    def web_session_add(self, token_hash: bytes, station_id: str, csrf: str, now: int, expires: int) -> None:
        """A browser session (design 7.4): only the SHA-256 of its cookie is
        kept. Sessions past their time are dropped here."""
        with self.transaction():
            self.db.execute("DELETE FROM web_sessions WHERE expires <= ?", (now,))
            self.db.execute(
                "INSERT INTO web_sessions (token_hash, station_id, csrf_token, created, expires) VALUES (?, ?, ?, ?, ?)",
                (bytes(token_hash), station_id, csrf, now, expires),
            )

    def web_session_get(self, token_hash: bytes, now: int) -> Optional[Dict[str, Any]]:
        """The session a cookie names, while it lasts, or None."""
        row = self.db.execute(
            "SELECT station_id, csrf_token, created, expires FROM web_sessions WHERE token_hash = ? AND expires > ?",
            (bytes(token_hash), now),
        ).fetchone()
        return dict(row) if row is not None else None

    def web_sessions_end(self, station_id: str) -> int:
        """Sign out everywhere: every browser session of the station ends."""
        return self.db.execute("DELETE FROM web_sessions WHERE station_id = ?", (station_id,)).rowcount

    def weblink_add(self, token_hash: bytes, station_id: str, now: int, expires: int) -> None:
        """A one-time sign-in link (wire spec 5.4.10): only the SHA-256 of
        its token is kept. Used and expired links are dropped here."""
        with self.transaction():
            self.db.execute("DELETE FROM weblinks WHERE used IS NOT NULL OR expires <= ?", (now,))
            self.db.execute(
                "INSERT INTO weblinks (token_hash, station_id, created, expires, used) VALUES (?, ?, ?, ?, NULL)",
                (bytes(token_hash), station_id, now, expires),
            )

    def weblink_use(self, token_hash: bytes, now: int) -> Optional[str]:
        """Use a sign-in link: its station id the first time, before it
        expires; None for a link used before, expired, or never made."""
        with self.transaction():
            row = self.db.execute(
                "SELECT station_id FROM weblinks WHERE token_hash = ? AND used IS NULL AND expires > ?",
                (bytes(token_hash), now),
            ).fetchone()
            if row is None:
                return None
            self.db.execute("UPDATE weblinks SET used = ? WHERE token_hash = ?", (now, bytes(token_hash)))
        return str(row["station_id"])

    # ------------------------------------------------------------- sessions

    def _session_add(self, turn_id: int) -> None:
        """An ended talking turn joins its talker's latest session in the
        same room id when it starts less than 10 minutes after that session
        ended, and otherwise starts a new session (design 4, C18)."""
        turn = self.turn_get(turn_id)
        if turn is None or turn["kind"] != "talking" or not turn["talker_id"] or turn["end_ms"] is None:
            return
        sid = turn["talker_id"]
        token = f"% {int(turn_id)} %"
        if self.db.execute(
            "SELECT 1 FROM sessions WHERE station_id = ? AND (' ' || turn_ids || ' ') LIKE ?", (sid, token)
        ).fetchone():
            return
        start = int(turn["start_ms"])
        end = int(turn["end_ms"])
        last = self.db.execute(
            "SELECT * FROM sessions WHERE station_id = ? AND room_id = ? ORDER BY end_ms DESC, id DESC LIMIT 1",
            (sid, turn["room_id"]),
        ).fetchone()
        if last is not None and start - int(last["end_ms"]) < SESSION_GAP_MS:
            self.db.execute(
                "UPDATE sessions SET start_ms = ?, end_ms = ?, turn_ids = ? WHERE id = ?",
                (min(start, int(last["start_ms"])), max(end, int(last["end_ms"])), f"{last['turn_ids']} {turn_id}", last["id"]),
            )
            return
        self.db.execute(
            "INSERT INTO sessions (station_id, room_id, start_ms, end_ms, turn_ids) VALUES (?, ?, ?, ?, ?)",
            (sid, turn["room_id"], start, end, str(turn_id)),
        )

    @staticmethod
    def session_turn_ids(session: Dict[str, Any]) -> List[int]:
        return [int(t) for t in (session.get("turn_ids") or "").split()]

    def sessions_of(self, station_id: str) -> List[Dict[str, Any]]:
        """The station's archive sessions, newest first."""
        rows = self.db.execute(
            "SELECT * FROM sessions WHERE station_id = ? ORDER BY start_ms DESC, id DESC", (station_id,)
        ).fetchall()
        return [dict(r) for r in rows]

    def session_get(self, session_id: int) -> Optional[Dict[str, Any]]:
        row = self.db.execute("SELECT * FROM sessions WHERE id = ?", (session_id,)).fetchone()
        return dict(row) if row is not None else None

    def session_turns(self, session: Dict[str, Any]) -> List[Dict[str, Any]]:
        """The session's turns that are still stored, by start."""
        ids = self.session_turn_ids(session)
        if not ids:
            return []
        marks = ",".join("?" for _ in ids)
        rows = self.db.execute(
            f"SELECT * FROM turns WHERE id IN ({marks}) ORDER BY start_ms, id", tuple(ids)
        ).fetchall()
        return [dict(r) for r in rows]

    def readings_of_turns(self, turn_ids: List[int], not_by: str) -> List[Dict[str, Any]]:
        """Every counted reading stored with these turns by any station but
        `not_by`, by turn then station."""
        if not turn_ids:
            return []
        marks = ",".join("?" for _ in turn_ids)
        rows = self.db.execute(
            f"SELECT * FROM turn_readings WHERE turn_id IN ({marks}) AND station_id != ? AND not_counted = 0"
            " ORDER BY turn_id, station_id",
            tuple(turn_ids) + (not_by,),
        ).fetchall()
        return [dict(r) for r in rows]

    def messages_between(self, room_id: str, start: int, end: int) -> List[Dict[str, Any]]:
        """The room id's messages posted from `start` to `end`, oldest first."""
        rows = self.db.execute(
            "SELECT * FROM messages WHERE room_id = ? AND t >= ? AND t <= ? ORDER BY id", (room_id, start, end)
        ).fetchall()
        return [dict(r) for r in rows]

    # ------------------------------------------------------------- history

    def move_history(self, from_id: str, to_id: str, by: str, now: int = 0) -> None:
        """Move a station's whole history to another station id in one
        transaction (design 7.3, C6, D26): its station row, sessions,
        turns, readings, names, messages and reports. The move is logged in
        review_actions. Nothing else is left under the old id but its
        revoked key, which still refuses a sign-in with 4004 (wire spec
        4.2). Used only by "Move my history to a new key", signed in by the
        old key."""
        with self.transaction():
            old = self.station_get(from_id)
            if old is not None:
                self.db.execute(
                    """
                    INSERT INTO stations (id, kind, callsign, grid, first_seen, last_seen, posting_block_until,
                        posting_block_forever, not_counted_until, not_counted_forever, mentions_seen)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ON CONFLICT (id) DO UPDATE SET
                        callsign = COALESCE(stations.callsign, excluded.callsign),
                        grid = COALESCE(stations.grid, excluded.grid),
                        first_seen = MIN(stations.first_seen, excluded.first_seen),
                        last_seen = MAX(stations.last_seen, excluded.last_seen),
                        posting_block_until = MAX(COALESCE(stations.posting_block_until, 0),
                            COALESCE(excluded.posting_block_until, 0)),
                        posting_block_forever = MAX(stations.posting_block_forever, excluded.posting_block_forever),
                        not_counted_until = MAX(COALESCE(stations.not_counted_until, 0),
                            COALESCE(excluded.not_counted_until, 0)),
                        not_counted_forever = MAX(stations.not_counted_forever, excluded.not_counted_forever),
                        mentions_seen = MAX(COALESCE(stations.mentions_seen, -1), COALESCE(excluded.mentions_seen, -1))
                    """,
                    (
                        to_id,
                        old["kind"],
                        old["callsign"],
                        old["grid"],
                        old["first_seen"],
                        old["last_seen"],
                        old["posting_block_until"],
                        old["posting_block_forever"],
                        old["not_counted_until"],
                        old["not_counted_forever"],
                        old["mentions_seen"],
                    ),
                )
                self.db.execute("DELETE FROM stations WHERE id = ?", (from_id,))
            moves = (
                ("sessions", "station_id"),
                ("turns", "talker_id"),
                ("turns", "second_talker_id"),
                ("turn_readings", "station_id"),
                ("turn_names", "station_id"),
                ("messages", "station_id"),
                ("messages", "removed_by"),
                ("reports", "reporter_id"),
                ("reports", "reported_station_id"),
                ("reports", "closed_by"),
            )
            for table, column in moves:
                self.db.execute(f"UPDATE {table} SET {column} = ? WHERE {column} = ?", (to_id, from_id))
            for row in self.db.execute(
                "SELECT id, member_ids FROM turns WHERE (' ' || member_ids || ' ') LIKE ?", (f"% {from_id} %",)
            ).fetchall():
                ids = self.member_ids(dict(row))
                ids.discard(from_id)
                ids.add(to_id)
                self.db.execute("UPDATE turns SET member_ids = ? WHERE id = ?", (" ".join(sorted(ids)), row["id"]))
            self.db.execute("DELETE FROM web_sessions WHERE station_id = ?", (from_id,))
            self.db.execute("DELETE FROM weblinks WHERE station_id = ?", (from_id,))
            self.db.execute(
                "INSERT INTO review_actions (helper_id, action, target, detail, t, undone) VALUES (?, 'move-history', ?, ?, ?, NULL)",
                (by, from_id, to_id, now),
            )

    def key_move(self, old_hash: bytes, new_hash: bytes, new_id: str, now: int) -> Optional[str]:
        """"Move my history to a new key", in one transaction: the new key
        is issued, the old one revoked, and the old station's history moved
        to the new key's station id. Returns the old station id, or None
        (and nothing changes) when the old key is unknown or revoked."""
        with self.transaction():
            row = self.key_lookup(old_hash)
            if row is None or row["revoked"] is not None:
                return None
            old_id = str(row["station_id"])
            self.key_add(new_hash, new_id, now)
            self.key_revoke(old_hash, now)
            self.move_history(old_id, new_id, old_id, now)
        return old_id

    # ------------------------------------------------------------- delete

    def delete_archive(self, station_id: str) -> List[Tuple[int, str]]:
        """Delete my archive (design 13, C9), in one transaction: the
        station's sessions, its ended talking turns and every reading and
        name stored with them, and the text of its messages, whose rows
        stay with state `deleted`. Its readings of other talkers stay,
        under the callsign stored with them. A talking turn still open is
        left to end and be stored as new. Returns (id, room id) of each
        message whose text was cleared."""
        with self.transaction():
            turn_ids = [
                int(r[0])
                for r in self.db.execute(
                    "SELECT id FROM turns WHERE kind = 'talking' AND talker_id = ? AND end_ms IS NOT NULL",
                    (station_id,),
                )
            ]
            for i in range(0, len(turn_ids), 500):
                chunk = turn_ids[i : i + 500]
                marks = ",".join("?" for _ in chunk)
                for table, column in (("turn_readings", "turn_id"), ("turn_names", "turn_id"), ("turns", "id")):
                    self.db.execute(f"DELETE FROM {table} WHERE {column} IN ({marks})", tuple(chunk))
            self.db.execute("DELETE FROM sessions WHERE station_id = ?", (station_id,))
            cleared = [
                (int(r["id"]), str(r["room_id"]))
                for r in self.db.execute(
                    "SELECT id, room_id FROM messages WHERE station_id = ? AND text IS NOT NULL", (station_id,)
                )
            ]
            self.db.execute(
                "UPDATE messages SET text = NULL, mentions = '',"
                " state = CASE WHEN state = 'shown' THEN 'deleted' ELSE state END"
                " WHERE station_id = ?",
                (station_id,),
            )
            # A removed message's text kept for its Undo goes too.
            self.db.execute(
                "UPDATE review_actions SET undo_text = NULL WHERE action = 'remove' AND undo_text IS NOT NULL"
                " AND target IN (SELECT CAST(id AS TEXT) FROM messages WHERE station_id = ?)",
                (station_id,),
            )
        return cleared

    # ------------------------------------------------------------- schema

    def columns(self) -> Dict[str, List[str]]:
        """Every table's column names, for the schema tests."""
        out: Dict[str, List[str]] = {}
        for (name,) in self.db.execute(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\'"
        ):
            out[name] = [r["name"] for r in self.db.execute(f"PRAGMA table_info({name})")]
        return out
