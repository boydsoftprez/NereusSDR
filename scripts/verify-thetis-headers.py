#!/usr/bin/env python3
"""Verify that every file declared in THETIS-PROVENANCE.md carries the
required Thetis license header markers. Exit 1 on any failure.

Verbatim-preservation model (Pass 5, 2026-04-17 onward): each NereusSDR
file's header must contain the upstream source's own top-of-file header
BYTE-FOR-BYTE. The verifier therefore only checks for anchor markers
that every Thetis-derived file will carry:

  1. "Ported from" — anchors the NereusSDR port-citation block
  2. "Thetis"      — upstream identity (present in all cited Thetis sources)
  3. "Copyright (C)" — every cited GPL/LGPL source carries a copyright line
  4. "General Public License" — matches both GPL and LGPL
  5. "Modification history (NereusSDR)" — anchors the per-file mod block

The Dual-Licensing Statement check was dropped in Pass 5: its presence is
now 100 % determined by whether the upstream source has one in its
verbatim header, so per-variant gating is redundant.

Pass 6 (2026-04-17, Compliance Plan T12) added two structural checks
that look across the PROVENANCE table and source-file metadata:

  - **Orphan-pair check**: for every PROVENANCE-listed `.h`, require its
    sibling `.cpp`/`.cc`/`.c` (if it exists on disk) to either be listed
    in PROVENANCE too or carry an explicit opt-out marker. Same in
    reverse. Catches the T2 orphan defect class (header-cited .h with
    bare implementation .cpp) without waiting for an external auditor
    to find it.

  - **Samphire-marker consistency**: if a file's PROVENANCE source-list
    cell cites a known Samphire-authored Thetis source, require the
    file's first HEADER_WINDOW lines to contain "MW0LGE" — proves the
    upstream Samphire copyright/dual-license block actually got
    transcribed, not just paraphrased away.

Files under `docs/attribution/` themselves are exempt (they document the
templates, they are not themselves derived source).
"""

import re
import sys
from pathlib import Path
from typing import Optional

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"

REQUIRED_MARKERS = [
    "Ported from",
    "Thetis",
    "Copyright (C)",
    "General Public License",
    "Modification history (NereusSDR)",
]

# Header must appear within this many lines of top of file
HEADER_WINDOW = 160

# Opt-out marker for sibling files that intentionally carry no port header
# (e.g. pure Qt scaffolding whose semantics don't derive from the cited
# upstream). Must appear in the first HEADER_WINDOW lines of the sibling.
OPT_OUT_MARKER = "Independently implemented from"

# Sibling-pair extensions: when one of these is in PROVENANCE, look for
# the other on disk to check the orphan-pair invariant.
SIBLING_PAIRS = {
    ".h":   [".cpp", ".cc", ".c"],
    ".cpp": [".h", ".hpp"],
    ".cc":  [".h", ".hpp"],
    ".c":   [".h"],
    ".hpp": [".cpp", ".cc"],
}

# Known Samphire-authored upstream Thetis sources. If any of these appears
# in a PROVENANCE row's source-list cell, the corresponding NereusSDR file
# MUST contain "MW0LGE" in its header window — otherwise the verbatim
# Samphire copyright/dual-license block was not preserved.
SAMPHIRE_AUTHORED_SOURCES = {
    "MeterManager.cs",
    "cmaster.cs",
    "console.cs",          # heavily Samphire-modified
    "bandwidth_monitor.c",
    "bandwidth_monitor.h",
    "ucMeter.cs",
    "ucRadioList.cs",
    "frmMeterDisplay.cs",
    "frmAddCustomRadio.cs",
    "clsHardwareSpecific.cs",
    "clsDiscoveredRadioPicker.cs",
    "clsRadioDiscovery.cs",
    "DiversityForm.cs",
    "PSForm.cs",
}


def parse_provenance(text: str):
    """Yield (file_path, source_cell) tuples from the PROVENANCE.md tables.

    Table rows we care about look like:
      | src/core/WdspEngine.cpp | cmaster.cs | full | port | samphire | ... |

    Returns the first cell (relative path) and the second cell (raw
    source-file list) for each row whose path exists on disk.
    """
    rows = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 2:
            continue
        candidate = cells[0]
        if not candidate or candidate.lower() in ("nereussdr file", "file"):
            continue
        rel = candidate.replace("`", "")
        if not (REPO / rel).is_file():
            continue
        source_cell = cells[1] if len(cells) >= 2 else ""
        rows.append((rel, source_cell))
    return rows


def check_required_markers(path: Path):
    head = "\n".join(path.read_text(errors="replace").splitlines()[:HEADER_WINDOW])
    return [m for m in REQUIRED_MARKERS if m not in head]


def check_orphan_pair(rel: str, listed) -> Optional[str]:
    """Return a failure message if a sibling file exists on disk but is
    neither listed in PROVENANCE nor carries the opt-out marker.
    """
    p = Path(rel)
    suffix = p.suffix
    if suffix not in SIBLING_PAIRS:
        return None
    for sib_ext in SIBLING_PAIRS[suffix]:
        sib_rel = str(p.with_suffix(sib_ext))
        sib_path = REPO / sib_rel
        if not sib_path.is_file():
            continue
        if sib_rel in listed:
            return None  # sibling also cited — OK
        # Check for opt-out marker in the sibling
        try:
            head = "\n".join(
                sib_path.read_text(errors="replace").splitlines()[:HEADER_WINDOW]
            )
        except Exception:
            head = ""
        if OPT_OUT_MARKER in head:
            return None  # explicit opt-out — OK
        # Sibling exists, isn't listed, no opt-out: orphan
        return (
            f"orphan-sibling: {sib_rel} exists on disk but is not in "
            f"PROVENANCE and does not carry the opt-out marker "
            f'"// {OPT_OUT_MARKER} <upstream> interface". Either add a '
            f"PROVENANCE row or add the opt-out comment to its head."
        )
    return None


def check_samphire_marker(path: Path, source_cell: str) -> Optional[str]:
    """If the PROVENANCE source-list cites a Samphire-authored Thetis
    source, the file's header must contain MW0LGE.
    """
    cited = [s for s in SAMPHIRE_AUTHORED_SOURCES if s in source_cell]
    if not cited:
        return None
    head = "\n".join(path.read_text(errors="replace").splitlines()[:HEADER_WINDOW])
    if "MW0LGE" in head:
        return None
    return (
        f"missing-samphire-marker: PROVENANCE cites Samphire-authored "
        f"source(s) {sorted(cited)} but file header lacks 'MW0LGE'. "
        f"Verbatim Samphire copyright/dual-license block was likely not "
        f"transcribed."
    )


def main():
    if not PROVENANCE.is_file():
        print(f"ERROR: {PROVENANCE} not found", file=sys.stderr)
        return 2
    rows = parse_provenance(PROVENANCE.read_text())
    if not rows:
        print("ERROR: no derived-file rows parsed from PROVENANCE.md",
              file=sys.stderr)
        return 2
    listed = {rel for rel, _ in rows}
    failures = 0
    for rel, source_cell in rows:
        path = REPO / rel
        problems = []
        missing = check_required_markers(path)
        if missing:
            problems.append(f"missing-markers: {', '.join(missing)}")
        orphan = check_orphan_pair(rel, listed)
        if orphan:
            problems.append(orphan)
        samphire = check_samphire_marker(path, source_cell)
        if samphire:
            problems.append(samphire)
        if problems:
            failures += 1
            for p in problems:
                print(f"FAIL {rel} — {p}")
    total = len(rows)
    ok = total - failures
    print(f"\n{ok}/{total} files pass header check")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
