# no-port-check: NereusSDR-original.
"""Every session fixture whose runs names "service", against the real
server over real WebSockets, and the WebSocket listener's answers to
requests that are not a station or live connection (wire spec 2.1)."""

import asyncio

import pytest

from helpers import live_service
from runner import FixtureFailure, load_fixture, load_manifest, run_fixture

SESSIONS = [f for f in load_manifest()["fixtures"] if f["kind"] == "session"]

# Fixtures whose server behavior a later task of the Roundtable plan builds.
# Each later task removes its own entries; nothing else is skipped.
LATER = {
    "reading-while-joining-dropped": "Task 3 (readings and turns)",
    "reading-unknown-slice": "Task 3 (readings and turns)",
    "reading-keyed-slice": "Task 3 (readings and turns)",
    "read-room": "Task 3 (room views and reads)",
    "resync": "Task 3 (room views and resync)",
    "post-not-member": "Task 4 (messages)",
    "post-away": "Task 4 (messages)",
    "post-cleaned": "Task 4 (messages)",
    "post-accepted": "Task 4 (messages)",
    "hidden-ends-presence": "Tasks 4 and 5 (hidden, and the live reader's view)",
    "live-watch-not-found": "Task 5 (live readers)",
    "weblink": "Task 6 (website sign-in links)",
    "delete-archive": "Task 6 (archive deletion)",
}


@pytest.mark.parametrize("entry", SESSIONS, ids=[f["id"] for f in SESSIONS])
def test_session(entry):
    fixture = load_fixture(entry["file"])
    assert set(fixture["runs"]) <= {"service", "core"}
    if "service" not in fixture["runs"]:
        pytest.skip("runs on the Core's runner only")
    if entry["id"] in LATER:
        pytest.skip("built by " + LATER[entry["id"]])
    run_fixture(entry["file"])


def test_later_names_real_fixtures():
    ids = {f["id"] for f in SESSIONS}
    assert set(LATER) <= ids


def test_runner_reports_the_step_that_differs():
    fixture = load_fixture("sessions/signin-spki.json")
    fixture["steps"][-1]["message"]["gen"] = 2
    with pytest.raises(FixtureFailure) as info:
        run_fixture(fixture, "signin-spki-altered")
    assert "step 4" in str(info.value) and "message.gen" in str(info.value)


async def _http(port, path, upgrade=False):
    reader, writer = await asyncio.open_connection("127.0.0.1", port)
    head = f"GET {path} HTTP/1.1\r\nHost: roundtable.conformance.invalid\r\n"
    if upgrade:
        head += (
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n"
        )
    writer.write((head + "\r\n").encode("ascii"))
    await writer.drain()
    status = await asyncio.wait_for(reader.readline(), 5)
    rest = await asyncio.wait_for(reader.read(65536), 5)
    writer.close()
    return status.decode("ascii"), rest.decode("utf-8", "replace")


def test_websocket_paths_without_an_upgrade_get_426_and_others_404():
    async def go():
        async with live_service() as live:
            port = live.listeners.ws_port()
            for path in ("/v1/station", "/v1/live"):
                status, rest = await _http(port, path)
                assert status.split()[1] == "426", (path, status)
                assert "upgrade" in rest.lower()
            for path in ("/", "/v1/api/rooms", "/v1/stationx", "/v2/station"):
                status, _ = await _http(port, path)
                assert status.split()[1] == "404", (path, status)
            status, _ = await _http(port, "/v1/other", upgrade=True)
            assert status.split()[1] == "404", status
            status, _ = await _http(port, "/v1/station", upgrade=True)
            assert status.split()[1] == "101", status
            # The HTTP listener answers 404 until there is a read API.
            status, _ = await _http(live.listeners.http_port(), "/v1/api/rooms")
            assert status.split()[1] == "404", status

    asyncio.run(go())
