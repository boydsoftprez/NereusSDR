#!/usr/bin/env python3
"""Tree-wide audit of ``// From Thetis <file>:<line>`` inline cites.

Compliance Plan Task 8. ``scripts/check-new-ports.py`` already enforces
the version-stamp requirement on *new or modified* cites in a PR diff;
this script is the matching tree-wide sweep that catches historical
stamps missed before the diff-gate existed.

Every cite must carry a version stamp matching ``[vX.Y.Z[.W][+shortsha]]``
or ``[@shortsha]`` so a reviewer can reproduce the upstream line number
against a known Thetis revision. Grammar is documented at
``docs/attribution/HOW-TO-PORT.md §Inline cite versioning``.

Usage:
    python3 scripts/verify-inline-cites.py                    # text
    python3 scripts/verify-inline-cites.py --format=json      # JSON
    python3 scripts/verify-inline-cites.py --baseline         # print count

Findings are emitted in both modes. Exit code 0 iff the count of
``missing-stamp`` findings is <= the grandfathered baseline in
``tests/compliance/inline-cites-baseline.json`` (lets the pre-push hook
block *regressions* without requiring a tree-wide remediation first).

Use ``--strict`` to fail on any finding (CI will flip to this once the
baseline has been remediated to zero).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCAN_ROOTS = ["src", "tests"]
SCAN_SUFFIXES = {".cpp", ".cc", ".c", ".h", ".hpp"}
BASELINE_FILE = REPO / "tests" / "compliance" / "inline-cites-baseline.json"

# Reuse the exact regexes from scripts/check-new-ports.py so this
# tree-wide auditor and the PR-diff auditor stay in lockstep.
RE_THETIS_CITE = re.compile(
    r"//\s*From\s+Thetis\s+[\w./-]+\.(?:cs|c|h|cpp)(?::\d+(?:[,\s]+\d+)*)"
)
RE_HAS_VERSION_STAMP = re.compile(
    r"\[(?:v\d+(?:\.\d+)+(?:\+[0-9a-f]{7,})?|@[0-9a-f]{7,})\]"
)


def iter_source_files():
    for root in SCAN_ROOTS:
        base = REPO / root
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file():
                continue
            if p.suffix not in SCAN_SUFFIXES:
                continue
            yield p


def scan():
    """Return list of findings dicts."""
    findings = []
    for path in iter_source_files():
        try:
            text = path.read_text(errors="replace")
        except OSError:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            if not RE_THETIS_CITE.search(line):
                continue
            if RE_HAS_VERSION_STAMP.search(line):
                continue
            findings.append({
                "path": str(path.relative_to(REPO)),
                "line": lineno,
                "kind": "missing-stamp",
                "cite": line.strip(),
            })
    findings.sort(key=lambda f: (f["path"], f["line"]))
    return findings


def load_baseline() -> int:
    if not BASELINE_FILE.is_file():
        return 0
    try:
        data = json.loads(BASELINE_FILE.read_text())
    except (OSError, json.JSONDecodeError):
        return 0
    return int(data.get("missing_stamp_baseline", 0))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--format", choices=["text", "json"], default="text")
    ap.add_argument(
        "--baseline",
        action="store_true",
        help="Print the count and exit 0 — used when capturing a baseline",
    )
    ap.add_argument(
        "--strict",
        action="store_true",
        help="Fail on any finding, ignoring the grandfathered baseline",
    )
    args = ap.parse_args()

    findings = scan()
    missing = [f for f in findings if f["kind"] == "missing-stamp"]

    if args.baseline:
        print(len(missing))
        return 0

    if args.format == "json":
        print(json.dumps(findings, indent=2))
    else:
        for f in findings:
            print(f"{f['path']}:{f['line']}  [{f['kind']}]  {f['cite']}")
        print()
        print(f"{len(missing)} cite(s) without version stamp")
        if not args.strict:
            baseline = load_baseline()
            print(f"grandfathered baseline: {baseline}")

    if args.strict:
        return 1 if missing else 0
    baseline = load_baseline()
    if len(missing) > baseline:
        print(
            f"\nFAIL: missing-stamp count {len(missing)} exceeds baseline "
            f"{baseline}. Either add a version stamp "
            f"(`[vX.Y.Z.W]` or `[@shortsha]`) to the new cite(s), or "
            f"capture a new baseline with `--baseline` (only when the "
            f"regression is intentional, e.g. after a bulk upstream pull).",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
