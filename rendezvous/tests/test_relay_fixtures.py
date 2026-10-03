# no-port-check: NereusSDR-original.
"""Every frame-protocol fixture in rendezvous/conformance/v1/relay/ against
the real relay (rendezvous document section 12.8)."""

from pathlib import Path

import pytest

from relay_runner import RELAY_FIXTURES, RelayFixtureFailure, load_fixture, load_manifest, run_relay_fixture

FIXTURES = load_manifest()["fixtures"]


def test_every_relay_fixture_is_listed_once():
    listed = [f["file"] for f in FIXTURES]
    assert sorted(listed) == sorted(p.name for p in Path(RELAY_FIXTURES).glob("*.json") if p.name != "manifest.json")
    assert len(set(listed)) == len(listed)
    assert load_manifest()["relayFrameVersions"] == [1]


RELAY_RUN = [f for f in FIXTURES if "relay" in load_fixture(f["file"])["runs"]]

_STEP_KEYS = [
    {"connect"}, {"connect", "address"}, {"from", "role", "binary"}, {"from", "role", "text"}, {"to", "binary"},
    {"advanceMs"}, {"disconnect"}, {"drop"}, {"expectClosed", "code"}, {"expectSilent"}, {"shutdown"},
]


@pytest.mark.parametrize("entry", FIXTURES, ids=[f["id"] for f in FIXTURES])
def test_relay_fixture_shape(entry):
    """Section 12.8: every fixture is well formed, and each one a leg's
    runner runs plays that leg on the connection named for it, with the
    leg's own messages marked as it may send them."""
    fixture = load_fixture(entry["file"])
    runs = set(fixture["runs"])
    assert runs and runs <= {"relay", "core", "app"}, runs
    names = set()
    for step in fixture["steps"]:
        assert set(step) in _STEP_KEYS, step
        for key in ("connect", "from", "to", "disconnect", "drop", "expectClosed", "expectSilent"):
            if key in step:
                assert step[key].startswith(("core", "device")), step
                names.add(step[key])
        if "from" in step:
            assert step["role"] in ("behaviour", "scripted"), step
    for run, leg in (("core", "core"), ("app", "device")):
        if run in runs:
            assert leg in names, f"{entry['id']}: runs {run} but has no connection named {leg}"
    # Every behaviour JOIN on a leg's own connection uses that leg's first
    # token placeholder, which fills to the same token each time (a leg
    # joins again with the token it was given).
    for leg in ("core", "device"):
        joins = [
            step["binary"][1]
            for step in fixture["steps"]
            if step.get("from") == leg and step.get("role") == "behaviour" and step["binary"][0] == "80"
        ]
        assert len(set(joins)) <= 1, f"{entry['id']}: {leg} joins with {sorted(set(joins))}"
    if "relay" not in runs:
        # A reader-rule fixture: the leg joins, and every message to it is
        # the relay's.
        assert fixture["steps"][1]["from"] in ("core", "device") and fixture["steps"][1]["binary"][0] == "80"


@pytest.mark.parametrize("entry", RELAY_RUN, ids=[f["id"] for f in RELAY_RUN])
def test_relay_fixture(entry):
    run_relay_fixture(load_fixture(entry["file"]), entry["id"])


def test_a_token_placeholder_fills_to_the_same_token_after_time_moves():
    from relay_runner import RelayRunner

    runner = RelayRunner(load_fixture("end-ended-core.json"), "tokens")
    first = runner.fill(["$token:core:s:a"])
    runner.clock.advance(31000)
    assert runner.fill(["$token:core:s:a"]) == first
    assert runner.fill(["$token:core:s:a:expired"]) != first


def test_each_end_code_is_seen_by_each_leg():
    """Every END code a conformant leg can meet is played towards the Core's
    leg and towards the device's (section 12.8)."""
    seen = {"core": set(), "app": set()}
    for entry in FIXTURES:
        fixture = load_fixture(entry["file"])
        for run, leg in (("core", "core"), ("app", "device")):
            if run not in fixture["runs"]:
                continue
            for step in fixture["steps"]:
                if step.get("to") == leg and step["binary"][0] == "83":
                    seen[run].add(bytes.fromhex(step["binary"][1]).decode())
    want = {"expired", "full", "tooManySessions", "tooManyConnections", "shuttingDown", "replaced", "ended",
            "peerGone", "idle"}
    assert seen["core"] == want and seen["app"] == want, seen


def test_the_runner_reports_the_step_that_differs():
    fixture = load_fixture("join-and-forward.json")
    fixture["steps"][2]["binary"] = ["81", "01", "01"]
    with pytest.raises(RelayFixtureFailure) as info:
        run_relay_fixture(fixture, "altered")
    assert "step 2" in str(info.value) and "at byte 2" in str(info.value)
