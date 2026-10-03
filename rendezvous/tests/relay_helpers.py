# no-port-check: NereusSDR-original.
"""A live relay on loopback, legs that join it, and grants minted with its
secret, for the relay's tests (rendezvous document section 12)."""

from __future__ import annotations

import asyncio
import os
import tempfile
from contextlib import asynccontextmanager
from typing import Any, AsyncIterator, Dict, Optional, Tuple

from nereus_rendezvous import relaygrant
from nereus_rendezvous.clock import ManualClock
from nereus_relay import transport as relay_transport
from nereus_relay.config import Config
from nereus_relay.relay import TAG_END, TAG_JOIN, TAG_PEER, TAG_READY, Relay

try:  # websockets 13 and later
    from websockets.asyncio.client import connect as _ws_connect
    from websockets.asyncio.client import unix_connect as _ws_unix_connect

    _HEADERS_KW = "additional_headers"
except ImportError:  # websockets 10.x: the legacy client
    from websockets.legacy.client import connect as _ws_connect  # type: ignore[no-redef]
    from websockets.legacy.client import unix_connect as _ws_unix_connect  # type: ignore[no-redef]

    _HEADERS_KW = "extra_headers"

from websockets.exceptions import ConnectionClosed

WALL = 1800000000
SECRET = b"relay-test-secret-made-for-these-tests"
STATION_A = relaygrant.station_of(SECRET, "a" * 26)
STATION_B = relaygrant.station_of(SECRET, "b" * 26)


def make_relay_config(**overrides: Any) -> Config:
    config = Config()
    config.relay_secret = SECRET
    # Tests run as one user: no group to set, and the owner alone connects.
    config.socket_group = ""
    config.socket_mode = 0o600
    config.ping_interval_seconds = 0
    config.ping_timeout_seconds = 0
    for key, value in overrides.items():
        setattr(config, key, value)
    return config


@asynccontextmanager
async def live_relay(**overrides: Any) -> AsyncIterator[Tuple[Relay, ManualClock, str]]:
    clock = ManualClock(WALL)
    directory = short_directory()
    overrides.setdefault("socket", os.path.join(directory, "relay.sock"))
    relay = Relay(make_relay_config(**overrides), clock)
    server = await relay_transport.start(relay)
    try:
        yield relay, clock, "unix:" + relay.config.socket
    finally:
        await relay_transport.stop([server], relay, grace_s=0.05, timeout_s=5)
        os.rmdir(directory)


def short_directory() -> str:
    """A directory for a Unix socket whose path fits the platform's limit
    (104 bytes on macOS, where the default temporary directory is long)."""
    return tempfile.mkdtemp(prefix="nr", dir="/tmp")


def grant_pair(
    expires: int = WALL + 120, session: Optional[bytes] = None, station: Optional[bytes] = None
) -> Tuple[str, str, bytes]:
    """(core token, device token, session) of one grant. Each grant is for a
    station of its own unless one is named, so the per-station cap stays
    out of the way of tests about other things."""
    session = session or os.urandom(relaygrant.SESSION_BYTES)
    station = station or os.urandom(relaygrant.STATION_BYTES)
    return (
        relaygrant.mint(SECRET, relaygrant.LEG_CORE, session, station, expires),
        relaygrant.mint(SECRET, relaygrant.LEG_DEVICE, session, station, expires),
        session,
    )


async def connect(uri: str, address: str = "192.0.2.1", max_queue: Optional[int] = None) -> Any:
    """A leg as Caddy forwards it: to "unix:<path>" (the relay's socket) with
    X-Forwarded-For, or to a ws:// or wss:// URL."""
    kwargs: Dict[str, Any] = {
        _HEADERS_KW: {"X-Forwarded-For": address},
        "max_size": None,
        "ping_interval": None,
        "compression": None,
    }
    if max_queue is not None:
        kwargs["max_queue"] = max_queue
    if uri.startswith("unix:"):
        return await _ws_unix_connect(uri[len("unix:"):], "ws://localhost/v1/relay", **kwargs)
    return await _ws_connect(uri, **kwargs)


async def recv(ws: Any, timeout: float = 5.0) -> Any:
    return await asyncio.wait_for(ws.recv(), timeout)


async def join(uri: str, token: str, address: str = "192.0.2.1", **kwargs: Any) -> Tuple[Any, bytes]:
    """Open a leg, join it, and return it with the READY frame."""
    ws = await connect(uri, address, **kwargs)
    await ws.send(bytes([TAG_JOIN]) + token.encode("ascii"))
    ready = await recv(ws)
    assert isinstance(ready, bytes) and ready[0] == TAG_READY, ready
    return ws, ready


async def expect_end(ws: Any, code: str, close_code: int = 1000, timeout: float = 5.0) -> None:
    """The relay sends END with this code, then closes with close_code."""
    frame = await recv(ws, timeout)
    while isinstance(frame, bytes) and frame[0] == TAG_PEER:
        frame = await recv(ws, timeout)
    assert isinstance(frame, bytes) and frame[0] == TAG_END, frame
    assert frame[1:].decode("ascii") == code, frame
    await expect_close(ws, close_code, timeout)


async def expect_close(ws: Any, close_code: int, timeout: float = 5.0) -> None:
    try:
        frame = await recv(ws, timeout)
    except ConnectionClosed as exc:
        got = exc.rcvd.code if exc.rcvd is not None else None
        assert got == close_code, got
        return
    raise AssertionError(f"received {frame!r} instead of the close")


async def expect_nothing(ws: Any, timeout: float = 0.2) -> None:
    try:
        frame = await recv(ws, timeout)
    except asyncio.TimeoutError:
        return
    raise AssertionError(f"received {frame!r}")


async def settle(relay: Relay, handled: int, timeout: float = 5.0) -> None:
    """Wait until the relay has read `handled` frames in all."""
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while relay.frames_handled < handled:
        if loop.time() > deadline:
            raise AssertionError(f"the relay read {relay.frames_handled} frames, not {handled}")
        await asyncio.sleep(0.001)
    for _ in range(5):
        await asyncio.sleep(0)
