# no-port-check: NereusSDR-original.
"""Relay grants: the tokens that admit a leg to the WebSocket relay.

Rendezvous document section 12.2 is the authority. The rendezvous service
mints one grant per introduction a station accepts with the relay allowed,
one token for each end; the relay (nereus_relay) checks a token with the
same secret and nothing else, so the two processes share no state.

    station = HMAC-SHA256(secret, "NereusSDR relay station v1\\n" || station
              id)[0..8]
    payload = version (1 byte, 1) || leg (1 byte: 1 the Core, 2 the device)
              || session (16 random bytes) || station (8 bytes)
              || expires (4 bytes, big-endian, Unix seconds)
    mac     = HMAC-SHA256(secret, "NereusSDR relay grant v1\\n" || payload)
    token   = base64url(payload || mac), no padding: 62 bytes, 83 characters

`station` is opaque: the relay counts live sessions per station with it
(section 12.5) without learning the station's id.

Only hashlib, hmac and base64 are used, so the relay imports this module
without the cryptography package.

Watch relay socket grants use a separate v2 grammar and MAC domain. The
existing verify() accepts primary v1 grants only; future watch admission
must explicitly call verify_watch(). Neither grant authenticates a Core
session or authorizes Core watchdog and key operations.
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import struct
from typing import NamedTuple, Optional

VERSION = 1
WATCH_VERSION = 2
PURPOSE_PRIMARY = 1
PURPOSE_WATCH = 2
LEG_CORE = 1
LEG_DEVICE = 2
LEGS = (LEG_CORE, LEG_DEVICE)
SESSION_BYTES = 16
STATION_BYTES = 8
PAYLOAD_BYTES = 2 + SESSION_BYTES + STATION_BYTES + 4
MAC_BYTES = 32
TOKEN_BYTES = PAYLOAD_BYTES + MAC_BYTES
TOKEN_CHARS = 83
PREFIX = b"NereusSDR relay grant v1\n"
WATCH_PAYLOAD_BYTES = PAYLOAD_BYTES + 1
WATCH_TOKEN_BYTES = WATCH_PAYLOAD_BYTES + MAC_BYTES
WATCH_TOKEN_CHARS = 84
WATCH_PREFIX = b"NereusSDR relay grant v2\n"
STATION_PREFIX = b"NereusSDR relay station v1\n"
EXPIRES_MAX = 4294967295

_ALPHABET = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")


class Grant(NamedTuple):
    leg: int
    session: bytes
    station: bytes
    expires: int
    purpose: int = PURPOSE_PRIMARY
    version: int = VERSION


def to_b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def from_b64url(text: str) -> Optional[bytes]:
    """Strict base64url: no padding, only the alphabet, and the text must be
    exactly what its bytes encode to (so unused low bits are zero)."""
    if not isinstance(text, str) or not text or len(text) % 4 == 1:
        return None
    if any(c not in _ALPHABET for c in text):
        return None
    try:
        data = base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))
    except (ValueError, TypeError):
        return None
    if to_b64url(data) != text:
        return None
    return data


def station_of(secret: bytes, station_id: str) -> bytes:
    """The opaque per-station value a grant carries."""
    return hmac.new(bytes(secret), STATION_PREFIX + station_id.encode("ascii"), hashlib.sha256).digest()[:STATION_BYTES]


def payload_of(leg: int, session: bytes, station: bytes, expires: int) -> bytes:
    if (
        leg not in LEGS
        or len(session) != SESSION_BYTES
        or len(station) != STATION_BYTES
        or not 0 <= expires <= EXPIRES_MAX
    ):
        raise ValueError("not a relay grant")
    return bytes([VERSION, leg]) + bytes(session) + bytes(station) + struct.pack(">I", expires)


def mac_of(secret: bytes, payload: bytes) -> bytes:
    return hmac.new(bytes(secret), PREFIX + payload, hashlib.sha256).digest()


def mint(secret: bytes, leg: int, session: bytes, station: bytes, expires: int) -> str:
    payload = payload_of(leg, session, station, expires)
    return to_b64url(payload + mac_of(secret, payload))


def mint_watch(secret: bytes, leg: int, session: bytes, station: bytes, expires: int) -> str:
    """Mint a v2 watch relay socket grant, never a Core/session credential."""
    primary_payload = payload_of(leg, session, station, expires)
    payload = bytes([WATCH_VERSION, leg, PURPOSE_WATCH]) + primary_payload[2:]
    mac = hmac.new(bytes(secret), WATCH_PREFIX + payload, hashlib.sha256).digest()
    return to_b64url(payload + mac)


def verify(secret: bytes, token: str) -> Optional[Grant]:
    """A primary v1 grant only, or None when it is not one this secret
    minted: bad base64url, another length, another version or leg, or a MAC
    that does not verify (compared in constant time)."""
    if not isinstance(token, str) or len(token) != TOKEN_CHARS:
        return None
    raw = from_b64url(token)
    if raw is None or len(raw) != TOKEN_BYTES:
        return None
    payload, mac = raw[:PAYLOAD_BYTES], raw[PAYLOAD_BYTES:]
    if not hmac.compare_digest(mac, mac_of(secret, payload)):
        return None
    if payload[0] != VERSION or payload[1] not in LEGS:
        return None
    at = 2 + SESSION_BYTES
    (expires,) = struct.unpack(">I", payload[at + STATION_BYTES :])
    return Grant(payload[1], payload[2:at], payload[at : at + STATION_BYTES], expires)


def verify_watch(secret: bytes, token: str) -> Optional[Grant]:
    """A watch v2 relay socket grant only; never accepts a primary grant."""
    if not isinstance(token, str) or len(token) != WATCH_TOKEN_CHARS:
        return None
    raw = from_b64url(token)
    if raw is None or len(raw) != WATCH_TOKEN_BYTES:
        return None
    payload, mac = raw[:WATCH_PAYLOAD_BYTES], raw[WATCH_PAYLOAD_BYTES:]
    expected = hmac.new(bytes(secret), WATCH_PREFIX + payload, hashlib.sha256).digest()
    if not hmac.compare_digest(mac, expected):
        return None
    if payload[0] != WATCH_VERSION or payload[1] not in LEGS or payload[2] != PURPOSE_WATCH:
        return None
    at = 3 + SESSION_BYTES
    (expires,) = struct.unpack(">I", payload[at + STATION_BYTES :])
    return Grant(payload[1], payload[3:at], payload[at : at + STATION_BYTES], expires, PURPOSE_WATCH, WATCH_VERSION)
