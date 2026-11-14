# no-port-check: NereusSDR-original.
"""Configuration: one INI file read at start.

Two sections, [roundtable] and [limits]. Every key is optional and the
defaults are nereussdr.com's; an unknown section or key refuses to start, so
a typo never silently leaves a default in force. The server never writes
this file. The [limits] values are wire spec 9.1's; the first ten are sent
to every station in `welcome`.
"""

from __future__ import annotations

import configparser
import ipaddress
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from . import protocol


class ConfigError(Exception):
    pass


@dataclass
class Config:
    # [roundtable]
    # Loopback only: Caddy terminates TLS and forwards by path (wire spec
    # 2.1). IPv6 addresses go in brackets.
    ws_listen: List[str] = field(default_factory=lambda: ["127.0.0.1:8720", "[::1]:8720"])
    http_listen: List[str] = field(default_factory=lambda: ["127.0.0.1:8721", "[::1]:8721"])
    # The host name stations sign in `hello` (wire spec 4.3) and links name.
    public_host: str = "roundtable.nereussdr.com"
    database: str = "/var/lib/nereus-roundtable/roundtable.db"
    backup_dir: str = "/var/lib/nereus-roundtable/backup"
    static_dir: str = "/opt/nereus-roundtable/static"
    # Station ids of the Roundtable helpers (design 11.7).
    helpers: List[str] = field(default_factory=list)
    # Peers whose X-Forwarded-For is believed (wire spec 9.1).
    trusted_proxies: List[str] = field(default_factory=lambda: ["127.0.0.1", "::1"])
    log_level: str = "info"
    # [limits], per station unless the name says otherwise (wire spec 9.1).
    readings_per_second: int = 2
    presence_per_second: int = 1
    messages_per_minute: int = 6
    messages_per_hour: int = 60
    message_code_points: int = 500
    message_lines: int = 10
    names_per_minute: int = 10
    reports_per_hour: int = 10
    slices: int = 4
    reads: int = 4
    station_connections_per_group: int = 8
    keys_per_group_per_day: int = 3
    api_requests_per_minute: int = 60
    live_sockets_per_group: int = 4
    live_sockets_total: int = 500
    watches_per_live_socket: int = 4
    inbound_message_bytes: int = protocol.MAX_INBOUND_BYTES
    outbound_queue_messages: int = 1024
    outbound_queue_bytes: int = 4194304
    # Timers (wire spec 9.2). 0 turns the ping off (the tests do).
    hello_timeout_ms: int = 10000
    ping_interval_ms: int = 20000
    silence_close_ms: int = 45000


ROUNDTABLE_KEYS = (
    "ws_listen",
    "http_listen",
    "public_host",
    "database",
    "backup_dir",
    "static_dir",
    "helpers",
    "trusted_proxies",
    "log_level",
)
LIMIT_FIELDS = (
    "readings_per_second",
    "presence_per_second",
    "messages_per_minute",
    "messages_per_hour",
    "message_code_points",
    "message_lines",
    "names_per_minute",
    "reports_per_hour",
    "slices",
    "reads",
    "station_connections_per_group",
    "keys_per_group_per_day",
    "api_requests_per_minute",
    "live_sockets_per_group",
    "live_sockets_total",
    "watches_per_live_socket",
    "inbound_message_bytes",
    "outbound_queue_messages",
    "outbound_queue_bytes",
    "hello_timeout_ms",
    "ping_interval_ms",
    "silence_close_ms",
)
_SECTIONS = {"roundtable": ROUNDTABLE_KEYS, "limits": LIMIT_FIELDS}

# The `limits` object of `welcome` (wire spec 5.2), from the fields above.
WIRE_LIMITS: Dict[str, str] = {
    "readingsPerSecond": "readings_per_second",
    "presencePerSecond": "presence_per_second",
    "messagesPerMinute": "messages_per_minute",
    "messagesPerHour": "messages_per_hour",
    "messageCodePoints": "message_code_points",
    "messageLines": "message_lines",
    "namesPerMinute": "names_per_minute",
    "reportsPerHour": "reports_per_hour",
    "slices": "slices",
    "reads": "reads",
}

_MAY_BE_ZERO = ("ping_interval_ms",)
_HOST = re.compile(r"\A(?=.{1,253}\Z)[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)*\Z")


def parse_listen(text: str) -> Tuple[str, int]:
    """`127.0.0.1:8720` or `[::1]:8720`."""
    item = text.strip()
    if item.startswith("["):
        host, sep, port_text = item[1:].partition("]:")
    else:
        host, sep, port_text = item.rpartition(":")
    if not sep or not host:
        raise ConfigError(f"listen: bad address {item!r}")
    try:
        port = int(port_text)
    except ValueError as exc:
        raise ConfigError(f"listen: bad port in {item!r}") from exc
    if not 0 <= port <= 65535:
        raise ConfigError(f"listen: bad port in {item!r}")
    try:
        ipaddress.ip_address(host)
    except ValueError as exc:
        raise ConfigError(f"listen: {host!r} is not an IP address") from exc
    return host, port


def wire_limits(config: Config) -> Dict[str, int]:
    return {key: getattr(config, attr) for key, attr in WIRE_LIMITS.items()}


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
    check(config)
    return config


def apply(config: Config, key: str, value: str) -> None:
    if key in ("ws_listen", "http_listen", "helpers", "trusted_proxies"):
        setattr(config, key, value.split())
    elif key in ("public_host", "database", "backup_dir", "static_dir", "log_level"):
        setattr(config, key, value.strip())
    elif key in LIMIT_FIELDS:
        try:
            number = int(value)
        except ValueError as exc:
            raise ConfigError(f"{key}: not a whole number") from exc
        setattr(config, key, number)
    else:  # pragma: no cover - the section tables name every key
        raise ConfigError(f"unknown key {key}")


def check(config: Config) -> None:
    """Every address must parse, the host name must be what a station would
    sign, and every limit must leave the server usable."""
    for key in ("ws_listen", "http_listen"):
        addresses = getattr(config, key)
        if not addresses:
            raise ConfigError(f"{key}: no address")
        for item in addresses:
            parse_listen(item)
    for proxy in config.trusted_proxies:
        try:
            ipaddress.ip_address(proxy)
        except ValueError as exc:
            raise ConfigError(f"trusted_proxies: {proxy!r} is not an IP address") from exc
    # Wire spec 4.3: lower case, no port, no trailing dot.
    if not _HOST.match(config.public_host):
        raise ConfigError("public_host: a lower-case host name with no port and no trailing dot")
    for helper in config.helpers:
        if not re.match(r"\A[a-z2-7]{26}\Z", helper):
            raise ConfigError("helpers: each is a 26-character station id")
    for key in ("database", "backup_dir", "static_dir"):
        if not getattr(config, key):
            raise ConfigError(f"{key}: empty")
    for key in LIMIT_FIELDS:
        value = getattr(config, key)
        if type(value) is not int:
            raise ConfigError(f"{key}: not a whole number")
        if value < 0 or (value == 0 and key not in _MAY_BE_ZERO):
            raise ConfigError(f"{key}: must be at least 1" if key not in _MAY_BE_ZERO else f"{key}: negative")
    if config.slices > protocol.MAX_SLICES:
        raise ConfigError(f"slices: at most {protocol.MAX_SLICES}")
    if config.inbound_message_bytes > protocol.MAX_INBOUND_BYTES:
        raise ConfigError(f"inbound_message_bytes: at most {protocol.MAX_INBOUND_BYTES}")
    if config.log_level.upper() not in ("DEBUG", "INFO", "WARNING", "ERROR"):
        raise ConfigError("log_level: debug, info, warning or error")
