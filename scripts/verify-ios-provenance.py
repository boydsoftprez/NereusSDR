#!/usr/bin/env python3
"""Provenance check over the iPhone and iPad app's sources under ios/.

R-IOS-29, design decision D4. The app is NereusSDR-original, licensed GPLv3
plus an App Store permission (ios/LICENSE) that the authors of Thetis, WDSP
and AetherSDR have not granted, so none of their code or text may enter
ios/. This check fails (exit 1, one line per finding naming the file and the
rule) when:

  header    a source file (.swift .c .h .m .metal .cpp) outside a vendored
            directory does not start with the two app header lines;
  upstream  such a file names Thetis, WDSP, wdsp, AetherSDR, FlexRadio, NR0V,
            MW0LGE or Warren Pratt, or carries a "Copyright (C)" line;
  licence   a vendored directory's ios/THIRD-PARTY.md row names a licence
            outside BSD-2-Clause, BSD-3-Clause, MIT, ISC, Apache-2.0 and
            MPL-2.0;
  sha256    a vendored directory's row lacks the archive SHA-256;
  fixtures  a file under ios/ other than a test refers to tests/data/link
            (the conformance fixtures are never bundled into the app);
  licences  ios/NereusApp/Resources/Licenses.json, which the app's
            licences screen shows, lacks an entry for a THIRD-PARTY.md row,
            names a library no row lists, gives a different licence, or
            carries a notice other than the library's own licence file
            (LICENSE, LICENSE.md or COPYING in its directory), followed,
            when packaging/third-party-licenses/<name>-notices.txt exists
            (the name lowercased, spaces as hyphens), by a newline and that
            file's text: the notices in the library's source files that its
            licence file does not carry;
  wordlist  the app's copy of the pairing word list
            (NereusKit/Sources/NereusLink/Resources/pairing-words-v1.txt)
            differs from the Core's, resources/pairing-words-v1.txt, the
            Core's is missing beside the copy, or the copy is missing beside
            the Core's.

Vendored directories are the Path column of ios/THIRD-PARTY.md, relative to
ios/. Package.swift may keep its "// swift-tools-version" line first, with the
header lines straight after it. Hidden directories (.build, .swiftpm) and
DerivedData hold build output and are skipped.

Usage:
  python3 scripts/verify-ios-provenance.py [--root REPO_ROOT]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

REPO = Path(__file__).resolve().parent.parent

HEADER_PREFIX = "// NereusSDR for iOS: "
SPDX_LINE = (
    "// SPDX-License-Identifier: GPL-3.0-or-later WITH "
    "AdditionRef-NereusSDR-AppStore-permission"
)
TOOLS_VERSION_PREFIX = "// swift-tools-version"

SOURCE_SUFFIXES = {".swift", ".c", ".h", ".m", ".metal", ".cpp"}

UPSTREAM_NAMES = (
    "Thetis", "WDSP", "wdsp", "AetherSDR", "FlexRadio", "NR0V", "MW0LGE",
    "Warren Pratt",
)
COPYRIGHT_RE = re.compile(r"Copyright \([Cc]\)")

ALLOWED_LICENCES = {
    "BSD-2-Clause", "BSD-3-Clause", "MIT", "ISC", "Apache-2.0", "MPL-2.0",
}
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")

FIXTURE_PATH = b"tests/data/link"

SKIPPED_DIRS = {"DerivedData"}

LICENSES_JSON = "NereusApp/Resources/Licenses.json"
LICENCE_FILES = ("LICENSE", "LICENSE.md", "COPYING")
NOTICES_DIR = "packaging/third-party-licenses"

WORD_LIST_COPY = "NereusKit/Sources/NereusLink/Resources/pairing-words-v1.txt"
WORD_LIST = "resources/pairing-words-v1.txt"


def _cell(text: str) -> str:
    return text.strip().strip("`").strip()


def read_third_party(ios: Path) -> Tuple[List[Dict[str, str]], List[str]]:
    """Return the vendored rows of ios/THIRD-PARTY.md and any table problems.

    Columns are found by their header text, so their order does not matter.
    """
    table = ios / "THIRD-PARTY.md"
    problems: List[str] = []
    if not table.exists():
        return [], ["ios/THIRD-PARTY.md: table: file is missing"]

    rows: List[Dict[str, str]] = []
    columns: Optional[Dict[str, int]] = None
    for line in table.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped.startswith("|"):
            continue
        cells = [c.strip() for c in stripped.strip("|").split("|")]
        if all(re.fullmatch(r":?-+:?", c) for c in cells if c):
            continue
        if columns is None:
            lowered = [c.lower() for c in cells]
            found: Dict[str, int] = {}
            for index, name in enumerate(lowered):
                if "sha" in name:
                    found["sha256"] = index
                elif "licen" in name:
                    found["licence"] = index
                elif name == "path" or name.startswith("path"):
                    found["path"] = index
                elif name == "name":
                    found["name"] = index
            missing = [k for k in ("sha256", "licence", "path") if k not in found]
            if missing:
                problems.append(
                    "ios/THIRD-PARTY.md: table: header row lacks column(s) "
                    + ", ".join(missing)
                )
                return [], problems
            columns = found
            continue

        def get(key: str) -> str:
            index = columns.get(key) if columns else None
            if index is None or index >= len(cells):
                return ""
            return _cell(cells[index])

        path = get("path")
        name = get("name") or path
        rows.append({
            "name": name,
            "path": path,
            "licence": get("licence"),
            "sha256": get("sha256"),
        })
    return rows, problems


def _normalise_vendored(path: str) -> str:
    path = path.strip().strip("/")
    if path.startswith("ios/"):
        path = path[len("ios/"):]
    return path.rstrip("/")


def _is_under(relative: str, directory: str) -> bool:
    return relative == directory or relative.startswith(directory + "/")


def _is_test(relative: str) -> bool:
    """A test file: under a Tests/tests directory (any directory whose name
    ends in "Tests" counts), or a test runner script named *-test.sh."""
    parts = relative.split("/")
    for part in parts[:-1]:
        if part in ("tests", "Tests") or part.endswith("Tests"):
            return True
    return parts[-1].endswith("-test.sh")


def _walk(ios: Path) -> List[Path]:
    files: List[Path] = []
    stack = [ios]
    while stack:
        directory = stack.pop()
        for entry in sorted(directory.iterdir()):
            if entry.is_dir():
                if entry.name.startswith(".") or entry.name in SKIPPED_DIRS:
                    continue
                stack.append(entry)
            elif entry.is_file():
                files.append(entry)
    return sorted(files)


def _header_problem(relative: str, text: str) -> Optional[str]:
    lines = text.splitlines()
    if relative.split("/")[-1] == "Package.swift" and lines and \
            lines[0].startswith(TOOLS_VERSION_PREFIX):
        lines = lines[1:]
    if len(lines) < 2:
        return "the file does not start with the two app header lines"
    first, second = lines[0], lines[1]
    if not first.startswith(HEADER_PREFIX) or not first[len(HEADER_PREFIX):].strip():
        return ('line 1 must be "' + HEADER_PREFIX
                + '<what this file is for>"')
    if second.rstrip() != SPDX_LINE:
        return 'line 2 must be "' + SPDX_LINE + '"'
    return None


def notices_file(root: Path, name: str) -> Path:
    """The source notices file for the library called `name`."""
    return root / NOTICES_DIR / (name.lower().replace(" ", "-") + "-notices.txt")


def check_licenses_json(root: Path, ios: Path, rows: List[Dict[str, str]]) -> List[str]:
    """Findings for the licences screen's list against THIRD-PARTY.md."""
    shown = "ios/" + LICENSES_JSON
    path = ios / LICENSES_JSON
    named = [row for row in rows if row["name"]]
    if not path.exists():
        if not named:
            return []
        return [shown + ": licences: file is missing; the licences screen "
                "must list every THIRD-PARTY.md row"]
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (ValueError, UnicodeDecodeError) as error:
        return [shown + ": licences: not readable JSON (" + str(error) + ")"]
    libraries = data.get("libraries") if isinstance(data, dict) else None
    if not isinstance(libraries, list) or not all(isinstance(e, dict) for e in libraries):
        return [shown + ": licences: \"libraries\" must be an array of objects"]

    findings: List[str] = []
    entries: Dict[str, dict] = {}
    for entry in libraries:
        name = entry.get("name")
        if not isinstance(name, str) or not name:
            findings.append(shown + ": licences: an entry has no name")
            continue
        entries[name] = entry

    row_names = {row["name"] for row in named}
    for name in sorted(set(entries) - row_names):
        findings.append(shown + ": licences: " + name
                        + " names no ios/THIRD-PARTY.md row")

    for row in named:
        name = row["name"]
        entry = entries.get(name)
        if entry is None:
            findings.append(shown + ": licences: no entry for the ios/THIRD-PARTY.md row "
                            + name)
            continue
        if entry.get("licence") != row["licence"]:
            findings.append(shown + ": licences: " + name + " gives licence "
                            + str(entry.get("licence")) + ", the row gives "
                            + row["licence"])
        notice = entry.get("notice")
        if not isinstance(notice, str) or not notice.strip():
            findings.append(shown + ": licences: " + name + " has no notice text")
            continue
        directory = ios / _normalise_vendored(row["path"])
        notices = notices_file(root, name)
        for candidate in LICENCE_FILES:
            licence_file = directory / candidate
            if licence_file.is_file():
                expected = licence_file.read_text(encoding="utf-8", errors="replace")
                source = "ios/" + licence_file.relative_to(ios).as_posix()
                if notices.is_file():
                    expected += "\n" + notices.read_text(encoding="utf-8", errors="replace")
                    source += " followed by " + notices.relative_to(root).as_posix()
                if notice != expected:
                    findings.append(
                        shown + ": licences: " + name + "'s notice differs from " + source)
                break
    return findings


def check_word_list(root: Path, ios: Path) -> List[str]:
    """Findings for the app's copy of the pairing word list."""
    copy = ios / WORD_LIST_COPY
    shown = "ios/" + WORD_LIST_COPY
    original = root / WORD_LIST
    if not copy.is_file():
        if original.is_file():
            return [shown + ": wordlist: missing; the app needs its copy of the Core's "
                    + WORD_LIST]
        return []
    if not original.is_file():
        return [shown + ": wordlist: the Core's " + WORD_LIST + " is missing"]
    if copy.read_bytes() != original.read_bytes():
        return [shown + ": wordlist: differs from the Core's " + WORD_LIST]
    return []


def check(root: Path) -> List[str]:
    ios = root / "ios"
    if not ios.is_dir():
        return []

    findings: List[str] = []
    rows, problems = read_third_party(ios)
    findings.extend(problems)

    vendored: List[str] = []
    for row in rows:
        directory = _normalise_vendored(row["path"])
        label = "ios/THIRD-PARTY.md: row " + (row["name"] or "(unnamed)")
        if not directory:
            findings.append(label + ": path: the row names no directory")
            continue
        vendored.append(directory)
        if row["licence"] not in ALLOWED_LICENCES:
            findings.append(
                label + ": licence: " + (row["licence"] or "(empty)")
                + " is not one of " + ", ".join(sorted(ALLOWED_LICENCES))
            )
        if not SHA256_RE.match(row["sha256"]):
            findings.append(label + ": sha256: the row lacks the archive SHA-256")

    findings.extend(check_licenses_json(root, ios, rows))
    findings.extend(check_word_list(root, ios))

    for path in _walk(ios):
        relative = path.relative_to(ios).as_posix()
        shown = "ios/" + relative
        data = path.read_bytes()

        if FIXTURE_PATH in data and not _is_test(relative):
            findings.append(
                shown + ": fixtures: refers to tests/data/link; conformance "
                "fixtures are never bundled into the app"
            )

        if path.suffix not in SOURCE_SUFFIXES:
            continue
        if any(_is_under(relative, v) for v in vendored):
            continue

        text = data.decode("utf-8", errors="replace")
        problem = _header_problem(relative, text)
        if problem:
            findings.append(shown + ": header: " + problem)
        for name in UPSTREAM_NAMES:
            if name in text:
                findings.append(
                    shown + ": upstream: names " + name
                    + "; no upstream code or text may enter the app"
                )
        if COPYRIGHT_RE.search(text):
            findings.append(
                shown + ": upstream: carries a Copyright (C) line; "
                "no upstream code or text may enter the app"
            )
    return findings


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=REPO,
                        help="repository root (default: this checkout)")
    args = parser.parse_args(argv)

    findings = check(args.root.resolve())
    if findings:
        for finding in findings:
            print("FAIL " + finding, file=sys.stderr)
        print("verify-ios-provenance: " + str(len(findings))
              + " finding(s)", file=sys.stderr)
        return 1
    print("verify-ios-provenance: ios/ is clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
