# no-port-check: NereusSDR-original.
"""Identity values on the Roundtable wire (wire spec section 4).

Keys and signatures are the station link's and the rendezvous's: a public
key is strict base64url of a canonical 91-byte P-256 SubjectPublicKeyInfo
DER, a signature is ECDSA P-256 over SHA-256 as raw r || s (64 bytes). The
helpers are copied from rendezvous/server/nereus_rendezvous/identity.py
(our own code) with Roundtable's prefixes: the Roundtable id, the website
key id and the hello transcript (wire spec 4.1 to 4.3).
"""

from __future__ import annotations

import base64
import hashlib
import os
import re
from typing import Optional

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import (
    decode_dss_signature,
    encode_dss_signature,
)

SPKI_BYTES = 91
SIGNATURE_BYTES = 64
NONCE_BYTES = 32
WEBSITE_KEY_BYTES = 32
ID_CHARS = 26
# The order n of the P-256 group (SEC 2, secp256r1).
P256_ORDER = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551

ID_PREFIX = b"NereusSDR roundtable id v1\n"
KEY_PREFIX = b"NereusSDR roundtable key v1\n"
HELLO_PREFIX = b"NereusSDR roundtable hello v1\n"
# The rendezvous's own prefixes, used only to prove the two services never
# share an id or a signature (wire spec 4.1, 4.3).
RENDEZVOUS_ID_PREFIX = b"NereusSDR rendezvous id v1\n"
RENDEZVOUS_REGISTER_PREFIX = b"NereusSDR rendezvous register v1\n"

_B64URL_ALPHABET = re.compile(r"\A[A-Za-z0-9_-]*\Z")
_ID_PATTERN = re.compile(r"\A[a-z2-7]{26}\Z")


def to_b64url(data: bytes) -> str:
    """base64url without padding."""
    return base64.urlsafe_b64encode(bytes(data)).decode("ascii").rstrip("=")


def from_b64url(text: object) -> Optional[bytes]:
    """Strict base64url: only A-Z a-z 0-9 - _, no padding, a length that is
    not 1 mod 4, and the unused low bits of the last character zero, so one
    value has one text. None for anything else."""
    if not isinstance(text, str) or not _B64URL_ALPHABET.match(text):
        return None
    if len(text) % 4 == 1:
        return None
    padded = text + "=" * (-len(text) % 4)
    try:
        data = base64.urlsafe_b64decode(padded.encode("ascii"))
    except (ValueError, TypeError):
        return None
    if to_b64url(data) != text:
        return None
    return data


def b64url_of_length(text: object, length: int) -> Optional[bytes]:
    """Strict base64url that decodes to exactly `length` bytes."""
    data = from_b64url(text)
    if data is None or len(data) != length:
        return None
    return data


def load_p256_spki(spki: object) -> Optional[ec.EllipticCurvePublicKey]:
    """A canonical 91-byte P-256 SubjectPublicKeyInfo DER: it loads as a
    P-256 key and encodes back to the same bytes."""
    if not isinstance(spki, (bytes, bytearray)) or len(spki) != SPKI_BYTES:
        return None
    try:
        key = serialization.load_der_public_key(bytes(spki))
    except (ValueError, TypeError):
        return None
    if not isinstance(key, ec.EllipticCurvePublicKey) or key.curve.name != "secp256r1":
        return None
    if spki_of(key) != bytes(spki):
        return None
    return key


def is_p256_spki(spki: object) -> bool:
    return load_p256_spki(spki) is not None


def spki_of(key: ec.EllipticCurvePublicKey) -> bytes:
    return key.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)


def _id_of(prefix: bytes, data: bytes) -> str:
    digest = hashlib.sha256(prefix + bytes(data)).digest()
    return base64.b32encode(digest).decode("ascii").lower().rstrip("=")[:ID_CHARS]


def roundtable_id(spki: bytes) -> str:
    """RFC 4648 base32 (standard alphabet, lowercased, no padding) of
    SHA-256("NereusSDR roundtable id v1\\n" || SPKI DER), first 26
    characters (wire spec 4.1)."""
    return _id_of(ID_PREFIX, spki)


def rendezvous_id(spki: bytes) -> str:
    """The rendezvous id of the same key, for tests that prove the two
    differ. The server never uses it."""
    return _id_of(RENDEZVOUS_ID_PREFIX, spki)


def key_id(key: bytes) -> str:
    """A website key's station id: the same base32 form of
    SHA-256("NereusSDR roundtable key v1\\n" || the 32 raw key bytes)
    (wire spec 4.2)."""
    return _id_of(KEY_PREFIX, key)


def key_hash(key: bytes) -> bytes:
    """What the server stores for a website key: SHA-256 of its 32 raw bytes,
    no prefix. The key itself is never stored."""
    return hashlib.sha256(bytes(key)).digest()


def is_station_id(text: object) -> bool:
    return isinstance(text, str) and bool(_ID_PATTERN.match(text))


def new_key() -> bytes:
    """A new website key: 32 bytes from the operating system's generator."""
    return os.urandom(WEBSITE_KEY_BYTES)


def hello_transcript(host: str, nonce: bytes) -> bytes:
    """What a station key signs: the prefix, the host name's ASCII bytes,
    one line feed, then the challenge nonce's 32 raw bytes (wire spec 4.3)."""
    return HELLO_PREFIX + host.encode("ascii") + b"\n" + bytes(nonce)


def verify_raw(spki: bytes, message: bytes, signature: bytes) -> bool:
    """ECDSA P-256 over SHA-256 with a raw 64-byte r || s signature."""
    key = load_p256_spki(spki)
    if key is None or not isinstance(signature, (bytes, bytearray)) or len(signature) != SIGNATURE_BYTES:
        return False
    r = int.from_bytes(signature[:32], "big")
    s = int.from_bytes(signature[32:], "big")
    # Wire spec 4.1: r and s each from 1 to n - 1. A high-s signature is
    # valid, as OpenSSL and CryptoKit verify it.
    if not (1 <= r < P256_ORDER and 1 <= s < P256_ORDER):
        return False
    try:
        key.verify(encode_dss_signature(r, s), bytes(message), ec.ECDSA(hashes.SHA256()))
    except InvalidSignature:
        return False
    return True


def verify_hello(spki: object, sig: object, host: str, nonce: bytes) -> bool:
    """True when `spki` is a canonical P-256 key and `sig` verifies over the
    hello transcript built with this server's host name and the nonce sent
    on this connection."""
    if not isinstance(spki, (bytes, bytearray)) or not isinstance(sig, (bytes, bytearray)):
        return False
    return verify_raw(bytes(spki), hello_transcript(host, nonce), bytes(sig))


def sign_raw(private_key: ec.EllipticCurvePrivateKey, message: bytes) -> bytes:
    """Sign as a station does. The server never signs; tests do."""
    r, s = decode_dss_signature(private_key.sign(bytes(message), ec.ECDSA(hashes.SHA256())))
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")
