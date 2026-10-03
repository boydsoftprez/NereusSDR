# no-port-check: NereusSDR-original.
"""A small STUN and TURN client (RFC 8489, RFC 8656) for the coturn check.

rendezvous/tests/coturn-check.sh runs it inside the test containers, beside
coturn's own turnutils_uclient, for what uclient cannot show: the error code
of each refusal, a peer address in any form (IPv4-mapped, NAT64), and what
coturn does when an allocation is refreshed after its credential expired.
Standard library only, so it runs on Ubuntu's python3 with nothing added.
It is not a pytest module (no test_ prefix); the check calls it.

Each command prints one JSON object per line on standard output.

  binding HOST PORT
  allocate HOST PORT --secret-file F --id ID --ttl S [--family 4|6]
           [--peer IP:PORT ...] [--transport udp|tcp]
  expiry HOST PORT --secret-file F --id ID --ttl S --wait S [--wait S ...]
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import hmac
import ipaddress
import json
import os
import socket
import struct
import sys
import time
from typing import Dict, List, Optional, Tuple

MAGIC = 0x2112A442

BINDING, ALLOCATE, REFRESH, SEND, DATA, CREATE_PERMISSION = 0x001, 0x003, 0x004, 0x006, 0x007, 0x008
REQUEST, INDICATION, SUCCESS, ERROR = 0x000, 0x010, 0x100, 0x110

USERNAME = 0x0006
MESSAGE_INTEGRITY = 0x0008
ERROR_CODE = 0x0009
LIFETIME = 0x000D
XOR_PEER_ADDRESS = 0x0012
DATA_ATTR = 0x0013
REALM = 0x0014
NONCE = 0x0015
XOR_RELAYED_ADDRESS = 0x0016
REQUESTED_ADDRESS_FAMILY = 0x0017
REQUESTED_TRANSPORT = 0x0019
XOR_MAPPED_ADDRESS = 0x0020


def emit(**fields) -> None:
    print(json.dumps(fields, sort_keys=True), flush=True)


def message_type(method: int, klass: int) -> int:
    # The class bits sit at 0x0010 and 0x0100 among the method bits.
    return (method & 0x000F) | ((method & 0x0070) << 1) | ((method & 0x0F80) << 2) | klass


def split_type(value: int) -> Tuple[int, int]:
    klass = value & 0x0110
    method = (value & 0x000F) | ((value & 0x00E0) >> 1) | ((value & 0x3E00) >> 2)
    return method, klass


def attr(kind: int, value: bytes) -> bytes:
    pad = (4 - len(value) % 4) % 4
    return struct.pack("!HH", kind, len(value)) + value + b"\x00" * pad


def xor_address(ip: str, port: int, tid: bytes) -> bytes:
    address = ipaddress.ip_address(ip)
    xport = port ^ (MAGIC >> 16)
    if address.version == 4:
        raw = int(address) ^ MAGIC
        return struct.pack("!BBHI", 0, 1, xport, raw)
    mask = struct.pack("!I", MAGIC) + tid
    raw = bytes(a ^ b for a, b in zip(address.packed, mask))
    return struct.pack("!BBH", 0, 2, xport) + raw


def parse_xor_address(value: bytes, tid: bytes) -> str:
    family, xport = value[1], struct.unpack("!H", value[2:4])[0]
    port = xport ^ (MAGIC >> 16)
    if family == 1:
        raw = struct.unpack("!I", value[4:8])[0] ^ MAGIC
        return "%s:%d" % (ipaddress.IPv4Address(raw), port)
    mask = struct.pack("!I", MAGIC) + tid
    raw = bytes(a ^ b for a, b in zip(value[4:20], mask))
    return "[%s]:%d" % (ipaddress.IPv6Address(raw), port)


def build(method: int, klass: int, attrs: List[bytes], key: Optional[bytes] = None, tid: Optional[bytes] = None) -> Tuple[bytes, bytes]:
    tid = tid or os.urandom(12)
    body = b"".join(attrs)
    if key is not None:
        # MESSAGE-INTEGRITY covers the header with the length already
        # counting the integrity attribute itself (RFC 8489 section 14.5).
        header = struct.pack("!HHI", message_type(method, klass), len(body) + 24, MAGIC) + tid
        mac = hmac.new(key, header + body, hashlib.sha1).digest()
        body += attr(MESSAGE_INTEGRITY, mac)
    header = struct.pack("!HHI", message_type(method, klass), len(body), MAGIC) + tid
    return header + body, tid


def parse(packet: bytes) -> Optional[Dict[str, object]]:
    if len(packet) < 20:
        return None
    kind, length, magic = struct.unpack("!HHI", packet[:8])
    if magic != MAGIC or (kind & 0xC000):
        return None
    method, klass = split_type(kind)
    tid = packet[8:20]
    attrs: Dict[int, bytes] = {}
    offset = 20
    while offset + 4 <= 20 + length:
        a_kind, a_len = struct.unpack("!HH", packet[offset:offset + 4])
        attrs.setdefault(a_kind, packet[offset + 4:offset + 4 + a_len])
        offset += 4 + a_len + (4 - a_len % 4) % 4
    out: Dict[str, object] = {"method": method, "class": klass, "tid": tid, "attrs": attrs}
    if ERROR_CODE in attrs:
        value = attrs[ERROR_CODE]
        out["error"] = (value[2] & 0x7) * 100 + value[3]
        out["reason"] = value[4:].decode("utf-8", "replace")
    return out


class Client:
    def __init__(self, host: str, port: int, transport: str = "udp", timeout: float = 3.0) -> None:
        info = socket.getaddrinfo(host, port, 0, socket.SOCK_DGRAM if transport == "udp" else socket.SOCK_STREAM)[0]
        self.family = info[0]
        self.server = info[4]
        self.transport = transport
        self.sock = socket.socket(info[0], info[1])
        self.sock.settimeout(timeout)
        if transport == "tcp":
            self.sock.connect(self.server)
        self.username: Optional[str] = None
        self.realm: Optional[bytes] = None
        self.nonce: Optional[bytes] = None
        self.key: Optional[bytes] = None

    def _send(self, data: bytes) -> None:
        if self.transport == "udp":
            self.sock.sendto(data, self.server)
        else:
            self.sock.sendall(data)

    def _recv(self) -> Optional[bytes]:
        try:
            if self.transport == "udp":
                return self.sock.recvfrom(65535)[0]
            return self.sock.recv(65535)
        except socket.timeout:
            return None

    def transact(self, method: int, attrs, auth: bool = False, retries: int = 3) -> Optional[Dict[str, object]]:
        """A request and its answer, resent on silence (UDP), or None.
        `attrs` is a list, or a function of the transaction id (an IPv6
        XOR-PEER-ADDRESS depends on it)."""
        tid = os.urandom(12)
        full = list(attrs(tid) if callable(attrs) else attrs)
        key = None
        if auth and self.key is not None:
            full += [
                attr(USERNAME, self.username.encode()),
                attr(REALM, self.realm),
                attr(NONCE, self.nonce),
            ]
            key = self.key
        packet, _ = build(method, REQUEST, full, key, tid=tid)
        for _ in range(retries):
            self._send(packet)
            deadline = time.monotonic() + self.sock.gettimeout()
            while time.monotonic() < deadline:
                data = self._recv()
                if data is None:
                    break
                answer = parse(data)
                if answer is not None and answer["tid"] == tid:
                    return answer
        return None

    def authenticated(self, method: int, attrs) -> Optional[Dict[str, object]]:
        """A request with long-term credentials. A 401 or 438 answer
        carrying a nonce is answered once more with that nonce, as a TURN
        client does; what comes back then is the answer."""
        answer = self.transact(method, attrs, auth=self.key is not None)
        # The first answer's error, when the request had to be sent again.
        self.challenged: Optional[int] = None
        if answer is None or answer.get("error") not in (401, 438) or NONCE not in answer["attrs"]:
            return answer
        self.challenged = answer.get("error") if self.key is not None else None
        got = answer["attrs"]
        self.realm = got.get(REALM, self.realm)
        self.nonce = got[NONCE]
        self.key = hashlib.md5(b"%s:%s:%s" % (self.username.encode(), self.realm, self._password.encode())).digest()
        return self.transact(method, attrs, auth=True)

    def login(self, username: str, password: str) -> None:
        self.username = username
        self._password = password

    def send_indication(self, peer: str, port: int, data: bytes) -> None:
        tid = os.urandom(12)
        packet, _ = build(SEND, INDICATION, [attr(XOR_PEER_ADDRESS, xor_address(peer, port, tid)), attr(DATA_ATTR, data)], tid=tid)
        self._send(packet)

    def wait_data(self, seconds: float) -> Optional[bytes]:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            data = self._recv()
            if data is None:
                continue
            answer = parse(data)
            if answer and answer["method"] == DATA and answer["class"] == INDICATION:
                return answer["attrs"].get(DATA_ATTR)
        return None


def credentials(secret_file: str, station_id: str, ttl: int) -> Tuple[str, str, int]:
    # coturn's time-limited form (rendezvous document section 8), computed
    # here independently of the service's code.
    with open(secret_file, "rb") as handle:
        secret = handle.read().rstrip(b"\r\n")
    expires = int(time.time()) + ttl
    username = "%d:%s" % (expires, station_id)
    password = base64.b64encode(hmac.new(secret, username.encode(), hashlib.sha1).digest()).decode()
    return username, password, expires


def family_attrs(family: Optional[str]) -> List[bytes]:
    if family == "6":
        return [attr(REQUESTED_ADDRESS_FAMILY, b"\x02\x00\x00\x00")]
    if family == "4":
        return [attr(REQUESTED_ADDRESS_FAMILY, b"\x01\x00\x00\x00")]
    return []


def outcome(answer: Optional[Dict[str, object]]) -> Dict[str, object]:
    if answer is None:
        return {"ok": False, "error": None, "reason": "no answer"}
    if answer["class"] == SUCCESS:
        return {"ok": True}
    return {"ok": False, "error": answer.get("error"), "reason": answer.get("reason")}


def do_binding(args) -> int:
    client = Client(args.host, args.port)
    answer = client.transact(BINDING, [])
    if answer is None or answer["class"] != SUCCESS or XOR_MAPPED_ADDRESS not in answer["attrs"]:
        emit(step="binding", ok=False)
        return 1
    emit(step="binding", ok=True, mapped=parse_xor_address(answer["attrs"][XOR_MAPPED_ADDRESS], answer["tid"]))
    return 0


def allocate(client: Client, args) -> Optional[Dict[str, object]]:
    transport = 6 if args.relay == "tcp" else 17
    attrs = [attr(REQUESTED_TRANSPORT, bytes([transport, 0, 0, 0])), attr(LIFETIME, struct.pack("!I", 600))]
    return client.authenticated(ALLOCATE, attrs + family_attrs(args.family))


def do_allocate(args) -> int:
    client = Client(args.host, args.port, args.transport)
    if args.anonymous:
        pass
    elif args.password is not None:
        client.login(args.username, args.password)
    else:
        username, password, _ = credentials(args.secret_file, args.id, args.ttl)
        client.login(username, password)
    answer = allocate(client, args) if not args.anonymous else client.transact(
        ALLOCATE, [attr(REQUESTED_TRANSPORT, b"\x11\x00\x00\x00")]
    )
    result = outcome(answer)
    if result["ok"]:
        result["relayed"] = parse_xor_address(answer["attrs"][XOR_RELAYED_ADDRESS], answer["tid"])
    emit(step="allocate", **result)
    if not result["ok"]:
        return 1
    status = 0
    for peer in args.peer:
        host, _, port = peer.rpartition(":")
        host = host.strip("[]")
        permission = client.authenticated(
            CREATE_PERMISSION, lambda tid, h=host, p=int(port): [attr(XOR_PEER_ADDRESS, xor_address(h, p, tid))]
        )
        granted = outcome(permission)
        echoed = None
        if granted["ok"]:
            payload = os.urandom(16).hex().encode()
            client.send_indication(host, int(port), payload)
            echoed = client.wait_data(2.0) == payload
        emit(step="peer", peer=peer, permission=granted, relayed=echoed)
    # Release the allocation (a refresh with lifetime 0), so the quota is
    # free again for the next step of the check.
    release = client.authenticated(REFRESH, [attr(LIFETIME, struct.pack("!I", 0))])
    emit(step="release", **outcome(release))
    return status


def do_expiry(args) -> int:
    """Allocate with a credential that expires in --ttl seconds, then, after
    each --wait (seconds from the start), refresh the allocation with the
    same credential and record coturn's answer."""
    client = Client(args.host, args.port)
    username, password, expires = credentials(args.secret_file, args.id, args.ttl)
    client.login(username, password)
    start = time.time()
    first = outcome(allocate(client, args))
    emit(step="allocate", at=0, credentialExpiresIn=expires - int(start), **first)
    if not first["ok"]:
        return 1
    for wait in args.wait:
        delay = start + wait - time.time()
        if delay > 0:
            time.sleep(delay)
        answer = client.authenticated(REFRESH, [attr(LIFETIME, struct.pack("!I", 600))])
        emit(
            step="refresh",
            at=round(time.time() - start, 1),
            credentialExpired=time.time() > expires,
            firstAnswer=client.challenged,
            **outcome(answer),
        )
        for peer in args.peer:
            host, _, port = peer.rpartition(":")
            host = host.strip("[]")
            permission = client.authenticated(
                CREATE_PERMISSION, lambda tid, h=host, p=int(port): [attr(XOR_PEER_ADDRESS, xor_address(h, p, tid))]
            )
            granted = outcome(permission)
            echoed = None
            if granted["ok"]:
                payload = os.urandom(16).hex().encode()
                client.send_indication(host, int(port), payload)
                echoed = client.wait_data(2.0) == payload
            emit(step="peer-after-refresh", peer=peer, permission=granted, relayed=echoed, firstAnswer=client.challenged)
    # A fresh allocation with the expired credential, for comparison.
    late = Client(args.host, args.port)
    late.login(username, password)
    emit(step="new-allocation-with-expired-credential", **outcome(allocate(late, args)))
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="turn_probe")
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("binding")
    b.add_argument("host")
    b.add_argument("port", type=int)
    for name in ("allocate", "expiry"):
        p = sub.add_parser(name)
        p.add_argument("host")
        p.add_argument("port", type=int)
        p.add_argument("--secret-file")
        p.add_argument("--id", default="probeprobeprobeprobeprobe2")
        p.add_argument("--ttl", type=int, default=3600)
        p.add_argument("--family", choices=["4", "6"])
        p.add_argument("--relay", choices=["udp", "tcp"], default="udp")
        p.add_argument("--peer", action="append", default=[])
        if name == "allocate":
            p.add_argument("--transport", choices=["udp", "tcp"], default="udp")
            p.add_argument("--username")
            p.add_argument("--password")
            p.add_argument("--anonymous", action="store_true")
        else:
            p.add_argument("--wait", type=float, action="append", default=[])
    args = parser.parse_args(argv)
    if args.command == "binding":
        return do_binding(args)
    if args.command == "allocate":
        return do_allocate(args)
    return do_expiry(args)


if __name__ == "__main__":
    sys.exit(main())
