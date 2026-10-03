#!/usr/bin/env python3
# no-port-check: NereusSDR-original.
# =================================================================
# tests/tools/harness_front.py  (NereusSDR)
# =================================================================
#
# iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the traversal
# harness's stand-in for Caddy on the service's host. It terminates TLS on
# TCP 443 and routes by path, as the real server's Caddyfile does
# (rendezvous document section 12.7): /v1/relay* to the relay's Unix socket
# with the client's address appended to X-Forwarded-For, everything else to
# the rendezvous service. Every connection has TCP_NODELAY, as Caddy's (Go's
# default) have.
#
# On the relay's path it also reads the WebSocket frames going both ways
# (after TLS, before the relay) and counts, per data tag, the datagrams and
# those holding any of --marker's byte strings: the harness's proof that
# the relay never sees the session's plaintext (section 12.3's "what the
# relay reads of a datagram"). The counts go to --report as JSON, rewritten
# every second.
#
# Harness only: it never runs on a real server.
#
# =================================================================
# Modification history (NereusSDR):
#   2026-09-27: original implementation for NereusSDR by J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
# =================================================================

import argparse
import asyncio
import itertools
import json
import socket
import ssl

COUNTS = {"datagrams": {}, "markerHits": {}, "relayConnections": 0,
          "relaySockets": {}}
CONNECTION_IDS = itertools.count(1)


class FrameScanner:
    """Reads WebSocket frames from one direction of a stream and counts
    the relay's data messages and marker hits in their payloads."""

    def __init__(self, markers, connection, direction, response_header=False):
        self.buffer = bytearray()
        self.markers = markers
        self.connection = connection
        self.direction = direction
        self.response_header = response_header

    def feed(self, data):
        self.buffer.extend(data)
        if self.response_header:
            end = self.buffer.find(b"\r\n\r\n")
            if end < 0:
                return
            del self.buffer[:end + 4]
            self.response_header = False
        while True:
            if len(self.buffer) < 2:
                return
            opcode = self.buffer[0] & 0x0F
            masked = self.buffer[1] & 0x80
            length = self.buffer[1] & 0x7F
            offset = 2
            if length == 126:
                if len(self.buffer) < 4:
                    return
                length = int.from_bytes(self.buffer[2:4], "big")
                offset = 4
            elif length == 127:
                if len(self.buffer) < 10:
                    return
                length = int.from_bytes(self.buffer[2:10], "big")
                offset = 10
            mask = b""
            if masked:
                if len(self.buffer) < offset + 4:
                    return
                mask = bytes(self.buffer[offset:offset + 4])
                offset += 4
            if len(self.buffer) < offset + length:
                return
            payload = bytes(self.buffer[offset:offset + length])
            del self.buffer[:offset + length]
            if mask:
                payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
            if opcode == 0x2 and payload:
                tag = payload[0]
                if 1 <= tag <= 0x7F:
                    key = str(tag)
                    COUNTS["datagrams"][key] = COUNTS["datagrams"].get(key, 0) + 1
                    tags = COUNTS["relaySockets"][self.connection][self.direction]
                    tags[key] = tags.get(key, 0) + 1
                    for marker in self.markers:
                        if marker in payload[1:]:
                            COUNTS["markerHits"][key] = COUNTS["markerHits"].get(key, 0) + 1


async def pump(reader, writer, scanner=None):
    try:
        while True:
            data = await reader.read(65536)
            if not data:
                break
            if scanner is not None:
                scanner.feed(data)
            writer.write(data)
            await writer.drain()
    except (ConnectionError, asyncio.IncompleteReadError, OSError):
        pass
    finally:
        try:
            writer.close()
        except Exception:
            pass


def nodelay(writer):
    sock = writer.get_extra_info("socket")
    if sock is not None and sock.family in (socket.AF_INET, socket.AF_INET6):
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)


async def handle(reader, writer, args, markers):
    nodelay(writer)
    try:
        head = await reader.readuntil(b"\r\n\r\n")
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError, OSError):
        writer.close()
        return
    first = head.split(b"\r\n", 1)[0].split(b" ")
    path = first[1] if len(first) > 1 else b"/"
    peer = writer.get_extra_info("peername")
    address = peer[0] if peer else ""
    try:
        if path.startswith(b"/v1/relay"):
            COUNTS["relayConnections"] += 1
            connection = str(next(CONNECTION_IDS))
            COUNTS["relaySockets"][connection] = {
                "source": address, "toRelay": {}, "fromRelay": {}}
            up_reader, up_writer = await asyncio.open_unix_connection(args.relay_socket)
            head = head[:-2] + b"X-Forwarded-For: " + address.encode() + b"\r\n\r\n"
            up_writer.write(head)
            await asyncio.gather(
                pump(reader, up_writer, FrameScanner(markers, connection, "toRelay")),
                pump(up_reader, writer, FrameScanner(markers, connection, "fromRelay",
                                                   response_header=True)),
            )
        else:
            up_reader, up_writer = await asyncio.open_connection(args.service_host,
                                                                 args.service_port)
            nodelay(up_writer)
            up_writer.write(head)
            await asyncio.gather(pump(reader, up_writer), pump(up_reader, writer))
    except (ConnectionError, OSError):
        writer.close()


async def report(path):
    while True:
        await asyncio.sleep(1)
        with open(path + ".part", "w") as out:
            json.dump(COUNTS, out)
        import os
        os.replace(path + ".part", path)


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--listen", action="append", required=True, help="address to listen on")
    parser.add_argument("--port", type=int, default=443)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--relay-socket", required=True)
    parser.add_argument("--service-host", default="127.0.0.1")
    parser.add_argument("--service-port", type=int, default=8710)
    parser.add_argument("--marker", action="append", default=[], help="hex bytes to look for")
    parser.add_argument("--report", required=True)
    args = parser.parse_args()
    markers = [bytes.fromhex(m) for m in args.marker]
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(args.cert)
    servers = []
    for address in args.listen:
        servers.append(await asyncio.start_server(
            lambda r, w: handle(r, w, args, markers), address, args.port, ssl=context,
            limit=65536))
    asyncio.create_task(report(args.report))
    await asyncio.gather(*(server.serve_forever() for server in servers))


if __name__ == "__main__":
    asyncio.run(main())
