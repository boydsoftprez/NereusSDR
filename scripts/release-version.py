#!/usr/bin/env python3
"""Compute and validate NereusSDR calendar release versions from Git tags."""

import argparse
from datetime import date, datetime
from pathlib import Path
import re
import subprocess
import sys

CALENDAR_BASE = r"v(20\d{2})\.([1-9]|1[0-2])\.(0|[1-9]\d*)"
CALENDAR_FINAL = re.compile(CALENDAR_BASE)
CALENDAR_TAG = re.compile(CALENDAR_BASE + r"(?:-([A-Za-z0-9.]+))?")
LEGACY_FINAL = re.compile(r"v0\.(\d+)\.(\d+)")
CMAKE_VERSION = re.compile(r"project\s*\(\s*NereusSDR\s+VERSION\s+(\d+\.\d+\.\d+)(?=\s|\))")


class ReleaseError(Exception):
    """A release rule or repository lookup failed."""


def git(repo, *args):
    result = subprocess.run(
        ["git", "-C", str(repo), *args], capture_output=True, text=True,
    )
    if result.returncode:
        # Keep the CLI contract to one diagnostic line, even for Git failures.
        message = " ".join(result.stderr.splitlines()) or "Git command failed"
        raise ReleaseError(message)
    return result.stdout.strip()


def next_version(tags, ship_date, candidate):
    counters = []
    for tag in tags:
        match = CALENDAR_FINAL.fullmatch(tag)
        if match and (int(match[1]), int(match[2])) == (ship_date.year, ship_date.month):
            counters.append(int(match[3]))
    base = f"{ship_date.year}.{ship_date.month}.{max(counters, default=-1) + 1}"
    if not candidate:
        return base
    pattern = re.compile(r"v" + re.escape(base) + r"-rc([1-9]\d*)")
    candidates = [int(match[1]) for tag in tags if (match := pattern.fullmatch(tag))]
    return f"{base}-rc{max(candidates, default=0) + 1}"


def last_release(tags):
    finals = []
    for tag in tags:
        if match := CALENDAR_FINAL.fullmatch(tag):
            finals.append(((int(match[1]), int(match[2]), int(match[3])), tag))
        elif match := LEGACY_FINAL.fullmatch(tag):
            finals.append(((0, int(match[1]), int(match[2])), tag))
    if not finals:
        raise ReleaseError("No final desktop release tag exists in this repository")
    return max(finals)[1]


def check_tag(repo, tag):
    match = CALENDAR_TAG.fullmatch(tag)
    if not match:
        raise ReleaseError(
            f"{tag}: expected vYYYY.M.N[-suffix], month 1 to 12, no leading zeros"
        )
    ref = f"refs/tags/{tag}"
    try:
        git(repo, "show-ref", "--verify", "--quiet", ref)
    except ReleaseError:
        raise ReleaseError(f"Tag {tag} does not exist in this repository") from None

    recorded_date = git(repo, "for-each-ref", "--format=%(taggerdate:iso-strict)", ref)
    if not recorded_date:
        recorded_date = git(repo, "show", "-s", "--format=%cI", f"{ref}^{{commit}}")
    tag_date = datetime.fromisoformat(recorded_date)
    year, month = int(match[1]), int(match[2])
    if (tag_date.year, tag_date.month) != (year, month):
        offset = tag_date.strftime("%z")
        offset = f"{offset[:3]}:{offset[3:]}"
        raise ReleaseError(
            f"{tag} says {year}.{month} but was tagged {tag_date.date()} ({offset})"
        )

    cmake = git(repo, "show", f"{ref}:CMakeLists.txt")
    cmake_match = CMAKE_VERSION.search(cmake)
    if not cmake_match:
        raise ReleaseError(f"Cannot read project(NereusSDR VERSION X) in CMakeLists.txt at {tag}")
    base = tag[1:].split("-", 1)[0]
    if cmake_match[1] != base:
        raise ReleaseError(f"CMakeLists.txt version ({cmake_match[1]}) != tag version ({base})")
    return f"ok: {tag} matches its tag month and CMakeLists.txt version"


def ship_date(value):
    try:
        if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", value):
            raise ValueError
        return date.fromisoformat(value)
    except ValueError:
        raise argparse.ArgumentTypeError("date must be YYYY-MM-DD") from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    next_parser = commands.add_parser("next", help="print the next desktop release version")
    next_parser.add_argument("--date", type=ship_date, default=date.today())
    next_parser.add_argument("--rc", action="store_true", help="print the next release candidate")
    last_parser = commands.add_parser("last", help="print the highest final desktop release tag")
    check_parser = commands.add_parser("check", help="validate a calendar release tag")
    check_parser.add_argument("tag")
    for command_parser in (next_parser, last_parser, check_parser):
        command_parser.add_argument("--repo", type=Path, default=Path.cwd())
    args = parser.parse_args()
    try:
        if args.command == "check":
            output = check_tag(args.repo, args.tag)
        else:
            tags = git(args.repo, "tag", "--list").splitlines()
            output = next_version(tags, args.date, args.rc) if args.command == "next" else last_release(tags)
        print(output)
        return 0
    except (ReleaseError, OSError, ValueError) as error:
        print(" ".join(str(error).splitlines()), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
