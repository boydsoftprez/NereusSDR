# no-port-check: NereusSDR-original.
"""The configuration file: its defaults, the sample, and the refusals that
stop the server from starting."""

import dataclasses
import subprocess
import sys
from pathlib import Path

import pytest

from nereus_roundtable.config import LIMIT_FIELDS, ROUNDTABLE_KEYS, Config, ConfigError, load, wire_limits

SERVER = Path(__file__).resolve().parent.parent / "server"
SAMPLE = SERVER / "roundtable.conf.sample"


def test_defaults():
    config = load(None)
    assert config.ws_listen == ["127.0.0.1:8720", "[::1]:8720"]
    assert config.http_listen == ["127.0.0.1:8721", "[::1]:8721"]
    assert config.public_host == "roundtable.nereussdr.com"
    assert config.database == "/var/lib/nereus-roundtable/roundtable.db"
    assert config.helpers == []
    assert config.trusted_proxies == ["127.0.0.1", "::1"]
    assert wire_limits(config) == {
        "readingsPerSecond": 2,
        "presencePerSecond": 1,
        "messagesPerMinute": 6,
        "messagesPerHour": 60,
        "messageCodePoints": 500,
        "messageLines": 10,
        "namesPerMinute": 10,
        "reportsPerHour": 10,
        "slices": 4,
        "reads": 4,
    }
    assert (config.station_connections_per_group, config.live_sockets_per_group, config.live_sockets_total) == (8, 4, 500)
    assert (config.inbound_message_bytes, config.outbound_queue_messages, config.outbound_queue_bytes) == (16384, 1024, 4194304)
    assert (config.hello_timeout_ms, config.ping_interval_ms, config.silence_close_ms) == (10000, 20000, 45000)


def test_every_field_is_in_a_section():
    names = {f.name for f in dataclasses.fields(Config)}
    assert set(ROUNDTABLE_KEYS) | set(LIMIT_FIELDS) == names
    assert not set(ROUNDTABLE_KEYS) & set(LIMIT_FIELDS)


def test_the_sample_names_every_key_with_its_default():
    assert load(str(SAMPLE)) == Config()
    text = SAMPLE.read_text(encoding="utf-8")
    for key in ROUNDTABLE_KEYS + LIMIT_FIELDS:
        assert f"\n{key} =" in text, key


def _write(tmp_path, text):
    path = tmp_path / "roundtable.conf"
    path.write_text(text, encoding="utf-8")
    return str(path)


def test_a_file_sets_values(tmp_path):
    path = _write(
        tmp_path,
        "[roundtable]\npublic_host = rt.example.org\nws_listen = 127.0.0.1:9000\n"
        "helpers = ezp4x6p5d6d3opyk7lm2wuflhk\n[limits]\nslices = 2\nping_interval_ms = 0\n",
    )
    config = load(path)
    assert config.public_host == "rt.example.org"
    assert config.ws_listen == ["127.0.0.1:9000"]
    assert config.helpers == ["ezp4x6p5d6d3opyk7lm2wuflhk"]
    assert config.slices == 2 and config.ping_interval_ms == 0


@pytest.mark.parametrize(
    "text, words",
    [
        ("[roundtable]\npublic_hots = x\n", "unknown key public_hots"),
        ("[limits]\nreadings = 2\n", "unknown key readings"),
        ("[database]\npath = x\n", "unknown section [database]"),
        ("[limits]\nslices = 5\n", "slices"),
        ("[limits]\nslices = two\n", "slices"),
        ("[limits]\nhello_timeout_ms = 0\n", "hello_timeout_ms"),
        ("[limits]\ninbound_message_bytes = 16385\n", "inbound_message_bytes"),
        ("[roundtable]\npublic_host = Roundtable.example.org\n", "public_host"),
        ("[roundtable]\npublic_host = roundtable.example.org:443\n", "public_host"),
        ("[roundtable]\npublic_host = roundtable.example.org.\n", "public_host"),
        ("[roundtable]\nws_listen = localhost:8720\n", "listen"),
        ("[roundtable]\ntrusted_proxies = caddy\n", "trusted_proxies"),
        ("[roundtable]\nhelpers = JJ\n", "helpers"),
        ("[roundtable]\nlog_level = loud\n", "log_level"),
    ],
)
def test_refusals(tmp_path, text, words):
    with pytest.raises(ConfigError) as info:
        load(_write(tmp_path, text))
    assert words in str(info.value)


def test_an_unknown_key_exits_2_with_a_plain_message(tmp_path):
    path = _write(tmp_path, "[roundtable]\nlisten = 127.0.0.1:8720\n")
    result = subprocess.run(
        [sys.executable, "-m", "nereus_roundtable", "--config", path],
        cwd=str(SERVER),
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode == 2
    assert result.stderr.strip() == "nereus_roundtable: configuration: unknown key listen in [roundtable]"
    assert result.stdout == ""


def test_a_missing_file_exits_2(tmp_path):
    result = subprocess.run(
        [sys.executable, "-m", "nereus_roundtable", "--config", str(tmp_path / "none.conf")],
        cwd=str(SERVER),
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode == 2
    assert result.stderr.startswith("nereus_roundtable: configuration: cannot read")
    assert "Traceback" not in result.stderr

