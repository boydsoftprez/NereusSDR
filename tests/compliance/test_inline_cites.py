"""Regression gate on `scripts/verify-inline-cites.py`.

See ``tests/compliance/inline-cites-baseline.json`` for the current
grandfathered count. Any new cite without a version stamp pushes the
total above the baseline and fails this test — preventing drift.

Drops below the baseline are welcome. Recapture with:

    python3 scripts/verify-inline-cites.py --baseline

…and commit the updated ``inline-cites-baseline.json``.
"""
from __future__ import annotations

import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
BASELINE = REPO / "tests" / "compliance" / "inline-cites-baseline.json"


def test_missing_stamp_count_does_not_exceed_baseline():
    result = subprocess.run(
        ["python3", "scripts/verify-inline-cites.py", "--format=json"],
        cwd=REPO,
        capture_output=True,
        text=True,
        check=False,
    )
    findings = json.loads(result.stdout)
    missing = [f for f in findings if f["kind"] == "missing-stamp"]
    baseline = json.loads(BASELINE.read_text())["missing_stamp_baseline"]
    assert len(missing) <= baseline, (
        f"{len(missing)} cites without version stamp; baseline is {baseline}. "
        f"Either stamp the new cite(s) or (only if intentional) recapture "
        f"the baseline with `scripts/verify-inline-cites.py --baseline`.\n"
        f"First 5 new findings: {missing[:5]}"
    )


def test_verifier_exit_zero_when_at_baseline():
    result = subprocess.run(
        ["python3", "scripts/verify-inline-cites.py"],
        cwd=REPO,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, (
        f"verify-inline-cites.py exited {result.returncode} at baseline\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )


def test_strict_mode_fails_when_any_findings():
    """Sanity: --strict flips behaviour. When the tree actually has
    findings (baseline > 0), strict mode must exit 1."""
    result = subprocess.run(
        ["python3", "scripts/verify-inline-cites.py", "--strict"],
        cwd=REPO,
        capture_output=True,
        text=True,
        check=False,
    )
    baseline = json.loads(BASELINE.read_text())["missing_stamp_baseline"]
    if baseline == 0:
        assert result.returncode == 0
    else:
        assert result.returncode == 1, (
            "--strict should fail when there are unstamped cites"
        )
