# Test Execution Speed, Phase 1: Shared Application Library

**Status:** Implemented 2026-07-31. **The §2 link-time evidence below did
not reproduce at implementation time and is superseded by §2.0.** The
decision still stands, but on different grounds than originally argued.
**Date:** 2026-07-25
**Supersedes:** the subsystem-split approach in §5 Phase 1 of
[2026-07-25-test-execution-speed-design.md](2026-07-25-test-execution-speed-design.md)
**Scope:** Build system and release packaging. No application behavior changes.

---

## 1. Decision

Build `NereusSDRObjs` as a **single shared library** instead of an OBJECT
library. Do **not** split it into subsystem libraries.

This reverses the Phase 1 recommendation in the parent design doc. That
document proposed splitting `core` / `models` / `gui` so ninja could tell
what a change affects. Executing Phase 0 produced evidence that the split
is both more expensive and less effective than going shared.

---

## 2.0 Corrected evidence, measured at implementation (2026-07-31)

Same machine, same settings, both sides measured back to back on
`build/shared-app-library`: Apple Silicon, macOS 26.5.2, Qt 6.11.0,
`RelWithDebInfo`, ninja, ccache warm, `cmake --build -j8`, `ctest -j4`.
The OBJECT column was measured on the commit immediately before the flip,
not extrapolated.

| Metric | OBJECT | SHARED | Change |
| --- | --- | --- | --- |
| Touch one `src/core` file, rebuild all tests | 30 s | **22 s** | 1.4x |
| Relink 20 deleted binaries | 1 s | 1 s | none |
| Build `all_tests` from scratch | 479 s | **271 s** | 1.8x |
| `build/tests` on disk | 13 GB | **1.2 GB** | 10x |
| `tst_smoke` | 22,804,600 B | **88,728 B** | 257x |
| Full suite, cold | 362 s | **109 s** | **3.3x** |
| Full suite, warm | 42 to 44 s | 43 to 44 s | **none** |
| XProtect CPU burned during cold run | 189 s | **62 s** | 3x less |
| App RSS | 512 to 562 MB | 473 to 564 MB | within noise |
| Tests passing | 514/514 | **514/514** | none |

**Measurement hygiene, learned the hard way.** Earlier passes at this table
reported a 60% warm-suite regression that does not exist. The machine had
a `clangd` indexing run at 500% CPU and a second worktree running its own
suite, and neither was noticed because nothing recorded machine state
alongside the timings. The numbers above come from a scripted run that
samples load average and XProtect's cumulative CPU before and after every
single timing, so a contaminated sample is visible in the output instead
of silently becoming a data point. **Do not trust a timing on this
codebase that does not carry its load average.**

Two conclusions changed once measured properly:

- **There is no warm regression.** 42/43/44 s for OBJECT against
  43/43/44 s for SHARED. The dyld symbol-binding cost is real in principle
  but is lost in the noise at this scale.
- **The cold gap is larger than first reported**, 3.3x rather than 2.3x.

RSS behaved the opposite way: single samples suggested a 15% regression,
which would have failed §8's "within 10%" criterion. Three samples per
side gave 512.5 / 561.6 / 547.0 MB for OBJECT against 473.1 / 563.7 /
533.3 MB for SHARED. The spread is about 50 MB, the distributions overlap
completely, and SHARED's mean is marginally lower. The criterion is met,
and **any single-sample RSS comparison of this application is
meaningless.**

**The Developer Tools exemption did not remove the scan.** Phase 0 item 0b
predicts that exempting the build's parent app under System Settings ->
Privacy & Security -> Developer Tools removes the first-run scan. It was
enabled on the measuring machine and the parent app restarted, and
XProtect still burned 189 CPU-s during the OBJECT cold run and 62 s during
the SHARED one. Either the exemption was not applied to the right app, or
Gatekeeper assessment exemption does not cover XProtect's malware scan of
locally built Mach-Os. Worth resolving, because if 0b ever does work it
recovers most of the same cold-run time for free. It does not change this
decision either way: the shared library shrinks what there is to scan from
13 GB to 1.2 GB, which helps whether or not the exemption lands, and helps
every contributor without requiring per-machine configuration.

**The link-time case in §2 does not hold.** §2 reports 20 binaries
relinking in 73.5 s wall / 762 CPU-s, and extrapolates 31.4 min for the
full suite. Re-running that exact experiment (delete 20, rebuild) measures
**1 s**, and a touch-one-core-file rebuild of the whole suite measures
**34 s**, not 31 minutes. 38 CPU-s for a single 22 MB link is not a
plausible figure; the original sample most likely captured compile work or
a differently configured tree. Every size figure in §2 reproduced exactly,
so the discrepancy is specific to the link-time measurement.

**What the change actually buys.** Not link time. The win is macOS
XProtect: it malware-scans every freshly linked Mach-O on first execution,
and it was scanning 13 GB of test binaries on every cold run. The
instrumented figures put a number on it directly: 189 CPU-s of XProtect
during the OBJECT cold run against 62 s for SHARED. Collapsing each test
from 22 MB to 90 KB is what takes the cold suite from 362 s to 109 s.
Since any library edit relinks all 514 tests, the next run is always cold,
so the realistic edit-and-verify loop (rebuild, then run) goes from about
392 s to about 131 s, a 3x improvement. Disk drops 10x.

**No regression was found.** An earlier pass reported a 60% warm-suite
regression from dyld symbol binding. It was a measurement artifact; see
the hygiene note above. Properly instrumented, warm runs are 42-44 s on
both sides. The per-process binding cost is real in principle, so if warm
re-runs ever became the dominant workflow `-fvisibility=hidden` would be
the lever, but note that is not the cheap knob it sounds like here: the
514 test binaries link against this library's symbols, so hiding them by
default would require an export macro across the whole public surface.
§7 lists it as a non-goal and it should stay there.

**Windows verification is not available.** §5 mitigates the
`WINDOWS_EXPORT_ALL_SYMBOLS` risk with "verify the Windows CI row links all
tests before merge". That is not achievable: only the Linux CI job sets
`NEREUS_BUILD_TESTS=ON`, so the Windows job links the app and never the
test executables. The data-symbol limitation of
`WINDOWS_EXPORT_ALL_SYMBOLS` therefore remains unverified against the test
suite.

**`NEREUSSDR_ENABLE_LTO` does not exist on `main`.** §5 asks to verify a
`-DNEREUSSDR_ENABLE_LTO=ON` release build. `6ed89682` never landed on
`main`: PRs #304 and #305 brought the Phase 0 half (`EXCLUDE_FROM_ALL`,
`all_tests`, labels, timeouts) but not the LTO option. The generic
`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` release path was verified
instead.

---

## 2. Evidence (as originally written; see §2.0)

Measured on this machine (Apple Silicon, macOS 26.5.2, Qt 6.11.0,
`RelWithDebInfo`, ninja, ccache warm) by building the full tree both ways.
The shared build lived in a separate `build-shared/` directory with
`add_library(NereusSDRObjs SHARED ...)` as the only source change.

| Metric | OBJECT (today) | SHARED | Change |
| --- | --- | --- | --- |
| **Touch one `src/core` file, rebuild all tests** | **~31.4 min** | **29.5 s** | **64x** |
| `tst_smoke` binary | 22,613,768 B | 88,776 B | 255x smaller |
| `tst_slice_auto_agc` binary | 22,635,136 B | 95,440 B | 237x smaller |
| `tst_reconnect_on_silence` binary | 22,641,480 B | 105,144 B | 215x smaller |
| `build/tests` on disk | 12 GB | 1.6 GB | 7.5x smaller |
| Full suite run, cold | 438 s | 158 s | 2.8x faster |
| Full suite run, warm | n/a | 43.7 s | - |
| Tests passing | 513/513 | 512/513 | see §2.1 |
| `libNereusSDRObjs.dylib` | n/a | 22,613,464 B | built once |

The 31.4 min figure is `513 x 3.675 s`, scaled from a measured 20-binary
relink (73.5 s wall, 762 CPU-s). The 29.5 s figure is a **direct
measurement** of the same operation, not an extrapolation.

The mechanism is simple: today every test binary contains a private copy of
the entire application (25 MB of `__TEXT`). As a shared library the
application is linked once, and each test becomes a ~90 KB stub.

### 2.1 The one failing test is unrelated

`tst_tx_mic_source::concurrent_producerConsumer_noDataCorruption` failed
once during the shared suite run. It is a **pre-existing racy test**, not a
consequence of dynamic linking:

- `src/core/audio/TxMicSource.h:100` sets `kBlockFrames = 64`; the ring
  holds 512 frames, i.e. 8 blocks.
- `TxMicSource.h:115` documents that the ring **overwrites on overrun by
  design**, mirroring Thetis.
- The test pushes 32 blocks 50 us apart and asserts drained equals produced
  element-for-element, silently assuming the consumer is always scheduled
  fast enough to avoid overrun.
- The observed `drained[0] == 512` is exactly block 8's first sample: the
  fingerprint of the producer lapping the consumer once.

Reproduction rate under 8-way parallel load: 1 in 24 shared, 0 in 24
static, 0 in 12 for both when run serially. It is load-sensitive, not
link-mode-sensitive. Tracked separately; it must be fixed regardless of
whether this design is adopted.

**512 of 513 tests passing under dynamic linking is the important result.**
It means static-initialization order and singleton identity survive the
change, which was the main correctness risk.

---

## 3. Why not the subsystem split

Two findings from the Phase 0 include audit:

**It is capped by the dependency reality.** 83% of tests include a `core/`
header. Even a perfect split leaves a `src/core` edit relinking ~425 of 513
test binaries. The split mainly helps GUI edits, which are the cheaper case
already.

**It requires a design decision first.** Of the 26 upward `core -> models`
includes, 16 are mechanical (`Band.h` x14, the `DSPMode` enum x2), but 10
are genuine dependencies in five translation units:

| Unit | Depends on |
| --- | --- |
| `src/core/AudioEngine.cpp` | `RadioModel`, `SliceModel` |
| `src/core/SupportBundle.cpp` | `RadioModel` |
| `src/core/TwoToneController.{h,cpp}` | `SliceModel`, `TransmitModel`, `Band` |
| `src/core/TciServer.cpp` | `RadioModel`, `SliceModel`, `TransmitModel` |
| `src/core/MicProfileManager.cpp` | `TransmitModel` |

These are application-layer coordinators filed under `core`. Splitting
cleanly means deciding where they belong, most plausibly a new `app` layer.

Going shared needs none of that. The 12 upward includes stop being on the
critical path entirely, and the fix applies to core edits, which the split
cannot help.

---

## 4. Design

### 4.1 The build change

```cmake
add_library(NereusSDRObjs SHARED ...)   # was OBJECT
```

Everything downstream already uses `target_link_libraries(... NereusSDRObjs)`
and needs no change. Verified: all 513 test targets and the main executable
link and run.

Three follow-ons:

1. **Position-independent code.** CMake sets `-fPIC` automatically for
   `SHARED`. Confirm `POSITION_INDEPENDENT_CODE` is not being forced off
   anywhere.
2. **Windows symbol export.** Set `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON` (or
   `WINDOWS_EXPORT_ALL_SYMBOLS` on the target). The tree currently has no
   `__declspec` annotations and no export macro, so without this the DLL
   exports nothing and every test fails to link on Windows.
3. **Target rename.** `NereusSDRObjs` becomes a misnomer once it is not an
   OBJECT library. Rename to `NereusSDRLib` in the same change, since every
   call site is touched anyway. Optional but cheap.

### 4.2 Optional refinement: `LINK_DEPENDS_NO_SHARED`

CMake's `LINK_DEPENDS_NO_SHARED` property tells a target not to treat a
linked shared library as a link-order dependency, so dependents are not
relinked when only the library's implementation changes.

The measurements show this is **not required**: relinking 513 ~90 KB stubs
is part of the 29.5 s and is no longer a bottleneck. Treat it as a possible
later refinement, not part of this change. It carries a real hazard, since
a genuine ABI change would then not trigger relinks.

### 4.3 Packaging: the actual work

This is where the effort is, and where the risk is. **The project has
already shipped a shared library (`librade`) on all three platforms**, so
there is a working pattern to copy and a documented failure to avoid.

**macOS.** The app is a `MACOSX_BUNDLE` (CMakeLists.txt:876). The dylib
must land in `Contents/Frameworks/` with the executable's install name
resolving via `@executable_path/../Frameworks`. Note CMakeLists.txt:898
runs `codesign --force --sign -` on the bundle as a POST_BUILD step:
**signing order matters.** The dylib must be signed before the bundle is
sealed, or the bundle signature will not validate. `macdeployqt` handles Qt
frameworks but will need the dylib present first.

**Linux.** `linuxdeploy` plus `linuxdeploy-plugin-qt` build the AppImage.
There is a documented quirk, from the `librade` experience at
`.github/workflows/release.yml:258-283`: linuxdeploy traces `NEEDED` ELF
deps via `ld.so` / `LD_LIBRARY_PATH` / `RPATH` and **does not scan
`AppDir/usr/lib`**, and its `--library` flag did not work in the continuous
build (1-alpha @a9f929f). The established workaround is to prepend the
build-tree directory to `LD_LIBRARY_PATH` before invoking linuxdeploy.
Apply the same pattern, or add an `install()` rule for the new library so
the `cmake --install` step places it in `AppDir/usr/lib` and the problem
does not arise.

**Windows.** `windeployqt` resolves **only Qt DLLs**
(`.github/workflows/release.yml:724`). Every non-Qt runtime DLL is staged
by an explicit copy step: `libfftw3-3.dll`, `libfftw3f-3.dll`,
`deepfilter.dll`, `rade.dll`. The new DLL needs the same treatment in both
the portable ZIP and the NSIS installer, plus the existing
fail-loudly-if-missing guard pattern.

### 4.4 Test-side change

None required. `nereus_add_test()` already does
`target_link_libraries(${name} PRIVATE NereusSDRObjs Qt6::Test)`, which
works unchanged against a shared target. The `nereus_test_sandbox` OBJECT
library added in Phase 0 also works unchanged and must stay an OBJECT
library for the reasons documented at `tests/CMakeLists.txt`.

---

## 5. Risks

| Risk | Severity | Mitigation |
| --- | --- | --- |
| **A missing DLL/so/dylib means the app does not launch at all** | **High** | This exact failure shipped in v0.5.0: `rade.dll` was absent from the Windows installer and portable ZIP, and the .exe failed on a clean install with "rade.dll was not found" (fixed in v0.5.1, PR #250). Adding a second shared library doubles that surface. Add an explicit presence check to the release workflow for all three artifacts, and smoke-launch each artifact on a clean machine before release. |
| Windows exports nothing without `WINDOWS_EXPORT_ALL_SYMBOLS` | High | Set it; verify the Windows CI row links all tests before merge. |
| macOS bundle signature invalid if dylib signed after sealing | Medium | Sign the dylib first; verify with `codesign --verify --deep --strict`. |
| linuxdeploy fails to find the library | Medium | Known quirk with a documented workaround (§4.3); prefer an `install()` rule. |
| Dead-stripping is lost, raising resident memory | Medium | Measure RSS of the running app before and after. The app already loads essentially all of its own code, so the delta should be small, but it is unmeasured. |
| Interaction with `NEREUSSDR_ENABLE_LTO` | Medium | LTO defaults OFF since `6ed89682`. Verify a `-DNEREUSSDR_ENABLE_LTO=ON` release build still links. |
| Static-init order or singleton identity changes | Low | Largely retired by evidence: 512/513 tests pass under dynamic linking. Re-verify on Linux and Windows. |
| Symbol interposition changes behavior | Low | Single library, no plugin boundary. Consider `-fvisibility=hidden` later as a size and load-time optimization; not part of this change. |

---

## 6. Sequencing

1. **Land `6ed89682` on `main` first.** It carries `EXCLUDE_FROM_ALL` on
   tests plus the `all_tests` aggregate and LTO-off. It is already written
   and exercised on three branches, and it touches the same two CMake files
   this change does. Doing it first avoids a conflict and delivers the
   routine-build win immediately.
2. macOS: flip to `SHARED`, fix the bundle layout and signing order, verify
   the suite and a launched app.
3. Linux: verify AppImage build and launch.
4. Windows: add `WINDOWS_EXPORT_ALL_SYMBOLS`, stage the DLL, verify the ZIP
   and NSIS installer launch on a clean VM.
5. Measure and record the real numbers on all three platforms.

Each platform is a separate reviewable step. Do not merge until all three
artifacts have been launched from a clean environment.

---

## 7. Non-goals

- No application behavior changes.
- No subsystem split. If per-subsystem granularity is ever wanted, it can
  be layered on later; this change does not preclude it.
- No `LINK_DEPENDS_NO_SHARED` (see §4.2).
- No `-fvisibility=hidden` pass.
- No fix for `tst_tx_mic_source` here; it is a pre-existing flake tracked
  separately and independent of this design.

---

## 8. Acceptance

- Touch one `src/core` file, rebuild all tests: **under 60 s** (measured
  29.5 s on macOS; allow headroom for slower CI hardware).
- `build/tests` under 3 GB.
- Full suite green on macOS, Linux, and Windows.
- All three release artifacts launch from a clean environment with no
  missing-library error.
- Resident memory of the running app within 10% of the current build.
