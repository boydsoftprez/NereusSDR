#!/usr/bin/env python3
"""Verify that inline contributor tags from cited Thetis source are
preserved in the NereusSDR port.

Motivated by the 2026-04-21 DH1KLM drop discovered on Phase 3P-H: a
subagent ported `computeAlexFwdPower` / `computeRefPower` from Thetis
console.cs and silently dropped the `//DH1KLM` developer-attribution
comment on the REDPITAYA case lines. The existing hooks
(`verify-thetis-headers.py`, `check-new-ports.py`, `verify-inline-
cites.py`) validate STRUCTURE (file headers, cite stamps, PROVENANCE
rows) but do not compare ported CONTENT against the cited Thetis
lines. This script closes that gap.

Algorithm:
- Scan every `.cpp/.cc/.c/.h/.hpp` under src/ and tests/ for cites
  matching `// From Thetis <file>:<line(s)> [@sha|vX.Y.Z]` or
  `// Source: mi0bot <file>:<line(s)> [@sha]`.
- For each cite, open the cited file under ../Thetis or
  ../mi0bot-Thetis (configurable), extract any inline contributor
  tags in the cited line range ±5 lines, and require each tag to
  appear within ±10 lines of the cite in the port.
- Tags recognized: developer callsigns (DH1KLM, MW0LGE, W2PA, MI0BOT,
  KD5TFD, OE3IDE, G8NJJ, NR0V, G0ORX, VK6APH) + `//-W2PA` dash form +
  `//[X.Y.Z]Author` version-prefixed form + named contributors
  (Samphire, FlexRadio, Wigley, PAVEL).

Usage:
    python3 scripts/verify-inline-tag-preservation.py           # strict
    python3 scripts/verify-inline-tag-preservation.py --audit   # report only

Exit code 0 iff no findings (strict) or always 0 in audit mode.

Configuration:
    NEREUS_THETIS_DIR      — override ../Thetis path
    NEREUS_MI0BOT_DIR      — override ../mi0bot-Thetis path
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCAN_ROOTS = ["src", "tests"]
SCAN_SUFFIXES = {".cpp", ".cc", ".c", ".h", ".hpp"}

THETIS_DIR = Path(os.environ.get(
    "NEREUS_THETIS_DIR",
    REPO.parent.parent.parent / "Thetis")).expanduser()
MI0BOT_DIR = Path(os.environ.get(
    "NEREUS_MI0BOT_DIR",
    REPO.parent.parent.parent / "mi0bot-Thetis")).expanduser()

# Cite detectors. We scan for the Thetis filename + line token; the
# stamp presence is verified by the sibling verify-inline-cites.py
# hook, so we tolerate either stamp grammar here.
RE_CITE_RAMDOR = re.compile(
    r"//\s*(?:From\s+Thetis|Source:)\s+"
    r"(?P<file>[\w./-]+\.(?:cs|c|h|cpp))"
    r":(?P<lines>\d+(?:[,\s-]+\d+)*)"
)
RE_CITE_MI0BOT = re.compile(
    r"//\s*(?:From\s+mi0bot|Source:\s*mi0bot)\s+"
    r"(?P<file>[\w./-]+\.(?:cs|c|h|cpp))"
    r":(?P<lines>\d+(?:[,\s-]+\d+)*)"
)

# Inline tags we insist on preserving. The corpus is DISCOVERED from
# upstream Thetis source by scripts/discover-thetis-author-tags.py and
# committed to docs/attribution/thetis-author-tags.json. Using a
# discovered corpus (instead of a hardcoded list) means the check
# cannot silently miss contributors we haven't heard of — if the
# upstream file has `//NEWCALLSIGN`, the corpus contains it, so our
# port must too. CI runs the discovery in drift mode to catch
# upstream-added authors.

def load_corpus() -> tuple[list[str], list[str]]:
    corpus_path = REPO / "docs" / "attribution" / "thetis-author-tags.json"
    if not corpus_path.is_file():
        print(f"FATAL: corpus {corpus_path} not found. Run "
              f"scripts/discover-thetis-author-tags.py first.",
              file=sys.stderr)
        sys.exit(2)
    import json as _json
    data = _json.loads(corpus_path.read_text())
    return (list(data.get("callsign_tags", {}).keys()),
            list(data.get("named_tags", {}).keys()))


KNOWN_CALLSIGNS, KNOWN_NAMED = load_corpus()
# Anything matching //[X.Y.Z]Author or //[X.Y.Z.W]Author is a version
# tag carrying an author we must preserve.
RE_VERSION_TAG = re.compile(r"//\s*\[(\d+\.\d+(?:\.\d+)+)\]([A-Za-z][A-Za-z0-9_]+)")
# Anything matching //-Author is also a preservation target.
RE_DASH_TAG = re.compile(r"//\s*-\s*([A-Z][A-Z0-9]+)")


def iter_source_files():
    for root in SCAN_ROOTS:
        base = REPO / root
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if p.is_file() and p.suffix in SCAN_SUFFIXES:
                yield p


def parse_lines_token(tok: str):
    """`25008-25072` → [25008, 25072]; `340, 344, 350` → [340, 344, 350]."""
    out = []
    for chunk in re.split(r"[,\s]+", tok.strip()):
        if not chunk:
            continue
        if "-" in chunk:
            a, b = chunk.split("-", 1)
            try:
                out.append(int(a))
                out.append(int(b))
            except ValueError:
                continue
        else:
            try:
                out.append(int(chunk))
            except ValueError:
                continue
    return out


def resolve_upstream(cite_file: str, which: str) -> Path | None:
    """Find the cited file under ramdor Thetis or mi0bot Thetis."""
    bases = [THETIS_DIR] if which == "ramdor" else [MI0BOT_DIR, THETIS_DIR]
    for base in bases:
        # Direct path
        candidate = base / cite_file
        if candidate.is_file():
            return candidate
        # Search by basename under Project Files/Source and whole base
        basename = Path(cite_file).name
        for pf in ("Project Files/Source", "Project Files", ""):
            root = base / pf
            if not root.is_dir():
                continue
            for found in root.rglob(basename):
                if found.is_file():
                    return found
    return None


def find_header_end(text: list[str]) -> int:
    """Return the 1-based line where the file header (copyright /
    license block) ends. Heuristic: first line that starts with
    `#include`, `using `, `namespace `, `class `, `public:`,
    `private:`, `static `, `void `, `int `, `float `, `double `,
    `bool `, `enum `, `struct `, `typedef `, `#define`, `#ifdef`,
    `#ifndef`, `#pragma`, or similar code tokens. Anything found
    before this line is treated as file-header attribution and
    excluded from inline-tag preservation checks."""
    header_tokens = re.compile(
        r"^\s*(?:#(?:include|define|ifdef|ifndef|pragma|if)\b"
        r"|using\s|namespace\s|class\s|public:|private:|protected:"
        r"|static\s|void\s|int\s|float\s|double\s|bool\s|enum\s"
        r"|struct\s|typedef\s|template\s|extern\s|inline\s|return\s)"
    )
    for idx, line in enumerate(text):
        if header_tokens.match(line):
            return idx + 1
    return 1  # no body found; treat whole file as body to be safe


def extract_tags_from_region(src_path: Path, line_nums: list[int],
                             window: int = 5) -> set[tuple[int, str]]:
    """Pull every inline tag (callsign/named/version/dash) found within
    ±window lines of each cited line, EXCLUDING tags that fall inside
    the file's copyright/license header block (detected by
    find_header_end). File-header attribution is enforced separately
    by verify-thetis-headers.py."""
    try:
        text = src_path.read_text(encoding="utf-8", errors="replace").splitlines()
    except Exception:
        return set()
    header_end = find_header_end(text)
    tags: set[tuple[int, str]] = set()
    lo = max(1, min(line_nums) - window)
    hi = min(len(text), max(line_nums) + window)
    for i in range(lo - 1, hi):
        if (i + 1) < header_end:
            continue  # inside file header; not an inline tag
        line = text[i]
        # version-prefixed tag
        for m in RE_VERSION_TAG.finditer(line):
            tags.add((i + 1, m.group(2).upper()))
        # dash form
        for m in RE_DASH_TAG.finditer(line):
            tags.add((i + 1, m.group(1).upper()))
        # known callsigns / named (require `//` on the line so we only
        # pick comments, not string literals)
        if "//" in line:
            tail = line.split("//", 1)[1]
            for tag in KNOWN_CALLSIGNS + KNOWN_NAMED:
                if re.search(r"\b" + re.escape(tag) + r"\b", tail):
                    tags.add((i + 1, tag.upper()))
    return tags


def port_contains_tag(port_path: Path, cite_line: int, tag: str,
                      window: int = 10) -> bool:
    try:
        text = port_path.read_text(encoding="utf-8", errors="replace").splitlines()
    except Exception:
        return False
    lo = max(1, cite_line - window)
    hi = min(len(text), cite_line + window)
    for i in range(lo - 1, hi):
        if re.search(r"\b" + re.escape(tag) + r"\b", text[i]):
            return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", action="store_true",
                    help="Report findings but exit 0 (for historical sweeps)")
    ap.add_argument("--json", action="store_true",
                    help="Emit findings as JSON")
    args = ap.parse_args()

    if not THETIS_DIR.is_dir():
        print(f"FATAL: ramdor Thetis not found at {THETIS_DIR}", file=sys.stderr)
        print("Set NEREUS_THETIS_DIR or clone to ../Thetis", file=sys.stderr)
        return 2

    findings = []
    cite_count = 0

    for port in iter_source_files():
        try:
            lines = port.read_text(encoding="utf-8",
                                   errors="replace").splitlines()
        except Exception:
            continue
        for ln_idx, line in enumerate(lines):
            for rx, which in ((RE_CITE_MI0BOT, "mi0bot"),
                              (RE_CITE_RAMDOR, "ramdor")):
                m = rx.search(line)
                if not m:
                    continue
                cite_count += 1
                cite_line = ln_idx + 1
                line_nums = parse_lines_token(m.group("lines"))
                if not line_nums:
                    continue
                upstream = resolve_upstream(m.group("file"), which)
                if upstream is None:
                    findings.append({
                        "severity": "warn",
                        "file": str(port.relative_to(REPO)),
                        "cite_line": cite_line,
                        "which": which,
                        "source_file": m.group("file"),
                        "source_lines": line_nums,
                        "issue": "upstream-not-found",
                        "tag": None,
                    })
                    continue
                source_tags = extract_tags_from_region(upstream, line_nums)
                for src_line, tag in sorted(source_tags):
                    if not port_contains_tag(port, cite_line, tag):
                        findings.append({
                            "severity": "fail",
                            "file": str(port.relative_to(REPO)),
                            "cite_line": cite_line,
                            "which": which,
                            "source_file": m.group("file"),
                            "source_line": src_line,
                            "tag": tag,
                            "issue": "missing-inline-tag",
                        })
                break  # only one cite flavor per line

    # Report
    if args.json:
        import json
        print(json.dumps({"cite_count": cite_count,
                          "findings": findings}, indent=2))
    else:
        print(f"[tag-preservation] scanned {cite_count} cites across "
              f"{sum(1 for _ in iter_source_files())} files")
        for f in findings:
            if f["issue"] == "missing-inline-tag":
                print(f"  FAIL  {f['file']}:{f['cite_line']}  "
                      f"cites {f['source_file']}:{f['source_line']}  "
                      f"— missing `//{f['tag']}` tag within ±10 lines")
            else:
                print(f"  WARN  {f['file']}:{f['cite_line']}  "
                      f"{f['source_file']} — {f['issue']}")
        fails = [f for f in findings if f["severity"] == "fail"]
        if not fails:
            print("[tag-preservation] OK — no missing tags detected")
        else:
            print(f"[tag-preservation] {len(fails)} missing tag(s)")

    if args.audit:
        return 0
    return 1 if any(f["severity"] == "fail" for f in findings) else 0


if __name__ == "__main__":
    sys.exit(main())
