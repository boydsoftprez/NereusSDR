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

The schema is created whole on first open; schema_version names it so a
later version can move an older file forward.
"""

from __future__ import annotations

import sqlite3
from typing import Any, Dict, List, Optional, Tuple

SCHEMA_VERSION = 1

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
    not_counted_forever INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS keys (
    key_hash BLOB PRIMARY KEY,
    station_id TEXT NOT NULL,
    created INTEGER NOT NULL,
    revoked INTEGER
);
CREATE INDEX IF NOT EXISTS keys_by_station ON keys (station_id);
CREATE TABLE IF NOT EXISTS rooms (
    opening_id INTEGER PRIMARY KEY,
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
    id INTEGER PRIMARY KEY,
    opening_id INTEGER NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('talking', 'unknown', 'doubled')),
    talker_id TEXT,
    talker_callsign TEXT,
    second_talker_id TEXT,
    second_talker_callsign TEXT,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER
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
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY,
    opening_id INTEGER NOT NULL,
    room_id TEXT NOT NULL,
    station_id TEXT NOT NULL,
    callsign TEXT NOT NULL,
    nth INTEGER NOT NULL,
    t INTEGER NOT NULL,
    text TEXT,
    state TEXT NOT NULL CHECK (state IN ('shown', 'removed', 'deleted', 'expired')),
    dbm REAL,
    removed_by TEXT
);
CREATE INDEX IF NOT EXISTS messages_by_room ON messages (room_id, id);
CREATE INDEX IF NOT EXISTS messages_by_station ON messages (station_id);
CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY,
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
    closed_by TEXT
);
CREATE TABLE IF NOT EXISTS review_actions (
    id INTEGER PRIMARY KEY,
    helper_id TEXT NOT NULL,
    action TEXT NOT NULL,
    target TEXT NOT NULL,
    detail TEXT,
    t INTEGER NOT NULL,
    undone INTEGER
);
CREATE TABLE IF NOT EXISTS helpers (
    station_id TEXT PRIMARY KEY,
    added INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY,
    station_id TEXT NOT NULL,
    room_id TEXT NOT NULL,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER NOT NULL,
    turn_ids TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS sessions_by_station ON sessions (station_id, start_ms);
CREATE TABLE IF NOT EXISTS web_sessions (
    token_hash BLOB PRIMARY KEY,
    station_id TEXT NOT NULL,
    csrf_token TEXT NOT NULL,
    created INTEGER NOT NULL,
    expires INTEGER NOT NULL
);
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

    def _next_id(self, table: str, column: str) -> int:
        # Ids increase with time and are never reused (wire spec 4.4). No
        # AUTOINCREMENT, which would add a table of its own.
        row = self.db.execute(f"SELECT COALESCE(MAX({column}), 0) + 1 FROM {table}").fetchone()
        return int(row[0])

    def opening_open(self, room_id: str, band: str, family: str, center_hz: int, now: int) -> int:
        """A room opens: a new opening, whose id is returned."""
        opening_id = self._next_id("rooms", "opening_id")
        self.db.execute(
            "INSERT INTO rooms (opening_id, room_id, band, family, center_hz, opened, last_active, closed)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, NULL)",
            (opening_id, room_id, band, family, int(center_hz), now, now),
        )
        return opening_id

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
        with self.db:
            self.db.execute("BEGIN")
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
            rows = self.db.execute(
                "SELECT * FROM rooms WHERE closed IS NULL ORDER BY opening_id"
            ).fetchall()
        return [dict(r) for r in rows]

    def opening_get(self, opening_id: int) -> Optional[Dict[str, Any]]:
        row = self.db.execute("SELECT * FROM rooms WHERE opening_id = ?", (opening_id,)).fetchone()
        return dict(row) if row is not None else None

    # ------------------------------------------------------------- turns

    def turn_open(self, opening_id: int, kind: str, talkers: List[Tuple[str, str]], start: int) -> int:
        """A turn opens; its id is returned. `talkers` is (station id,
        callsign) in keying order; the first two are stored."""
        turn_id = self._next_id("turns", "id")
        first = talkers[0] if talkers else (None, None)
        second = talkers[1] if len(talkers) > 1 else (None, None)
        self.db.execute(
            "INSERT INTO turns (id, opening_id, kind, talker_id, talker_callsign, second_talker_id,"
            " second_talker_callsign, start_ms, end_ms) VALUES (?, ?, ?, ?, ?, ?, ?, ?, NULL)",
            (turn_id, opening_id, kind, first[0], first[1], second[0], second[1], start),
        )
        return turn_id

    def turn_talkers(self, turn_id: int, talkers: List[Tuple[str, str]]) -> None:
        first = talkers[0] if talkers else (None, None)
        second = talkers[1] if len(talkers) > 1 else (None, None)
        self.db.execute(
            "UPDATE turns SET talker_id = ?, talker_callsign = ?, second_talker_id = ?,"
            " second_talker_callsign = ? WHERE id = ?",
            (first[0], first[1], second[0], second[1], turn_id),
        )

    def turn_end(self, turn_id: int, end: int) -> None:
        self.db.execute("UPDATE turns SET end_ms = ? WHERE id = ?", (end, turn_id))

    def turns_of(self, opening_id: int) -> List[Dict[str, Any]]:
        rows = self.db.execute(
            "SELECT * FROM turns WHERE opening_id = ? ORDER BY start_ms, id", (opening_id,)
        ).fetchall()
        return [dict(r) for r in rows]

    def tracks_write(self, turn_id: int, tracks: List[Tuple[Any, ...]]) -> None:
        """Write (or rewrite) one turn's tracks: each is (station id,
        callsign, offset Hz, km, first second, last second, dBm bytes,
        present bits, not counted)."""
        if not tracks:
            return
        with self.db:
            self.db.execute("BEGIN")
            for sid, callsign, offset, km, first, last, dbm, bits, not_counted in tracks:
                self.db.execute(
                    "INSERT OR REPLACE INTO turn_readings (turn_id, station_id, callsign, offset_hz, km,"
                    " first_second, last_second, dbm_track, present_track, not_counted)"
                    " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (turn_id, sid, callsign, offset, km, first, last, bytes(dbm), bytes(bits), 1 if not_counted else 0),
                )

    def tracks_of(self, turn_id: int) -> List[Dict[str, Any]]:
        rows = self.db.execute(
            "SELECT * FROM turn_readings WHERE turn_id = ? ORDER BY station_id", (turn_id,)
        ).fetchall()
        return [dict(r) for r in rows]

    # ------------------------------------------------------------- schema

    def columns(self) -> Dict[str, List[str]]:
        """Every table's column names, for the schema tests."""
        out: Dict[str, List[str]] = {}
        for (name,) in self.db.execute("SELECT name FROM sqlite_master WHERE type = 'table'"):
            out[name] = [r["name"] for r in self.db.execute(f"PRAGMA table_info({name})")]
        return out
