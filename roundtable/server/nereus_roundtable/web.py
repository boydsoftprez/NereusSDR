# no-port-check: NereusSDR-original.
"""The web side of Roundtable: the key page's form actions, web sign-in,
the archive's JSON and Delete my archive (design 7.3, 7.4, 11.6, 13; wire
spec 4.2, 5.4.10).

Form actions (`application/x-www-form-urlencoded` POSTs):

- `POST /key/new` (callsign, grid): issues a website key, shown once; at
  most 3 a day per address group, counted in memory only (wire spec 9.1,
  9.3). Answers JSON `{"key","callsign","grid"}`.
- `POST /key/signin` (key): signs the browser in; `303` to `/archive`.
- `POST /key/profile` (key, callsign, grid): sets a key station's callsign
  and grid. Answers JSON `{"callsign","grid"}`.
- `POST /key/move` (key): "Move my history to a new key". Issues a new key,
  revokes the old one (its connection closes 4004) and moves the old
  station's history to the new key's station id, in one transaction.
  Answers JSON `{"key"}`.
- `POST /signout-everywhere` (csrf): ends every browser session of the
  signed-in station; `303` to `/`.
- `POST /archive/delete` (csrf, confirm): Delete my archive, with
  `confirm` exactly `delete my archive`. Answers JSON `{"deleted","notice"}`.

`GET /signin?t=<token>` swaps a one-time link from `weblinkOk` for a
session cookie and answers `303` to `/archive`, so the token leaves the
address bar (wire spec 5.4.10, 7.9).

Every POST is refused `403` unless `Origin` is exactly
`https://<public_host>`. A POST that acts on a browser session also needs
that session's CSRF token in its `csrf` field; one that acts by a website
key needs the key itself in its `key` field, which no other page can know.
Nothing acts on a callsign alone.

The session cookie is `rt_session`, 32 random bytes; only its SHA-256 is
stored, with the session's CSRF token and its end, 30 days on (design C14).

Archive JSON for the signed-in station (`Cache-Control: no-store`, never
`Access-Control-Allow-Origin`):

- `GET /v1/me`: `{"time","station":{"callsign","grid","kind"},"csrf"}`.
- `GET /v1/me/sessions`: `{"time","sessions":[summary...]}`, newest first.
  A summary is `{"id","room","band","family","hz","start","end","turns",
  "reported"}`, `reported` counting the stations with readings of it.
- `GET /v1/me/session/<id>`: `{"time","session":summary,"turns":[{"id",
  "start","end"}],"readers":[reader...],"messages":[message...]}`. A reader
  is `{"callsign","km","offsetHz","away","watched","tracks":[{"turn",
  "first","last","away","dbm":[...],"present":[...]}]}`: the callsign,
  distance and offset stored with its latest track, AWAY if it was AWAY in
  any turn, WATCHED in seconds from its signal-present bits, and per turn
  one dBm value (or null) and one present flag a second from second
  `first` of the turn to `last`. Readers are sorted by WATCHED, most
  first. Messages are the room id's from the session's start to its end,
  as the wire's message object with `mine`.
- `GET /v1/me/histogram[?days=N][&band=B]`: `{"time","days","band",
  "hours":[24 counts]}`: for each UTC hour, how many stations heard the
  station (a present second) in its talking turns that started in that
  hour.

A session holds only the station's talking turns; unknown and doubled
turns never appear, whatever their names (design 11.6, D10). Readings a
helper stopped counting are left out (design 11.7).

Nothing here logs an address, a key, a token or a callsign (wire spec 9.4).
"""

from __future__ import annotations

import hmac
import logging
import re
import sqlite3
from typing import Any, Dict, List, Optional, Set, Tuple
from urllib.parse import parse_qs

from . import identity, protocol
from .http import App, Request, Response
from .limits import AddressGroups, Window
from .rooms import band_of, message_object
from .service import DELETE_CONFIRM, short

log = logging.getLogger("nereus_roundtable")

COOKIE = "rt_session"
# Design 7.4 and C14: a browser session lasts 30 days.
SESSION_S = 2592000
SESSION_MS = SESSION_S * 1000
COOKIE_ATTRIBUTES = f"Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age={SESSION_S}"
CLEAR_COOKIE = f"{COOKIE}=; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=0"
TOKEN_BYTES = 32
WEBSITE_KEY_BYTES = 32
# Wire spec 9.1: website keys issued, a day per address group.
KEY_WINDOW_MS = 86400000
ARCHIVE_PAGE = "/archive"
DAY_MS = 86400000
HOUR_MS = 3600000
MAX_HISTOGRAM_DAYS = 36500
FORM_TYPE = "application/x-www-form-urlencoded"
_DECIMAL = re.compile(r"\A[1-9][0-9]{0,15}\Z")

REASONS: Dict[str, Tuple[int, str]] = {
    "forbidden": (403, "This request did not come from a Roundtable page."),
    "signed-out": (401, "Sign in to see your archive."),
    "bad-request": (400, "That request could not be read."),
    "bad-key": (403, "Roundtable did not accept that key."),
    "callsign": (400, "A callsign is 3 to 16 letters, digits and slashes, with at least one letter and one digit."),
    "grid": (400, "A grid square is 4 or 6 characters, like EM73 or EM73tu."),
    "not-found": (404, "That session is not in your archive."),
    "not-allowed": (400, "To delete your archive, confirm with the exact words asked for."),
    "server": (503, "Roundtable can't answer right now."),
}
LINK_REFUSED = "This sign-in link has expired or was already used. Ask the app for a new one.\n"


def _json(status: int, obj: Any, extra: Optional[List[Tuple[str, str]]] = None) -> Response:
    """A private answer: never cached, never readable by another site."""
    body = protocol.encode(obj).encode("utf-8")
    headers = [
        ("Content-Type", "application/json; charset=utf-8"),
        ("Cache-Control", "no-store"),
        ("Referrer-Policy", "no-referrer"),
    ]
    return Response(status, body, headers + list(extra or []))


def _refuse(cause: str, extra: Optional[List[Tuple[str, str]]] = None) -> Response:
    status, reason = REASONS[cause]
    return _json(status, {"error": cause, "reason": reason}, extra)


def _see_other(location: str, cookie: str) -> Response:
    return Response(
        303,
        b"",
        [
            ("Location", location),
            ("Set-Cookie", cookie),
            ("Cache-Control", "no-store"),
            ("Referrer-Policy", "no-referrer"),
        ],
    )


def track_object(track: Dict[str, Any]) -> Dict[str, Any]:
    """One reader's packed track of one turn, unpacked: a dBm value (or
    null, no reading) and a present flag for each second."""
    first = int(track["first_second"])
    last = int(track["last_second"])
    n = max(0, last - first + 1)
    dbm_bytes = bytes(track["dbm_track"])
    bits = bytes(track["present_track"])
    dbm: List[Optional[int]] = []
    present: List[bool] = []
    for i in range(n):
        b = dbm_bytes[i] if i < len(dbm_bytes) else 255
        dbm.append(None if b == 255 else b - 160)
        present.append(i // 8 < len(bits) and bool(bits[i // 8] >> (i % 8) & 1))
    return {
        "turn": int(track["turn_id"]),
        "first": first,
        "last": last,
        "away": bool(track["away"]),
        "dbm": dbm,
        "present": present,
    }


class Web:
    """`Web(service, store, config, app)` adds the form actions, web
    sign-in and the archive JSON to the HTTP app's route table."""

    def __init__(self, service: Any, store: Any, config: Any, app: App) -> None:
        self.service = service
        self.store = store
        self.config = config
        self.clock = service.clock
        self.origin = f"https://{config.public_host}"
        # Held in memory only, for 24 hours (wire spec 9.3).
        self.key_window = Window(config.keys_per_group_per_day, KEY_WINDOW_MS)
        app.route("POST", "/key/new", self.key_new)
        app.route("POST", "/key/signin", self.key_signin)
        app.route("POST", "/key/profile", self.key_profile)
        app.route("POST", "/key/move", self.key_move)
        app.route("POST", "/signout-everywhere", self.signout_everywhere)
        app.route("POST", "/archive/delete", self.archive_delete)
        app.route("GET", "/signin", self.signin)
        app.route("GET", "/v1/me", self.me)
        app.route("GET", "/v1/me/sessions", self.sessions)
        app.route("GET", "/v1/me/histogram", self.histogram)
        app.route_prefix("GET", "/v1/me/session/", self.session)

    # ----------------------------------------------------------- checks

    def _random(self, n: int) -> bytes:
        return self.service.random(n)

    def _form(self, request: Request) -> Optional[Dict[str, str]]:
        """The POST's fields, each given once, or None for a body that is
        not a form."""
        kind = (request.header("content-type") or "").split(";", 1)[0].strip().lower()
        if kind != FORM_TYPE:
            return None
        try:
            text = request.body.decode("utf-8")
            fields = parse_qs(text, keep_blank_values=True, strict_parsing=False, encoding="utf-8", errors="strict")
        except (UnicodeDecodeError, ValueError):
            return None
        out: Dict[str, str] = {}
        for name, values in fields.items():
            if len(values) != 1:
                return None
            out[name] = values[0]
        return out

    def _origin_ok(self, request: Request) -> bool:
        values = request.headers.get("origin", [])
        return len(values) == 1 and values[0] == self.origin

    def _post(self, request: Request) -> Tuple[Optional[Dict[str, str]], Optional[Response]]:
        """The Origin check every POST makes, then its form."""
        if not self._origin_ok(request):
            return None, _refuse("forbidden")
        form = self._form(request)
        if form is None:
            return None, _refuse("bad-request")
        return form, None

    def _web_session(self, request: Request) -> Optional[Dict[str, Any]]:
        """The live session the request's `rt_session` cookie names."""
        now = self.clock.wall_ms()
        for header in request.headers.get("cookie", []):
            for part in header.split(";"):
                name, sep, value = part.strip().partition("=")
                if not sep or name != COOKIE:
                    continue
                token = identity.b64url_of_length(value.strip(), TOKEN_BYTES)
                if token is None:
                    continue
                found = self.store.web_session_get(identity.key_hash(token), now)
                if found is not None:
                    return found
        return None

    def _signed_post(self, request: Request) -> Tuple[Optional[Dict[str, Any]], Optional[Dict[str, str]], Optional[Response]]:
        """A POST that acts on the browser's session: Origin, the session,
        then its CSRF token."""
        form, refused = self._post(request)
        if refused is not None:
            return None, None, refused
        assert form is not None
        session = self._web_session(request)
        if session is None:
            return None, None, _refuse("signed-out")
        given = form.get("csrf", "")
        if not given or not hmac.compare_digest(given.encode("utf-8"), str(session["csrf_token"]).encode("utf-8")):
            return None, None, _refuse("forbidden")
        return session, form, None

    def _key_of(self, form: Dict[str, str]) -> Optional[Tuple[bytes, Dict[str, Any]]]:
        """The form's website key and its row, while the key is good."""
        raw = identity.b64url_of_length(form.get("key", ""), WEBSITE_KEY_BYTES)
        if raw is None:
            return None
        digest = identity.key_hash(raw)
        row = self.store.key_lookup(digest)
        if row is None or row["revoked"] is not None:
            return None
        return digest, row

    @staticmethod
    def _profile(form: Dict[str, str]) -> Tuple[Optional[Tuple[str, Optional[str]]], Optional[Response]]:
        """The form's callsign and grid, normalized (wire spec 4.5); an
        empty grid is none."""
        callsign = form.get("callsign", "").strip()
        if not protocol.is_callsign(callsign):
            return None, _refuse("callsign")
        grid_text = form.get("grid", "").strip()
        grid: Optional[str] = None
        if grid_text:
            if not protocol.is_grid(grid_text):
                return None, _refuse("grid")
            grid = protocol.normalize_grid(grid_text)
        return (protocol.normalize_callsign(callsign), grid), None

    def _new_session(self, station_id: str) -> str:
        """A browser session for the station; returns its Set-Cookie
        value."""
        token = self._random(TOKEN_BYTES)
        csrf = identity.to_b64url(self._random(TOKEN_BYTES))
        now = self.clock.wall_ms()
        self.store.web_session_add(identity.key_hash(token), station_id, csrf, now, now + SESSION_MS)
        return f"{COOKIE}={identity.to_b64url(token)}; {COOKIE_ATTRIBUTES}"

    # ----------------------------------------------------------- key page

    def key_new(self, request: Request) -> Response:
        form, refused = self._post(request)
        if refused is not None:
            return refused
        assert form is not None
        profile, refused = self._profile(form)
        if refused is not None:
            return refused
        assert profile is not None
        group = AddressGroups.group(request.address)
        now = self.clock.now_ms()
        self.key_window.sweep(now)
        retry = self.key_window.retry_after(group, now)
        if retry:
            seconds = max(1, (retry + 999) // 1000)
            reason = f"This network has had {self.config.keys_per_group_per_day} keys today. Try again tomorrow."
            return _json(429, {"error": "rate", "reason": reason}, [("Retry-After", str(seconds))])
        raw = self._random(WEBSITE_KEY_BYTES)
        station_id = identity.key_id(raw)
        wall = self.clock.wall_ms()
        callsign, grid = profile
        try:
            with self.store.transaction():
                self.store.station_upsert(station_id, "key", callsign, grid, wall)
                self.store.key_add(identity.key_hash(raw), station_id, wall)
        except sqlite3.Error as exc:
            log.error("a database error issuing a key (%s)", type(exc).__name__)
            return _refuse("server")
        self.key_window.allow(group, now)
        log.info("issued a website key for station %s", short(station_id))
        return _json(200, {"key": identity.to_b64url(raw), "callsign": callsign, "grid": grid})

    def key_signin(self, request: Request) -> Response:
        form, refused = self._post(request)
        if refused is not None:
            return refused
        assert form is not None
        found = self._key_of(form)
        if found is None:
            return _refuse("bad-key")
        return _see_other(ARCHIVE_PAGE, self._new_session(str(found[1]["station_id"])))

    def key_profile(self, request: Request) -> Response:
        form, refused = self._post(request)
        if refused is not None:
            return refused
        assert form is not None
        found = self._key_of(form)
        if found is None:
            return _refuse("bad-key")
        profile, refused = self._profile(form)
        if refused is not None:
            return refused
        assert profile is not None
        callsign, grid = profile
        self.store.station_profile(str(found[1]["station_id"]), callsign, grid)
        return _json(200, {"callsign": callsign, "grid": grid})

    def key_move(self, request: Request) -> Response:
        form, refused = self._post(request)
        if refused is not None:
            return refused
        assert form is not None
        found = self._key_of(form)
        if found is None:
            return _refuse("bad-key")
        old_hash, row = found
        old_id = str(row["station_id"])
        raw = self._random(WEBSITE_KEY_BYTES)
        new_id = identity.key_id(raw)
        with self.service.event():
            # The old key's connection closes first, so what its leaving
            # writes (an ended turn, tracks) is moved with the rest.
            self.service.revoke_station(old_id)
            try:
                moved = self.store.key_move(old_hash, identity.key_hash(raw), new_id, self.clock.wall_ms())
            except sqlite3.Error as exc:
                log.error("a database error moving a station's history (%s)", type(exc).__name__)
                return _refuse("server")
            if moved is None:
                return _refuse("bad-key")
            self.service.station_moved(moved, new_id)
        return _json(200, {"key": identity.to_b64url(raw)})

    # ----------------------------------------------------------- sign-in

    def signin(self, request: Request) -> Response:
        values = request.query.get("t", [])
        token = identity.b64url_of_length(values[0], TOKEN_BYTES) if len(values) == 1 else None
        station_id = self.store.weblink_use(identity.key_hash(token), self.clock.wall_ms()) if token else None
        if station_id is None:
            return Response(
                403,
                LINK_REFUSED.encode("utf-8"),
                [
                    ("Content-Type", "text/plain; charset=utf-8"),
                    ("Cache-Control", "no-store"),
                    ("Referrer-Policy", "no-referrer"),
                ],
            )
        log.info("station %s signed in on the web", short(station_id))
        return _see_other(ARCHIVE_PAGE, self._new_session(station_id))

    def signout_everywhere(self, request: Request) -> Response:
        session, _, refused = self._signed_post(request)
        if refused is not None:
            return refused
        assert session is not None
        self.store.web_sessions_end(str(session["station_id"]))
        return _see_other("/", CLEAR_COOKIE)

    def archive_delete(self, request: Request) -> Response:
        session, form, refused = self._signed_post(request)
        if refused is not None:
            return refused
        assert session is not None and form is not None
        if form.get("confirm") != DELETE_CONFIRM:
            return _refuse("not-allowed")
        try:
            self.service.delete_archive(str(session["station_id"]))
        except sqlite3.Error as exc:
            log.error("a database error deleting an archive (%s)", type(exc).__name__)
            return _refuse("server")
        return _json(200, {"deleted": True, "notice": protocol.NOTICES["archive-deleted"]})

    # ----------------------------------------------------------- archive

    def _signed_in(self, request: Request) -> Tuple[Optional[str], Optional[Dict[str, Any]]]:
        session = self._web_session(request)
        if session is None:
            return None, None
        return str(session["station_id"]), session

    def me(self, request: Request) -> Response:
        sid, session = self._signed_in(request)
        if sid is None or session is None:
            return _refuse("signed-out")
        station = self.store.station_get(sid) or {}
        return _json(
            200,
            {
                "time": self.clock.wall_ms(),
                "station": {"callsign": station.get("callsign"), "grid": station.get("grid"), "kind": station.get("kind")},
                "csrf": session["csrf_token"],
            },
        )

    def _summary(self, session: Dict[str, Any], turns: List[Dict[str, Any]], readings: List[Dict[str, Any]]) -> Dict[str, Any]:
        center = protocol.room_id_center(session["room_id"])
        family, hz = center if center is not None else (None, None)
        return {
            "id": int(session["id"]),
            "room": session["room_id"],
            "band": band_of(hz) if hz is not None else "other",
            "family": family,
            "hz": hz,
            "start": int(session["start_ms"]),
            "end": int(session["end_ms"]),
            "turns": len(turns),
            "reported": len({r["station_id"] for r in readings}),
        }

    def _talking(self, sid: str, session: Dict[str, Any]) -> List[Dict[str, Any]]:
        # Only the station's own talking turns, whatever else the session
        # row names.
        return [t for t in self.store.session_turns(session) if t["kind"] == "talking" and t["talker_id"] == sid]

    def sessions(self, request: Request) -> Response:
        sid, _ = self._signed_in(request)
        if sid is None:
            return _refuse("signed-out")
        out = []
        for session in self.store.sessions_of(sid):
            turns = self._talking(sid, session)
            readings = self.store.readings_of_turns([int(t["id"]) for t in turns], sid)
            out.append(self._summary(session, turns, readings))
        return _json(200, {"time": self.clock.wall_ms(), "sessions": out})

    def session(self, request: Request) -> Response:
        sid, _ = self._signed_in(request)
        if sid is None:
            return _refuse("signed-out")
        text = request.path[len("/v1/me/session/") :]
        if not _DECIMAL.match(text):
            return _refuse("not-found")
        session = self.store.session_get(int(text))
        if session is None or session["station_id"] != sid:
            # Another station's session reads as no session at all.
            return _refuse("not-found")
        turns = self._talking(sid, session)
        readings = self.store.readings_of_turns([int(t["id"]) for t in turns], sid)
        order = {int(t["id"]): i for i, t in enumerate(turns)}
        readers: Dict[str, Dict[str, Any]] = {}
        for track in sorted(readings, key=lambda r: order.get(int(r["turn_id"]), 0)):
            obj = track_object(track)
            reader = readers.setdefault(
                track["station_id"],
                {"callsign": None, "km": None, "offsetHz": None, "away": False, "watched": 0, "tracks": []},
            )
            reader["callsign"] = track["callsign"]
            if track["km"] is not None:
                reader["km"] = int(track["km"])
            if track["offset_hz"] is not None:
                reader["offsetHz"] = int(track["offset_hz"])
            reader["away"] = reader["away"] or obj["away"]
            reader["watched"] += sum(1 for p in obj["present"] if p)
            reader["tracks"].append(obj)
        ordered = sorted(readers.values(), key=lambda r: (-r["watched"], r["callsign"] or ""))
        messages = [
            dict(message_object(row), mine=row["station_id"] == sid)
            for row in self.store.messages_between(session["room_id"], int(session["start_ms"]), int(session["end_ms"]))
        ]
        return _json(
            200,
            {
                "time": self.clock.wall_ms(),
                "session": self._summary(session, turns, readings),
                "turns": [{"id": int(t["id"]), "start": int(t["start_ms"]), "end": int(t["end_ms"])} for t in turns],
                "readers": ordered,
                "messages": messages,
            },
        )

    def histogram(self, request: Request) -> Response:
        sid, _ = self._signed_in(request)
        if sid is None:
            return _refuse("signed-out")
        days: Optional[int] = None
        band: Optional[str] = None
        values = request.query.get("days")
        if values is not None:
            if len(values) != 1 or not _DECIMAL.match(values[0]) or int(values[0]) > MAX_HISTOGRAM_DAYS:
                return _refuse("bad-request")
            days = int(values[0])
        values = request.query.get("band")
        if values is not None:
            if len(values) != 1 or values[0] not in protocol.BANDS + ("other",):
                return _refuse("bad-request")
            band = values[0]
        now = self.clock.wall_ms()
        since = now - days * DAY_MS if days is not None else None
        heard: List[Set[str]] = [set() for _ in range(24)]
        for session in self.store.sessions_of(sid):
            center = protocol.room_id_center(session["room_id"])
            if band is not None and (center is None or band_of(center[1]) != band):
                continue
            turns = [t for t in self._talking(sid, session) if since is None or int(t["start_ms"]) >= since]
            starts = {int(t["id"]): int(t["start_ms"]) for t in turns}
            for track in self.store.readings_of_turns(list(starts), sid):
                if any(track_object(track)["present"]):
                    hour = (starts[int(track["turn_id"])] // HOUR_MS) % 24
                    heard[hour].add(track["station_id"])
        return _json(200, {"time": now, "days": days, "band": band, "hours": [len(h) for h in heard]})
