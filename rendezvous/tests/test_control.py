# no-port-check: NereusSDR-original.
"""The control fixtures: every kind in each direction it travels, and the
refusals. Each is decoded (and, when it decodes, encoded again and
compared); each refusal the service receives is also sent to the running
service, which must answer protocolError and close."""

import asyncio
import json

import pytest

from nereus_rendezvous import protocol
from helpers import expect_closed, live_service, recv_json, register
from runner import load_fixture, load_manifest, ws_connect

CONTROL = [f for f in load_manifest()["fixtures"] if f["kind"] == "control"]


def _load(entry):
    fixture = load_fixture(entry["file"])
    frm = fixture["from"]
    expected_keys = {"from", "wire", "decodes"} | ({"to"} if frm == "server" else set())
    assert set(fixture) == expected_keys, entry["id"]
    to = fixture.get("to", "server")
    assert (frm, to) in protocol.TABLES
    return fixture, frm, to


TURN_KEYS = ("username", "password", "expires", "urls")


def _listed(wire, frm, to):
    """Section 10.3: the re-encode comparison is on the keys the kind lists
    (and, inside a turn object, the keys it lists); a key the receiver does
    not know is ignored (section 5.1), so it takes no part."""
    out = {"type": wire["type"]}
    for key, kind in protocol.TABLES[(frm, to)][wire["type"]]:
        value = wire[key]
        if kind is protocol.TURN_OR_NULL and value is not None:
            value = {k: value[k] for k in TURN_KEYS}
        out[key] = value
    return out


@pytest.mark.parametrize("entry", CONTROL, ids=[f["id"] for f in CONTROL])
def test_decode(entry):
    fixture, frm, to = _load(entry)
    text = json.dumps(fixture["wire"], ensure_ascii=False)
    try:
        decoded = protocol.decode(protocol.parse_text(text), frm, to)
    except protocol.DecodeError:
        assert fixture["decodes"] is False, entry["id"]
        return
    assert fixture["decodes"] is True, entry["id"]
    assert json.loads(protocol.encode(decoded)) == _listed(fixture["wire"], frm, to)


def test_unknown_keys_at_peers_have_fixtures():
    """Section 5.1: a station and a client ignore keys they do not know in
    a kind they know. Server-to-peer additions (section 11) rely on it, so
    each receiving role has a fixture carrying one."""
    extra = set()
    for entry in CONTROL:
        fixture, frm, to = _load(entry)
        if fixture["decodes"] and frm == "server" and fixture["wire"] != _listed(fixture["wire"], frm, to):
            extra.add(to)
    assert extra == {"station", "client"}


def test_lone_surrogates_count_only_in_listed_keys():
    """Section 5.1: a lone surrogate in a key the kind lists is a protocol
    error; one in a key it does not list is ignored with that key. JSON
    fixture files cannot hold a lone surrogate portably, so this is here."""
    listed = '{"type":"mailbox","body":"\\ud800"}'
    unlisted = '{"type":"mailbox.open","nameplate":5,"note":"\\ud800"}'
    with pytest.raises(protocol.DecodeError):
        protocol.decode(protocol.parse_text(listed), "client", "server")
    assert protocol.decode(protocol.parse_text(unlisted), "client", "server") == {"type": "mailbox.open", "nameplate": 5}

    async def go():
        async with live_service() as (service, uri):
            ws = await ws_connect(uri, "192.0.2.1")
            await recv_json(ws)
            await ws.send(unlisted)
            assert (await recv_json(ws))["code"] == "nameplateUnknown"

    asyncio.run(go())


def test_every_kind_in_each_direction_has_a_fixture():
    seen = set()
    for entry in CONTROL:
        fixture, frm, to = _load(entry)
        if fixture["decodes"]:
            seen.add((frm, to, fixture["wire"]["type"]))
    for (frm, to), table in protocol.TABLES.items():
        for kind in table:
            assert (frm, to, kind) in seen, (frm, to, kind)


INBOUND_REFUSALS = [
    e for e in CONTROL if not load_fixture(e["file"])["decodes"] and load_fixture(e["file"])["from"] != "server"
]


@pytest.mark.parametrize("entry", INBOUND_REFUSALS, ids=[f["id"] for f in INBOUND_REFUSALS])
def test_refusal_on_the_live_service(entry):
    fixture = load_fixture(entry["file"])

    async def go():
        async with live_service() as (service, uri):
            if fixture["from"] == "station":
                ws, _, _ = await register(uri)
            else:
                ws = await ws_connect(uri, "192.0.2.1")
                await recv_json(ws)
            await ws.send(json.dumps(fixture["wire"], ensure_ascii=False))
            answer = await recv_json(ws)
            assert answer["type"] == "error" and answer["code"] == "protocolError", answer
            await expect_closed(ws)

    asyncio.run(go())


def test_undecodable_text_is_refused():
    texts = [
        "not json",
        "[]",
        '{"type":"mailbox.open","nameplate":1,"nameplate":2}',
        '{"type":"mailbox.open","nameplate":NaN}',
        '{"type":"mailbox","body":"\\ud800"}',
        "{}",
    ]

    async def go():
        async with live_service() as (service, uri):
            for text in texts:
                ws = await ws_connect(uri, "192.0.2.1")
                await recv_json(ws)
                await ws.send(text)
                answer = await recv_json(ws)
                assert answer["code"] == "protocolError", text
                await expect_closed(ws)
            ws = await ws_connect(uri, "192.0.2.1")
            await recv_json(ws)
            await ws.send(b"\x00binary")
            assert (await recv_json(ws))["code"] == "protocolError"
            await expect_closed(ws)

    asyncio.run(go())


def test_transport_cap():
    """A message over 128 KiB never reaches the decoder: the WebSocket
    layer closes with 1009. One of exactly 128 KiB is read."""
    from websockets.exceptions import ConnectionClosed

    async def go():
        async with live_service() as (service, uri):
            ws = await ws_connect(uri, "192.0.2.1")
            await recv_json(ws)
            base = '{"type":"mailbox.open","nameplate":5,"pad":""}'
            exact = base.replace('"pad":""', '"pad":"' + "x" * (protocol.MAX_MESSAGE_BYTES - len(base)) + '"')
            assert len(exact.encode()) == protocol.MAX_MESSAGE_BYTES
            await ws.send(exact)
            assert (await recv_json(ws))["code"] == "nameplateUnknown"
            await ws.send(exact + " ")
            with pytest.raises(ConnectionClosed) as info:
                await asyncio.wait_for(ws.recv(), 5)
            # rcvd is the close frame the service sent (websockets 10 and later).
            assert info.value.rcvd is not None and info.value.rcvd.code == 1009

    asyncio.run(go())
