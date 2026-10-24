# no-port-check: NereusSDR-original.
"""The nightly snapshot (design 6.2, 12.3, C12): a consistent copy named
by its UTC date, the newest 2 kept, skipped with a warning under 15% free
disk; and the one-shot `--daily` and `--snapshot` flags. Free disk is
always a fake here, never this machine's."""

from __future__ import annotations

import logging
import os
import sqlite3

import pytest

from inproc import Harness, presence, slice_
from nereus_roundtable import __main__ as entry
from nereus_roundtable import retention as retention_mod
from nereus_roundtable.retention import Retention, snapshot_name
from nereus_roundtable.store import TABLES, Store

DAY_MS = 86400000
# 2026-10-24 03:30 UTC.
NIGHT = 1792812600000


def _fake(free, total=100):
    return lambda path: (total, free)


@pytest.fixture
def harness(tmp_path):
    h = Harness(store=Store(str(tmp_path / "roundtable.db")))
    yield h
    h.close()


def _fill(harness):
    a = harness.sign_in(address="192.0.2.1")
    b = harness.sign_in(address="192.0.2.2")
    a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
    b.send(presence("KG4VCF", "EM73", [slice_(0, 14212010)]))
    harness.advance(10000)
    a.send({"type": "post", "room": "usb-14212000", "text": "hello", "clientId": "c1"})
    b.send({"type": "report", "target": "message", "msg": 1, "reason": "spam"})


def _counts(path):
    db = sqlite3.connect(path)
    try:
        return {name: db.execute(f"SELECT COUNT(*) FROM {name}").fetchone()[0] for name in TABLES}
    finally:
        db.close()


def test_snapshot_names_the_utc_date():
    assert snapshot_name(NIGHT) == "roundtable-2026-10-24.db"
    assert snapshot_name(NIGHT - 3 * 3600000 - 1800001) == "roundtable-2026-10-23.db"


def test_a_snapshot_opens_and_its_row_counts_equal_the_live_database(harness, tmp_path):
    _fill(harness)
    live = harness.store.row_counts()
    assert live["messages"] == 1 and live["reports"] == 1 and live["stations"] == 2
    path = Retention(harness.store, _fake(50)).snapshot(str(tmp_path / "backup"), NIGHT)
    assert path == str(tmp_path / "backup" / "roundtable-2026-10-24.db")
    assert _counts(path) == live
    db = sqlite3.connect(path)
    try:
        assert db.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
        assert db.execute("PRAGMA journal_mode").fetchone()[0] == "delete"
        assert db.execute("SELECT text FROM messages").fetchall() == [("hello",)]
    finally:
        db.close()
    # One file, no partial copy and no log beside it.
    assert sorted(os.listdir(tmp_path / "backup")) == ["roundtable-2026-10-24.db"]


def test_a_third_snapshot_removes_the_oldest(harness, tmp_path):
    backup = tmp_path / "backup"
    backup.mkdir()
    (backup / "notes.txt").write_text("kept")
    r = Retention(harness.store, _fake(50))
    for day in range(3):
        r.snapshot(str(backup), NIGHT + day * DAY_MS)
    assert sorted(os.listdir(backup)) == ["notes.txt", "roundtable-2026-10-25.db", "roundtable-2026-10-26.db"]
    # The same day again replaces that day's file.
    r.snapshot(str(backup), NIGHT + 2 * DAY_MS + 1000)
    assert sorted(os.listdir(backup)) == ["notes.txt", "roundtable-2026-10-25.db", "roundtable-2026-10-26.db"]


def test_fourteen_percent_free_skips_the_snapshot_with_a_warning(harness, tmp_path, caplog):
    backup = tmp_path / "backup"
    with caplog.at_level(logging.INFO, logger="nereus_roundtable"):
        assert Retention(harness.store, _fake(14)).snapshot(str(backup), NIGHT) is None
    assert os.listdir(backup) == []
    assert [(r.levelname, r.getMessage()) for r in caplog.records] == [
        ("WARNING", "snapshot skipped: 14.0% of the disk is free, under 15%")
    ]
    caplog.clear()
    with caplog.at_level(logging.INFO, logger="nereus_roundtable"):
        assert Retention(harness.store, _fake(15)).snapshot(str(backup), NIGHT) is not None
    assert [r.levelname for r in caplog.records] == ["INFO"]


def _config(tmp_path):
    path = tmp_path / "roundtable.ini"
    path.write_text(
        "[roundtable]\n"
        f"database = {tmp_path / 'live.db'}\n"
        f"backup_dir = {tmp_path / 'backup'}\n"
        "log_level = info\n"
    )
    return str(path)


def test_the_one_shot_flags_run_one_job_and_exit(tmp_path, monkeypatch, caplog):
    config = _config(tmp_path)
    Store(str(tmp_path / "live.db")).close()
    seen = []
    monkeypatch.setattr(retention_mod, "disk_usage", lambda p: seen.append(p) or (100, 50))
    caplog.set_level(logging.INFO, logger="nereus_roundtable")
    assert entry.main(["--config", config, "--snapshot"]) == 0
    assert [n for n in os.listdir(tmp_path / "backup") if n.endswith(".db")] == [
        snapshot_name(entry.RealClock().wall_ms())
    ]
    assert entry.main(["--config", config, "--daily"]) == 0
    assert seen == [str(tmp_path / "backup"), str(tmp_path)]
    said = [(r.levelname, r.getMessage().split(":")[0]) for r in caplog.records]
    assert ("INFO", "snapshot written") in said and ("INFO", "disk") in said
    with pytest.raises(SystemExit) as exc:
        entry.main(["--config", config, "--daily", "--snapshot"])
    assert exc.value.code == 2
