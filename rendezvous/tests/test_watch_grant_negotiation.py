# no-port-check: NereusSDR-original.
"""Watch grants require explicit service and both-end support; old grants survive."""

import asyncio

import pytest

from helpers import introduce_message, live_service, recv_json, register
from nereus_rendezvous import config, protocol, relaygrant
from nereus_relay.relay import TAG_MEDIA, TAG_PEER, TAG_WATCH
from relay_helpers import join, live_relay, recv
from runner import ws_connect


@pytest.mark.parametrize("enabled,station_version,client_version,expected", [
    (1, 1, 1, True), (0, 1, 1, False),
    (1, None, 1, False), (1, 1, None, False),
    (1, 2, 1, False), (1, 1, 2, False),
])
def test_only_negotiated_pair_receives_watch_grants(enabled, station_version, client_version, expected, caplog, monkeypatch):
    minted = []
    mint_watch = relaygrant.mint_watch
    def record_mint(*args):
        token = mint_watch(*args)
        minted.append(token)
        return token
    monkeypatch.setattr(relaygrant, "mint_watch", record_mint)
    async def go():
        async with live_service({"relay": True}) as (service, uri):
            service.config.relay_watch_version = enabled
            station, _, sid = await register(uri, watch_relay_version=station_version)
            client = await ws_connect(uri, "192.0.2.50")
            hello = await recv_json(client)
            assert hello.get("watchRelayVersion", 0) == enabled
            assert hello["version"] == (2 if enabled else 1)
            introduction = introduce_message(sid, hello["nonce"])
            if client_version is not None:
                introduction["watchRelayVersion"] = client_version
            await client.send(protocol.encode(introduction))
            introduced = await recv_json(station)
            assert minted == []
            await station.send(protocol.encode({"type": "answer", "to": introduced["from"],
                                                "answer": "v=0\r\n", "turn": True}))
            assert (await recv_json(client))["type"] == "answer"
            device_grant = await recv_json(client)
            assert (await recv_json(station))["type"] == "credentials"
            core_grant = await recv_json(station)
            for message, leg in ((device_grant, relaygrant.LEG_DEVICE), (core_grant, relaygrant.LEG_CORE)):
                primary = relaygrant.verify(service.config.relay_secret, message["token"])
                assert primary is not None and primary.leg == leg and primary.version == 1
                assert ("watchToken" in message) is expected
                decoded = protocol.decode(message, "server", "client" if leg == relaygrant.LEG_DEVICE else "station")
                assert decoded == message
                if expected:
                    watch = relaygrant.verify_watch(service.config.relay_secret, message["watchToken"])
                    assert watch is not None and watch.leg == leg
                    assert (watch.session, watch.station, watch.expires) == (primary.session, primary.station, primary.expires)
                    assert relaygrant.verify(service.config.relay_secret, message["watchToken"]) is None
                    assert message["watchToken"] not in caplog.text
                assert message["token"] not in caplog.text
            if expected:
                assert device_grant["watchToken"] != core_grant["watchToken"]
                assert len(minted) == 2
                # Real service-issued grants admit distinct pairs on the real
                # loopback relay; this is routing evidence, not Core DTLS/TX.
                async with live_relay(relay_secret=service.config.relay_secret) as (relay, _, relay_uri):
                    primary_core, _ = await join(relay_uri, core_grant["token"])
                    primary_device, _ = await join(relay_uri, device_grant["token"])
                    assert await recv(primary_core) == bytes([TAG_PEER, 1])
                    watch_core, _ = await join(relay_uri, core_grant["watchToken"])
                    watch_device, _ = await join(relay_uri, device_grant["watchToken"])
                    assert await recv(watch_core) == bytes([TAG_PEER, 1])
                    await watch_device.send(bytes([TAG_WATCH, 42]))
                    assert await recv(watch_core) == bytes([TAG_WATCH, 42])
                    await primary_device.send(bytes([TAG_MEDIA, 43]))
                    assert await recv(primary_core) == bytes([TAG_MEDIA, 43])
                    assert len(relay.sessions) == 1
            else:
                assert minted == []
                assert set(device_grant) == {"type", "url", "token", "expires"}
                assert set(core_grant) == {"type", "from", "url", "token", "expires"}
    asyncio.run(go())


def test_relay_refusal_never_mints_watch_grants(monkeypatch):
    def unexpected_mint(*args):
        raise AssertionError("watch grant minted despite relay refusal")
    monkeypatch.setattr(relaygrant, "mint_watch", unexpected_mint)
    async def go():
        async with live_service({"relay": True}) as (service, uri):
            service.config.relay_watch_version = 1
            station, _, sid = await register(uri, watch_relay_version=1)
            client = await ws_connect(uri, "192.0.2.50")
            hello = await recv_json(client)
            introduction = introduce_message(sid, hello["nonce"])
            introduction["watchRelayVersion"] = 1
            await client.send(protocol.encode(introduction))
            introduced = await recv_json(station)
            await station.send(protocol.encode({"type": "answer", "to": introduced["from"],
                                                "answer": "v=0\r\n", "turn": False}))
            assert (await recv_json(client))["type"] == "answer"
            for connection in (station, client):
                with pytest.raises(asyncio.TimeoutError):
                    await asyncio.wait_for(connection.recv(), 0.05)
    asyncio.run(go())


def test_no_watch_advertisement_without_relay_secret():
    async def go():
        async with live_service() as (service, uri):
            service.config.relay_watch_version = 1
            client = await ws_connect(uri, "192.0.2.50")
            assert "watchRelayVersion" not in await recv_json(client)
    asyncio.run(go())


@pytest.mark.parametrize("bad", [True, False, 0, -1, "1", None, 65536])
def test_optional_wire_version_is_strict(bad):
    hello = {"type": "hello", "version": 1, "nonce": "A" * 43, "stun": [], "watchRelayVersion": bad}
    with pytest.raises(protocol.DecodeError):
        protocol.decode(hello, "server", "client")


def test_service_feature_is_off_by_default_and_config_bounded(tmp_path):
    assert config.Config().relay_watch_version == 0
    path = tmp_path / "rendezvous.conf"
    for value in (0, 1):
        path.write_text(f"[rendezvous]\nrelay_watch_version = {value}\n")
        assert config.load(str(path)).relay_watch_version == value
    for value in (-1, 2, "true"):
        path.write_text(f"[rendezvous]\nrelay_watch_version = {value}\n")
        with pytest.raises(config.ConfigError):
            config.load(str(path))
