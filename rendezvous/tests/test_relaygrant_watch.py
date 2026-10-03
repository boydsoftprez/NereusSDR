# no-port-check: NereusSDR-original.
"""The v2 watch grant grammar and its separation from primary relay grants."""

import asyncio
import hashlib
import hmac

import pytest

from nereus_rendezvous import relaygrant
from nereus_relay.relay import TAG_JOIN
from relay_helpers import SECRET as RELAY_SECRET, WALL, connect, expect_end, live_relay


SECRET = b"relay secret for watch grants"
SESSION = bytes(range(16))
STATION = bytes(range(16, 24))
EXPIRES = 1_800_000_000


def _signed(payload, prefix=relaygrant.WATCH_PREFIX, secret=SECRET):
    return relaygrant.to_b64url(payload + hmac.new(secret, prefix + payload, hashlib.sha256).digest())


def test_watch_wire_layout_and_primary_compatibility():
    for leg in relaygrant.LEGS:
        watch = relaygrant.mint_watch(SECRET, leg, SESSION, STATION, EXPIRES)
        raw = relaygrant.from_b64url(watch)
        payload = bytes([2, leg, 2]) + SESSION + STATION + EXPIRES.to_bytes(4, "big")
        assert len(watch) == 84 and len(raw) == 63
        assert raw == payload + hmac.new(SECRET, b"NereusSDR relay grant v2\n" + payload, hashlib.sha256).digest()
        assert relaygrant.verify_watch(SECRET, watch) == relaygrant.Grant(
            leg, SESSION, STATION, EXPIRES, relaygrant.PURPOSE_WATCH, relaygrant.WATCH_VERSION
        )
        assert relaygrant.verify(SECRET, watch) is None

        primary = relaygrant.mint(SECRET, leg, SESSION, STATION, EXPIRES)
        assert len(primary) == 83 and len(relaygrant.from_b64url(primary)) == 62
        assert relaygrant.verify(SECRET, primary) == relaygrant.Grant(leg, SESSION, STATION, EXPIRES)
        assert relaygrant.verify(SECRET, primary).purpose == relaygrant.PURPOSE_PRIMARY
        assert relaygrant.verify(SECRET, primary).version == relaygrant.VERSION
        assert relaygrant.verify_watch(SECRET, primary) is None


def test_watch_authentication_domain_version_and_purpose_are_strict():
    payload = bytes([2, relaygrant.LEG_DEVICE, 2]) + SESSION + STATION + EXPIRES.to_bytes(4, "big")
    valid = _signed(payload)
    assert relaygrant.verify_watch(SECRET, valid) is not None
    assert relaygrant.verify_watch(b"another secret", valid) is None
    assert relaygrant.verify_watch(SECRET, _signed(payload, relaygrant.PREFIX)) is None
    assert relaygrant.verify_watch(SECRET, _signed(bytes([2, 1, 1]) + payload[3:])) is None
    assert relaygrant.verify_watch(SECRET, _signed(bytes([2, 1, 3]) + payload[3:])) is None
    assert relaygrant.verify_watch(SECRET, _signed(bytes([3, 1, 2]) + payload[3:])) is None
    for leg in (0, 3):
        assert relaygrant.verify_watch(SECRET, _signed(bytes([2, leg, 2]) + payload[3:])) is None
    tampered = bytearray(relaygrant.from_b64url(valid))
    tampered[3] ^= 1
    assert relaygrant.verify_watch(SECRET, relaygrant.to_b64url(tampered)) is None


def test_watch_rejects_length_and_noncanonical_forms():
    token = relaygrant.mint_watch(SECRET, 1, SESSION, STATION, EXPIRES)
    for invalid in (token[:-1], token + "A", token + "=", token[:3] + "+" + token[4:], token[:3] + "/" + token[4:]):
        assert relaygrant.verify_watch(SECRET, invalid) is None
    assert relaygrant.verify_watch(SECRET, _signed(b"\x02\x01\x02" + SESSION + STATION + b"\x00" * 3)) is None
    assert relaygrant.verify_watch(SECRET, _signed(b"\x02\x01\x02" + SESSION + STATION + b"\x00" * 5)) is None

    # A v1 token has unused low bits in its final base64url character.
    primary = relaygrant.mint(SECRET, 1, SESSION, STATION, EXPIRES)
    alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
    altered = primary[:-1] + alphabet[alphabet.index(primary[-1]) ^ 1]
    assert relaygrant.from_b64url(altered) is None
    assert relaygrant.verify(SECRET, altered) is None


def test_watch_expiry_wire_boundaries_and_mint_inputs():
    for expires in (0, relaygrant.EXPIRES_MAX):
        token = relaygrant.mint_watch(SECRET, 1, SESSION, STATION, expires)
        assert relaygrant.verify_watch(SECRET, token).expires == expires
    for expires in (-1, relaygrant.EXPIRES_MAX + 1):
        with pytest.raises(ValueError):
            relaygrant.mint_watch(SECRET, 1, SESSION, STATION, expires)
    for leg, session, station in ((0, SESSION, STATION), (3, SESSION, STATION), (1, SESSION[:-1], STATION), (1, SESSION, STATION[:-1])):
        with pytest.raises(ValueError):
            relaygrant.mint_watch(SECRET, leg, session, station, EXPIRES)


def test_watch_grant_cannot_create_a_primary_session():
    async def go():
        async with live_relay() as (relay, _, uri):
            watch = relaygrant.mint_watch(RELAY_SECRET, relaygrant.LEG_DEVICE, SESSION, STATION, WALL + 120)
            ws = await connect(uri)
            await ws.send(bytes([TAG_JOIN]) + watch.encode("ascii"))
            await expect_end(ws, "peerGone")
            assert relay.sessions == {} and relay.pending == 0

    asyncio.run(go())
