# no-port-check: NereusSDR-original.
"""Retention and the disk line (design 6.2, 6.4, C12): text older than
365 days is cleared and the row stays; the daily job writes one line with
the database's size and the free disk, a warning under 20% free. Free
disk is always a fake here, never this machine's."""

from __future__ import annotations

import logging

import pytest

from inproc import Harness, presence, slice_
from nereus_roundtable import retention as retention_mod
from nereus_roundtable.retention import RETENTION_MS, Retention

ROOM = "usb-14212000"
DAY_MS = 86400000


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


def _posted(harness):
    a = harness.sign_in(address="192.0.2.1")
    b = harness.sign_in(address="192.0.2.2")
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    b.send(presence("KG4VCF", "EM73", [slice_(0, 14212010)]))
    harness.advance(10000)
    ids = []
    for n, text in enumerate(("old news", "recent news", "today")):
        harness.advance(1000)
        a.send({"type": "post", "room": ROOM, "text": text, "clientId": f"c{n}"})
        got = [m for m in a.take() if m["type"] == "posted"]
        assert got and got[0]["ok"], got
        ids.append(got[0]["msg"])
    return ids


def _fake(free, total=100):
    return lambda path: (total, free)


def test_a_message_366_days_old_loses_its_text_and_one_364_days_old_keeps_it(harness, caplog):
    old, recent, today = _posted(harness)
    now = harness.clock.wall_ms()
    harness.store.db.execute("UPDATE messages SET t = ? WHERE id = ?", (now - 366 * DAY_MS, old))
    harness.store.db.execute("UPDATE messages SET t = ? WHERE id = ?", (now - 364 * DAY_MS, recent))
    harness.store.db.commit()
    before = {r["id"]: dict(r) for r in harness.store.db.execute("SELECT * FROM messages")}
    with caplog.at_level(logging.INFO, logger="nereus_roundtable"):
        got = Retention(harness.store, _fake(50)).daily(now)
    assert got["expired"] == 1
    after = {r["id"]: dict(r) for r in harness.store.db.execute("SELECT * FROM messages")}
    assert (after[old]["text"], after[old]["state"], after[old]["mentions"]) == (None, "expired", "")
    # The row, its sender, time and reading stay.
    for key in ("station_id", "callsign", "t", "dbm", "room_id", "nth", "opening_id"):
        assert after[old][key] == before[old][key], key
    assert (after[recent]["text"], after[recent]["state"]) == ("recent news", "shown")
    assert (after[today]["text"], after[today]["state"]) == ("today", "shown")
    # The log names no message.
    assert all("news" not in r.getMessage() for r in caplog.records)
    # Run again: nothing more.
    assert Retention(harness.store, _fake(50)).daily(now)["expired"] == 0


def test_the_boundary_is_365_days_to_the_millisecond(harness):
    old, recent, _ = _posted(harness)
    now = harness.clock.wall_ms()
    harness.store.db.execute("UPDATE messages SET t = ? WHERE id = ?", (now - RETENTION_MS - 1, old))
    harness.store.db.execute("UPDATE messages SET t = ? WHERE id = ?", (now - RETENTION_MS, recent))
    harness.store.db.commit()
    Retention(harness.store, _fake(50)).daily(now)
    assert harness.store.message_get(old)["state"] == "expired"
    assert harness.store.message_get(recent)["state"] == "shown"


def test_the_disk_line_gives_the_size_and_free_disk_and_warns_under_20_percent(harness, caplog):
    with caplog.at_level(logging.INFO, logger="nereus_roundtable"):
        got = Retention(harness.store, _fake(500, 1000)).daily(harness.clock.wall_ms())
    lines = [r for r in caplog.records if r.getMessage().startswith("disk:")]
    assert [(r.levelname, r.getMessage()) for r in lines] == [
        ("INFO", "disk: database 0 bytes, 500 bytes free (50.0% of the disk)")
    ]
    assert (got["freeBytes"], got["freeShare"]) == (500, 0.5)
    caplog.clear()
    with caplog.at_level(logging.INFO, logger="nereus_roundtable"):
        Retention(harness.store, _fake(199, 1000)).daily(harness.clock.wall_ms())
    lines = [r for r in caplog.records if r.getMessage().startswith("disk:")]
    assert [(r.levelname, r.getMessage()) for r in lines] == [
        ("WARNING", "disk: database 0 bytes, 199 bytes free (19.9% of the disk); under 20% free")
    ]
    caplog.clear()
    with caplog.at_level(logging.INFO, logger="nereus_roundtable"):
        Retention(harness.store, _fake(200, 1000)).daily(harness.clock.wall_ms())
    assert [r.levelname for r in caplog.records if r.getMessage().startswith("disk:")] == ["INFO"]


def test_the_disk_line_counts_the_database_file_and_its_log(tmp_path, monkeypatch):
    from nereus_roundtable.store import Store

    path = str(tmp_path / "roundtable.db")
    store = Store(path)
    try:
        seen = []
        monkeypatch.setattr(retention_mod, "disk_usage", lambda p: seen.append(p) or (100, 50))
        got = Retention(store).daily(0)
        assert seen == [str(tmp_path)]
        size = (tmp_path / "roundtable.db").stat().st_size
        wal = tmp_path / "roundtable.db-wal"
        assert got["databaseBytes"] == size + (wal.stat().st_size if wal.exists() else 0) > 0
    finally:
        store.close()


def test_the_daily_job_clears_text_kept_for_a_lapsed_undo(harness):
    old, _, _ = _posted(harness)
    now = harness.clock.wall_ms()
    store = harness.store
    store.action_add("a" * 26, "remove", str(old), {}, now - 600001, None, "kept")
    store.action_add("a" * 26, "remove", str(old), {}, now - 600000, None, "still kept")
    Retention(store, _fake(50)).daily(now)
    texts = [r[0] for r in store.db.execute("SELECT undo_text FROM review_actions ORDER BY id")]
    assert texts == [None, "still kept"]
