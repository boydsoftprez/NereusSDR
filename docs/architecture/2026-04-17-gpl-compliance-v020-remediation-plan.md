# GPL Compliance v0.2.0 — Final Remediation Plan

> **Source:** Adversarial GPL compliance review run 2026-04-17 against the
> `compliance/v0.2.0-remediation` branch. This plan addresses the 14 findings
> rated MEDIUM or higher; LOW-severity items are batched into Task 13.

**Goal:** Close every defensible GPL/LGPL gap an adversarial reviewer (incl.
Richard Samphire MW0LGE) could escalate, before tagging v0.2.0.

**Architecture:** Walk the findings in dependency order — fix the source-tree
defects first (file headers, missing attributions) so the next CI verifier
run is clean, then fix the distribution-side defects (release notes, About
dialog, third-party docs), then a batched cleanup pass for the LOW items.

**Tech stack:** No new code beyond a few `qInfo()` lines and a
`release.yml` step; the bulk is text edits to source-file headers and
attribution docs.

**Workflow:** I will ask you per task: **implement / modify / skip**. Tasks
are ordered by severity-then-cost. Each task is independent and gets its own
GPG-signed commit.

---

## Task 1 — H4: Restore MainWindow.{h,cpp} attribution headers

**Severity:** HIGH (single most visible defect — most-viewed GUI file in the
tree, currently ships with zero copyright/attribution block; explicit
Bucket D.2 deferral).

**Files:**
- Modify: `src/gui/MainWindow.h`
- Modify: `src/gui/MainWindow.cpp`
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (add MainWindow rows)
- Modify: `docs/attribution/aethersdr-reconciliation.md` (move MainWindow
  out of Bucket D.2 into the resolved set)

**Change:** Prepend the standard NereusSDR multi-source header per
`HOW-TO-PORT.md`. Stack two verbatim upstream blocks: (1) Thetis
`Project Files/Source/Console/console.cs` Samphire-variant header, then
(2) AetherSDR `MainWindow.{h,cpp}` header. Add a `Modification history
(NereusSDR)` block dated 2026-04-17 / J.J. Boyd KG4VCF / Claude Opus.

**Verify:** `python3 scripts/verify-thetis-headers.py` passes; build
succeeds; About dialog still loads.

**Commit:** `fix(compliance): restore MainWindow attribution header (Bucket D.2)`

---

## Task 2 — H2 + H3: Fix orphan .cpp + missed-source-citation pattern (one pass)

**Severity:** HIGH (this is the exact MeterManager defect pattern Samphire
caught last time, repeating one layer down).

**Files:**
- Modify: `src/core/P1RadioConnection.cpp` (header stack — add `bandwidth_monitor.c`,
  `bandwidth_monitor.h`, `IoBoardHl2.cs` verbatim blocks)
- Modify (add headers): `src/core/mmio/ExternalVariableEngine.cpp`,
  `src/models/RxDspWorker.cpp`,
  `src/gui/containers/meter_property_editors/NeedleItemEditor.cpp`,
  `src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.cpp`,
  `src/gui/containers/meter_property_editors/BaseItemEditor.cpp`,
  `src/gui/containers/MmioEndpointsDialog.cpp`,
  `src/gui/setup/hardware/BandwidthMonitorTab.cpp`,
  `src/gui/setup/hardware/BandwidthMonitorTab.h`
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (add 9 rows; expand row 50
  source column to list mi0bot files explicitly)

**Change:** For each orphan .cpp, prepend the same verbatim header its `.h`
sibling already carries. For `P1RadioConnection.cpp`, add the three missed
upstream verbatim blocks (Samphire `bandwidth_monitor.{c,h}`, Campbell
`IoBoardHl2.cs`) to the existing stack. For `BandwidthMonitorTab.{cpp,h}`,
this is net-new attribution — they currently have only a plain
`// Source: ChannelMaster/bandwidth_monitor.h` comment which is an admission,
not a license header.

**Verify:** verifier passes; `git grep -l "bandwidth_monitor" src/` shows
every match has a real attribution block above it; build succeeds.

**Commit:** `fix(compliance): close header-on-.h-but-not-.cpp gap (8 files)`

---

## Task 3 — H1: Reconcile shader attribution self-contradiction

**Severity:** HIGH (worst-shape inconsistency — code comment admits AetherSDR
origin while attribution doc denies any external shader source; a reviewer
will quote both side-by-side).

**Files:**
- Modify: `resources/shaders/waterfall.frag` (add NereusSDR port-citation
  block since GLSL has no upstream header to copy verbatim)
- Modify: `docs/attribution/ASSETS.md` (rewrite the "All GLSL shaders are
  original NereusSDR artwork" section to disclose the AetherSDR-derived
  shader, GPLv3 compatible)

**Change:** Treat per HOW-TO-PORT.md rule 6 (project-level attribution for
no-header upstream): cite `ten9876/AetherSDR/.../texturedquad.frag` in a
shader-top comment. ASSETS.md gets a new paragraph naming the derived
shader and noting the rest are original.

**Verify:** `git grep "From AetherSDR" resources/shaders/` shows the
inline citation now sits above a real attribution block, not contradicting
ASSETS.md.

**Commit:** `fix(compliance): cite AetherSDR origin of waterfall shader`

---

## Task 4 — H5: Add missing copyright holders to About dialog

**Severity:** HIGH (notices preserved in file headers but stripped from the
interactive notice — strict GPLv3 §5(a) violation).

**Files:**
- Modify: `src/gui/AboutDialog.cpp` (the copyright string at lines ~210-215)

**Change:** Add **Bill Tracey (KD5TFD)** and **DH1KLM** to the named
copyright holders. Sweep `git grep -i "copyright (c)" src/` for any other
inline-credited contributor not yet in the dialog and add them too.

**Verify:** Diff the dialog copyright list against
`grep -RhE "Copyright \(C\)" src/ | sort -u`; every name in source headers
should appear in the dialog.

**Commit:** `fix(compliance): name KD5TFD + DH1KLM in About dialog copyright`

---

## Task 5 — H6: Ship GPLv2 §3 corresponding-source offer for FFTW3 Windows binary

**Severity:** HIGH (Windows installer ships `libfftw3f-3.dll` which is
GPLv2-or-later; release artifacts and notes neither include FFTW source
nor carry a §6(b)-compliant written offer).

**Files:**
- Modify: `.github/workflows/release.yml` (add a Windows-job step that
  downloads `fftw-3.3.5.tar.gz` from fftw.org and includes it in the
  installer payload OR appends it to the SHA256SUMS-tracked release assets)
- Modify: `.github/release-notes-template.md` (or wherever the release notes
  are generated — see release.yml; add a "Source for bundled binaries"
  section with FFTW3 + Qt LGPL written offer, 3-year validity, contact
  email per §6(b))

**Change:** Recommended path: append `fftw-3.3.5.tar.gz` to the release
assets list (cheapest, satisfies §6(a) directly). Plus an explicit
"Source for Qt6 (LGPL): https://download.qt.io/archive/qt/6.x.y/" line
in the notes for the dynamically-linked Qt obligation.

**Verify:** Build a v0.2.0-rc Windows installer locally and confirm
`fftw-3.3.5.tar.gz` is in the release directory listing. Diff
release-notes output against a §6 checklist.

**Commit:** `fix(compliance): provide §6 source offer for bundled FFTW3 + Qt6`

---

## Task 6 — M1: Disambiguate LICENSE-DUAL-LICENSING after GPLv3 relicense

**Severity:** MEDIUM (legal-textual — Samphire's reservation refers to
"LICENCE" which used to be GPLv2; now it's GPLv3, creating a hostile-reading
attack surface).

**Files:**
- Modify: `LICENSE-DUAL-LICENSING` (prepend a NereusSDR-specific preamble,
  leave the byte-identical upstream block intact below)

**Change:** Add a 2-3 line preamble: *"In NereusSDR, the reference to
'LICENCE' in the statement below refers to this project's root `LICENSE`
file, which contains GPLv3. NereusSDR elected GPLv3 under the
'or later' grant in Thetis source-file headers. Samphire's dual-licensing
reservation continues to apply to his original C# code; NereusSDR
contributors' C++ ports of that code are licensed under GPLv3 from the
NereusSDR side."* Then mark the original block as **`Upstream verbatim
text from ramdor/Thetis (preserved unchanged):`** and leave it untouched.

**Verify:** Byte-compare the upstream block against `ramdor/Thetis/master`
remains identical (preamble is additive, not a modification of upstream).

**Commit:** `fix(compliance): preamble LICENSE-DUAL-LICENSING for GPLv3 election`

---

## Task 7 — M2: Add startup §5(d) interactive notice

**Severity:** MEDIUM (About dialog buried two clicks deep under Help menu;
strict GPLv3 §5(d) wants visible-on-startup).

**Files:**
- Modify: `src/main.cpp` (or `MainWindow` constructor) — emit unconditional
  `qInfo()` banner at startup
- Modify: `src/gui/MainWindow.cpp` — show AboutDialog on first run, gated
  by `AppSettings` key `FirstRunAboutShown` (set to "True" after dismiss)

**Change:** The startup banner should print on stderr/stdout regardless of
GUI mode (compliant with §5(d) "interactive interfaces" — terminal
attached or not):

> NereusSDR vX.Y.Z  Copyright (C) ...
> This program comes with ABSOLUTELY NO WARRANTY; for details see
> Help → About or sections 15-16 of LICENSE.
> This is free software, and you are welcome to redistribute it under
> the GNU General Public License v3.

First-run About auto-popup is the belt-and-suspenders measure. Persist via
existing `AppSettings` (NOT QSettings — see CLAUDE.md).

**Verify:** Launch fresh build, confirm banner prints; delete
`~/.config/NereusSDR/NereusSDR.settings`, relaunch, confirm About appears.

**Commit:** `feat(compliance): startup license banner + first-run About dialog`

---

## Task 8 — M3: Add LGPLv3 §4(d)(1) relinking instructions for Qt6

**Severity:** MEDIUM (Qt is dynamically linked everywhere — satisfies
§4(d)(0) — but the §4(d)(1) "instructions for replacing" requirement isn't
met by the current `qt6.txt` content).

**Files:**
- Modify: `packaging/third-party-licenses/qt6.txt`

**Change:** Add a "How to replace Qt6 with a modified version" paragraph
covering all three platforms: replace `Qt6*.dll` (Windows install dir),
`Qt*.framework` or `Qt6*.dylib` (macOS app bundle Frameworks/), or
`libQt6*.so.6` (Linux AppImage `usr/lib/`). One-paragraph treatment is
sufficient for §4(d)(1).

**Verify:** `cat packaging/third-party-licenses/qt6.txt` shows the new
section; diff against an LGPLv3 §4 checklist.

**Commit:** `fix(compliance): document Qt6 LGPLv3 §4(d)(1) replacement steps`

---

## Task 9 — M4: Replace duplicate cross-needle background

**Severity:** MEDIUM (`cross-needle-bg.png` is byte-identical MD5 to
`cross-needle.png`; honestly disclosed in ASSETS.md but undermines the
"original layered artwork" claim).

**Files:**
- Either modify: `resources/meters/cross-needle-bg.png` (replace with a
  real background layer — solid-color or blurred backdrop)
- Or modify: `resources.qrc` + the renderer to use a programmatic solid
  fill for layer 0

**Change:** **Recommendation:** programmatic solid-fill is cheaper and
removes a binary asset entirely. The renderer would draw a flat-color
rectangle behind the foreground PNG. Reduces the asset audit surface.
Alternative: produce a real background image, ASSETS.md gets updated to
describe it.

**Verify:** Open the meter widget in-app, confirm cross-needle still
renders correctly. `md5 resources/meters/*.png` shows distinct hashes.

**Commit:** `fix(compliance): replace duplicate cross-needle background asset`

---

## Task 10 — M5: Link Third-Party Notices from About dialog

**Severity:** MEDIUM (Built-With section names Qt/FFTW/WDSP without
pointing recipients at `packaging/third-party-licenses/`).

**Files:**
- Modify: `src/gui/AboutDialog.cpp`
- Modify: CMake/install rules (verify `packaging/third-party-licenses/`
  ships into the install prefix at runtime-discoverable location)

**Change:** Add a "Third-Party Notices" link or section to the About
dialog footer, pointing to the bundled `third-party-licenses/` directory
(or open it via `QDesktopServices::openUrl(QUrl::fromLocalFile(...))`).

**Verify:** Click the link in a fresh build, confirm directory opens.

**Commit:** `fix(compliance): expose Third-Party Notices link in About dialog`

---

## Task 11 — M6: Add AetherSDR citation to AboutDialog.cpp itself

**Severity:** MEDIUM (Bucket D.1 deferred — the dialog displays AetherSDR
contributors but the file's own header doesn't cite AetherSDR as the
layout source).

**Files:**
- Modify: `src/gui/AboutDialog.cpp` (add port-citation block per Task 1
  pattern — AetherSDR `AboutDialog.{h,cpp}` as the layout source)

**Change:** Standard NereusSDR port-citation block; AetherSDR has no
verbatim header so use HOW-TO-PORT.md rule 6 (project-level citation).

**Verify:** verifier passes; About dialog still loads identically.

**Commit:** `fix(compliance): cite AetherSDR layout origin in AboutDialog`

---

## Task 12 — Pre-release verifier hardening

**Severity:** PROCESS (the current verifier accepted all 8 orphan-.cpp
files; without hardening, the same defect class will recur).

**Files:**
- Modify: `scripts/verify-thetis-headers.py`

**Change:** Two enhancements: (1) for every `.h` listed in PROVENANCE,
require its sibling `.cpp` to either be listed in PROVENANCE too OR
carry a `// Independently implemented from <X>.h interface` comment;
(2) parse the file's contributor list and warn if PROVENANCE variant
is `thetis-samphire` but the file's copyright lines don't include
`MW0LGE`.

**Verify:** Run against current tree post-Task-2; should now pass clean.
Run against pre-Task-2 tree; should flag the 8 orphans.

**Commit:** `chore(compliance): harden verifier against .h/.cpp orphan pattern`

---

## Task 13 — LOW-severity batch cleanup

**Severity:** LOW (eight items batched; none individually justify a
release block but all are easy and reduce audit surface).

**Files:** Various — see per-item below.

**Items:**
- **L1:** Verifier hardening already covered in Task 12.
- **L2:** `docs/attribution/WDSP-PROVENANCE.md:27` — change `fftw3.h` license
  label from BSD to GPLv2-or-later.
- **L3:** `src/gui/AboutDialog.cpp` warranty line — append "see sections
  15-16 of the License for details."
- **L4:** `docs/attribution/ASSETS.md` — add `resources/help/getting-started.md`
  entry under a new `## resources/help/` section.
- **L5:** `src/gui/AboutDialog.cpp:195-207` comment block — rewrite to
  reference GPLv3 §5(d) explicitly (not "GPLv2 §2(c) holdover").
- **L6:** `xattr -c resources/meters/ananMM.png` and recommit.
- **L7:** Ship `docs/attribution/LICENSE-GPLv2` (verbatim GPLv2 text from
  gnu.org) since WDSP files reference v2; add a one-line README note about
  the GPLv3 election under the upstream "or later" grant.
- **L8:** Reword `CONTRIBUTING.md:44, 144` to scope GPG-signing to "this
  repository's `main` branch" (project policy, not §7 contribution
  prerequisite). Adjust corresponding line in `CLAUDE.md` if it's stricter.

**Verify:** Each item is a one-or-two-line change; visual diff review.

**Commit:** `chore(compliance): batch cleanup of 8 low-severity items`

---

## Task 14 — Coordinate with Samphire before tagging v0.2.0

**Severity:** PROCESS (per the offline agreement — "circle back with
Richie before any new binaries ship").

**Files:** None — outbound communication.

**Change:** Email Samphire summarizing the remediation: link to this plan,
the resulting commits, the post-Task-12 verifier output, the new About
dialog screenshot, and the v0.2.0-rc Windows installer with FFTW source
included. Ask for sign-off (or specific objections) before `/release`.

**Verify:** Reply received; objections (if any) added as new tasks.

**Commit:** N/A (process step). Log in `docs/attribution/REMEDIATION-LOG.md`.

---

## Self-review

- **Spec coverage:** All 6 HIGH and 6 MEDIUM findings → Tasks 1-11. The
  three LOW items rolled into Task 13. Process items (verifier hardening,
  Samphire sign-off) → Tasks 12 and 14. Nothing dropped.
- **Type/symbol consistency:** Tasks reference real file paths; verifier
  script name is consistent.
- **Placeholder scan:** No "TBD" / "appropriate error handling" / etc.
- **One open question:** Task 9 has two paths (replace asset vs. solid-fill).
  Recommendation provided (solid-fill); will ask at decision time.

---

## Execution mode

Per your request: walk one task at a time, ask **implement / modify / skip**
per task. Each implemented task is its own GPG-signed commit on the
`compliance/v0.2.0-remediation` branch. After Task 14, the branch is
ready to merge to `main` and tag v0.2.0.
