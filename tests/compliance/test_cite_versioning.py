#!/usr/bin/env python3
"""Test cite-versioning scan in scripts/check-new-ports.py.

Runs without pytest: `python3 tests/compliance/test_cite_versioning.py`.
Exit 0 on pass, 1 on failure. Designed to live alongside the other
compliance verifiers so it can be wired into pre-commit / CI later.
"""

import importlib.util
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SCRIPT = REPO / "scripts" / "check-new-ports.py"


def load_module():
    spec = importlib.util.spec_from_file_location("cnp", SCRIPT)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


FIXTURE_BAD = """\
// NereusSDR-native file (not registered — so heuristics can fire).
// From Thetis console.cs:4821 -- original value 0.98f
static constexpr float kAgcDecay = 0.98f;
"""

FIXTURE_GOOD_TAG = """\
// From Thetis console.cs:4821 [v2.10.3.13] -- original value 0.98f
static constexpr float kAgcDecay = 0.98f;
"""

FIXTURE_GOOD_SHA = """\
// From Thetis setup.cs:847 [@abc1234] -- per-protocol rate list
static constexpr int kDefaultRate = 192000;
"""

FIXTURE_GOOD_TAG_PLUS_SHA = """\
// From Thetis cmaster.cs:491 [v2.10.3.13+abc1234] -- CMCreateCMaster
void createCMaster() {}
"""

FIXTURE_MULTI_FILE = """\
// From Thetis console.cs:4821, setup.cs:847 [v2.10.3.13] -- ...
static constexpr float kX = 1.0f;
"""


def write_tmp(body: str) -> Path:
    tmp = tempfile.NamedTemporaryFile(
        mode="w", suffix=".cpp", delete=False, dir=REPO
    )
    tmp.write(body)
    tmp.close()
    return Path(tmp.name)


def _run_case(cnp, body: str, expect_flag: bool) -> bool:
    tmp = write_tmp(body)
    try:
        rel = str(tmp.relative_to(REPO))
        # Simulate "every line in the fixture was added in the PR"
        line_count = len(body.splitlines())
        diff_lines = set(range(1, line_count + 1))
        findings = cnp.check_file(rel, listed=set(), diff_lines=diff_lines)
        has_cite_finding = any(
            "cite missing version stamp" in label
            for _line, label, _m in findings
        )
        ok = has_cite_finding is expect_flag
        label = "flagged" if has_cite_finding else "clean"
        want = "flag" if expect_flag else "clean"
        print(f"  {'PASS' if ok else 'FAIL'}: want {want}, got {label}")
        return ok
    finally:
        tmp.unlink()


def main() -> int:
    cnp = load_module()
    cnp.FULL_TREE = False  # cite-scan runs only in diff mode

    print("Bad: unstamped cite → should flag")
    bad = _run_case(cnp, FIXTURE_BAD, expect_flag=True)

    print("Good: [v2.10.3.13] tag → should pass")
    g1 = _run_case(cnp, FIXTURE_GOOD_TAG, expect_flag=False)

    print("Good: [@abc1234] sha → should pass")
    g2 = _run_case(cnp, FIXTURE_GOOD_SHA, expect_flag=False)

    print("Good: [v2.10.3.13+abc1234] tag+sha → should pass")
    g3 = _run_case(cnp, FIXTURE_GOOD_TAG_PLUS_SHA, expect_flag=False)

    print("Good: multi-file cite with single tag → should pass")
    g4 = _run_case(cnp, FIXTURE_MULTI_FILE, expect_flag=False)

    passed = all([bad, g1, g2, g3, g4])
    print(f"\n{'ALL PASS' if passed else 'FAIL'}")
    return 0 if passed else 1


def test_cite_versioning_suite() -> None:
    # pytest entry point — runs the full standalone suite and asserts
    # exit 0. Helper cases are named with a leading underscore so pytest
    # does not auto-collect them as tests (they require positional args,
    # not fixtures).
    assert main() == 0


if __name__ == "__main__":
    sys.exit(main())
