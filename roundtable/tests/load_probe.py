# no-port-check: NereusSDR-original.
"""The Roundtable load probe (design R-RT-9): the real server in its own
process, loaded over loopback WebSockets by client processes, and measured.

    python3 load_probe.py --stations 500 --rooms 150 --viewers 500 --minutes 30

Not a pytest module (no test_ prefix). It runs three kinds of process from
this one file:

- the server (`--serve CONFIG`): the same objects `python3 -m
  nereus_roundtable` builds, on a real clock and a database file in a
  temporary folder, with the default limits. Every 10 s it prints one JSON
  line of what it holds and how it keeps up: its CPU seconds, its resident
  memory, the earlier turns it keeps in memory (in all, for rooms that have
  closed, and past the 30 minutes a room shows them), the shared band-list
  tick (how often it ran, how long it took, the longest gap between two),
  each `unread_mentions` call a sign-in makes, and how late the event loop
  ran a 100 ms timer. At the end it prints its peak resident memory
  (VmHWM from /proc/self/status on Linux, `resource.getrusage` elsewhere)
  and its CPU seconds.
- client workers (`--worker SPEC`): each plays its share of the stations
  and the live viewers. A station signs in with its own key from its own
  address (an X-Forwarded-For the server believes from loopback, so every
  station is its own address group as behind Caddy), sends a presence
  every 10 s and a reading once a second, and posts now and then, half of
  its posts naming another station. In each room the members take turns
  keying for 30 to 90 s, and now and then the whole room moves to another
  frequency, so rooms close and earlier turns are left behind. A station
  signs in again every 15 minutes on average, which makes the server look
  up its unread mentions. A viewer watches one band (or all of them) and
  up to 4 of its rooms, and picks again every 3 to 6 minutes.
- the controller (no mode flag): writes the configuration, starts the
  server and the workers, and prints the summary when the time is up. It
  exits 1 when the server's peak memory reaches 256 MB or its CPU averaged
  over the run reaches half of one core.

`--mentions-bench N` times the `unread_mentions` query alone against a
database holding N messages, the cost a sign-in pays once the table has
grown.

Every process stops on its own at the deadline; the controller ends the
others if it is interrupted.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import random
import resource
import signal
import sqlite3
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT / "server"))
sys.path.insert(0, str(HERE))

PUBLIC_HOST = "roundtable.load.invalid"
STATS_S = 10.0
LAG_TIMER_S = 0.1
PRESENCE_S = 10.0
PRESENCE_GAP_S = 1.05
READING_S = 1.0
POST_MEAN_S = 240.0
RECONNECT_MEAN_S = 900.0
QSY_MEAN_S = 720.0
TURN_S = (30.0, 90.0)
GAP_S = (2.0, 8.0)
JOIN_DWELL_S = 11.0
VIEW_PICK_S = (180.0, 360.0)
SLOTS_PER_ROOM = 4
SLOT_HZ = 5000
RSS_LIMIT_BYTES = 256 * 1024 * 1024
CPU_LIMIT = 0.5
STEADY_AFTER_S = 60.0

# Band, low edge, high edge, phone mode: wide enough for 4 frequencies a
# room 5 kHz apart.
BANDS = [
    ("160m", 1800000, 2000000, "LSB"),
    ("80m", 3500000, 4000000, "LSB"),
    ("40m", 7000000, 7300000, "LSB"),
    ("20m", 14000000, 14350000, "USB"),
    ("17m", 18068000, 18168000, "USB"),
    ("15m", 21000000, 21450000, "USB"),
    ("12m", 24890000, 24990000, "USB"),
    ("10m", 28000000, 29700000, "USB"),
    ("6m", 50000000, 54000000, "USB"),
    ("2m", 144000000, 148000000, "USB"),
]

PHRASES = [
    "Good copy here, 59 into EM73",
    "Thanks for the QSO, 73",
    "Anyone hear the DX on the low end?",
    "Antenna is a dipole at 40 ft",
    "QRZ? Listening for a few minutes",
    "Band is opening to the west coast",
    "Running 100 W, nothing fancy",
    "Back in 10, 73",
]
GRIDS = ["EM73", "FN31", "DM79", "EN52", "FM18", "CN87", "EL96", "IO91", "JO62", "PM95", "QF56", "FN20", "EM12"]
LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def raise_file_limit() -> None:
    """A thousand sockets need more descriptors than macOS's default 256."""
    soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    want = 8192 if hard == resource.RLIM_INFINITY else min(hard, 8192)
    if soft < want:
        try:
            resource.setrlimit(resource.RLIMIT_NOFILE, (want, hard))
        except (ValueError, OSError):
            pass


def callsign_of(index: int) -> str:
    n = index // 10
    tail = LETTERS[n % 26] + LETTERS[(n // 26) % 26] + LETTERS[(n // 676) % 26]
    return f"K{index % 10}{tail}"


def station_address(index: int) -> str:
    return f"10.{index // 250 % 250}.{index % 250}.1"


def viewer_address(index: int) -> str:
    return f"10.{100 + index // 250 % 150}.{index % 250}.2"


def plan_rooms(count: int) -> List[Dict[str, Any]]:
    """`count` rooms, each with 4 frequencies of its own 5 kHz apart, spread
    over the bands by how many each holds."""
    slots: List[Tuple[str, int, str]] = []
    per_band: List[List[Tuple[str, int, str]]] = []
    for band, lo, hi, mode in BANDS:
        fits = (hi - lo - 2 * SLOT_HZ) // (SLOT_HZ * SLOTS_PER_ROOM)
        per_band.append([(band, lo + SLOT_HZ + k * SLOT_HZ * SLOTS_PER_ROOM, mode) for k in range(fits)])
    # Round robin over the bands, so every band gets rooms.
    while len(slots) < count and any(per_band):
        for queue in per_band:
            if queue and len(slots) < count:
                slots.append(queue.pop(0))
    if len(slots) < count:
        raise SystemExit(f"load_probe: at most {len(slots)} rooms fit the band plan")
    rooms = []
    for i, (band, base, mode) in enumerate(slots):
        rooms.append({"index": i, "band": band, "mode": mode, "hz": [base + k * SLOT_HZ for k in range(SLOTS_PER_ROOM)]})
    return rooms


def peak_rss_bytes() -> int:
    try:
        with open("/proc/self/status", "r", encoding="ascii") as status:
            for line in status:
                if line.startswith("VmHWM:"):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    # macOS gives bytes, Linux kilobytes.
    return int(peak) if sys.platform == "darwin" else int(peak) * 1024


def current_rss_bytes() -> Optional[int]:
    try:
        with open("/proc/self/status", "r", encoding="ascii") as status:
            for line in status:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    return None


def pct(values: List[float], share: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(share * len(ordered)))]


def emit(obj: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(obj, separators=(",", ":")) + "\n")
    sys.stdout.flush()


# ===================================================================== server


class Meter:
    """What the server process measures about itself between two lines."""

    def __init__(self) -> None:
        self.tick_ms: List[float] = []
        self.tick_gap_ms: List[float] = []
        self.last_tick: Optional[float] = None
        self.mention_ms: List[float] = []
        self.mention_rows = 0
        self.lag_ms: List[float] = []

    def take(self) -> Dict[str, Any]:
        out = {
            "ticks": len(self.tick_ms),
            "tickMsMax": round(max(self.tick_ms, default=0.0), 2),
            "tickMsP99": round(pct(self.tick_ms, 0.99), 2),
            "tickMsMean": round(sum(self.tick_ms) / len(self.tick_ms), 2) if self.tick_ms else 0.0,
            "tickGapMsMax": round(max(self.tick_gap_ms, default=0.0), 1),
            "mentionCalls": len(self.mention_ms),
            "mentionMsMax": round(max(self.mention_ms, default=0.0), 3),
            "mentionMsMean": round(sum(self.mention_ms) / len(self.mention_ms), 3) if self.mention_ms else 0.0,
            "mentionRows": self.mention_rows,
            "lagMsMax": round(max(self.lag_ms, default=0.0), 1),
            "lagMsP99": round(pct(self.lag_ms, 0.99), 1),
        }
        self.tick_ms, self.tick_gap_ms, self.mention_ms, self.lag_ms = [], [], [], []
        self.mention_rows = 0
        return out


def instrument(service: Any, store: Any, meter: Meter) -> None:
    """Wrap the band-list tick and the mentions lookup on these instances
    only; the server's code is unchanged."""
    band_tick = service._band_tick

    def timed_tick() -> None:
        start = time.perf_counter()
        if meter.last_tick is not None:
            meter.tick_gap_ms.append((start - meter.last_tick) * 1000)
        meter.last_tick = start
        band_tick()
        meter.tick_ms.append((time.perf_counter() - start) * 1000)

    service._band_tick = timed_tick
    unread = store.unread_mentions

    def timed_unread(station_id: str, limit: int = 50) -> List[Dict[str, Any]]:
        start = time.perf_counter()
        rows = unread(station_id, limit)
        meter.mention_ms.append((time.perf_counter() - start) * 1000)
        meter.mention_rows += len(rows)
        return rows

    store.unread_mentions = timed_unread


def held(service: Any, store: Any) -> Dict[str, Any]:
    from nereus_roundtable.rooms import EARLIER_MS

    book = service.book
    now = service.clock.wall_ms()
    turns = closed_turns = stale = closed_keys = 0
    for room_id, kept in book.earlier.items():
        room = book.rooms.get(room_id)
        is_closed = room is None or not room.live
        turns += len(kept)
        if is_closed:
            closed_keys += 1
            closed_turns += len(kept)
        stale += sum(1 for t in kept if t.end is not None and t.end < now - EARLIER_MS)
    messages = store.db.execute("SELECT count(*) FROM messages").fetchone()[0]
    return {
        "earlierKeys": len(book.earlier),
        "earlierTurns": turns,
        "earlierClosedKeys": closed_keys,
        "earlierClosedTurns": closed_turns,
        "earlierStale": stale,
        "rooms": len(book.rooms),
        "liveRooms": sum(1 for r in book.rooms.values() if r.live),
        "stations": len(service.stations),
        "connections": len(service.connections),
        "messages": int(messages),
        "bandBuilds": int(getattr(service, "band_builds", 0)),
    }


async def serve(config_path: str) -> int:
    from nereus_roundtable import transport
    from nereus_roundtable.__main__ import configure_logging
    from nereus_roundtable.clock import RealClock
    from nereus_roundtable.config import load
    from nereus_roundtable.http import App
    from nereus_roundtable.moderation import Moderation
    from nereus_roundtable.service import Service
    from nereus_roundtable.store import Store
    from nereus_roundtable.web import Web

    raise_file_limit()
    config = load(config_path)
    configure_logging(config.log_level)
    store = Store(config.database)
    service = Service(config, RealClock(), store)
    app = App(service, store, config)
    Moderation(Web(service, store, config, app), app)
    meter = Meter()
    instrument(service, store, meter)
    listeners = await transport.start(service, app, config)
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, stop.set)
    wall0 = time.monotonic()
    cpu0 = time.process_time()
    emit({"ready": True, "ws": listeners.ws_port(), "http": listeners.http_port(), "cpu": cpu0})

    async def lag() -> None:
        while True:
            start = loop.time()
            await asyncio.sleep(LAG_TIMER_S)
            meter.lag_ms.append(max(0.0, (loop.time() - start - LAG_TIMER_S) * 1000))

    async def stats() -> None:
        while True:
            await asyncio.sleep(STATS_S)
            line = {"t": round(time.monotonic() - wall0, 1), "cpu": round(time.process_time(), 3)}
            line["rss"] = current_rss_bytes()
            line["peak"] = peak_rss_bytes()
            line.update(held(service, store))
            line.update(meter.take())
            emit(line)

    tasks = [asyncio.ensure_future(lag()), asyncio.ensure_future(stats())]
    await stop.wait()
    for task in tasks:
        task.cancel()
    # How a sign-in's lookup costs against the table as it now stands.
    # The clients have gone by now, so the stations come from the table.
    sample = [row[0] for row in store.db.execute("SELECT id FROM stations WHERE callsign IS NOT NULL LIMIT 50")]
    costs = []
    for sid in sample:
        start = time.perf_counter()
        store.unread_mentions(sid)
        costs.append((time.perf_counter() - start) * 1000)
    final = {
        "final": True,
        "wall": round(time.monotonic() - wall0, 2),
        "cpu": round(time.process_time(), 3),
        "cpuAtReady": round(cpu0, 3),
        "peak": peak_rss_bytes(),
        "endMentionMsMax": round(max(costs, default=0.0), 3),
        "endMentionMsMean": round(sum(costs) / len(costs), 3) if costs else 0.0,
    }
    final.update(held(service, store))
    emit(final)
    await transport.stop(listeners, service, grace_s=0.2, timeout_s=10)
    store.close()
    return 0


# ===================================================================== clients


class RoomState:
    def __init__(self, plan: Dict[str, Any], rng: random.Random) -> None:
        self.plan = plan
        self.rng = rng
        self.slot = 0
        self.members: List["StationClient"] = []
        self.talker: Optional["StationClient"] = None

    @property
    def hz(self) -> int:
        return int(self.plan["hz"][self.slot])

    @property
    def mode(self) -> str:
        return str(self.plan["mode"])

    async def run(self, deadline: float, counts: Dict[str, int]) -> None:
        """Members take turns keying; between two turns the room may move."""
        await asyncio.sleep(self.rng.uniform(JOIN_DWELL_S + 2, JOIN_DWELL_S + 30))
        turn = 0
        while time.time() < deadline:
            live = [m for m in self.members if m.signed_in]
            if live:
                talker = live[turn % len(live)]
                turn += 1
                self.talker = talker
                talker.keyed = True
                talker.dirty = True
                await asyncio.sleep(self.rng.uniform(*TURN_S))
                talker.keyed = False
                talker.dirty = True
                self.talker = None
                counts["turns"] += 1
            gap = self.rng.uniform(*GAP_S)
            if self.rng.random() < (gap + sum(TURN_S) / 2) / QSY_MEAN_S:
                self.slot = (self.slot + 1) % SLOTS_PER_ROOM
                counts["qsy"] += 1
                for m in self.members:
                    m.dirty = True
                    m.joined_at = time.time()
            await asyncio.sleep(gap)


class StationClient:
    def __init__(self, worker: "Worker", index: int, room: RoomState) -> None:
        from cryptography.hazmat.primitives.asymmetric import ec

        self.worker = worker
        self.index = index
        self.room = room
        self.call = callsign_of(index)
        self.grid = GRIDS[index % len(GRIDS)]
        self.key = ec.generate_private_key(ec.SECP256R1())
        self.signed_in = False
        self.keyed = False
        self.dirty = False
        self.joined_at = 0.0
        self.posts = 0

    async def sign_in(self) -> Any:
        from nereus_roundtable import identity, protocol
        from runner import ws_connect

        ws = await ws_connect(self.worker.base + "/v1/station", station_address(self.index))
        challenge = json.loads(await asyncio.wait_for(ws.recv(), 30))
        nonce = identity.from_b64url(challenge["nonce"])
        sig = identity.sign_raw(self.key, identity.hello_transcript(PUBLIC_HOST, nonce))
        await ws.send(
            protocol.encode(
                {
                    "type": "hello",
                    "v": 1,
                    "software": "NereusSDR",
                    "softwareVersion": "2026.10.1",
                    "spki": identity.to_b64url(identity.spki_of(self.key.public_key())),
                    "sig": identity.to_b64url(sig),
                }
            )
        )
        welcome = json.loads(await asyncio.wait_for(ws.recv(), 30))
        if welcome.get("type") != "welcome":
            raise RuntimeError(f"sign-in answered {welcome.get('type')}")
        self.worker.counts["mentionsAtWelcome"] += len(welcome.get("mentions", []))
        return ws

    def presence(self) -> Dict[str, Any]:
        room = self.room
        mode = room.mode
        return {
            "type": "presence",
            "callsign": self.call,
            "grid": self.grid,
            "atRadio": True,
            "slices": [{"id": 0, "hz": room.hz, "mode": mode, "filterHz": 2700, "keyed": self.keyed}],
        }

    def post_text(self) -> str:
        rng = self.worker.rng
        text = rng.choice(PHRASES)
        if rng.random() < 0.5:
            others = [m for m in self.room.members if m is not self] or self.worker.stations
            text = f"{rng.choice(others).call} {text}"
            self.worker.counts["mentionPosts"] += 1
        return text

    async def read(self, ws: Any) -> None:
        counts = self.worker.counts
        try:
            async for text in ws:
                counts["stationFrames"] += 1
                counts["stationBytes"] += len(text)
                head = text[:40]
                if head.startswith('{"type":"error"'):
                    code = json.loads(text).get("code", "?")
                    counts["error:" + str(code)] = counts.get("error:" + str(code), 0) + 1
                elif head.startswith('{"type":"posted"'):
                    msg = json.loads(text)
                    key = "posted:" + str(msg.get("cause") or "ok")
                    counts[key] = counts.get(key, 0) + 1
        except Exception:  # noqa: BLE001 - a closed socket ends the reader
            return

    async def run(self, start_at: float, deadline: float) -> None:
        from nereus_roundtable import protocol

        rng = self.worker.rng
        counts = self.worker.counts
        await asyncio.sleep(max(0.0, start_at - time.time()))
        first = True
        while time.time() < deadline:
            try:
                async with self.worker.signing:
                    ws = await self.sign_in()
            except Exception as exc:  # noqa: BLE001 - counted and retried
                counts["signInFailed"] += 1
                counts["lastFailure"] = type(exc).__name__  # type: ignore[assignment]
                await asyncio.sleep(5)
                continue
            counts["signIns"] += 1
            if first:
                first = False
                self.worker.signed += 1
            self.signed_in = True
            self.joined_at = time.time()
            reader = asyncio.ensure_future(self.read(ws))
            now = time.time()
            next_presence = now
            last_presence = 0.0
            next_reading = now + 1
            next_post = now + rng.expovariate(1 / POST_MEAN_S)
            leave_at = now + rng.expovariate(1 / RECONNECT_MEAN_S)
            try:
                while time.time() < min(deadline, leave_at) and not reader.done():
                    now = time.time()
                    # One presence a second at most (the server's limit).
                    if (now >= next_presence or self.dirty) and now - last_presence >= PRESENCE_GAP_S:
                        self.dirty = False
                        await ws.send(protocol.encode(self.presence()))
                        counts["presence"] += 1
                        last_presence = now
                        next_presence = now + PRESENCE_S
                    if now >= next_reading:
                        next_reading += READING_S
                        if not self.keyed and now - self.joined_at >= JOIN_DWELL_S:
                            dbm = round(-110 + (self.index % 50) + rng.uniform(-3, 3), 1)
                            present = self.room.talker is not None
                            reading = {"type": "reading", "slice": 0, "dbm": dbm, "t": int(now * 1000), "present": present}
                            await ws.send(protocol.encode(reading))
                            counts["readings"] += 1
                    if now >= next_post:
                        next_post = now + rng.expovariate(1 / POST_MEAN_S)
                        if now - self.joined_at >= JOIN_DWELL_S + 1:
                            self.posts += 1
                            room_id = f"{self.room.mode.lower()}-{self.room.hz}"
                            post = {"type": "post", "room": room_id, "text": self.post_text(), "clientId": f"c{self.index}-{self.posts}"}
                            await ws.send(protocol.encode(post))
                            counts["posts"] += 1
                    await asyncio.sleep(max(0.0, min(next_reading, next_presence) - time.time()))
            except Exception:  # noqa: BLE001 - the socket closed under it
                counts["sendFailed"] += 1
            self.signed_in = False
            if reader.done():
                counts["closedByServer"] += 1
            try:
                await ws.close()
            except Exception:  # noqa: BLE001
                pass
            reader.cancel()
            if time.time() < deadline and time.time() >= leave_at:
                counts["reconnects"] += 1


class ViewerClient:
    def __init__(self, worker: "Worker", index: int, bands: List[str]) -> None:
        self.worker = worker
        self.index = index
        self.bands = bands
        self.watched: List[str] = []
        self.want = 1 + index % 4
        self.need_pick = False

    async def read(self, ws: Any) -> None:
        from nereus_roundtable import protocol

        counts = self.worker.counts
        try:
            async for text in ws:
                counts["viewerFrames"] += 1
                counts["viewerBytes"] += len(text)
                if self.need_pick and text.startswith('{"type":"rooms",'):
                    ids = [r["id"] for r in json.loads(text).get("rooms", [])]
                    self.worker.rng.shuffle(ids)
                    self.need_pick = False
                    for room_id in self.watched:
                        await ws.send(protocol.encode({"type": "unwatch", "room": room_id}))
                    self.watched = ids[: self.want]
                    for room_id in self.watched:
                        await ws.send(protocol.encode({"type": "watch", "room": room_id}))
                        counts["watches"] += 1
                elif text.startswith('{"type":"error"'):
                    code = json.loads(text).get("code", "?")
                    counts["viewerError:" + str(code)] = counts.get("viewerError:" + str(code), 0) + 1
        except Exception:  # noqa: BLE001 - a closed socket ends the reader
            return

    async def run(self, start_at: float, deadline: float) -> None:
        from nereus_roundtable import protocol
        from runner import ws_connect

        rng = self.worker.rng
        counts = self.worker.counts
        await asyncio.sleep(max(0.0, start_at - time.time()))
        while time.time() < deadline:
            try:
                ws = await ws_connect(self.worker.base + "/v1/live", viewer_address(self.index))
            except Exception:  # noqa: BLE001 - counted and retried
                counts["viewerConnectFailed"] += 1
                await asyncio.sleep(5)
                continue
            counts["viewerConnects"] += 1
            self.watched = []
            reader = asyncio.ensure_future(self.read(ws))
            try:
                while time.time() < deadline and not reader.done():
                    band = rng.choice(self.bands + ["all"])
                    self.need_pick = True
                    await ws.send(protocol.encode({"type": "watchBand", "band": band}))
                    wait = min(rng.uniform(*VIEW_PICK_S), deadline - time.time())
                    end = time.time() + max(0.0, wait)
                    while time.time() < end and not reader.done():
                        await asyncio.sleep(min(1.0, max(0.0, end - time.time())))
            except Exception:  # noqa: BLE001 - the socket closed under it
                pass
            if reader.done() and time.time() < deadline:
                counts["viewerClosedByServer"] += 1
            try:
                await ws.close()
            except Exception:  # noqa: BLE001
                pass
            reader.cancel()


class Worker:
    def __init__(self, spec: Dict[str, Any]) -> None:
        self.spec = spec
        self.rng = random.Random(spec["seed"])
        self.base = f"ws://127.0.0.1:{spec['ws']}"
        self.counts: Dict[str, int] = {
            key: 0
            for key in (
                "signIns",
                "signInFailed",
                "reconnects",
                "closedByServer",
                "sendFailed",
                "presence",
                "readings",
                "posts",
                "mentionPosts",
                "mentionsAtWelcome",
                "turns",
                "qsy",
                "stationFrames",
                "stationBytes",
                "viewerConnects",
                "viewerConnectFailed",
                "viewerClosedByServer",
                "viewerFrames",
                "viewerBytes",
                "watches",
            )
        }
        self.signing = asyncio.Semaphore(8)
        self.signed = 0
        self.rooms = [RoomState(plan, random.Random(spec["seed"] * 1000 + plan["index"])) for plan in spec["rooms"]]
        by_index = {room.plan["index"]: room for room in self.rooms}
        self.stations: List[StationClient] = []
        for index, room_index in spec["stations"]:
            st = StationClient(self, index, by_index[room_index])
            by_index[room_index].members.append(st)
            self.stations.append(st)
        bands = sorted({plan["band"] for plan in spec["allRooms"]})
        self.viewers = [ViewerClient(self, index, bands) for index in spec["viewers"]]

    async def run(self) -> None:
        start = float(self.spec["start"])
        deadline = float(self.spec["deadline"])
        ramp = float(self.spec["ramp"])
        tasks = []
        n = max(1, len(self.stations))
        for i, st in enumerate(self.stations):
            tasks.append(asyncio.ensure_future(st.run(start + ramp * i / n, deadline)))
        m = max(1, len(self.viewers))
        for i, viewer in enumerate(self.viewers):
            tasks.append(asyncio.ensure_future(viewer.run(start + ramp + 5 + 20 * i / m, deadline)))
        for room in self.rooms:
            tasks.append(asyncio.ensure_future(room.run(deadline, self.counts)))
        announced = False
        while time.time() < deadline:
            await asyncio.sleep(1)
            if not announced and self.signed >= len(self.stations):
                announced = True
                emit({"signed": True, "worker": self.spec["worker"]})
        await asyncio.wait(tasks, timeout=3)
        for task in tasks:
            task.cancel()
        self.counts["cpu"] = round(time.process_time(), 2)  # type: ignore[assignment]
        emit({"done": True, "worker": self.spec["worker"], "counts": self.counts})


async def work(spec_path: str) -> int:
    raise_file_limit()
    spec = json.loads(Path(spec_path).read_text("utf-8"))
    await Worker(spec).run()
    return 0


# ===================================================================== bench


def mentions_bench(count: int) -> int:
    """Time unread_mentions for stations with and without mentions against
    a table of `count` messages, as a year's posting would leave it."""
    from nereus_roundtable import messages
    from nereus_roundtable.store import Store

    with tempfile.TemporaryDirectory(prefix="rt-mentions-") as folder:
        store = Store(os.path.join(folder, "bench.db"))
        rng = random.Random(3)
        calls = [callsign_of(i) for i in range(2000)]
        rows = []
        t0 = 1800000000000
        for i in range(count):
            text = rng.choice(PHRASES)
            if rng.random() < 0.5:
                text = f"{rng.choice(calls)} {text}"
            words = messages.words(text)
            rows.append((1, "usb-14250000", f"s{i % 2000}", calls[i % 2000], 1, t0 + i * 1000, text, "shown", None, None, " " + " ".join(words) + " "))
        store.db.executemany(
            "INSERT INTO messages (opening_id, room_id, station_id, callsign, nth, t, text, state, dbm, removed_by, mentions)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            rows,
        )
        for sid, call in (("quiet", "N0QQQ"), ("named", calls[7])):
            store.db.execute(
                "INSERT INTO stations (id, kind, callsign, first_seen, last_seen) VALUES (?, 'core', ?, ?, ?)",
                (sid, call, t0, t0),
            )
        store.db.commit()
        out: Dict[str, Any] = {"messages": count}
        for sid in ("quiet", "named"):
            costs = []
            for _ in range(5):
                start = time.perf_counter()
                store.unread_mentions(sid)
                costs.append((time.perf_counter() - start) * 1000)
            out[sid + "Ms"] = round(min(costs), 2)
        store.close()
    emit(out)
    return 0


# ===================================================================== controller


def write_config(folder: Path) -> Path:
    path = folder / "roundtable.conf"
    path.write_text(
        "[roundtable]\n"
        "ws_listen = 127.0.0.1:0\n"
        "http_listen = 127.0.0.1:0\n"
        f"public_host = {PUBLIC_HOST}\n"
        f"database = {folder / 'roundtable.db'}\n"
        f"backup_dir = {folder / 'backup'}\n"
        f"static_dir = {ROOT / 'server' / 'static'}\n"
        "log_level = warning\n",
        "utf-8",
    )
    return path


async def control(args: argparse.Namespace) -> int:
    folder = Path(tempfile.mkdtemp(prefix="rt-load-"))
    config = write_config(folder)
    me = str(Path(__file__).resolve())
    server = await asyncio.create_subprocess_exec(sys.executable, me, "--serve", str(config), stdout=subprocess.PIPE)
    assert server.stdout is not None
    ready = json.loads((await asyncio.wait_for(server.stdout.readline(), 60)).decode())
    rooms = plan_rooms(args.rooms)
    workers = max(1, args.workers)
    start = time.time() + 2
    deadline = start + args.minutes * 60
    procs = []
    for w in range(workers):
        mine = [r for r in rooms if r["index"] % workers == w]
        ids = {r["index"] for r in mine}
        spec = {
            "worker": w,
            "seed": args.seed * 100 + w,
            "ws": ready["ws"],
            "start": start,
            "deadline": deadline,
            "ramp": args.ramp,
            "rooms": mine,
            "allRooms": rooms,
            "stations": [[i, i % args.rooms] for i in range(args.stations) if i % args.rooms in ids],
            "viewers": [v for v in range(args.viewers) if v % workers == w],
        }
        spec_path = folder / f"worker-{w}.json"
        spec_path.write_text(json.dumps(spec), "utf-8")
        procs.append(await asyncio.create_subprocess_exec(sys.executable, me, "--worker", str(spec_path), stdout=subprocess.PIPE))

    lines: List[Dict[str, Any]] = []
    signed_at: Dict[int, float] = {}
    done: Dict[int, Dict[str, Any]] = {}

    async def follow_server() -> None:
        assert server.stdout is not None
        while True:
            raw = await server.stdout.readline()
            if not raw:
                return
            line = json.loads(raw.decode())
            lines.append(line)
            if not line.get("final") and args.verbose:
                print("server " + json.dumps(line), flush=True)

    async def follow_worker(w: int, proc: Any) -> None:
        while True:
            raw = await proc.stdout.readline()
            if not raw:
                return
            line = json.loads(raw.decode())
            if line.get("signed"):
                signed_at[w] = time.time()
            if line.get("done"):
                done[w] = line["counts"]

    followers = [asyncio.ensure_future(follow_server())]
    followers += [asyncio.ensure_future(follow_worker(w, p)) for w, p in enumerate(procs)]
    try:
        for proc in procs:
            await proc.wait()
    finally:
        for proc in procs:
            if proc.returncode is None:
                proc.terminate()
        if server.returncode is None:
            server.send_signal(signal.SIGTERM)
        await asyncio.wait_for(server.wait(), 60)
        await asyncio.wait(followers, timeout=10)
    final = next((line for line in lines if line.get("final")), None)
    periodic = [line for line in lines if "t" in line and not line.get("final")]
    return report(args, ready, final, periodic, signed_at, done, start, folder)


def report(
    args: argparse.Namespace,
    ready: Dict[str, Any],
    final: Optional[Dict[str, Any]],
    periodic: List[Dict[str, Any]],
    signed_at: Dict[int, float],
    done: Dict[int, Dict[str, Any]],
    start: float,
    folder: Path,
) -> int:
    if final is None:
        print("load_probe: the server printed no final line", file=sys.stderr)
        return 1
    cpu_run = (final["cpu"] - final["cpuAtReady"]) / max(1.0, final["wall"])
    all_signed = max(signed_at.values()) - start if len(signed_at) == len(done) and signed_at else None
    steady = None
    if all_signed is not None:
        # From a stats line a minute after the last sign-in to the end.
        offset = all_signed + 2 + STEADY_AFTER_S
        first = next((p for p in periodic if p["t"] >= offset), None)
        if first is not None and final["wall"] > first["t"]:
            steady = (final["cpu"] - first["cpu"]) / (final["wall"] - first["t"])
    counts: Dict[str, Any] = {}
    for c in done.values():
        for key, value in c.items():
            if isinstance(value, (int, float)):
                counts[key] = counts.get(key, 0) + value
    ticks = sum(p["ticks"] for p in periodic)
    gaps = [p["tickGapMsMax"] for p in periodic if p["ticks"]]
    mention_calls = sum(p["mentionCalls"] for p in periodic)
    mention_max = max((p["mentionMsMax"] for p in periodic), default=0.0)
    mention_mean = (
        sum(p["mentionMsMean"] * p["mentionCalls"] for p in periodic) / mention_calls if mention_calls else 0.0
    )
    series = [
        {k: p[k] for k in ("t", "earlierTurns", "earlierClosedKeys", "earlierClosedTurns", "earlierStale", "liveRooms", "messages")}
        for p in periodic[:: max(1, len(periodic) // 12)]
    ]
    summary = {
        "load": {"stations": args.stations, "rooms": args.rooms, "viewers": args.viewers, "minutes": args.minutes, "workers": args.workers},
        "server": {
            "peakRssMB": round(final["peak"] / 1048576, 1),
            "cpuPerWallSecond": round(cpu_run, 3),
            "cpuPerWallSecondSteady": round(steady, 3) if steady is not None else None,
            "wallSeconds": final["wall"],
            "allSignedInAfterSeconds": round(all_signed, 1) if all_signed is not None else None,
        },
        "bandTick": {
            "ticks": ticks,
            "mostAtOneEvery2s": round(final["wall"] / 2),
            "msMax": max((p["tickMsMax"] for p in periodic), default=0.0),
            "msP99Worst": max((p["tickMsP99"] for p in periodic), default=0.0),
            "gapMsMax": max(gaps, default=0.0),
        },
        "eventLoopLagMs": {
            "max": max((p["lagMsMax"] for p in periodic), default=0.0),
            "p99Worst": max((p["lagMsP99"] for p in periodic), default=0.0),
        },
        "unreadMentions": {
            "calls": mention_calls,
            "msMean": round(mention_mean, 3),
            "msMax": mention_max,
            "endMsMean": final.get("endMentionMsMean"),
            "endMsMax": final.get("endMentionMsMax"),
            "messagesAtEnd": final.get("messages"),
        },
        "earlier": {
            "atEnd": {k: final.get(k) for k in ("earlierKeys", "earlierTurns", "earlierClosedKeys", "earlierClosedTurns", "earlierStale")},
            "series": series,
        },
        "clients": counts,
    }
    print(json.dumps(summary, indent=2))
    ok = final["peak"] < RSS_LIMIT_BYTES and cpu_run < CPU_LIMIT
    print(
        f"load_probe: peak RSS {final['peak'] / 1048576:.1f} MB (limit 256), "
        f"CPU {cpu_run:.3f} of one core over the run (limit {CPU_LIMIT}): {'PASS' if ok else 'FAIL'}"
    )
    if not args.keep:
        for name in sorted(os.listdir(folder)):
            path = folder / name
            if path.is_file():
                path.unlink()
        try:
            (folder / "backup").rmdir()
        except OSError:
            pass
        folder.rmdir()
    return 0 if ok else 1


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--stations", type=int, default=500)
    parser.add_argument("--rooms", type=int, default=150)
    parser.add_argument("--viewers", type=int, default=500)
    parser.add_argument("--minutes", type=float, default=30.0)
    parser.add_argument("--workers", type=int, default=4, help="client processes")
    parser.add_argument("--ramp", type=float, default=60.0, help="seconds over which the stations first sign in")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--verbose", action="store_true", help="print each of the server's 10 s lines")
    parser.add_argument("--keep", action="store_true", help="keep the temporary folder with the database")
    parser.add_argument("--serve", metavar="CONFIG", help=argparse.SUPPRESS)
    parser.add_argument("--worker", metavar="SPEC", help=argparse.SUPPRESS)
    parser.add_argument("--mentions-bench", type=int, metavar="N", help="time unread_mentions against N messages and exit")
    args = parser.parse_args(argv)
    if args.serve:
        return asyncio.run(serve(args.serve))
    if args.worker:
        return asyncio.run(work(args.worker))
    if args.mentions_bench is not None:
        return mentions_bench(args.mentions_bench)
    if args.viewers > 500:
        print("load_probe: the server takes 500 live sockets in all", file=sys.stderr)
    return asyncio.run(control(args))


if __name__ == "__main__":
    sys.exit(main())
