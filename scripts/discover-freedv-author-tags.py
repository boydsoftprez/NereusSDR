#!/usr/bin/env python3
"""Discover every inline contributor/author tag present in upstream
freedv-gui source (drowe67/freedv-gui).

Feeds `verify-inline-tag-preservation.py` - that sibling script must
not use a hardcoded contributor list, or we silently drop tags from
unknown authors. This script builds the corpus mechanically from
upstream source, writes
`docs/attribution/freedv-gui-author-tags.json`, and exits 0.

freedv-gui is a C++ project (with some C in pipeline + bundled C in
3rdparty). Attribution patterns observed in the source tree:

  - File headers: `// Authors:         <Name>` line in a per-file
    block-comment header (Mooneer Salem, David Rowe, David Witten,
    Jim Ahlstrom, ...). Format mirrors wxWidgets-style heading.
  - Parenthesized callsigns inline: `(VK5DGR)`, `(K6AQ)`, `(N2ADR)`.
  - Slash-separated callsigns: rare in freedv-gui but possible
    (e.g. `G0ORX/N6LYT`). Mirrors deskhpsdr discovery.
  - Bare `// CALLSIGN:` inline tags in function bodies are uncommon
    in freedv-gui but the pattern is supported for parity with the
    Thetis / deskhpsdr discoverers.
  - Prose attribution: `by <CALLSIGN>`, `from <CALLSIGN>`.

Output JSON schema (mirrors thetis-author-tags.json /
deskhpsdr-author-tags.json):
    {
      "generated_utc": "...",
      "freedv_gui_sha": "...",
      "callsign_tags": {
        "VK5DGR": {"count": 4, "first_seen": "main.cpp:6", ...},
        ...
      },
      "named_tags": {
        "Mooneer Salem": {"count": 47, "first_seen": "...", ...},
        ...
      }
    }

Usage:
    python3 scripts/discover-freedv-author-tags.py              # write
    python3 scripts/discover-freedv-author-tags.py --dry-run    # stdout
    python3 scripts/discover-freedv-author-tags.py --drift      # exit 1
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
FREEDV_DIR = Path(os.environ.get(
    "NEREUS_FREEDV_DIR",
    # Standard sibling relative to each worktree depth:
    #   worktree at .claude/worktrees/<branch>/ -> ../../../../freedv-gui
    #   main checkout                            -> ../freedv-gui
    # We probe both; the first existing one wins (see main()).
    "/Users/j.j.boyd/freedv-gui")).expanduser()

CORPUS_PATH = REPO / "docs" / "attribution" / "freedv-gui-author-tags.json"

# freedv-gui is C++ with some C in pipeline; scan the standard set.
SCAN_SUFFIXES = {".c", ".h", ".cpp", ".hpp", ".cc"}

# Callsign regex core: letter-digit-letter form common to amateur radio
# (1-2 prefix letters + digit + 1-4 suffix letters), 4-7 chars total.
RE_CALLSIGN_CORE = r"(?:[A-Z][A-Z0-9]?[0-9][A-Z]{1,4})"

# Patterns for callsign discovery. Ordered most-specific-first.

# Callsign in parentheses (most common freedv-gui inline pattern):
#   "(VK5DGR)", "(K6AQ)", "(N2ADR)"
RE_PAREN_CALLSIGN = re.compile(r"\((" + RE_CALLSIGN_CORE + r")\)")
# Slash-separated callsigns: G0ORX/N6LYT
RE_SLASH_CALLSIGN = re.compile(
    r"\b(" + RE_CALLSIGN_CORE + r")/(" + RE_CALLSIGN_CORE + r")\b")
# Bare callsign after dash in copyright lines:
#   "Copyright (c) 2026 - Mooneer Salem, K6AQ"
RE_COPYRIGHT_CALLSIGN = re.compile(
    r"\d{4}(?:,\d{4})*\s*-\s*[A-Za-z\s]+,\s*(" + RE_CALLSIGN_CORE + r")\b")

# Bare `// CALLSIGN:` inline tag in function bodies (byte-mirrored from
# scripts/discover-deskhpsdr-author-tags.py):
#   "// VK5DGR: tweaked the FFT window"
# Applied AFTER the more-specific patterns so it only fires when nothing
# else matched.
RE_BARE_CALLSIGN = re.compile(r"//\s*(" + RE_CALLSIGN_CORE + r")(?:_[\w]+)?\b")

# Prose attribution "by/from CALLSIGN" in headers, inline comments, strings:
#   "// patch by K6AQ"
#   "// idea from VK5DGR"
# Applied AFTER the more-specific copyright / paren / slash regexes so
# that "Copyright (C) ... by NAME" is caught by RE_COPYRIGHT_CALLSIGN
# first.
RE_BY_CALLSIGN = re.compile(r"\b(?:by|from)\s+(" + RE_CALLSIGN_CORE + r")\b")

# Name-then-callsign prose pattern (freedv-gui-specific):
#   "David Rowe VK5DGR"
#   "Mooneer Salem K6AQ"
#   "David Witten KD0EAG"
# Two capitalized names immediately followed by a callsign.
RE_NAME_THEN_CALLSIGN = re.compile(
    r"\b[A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,3}\s+(" + RE_CALLSIGN_CORE + r")\b")

# Named credits in inline comments or attribution blocks.
# Most freedv-gui files use `// Authors:         Name1, Name2` headers,
# so we add an explicit "Authors:" entry on top of the deskhpsdr set.
RE_NAMED_CREDIT = re.compile(
    r"(?://|/\*)\s*Authors?\s*:\s*"
    r"([A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,3}"
    r"(?:\s*,\s*[A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,3})*)"
)
RE_NAMED_CREDIT_PROSE = re.compile(
    r"(?:Contribution\s+of\s+\S+(?:\s+\S+)*\s+from\s+|"
    r"Contributed\s+(?:initially\s+)?by\s+|"
    r"Credit[:,]?\s+(?:to\s+)?|"
    r"By\s+|"
    r"Author[:,]?\s+)"
    r"([A-Z][a-z]+(?:\s+[A-Z\"][a-z]+\"|[A-Z][a-z]+){0,3})"
)
# Copyright line named author:
#   "2015 - John Melton, G0ORX/N6LYT"
RE_COPYRIGHT_NAME = re.compile(
    r"\d{4}(?:,\d{4})*\s*-\s*([A-Z][a-z]+(?:\s+[A-Z][a-z]+){1,3})\b")

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

# Callsigns whose name/role are known but not embedded in the source in a
# machine-readable form (e.g. discovered only via the "by CALLSIGN" prose
# pattern). Used as a fallback in the merge step when neither the
# existing corpus nor the source tree provides the name inline.
KNOWN_IDENTITIES: dict[str, dict[str, str]] = {
    "VK5DGR": {
        "name": "David Rowe",
        "role": (
            "FreeDV / Codec2 founder; primary upstream author of freedv-gui"
        ),
    },
    "K6AQ": {
        "name": "Mooneer Salem",
        "role": (
            "freedv-gui primary maintainer; FreeDV Reporter, RADE, "
            "PSK Reporter client author"
        ),
    },
    "N2ADR": {
        "name": "Jim Ahlstrom",
        "role": "freedv-gui contributor; QuiskRigCtl integration",
    },
    "G0ORX": {
        "name": "John Melton",
        "role": (
            "piHPSDR primary author; appears in freedv-gui via "
            "shared protocol heritage"
        ),
    },
    "KD0EAG": {
        "name": "David Witten",
        "role": "freedv-gui co-creator (with David Rowe, 2012)",
    },
    "N6LYT": {
        "name": "John Melton",
        "role": (
            "John Melton's secondary US callsign; cross-listed with "
            "G0ORX where used"
        ),
    },
    "NH6Z": {
        "name": "Annaliese McDermond",
        "role": (
            "smartsdr-codec2 (FlexRadio integration) original author; "
            "VITA-49 packet layouts"
        ),
    },
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
    """Walk src/ inside freedv-gui - skip vendored 3rdparty/ sub-tree."""
    src_root = base / "src"
    if not src_root.is_dir():
        # Fallback: if no src/ subdirectory, walk entire base
        for p in base.rglob("*"):
            if p.is_file() and p.suffix in SCAN_SUFFIXES:
                yield p
        return
    for p in src_root.rglob("*"):
        if p.is_file() and p.suffix in SCAN_SUFFIXES:
            # Skip bundled 3rd-party subtree (carries its own attribution
            # under its own licenses; not the freedv-gui authorship corpus).
            if "3rdparty" in p.parts:
                continue
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
        # Split on commas in case the "Authors:" line carries several
        # names (e.g. "David Rowe, David Witten").
        for piece in re.split(r"\s*,\s*", name):
            piece = piece.strip().rstrip('"').strip()
            if not piece or len(piece) < 5:
                continue
            # Skip all-uppercase (callsign, not name)
            if piece == piece.upper():
                continue
            entry = named.setdefault(piece, {"count": 0, "first_seen": None})
            entry["count"] += 1
            if entry["first_seen"] is None:
                entry["first_seen"] = f"{src_file.name}:{line_num}"

    if not base.is_dir():
        print(f"[discover] WARN: freedv-gui not found at {base}",
              file=sys.stderr)
        return callsigns, named

    for src in iter_upstream_files(base):
        try:
            text = src.read_text(encoding="utf-8",
                                 errors="replace").splitlines()
        except Exception:
            continue
        for ln_idx, line in enumerate(text):
            # Parenthesized callsign (most common inline pattern)
            for m in RE_PAREN_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
            # Slash-separated callsigns: G0ORX/N6LYT
            for m in RE_SLASH_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
                record_callsign(m.group(2), src, ln_idx + 1)
            # Copyright-line comma callsign
            for m in RE_COPYRIGHT_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
            # Prose "by CALLSIGN" form
            for m in RE_BY_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
            # "Name CALLSIGN" prose pattern (freedv-gui about-dialog
            # style): "David Rowe VK5DGR", "Mooneer Salem K6AQ"
            for m in RE_NAME_THEN_CALLSIGN.finditer(line):
                record_callsign(m.group(1), src, ln_idx + 1)
            # Bare `// CALLSIGN:` inline tag - applied last (least
            # specific). Restrict to the comment tail to avoid firing
            # on non-comment code.
            if "//" in line:
                commented = "//" + line.split("//", 1)[1]
                for m in RE_BARE_CALLSIGN.finditer(commented):
                    record_callsign(m.group(1), src, ln_idx + 1)
            # Copyright-line named author
            for m in RE_COPYRIGHT_NAME.finditer(line):
                record_named(m.group(1), src, ln_idx + 1)
            # Named credits: "// Authors:         Mooneer Salem, ..."
            for m in RE_NAMED_CREDIT.finditer(line):
                record_named(m.group(1), src, ln_idx + 1)
            # Named credits prose (Contribution of ... from ...)
            for m in RE_NAMED_CREDIT_PROSE.finditer(line):
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

    freedv_dir = FREEDV_DIR
    if not freedv_dir.is_dir():
        # Probe alternative relative paths for worktree layouts
        alternatives = [
            REPO.parent / "freedv-gui",
            REPO.parent.parent / "freedv-gui",
            REPO.parent.parent.parent / "freedv-gui",
            REPO.parent.parent.parent.parent / "freedv-gui",
        ]
        found = next((p for p in alternatives if p.is_dir()), None)
        if found is None:
            print(f"FATAL: freedv-gui not found at {freedv_dir}",
                  file=sys.stderr)
            print("Set NEREUS_FREEDV_DIR or ensure freedv-gui is cloned",
                  file=sys.stderr)
            return 2
        freedv_dir = found

    callsigns, named = discover(freedv_dir)

    # Merge with existing corpus so human-edited fields (name, role)
    # are preserved across re-runs. Only mechanical fields (count,
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
        known = KNOWN_IDENTITIES.get(cs, {})
        meta["name"] = prev.get("name") or known.get("name") or None
        meta["role"] = prev.get("role") or known.get("role") or None

    for nm, meta in named.items():
        prev = existing_named.get(nm, {})
        meta.setdefault("role", prev.get("role", None))

    corpus = {
        "generated_utc": datetime.now(timezone.utc).isoformat(
            timespec="seconds"),
        "freedv_gui_sha": git_sha(freedv_dir),
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
            print("Run scripts/discover-freedv-author-tags.py and commit "
                  "the refreshed corpus (populate name/role fields).")
            return 1
        # Fail if any committed callsign still has name=null - same
        # unconditional enforcement as the Thetis / deskhpsdr discoverers.
        unnamed = sorted(
            cs for cs, meta in committed["callsign_tags"].items()
            if meta.get("name") in (None, "")
        )
        if unnamed:
            print(f"DRIFT: {len(unnamed)} callsign(s) in corpus have "
                  f"name=null:")
            for cs in unnamed:
                meta = committed["callsign_tags"][cs]
                print(f"  {cs}: {meta['first_seen']} "
                      f"(role: {meta.get('role') or '(null)'})")
            print("Populate the name field in "
                  "docs/attribution/freedv-gui-author-tags.json and commit.")
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
