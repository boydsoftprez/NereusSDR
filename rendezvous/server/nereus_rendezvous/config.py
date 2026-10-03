# no-port-check: NereusSDR-original.
"""Configuration: one INI file read at start, and the TURN secret and the
relay secret from files of their own.

The secret files are separate so systemd can hand them over with
LoadCredential= and they never sit in the configuration or the repository.
The service never writes any of these files.
"""

from __future__ import annotations

import configparser
import dataclasses
import json
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


# The TURN URL list as the wire carries it (a compact JSON array) is at
# most this many bytes (rendezvous document section 2).
TURN_URLS_JSON_MAX = 768


class ConfigError(Exception):
    pass


@dataclass
class Config:
    # [rendezvous]
    listen: List[Tuple[str, int]] = field(default_factory=lambda: [("127.0.0.1", 8710), ("::1", 8710)])
    trusted_proxies: List[str] = field(default_factory=lambda: ["127.0.0.1", "::1"])
    stun_urls: List[str] = field(
        default_factory=lambda: [
            # IPv4 first: the pinned libjuice uses only the first STUN
            # server, and an IPv4-only peer behind NAT needs its
            # server-reflexive candidate (rendezvous document section 8).
            "stun:rv4.nereussdr.com:3478",
            "stun:rv6.nereussdr.com:3478",
        ]
    )
    # The same order as stun_urls, IPv4 first (rendezvous document section 8).
    turn_urls: List[str] = field(
        default_factory=lambda: [
            "turn:rv4.nereussdr.com:3478?transport=udp",
            "turn:rv4.nereussdr.com:443?transport=udp",
            "turn:rv6.nereussdr.com:3478?transport=udp",
            "turn:rv6.nereussdr.com:443?transport=udp",
        ]
    )
    turn_secret_file: str = ""
    turn_ttl_seconds: int = 86400
    # The WebSocket relay (rendezvous document section 12): the URL both
    # ends open, the secret shared with the relay, and how long a grant
    # admits a first join. Empty relay_secret_file: no grants.
    relay_url: str = "wss://rv.nereussdr.com/v1/relay"
    relay_secret_file: str = ""
    relay_ttl_seconds: int = 120
    # Enable only after the associated relay supports purpose-separated watch
    # legs. The default keeps existing deployments and old peers unchanged.
    relay_watch_version: int = 0
    log_level: str = "info"
    # [limits]
    introductions_per_address_per_minute: int = 30
    introductions_per_station_per_minute: int = 60
    mailbox_opens_per_address_per_minute: int = 10
    candidates_per_side: int = 64
    mailbox_messages_per_side: int = 32
    connections_per_address: int = 16
    stations_per_address: int = 4
    max_connections: int = 256
    max_stations: int = 2000
    handshake_timeout_ms: int = 10000
    idle_timeout_ms: int = 30000
    introduction_lifetime_ms: int = 120000
    mailbox_lifetime_ms: int = 300000
    ping_interval_seconds: int = 20
    ping_timeout_seconds: int = 20
    send_queue_messages: int = 256
    send_queue_bytes: int = 1048576
    send_budget_bytes: int = 33554432
    send_stall_ms: int = 30000
    socket_buffer_bytes: int = 16384
    # Not from the file: the secrets' bytes, read from turn_secret_file and
    # relay_secret_file.
    turn_secret: Optional[bytes] = field(default=None, repr=False)
    relay_secret: Optional[bytes] = field(default=None, repr=False)


_SECTIONS = {
    "rendezvous": [
        "listen",
        "trusted_proxies",
        "stun_urls",
        "turn_urls",
        "turn_secret_file",
        "turn_ttl_seconds",
        "relay_url",
        "relay_secret_file",
        "relay_ttl_seconds",
        "relay_watch_version",
        "log_level",
    ],
    "limits": [
        "introductions_per_address_per_minute",
        "introductions_per_station_per_minute",
        "mailbox_opens_per_address_per_minute",
        "candidates_per_side",
        "mailbox_messages_per_side",
        "connections_per_address",
        "stations_per_address",
        "max_connections",
        "max_stations",
        "handshake_timeout_ms",
        "idle_timeout_ms",
        "introduction_lifetime_ms",
        "mailbox_lifetime_ms",
        "ping_interval_seconds",
        "ping_timeout_seconds",
        "send_queue_messages",
        "send_queue_bytes",
        "send_budget_bytes",
        "send_stall_ms",
        "socket_buffer_bytes",
    ],
}


def parse_listen(text: str) -> List[Tuple[str, int]]:
    """`127.0.0.1:8710 [::1]:8710`: space-separated host:port, IPv6 in
    brackets."""
    out: List[Tuple[str, int]] = []
    for item in text.split():
        if item.startswith("["):
            host, sep, rest = item[1:].partition("]:")
            if not sep:
                raise ConfigError(f"listen: bad address {item!r}")
            port_text = rest
        else:
            host, sep, port_text = item.rpartition(":")
            if not sep:
                raise ConfigError(f"listen: bad address {item!r}")
        try:
            port = int(port_text)
        except ValueError as exc:
            raise ConfigError(f"listen: bad port in {item!r}") from exc
        if not 0 <= port <= 65535:
            raise ConfigError(f"listen: bad port in {item!r}")
        out.append((host, port))
    if not out:
        raise ConfigError("listen: no address")
    return out


def read_secret(path: str, what: str = "TURN") -> bytes:
    try:
        with open(path, "rb") as handle:
            data = handle.read()
    except OSError as exc:
        # The error names the path, never the contents.
        raise ConfigError(f"cannot read the {what} secret file {path}") from exc
    secret = data.rstrip(b"\r\n")
    if not secret:
        raise ConfigError(f"the {what} secret file is empty")
    return secret


def load(path: Optional[str]) -> Config:
    config = Config()
    if path:
        parser = configparser.ConfigParser(interpolation=None)
        try:
            with open(path, "r", encoding="utf-8") as handle:
                parser.read_file(handle)
        except (OSError, configparser.Error) as exc:
            raise ConfigError(f"cannot read {path}: {exc}") from exc
        for section in parser.sections():
            if section not in _SECTIONS:
                raise ConfigError(f"unknown section [{section}]")
            for key, value in parser.items(section):
                if key not in _SECTIONS[section]:
                    raise ConfigError(f"unknown key {key} in [{section}]")
                apply(config, key, value)
    if config.turn_secret_file:
        config.turn_secret = read_secret(config.turn_secret_file)
    if config.relay_secret_file:
        config.relay_secret = read_secret(config.relay_secret_file, "relay")
    check(config)
    return config


# Zero turns the WebSocket ping off, and leaves the kernel's own socket
# buffer sizing; every other number must be at least 1, because 0 would make
# the service refuse everything, or nothing.
_MAY_BE_ZERO = ("ping_interval_seconds", "ping_timeout_seconds", "socket_buffer_bytes")


def check(config: Config) -> None:
    """URLs must fit the wire (rendezvous document section 5.2), and every
    limit, timeout and cap must leave the service usable."""
    if type(config.relay_watch_version) is not int or config.relay_watch_version not in (0, 1):
        raise ConfigError("relay_watch_version: 0 (disabled) or 1")
    for key in _SECTIONS["limits"] + ["turn_ttl_seconds", "relay_ttl_seconds"]:
        value = getattr(config, key)
        if not isinstance(value, int) or isinstance(value, bool):
            raise ConfigError(f"{key}: not a whole number")
        if value < 0 or (value == 0 and key not in _MAY_BE_ZERO):
            raise ConfigError(f"{key}: must be at least 1" if key not in _MAY_BE_ZERO else f"{key}: negative")
    if config.send_budget_bytes < config.send_queue_bytes:
        raise ConfigError("send_budget_bytes: smaller than send_queue_bytes")
    for name in ("stun_urls", "turn_urls"):
        urls = getattr(config, name)
        if len(urls) > 8:
            raise ConfigError(f"{name}: at most 8")
        for url in urls:
            if not 1 <= len(url.encode("utf-8")) <= 512:
                raise ConfigError(f"{name}: a URL longer than 512 bytes")
            # Printable ASCII without a quote or a backslash, so a URL is
            # never escaped on the wire and its length is exactly known.
            if not all(0x21 <= ord(c) <= 0x7E and c not in '"\\' for c in url):
                raise ConfigError(f"{name}: a URL must be printable ASCII without quotes or backslashes")
    # Section 2: the service never sends more than 132096 bytes. The
    # largest message it builds is an answer carrying a turn object, whose
    # URL list is the only part set by configuration; this cap keeps it
    # within that bound.
    if len(json.dumps(config.turn_urls, separators=(",", ":"))) > TURN_URLS_JSON_MAX:
        raise ConfigError(f"turn_urls: more than {TURN_URLS_JSON_MAX} bytes together")
    # Section 12.1: a wss URL of printable ASCII, at most 512 bytes. The
    # grant travels in a message of its own, so it takes nothing from the
    # answer's size bound (section 2).
    url = config.relay_url
    if (
        not 1 <= len(url.encode("utf-8")) <= 512
        or not url.startswith("wss://")
        or not all(0x21 <= ord(c) <= 0x7E for c in url)
    ):
        raise ConfigError("relay_url: a wss:// URL of printable ASCII, at most 512 bytes")
    # Two secrets, never one: coturn's is written into its configuration,
    # and a relay token must not be forgeable by anyone who reads that.
    if config.relay_secret is not None and config.relay_secret == config.turn_secret:
        raise ConfigError("the relay secret must differ from the TURN secret")
    if config.relay_ttl_seconds > 86400:
        raise ConfigError("relay_ttl_seconds: at most 86400")
    if config.log_level.upper() not in ("DEBUG", "INFO", "WARNING", "ERROR"):
        raise ConfigError("log_level: debug, info, warning or error")


def apply(config: Config, key: str, value: str) -> None:
    fields = {f.name: f for f in dataclasses.fields(Config)}
    if key == "listen":
        config.listen = parse_listen(value)
    elif key in ("trusted_proxies", "stun_urls", "turn_urls"):
        setattr(config, key, value.split())
    elif key in ("turn_secret_file", "relay_secret_file", "relay_url", "log_level"):
        setattr(config, key, value.strip())
    elif key in fields:
        try:
            number = int(value)
        except ValueError as exc:
            raise ConfigError(f"{key}: not a whole number") from exc
        if number < 0:
            raise ConfigError(f"{key}: negative")
        setattr(config, key, number)
    else:
        raise ConfigError(f"unknown key {key}")
