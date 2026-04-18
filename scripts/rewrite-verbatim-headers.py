#!/usr/bin/env python3
"""Rewrite every Thetis-derived NereusSDR source file's header block so
that the Thetis / mi0bot / Wigley / FlexRadio / Samphire license stanza
is a BYTE-FOR-BYTE verbatim copy of the upstream source's own top-of-file
header block.

Per Richard Samphire (MW0LGE)'s explicit guidance (Pass 5 of the GPL
compliance remediation). The only additions are small NereusSDR-specific
port-citation and Modification-History blocks that are stacked above the
verbatim upstream block(s).

Usage:
    python3 scripts/rewrite-verbatim-headers.py [--dry-run]
    python3 scripts/rewrite-verbatim-headers.py --file src/core/FFTEngine.cpp [--dry-run]
    python3 scripts/rewrite-verbatim-headers.py --diff --file src/core/FFTEngine.cpp
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"

THETIS_ROOT = Path("/Users/j.j.boyd/Thetis")
MI0BOT_ROOT = Path("/Users/j.j.boyd/mi0bot-Thetis")

PORT_DATE = "2026-04-17"
AUTHOR = "J.J. Boyd"
CALLSIGN = "KG4VCF"
AI_TOOL = "Anthropic Claude Code"

# Files that should be left alone regardless of provenance row.
# Hl2IoBoardTab was cured correctly in Phase 3 — per Richie's spec, detect
# and skip rather than rewrite.
SKIP_FILES = {
    "src/gui/setup/hardware/Hl2IoBoardTab.cpp",
    "src/gui/setup/hardware/Hl2IoBoardTab.h",
}

# Rows in PROVENANCE.md whose files have their header cured in this pass
# but where the cited source has NO usable top-of-file header block.
# Handled inline below by detecting empty extracted header.

# mi0bot-fork-unique files — source lives only in mi0bot-Thetis, not ramdor.
MI0BOT_ONLY_FILES = {
    "HPSDR/IoBoardHl2.cs",
    "HPSDR/clsRadioDiscovery.cs",
}


# =================================================================
# Provenance parsing
# =================================================================

def parse_provenance_tables() -> list[dict]:
    """Return list of rows: {path, sources: [source-rel-path, ...], is_mi0bot_row}."""
    rows = []
    text = PROVENANCE.read_text()

    # We care about tables where column 1 is a NereusSDR file path.
    # The mi0bot table header contains "mi0bot source" instead of "Thetis source".
    # The "Independently implemented" table has the path in column 2 —
    # those rows must be skipped.

    current_section = None
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.startswith("## "):
            current_section = line.lstrip("# ").strip()
            continue
        if not line.startswith("|"):
            continue
        if line.startswith("|---") or line.startswith("| ---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 2:
            continue
        # Filter table header rows.
        if cells[0].lower() in ("nereussdr file", "file") or "nereussdr file" in cells[0].lower():
            continue
        if cells[0].lower().startswith("behavioral resemblance"):
            continue
        # Independently implemented table has the path in column 2 by design.
        if current_section and "independently implemented" in current_section.lower():
            continue
        rel = cells[0].replace("`", "").strip()
        if not rel:
            continue
        file_path = REPO / rel
        if not file_path.is_file():
            continue
        # The source list is in column 2, semicolon-separated.
        if len(cells) < 2:
            continue
        raw_sources = cells[1]
        sources = [s.strip().replace("`", "")
                   for s in raw_sources.split(";")
                   if s.strip()]
        if not sources:
            continue
        is_mi0bot_table = (current_section and "mi0bot" in current_section.lower())
        rows.append({
            "path": rel,
            "sources": sources,
            "is_mi0bot_row": is_mi0bot_table,
        })
    return rows


# =================================================================
# Resolving Thetis source paths
# =================================================================

def resolve_source_path(source_rel: str, is_mi0bot_row: bool) -> Path | None:
    """Given a relative source path string from PROVENANCE.md, return the
    absolute Path to the source file on disk, or None if it cannot be
    resolved.
    """
    # Normalize Windows-style separators, just in case.
    src = source_rel.replace("\\", "/").strip()

    # Sources that are just a bare filename (e.g. "cmaster.c").
    if "/" not in src:
        # Try ChannelMaster for .c/.h
        if src.endswith(".c") or src.endswith(".h"):
            candidate = THETIS_ROOT / "Project Files" / "Source" / "ChannelMaster" / src
            if candidate.is_file():
                return candidate
        # Try Console for .cs
        if src.endswith(".cs"):
            candidate = THETIS_ROOT / "Project Files" / "Source" / "Console" / src
            if candidate.is_file():
                return candidate
        return None

    # For mi0bot-table rows, source paths typically start with "HPSDR/"
    # (e.g. HPSDR/clsRadioDiscovery.cs). These files usually exist at
    # "Project Files/Source/Console/HPSDR/..." in the mi0bot fork.
    if is_mi0bot_row:
        if src.startswith("HPSDR/"):
            candidate = MI0BOT_ROOT / "Project Files" / "Source" / "Console" / src
            if candidate.is_file():
                return candidate
        candidate = MI0BOT_ROOT / src
        if candidate.is_file():
            return candidate

    # For ramdor-table rows, try Thetis first.
    candidate = THETIS_ROOT / src
    if candidate.is_file():
        return candidate

    # Some rows use a full path that's really mi0bot-only (IoBoardHl2.cs, etc.)
    candidate = MI0BOT_ROOT / src
    if candidate.is_file():
        return candidate

    # Last-ditch: strip leading "Project Files/Source/" and try Console/.
    short = src
    for prefix in ("Project Files/Source/", "Project Files/", ""):
        trial = short.removeprefix(prefix)
        # Try Console, then ChannelMaster
        for dirp in ("Project Files/Source/Console",
                     "Project Files/Source/Console/HPSDR",
                     "Project Files/Source/ChannelMaster"):
            alt = THETIS_ROOT / dirp / trial
            if alt.is_file():
                return alt
            alt = MI0BOT_ROOT / dirp / trial
            if alt.is_file():
                return alt
    return None


# =================================================================
# Header extraction from upstream source
# =================================================================

def extract_source_header(source_path: Path) -> list[str]:
    """Return the verbatim lines (no trailing newlines) of the source
    file's top-of-file header block. Empty list if the source has no
    header."""
    raw = source_path.read_text(encoding="utf-8", errors="replace")
    # Strip UTF-8 BOM if present.
    if raw.startswith("\ufeff"):
        raw = raw[1:]
    lines = raw.split("\n")

    # State machine to find first non-comment non-blank line.
    # Comment forms we accept (including inside a /* */ block):
    #   - //... (line comment)
    #   - /* ... */ (single-line block comment)
    #   - /* (start of multi-line block comment)
    #   - any line while inside /* */
    #   - */ (end of multi-line block comment)
    #   - blank / whitespace-only line
    # Any other line terminates the header.
    header_lines: list[str] = []
    in_block = False

    def is_blank(s: str) -> bool:
        return s.strip() == ""

    for i, line in enumerate(lines):
        stripped = line.strip()

        if in_block:
            header_lines.append(line)
            # Does this line close the block?
            if "*/" in line:
                in_block = False
                # A block-comment close may be followed by more code on the
                # same line (rare). We don't special-case that — if there's
                # code after the close on the same line, we've still kept
                # the line as verbatim, and we'll stop header accumulation
                # on the next non-comment line.
            continue

        if is_blank(stripped):
            header_lines.append(line)
            continue

        if stripped.startswith("//"):
            header_lines.append(line)
            continue

        if stripped.startswith("/*"):
            header_lines.append(line)
            # Is the block closed on this same line?
            if "*/" in stripped[2:]:
                # closed same line
                continue
            in_block = True
            continue

        # Not a comment and not blank — end of header.
        break

    # Trim trailing blank lines from header.
    while header_lines and header_lines[-1].strip() == "":
        header_lines.pop()

    return header_lines


# =================================================================
# NereusSDR port block generation
# =================================================================

def build_nereus_blocks(nereus_rel: str,
                        source_entries: list[tuple[str, bool]],
                        has_aethersdr: bool = False) -> list[str]:
    """Return the NereusSDR port-citation + modification-history block
    lines (no trailing newline per line).

    source_entries is [(source_rel_path, has_header_bool), ...].
    """
    width = "// ================================================================="
    lines: list[str] = []
    lines.append(width)
    lines.append(f"// {nereus_rel}  (NereusSDR)")
    lines.append(width)
    lines.append("//")
    if len(source_entries) == 1:
        lines.append("// Ported from Thetis source:")
    else:
        lines.append("// Ported from Thetis sources:")
    for src, has_hdr in source_entries:
        if has_hdr:
            lines.append(f"//   {src}, original licence from Thetis source is included below")
        else:
            lines.append(f"//   {src} (upstream has no top-of-file header — project-level LICENSE applies)")
    lines.append("//")
    lines.append(width)
    lines.append("// Modification history (NereusSDR):")
    # Wrap to match Richie's FFTEngine.cpp reference exactly:
    #   //   YYYY-MM-DD — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
    #   //                 (CALLSIGN), with AI-assisted transformation via Anthropic
    #   //                 Claude Code.
    lines.append(f"//   {PORT_DATE} — Reimplemented in C++20/Qt6 for NereusSDR by {AUTHOR}")
    lines.append(f"//                 ({CALLSIGN}), with AI-assisted transformation via Anthropic")
    lines.append("//                 Claude Code.")
    if has_aethersdr:
        lines.append("//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,")
        lines.append("//                 GPLv3).")
    lines.append(width)
    return lines


def build_no_header_note(source_rel: str) -> list[str]:
    """Return the 'upstream has no header' note lines for a source that
    has no top-of-file GPL header."""
    return [
        "//",
        f"// Upstream source '{source_rel}' has no top-of-file GPL header —",
        "// project-level Thetis LICENSE applies.",
    ]


# =================================================================
# NereusSDR file editing
# =================================================================

# Sentinel comment block width matched by our NereusSDR port-citation block
_WIDE = "// ================================================================="


def find_body_start(nereus_lines: list[str]) -> int:
    """Return the line index at which the NereusSDR file's BODY begins
    (i.e. where the existing header ends and code starts).

    We want to locate the first line that is:
      - NOT part of a `//` or `/* */` comment
      - NOT blank
    Then the body starts at the first non-blank line that is code-like.

    Exception: preserve a `#pragma once` that appears at line 0 ABOVE the
    header. Our new header goes below it in that case. If `#pragma once`
    appears later (which is the typical NereusSDR convention), we leave
    it in the body.
    """
    idx = 0
    n = len(nereus_lines)

    # Skip a line-0 `#pragma once` — caller will re-insert it.
    if n > 0 and nereus_lines[0].strip() == "#pragma once":
        # Signal this via idx=-1 by returning a sentinel; the caller checks.
        pass  # handled in rewrite_file

    in_block = False

    def is_blank(s: str) -> bool:
        return s.strip() == ""

    for i, line in enumerate(nereus_lines):
        stripped = line.strip()

        if in_block:
            if "*/" in line:
                in_block = False
            continue

        if is_blank(stripped):
            continue

        if stripped.startswith("//"):
            continue

        if stripped.startswith("/*"):
            if "*/" not in stripped[2:]:
                in_block = True
            continue

        # Code line found.
        return i

    return n  # all comments / blank


def compose_new_content(nereus_rel: str,
                         source_headers: list[tuple[str, list[str]]],
                         body_lines: list[str],
                         had_pragma_once_at_top: bool,
                         has_aethersdr: bool = False) -> str:
    """Return the full new file content as a single string."""
    out: list[str] = []

    if had_pragma_once_at_top:
        out.append("#pragma once")
        out.append("")

    nereus_block = build_nereus_blocks(
        nereus_rel,
        [(sr, bool(hdr)) for sr, hdr in source_headers],
        has_aethersdr=has_aethersdr,
    )
    out.extend(nereus_block)

    for src_rel, hdr_lines in source_headers:
        out.append("")  # blank line between our block and first source
        if not hdr_lines:
            out.extend(build_no_header_note(src_rel))
        else:
            out.extend(hdr_lines)

    out.append("")  # blank separator before body
    # body_lines may start with blank lines; collapse leading blanks to 0
    while body_lines and body_lines[0].strip() == "":
        body_lines = body_lines[1:]
    out.extend(body_lines)

    return "\n".join(out) + ("\n" if not out or not out[-1].endswith("\n") else "")


# =================================================================
# Processing
# =================================================================

def detect_already_correct(first_lines: list[str]) -> bool:
    """Heuristic: already in Richie-verbatim form if the file begins with
    our new-style NereusSDR block AND contains a verbatim cue from the
    source header stanza (e.g. 'You may contact us via email at' or the
    verbatim boxed Dual-Licensing Statement we never wrote in our own
    prose, i.e. lines containing '===//' which is a hallmark of the
    Thetis ASCII box).

    Conservative default: return False so we re-write.
    """
    # If the file still contains 'Original Thetis copyright and license
    # (preserved per GNU GPL)' prose, it's the OLD form — rewrite.
    joined = "\n".join(first_lines[:150])
    if "Original Thetis copyright and license (preserved per GNU GPL)" in joined:
        return False
    if "Dual-Licensing Statement (applies ONLY to Richard Samphire" in joined:
        # Our paraphrased wrapper — rewrite.
        return False
    # Hl2IoBoardTab specifically — skip if the Richie-form marker is there.
    return False


def detect_mixed_with_aethersdr(current_text: str) -> bool:
    """Return True iff the file's current header has an AetherSDR
    attribution stanza — indicating the port is a Thetis+AetherSDR
    mix. We preserve that fact in the new Modification-History."""
    head = current_text[:5000]
    if "AetherSDR contributors" in head or "Jeremy (KK7GWY)" in head:
        return True
    return False


def process_file(row: dict, dry_run: bool, diff_mode: bool,
                  verbose: bool = False) -> dict:
    nereus_rel = row["path"]
    nereus_path = REPO / nereus_rel

    if nereus_rel in SKIP_FILES:
        return {"path": nereus_rel, "status": "skipped-explicit"}

    sources = row["sources"]
    is_mi0bot_row = row["is_mi0bot_row"]

    # Resolve all sources and extract headers.
    source_headers: list[tuple[str, list[str]]] = []
    unresolved: list[str] = []
    for src in sources:
        resolved = resolve_source_path(src, is_mi0bot_row=is_mi0bot_row)
        if resolved is None:
            unresolved.append(src)
            continue
        header_lines = extract_source_header(resolved)
        source_headers.append((src, header_lines))

    if unresolved:
        return {"path": nereus_rel,
                "status": "unresolved",
                "unresolved": unresolved}

    if not source_headers:
        return {"path": nereus_rel, "status": "no-sources"}

    # Read current file
    current = nereus_path.read_text(encoding="utf-8", errors="replace")
    current_lines = current.split("\n")
    # Remove trailing blank line caused by final newline
    if current_lines and current_lines[-1] == "":
        current_lines = current_lines[:-1]

    has_aethersdr = detect_mixed_with_aethersdr(current)

    # Detect `#pragma once` at line 0 (rare in this project).
    had_pragma_once_at_top = (len(current_lines) > 0
                              and current_lines[0].strip() == "#pragma once")
    if had_pragma_once_at_top:
        search_lines = current_lines[1:]
        # Skip an immediately-following blank after #pragma once
        while search_lines and search_lines[0].strip() == "":
            search_lines = search_lines[1:]
        body_offset = find_body_start(search_lines)
        body_lines = search_lines[body_offset:]
    else:
        body_offset = find_body_start(current_lines)
        body_lines = current_lines[body_offset:]

    new_content = compose_new_content(
        nereus_rel, source_headers, body_lines,
        had_pragma_once_at_top=had_pragma_once_at_top,
        has_aethersdr=has_aethersdr,
    )

    if new_content == current:
        return {"path": nereus_rel, "status": "unchanged"}

    if dry_run:
        if diff_mode or verbose:
            print(f"\n----- {nereus_rel} -----")
            # Show the new header region + the first couple of body lines
            header_end = 0
            newlines = new_content.split("\n")
            for i, ln in enumerate(newlines[:400]):
                if ln.strip() and not (ln.startswith("//") or ln.startswith("/*") or ln.startswith(" *") or ln.startswith("*/")):
                    header_end = i
                    break
            cutoff = max(header_end + 5, 10)
            print("\n".join(newlines[:cutoff]))
            if cutoff < len(newlines):
                print("...")
        return {"path": nereus_rel, "status": "would-change",
                "bytes_old": len(current), "bytes_new": len(new_content)}

    nereus_path.write_text(new_content, encoding="utf-8")
    return {"path": nereus_rel, "status": "rewritten"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--diff", action="store_true",
                    help="Print proposed content (implies --dry-run)")
    ap.add_argument("--file", help="Limit to a single NereusSDR file (repo-relative path)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if args.diff:
        args.dry_run = True

    rows = parse_provenance_tables()
    if args.file:
        rows = [r for r in rows if r["path"] == args.file]
        if not rows:
            print(f"ERROR: no PROVENANCE row for {args.file}", file=sys.stderr)
            return 2

    statuses = {}
    unresolved_report = []
    for row in rows:
        result = process_file(row, dry_run=args.dry_run,
                              diff_mode=args.diff,
                              verbose=args.verbose)
        status = result["status"]
        statuses.setdefault(status, 0)
        statuses[status] += 1
        if status == "unresolved":
            unresolved_report.append((result["path"], result["unresolved"]))

    print("\n===== Summary =====")
    for k, v in sorted(statuses.items()):
        print(f"  {k}: {v}")
    if unresolved_report:
        print("\nUnresolved sources:")
        for p, u in unresolved_report:
            print(f"  {p}: {u}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
