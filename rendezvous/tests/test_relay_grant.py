# no-port-check: NereusSDR-original.
"""Relay grants at the rendezvous service (rendezvous document sections
12.1 and 12.2): minted only when the Core accepts an introduction with the
relay allowed, one session per introduction, the Core's leg and the
device's told apart, and accepted by the real relay; and the service's and
the relay's configuration."""

import asyncio
from pathlib import Path

import pytest

from nereus_rendezvous import config as rv_config
from nereus_rendezvous import protocol, relaygrant
from nereus_relay import config as relay_config
from nereus_relay.relay import TAG_MEDIA, TAG_PEER
from helpers import introduce_message, live_service, recv_json, register
from relay_helpers import SECRET, join, live_relay, recv
from runner import ws_connect

SERVER = Path(__file__).resolve().parent.parent / "server"


async def _client(uri, address="192.0.2.50"):
    ws = await ws_connect(uri, address)
    hello = await recv_json(ws)
    return ws, hello


async def _silent(ws, timeout=0.2):
    try:
        text = await asyncio.wait_for(ws.recv(), timeout)
    except asyncio.TimeoutError:
        return
    raise AssertionError(f"received {text[:80]}")


def test_grants_only_after_acceptance_with_the_relay_allowed(monkeypatch):
    from nereus_rendezvous import service as service_module

    minted = []
    real_mint = service_module.relaygrant.mint

    def counting_mint(*args, **kwargs):
        value = real_mint(*args, **kwargs)
        minted.append(value)
        return value

    monkeypatch.setattr(service_module.relaygrant, "mint", counting_mint)

    async def go():
        async with live_service({"relay": True}) as (service, uri):
            secret = service.config.relay_secret
            st, _, sid = await register(uri)
            # An introduction, unanswered (an unpaired device, say): nothing.
            cl, hello = await _client(uri)
            await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
            intro = await recv_json(st)
            await _silent(cl)
            await _silent(st)
            assert minted == []
            # Answered with the relay not allowed: no grant either.
            await st.send(protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": False}))
            assert (await recv_json(cl))["type"] == "answer"
            await _silent(cl)
            await _silent(st)
            assert minted == []
            await cl.close()
            assert (await recv_json(st))["type"] == "introduction.end"
            # Answered with the relay allowed: one grant, a token per end.
            cl2, hello2 = await _client(uri, "192.0.2.51")
            await cl2.send(protocol.encode(introduce_message(sid, hello2["nonce"])))
            intro2 = await recv_json(st)
            await st.send(protocol.encode({"type": "answer", "to": intro2["from"], "answer": "v=0\r\n", "turn": True}))
            assert (await recv_json(cl2))["type"] == "answer"
            device_grant = await recv_json(cl2)
            assert (await recv_json(st))["type"] == "credentials"
            core_grant = await recv_json(st)
            assert device_grant["type"] == core_grant["type"] == "relay.grant"
            assert core_grant["from"] == intro2["from"]
            assert len(minted) == 2
            core = relaygrant.verify(secret, core_grant["token"])
            device = relaygrant.verify(secret, device_grant["token"])
            assert core.leg == relaygrant.LEG_CORE and device.leg == relaygrant.LEG_DEVICE
            assert core.session == device.session
            assert core_grant["expires"] == device_grant["expires"] == core.expires
            # A second answer to the same introduction mints nothing more.
            await st.send(protocol.encode({"type": "answer", "to": intro2["from"], "answer": "v=0\r\n", "turn": True}))
            assert (await recv_json(st))["code"] == "unknownIntroduction"
            assert len(minted) == 2
            # Another introduction gets a session of its own.
            cl3, hello3 = await _client(uri, "192.0.2.52")
            await cl3.send(protocol.encode(introduce_message(sid, hello3["nonce"])))
            intro3 = await recv_json(st)
            await st.send(protocol.encode({"type": "answer", "to": intro3["from"], "answer": "v=0\r\n", "turn": True}))
            await recv_json(cl3)
            other = relaygrant.verify(secret, (await recv_json(cl3))["token"])
            assert other.session != core.session

    asyncio.run(go())


def test_the_services_grants_pair_on_the_real_relay():
    """End to end: the tokens the service sends each end admit the two legs
    of one session on the relay, which forwards between them."""

    async def go():
        async with live_service({"relay": True}) as (service, uri):
            service.config.relay_secret = SECRET
            async with live_relay() as (relay, clock, relay_uri):
                st, _, sid = await register(uri)
                cl, hello = await _client(uri)
                await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
                intro = await recv_json(st)
                await st.send(protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": True}))
                await recv_json(cl)
                device_token = (await recv_json(cl))["token"]
                await recv_json(st)
                core_token = (await recv_json(st))["token"]
                core_leg, ready = await join(relay_uri, core_token)
                assert ready[2] == 0
                device_leg, ready = await join(relay_uri, device_token)
                assert ready[2] == 1
                assert await recv(core_leg) == bytes([TAG_PEER, 1])
                await device_leg.send(bytes([TAG_MEDIA, 42]))
                assert await recv(core_leg) == bytes([TAG_MEDIA, 42])

    asyncio.run(go())


def test_service_relay_settings(tmp_path):
    turn_file = tmp_path / "turn-secret"
    relay_file = tmp_path / "relay-secret"
    turn_file.write_text("t" * 64 + "\n")
    relay_file.write_text("r" * 64 + "\n")
    conf = tmp_path / "rendezvous.conf"
    conf.write_text(
        "[rendezvous]\n"
        f"turn_secret_file = {turn_file}\n"
        f"relay_secret_file = {relay_file}\n"
        "relay_url = wss://rv.example.org/v1/relay\n"
        "relay_ttl_seconds = 90\n"
    )
    loaded = rv_config.load(str(conf))
    assert loaded.relay_secret == b"r" * 64 and loaded.relay_url == "wss://rv.example.org/v1/relay"
    assert loaded.relay_ttl_seconds == 90
    # One secret for both is refused.
    relay_file.write_text("t" * 64 + "\n")
    with pytest.raises(rv_config.ConfigError):
        rv_config.load(str(conf))
    relay_file.write_text("r" * 64 + "\n")
    for bad in ("https://rv.example.org/v1/relay", "wss://has space", "wss://" + "a" * 507, "ws://rv.example.org/"):
        conf.write_text(f"[rendezvous]\nrelay_url = {bad}\n")
        with pytest.raises(rv_config.ConfigError):
            rv_config.load(str(conf))
    for ttl in ("0", "86401"):
        conf.write_text(f"[rendezvous]\nrelay_ttl_seconds = {ttl}\n")
        with pytest.raises(rv_config.ConfigError):
            rv_config.load(str(conf))
    # No relay secret: no grants, and the defaults stand.
    defaults = rv_config.Config()
    assert defaults.relay_secret_file == "" and defaults.relay_ttl_seconds == 120
    assert defaults.relay_url == "wss://rv.nereussdr.com/v1/relay"


def test_relay_sample_configuration_is_the_defaults(tmp_path):
    secret = tmp_path / "relay-secret"
    secret.write_text("s" * 64 + "\n")
    text = (SERVER / "relay.conf.sample").read_text().replace("relay_secret_file =", f"relay_secret_file = {secret}")
    conf = tmp_path / "relay.conf"
    conf.write_text(text)
    loaded = relay_config.load(str(conf))
    expected = relay_config.Config()
    expected.relay_secret_file = str(secret)
    expected.relay_secret = b"s" * 64
    assert loaded == expected
    assert (expected.slots, expected.rate_bytes_per_second, expected.rejoin_ms, expected.idle_timeout_ms) == (16, 80000, 30000, 30000)
    assert (expected.socket, expected.socket_mode, expected.socket_group) == ("/run/nereus-relay/relay.sock", 0o660, "caddy")


@pytest.mark.parametrize(
    "text",
    [
        "[relay]\n",
        "[limits]\nslots = 0\n",
        "[limits]\nqueue_bytes = 1500\n",
        "[limits]\nrate_bytes_per_second = 1000\n",
        "[limits]\nnot_a_key = 1\n",
        "[other]\n",
        "[relay]\nlog_level = loud\n",
        "[relay]\nsocket = relative/relay.sock\n",
        "[relay]\nsocket_mode = 0999\n",
        "[relay]\nlisten = 127.0.0.1:8711\n",
    ],
)
def test_relay_configuration_refused(tmp_path, text):
    secret = tmp_path / "relay-secret"
    secret.write_text("s" * 64 + "\n")
    conf = tmp_path / "relay.conf"
    body = text if text == "[relay]\n" else f"[relay]\nrelay_secret_file = {secret}\n" + text.replace("[relay]\n", "")
    conf.write_text(body)
    with pytest.raises(relay_config.ConfigError):
        relay_config.load(str(conf))


def test_relay_secret_file_errors_name_no_contents(tmp_path):
    empty = tmp_path / "empty"
    empty.write_text("\n")
    conf = tmp_path / "relay.conf"
    conf.write_text(f"[relay]\nrelay_secret_file = {empty}\n")
    with pytest.raises(relay_config.ConfigError, match="empty"):
        relay_config.load(str(conf))
    conf.write_text(f"[relay]\nrelay_secret_file = {tmp_path / 'missing'}\n")
    with pytest.raises(relay_config.ConfigError, match="cannot read"):
        relay_config.load(str(conf))


def test_both_ends_get_one_expiry_even_when_the_clock_moves_between_sends():
    """Section 12.1 (M1): one expiry per grant, however the clock moves
    while the service sends the two halves."""
    from nereus_rendezvous import transport
    from nereus_rendezvous.clock import ManualClock
    from nereus_rendezvous.service import Service
    from runner import make_config

    class TickingClock(ManualClock):
        def wall_seconds(self) -> int:
            self._wall_start += 1
            return super().wall_seconds()

    async def go():
        service = Service(make_config({"relay": True}, b"test-secret"), TickingClock())
        server = await transport.start(service, "127.0.0.1", 0)
        uri = "ws://127.0.0.1:%d/" % server.sockets[0].getsockname()[1]
        try:
            st, _, sid = await register(uri)
            cl, hello = await _client(uri)
            await cl.send(protocol.encode(introduce_message(sid, hello["nonce"])))
            intro = await recv_json(st)
            await st.send(protocol.encode({"type": "answer", "to": intro["from"], "answer": "v=0\r\n", "turn": True}))
            await recv_json(cl)
            device = await recv_json(cl)
            await recv_json(st)
            core = await recv_json(st)
            assert device["expires"] == core["expires"]
            secret = service.config.relay_secret
            assert relaygrant.verify(secret, device["token"]).expires == relaygrant.verify(secret, core["token"]).expires == core["expires"]
        finally:
            await transport.stop([server], service, grace_s=0.05, timeout_s=5)

    asyncio.run(go())
