# no-port-check: NereusSDR-original.
"""End to end: other software with a website key and a station signing in
as a Core does, side by side on one frequency of the real server (design
D22 to D24, R-RT-24).

The server runs as its own process (`python3 -m nereus_roundtable`) on
loopback ports with a temporary database and the real clock. A website key
comes from the key page's form action over HTTP. Then both stations join
one USB room, key, read each other, open an unknown turn and name it, post,
delete, report and go AWAY, and every answer the key station gets is the
answer the Core-style station gets for the same request, under the same
limits. At the end the server's log and files hold no address, no whole
station id, no callsign and no message text (wire spec 9.3, 9.4).

Skipped, with the reason, when this Python lacks websockets or
cryptography. Nothing leaves 127.0.0.1.
"""

from __future__ import annotations

import asyncio
import json
import os
import signal
import socket
import sqlite3
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any, Dict, Iterator, List, Optional

import pytest

pytest.importorskip("websockets", reason="this Python has no websockets module (apt python3-websockets)")
pytest.importorskip("cryptography", reason="this Python has no cryptography module (apt python3-cryptography)")

from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

from nereus_roundtable import identity  # noqa: E402
from test_client import StationClient, usb_slice  # noqa: E402

SERVER = Path(__file__).resolve().parent.parent / "server"
HOST = "127.0.0.1"
HZ = 14212000
ROOM = "usb-14212000"
CORE_CALL = "KG4VCF"
KEY_CALL = "W4XYZ"
NAME = "W1AW"
# The [limits] this run sets: reports lowered so their limit is reached
# quickly; every other limit is the default.
REPORTS_PER_HOUR = 3
JOIN_TIMEOUT_S = 25.0
TURN_TIMEOUT_S = 10.0


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, 0))
        return s.getsockname()[1]


def _load() -> str:
    try:
        return "load average %.2f %.2f %.2f" % os.getloadavg()
    except OSError:
        return "load average not available"


class Server:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.ws_port = _free_port()
        self.http_port = _free_port()
        while self.http_port == self.ws_port:
            self.http_port = _free_port()
        self.database = root / "roundtable.db"
        self.log = root / "server.log"
        (root / "static").mkdir()
        (root / "backup").mkdir()
        self.config = root / "roundtable.conf"
        self.config.write_text(
            "[roundtable]\n"
            f"ws_listen = {HOST}:{self.ws_port}\n"
            f"http_listen = {HOST}:{self.http_port}\n"
            f"public_host = {HOST}\n"
            f"database = {self.database}\n"
            f"backup_dir = {root / 'backup'}\n"
            f"static_dir = {root / 'static'}\n"
            "log_level = info\n"
            "[limits]\n"
            f"reports_per_hour = {REPORTS_PER_HOUR}\n",
            encoding="utf-8",
        )
        self.proc: Optional[subprocess.Popen] = None
        self._log_handle: Any = None

    @property
    def station_uri(self) -> str:
        return f"ws://{HOST}:{self.ws_port}/v1/station"

    def start(self) -> None:
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        self._log_handle = open(self.log, "wb")
        self.proc = subprocess.Popen(
            [sys.executable, "-m", "nereus_roundtable", "--config", str(self.config)],
            cwd=str(SERVER),
            env=env,
            stdout=self._log_handle,
            stderr=subprocess.STDOUT,
        )
        deadline = time.monotonic() + 20
        for port in (self.ws_port, self.http_port):
            while True:
                if self.proc.poll() is not None:
                    raise AssertionError(f"the server exited with {self.proc.returncode}: {self.log_text()}")
                try:
                    with socket.create_connection((HOST, port), timeout=0.5):
                        break
                except OSError:
                    if time.monotonic() > deadline:
                        raise AssertionError(f"the server did not listen on {port} in 20 s ({_load()})")
                    time.sleep(0.05)

    def stop(self) -> int:
        assert self.proc is not None
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(5)
        self._log_handle.close()
        return self.proc.returncode

    def log_text(self) -> str:
        return self.log.read_bytes().decode("utf-8", "replace")

    def new_key(self, callsign: str, grid: str) -> str:
        """The key page's `POST /key/new` (web.py)."""
        body = urllib.parse.urlencode({"callsign": callsign, "grid": grid}).encode("ascii")
        request = urllib.request.Request(
            f"http://{HOST}:{self.http_port}/key/new",
            data=body,
            method="POST",
            headers={"Content-Type": "application/x-www-form-urlencoded", "Origin": f"https://{HOST}"},
        )
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(request, timeout=10) as answer:
            assert answer.status == 200
            found = json.loads(answer.read().decode("utf-8"))
        assert found["callsign"] == callsign and found["grid"] == grid, found
        return found["key"]


@pytest.fixture
def server(tmp_path: Path) -> Iterator[Server]:
    s = Server(tmp_path)
    s.start()
    try:
        yield s
    finally:
        if s.proc is not None and s.proc.poll() is None:
            s.stop()


def same(a: Dict[str, Any], b: Dict[str, Any], code: str, about: Optional[str] = None) -> None:
    """The two stations got the same refusal."""
    assert a["type"] == b["type"], (a, b)
    assert a["code"] == b["code"] == code, (a, b)
    assert a["reason"] == b["reason"], (a, b)
    if a["type"] == "error":
        assert a["about"] == b["about"] == about, (a, b)


def open_turn(view: Any, kind: str) -> Optional[Dict[str, Any]]:
    for turn in view.turns_of(kind):
        if turn["end"] is None:
            return turn
    return None


async def both_answer(core: StationClient, key: StationClient, send: Any, what: str) -> List[Dict[str, Any]]:
    """Sends one request from each and returns their first answers."""
    marks = [await send(core), await send(key)]
    return [await core.answer(marks[0], what), await key.answer(marks[1], what)]


async def scenario(srv: Server) -> Dict[str, str]:
    website_key = srv.new_key(KEY_CALL, "FN42")
    core = StationClient(
        srv.station_uri, HOST, "192.0.2.10", station_key=ec.generate_private_key(ec.SECP256R1()),
        software="NereusSDR", software_version="26.10.3",
    )
    key = StationClient(srv.station_uri, HOST, "192.0.2.20", website_key=website_key)
    try:
        # ---- Sign-in (wire spec 7.1, 7.2).
        cw = await core.connect()
        kw = await key.connect()
        assert (cw["kind"], cw["callsign"], cw["grid"]) == ("core", None, None)
        assert (kw["kind"], kw["callsign"], kw["grid"]) == ("key", KEY_CALL, "FN42")
        assert kw["station"] == identity.key_id(identity.from_b64url(website_key))
        assert cw["limits"] == kw["limits"]
        assert kw["limits"]["reportsPerHour"] == REPORTS_PER_HOUR

        # ---- Before presence a reading names an unlisted slice.
        a, b = await both_answer(core, key, lambda s: s.send(
            {"type": "reading", "slice": 0, "dbm": -90.0, "t": int(time.time() * 1000), "present": False}), "unknown-slice")
        same(a, b, "unknown-slice", "reading")

        # ---- Presence. A Core must send its callsign; a key station's is
        # the key page's, whatever it sends.
        mark = await core.presence(None, "EM73", [usb_slice(HZ)])
        refused = await core.answer(mark, "a Core's presence without a callsign")
        assert (refused["code"], refused["about"]) == ("not-allowed", "presence")
        await core.presence(CORE_CALL, "EM73", [usb_slice(HZ)])
        await key.presence("ZZ9ZZ", None, [usb_slice(HZ + 100)])

        # ---- Joining: no post yet.
        a = await core.post(ROOM, "early", "c-early")
        b = await key.post(ROOM, "early", "k-early")
        same(a, b, "not-member")

        # ---- Both become members after 10 s; the room opens.
        def members(s: StationClient) -> bool:
            view = s.view(ROOM)
            if view is None:
                return False
            rows = [view.row(CORE_CALL), view.row(KEY_CALL)]
            return all(r is not None and not r["joining"] for r in rows)

        await core.wait_for(lambda: members(core), "both members in the Core's view", JOIN_TIMEOUT_S)
        await key.wait_for(lambda: members(key), "both members in the key's view", JOIN_TIMEOUT_S)
        krow = core.view(ROOM).row(KEY_CALL)
        assert (krow["grid"], krow["software"], krow["softwareVersion"], krow["offsetHz"]) == (
            "FN42", "Example Logger", "3.1", 100)
        assert key.view(ROOM).own_row()["callsign"] == KEY_CALL
        assert core.view(ROOM).row("ZZ9ZZ") is None

        # ---- Turn 1: the key station talks, the Core reads it.
        await key.presence(None, None, [usb_slice(HZ + 100, keyed=True)])
        t1 = await core.wait_for(lambda: open_turn(core.view(ROOM), "talking"), "the key station's turn")
        assert [t["callsign"] for t in t1["talkers"]] == [KEY_CALL]
        await core.reading(0, -80.0, True)

        def read_with(s: StationClient, callsign: str, turn_id: int) -> bool:
            row = s.view(ROOM).row(callsign)
            return bool(row and row["reading"] and row["reading"]["turn"] == turn_id and row["reading"]["present"])

        await key.wait_for(lambda: read_with(key, CORE_CALL, t1["id"]), "the Core's reading of turn 1")
        assert key.view(ROOM).row(CORE_CALL)["km"] is not None
        await key.wait_for(lambda: key.view(ROOM).turns[t1["id"]]["heard"] == 1, "turn 1 heard by 1")
        mark_k = await key.reading(0, -85.0, False)
        await key.presence(None, None, [usb_slice(HZ + 100)])
        await core.wait_for(lambda: core.view(ROOM).turns[t1["id"]]["end"] is not None, "turn 1 ends", TURN_TIMEOUT_S)

        # ---- Turn 2: the Core talks, the key station reads it.
        await core.presence(CORE_CALL, "EM73", [usb_slice(HZ, keyed=True)])
        t2 = await key.wait_for(lambda: open_turn(key.view(ROOM), "talking"), "the Core's turn")
        assert [t["callsign"] for t in t2["talkers"]] == [CORE_CALL]
        mark_c = await core.reading(0, -85.0, False)
        refused_c = await core.answer(mark_c, "a keyed Core's reading")
        refused_k = [m for m in key.answers(mark_k) if m["type"] == "error" and m["about"] == "reading"]
        assert refused_k, "the keyed key station's reading was not refused"
        same(refused_c, refused_k[0], "slice-keyed", "reading")
        await key.reading(0, -80.0, True)
        await core.wait_for(lambda: read_with(core, KEY_CALL, t2["id"]), "the key station's reading of turn 2")
        await core.presence(CORE_CALL, "EM73", [usb_slice(HZ)])
        await key.wait_for(lambda: key.view(ROOM).turns[t2["id"]]["end"] is not None, "turn 2 ends", TURN_TIMEOUT_S)

        # ---- An unknown turn from both stations' present readings.
        await key.reading(0, -79.0, True)
        unknown = await core.wait_for(lambda: open_turn(core.view(ROOM), "unknown"), "the unknown turn")
        await core.reading(0, -81.0, True)
        await core.wait_for(lambda: core.view(ROOM).turns[unknown["id"]]["end"] is not None,
                            "the unknown turn goes quiet", TURN_TIMEOUT_S)

        # ---- Naming: the same refusals, then one name each.
        a, b = await both_answer(core, key, lambda s: s.name(t1["id"], NAME), "naming a talking turn")
        same(a, b, "bad-turn", "name")
        a, b = await both_answer(core, key, lambda s: s.name(999999999, NAME), "naming no turn")
        same(a, b, "not-found", "name")
        await core.name(unknown["id"], NAME)
        await key.name(unknown["id"], NAME)
        for s in (core, key):
            await s.wait_for(lambda s=s: s.view(ROOM).turns[unknown["id"]]["names"] == [{"name": NAME, "count": 2}],
                             "the unknown turn named by both")
            assert s.view(ROOM).turns[unknown["id"]]["mine"] == NAME

        # ---- Posts: mentions, the same limits and refusals.
        texts = {"core": f"{KEY_CALL} good copy here", "key": f"{CORE_CALL} thanks, solid here"}
        a = await core.post(ROOM, texts["core"], "c-1")
        b = await key.post(ROOM, texts["key"], "k-1")
        assert a["ok"] and b["ok"], (a, b)
        await key.wait_for(lambda: any(m["type"] == "mention" and m["message"]["id"] == a["msg"] for m in key.received),
                           "the key station's mention")
        await core.wait_for(lambda: any(m["type"] == "mention" and m["message"]["id"] == b["msg"] for m in core.received),
                            "the Core's mention")
        await core.wait_for(lambda: b["msg"] in core.view(ROOM).messages, "the key station's message in the Core's view")
        shown = core.view(ROOM).messages[b["msg"]]
        assert (shown["callsign"], shown["state"], shown["text"]) == (KEY_CALL, "shown", texts["key"])
        a = await core.post(ROOM, "x" * 501, "c-long")
        b = await key.post(ROOM, "x" * 501, "k-long")
        same(a, b, "too-long")
        a = await core.post(ROOM, "​", "c-empty")
        b = await key.post(ROOM, "​", "k-empty")
        same(a, b, "not-allowed")
        last: Dict[str, int] = {}
        for i in range(2, 7):
            a = await core.post(ROOM, f"core note {i}", f"c-{i}")
            b = await key.post(ROOM, f"key note {i}", f"k-{i}")
            assert a["ok"] and b["ok"], (i, a, b)
            last = {"core": a["msg"], "key": b["msg"]}
        a = await core.post(ROOM, "one too many", "c-7")
        b = await key.post(ROOM, "one too many", "k-7")
        same(a, b, "rate")

        # ---- Delete: only one's own.
        a, b = await both_answer(core, key, lambda s: s.delete(last["key"] if s is core else last["core"]), "delete")
        same(a, b, "not-allowed", "delete")
        for s, own in ((core, last["core"]), (key, last["key"])):
            mark = await s.delete(own)
            assert await s.silent_answer(mark) == []
        for s in (core, key):
            await s.wait_for(lambda s=s: all(s.view(ROOM).messages.get(m, {}).get("state") == "deleted"
                                             for m in last.values()), "both deletions shown")

        # ---- Reports: answered by nothing, then the same limit.
        a, b = await both_answer(core, key, lambda s: s.report_message(999999999, "spam"), "a report of nothing")
        same(a, b, "not-found", "report")
        refs = {"core": core.view(ROOM).own_row()["ref"], "key": key.view(ROOM).own_row()["ref"]}
        first_key_msg = [m for m in core.view(ROOM).messages.values() if m["text"] == texts["key"]][0]["id"]
        first_core_msg = [m for m in key.view(ROOM).messages.values() if m["text"] == texts["core"]][0]["id"]
        for s, other_msg, other_ref in ((core, first_key_msg, refs["key"]), (key, first_core_msg, refs["core"])):
            mark = len(s.received)
            await s.report_message(other_msg, "spam")
            await s.report_turn(unknown["id"], "wrongName")
            await s.report_station(ROOM, other_ref, "falseReadings")
            assert await s.silent_answer(mark) == []
        a, b = await both_answer(core, key, lambda s: s.report_turn(unknown["id"], None), "a fourth report")
        same(a, b, "rate", "report")
        assert a["retryAfterMs"] >= 1 and b["retryAfterMs"] >= 1

        # ---- AWAY: shown on the row; no posting or naming.
        await core.presence(CORE_CALL, "EM73", [usb_slice(HZ)], at_radio=False)
        await key.presence(None, None, [usb_slice(HZ + 100)], at_radio=False)
        await core.wait_for(lambda: core.view(ROOM).row(KEY_CALL)["away"], "the key station AWAY")
        await key.wait_for(lambda: key.view(ROOM).row(CORE_CALL)["away"], "the Core AWAY")
        await core.wait_for(lambda: core.view(ROOM).own_row()["away"], "the Core's own row AWAY")
        await key.wait_for(lambda: key.view(ROOM).own_row()["away"], "the key station's own row AWAY")
        a = await core.post(ROOM, "away note", "c-away")
        b = await key.post(ROOM, "away note", "k-away")
        same(a, b, "away")
        a, b = await both_answer(core, key, lambda s: s.name(unknown["id"], NAME), "naming while AWAY")
        same(a, b, "away", "name")

        # Nothing the server sent either station was a protocol close.
        assert core.closed_code is None and key.closed_code is None
        return {
            "core_id": core.station_id,
            "key_id": key.station_id,
            "website_key": website_key,
            "texts": json.dumps(texts),
        }
    finally:
        await core.close()
        await key.close()


def test_a_website_key_station_does_what_a_core_does(server: Server) -> None:
    try:
        seen = asyncio.run(scenario(server))
    except AssertionError as exc:
        raise AssertionError(f"{exc} ({_load()})") from None

    # The review list holds each station's three reports, counted once each.
    with sqlite3.connect(f"file:{server.database}?mode=ro", uri=True) as db:
        rows = db.execute("SELECT reporter_id, target FROM reports ORDER BY id").fetchall()
    by = {seen["core_id"]: [], seen["key_id"]: []}  # type: Dict[str, List[str]]
    for reporter, target in rows:
        by[reporter].append(target)
    assert by[seen["core_id"]] == ["message", "turn", "station"]
    assert by[seen["key_id"]] == ["message", "turn", "station"]

    assert server.stop() == 0
    log = server.log_text()
    assert "Traceback" not in log, log
    # Wire spec 9.4: no address, whole station id, callsign, key, message
    # text. 9.3: no address in any file.
    forbidden = [HOST, "192.0.2.", seen["core_id"], seen["key_id"], CORE_CALL, KEY_CALL, seen["website_key"]]
    forbidden += list(json.loads(seen["texts"]).values())
    for word in forbidden:
        assert word not in log, f"the server's log holds {word!r}"
    for path in server.root.iterdir():
        if path.name.startswith("roundtable.db"):
            data = path.read_bytes()
            for address in (b"127.0.0.1", b"192.0.2."):
                assert address not in data, f"{path.name} holds an address"
