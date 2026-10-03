# no-port-check: NereusSDR-original.
"""The service's conformance runner (runs value "service").

It builds a real service from a fixture's serverSetup, listens on loopback,
plays every connection itself over real WebSockets, fills the connections'
messages and matches every message the service sends. The fixture format
and the placeholder grammar are the rendezvous document's section 10.
"""

from __future__ import annotations

import asyncio
import base64
import hashlib
import hmac
import json
import os
import re
from pathlib import Path
from typing import Any, Dict, List, Optional

from cryptography.hazmat.primitives.asymmetric import ec

from nereus_rendezvous import identity, protocol, relaygrant, transport
from nereus_rendezvous.clock import ManualClock
from nereus_rendezvous.config import Config
from nereus_rendezvous.service import Service

try:  # websockets 13 and later
    from websockets.asyncio.client import connect as _ws_connect

    _HEADERS_KW = "additional_headers"
except ImportError:  # websockets 10.x: the legacy client
    from websockets.legacy.client import connect as _ws_connect  # type: ignore[no-redef]

    _HEADERS_KW = "extra_headers"

from websockets.exceptions import ConnectionClosed

CONFORMANCE = Path(__file__).resolve().parent.parent / "conformance" / "v1"
DEFAULT_ADDRESS = "192.0.2.1"
DEFAULT_WALL_CLOCK = 1800000000
FIXTURE_STUN = ["stun:rv4.conformance.invalid:3478", "stun:rv6.conformance.invalid:3478"]
FIXTURE_TURN = [
    "turn:rv4.conformance.invalid:3478?transport=udp",
    "turn:rv6.conformance.invalid:3478?transport=udp",
]
FIXTURE_RELAY_URL = "wss://rv.conformance.invalid/v1/relay"
RELAY_LEGS = {"core": relaygrant.LEG_CORE, "device": relaygrant.LEG_DEVICE}
RECV_TIMEOUT_S = 5.0
SILENCE_S = 0.1

_SETUP_KEYS = {
    "stunUrls": "stun_urls",
    "turnUrls": "turn_urls",
    "turnTtlSeconds": "turn_ttl_seconds",
    "relayUrl": "relay_url",
    "relayTtlSeconds": "relay_ttl_seconds",
    "introductionsPerAddressPerMinute": "introductions_per_address_per_minute",
    "introductionsPerStationPerMinute": "introductions_per_station_per_minute",
    "mailboxOpensPerAddressPerMinute": "mailbox_opens_per_address_per_minute",
    "candidatesPerSide": "candidates_per_side",
    "mailboxMessagesPerSide": "mailbox_messages_per_side",
    "connectionsPerAddress": "connections_per_address",
    "stationsPerAddress": "stations_per_address",
    "maxConnections": "max_connections",
    "maxStations": "max_stations",
    "handshakeTimeoutMs": "handshake_timeout_ms",
    "idleTimeoutMs": "idle_timeout_ms",
    "introductionLifetimeMs": "introduction_lifetime_ms",
    "mailboxLifetimeMs": "mailbox_lifetime_ms",
}

_PLACEHOLDER = re.compile(
    r"\A\$(any|string|int|capture|ref|b64|key|device|register|introduce|turn|sdp|candidate|relayToken)(?::(.*))?\Z"
)


class FixtureFailure(AssertionError):
    pass


def load_manifest() -> Dict[str, Any]:
    return json.loads((CONFORMANCE / "manifest.json").read_text(encoding="utf-8"))


def load_fixture(path: str) -> Dict[str, Any]:
    return json.loads((CONFORMANCE / path).read_text(encoding="utf-8"))


def sdp_text(which: str) -> str:
    return (CONFORMANCE / "sdp" / f"{which}.sdp").read_bytes().decode("utf-8")


def role_of(conn: str) -> str:
    """A connection named station... plays the station role, client... the
    client role (section 10.3)."""
    if conn.startswith("station"):
        return "station"
    if conn.startswith("client"):
        return "client"
    raise FixtureFailure(f"connection {conn!r} is neither a station nor a client")


async def ws_connect(uri: str, address: str) -> Any:
    kwargs = {
        _HEADERS_KW: {"X-Forwarded-For": address},
        "max_size": None,
        "ping_interval": None,
        "compression": None,
    }
    return await _ws_connect(uri, **kwargs)


def make_config(setup: Dict[str, Any], secret: bytes, relay_secret: Optional[bytes] = None) -> Config:
    config = Config()
    config.stun_urls = list(FIXTURE_STUN)
    config.turn_urls = list(FIXTURE_TURN)
    config.turn_secret = secret
    config.relay_url = FIXTURE_RELAY_URL
    config.ping_interval_seconds = 0
    config.ping_timeout_seconds = 0
    for key, value in setup.items():
        if key == "turn":
            if not isinstance(value, bool):
                raise FixtureFailure("serverSetup.turn must be true or false")
            config.turn_secret = secret if value else None
        elif key == "relay":
            # Section 10.4: a relay secret is configured (the runner makes
            # one at run time), so accepted introductions get relay grants.
            if not isinstance(value, bool):
                raise FixtureFailure("serverSetup.relay must be true or false")
            config.relay_secret = (relay_secret or os.urandom(32).hex().encode("ascii")) if value else None
        elif key == "wallClock":
            continue
        elif key in _SETUP_KEYS:
            setattr(config, _SETUP_KEYS[key], value)
        else:
            raise FixtureFailure(f"unknown serverSetup key {key}")
    return config


class Runner:
    def __init__(self, fixture: Dict[str, Any], name: str) -> None:
        self.fixture = fixture
        self.name = name
        allowed = {"runs", "serverSetup", "pairedDevices", "steps"}
        extra = set(fixture) - allowed
        if extra:
            raise FixtureFailure(f"{name}: unknown top-level keys {sorted(extra)}")
        self.setup = fixture.get("serverSetup", {})
        self.secret = os.urandom(32).hex().encode("ascii")
        self.relay_secret = os.urandom(32).hex().encode("ascii")
        self.clock = ManualClock(self.setup.get("wallClock", DEFAULT_WALL_CLOCK))
        self.config = make_config(self.setup, self.secret, self.relay_secret)
        self.service = Service(self.config, self.clock)
        self.conns: Dict[str, Any] = {}
        self.keys: Dict[str, ec.EllipticCurvePrivateKey] = {}
        self.devices: Dict[str, ec.EllipticCurvePrivateKey] = {}
        self.records: Dict[str, Any] = {}
        self.fills: Dict[str, Any] = {}
        self.counter = 0
        self.candidates = 0
        self.sent = 0
        self.disconnects = 0
        # Every raw text the service sent, by connection, for tests that
        # compare bytes.
        self.raw: Dict[str, List[str]] = {}

    # ------------------------------------------------------------- keys

    def key(self, name: str) -> ec.EllipticCurvePrivateKey:
        if name not in self.keys:
            self.keys[name] = ec.generate_private_key(ec.SECP256R1())
        return self.keys[name]

    def device(self, name: str) -> ec.EllipticCurvePrivateKey:
        if name not in self.devices:
            self.devices[name] = ec.generate_private_key(ec.SECP256R1())
        return self.devices[name]

    def station_id(self, name: str) -> str:
        return identity.rendezvous_id(identity.spki_of(self.key(name).public_key()))

    def device_id(self, name: str) -> str:
        return identity.to_b64url(identity.fingerprint(identity.spki_of(self.device(name).public_key())))

    # ------------------------------------------------------------- fill

    def fill(self, template: Any) -> Any:
        if isinstance(template, dict):
            return {k: self.fill(v) for k, v in template.items()}
        if isinstance(template, list):
            return [self.fill(v) for v in template]
        if not isinstance(template, str):
            return template
        m = _PLACEHOLDER.match(template)
        if not m:
            return template
        kind, arg = m.group(1), m.group(2)
        parts = arg.split(":") if arg is not None else []
        if kind == "string":
            if parts:
                self.records[arg] = "conformance"
            return "conformance"
        if kind == "int":
            if not parts:
                return 0
            self.counter += 1
            self.records[parts[0]] = self.counter
            return self.counter
        if kind == "ref":
            if arg not in self.records:
                raise FixtureFailure(f"{template}: nothing recorded under {arg}")
            return self.records[arg]
        if kind == "b64":
            value = identity.to_b64url(os.urandom(int(parts[0])))
            self.records[parts[1]] = value
            return value
        if kind == "key":
            if parts[1] == "id":
                return self.station_id(parts[0])
            return identity.to_b64url(identity.spki_of(self.key(parts[0]).public_key()))
        if kind == "device":
            return self.device_id(parts[0])
        if kind == "register":
            k, nonce_name, case = parts
            nonce = self._nonce(nonce_name, template)
            if case == "otherNonce":
                nonce = os.urandom(identity.NONCE_BYTES)
            signature = identity.sign_raw(self.key(k), identity.register_transcript(nonce))
            value = identity.to_b64url(self._case(signature, case, template))
            self.fills[template] = value
            return value
        if kind == "introduce":
            d, k, nonce_name, case = parts
            nonce = self._nonce(nonce_name, template)
            if case == "otherNonce":
                nonce = os.urandom(identity.NONCE_BYTES)
            signature = identity.sign_raw(self.device(d), identity.introduce_transcript(self.station_id(k), nonce))
            value = identity.to_b64url(self._case(signature, case, template))
            self.fills[template] = value
            return value
        if kind == "sdp":
            value = sdp_text(parts[0])
            self.records[parts[1]] = value
            return value
        if kind == "relayToken":
            # A core or app runner plays the service here; the service's
            # runner only ever matches this placeholder.
            leg, session_name, k = self._relay_parts(parts, template)
            session = self.records.setdefault("relaySession:" + session_name, os.urandom(relaygrant.SESSION_BYTES))
            expires = self.clock.wall_seconds() + self.config.relay_ttl_seconds
            station = relaygrant.station_of(self.relay_secret, self.station_id(k))
            return relaygrant.mint(self.relay_secret, leg, session, station, expires)
        if kind == "candidate":
            self.candidates += 1
            n = self.candidates
            value = f"candidate:{n} 1 UDP 2122317823 2001:db8::{n} {50000 + n} typ host"
            self.records[parts[0]] = value
            return value
        raise FixtureFailure(f"{template} cannot be filled in a connection's message")

    @staticmethod
    def _relay_parts(parts: List[str], template: str) -> Any:
        if len(parts) != 3 or parts[0] not in RELAY_LEGS or not parts[1] or not parts[2]:
            raise FixtureFailure(f"{template}: write $relayToken:<core|device>:<session>:<station key>")
        return RELAY_LEGS[parts[0]], parts[1], parts[2]

    def _nonce(self, name: str, template: str) -> bytes:
        if name not in self.records:
            raise FixtureFailure(f"{template}: no nonce recorded under {name}")
        data = identity.from_b64url(self.records[name])
        if data is None or len(data) != identity.NONCE_BYTES:
            raise FixtureFailure(f"{template}: {name} is not a nonce")
        return data

    @staticmethod
    def _case(signature: bytes, case: str, template: str) -> bytes:
        if case in ("signed", "otherNonce"):
            return signature
        if case == "flippedBit":
            flipped = bytearray(signature)
            flipped[-1] ^= 1
            return bytes(flipped)
        raise FixtureFailure(f"{template}: unknown case {case}")

    # ------------------------------------------------------------- match

    def match(self, template: Any, actual: Any, path: str) -> None:
        if isinstance(template, str):
            m = _PLACEHOLDER.match(template)
            if m:
                self._match_placeholder(m.group(1), m.group(2), template, actual, path)
                return
        if isinstance(template, dict):
            if not isinstance(actual, dict) or set(template) != set(actual):
                raise FixtureFailure(f"{path}: keys {sorted(actual) if isinstance(actual, dict) else actual!r} are not {sorted(template)}")
            for k in template:
                self.match(template[k], actual[k], f"{path}.{k}")
            return
        if isinstance(template, list):
            if not isinstance(actual, list) or len(actual) != len(template):
                raise FixtureFailure(f"{path}: {actual!r} is not a list of {len(template)}")
            for i, (t, a) in enumerate(zip(template, actual)):
                self.match(t, a, f"{path}[{i}]")
            return
        if isinstance(template, bool) or isinstance(actual, bool) or template is None or actual is None:
            if template is not actual:
                raise FixtureFailure(f"{path}: {actual!r} is not {template!r}")
            return
        if template != actual:
            raise FixtureFailure(f"{path}: {actual!r} is not {template!r}")

    def _match_placeholder(self, kind: str, arg: Optional[str], template: str, actual: Any, path: str) -> None:
        parts = arg.split(":") if arg is not None else []

        def need(ok: bool, what: str) -> None:
            if not ok:
                raise FixtureFailure(f"{path}: {actual!r} is not {what} ({template})")

        is_int = isinstance(actual, int) and not isinstance(actual, bool)
        if kind == "any":
            return
        if kind == "string":
            need(isinstance(actual, str), "a string")
            if parts:
                self.records[arg] = actual
            return
        if kind == "int":
            need(is_int, "a whole number")
            if parts:
                self.records[parts[0]] = actual
            return
        if kind == "capture":
            self.records[arg] = actual
            return
        if kind == "ref":
            need(arg in self.records and self.records[arg] == actual, f"the value recorded as {arg}")
            return
        if kind == "b64":
            need(identity.b64url_of_length(actual, int(parts[0])) is not None, f"base64url of {parts[0]} bytes")
            self.records[parts[1]] = actual
            return
        if kind == "key":
            want = (
                self.station_id(parts[0])
                if parts[1] == "id"
                else identity.to_b64url(identity.spki_of(self.key(parts[0]).public_key()))
            )
            need(actual == want, f"key {parts[0]}'s {parts[1]}")
            return
        if kind == "device":
            need(actual == self.device_id(parts[0]), f"device {parts[0]}'s id")
            return
        if kind in ("introduce", "register"):
            need(template in self.fills and self.fills[template] == actual, "the signature that was sent")
            return
        if kind == "sdp":
            need(isinstance(actual, str) and actual != "", "an SDP text")
            self.records[parts[1]] = actual
            return
        if kind == "candidate":
            need(isinstance(actual, str) and (actual == "" or actual.startswith("candidate:")), "a candidate")
            self.records[parts[0]] = actual
            return
        if kind == "turn":
            k, name = parts
            need(isinstance(actual, dict) and set(actual) == {"username", "password", "expires", "urls"}, "a TURN object")
            expires = self.clock.wall_seconds() + self.config.turn_ttl_seconds
            need(actual["expires"] == expires, f"expiring at {expires}")
            need(actual["username"] == f"{expires}:{self.station_id(k)}", "the expiry and the station id")
            # coturn's algorithm, computed here and not by the service's code.
            digest = hmac.new(self.secret, actual["username"].encode("utf-8"), hashlib.sha1).digest()
            need(actual["password"] == base64.b64encode(digest).decode("ascii"), "HMAC-SHA1 of the username")
            need(actual["urls"] == list(self.config.turn_urls), "the configured TURN URLs")
            self.records[name] = actual
            return
        if kind == "relayToken":
            leg, session_name, k = self._relay_parts(parts, template)
            # Section 12.2, recomputed here with hmac directly rather than
            # with the service's code.
            raw = identity.b64url_of_length(actual, relaygrant.TOKEN_BYTES)
            need(raw is not None, "base64url of 62 bytes")
            payload, mac = raw[: relaygrant.PAYLOAD_BYTES], raw[relaygrant.PAYLOAD_BYTES :]
            want = hmac.new(self.relay_secret, b"NereusSDR relay grant v1\n" + payload, hashlib.sha256).digest()
            need(hmac.compare_digest(mac, want), "HMAC-SHA256 of the payload under the relay secret")
            need(payload[0] == 1, "version 1")
            need(payload[1] == leg, f"the {parts[0]} leg")
            expires = self.clock.wall_seconds() + self.config.relay_ttl_seconds
            station = hmac.new(
                self.relay_secret, b"NereusSDR relay station v1\n" + self.station_id(k).encode(), hashlib.sha256
            ).digest()[:8]
            need(payload[18:26] == station, f"station key {k}'s value")
            need(int.from_bytes(payload[26:30], "big") == expires, f"expiring at {expires}")
            session = payload[2:18]
            key = "relaySession:" + session_name
            if key in self.records:
                need(self.records[key] == session, f"the session recorded as {session_name}")
            else:
                # A new name is a new session: no other name holds it.
                others = [v for k, v in self.records.items() if k.startswith("relaySession:")]
                need(session not in others, f"a session of its own ({session_name})")
                self.records[key] = session
            return
        raise FixtureFailure(f"{path}: unknown placeholder {template}")

    # ------------------------------------------------------------- run

    async def quiesce(self) -> None:
        """Wait until the service has read every frame sent and seen every
        close, and every outbound queue is empty."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + RECV_TIMEOUT_S
        while True:
            caught_up = self.service.frames_handled >= self.sent and self.service.peer_closes >= self.disconnects
            drained = all(c.queue.empty() for c in self.service.connections)
            if caught_up and drained:
                break
            if loop.time() > deadline:
                raise FixtureFailure("the service did not catch up")
            await asyncio.sleep(0.001)
        for _ in range(5):
            await asyncio.sleep(0)

    async def recv(self, conn: str, timeout: float) -> str:
        text = await asyncio.wait_for(self.conns[conn].recv(), timeout)
        self.raw.setdefault(conn, []).append(text)
        return text

    async def run(self) -> None:
        server = await transport.start(self.service, "127.0.0.1", 0)
        port = server.sockets[0].getsockname()[1]
        self.uri = f"ws://127.0.0.1:{port}/"
        try:
            for index, step in enumerate(self.fixture["steps"]):
                try:
                    await self.step(step)
                except FixtureFailure as exc:
                    raise FixtureFailure(f"{self.name} step {index} {json.dumps(step)[:160]}: {exc}") from None
        finally:
            for ws in self.conns.values():
                try:
                    await ws.close()
                except Exception:  # noqa: BLE001
                    pass
            await transport.stop([server], self.service, grace_s=0, timeout_s=5)

    async def step(self, step: Dict[str, Any]) -> None:
        if "connect" in step:
            if set(step) - {"connect", "address"}:
                raise FixtureFailure("unknown keys in a connect step")
            name = step["connect"]
            role_of(name)
            self.conns[name] = await ws_connect(self.uri, step.get("address", DEFAULT_ADDRESS))
            return
        if "advanceMs" in step:
            await self.quiesce()
            self.clock.advance(int(step["advanceMs"]))
            for _ in range(5):
                await asyncio.sleep(0)
            return
        if "disconnect" in step:
            await self.conns[step["disconnect"]].close()
            self.disconnects += 1
            await self.quiesce()
            return
        if "expectClosed" in step:
            if set(step) != {"expectClosed", "code"}:
                raise FixtureFailure("an expectClosed step has expectClosed and code")
            conn = step["expectClosed"]
            try:
                text = await self.recv(conn, RECV_TIMEOUT_S)
            except ConnectionClosed as exc:
                # rcvd is the close frame the service sent.
                got = exc.rcvd.code if exc.rcvd is not None else None
                if got != step["code"]:
                    raise FixtureFailure(f"{conn} was closed with {got}, not {step['code']}") from None
                return
            except asyncio.TimeoutError:
                raise FixtureFailure(f"{conn} was not closed") from None
            raise FixtureFailure(f"{conn} received {text[:120]} instead of the close")
        if "expectSilent" in step:
            conn = step["expectSilent"]
            await self.quiesce()
            try:
                text = await self.recv(conn, SILENCE_S)
            except (asyncio.TimeoutError, ConnectionClosed):
                # A closed connection receives nothing either.
                return
            raise FixtureFailure(f"{conn} received {text[:120]}")
        sender = step.get("from")
        if sender == "server":
            if set(step) != {"from", "to", "message"}:
                raise FixtureFailure("a server step has from, to and message")
            conn = step["to"]
            try:
                text = await self.recv(conn, RECV_TIMEOUT_S)
            except asyncio.TimeoutError:
                raise FixtureFailure(f"{conn} received nothing") from None
            except ConnectionClosed:
                raise FixtureFailure(f"{conn} was closed") from None
            actual = json.loads(text)
            try:
                decoded = protocol.decode(actual, "server", role_of(conn))
            except protocol.DecodeError as exc:
                raise FixtureFailure(f"the service sent a message its peer cannot decode: {exc}") from None
            if decoded != actual:
                raise FixtureFailure("the service sent keys its peer does not know")
            self.match(step["message"], actual, "message")
            return
        if sender is not None:
            if set(step) != {"from", "role", "message"} or step["role"] not in ("behaviour", "scripted"):
                raise FixtureFailure("a connection step has from, role (behaviour or scripted) and message")
            role_of(sender)
            filled = self.fill(step["message"])
            try:
                await self.conns[sender].send(protocol.encode(filled))
            except ConnectionClosed:
                raise FixtureFailure(f"{sender} was closed before it could send") from None
            self.sent += 1
            return
        raise FixtureFailure("unknown step")


def run_fixture(fixture: Dict[str, Any], name: str) -> Runner:
    runner = Runner(fixture, name)
    asyncio.run(runner.run())
    return runner
