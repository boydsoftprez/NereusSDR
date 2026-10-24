# no-port-check: NereusSDR-original.
"""The Roundtable server's connections: the envelope, the connection caps,
sign-in and replacement (wire spec sections 2.4, 3, 4, 5.1, 5.3.1, 9.1),
a station's presence, readings and views: `rooms`, `roomsDelta`,
`room` and `delta`, with their generations, pacing and resync (5.3.2,
5.3.3, 5.3.8 to 5.3.10, 5.4.3 to 5.4.6, 6.11), and its messages, names,
reports, mentions and going hidden (5.3.4 to 5.3.7, 5.3.11, 5.3.14, 5.4.7,
5.4.8, 6.9, 6.10), and a live reader's watched band list and rooms
(3.2, 5.5): the same `rooms`, `roomsDelta`, `room` and `delta` a station
gets, with every `you` false and every `mine` null.

The service is transport-agnostic: a connection is anything with async
send(text), close(code) and async iteration over incoming frames
(transport.WsTransport). All state changes happen synchronously in the
reading task of the connection whose message caused them, so the order of
what the server sends follows the order of what it read. Each connection
has its own outbound queue and writer task, so one slow peer never holds up
another.

Every message read and every timer is one event. The rooms and turns it
changes are collected while it runs; when it ends, each connection gets, in
order, a `roomsDelta` if one is due, the full `room` views that entered its
view, one `delta` for each room it views that changed, then any `mention`
(6.11 "Order"). An answer (`posted`, `error`) is queued by the handler
itself, so it comes first.

The client address is used only to find its address group, which is held
in memory while the connection is open and never logged (wire spec 9.1,
9.4). Logs name at most the first 6 characters of a station id, and never
message text or a callsign.
"""

from __future__ import annotations

import asyncio
import logging
import os
import sqlite3
from contextlib import contextmanager
from typing import Any, Callable, Dict, Iterator, List, Optional, Set, Tuple

from . import identity, messages, protocol
from .clock import RealClock, TimerHandle
from .config import Config, check as check_config, wire_limits
from .limits import AddressGroups, Window
from .rooms import SILENT_LEAVE_MS, Profile, Room, RoomBook, RoomDiff, Row, message_object
from .store import Store

log = logging.getLogger("nereus_roundtable")

STATION = "station"
LIVE = "live"

# Wire spec 6.11 and 9.2.
BAND_PACING_MS = 2000
# Wire spec 6.7 and 9.2: a reading stamped further than this from the
# server's clock is taken as stamped at its arrival.
READING_SKEW_MS = 5000
# Wire spec 5.3.3: at most one `rate` error a minute per station for
# readings over the limit.
READING_RATE_ERROR_MS = 60000
# Wire spec 6.9 and 9.2: an unknown turn may be named up to 24 hours after
# it ends.
NAME_AFTER_MS = 86400000
# Wire spec 5.3: after `hidden`, these are the only station kinds taken.
AFTER_HIDDEN = frozenset({"weblink", "deleteArchive"})


def short(station_id: str) -> str:
    """A station id in a log: its first 6 characters, never the whole."""
    return station_id[:6]


class _Close:
    def __init__(self, code: int) -> None:
        self.code = code


class _View:
    """One generation a connection holds: its band list, or one room's
    view (6.11). `lost` is set when a droppable message did not fit; the
    next message sent then shows the gap and the view goes `stale`, and a
    stale view gets nothing more until `resync` (except a room's closing)."""

    def __init__(self, opening: Optional[int] = None) -> None:
        self.gen = 1
        self.opening = opening
        self.lost = False
        self.stale = False


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
        self.software = ""
        self.software_version = ""
        # The latest presence's slices by id; None before any presence.
        self.presence: Optional[Dict[int, Dict[str, Any]]] = None
        # The station's current callsign, for mentions (6.10): the stored
        # one at sign-in, then its presence's.
        self.callsign: Optional[str] = None
        # Set by `hidden` (5.3.14): nothing but answers from then on.
        self.hidden = False
        self.silence_timer: Optional[TimerHandle] = None
        self.reading_rate_error_ms: Optional[int] = None
        # Views (6.11): the rooms it reads, each room view's generation,
        # the full views owed at the end of the event, and the band list.
        self.reads: Set[str] = set()
        self.views: Dict[str, _View] = {}
        self.want_full: List[str] = []
        self.band: Optional[_View] = None
        self.band_sent: Dict[str, Tuple[Optional[int], Dict[str, Any]]] = {}
        self.band_last_ms: Optional[int] = None
        self.band_timer: Optional[TimerHandle] = None
        # Live path (5.5): the watched band ("all" or a band name, None
        # before any `watchBand`) and the watched rooms.
        self.watch_band: Optional[str] = None
        self.watched: Set[str] = set()

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
        for name in ("silence_timer", "band_timer"):
            handle = getattr(self, name)
            if handle is not None:
                handle.cancel()
                setattr(self, name, None)

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
        self.presence_window = Window(config.presence_per_second, 1000)
        self.reading_window = Window(config.readings_per_second, 1000)
        self.post_minute_window = Window(config.messages_per_minute, 60000)
        self.post_hour_window = Window(config.messages_per_hour, 3600000)
        self.name_window = Window(config.names_per_minute, 60000)
        self.report_window = Window(config.reports_per_hour, 3600000)
        # `mention`s owed at the end of the event, after every delta.
        self._mentions: List[Tuple[Connection, Dict[str, Any]]] = []
        self.stopping = False
        self._depth = 0
        self._viewers: Dict[str, Set[Connection]] = {}
        # Connections owed a full view at the end of the event.
        self._dirty: Set[Connection] = set()
        self.book = RoomBook(_EventClock(self), self.store, random)
        self.book.reload()
        self.on(STATION, "presence", self._presence)
        self.on(STATION, "reading", self._reading)
        self.on(STATION, "read", self._read)
        self.on(STATION, "unread", self._unread)
        self.on(STATION, "resync", self._resync)
        self.on(STATION, "post", self._post)
        self.on(STATION, "delete", self._delete)
        self.on(STATION, "name", self._name)
        self.on(STATION, "report", self._report)
        self.on(STATION, "mentionsSeen", self._mentions_seen)
        self.on(STATION, "hidden", self._hidden)
        self.on(LIVE, "watchBand", self._watch_band)
        self.on(LIVE, "watch", self._watch)
        self.on(LIVE, "unwatch", self._unwatch)
        self.on(LIVE, "resync", self._live_resync)

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
                        with self.event():
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

    @contextmanager
    def event(self) -> Iterator[None]:
        """One message or timer (6.11): what it changes is sent when the
        outermost event ends."""
        self._depth += 1
        try:
            yield
        finally:
            self._depth -= 1
            if self._depth == 0:
                self._flush()

    def fire(self, callback: Callable[[], None]) -> None:
        """Run a timer's work as one event."""
        with self.event():
            callback()

    def call_later(self, delay_ms: int, callback: Callable[[], None]) -> TimerHandle:
        return self.clock.call_later(delay_ms, lambda: self.fire(callback))

    def _hello_timeout(self, conn: Connection) -> None:
        conn.timer = None
        if conn.station_id is None and not conn.closing:
            log.info("closed a station connection that sent no hello in time")
            conn.close(protocol.CLOSE_HELLO_TIMEOUT)

    def detach(self, conn: Connection) -> None:
        """Forget a connection. Safe to call more than once."""
        if conn.detached:
            return
        with self.event():
            self._detach(conn)

    def _detach(self, conn: Connection) -> None:
        conn.detached = True
        for room_id in list(conn.views):
            self._drop_view(conn, room_id)
        conn.reads.clear()
        conn.watched.clear()
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
            # Its rows leave every room (6.4).
            self.book.remove_station(conn.station_id)
            del self.stations[conn.station_id]
            log.info("station %s left", short(conn.station_id))

    def shutdown(self) -> None:
        self.stopping = True
        for conn in list(self.connections):
            conn.close(protocol.CLOSE_GOING_AWAY)
        # The open turns' tracks, so a restart loses as little as it can.
        self.book.flush()

    # ----------------------------------------------------------- dispatch

    def on(self, path: str, kind: str, handler: Callable[[Connection, Dict[str, Any]], None]) -> None:
        """Set the handler for one kind on one path. A kind with no handler
        is decoded, so a malformed one still closes 4002, and then ignored."""
        self._handlers[path][kind] = handler

    def _on_frame(self, conn: Connection, frame: Any) -> None:
        if conn.station_id is not None:
            self._heard_from(conn)
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
            if conn.hidden and kind not in AFTER_HIDDEN:
                # Wire spec 5.3 and 5.3.14; a post is always answered with
                # `posted` (5.3.4).
                if kind == "post":
                    self._posted(conn, msg["clientId"], "hidden")
                else:
                    conn.send(protocol.error_message("hidden", kind))
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
        conn.software = msg["software"]
        conn.software_version = msg["softwareVersion"]
        previous = self.stations.get(station_id)
        if previous is not None and previous is not conn:
            # Wire spec 3.1: one connection per station id; the newer wins.
            log.info("station %s signed in again; closing the older connection", short(station_id))
            previous.close(protocol.CLOSE_REPLACED)
        conn.station_id = station_id
        conn.station_kind = kind
        conn.callsign = (self.store.station_get(station_id) or {}).get("callsign")
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
        self._send_rooms(conn)
        self._heard_from(conn)

    def mentions_for(self, station_id: str) -> List[Dict[str, Any]]:
        """Messages naming the station posted after its last `mentionsSeen`,
        oldest first, at most the newest 50 (wire spec 5.4.2)."""
        out: List[Dict[str, Any]] = []
        for row in self.store.unread_mentions(station_id):
            room = self.book.rooms.get(row["room_id"])
            obj = room.message_object(row) if room is not None else message_object(row)
            out.append({"room": row["room_id"], "message": obj})
        return out

    def rooms_message(self) -> Dict[str, Any]:
        """The full band list, generation 1 (wire spec 5.4.3): every live
        room in ascending order of center frequency."""
        return protocol.message("rooms", STATION, gen=1, rooms=[r.summary() for r in self.book.live_rooms()])

    # ----------------------------------------------------------- presence

    def _heard_from(self, conn: Connection) -> None:
        """Something came from the station: its rows leave only after 30 s
        with nothing (6.4)."""
        if conn.silence_timer is not None:
            conn.silence_timer.cancel()
        conn.silence_timer = self.call_later(SILENT_LEAVE_MS, lambda: self._silent(conn))

    def _silent(self, conn: Connection) -> None:
        conn.silence_timer = None
        sid = conn.station_id
        if sid is None or conn.closing or self.stations.get(sid) is not conn:
            return
        if self.book.has_placements(sid):
            log.info("station %s silent for 30 s; its rows leave", short(sid))
        # The connection stays open; the next presence places the slices
        # again, joining.
        self.book.remove_station(sid)

    def _presence(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.2."""
        sid = conn.station_id
        assert sid is not None
        now = self.clock.now_ms()
        retry = self.presence_window.retry_after(sid, now)
        if retry:
            conn.send(protocol.error_message("rate", "presence", retry))
            return
        stored = self.store.station_get(sid) or {}
        if conn.station_kind == "key":
            callsign = stored.get("callsign")
            grid = msg["grid"] if msg["grid"] is not None else stored.get("grid")
        else:
            callsign = msg["callsign"]
            grid = msg["grid"]
        if callsign is None or not protocol.is_callsign(callsign):
            conn.send(protocol.error_message("not-allowed-callsign", "presence"))
            return
        self.presence_window.allow(sid, now)
        callsign = protocol.normalize_callsign(callsign)
        conn.callsign = callsign
        grid = protocol.normalize_grid(grid) if grid is not None and protocol.is_grid(grid) else None
        if conn.station_kind == "core":
            self.store.station_upsert(sid, "core", callsign, grid, self.clock.wall_ms())
        slices = msg["slices"]
        conn.presence = {s["id"]: s for s in slices}
        profile = Profile(callsign, grid, not msg["atRadio"], conn.software, conn.software_version, conn)
        self.book.apply_presence(sid, profile, slices)

    def _reading(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.3, checked in its order."""
        sid = conn.station_id
        assert sid is not None
        slice_id = msg["slice"]
        listed = conn.presence.get(slice_id) if conn.presence is not None else None
        if listed is None:
            conn.send(protocol.error_message("unknown-slice", "reading"))
            return
        if listed["keyed"]:
            conn.send(protocol.error_message("slice-keyed", "reading"))
            return
        row = self.book.counted_row(sid, slice_id)
        if row is None or not row.member:
            # Joining, or not the station's counted slice there: dropped.
            return
        now = self.clock.now_ms()
        key = f"{sid}/{slice_id}"
        retry = self.reading_window.retry_after(key, now)
        if retry:
            last = conn.reading_rate_error_ms
            if last is None or now - last >= READING_RATE_ERROR_MS:
                conn.reading_rate_error_ms = now
                conn.send(protocol.error_message("rate", "reading", retry))
            return
        self.reading_window.allow(key, now)
        wall = self.clock.wall_ms()
        t = msg["t"]
        if abs(t - wall) > READING_SKEW_MS:
            t = wall
        self.book.reading(row, msg["dbm"], t, msg["present"])

    # ----------------------------------------------------------- messages

    def _posted(self, conn: Connection, client_id: str, cause: Optional[str] = None, msg_id: Optional[int] = None) -> None:
        """The answer to every `post` (5.4.7)."""
        if cause is None:
            conn.send(protocol.message("posted", STATION, clientId=client_id, ok=True, msg=msg_id, code=None, reason=None))
            return
        code, reason, _ = protocol.REASONS[cause]
        conn.send(protocol.message("posted", STATION, clientId=client_id, ok=False, msg=None, code=code, reason=reason))

    def _member_row(self, conn: Connection, room_id: str) -> Tuple[Optional[Room], Optional[Row]]:
        """The live room with this id and the station's row there, when that
        row is a member (not joining)."""
        room = self.book.live(room_id)
        row = room.rows.get(conn.station_id or "") if room is not None else None
        if row is None or not row.member:
            return room, None
        return room, row

    def _post(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.4, its refusals in its order (`hidden` is checked
        before any handler)."""
        sid = conn.station_id
        assert sid is not None
        client_id = msg["clientId"]
        room, row = self._member_row(conn, msg["room"])
        if room is None or row is None:
            self._posted(conn, client_id, "not-member")
            return
        if row.away:
            self._posted(conn, client_id, "away")
            return
        wall = self.clock.wall_ms()
        if self.store.station_blocked(sid, wall):
            self._posted(conn, client_id, "blocked")
            return
        now = self.clock.now_ms()
        if self.post_minute_window.retry_after(sid, now) or self.post_hour_window.retry_after(sid, now):
            self._posted(conn, client_id, "rate")
            return
        cleaned = messages.clean_text(msg["text"])
        refusal = messages.check(cleaned, self.config.message_code_points, self.config.message_lines)
        if refusal == "too-long":
            self._posted(conn, client_id, "too-long")
            return
        if refusal is not None:
            self._posted(conn, client_id, "not-allowed-empty")
            return
        # The sender's latest reading here, when it is stored with a turn
        # still open (6.10, design D4).
        dbm = None
        if row.reading is not None and row.reading_turn is not None and row.reading_turn in room.engine.open:
            dbm = row.reading.dbm
        words = messages.words(cleaned)
        assert room.opening_id is not None
        try:
            msg_id = self.store.message_add(room.opening_id, room.id, sid, row.callsign, row.nth, wall, cleaned, dbm, words)
            stored = self.store.message_get(msg_id)
        except sqlite3.Error as exc:
            log.error("a database error saving a message (%s)", type(exc).__name__)
            self._posted(conn, client_id, "server")
            return
        assert stored is not None
        self.post_minute_window.allow(sid, now)
        self.post_hour_window.allow(sid, now)
        room.message_count += 1
        room.last = max(room.last, wall)
        room.message_rows[msg_id] = row
        room.message_changed(stored)
        self._posted(conn, client_id, None, msg_id)
        # Every other connected station whose base callsign it names gets
        # `mention` after the deltas (6.10, 6.11 "Order").
        bases = set(words)
        mention = protocol.message("mention", STATION, room=room.id, message=room.message_object(stored))
        for other in self.stations.values():
            if other is conn or other.hidden or other.closing or not other.callsign:
                continue
            if messages.base_callsign(other.callsign) in bases:
                self._mentions.append((other, mention))

    def _delete(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.5."""
        sid = conn.station_id
        assert sid is not None
        result = self.store.message_delete(msg["msg"], sid)
        if result == "not-found":
            conn.send(protocol.error_message("not-found", "delete"))
            return
        if result == "not-allowed":
            conn.send(protocol.error_message("not-allowed-delete", "delete"))
            return
        if result != "deleted":
            return
        row = self.store.message_get(msg["msg"])
        assert row is not None
        room = self.book.live(row["room_id"])
        if room is not None:
            # Views hold the last 50 of the room id, any opening.
            room.message_changed(row)

    def _find_turn(self, room_id: str, turn_id: int) -> Tuple[Optional[Room], Optional[Any]]:
        """A turn the rooms still hold in memory (open, or ended in this
        process), with its room."""
        room = self.book.rooms.get(room_id)
        if room is not None:
            for turn in room.engine.open:
                if turn.id == turn_id:
                    return room, turn
        for turn in self.book.earlier.get(room_id, []):
            if turn.id == turn_id:
                return room, turn
        return room, None

    def _name(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.6 and 6.9, its refusals in its order."""
        sid = conn.station_id
        assert sid is not None
        turn_id = msg["turn"]
        stored = self.store.turn_get(turn_id)
        if stored is None:
            conn.send(protocol.error_message("not-found", "name"))
            return
        wall = self.clock.wall_ms()
        end = stored["end_ms"]
        if stored["kind"] != "unknown" or (end is not None and wall - end > NAME_AFTER_MS):
            conn.send(protocol.error_message("bad-turn", "name"))
            return
        _, row = self._member_row(conn, stored["room_id"])
        if row is None:
            conn.send(protocol.error_message("not-member", "name"))
            return
        if row.away:
            conn.send(protocol.error_message("away", "name"))
            return
        if self.store.station_blocked(sid, wall):
            conn.send(protocol.error_message("blocked", "name"))
            return
        room, turn = self._find_turn(stored["room_id"], turn_id)
        during = turn.members_during if turn is not None else self.store.member_ids(stored)
        if sid not in during:
            conn.send(protocol.error_message("not-allowed-name", "name"))
            return
        now = self.clock.now_ms()
        retry = self.name_window.retry_after(sid, now)
        if retry:
            conn.send(protocol.error_message("rate", "name", retry))
            return
        self.name_window.allow(sid, now)
        name = protocol.normalize_callsign(msg["name"])
        self.store.turn_name(turn_id, sid, name, row.callsign, wall)
        if turn is not None and turn.set_name(sid, name) and room is not None and room.live:
            # The turn's names change in the room's views.
            self.book.touch(room)

    def _report(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.7: onto the review list; answered by nothing."""
        sid = conn.station_id
        assert sid is not None
        target = msg["target"]
        reported: Optional[str] = None
        if target == "message":
            key: Any = msg["msg"]
            found = self.store.message_get(key) is not None
        elif target == "turn":
            key = msg["turn"]
            found = self.store.turn_get(key) is not None
        else:
            key = (msg["room"], msg["ref"])
            room = self.book.live(msg["room"])
            row = next((r for r in room.rows.values() if r.ref == msg["ref"]), None) if room is not None else None
            found = row is not None
            reported = row.station_id if row is not None else None
        if not found:
            conn.send(protocol.error_message("not-found", "report"))
            return
        now = self.clock.now_ms()
        retry = self.report_window.retry_after(sid, now)
        if retry:
            conn.send(protocol.error_message("rate", "report", retry))
            return
        self.report_window.allow(sid, now)
        self.store.report_add(target, key, msg["reason"], sid, self.clock.wall_ms(), reported)

    def _mentions_seen(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.11: answered by nothing."""
        assert conn.station_id is not None
        self.store.mention_seen(conn.station_id, msg["t"])

    def _hidden(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.14: the station's presence ends at once, and the
        connection gets nothing more but answers."""
        sid = conn.station_id
        assert sid is not None
        log.info("station %s went hidden", short(sid))
        conn.hidden = True
        conn.presence = None
        for room_id in list(conn.views):
            self._drop_view(conn, room_id)
        conn.reads.clear()
        conn.want_full = []
        conn.band = None
        if conn.band_timer is not None:
            conn.band_timer.cancel()
            conn.band_timer = None
        # Its rows leave every room (a delta to every other viewer); its
        # open talking turn ends now with no 3-second wait (6.6), and its
        # joining slices are dropped.
        self.book.remove_station(sid)

    # ----------------------------------------------------------- live

    def _watch_band(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.5: the band list of one band, or of all; a new
        `watchBand` replaces the old."""
        conn.watch_band = msg["band"]
        self._send_rooms(conn)

    def _watch(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.5: a room's view, at most 4 at once."""
        room = self.book.live(msg["room"])
        if room is None:
            conn.send(protocol.error_message("not-found", "watch", receiver=LIVE))
            return
        if room.id not in conn.watched:
            if len(conn.watched) >= self.config.watches_per_live_socket:
                conn.send(protocol.error_message("not-allowed-watch", "watch", receiver=LIVE))
                return
            conn.watched.add(room.id)
        self._owe_full(conn, room.id)

    def _unwatch(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.5: answered by nothing."""
        conn.watched.discard(msg["room"])
        self._drop_view(conn, msg["room"])

    def _live_resync(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.5: the watched band list, or one watched room's view,
        generation 1; `not-found` for what is not watched."""
        room_id = msg["room"]
        if room_id is None:
            if conn.watch_band is None:
                conn.send(protocol.error_message("not-found", "resync", receiver=LIVE))
                return
            self._send_rooms(conn)
            return
        if room_id not in conn.views or self.book.live(room_id) is None:
            conn.send(protocol.error_message("not-found", "resync", receiver=LIVE))
            return
        self._owe_full(conn, room_id)

    # ----------------------------------------------------------- views

    def _in_rooms(self, conn: Connection) -> Set[str]:
        """The live rooms the station has a row in (joining or member)."""
        if conn.station_id is None:
            return set()
        return {row.room.id for row in self.book.rows_of(conn.station_id) if row.room.live}

    def _read(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.8."""
        room = self.book.live(msg["room"])
        if room is None:
            conn.send(protocol.error_message("not-found", "read"))
            return
        inside = self._in_rooms(conn)
        if room.id not in conn.reads and room.id not in inside:
            if len(conn.reads - inside) >= self.config.reads:
                conn.send(protocol.error_message("not-allowed-read", "read"))
                return
            conn.reads.add(room.id)
        self._owe_full(conn, room.id)

    def _unread(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.9: answered by nothing, never refused."""
        room_id = msg["room"]
        conn.reads.discard(room_id)
        if room_id not in self._in_rooms(conn):
            self._drop_view(conn, room_id)

    def _resync(self, conn: Connection, msg: Dict[str, Any]) -> None:
        """Wire spec 5.3.10."""
        room_id = msg["room"]
        if room_id is None:
            self._send_rooms(conn)
            return
        if room_id not in conn.views or self.book.live(room_id) is None:
            conn.send(protocol.error_message("not-found", "resync"))
            return
        self._owe_full(conn, room_id)

    def _owe_full(self, conn: Connection, room_id: str) -> None:
        if room_id not in conn.want_full:
            conn.want_full.append(room_id)
        self._dirty.add(conn)

    def _drop_view(self, conn: Connection, room_id: str) -> None:
        conn.views.pop(room_id, None)
        viewers = self._viewers.get(room_id)
        if viewers is not None:
            viewers.discard(conn)
            if not viewers:
                del self._viewers[room_id]

    def _band_now(self) -> Dict[str, Tuple[Optional[int], Dict[str, Any]]]:
        return {room.id: (room.opening_id, room.summary()) for room in self.book.live_rooms()}

    @staticmethod
    def _band_for(
        conn: Connection, band: Dict[str, Tuple[Optional[int], Dict[str, Any]]]
    ) -> Dict[str, Tuple[Optional[int], Dict[str, Any]]]:
        """The band list as one connection views it: every live room for a
        station, the watched band's for a live reader (5.4.3)."""
        if conn.path != LIVE or conn.watch_band == "all":
            return band
        return {room_id: entry for room_id, entry in band.items() if entry[1]["band"] == conn.watch_band}

    def _send_rooms(self, conn: Connection) -> None:
        """A full band list, generation 1 (5.4.3)."""
        band = self._band_for(conn, self._band_now())
        conn.send(protocol.message("rooms", conn.path, gen=1, rooms=[summary for _, summary in band.values()]))
        conn.band = _View()
        conn.band_sent = band
        conn.band_last_ms = self.clock.now_ms()
        if conn.band_timer is not None:
            conn.band_timer.cancel()
            conn.band_timer = None

    def _band_check(self, conn: Connection, band: Dict[str, Tuple[Optional[int], Dict[str, Any]]]) -> None:
        """Send a `roomsDelta` now, or 2000 ms after the last band-list
        message, when the band list differs from what the connection last
        received (6.11 "Pacing")."""
        view = conn.band
        if view is None or view.stale or conn.band_timer is not None:
            return
        band = self._band_for(conn, band)
        if band == conn.band_sent:
            return
        now = self.clock.now_ms()
        last = conn.band_last_ms
        if last is not None and now - last < BAND_PACING_MS:
            conn.band_timer = self.call_later(last + BAND_PACING_MS - now, lambda: self._band_due(conn))
            return
        self._send_rooms_delta(conn, band)

    def _band_due(self, conn: Connection) -> None:
        conn.band_timer = None
        if conn.closing or conn.band is None or conn.band.stale:
            return
        band = self._band_for(conn, self._band_now())
        if band != conn.band_sent:
            self._send_rooms_delta(conn, band)

    def _send_rooms_delta(self, conn: Connection, band: Dict[str, Tuple[Optional[int], Dict[str, Any]]]) -> None:
        """Wire spec 5.4.4, merged against what the connection last
        received."""
        view = conn.band
        assert view is not None
        sent = conn.band_sent
        add = [summary for room_id, (_, summary) in band.items() if room_id not in sent]
        change = [
            summary
            for room_id, (opening, summary) in band.items()
            if room_id in sent and sent[room_id] != (opening, summary)
        ]
        remove = [room_id for room_id in sent if room_id not in band]
        view.gen += 1
        msg = protocol.message("roomsDelta", conn.path, gen=view.gen, add=add, change=change, remove=remove)
        # A removal is always queued (9.1).
        if not self._send_view(conn, view, msg, droppable=not remove):
            return
        conn.band_sent = band
        conn.band_last_ms = self.clock.now_ms()

    def _send_view(self, conn: Connection, view: _View, msg: Dict[str, Any], droppable: bool) -> bool:
        ok = conn.send(msg, droppable)
        if not ok:
            view.lost = True
        elif view.lost:
            # This one shows the gap; nothing more until resync.
            view.stale = True
        return ok

    def _full_room(self, conn: Connection, room: Room) -> None:
        """A full view of one room, generation 1 (5.4.5)."""
        sid = conn.station_id
        msg = protocol.message(
            "room",
            conn.path,
            gen=1,
            id=room.id,
            opening=room.opening_id,
            band=room.band,
            family=room.family,
            hz=room.center,
            members=[room.member_object(row, row.station_id == sid) for row in room.rows.values()],
            now=[room.turn_object(t, t.given.get(sid or "")) for t in room.now_turns()],
            earlier=[room.turn_object(t, t.given.get(sid or "")) for t in room.earlier_turns()],
            messages=room.recent_messages(),
        )
        conn.send(msg)
        conn.views[room.id] = _View(room.opening_id)
        self._viewers.setdefault(room.id, set()).add(conn)

    def _delta(self, conn: Connection, room: Room, diff: RoomDiff) -> None:
        """One change to one room in the connection's view (5.4.6)."""
        view = conn.views.get(room.id)
        if view is None:
            return
        sid = conn.station_id
        if not view.stale or diff.closed:
            view.gen += 1
            members = [dict(obj, you=True) if row.station_id == sid else obj for row, obj in diff.members]
            msg = protocol.message(
                "delta",
                conn.path,
                room=room.id,
                gen=view.gen,
                members=members,
                left=[row.ref for row in diff.left],
                turns=diff.turns_for(sid),
                messages=diff.messages,
                closed=diff.closed,
            )
            # A closing delta is always queued (9.1).
            self._send_view(conn, view, msg, droppable=not diff.closed)
        if diff.closed:
            conn.reads.discard(room.id)
            conn.watched.discard(room.id)
            self._drop_view(conn, room.id)
        elif room.id not in conn.reads and any(row.station_id == sid for row in diff.left):
            if room.id not in self._in_rooms(conn):
                # Its own row left and it does not read the room (6.11).
                self._drop_view(conn, room.id)

    def _flush(self) -> None:
        """The end of an event: send what it changed, connection by
        connection, in the order of 6.11. A send that closes a connection
        changes rooms again, so this runs until nothing is left."""
        self._depth += 1
        try:
            for _ in range(100):
                rooms = self.book.take_touched()
                if not rooms and not self._dirty:
                    break
                self._flush_once(rooms)
            mentions, self._mentions = self._mentions, []
            for conn, mention in mentions:
                if not self.stopping and not conn.hidden:
                    conn.send(mention)
        finally:
            self._depth -= 1

    def _flush_once(self, rooms: List[Room]) -> None:
        diffs: List[Tuple[Room, RoomDiff]] = []
        band_changed = False
        for room in rooms:
            diff = room.diff()
            if diff is None:
                continue
            band_changed = band_changed or diff.band or diff.closed
            if diff:
                diffs.append((room, diff))
        dirty = self._dirty
        self._dirty = set()
        if self.stopping:
            return
        targets: List[Connection] = []
        seen: Set[Connection] = set()

        def want(conn: Optional[Connection]) -> None:
            if conn is None or conn in seen or conn.closing or conn.hidden:
                return
            if conn.station_id is not None or conn.path == LIVE:
                seen.add(conn)
                targets.append(conn)

        if band_changed:
            for conn in self.stations.values():
                want(conn)
            for conn in list(self.connections):
                if conn.path == LIVE and conn.band is not None:
                    want(conn)
        for conn in dirty:
            want(conn)
        for room, diff in diffs:
            for conn in list(self._viewers.get(room.id, ())):
                want(conn)
            for row, _ in diff.members:
                want(self.stations.get(row.station_id))
        band = self._band_now() if band_changed else None
        for conn in targets:
            if band is not None:
                self._band_check(conn, band)
            fresh: Set[str] = set()
            owed = list(conn.want_full)
            conn.want_full = []
            for room_id in sorted(self._in_rooms(conn)):
                view = conn.views.get(room_id)
                room = self.book.live(room_id)
                if room is not None and (view is None or view.opening != room.opening_id) and room_id not in owed:
                    owed.append(room_id)
            for room_id in owed:
                room = self.book.live(room_id)
                if room is None:
                    continue
                self._full_room(conn, room)
                fresh.add(room_id)
            for room, diff in diffs:
                if room.id not in fresh:
                    self._delta(conn, room, diff)


class _EventClock:
    """The service's clock for the rooms: each timer runs as one event."""

    def __init__(self, service: Service) -> None:
        self._service = service

    def now_ms(self) -> int:
        return self._service.clock.now_ms()

    def wall_ms(self) -> int:
        return self._service.clock.wall_ms()

    def call_later(self, delay_ms: int, callback: Callable[[], None]) -> TimerHandle:
        return self._service.call_later(delay_ms, callback)
