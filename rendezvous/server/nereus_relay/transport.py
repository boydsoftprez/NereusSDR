# no-port-check: NereusSDR-original.
"""The relay's WebSocket glue, on the rendezvous service's (which isolates
the two websockets APIs, 10.4 on Ubuntu 24.04 and 13 and later): the same
kernel buffer setting and 426 answer, with the relay's own listening
socket, message size, queue and client address.

The relay listens on a Unix stream socket, Caddy its only peer (rendezvous
document section 12.5). On Linux what waits in a Unix stream is charged to
its sender's send buffer, so with the relay's own SO_SNDBUF on every
accepted connection the hop from the relay to Caddy holds what the relay
allows and no more. There is no peer address on a Unix socket: the client's
address is the last one in X-Forwarded-For, which Caddy sets, and a request
without one is refused.
"""

from __future__ import annotations

import grp
import http
import logging
import os
import socket
from typing import Any, List, Optional

from nereus_rendezvous import transport as rv_transport

from .config import MAX_MESSAGE_BYTES
from .relay import Relay

# The relay reads each message as it arrives; a few waiting are enough, and
# with max_size they bound what a peer can make it hold on the way in.
MAX_QUEUE = 16
# The high-water mark of the library's write buffer. The relay's own
# drop-oldest queue sits in front of it (section 12.5), so this is kept to
# a few messages: bytes past the queue cannot be dropped any more.
WRITE_LIMIT_BYTES = 8192


class LegTransport:
    """What the relay needs from a connection."""

    def __init__(self, ws: Any) -> None:
        self.ws = ws

    async def send(self, frame: Any) -> None:
        await self.ws.send(bytes(frame))

    async def close(self, code: int) -> None:
        await self.ws.close(code, "")

    def __aiter__(self) -> Any:
        return self.ws.__aiter__()


def forwarded_address(headers: Any) -> Optional[str]:
    """The last address in X-Forwarded-For, the one Caddy added, or None."""
    values = list(headers.get_all("X-Forwarded-For")) if headers is not None and hasattr(headers, "get_all") else []
    if not values:
        return None
    return rv_transport._parse_ip(values[-1].split(",")[-1])


_NO_ADDRESS_TEXT = "This address takes relay connections through the NereusSDR server only.\n"

if rv_transport.NEW_API:

    async def _process_request(connection: Any, request: Any) -> Any:
        if forwarded_address(request.headers) is None:
            return connection.respond(http.HTTPStatus.BAD_REQUEST, _NO_ADDRESS_TEXT)
        return await rv_transport._process_request(connection, request)

else:

    async def _process_request(path: str, request_headers: Any) -> Any:  # type: ignore[misc]
        if forwarded_address(request_headers) is None:
            return (
                http.HTTPStatus.BAD_REQUEST,
                [("Content-Type", "text/plain; charset=utf-8"), ("Connection", "close")],
                _NO_ADDRESS_TEXT.encode("utf-8"),
            )
        return await rv_transport._process_request(path, request_headers)


def serve_kwargs(relay: Relay) -> dict:
    config = relay.config
    kwargs = dict(
        max_size=MAX_MESSAGE_BYTES,
        max_queue=MAX_QUEUE,
        write_limit=WRITE_LIMIT_BYTES,
        ping_interval=config.ping_interval_seconds or None,
        ping_timeout=config.ping_timeout_seconds or None,
        # A closing leg holds its place per address until it has closed
        # (section 12.5), so a peer that never answers the close is dropped
        # after 2 s.
        close_timeout=2,
        compression=None,
        server_header=None,
        process_request=_process_request,
    )
    if rv_transport.NEW_API:
        kwargs["open_timeout"] = config.join_timeout_ms / 1000.0
    return kwargs


def unix_listening_socket(path: str, mode: int, group: str, buffer_bytes: int) -> socket.socket:
    """The listening Unix socket at `path`, with its buffers set before it
    listens, then the file's mode and group (who may connect). A socket file
    left behind by an earlier run is replaced."""
    try:
        if os.path.exists(path) and not os.path.isdir(path):
            os.unlink(path)
    except OSError:
        pass
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        if buffer_bytes:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, buffer_bytes)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, buffer_bytes)
        sock.bind(path)
        if group:
            os.chown(path, -1, grp.getgrnam(group).gr_gid)
        os.chmod(path, mode)
        sock.setblocking(False)
    except (OSError, KeyError):
        sock.close()
        raise
    return sock


async def start(relay: Relay) -> Any:
    """Listen on the relay's Unix socket; returns the server."""

    async def handler(ws: Any) -> None:
        # Set again on the accepted connection: its send buffer is what the
        # hop to Caddy can hold (section 12.5).
        rv_transport.set_buffers(ws, relay.config.socket_buffer_bytes)
        address = forwarded_address(rv_transport._request_headers(ws)) or ""
        await relay.run_connection(LegTransport(ws), address)

    config = relay.config
    sock = unix_listening_socket(config.socket, config.socket_mode, config.socket_group, config.socket_buffer_bytes)
    return await rv_transport._serve(
        handler,
        sock=sock,
        logger=logging.getLogger("websockets.quiet"),
        **serve_kwargs(relay),
    )


async def stop(servers: List[Any], relay: Relay, grace_s: float = 0.5, timeout_s: float = 10.0) -> None:
    """Stop listening, end every leg with shuttingDown, and wait for them to
    close (the order the rendezvous service's stop explains)."""
    await rv_transport.stop(servers, relay, grace_s=grace_s, timeout_s=timeout_s)
    try:
        os.unlink(relay.config.socket)
    except OSError:
        pass
