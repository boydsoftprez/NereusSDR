# no-port-check: NereusSDR-original.
"""Run the Roundtable server: python3 -m nereus_roundtable --config FILE

With --daily or --snapshot it runs that one job on the database and exits
(retention.py), for the systemd timers.

SIGTERM or SIGINT stops the server cleanly (systemd sends SIGTERM). With
--stop-on-stdin-eof it also stops the same way when its standard input
reaches end of file: a stop that works on every OS, for a parent process
such as the end-to-end test, since Windows has no SIGTERM to send a console
program. It is off unless given."""

from __future__ import annotations

import argparse
import asyncio
import logging
import os
import signal
import sqlite3
import sys
import threading

from . import transport
from .http import App
from .moderation import Moderation
from .retention import Retention
from .clock import RealClock
from .config import ConfigError, load
from .service import Service
from .store import Store, StoreError
from .web import Web


def configure_logging(level: str) -> None:
    # Standard error only: under systemd that is the journal. No file.
    logging.basicConfig(stream=sys.stderr, format="%(levelname)s %(message)s")
    logging.getLogger("nereus_roundtable").setLevel(level.upper())
    # The websockets library's own records can carry peer addresses; keep
    # them out of the log entirely.
    for name in ("websockets", "websockets.quiet"):
        quiet = logging.getLogger(name)
        quiet.setLevel(logging.CRITICAL + 1)
        quiet.propagate = False
    # asyncio's records for a failed connection can name the peer too.
    logging.getLogger("asyncio").setLevel(logging.CRITICAL + 1)


def _watch_stdin(loop: asyncio.AbstractEventLoop, stop: asyncio.Event) -> None:
    """Stop when standard input reaches end of file. A daemon thread reads
    the raw descriptor, which works on Windows' Proactor loop as on POSIX
    (an event-loop reader does not), and holds no lock of sys.stdin, so a
    stop by signal while it still waits exits cleanly. Whatever arrives on
    standard input is read and dropped."""
    log = logging.getLogger("nereus_roundtable")

    def watch() -> None:
        try:
            fd = sys.stdin.fileno()
            while os.read(fd, 4096):
                pass
        except (OSError, ValueError, AttributeError):
            # No usable standard input counts as its end.
            pass
        try:
            loop.call_soon_threadsafe(log.info, "standard input closed")
            loop.call_soon_threadsafe(stop.set)
        except RuntimeError:
            # The loop already closed: the server stopped another way.
            pass

    threading.Thread(target=watch, name="stdin-eof", daemon=True).start()


async def run(config_path: str, stop_on_stdin_eof: bool = False) -> int:
    config = load(config_path)
    configure_logging(config.log_level)
    log = logging.getLogger("nereus_roundtable")
    try:
        store = Store(config.database)
    except (OSError, StoreError, sqlite3.Error) as exc:
        print(f"nereus_roundtable: database: {exc}", file=sys.stderr)
        return 1
    service = Service(config, RealClock(), store)
    app = App(service, store, config)
    web = Web(service, store, config, app)
    Moderation(web, app)
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            # Windows event loops do not implement add_signal_handler().
            signal.signal(sig, lambda _signum, _frame: loop.call_soon_threadsafe(stop.set))
    # Listener readiness must imply that SIGTERM can take the cleanup path.
    # start() awaits while opening sockets, so install handlers before it.
    listeners = await transport.start(service, app, config)
    log.info(
        "listening on %d WebSocket and %d HTTP addresses",
        len(listeners.ws_servers),
        len(listeners.http_servers),
    )
    if stop_on_stdin_eof:
        _watch_stdin(loop, stop)
    await stop.wait()
    log.info("stopping")
    await transport.stop(listeners, service)
    store.close()
    return 0


def run_job(config_path: str, job: str) -> int:
    """One job on the database, then exit: the daily retention and disk
    line, or the nightly snapshot."""
    config = load(config_path)
    configure_logging(config.log_level)
    try:
        store = Store(config.database)
    except (OSError, StoreError, sqlite3.Error) as exc:
        print(f"nereus_roundtable: database: {exc}", file=sys.stderr)
        return 1
    try:
        retention = Retention(store)
        now = RealClock().wall_ms()
        if job == "daily":
            retention.daily(now)
        else:
            retention.snapshot(config.backup_dir, now)
    except (OSError, sqlite3.Error) as exc:
        print(f"nereus_roundtable: {job}: {type(exc).__name__}", file=sys.stderr)
        return 1
    finally:
        store.close()
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="nereus_roundtable")
    parser.add_argument("--config", required=True, help="the INI configuration file")
    jobs = parser.add_mutually_exclusive_group()
    jobs.add_argument("--daily", action="store_true", help="run the daily retention job and exit")
    jobs.add_argument("--snapshot", action="store_true", help="write the nightly snapshot and exit")
    parser.add_argument(
        "--stop-on-stdin-eof",
        action="store_true",
        help="also stop cleanly, as on SIGTERM, when standard input reaches end of file",
    )
    args = parser.parse_args(argv)
    try:
        if args.daily or args.snapshot:
            return run_job(args.config, "daily" if args.daily else "snapshot")
        return asyncio.run(run(args.config, args.stop_on_stdin_eof))
    except ConfigError as exc:
        print(f"nereus_roundtable: configuration: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
