# no-port-check: NereusSDR-original.
"""Run the rendezvous service: python3 -m nereus_rendezvous --config FILE"""

from __future__ import annotations

import argparse
import asyncio
import logging
import signal
import sys

from . import transport
from .config import ConfigError, load
from .service import Service


def configure_logging(level: str) -> None:
    # Standard error only: under systemd that is the journal. No file.
    logging.basicConfig(stream=sys.stderr, format="%(levelname)s %(message)s")
    logging.getLogger("nereus_rendezvous").setLevel(level.upper())
    # The websockets library's own records can carry peer addresses; keep
    # them out of the log entirely.
    quiet = logging.getLogger("websockets")
    quiet.setLevel(logging.CRITICAL + 1)
    quiet.propagate = False
    quiet_named = logging.getLogger("websockets.quiet")
    quiet_named.setLevel(logging.CRITICAL + 1)
    quiet_named.propagate = False


async def run(config_path: str) -> int:
    config = load(config_path)
    configure_logging(config.log_level)
    service = Service(config)
    servers = [await transport.start(service, host, port) for host, port in config.listen]
    log = logging.getLogger("nereus_rendezvous")
    log.info(
        "listening on %d addresses, relay %s, relay grants %s",
        len(servers),
        "on" if config.turn_secret else "off",
        "on" if config.relay_secret else "off",
    )
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, stop.set)
    await stop.wait()
    log.info("stopping")
    await transport.stop(servers, service)
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="nereus_rendezvous")
    parser.add_argument("--config", required=True, help="the INI configuration file")
    args = parser.parse_args(argv)
    try:
        return asyncio.run(run(args.config))
    except ConfigError as exc:
        print(f"nereus_rendezvous: configuration: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
