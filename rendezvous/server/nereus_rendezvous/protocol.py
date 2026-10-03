# no-port-check: NereusSDR-original.
"""The rendezvous wire: message kinds, their keys, decoding and encoding.

docs/architecture/2026-09-23-rendezvous-v1.md sections 5 and 6 are the
authority; this module is the service's reading of them. Every message is
one JSON object in one WebSocket text frame with a string `type`. A decoder
checks each key's kind and length; the checks that need cryptography (a
canonical key, an id that matches its key, a signature) happen after
decoding, in the service.
"""

from __future__ import annotations

import json
import re
from typing import Any, Dict, List, Optional, Tuple

from . import identity

VERSION = 1
# Optional watch-grant keys are offered only by a service advertising v2.
WATCH_PROTOCOL_VERSION = 2

# Section 5.1: caps. The transport refuses a message longer than
# MAX_MESSAGE_BYTES before it is decoded; the field caps are in UTF-8 bytes
# of the decoded string, and a sender must also keep the whole encoded
# message within MAX_MESSAGE_BYTES (section 2). A station or client accepts
# messages from the service of up to PEER_RECEIVE_BYTES; the service never
# sends one longer than MAX_SERVICE_MESSAGE_BYTES.
MAX_MESSAGE_BYTES = 131072
MAX_SERVICE_MESSAGE_BYTES = 132096
PEER_RECEIVE_BYTES = 262144
MAX_SDP_BYTES = 65536
MAX_CANDIDATE_BYTES = 4096
MAX_BODY_BYTES = 65536
MAX_REASON_BYTES = 1024
MAX_URL_BYTES = 512
MAX_URLS = 8
MAX_TURN_USERNAME_BYTES = 512
MAX_TURN_PASSWORD_BYTES = 128
NAMEPLATE_MIN = 1
NAMEPLATE_MAX = 999999
RETRY_MAX_MS = 2147483647
EXPIRES_MAX = 4294967295
# Section 12.1: a relay grant's URL and token.
MAX_RELAY_URL_BYTES = 512
MAX_RELAY_TOKEN_BYTES = 512
RELAY_URL_PREFIX = "wss://"
VERSION_MAX = 65535

_CODE_PATTERN = re.compile(r"\A[A-Za-z]{1,64}\Z")
_TOKEN_ALPHABET = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")

# Field kinds. Each is a tuple whose first element names the kind.
RID = ("rid",)
BOOL = ("bool",)
CODE = ("code",)
URLS = ("urls",)
TURN_OR_NULL = ("turn",)
RELAY_URL = ("relayUrl",)
RELAY_TOKEN = ("relayToken",)


def B64(n: int) -> Tuple[str, int]:
    return ("b64", n)


def UTF8(lo: int, hi: int) -> Tuple[str, int, int]:
    return ("utf8", lo, hi)


def INT(lo: int, hi: int) -> Tuple[str, int, int]:
    return ("int", lo, hi)


SDP = UTF8(1, MAX_SDP_BYTES)
# Section 5.2: the empty string (the end of candidates), or one candidate
# attribute value as RFC 8839 writes it, starting "candidate:" and never
# with the "a=" of an SDP line.
CANDIDATE = ("candidate",)
CANDIDATE_PREFIX = "candidate:"
BODY = UTF8(1, MAX_BODY_BYTES)
NAMEPLATE = INT(NAMEPLATE_MIN, NAMEPLATE_MAX)
INTRO_ID = B64(identity.INTRODUCTION_ID_BYTES)
NONCE = B64(identity.NONCE_BYTES)

# Sender -> kind -> ordered keys. The order is the order the service
# encodes them in; a decoder ignores order.
FROM_STATION: Dict[str, List[Tuple[str, tuple]]] = {
    "register": [("id", RID), ("publicKey", B64(identity.SPKI_BYTES))],
    "prove": [("signature", B64(identity.SIGNATURE_BYTES))],
    "answer": [("to", INTRO_ID), ("answer", SDP), ("turn", BOOL)],
    "candidate": [("to", INTRO_ID), ("candidate", CANDIDATE)],
    "nameplate.claim": [],
    "nameplate.release": [],
    "mailbox": [("body", BODY)],
    "mailbox.close": [],
}

FROM_CLIENT: Dict[str, List[Tuple[str, tuple]]] = {
    "introduce": [
        ("id", RID),
        ("device", B64(identity.FINGERPRINT_BYTES)),
        ("deviceSignature", B64(identity.SIGNATURE_BYTES)),
        ("offer", SDP),
    ],
    "candidate": [("candidate", CANDIDATE)],
    "mailbox.open": [("nameplate", NAMEPLATE)],
    "mailbox": [("body", BODY)],
    "mailbox.close": [],
}

_HELLO = [("version", INT(1, VERSION_MAX)), ("nonce", NONCE), ("stun", URLS)]
_ERROR = [("code", CODE), ("reason", UTF8(1, MAX_REASON_BYTES)), ("retryAfterMs", INT(0, RETRY_MAX_MS))]

TO_STATION: Dict[str, List[Tuple[str, tuple]]] = {
    "hello": _HELLO,
    "challenge": [("nonce", NONCE)],
    "registered": [("id", RID)],
    "introduction": [
        ("from", INTRO_ID),
        ("device", B64(identity.FINGERPRINT_BYTES)),
        ("deviceSignature", B64(identity.SIGNATURE_BYTES)),
        ("offer", SDP),
        ("nonce", NONCE),
    ],
    "credentials": [("from", INTRO_ID), ("turn", TURN_OR_NULL)],
    # Section 12.1: after credentials, when the service holds a relay secret.
    "relay.grant": [("from", INTRO_ID), ("url", RELAY_URL), ("token", RELAY_TOKEN), ("expires", INT(0, EXPIRES_MAX))],
    "candidate": [("from", INTRO_ID), ("candidate", CANDIDATE)],
    "introduction.end": [("from", INTRO_ID), ("code", CODE)],
    "nameplate": [("nameplate", NAMEPLATE)],
    "nameplate.released": [],
    "mailbox.opened": [("nameplate", NAMEPLATE)],
    "mailbox": [("body", BODY)],
    "mailbox.closed": [("code", CODE)],
    "error": _ERROR,
}

TO_CLIENT: Dict[str, List[Tuple[str, tuple]]] = {
    "hello": _HELLO,
    "answer": [("answer", SDP), ("turn", TURN_OR_NULL)],
    # Section 12.1: after answer, when the service holds a relay secret.
    "relay.grant": [("url", RELAY_URL), ("token", RELAY_TOKEN), ("expires", INT(0, EXPIRES_MAX))],
    "candidate": [("candidate", CANDIDATE)],
    "introduction.end": [("code", CODE)],
    "mailbox.opened": [("nameplate", NAMEPLATE)],
    "mailbox": [("body", BODY)],
    "mailbox.closed": [("code", CODE)],
    "error": _ERROR,
}

_TURN_KEYS = [
    ("username", UTF8(1, MAX_TURN_USERNAME_BYTES)),
    ("password", UTF8(1, MAX_TURN_PASSWORD_BYTES)),
    ("expires", INT(0, EXPIRES_MAX)),
    ("urls", URLS),
]

TABLES = {
    ("station", "server"): FROM_STATION,
    ("client", "server"): FROM_CLIENT,
    ("server", "station"): TO_STATION,
    ("server", "client"): TO_CLIENT,
}

# Optional negotiated extensions: unknown keys remain ignored, but known
# optional keys are validated when present. Never add them to old peers' frames
# merely because this service understands them.
OPTIONAL_FIELDS = {
    ("station", "server", "register"): [("watchRelayVersion", INT(1, VERSION_MAX))],
    ("client", "server", "introduce"): [("watchRelayVersion", INT(1, VERSION_MAX))],
    ("server", "station", "hello"): [("watchRelayVersion", INT(1, VERSION_MAX))],
    ("server", "client", "hello"): [("watchRelayVersion", INT(1, VERSION_MAX))],
    ("server", "station", "relay.grant"): [("watchToken", RELAY_TOKEN)],
    ("server", "client", "relay.grant"): [("watchToken", RELAY_TOKEN)],
}


class DecodeError(Exception):
    """A message that is not what its kind says it must be."""


def _utf8_len(text: str) -> Optional[int]:
    try:
        return len(text.encode("utf-8"))
    except UnicodeEncodeError:
        # A lone surrogate (a "\\ud800" escape) is not a Unicode scalar
        # value and cannot travel as UTF-8.
        return None


def _is_int(value: Any) -> bool:
    # JSON true and false are not numbers, though Python's bool is an int.
    return isinstance(value, int) and not isinstance(value, bool)


def _check(key: str, value: Any, kind: tuple) -> None:
    name = kind[0]
    if name == "rid":
        if not identity.is_rendezvous_id(value):
            raise DecodeError(f"{key}: not a rendezvous id")
    elif name == "b64":
        if identity.b64url_of_length(value, kind[1]) is None:
            raise DecodeError(f"{key}: not base64url of {kind[1]} bytes")
    elif name == "utf8":
        if not isinstance(value, str):
            raise DecodeError(f"{key}: not a string")
        n = _utf8_len(value)
        if n is None or n < kind[1] or n > kind[2]:
            raise DecodeError(f"{key}: length out of range")
    elif name == "candidate":
        _check(key, value, UTF8(0, MAX_CANDIDATE_BYTES))
        if value and not value.startswith(CANDIDATE_PREFIX):
            raise DecodeError(f"{key}: not a candidate attribute value")
    elif name == "int":
        if not _is_int(value) or value < kind[1] or value > kind[2]:
            raise DecodeError(f"{key}: not a whole number in range")
    elif name == "bool":
        if not isinstance(value, bool):
            raise DecodeError(f"{key}: not true or false")
    elif name == "code":
        if not isinstance(value, str) or not _CODE_PATTERN.match(value):
            raise DecodeError(f"{key}: not a code")
    elif name == "urls":
        if not isinstance(value, list) or len(value) > MAX_URLS:
            raise DecodeError(f"{key}: not a list of at most {MAX_URLS} URLs")
        for url in value:
            _check(key, url, UTF8(1, MAX_URL_BYTES))
    elif name == "relayUrl":
        _check(key, value, UTF8(1, MAX_RELAY_URL_BYTES))
        if not value.startswith(RELAY_URL_PREFIX) or not all(0x21 <= ord(c) <= 0x7E for c in value):
            raise DecodeError(f"{key}: not a wss URL")
    elif name == "relayToken":
        _check(key, value, UTF8(1, MAX_RELAY_TOKEN_BYTES))
        if not all(c in _TOKEN_ALPHABET for c in value):
            raise DecodeError(f"{key}: not base64url characters")
    elif name == "turn":
        if value is None:
            return
        if not isinstance(value, dict):
            raise DecodeError(f"{key}: not an object or null")
        for sub, subkind in _TURN_KEYS:
            if sub not in value:
                raise DecodeError(f"{key}.{sub}: missing")
            _check(f"{key}.{sub}", value[sub], subkind)
    else:  # pragma: no cover - a table error
        raise AssertionError(name)


def _reject_constant(text: str) -> Any:
    raise DecodeError(f"{text} is not JSON")


def _no_duplicates(pairs: List[Tuple[str, Any]]) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    for k, v in pairs:
        if k in out:
            raise DecodeError(f"duplicate key {k}")
        out[k] = v
    return out


def parse_text(text: str) -> Dict[str, Any]:
    """Parse one text frame as a JSON object with a string `type`. Refuses
    NaN and Infinity, duplicate keys, and anything but an object."""
    try:
        obj = json.loads(text, parse_constant=_reject_constant, object_pairs_hook=_no_duplicates)
    except DecodeError:
        raise
    except (ValueError, RecursionError) as exc:
        raise DecodeError("not JSON") from exc
    if not isinstance(obj, dict):
        raise DecodeError("not an object")
    if not isinstance(obj.get("type"), str):
        raise DecodeError("no string type")
    return obj


def decode(obj: Dict[str, Any], sender: str, receiver: str) -> Dict[str, Any]:
    """Decode a parsed message travelling from `sender` to `receiver`.
    Returns the message with exactly its kind's keys (unknown keys are
    dropped, never forwarded). Raises DecodeError for an unknown kind, a
    missing key, or a key of the wrong kind or length."""
    table = TABLES[(sender, receiver)]
    kind = obj.get("type")
    if not isinstance(kind, str) or kind not in table:
        raise DecodeError(f"unknown kind {kind!r}")
    out: Dict[str, Any] = {"type": kind}
    for key, fkind in table[kind]:
        if key not in obj:
            raise DecodeError(f"{key}: missing")
        _check(key, obj[key], fkind)
        value = obj[key]
        if fkind is TURN_OR_NULL and value is not None:
            value = {sub: value[sub] for sub, _ in _TURN_KEYS}
        out[key] = value
    for key, fkind in OPTIONAL_FIELDS.get((sender, receiver, kind), []):
        if key in obj:
            _check(key, obj[key], fkind)
            out[key] = obj[key]
    return out


def encode(message: Dict[str, Any]) -> str:
    """Compact JSON, UTF-8 left as is (not escaped), keys in the order the
    message holds them."""
    return json.dumps(message, separators=(",", ":"), ensure_ascii=False, allow_nan=False)


def message(kind: str, receiver: str, **fields: Any) -> Dict[str, Any]:
    """Build a server message with its keys in the document's order."""
    table = TABLES[("server", receiver)]
    out: Dict[str, Any] = {"type": kind}
    for key, _ in table[kind]:
        out[key] = fields[key]
    for key, fkind in OPTIONAL_FIELDS.get(("server", receiver, kind), []):
        if key in fields:
            _check(key, fields[key], fkind)
            out[key] = fields[key]
    return out


def turn_object(username: str, password: str, expires: int, urls: List[str]) -> Dict[str, Any]:
    return {"username": username, "password": password, "expires": expires, "urls": list(urls)}


# Section 7: every error, its words and whether the connection closes.
# (code, closes, words, default retryAfterMs)
ERRORS: Dict[str, Tuple[bool, str, int]] = {
    "protocolError": (
        True,
        "A message could not be read, so the connection was closed. Updating the app or the Core may help.",
        0,
    ),
    "proofFailed": (True, "The Core could not prove its identity, so it was not registered.", 0),
    "replaced": (True, "The Core registered again on another connection, so this one was closed.", 0),
    "timeout": (True, "The connection did not finish starting in time.", 0),
    "idle": (True, "The connection was closed because it was not being used.", 0),
    "tooManyConnections": (True, "Too many connections are open from this network. Try again shortly.", 5000),
    "overloaded": (True, "The remote access service is busy. Try again shortly.", 5000),
    "shuttingDown": (True, "The remote access service is restarting. Try again shortly.", 5000),
    "offline": (
        False,
        "The Core is not reachable right now. Check that it is running and connected to the internet.",
        0,
    ),
    "rateLimited": (False, "Too many attempts. Try again in a minute.", 0),
    "noIntroduction": (False, "There is no connection attempt in progress.", 0),
    "unknownIntroduction": (False, "That connection attempt has already ended.", 0),
    "tooManyCandidates": (False, "Too many network addresses were offered for one connection attempt.", 0),
    "nameplateUnknown": (
        False,
        "No Core is showing that pairing code right now. Check the code and try again.",
        0,
    ),
    "nameplateBusy": (False, "Another device is pairing with this Core right now. Try again shortly.", 5000),
    "nameplatesExhausted": (False, "Pairing codes are not available right now. Try again shortly.", 5000),
    "noMailbox": (False, "There is no pairing in progress on this connection.", 0),
    "tooManyMessages": (False, "Too many pairing messages were sent.", 0),
}


def error_message(code: str, receiver: str, retry_after_ms: Optional[int] = None) -> Dict[str, Any]:
    closes, words, default_retry = ERRORS[code]
    retry = default_retry if retry_after_ms is None else retry_after_ms
    return message("error", receiver, code=code, reason=words, retryAfterMs=retry)


def error_closes(code: str) -> bool:
    return ERRORS[code][0]
