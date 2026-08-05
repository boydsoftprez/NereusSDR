#!/usr/bin/env python3
"""Detect new or modified C/C++ source files in a PR diff that look like
ports of Thetis code but are not registered in THETIS-PROVENANCE.md.

Closes the gap left by `verify-thetis-headers.py`, which only checks files
already in PROVENANCE: a contributor could write a brand-new file that
ports a Thetis source (e.g. cmaster.cs) and leave it out of PROVENANCE,
and the existing verifier would never see it. That is exactly the
defect class Samphire flagged on MeterManager.cs in the v0.1.x series.

Heuristics (any one match flags the file):

  - Thetis contributor callsigns (MW0LGE, W2PA, W5WC, VK6APH, KK7GWY,
    MI0BOT, G8NJJ, NR0V, G0ORX, KD5TFD, DH1KLM, OE3IDE, …)
  - References to known Thetis source filenames (MeterManager.cs,
    console.cs, cmaster.cs, bandwidth_monitor.{c,h}, IoBoardHl2.cs, …)
  - C# class-name patterns (cls*/uc*/frm*) appearing in C++ source —
    these are Samphire-era Thetis conventions and are very unusual in
    NereusSDR-original code
  - Explicit `// Source:` or `// From <thetis-file>` citation comments

Skip conditions (file is not flagged):

  - Path is already in `THETIS-PROVENANCE.md` (existing verifier owns it)
  - File header contains `Independently implemented from` (T12 opt-out)
  - File header contains `no-port-check: <reason>` (per-file escape hatch
    for genuine false positives — e.g. a docstring that mentions a Thetis
    file purely for context, not as a derivation claim)

Diff range: `git merge-base BASE_REF HEAD`..HEAD, computed once. Override
BASE_REF via the CHECK_NEW_PORTS_BASE_REF env var (default: origin/main).

Full-tree mode: set `CHECK_NEW_PORTS_FULL=1` (or pass `--full-tree` on the
CLI) to walk every `src/**/*.{cpp,h,...}` instead of the diff. This closes
the historical-gap case: files committed before the heuristic check
existed that slipped through the diff-only gate. The cure is the same —
PROVENANCE/reconciliation row, `Independently implemented from` marker,
or `no-port-check:` escape hatch. Full-tree mode also consults the
AetherSDR and WDSP provenance docs when computing the "listed" set.

Exit 0 on clean, 1 on any flagged file.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
WDSP_PROVENANCE = REPO / "docs" / "attribution" / "WDSP-PROVENANCE.md"
AETHER_RECONCILIATION = REPO / "docs" / "attribution" / "aethersdr-reconciliation.md"
FREEDV_PROVENANCE = REPO / "docs" / "attribution" / "FREEDV-GUI-PROVENANCE.md"
BASE_REF = os.environ.get("CHECK_NEW_PORTS_BASE_REF", "origin/main")
FULL_TREE = (
    os.environ.get("CHECK_NEW_PORTS_FULL") == "1"
    or "--full-tree" in sys.argv
)

# C/C++ source extensions only — scripts/, docs/, tests/ Python, YAML,
# Markdown all skipped automatically.
EXTENSIONS = {".cpp", ".h", ".c", ".cc", ".hpp", ".hxx"}

# Header window used to detect the per-file skip markers.
HEADER_WINDOW = 160

OPT_OUT_MARKER = "Independently implemented from"
NO_PORT_CHECK_MARKER = "no-port-check:"

CALLSIGNS = [
    # Thetis (and Thetis-fork) contributors. KK7GWY is AetherSDR's primary
    # author and is listed in full-tree mode only — see AETHER_CALLSIGNS.
    "MW0LGE", "W2PA", "W5WC", "VK6APH", "MI0BOT", "G8NJJ", "NR0V",
    "G0ORX", "KD5TFD", "DH1KLM", "W4WMT", "N1GP", "KE9NS", "K5SO",
    "OE3IDE", "KD0OSS", "AA6E",
]

# Extra tells that mark a file as AetherSDR-derived. Only consulted in
# full-tree mode because AetherSDR provenance lives in
# `aethersdr-reconciliation.md`, which diff-mode doesn't consult.
AETHER_CALLSIGNS = ["KK7GWY"]
AETHER_FILES = [
    "AppletPanel", "CatApplet", "CwxPanel", "DvkPanel", "TunerApplet",
    "EqApplet", "PhoneApplet", "PhoneCwApplet", "RxApplet", "TxApplet",
    "VfoWidget", "SpectrumOverlayMenu", "SpectrumWidget", "FilterPassbandWidget",
    "GuardedSlider", "AudioEngine", "RadioSetupDialog", "RadioDiscovery",
    "RadioModel", "SliceModel", "PanadapterModel",
]

# freedv-gui contributor callsigns. Sparse compared to Thetis - only a
# handful appear in the freedv-gui source tree (verified by
# scripts/discover-freedv-author-tags.py). Listed here so a contributor
# can't drop one during a port without flagging the new-port detector.
# Always-on (diff + full-tree) because freedv-gui callsigns are
# distinctive enough to over-trigger only on actual ports.
FREEDV_CALLSIGNS = ["VK5DGR", "K6AQ", "KD0EAG", "NH6Z", "N2ADR"]
# Distinctive freedv-gui source filenames. Same precaution as
# AETHER_FILES: limited to bases unlikely to false-positive against
# NereusSDR-original code. Full-tree-only and gated on a "freedv-gui"
# sibling marker (see RE_FREEDV_FILE_NEAR_MARKER).
FREEDV_FILES = [
    "RADEReceiveStep", "RADETransmitStep", "rade_text", "FreeDVReporter",
    "pskreporter", "EqualizerStep", "AgcStep", "freedv_reporter",
    "freedv_interface",
]

# Distinctive Thetis source filenames. Limited to bases that are unlikely
# to false-positive against NereusSDR-original code (e.g. "Setup" alone
# would over-trigger; "setup.cs" with the .cs extension is specific to
# the C# upstream).
THETIS_FILES = [
    "MeterManager", "console", "cmaster", "setup", "display", "radio",
    "networkproto1", "NetworkIO", "bandwidth_monitor", "clsMMIO",
    "clsHardwareSpecific", "clsDiscoveredRadioPicker", "clsRadioDiscovery",
    "ucMeter", "ucRadioList", "frmAddCustomRadio", "frmMeterDisplay",
    "frmVariablePicker", "specHPSDR", "IoBoardHl2", "enums", "DiversityForm",
    "PSForm", "dsp", "protocol2", "RXA", "TXA",
]

# Compiled patterns. \b word boundaries keep the matches strict.
RE_CALLSIGN = re.compile(r"\b(" + "|".join(CALLSIGNS) + r")\b")
RE_THETIS_FILE = re.compile(
    r"\b(" + "|".join(re.escape(f) for f in THETIS_FILES) + r")\.(cs|c|h|cpp)\b"
)
RE_THETIS_CLASS = re.compile(r"\b(cls|uc|frm)[A-Z]\w{2,}\b")
RE_SOURCE_COMMENT = re.compile(
    r"//\s*(Source|From|Ported from)\s*[:\-]?\s*.*\b(thetis|MeterManager|console\.cs|cmaster\.cs|bandwidth_monitor|IoBoardHl2)\b",
    re.IGNORECASE,
)
# AetherSDR tells (full-tree only). AETHER_FILE names overlap with NereusSDR's
# own class names (`RadioModel`, `SliceModel`, `AudioEngine`, …), so the bare
# filename match would fire on every downstream user. Require the word
# "AetherSDR" on the same line.
RE_AETHER_CALLSIGN = re.compile(r"\b(" + "|".join(AETHER_CALLSIGNS) + r")\b")
RE_AETHER_FILE_NEAR_MARKER = re.compile(
    r"\bAetherSDR\b.*\b("
    + "|".join(re.escape(f) for f in AETHER_FILES)
    + r")\b|\b("
    + "|".join(re.escape(f) for f in AETHER_FILES)
    + r")\b.*\bAetherSDR\b",
    re.IGNORECASE,
)
RE_AETHER_COMMENT = re.compile(
    r"//\s*(Source|From|Ported from|Layout from|Pattern from).*\bAetherSDR\b",
    re.IGNORECASE,
)

# Diff-mode-only: enforce that every new/modified `// From Thetis
# <file>.<ext>:<line>` cite carries a version stamp — either a Thetis
# release tag `[v2.10.3.13]` or a short SHA `[@abc1234]` (optionally
# combined `[v2.10.3.13+abc1234]` for post-tag fixes pulled before the
# next release).
RE_THETIS_CITE = re.compile(
    r"//\s*From\s+Thetis\s+[\w./-]+\.(?:cs|c|h|cpp)(?::\d+(?:[,\s]+\d+)*)"
)
RE_HAS_VERSION_STAMP = re.compile(
    r"\[(?:v\d+(?:\.\d+)+(?:\+[0-9a-f]{7,})?|@[0-9a-f]{7,})\]"
)

# freedv-gui-specific patterns (mirror the AetherSDR set above).
RE_FREEDV_CALLSIGN = re.compile(r"\b(" + "|".join(FREEDV_CALLSIGNS) + r")\b")
RE_FREEDV_FILE_NEAR_MARKER = re.compile(
    r"\bfreedv-gui\b.*\b("
    + "|".join(re.escape(f) for f in FREEDV_FILES)
    + r")\b|\b("
    + "|".join(re.escape(f) for f in FREEDV_FILES)
    + r")\b.*\bfreedv-gui\b",
    re.IGNORECASE,
)
RE_FREEDV_COMMENT = re.compile(
    r"//\s*(Source|From|Ported from|Layout from|Pattern from).*\bfreedv-gui\b",
    re.IGNORECASE,
)
# Diff-mode cite-version stamp gate for freedv-gui ports. Mirrors
# RE_THETIS_CITE above. Captures `// From freedv-gui <path>:<line>` or
# `// Source: freedv-gui/<path>:<line>` constructions.
RE_FREEDV_CITE = re.compile(
    r"//\s*(?:From\s+freedv-gui\s+|Source:\s+freedv-gui/)"
    r"[\w./-]+\.(?:c|h|cpp|cc|hpp)(?::\d+(?:[,\s]+\d+)*)"
)


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)


def diffed_files():
    """Return paths of added or modified files in the BASE_REF..HEAD range."""
    mb = run(["git", "merge-base", BASE_REF, "HEAD"])
    if mb.returncode != 0:
        # Fallback: compare directly to BASE_REF (may include unrelated work
        # but conservative — better to over-check than miss).
        base = BASE_REF
    else:
        base = mb.stdout.strip()
    diff = run(
        ["git", "diff", f"{base}..HEAD", "--diff-filter=AM", "--name-only"]
    )
    if diff.returncode != 0:
        print(f"WARN: git diff failed: {diff.stderr}", file=sys.stderr)
        return []
    return [line for line in diff.stdout.splitlines() if line]


def diffed_lines(rel):
    """Return set of 1-based line numbers added/modified in BASE_REF..HEAD for this file.

    Parses unified-diff hunk headers like `@@ -45,0 +46,3 @@` (meaning
    3 lines starting at new line 46 were added). Pure deletions
    (new-count == 0) contribute nothing. Returns an empty set if the
    file has no diff hunks on the NEW side.
    """
    mb = run(["git", "merge-base", BASE_REF, "HEAD"])
    base = mb.stdout.strip() if mb.returncode == 0 else BASE_REF
    diff = run(["git", "diff", "-U0", f"{base}..HEAD", "--", rel])
    if diff.returncode != 0:
        return set()
    lines = set()
    for raw in diff.stdout.splitlines():
        m = re.match(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", raw)
        if not m:
            continue
        start = int(m.group(1))
        count = int(m.group(2)) if m.group(2) is not None else 1
        if count == 0:
            continue
        for i in range(start, start + count):
            lines.add(i)
    return lines


def parse_provenance_paths(*doc_paths):
    """Return union of *first-column* file paths listed in provenance tables.

    Default (no args): just THETIS-PROVENANCE.md (diff-mode contract).

    Full-tree mode passes (PROVENANCE, WDSP_PROVENANCE, AETHER_RECONCILIATION)
    to get the complete "registered somewhere" set. All three docs use the
    same markdown-table convention: the first cell of each data row is the
    registered NereusSDR file path. Counterpart / source / prose cells
    sometimes contain `src/...` strings too (e.g. reconciliation cites
    AetherSDR upstream paths that happen to share a filename with a
    NereusSDR file), so we MUST NOT pull paths from anywhere else in the
    row — doing so allowlists files that aren't actually registered and
    creates a false-negative loophole for future ports.

    Backtick wrapping (``| `src/foo.h` |``) is handled. The `.{h,cpp}`
    shorthand is only recognised in the first cell (it is, in practice,
    never used there today — the shorthand is only used in the counterpart
    column — but we support it for robustness).
    """
    if not doc_paths:
        doc_paths = (PROVENANCE,)
    paths = set()
    for doc in doc_paths:
        if not doc.is_file():
            continue
        for line in doc.read_text().splitlines():
            line = line.strip()
            if not line.startswith("|") or line.startswith("|---"):
                continue
            cells = [c.strip() for c in line.strip("|").split("|")]
            if not cells:
                continue
            first = cells[0].strip("`").strip()
            if not first or first.lower() in ("nereussdr file", "file"):
                continue
            # Expand `.{h,cpp}` shorthand if it somehow appears in column 1.
            m = re.match(r"(src/.+)\.\{h,cpp\}$", first)
            if m:
                paths.add(f"{m.group(1)}.h")
                paths.add(f"{m.group(1)}.cpp")
            else:
                paths.add(first)
    return paths


def all_src_files():
    """Yield repo-relative paths for every C/C++ source file under src/."""
    src = REPO / "src"
    if not src.is_dir():
        return []
    out = []
    for path in src.rglob("*"):
        if path.is_file() and path.suffix in EXTENSIONS:
            out.append(str(path.relative_to(REPO)))
    return sorted(out)


def check_file(rel, listed, diff_lines=None):
    """Return list of (line_num, label, match_text) findings, or [] if OK."""
    path = REPO / rel
    if not path.is_file():
        return []
    if rel in listed:
        return []

    try:
        text = path.read_text(errors="replace")
    except Exception:
        return []

    head = "\n".join(text.splitlines()[:HEADER_WINDOW])
    if OPT_OUT_MARKER in head:
        return []
    if NO_PORT_CHECK_MARKER in head:
        return []

    patterns = [
        (RE_SOURCE_COMMENT, "Source: comment citing Thetis"),
        (RE_THETIS_FILE, "Thetis filename reference"),
        (RE_CALLSIGN, "Thetis contributor callsign"),
        (RE_THETIS_CLASS, "Thetis-style C# class name (cls*/uc*/frm*)"),
    ]
    if FULL_TREE:
        patterns.extend([
            (RE_AETHER_COMMENT, "Source: comment citing AetherSDR"),
            (RE_AETHER_CALLSIGN, "AetherSDR contributor callsign"),
            (RE_AETHER_FILE_NEAR_MARKER, "AetherSDR filename adjacent to 'AetherSDR' marker"),
            (RE_FREEDV_COMMENT, "Source: comment citing freedv-gui"),
            (RE_FREEDV_CALLSIGN, "freedv-gui contributor callsign"),
            (RE_FREEDV_FILE_NEAR_MARKER, "freedv-gui filename adjacent to 'freedv-gui' marker"),
        ])
    findings = []
    for i, line in enumerate(text.splitlines(), start=1):
        for pattern, label in patterns:
            m = pattern.search(line)
            if m:
                findings.append((i, label, m.group(0)))
                break  # one finding per line — keeps output readable

    # Cite-versioning scan runs only in diff mode AND only on lines that
    # the PR actually added/modified. Whole-file scanning would regress
    # ~250 existing unstamped cites in the current tree the moment
    # anyone touches a Thetis-derived file — see HOW-TO-PORT.md
    # §Inline cite versioning "cost proportional to churn" promise.
    if not FULL_TREE and diff_lines is not None:
        for i, line in enumerate(text.splitlines(), start=1):
            if i not in diff_lines:
                continue
            m = RE_THETIS_CITE.search(line)
            if m and not RE_HAS_VERSION_STAMP.search(line):
                findings.append((
                    i,
                    "Thetis cite missing version stamp",
                    m.group(0).strip(),
                ))
                continue
            m = RE_FREEDV_CITE.search(line)
            if m and not RE_HAS_VERSION_STAMP.search(line):
                findings.append((
                    i,
                    "freedv-gui cite missing version stamp",
                    m.group(0).strip(),
                ))

    return findings


def main():
    if FULL_TREE:
        files = all_src_files()
        listed = parse_provenance_paths(
            PROVENANCE, WDSP_PROVENANCE, AETHER_RECONCILIATION,
            FREEDV_PROVENANCE,
        )
        mode_label = "full-tree"
    else:
        files = diffed_files()
        listed = parse_provenance_paths()
        mode_label = "diff"
    if not files:
        print("No added/modified files in diff range — nothing to check.")
        return 0

    failures = 0
    checked = 0

    for rel in files:
        ext = Path(rel).suffix
        if ext not in EXTENSIONS:
            continue
        checked += 1
        dlines = diffed_lines(rel) if not FULL_TREE else None
        findings = check_file(rel, listed, diff_lines=dlines)
        if not findings:
            continue
        failures += 1
        print(f"FAIL {rel}:")
        for line_num, label, match in findings[:5]:
            print(f"  L{line_num} [{label}]: {match}")
        if len(findings) > 5:
            print(f"  ... and {len(findings) - 5} more matches")
        has_cite_issue = any(
            "cite missing version stamp" in label
            for _i, label, _m in findings
        )
        if has_cite_issue:
            print(
                f"  Cite cure: append `[vX.Y.Z.W]` (e.g. [v2.10.3.13])"
                f" or `[@shortsha]` (e.g. [@abc1234]) to the cite line"
                f" — see docs/attribution/HOW-TO-PORT.md §Inline cite"
                f" versioning. Run `git -C ../Thetis describe --tags`"
                f" or `git -C ../Thetis rev-parse --short HEAD` to get"
                f" the stamp."
            )
        print(
            f"  Attribution cure: add a PROVENANCE row + verbatim"
            f" header per docs/attribution/HOW-TO-PORT.md, OR add a"
            f" `// {OPT_OUT_MARKER} <X>.h interface` comment if"
            f" genuinely independent, OR add `// {NO_PORT_CHECK_MARKER}"
            f" <reason>` in the file head to suppress this check for a"
            f" legitimate false positive."
        )
        print()

    if failures == 0:
        print(
            f"OK [{mode_label}]: {checked} C/C++ file(s) checked, "
            f"all properly attributed or skip-marked."
        )
        return 0
    print(
        f"[{mode_label}] {failures} of {checked} file(s) "
        f"flagged for missing attribution."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
