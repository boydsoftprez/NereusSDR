# no-port-check: NereusSDR-original.
"""The daily jobs: retention, the disk line and the snapshot (design 6.2,
6.4; wire spec 9.3).

- `Retention(store, free_disk).daily(now)`: every message posted more than
  365 days before `now` loses its text (`state` `expired`; the row, its
  sender, time and reading stay), the text kept for a lapsed Undo goes, and
  one journal line gives the database's size and the free disk, with a
  warning under 20% free. Its timer runs it at 04:00 UTC.
- `Retention(store, free_disk).snapshot(dir, now)`: a consistent copy of
  the database through SQLite's online backup, written to
  `roundtable-YYYY-MM-DD.db` (the UTC date of `now`) in `dir`, keeping the
  newest 2. While the disk holding `dir` is under 15% free it is skipped
  with a warning.

`free_disk(path)` gives `(total, free)` bytes of the disk holding `path`;
it is `shutil.disk_usage` unless a test gives another. Readings are never
deleted to make room (design 6.2).

`python3 -m nereus_roundtable --config FILE --daily` or `--snapshot` runs
one job and exits, for the timers. Log lines name no message, station or
address (wire spec 9.4).
"""

from __future__ import annotations

import datetime
import logging
import os
import re
import shutil
import sqlite3
from typing import Any, Callable, Dict, Optional, Tuple

from .moderation import UNDO_MS

log = logging.getLogger("nereus_roundtable")

# Design 6.4: text is cleared 365 days after posting.
RETENTION_MS = 365 * 86400000
# Design 6.2 and C12.
SNAPSHOTS_KEPT = 2
SNAPSHOT_MIN_FREE = 0.15
DISK_WARN_FREE = 0.20
SNAPSHOT_NAME = re.compile(r"\Aroundtable-\d{4}-\d{2}-\d{2}\.db\Z")


def disk_usage(path: str) -> Tuple[int, int]:
    usage = shutil.disk_usage(path)
    return int(usage.total), int(usage.free)


def snapshot_name(now: int) -> str:
    day = datetime.datetime.fromtimestamp(now / 1000, tz=datetime.timezone.utc)
    return f"roundtable-{day:%Y-%m-%d}.db"


class Retention:
    def __init__(self, store: Any, free_disk: Optional[Callable[[str], Tuple[int, int]]] = None) -> None:
        self.store = store
        self.free_disk = free_disk or disk_usage

    def _database_dir(self) -> str:
        path = self.store.path
        if path == ":memory:" or not path:
            return os.getcwd()
        return os.path.dirname(os.path.abspath(path)) or os.getcwd()

    def database_bytes(self) -> int:
        """The database file with its write-ahead log."""
        path = self.store.path
        if path == ":memory:" or not path:
            return 0
        total = 0
        for name in (path, path + "-wal"):
            try:
                total += os.path.getsize(name)
            except OSError:
                pass
        return total

    def daily(self, now: int) -> Dict[str, Any]:
        """The 04:00 UTC job. Returns what it did and saw."""
        expired = self.store.messages_expire(now - RETENTION_MS)
        self.store.undo_purge(now - UNDO_MS)
        size = self.database_bytes()
        total, free = self.free_disk(self._database_dir())
        share = free / total if total > 0 else 0.0
        line = f"disk: database {size} bytes, {free} bytes free ({share * 100:.1f}% of the disk)"
        if share < DISK_WARN_FREE:
            log.warning("%s; under %d%% free", line, int(DISK_WARN_FREE * 100))
        else:
            log.info("%s", line)
        log.info("retention: cleared the text of %d messages older than 365 days", len(expired))
        return {"expired": len(expired), "databaseBytes": size, "freeBytes": free, "freeShare": share}

    @staticmethod
    def _remove_part(part: str) -> None:
        """A partial copy left by a run that stopped part way."""
        for name in (part, part + "-wal", part + "-shm"):
            if os.path.exists(name):
                os.remove(name)

    def snapshot(self, directory: str, now: int) -> Optional[str]:
        """The nightly snapshot; its path, or None when skipped."""
        os.makedirs(directory, exist_ok=True)
        total, free = self.free_disk(directory)
        share = free / total if total > 0 else 0.0
        if share < SNAPSHOT_MIN_FREE:
            log.warning(
                "snapshot skipped: %.1f%% of the disk is free, under %d%%", share * 100, int(SNAPSHOT_MIN_FREE * 100)
            )
            return None
        final = os.path.join(directory, snapshot_name(now))
        part = final + ".part"
        self._remove_part(part)
        dest = sqlite3.connect(part)
        try:
            self.store.db.backup(dest)
            # One file, with no write-ahead log beside it.
            dest.execute("PRAGMA journal_mode=DELETE")
            dest.commit()
        finally:
            dest.close()
        # Leaving WAL mode can leave an empty -shm beside the copy.
        for sidecar in (part + "-wal", part + "-shm"):
            if os.path.exists(sidecar):
                os.remove(sidecar)
        os.replace(part, final)
        kept = sorted(name for name in os.listdir(directory) if SNAPSHOT_NAME.match(name))
        for name in kept[:-SNAPSHOTS_KEPT]:
            os.remove(os.path.join(directory, name))
        log.info("snapshot written: %s", os.path.basename(final))
        return final
