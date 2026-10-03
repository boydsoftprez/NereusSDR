# R3 post-merge follow-ups implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Two small tasks. Runs in
> the integration worktree (lane A) after the lane B carry.

**Goal:** close what two reviews left open once both lanes are on the integration
branch: the wording plan's table-key check and last raw reasons, and the display
sender's ordering for older apps.

**Architecture:** the table-key check matches whole quoted literals; the remote
Tuner Genius error, the app's own "The station does not support ..." refusals and
the NNR adapter's reasons go through `OperatorReasonText` like every other reason.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen).

**Spec:** Task 1: the user wording plan's re-review
(`.crew/2026-09-23-r3-user-wording-plan/re-review.md` in
`/Users/j.j.boyd/.codex/worktrees/nereus-lane-b/NereusSDR`), item 10 and the out-of-scope
list; R-R3-17, R-R3-21. Task 2: the CPU-adaptive plan's follow-up check
(`.crew/2026-09-23-r3-cpu-adaptive-spectrum-plan/progress.md`, "Follow-up check" lines, in the
integration worktree); R-R3-08, R-R3-37.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters.
  Stage explicit paths only. Every commit names its R-R3 IDs.
- Wire strings never change; translate at display time; log the raw text.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Build
  exact targets, then `ctest -R '^(...)$' --no-tests=error --output-on-failure`.
  Never build the `NereusSDR` target. No hardware.

## Task 1: The last raw reasons and a check that cannot be fooled

**Requirements:** R-R3-17, R-R3-21.

**Files:**
- Modify: `tests/tst_operator_wording_sweep.cpp` (`everyTableKeyIsStillInTheSources`
  joins adjacent string literals, then matches each key as a whole quoted literal,
  `"` + key + `"`, or `"` + key for the media-start prefix), `src/gui/OperatorReasonText.cpp`
  (entries for the Core's Tuner Genius identity error, the app's "The station does
  not support ..." refusals in Core wording, the NNR adapter's reasons),
  `src/gui/setup/CatNetworkSetupPages.cpp` (the remote Tuner Genius row's error
  through `forDisplay`), `src/core/session/StationClient.cpp` only if a refusal is
  shown without passing through a display site (prefer translating at the display
  site)
- Test: `tests/tst_operator_wording_sweep.cpp`, the tests covering the changed
  display sites

**Acceptance:**
- Rewording "endpoint limit reached" in `DaemonMediaController.cpp` turns the key
  check red (show it, then revert); the check still requires its floors.
- No internal term reaches a user from the Tuner Genius row; "PS3" and "The station
  does not support" no longer appear in anything a user reads (the raw text is
  still logged).
- Wire strings and compare sites unchanged.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_operator_wording_sweep tst_remote_peripherals tst_nnr_controls -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_operator_wording_sweep|tst_remote_peripherals|tst_nnr_controls)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (small).

- [ ] **Step 1:** Whole-literal matching with a red run, the remaining entries and
  display routing; commit.

## Task 2: Older apps keep their even share of spectrum frames

**Requirements:** R-R3-08, R-R3-37.

**Files:**
- Modify: `src/core/session/media/DaemonMediaController.cpp` (earliest-due ordering
  only when `displayPacingRequired()`; round-robin in legacy mode, as before),
  `src/core/session/media/DisplayBudget.cpp` (the refill comment names PureSignal's
  whole-snapshot burst), `docs/architecture/2026-09-22-session-display-budget-design.md`
  (the sender and pacing section matches the code: PureSignal bursts one worst-case
  snapshot, budget mode orders by earliest deadline, spectrum is paced to the budget
  less PureSignal's share; each with its reason)
- Test: `tests/tst_daemon_media_controller.cpp` (a legacy session with pans on two
  sources)

**Acceptance:**
- An oversubscribed legacy session with eight pans on two sources gives each pan its
  even share (25 fps each at 60 requested); the test fails against the current
  ordering first.
- The budget-mode ceiling case still gives the active pan its planned 60 fps and a
  total of at least 195 of 200.
- The design document and the comment agree with the code.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_daemon_media_controller tst_display_budget -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_daemon_media_controller|tst_display_budget)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (small).

- [ ] **Step 1:** Mode-dependent ordering with the two-source legacy test red first,
  the comment and the design text; commit.
