# no-port-check: NereusSDR-original.
"""A scripted Roundtable server for the web page checks.

The real Service, HTTP app, web routes and review routes run on loopback
through transport.start, with an in-memory store. About 700 in-process
stations sign in with real station keys and send presence, readings,
posts, names and reports through the service's own reading path, making
200 live rooms over 8 bands. The clock is a ManualClock started 75
seconds in the past and then kept in step with real time, so the rooms
have a history (earlier turns, joined members, a stale reading) the
moment the pages open. An archive for KG4VCF and a few review reports are
written straight to the store.

One port serves it all: a small path router sends /v1/live and
/v1/station to the WebSocket listener and everything else to the HTTP
listener, and answers the test controls under /__test/ itself. The
controls exist only here, never in the server.

    python3 serve.py [--port N]

prints `READY http://localhost:N` once the scenario is in place, and stops
on SIGTERM or when stdin closes.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import random
import signal
import sys
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple
from urllib.parse import parse_qs, urlencode

HERE = Path(__file__).resolve().parent
TESTS = HERE.parent
ROOT = TESTS.parent
sys.path.insert(0, str(ROOT / "server"))
sys.path.insert(0, str(TESTS))

from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

from inproc import Station  # noqa: E402
from nereus_roundtable import identity, protocol, transport  # noqa: E402
from nereus_roundtable.clock import ManualClock  # noqa: E402
from nereus_roundtable.config import Config  # noqa: E402
from nereus_roundtable.http import App, Request, split_path  # noqa: E402
from nereus_roundtable.moderation import Moderation  # noqa: E402
from nereus_roundtable.rooms import band_of  # noqa: E402
from nereus_roundtable.service import LIVE, Service, _Close  # noqa: E402
from nereus_roundtable.store import Store  # noqa: E402
from nereus_roundtable.web import Web  # noqa: E402

PUBLIC_HOST = "roundtable.nereussdr.com"
HISTORY_MS = 75000
STEP_MS = 500
DAY_MS = 86400000
CORPUS = json.loads((TESTS / "data" / "text-corpus.json").read_text("utf-8"))["cases"]

# Texts that would set a page flag if any of them ran as script.
INJECTIONS = [
    '<img src=x onerror="window.__rtInjected=1">',
    "<script>window.__rtInjected=1</script>",
    "javascript:window.__rtInjected=1",
    '[click](javascript:window.__rtInjected=1) <svg onload="window.__rtInjected=1"></svg>',
]

# Band, low edge, high edge, rooms, phone mode (wire spec 6.2).
BAND_PLAN = [
    ("160m", 1800000, 2000000, 10, "LSB"),
    ("80m", 3500000, 4000000, 30, "LSB"),
    ("40m", 7000000, 7300000, 44, "LSB"),
    ("20m", 14000000, 14350000, 52, "USB"),
    ("17m", 18068000, 18168000, 14, "USB"),
    ("15m", 21000000, 21450000, 22, "USB"),
    ("10m", 28000000, 29700000, 20, "USB"),
    ("6m", 50000000, 54000000, 8, "USB"),
]

PHRASES = [
    "Good copy here, **59** into EM73",
    "Thanks for the QSO, 73",
    "Anyone hear the DX on the low end?",
    "Antenna is a *dipole* at 40 ft",
    "QRZ? Listening for a few minutes",
    "Band is opening to the west coast",
    "Running 100 W, nothing fancy",
    "Back in 10, *73*",
]

GRIDS = ["EM73", "FN31", "DM79", "EN52", "FM18", "CN87", "EL96", "IO91", "JO62", "PM95", "QF56", "FN20", "EM12", "DN70", "EM48"]
PREFIXES = ["K", "W", "N", "KD", "KG", "KE", "AA", "AB", "VE", "G", "M", "DL", "F", "EA", "JA", "VK", "ZL", "PY", "OH", "SM", "ON", "PA", "OK", "SP", "HA"]

log = logging.getLogger("roundtable.pages")


def drain(st: Station, keep: Optional[List[Dict[str, Any]]] = None) -> None:
    """Empty a station's queue as a reading client would. With `keep`,
    the messages are decoded and kept."""
    queue = st.conn.queue
    while not queue.empty():
        item = queue.get_nowait()
        if isinstance(item, _Close):
            st.closed = item.code
            continue
        text, size = item
        st.conn.queued_bytes -= size
        if keep is not None:
            keep.append(json.loads(text))


class _Holder:
    def __init__(self, service: Service) -> None:
        self.service = service


class Sim:
    """One scripted station: its connection and how it behaves."""

    def __init__(self, world: "World", call: str, grid: str, room: Optional["RoomPlan"], role: str) -> None:
        self.world = world
        self.call = call
        self.grid = grid
        self.room = room
        self.role = role  # talker, reader, hearer, quiet, mute, desk
        self.keyed = False
        self.away = False
        self.period = 5000
        self.next = 0
        self.offset = 0
        self.dbm = -90.0
        self.readings_until: Optional[int] = None
        self.kept: Optional[List[Dict[str, Any]]] = None
        self.st = world.sign_in(world.address())
        self.sid = self.st.station_id

    @property
    def hz(self) -> int:
        assert self.room is not None
        return self.room.hz + self.offset

    def presence(self) -> None:
        slices = [] if self.room is None else [protocol_slice(0, self.hz, self.room.mode, self.keyed)]
        self.st.send({"type": "presence", "callsign": self.call, "grid": self.grid, "atRadio": not self.away, "slices": slices})

    def key(self, keyed: bool) -> None:
        self.keyed = keyed
        self.presence()

    def frame(self, now_wall: int) -> None:
        """Its periodic frame: a reading, or a presence when it sends none."""
        room = self.room
        sends_reading = (
            room is not None
            and not self.keyed
            and self.role in ("reader", "hearer", "quiet")
            and (self.readings_until is None or self.world.offset_ms() < self.readings_until)
        )
        if sends_reading:
            assert room is not None
            if self.role == "hearer":
                present = True
            elif self.role == "quiet":
                present = False
            else:
                present = room.anyone_keyed()
            dbm = -66.735748 if self.call == "N4RDR" else round(self.dbm + self.world.rng.uniform(-3, 3), 1)
            self.st.send({"type": "reading", "slice": 0, "dbm": dbm, "t": now_wall, "present": present})
        else:
            self.presence()

    def post(self, text: str, client_id: str) -> None:
        assert self.room is not None
        self.st.send({"type": "post", "room": self.room.id, "text": text, "clientId": client_id})


def protocol_slice(slice_id: int, hz: int, mode: str, keyed: bool) -> Dict[str, Any]:
    return {"id": slice_id, "hz": hz, "mode": mode, "filterHz": 500 if mode.startswith("CW") else 2700, "keyed": keyed}


class RoomPlan:
    def __init__(self, band: str, hz: int, mode: str, kind: str) -> None:
        self.band = band
        self.hz = hz
        self.mode = mode
        self.kind = kind
        family = {"LSB": "lsb", "USB": "usb", "CWU": "cw", "DIGU": "digu"}[mode]
        self.id = f"{family}-{hz}"
        self.members: List[Sim] = []
        self.rotate_at: Optional[int] = None

    def anyone_keyed(self) -> bool:
        return any(m.keyed for m in self.members)


class World:
    def __init__(self, port: int) -> None:
        self.rng = random.Random(7)
        self.port = port
        config = Config()
        config.public_host = PUBLIC_HOST
        config.ping_interval_ms = 0
        config.static_dir = str(ROOT / "server" / "static")
        config.ws_listen = ["127.0.0.1:0"]
        config.http_listen = ["127.0.0.1:0"]
        # One machine plays every station and every reader.
        config.station_connections_per_group = 100000
        config.live_sockets_per_group = 64
        config.api_requests_per_minute = 100000
        config.messages_per_minute = 1000
        config.messages_per_hour = 10000
        config.presence_per_second = 5
        config.names_per_minute = 100
        config.reports_per_hour = 100
        self.config = config
        self.real_start = int(time.time() * 1000)
        self.clock = ManualClock(self.real_start - HISTORY_MS)
        self.store = Store(":memory:")
        self.service = Service(config, self.clock, self.store)
        self.app = App(self.service, self.store, config)
        self.web = Web(self.service, self.store, config, self.app)
        Moderation(self.web, self.app)
        self.web.origin = f"http://localhost:{port}"
        self.holder = _Holder(self.service)
        self.sims: List[Sim] = []
        self.rooms: List[RoomPlan] = []
        self.events: List[Tuple[int, int, Callable[[], None]]] = []
        self._seq = 0
        self._addr = 0
        self._calls: set = set()
        self.refuse_live = False
        self.facts: Dict[str, Any] = {"corpus": {}, "injections": [], "reports": {}, "sessions": {}}
        self.cookies: Dict[str, Tuple[str, str]] = {}

    # ----------------------------------------------------------- helpers

    def offset_ms(self) -> int:
        return self.clock.wall_ms() - (self.real_start - HISTORY_MS)

    def address(self) -> str:
        self._addr += 1
        return f"10.{(self._addr // 250) % 250}.{self._addr % 250}.1"

    def sign_in(self, address: str) -> Station:
        key = ec.generate_private_key(ec.SECP256R1())
        st = Station(self.holder, address)  # type: ignore[arg-type]
        got: List[Dict[str, Any]] = []
        drain(st, got)
        nonce = identity.from_b64url(got[0]["nonce"])
        sig = identity.sign_raw(key, identity.hello_transcript(PUBLIC_HOST, nonce))
        st.send(
            {
                "type": "hello",
                "v": 1,
                "software": "NereusSDR",
                "softwareVersion": "2026.10.1",
                "spki": identity.to_b64url(identity.spki_of(key.public_key())),
                "sig": identity.to_b64url(sig),
            }
        )
        got = []
        drain(st, got)
        assert got and got[0]["type"] == "welcome", got[:1]
        st.station_id = got[0]["station"]
        return st

    def callsign(self) -> str:
        while True:
            p = self.rng.choice(PREFIXES)
            n = self.rng.randint(0, 9)
            s = "".join(self.rng.choice("ABCDEFGHIJKLMNOPQRSTUVWXYZ") for _ in range(self.rng.choice((2, 3, 3))))
            call = f"{p}{n}{s}"
            if call not in self._calls:
                self._calls.add(call)
                return call

    def at(self, offset_ms: int, fn: Callable[[], None]) -> None:
        self._seq += 1
        self.events.append((offset_ms, self._seq, fn))

    def add_sim(self, room: Optional[RoomPlan], role: str, call: Optional[str] = None, grid: Optional[str] = None) -> Sim:
        call = call or self.callsign()
        self._calls.add(call)
        sim = Sim(self, call, grid or self.rng.choice(GRIDS), room, role)
        if room is not None:
            # The first slice in a room sets its center (6.3).
            if room.members:
                wide = 60 if room.mode == "CWU" else 400
                sim.offset = self.rng.randint(-wide, wide) // 10 * 10
            room.members.append(sim)
        sim.dbm = self.rng.uniform(-112, -58)
        sim.period = {"hearer": 1000, "quiet": 20000, "mute": 20000}.get(role, 8000)
        sim.next = self.rng.randint(10000, 10000 + sim.period)
        self.sims.append(sim)
        return sim

    # ----------------------------------------------------------- rooms

    def plan_rooms(self) -> None:
        special = {
            14212000: ("20m", "USB", "special-talk"),
            14250000: ("20m", "USB", "special-stale"),
            7185000: ("40m", "LSB", "special-corpus"),
        }
        for band, lo, hi, count, phone in BAND_PLAN:
            mine = [hz for hz in special if lo <= hz <= hi]
            for hz in mine:
                self.rooms.append(RoomPlan(band, hz, special[hz][1], special[hz][2]))
            slots = [hz for hz in range(lo + 5000, hi - 4999, 5000) if all(abs(hz - s) >= 5000 for s in special)]
            picked = sorted(self.rng.sample(slots, count - len(mine)))
            for i, hz in enumerate(picked):
                r = self.rng.random()
                mode = phone
                if i < max(1, count // 10):
                    mode = "CWU"
                elif r < 0.08:
                    mode = "DIGU"
                k = self.rng.random()
                if k < 0.45:
                    kind = "talking"
                elif k < 0.53:
                    kind = "unknown"
                elif k < 0.57 and mode != "DIGU":
                    kind = "doubled"
                else:
                    kind = "quiet"
                self.rooms.append(RoomPlan(band, hz, mode, kind))

    def populate(self) -> None:
        rng = self.rng
        for room in self.rooms:
            if room.kind == "special-talk":
                self.special_talk(room)
            elif room.kind == "special-stale":
                self.special_stale(room)
            elif room.kind == "special-corpus":
                self.special_corpus(room)
            elif room.kind == "talking":
                n = rng.choice((1, 1, 1, 2, 2, 2, 3, 3, 4, 6, 9, 14))
                talker = self.add_sim(room, "reader")
                for _ in range(n):
                    self.add_sim(room, "reader")
                start = rng.randint(0, 65000)
                self.at(start, lambda t=talker: t.key(True))
                room.rotate_at = start + rng.randint(240000, 540000)
            elif room.kind == "doubled":
                a = self.add_sim(room, "reader")
                b = self.add_sim(room, "reader")
                for _ in range(rng.randint(0, 2)):
                    self.add_sim(room, "reader")
                start = rng.randint(15000, 60000)
                self.at(start, lambda s=a: s.key(True))
                self.at(start + 4000, lambda s=b: s.key(True))
            elif room.kind == "unknown":
                self.add_sim(room, "hearer")
                for _ in range(rng.randint(0, 2)):
                    self.add_sim(room, "reader")
            else:
                for _ in range(rng.randint(1, 2)):
                    sim = self.add_sim(room, "quiet")
                    sim.away = rng.random() < 0.4
            if room.kind in ("talking", "quiet", "unknown") and rng.random() < 0.3 and room.members:
                for j in range(rng.randint(1, 3)):
                    who = rng.choice(room.members)
                    text = rng.choice(PHRASES)
                    self.at(rng.randint(15000, 70000), lambda w=who, x=text, c=f"p{j}": w.post(x, c))
        poster_room = next(r for r in self.rooms if r.band == "40m" and r.kind == "talking")
        # A member with no reading who posts: found by what it posted.
        poster = self.add_sim(poster_room, "mute", call="VE3PST", grid="FN03")
        self.at(50000, lambda: poster.post("Anyone on for the net tonight?", "ve3pst"))
        self.facts["poster"] = {"callsign": "VE3PST", "room": poster_room.id}

    def special_talk(self, room: RoomPlan) -> None:
        """K4ABC talking with 9 readers; earlier W9ABC talking, then an
        unknown turn two members named W4XYZ; one reading gone stale; a
        message that gets reported."""
        k4 = self.add_sim(room, "reader", "K4ABC", "EM73")
        w9 = self.add_sim(room, "reader", "W9ABC", "EN52")
        readers = {}
        for call, grid in (
            ("N4RDR", "EM85"),
            ("W5STL", "EM12"),
            ("AA4QQ", "FM18"),
            ("KD8LMN", "EN80"),
            ("VE3XYZ", "FN03"),
            ("G4ABC", "IO91"),
            ("N0SPM", "EN34"),
            ("JA1QRP", "PM95"),
        ):
            readers[call] = self.add_sim(room, "reader", call, grid)
        readers["JA1QRP"].away = True
        readers["W5STL"].readings_until = 50000
        g4 = readers["G4ABC"]
        self.at(0, lambda: w9.key(True))
        self.at(20000, lambda: w9.key(False))

        def hear() -> None:
            g4.role = "hearer"
            g4.period = 1000
            g4.next = self.offset_ms()

        def stop_hearing() -> None:
            g4.role = "reader"
            g4.period = 5000

        self.at(24000, hear)
        self.at(30500, stop_hearing)

        def name_it() -> None:
            live = self.service.book.live(room.id)
            assert live is not None
            unknown = [t for t in live.earlier_turns() if t.kind == "unknown"]
            assert unknown, "the unknown turn"
            turn_id = unknown[0].id
            self.facts["unknownTurn"] = turn_id
            for who in (g4, readers["AA4QQ"]):
                who.st.send({"type": "name", "turn": turn_id, "name": "W4XYZ"})

        self.at(37000, name_it)
        self.at(40000, lambda: k4.key(True))
        spam: Dict[str, int] = {}

        def posts() -> None:
            n0 = readers["N0SPM"]
            n0.kept = []
            n0.post("Buy my amplifier, cheap: http://example.com/amp", "spam")
            readers["N4RDR"].post("**Good copy** here, *59* in EM85. Photos at https://example.org/shack", "n4")
            readers["VE3XYZ"].post("Thanks for the QSO, 73", "ve3")

        def find_spam() -> None:
            n0 = readers["N0SPM"]
            drain(n0.st, n0.kept)
            got = [m for m in (n0.kept or []) if m["type"] == "posted" and m.get("clientId") == "spam"]
            assert got and got[0]["ok"], got
            spam["id"] = got[0]["msg"]
            n0.kept = None

        self.at(45000, posts)
        self.at(45500, find_spam)

        def reports() -> None:
            live = self.service.book.live(room.id)
            assert live is not None
            readers["AA4QQ"].st.send({"type": "report", "target": "message", "msg": spam["id"], "reason": "spam"})
            readers["KD8LMN"].st.send({"type": "report", "target": "turn", "turn": self.facts["unknownTurn"], "reason": "wrongName"})
            ref = live.rows[readers["W5STL"].sid].ref
            readers["N4RDR"].st.send({"type": "report", "target": "station", "room": room.id, "ref": ref, "reason": "falseReadings"})
            self.facts["reports"].update({"message": spam["id"], "station": "W5STL"})

        self.at(60000, reports)
        self.facts["talkRoom"] = {"id": room.id, "talker": "K4ABC", "reader": "N4RDR", "stale": "W5STL", "poster": "N4RDR"}

    def special_stale(self, room: RoomPlan) -> None:
        """Talking, but nothing new in the room for over a minute."""
        talker = self.add_sim(room, "mute", "W7OLD", "DN70")
        self.add_sim(room, "mute", "W7RDA", "DN40")
        self.add_sim(room, "mute", "K7RDB", "CN87")
        self.at(0, lambda: talker.key(True))
        self.facts["staleRoom"] = room.id

    def special_corpus(self, room: RoomPlan) -> None:
        """The text corpus and the injections, posted by one member."""
        talker = self.add_sim(room, "reader", "W2RDR", "FN20")
        poster = self.add_sim(room, "reader", "K1TXT", "FN31")
        self.at(5000, lambda: talker.key(True))
        texts = [(c["name"], c["text"]) for c in CORPUS] + [(f"inject-{i}", t) for i, t in enumerate(INJECTIONS)]

        def post_all() -> None:
            poster.kept = []
            for i, (_name, text) in enumerate(texts):
                poster.post(text, f"c{i}")
            drain(poster.st, poster.kept)
            for m in poster.kept:
                if m["type"] == "posted" and m["ok"]:
                    name = texts[int(m["clientId"][1:])][0]
                    if name.startswith("inject-"):
                        self.facts["injections"].append(m["msg"])
                    else:
                        self.facts["corpus"][str(m["msg"])] = name
            poster.kept = None

        self.at(14000, post_all)
        self.facts["corpusRoom"] = room.id

    # ----------------------------------------------------------- desk stations

    def desk(self, call: str, grid: str) -> Sim:
        sim = self.add_sim(None, "desk", call, grid)
        sim.presence()
        return sim

    # ----------------------------------------------------------- archive

    def archive(self, kg: Sim) -> None:
        """KG4VCF's history, written as the rooms write it."""
        store = self.store
        rng = random.Random(11)
        now = self.real_start
        sid = kg.sid

        def reader_ids(n: int) -> List[Tuple[str, str, int]]:
            out = []
            for i in range(n):
                call = ["N4RDR", "W5STL", "AA4QQ", "KD8LMN", "VE3XYZ", "G4ABC", "JA1QRP", "EA7FUN"][i] if i < 8 else self.callsign()
                rid = identity.key_id(f"arch-{call}".encode("ascii").ljust(32, b"."))
                store.station_upsert(rid, "core", call, rng.choice(GRIDS), now)
                out.append((rid, call, rng.choice((90, 240, 610, 1180, 1900, 3100, 7400))))
            return out

        def session(room_id: str, start: int, turns: List[Tuple[int, int]], readers: List[Tuple[str, str, int]], lose: float = 0.0) -> int:
            family, hz = protocol.room_id_center(room_id)  # type: ignore[misc]
            opening = store.opening_open(room_id, band_of(hz), family, hz, start)
            for n_turn, (a, b) in enumerate(turns):
                t0 = start + a * 1000
                t1 = start + b * 1000
                turn_id = store.turn_open(opening, "talking", [(sid, "KG4VCF")], t0)
                rows = []
                length = b - a
                for rid, call, km in readers:
                    # Every reader hears the first turn; some miss later ones.
                    if n_turn and rng.random() < 0.25 and len(readers) > 6:
                        continue
                    first = rng.randint(0, max(0, length // 5))
                    last = length - 1 - rng.randint(0, max(0, length // 6))
                    if last <= first:
                        continue
                    present = []
                    gap = rng.random() < lose
                    g0 = rng.randint(first, last)
                    g1 = g0 + rng.randint(8, 40)
                    for s in range(first, last + 1):
                        present.append(not (gap and g0 <= s <= g1))
                    base = rng.uniform(-112, -60)
                    dbm = bytes([max(0, min(254, int(base + rng.uniform(-4, 4)) + 160)) for _ in present])
                    bits = bytearray((len(present) + 7) // 8)
                    for i, p in enumerate(present):
                        if p:
                            bits[i // 8] |= 1 << (i % 8)
                    rows.append((rid, call, rng.randint(-300, 300) // 10 * 10, km, first, last, dbm, bytes(bits), False, rng.random() < 0.12))
                store.tracks_write(turn_id, rows)
                store.turn_end(turn_id, t1)
            sessions = [s for s in store.sessions_of(sid) if s["room_id"] == room_id]
            return int(sessions[-1]["id"])

        busy_start = now - 3 * 3600000
        busy_readers = reader_ids(34)
        busy = session("usb-14230000", busy_start, [(0, 260), (300, 560), (640, 900)], busy_readers, lose=0.3)
        o = store.opening_latest("usb-14230000")
        assert o is not None
        for k, (who, call, text) in enumerate(
            (
                (busy_readers[0][0], "N4RDR", "Solid copy on the **beam**, 57 here"),
                (sid, "KG4VCF", "Thanks all, *QSY* to 14.233 in a minute"),
                (busy_readers[2][0], "AA4QQ", "Fading out here, 73"),
            )
        ):
            store.message_add(o["opening_id"], "usb-14230000", who, call, 1, busy_start + (120 + k * 250) * 1000, text, -84.0)
        self.facts["sessions"]["busy"] = busy
        self.facts["sessions"]["busyReaders"] = len(busy_readers)
        small = [
            ("lsb-7210000", now - 1 * DAY_MS - 2 * 3600000, [(0, 180)], 5),
            ("usb-18130000", now - 3 * DAY_MS, [(0, 120), (200, 300)], 2),
            ("usb-21300000", now - 10 * DAY_MS, [(0, 400)], 8),
            ("lsb-3850000", now - 20 * DAY_MS, [(0, 240)], 3),
        ]
        for room_id, start, turns, n in small:
            session(room_id, start, turns, reader_ids(n), lose=0.2)
        self.facts["sessions"]["empty"] = session("usb-14300000", now - 2 * DAY_MS, [(0, 90)], [])
        old_start = now - 400 * DAY_MS
        old = session("lsb-7200000", old_start, [(0, 200)], reader_ids(2))
        o = store.opening_latest("lsb-7200000")
        assert o is not None
        store.message_add(o["opening_id"], "lsb-7200000", sid, "KG4VCF", 1, old_start + 60000, "A year-old note", None)
        store.message_add(o["opening_id"], "lsb-7200000", busy_readers[1][0], "W5STL", 1, old_start + 90000, "Another year-old note", None)
        store.messages_expire(now - 365 * DAY_MS)
        self.facts["sessions"]["old"] = old

    def old_report(self) -> None:
        """A report waiting more than a day."""
        store = self.store
        now = self.real_start
        sender = identity.key_id(b"page-old-sender".ljust(32, b"."))
        reporter = identity.key_id(b"page-old-reporter".ljust(32, b"."))
        store.station_upsert(sender, "core", "W0OLD", "EN34", now - 3 * DAY_MS)
        store.station_upsert(reporter, "core", "K0RPT", "EN35", now - 3 * DAY_MS)
        opening = store.opening_open("usb-14280000", "20m", "usb", 14280000, now - 2 * DAY_MS)
        msg = store.message_add(opening, "usb-14280000", sender, "W0OLD", 1, now - 2 * DAY_MS + 60000, "Old news, never mind", None)
        store.report_add("message", msg, "offTopic", reporter, now - 26 * 3600000)
        self.facts["reports"]["old"] = msg

    # ----------------------------------------------------------- running

    def run_due(self, until_offset: int) -> None:
        self.events.sort()
        while self.events and self.events[0][0] <= until_offset:
            _, _, fn = self.events.pop(0)
            fn()

    def step(self, ms: int) -> None:
        self.clock.advance(ms)
        off = self.offset_ms()
        self.run_due(off)
        wall = self.clock.wall_ms()
        for sim in self.sims:
            if sim.room is None:
                continue
            if off >= sim.next:
                sim.next = off + sim.period
                sim.frame(wall)
        for room in self.rooms:
            if room.rotate_at is not None and off >= room.rotate_at and len(room.members) > 1:
                talking = [m for m in room.members if m.keyed]
                rest = [m for m in room.members if not m.keyed]
                for m in talking:
                    m.key(False)
                if rest:
                    self.rng.choice(rest).key(True)
                room.rotate_at = off + self.rng.randint(240000, 540000)
        for sim in self.sims:
            drain(sim.st, sim.kept)

    def catch_up(self) -> None:
        real = int(time.time() * 1000)
        while self.clock.wall_ms() < real:
            self.step(min(STEP_MS, real - self.clock.wall_ms()))

    def build(self) -> None:
        began = time.monotonic()
        self.plan_rooms()
        kg = self.desk("KG4VCF", "EM85")
        w8 = self.desk("W8DXR", "EN81")
        self.config.helpers.extend([kg.sid, w8.sid])
        self.facts["kg"] = kg.sid
        self.facts["w8"] = w8.sid
        self.populate()
        print(f"signed in {len(self.sims)} stations in {time.monotonic() - began:.1f} s", file=sys.stderr, flush=True)
        for sim in self.sims:
            if sim.room is not None:
                sim.presence()
        for sim in self.sims:
            drain(sim.st)
        self.archive(kg)
        self.old_report()
        self.kg = kg
        self.w8 = w8
        while self.offset_ms() < HISTORY_MS - STEP_MS and self.events:
            self.step(STEP_MS)
        self.run_due(HISTORY_MS)
        self.catch_up()
        print(f"history done in {time.monotonic() - began:.1f} s", file=sys.stderr, flush=True)
        self.facts["rooms"] = len(self.service.book.live_rooms())
        self.facts["stations"] = len(self.sims)

    # ----------------------------------------------------------- in-process web

    async def call(self, method: str, target: str, cookie: Optional[str] = None, form: Optional[Dict[str, str]] = None) -> Tuple[int, Any, Dict[str, str]]:
        path, _, query = target.partition("?")
        headers: Dict[str, List[str]] = {"origin": [self.web.origin]}
        body = b""
        if cookie:
            headers["cookie"] = [cookie]
        if form is not None:
            headers["content-type"] = ["application/x-www-form-urlencoded"]
            body = urlencode(form).encode("utf-8")
        req = Request(method, path, split_path(path) or [], parse_qs(query, keep_blank_values=True), headers, body, "127.0.0.9")
        resp = await self.app.answer(req)
        heads = {k.lower(): v for k, v in resp.headers}
        try:
            data = json.loads(resp.body.decode("utf-8")) if resp.body else None
        except ValueError:
            data = None
        return resp.status, data, heads

    def link(self, sim: Sim) -> str:
        url, _ = self.service.make_weblink(sim.sid)
        return url[len(f"https://{PUBLIC_HOST}") :]

    async def helper_cookie(self, sim: Sim) -> Tuple[str, str]:
        if sim.sid not in self.cookies:
            status, _, heads = await self.call("GET", self.link(sim))
            assert status == 303, status
            cookie = heads["set-cookie"].split(";", 1)[0]
            _, me, _ = await self.call("GET", "/v1/me", cookie)
            self.cookies[sim.sid] = (cookie, me["csrf"])
        return self.cookies[sim.sid]

    async def other_helper(self, act: str, report: int, until: Optional[str]) -> Any:
        cookie, csrf = await self.helper_cookie(self.w8)
        _, listing, _ = await self.call("GET", "/v1/review/reports", cookie)
        entry = next(e for e in listing["reports"] if e["id"] == report)
        form = {"csrf": csrf, "report": str(report), "version": str(entry["version"])}
        if until is not None:
            form["until"] = until
        status, data, _ = await self.call("POST", f"/v1/review/{act}", cookie, form)
        return {"status": status, "body": data}

    async def control(self, target: str) -> Tuple[int, Any]:
        path, _, query = target.partition("?")
        q = {k: v[-1] for k, v in parse_qs(query).items()}
        name = path[len("/__test/") :]
        if name == "facts":
            return 200, self.facts
        if name == "signin":
            # 2026-10-30: real owner-follow fixture identity, retaining desks.
            # J.J. Boyd / KG4VCF, AI-assisted via Codex.
            who = q.get("who", "KG4VCF")
            sim = self.w8 if who == "W8DXR" else self.kg
            if who not in ("KG4VCF", "W8DXR"):
                sim = next((candidate for candidate in self.sims if candidate.call == who), None)
                if sim is None:
                    return 404, {"error": "station-not-found"}
            return 200, {"path": self.link(sim)}
        if name == "history":
            # Disposable fixture-owned records only, using real service/store
            # mutation paths; this is not a production authorization bypass.
            ident = int(q.get("msg", "0"))
            row = self.store.message_get(ident)
            sender = next((sim for sim in self.sims if row and sim.sid == row["station_id"]), None)
            if sender is None:
                return 404, {"error": "fixture-message-not-found"}
            action = q.get("act")
            with self.service.event():
                if action == "delete":
                    sender.st.send({"type": "delete", "msg": ident})
                elif action == "remove":
                    self.store.message_remove(ident, self.kg.sid)
                    self.service.message_changed(ident)
                elif action == "expire":
                    self.store.messages_expire(int(row["t"]) + 1)
                else:
                    return 400, {"error": "unknown-fixture-action"}
            return 200, {"id": ident, "state": self.store.message_get(ident)["state"]}
        if name == "live":
            do = q.get("do", "")
            if do == "drop":
                for conn in list(self.service.connections):
                    if conn.path == LIVE:
                        conn.close(protocol.CLOSE_GOING_AWAY)
            self.refuse_live = do in ("refuse", "drop-refuse")
            if do == "drop-refuse":
                for conn in list(self.service.connections):
                    if conn.path == LIVE:
                        conn.close(protocol.CLOSE_GOING_AWAY)
            return 200, {"refusing": self.refuse_live}
        if name == "helper":
            return 200, await self.other_helper(q["act"], int(q["report"]), q.get("until"))
        return 404, {"error": "not-found"}


async def pipe(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    try:
        while True:
            data = await reader.read(65536)
            if not data:
                break
            writer.write(data)
            await writer.drain()
    except (ConnectionError, asyncio.CancelledError):
        pass
    finally:
        try:
            writer.close()
        except Exception:  # noqa: BLE001 - closing a socket the peer dropped
            pass


async def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=0)
    args = parser.parse_args()
    logging.basicConfig(stream=sys.stderr, level=logging.WARNING, format="%(levelname)s %(message)s")
    for name in ("websockets", "websockets.quiet", "asyncio", "nereus_roundtable"):
        logging.getLogger(name).setLevel(logging.CRITICAL + 1)

    holder: Dict[str, Any] = {}

    async def front(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        world: World = holder["world"]
        try:
            head = await reader.readuntil(b"\r\n\r\n")
        except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError):
            writer.close()
            return
        line = head.split(b"\r\n", 1)[0].decode("latin-1")
        parts = line.split(" ")
        target = parts[1] if len(parts) > 2 else "/"
        path = target.split("?", 1)[0]
        if path.startswith("/__test/"):
            status, data = await world.control(target)
            body = json.dumps(data).encode("utf-8")
            writer.write(
                f"HTTP/1.1 {status} X\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n".encode("latin-1")
                + body
            )
            await writer.drain()
            writer.close()
            return
        if path in ("/v1/live", "/v1/station"):
            if world.refuse_live:
                writer.close()
                return
            port = holder["listeners"].ws_port()
        else:
            port = holder["listeners"].http_port()
        try:
            up_reader, up_writer = await asyncio.open_connection("127.0.0.1", port)
        except OSError:
            writer.close()
            return
        up_writer.write(head)
        await asyncio.gather(pipe(reader, up_writer), pipe(up_reader, writer))

    server = await asyncio.start_server(front, "127.0.0.1", args.port)
    port = server.sockets[0].getsockname()[1]
    world = World(port)
    holder["world"] = world
    world.build()
    listeners = await transport.start(world.service, world.app, world.config)
    holder["listeners"] = listeners
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    loop.add_signal_handler(signal.SIGTERM, stop.set)
    loop.add_signal_handler(signal.SIGINT, stop.set)

    def stdin_closed() -> None:
        if not sys.stdin.read(1):
            loop.remove_reader(sys.stdin.fileno())
            stop.set()

    try:
        loop.add_reader(sys.stdin.fileno(), stdin_closed)
    except (OSError, ValueError):
        pass

    async def ticker() -> None:
        while not stop.is_set():
            await asyncio.sleep(0.1)
            world.catch_up()

    tick = asyncio.ensure_future(ticker())
    print(f"READY http://localhost:{port}", flush=True)
    await stop.wait()
    tick.cancel()
    server.close()
    await transport.stop(listeners, world.service, 0.1, 2.0)
    world.store.close()
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
