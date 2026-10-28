# no-port-check: NereusSDR-original.
"""The server's conformance runner (runs value "service").

It builds a real server from a session fixture's setup, listens on
loopback with a ManualClock, plays every connection itself over real
WebSockets, fills the connections' messages and matches every message the
server sends. The fixture format and the placeholder grammar are the wire
spec's section 11.4.
"""

from __future__ import annotations

import asyncio
import json
import os
import re
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from cryptography.hazmat.primitives.asymmetric import ec

from nereus_roundtable import identity, protocol, transport
from nereus_roundtable.clock import DEFAULT_WALL_START_MS, ManualClock
from nereus_roundtable.config import LIMIT_FIELDS, Config
from nereus_roundtable.service import Service
from nereus_roundtable.store import Store

try:  # websockets 13 and later
    from websockets.asyncio.client import connect as _ws_connect

    _HEADERS_KW = "additional_headers"
except ImportError:  # websockets 10.x: the legacy client
    from websockets.legacy.client import connect as _ws_connect  # type: ignore[no-redef]

    _HEADERS_KW = "extra_headers"

from websockets.exceptions import ConnectionClosed

CONFORMANCE = Path(__file__).resolve().parent.parent / "conformance" / "v1"
DEFAULT_ADDRESS = "192.0.2.1"
DEFAULT_HOST = "roundtable.conformance.invalid"
OTHER_HOST = "roundtable.example.invalid"
RECV_TIMEOUT_S = 5.0
SILENCE_S = 0.1


def _camel(name: str) -> str:
    head, *rest = name.split("_")
    return head + "".join(part.capitalize() for part in rest)


# setup.limits keys: the camelCase of every [limits] value.
LIMIT_OVERRIDES = {_camel(name): name for name in LIMIT_FIELDS}

_PLACEHOLDER = re.compile(r"\A\$(any|string|int|capture|ref|b64|key|hello|websiteKey|rowRef|weblink)(?::(.*))?\Z")
_ROW_REF = re.compile(r"\A[a-z2-7]{8}\Z")


class FixtureFailure(AssertionError):
    pass


def load_manifest() -> Dict[str, Any]:
    return json.loads((CONFORMANCE / "manifest.json").read_text(encoding="utf-8"))


def load_fixture(path: str) -> Dict[str, Any]:
    return json.loads((CONFORMANCE / path).read_text(encoding="utf-8"))


def path_of(conn: str) -> str:
    """station, station2, ... are on /v1/station; live, live2, ... on
    /v1/live (wire spec 11.4)."""
    if re.match(r"\Astation[0-9]*\Z", conn):
        return transport.STATION_PATH
    if re.match(r"\Alive[0-9]*\Z", conn):
        return transport.LIVE_PATH
    raise FixtureFailure(f"connection {conn!r} is neither a station nor a live reader")


def role_of(conn: str) -> str:
    return "station" if path_of(conn) == transport.STATION_PATH else "live"


async def ws_connect(uri: str, address: str) -> Any:
    kwargs = {
        _HEADERS_KW: {"X-Forwarded-For": address},
        "max_size": None,
        "ping_interval": None,
        "compression": None,
    }
    return await _ws_connect(uri, **kwargs)


def make_config(setup: Dict[str, Any]) -> Config:
    """A server on loopback port 0 with no pings, from a fixture's setup."""
    config = Config()
    config.ws_listen = ["127.0.0.1:0"]
    config.http_listen = ["127.0.0.1:0"]
    config.public_host = setup.get("host", DEFAULT_HOST)
    config.ping_interval_ms = 0
    for key, value in setup.get("limits", {}).items():
        if key not in LIMIT_OVERRIDES:
            raise FixtureFailure(f"unknown setup.limits key {key}")
        setattr(config, LIMIT_OVERRIDES[key], value)
    return config


class Runner:
    def __init__(self, fixture: Dict[str, Any], name: str) -> None:
        self.fixture = fixture
        self.name = name
        extra = set(fixture) - {"runs", "setup", "steps"}
        if extra:
            raise FixtureFailure(f"{name}: unknown top-level keys {sorted(extra)}")
        self.setup = fixture.get("setup", {})
        extra = set(self.setup) - {"host", "wallClock", "keys", "limits"}
        if extra:
            raise FixtureFailure(f"{name}: unknown setup keys {sorted(extra)}")
        self.host = self.setup.get("host", DEFAULT_HOST)
        self.clock = ManualClock(self.setup.get("wallClock", DEFAULT_WALL_START_MS))
        self.config = make_config(self.setup)
        self.store = Store(":memory:")
        self.service = Service(self.config, self.clock, self.store)
        self.conns: Dict[str, Any] = {}
        self.keys: Dict[str, ec.EllipticCurvePrivateKey] = {}
        self.website_keys: Dict[str, bytes] = {}
        self.records: Dict[str, Any] = {}
        self.counter = 0
        self.sent = 0
        self.disconnects = 0
        self.uri = ""
        self.raw: Dict[str, List[str]] = {}
        self._issue_keys()

    # ------------------------------------------------------------- keys

    def _issue_keys(self) -> None:
        now = self.clock.wall_ms()
        for entry in self.setup.get("keys", []):
            extra = set(entry) - {"name", "callsign", "grid", "revoked"}
            if extra or "name" not in entry:
                raise FixtureFailure(f"a setup key is {{name, callsign, grid, revoked}}, not {sorted(entry)}")
            raw = identity.new_key()
            self.website_keys[entry["name"]] = raw
            station = identity.key_id(raw)
            self.store.station_upsert(station, "key", entry.get("callsign"), entry.get("grid"), now)
            self.store.key_add(identity.key_hash(raw), station, now)
            if entry.get("revoked", False):
                self.store.key_revoke(identity.key_hash(raw), now)

    def key(self, name: str) -> ec.EllipticCurvePrivateKey:
        if name not in self.keys:
            self.keys[name] = ec.generate_private_key(ec.SECP256R1())
        return self.keys[name]

    def spki(self, name: str) -> bytes:
        return identity.spki_of(self.key(name).public_key())

    def station_id(self, name: str) -> str:
        return identity.roundtable_id(self.spki(name))

    def website_key(self, name: str, template: str) -> bytes:
        if name not in self.website_keys:
            raise FixtureFailure(f"{template}: setup.keys has no key {name}")
        return self.website_keys[name]

    def _nonce(self, name: str, template: str) -> bytes:
        if name not in self.records:
            raise FixtureFailure(f"{template}: no nonce recorded under {name}")
        data = identity.b64url_of_length(self.records[name], identity.NONCE_BYTES)
        if data is None:
            raise FixtureFailure(f"{template}: {name} is not a nonce")
        return data

    def hello_signature(self, parts: List[str], template: str) -> bytes:
        if len(parts) != 3:
            raise FixtureFailure(f"{template}: write $hello:<key>:<nonce>:<case>")
        k, nonce_name, case = parts
        nonce = self._nonce(nonce_name, template)
        key = self.key(k)
        if case == "signed":
            return identity.sign_raw(key, identity.hello_transcript(self.host, nonce))
        if case == "otherNonce":
            return identity.sign_raw(key, identity.hello_transcript(self.host, os.urandom(identity.NONCE_BYTES)))
        if case == "otherHost":
            return identity.sign_raw(key, identity.hello_transcript(OTHER_HOST, nonce))
        if case == "rendezvousPrefix":
            return identity.sign_raw(key, identity.RENDEZVOUS_REGISTER_PREFIX + nonce)
        if case == "flippedBit":
            flipped = bytearray(identity.sign_raw(key, identity.hello_transcript(self.host, nonce)))
            flipped[-1] ^= 1
            return bytes(flipped)
        raise FixtureFailure(f"{template}: unknown case {case}")

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
            self.counter += 1
            if parts:
                self.records[parts[0]] = self.counter
            return self.counter
        if kind == "ref":
            if arg not in self.records:
                raise FixtureFailure(f"{template}: nothing recorded under {arg}")
            return self.records[arg]
        if kind == "b64":
            value = identity.to_b64url(os.urandom(int(parts[0])))
            if len(parts) > 1:
                self.records[parts[1]] = value
            return value
        if kind == "key":
            if len(parts) != 2 or parts[1] not in ("spki", "id"):
                raise FixtureFailure(f"{template}: write $key:<key>:spki or :id")
            return self.station_id(parts[0]) if parts[1] == "id" else identity.to_b64url(self.spki(parts[0]))
        if kind == "hello":
            return identity.to_b64url(self.hello_signature(parts, template))
        if kind == "websiteKey":
            raw = self.website_key(parts[0], template)
            if len(parts) > 1:
                if parts[1] != "id":
                    raise FixtureFailure(f"{template}: write $websiteKey:<key> or :id")
                return identity.key_id(raw)
            return identity.to_b64url(raw)
        raise FixtureFailure(f"{template} cannot be filled in a connection's message")

    # ------------------------------------------------------------- match

    def match(self, template: Any, actual: Any, path: str) -> None:
        if isinstance(template, str):
            m = _PLACEHOLDER.match(template)
            if m:
                self._match_placeholder(m.group(1), m.group(2), template, actual, path)
                return
        if isinstance(template, dict):
            if not isinstance(actual, dict) or set(template) != set(actual):
                got = sorted(actual) if isinstance(actual, dict) else actual
                raise FixtureFailure(f"{path}: keys {got!r} are not {sorted(template)}")
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
            if len(parts) > 1:
                self.records[parts[1]] = actual
            return
        if kind == "key":
            want = self.station_id(parts[0]) if parts[1] == "id" else identity.to_b64url(self.spki(parts[0]))
            need(actual == want, f"key {parts[0]}'s {parts[1]}")
            return
        if kind == "hello":
            k, nonce_name, case = parts
            need(case == "signed", "matched only as signed")
            sig = identity.from_b64url(actual)
            nonce = self._nonce(nonce_name, template)
            need(identity.verify_hello(self.spki(k), sig, self.host, nonce), "a signature that verifies")
            return
        if kind == "websiteKey":
            raw = self.website_key(parts[0], template)
            want = identity.key_id(raw) if len(parts) > 1 else identity.to_b64url(raw)
            need(actual == want, f"website key {parts[0]}'s {'id' if len(parts) > 1 else 'text'}")
            return
        if kind == "rowRef":
            need(isinstance(actual, str) and _ROW_REF.match(actual) is not None, "a row ref")
            if parts:
                self.records[parts[0]] = actual
            return
        if kind == "weblink":
            prefix = f"https://{self.host}/signin?t="
            ok = isinstance(actual, str) and actual.startswith(prefix)
            need(ok and identity.b64url_of_length(actual[len(prefix) :], 32) is not None, "a sign-in link")
            if parts:
                self.records[parts[0]] = actual
            return
        raise FixtureFailure(f"{path}: unknown placeholder {template}")

    # ------------------------------------------------------------- run

    async def quiesce(self) -> None:
        """Wait until the server has read every frame sent and seen every
        close, and every outbound queue is empty."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + RECV_TIMEOUT_S
        while True:
            caught_up = self.service.frames_handled >= self.sent and self.service.peer_closes >= self.disconnects
            drained = all(c.queue.empty() for c in self.service.connections)
            if caught_up and drained:
                break
            if loop.time() > deadline:
                raise FixtureFailure("the server did not catch up")
            await asyncio.sleep(0.001)
        for _ in range(5):
            await asyncio.sleep(0)

    async def recv(self, conn: str, timeout: float) -> str:
        if conn not in self.conns:
            raise FixtureFailure(f"{conn} was never connected")
        text = await asyncio.wait_for(self.conns[conn].recv(), timeout)
        self.raw.setdefault(conn, []).append(text)
        return text

    async def run(self) -> None:
        listeners = await transport.start(self.service, None, self.config)
        self.uri = f"ws://127.0.0.1:{listeners.ws_port()}"
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
            await transport.stop(listeners, self.service, grace_s=0, timeout_s=5)
            self.store.close()

    async def step(self, step: Dict[str, Any]) -> None:
        if "connect" in step:
            if set(step) - {"connect", "address"}:
                raise FixtureFailure("unknown keys in a connect step")
            name = step["connect"]
            self.conns[name] = await ws_connect(self.uri + path_of(name), step.get("address", DEFAULT_ADDRESS))
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
                # rcvd is the close frame the server sent.
                got = exc.rcvd.code if exc.rcvd is not None else None
                if got != step["code"]:
                    raise FixtureFailure(f"{conn} was closed with {got}, not {step['code']}") from None
                return
            except asyncio.TimeoutError:
                raise FixtureFailure(f"{conn} was not closed") from None
            raise FixtureFailure(f"{conn} received {text[:120]} instead of the close")
        if "expectSilent" in step:
            if set(step) != {"expectSilent"}:
                raise FixtureFailure("an expectSilent step has expectSilent only")
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
            except ConnectionClosed as exc:
                code = exc.rcvd.code if exc.rcvd is not None else None
                raise FixtureFailure(f"{conn} was closed with {code}") from None
            if len(text.encode("utf-8")) > protocol.MAX_OUTBOUND_BYTES:
                raise FixtureFailure("the server sent a message over 1 MiB")
            try:
                decoded = protocol.decode(text, "server", role_of(conn))
            except protocol.ProtocolError as exc:
                raise FixtureFailure(f"the server sent a message its peer cannot decode: {exc}") from None
            actual = json.loads(text)
            if decoded != actual:
                raise FixtureFailure("the server sent keys its peer does not know")
            self.match(step["message"], actual, "message")
            return
        if sender is not None:
            if set(step) != {"from", "role", "message"} or step["role"] not in ("behaviour", "scripted"):
                raise FixtureFailure("a connection step has from, role (behaviour or scripted) and message")
            path_of(sender)
            filled = self.fill(step["message"])
            try:
                await self.conns[sender].send(protocol.encode(filled))
            except ConnectionClosed:
                raise FixtureFailure(f"{sender} was closed before it could send") from None
            self.sent += 1
            return
        raise FixtureFailure("unknown step")


def run_fixture(fixture: Union[str, Dict[str, Any]], name: Optional[str] = None) -> Runner:
    """Run one session fixture: a path under conformance/v1 (such as
    "sessions/signin-spki.json") or the fixture itself."""
    if isinstance(fixture, str):
        name = name or Path(fixture).stem
        fixture = load_fixture(fixture)
    runner = Runner(fixture, name or "fixture")
    asyncio.run(runner.run())
    return runner
