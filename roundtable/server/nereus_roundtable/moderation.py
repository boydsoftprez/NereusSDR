# no-port-check: NereusSDR-original.
"""The review list: what helpers see and do (design 7.5, 11.7, 12.2, 12.3,
C16, C21, D16, D26).

Helpers act on stations, never on identities: they remove messages, block a
station from posting and naming turns, and stop counting a station's
readings and keyed state. No helper action moves history, signs anyone out
or deletes anyone (D26).

Every route needs a browser session (the `rt_session` cookie of web.py)
whose station is a helper: one in the config's `helpers`, or one added here.
Signed out is `401`; signed in as anyone else is `403` on every route. A
POST is a form (`application/x-www-form-urlencoded`) with the session's
`csrf` and an `Origin` of exactly `https://<public_host>`, checked by
web.py's own `_signed_post`, then the helper check.

JSON (`Cache-Control: no-store`):

- `GET /v1/review/reports`: `{"time","olderThanDay","reports":[entry...]}`,
  the open entries oldest first. An entry is every open report of one item:
  `{"id","version","target","room","first","count","reports":[{"callsign",
  "reason","t"}],"message","turn","station"}`. `message` (for a message) is
  `{"id","callsign","t","state","text","room"}`; `turn` (for a turn) is
  `{"id","kind","start","end","room","talkers":[callsign...],"names":
  [{"station","callsign","name"}]}`; `station` is the station acted on (a
  message's sender, or the station reported) as `{"id","callsign","grid",
  "blocked","notCounted"}`, each of the last two null or `{"until"}` with
  `until` null for good.
- `POST /v1/review/remove` (report, version): a message's text goes;
  `state` `removed`, so rooms show "Message removed".
- `POST /v1/review/block` (report, version, until): the station of a
  message or station entry may not post or name turns, until `until` (Unix
  ms, in the future) or, with `until` `forever`, for good.
- `POST /v1/review/stop-counting` (report, version, until): likewise, its
  readings and keyed state stop showing live and stop crediting turns; its
  stored rows are kept and marked.
- `POST /v1/review/leave` (report, version): closes the entry, no change.
- `POST /v1/review/undo` (action, version): undoes the entry's latest
  action within 600000 ms of it, and opens the entry again if that action
  closed it.
- `POST /v1/review/unblock`, `/resume-counting` (station): lift a block or
  a stop-counting at any time, "for good" ones included, by station id.
  Refused `409` `not-blocked` or `not-stopped` when there is none to lift.
  Logged like the others; it is not tied to an entry and has no Undo. The
  station is sent no notice (wire spec 5.4 has none for a lift).
- `POST /v1/review/helpers/add`, `/remove` (station): by station id only.
- `GET /v1/review/log`: `{"time","actions":[action...],"helpers":[{"id",
  "callsign","added","config"}]}`, newest first.
- `GET /v1/review/stations?callsign=`: `{"time","stations":[{"id",
  "callsign","grid","kind","lastSeen","helper"}]}`, the stations whose
  stored callsign has that base callsign, most recently seen first.

An action answers `{"action":action,"report":{"id","version","closed"}}`.
An action is `{"id","t","helper","helperCallsign","action","target",
"report","detail","undone"}`.

Each action names the entry's version. When another helper acted first the
version has moved on, and the action is refused `409` `changed` with what
they did (`action`) and the entry's `version`. Closing an entry tells each
connected reporter `notice` `report-closed`; the reported station is never
told who reported (D16). A block or stop-counting tells the station
`notice` `blocked` or `not-counted`.

Nothing here logs an address, a callsign or message text (wire spec 9.4).
"""

from __future__ import annotations

import json
import logging
import re
import sqlite3
from typing import Any, Dict, Optional, Tuple

from . import messages, protocol
from .http import App, Request, Response
from .service import short
from .web import _refuse as web_refuse

log = logging.getLogger("nereus_roundtable")

# Design 11.7: Undo for 10 minutes after an action.
UNDO_MS = 600000
DAY_MS = 86400000
LOG_LIMIT = 200
UNDOABLE = frozenset({"remove", "block", "stop-counting", "leave"})
_DECIMAL = re.compile(r"\A[1-9][0-9]{0,15}\Z")
_STATION_ID = re.compile(r"\A[a-z2-7]{26}\Z")

REASONS: Dict[str, Tuple[int, str]] = {
    "not-helper": (403, "Only Roundtable helpers can open the review list."),
    "bad-request": (400, "That request could not be read."),
    "bad-until": (400, "Choose a time in the future, or for good."),
    "not-for-this": (400, "That action does not apply to this report."),
    "config-helper": (400, "This helper is set in the server's configuration."),
    "no-station": (404, "There is no station with that id."),
    "no-report": (404, "That report is not on the review list."),
    "changed": (409, "Another helper acted on this report first."),
    "too-late": (409, "Undo is possible for 10 minutes after an action."),
    "gone": (409, "That message has no text to remove or restore."),
    "not-blocked": (409, "That station is not blocked."),
    "not-stopped": (409, "That station's readings are being counted."),
    "server": (503, "Roundtable can't answer right now."),
}


def _json(status: int, obj: Any) -> Response:
    body = protocol.encode(obj).encode("utf-8")
    return Response(
        status,
        body,
        [
            ("Content-Type", "application/json; charset=utf-8"),
            ("Cache-Control", "no-store"),
            ("Referrer-Policy", "no-referrer"),
        ],
    )


def _refuse(cause: str, **extra: Any) -> Response:
    status, reason = REASONS[cause]
    return _json(status, dict({"error": cause, "reason": reason}, **extra))


def _status(until: Optional[int], forever: bool, now: int) -> Optional[Dict[str, Any]]:
    if forever:
        return {"until": None}
    if until is not None and until > now:
        return {"until": int(until)}
    return None


class Moderation:
    """`Moderation(web, app)` adds the review list's routes to the HTTP
    app, using web.py's sessions and its CSRF and Origin checks."""

    def __init__(self, web: Any, app: App) -> None:
        self.web = web
        self.service = web.service
        self.store = web.store
        self.config = web.config
        self.clock = web.clock
        app.route("GET", "/v1/review/reports", self.reports)
        app.route("GET", "/v1/review/log", self.log)
        app.route("GET", "/v1/review/stations", self.stations)
        app.route("POST", "/v1/review/remove", self.remove)
        app.route("POST", "/v1/review/block", self.block)
        app.route("POST", "/v1/review/stop-counting", self.stop_counting)
        app.route("POST", "/v1/review/leave", self.leave)
        app.route("POST", "/v1/review/undo", self.undo)
        app.route("POST", "/v1/review/unblock", self.unblock)
        app.route("POST", "/v1/review/resume-counting", self.resume_counting)
        app.route("POST", "/v1/review/helpers/add", self.helper_add)
        app.route("POST", "/v1/review/helpers/remove", self.helper_remove)
        # Text kept for an Undo that ran out before a restart.
        self.store.undo_purge(self.clock.wall_ms() - UNDO_MS)

    # ----------------------------------------------------------- checks

    def _helper_get(self, request: Request) -> Tuple[Optional[str], Optional[Response]]:
        sid, _ = self.web._signed_in(request)
        if sid is None:
            return None, web_refuse("signed-out")
        if not self.service.is_helper(sid):
            return None, _refuse("not-helper")
        return sid, None

    def _helper_post(self, request: Request) -> Tuple[Optional[str], Dict[str, str], Optional[Response]]:
        session, form, refused = self.web._signed_post(request)
        if refused is not None:
            return None, {}, refused
        assert session is not None and form is not None
        sid = str(session["station_id"])
        if not self.service.is_helper(sid):
            return None, {}, _refuse("not-helper")
        return sid, form, None

    @staticmethod
    def _number(form: Dict[str, str], name: str) -> Optional[int]:
        text = form.get(name, "")
        return int(text) if _DECIMAL.match(text) else None

    def _entry(self, form: Dict[str, str]) -> Tuple[Optional[Dict[str, Any]], Optional[Response]]:
        """The entry the form names, at the version the form names."""
        item_id = self._number(form, "report")
        version = self._number(form, "version")
        if item_id is None or version is None:
            return None, _refuse("bad-request")
        item = self.store.review_item(item_id)
        if item is None:
            return None, _refuse("no-report")
        if item["version"] != version:
            return None, self._conflict(item)
        return item, None

    def _conflict(self, item: Dict[str, Any]) -> Response:
        latest = self.store.action_latest(item["id"])
        return _refuse("changed", version=item["version"], action=self._action_object(latest) if latest else None)

    def _until(self, form: Dict[str, str]) -> Tuple[Optional[Tuple[Optional[int], bool]], Optional[Response]]:
        text = form.get("until", "")
        if text == "forever":
            return (None, True), None
        if not _DECIMAL.match(text) or int(text) <= self.clock.wall_ms():
            return None, _refuse("bad-until")
        return (int(text), False), None

    def _station_of(self, item: Dict[str, Any]) -> Optional[str]:
        """The station an entry's block or stop-counting acts on: a
        message's sender, or the station reported."""
        if item["target"] == "station":
            return item["station"]
        if item["target"] == "message":
            row = self.store.message_get(int(item["message_id"]))
            return str(row["station_id"]) if row is not None else None
        return None

    # ----------------------------------------------------------- objects

    def _callsign(self, station_id: Optional[str]) -> Optional[str]:
        if not station_id:
            return None
        row = self.store.station_get(station_id)
        return row["callsign"] if row is not None else None

    def _action_object(self, row: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "id": int(row["id"]),
            "t": int(row["t"]),
            "helper": row["helper_id"],
            "helperCallsign": self._callsign(row["helper_id"]),
            "action": row["action"],
            "target": row["target"],
            "report": row["report"],
            "detail": json.loads(row["detail"]) if row["detail"] else None,
            "undone": row["undone"],
        }

    def _station_object(self, station_id: Optional[str], now: int) -> Optional[Dict[str, Any]]:
        if not station_id:
            return None
        row = self.store.station_get(station_id) or {}
        return {
            "id": station_id,
            "callsign": row.get("callsign"),
            "grid": row.get("grid"),
            "blocked": _status(row.get("posting_block_until"), bool(row.get("posting_block_forever")), now),
            "notCounted": _status(row.get("not_counted_until"), bool(row.get("not_counted_forever")), now),
        }

    def _entry_object(self, item: Dict[str, Any], now: int) -> Dict[str, Any]:
        reports = [r for r in item["reports"] if r["closed"] is None] or item["reports"]
        message = None
        turn = None
        room = item["room_id"]
        if item["target"] == "message":
            row = self.store.message_get(int(item["message_id"]))
            if row is not None:
                room = row["room_id"]
                message = {
                    "id": int(row["id"]),
                    "callsign": row["callsign"],
                    "t": int(row["t"]),
                    "state": row["state"],
                    "text": row["text"] if row["state"] == "shown" else None,
                    "room": row["room_id"],
                }
        elif item["target"] == "turn":
            row = self.store.turn_get(int(item["turn_id"]))
            if row is not None:
                room = row["room_id"]
                names = self.store.db.execute(
                    "SELECT station_id, callsign, name FROM turn_names WHERE turn_id = ? ORDER BY t, station_id",
                    (int(row["id"]),),
                ).fetchall()
                turn = {
                    "id": int(row["id"]),
                    "kind": row["kind"],
                    "start": int(row["start_ms"]),
                    "end": row["end_ms"],
                    "room": row["room_id"],
                    "talkers": [c for c in (row["talker_callsign"], row["second_talker_callsign"]) if c],
                    "names": [{"station": n["station_id"], "callsign": n["callsign"], "name": n["name"]} for n in names],
                }
        return {
            "id": item["id"],
            "version": item["version"],
            "target": item["target"],
            "room": room,
            "first": item["first"],
            "count": len({r["reporter_id"] for r in reports}),
            "reports": [
                {"callsign": self._callsign(r["reporter_id"]), "reason": r["reason"], "t": int(r["t"])} for r in reports
            ],
            "message": message,
            "turn": turn,
            "station": self._station_object(self._station_of(item), now),
        }

    # ----------------------------------------------------------- reading

    def reports(self, request: Request) -> Response:
        _, refused = self._helper_get(request)
        if refused is not None:
            return refused
        now = self.clock.wall_ms()
        entries = [self._entry_object(item, now) for item in self.store.review_open()]
        older = sum(1 for e in entries if e["first"] < now - DAY_MS)
        return _json(200, {"time": now, "olderThanDay": older, "reports": entries})

    def log(self, request: Request) -> Response:
        _, refused = self._helper_get(request)
        if refused is not None:
            return refused
        added = {r["station_id"]: int(r["added"]) for r in self.store.helpers_list()}
        ids = list(self.config.helpers) + [sid for sid in added if sid not in self.config.helpers]
        helpers = [
            {"id": sid, "callsign": self._callsign(sid), "added": added.get(sid), "config": sid in self.config.helpers}
            for sid in ids
        ]
        actions = [self._action_object(r) for r in self.store.actions_log(LOG_LIMIT)]
        return _json(200, {"time": self.clock.wall_ms(), "actions": actions, "helpers": helpers})

    def stations(self, request: Request) -> Response:
        _, refused = self._helper_get(request)
        if refused is not None:
            return refused
        values = request.query.get("callsign", [])
        if len(values) != 1 or not protocol.is_callsign(values[0].strip()):
            return _refuse("bad-request")
        base = messages.base_callsign(protocol.normalize_callsign(values[0].strip()))
        out = [
            {
                "id": r["id"],
                "callsign": r["callsign"],
                "grid": r["grid"],
                "kind": r["kind"],
                "lastSeen": int(r["last_seen"]),
                "helper": self.service.is_helper(r["id"]),
            }
            for r in self.store.stations_by_callsign(base)
        ]
        return _json(200, {"time": self.clock.wall_ms(), "stations": out})

    # ----------------------------------------------------------- actions

    def _answer(self, item_id: int, action_id: int) -> Response:
        item = self.store.review_item(item_id)
        action = self.store.action_get(action_id)
        assert item is not None and action is not None
        return _json(
            200,
            {
                "action": self._action_object(action),
                "report": {"id": item["id"], "version": item["version"], "closed": item["closed"] is not None},
            },
        )

    def _act(self, request: Request, kind: str) -> Response:
        """Remove, block, stop counting or leave: one entry, at its version."""
        helper, form, refused = self._helper_post(request)
        if refused is not None:
            return refused
        assert helper is not None
        item, refused = self._entry(form)
        if refused is not None:
            return refused
        assert item is not None
        station: Optional[str] = None
        until: Optional[int] = None
        forever = False
        if kind == "remove" and item["target"] != "message":
            return _refuse("not-for-this")
        if kind in ("block", "stop-counting"):
            station = self._station_of(item)
            if station is None or self.store.station_get(station) is None:
                return _refuse("not-for-this")
            parsed, refused = self._until(form)
            if refused is not None:
                return refused
            assert parsed is not None
            until, forever = parsed
        now = self.clock.wall_ms()
        undo_text: Optional[str] = None
        closed = False
        with self.service.event():
            try:
                with self.store.transaction():
                    if kind == "remove":
                        message_id = int(item["message_id"])
                        row = self.store.message_get(message_id)
                        undo_text = self.store.message_remove(message_id, helper)
                        if undo_text is None or row is None:
                            return _refuse("gone")
                        target = str(message_id)
                        detail: Dict[str, Any] = {"station": row["station_id"], "room": row["room_id"]}
                    elif kind in ("block", "stop-counting"):
                        assert station is not None
                        prior = self.store.station_get(station) or {}
                        if kind == "block":
                            was = (prior.get("posting_block_until"), bool(prior.get("posting_block_forever")))
                            self.store.station_block_set(station, until, forever)
                        else:
                            was = (prior.get("not_counted_until"), bool(prior.get("not_counted_forever")))
                            self.store.station_not_counted_set(station, until, forever)
                        target = station
                        detail = {"until": until, "forever": forever, "prior": {"until": was[0], "forever": was[1]}}
                    else:
                        target = str(item["id"])
                        detail = {}
                    action_id = self.store.action_add(helper, kind, target, detail, now, item["id"], undo_text)
                    self.store.review_bump(item["id"], item["version"] + 1)
                    closed = self.store.review_close(item["id"], helper, action_id, now)
            except sqlite3.Error as exc:
                log.error("a database error on a review action (%s)", type(exc).__name__)
                return _refuse("server")
            log.info("helper %s: %s on review entry %d", short(helper), kind, item["id"])
            if kind == "remove":
                self.service.message_changed(int(item["message_id"]))
                # The text is kept only while Undo lasts.
                self.clock.call_later(UNDO_MS + 1, lambda: self.store.undo_purge(now + 1))
            elif kind == "block":
                assert station is not None
                self.service.notice(station, "blocked", until)
            elif kind == "stop-counting":
                assert station is not None
                if self.service.apply_counting(station):
                    self.service.notice(station, "not-counted", until)
            if closed:
                self._tell_reporters(item)
        return self._answer(item["id"], action_id)

    def _tell_reporters(self, item: Dict[str, Any]) -> None:
        """Wire spec 5.3.7: each reporter is told only that it was looked
        at."""
        for reporter in sorted({r["reporter_id"] for r in item["reports"] if r["closed"] is None}):
            self.service.notice(reporter, "report-closed", None)

    def remove(self, request: Request) -> Response:
        return self._act(request, "remove")

    def block(self, request: Request) -> Response:
        return self._act(request, "block")

    def stop_counting(self, request: Request) -> Response:
        return self._act(request, "stop-counting")

    def leave(self, request: Request) -> Response:
        return self._act(request, "leave")

    def undo(self, request: Request) -> Response:
        helper, form, refused = self._helper_post(request)
        if refused is not None:
            return refused
        assert helper is not None
        action_id = self._number(form, "action")
        version = self._number(form, "version")
        if action_id is None or version is None:
            return _refuse("bad-request")
        action = self.store.action_get(action_id)
        if action is None or action["action"] not in UNDOABLE or action["report"] is None:
            return _refuse("no-report")
        item = self.store.review_item(int(action["report"]))
        if item is None:
            return _refuse("no-report")
        latest = self.store.action_latest(item["id"])
        if item["version"] != version or action["undone"] is not None or latest is None or latest["id"] != action_id:
            return self._conflict(item)
        now = self.clock.wall_ms()
        if now - int(action["t"]) > UNDO_MS:
            return _refuse("too-late")
        kind = action["action"]
        detail = json.loads(action["detail"]) if action["detail"] else {}
        prior = detail.get("prior") or {}
        with self.service.event():
            try:
                with self.store.transaction():
                    if kind == "remove":
                        text = action["undo_text"]
                        if text is None or not self.store.message_restore(int(action["target"]), text):
                            return _refuse("gone")
                    elif kind == "block":
                        self.store.station_block_set(action["target"], prior.get("until"), bool(prior.get("forever")))
                    elif kind == "stop-counting":
                        self.store.station_not_counted_set(
                            action["target"], prior.get("until"), bool(prior.get("forever"))
                        )
                    self.store.action_undone(action_id, now)
                    self.store.review_reopen(item["id"], action_id)
                    self.store.review_bump(item["id"], item["version"] + 1)
                    undo_id = self.store.action_add(helper, "undo", str(action_id), {"undid": kind}, now, item["id"])
            except sqlite3.Error as exc:
                log.error("a database error undoing a review action (%s)", type(exc).__name__)
                return _refuse("server")
            log.info("helper %s: undid %s on review entry %d", short(helper), kind, item["id"])
            if kind == "remove":
                self.service.message_changed(int(action["target"]))
            elif kind == "stop-counting":
                self.service.apply_counting(action["target"])
        return self._answer(item["id"], undo_id)

    # ----------------------------------------------------------- lifts

    def _lift(self, request: Request, kind: str) -> Response:
        """Unblock or resume counting a station, by station id, at any
        time; its mark goes and the live rooms follow at once."""
        helper, form, refused = self._helper_post(request)
        if refused is not None:
            return refused
        assert helper is not None
        station = form.get("station", "")
        if not _STATION_ID.match(station):
            return _refuse("bad-request")
        row = self.store.station_get(station)
        if row is None:
            return _refuse("no-station")
        now = self.clock.wall_ms()
        if kind == "unblock":
            was = (row.get("posting_block_until"), bool(row.get("posting_block_forever")))
            if not self.store.station_blocked(station, now):
                return _refuse("not-blocked")
        else:
            was = (row.get("not_counted_until"), bool(row.get("not_counted_forever")))
            if not self.store.station_not_counted(station, now):
                return _refuse("not-stopped")
        detail = {"prior": {"until": was[0], "forever": was[1]}}
        with self.service.event():
            try:
                with self.store.transaction():
                    if kind == "unblock":
                        self.store.station_block_set(station, None, False)
                    else:
                        self.store.station_not_counted_set(station, None, False)
                    action_id = self.store.action_add(helper, kind, station, detail, now)
            except sqlite3.Error as exc:
                log.error("a database error lifting a mark (%s)", type(exc).__name__)
                return _refuse("server")
            log.info("helper %s: %s station %s", short(helper), kind, short(station))
            if kind == "resume-counting":
                self.service.apply_counting(station)
        action = self.store.action_get(action_id)
        assert action is not None
        return _json(200, {"action": self._action_object(action)})

    def unblock(self, request: Request) -> Response:
        return self._lift(request, "unblock")

    def resume_counting(self, request: Request) -> Response:
        return self._lift(request, "resume-counting")

    # ----------------------------------------------------------- helpers

    def _helper_change(self, request: Request, add: bool) -> Response:
        helper, form, refused = self._helper_post(request)
        if refused is not None:
            return refused
        assert helper is not None
        station = form.get("station", "")
        if not _STATION_ID.match(station):
            return _refuse("bad-request")
        if add and self.store.station_get(station) is None:
            return _refuse("no-station")
        if not add and station in self.config.helpers:
            return _refuse("config-helper")
        now = self.clock.wall_ms()
        try:
            with self.store.transaction():
                changed = self.store.helper_add(station, now) if add else self.store.helper_remove(station)
                if changed:
                    self.store.action_add(helper, "helper-add" if add else "helper-remove", station, {}, now)
        except sqlite3.Error as exc:
            log.error("a database error changing the helpers (%s)", type(exc).__name__)
            return _refuse("server")
        if changed:
            log.info("helper %s %s helper %s", short(helper), "added" if add else "removed", short(station))
        return _json(200, {"changed": changed, "helper": self.service.is_helper(station)})

    def helper_add(self, request: Request) -> Response:
        return self._helper_change(request, True)

    def helper_remove(self, request: Request) -> Response:
        return self._helper_change(request, False)

