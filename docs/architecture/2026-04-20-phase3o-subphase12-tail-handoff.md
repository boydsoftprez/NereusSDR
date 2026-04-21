# Phase 3O Sub-Phase 12 — Tail Handoff (Tasks 12.2–12.5)

**Date:** 2026-04-20
**Handoff kind:** mid-sub-phase, same branch
**Governing docs (read both, in order):**
1. [`2026-04-20-phase3o-subphase12-addendum.md`](2026-04-20-phase3o-subphase12-addendum.md) — authoritative scope and Q1–Q5 locked decisions.
2. [`2026-04-19-phase3o-vax-plan.md`](2026-04-19-phase3o-vax-plan.md) §§ Sub-Phase 12 — per-task check-lists, revised inline.

This doc is session-state only. It doesn't duplicate the addendum.

---

## Where we are (state at handoff)

```
Branch:     feature/phase-3o-setup-audio-page
Worktree:   ~/NereusSDR/.worktrees/phase-3o-setup-audio-page
HEAD:       47dd6ff  feat(setup): Sub-Phase 12 Task 12.1 — Audio nav refactor
Parent:     4523e83  docs(phase3o): Sub-Phase 12 brainstorm addendum + spec/plan updates
Upstream:   6595191  (origin/main, pre-existing — PR #78 merged)

Task 12.1: ✅ DONE — nav refactor, 8 new files, 2 deleted, build green
Task 12.2: ⬜ TODO — AudioDevicesPage (next — recommended to dispatch first in new session)
Task 12.3: ⬜ TODO — AudioVaxPage (release-gate unblocker for PR #80's TODO markers)
Task 12.4: ⬜ TODO — AudioAdvancedPage
Task 12.5: ⬜ TODO — AudioTciPage (placeholder rewrite — trivial)
```

Working tree is clean. The four scaffolding pages render as "Scaffolding only — populated in Task 12.X" placeholders; the Setup dialog's left nav shows the flat `Devices / VAX / TCI / Advanced` children under `Audio`.

## What the new session needs to do

1. Read `CLAUDE.md` and `CONTRIBUTING.md` (standing project rule on every new task).
2. Read the **addendum** — it is the single source of truth for Sub-Phase 12 decisions. Don't try to reconstruct them from chat history.
3. Read the **plan** §§ Task 12.2 through 12.5 for the per-step checklists.
4. Dispatch subagents using `superpowers:subagent-driven-development` in this order:
   - **Task 12.2** first (biggest; includes the Step 0 engine scaffolding everyone else depends on).
   - **Task 12.3** second (release-gate unblocker — removes `TODO(sub-phase-12-release-blocker)` in `MainWindow.cpp` and both `TODO(sub-phase-12-open-setup-audio)` markers in `VaxFirstRunDialog.cpp`; those markers land when PR #80 merges).
   - **Tasks 12.4 and 12.5** in any order.
5. Per-task cycle (per skill): implementer → spec-reviewer → code-quality-reviewer → commit landed → next.
6. After all four land, invoke `superpowers:finishing-a-development-branch` to decide PR / merge / cleanup.

## PR #80 coupling

PR #80 (Sub-Phase 11: `VaxFirstRunDialog`) is still **open** as of this handoff. The release-gate rule (memory: `project_nereussdr_phase3o_subphase11_release_gate`) says Sub-Phase 11 and Sub-Phase 12 must ship on the same tag — a user who hits **Skip** on the first-run dialog has no way back to cable binding until Task 12.3 wires `VaxFirstRunDialog::openSetupAudioPage("VAX")` → `SetupDialog::selectPage("VAX")`.

**Two possible states when Task 12.3 starts:**

1. **PR #80 has merged to main.** Rebase `feature/phase-3o-setup-audio-page` onto `origin/main` before starting 12.3 so the `VaxFirstRunDialog` class exists in the worktree. The signal is named `openSetupAudioTab(QString)` as of PR #80; Task 12.3 renames it to `openSetupAudioPage(QString)` as part of the wire-up — and removes the two `TODO(sub-phase-12-open-setup-audio)` markers and the one `TODO(sub-phase-12-release-blocker)` marker.
2. **PR #80 has NOT merged.** Task 12.3 can still build `AudioVaxPage` (channel cards + `QMenu` picker + native-override badge) but will have to stub the dialog-signal wire-up. Mark with a `TODO(sub-phase-11-merge)` and land the wire-up once #80 merges (small follow-up commit). Don't let PR #80 block 12.3's UI work.

Check `gh pr view 80` before starting 12.3.

## Review notes from Task 12.1 (for follow-up tasks' awareness)

Code-quality review turned up five minor items; none block 12.1, but a few influence how 12.2–12.5 should be written:

- **M1 — "Scaffolding only" user-visible text.** The scaffolding labels say "Task 12.X" which is developer jargon. Each follow-up task replaces its page's body, so this auto-resolves. Just don't carry the dev jargon forward.
- **M2 — `AudioTciPage` uses section+label instead of the single centered `QLabel` the plan suggested.** Task 12.5 implementer should notice and either keep the section+label pattern (consistent with the others in 12.1) or swap to the single-label pattern (more faithful to the plan text). Either is defensible; pick one and commit.
- **M3 — CMakeLists intra-block alphabetization doesn't match the outer grouping convention.** Not worth changing; keep new files alphabetical within their block.
- **M5 — design-spec reference in `AudioTciPage.h` class comment.** Currently says "(see design spec §11.1)" without naming the file. Task 12.5 should clarify to "see 2026-04-19-vax-design.md §11.1" in the rewritten page comment.
- **PR description note.** When the final PR goes up (end of Sub-Phase 12), note in the body that the per-file-per-page split (vs. the `AudioSetupPages.cpp` aggregation pattern used by Dsp/Display/Transmit) is a deliberate choice — the four follow-up tasks each add substantial independent content, so per-file wins on commit locality.

## Task 12.2 specific risks / gotchas

This is the biggest task. A few landmines a fresh session should know about:

- **Step 0 scaffolding lands *before* the three device cards.** Don't skip it — the `DeviceCard` widget depends on `AudioEngine::speakersConfigChanged` signal + `AudioDeviceConfig::loadFromSettings` helpers. Addendum §4 has the full list of Step 0 items.
- **Live-reconfig mutex.** `AudioEngine::setSpeakersConfig` must acquire `m_speakersBusMutex` during tear-down/rebuild; `rxBlockReady` uses `try_lock` and drops the block if busy. Test this with `AudioEngineSpeakersLiveReconfigTest` (mock DSP-thread traffic + main-thread setSpeakersConfig calls).
- **`MasterOutputWidget` currently polls AppSettings on a timer.** Replace the polling with the new `AudioEngine::speakersConfigChanged` subscription. Don't leave both — the timer poll becomes stale after the signal fires.
- **`ensureSpeakersOpen()` currently uses hardcoded defaults.** Replace with `AudioDeviceConfig::loadFromSettings("audio/Speakers")`. On a fresh install with no `audio/*` keys, `loadFromSettings` returns a default-constructed `AudioDeviceConfig` (empty `deviceName`), which `makeBus` then treats as "platform default" — same as current behavior, no regression.
- **Debounce on buffer-size scrub.** Q1 locked live-edit; we debounce 200ms on `setSpeakersConfig` calls triggered by rapid `valueChanged` signals (e.g., user scrubbing the buffer-size combo). Don't debounce across different controls — only intra-control.
- **Tests are required.** `AudioDeviceConfigRoundtripTest`, `AudioEngineSpeakersLiveReconfigTest`, `MasterOutputWidgetSignalRefreshTest`, `DeviceCardTest` — all 4 land in 12.2.

## Task 12.3 specific risks / gotchas

- **Full picker on Mac/Linux is NEW scope** (Q2 → option C) — the spec originally said "hidden on Mac/Linux native." Don't render compact cards on Mac/Linux; render the full 7-row form. Default binding = native HAL; allow override.
- **Amber "override — no consumer" warning badge.** When a Mac/Linux card is bound to a non-native device and `VirtualCableDetector::consumerCount()` returns 0, show the badge. Requires a new method on `VirtualCableDetector` — add it alongside.
- **`QMenu` auto-detect popup.** Match `BandButtonItem` right-click and `HardwarePage` antenna picker idioms. One-click bind. No intermediate "Assign" button.
- **Reassign-confirm modal for already-assigned cables.** Addendum §2.3. Use `QMessageBox` with exact copy "Reassign CABLE-A from VAX 2 to VAX 3? VAX 2 will become unassigned."

## Task 12.4 specific risks / gotchas

- **`SendIqToVax` + `TxMonitorToVax` checkboxes: ENABLED with warning log + note-inline.** Q4 locked option (a). Don't grey them out. Don't hide them.
- **`AudioEngine::resetAudioSettings()` scope.** Addendum §2.5 lists exactly which keys to clear and which to preserve. Write `AudioEngineResetAudioSettingsTest` to lock in the boundary — iterate every seeded `audio/*` key, call reset, assert cleared; iterate `slice/<N>/VaxChannel` and `tx/OwnerSlot`, call reset, assert preserved.
- **Confirm modal copy is verbatim** in addendum §2.5. Copy it exactly.

## Task 12.5 specific risks / gotchas

- **Trivial.** Rewrite `AudioTciPage` from the scaffolding placeholder (section + label) to either a single centered `QLabel` or keep the section+label pattern. Either is fine.
- Address M5 review note (include design-spec filename in class comment).

## Onboarding commands for the new session

```bash
cd ~/NereusSDR/.worktrees/phase-3o-setup-audio-page
git status                              # confirm clean
git log --oneline -5                    # confirm HEAD=47dd6ff, parent=4523e83
gh pr view 80 --json state,mergedAt     # check PR #80 status (affects 12.3)
cat docs/architecture/2026-04-20-phase3o-subphase12-addendum.md  # read the addendum
cat docs/architecture/2026-04-19-phase3o-vax-plan.md | sed -n '/## Sub-Phase 12/,/## Sub-Phase 13/p'  # just the relevant slice
```

Then invoke `superpowers:subagent-driven-development` and dispatch Task 12.2 first.

## Acceptance reminder

Sub-Phase 12 is complete when, per addendum §8:

- All 4 left-nav Audio pages render and bind settings round-trip.
- Live-edit of any Devices-tab control reopens the bus and updates the "Negotiated" pill within 100 ms.
- `VaxFirstRunDialog`'s Customize button navigates to Audio → VAX.
- Reset clears exactly the keys in addendum §2.5.
- Build green on all three platforms; ctest passes.
- Attribution verifier scripts pass (no new Thetis ports — all pages are NereusSDR-native Qt widgets, per `feedback_source_first_ui_vs_dsp`).

---

*Handoff prepared at end of Task 12.1 completion, 2026-04-20. Tail session continues from HEAD=47dd6ff with the four remaining tasks.*
