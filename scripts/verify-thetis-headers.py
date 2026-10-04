#!/usr/bin/env python3
"""Verify upstream license-header markers are present in every derived
NereusSDR source file.

Originally this script covered only Thetis-derived files listed in
``docs/attribution/THETIS-PROVENANCE.md``. Pass 7 (2026-04-18, Compliance
Plan T7) extended coverage to two additional lineage kinds:

  * ``aethersdr`` — files in Bucket A of
    ``docs/attribution/aethersdr-reconciliation.md`` (genuine AetherSDR
    derivations). AetherSDR has no per-file copyright headers, so the
    check is for the project-level attribution we require in the port
    (``AetherSDR`` mention + Modification-history block).

  * ``wdsp`` — every ``.c``/``.h`` under ``third_party/wdsp/src/``. These
    are vendored verbatim from TAPR/OpenHPSDR-wdsp; the check ensures the
    Warren Pratt copyright + GPL permission block survived the import.

Use ``--all-kinds`` (or ``--kind=KIND``) to run non-default verifiers.
Default remains ``thetis`` for backwards compatibility with existing CI
and pre-push hook invocations.

Verbatim-preservation model (Pass 5, 2026-04-17 onward, thetis kind):
each NereusSDR file's header must contain the upstream source's own
top-of-file header BYTE-FOR-BYTE. The verifier therefore only checks for
anchor markers that every Thetis-derived file will carry:

  1. "Ported from" — anchors the NereusSDR port-citation block
  2. "Thetis"      — upstream identity (present in all cited Thetis sources)
  3. "Copyright (C)" — every cited GPL/LGPL source carries a copyright line
  4. "General Public License" — matches both GPL and LGPL
  5. "Modification history (NereusSDR)" — anchors the per-file mod block

The Dual-Licensing Statement check was dropped in Pass 5: its presence is
now 100 % determined by whether the upstream source has one in its
verbatim header, so per-variant gating is redundant.

Pass 6 (2026-04-17, Compliance Plan T12) added two structural checks
that look across the PROVENANCE table and source-file metadata:

  - **Orphan-pair check**: for every PROVENANCE-listed `.h`, require its
    sibling `.cpp`/`.cc`/`.c` (if it exists on disk) to either be listed
    in PROVENANCE too or carry an explicit opt-out marker. Same in
    reverse. Catches the T2 orphan defect class (header-cited .h with
    bare implementation .cpp) without waiting for an external auditor
    to find it.

  - **Samphire-marker consistency**: if a file's PROVENANCE source-list
    cell cites a known Samphire-authored Thetis source, require the
    file's first HEADER_WINDOW lines to contain "MW0LGE" — proves the
    upstream Samphire copyright/dual-license block actually got
    transcribed, not just paraphrased away.

Files under `docs/attribution/` themselves are exempt (they document the
templates, they are not themselves derived source).
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
from header_block import HEADER_WINDOW, header_text  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
AETHERSDR_RECONCILIATION = REPO / "docs" / "attribution" / "aethersdr-reconciliation.md"
WDSP_SRC_DIR = REPO / "third_party" / "wdsp" / "src"

# Per-kind required-marker tables. Keys must match the --kind CLI choices.
MARKERS_BY_KIND = {
    "thetis": [
        "Ported from",
        "Thetis",
        "Copyright (C)",
        "General Public License",
        "Modification history (NereusSDR)",
    ],
    "aethersdr": [
        "AetherSDR",
        "Modification history (NereusSDR)",
    ],
    "wdsp": [
        "Copyright (C)",
        "General Public License",
    ],
}

# Header must appear in the file's leading comment block, read whole
# however long it grows, and never less than HEADER_WINDOW lines
# (scripts/header_block.py).

# Opt-out marker for sibling files that intentionally carry no port header
# (e.g. pure Qt scaffolding whose semantics don't derive from the cited
# upstream). Must appear in the first HEADER_WINDOW lines of the sibling.
OPT_OUT_MARKER = "Independently implemented from"

# Sibling-pair extensions: when one of these is in PROVENANCE, look for
# the other on disk to check the orphan-pair invariant.
SIBLING_PAIRS = {
    ".h":   [".cpp", ".cc", ".c"],
    ".cpp": [".h", ".hpp"],
    ".cc":  [".h", ".hpp"],
    ".c":   [".h"],
    ".hpp": [".cpp", ".cc"],
}

# Known Samphire-authored upstream Thetis sources. If any of these appears
# in a PROVENANCE row's source-list cell, the corresponding NereusSDR file
# MUST contain "MW0LGE" in its header window — otherwise the verbatim
# Samphire copyright/dual-license block was not preserved.
SAMPHIRE_AUTHORED_SOURCES = {
    "MeterManager.cs",
    "cmaster.cs",
    "console.cs",          # heavily Samphire-modified
    "bandwidth_monitor.c",
    "bandwidth_monitor.h",
    "ucMeter.cs",
    "ucRadioList.cs",
    "frmMeterDisplay.cs",
    "frmAddCustomRadio.cs",
    "clsHardwareSpecific.cs",
    "clsDiscoveredRadioPicker.cs",
    "clsRadioDiscovery.cs",
    "DiversityForm.cs",
    "PSForm.cs",
}

# WDSP utility/build files exempt from the copyright-header check. These
# ship without a permission block in TAPR upstream, so we track the set
# explicitly rather than silently ignoring missing markers. Any new file
# added upstream fails the check until a human confirms the exemption is
# intentional.
#
# Documented in docs/attribution/WDSP-PROVENANCE.md under "License census".
# Pinned WDSP 2.10 gives calculus.c/.h full headers; FDnoiseIQ and fastmath
# are no longer in this tree. New Nereus ABI headers carry their own grants.
WDSP_HEADER_EXEMPTIONS = {
    # MSVC IDE artifacts (generated, no human authorship)
    "resource.h",
    "resource1.h",
    # Minimal wrappers (version stubs, empty headers)
    "version.c",
    "version.h",
    # FFTW3 third-party header vendored into WDSP (GPLv2-or-later; the
    # full GPLv2 text lives at third_party/fftw3/COPYING, and the
    # standalone FFTW3 provenance doc is
    # docs/attribution/FFTW3-PROVENANCE.md)
    "fftw3.h",
}


def parse_provenance(text: str):
    """Yield (file_path, source_cell) tuples from the PROVENANCE.md tables.

    Table rows we care about look like:
      | src/core/WdspEngine.cpp | cmaster.cs | full | port | samphire | ... |

    Returns the first cell (relative path) and the second cell (raw
    source-file list) for each row whose path exists on disk.
    """
    rows = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 2:
            continue
        candidate = cells[0]
        if not candidate or candidate.lower() in ("nereussdr file", "file"):
            continue
        rel = candidate.replace("`", "")
        if not (REPO / rel).is_file():
            continue
        source_cell = cells[1] if len(cells) >= 2 else ""
        rows.append((rel, source_cell))
    return rows


def parse_aethersdr_bucket_a(text: str):
    """Extract file paths from Bucket A of aethersdr-reconciliation.md.

    Bucket A is delimited by "## Bucket A" ... "## Bucket B". Within,
    per-topic tables list NereusSDR files in the first column.
    Returns a list of relative paths.
    """
    in_bucket_a = False
    paths = []
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("## Bucket A"):
            in_bucket_a = True
            continue
        if line.startswith("## Bucket "):
            in_bucket_a = False
            continue
        if not in_bucket_a:
            continue
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 2:
            continue
        candidate = cells[0].replace("`", "")
        if not candidate.startswith(("src/", "tests/")):
            continue
        if (REPO / candidate).is_file():
            paths.append(candidate)
    # De-dupe while preserving order
    seen = set()
    out = []
    for p in paths:
        if p in seen:
            continue
        seen.add(p)
        out.append(p)
    return out


def list_wdsp_sources():
    """Every .c/.h under third_party/wdsp/src/ except documented exempt."""
    if not WDSP_SRC_DIR.is_dir():
        return []
    out = []
    for p in sorted(WDSP_SRC_DIR.glob("*")):
        if p.suffix not in (".c", ".h"):
            continue
        if p.name in WDSP_HEADER_EXEMPTIONS:
            continue
        out.append(str(p.relative_to(REPO)))
    return out


CAT_SHARED_HEADER_PATHS = {
    "resources/cat/CATStructs.xml",
    "resources/cat/CommandContracts.json",
    "tests/data/cat/requests.json",
    "tests/data/cat/compatibility.csv",
}
CAT_XML_NO_HEADER = (
    "Upstream source has no top-of-file GPL header — project-level LICENSE applies"
)


def cat_shared_header_path(path: Path) -> Optional[str]:
    try:
        relative = path.relative_to(REPO).as_posix()
    except ValueError:
        return None
    return relative if relative in CAT_SHARED_HEADER_PATHS else None


def check_required_markers(path: Path, markers):
    head = attribution_header_text(path)
    if (cat_shared_header_path(path) == "resources/cat/CATStructs.xml"
            and markers == MARKERS_BY_KIND["thetis"]):
        # The exact upstream XML has no header. Never invent copyright:
        # require the documented project licence notice in its named sidecar.
        markers = [m for m in markers if m != "Copyright (C)"] + [CAT_XML_NO_HEADER]
        head = " ".join(head.split())
    return [m for m in markers if m not in head]


def attribution_header_text(path: Path) -> str:
    """Setup JSON and explicitly allowlisted CAT data use named sidecars."""
    if (cat_shared_header_path(path) or
            (path.suffix == ".json" and path.parent.name == "setup"
             and path.parent.parent.name == "resources")):
        headers = path.parent / "HEADERS.md"
        if not headers.is_file():
            return ""
        text = headers.read_text(errors="replace")
        # A shared header only covers resources explicitly named in it.
        return text if f"`{path.name}`" in text else ""
    return header_text(path.read_text(errors="replace"), path.suffix)


def check_orphan_pair(rel: str, listed) -> Optional[str]:
    """Return a failure message if a sibling file exists on disk but is
    neither listed in PROVENANCE nor carries the opt-out marker.
    """
    p = Path(rel)
    suffix = p.suffix
    if suffix not in SIBLING_PAIRS:
        return None
    for sib_ext in SIBLING_PAIRS[suffix]:
        sib_rel = str(p.with_suffix(sib_ext))
        sib_path = REPO / sib_rel
        if not sib_path.is_file():
            continue
        if sib_rel in listed:
            return None  # sibling also cited — OK
        # Check for opt-out marker in the sibling
        try:
            head = header_text(sib_path.read_text(errors="replace"), sib_path.suffix)
        except Exception:
            head = ""
        if OPT_OUT_MARKER in head:
            return None  # explicit opt-out — OK
        # Sibling exists, isn't listed, no opt-out: orphan
        return (
            f"orphan-sibling: {sib_rel} exists on disk but is not in "
            f"PROVENANCE and does not carry the opt-out marker "
            f'"// {OPT_OUT_MARKER} <upstream> interface". Either add a '
            f"PROVENANCE row or add the opt-out comment to its head."
        )
    return None


def check_samphire_marker(path: Path, source_cell: str) -> Optional[str]:
    """If the PROVENANCE source-list cites a Samphire-authored Thetis
    source, the file's header must contain MW0LGE.
    """
    cited = [s for s in SAMPHIRE_AUTHORED_SOURCES if s in source_cell]
    if not cited:
        return None
    head = attribution_header_text(path)
    if "MW0LGE" in head:
        return None
    return (
        f"missing-samphire-marker: PROVENANCE cites Samphire-authored "
        f"source(s) {sorted(cited)} but file header lacks 'MW0LGE'. "
        f"Verbatim Samphire copyright/dual-license block was likely not "
        f"transcribed."
    )


def verify_thetis_kind():
    """Original THETIS-PROVENANCE verifier logic."""
    if not PROVENANCE.is_file():
        print(f"ERROR: {PROVENANCE} not found", file=sys.stderr)
        return None
    rows = parse_provenance(PROVENANCE.read_text())
    if not rows:
        print("ERROR: no derived-file rows parsed from PROVENANCE.md",
              file=sys.stderr)
        return None
    listed = {rel for rel, _ in rows}
    markers = MARKERS_BY_KIND["thetis"]
    failures = 0
    for rel, source_cell in rows:
        path = REPO / rel
        problems = []
        missing = check_required_markers(path, markers)
        if missing:
            problems.append(f"missing-markers: {', '.join(missing)}")
        orphan = check_orphan_pair(rel, listed)
        if orphan:
            problems.append(orphan)
        samphire = check_samphire_marker(path, source_cell)
        if samphire:
            problems.append(samphire)
        if problems:
            failures += 1
            for p in problems:
                print(f"FAIL [thetis] {rel} — {p}")
    total = len(rows)
    print(f"\n[thetis]   {total - failures}/{total} files pass header check")
    return failures


def verify_aethersdr_kind():
    if not AETHERSDR_RECONCILIATION.is_file():
        print(f"ERROR: {AETHERSDR_RECONCILIATION} not found", file=sys.stderr)
        return None
    paths = parse_aethersdr_bucket_a(AETHERSDR_RECONCILIATION.read_text())
    if not paths:
        print("ERROR: no Bucket A rows parsed from aethersdr-reconciliation.md",
              file=sys.stderr)
        return None
    markers = MARKERS_BY_KIND["aethersdr"]
    failures = 0
    for rel in paths:
        path = REPO / rel
        missing = check_required_markers(path, markers)
        if missing:
            failures += 1
            print(f"FAIL [aethersdr] {rel} — missing-markers: {', '.join(missing)}")
    total = len(paths)
    print(f"[aethersdr] {total - failures}/{total} files pass header check")
    return failures


def verify_wdsp_kind():
    paths = list_wdsp_sources()
    # Pinned 2.10 also has source-derived C++ boundary adapters. Keep their
    # grants under the WDSP gate rather than inventing Thetis attribution.
    adapters = [
        "src/core/dsp/DspAssetValidation.cpp",
        "src/core/dsp/Ps3DisplayAdapter.cpp",
        "src/core/dsp/Ps3DisplayAdapter.h",
    ]
    paths.extend(adapters)
    if not paths:
        print(f"ERROR: no sources found under {WDSP_SRC_DIR}", file=sys.stderr)
        return None
    markers = MARKERS_BY_KIND["wdsp"]
    failures = 0
    for rel in paths:
        path = REPO / rel
        required = markers + (["Ported from TAPR", "Modification history (NereusSDR)"]
                              if rel in adapters else [])
        missing = check_required_markers(path, required)
        if missing:
            failures += 1
            print(f"FAIL [wdsp] {rel} — missing-markers: {', '.join(missing)}")
    total = len(paths)
    print(f"[wdsp]     {total - failures}/{total} files pass header check")
    return failures


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--kind",
        choices=["thetis", "aethersdr", "wdsp"],
        default="thetis",
        help="Which lineage kind to verify (default: thetis)",
    )
    ap.add_argument(
        "--all-kinds",
        action="store_true",
        help="Verify all kinds (thetis + aethersdr + wdsp)",
    )
    args = ap.parse_args()

    if args.all_kinds:
        kinds = ["thetis", "aethersdr", "wdsp"]
    else:
        kinds = [args.kind]

    total_failures = 0
    for k in kinds:
        if k == "thetis":
            f = verify_thetis_kind()
        elif k == "aethersdr":
            f = verify_aethersdr_kind()
        elif k == "wdsp":
            f = verify_wdsp_kind()
        else:
            print(f"ERROR: unknown kind {k!r}", file=sys.stderr)
            return 2
        if f is None:
            return 2
        total_failures += f

    return 0 if total_failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
