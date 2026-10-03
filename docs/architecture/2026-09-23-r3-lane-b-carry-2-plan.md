# R3 lane B carry 2 implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. One task. Runs in the
> integration worktree (lane A) while no other implementer works there.

**Goal:** bring the receiver audio plan's first three tasks from the second lane into
the integration branch, so the second lane can build TCI and VAX in a remote window on
top of the remote window Setup work, and run the full suite on the merged tree.

**Architecture:** one signed `git merge -S --no-ff codex/lane-b` at `f1115117` (four
commits since the first carry: `dd82d295` extra audio streams on the media connection,
`06fdb89c` and `91061bb7` the Core's per-receiver streams and the local VAX AF-gain fix,
`f1115117` the window's speakerless receiver consumers). The integration branch has
since gained the post-merge follow-ups, the remote window Setup plan and the NR3 crash
hotfix.

**Tech stack:** git, C++20, Qt 6, Qt Test (off-screen).

**Spec:** the ledgers `.crew/2026-09-23-r3-receiver-audio-plan/progress.md` (second
lane) and `.crew/2026-09-23-r3-remote-window-setup-plan/progress.md`,
`.crew/2026-09-23-r3-post-merge-follow-ups-plan/progress.md`,
`.crew/2026-09-23-r3-nr3-model-crash-hotfix-plan/progress.md` (integration). R3 plan
requirements R-R3-43, R-R3-21, R-R3-23, R-R3-10.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`. Read the second
  lane only through git; never touch its worktree.
- The merge commit and any follow-up: GPG-signed with hooks
  (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never `--no-gpg-sign` or `--no-verify`,
  no `Co-Authored-By`, no em-dash characters. Stage explicit paths only. Every commit
  names its R-R3 IDs.
- Resolve conflicts so both sides keep their tested behaviour; never drop one side's
  change to make a conflict go away; goldens stay as each side wrote them, extended only
  where both sides added entries.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`; tests
  must not touch this computer's real audio resources (the operator's test apps are
  running). Never build the `NereusSDR` target. No hardware.

## Task 1: Merge the second lane's receiver audio work

**Requirements:** R-R3-43, R-R3-21, R-R3-23, R-R3-10.

**Files:** the merge and whatever conflict resolution touches (likely
`src/gui/RemoteConnectionController.cpp`, `src/gui/RemoteMediaController.cpp`,
`src/core/session/StationCapabilities.cpp`, `tests/tst_display_budget_contract.cpp`,
`tests/CMakeLists.txt`).

**Acceptance:**
- One signed merge commit; afterwards `git cherry codex/integrate-r2-main codex/lane-b`
  lists nothing.
- Every conflict is listed in the report with its resolution and the test that proves
  both sides still hold.
- `all_tests` builds; the full suite runs once, off-screen, and its summary line is in
  the report; any failure is diagnosed, not waived.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target all_tests -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -j6 --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Merge, resolve, affected tests.
- [ ] **Step 2:** Full suite; report.
