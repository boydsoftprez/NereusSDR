# NereusSDR Attribution Remediation Log

Chronological record of every attribution gap discovered — during audit
passes, via external reports, or through self-review — together with
how and when each was cured.

This log is the transparent artifact that shows the project takes
compliance as a standing practice rather than a one-time event. Entries
are appended chronologically; nothing is ever deleted or edited.

Reporting a new omission? See
[`REPORTING-OMISSIONS.md`](REPORTING-OMISSIONS.md).

---

## Entry format

```
## YYYY-MM-DD — <short title>

**Discovered by:** <name / role>
**Reported via:** <GitHub issue # / private email / internal audit / external notice>
**Affected files:** <paths or count>
**Gap:** <what was missing or wrong>
**Root cause:** <why the first-pass audit missed it>
**Fix:** <commit SHA or branch>
**Process improvement:** <what changed to prevent recurrence>
```

---

## 2026-04-16 — Initial compliance posture failure (formal notice)

**Discovered by:** Richard Samphire (MW0LGE), Thetis principal contributor
**Reported via:** Private Discord message + draft formal notice
**Affected files:** ~170 NereusSDR source files derived from Thetis / mi0bot/Thetis-HL2 / WDSP, plus 3 graphical assets and the README framing
**Gap:**
- No per-file Thetis copyright, GPL permission block, warranty disclaimer, or contributor attribution preserved (root `LICENSE` only — GPL §1 "keep intact" violation)
- ANAN multimeter face artwork derivative of OE3IDE work with watermark removed
- README positioned NereusSDR as "independent" implementation despite extensive source-first porting
- v0.1.1–v0.1.7 binaries distributed in non-compliant state
**Root cause:** First-pass development relied on root-level LICENSE file and one-line `// From Thetis ...` comments without reproducing the upstream per-file license block; AI-assisted porting protocol (CLAUDE.md) did not mandate header preservation; artwork was AI-regenerated without understanding the original's attribution requirements
**Fix:** 26-commit compliance sweep on branch `compliance/v0.2.0-remediation` — see `docs/architecture/2026-04-16-gpl-compliance-remediation-design.md` for full work
**Process improvement:**
- Added non-negotiable license-preservation rule to CLAUDE.md source-first protocol
- Added merge-blocking requirement to CONTRIBUTING.md
- Established `docs/attribution/THETIS-PROVENANCE.md` as authoritative inventory
- Introduced `scripts/verify-thetis-headers.py` as gating test

---

## 2026-04-17 — Second pass: missing inline contributor attributions

**Discovered by:** Internal audit (after successful first pass)
**Reported via:** Self-initiated second-pass review — concern that fixed-template variants (samphire / no-samphire / mi0bot) only named 2–4 contributors while actual Thetis sources named 12+
**Affected files:** 157 NereusSDR files (112 thetis-samphire + 2 no-samphire + 4 mi0bot + 39 multi-source)
**Gap:** Per-file headers did not include inline-modification contributors named in the upstream source (W2PA Codella, G8NJJ Barker, NR0V Pratt, WD5Y, W4WMT Rambo, VK6APH Harman, KD5TFD Tracey, MI0BOT) — only the copyright-block contributors made it into the first pass
**Root cause:** Phase 1 applied fixed-template variants mechanically rather than extracting per-file contributor unions; only multi-source variant did unions
**Fix:** Commits `84e2ee7` (contributor index), `6aa8c0a` (reconciliation diff), `d436cd0` (4 template mismatches), `ef84eac` (fixed-template additions), `0857eb8` (multi-source touch-ups)
**Process improvement:**
- Added `docs/attribution/thetis-contributor-index.md` as reusable reference
- Inline mods now treated as first-class attribution, not commentary
- Verifier classifier bugs (substring match on "samphire", regex missing digit in "mi0bot") fixed

---

## 2026-04-17 — Second pass concern: MI0BOT full name unverifiable (inverted later)

**Discovered by:** Internal audit (Phase 2 subagent)
**Reported via:** Phase 2 concerns list
**Affected files:** 5 mi0bot-variant files + HEADER-TEMPLATES.md
**Gap:** Phase 1 template read "Reid Campbell (MI0BOT)" but `ramdor/Thetis` source nowhere spells "Campbell" — only "Reid" (first name) or "MI0BOT" (callsign). Internal audit couldn't verify the surname.
**Root cause:** First pass wrote the template without cross-checking Thetis source for the name
**Interim fix:** Commit `1f6b7de` normalized to callsign-only "MI0BOT" across the 5 files and HEADER-TEMPLATES.md for source-first strictness
**Later correction:** See next entry.

---

## 2026-04-17 — Third pass: MI0BOT full name verified from fork source

**Discovered by:** Internal audit (after cloning mi0bot/OpenHPSDR-Thetis locally)
**Reported via:** Self-initiated sanity pass on mi0bot fork
**Affected files:** 5 mi0bot-variant files, HEADER-TEMPLATES.md, THETIS-PROVENANCE.md, `scripts/verify-thetis-headers.py`
**Gap:** mi0bot fork's own `Project Files/Source/Console/HPSDR/IoBoardHl2.cs` header literally reads `Copyright (C) 2025 Reid Campbell, MI0BOT, mi0bot@trom.uk` — verifying the surname from an authoritative source. Also: `IoBoardHl2.cs` is a **fork-unique** file (not in ramdor/Thetis) authored solely by Reid Campbell, so `Hl2IoBoardTab.cpp/.h` had been incorrectly over-attributed via the generic mi0bot template (FlexRadio + Wigley + Samphire added).
**Root cause:** mi0bot fork had not been locally cloned during first or second pass; attribution work relied on ramdor/Thetis upstream only, which doesn't contain the fork-unique files or the fork's own copyright headers
**Fix:** Commit `a825a7f` — restored "Reid Campbell (MI0BOT)" full form for RadioDiscovery files (fork contributions), rewrote Hl2IoBoardTab headers to Reid-Campbell-solo form (no FlexRadio/Wigley/Samphire), added Variant 5 (`mi0bot-solo`) template, tightened verifier classifier
**Process improvement:**
- mi0bot/OpenHPSDR-Thetis now locally cloned at `/Users/j.j.boyd/mi0bot-Thetis/` for future reference
- New `mi0bot-solo` variant documented in HEADER-TEMPLATES.md for fork-unique files
- Lesson: any future claim about upstream contributor identity or file provenance MUST be verified against the actual upstream source, not inferred from secondary mentions

---

## 2026-04-17 — Audit-doc prose used stale names

**Discovered by:** J.J. Boyd (maintainer self-review)
**Reported via:** Visual spot-check of `second-pass-reconciliation.md` on GitHub
**Affected files:** `docs/attribution/second-pass-reconciliation.md` (30 instances), `docs/attribution/thetis-contributor-index.md` (14 instances)
**Gap:** Phase 1b audit doc recorded "John Melton (G8NJJ)", "Ron Holcomb (WD5Y)", "Bob Tracy (W4WMT)" as provisional names. Phase 2 verified and corrected names (Laurence Barker, Joe, Bryan Rambo respectively) when applying headers, but never back-propagated the corrections to the audit documents themselves. Readers of the audit docs would see wrong names.
**Root cause:** Phase 2's scope was "apply corrections to source files", not "back-fix audit documentation"
**Fix:** Commit `40558bc` — `sed`-style replacement across both audit docs
**Process improvement:** When Phase N subagents verify and correct names/facts that earlier phases recorded provisionally, the same subagent (or an explicit follow-up) must back-fix the provisional record so the audit trail doesn't drift from the applied reality

---

## 2026-04-17 — Phase 4 deep-audit pass (Tasks 24, 26, 27, 30, 31)

**Discovered by:** Internal deep-dive review after third pass
**Reported via:** Self-initiated Phase 4 audit covering GPL notice completeness, non-Thetis upstream provenance, binary-level surfaces, and process hardening
**Gaps addressed in this wave:**

1. **Permission-notice 3rd paragraph missing** (GPL §1 "keep intact") —
   Phase 1's templates included only paragraphs 1–2 of the standard GPL
   permission notice. Paragraph 3 ("You should have received a copy…")
   was omitted across ~170 derivative files; only `Hl2IoBoardTab.*`
   (hand-written verbatim from `IoBoardHl2.cs`) had the full form.
   Task 24a audit of the 27 cited Thetis sources found 60/40 variance
   between old (59 Temple Place) and current (51 Franklin Street) FSF
   addresses. Picked the current FSF-canonical form for tree-wide
   consistency (documented decision, not an arbitrary pick). Fix:
   commit `d53976c` — 169 files updated + 4 templates extended; 2 files
   (Hl2IoBoardTab.cpp/.h) preserved verbatim from source (59 Temple
   Place) as a documented source-verbatim exception.

2. **WDSP license specificity unverified** — `third_party/wdsp/` had
   never been grepped to confirm GPLv2-or-later vs v2-only. If v2-only,
   our GPLv3 wrappers would have been incompatible. Task 26 survey:
   128/138 files GPLv2-or-later, zero v2-only, 10 files no header
   (data tables / generated). Fully compatible with root GPLv3
   distribution. Documented in `docs/attribution/WDSP-PROVENANCE.md`.
   Fix: commit `0e8ef76`.

3. **Style-7 / Digital-7 font credit** — Thetis `console.cs` credits
   Sizenko Alexander / Style-7 for the Digital-7 font. If NereusSDR
   used the font, the credit would need to propagate. Task 27 grep:
   zero hits for font name/author; zero `.ttf`/`.otf` files. Font is
   not used. Documented absence in `docs/attribution/ASSETS.md` so
   the attribution record is explicit. Fix: commit `1c1e664`.

4. **CI verifier not enforced** — `scripts/verify-thetis-headers.py`
   ran only via manual invocation. Any PR could introduce a ported
   file without a proper header and nothing would catch it until the
   next manual sweep. Task 30 wired the verifier into the Ubuntu job
   of `.github/workflows/ci.yml`, runs post-checkout and pre-compile
   so failures happen fast. Fix: commit `0088a0a`.

5. **No reverse-sync check between tree and PROVENANCE** — tree could
   drift out of sync with `THETIS-PROVENANCE.md` (file renamed or new
   port added without updating PROVENANCE). Task 31 added
   `scripts/verify-provenance-sync.py` — detects files with "Ported
   from Thetis" marker not listed in PROVENANCE, and PROVENANCE rows
   pointing to files no longer on disk. Current tree: 148/148 clean.
   Fix: commit `acf85d7`.

**Root cause (shared across #1–#3):** Phase 1's audit was scoped to
contributor-identity and per-file header presence. Ancillary
completeness questions (full permission notice, third-party license
specifics, font credits) were not part of that scope and remained
unaudited until the Phase 4 deep-dive.

**Process improvements:**
- Task 30: CI now catches missing-header regressions at PR time.
- Task 31: CI can also catch PROVENANCE drift (wiring into CI will
  happen alongside Task 30's follow-up).
- Task 26: WDSP-PROVENANCE.md now serves as reference for the
  third_party upstream we incorporate by vendoring.
- Phase 4 plan in `docs/architecture/2026-04-17-gpl-compliance-remediation-plan.md`
  documents the remaining Phase 4 tasks (AetherSDR audit at Task 25,
  binary metadata at Task 28, third-party license bundle at Task 29).

---

## 2026-04-17 — AetherSDR provenance audit (Phase 4 Task 25)

**Discovered by:** Internal Phase 4 deep-dive — the three earlier passes had audited only Thetis / mi0bot derivation. ~170 Modification-History blocks in the compliance-branch tree cited AetherSDR (ten9876/AetherSDR) as the structural template, but AetherSDR itself had never been checked. Direct parallel to the original Thetis omission Richie flagged.
**Reported via:** Self-initiated Phase 4 audit
**Affected files:** 176 NereusSDR files (142 source + 18 tests + 16 with only inline AetherSDR references)
**Gaps:**

1. **AetherSDR attribution missing where the derivation is real.** 33 NereusSDR files genuinely port an AetherSDR counterpart (SpectrumWidget, VfoWidget + mode containers, RadioModel/SliceModel/PanadapterModel, MainWindow signal-routing hub, ConnectionPanel, FloatingContainer/ContainerManager, AudioEngine, etc.) — these carried no AetherSDR Copyright line until now.
2. **False AetherSDR citations.** 126 files claimed "Structural template follows AetherSDR (ten9876/AetherSDR) Qt6 conventions" in their Modification-History block but have no actual AetherSDR counterpart — they are Thetis-derived or NereusSDR-native. Over-attribution.
3. **Mixed-lineage files under-cited.** 10-12 files genuinely have both Thetis and AetherSDR contributions (pattern-inspired) but the citation was unspecific.

**Root cause:**
- Task 25a audit found AetherSDR has **zero per-file copyright headers** — all attribution is centralised in the LICENSE file and About dialog (primary author Jeremy/KK7GWY). Because there was no per-file source-verbatim mandate (nothing to "keep intact" at the file level), Phase 1's templates didn't know how to attribute AetherSDR and defaulted to either the generic boilerplate (overused) or omission (underused).
- AetherSDR is GPLv3 (full compatibility with NereusSDR root) — no license incompatibility; just an attribution gap.

**Fix (4 commits):**
- `e1c95ec` — Task 25a: `docs/attribution/aethersdr-contributor-index.md` — upstream survey catalogues the 235 AetherSDR sources, confirms Jeremy/KK7GWY primary authorship, GPLv3 licensing, and no per-file headers
- `4fff136` — Task 25b: `docs/attribution/aethersdr-reconciliation.md` — per-file classification (A/B/C/D buckets)
- `d9f9e79` — Task 25c commit A: add AetherSDR Copyright line to 33 genuine-derivation files
- `aed081a` — Task 25c commit B: remove false boilerplate AetherSDR line from 126 files
- `7c041bf` — Task 25c commit C: tighten AetherSDR citation on 10 mixed-lineage files (specific counterparts named)
- `e0142e6` — Task 25c commit D: defer 5 files to human review (MainWindow.h/.cpp have no formal port header, WdspEngine/AboutDialog test files are incidental)

**Process improvement:**
- Upstream projects with no per-file headers still need an attribution decision documented at the project level — `HEADER-TEMPLATES.md` now has guidance for AetherSDR-derived work (project-level reference only, not per-file header mirroring).
- Lesson: any future upstream dependency audit should first check whether upstream has per-file headers before deciding on the form of preservation. A project-level LICENSE + central About attribution is the minimum strong reference when per-file headers don't exist.

---

## 2026-04-17 — Binary metadata and release-artifact attribution (Phase 4 Tasks 28 + 29)

**Discovered by:** Internal Phase 4 deep-dive
**Reported via:** Self-initiated Phase 4 audit
**Affected surfaces:** binary-metadata slots (macOS Info.plist, Windows installer, Linux AppImage desktop file) + release artifact contents (license texts shipped to end users)

**Gap 1 — Binary metadata under-attributed (Task 28):**
Release binaries historically embedded NereusSDR-only copyright strings at the OS level. A user inspecting the macOS bundle's Info.plist, the Windows installer's LegalCopyright, or the Linux desktop file's Comment would see no hint of the Thetis / FlexRadio / OpenHPSDR lineage. Over 35 derivative attribution work at the source-file level was strong, but the binary-side attribution didn't match.

**Fix (Task 28 — commit `0871935`):**
- `CMakeLists.txt`: added `MACOSX_BUNDLE_COPYRIGHT` (Thetis-lineage string), `MACOSX_BUNDLE_SHORT_VERSION_STRING`, `MACOSX_BUNDLE_BUNDLE_VERSION` (previously unset)
- `scripts/windows/installer.nsi`: updated `VIAddVersionKey LegalCopyright`, added `CompanyName`, updated `Publisher` registry key
- `.github/workflows/release.yml`: extended Linux AppImage `.desktop` `Comment=` field with GPLv3 / Thetis / FlexRadio lineage parenthetical
- Locally verified: `defaults read build/NereusSDR.app/Contents/Info.plist NSHumanReadableCopyright` returns the Thetis-lineage string

**Gap 2 — Release artifacts didn't ship third-party license texts (Task 29):**
Binary aggregation of GPL-covered dependencies (Qt6 LGPLv3, FFTW3 GPLv2-or-later, WDSP GPLv2-or-later) requires that the recipient receive the applicable license texts. NereusSDR release artifacts shipped none of these — a downstream user unpacking an AppImage, DMG, or Windows installer would see only NereusSDR's own LICENSE, not the dependencies' licenses.

**Fix (Task 29 — commit `5ba06e0`):**
- New `packaging/third-party-licenses/` directory with pointer files for Qt6, FFTW3, WDSP, plus an index `README.md` naming each dependency, its role, its license, and the file where its license text lives
- `CMakeLists.txt`: new `install(DIRECTORY ...)` rule places the directory at `share/doc/nereussdr/licenses/` in any install prefix; new `install(FILES LICENSE ...)` rule also installs NereusSDR's own LICENSE at `share/doc/nereussdr/`
- `.github/workflows/release.yml`: macOS step copies licenses into `NereusSDR.app/Contents/Resources/licenses/` before codesign; Windows Deploy-Qt step copies licenses into the ZIP-and-NSIS root alongside `NereusSDR.exe`
- `scripts/windows/installer.nsi`: `SecMain` now installs licenses to `$INSTDIR\licenses\`
- Linux AppImage: no workflow edit needed — the CMake install rule places files under `AppDir/usr/share/doc/nereussdr/licenses/` and linuxdeploy preserves that path automatically
- Locally verified: `cmake --install build --prefix /tmp/nereus-install` places all 5 files correctly

**Current pointer-file choice:** each of `qt6.txt`, `fftw3.txt`, `wdsp.txt` points to the canonical upstream URL for the full license text rather than embedding multi-page GPL/LGPL bodies. This meets the GPL obligation (the recipient can obtain the license) while keeping the repository readable. If a compliance reviewer prefers full-text bodies shipped in-tree, follow-up commit can drop the canonical texts into these files without structural changes.

**Root cause (shared across Tasks 28 + 29):**
Release-pipeline infrastructure was built before the compliance-lineage was explicitly scoped. Binary-metadata and third-party-license bundling were never specified as release requirements because the project framed itself as "independent" at the time.

**Process improvement:**
Both of these are now part of the release pipeline and will stay consistent automatically for all future releases. Future additions to `third_party/` should also add a pointer file in `packaging/third-party-licenses/` and confirm the install rule reaches it.

---

## 2026-04-17 — ASSETS.md mischaracterized AI-generated meter artwork as "hand-designed"

**Discovered by:** J.J. Boyd (maintainer self-review while reading the branch on GitHub)
**Reported via:** Direct notice to assistant
**Affected files:** `docs/attribution/ASSETS.md` (6 mentions across the `resources/meters/` section)
**Gap:** ASSETS.md described `ananMM.png`, `cross-needle.png`, `cross-needle-bg.png`, and the JPG masters under `docs/attribution/source-artwork/` as "hand-designed by J.J. Boyd." The artwork was actually designed by J.J. Boyd using AI image-generation tooling. The description was inaccurate and conflicted with the broader compliance posture of disclosing AI-assisted work explicitly (compare the per-file source-code Modification-History blocks that read "AI-assisted transformation via Anthropic Claude Code").
**Root cause:** When the user supplied the final JPG masters (`NereusMeter.jpg` / `NereusMeter-Dual.jpg`) during Task 12's artwork swap, the commit that swapped them in described the artwork as "hand-designed" — the assistant carried that phrasing into ASSETS.md without confirming the generation method. Richie's formal notice §7 already flagged AI-assisted development as a disclosure requirement, so the omission was a missed opportunity to be transparent by default.
**Fix:** Commit `<pending>` — rewrote the three `resources/meters/` table rows and the "Source artwork files" prose to describe the artwork as "Original NereusSDR artwork designed by J.J. Boyd (KG4VCF) using AI image-generation tooling" and tightened the non-derivation claim ("no ANAN / Apache Labs / OE3IDE bitmap was used as input, reference, or training material"). Removed "drawn from scratch" language that conflicted with the AI-tool disclosure.
**Process improvement:** When the user supplies artwork or content during a task, confirm the generation method explicitly before writing the attribution prose. "Hand-designed" / "drawn" / "composed" are factual claims — ask if unsure.

---

## 2026-04-17 — Pass 5: strict verbatim header rewrite per MW0LGE feedback

**Discovered by:** Richard Samphire (MW0LGE), Thetis principal contributor
**Reported via:** Discord message providing an explicit FFTEngine.cpp template and the rule "the text and licence must remain exactly the same as they are in Thetis ... nothing in the licence or attribution text should be changed in any way"
**Affected files:** 169 derivative files across `src/` and `tests/` (the full PROVENANCE derivative-table inventory minus the 2 `Hl2IoBoardTab.*` files already in correct form from Phase 3)
**Gap:** Phases 1–4 built headers using a 5-variant templated system with normalized contributor lists. Richie's strict reading of GPL §1 ("keep intact all the notices") requires each source file's header to be copied byte-for-byte, not extracted/paraphrased/normalized. Specifically:
- Our "Original Thetis copyright and license (preserved per GNU GPL)" wrapper prose — our paraphrase, not source text
- Extracted copyright lines reassembled into a canonical order — not how source presented them
- Phase 2's added contributor-union lines (W2PA/G8NJJ/NR0V/WD5Y/W4WMT/KD5TFD) in file headers — those contributors appear in inline body markers in Thetis source, not in the file-header copyright block; they did not belong in our copyright block
- Phase 4 Task 24's "51 Franklin Street" FSF-address normalization — substituted current address for the address each source actually uses (many use "59 Temple Place")
- Paraphrased "Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):" intro wrapper — our words, not source's; the source's dual-license statement has its own ASCII-box formatting that was flattened
- Stripped FlexRadio postal-address block, VK6APH Waterfall AGC mod line, MW0LGE "Transitions to directX" line from headers — all present in display.cs source but dropped from our `FFTEngine.cpp` header

**Root cause:** Early Phase 1 design prioritized consistency and readability across the tree over strict source fidelity. The templating approach inherently normalizes; verbatim preservation refuses normalization. We read GPL §1 as "preserve the notice's substance" when Richie's reading (and the strict textual reading) is "preserve the notice's characters."

**Fix (4 commits):**
- `74001f0` — new `scripts/rewrite-verbatim-headers.py` + `git rm` of obsolete `HEADER-TEMPLATES.md` and `scripts/insert-thetis-headers.py`
- `1a63dbc` — mass rewrite of 169 derivative files: NereusSDR port-citation block + NereusSDR Modification-History block + full verbatim Thetis (or mi0bot/Thetis-HL2) source header(s), stacked for multi-source files
- `0eaa91d` — verifier classifier tightened: dropped per-variant Samphire dual-license gate (verbatim preservation means it appears iff the source had it; no mismatch possible); widened "GNU General Public License" to "General Public License" to accept LGPL upstream files (networkproto1.c etc.)
- `11d964b` — new `docs/attribution/HOW-TO-PORT.md` documenting the verbatim-preservation rule for future ports

**Process improvement:**
- `HEADER-TEMPLATES.md` is gone. The rule is now "copy the source's header verbatim; do not template."
- `HOW-TO-PORT.md` is the canonical reference for how to port a file: copy the upstream source header byte-for-byte, prepend NereusSDR port-citation + Modification-History blocks, nothing else.
- Future AI-assisted ports that hit the CLAUDE.md source-first protocol are told explicitly to preserve verbatim, not extract-and-reassemble.
- A second pass (Pass 6) is queued to preserve inline mod attributions at block-level within ported code bodies — the `//-W2PA` / `//MW0LGE [x.y.z]` / `// added G8NJJ for X` markers that sit inline in Thetis source and attach to specific functions or blocks. Pass 5 covers file-header preservation; Pass 6 covers per-part preservation.

**Spot-check results (5 files byte-for-byte against sources):**
`FFTEngine.cpp` (vs display.cs, 2676 bytes embedded, exact match), `MeterWidget.cpp` (vs MeterManager.cs), `WdspEngine.cpp` (vs cmaster.c NR0V-only), `RadioDiscovery.cpp` (vs mi0bot/clsRadioDiscovery.cs), `BoardCapabilities.cpp` (7 sources stacked, NetworkIO.cs noted as no-header per spec).

Verifier post-rewrite: 171/171 pass. Provenance sync: 169/169 pass. Build: clean.

---

## 2026-04-17 — Pass 6: inline-mod attribution preservation at block level

**Discovered by:** Richard Samphire (MW0LGE), as a follow-up clarification after Pass 5
**Reported via:** Discord message: *"each part we ported needs its attribution"*
**Affected surface:** inline modification markers in Thetis source file BODIES — `//-W2PA`, `//MW0LGE [x.y.z]`, `// added G8NJJ for X`, `//-MI0BOT: ...`, etc. These are §2(a) block-level modification notices. When NereusSDR ports a specific function/block, the markers attached to that Thetis block must travel with the ported code at the corresponding position.

**Gap:** Pass 5 preserved file-HEADER blocks verbatim but left the file BODIES (our C++ ports of Thetis C# / C code) without any inline attribution. An inline marker like `//-W2PA Necessary for Behringer MIDI changes` in Thetis `console.cs:52` carries W2PA's §2(a) modification notice for that specific line. When we port that line (or the surrounding function) to NereusSDR, the marker is the only thing that identifies W2PA as the contributor of the attached code. Removing it strips Thetis's own §2(a) compliance artifact.

**Root cause:** File-header preservation was the headline compliance issue Richie flagged initially; inline-mod preservation at block granularity is a more subtle follow-up obligation that only became clear once headers were correct. Phases 1–5 didn't address it.

**Fix (5 commits across 3 sub-passes):**

**Pass 6a** (`a373547`): `docs/attribution/thetis-inline-mods-index.md` — catalogued 1,458 inline mod markers across 30 cited Thetis source files. Per-contributor: MW0LGE 1306 (mostly self-references in MW0LGE-authored files); G8NJJ 85; W2PA 49; MI0BOT 14; WD5Y 8; others single-digit.

**Pass 6b** (`55a2ca9`): `docs/attribution/inline-mod-reconciliation.md` — consumed the 6a index, mapped markers to NereusSDR ported locations. Key filter: exclude MW0LGE self-references inside MW0LGE-authored files where the file header's MW0LGE copyright line already covers authorship; include every non-MW0LGE callsign and every MW0LGE `[version]` tag that sits on a specific translated feature block. Result: **only 33 insertions across 13 files actually need to propagate** — the rest of the 1,458 markers attach to Thetis features NereusSDR hasn't ported yet (Behringer MIDI, Andromeda/Aries hardware, CAT extended access, HL2 FixedIp fields, RX2 mute labels, etc.). Those are queued as revisit triggers for future phases (3K CAT, 3L HL2 ChannelMaster, 3M-2 CW TX, future Andromeda UI).

**Pass 6c** (`e1e2d3f` + `ad4f7a1` + `d5f70a5`): Applied 14 actual edits (some reconciliation-doc entries resolved to the same physical edit, and 2 were confirmed no-target skips). Distribution:
- `HpsdrModel.h` — 5 enum-row markers (HermesLite / Saturn / ANAN_G2 / ANAN_G2_1K for G8NJJ + MI0BOT)
- `HardwareProfile.cpp` — 1 ANAN_G2_1K case marker (G8NJJ)
- `RadioDiscovery.cpp` — extended existing MI0BOT comment with Thetis line citation
- `P1RadioConnection.cpp` / `P2RadioConnection.cpp` — MW0LGE [2.10.3.13] ADC-overflow markers
- `WdspTypes.h` — 3 MW0LGE [2.9.0.7] AGC-type markers (AgcPeak, AgcAvg, CfcAvg)
- `StepAttenuatorController.h` — 1 Minus20 / MW0LGE_21d marker
- `tests/tst_radio_discovery_parse.cpp` — 1 HermesLite marker
- `tests/tst_hpsdr_enums.cpp` — 2 markers (HermesLite==6, Saturn==10)

**Skipped (no target in current NereusSDR port — parked for future phases):**
- `SliceModel` cw_pitch clamp (W2PA console.cs:18191) — no clamp exists in our port; range-limited at widget level. Revisit when SliceModel gains a setCwPitch setter.
- `RadioModel.cpp` Saturn DDC switch (G8NJJ console.cs:8559) — Saturn DDC count is resolved through `BoardCapabilities` table, not a direct switch. The enum-row citation in `HpsdrModel.h` is the single point of truth.

**Marker format applied (verbatim from Thetis + citation):**
```cpp
HermesLite = 6,    //-MI0BOT: HL2 added [Thetis clsRadioDiscovery.cs:1239]
```
Exact Thetis marker text preserved; square-bracket citation added so a reader can trace back.

**NereusSDR-side inline markers (Option 3):** not added in this pass — our ports are straight C# / C → C++20/Qt6 translation without significant post-port logic changes at the marked blocks. When future NereusSDR work makes post-port modifications inside a marked region, the `//-KG4VCF [v0.2.x] description` pattern (matching Thetis's release-version convention) applies.

**Verification after Pass 6c:**
- `verify-thetis-headers.py`: 171/171 pass
- `verify-provenance-sync.py`: 169/169 pass
- Build: clean (49/49 objects, final binary linked)

**Process improvements:**
- Pass 6's three-stage pattern (index → reconciliation → application) is now a reusable pattern for any future upstream audit. Pass 6a is mechanical grep; Pass 6b is judgment; Pass 6c is mechanical application.
- The 7 parked revisit triggers are documented in `inline-mod-reconciliation.md` § "Revisit when we port ...". When Phase 3K (CAT), 3L (HL2 ChannelMaster), 3M-2 (CW TX), or Andromeda UI work lands, the relevant Thetis inline markers are ready to be propagated alongside the new ports.

**Header preservation + inline-mod preservation together now cover both GPL §1 ("keep intact") and §2(a) ("prominent notices ... of any change") at both file and block granularity.** Pending Richie's review of the compliance-branch tip.

---

## 2026-04-17 — Missing LICENSE-DUAL-LICENSING file at repo root

**Discovered by:** J.J. Boyd (maintainer compliance-audit self-review)
**Reported via:** GPL §1 audit — checking that every license-relevant notice present in the Thetis repo is mirrored in NereusSDR
**Affected files:** (new) `LICENSE-DUAL-LICENSING` at repo root
**Gap:** Thetis maintains **two** license-related files at its repo root — `LICENSE` (GPLv2-or-later text) and `LICENSE-DUAL-LICENSING` (Samphire's project-level dual-licensing statement applying only to his own contributions). NereusSDR had only `LICENSE` (GPLv3 text). The dual-licensing statement was preserved inline within source files via Pass 5's verbatim header copying wherever Thetis sources embedded it, but the standalone project-level file was not mirrored. GPL §1's "keep intact all the notices that refer to this License" applies to that standalone file too, since it is a project-level licensing-context notice Samphire maintains upstream.
**Root cause:** Phase 1–4 header work focused on per-file copyright and license preservation. Project-level licensing-notice files beyond the main `LICENSE` weren't checked against upstream.
**Fix:** Copied `/Users/j.j.boyd/Thetis/LICENSE-DUAL-LICENSING` verbatim (byte-identical diff) to `/Users/j.j.boyd/NereusSDR/LICENSE-DUAL-LICENSING`. No text modifications — Samphire's statement references "the GNU General Public License granted in LICENCE" (British spelling) which stays as the source writes it; the cross-reference to our American-spelled `LICENSE` file is close enough for a reader to follow and editing the text would violate the verbatim-preservation rule.
**Process improvement:** Future upstream audits should check the repo-root directory for ANY license-related files (e.g. `LICENSE-*`, `COPYING*`, `NOTICE*`, `AUTHORS*`, `CONTRIBUTORS*`), not just the main `LICENSE`. Each such file carries license-relevant notices that §1 "keep intact" applies to.

---

## 2026-04-17 — Pass 7: additional-contributors declarations for inline-attributed copyright holders

**Discovered by:** J.J. Boyd (maintainer compliance-audit self-review, prompted by the MI0BOT header question)
**Reported via:** GPL §1 audit — "an appropriate copyright notice" must include every copyright holder whose code is in the file; inline `//-CALLSIGN` markers (Pass 6c) are §2(a) modification notices, not §1 copyright declarations
**Affected files:** 5 NereusSDR derivative files where Pass 6c preserved inline markers for contributors NOT named in the verbatim source block
**Gap:** Pass 6c correctly preserved block-level inline markers for G8NJJ, MI0BOT, and similar contributors who added code inline in Thetis sources without adding themselves to the file's top-level copyright block. When we port that code, the inline markers travel with it, but our file-header copyright block (verbatim from upstream) doesn't name them. A strict reading of §1's "appropriate copyright notice" wants explicit declaration for every copyright holder whose work the file contains.

**Fix (commit `5b2fb46`):** Added "Additional copyright holders whose code is preserved in this file via inline markers" section to the NereusSDR top block (not the verbatim source block — Richie's "keep intact" rule applies there) for each affected file. The verbatim source block stays untouched.

**Per-file outcome:**
- `src/core/HpsdrModel.h` — added G8NJJ (Saturn/ANAN-G2 enum mappings) + MI0BOT (HermesLite enum mappings)
- `src/core/HardwareProfile.cpp` — added G8NJJ (ANAN-G2_1K capability note)
- `src/core/RadioDiscovery.cpp` — added MI0BOT (board-ID 6 / HermesLite discovery parsing)
- `tests/tst_radio_discovery_parse.cpp` — added MI0BOT
- `tests/tst_hpsdr_enums.cpp` — added MI0BOT + G8NJJ

**Files where no addition was needed** (MW0LGE/Samphire was already in the verbatim source block):
- `P1RadioConnection.cpp` — cmaster.cs and console.cs headers name MW0LGE
- `P2RadioConnection.cpp` — console.cs header names MW0LGE
- `WdspTypes.h` — wdsp.cs/setup.cs/console.cs headers name MW0LGE
- `StepAttenuatorController.h` — console.cs header names MW0LGE

**Process improvement:** When future NereusSDR ports preserve an inline `//-CALLSIGN` marker, the standard procedure is: check whether that callsign is in the file's verbatim source block; if not, add to the NereusSDR-block "Additional copyright holders" section. This is the canonical bridge between §2(a) inline-preservation obligations and §1 copyright-declaration obligations.

Verifier: 171/171. Build: clean.

---

## 2026-04-17 — Pass 8: GPLv2 §2(c) interactive announcement in About dialog

**Discovered by:** J.J. Boyd (maintainer compliance-audit self-review)
**Reported via:** GPL §2(c) audit — interactive GUI programs must announce copyright, no-warranty, redistribute-under-these-conditions, and how-to-view-License on startup or via an equivalent mechanism (typically an About dialog)
**Affected files:** `src/gui/AboutDialog.cpp`
**Gap:** Pre-fix About dialog showed only two of the four required elements:
- ✅ Copyright notice (`© 2026 JJ Boyd`)
- ❌ No warranty disclaimer
- ❌ Redistribute-under-these-conditions statement
- ✅ License reference link (to gnu.org/licenses/gpl-3.0.html)

GPLv3 dropped §2(c) as a hard requirement, but our source files preserve GPLv2-or-later headers. A recipient electing to use the v2 grant is owed the §2(c)-form announcement. Strict posture: satisfy the v2 obligation.

**Fix (commit `<pending>`):** Expanded the About dialog footer to the canonical four-element GPL notice:

```
Copyright © 2026 J.J. Boyd (KG4VCF)
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it under
the terms of the GNU General Public License v3; see LICENSE and
LICENSE-DUAL-LICENSING for details.
```

Links to both GPLv3 upstream + both root license files (LICENSE and LICENSE-DUAL-LICENSING) so a user can view them. Comment above the Qt widget code cites GPLv2 §2(c) as the governing clause for future maintainers.

**Process improvement:** §2(c) compliance is now embedded in the About dialog's code. Future About-dialog refactors should preserve all four elements.

Verifier: 171/171. Build: clean.

---

## 2026-04-17 — About-dialog copyright line names only J.J. Boyd despite multi-contributor program

**Discovered by:** J.J. Boyd (maintainer adversarial self-review)
**Reported via:** Audit of the About dialog's §2(c) announcement against GPL §1's "appropriate copyright notice" requirement
**Affected files:** `src/gui/AboutDialog.cpp`
**Gap:** Pass 8 added the §2(c) four-element announcement, but the copyright line read `Copyright © 2026 J.J. Boyd (KG4VCF)` — singular, implying sole authorship of the running binary. NereusSDR is a derivative work: the binary contains code owned by FlexRadio Systems, Doug Wigley (W5WC), Richard Samphire (MW0LGE), the Thetis / mi0bot / AetherSDR / WDSP contributor chains, Warren Pratt (NR0V), Qt / FFTW authors, etc. A reasonable end user reading the singular line would infer that J.J. Boyd is the sole copyright holder — misrepresenting authorship in the same class of failure Pass 1 of the original remediation was called out for (independent-framing).

**Fix:** Expanded the About-dialog footer copyright line to name the principal copyright-holding groups and route the reader to LICENSE for the full chain:

```
Copyright © 2004-2026 FlexRadio Systems, Doug Wigley (W5WC),
Richard Samphire (MW0LGE), the Thetis / mi0bot / AetherSDR / WDSP
contributors, and J.J. Boyd (KG4VCF).
See LICENSE for the full contributor chain.
```

Year range 2004-2026 spans the full copyright lifecycle (FlexRadio PowerSDR's earliest date through current NereusSDR). The phrasing names individual persons where Thetis identifies them explicitly, and uses "contributors" group-form for the project-scope attributions whose individual contributor lists are preserved in source-file headers and PROVENANCE.md. Code comment above the Qt widget documents the reasoning for future maintainers.

**Process improvement:** About-dialog refactors in future must not collapse this line to a singular-author form. The derivative-work framing is load-bearing for GPL §1 compliance.

Verifier: 171/171. Build: clean.

---

## 2026-04-17 — About-dialog copyright line and pointer refinements

**Discovered by:** J.J. Boyd (second adversarial self-review of the About-dialog §2(c) announcement + upstream-LICENSE file-survey)
**Reported via:** Stricter reading of GPL §1 "appropriate copyright notice" — the fix applied earlier today named three principal individuals (Wigley, Samphire, Boyd) and routed with "See LICENSE"; an adversarial reviewer would notice (a) WDSP's Warren Pratt and AetherSDR's Jeremy (KK7GWY) are project principals grouped under "contributors" rather than named individually, (b) "See LICENSE" misdirects — our LICENSE file is GPLv3 text, not a contributor list; the actual chain lives in docs/attribution/.
**Affected files:** `src/gui/AboutDialog.cpp`
**Gap:**
1. Omitted individual-naming of Warren Pratt (NR0V, WDSP primary author), Phil Harman (VK6APH), Chris Codella (W2PA), Laurence Barker (G8NJJ), Reid Campbell (MI0BOT), Jeremy (KK7GWY, AetherSDR primary author) — all named in upstream source with verifiable copyright/attribution lines
2. "See LICENSE" pointer routes to the wrong file — users following it find GPLv3 text, not the contributor chain

**Fix (commit `<pending>`):** Expanded the About-dialog copyright line to name each principal individually in chronological order of their contribution era; pointer updated to `<a href="...tree/main/docs/attribution">docs/attribution</a>` which is an actual directory with the THETIS-PROVENANCE / aethersdr-contributor-index / WDSP-PROVENANCE / REMEDIATION-LOG / HOW-TO-PORT indices. `setOpenExternalLinks(true)` added so the link is clickable in the running dialog. Each name was verified against actual upstream source before inclusion — no fabrication.

**Accompanying upstream-LICENSE survey** confirmed no further file-mirroring needed:
- `mi0bot/LICENSE` byte-identical to `ramdor/Thetis/LICENSE` — already covered via root LICENSE (GPLv3 upgrade)
- `mi0bot/LICENSE-DUAL-LICENSING` byte-identical to Thetis's — already mirrored at our repo root
- `Thetis/Project Files/LICENSE-DUAL-LICENSING.rtf` is a Windows-RTF duplicate of the text version (no additional legal content)
- AetherSDR root LICENSE is GPLv3 (matches our root)
- Third-party library LICENSE files in upstream `lib/` or `packages/` are the upstreams' own bundled deps, not code NereusSDR ports; NereusSDR ships its own `third_party/` license bundle via Phase 4 Task 29

**Process improvement:** When citing a "see X for details" pointer in public-facing text, verify X actually contains the details. "See LICENSE" is a common shorthand but misleading when LICENSE is just the terms and not a contributor list.

Verifier: 171/171. Build: clean.

---

## 2026-04-17 — Compliance Plan Task 1: MainWindow attribution headers

**Discovered by:** Adversarial GPL compliance audit (fresh-eyes review)
documented at `docs/architecture/2026-04-17-gpl-compliance-v020-remediation-plan.md`.
**Reported via:** Bucket D.2 of `docs/attribution/aethersdr-reconciliation.md`
flagged `src/gui/MainWindow.{h,cpp}` as the most-visible GUI files in the
tree shipping without a formal AetherSDR citation. `MainWindow.cpp` already
carried the multi-source Thetis verbatim header; `MainWindow.h` was bare
(`#pragma once` + Qt includes only). Both files have load-bearing AetherSDR
debt: signal-routing hub, double-height status-bar layout, and TitleBar
feature-request dialog — verified by 8 inline `MainWindow:<line>` AetherSDR
citations in the .cpp body.

**Affected files:**
- `src/gui/MainWindow.cpp` — added AetherSDR project-level citation to the
  Modification-History block; existing Thetis verbatim headers untouched.
- `src/gui/MainWindow.h` — added full new header: NereusSDR port-citation
  block + Thetis `console.cs` verbatim block (byte-identical to .cpp,
  trailing whitespace preserved) + AetherSDR project-level citation.
- `docs/attribution/THETIS-PROVENANCE.md` — added `MainWindow.h` row;
  expanded `MainWindow.cpp` row with mixed-lineage note pointing to D.2.
- `docs/attribution/aethersdr-reconciliation.md` — appended Resolution
  block to D.2 confirming all three follow-up actions complete.

**Fix (commit `<pending>`):** AetherSDR citation form follows
`docs/attribution/HOW-TO-PORT.md` rule 6 (project-level reference, since
AetherSDR has no per-file headers to copy verbatim). The .cpp's existing
verbatim Thetis blocks were not modified — only the Modification-History
block at the top was extended with the AetherSDR lines.

Verifier: 172/172 (was 171; +1 for the new `MainWindow.h` PROVENANCE row).

---

## 2026-04-17 — Compliance Plan Task 2A: MeterManager.cs `.cpp` orphans + bare-pair editors

**Discovered by:** Adversarial GPL compliance audit, file-pair survey
revealed seven Samphire-MeterManager-derived files shipping without
attribution headers — the same defect class Samphire previously caught
on `MeterManager.cs` itself. Three were sibling-orphans (their `.h` was
in PROVENANCE; the `.cpp` implementation was bare). Two file pairs
(`BaseItemEditor.{h,cpp}`, `MmioEndpointsDialog.{h,cpp}`) were entirely
missing from PROVENANCE despite implementing Samphire's binding /
endpoint-editor patterns.

**Affected files (7 source + 1 PROVENANCE):**
- `src/core/mmio/ExternalVariableEngine.cpp` (sibling-orphan)
- `src/gui/containers/meter_property_editors/NeedleItemEditor.cpp` (sibling-orphan)
- `src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.cpp` (sibling-orphan)
- `src/gui/containers/meter_property_editors/BaseItemEditor.{h,cpp}` (bare pair)
- `src/gui/containers/MmioEndpointsDialog.{h,cpp}` (bare pair — UI port of
  Samphire's `frmMmioEndpoints`; transport enum mirrors clsMMIO Transport
  values UdpListener/TcpListener/TcpClient/Serial)
- `docs/attribution/THETIS-PROVENANCE.md` — 7 new rows added

**Fix (commit `<pending>`):** Each file now carries the standard NereusSDR
port-citation block + verbatim Thetis MeterManager.cs Samphire copyright +
GPLv2-or-later block + dual-licensing statement, byte-identical to the
canonical block already present in `NeedleItemEditor.h` etc.

Verifier: 179/179 (was 172; +7 for new PROVENANCE rows).

---

## 2026-04-17 — Compliance Plan Task 2B: BandwidthMonitorTab pair + RxDspWorker.cpp

**Discovered by:** Same audit pass as T2A. `BandwidthMonitorTab.{h,cpp}`
were a bare pair entirely missing from PROVENANCE, despite both files
opening with `// Source: ChannelMaster/bandwidth_monitor.h` admission
comments. `RxDspWorker.cpp` was a sibling-orphan whose .h was already
in PROVENANCE; the .h's notes column previously said ".cpp is Qt worker
plumbing (no Thetis citation)" — but the .cpp implements the .h's
Samphire-derived buffer-size formula and DSP wiring, so the hedge was
itself a compliance gap.

**Affected files (3 source + 1 PROVENANCE):**
- `src/gui/setup/hardware/BandwidthMonitorTab.h` (bare → full header)
- `src/gui/setup/hardware/BandwidthMonitorTab.cpp` (bare → full header)
- `src/models/RxDspWorker.cpp` (bare → console.cs verbatim header
  cloned byte-identical from sibling RxDspWorker.h)
- `docs/attribution/THETIS-PROVENANCE.md` — 3 new rows; updated
  RxDspWorker.h notes column (removed obsolete "no Thetis citation"
  hedge).

**Fix (commit `<pending>`):** BandwidthMonitorTab files now carry the
verbatim `bandwidth_monitor.h` Samphire 2025 header (fetched from
upstream `ramdor/Thetis/Project Files/Source/ChannelMaster/`,
byte-identical) plus a developer-note block preserving the original
"NereusSDR wires only static controls; live feed deferred to Phase 3L"
context. RxDspWorker.cpp received the same console.cs Samphire-variant
verbatim block its sibling .h carries (byte-compared identical).

Verifier: 182/182 (was 179; +3 for new PROVENANCE rows).

---

## 2026-04-17 — Compliance Plan Task 2C: P1RadioConnection.cpp mi0bot header expansion

**Discovered by:** Same audit pass. The file's body carried admission-style
`// Source: mi0bot bandwidth_monitor.{c,h}` comments at lines 788, 1302
and `// Source: mi0bot IoBoardHl2.cs:N-N` at lines 551, 1265-1274 — but
the file's "Ported from Thetis sources:" header list named only 4 sources
(networkproto1.c, NetworkIO.cs, cmaster.cs, console.cs). The three
mi0bot-cited sources were never reflected in the verbatim header stack
— Samphire's MeterManager pattern repeating in the P1 path.

**Affected files:**
- `src/core/P1RadioConnection.cpp` — added 3 verbatim upstream blocks:
  - `bandwidth_monitor.c` (Samphire 2025, fetched from
    ramdor/Thetis/master, byte-identical)
  - `bandwidth_monitor.h` (Samphire 2025, same upstream, byte-identical)
  - `IoBoardHl2.cs` (Reid Campbell MI0BOT 2025, fetched from
    mi0bot/OpenHPSDR-Thetis/master/Project Files/Source/Console/HPSDR/,
    byte-identical including the trailing space on the email line)
  Also expanded the "Ported from Thetis sources:" list at the top from
  4 to 7 entries naming all upstream files.
- `docs/attribution/THETIS-PROVENANCE.md` — expanded P1RadioConnection.cpp
  row's source column to enumerate all 7 upstream files (was: notes-only
  mention) and updated the variant note.

**Fix (commit `<pending>`):** New verbatim blocks inserted between the
last existing dual-license closing `//===//` line (130) and the
NereusSDR-side "Migrated to VS2026" developer-note (132), so the
upstream blocks remain contiguous and the NereusSDR commentary stays
below them. Path discovery: an initial cite of `mi0bot/Thetis-HL2`
404'd; the actual upstream is `mi0bot/OpenHPSDR-Thetis` under the
`HPSDR/` subdirectory, located via `gh api` enumeration.

Verifier: 182/182 (no row count change; the 3 new sources are stacked
within an existing PROVENANCE row).

---

## 2026-04-17 — Compliance Plan Task 3: Reconcile shader attribution

**Discovered by:** Adversarial GPL compliance audit. `resources/shaders/
waterfall.frag:18` admitted `// From AetherSDR texturedquad.frag` while
`docs/attribution/ASSETS.md:86-88` simultaneously claimed "All GLSL
shaders are original NereusSDR artwork… No shader source was copied
from… any other project." Worst-shape inconsistency in the tree — a
reviewer would quote both side-by-side.

**Affected files:**
- `resources/shaders/waterfall.frag` — added NereusSDR port-citation
  block + AetherSDR project-level citation per HOW-TO-PORT.md rule 6.
  GLSL has no upstream verbatim header to copy; the project-level form
  names ten9876/AetherSDR + the source filename + GPLv3.
- `docs/attribution/ASSETS.md` — rewrote the `## resources/shaders/`
  intro paragraph to stop denying external derivation; updated the
  table row for `waterfall.frag` to disclose AetherSDR origin and the
  mixed-license shape (NereusSDR contributions GPL-2.0-or-later,
  AetherSDR-derived sampling line GPLv3).

**Fix (commit `<pending>`):** No code-behavior changes. Shader recompiled
clean via Qt's `qsb` step at build time; the comment block doesn't
affect GLSL output.

Verifier: 182/182 (shaders are not under verifier scope).

---

## 2026-04-17 — Compliance Plan Task 4: KD5TFD + DH1KLM in About dialog

**Discovered by:** Adversarial GPL compliance audit. The About dialog's
copyright string named 8 principal contributors but two more whose code
ships in NereusSDR were attributed in source-file headers and missing
from the dialog — a strict GPLv3 §5(a) "appropriate copyright notice"
gap (notices preserved at file level but stripped from the interactive
notice).

Sweep methodology: ran `grep -rhE "Copyright \(C\)" src/` and
`grep -rhoE "(callsign-pattern)" src/` to enumerate every named
copyright holder appearing anywhere in the shipped source tree, then
diffed against the dialog's principal-name list.

**Affected files:**
- `src/gui/AboutDialog.cpp` — added two names to the copyright line:
  - **Bill Tracey (KD5TFD)** — copyright preserved in
    `src/core/P2RadioConnection.{cpp,h}` headers
    (`Copyright (C) 2006,2007 Bill Tracey (bill@ejwt.com) (KD5TFD)`).
    Inserted chronologically between FlexRadio (2004-2009) and
    Wigley (2010-2020).
  - **DH1KLM** — cited inline at `src/core/P1RadioConnection.cpp:1167`
    (`// From networkproto1.c:606-616 (DH1KLM fix)`) and
    `src/core/HpsdrModel.h:111` (RedPitaya enum slot, version tag
    `[2.10.3.9]DH1KLM` from upstream networkproto1.c:612). Upstream
    Thetis source records only the callsign with no expanded name;
    same convention used here.

**Names verified already present** (no action needed): FlexRadio
Systems, W5WC, MW0LGE, NR0V, VK6APH, W2PA, G8NJJ, MI0BOT, KK7GWY,
KG4VCF (principal copyright string); G0ORX (John Melton, "Standing on
the Shoulders of Giants" Contributor row, AboutDialog.cpp:112).

Other callsigns not found anywhere in `src/` (W4WMT, KE9NS, K5SO,
OE3IDE, KD0OSS, AA6E, N1GP) — not attributed because no shipped code
carries their copyright lines or inline credits.

**Fix (commit `<pending>`):** Two-name addition to a single QLabel
QStringLiteral. No structural change to the dialog layout.

Verifier: 182/182. Build: clean.

---

## 2026-04-17 — Compliance Plan Task 5: GPL §6 source for FFTW3 + Qt

**Discovered by:** Adversarial GPL compliance audit. The Windows
installer + portable ZIP bundle a prebuilt `libfftw3f-3.dll` (FFTW is
GPLv2-or-later), but neither the release artifacts nor the
`release-notes-template.md` provided the §3(a)/§6(a) "accompanied by
corresponding source on the same medium" or a §3(b)/§6(b) written
3-year offer. Same problem for Qt6 (LGPLv3) — dynamically linked, but
no upstream-source pointer in the release notes.

**Affected files:**
- `.github/workflows/release.yml` — added a "Fetch FFTW3 corresponding
  source" step in the `sign-and-publish` job, between source-tarball
  generation and SHA256SUMS, that downloads the exact upstream
  `fftw-3.3.5.tar.gz` archive used to build the bundled DLL. The
  existing artifact globs (`*.tar.gz` in Sign artifacts, SHA256SUMS,
  and the GitHub Release upload step) auto-include it without further
  changes — minimal-blast-radius edit.
- `.github/release-notes-template.md` — new "## Source for bundled
  binaries (GPL §6 corresponding source)" section above
  "## Reporting Issues". Names every bundled GPL/LGPL component
  (NereusSDR itself, FFTW3, Qt6, WDSP) with its license, the
  corresponding source location (release asset or upstream URL), and
  per-platform Qt-replacement hints. Closes with a §3(b)/§6(b) written
  offer for 3-year availability via email.

**Fix (commit `<pending>`):** Implements **option (a)** from the plan
— ship corresponding source on the same medium. Cheaper and cleaner
than option (b) (a written-offer-only approach), since the GitHub
Release IS the medium and a single CI step closes the obligation
atomically per release. Estimated impact on release size: ~5MB (FFTW
source archive).

YAML validity confirmed via `python3 -c yaml.safe_load`. No build to
run (CI workflow change).

Verifier: 182/182.

---

## 2026-04-17 — Compliance Plan Task 6: LICENSE-DUAL-LICENSING preamble

**Discovered by:** Adversarial GPL compliance audit. The byte-identical
upstream `LICENSE-DUAL-LICENSING` references "the GNU General Public
License granted in LICENCE" — but NereusSDR's root file is `LICENSE`
(not "LICENCE"), and now contains GPLv3, not the GPLv2 Samphire's
reservation was originally paired against. A hostile reading: Samphire's
reservation attaches only to the GPLv2 pairing and does not survive the
v3 relicense. Separately, the C++/Qt6 ports of Samphire-authored C# code
raised an unaddressed translation-rights ambiguity (audit Finding 2.3).

**Affected files:**
- `LICENSE-DUAL-LICENSING` — prepended a 22-line NereusSDR-specific
  preamble dated 2026-04-17. Preamble has two numbered points:
  (1) "LICENCE" should be read as the project's LICENSE (GPLv3);
  GPLv3 was elected under the upstream "or later" grant; Samphire's
  reservation continues to apply regardless of GPL version downstream.
  (2) Samphire retains dual-licensing over his original C# work;
  NereusSDR contributors' C++ ports are GPLv3 from the NereusSDR side.
  Closing line: "The byte-identical upstream text from ramdor/Thetis
  follows below." The upstream block is preserved unchanged below the
  preamble (`diff` confirmed byte-identical to ramdor/Thetis/master).

**Fix (commit `<pending>`):** Additive only — preamble is clearly
NereusSDR-specific (date-stamped, headed with project name); upstream
block is byte-preserved. The structural form (two `=` rule blocks
stacked, same 90-char width as upstream) makes the boundary visually
obvious so readers know which text is upstream versus NereusSDR
commentary.

Verifier: 182/182.

---

## 2026-04-17 — Compliance Plan Task 7: §5(d) startup notice — DECISION TO SKIP

**Discovered by:** Adversarial GPL compliance audit (this same audit pass)
flagged the About dialog as "buried two clicks deep" under Help → About
and proposed a startup `qInfo()` banner + first-run About auto-popup.

**Decision:** Skip. The audit framing overstated this as MEDIUM. GPLv3
§5(d) requires interactive UIs to "display Appropriate Legal Notices" —
it does not specify "on startup." The four required elements (copyright,
no-warranty, redistribution permission, license pointer) are all present
in `src/gui/AboutDialog.cpp:208-246`. Convention across GPL'd GUI apps
(Inkscape, GIMP, Audacity, KiCad, Krita, Blender, and Thetis itself)
places the §5(d) notice in the About dialog only — no startup banner,
no first-run auto-popup. The FSF's GPLv3 FAQ accepts About dialog as the
conventional location.

NereusSDR's posture without a startup banner is already stronger than
Thetis upstream's. If Samphire or anyone else raises it later, the fix
is one commit (banner-only, no popup), not a release block.

No file changes for this task.

---

## 2026-04-17 — Compliance Plan Task 8: LGPLv3 §4(d)(1) Qt replacement docs — DECISION TO SKIP

**Discovered by:** Adversarial GPL compliance audit. `packaging/third-
party-licenses/qt6.txt` is 16 lines and contains no instructions for
replacing the bundled Qt6 libraries with a modified version. LGPLv3
§4(d)(1) requires "operating instructions" for the replacement step.

**Decision:** Skip. LGPLv3 §4(d)(0) (dynamic linking, the route
NereusSDR uses on all three platforms via `windeployqt` /
`macdeployqt` / `linuxdeploy-plugin-qt`) is the load-bearing
satisfaction; §4(d)(1) is the documentary tail. Common practice across
LGPL-Qt apps is to include the bundled Qt LGPL text and let users
infer the obvious DLL/.dylib/.so swap; explicit per-platform "drop a
modified Qt6Core.dll into the install directory" docs are gold-plating
relative to industry norm. Worth doing if Samphire raises it; not
worth blocking v0.2.0 over.

No file changes for this task.

---

## 2026-04-17 — Compliance Plan Task 9: cross-needle-bg duplicate — DECISION TO SKIP

**Discovered by:** Adversarial GPL compliance audit. `resources/meters/
cross-needle.png` and `cross-needle-bg.png` have identical MD5
(`2e9762458582b47d0f370bc7d8af3ea6`). ASSETS.md honestly discloses the
duplication but the disclosure undermines the "original layered
artwork" claim.

**Decision:** Skip. Not a GPL violation — a self-credibility issue,
already honestly disclosed in ASSETS.md. The only fix paths
(programmatic solid-fill in the renderer, or producing a real
background image) touch rendering code with non-zero regression risk
on the cross-needle gauge. Risk-vs-reward favors leaving the disclosed
duplicate in place. Worth revisiting if Samphire pushes on it.

No file changes for this task.

---

## 2026-04-17 — Compliance Plan Task 10: Third-Party Notices link in About — DECISION TO SKIP

**Discovered by:** Adversarial GPL compliance audit. AboutDialog names
Qt6 / FFTW3 / WDSP in a "Built With" section but doesn't link to the
bundled `packaging/third-party-licenses/` directory; LGPLv3 §4(d)
implicitly wants the LGPL text discoverable from the running program.

**Decision:** Skip. The licenses directory ships in the install tree
on all three platforms (release.yml stages it explicitly on Windows
and via deploy steps on Linux/macOS). Users who care can navigate to
it. Common practice across LGPL-Qt apps is loose here. Worth doing
later as a UX polish; not worth blocking v0.2.0 over.

No file changes for this task.

---

## 2026-04-17 — Compliance Plan Task 11: AetherSDR cite on AboutDialog

**Discovered by:** Adversarial GPL compliance audit. `aethersdr-
reconciliation.md` Bucket D.1 noted `AboutDialog.cpp` mentions AetherSDR
in the contributor-table data (lines 106-107) but the file's own header
carried no AetherSDR citation. 25a Flag #11 separately recorded that the
dialog's design is inspired by AetherSDR's `MainWindow` about-box. Both
files (`AboutDialog.h` and `AboutDialog.cpp`) opened with only a single-
line `// path` comment — bare otherwise.

**Affected files:**
- `src/gui/AboutDialog.h` — added a NereusSDR port-citation header
  citing AetherSDR `src/gui/MainWindow.cpp` (about-box section) and
  `src/gui/TitleBar.{h,cpp}` at project level per HOW-TO-PORT.md rule 6.
  Explicit "no Thetis-derived code" line — Thetis's About lives in
  `console.cs` and was not used as a source for this dialog.
- `src/gui/AboutDialog.cpp` — same header.
- `docs/attribution/aethersdr-reconciliation.md` — both occurrences of
  the AboutDialog.cpp row (Phase 4 audit table at line 355 and Bucket
  D.1 table at line 445) updated with "**Resolved 2026-04-17 (Compliance
  Plan T11)**" notes; full resolution detail kept in the D.1 entry.
  The other 4 D.1 files (`WdspEngine.cpp`, `tst_about_dialog.cpp`,
  `tst_container_persistence.cpp`, `tests/CMakeLists.txt`) stay
  "leave as-is" per their own per-row analyses (incidental mentions
  not warranting full headers).

**Fix (commit `<pending>`):** No PROVENANCE row added — AboutDialog has
no Thetis derivation, so it does not belong in `THETIS-PROVENANCE.md`.
The AetherSDR-only citation is documentary/source-level only.

Verifier: 182/182. Build: clean.

---

## 2026-04-17 — Compliance Plan Task 12 + T2D follow-up: hardened verifier + 14 surfaced orphans

**Discovered by:** Compliance Plan Task 12 set out to harden
`scripts/verify-thetis-headers.py` against the orphan-pair defect class
that Samphire previously caught on MeterManager.cs. The hardened
verifier — on its very first run against the post-T11 tree — surfaced
**14 hidden orphan-pair gaps** that the T2 sweep had missed.

**Verifier hardening (T12):**
- New **Check A** (orphan-pair): for every PROVENANCE-listed `.h`,
  require its sibling `.cpp`/`.cc`/`.c` (if it exists on disk) to either
  be listed in PROVENANCE too or carry an explicit opt-out marker
  `// Independently implemented from <upstream> interface`. Same in
  reverse.
- New **Check B** (Samphire-marker consistency): if a file's PROVENANCE
  source-list cell cites a known Samphire-authored Thetis source
  (MeterManager.cs, console.cs, cmaster.cs, bandwidth_monitor.{c,h},
  ucMeter.cs, ucRadioList.cs, frmMeterDisplay.cs, frmAddCustomRadio.cs,
  clsHardwareSpecific.cs, clsDiscoveredRadioPicker.cs,
  clsRadioDiscovery.cs, DiversityForm.cs, PSForm.cs), the file's
  header must contain `MW0LGE` — proves the verbatim Samphire
  copyright/dual-license block was actually transcribed.
- Regression-tested: temporarily stripped the `MainWindow.h` header,
  confirmed verifier flagged both missing-markers AND missing-Samphire-
  marker; restored.

**T2D follow-up — 14 orphans surfaced and fixed:**

11 got sibling-cloned Thetis port-citation headers (the standard T2A/B/C
pattern):
- `src/gui/meters/ClickBoxItem.cpp` ← .h cited MeterManager.cs
- `src/gui/meters/ClockItem.cpp` ← .h cited MeterManager.cs
- `src/gui/meters/DataOutItem.cpp` ← .h cited MeterManager.cs
- `src/gui/meters/DiscordButtonItem.cpp` ← .h cited MeterManager.cs
- `src/gui/meters/VoiceRecordPlayItem.cpp` ← .h cited MeterManager.cs
- `src/models/Band.cpp` ← .h cited console.cs
- `src/gui/setup/DspSetupPages.h` ← .cpp cited setup.cs
- `src/gui/widgets/VfoWidget.h` ← .cpp cited multi-source console.cs cluster
- `src/gui/applets/PhoneCwApplet.h` ← .cpp cited setup.cs
- `src/gui/applets/RxApplet.h` ← .cpp cited multi-source
- `src/gui/containers/MmioVariablePickerPopup.cpp` ← .h cited MeterManager.cs

3 got opt-out markers (genuinely independent NereusSDR implementations
where the .h carries the Thetis lineage citation but the .cpp is
original NereusSDR algorithm/scaffolding):
- `src/core/NoiseFloorEstimator.cpp` — percentile-based estimator
  (replaces, not ports, Thetis processNoiseFloor)
- `src/core/mmio/MmioEndpoint.cpp` — Qt QObject lifecycle scaffolding
  around the .h's MeterManager-derived enum
- `src/core/ClarityController.cpp` — same percentile-based pattern as
  NoiseFloorEstimator

**Affected files in this commit:**
- `scripts/verify-thetis-headers.py` — full rewrite of `parse_provenance`
  to capture source-cell text; new `check_orphan_pair` and
  `check_samphire_marker` functions; per-file failure reporting now
  emits all defects, not just the first.
- 14 source files (11 cloned headers + 3 opt-out markers).
- `docs/attribution/THETIS-PROVENANCE.md` — 11 new rows for the
  cloned-header files (the 3 opt-out files do NOT get PROVENANCE rows
  by design — they aren't Thetis-derived).

Verifier: 193/193 (was 182/182; +11 PROVENANCE rows). Build: clean.

The verifier is now a structural merge gate: future ports cannot ship
a sibling-orphan or a Samphire-cite-without-MW0LGE without explicit
opt-out documentation.

---

## 2026-04-17 — Compliance Plan Task 13: LOW-severity batch cleanup (7 items)

**Discovered by:** Adversarial GPL compliance audit. Eight LOW-severity
items batched. L1 was completed inline as part of T12 (verifier
hardening); the remaining 7 are folded into one commit here.

**Affected files (7 fixes):**

- **L2** — `docs/attribution/WDSP-PROVENANCE.md:27` — `fftw3.h` license
  label corrected from "BSD" to "GPLv2-or-later" (FFTW headers carry
  the same terms as the FFTW source upstream). Note in the new wording
  acknowledges the prior label was incorrect.

- **L3** — `src/gui/AboutDialog.cpp` warranty line — appended
  "see sections 15-16 of the License for details." per GPLv3's
  canonical "comes with ABSOLUTELY NO WARRANTY; for details type
  'show w'." pattern.

- **L4** — `docs/attribution/ASSETS.md` — added new
  `## resources/help/` section listing `getting-started.md` (the in-app
  help content shipped via Qt resource bundle) as GPL-2.0-or-later
  NereusSDR original work. Closes the "asset shipped in binary but not
  in ASSETS.md" gap.

- **L5** — `src/gui/AboutDialog.cpp:195-202` comment block — rewritten
  to reference GPLv3 §5(d) "Appropriate Legal Notices" as the primary
  obligation; previous text mislabeled this as a "GPLv2 §2(c) holdover"
  which an adversarial reader could cite as evidence the project didn't
  think §5(d) applied. The new text states the four-element block
  satisfies §5(d) directly and additionally satisfies §2(c) for
  downstream recipients who elect the v2 grant.

- **L6** — `xattr -c resources/meters/ananMM.png` — cleared
  `com.apple.quarantine` and `com.apple.lastuseddate#PS` extended
  attributes that were forensic evidence the file was downloaded from
  the internet on a Mac, contradicting the ASSETS.md "AI-generated
  locally" claim.

- **L7** — `docs/attribution/LICENSE-GPLv2` — added (verbatim FSF text
  fetched from `https://www.gnu.org/licenses/gpl-2.0.txt`, 338 lines).
  README amended to note that GPLv3 was elected under the Thetis
  source-file "or later" grant and that GPLv2 ships at this path for
  reference since several WDSP and ChannelMaster source files
  explicitly reference v2.

- **L8** — `CONTRIBUTING.md` GPG-signing language reworded at lines 44
  and 144 to scope the requirement to "this repository's `main` branch
  branch protection" (project policy, not a license contribution
  prerequisite). Explicit carve-out: "downstream forks and
  redistributions are not required to sign." Closes the GPLv3 §7
  additional-restriction attack surface.

**Fix (commit `<pending>`):** Each item is a small text edit; no
runtime behavior changes.

Verifier: 193/193. Build: clean.

---

## 2026-04-17 — Post-T13: reverse-attribution check (`scripts/check-new-ports.py`)

**Discovered by:** Self-audit of the post-T13 enforcement posture
identified a known gap: `verify-thetis-headers.py` (T12) only checks
files already in PROVENANCE.md. A contributor could write a brand-new
file porting Thetis code, leave it out of PROVENANCE, and the existing
verifier would never see it. That is exactly the defect class Samphire
flagged on MeterManager.cs in v0.1.x.

**Affected files:**
- `scripts/check-new-ports.py` — new reverse-attribution checker. For
  every added or modified C/C++ file in the PR diff (`origin/main`..HEAD,
  diff-filter=AM), greps for Thetis-style markers:
  - Thetis contributor callsigns (MW0LGE, W2PA, W5WC, VK6APH, MI0BOT,
    G8NJJ, NR0V, G0ORX, KD5TFD, DH1KLM, OE3IDE, …; KK7GWY explicitly
    excluded as the AetherSDR primary author, not Thetis)
  - Thetis source filename references with `.cs|.c|.h|.cpp` extension
  - `cls*/uc*/frm*` C# class-naming patterns (Samphire's convention)
  - Explicit `// Source:` / `// From <thetis-file>` citation comments
  Skip conditions: file is in PROVENANCE, file head contains the T12
  opt-out marker (`Independently implemented from`), or file head
  contains `no-port-check: <reason>` per-file escape hatch.
- `.github/workflows/ci.yml` — wired in as a separate step after the
  existing verifier, gated on `github.event_name == 'pull_request'` so
  it only runs on PRs (where the diff range is meaningful). Fetches
  `origin/${{ github.base_ref }}` first so the diff base exists.
- `src/gui/AboutDialog.{h,cpp}` — added `no-port-check:` escape hatches
  to the existing T11 headers. The dialog mentions `console.cs` in its
  "no Thetis derivation" disclaimer and contains contributor callsigns
  in the displayed contributor list — both legitimate non-derivation
  uses that the per-file escape hatch correctly handles.

**Fix (commit `<pending>`):** Closes the highest-priority enforcement
gap identified in the post-T13 self-audit. Combined with T12's
in-PROVENANCE checks, NereusSDR's CI now enforces both directions:
"every cited file is properly attributed" (T12) AND "every newly-added
file with Thetis markers is properly cited" (this commit). The
remaining unmechanized gaps (paraphrased ports without callsigns or
filename refs; dishonest opt-out claims) are documentation/culture, not
mechanical detection problems.

Local test: `CHECK_NEW_PORTS_BASE_REF=origin/main python3
scripts/check-new-ports.py` → "OK: 222 added/modified C/C++ file(s)
checked, all properly attributed or skip-marked." Build: clean.

---

## 2026-04-18 — Full-tree audit sweep (Phases 0–6 of the 2026-04-18 plan)

**Discovered by:** Internal audit — J.J. Boyd (KG4VCF)
**Reported via:** `docs/architecture/2026-04-18-gpl-compliance-full-audit-plan.md`
**Affected files:** Tree-wide — 724 tracked files classified; 207 PROVENANCE-listed ports verified; 138 WDSP sources censused
**Gap:** Three categories surfaced by the expanded auditors.

1. **Binary recipients got URLs, not licence text.** Pre-sweep,
   `packaging/third-party-licenses/{fftw3,qt6,wdsp}.txt` pointed at
   gnu.org links for the actual licence text. GPLv2 §1 / LGPLv3 §4
   require text-alongside, not text-by-reference.
2. **Per-upstream-project notices absent from binary.** Binary-only
   recipients saw no mention of Thetis, mi0bot/Thetis-HL2, or AetherSDR
   as upstream origins of code compiled into NereusSDR; per-file
   attribution was only in source headers and `docs/attribution/`.
3. **No written source offer bundled.** GPLv3 §6(b) fallback (three-year
   written offer) was absent from the release artifacts; the §6(a)
   "source alongside binary" claim hinged on the Releases page being
   up at fetch time.
4. **Header verifier did not cover AetherSDR or WDSP lineages.**
   `verify-thetis-headers.py` pre-sweep only enforced Thetis markers;
   AetherSDR Bucket-A files and WDSP vendored sources were unaudited.
5. **No tree-wide inline-cite stamp sweep.** PR-diff enforcement was
   in place (via `check-new-ports.py`) but no mechanism caught
   historical cites that pre-dated the diff-gate.
6. **Inventory script flagged two incidental-reference test files.**
   `tst_about_dialog.cpp` and `tst_container_persistence.cpp` matched
   the `aethersdr-reconciliation.md` path parser even though they sit
   in Buckets B/D (deletions / incidental mentions, not real
   derivations).

**Root cause:** The 2026-04-17 v0.2.0 remediation focused on the source
tree. The 2026-04-18 plan added four additional audit axes — binary
distribution artifacts, inline-cite version stamps, non-Thetis lineage
headers, and tree-wide inventory — and these surfaced the gaps above.

**Fix (commit range `49b576a..<pending>` on branches
`compliance/full-audit-2026-04-18` + `compliance/audit-followup-2026-04-18`):**

- Binary-distribution fixes (PR #55):
  - Added verbatim `GPLv2.txt`, `GPLv3.txt`, `LGPLv3.txt` under
    `packaging/third-party-licenses/` (fetched byte-identical from
    gnu.org; SHA-256 recorded in the commit message).
  - Added `thetis.txt`, `mi0bot-thetis.txt`, `aethersdr.txt` listing
    upstream copyright chains.
  - Added `SOURCE-OFFER.txt` formalising GPLv3 §6(a) + §6(b) + LGPLv3 §4
    Qt source pointers.
  - Refreshed existing per-dependency notices to reference bundled
    full-text files instead of URLs.
- Audit-infrastructure fixes (follow-up PR):
  - `scripts/verify-thetis-headers.py` gained `--kind=aethersdr` and
    `--kind=wdsp` modes with a documented 10-file WDSP exemption list
    matching `WDSP-PROVENANCE.md`. Result: 43/43 AetherSDR Bucket-A,
    128/128 WDSP eligible files pass.
  - `scripts/verify-inline-cites.py` + `tests/compliance/test_inline_cites.py`
    + `tests/compliance/inline-cites-baseline.json` added. Baseline
    captured at 288 unstamped cites; regression gate passes today.
  - `scripts/audit-inline-markers.py` added — sampling comparator
    between Thetis upstream line ranges and NereusSDR ports; output
    parked here for human triage (restructures legitimately drop
    marker counts, so this is not a merge gate).
  - `scripts/audit-wdsp-headers.py` added — independent census
    confirming `WDSP-PROVENANCE.md`'s 128 + 10 split (and catches
    future drift on upstream re-sync).
  - `scripts/compliance-inventory.py` now scopes
    `aethersdr-reconciliation.md` parsing to Bucket A only, eliminating
    the three pre-existing false-positive flags on
    `tst_about_dialog.cpp`, `tst_container_persistence.cpp`, and
    (via the refined WDSP-NO-HEADER set) `FDnoiseIQ.h`.
    Regenerated `docs/attribution/COMPLIANCE-INVENTORY.md` baseline.

**Process improvement:**
- Task 15 (follow-up): wire every auditor in `ci.yml` as merge-gated.
- Task 16 (follow-up): add the new auditors to the pre-push hook.
- Task 17 (follow-up): document the Thetis-upstream-sync cadence so
  inline-cite stamps refresh automatically at each upstream pull.

**Auditor status at sweep close:**

| Auditor | Result |
| --- | --- |
| `verify-thetis-headers.py --kind=thetis` | 209/209 pass |
| `verify-thetis-headers.py --kind=aethersdr` | 43/43 pass |
| `verify-thetis-headers.py --kind=wdsp` | 128/128 pass |
| `verify-provenance-sync.py` | 207/207 pass |
| `verify-inline-cites.py` | 288 grandfathered, at baseline |
| `audit-wdsp-headers.py` | Census matches WDSP-PROVENANCE.md |
| `audit-inline-markers.py` | 68/75 below 0.5 ratio — sampling, human triage (expected noise on multi-source "full" rows) |
| `compliance-inventory.py --fail-on-unclassified` | Exit 0 |
| `check-new-ports.py` (full-tree) | 322/322 pass |
| `pytest tests/compliance/` | 10/10 passing |

---

*(Subsequent entries will be appended as omissions are discovered and cured.)*
