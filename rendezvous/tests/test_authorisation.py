# no-port-check: NereusSDR-original.
"""Authorisation invariants: registration needs a valid proof over a fresh
challenge and an id derived from the key; a station receives introductions
only for its own id; an unknown id and an offline one answer the same
bytes; relay credentials are minted only after the station accepts."""

import asyncio
import os

from cryptography.hazmat.primitives.asymmetric import ec

from nereus_rendezvous import identity, protocol
from helpers import expect_closed, introduce_message, live_service, random_id, recv_json, register
from runner import ws_connect


async def _client(uri, address="192.0.2.50"):
    ws = await ws_connect(uri, address)
    hello = await recv_json(ws)
    return ws, hello


async def _silent(ws, timeout=0.2):
    try:
        text = await asyncio.wait_for(ws.recv(), timeout)
    except asyncio.TimeoutError:
        return
    raise AssertionError(f"received {text[:100]}")


def test_unproven_registration_is_not_registered():
    async def go():
        async with live_service() as (service, uri):
            key = ec.generate_private_key(ec.SECP256R1())
            spki = identity.spki_of(key.public_key())
            sid = identity.rendezvous_id(spki)
            st = await ws_connect(uri, "192.0.2.1")
            await recv_json(st)
            await st.send(protocol.encode({"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)}))
            assert (await recv_json(st))["type"] == "challenge"
            # Challenged but never proved: not registered.
            cl, hello = await _client(uri)
            await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
            assert (await recv_json(cl))["code"] == "offline"
            await _silent(st)
            # A signature over the hello nonce instead of the challenge fails.
            wrong = identity.sign_raw(key, identity.register_transcript(identity.from_b64url(hello["nonce"])))
            await st.send(protocol.encode({"type": "prove", "signature": identity.to_b64url(wrong)}))
            assert (await recv_json(st))["code"] == "proofFailed"
            await expect_closed(st)
            assert sid not in service.stations

    asyncio.run(go())


def test_each_registration_gets_a_fresh_challenge():
    async def go():
        async with live_service() as (service, uri):
            key = ec.generate_private_key(ec.SECP256R1())
            spki = identity.spki_of(key.public_key())
            sid = identity.rendezvous_id(spki)
            nonces = set()
            for _ in range(3):
                st = await ws_connect(uri, "192.0.2.1")
                hello = await recv_json(st)
                await st.send(protocol.encode({"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)}))
                challenge = (await recv_json(st))["nonce"]
                assert challenge != hello["nonce"]
                nonces.add(challenge)
                nonces.add(hello["nonce"])
                await st.close()
            assert len(nonces) == 6

    asyncio.run(go())


def test_a_proof_replayed_on_another_connection_fails():
    async def go():
        async with live_service() as (service, uri):
            key = ec.generate_private_key(ec.SECP256R1())
            spki = identity.spki_of(key.public_key())
            sid = identity.rendezvous_id(spki)
            reg = protocol.encode({"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)})
            first = await ws_connect(uri, "192.0.2.1")
            await recv_json(first)
            await first.send(reg)
            nonce = identity.from_b64url((await recv_json(first))["nonce"])
            proof = identity.to_b64url(identity.sign_raw(key, identity.register_transcript(nonce)))
            # An observer replays the proof on its own connection.
            second = await ws_connect(uri, "198.51.100.3")
            await recv_json(second)
            await second.send(reg)
            await recv_json(second)
            await second.send(protocol.encode({"type": "prove", "signature": proof}))
            assert (await recv_json(second))["code"] == "proofFailed"
            await expect_closed(second)
            assert sid not in service.stations

    asyncio.run(go())


def test_an_id_not_derived_from_the_key_is_refused_before_a_challenge():
    async def go():
        async with live_service() as (service, uri):
            key = ec.generate_private_key(ec.SECP256R1())
            victim = random_id()
            st = await ws_connect(uri, "192.0.2.1")
            await recv_json(st)
            spki = identity.spki_of(key.public_key())
            await st.send(protocol.encode({"type": "register", "id": victim, "publicKey": identity.to_b64url(spki)}))
            assert (await recv_json(st))["code"] == "proofFailed"
            await expect_closed(st)

    asyncio.run(go())


def test_introductions_reach_only_their_own_station():
    async def go():
        async with live_service() as (service, uri):
            a, _, sid_a = await register(uri)
            b, _, sid_b = await register(uri, address="198.51.100.2")
            for target, other, sid in ((a, b, sid_a), (b, a, sid_b)):
                cl, hello = await _client(uri)
                await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
                got = await recv_json(target)
                assert got["type"] == "introduction"
                assert got["nonce"] == hello["nonce"]
                await _silent(other)
                await cl.close()
                assert (await recv_json(target))["type"] == "introduction.end"

    asyncio.run(go())


def test_unknown_and_offline_answer_the_same_bytes():
    async def go():
        async with live_service({"introductionsPerAddressPerMinute": 100}) as (service, uri):
            st, _, sid = await register(uri)
            await st.close()
            while sid in service.stations:
                await asyncio.sleep(0.001)
            half = ec.generate_private_key(ec.SECP256R1())
            half_spki = identity.spki_of(half.public_key())
            half_id = identity.rendezvous_id(half_spki)
            pending = await ws_connect(uri, "192.0.2.9")
            await recv_json(pending)
            await pending.send(protocol.encode({"type": "register", "id": half_id, "publicKey": identity.to_b64url(half_spki)}))
            await recv_json(pending)
            raw = []
            for target in (sid, random_id(), half_id):
                cl, hello = await _client(uri)
                await cl.send(protocol.encode(introduce_message(target, hello["nonce"])))
                raw.append(await asyncio.wait_for(cl.recv(), 5))
                await cl.close()
            assert raw[0] == raw[1] == raw[2]
            assert raw[0] == protocol.encode(protocol.error_message("offline", "client"))

    asyncio.run(go())


def test_credentials_only_after_acceptance(monkeypatch):
    """Relay credentials are minted only when the station answers with turn
    true: never on an introduction alone, never on an answer with turn
    false, and the one minted object goes to both ends."""
    from nereus_rendezvous import service as service_module

    minted = []
    real_mint = service_module.turn.mint

    def counting_mint(*args, **kwargs):
        value = real_mint(*args, **kwargs)
        minted.append(value)
        return value

    monkeypatch.setattr(service_module.turn, "mint", counting_mint)

    async def go():
        async with live_service() as (service, uri):
            st, _, sid = await register(uri)
            # An introduction, unanswered: nothing minted, nothing sent on.
            cl, hello = await _client(uri)
            await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
            intro = await recv_json(st)
            assert intro["type"] == "introduction"
            await _silent(cl)
            await _silent(st)
            assert minted == []
            # Answered without relay: turn null, no credentials message.
            await st.send(protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": False}))
            answer = await recv_json(cl)
            assert answer == {"type": "answer", "answer": "v=0\r\n", "turn": None}
            await _silent(st)
            assert minted == []
            await cl.close()
            assert (await recv_json(st))["type"] == "introduction.end"
            # Answered with relay: one object minted, the same to both ends.
            cl2, hello2 = await _client(uri, "192.0.2.51")
            await cl2.send(protocol.encode(introduce_message(sid, hello2["nonce"])))
            intro2 = await recv_json(st)
            await _silent(cl2)
            assert minted == []
            await st.send(protocol.encode({"type": "answer", "to": intro2["from"], "answer": "v=0\r\n", "turn": True}))
            answer2 = await recv_json(cl2)
            credentials = await recv_json(st)
            assert len(minted) == 1
            assert answer2["turn"] == minted[0]
            assert credentials == {"type": "credentials", "from": intro2["from"], "turn": minted[0]}
            assert minted[0]["username"].endswith(":" + sid)
            # A second answer to the same introduction mints nothing more.
            await st.send(protocol.encode({"type": "answer", "to": intro2["from"], "answer": "v=0\r\n", "turn": True}))
            assert (await recv_json(st))["code"] == "unknownIntroduction"
            assert len(minted) == 1

    asyncio.run(go())


def test_forwarded_fields_are_untouched():
    async def go():
        async with live_service() as (service, uri):
            st, _, sid = await register(uri)
            cl, hello = await _client(uri)
            msg = introduce_message(sid, hello["nonce"], offer="v=0\r\né中 \"q\" \\ \U0001f4fb\r\n")
            flipped = bytearray(identity.from_b64url(msg["deviceSignature"]))
            flipped[5] ^= 0x10
            msg["deviceSignature"] = identity.to_b64url(bytes(flipped))
            msg["extra"] = "never forwarded"
            await cl.send(protocol.encode(msg))
            got = await recv_json(st)
            assert got["device"] == msg["device"]
            assert got["deviceSignature"] == msg["deviceSignature"]
            assert got["offer"] == msg["offer"]
            assert "extra" not in got
            assert got["nonce"] == hello["nonce"]

    asyncio.run(go())


def test_a_client_supplied_nonce_is_never_used():
    async def go():
        async with live_service() as (service, uri):
            st, _, sid = await register(uri)
            cl, hello = await _client(uri)
            msg = introduce_message(sid, hello["nonce"])
            msg["nonce"] = identity.to_b64url(os.urandom(32))
            await cl.send(protocol.encode(msg))
            got = await recv_json(st)
            assert got["nonce"] == hello["nonce"] != msg["nonce"]

    asyncio.run(go())
