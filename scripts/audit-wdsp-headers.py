#!/usr/bin/env python3
"""Walk third_party/wdsp/src/, classify each file's licence header.

Compliance Plan Task 11. ``verify-thetis-headers.py --kind=wdsp`` (Task 7)
enforces the GPLv2-or-later markers with an explicit exemption set;
this script is the independent census that re-verifies the
WDSP-PROVENANCE.md claim (132 full-header files + 10 exempt utilities).

Use this whenever the WDSP vendored tree is re-synced from upstream to
catch drift before the verifier's exemption set goes stale.

Usage:
    python3 scripts/audit-wdsp-headers.py
    python3 scripts/audit-wdsp-headers.py --format=json
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
WDSP_SRC = REPO / "third_party" / "wdsp" / "src"

# Expected classification totals per WDSP-PROVENANCE.md.
# Updated 2026-04-23: Sub-epic C-1 added rnnr.c/.h + sbnr.c/.h (4 new GPLv2-or-later files).
# Updated 2026-05-04: issue #167 Phase 1 Agent 1C added txgain_stub.c
# (NereusSDR-original glue stub authored by J.J. Boyd KG4VCF, GPLv2-or-later
# permission block) bumping the count from 132 to 133.
# Used to flag drift when the census shifts without a docs update.
EXPECTED = {
    "gpl2-or-later": 133,
    "copyright-no-permission-block": 0,
    "no-header": 10,
}


def classify(text: str) -> str:
    # Normalise whitespace so permission text that wraps across lines
    # still matches. WDSP headers use hard-wrapped C comments like:
    #   "either version 2\nof the License, or (at your option) any later version."
    import re
    head = text[:2000]
    flat = re.sub(r"\s+", " ", head)
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

    if args.format == "json":
        print(json.dumps({
            "total": len(rows),
            "counts": dict(counts),
            "expected": EXPECTED,
            "drift": drift,
            "files": rows,
        }, indent=2))
        return 1 if drift else 0

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

    if drift:
        print(
            f"\nFAIL: WDSP-PROVENANCE.md counts disagree with tree: {drift}",
            file=sys.stderr,
        )
        return 1
    print("\nCensus matches WDSP-PROVENANCE.md ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main())
