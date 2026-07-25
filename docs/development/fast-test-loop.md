# Fast Test Loop

The suite has **513 registered tests** (517 `tst_*.cpp` files; four are
Linux/PipeWire-only and register only on Linux).

Every test executable statically links the entire application, so each one
costs about **38 CPU-seconds to link** and lands at roughly 35 MB. Building
all of them costs about **32 minutes**, and running them cold adds about
5 more. Almost nothing you do day to day needs that.

## Everyday commands

Build and run one test:

```bash
cmake --build build --target tst_slice_auto_agc && ctest --test-dir build -R '^tst_slice_auto_agc$' --output-on-failure
```

Run a subsystem. Labels are derived automatically at configure time from
each test's own `#include` lines, so they never need hand-maintenance:

```bash
ctest --test-dir build -L core
```

Current distribution:

| Label | Tests |
| --- | --- |
| `core` | 423 |
| `models` | 191 |
| `gui` | 152 |
| `unclassified` | 5 |

Tests commonly touch more than one subsystem, so these do not sum to 513.
`unclassified` means the test includes no `core/`, `models/`, or `gui/`
header at all; the five current members are all legitimately in that
category (a smoke test, a WDSP `extern "C"` test, a build-hygiene grep
test, and two that deliberately avoid instantiating GUI classes).

Every test also carries `TIMEOUT 120`, so a hung test fails instead of
blocking forever.

## Known wart on this branch: a plain build builds everything

On this branch, test executables are **not** `EXCLUDE_FROM_ALL`, so:

```bash
cmake --build build        # builds the app AND all 513 test binaries
```

That is the 32-minute path, and it triggers on any source change. Until the
fix lands, name your target explicitly:

```bash
cmake --build build --target NereusSDR              # app only
cmake --build build --target tst_slice_auto_agc     # one test
```

The fix already exists in commit `6ed89682` (`EXCLUDE_FROM_ALL` on tests
plus an `all_tests` aggregate target) but is not yet on `main`. It lives on
`feature/rfkit-rf2ks-applet`, `feature/phase3f-sub-epic-a-foundation`, and
`claude/adoring-elgamal-24e14e`. Once merged, a plain
`cmake --build build` builds only the app, and the full suite becomes an
explicit opt-in:

```bash
cmake --build build --target all_tests && ctest --test-dir build
```

## macOS: the first-run scan

macOS malware-scans every freshly linked binary the first time it runs.
With hundreds of new binaries this adds roughly 5 minutes to a cold suite
run, and it is why a test asserting `1 + 1 == 2` can take 6 seconds cold
and 0.1 seconds warm.

To exempt build-spawned processes, add your terminal under:

**System Settings → Privacy & Security → Developer Tools**

Machine-local; changes nothing in the repository.

## ccache

Wired automatically when `ccache` is on `PATH`. Install it:

```bash
brew install ccache
```

It needs one non-default setting, because the build shares a precompiled
header across targets via `REUSE_FROM`:

```bash
ccache --set-config sloppiness=pch_defines,time_macros
```

Without that, PCH'd compiles are effectively uncacheable. Check
effectiveness with `ccache -s`.

## Writing tests that stay fast

**Never sleep to wait for a state change.** Wait on the signal instead.
`tst_reconnect_on_silence` used to call `QTest::qWait(35000)` and
`QTest::qWait(7000)` while waiting out a real reconnect timeline. At 53.5
seconds it was the slowest test in the suite, and because a parallel run
cannot finish before its slowest single test, it set a hard floor on the
whole suite no matter how many cores were available.

It now runs in **1.3 seconds** by injecting compressed timings and
asserting on a `QSignalSpy` transition count:

```cpp
QSignalSpy transitions(&conn, &P1RadioConnection::connectionStateChanged);
// ...
QTRY_VERIFY_WITH_TIMEOUT(transitions.count() >= 7, 4000);
QCOMPARE(transitions.count(), 7);
```

`QTRY_*` returns the moment the condition holds, so the timeout is only an
upper bound. This is both faster and more robust under parallel load than a
fixed `qWait` computed against a hand-derived deadline.

If a timing constant makes a test slow, add a narrow test-only seam rather
than sleeping. Keep production defaults untouched, and make it obvious the
setter has no production callers.

## Where the settings file lives

Tests redirect Qt's writable locations into a sandbox via
`tests/TestSandboxInit.cpp`, which runs before `main()`. This exists
because a ctest run once overwrote a developer's real settings file
(v0.1.1 alpha).

The real file is **platform-specific**. CLAUDE.md quotes the Linux path;
on macOS it resolves elsewhere (see `src/core/AppSettings.cpp:112-118`):

| Platform | Path |
| --- | --- |
| macOS | `~/Library/Preferences/NereusSDR/NereusSDR.settings` |
| Linux | `~/.config/NereusSDR/NereusSDR.settings` |

If you are verifying that a test did not touch it, check the right one. A
check against the wrong path silently "passes" while proving nothing.

## Why the suite is slow, structurally

Linking dominates: about 32 minutes of the 37 is the linker, not the tests.
The cause is that `NereusSDRObjs` is one all-or-nothing OBJECT library
spanning `core`, `models`, and `gui`, so every source file is a transitive
input to every test binary and the build graph cannot tell that a
`SpectrumWidget` edit is irrelevant to a protocol test.

Measurements and the phased fix are in
[docs/architecture/2026-07-25-test-execution-speed-design.md](../architecture/2026-07-25-test-execution-speed-design.md).
