# no-port-check: NereusSDR-original.
"""The WebSocket relay: datagrams between the two legs of one relay grant.

Rendezvous document section 12 is the authority. Each end (the Core and
the device) opens one WebSocket leg and joins it with the token of its
relay grant; the relay pairs the two legs of one grant's session and
forwards every data frame from one leg to the other. A data frame is one
ICE, DTLS, SCTP or SRTP datagram behind a one-byte stream tag. The relay
reads the tag and the length and nothing else: every reliable or secure
layer runs end to end, and what the relay carries is ciphertext.

Nothing here writes to disk. Everything lives in memory; a restart forgets
every session, and a leg whose grant has not yet expired simply joins
again.

Traffic runs in two lanes, one per stream tag (1 control, 2 media): each
lane has its own rate cap each way and, towards each leg, its own bounded
queue that drops its oldest frame when a new one would not fit. A leg's
writer takes the lanes in turn, so a burst in one lane never spends the
other's budget, evicts its datagrams or holds them back (section 12.5).
Lanes, caps and queues belong to the session, so a leg that joins again
keeps them. A leg that reads slowly loses its oldest datagrams (which SCTP
sees as loss) instead of growing memory or holding up the other leg.
"""

from __future__ import annotations

import asyncio
import collections
import datetime
import logging
from typing import Any, Deque, Dict, Optional, Set, Tuple

from nereus_rendezvous import relaygrant
from nereus_rendezvous.clock import RealClock, TimerHandle
from nereus_rendezvous.limits import address_group

from .config import MAX_MESSAGE_BYTES, Config, check

log = logging.getLogger("nereus_relay")

# Section 12.3: the frame's first byte.
TAG_CONTROL = 0x01
TAG_MEDIA = 0x02
TAG_WATCH = 0x03
# Primary lanes remain unchanged. Tags 3 through 0x7F are reserved on
# primary legs; only a watch leg may carry tag 3.
LANES = (TAG_CONTROL, TAG_MEDIA)
DATA_TAG_MAX = 0x7F
TAG_JOIN = 0x80
TAG_READY = 0x81
TAG_PEER = 0x82
TAG_END = 0x83
FRAME_VERSION = 1

CLOSE_NORMAL = 1000
CLOSE_GOING_AWAY = 1001
CLOSE_NOT_READING = 1008

# Section 12.4: why the relay ends a leg (the END frame's text).
END_CODES = (
    "protocolError",
    "timeout",
    "badToken",
    "expired",
    "ended",
    "full",
    "tooManyConnections",
    "tooManySessions",
    "replaced",
    "peerGone",
    "idle",
    "shuttingDown",
)

LEG_NAMES = {relaygrant.LEG_CORE: "core", relaygrant.LEG_DEVICE: "device"}


def short_session(session: bytes) -> str:
    """Sessions in logs: six characters of the id, never the whole."""
    return relaygrant.to_b64url(session)[:6]


def other_side(side: int) -> int:
    return relaygrant.LEG_DEVICE if side == relaygrant.LEG_CORE else relaygrant.LEG_CORE


class DropOldestQueue:
    """A bounded queue of frames. A push that would pass either cap drops
    the oldest frames until the new one fits, so what waits is always the
    newest, and never more than max_frames frames or max_bytes bytes."""

    def __init__(self, max_frames: int, max_bytes: int) -> None:
        self.max_frames = max_frames
        self.max_bytes = max_bytes
        self._frames: Deque[Any] = collections.deque()
        self.bytes = 0
        self.dropped = 0
        self.peak_bytes = 0

    def push(self, frame: Any) -> int:
        """Queue a frame; returns how many older frames were dropped."""
        size = len(frame)
        dropped = 0
        while self._frames and (len(self._frames) >= self.max_frames or self.bytes + size > self.max_bytes):
            old = self._frames.popleft()
            self.bytes -= len(old)
            dropped += 1
        self._frames.append(frame)
        self.bytes += size
        self.dropped += dropped
        if self.bytes > self.peak_bytes:
            self.peak_bytes = self.bytes
        return dropped

    def pop(self) -> Optional[Any]:
        if not self._frames:
            return None
        frame = self._frames.popleft()
        self.bytes -= len(frame)
        return frame

    def clear(self) -> None:
        self._frames.clear()
        self.bytes = 0

    def __len__(self) -> int:
        return len(self._frames)


class TokenBucket:
    """At most `rate` bytes a second on average, with bursts of up to one
    second's worth (section 12.5). A frame that does not fit is dropped,
    not delayed."""

    def __init__(self, rate: int, now_ms: int) -> None:
        self.rate = rate
        self.capacity = float(rate)
        self.tokens = float(rate)
        self.last_ms = now_ms

    def allow(self, size: int, now_ms: int) -> bool:
        elapsed = max(0, now_ms - self.last_ms)
        self.last_ms = now_ms
        self.tokens = min(self.capacity, self.tokens + elapsed * self.rate / 1000.0)
        if self.tokens < size:
            return False
        self.tokens -= size
        return True


class DataUse:
    """The relay's data-use count (section 12.6): bytes forwarded, by UTC
    day, with a journal line when a day ends, as the server's data-use
    report does for the whole host (rendezvous/deploy/data-use.py)."""

    def __init__(self, wall_seconds: int) -> None:
        self.started = wall_seconds
        self.day = self._day_of(wall_seconds)
        self.day_bytes = 0
        self.day_sessions = 0
        self.total_bytes = 0

    @staticmethod
    def _day_of(ts: int) -> str:
        return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m-%d")

    def add(self, size: int) -> None:
        self.day_bytes += size
        self.total_bytes += size

    def session(self) -> None:
        self.day_sessions += 1

    def line(self) -> str:
        return "relay data use: %.2f GB forwarded on %s (UTC) in %d sessions; %.2f GB since the relay started" % (
            self.day_bytes / 1e9,
            self.day,
            self.day_sessions,
            self.total_bytes / 1e9,
        )

    def roll(self, wall_seconds: int) -> Optional[str]:
        """The finished day's line once the UTC day has changed, else None."""
        today = self._day_of(wall_seconds)
        if today == self.day:
            return None
        line = self.line()
        self.day = today
        self.day_bytes = 0
        self.day_sessions = 0
        return line


class _Close:
    def __init__(self, code: int) -> None:
        self.code = code


class Session:
    def __init__(self, sid: bytes, station: bytes, expires: int, now_ms: int, config: Config) -> None:
        self.sid = sid
        self.station = station
        self.expires = expires
        self.started_ms = now_ms
        # Section 12.5: a rate cap for each (sending side, lane) and a queue
        # for each (receiving side, lane), kept by the session so a leg that
        # joins again keeps them.
        self.buckets: Dict[Tuple[int, int], TokenBucket] = {
            (side, lane): TokenBucket(config.rate_bytes_per_second, now_ms)
            for side in relaygrant.LEGS
            for lane in LANES
        }
        self.queues: Dict[Tuple[int, int], DropOldestQueue] = {
            (side, lane): DropOldestQueue(config.queue_frames, config.queue_bytes)
            for side in relaygrant.LEGS
            for lane in LANES
        }
        self.legs: Dict[int, Optional["Leg"]] = {relaygrant.LEG_CORE: None, relaygrant.LEG_DEVICE: None}
        self.watch_legs: Dict[int, Optional["Leg"]] = {side: None for side in relaygrant.LEGS}
        self.watch_buckets: Dict[int, TokenBucket] = {
            side: TokenBucket(config.watch_rate_bytes_per_second, now_ms) for side in relaygrant.LEGS
        }
        self.watch_queues: Dict[int, DropOldestQueue] = {
            side: DropOldestQueue(config.watch_queue_frames, config.watch_queue_bytes) for side in relaygrant.LEGS
        }
        # A side that has not joined yet, or has left, has this long to come
        # (back); idleness counts only while both legs are present.
        self.away: Dict[int, Optional[TimerHandle]] = {relaygrant.LEG_CORE: None, relaygrant.LEG_DEVICE: None}
        self.idle: Optional[TimerHandle] = None
        self.last_activity_ms = now_ms
        self.forwarded_bytes = 0
        self.forwarded_frames = 0
        self.dropped_rate = 0
        self.dropped_queue = 0
        self.dropped_no_peer = 0
        self.dropped_unknown = 0
        self.ended = False

    def clear_side(self, side: int) -> None:
        for lane in LANES:
            self.queues[(side, lane)].clear()

    def clear_watch_side(self, side: int) -> None:
        self.watch_queues[side].clear()

    def queued(self, side: int) -> bool:
        return any(len(self.queues[(side, lane)]) for lane in LANES)


class Leg:
    """One WebSocket connection to the relay: waiting until it joins, then
    one side of a session."""

    def __init__(self, relay: "Relay", transport: Any, address: str) -> None:
        self.relay = relay
        self.transport = transport
        self.group = address_group(address)
        self.session: Optional[Session] = None
        self.side = 0
        self.watch = False
        self.joined = False
        self.counted = False
        self.control: Deque[Any] = collections.deque()
        self.next_lane = 0
        self.wake = asyncio.Event()
        self.closing = False
        self.aborted = False
        self.writer: Optional["asyncio.Future[None]"] = None
        self.timer: Optional[TimerHandle] = None
        # When the send under way started, and the one timer that checks it
        # (armed lazily, section 12.5).
        self.sending_since: Optional[int] = None
        self.stall_timer: Optional[TimerHandle] = None

    def send_control(self, frame: bytes) -> None:
        if self.closing:
            return
        # A blocked watch writer can otherwise collect unbounded PEER notices
        # as watch sockets repeatedly join and leave before the stall timer.
        if self.watch and frame[0] == TAG_PEER and len(self.control) >= 16:
            for old in self.control:
                if isinstance(old, bytes) and old[0] == TAG_PEER:
                    self.control.remove(old)
                    break
            else:
                return
        self.control.append(frame)
        self.wake.set()

    def end(self, code: str, close_code: int = CLOSE_NORMAL) -> None:
        """Send END with its code, then close. Whatever data waits for this
        connection is no use to it any more."""
        if self.closing:
            return
        self.control.append(bytes([TAG_END]) + code.encode("ascii"))
        self.closing = True
        self.control.append(_Close(close_code))
        self.wake.set()
        self.relay.detach(self)

    def cancel_timer(self) -> None:
        if self.timer is not None:
            self.timer.cancel()
            self.timer = None

    def stopped_reading(self) -> None:
        """One send has taken longer than send_stall_ms: the peer is not
        reading at all. The connection goes (1008, no END: it would not read
        one) and its place in the session waits for a rejoin."""
        if self.aborted:
            return
        self.aborted = True
        self.closing = True
        self.control.clear()
        if self.writer is not None:
            self.writer.cancel()
        asyncio.ensure_future(self._close_quietly(CLOSE_NOT_READING))
        self.relay.detach(self)

    async def _close_quietly(self, code: int) -> None:
        try:
            await self.transport.close(code)
        except Exception:  # noqa: BLE001 - the peer is gone either way
            return

    def _check_stall(self) -> None:
        self.stall_timer = None
        started = self.sending_since
        if started is None:
            return
        stall = self.relay.config.send_stall_ms
        waited = self.relay.clock.now_ms() - started
        if waited >= stall:
            self.stopped_reading()
            return
        self.stall_timer = self.relay.clock.call_later(stall - waited, self._check_stall)

    def _next_data(self) -> Optional[Any]:
        """The next datagram for this leg, the lanes taken in turn."""
        session = self.session
        if session is None or self.closing:
            return None
        if self.watch:
            if session.watch_legs.get(self.side) is not self:
                return None
            return session.watch_queues[self.side].pop()
        if session.legs.get(self.side) is not self:
            return None
        for i in range(len(LANES)):
            lane = LANES[(self.next_lane + i) % len(LANES)]
            frame = session.queues[(self.side, lane)].pop()
            if frame is not None:
                self.next_lane = (self.next_lane + i + 1) % len(LANES)
                return frame
        return None

    async def write_loop(self) -> None:
        try:
            while True:
                if self.control:
                    item = self.control.popleft()
                else:
                    item = self._next_data()
                    if item is None:
                        self.wake.clear()
                        await self.wake.wait()
                        continue
                if isinstance(item, _Close):
                    await self.transport.close(item.code)
                    return
                self.sending_since = self.relay.clock.now_ms()
                if self.stall_timer is None:
                    self.stall_timer = self.relay.clock.call_later(self.relay.config.send_stall_ms, self._check_stall)
                await self.transport.send(item)
                self.sending_since = None
        except Exception:  # noqa: BLE001 - the peer went away; the reader cleans up
            return
        finally:
            self.sending_since = None
            if self.stall_timer is not None:
                self.stall_timer.cancel()
                self.stall_timer = None


class Relay:
    def __init__(self, config: Config, clock: Any = None) -> None:
        check(config)
        if not config.relay_secret:
            raise ValueError("the relay needs its secret")
        self.config = config
        self.clock = clock or RealClock()
        self.legs: Set[Leg] = set()
        # Connections that have not joined and are not closing, oldest
        # first; the oldest makes room when a new one comes and they are
        # at max_pending (section 12.5).
        self.waiting: "collections.OrderedDict[Leg, None]" = collections.OrderedDict()
        # Every connection per address group, from its arrival until its
        # handler has finished, refused and closing ones included.
        self.per_group: Dict[str, int] = {}
        self.sessions: Dict[bytes, Session] = {}
        # Live sessions per station value (section 12.5).
        self.per_station: Dict[bytes, int] = {}
        # Sessions that have ended, until their grant's expiry: a token of
        # an ended session never opens a new one (section 12.4).
        self.spent: Dict[bytes, int] = {}
        self.data_use = DataUse(self.clock.wall_seconds())
        # Counters the conformance runner reads to know the relay has caught
        # up with what it sent; nothing else uses them.
        self.frames_handled = 0
        self.accepted = 0
        self.finished = 0
        self._day_timer: Optional[TimerHandle] = None

    @property
    def pending(self) -> int:
        return len(self.waiting)

    # ----------------------------------------------------------- lifecycle

    def start_day_timer(self) -> None:
        """Checks once an hour whether the UTC day has changed, to log the
        day's data use."""

        def tick() -> None:
            line = self.data_use.roll(self.clock.wall_seconds())
            if line is not None:
                log.info("%s", line)
            self._day_timer = self.clock.call_later(3600000, tick)

        self._day_timer = self.clock.call_later(3600000, tick)

    async def run_connection(self, transport: Any, address: str) -> None:
        leg = Leg(self, transport, address)
        writer = asyncio.ensure_future(leg.write_loop())
        leg.writer = writer
        self.accept(leg)
        try:
            async for frame in transport:
                if not leg.closing:
                    try:
                        self.on_frame(leg, frame)
                    except Exception as exc:  # noqa: BLE001
                        # A bug, not the peer's doing. The class name only.
                        log.error("a frame handler failed (%s); closing its connection", type(exc).__name__)
                        self.frames_handled += 1
                        break
                self.frames_handled += 1
        except Exception as exc:  # noqa: BLE001 - any transport end is an end
            log.debug("a relay connection ended: %s", type(exc).__name__)
        finally:
            self.detach(leg)
            if leg.closing and not writer.done():
                await asyncio.wait({writer}, timeout=5)
            if not writer.done():
                writer.cancel()
                await asyncio.wait({writer})
            self._uncount(leg)
            self.finished += 1

    def accept(self, leg: Leg) -> None:
        # Counted from now until its handler ends, refused or not, so a
        # refused connection still closing holds its place (section 12.5).
        self.accepted += 1
        before = self.per_group.get(leg.group, 0)
        leg.counted = True
        self.per_group[leg.group] = before + 1
        self.legs.add(leg)
        if before >= self.config.connections_per_address:
            leg.end("tooManyConnections")
            return
        while len(self.waiting) >= self.config.max_pending:
            oldest = next(iter(self.waiting))
            oldest.end("full")
        self.waiting[leg] = None
        leg.timer = self.clock.call_later(self.config.join_timeout_ms, lambda: leg.end("timeout"))

    def _uncount(self, leg: Leg) -> None:
        if not leg.counted:
            return
        leg.counted = False
        left = self.per_group.get(leg.group, 1) - 1
        if left > 0:
            self.per_group[leg.group] = left
        else:
            self.per_group.pop(leg.group, None)

    def shutdown(self) -> None:
        for session in list(self.sessions.values()):
            self._retire_watches(session, "shuttingDown", CLOSE_GOING_AWAY)
        for leg in list(self.legs):
            leg.end("shuttingDown", close_code=CLOSE_GOING_AWAY)

    def _retire_watches(self, session: Session, code: str, close_code: int = CLOSE_NORMAL) -> None:
        # Remove ownership before calling end(): detach may run synchronously.
        for side in relaygrant.LEGS:
            watch = session.watch_legs[side]
            session.watch_legs[side] = None
            session.clear_watch_side(side)
            if watch is not None:
                watch.session = None
                watch.end(code, close_code)

    def detach(self, leg: Leg) -> None:
        """Forget a connection's part in the relay. A joined leg leaves its
        session's place open for a rejoin (section 12.4). Safe to call more
        than once. Its count per address group lasts until its handler
        ends (_uncount)."""
        leg.cancel_timer()
        self.legs.discard(leg)
        self.waiting.pop(leg, None)
        session = leg.session
        leg.session = None
        if session is None or session.ended:
            return
        if leg.watch:
            if session.watch_legs.get(leg.side) is not leg:
                return
            session.watch_legs[leg.side] = None
            session.clear_watch_side(leg.side)
            other_watch = session.watch_legs[other_side(leg.side)]
            if other_watch is not None:
                other_watch.send_control(bytes([TAG_PEER, 0]))
            return
        if session.legs.get(leg.side) is not leg:
            return
        self._retire_watches(session, "peerGone")
        session.legs[leg.side] = None
        session.clear_side(leg.side)
        log.info("relay session %s: %s leg left", short_session(session.sid), LEG_NAMES[leg.side])
        self._stop_idle(session)
        self._start_away(session, leg.side)
        other = session.legs[other_side(leg.side)]
        if other is not None:
            other.send_control(bytes([TAG_PEER, 0]))

    # ----------------------------------------------------------- frames

    def on_frame(self, leg: Leg, frame: Any) -> None:
        # Section 12.3: binary frames only; the tag is the first byte.
        if not isinstance(frame, (bytes, bytearray, memoryview)) or len(frame) == 0:
            leg.end("protocolError")
            return
        tag = frame[0]
        if not leg.joined:
            if tag != TAG_JOIN:
                leg.end("protocolError")
                return
            self._join(leg, bytes(frame[1:]))
            return
        if tag == 0 or tag > DATA_TAG_MAX or len(frame) < 2:
            leg.end("protocolError")
            return
        if leg.watch and (tag != TAG_WATCH or len(frame) > MAX_MESSAGE_BYTES):
            leg.end("protocolError")
            return
        self._forward(leg, tag, frame)

    def _forward(self, leg: Leg, tag: int, frame: Any) -> None:
        """The whole of what the relay does with a datagram: it has read the
        tag (above) and takes the length; the frame goes on as it came."""
        session = leg.session
        if session is None:
            return
        if leg.watch:
            other_s = other_side(leg.side)
            other = session.watch_legs[other_s]
            if other is None or other.closing:
                session.dropped_no_peer += 1
                return
            size = len(frame)
            now = self.clock.now_ms()
            if not session.watch_buckets[leg.side].allow(size, now):
                session.dropped_rate += 1
                return
            if not session.buckets[(leg.side, TAG_CONTROL)].allow(size, now):
                session.dropped_rate += 1
                return
            session.dropped_queue += session.watch_queues[other_s].push(frame)
            other.wake.set()
            session.forwarded_bytes += size
            session.forwarded_frames += 1
            self.data_use.add(size)
            return
        if tag not in LANES:
            session.dropped_unknown += 1
            return
        size = len(frame)
        other_s = other_side(leg.side)
        other = session.legs[other_s]
        if other is None or other.closing:
            session.dropped_no_peer += 1
            return
        now = self.clock.now_ms()
        if not session.buckets[(leg.side, tag)].allow(size, now):
            session.dropped_rate += 1
            return
        session.dropped_queue += session.queues[(other_s, tag)].push(frame)
        other.wake.set()
        session.forwarded_bytes += size
        session.forwarded_frames += 1
        self.data_use.add(size)
        session.last_activity_ms = now

    # ----------------------------------------------------------- joining

    def _join(self, leg: Leg, token_bytes: bytes) -> None:
        try:
            token = token_bytes.decode("ascii")
        except UnicodeDecodeError:
            token = ""
        secret = self.config.relay_secret or b""
        grant = relaygrant.verify(secret, token)
        if grant is None:
            grant = relaygrant.verify_watch(secret, token)
        if grant is None:
            leg.end("badToken")
            return
        now_wall = self.clock.wall_seconds()
        session = self.sessions.get(grant.session)
        if grant.purpose == relaygrant.PURPOSE_WATCH:
            if session is None or session.ended or any(
                session.legs[side] is None or session.legs[side].closing for side in relaygrant.LEGS
            ):
                leg.end("peerGone")
                return
            if session.station != grant.station or session.expires != grant.expires:
                leg.end("badToken")
                return
            side = grant.leg
            previous = session.watch_legs[side]
            if previous is not None and previous is not leg:
                session.watch_legs[side] = None
                session.clear_watch_side(side)
                previous.session = None
                previous.end("replaced")
            leg.cancel_timer()
            self.waiting.pop(leg, None)
            leg.joined = True
            leg.watch = True
            leg.session = session
            leg.side = side
            session.watch_legs[side] = leg
            other = session.watch_legs[other_side(side)]
            leg.send_control(bytes([TAG_READY, FRAME_VERSION, 1 if other is not None else 0]))
            if other is not None:
                other.send_control(bytes([TAG_PEER, 1]))
            return
        if session is None:
            self._prune_spent(now_wall)
            if grant.session in self.spent:
                leg.end("ended")
                return
            if grant.expires < now_wall:
                leg.end("expired")
                return
            if len(self.sessions) >= self.config.slots:
                leg.end("full")
                return
            if self.per_station.get(grant.station, 0) >= self.config.sessions_per_station:
                leg.end("tooManySessions")
                return
            session = Session(grant.session, grant.station, grant.expires, self.clock.now_ms(), self.config)
            self.sessions[grant.session] = session
            self.per_station[grant.station] = self.per_station.get(grant.station, 0) + 1
            self.data_use.session()
            log.info("relay session %s opened", short_session(session.sid))
            for side in relaygrant.LEGS:
                self._start_away(session, side)
        side = grant.leg
        previous = session.legs[side]
        if previous is not None and previous is not leg:
            self._retire_watches(session, "replaced")
            # A leg that joins again while its older connection still looks
            # alive (a reset the relay has not seen yet) takes its place.
            previous.session = None
            previous.end("replaced")
            log.info("relay session %s: %s leg replaced", short_session(session.sid), LEG_NAMES[side])
        leg.cancel_timer()
        self.waiting.pop(leg, None)
        leg.joined = True
        leg.session = session
        leg.side = side
        session.legs[side] = leg
        timer = session.away[side]
        if timer is not None:
            timer.cancel()
            session.away[side] = None
        other = session.legs[other_side(side)]
        log.info("relay session %s: %s leg joined", short_session(session.sid), LEG_NAMES[side])
        leg.send_control(bytes([TAG_READY, FRAME_VERSION, 1 if other is not None else 0]))
        if session.queued(side):
            leg.wake.set()
        if other is not None:
            other.send_control(bytes([TAG_PEER, 1]))
            session.last_activity_ms = self.clock.now_ms()
            self._arm_idle(session)

    # ----------------------------------------------------------- timers

    def _arm_idle(self, session: Session) -> None:
        """One timer for idleness, checked lazily: when it fires it looks at
        the last activity and sets itself again for what remains, so a busy
        session costs one timer an idle period, not one a datagram."""
        if session.idle is not None or session.ended:
            return
        if not all(session.legs[s] is not None for s in relaygrant.LEGS):
            return
        remaining = session.last_activity_ms + self.config.idle_timeout_ms - self.clock.now_ms()
        session.idle = self.clock.call_later(max(0, remaining), lambda: self._check_idle(session))

    def _check_idle(self, session: Session) -> None:
        session.idle = None
        if session.ended or not all(session.legs[s] is not None for s in relaygrant.LEGS):
            return
        if self.clock.now_ms() - session.last_activity_ms >= self.config.idle_timeout_ms:
            self.end_session(session, "idle")
            return
        self._arm_idle(session)

    def _stop_idle(self, session: Session) -> None:
        if session.idle is not None:
            session.idle.cancel()
            session.idle = None

    def _start_away(self, session: Session, side: int) -> None:
        if session.legs[side] is not None or session.away[side] is not None:
            return
        session.away[side] = self.clock.call_later(self.config.rejoin_ms, lambda: self.end_session(session, "peerGone"))

    def _prune_spent(self, now_wall: int) -> None:
        for sid in [s for s, exp in self.spent.items() if exp < now_wall]:
            del self.spent[sid]

    def end_session(self, session: Session, code: str) -> None:
        if session.ended:
            return
        session.ended = True
        self._retire_watches(session, code)
        self._stop_idle(session)
        for side in relaygrant.LEGS:
            timer = session.away[side]
            if timer is not None:
                timer.cancel()
                session.away[side] = None
            session.clear_side(side)
        if self.sessions.get(session.sid) is session:
            del self.sessions[session.sid]
            left = self.per_station.get(session.station, 1) - 1
            if left > 0:
                self.per_station[session.station] = left
            else:
                self.per_station.pop(session.station, None)
        self.spent[session.sid] = session.expires
        seconds = (self.clock.now_ms() - session.started_ms) // 1000
        log.info(
            "relay session %s ended (%s) after %d s: %d frames, %.2f MB forwarded; dropped %d over the rate, "
            "%d from full queues, %d with no peer, %d with a tag it does not carry",
            short_session(session.sid),
            code,
            seconds,
            session.forwarded_frames,
            session.forwarded_bytes / 1e6,
            session.dropped_rate,
            session.dropped_queue,
            session.dropped_no_peer,
            session.dropped_unknown,
        )
        for side in relaygrant.LEGS:
            leg = session.legs[side]
            session.legs[side] = None
            if leg is not None:
                leg.session = None
                leg.end(code)
