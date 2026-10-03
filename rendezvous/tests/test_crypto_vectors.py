# no-port-check: NereusSDR-original.
"""The crypto and derivation vectors, against the service's identity, TURN
and relay grant code, and TURN passwords and relay grant MACs recomputed
here with hmac directly."""

import base64
import hashlib
import hmac

import pytest
from cryptography.hazmat.primitives.asymmetric import ec

from nereus_rendezvous import identity, relaygrant, turn
from runner import load_fixture, load_manifest

CRYPTO = {f["id"]: f["file"] for f in load_manifest()["fixtures"] if f["kind"] == "crypto"}


def test_every_crypto_file_is_listed():
    assert set(CRYPTO) == {
        "base64url",
        "p256-spki",
        "rendezvous-id",
        "register-proof",
        "introduce-signature",
        "turn-credentials",
        "relay-grant",
    }


def test_base64url():
    for case in load_fixture(CRYPTO["base64url"])["cases"]:
        decoded = identity.from_b64url(case["text"])
        if case["valid"]:
            assert decoded is not None and decoded.hex() == case["bytesHex"], case
        else:
            assert decoded is None, case


def test_p256_spki():
    for case in load_fixture(CRYPTO["p256-spki"])["cases"]:
        assert identity.is_p256_spki(bytes.fromhex(case["spkiHex"])) is case["valid"], case["name"]


def test_rendezvous_id():
    vectors = load_fixture(CRYPTO["rendezvous-id"])
    assert bytes.fromhex(vectors["prefixHex"]) == b"NereusSDR rendezvous id v1\n"
    for case in vectors["cases"]:
        spki = identity.from_b64url(case["publicKey"])
        assert hashlib.sha256(identity.ID_PREFIX + spki).hexdigest() == case["digestHex"]
        assert identity.rendezvous_id(spki) == case["id"]
        # The id is the first 26 characters of the lowercased base32 of the
        # digest, computed here without the service's code.
        assert base64.b32encode(bytes.fromhex(case["digestHex"])).decode().lower()[:26] == case["id"]
        assert identity.is_rendezvous_id(case["id"])


EDGE_CASES = {"highS", "rZero", "sIsN", "rIsN"}


def _fixed_key(block):
    """The vectors hold a fixed public key and no private key (the Global
    Constraints: no fixture holds a private key)."""
    assert set(block) == {"fixed", "publicKey", "id"}, sorted(block)
    spki = identity.b64url_of_length(block["publicKey"], identity.SPKI_BYTES)
    assert spki is not None and identity.is_p256_spki(spki)
    return spki


def test_register_proof():
    vectors = load_fixture(CRYPTO["register-proof"])
    spki = _fixed_key(vectors["key"])
    assert identity.rendezvous_id(spki) == vectors["key"]["id"]
    nonce = identity.from_b64url(vectors["nonce"])
    transcript = identity.register_transcript(nonce)
    assert transcript.hex() == vectors["transcriptHex"]
    assert transcript == b"NereusSDR rendezvous register v1\n" + nonce
    names = set()
    for case in vectors["cases"]:
        names.add(case["name"])
        pub = identity.b64url_of_length(case["publicKey"], identity.SPKI_BYTES)
        sig = identity.b64url_of_length(case["signature"], identity.SIGNATURE_BYTES)
        ok = pub is not None and sig is not None and identity.verify_raw(pub, transcript, sig)
        assert ok is case["valid"], case["name"]
    assert {"signed", "otherNonce", "flippedBit", "keyCompressed", "keyOtherCurve", "keyBadBase64url"} | EDGE_CASES <= names


def test_high_s_is_accepted_and_out_of_range_refused():
    """Section 4.1: r and s are each from 1 to n - 1; a high-s signature is
    valid. Checked here with a key made at run time."""
    n = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
    key = ec.generate_private_key(ec.SECP256R1())
    spki = identity.spki_of(key.public_key())
    message = identity.register_transcript(bytes(32))
    sig = identity.sign_raw(key, message)
    r, s = int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big")
    other = (n - s).to_bytes(32, "big")
    assert identity.verify_raw(spki, message, sig)
    assert identity.verify_raw(spki, message, sig[:32] + other)
    for bad_r, bad_s in ((0, s), (r, 0), (n, s), (r, n)):
        assert not identity.verify_raw(spki, message, bad_r.to_bytes(32, "big") + bad_s.to_bytes(32, "big"))


def test_introduce_signature():
    vectors = load_fixture(CRYPTO["introduce-signature"])
    spki = _fixed_key(vectors["device"])
    assert identity.to_b64url(identity.fingerprint(spki)) == vectors["device"]["id"]
    nonce = identity.from_b64url(vectors["nonce"])
    transcript = identity.introduce_transcript(vectors["stationId"], nonce)
    assert transcript.hex() == vectors["transcriptHex"]
    assert transcript == b"NereusSDR introduce v1\n" + vectors["stationId"].encode("ascii") + nonce
    names = set()
    for case in vectors["cases"]:
        names.add(case["name"])
        sig = identity.b64url_of_length(case["signature"], identity.SIGNATURE_BYTES)
        assert (sig is not None and identity.verify_raw(spki, transcript, sig)) is case["valid"], case["name"]
    assert EDGE_CASES <= names


def test_no_vector_holds_a_private_key():
    from runner import CONFORMANCE

    for path in CONFORMANCE.rglob("*"):
        if path.is_file():
            text = path.read_text(encoding="utf-8")
            assert "PRIVATE KEY" not in text and "privateKey" not in text and "Pkcs8" not in text, path


def test_turn_credentials():
    for case in load_fixture(CRYPTO["turn-credentials"])["cases"]:
        secret = case["secret"].encode("utf-8")
        assert turn.username_for(case["expires"], case["stationId"]) == case["username"]
        assert turn.password_for(secret, case["username"]) == case["password"]
        independent = base64.b64encode(hmac.new(secret, case["username"].encode(), hashlib.sha1).digest()).decode()
        assert independent == case["password"]
        minted = turn.mint(secret, case["stationId"], case["expires"] - 86400, 86400, ["turn:x"])
        assert minted == {"username": case["username"], "password": case["password"], "expires": case["expires"], "urls": ["turn:x"]}


def test_relay_grant():
    """Section 12.2: every valid token is what the service mints and the
    relay accepts, its MAC recomputed here with hmac directly; every
    invalid one is refused by the relay's check."""
    vectors = load_fixture(CRYPTO["relay-grant"])
    assert bytes.fromhex(vectors["prefixHex"]) == relaygrant.PREFIX
    assert bytes.fromhex(vectors["stationPrefixHex"]) == relaygrant.STATION_PREFIX
    for case in vectors["cases"]:
        secret = case["secret"].encode("utf-8")
        got = relaygrant.verify(secret, case["token"])
        if not case["valid"]:
            assert got is None, case["name"]
            continue
        session = bytes.fromhex(case["sessionHex"])
        station = bytes.fromhex(case["stationHex"])
        independent = hmac.new(secret, relaygrant.STATION_PREFIX + case["stationId"].encode(), hashlib.sha256).digest()[:8]
        assert relaygrant.station_of(secret, case["stationId"]) == station == independent, case["name"]
        assert relaygrant.mint(secret, case["leg"], session, station, case["expires"]) == case["token"], case["name"]
        assert got == relaygrant.Grant(case["leg"], session, station, case["expires"]), case["name"]
        raw = base64.urlsafe_b64decode(case["token"] + "=" * (-len(case["token"]) % 4))
        payload = bytes([1, case["leg"]]) + session + station + case["expires"].to_bytes(4, "big")
        assert raw[:30] == payload
        assert raw[30:] == hmac.new(secret, relaygrant.PREFIX + payload, hashlib.sha256).digest()
        assert len(case["token"]) == 83


@pytest.mark.parametrize("text", ["", "A" * 26, "a" * 25, "a" * 27, "abcdefghijklmnopqrstuvwxy1", "abcdefghijklmnopqrstuvwxy8"])
def test_rendezvous_id_shape_refused(text):
    assert not identity.is_rendezvous_id(text)
