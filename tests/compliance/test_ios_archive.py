#!/usr/bin/env python3
"""Tests for ios/scripts/archive.sh, the Release archive (D132, R-IOS-28).

archive.sh runs inside a throwaway git repository holding copies of the
scripts and project.yml, with stand-in xcodegen and xcodebuild first on
PATH: xcodebuild records its arguments and makes the archived app (with or
without its privacy manifest) and does nothing else, so these tests never
sign, build or reach App Store Connect.

Runs under pytest or alone: `python3 tests/compliance/test_ios_archive.py`.
"""

from __future__ import annotations

import os
import re
import shutil
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]
SCRIPTS = PROJECT / "ios" / "scripts"
ARCHIVE = SCRIPTS / "archive.sh"
COPIED = ("archive.sh", "build-number.sh", "build-tag.sh", "generate-project.sh")

STAND_IN_XCODEBUILD = """#!/bin/sh
printf '%s\\n' "$@" > "$NEREUS_TEST_ARGS"
path=""
while [ $# -gt 0 ]; do
    if [ "$1" = "-archivePath" ]; then path=$2; fi
    shift
done
app="$path/Products/Applications/NereusSDR.app"
mkdir -p "$app"
if [ "${NEREUS_TEST_MANIFEST:-yes}" = yes ]; then
    printf '<plist/>' > "$app/PrivacyInfo.xcprivacy"
fi
exit "${NEREUS_TEST_XCODEBUILD_STATUS:-0}"
"""

STAND_IN_XCODEGEN = "#!/bin/sh\nexit 0\n"


def git(repo: Path, *args: str) -> str:
    return subprocess.run(["git", *args], cwd=repo, check=True, capture_output=True, text=True).stdout.strip()


def project_version() -> str:
    found = re.findall(r'^\s*MARKETING_VERSION:\s*"([0-9][0-9.]*)"\s*$',
                       (PROJECT / "ios" / "project.yml").read_text(encoding="utf-8"), re.M)
    assert len(found) == 1, found
    return found[0]


class Checkout:
    """A throwaway repository with the scripts and project.yml, on branch feature/x."""

    def __init__(self, with_git: bool = True) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)
        scripts = self.root / "ios" / "scripts"
        scripts.mkdir(parents=True)
        for name in COPIED:
            shutil.copy2(SCRIPTS / name, scripts / name)
        shutil.copy2(PROJECT / "ios" / "project.yml", self.root / "ios" / "project.yml")
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for name, text in (("xcodebuild", STAND_IN_XCODEBUILD), ("xcodegen", STAND_IN_XCODEGEN)):
            tool = self.bin / name
            tool.write_text(text, encoding="utf-8")
            tool.chmod(tool.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        self.args = self.root / "args.txt"
        if with_git:
            git(self.root, "init", "-q", "-b", "feature/x")
            git(self.root, "config", "user.name", "Test")
            git(self.root, "config", "user.email", "test@example.invalid")
            git(self.root, "config", "commit.gpgsign", "false")
            git(self.root, "config", "tag.gpgsign", "false")
            git(self.root, "add", ".")
            git(self.root, "commit", "-q", "-m", "first")
            (self.root / "ios" / "notes.txt").write_text("two\n", encoding="utf-8")
            git(self.root, "add", ".")
            git(self.root, "commit", "-q", "-m", "second")

    def run(self, **env: str) -> subprocess.CompletedProcess:
        environment = {k: v for k, v in os.environ.items() if k != "NEREUS_BUILD_TAG"}
        environment["PATH"] = str(self.bin) + os.pathsep + environment.get("PATH", "")
        environment["NEREUS_TEST_ARGS"] = str(self.args)
        # Git must not find a repository above a checkout made without one.
        environment["GIT_CEILING_DIRECTORIES"] = str(self.root.parent)
        environment.update(env)
        return subprocess.run(["sh", str(self.root / "ios" / "scripts" / "archive.sh")], cwd=self.root,
                              env=environment, capture_output=True, text=True)

    def recorded(self) -> list:
        return self.args.read_text(encoding="utf-8").splitlines()

    def close(self) -> None:
        self._temp.cleanup()


class ArchiveScript(unittest.TestCase):
    def test_the_script_parses(self) -> None:
        result = subprocess.run(["sh", "-n", str(ARCHIVE)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(os.access(ARCHIVE, os.X_OK))

    def test_it_names_no_personal_path(self) -> None:
        text = ARCHIVE.read_text(encoding="utf-8")
        self.assertNotIn("/Users/", text)
        self.assertNotIn("worktrees", text)
        self.assertNotIn("—", text)

    def test_a_release_archive_with_the_commit_count_the_version_and_the_tag(self) -> None:
        checkout = Checkout()
        try:
            result = checkout.run()
            self.assertEqual(result.returncode, 0, result.stderr)
            args = checkout.recorded()
            version = project_version()
            sha = git(checkout.root, "rev-parse", "--short", "HEAD")
            for setting in ("CURRENT_PROJECT_VERSION=2", "MARKETING_VERSION=" + version,
                            "NEREUS_BUILD_TAG=feature/x@" + sha):
                self.assertIn(setting, args)
            self.assertEqual(args[args.index("-configuration") + 1], "Release")
            self.assertEqual(args[args.index("-destination") + 1], "generic/platform=iOS")
            self.assertEqual(args[-1], "archive")
            archive = Path(args[args.index("-archivePath") + 1])
            self.assertEqual(archive.name, "NereusSDR %s (2).xcarchive" % version)
            self.assertEqual(archive.parent.resolve(), (checkout.root / "ios" / ".build" / "archive").resolve())
            self.assertIn("Archived NereusSDR %s (2)" % version, result.stdout)
        finally:
            checkout.close()

    def refused(self, checkout: Checkout, words: str, **env: str) -> None:
        result = checkout.run(**env)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(words, result.stderr)
        self.assertFalse(checkout.args.exists(), "xcodebuild ran")

    def test_a_modified_tracked_file_is_refused(self) -> None:
        checkout = Checkout()
        try:
            (checkout.root / "ios" / "notes.txt").write_text("three\n", encoding="utf-8")
            self.refused(checkout, "uncommitted changes")
        finally:
            checkout.close()

    def test_a_staged_change_is_refused(self) -> None:
        checkout = Checkout()
        try:
            (checkout.root / "ios" / "notes.txt").write_text("three\n", encoding="utf-8")
            git(checkout.root, "add", "ios/notes.txt")
            self.refused(checkout, "uncommitted changes")
        finally:
            checkout.close()

    def test_an_untracked_file_does_not_count(self) -> None:
        checkout = Checkout()
        try:
            (checkout.root / "ios" / "scratch.txt").write_text("x\n", encoding="utf-8")
            result = checkout.run()
            self.assertEqual(result.returncode, 0, result.stderr)
            sha = git(checkout.root, "rev-parse", "--short", "HEAD")
            self.assertIn("NEREUS_BUILD_TAG=feature/x@" + sha, checkout.recorded())
        finally:
            checkout.close()

    def test_a_build_tag_that_names_another_commit_is_refused(self) -> None:
        checkout = Checkout()
        try:
            first = git(checkout.root, "rev-parse", "--short", "HEAD~1")
            self.refused(checkout, "does not name HEAD", NEREUS_BUILD_TAG="feature/x@" + first)
            self.refused(checkout, "does not name HEAD", NEREUS_BUILD_TAG="my-test-build")
        finally:
            checkout.close()

    def test_a_build_tag_turned_off_off_a_release_tag_is_refused(self) -> None:
        checkout = Checkout()
        try:
            self.refused(checkout, "does not name HEAD", NEREUS_BUILD_TAG=" ")
        finally:
            checkout.close()

    def test_a_release_tag_archives_with_no_build_name(self) -> None:
        checkout = Checkout()
        try:
            git(checkout.root, "tag", "v1.0")
            result = checkout.run()
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("NEREUS_BUILD_TAG=", checkout.recorded())
        finally:
            checkout.close()

    def test_no_git_no_archive(self) -> None:
        checkout = Checkout(with_git=False)
        try:
            result = checkout.run()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("no build number", result.stderr)
            self.assertFalse(checkout.args.exists())
        finally:
            checkout.close()

    def test_an_archive_without_its_privacy_manifest_fails(self) -> None:
        checkout = Checkout()
        try:
            result = checkout.run(NEREUS_TEST_MANIFEST="no")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("PrivacyInfo.xcprivacy", result.stderr)
        finally:
            checkout.close()

    def test_a_failed_archive_fails(self) -> None:
        checkout = Checkout()
        try:
            result = checkout.run(NEREUS_TEST_XCODEBUILD_STATUS="65")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("the archive failed", result.stderr)
        finally:
            checkout.close()


if __name__ == "__main__":
    unittest.main()
