# no-port-check: NereusSDR-original.
"""The Roundtable wire: message kinds, their keys, decoding and encoding.

docs/architecture/2026-10-23-roundtable-v1.md sections 5 and 8 are the
authority; this module is the server's reading of them. Every message is
one JSON object in one WebSocket text frame with a string `type`. decode()
checks each listed key's field kind (section 5.2) and returns the message
with only the keys its kind lists, at every depth; a key a kind does not
list is ignored. Anything else is a protocol error (section 5.1), raised as
ProtocolError. The checks that need cryptography (a canonical key, a
signature) and the server's state happen after decoding, in the service.
"""

from __future__ import annotations

import base64
import json
import math
import re
import unicodedata
from typing import Any, Callable, Dict, List, Optional, Tuple

VERSION = 1
# 2026-10-30: additive frequency views, J.J. Boyd / KG4VCF, AI-assisted via Codex.
FREQUENCY_VERSION = 1
CHAT_RADIUS_HZ = 5000

# Section 2.2: the inbound cap, enforced by the transport before decoding,
# and the largest message the server ever sends.
MAX_INBOUND_BYTES = 16384
MAX_OUTBOUND_BYTES = 1048576

# Section 2.4: close codes.
CLOSE_NORMAL = 1000
CLOSE_GOING_AWAY = 1001
CLOSE_TOO_BIG = 1009
CLOSE_TRY_LATER = 1013
CLOSE_SIGN_IN = 4001
CLOSE_PROTOCOL = 4002
CLOSE_REPLACED = 4003
CLOSE_REVOKED = 4004
CLOSE_HELLO_TIMEOUT = 4005

MAX_SAFE = 9007199254740991
MAX_HZ = 99999999999
MAX_SLICES = 4

FAMILIES = ("lsb", "usb", "digl", "digu", "am", "fm", "cw", "fdvl", "fdvu")
MODES = ("LSB", "USB", "DIGL", "DIGU", "AM", "SAM", "FM", "CWL", "CWU", "FDVL", "FDVU")
# Section 6.2, in frequency order.
BANDS = (
    "2200m", "630m", "160m", "80m", "60m", "40m", "30m", "20m", "17m",
    "15m", "12m", "10m", "6m", "4m", "2m", "1.25m", "70cm", "33cm", "23cm",
)
STATES = ("talking", "unknown", "doubled", "quiet")
TURN_KINDS = ("talking", "unknown", "doubled")
MESSAGE_STATES = ("shown", "removed", "deleted", "expired")
REPORT_REASONS = {
    "message": ("spam", "abuse", "offTopic", "other"),
    "turn": ("wrongName",),
    "station": ("falseReadings",),
}
# Section 5.3.4 and the keys report adds by target (5.3.7).
REPORT_KEYS = {"message": ("msg",), "turn": ("turn",), "station": ("room", "ref")}

LIMIT_KEYS = (
    "readingsPerSecond",
    "presencePerSecond",
    "messagesPerMinute",
    "messagesPerHour",
    "messageCodePoints",
    "messageLines",
    "namesPerMinute",
    "reportsPerHour",
    "slices",
    "reads",
)


class OptionalField:
    def __init__(self, check):
        self.check = check


def optional(check):
    return OptionalField(check)


class ProtocolError(Exception):
    """A message that is not what its kind says it must be. The connection
    closes with close_code (4002) and no error message (section 5.1)."""

    def __init__(self, close_code: int = CLOSE_PROTOCOL, detail: str = "") -> None:
        super().__init__(detail or "protocol error")
        self.close_code = close_code
        self.detail = detail


def _bad(path: str, what: str) -> ProtocolError:
    return ProtocolError(CLOSE_PROTOCOL, f"{path}: {what}")


# ----------------------------------------------------------------- values

_CALLSIGN_CHARS = re.compile(r"\A[A-Za-z0-9/]{3,16}\Z")
_GRID = re.compile(r"\A[A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2})?\Z")
_ROOM_ID = re.compile(r"\A(" + "|".join(FAMILIES) + r")-([1-9][0-9]{0,10})\Z")
_ROW_REF = re.compile(r"\A[a-z2-7]{8}\Z")
_SID = re.compile(r"\A[a-z2-7]{26}\Z")
_CODE = re.compile(r"\A[a-z-]{1,32}\Z")
_ABOUT = re.compile(r"\A[A-Za-z]{1,32}\Z")
_B64URL = re.compile(r"\A[A-Za-z0-9_-]*\Z")


def is_callsign(text: object) -> bool:
    """Section 4.5: 3 to 16 of A-Z a-z 0-9 /, at least one letter and one
    digit, not starting or ending with / and with no //."""
    if not isinstance(text, str) or not _CALLSIGN_CHARS.match(text):
        return False
    if text.startswith("/") or text.endswith("/") or "//" in text:
        return False
    return any(c.isalpha() for c in text) and any(c.isdigit() for c in text)


def normalize_callsign(text: str) -> str:
    return text.upper()


def is_grid(text: object) -> bool:
    """Section 4.5: a 4 or 6 character Maidenhead locator, in any case."""
    return isinstance(text, str) and bool(_GRID.match(text))


def normalize_grid(text: str) -> str:
    """The field upper case, the subsquare lower case (`EM73tu`)."""
    return text[:2].upper() + text[2:4] + text[4:].lower()


def room_id_center(text: str) -> Optional[Tuple[str, int]]:
    m = _ROOM_ID.match(text) if isinstance(text, str) else None
    if m is None:
        return None
    return m.group(1), int(m.group(2))


def _utf8_len(text: str) -> Optional[int]:
    try:
        return len(text.encode("utf-8"))
    except UnicodeEncodeError:
        # A lone surrogate is not a Unicode scalar value and cannot travel
        # as UTF-8 (section 5.1).
        return None


def _b64_len(text: str) -> Optional[int]:
    """The decoded length of strict base64url (as identity.from_b64url),
    without importing cryptography here."""
    if not _B64URL.match(text) or len(text) % 4 == 1:
        return None
    padded = text + "=" * (-len(text) % 4)
    try:
        data = base64.urlsafe_b64decode(padded.encode("ascii"))
    except (ValueError, TypeError):
        return None
    if base64.urlsafe_b64encode(data).decode("ascii").rstrip("=") != text:
        return None
    return len(data)


# ----------------------------------------------------------------- field kinds
#
# A field kind is a function (value, path) -> decoded value that raises
# ProtocolError. Objects return only the keys they list.

Check = Callable[[Any, str], Any]


class _ExponentNumber(float):
    """A JSON number written with an exponent: not a whole number and not a
    dbm (section 5.1). Only ever seen in a key nothing lists."""


def _whole(lo: int, hi: int) -> Check:
    def check(value: Any, path: str) -> Any:
        # bool is an int in Python; JSON true and false are not numbers.
        if type(value) is not int or value < lo or value > hi:
            raise _bad(path, f"not a whole number from {lo} to {hi}")
        return value

    return check


def _number(lo: float, hi: float) -> Check:
    def check(value: Any, path: str) -> Any:
        if type(value) is int:
            ok = True
        elif type(value) is float:
            ok = math.isfinite(value)
        else:
            ok = False
        if not ok or value < lo or value > hi:
            raise _bad(path, f"not a number from {lo} to {hi}")
        return value

    return check


def _boolean(value: Any, path: str) -> Any:
    if not isinstance(value, bool):
        raise _bad(path, "not true or false")
    return value


def _string(lo: int, hi: int, chars: Optional[Callable[[str], bool]] = None) -> Check:
    def check(value: Any, path: str) -> Any:
        if not isinstance(value, str):
            raise _bad(path, "not a string")
        n = _utf8_len(value)
        if n is None or n < lo or n > hi:
            raise _bad(path, f"not a string of {lo} to {hi} bytes")
        if chars is not None and not chars(value):
            raise _bad(path, "a character not allowed here")
        return value

    return check


def _pattern(regex: "re.Pattern[str]", what: str) -> Check:
    def check(value: Any, path: str) -> Any:
        if not isinstance(value, str) or not regex.match(value):
            raise _bad(path, f"not {what}")
        return value

    return check


def _one_of(values: Tuple[str, ...]) -> Check:
    allowed = frozenset(values)

    def check(value: Any, path: str) -> Any:
        if not isinstance(value, str) or value not in allowed:
            raise _bad(path, f"not one of {', '.join(values)}")
        return value

    return check


def _b64(n: int) -> Check:
    def check(value: Any, path: str) -> Any:
        if not isinstance(value, str) or _b64_len(value) != n:
            raise _bad(path, f"not strict base64url of {n} bytes")
        return value

    return check


def _or_null(kind: Check) -> Check:
    def check(value: Any, path: str) -> Any:
        if value is None:
            return None
        return kind(value, path)

    return check


def _array(kind: Check, lo: int = 0, hi: Optional[int] = None) -> Check:
    def check(value: Any, path: str) -> Any:
        if not isinstance(value, list):
            raise _bad(path, "not an array")
        if len(value) < lo or (hi is not None and len(value) > hi):
            raise _bad(path, "an array of the wrong length")
        return [kind(item, f"{path}[{i}]") for i, item in enumerate(value)]

    return check


Fields = List[Tuple[str, Check]]


def _fields(raw: Dict[str, Any], fields: Fields, path: str) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    for key, kind in fields:
        if isinstance(kind, OptionalField):
            if key in raw:
                out[key] = kind.check(raw[key], f"{path}{key}")
            continue
        if key not in raw:
            raise _bad(f"{path}{key}", "missing")
        out[key] = kind(raw[key], f"{path}{key}")
    return out


def _object(fields: Fields, post: Optional[Callable[[Dict[str, Any], Dict[str, Any], str], None]] = None) -> Check:
    def check(value: Any, path: str) -> Any:
        if not isinstance(value, dict):
            raise _bad(path, "not an object")
        out = _fields(value, fields, path + ".")
        if post is not None:
            post(value, out, path + ".")
        return out

    return check


def _no_control(text: str) -> bool:
    return not any(unicodedata.category(c) == "Cc" for c in text)


def _printable_ascii(text: str) -> bool:
    return all(0x21 <= ord(c) <= 0x7E for c in text)


def _callsign(value: Any, path: str) -> Any:
    if not is_callsign(value):
        raise _bad(path, "not a callsign")
    return value


def _grid(value: Any, path: str) -> Any:
    if not is_grid(value):
        raise _bad(path, "not a grid")
    return value


def _band_or(extra: Tuple[str, ...]) -> Check:
    return _one_of(BANDS + ("other",) + extra)


def _weblink_url(value: Any, path: str) -> Any:
    _string(1, 512, _printable_ascii)(value, path)
    if not value.startswith("https://"):
        raise _bad(path, "not an https:// address")
    return value


VERSION_KIND = _whole(1, 65535)
SID = _pattern(_SID, "a station id")
SPKI = _b64(91)
SIG = _b64(64)
NONCE = _b64(32)
WEBSITE_KEY = _b64(32)
SOFTWARE = _string(1, 64, _no_control)
SOFTWARE_VERSION = _string(1, 32, _no_control)
CALLSIGN = _callsign
GRID = _grid
BOOL = _boolean
SLICE_ID = _whole(0, 255)
HZ = _whole(1, MAX_HZ)
FILTER_HZ = _whole(1, 1000000)
MODE = _one_of(MODES)
FAMILY = _one_of(FAMILIES)
BAND = _band_or(())
ROOM_ID = _pattern(_ROOM_ID, "a room id")
DBM = _number(-160, 94)
UNIX_MS = _whole(0, MAX_SAFE)
ID = _whole(1, MAX_SAFE)
GEN = _whole(1, MAX_SAFE)
COUNT = _whole(0, MAX_SAFE)
NTH = _whole(1, 65535)
KM = _or_null(_whole(0, 20038))
OFFSET_HZ = _whole(-1000000, 1000000)
ROW_REF = _pattern(_ROW_REF, "a row ref")
TEXT = _string(1, 4000)
CLIENT_ID = _string(1, 64, _printable_ascii)
REASON = _string(1, 1024)
CODE = _pattern(_CODE, "a code")
ABOUT = _or_null(_pattern(_ABOUT, "a message type"))
RETRY = _whole(0, 2147483647)
WEBLINK_URL = _weblink_url
STATE = _one_of(STATES)
TURN_KIND = _one_of(TURN_KINDS)
MESSAGE_STATE = _one_of(MESSAGE_STATES)
CONFIRM = _string(1, 64)

# ----------------------------------------------------------------- objects


def _unique_slice_ids(raw: Dict[str, Any], out: Dict[str, Any], path: str) -> None:
    ids = [s["id"] for s in out["slices"]]
    if len(set(ids)) != len(ids):
        raise _bad(f"{path}slices", "a slice id repeated")


SLICE = _object([("id", SLICE_ID), ("hz", HZ), ("mode", MODE), ("filterHz", FILTER_HZ), ("keyed", BOOL)])

def _line_text(raw: Dict[str, Any], out: Dict[str, Any], path: str) -> None:
    # As a message's: `text` is a text exactly when `state` is shown, and a
    # summary's holds at most 80 code points (6.11).
    if "text" not in raw:
        raise _bad(f"{path}text", "missing")
    if out["state"] == "shown":
        out["text"] = TEXT(raw["text"], f"{path}text")
        if len(out["text"]) > 80:
            raise _bad(f"{path}text", "longer than 80 code points")
    elif raw["text"] is not None:
        raise _bad(f"{path}text", "not null for a message that is not shown")
    else:
        out["text"] = None


LAST_MESSAGE = _object(
    [("callsign", CALLSIGN), ("nth", NTH), ("t", UNIX_MS), ("state", MESSAGE_STATE)],
    _line_text,
)

SUMMARY = _object(
    [
        ("id", ROOM_ID),
        ("band", BAND),
        ("family", FAMILY),
        ("hz", HZ),
        ("state", STATE),
        ("talkers", _array(CALLSIGN)),
        ("members", COUNT),
        ("messages", COUNT),
        ("last", UNIX_MS),
        ("folded", BOOL),
        ("start", _or_null(UNIX_MS)),
        ("heard", COUNT),
        ("readings", _array(DBM, 0, 12)),
        ("lastMessage", _or_null(LAST_MESSAGE)),
        ("ended", _or_null(UNIX_MS)),
    ]
)

READING = _object([("dbm", DBM), ("t", UNIX_MS), ("present", BOOL), ("turn", _or_null(ID))])

MEMBER = _object(
    [
        ("ref", ROW_REF),
        ("callsign", CALLSIGN),
        ("nth", NTH),
        ("grid", _or_null(GRID)),
        ("km", KM),
        ("offsetHz", OFFSET_HZ),
        ("joining", BOOL),
        ("away", BOOL),
        ("keyed", BOOL),
        ("software", SOFTWARE),
        ("softwareVersion", SOFTWARE_VERSION),
        ("reading", _or_null(READING)),
        ("you", BOOL),
    ]
)

TALKER = _object([("ref", _or_null(ROW_REF)), ("callsign", CALLSIGN), ("nth", NTH)])
NAME_COUNT = _object([("name", CALLSIGN), ("count", _whole(1, MAX_SAFE))])

TURN = _object(
    [
        ("id", ID),
        ("kind", TURN_KIND),
        ("talkers", _array(TALKER)),
        ("start", UNIX_MS),
        ("end", _or_null(UNIX_MS)),
        ("ending", BOOL),
        ("names", _array(NAME_COUNT)),
        ("heard", COUNT),
        ("mine", _or_null(CALLSIGN)),
    ]
)


def _message_text(raw: Dict[str, Any], out: Dict[str, Any], path: str) -> None:
    # `text` is a text exactly when `state` is shown, otherwise null.
    if "text" not in raw:
        raise _bad(f"{path}text", "missing")
    if out["state"] == "shown":
        out["text"] = TEXT(raw["text"], f"{path}text")
    elif raw["text"] is not None:
        raise _bad(f"{path}text", "not null for a message that is not shown")
    else:
        out["text"] = None
    if "dbm" not in raw:
        raise _bad(f"{path}dbm", "missing")
    out["dbm"] = _or_null(DBM)(raw["dbm"], f"{path}dbm")


MESSAGE = _object(
    [
        ("id", ID),
        ("ref", _or_null(ROW_REF)),
        ("callsign", CALLSIGN),
        ("nth", NTH),
        ("t", UNIX_MS),
        ("state", MESSAGE_STATE),
    ],
    _message_text,
)

# Extensions preserve missing fields and old v1 message bytes.
def extended(check, fields):
    def apply(raw, path):
        out = check(raw, path)
        out.update(_fields(raw, [(key, optional(kind)) for key, kind in fields], path + "."))
        return out
    return apply


FREQUENCY = _whole(1, 1)
BEARING = _or_null(_whole(0, 359))
STATION_PRESENCE = _object([
    ("ref", ROW_REF), ("slice", SLICE_ID), ("sourceRoom", ROOM_ID),
    ("hz", HZ), ("family", FAMILY), ("callsign", CALLSIGN), ("nth", NTH),
    ("grid", _or_null(GRID)), ("keyed", BOOL), ("away", BOOL), ("joining", BOOL),
])
SUMMARY = extended(SUMMARY, [("stations", _array(STATION_PRESENCE))])
MEMBER = extended(MEMBER, [("hz", HZ), ("sourceRoom", ROOM_ID), ("slice", SLICE_ID), ("bearingDeg", BEARING)])
TALKER = extended(TALKER, [("hz", _or_null(HZ)), ("grid", _or_null(GRID)), ("sourceRoom", ROOM_ID)])
# Rebuild only the nested talker decoder, preserving all v1 turn fields.
TURN = _object([
    ("id", ID), ("kind", TURN_KIND), ("talkers", _array(TALKER)),
    ("start", UNIX_MS), ("end", _or_null(UNIX_MS)), ("ending", BOOL),
    ("names", _array(NAME_COUNT)), ("heard", COUNT), ("mine", _or_null(CALLSIGN)),
    ("hz", optional(HZ)), ("sourceRoom", optional(ROOM_ID)),
])
MESSAGE = extended(MESSAGE, [("hz", _or_null(HZ)), ("family", _or_null(FAMILY)), ("sourceRoom", ROOM_ID)])

MENTION_FIELDS: Fields = [("room", ROOM_ID), ("message", MESSAGE)]
MENTION = _object(MENTION_FIELDS)

LIMITS = _object([(key, COUNT) for key in LIMIT_KEYS])

# ----------------------------------------------------------------- kinds


class Kind:
    """One message kind: its listed keys in the order the server writes
    them, and an optional check of the keys that depend on each other."""

    def __init__(self, fields: Fields, post: Optional[Callable[[Dict[str, Any], Dict[str, Any], Dict[str, Any]], None]] = None) -> None:
        self.fields = fields
        self.post = post


def _hello_keys(raw: Dict[str, Any], out: Dict[str, Any], ctx: Dict[str, Any]) -> None:
    """Section 5.3.1: either spki with sig, or key; anything else is a
    protocol error."""
    if "frequencyVersion" in raw:
        out["frequencyVersion"] = FREQUENCY(raw["frequencyVersion"], "frequencyVersion")
    has_spki, has_sig, has_key = "spki" in raw, "sig" in raw, "key" in raw
    if has_spki and has_sig and not has_key:
        out["spki"] = SPKI(raw["spki"], "spki")
        out["sig"] = SIG(raw["sig"], "sig")
    elif has_key and not has_spki and not has_sig:
        out["key"] = WEBSITE_KEY(raw["key"], "key")
    else:
        raise _bad("hello", "needs spki with sig, or key")


def _presence_slices(raw: Dict[str, Any], out: Dict[str, Any], ctx: Dict[str, Any]) -> None:
    if "slices" not in raw:
        raise _bad("slices", "missing")
    out["slices"] = _array(SLICE, 0, ctx.get("max_slices", MAX_SLICES))(raw["slices"], "slices")
    _unique_slice_ids(raw, out, "")


def _report_keys(raw: Dict[str, Any], out: Dict[str, Any], ctx: Dict[str, Any]) -> None:
    """Section 5.3.7: the reason must be one of its target's, and a key a
    target adds is present exactly when that is the target."""
    target = out["target"]
    if "reason" not in raw:
        raise _bad("reason", "missing")
    out["reason"] = _or_null(_one_of(REPORT_REASONS[target]))(raw["reason"], "reason")
    kinds = {"msg": ID, "turn": ID, "room": ROOM_ID, "ref": ROW_REF}
    for other, keys in REPORT_KEYS.items():
        for key in keys:
            if other == target:
                if key not in raw:
                    raise _bad(key, "missing")
                out[key] = kinds[key](raw[key], key)
            elif key in raw and key not in REPORT_KEYS[target]:
                raise _bad(key, f"not a key of a {target} report")


KINDS_FROM_STATION: Dict[str, Kind] = {
    "hello": Kind([("v", VERSION_KIND), ("software", SOFTWARE), ("softwareVersion", SOFTWARE_VERSION)], _hello_keys),
    "presence": Kind([("callsign", _or_null(CALLSIGN)), ("grid", _or_null(GRID)), ("atRadio", BOOL)], _presence_slices),
    "reading": Kind([("slice", SLICE_ID), ("dbm", DBM), ("t", UNIX_MS), ("present", BOOL)]),
    "post": Kind([("room", ROOM_ID), ("text", TEXT), ("clientId", CLIENT_ID)]),
    "postAt": Kind([("slice", SLICE_ID), ("hz", HZ), ("family", FAMILY), ("text", TEXT), ("clientId", CLIENT_ID)]),
    "readAt": Kind([("hz", HZ), ("family", FAMILY)]),
    "unreadAt": Kind([("hz", HZ), ("family", FAMILY)]),
    "delete": Kind([("msg", ID)]),
    "name": Kind([("turn", ID), ("name", CALLSIGN)]),
    "report": Kind([("target", _one_of(tuple(REPORT_REASONS)))], _report_keys),
    "read": Kind([("room", ROOM_ID)]),
    "unread": Kind([("room", ROOM_ID), ("view", optional(_one_of(("frequency",))))]),
    "resync": Kind([("room", _or_null(ROOM_ID)), ("view", optional(_one_of(("frequency",))))]),
    "mentionsSeen": Kind([("t", UNIX_MS)]),
    "weblink": Kind([]),
    "deleteArchive": Kind([("confirm", CONFIRM)]),
    "hidden": Kind([]),
}

KINDS_FROM_LIVE: Dict[str, Kind] = {
    "watchBand": Kind([("band", _band_or(("all",))), ("frequencyVersion", optional(FREQUENCY))]),
    "watchAt": Kind([("hz", HZ), ("family", FAMILY)]),
    "unwatchAt": Kind([("hz", HZ), ("family", FAMILY)]),
    "watch": Kind([("room", ROOM_ID)]),
    "unwatch": Kind([("room", ROOM_ID), ("view", optional(_one_of(("frequency",))))]),
    "resync": Kind([("room", _or_null(ROOM_ID)), ("view", optional(_one_of(("frequency",))))]),
}

_ROOMS = Kind([("gen", GEN), ("rooms", _array(SUMMARY)), ("frequencyVersion", optional(FREQUENCY))])
_ROOMS_DELTA = Kind(
    [("gen", GEN), ("add", _array(SUMMARY)), ("change", _array(SUMMARY)), ("remove", _array(ROOM_ID))]
)
_ROOM = Kind(
    [
        ("gen", GEN),
        ("id", ROOM_ID),
        ("opening", ID),
        ("band", BAND),
        ("family", FAMILY),
        ("hz", HZ),
        ("members", _array(MEMBER)),
        ("now", _array(TURN)),
        ("earlier", _array(TURN)),
        ("messages", _array(MESSAGE)),
        ("view", optional(_one_of(("frequency",)))),
    ]
)
_DELTA = Kind(
    [
        ("room", ROOM_ID),
        ("gen", GEN),
        ("members", _array(MEMBER)),
        ("left", _array(_string(1, 64))),
        ("turns", _array(TURN)),
        ("messages", _array(MESSAGE)),
        ("closed", BOOL),
        ("view", optional(_one_of(("frequency",)))),
    ]
)
_ERROR = Kind([("code", CODE), ("reason", REASON), ("about", ABOUT), ("retryAfterMs", RETRY)])

KINDS_TO_STATION: Dict[str, Kind] = {
    "challenge": Kind([("nonce", NONCE)]),
    "welcome": Kind(
        [
            ("station", SID),
            ("kind", _one_of(("core", "key"))),
            ("callsign", _or_null(CALLSIGN)),
            ("grid", _or_null(GRID)),
            ("time", UNIX_MS),
            ("limits", LIMITS),
            ("mentions", _array(MENTION)),
            ("frequencyVersion", optional(FREQUENCY)),
        ]
    ),
    "rooms": _ROOMS,
    "roomsDelta": _ROOMS_DELTA,
    "room": _ROOM,
    "delta": _DELTA,
    "posted": Kind(
        [
            ("clientId", CLIENT_ID),
            ("ok", BOOL),
            ("msg", _or_null(ID)),
            ("code", _or_null(CODE)),
            ("reason", _or_null(REASON)),
        ]
    ),
    "mention": Kind(MENTION_FIELDS),
    "notice": Kind([("code", CODE), ("text", REASON), ("until", _or_null(UNIX_MS))]),
    "weblinkOk": Kind([("url", WEBLINK_URL), ("expires", UNIX_MS)]),
    "error": _ERROR,
}

KINDS_TO_LIVE: Dict[str, Kind] = {
    "rooms": _ROOMS,
    "roomsDelta": _ROOMS_DELTA,
    "room": _ROOM,
    "delta": _DELTA,
    "error": _ERROR,
}

def _history_ids(value: Any, path: str) -> List[int]:
    ids = _array(ID, hi=200)(value, path)
    if len(set(ids)) != len(ids):
        raise _bad(path, "duplicate message IDs")
    return ids


def _history_text(raw: Dict[str, Any], out: Dict[str, Any], path: str) -> None:
    if "text" not in raw:
        raise _bad(path + "text", "missing")
    out["text"] = TEXT(raw["text"], path + "text") if out["state"] == "shown" else None
    if out["state"] != "shown" and raw["text"] is not None:
        raise _bad(path + "text", "not null for a message that is not shown")


_HISTORY_REQUEST = Kind([("clientId", CLIENT_ID), ("ids", _history_ids)])
_HISTORY_STATE = _object([("id", ID), ("state", MESSAGE_STATE)], _history_text)
_HISTORY_REPLY = Kind([("clientId", CLIENT_ID), ("states", _array(_HISTORY_STATE, hi=200))])


# Keep the frozen v1 kind tables intact for old-peer conformance. The
# negotiated extension is tested independently and dispatched by decode.
FREQUENCY_TABLES = {
    ("station", "server"): {key: KINDS_FROM_STATION.pop(key) for key in ("postAt", "readAt", "unreadAt")},
    ("live", "server"): {key: KINDS_FROM_LIVE.pop(key) for key in ("watchAt", "unwatchAt")},
    ("server", "station"): {"history": _HISTORY_REPLY},
    ("server", "live"): {"history": _HISTORY_REPLY},
}
for _sender in ("station", "live"):
    FREQUENCY_TABLES[(_sender, "server")]["history"] = _HISTORY_REQUEST

TABLES: Dict[Tuple[str, str], Dict[str, Kind]] = {
    ("station", "server"): KINDS_FROM_STATION,
    ("live", "server"): KINDS_FROM_LIVE,
    ("server", "station"): KINDS_TO_STATION,
    ("server", "live"): KINDS_TO_LIVE,
}

# ----------------------------------------------------------------- decode


def _reject_constant(text: str) -> Any:
    raise ProtocolError(CLOSE_PROTOCOL, f"{text} is not JSON")


def _parse_float(text: str) -> float:
    if "e" in text or "E" in text:
        return _ExponentNumber(text)
    return float(text)


def _no_duplicates(pairs: List[Tuple[str, Any]]) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    for key, value in pairs:
        if key in out:
            raise ProtocolError(CLOSE_PROTOCOL, f"the key {key!r} appears twice")
        out[key] = value
    return out


def parse(text: str) -> Dict[str, Any]:
    """Parse one text frame as a JSON object with a string `type`. Refuses
    NaN and Infinity, duplicate keys at any depth, and anything but an
    object."""
    if not isinstance(text, str):
        raise ProtocolError(CLOSE_PROTOCOL, "not a text frame")
    try:
        obj = json.loads(
            text,
            parse_constant=_reject_constant,
            parse_float=_parse_float,
            object_pairs_hook=_no_duplicates,
        )
    except ProtocolError:
        raise
    except (ValueError, RecursionError) as exc:
        raise ProtocolError(CLOSE_PROTOCOL, "not JSON") from exc
    if not isinstance(obj, dict):
        raise ProtocolError(CLOSE_PROTOCOL, "not an object")
    if not isinstance(obj.get("type"), str):
        raise ProtocolError(CLOSE_PROTOCOL, "no string type")
    return obj


def decode(text: str, sender: str = "station", receiver: str = "server", max_slices: int = MAX_SLICES) -> Dict[str, Any]:
    """Decode one message travelling from `sender` to `receiver` ("station"
    or "live" to "server", or "server" to "station" or "live"). Returns the
    message with exactly the keys its kind lists, at every depth. Raises
    ProtocolError for anything section 5.1 calls a protocol error: an
    unknown kind, a kind of the other path or direction, a missing key, or
    a key of the wrong field kind."""
    obj = parse(text)
    return decode_object(obj, sender, receiver, max_slices)


def decode_object(obj: Dict[str, Any], sender: str, receiver: str, max_slices: int = MAX_SLICES) -> Dict[str, Any]:
    table = TABLES[(sender, receiver)]
    kind = obj.get("type")
    extension = FREQUENCY_TABLES.get((sender, receiver), {})
    if not isinstance(kind, str) or (kind not in table and kind not in extension):
        raise ProtocolError(CLOSE_PROTOCOL, f"unknown kind {kind!r}")
    spec = table[kind] if kind in table else extension[kind]
    out: Dict[str, Any] = {"type": kind}
    out.update(_fields(obj, spec.fields, ""))
    if spec.post is not None:
        spec.post(obj, out, {"max_slices": max_slices})
    if kind == "delta":
        for ref in out["left"]:
            parts = ref.rsplit("/", 1)
            if out.get("view") == "frequency":
                if len(parts) != 2 or room_id_center(parts[0]) is None:
                    raise _bad("left", "not a sourceRoom/ref")
                ROW_REF(parts[1], "left")
            else:
                ROW_REF(ref, "left")
    return out


def encode(message: Dict[str, Any]) -> str:
    """Compact JSON, non-ASCII written as UTF-8 rather than escaped, keys in
    the order the message holds them (section 2.2)."""
    return json.dumps(message, separators=(",", ":"), ensure_ascii=False, allow_nan=False)


def message(kind: str, receiver: str = "station", /, **fields: Any) -> Dict[str, Any]:
    """A server message with its keys in the order section 5.4 lists them."""
    table = TABLES[("server", receiver)]
    spec = table[kind] if kind in table else FREQUENCY_TABLES[("server", receiver)][kind]
    out: Dict[str, Any] = {"type": kind}
    for key, check in spec.fields:
        if key in fields:
            out[key] = fields[key]
        elif not isinstance(check, OptionalField):
            raise KeyError(key)
    return out


# ----------------------------------------------------------------- errors

# Section 8.2: every code the server sends in `error` or `posted`.
ERROR_CODES = frozenset(
    {
        "not-member",
        "away",
        "hidden",
        "blocked",
        "rate",
        "too-long",
        "unknown-slice",
        "slice-keyed",
        "bad-turn",
        "not-allowed",
        "not-found",
        "version",
        "bad-sign-in",
        "server",
    }
)

# Section 8.2's rows, by cause: (code, reason, default retryAfterMs). A
# screen may show the reason as sent, so each is the exact text.
REASONS: Dict[str, Tuple[str, str, int]] = {
    "stale-context": ("not-allowed", "The selected slice has changed frequency or mode. Keep the draft and try on its frequency.", 0),
    "not-member": ("not-member", "Tune to this frequency and stay for 10 seconds before you post or name a turn.", 0),
    "away": ("away", "Nobody is at the radio, so this station can't post or name turns.", 0),
    "hidden": ("hidden", "This station is hidden, so nothing more is taken from it.", 0),
    "blocked": ("blocked", "This station has been blocked from posting and naming turns.", 0),
    "rate": ("rate", "Too many at once. Try again in a moment.", 1),
    "rate-network": ("rate", "Too many connections from this network. Try again shortly.", 5000),
    "rate-busy": ("rate", "Roundtable is busy. Try again shortly.", 5000),
    "too-long": ("too-long", "A message can be up to 500 characters and 10 lines.", 0),
    "unknown-slice": ("unknown-slice", "That reading is for a slice this station has not listed.", 0),
    "slice-keyed": ("slice-keyed", "A slice can't send readings while it is transmitting.", 0),
    "bad-turn": ("bad-turn", "Only a turn with an unknown talker can be named, up to 24 hours after it ends.", 0),
    "not-allowed-name": ("not-allowed", "You can name a turn only if you were on this frequency during it.", 0),
    "not-allowed-delete": ("not-allowed", "You can delete only your own messages.", 0),
    "not-allowed-read": ("not-allowed", "You can read up to 4 other frequencies at once.", 0),
    "not-allowed-watch": ("not-allowed", "You can watch up to 4 frequencies at once.", 0),
    "not-allowed-archive": ("not-allowed", "To delete your archive, confirm with the exact words asked for.", 0),
    "not-allowed-callsign": ("not-allowed", "This station needs a callsign before it can send its presence.", 0),
    "not-allowed-empty": ("not-allowed", "There is nothing to send.", 0),
    "not-found": ("not-found", "That room, turn or message is not here.", 0),
    "version": ("version", "This version of the Roundtable protocol is not supported. Updating the software may help.", 0),
    "bad-sign-in": ("bad-sign-in", "Roundtable did not accept this station's key or signature.", 0),
    "server": ("server", "Roundtable can't save messages right now.", 0),
}

# Section 8.4: notices.
NOTICES: Dict[str, str] = {
    "blocked": "A Roundtable helper has blocked this station from posting and naming turns.",
    "not-counted": "A Roundtable helper has stopped counting this station's readings and keyed state.",
    "report-closed": "A report you made has been looked at.",
    "version-ending": "This version of Roundtable stops working on the date shown. Updating the software keeps it working.",
    "archive-deleted": "Your Roundtable archive has been deleted. Backups age out within 7 days.",
}


def error_message(cause: str, about: Optional[str], retry_after_ms: Optional[int] = None, receiver: str = "station") -> Dict[str, Any]:
    """An `error` for one of section 8.2's causes. `retryAfterMs` is set for
    `rate` (at least 1) and is otherwise 0 (section 5.4.11)."""
    code, reason, default_retry = REASONS[cause]
    retry = default_retry if retry_after_ms is None else retry_after_ms
    if code == "rate":
        retry = max(1, retry)
    else:
        retry = 0
    return message("error", receiver, code=code, reason=reason, about=about, retryAfterMs=retry)


def legacy_message(value):
    """Project optional frequency metadata away for unnegotiated v1 peers."""
    if isinstance(value, list):
        return [legacy_message(item) for item in value]
    if not isinstance(value, dict):
        return value
    drop = {"frequencyVersion", "stations", "sourceRoom", "bearingDeg", "view"}
    if "joining" in value:
        drop.add("slice")
    if "ref" in value and "callsign" in value:
        drop.update(("hz", "grid")) if "joining" not in value else drop.add("hz")
    if "kind" in value and "talkers" in value and "start" in value:
        drop.add("hz")
    if "text" in value and "state" in value:
        drop.update(("hz", "family"))
    return {key: legacy_message(item) for key, item in value.items() if key not in drop}
