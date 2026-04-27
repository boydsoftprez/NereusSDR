#!/usr/bin/env python3
"""Discover every inline contributor/author tag present in upstream
deskhpsdr source (dl1bz/deskhpsdr, fork of piHPSDR by DL1YCF / G0ORX).

Feeds `verify-inline-tag-preservation.py` — that sibling script must
not use a hardcoded contributor list, or we silently drop tags from
unknown authors. This script builds the corpus mechanically from
upstream source, writes
`docs/attribution/deskhpsdr-author-tags.json`, and exits 0.

deskhpsdr is a C project (no C# like Thetis). Attribution patterns
observed in the source tree:

  - File headers: `Copyright (C) YYYY - Name, CALLSIGN` blocks
  - Inline attribution: `// Contribution of <phrase> from N1GP (Rick Koch)`
  - Inline notes: `// Contributed initially by Davide "ra1nb0w"`
  - Named + parenthesized callsign: `N1GP (Rick Koch)`

Unlike Thetis, deskhpsdr does NOT use inline developer-tag comments in the
`//DH1KLM` or `//MW0LGE` style within function bodies. Attribution is
carried in file headers and occasional named contribution comments. The
regex set is therefore tuned for C-project norms:
  - Callsign in parentheses: `(N1GP)`, `(G0ORX)`, `(DL1YCF)`, `(DL1BZ)`
  - Named credit lines: `Contribution of ... from NAME (CALLSIGN)`,
    `Contributed initially by NAME`
  - Copyright header callsigns: extracted from `YYYY - Name, CALLSIGN`

Output JSON schema (mirrors thetis-author-tags.json):
    {
      "generated_utc": "...",
      "deskhpsdr_sha": "...",
      "callsign_tags": {
        "G0ORX": {"count": 4, "first_seen": "new_protocol.c:2", ...},
        ...
      },
      "named_tags": {
        "John Melton": {"count": 4, "first_seen": "new_protocol.c:2", ...},
        ...
      }
    }

Usage:
    python3 scripts/discover-deskhpsdr-author-tags.py              # write
    python3 scripts/discover-deskhpsdr-author-tags.py --dry-run    # stdout
    python3 scripts/discover-deskhpsdr-author-tags.py --drift      # exit 1
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
DESKHPSDR_DIR = Path(os.environ.get(
    "NEREUS_DESKHPSDR_DIR",
    # Standard sibling relative to each worktree depth:
    #   worktree at .worktrees/<branch>/ → ../../../deskhpsdr
    #   main checkout                    → ../deskhpsdr
    # We probe both; the first existing one wins.
    "/Users/j.j.boyd/deskhpsdr")).expanduser()

CORPUS_PATH = REPO / "docs" / "attribution" / "deskhpsdr-author-tags.json"

# deskhpsdr is pure C — scan .c and .h only
SCAN_SUFFIXES = {".c", ".h"}

# Callsign regex core: letter-digit-letter form common to amateur radio
# (1-2 prefix letters + digit + 1-4 suffix letters), 4-7 chars total.
RE_CALLSIGN_CORE = r"(?:[A-Z][A-Z0-9]?[0-9][A-Z]{1,4})"

# Patterns for callsign discovery.  Ordered most-specific-first.
# deskhpsdr uses C-style `/* ... */` block comments for headers AND
# `//` inline comments. We match both.

# Callsign in parentheses (most common deskhpsdr pattern):
#   "N1GP (Rick Koch)", "G0ORX/N6LYT", "John Melton, G0ORX/N6LYT"
RE_PAREN_CALLSIGN = re.compile(r"\((" + RE_CALLSIGN_CORE + r")\)")
# Slash-separated callsigns: G0ORX/N6LYT
RE_SLASH_CALLSIGN = re.compile(
    r"\b(" + RE_CALLSIGN_CORE + r")/(" + RE_CALLSIGN_CORE + r")\b")
# Bare callsign after comma in copyright lines:
#   "2015 - John Melton, G0ORX/N6LYT"
RE_COMMA_CALLSIGN = re.compile(r",\s*(" + RE_CALLSIGN_CORE + r")\b")
# Bare callsign after dash in copyright lines:
#   "2024,2025 - Heiko Amft, DL1BZ"
RE_COPYRIGHT_CALLSIGN = re.compile(
    r"\d{4}(?:,\d{4})*\s*-\s*[A-Za-z\s]+,\s*(" + RE_CALLSIGN_CORE + r")\b")

# Named credits in inline comments or attribution blocks:
#   "Contribution of interfacing to PiHPSDR from N1GP (Rick Koch)"
#   "Contributed initially by Davide"
RE_NAMED_CREDIT = re.compile(
    r"(?:Contribution\s+of\s+\S+(?:\s+\S+)*\s+from\s+|"
    r"Contributed\s+(?:initially\s+)?by\s+|"
    r"Credit[:,]?\s+(?:to\s+)?|"
    r"By\s+|"
    r"Author[:,]?\s+)"
    r"([A-Z][a-z]+(?:\s+[A-Z\"][a-z]+\"|[A-Z][a-z]+){0,3})"
)
# Copyright line named author:
#   "2015 - John Melton, G0ORX/N6LYT"
#   "2024,2025 - Heiko Amft, DL1BZ (Project deskHPSDR)"
RE_COPYRIGHT_NAME = re.compile(
    r"\d{4}(?:,\d{4})*\s*-\s*([A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,3})\b"
)

# Noise: patterns that look like callsigns but aren't
NOISE_PATTERNS = re.compile(
    r"^(?:TODO|FIXME|NOTE|XXX|HACK|IDEA|BUG|REF|SEE|"
    r"[0-9A-F]+|"          # pure hex
    r"V\d+(?:\.\d+)*|"     # version strings
    r"[A-Z]{1,3}[0-9]+)$"  # short letter+digit codes
)
FALSE_POSITIVE_TOKENS = {
    "ABORT", "ASSERT",
    "RX2ATT", "RX2EN", "RX1ATT", "RX1EN",
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
    """Walk src/ (and only src/) inside deskhpsdr — skip vendored
    wdsp-1.29/ and libsolar/ sub-trees."""
    src_root = base / "src"
    if not src_root.is_dir():
        # Fallback: if no src/ subdirectory, walk entire base
        for p in base.rglob("*"):
            if p.is_file() and p.suffix in SCAN_SUFFIXES:
                yield p
        return
    for p in src_root.rglob("*"):
        if p.is_file() and p.suffix in SCAN_SUFFIXES:
            yield p


def classify_callsign(raw: str) -> str | None:
    """Return cleaned callsign tag if accepted, else None."""
    tag = raw.strip().upper()
    if not tag:
        return None
    if NOISE_PATTERNS.match(tag):
        return None
    if tag in FALSE_POSITIVE_TOKENS:
        return None
    if len(tag) < 4 or len(tag) > 7:
        return None
    return tag


def discover(base: Path):
    callsigns: dict[str, dict] = {}
    named: dict[str, dict] = {}

    def record_callsign(tag: str, src_file: Path, line_num: int):
        cleaned = classify_callsign(tag)
        if cleaned is None:
            return
        entry = callsigns.setdefault(cleaned, {
            "count": 0, "first_seen": None, "files": set()})
        entry["count"] += 1
        entry["files"].add(src_file.name)
        if entry["first_seen"] is None:
            entry["first_seen"] = f"{src_file.name}:{line_num}"

    def record_named(name: str, src_file: Path, line_num: int):
        name = name.strip()
        # Strip trailing quote artifacts from "ra1nb0w"-style nicknames
        name = name.rstrip('"').strip()
        if not name or len(name) < 4:
            return
        # Skip all-uppercase (callsign, not name)
        if name == name.upper():
            return
        entry = named.setdefault(name, {"count": 0, "first_seen": None})
        entry["count"] += 1
        if entry["first_seen"] is None:
            entry["first_seen"] = f"{src_file.name}:{line_num}"

    if not base.is_dir():
        print(f"[discover] WARN: deskhpsdr not found at {base}",
              file=sys.stderr)
        return callsigns, named

    for src in iter_upstream_files(base):
        try:
            text = src.read_text(encoding="utf-8",
                                 errors="replace").splitlines()
        except Exception:
            continue
        for ln_idx, line in enumerate(line for line in text):
            # Parenthesized callsign (most common deskhpsdr pattern)
            for m in RE_PAREN_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
            # Slash-separated callsigns: G0ORX/N6LYT
            for m in RE_SLASH_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
                record_callsign(m.group(2), src, ln_idx + 1)
            # Copyright-line comma callsign
            for m in RE_COPYRIGHT_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
            # Copyright-line named author
            for m in RE_COPYRIGHT_NAME.finditer(line):
                record_named(m.group(1), src, ln_idx + 1)
            # Named credits (inline comments)
            for m in RE_NAMED_CREDIT.finditer(line):
                record_named(m.group(1), src, ln_idx + 1)

    # Convert sets to sorted lists for JSON serialization
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

    deskhpsdr_dir = DESKHPSDR_DIR
    if not deskhpsdr_dir.is_dir():
        # Probe alternative relative paths for worktree layouts
        alternatives = [
            REPO.parent / "deskhpsdr",
            REPO.parent.parent / "deskhpsdr",
            REPO.parent.parent.parent / "deskhpsdr",
        ]
        found = next((p for p in alternatives if p.is_dir()), None)
        if found is None:
            print(f"FATAL: deskhpsdr not found at {deskhpsdr_dir}",
                  file=sys.stderr)
            print("Set NEREUS_DESKHPSDR_DIR or ensure deskhpsdr is cloned",
                  file=sys.stderr)
            return 2
        deskhpsdr_dir = found

    callsigns, named = discover(deskhpsdr_dir)

    # Merge with existing corpus so human-edited fields (name, role)
    # are preserved across re-runs.  Only mechanical fields (count,
    # first_seen, files) get refreshed from the upstream walk.
    existing = {}
    if CORPUS_PATH.is_file():
        try:
            existing = json.loads(CORPUS_PATH.read_text())
        except Exception:
            existing = {}
    existing_cs = existing.get("callsign_tags", {})
    existing_named = existing.get("named_tags", {})

    for cs, meta in callsigns.items():
        prev = existing_cs.get(cs, {})
        meta["name"] = prev.get("name", None)
        meta["role"] = prev.get("role", None)

    for nm, meta in named.items():
        prev = existing_named.get(nm, {})
        meta.setdefault("role", prev.get("role", None))

    corpus = {
        "generated_utc": datetime.now(timezone.utc).isoformat(
            timespec="seconds"),
        "deskhpsdr_sha": git_sha(deskhpsdr_dir),
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
            print("Run scripts/discover-deskhpsdr-author-tags.py and commit "
                  "the refreshed corpus (populate name/role fields).")
            return 1
        # Also fail if any committed callsign still has name=null
        unnamed = sorted(cs for cs, meta in committed["callsign_tags"].items()
                         if meta.get("name") in (None, ""))
        if unnamed:
            print(f"DRIFT: {len(unnamed)} callsign(s) in corpus have "
                  f"name=null:")
            for cs in unnamed:
                meta = committed["callsign_tags"][cs]
                print(f"  {cs}: {meta['first_seen']} "
                      f"(role: {meta.get('role') or '(null)'})")
            print("Populate the name field in "
                  "docs/attribution/deskhpsdr-author-tags.json and commit.")
            return 1
        print("[drift] no new author tags; all callsigns have names; "
              "corpus is current")
        return 0

    CORPUS_PATH.parent.mkdir(parents=True, exist_ok=True)
    CORPUS_PATH.write_text(text + "\n")
    print(f"[discover] wrote {CORPUS_PATH.relative_to(REPO)}")
    print(f"  {len(corpus['callsign_tags'])} callsign tags")
    print(f"  {len(corpus['named_tags'])} named contributors")
    return 0


if __name__ == "__main__":
    sys.exit(main())
