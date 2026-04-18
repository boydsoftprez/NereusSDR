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

Exit 0 on clean, 1 on any flagged file.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
BASE_REF = os.environ.get("CHECK_NEW_PORTS_BASE_REF", "origin/main")

# C/C++ source extensions only — scripts/, docs/, tests/ Python, YAML,
# Markdown all skipped automatically.
EXTENSIONS = {".cpp", ".h", ".c", ".cc", ".hpp", ".hxx"}

# Header window used to detect the per-file skip markers.
HEADER_WINDOW = 160

OPT_OUT_MARKER = "Independently implemented from"
NO_PORT_CHECK_MARKER = "no-port-check:"

CALLSIGNS = [
    # Thetis (and Thetis-fork) contributors only. KK7GWY is the AetherSDR
    # primary author and is NOT included here — AetherSDR provenance is
    # tracked separately in `aethersdr-reconciliation.md`, not
    # `THETIS-PROVENANCE.md`.
    "MW0LGE", "W2PA", "W5WC", "VK6APH", "MI0BOT", "G8NJJ", "NR0V",
    "G0ORX", "KD5TFD", "DH1KLM", "W4WMT", "N1GP", "KE9NS", "K5SO",
    "OE3IDE", "KD0OSS", "AA6E",
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


def parse_provenance_paths():
    """Return set of file paths listed in THETIS-PROVENANCE.md tables."""
    paths = set()
    if not PROVENANCE.is_file():
        return paths
    for line in PROVENANCE.read_text().splitlines():
        line = line.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells:
            continue
        first = cells[0].replace("`", "")
        if first and first.lower() not in ("nereussdr file", "file"):
            paths.add(first)
    return paths


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

    findings = []
    for i, line in enumerate(text.splitlines(), start=1):
        for pattern, label in [
            (RE_SOURCE_COMMENT, "Source: comment citing Thetis"),
            (RE_THETIS_FILE, "Thetis filename reference"),
            (RE_CALLSIGN, "Thetis contributor callsign"),
            (RE_THETIS_CLASS, "Thetis-style C# class name (cls*/uc*/frm*)"),
        ]:
            m = pattern.search(line)
            if m:
                findings.append((i, label, m.group(0)))
                break  # one finding per line — keeps output readable
    return findings


def main():
    files = diffed_files()
    if not files:
        print("No added/modified files in diff range — nothing to check.")
        return 0

    listed = parse_provenance_paths()
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
            f"OK: {checked} added/modified C/C++ file(s) checked, "
            f"all properly attributed or skip-marked."
        )
        return 0
    print(f"{failures} of {checked} file(s) flagged for missing attribution.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
