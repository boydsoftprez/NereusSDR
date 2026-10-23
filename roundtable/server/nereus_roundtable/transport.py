# no-port-check: NereusSDR-original.
"""The listeners, and the WebSocket glue, isolated because the two
websockets APIs differ (wire spec 2.1 to 2.3, 9.1).

Ubuntu 24.04 ships python3-websockets 10.4, whose `serve` is the legacy
asyncio implementation; websockets 13 and later have the new one in
`websockets.asyncio.server`. A one-argument handler, `async for` over a
connection, `send`, `ping`, `close(code, reason)` and `remote_address` are
the same on both. The request path, the request headers, the answer to a
request that is not an upgrade and the opening-handshake timeout are not,
and are handled here and nowhere else.

The WebSocket listener serves `/v1/station` and `/v1/live`; a request to
either that is not an upgrade gets 426 and any other path 404. The HTTP
listener hands each connection to the HTTP app (the read API and the web
pages); until there is one it answers 404.
"""

from __future__ import annotations

import asyncio
import http
import ipaddress
import logging
import socket
from typing import Any, Iterable, List, Optional, Sequence

from websockets.exceptions import ConnectionClosed

from .config import parse_listen

try:  # websockets 13 and later
    from websockets.asyncio.server import serve as _serve

    NEW_API = True
except ImportError:  # websockets 10.x to 12.x: the legacy implementation
    from websockets.legacy.server import serve as _serve  # type: ignore[no-redef]

    NEW_API = False

log = logging.getLogger("nereus_roundtable")

STATION_PATH = "/v1/station"
LIVE_PATH = "/v1/live"
WS_PATHS = (STATION_PATH, LIVE_PATH)

MAX_QUEUE = 1
WRITE_LIMIT_BYTES = 32768
CLOSE_TIMEOUT_S = 5
HTTP_HEAD_LIMIT_BYTES = 16384
HTTP_HEAD_TIMEOUT_S = 10


# ----------------------------------------------------------------- addresses


def _parse_ip(text: str) -> Optional[str]:
    try:
        return str(ipaddress.ip_address(text.strip().strip("[]").split("%", 1)[0]))
    except ValueError:
        return None


def forwarded_address(peer: str, forwarded: Sequence[str], trusted_proxies: Iterable[str]) -> str:
    """The client's address: the peer's, or, when the peer is a trusted
    proxy (Caddy on loopback), the last address in X-Forwarded-For, which is
    the one the proxy itself added (wire spec 9.1). `forwarded` is every
    X-Forwarded-For header value, in order. Empty when the peer has none."""
    peer_ip = _parse_ip(peer) if peer else None
    if peer_ip is None:
        return ""
    trusted = {p for p in (_parse_ip(t) for t in trusted_proxies) if p is not None}
    if peer_ip not in trusted or not forwarded:
        return peer_ip
    last = _parse_ip(forwarded[-1].split(",")[-1])
    return last if last is not None else peer_ip


def _request_headers(ws: Any) -> Any:
    request = getattr(ws, "request", None)
    if request is not None and hasattr(request, "headers"):
        return request.headers
    return getattr(ws, "request_headers", None)


def _request_path(ws: Any) -> str:
    request = getattr(ws, "request", None)
    path = request.path if request is not None and hasattr(request, "path") else getattr(ws, "path", "")
    return str(path or "").split("?", 1)[0]


def client_address(ws: Any, trusted_proxies: Iterable[str]) -> str:
    """forwarded_address for a WebSocket connection of either API."""
    remote = getattr(ws, "remote_address", None)
    peer = str(remote[0]) if remote else ""
    headers = _request_headers(ws)
    values: List[str] = list(headers.get_all("X-Forwarded-For")) if headers is not None and hasattr(headers, "get_all") else []
    return forwarded_address(peer, values, trusted_proxies)


# ----------------------------------------------------------------- handshake

NOT_A_WEBSOCKET_TEXT = (
    "This address is the NereusSDR Roundtable. It takes WebSocket "
    "connections from NereusSDR and other station software only.\n"
)
NOT_FOUND_TEXT = "Not found.\n"
# RFC 9110: a 426 names the protocol in Upgrade (section 15.5.22), and a
# sender of Upgrade lists it in Connection (section 7.8); "close" because
# the server ends the connection after this answer.
_NOT_A_WEBSOCKET_HEADERS = (
    ("Upgrade", "websocket"),
    ("Connection", "upgrade, close"),
    ("Content-Type", "text/plain; charset=utf-8"),
    ("Cache-Control", "no-store"),
)
_NOT_FOUND_HEADERS = (
    ("Connection", "close"),
    ("Content-Type", "text/plain; charset=utf-8"),
    ("Cache-Control", "no-store"),
)


def is_websocket_upgrade(headers: Any) -> bool:
    """True when the request asks for a WebSocket: an Upgrade header with
    the token websocket, in any case (wire spec 2.1). The library then
    checks the rest of the handshake as usual."""
    values = list(headers.get_all("Upgrade")) if hasattr(headers, "get_all") else []
    return any(token.strip().lower() == "websocket" for value in values for token in value.split(","))


def _refusal(path: str, headers: Any) -> Optional[Any]:
    """(status, headers, text) for a request the WebSocket listener does not
    take, or None to go on with the handshake."""
    if path.split("?", 1)[0] not in WS_PATHS:
        return http.HTTPStatus.NOT_FOUND, _NOT_FOUND_HEADERS, NOT_FOUND_TEXT
    if not is_websocket_upgrade(headers):
        return http.HTTPStatus.UPGRADE_REQUIRED, _NOT_A_WEBSOCKET_HEADERS, NOT_A_WEBSOCKET_TEXT
    return None


if NEW_API:

    async def _process_request(connection: Any, request: Any) -> Any:
        refusal = _refusal(request.path, request.headers)
        if refusal is None:
            return None
        status, headers, text = refusal
        response = connection.respond(status, text)
        for name, value in headers:
            if name in response.headers:
                del response.headers[name]
            response.headers[name] = value
        return response

else:

    async def _process_request(path: str, request_headers: Any) -> Any:  # type: ignore[misc]
        refusal = _refusal(path, request_headers)
        if refusal is None:
            return None
        status, headers, text = refusal
        # The legacy implementation needs an HTTPStatus, not a number.
        return status, list(headers), text.encode("utf-8")


def serve_kwargs(config: Any) -> dict:
    # max_size: the inbound cap, at the WebSocket layer, before anything is
    # decoded; a longer message closes 1009 (wire spec 2.2). The library's
    # own pings are off: the heartbeat below sends them and closes with the
    # wire spec's code.
    kwargs = dict(
        max_size=config.inbound_message_bytes,
        max_queue=MAX_QUEUE,
        write_limit=WRITE_LIMIT_BYTES,
        ping_interval=None,
        ping_timeout=None,
        close_timeout=CLOSE_TIMEOUT_S,
        compression=None,
        server_header=None,
        process_request=_process_request,
    )
    if NEW_API:
        kwargs["open_timeout"] = config.hello_timeout_ms / 1000.0
    return kwargs


# ----------------------------------------------------------------- connections


class WsTransport:
    """What the service needs from a connection: send, close and the
    incoming frames. It notes when the last frame arrived, for the
    heartbeat."""

    def __init__(self, ws: Any) -> None:
        self.ws = ws
        self.last_frame = asyncio.get_running_loop().time()

    def touch(self) -> None:
        self.last_frame = asyncio.get_running_loop().time()

    async def send(self, text: str) -> None:
        await self.ws.send(text)

    async def close(self, code: int) -> None:
        await self.ws.close(code, "")

    def __aiter__(self) -> "WsTransport":
        self._frames = self.ws.__aiter__()
        return self

    async def __anext__(self) -> Any:
        frame = await self._frames.__anext__()
        self.touch()
        return frame


async def heartbeat(transport: WsTransport, interval_ms: int, silence_ms: int) -> None:
    """Wire spec 2.3: a ping every interval; close with 1000 and no message
    when no message or pong has arrived for silence_ms."""
    loop = asyncio.get_running_loop()
    interval_s = interval_ms / 1000.0
    silence_s = silence_ms / 1000.0
    next_ping = loop.time() + interval_s

    def ponged(future: "asyncio.Future[Any]") -> None:
        if not future.cancelled() and future.exception() is None:
            transport.touch()

    while True:
        wake = min(next_ping, transport.last_frame + silence_s)
        await asyncio.sleep(max(0.0, wake - loop.time()))
        now = loop.time()
        if now >= transport.last_frame + silence_s:
            log.info("closed a connection that went silent")
            await transport.close(1000)
            return
        if now >= next_ping:
            next_ping = now + interval_s
            try:
                waiter = await transport.ws.ping()
            except ConnectionClosed:
                return
            asyncio.ensure_future(waiter).add_done_callback(ponged)


# ----------------------------------------------------------------- listeners


def listening_socket(host: str, port: int) -> socket.socket:
    """A bound listening socket; an IPv6 one takes IPv6 only, so the IPv4
    and IPv6 loopback addresses can share a port."""
    family = socket.AF_INET6 if ":" in host else socket.AF_INET
    sock = socket.socket(family, socket.SOCK_STREAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if family == socket.AF_INET6:
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
        sock.bind((host, port))
        sock.setblocking(False)
    except OSError:
        sock.close()
        raise
    return sock


class Listeners:
    def __init__(self) -> None:
        self.ws_servers: List[Any] = []
        self.http_servers: List[Any] = []

    def ws_port(self, index: int = 0) -> int:
        return int(self.ws_servers[index].sockets[0].getsockname()[1])

    def http_port(self, index: int = 0) -> int:
        return int(self.http_servers[index].sockets[0].getsockname()[1])


async def _not_found(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    """The HTTP listener's answer while there is no HTTP app."""
    try:
        await asyncio.wait_for(reader.readuntil(b"\r\n\r\n"), HTTP_HEAD_TIMEOUT_S)
        body = NOT_FOUND_TEXT.encode("utf-8")
        head = (
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            f"Content-Length: {len(body)}\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n\r\n"
        )
        writer.write(head.encode("ascii") + body)
        await writer.drain()
    except (asyncio.TimeoutError, asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError):
        pass
    finally:
        writer.close()


async def start(service: Any, http_app: Any, config: Any) -> Listeners:
    """Listen on every ws_listen and http_listen address. `http_app` has
    `async handle(reader, writer)`, or is None."""
    listeners = Listeners()

    async def handler(ws: Any) -> None:
        path = _request_path(ws)
        address = client_address(ws, config.trusted_proxies)
        transport = WsTransport(ws)
        beat = None
        if config.ping_interval_ms:
            beat = asyncio.ensure_future(heartbeat(transport, config.ping_interval_ms, config.silence_close_ms))
        try:
            if path == STATION_PATH:
                await service.handle_station(transport, address)
            elif path == LIVE_PATH:
                await service.handle_live(transport, address)
        finally:
            if beat is not None:
                beat.cancel()

    http_handler = http_app.handle if http_app is not None else _not_found
    try:
        for item in config.ws_listen:
            host, port = parse_listen(item)
            server = await _serve(
                handler,
                sock=listening_socket(host, port),
                logger=logging.getLogger("websockets.quiet"),
                **serve_kwargs(config),
            )
            listeners.ws_servers.append(server)
        for item in config.http_listen:
            host, port = parse_listen(item)
            server = await asyncio.start_server(
                http_handler, sock=listening_socket(host, port), limit=HTTP_HEAD_LIMIT_BYTES
            )
            listeners.http_servers.append(server)
    except BaseException:
        for server in listeners.ws_servers + listeners.http_servers:
            server.close()
        raise
    return listeners


async def stop(listeners: Listeners, service: Any, grace_s: float = 0.5, timeout_s: float = 10.0) -> None:
    """Stop listening, close every connection with 1001, and wait for them
    to end.

    Order matters on Python 3.12 with the legacy implementation: its close()
    waits for the connections to end before it closes them, so the service
    closes them itself. The new implementation would close them before the
    service does, so it is asked not to."""
    for server in listeners.ws_servers:
        try:
            server.close(close_connections=False)
        except TypeError:
            server.close()
    for server in listeners.http_servers:
        server.close()
    service.shutdown()
    await asyncio.sleep(grace_s)
    for server in listeners.ws_servers + listeners.http_servers:
        try:
            await asyncio.wait_for(server.wait_closed(), timeout_s)
        except asyncio.TimeoutError:
            log.warning("some connections did not close in time")
