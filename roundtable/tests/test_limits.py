# no-port-check: NereusSDR-original.
"""Address groups, sliding windows, the client address, the connection
caps, the outbound queue and the heartbeat (wire spec 2.3, 9.1)."""

import asyncio

import pytest

from nereus_roundtable import protocol, transport
from nereus_roundtable.clock import ManualClock
from nereus_roundtable.config import Config
from nereus_roundtable.limits import AddressGroups, Window
from nereus_roundtable.service import Connection, Service
from helpers import expect_closed, live_service, recv_json, sign_in
from runner import ws_connect

group = AddressGroups.group


def test_ipv6_groups_are_slash_64():
    assert group("2001:db8:1:2::1") == group("2001:db8:1:2:ffff::9")
    assert group("2001:db8:1:2::1") != group("2001:db8:1:3::1")
    assert group("2001:db8:1:2::1") == "2001:db8:1:2::/64"


def test_ipv4_groups_are_one_address():
    assert group("192.0.2.1") != group("192.0.2.2")
    assert group("192.0.2.1") == "192.0.2.1"
    # An IPv4-mapped IPv6 address counts as its IPv4 address.
    assert group("::ffff:192.0.2.1") == "192.0.2.1"


def test_group_counts():
    groups = AddressGroups()
    groups.add("192.0.2.1")
    groups.add("192.0.2.1")
    assert groups.count("192.0.2.1") == 2 and len(groups) == 1
    groups.remove("192.0.2.1")
    groups.remove("192.0.2.1")
    assert groups.count("192.0.2.1") == 0 and len(groups) == 0


def test_window_is_sliding_and_half_open():
    window = Window(1, 1000)
    assert window.allow("a", 0)
    assert not window.allow("a", 999)
    assert window.retry_after("a", 999) == 1
    assert window.allow("a", 1000)
    # Keys are separate.
    assert window.allow("b", 1000)


def test_window_refusals_are_not_counted():
    window = Window(2, 60000)
    assert window.allow("a", 0) and window.allow("a", 10)
    for t in range(20, 60000, 5000):
        assert not window.allow("a", t)
    assert window.retry_after("a", 59999) == 1
    assert window.allow("a", 60000)
    assert not window.allow("a", 60001)
    assert window.allow("a", 60010)


def test_forwarded_address_is_believed_only_from_a_trusted_proxy():
    trusted = ["127.0.0.1", "::1"]
    assert transport.forwarded_address("127.0.0.1", ["198.51.100.7"], trusted) == "198.51.100.7"
    assert transport.forwarded_address("203.0.113.5", ["198.51.100.7"], trusted) == "203.0.113.5"
    assert transport.forwarded_address("::1", ["2001:db8::1"], trusted) == "2001:db8::1"
    # The last entry is the one the proxy added.
    assert transport.forwarded_address("127.0.0.1", ["10.0.0.1, 198.51.100.7"], trusted) == "198.51.100.7"
    assert transport.forwarded_address("127.0.0.1", ["10.0.0.1", "198.51.100.8"], trusted) == "198.51.100.8"
    # No header, or one that does not parse: the peer.
    assert transport.forwarded_address("127.0.0.1", [], trusted) == "127.0.0.1"
    assert transport.forwarded_address("127.0.0.1", ["not an address"], trusted) == "127.0.0.1"


class _Headers:
    def __init__(self, values):
        self.values = values

    def get_all(self, name):
        return list(self.values) if name.lower() == "x-forwarded-for" else []


class _FakeWs:
    def __init__(self, peer, forwarded):
        self.remote_address = (peer, 50000)
        self.request_headers = _Headers(forwarded)


def test_client_address_of_a_websocket():
    trusted = ["127.0.0.1", "::1"]
    assert transport.client_address(_FakeWs("127.0.0.1", ["198.51.100.7"]), trusted) == "198.51.100.7"
    assert transport.client_address(_FakeWs("203.0.113.5", ["198.51.100.7"]), trusted) == "203.0.113.5"


def test_forwarded_address_reaches_the_service():
    async def go():
        async with live_service() as live:
            ws = await ws_connect(live.station_uri, "198.51.100.7")
            await recv_json(ws)
            assert live.service.station_groups.count("198.51.100.7") == 1
            await ws.close()
            for _ in range(500):
                if live.service.station_groups.count("198.51.100.7") == 0:
                    break
                await asyncio.sleep(0.01)
            assert live.service.station_groups.count("198.51.100.7") == 0

    asyncio.run(go())


def test_station_connections_from_one_v6_network_share_a_cap():
    async def go():
        async with live_service({"limits": {"stationConnectionsPerGroup": 2}}) as live:
            a = await ws_connect(live.station_uri, "2001:db8:1:2::1")
            b = await ws_connect(live.station_uri, "2001:db8:1:2:ffff::9")
            await recv_json(a)
            await recv_json(b)
            c = await ws_connect(live.station_uri, "2001:db8:1:2::77")
            error = await recv_json(c)
            assert error == protocol.error_message("rate-network", None)
            assert await expect_closed(c) == protocol.CLOSE_TRY_LATER
            d = await ws_connect(live.station_uri, "2001:db8:1:3::1")
            assert (await recv_json(d))["type"] == "challenge"

    asyncio.run(go())


def test_live_sockets_per_group_and_in_all():
    async def go():
        async with live_service({"limits": {"liveSocketsTotal": 6}}) as live:
            held = [await ws_connect(live.live_uri, "192.0.2.1") for _ in range(4)]
            fifth = await ws_connect(live.live_uri, "192.0.2.1")
            error = await recv_json(fifth)
            assert error["type"] == "error" and error["code"] == "rate"
            assert error["reason"] == "Too many connections from this network. Try again shortly."
            assert error["about"] is None and error["retryAfterMs"] == 5000
            assert await expect_closed(fifth) == protocol.CLOSE_TRY_LATER
            held += [await ws_connect(live.live_uri, "192.0.2.2") for _ in range(2)]
            for _ in range(500):
                if live.service.live_count == 6:
                    break
                await asyncio.sleep(0.01)
            busy = await ws_connect(live.live_uri, "192.0.2.3")
            error = await recv_json(busy)
            assert error["code"] == "rate" and error["reason"] == "Roundtable is busy. Try again shortly."
            assert error["retryAfterMs"] == 5000
            assert await expect_closed(busy) == protocol.CLOSE_TRY_LATER
            # A live reader is sent nothing until it asks.
            with pytest.raises(asyncio.TimeoutError):
                await asyncio.wait_for(held[0].recv(), 0.1)

    asyncio.run(go())


def test_live_connections_do_not_count_against_station_connections():
    async def go():
        async with live_service({"limits": {"stationConnectionsPerGroup": 1}}) as live:
            await ws_connect(live.live_uri, "192.0.2.1")
            ws, _, _ = await sign_in(live, address="192.0.2.1")
            assert ws is not None

    asyncio.run(go())


class _NoTransport:
    def __aiter__(self):
        return self

    async def __anext__(self):
        raise StopAsyncIteration


def test_outbound_queue_drops_droppable_then_closes_past_twice_the_cap():
    async def go():
        config = Config()
        config.outbound_queue_messages = 2
        service = Service(config, ManualClock())
        conn = Connection(service, _NoTransport(), "station", "192.0.2.1")
        rooms = service.rooms_message()
        assert conn.send(rooms) and conn.send(rooms)
        # At the cap a droppable message is dropped and marks the gap.
        assert not conn.send(rooms, droppable=True)
        assert conn.dropped and not conn.closing
        # Others are still queued, up to twice the cap.
        assert conn.send(rooms) and conn.send(rooms)
        assert conn.queue.qsize() == 4
        assert not conn.send(rooms)
        assert conn.closing

    asyncio.run(go())


def test_outbound_queue_bytes_cap():
    async def go():
        config = Config()
        service = Service(config, ManualClock())
        conn = Connection(service, _NoTransport(), "station", "192.0.2.1")
        size = len(protocol.encode(service.rooms_message()).encode())
        config.outbound_queue_bytes = size * 2
        rooms = service.rooms_message()
        assert conn.send(rooms) and conn.send(rooms)
        assert not conn.send(rooms, droppable=True)
        assert conn.send(rooms) and conn.send(rooms)
        assert not conn.send(rooms) and conn.closing

    asyncio.run(go())


class _SilentWs:
    """A peer that never answers a ping."""

    def __init__(self):
        self.pings = 0
        self.closed = None

    async def ping(self):
        self.pings += 1
        return asyncio.get_running_loop().create_future()

    async def close(self, code, reason):
        self.closed = code


def test_heartbeat_pings_and_closes_a_silent_connection_with_1000():
    async def go():
        ws = _SilentWs()
        conn = transport.WsTransport(ws)
        await asyncio.wait_for(transport.heartbeat(conn, 20, 70), 2)
        assert ws.closed == 1000
        assert ws.pings >= 3

    asyncio.run(go())


def test_heartbeat_keeps_a_connection_that_answers():
    async def go():
        config_setup = {}
        async with live_service(config_setup) as live:
            live.service.config.ping_interval_ms = 20
            live.service.config.silence_close_ms = 70
            ws = await ws_connect(live.station_uri, "192.0.2.1")
            await recv_json(ws)
            # The client library answers pings; nothing closes it for
            # silence (the hello timeout runs on the ManualClock).
            with pytest.raises(asyncio.TimeoutError):
                await asyncio.wait_for(ws.recv(), 0.3)

    asyncio.run(go())
