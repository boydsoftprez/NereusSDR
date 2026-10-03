# no-port-check: NereusSDR-original.
"""Bounded watch sockets share a live relay session without changing its primary path."""

import asyncio
import os

import pytest

from nereus_rendezvous import relaygrant
from nereus_rendezvous.clock import ManualClock
from nereus_relay.config import MAX_MESSAGE_BYTES, ConfigError, check
from nereus_relay.relay import Leg, Relay, TAG_CONTROL, TAG_JOIN, TAG_MEDIA, TAG_PEER, TAG_READY, TAG_WATCH
from relay_helpers import (
    SECRET, STATION_A, WALL, connect, expect_close, expect_end, grant_pair, join,
    live_relay, make_relay_config, recv, settle,
)


def watch_pair(session, station, expires=WALL + 120):
    return tuple(relaygrant.mint_watch(SECRET, side, session, station, expires) for side in relaygrant.LEGS)


async def paired(uri, expires=WALL + 120, station=STATION_A):
    primary_core, primary_device, sid = grant_pair(expires=expires, station=station)
    core, _ = await join(uri, primary_core)
    device, _ = await join(uri, primary_device)
    assert await recv(core) == bytes([TAG_PEER, 1])
    return core, device, sid, primary_core, primary_device


async def watches(uri, sid, station=STATION_A, expires=WALL + 120):
    core_token, device_token = watch_pair(sid, station, expires)
    core, ready = await join(uri, core_token)
    assert ready == bytes([TAG_READY, 1, 0])
    device, ready = await join(uri, device_token)
    assert ready == bytes([TAG_READY, 1, 1])
    assert await recv(core) == bytes([TAG_PEER, 1])
    return core, device


def test_watch_and_primary_forward_only_to_matching_legs():
    async def go():
        async with live_relay() as (relay, _, uri):
            core, device, sid, _, _ = await paired(uri)
            watch_core, watch_device = await watches(uri, sid)
            assert len(relay.sessions) == relay.per_station[STATION_A] == relay.data_use.day_sessions == 1
            watch_frame = bytes([TAG_WATCH]) + b"\x16\xfe\xfd" + os.urandom(40)
            for sender, receiver in ((watch_core, watch_device), (watch_device, watch_core)):
                await sender.send(watch_frame)
                assert await recv(receiver) == watch_frame
            for sender, receiver in ((core, device), (device, core)):
                frame = bytes([TAG_MEDIA, 0x57])
                await sender.send(frame)
                assert await recv(receiver) == frame
            await device.send(bytes([TAG_WATCH, 1]))
            await settle(relay, 9)
            assert next(iter(relay.sessions.values())).dropped_unknown == 1
            assert relay.data_use.total_bytes == 2 * len(watch_frame) + 4

    asyncio.run(go())


def test_watch_rejects_missing_primary_mismatch_and_wrong_purpose():
    async def rejected(uri, token, code):
        ws = await connect(uri)
        await ws.send(bytes([TAG_JOIN]) + token.encode())
        await expect_end(ws, code)

    async def go():
        async with live_relay() as (relay, clock, uri):
            sid = os.urandom(16)
            await rejected(uri, watch_pair(sid, STATION_A)[0], "peerGone")
            assert relay.sessions == {} and relay.data_use.day_sessions == 0
            core, device, sid, _, _ = await paired(uri, expires=WALL)
            clock.advance(1000)
            await rejected(uri, watch_pair(sid, STATION_A, WALL + 1)[0], "badToken")
            await rejected(uri, watch_pair(sid, b"x" * 8, WALL)[0], "badToken")
            await rejected(uri, "invalid", "badToken")
            watch_core, _ = await join(uri, watch_pair(sid, STATION_A, WALL)[0])
            assert watch_core and core and device
            await device.close()
            await expect_end(watch_core, "peerGone")
            await rejected(uri, watch_pair(sid, STATION_A, WALL)[1], "peerGone")

    asyncio.run(go())


def test_watch_replacement_and_primary_replacement_retire_both_watches():
    async def go():
        async with live_relay() as (relay, _, uri):
            core, device, sid, core_token, device_token = await paired(uri)
            wc, wd = await watches(uri, sid)
            old_leg = next(leg for leg in relay.legs if leg.watch and leg.side == 1)
            replacement_wc, ready = await join(uri, watch_pair(sid, STATION_A)[0])
            assert ready == bytes([TAG_READY, 1, 1])
            await expect_end(wc, "replaced")
            assert await recv(wd) == bytes([TAG_PEER, 1])
            relay.detach(old_leg)
            assert old_leg.session is None
            assert next(iter(relay.sessions.values())).watch_legs[1] is not None
            await device.send(bytes([TAG_MEDIA, 1]))
            assert await recv(core) == bytes([TAG_MEDIA, 1])
            new_core, _ = await join(uri, core_token)
            await expect_end(core, "replaced")
            await expect_end(replacement_wc, "replaced")
            await expect_end(wd, "replaced")
            session = next(iter(relay.sessions.values()))
            assert all(leg is None for leg in session.watch_legs.values())
            assert session.legs[1] is not None and session.legs[2] is not None
            assert await recv(device) == bytes([TAG_PEER, 1])
            await new_core.send(bytes([TAG_CONTROL, 2]))
            assert await recv(device) == bytes([TAG_CONTROL, 2])
            assert relay.data_use.day_sessions == 1
            # Cleanup or a late frame from the replaced watch cannot take
            # ownership from a newly joined watch on the same side.
            fresh_wc, _ = await join(uri, watch_pair(sid, STATION_A)[0])
            relay.detach(old_leg)
            assert session.watch_legs[1] is not None
            assert fresh_wc

    asyncio.run(go())


@pytest.mark.parametrize("wrong", [TAG_CONTROL, TAG_MEDIA, 0x04, 0x80])
def test_watch_wrong_tags_close_only_watch(wrong):
    async def go():
        async with live_relay() as (relay, _, uri):
            core, device, sid, _, _ = await paired(uri)
            wc, wd = await watches(uri, sid)
            await wc.send(bytes([wrong, 1]))
            await expect_end(wc, "protocolError")
            assert await recv(wd) == bytes([TAG_PEER, 0])
            await device.send(bytes([TAG_MEDIA, 9]))
            assert await recv(core) == bytes([TAG_MEDIA, 9])
            assert len(relay.sessions) == 1

    asyncio.run(go())


def test_watch_oversize_closes_and_watch_traffic_cannot_prevent_idle():
    async def go():
        async with live_relay() as (relay, clock, uri):
            core, device, sid, _, _ = await paired(uri)
            wc, wd = await watches(uri, sid)
            await wc.send(bytes([TAG_WATCH]) + bytes(MAX_MESSAGE_BYTES))
            await expect_close(wc, 1009)
            assert await recv(wd) == bytes([TAG_PEER, 0])
            wc, ready = await join(uri, watch_pair(sid, STATION_A)[0])
            assert ready == bytes([TAG_READY, 1, 1])
            assert await recv(wd) == bytes([TAG_PEER, 1])
            session = next(iter(relay.sessions.values()))
            before = session.last_activity_ms
            for _ in range(3):
                clock.advance(9000)
                await wd.send(bytes([TAG_WATCH, 1]))
                await settle(relay, relay.frames_handled + 1)
                assert session.last_activity_ms == before
            clock.advance(3000)
            assert session.ended and not relay.sessions
            await expect_end(wd, "idle")
            await expect_end(core, "idle")
            await expect_end(device, "idle")

    asyncio.run(go())


class NoSend:
    async def send(self, frame):
        raise AssertionError("writer must remain blocked")

    async def close(self, code):
        pass


def blocked_pair(**overrides):
    clock = ManualClock(WALL)
    relay = Relay(make_relay_config(**overrides), clock)
    pcore, pdevice, sid = grant_pair(station=STATION_A)
    legs = []
    for token in (pcore, pdevice, *watch_pair(sid, STATION_A)):
        leg = Leg(relay, NoSend(), "198.51.100.1")
        relay.accept(leg)
        relay.on_frame(leg, bytes([TAG_JOIN]) + token.encode())
        legs.append(leg)
    return relay, clock, legs


def test_watch_rate_spends_shared_control_budget_and_queue_stays_bounded():
    async def go():
        relay, clock, (core, device, wc, wd) = blocked_pair(rate_bytes_per_second=MAX_MESSAGE_BYTES * 2,
                                                         watch_rate_bytes_per_second=MAX_MESSAGE_BYTES * 4,
                                                         watch_queue_frames=2, watch_queue_bytes=MAX_MESSAGE_BYTES * 2)
        session = core.session
        for number in range(4):
            relay.on_frame(wd, bytes([TAG_WATCH, number]) + bytes(MAX_MESSAGE_BYTES - 2))
        assert session.forwarded_frames == 2 and session.dropped_rate == 2
        assert len(session.watch_queues[1]) == 2 and session.watch_queues[1].bytes == MAX_MESSAGE_BYTES * 2
        relay.on_frame(device, bytes([TAG_CONTROL, 1]))
        assert session.dropped_rate == 3
        assert session.buckets[(2, TAG_MEDIA)].tokens == relay.config.rate_bytes_per_second
        assert session.watch_buckets[2].tokens == 0
        assert session.dropped_queue == 0
        clock.advance(1000)
        relay.on_frame(wd, bytes([TAG_WATCH, 5]))
        assert session.dropped_queue == 1
        assert session.watch_queues[1].peak_bytes <= MAX_MESSAGE_BYTES * 2
        assert relay.data_use.total_bytes == 2 * MAX_MESSAGE_BYTES + 2

    asyncio.run(go())


def test_watch_specific_bucket_checks_before_shared_control_budget():
    async def go():
        relay, _, (core, device, wc, wd) = blocked_pair(watch_rate_bytes_per_second=MAX_MESSAGE_BYTES)
        session = core.session
        relay.on_frame(wd, bytes([TAG_WATCH]) + bytes(MAX_MESSAGE_BYTES - 1))
        remaining = session.buckets[(2, TAG_CONTROL)].tokens
        relay.on_frame(wd, bytes([TAG_WATCH, 1]))
        assert session.dropped_rate == 1
        assert session.buckets[(2, TAG_CONTROL)].tokens == remaining
        relay.on_frame(device, bytes([TAG_CONTROL, 1]))
        assert session.forwarded_frames == 2

    asyncio.run(go())


def test_watch_rejoin_keeps_its_rate_bucket_and_clears_old_queue():
    async def go():
        relay, _, (core, device, wc, wd) = blocked_pair(watch_rate_bytes_per_second=MAX_MESSAGE_BYTES)
        session = core.session
        relay.on_frame(wd, bytes([TAG_WATCH]) + bytes(MAX_MESSAGE_BYTES - 1))
        assert session.watch_buckets[2].tokens == 0
        assert len(session.watch_queues[1]) == 1
        new_core = Leg(relay, NoSend(), "198.51.100.2")
        relay.accept(new_core)
        relay.on_frame(new_core, bytes([TAG_JOIN]) + watch_pair(session.sid, STATION_A)[0].encode())
        assert new_core.session is session and wc.session is None
        assert len(session.watch_queues[1]) == 0
        new_device = Leg(relay, NoSend(), "198.51.100.3")
        relay.accept(new_device)
        relay.on_frame(new_device, bytes([TAG_JOIN]) + watch_pair(session.sid, STATION_A)[1].encode())
        relay.on_frame(new_device, bytes([TAG_WATCH, 1]))
        assert session.dropped_rate == 1 and session.watch_buckets[2].tokens == 0
        assert session.legs[1] is core and session.legs[2] is device

    asyncio.run(go())


def test_watch_rejoin_churn_bounds_peer_control_notices():
    async def go():
        relay, _, (_, _, wc, wd) = blocked_pair()
        session = wc.session
        for number in range(40):
            token = watch_pair(session.sid, STATION_A)[0]
            replacement = Leg(relay, NoSend(), f"198.51.100.{number + 10}")
            relay.accept(replacement)
            relay.on_frame(replacement, bytes([TAG_JOIN]) + token.encode())
            wc = replacement
            assert len(wd.control) <= 16
        assert wd.control[-1] == bytes([TAG_PEER, 1])
        assert session.watch_legs[1] is wc

    asyncio.run(go())


def test_watch_configuration_and_physical_address_limit():
    for key, value in (("watch_rate_bytes_per_second", 0), ("watch_queue_frames", 0),
                       ("watch_queue_bytes", MAX_MESSAGE_BYTES - 1),
                       ("watch_rate_bytes_per_second", MAX_MESSAGE_BYTES - 1)):
        with pytest.raises(ConfigError):
            check(make_relay_config(**{key: value}))

    async def go():
        async with live_relay() as (relay, _, uri):
            sockets = []
            for _ in range(9):
                pcore, pdevice, sid = grant_pair(station=STATION_A)
                for token in (pcore, pdevice, *watch_pair(sid, STATION_A)):
                    ws, _ = await join(uri, token)
                    sockets.append(ws)
            assert len(sockets) == 36 and relay.per_group["192.0.2.1"] == 36
            assert relay.per_station[STATION_A] == 9 and len(relay.sessions) == 9
            overflow = await connect(uri)
            await expect_end(overflow, "tooManyConnections")
            assert all(not leg.closing for session in relay.sessions.values() for leg in
                       (*session.legs.values(), *session.watch_legs.values()))
            await asyncio.gather(*(ws.close() for ws in sockets))

    asyncio.run(go())
