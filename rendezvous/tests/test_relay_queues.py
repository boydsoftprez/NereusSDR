# no-port-check: NereusSDR-original.
"""The relay's queues and what it reads of a frame (rendezvous document
section 12.5): a slow reader never grows the relay's memory and loses its
oldest datagrams first, and the relay reads a datagram's tag and length
and nothing more."""

import asyncio
import logging
import struct

from nereus_rendezvous.clock import ManualClock
from nereus_relay import transport as relay_transport
from nereus_relay.relay import TAG_CONTROL, TAG_JOIN, TAG_MEDIA, TAG_PEER, DropOldestQueue, Leg, Relay
from nereus_rendezvous import relaygrant
from relay_helpers import SECRET, WALL, grant_pair, join, live_relay, make_relay_config, recv, settle


def test_drop_oldest_queue_keeps_the_newest_within_both_caps():
    queue = DropOldestQueue(max_frames=8, max_bytes=10000)
    for n in range(100):
        queue.push(struct.pack(">I", n) + bytes(996))
        assert len(queue) <= 8 and queue.bytes <= 10000
    kept = []
    while len(queue):
        kept.append(struct.unpack(">I", queue.pop()[:4])[0])
    assert kept == list(range(92, 100))
    assert queue.dropped == 92 and queue.peak_bytes == 8000
    # The byte cap binds before the frame cap: 1500-byte frames, 6 fit.
    queue = DropOldestQueue(max_frames=64, max_bytes=9000)
    for n in range(20):
        queue.push(struct.pack(">I", n) + bytes(1496))
    kept = []
    while len(queue):
        kept.append(struct.unpack(">I", queue.pop()[:4])[0])
    assert kept == list(range(14, 20)) and queue.peak_bytes <= 9000


def test_a_slow_reader_never_grows_memory_and_loses_the_oldest_first():
    """The Core's leg stops reading while the device sends 3000 datagrams
    as fast as it can. The relay's queue for that leg never holds more than
    its cap, the library's write buffer stays at its limit, and when the
    reader comes back what it gets is in order, ends with the newest, and
    is exactly what was sent less what the queue dropped."""
    total = 3000

    async def go():
        async with live_relay(rate_bytes_per_second=10 ** 9, queue_frames=64, queue_bytes=24576) as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            # max_queue 1: the reader's own library holds one message and
            # then stops reading its socket, as a stalled app would.
            core, _ = await join(uri, core_token, max_queue=1)
            device, _ = await join(uri, device_token)
            core_leg = next(leg for leg in relay.legs if leg.side == 1)
            ws_transport = core_leg.transport.ws.transport
            queue = core_leg.session.queues[(1, TAG_MEDIA)]
            peak_buffer = 0
            for n in range(total):
                await device.send(bytes([TAG_MEDIA]) + struct.pack(">I", n) + bytes(995))
                if n % 50 == 0:
                    await asyncio.sleep(0)
                    peak_buffer = max(peak_buffer, ws_transport.get_write_buffer_size())
            await settle(relay, 2 + total)
            peak_buffer = max(peak_buffer, ws_transport.get_write_buffer_size())
            assert queue.peak_bytes <= 24576 and len(queue) <= 64
            assert queue.dropped > 0
            assert peak_buffer <= relay_transport.WRITE_LIMIT_BYTES + 2 * 1024
            session = next(iter(relay.sessions.values()))
            dropped = session.dropped_queue
            assert dropped == queue.dropped
            got = []
            while True:
                try:
                    frame = await asyncio.wait_for(core.recv(), 1.0)
                except asyncio.TimeoutError:
                    break
                if frame[0] == TAG_PEER:
                    continue
                got.append(struct.unpack(">I", frame[1:5])[0])
            assert got == sorted(got) and len(set(got)) == len(got)
            assert got[-1] == total - 1
            assert len(got) + dropped == total
            # The frames still queued when the reader came back were the
            # newest: the last run of what arrived is contiguous up to the
            # end and at least as long as what the queue held.
            run = 1
            while run < len(got) and got[-run - 1] == got[-run] - 1:
                run += 1
            assert run >= 16

    asyncio.run(go())


class _Opaque(bytes):
    """A datagram that records every way it is read beyond its first byte
    and its length."""

    touched: list = []

    def __getitem__(self, index):
        if index != 0:
            _Opaque.touched.append(("getitem", index))
        return bytes.__getitem__(self, index)

    def __iter__(self):
        _Opaque.touched.append(("iter",))
        return bytes.__iter__(self)

    def __contains__(self, item):
        _Opaque.touched.append(("contains",))
        return bytes.__contains__(self, item)

    def __eq__(self, other):
        _Opaque.touched.append(("eq",))
        return bytes.__eq__(self, other)

    __hash__ = bytes.__hash__

    def find(self, *args):
        _Opaque.touched.append(("find",))
        return bytes.find(self, *args)

    def decode(self, *args, **kwargs):
        _Opaque.touched.append(("decode",))
        return bytes.decode(self, *args, **kwargs)

    def startswith(self, *args):
        _Opaque.touched.append(("startswith",))
        return bytes.startswith(self, *args)


class _NullTransport:
    async def send(self, frame):
        await asyncio.sleep(3600)

    async def close(self, code):
        return

    def __aiter__(self):
        return self

    async def __anext__(self):
        raise StopAsyncIteration


def _paired(**overrides):
    """A relay on a manual clock with two joined legs whose transports never
    finish a send, driven through the relay's own frame handling."""
    clock = ManualClock(WALL)
    relay = Relay(make_relay_config(**overrides), clock)
    core = Leg(relay, _NullTransport(), "198.51.100.1")
    device = Leg(relay, _NullTransport(), "198.51.100.2")
    core_token, device_token, _ = grant_pair()
    for leg, token in ((core, core_token), (device, device_token)):
        relay.accept(leg)
        relay.on_frame(leg, bytes([TAG_JOIN]) + token.encode())
    assert core.joined and device.joined and core.session is device.session
    core.control.clear()
    device.control.clear()
    return relay, clock, core, device


def _drain(leg):
    out = []
    while True:
        frame = leg._next_data()
        if frame is None:
            return out
        out.append(frame)


def test_lanes_a_control_flood_neither_evicts_nor_holds_back_media():
    """Section 12.5, two lanes: 200 control datagrams sent into a leg that is
    not being written to fill and churn the control queue only; every media
    datagram sent among them is still queued, and the writer takes the
    lanes in turn, so the media ones come out among the first."""

    async def go():
        relay, clock, core, device = _paired(rate_bytes_per_second=10 ** 9)
        media = []
        for n in range(200):
            relay.on_frame(device, bytes([TAG_CONTROL]) + struct.pack(">I", n) + bytes(100))
            if n % 40 == 0:
                frame = bytes([TAG_MEDIA]) + struct.pack(">I", n)
                media.append(frame)
                relay.on_frame(device, frame)
        session = core.session
        assert session.queues[(1, TAG_CONTROL)].dropped > 0
        assert session.queues[(1, TAG_MEDIA)].dropped == 0
        out = _drain(core)
        tags = [f[0] for f in out]
        assert [f for f in out if f[0] == TAG_MEDIA] == media
        # Taken in turn: the five media datagrams are within the first ten.
        assert tags[:10].count(TAG_MEDIA) == len(media)
        # The control lane kept its newest.
        control = [struct.unpack(">I", f[1:5])[0] for f in out if f[0] == TAG_CONTROL]
        assert control == list(range(200 - len(control), 200))

    asyncio.run(go())


def test_lanes_each_tag_has_its_own_cap_each_way_and_a_rejoin_keeps_them():
    async def go():
        relay, clock, core, device = _paired(queue_frames=1000, queue_bytes=10 ** 6)
        big = 1500
        per_second = 80000 // (big + 1)
        for tag in (TAG_CONTROL, TAG_MEDIA):
            for _ in range(100):
                relay.on_frame(device, bytes([tag]) + bytes(big))
            relay.on_frame(core, bytes([tag]) + bytes(big))
        session = core.session
        assert len(session.queues[(1, TAG_CONTROL)]) == per_second
        assert len(session.queues[(1, TAG_MEDIA)]) == per_second
        # The other direction spent nothing of the device's budget.
        assert len(session.queues[(2, TAG_CONTROL)]) == 1 and len(session.queues[(2, TAG_MEDIA)]) == 1
        assert session.dropped_rate == 2 * (100 - per_second)
        # The device's connection is replaced by a new one: both of its
        # buckets are still spent.
        token = relaygrant.mint(SECRET, relaygrant.LEG_DEVICE, session.sid, session.station, session.expires)
        again = Leg(relay, _NullTransport(), "198.51.100.3")
        relay.accept(again)
        relay.on_frame(again, bytes([TAG_JOIN]) + token.encode())
        assert again.joined and again.session is session
        before = session.dropped_rate
        for tag in (TAG_CONTROL, TAG_MEDIA):
            relay.on_frame(again, bytes([tag]) + bytes(big))
        assert session.dropped_rate == before + 2
        clock.advance(1000)
        for tag in (TAG_CONTROL, TAG_MEDIA):
            relay.on_frame(again, bytes([tag]) + bytes(big))
        assert session.dropped_rate == before + 2
        relay.end_session(session, "idle")

    asyncio.run(go())


MARKER = b"NEREUS-RELAY-PLAINTEXT-MARKER"
# A DTLS 1.2 application-data record header, then the marker, as a peer
# would carry it inside ciphertext; STUN and RTP first bytes too.
DATAGRAMS = [
    b"\x17\xfe\xfd\x00\x01\x00\x00\x00\x00\x00\x07\x00\x40" + MARKER + bytes(20),
    b"\x16\xfe\xfd\x00\x00" + MARKER,
    b"\x00\x01\x00\x08\x21\x12\xa4\x42" + MARKER,
    b"\x80\x6f\x00\x01" + MARKER,
]


def test_the_relay_reads_only_the_tag_and_the_length(caplog):
    """Requirement 6: frames that look like DTLS, STUN and RTP, each holding
    a known marker, go through the relay's frame handling; it reads the
    first byte and the length and nothing else, queues the very object it
    was given, and no log line holds the marker."""
    caplog.set_level(logging.DEBUG)

    async def go():
        relay, clock, core, device = _paired(queue_frames=1000, queue_bytes=10 ** 6, rate_bytes_per_second=10 ** 9)
        sent = []
        for tag in (1, 2):
            for datagram in DATAGRAMS:
                frame = _Opaque(bytes([tag]) + datagram)
                sent.append(frame)
                _Opaque.touched = []
                relay.on_frame(device, frame)
                assert _Opaque.touched == [], _Opaque.touched
        queued = []
        while True:
            frame = core._next_data()
            if frame is None:
                break
            queued.append(frame)
        assert len(queued) == len(sent)
        assert {id(f) for f in queued} == {id(f) for f in sent}
        relay.end_session(core.session, "idle")

    asyncio.run(go())
    for record in caplog.records:
        assert MARKER.decode() not in record.getMessage()


def test_marked_datagrams_cross_the_real_relay_byte_for_byte(caplog):
    caplog.set_level(logging.DEBUG)

    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            for tag in (1, 2):
                for datagram in DATAGRAMS:
                    frame = bytes([tag]) + datagram
                    await device.send(frame)
                    assert await recv(core) == frame
                    await core.send(frame)
                    assert await recv(device) == frame

    asyncio.run(go())
    for record in caplog.records:
        if record.name.startswith("nereus"):
            assert MARKER.decode() not in record.getMessage()


def test_a_send_stuck_for_send_stall_ms_closes_the_leg_with_one_timer():
    """Section 12.5: one message waiting 10 s to be written closes the leg
    (1008) and its place waits for a rejoin. The check is one lazily armed
    timer, not a timer made and cancelled for every datagram."""

    async def go():
        relay, clock, core, device = _paired(rate_bytes_per_second=10 ** 9)
        armed = []
        real_call_later = clock.call_later

        def counting(delay, callback):
            armed.append(delay)
            return real_call_later(delay, callback)

        clock.call_later = counting
        core.writer = asyncio.ensure_future(core.write_loop())
        for n in range(50):
            relay.on_frame(device, bytes([TAG_MEDIA, n]))
        await asyncio.sleep(0.01)
        assert core.sending_since is not None
        assert armed.count(relay.config.send_stall_ms) == 1
        clock.advance(9999)
        assert not core.aborted
        clock.advance(1)
        await asyncio.sleep(0.01)
        assert core.aborted and core.session is None
        session = device.session
        assert session.legs[1] is None and session.away[1] is not None
        relay.end_session(session, "idle")

    asyncio.run(go())


def test_idleness_is_checked_lazily():
    """A busy session costs one idle timer an idle period, not one a
    datagram, and still ends 30 s after its last datagram."""

    async def go():
        relay, clock, core, device = _paired(rate_bytes_per_second=10 ** 9, queue_frames=10 ** 4, queue_bytes=10 ** 7)
        armed = []
        real_call_later = clock.call_later

        def counting(delay, callback):
            armed.append(delay)
            return real_call_later(delay, callback)

        clock.call_later = counting
        session = core.session
        for n in range(1000):
            relay.on_frame(device, bytes([TAG_MEDIA, n % 256]))
            clock.advance(50)
        assert len(armed) <= 3, armed
        assert session.last_activity_ms == clock.now_ms() - 50
        clock.advance(29999 - 50)
        assert not session.ended
        clock.advance(1)
        assert session.ended

    asyncio.run(go())
