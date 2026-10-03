#!/usr/bin/env python3
"""Regression coverage for staged-index auditing in check-new-ports.py.

Runs real checker subprocesses in temporary Git repositories.  It intentionally
does not mock Git, the checker, provenance parsing, or file reads: the failure
being guarded against is the difference between index and worktree content.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
CHECKER = PROJECT / "scripts" / "check-new-ports.py"

BAD_PORT = "// From Thetis console.cs:4821 [v2.10.3.15]\nvoid ported() {}\n"
BAD_CITE = "// From Thetis console.cs:4821\nvoid cited() {}\n"
NATIVE = "void native() {}\n"


class CheckerRepo:
    def __init__(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.path = Path(self._temp.name)
        self._git("init", "-b", "main")
        self._git("config", "user.name", "Checker fixture")
        self._git("config", "user.email", "checker-fixture@example.invalid")
        self._git("config", "commit.gpgSign", "false")
        self.write("scripts/check-new-ports.py", CHECKER.read_text())
        # The checker reads each file's header through the shared rule.
        self.write("scripts/header_block.py",
                   (CHECKER.parent / "header_block.py").read_text())
        for name in (
            "THETIS-PROVENANCE.md",
            "WDSP-PROVENANCE.md",
            "aethersdr-reconciliation.md",
            "FREEDV-GUI-PROVENANCE.md",
        ):
            self.write(f"docs/attribution/{name}", "| NereusSDR file | Source |\n|---|---|\n")
        self.write("src/Existing.cpp", NATIVE)
        self._git("add", ".")
        self._git("commit", "-m", "baseline")
        self._git("branch", "base")

    def close(self) -> None:
        self._temp.cleanup()

    def _git(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", *args], cwd=self.path, text=True, capture_output=True,
            check=True,
        )

    def write(self, relative: str, content: str) -> None:
        path = self.path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def stage(self, *paths: str) -> None:
        self._git("add", *paths)

    def commit(self, message: str) -> None:
        self._git("commit", "-m", message)

    def check(self, *, full_tree: bool = False) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, "scripts/check-new-ports.py"]
        if full_tree:
            command.append("--full-tree")
        env = os.environ.copy()
        env["CHECK_NEW_PORTS_BASE_REF"] = "base"
        env["CHECK_NEW_PORTS_FULL"] = "0"
        return subprocess.run(command, cwd=self.path, text=True,
                              capture_output=True, env=env)


class NewPortsStagedTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = CheckerRepo()

    def tearDown(self) -> None:
        self.fixture.close()

    def assert_fails_for(self, result: subprocess.CompletedProcess[str], path: str) -> None:
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(f"FAIL {path}:", result.stdout)

    def test_new_staged_bad_file_fails_before_commit(self) -> None:
        self.fixture.write("src/New.cpp", BAD_PORT)
        self.fixture.stage("src/New.cpp")

        self.assert_fails_for(self.fixture.check(), "src/New.cpp")

    def test_modified_staged_bad_cite_fails_before_commit(self) -> None:
        self.fixture.write("src/Existing.cpp", BAD_CITE)
        self.fixture.stage("src/Existing.cpp")

        result = self.fixture.check()
        self.assert_fails_for(result, "src/Existing.cpp")
        self.assertIn("Thetis cite missing version stamp", result.stdout)

    def test_staged_source_and_staged_provenance_pass(self) -> None:
        self.fixture.write("src/Registered.cpp", BAD_PORT)
        self.fixture.write(
            "docs/attribution/THETIS-PROVENANCE.md",
            "| NereusSDR file | Source |\n|---|---|\n"
            "| src/Registered.cpp | console.cs |\n",
        )
        self.fixture.stage("src/Registered.cpp", "docs/attribution/THETIS-PROVENANCE.md")

        result = self.fixture.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("OK [diff]", result.stdout)

    def test_unstaged_provenance_cannot_allow_staged_bad_source(self) -> None:
        self.fixture.write("src/Unregistered.cpp", BAD_PORT)
        self.fixture.stage("src/Unregistered.cpp")
        self.fixture.write(
            "docs/attribution/THETIS-PROVENANCE.md",
            "| NereusSDR file | Source |\n|---|---|\n"
            "| src/Unregistered.cpp | console.cs |\n",
        )

        self.assert_fails_for(self.fixture.check(), "src/Unregistered.cpp")

    def test_staged_bad_source_with_clean_worktree_still_fails(self) -> None:
        self.fixture.write("src/Existing.cpp", BAD_PORT)
        self.fixture.stage("src/Existing.cpp")
        self.fixture._git("restore", "--worktree", "src/Existing.cpp")

        self.assert_fails_for(self.fixture.check(), "src/Existing.cpp")

    def test_clean_index_committed_pr_bad_file_still_fails(self) -> None:
        self.fixture.write("src/Committed.cpp", BAD_PORT)
        self.fixture.stage("src/Committed.cpp")
        self.fixture.commit("bad port in PR")

        self.assert_fails_for(self.fixture.check(), "src/Committed.cpp")

    def test_staged_rename_reads_new_index_path(self) -> None:
        self.fixture.write("src/Old.cpp", BAD_PORT)
        self.fixture.write(
            "docs/attribution/THETIS-PROVENANCE.md",
            "| NereusSDR file | Source |\n|---|---|\n"
            "| src/Old.cpp | console.cs |\n",
        )
        self.fixture.stage("src/Old.cpp", "docs/attribution/THETIS-PROVENANCE.md")
        self.fixture.commit("registered original path")
        # The original, registered path is part of the PR base. This makes
        # the staged move below a real BASE-to-index rename rather than an
        # add that happens to have been moved after the base was cut.
        self.fixture._git("branch", "-f", "base", "HEAD")
        self.fixture._git("mv", "src/Old.cpp", "src/Renamed.cpp")

        name_status = self.fixture._git(
            "diff", "--cached", "base", "--name-status"
        ).stdout
        self.assertIn("R100\tsrc/Old.cpp\tsrc/Renamed.cpp", name_status)

        self.assert_fails_for(self.fixture.check(), "src/Renamed.cpp")

    def test_staged_deletion_never_reads_recreated_worktree_file(self) -> None:
        self.fixture._git("rm", "src/Existing.cpp")
        # The index says deletion. An unstaged recreation must not be scanned
        # as though it were part of that staged snapshot.
        self.fixture.write("src/Existing.cpp", BAD_PORT)

        result = self.fixture.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("No added/modified files", result.stdout)

    def test_full_tree_keeps_worktree_sweep_and_all_registry_lookup(self) -> None:
        self.fixture.write("src/WorktreeOnly.cpp", BAD_PORT)
        self.assert_fails_for(self.fixture.check(full_tree=True), "src/WorktreeOnly.cpp")

        self.fixture.write(
            "docs/attribution/WDSP-PROVENANCE.md",
            "| NereusSDR file | Source |\n|---|---|\n"
            "| src/WorktreeOnly.cpp | upstream |\n",
        )
        result = self.fixture.check(full_tree=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("OK [full-tree]", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
