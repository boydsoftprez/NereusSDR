# no-port-check: NereusSDR-original.
"""A live service on loopback and a station that registers, for tests that
drive the service directly rather than through a session fixture."""

from __future__ import annotations

import asyncio
import json
import os
from contextlib import asynccontextmanager
from typing import Any, AsyncIterator, Dict, Optional, Tuple

from cryptography.hazmat.primitives.asymmetric import ec

from nereus_rendezvous import identity, protocol, transport
from nereus_rendezvous.clock import ManualClock
from nereus_rendezvous.service import Service
from runner import make_config, ws_connect


@asynccontextmanager
async def live_service(setup: Optional[Dict[str, Any]] = None, secret: bytes = b"test-secret") -> AsyncIterator[Tuple[Service, str]]:
    config = make_config(setup or {}, secret)
    service = Service(config, ManualClock())
    server = await transport.start(service, "127.0.0.1", 0)
    port = server.sockets[0].getsockname()[1]
    try:
        yield service, f"ws://127.0.0.1:{port}/"
    finally:
        await transport.stop([server], service, grace_s=0.05, timeout_s=5)


async def recv_json(ws: Any, timeout: float = 5.0) -> Dict[str, Any]:
    return json.loads(await asyncio.wait_for(ws.recv(), timeout))


async def register(uri: str, key: Optional[ec.EllipticCurvePrivateKey] = None, address: str = "192.0.2.1", watch_relay_version: Optional[int] = None) -> Tuple[Any, ec.EllipticCurvePrivateKey, str]:
    key = key or ec.generate_private_key(ec.SECP256R1())
    spki = identity.spki_of(key.public_key())
    sid = identity.rendezvous_id(spki)
    ws = await ws_connect(uri, address)
    hello = await recv_json(ws)
    assert hello["type"] == "hello"
    registration = {"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)}
    if watch_relay_version is not None:
        registration["watchRelayVersion"] = watch_relay_version
    await ws.send(protocol.encode(registration))
    challenge = await recv_json(ws)
    nonce = identity.from_b64url(challenge["nonce"])
    sig = identity.sign_raw(key, identity.register_transcript(nonce))
    await ws.send(protocol.encode({"type": "prove", "signature": identity.to_b64url(sig)}))
    registered = await recv_json(ws)
    assert registered == {"type": "registered", "id": sid}
    return ws, key, sid


async def expect_closed(ws: Any, timeout: float = 5.0) -> None:
    from websockets.exceptions import ConnectionClosed

    try:
        text = await asyncio.wait_for(ws.recv(), timeout)
    except ConnectionClosed:
        return
    raise AssertionError(f"received {text[:100]} instead of the close")


def introduce_message(sid: str, nonce_b64: str, device: Optional[ec.EllipticCurvePrivateKey] = None, offer: str = "v=0\r\n") -> Dict[str, Any]:
    device = device or ec.generate_private_key(ec.SECP256R1())
    nonce = identity.from_b64url(nonce_b64)
    sig = identity.sign_raw(device, identity.introduce_transcript(sid, nonce))
    return {
        "type": "introduce",
        "id": sid,
        "device": identity.to_b64url(identity.fingerprint(identity.spki_of(device.public_key()))),
        "deviceSignature": identity.to_b64url(sig),
        "offer": offer,
    }


def random_id() -> str:
    return identity.rendezvous_id(os.urandom(91))
