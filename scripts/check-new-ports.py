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


RE_PATH_IN_CELL = re.compile(r"`?(src/[^ `|]+\.(?:cpp|h|hpp|c|cc|hxx))`?")


def parse_provenance_paths(*doc_paths):
    """Return union of file paths listed in the given provenance / reconciliation docs.

    Default (no args): just THETIS-PROVENANCE.md table (diff-mode behaviour
    preserved — the heuristic owner is still Thetis).

    Full-tree mode calls with (PROVENANCE, WDSP_PROVENANCE,
    AETHER_RECONCILIATION) to get the complete "registered somewhere" set.
    Reconciliation doc cells sometimes wrap paths in backticks and list
    the `.{h,cpp}` shorthand, so we extract paths with a regex instead of
    relying on the first-cell convention.
    """
    if not doc_paths:
        doc_paths = (PROVENANCE,)
    paths = set()
    for doc in doc_paths:
        if not doc.is_file():
            continue
        text = doc.read_text()
        if doc == PROVENANCE:
            # Strict first-cell parse preserves original diff-mode contract.
            for line in text.splitlines():
                line = line.strip()
                if not line.startswith("|") or line.startswith("|---"):
                    continue
                cells = [c.strip() for c in line.strip("|").split("|")]
                if not cells:
                    continue
                first = cells[0].replace("`", "")
                if first and first.lower() not in ("nereussdr file", "file"):
                    paths.add(first)
        else:
            # Reconciliation doc uses backtick-wrapped paths anywhere in a
            # row; some rows enumerate .{h,cpp} shorthand which we expand.
            for m in RE_PATH_IN_CELL.finditer(text):
                paths.add(m.group(1))
            for m in re.finditer(r"`(src/[^`]+)\.\{h,cpp\}`", text):
                base = m.group(1)
                paths.add(f"{base}.h")
                paths.add(f"{base}.cpp")
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


def check_file(rel, listed):
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
        ])
    findings = []
    for i, line in enumerate(text.splitlines(), start=1):
        for pattern, label in patterns:
            m = pattern.search(line)
            if m:
                findings.append((i, label, m.group(0)))
                break  # one finding per line — keeps output readable
    return findings


def main():
    if FULL_TREE:
        files = all_src_files()
        listed = parse_provenance_paths(
            PROVENANCE, WDSP_PROVENANCE, AETHER_RECONCILIATION
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
        findings = check_file(rel, listed)
        if not findings:
            continue
        failures += 1
        print(f"FAIL {rel}:")
        for line_num, label, match in findings[:5]:
            print(f"  L{line_num} [{label}]: {match}")
        if len(findings) > 5:
            print(f"  ... and {len(findings) - 5} more matches")
        print(
            f"  Cure: add a PROVENANCE row + verbatim header per "
            f"docs/attribution/HOW-TO-PORT.md, OR add a "
            f"`// {OPT_OUT_MARKER} <X>.h interface` comment if "
            f"genuinely independent, OR add `// {NO_PORT_CHECK_MARKER} "
            f"<reason>` in the file head to suppress this check for a "
            f"legitimate false positive."
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
