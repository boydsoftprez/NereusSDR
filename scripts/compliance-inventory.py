#!/usr/bin/env python3
"""Walk git-tracked files, classify each by upstream lineage, emit
markdown or JSON inventory.

This is the backbone of the NereusSDR GPL compliance audit (2026-04-18
full-tree audit plan, Task 4). Read by CI and by
docs/attribution/COMPLIANCE-INVENTORY.md generation (Task 5).

Usage:
  python3 scripts/compliance-inventory.py                     # markdown to stdout
  python3 scripts/compliance-inventory.py --format=json       # JSON to stdout
  python3 scripts/compliance-inventory.py --fail-on-unclassified
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

THETIS_PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
AETHER_RECONCILIATION = REPO / "docs" / "attribution" / "aethersdr-reconciliation.md"


def _git_tracked_files() -> list[str]:
    out = subprocess.check_output(
        ["git", "-C", str(REPO), "ls-files"], text=True
    )
    return [line.strip() for line in out.splitlines() if line.strip()]


def _paths_from_table(md_path: Path) -> set[str]:
    """Extract NereusSDR-relative file paths from a markdown table's first
    column. Rows that don't start with src/, tests/, or third_party/ are
    ignored (this skips table separators, legend rows, etc.).

    Handles both plain paths (src/foo.cpp) and backtick-wrapped paths
    (`src/foo.cpp`) as appear in different provenance documents."""
    if not md_path.exists():
        return set()
    paths: set[str] = set()
    for line in md_path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| "):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not cells:
            continue
        first = cells[0].strip("`")
        if first.startswith(("src/", "tests/", "third_party/")):
            paths.add(first)
    return paths


def _aethersdr_bucket_a_paths(md_path: Path) -> set[str]:
    """Bucket-A-only extractor for aethersdr-reconciliation.md.

    The reconciliation doc's tables cover four buckets (A: genuine
    derivations, B: false boilerplate, C: mixed, D: incidental mentions).
    Only Bucket A files carry a real AetherSDR derivation claim that
    needs the project-level attribution header. The inventory must not
    flag Buckets B/C/D as "aethersdr-port" — those rows are either
    deletions or incidental mentions.
    """
    if not md_path.exists():
        return set()
    paths: set[str] = set()
    in_bucket_a = False
    for line in md_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped.startswith("## Bucket A"):
            in_bucket_a = True
            continue
        if stripped.startswith("## Bucket "):
            in_bucket_a = False
            continue
        if not in_bucket_a:
            continue
        if not stripped.startswith("|") or stripped.startswith("|---"):
            continue
        cells = [c.strip() for c in stripped.strip("|").split("|")]
        if not cells:
            continue
        first = cells[0].strip("`")
        if first.startswith(("src/", "tests/")):
            paths.add(first)
    return paths


THETIS_PORTS = _paths_from_table(THETIS_PROVENANCE)
AETHER_PORTS = _aethersdr_bucket_a_paths(AETHER_RECONCILIATION)


def classify(path: str) -> str:
    if path.startswith("third_party/wdsp/"):
        return "wdsp-vendored"
    if path.startswith("third_party/fftw3/"):
        return "fftw3-vendored"
    if path in THETIS_PORTS:
        # Future refinement: split mi0bot-port vs thetis-port by inspecting
        # the PROVENANCE variant cell. For now, "thetis-port" covers both.
        return "thetis-port"
    if path in AETHER_PORTS:
        return "aethersdr-port"
    if path.startswith("docs/attribution/"):
        return "attribution-doc"
    if path.startswith("docs/"):
        return "docs"
    if path.startswith("tests/"):
        return "test"
    if path.startswith(("packaging/", ".github/")):
        return "packaging"
    if path.startswith("resources/") or path == "resources.qrc":
        return "resource"
    return "nereussdr-original"


# Required header markers per classification. Empty list = advisory-only.
REQUIRED_MARKERS: dict[str, list[str]] = {
    "thetis-port": [
        "Ported from", "Thetis", "Copyright (C)",
        "General Public License", "Modification history (NereusSDR)",
    ],
    "aethersdr-port": ["AetherSDR", "Modification history (NereusSDR)"],
    "wdsp-vendored": ["Copyright (C)", "General Public License"],
    # The following classes carry no merge-gated marker requirement; the
    # inventory records them without flagging missing markers.
    "fftw3-vendored": [],
    "nereussdr-original": [],
    "attribution-doc": [],
    "docs": [],
    "test": [],
    "packaging": [],
    "resource": [],
}

# File extensions where we actually read the head looking for markers.
# Binary and vendored non-source files are skipped even if classified in a
# bucket that has markers, to avoid false "missing" noise on e.g. the WDSP
# data-table .c files which legitimately have no GPL header (see
# WDSP-PROVENANCE.md "10 files with no license headers").
TEXT_SUFFIXES = {".c", ".h", ".hpp", ".cpp", ".cc", ".py"}

# Within wdsp-vendored, these basenames are documented as utility/data/no-
# header files per WDSP-PROVENANCE.md and must not be flagged. Keep in
# sync with WDSP_HEADER_EXEMPTIONS in scripts/verify-thetis-headers.py.
WDSP_NO_HEADER_BASENAMES = {
    "calculus.c", "calculus.h", "FDnoiseIQ.c", "FDnoiseIQ.h",
    "resource.h", "resource1.h",
    "version.c", "version.h", "fastmath.h", "fftw3.h",
}


def _verify_markers(path: str, classification: str) -> list[str]:
    markers = REQUIRED_MARKERS.get(classification, [])
    if not markers:
        return []
    abs_path = REPO / path
    if abs_path.suffix not in TEXT_SUFFIXES:
        return []
    if classification == "wdsp-vendored" and abs_path.name in WDSP_NO_HEADER_BASENAMES:
        return []
    try:
        head = abs_path.read_text(encoding="utf-8", errors="replace")[:8000]
    except (IsADirectoryError, FileNotFoundError):
        return []
    return [m for m in markers if m not in head]


def build_inventory() -> list[dict]:
    rows: list[dict] = []
    for path in _git_tracked_files():
        if (REPO / path).is_dir():
            continue
        c = classify(path)
        missing = _verify_markers(path, c)
        rows.append({
            "path": path,
            "classification": c,
            "missing_markers": missing,
        })
    return rows


def render_markdown(rows: list[dict]) -> str:
    buckets: dict[str, list[dict]] = {}
    for r in rows:
        buckets.setdefault(r["classification"], []).append(r)
    out: list[str] = []
    out.append("# Compliance Inventory")
    out.append("")
    out.append(f"Total tracked files: **{len(rows)}**")
    out.append("")
    for cls in sorted(buckets):
        items = buckets[cls]
        out.append(f"## {cls} ({len(items)})")
        out.append("")
        for r in items:
            flag = ""
            if r["missing_markers"]:
                flag = f"  warning: missing: {', '.join(r['missing_markers'])}"
            out.append(f"- `{r['path']}`{flag}")
        out.append("")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--format", choices=["markdown", "json"], default="markdown")
    ap.add_argument(
        "--fail-on-unclassified", action="store_true",
        help="Exit 1 if any file has missing required markers.",
    )
    args = ap.parse_args()

    rows = build_inventory()

    if args.format == "json":
        print(json.dumps(rows, indent=2))
    else:
        print(render_markdown(rows))

    if args.fail_on_unclassified:
        bad = [r for r in rows if r["missing_markers"]]
        if bad:
            print(f"\nFAIL: {len(bad)} files missing required markers",
                  file=sys.stderr)
            for r in bad[:20]:
                print(f"  {r['path']}  [{r['classification']}]  "
                      f"missing={r['missing_markers']}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
