#!/usr/bin/env python3
"""Check that every shipped third-party library has its licence text (R-R3-50).

packaging/third-party-licenses/ is the one place licence texts ship from:
the root CMakeLists.txt installs it, and release.yml copies it into the
macOS app and the Windows ZIP and installer. Its README.md holds two kinds
of table this script reads:

  * licence tables, whose header has a "Notice file" column: one row per
    library, naming its notice file and its full licence text file;
  * the sources table, whose header has a "Comes from" column: one row per
    library, naming where the build gets it (`third_party/<dir>`, or a
    backticked FetchContent / ExternalProject name) and its notice file.

The check fails, printing one line per problem, when:

  1. a directory under third_party/ has no sources row naming it;
  2. a FetchContent_Declare or ExternalProject_Add name in a CMake file
     has no sources row naming it (the name may sit on a later line than
     the opening parenthesis);
  3. a file a row names is missing from the folder (or a sources row
     names no notice file at all);
  4. a text file in the folder is named by no row;
  5. deepfilternet-crates.txt names a DeepFilterNet commit other than the
     one in third_party/deepfilter/COMMIT (or names none), or either setup
     script (setup-deepfilter.sh's DFNR_COMMIT, setup-deepfilter.ps1's
     $DfnrCommit) pins another commit, names none or is missing. The
     release path downloads a prebuilt library and never regenerates the
     file, so the committed file must match the commit both scripts build.

Exceptions go only through EXCEPTED_SOURCES and EXCEPTED_TEXTS below, each
with its reason.

Usage:
  python3 scripts/check-third-party-licenses.py [--root PATH]

Exit 0 when clean, 1 when any problem is found.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

LICENSE_DIR = Path("packaging/third-party-licenses")
README = LICENSE_DIR / "README.md"
CRATE_NOTICES = LICENSE_DIR / "deepfilternet-crates.txt"
DEEPFILTER_COMMIT = Path("third_party/deepfilter/COMMIT")

# Sources (a third_party/ directory name, or a FetchContent / ExternalProject
# name) that need no row. Each entry carries its reason.
EXCEPTED_SOURCES: dict[str, str] = {}

# Files in the licence folder that no row needs to name.
EXCEPTED_TEXTS: dict[str, str] = {
    "SOURCE-OFFER.txt": "the written source offer, not a licence text; "
                        "the README's file inventory names it",
}

# Directory names never searched for CMake files: VCS and tool state, build
# trees and fetched sources (a fetched library's own FetchContent calls are
# its business, not ours).
_SKIP_DIR_PREFIXES = (".", "build", "_deps", "cmake-build")

_FETCH_RE = re.compile(
    r"\b(FetchContent_Declare|ExternalProject_Add)\s*\(\s*([A-Za-z0-9_.+-]+)")
_TICK_RE = re.compile(r"`([^`]+)`")
_CRATE_COMMIT_RE = re.compile(r"^DeepFilterNet commit: (\S+)$", re.MULTILINE)

# The DeepFilterNet pin in each setup script: the file, and the assignment
# that holds it (DFNR_COMMIT="<sha>" / $DfnrCommit = "<sha>").
DEEPFILTER_SETUP_PINS: tuple[tuple[Path, re.Pattern[str]], ...] = (
    (Path("setup-deepfilter.sh"),
     re.compile(r'^\s*DFNR_COMMIT\s*=\s*"?([0-9A-Za-z]+)"?\s*$', re.MULTILINE)),
    (Path("setup-deepfilter.ps1"),
     re.compile(r'^\s*\$DfnrCommit\s*=\s*"([0-9A-Za-z]+)"\s*$', re.MULTILINE)),
)


def _split_row(line: str) -> list[str]:
    body = line.strip()
    if body.startswith("|"):
        body = body[1:]
    if body.endswith("|"):
        body = body[:-1]
    return [cell.strip() for cell in body.split("|")]


def parse_tables(text: str) -> list[tuple[list[str], list[list[str]]]]:
    """Return every Markdown table as (header cells, data rows)."""
    tables: list[tuple[list[str], list[list[str]]]] = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        nxt = lines[i + 1] if i + 1 < len(lines) else ""
        if line.lstrip().startswith("|") and re.match(r"^\s*\|[\s:|-]+\|\s*$", nxt):
            header = _split_row(line)
            rows: list[list[str]] = []
            i += 2
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append(_split_row(lines[i]))
                i += 1
            tables.append((header, rows))
            continue
        i += 1
    return tables


def _txt_names(cell: str) -> list[str]:
    return [t for t in _TICK_RE.findall(cell) if t.endswith(".txt") and "/" not in t]


def _column(header: list[str], name: str) -> int | None:
    for idx, cell in enumerate(header):
        if cell.lower() == name.lower():
            return idx
    return None


def _cell(row: list[str], idx: int | None) -> str:
    if idx is None or idx >= len(row):
        return ""
    return row[idx]


def find_fetch_names(root: Path) -> list[tuple[str, str, Path]]:
    """Every (kind, name, file) FetchContent / ExternalProject declaration."""
    found: list[tuple[str, str, Path]] = []
    stack = [root]
    while stack:
        current = stack.pop()
        for entry in sorted(current.iterdir()):
            if entry.is_dir():
                if entry.name.startswith(_SKIP_DIR_PREFIXES):
                    continue
                stack.append(entry)
                continue
            if entry.name != "CMakeLists.txt" and not entry.name.endswith(".cmake"):
                continue
            try:
                text = entry.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            # Strip comments line by line, then match over the whole file:
            # a declaration's name is often on the line after its "(".
            code = "\n".join(line.split("#", 1)[0] for line in text.splitlines())
            for match in _FETCH_RE.finditer(code):
                found.append((match.group(1), match.group(2), entry))
    return found


def check(root: Path) -> tuple[list[str], str]:
    """Return (problems, summary) for the tree at root."""
    problems: list[str] = []
    readme = root / README
    folder = root / LICENSE_DIR
    if not readme.is_file():
        return [f"{README.as_posix()} is missing"], ""

    tables = parse_tables(readme.read_text(encoding="utf-8"))
    named: dict[str, str] = {}      # file -> first row that names it
    source_cells: list[str] = []
    library_count = 0

    for header, rows in tables:
        notice_col = _column(header, "Notice file")
        full_col = _column(header, "Full licence text")
        from_col = _column(header, "Comes from")
        if notice_col is None and from_col is None:
            continue
        for row in rows:
            label = row[0] if row else "(empty row)"
            notices = _txt_names(_cell(row, notice_col))
            for name in notices + _txt_names(_cell(row, full_col)):
                named.setdefault(name, label)
            if from_col is not None:
                library_count += 1
                source_cells.append(_cell(row, from_col))
                if not notices:
                    problems.append(
                        f"{label}: its row in the sources table names no notice file")

    # 1. third_party/ directories.
    third_party = root / "third_party"
    if third_party.is_dir():
        for entry in sorted(third_party.iterdir()):
            if not entry.is_dir() or entry.name.startswith("."):
                continue
            if entry.name in EXCEPTED_SOURCES:
                continue
            token = f"third_party/{entry.name}"
            if not any(token in _TICK_RE.findall(cell) for cell in source_cells):
                problems.append(
                    f"{token} has no row naming it in {README.as_posix()}")

    # 2. FetchContent / ExternalProject names.
    reported: set[str] = set()
    for kind, name, path in find_fetch_names(root):
        if name in EXCEPTED_SOURCES or name in reported:
            continue
        if not any(name in _TICK_RE.findall(cell) for cell in source_cells):
            reported.add(name)
            rel = path.relative_to(root).as_posix()
            problems.append(
                f"{kind} {name} ({rel}) has no row naming it in {README.as_posix()}")

    # 3. Named files that are missing.
    for name, label in sorted(named.items()):
        if not (folder / name).is_file():
            problems.append(
                f"{name} is named by the {label} row but missing from "
                f"{LICENSE_DIR.as_posix()}/")

    # 4. Texts in the folder that no row names.
    if folder.is_dir():
        for entry in sorted(folder.iterdir()):
            if entry.suffix != ".txt" or entry.name in EXCEPTED_TEXTS:
                continue
            if entry.name not in named:
                problems.append(
                    f"{entry.name} in {LICENSE_DIR.as_posix()}/ is named by no row "
                    f"in {README.as_posix()}")

    # 5. The crate notices match the pinned DeepFilterNet commit.
    crates = root / CRATE_NOTICES
    pin_file = root / DEEPFILTER_COMMIT
    if crates.is_file() and pin_file.is_file():
        pinned = pin_file.read_text(encoding="utf-8").strip()
        found = _CRATE_COMMIT_RE.search(
            crates.read_text(encoding="utf-8", errors="replace"))
        if found is None:
            problems.append(
                f"{CRATE_NOTICES.as_posix()} names no DeepFilterNet commit; "
                f"regenerate it with setup-deepfilter.sh")
        elif found.group(1) != pinned:
            problems.append(
                f"{CRATE_NOTICES.as_posix()} was generated from DeepFilterNet "
                f"{found.group(1)}, but {DEEPFILTER_COMMIT.as_posix()} pins "
                f"{pinned}; regenerate it with setup-deepfilter.sh")
        # The setup scripts build that commit: both must pin it.
        for script, pin_re in DEEPFILTER_SETUP_PINS:
            path = root / script
            if not path.is_file():
                problems.append(
                    f"{script.as_posix()} is missing; it must pin DeepFilterNet "
                    f"{pinned} as {DEEPFILTER_COMMIT.as_posix()} does")
                continue
            pins = pin_re.findall(path.read_text(encoding="utf-8", errors="replace"))
            if not pins:
                problems.append(
                    f"{script.as_posix()} names no DeepFilterNet commit; it must "
                    f"pin {pinned} as {DEEPFILTER_COMMIT.as_posix()} does")
            for other in sorted(set(pins) - {pinned}):
                problems.append(
                    f"{script.as_posix()} pins DeepFilterNet {other}, but "
                    f"{DEEPFILTER_COMMIT.as_posix()} pins {pinned}; the two "
                    f"setup scripts, the COMMIT file and "
                    f"{CRATE_NOTICES.as_posix()} must name one commit")

    summary = (f"third-party licences: {library_count} libraries, "
               f"{len(named)} named files, all present")
    return problems, summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parent.parent,
                        help="repository root (default: this script's repository)")
    args = parser.parse_args(argv)
    problems, summary = check(args.root.resolve())
    if problems:
        for problem in problems:
            print(problem)
        return 1
    print(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
