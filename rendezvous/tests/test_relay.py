# no-port-check: NereusSDR-original.
"""The WebSocket relay (rendezvous document section 12), over real
WebSockets on loopback: pairing by grant, the two legs told apart, one
grant useless for another, rejoin, the limits, the refusals, and the frame
format."""

import asyncio
import logging

import pytest

from nereus_rendezvous import relaygrant
from nereus_relay import relay as relay_module
from nereus_relay.relay import TAG_CONTROL, TAG_JOIN, TAG_MEDIA, TAG_PEER, TAG_READY
from relay_helpers import (
    SECRET,
    STATION_A,
    WALL,
    connect,
    expect_close,
    expect_end,
    expect_nothing,
    grant_pair,
    join,
    live_relay,
    recv,
    settle,
)


def test_two_legs_pair_and_forward_both_ways_unchanged():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, ready = await join(uri, core_token, "198.51.100.1")
            assert ready == bytes([TAG_READY, 1, 0])
            device, ready = await join(uri, device_token, "203.0.113.9")
            assert ready == bytes([TAG_READY, 1, 1])
            assert await recv(core) == bytes([TAG_PEER, 1])
            # Both stream tags, and every byte value.
            frames = [
                bytes([TAG_CONTROL]) + bytes(range(256)),
                bytes([TAG_MEDIA]) + b"\x16\xfe\xfd" + bytes(1497),
            ]
            for frame in frames:
                await device.send(frame)
                assert await recv(core) == frame
                await core.send(frame)
                assert await recv(device) == frame
            # A reserved data tag is dropped and counted, not forwarded, and
            # the leg stays.
            await device.send(bytes([0x05, 0x00]))
            await device.send(bytes([0x7F]) + b"\x80\x01rtp")
            await device.send(bytes([TAG_MEDIA, 9]))
            assert await recv(core) == bytes([TAG_MEDIA, 9])
            session = next(iter(relay.sessions.values()))
            assert session.dropped_unknown == 2
            assert len(relay.sessions) == 1

    asyncio.run(go())


def test_the_leg_comes_from_the_token_and_a_second_connection_replaces_the_first():
    """Two device legs never pair with each other: the leg is the one the
    grant names, and a second connection for a leg takes its place."""

    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            first, ready = await join(uri, device_token)
            assert ready == bytes([TAG_READY, 1, 0])
            second, ready = await join(uri, device_token, "203.0.113.50")
            assert ready == bytes([TAG_READY, 1, 0])
            await expect_end(first, "replaced")
            await second.send(bytes([TAG_MEDIA, 1, 2, 3]))
            core, ready = await join(uri, core_token)
            assert ready == bytes([TAG_READY, 1, 1])
            assert await recv(second) == bytes([TAG_PEER, 1])
            await second.send(bytes([TAG_MEDIA, 4]))
            assert await recv(core) == bytes([TAG_MEDIA, 4])

    asyncio.run(go())


def test_a_grant_of_one_introduction_is_useless_for_another():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_a, device_a, _ = grant_pair()
            core_b, device_b, _ = grant_pair()
            dev_a, ready = await join(uri, device_a)
            assert ready[2] == 0
            cor_b, ready = await join(uri, core_b)
            # Each is alone in its own session: the other introduction's leg
            # is no peer of it.
            assert ready[2] == 0
            await dev_a.send(bytes([TAG_MEDIA, 9]))
            await cor_b.send(bytes([TAG_MEDIA, 8]))
            await settle(relay, 4)
            await expect_nothing(cor_b)
            await expect_nothing(dev_a)
            assert len(relay.sessions) == 2
            assert sum(s.dropped_no_peer for s in relay.sessions.values()) == 2

    asyncio.run(go())


def test_a_grant_with_another_session_id_but_the_same_secret_does_not_pair():
    """Changing the session inside a token breaks its MAC."""

    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, session = grant_pair()
            raw = bytearray(relaygrant.from_b64url(device_token))
            raw[5] ^= 1
            forged = relaygrant.to_b64url(bytes(raw))
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + forged.encode())
            await expect_end(ws, "badToken")

    asyncio.run(go())


@pytest.mark.parametrize(
    "first",
    [
        "otherSecret",
        "flippedBit",
        "garbage",
        "empty",
        "notAscii",
    ],
)
def test_bad_tokens_are_refused(first):
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, _, session = grant_pair()
            payloads = {
                "otherSecret": relaygrant.mint(b"not the relay's secret", 1, session, STATION_A, WALL + 120).encode(),
                "flippedBit": (core_token[:-1] + ("A" if core_token[-1] != "A" else "B")).encode(),
                "garbage": b"hello",
                "empty": b"",
                "notAscii": b"\xff" * 72,
            }
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + payloads[first])
            await expect_end(ws, "badToken")
            assert relay.sessions == {} and relay.pending == 0

    asyncio.run(go())


@pytest.mark.parametrize("first", ["text", "data", "ready", "zero", "emptyFrame"])
def test_a_first_message_that_is_not_a_join_is_a_protocol_error(first):
    frames = {
        "text": "join",
        "data": bytes([TAG_MEDIA, 1]),
        "ready": bytes([TAG_READY, 1, 0]),
        "zero": bytes([0, 1]),
        "emptyFrame": b"",
    }

    async def go():
        async with live_relay() as (relay, clock, uri):
            ws = await connect(uri)
            await ws.send(frames[first])
            await expect_end(ws, "protocolError")

    asyncio.run(go())


@pytest.mark.parametrize("frame", ["text", b"\x00\x01", b"\x80x", b"\x81\x01\x00", b"\xff\x00", b"\x01"])
def test_after_joining_only_data_frames_with_a_payload_are_accepted(frame):
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            await device.send(frame)
            await expect_end(device, "protocolError")
            # The other leg is told its peer left, and may wait for it.
            assert await recv(core) == bytes([TAG_PEER, 0])

    asyncio.run(go())


def test_frame_size_cap():
    """Section 12.3: at most 1501 bytes (the tag and 1500); a longer
    message closes the leg at the WebSocket layer (1009)."""

    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            biggest = bytes([TAG_MEDIA]) + bytes(1500)
            await device.send(biggest)
            assert await recv(core) == biggest
            await device.send(biggest + b"\x00")
            await expect_close(device, 1009)

    asyncio.run(go())


def test_an_expired_grant_opens_no_session_but_a_live_session_takes_its_rejoin():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair(expires=WALL + 120)
            expired_core, _, _ = grant_pair(expires=WALL - 1)
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + expired_core.encode())
            await expect_end(ws, "expired")
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            # Keep the session busy past the grant's expiry.
            for _ in range(13):
                clock.advance(10000)
                await device.send(bytes([TAG_MEDIA, 1]))
                assert await recv(core) == bytes([TAG_MEDIA, 1])
            assert clock.wall_seconds() > WALL + 120
            await device.close()
            assert await recv(core) == bytes([TAG_PEER, 0])
            device, ready = await join(uri, device_token)
            assert ready == bytes([TAG_READY, 1, 1])
            await device.send(bytes([TAG_MEDIA, 2]))
            assert await recv(core) == bytes([TAG_PEER, 1])
            assert await recv(core) == bytes([TAG_MEDIA, 2])

    asyncio.run(go())


def test_rejoin_within_the_window_and_peer_gone_after_it():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            await device.close()
            assert await recv(core) == bytes([TAG_PEER, 0])
            # Frames for an absent peer are dropped, not queued.
            await core.send(bytes([TAG_MEDIA, 1]))
            clock.advance(29999)
            device, ready = await join(uri, device_token, "203.0.113.77")
            assert ready == bytes([TAG_READY, 1, 1])
            assert await recv(core) == bytes([TAG_PEER, 1])
            await expect_nothing(device)
            await core.send(bytes([TAG_MEDIA, 2]))
            assert await recv(device) == bytes([TAG_MEDIA, 2])
            await device.close()
            assert await recv(core) == bytes([TAG_PEER, 0])
            clock.advance(29999)
            await expect_nothing(core)
            clock.advance(1)
            await expect_end(core, "peerGone")
            assert relay.sessions == {}

    asyncio.run(go())


def test_a_peer_that_never_joins():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, _, _ = grant_pair()
            core, _ = await join(uri, core_token)
            clock.advance(29999)
            await expect_nothing(core)
            clock.advance(1)
            await expect_end(core, "peerGone")

    asyncio.run(go())


def test_idle_session_ends_and_its_tokens_are_spent():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            clock.advance(20000)
            await core.send(bytes([TAG_CONTROL, 1]))
            assert await recv(device) == bytes([TAG_CONTROL, 1])
            clock.advance(29999)
            await expect_nothing(core)
            clock.advance(1)
            await expect_end(core, "idle")
            await expect_end(device, "idle")
            # The grant is still within its expiry, but its session has
            # ended: it opens nothing more.
            for token in (core_token, device_token):
                ws = await connect(uri)
                await ws.send(bytes([TAG_JOIN]) + token.encode())
                await expect_end(ws, "ended")

    asyncio.run(go())


def test_slots_bound_sessions_but_never_a_rejoin():
    async def go():
        async with live_relay(slots=1) as (relay, clock, uri):
            core_a, device_a, _ = grant_pair()
            core_b, _, _ = grant_pair()
            core, _ = await join(uri, core_a)
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + core_b.encode())
            await expect_end(ws, "full")
            device, ready = await join(uri, device_a)
            assert ready[2] == 1

    asyncio.run(go())


def test_connections_per_address_count_until_closed():
    """Every connection counts for its address group from its arrival until
    it has closed, refused ones too (section 12.5)."""

    async def go():
        async with live_relay(connections_per_address=2) as (relay, clock, uri):
            a1 = await connect(uri, "198.51.100.5")
            a2 = await connect(uri, "198.51.100.5")
            a3 = await connect(uri, "198.51.100.5")
            await expect_end(a3, "tooManyConnections")
            # IPv6 counts by /56: two addresses in one /56 are one group.
            b1 = await connect(uri, "2001:db8:1:ff::1")
            b2 = await connect(uri, "2001:db8:1:1::2")
            b3 = await connect(uri, "2001:db8:1:2::3")
            await expect_end(b3, "tooManyConnections")
            # A joined leg still counts for its group.
            core_token, _, _ = grant_pair()
            await a1.send(bytes([TAG_JOIN]) + core_token.encode())
            assert (await recv(a1))[0] == TAG_READY
            a4 = await connect(uri, "198.51.100.5")
            await expect_end(a4, "tooManyConnections")
            # Once one has closed, its place is free again.
            await a2.close()
            for _ in range(100):
                if relay.per_group.get("198.51.100.5") == 1:
                    break
                await asyncio.sleep(0.01)
            a5 = await connect(uri, "198.51.100.5")
            await expect_nothing(a5)
            for ws in (a1, a5, b1, b2):
                await ws.close()

    asyncio.run(go())


def test_the_oldest_waiting_connection_makes_room():
    """When every waiting place is taken, the connection waiting longest is
    ended (full) and the new one admitted, so a few networks holding
    connections open cannot shut every join out (section 12.5)."""

    async def go():
        async with live_relay(max_pending=3) as (relay, clock, uri):
            w1 = await connect(uri, "203.0.113.1")
            w2 = await connect(uri, "203.0.113.2")
            w3 = await connect(uri, "203.0.113.3")
            await asyncio.sleep(0.05)
            assert relay.pending == 3
            joiner = await connect(uri, "203.0.113.4")
            await expect_end(w1, "full")
            core_token, _, _ = grant_pair()
            await joiner.send(bytes([TAG_JOIN]) + core_token.encode())
            assert (await recv(joiner))[0] == TAG_READY
            assert relay.pending == 2
            await expect_nothing(w2)
            for ws in (w2, w3, joiner):
                await ws.close()

    asyncio.run(go())


def test_default_limits_allow_four_devices_reconnects_and_fifth_device():
    """Four old paths plus four reconnects and one replacement introduction.
    All endpoints can share one address group; this tests transport admission,
    not Core device authorization, which still admits only four devices.
    """
    async def go():
        async with live_relay() as (relay, clock, uri):
            pairs = []
            for _ in range(9):
                core_token, device_token, _ = grant_pair(station=STATION_A)
                core, _ = await join(uri, core_token, "198.51.100.1")
                device, ready = await join(uri, device_token, "198.51.100.1")
                assert ready == bytes([TAG_READY, 1, 1])
                assert await recv(core) == bytes([TAG_PEER, 1])
                pairs.append((core, device))
            assert relay.per_station[STATION_A] == 9
            extra_token, _, _ = grant_pair(station=STATION_A)
            extra = await connect(uri, "203.0.113.99")
            await extra.send(bytes([TAG_JOIN]) + extra_token.encode())
            await expect_end(extra, "tooManySessions")
            for index, (core, device) in enumerate(pairs):
                frame = bytes([TAG_CONTROL, index])
                await device.send(frame)
                assert await recv(core) == frame
            assert len(relay.sessions) == 9

    asyncio.run(go())


def test_sessions_per_station_are_capped():
    """Anyone can get grants for a station they register themselves, so the
    relay caps live sessions per station value (configured as 2 here): a third for
    one station is refused, another station's is not, and a rejoin into a
    live session never is."""

    async def go():
        async with live_relay(sessions_per_station=2) as (relay, clock, uri):
            from relay_helpers import STATION_A, STATION_B

            legs = []
            tokens = []
            for _ in range(2):
                core_token, device_token, _ = grant_pair(station=STATION_A)
                tokens.append(device_token)
                ws, _ = await join(uri, core_token)
                legs.append(ws)
            third, _, _ = grant_pair(station=STATION_A)
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + third.encode())
            await expect_end(ws, "tooManySessions")
            other, _, _ = grant_pair(station=STATION_B)
            ws, ready = await join(uri, other)
            assert ready[0] == TAG_READY
            device, ready = await join(uri, tokens[0])
            assert ready == bytes([TAG_READY, 1, 1])
            # A session that ends frees its place for the station.
            clock.advance(30000)
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + third.encode())
            assert (await recv(ws))[0] == TAG_READY
            assert relay.per_station.get(STATION_A) == 1

    asyncio.run(go())


def test_a_connection_that_never_joins_times_out():
    async def go():
        async with live_relay() as (relay, clock, uri):
            ws = await connect(uri)
            await asyncio.sleep(0.05)
            clock.advance(9999)
            await expect_nothing(ws)
            clock.advance(1)
            await expect_end(ws, "timeout")

    asyncio.run(go())


def test_the_rate_cap_drops_what_is_over_it_each_way():
    """80000 bytes a second each way, bursts of one second: 53 frames of
    1501 bytes pass at once, the rest are dropped; a second later 53 more."""

    async def go():
        async with live_relay(queue_frames=1000, queue_bytes=1000000) as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            frame = bytes([TAG_MEDIA]) + bytes(1500)
            for _ in range(100):
                await device.send(frame)
            # The other direction has a bucket of its own.
            await core.send(bytes([TAG_MEDIA, 7]))
            await settle(relay, 2 + 101)
            got = 0
            while True:
                try:
                    await asyncio.wait_for(core.recv(), 0.3)
                    got += 1
                except asyncio.TimeoutError:
                    break
            assert got == 80000 // 1501
            assert await recv(device) == bytes([TAG_MEDIA, 7])
            session = next(iter(relay.sessions.values()))
            assert session.dropped_rate == 100 - got
            clock.advance(1000)
            for _ in range(100):
                await device.send(frame)
            await settle(relay, 2 + 201)
            again = 0
            while True:
                try:
                    await asyncio.wait_for(core.recv(), 0.3)
                    again += 1
                except asyncio.TimeoutError:
                    break
            assert again == 80000 // 1501
            # A leg that joins again keeps its session's bucket: no fresh
            # second's worth for reconnecting.
            await device.close()
            assert await recv(core) == bytes([TAG_PEER, 0])
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            await device.send(frame)
            await settle(relay, 2 + 201 + 2)
            try:
                got_after = await asyncio.wait_for(core.recv(), 0.3)
            except asyncio.TimeoutError:
                got_after = None
            assert got_after is None

    asyncio.run(go())


def test_legs_come_over_the_unix_socket_with_its_buffers():
    """Section 12.5: Caddy reaches the relay over a Unix stream socket whose
    buffers the relay sets on every connection it accepts (the send buffer
    is what the hop to Caddy can hold), and whose file only its owner (in
    production, Caddy's group too) may open."""
    import os
    import socket
    import stat

    async def go():
        async with live_relay() as (relay, clock, uri):
            assert stat.S_ISSOCK(os.stat(relay.config.socket).st_mode)
            assert stat.S_IMODE(os.stat(relay.config.socket).st_mode) == 0o600
            core_token, device_token, _ = grant_pair()
            await join(uri, core_token)
            await join(uri, device_token)
            assert len(relay.legs) == 2
            for leg in relay.legs:
                sock = leg.transport.ws.transport.get_extra_info("socket")
                assert sock.family == socket.AF_UNIX
                # Linux reports twice the value set; macOS about the value.
                assert 16384 <= sock.getsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF) <= 2 * 16384
            path = relay.config.socket
        assert not os.path.exists(path), "the socket file is removed when the relay stops"

    asyncio.run(go())


def test_a_request_without_x_forwarded_for_is_refused():
    """There is no peer address on a Unix socket, so the relay takes the
    client's from X-Forwarded-For, which Caddy sets, and refuses a request
    without one (400) before any WebSocket opens."""
    import socket

    async def go():
        async with live_relay() as (relay, clock, uri):
            path = uri[len("unix:"):]
            loop = asyncio.get_running_loop()

            def ask(extra: str) -> bytes:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(path)
                s.sendall((
                    "GET /v1/relay HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n" + extra + "\r\n"
                ).encode())
                data = s.recv(200)
                s.close()
                return data

            assert (await loop.run_in_executor(None, ask, "")).startswith(b"HTTP/1.1 400")
            assert (await loop.run_in_executor(None, ask, "X-Forwarded-For: not-an-address\r\n")).startswith(b"HTTP/1.1 400")
            assert (await loop.run_in_executor(None, ask, "X-Forwarded-For: 192.0.2.7\r\n")).startswith(b"HTTP/1.1 101")
            assert relay.legs == set() or all(leg.group == "192.0.2.7" for leg in relay.legs)

    asyncio.run(go())


def test_a_stale_socket_file_is_replaced():
    import os

    from relay_helpers import short_directory

    directory = short_directory()
    path = os.path.join(directory, "relay.sock")
    open(path, "w").close()

    async def go():
        async with live_relay(socket=path) as (relay, clock, uri):
            core_token, _, _ = grant_pair()
            await join(uri, core_token)

    asyncio.run(go())
    os.rmdir(directory)


def test_shutdown_tells_every_leg():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, _, _ = grant_pair()
            core, _ = await join(uri, core_token)
            pending = await connect(uri)
            await asyncio.sleep(0.05)
            relay.shutdown()
            await expect_end(core, "shuttingDown", 1001)
            await expect_end(pending, "shuttingDown", 1001)

    asyncio.run(go())


def test_logs_hold_no_token_address_or_whole_session(caplog):
    caplog.set_level(logging.DEBUG)

    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, session = grant_pair()
            core, _ = await join(uri, core_token, "198.51.100.33")
            device, _ = await join(uri, device_token, "2001:db8:33::1")
            await device.send(bytes([TAG_MEDIA]) + b"PLAINTEXT-MARKER")
            await recv(core)
            await recv(core)
            clock.advance(30000)
            await expect_end(core, "idle")
            return core_token, device_token, session

    core_token, device_token, session = asyncio.run(go())
    text = "\n".join(r.getMessage() for r in caplog.records if r.name.startswith("nereus"))
    assert "relay session" in text and "ended (idle)" in text
    whole = relaygrant.to_b64url(session)
    assert whole[:6] in text and whole not in text
    for value in (core_token, device_token, "198.51.100.33", "2001:db8:33", "127.0.0.1", "PLAINTEXT-MARKER"):
        assert value not in text


def test_data_use_counts_by_utc_day():
    use = relay_module.DataUse(WALL)
    use.session()
    use.add(1500000000)
    assert use.roll(WALL + 60) is None
    day = relay_module.DataUse._day_of(WALL)
    line = use.roll(WALL + 86400)
    assert line == f"relay data use: 1.50 GB forwarded on {day} (UTC) in 1 sessions; 1.50 GB since the relay started"
    use.add(500000000)
    assert use.line().endswith("0.50 GB forwarded on %s (UTC) in 0 sessions; 2.00 GB since the relay started"
                               % relay_module.DataUse._day_of(WALL + 86400))


def test_the_hourly_check_logs_the_finished_day(caplog):
    caplog.set_level(logging.INFO)

    async def go():
        async with live_relay() as (relay, clock, uri):
            core_token, device_token, _ = grant_pair()
            core, _ = await join(uri, core_token)
            device, _ = await join(uri, device_token)
            assert await recv(core) == bytes([TAG_PEER, 1])
            await device.send(bytes([TAG_MEDIA]) + bytes(999))
            await recv(core)
            relay.start_day_timer()
            clock.advance(86400 * 1000)

    asyncio.run(go())
    lines = [r.getMessage() for r in caplog.records if r.getMessage().startswith("relay data use")]
    assert lines and "0.00 GB forwarded on" in lines[0] and "in 1 sessions" in lines[0]


def test_a_refused_connection_counts_until_it_has_closed():
    """Section 12.5 (M3): a refused connection whose peer never answers the
    close still holds its address group's place until it has closed, so a
    new connection from that group meanwhile is refused too."""
    import sys
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from caddy_probe import _SlowLeg

    async def go():
        async with live_relay(connections_per_address=1) as (relay, clock, uri):
            first = await connect(uri, "198.51.100.44")
            await asyncio.sleep(0.05)
            loop = asyncio.get_running_loop()
            # A peer that is refused and then never reads nor answers the close.
            silent = await loop.run_in_executor(None, lambda: _SlowLeg(uri, "", 4096, "198.51.100.44"))
            await asyncio.sleep(0.2)
            assert relay.per_group.get("198.51.100.44") == 2
            await first.close()
            for _ in range(100):
                if relay.per_group.get("198.51.100.44") == 1:
                    break
                await asyncio.sleep(0.01)
            assert relay.per_group.get("198.51.100.44") == 1
            late = await connect(uri, "198.51.100.44")
            await expect_end(late, "tooManyConnections")
            silent.sock.close()

    asyncio.run(go())
