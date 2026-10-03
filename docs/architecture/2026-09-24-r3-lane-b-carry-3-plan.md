# R3 lane B carry 3 implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. One task. Runs in the
> integration worktree (lane A) while no other implementer works there.

**Goal:** bring the finished receiver audio plan and the headphones plan's first task
from the second lane into the integration branch, and run the full suite on the merged
tree, so the next operator checkpoint and the remaining R3 plans build on one tree.

**Architecture:** one signed `git merge -S --no-ff` of `codex/lane-b` at `f4700545`:
the receiver audio plan's Tasks 4-7 (`c5ac13cc` TCI in a remote window, `5f9ae4dd` VAX in
a remote window, `f83d4bc0` remote playback on any speaker format, `636e4b86` the FT8
bench), its fix wave and follow-up (`fb22b91a`, `9c457eda`, `f9ffee3e`, `60354baf`,
`1db56baa`, `638bd746`, `e1f99192`, `1192040b`) and the headphones plan's Task 1
(`f4700545`). The integration branch has since gained the remote window Setup fix wave,
the radio hardware plan (Tasks 1-5 and its fix wave), the receiver load hotfix, the
accessories plan's Task 1, the zoom hotfix and the Linux suite fixes. A trial merge
conflicts in `src/core/daemon/DaemonApp.cpp`, `src/core/settings/SettingsScope.cpp`,
`src/gui/MainWindow.cpp`, `src/gui/SetupDialog.cpp`, `src/models/RadioModel.{h,cpp}`,
`tests/tst_daemon_app.cpp`, `tests/tst_remote_gui_gating.cpp` and
`tests/tst_remote_window_harness.cpp`.

**Tech stack:** git, C++20, Qt 6, Qt Test (off-screen).

**Spec:** the ledgers `.crew/2026-09-23-r3-receiver-audio-plan/progress.md` and
`.crew/2026-09-23-r3-headphones-plan/progress.md` (second lane, read through git) and
`.crew/2026-09-23-r3-remote-radio-hardware-plan/progress.md`,
`.crew/2026-09-23-r3-remote-window-setup-plan/progress.md`,
`.crew/2026-09-23-r3-core-owned-accessories-plan/progress.md` (integration). R3 plan
requirements R-R3-42, R-R3-43, R-R3-44, R-R3-45, R-R3-46, R-R3-23, R-R3-21.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`. Read the second
  lane only through git; never touch its worktree. This task alone may run the one
  merge named above and commit it; no other checkout, rebase, reset or push.
- The merge commit and any follow-up: GPG-signed with hooks
  (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never `--no-gpg-sign` or `--no-verify`,
  no `Co-Authored-By`, no em-dash characters. Stage explicit paths only. Every commit
  names its R-R3 IDs.
- Resolve conflicts so both sides keep their tested behaviour; never drop one side's
  change to make a conflict go away; goldens stay as each side wrote them, extended only
  where both sides added entries. Capability versions from both sides coexist
  (`radioHardwareVersion` 3 from integration; `receiverAudioVersion` and the headphones
  work from the second lane).
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`; tests
  must not touch this computer's real audio resources (the second lane added a test-mode
  device guard; keep it effective). Never build the `NereusSDR` target. No hardware.

## Task 1: Merge the second lane's audio work

**Requirements:** R-R3-42, R-R3-43, R-R3-44, R-R3-45, R-R3-46, R-R3-23, R-R3-21.

**Files:** the merge and the conflict resolution (the nine files above, plus whatever
the resolution reaches).

**Acceptance:**
- One signed merge commit; afterwards `git log codex/integrate-r2-main..f4700545` lists
  nothing.
- Every conflict is listed in the report with its resolution and the test that proves
  both sides still hold.
- `all_tests` builds; the full suite runs once, off-screen, and its summary line is in
  the report; any failure is diagnosed and fixed in a follow-up commit, not waived.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target all_tests -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -j6 --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Merge, resolve, build the conflicted files' tests first, commit.
- [ ] **Step 2:** Build `all_tests`, run the full suite once, fix and commit anything it
  finds.
