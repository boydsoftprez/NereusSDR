# no-port-check: NereusSDR-original.
"""The relay's configuration: one INI file read at start, and the relay
secret from a file of its own (systemd LoadCredential=), the same bytes
the rendezvous service mints grants with. The relay never writes either
file. Rendezvous document section 12.5 explains each limit.
"""

from __future__ import annotations

import configparser
import dataclasses
from dataclasses import dataclass, field
from typing import Optional

# Section 12.3: one relay message is a tag byte and at most 1500 bytes.
MAX_MESSAGE_BYTES = 1501


class ConfigError(Exception):
    pass


@dataclass
class Config:
    # [relay]
    # A Unix stream socket, Caddy its only peer (section 12.5): its buffers
    # are the relay's own, and the file's mode and group decide who may
    # connect. An empty group leaves the file's group as it is.
    socket: str = "/run/nereus-relay/relay.sock"
    socket_mode: int = 0o660
    socket_group: str = "caddy"
    relay_secret_file: str = ""
    log_level: str = "info"
    # [limits]
    slots: int = 16
    # Four current paths, four simultaneous reconnect introductions, and
    # one fifth-device confirmation path. Core admission still limits the
    # admitted devices to four; these are transport sessions, not places.
    sessions_per_station: int = 9
    # Both primary and watch ends of all nine paths may share one address.
    connections_per_address: int = 36
    max_pending: int = 64
    join_timeout_ms: int = 10000
    rejoin_ms: int = 30000
    idle_timeout_ms: int = 30000
    rate_bytes_per_second: int = 80000
    queue_frames: int = 64
    queue_bytes: int = 24576
    watch_rate_bytes_per_second: int = 8000
    watch_queue_frames: int = 16
    watch_queue_bytes: int = 8192
    send_stall_ms: int = 10000
    ping_interval_seconds: int = 20
    ping_timeout_seconds: int = 20
    socket_buffer_bytes: int = 16384
    # Not from the file: the secret's bytes, read from relay_secret_file.
    relay_secret: Optional[bytes] = field(default=None, repr=False)


_SECTIONS = {
    "relay": ["socket", "socket_mode", "socket_group", "relay_secret_file", "log_level"],
    "limits": [
        "slots",
        "sessions_per_station",
        "connections_per_address",
        "max_pending",
        "join_timeout_ms",
        "rejoin_ms",
        "idle_timeout_ms",
        "rate_bytes_per_second",
        "queue_frames",
        "queue_bytes",
        "watch_rate_bytes_per_second",
        "watch_queue_frames",
        "watch_queue_bytes",
        "send_stall_ms",
        "ping_interval_seconds",
        "ping_timeout_seconds",
        "socket_buffer_bytes",
    ],
}

# Zero turns the WebSocket ping off and leaves the kernel's own socket
# buffer sizing; every other number must be at least 1.
_MAY_BE_ZERO = ("ping_interval_seconds", "ping_timeout_seconds", "socket_buffer_bytes")


def read_secret(path: str) -> bytes:
    try:
        with open(path, "rb") as handle:
            data = handle.read()
    except OSError as exc:
        # The error names the path, never the contents.
        raise ConfigError(f"cannot read the relay secret file {path}") from exc
    secret = data.rstrip(b"\r\n")
    if not secret:
        raise ConfigError("the relay secret file is empty")
    return secret


def check(config: Config) -> None:
    for key in _SECTIONS["limits"]:
        value = getattr(config, key)
        if not isinstance(value, int) or isinstance(value, bool):
            raise ConfigError(f"{key}: not a whole number")
        if value < 0 or (value == 0 and key not in _MAY_BE_ZERO):
            raise ConfigError(f"{key}: must be at least 1" if key not in _MAY_BE_ZERO else f"{key}: negative")
    # A queue must hold at least one whole message, or every frame would be
    # dropped on arrival.
    if config.queue_bytes < MAX_MESSAGE_BYTES:
        raise ConfigError(f"queue_bytes: at least {MAX_MESSAGE_BYTES}")
    if config.rate_bytes_per_second < MAX_MESSAGE_BYTES:
        raise ConfigError(f"rate_bytes_per_second: at least {MAX_MESSAGE_BYTES}")
    if config.watch_queue_bytes < MAX_MESSAGE_BYTES:
        raise ConfigError(f"watch_queue_bytes: at least {MAX_MESSAGE_BYTES}")
    if config.watch_rate_bytes_per_second < MAX_MESSAGE_BYTES:
        raise ConfigError(f"watch_rate_bytes_per_second: at least {MAX_MESSAGE_BYTES}")
    if not config.socket.startswith("/") or len(config.socket.encode("utf-8")) > 100:
        raise ConfigError("socket: an absolute path of at most 100 bytes")
    if not isinstance(config.socket_mode, int) or not 0 <= config.socket_mode <= 0o777:
        raise ConfigError("socket_mode: an octal mode such as 0660")
    if config.log_level.upper() not in ("DEBUG", "INFO", "WARNING", "ERROR"):
        raise ConfigError("log_level: debug, info, warning or error")


def apply(config: Config, key: str, value: str) -> None:
    fields = {f.name: f for f in dataclasses.fields(Config)}
    if key == "socket_mode":
        try:
            config.socket_mode = int(value.strip(), 8)
        except ValueError as exc:
            raise ConfigError("socket_mode: an octal mode such as 0660") from exc
    elif key in ("socket", "socket_group", "relay_secret_file", "log_level"):
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
    if not config.relay_secret_file:
        raise ConfigError("relay_secret_file: the relay needs the secret it shares with the rendezvous service")
    config.relay_secret = read_secret(config.relay_secret_file)
    check(config)
    return config
