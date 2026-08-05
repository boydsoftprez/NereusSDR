#!/usr/bin/env python3
"""Verify upstream license-header markers are present in every
NereusSDR source file derived from freedv-gui (drowe67/freedv-gui).

Mirrors `scripts/verify-thetis-headers.py` shape: walks the
`docs/attribution/FREEDV-GUI-PROVENANCE.md` registry, and for every
NereusSDR file listed there, confirms the file's first ~160 lines
carry the required upstream-attribution markers.

Verbatim-preservation model: each NereusSDR file's header must contain
the upstream source's own top-of-file header BYTE-FOR-BYTE. The
verifier therefore only checks for anchor markers that every
freedv-gui-derived file will carry:

  1. "Ported from"                  - anchors the NereusSDR port-citation block
  2. "freedv-gui"                   - upstream identity
  3. "Copyright"                    - every cited LGPL/BSD source carries one
  4. "License"                      - matches LGPL or BSD-2-Clause headers
  5. "Modification history (NereusSDR)" - anchors the per-file mod block

Files under `docs/attribution/` themselves are exempt (they document
the templates, they are not themselves derived source).

Until the FREEDV-GUI-PROVENANCE.md registry has its first row, the
verifier exits 0 with an "empty registry, nothing to check" message.
This is intentional - matches the libspecbleach / rnnoise pattern
where the verifier is wired in advance of the first port.

Usage:
    python3 scripts/verify-freedv-headers.py              # default
    python3 scripts/verify-freedv-headers.py --diff       # placeholder
    python3 scripts/verify-freedv-headers.py --full-tree  # placeholder
"""

import argparse
import os
import sys
from pathlib import Path
from typing import Optional

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "FREEDV-GUI-PROVENANCE.md"
FREEDV_DIR = Path(os.environ.get(
    "NEREUS_FREEDV_DIR", "/Users/j.j.boyd/freedv-gui")).expanduser()

# Required marker set (mirrors verify-thetis-headers.py "thetis" kind
# but adapted for freedv-gui's LGPL / BSD header text).
MARKERS = [
    "Ported from",
    "freedv-gui",
    "Copyright",
    "License",
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


def parse_provenance(text: str):
    """Yield (file_path, source_cell) tuples from FREEDV-GUI-PROVENANCE.md
    tables.

    Table rows we care about look like:
      | src/integrations/RadeReceiveStep.cpp | src/pipeline/RADEReceiveStep.cpp | full | port | ... |

    Returns the first cell (relative path) and the second cell (raw
    source-file list) for each row whose path exists on disk. Skips
    placeholder rows like `(none yet - registry seeded by ...)`.
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
        # Strip backticks the way the Thetis verifier does
        rel = candidate.replace("`", "")
        if not rel or rel.lower() in ("nereussdr file", "file"):
            continue
        # Skip the "(none yet)" placeholder row so an empty registry
        # doesn't crash the verifier.
        if rel.startswith("(") and rel.endswith(")"):
            continue
        if not (REPO / rel).is_file():
            continue
        source_cell = cells[1] if len(cells) >= 2 else ""
        rows.append((rel, source_cell))
    return rows


def check_required_markers(path: Path, markers):
    head = "\n".join(
        path.read_text(errors="replace").splitlines()[:HEADER_WINDOW])
    return [m for m in markers if m not in head]


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
            return None  # sibling also cited - OK
        # Check for opt-out marker in the sibling
        try:
            head = "\n".join(
                sib_path.read_text(errors="replace").splitlines()[:HEADER_WINDOW]
            )
        except Exception:
            head = ""
        if OPT_OUT_MARKER in head:
            return None  # explicit opt-out - OK
        # Sibling exists, isn't listed, no opt-out: orphan
        return (
            f"orphan-sibling: {sib_rel} exists on disk but is not in "
            f"FREEDV-GUI-PROVENANCE and does not carry the opt-out marker "
            f'"// {OPT_OUT_MARKER} <upstream> interface". Either add a '
            f"PROVENANCE row or add the opt-out comment to its head."
        )
    return None


def upstream_present() -> bool:
    """Is a freedv-gui clone reachable on this machine?

    SKIP gracefully if absent (matches the pre-commit hook's
    NEREUS_THETIS_DIR auto-locate behavior). CI sets the env var
    explicitly so the strict check fires there.
    """
    if FREEDV_DIR.is_dir():
        return True
    for candidate in (
        REPO.parent / "freedv-gui",
        REPO.parent.parent / "freedv-gui",
        REPO.parent.parent.parent / "freedv-gui",
        REPO.parent.parent.parent.parent / "freedv-gui",
    ):
        if candidate.is_dir():
            return True
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    # CLI surface mirrors verify-thetis-headers.py for muscle-memory
    # parity. We don't currently need different "kinds" - freedv-gui
    # is a single upstream - but we accept and ignore --kind / --diff
    # / --full-tree for caller-side compatibility.
    ap.add_argument("--diff", action="store_true",
                    help="Reserved; the verifier always walks the "
                         "PROVENANCE registry (mirrors Thetis verifier "
                         "default).")
    ap.add_argument("--full-tree", action="store_true",
                    help="Reserved; same as default for now.")
    ap.add_argument("--kind", default="freedv",
                    choices=["freedv"],
                    help="Reserved; only 'freedv' kind is supported.")
    args = ap.parse_args()

    if not PROVENANCE.is_file():
        print(f"ERROR: {PROVENANCE} not found", file=sys.stderr)
        return 2

    if not upstream_present() and not os.environ.get("NEREUS_FREEDV_DIR"):
        # Local-dev convenience: SKIP if no clone is reachable. CI sets
        # NEREUS_FREEDV_DIR explicitly so the strict check fires there.
        print(
            "[freedv] SKIPPED (no freedv-gui clone found locally; searched "
            "/Users/j.j.boyd/freedv-gui, ../freedv-gui, ../../freedv-gui, "
            "../../../freedv-gui, ../../../../freedv-gui; "
            "set NEREUS_FREEDV_DIR to override; CI runs strict)"
        )
        return 0

    rows = parse_provenance(PROVENANCE.read_text())
    if not rows:
        # Empty registry is a no-op (matches the libspecbleach pattern).
        print("[freedv] empty registry; no derived files to check yet.")
        return 0

    listed = {rel for rel, _ in rows}
    failures = 0
    for rel, _source_cell in rows:
        path = REPO / rel
        problems = []
        missing = check_required_markers(path, MARKERS)
        if missing:
            problems.append(f"missing-markers: {', '.join(missing)}")
        orphan = check_orphan_pair(rel, listed)
        if orphan:
            problems.append(orphan)
        if problems:
            failures += 1
            for p in problems:
                print(f"FAIL [freedv] {rel} - {p}")
    total = len(rows)
    print(f"\n[freedv] {total - failures}/{total} files pass header check")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
