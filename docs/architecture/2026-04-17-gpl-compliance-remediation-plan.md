# GPL Compliance Remediation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute the 6-step GPL remediation agreed with Richard Samphire (MW0LGE) so that NereusSDR v0.2.0 ships with per-file Thetis copyright/license/attribution preservation, original artwork, re-framed public positioning, and a verifiable provenance inventory.

**Architecture:** A scripted verification pass gates every header-restoration batch. PROVENANCE.md is the single source of truth — it lists every derived file with its Thetis upstream, template variant, and contributor set. A header-insertion helper reads PROVENANCE.md and mechanically applies the right template to each file. Batches are committed by upstream grouping so each commit is reviewable.

**Tech Stack:** Python 3 (audit + insertion scripts), bash (verification), git (GPG-signed commits), gh CLI (release + issue management), existing NereusSDR C++/Qt6 build.

**Spec:** `docs/architecture/2026-04-16-gpl-compliance-remediation-design.md`

**Branch:** All work on `compliance/v0.2.0-remediation`. GPG-sign every commit. Trailer: `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`.

---

## Task ordering

```
Scaffold + templates + verify script (Tasks 1-3)
        ↓
Audit: outbound then inbound (Tasks 4-5)
        ↓
PROVENANCE.md finalized (Task 6)
        ↓
Header-insertion helper (Task 7)
        ↓
Header batches — thetis-samphire, thetis-no-samphire, mi0bot, multi-source (Tasks 8-11)
        ↓                      ↑
        ↓              (parallel to 8-11)
ANAN artwork + cross-needle audit + ASSETS.md (Tasks 12-13)
        ↓
README + CLAUDE.md + CONTRIBUTING.md re-framing (Tasks 14-16)
        ↓
Legacy branch deletion (Task 17)
        ↓
Version bump + CHANGELOG (Task 18)
        ↓
Build smoke + verification checklist (Tasks 19-20)
        ↓
Merge to main (Task 21)
        ↓
Courtesy bundle to Richie — manual (Task 22)
        ↓
Quiet period + v0.2.0 release — manual (Task 23)
```

---

### Task 1: Scaffold `docs/attribution/` directory

**Files:**
- Create: `docs/attribution/README.md`

- [ ] **Step 1: Create the directory and index file**

```bash
mkdir -p docs/attribution
```

- [ ] **Step 2: Write the index**

Create `docs/attribution/README.md`:

```markdown
# NereusSDR Attribution

This directory holds the public-facing attribution record for NereusSDR:

- [`HEADER-TEMPLATES.md`](HEADER-TEMPLATES.md) — Canonical header-block
  templates applied to every file derived from Thetis, mi0bot/Thetis-HL2,
  or other upstream GPL code. Any new port must use one of these templates.
- [`THETIS-PROVENANCE.md`](THETIS-PROVENANCE.md) — File-by-file inventory
  mapping each NereusSDR derived source file to its Thetis upstream source,
  line ranges, derivation type, and contributor set.
- [`ASSETS.md`](ASSETS.md) — Inventory of every graphical/binary asset
  under `resources/` and `docs/images/` with origin, author, and license.

Per-file license headers live in the source files themselves; this
directory is the index.

NereusSDR as a whole is licensed under GPLv3 (see root `LICENSE`), which
is compatible with Thetis's GPLv2-or-later terms.
```

- [ ] **Step 3: Commit**

```bash
git add docs/attribution/README.md
git commit -S -m "docs(compliance): scaffold docs/attribution/ directory"
```

---

### Task 2: Write `HEADER-TEMPLATES.md`

**Files:**
- Create: `docs/attribution/HEADER-TEMPLATES.md`

- [ ] **Step 1: Write the four template variants**

Create `docs/attribution/HEADER-TEMPLATES.md` with this content:

````markdown
# Header Templates for Thetis-Derived Files

Every NereusSDR source file listed in `THETIS-PROVENANCE.md` must carry
one of the four header blocks below, placed immediately after any
`#pragma once` / include guard and before the first `#include`.

Per spec `docs/architecture/2026-04-16-gpl-compliance-remediation-design.md`
§6.

Substitution rules:
- `<FILENAME>` — relative path from repo root
- `<THETIS_SOURCE_PATHS>` — list of Thetis source paths (newline-separated
  if multiple), each prefixed with `//   ` and beginning with
  `Project Files/Source/...`
- `<YEAR_RANGE>` for each contributor — inherit from the Thetis source
  file's copyright block verbatim (do not recompute)
- `<PORT_DATE>` — ISO date `YYYY-MM-DD` when the port was introduced into
  NereusSDR (use today's date at header-insertion time for existing files;
  for new ports, use the port date)

---

## Variant 1 — `thetis-samphire`

**Use when:** the Thetis source file(s) contain contributions from
Richard Samphire (MW0LGE). His per-file header line reads
`Copyright (C) 2019-2026 Richard Samphire (MW0LGE) – heavily modified`
(or similar).

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
<THETIS_SOURCE_PATHS>
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================
```

---

## Variant 2 — `thetis-no-samphire`

**Use when:** the Thetis source file(s) contain only FlexRadio / Wigley
contributions (no Samphire heavy-modification line). Drop the
dual-licensing block; keep the two-copyright-line form.

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
<THETIS_SOURCE_PATHS>
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================
```

---

## Variant 3 — `mi0bot`

**Use when:** the source file comes from the mi0bot/Thetis-HL2 fork. Adds
Reid Campbell's attribution and keeps the upstream Thetis chain the fork
itself descends from.

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/Thetis-HL2 source:
<THETIS_SOURCE_PATHS>
//
// Original copyright and license (preserved per GNU GPL):
//
//   Thetis / Thetis-HL2 is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — Thetis heavy modifications
//   Copyright (C) <YEAR_RANGE>  Reid Campbell (MI0BOT) — Hermes-Lite fork contributions
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions in the upstream Thetis source — preserved verbatim from
// Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================
```

---

## Variant 4 — `multi-source`

**Use when:** the NereusSDR file synthesizes logic from multiple Thetis
source files with different contributor sets. Enumerate each source; list
the union of contributors. If any cited source includes Samphire
contributions, include the dual-licensing block.

Skeleton (adapt copyright lines to match union of cited sources):

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
<THETIS_SOURCE_PATHS>
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   <additional per-file contributor lines as present in the sources>
//
//   [GPLv2-or-later permission block verbatim — see Variant 1]
//
// [Samphire dual-licensing block if any cited source had Samphire content]
//
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================
```

---

## Non-applicability

Files that do NOT require a Thetis header:

- Pure NereusSDR-native Qt widgets (`VfoWidget`, `HGauge`, `ColorSwatchButton`,
  applet panel widgets, etc.)
- `src/models/Band.h` (IARU Region 2 spec data — no Thetis source consulted)
- Build files, CI config, shell scripts, markdown docs
- `third_party/wdsp/` (already vendored with upstream TAPR attribution)
- `third_party/fftw3/` (out of scope)

Files in these categories are listed in `THETIS-PROVENANCE.md` under the
"Independently implemented — Thetis-like but not derived" section with a
short justification.
````

- [ ] **Step 2: Commit**

```bash
git add docs/attribution/HEADER-TEMPLATES.md
git commit -S -m "docs(compliance): header template variants for derived files"
```

---

### Task 3: Write verification script `scripts/verify-thetis-headers.py`

**Files:**
- Create: `scripts/verify-thetis-headers.py`
- Test: run against tree (expect failures until Task 8+ lands)

- [ ] **Step 1: Write the script**

Create `scripts/verify-thetis-headers.py`:

```python
#!/usr/bin/env python3
"""Verify that every file declared in THETIS-PROVENANCE.md carries the
required Thetis license header markers. Exit 1 on any failure.

Required markers per header (all must appear in first 120 lines):
  1. "Ported from" (anchors the attribution block)
  2. "Thetis"
  3. "Copyright (C)"
  4. "GNU General Public License"
  5. "Modification history (NereusSDR)"

Samphire-sourced files additionally require:
  6. "Dual-Licensing Statement"

Files under `docs/attribution/` themselves are exempt (they document the
templates, they are not themselves derived source).
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"

REQUIRED_MARKERS = [
    "Ported from",
    "Thetis",
    "Copyright (C)",
    "GNU General Public License",
    "Modification history (NereusSDR)",
]
SAMPHIRE_MARKER = "Dual-Licensing Statement"

# Header must appear within this many lines of top of file
HEADER_WINDOW = 120


def parse_provenance(text: str):
    """Yield (file_path, variant) tuples from the PROVENANCE.md tables.

    Table rows we care about look like:
      | src/core/WdspEngine.cpp | cmaster.cs | full | port | samphire | ... |

    We accept any column layout whose first cell is a repo-relative path
    that exists on disk. Variant is detected from a 'variant' or 'template'
    column keyword, or inferred as 'thetis-no-samphire' if unspecified.
    """
    rows = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells:
            continue
        candidate = cells[0]
        if not candidate or candidate.lower() in ("nereussdr file", "file"):
            continue
        rel = candidate.replace("`", "")
        if not (REPO / rel).is_file():
            continue
        joined = " ".join(c.lower() for c in cells)
        if "samphire" in joined or "mi0bot" in joined or "multi-source" in joined:
            variant = "samphire-required"
        else:
            variant = "plain"
        rows.append((rel, variant))
    return rows


def check_file(path: Path, variant: str):
    head = "\n".join(path.read_text(errors="replace").splitlines()[:HEADER_WINDOW])
    missing = [m for m in REQUIRED_MARKERS if m not in head]
    if variant == "samphire-required" and SAMPHIRE_MARKER not in head:
        missing.append(SAMPHIRE_MARKER)
    return missing


def main():
    if not PROVENANCE.is_file():
        print(f"ERROR: {PROVENANCE} not found", file=sys.stderr)
        return 2
    rows = parse_provenance(PROVENANCE.read_text())
    if not rows:
        print("ERROR: no derived-file rows parsed from PROVENANCE.md",
              file=sys.stderr)
        return 2
    failures = 0
    for rel, variant in rows:
        missing = check_file(REPO / rel, variant)
        if missing:
            failures += 1
            print(f"FAIL {rel} — missing: {', '.join(missing)}")
    total = len(rows)
    ok = total - failures
    print(f"\n{ok}/{total} files pass header check")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Make it executable**

```bash
chmod +x scripts/verify-thetis-headers.py
```

- [ ] **Step 3: Verify it runs (expect error — no PROVENANCE.md yet)**

```bash
python3 scripts/verify-thetis-headers.py
```

Expected output:
```
ERROR: .../docs/attribution/THETIS-PROVENANCE.md not found
```
Exit code 2.

- [ ] **Step 4: Commit**

```bash
git add scripts/verify-thetis-headers.py
git commit -S -m "chore(compliance): add header-verification script"
```

---

### Task 4: Outbound audit sweep

**Files:**
- Create: `docs/attribution/audit-outbound.md` (intermediate work product)

- [ ] **Step 1: Enumerate self-declared derivations**

```bash
grep -rlnE "From Thetis|Porting from|Thetis Project Files|mi0bot|MW0LGE|W5WC|clsHardwareSpecific|MeterManager\.cs|console\.cs|setup\.cs|NetworkIO\.cs|cmaster\.cs|display\.cs|RXA\.c|TXA\.c|clsRadioDiscovery" src/ tests/ 2>/dev/null | sort -u > /tmp/nereus-outbound.txt
wc -l /tmp/nereus-outbound.txt
```

Expected: ~90-130 files. Record the count.

- [ ] **Step 2: For each file, extract cited Thetis paths**

Run per-file inspection to build a mapping. For each file in the list,
read the top 40 lines and any in-body `// From Thetis ...` comments:

```bash
while read -r f; do
    echo "=== $f ==="
    head -40 "$f" | grep -E "Thetis|mi0bot|MW0LGE" || true
    grep -nE "// From Thetis|// Porting" "$f" | head -5
done < /tmp/nereus-outbound.txt > /tmp/nereus-outbound-raw.txt
```

- [ ] **Step 3: Structure findings**

Create `docs/attribution/audit-outbound.md` with one section per NereusSDR
file. For each, record:
- NereusSDR file path
- Every cited Thetis source path (read the actual Thetis file at
  `~/Thetis/<path>` to confirm it exists and extract its contributor list)
- Line ranges cited
- Derivation type guess (port / reference / structural / wrapper)
- Template variant (thetis-samphire / thetis-no-samphire / mi0bot / multi-source)

Structure:
```markdown
# Outbound audit — self-declared Thetis derivations

Intermediate work product feeding THETIS-PROVENANCE.md. Delete after
PROVENANCE.md is finalized.

## src/core/WdspEngine.cpp
- Thetis sources: `Project Files/Source/Console/cmaster.cs`
- Thetis copyright: FlexRadio, Wigley, Samphire (dual-license applies)
- Lines cited: per-function comments through file
- Type: port
- Variant: thetis-samphire

## src/core/RadioDiscovery.cpp
- mi0bot source: `clsRadioDiscovery.cs`
- Copyright: Wigley + MI0BOT (fork), Samphire in upstream Thetis chain
- Type: port
- Variant: mi0bot

[...all ~90-130 files...]
```

- [ ] **Step 4: Cross-check against Thetis sources**

For every Thetis source cited, confirm by opening the file at
`~/Thetis/<path>` and noting its actual per-file copyright block. Record
the contributor names and year ranges verbatim — these feed the
header templates.

- [ ] **Step 5: Commit the intermediate work product**

```bash
git add docs/attribution/audit-outbound.md
git commit -S -m "docs(compliance): outbound audit — self-declared derivations"
```

---

### Task 5: Inbound audit sweep

**Files:**
- Create: `docs/attribution/audit-inbound.md` (intermediate work product)

- [ ] **Step 1: List all Thetis sources cited from Task 4**

Extract unique Thetis paths from `audit-outbound.md`:

```bash
grep -oE "Project Files/Source[^\`]*\.(cs|c|h)" docs/attribution/audit-outbound.md | sort -u > /tmp/thetis-sources-cited.txt
grep -oE "[A-Za-z][A-Za-z0-9_]*\.(cs|c|h)" docs/attribution/audit-outbound.md | sort -u > /tmp/thetis-filenames.txt
wc -l /tmp/thetis-sources-cited.txt /tmp/thetis-filenames.txt
```

- [ ] **Step 2: For each Thetis source, search NereusSDR for undeclared uses**

For every file in `/tmp/thetis-filenames.txt`, search NereusSDR for files
that reproduce its distinctive symbols (function names, constants) but do
NOT currently cite it:

```bash
# Example for MeterManager.cs
rg -l 'shouldRender|meterFilterRule|applyMeterRule' src/ | sort -u
# Cross-reference: which of these already cite MeterManager.cs?
rg -l 'MeterManager' src/
# The set-difference is the inbound finding list
```

Do this for each cited Thetis source. Richie flagged `MeterManager.cs:1049`
specifically — that file is guaranteed to surface NereusSDR equivalents
that didn't declare the dependency.

- [ ] **Step 3: Inspect each candidate**

For each candidate, read the NereusSDR file and judge:
- **Is it actually derived** from the Thetis source? (Same constants,
  same structure, same byte layouts?) → add to PROVENANCE.md as a
  newly-discovered derivation.
- **Is it independently implemented** but resembles Thetis? (E.g.
  implementation followed OpenHPSDR public spec rather than Thetis
  source.) → add to PROVENANCE.md "Independently implemented" section
  with a short justification.

- [ ] **Step 4: Record findings**

Create `docs/attribution/audit-inbound.md`:

```markdown
# Inbound audit — discovered / judged derivations

Intermediate work product. Each entry is either folded into PROVENANCE.md
as a new derived-file row, or into the "Independently implemented"
section with a justification.

## Newly discovered derivations
- src/gui/MeterWidget.cpp → MeterManager.cs:1049 (Richie-flagged example)
- [...]

## Independently implemented — Thetis-like but not derived
- src/models/Band.h: band enum order mirrors Thetis convention; band
  frequency boundaries come from IARU Region 2 spec; no Thetis source
  consulted.
- [...]
```

- [ ] **Step 5: Commit**

```bash
git add docs/attribution/audit-inbound.md
git commit -S -m "docs(compliance): inbound audit — discovered + judged derivations"
```

---

### Task 6: Finalize `THETIS-PROVENANCE.md`

**Files:**
- Create: `docs/attribution/THETIS-PROVENANCE.md`
- Delete (after Task 7 passes): `docs/attribution/audit-outbound.md`, `docs/attribution/audit-inbound.md`

- [ ] **Step 1: Consolidate Tasks 4 + 5 into a single structured inventory**

Create `docs/attribution/THETIS-PROVENANCE.md`:

```markdown
# Thetis Provenance — NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on Thetis (ramdor/Thetis), its mi0bot/Thetis-HL2
fork, or WDSP (TAPR/OpenHPSDR-wdsp). Per-file license headers live in the
source files themselves; this index is the grep-able summary.

NereusSDR as a whole is distributed under GPLv3 (root `LICENSE`), which is
compatible with Thetis's GPLv2-or-later terms.

## Legend

Derivation type:
- `port`       — direct reimplementation in C++/Qt6 of a Thetis source file
- `reference`  — consulted for behavior during independent implementation
- `structural` — architectural template with substantive behavioral echo
- `wrapper`    — thin C++ wrapper around vendored C source (WDSP)

Template variant (see `HEADER-TEMPLATES.md`):
- `thetis-samphire`     — Thetis source contains Samphire contributions
- `thetis-no-samphire`  — Thetis source has no Samphire contributions
- `mi0bot`              — sourced from mi0bot/Thetis-HL2 fork
- `multi-source`        — synthesizes multiple Thetis sources

## Files derived from ramdor/Thetis

| NereusSDR file | Thetis source | Line ranges | Type | Variant | Notes |
| --- | --- | --- | --- | --- | --- |
| src/core/WdspEngine.cpp | Project Files/Source/Console/cmaster.cs | full | port | thetis-samphire | |
| ... (populate from audit-outbound.md + audit-inbound.md "newly discovered") |

## Files derived from mi0bot/Thetis-HL2

| NereusSDR file | mi0bot source | Line ranges | Type | Variant | Notes |
| --- | --- | --- | --- | --- | --- |
| src/core/RadioDiscovery.cpp | clsRadioDiscovery.cs | full | port | mi0bot | |
| ... |

## Files derived from TAPR WDSP

Vendored in `third_party/wdsp/` with upstream TAPR/OpenHPSDR-wdsp license
and Warren Pratt (NR0V) attribution preserved. No per-file NereusSDR
header required. See `third_party/wdsp/README.md` and
`third_party/wdsp/LICENSE`.

## Independently implemented — Thetis-like but not derived

Files whose behavior resembles Thetis but whose implementation was
written without consulting Thetis source. No header required.

| NereusSDR file | Behavioral resemblance | Basis of implementation |
| --- | --- | --- |
| src/models/Band.h | Band enum order mirrors Thetis convention | Ham-band definitions from IARU Region 2 spec; no Thetis source consulted |
| ... |
```

- [ ] **Step 2: Validate the file paths in the tables**

```bash
# Extract first column (file path) from every table row, verify file exists
awk -F'|' '/^\|[ ]*[a-zA-Z]/ {gsub(/^ *| *$/, "", $2); print $2}' \
    docs/attribution/THETIS-PROVENANCE.md | \
  grep -E '^src/|^tests/' | \
  while read -r f; do
    if [[ ! -f "$f" ]]; then echo "MISSING: $f"; fi
  done
```

Expected output: empty (all listed files exist).

- [ ] **Step 3: Run verification script (expect failures — no headers yet)**

```bash
python3 scripts/verify-thetis-headers.py
```

Expected: all listed files report missing markers. Exit code 1. Record
the `N/N files pass` line — it sets the baseline count.

- [ ] **Step 4: Commit**

```bash
git add docs/attribution/THETIS-PROVENANCE.md
git commit -S -m "docs(compliance): THETIS-PROVENANCE.md — full derived-file inventory"
```

- [ ] **Step 5: Remove intermediate work products**

```bash
git rm docs/attribution/audit-outbound.md docs/attribution/audit-inbound.md
git commit -S -m "docs(compliance): drop intermediate audit work products"
```

---

### Task 7: Header-insertion helper `scripts/insert-thetis-headers.py`

**Files:**
- Create: `scripts/insert-thetis-headers.py`

- [ ] **Step 1: Write the helper**

Create `scripts/insert-thetis-headers.py`:

```python
#!/usr/bin/env python3
"""Insert the correct Thetis header template into files listed in
THETIS-PROVENANCE.md.

Reads PROVENANCE.md, for each derived-file row determines the variant
from the table, loads the matching template block from HEADER-TEMPLATES.md,
substitutes `<FILENAME>`, `<THETIS_SOURCE_PATHS>`, and `<PORT_DATE>`, and
injects the rendered header into the file if it does not already have
one (detected by presence of the 'Ported from' marker).

Idempotent: running twice does nothing on the second pass.

Usage:
    python3 scripts/insert-thetis-headers.py --variant thetis-samphire [--dry-run]
    python3 scripts/insert-thetis-headers.py --file src/core/WdspEngine.cpp [--dry-run]
    python3 scripts/insert-thetis-headers.py --all [--dry-run]
"""

import argparse
import re
import sys
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROVENANCE = REPO / "docs" / "attribution" / "THETIS-PROVENANCE.md"
TEMPLATES = REPO / "docs" / "attribution" / "HEADER-TEMPLATES.md"
ALREADY_HEADERED = "Ported from"


def load_templates():
    """Parse HEADER-TEMPLATES.md, return {variant_name: template_text}."""
    text = TEMPLATES.read_text()
    variants = {}
    current = None
    current_block = []
    in_code = False
    for line in text.splitlines():
        m = re.match(r"^## Variant \d+ — `([a-z\-]+)`", line)
        if m:
            if current and current_block:
                variants[current] = "\n".join(current_block).rstrip() + "\n"
            current = m.group(1)
            current_block = []
            in_code = False
            continue
        if current is None:
            continue
        if line.startswith("```cpp"):
            in_code = True
            continue
        if line.startswith("```") and in_code:
            variants[current] = "\n".join(current_block).rstrip() + "\n"
            current_block = []
            current = None
            in_code = False
            continue
        if in_code:
            current_block.append(line)
    return variants


def parse_provenance():
    """Yield dicts: {file, sources, variant}."""
    text = PROVENANCE.read_text()
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("|") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 5:
            continue
        rel = cells[0].replace("`", "")
        if not (REPO / rel).is_file():
            continue
        src = cells[1]
        variant = cells[4].strip().lower()
        if variant not in ("thetis-samphire", "thetis-no-samphire",
                           "mi0bot", "multi-source"):
            continue
        yield {"file": rel, "source": src, "variant": variant}


def render(template, filename, sources):
    """Apply substitutions. `sources` is a list of Thetis paths."""
    src_block = "\n".join(f"//   {s}" for s in sources)
    out = template.replace("<FILENAME>", filename)
    out = out.replace("<THETIS_SOURCE_PATHS>", src_block)
    out = out.replace("<PORT_DATE>", date.today().isoformat())
    return out


def insert(path: Path, header: str, dry_run: bool):
    content = path.read_text()
    if ALREADY_HEADERED in content.splitlines()[0:120] if False else (
            ALREADY_HEADERED in "\n".join(content.splitlines()[:120])):
        return False
    # Preserve any leading `#pragma once` or include guard
    lines = content.splitlines()
    insert_at = 0
    for i, line in enumerate(lines[:10]):
        s = line.strip()
        if s.startswith("#pragma once") or s.startswith("#ifndef ") or \
                s.startswith("#define ") or s == "":
            insert_at = i + 1
            continue
        break
    new_content = "\n".join(lines[:insert_at]) + \
        ("\n" if insert_at else "") + header + "\n" + \
        "\n".join(lines[insert_at:]) + "\n"
    if dry_run:
        print(f"[DRY] would header: {path.relative_to(REPO)}")
        return True
    path.write_text(new_content)
    print(f"headered: {path.relative_to(REPO)}")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", help="only process this variant")
    ap.add_argument("--file", help="only process this file path")
    ap.add_argument("--all", action="store_true", help="process all")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    if not (args.variant or args.file or args.all):
        ap.error("pick one of --variant, --file, --all")
    templates = load_templates()
    touched = 0
    for row in parse_provenance():
        if args.file and row["file"] != args.file:
            continue
        if args.variant and row["variant"] != args.variant:
            continue
        template = templates.get(row["variant"])
        if not template:
            print(f"WARN: no template for variant {row['variant']}",
                  file=sys.stderr)
            continue
        sources = [s.strip() for s in row["source"].split(";") if s.strip()]
        rendered = render(template, row["file"], sources)
        if insert(REPO / row["file"], rendered, args.dry_run):
            touched += 1
    print(f"\n{touched} files touched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Make it executable, dry-run it against a single file**

```bash
chmod +x scripts/insert-thetis-headers.py
python3 scripts/insert-thetis-headers.py --file src/core/WdspEngine.cpp --dry-run
```

Expected: `[DRY] would header: src/core/WdspEngine.cpp` + `1 files touched`.

- [ ] **Step 3: Commit**

```bash
git add scripts/insert-thetis-headers.py
git commit -S -m "chore(compliance): add header-insertion helper"
```

---

### Task 8: Apply `thetis-samphire` variant

**Files:** every file in PROVENANCE.md with variant `thetis-samphire`.

- [ ] **Step 1: Dry-run the variant**

```bash
python3 scripts/insert-thetis-headers.py --variant thetis-samphire --dry-run
```

Expected: list of files + count. Record the count.

- [ ] **Step 2: Apply**

```bash
python3 scripts/insert-thetis-headers.py --variant thetis-samphire
```

- [ ] **Step 3: Run verifier (expect the samphire subset to pass)**

```bash
python3 scripts/verify-thetis-headers.py
```

Expected: the samphire-variant files in PROVENANCE.md pass; the other
variants still fail. Count matches Step 1.

- [ ] **Step 4: Spot-check 3 random files against their Thetis sources**

For each of 3 headered files picked at random, open the cited Thetis
source file (`~/Thetis/<path>`) and confirm the copyright lines in the
NereusSDR header exactly match the Thetis file's copyright block.

- [ ] **Step 5: Build smoke — confirm headers don't break compilation**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo 2>&1 | tail -5
cmake --build build --target NereusSDR -j 2>&1 | tail -10
```

Expected: build succeeds. If any file errors (e.g. header block dropped
before a file-level `namespace` declaration in a way that breaks), the
insertion script picked the wrong insert point — fix that file by hand,
adjust the script's insert-point logic, re-run.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -S -m "feat(compliance): restore Thetis headers — samphire variant"
```

---

### Task 9: Apply `thetis-no-samphire` variant

Same steps as Task 8 but with `--variant thetis-no-samphire`. Record the
count. Commit message:
`feat(compliance): restore Thetis headers — no-samphire variant`.

- [ ] **Step 1: Dry-run**

```bash
python3 scripts/insert-thetis-headers.py --variant thetis-no-samphire --dry-run
```

- [ ] **Step 2: Apply**

```bash
python3 scripts/insert-thetis-headers.py --variant thetis-no-samphire
```

- [ ] **Step 3: Verify**

```bash
python3 scripts/verify-thetis-headers.py
```

- [ ] **Step 4: Spot-check 3 random files**

Same as Task 8.5.

- [ ] **Step 5: Build smoke**

```bash
cmake --build build --target NereusSDR -j 2>&1 | tail -10
```

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -S -m "feat(compliance): restore Thetis headers — no-samphire variant"
```

---

### Task 10: Apply `mi0bot` variant

Same pattern.

- [ ] **Step 1: Dry-run**

```bash
python3 scripts/insert-thetis-headers.py --variant mi0bot --dry-run
```

- [ ] **Step 2: Apply**

```bash
python3 scripts/insert-thetis-headers.py --variant mi0bot
```

- [ ] **Step 3: Verify**

```bash
python3 scripts/verify-thetis-headers.py
```

- [ ] **Step 4: Spot-check against both the mi0bot source AND the ramdor/Thetis upstream**

Confirm that the mi0bot-variant header correctly preserves both Reid
Campbell's mi0bot attribution AND the ramdor/Thetis chain (including
Samphire dual-license block).

- [ ] **Step 5: Build smoke**

```bash
cmake --build build --target NereusSDR -j 2>&1 | tail -10
```

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -S -m "feat(compliance): restore Thetis headers — mi0bot variant"
```

---

### Task 11: Apply `multi-source` variant

Same pattern, slightly more careful. Multi-source files need their
header's copyright block to reflect the union of contributors from every
cited Thetis file — verify by hand after the script runs.

- [ ] **Step 1: Dry-run**

```bash
python3 scripts/insert-thetis-headers.py --variant multi-source --dry-run
```

- [ ] **Step 2: Apply**

```bash
python3 scripts/insert-thetis-headers.py --variant multi-source
```

- [ ] **Step 3: Verify**

```bash
python3 scripts/verify-thetis-headers.py
```

Expected: all files now pass. Failure count = 0.

- [ ] **Step 4: Review every multi-source file by hand**

For each multi-source file, confirm the rendered copyright block contains
copyright lines for every contributor named in every cited Thetis source.
If the script's template didn't capture the union, edit the file and
expand the block by hand.

- [ ] **Step 5: Build smoke + full test suite**

```bash
cmake --build build -j 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: build green, tests pass.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -S -m "feat(compliance): restore Thetis headers — multi-source variant"
```

---

### Task 12: ANAN multimeter face replacement

**Files:**
- Delete: `resources/meters/ananMM.png`
- Create: replacement artwork (Qt-rendered path recommended; bitmap fallback acceptable)

Decision gate: choose rendering approach. Recommended = Qt-rendered.

#### Option A (recommended): Qt-rendered face

- [ ] **Step 1: Delete the existing bitmap**

```bash
git rm resources/meters/ananMM.png
```

- [ ] **Step 2: Find the load site**

```bash
rg -n "ananMM\.png|ananMM" src/
```

Expected hits: `NeedleItem` or `ItemGroup` preset factory method.

- [ ] **Step 3: Replace with a Qt-painted face in the needle item**

Modify the identified load site so that when the ANANMM preset is
selected, the face is painted via `QPainter` directly rather than loading
a bitmap. Use generic NereusSDR styling:

- Dark navy background `#0f0f1a` (matches project palette)
- Light arc tick marks `#c8d8e8` at 10-division spacing
- Scale labels in generic sans-serif (Qt default), not Ernst's digital font
- No "ANAN" or "Apache Labs" text
- Title: "NereusSDR Multi-Meter" or similar neutral wording

Exact implementation depends on the load-site class; follow the
`NeedleItem::paint()` pattern already established for other needle
geometries.

- [ ] **Step 4: Build + visual smoke**

```bash
cmake --build build -j 2>&1 | tail -5
./build/NereusSDR &
```

In the running app, select the ANANMM preset in a container and confirm
the face renders with the new generic styling and no bitmap load errors
in the log.

- [ ] **Step 5: Kill + commit**

```bash
pkill -f "build/NereusSDR"
git add -A
git commit -S -m "feat(compliance): replace ANAN multimeter bitmap with original Qt-painted face"
```

#### Option B (fallback): New bitmap

- [ ] **Step 1: Create `resources/meters/ananMM.png` as an originally-designed face**

Design must not use Ernst-style fonts, must not include ANAN/Apache
branding, must not reproduce the Thetis skin face layout.

- [ ] **Step 2: Commit**

```bash
git add resources/meters/ananMM.png
git commit -S -m "feat(compliance): replace ANAN multimeter face with original artwork"
```

---

### Task 13: Cross-needle asset audit + `ASSETS.md`

**Files:**
- Inspect: `resources/meters/cross-needle.png`, `resources/meters/cross-needle-bg.png`
- Create: `docs/attribution/ASSETS.md`

- [ ] **Step 1: Check git history on the cross-needle assets**

```bash
git log --all --follow --format="%h %ad %s" -- resources/meters/cross-needle.png resources/meters/cross-needle-bg.png
```

Record the commit that introduced each. If the add was part of a
"ported from Thetis skin" or similar commit, treat as derivative.

- [ ] **Step 2: Compare visually against Thetis skins**

```bash
ls ~/Thetis/Skins/ 2>/dev/null
```

If a Thetis skin contains a cross-needle-bg bitmap that matches, the
NereusSDR file is derivative and must be replaced.

- [ ] **Step 3: Replace if derivative**

If derivative, delete and replace with Qt-painted or originally-drawn
alternatives using the same approach as Task 12. If genuinely original,
leave in place — Task 13.4 documents them.

- [ ] **Step 4: Write `ASSETS.md`**

Create `docs/attribution/ASSETS.md`:

```markdown
# NereusSDR Asset Provenance

Inventory of every graphical/binary asset shipped in NereusSDR. Per-asset
origin, author, and license.

## resources/meters/
| File | Origin | Author | License | Added |
| --- | --- | --- | --- | --- |
| ananMM.png | Original NereusSDR artwork / Qt-painted | J.J. Boyd (KG4VCF) | GPLv3 | 2026-04-17 |
| cross-needle.png | [original/derivative — fill from Task 13.1-13.3] | | | |
| cross-needle-bg.png | [same] | | | |

## resources/icons/
| File | Origin | Author | License | Added |
| --- | --- | --- | --- | --- |
| NereusSDR.png / NereusSDR.ico / NereusSDR.icns / iconset/* | Original NereusSDR branding | [author] | GPLv3 | [date] |

## resources/shaders/
All shaders are NereusSDR-original (written for Qt6 QRhi). GPLv3.

## resources/bandplans/
All JSON band-plan files are derived from publicly published IARU and
ARRL band plans, not from Thetis. Distributed per GPLv3.

## docs/images/
| File | Origin | Author | License | Added |
| --- | --- | --- | --- | --- |
| nereussdr-v016-screenshot.jpg | Screenshot of the running app | J.J. Boyd (KG4VCF) | GPLv3 | |
```

- [ ] **Step 5: Commit**

```bash
git add docs/attribution/ASSETS.md
git commit -S -m "docs(compliance): ASSETS.md — graphical asset provenance"
```

---

### Task 14: README opening + footer re-framing

**Files:**
- Modify: `README.md:10` (opening paragraph) and `README.md:267` (footer)

- [ ] **Step 1: Replace the opening paragraph**

Edit `README.md`. Find:

```
NereusSDR is an independent cross-platform SDR client deeply informed by the workflow, feature set, and operating style of [Thetis](https://github.com/ramdor/Thetis), reimagined with a new GUI, a modernized architecture, and native support for macOS, Linux, and Windows.
```

Replace with:

```
NereusSDR is a C++20/Qt6 port of [Thetis](https://github.com/ramdor/Thetis) — the canonical OpenHPSDR / Apache Labs SDR console, itself descended from FlexRadio PowerSDR — carrying its radio logic, DSP integration, and feature set forward to a native cross-platform codebase (macOS, Linux, Windows) with a Qt-based GUI. The Thetis contributor lineage (FlexRadio Systems, Doug Wigley W5WC, Richard Samphire MW0LGE, and the wider OpenHPSDR community) is preserved per-file in source headers and summarized in [docs/attribution/THETIS-PROVENANCE.md](docs/attribution/THETIS-PROVENANCE.md). Distributed under GPLv3, compatible with Thetis's GPLv2-or-later terms.
```

- [ ] **Step 2: Replace the footer**

Find:

```
*NereusSDR is an independent project and is not affiliated with or endorsed by Apache Labs or the OpenHPSDR project.*
```

Replace with:

```
*NereusSDR is a derivative work of Thetis licensed under the GNU General Public License. It is not affiliated with or endorsed by Apache Labs, FlexRadio Systems, ramdor/Thetis, or the OpenHPSDR project.*
```

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -S -m "docs(compliance): re-frame README as port-of-Thetis"
```

---

### Task 15: CLAUDE.md source-first license-preservation rule

**Files:**
- Modify: `CLAUDE.md` — insert new block inside `⚠️ SOURCE-FIRST PORTING PROTOCOL` section, after the `READ → SHOW → TRANSLATE` block

- [ ] **Step 1: Insert the license-preservation rule**

Find the line in `CLAUDE.md` that ends the `### The Rule: READ → SHOW → TRANSLATE` block (it ends with "behavior and logic must come from Thetis.") and insert immediately after it:

```markdown

### License-preservation rule (non-negotiable)

When porting any Thetis file, you MUST — in the same commit that introduces
the port — copy the following from the Thetis source into the NereusSDR
file's header comment:

1. All `Copyright (C)` lines naming contributors (FlexRadio, Wigley,
   Samphire, W2PA, mi0bot, etc.)
2. The GPLv2-or-later permission block verbatim
3. The Samphire dual-licensing statement — ONLY if the Thetis source file
   contains Samphire-authored contributions
4. A trailing "Modification history (NereusSDR)" block with the port date,
   human author, and AI tooling disclosure

Templates live in `docs/attribution/HEADER-TEMPLATES.md`. Failure to
preserve these notices on a new port is a GPL compliance bug, not a style
nit — reject the PR.
```

- [ ] **Step 2: Add a row to the "What Counts As 'Guessing'" list**

Find the list that begins "- Writing a function body without first reading the Thetis equivalent" and append:

```markdown
- Porting a Thetis file without copying its license header and appending a modification note
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -S -m "docs(compliance): add license-preservation rule to source-first protocol"
```

---

### Task 16: CONTRIBUTING.md license-preservation section

**Files:**
- Modify: `CONTRIBUTING.md` — new section before "Code of Conduct"

- [ ] **Step 1: Insert the section**

Find the heading `## Code of Conduct` and insert directly before it:

```markdown
## License Preservation on Derived Code

Any PR that ports, translates, or materially adapts Thetis source code
into NereusSDR must preserve the original license header, copyright
lines, and dual-licensing notices from the Thetis source file, and append
a dated modification note to the NereusSDR file. See
[docs/attribution/HEADER-TEMPLATES.md](docs/attribution/HEADER-TEMPLATES.md)
for templates and [docs/attribution/THETIS-PROVENANCE.md](docs/attribution/THETIS-PROVENANCE.md)
for the existing provenance inventory.

This is a merge-blocking requirement, not a style preference.

```

- [ ] **Step 2: Commit**

```bash
git add CONTRIBUTING.md
git commit -S -m "docs(compliance): CONTRIBUTING.md license-preservation requirement"
```

---

### Task 17: Delete legacy `docs/readme-independent-framing` branch

**Files:** remote branch on `origin`.

- [ ] **Step 1: Confirm no outstanding work**

```bash
git log --oneline origin/main..origin/docs/readme-independent-framing 2>/dev/null | head -5
```

If the branch has unique commits that aren't in `main` and aren't reflected
in the Task 14 README re-framing, capture them before deletion. If just
the old "independent" framing — delete.

- [ ] **Step 2: Delete remote branch**

```bash
git push origin --delete docs/readme-independent-framing
```

- [ ] **Step 3: Prune local tracking branch**

```bash
git fetch --prune
```

(No commit — branch deletion is a ref-level action, not a tree change.)

---

### Task 18: Version bump + CHANGELOG

**Files:**
- Modify: `CMakeLists.txt` (version number)
- Modify: `CHANGELOG.md` (new v0.2.0 entry)

- [ ] **Step 1: Find current version string**

```bash
grep -n "project(NereusSDR\|VERSION " CMakeLists.txt | head -5
```

- [ ] **Step 2: Bump to 0.2.0**

Edit `CMakeLists.txt` — change the `VERSION 0.1.7` (or whatever the exact
line reads) to `VERSION 0.2.0`.

- [ ] **Step 3: Add CHANGELOG entry**

Edit `CHANGELOG.md`. Find the most recent entry header and insert a new
`## [0.2.0] - 2026-04-17` (or the actual release date) block above it:

```markdown
## [0.2.0] - 2026-04-17

### License compliance remediation

- Restored Thetis copyright, GPLv2-or-later permission, per-contributor
  attribution, and Samphire dual-licensing notices across all derived
  source files.
- Added per-file modification history blocks disclosing reimplementation
  date, author, and AI-assisted transformation, per GPL notice-preservation
  requirements.
- Published `docs/attribution/THETIS-PROVENANCE.md` — complete
  file-by-file provenance inventory covering ramdor/Thetis,
  mi0bot/Thetis-HL2, and TAPR WDSP derivations.
- Replaced `resources/meters/ananMM.png` with original NereusSDR artwork;
  audited remaining meter assets and documented in
  `docs/attribution/ASSETS.md`.
- Re-framed README to accurately describe NereusSDR as a C++20/Qt6 port
  of Thetis within the FlexRadio / OpenHPSDR / Thetis lineage.
- Added non-negotiable license-preservation rule to the source-first
  porting protocol in CLAUDE.md and to CONTRIBUTING.md.
- No functional changes.

**Binaries from v0.1.1–v0.1.7 remain withdrawn and should not be
redistributed.**
```

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt CHANGELOG.md
git commit -S -m "release: v0.2.0 — license compliance remediation"
```

---

### Task 19: Re-enable `release.yml` tag-push trigger

**Files:**
- Modify: `.github/workflows/release.yml`

Only do this at the end, after all other tasks are green. The trigger was
frozen in commit `d3e3e29` to prevent accidental release-builds during
remediation.

- [ ] **Step 1: Un-comment the trigger**

Edit `.github/workflows/release.yml`. Find the block:

```yaml
on:
  # Tag-push trigger disabled during compliance/v0.2.0-remediation work.
  # Re-enable only after the compliance branch has merged and Richie's
  # pre-release courtesy review has cleared. See
  # docs/architecture/2026-04-16-gpl-compliance-remediation-design.md §4.4.
  # push:
  #   tags: ['v*']
  workflow_dispatch:
```

Replace with:

```yaml
on:
  push:
    tags: ['v*']
  workflow_dispatch:
```

- [ ] **Step 2: Commit**

```bash
git add .github/workflows/release.yml
git commit -S -m "ci(compliance): re-enable release.yml tag-push trigger"
```

---

### Task 20: Final verification sweep

**Files:** none modified — this is a checklist walk.

- [ ] **Step 1: Run verification script on full tree**

```bash
python3 scripts/verify-thetis-headers.py
```

Expected: `N/N files pass header check`. Zero failures.

- [ ] **Step 2: Walk the spec §12 checklist**

Open `docs/architecture/2026-04-16-gpl-compliance-remediation-design.md`
§12 and confirm every checkbox is green:

- [ ] Every file in `THETIS-PROVENANCE.md` carries the new header block (verifier passed, Step 1)
- [ ] Random spot-check: 10 headered files verified against cited Thetis sources
- [ ] Inbound audit documented — "Independently implemented" section populated
- [ ] `third_party/wdsp/` and `third_party/fftw3/` not modified (`git diff --stat main...HEAD -- third_party/`)
- [ ] `resources/meters/ananMM.png` replaced
- [ ] `cross-needle*.png` audited (replaced or documented)
- [ ] `docs/attribution/ASSETS.md` present, complete
- [ ] README opening and footer replaced
- [ ] `docs/readme-independent-framing` deleted on origin
- [ ] CLAUDE.md license-preservation rule added
- [ ] CONTRIBUTING.md section added
- [ ] `docs/attribution/HEADER-TEMPLATES.md` present
- [ ] CHANGELOG v0.2.0 entry written
- [ ] `CMakeLists.txt` at 0.2.0
- [ ] All compliance-branch commits GPG-signed (`git log --show-signature compliance/v0.2.0-remediation ^origin/main | grep -c "gpg: Good signature"` matches `git rev-list --count compliance/v0.2.0-remediation ^origin/main`)

- [ ] **Step 3: Clean build + test on all accessible platforms**

Minimum:

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j 2>&1 | tail -20
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Expected: clean build, all tests green.

If multi-platform access exists, repeat on macOS, Linux, and Windows
(per standing NereusSDR release practice).

- [ ] **Step 4: Run the app briefly**

```bash
./build/NereusSDR &
sleep 3
pkill -f "build/NereusSDR"
```

Expected: app launches without crash, no license-related log warnings.

- [ ] **Step 5: Write verification summary**

Create `docs/architecture/2026-04-17-compliance-verification.md` documenting:
- Total derived files headered (from Step 1 output)
- Spot-check results (Step 2)
- Build + test results (Step 3)
- App smoke (Step 4)

Commit it:

```bash
git add docs/architecture/2026-04-17-compliance-verification.md
git commit -S -m "docs(compliance): verification summary"
```

---

### Task 21: Merge compliance branch to main

**Prerequisites:** Task 20 complete, verification green.

- [ ] **Step 1: Push final state**

```bash
git push origin compliance/v0.2.0-remediation
```

- [ ] **Step 2: Open PR**

```bash
gh pr create --title "compliance: v0.2.0 — GPL remediation" --body "$(cat <<'EOF'
## Summary

Executes the 6-step GPL compliance remediation agreed with Richard
Samphire (MW0LGE) per the design spec at
`docs/architecture/2026-04-16-gpl-compliance-remediation-design.md`.

- Full Thetis copyright, GPLv2-or-later permission, per-contributor
  attribution, and Samphire dual-licensing notices restored across all
  derived source files (see `docs/attribution/THETIS-PROVENANCE.md`)
- Modification history with AI-assisted transformation disclosure on
  every derived file
- `resources/meters/ananMM.png` replaced with original NereusSDR artwork;
  all graphical assets audited in `docs/attribution/ASSETS.md`
- README re-framed as C++20/Qt6 port of Thetis within the OpenHPSDR
  lineage
- CLAUDE.md source-first protocol updated with non-negotiable
  license-preservation rule; CONTRIBUTING.md mirror requirement
- `release.yml` tag-push trigger re-enabled (was frozen during
  remediation)

## Test plan

- [ ] `python3 scripts/verify-thetis-headers.py` — all derived files pass
- [ ] Clean build on Linux / macOS / Windows
- [ ] Full test suite green
- [ ] App smoke — launch + exit without crash
- [ ] Manual spot-check of 10 random headered files against cited Thetis sources

J.J. Boyd ~ KG4VCF

Co-Authored with Claude Code
EOF
)"
```

- [ ] **Step 3: After CI green, merge**

```bash
gh pr merge --squash --delete-branch
```

(Squash-merge keeps `main` history clean; the compliance work is logically
one change. Delete the branch after merge.)

---

### Task 22 (MANUAL): Courtesy bundle to Richie

**Files:** none — human step.

Executed by the maintainer on Discord after Task 21 merges. Draft text
lives in spec §9.4. Sending this is a prerequisite to Task 23.

- [ ] **Step 1: Capture numbers for the message**

```bash
python3 scripts/verify-thetis-headers.py | tail -3
```

Pull the `N/N files pass` number to fill `[N]` in the draft.

- [ ] **Step 2: Collect sample-file links**

Pick 3 files from `THETIS-PROVENANCE.md` — one `thetis-samphire`, one
`thetis-no-samphire`, one `mi0bot`. Collect GitHub `main`-branch URLs
for each.

- [ ] **Step 3: Capture before/after of `ananMM.png`**

Export the original bitmap (from git history at v0.1.7) and the new face
(screenshot of running app with ANANMM preset). Create a side-by-side
image.

- [ ] **Step 4: Send the Discord message**

Use draft from spec §9.4, fill in `[N]` and links. Send on the existing
Discord thread with Richie.

---

### Task 23 (MANUAL): Quiet period + v0.2.0 release

**Files:** none — human step.

- [ ] **Step 1: Wait for Richie's response**

Three outcomes:
- **Ack / "ship it"** → proceed immediately to Step 3
- **Correction requests** → address them on a follow-up branch merged to
  `main`, re-send bundle (loops back to Task 22)
- **Silence** → five business days + one polite follow-up ping, then
  proceed to Step 3

- [ ] **Step 2: Discord announcement before release**

Post on the Discord thread: "About to cut v0.2.0 now unless any last
concerns — thanks again for the patience."

- [ ] **Step 3: Tag and release**

```bash
git checkout main
git pull
git tag -s v0.2.0 -m "NereusSDR v0.2.0 — license compliance remediation"
git push origin v0.2.0
```

The re-enabled `release.yml` workflow runs automatically on tag push,
produces signed binaries for all three platforms, and creates a GitHub
Release draft.

- [ ] **Step 4: Fill release notes + publish**

```bash
gh release edit v0.2.0 --notes "$(cat <<'EOF'
# NereusSDR v0.2.0 — License compliance remediation

This release supersedes the withdrawn v0.1.x alpha builds. It restores
full per-file Thetis copyright, GPL license, per-contributor attribution,
and Samphire dual-licensing notices across all derived source files, adds
modification history disclosing reimplementation date and AI-assisted
transformation, replaces the ANAN multimeter face with original
artwork, and re-frames the project as a C++20/Qt6 port of Thetis within
the FlexRadio / OpenHPSDR / Thetis contributor lineage.

No functional changes from v0.1.7.

See:
- [`docs/attribution/THETIS-PROVENANCE.md`](https://github.com/boydsoftprez/NereusSDR/blob/main/docs/attribution/THETIS-PROVENANCE.md) — file-by-file provenance inventory
- [`CHANGELOG.md`](https://github.com/boydsoftprez/NereusSDR/blob/main/CHANGELOG.md) — full entry

[If Richie accepted a reviewer credit, add: "Compliance reviewed by Richard Samphire (MW0LGE)."]

**v0.1.1 through v0.1.7 binaries remain withdrawn and should not be redistributed.**

— J.J. Boyd ~ KG4VCF

Co-Authored with Claude Code
EOF
)"
gh release edit v0.2.0 --draft=false
```

- [ ] **Step 5: Cross-post to Thetis Facebook group**

Reply on the v0.1.7 announcement thread with a v0.2.0 announcement,
linking the release page and the pinned withdrawal-note issue.

- [ ] **Step 6: Unpin the withdrawal-note issue**

```bash
gh issue unpin 46
gh issue close 46 --comment "Resolved by v0.2.0 release."
```

---

## Self-review notes

- **Spec coverage:** Every workstream A–G in the spec maps to at least one
  task. Task 4-6 cover workstream B (audit). Tasks 7-11 cover workstream C
  (header restoration). Tasks 12-13 cover workstream D (artwork). Tasks
  14-16 cover workstream E (re-framing). Task 17 covers the legacy-branch
  deletion. Tasks 18-20 cover F (version bump + verification). Tasks
  21-23 cover G (release). Chunk A immediate actions were executed
  outside this plan; the plan begins after they completed.
- **Placeholder scan:** No "TBD" / "TODO" / "fill in details" in action
  steps. The PROVENANCE.md and audit files intentionally contain
  placeholder table rows because their content is populated during
  execution from the audit sweep — that is content to be produced by the
  task, not a gap in the plan itself.
- **Type consistency:** Script names used consistently:
  `scripts/verify-thetis-headers.py` (introduced Task 3, used Tasks 6,
  8-11, 20, 22) and `scripts/insert-thetis-headers.py` (Task 7, used
  Tasks 8-11). Variant names consistent across templates, verifier,
  inserter, and provenance: `thetis-samphire`, `thetis-no-samphire`,
  `mi0bot`, `multi-source`.
- **Known sequencing risk:** Task 12 (ANAN artwork) runs after the header
  tasks. If the maintainer chooses Option A (Qt-render) and that requires
  touching a file that already received a Thetis header (likely
  `NeedleItem.cpp`), the header must not be disturbed — the paint change
  goes in the function body only. The task step explicitly flags this.
