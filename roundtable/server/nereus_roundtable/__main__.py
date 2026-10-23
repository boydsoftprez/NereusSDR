# no-port-check: NereusSDR-original.
"""Run the Roundtable server: python3 -m nereus_roundtable --config FILE"""

from __future__ import annotations

import argparse
import asyncio
import logging
import signal
import sqlite3
import sys

from . import transport
from .clock import RealClock
from .config import ConfigError, load
from .service import Service
from .store import Store, StoreError


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


async def run(config_path: str) -> int:
    config = load(config_path)
    configure_logging(config.log_level)
    log = logging.getLogger("nereus_roundtable")
    try:
        store = Store(config.database)
    except (OSError, StoreError, sqlite3.Error) as exc:
        print(f"nereus_roundtable: database: {exc}", file=sys.stderr)
        return 1
    service = Service(config, RealClock(), store)
    listeners = await transport.start(service, None, config)
    log.info(
        "listening on %d WebSocket and %d HTTP addresses",
        len(listeners.ws_servers),
        len(listeners.http_servers),
    )
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            # Windows event loops do not implement add_signal_handler().
            signal.signal(sig, lambda _signum, _frame: loop.call_soon_threadsafe(stop.set))
    await stop.wait()
    log.info("stopping")
    await transport.stop(listeners, service)
    store.close()
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="nereus_roundtable")
    parser.add_argument("--config", required=True, help="the INI configuration file")
    args = parser.parse_args(argv)
    try:
        return asyncio.run(run(args.config))
    except ConfigError as exc:
        print(f"nereus_roundtable: configuration: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
