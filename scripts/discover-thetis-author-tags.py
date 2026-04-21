#!/usr/bin/env python3
"""Discover every inline contributor/author tag present in upstream
Thetis source (ramdor + mi0bot fork).

Feeds `verify-inline-tag-preservation.py` — that sibling script must
not use a hardcoded contributor list, or we silently drop tags from
unknown authors. This script builds the corpus mechanically from
upstream source, writes
`docs/attribution/thetis-author-tags.json`, and exits 0.

Tag shapes detected (all require `//` prefix so string literals are
excluded):
    //DH1KLM                          (bare callsign)
    // MW0LGE                         (bare callsign with leading space)
    //MW0LGE_21k5 change to rx2       (callsign + underscored variant)
    //-W2PA                           (dash prefix)
    // -W2PA                          (dash prefix w/ space)
    //[2.10.3.13]MW0LGE               (version-prefixed)
    //MW0LGE [2.9.0.7]                (callsign + version suffix)
    // Credit: Richard Samphire       (named credit)
    // By Doug Wigley                 (named credit)
    // Reid Campbell (MI0BOT)         (named + parenthesized callsign)

Output JSON schema:
    {
      "generated_utc": "...",
      "thetis_sha": "...",
      "mi0bot_sha": "...",
      "callsign_tags": {
        "DH1KLM": {"count": 47, "first_seen": "console.cs:6739"},
        ...
      },
      "named_tags": {
        "Richard Samphire": {"count": 12, "first_seen": "..."},
        ...
      },
      "noise": [...]  // manually curated skip list
    }

Usage:
    python3 scripts/discover-thetis-author-tags.py              # write
    python3 scripts/discover-thetis-author-tags.py --dry-run    # stdout
    python3 scripts/discover-thetis-author-tags.py --drift      # exit 1
                                                                # if new
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
THETIS_DIR = Path(os.environ.get(
    "NEREUS_THETIS_DIR", REPO.parent.parent.parent / "Thetis")).expanduser()
MI0BOT_DIR = Path(os.environ.get(
    "NEREUS_MI0BOT_DIR", REPO.parent.parent.parent / "mi0bot-Thetis")).expanduser()

CORPUS_PATH = REPO / "docs" / "attribution" / "thetis-author-tags.json"
SCAN_SUFFIXES = {".cs", ".c", ".h", ".cpp"}

# Patterns that surface author tags. Ordered most-specific-first.
# Callsign pattern: letter-digit-letter form common to amateur radio
# (1-2 prefix letters + digit + 1-4 suffix letters), 4-7 chars total.
RE_CALLSIGN_CORE = r"(?:[A-Z][A-Z0-9]?[0-9][A-Z]{1,4})"

# Discovery regexes (each tag name must land in group 1)
RE_BARE_CALLSIGN   = re.compile(r"//\s*(" + RE_CALLSIGN_CORE + r")(?:_[\w]+)?\b")
RE_DASH_CALLSIGN   = re.compile(r"//\s*-\s*(" + RE_CALLSIGN_CORE + r")\b")
RE_VERSION_TAG     = re.compile(r"//\s*\[\d+\.\d+(?:\.\d+)+\](" + RE_CALLSIGN_CORE + r")\b")
RE_CALLSIGN_VER    = re.compile(r"//\s*(" + RE_CALLSIGN_CORE + r")\s*\[\d+\.\d+")
RE_PAREN_CALLSIGN  = re.compile(r"\((" + RE_CALLSIGN_CORE + r")\)")
RE_NAMED_CREDIT    = re.compile(
    r"//\s*(?:Credit[:,]?\s*to\s+|By\s+|Author[:,]?\s+|Contributions\s+by\s+)"
    r"([A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,3})"
)

# Noise: patterns that LOOK like callsigns but aren't. Curated; also
# applies to tokens composed entirely of hex digits.
NOISE_PATTERNS = re.compile(
    r"^(?:TODO|FIXME|NOTE|XXX|HACK|IDEA|BUG|REF|SEE|N[_]?[0-9]+|"
    r"V\d+(?:\.\d+)*|L\d+|"
    r"[0-9A-F]+|"          # pure hex (addresses, opcodes)
    r"[A-Z]{1,3}[0-9]+)$"  # short letter+digit codes (X87, MK5, etc.)
)
# Known false-positive callsign-shaped tokens in Thetis macro/enum land
FALSE_POSITIVE_TOKENS = {
    "X1Y1", "X2Y2",          # rect corners
    "ABORT", "ASSERT",
    "RX2ATT", "RX2EN",       # CAT command identifiers, not callsigns
    "RX1ATT", "RX1EN",
    "TX1EN", "TX2EN",
    "RX2", "RX1", "TX1", "TX2",
}


def git_sha(path: Path) -> str | None:
    if not (path / ".git").exists():
        return None
    try:
        return subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "--short", "HEAD"],
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        return None


def iter_upstream_files(base: Path):
    if not base.is_dir():
        return
    for p in base.rglob("*"):
        if p.is_file() and p.suffix in SCAN_SUFFIXES:
            yield p


def classify_match(raw: str) -> str | None:
    """Return cleaned tag if accepted, else None."""
    tag = raw.strip()
    if not tag:
        return None
    if NOISE_PATTERNS.match(tag):
        return None
    if tag in FALSE_POSITIVE_TOKENS:
        return None
    # callsigns are uppercase by convention
    if tag != tag.upper():
        return None
    # reject single-letter or over-long
    if len(tag) < 4 or len(tag) > 7:
        return None
    return tag


def discover():
    callsigns: dict[str, dict] = {}
    named: dict[str, dict] = {}

    def record_callsign(tag: str, src_file: Path, line_num: int):
        cleaned = classify_match(tag)
        if cleaned is None:
            return
        entry = callsigns.setdefault(cleaned, {"count": 0, "first_seen": None,
                                               "files": set()})
        entry["count"] += 1
        entry["files"].add(src_file.name)
        if entry["first_seen"] is None:
            entry["first_seen"] = f"{src_file.name}:{line_num}"

    def record_named(name: str, src_file: Path, line_num: int):
        name = name.strip()
        if not name or len(name) < 5:
            return
        entry = named.setdefault(name, {"count": 0, "first_seen": None})
        entry["count"] += 1
        if entry["first_seen"] is None:
            entry["first_seen"] = f"{src_file.name}:{line_num}"

    for base in (THETIS_DIR, MI0BOT_DIR):
        if not base.is_dir():
            print(f"[discover] WARN: upstream {base} not found",
                  file=sys.stderr)
            continue
        for src in iter_upstream_files(base):
            try:
                text = src.read_text(encoding="utf-8",
                                     errors="replace").splitlines()
            except Exception:
                continue
            for ln_idx, line in enumerate(text):
                if "//" not in line:
                    continue
                tail = line.split("//", 1)[1]
                if not tail:
                    continue
                commented = "//" + tail
                for rx in (RE_BARE_CALLSIGN, RE_DASH_CALLSIGN,
                           RE_VERSION_TAG, RE_CALLSIGN_VER):
                    for m in rx.finditer(commented):
                        record_callsign(m.group(1), src, ln_idx + 1)
                for m in RE_PAREN_CALLSIGN.finditer(commented):
                    record_callsign(m.group(1), src, ln_idx + 1)
                for m in RE_NAMED_CREDIT.finditer(commented):
                    record_named(m.group(1), src, ln_idx + 1)

    # convert sets to sorted lists for JSON
    for entry in callsigns.values():
        entry["files"] = sorted(entry["files"])

    return callsigns, named


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true",
                    help="Print corpus to stdout; don't write file")
    ap.add_argument("--drift", action="store_true",
                    help="Exit 1 if discovered corpus differs from "
                         "committed corpus (for CI)")
    args = ap.parse_args()

    if not THETIS_DIR.is_dir():
        print(f"FATAL: ../Thetis not found at {THETIS_DIR}", file=sys.stderr)
        return 2

    callsigns, named = discover()

    corpus = {
        "generated_utc": datetime.now(timezone.utc).isoformat(
            timespec="seconds"),
        "thetis_sha": git_sha(THETIS_DIR),
        "mi0bot_sha": git_sha(MI0BOT_DIR),
        "callsign_tags": {
            k: v for k, v in sorted(callsigns.items(),
                                    key=lambda x: (-x[1]["count"], x[0]))
        },
        "named_tags": {
            k: v for k, v in sorted(named.items(),
                                    key=lambda x: (-x[1]["count"], x[0]))
        },
    }

    text = json.dumps(corpus, indent=2, sort_keys=False)

    if args.dry_run:
        print(text)
        return 0

    if args.drift:
        if not CORPUS_PATH.is_file():
            print("FATAL: no committed corpus to drift-check against",
                  file=sys.stderr)
            return 1
        committed = json.loads(CORPUS_PATH.read_text())
        committed_keys = set(committed.get("callsign_tags", {}))
        discovered_keys = set(corpus["callsign_tags"])
        new_tags = discovered_keys - committed_keys
        if new_tags:
            print(f"DRIFT: {len(new_tags)} new author tag(s) in upstream "
                  f"that are NOT in committed corpus:")
            for t in sorted(new_tags):
                print(f"  {t}: {corpus['callsign_tags'][t]['first_seen']}")
            print("Run scripts/discover-thetis-author-tags.py and commit "
                  "the refreshed corpus.")
            return 1
        print("[drift] no new author tags; corpus is current")
        return 0

    CORPUS_PATH.parent.mkdir(parents=True, exist_ok=True)
    CORPUS_PATH.write_text(text + "\n")
    print(f"[discover] wrote {CORPUS_PATH.relative_to(REPO)}")
    print(f"  {len(corpus['callsign_tags'])} callsign tags")
    print(f"  {len(corpus['named_tags'])} named contributors")
    return 0


if __name__ == "__main__":
    sys.exit(main())
