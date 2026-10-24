# no-port-check: NereusSDR-original.
"""A live server on loopback and a station that signs in, for tests that
drive the server directly rather than through a session fixture."""

from __future__ import annotations

import asyncio
import json
from contextlib import asynccontextmanager
from typing import Any, AsyncIterator, Dict, Optional, Tuple

from cryptography.hazmat.primitives.asymmetric import ec
from websockets.exceptions import ConnectionClosed

from nereus_roundtable import identity, protocol, transport
from nereus_roundtable.http import App
from nereus_roundtable.clock import ManualClock
from nereus_roundtable.service import Service
from nereus_roundtable.store import Store
from runner import DEFAULT_ADDRESS, make_config, ws_connect


class Live:
    def __init__(self, service: Service, listeners: transport.Listeners, http_app: Optional[App] = None) -> None:
        self.service = service
        self.http_app = http_app
        self.listeners = listeners
        self.clock = service.clock
        self.base = f"ws://127.0.0.1:{listeners.ws_port()}"
        self.station_uri = self.base + transport.STATION_PATH
        self.live_uri = self.base + transport.LIVE_PATH
        self.http_port = listeners.http_port()


@asynccontextmanager
async def live_service(
    setup: Optional[Dict[str, Any]] = None, http_app: bool = False, static_dir: Optional[str] = None
) -> AsyncIterator[Live]:
    """A server on loopback with a ManualClock. With `http_app`, the HTTP
    listener serves the read API and `static_dir`."""
    config = make_config(setup or {})
    if static_dir is not None:
        config.static_dir = static_dir
    store = Store(":memory:")
    service = Service(config, ManualClock(), store)
    app = App(service, store, config) if http_app else None
    listeners = await transport.start(service, app, config)
    try:
        yield Live(service, listeners, app)
    finally:
        await transport.stop(listeners, service, grace_s=0.05, timeout_s=5)
        store.close()


async def recv_json(ws: Any, timeout: float = 5.0) -> Dict[str, Any]:
    return json.loads(await asyncio.wait_for(ws.recv(), timeout))


async def sign_in(
    live: Live, key: Optional[ec.EllipticCurvePrivateKey] = None, address: str = DEFAULT_ADDRESS
) -> Tuple[Any, ec.EllipticCurvePrivateKey, Dict[str, Any]]:
    """A station signs in with a station key; returns the connection, the
    key and the welcome. The rooms list after it is read too."""
    key = key or ec.generate_private_key(ec.SECP256R1())
    spki = identity.spki_of(key.public_key())
    ws = await ws_connect(live.station_uri, address)
    challenge = await recv_json(ws)
    assert challenge["type"] == "challenge"
    nonce = identity.from_b64url(challenge["nonce"])
    sig = identity.sign_raw(key, identity.hello_transcript(live.service.config.public_host, nonce))
    hello = {
        "type": "hello",
        "v": 1,
        "software": "test",
        "softwareVersion": "1",
        "spki": identity.to_b64url(spki),
        "sig": identity.to_b64url(sig),
    }
    await ws.send(protocol.encode(hello))
    welcome = await recv_json(ws)
    assert welcome["type"] == "welcome" and welcome["station"] == identity.roundtable_id(spki), welcome
    rooms = await recv_json(ws)
    assert rooms["type"] == "rooms", rooms
    return ws, key, welcome


async def expect_closed(ws: Any, timeout: float = 5.0) -> Optional[int]:
    """Wait for the server's close with no message before it; returns the
    close code it sent."""
    try:
        text = await asyncio.wait_for(ws.recv(), timeout)
    except ConnectionClosed as exc:
        return exc.rcvd.code if exc.rcvd is not None else None
    raise AssertionError(f"received {text[:100]} instead of the close")
