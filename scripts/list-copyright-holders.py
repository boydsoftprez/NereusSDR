#!/usr/bin/env python3
"""Dump distinct copyright-holder names referenced in the attribution
docs, so the About-dialog §5(d) line can be diffed against the tree's
current lineage.

Compliance Plan Task 14. Sources:

- ``docs/attribution/THETIS-PROVENANCE.md`` (ramdor/Thetis +
  mi0bot/Thetis-HL2 rows)
- ``docs/attribution/aethersdr-reconciliation.md`` (Bucket A rows)
- ``docs/attribution/WDSP-PROVENANCE.md`` (copyright-statement dump)
- ``third_party/wdsp/src/*.c`` header lines (cross-check)

Output: sorted list of "Name (CALLSIGN)" tuples where we can extract
both, otherwise bare names / callsigns. Use the output to diff against
``src/gui/AboutDialog.cpp`` principals list when refreshing at release
time.

Usage:
    python3 scripts/list-copyright-holders.py
    python3 scripts/list-copyright-holders.py --format=json
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Callsign regex — approximate, covers ham-radio amateur prefixes:
# 1-2 letter country + optional digit(s) + 1-4 letter suffix.
CALLSIGN_RE = re.compile(r"\b([A-Z]{1,2}[0-9][A-Z]{2,4})\b")

# Structured "Name (CALLSIGN)" pattern.
NAME_CALL_RE = re.compile(
    r"([A-Z][a-zA-Z.\-']+(?:\s+[A-Z][a-zA-Z.\-']+){0,3})"
    r"\s*[,(]\s*"
    r"([A-Z]{1,2}[0-9][A-Z]{2,4}|KG4VCF)"
    r"\)?"
)

DOCS = [
    REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md",
    REPO / "docs" / "attribution" / "aethersdr-reconciliation.md",
    REPO / "docs" / "attribution" / "WDSP-PROVENANCE.md",
    REPO / "docs" / "attribution" / "thetis-contributor-index.md",
]

# Strings that look like callsigns but aren't.
FALSE_POSITIVES = {
    "PWM", "WWV", "WPM", "WSA", "WSJT", "FT8", "CW",
    "PSK", "SWR", "ALC", "LSB", "USB", "CTU", "MOX",
    "TCI", "CAT", "RBN", "ADC", "DAC", "FFT", "DSP",
    "GPL", "IQC", "NSA", "UDP", "GPU", "UDP", "BSD",
    "MIT", "FSF", "TAPR", "PTT", "SDR", "VAC", "RF",
    "VCO", "VHF", "UHF", "HF", "CFG", "EQ", "NR",
    "NB", "AGC", "ANF", "APF", "BPF", "LPF", "HPF",
    "RXA", "TXA", "EMNR", "SNB", "VFO", "DDC", "DUC",
}


def harvest_name_call(text: str):
    hits = set()
    for m in NAME_CALL_RE.finditer(text):
        name = m.group(1).strip().rstrip(".")
        call = m.group(2).strip()
        if not call or call in FALSE_POSITIVES:
            continue
        # Skip if the "name" is actually another callsign that got
        # title-cased (e.g. "Mw0lge, W2PA" matches).
        if CALLSIGN_RE.fullmatch(name.upper()):
            continue
        hits.add((name, call))
    return hits


def harvest_bare_calls(text: str):
    hits = set()
    for m in CALLSIGN_RE.finditer(text):
        call = m.group(1)
        if call in FALSE_POSITIVES:
            continue
        hits.add(call)
    return hits


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--format", choices=["text", "json"], default="text")
    args = ap.parse_args()

    paired = set()
    bare = set()
    for doc in DOCS:
        if not doc.is_file():
            continue
        text = doc.read_text(errors="replace")
        paired.update(harvest_name_call(text))
        bare.update(harvest_bare_calls(text))

    # Also walk WDSP headers for any callsigns in the Copyright lines.
    wdsp_src = REPO / "third_party" / "wdsp" / "src"
    if wdsp_src.is_dir():
        for p in wdsp_src.glob("*.c"):
            head = p.read_text(errors="replace")[:1500]
            # "Warren Pratt, NR0V" style
            paired.update(harvest_name_call(head))

    paired_calls = {c for _, c in paired}
    orphan_calls = {c for c in bare if c not in paired_calls}

    if args.format == "json":
        print(json.dumps({
            "paired": sorted([{"name": n, "callsign": c} for n, c in paired],
                             key=lambda r: r["callsign"]),
            "orphan_callsigns": sorted(orphan_calls),
        }, indent=2))
        return 0

    print("Name + callsign pairs (alphabetical by callsign):\n")
    for name, call in sorted(paired, key=lambda r: r[1]):
        print(f"  {call}   {name}")

    if orphan_calls:
        print("\nBare callsigns (no associated name found in docs):\n")
        for c in sorted(orphan_calls):
            print(f"  {c}")

    print(f"\n{len(paired)} pair(s), {len(orphan_calls)} orphan callsign(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
