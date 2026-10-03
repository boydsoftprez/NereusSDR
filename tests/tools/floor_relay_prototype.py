# no-port-check: NereusSDR-original.
"""iPhone app plan Task 29 Step 1 (R-IOS-16): option (E) of the relay floor,
prototyped far enough to measure. Not the service's code.

Runs the rendezvous service exactly as `python3 -m nereus_rendezvous`
does, with one difference: a WebSocket whose request path is /v1/relay is
a relay leg instead of a rendezvous connection. A leg names a pairing
token and a role (device or core) in its query; the service forwards
every binary frame it receives on one leg to the other leg of the same
token, and drops it when the other leg is not there. A leg that drops and
comes back under the same token takes its place again, so a reset costs
only the frames sent meanwhile. The frames are ICE, DTLS, SCTP and SRTP
datagrams: the service sees only their ciphertext.

The measurement counts bytes, frames and the relay's CPU time and prints
them to standard error when a leg closes.

Usage: PYTHONPATH=rendezvous/server python3 floor_relay_prototype.py --config FILE

Modification history (NereusSDR):
  2026-09-26: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
              with AI-assisted implementation via Anthropic Claude Code.
"""

from __future__ import annotations

import argparse
import asyncio
import resource
import sys
import urllib.parse

from nereus_rendezvous import __main__ as service_main
from nereus_rendezvous import transport

RELAY_PATH = "/v1/relay"
MAX_FRAME_BYTES = 2048

_legs: dict = {}
# The measurement tool puts this text in every display frame; a relay that
# could read what it carries would find it.
PLAINTEXT_MARKER = b"NEREUS-FLOOR-PLAINTEXT-MARKER"
_counts = {"frames": 0, "bytes": 0, "dropped": 0, "plaintext": 0}


def _path(ws) -> str:
    request = getattr(ws, "request", None)
    if request is not None:
        return request.path
    return getattr(ws, "path", "")


async def relay_leg(ws, query: dict) -> None:
    token = (query.get("token") or [""])[0]
    role = (query.get("role") or [""])[0]
    if not token or role not in ("device", "core"):
        await ws.close(4000)
        return
    other = "core" if role == "device" else "device"
    legs = _legs.setdefault(token, {})
    legs[role] = ws
    try:
        async for frame in ws:
            if not isinstance(frame, (bytes, bytearray)) or len(frame) > MAX_FRAME_BYTES:
                continue
            peer = legs.get(other)
            if peer is None:
                _counts["dropped"] += 1
                continue
            _counts["frames"] += 1
            _counts["bytes"] += len(frame)
            if PLAINTEXT_MARKER in frame:
                _counts["plaintext"] += 1
            try:
                await peer.send(frame)
            except Exception:
                _counts["dropped"] += 1
    finally:
        if legs.get(role) is ws:
            del legs[role]
        if not legs:
            _legs.pop(token, None)
        usage = resource.getrusage(resource.RUSAGE_SELF)
        print(
            f"relay leg closed: role={role} frames={_counts['frames']} bytes={_counts['bytes']} "
            f"dropped={_counts['dropped']} plaintext={_counts['plaintext']} cpu_s={usage.ru_utime + usage.ru_stime:.3f}",
            file=sys.stderr,
            flush=True,
        )


_original_start = transport.start


async def start_with_relay(service, host, port):
    original_run = service.run_connection

    async def run_connection(ws_transport, address):
        ws = getattr(ws_transport, "_ws", None) or getattr(ws_transport, "ws", None)
        path = _path(ws) if ws is not None else ""
        parsed = urllib.parse.urlsplit(path)
        if parsed.path == RELAY_PATH:
            await relay_leg(ws, urllib.parse.parse_qs(parsed.query))
            return
        await original_run(ws_transport, address)

    service.run_connection = run_connection
    return await _original_start(service, host, port)


def main() -> int:
    parser = argparse.ArgumentParser(prog="floor_relay_prototype")
    parser.add_argument("--config", required=True)
    args = parser.parse_args()
    transport.start = start_with_relay
    return asyncio.run(service_main.run(args.config))


if __name__ == "__main__":
    sys.exit(main())
