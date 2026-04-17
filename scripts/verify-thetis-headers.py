#!/usr/bin/env python3
"""Verify that every file declared in THETIS-PROVENANCE.md carries the
required Thetis license header markers. Exit 1 on any failure.

Required markers per header (all must appear in first 120 lines):
  1. "Ported from" (anchors the attribution block)
  2. "Thetis"
  3. "Copyright (C)"
  4. "GNU General Public License"
  5. "Modification history (NereusSDR)"

Samphire-sourced files additionally require:
  6. "Dual-Licensing Statement"

Files under `docs/attribution/` themselves are exempt (they document the
templates, they are not themselves derived source).
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"

REQUIRED_MARKERS = [
    "Ported from",
    "Thetis",
    "Copyright (C)",
    "GNU General Public License",
    "Modification history (NereusSDR)",
]
SAMPHIRE_MARKER = "Dual-Licensing Statement"

# Header must appear within this many lines of top of file
HEADER_WINDOW = 120


def parse_provenance(text: str):
    """Yield (file_path, variant) tuples from the PROVENANCE.md tables.

    Table rows we care about look like:
      | src/core/WdspEngine.cpp | cmaster.cs | full | port | samphire | ... |

    We accept any column layout whose first cell is a repo-relative path
    that exists on disk. Variant is detected from a 'variant' or 'template'
    column keyword, or inferred as 'thetis-no-samphire' if unspecified.
    """
    rows = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells:
            continue
        candidate = cells[0]
        if not candidate or candidate.lower() in ("nereussdr file", "file"):
            continue
        rel = candidate.replace("`", "")
        if not (REPO / rel).is_file():
            continue
        # Use the variant cell (index 4) exact-match to avoid false positives:
        # "thetis-no-samphire" contains the substring "samphire"; "mi0bot-solo"
        # matches any "mi0bot..." prefix. Samphire dual-license stanza is
        # required only for variants whose upstream actually has Samphire
        # content.
        variant_cell = cells[4].strip().lower() if len(cells) > 4 else ""
        if variant_cell in ("thetis-samphire", "mi0bot", "multi-source"):
            variant = "samphire-required"
        else:
            variant = "plain"
        rows.append((rel, variant))
    return rows


def check_file(path: Path, variant: str):
    head = "\n".join(path.read_text(errors="replace").splitlines()[:HEADER_WINDOW])
    missing = [m for m in REQUIRED_MARKERS if m not in head]
    if variant == "samphire-required" and SAMPHIRE_MARKER not in head:
        missing.append(SAMPHIRE_MARKER)
    return missing


def main():
    if not PROVENANCE.is_file():
        print(f"ERROR: {PROVENANCE} not found", file=sys.stderr)
        return 2
    rows = parse_provenance(PROVENANCE.read_text())
    if not rows:
        print("ERROR: no derived-file rows parsed from PROVENANCE.md",
              file=sys.stderr)
        return 2
    failures = 0
    for rel, variant in rows:
        missing = check_file(REPO / rel, variant)
        if missing:
            failures += 1
            print(f"FAIL {rel} — missing: {', '.join(missing)}")
    total = len(rows)
    ok = total - failures
    print(f"\n{ok}/{total} files pass header check")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
