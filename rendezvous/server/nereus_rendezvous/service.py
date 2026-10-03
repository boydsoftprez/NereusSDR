# no-port-check: NereusSDR-original.
"""The rendezvous service: registrations, introductions, relay credentials,
nameplates and pairing mailboxes.

Everything here lives in memory. Nothing about a station, a device, an
introduction or a mailbox is ever written to disk (rendezvous document
section 9); a restart forgets it all and stations register again.

The service is transport-agnostic: a connection is anything with async
send(text), close(code) and async iteration over incoming frames. All state
changes happen synchronously in the reading task of the connection whose
message caused them, so the order of what the service sends follows the
order of what it read. Each connection has its own outbound queue and
writer task, so one slow peer never holds up another's handler.
"""

from __future__ import annotations

import asyncio
import contextlib
import heapq
import logging
import os
from typing import Any, Callable, Dict, Iterator, List, Optional, Set

from . import identity, protocol, relaygrant, turn
from .clock import RealClock, TimerHandle
from .config import Config
from .config import check as check_config
from .limits import WindowLimiter, address_group, first_allowed

log = logging.getLogger("nereus_rendezvous")

MINUTE_MS = 60000
CLOSE_NORMAL = 1000
CLOSE_GOING_AWAY = 1001
# A peer that stops reading is closed with 1008 (policy violation) and no
# error message, since it would not read one (rendezvous document section 2).
CLOSE_NOT_READING = 1008


def short(text: str) -> str:
    """Ids in logs: a few characters, never the whole value."""
    return text[:6]


class _Close:
    def __init__(self, code: int) -> None:
        self.code = code


class Introduction:
    def __init__(self, iid: str, station_id: str, client: "Connection", station: "Connection", watch_relay_version: int = 0) -> None:
        self.iid = iid
        self.station_id = station_id
        self.client = client
        self.station = station
        self.watch_relay_version = watch_relay_version
        self.answered = False
        self.client_candidates = 0
        self.station_candidates = 0
        self.timer: Optional[TimerHandle] = None


class Mailbox:
    def __init__(self, nameplate: int, station: "Connection", client: "Connection") -> None:
        self.nameplate = nameplate
        self.station = station
        self.client = client
        self.station_messages = 0
        self.client_messages = 0
        self.timer: Optional[TimerHandle] = None


class Connection:
    def __init__(self, service: "Service", transport: Any, address: str) -> None:
        self.service = service
        self.transport = transport
        self.group = address_group(address)
        self.role: Optional[str] = None
        self.nonce = service.random(identity.NONCE_BYTES)
        self.queue: "asyncio.Queue[Any]" = asyncio.Queue()
        self.queued_bytes = 0
        # The message the writer is sending now, and when a send takes too
        # long, the timer that ends the connection (section 9.1).
        self.inflight = 0
        self.stall_timer: Optional[TimerHandle] = None
        self.aborted = False
        self.writer: Optional["asyncio.Future[None]"] = None
        self.close_task: Optional["asyncio.Future[None]"] = None
        self.closing = False
        self.detached = False
        # Which pool the connection is counted in (section 9.1): every
        # connection that is not a registered station, or the stations.
        self.counted = False
        self.station_counted = False
        self.timer: Optional[TimerHandle] = None
        # Station role.
        self.challenge: Optional[bytes] = None
        self.pending_spki: Optional[bytes] = None
        self.pending_id: Optional[str] = None
        self.station_id: Optional[str] = None
        self.watch_relay_version = 0
        self.introductions: Dict[str, Introduction] = {}
        self.nameplate: Optional[int] = None
        self.mailbox: Optional[Mailbox] = None
        # Client role.
        self.introduction: Optional[Introduction] = None

    @property
    def receiver(self) -> str:
        return self.role or "client"

    def send(self, message: Dict[str, Any]) -> None:
        if self.closing:
            return
        with self.service.holding_cleanup():
            self._send(message)

    def _send(self, message: Dict[str, Any]) -> None:
        text = protocol.encode(message)
        size = len(text.encode("utf-8"))
        config = self.service.config
        if self.queue.qsize() >= config.send_queue_messages or self.queued_bytes + size > config.send_queue_bytes:
            self.stopped_reading()
            return
        # The budget for every queue together: close whoever holds the most
        # until this message fits, which is this connection only when it is
        # itself the largest holder (rendezvous document section 9.1).
        while self.service.queued_bytes + size > config.send_budget_bytes:
            hog = self.service.largest_holder()
            if hog is None or hog is self:
                self.stopped_reading()
                return
            hog.stopped_reading()
        self._hold(size)
        self.queue.put_nowait((text, size))

    def _hold(self, size: int) -> None:
        self.queued_bytes += size
        self.service.queued_bytes += size
        self.service.holders.add(self)

    def _release(self, size: int) -> None:
        self.queued_bytes -= size
        self.service.queued_bytes -= size
        if self.queued_bytes <= 0:
            self.service.holders.discard(self)

    def release_all(self) -> None:
        """Forget every byte this connection holds: what waits in its queue
        and the message in flight. The one place a queue is emptied, called
        whenever the writer ends, for any reason, and when the connection is
        dropped, so no path leaves bytes counted against the budget."""
        while not self.queue.empty():
            item = self.queue.get_nowait()
            if isinstance(item, tuple):
                self._release(item[1])
        if self.inflight:
            self._release(self.inflight)
            self.inflight = 0
        if self.stall_timer is not None:
            self.stall_timer.cancel()
            self.stall_timer = None

    def stopped_reading(self) -> None:
        """A peer whose messages pile up (its own cap, the largest share of
        the service's budget, or one send blocked too long) is dropped rather
        than buffered without bound: what is queued is discarded, the writer
        stops, and the connection closes with 1008 and no error message."""
        if self.aborted:
            return
        self.aborted = True
        log.warning("closed a connection that stopped reading")
        self.closing = True
        self.release_all()
        if self.writer is not None:
            self.writer.cancel()
        self.close_task = asyncio.ensure_future(self._close_quietly(CLOSE_NOT_READING))
        # Its peers hear of its leaving only after the sends under way are
        # done, so a message it sent reaches the other end before the
        # cleanup its leaving causes (Service.detach_later).
        self.service.detach_later(self)

    async def _close_quietly(self, code: int) -> None:
        try:
            await self.transport.close(code)
        except Exception:  # noqa: BLE001 - the peer is gone either way
            return

    def fail(self, code: str, retry_after_ms: Optional[int] = None, close_code: int = CLOSE_NORMAL) -> None:
        """Send an error. An error that closes is followed by the close."""
        if self.closing:
            return
        self.send(protocol.error_message(code, self.receiver, retry_after_ms))
        if protocol.error_closes(code):
            log.info("closed a %s connection: %s", self.role or "new", code)
            self.closing = True
            self.queue.put_nowait(_Close(close_code))
            self.service.detach(self)
        else:
            log.info("refused a %s request: %s", self.role or "new", code)

    def cancel_timer(self) -> None:
        if self.timer is not None:
            self.timer.cancel()
            self.timer = None

    async def write_loop(self) -> None:
        try:
            while True:
                item = await self.queue.get()
                if isinstance(item, _Close):
                    await self.transport.close(item.code)
                    return
                text, size = item
                self.inflight = size
                self.stall_timer = self.service.clock.call_later(
                    self.service.config.send_stall_ms, self.stopped_reading
                )
                await self.transport.send(text)
                self.stall_timer.cancel()
                self.stall_timer = None
                self.inflight = 0
                self._release(size)
        except Exception:  # noqa: BLE001 - the peer went away; the reader cleans up
            return
        finally:
            # Cancelled, failed or closed: nothing this connection queued
            # stays counted.
            self.release_all()


class Service:
    def __init__(
        self,
        config: Config,
        clock: Any = None,
        random: Callable[[int], bytes] = os.urandom,
    ) -> None:
        self.config = config
        self.clock = clock or RealClock()
        self.random = random
        self.connections: Set[Connection] = set()
        # Section 9.1's two pools: connections that are not registered
        # stations (clients, and connections still starting), and
        # registered stations, each counted in total and per address group.
        self.unregistered = 0
        self.per_group: Dict[str, int] = {}
        self.station_count = 0
        self.stations_per_group: Dict[str, int] = {}
        # Bytes waiting in every connection's outbound queue together, and
        # the connections holding any.
        self.queued_bytes = 0
        self.holders: Set[Connection] = set()
        self.stations: Dict[str, Connection] = {}
        self.introductions: Dict[str, Introduction] = {}
        self.nameplates: Dict[int, Connection] = {}
        self._freed: List[int] = []
        self._next_nameplate = protocol.NAMEPLATE_MIN
        self.intro_by_address = WindowLimiter(config.introductions_per_address_per_minute, MINUTE_MS)
        self.intro_by_station = WindowLimiter(config.introductions_per_station_per_minute, MINUTE_MS)
        self.mailbox_by_address = WindowLimiter(config.mailbox_opens_per_address_per_minute, MINUTE_MS)
        self._last_sweep_ms = 0
        # Counters the conformance runner reads to know the service has
        # caught up with what it sent; nothing else uses them.
        self.frames_handled = 0
        self.peer_closes = 0
        # Connections dropped for not reading while a handler or a send is
        # under way, whose cleanup waits until it is done (detach_later).
        self._cleanup_depth = 0
        self._deferred: List[Connection] = []
        check_config(config)

    # ----------------------------------------------------------- lifecycle

    async def run_connection(self, transport: Any, address: str) -> None:
        conn = Connection(self, transport, address)
        writer = asyncio.ensure_future(conn.write_loop())
        conn.writer = writer
        self.accept(conn)
        try:
            async for frame in transport:
                if not conn.closing:
                    try:
                        self.on_frame(conn, frame)
                    except Exception as exc:  # noqa: BLE001
                        # A bug, not the peer's doing. The class name only:
                        # an exception's text can hold what the peer sent.
                        log.error("a message handler failed (%s); closing its connection", type(exc).__name__)
                        self.frames_handled += 1
                        break
                self.frames_handled += 1
        except Exception as exc:  # noqa: BLE001 - any transport end is an end
            log.debug("a connection ended: %s", type(exc).__name__)
        finally:
            if not conn.closing:
                self.peer_closes += 1
            self.detach(conn)
            if conn.closing and not writer.done():
                await asyncio.wait({writer}, timeout=5)
            if not writer.done():
                writer.cancel()
                await asyncio.wait({writer})
            conn.release_all()

    def largest_holder(self) -> Optional[Connection]:
        """The connection holding the most queued bytes, among those not
        already being dropped."""
        best: Optional[Connection] = None
        for conn in self.holders:
            if conn.aborted:
                continue
            if best is None or conn.queued_bytes > best.queued_bytes:
                best = conn
        return best

    @contextlib.contextmanager
    def holding_cleanup(self) -> Iterator[None]:
        """While this is held (a message handler, or one send), a connection
        dropped for not reading is detached only when it is released, so
        the service's sends keep the order it read: the message being
        forwarded goes out before the peerLeft or clientLeft its sender's
        leaving causes."""
        self._cleanup_depth += 1
        try:
            yield
        finally:
            self._cleanup_depth -= 1
            if self._cleanup_depth == 0:
                self._run_deferred()

    def detach_later(self, conn: Connection) -> None:
        if self._cleanup_depth == 0:
            self.detach(conn)
        elif conn not in self._deferred:
            self._deferred.append(conn)

    def _run_deferred(self) -> None:
        # A cleanup's own sends may drop further connections; they join
        # the list and are detached in turn.
        while self._deferred:
            conn = self._deferred.pop(0)
            self._cleanup_depth += 1
            try:
                self.detach(conn)
            finally:
                self._cleanup_depth -= 1

    def accept(self, conn: Connection) -> None:
        # A new connection has no role yet; it is counted with the clients
        # until it registers, so a registered station never holds a slot
        # a client needs, nor the reverse.
        if self.unregistered >= self.config.max_connections:
            conn.fail("overloaded")
            return
        if self.per_group.get(conn.group, 0) >= self.config.connections_per_address:
            conn.fail("tooManyConnections")
            return
        conn.counted = True
        self.connections.add(conn)
        self.unregistered += 1
        self.per_group[conn.group] = self.per_group.get(conn.group, 0) + 1
        conn.send(
            protocol.message(
                "hello",
                "client",
                version=(protocol.WATCH_PROTOCOL_VERSION
                         if self.config.relay_watch_version == 1 and self.config.relay_secret
                         else protocol.VERSION),
                nonce=identity.to_b64url(conn.nonce),
                stun=list(self.config.stun_urls),
                **({"watchRelayVersion": 1}
                   if self.config.relay_watch_version == 1 and self.config.relay_secret else {}),
            )
        )
        conn.timer = self.clock.call_later(self.config.handshake_timeout_ms, lambda: conn.fail("timeout"))

    def shutdown(self) -> None:
        for conn in list(self.connections):
            conn.fail("shuttingDown", close_code=CLOSE_GOING_AWAY)

    def detach(self, conn: Connection) -> None:
        """Forget a connection and tell whoever was waiting on it. Safe to
        call more than once."""
        if conn.detached:
            return
        conn.detached = True
        conn.cancel_timer()
        self.connections.discard(conn)
        if conn.counted:
            self._uncount(conn)
        if conn.station_counted:
            conn.station_counted = False
            self.station_count -= 1
            _decrement(self.stations_per_group, conn.group)
        if conn.role == "station":
            if conn.station_id is not None and self.stations.get(conn.station_id) is conn:
                del self.stations[conn.station_id]
                log.info("station %s left", short(conn.station_id))
            for intro in list(conn.introductions.values()):
                self._end_introduction(intro, station_code=None, client_code="stationLeft")
            if conn.nameplate is not None:
                self._release_nameplate(conn, client_code="peerLeft")
        elif conn.role == "client":
            if conn.introduction is not None:
                self._end_introduction(conn.introduction, station_code="clientLeft", client_code=None)
            if conn.mailbox is not None:
                self._close_mailbox(conn.mailbox, station_code="peerLeft", client_code=None)

    def _uncount(self, conn: Connection) -> None:
        conn.counted = False
        self.unregistered -= 1
        _decrement(self.per_group, conn.group)

    # ----------------------------------------------------------- dispatch

    def on_frame(self, conn: Connection, frame: Any) -> None:
        if not isinstance(frame, str):
            conn.fail("protocolError")
            return
        try:
            obj = protocol.parse_text(frame)
            role = conn.role
            if role is None:
                kind = obj.get("type")
                if kind == "register":
                    role = "station"
                elif kind in ("introduce", "mailbox.open"):
                    role = "client"
                else:
                    raise protocol.DecodeError("a first message of no role")
            msg = protocol.decode(obj, role, "server")
        except protocol.DecodeError:
            conn.fail("protocolError")
            return
        if conn.role is None:
            conn.role = role
            if role == "client":
                conn.cancel_timer()
        handler = getattr(self, "_%s_%s" % (conn.role, msg["type"].replace(".", "_")))
        with self.holding_cleanup():
            handler(conn, msg)

    # ----------------------------------------------------------- station

    def _station_register(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if conn.challenge is not None or conn.station_id is not None:
            conn.fail("protocolError")
            return
        spki = identity.from_b64url(msg["publicKey"])
        if spki is None or not identity.is_p256_spki(spki) or identity.rendezvous_id(spki) != msg["id"]:
            conn.fail("proofFailed")
            return
        conn.pending_spki = spki
        conn.pending_id = msg["id"]
        conn.watch_relay_version = msg.get("watchRelayVersion", 0)
        conn.challenge = self.random(identity.NONCE_BYTES)
        conn.send(protocol.message("challenge", "station", nonce=identity.to_b64url(conn.challenge)))

    def _station_prove(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if conn.challenge is None or conn.station_id is not None:
            conn.fail("protocolError")
            return
        signature = identity.from_b64url(msg["signature"])
        assert conn.pending_spki is not None and conn.pending_id is not None
        if signature is None or not identity.verify_raw(
            conn.pending_spki, identity.register_transcript(conn.challenge), signature
        ):
            conn.fail("proofFailed")
            return
        station_id = conn.pending_id
        previous = self.stations.get(station_id)
        if previous is conn:
            previous = None
        # Section 9.1: registered stations are a pool of their own, capped
        # per address group and in total. The registration this one
        # replaces does not count against it.
        in_group = self.stations_per_group.get(conn.group, 0)
        total = self.station_count
        if previous is not None and previous.station_counted:
            total -= 1
            if previous.group == conn.group:
                in_group -= 1
        if in_group >= self.config.stations_per_address:
            conn.fail("tooManyConnections")
            return
        if total >= self.config.max_stations:
            conn.fail("overloaded")
            return
        conn.cancel_timer()
        if previous is not None:
            log.info("station %s registered again; closing the older connection", short(station_id))
            previous.fail("replaced")
        if conn.counted:
            self._uncount(conn)
        conn.station_counted = True
        self.station_count += 1
        self.stations_per_group[conn.group] = self.stations_per_group.get(conn.group, 0) + 1
        conn.station_id = station_id
        self.stations[station_id] = conn
        log.info("station %s registered", short(station_id))
        conn.send(protocol.message("registered", "station", id=station_id))

    def _registered(self, conn: Connection) -> bool:
        if conn.station_id is None:
            conn.fail("protocolError")
            return False
        return True

    def _station_answer(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if not self._registered(conn):
            return
        intro = conn.introductions.get(msg["to"])
        if intro is None or intro.answered:
            conn.fail("unknownIntroduction")
            return
        intro.answered = True
        relay: Optional[Dict[str, Any]] = None
        if msg["turn"] and self.config.turn_secret:
            relay = turn.mint(
                self.config.turn_secret,
                intro.station_id,
                self.clock.wall_seconds(),
                self.config.turn_ttl_seconds,
                self.config.turn_urls,
            )
        # Section 12.1: the relay grant is minted at the same moment as the
        # TURN credentials, for an introduction the station accepted with the
        # relay allowed, and never otherwise. One random session names the
        # pairing; each end gets the token for its own leg.
        grant_session: Optional[bytes] = None
        if msg["turn"] and self.config.relay_secret:
            grant_session = self.random(relaygrant.SESSION_BYTES)
        log.info(
            "introduction %s to station %s answered, relay %s, relay grant %s",
            short(intro.iid),
            short(intro.station_id),
            "offered" if relay else "not offered",
            "offered" if grant_session is not None else "not offered",
        )
        tokens: Dict[int, str] = {}
        watch_tokens: Dict[int, str] = {}
        grant_expires = 0
        if grant_session is not None:
            # One expiry for the whole grant, so both ends get the same value.
            secret = self.config.relay_secret
            assert secret is not None
            grant_expires = self.clock.wall_seconds() + self.config.relay_ttl_seconds
            station = relaygrant.station_of(secret, intro.station_id)
            for leg in relaygrant.LEGS:
                tokens[leg] = relaygrant.mint(secret, leg, grant_session, station, grant_expires)
                if (self.config.relay_watch_version == 1 and conn.watch_relay_version == 1
                        and intro.watch_relay_version == 1):
                    watch_tokens[leg] = relaygrant.mint_watch(secret, leg, grant_session, station, grant_expires)
        intro.client.send(protocol.message("answer", "client", answer=msg["answer"], turn=relay))
        if grant_session is not None:
            intro.client.send(self._relay_grant("client", tokens[relaygrant.LEG_DEVICE], grant_expires,
                                               watch_token=watch_tokens.get(relaygrant.LEG_DEVICE)))
        if msg["turn"]:
            conn.send(protocol.message("credentials", "station", **{"from": intro.iid, "turn": relay}))
        if grant_session is not None:
            conn.send(self._relay_grant("station", tokens[relaygrant.LEG_CORE], grant_expires, intro.iid,
                                       watch_tokens.get(relaygrant.LEG_CORE)))

    def _relay_grant(self, receiver: str, token: str, expires: int, iid: Optional[str] = None,
                     watch_token: Optional[str] = None) -> Dict[str, Any]:
        fields: Dict[str, Any] = {"url": self.config.relay_url, "token": token, "expires": expires}
        if iid is not None:
            fields["from"] = iid
        if watch_token is not None:
            fields["watchToken"] = watch_token
        return protocol.message("relay.grant", receiver, **fields)

    def _station_candidate(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if not self._registered(conn):
            return
        intro = conn.introductions.get(msg["to"])
        if intro is None:
            conn.fail("unknownIntroduction")
            return
        if not intro.answered:
            # A station sends candidates only after its answer.
            conn.fail("protocolError")
            return
        if msg["candidate"]:
            if intro.station_candidates >= self.config.candidates_per_side:
                conn.fail("tooManyCandidates")
                return
            intro.station_candidates += 1
        intro.client.send(protocol.message("candidate", "client", candidate=msg["candidate"]))

    def _station_nameplate_claim(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if not self._registered(conn):
            return
        if conn.nameplate is None:
            number = self._allocate_nameplate()
            if number is None:
                conn.fail("nameplatesExhausted")
                return
            conn.nameplate = number
            self.nameplates[number] = conn
            log.info("station %s claimed a nameplate", short(conn.station_id or ""))
        conn.send(protocol.message("nameplate", "station", nameplate=conn.nameplate))

    def _station_nameplate_release(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if not self._registered(conn):
            return
        if conn.nameplate is not None:
            self._release_nameplate(conn, client_code="released")
        conn.send(protocol.message("nameplate.released", "station"))

    def _station_mailbox(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if not self._registered(conn):
            return
        box = conn.mailbox
        if box is None:
            conn.fail("noMailbox")
            return
        if box.station_messages >= self.config.mailbox_messages_per_side:
            conn.fail("tooManyMessages")
            return
        box.station_messages += 1
        box.client.send(protocol.message("mailbox", "client", body=msg["body"]))

    def _station_mailbox_close(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if not self._registered(conn):
            return
        if conn.mailbox is None:
            conn.fail("noMailbox")
            return
        self._close_mailbox(conn.mailbox, station_code="closed", client_code="peerClosed")

    # ----------------------------------------------------------- client

    def _client_introduce(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if conn.introduction is not None:
            conn.fail("protocolError")
            return
        now = self.clock.now_ms()
        self._maybe_sweep(now)
        station_id = msg["id"]
        # The per-station limit counts per station per network (JJ's ruling,
        # 2026-09-25): a flood from one network blocks only that network.
        station_key = "%s %s" % (station_id, conn.group)
        wait = first_allowed(now, (self.intro_by_address, conn.group), (self.intro_by_station, station_key))
        if wait is not None:
            conn.fail("rateLimited", retry_after_ms=wait)
            self._start_idle(conn)
            return
        self.intro_by_address.record(conn.group, now)
        self.intro_by_station.record(station_key, now)
        station = self.stations.get(station_id)
        if station is None or station.closing:
            # An unknown id and an offline one get the same bytes.
            conn.fail("offline")
            self._start_idle(conn)
            return
        iid = identity.to_b64url(self.random(identity.INTRODUCTION_ID_BYTES))
        while iid in self.introductions:
            iid = identity.to_b64url(self.random(identity.INTRODUCTION_ID_BYTES))
        intro = Introduction(iid, station_id, conn, station, msg.get("watchRelayVersion", 0))
        self.introductions[iid] = intro
        station.introductions[iid] = intro
        conn.introduction = intro
        conn.cancel_timer()
        intro.timer = self.clock.call_later(
            self.config.introduction_lifetime_ms,
            lambda: self._end_introduction(intro, station_code="expired", client_code="expired"),
        )
        log.info("introduction %s to station %s", short(iid), short(station_id))
        station.send(
            protocol.message(
                "introduction",
                "station",
                **{
                    "from": iid,
                    "device": msg["device"],
                    "deviceSignature": msg["deviceSignature"],
                    "offer": msg["offer"],
                    "nonce": identity.to_b64url(conn.nonce),
                },
            )
        )

    def _client_candidate(self, conn: Connection, msg: Dict[str, Any]) -> None:
        intro = conn.introduction
        if intro is None:
            conn.fail("noIntroduction")
            return
        if msg["candidate"]:
            if intro.client_candidates >= self.config.candidates_per_side:
                conn.fail("tooManyCandidates")
                return
            intro.client_candidates += 1
        intro.station.send(
            protocol.message("candidate", "station", **{"from": intro.iid, "candidate": msg["candidate"]})
        )

    def _client_mailbox_open(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if conn.mailbox is not None:
            conn.fail("protocolError")
            return
        now = self.clock.now_ms()
        self._maybe_sweep(now)
        wait = self.mailbox_by_address.check(conn.group, now)
        if wait is not None:
            conn.fail("rateLimited", retry_after_ms=wait)
            self._start_idle(conn)
            return
        self.mailbox_by_address.record(conn.group, now)
        number = msg["nameplate"]
        station = self.nameplates.get(number)
        if station is None or station.closing:
            conn.fail("nameplateUnknown")
            self._start_idle(conn)
            return
        if station.mailbox is not None:
            conn.fail("nameplateBusy")
            self._start_idle(conn)
            return
        box = Mailbox(number, station, conn)
        station.mailbox = box
        conn.mailbox = box
        conn.cancel_timer()
        box.timer = self.clock.call_later(
            self.config.mailbox_lifetime_ms,
            lambda: self._close_mailbox(box, station_code="expired", client_code="expired"),
        )
        log.info("mailbox opened for station %s", short(station.station_id or ""))
        conn.send(protocol.message("mailbox.opened", "client", nameplate=number))
        station.send(protocol.message("mailbox.opened", "station", nameplate=number))

    def _client_mailbox(self, conn: Connection, msg: Dict[str, Any]) -> None:
        box = conn.mailbox
        if box is None:
            conn.fail("noMailbox")
            return
        if box.client_messages >= self.config.mailbox_messages_per_side:
            conn.fail("tooManyMessages")
            return
        box.client_messages += 1
        box.station.send(protocol.message("mailbox", "station", body=msg["body"]))

    def _client_mailbox_close(self, conn: Connection, msg: Dict[str, Any]) -> None:
        if conn.mailbox is None:
            conn.fail("noMailbox")
            return
        self._close_mailbox(conn.mailbox, station_code="peerClosed", client_code="closed")

    # ----------------------------------------------------------- helpers

    def _maybe_sweep(self, now: int) -> None:
        """Drop rate-limit keys whose events have all left the window, at
        most once a minute, so the tables hold only the last minute."""
        if now - self._last_sweep_ms < MINUTE_MS:
            return
        self._last_sweep_ms = now
        for limiter in (self.intro_by_address, self.intro_by_station, self.mailbox_by_address):
            limiter.sweep(now)

    def _start_idle(self, conn: Connection) -> None:
        """Section 3: the idle timer starts once, when a client is left with
        nothing pending, and never restarts while it runs, so refused
        requests cannot keep a connection open."""
        if conn.closing or conn.role != "client":
            return
        if conn.introduction is not None or conn.mailbox is not None:
            return
        if conn.timer is not None:
            return
        conn.timer = self.clock.call_later(self.config.idle_timeout_ms, lambda: conn.fail("idle"))

    def _end_introduction(self, intro: Introduction, station_code: Optional[str], client_code: Optional[str]) -> None:
        if self.introductions.get(intro.iid) is not intro:
            return
        del self.introductions[intro.iid]
        intro.station.introductions.pop(intro.iid, None)
        if intro.timer is not None:
            intro.timer.cancel()
        if intro.client.introduction is intro:
            intro.client.introduction = None
        log.info("introduction %s ended: %s", short(intro.iid), station_code or client_code)
        if station_code is not None:
            intro.station.send(
                protocol.message("introduction.end", "station", **{"from": intro.iid, "code": station_code})
            )
        if client_code is not None:
            intro.client.send(protocol.message("introduction.end", "client", code=client_code))
            self._start_idle(intro.client)

    def _close_mailbox(self, box: Mailbox, station_code: Optional[str], client_code: Optional[str]) -> None:
        if box.station.mailbox is not box:
            return
        box.station.mailbox = None
        if box.client.mailbox is box:
            box.client.mailbox = None
        if box.timer is not None:
            box.timer.cancel()
        log.info("mailbox closed: %s", station_code or client_code)
        if station_code is not None:
            box.station.send(protocol.message("mailbox.closed", "station", code=station_code))
        if client_code is not None:
            box.client.send(protocol.message("mailbox.closed", "client", code=client_code))
            self._start_idle(box.client)

    def _allocate_nameplate(self) -> Optional[int]:
        while self._freed:
            number = heapq.heappop(self._freed)
            if number not in self.nameplates:
                return number
        if self._next_nameplate > protocol.NAMEPLATE_MAX:
            return None
        number = self._next_nameplate
        self._next_nameplate += 1
        return number

    def _release_nameplate(self, conn: Connection, client_code: str) -> None:
        number = conn.nameplate
        if number is None:
            return
        if conn.mailbox is not None:
            self._close_mailbox(conn.mailbox, station_code=None, client_code=client_code)
        conn.nameplate = None
        if self.nameplates.get(number) is conn:
            del self.nameplates[number]
            heapq.heappush(self._freed, number)
        log.info("station %s released its nameplate", short(conn.station_id or ""))


def _decrement(table: Dict[str, int], key: str) -> None:
    left = table.get(key, 1) - 1
    if left > 0:
        table[key] = left
    else:
        table.pop(key, None)
