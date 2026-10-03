#!/usr/bin/env python3
"""Walk third_party/wdsp/src/, classify each file's licence header.

Compliance Plan Task 11. ``verify-thetis-headers.py --kind=wdsp`` (Task 7)
enforces the GPLv2-or-later markers with an explicit exemption set;
this script is the independent census that re-verifies the
WDSP-PROVENANCE.md claim (164 full-header files + 5 exempt utilities).

Use this whenever the WDSP vendored tree is re-synced from upstream to
catch drift before the verifier's exemption set goes stale.

It also checks the warning lists in third_party/wdsp/CMakeLists.txt
(fix wave, 2026-09-30): every source in WDSP_SOURCES that is not in
WDSP_EDITED_SOURCES builds with warnings off, so each of those must be
byte-identical to its pinned_sha256 in
docs/architecture/wdsp210-verification/source-manifest.csv, and each
edited source must differ from it. A silenced file that differs from the
pin fails the run, as does an edited file that matches it (the list is
stale either way).

Usage:
    python3 scripts/audit-wdsp-headers.py
    python3 scripts/audit-wdsp-headers.py --format=json
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
WDSP_SRC = REPO / "third_party" / "wdsp" / "src"
WDSP_CMAKE = REPO / "third_party" / "wdsp" / "CMakeLists.txt"
WDSP_MANIFEST = (REPO / "docs" / "architecture" / "wdsp210-verification"
                 / "source-manifest.csv")

# Pinned TAPR WDSP 2.10 plus retained Nereus extensions, audited 2026-09-22.
# The two Nereus ABI headers have full grants; obsolete FDnoiseIQ/fastmath
# are removed, while pinned calculus.c/.h now have upstream grants.
# 2026-09-23: Nereus dsplock.c/.h (R-R3-39) add two full-grant files.
EXPECTED = {
    "gpl2-or-later": 164,
    "copyright-no-permission-block": 0,
    "no-header": 5,
}


def classify(text: str) -> str:
    # Normalise whitespace so permission text that wraps across lines
    # still matches. WDSP headers use hard-wrapped C comments like:
    #   "either version 2\nof the License, or (at your option) any later version."
    import re
    head = text[:2000]
    # Both plain and star-prefixed block comments carry the same grant.
    flat = re.sub(r"\s+", " ", re.sub(r"(?m)^\s*\* ?", "", head))
    perm_markers = (
        "either version 2 of the License, or",
        "any later version",
    )
    if all(m in flat for m in perm_markers):
        return "gpl2-or-later"
    if "Warren Pratt" in head or "Copyright (C)" in head:
        return "copyright-no-permission-block"
    return "no-header"


def scan():
    rows = []
    if not WDSP_SRC.is_dir():
        return rows
    for p in sorted(WDSP_SRC.glob("*")):
        if p.suffix not in (".c", ".h"):
            continue
        text = p.read_text(errors="replace")
        rows.append({
            "path": str(p.relative_to(REPO)),
            "classification": classify(text),
        })
    return rows


def cmake_list(text: str, name: str) -> list[str]:
    match = re.search(r"set\(" + name + r"\s*\n(.*?)\)", text, re.S)
    if not match:
        return []
    return [entry for entry in match.group(1).split() if not entry.startswith("#")]


def check_warning_lists() -> list[str]:
    """Problems with WDSP_EDITED_SOURCES against the pinned hashes."""
    if not WDSP_CMAKE.is_file() or not WDSP_MANIFEST.is_file():
        return [f"missing {WDSP_CMAKE.relative_to(REPO)} or "
                f"{WDSP_MANIFEST.relative_to(REPO)}"]
    text = WDSP_CMAKE.read_text()
    sources = cmake_list(text, "WDSP_SOURCES")
    edited = cmake_list(text, "WDSP_EDITED_SOURCES")
    if not sources or not edited:
        return ["could not read WDSP_SOURCES or WDSP_EDITED_SOURCES"]
    with WDSP_MANIFEST.open(newline="") as handle:
        pinned = {row["path"]: row["pinned_sha256"]
                  for row in csv.DictReader(handle)}
    problems = []
    for entry in edited:
        if entry not in sources:
            problems.append(f"{entry}: in WDSP_EDITED_SOURCES but not WDSP_SOURCES")
    for entry in sources:
        path = WDSP_CMAKE.parent / entry
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        matches_pin = digest == pinned.get(Path(entry).name, "")
        if entry in edited and matches_pin:
            problems.append(f"{entry}: matches the pin but is listed as edited")
        elif entry not in edited and not matches_pin:
            problems.append(f"{entry}: differs from the pin but builds with "
                            "warnings silenced; add it to WDSP_EDITED_SOURCES")
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--format", choices=["text", "json"], default="text")
    args = ap.parse_args()

    rows = scan()
    counts = Counter(r["classification"] for r in rows)

    drift = {
        kind: counts.get(kind, 0) - expected
        for kind, expected in EXPECTED.items()
        if counts.get(kind, 0) != expected
    }

    list_problems = check_warning_lists()

    if args.format == "json":
        print(json.dumps({
            "total": len(rows),
            "counts": dict(counts),
            "expected": EXPECTED,
            "drift": drift,
            "warningListProblems": list_problems,
            "files": rows,
        }, indent=2))
        return 1 if drift or list_problems else 0

    print(f"WDSP header census — {len(rows)} files scanned under "
          f"{WDSP_SRC.relative_to(REPO)}/")
    for kind in ("gpl2-or-later", "copyright-no-permission-block", "no-header"):
        n = counts.get(kind, 0)
        expected = EXPECTED[kind]
        marker = "" if n == expected else f"  ⚠ expected {expected}"
        print(f"  {kind:35s} {n:4d}{marker}")

    no_header = [r["path"] for r in rows if r["classification"] == "no-header"]
    if no_header:
        print("\nFiles with no header (should match verifier exemption set):")
        for n in no_header:
            print(f"  {n}")

    covered = [r["path"] for r in rows
               if r["classification"] == "copyright-no-permission-block"]
    if covered:
        print("\nFiles with Copyright but no permission block "
              "(investigate — possible upstream drift):")
        for n in covered:
            print(f"  {n}")

    if list_problems:
        print("\nFAIL: WDSP_EDITED_SOURCES disagrees with the pinned hashes:",
              file=sys.stderr)
        for problem in list_problems:
            print(f"  {problem}", file=sys.stderr)
    else:
        print("\nWarning lists match the pinned hashes ✓")

    if drift:
        print(
            f"\nFAIL: WDSP-PROVENANCE.md counts disagree with tree: {drift}",
            file=sys.stderr,
        )
        return 1
    if list_problems:
        return 1
    print("\nCensus matches WDSP-PROVENANCE.md ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main())
