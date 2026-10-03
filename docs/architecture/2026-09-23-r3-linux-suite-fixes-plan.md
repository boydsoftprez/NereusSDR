# R3 Linux test suite fixes implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; the next integration batch review covers it after the
> carry. Runs in the hotfix worktree `/Users/j.j.boyd/.codex/worktrees/nereus-hotfix/NereusSDR`
> (branch `codex/hotfix-linux-suite`, build directory `build-hotfix`), based on the
> integration head named at dispatch; the controller carries it into the integration
> branch afterwards.

**Goal:** the whole test suite passes on Linux as it does on macOS, and the Core loads
its band plans on every platform.

**Architecture:** the first full Linux run (Debian 13, GCC 14.2, Qt 6.8.2, in the Rock
builder image) failed 7 of 776 tests, 5 every time and 2 now and then. One is a product
defect: the band-plan files are compiled into the GUI library but read by the Core
library, so a program that links only the Core (the `nereusd` Core itself, and the test)
loads no band plans once the linker drops the unused GUI library, which Debian's and
Ubuntu's GCC do by default. The rest are test defects that the Mac hides: a pointer used
after the recovery it waits for deletes its object, a first-run audio dialog that opens
modally on Linux without an audio server, a test function that runs past QtTest's
5-minute limit on slower machines, and tests that share one settings file while ctest
runs them at the same time.

**Tech stack:** CMake, C++20, Qt 6 (resources, Qt Test off-screen), the arm64 Debian 13
Docker container used for the Rock build.

**Spec:** the Linux suite report of 2026-09-23 with root causes and file:line:
`/Users/j.j.boyd/.config/nereus/work/linux-suite-e0b41d4b/` (`ctest-full.log`,
`ctest-full-2.log`, `ctest-full-3.log`, `rerun/`, the driver `run-in-container.sh`; the
stopped container `nereus-linux-suite-e0b41d4b` keeps its build tree). The operator's
standing rule: failed tests are never ignored. R3 plan requirements R-R3-10 (the
desktop package keeps self-contained local operation), R-R3-21.

## Global Constraints

- Work only in the hotfix worktree and its build directory; never checkout, rebase,
  merge, reset or push; never touch the integration, lane B or checkpoint worktrees.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- A valid test stays valid: fix the test's defect, never loosen what it checks, never
  skip it on Linux.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. On the Mac,
  build exact targets with -j4 under `nice` (the operator is testing live) and never
  open audio devices; the Linux checks run in the container (Task 3).

## Task 1: The Core loads its band plans

**Requirements:** R-R3-10, R-R3-21.

**Files:**
- Modify: `resources.qrc` and `CMakeLists.txt` (the band-plan files move to a resource
  file attached to NereusCore; GUI assets stay in NereusSDRLib; the resource is
  initialised from the Core so a Core-only link keeps it), `src/core/BandPlanManager.cpp`
  if its loader needs the Core resource's init
- Test: `tests/tst_bandplan_manager.cpp` (links only the Core, as today), plus a case
  that a Core-only program loads the full band-plan set

**Acceptance:**
- `tst_bandplan_manager` passes on macOS and on Linux; on Linux it loads the same plans
  as on the Mac (it loaded none before, 6 of 9 cases failing).
- `nereusd` (built in the Rock container) loads the band plans at start (a log line or a
  test proves it).
- The app's band plans are unchanged.

**Verification:** `tst_bandplan_manager` on the Mac by exact name; the Linux check runs
in Task 3.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Move the resource, keep it linked from the Core, test; commit.

## Task 2: Test defects the Mac hides

**Requirements:** R-R3-21.

**Files:**
- Modify: `tests/tst_remote_media_controller.cpp` (copy the description clock right
  after the second recovery is observed, before the transport it belongs to is deleted
  by that recovery, and check the copy); the MainWindow tests
  `tests/tst_gui_connection_controller.cpp`, `tests/tst_station_lan_selection.cpp`,
  `tests/tst_remote_gui_gating.cpp` and every other test that builds a MainWindow (a
  shared helper sets `Audio/LinuxFirstRunSeen=True` with the existing
  `audio/FirstRunComplete`, so the Linux first-run audio dialog never opens modally in
  a test); `tests/tst_remote_audio_clock.cpp` (the two simulated hours as separate data
  rows, each inside QtTest's 5-minute function limit, and a ctest TIMEOUT that fits);
  `tests/CMakeLists.txt` `nereus_add_test()` (each test gets its own settings location,
  so tests that save, clear and reload the settings file cannot overwrite each other
  when ctest runs them at the same time: `tst_nnr_settings` and
  `tst_ps3_settings_persistence` failed that way); `tests/tst_rx_dsp_worker_thread.cpp`
  (when its 5 s wait at the worker result times out, it prints every thread's state
  before failing, so the next occurrence carries its cause)
- Test: the changed tests themselves

**Acceptance:**
- `tst_remote_media_controller` no longer uses a deleted transport (it crashed 3 of 3
  on Linux) and still checks the same recovery timing.
- The three MainWindow tests finish on Linux without an audio server in their usual
  time (they hung 120 s), and the first-run dialog still opens for a real first run in
  the app (unchanged product behaviour).
- `tst_remote_audio_clock` checks both simulated hours with the same tolerances and no
  single function runs near 300 s.
- Two settings tests run side by side 150 times without a failure (they failed 23 of
  150 before).
- No assertion is weakened anywhere.

**Verification:** the changed tests on the Mac by exact name (the ones that build a
MainWindow run off-screen without opening audio devices); Linux in Task 3.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Fix each test defect; commit.

## Task 3: The full suite on Linux

**Requirements:** R-R3-21.

**Files:**
- Modify: `/Users/j.j.boyd/.config/nereus/work/linux-suite-e0b41d4b/run-in-container.sh`
  only if needed to point it at this branch's signed archive (the work directory, not
  the repository)
- Test: the full suite in the container, and on the Mac the tests Tasks 1 and 2 changed

**Acceptance:**
- The full suite passes on Linux in the Rock builder image (776 or more tests, 0
  failures), including the 18 tests the receiver load hotfix built but could not run on
  the Mac because they can open a microphone.
- Any failure that remains is diagnosed with its cause in the report, never marked
  flaky and left.

**Verification:** the container's ctest summary line and its log path in the report.

**Execution note (advisory):** opus. Uses Docker; the Mac is shared with the operator's
live test, so cap the container at 6 CPUs as the first run did.

- [ ] **Step 1:** Package the signed branch head, run the suite in the container,
  report.
