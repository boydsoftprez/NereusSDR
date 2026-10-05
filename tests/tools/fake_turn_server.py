#!/usr/bin/env python3
# no-port-check: NereusSDR-original.
"""A small STUN and TURN (UDP) server for tests on this computer only.

iPhone app plan Task 27 (R-IOS-16). tst_rendezvous_client runs it beside the
rendezvous service so a connection the service introduces can gather
server-reflexive and relayed candidates without anything leaving this
computer. It answers:

  - STUN Binding (RFC 8489) with XOR-MAPPED-ADDRESS, no credentials;
  - TURN over UDP (RFC 8656): Allocate, Refresh, CreatePermission,
    ChannelBind, Send indications and ChannelData, with coturn's
    time-limited credentials (use-auth-secret): the password of a username
    is standard base64 of HMAC-SHA1(secret, username), the rendezvous
    document's section 8, so the credentials the service mints work here.

It is a test fake, not a relay: one process, no quotas, no expiry, loopback
only. The real relay is coturn (Task 26); the traversal harness
(tests/scripts/traversal-harness.sh) runs that.

With --quota-full every allocation is refused with 486 (Allocation Quota
Reached), as a full relay refuses one (the relay is sized by allocations,
eight for each Core's id), so a test can show a full relay is not fatal.

Usage: fake_turn_server.py --secret-file FILE --port-file FILE [--listen ADDR]
                           [--quota-full] [--parent-pid PID]
It writes the UDP port it bound to --port-file, then prints one line to
standard output for each allocation (ALLOCATED n, currently live) and, for datagrams it
relays, at 1, 2, 4, 8 ... of them each way (RELAYED OUT n from a client to
a peer, RELAYED IN n from a peer to a client), so a test can see the relay
was used, and RELEASED n for each allocation a client gives back with a
Refresh of LIFETIME 0 (iPhone app plan Task 28). TURN lines include an
opaque allocation ID and cumulative created, released and live counts. It
never prints a credential.

With --parent-pid (the test process that started it) it exits within half a
second of that process ending, however it ended: a test killed at its ctest
timeout, or one that crashed, runs no destructor to stop its helper, and
before this the helper was left running with parent 1 for as long as the
computer stayed up (tst_path_racer's, found 22 hours on). POSIX only; on
Windows the option is accepted and does nothing.

A datagram it cannot send (a peer address the loopback-bound relay socket
cannot reach, such as another interface's address routed off this computer:
EADDRNOTAVAIL on macOS) is dropped, as a relay drops what it cannot deliver,
and counted with UNSENT n at 1, 2, 4, 8 ... of them. Before 2026-09-30 the
error ended the whole process, so every later release went unanswered.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import hashlib
import hmac
import ipaddress
import os
import selectors
import socket
import struct
import sys

MAGIC = 0x2112A442
REALM = b"nereussdr-test"

# Methods and classes (RFC 8489 section 5, RFC 8656 section 16).
BINDING, ALLOCATE, REFRESH, SEND, DATA, CREATE_PERMISSION, CHANNEL_BIND = (
    0x001, 0x003, 0x004, 0x006, 0x007, 0x008, 0x009)
REQUEST, INDICATION, SUCCESS, ERROR = 0x000, 0x010, 0x100, 0x110

# Attributes.
USERNAME, MESSAGE_INTEGRITY, ERROR_CODE = 0x0006, 0x0008, 0x0009
CHANNEL_NUMBER, LIFETIME, XOR_PEER_ADDRESS, DATA_ATTR = 0x000C, 0x000D, 0x0012, 0x0013
REALM_ATTR, NONCE, XOR_RELAYED_ADDRESS, REQUESTED_TRANSPORT = 0x0014, 0x0015, 0x0016, 0x0019
XOR_MAPPED_ADDRESS, FINGERPRINT = 0x0020, 0x8028


def message_type(method: int, cls: int) -> int:
    return ((method & 0x0F80) << 2) | ((method & 0x0070) << 1) | (method & 0x000F) | cls


def split_type(value: int):
    method = (value & 0x000F) | ((value & 0x00E0) >> 1) | ((value & 0x3E00) >> 2)
    return method, value & 0x0110


def parse(data: bytes):
    if len(data) < 20 or data[0] & 0xC0:
        return None
    mtype, length, magic = struct.unpack("!HHI", data[:8])
    if magic != MAGIC or 20 + length > len(data):
        return None
    txid = data[8:20]
    attrs = []
    offset = 20
    end = 20 + length
    while offset + 4 <= end:
        atype, alen = struct.unpack("!HH", data[offset:offset + 4])
        value = data[offset + 4:offset + 4 + alen]
        attrs.append((atype, value, offset))
        offset += 4 + alen + ((4 - alen % 4) % 4)
    method, cls = split_type(mtype)
    return method, cls, txid, attrs, data[:end]


def attr(attrs, wanted):
    for atype, value, _ in attrs:
        if atype == wanted:
            return value
    return None


def xor_address(addr, txid: bytes) -> bytes:
    host, port = addr[0], addr[1]
    ip = ipaddress.ip_address(host)
    xport = port ^ (MAGIC >> 16)
    if ip.version == 4:
        xaddr = int(ip) ^ MAGIC
        return struct.pack("!BBHI", 0, 1, xport, xaddr)
    key = struct.pack("!I", MAGIC) + txid
    raw = bytes(a ^ b for a, b in zip(ip.packed, key))
    return struct.pack("!BBH", 0, 2, xport) + raw


def read_xor_address(value: bytes, txid: bytes):
    family = value[1]
    port = struct.unpack("!H", value[2:4])[0] ^ (MAGIC >> 16)
    if family == 1:
        raw = struct.unpack("!I", value[4:8])[0] ^ MAGIC
        return str(ipaddress.IPv4Address(raw)), port
    key = struct.pack("!I", MAGIC) + txid
    raw = bytes(a ^ b for a, b in zip(value[4:20], key))
    return str(ipaddress.IPv6Address(raw)), port


def build(method, cls, txid, attrs, key: bytes | None):
    body = b""
    for atype, value in attrs:
        body += struct.pack("!HH", atype, len(value)) + value + b"\0" * ((4 - len(value) % 4) % 4)
    if key is not None:
        header = struct.pack("!HHI", message_type(method, cls), len(body) + 24, MAGIC) + txid
        mac = hmac.new(key, header + body, hashlib.sha1).digest()
        body += struct.pack("!HH", MESSAGE_INTEGRITY, 20) + mac
    header = struct.pack("!HHI", message_type(method, cls), len(body) + 8, MAGIC) + txid
    crc = (binascii.crc32(header + body) & 0xFFFFFFFF) ^ 0x5354554E
    body += struct.pack("!HHI", FINGERPRINT, 4, crc)
    return struct.pack("!HHI", message_type(method, cls), len(body), MAGIC) + txid + body


def integrity_ok(raw: bytes, attrs, key: bytes) -> bool:
    for atype, value, offset in attrs:
        if atype == MESSAGE_INTEGRITY:
            header = bytearray(raw[:20])
            struct.pack_into("!H", header, 2, offset + 24 - 20)
            mac = hmac.new(key, bytes(header) + raw[20:offset], hashlib.sha1).digest()
            return hmac.compare_digest(mac, value)
    return False


class Allocation:
    def __init__(self, client, relay: socket.socket, key: bytes, allocation_id: int):
        self.client = client
        self.relay = relay
        self.key = key
        self.id = allocation_id
        self.permissions = set()
        self.channels = {}   # number -> peer address
        self.peers = {}      # peer address -> number


class Server:
    def __init__(self, listen: str, secret: bytes, quota_full: bool = False,
                 drop_first_release_request: bool = False,
                 drop_first_release_response: bool = False,
                 drop_all_release_requests: bool = False,
                 hold_allocate_success_file: str | None = None,
                 release_fault: str | None = None,
                 hold_allocate_request_file: str | None = None):
        self.secret = secret
        self.quota_full = quota_full
        self.drop_first_release_request = drop_first_release_request
        self.drop_first_release_response = drop_first_release_response
        self.drop_all_release_requests = drop_all_release_requests
        self.hold_allocate_success_file = hold_allocate_success_file
        self.held_allocate_success = None
        self.held_allocate_once = False
        self.hold_allocate_request_file = hold_allocate_request_file
        self.held_allocate_request = None
        self.held_request_once = False
        self.release_fault = release_fault
        self.nonce = base64.b16encode(os.urandom(8)).lower()
        family = socket.AF_INET6 if ":" in listen else socket.AF_INET
        self.family = family
        self.listen = listen
        self.sock = socket.socket(family, socket.SOCK_DGRAM)
        self.sock.bind((listen, 0))
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.sock, selectors.EVENT_READ, None)
        self.allocations = {}
        self.created = 0
        self.relayed = 0
        self.relayed_out = 0
        # iPhone app plan Task 28: allocations given back with LIFETIME 0.
        self.released = 0
        self.release_requests = {}
        self.allocation_ids = {}
        # Datagrams dropped because the socket refused to send them.
        self.unsent = 0

    def port(self) -> int:
        return self.sock.getsockname()[1]

    def key_for(self, username: bytes) -> bytes:
        password = base64.b64encode(hmac.new(self.secret, username, hashlib.sha1).digest())
        return hashlib.md5(username + b":" + REALM + b":" + password).digest()

    def say(self, text: str) -> None:
        print(text, flush=True)

    def count(self, direction: str, total: int) -> None:
        # One line at 1, 2, 4, 8 ... datagrams each way, so a test sees the
        # relay carried traffic both ways without a line per datagram.
        if total & (total - 1) == 0:
            self.say(f"RELAYED {direction} {total}")

    def send(self, sock: socket.socket, data: bytes, addr) -> bool:
        # A send the operating system refuses drops that one datagram; the
        # server, and every allocation it holds, carries on.
        try:
            sock.sendto(data, addr)
            return True
        except OSError:
            self.unsent += 1
            if self.unsent & (self.unsent - 1) == 0:
                self.say(f"UNSENT {self.unsent}")
            return False

    def to_peer(self, allocation: Allocation, data: bytes, peer) -> None:
        if not self.send(allocation.relay, data, peer):
            return
        self.relayed_out += 1
        self.count("OUT", self.relayed_out)

    def run(self, parent_pid: int | None = None) -> None:
        while True:
            if parent_pid is not None and not process_alive(parent_pid):
                return
            held = self.held_allocate_success or self.held_allocate_request
            if held:
                timeout = 0.1
            elif parent_pid is not None:
                timeout = PARENT_POLL_S
            else:
                timeout = None
            for key, _ in self.selector.select(timeout):
                sock = key.fileobj
                # A release on the control socket can unregister and close a
                # relay socket later in this same ready batch. Selector keys
                # are snapshots, so check current ownership before recvfrom.
                try:
                    current = self.selector.get_key(sock)
                except (KeyError, ValueError):
                    continue
                if current != key:
                    continue
                if key.data is not None:
                    allocation = key.data
                    if (self.allocations.get(allocation.client) is not allocation
                            or allocation.relay is not sock):
                        continue
                try:
                    data, addr = sock.recvfrom(65535)
                except OSError:
                    continue
                if key.data is None:
                    self.from_client(data, addr[:2])
                else:
                    self.from_peer(key.data, data, addr[:2])
            if (self.held_allocate_request and self.hold_allocate_request_file
                    and os.path.exists(self.hold_allocate_request_file)):
                txid, client, key = self.held_allocate_request
                self.held_allocate_request = None
                self.allocate(txid, client, key)
                self.say("SENT_HELD_ALLOCATE_REQUEST")
            if (self.held_allocate_success and self.hold_allocate_success_file
                    and os.path.exists(self.hold_allocate_success_file)):
                client, packet, allocation_id = self.held_allocate_success
                self.held_allocate_success = None
                self.send(self.sock, packet, client)
                self.say(f"SENT_HELD_ALLOCATE id={allocation_id}")

    def from_peer(self, allocation: Allocation, data: bytes, peer) -> None:
        if peer[0] not in allocation.permissions:
            return
        self.relayed += 1
        self.count("IN", self.relayed)
        number = allocation.peers.get(peer)
        if number is not None:
            self.send(self.sock, struct.pack("!HH", number, len(data)) + data, allocation.client)
            return
        txid = os.urandom(12)
        message = build(DATA, INDICATION, txid,
                        [(XOR_PEER_ADDRESS, xor_address(peer, txid)), (DATA_ATTR, data)], None)
        self.send(self.sock, message, allocation.client)

    def from_client(self, data: bytes, client) -> None:
        if data and 0x40 <= data[0] <= 0x7F:
            allocation = self.allocations.get(client)
            if allocation is None or len(data) < 4:
                return
            number, length = struct.unpack("!HH", data[:4])
            peer = allocation.channels.get(number)
            if peer is not None:
                self.to_peer(allocation, data[4:4 + length], peer)
            return
        parsed = parse(data)
        if parsed is None:
            return
        method, cls, txid, attrs, raw = parsed
        if method == BINDING and cls == REQUEST:
            self.send(self.sock, build(BINDING, SUCCESS, txid,
                                   [(XOR_MAPPED_ADDRESS, xor_address(client, txid))], None),
                             client)
            return
        if method == SEND and cls == INDICATION:
            allocation = self.allocations.get(client)
            peer_value, payload = attr(attrs, XOR_PEER_ADDRESS), attr(attrs, DATA_ATTR)
            if allocation and peer_value and payload is not None:
                peer = read_xor_address(peer_value, txid)
                if peer[0] in allocation.permissions:
                    self.to_peer(allocation, payload, peer)
            return
        if cls != REQUEST or method not in (ALLOCATE, REFRESH, CREATE_PERMISSION, CHANNEL_BIND):
            return
        username = attr(attrs, USERNAME)
        if username is None or attr(attrs, MESSAGE_INTEGRITY) is None:
            self.challenge(method, txid, client)
            return
        key = self.key_for(username)
        if not integrity_ok(raw, attrs, key) or attr(attrs, NONCE) != self.nonce:
            self.challenge(method, txid, client)
            return
        if method == ALLOCATE:
            if self.hold_allocate_request_file and not self.held_request_once:
                self.held_request_once = True
                self.held_allocate_request = (txid, client, key)
                self.say("HELD_ALLOCATE_REQUEST")
            else:
                self.allocate(txid, client, key)
        elif method == REFRESH:
            self.refresh(txid, client, attrs, key)
        elif method == CREATE_PERMISSION:
            self.permit(txid, client, attrs, key)
        else:
            self.bind_channel(txid, client, attrs, key)

    def challenge(self, method, txid, client) -> None:
        error = struct.pack("!HBB", 0, 4, 1) + b"Unauthorized"
        self.send(self.sock, build(method, ERROR, txid,
                               [(ERROR_CODE, error), (REALM_ATTR, REALM), (NONCE, self.nonce)], None),
                         client)

    def allocate(self, txid, client, key) -> None:
        if self.quota_full:
            error = struct.pack("!HBB", 0, 4, 86) + b"Allocation Quota Reached"
            self.send(self.sock, build(ALLOCATE, ERROR, txid, [(ERROR_CODE, error)], key), client)
            self.say("QUOTA 486")
            return
        allocation = self.allocations.get(client)
        if allocation is None:
            relay = socket.socket(self.family, socket.SOCK_DGRAM)
            relay.bind((self.listen, 0))
            self.created += 1
            allocation = Allocation(client, relay, key, self.created)
            self.allocations[client] = allocation
            self.allocation_ids[client] = allocation.id
            self.selector.register(relay, selectors.EVENT_READ, allocation)
            self.say(f"ALLOCATED {len(self.allocations)}")
            self.say(f"TURN id={allocation.id} created={self.created} released={self.released} live={len(self.allocations)}")
        relayed = allocation.relay.getsockname()[:2]
        response = build(ALLOCATE, SUCCESS, txid, [
            (XOR_RELAYED_ADDRESS, xor_address(relayed, txid)),
            (XOR_MAPPED_ADDRESS, xor_address(client, txid)),
            (LIFETIME, struct.pack("!I", 600)),
        ], key)
        if self.hold_allocate_success_file and not self.held_allocate_once:
            self.held_allocate_once = True
            self.held_allocate_success = (client, response, allocation.id)
            self.say(f"HELD_ALLOCATE id={allocation.id}")
        else:
            self.send(self.sock, response, client)

    def refresh(self, txid, client, attrs, key) -> None:
        lifetime = attr(attrs, LIFETIME)
        seconds = struct.unpack("!I", lifetime)[0] if lifetime else 600
        if seconds == 0:
            allocation_id = self.allocation_ids.get(client, 0)
            release_key = (client, allocation_id)
            number = self.release_requests.get(release_key, 0) + 1
            self.release_requests[release_key] = number
            self.say(f"RELEASE_REQUEST id={allocation_id} "
                     f"number={number} tx={txid.hex()}")
            if self.drop_all_release_requests or (
                    self.drop_first_release_request and number == 1):
                self.say(f"DROP_RELEASE_REQUEST id={self.allocation_ids.get(client, 0)}")
                return
            if self.release_fault in ("stale-nonce", "repeat-stale-nonce") and (
                    number == 1 or self.release_fault == "repeat-stale-nonce" and number == 2):
                if number == 1:
                    self.nonce = base64.b16encode(os.urandom(8)).lower()
                # On the second challenge send a signed but unusable nonce.
                # A capped client keeps the previous valid nonce and retries
                # its current transaction; one that accepts a second challenge
                # cannot release this allocation.
                offered_nonce = (base64.b16encode(os.urandom(8)).lower()
                                 if number == 2 else self.nonce)
                error = struct.pack("!HBB", 0, 4, 38) + b"Stale Nonce"
                self.send(self.sock, build(REFRESH, ERROR, txid, [
                    (ERROR_CODE, error), (REALM_ATTR, REALM), (NONCE, offered_nonce),
                ], key), client)
                self.say(f"RELEASE_FAULT {self.release_fault} id={allocation_id} number={number}")
                return
            if number == 1 and self.release_fault in (
                    "wrong-id", "wrong-source", "bad-integrity", "unsigned-absent",
                    "wrong-method", "missing-lifetime", "nonzero-lifetime"):
                if self.release_fault == "unsigned-absent":
                    error = struct.pack("!HBB", 0, 4, 37) + b"Allocation Mismatch"
                    wrong = build(REFRESH, ERROR, txid, [(ERROR_CODE, error)], None)
                elif self.release_fault == "wrong-id":
                    wrong = build(REFRESH, SUCCESS, bytes([txid[0] ^ 1]) + txid[1:],
                                  [(LIFETIME, struct.pack("!I", 0))], key)
                elif self.release_fault == "wrong-method":
                    wrong = build(ALLOCATE, SUCCESS, txid,
                                  [(LIFETIME, struct.pack("!I", 0))], key)
                elif self.release_fault == "missing-lifetime":
                    wrong = build(REFRESH, SUCCESS, txid, [], key)
                elif self.release_fault == "nonzero-lifetime":
                    wrong = build(REFRESH, SUCCESS, txid,
                                  [(LIFETIME, struct.pack("!I", 600))], key)
                else:
                    wrong_key = b"wrong" if self.release_fault == "bad-integrity" else key
                    wrong = build(REFRESH, SUCCESS, txid,
                                  [(LIFETIME, struct.pack("!I", 0))], wrong_key)
                if self.release_fault == "wrong-source":
                    spoof = socket.socket(self.family, socket.SOCK_DGRAM)
                    try:
                        spoof.bind((self.listen, 0))
                        spoof.sendto(wrong, client)
                    finally:
                        spoof.close()
                else:
                    self.send(self.sock, wrong, client)
                self.say(f"RELEASE_FAULT {self.release_fault} id={allocation_id}")
                # Do not send a valid acknowledgement until a retry.
                return
        if seconds == 0 and client in self.allocations:
            allocation = self.allocations.pop(client)
            self.selector.unregister(allocation.relay)
            allocation.relay.close()
            self.released += 1
            self.say(f"RELEASED {self.released}")
            self.say(f"TURN id={allocation.id} created={self.created} released={self.released} live={len(self.allocations)}")
        if seconds == 0 and self.drop_first_release_response and number == 1:
            self.say(f"DROP_RELEASE_RESPONSE id={self.allocation_ids.get(client, 0)}")
            return
        self.send(self.sock, build(REFRESH, SUCCESS, txid,
                               [(LIFETIME, struct.pack("!I", min(seconds, 600)))], key), client)

    def permit(self, txid, client, attrs, key) -> None:
        allocation = self.allocations.get(client)
        if allocation is None:
            return
        for atype, value, _ in attrs:
            if atype == XOR_PEER_ADDRESS:
                allocation.permissions.add(read_xor_address(value, txid)[0])
        self.send(self.sock, build(CREATE_PERMISSION, SUCCESS, txid, [], key), client)

    def bind_channel(self, txid, client, attrs, key) -> None:
        allocation = self.allocations.get(client)
        number_value, peer_value = attr(attrs, CHANNEL_NUMBER), attr(attrs, XOR_PEER_ADDRESS)
        if allocation is None or number_value is None or peer_value is None:
            return
        number = struct.unpack("!H", number_value[:2])[0]
        peer = read_xor_address(peer_value, txid)
        allocation.channels[number] = peer
        allocation.peers[peer] = number
        allocation.permissions.add(peer[0])
        self.send(self.sock, build(CHANNEL_BIND, SUCCESS, txid, [], key), client)


# How often the server looks for its parent while nothing arrives.
PARENT_POLL_S = 0.5


def process_alive(pid: int) -> bool:
    """True while process `pid` exists (POSIX; always True on Windows, where
    os.kill with signal 0 would end the process instead of probing it)."""
    if os.name != "posix":
        return True
    if os.getppid() == 1:
        return False  # re-parented to init: the parent is gone
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--secret-file", required=True)
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--listen", default="127.0.0.1")
    parser.add_argument("--quota-full", action="store_true")
    parser.add_argument("--drop-first-release-request", action="store_true")
    parser.add_argument("--drop-first-release-response", action="store_true")
    parser.add_argument("--drop-all-release-requests", action="store_true")
    parser.add_argument("--hold-allocate-success-file")
    parser.add_argument("--hold-allocate-request-file")
    parser.add_argument("--release-fault", choices=[
        "stale-nonce", "repeat-stale-nonce", "wrong-id", "wrong-source",
        "bad-integrity", "unsigned-absent", "wrong-method", "missing-lifetime",
        "nonzero-lifetime"])
    parser.add_argument("--parent-pid", type=int, default=None)
    args = parser.parse_args()
    with open(args.secret_file, "rb") as handle:
        secret = handle.read().rstrip(b"\r\n")
    server = Server(args.listen, secret, args.quota_full,
                    args.drop_first_release_request, args.drop_first_release_response,
                    args.drop_all_release_requests, args.hold_allocate_success_file,
                    args.release_fault, args.hold_allocate_request_file)
    with open(args.port_file + ".part", "w", encoding="ascii") as handle:
        handle.write(str(server.port()))
    os.replace(args.port_file + ".part", args.port_file)
    try:
        server.run(args.parent_pid)
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
