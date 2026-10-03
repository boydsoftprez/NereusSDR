# no-port-check: NereusSDR-original.
"""Outbound queues and message sizes: a peer that stops reading is closed
(1008) at its own byte cap or at the service's budget for every queue
together; the WebSocket options that bound what the service holds; the
sender's rule for the encoded size (section 2) at its worst case; and a
handler's failure logged by class name only."""

import asyncio
import json
import logging

import pytest
from cryptography.hazmat.primitives.asymmetric import ec

from nereus_rendezvous import identity, protocol, transport
from nereus_rendezvous.clock import ManualClock
from nereus_rendezvous.service import CLOSE_NOT_READING, Service
from helpers import introduce_message, live_service, recv_json, register
from runner import make_config, ws_connect


class FakeTransport:
    """A connection whose peer either reads everything at once or never
    reads at all (its send never returns)."""

    def __init__(self, reads: bool) -> None:
        self.reads = reads
        self.incoming: "asyncio.Queue" = asyncio.Queue()
        self.sent = []
        self.closed_with = None

    async def send(self, text: str) -> None:
        self.sent.append(text)
        if not self.reads:
            await asyncio.Event().wait()

    async def close(self, code: int) -> None:
        self.closed_with = code
        await self.incoming.put(None)

    def __aiter__(self):
        return self

    async def __anext__(self):
        item = await self.incoming.get()
        if item is None:
            raise StopAsyncIteration
        return item


async def _settle():
    for _ in range(20):
        await asyncio.sleep(0)


def _service(**limits):
    config = make_config({}, b"secret")
    for key, value in limits.items():
        setattr(config, key, value)
    return Service(config, ManualClock())


async def _start(service, fake, address):
    before = set(service.connections)
    task = asyncio.ensure_future(service.run_connection(fake, address))
    await _settle()
    (conn,) = set(service.connections) - before
    return conn, task


def test_a_peer_that_stops_reading_is_closed_at_its_byte_cap():
    async def go():
        service = _service(send_queue_bytes=1048576, mailbox_messages_per_side=64)
        station_io = FakeTransport(reads=True)
        station, _ = await _start(service, station_io, "192.0.2.1")
        key = ec.generate_private_key(ec.SECP256R1())
        spki = identity.spki_of(key.public_key())
        sid = identity.rendezvous_id(spki)
        await station_io.incoming.put(protocol.encode({"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)}))
        await _settle()
        nonce = identity.from_b64url(json.loads(station_io.sent[-1])["nonce"])
        proof = identity.to_b64url(identity.sign_raw(key, identity.register_transcript(nonce)))
        await station_io.incoming.put(protocol.encode({"type": "prove", "signature": proof}))
        await station_io.incoming.put(protocol.encode({"type": "nameplate.claim"}))
        await _settle()
        number = json.loads(station_io.sent[-1])["nameplate"]

        client_io = FakeTransport(reads=False)
        client, client_task = await _start(service, client_io, "198.51.100.1")
        await client_io.incoming.put(protocol.encode({"type": "mailbox.open", "nameplate": number}))
        await _settle()
        body = "b" * 60000
        sent = 0
        while not client.closing and sent < 64:
            await station_io.incoming.put(protocol.encode({"type": "mailbox", "body": body}))
            await _settle()
            sent += 1
        # 60000-byte bodies against a 1 MiB cap: the 18th does not fit.
        assert client.closing and sent == 18, sent
        assert client_io.closed_with == CLOSE_NOT_READING
        assert client.queued_bytes == 0 and service.queued_bytes == 0
        # Nothing queued after the cap went out: the writer stopped at hello.
        assert len(client_io.sent) == 1 and json.loads(client_io.sent[0])["type"] == "hello"
        assert json.loads(station_io.sent[-1]) == {"type": "mailbox.closed", "code": "peerLeft"}
        await client_task
        assert client not in service.connections

    asyncio.run(go())


def test_the_budget_for_every_queue_together_closes_the_largest_holder():
    """Section 9.1: a send that would pass the budget closes whoever holds
    the most queued bytes, not the recipient, unless the recipient is the
    largest holder itself."""

    async def go():
        service = _service(send_queue_bytes=1048576, send_budget_bytes=1500000)
        a_io, b_io = FakeTransport(reads=False), FakeTransport(reads=False)
        a, _ = await _start(service, a_io, "192.0.2.1")
        b, _ = await _start(service, b_io, "198.51.100.1")
        big = {"type": "mailbox", "body": "x" * 100000}
        for _ in range(8):
            a.send(big)
        assert not a.closing and service.queued_bytes > 800000
        sent = 0
        while not a.closing and sent < 8:
            b.send(big)
            sent += 1
        # a holds 8 messages and b fewer: the message to b that would pass
        # 1500000 bytes closes a, the largest holder, and b keeps it.
        assert a.closing and not b.closing
        assert a.queued_bytes == 0 and service.queued_bytes == b.queued_bytes
        await _settle()
        assert a_io.closed_with == CLOSE_NOT_READING and b_io.closed_with is None
        # Now b is the largest holder: its own next messages close it.
        while not b.closing:
            b.send(big)
        assert b.queued_bytes == 0 and service.queued_bytes == 0
        await _settle()
        assert b_io.closed_with == CLOSE_NOT_READING

    asyncio.run(go())


async def _registered_station(service, io, address):
    station, task = await _start(service, io, address)
    key = ec.generate_private_key(ec.SECP256R1())
    spki = identity.spki_of(key.public_key())
    sid = identity.rendezvous_id(spki)
    await io.incoming.put(protocol.encode({"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)}))
    await _settle()
    nonce = identity.from_b64url(json.loads(io.sent[-1])["nonce"])
    proof = identity.to_b64url(identity.sign_raw(key, identity.register_transcript(nonce)))
    await io.incoming.put(protocol.encode({"type": "prove", "signature": proof}))
    await _settle()
    assert json.loads(io.sent[-1])["type"] == "registered"
    return station, sid


def _fill(conn, total):
    """Queue at least `total` bytes, in steps of about 1 KiB, on a
    connection whose peer never reads."""
    filler = {"type": "mailbox", "body": "f" * 1000}
    while conn.queued_bytes < total:
        conn.send(filler)


@pytest.mark.parametrize("path", ["mailbox", "candidate"])
def test_an_evicted_sender_is_cleaned_up_after_its_own_message(path):
    """When forwarding a peer's message passes the budget and the sender is
    the largest holder, the sender is closed (1008), but what it sent still
    reaches the other end first, and only then the cleanup its leaving
    causes (mailbox.closed peerLeft, introduction.end clientLeft): the
    service's sends keep the order it read."""

    async def go():
        service = _service(
            send_queue_bytes=400000, send_budget_bytes=400000, send_queue_messages=1000, handshake_timeout_ms=600000
        )
        station_io = FakeTransport(reads=True)
        station, sid = await _registered_station(service, station_io, "192.0.2.1")
        client_io = FakeTransport(reads=False)
        client, client_task = await _start(service, client_io, "198.51.100.1")
        if path == "mailbox":
            await station_io.incoming.put(protocol.encode({"type": "nameplate.claim"}))
            await _settle()
            number = json.loads(station_io.sent[-1])["nameplate"]
            await client_io.incoming.put(protocol.encode({"type": "mailbox.open", "nameplate": number}))
            await _settle()
            assert json.loads(station_io.sent[-1])["type"] == "mailbox.opened"
            forwarded = {"type": "mailbox", "body": "m" * 60000}
            expected_forward = forwarded
            expected_cleanup = {"type": "mailbox.closed", "code": "peerLeft"}
        else:
            nonce = json.loads(client_io.sent[0])["nonce"]
            await client_io.incoming.put(protocol.encode(introduce_message(sid, nonce)))
            await _settle()
            intro = json.loads(station_io.sent[-1])
            assert intro["type"] == "introduction"
            await station_io.incoming.put(
                protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": False})
            )
            await _settle()
            candidate = "candidate:1 1 udp 2130706431 192.0.2.10 50000 typ host " + "x" * 3000
            forwarded = {"type": "candidate", "candidate": candidate}
            expected_forward = {"type": "candidate", "from": intro["from"], "candidate": candidate}
            expected_cleanup = {"type": "introduction.end", "from": intro["from"], "code": "clientLeft"}
        # The client stops reading and holds the most queued bytes; the
        # station reads everything and holds none.
        # Short of the budget by less than the forwarded message.
        _fill(client, 400000 - len(protocol.encode(expected_forward)) + 100)
        assert not client.closing and service.queued_bytes < 400000
        assert service.largest_holder() is client and not station.queued_bytes
        before = len(station_io.sent)
        await client_io.incoming.put(protocol.encode(forwarded))
        await _settle()
        assert client.closing and client_io.closed_with == CLOSE_NOT_READING
        after = [json.loads(t) for t in station_io.sent[before:]]
        assert after == [expected_forward, expected_cleanup], after
        assert not station.closing
        await client_task
        assert client not in service.connections

    asyncio.run(go())


def test_a_writer_blocked_too_long_is_closed():
    """Section 9.1: a connection whose writer spends longer than
    send_stall_ms (30000 by default) on one message is closed with 1008,
    since a ping stuck behind the same write would never time it out."""

    async def go():
        # A handshake timer longer than the stall limit, so only the stall
        # can close the connection here.
        service = _service(handshake_timeout_ms=600000)
        assert service.config.send_stall_ms == 30000
        io = FakeTransport(reads=False)
        conn, task = await _start(service, io, "192.0.2.1")
        conn.send({"type": "mailbox", "body": "queued behind the stuck hello"})
        service.clock.advance(29999)
        await _settle()
        assert not conn.closing
        service.clock.advance(1)
        await _settle()
        assert conn.closing and io.closed_with == CLOSE_NOT_READING
        assert service.queued_bytes == 0
        await task
        assert service.queued_bytes == 0 and not service.holders

    asyncio.run(go())


class RaisingTransport(FakeTransport):
    """A peer whose second send fails."""

    async def send(self, text: str) -> None:
        self.sent.append(text)
        if len(self.sent) >= 2:
            raise ConnectionResetError("gone")


def test_no_path_leaves_bytes_counted():
    """C1 of the follow-up review: whenever a writer ends or a connection
    goes, every byte it had queued is released from the budget."""

    async def go():
        big = {"type": "mailbox", "body": "y" * 1000}
        service = _service()

        # The peer goes away while its writer is stuck with messages queued.
        io = FakeTransport(reads=False)
        conn, task = await _start(service, io, "192.0.2.1")
        for _ in range(3):
            conn.send(big)
        assert service.queued_bytes > 3000
        await io.incoming.put(None)
        await task
        assert conn.queued_bytes == 0 and service.queued_bytes == 0

        # The writer's send raises with messages still queued.
        io = RaisingTransport(reads=True)
        conn, task = await _start(service, io, "192.0.2.2")
        for _ in range(3):
            conn.send(big)
        await _settle()
        assert conn.writer.done()
        assert conn.queued_bytes == 0 and service.queued_bytes == 0
        await io.incoming.put(None)
        await task

        # An error that closes, to a peer that never reads: the error and
        # the close wait behind the stuck hello until the stall limit.
        io = FakeTransport(reads=False)
        conn, task = await _start(service, io, "192.0.2.3")
        conn.send(big)
        conn.fail("protocolError")
        assert conn.closing and service.queued_bytes > 1000
        service.clock.advance(service.config.send_stall_ms)
        await _settle()
        assert conn.queued_bytes == 0 and service.queued_bytes == 0
        await task
        assert service.queued_bytes == 0 and not service.holders

    asyncio.run(go())


def test_websocket_options_bound_what_is_held():
    config = make_config({}, b"secret")
    kwargs = transport.serve_kwargs(config)
    assert kwargs["max_size"] == protocol.MAX_MESSAGE_BYTES == 131072
    assert kwargs["max_queue"] == 1
    assert kwargs["write_limit"] == 32768


def test_the_encoded_size_rule_at_its_worst_case():
    """Section 2: a field's cap counts decoded UTF-8 bytes, and the whole
    encoded message must also fit in 131072 bytes. A legal 65536-byte body
    whose every character must be escaped cannot be sent; the largest one
    that fits is forwarded unchanged; and the longest introduction the
    service can build from a 131072-byte introduce is within
    MAX_SERVICE_MESSAGE_BYTES, which peers accept."""
    from websockets.exceptions import ConnectionClosed

    async def go():
        async with live_service({"connectionsPerAddress": 8}) as (service, uri):
            st, _, sid = await register(uri)
            await st.send(protocol.encode({"type": "nameplate.claim"}))
            number = (await recv_json(st))["nameplate"]

            # Every character a control character: 6 bytes each on the wire.
            worst = "\x01" * protocol.MAX_BODY_BYTES
            text = protocol.encode({"type": "mailbox", "body": worst})
            assert len(text.encode()) > protocol.MAX_MESSAGE_BYTES
            cl = await ws_connect(uri, "198.51.100.1")
            await recv_json(cl)
            await cl.send(protocol.encode({"type": "mailbox.open", "nameplate": number}))
            assert (await recv_json(cl))["type"] == "mailbox.opened"
            assert (await recv_json(st))["type"] == "mailbox.opened"
            await cl.send(text)
            with pytest.raises(ConnectionClosed) as info:
                await asyncio.wait_for(cl.recv(), 5)
            assert info.value.rcvd.code == 1009
            # websockets 10.4 (legacy) ends the connection only after its
            # close timeout (5 s) once it has failed it with 1009.
            assert (await recv_json(st, 15)) == {"type": "mailbox.closed", "code": "peerLeft"}

            # The largest all-escaped body that fits is forwarded unchanged.
            envelope = len(protocol.encode({"type": "mailbox", "body": ""}).encode())
            fits = "\x01" * ((protocol.MAX_MESSAGE_BYTES - envelope) // 6)
            cl2 = await ws_connect(uri, "198.51.100.2")
            await recv_json(cl2)
            await cl2.send(protocol.encode({"type": "mailbox.open", "nameplate": number}))
            await recv_json(cl2)
            await recv_json(st)
            await cl2.send(protocol.encode({"type": "mailbox", "body": fits}))
            raw = await asyncio.wait_for(st.recv(), 5)
            assert json.loads(raw)["body"] == fits
            assert len(raw.encode()) <= protocol.MAX_SERVICE_MESSAGE_BYTES

            # The longest introduce: an offer padded until the encoded
            # message is exactly 131072 bytes.
            cl3 = await ws_connect(uri, "198.51.100.3")
            hello = await recv_json(cl3)
            msg = introduce_message(sid, hello["nonce"], offer="\x01")
            short = len(protocol.encode(msg).encode())
            msg["offer"] = "\x01" * ((protocol.MAX_MESSAGE_BYTES - short) // 6 + 1)
            pad = protocol.MAX_MESSAGE_BYTES - len(protocol.encode(msg).encode())
            msg["offer"] += "v" * pad
            text = protocol.encode(msg)
            assert len(text.encode()) == protocol.MAX_MESSAGE_BYTES
            await cl3.send(text)
            raw = await asyncio.wait_for(st.recv(), 5)
            assert json.loads(raw)["type"] == "introduction"
            assert len(raw.encode()) <= protocol.MAX_SERVICE_MESSAGE_BYTES <= protocol.PEER_RECEIVE_BYTES

    asyncio.run(go())


def test_a_failing_handler_is_logged_by_class_name_only(caplog, monkeypatch):
    secret_words = "SECRET-CONTENT-OF-THE-EXCEPTION"

    def boom(self, conn, msg):
        raise ValueError(secret_words)

    monkeypatch.setattr(Service, "_client_mailbox_open", boom)

    async def go():
        async with live_service() as (service, uri):
            ws = await ws_connect(uri, "192.0.2.1")
            await recv_json(ws)
            await ws.send(protocol.encode({"type": "mailbox.open", "nameplate": 3}))
            from websockets.exceptions import ConnectionClosed

            with pytest.raises(ConnectionClosed):
                await asyncio.wait_for(ws.recv(), 5)

    with caplog.at_level(logging.DEBUG, logger="nereus_rendezvous"):
        asyncio.run(go())
    text = "\n".join(r.getMessage() for r in caplog.records)
    assert "ValueError" in text
    assert secret_words not in text


def _turn_urls_of_json_length(total):
    """Eight TURN URLs whose compact JSON array is exactly `total` bytes."""
    chars = total - 2 - 7 - 8 * 2
    lengths = [chars // 8] * 8
    lengths[-1] += chars - sum(lengths)
    urls = ["turn:" + "a" * (n - 5) for n in lengths]
    assert len(json.dumps(urls, separators=(",", ":"))) == total
    return urls


def test_the_longest_answer_the_service_sends_is_within_its_bound():
    """Section 2: the service never sends more than 132096 bytes. The
    largest message it builds is the answer to a client, from a station's
    answer of 131072 bytes, with a turn object whose URL list is at the
    configuration's cap (768 bytes as JSON); one byte more is refused."""
    from nereus_rendezvous import config as cfg

    urls = _turn_urls_of_json_length(cfg.TURN_URLS_JSON_MAX)
    over = _turn_urls_of_json_length(cfg.TURN_URLS_JSON_MAX + 1)
    config = make_config({"turnUrls": over}, b"secret")
    with pytest.raises(cfg.ConfigError):
        Service(config, ManualClock())

    async def go():
        async with live_service({"turnUrls": urls}) as (service, uri):
            st, _, sid = await register(uri)
            cl = await ws_connect(uri, "198.51.100.1")
            hello = await recv_json(cl)
            await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
            intro = await recv_json(st)
            msg = {"type": "answer", "to": intro["from"], "answer": "\x01", "turn": True}
            short = len(protocol.encode(msg).encode())
            msg["answer"] = "\x01" * ((protocol.MAX_MESSAGE_BYTES - short) // 6 + 1)
            msg["answer"] += "v" * (protocol.MAX_MESSAGE_BYTES - len(protocol.encode(msg).encode()))
            text = protocol.encode(msg)
            assert len(text.encode()) == protocol.MAX_MESSAGE_BYTES
            await st.send(text)
            raw = await asyncio.wait_for(cl.recv(), 5)
            answer = json.loads(raw)
            assert answer["turn"]["urls"] == urls
            assert protocol.MAX_MESSAGE_BYTES < len(raw.encode()) <= protocol.MAX_SERVICE_MESSAGE_BYTES

    asyncio.run(go())
