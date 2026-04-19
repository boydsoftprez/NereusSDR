#!/usr/bin/env python3
"""Verify that provenance declarations stay in sync with source tree.

This script catches two classes of drift:
  1. Files in src/ or tests/ that contain "Ported from" or "From Thetis"
     markers in the first 120 lines but are NOT listed in
     docs/attribution/THETIS-PROVENANCE.md (new ports that missed
     PROVENANCE update).
  2. Rows in PROVENANCE.md that point to files no longer present on disk
     (stale rows after renames/deletes).

Exit 0 on clean, 1 on any discrepancy.
"""

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
SRC_DIR = REPO / "src"
TESTS_DIR = REPO / "tests"
HEADER_WINDOW = 120

# Markers that indicate a file is derived from Thetis (not just any "Ported from")
# These mark actual Thetis ports that require PROVENANCE entries
DERIVATION_MARKERS = ["Ported from Thetis", "From Thetis"]


def find_ported_files():
    """Scan src/ and tests/ for files with derivation markers.

    Returns set of repo-relative paths (str) that contain a Thetis marker
    in the first 120 lines.
    """
    ported = set()

    # Collect all .cpp and .h files
    for src_dir in [SRC_DIR, TESTS_DIR]:
        if not src_dir.exists():
            continue
        for filepath in src_dir.rglob("*"):
            if filepath.suffix not in (".cpp", ".h"):
                continue
            if not filepath.is_file():
                continue

            # Read first 120 lines
            try:
                head = "\n".join(
                    filepath.read_text(errors="replace").splitlines()[:HEADER_WINDOW]
                )
            except Exception:
                continue

            # Check for derivation marker
            for marker in DERIVATION_MARKERS:
                if marker in head:
                    rel = str(filepath.relative_to(REPO))
                    ported.add(rel)
                    break

    return ported


def parse_provenance_paths():
    """Parse THETIS-PROVENANCE.md and extract all declared file paths.

    Returns tuple of (declared_paths, independent_paths, path_linenos):
      - declared_paths: set of paths from derivative tables (column 0)
      - independent_paths: set of paths from "Independently implemented"
        section (column 1, index 1)
      - path_linenos: dict[path -> lineno in PROVENANCE.md] for orphan
        reporting

    Files in independent_paths are expected to NOT have "Ported from"
    markers, so they should not be flagged as missing from ported_files.
    """
    declared = set()
    independent = set()
    path_linenos: dict[str, int] = {}

    if not PROVENANCE.is_file():
        return declared, independent, path_linenos

    text = PROVENANCE.read_text()
    in_independent_section = False

    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()

        # Track when we enter the independent section
        if "Independently implemented" in line:
            in_independent_section = True
            continue

        # Skip non-data rows
        if not line.startswith("|") or line.startswith("|---"):
            continue

        # Parse cells
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells or len(cells) < 2:
            continue

        # Skip rows that are all dashes or separators
        all_cells_text = " ".join(cells)
        if "---" in all_cells_text or not cells[0] or cells[0] == "---":
            continue

        # Skip header rows by checking if first cell looks like a column header
        # Headers are typically: "NereusSDR file", "Behavioral resemblance", etc.
        # They don't start with "src/" or "tests/"
        first_cell = cells[0]
        if not (first_cell.startswith("src/") or first_cell.startswith("tests/")):
            continue

        # For independent section, extract from column 1 (index 1)
        if in_independent_section:
            candidate = cells[1].replace("`", "").strip()
            if candidate:
                independent.add(candidate)
                path_linenos.setdefault(candidate, lineno)
        else:
            # For derivative section, extract from column 0 (index 0)
            candidate = first_cell.replace("`", "").strip()
            if candidate:
                declared.add(candidate)
                path_linenos.setdefault(candidate, lineno)

    return declared, independent, path_linenos


def main():
    # Find all ported files
    ported_files = find_ported_files()

    # Find all declared files (both derivative and independent sections)
    declared_files, independent_files, path_linenos = parse_provenance_paths()

    # Files that are expected to be in PROVENANCE (either derived or independent)
    all_expected = declared_files | independent_files

    failures = 0

    # Check 1: ported files not in PROVENANCE
    missing_from_provenance = ported_files - all_expected
    for path in sorted(missing_from_provenance):
        failures += 1
        print(f"FAIL {path} — has 'Ported from' marker but not in PROVENANCE.md")

    # Check 2: files listed in PROVENANCE that don't exist on disk
    #          (Note: we only check declared_files since independent files may not exist)
    missing_on_disk = {p for p in declared_files if not (REPO / p).is_file()}
    for path in sorted(missing_on_disk):
        failures += 1
        lineno = path_linenos.get(path, "?")
        print(f"FAIL THETIS-PROVENANCE.md:{lineno}  orphan row: {path!r} not on disk")

    # Summary
    total = len(ported_files)
    ok = total - len(missing_from_provenance)
    print(f"\n{ok}/{total} files pass sync check")

    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
