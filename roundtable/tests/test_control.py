# no-port-check: NereusSDR-original.
"""The control fixtures (wire spec 11.3): every kind in each direction it
travels, and the refusals. Each is decoded with the receiver's decoder and,
when it decodes, encoded again and compared on the keys its kind lists.
Each refusal the server receives is also sent to the running server, which
must close with 4002 and send nothing first. The inbound cap, a lone
surrogate and a binary frame are checked against the running server here,
since a fixture file cannot hold them."""

import asyncio
import json

import pytest
from websockets.exceptions import ConnectionClosed

from nereus_roundtable import protocol
from helpers import expect_closed, live_service, recv_json, sign_in
from runner import DEFAULT_ADDRESS, load_fixture, load_manifest, ws_connect

CONTROL = [f for f in load_manifest()["fixtures"] if f["kind"] == "control"]

# The fixtures that carry keys no kind lists (wire spec 12: version 1 adds,
# so every receiver ignores what it does not know).
EXTRA_KEYS = {
    "station-hello-extra-keys",
    "station-presence-key-station",
    "server-station-room-extra-keys",
    "server-station-welcome-extra-limits",
}


def _load(entry):
    fixture = load_fixture(entry["file"])
    frm = fixture["from"]
    body = {"wireText"} if "wireText" in fixture else {"wire"}
    expected = {"from", "decodes"} | body | ({"to"} if frm == "server" else set())
    assert set(fixture) == expected, entry["id"]
    to = fixture.get("to", "server")
    assert (frm, to) in protocol.TABLES, entry["id"]
    return fixture, frm, to


def _text(fixture):
    if "wireText" in fixture:
        return fixture["wireText"]
    return json.dumps(fixture["wire"], ensure_ascii=False)


def _within(decoded, wire, path="message"):
    """`decoded` holds only keys `wire` holds, with the same values: the
    decoder keeps the listed keys and drops the rest."""
    if isinstance(decoded, dict):
        assert isinstance(wire, dict), path
        for key, value in decoded.items():
            assert key in wire, f"{path}.{key}"
            _within(value, wire[key], f"{path}.{key}")
        return
    if isinstance(decoded, list):
        assert isinstance(wire, list) and len(wire) == len(decoded), path
        for i, (d, w) in enumerate(zip(decoded, wire)):
            _within(d, w, f"{path}[{i}]")
        return
    assert type(decoded) is type(wire) or (isinstance(decoded, float) and isinstance(wire, (int, float))), path
    assert decoded == wire, path


@pytest.mark.parametrize("entry", CONTROL, ids=[f["id"] for f in CONTROL])
def test_decode(entry):
    fixture, frm, to = _load(entry)
    try:
        decoded = protocol.decode(_text(fixture), frm, to)
    except protocol.ProtocolError as exc:
        assert fixture["decodes"] is False, f"{entry['id']}: refused ({exc})"
        assert exc.close_code == protocol.CLOSE_PROTOCOL
        return
    assert fixture["decodes"] is True, entry["id"]
    encoded = json.loads(protocol.encode(decoded))
    _within(encoded, fixture["wire"])
    if entry["id"] in EXTRA_KEYS:
        assert encoded != fixture["wire"], f"{entry['id']} has no key its kind does not list"
    else:
        assert encoded == fixture["wire"], entry["id"]


def test_extra_key_fixtures_exist():
    ids = {f["id"] for f in CONTROL}
    assert EXTRA_KEYS <= ids


def test_every_kind_in_each_direction_has_an_accepted_and_a_refused_fixture():
    accepted, refused = set(), set()
    for entry in CONTROL:
        fixture, frm, to = _load(entry)
        kind = fixture["wire"].get("type") if "wire" in fixture else None
        (accepted if fixture["decodes"] else refused).add((frm, to, kind))
    for (frm, to), table in protocol.TABLES.items():
        for kind in table:
            assert (frm, to, kind) in accepted, ("accepted", frm, to, kind)
            # A kind with no keys (weblink, hidden) has no refused form: any
            # key it carries is one it does not list, which is ignored.
            if table[kind].fields or table[kind].post is not None:
                assert (frm, to, kind) in refused, ("refused", frm, to, kind)


INBOUND_REFUSALS = [
    e for e in CONTROL if not load_fixture(e["file"])["decodes"] and load_fixture(e["file"])["from"] != "server"
]


@pytest.mark.parametrize("entry", INBOUND_REFUSALS, ids=[f["id"] for f in INBOUND_REFUSALS])
def test_refusal_on_the_running_server(entry):
    fixture = load_fixture(entry["file"])

    async def go():
        async with live_service() as live:
            if fixture["from"] == "station":
                ws, _, _ = await sign_in(live)
            else:
                ws = await ws_connect(live.live_uri, DEFAULT_ADDRESS)
            await ws.send(_text(fixture))
            assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL

    asyncio.run(go())


def test_lone_surrogates_count_only_in_listed_keys():
    """Wire spec 5.1: a lone surrogate in a key the kind lists is a protocol
    error; one in a key it does not list is ignored with that key, at any
    depth."""
    listed = '{"type":"post","room":"usb-14212000","text":"\\ud800","clientId":"a"}'
    unlisted = '{"type":"post","room":"usb-14212000","text":"hi","clientId":"a","note":{"deep":["\\udc00"]}}'
    with pytest.raises(protocol.ProtocolError):
        protocol.decode(listed)
    assert protocol.decode(unlisted) == {"type": "post", "room": "usb-14212000", "text": "hi", "clientId": "a"}

    async def go():
        async with live_service() as live:
            ws, _, _ = await sign_in(live)
            before = live.service.frames_handled
            await ws.send('{"type":"mentionsSeen","t":1,"note":"\\ud800"}')
            await _caught_up(live, before + 1)
            await ws.send(listed)
            assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL

    asyncio.run(go())


async def _caught_up(live, frames):
    for _ in range(5000):
        if live.service.frames_handled >= frames:
            return
        await asyncio.sleep(0.001)
    raise AssertionError("the server did not read the frame")


def test_undecodable_text_and_binary_frames_close_4002():
    texts = [
        "not json",
        "[]",
        "{}",
        '{"type":7}',
        '{"type":"mentionsSeen","t":1,"t":2}',
        '{"type":"reading","slice":0,"dbm":NaN,"t":1,"present":true}',
        '{"type":"reading","slice":0,"dbm":-9e1,"t":1,"present":true}',
        '{"type":"mentionsSeen","t":1.0}',
        '{"type":"welcome"}',
        '{"type":"watch","room":"usb-14212000"}',
    ]

    async def go():
        async with live_service() as live:
            for text in texts:
                ws, _, _ = await sign_in(live)
                await ws.send(text)
                assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL, text
            ws, _, _ = await sign_in(live)
            await ws.send(b"\x00binary")
            assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL
            # Before sign-in too, on both paths.
            ws = await ws_connect(live.station_uri, DEFAULT_ADDRESS)
            await recv_json(ws)
            await ws.send(b"{}")
            assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL
            ws = await ws_connect(live.live_uri, DEFAULT_ADDRESS)
            await ws.send(b"{}")
            assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL
            ws = await ws_connect(live.live_uri, DEFAULT_ADDRESS)
            await ws.send('{"type":"presence","callsign":null,"grid":null,"atRadio":true,"slices":[]}')
            assert await expect_closed(ws) == protocol.CLOSE_PROTOCOL

    asyncio.run(go())


def test_inbound_cap():
    """A message over 16384 bytes never reaches the decoder: the WebSocket
    layer closes with 1009. One of exactly 16384 bytes is read."""

    async def go():
        async with live_service() as live:
            ws, _, _ = await sign_in(live)
            base = '{"type":"mentionsSeen","t":1,"pad":""}'
            exact = base.replace('"pad":""', '"pad":"' + "x" * (protocol.MAX_INBOUND_BYTES - len(base)) + '"')
            assert len(exact.encode()) == protocol.MAX_INBOUND_BYTES
            before = live.service.frames_handled
            await ws.send(exact)
            await _caught_up(live, before + 1)
            await ws.send(exact + " ")
            with pytest.raises(ConnectionClosed) as info:
                await asyncio.wait_for(ws.recv(), 5)
            assert info.value.rcvd is not None and info.value.rcvd.code == protocol.CLOSE_TOO_BIG

    asyncio.run(go())
