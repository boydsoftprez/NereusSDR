# Phase 3L HL2 Visibility — Lean Implementation Plan

**Status:** Plan; ready to execute 2026-04-30.
**Design doc:** [`phase3l-hl2-visibility-design.md`](phase3l-hl2-visibility-design.md)
**Companion plan:** [`phase-hl2-end-to-end-plan.md`](phase-hl2-end-to-end-plan.md) (wire encoding — separate)
**Branch:** `test/hl2-hardware-2026-04-29` (currently 2 commits behind `origin/main`).

This plan covers **sequencing + source-first gates** for the three substantive tasks (#22-#24). Small targeted tasks (#25 relabels, #26 signed ATT) and the WIP carry-forward have no sequencing risk and are tracked in TaskList only.

---

## 1. PR sequencing decision

Two candidate strategies, recommendation first:

**A. Single PR (recommended).** Stack all 7 work items into one HL2-visibility PR.

- Pro: One review cycle, one rebase, all "make HL2 work like mi0bot" lands as a coherent unit.
- Pro: WIP carry-forward bug fixes live alongside the new surfaces they enable (e.g., live OC LED indicator is meaningless without the new HL2 Options tab consuming it).
- Pro: Matches the design doc framing ("they're all 'make HL2 work like mi0bot'").
- Con: Bigger diff (~600 lines new + 12 files modified).

**B. Two PRs (carry-forward first, then visibility on top).**

- Pro: Today's bug fixes ship faster.
- Con: Carry-forward PR includes signals/methods (`currentOcByteChanged`, `i2cTxComposed`) that have no consumer until PR 2 — review will rightly ask "why does this signal exist?"
- Con: Two rebases, two reviews.

**Decision: A.** Strategy B's "ship bug fixes faster" is illusory — neither PR ships in a release until both land.

---

## 2. Commit sequencing within the single PR

Order matters for `git bisect` and reviewer flow. Each commit GPG-signed.

| # | Commit | Files | Rationale |
|---|---|---|---|
| 1 | `fix(hl2): persistence key + restoreSettings reconcile` | `Hl2IoBoardTab.{h,cpp}`, `RadioModel.cpp`, `tst_hl2_io_board_tab.cpp` | Carry-forward. Root-cause fix for "no clicks without checkbox." Independent, bisect-friendly. |
| 2 | `feat(hl2): sequenced I2C probe state machine + REG_CONTROL=1 init` | `P1RadioConnection.{h,cpp}`, `IoBoardHl2.{h,cpp}`, `tst_p1_hl2_i2c_wiring.cpp` | Carry-forward. Replaces 3-reads-at-once. Independent of UI work. |
| 3 | `feat(hl2): live OC byte LED indicator on I/O Board tab + status label clarification` | `Hl2IoBoardTab.{h,cpp}`, `IoBoardHl2.{h,cpp}` (signal already added in #2) | Carry-forward. Diagnostic feature; depends on #2's signal emission. |
| 4 | `fix(hl2): signed −28..+32 dB step attenuator range` | `BoardCapabilities.cpp`, `RxApplet*.cpp`, `tst_p1_codec_hl2.cpp` | Task #26. Small; lands here so subsequent UI work sees the correct range. |
| 5 | `feat(hl2): tab relabels when HermesLite connected` | `HardwarePage.cpp`, `BoardCapabilities.cpp` | Task #25. Sets up labels the next commits reference. |
| 6 | `feat(hl2): OcOutputsSwlTab — 13 SWL bands × 7 pins matrix` | NEW `OcOutputsSwlTab.{h,cpp}`, `OcOutputsTab.cpp`, `CMakeLists.txt`, `tst_oc_outputs_swl_tab.cpp` (NEW) | Task #22 part A. Replaces `m_swlTab` QLabel placeholder. |
| 7 | `feat(hl2): VHF sub-tab placeholder for tab-count parity` | `OcOutputsTab.cpp` | Task #22 part B. ~5 lines. Splits from #6 only because the source-first cite is different. |
| 8 | `feat(hl2): N2ADR Filter master toggle writes 13 SWL pin-7 entries` | `Hl2IoBoardTab.cpp`, `RadioModel.cpp`, `tst_hl2_io_board_tab.cpp` | Task #23. Depends on #6 for the visual confirmation. |
| 9 | `feat(hl2): Hl2OptionsTab — Hermes Lite Options + I2C Control + I/O Pin State` | NEW `Hl2OptionsTab.{h,cpp}`, NEW `Hl2OptionsModel.{h,cpp}`, NEW `OcLedStripWidget.{h,cpp}`, `HardwarePage.cpp`, `CMakeLists.txt`, `tst_hl2_options_tab.cpp` (NEW) | Task #24. Largest single commit (~400 lines). Wire-format work for the 8 knobs deferred per design §4. |

If commits #6+#7 split feels artificial, fold them. Don't fold #8 into #6 — they have different review concerns (UI rendering vs. state-mutation handler).

---

## 3. Source-first gates (READ → SHOW → TRANSLATE)

Per CLAUDE.md non-negotiable. **Each commit below must run this protocol before code is written.**

### 3.1 Commit #6 (OcOutputsSwlTab)

| Step | Action |
|---|---|
| READ | `mi0bot setup.designer.cs:20414-20430+` (tpOCSWLControl + grpExtPAControlSWL + grpExtCtrlSWL + groupBoxTS19 + btnCtrlSWLReset). Capture exact group structure. |
| READ | Existing `src/gui/setup/hardware/OcOutputsHfTab.{h,cpp}` for the structural template — match its style. |
| SHOW | State: `"Porting from mi0bot setup.designer.cs:20414-20430 [v2.10.3.13-beta2] tpOCSWLControl — original C# control hierarchy: …"` Quote the relevant 10-15 lines. |
| TRANSLATE | New file with verbatim mi0bot header + NereusSDR modification block per `docs/attribution/HOW-TO-PORT.md`. Inline cite stamps on every ported line: `// From mi0bot setup.designer.cs:20418 [v2.10.3.13-beta2]`. Add PROVENANCE row. |
| VERIFY | Run `python3 scripts/verify-thetis-headers.py`, `python3 scripts/verify-inline-tag-preservation.py`, `python3 scripts/check-new-ports.py` before commit. |

### 3.2 Commit #8 (HERCULES SWL extension)

| Step | Action |
|---|---|
| READ | `mi0bot setup.cs:14324-14368` — the full HL2 branch of `chkHERCULES_CheckedChanged`. Note: 19 RX entries, 10 TX entries (HF amateur) + 13 SWL pin-7 RX entries. |
| READ | Our existing `Hl2IoBoardTab::onN2adrToggled` and `RadioModel.cpp` app-launch reconcile (modified in commit #1) to understand the current pattern. |
| SHOW | State: `"Porting from mi0bot setup.cs:14324-14368 [v2.10.3.13-beta2] chkHERCULES_CheckedChanged HL2 branch — original logic: …"` Quote all 13 SWL `chkOCrcv*7.Checked = true` lines so the band-list translation is auditable. |
| TRANSLATE | Extend `onN2adrToggled` to also call `OcMatrix::setBand(Band::Swl120m, {6}, {})` etc. for all 13 SWL bands. Inline cite stamps; preserve any `//` comments from upstream verbatim. |
| VERIFY | Same script triad. Plus: write a unit test that toggles N2ADR on, checks all 33 expected band-pin pairs in OcMatrix (10 HF amateur RX + 9 HF amateur N2ADR pin-7 + 13 SWL pin-7 + ≤10 HF amateur TX). |

### 3.3 Commit #9 (Hl2OptionsTab)

| Step | Action |
|---|---|
| READ | `mi0bot setup.designer.cs:11075-11135` (the three group boxes verbatim — captured already in design §2.2). |
| READ | `mi0bot setup.cs:tpHL2Options_*` event handlers if they exist (search: `grep -n "chkSwapAudioChannels\|chkCl2Enable\|chkExt10MHz\|chkDisconnectReset\|udPTTHang\|udTxBufferLat\|chkHL2PsSync\|chkHL2BandVolts" mi0bot setup.cs`). Capture default values. |
| READ | `mi0bot setup.designer.cs:groupBoxI2CControl` and `grpIOPinState` to confirm the exact widget set + layout. |
| SHOW | Three SHOW blocks, one per group box. Quote upstream group structure + default values. |
| TRANSLATE | New `Hl2OptionsTab` + `Hl2OptionsModel` (8 knobs as fields with persisted defaults matching mi0bot) + `OcLedStripWidget` (extracted from today's WIP inline LED in `Hl2IoBoardTab`). Wire-format composition for the 8 knobs is **explicitly deferred** — log a `qCWarning` saying "wire emission TBD Phase 3L follow-up" so test harness flags it. |
| VERIFY | Same script triad. Plus: tab loads cleanly when caps.boardKind == HermesLite, hidden otherwise. |

---

## 4. Rebase plan

Identical to design §5 — repeat here for handoff completeness:

```bash
git -C /Users/j.j.boyd/NereusSDR-hl2-hwtest stash push -u -m "hl2-bringup-2026-04-30-wip"
git -C /Users/j.j.boyd/NereusSDR-hl2-hwtest fetch origin
git -C /Users/j.j.boyd/NereusSDR-hl2-hwtest rebase origin/main
git -C /Users/j.j.boyd/NereusSDR-hl2-hwtest stash pop
# expect zero conflicts: PR #151 (spectrum repaint) + PR #152 (TX polish) touch unrelated subsystems
cmake --build build -j$(nproc)  # smoke
ctest --test-dir build --output-on-failure  # smoke
```

If conflicts: stop and ask. Don't auto-resolve.

---

## 5. Verification gates (in order)

| After commit # | Gate |
|---|---|
| 1 | App launches, N2ADR persistence round-trips, app-launch matrix reflects persisted state |
| 2 | HL2 connect → I2C log shows HwVersion → FwMajor → FwMinor → ControlInit sequence (4 reads, not 3 batched) |
| 3 | Band change → live OC LED indicator updates; status label says "mi0bot custom I/O board (0x41): Not detected" |
| 4 | RxApplet S-ATT slider accepts −28..+32, codec test for 3 corners passes |
| 5 | HL2 connect → tabs labeled "Hermes Lite Control" + "Ant/Filters"; non-HL2 → labels revert |
| 6 | Open Hermes Lite Control → SWL sub-tab → 13-band matrix renders; manual checkbox toggle persists |
| 7 | VHF sub-tab visible with placeholder text |
| 8 | Toggle N2ADR Filter on → SWL sub-tab pin 7 column lights up across all 13 bands; off → all clear |
| 9 | HL2 Options tab visible; all 8 knobs round-trip to AppSettings; I2C R/W tool composes & enqueues; output LED click toggles state (no-op on wire when no board, but state changes locally) |

Hardware test gates (separate session, post-PR-build):

- Connect HL2 → verify §1-3 + §5 + §6 + §8 + §9 LED-strip live updates
- Toggle N2ADR on → audible relay clicks per band (existing) + visual confirmation across HF + SWL tabs
- Step ATT to −28, 0, +32 → measure noise floor change at antenna port

---

## 6. Stop conditions

Stop and ask for direction if:

- Rebase produces any conflict (don't auto-resolve)
- Source-first READ doesn't find the cited mi0bot lines (line numbers may have drifted; capture fresh)
- Any of the three verification scripts (`verify-thetis-headers`, `verify-inline-tag-preservation`, `check-new-ports`) fails
- A test that was green before a commit goes red after — revert the commit, investigate, do not push fix-on-top

---

**Author:** J.J. Boyd ~ KG4VCF, with AI tooling.
