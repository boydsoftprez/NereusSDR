# no-port-check: NereusSDR-original.
"""Run the WebSocket relay: python3 -m nereus_relay --config FILE"""

from __future__ import annotations

import argparse
import asyncio
import logging
import signal
import sys

from nereus_rendezvous.__main__ import configure_logging as _configure_rendezvous_logging

from . import transport
from .config import ConfigError, load
from .relay import Relay


def configure_logging(level: str) -> None:
    # Standard error only (the journal under systemd), and the websockets
    # library's own records, which can carry addresses, switched off: the
    # rendezvous service's settings, for the relay's logger as well.
    _configure_rendezvous_logging(level)
    logging.getLogger("nereus_relay").setLevel(level.upper())


async def run(config_path: str) -> int:
    config = load(config_path)
    configure_logging(config.log_level)
    relay = Relay(config)
    servers = [await transport.start(relay)]
    log = logging.getLogger("nereus_relay")
    log.info("listening on its Unix socket, %d slots", config.slots)
    relay.start_day_timer()
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, stop.set)
    await stop.wait()
    log.info("stopping")
    log.info("%s (so far today, as it stops)", relay.data_use.line())
    await transport.stop(servers, relay)
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="nereus_relay")
    parser.add_argument("--config", required=True, help="the INI configuration file")
    args = parser.parse_args(argv)
    try:
        return asyncio.run(run(args.config))
    except ConfigError as exc:
        print(f"nereus_relay: configuration: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
