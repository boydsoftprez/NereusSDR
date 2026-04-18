# GPL Compliance Remediation — NereusSDR v0.2.0 Design

**Date:** 2026-04-16
**Author:** J.J. Boyd (KG4VCF)
**Status:** Draft — pending user review
**Target release:** v0.2.0 (compliance reset)

---

## 1. Context

On 2026-04-16, shortly after NereusSDR v0.1.7 was announced to the Thetis
Facebook user group, **Richard Samphire (MW0LGE / "Ramdor" / ramdor)** — a
principal Thetis contributor and the author of the Thetis dual-licensing
statement — contacted the maintainer (J.J. Boyd, KG4VCF) via Discord with
serious GPL compliance concerns about NereusSDR.

Richie's concerns, in summary:

1. NereusSDR is unambiguously a derivative work of Thetis — it contains
   direct line-level citations to Thetis source files, preserves Thetis
   constants verbatim, and follows Thetis implementation patterns (the
   project's own CLAUDE.md source-first protocol explicitly mandates this).
2. Despite being a derivative work, NereusSDR ported files do **not**
   preserve the original Thetis per-file copyright notices, GPL permission
   blocks, contributor attribution, or the Samphire dual-licensing statement.
3. A root-level `LICENSE` file naming GPLv3 does not substitute for
   per-file notice preservation required under GPL.
4. Public distribution of v0.1.x alpha binaries occurred while the source
   tree was non-compliant; under GPL §12 the prior non-compliant binaries
   cannot be retrospectively cured.
5. A draft formal legal notice (hosted at a Google Docs URL) exists and
   reserves rights including GPL enforcement, DMCA takedowns, platform
   notifications, and further legal escalation.
6. A separate concern was raised about `resources/meters/ananMM.png` — the
   ANAN multimeter face appears to reproduce an original Ernst OE3IDE
   asset (matching fonts, 100% scale, watermark removed). Richie had
   personally asked Ernst to add that watermark during original Thetis
   meter development.

The Discord exchange was collegial. Richie explicitly stated: *"I love the
idea of someone taking it forward and making it modern, but only if the
history and attribution of the developers is included in the new source."*
He proposed a 6-step remedy, which the maintainer accepted in writing. He
is awaiting input from the broader Thetis / OpenHPSDR developer group
before deciding on any escalation. He stated he will verify whether the
repository is corrected by checking it publicly.

This design document captures the remediation plan the maintainer has
committed to. It is intended to produce a source tree that is defensibly
GPL-compliant under any reasonable review — Richie's personally, the
OpenHPSDR developer community's, or a court's.

### Agreed 6-step remedy (verbatim from Richie)

> 1. Pull the currently distributed binaries and pause further public distribution for now.
> 2. Audit the codebase thoroughly for all files and implementations derived from, translated from, mapped from, or materially based on Thetis.
> 3. In each corresponding derived file, or where any Thetis code is directly referenced, restore and preserve the relevant original copyright notices, GPL notices, attribution, dual licensing statements where applicable, and provenance context from the Thetis source it was derived from.
> 4. Add clear modification notes showing that the file was reworked, ported, translated, or refactored, including by AI tooling where applicable.
> 5. Rebuild and only then re-release once the source tree is in proper compliance.
> 6. Make a short public note that earlier public builds were withdrawn pending some important fixes.

The maintainer also committed to showing Richie a diff summary before any
v0.2.0 binary ships.

---

## 2. Goals and Non-goals

**Goals:**

- Satisfy all six items of Richie's agreed remedy.
- Produce a file-by-file provenance inventory that is trivial for any third
  party (Richie, OpenHPSDR community, GPL enforcement attorney) to verify.
- Prevent recurrence by updating the project's own source-first protocol so
  the AI tooling cannot re-introduce the same defect on future ports.
- Re-frame the project's public positioning (README, release descriptions)
  to accurately reflect that NereusSDR is a port of Thetis within the
  OpenHPSDR/FlexRadio lineage, not an "independent" implementation.
- Replace the `ananMM.png` artwork with original content that carries no
  OE3IDE provenance.
- Ship v0.2.0 as a clean compliance cut after pre-release courtesy review
  by Richie.

**Non-goals:**

- Re-licensing NereusSDR. It stays GPLv3, which is compatible with Thetis's
  GPLv2-or-later terms. No license change required.
- Changing project architecture, feature set, or implementation.
- Re-headering `third_party/wdsp/` (Richie's §6.1 explicitly notes WDSP
  attribution is already properly preserved) or `third_party/fftw3/`.
- Admitting legal liability in public communications. The public withdrawal
  note remains vague — Richie wrote that wording himself and agreed to it.
- Attempting to cure the v0.1.x binaries. They remain permanently drafted;
  a compliant tree produces v0.2.0 binaries as fresh artifacts.

---

## 3. Workstream overview

| # | Workstream | Gate | Deliverable |
|---|---|---|---|
| A | Immediate actions (binary withdrawal + notices) | Tonight | Releases drafted, public note posted, Discord ack to Richie, release workflow frozen |
| B | Bi-directional audit | Before C | `docs/attribution/THETIS-PROVENANCE.md` |
| C | Header restoration + modification notes | Before E | Per-file headers on every inventoried file; `docs/attribution/HEADER-TEMPLATES.md` |
| D | ANAN artwork replacement + asset provenance audit | Parallel to C | Original `ananMM.png`, audited cross-needle assets, `docs/attribution/ASSETS.md` |
| E | README + CLAUDE.md re-framing | Before F | README opening/footer updated; CLAUDE.md source-first license-preservation rule added; `docs/readme-independent-framing` branch deleted |
| F | v0.2.0 rebuild + Richie courtesy review | Before G | Compliance branch merged to main; pre-release bundle sent to Richie |
| G | v0.2.0 public release | After F + quiet period | Binaries + release notes + collaboration credit (if accepted) |

Workstreams are gated strictly: the binary build for G does not run until F
clears. In-flight feature PRs are frozen from merge until the compliance
branch lands.

---

## 4. Workstream A — Immediate actions

Must complete tonight, regardless of downstream workstream progress.

### A1. Withdraw released binaries

For each of: `v0.1.1`, `v0.1.2`, `v0.1.4`, `v0.1.5`, `v0.1.6`, `v0.1.7-rc1`,
`v0.1.7` — convert the GitHub Release to **draft**:

```bash
gh release edit v0.1.1 --draft
# ... repeat for each tag
```

Rationale: draft removes binaries from public download pages but preserves
git tags, changelog text, and release URLs. Deletion would break
`git describe` and look evasive. The tags stay; only the asset availability
changes.

### A2. Public withdrawal note

Posted as a pinned GitHub issue titled "NereusSDR alpha builds withdrawn —
compliance housekeeping" and cross-posted to the Thetis Facebook user group
thread where v0.1.7 was announced. Body (using Richie's agreed phrasing):

> **NereusSDR alpha builds withdrawn pending compliance housekeeping**
>
> All v0.1.x alpha binaries have been withdrawn while we review some licensing
> and attribution issues that need to be addressed before further public
> distribution. If you've installed a v0.1.x build, please uninstall it. A
> compliant v0.2.0 will follow once the work is complete.
>
> — J.J. Boyd ~ KG4VCF

### A3. Private Discord acknowledgment to Richie

Short reply on the existing Discord thread (not a new formal letter — channel
is already warm):

> Richie — thanks again for flagging this so clearly. I'm working from your
> 6-point roadmap as the plan of record. Tonight I'm pulling the v0.1.x
> binaries to draft and posting the withdrawal note. Next couple of weeks:
> full bi-directional audit, header restoration with modification notes,
> original artwork for the meter face, and a re-framed README that matches
> the port-of-Thetis reality. I'll keep you posted as milestones land, and —
> per my earlier note — I'd like to share a pre-release diff summary with
> you before any v0.2.0 binary goes out. Appreciate the patience and the
> collegial handling. — JJ

### A4. Freeze release workflow

Edit `.github/workflows/release.yml` to comment out the `on.push.tags`
trigger (or gate it on a feature-flag secret). Single-line edit, committed
on `main` directly so no one can accidentally trigger a release build during
remediation.

---

## 5. Workstream B — Bi-directional audit

The 93 NereusSDR files that currently self-identify with a `// From Thetis`
or `// Porting from Thetis` comment are a **floor**, not a ceiling. Richie's
`MeterManager.cs:1049` example demonstrates that derivations exist in files
that do not currently declare them. The audit must run in both directions.

### B1. Outbound sweep (self-declared derivations)

Grep the tree for every reference to Thetis upstream:

```bash
# Catch common declaration patterns
grep -rlnE "From Thetis|Porting from|Thetis Project Files|mi0bot|MW0LGE|W5WC|clsHardwareSpecific|MeterManager\.cs|console\.cs|setup\.cs|NetworkIO\.cs|cmaster\.cs|display\.cs" src/ tests/
```

For each matching file:

1. Extract every Thetis source path and line range cited.
2. Read the referenced Thetis source file(s); capture their copyright block(s).
3. Record the NereusSDR file → Thetis source mapping in the provenance table
   with derivation type (`port` / `reference` / `structural` / `wrapper`).

### B2. Inbound sweep (undeclared derivations)

For each Thetis source file appearing in the outbound sweep (plus any other
Thetis file whose name surfaces in project docs — `cmaster.cs`, `RXA.c`,
`TXA.c`, `dsp.cs`, `protocol2.cs`, `display.cs`, `audio.cs`, `clsHardwareSpecific.cs`,
`clsRadioDiscovery.cs`, `MeterManager.cs`, etc.) — search NereusSDR for
structural echoes that do not currently cite it:

- Port-derived constants preserved verbatim per the source-first protocol
  (magic numbers like `0.98f`, specific buffer sizes, protocol byte offsets)
- Function/class names translated straightforwardly from C# (e.g.
  `SetRxADC` → `setRxAdc`, `UpdateDDCs` → `updateDDCs`)
- Protocol wire-format byte layouts
- WDSP API call sequences preserved from a specific Thetis callsite

Each inbound-sweep finding is either:

- **Added to the provenance inventory** (a derivation we missed), and gets
  a full header block per §6, or
- **Justified in writing** in a dedicated section of `THETIS-PROVENANCE.md`
  titled "Independently implemented — Thetis-like but not derived" with a
  brief explanation (e.g. "independent parser following OpenHPSDR protocol
  spec; no Thetis code consulted").

### B3. Upstream classification

Every derivation must be attributed to one of three upstreams with its
correct copyright block:

- **`ramdor/Thetis`** (primary Thetis upstream — GPLv2-or-later).
  Contributors to preserve: FlexRadio Systems (2004–2009), Doug Wigley W5WC
  (2010–2020), Richard Samphire MW0LGE (2019–2026 — with dual-license
  carve-out), and per-file named contributors where present (W2PA / Codella,
  G8NJJ / Barker, N1GP / Koch, W4WMT / Rambo, W5SD / Allen, WD5Y / Torrey,
  M0YGG / Mansfield).
- **`mi0bot/Thetis-HL2`** (Hermes-Lite fork — separate upstream).
  Contributor to preserve: Reid Campbell (MI0BOT) plus the ramdor/Thetis
  chain it descends from.
- **`TAPR/OpenHPSDR-wdsp`** (WDSP C source). Contributor: Warren Pratt
  (NR0V). Already cleanly vendored in `third_party/wdsp/` — no action.

### B4. Output artifact

All audit output lands in a single file: `docs/attribution/THETIS-PROVENANCE.md`
(structure detailed in §10).

Expected inventory size: 100–135 files after both sweeps close.

---

## 6. Workstream C — Header restoration and modification notes

### C1. Header template

Every inventoried derived file receives a block inserted immediately after
any `#pragma once` / include-guard and before the primary `#include`
statements. The template varies by upstream; the canonical `ramdor/Thetis`
form (for a file with Samphire contributions in the Thetis source) is:

```cpp
// =================================================================
// <relative path, e.g. src/gui/MeterWidget.cpp>  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//   Project Files/Source/Console/ucMeter.cs
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
//   For any code originally written by Richard Samphire MW0LGE, or for any
//   modifications made by him, the copyright holder for those portions
//   (Richard Samphire) reserves the right to use, license, and distribute
//   such code under different terms, including closed-source and proprietary
//   licences, in addition to the GNU General Public License granted in
//   LICENCE. Nothing in this statement restricts any rights granted to
//   recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Structural template follows AetherSDR
//                (ten9876/AetherSDR) Qt6 conventions.
// =================================================================
```

### C2. Template variants

Four variants live in `docs/attribution/HEADER-TEMPLATES.md`:

| Variant | When used | Changes vs canonical form |
|---|---|---|
| `thetis-samphire` | File sourced from Thetis code containing Samphire contributions | Canonical (above) |
| `thetis-no-samphire` | Thetis-sourced but no Samphire contributions (pure FlexRadio / Wigley) | Drop dual-license block |
| `mi0bot` | File sourced from the Hermes-Lite fork | Add `Copyright (C) [dates] Reid Campbell (MI0BOT) — Hermes-Lite fork contributions` under Wigley line; preserve the upstream Thetis chain too |
| `multi-source` | File synthesizes multiple Thetis files | List each Thetis source path; include union of all contributors' copyright lines |

### C3. Modification note rules

Every derived file carries a `Modification history (NereusSDR)` block with:

1. **Date** in `YYYY-MM-DD` ISO format (unambiguous internationally).
2. **Human author** with callsign (J.J. Boyd, KG4VCF).
3. **AI tooling disclosure** — explicit: "AI-assisted transformation via
   Anthropic Claude Code." Richie already observed AI involvement (§7 of the
   formal notice); disclosure is stronger than silence.
4. **Optional architectural note** where AetherSDR structural template applies.

Subsequent material edits append new dated lines to the same block.
Trivial edits (typo fixes, whitespace, lint) do not require a new line.

### C4. Where headers do NOT go

- Pure NereusSDR-native files (Qt widgets written from scratch: `VfoWidget`,
  `HGauge`, `ColorSwatchButton`, applet panels, etc.) — no Thetis header.
- `src/models/Band.h` — this is a NereusSDR enum; even though order matches
  Thetis convention, the enum itself is independently written. Goes on the
  "independently implemented" list in the provenance doc.
- Test fixtures under `tests/` unless they import Thetis data (e.g. recorded
  I/Q from a Thetis capture) — spot-check case by case.
- Build files, CI config, shell scripts — no.

---

## 7. Workstream D — ANAN artwork replacement and asset provenance

### D1. ANAN multimeter face (`resources/meters/ananMM.png`)

Delete. Replace with original artwork designed specifically for NereusSDR:

- No Apache Labs or ANAN branding
- No Ernst-styled typography (the original face used Ernst's chosen font set)
- Generic multimeter face with NereusSDR styling, suitable for any OpenHPSDR
  radio

Implementation options — maintainer's choice:

1. **Qt-rendered gauge.** Draw the face programmatically in `NeedleItem`
   rather than loading a bitmap. Trivially original.
2. **New bitmap.** Commission or self-create an original PNG with distinct
   design language. Slightly higher effort, preserves the image-loading path.

Either is acceptable. Option 1 is recommended because it eliminates the
asset-provenance question entirely for this widget class going forward.

### D2. Cross-needle assets (`cross-needle.png`, `cross-needle-bg.png`)

Audit provenance:

- Check git log for when and by whom these assets were added
- Compare against Thetis skin asset library for identical or near-identical matches
- If provenance traces to Thetis / OE3IDE / any unattributed upstream: delete
  and replace using option 1 or 2 above
- If genuinely original: document origin in `docs/attribution/ASSETS.md`

### D3. `docs/attribution/ASSETS.md`

New file. Table listing every image / icon / binary asset in `resources/` and
`docs/images/` with columns: *path · origin (original / commissioned /
licensed) · author · license · date added*. Covers the OE3IDE question
proactively for all assets, not just the two Richie named.

### D4. Worktree propagation

The 11 existing worktrees under `.worktrees/` each have their own copy of
`ananMM.png`. Do not hand-edit each worktree. After `compliance/v0.2.0-remediation`
merges to `main`, each feature branch that has a worktree will pick up the
replacement on rebase/merge. Feature-branch owners are notified via the
branch-freeze communication.

---

## 8. Workstream E — README and CLAUDE.md re-framing

### E1. README

**Opening paragraph replacement** (current README.md line 10):

Before:
> NereusSDR is an independent cross-platform SDR client deeply informed by
> the workflow, feature set, and operating style of [Thetis](...), reimagined
> with a new GUI, a modernized architecture, and native support for macOS,
> Linux, and Windows.

After:
> NereusSDR is a C++20/Qt6 port of [Thetis](https://github.com/ramdor/Thetis)
> — the canonical OpenHPSDR / Apache Labs SDR console, itself descended from
> FlexRadio PowerSDR — carrying its radio logic, DSP integration, and feature
> set forward to a native cross-platform codebase (macOS, Linux, Windows) with
> a Qt-based GUI. The Thetis contributor lineage (FlexRadio Systems, Doug
> Wigley W5WC, Richard Samphire MW0LGE, and the wider OpenHPSDR community) is
> preserved per-file in source headers and summarized in
> [docs/attribution/THETIS-PROVENANCE.md](docs/attribution/THETIS-PROVENANCE.md).
> Distributed under GPLv3, compatible with Thetis's GPLv2-or-later terms.

**Footer replacement** (current README.md line 267):

Before:
> *NereusSDR is an independent project and is not affiliated with or endorsed
> by Apache Labs or the OpenHPSDR project.*

After:
> *NereusSDR is a derivative work of Thetis licensed under the GNU General
> Public License. It is not affiliated with or endorsed by Apache Labs,
> FlexRadio Systems, ramdor/Thetis, or the OpenHPSDR project.*

### E2. `docs/readme-independent-framing` branch deletion

The branch exists as a single-purpose branch re-arguing the "independent"
framing that the formal notice cites as evidence against the project. With
the README now corrected to the opposite framing, leaving this branch
reachable is an own-goal. Delete from `origin` as part of the compliance merge:

```bash
git push origin --delete docs/readme-independent-framing
```

### E3. CLAUDE.md source-first license-preservation rule

Add the following block to `CLAUDE.md` inside the `⚠️ SOURCE-FIRST PORTING
PROTOCOL` section, immediately after the existing `READ → SHOW → TRANSLATE`
block:

```markdown
### License-preservation rule (non-negotiable)

When porting any Thetis file, you MUST — in the same commit that introduces
the port — copy the following from the Thetis source into the NereusSDR file's
header comment:

1. All `Copyright (C)` lines naming contributors (FlexRadio, Wigley, Samphire,
   W2PA, mi0bot, etc.)
2. The GPLv2-or-later permission block verbatim
3. The Samphire dual-licensing statement — ONLY if the Thetis source file
   contains Samphire-authored contributions
4. A trailing "Modification history (NereusSDR)" block with the port date,
   human author, and AI tooling disclosure

Templates live in `docs/attribution/HEADER-TEMPLATES.md`. Failure to preserve
these notices on a new port is a GPL compliance bug, not a style nit — reject
the PR.
```

Also add one row to the existing "What Counts As 'Guessing' (NEVER Do These)"
list:

> - Porting a Thetis file without copying its license header and appending a
>   modification note

### E4. CONTRIBUTING.md addition

Add a new section before the "Code of Conduct" section:

```markdown
## License Preservation on Derived Code

Any PR that ports, translates, or materially adapts Thetis source code into
NereusSDR must preserve the original license header, copyright lines, and
dual-licensing notices from the Thetis source file, and append a dated
modification note to the NereusSDR file. See
[docs/attribution/HEADER-TEMPLATES.md](docs/attribution/HEADER-TEMPLATES.md)
for templates and [docs/attribution/THETIS-PROVENANCE.md](docs/attribution/THETIS-PROVENANCE.md)
for the existing provenance inventory.

This is a merge-blocking requirement, not a style preference.
```

---

## 9. Workstream F — v0.2.0 rebuild and Richie courtesy review

### F1. Compliance branch

Branch `compliance/v0.2.0-remediation` off `main`. All headering work,
artwork replacement, README/CLAUDE.md updates, PROVENANCE doc, and CHANGELOG
entry land on this branch. Commits are GPG-signed per standing project rule.

### F2. Version bump

`CMakeLists.txt` version changes from `0.1.7` to `0.2.0`. Not `0.1.8` —
the minor bump signals a clean compliance reset and makes the break obvious
to anyone scanning release history.

### F3. CHANGELOG entry for v0.2.0

Neutral, factual, non-incriminating:

```markdown
## v0.2.0 — License compliance remediation

- Restored Thetis copyright, GPLv2-or-later permission, per-contributor
  attribution, and Samphire dual-licensing notices across all derived
  source files.
- Added per-file modification history blocks disclosing reimplementation
  date, author, and AI-assisted transformation, per GPL notice-preservation
  requirements.
- Published `docs/attribution/THETIS-PROVENANCE.md` — complete file-by-file
  provenance inventory covering ramdor/Thetis, mi0bot/Thetis-HL2, and TAPR
  WDSP derivations.
- Replaced `resources/meters/ananMM.png` with original NereusSDR artwork;
  audited remaining meter assets and documented in
  `docs/attribution/ASSETS.md`.
- Re-framed README to accurately describe NereusSDR as a C++20/Qt6 port of
  Thetis within the FlexRadio / OpenHPSDR / Thetis lineage.
- Added non-negotiable license-preservation rule to the source-first
  porting protocol in CLAUDE.md and to CONTRIBUTING.md.
- No functional changes.

**Binaries from v0.1.1–v0.1.7 remain withdrawn and should not be redistributed.**
```

### F4. Courtesy bundle for Richie

Before re-enabling `release.yml` or pushing the v0.2.0 tag, send Richie a
message on the existing Discord thread containing:

- Link to `docs/attribution/THETIS-PROVENANCE.md` on `main`
- Links to 3–5 representative ported files so he can verify header correctness
  (include at minimum a Samphire-touched file, a Wigley-only file, and an
  mi0bot-sourced file)
- Before/after of `ananMM.png`
- Diff of the README re-framing
- Planned v0.2.0 publish date

Draft message (approved in brainstorming session):

> Richie — compliance sweep wrapped. Summary:
>
> - [N] files now carry full Thetis headers with per-contributor copyright,
>   GPLv2-or-later permission block, Samphire dual-license statement where
>   applicable, and a dated modification note disclosing AI-assisted
>   transformation.
> - File-by-file provenance inventory at [link to THETIS-PROVENANCE.md].
> - ANAN multimeter face replaced with original artwork; cross-needle assets
>   audited.
> - README re-framed as a port of Thetis within the OpenHPSDR/FlexRadio
>   lineage; the `docs/readme-independent-framing` branch has been deleted.
> - CLAUDE.md source-first protocol now has a non-negotiable license-
>   preservation rule so this can't recur on future ports.
>
> Before I build v0.2.0 binaries, I'd be grateful if you'd skim and flag
> anything you'd still like corrected. If you'd be willing to be listed as
> compliance reviewer on the v0.2.0 release notes — wording entirely your
> call, or decline cleanly — the project and I would consider it an honor to
> acknowledge you. Either way, I'll hold the build until you've had a chance
> to look, or five business days have passed with one polite follow-up ping.
> Deep thanks for the generous handling and the nudge toward doing this
> right.
>
> — J.J. Boyd ~ KG4VCF

### F5. Quiet period

Hold v0.2.0 binary build until one of the following is true:

- Richie responds with "ship it" or equivalent
- Richie responds with correction requests (address them, re-send, wait again)
- Five business days elapse with no response, plus one polite follow-up ping

The five-business-day default is a ceiling, not a target — the maintainer
has indicated intent to execute the underlying work faster than two to three
weeks, and Richie may respond quickly. If he does, ship when ready.

---

## 10. Workstream G — v0.2.0 public release

Post-merge, post-Richie-ack:

- Re-enable `release.yml` by reverting the A4 edit.
- Push the `v0.2.0` tag; CI produces signed binaries per existing release
  pipeline (Linux AppImage ×2 archs, macOS Apple Silicon DMG, Windows
  portable ZIP + NSIS installer).
- Release notes for v0.2.0 use the CHANGELOG entry verbatim plus, if Richie
  accepted the reviewer credit, a "Compliance reviewed by" line acknowledging
  him.
- Announce v0.2.0 on the same Thetis Facebook thread where v0.1.7 was
  announced, linking the withdrawal note and the release notes.
- Unpin the withdrawal-note issue after v0.2.0 is live.

---

## 11. `docs/attribution/THETIS-PROVENANCE.md` structure

Single markdown file, grep-friendly. Skeleton:

```markdown
# Thetis Provenance — NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on Thetis (ramdor/Thetis), its mi0bot/Thetis-HL2
fork, or WDSP (TAPR/OpenHPSDR-wdsp). Per-file license headers live in the
source files themselves; this index is the grep-able summary.

## Legend

Derivation type:
- `port`       — direct reimplementation in C++/Qt6 of a Thetis C# source file
- `reference`  — consulted for behavior during independent implementation
- `structural` — architectural template with substantive behavioral echo
- `wrapper`    — thin C++ wrapper around vendored C source (WDSP)

## Files derived from ramdor/Thetis

| NereusSDR file | Thetis source | Line ranges | Type | Contributors | Notes |
| --- | --- | --- | --- | --- | --- |
| src/core/WdspEngine.cpp | Project Files/Source/Console/cmaster.cs | full | port | FlexRadio, Wigley, Samphire | |
| src/gui/MeterWidget.cpp | Project Files/Source/Console/MeterManager.cs | 1049+ | port | FlexRadio, Wigley, Samphire | |
| ... | ... | ... | ... | ... | ... |

## Files derived from mi0bot/Thetis-HL2

| NereusSDR file | mi0bot source | Line ranges | Type | Contributors | Notes |
| --- | --- | --- | --- | --- | --- |
| src/core/RadioDiscovery.cpp | clsRadioDiscovery.cs | full | port | MI0BOT, Wigley | |

## Files derived from TAPR WDSP

Vendored in `third_party/wdsp/` with upstream TAPR/OpenHPSDR-wdsp license
and Warren Pratt (NR0V) attribution preserved. No per-file NereusSDR header
required. See `third_party/wdsp/README.md` and `third_party/wdsp/LICENSE`.

## Independently implemented — Thetis-like but not derived

Files whose behavior resembles Thetis but whose implementation was written
without consulting Thetis source. No header required.

| NereusSDR file | Behavioral resemblance | Basis of implementation |
| --- | --- | --- |
| src/models/Band.h | Band enum order mirrors Thetis convention | Ham-band definitions from IARU Region 2 spec; no Thetis source consulted |
| ... | ... | ... |
```

---

## 12. Verification checklist (merge gate)

`compliance/v0.2.0-remediation` does not merge to `main` until every box is
green:

- [ ] Every file listed in `THETIS-PROVENANCE.md` carries the new header block
- [ ] Random spot-check: 10 headered files verified against their cited Thetis sources (header contents match the Thetis file's actual copyright lines)
- [ ] Inbound audit documented — "Independently implemented" section populated with judgment-call justifications
- [ ] `third_party/wdsp/` and `third_party/fftw3/` not modified
- [ ] `resources/meters/ananMM.png` replaced with original artwork
- [ ] `cross-needle.png` and `cross-needle-bg.png` audited — replaced or documented
- [ ] `docs/attribution/ASSETS.md` present, covers every asset in `resources/` and `docs/images/`
- [ ] README opening and footer replaced per §8.1
- [ ] `docs/readme-independent-framing` branch deleted on `origin`
- [ ] CLAUDE.md source-first license-preservation rule added
- [ ] CONTRIBUTING.md license-preservation section added
- [ ] `docs/attribution/HEADER-TEMPLATES.md` present with 4 template variants
- [ ] CHANGELOG `v0.2.0` entry written
- [ ] `CMakeLists.txt` version bumped to `0.2.0`
- [ ] Smoke build passes on all 3 platforms in CI
- [ ] Richie pre-release bundle sent; ack received OR quiet-period elapsed with one follow-up ping
- [ ] All compliance-branch commits GPG-signed

---

## 13. In-flight branch handling

At the start of this work, the repo has ~15 active feature branches (
`feature/sample-rate-wiring`, `feature/step-attenuator`,
`feature/display-clarity-pr3`, `feature/phase3g10-*`, etc.). Each was branched
before the compliance work and touches files that will receive new headers.

Policy for the compliance window:

1. **No PR merges to `main`** until `compliance/v0.2.0-remediation` lands. The
   branch freeze is communicated in the pinned withdrawal-note issue.
2. **After merge**, each feature branch rebases onto the new `main`. Header
   additions are file-top-only; they rebase cleanly unless the feature
   branch also edits the top of the same file, which is rare.
3. **Any feature branch that adds a new file ported from Thetis** must add
   the header at file-creation time going forward, per the CLAUDE.md
   addendum. The PR-reviewer checklist flags missing headers as a
   merge-blocker.

---

## 14. Out of scope

- Re-licensing the project. Stays GPLv3.
- Any functional change, feature addition, bug fix, or refactor unrelated
  to compliance. The compliance branch is surgically focused — any
  drive-by improvement goes in a separate PR after v0.2.0 ships.
- Retrospective cure of v0.1.x binaries (not possible per GPL §12).
- Proactive legal counsel engagement. Retained as an option but not
  activated unless Richie or another party escalates despite the
  remediation.
- Public acknowledgment of "license violation" in the withdrawal note.
  Richie wrote the agreed wording; it stays as drafted.

---

## 15. Open questions

Deferred to implementation planning (`writing-plans` output):

1. Exact expected headcount of derived files after inbound sweep — drives
   effort estimate for header insertion.
2. Whether to Qt-render the replacement multimeter face or ship a bitmap.
3. Exact wording Richie chooses (if any) for the compliance-reviewer credit
   line in v0.2.0 release notes.
4. Whether to close or re-style the `release.yml` workflow freeze (comment
   out trigger vs feature-flag secret).

---

## 16. Success criteria

This remediation is successful when:

1. Richie has reviewed the compliance tree and either acknowledged it as
   compliant or the quiet period has elapsed without objection.
2. A neutral third-party reviewer (e.g. a Conservancy-style GPL compliance
   reviewer) could examine any derived file in NereusSDR and find the
   Thetis copyright, license, and attribution notices correctly preserved.
3. The project's public positioning (README, release notes) accurately
   describes NereusSDR as a derivative port of Thetis.
4. Future Thetis ports — executed by the maintainer, contributors, or AI
   tooling — cannot re-introduce the same defect because the source-first
   protocol now mandates header preservation in the same commit as the port.
5. v0.2.0 has shipped cleanly as the first binary release of the compliant
   tree.
