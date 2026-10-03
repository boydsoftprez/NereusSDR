#!/usr/bin/env python3
"""Tests for scripts/verify-test-registration.py (PR #327 Linux configure).

Runs under pytest or alone: `python3 tests/compliance/test_test_registration.py`.
"""

import importlib.util
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SCRIPT = REPO / "scripts" / "verify-test-registration.py"


def load_module():
    spec = importlib.util.spec_from_file_location("vtr", SCRIPT)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def run_on(text: str) -> list[str]:
    m = load_module()
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "CMakeLists.txt"
        p.write_text(text, encoding="utf-8")
        return m.check(p)


def test_unguarded_property_on_sharded_test_fails():
    errs = run_on("nereus_add_test(tst_a)\n"
                  "set_tests_properties(tst_a PROPERTIES TIMEOUT 900)\n")
    assert len(errs) == 1 and "if(TEST tst_a)" in errs[0]


def test_guarded_property_passes():
    errs = run_on("nereus_add_test(tst_a)\n"
                  "if(TEST tst_a)\n"
                  "    set_tests_properties(tst_a PROPERTIES TIMEOUT 900)\n"
                  "    get_test_property(tst_a LABELS _l)\n"
                  "    add_test(NAME tst_a_more COMMAND tst_a)\n"
                  "    set_property(TEST tst_a_more APPEND PROPERTY ENVIRONMENT X=1)\n"
                  "endif()\n")
    assert errs == []


def test_else_branch_of_test_guard_is_not_guarded():
    errs = run_on("nereus_add_test(tst_a)\n"
                  "if(TEST tst_a)\n"
                  "else()\n"
                  "    set_tests_properties(tst_a PROPERTIES TIMEOUT 1)\n"
                  "endif()\n")
    assert len(errs) == 1


def test_plain_add_test_under_other_condition_fails():
    errs = run_on("if(LINUX)\n"
                  "    add_test(NAME t_plain COMMAND echo)\n"
                  "endif()\n"
                  "set_tests_properties(t_plain PROPERTIES TIMEOUT 1)\n")
    assert len(errs) == 1


def test_plain_add_test_same_branch_passes():
    errs = run_on("if(LINUX)\n"
                  "    add_test(NAME t_plain COMMAND echo)\n"
                  "    set_tests_properties(t_plain PROPERTIES TIMEOUT 1)\n"
                  "endif()\n")
    assert errs == []


def test_unknown_test_fails_and_comments_ignored():
    errs = run_on("# set_tests_properties(tst_ghost PROPERTIES TIMEOUT 1)\n"
                  "set_tests_properties(tst_missing PROPERTIES TIMEOUT 1)\n")
    assert len(errs) == 1 and "tst_missing" in errs[0]


def test_repository_file_passes():
    m = load_module()
    assert m.check(REPO / "tests" / "CMakeLists.txt") == []


if __name__ == "__main__":
    failures = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print(f"PASS {name}")
            except AssertionError as e:
                failures += 1
                print(f"FAIL {name}: {e}")
    sys.exit(1 if failures else 0)
