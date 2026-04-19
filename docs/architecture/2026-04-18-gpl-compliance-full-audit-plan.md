# GPL Compliance — Full-Tree Line-by-Line Audit Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a repeatable, exhaustive audit that classifies every file and every ported line in NereusSDR against its upstream origin, produces a public compliance inventory, closes every remaining gap from the 2026-04-17 v0.2.0 remediation sweep, and prevents regression via CI gates.

**Architecture:** Build on existing infrastructure (`verify-thetis-headers.py`, `verify-provenance-sync.py`, `check-new-ports.py`, `THETIS-PROVENANCE.md`, `WDSP-PROVENANCE.md`, `HOW-TO-PORT.md`, pre-commit hook). Extend scope to (a) vendored third-party trees (`third_party/wdsp/`, `third_party/fftw3/`), (b) inline cite version stamps across the entire tree, (c) binary distribution artifacts, (d) full-file inventory with per-file classification. Fix known gaps first (quick wins), then build a whole-tree inventory, then remediate findings, then lock it in via CI.

**Tech stack:** Python 3 for audit scripts, Markdown for inventories/PROVENANCE docs, CMake/shell for build-time compliance, `release.yml` for distribution artifacts. No C++ code changes required.

**Workflow:** Per task, reviewer confirms **implement / modify / skip**. Tasks ordered by severity-then-cost. Each task is independent and gets its own GPG-signed commit. Branch: `compliance/full-audit-2026-04-18`.

---

## Phase 0 — Foundational Gaps (Quick Wins)

These close known holes identified in the 2026-04-18 compliance review. No audit needed; the gaps are already established.

### Task 1 — Drop `COPYING` into `third_party/wdsp/`

**Severity:** MEDIUM (GPLv2 §1: each copy of the Program must be "accompanied by a copy of this License." Headers point to "you should have received a copy" — but the nearest copy lives outside the vendored dir. Self-containment is the cure.)

**Files:**
- Create: `third_party/wdsp/COPYING`

**Steps:**

- [ ] **Step 1: Copy GPLv2 text into the vendored tree**

```bash
cp docs/attribution/LICENSE-GPLv2 third_party/wdsp/COPYING
```

- [ ] **Step 2: Verify byte-identical to canonical FSF text**

```bash
diff third_party/wdsp/COPYING docs/attribution/LICENSE-GPLv2
```

Expected: empty diff.

- [ ] **Step 3: Commit**

```bash
git add third_party/wdsp/COPYING
git commit -S -m "compliance: add COPYING (GPLv2) to third_party/wdsp/

GPLv2 §1 requires a copy of the license to accompany the Program.
Place a verbatim GPLv2 copy alongside the WDSP sources so the
vendored tree is self-contained if extracted independently.

Refs: docs/attribution/WDSP-PROVENANCE.md"
```

---

### Task 2 — Correct GPL wording in `third_party/wdsp/CMakeLists.txt`

**Severity:** LOW (wording bug — current text says "GPL-2.0" which understates the grant; WDSP headers all say "version 2 … or any later version" = GPL-2.0-or-later. This exact phrase is what makes our GPLv3 §5(b) election lawful.)

**Files:**
- Modify: `third_party/wdsp/CMakeLists.txt:6`

**Steps:**

- [ ] **Step 1: Apply one-line edit**

Replace line 6:

```cmake
# WDSP is licensed under GPL-2.0.
```

With:

```cmake
# WDSP is licensed under GPL-2.0-or-later. NereusSDR distributes the
# combined work under GPLv3 via GPLv3 §5(b) upgrade. See
# docs/attribution/WDSP-PROVENANCE.md.
```

- [ ] **Step 2: Build check**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc) --target wdsp_static
```

Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add third_party/wdsp/CMakeLists.txt
git commit -S -m "compliance: correct WDSP licence wording to GPL-2.0-or-later"
```

---

### Task 3 — Create `FFTW3-PROVENANCE.md` and drop `COPYING` into `third_party/fftw3/`

**Severity:** HIGH (FFTW3 is currently un-documented in our attribution tree. We ship `libfftw3f-3.dll`, `libfftw3f-3.a`, `fftw3.h` for Windows targets with no LICENSE file alongside. FFTW3 is GPL-2.0-or-later (MIT commercial license is the alternative, which we do not hold). This is the single most defensible gap.)

**Files:**
- Create: `third_party/fftw3/COPYING`
- Create: `docs/attribution/FFTW3-PROVENANCE.md`
- Modify: `docs/attribution/README.md` (link new provenance doc)

**Steps:**

- [ ] **Step 1: Download canonical FFTW3 COPYING**

```bash
curl -fsSL https://raw.githubusercontent.com/FFTW/fftw3/master/COPYING \
  -o third_party/fftw3/COPYING
```

- [ ] **Step 2: Verify SHA-256 matches upstream advertised hash**

Record the sha256 in the provenance doc (next step).

```bash
shasum -a 256 third_party/fftw3/COPYING
```

- [ ] **Step 3: Write `docs/attribution/FFTW3-PROVENANCE.md`**

Content template (mirror the WDSP-PROVENANCE.md structure):

```markdown
# FFTW3 Provenance & License

FFTW3 (Matteo Frigo & Steven G. Johnson's FFT library) ships pre-built
binaries in `third_party/fftw3/` for Windows targets only. Linux and
macOS builds use system FFTW3 (GPL-2.0-or-later from the distro).

## Upstream

- **Authors:** Matteo Frigo, Steven G. Johnson, MIT
- **Canonical repository:** https://github.com/FFTW/fftw3
- **Upstream homepage:** https://fftw.org/
- **Version in NereusSDR:** 3.3.10 (per `libfftw3f-3.dll` header)
- **Binaries vendored from:** https://fftw.org/install/windows.html
- **Vendored file tree:**
  - `third_party/fftw3/include/fftw3.h`
  - `third_party/fftw3/lib/libfftw3f-3.a`
  - `third_party/fftw3/lib/libfftw3f-3.def`
  - `third_party/fftw3/bin/libfftw3f-3.dll`
  - `third_party/fftw3/COPYING` (GPLv2 text)

## Licence

FFTW3 is dual-licensed:

1. **GPL-2.0-or-later** (what NereusSDR uses — compatible with GPLv3 §5(b))
2. **MIT commercial licence** (purchased from MIT; NereusSDR does NOT hold this)

The GPL route is unambiguous for our distribution. See upstream FAQ:
https://www.fftw.org/faq/section1.html#isfftwfree

## Compatibility with NereusSDR

Identical argument as WDSP: GPLv3 §5(b) permits combining GPLv3 code with
code under GPLv2-or-later. FFTW3's "or any later version" language
satisfies this directly.

## NereusSDR modifications

None. Binaries are used verbatim from upstream distribution.

## Binary distribution

`libfftw3f-3.dll` ships inside the NereusSDR Windows installer and portable
ZIP. GPLv2 §3 (source-code availability) is satisfied by the public upstream
download at https://fftw.org/install/windows.html; NereusSDR release notes
point to this location.
```

- [ ] **Step 4: Add pointer from `docs/attribution/README.md`**

Add bullet under "Per-component provenance":

```markdown
- [FFTW3-PROVENANCE.md](FFTW3-PROVENANCE.md) — FFTW3 binaries vendored at `third_party/fftw3/`
```

- [ ] **Step 5: Build check**

```bash
cmake --build build -j$(nproc)
```

Expected: SUCCESS (no build change).

- [ ] **Step 6: Commit**

```bash
git add third_party/fftw3/COPYING docs/attribution/FFTW3-PROVENANCE.md docs/attribution/README.md
git commit -S -m "compliance: add FFTW3 COPYING + PROVENANCE doc

FFTW3 is GPL-2.0-or-later (dual-licensed with a commercial MIT option
we do not hold). Document the licence, upstream source, and vendored
binary inventory. Drop GPLv2 text into third_party/fftw3/ so the
vendored tree is self-contained.

Closes a compliance gap identified in the 2026-04-18 audit."
```

---

## Phase 1 — Full-Tree Inventory & Classification

This phase produces the line-by-line manifest the user asked for.

### Task 4 — Write `scripts/compliance-inventory.py`

**Severity:** FOUNDATIONAL (without a tree-wide inventory, "every line every file" is aspirational. This script is the audit's backbone.)

**Files:**
- Create: `scripts/compliance-inventory.py`
- Create: `tests/compliance/test_inventory.py` (pytest harness)

**Purpose:** Walk every tracked file under version control, classify it into exactly one of:
- `nereussdr-original` — no upstream lineage
- `thetis-port` — listed in `THETIS-PROVENANCE.md`
- `aethersdr-port` — listed in `aethersdr-reconciliation.md`
- `mi0bot-port` — listed in `THETIS-PROVENANCE.md` with `mi0bot` variant
- `wdsp-vendored` — under `third_party/wdsp/`
- `fftw3-vendored` — under `third_party/fftw3/`
- `build-artifact` — under `build/`, `build-clean/`, etc. (git-ignored; sanity check only)
- `docs` — under `docs/` non-attribution
- `attribution-doc` — under `docs/attribution/`
- `resource` — under `resources/`, `resources.qrc`
- `test` — under `tests/`
- `packaging` — under `packaging/`, `.github/`

Per classification, verify expected header markers (from `HOW-TO-PORT.md`):

| Classification | Required markers |
|---|---|
| `thetis-port` | "Ported from", "Copyright (C)", "General Public License", "Modification history (NereusSDR)", version stamp |
| `wdsp-vendored` | "Copyright (C)", upstream Warren Pratt / G0ORX name, GPL permission block (unchanged) |
| `fftw3-vendored` | N/A (binaries + one header — doc-level only) |
| `aethersdr-port` | NereusSDR port-citation + AetherSDR project URL (per HOW-TO-PORT rule 6) |
| `nereussdr-original` | NereusSDR GPLv3 header OR is documented as template-exempt (e.g. generated Qt files) |

**Steps:**

- [ ] **Step 1: Write failing test**

`tests/compliance/test_inventory.py`:

```python
import subprocess
import json
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent

def test_inventory_produces_every_tracked_file():
    """The inventory must cover every git-tracked file. No silent drops."""
    tracked = subprocess.check_output(
        ["git", "-C", str(REPO), "ls-files"], text=True
    ).splitlines()
    result = subprocess.run(
        ["python3", "scripts/compliance-inventory.py", "--format=json"],
        cwd=REPO, capture_output=True, text=True, check=True
    )
    inventory = json.loads(result.stdout)
    inventoried = {row["path"] for row in inventory}
    missing = set(tracked) - inventoried
    assert not missing, f"{len(missing)} tracked files missing from inventory: {sorted(missing)[:10]}"

def test_inventory_classifies_wdsp_sources_as_vendored():
    result = subprocess.run(
        ["python3", "scripts/compliance-inventory.py", "--format=json"],
        cwd=REPO, capture_output=True, text=True, check=True
    )
    inventory = json.loads(result.stdout)
    wdsp = [r for r in inventory if r["path"].startswith("third_party/wdsp/src/")]
    assert wdsp, "no wdsp source files found"
    assert all(r["classification"] == "wdsp-vendored" for r in wdsp)
```

- [ ] **Step 2: Run test — expect failure**

```bash
pytest tests/compliance/test_inventory.py -v
```

Expected: FAIL (script does not exist).

- [ ] **Step 3: Implement `scripts/compliance-inventory.py`**

Top-level structure:

```python
#!/usr/bin/env python3
"""Walk git-tracked files, classify each by upstream lineage, emit
markdown or JSON inventory. Read by CI and by the
docs/attribution/COMPLIANCE-INVENTORY.md generator.

Usage:
  python3 scripts/compliance-inventory.py              # markdown to stdout
  python3 scripts/compliance-inventory.py --format=json
  python3 scripts/compliance-inventory.py --fail-on-unclassified
"""
import argparse, json, re, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

def tracked_files():
    out = subprocess.check_output(
        ["git", "-C", str(REPO), "ls-files"], text=True
    )
    return [line.strip() for line in out.splitlines() if line.strip()]

def parse_provenance_paths(md_path, table_heading_regex):
    """Return the set of NereusSDR paths listed in a PROVENANCE markdown
    table. Table rows look like:
       | src/core/Foo.cpp | Project Files/Source/... | ...
    """
    text = (REPO / md_path).read_text(encoding="utf-8")
    paths = set()
    for line in text.splitlines():
        if not line.startswith("| "): continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 2: continue
        first = cells[0]
        if first.startswith(("src/", "tests/", "third_party/")):
            paths.add(first)
    return paths

THETIS_PORTS     = parse_provenance_paths("docs/attribution/THETIS-PROVENANCE.md", None)
AETHER_PORTS     = parse_provenance_paths("docs/attribution/aethersdr-reconciliation.md", None)

def classify(path: str) -> str:
    if path.startswith("third_party/wdsp/"):   return "wdsp-vendored"
    if path.startswith("third_party/fftw3/"):  return "fftw3-vendored"
    if path in THETIS_PORTS:                   return "thetis-port"
    if path in AETHER_PORTS:                   return "aethersdr-port"
    if path.startswith("docs/attribution/"):   return "attribution-doc"
    if path.startswith("docs/"):               return "docs"
    if path.startswith("tests/"):              return "test"
    if path.startswith("packaging/") or path.startswith(".github/"):
        return "packaging"
    if path.startswith("resources/") or path == "resources.qrc":
        return "resource"
    return "nereussdr-original"

REQUIRED_MARKERS = {
    "thetis-port":     ["Ported from", "Thetis", "Copyright (C)", "General Public License", "Modification history (NereusSDR)"],
    "aethersdr-port":  ["AetherSDR", "Modification history (NereusSDR)"],
    "wdsp-vendored":   ["Copyright (C)", "General Public License"],
    "nereussdr-original": [],  # GPLv3 header encouraged, not merge-gated
}

def verify_markers(path, classification):
    markers = REQUIRED_MARKERS.get(classification, [])
    if not markers: return []
    try:
        head = (REPO / path).read_text(encoding="utf-8", errors="replace")[:8000]
    except (IsADirectoryError, FileNotFoundError):
        return []
    return [m for m in markers if m not in head]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--format", choices=["markdown", "json"], default="markdown")
    ap.add_argument("--fail-on-unclassified", action="store_true")
    args = ap.parse_args()

    rows = []
    unclassified = 0
    for path in tracked_files():
        if Path(REPO / path).is_dir(): continue
        c = classify(path)
        missing = verify_markers(path, c)
        rows.append({"path": path, "classification": c, "missing_markers": missing})
        if c == "nereussdr-original" and path.endswith((".cpp", ".h", ".py")):
            # soft signal, not a failure
            pass

    if args.format == "json":
        print(json.dumps(rows, indent=2))
    else:
        # Render markdown by classification
        buckets = {}
        for r in rows:
            buckets.setdefault(r["classification"], []).append(r)
        for c, items in sorted(buckets.items()):
            print(f"## {c} ({len(items)})\n")
            for r in items:
                flag = "" if not r["missing_markers"] else f"  ⚠ missing: {r['missing_markers']}"
                print(f"- `{r['path']}`{flag}")
            print()

    if args.fail_on_unclassified:
        bad = [r for r in rows if r["missing_markers"]]
        if bad:
            print(f"\nFAIL: {len(bad)} files missing required markers", file=sys.stderr)
            sys.exit(1)

if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run test — expect pass**

```bash
pytest tests/compliance/test_inventory.py -v
```

Expected: PASS.

- [ ] **Step 5: Run against the tree — capture output**

```bash
python3 scripts/compliance-inventory.py > /tmp/inventory.md
wc -l /tmp/inventory.md
```

Record the line count in the commit message for future baseline comparisons.

- [ ] **Step 6: Commit**

```bash
git add scripts/compliance-inventory.py tests/compliance/test_inventory.py
git commit -S -m "compliance: add tree-wide inventory script + test"
```

---

### Task 5 — Generate & commit `docs/attribution/COMPLIANCE-INVENTORY.md` baseline

**Severity:** MEDIUM (the inventory must be checked in so the public can verify; also provides the delta baseline for future drift detection.)

**Files:**
- Create: `docs/attribution/COMPLIANCE-INVENTORY.md`

**Steps:**

- [ ] **Step 1: Generate the inventory**

```bash
python3 scripts/compliance-inventory.py > docs/attribution/COMPLIANCE-INVENTORY.md
```

- [ ] **Step 2: Prepend a frontmatter block**

Manually edit the top of the file to add:

```markdown
# Compliance Inventory (auto-generated)

Generated by `scripts/compliance-inventory.py` on 2026-04-18.

This is the canonical manifest of every git-tracked file in NereusSDR,
classified by upstream lineage. Regenerate whenever THETIS-PROVENANCE.md,
aethersdr-reconciliation.md, WDSP-PROVENANCE.md, or the source tree
changes. Drift between this file and the current tree is a CI failure.

See `docs/attribution/README.md` for the relationship between this
inventory and per-component provenance docs.

---
```

- [ ] **Step 3: Commit**

```bash
git add docs/attribution/COMPLIANCE-INVENTORY.md
git commit -S -m "compliance: check in generated COMPLIANCE-INVENTORY.md baseline"
```

---

### Task 6 — Cross-check: every PROVENANCE-listed file exists on disk

**Severity:** MEDIUM (dead rows in PROVENANCE mislead reviewers and obscure real gaps.)

**Files:**
- Modify: `scripts/verify-provenance-sync.py` (already exists — extend it)

**Steps:**

- [ ] **Step 1: Read the existing script**

```bash
cat scripts/verify-provenance-sync.py
```

Understand what it checks today.

- [ ] **Step 2: Add "orphan-row" check**

For every path cell in `THETIS-PROVENANCE.md` table rows, verify the file exists. If not, emit an error with the row number.

Sample diff:

```python
def check_orphan_rows(md_path: Path) -> list[str]:
    errors = []
    for lineno, line in enumerate(md_path.read_text().splitlines(), 1):
        if not line.startswith("| "): continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not cells: continue
        path = cells[0]
        if not path.startswith(("src/", "tests/", "third_party/")): continue
        if not (REPO / path).exists():
            errors.append(f"{md_path.name}:{lineno}  orphan row: {path!r} not on disk")
    return errors
```

Wire into `main()` alongside existing checks.

- [ ] **Step 3: Run against current tree**

```bash
python3 scripts/verify-provenance-sync.py
```

Expected: PASS (or surface real orphans — triage in Task 10).

- [ ] **Step 4: Commit**

```bash
git add scripts/verify-provenance-sync.py
git commit -S -m "compliance: verify-provenance-sync now rejects orphan PROVENANCE rows"
```

---

## Phase 2 — Per-File Header Audit (Every Ported File)

### Task 7 — Extend `verify-thetis-headers.py` to cover AetherSDR and WDSP wrappers

**Severity:** MEDIUM (today the verifier only enforces Thetis-derived files. AetherSDR- and WDSP-wrapper files slip through.)

**Files:**
- Modify: `scripts/verify-thetis-headers.py`

**Steps:**

- [ ] **Step 1: Parameterize the marker set**

Convert the current hard-coded `REQUIRED_MARKERS` list into a per-classification table:

```python
MARKERS_BY_KIND = {
    "thetis":    ["Ported from", "Thetis", "Copyright (C)", "General Public License", "Modification history (NereusSDR)"],
    "aethersdr": ["AetherSDR", "Modification history (NereusSDR)"],
    "wdsp":      ["Copyright (C)", "General Public License"],
}
```

- [ ] **Step 2: Build the file list by classification**

Read `THETIS-PROVENANCE.md`, `aethersdr-reconciliation.md`, and `third_party/wdsp/src/*.{c,h}` to build three lists.

- [ ] **Step 3: Verify each file against its kind's markers**

- [ ] **Step 4: Run**

```bash
python3 scripts/verify-thetis-headers.py --all-kinds
```

Expected: PASS or discrete FAIL list (triage in Task 10).

- [ ] **Step 5: Commit**

```bash
git add scripts/verify-thetis-headers.py
git commit -S -m "compliance: extend header verifier to AetherSDR + WDSP scopes"
```

---

### Task 8 — Inline cite version-stamp audit

**Severity:** MEDIUM (per `feedback_inline_cite_versioning.md`: every `// From Thetis file:line` cite needs `[vX.Y.Z.W]` or `[@sha]`. PR-diff enforced today; older cites grandfathered. This task catches what slipped through.)

**Files:**
- Create: `scripts/verify-inline-cites.py`
- Create: `tests/compliance/test_inline_cites.py`

**Steps:**

- [ ] **Step 1: Write the failing test**

```python
# tests/compliance/test_inline_cites.py
import subprocess, json
from pathlib import Path
REPO = Path(__file__).resolve().parent.parent.parent

def test_every_from_thetis_cite_has_version_stamp():
    result = subprocess.run(
        ["python3", "scripts/verify-inline-cites.py", "--format=json"],
        cwd=REPO, capture_output=True, text=True
    )
    findings = json.loads(result.stdout)
    bare = [f for f in findings if f["kind"] == "missing-stamp"]
    # Grandfathered baseline allowed — capture count for drift detection
    assert len(bare) <= 0, f"{len(bare)} unstamped cites: {bare[:5]}"
```

- [ ] **Step 2: Implement `scripts/verify-inline-cites.py`**

Regex over `src/` and `tests/`:

```python
import re, sys, json, argparse
from pathlib import Path
REPO = Path(__file__).resolve().parent.parent

CITE_RE = re.compile(
    r"//\s*From\s+Thetis\s+([^\s:]+):([\d,\-\s]+)(?:\s+\[([^\]]+)\])?",
    re.IGNORECASE,
)

def scan():
    findings = []
    for p in REPO.rglob("*"):
        if not p.is_file(): continue
        if ".git" in p.parts or "build" in p.parts: continue
        if p.suffix not in (".cpp", ".h", ".hpp", ".c"): continue
        for lineno, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
            m = CITE_RE.search(line)
            if not m: continue
            stamp = m.group(3)
            rel = str(p.relative_to(REPO))
            if not stamp:
                findings.append({"path": rel, "line": lineno,
                                 "kind": "missing-stamp", "cite": line.strip()})
            elif not (stamp.startswith("v") or stamp.startswith("@")):
                findings.append({"path": rel, "line": lineno,
                                 "kind": "malformed-stamp", "cite": line.strip()})
    return findings

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--format", choices=["text", "json"], default="text")
    args = ap.parse_args()
    findings = scan()
    if args.format == "json":
        print(json.dumps(findings, indent=2))
    else:
        for f in findings:
            print(f"{f['path']}:{f['line']}  [{f['kind']}]  {f['cite']}")
    sys.exit(1 if findings else 0)

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Baseline capture**

```bash
python3 scripts/verify-inline-cites.py --format=json > /tmp/cite-baseline.json
jq 'length' /tmp/cite-baseline.json
```

Record the baseline count in the commit message; this becomes the drift detector.

- [ ] **Step 4: Update the test threshold**

Change the test assertion to the baseline value (grandfather):

```python
GRANDFATHERED_UNSTAMPED = 42  # from /tmp/cite-baseline.json
assert len(bare) <= GRANDFATHERED_UNSTAMPED
```

- [ ] **Step 5: Commit**

```bash
git add scripts/verify-inline-cites.py tests/compliance/test_inline_cites.py
git commit -S -m "compliance: audit inline // From Thetis cites for version stamps

Baseline: <N> grandfathered unstamped cites. New cites (PR diff) MUST
carry [vX.Y.Z.W] or [@shortsha]. Drift detector fails if baseline grows.

Refs: docs/attribution/HOW-TO-PORT.md §Inline cite versioning"
```

---

### Task 9 — Inline comment preservation audit (verbatim upstream markers)

**Severity:** MEDIUM (CLAUDE.md requires preserving `// MW0LGE`, `//-W2PA`, `//-VK6APH`, etc. verbatim. When ports restructure code, markers can drift. This task samples and catches it.)

**Files:**
- Create: `scripts/audit-inline-markers.py`

**Method:** For each ported file with a Thetis source cited in PROVENANCE:
1. Read the upstream source file from `../Thetis/`
2. Count occurrences of `// MW0LGE`, `//-W2PA`, `//-VK6APH`, `//-G8NJJ`, `// MW0LGE_21k5`, `// MI0BOT`, `// DH1KLM`
3. Count the same markers in the NereusSDR port
4. Report files where the downstream count is < 50% of the upstream count (threshold tunable)

**Steps:**

- [ ] **Step 1: Skeleton script**

```python
#!/usr/bin/env python3
"""Sample-compare inline-attribution markers between Thetis upstreams
and NereusSDR ports. Emit per-file ratio; flag files < 0.5 threshold.

This is a sampling audit — not every restructure preserves count 1:1.
Flagged files need human review, not automatic rejection.
"""
import re, sys, json
from pathlib import Path
REPO = Path(__file__).resolve().parent.parent
THETIS = REPO.parent / "Thetis"

MARKERS = [r"//\s*MW0LGE", r"//-\s*W2PA", r"//-\s*VK6APH",
           r"//-\s*G8NJJ", r"//\s*MI0BOT", r"//\s*DH1KLM", r"//-\s*KD5TFD"]

# Load THETIS-PROVENANCE.md, walk each port, count markers in both sides.
# ... (implementation per skeleton)
```

- [ ] **Step 2: Run sampling**

```bash
python3 scripts/audit-inline-markers.py > /tmp/marker-audit.txt
```

- [ ] **Step 3: Triage flagged files in Task 10.**

- [ ] **Step 4: Commit**

```bash
git add scripts/audit-inline-markers.py
git commit -S -m "compliance: add inline-marker preservation audit script"
```

---

### Task 10 — Triage findings from Tasks 6/7/8/9; land remediation punchlist

**Severity:** HIGH (this is where the actual compliance defects get fixed. Individual fixes will likely be small — a handful of missing markers, a few orphan PROVENANCE rows.)

**Files:**
- Create: `docs/attribution/REMEDIATION-LOG.md` (append new section dated 2026-04-18)

**Steps:**

- [ ] **Step 1: Collect findings**

Run all four auditors in sequence, save output:

```bash
python3 scripts/verify-provenance-sync.py  > /tmp/finding-orphans.txt 2>&1 || true
python3 scripts/verify-thetis-headers.py --all-kinds > /tmp/finding-headers.txt 2>&1 || true
python3 scripts/verify-inline-cites.py     > /tmp/finding-cites.txt 2>&1 || true
python3 scripts/audit-inline-markers.py    > /tmp/finding-markers.txt 2>&1 || true
```

- [ ] **Step 2: Classify each finding by severity**

  - HIGH: missing upstream copyright line, missing GPL permission block
  - MEDIUM: missing modification history block, missing inline cite version stamp
  - LOW: inline marker count drop ≥ 50%

- [ ] **Step 3: File one GPG-signed commit per severity bucket**

- [ ] **Step 4: Re-run all auditors — expect green**

- [ ] **Step 5: Append to REMEDIATION-LOG.md** a dated entry summarising the sweep, the scripts used, and the commit SHAs.

---

## Phase 3 — Vendored Code Deep-Audit

### Task 11 — WDSP file-by-file header spot-check

**Severity:** LOW (WDSP-PROVENANCE.md already claims 128/138 files have full GPLv2-or-later headers. This task verifies the claim by running a mechanical check, catches any drift since the 2026-02 sweep.)

**Files:**
- Create: `scripts/audit-wdsp-headers.py`

**Steps:**

- [ ] **Step 1: Script**

```python
#!/usr/bin/env python3
"""Walk third_party/wdsp/src/, classify each file's license header."""
import sys
from pathlib import Path
WDSP_SRC = Path(__file__).resolve().parent.parent / "third_party/wdsp/src"

def classify(text: str) -> str:
    head = text[:2000]
    if "either version 2 of the License, or" in head and "any later version" in head:
        return "gpl2-or-later"
    if "Warren Pratt" in head or "Copyright (C)" in head:
        return "copyright-no-permission-block"
    return "no-header"

rows = []
for p in sorted(WDSP_SRC.glob("*")):
    if p.suffix not in (".c", ".h"): continue
    rows.append((p.name, classify(p.read_text(errors="replace"))))

from collections import Counter
counts = Counter(r[1] for r in rows)
print(f"WDSP header classification ({sum(counts.values())} files):")
for kind, n in counts.items():
    print(f"  {kind}: {n}")

no_header = [r[0] for r in rows if r[1] == "no-header"]
if no_header:
    print("\nFiles with no header:")
    for n in no_header: print(f"  {n}")
```

- [ ] **Step 2: Run**

```bash
python3 scripts/audit-wdsp-headers.py
```

Expected: counts match WDSP-PROVENANCE.md claim (128 gpl2-or-later, 10 no-header/utility).

- [ ] **Step 3: Update WDSP-PROVENANCE.md with new verification date if counts still match; otherwise file a remediation task.**

- [ ] **Step 4: Commit**

```bash
git add scripts/audit-wdsp-headers.py docs/attribution/WDSP-PROVENANCE.md
git commit -S -m "compliance: mechanical WDSP-header audit + re-verify claim"
```

---

## Phase 4 — Binary Distribution Compliance

### Task 12 — Bundle `THIRD-PARTY-LICENSES.txt` into every release artifact

**Severity:** MEDIUM (GPLv2 §1 and GPLv3 §6 require license text to accompany the binary. Today the About dialog satisfies §5(d) interactive-notice, but the installer/zip/DMG/AppImage do not ship a separate license file.)

**Files:**
- Create: `packaging/THIRD-PARTY-LICENSES.txt.in` (template assembled at build time)
- Modify: `.github/workflows/release.yml` (three platforms)
- Modify: `packaging/nsis/NereusSDR.nsi` (Windows installer)
- Modify: `packaging/macos/*.sh` (DMG)
- Modify: `packaging/linux/AppImage.sh` (AppImage)

**Steps:**

- [ ] **Step 1: Assemble the template**

Concatenate at build time:
- NereusSDR LICENSE (GPLv3)
- LICENSE-DUAL-LICENSING
- third_party/wdsp/COPYING (GPLv2, from Task 1)
- third_party/fftw3/COPYING (GPLv2, from Task 3)
- Qt6 LGPL notice (link + text pointer to qt.io/licensing)
- Full contributor chain reference to docs/attribution/

Add a small CMake target that builds `THIRD-PARTY-LICENSES.txt` from these sources.

- [ ] **Step 2: Wire into each packager**

- Windows NSIS: install `$INSTDIR\THIRD-PARTY-LICENSES.txt`
- Windows portable ZIP: include at zip root
- macOS DMG: install into `NereusSDR.app/Contents/Resources/`
- Linux AppImage: include at AppImage root

- [ ] **Step 3: Test one local release**

```bash
gh workflow run release.yml --ref compliance/full-audit-2026-04-18
```

Wait for artifacts, extract, confirm the file is present.

- [ ] **Step 4: Commit**

```bash
git add packaging/ .github/workflows/release.yml
git commit -S -m "release: bundle THIRD-PARTY-LICENSES.txt in all release artifacts

Satisfies GPLv2 §1 / GPLv3 §6 accompany-the-binary obligation.
Assembled at build time from root LICENSE + LICENSE-DUAL-LICENSING +
third_party/wdsp/COPYING + third_party/fftw3/COPYING + Qt6 LGPL notice."
```

---

### Task 13 — Release-notes "written offer equivalent" (§6(d))

**Severity:** LOW (GPLv3 §6(d) is satisfied by hosting source on the same download page. We already do this via GitHub Releases. This task adds a single paragraph making the obligation explicit for auditors.)

**Files:**
- Modify: `.github/release-template.md` (if exists; else in `release.yml`)
- Modify: `packaging/release-body.sh`

**Steps:**

- [ ] **Step 1: Add a "Source & Licence" section**

```markdown
## Source & Licence

NereusSDR is distributed under **GPLv3** (see [LICENSE](LICENSE)). This
release bundles **WDSP** (GPL-2.0-or-later, © Warren Pratt NR0V) and
**FFTW3** (GPL-2.0-or-later, © Matteo Frigo & Steven G. Johnson / MIT).
The combined work is conveyed under GPLv3 via GPLv3 §5(b).

**Corresponding source** for this release is the public git tree at the
**[commit tagged `v{VERSION}`](https://github.com/boydsoftprez/NereusSDR/tree/v{VERSION})**.
GPLv3 §6(d) is satisfied by hosting source adjacent to the binary on
this same Releases page.
```

- [ ] **Step 2: Wire into release.yml so every release auto-includes the block.**

- [ ] **Step 3: Commit**

```bash
git add .github/release-template.md
git commit -S -m "release: add Source & Licence §6(d) block to release body"
```

---

### Task 14 — About dialog §5(d) copyright-line sync with current PROVENANCE

**Severity:** LOW (the About dialog's copyright line names principals. As PROVENANCE grows, principals may be missed. Audit at release time.)

**Files:**
- Modify: `src/gui/AboutDialog.cpp:232-244`

**Steps:**

- [ ] **Step 1: Dump the distinct copyright-holder names from THETIS-PROVENANCE.md**

```bash
python3 scripts/list-copyright-holders.py   # new script, trivial
```

- [ ] **Step 2: Diff against the AboutDialog.cpp name list. Add any missing principal.**

- [ ] **Step 3: Build and eyeball the dialog**

```bash
cmake --build build -j$(nproc) && ./build/NereusSDR
```

- [ ] **Step 4: Commit if changes needed**

```bash
git add src/gui/AboutDialog.cpp
git commit -S -m "compliance: refresh About dialog §5(d) copyright-holder list"
```

---

## Phase 5 — CI Enforcement (Prevent Regression)

### Task 15 — Wire all auditors into `.github/workflows/ci.yml` as merge gates

**Severity:** HIGH (without CI enforcement, the audits drift the moment the next PR lands.)

**Files:**
- Modify: `.github/workflows/ci.yml`

**Steps:**

- [ ] **Step 1: Add a `compliance` job**

```yaml
  compliance:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.12' }
      - run: pip install pytest
      - run: python3 scripts/verify-thetis-headers.py --all-kinds
      - run: python3 scripts/verify-provenance-sync.py
      - run: python3 scripts/verify-inline-cites.py
      - run: python3 scripts/check-new-ports.py
      - run: python3 scripts/compliance-inventory.py --fail-on-unclassified
      - run: pytest tests/compliance/ -v
```

- [ ] **Step 2: Mark the job required in branch protection** (manual, via GitHub settings — note in PR body)

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -S -m "ci: add compliance job — all auditors merge-gated"
```

---

### Task 16 — Pre-commit hook parity with CI

**Severity:** LOW (local hook already installed via `scripts/install-hooks.sh` per CLAUDE.md. Ensure it runs the new auditors from Tasks 4/8/9.)

**Files:**
- Modify: `scripts/git-hooks/pre-push` (or `pre-commit`, per current hook setup)
- Modify: `scripts/install-hooks.sh`

**Steps:**

- [ ] **Step 1: Add the three new script calls**

Append:

```bash
python3 scripts/verify-inline-cites.py || exit 1
python3 scripts/compliance-inventory.py --fail-on-unclassified || exit 1
```

- [ ] **Step 2: Test the hook locally**

```bash
bash scripts/install-hooks.sh
git commit --allow-empty -m "hook test"
```

Expected: hook runs and passes.

- [ ] **Step 3: Commit**

```bash
git add scripts/git-hooks/ scripts/install-hooks.sh
git commit -S -m "compliance: pre-push hook now runs all compliance auditors"
```

---

## Phase 6 — Cadence & Re-Scan Protocol

### Task 17 — Document the "pull Thetis upstream" procedure

**Severity:** LOW (Thetis ships new releases; our inline cite stamps need refreshing. Document the procedure so it doesn't get skipped.)

**Files:**
- Create: `docs/attribution/UPSTREAM-SYNC-PROTOCOL.md`

**Content outline:**

1. When to do it: before cutting any minor release, or when Thetis ships a new tag.
2. How:
   - `git -C ../Thetis fetch --tags`
   - `git -C ../Thetis describe --tags` — new version stamp
   - Re-run `scripts/verify-inline-cites.py` to surface stale stamps
   - For each ported file, diff upstream against NereusSDR port
   - For drift, either pull the upstream change + refresh stamps OR document the intentional divergence in source file + PROVENANCE notes column
3. Close with a REMEDIATION-LOG entry.

- [ ] **Step 1: Write the doc per outline.**

- [ ] **Step 2: Link from `CONTRIBUTING.md` and `docs/attribution/README.md`.**

- [ ] **Step 3: Commit**

```bash
git add docs/attribution/UPSTREAM-SYNC-PROTOCOL.md CONTRIBUTING.md docs/attribution/README.md
git commit -S -m "docs: document upstream (Thetis/WDSP) sync and stamp-refresh protocol"
```

---

## Rollup — Final Verification & Release

### Task 18 — Green-field verification run

- [ ] **Step 1:** Fresh clone in `/tmp/`, run every auditor from scratch.
- [ ] **Step 2:** Build all three target platforms, extract release artifacts, confirm THIRD-PARTY-LICENSES.txt present.
- [ ] **Step 3:** Manually open About dialog, confirm §5(d) notice + copyright-holder list matches current PROVENANCE.
- [ ] **Step 4:** Tag the audit sweep in REMEDIATION-LOG.md.

### Task 19 — Merge & cut patch release

- [ ] **Step 1:** Open PR `compliance/full-audit-2026-04-18` → `main`.
- [ ] **Step 2:** Title: *compliance: full-tree GPL audit (every file, every line) — 2026-04-18*.
- [ ] **Step 3:** Body summarises each phase and links to REMEDIATION-LOG entry.
- [ ] **Step 4:** After merge, run `/release patch` to cut a compliance-only patch release (bumps version, publishes binaries with bundled THIRD-PARTY-LICENSES.txt).

---

## Self-Review Notes

**Spec coverage (user's "every line every file from origin" ask):**

| Requirement | Covered by |
|---|---|
| Classify every file | Task 4/5 (inventory) |
| Verify every ported file's header | Task 7 (extended verifier) |
| Verify every inline cite | Task 8 (inline cite audit) |
| Verify inline-marker preservation | Task 9 (marker audit) |
| Audit vendored trees | Tasks 1/2/3/11 |
| Fix known gaps | Tasks 1/2/3 |
| Compliance for binaries | Tasks 12/13/14 |
| Prevent regression | Tasks 15/16 |
| Periodic re-scan | Task 17 |

**Non-coverage (acceptable):** This plan does not re-port any file. It audits and documents. Actual content remediation (restoring a missed MW0LGE tag, adding a missing Samphire dual-license block) lives under Task 10's bucket — the punchlist sized after findings are collected.

**Risks:**

1. Task 9's 50% threshold may flag false positives (restructured ports legitimately have fewer inline markers). Mitigation: sampling audit, human review required.
2. Task 12 requires testing each platform's release pipeline. Mitigation: do the wiring in a feature branch, trigger release.yml against the branch, do not tag until all three platforms verified.
3. Task 15 will likely fail on first CI run — surfacing the real findings. That's the point. Expect 1-3 follow-up commits before the gate goes green.

---

## Execution Handoff

Plan complete and saved to `docs/architecture/2026-04-18-gpl-compliance-full-audit-plan.md`.

**Two execution options:**

1. **Subagent-Driven (recommended for this size)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Best for 19-task plans.
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
