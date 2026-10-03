# no-port-check: NereusSDR-original.
"""The client side of rendezvous/tests/caddy-check.sh: WebSockets through
Caddy to the rendezvous service, by host name, over TLS checked against the
local certificate authority Caddy made for the test.

Not a pytest module (no test_ prefix); the check runs it in its client
containers, on Ubuntu's python3 and python3-websockets.

  open URL --cacert F --count N [--hold S]
      Open N WebSockets one after another, each claiming a different
      X-Forwarded-For of its own (which Caddy must replace), print the first
      message each receives, then keep the open ones for S seconds.

  raw HOST --cacert F --upgrade VALUE --host-header VALUE
      One WebSocket opening request over HTTP/1.1, written byte for byte
      (for the exact headers of a given client), and what came back: the
      status line and whether the first frame was the service's hello.

  h2 HOST --cacert F
      What an HTTP/2 client gets: whether the server offers WebSockets over
      HTTP/2 (RFC 8441, SETTINGS_ENABLE_CONNECT_PROTOCOL), the status of a
      plain GET, and, when offered, the status of an extended CONNECT for a
      WebSocket and whether the service's hello came back on it.

  relay URL --cacert F --secret-file F [--extra N]
      The WebSocket relay behind the same name (rendezvous document section
      12): a Core leg and a device leg of one grant (minted here with the
      relay's secret) join through Caddy, datagrams of every size up to the
      cap cross both ways, then N more connections are opened from this
      client and the first frame each gets within 1 s is reported.

  slowreader URL --cacert F --secret-file F [--seconds S] [--send-rate N]
      [--read-rate N] [--rcvbuf B]
      Where delay builds up behind the relay's queue (rendezvous document
      section 12.5): the device leg sends N media datagrams a second, each
      carrying a sequence number and the time it was sent, for S seconds;
      the Core leg, a blocking socket with a small receive buffer and no
      library buffering in front of it, reads only N a second.
      Reports how old the datagrams are when read, early and late, and how
      many were lost on the way. A queue that drops its oldest keeps the age
      flat; buffering behind it lets the age grow for as long as the
      sender outruns the reader.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import base64
import os
import socket
import ssl
import struct
import sys

try:  # websockets 13 and later
    from websockets.asyncio.client import connect as _connect

    _HEADERS = "additional_headers"
except ImportError:  # Ubuntu 24.04's 10.4
    from websockets.legacy.client import connect as _connect  # type: ignore[no-redef]

    _HEADERS = "extra_headers"


async def run(args) -> int:
    context = ssl.create_default_context(cafile=args.cacert)
    held = []
    for n in range(args.count):
        claimed = "203.0.113.%d" % (n + 1)
        try:
            ws = await _connect(args.url, ssl=context, **{_HEADERS: {"X-Forwarded-For": claimed}})
        except Exception as exc:  # noqa: BLE001 - report and go on
            print(json.dumps({"n": n, "upgraded": False, "error": type(exc).__name__}), flush=True)
            continue
        first = json.loads(await asyncio.wait_for(ws.recv(), 5))
        print(json.dumps({"n": n, "upgraded": True, "first": first.get("type"), "code": first.get("code")}), flush=True)
        held.append(ws)
    await asyncio.sleep(args.hold)
    for ws in held:
        await ws.close()
    return 0


def tls(host: str, cacert: str, alpn: str) -> ssl.SSLSocket:
    context = ssl.create_default_context(cafile=cacert)
    context.set_alpn_protocols([alpn])
    sock = socket.create_connection((host, 443), timeout=10)
    return context.wrap_socket(sock, server_hostname=host)


def raw(args) -> int:
    conn = tls(args.host, args.cacert, "http/1.1")
    key = base64.b64encode(os.urandom(16)).decode()
    request = (
        "GET / HTTP/1.1\r\nHost: %s\r\nUpgrade: %s\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (args.host_header, args.upgrade, key)
    )
    conn.sendall(request.encode("ascii"))
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            break
        data += chunk
    head, _, rest = data.partition(b"\r\n\r\n")
    status = head.split(b"\r\n", 1)[0].decode("latin-1")
    hello = False
    if " 101 " in status + " ":
        while b'"type":"hello"' not in rest:
            chunk = conn.recv(4096)
            if not chunk:
                break
            rest += chunk
        hello = b'"type":"hello"' in rest
    print(json.dumps({"upgrade": args.upgrade, "host": args.host_header, "status": status, "hello": hello}), flush=True)
    conn.close()
    return 0


# HTTP/2, just enough to see a response's status (RFC 9113, RFC 7541).
_STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8A: 206, 0x8B: 304, 0x8C: 400, 0x8D: 404, 0x8E: 500}
# RFC 7541 Appendix B, the digits only: a status is three digits.
_HUFFMAN_DIGITS = {
    "00000": "0", "00001": "1", "00010": "2", "011001": "3", "011010": "4",
    "011011": "5", "011100": "6", "011101": "7", "011110": "8", "011111": "9",
}


def _huffman_digits(raw_bytes: bytes) -> str:
    bits = "".join("{:08b}".format(b) for b in raw_bytes)
    out, code = "", ""
    for bit in bits:
        code += bit
        if code in _HUFFMAN_DIGITS:
            out += _HUFFMAN_DIGITS[code]
            code = ""
    return out


def _status(block: bytes) -> int:
    first = block[0]
    if first in _STATIC_STATUS:
        return _STATIC_STATUS[first]
    # A literal whose name is one of the static ":status" entries (8 to 14),
    # with or without indexing.
    if 0x48 <= first <= 0x4E or 0x08 <= first <= 0x0E or 0x18 <= first <= 0x1E:
        huffman, length = block[1] & 0x80, block[1] & 0x7F
        value = block[2:2 + length]
        return int(_huffman_digits(value) if huffman else value.decode("ascii"))
    raise ValueError("unexpected first header field %#x" % first)


def _literal(name: str, value: str) -> bytes:
    return bytes([0x00, len(name)]) + name.encode() + bytes([len(value)]) + value.encode()


def _frame(kind: int, flags: int, stream: int, payload: bytes) -> bytes:
    return struct.pack("!I", len(payload))[1:] + bytes([kind, flags]) + struct.pack("!I", stream) + payload


class H2:
    def __init__(self, host: str, cacert: str) -> None:
        self.conn = tls(host, cacert, "h2")
        self.alpn = self.conn.selected_alpn_protocol()
        self.buffer = b""
        self.conn.sendall(b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n" + _frame(4, 0, 0, b""))
        self.settings = {}

    def read_frame(self):
        while len(self.buffer) < 9:
            chunk = self.conn.recv(65536)
            if not chunk:
                raise EOFError
            self.buffer += chunk
        length = int.from_bytes(self.buffer[:3], "big")
        while len(self.buffer) < 9 + length:
            chunk = self.conn.recv(65536)
            if not chunk:
                raise EOFError
            self.buffer += chunk
        kind, flags = self.buffer[3], self.buffer[4]
        stream = int.from_bytes(self.buffer[5:9], "big") & 0x7FFFFFFF
        payload = self.buffer[9:9 + length]
        self.buffer = self.buffer[9 + length:]
        if kind == 4 and not flags & 1:
            for i in range(0, len(payload), 6):
                ident, value = struct.unpack("!HI", payload[i:i + 6])
                self.settings[ident] = value
            self.conn.sendall(_frame(4, 1, 0, b""))
        return kind, flags, stream, payload

    def request(self, headers, end_stream: bool, want_hello: bool = False):
        block = b"".join(_literal(n, v) for n, v in headers)
        self.conn.sendall(_frame(1, 0x04 | (0x01 if end_stream else 0), 1, block))
        status, hello, reset = None, False, None
        data = b""
        while True:
            try:
                kind, flags, stream, payload = self.read_frame()
            except (EOFError, socket.timeout, OSError):
                break
            if stream != 1:
                continue
            if kind == 1:
                status = _status(payload)
                if flags & 1 or not want_hello:
                    break
            elif kind == 0:
                data += payload
                if b'"type":"hello"' in data:
                    hello = True
                    break
                if flags & 1:
                    break
            elif kind == 3:
                reset = struct.unpack("!I", payload[:4])[0]
                break
        return {"status": status, "hello": hello, "reset": reset}


def h2(args) -> int:
    probe = H2(args.host, args.cacert)
    if probe.alpn != "h2":
        print(json.dumps({"alpn": probe.alpn}), flush=True)
        return 0
    get = probe.request([(":method", "GET"), (":scheme", "https"), (":authority", args.host), (":path", "/")], True)
    connect_offered = probe.settings.get(8) == 1
    connect = None
    if connect_offered:
        probe = H2(args.host, args.cacert)
        # Read the server's SETTINGS first, as RFC 8441 requires.
        while 8 not in probe.settings:
            probe.read_frame()
        connect = probe.request(
            [(":method", "CONNECT"), (":protocol", "websocket"), (":scheme", "https"),
             (":authority", args.host), (":path", "/"), ("sec-websocket-version", "13")],
            False, want_hello=True)
    print(json.dumps({"alpn": "h2", "serverSettings": sorted(probe.settings), "enableConnectProtocol": connect_offered,
                      "get": get, "connect": connect}), flush=True)
    return 0


async def relay(args) -> int:
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "server"))
    from nereus_rendezvous import relaygrant

    context = ssl.create_default_context(cafile=args.cacert)
    secret = open(args.secret_file, "rb").read().rstrip(b"\r\n")
    session = os.urandom(relaygrant.SESSION_BYTES)
    expires = int(__import__("time").time()) + 120
    result = {}

    async def leg(side, claimed):
        ws = await _connect(args.url, ssl=context, max_size=None, **{_HEADERS: {"X-Forwarded-For": claimed}})
        await ws.send(b"\x80" + relaygrant.mint(secret, side, session, relaygrant.station_of(secret, "caddycheckcaddycheckcaddyc"), expires).encode())
        return ws, await asyncio.wait_for(ws.recv(), 5)

    core, result["coreReady"] = await leg(relaygrant.LEG_CORE, "203.0.113.1")
    device, result["deviceReady"] = await leg(relaygrant.LEG_DEVICE, "203.0.113.2")
    result["corePeer"] = await asyncio.wait_for(core.recv(), 5)
    crossed = 0
    for size in (1, 100, 1200, 1500):
        for tag in (1, 2):
            frame = bytes([tag]) + os.urandom(size)
            await device.send(frame)
            crossed += await asyncio.wait_for(core.recv(), 5) == frame
            await core.send(frame)
            crossed += await asyncio.wait_for(device.recv(), 5) == frame
    result["crossed"] = crossed
    result["extra"] = []
    extra = []
    for n in range(args.extra):
        ws = await _connect(args.url, ssl=context, **{_HEADERS: {"X-Forwarded-For": "203.0.113.%d" % (n + 10)}})
        extra.append(ws)
        try:
            first = await asyncio.wait_for(ws.recv(), 1)
            result["extra"].append(first[1:].decode() if first[:1] == b"\x83" else first.hex())
        except asyncio.TimeoutError:
            result["extra"].append(None)
    for ws in [core, device] + extra:
        await ws.close()
    result = {k: (v.hex() if isinstance(v, bytes) else v) for k, v in result.items()}
    print(json.dumps(result), flush=True)
    return 0


class _SlowLeg:
    """A WebSocket leg that reads its socket only as fast as it is asked to:
    a blocking socket (TLS or not) with a small receive buffer, reading one
    frame at a time with no library buffering in front of it."""

    def __init__(self, url: str, cacert: str, rcvbuf: int, claimed: str) -> None:
        from urllib.parse import urlsplit

        if url.startswith("unix:"):
            # The relay's own socket, as Caddy reaches it.
            parts = urlsplit("ws://localhost/v1/relay")
            host = "localhost"
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
            sock.connect(url[len("unix:"):])
        else:
            parts = urlsplit(url)
            host, port = parts.hostname, parts.port or (443 if parts.scheme == "wss" else 80)
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
            sock.connect((host, port))
        if parts.scheme == "wss":
            context = ssl.create_default_context(cafile=cacert)
            context.set_alpn_protocols(["http/1.1"])
            sock = context.wrap_socket(sock, server_hostname=host)
        self.sock = sock
        key = base64.b64encode(os.urandom(16)).decode()
        sock.sendall((
            "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nX-Forwarded-For: %s\r\n\r\n"
            % (parts.path or "/", host, key, claimed)
        ).encode("ascii"))
        head = b""
        while b"\r\n\r\n" not in head:
            head += sock.recv(1)
        assert b" 101 " in head.split(b"\r\n", 1)[0], head

    def _exactly(self, n: int) -> bytes:
        out = b""
        while len(out) < n:
            chunk = self.sock.recv(n - len(out))
            if not chunk:
                raise EOFError
            out += chunk
        return out

    def send(self, payload: bytes) -> None:
        mask = os.urandom(4)
        n = len(payload)
        head = bytes([0x82]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n))
        self.sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def recv(self) -> bytes:
        first, second = self._exactly(2)
        n = second & 0x7F
        if n == 126:
            (n,) = struct.unpack(">H", self._exactly(2))
        elif n == 127:
            (n,) = struct.unpack(">Q", self._exactly(8))
        payload = self._exactly(n)
        if first & 0x0F == 0x9:  # a ping: answer it, and read on
            self.sock.sendall(bytes([0x8A, 0x80 | len(payload)]) + b"\0\0\0\0" + payload)
            return self.recv()
        return payload


async def slowreader(args) -> int:
    import time

    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "server"))
    from nereus_rendezvous import relaygrant

    context = ssl.create_default_context(cafile=args.cacert) if args.url.startswith("wss:") else None
    secret = open(args.secret_file, "rb").read().rstrip(b"\r\n")
    session = os.urandom(relaygrant.SESSION_BYTES)
    station = relaygrant.station_of(secret, "slowreaderslowreaderslowre")
    expires = int(time.time()) + 120
    loop = asyncio.get_running_loop()

    core = _SlowLeg(args.url, args.cacert, args.rcvbuf, "203.0.113.1")
    core.send(b"\x80" + relaygrant.mint(secret, relaygrant.LEG_CORE, session, station, expires).encode())
    assert core.recv()[0] == 0x81
    kwargs = {_HEADERS: {"X-Forwarded-For": "203.0.113.2"}, "max_size": None}
    if context is not None:
        kwargs["ssl"] = context
    device = await _connect(args.url, **kwargs)
    await device.send(b"\x80" + relaygrant.mint(secret, relaygrant.LEG_DEVICE, session, station, expires).encode())
    await asyncio.wait_for(device.recv(), 5)
    assert core.recv() == b"\x82\x01"
    total = int(args.seconds * args.send_rate)
    start = time.monotonic()

    async def send():
        for n in range(total):
            due = start + n / args.send_rate
            await asyncio.sleep(max(0.0, due - time.monotonic()))
            await device.send(b"\x02" + struct.pack(">Id", n, time.monotonic()) + bytes(987))

    def read():
        got = []
        core.sock.settimeout(2.0)
        next_read = time.monotonic()
        while time.monotonic() < start + args.seconds + 5:
            next_read += 1.0 / args.read_rate
            time.sleep(max(0.0, next_read - time.monotonic()))
            try:
                frame = core.recv()
            except (socket.timeout, EOFError, OSError):
                break
            now = time.monotonic()
            n, sent_at = struct.unpack(">Id", frame[1:13])
            got.append((now - start, n, now - sent_at))
        return got

    reader = loop.run_in_executor(None, read)
    await send()
    got = await reader
    early = [a for t, _, a in got if t < 5]
    late = [a for t, _, a in got if args.seconds - 5 <= t < args.seconds]
    seqs = [n for _, n, _ in got]
    print(json.dumps({
        "sent": total,
        "read": len(got),
        "inOrder": seqs == sorted(seqs),
        "droppedAmongWhatWasRead": (seqs[-1] - seqs[0] + 1 - len(seqs)) if seqs else None,
        "ageEarlyMaxS": round(max(early), 3) if early else None,
        "ageLateMaxS": round(max(late), 3) if late else None,
        "ageByFiveSeconds": [round(max([a for t, _, a in got if k <= t < k + 5] or [0]), 3)
                             for k in range(0, int(args.seconds), 5)],
    }), flush=True)
    await device.close()
    core.sock.close()
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="caddy_probe")
    sub = parser.add_subparsers(dest="command", required=True)
    o = sub.add_parser("open")
    o.add_argument("url")
    o.add_argument("--cacert", required=True)
    o.add_argument("--count", type=int, default=1)
    o.add_argument("--hold", type=float, default=0.0)
    r = sub.add_parser("raw")
    r.add_argument("host")
    r.add_argument("--cacert", required=True)
    r.add_argument("--upgrade", required=True)
    r.add_argument("--host-header", required=True)
    h = sub.add_parser("h2")
    h.add_argument("host")
    h.add_argument("--cacert", required=True)
    w = sub.add_parser("relay")
    w.add_argument("url")
    w.add_argument("--cacert", required=True)
    w.add_argument("--secret-file", required=True)
    w.add_argument("--extra", type=int, default=0)
    sr = sub.add_parser("slowreader")
    sr.add_argument("url")
    sr.add_argument("--cacert", required=True)
    sr.add_argument("--secret-file", required=True)
    sr.add_argument("--seconds", type=float, default=20)
    sr.add_argument("--send-rate", type=float, default=75)
    sr.add_argument("--read-rate", type=float, default=20)
    sr.add_argument("--rcvbuf", type=int, default=16384)
    args = parser.parse_args(argv)
    if args.command == "slowreader":
        return asyncio.run(slowreader(args))
    if args.command == "relay":
        return asyncio.run(relay(args))
    if args.command == "raw":
        return raw(args)
    if args.command == "h2":
        return h2(args)
    return asyncio.run(run(args))


if __name__ == "__main__":
    sys.exit(main())
