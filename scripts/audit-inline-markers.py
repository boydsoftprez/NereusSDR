#!/usr/bin/env python3
"""Sample-compare inline-attribution markers between Thetis upstream
sources and the NereusSDR ports.

Compliance Plan Task 9. For every file registered in
``docs/attribution/THETIS-PROVENANCE.md``, count occurrences of the
well-known contributor markers in the upstream Thetis source(s) named
in the provenance row's source-list cell, then count the same markers
in the NereusSDR port, and emit the per-file ratio.

This is a *sampling* audit. When a port restructures code, marker
count can legitimately drop even if every retained marker was preserved
verbatim. Files below ``--threshold`` (default 0.5) are flagged for
human review, not automatic rejection. CLAUDE.md's "preserve verbatim"
rule is the authoritative standard; this script is the smoke detector.

The upstream Thetis checkout is read from ``../Thetis/`` relative to
the NereusSDR repo root (matches CLAUDE.md SOURCE-FIRST PORTING
PROTOCOL). Override with ``--thetis=PATH``.

Usage:
    python3 scripts/audit-inline-markers.py
    python3 scripts/audit-inline-markers.py --threshold=0.4
    python3 scripts/audit-inline-markers.py --format=json
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"

# Contributor markers preserved verbatim in Thetis source files. Regex
# tolerates whitespace around the dash and the callsign. Order
# intentionally lists the most common markers first — output is
# per-marker so sparse-hit files are still useful.
MARKERS = {
    "MW0LGE":   re.compile(r"//\s*MW0LGE", re.IGNORECASE),
    "W2PA":     re.compile(r"//-?\s*W2PA",  re.IGNORECASE),
    "VK6APH":   re.compile(r"//-?\s*VK6APH", re.IGNORECASE),
    "G8NJJ":    re.compile(r"//-?\s*G8NJJ",  re.IGNORECASE),
    "KD5TFD":   re.compile(r"//-?\s*KD5TFD", re.IGNORECASE),
    "MI0BOT":   re.compile(r"//\s*MI0BOT",   re.IGNORECASE),
    "DH1KLM":   re.compile(r"//\s*DH1KLM",   re.IGNORECASE),
    "N4HY":     re.compile(r"//-?\s*N4HY",   re.IGNORECASE),
    "AB2KT":    re.compile(r"//-?\s*AB2KT",  re.IGNORECASE),
}


def parse_provenance_rows(text: str):
    """Yield (nereus_rel, source_cell, line_ranges_cell) for each row."""
    out = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 3:
            continue
        first = cells[0].replace("`", "")
        if not first or first.lower() in ("nereussdr file", "file"):
            continue
        if not (REPO / first).is_file():
            continue
        out.append((first, cells[1], cells[2]))
    return out


def extract_thetis_source_paths(source_cell: str):
    """From a PROVENANCE row's source-list cell, pick file paths that
    look like real Thetis sources. A cell can hold multiple
    semicolon-separated sources with inline annotations; we keep only
    those that end in .cs/.c/.h and look like a relative path.
    """
    paths = []
    for frag in re.split(r"[;,]", source_cell):
        frag = frag.strip()
        if not frag:
            continue
        # Strip annotations like "(mi0bot/OpenHPSDR-Thetis fork)"
        frag = re.sub(r"\s*\([^)]*\)\s*$", "", frag).strip()
        if not frag.endswith((".cs", ".c", ".h")):
            continue
        paths.append(frag)
    return paths


def parse_line_ranges(cell: str):
    """Parse line-range cell like "85-184; 164-171" or "5866" or "full".

    Returns None for "full" (no scoping — full-file comparison) or an
    empty result. Otherwise a list of (start, end) 1-based inclusive
    tuples, or None for a single-line cite.
    """
    cell = cell.strip().lower()
    if not cell or cell == "full":
        return None
    ranges = []
    for frag in re.split(r"[;,]", cell):
        frag = frag.strip()
        if not frag:
            continue
        m = re.match(r"^(\d+)\s*-\s*(\d+)$", frag)
        if m:
            ranges.append((int(m.group(1)), int(m.group(2))))
            continue
        m = re.match(r"^(\d+)$", frag)
        if m:
            n = int(m.group(1))
            ranges.append((n, n))
    return ranges or None


def count_markers(text: str, ranges=None):
    """Count markers, optionally scoped to specific 1-based line ranges."""
    if ranges is None:
        return {name: len(rx.findall(text)) for name, rx in MARKERS.items()}
    lines = text.splitlines()
    selected = []
    for start, end in ranges:
        # Expand context ±20 lines to capture a marker comment that
        # precedes a ported block of code.
        lo = max(1, start - 20)
        hi = min(len(lines), end + 20)
        selected.extend(lines[lo - 1:hi])
    blob = "\n".join(selected)
    return {name: len(rx.findall(blob)) for name, rx in MARKERS.items()}


def audit(thetis_root: Path, rows):
    findings = []
    for rel, source_cell, line_ranges_cell in rows:
        ns_text = (REPO / rel).read_text(errors="replace")
        ns_counts = count_markers(ns_text)  # port is small — count whole file

        up_counts = {name: 0 for name in MARKERS}
        upstream_paths = extract_thetis_source_paths(source_cell)
        ranges = parse_line_ranges(line_ranges_cell)
        resolved = []
        for up_rel in upstream_paths:
            full = thetis_root / up_rel
            if not full.is_file():
                continue
            try:
                up_text = full.read_text(errors="replace")
            except OSError:
                continue
            resolved.append(str(full.relative_to(thetis_root)))
            per = count_markers(up_text, ranges=ranges)
            for name, n in per.items():
                up_counts[name] += n

        up_total = sum(up_counts.values())
        ns_total = sum(ns_counts.values())

        # Skip rows where we couldn't resolve any upstream file (line
        # ranges, multi-source rows with annotations that mask the path,
        # etc.) — not actionable without a tighter provenance parse.
        if not resolved:
            continue
        # Skip rows where upstream has zero markers — ratio is undefined
        # and there's nothing to preserve.
        if up_total == 0:
            continue

        ratio = ns_total / up_total if up_total else 1.0
        findings.append({
            "path": rel,
            "upstream_sources": resolved,
            "upstream_counts": up_counts,
            "port_counts": ns_counts,
            "upstream_total": up_total,
            "port_total": ns_total,
            "ratio": round(ratio, 3),
        })
    return findings


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--thetis",
        default=str((REPO.parent / "Thetis").resolve()),
        help="Path to the upstream Thetis checkout (default: ../Thetis)",
    )
    ap.add_argument(
        "--threshold",
        type=float,
        default=0.5,
        help="Flag files whose port/upstream marker ratio is below this",
    )
    ap.add_argument(
        "--format",
        choices=["text", "json"],
        default="text",
    )
    args = ap.parse_args()

    thetis_root = Path(args.thetis)
    if not thetis_root.is_dir():
        print(f"ERROR: Thetis checkout not found at {thetis_root}", file=sys.stderr)
        print(
            "  Clone https://github.com/ramdor/Thetis at this path, or pass "
            "--thetis=<path> to override.",
            file=sys.stderr,
        )
        return 2

    if not PROVENANCE.is_file():
        print(f"ERROR: {PROVENANCE} not found", file=sys.stderr)
        return 2

    rows = parse_provenance_rows(PROVENANCE.read_text())
    if not rows:
        print("ERROR: no PROVENANCE rows parsed", file=sys.stderr)
        return 2

    findings = audit(thetis_root, rows)

    flagged = [f for f in findings if f["ratio"] < args.threshold]

    if args.format == "json":
        print(json.dumps({
            "scanned": len(findings),
            "threshold": args.threshold,
            "flagged_count": len(flagged),
            "flagged": flagged,
        }, indent=2))
    else:
        print(f"Inline-marker audit — {len(findings)} scannable row(s)")
        print(f"Threshold: ratio < {args.threshold} flagged for review\n")
        if not flagged:
            print("No files below threshold. ✓")
        else:
            print(f"{len(flagged)} file(s) below threshold:\n")
            for f in flagged:
                tags = ", ".join(
                    f"{k}={f['upstream_counts'][k]}->{f['port_counts'][k]}"
                    for k in MARKERS
                    if f['upstream_counts'][k] or f['port_counts'][k]
                )
                print(
                    f"  {f['path']}  ratio={f['ratio']}  "
                    f"({f['port_total']}/{f['upstream_total']})  {tags}"
                )
                print(f"    upstream: {', '.join(f['upstream_sources'])}")
        print(f"\n{len(findings) - len(flagged)}/{len(findings)} files >= threshold")

    # Sampling audit — always exit 0 so it doesn't gate CI. Use the
    # output to populate a Task-10 triage punchlist.
    return 0


if __name__ == "__main__":
    sys.exit(main())
