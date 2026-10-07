"""Regression tests for the upstream-clone lookup in scripts/git-hooks/pre-commit.

The hook runs under ``set -euo pipefail`` and finds the Thetis,
Thetis-v2.10.3.15, mi0bot-Thetis, deskhpsdr and freedv-gui clones by
walking every ancestor of the working tree and then of the main checkout.
From b8fd5a6c (2026-07-28) until these tests landed, find_upstream()
returned 1 when a clone was missing, so the
``X_LOCAL="$(find_upstream ...)"`` assignments ended the hook with exit 1
and no message. A checkout without every clone above it (a
``git worktree add`` under /private/tmp, or a machine with no deskhpsdr
clone) could not commit a source or docs change, and nothing said why.

A commit that stages C or C++ now needs every clone: the tag check
compares ported comments with the upstream lines they cite, so a missing
clone blocks the commit with a message naming it, rather than skipping
the check or passing its cites as unchecked warnings. Ported comments live
only in C and C++ files, so a commit without any (docs, scripts) still
goes through: it runs the check with the clones it finds, skips it without
Thetis, and names what is missing.

Each test runs the real hook against one staged file, C++ or docs, in a
throwaway repository under ``tmp_path``. Every script the hook calls is a
stub there, so the tests exercise only the hook's own control flow: the
other gates exit 0 (CI runs the real ones as separate steps), and the
tag-check stub records the NEREUS_*_DIR paths the hook exported to it and
exits with the status the test chooses.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parent.parent.parent
HOOK = REPO / "scripts" / "git-hooks" / "pre-commit"

# Upstream clone directory name -> the variable the hook exports for it.
UPSTREAMS = {
    "Thetis": "NEREUS_THETIS_DIR",
    "Thetis-v2.10.3.15": "NEREUS_THETIS_V21015_DIR",
    "mi0bot-Thetis": "NEREUS_MI0BOT_DIR",
    "deskhpsdr": "NEREUS_DESKHPSDR_DIR",
    "freedv-gui": "NEREUS_FREEDV_DIR",
}

TAG_CHECK = "verify-inline-tag-preservation.py"
SKIPPED = "[pre-commit] tag-preservation SKIPPED (NEREUS_SKIP_TAG_CHECK set)"
DOCS_SKIPPED = "[pre-commit] tag-preservation SKIPPED (no Thetis clone found locally;"
DOCS_PARTIAL = "[pre-commit] tag-preservation running without every upstream clone;"
MISSING = "[pre-commit] BLOCKED: upstream clone missing"
BLOCKED = "[pre-commit] BLOCKED: dropped developer-attribution tag"

CXX = "src/Port.cpp"
DOCS = "notes.md"

PASSING_STUB = "import sys\nsys.exit(0)\n"
TAG_CHECK_STUB = """\
import json, os, sys
with open(os.environ["TAG_CHECK_RECORD"], "w") as f:
    json.dump({k: v for k, v in os.environ.items() if k.startswith("NEREUS_")}, f)
sys.exit(int(os.environ["TAG_CHECK_EXIT"]))
"""


def _env(**extra: str) -> dict[str, str]:
    # NEREUS_* would override the lookup under test. GIT_* is set when
    # pytest itself runs from inside a git hook, and would point the
    # throwaway repository's git commands at the real one.
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("NEREUS_", "GIT_"))}
    env.update(extra)
    return env


def _checkout(tmp_path: Path, present=(), staged: str = CXX) -> Path:
    """A throwaway checkout with exactly the `present` clones beside it
    and `staged` as the one staged change."""
    for name in present:
        (tmp_path / name).mkdir()
    repo = tmp_path / "NereusSDR"
    (repo / "scripts").mkdir(parents=True)
    hook_text = HOOK.read_text(encoding="utf-8")
    called = set(re.findall(r"scripts/([\w.-]+\.py)", hook_text))
    assert TAG_CHECK in called, f"{HOOK} no longer runs {TAG_CHECK}"
    for name in called:
        stub = TAG_CHECK_STUB if name == TAG_CHECK else PASSING_STUB
        (repo / "scripts" / name).write_text(stub)
    (repo / staged).parent.mkdir(parents=True, exist_ok=True)
    (repo / staged).write_text("// staged change\n")
    for args in (["init", "-q"], ["add", staged]):
        _git(repo, *args)

    absent = [name for name in UPSTREAMS if name not in present]
    stray = [base / name for base in (repo, *repo.parents) for name in absent
             if (base / name).is_dir()]
    assert not stray, (f"fixture broken: {stray} would be found from {repo}; "
                       f"rerun with --basetemp somewhere else")
    return repo


def _git(repo: Path, *args: str) -> None:
    subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t",
                    "-c", "commit.gpgsign=false", *args],
                   cwd=repo, env=_env(), check=True, capture_output=True)


def _named_missing(result: subprocess.CompletedProcess) -> set[str]:
    """The clone names the hook's missing-clone message lists."""
    return set(re.findall(r"^\s+\.\./(\S+)\s+\(set ", result.stdout, re.M))


def _run_hook(repo: Path, tag_check_exit: int = 0, **env: str):
    """Run the real hook in `repo`.

    Returns the finished process and the NEREUS_* paths the tag check was
    given, or None when the hook never ran the tag check.
    """
    record = repo.parent / "tag-check.json"
    result = subprocess.run(
        ["bash", str(HOOK)], cwd=repo, capture_output=True, text=True,
        env=_env(TAG_CHECK_RECORD=str(record),
                 TAG_CHECK_EXIT=str(tag_check_exit), **env))
    seen = None
    if record.exists():
        seen = {k: Path(v).resolve()
                for k, v in json.loads(record.read_text()).items()}
    return result, seen


def _dirs(base: Path, names) -> dict[str, Path]:
    return {UPSTREAMS[name]: (base / name).resolve() for name in names}


def _describe(result: subprocess.CompletedProcess) -> str:
    return (f"hook exited {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")


def test_cxx_commit_with_no_clones_blocks_and_names_every_one(tmp_path):
    repo = _checkout(tmp_path)
    result, seen = _run_hook(repo)
    assert result.returncode == 1, _describe(result)
    assert MISSING in result.stdout, _describe(result)
    assert _named_missing(result) == set(UPSTREAMS), _describe(result)
    assert seen is None, "the tag check ran with no clones"


@pytest.mark.parametrize("missing", list(UPSTREAMS))
def test_cxx_commit_with_a_missing_clone_blocks_and_names_it(tmp_path, missing):
    present = [name for name in UPSTREAMS if name != missing]
    repo = _checkout(tmp_path, present)
    result, seen = _run_hook(repo)
    assert result.returncode == 1, _describe(result)
    assert MISSING in result.stdout, _describe(result)
    assert _named_missing(result) == {missing}, _describe(result)
    assert seen is None, f"the tag check ran without {missing}"


def test_skip_switch_still_skips_on_purpose(tmp_path):
    repo = _checkout(tmp_path)
    result, seen = _run_hook(repo, NEREUS_SKIP_TAG_CHECK="1")
    assert result.returncode == 0, _describe(result)
    assert SKIPPED in result.stdout, _describe(result)
    assert seen is None


@pytest.mark.parametrize("staged", [CXX, "src/Port.h", "third_party/x.c"])
def test_every_c_or_cxx_suffix_needs_the_clones(tmp_path, staged):
    repo = _checkout(tmp_path, staged=staged)
    result, seen = _run_hook(repo)
    assert result.returncode == 1, _describe(result)
    assert MISSING in result.stdout, _describe(result)
    assert seen is None


@pytest.mark.parametrize("missing", list(UPSTREAMS))
def test_docs_commit_with_a_missing_clone_passes_and_names_it(tmp_path, missing):
    present = [name for name in UPSTREAMS if name != missing]
    repo = _checkout(tmp_path, present, staged=DOCS)
    result, seen = _run_hook(repo)
    assert result.returncode == 0, _describe(result)
    assert MISSING not in result.stdout, _describe(result)
    assert _named_missing(result) == {missing}, _describe(result)
    if missing == "Thetis":
        assert DOCS_SKIPPED in result.stdout, _describe(result)
        assert seen is None, "the tag check ran without Thetis"
    else:
        assert DOCS_PARTIAL in result.stdout, _describe(result)
        assert seen == _dirs(tmp_path, present), _describe(result)


def test_docs_commit_with_no_clones_skips_and_names_every_one(tmp_path):
    repo = _checkout(tmp_path, staged=DOCS)
    result, seen = _run_hook(repo)
    assert result.returncode == 0, _describe(result)
    assert DOCS_SKIPPED in result.stdout, _describe(result)
    assert _named_missing(result) == set(UPSTREAMS), _describe(result)
    assert seen is None


def test_docs_commit_with_some_clones_still_blocks_a_dropped_tag(tmp_path):
    present = [name for name in UPSTREAMS if name != "mi0bot-Thetis"]
    repo = _checkout(tmp_path, present, staged=DOCS)
    result, seen = _run_hook(repo, tag_check_exit=1)
    assert result.returncode == 1, _describe(result)
    assert BLOCKED in result.stdout, _describe(result)
    assert seen == _dirs(tmp_path, present)


@pytest.mark.parametrize("staged", [CXX, DOCS])
@pytest.mark.parametrize("tag_check_exit", [0, 1])
def test_all_clones_present_keeps_strict_check(tmp_path, tag_check_exit, staged):
    repo = _checkout(tmp_path, list(UPSTREAMS), staged=staged)
    result, seen = _run_hook(repo, tag_check_exit)
    assert result.returncode == tag_check_exit, _describe(result)
    assert (BLOCKED in result.stdout) == (tag_check_exit != 0), _describe(result)
    assert seen == _dirs(tmp_path, UPSTREAMS)


def test_env_override_finds_clones_outside_the_ancestors(tmp_path):
    elsewhere = tmp_path / "elsewhere"
    for name in UPSTREAMS:
        (elsewhere / name).mkdir(parents=True)
    repo = _checkout(tmp_path)
    overrides = {var: str(elsewhere / name) for name, var in UPSTREAMS.items()}
    result, seen = _run_hook(repo, **overrides)
    assert result.returncode == 0, _describe(result)
    assert seen == _dirs(elsewhere, UPSTREAMS)


def test_worktree_elsewhere_finds_clones_beside_the_main_checkout(tmp_path):
    repo = _checkout(tmp_path, list(UPSTREAMS))
    _git(repo, "add", "scripts")
    _git(repo, "commit", "-q", "-m", "fixture")
    # Outside tmp_path, so no ancestor of the worktree holds the clones.
    worktree = tmp_path.parent / f"{tmp_path.name}-worktree"
    _git(repo, "worktree", "add", "-q", str(worktree))
    (worktree / "src" / "More.cpp").write_text("// another change\n")
    _git(worktree, "add", "src/More.cpp")
    result, seen = _run_hook(worktree)
    assert result.returncode == 0, _describe(result)
    assert seen == _dirs(tmp_path, UPSTREAMS), _describe(result)
