# no-port-check: NereusSDR-original.
"""Every turn fixture (wire spec 11.5) against the real service, in one
process with a ManualClock. Turns and membership are read from the
server's own state; every message the service queues on the way must
decode, carry no error and skip no generation."""

from __future__ import annotations

from typing import Any, Dict, List, Optional

import pytest

from inproc import Harness, Station
from nereus_roundtable.clock import DEFAULT_WALL_START_MS
from runner import load_fixture, load_manifest

TURNS = [f for f in load_manifest()["fixtures"] if f["kind"] == "turns"]


class Generations:
    """Checks that a connection's generations never skip (6.11)."""

    def __init__(self) -> None:
        self.band: Optional[int] = None
        self.rooms: Dict[str, int] = {}

    def see(self, msg: Dict[str, Any]) -> None:
        kind = msg["type"]
        if kind == "rooms":
            assert msg["gen"] == 1
            self.band = 1
        elif kind == "roomsDelta":
            assert self.band is not None and msg["gen"] == self.band + 1, msg
            self.band = msg["gen"]
        elif kind == "room":
            assert msg["gen"] == 1
            self.rooms[msg["id"]] = 1
        elif kind == "delta":
            assert msg["room"] in self.rooms and msg["gen"] == self.rooms[msg["room"]] + 1, msg
            self.rooms[msg["room"]] = msg["gen"]
            if msg["closed"]:
                del self.rooms[msg["room"]]


class TurnRunner:
    def __init__(self, fixture: Dict[str, Any], name: str) -> None:
        assert set(fixture) <= {"runs", "setup", "steps"}, name
        assert fixture["runs"] == ["service"]
        setup = fixture["setup"]
        assert set(setup) <= {"stations", "host", "wallClock"}
        self.name = name
        self.start = setup.get("wallClock", DEFAULT_WALL_START_MS)
        self.harness = Harness(wall_start_ms=self.start)
        self.stations: Dict[str, Station] = {}
        self.setup: Dict[str, Dict[str, Any]] = {}
        self.names: Dict[str, str] = {}
        self.gens: Dict[str, Generations] = {}
        for entry in setup["stations"]:
            assert set(entry) == {"name", "callsign", "grid"}
            station = self.harness.sign_in(address=f"192.0.2.{len(self.stations) + 1}")
            self.stations[entry["name"]] = station
            self.setup[entry["name"]] = entry
            self.names[station.station_id] = entry["name"]
            self.gens[entry["name"]] = Generations()
            self.gens[entry["name"]].see(station.rooms)  # type: ignore[attr-defined]
        self.fixture = fixture

    def drain(self) -> None:
        for name, station in self.stations.items():
            for msg in station.take():
                assert msg["type"] != "error", f"{name} got {msg}"
                self.gens[name].see(msg)

    def run(self) -> None:
        try:
            for index, step in enumerate(self.fixture["steps"]):
                try:
                    self.step(step)
                    self.drain()
                except AssertionError as exc:
                    raise AssertionError(f"{self.name} step {index} {step}: {exc}") from None
        finally:
            self.harness.close()

    def step(self, step: Dict[str, Any]) -> None:
        assert len(step) == 1, step
        if "presence" in step:
            p = step["presence"]
            entry = self.setup[p["station"]]
            self.stations[p["station"]].send(
                {
                    "type": "presence",
                    "callsign": entry["callsign"],
                    "grid": entry["grid"],
                    "atRadio": p.get("atRadio", True),
                    "slices": p["slices"],
                }
            )
            return
        if "reading" in step:
            r = step["reading"]
            self.stations[r["station"]].send(
                {
                    "type": "reading",
                    "slice": r["slice"],
                    "dbm": r["dbm"],
                    "t": self.harness.clock.wall_ms(),
                    "present": r["present"],
                }
            )
            return
        if "advanceMs" in step:
            self.harness.advance(int(step["advanceMs"]))
            return
        if "expectMembers" in step:
            for name, want in step["expectMembers"].items():
                assert self.membership(name) == want, f"{name} is {self.membership(name)}, not {want}"
            return
        if "expectTurns" in step:
            got = self.turns(step["expectTurns"])
            assert got == step["expectTurns"], f"turns are {got}"
            return
        raise AssertionError(f"unknown step {step}")

    def membership(self, name: str) -> str:
        rows = self.harness.service.book.rows_of(self.stations[name].station_id)
        if any(row.member and row.room.live for row in rows):
            return "member"
        return "joining" if rows else "absent"

    def turns(self, expected: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        book = self.harness.service.book
        found: Dict[int, Any] = {}
        for turns in book.earlier.values():
            for turn in turns:
                found[turn.id] = turn
        for room in book.rooms.values():
            for turn in room.engine.open:
                found[turn.id] = turn

        def talker_name(turn: Any) -> str:
            return self.names[turn.talker.station_id] if turn.talker is not None else ""

        ordered = sorted(found.values(), key=lambda t: (t.start, talker_name(t), t.id))
        out: List[Dict[str, Any]] = []
        for i, turn in enumerate(ordered):
            want = expected[i] if i < len(expected) else {}
            obj: Dict[str, Any] = {"kind": turn.kind}
            if turn.kind == "doubled":
                obj["talkers"] = [self.names[row.station_id] for row in turn.talkers]
            else:
                obj["talker"] = talker_name(turn) or None
            obj["startMs"] = turn.start - self.start
            obj["endMs"] = turn.end - self.start if turn.end is not None else None
            if "ending" in want:
                obj["ending"] = turn.ending
            if "readings" in want:
                obj["readings"] = {self.names[sid]: n for sid, n in turn.readings_by_station().items()}
            out.append(obj)
        return out


@pytest.mark.parametrize("entry", TURNS, ids=[f["id"] for f in TURNS])
def test_turn_fixture(entry):
    TurnRunner(load_fixture(entry["file"]), entry["id"]).run()


def test_every_turn_fixture_of_the_spec_is_listed():
    assert {f["id"] for f in TURNS} == {
        "talking",
        "unknown-then-talking",
        "quiet-after-3s",
        "same-talker-continues",
        "waiting-reading-dropped",
        "doubled",
        "long-key",
        "digital",
        "join-dwell",
        "two-slices-one-room",
        "talker-leaves",
        "key-in-candidate",
    }
