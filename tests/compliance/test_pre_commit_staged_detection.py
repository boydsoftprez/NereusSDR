#!/usr/bin/env python3
"""Exercise the tracked hook's applicability checks against real Git indices.

Only downstream verifier execution is instrumented; the hook, Git, and grep
are real. A large index must not skip gates when an early match closes a pipe,
and detector errors must block rather than masquerade as nonmatching paths.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
HOOK = PROJECT / "scripts/git-hooks/pre-commit"
SOURCE_GATES = [
    "verify-thetis-headers.py",
    "verify-freedv-headers.py",
    "check-new-ports.py",
    "check-new-ports.py full",
    "verify-thetis-headers.py --all-kinds",
    "verify-inline-cites.py",
    "compliance-inventory.py --fail-on-unclassified",
    "verify-no-gui-dsp-access.py",
    "verify-no-captured-slice-spectrum-wiring.py",
    "verify-no-image-cursors.py",
    "verify-inline-tag-preservation.py",
]
LICENSE_GATE = "check-third-party-licenses.py"
REGISTRATION_GATE = "verify-test-registration.py"

# These stand-ins record the actual hook's dispatch and optionally fail. They
# avoid unrelated whole-tree/upstream checks in a deliberately tiny fixture.
VERIFIER = """import os, sys
from pathlib import Path
name = Path(sys.argv[0]).name
call = " ".join([name, *sys.argv[1:]])
if os.environ.get("CHECK_NEW_PORTS_FULL") == "1":
    call += " full"
with open("verifier-calls", "a") as log:
    log.write(call + "\\n")
sys.exit(1 if name == os.environ.get("FAIL_VERIFIER") else 0)
"""


class HookRepo:
    def __init__(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)
        self.git("init", "-b", "main")
        self.write("pre-commit", HOOK.read_text())
        for name in {call.split()[0] for call in SOURCE_GATES} | {
            LICENSE_GATE, REGISTRATION_GATE,
        }:
            self.write(f"scripts/{name}", VERIFIER)
        # Keep the existing upstream-discovery path active, with no clones.
        self.upstream = self.path / "upstream"
        self.upstream.mkdir()

    def close(self) -> None:
        self.temp.cleanup()

    def git(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(["git", *args], cwd=self.path, text=True,
                              capture_output=True, check=True)

    def write(self, relative: str, content: str) -> None:
        path = self.path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def stage(self, *paths: str, large: bool = False) -> None:
        for path in paths:
            if not (self.path / path).exists():
                self.write(path, "fixture\n")
        staged = list(paths)
        if large:
            for index in range(2156):
                self.write(f"zzz/{index:04d}-{'x' * 100}.txt", "fixture\n")
            staged.append("zzz")
        self.git("add", "--", *staged)
        if large:
            listing = self.git("diff", "--cached", "--name-only").stdout
            # More than common pipe buffers; matching names sort before zzz/.
            assert len(listing.encode()) > 200_000

    def run(self, *, fail_verifier: str = "", grep_error: bool = False
            ) -> tuple[subprocess.CompletedProcess[str], list[str]]:
        env = os.environ.copy()
        for name in ("THETIS", "MI0BOT", "DESKHPSDR", "FREEDV"):
            env[f"NEREUS_{name}_DIR"] = str(self.upstream)
        env.pop("NEREUS_SKIP_TAG_CHECK", None)
        env.pop("CHECK_NEW_PORTS_FULL", None)
        env["FAIL_VERIFIER"] = fail_verifier
        if grep_error:
            self.write("bin/grep", "#!/usr/bin/env bash\n"
                       "exec /usr/bin/grep -E '['\n")
            (self.path / "bin/grep").chmod(0o755)
            env["PATH"] = str(self.path / "bin") + os.pathsep + env["PATH"]
        result = subprocess.run(["bash", "pre-commit"], cwd=self.path,
                                env=env, text=True, capture_output=True)
        log = self.path / "verifier-calls"
        return result, log.read_text().splitlines() if log.exists() else []


class PreCommitStagedDetectionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.repo = HookRepo()

    def tearDown(self) -> None:
        self.repo.close()

    def test_large_matching_index_runs_all_applicable_gates(self) -> None:
        self.repo.stage("CMakeLists.txt", "tests/CMakeLists.txt",
                        "src/Fixture.cpp", large=True)
        result, calls = self.repo.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(calls, [LICENSE_GATE, REGISTRATION_GATE, *SOURCE_GATES])

    def test_large_nonmatching_index_skips_gates(self) -> None:
        self.repo.stage(large=True)
        result, calls = self.repo.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(calls, [])

    def test_small_indices_keep_each_detector_scope(self) -> None:
        cases = [
            ("cmake/fixture.cmake", [LICENSE_GATE]),
            ("third_party/fixture.txt", [LICENSE_GATE]),
            ("packaging/third-party-licenses/fixture.txt", [LICENSE_GATE]),
            ("scripts/check-third-party-licenses.py", [LICENSE_GATE, *SOURCE_GATES]),
            ("tests/CMakeLists.txt", [LICENSE_GATE, REGISTRATION_GATE]),
            ("scripts/verify-test-registration.py", [REGISTRATION_GATE, *SOURCE_GATES]),
            ("docs/fixture.md", SOURCE_GATES),
            ("src/Fixture.hpp", SOURCE_GATES),
            ("assets/fixture.txt", []),
        ]
        for path, expected in cases:
            with self.subTest(path=path):
                self.repo.git("read-tree", "--empty")
                (self.repo.path / "verifier-calls").unlink(missing_ok=True)
                self.repo.stage(path)
                result, calls = self.repo.run()
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(calls, expected)

    def test_git_diff_failure_blocks_commit(self) -> None:
        self.repo.stage("assets/fixture.txt")
        # rev-parse still works, but real git diff cannot read a corrupt index.
        (self.repo.path / ".git/index").write_bytes(b"corrupt index\n")
        result, calls = self.repo.run()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("index", result.stderr)
        self.assertEqual(calls, [])

    def test_grep_error_blocks_commit(self) -> None:
        self.repo.stage("assets/fixture.txt")
        # Exercise grep's genuine invalid-expression exit 2 at its boundary.
        result, calls = self.repo.run(grep_error=True)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(result.stderr, "grep must report its real error")
        self.assertEqual(calls, [])

    def test_applicable_verifier_failure_still_blocks_commit(self) -> None:
        self.repo.stage("CMakeLists.txt", "tests/CMakeLists.txt", "src/Fixture.cpp")
        expected = [LICENSE_GATE, REGISTRATION_GATE, *SOURCE_GATES]
        for name in dict.fromkeys(call.split()[0] for call in expected):
            with self.subTest(verifier=name):
                (self.repo.path / "verifier-calls").unlink(missing_ok=True)
                result, calls = self.repo.run(fail_verifier=name)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("BLOCKED", result.stdout)
                # Some gates rerun the failing verifier to display diagnostics.
                failing = next(i for i, call in enumerate(expected)
                               if call.split()[0] == name)
                self.assertEqual(calls[:failing + 1], expected[:failing + 1])
                self.assertTrue(all(call.split()[0] == name
                                    for call in calls[failing:]))

    def test_real_test_registration_verifier_failure_blocks_commit(self) -> None:
        shutil.copyfile(PROJECT / "scripts/verify-test-registration.py",
                        self.repo.path / "scripts/verify-test-registration.py")
        self.repo.write("tests/CMakeLists.txt",
                        "nereus_add_test(tst_fixture)\n"
                        "set_tests_properties(tst_fixture PROPERTIES TIMEOUT 1)\n")
        self.repo.git("add", "tests/CMakeLists.txt")
        result, calls = self.repo.run()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("if(TEST tst_fixture)", result.stdout)
        self.assertIn("BLOCKED", result.stdout)
        self.assertEqual(calls, [LICENSE_GATE])


if __name__ == "__main__":
    unittest.main(verbosity=2)
