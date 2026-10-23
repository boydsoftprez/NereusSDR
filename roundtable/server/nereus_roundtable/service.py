# no-port-check: NereusSDR-original.
"""The Roundtable server's connections: the envelope, the connection caps,
sign-in and replacement (wire spec sections 2.4, 3, 4, 5.1, 5.3.1, 9.1).

The service is transport-agnostic: a connection is anything with async
send(text), close(code) and async iteration over incoming frames
(transport.WsTransport). All state changes happen synchronously in the
reading task of the connection whose message caused them, so the order of
what the server sends follows the order of what it read. Each connection
has its own outbound queue and writer task, so one slow peer never holds up
another.

The client address is used only to find its address group, which is held
in memory while the connection is open and never logged (wire spec 9.1,
9.4). Logs name at most the first 6 characters of a station id.
"""

from __future__ import annotations

import asyncio
import logging
import os
from typing import Any, Callable, Dict, Optional, Set

from . import identity, protocol
from .clock import RealClock, TimerHandle
from .config import Config, check as check_config, wire_limits
from .limits import AddressGroups
from .store import Store

log = logging.getLogger("nereus_roundtable")

STATION = "station"
LIVE = "live"


def short(station_id: str) -> str:
    """A station id in a log: its first 6 characters, never the whole."""
    return station_id[:6]


class _Close:
    def __init__(self, code: int) -> None:
        self.code = code


class Connection:
    def __init__(self, service: "Service", transport: Any, path: str, address: str) -> None:
        self.service = service
        self.transport = transport
        self.path = path
        # Only the group is kept, and only for as long as the connection.
        self.group = AddressGroups.group(address)
        self.counted = False
        self.queue: "asyncio.Queue[Any]" = asyncio.Queue()
        self.queued_bytes = 0
        self.writer: Optional["asyncio.Future[None]"] = None
        self.closing = False
        self.detached = False
        self.timer: Optional[TimerHandle] = None
        # Set by a droppable message that did not fit (wire spec 9.1); the
        # next generation the connection gets then shows the gap.
        self.dropped = False
        # Station path.
        self.nonce = b""
        self.station_id: Optional[str] = None
        self.station_kind: Optional[str] = None

    def send(self, message: Dict[str, Any], droppable: bool = False) -> bool:
        """Queue one message. A droppable one (a room delta or a band-list
        change) is dropped when the queue holds its cap of messages or
        bytes; past twice the cap the connection is closed with 1000 (wire
        spec 9.1). True when queued."""
        if self.closing:
            return False
        text = protocol.encode(message)
        size = len(text.encode("utf-8"))
        config = self.service.config
        count = self.queue.qsize()
        if droppable and (count >= config.outbound_queue_messages or self.queued_bytes + size > config.outbound_queue_bytes):
            self.dropped = True
            return False
        if count >= 2 * config.outbound_queue_messages or self.queued_bytes + size > 2 * config.outbound_queue_bytes:
            log.warning("closed a connection whose queue passed twice its cap")
            self._discard_queue()
            self.close(protocol.CLOSE_NORMAL)
            return False
        self.queued_bytes += size
        self.queue.put_nowait((text, size))
        return True

    def close(self, code: int, error: Optional[Dict[str, Any]] = None) -> None:
        """Close after what is queued, with `error` sent first when the code
        has one (wire spec 2.4)."""
        if self.closing:
            return
        if error is not None:
            self.send(error)
        self.closing = True
        self.queue.put_nowait(_Close(code))
        self.service.detach(self)

    def _discard_queue(self) -> None:
        while not self.queue.empty():
            self.queue.get_nowait()
        self.queued_bytes = 0

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
                await self.transport.send(text)
                self.queued_bytes -= size
        except Exception:  # noqa: BLE001 - the peer went away; the reader cleans up
            return
        finally:
            self._discard_queue()


class Service:
    def __init__(
        self,
        config: Config,
        clock: Any = None,
        store: Optional[Store] = None,
        random: Callable[[int], bytes] = os.urandom,
    ) -> None:
        check_config(config)
        self.config = config
        self.clock = clock or RealClock()
        self.store = store if store is not None else Store(":memory:")
        self.random = random
        self.connections: Set[Connection] = set()
        self.stations: Dict[str, Connection] = {}
        self.station_groups = AddressGroups()
        self.live_groups = AddressGroups()
        self.live_count = 0
        # Counters the conformance runner reads to know the service has
        # caught up with what it sent; nothing else uses them.
        self.frames_handled = 0
        self.peer_closes = 0
        self._handlers: Dict[str, Dict[str, Callable[[Connection, Dict[str, Any]], None]]] = {STATION: {}, LIVE: {}}

    # ----------------------------------------------------------- lifecycle

    async def handle_station(self, transport: Any, peer_addr: str) -> None:
        """Serve one /v1/station connection until it ends."""
        await self._run(transport, STATION, peer_addr)

    async def handle_live(self, transport: Any, peer_addr: str) -> None:
        """Serve one /v1/live connection until it ends."""
        await self._run(transport, LIVE, peer_addr)

    async def _run(self, transport: Any, path: str, address: str) -> None:
        conn = Connection(self, transport, path, address)
        writer = asyncio.ensure_future(conn.write_loop())
        conn.writer = writer
        self.connections.add(conn)
        self._accept(conn)
        try:
            async for frame in transport:
                if not conn.closing:
                    try:
                        self._on_frame(conn, frame)
                    except Exception as exc:  # noqa: BLE001
                        # A bug, not the peer's doing. The class name only:
                        # an exception's text can hold what the peer sent.
                        log.error("a message handler failed (%s); closing its connection", type(exc).__name__)
                        conn.close(protocol.CLOSE_NORMAL)
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

    def _accept(self, conn: Connection) -> None:
        config = self.config
        if conn.path == STATION:
            # Wire spec 9.1: 8 station connections per address group,
            # counted at connect, before sign-in.
            if self.station_groups.count(conn.group) >= config.station_connections_per_group:
                log.info("refused a station connection: too many from one network")
                conn.close(protocol.CLOSE_TRY_LATER, protocol.error_message("rate-network", None))
                return
            self.station_groups.add(conn.group)
            conn.counted = True
            conn.nonce = self.random(identity.NONCE_BYTES)
            conn.send(protocol.message("challenge", STATION, nonce=identity.to_b64url(conn.nonce)))
            conn.timer = self.clock.call_later(config.hello_timeout_ms, lambda: self._hello_timeout(conn))
            return
        # Live sockets: 4 per address group, 500 in all. A live reader is
        # sent nothing until it asks (wire spec 3.2).
        if self.live_groups.count(conn.group) >= config.live_sockets_per_group:
            log.info("refused a live connection: too many from one network")
            conn.close(protocol.CLOSE_TRY_LATER, protocol.error_message("rate-network", None, receiver=LIVE))
            return
        if self.live_count >= config.live_sockets_total:
            log.info("refused a live connection: the server is full")
            conn.close(protocol.CLOSE_TRY_LATER, protocol.error_message("rate-busy", None, receiver=LIVE))
            return
        self.live_groups.add(conn.group)
        self.live_count += 1
        conn.counted = True

    def _hello_timeout(self, conn: Connection) -> None:
        conn.timer = None
        if conn.station_id is None and not conn.closing:
            log.info("closed a station connection that sent no hello in time")
            conn.close(protocol.CLOSE_HELLO_TIMEOUT)

    def detach(self, conn: Connection) -> None:
        """Forget a connection. Safe to call more than once."""
        if conn.detached:
            return
        conn.detached = True
        conn.cancel_timer()
        self.connections.discard(conn)
        if conn.counted:
            conn.counted = False
            if conn.path == STATION:
                self.station_groups.remove(conn.group)
            else:
                self.live_groups.remove(conn.group)
                self.live_count -= 1
        if conn.station_id is not None and self.stations.get(conn.station_id) is conn:
            del self.stations[conn.station_id]
            log.info("station %s left", short(conn.station_id))

    def shutdown(self) -> None:
        for conn in list(self.connections):
            conn.close(protocol.CLOSE_GOING_AWAY)

    # ----------------------------------------------------------- dispatch

    def on(self, path: str, kind: str, handler: Callable[[Connection, Dict[str, Any]], None]) -> None:
        """Set the handler for one kind on one path. A kind with no handler
        is decoded, so a malformed one still closes 4002, and then ignored."""
        self._handlers[path][kind] = handler

    def _on_frame(self, conn: Connection, frame: Any) -> None:
        if not isinstance(frame, str):
            # A binary frame is a protocol error (wire spec 2.2).
            conn.close(protocol.CLOSE_PROTOCOL)
            return
        try:
            msg = protocol.decode(frame, conn.path, "server", max_slices=self.config.slices)
        except protocol.ProtocolError as exc:
            log.debug("protocol error: %s", type(exc).__name__)
            conn.close(exc.close_code)
            return
        kind = msg["type"]
        if conn.path == STATION:
            if conn.station_id is None:
                # The first message must be hello (wire spec 3.1).
                if kind != "hello":
                    conn.close(protocol.CLOSE_PROTOCOL)
                    return
                self._hello(conn, msg)
                return
            if kind == "hello":
                # A second hello is a protocol error.
                conn.close(protocol.CLOSE_PROTOCOL)
                return
        handler = self._handlers[conn.path].get(kind)
        if handler is not None:
            handler(conn, msg)

    # ----------------------------------------------------------- sign-in

    def _refuse_sign_in(self, conn: Connection, cause: str) -> None:
        log.info("refused a sign-in: %s", cause)
        conn.close(protocol.CLOSE_SIGN_IN, protocol.error_message(cause, "hello"))

    def _hello(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.1 and 4: the version, then the key."""
        if msg["v"] != protocol.VERSION:
            self._refuse_sign_in(conn, "version")
            return
        now = self.clock.wall_ms()
        callsign: Optional[str] = None
        grid: Optional[str] = None
        if "spki" in msg:
            spki = identity.from_b64url(msg["spki"])
            sig = identity.from_b64url(msg["sig"])
            if not identity.verify_hello(spki, sig, self.config.public_host, conn.nonce):
                self._refuse_sign_in(conn, "bad-sign-in")
                return
            assert spki is not None
            station_id = identity.roundtable_id(spki)
            kind = "core"
            self.store.station_upsert(station_id, kind, None, None, now)
        else:
            key = identity.from_b64url(msg["key"])
            assert key is not None
            row = self.store.key_lookup(identity.key_hash(key))
            if row is None:
                self._refuse_sign_in(conn, "bad-sign-in")
                return
            if row["revoked"] is not None:
                log.info("refused a sign-in: a revoked key")
                conn.close(protocol.CLOSE_REVOKED)
                return
            station_id = row["station_id"]
            kind = "key"
            station = self.store.station_get(station_id)
            if station is not None:
                callsign = station["callsign"]
                grid = station["grid"]
            self.store.station_upsert(station_id, kind, None, None, now)
        conn.cancel_timer()
        previous = self.stations.get(station_id)
        if previous is not None and previous is not conn:
            # Wire spec 3.1: one connection per station id; the newer wins.
            log.info("station %s signed in again; closing the older connection", short(station_id))
            previous.close(protocol.CLOSE_REPLACED)
        conn.station_id = station_id
        conn.station_kind = kind
        self.stations[station_id] = conn
        log.info("station %s signed in (%s)", short(station_id), kind)
        conn.send(
            protocol.message(
                "welcome",
                STATION,
                station=station_id,
                kind=kind,
                callsign=protocol.normalize_callsign(callsign) if callsign else None,
                grid=protocol.normalize_grid(grid) if grid else None,
                time=now,
                limits=wire_limits(self.config),
                mentions=self.mentions_for(station_id),
            )
        )
        conn.send(self.rooms_message())

    def mentions_for(self, station_id: str) -> list:
        """Messages naming the station since its last `mentionsSeen` (wire
        spec 5.4.2). The server keeps no messages yet, so there are none."""
        return []

    def rooms_message(self) -> Dict[str, Any]:
        """The full band list, generation 1 (wire spec 5.4.3). The server
        keeps no rooms yet, so it is empty."""
        return protocol.message("rooms", STATION, gen=1, rooms=[])
