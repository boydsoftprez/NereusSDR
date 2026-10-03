# no-port-check: NereusSDR-original.
"""Every session fixture whose runs names "service", against the real
service over real WebSockets."""

import pytest

from runner import FixtureFailure, load_fixture, load_manifest, run_fixture

SESSIONS = [f for f in load_manifest()["fixtures"] if f["kind"] == "session"]


@pytest.mark.parametrize("entry", SESSIONS, ids=[f["id"] for f in SESSIONS])
def test_session(entry):
    fixture = load_fixture(entry["file"])
    assert "service" in fixture["runs"], "the service's runner runs every fixture"
    assert set(fixture["runs"]) <= {"service", "core", "app"}
    run_fixture(fixture, entry["id"])


def test_runner_reports_the_step_that_differs():
    fixture = load_fixture("sessions/register.json")
    fixture["steps"][-1]["message"]["type"] = "challenge"
    with pytest.raises(FixtureFailure) as info:
        run_fixture(fixture, "register-altered")
    assert "step 5" in str(info.value) and "message.type" in str(info.value)
