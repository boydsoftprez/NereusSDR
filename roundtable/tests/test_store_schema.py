# no-port-check: NereusSDR-original.
"""The history database: WAL, the schema's tables, no address column, and
the station and key queries (wire spec 9.3)."""

import re
import sqlite3

import pytest

from nereus_roundtable import identity
from nereus_roundtable.store import SCHEMA_VERSION, TABLES, Store, StoreError

EXPECTED_TABLES = {
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
}
ADDRESS_LIKE = re.compile(r"addr|(^|_)ip($|_)|address", re.IGNORECASE)


@pytest.fixture
def store(tmp_path):
    s = Store(str(tmp_path / "roundtable.db"))
    yield s
    s.close()


def test_wal(store, tmp_path):
    assert store.journal_mode == "wal"
    other = sqlite3.connect(str(tmp_path / "roundtable.db"))
    try:
        assert other.execute("PRAGMA journal_mode").fetchone()[0].lower() == "wal"
    finally:
        other.close()


def test_tables(store):
    columns = store.columns()
    assert set(columns) == EXPECTED_TABLES == set(TABLES)
    assert len(EXPECTED_TABLES) == 14


def test_no_column_holds_an_address(store):
    for table, names in store.columns().items():
        for name in names:
            assert not ADDRESS_LIKE.search(name), f"{table}.{name}"
    # The pattern itself finds what it is meant to.
    for name in ("ip", "client_ip", "addr", "peer_address", "IP_v6"):
        assert ADDRESS_LIKE.search(name), name


def test_schema_version(store, tmp_path):
    assert store.db.execute("SELECT version FROM schema_version").fetchall()[0][0] == SCHEMA_VERSION
    store.close()
    again = Store(str(tmp_path / "roundtable.db"))
    assert again.db.execute("SELECT count(*) FROM schema_version").fetchone()[0] == 1
    again.db.execute("UPDATE schema_version SET version = 99")
    again.close()
    with pytest.raises(StoreError):
        Store(str(tmp_path / "roundtable.db"))


def test_station_upsert_keeps_a_stored_callsign(store):
    sid = "ezp4x6p5d6d3opyk7lm2wuflhk"
    store.station_upsert(sid, "key", "W4XYZ", "EM73", 1000)
    store.station_upsert(sid, "key", None, None, 2000)
    row = store.station_get(sid)
    assert (row["callsign"], row["grid"], row["first_seen"], row["last_seen"]) == ("W4XYZ", "EM73", 1000, 2000)
    store.station_upsert(sid, "key", "K4ABC", None, 3000)
    assert store.station_get(sid)["callsign"] == "K4ABC"
    assert store.station_get("aaaaaaaaaaaaaaaaaaaaaaaaaa") is None


def test_keys(store):
    raw = identity.new_key()
    sid = identity.key_id(raw)
    digest = identity.key_hash(raw)
    assert store.key_lookup(digest) is None
    store.key_add(digest, sid, 1000)
    assert store.key_lookup(digest) == {"station_id": sid, "created": 1000, "revoked": None}
    assert store.key_revoke(digest, 2000)
    assert not store.key_revoke(digest, 3000)
    assert store.key_lookup(digest)["revoked"] == 2000
    assert not store.key_revoke(identity.key_hash(identity.new_key()), 3000)
    # Only the hash is stored, never the key.
    stored = store.db.execute("SELECT key_hash FROM keys").fetchall()
    assert [bytes(r[0]) for r in stored] == [digest]
