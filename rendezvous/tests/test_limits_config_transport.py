# no-port-check: NereusSDR-original.
"""Address grouping, the rate-limit window, configuration, the proxy
address rule, nameplate allocation and listening in both address
families."""

import asyncio
import sys
import socket

import pytest

from nereus_rendezvous import config as cfg
from nereus_rendezvous import transport
from nereus_rendezvous.clock import ManualClock
from nereus_rendezvous.limits import WindowLimiter, address_group
from nereus_rendezvous.service import Service
from helpers import recv_json
from runner import ws_connect


@pytest.mark.parametrize(
    "address,group",
    [
        ("192.0.2.1", "192.0.2.1"),
        ("::ffff:192.0.2.1", "192.0.2.1"),
        ("2001:db8:1:2:3:4:5:6", "2001:db8:1::/56"),
        ("2001:db8:1:ff::", "2001:db8:1::/56"),
        ("2001:db8:1:100::1", "2001:db8:1:100::/56"),
        ("fe80::1%en0", "fe80::/56"),
        ("not an address", "not an address"),
    ],
)
def test_address_group(address, group):
    assert address_group(address) == group


def test_window_limiter():
    limiter = WindowLimiter(2, 60000)
    assert limiter.check("a", 0) is None
    limiter.record("a", 0)
    limiter.record("a", 10)
    assert limiter.check("a", 1000) == 59000
    assert limiter.check("b", 1000) is None
    assert limiter.check("a", 60000) is None
    limiter.sweep(120000)
    assert len(limiter) == 0


def test_config_file(tmp_path):
    secret = tmp_path / "secret"
    secret.write_bytes(b"s3cret\r\n")
    path = tmp_path / "r.conf"
    path.write_text(
        "[rendezvous]\nlisten = 127.0.0.1:9000 [::1]:9001\nturn_urls = turn:a turn:b\nturn_secret_file = %s\n"
        "[limits]\nintroductions_per_address_per_minute = 5\n" % secret
    )
    config = cfg.load(str(path))
    assert config.listen == [("127.0.0.1", 9000), ("::1", 9001)]
    assert config.turn_urls == ["turn:a", "turn:b"]
    assert config.turn_secret == b"s3cret"
    assert config.introductions_per_address_per_minute == 5
    assert config.introductions_per_station_per_minute == 60


@pytest.mark.parametrize(
    "text",
    [
        "[rendezvous]\nunknown = 1\n",
        "[other]\nx = 1\n",
        "[limits]\ncandidates_per_side = many\n",
        "[limits]\ncandidates_per_side = -1\n",
        "[rendezvous]\nlisten = nowhere\n",
        "[rendezvous]\nturn_ttl_seconds = 0\n",
        "[limits]\nconnections_per_address = 0\n",
        "[limits]\nstations_per_address = 0\n",
        "[limits]\nmax_connections = 0\n",
        "[limits]\nmax_stations = 0\n",
        "[limits]\nhandshake_timeout_ms = 0\n",
        "[limits]\nidle_timeout_ms = 0\n",
        "[limits]\nintroduction_lifetime_ms = 0\n",
        "[limits]\nmailbox_lifetime_ms = 0\n",
        "[limits]\nsend_queue_messages = 0\n",
        "[limits]\nsend_queue_bytes = 0\n",
        "[limits]\nsend_budget_bytes = 0\n",
        "[limits]\nsend_queue_bytes = 2000\nsend_budget_bytes = 1000\n",
        "[limits]\nintroductions_per_address_per_minute = 0\n",
        "[limits]\nping_interval_seconds = -1\n",
        "[limits]\nsend_stall_ms = 0\n",
        '[rendezvous]\nturn_urls = turn:a"b\n',
        "[rendezvous]\nturn_urls = turn:a\\b\n",
    ],
)
def test_config_refused(tmp_path, text):
    path = tmp_path / "r.conf"
    path.write_text(text)
    with pytest.raises(cfg.ConfigError):
        cfg.load(str(path))


def test_config_refused_in_code():
    """Service() checks a Config built in code (as the runner builds one)
    the same way."""
    config = cfg.Config()
    config.max_connections = 0
    with pytest.raises(cfg.ConfigError):
        Service(config, ManualClock())


def test_pings_may_be_off(tmp_path):
    path = tmp_path / "r.conf"
    path.write_text("[limits]\nping_interval_seconds = 0\nping_timeout_seconds = 0\n")
    assert cfg.load(str(path)).ping_interval_seconds == 0


def test_empty_secret_refused(tmp_path):
    secret = tmp_path / "secret"
    secret.write_text("\n")
    path = tmp_path / "r.conf"
    path.write_text("[rendezvous]\nturn_secret_file = %s\n" % secret)
    with pytest.raises(cfg.ConfigError):
        cfg.load(str(path))


def test_defaults():
    config = cfg.Config()
    assert config.listen == [("127.0.0.1", 8710), ("::1", 8710)]
    assert config.turn_ttl_seconds == 86400
    assert (config.introductions_per_address_per_minute, config.introductions_per_station_per_minute) == (30, 60)
    assert config.mailbox_opens_per_address_per_minute == 10
    assert config.candidates_per_side == 64
    assert config.introduction_lifetime_ms == 120000
    assert (config.connections_per_address, config.stations_per_address) == (16, 4)
    assert (config.max_connections, config.max_stations) == (256, 2000)
    assert (config.send_queue_bytes, config.send_budget_bytes) == (1048576, 33554432)
    assert config.send_stall_ms == 30000
    # IPv4 first in both lists (rendezvous document section 8); the sample
    # matches (test_sample_configuration_is_the_defaults) and coturn-check.sh
    # compares what setup-server.sh writes with these.
    assert config.stun_urls == ["stun:rv4.nereussdr.com:3478", "stun:rv6.nereussdr.com:3478"]
    assert config.turn_urls == [
        "turn:rv4.nereussdr.com:3478?transport=udp",
        "turn:rv4.nereussdr.com:443?transport=udp",
        "turn:rv6.nereussdr.com:3478?transport=udp",
        "turn:rv6.nereussdr.com:443?transport=udp",
    ]
    assert all("rv4.nereussdr.com" in u or "rv6.nereussdr.com" in u for u in config.turn_urls + config.stun_urls)
    assert {u.split(":")[2].split("?")[0] for u in config.turn_urls} == {"3478", "443"}


class _Headers:
    def __init__(self, values):
        self._values = values

    def get_all(self, name):
        return self._values if name.lower() == "x-forwarded-for" else []


class _Ws:
    def __init__(self, peer, forwarded):
        self.remote_address = (peer, 1234)
        self.request_headers = _Headers(forwarded)


@pytest.mark.parametrize(
    "peer,forwarded,want",
    [
        ("127.0.0.1", ["198.51.100.1"], "198.51.100.1"),
        ("::1", ["10.0.0.1, 2001:db8::9"], "2001:db8::9"),
        ("127.0.0.1", ["a, b", "198.51.100.2"], "198.51.100.2"),
        ("127.0.0.1", [], "127.0.0.1"),
        ("127.0.0.1", ["garbage"], "127.0.0.1"),
        ("203.0.113.5", ["198.51.100.1"], "203.0.113.5"),
    ],
)
def test_client_address(peer, forwarded, want):
    assert transport.client_address(_Ws(peer, forwarded), ["127.0.0.1", "::1"]) == want


def test_nameplates_lowest_free_and_capped():
    config = cfg.Config()
    service = Service(config, ManualClock())
    assert [service._allocate_nameplate() for _ in range(3)] == [1, 2, 3]
    service._next_nameplate = 999999
    assert service._allocate_nameplate() == 999999
    assert service._allocate_nameplate() is None


def _has_ipv6():
    try:
        with socket.socket(socket.AF_INET6) as s:
            s.bind(("::1", 0))
        return True
    except OSError:
        return False


def test_listens_on_both_families():
    if not _has_ipv6():
        pytest.skip("this machine has no IPv6 loopback")

    async def go():
        config = cfg.Config()
        config.ping_interval_seconds = 0
        service = Service(config, ManualClock())
        v4 = await transport.start(service, "127.0.0.1", 0)
        port = v4.sockets[0].getsockname()[1]
        v6 = await transport.start(service, "::1", port)
        try:
            for uri in (f"ws://127.0.0.1:{port}/", f"ws://[::1]:{port}/"):
                ws = await ws_connect(uri, "192.0.2.1")
                assert (await recv_json(ws))["type"] == "hello"
                await ws.close()
        finally:
            await transport.stop([v4, v6], service, grace_s=0.05, timeout_s=5)

    asyncio.run(go())


def test_stop_tells_every_connection():
    from websockets.exceptions import ConnectionClosed

    async def go():
        config = cfg.Config()
        config.ping_interval_seconds = 0
        service = Service(config, ManualClock())
        server = await transport.start(service, "127.0.0.1", 0)
        port = server.sockets[0].getsockname()[1]
        ws = await ws_connect(f"ws://127.0.0.1:{port}/", "192.0.2.1")
        await recv_json(ws)
        await transport.stop([server], service, grace_s=0.05, timeout_s=5)
        answer = await recv_json(ws)
        assert answer["code"] == "shuttingDown" and answer["retryAfterMs"] == 5000
        with pytest.raises(ConnectionClosed) as info:
            await asyncio.wait_for(ws.recv(), 5)
        assert info.value.rcvd.code == 1001

    asyncio.run(go())


def test_sample_configuration_is_the_defaults():
    from pathlib import Path

    sample = Path(__file__).resolve().parent.parent / "server" / "rendezvous.conf.sample"
    loaded = cfg.load(str(sample))
    defaults = cfg.Config()
    assert loaded == defaults
    # Order matters (section 8), and dataclass equality compares the lists in
    # order; said outright for the two URL lists.
    assert (loaded.stun_urls, loaded.turn_urls) == (defaults.stun_urls, defaults.turn_urls)


def test_missing_secret_file_is_a_configuration_error(tmp_path):
    path = tmp_path / "r.conf"
    path.write_text("[rendezvous]\nturn_secret_file = %s\n" % (tmp_path / "absent"))
    with pytest.raises(cfg.ConfigError):
        cfg.load(str(path))


def test_each_connection_gets_the_configured_socket_buffers():
    """Section 9.1: the kernel buffers of every accepted connection are set
    by the service (SO_RCVBUF and SO_SNDBUF, inherited from the listening
    socket), so the kernel memory a connection can hold is bounded without
    a host setting. Linux reports twice the value it was given."""

    async def go():
        config = cfg.Config()
        config.ping_interval_seconds = 0
        assert config.socket_buffer_bytes == 16384
        service = Service(config, ManualClock())
        server = await transport.start(service, "127.0.0.1", 0)
        port = server.sockets[0].getsockname()[1]
        try:
            ws = await ws_connect(f"ws://127.0.0.1:{port}/", "192.0.2.1")
            await recv_json(ws)
            (conn,) = service.connections
            sock = conn.transport.ws.transport.get_extra_info("socket")
            options = [socket.SO_SNDBUF]
            # macOS grows a connected socket's receive buffer on its own
            # whatever it was set to; Linux, where the service runs, keeps it.
            if sys.platform.startswith("linux"):
                options.append(socket.SO_RCVBUF)
            for option in options:
                assert sock.getsockopt(socket.SOL_SOCKET, option) in (16384, 32768), option
            await ws.close()
        finally:
            await transport.stop([server], service, grace_s=0.05, timeout_s=5)

    asyncio.run(go())


def test_socket_buffer_bytes_may_be_zero_but_not_negative(tmp_path):
    path = tmp_path / "r.conf"
    path.write_text("[limits]\nsocket_buffer_bytes = 0\n")
    assert cfg.load(str(path)).socket_buffer_bytes == 0
    path.write_text("[limits]\nsocket_buffer_bytes = -1\n")
    with pytest.raises(cfg.ConfigError):
        cfg.load(str(path))


async def _raw_request(port: int, request: bytes) -> bytes:
    reader, writer = await asyncio.open_connection("127.0.0.1", port)
    try:
        writer.write(request)
        await writer.drain()
        head = await asyncio.wait_for(reader.readuntil(b"\r\n\r\n"), 5)
        rest = b""
        if head.startswith(b"HTTP/1.1 101"):
            # The first frame: the service's hello, unmasked, a short text.
            first = await asyncio.wait_for(reader.readexactly(2), 5)
            length = first[1] & 0x7F
            if length == 126:
                length = int.from_bytes(await reader.readexactly(2), "big")
            rest = await asyncio.wait_for(reader.readexactly(length), 5)
        else:
            try:
                rest = await asyncio.wait_for(reader.read(), 5)
            except asyncio.IncompleteReadError:
                pass
        return head + rest
    finally:
        writer.close()


def _live(go):
    async def wrapper():
        config = cfg.Config()
        config.ping_interval_seconds = 0
        service = Service(config, ManualClock())
        server = await transport.start(service, "127.0.0.1", 0)
        port = server.sockets[0].getsockname()[1]
        try:
            await go(port)
        finally:
            await transport.stop([server], service, grace_s=0.05, timeout_s=5)

    asyncio.run(wrapper())


def test_a_request_that_is_not_a_websocket_gets_426():
    """Caddy sends every request for rv to the service
    (rendezvous/deploy/Caddyfile), so the service answers the rest itself:
    426, a short plain text, Upgrade: websocket, and no Server header."""

    async def go(port):
        answer = await _raw_request(port, b"GET / HTTP/1.1\r\nHost: rv.nereussdr.com\r\nConnection: keep-alive\r\n\r\n")
        head, _, body = answer.partition(b"\r\n\r\n")
        lines = head.decode("latin-1").split("\r\n")
        assert lines[0].startswith("HTTP/1.1 426"), lines[0]
        headers = {k.lower(): v.strip() for k, _, v in (line.partition(":") for line in lines[1:])}
        assert headers["upgrade"] == "websocket"
        assert headers["connection"] == "upgrade, close"
        assert headers["content-type"] == "text/plain; charset=utf-8"
        assert headers["cache-control"] == "no-store"
        assert "server" not in headers
        assert body.decode("utf-8") == transport.NOT_A_WEBSOCKET_TEXT

    _live(go)


@pytest.mark.parametrize(
    "upgrade,host",
    [
        ("websocket", "rv.nereussdr.com"),
        # Apple's Network.framework, as captured by the phone session.
        ("WebSocket", "rv.nereussdr.com:443"),
    ],
)
def test_websocket_upgrade_in_any_case_and_with_a_port_in_host(upgrade, host):
    async def go(port):
        request = (
            "GET / HTTP/1.1\r\nHost: %s\r\nUpgrade: %s\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n" % (host, upgrade)
        ).encode("ascii")
        answer = await _raw_request(port, request)
        head, _, frame = answer.partition(b"\r\n\r\n")
        assert head.startswith(b"HTTP/1.1 101"), head
        assert b'"type":"hello"' in frame, frame

    _live(go)


def _url_lists(value):
    if isinstance(value, list):
        if value and all(isinstance(x, str) and x.startswith(("stun:", "turn:")) for x in value):
            yield value
        for item in value:
            yield from _url_lists(item)
    elif isinstance(value, dict):
        for item in value.values():
            yield from _url_lists(item)


def test_every_url_list_puts_the_ipv4_name_first():
    """Section 8: IPv4 first, in the service's defaults, the sample, the
    runner's fixture settings and every URL list in the conformance
    vectors (coturn-check.sh checks what setup-server.sh writes)."""
    import json
    from pathlib import Path

    import runner

    root = Path(__file__).resolve().parent.parent / "conformance"
    lists = [runner.FIXTURE_STUN, runner.FIXTURE_TURN, cfg.Config().stun_urls, cfg.Config().turn_urls]
    for path in sorted(root.rglob("*.json")):
        lists.extend(_url_lists(json.loads(path.read_text(encoding="utf-8"))))
    assert len(lists) > 60
    for urls in lists:
        families = ["rv4" if "rv4." in u else "rv6" if "rv6." in u else "other" for u in urls]
        assert families == sorted(families, key=lambda f: {"rv4": 0, "rv6": 1, "other": 2}[f]), urls
