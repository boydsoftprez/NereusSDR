# Off-screen test windows implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** running the test suite on a Mac no longer puts thousands of windows on
the operator's screen or steals focus; the tests still exercise the same code.

**Architecture:** `nereus_add_test` registers every test with the Qt
`offscreen` platform in its environment, behind a CMake option that defaults on.
A test that genuinely needs the native window system opts out at registration
and carries a `native-window` label so it can be run on purpose. The fast test
loop guide says how to see windows when a person wants to.

**Tech stack:** CMake 3.22+ test properties, CTest, Qt 6 platform plugins
(`offscreen` ships with Qt: `/opt/homebrew/share/qt/plugins/platforms/libqoffscreen.dylib`).

**Spec:** operator report of 2026-09-22 (test runs open and close thousands of
windows and make the Mac hard to use) and `docs/development/fast-test-loop.md`.
No product requirement changes.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only.
- Never build the `NereusSDR` target: the operator's GUI runs from
  `build-integration/NereusSDR.app`. Test targets, `nereusd` and `all_tests`
  are fine.
- A valid test is never weakened to pass off-screen. A failure that shows a
  product defect (for example code that assumes a native window handle
  without checking) is reported as a finding, not hidden behind the opt-out.
- Hardware (Rock, radio, microphones, the running GUI) is off limits.
- Record `uptime` load averages with any timing.

## What already exists

- `tests/CMakeLists.txt:114-231` `nereus_add_test(name [EXTRA_LABELS ...] [sources...])`
  is the only place a test is registered (`add_test` at :192). It sets
  `LABELS` and `TIMEOUT 120` (:204-206) and, on Windows, an
  `ENVIRONMENT_MODIFICATION` that prepends the library directory to `PATH`
  (:211-215). Nothing sets `QT_QPA_PLATFORM`.
- 481 test sources create a `QApplication` (`QTEST_MAIN` or explicit), so on
  macOS each one shows real Cocoa windows.
- CI's Linux gate already runs the whole suite off-screen:
  `export QT_QPA_PLATFORM=offscreen` then `xvfb-run -a ctest`
  (`.github/workflows/ci.yml:715-732`). macOS and Windows CI configure with
  tests off. So the suite is known to pass off-screen on Linux; macOS
  off-screen is untested.
- Last full native gate: 743/743 at 9982cbed (254.84 s). Tests added since
  then (microphone capture helper, contention fix) raise the count.

## Task 1: Run tests off-screen by default

**Requirements:** test infrastructure; `docs/development/fast-test-loop.md`.

**Files:**
- Modify: `tests/CMakeLists.txt` (option, per-test environment, opt-out keyword)
- Modify: `docs/development/fast-test-loop.md` (a short "Test windows" section)
- Modify (only where the trial requires it): individual `nereus_add_test(...)`
  calls that must opt out, and test sources whose assumptions are artifacts of
  the native platform rather than product behaviour

**Interfaces:**
- Consumes: nothing.
- Produces: CMake option `NEREUS_TESTS_OFFSCREEN` (BOOL, default `ON`);
  `nereus_add_test(<name> NATIVE_WINDOW ...)` keyword that skips the
  off-screen environment and adds the label `native-window`.

**Acceptance:**
- With the default configure, every registered test except `NATIVE_WINDOW`
  ones runs with `QT_QPA_PLATFORM=offscreen`, set with
  `ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen"` so the Windows
  `PATH` modification is kept alongside it (both entries present on Windows).
  Prove it from `ctest --test-dir build-integration --show-only=json-v1`: count
  tests with and without the setting and name the exceptions.
- `-DNEREUS_TESTS_OFFSCREEN=OFF` removes the setting from every test (native
  windows again, for a person who wants to watch).
- `NATIVE_WINDOW` tests carry the `native-window` label, so
  `ctest -L native-window` runs exactly them and `ctest -LE native-window`
  runs everything else.
- Full trial on this Mac with the new default: build `nereusd` and `all_tests`,
  then run the whole suite once
  (`ctest --test-dir build-integration --output-on-failure -j6`), with load
  averages before and after. For each failure, rerun that one test with
  `QT_QPA_PLATFORM=cocoa` to classify it:
  - fails both ways: a real failure; diagnose it and report it (do not mark it
    native);
  - fails only off-screen because the test needs something the off-screen
    platform does not provide (native menu bar, Retina pixel ratio, a Cocoa
    view, GPU rendering): opt it out with `NATIVE_WINDOW` and say which
    capability it needs;
  - fails only off-screen because product code misbehaves without a native
    window: report it as a finding with file:line; do not opt it out.
- The guide says, in plain words, that tests run without windows by default,
  how to turn windows back on for one run or one build, and how to run the
  `native-window` group.

**Verification:** build-system change with integration evidence: the JSON
listing counts, the option toggle (configure twice, compare), and the full
off-screen trial's summary line. Whether any window still appears on the
operator's screen is checked by the controller, not the implementer.
```sh
cmake -S /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR -B /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target nereusd all_tests -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --output-on-failure -j6
```

**Execution note (advisory):** opus. Runs after the microphone plan's Task 5
commits (both edit `tests/CMakeLists.txt`); its full trial doubles as the
full-suite check for that task.

- [ ] **Step 1:** Add the option, the per-test environment and the
  `NATIVE_WINDOW` keyword; prove the listing counts and the toggle.
- [ ] **Step 2:** Run the full off-screen trial, classify every failure as
  above, apply only justified opt-outs, update the guide; commit.

## Task 2: Fix what the off-screen trial exposed

Added after Task 1's full trial (745 of 748 passed). The suite must be green
off-screen before the batch's final gate.

**Requirements:** R-R3-09 (float/dock must not leak), test infrastructure.

**Files:**
- Modify: `src/gui/PanadapterStack.cpp` (release a retiring floating window
  when rendering fails as well as after a submitted frame)
- Modify: `tests/tst_p2_established_silence.cpp` (stop checks at lines 188 and
  349 wait like the file's other stop checks at 110, 213 and 299)
- Modify (cause first): `tests/tst_remote_audio_clock.cpp` and/or its
  registration in `tests/CMakeLists.txt`
- Test: `tests/tst_panadapter_stack_layouts.cpp` (only if a case must observe
  the release directly)

**Interfaces:**
- Consumes: Task 1's off-screen default.
- Produces: no new API.

**Acceptance:**
- `PanadapterStack.cpp:613` today releases a retiring floating window only on
  the spectrum widget's `frameSubmitted`; without a GPU renderer Qt emits
  `renderFailed` instead and the hidden window stays alive until the pan's
  spectrum widget is destroyed. After the fix, a float/dock cycle without a GPU
  renderer leaves no retiring window alive, and the GPU path is unchanged.
  `tst_panadapter_stack_layouts` passes 30/30 off-screen; one native rerun of
  just that test also passes 30/30.
- `tst_p2_established_silence` no longer reads the fake radio's stop count
  before the stop datagram can arrive; it passes with
  `--repeat until-fail:10` while the machine is busy (run it alongside the full
  trial or another heavy job and record the load).
- `tst_remote_audio_clock`: find out why it needs about 115 s of CPU alone.
  Keep everything it proves. Either make it cheaper with a written argument
  that coverage is unchanged, or give its registration a cost that CTest
  respects (for example `PROCESSORS` or `RUN_SERIAL`) and a `TIMEOUT` that fits
  its measured duration under load, with the reason in a comment. It passes
  in a full `-j6` run.
- One full off-screen run of the suite passes (`ctest -j6`), with load
  averages before and after.

**Verification:** a product defect on the no-GPU path plus two test
reliability fixes; the full run is the gate for this task.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target nereusd all_tests -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_panadapter_stack_layouts|tst_p2_established_silence|tst_remote_audio_clock)$' --no-tests=error --output-on-failure
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --output-on-failure -j6
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Fix the window release and the two tests; run the three.
- [ ] **Step 2:** Full off-screen run; commit.
