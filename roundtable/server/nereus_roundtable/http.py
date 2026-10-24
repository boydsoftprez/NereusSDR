# no-port-check: NereusSDR-original.
"""The HTTP listener's app: the read API (wire spec 10), the web pages'
static files, and a route table later parts of the server add their own
pages and form actions to.

It is a small HTTP/1.1 server on asyncio streams from the standard library.
Each connection carries one request: the request line, the headers (16 KiB
at most, the listener's stream limit) and a body of at most 16 KiB given by
Content-Length. The answer always says `Connection: close` and the
connection then ends; there is no keep-alive and no chunked body.

The read API answers `GET` and `HEAD` only, with compact JSON, and takes 60
requests a minute per address group (wire spec 9.1, 10.1). Its objects are
a live reader's: `you` false, `mine` null, no station id, no address. The
client address is used only to find its address group, in memory; nothing
here logs a request, an address, a path or a callsign (wire spec 9.4).

Static files come from `static_dir` only. A path segment that starts with
`.` (so `.` and `..` too) or decodes to hold `/`, `\\` or NUL is 404, as is
a directory: no route ever lists one. Every answer carries the
Content-Security-Policy header.
"""

from __future__ import annotations

import asyncio
import http
import inspect
import json
import logging
import os
import re
import sqlite3
from typing import Any, Awaitable, Callable, Dict, List, Optional, Tuple, Union
from urllib.parse import parse_qs, unquote

from . import messages, protocol
from .limits import AddressGroups, Window
from .rooms import EARLIER_MS, VIEW_MESSAGES, Room, message_object
from .transport import forwarded_address

log = logging.getLogger("nereus_roundtable")

CSP = (
    "default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self'; "
    "script-src 'self'; frame-ancestors 'none'; form-action 'self'"
)
MAX_BODY_BYTES = 16384
READ_TIMEOUT_S = 10
WRITE_TIMEOUT_S = 10
API_PREFIX = "/v1/api/"
API_WINDOW_MS = 60000
# Wire spec 10.4.
MESSAGES_LIMIT_MAX = 50
_DECIMAL = re.compile(r"\A[0-9]{1,16}\Z")
_METHOD = re.compile(r"\A[A-Z]{1,16}\Z")
_HEADER_NAME = re.compile(r"\A[!#$%&'*+.^_`|~0-9A-Za-z-]+\Z")

# Wire spec 10.6: the read API's errors, by status.
API_ERRORS: Dict[int, Tuple[str, str]] = {
    400: ("bad-request", "That request could not be read."),
    404: ("not-found", "That room, turn or message is not here."),
    405: ("not-allowed", "Only GET and HEAD are supported."),
    429: ("rate", "Too many at once. Try again in a moment."),
    503: ("server", "Roundtable can't answer right now."),
}
# The plain answers outside the read API.
TEXT_ERRORS: Dict[int, str] = {
    400: "That request could not be read.\n",
    404: "Not found.\n",
    405: "That method is not supported here.\n",
    413: "That request is too large.\n",
    431: "That request's headers are too large.\n",
}

CONTENT_TYPES: Dict[str, str] = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".mjs": "text/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".txt": "text/plain; charset=utf-8",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".ico": "image/x-icon",
    ".webp": "image/webp",
    ".woff2": "font/woff2",
    ".webmanifest": "application/manifest+json",
}


class Request:
    """One request. `path` is the target before `?`, as sent; `segments`
    are its `/`-separated parts, each percent-decoded; `headers` maps each
    lower-case name to its values in order. `address` is the client's,
    for its address group only."""

    def __init__(
        self,
        method: str,
        path: str,
        segments: List[str],
        query: Dict[str, List[str]],
        headers: Dict[str, List[str]],
        body: bytes,
        address: str,
    ) -> None:
        self.method = method
        self.path = path
        self.segments = segments
        self.query = query
        self.headers = headers
        self.body = body
        self.address = address

    def header(self, name: str) -> Optional[str]:
        values = self.headers.get(name.lower())
        return values[-1] if values else None


class Response:
    def __init__(self, status: int, body: bytes = b"", headers: Optional[List[Tuple[str, str]]] = None) -> None:
        self.status = status
        self.body = body
        self.headers = list(headers or [])

    def head(self, name: str) -> Optional[str]:
        for key, value in self.headers:
            if key.lower() == name.lower():
                return value
        return None


Handler = Callable[[Request], Union[Response, Awaitable[Response]]]


def json_response(status: int, obj: Any, extra: Optional[List[Tuple[str, str]]] = None) -> Response:
    """A read API answer (wire spec 10.1)."""
    body = protocol.encode(obj).encode("utf-8")
    headers = [
        ("Content-Type", "application/json; charset=utf-8"),
        ("Cache-Control", "no-store"),
        ("Access-Control-Allow-Origin", "*"),
    ]
    return Response(status, body, headers + list(extra or []))


def api_error(status: int, extra: Optional[List[Tuple[str, str]]] = None) -> Response:
    code, reason = API_ERRORS[status]
    return json_response(status, {"error": code, "reason": reason}, extra)


def text_response(status: int, text: str, extra: Optional[List[Tuple[str, str]]] = None) -> Response:
    headers = [("Content-Type", "text/plain; charset=utf-8"), ("Cache-Control", "no-store")]
    return Response(status, text.encode("utf-8"), headers + list(extra or []))


def text_error(status: int) -> Response:
    return text_response(status, TEXT_ERRORS[status])


class _BadRequest(Exception):
    def __init__(self, status: int) -> None:
        super().__init__(status)
        self.status = status


def parse_head(head: bytes) -> Tuple[str, str, str, Dict[str, List[str]]]:
    """(method, path, query, headers) of a request head ending in a blank
    line. Raises _BadRequest(400) for anything malformed."""
    try:
        text = head.decode("ascii")
    except UnicodeDecodeError as exc:
        raise _BadRequest(400) from exc
    lines = text.split("\r\n")
    while lines and lines[-1] == "":
        lines.pop()
    if not lines:
        raise _BadRequest(400)
    parts = lines[0].split(" ")
    if len(parts) != 3:
        raise _BadRequest(400)
    method, target, version = parts
    if not _METHOD.match(method) or version not in ("HTTP/1.1", "HTTP/1.0"):
        raise _BadRequest(400)
    # Origin form only: Caddy forwards `/path?query`.
    if not target.startswith("/") or "#" in target:
        raise _BadRequest(400)
    path, _, query = target.partition("?")
    headers: Dict[str, List[str]] = {}
    for line in lines[1:]:
        name, sep, value = line.partition(":")
        if not sep or not _HEADER_NAME.match(name):
            # Folded lines (obsolete) and stray text are refused.
            raise _BadRequest(400)
        headers.setdefault(name.lower(), []).append(value.strip(" \t"))
    return method, path, query, headers


def split_path(path: str) -> Optional[List[str]]:
    """The path's segments, each percent-decoded as UTF-8, or None for a
    path no route serves: one with a `.` or `..` segment, a hidden name,
    or a NUL or backslash anywhere."""
    out: List[str] = []
    for raw in path.split("/")[1:]:
        try:
            seg = unquote(raw, encoding="utf-8", errors="strict")
        except UnicodeDecodeError:
            return None
        if seg.startswith(".") or "\x00" in seg or "\\" in seg:
            return None
        out.append(seg)
    return out


class App:
    """`App(service, store, config)`: the HTTP listener's app. The listener
    calls `handle(reader, writer)` for each connection."""

    def __init__(self, service: Any, store: Any, config: Any) -> None:
        self.service = service
        self.store = store
        self.config = config
        self.clock = service.clock
        self.api_window = Window(config.api_requests_per_minute, API_WINDOW_MS)
        self._swept_ms: Optional[int] = None
        self._routes: Dict[Tuple[str, str], Handler] = {}

    # ----------------------------------------------------------- routes

    def route(self, method: str, path: str, handler: Handler) -> None:
        """Answer `method` on exactly `path` (as sent, before `?`) with
        `handler(request)`, which returns a Response or an awaitable of
        one. A route for GET also answers HEAD, without the body. A route
        comes before the read API and the static files."""
        self._routes[(method.upper(), path)] = handler

    # ----------------------------------------------------------- the wire

    async def handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            try:
                request = await asyncio.wait_for(self._read(reader, writer), READ_TIMEOUT_S)
            except _BadRequest as bad:
                await self._write(writer, text_error(bad.status), head_only=False)
                return
            if request is None:
                return
            response = await self.answer(request)
            await self._write(writer, response, head_only=request.method == "HEAD")
        except (asyncio.TimeoutError, asyncio.IncompleteReadError, ConnectionError):
            return
        except Exception as exc:  # noqa: BLE001 - one request's failure ends only it
            log.error("an HTTP request failed (%s)", type(exc).__name__)
        finally:
            writer.close()

    async def _read(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> Optional[Request]:
        try:
            head = await reader.readuntil(b"\r\n\r\n")
        except asyncio.LimitOverrunError as exc:
            raise _BadRequest(431) from exc
        except asyncio.IncompleteReadError:
            return None
        method, path, query_text, headers = parse_head(head)
        if "transfer-encoding" in headers:
            raise _BadRequest(400)
        lengths = headers.get("content-length", [])
        length = 0
        if lengths:
            if len(set(lengths)) != 1 or not _DECIMAL.match(lengths[0]):
                raise _BadRequest(400)
            length = int(lengths[0])
            if length > MAX_BODY_BYTES:
                raise _BadRequest(413)
        body = await reader.readexactly(length) if length else b""
        try:
            query = parse_qs(query_text, keep_blank_values=True, encoding="utf-8", errors="strict")
        except (UnicodeDecodeError, ValueError) as exc:
            raise _BadRequest(400) from exc
        peer = writer.get_extra_info("peername")
        peer_text = str(peer[0]) if peer else ""
        address = forwarded_address(peer_text, headers.get("x-forwarded-for", []), self.config.trusted_proxies)
        return Request(method, path, split_path(path) or [], query, headers, body, address)

    async def _write(self, writer: asyncio.StreamWriter, response: Response, head_only: bool) -> None:
        status = http.HTTPStatus(response.status)
        lines = [f"HTTP/1.1 {status.value} {status.phrase}"]
        for name, value in response.headers:
            lines.append(f"{name}: {value}")
        lines.append(f"Content-Length: {len(response.body)}")
        lines.append(f"Content-Security-Policy: {CSP}")
        lines.append("X-Content-Type-Options: nosniff")
        lines.append("Connection: close")
        data = ("\r\n".join(lines) + "\r\n\r\n").encode("latin-1")
        if not head_only:
            data += response.body
        writer.write(data)
        await asyncio.wait_for(writer.drain(), WRITE_TIMEOUT_S)

    # ----------------------------------------------------------- dispatch

    async def answer(self, request: Request) -> Response:
        """The answer to one request, before it is written."""
        method = "GET" if request.method == "HEAD" else request.method
        handler = self._routes.get((method, request.path))
        if handler is None and request.method == "HEAD":
            handler = self._routes.get(("HEAD", request.path))
        if handler is not None:
            result = handler(request)
            if inspect.isawaitable(result):
                result = await result
            return result
        if any(path == request.path for _, path in self._routes):
            allowed = sorted({m for m, path in self._routes if path == request.path})
            if "GET" in allowed:
                allowed.append("HEAD")
            return text_response(405, TEXT_ERRORS[405], [("Allow", ", ".join(sorted(set(allowed))))])
        if request.path.startswith(API_PREFIX) or request.path == API_PREFIX.rstrip("/"):
            return self._api(request)
        return self._static(request)

    # ----------------------------------------------------------- read API

    def _api(self, request: Request) -> Response:
        if request.method not in ("GET", "HEAD"):
            return api_error(405, [("Allow", "GET, HEAD")])
        now = self.clock.now_ms()
        self._sweep(now)
        group = AddressGroups.group(request.address)
        retry = self.api_window.retry_after(group, now)
        if retry:
            seconds = max(1, (retry + 999) // 1000)
            return api_error(429, [("Retry-After", str(seconds))])
        self.api_window.allow(group, now)
        segments = split_path(request.path)
        if segments is None or segments[:2] != ["v1", "api"]:
            return api_error(404)
        route = segments[2:]
        try:
            if route == ["rooms"]:
                return self._rooms(request)
            if len(route) == 3 and route[0] == "room":
                return self._room(route[1], route[2])
            if len(route) == 4 and route[0] == "room" and route[3] == "messages":
                return self._messages(request, route[1], route[2])
            if len(route) == 2 and route[0] == "callsign":
                return self._callsign(route[1])
        except sqlite3.Error as exc:
            log.error("a database error answering the read API (%s)", type(exc).__name__)
            return api_error(503)
        return api_error(404)

    def _sweep(self, now: int) -> None:
        # Groups whose requests have all left the window are forgotten, at
        # most once a window.
        if self._swept_ms is None or now - self._swept_ms >= API_WINDOW_MS:
            self._swept_ms = now
            self.api_window.sweep(now)

    def _time(self) -> int:
        return self.clock.wall_ms()

    @staticmethod
    def _one(request: Request, name: str) -> Optional[List[str]]:
        return request.query.get(name)

    def _rooms(self, request: Request) -> Response:
        """Wire spec 10.2."""
        band: Optional[str] = None
        values = self._one(request, "band")
        if values is not None:
            if len(values) != 1 or values[0] not in protocol.BANDS + ("other",):
                return api_error(400)
            band = values[0]
        rooms = [r.summary() for r in self.service.book.live_rooms() if band is None or r.band == band]
        return json_response(200, {"time": self._time(), "rooms": rooms})

    @staticmethod
    def _room_id(family: str, hz: str) -> Optional[str]:
        room_id = f"{family}-{hz}"
        return room_id if protocol.room_id_center(room_id) is not None else None

    def _room(self, family: str, hz: str) -> Response:
        """Wire spec 10.3: the live view, else the most recent opening,
        closed and quiet."""
        room_id = self._room_id(family, hz)
        if room_id is None:
            return api_error(400)
        room = self.service.book.live(room_id)
        if room is not None:
            view = live_view(room)
            return json_response(200, {"time": self._time(), "live": True, "closed": None, "room": view})
        opening = self.store.opening_latest(room_id)
        if opening is None:
            return api_error(404)
        closed = opening["closed"] if opening["closed"] is not None else opening["last_active"]
        view = {
            "id": room_id,
            "opening": int(opening["opening_id"]),
            "band": opening["band"],
            "family": opening["family"],
            "hz": int(opening["center_hz"]),
            "members": [],
            "now": [],
            "earlier": self._closed_turns(room_id, int(opening["opening_id"]), int(closed)),
            "messages": [message_object(r) for r in self.store.messages_of_room(room_id, VIEW_MESSAGES)],
        }
        return json_response(200, {"time": self._time(), "live": False, "closed": int(closed), "room": view})

    def _closed_turns(self, room_id: str, opening_id: int, closed: int) -> List[Dict[str, Any]]:
        """The closed opening's turns that ended in the 30 minutes before it
        closed, newest first. A turn this process still holds is shown as
        it was; one from before a restart is rebuilt from the store."""
        held = {t.id: t for t in self.service.book.earlier.get(room_id, [])}
        out: List[Tuple[int, int, Dict[str, Any]]] = []
        for row in self.store.turns_of(opening_id):
            end = row["end_ms"]
            if end is None or end < closed - EARLIER_MS or end > closed:
                continue
            turn = held.get(int(row["id"]))
            obj = closed_turn_object(turn) if turn is not None else self._stored_turn_object(row)
            out.append((int(row["start_ms"]), int(row["id"]), obj))
        out.sort(key=lambda entry: (entry[0], entry[1]), reverse=True)
        return [obj for _, _, obj in out]

    def _stored_turn_object(self, row: Dict[str, Any]) -> Dict[str, Any]:
        talkers = []
        for key in ("talker_callsign", "second_talker_callsign"):
            if row[key]:
                # The store keeps no row number for a talker; after a
                # restart each shows as the first of its callsign.
                talkers.append({"ref": None, "callsign": row[key], "nth": 1})
        heard = 0
        if row["kind"] != "doubled":
            for track in self.store.tracks_of(int(row["id"])):
                n = int(track["last_second"]) - int(track["first_second"])
                bits = bytes(track["present_track"])
                if n >= 0 and n // 8 < len(bits) and bits[n // 8] >> (n % 8) & 1:
                    heard += 1
        names = [{"name": n, "count": c} for n, c in self.store.name_counts(int(row["id"])).items()]
        return {
            "id": int(row["id"]),
            "kind": row["kind"],
            "talkers": talkers,
            "start": int(row["start_ms"]),
            "end": int(row["end_ms"]),
            "ending": False,
            "names": names,
            "heard": heard,
            "mine": None,
        }

    def _messages(self, request: Request, family: str, hz: str) -> Response:
        """Wire spec 10.4."""
        room_id = self._room_id(family, hz)
        if room_id is None:
            return api_error(400)
        before: Optional[int] = None
        limit = MESSAGES_LIMIT_MAX
        for name in ("before", "limit"):
            values = self._one(request, name)
            if values is None:
                continue
            if len(values) != 1 or not _DECIMAL.match(values[0]):
                return api_error(400)
            if name == "before":
                before = int(values[0])
            else:
                limit = int(values[0])
                if not 1 <= limit <= MESSAGES_LIMIT_MAX:
                    return api_error(400)
        if self.store.opening_latest(room_id) is None:
            return api_error(404)
        room = self.service.book.live(room_id)
        rows = self.store.messages_of_room(room_id, limit, before)
        objects = [room.message_object(r) if room is not None else message_object(r) for r in reversed(rows)]
        return json_response(200, {"time": self._time(), "messages": objects})

    def _callsign(self, asked: str) -> Response:
        """Wire spec 10.5: one flat entry per live row whose base callsign
        is the one asked for, by room id, then ref."""
        if not protocol.is_callsign(asked):
            return api_error(400)
        callsign = protocol.normalize_callsign(asked)
        base = messages.base_callsign(callsign)
        rows = []
        for room in self.service.book.live_rooms():
            for row in room.rows.values():
                if messages.base_callsign(row.callsign) == base:
                    rows.append({"callsign": row.callsign, "room": room.id, "ref": row.ref})
        rows.sort(key=lambda r: (r["room"], r["ref"]))
        return json_response(200, {"time": self._time(), "callsign": callsign, "rows": rows})

    # ----------------------------------------------------------- static

    def _static(self, request: Request) -> Response:
        if request.method not in ("GET", "HEAD"):
            return text_response(405, TEXT_ERRORS[405], [("Allow", "GET, HEAD")])
        segments = split_path(request.path)
        if segments is None or any("/" in seg for seg in segments):
            return text_error(404)
        if not segments or segments[-1] == "":
            segments = segments[:-1] + ["index.html"] if segments else ["index.html"]
        if any(seg == "" for seg in segments):
            return text_error(404)
        root = os.path.realpath(self.config.static_dir)
        target = os.path.realpath(os.path.join(root, *segments))
        if os.path.commonpath([root, target]) != root or not os.path.isfile(target):
            # Outside the folder, missing, or a directory: never listed.
            return text_error(404)
        try:
            with open(target, "rb") as handle:
                body = handle.read()
        except OSError:
            return text_error(404)
        kind = CONTENT_TYPES.get(os.path.splitext(target)[1].lower(), "application/octet-stream")
        return Response(200, body, [("Content-Type", kind), ("Cache-Control", "no-cache")])


def live_view(room: Room) -> Dict[str, Any]:
    """A live room as a live reader sees it (5.4.5 without `type` and
    `gen`): every `you` false, every `mine` null."""
    return {
        "id": room.id,
        "opening": room.opening_id,
        "band": room.band,
        "family": room.family,
        "hz": room.center,
        "members": [room.member_object(row, False) for row in room.rows.values()],
        "now": [room.turn_object(t) for t in room.now_turns()],
        "earlier": [room.turn_object(t) for t in room.earlier_turns()],
        "messages": room.recent_messages(),
    }


def closed_turn_object(turn: Any) -> Dict[str, Any]:
    """An ended turn of a closed room: its rows have left, so no ref."""
    return {
        "id": turn.id,
        "kind": turn.kind,
        "talkers": [{"ref": None, "callsign": t.callsign, "nth": t.nth} for t in turn.talkers],
        "start": turn.start,
        "end": turn.end,
        "ending": False,
        "names": [dict(n) for n in turn.names],
        "heard": turn.heard(),
        "mine": None,
    }
