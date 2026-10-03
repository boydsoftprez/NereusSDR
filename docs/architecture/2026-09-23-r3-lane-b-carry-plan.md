# R3 lane B carry implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. One task. Runs in
> the integration worktree (lane A) when no other implementer is active there, after
> the second lane's wording fix wave has passed its re-review.

**Goal:** bring the second builder lane's reviewed work into the integration
branch as one signed merge, with both lanes' behaviour intact, and finish the
items that were carried for the merge.

**Architecture:** the second lane branched from the integration branch at
`f1297d20` and holds 25 commits: the R3 audio and DSP control batch (fullband,
NR3 models on the Core, Core-owned notches, lossless audio, the audio delay
readout), the user wording plan, and their fix waves. The integration branch has
since gained the DSP overload batch, the CPU-adaptive spectrum plan and the
Connections change. One `git merge -S --no-ff codex/lane-b` brings the lane in with
its history and one conflict resolution; the carried items are finished on top.

**Tech stack:** git, C++20, Qt 6, Qt Test (off-screen).

**Spec:** the ledgers of the plans being merged:
`.crew/2026-09-23-r3-audio-and-dsp-control-plan/progress.md` and
`.crew/2026-09-23-r3-user-wording-plan/progress.md` in
`/Users/j.j.boyd/.codex/worktrees/nereus-lane-b/NereusSDR`, and the carried items in
`.crew/2026-09-23-r3-cpu-adaptive-spectrum-plan/progress.md` and
`fix-wave-findings.md` ("Carried into the lane B merge") in the integration
worktree. R3 plan requirements R-R3-08, R-R3-17, R-R3-21, R-R3-23, R-R3-35, R-R3-37.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`. Read the
  second lane only through git (`codex/lane-b`); never touch its worktree.
- The merge commit and every follow-up commit: GPG-signed with hooks
  (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never `--no-gpg-sign` or
  `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage explicit paths
  only. Every commit names its R-R3 IDs.
- Resolve conflicts so both lanes keep their tested behaviour; never drop one
  side's change to make a conflict go away. Wire strings and goldens stay as each
  lane left them.
- Operator wording: every string either lane added passes `OperatorWording::isPlain`;
  Core reasons shown to the user go through `OperatorReasonText`.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Never
  build the `NereusSDR` target. No hardware. Never checkout, rebase, reset or push.

## Task 1: Merge the second lane and finish the carried items

**Requirements:** R-R3-08, R-R3-17, R-R3-21, R-R3-23, R-R3-35, R-R3-37.

**Files:**
- Merge: `codex/lane-b` at the head the controller names in the dispatch.
- Modify after the merge: `src/gui/PanStatusText.{h,cpp}` and
  `src/gui/RemoteMediaController.{h,cpp}` (carried items), the tests that cover them,
  and whatever the conflict resolution touches.

**Interfaces:**
- Consumes: `RemoteMediaController::panDisplayBudgetReason(panId)` returning
  `NereusSDR::DisplayBudgetReason {None, CoreBusy}` (`src/core/session/media/DisplayBudget.h`)
  and `StationClient::remoteDisplayBudgetReason()` from the integration branch;
  the pan status builder, `OperatorReasonText` and `tests/OperatorWording.h` from
  the second lane.

**Acceptance:**
- One signed merge commit; afterwards `git cherry codex/integrate-r2-main codex/lane-b`
  lists nothing.
- Every conflict is listed in the report with how it was resolved and which test
  proves both sides still hold (for example `RemoteMediaController.cpp`,
  `DaemonMediaController.cpp`, `StationServer.cpp`, `StationCapabilities.cpp`,
  `SettingsScope.cpp`, `MainWindow.cpp`, `RadioModel.cpp`, `tests/CMakeLists.txt`).
- Carried item 1: a pan cut because the Core is busy reads "Core busy" in its short
  line and explanation (the builder reads the pan's budget reason); a cut for
  display capacity without that reason keeps the capacity wording; tests for both.
- Carried item 2: at session start in budget mode no pan shows "Waiting for the
  Core" unless the wait lasts longer than a named grace constant; a wait past the
  grace shows it; tests for both.
- Budget mode at the requested quality shows no status line (the integration
  branch's rule survives the builder); test.
- The NNR step-back wording from the DSP overload batch and the Core-busy wording
  pass the wording sweep; the sweep test covers them.
- `all_tests` builds; the full suite runs once, off-screen, and its summary line is
  in the report. Any failure is diagnosed, not waived: the known cty.dat path tests
  (`tst_cty_dat_parser`, `tst_adif_parser`, `tst_dxcc_color_provider` fail under
  ctest from a relative `__FILE__`) are named as such with their evidence, and
  `tst_capture_session_demand`'s 1000 ms disconnect bound is watched (it failed once
  at load average 42).

**Verification:** integration of two reviewed batches; full suite.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target all_tests -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -j6 --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Merge, resolve, build and run the affected tests of both lanes.
- [ ] **Step 2:** Carried items with tests; commit.
- [ ] **Step 3:** Full suite; report.
