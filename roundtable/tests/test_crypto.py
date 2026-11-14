# no-port-check: NereusSDR-original.
"""The crypto vectors (wire spec 4, 11.2), checked against the server's own
identity code, with hashlib and cryptography directly where the vector
holds a digest."""

import hashlib

import pytest
from cryptography.hazmat.primitives.asymmetric import ec

from nereus_roundtable import identity
from runner import load_fixture

BASE64URL = load_fixture("crypto/base64url.json")
SPKI = load_fixture("crypto/p256-spki.json")
IDS = load_fixture("crypto/roundtable-id.json")
KEY_IDS = load_fixture("crypto/key-id.json")
HELLO = load_fixture("crypto/hello-signature.json")


def _private(hex_text):
    return ec.derive_private_key(int(hex_text, 16), ec.SECP256R1())


@pytest.mark.parametrize("case", BASE64URL["cases"], ids=[repr(c["text"]) for c in BASE64URL["cases"]])
def test_base64url(case):
    got = identity.from_b64url(case["text"])
    if case["valid"]:
        assert got == bytes.fromhex(case["bytesHex"])
        assert identity.to_b64url(got) == case["text"]
    else:
        assert got is None


@pytest.mark.parametrize("case", SPKI["cases"], ids=[c["name"] for c in SPKI["cases"]])
def test_p256_spki(case):
    assert identity.is_p256_spki(bytes.fromhex(case["spkiHex"])) is case["valid"]


def test_roundtable_id_prefixes():
    assert bytes.fromhex(IDS["prefixHex"]) == identity.ID_PREFIX
    assert bytes.fromhex(IDS["rendezvousPrefixHex"]) == identity.RENDEZVOUS_ID_PREFIX


@pytest.mark.parametrize("case", IDS["cases"], ids=[c["name"] for c in IDS["cases"]])
def test_roundtable_id(case):
    spki = bytes.fromhex(case["spkiHex"])
    assert identity.spki_of(_private(case["privateKeyHex"]).public_key()) == spki
    assert identity.from_b64url(case["publicKey"]) == spki
    assert hashlib.sha256(identity.ID_PREFIX + spki).hexdigest() == case["digestHex"]
    assert identity.roundtable_id(spki) == case["id"]
    assert hashlib.sha256(identity.RENDEZVOUS_ID_PREFIX + spki).hexdigest() == case["rendezvousDigestHex"]
    assert identity.rendezvous_id(spki) == case["rendezvousId"]
    assert case["id"] != case["rendezvousId"]
    assert identity.is_station_id(case["id"])


def test_key_id_prefix():
    assert bytes.fromhex(KEY_IDS["prefixHex"]) == identity.KEY_PREFIX


@pytest.mark.parametrize("case", KEY_IDS["cases"], ids=[c.get("why", c["key"][:8]) for c in KEY_IDS["cases"]])
def test_key_id(case):
    raw = identity.b64url_of_length(case["key"], identity.WEBSITE_KEY_BYTES)
    if not case["valid"]:
        assert raw is None
        return
    assert raw == bytes.fromhex(case["keyHex"])
    assert hashlib.sha256(identity.KEY_PREFIX + raw).hexdigest() == case["digestHex"]
    assert identity.key_id(raw) == case["id"]
    assert identity.key_hash(raw).hex() == case["keyHashHex"]
    assert hashlib.sha256(raw).hexdigest() == case["keyHashHex"]


def test_new_key_is_32_random_bytes():
    a, b = identity.new_key(), identity.new_key()
    assert len(a) == len(b) == 32 and a != b


def test_hello_transcript():
    nonce = identity.b64url_of_length(HELLO["nonce"], identity.NONCE_BYTES)
    assert nonce is not None
    assert identity.hello_transcript(HELLO["host"], nonce).hex() == HELLO["transcriptHex"]
    assert bytes.fromhex(HELLO["transcriptHex"]) == identity.HELLO_PREFIX + HELLO["host"].encode() + b"\n" + nonce


@pytest.mark.parametrize("case", HELLO["cases"], ids=[c["name"] for c in HELLO["cases"]])
def test_hello_signature(case):
    nonce = identity.from_b64url(HELLO["nonce"])
    spki = identity.from_b64url(case["publicKey"])
    sig = identity.b64url_of_length(case["signature"], identity.SIGNATURE_BYTES)
    assert identity.verify_hello(spki, sig, HELLO["host"], nonce) is case["valid"]


@pytest.mark.parametrize("which", ["key", "otherKey"])
def test_fixed_keys_sign_and_verify(which):
    entry = HELLO[which]
    private = _private(entry["privateKeyHex"])
    spki = identity.spki_of(private.public_key())
    assert identity.to_b64url(spki) == entry["publicKey"]
    if "id" in entry:
        assert identity.roundtable_id(spki) == entry["id"]
    nonce = identity.from_b64url(HELLO["nonce"])
    sig = identity.sign_raw(private, identity.hello_transcript(HELLO["host"], nonce))
    assert identity.verify_hello(spki, sig, HELLO["host"], nonce)
    assert not identity.verify_hello(spki, sig, "roundtable.example.invalid", nonce)
    rendezvous = identity.sign_raw(private, identity.RENDEZVOUS_REGISTER_PREFIX + nonce)
    assert not identity.verify_hello(spki, rendezvous, HELLO["host"], nonce)
