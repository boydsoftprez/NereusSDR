# Test Execution Speed, Phase 0: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the cheap, non-structural costs from the NereusSDR test loop: wire ccache locally, stop compiling `TestSandboxInit.cpp` 519 times, give ctest `LABELS` and `TIMEOUT` so subsets can be requested at all, and remove the 53-second sleep floor that currently bounds every parallel run.

**Architecture:** All changes are build configuration plus one narrow test seam in `P1RadioConnection`. No DSP, protocol, or UI behavior changes. Labels are derived mechanically at configure time from each test's own `#include` lines, so the taxonomy maintains itself and no test file needs editing.

**Tech Stack:** CMake 3.x, ninja, ctest, Qt6 Test, ccache, clang (Apple), zsh.

**Design doc:** [2026-07-25-test-execution-speed-design.md](2026-07-25-test-execution-speed-design.md)

---

## Prerequisite (not a task in this plan)

`6ed89682` (`EXCLUDE_FROM_ALL` on tests + `all_tests` aggregate + LTO off)
is **not on `main`**. It lives on `feature/rfkit-rf2ks-applet`,
`feature/phase3f-sub-epic-a-foundation`, and `claude/adoring-elgamal-24e14e`.

It is the single largest win in the whole design and the code already
exists. Landing it on `main` is a maintainer decision (open a PR from one
of those branches, or cherry-pick `6ed89682`), so it is deliberately not a
task here.

**This plan does not depend on it.** Every task below applies cleanly with
or without it. Tasks 2 and 3 touch `nereus_add_test()`, which that commit
also edits, so expect a small conflict if both land; resolve by keeping
`EXCLUDE_FROM_ALL` on the `add_executable` line and this plan's other
changes around it.

---

## Scope note: why Phase 1 is not in this plan

The design doc estimated "roughly a dozen upward includes" between the
current state and a layered build graph. The exact audit says otherwise:

| Category | Count | Nature |
| --- | --- | --- |
| `models/Band.h` | 14 | Mechanical: move `Band.h` to a `common` leaf |
| `models/SliceModel.h` in `dsp/RxChannelState.h`, `dsp/TxChannelState.h`, needed only for the `DSPMode` enum | 2 | Mechanical: move `DSPMode` to `common` |
| Real model dependencies in `AudioEngine.cpp`, `SupportBundle.cpp`, `TwoToneController.{h,cpp}`, `TciServer.cpp`, `MicProfileManager.cpp` | 10 | **Not mechanical** |

That third row is the finding. Those five units sit in `src/core` but
depend on `RadioModel`, `SliceModel`, and `TransmitModel`, which means they
are application-layer coordinators filed under `core`. Splitting the
library cleanly requires deciding where they belong, which is a design
question, not a refactor step.

Phase 1 therefore needs its own design pass and its own plan. Attempting it
from the current spec would mean inventing that layering decision
mid-implementation.

---

## File Structure

| File | Responsibility | Change |
| --- | --- | --- |
| `CMakeLists.txt` | Top-level build config | Modify: add ccache launcher block |
| `tests/CMakeLists.txt` | Test target registration | Modify: label/timeout derivation, shared sandbox object lib |
| `src/core/P1RadioConnection.h` | P1 connection state machine | Modify: two constants become instance members + test seam |
| `src/core/P1RadioConnection.cpp` | Same | Modify: 2 usage sites, 1 new setter |
| `tests/tst_reconnect_on_silence.cpp` | Reconnect timeline coverage | Modify: use the seam, drop 42s of sleeping |
| `docs/development/fast-test-loop.md` | Developer guide | Create |

---

## Task 1: Wire ccache automatically when present

**Why:** ccache is configured in CI but has never been installed on dev
machines (`which ccache` returns nothing). A full compile is 1,537 CPU-s;
most of that is repaid on every branch switch and reconfigure.

**Files:**
- Modify: `CMakeLists.txt` (insert before the first `add_library`/`add_executable`, near the existing ccache comment at line ~1312)

- [ ] **Step 1: Install ccache**

```bash
brew install ccache
```

- [ ] **Step 2: Configure sloppiness for the shared PCH**

The PCH makes default ccache settings nearly useless here. `CMakeLists.txt`
already documents the requirement; this applies it.

```bash
ccache --set-config sloppiness=pch_defines,time_macros && ccache --set-config max_size=25G && ccache -p | grep -E "sloppiness|max_size"
```

Expected output includes `sloppiness = pch_defines,time_macros` and
`max_size = 25.0 GB`.

- [ ] **Step 3: Add the CMake launcher block**

Insert into `CMakeLists.txt` immediately above the existing comment block
that begins `# ccache must be configured with sloppiness=pch_defines`:

```cmake
# ── ccache compiler launcher (2026-07-25) ──────────────────────────────
# CI has always configured ccache explicitly; local dev machines never
# had it wired at all. Auto-detect and use it when present so branch
# switches and reconfigures stop paying full compile cost (measured:
# 1,537 CPU-s for a full compile, 313s app + 1,101s tests + 123s moc).
#
# Guarded on CMAKE_CXX_COMPILER_LAUNCHER being undefined so an explicit
# -DCMAKE_CXX_COMPILER_LAUNCHER=... on the configure line always wins,
# and so CI's own ccache setup is not double-applied.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "C compiler launcher")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "CXX compiler launcher")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not in use (not found, or launcher already set)")
endif()
```

- [ ] **Step 4: Reconfigure and confirm the message appears**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON 2>&1 | grep ccache
```

Expected: `-- ccache enabled: /opt/homebrew/bin/ccache`

- [ ] **Step 5: Verify cache actually fills**

```bash
ccache -z && cmake --build build --target NereusSDRObjs -j 2>/dev/null | tail -1 && ccache -s | grep -iE "cacheable|hit|miss"
```

Expected: nonzero "cacheable calls" on this first pass (misses are correct
here; this run is populating the cache).

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt
git commit -S -m "build: auto-wire ccache when present

CI configured ccache; local dev machines never had it, so every branch
switch and reconfigure paid the full 1,537 CPU-s compile. Auto-detect
and set CMAKE_{C,CXX}_COMPILER_LAUNCHER when ccache is on PATH.

Guarded on CMAKE_CXX_COMPILER_LAUNCHER being undefined so an explicit
-D on the configure line wins and CI's existing setup is untouched.

Requires sloppiness=pch_defines,time_macros (already documented at the
adjacent comment block) because of the REUSE_FROM shared PCH."
```

---

## Task 2: Compile `TestSandboxInit.cpp` once instead of 519 times

**Why:** Every test target compiles it separately at 0.10s each: 50 CPU-s
of pure duplication. It must remain an OBJECT library (not a static
library) because its only content is a global static constructor, which a
static-library link would discard as unreferenced.

**Files:**
- Modify: `tests/CMakeLists.txt` (the `nereus_add_test` function, and a new library above it)

- [ ] **Step 1: Add the shared object library above `function(nereus_add_test name)`**

```cmake
# ── Shared sandbox-init object library (2026-07-25) ────────────────────
# TestSandboxInit.cpp was previously listed as a source on all 519 test
# executables, so it was compiled 519 times (0.10s each, ~50 CPU-s of
# pure duplication).
#
# It MUST stay an OBJECT library, not a static library: its entire
# payload is a file-scope static constructor (g_nereusTestSandboxInit)
# with no referenced symbols, and a static-library link would drop it
# as unused. $<TARGET_OBJECTS:> forces the object in unconditionally,
# preserving the pre-main() QStandardPaths::setTestModeEnabled(true)
# guarantee that keeps ctest runs from overwriting the developer's real
# NereusSDR.settings (see the header comment in TestSandboxInit.cpp).
add_library(nereus_test_sandbox OBJECT TestSandboxInit.cpp)
target_link_libraries(nereus_test_sandbox PRIVATE Qt6::Core)
if(NEREUS_USE_PCH)
    target_precompile_headers(nereus_test_sandbox REUSE_FROM NereusSDRObjs)
endif()
```

- [ ] **Step 2: Swap the source for the object in `nereus_add_test`**

Replace this line:

```cmake
    add_executable(${name} ${name}.cpp TestSandboxInit.cpp ${ARGN})
```

with:

```cmake
    add_executable(${name} ${name}.cpp $<TARGET_OBJECTS:nereus_test_sandbox> ${ARGN})
```

(If `6ed89682` has landed, the line reads `add_executable(${name}
EXCLUDE_FROM_ALL ${name}.cpp TestSandboxInit.cpp ${ARGN})`; keep
`EXCLUDE_FROM_ALL` and swap only the `TestSandboxInit.cpp` token.)

- [ ] **Step 3: Rebuild one test and run it**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON >/dev/null && cmake --build build --target tst_app_settings_profile -j 2>&1 | tail -2 && ctest --test-dir build -R '^tst_app_settings_profile$' --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`.

**Do not treat this test as the guard.** Verified 2026-07-25:
`tst_app_settings_profile` contains zero `save()`, `load()`, or
`setValue()` calls, and its one `QStandardPaths` assertion computes the
expected value by calling the same API that the code under test calls,
so both sides agree whether or not the sandbox constructor ran. It cannot
detect a dropped sandbox. Use the mechanical proof in Step 4 instead.

- [ ] **Step 4: Prove the sandbox object survived, mechanically**

The settings file lives at a **platform-specific** path. On macOS
`QStandardPaths::GenericConfigLocation` resolves to `~/Library/Preferences`
(see `src/core/AppSettings.cpp:112-118`), NOT the `~/.config/...` path
quoted in CLAUDE.md, which is Linux-only. Checking the wrong path yields a
vacuous "unchanged" that proves nothing.

Capture before the test run:

```bash
SETTINGS=~/Library/Preferences/NereusSDR/NereusSDR.settings   # macOS
# SETTINGS=~/.config/NereusSDR/NereusSDR.settings             # Linux
ls -la "$SETTINGS" && md5 -q "$SETTINGS"
```

Re-check after. Both mtime and checksum must be identical.

Then confirm the object is genuinely linked in, which is the decisive test:

```bash
nm build/tests/tst_app_settings_profile | grep -ci "SandboxInit"
```

Expected: nonzero. Zero means a static-library-style drop occurred; revert
immediately.

- [ ] **Step 5: Confirm the dedup actually happened**

Count **compile rules**, not substring occurrences. A plain
`grep -c TestSandboxInit build/build.ninja` returns ~515 even when the fix
is working, because ninja expands `$<TARGET_OBJECTS:>` into the literal
`.o` path on every consuming link edge. That is expected and harmless. The
number that proves dedup is the compile-rule count:

```bash
grep -c "^build.*TestSandboxInit\.cpp\.o: CXX_COMPILER" build/build.ninja
```

Expected: `1`.

- [ ] **Step 6: Commit**

```bash
git add tests/CMakeLists.txt
git commit -S -m "build(tests): compile TestSandboxInit once, not 519 times

TestSandboxInit.cpp was a listed source on every test executable, so it
compiled once per target (0.10s x 519 = ~50 CPU-s of duplication).

Kept as an OBJECT library rather than a static library on purpose: its
whole payload is a file-scope static constructor with no referenced
symbols, which a static-library link would discard. \$<TARGET_OBJECTS:>
forces it in, preserving the pre-main() setTestModeEnabled(true) that
stops ctest runs from overwriting the developer's real settings file."
```

---

## Task 3: Derive ctest `LABELS` and set a default `TIMEOUT`

**Why:** `tests/CMakeLists.txt` currently defines zero labels, zero
fixtures, and zero timeouts, so there is no way to request a subset at all,
and a hung test blocks indefinitely rather than failing.

Labels are derived at configure time from each test's own `#include` lines
so no test file is edited and the taxonomy cannot drift.

**Files:**
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add the derivation helper above `function(nereus_add_test name)`**

```cmake
# ── Subsystem label derivation (2026-07-25) ────────────────────────────
# Scans a test's own #include lines and emits ctest LABELS for the
# subsystems it touches, so `ctest -L core` works without editing 519
# test files or hand-maintaining a mapping table.
#
# Measured distribution across the suite: core 83%, models 38%, gui 30%
# (tests commonly touch more than one, so these do not sum to 100).
function(_nereus_derive_test_labels out_var src_file)
    set(_labels "")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src_file}")
        set(${out_var} "unclassified" PARENT_SCOPE)
        return()
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${src_file}" _incs
         REGEX "^[ \t]*#include[ \t]+\"")
    foreach(_line IN LISTS _incs)
        if(_line MATCHES "\"(\\.\\./)*core/")
            list(APPEND _labels "core")
        endif()
        if(_line MATCHES "\"(\\.\\./)*models/")
            list(APPEND _labels "models")
        endif()
        if(_line MATCHES "\"(\\.\\./)*gui/")
            list(APPEND _labels "gui")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _labels)
    if(NOT _labels)
        set(_labels "unclassified")
    endif()
    set(${out_var} "${_labels}" PARENT_SCOPE)
endfunction()
```

- [ ] **Step 2: Apply labels and timeout in `nereus_add_test`**

Replace this line:

```cmake
    add_test(NAME ${name} COMMAND ${name})
```

with:

```cmake
    add_test(NAME ${name} COMMAND ${name})

    # Labels let `ctest -L core` request a subset; TIMEOUT converts a hung
    # test from an indefinite block into a failure. 120s is deliberately
    # generous: the slowest test in the suite as of 2026-07-25 is
    # tst_reconnect_on_silence at 53.5s, and Task 4 brings that under 1s.
    _nereus_derive_test_labels(_test_labels "${name}.cpp")
    set_tests_properties(${name} PROPERTIES
        LABELS "${_test_labels}"
        TIMEOUT 120)
```

- [ ] **Step 3: Reconfigure and verify labels exist**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON >/dev/null && ctest --test-dir build -N -L core 2>/dev/null | tail -1
```

Expected: `Total Tests: 433` (or close; the design measured 83% of 519).

- [ ] **Step 4: Verify the other two labels resolve**

```bash
for l in core models gui unclassified; do printf "%-14s %s\n" "$l" "$(ctest --test-dir build -N -L $l 2>/dev/null | grep -c 'Test *#')"; done
```

Expected roughly: `core 433`, `models 197`, `gui 156`, `unclassified 5`.

- [ ] **Step 5: Verify TIMEOUT is attached**

```bash
grep -c "TIMEOUT" build/tests/CTestTestfile.cmake
```

Expected: a count equal to the number of registered tests (519).

- [ ] **Step 6: Commit**

```bash
git add tests/CMakeLists.txt
git commit -S -m "test(ctest): derive subsystem LABELS, add default TIMEOUT

tests/CMakeLists.txt defined zero labels, zero fixtures and zero
timeouts, so there was no way to request a subset of the suite and a
hung test blocked indefinitely instead of failing.

Labels are derived at configure time from each test's own #include
lines rather than hand-maintained, so no test file is edited and the
mapping cannot drift. Measured distribution: core 83%, models 38%,
gui 30% (tests commonly touch more than one).

TIMEOUT 120 is deliberately generous against the current slowest test
(tst_reconnect_on_silence, 53.5s)."
```

---

## Task 4: Remove the 53-second sleep floor from `tst_reconnect_on_silence`

**Why:** The test contains `QTest::qWait(35000)` and `QTest::qWait(7000)`:
42 seconds of literal sleeping, and the test costs 53.5s. While it exists,
**no amount of parallelism gets the suite below ~53 seconds.** It is the
single slowest test and it sets the floor for every `ctest -j` run.

The fix adds a narrow test seam: two timing constants become instance
members seeded from the same `constexpr` defaults. Production values and
behavior are unchanged.

**Files:**
- Modify: `src/core/P1RadioConnection.h:466-468`
- Modify: `src/core/P1RadioConnection.cpp:2366`, `:2383`
- Modify: `tests/tst_reconnect_on_silence.cpp` (both test methods)

The test class has **two** methods, each with its own connection object:

| Line | Method | Cost driver |
| --- | --- | --- |
| 39 | `silenceTriggersErrorThenRecoversOnResume()`, `conn` at 43 | `QTRY_VERIFY_WITH_TIMEOUT` waits, each blocking ~2s on the watchdog |
| 72 | `boundedRetriesExhaustStayInLinkLost()`, `conn` at 76 | the `qWait(35000)` + `qWait(7000)` pair |

Both get the seam. The second is where the 42 seconds live, but the first
still costs several seconds waiting out the 2000ms watchdog repeatedly.

- [ ] **Step 1: Write the failing test**

First, in **both** methods, insert the seam call between the `conn.init();`
line and the `conn.connectToRadio(makeInfo(fake));` line (that is, after
line 44 in the first method and after the corresponding `init()` in the
second):

```cpp
        // Compress the reconnect timeline 100x: watchdog 2000ms -> 20ms,
        // reconnect interval 5000ms -> 50ms. Without this the second test
        // method sleeps 42 real seconds and sets the parallel floor for
        // the entire suite.
        conn.setReconnectTimingForTest(20, 50);
```

Then replace lines 88 through 105 (inside
`boundedRetriesExhaustStayInLinkLost`) with:

```cpp
        // Timeline is driven by two P1RadioConnection timing values that
        // the test compresses 100x via setReconnectTimingForTest():
        //   watchdog silence  2000ms -> 20ms
        //   reconnect interval 5000ms -> 50ms
        //
        // Compressed timeline (all times relative to fake.stop()):
        //   ~20ms:  watchdog trips → LinkLost (attempt 0)
        //   ~70ms:  reconnect timeout → attempt 1, Connecting
        //   ~90ms:  watchdog trips → LinkLost
        //   ~140ms: reconnect timeout → attempt 2, Connecting
        //   ~160ms: watchdog trips → LinkLost
        //   ~210ms: reconnect timeout → attempt 3, Connecting
        //   ~230ms: watchdog trips → LinkLost
        //   ~280ms: reconnect timeout → retries exhausted, stays in LinkLost
        //
        // Wait 350ms total — well past the 280ms exhaust point — then verify.
        QTest::qWait(350);
        QCOMPARE(conn.state(), ConnectionState::LinkLost);

        // Wait another 70ms to confirm no further state changes (reconnect
        // timer would fire at 330ms from exhaust point if unbounded).
        QTest::qWait(70);
        QCOMPARE(conn.state(), ConnectionState::LinkLost);
```

Also reduce the four `QTRY_VERIFY_WITH_TIMEOUT` timeouts in the first
method (currently `3000`, `3000`, `5000`, `8000`) to `500` each, since the
compressed watchdog now trips at 20ms rather than 2000ms. `QTRY_*` returns
as soon as its condition holds, so these are upper bounds and leaving them
high would not slow a passing run; lowering them makes a genuine regression
fail in half a second instead of up to eight.

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build build --target tst_reconnect_on_silence -j 2>&1 | tail -5
```

Expected: FAIL to compile, with
`error: no member named 'setReconnectTimingForTest' in 'P1RadioConnection'`

- [ ] **Step 3: Add the seam to the header**

In `src/core/P1RadioConnection.h`, replace lines 466-468:

```cpp
    static constexpr int kWatchdogSilenceMs    = 2000;          // silence → Error threshold
    static constexpr int kReconnectIntervalMs  = 5000;          // delay between retry attempts
    static constexpr int kMaxReconnectAttempts = 3;             // max retries before staying in Error
```

with:

```cpp
    // Defaults unchanged. As of 2026-07-25 the first two are seeded into
    // instance members so tests can compress the reconnect timeline;
    // nothing outside the test suite calls setReconnectTimingForTest(),
    // so production timing is bit-identical to before.
    static constexpr int kWatchdogSilenceMs    = 2000;          // silence → Error threshold
    static constexpr int kReconnectIntervalMs  = 5000;          // delay between retry attempts
    static constexpr int kMaxReconnectAttempts = 3;             // max retries before staying in Error

    int m_watchdogSilenceMs{kWatchdogSilenceMs};
    int m_reconnectIntervalMs{kReconnectIntervalMs};
```

Then add to the `public:` section (next to the other public slots/methods):

```cpp
    // Test-only seam: compress the silence-watchdog and reconnect-retry
    // timeline so tst_reconnect_on_silence does not sleep for 42 real
    // seconds (which set the parallel floor for the whole ctest run).
    // Not called anywhere in production code.
    void setReconnectTimingForTest(int watchdogSilenceMs, int reconnectIntervalMs);
```

- [ ] **Step 4: Implement the setter and switch the two usage sites**

In `src/core/P1RadioConnection.cpp`, add near the other setters:

```cpp
void P1RadioConnection::setReconnectTimingForTest(int watchdogSilenceMs,
                                                  int reconnectIntervalMs)
{
    m_watchdogSilenceMs   = watchdogSilenceMs;
    m_reconnectIntervalMs = reconnectIntervalMs;
}
```

At line 2366, change:

```cpp
    if (silenceMs > kWatchdogSilenceMs) {
```

to:

```cpp
    if (silenceMs > m_watchdogSilenceMs) {
```

At line 2383, change:

```cpp
        m_reconnectTimer->start(kReconnectIntervalMs);
```

to:

```cpp
        m_reconnectTimer->start(m_reconnectIntervalMs);
```

Leave `kMaxReconnectAttempts` as `static constexpr`: it is a count, not a
duration, and does not affect wall time.

- [ ] **Step 5: Run the test to verify it passes, and time it**

```bash
cmake --build build --target tst_reconnect_on_silence -j >/dev/null 2>&1 && /usr/bin/time -p ./build/tests/tst_reconnect_on_silence 2>&1 | tail -4
```

Expected: `PASS`, and `real` under 2.0 seconds (was 53.5s).

- [ ] **Step 6: Verify no production caller regressed**

```bash
grep -rn "setReconnectTimingForTest" src/ | grep -v "P1RadioConnection" || echo "OK: no production callers"
```

Expected: `OK: no production callers`

- [ ] **Step 7: Run the sibling P1 connection tests**

```bash
ctest --test-dir build -R 'tst_p1_|tst_reconnect|tst_connection' --output-on-failure 2>&1 | tail -5
```

Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add src/core/P1RadioConnection.h src/core/P1RadioConnection.cpp tests/tst_reconnect_on_silence.cpp
git commit -S -m "test(p1): compress reconnect timeline, drop 53s suite floor

tst_reconnect_on_silence contained QTest::qWait(35000) plus
QTest::qWait(7000): 42 seconds of literal sleeping, 53.5s total. It was
the slowest test in the suite and set a hard floor on wall time for
every ctest -j run, since no amount of parallelism can finish before
the slowest single test.

kWatchdogSilenceMs and kReconnectIntervalMs are now seeded into
instance members so a test can scale the timeline 100x. Defaults and
production behaviour are unchanged: setReconnectTimingForTest() has no
production callers. kMaxReconnectAttempts stays constexpr, being a
count rather than a duration.

Test now runs in under 2s."
```

---

## Task 5: Resolve the 7 unregistered test files

**Why:** 526 `tst_*.cpp` files exist but only 519 are registered with
`nereus_add_test()`. The other seven compile-rot silently: nothing builds
or runs them, so they can reference deleted APIs indefinitely without
anyone noticing.

The seven:

```
tst_linux_backend_detection
tst_linux_pipe_bus
tst_p2_regression_freeze_capture
tst_pipewire_stream_config
tst_pipewire_stream_integration
tst_slice_auto_agc
tst_tx_applet_mic_gain
```

- [ ] **Step 1: Determine whether each is intentionally platform-gated**

```bash
cd /Users/j.j.boyd/NereusSDR && for t in tst_linux_backend_detection tst_linux_pipe_bus tst_p2_regression_freeze_capture tst_pipewire_stream_config tst_pipewire_stream_integration tst_slice_auto_agc tst_tx_applet_mic_gain; do printf "%-38s %s\n" "$t" "$(grep -c 'Q_OS_LINUX\|NEREUS_HAVE_PIPEWIRE' tests/$t.cpp)"; done
```

Interpretation: a nonzero count means the file is Linux/PipeWire-specific
and its absence from the macOS build may be deliberate. Zero means it is
simply unregistered.

- [ ] **Step 2: Check whether each still compiles**

For each file that Step 1 showed as **not** platform-gated, add a temporary
registration at the end of `tests/CMakeLists.txt`:

```cmake
# TEMPORARY — compile check for unregistered tests, remove before commit
nereus_add_test(tst_slice_auto_agc)
nereus_add_test(tst_tx_applet_mic_gain)
nereus_add_test(tst_p2_regression_freeze_capture)
```

Then:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON >/dev/null && cmake --build build --target tst_slice_auto_agc tst_tx_applet_mic_gain tst_p2_regression_freeze_capture -j 2>&1 | tail -20
```

- [ ] **Step 3: Register the ones that compile and pass; delete the ones that do not**

For each that builds and passes, keep its `nereus_add_test()` line and move
it next to its thematic neighbours in the file (remove the TEMPORARY
comment).

For each that fails to compile, it is dead code referencing a removed API.
Delete the file:

```bash
git rm tests/<name>.cpp
```

Do **not** attempt to repair a dead test in this task. If one looks like it
covers behavior worth keeping, note it and open a follow-up issue instead;
repairing it is a separate change with its own review.

For the Linux/PipeWire-gated files, leave them alone and add a comment in
`tests/CMakeLists.txt` recording why they are absent:

```cmake
# ── Linux-only test sources (not registered on this platform) ──────────
# tst_linux_backend_detection, tst_linux_pipe_bus,
# tst_pipewire_stream_config, tst_pipewire_stream_integration
# are Linux/PipeWire-specific. They are intentionally not registered
# via nereus_add_test() here. Audited 2026-07-25; if the Linux audio
# work needs them in CI, register them under an if(LINUX) guard.
```

- [ ] **Step 4: Confirm the counts now reconcile**

```bash
cd /Users/j.j.boyd/NereusSDR && echo "files: $(ls tests/tst_*.cpp | wc -l)  registered: $(cd build && ctest -N 2>/dev/null | grep -cE '^ +Test +#')"
```

Expected: the gap is either zero or exactly the number of
intentionally-Linux-gated files, with that number documented in the comment
from Step 3.

- [ ] **Step 5: Commit**

```bash
git add tests/CMakeLists.txt tests/
git commit -S -m "test: reconcile 7 unregistered tst_*.cpp files

526 test source files existed but only 519 were registered via
nereus_add_test(), so seven were never built or run and could rot
against deleted APIs indefinitely.

Registered the ones that still build and pass, deleted the ones that
no longer compile (dead code referencing removed APIs), and documented
the Linux/PipeWire-specific ones as intentionally unregistered on this
platform."
```

---

## Task 6: Document the fast test loop

**Why:** The 2026-05-02 policy eroded partly because it lived only in
review feedback. The commands need to be somewhere a new contributor or a
fresh agent session will find them.

**Files:**
- Create: `docs/development/fast-test-loop.md`

- [ ] **Step 1: Write the guide**

```markdown
# Fast Test Loop

The suite has 519 test executables. Each one statically links the whole
application, so building all of them costs roughly 32 minutes. Almost
nothing you do day to day needs that.

## Everyday commands

Build and run one test:

    cmake --build build --target tst_slice_model && ctest --test-dir build -R '^tst_slice_model$' --output-on-failure

Run a subsystem (labels are derived from each test's includes):

    ctest --test-dir build -L core
    ctest --test-dir build -L models
    ctest --test-dir build -L gui

Build the whole suite, only when you actually want it:

    cmake --build build --target all_tests && ctest --test-dir build

## macOS: the first-run scan

macOS malware-scans every freshly linked binary the first time it runs.
With 519 new binaries this adds roughly 5 minutes to a cold suite run, and
it is why a test asserting `1 + 1 == 2` can take 6 seconds cold and 0.1
seconds warm.

To exempt build-spawned processes, add your terminal under:

    System Settings -> Privacy & Security -> Developer Tools

This is machine-local and affects nothing in the repository.

## ccache

Configured automatically when `ccache` is on PATH. Install with
`brew install ccache`. It needs one non-default setting because of the
shared precompiled header:

    ccache --set-config sloppiness=pch_defines,time_macros

Check effectiveness with `ccache -s`.

## Why not just run everything

Because linking dominates. A full suite build is about 32 minutes of
linking and about 5 minutes of running. The measurements and the plan to
fix it structurally are in
[docs/architecture/2026-07-25-test-execution-speed-design.md](../architecture/2026-07-25-test-execution-speed-design.md).
```

- [ ] **Step 2: Verify every command in the guide actually runs**

```bash
cd /Users/j.j.boyd/NereusSDR && ctest --test-dir build -N -L core >/dev/null && ctest --test-dir build -N -L models >/dev/null && ctest --test-dir build -N -L gui >/dev/null && echo "all label queries OK"
```

Expected: `all label queries OK`

- [ ] **Step 3: Commit**

```bash
git add docs/development/fast-test-loop.md
git commit -S -m "docs(dev): fast test loop guide

The 2026-05-02 'run the narrow test, not the suite' policy eroded partly
because it only ever lived in review feedback. Writes the actual
commands down: per-test builds, the new ctest -L subsystem labels, the
all_tests opt-in, the macOS Developer Tools exemption for the first-run
scan, and ccache's required sloppiness setting."
```

---

## Verification

After all tasks, re-run the design doc's §2 measurements and compare.

- [ ] **Single-test loop**

```bash
cd /Users/j.j.boyd/NereusSDR && touch src/core/AppSettings.cpp && /usr/bin/time -p sh -c 'cmake --build build --target tst_app_settings_profile -j >/dev/null 2>&1' 2>&1 | grep real
```

Expected: under 10s.

- [ ] **Slowest test no longer sets a floor**

```bash
cd /Users/j.j.boyd/NereusSDR/build && awk 'NF==3 {print $3, $1}' Testing/Temporary/CTestCostData.txt | sort -rn | head -3
```

Expected: `tst_reconnect_on_silence` no longer at the top, and the new
maximum under 20s.

- [ ] **Labels usable**

```bash
ctest --test-dir build -N -L gui 2>/dev/null | tail -1
```

Expected: roughly 156 tests, not 519.

- [ ] **ccache warm**

```bash
ccache -s | grep -iE "hit rate|cache hit"
```

Expected: nonzero hit rate after a second build.

- [ ] **Full suite still green**

```bash
cmake --build build --target all_tests -j >/dev/null 2>&1; ctest --test-dir build --output-on-failure 2>&1 | tail -3
```

Expected: `100% tests passed`. This is the one deliberately slow run;
expect roughly 37 minutes until Phase 1 lands.

---

## Out of scope

- Phase 1 (shared subsystem libraries). Needs its own design pass, see the
  scope note above.
- Phase 2 (skip-unchanged default). Inert until Phase 1 lands, because
  touching any source file currently changes all 519 test binary hashes.
- Phase 3 (selective consolidation).
- `-O0` for test compiles. Rejected in the design doc §7: blocked by the
  shared `-O2` PCH.
- Landing `6ed89682` on `main`. Maintainer decision, see Prerequisite.
