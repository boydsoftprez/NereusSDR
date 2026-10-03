# no-port-check: NereusSDR-original.
"""Identity values on the rendezvous wire.

The values are the station link's (docs/architecture/2026-09-23-station-link-v1.md
section 3.4): a public key is strict base64url of a canonical 91-byte P-256
SubjectPublicKeyInfo DER, a signature is ECDSA P-256 over SHA-256 as raw
r || s (64 bytes), base64url. The rendezvous id and the two transcripts are
defined in docs/architecture/2026-09-23-rendezvous-v1.md section 4.
"""

from __future__ import annotations

import base64
import hashlib
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
FINGERPRINT_BYTES = 32
INTRODUCTION_ID_BYTES = 16
ID_CHARS = 26
# The order n of the P-256 group (SEC 2, secp256r1).
P256_ORDER = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551

ID_PREFIX = b"NereusSDR rendezvous id v1\n"
REGISTER_PREFIX = b"NereusSDR rendezvous register v1\n"
INTRODUCE_PREFIX = b"NereusSDR introduce v1\n"

_B64URL_ALPHABET = re.compile(r"\A[A-Za-z0-9_-]*\Z")
_ID_PATTERN = re.compile(r"\A[a-z2-7]{26}\Z")


def to_b64url(data: bytes) -> str:
    """base64url without padding."""
    return base64.urlsafe_b64encode(data).decode("ascii").rstrip("=")


def from_b64url(text: str) -> Optional[bytes]:
    """Strict base64url: only A-Z a-z 0-9 - _, no padding, a length that is
    not 1 mod 4, and the unused low bits of the last character zero, so one
    value has one text. Returns None for anything else."""
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
    if not isinstance(text, str):
        return None
    data = from_b64url(text)
    if data is None or len(data) != length:
        return None
    return data


def is_p256_spki(spki: bytes) -> bool:
    """A canonical 91-byte P-256 SubjectPublicKeyInfo DER: it loads as a
    P-256 key and encodes back to the same bytes."""
    key = load_p256_spki(spki)
    return key is not None


def load_p256_spki(spki: bytes) -> Optional[ec.EllipticCurvePublicKey]:
    if not isinstance(spki, (bytes, bytearray)) or len(spki) != SPKI_BYTES:
        return None
    try:
        key = serialization.load_der_public_key(bytes(spki))
    except (ValueError, TypeError):
        return None
    if not isinstance(key, ec.EllipticCurvePublicKey):
        return None
    if key.curve.name != "secp256r1":
        return None
    again = key.public_bytes(
        serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo
    )
    if again != bytes(spki):
        return None
    return key


def spki_of(key: ec.EllipticCurvePublicKey) -> bytes:
    return key.public_bytes(
        serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo
    )


def rendezvous_id(spki: bytes) -> str:
    """RFC 4648 base32 (standard alphabet, lowercased, no padding) of
    SHA-256("NereusSDR rendezvous id v1\\n" || SPKI DER), first 26
    characters."""
    digest = hashlib.sha256(ID_PREFIX + bytes(spki)).digest()
    return base64.b32encode(digest).decode("ascii").lower().rstrip("=")[:ID_CHARS]


def is_rendezvous_id(text: object) -> bool:
    return isinstance(text, str) and bool(_ID_PATTERN.match(text))


def fingerprint(spki: bytes) -> bytes:
    """A device id is SHA-256 of its SPKI DER (link section 3.5)."""
    return hashlib.sha256(bytes(spki)).digest()


def register_transcript(nonce: bytes) -> bytes:
    """What a station signs to register: the prefix, then the challenge
    nonce's 32 raw bytes (decoded from its base64url)."""
    return REGISTER_PREFIX + bytes(nonce)


def introduce_transcript(station_id: str, nonce: bytes) -> bytes:
    """What a device signs to introduce itself: the prefix, the station id's
    26 ASCII bytes, then the introducing connection's hello nonce as 32 raw
    bytes."""
    return INTRODUCE_PREFIX + station_id.encode("ascii") + bytes(nonce)


def verify_raw(spki: bytes, message: bytes, signature: bytes) -> bool:
    """ECDSA P-256 over SHA-256 with a raw 64-byte r || s signature."""
    key = load_p256_spki(spki)
    if key is None or not isinstance(signature, (bytes, bytearray)):
        return False
    if len(signature) != SIGNATURE_BYTES:
        return False
    r = int.from_bytes(signature[:32], "big")
    s = int.from_bytes(signature[32:], "big")
    # Rendezvous document section 4.1: r and s each from 1 to n - 1. A
    # high-s signature (s above n / 2) is valid, as OpenSSL and CryptoKit
    # verify it.
    if not (1 <= r < P256_ORDER and 1 <= s < P256_ORDER):
        return False
    try:
        key.verify(encode_dss_signature(r, s), bytes(message), ec.ECDSA(hashes.SHA256()))
    except InvalidSignature:
        return False
    return True


def sign_raw(private_key: ec.EllipticCurvePrivateKey, message: bytes) -> bytes:
    """Sign as a station or device does. The service never signs; tests and
    the conformance generator do."""
    r, s = decode_dss_signature(private_key.sign(bytes(message), ec.ECDSA(hashes.SHA256())))
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")
