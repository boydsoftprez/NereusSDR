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
- For each cite, open the cited file under the ONE upstream that cite
  names (../Thetis, ../mi0bot-Thetis, ../deskhpsdr or ../freedv-gui,
  each configurable), extract any inline contributor tags in the
  cited line range ±5 lines, and require each tag to appear within ±10
  lines of the cite in the port.
- Upstream clones are located by walking parent directories, so this
  works from a plain checkout and from a linked worktree under
  .claude/worktrees/ without either needing to know its own depth.
- Tags recognized: developer callsigns (DH1KLM, MW0LGE, W2PA, MI0BOT,
  KD5TFD, OE3IDE, G8NJJ, NR0V, G0ORX, VK6APH) + `//-W2PA` dash form +
  `//[X.Y.Z]Author` version-prefixed form + named contributors
  (Samphire, FlexRadio, Wigley, PAVEL).

Usage:
    python3 scripts/verify-inline-tag-preservation.py           # strict
    python3 scripts/verify-inline-tag-preservation.py --audit   # report only

Exit code 0 iff no findings (strict) or always 0 in audit mode.

Configuration:
    NEREUS_THETIS_DIR:      override ../Thetis path
    NEREUS_MI0BOT_DIR:      override ../mi0bot-Thetis path
    NEREUS_FREEDV_DIR:      override ../freedv-gui path
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

def discover_sibling(name: str) -> Path:
    """Locate an upstream clone sitting beside the NereusSDR checkout.

    CLAUDE.md places upstreams as siblings of the NereusSDR root
    (../Thetis, ../mi0bot-Thetis).  Computing that from REPO is awkward
    because REPO is the *worktree* root when this runs from inside
    .claude/worktrees/<name>, and the repo root otherwise.  The previous
    fixed `REPO.parent.parent.parent / name` arithmetic resolved to
    "/Thetis" from a plain checkout and "<repo>/Thetis" from a worktree.
    Neither path exists, so every local run hit the FATAL-and-skip branch
    in main() and the hook has never actually verified a tag outside CI
    (which supplies the paths by env var and was unaffected).

    Walking ancestors is correct from both locations: a plain checkout
    finds ../Thetis one level up, and a worktree finds it four levels up,
    without either needing to know how deep it is.
    """
    for base in (REPO, *REPO.parents):
        candidate = base / name
        if candidate.is_dir():
            return candidate
    # Nothing found. Return the conventional location so the diagnostic
    # in main() names the path a developer is expected to clone into.
    return REPO.parent / name


THETIS_DIR = Path(os.environ.get(
    "NEREUS_THETIS_DIR", discover_sibling("Thetis"))).expanduser()
MI0BOT_DIR = Path(os.environ.get(
    "NEREUS_MI0BOT_DIR", discover_sibling("mi0bot-Thetis"))).expanduser()
DESKHPSDR_DIR = Path(os.environ.get(
    "NEREUS_DESKHPSDR_DIR", discover_sibling("deskhpsdr"))).expanduser()
FREEDV_DIR = Path(os.environ.get(
    "NEREUS_FREEDV_DIR", discover_sibling("freedv-gui"))).expanduser()

# Cite detectors. We scan for the upstream filename + line token; the
# stamp presence is verified by the sibling verify-inline-cites.py
# hook, so we tolerate either stamp grammar here.

# Shared line-spec grammar for every cite detector below. A cite may name
# one line (`:4821`), a range (`:25008-25072`), a comma list
# (`:340, 344, 350`), or several disjoint locations joined with `+` in
# either the tight or spaced form:
#
#   freedv_reporter.cpp:79+93-153
#   console.cs:2304-2337 + 5400-5448 + 5763
#
# `+` MUST stay in the separator class. Before it was added (Codex review,
# PR #309) the match stopped at the first location, so a cite was counted
# as checked while every tag in the discarded locations went unverified —
# exactly the silent-drop this whole script exists to prevent. Keeping the
# grammar in one constant is deliberate: the bug was four copies of the
# same regex, and fixing three of them would have looked identical to
# fixing all four.
LINE_SPEC = r"(?P<lines>\d+(?:[+,\s-]+\d+)*)"

RE_CITE_RAMDOR = re.compile(
    r"//\s*(?:From\s+Thetis|Source:)\s+"
    r"(?P<file>[\w./-]+\.(?:cs|c|h|cpp))"
    r":" + LINE_SPEC
)
RE_CITE_MI0BOT = re.compile(
    r"//\s*(?:From\s+mi0bot|Source:\s*mi0bot)\s+"
    r"(?P<file>[\w./-]+\.(?:cs|c|h|cpp))"
    r":" + LINE_SPEC
)
# Note: RE_CITE_DESKHPSDR is checked BEFORE RE_CITE_RAMDOR in the scan
# loop. The ramdor regex uses a bare "Source:" prefix which would
# otherwise also match "Source: deskhpsdr/src/..." cite lines (since
# deskhpsdr paths begin with "deskhpsdr/src/" which matches the ramdor
# [\w./-]+ file pattern). Ordering ensures deskhpsdr cites are routed to
# the right upstream before the ramdor catch-all fires.
#
# Two cite forms supported:
#   "Source: deskhpsdr/src/new_protocol.c:1480"  — file starts with deskhpsdr/
#   "From deskhpsdr src/new_protocol.c:1480"     — keyword then bare src/ path
RE_CITE_DESKHPSDR = re.compile(
    r"//\s*(?:"
    r"Source:\s+(?=deskhpsdr/)"    # "Source: " followed by deskhpsdr/ path
    r"|From\s+deskhpsdr\s+"        # "From deskhpsdr " followed by any path
    r")"
    r"(?P<file>(?:deskhpsdr/)?[\w./-]+\.(?:c|h))"
    r":" + LINE_SPEC
)
# freedv-gui, same shape and the same ordering requirement as deskhpsdr:
# "Source: freedv-gui/src/main.cpp:1971-1996" is matched by the bare
# "Source:" alternative in RE_CITE_RAMDOR, which then tries to resolve a
# freedv-gui path under the Thetis clone and reports upstream-not-found.
# The freedv-gui corpus is already merged into the tag list by
# load_corpus(), so before this detector existed those cites loaded
# contributor tags that nothing was ever checked against.
RE_CITE_FREEDV = re.compile(
    r"//\s*(?:"
    r"Source:\s+(?=freedv-gui/)"   # "Source: " followed by freedv-gui/ path
    r"|From\s+freedv-gui\s+"       # "From freedv-gui " followed by any path
    r")"
    r"(?P<file>(?:freedv-gui/)?[\w./-]+\.(?:c|h|cpp|cc|hpp))"
    r":" + LINE_SPEC
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
    """Load Thetis + deskhpsdr corpora and merge into a single tag list."""
    import json as _json

    # Thetis corpus (required)
    thetis_path = REPO / "docs" / "attribution" / "thetis-author-tags.json"
    if not thetis_path.is_file():
        print(f"FATAL: Thetis corpus {thetis_path} not found. Run "
              f"scripts/discover-thetis-author-tags.py first.",
              file=sys.stderr)
        sys.exit(2)
    thetis_data = _json.loads(thetis_path.read_text())
    callsigns = set(thetis_data.get("callsign_tags", {}).keys())
    named = set(thetis_data.get("named_tags", {}).keys())

    # deskhpsdr corpus (optional — warn if missing, don't hard-fail;
    # the corpus file is only present after running
    # discover-deskhpsdr-author-tags.py and is committed to the repo
    # from that first run onward).
    deskhpsdr_path = REPO / "docs" / "attribution" / "deskhpsdr-author-tags.json"
    if deskhpsdr_path.is_file():
        try:
            dh_data = _json.loads(deskhpsdr_path.read_text())
            callsigns.update(dh_data.get("callsign_tags", {}).keys())
            named.update(dh_data.get("named_tags", {}).keys())
        except Exception as exc:
            print(f"WARN: failed to load deskhpsdr corpus: {exc}",
                  file=sys.stderr)
    else:
        print(f"WARN: deskhpsdr corpus {deskhpsdr_path} not found. "
              f"Run scripts/discover-deskhpsdr-author-tags.py to populate it.",
              file=sys.stderr)

    # freedv-gui corpus (optional, parallel to deskhpsdr).
    # Populated by scripts/discover-freedv-author-tags.py once Phase 3J-2 + 3R
    # starts porting from drowe67/freedv-gui.
    freedv_path = REPO / "docs" / "attribution" / "freedv-gui-author-tags.json"
    if freedv_path.is_file():
        try:
            fdv_data = _json.loads(freedv_path.read_text())
            callsigns.update(fdv_data.get("callsign_tags", {}).keys())
            named.update(fdv_data.get("named_tags", {}).keys())
        except Exception as exc:
            print(f"WARN: failed to load freedv-gui corpus: {exc}",
                  file=sys.stderr)
    else:
        print(f"WARN: freedv-gui corpus {freedv_path} not found. "
              f"Run scripts/discover-freedv-author-tags.py to populate it.",
              file=sys.stderr)

    return (sorted(callsigns), sorted(named))


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
    """`25008-25072` → [25008, 25072]; `340, 344, 350` → [340, 344, 350].

    `+` joins disjoint locations and separates exactly like a comma:
    `79+93-153` → [79, 93, 153]. It must be split here as well as matched
    by LINE_SPEC — left in place it would make `79+93` a single chunk,
    and the int() below would raise ValueError and silently drop BOTH
    locations rather than just the second.
    """
    return [n for span in parse_lines_spans(tok) for n in
            (span if span[0] != span[1] else span[:1])]


def parse_lines_spans(tok: str) -> list[tuple[int, int]]:
    """Split a cite's line token into DISJOINT (lo, hi) spans.

    `25008-25072`      → [(25008, 25072)]        one contiguous range
    `340, 344, 350`    → [(340,340), (344,344), (350,350)]
    `79+93-153`        → [(79,79), (93,153)]
    `2304-2337 + 5400-5448 + 5763`
                       → [(2304,2337), (5400,5448), (5763,5763)]

    Spans matter because the caller scans upstream for author tags.
    Collapsing a cite to a flat list and scanning min..max bridges every
    line between disjoint locations: the DisplaySetupPages.cpp:907 cite
    above would sweep 2299..5768 of display.cs, roughly 3,500 lines, and
    demand that every `//MW0LGE` in that block appear in a port that only
    ever touched three small regions. That is the "invents violations"
    half of this script's original bug — keep the components separate.
    """
    spans: list[tuple[int, int]] = []
    for chunk in re.split(r"[+,\s]+", tok.strip()):
        if not chunk:
            continue
        if "-" in chunk:
            a, b = chunk.split("-", 1)
            try:
                lo, hi = int(a), int(b)
            except ValueError:
                continue
            spans.append((lo, hi) if lo <= hi else (hi, lo))
        else:
            try:
                n = int(chunk)
            except ValueError:
                continue
            spans.append((n, n))
    return spans


def resolve_upstream(cite_file: str, which: str) -> Path | None:
    """Find the cited file under the ONE upstream the cite names.

    `which` comes from whichever cite detector matched, and is honoured
    exactly: a cite that says mi0bot is resolved against the mi0bot clone
    and nowhere else. See the comment on the `bases` assignment below for
    why falling back to a sibling upstream is actively harmful.
    """
    if which in ("deskhpsdr", "freedv-gui"):
        # These cite paths are relative to the upstream repo root
        # (e.g. "src/new_protocol.c") or just a bare filename. Try both
        # direct and src/ prefix.
        for base in (_deskhpsdr_search_bases() if which == "deskhpsdr"
                     else _freedv_search_bases()):
            candidate = base / cite_file
            if candidate.is_file():
                return candidate
            basename = Path(cite_file).name
            for sub in ("src", ""):
                root = base / sub
                if not root.is_dir():
                    continue
                for found in root.rglob(basename):
                    if found.is_file():
                        return found
        return None

    # No cross-upstream fallback. mi0bot/OpenHPSDR-Thetis is a *fork* of
    # ramdor/Thetis, so both clones carry same-named files (console.cs,
    # clsHardwareSpecific.cs) whose contents and line numbering have
    # diverged. Resolving an mi0bot cite against the ramdor clone opens
    # unrelated code at the cited line number, harvests whatever author
    # tags happen to sit near it, and then reports our port as missing
    # tags it was never supposed to carry.
    #
    # That is not hypothetical: with the mi0bot clone absent this
    # produced 5 confident FAILs on clean main (PaGainProfile.cpp,
    # PaTelemetryScaling.{h,cpp}, tst_pa_gain_profile.cpp), each naming a
    # real contributor (//N1GP, //DH1KLM) and each entirely spurious. A
    # tool that fabricates GPL-attribution violations is worse than one
    # that reports it cannot check, so an absent clone now warns.
    bases = [THETIS_DIR] if which == "ramdor" else [MI0BOT_DIR]
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


def _sibling_search_bases(primary: Path, name: str) -> list[Path]:
    """Candidate base directories for an upstream, env override first."""
    candidates = [primary]
    for base in (REPO, *REPO.parents):
        p = (base / name).resolve()
        if p not in candidates:
            candidates.append(p)
    return [p for p in candidates if p.is_dir()]


def _deskhpsdr_search_bases() -> list[Path]:
    """Return candidate deskhpsdr base directories, preferring env override."""
    return _sibling_search_bases(DESKHPSDR_DIR, "deskhpsdr")


def _freedv_search_bases() -> list[Path]:
    """Return candidate freedv-gui base directories, preferring env override."""
    return _sibling_search_bases(FREEDV_DIR, "freedv-gui")


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


def extract_tags_from_region(src_path: Path, spans: list[tuple[int, int]],
                             window: int = 5) -> set[tuple[int, str]]:
    """Pull every inline tag (callsign/named/version/dash) found within
    ±window lines of each cited SPAN, EXCLUDING tags that fall inside
    the file's copyright/license header block (detected by
    find_header_end). File-header attribution is enforced separately
    by verify-thetis-headers.py.

    Each span is padded and scanned on its own. Scanning min..max across
    all spans instead would bridge the gaps between disjoint locations
    and manufacture requirements from untouched upstream code — see
    parse_lines_spans() for the cite that exposed this.
    """
    try:
        text = src_path.read_text(encoding="utf-8", errors="replace").splitlines()
    except Exception:
        return set()
    header_end = find_header_end(text)
    tags: set[tuple[int, str]] = set()
    rows: set[int] = set()
    for lo, hi in spans:
        rows.update(range(max(1, lo - window), min(len(text), hi + window) + 1))
    for row in sorted(rows):
        if row < header_end:
            continue  # inside file header; not an inline tag
        line = text[row - 1]
        # version-prefixed tag
        for m in RE_VERSION_TAG.finditer(line):
            tags.add((row, m.group(2).upper()))
        # dash form
        for m in RE_DASH_TAG.finditer(line):
            tags.add((row, m.group(1).upper()))
        # known callsigns / named (require `//` on the line so we only
        # pick comments, not string literals)
        if "//" in line:
            tail = line.split("//", 1)[1]
            for tag in KNOWN_CALLSIGNS + KNOWN_NAMED:
                if re.search(r"\b" + re.escape(tag) + r"\b", tail):
                    tags.add((row, tag.upper()))
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

    # mi0bot is the authoritative upstream for HL2 ports, so a missing
    # clone leaves every `// From mi0bot ...` cite unverified. It used to
    # be worse than unverified: the resolver fell through to the ramdor
    # clone and invented failures (see resolve_upstream). Say so loudly.
    if not MI0BOT_DIR.is_dir():
        print(f"WARN: mi0bot-Thetis not found at {MI0BOT_DIR}. Every "
              f"`// From mi0bot ...` cite will emit upstream-not-found "
              f"instead of being verified. Set NEREUS_MI0BOT_DIR or clone "
              f"to a sibling directory.", file=sys.stderr)

    # deskhpsdr is optional for now — warn if absent so local runs without
    # the clone still pass, but CI that has it cloned will do full checks.
    if not _deskhpsdr_search_bases():
        print(f"WARN: deskhpsdr not found (searched {DESKHPSDR_DIR} and "
              f"sibling directories). deskhpsdr cite checks will "
              f"emit upstream-not-found warnings rather than hard-failing. "
              f"Set NEREUS_DESKHPSDR_DIR or clone to a sibling directory.",
              file=sys.stderr)

    # freedv-gui, same treatment as deskhpsdr.
    if not _freedv_search_bases():
        print(f"WARN: freedv-gui not found (searched {FREEDV_DIR} and "
              f"sibling directories). freedv-gui cite checks will "
              f"emit upstream-not-found warnings rather than hard-failing. "
              f"Set NEREUS_FREEDV_DIR or clone to a sibling directory.",
              file=sys.stderr)

    findings = []
    cite_count = 0

    for port in iter_source_files():
        try:
            lines = port.read_text(encoding="utf-8",
                                   errors="replace").splitlines()
        except Exception:
            continue
        for ln_idx, line in enumerate(lines):
            # IMPORTANT: check most-specific upstreams first.
            # RE_CITE_DESKHPSDR must precede RE_CITE_RAMDOR because the
            # ramdor regex has a bare "Source:" prefix that also matches
            # "Source: deskhpsdr/src/..." cite lines. First match wins
            # (break at end of loop body).
            for rx, which in ((RE_CITE_MI0BOT, "mi0bot"),
                              (RE_CITE_DESKHPSDR, "deskhpsdr"),
                              (RE_CITE_FREEDV, "freedv-gui"),
                              (RE_CITE_RAMDOR, "ramdor")):
                m = rx.search(line)
                if not m:
                    continue
                cite_count += 1
                cite_line = ln_idx + 1
                spans = parse_lines_spans(m.group("lines"))
                if not spans:
                    continue
                line_nums = parse_lines_token(m.group("lines"))
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
                source_tags = extract_tags_from_region(upstream, spans)
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
