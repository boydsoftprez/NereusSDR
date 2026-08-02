# Remote Daemon R1: Build Split and Daemon Skeleton, Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the monolithic build so a headless `nereusd` links no widget toolkit, and get that daemon connecting to a radio and running the RX chain to N slices on a Raspberry Pi 4.

**Architecture:** `NereusSDRObjs` becomes an aggregate over two new libraries, `NereusCore` (core + models) and `NereusGui` (GUI + resources). Six extractions move spectrum production out of QWidget classes into `src/core/`, because a daemon built without them connects to a radio and emits no spectrum at all. A new `nereusd` executable links only `NereusCore`.

**Tech Stack:** C++20, Qt6 (Core / Gui / Network / WebSockets / Multimedia / SerialPort in core; Widgets / Svg / GuiPrivate only in GUI), CMake + Ninja, FFTW3f, WDSP, Qt Test.

**Design doc:** [2026-07-28-remote-daemon-architecture-design.md](2026-07-28-remote-daemon-architecture-design.md). R1 scope is §15; the extraction list is §4.2; measured hardware capacity is §4.5a.

## Global Constraints

- **Hardware floor: Raspberry Pi 4 Model B Rev 1.5, quad Cortex-A72, 8 GB.** Pi 5 is the preferred target, not the minimum (§4.5).
- **No `goto`, no raw `new`/`delete`, no `#define` for constants, braces on all control flow.** Classes `PascalCase`, methods `camelCase`, constants `kPascalCase`, members `m_camelCase`. Per CLAUDE.md.
- **Platform guards use `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX`**, never `_WIN32` or `__APPLE__`.
- **Settings go through `AppSettings`, never `QSettings`.**
- **All commits GPG-signed.** Never `--no-gpg-sign`. No `Co-Authored-By: Claude` trailer.
- **No em-dash (`—`) or en-dash (`–`) characters** in any file, commit message, or comment.
- **Relocated files keep their upstream licence headers byte-for-byte**, and their `docs/attribution/THETIS-PROVENANCE.md` rows get path updates in the same commit. `SpectrumDetector` and `SpectrumAvenger` are verbatim WDSP `analyzer.c` ports; moving them is not a re-port, but the PROVENANCE paths must still track.
- **Do not run the full test suite except once at the very end.** It takes roughly 37 minutes, 32 of it linking. Iterate with `ctest -R <name>` or `ctest -L <label>`. See `docs/development/fast-test-loop.md`.
- **Every task ends green:** the targeted tests pass and the tree builds before you commit.

## Two corrections to the design doc, applied here

**1. Target kind is OBJECT, not SHARED.** §4.1 specifies two `SHARED` libraries on the basis that `main` carries an approved shared-library decision. It does not: `docs/architecture/2026-07-25-test-execution-speed-phase1-design.md:3` reads `**Status:** Design, pending approval`, and `CMakeLists.txt:959` still declares `add_library(NereusSDRObjs OBJECT ...)`. This plan therefore splits into two **OBJECT** libraries, preserving today's semantics exactly.

The choice is one word per target and nothing else in R1 depends on it. When phase 1 is approved, changing `OBJECT` to `SHARED` on both new targets is the whole conversion, and it can land before or after R1 without conflict. Update §4.1 to say so.

**2. The test call-site count is 589, not 561.** `grep -cE '^\s*nereus_add_test\(' tests/CMakeLists.txt` returns 589 on `main`. The design's 561 is stale. The mechanism is unchanged and Task 1 still protects it.

---

## File Structure

**New core files:**

| File | Responsibility |
| --- | --- |
| `src/core/spectrum/SpectrumDetectorMode.h` | The `SpectrumDetector` enum alone, so nothing needs `gui/SpectrumWidget.h` for it |
| `src/core/spectrum/SpectrumDetector.{h,cpp}` | Relocated from `src/gui/spectrum/`, WDSP `detector()` port |
| `src/core/spectrum/SpectrumAvenger.{h,cpp}` | Relocated from `src/gui/spectrum/`, WDSP `avenger()` port |
| `src/core/spectrum/ISpectrumSink.h` | Abstract sink `RadioModel` holds instead of `SpectrumWidget*` |
| `src/core/spectrum/SpectrumReducer.{h,cpp}` | Crop and reduce: explicit pixel count and `(centreHz, spanHz)`, no widget geometry |
| `src/core/spectrum/FftEnginePool.{h,cpp}` | Owns per-stream `FFTEngine`s and their thread |
| `src/core/spectrum/FftTopology.{h,cpp}` | Builds `FFTRouter` mappings from subscriptions rather than from widgets |
| `src/core/CoreInit.{h,cpp}` | Shared startup both binaries call: settings migration, logging, wisdom |
| `src/core/daemon/DaemonConfig.{h,cpp}` | Parses and validates the daemon config file |
| `src/core/daemon/DaemonApp.{h,cpp}` | Headless lifecycle: connect, slices, endpoints, shutdown |
| `src/server_main.cpp` | `nereusd` entry point |

**Modified:**

| File | Change |
| --- | --- |
| `CMakeLists.txt:959-1010` | Split targets; aggregate preserves the `NereusSDRObjs` name |
| `src/models/RadioModel.{h,cpp}` | Hold `ISpectrumSink*` instead of `SpectrumWidget*` |
| `src/gui/SpectrumWidget.{h,cpp}` | Implement `ISpectrumSink`; delegate reduction to `SpectrumReducer` |
| `src/gui/MainWindow.{h,cpp}` | Delegate pool and topology to the extracted services |
| `src/gui/spectrum/` | Emptied of `SpectrumDetector` and `SpectrumAvenger` |
| `src/main.cpp:280` | Call `CoreInit::initialize()` instead of migrating inline |
| `docs/attribution/THETIS-PROVENANCE.md` | Path updates for the two relocated files |
| `packaging/` | systemd unit and config sample |

---

### Task 1: Split the CMake targets, preserving the `NereusSDRObjs` name

Pure build surgery. No source file moves, no code changes. This must be provably behaviour-preserving before anything else happens, because 589 test targets depend on the name.

**Files:**
- Modify: `CMakeLists.txt:959-1010`

**Interfaces:**
- Consumes: nothing.
- Produces: CMake targets `NereusCore` (OBJECT) and `NereusGui` (OBJECT), plus `NereusSDRObjs` as an INTERFACE library aggregating both. Every existing `target_link_libraries(x PRIVATE NereusSDRObjs)` keeps working unchanged.

- [ ] **Step 1: Capture the baseline so you can prove nothing changed**

```bash
cd /Users/j.j.boyd/NereusSDR
cmake -B /tmp/r1-before -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
ninja -C /tmp/r1-before -t targets all | sort > /tmp/targets-before.txt
wc -l /tmp/targets-before.txt
grep -cE '^\s*nereus_add_test\(' tests/CMakeLists.txt   # expect 589
```

- [ ] **Step 2: Replace the target block**

In `CMakeLists.txt`, replace the `add_library(NereusSDRObjs OBJECT ...)` block at line 959 and the Linux-only `target_sources` that follows it with:

```cmake
# Split for the headless daemon (docs/architecture/2026-07-28-remote-daemon-architecture-design.md
# section 4.1).  NereusCore must reference no widget symbol so nereusd can link
# it alone.  NereusSDRObjs survives as an INTERFACE aggregate because 589
# nereus_add_test() call sites link it by name and reuse its PCH.
add_library(NereusCore OBJECT
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
)

add_library(NereusGui OBJECT
    ${GUI_SOURCES}
    ${RESOURCES}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(NereusGui PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
endif()

add_library(NereusSDRObjs INTERFACE)
target_link_libraries(NereusSDRObjs INTERFACE NereusCore NereusGui)
```

- [ ] **Step 3: Retarget the link and property calls**

Every `target_link_libraries(NereusSDRObjs PUBLIC ...)` and
`target_include_directories(NereusSDRObjs ...)` and
`target_compile_definitions(NereusSDRObjs ...)` in `CMakeLists.txt` must now name a
concrete target. INTERFACE libraries cannot carry `PUBLIC`.

Mechanical rule: **rename `NereusSDRObjs` to `NereusCore` in all of them, then move the
GUI-only ones to `NereusGui`.** The GUI-only ones are `Qt6::Widgets`, `Qt6::Svg`, and
`Qt6::GuiPrivate` (line 1316). Everything else stays on `NereusCore`. Since `NereusGui`
links `NereusCore` publicly, GUI sources still see every core dependency.

Add after the two libraries are declared:

```cmake
target_link_libraries(NereusGui PUBLIC NereusCore)
```

- [ ] **Step 4: Retarget the LTO twin and the executable**

The LTO block at line 984 and the executable at line 999 both name `NereusSDRObjs_LTO`.
Split it the same way:

```cmake
if(NEREUSSDR_LTO_AVAILABLE)
    add_library(NereusCore_LTO OBJECT ${CORE_SOURCES} ${MODEL_SOURCES})
    add_library(NereusGui_LTO  OBJECT ${GUI_SOURCES}  ${RESOURCES})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(NereusGui_LTO PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
    endif()
    target_link_libraries(NereusCore_LTO PUBLIC NereusCore)
    target_link_libraries(NereusGui_LTO  PUBLIC NereusGui)
    foreach(_t NereusCore_LTO NereusGui_LTO)
        set_target_properties(${_t} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endforeach()
endif()
```

and in the executable block replace `target_link_libraries(NereusSDR PRIVATE NereusSDRObjs_LTO)`
with `target_link_libraries(NereusSDR PRIVATE NereusCore_LTO NereusGui_LTO)`.

Note: linking `NereusCore_LTO PUBLIC NereusCore` would compile the sources twice into one
link line. Do **not** do that. Drop the two `target_link_libraries(..._LTO PUBLIC ...)`
lines above and instead copy the dependency lists, or simplest: give the LTO twins the
same `target_link_libraries` calls the non-LTO ones get, by hoisting those calls into a
CMake function taking the target name. Use the function; duplicating 20 link lines twice
is how they drift.

- [ ] **Step 5: Fix the test helper's PCH source**

`tests/CMakeLists.txt:114` `nereus_add_test` does `target_precompile_headers(${name} REUSE_FROM NereusSDRObjs)`. An INTERFACE library has no PCH to reuse. Change that one line to:

```cmake
        target_precompile_headers(${name} REUSE_FROM NereusCore)
```

Leave `target_link_libraries(${name} PRIVATE NereusSDRObjs Qt6::Test)` alone. That is the
whole point of keeping the name.

- [ ] **Step 6: Prove the target graph is unchanged**

```bash
cmake -B /tmp/r1-after -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
ninja -C /tmp/r1-after -t targets all | sort > /tmp/targets-after.txt
diff <(grep -v 'NereusCore\|NereusGui' /tmp/targets-before.txt) \
     <(grep -v 'NereusCore\|NereusGui' /tmp/targets-after.txt) && echo "TARGET GRAPH UNCHANGED"
```

Expected: `TARGET GRAPH UNCHANGED`. Any other output means a test target was lost; fix
before continuing.

- [ ] **Step 7: Build the app and a sample of tests**

```bash
cmake --build /tmp/r1-after --target NereusSDR -j$(sysctl -n hw.ncpu)
cmake --build /tmp/r1-after --target tst_radio_model tst_fft_engine -j$(sysctl -n hw.ncpu)
ctest --test-dir /tmp/r1-after -R 'tst_radio_model|tst_fft_engine' --output-on-failure
```

Expected: app links, both tests build and pass.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt
git commit -S -m "build(r1): split NereusSDRObjs into NereusCore and NereusGui

NereusSDRObjs survives as an INTERFACE aggregate over the two, so all 589
nereus_add_test call sites link it by name unchanged.  The helper's
REUSE_FROM moves to NereusCore because an INTERFACE library carries no PCH.

Target kind stays OBJECT.  The shared-library conversion in
2026-07-25-test-execution-speed-phase1-design.md is still pending approval and
is orthogonal: it is one word per target and can land either side of this."
```

---

### Task 2: Extract the `SpectrumDetector` enum into a core header

`SpectrumDetector.h:62` includes `gui/SpectrumWidget.h` solely to get the enum, and that header drags in `<QWidget>`, `<QRhiWidget>` and `<rhi/qrhi.h>`. Until this is done, the "GUI-free" spectrum reducer is not GUI-free and Task 3 cannot move it.

**Files:**
- Create: `src/core/spectrum/SpectrumDetectorMode.h`
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/spectrum/SpectrumDetector.h`
- Test: `tests/tst_spectrum_detector_mode.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `enum class SpectrumDetectorMode { Peak, Rosenfell, Average, Sample, Rms }` in `namespace NereusSDR`, header `core/spectrum/SpectrumDetectorMode.h`. `SpectrumWidget` keeps a `using SpectrumDetector = SpectrumDetectorMode;` alias so no call site changes in this task.

- [ ] **Step 1: Read the existing enum before you move it**

```bash
grep -n "enum class SpectrumDetector" -A 12 src/gui/SpectrumWidget.h
```

Copy the enumerators and their order **exactly**. The values map to WDSP `analyzer.c`
detector modes and are wire-significant; renumbering them silently changes the display.

- [ ] **Step 2: Write the failing test**

Create `tests/tst_spectrum_detector_mode.cpp`:

```cpp
#include <QtTest>
#include "core/spectrum/SpectrumDetectorMode.h"

class TstSpectrumDetectorMode : public QObject {
    Q_OBJECT
private slots:
    // The enumerator values are WDSP analyzer.c detector modes.  Pin them so a
    // future reorder cannot silently change what the display draws.
    void valuesArePinned()
    {
        using M = NereusSDR::SpectrumDetectorMode;
        QCOMPARE(static_cast<int>(M::Peak),      0);
        QCOMPARE(static_cast<int>(M::Rosenfell), 1);
        QCOMPARE(static_cast<int>(M::Average),   2);
        QCOMPARE(static_cast<int>(M::Sample),    3);
        QCOMPARE(static_cast<int>(M::Rms),       4);
    }

    // The whole point of the extraction: this header must not pull in a widget.
    void headerIsWidgetFree()
    {
        QVERIFY(true);   // compiling this TU at all proves it
    }
};

QTEST_MAIN(TstSpectrumDetectorMode)
#include "tst_spectrum_detector_mode.moc"
```

If Step 1 showed different enumerators or a different order, **use what Step 1 showed**,
not what is written above.

- [ ] **Step 3: Register the test and run it to see it fail**

Add to `tests/CMakeLists.txt` alongside the other spectrum tests:

```cmake
nereus_add_test(tst_spectrum_detector_mode)
```

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build --target tst_spectrum_detector_mode 2>&1 | tail -5
```

Expected: FAIL, `core/spectrum/SpectrumDetectorMode.h` file not found.

- [ ] **Step 4: Create the header**

`src/core/spectrum/SpectrumDetectorMode.h`:

```cpp
#pragma once
// =================================================================
// src/core/spectrum/SpectrumDetectorMode.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Enumeration only, extracted from
// gui/SpectrumWidget.h so that SpectrumDetector and SpectrumAvenger can
// live in src/core/ without dragging in QWidget and the QRhi stack.
// The values are WDSP analyzer.c detector modes and are pinned by
// tests/tst_spectrum_detector_mode.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 2 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

namespace NereusSDR {

/// Bin-to-pixel reduction mode. Mirrors the WDSP analyzer detector modes.
enum class SpectrumDetectorMode {
    Peak      = 0,
    Rosenfell = 1,
    Average   = 2,
    Sample    = 3,
    Rms       = 4,
};

} // namespace NereusSDR
```

Match the enumerators to Step 1's output.

- [ ] **Step 5: Point the old name at the new one**

In `src/gui/SpectrumWidget.h`, delete the enum definition and add near the top:

```cpp
#include "core/spectrum/SpectrumDetectorMode.h"
```

then inside the class (or the same scope the enum previously occupied):

```cpp
    // Extracted to core in R1; alias kept so existing call sites are untouched.
    using SpectrumDetector = NereusSDR::SpectrumDetectorMode;
```

In `src/gui/spectrum/SpectrumDetector.h`, replace the `#include "gui/SpectrumWidget.h"`
at line 62 with `#include "core/spectrum/SpectrumDetectorMode.h"` and change the type it
uses to `NereusSDR::SpectrumDetectorMode`.

- [ ] **Step 6: Run the test and a build**

```bash
cmake --build build --target tst_spectrum_detector_mode NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_spectrum_detector_mode --output-on-failure
grep -c 'gui/SpectrumWidget.h' src/gui/spectrum/SpectrumDetector.h   # expect 0
```

Expected: PASS, app builds, grep returns 0.

- [ ] **Step 7: Commit**

```bash
git add src/core/spectrum/SpectrumDetectorMode.h src/gui/SpectrumWidget.h \
        src/gui/spectrum/SpectrumDetector.h tests/tst_spectrum_detector_mode.cpp \
        tests/CMakeLists.txt
git commit -S -m "refactor(r1): extract SpectrumDetector enum to a core header

SpectrumDetector.h included gui/SpectrumWidget.h purely for this enum, and
that header pulls QWidget, QRhiWidget and rhi/qrhi.h, so the reducer was not
GUI-free despite having no other widget dependency.  SpectrumWidget keeps a
using-alias so no call site changes.  Enumerator values are pinned by test
because they are WDSP analyzer.c detector modes."
```

---

### Task 3: Relocate `SpectrumDetector` and `SpectrumAvenger` to `src/core/spectrum/`

**Files:**
- Move: `src/gui/spectrum/SpectrumDetector.{h,cpp}` and `SpectrumAvenger.{h,cpp}` to `src/core/spectrum/`
- Modify: `CMakeLists.txt` (`CORE_SOURCES` gains them, `GUI_SOURCES` loses them), `docs/attribution/THETIS-PROVENANCE.md`

**Interfaces:**
- Consumes: `NereusSDR::SpectrumDetectorMode` from Task 2.
- Produces: same classes, same signatures, at `core/spectrum/SpectrumDetector.h` and `core/spectrum/SpectrumAvenger.h`.

- [ ] **Step 1: Move with `git mv` so history follows**

```bash
git mv src/gui/spectrum/SpectrumDetector.h   src/core/spectrum/
git mv src/gui/spectrum/SpectrumDetector.cpp src/core/spectrum/
git mv src/gui/spectrum/SpectrumAvenger.h    src/core/spectrum/
git mv src/gui/spectrum/SpectrumAvenger.cpp  src/core/spectrum/
```

- [ ] **Step 2: Update the path comment in each header, and nothing else**

Each file's header comment opens with its own path (for example
`// src/gui/spectrum/SpectrumDetector.h  (NereusSDR)`). Update that line only.

**Do not touch the WDSP copyright block or the GPL permission text.** These are verbatim
`analyzer.c` ports and the notices must survive byte-for-byte. Append one line to the
existing `Modification history (NereusSDR)` block:

```
//   2026-08-02  J.J. Boyd / KG4VCF  Relocated from src/gui/spectrum/ to
//                                    src/core/spectrum/ for the headless
//                                    daemon build split (R1).  No logic
//                                    change.  AI-assisted transformation via
//                                    Anthropic Claude Code.
```

- [ ] **Step 3: Update includes across the tree**

```bash
grep -rln 'gui/spectrum/SpectrumDetector.h\|gui/spectrum/SpectrumAvenger.h' src tests
```

Replace each with `core/spectrum/...`. Expect hits in `SpectrumWidget.cpp` and the
existing spectrum tests.

- [ ] **Step 4: Move them between the CMake source lists**

In `CMakeLists.txt`, remove the four `src/gui/spectrum/Spectrum{Detector,Avenger}.cpp`
entries from `GUI_SOURCES` (starting line 676) and add them to `CORE_SOURCES`
(starting line 493) as `src/core/spectrum/...`.

- [ ] **Step 5: Update PROVENANCE paths**

```bash
grep -n 'SpectrumDetector\|SpectrumAvenger' docs/attribution/THETIS-PROVENANCE.md
```

Change the NereusSDR-path column of each row from `src/gui/spectrum/...` to
`src/core/spectrum/...`. Leave the upstream file, version, and licence columns alone.

- [ ] **Step 6: Verify the move did not break attribution or the build**

```bash
python3 scripts/verify-thetis-headers.py
python3 scripts/check-new-ports.py
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build --target NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R 'spectrum' --output-on-failure
```

Expected: both scripts pass, app builds, existing spectrum tests pass unchanged.

- [ ] **Step 7: Commit**

```bash
git add -A src/core/spectrum src/gui CMakeLists.txt docs/attribution/THETIS-PROVENANCE.md tests
git commit -S -m "refactor(r1): relocate SpectrumDetector and SpectrumAvenger to core

Both are verbatim WDSP analyzer.c ports with no GUI dependency once the
detector enum moved out in the previous commit.  git mv preserves history;
WDSP copyright and GPL blocks are untouched; PROVENANCE rows get path updates
in this same commit.  No logic change."
```

---

### Task 4: Extract the view-hook interface out of `RadioModel`

`RadioModel` holds two non-owning view hooks, `m_spectrumWidget` and `m_fftEngine`, wired once at `MainWindow.cpp:3103-3104`. The first is the single `#include "gui/"` in all of `src/core` and `src/models`, and it is what stops `NereusCore` compiling alone.

**Files:**
- Create: `src/core/spectrum/ISpectrumSink.h`
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp:299`, `src/gui/SpectrumWidget.h`
- Test: `tests/tst_core_has_no_gui_includes.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class ISpectrumSink` in `namespace NereusSDR` with the methods `RadioModel` actually calls. `RadioModel::setSpectrumSink(ISpectrumSink*)` replaces `setSpectrumWidget(SpectrumWidget*)`.

- [ ] **Step 1: Find out what `RadioModel` actually calls on the widget**

```bash
grep -n 'm_spectrumWidget->' src/models/RadioModel.cpp
```

The interface gets **exactly** these methods and no others. Write the list down; you need
it in Step 3. Do not add methods speculatively.

- [ ] **Step 2: Write the failing test**

Create `tests/tst_core_has_no_gui_includes.cpp`. This is a guard test, not a unit test:
it asserts the property the whole daemon build depends on.

```cpp
#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>

class TstCoreHasNoGuiIncludes : public QObject {
    Q_OBJECT
private slots:
    // src/core and src/models must never include src/gui.  This is the
    // invariant that lets nereusd link NereusCore without a widget toolkit.
    void coreNeverIncludesGui()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QStringList offenders;
        for (const QString& sub : {QStringLiteral("/src/core"), QStringLiteral("/src/models")}) {
            QDirIterator it(root + sub, {"*.h", "*.cpp", "*.mm"},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
                QTextStream ts(&f);
                int line = 0;
                while (!ts.atEnd()) {
                    ++line;
                    const QString l = ts.readLine();
                    if (l.contains(QStringLiteral("#include \"gui/"))) {
                        offenders << QStringLiteral("%1:%2").arg(path).arg(line);
                    }
                }
            }
        }
        if (!offenders.isEmpty()) {
            qWarning() << "core/models include gui:" << offenders;
        }
        QCOMPARE(offenders.size(), 0);
    }
};

QTEST_MAIN(TstCoreHasNoGuiIncludes)
#include "tst_core_has_no_gui_includes.moc"
```

- [ ] **Step 3: Register it and watch it fail**

```cmake
nereus_add_test(tst_core_has_no_gui_includes)
target_compile_definitions(tst_core_has_no_gui_includes PRIVATE
    NEREUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
```

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build --target tst_core_has_no_gui_includes
ctest --test-dir build -R tst_core_has_no_gui_includes --output-on-failure
```

Expected: FAIL, reporting `src/models/RadioModel.cpp:299`.

- [ ] **Step 4: Write the interface**

`src/core/spectrum/ISpectrumSink.h`. Replace the method list below with what Step 1
actually found:

```cpp
#pragma once
// =================================================================
// src/core/spectrum/ISpectrumSink.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Abstract sink that RadioModel holds
// in place of a concrete SpectrumWidget*, so src/core and src/models
// contain no reference to src/gui and nereusd can link NereusCore alone.
// SpectrumWidget implements it; the daemon supplies its own implementation
// in a later phase.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 1 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

namespace NereusSDR {

/// Non-owning display sink. Implementations must tolerate being called from
/// the thread RadioModel runs on (the main thread today).
class ISpectrumSink {
public:
    virtual ~ISpectrumSink() = default;

    // ONE METHOD PER m_spectrumWidget-> CALL FOUND IN STEP 1. Example shape:
    virtual void setVfoFrequency(double hz) = 0;
    virtual void setFilterOffset(int lowHz, int highHz) = 0;
};

} // namespace NereusSDR
```

- [ ] **Step 5: Swap `RadioModel` over**

In `RadioModel.h`, replace the forward declaration and member:

```cpp
#include "core/spectrum/ISpectrumSink.h"
...
    void setSpectrumSink(NereusSDR::ISpectrumSink* sink) { m_spectrumSink = sink; }
    NereusSDR::ISpectrumSink* spectrumSink() const { return m_spectrumSink; }
...
    NereusSDR::ISpectrumSink* m_spectrumSink{nullptr};
```

Delete `#include "gui/SpectrumWidget.h"` from `RadioModel.cpp:299` and rename every
`m_spectrumWidget->` to `m_spectrumSink->`.

Keep `setSpectrumWidget` as a deprecated inline forwarding to `setSpectrumSink` **only if**
Step 1 found call sites outside `MainWindow.cpp:3103`. Otherwise delete it and update that
one call site.

- [ ] **Step 6: Make `SpectrumWidget` implement it**

In `src/gui/SpectrumWidget.h`, add `#include "core/spectrum/ISpectrumSink.h"`, add
`public NereusSDR::ISpectrumSink` to the base list, and mark the matching methods
`override`. They already exist; you are only adding the keyword.

- [ ] **Step 7: Run the guard test and build**

```bash
cmake --build build --target tst_core_has_no_gui_includes NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_core_has_no_gui_includes --output-on-failure
ctest --test-dir build -R 'tst_radio_model' --output-on-failure
```

Expected: guard test PASSES (zero offenders), app builds, `tst_radio_model` still passes.

- [ ] **Step 8: Commit**

```bash
git add src/core/spectrum/ISpectrumSink.h src/models/RadioModel.h src/models/RadioModel.cpp \
        src/gui/SpectrumWidget.h src/gui/MainWindow.cpp \
        tests/tst_core_has_no_gui_includes.cpp tests/CMakeLists.txt
git commit -S -m "refactor(r1): RadioModel holds ISpectrumSink, not SpectrumWidget

Removes the only #include \"gui/\" in src/core and src/models, which is the
invariant nereusd needs to link NereusCore without a widget toolkit.  A new
guard test walks both trees and fails on any reintroduction, so this cannot
silently regress."
```

---

### Task 5: Extract crop-and-reduce into a core service

This is a refactor, not a relocation. The stage lives inside a QRhiWidget, `visibleBinRange()` is private, and the pixel count is derived from widget geometry (`const int displayWidth = qMax(width() - effectiveStripW(), 800);` at `SpectrumWidget.cpp:2679`, where `effectiveStripW()` is itself a visibility query). Design §9.4 requires pixel count decoupled from widget width; today it is the opposite.

**Files:**
- Create: `src/core/spectrum/SpectrumReducer.{h,cpp}`
- Modify: `src/gui/SpectrumWidget.cpp:2679+`
- Test: `tests/tst_spectrum_reducer.cpp`

**Interfaces:**
- Consumes: `SpectrumDetector`, `SpectrumAvenger` (Task 3), `SpectrumDetectorMode` (Task 2).
- Produces:

```cpp
namespace NereusSDR {
struct ReducerConfig {
    int    pixels        {1024};   // explicit, never widget geometry
    double centreHz      {0.0};    // crop window centre
    double spanHz        {0.0};    // crop window width
    double streamCentreHz{0.0};    // DDC centre the bins are relative to
    double sampleRateHz  {0.0};
    SpectrumDetectorMode detector {SpectrumDetectorMode::Peak};
    int    averageMode   {0};
    double averageTau    {0.12};
};

class SpectrumReducer {
public:
    void setConfig(const ReducerConfig& cfg);
    const ReducerConfig& config() const;
    /// binsLinear is |X[k]|^2 as FFTEngine::fftReadyLinear emits.
    /// Returns exactly config().pixels dBm values. Never resizes to a widget.
    const QVector<float>& reduce(const QVector<float>& binsLinear,
                                 double windowEnb, double dbmOffset);
    /// First and last source bin covered by the crop window, clamped to range.
    static std::pair<int,int> visibleBinRange(int binCount, const ReducerConfig& cfg);
};
}
```

- [ ] **Step 1: Write the failing tests**

`tests/tst_spectrum_reducer.cpp`:

```cpp
#include <QtTest>
#include "core/spectrum/SpectrumReducer.h"

using namespace NereusSDR;

class TstSpectrumReducer : public QObject {
    Q_OBJECT
private slots:
    // Design section 9.4: the pixel count is a bandwidth control, and must
    // come from config alone.  No widget geometry may influence it.
    void outputSizeAlwaysMatchesRequestedPixels()
    {
        SpectrumReducer r;
        for (int px : {128, 512, 1024, 1184}) {
            ReducerConfig c;
            c.pixels = px;
            c.centreHz = 14'200'000.0; c.spanHz = 192'000.0;
            c.streamCentreHz = 14'200'000.0; c.sampleRateHz = 192'000.0;
            r.setConfig(c);
            QVector<float> bins(4096, 1.0e-9f);
            QCOMPARE(r.reduce(bins, 1.0, 0.0).size(), px);
        }
    }

    // Full-span crop must select every bin.
    void fullSpanSelectsAllBins()
    {
        ReducerConfig c;
        c.centreHz = 14'200'000.0; c.spanHz = 192'000.0;
        c.streamCentreHz = 14'200'000.0; c.sampleRateHz = 192'000.0;
        auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 0);
        QCOMPARE(last, 4095);
    }

    // A centred half-span crop must select the middle half.
    void halfSpanSelectsMiddleHalf()
    {
        ReducerConfig c;
        c.centreHz = 14'200'000.0; c.spanHz = 96'000.0;
        c.streamCentreHz = 14'200'000.0; c.sampleRateHz = 192'000.0;
        auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 1024);
        QCOMPARE(last, 3071);
    }

    // Off-centre crops must not walk off the end of the bin array.
    void cropIsClampedToRange()
    {
        ReducerConfig c;
        c.centreHz = 14'290'000.0; c.spanHz = 192'000.0;
        c.streamCentreHz = 14'200'000.0; c.sampleRateHz = 192'000.0;
        auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QVERIFY(first >= 0);
        QVERIFY(last <= 4095);
        QVERIFY(first <= last);
    }

    // A single strong bin must dominate its pixel under Peak detection.
    void peakDetectorFindsTheTone()
    {
        SpectrumReducer r;
        ReducerConfig c;
        c.pixels = 256;
        c.centreHz = 14'200'000.0; c.spanHz = 192'000.0;
        c.streamCentreHz = 14'200'000.0; c.sampleRateHz = 192'000.0;
        c.detector = SpectrumDetectorMode::Peak;
        r.setConfig(c);
        QVector<float> bins(4096, 1.0e-12f);
        bins[2048] = 1.0f;
        const QVector<float>& out = r.reduce(bins, 1.0, 0.0);
        int argmax = 0;
        for (int i = 1; i < out.size(); ++i) { if (out[i] > out[argmax]) argmax = i; }
        QCOMPARE(argmax, 128);
    }
};

QTEST_MAIN(TstSpectrumReducer)
#include "tst_spectrum_reducer.moc"
```

- [ ] **Step 2: Register and confirm the failure**

```cmake
nereus_add_test(tst_spectrum_reducer)
```

```bash
cmake --build build --target tst_spectrum_reducer 2>&1 | tail -5
```

Expected: FAIL, `core/spectrum/SpectrumReducer.h` not found.

- [ ] **Step 3: Read the code you are extracting**

```bash
sed -n '2660,2780p' src/gui/SpectrumWidget.cpp
grep -n 'visibleBinRange' src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
```

The reducer must reproduce this arithmetic exactly. Copy the bin-range maths and the
detector and avenger call sequence verbatim; the only permitted change is that
`displayWidth` becomes `m_cfg.pixels` and the window comes from config rather than
member state.

- [ ] **Step 4: Implement `SpectrumReducer`**

Write `src/core/spectrum/SpectrumReducer.{h,cpp}` against the interface above,
transcribing Step 3's arithmetic. Header comment follows the pattern used in Task 2, with
`no-port-check: NereusSDR-original` and a note that the reduction maths is lifted from
`SpectrumWidget::updateSpectrumLinear`.

- [ ] **Step 5: Run the tests**

```bash
cmake --build build --target tst_spectrum_reducer -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_spectrum_reducer --output-on-failure
```

Expected: all five PASS. If `peakDetectorFindsTheTone` fails, your bin-to-pixel mapping is
off by a rounding convention; match Step 3's exactly rather than adjusting the test.

- [ ] **Step 6: Make `SpectrumWidget` delegate**

Replace the body of the reduction section in `updateSpectrumLinear` with a
`SpectrumReducer` member. The widget still computes `displayWidth` from its own geometry,
but now it does so to **fill in `ReducerConfig::pixels`**, which keeps local rendering
identical while making the dependency explicit and removable.

- [ ] **Step 7: Prove the display is unchanged**

```bash
cmake --build build --target NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R 'spectrum|display' --output-on-failure
```

Then launch the app against the radio and confirm the panadapter and waterfall look
identical to before. This step is a visual check and cannot be automated.

- [ ] **Step 8: Commit**

```bash
git add src/core/spectrum/SpectrumReducer.h src/core/spectrum/SpectrumReducer.cpp \
        src/gui/SpectrumWidget.cpp CMakeLists.txt \
        tests/tst_spectrum_reducer.cpp tests/CMakeLists.txt
git commit -S -m "refactor(r1): extract crop-and-reduce into core SpectrumReducer

The stage lived inside a QRhiWidget with a private visibleBinRange and a pixel
count derived from widget geometry, which is the inverse of what design
section 9.4 requires.  SpectrumReducer takes an explicit pixel count and an
explicit (centreHz, spanHz) window.  SpectrumWidget still fills pixels from its
own width, so local rendering is unchanged, but the dependency is now explicit
and the daemon can supply its own value."
```

---

### Task 6: Extract the FFT engine pool owner

`MainWindow::createFftEngineForStream` (`MainWindow.cpp:1430`) creates per-stream engines, reads four global AppSettings keys, and parks every engine on one shared `m_fftThread` (`MainWindow.h:597-606`). That is core work sitting in a QWidget.

The Pi 4 measurement in design §4.5a makes the thread question concrete rather than theoretical: five streams at 30 fps need 35% of one thread at FFT 65,536 but **176% at 262,144**, and four threads deliver only 1.35x the aggregate throughput of one because the workload is memory-bandwidth bound. The pool must therefore expose its thread policy rather than hard-coding one shared thread.

**Files:**
- Create: `src/core/spectrum/FftEnginePool.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp:1430`, `src/gui/MainWindow.h:597-606`
- Test: `tests/tst_fft_engine_pool.cpp`

**Interfaces:**
- Consumes: `FFTEngine` (existing, `src/core/FFTEngine.h`).
- Produces:

```cpp
namespace NereusSDR {
struct FftPoolConfig {
    int    fps            {30};
    int    fftSize        {4096};
    int    windowType     {4};
    double hzPerBinTarget {0.0};
    int    threadCount    {1};   // 1 = today's shared thread
};

class FftEnginePool : public QObject {
    Q_OBJECT
public:
    explicit FftEnginePool(QObject* parent = nullptr);
    ~FftEnginePool() override;
    void       setConfig(const FftPoolConfig& cfg);
    FFTEngine* engineForStream(int streamIndex);   // creates on first use
    void       removeStream(int streamIndex);
    QList<int> streams() const;
    int        engineCount() const;
signals:
    void fftFrameReady(int streamIndex, const QVector<float>& binsLinear,
                       double windowEnb, double dbmOffset);
};
}
```

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest>
#include "core/spectrum/FftEnginePool.h"

using namespace NereusSDR;

class TstFftEnginePool : public QObject {
    Q_OBJECT
private slots:
    void createsOneEnginePerStreamAndReuses()
    {
        FftEnginePool pool;
        FFTEngine* a = pool.engineForStream(0);
        FFTEngine* b = pool.engineForStream(1);
        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);
        QVERIFY(a != b);
        QCOMPARE(pool.engineForStream(0), a);   // reuse, not recreate
        QCOMPARE(pool.engineCount(), 2);
    }

    void removeStreamDropsTheEngine()
    {
        FftEnginePool pool;
        pool.engineForStream(0);
        pool.engineForStream(1);
        pool.removeStream(0);
        QCOMPARE(pool.engineCount(), 1);
        QCOMPARE(pool.streams(), QList<int>{1});
    }

    // Config must reach engines created BEFORE and AFTER the call, otherwise
    // stream 0 and stream 4 silently run different FFT sizes.
    void configAppliesToExistingAndFutureEngines()
    {
        FftEnginePool pool;
        FFTEngine* early = pool.engineForStream(0);
        FftPoolConfig cfg;
        cfg.fftSize = 16384;
        cfg.fps     = 15;
        pool.setConfig(cfg);
        FFTEngine* late = pool.engineForStream(1);
        QCOMPARE(early->fftSize(), 16384);
        QCOMPARE(late->fftSize(),  16384);
        QCOMPARE(early->outputFps(), 15);
        QCOMPARE(late->outputFps(),  15);
    }
};

QTEST_MAIN(TstFftEnginePool)
#include "tst_fft_engine_pool.moc"
```

- [ ] **Step 2: Register, run, confirm it fails**

```cmake
nereus_add_test(tst_fft_engine_pool)
```

```bash
cmake --build build --target tst_fft_engine_pool 2>&1 | tail -5
```

Expected: FAIL, header not found.

- [ ] **Step 3: Read what you are extracting**

```bash
sed -n '1430,1500p' src/gui/MainWindow.cpp
sed -n '590,610p' src/gui/MainWindow.h
```

Note the four AppSettings keys it reads and the thread parking. Reproduce both.

- [ ] **Step 4: Implement the pool**

Write `FftEnginePool.{h,cpp}`. Own the `QThread`, move each engine onto it with
`moveToThread`, and forward each engine's `fftReadyLinear` to the pool's
`fftFrameReady` with the stream index attached. Destructor quits and waits on the thread
before deleting engines.

`threadCount` above 1 is accepted and creates that many threads round-robin. **Do not
default it above 1**: design §4.5a measured only 1.35x aggregate scaling on the floor
hardware, so more threads cost memory bandwidth without buying throughput.

- [ ] **Step 5: Run the tests**

```bash
cmake --build build --target tst_fft_engine_pool -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_fft_engine_pool --output-on-failure
```

Expected: all three PASS.

- [ ] **Step 6: Make `MainWindow` use it**

Replace `createFftEngineForStream` and the `m_fftEngines` map and `m_fftThread` member
with an `FftEnginePool` member. `MainWindow` now reads the four AppSettings keys once,
fills `FftPoolConfig`, and calls `setConfig`.

- [ ] **Step 7: Verify the GUI still receives frames**

```bash
cmake --build build --target NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R 'fft|spectrum' --output-on-failure
```

Launch against the radio; confirm the panadapter still updates on every pan.

- [ ] **Step 8: Commit**

```bash
git add src/core/spectrum/FftEnginePool.h src/core/spectrum/FftEnginePool.cpp \
        src/gui/MainWindow.h src/gui/MainWindow.cpp CMakeLists.txt \
        tests/tst_fft_engine_pool.cpp tests/CMakeLists.txt
git commit -S -m "refactor(r1): extract the FFT engine pool out of MainWindow

Per-stream engine creation, the four global display AppSettings keys, and the
shared FFT thread were core concerns living in a QWidget, so a headless daemon
had no way to produce spectrum.  FftEnginePool owns them and exposes
threadCount as policy rather than hard-coding one thread, because the Pi 4
measurement in design section 4.5a shows the single shared thread saturating at
five streams and FFT 262144.  Default stays 1: four threads measured only 1.35x
aggregate throughput, the workload being memory-bandwidth bound."
```

---

### Task 7: Extract the topology builder

`MainWindow::rebuildFftRouting` (`MainWindow.cpp:2092`) iterates `m_panStack->allApplets()`, a QWidget, so without `PanadapterStack` the `FFTRouter` stays empty and the daemon routes nothing.

**Files:**
- Create: `src/core/spectrum/FftTopology.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp:2092`
- Test: `tests/tst_fft_topology.cpp`

**Interfaces:**
- Consumes: `FFTRouter` (existing, `src/core/FFTRouter.h`).
- Produces:

```cpp
namespace NereusSDR {
/// One consumer of a stream's FFT frames. In the GUI this is a pan; in the
/// daemon it is a remote endpoint. Design section 9.4: the daemon keys on
/// endpoint ids, not pan ids.
struct SpectrumSubscription {
    QString consumerId;
    int     streamIndex {0};
};

class FftTopology {
public:
    void subscribe(const QString& consumerId, int streamIndex);
    void unsubscribe(const QString& consumerId);
    void applyTo(FFTRouter& router) const;   // idempotent: full rebuild
    QList<SpectrumSubscription> subscriptions() const;
};
}
```

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest>
#include "core/spectrum/FftTopology.h"
#include "core/FFTRouter.h"

using namespace NereusSDR;

class TstFftTopology : public QObject {
    Q_OBJECT
private slots:
    void manyConsumersOnOneStream()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("b", 0);
        t.subscribe("c", 1);
        FFTRouter r;
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0).size(), 2);
        QCOMPARE(r.pansForReceiver(1).size(), 1);
    }

    void unsubscribeRemovesOnlyThatConsumer()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("b", 0);
        t.unsubscribe("a");
        FFTRouter r;
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0), QList<QString>{"b"});
    }

    // applyTo must be a full rebuild, so a stale mapping cannot survive.
    void applyToIsIdempotentAndAuthoritative()
    {
        FftTopology t;
        t.subscribe("a", 0);
        FFTRouter r;
        t.applyTo(r);
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0), QList<QString>{"a"});
        t.unsubscribe("a");
        t.applyTo(r);
        QVERIFY(r.pansForReceiver(0).isEmpty());
    }

    // Re-subscribing a consumer to a different stream must move it, not clone it.
    void resubscribeMovesTheConsumer()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("a", 1);
        FFTRouter r;
        t.applyTo(r);
        QVERIFY(r.pansForReceiver(0).isEmpty());
        QCOMPARE(r.pansForReceiver(1), QList<QString>{"a"});
    }
};

QTEST_MAIN(TstFftTopology)
#include "tst_fft_topology.moc"
```

- [ ] **Step 2: Register, run, confirm failure**

```cmake
nereus_add_test(tst_fft_topology)
```

Expected: FAIL, header not found.

- [ ] **Step 3: Implement `FftTopology`**

Back it with a `QMap<QString,int>` of consumer to stream. `applyTo` calls
`router.removePan` for consumers no longer present, then `router.mapPanToReceiver` for
each current entry. It must be safe to call repeatedly.

- [ ] **Step 4: Run the tests**

```bash
cmake --build build --target tst_fft_topology -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_fft_topology --output-on-failure
```

Expected: all four PASS.

- [ ] **Step 5: Make `MainWindow::rebuildFftRouting` delegate**

Keep the method. Its body becomes: walk `m_panStack->allApplets()`, call
`m_topology.subscribe(panId, streamIndex)` for each, then `m_topology.applyTo(*router)`.
The widget walk stays GUI-side, which is correct; only the topology algebra moves to core.

- [ ] **Step 6: Verify the GUI routes unchanged**

```bash
cmake --build build --target NereusSDR -j$(sysctl -n hw.ncpu)
```

Launch, switch layouts (Single, Stacked, Side-by-Side, Wide+2, Grid 2x2) and confirm every
pan still draws.

- [ ] **Step 7: Commit**

```bash
git add src/core/spectrum/FftTopology.h src/core/spectrum/FftTopology.cpp \
        src/gui/MainWindow.cpp CMakeLists.txt \
        tests/tst_fft_topology.cpp tests/CMakeLists.txt
git commit -S -m "refactor(r1): extract FFT routing topology into core

rebuildFftRouting derived the router's mappings by walking PanadapterStack, a
QWidget, so a headless daemon left FFTRouter empty and routed nothing.
FftTopology holds consumer-to-stream subscriptions and rebuilds the router
authoritatively.  The GUI still supplies its consumers by walking pans; the
daemon will supply endpoint ids instead, per design section 9.4."
```

---

### Task 8: Shared core initialisation

`AppSettings::ensureSettingsAtVersion` has exactly one call site, `src/main.cpp:280`. A daemon-first install would run against an unmigrated store.

**Files:**
- Create: `src/core/CoreInit.{h,cpp}`
- Modify: `src/main.cpp:280`
- Test: `tests/tst_core_init.cpp`

**Interfaces:**
- Consumes: `AppSettings`.
- Produces: `NereusSDR::CoreInit::initialize(const QString& profile = {})`, idempotent, returns `bool`.

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest>
#include "core/CoreInit.h"
#include "core/AppSettings.h"

class TstCoreInit : public QObject {
    Q_OBJECT
private slots:
    void migratesSettingsToCurrentVersion()
    {
        QVERIFY(NereusSDR::CoreInit::initialize());
        const QString v = NereusSDR::AppSettings::instance()
                              .value("SettingsSchemaVersion", "0").toString();
        QVERIFY(v.toInt() >= 6);
    }

    void isIdempotent()
    {
        QVERIFY(NereusSDR::CoreInit::initialize());
        QVERIFY(NereusSDR::CoreInit::initialize());
    }
};

QTEST_MAIN(TstCoreInit)
#include "tst_core_init.moc"
```

- [ ] **Step 2: Register, run, confirm failure**

```cmake
nereus_add_test(tst_core_init)
```

Expected: FAIL, `core/CoreInit.h` not found.

- [ ] **Step 3: Read what `main.cpp` does before the GUI starts**

```bash
sed -n '255,300p' src/main.cpp
```

Everything here that is not Qt Widgets or window setup belongs in `CoreInit`: the profile
pin, the settings migration, and the logging handler install.

- [ ] **Step 4: Implement `CoreInit`**

Move the settings migration and logging setup into `CoreInit::initialize`. Guard with a
file-static `bool` so a second call is a no-op returning `true`.

- [ ] **Step 5: Call it from `main.cpp`**

Replace the inline migration at line 280 with `NereusSDR::CoreInit::initialize(profile);`.

- [ ] **Step 6: Run the test and the app**

```bash
cmake --build build --target tst_core_init NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_core_init --output-on-failure
```

Expected: both PASS; the app starts and its settings still load.

- [ ] **Step 7: Commit**

```bash
git add src/core/CoreInit.h src/core/CoreInit.cpp src/main.cpp CMakeLists.txt \
        tests/tst_core_init.cpp tests/CMakeLists.txt
git commit -S -m "refactor(r1): shared CoreInit for settings migration and logging

ensureSettingsAtVersion had one call site, in the GUI's main().  A daemon-first
install would have run against an unmigrated store.  Both binaries now call
CoreInit::initialize, which is idempotent."
```

---

### Task 9: `nereusd` skeleton and config file

**Files:**
- Create: `src/server_main.cpp`, `src/core/daemon/DaemonConfig.{h,cpp}`, `packaging/nereusd.conf.sample`
- Modify: `CMakeLists.txt`
- Test: `tests/tst_daemon_config.cpp`

**Interfaces:**
- Consumes: `CoreInit` (Task 8).
- Produces:

```cpp
namespace NereusSDR {
struct DaemonConfig {
    QString radioMac;              // empty = first discovered
    int     sampleRateHz {192000};
    int     sliceCount   {1};      // clamped to BoardCapabilities::maxSlices
    QString audioDevice;           // empty = no local audio
    QString logLevel     {"info"};
    static DaemonConfig fromFile(const QString& path, QString* errorOut);
    static DaemonConfig defaults();
    bool validate(QString* errorOut) const;
};
}
```

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest>
#include <QTemporaryFile>
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstDaemonConfig : public QObject {
    Q_OBJECT
private slots:
    void defaultsAreValid()
    {
        QString err;
        QVERIFY(DaemonConfig::defaults().validate(&err));
        QVERIFY2(err.isEmpty(), qPrintable(err));
    }

    void parsesAWellFormedFile()
    {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("radio_mac = 00:1C:2D:05:37:2A\n"
                "sample_rate_hz = 384000\n"
                "slice_count = 3\n"
                "log_level = debug\n");
        f.flush();
        QString err;
        DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.radioMac, QStringLiteral("00:1C:2D:05:37:2A"));
        QCOMPARE(c.sampleRateHz, 384000);
        QCOMPARE(c.sliceCount, 3);
        QCOMPARE(c.logLevel, QStringLiteral("debug"));
    }

    void rejectsSliceCountBelowOne()
    {
        DaemonConfig c = DaemonConfig::defaults();
        c.sliceCount = 0;
        QString err;
        QVERIFY(!c.validate(&err));
        QVERIFY(!err.isEmpty());
    }

    void missingFileYieldsDefaultsAndAnError()
    {
        QString err;
        DaemonConfig c = DaemonConfig::fromFile("/nonexistent/nereusd.conf", &err);
        QVERIFY(!err.isEmpty());
        QCOMPARE(c.sliceCount, DaemonConfig::defaults().sliceCount);
    }
};

QTEST_MAIN(TstDaemonConfig)
#include "tst_daemon_config.moc"
```

- [ ] **Step 2: Register, run, confirm failure**

```cmake
nereus_add_test(tst_daemon_config)
```

Expected: FAIL, header not found.

- [ ] **Step 3: Implement `DaemonConfig`**

Simple `key = value` lines, `#` comments, whitespace trimmed. Unknown keys are a warning,
not an error, so a newer config file does not break an older daemon.

- [ ] **Step 4: Write `src/server_main.cpp`**

```cpp
// nereusd: headless NereusSDR daemon.
// Design: docs/architecture/2026-07-28-remote-daemon-architecture-design.md
#include <QCoreApplication>
#include <QCommandLineParser>
#include <csignal>
#include "core/CoreInit.h"
#include "core/daemon/DaemonConfig.h"
#include "core/LogCategories.h"

namespace {
QCoreApplication* g_app = nullptr;
void onTerm(int) { if (g_app) { g_app->quit(); } }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nereusd"));
    g_app = &app;
    std::signal(SIGTERM, onTerm);
    std::signal(SIGINT,  onTerm);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NereusSDR headless daemon"));
    parser.addHelpOption();
    QCommandLineOption cfgOpt({"c", "config"},
        QStringLiteral("Config file path."), QStringLiteral("path"),
        QStringLiteral("/etc/nereusd.conf"));
    parser.addOption(cfgOpt);
    parser.process(app);

    if (!NereusSDR::CoreInit::initialize()) {
        qCCritical(lcApp) << "core initialisation failed";
        return 1;
    }

    QString err;
    const NereusSDR::DaemonConfig cfg =
        NereusSDR::DaemonConfig::fromFile(parser.value(cfgOpt), &err);
    if (!err.isEmpty()) {
        qCWarning(lcApp) << "config:" << err << "- continuing with defaults";
    }
    if (!cfg.validate(&err)) {
        qCCritical(lcApp) << "invalid config:" << err;
        return 2;
    }

    qCInfo(lcApp) << "nereusd starting, slices" << cfg.sliceCount
                  << "rate" << cfg.sampleRateHz;
    return app.exec();
}
```

- [ ] **Step 5: Add the target**

```cmake
add_executable(nereusd src/server_main.cpp)
target_link_libraries(nereusd PRIVATE NereusCore)
```

Place it after the `NereusCore` declaration. It links `NereusCore` only. **If this fails to
link because of a widget symbol, that is the extraction work in Tasks 2 through 7 being
incomplete; fix the extraction, do not link `NereusGui`.**

- [ ] **Step 6: Build and smoke it**

```bash
cmake --build build --target nereusd tst_daemon_config -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_daemon_config --output-on-failure
./build/nereusd --help
otool -L ./build/nereusd 2>/dev/null | grep -i widgets && echo "FAIL: links Widgets" || echo "OK: no Widgets"
```

Expected: tests pass, `--help` prints, and **no QtWidgets in the link**. On Linux use
`ldd ./build/nereusd | grep -i widgets`.

- [ ] **Step 7: Write the sample config**

`packaging/nereusd.conf.sample`, every key documented with its default.

- [ ] **Step 8: Commit**

```bash
git add src/server_main.cpp src/core/daemon/DaemonConfig.h src/core/daemon/DaemonConfig.cpp \
        packaging/nereusd.conf.sample CMakeLists.txt \
        tests/tst_daemon_config.cpp tests/CMakeLists.txt
git commit -S -m "feat(r1): nereusd skeleton with config file

Headless entry point linking NereusCore only, with SIGTERM and SIGINT
handling, a key=value config file, and validation.  Unknown config keys warn
rather than fail so a newer file does not break an older daemon.  The build
asserts the binary links no Qt Widgets."
```

---

### Task 10: Daemon radio connection and slice orchestration

Every `addSliceOnPan` call site is GUI-resident and `connectToRadio` uses the no-argument `addSlice` overload, so without this the daemon gets Slice A and nothing else.

**Files:**
- Create: `src/core/daemon/DaemonApp.{h,cpp}`
- Modify: `src/server_main.cpp`
- Test: `tests/tst_daemon_app.cpp`

**Interfaces:**
- Consumes: `DaemonConfig` (Task 9), `FftEnginePool` (Task 6), `FftTopology` (Task 7), `RadioModel`.
- Produces: `NereusSDR::DaemonApp` with `bool start(const DaemonConfig&)`, `void stop()`, `int sliceCount() const`, signal `void radioConnected(bool)`.

- [ ] **Step 1: Write the failing test**

Use the existing `P1FakeRadio` test fake rather than real hardware.

```bash
grep -rn "P1FakeRadio" tests/CMakeLists.txt | head -3
```

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstDaemonApp : public QObject {
    Q_OBJECT
private slots:
    // The whole point of the task: a headless start must create the
    // configured number of slices, not just Slice A.
    void createsConfiguredSliceCount()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 3;
        DaemonApp app;
        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 3);
        app.stop();
    }

    void clampsSliceCountToBoardCapability()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 99;
        DaemonApp app;
        QVERIFY(app.start(cfg));
        QVERIFY(app.sliceCount() >= 1);
        QVERIFY(app.sliceCount() <= 5);   // no supported SKU exceeds 5
        app.stop();
    }

    void stopIsSafeWithoutStart()
    {
        DaemonApp app;
        app.stop();          // must not crash
        QCOMPARE(app.sliceCount(), 0);
    }

    void restartIsClean()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 2;
        DaemonApp app;
        QVERIFY(app.start(cfg));
        app.stop();
        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 2);   // not 4
        app.stop();
    }
};

QTEST_MAIN(TstDaemonApp)
#include "tst_daemon_app.moc"
```

- [ ] **Step 2: Register, run, confirm failure**

```cmake
nereus_add_test(tst_daemon_app)
```

Expected: FAIL, header not found.

- [ ] **Step 3: Implement `DaemonApp`**

`start()` owns a `RadioModel`, calls `CoreInit`, applies config, connects to the radio (or
discovers if `radioMac` is empty), then creates slices up to
`min(cfg.sliceCount, caps.maxSlices)` using `RadioModel`'s slice-creation API. Each slice
gets an `FftTopology` subscription keyed by an **endpoint id the daemon mints**, per design
§9.4, not a pan id.

`stop()` tears down in reverse and leaves `sliceCount() == 0`.

- [ ] **Step 4: Run the tests**

```bash
cmake --build build --target tst_daemon_app -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R tst_daemon_app --output-on-failure
```

Expected: all four PASS. `restartIsClean` failing with 4 instead of 2 means `stop()` is
not clearing the slice list.

- [ ] **Step 5: Wire it into `server_main.cpp`**

Replace the `qCInfo` placeholder with a `DaemonApp` instance, `start(cfg)`, and a
`stop()` on the quit path.

- [ ] **Step 6: Commit**

```bash
git add src/core/daemon/DaemonApp.h src/core/daemon/DaemonApp.cpp src/server_main.cpp \
        CMakeLists.txt tests/tst_daemon_app.cpp tests/CMakeLists.txt
git commit -S -m "feat(r1): daemon radio connection and slice orchestration

Slice creation ran only through GUI call sites, so a headless daemon got
Slice A and nothing else.  DaemonApp creates min(config, maxSlices) slices and
subscribes each to the FFT topology under a daemon-minted endpoint id rather
than a pan id, per design section 9.4."
```

---

### Task 11: Dedicated thread for the wideband FFT

`RadioModel` hops the wideband FFT onto the main thread to stay off the network hot path. `nereusd` has no main thread in that sense.

**Files:**
- Modify: `src/models/RadioModel.cpp` (the wideband dispatch), `src/core/daemon/DaemonApp.cpp`
- Test: `tests/tst_wideband_thread.cpp`

**Interfaces:**
- Consumes: `DaemonApp` (Task 10).
- Produces: `DaemonApp::widebandThread()` returning `QThread*`.

- [ ] **Step 1: Find the hop**

```bash
grep -n "wideband" src/models/RadioModel.cpp | grep -i "thread\|invokeMethod\|Qt::QueuedConnection" | head
```

- [ ] **Step 2: Write the failing test**

```cpp
#include <QtTest>
#include <QThread>
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstWidebandThread : public QObject {
    Q_OBJECT
private slots:
    void daemonProvidesADedicatedWidebandThread()
    {
        DaemonApp app;
        QVERIFY(app.start(DaemonConfig::defaults()));
        QThread* wb = app.widebandThread();
        QVERIFY(wb != nullptr);
        QVERIFY(wb->isRunning());
        QVERIFY(wb != QThread::currentThread());   // never the caller's thread
        app.stop();
        QVERIFY(!wb->isRunning());                 // joined on stop
    }
};

QTEST_MAIN(TstWidebandThread)
#include "tst_wideband_thread.moc"
```

- [ ] **Step 3: Register, run, confirm failure**

```cmake
nereus_add_test(tst_wideband_thread)
```

Expected: FAIL, `widebandThread()` not declared.

- [ ] **Step 4: Implement**

`DaemonApp` owns a `QThread` started in `start()` and quit-and-waited in `stop()`. Change
the wideband dispatch in `RadioModel` to target an injectable thread, defaulting to the
current behaviour so the GUI is unaffected.

- [ ] **Step 5: Verify both paths**

```bash
cmake --build build --target tst_wideband_thread NereusSDR -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R 'tst_wideband_thread|wideband' --output-on-failure
```

Expected: PASS, and the GUI's wideband pan still works when zoomed past the DDC span.

- [ ] **Step 6: Commit**

```bash
git add src/models/RadioModel.cpp src/core/daemon/DaemonApp.h src/core/daemon/DaemonApp.cpp \
        tests/tst_wideband_thread.cpp tests/CMakeLists.txt
git commit -S -m "feat(r1): dedicated wideband FFT thread for the daemon

RadioModel hopped the wideband FFT to the main thread to stay off the network
hot path; nereusd has no main thread in that sense.  The target thread is now
injectable and DaemonApp supplies its own, joined on stop.  GUI behaviour is
unchanged by default."
```

---

### Task 12: systemd unit and packaging

**Files:**
- Create: `packaging/nereusd.service`
- Modify: `CMakeLists.txt` (install rules)

- [ ] **Step 1: Write the unit**

`packaging/nereusd.service`:

```ini
[Unit]
Description=NereusSDR headless SDR daemon
Documentation=https://github.com/boydsoftprez/NereusSDR
After=network-online.target sound.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/nereusd --config /etc/nereusd.conf
Restart=on-failure
RestartSec=5
User=nereusd
Group=nereusd
# Radio I/O is UDP only; no elevated capability is required.
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/nereusd
# Real-time audio scheduling, per design section 4.3.
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
```

- [ ] **Step 2: Add install rules**

```cmake
install(TARGETS nereusd RUNTIME DESTINATION bin)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES packaging/nereusd.service
            DESTINATION lib/systemd/system)
    install(FILES packaging/nereusd.conf.sample
            DESTINATION share/nereusd)
endif()
```

- [ ] **Step 3: Verify the unit parses**

On the Pi:

```bash
systemd-analyze verify packaging/nereusd.service && echo "UNIT OK"
```

Expected: `UNIT OK` with no warnings.

- [ ] **Step 4: Commit**

```bash
git add packaging/nereusd.service CMakeLists.txt
git commit -S -m "build(r1): systemd unit and install rules for nereusd

Hardened service (NoNewPrivileges, ProtectSystem=strict, ProtectHome) with
LimitRTPRIO and LimitMEMLOCK raised for real-time audio scheduling per design
section 4.3.  Radio I/O is UDP only so no elevated capability is needed."
```

---

### Task 13: Pi 4 bench verification

R1 is done when a headless daemon on the floor hardware receives from an ANAN with N slices.

**Files:**
- Create: `docs/architecture/2026-08-02-remote-daemon-r1-verification/README.md`

- [ ] **Step 1: Build on the Pi**

```bash
ssh jj@192.168.109.133
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
     qt6-base-dev qt6-base-private-dev qt6-multimedia-dev qt6-svg-dev \
     qt6-websockets-dev libfftw3-dev libgl1-mesa-dev libasound2-dev \
     libjack-jackd2-dev libpipewire-0.3-dev
git clone <repo> ~/NereusSDR && cd ~/NereusSDR
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target nereusd -j4
```

Record the wall-clock build time in the verification README. Expect it to be long; the
vendored RADE and Opus ExternalProject dominates.

- [ ] **Step 2: Confirm the daemon links no widget toolkit**

```bash
ldd build/nereusd | grep -i -E 'widgets|svg' && echo "FAIL" || echo "OK: no Widgets, no Svg"
```

Expected: `OK`.

- [ ] **Step 3: Run against the radio**

```bash
cp packaging/nereusd.conf.sample /tmp/nereusd.conf
# set radio_mac, sample_rate_hz = 192000, slice_count = 3
./build/nereusd --config /tmp/nereusd.conf
```

Expected: connects, logs three slices, stays up.

- [ ] **Step 4: Measure while it runs**

```bash
pidstat -p $(pgrep nereusd) -t 1 10       # per-thread CPU
vcgencmd measure_temp
vcgencmd get_throttled                    # must stay 0x0
```

Record per-thread CPU against the §4.5a predictions: FFT 4096 at 30 fps should cost
roughly 0.2% of one core per stream, so three streams should be well under 1% on the FFT
thread. A large discrepancy means the pool is not applying config.

- [ ] **Step 5: Fill in the verification matrix**

Create the README with one row per check: build succeeds, no Widgets linkage, connects,
N slices created, RX audio present if a device is configured, per-thread CPU inside
prediction, no thermal throttling, systemd unit starts and restarts, SIGTERM shuts down
cleanly. Mark each PASS, FAIL, or DEFERRED with evidence.

- [ ] **Step 6: Run the full test suite once**

This is the one place the whole suite runs. Roughly 37 minutes.

```bash
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

Expected: all green. Any pre-existing failure must be diagnosed, not carried.

- [ ] **Step 7: Commit**

```bash
git add docs/architecture/2026-08-02-remote-daemon-r1-verification/README.md
git commit -S -m "docs(r1): bench verification matrix for the daemon skeleton

Headless nereusd on the floor hardware (Pi 4B Rev 1.5) receiving from an
ANAN with N slices, with per-thread CPU recorded against the design section
4.5a predictions."
```

---

## Self-Review

**Spec coverage.** All nine §4.2 prerequisites map to tasks: 1 to Task 4, 2 to Task 2, 3 to Task 3, 4 to Task 6, 5 to Task 7, 6 to Task 5, 7 to Task 8, 8 to Task 1, 9 to Task 1 Step 7 and Task 9 Step 6. §15's other R1 items: config file and systemd in Tasks 9 and 12, wideband thread in Task 11, slice and endpoint orchestration in Task 10, Pi verification in Task 13.

**Deliberately out of scope**, all R2 or later: `StateMirror`, `SettingsProxy`, the transport, TLS, the display codec, Opus encoding, and the two-shared-FFT tier. `FftEnginePool::threadCount` exists so the tier can be added without reopening the pool, but R1 does not implement it.

**Known gaps carried forward.** Task 5 Step 7 and Task 7 Step 6 are visual checks that cannot be automated. Task 13 Step 1's build time is unknown and could be hours on a Pi 4. WDSP per-slice CPU is still unmeasured (design §4.5a), and Task 13 Step 4 is the first opportunity to observe it in situ.

**Ordering constraint.** Tasks 2, 3, and 4 must precede Task 9, because `nereusd` cannot link until `NereusCore` is widget-free. Task 1 must be first. Tasks 5, 6, and 7 are independent of one another and may run in any order after Task 4.
