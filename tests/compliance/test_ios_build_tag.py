#!/usr/bin/env python3
"""Tests for ios/scripts/build-tag.sh and the no-device path of
ios/scripts/device-install.sh (R-IOS-28).

build-tag.sh runs inside temporary git repositories and must give the same
tag as the desktop's cmake/NereusBuildTag.cmake. device-install.sh runs with
a stand-in xcrun first on PATH that lists no connected device and fails on
anything else, so these tests can never reach a real iPhone.
"""

from __future__ import annotations

import json
import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Optional

PROJECT = Path(__file__).resolve().parents[2]
BUILD_TAG = PROJECT / "ios" / "scripts" / "build-tag.sh"
DEVICE_INSTALL = PROJECT / "ios" / "scripts" / "device-install.sh"


def git(repo: Path, *args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=repo, check=True, capture_output=True, text=True,
    ).stdout.strip()


class Repo:
    """A throwaway repository with one commit on branch feature/x."""

    def __init__(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)
        git(self.root, "init", "-q", "-b", "feature/x")
        git(self.root, "config", "user.name", "Test")
        git(self.root, "config", "user.email", "test@example.invalid")
        git(self.root, "config", "commit.gpgsign", "false")
        git(self.root, "config", "tag.gpgsign", "false")
        (self.root / "tracked.txt").write_text("one\n")
        git(self.root, "add", "tracked.txt")
        git(self.root, "commit", "-q", "-m", "first")

    def sha(self) -> str:
        return git(self.root, "rev-parse", "--short", "HEAD")

    def tag(self, env: Optional[dict] = None) -> str:
        environment = {k: v for k, v in os.environ.items() if k != "NEREUS_BUILD_TAG"}
        environment.update(env or {})
        result = subprocess.run(
            ["sh", str(BUILD_TAG)], cwd=self.root, env=environment,
            check=True, capture_output=True, text=True,
        )
        return result.stdout

    def close(self) -> None:
        self._temp.cleanup()


class BuildTagTests(unittest.TestCase):
    def setUp(self) -> None:
        self.repo = Repo()

    def tearDown(self) -> None:
        self.repo.close()

    def test_branch_commit_gives_branch_at_sha(self) -> None:
        self.assertEqual(self.repo.tag(), "feature/x@" + self.repo.sha() + "\n")

    def test_modified_tracked_file_adds_dirty(self) -> None:
        (self.repo.root / "tracked.txt").write_text("two\n")
        self.assertEqual(self.repo.tag(), "feature/x@" + self.repo.sha() + "-dirty\n")

    def test_staged_change_adds_dirty(self) -> None:
        (self.repo.root / "tracked.txt").write_text("two\n")
        git(self.repo.root, "add", "tracked.txt")
        self.assertEqual(self.repo.tag(), "feature/x@" + self.repo.sha() + "-dirty\n")

    def test_untracked_file_does_not_count(self) -> None:
        (self.repo.root / "scratch.txt").write_text("x\n")
        self.assertEqual(self.repo.tag(), "feature/x@" + self.repo.sha() + "\n")

    def test_detached_head_gives_detached_at_sha(self) -> None:
        sha = self.repo.sha()
        git(self.repo.root, "checkout", "-q", "--detach", "HEAD")
        self.assertEqual(self.repo.tag(), "detached@" + sha + "\n")

    def test_tagged_head_gives_nothing(self) -> None:
        git(self.repo.root, "tag", "-a", "v1.0.0", "-m", "release")
        self.assertEqual(self.repo.tag(), "")

    def test_tagged_head_gives_nothing_even_when_dirty(self) -> None:
        git(self.repo.root, "tag", "v1.0.0")
        (self.repo.root / "tracked.txt").write_text("two\n")
        self.assertEqual(self.repo.tag(), "")

    def test_override_wins_outright(self) -> None:
        git(self.repo.root, "tag", "v1.0.0")
        self.assertEqual(self.repo.tag({"NEREUS_BUILD_TAG": "x"}), "x\n")

    def test_single_space_turns_the_tag_off(self) -> None:
        self.assertEqual(self.repo.tag({"NEREUS_BUILD_TAG": " "}), "")

    def test_empty_override_derives_from_git(self) -> None:
        self.assertEqual(self.repo.tag({"NEREUS_BUILD_TAG": ""}),
                         "feature/x@" + self.repo.sha() + "\n")

    def test_outside_a_repository_gives_nothing(self) -> None:
        with tempfile.TemporaryDirectory() as empty:
            environment = {k: v for k, v in os.environ.items() if k != "NEREUS_BUILD_TAG"}
            environment["GIT_CEILING_DIRECTORIES"] = str(Path(empty).parent)
            result = subprocess.run(
                ["sh", str(BUILD_TAG)], cwd=empty, env=environment,
                check=True, capture_output=True, text=True,
            )
            self.assertEqual(result.stdout, "")


FAKE_XCRUN = """#!/bin/sh
# Stand-in for xcrun: lists the devices in $FAKE_DEVICES and refuses
# everything else, so no test can reach a real device.
if [ "$1" = "devicectl" ] && [ "$2" = "list" ] && [ "$3" = "devices" ]; then
    out=""
    while [ $# -gt 0 ]; do
        if [ "$1" = "--json-output" ]; then out="$2"; fi
        shift
    done
    cp "$FAKE_DEVICES" "$out"
    exit 0
fi
echo "fake xcrun refused: $*" >&2
exit 97
"""


def simulator(name: str) -> dict:
    return {
        "identifier": "SIM-" + name,
        "properties": {
            "hardware": {"platform": "iOS", "reality": "simulated", "udid": "SIM-" + name},
            "connection": {"transportType": "sameMachine", "state": "connected"},
            "state": {"name": name},
        },
    }


def iphone(name: str, udid: str, transport: Optional[str]) -> dict:
    connection = {"pairingState": "paired"}
    if transport:
        connection["transportType"] = transport
    return {
        "identifier": "CD-" + udid,
        "properties": {
            "hardware": {"platform": "iOS", "reality": "physical", "udid": udid},
            "connection": connection,
            "state": {"name": name},
        },
    }


class DeviceInstallNoDeviceTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.dir = Path(self._temp.name)
        fake = self.dir / "bin" / "xcrun"
        fake.parent.mkdir()
        fake.write_text(FAKE_XCRUN)
        fake.chmod(fake.stat().st_mode | stat.S_IXUSR)

    def tearDown(self) -> None:
        self._temp.cleanup()

    def run_install(self, devices: list, *args: str) -> subprocess.CompletedProcess:
        listing = self.dir / "devices.json"
        listing.write_text(json.dumps({"result": {"devices": devices}}))
        environment = dict(os.environ)
        environment["PATH"] = str(self.dir / "bin") + os.pathsep + environment["PATH"]
        environment["FAKE_DEVICES"] = str(listing)
        return subprocess.run(
            ["sh", str(DEVICE_INSTALL), *args], env=environment,
            capture_output=True, text=True, timeout=60,
        )

    def test_script_parses(self) -> None:
        subprocess.run(["sh", "-n", str(DEVICE_INSTALL)], check=True)

    def test_no_iphone_connected_names_what_to_connect(self) -> None:
        result = self.run_install([simulator("iPhone 17"),
                                   iphone("JJ's iPhone", "00008150-0001", None)])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no iPhone is connected", result.stderr)
        self.assertIn("Connect your iPhone to this Mac with a cable", result.stderr)
        self.assertNotIn("fake xcrun refused", result.stderr)
        self.assertNotIn("Installing on", result.stdout)

    def test_named_iphone_not_connected(self) -> None:
        result = self.run_install([iphone("Other", "00008150-0002", "wired")], "JJ's iPhone")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no connected iPhone is called \"JJ's iPhone\"", result.stderr)
        self.assertNotIn("Installing on", result.stdout)

    def test_two_iphones_ask_for_a_name(self) -> None:
        result = self.run_install([iphone("One", "00008150-0003", "wired"),
                                   iphone("Two", "00008150-0004", "localNetwork")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("more than one iPhone is connected", result.stderr)
        self.assertIn("One (00008150-0003)", result.stderr)
        self.assertNotIn("Installing on", result.stdout)


if __name__ == "__main__":
    unittest.main()
