# no-port-check: NereusSDR-original.
"""The WebSocket glue, isolated because the two websockets APIs differ.

Ubuntu 24.04 ships python3-websockets 10.4, whose `websockets.serve` is the
legacy asyncio implementation; websockets 13 and later have the new asyncio
implementation in `websockets.asyncio.server` (the default `serve` from
14). A one-argument handler, `async for` over a connection, `send`,
`close(code, reason)` and `remote_address` are the same on both. The
request headers and the opening-handshake timeout are not, and are handled
here.
"""

from __future__ import annotations

import asyncio
import http
import ipaddress
import logging
import socket
from typing import Any, Iterable, List, Optional

from . import protocol

try:  # websockets 13 and later
    from websockets.asyncio.server import serve as _serve

    NEW_API = True
except ImportError:  # websockets 10.x to 12.x: the legacy implementation
    from websockets.legacy.server import serve as _serve  # type: ignore[no-redef]

    NEW_API = False


def _request_headers(ws: Any) -> Any:
    request = getattr(ws, "request", None)
    if request is not None and hasattr(request, "headers"):
        return request.headers
    return getattr(ws, "request_headers", None)


def _parse_ip(text: str) -> Optional[str]:
    try:
        return str(ipaddress.ip_address(text.strip().strip("[]").split("%", 1)[0]))
    except ValueError:
        return None


def client_address(ws: Any, trusted_proxies: Iterable[str]) -> str:
    """The client's address: the peer's, or, when the peer is a trusted
    proxy (Caddy on loopback), the last address in X-Forwarded-For, which is
    the one the proxy itself added."""
    remote = getattr(ws, "remote_address", None)
    peer = _parse_ip(str(remote[0])) if remote else None
    if peer is None:
        return ""
    trusted = {p for p in (_parse_ip(t) for t in trusted_proxies) if p is not None}
    if peer not in trusted:
        return peer
    headers = _request_headers(ws)
    if headers is None:
        return peer
    values: List[str] = list(headers.get_all("X-Forwarded-For")) if hasattr(headers, "get_all") else []
    if not values:
        return peer
    last = values[-1].split(",")[-1]
    forwarded = _parse_ip(last)
    return forwarded if forwarded is not None else peer


class WsTransport:
    """What the service needs from a connection."""

    def __init__(self, ws: Any) -> None:
        self.ws = ws

    async def send(self, text: str) -> None:
        await self.ws.send(text)

    async def close(self, code: int) -> None:
        await self.ws.close(code, "")

    def __aiter__(self) -> Any:
        return self.ws.__aiter__()


# The answer to anything that is not a WebSocket upgrade. Caddy sends every
# request for the service's host name here (rendezvous/deploy/Caddyfile),
# so this is what a browser, or a probe, sees.
NOT_A_WEBSOCKET_TEXT = (
    "This address is the NereusSDR connection service. It takes WebSocket "
    "connections from NereusSDR and the NereusSDR app only.\n"
)
# RFC 9110: a 426 names the protocol in Upgrade (section 15.5.22), and a
# sender of Upgrade lists it in Connection (section 7.8); "close" because
# the service ends the connection after this answer.
_NOT_A_WEBSOCKET_HEADERS = (
    ("Upgrade", "websocket"),
    ("Connection", "upgrade, close"),
    ("Content-Type", "text/plain; charset=utf-8"),
    ("Cache-Control", "no-store"),
)


def is_websocket_upgrade(headers: Any) -> bool:
    """True when the request asks for a WebSocket: an Upgrade header with
    the token websocket, in any case (RFC 6455 section 4.2.1; Apple's
    Network.framework writes "WebSocket"). The library then checks the
    rest of the handshake as usual."""
    values = list(headers.get_all("Upgrade")) if hasattr(headers, "get_all") else []
    return any(token.strip().lower() == "websocket" for value in values for token in value.split(","))


if NEW_API:

    async def _process_request(connection: Any, request: Any) -> Any:
        if is_websocket_upgrade(request.headers):
            return None
        response = connection.respond(http.HTTPStatus.UPGRADE_REQUIRED, NOT_A_WEBSOCKET_TEXT)
        for name, value in _NOT_A_WEBSOCKET_HEADERS:
            if name in response.headers:
                del response.headers[name]
            response.headers[name] = value
        return response

else:

    async def _process_request(path: str, request_headers: Any) -> Any:  # type: ignore[misc]
        if is_websocket_upgrade(request_headers):
            return None
        # The legacy implementation needs an HTTPStatus, not a number.
        return http.HTTPStatus.UPGRADE_REQUIRED, list(_NOT_A_WEBSOCKET_HEADERS), NOT_A_WEBSOCKET_TEXT.encode("utf-8")


MAX_QUEUE = 1
WRITE_LIMIT_BYTES = 32768


def serve_kwargs(config: Any) -> dict:
    # max_queue: the service reads every message as soon as it arrives, so
    # one waiting message is enough; with max_size it bounds what a peer can
    # make the service hold on the way in (rendezvous document section 9.1).
    # write_limit: the high-water mark of the write buffer, in bytes, under
    # the same name and meaning in websockets 10.x (legacy) and 13 and later;
    # the service's own outbound queue (send_queue_bytes) sits in front of it.
    kwargs = dict(
        max_size=protocol.MAX_MESSAGE_BYTES,
        max_queue=MAX_QUEUE,
        write_limit=WRITE_LIMIT_BYTES,
        ping_interval=config.ping_interval_seconds or None,
        ping_timeout=config.ping_timeout_seconds or None,
        close_timeout=5,
        compression=None,
        server_header=None,
        process_request=_process_request,
    )
    if NEW_API:
        kwargs["open_timeout"] = config.handshake_timeout_ms / 1000.0
    return kwargs


def listening_socket(host: str, port: int, buffer_bytes: int) -> socket.socket:
    """The listening socket, made here rather than by asyncio so its kernel
    buffers can be set before it listens: a connection it accepts inherits
    them, so the kernel memory each connection can hold is bounded by the
    service itself, without any host setting (rendezvous document section
    9.1). 0 leaves the kernel's own sizing."""
    family = socket.AF_INET6 if ":" in host else socket.AF_INET
    sock = socket.socket(family, socket.SOCK_STREAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if family == socket.AF_INET6:
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
        if buffer_bytes:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, buffer_bytes)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, buffer_bytes)
        sock.bind((host, port))
        sock.setblocking(False)
    except OSError:
        sock.close()
        raise
    return sock


def set_buffers(ws: Any, buffer_bytes: int) -> None:
    """Set the accepted connection's buffers again. Linux already gave it
    the listening socket's; macOS, where the tests also run, does not pass
    them on."""
    if not buffer_bytes:
        return
    transport = getattr(ws, "transport", None)
    sock = transport.get_extra_info("socket") if transport is not None else None
    if sock is None:
        return
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, buffer_bytes)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, buffer_bytes)
    except OSError:
        return


async def start(service: Any, host: str, port: int) -> Any:
    """Listen on one address; returns the server (use .sockets for the
    bound port and .close() / .wait_closed() to stop)."""

    async def handler(ws: Any) -> None:
        set_buffers(ws, service.config.socket_buffer_bytes)
        address = client_address(ws, service.config.trusted_proxies)
        await service.run_connection(WsTransport(ws), address)

    sock = listening_socket(host, port, service.config.socket_buffer_bytes)
    return await _serve(
        handler,
        sock=sock,
        logger=logging.getLogger("websockets.quiet"),
        **serve_kwargs(service.config),
    )


async def stop(servers: List[Any], service: Any, grace_s: float = 0.5, timeout_s: float = 10.0) -> None:
    """Stop listening, tell every connection the service is going
    (shuttingDown), and wait for them to close.

    Order matters on Python 3.12 with the legacy implementation: its close()
    waits for the listening socket's connections to end before it closes
    them, so the service closes them itself. The new implementation would
    close them with 1001 before the message goes out, so it is asked not
    to."""
    for server in servers:
        try:
            server.close(close_connections=False)
        except TypeError:
            server.close()
    service.shutdown()
    await asyncio.sleep(grace_s)
    for server in servers:
        try:
            await asyncio.wait_for(server.wait_closed(), timeout_s)
        except asyncio.TimeoutError:
            logging.getLogger("nereus_rendezvous").warning("some connections did not close in time")
