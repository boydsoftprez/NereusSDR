# Fast Test Loop

The suite has **513 registered tests** (517 `tst_*.cpp` files; four are
Linux/PipeWire-only and register only on Linux).

The application is built as a single shared library (`NereusSDRLib`) that
every test links dynamically, so a test executable is about **90 KB**, not a
private 22 MB copy of the whole app. Measured on an Apple Silicon dev
machine, `RelWithDebInfo`, ninja, ccache warm, `-j8` / `ctest -j4`:

| | Value |
| --- | --- |
| Touch one `src/core` file, rebuild `all_tests` | **25 s** |
| Build `all_tests` from a warm tree | **271 s** |
| `build/tests` on disk | **1.6 GB** |
| Full suite, cold | **121 s** |
| Full suite, warm | **50 to 56 s** |

"Cold" means the binaries were just relinked, which is the normal case after
any edit. It is slower than warm because macOS malware-scans every freshly
linked Mach-O the first time it runs.

Almost nothing you do day to day needs the full suite anyway.

## Everyday commands

Build and run one test:

```bash
cmake --build build --target tst_slice_auto_agc && ctest --test-dir build -R '^tst_slice_auto_agc$' --output-on-failure
```

Run a subsystem. Labels are derived automatically at configure time from
each test's own `#include` lines, so they never need hand-maintenance:

```bash
cmake --build build --target tests_core && ctest --test-dir build -L core
```

**Always build the matching `tests_<label>` target first.** Test
executables are `EXCLUDE_FROM_ALL`, so a plain `cmake --build build` does
not rebuild them. Running `ctest -L core` on its own is unsafe: on a clean
tree every selected test reports "Not Run", and on an already-populated
tree it silently executes **stale binaries built from your previous code**
and returns a false green. There is one `tests_<label>` target per label,
generated from the same derivation that produces the labels, so the two
cannot drift apart.

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

### Labels narrow the run, not the dependency

Labels are derived from each test's **direct** includes, so they are a
triage aid, not a blast-radius calculation. **85 of the 514 tests carry no
`core` label but still link all of `NereusSDRLib`**, so a `src/core` edit
genuinely affects them even though `ctest -L core` will not run them.

Every test depends on every source file, and the shared library does not
change that: it makes each dependency cheap, not narrower. Use `-L` to get
fast feedback while iterating; use the full suite before you call something
done.

## Building tests is opt-in

Test executables are `EXCLUDE_FROM_ALL`, so a routine build only builds the
app. Measured after touching `src/core/AppSettings.cpp`:

```bash
cmake --build build        # 1.64 s, links 0 test binaries
```

Everything that runs tests must therefore name a target first:

```bash
cmake --build build --target tst_slice_auto_agc   # one test
cmake --build build --target tests_core           # one subsystem
cmake --build build --target all_tests            # the lot (~271 s warm tree)
```

Then run `ctest`. **Skipping the build step is the one real footgun here**:
`ctest` on its own will happily run whatever binaries are already on disk,
which after a source change means testing your previous code and getting a
green that means nothing.

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

## Why the suite costs what it does

Two structural facts, in order of how much they cost:

**Every source file is a transitive input to every test binary.**
`NereusSDRLib` is one all-or-nothing library spanning `core`, `models`, and
`gui`, so the build graph cannot tell that a `SpectrumWidget` edit is
irrelevant to a protocol test. Touching any library source relinks all 514
tests. Building it shared made each of those relinks cheap; it did not make
the graph narrower. A subsystem split would, but 83% of tests include a
`core/` header, so even a perfect split leaves a `src/core` edit relinking
most of the suite. That is why the split was rejected.

**macOS rescans every freshly linked binary.** Gatekeeper malware-scans each
new Mach-O on first execution, which is the whole gap between the cold
(121 s) and warm (50 to 56 s) suite figures above. It used to cost far more:
while each test embedded a private 22 MB copy of the application, the same
scan ran over 12 GB of binaries and a cold run took 302 s. Exempting your
terminal under Developer Tools removes most of what remains.

One tradeoff worth knowing: the warm suite got slightly *slower* when the
app became a shared library (43 s to 50-56 s), because each of 514 test
processes now pays dyld symbol binding against a 22 MB library. Cold is the
case that matters, since any library edit relinks everything and makes the
next run cold.

Measurements and the phased fix are in
[docs/architecture/2026-07-25-test-execution-speed-design.md](../architecture/2026-07-25-test-execution-speed-design.md)
and
[docs/architecture/2026-07-25-test-execution-speed-phase1-design.md](../architecture/2026-07-25-test-execution-speed-phase1-design.md).
