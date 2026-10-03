# no-port-check: NereusSDR-original.
"""The relay's conformance runner (runs value "relay"): the frame-protocol
fixtures of rendezvous/conformance/v1/relay/ (rendezvous document section
12.8) against the real relay, every leg played here over real WebSockets
on loopback, the relay on a manual clock.
"""

from __future__ import annotations

import asyncio
import json
import os
import re
from pathlib import Path
from typing import Any, Dict, List

from websockets.exceptions import ConnectionClosed

from nereus_rendezvous import relaygrant
from nereus_rendezvous.clock import ManualClock
from nereus_relay import transport as relay_transport
from nereus_relay.relay import Relay
from relay_helpers import SECRET, connect, make_relay_config, short_directory

RELAY_FIXTURES = Path(__file__).resolve().parent.parent / "conformance" / "v1" / "relay"
DEFAULT_ADDRESS = "192.0.2.1"
DEFAULT_WALL_CLOCK = 1800000000
DEFAULT_GRANT_TTL = 120
RECV_TIMEOUT_S = 5.0
SILENCE_S = 0.1

_SETUP_KEYS = {
    "slots": "slots",
    "sessionsPerStation": "sessions_per_station",
    "connectionsPerAddress": "connections_per_address",
    "maxPending": "max_pending",
    "joinTimeoutMs": "join_timeout_ms",
    "rejoinMs": "rejoin_ms",
    "idleTimeoutMs": "idle_timeout_ms",
}
_LEGS = {"core": relaygrant.LEG_CORE, "device": relaygrant.LEG_DEVICE}
_HEX = re.compile(r"\A(?:[0-9a-f]{2})+\Z")


class RelayFixtureFailure(AssertionError):
    pass


def load_manifest() -> Dict[str, Any]:
    return json.loads((RELAY_FIXTURES / "manifest.json").read_text(encoding="utf-8"))


def load_fixture(name: str) -> Dict[str, Any]:
    return json.loads((RELAY_FIXTURES / name).read_text(encoding="utf-8"))


def leg_of(conn: str) -> int:
    for name, leg in _LEGS.items():
        if conn.startswith(name):
            return leg
    raise RelayFixtureFailure(f"connection {conn!r} is neither core nor device")


class RelayRunner:
    def __init__(self, fixture: Dict[str, Any], name: str) -> None:
        extra = set(fixture) - {"runs", "relaySetup", "steps"}
        if extra:
            raise RelayFixtureFailure(f"{name}: unknown top-level keys {sorted(extra)}")
        self.fixture = fixture
        self.name = name
        setup = dict(fixture.get("relaySetup", {}))
        self.clock = ManualClock(setup.pop("wallClock", DEFAULT_WALL_CLOCK))
        self.ttl = setup.pop("grantTtlSeconds", DEFAULT_GRANT_TTL)
        overrides = {}
        for key, value in setup.items():
            if key not in _SETUP_KEYS:
                raise RelayFixtureFailure(f"unknown relaySetup key {key}")
            overrides[_SETUP_KEYS[key]] = value
        self.relay = Relay(make_relay_config(**overrides), self.clock)
        self.conns: Dict[str, Any] = {}
        self.records: Dict[str, bytes] = {}
        # Section 12.8: a token placeholder fills to the same token every
        # time its key (<leg>:<session>:<station>[:case]) comes back, so a
        # leg joins again with the very token it was given.
        self.tokens: Dict[str, bytes] = {}
        self.sent = 0

    # ------------------------------------------------------------- bytes

    def _token(self, parts: List[str], template: str) -> bytes:
        key = ":".join(parts)
        if key not in self.tokens:
            self.tokens[key] = self._mint(parts, template)
        return self.tokens[key]

    def _mint(self, parts: List[str], template: str) -> bytes:
        if len(parts) not in (3, 4) or parts[0] not in _LEGS:
            raise RelayFixtureFailure(f"{template}: write $token:<core|device>:<session>:<station>[:expired|forged]")
        session = self.records.setdefault("session:" + parts[1], os.urandom(relaygrant.SESSION_BYTES))
        station = relaygrant.station_of(SECRET, (parts[2] * 26)[:26])
        case = parts[3] if len(parts) == 4 else "valid"
        if case not in ("valid", "expired", "forged"):
            raise RelayFixtureFailure(f"{template}: unknown case {case}")
        expires = self.clock.wall_seconds() + self.ttl
        if case == "expired":
            expires = self.clock.wall_seconds() - 1
        token = relaygrant.mint(SECRET, _LEGS[parts[0]], session, station, expires)
        if case == "forged":
            raw = bytearray(relaygrant.from_b64url(token))
            raw[-1] ^= 1
            token = relaygrant.to_b64url(bytes(raw))
        return token.encode("ascii")

    def fill(self, parts: List[str]) -> bytes:
        out = b""
        for part in parts:
            if _HEX.match(part):
                out += bytes.fromhex(part)
            elif part.startswith("$token:"):
                out += self._token(part.split(":")[1:], part)
            elif part.startswith("$bytes:"):
                _, n, name = part.split(":")
                value = os.urandom(int(n))
                self.records[name] = value
                out += value
            elif part.startswith("$ref:"):
                out += self.records[part[5:]]
            else:
                raise RelayFixtureFailure(f"unknown part {part!r}")
        return out

    def match(self, parts: List[str], actual: Any) -> None:
        if not isinstance(actual, bytes):
            raise RelayFixtureFailure(f"received text {actual[:60]!r}, not a binary message")
        at = 0
        for part in parts:
            if part.startswith("$bytes:"):
                _, n, name = part.split(":")
                piece = actual[at : at + int(n)]
                if len(piece) != int(n):
                    raise RelayFixtureFailure(f"{actual[:16].hex()}...: shorter than {part}")
                self.records[name] = piece
                at += int(n)
                continue
            want = self.fill([part])
            if actual[at : at + len(want)] != want:
                raise RelayFixtureFailure(f"at byte {at}: {actual[at:at + 16].hex()} is not {want[:16].hex()} ({part})")
            at += len(want)
        if at != len(actual):
            raise RelayFixtureFailure(f"{len(actual) - at} bytes more than {parts}")

    # ------------------------------------------------------------- run


    async def quiesce(self) -> None:
        loop = asyncio.get_running_loop()
        deadline = loop.time() + RECV_TIMEOUT_S
        while True:
            caught_up = self.relay.frames_handled >= self.sent
            drained = all(
                not leg.control and (leg.session is None or not leg.session.queued(leg.side)) for leg in self.relay.legs
            )
            if caught_up and drained:
                break
            if loop.time() > deadline:
                raise RelayFixtureFailure("the relay did not catch up")
            await asyncio.sleep(0.001)
        for _ in range(5):
            await asyncio.sleep(0)

    async def run(self) -> None:
        directory = short_directory()
        self.relay.config.socket = os.path.join(directory, "relay.sock")
        server = await relay_transport.start(self.relay)
        self.uri = "unix:" + self.relay.config.socket
        try:
            for index, step in enumerate(self.fixture["steps"]):
                try:
                    await self.step(step)
                except RelayFixtureFailure as exc:
                    raise RelayFixtureFailure(f"{self.name} step {index} {json.dumps(step)[:120]}: {exc}") from None
        finally:
            for ws in self.conns.values():
                try:
                    await ws.close()
                except Exception:  # noqa: BLE001
                    pass
            await relay_transport.stop([server], self.relay, grace_s=0, timeout_s=5)
            os.rmdir(directory)

    async def step(self, step: Dict[str, Any]) -> None:
        if "connect" in step:
            leg_of(step["connect"])
            before = self.relay.accepted
            self.conns[step["connect"]] = await connect(self.uri, step.get("address", DEFAULT_ADDRESS))
            loop = asyncio.get_running_loop()
            deadline = loop.time() + RECV_TIMEOUT_S
            while self.relay.accepted == before:
                if loop.time() > deadline:
                    raise RelayFixtureFailure("the relay did not take the connection")
                await asyncio.sleep(0.001)
            return
        if "advanceMs" in step:
            await self.quiesce()
            self.clock.advance(int(step["advanceMs"]))
            for _ in range(5):
                await asyncio.sleep(0)
            return
        if "disconnect" in step or "drop" in step:
            # disconnect: the leg closes its connection. drop: the
            # connection breaks with no close at all (a network break); the
            # relay sees both the same way, and a leg must join again after
            # a drop (section 12.3).
            conn = step.get("disconnect") or step["drop"]
            before = self.relay.finished
            if "drop" in step:
                transport = getattr(self.conns[conn], "transport", None)
                if transport is not None:
                    transport.abort()
            else:
                await self.conns[conn].close()
            loop = asyncio.get_running_loop()
            deadline = loop.time() + RECV_TIMEOUT_S
            while self.relay.finished == before:
                if loop.time() > deadline:
                    raise RelayFixtureFailure("the relay did not see the close")
                await asyncio.sleep(0.001)
            return
        if "shutdown" in step:
            await self.quiesce()
            self.relay.shutdown()
            return
        if "expectClosed" in step:
            conn = step["expectClosed"]
            try:
                frame = await asyncio.wait_for(self.conns[conn].recv(), RECV_TIMEOUT_S)
            except ConnectionClosed as exc:
                got = exc.rcvd.code if exc.rcvd is not None else None
                if got != step["code"]:
                    raise RelayFixtureFailure(f"{conn} was closed with {got}, not {step['code']}") from None
                return
            except asyncio.TimeoutError:
                raise RelayFixtureFailure(f"{conn} was not closed") from None
            raise RelayFixtureFailure(f"{conn} received {frame!r:.60} instead of the close")
        if "expectSilent" in step:
            conn = step["expectSilent"]
            await self.quiesce()
            try:
                frame = await asyncio.wait_for(self.conns[conn].recv(), SILENCE_S)
            except (asyncio.TimeoutError, ConnectionClosed):
                return
            raise RelayFixtureFailure(f"{conn} received {frame!r:.60}")
        if "to" in step:
            if set(step) != {"to", "binary"}:
                raise RelayFixtureFailure("a relay step has to and binary")
            conn = step["to"]
            try:
                frame = await asyncio.wait_for(self.conns[conn].recv(), RECV_TIMEOUT_S)
            except asyncio.TimeoutError:
                raise RelayFixtureFailure(f"{conn} received nothing") from None
            except ConnectionClosed:
                raise RelayFixtureFailure(f"{conn} was closed") from None
            self.match(step["binary"], frame)
            return
        if "from" in step:
            if step.get("role") not in ("behaviour", "scripted") or not ({"binary", "text"} & set(step)):
                raise RelayFixtureFailure("a leg step has from, role and binary or text")
            conn = step["from"]
            leg_of(conn)
            message: Any = step["text"] if "text" in step else self.fill(step["binary"])
            try:
                await self.conns[conn].send(message)
            except ConnectionClosed:
                raise RelayFixtureFailure(f"{conn} was closed before it could send") from None
            self.sent += 1
            return
        raise RelayFixtureFailure("unknown step")


def run_relay_fixture(fixture: Dict[str, Any], name: str) -> RelayRunner:
    runner = RelayRunner(fixture, name)
    asyncio.run(runner.run())
    return runner
