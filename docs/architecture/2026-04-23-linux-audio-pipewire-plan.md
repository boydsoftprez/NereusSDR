# Linux Audio — PipeWire-native Bridge — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `pactl`-primary Linux audio bridge with a libpipewire-0.3 native path plus per-slice output routing, split sidetone/MON sinks, and live telemetry, while keeping a `pactl`/`QAudioSink` fallback for pure-Pulse hosts.

**Architecture:** Add a runtime detector (`LinuxAudioBackend`) cached in `AudioEngine`. Introduce one unified `PipeWireStream` class that handles all three PipeWire stream shapes (virtual source, sink producer, source consumer) via a `StreamConfig` struct. Wrap it in a role-driven `PipeWireBus : IAudioBus`. Leave `LinuxPipeBus` untouched as the Pactl-path fallback; add a minimal `QAudioSinkAdapter : IAudioBus` for the None-path speakers. Thread-loop lifecycle is `pw_thread_loop` owned by `AudioEngine`, single instance shared by all streams. Per-slice routing is a new `QString sinkNodeName` property on `SliceModel`. All PipeWire code is behind `#ifdef NEREUS_HAVE_PIPEWIRE`.

**Tech Stack:** C++20, Qt6 (Core, Widgets, Multimedia, Test), libpipewire-0.3 ≥0.3.50 (pkg-config), CMake, Ninja, GPG-signed commits per project convention.

**Spec:** `docs/architecture/2026-04-23-linux-audio-pipewire-design.md` — read it before starting. Every task references spec sections by number.

**Branch:** `ubuntu-dev`

---

## File Structure

### New source files

| Path | Responsibility |
|---|---|
| `src/core/audio/LinuxAudioBackend.h` | `enum class LinuxAudioBackend { PipeWire, Pactl, None }` plus the `detectLinuxBackend()` free function declaration and an injection seam for unit tests. |
| `src/core/audio/LinuxAudioBackend.cpp` | Real detection implementation (probe `$XDG_RUNTIME_DIR/pipewire-0` then probe `pactl` binary). |
| `src/core/audio/AudioRingSpsc.h` | Single-producer single-consumer lock-free byte ring (template on capacity). Header-only. |
| `src/core/audio/PipeWireThreadLoop.h` | RAII wrapper for `pw_thread_loop` + `pw_context` + `pw_core`. |
| `src/core/audio/PipeWireThreadLoop.cpp` | Implementation. |
| `src/core/audio/PipeWireStream.h` | Unified stream class: `StreamConfig` struct, lifecycle, push/pull, telemetry. |
| `src/core/audio/PipeWireStream.cpp` | Implementation (OUTPUT and INPUT directions). |
| `src/core/audio/PipeWireBus.h` | `IAudioBus` wrapper mapping `Role` → `StreamConfig`. |
| `src/core/audio/PipeWireBus.cpp` | Implementation. |
| `src/core/audio/QAudioSinkAdapter.h` | Thin `IAudioBus` wrapper over `QAudioSink` for the None-path speakers. |
| `src/core/audio/QAudioSinkAdapter.cpp` | Implementation. |
| `src/gui/VaxLinuxFirstRunDialog.h` | Modal dialog: detected state + per-distro remediation + Rescan. |
| `src/gui/VaxLinuxFirstRunDialog.cpp` | Implementation. |
| `src/gui/setup/AudioBackendStrip.h` | Diagnostic strip widget shown at the top of each Setup → Audio sub-page. |
| `src/gui/setup/AudioBackendStrip.cpp` | Implementation. |
| `src/gui/setup/AudioOutputPage.h` | New Setup sub-page: Primary / Sidetone / MON / Per-slice routing with telemetry blocks. |
| `src/gui/setup/AudioOutputPage.cpp` | Implementation. |

### New test files

| Path | Coverage |
|---|---|
| `tests/tst_audio_ring_spsc.cpp` | Push/pop correctness, overflow-drops-oldest, cross-thread race, no leaks. |
| `tests/tst_linux_backend_detection.cpp` | Injection-seam unit test: all 8 combinations of (socket present, pactl present) × (forced override set). |
| `tests/tst_pipewire_stream_config.cpp` | `StreamConfig` → `pw_properties` round-trip; format → `spa_audio_info_raw` translation. |
| `tests/tst_slice_sink_routing.cpp` | `SliceModel::sinkNodeName` property set/get/signal + AppSettings round-trip. |
| `tests/tst_audio_engine_route_dispatch.cpp` | `rxBlockReady` picks the right `IAudioBus` instance based on slice's `sinkNodeName`. |
| `tests/tst_pipewire_stream_integration.cpp` | Env-gated: real `pw_stream_connect` reaches STREAMING, push 1 s of audio, clean shutdown. |
| `tests/tst_linux_audio_detection_integration.cpp` | Env-gated: detection against the real daemon returns PipeWire; detection against `chmod 000` socket returns None. |

### New docs

| Path | Content |
|---|---|
| `docs/architecture/linux-audio-verification/README.md` | Manual verification matrix (7 distro rows × scenarios), per-row result table. |

### Modified files

| Path | What changes |
|---|---|
| `CMakeLists.txt` (root) | `pkg_check_modules(PIPEWIRE libpipewire-0.3>=0.3.50)`, conditional `NEREUS_HAVE_PIPEWIRE` define, link new source files. |
| `tests/CMakeLists.txt` | Register the seven new test files. |
| `src/core/AudioEngine.h` | Add `LinuxAudioBackend` member, `PipeWireThreadLoop` instance (Linux+PipeWire only), new accessors, `audioBackendChanged()` signal. |
| `src/core/AudioEngine.cpp` | Call `detectLinuxBackend()` in ctor, start/stop thread loop, rewrite `makeVaxBus()` to dispatch on backend, add `makeTxInputBus`, `makePrimaryOut`, `makeSidetoneOut`, `makeMonitorOut`, per-slice route cache in `rxBlockReady`. |
| `src/models/SliceModel.h` | Add `Q_PROPERTY(QString sinkNodeName ...)`, getter/setter/signal. |
| `src/models/SliceModel.cpp` | Setter + AppSettings persist/restore. |
| `src/gui/setup/AudioVaxPage.h` | Rebuild card layout: exposed-as label, format, consumers, level meter, Rename button. |
| `src/gui/setup/AudioVaxPage.cpp` | Wire telemetry signals from `PipeWireBus::stream()`. |
| `src/gui/setup/SetupDialog.cpp` | Register the new `AudioOutputPage` under Audio, add the `AudioBackendStrip` header to each Audio sub-page. |
| `src/gui/MainWindow.h` | Add `Help → Diagnose audio backend` action. |
| `src/gui/MainWindow.cpp` | Action handler: show `VaxLinuxFirstRunDialog` in diagnostic mode; first-run trigger in ctor. |
| `CLAUDE.md` | Add `libpipewire-0.3-dev` to the Build dependency list. |
| `CHANGELOG.md` | New entry under the upcoming version. |

---

## Task order and dependencies

```
1  Build wiring ──────────────┐
2  Backend enum header         │
3  Detection (unit, mocked) ───┤
4  Detection (real probes) ────┤
5  AudioEngine wires detection─┘
                               │
6  AudioRingSpsc ──────────────┤
7  PipeWireThreadLoop ─────────┤
8  PipeWireStream scaffolding ─┤
9  Stream OUTPUT direction ────┤
10 Stream telemetry ───────────┤
11 Stream INPUT direction ─────┘
                               │
12 PipeWireBus wrapper ────────┤
13 QAudioSinkAdapter ──────────┤
14 AudioEngine::makeVaxBus ────┘
                               │
15 SliceModel sinkNodeName ────┤
16 AudioEngine route dispatch ─┘
                               │
17 AudioBackendStrip widget ───┤
18 VaxLinuxFirstRunDialog ─────┤
19 Help → Diagnose menu ───────┤
20 AudioOutputPage ────────────┤
21 AudioVaxPage rebuild ───────┤
22 SetupDialog wiring ─────────┤
23 First-run trigger ──────────┘
                               │
24 Integration test (env-gated)┤
25 Verification matrix doc ────┤
26 CLAUDE.md + CHANGELOG ──────┤
27 Shakedown on affected host ─┘
```

Tasks 1–5 land build + detection. Tasks 6–11 land the stream core. Tasks 12–14 make the first end-to-end audio path real. Tasks 15–16 add routing. Tasks 17–23 are UI. Tasks 24–27 verify and document.

---

## Task 1: CMake — libpipewire-0.3 detection

**Files:**
- Modify: `CMakeLists.txt` (around line 154 where `FFTW3` is already detected via pkg-config)

**Purpose:** Add optional libpipewire-0.3 build dependency with a `NEREUS_HAVE_PIPEWIRE` compile flag. No code changes yet — just the flag so later tasks can compile conditionally.

- [ ] **Step 1: Read the current pkg-config block**

Run: `grep -n "pkg_check_modules\|PkgConfig" CMakeLists.txt`
Identify the block that finds `FFTW3` (around line 154) — that's the pattern to follow.

- [ ] **Step 2: Add the PipeWire detection block**

Insert after the FFTW3 block (on Linux-only branch):

```cmake
if(UNIX AND NOT APPLE)
    pkg_check_modules(PIPEWIRE libpipewire-0.3>=0.3.50)
    if(PIPEWIRE_FOUND)
        message(STATUS "PipeWire ${PIPEWIRE_VERSION} — native Linux audio bridge enabled")
    else()
        message(STATUS "libpipewire-0.3 >= 0.3.50 not found — Linux audio limited to pactl path")
    endif()
endif()
```

- [ ] **Step 3: Propagate the flag where the audio sources are wired**

Find the block that links the audio sources on Linux (search `LinuxPipeBus.cpp` in `CMakeLists.txt`; already referenced on existing Linux branch). Add immediately after:

```cmake
if(PIPEWIRE_FOUND)
    target_link_libraries(NereusSDR PRIVATE ${PIPEWIRE_LIBRARIES})
    target_include_directories(NereusSDR PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
    target_compile_options(NereusSDR PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
    target_compile_definitions(NereusSDR PRIVATE NEREUS_HAVE_PIPEWIRE)
endif()
```

- [ ] **Step 4: Install the dev package if missing**

Run: `dpkg -l libpipewire-0.3-dev 2>/dev/null | grep ^ii || sudo apt install -y libpipewire-0.3-dev pkg-config`
Expected on the shakedown machine: "Setting up libpipewire-0.3-dev ...".

- [ ] **Step 5: Reconfigure + build**

Run: `cd ~/nereussdr && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON 2>&1 | grep -i pipewire`
Expected: `-- PipeWire 1.4.7 — native Linux audio bridge enabled` (or the installed version).
Run: `cmake --build build -j$(nproc) 2>&1 | tail -5` — should succeed unchanged (no code references the flag yet).

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
build(linux): detect libpipewire-0.3 via pkg-config

Adds NEREUS_HAVE_PIPEWIRE compile flag gated on
libpipewire-0.3 >= 0.3.50. No code changes yet — this wires the
build system so subsequent tasks can land PipeWire sources behind
the flag. Builds without libpipewire-0.3-dev still succeed; the
Linux audio path simply stays pactl-only.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `LinuxAudioBackend.h` — enum + detection seam

**Files:**
- Create: `src/core/audio/LinuxAudioBackend.h`

**Purpose:** Declare the enum and the detection function. Expose an injection seam (functor arguments) so unit tests can mock without a real PipeWire daemon or a real filesystem.

- [ ] **Step 1: Create the header**

```cpp
// =================================================================
// src/core/audio/LinuxAudioBackend.h  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Created for the Linux PipeWire-native bridge
//                (design: docs/architecture/2026-04-23-linux-
//                audio-pipewire-design.md §4). J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================
#pragma once

#include <QString>
#include <functional>

namespace NereusSDR {

enum class LinuxAudioBackend {
    PipeWire,   // libpipewire-0.3 native path
    Pactl,      // pactl shell-out to PulseAudio or pipewire-pulse
    None        // no working backend detected
};

// Probe callbacks — exposed for unit-test injection.
// Return true iff the respective backend is available.
struct LinuxAudioBackendProbes {
    std::function<bool(int timeoutMs)> pipewireSocketReachable;
    std::function<bool(int timeoutMs)> pactlBinaryRunnable;
    std::function<QString()>           forcedBackendOverride;
};

LinuxAudioBackendProbes defaultProbes();

// Pure function: given probes + overrides, return the detected backend.
LinuxAudioBackend detectLinuxBackend(const LinuxAudioBackendProbes& probes);

// Convenience: call with defaultProbes().
LinuxAudioBackend detectLinuxBackend();

QString toString(LinuxAudioBackend b);

}  // namespace NereusSDR
```

- [ ] **Step 2: Verify it compiles (no .cpp yet, header-only reference is fine)**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: build succeeds unchanged.

- [ ] **Step 3: Commit**

```bash
git add src/core/audio/LinuxAudioBackend.h
git commit -S -m "$(cat <<'EOF'
feat(audio): add LinuxAudioBackend enum + detection seam

Declares the three-way Linux audio backend enum (PipeWire / Pactl /
None) and a probe-callback struct so detectLinuxBackend() can be
unit-tested without a real daemon or real filesystem state.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Detection unit test (mocked probes)

**Files:**
- Create: `tests/tst_linux_backend_detection.cpp`
- Modify: `tests/CMakeLists.txt` — register the new test.

**Purpose:** TDD for `detectLinuxBackend(probes)`. Verify all 8 combinations plus the forced-override cases using the injection seam. No real daemon needed.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_linux_backend_detection.cpp`:

```cpp
// =================================================================
// tests/tst_linux_backend_detection.cpp  (NereusSDR)
// =================================================================
// Author: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-04-23
// =================================================================
#include <QtTest/QtTest>
#include "core/audio/LinuxAudioBackend.h"

using namespace NereusSDR;

class TestLinuxBackendDetection : public QObject {
    Q_OBJECT

private:
    LinuxAudioBackendProbes makeProbes(bool pw, bool pactl,
                                       QString forced = QString())
    {
        LinuxAudioBackendProbes p;
        p.pipewireSocketReachable = [pw](int) { return pw; };
        p.pactlBinaryRunnable     = [pactl](int) { return pactl; };
        p.forcedBackendOverride   = [forced]() { return forced; };
        return p;
    }

private slots:
    void bothAvailable_prefersPipewire() {
        QCOMPARE(detectLinuxBackend(makeProbes(true, true)),
                 LinuxAudioBackend::PipeWire);
    }

    void pipewireOnly_returnsPipewire() {
        QCOMPARE(detectLinuxBackend(makeProbes(true, false)),
                 LinuxAudioBackend::PipeWire);
    }

    void pactlOnly_returnsPactl() {
        QCOMPARE(detectLinuxBackend(makeProbes(false, true)),
                 LinuxAudioBackend::Pactl);
    }

    void neither_returnsNone() {
        QCOMPARE(detectLinuxBackend(makeProbes(false, false)),
                 LinuxAudioBackend::None);
    }

    void forcedPipewire_honoured() {
        QCOMPARE(detectLinuxBackend(makeProbes(false, true, "pipewire")),
                 LinuxAudioBackend::PipeWire);
    }

    void forcedPactl_honoured() {
        QCOMPARE(detectLinuxBackend(makeProbes(true, true, "pactl")),
                 LinuxAudioBackend::Pactl);
    }

    void forcedNone_honoured() {
        QCOMPARE(detectLinuxBackend(makeProbes(true, true, "none")),
                 LinuxAudioBackend::None);
    }

    void forcedUnknown_fallsThrough() {
        QCOMPARE(detectLinuxBackend(makeProbes(true, false, "garbage")),
                 LinuxAudioBackend::PipeWire);
    }

    void toStringMapping() {
        QCOMPARE(toString(LinuxAudioBackend::PipeWire), QStringLiteral("PipeWire"));
        QCOMPARE(toString(LinuxAudioBackend::Pactl),    QStringLiteral("Pactl"));
        QCOMPARE(toString(LinuxAudioBackend::None),     QStringLiteral("None"));
    }
};

QTEST_MAIN(TestLinuxBackendDetection)
#include "tst_linux_backend_detection.moc"
```

- [ ] **Step 2: Register the test**

Find `tests/CMakeLists.txt` and follow the pattern used by other `tst_*` entries. Add a `nereus_add_test(tst_linux_backend_detection)` or equivalent macro call (match the existing macro name used for other tests).

- [ ] **Step 3: Run the failing test**

Run: `cd ~/nereussdr && cmake -B build -G Ninja -DNEREUS_BUILD_TESTS=ON && cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: LINK ERROR — `undefined reference to NereusSDR::detectLinuxBackend` and `toString`. Good — that's the failing-test confirmation.

- [ ] **Step 4: Implement the pure detection logic (`.cpp` stub)**

Create `src/core/audio/LinuxAudioBackend.cpp`:

```cpp
// =================================================================
// src/core/audio/LinuxAudioBackend.cpp  (NereusSDR)
// =================================================================
//  Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//  2026-04-23 — created. AI-assisted via Anthropic Claude Code.
// =================================================================
#include "core/audio/LinuxAudioBackend.h"

namespace NereusSDR {

LinuxAudioBackend detectLinuxBackend(const LinuxAudioBackendProbes& probes)
{
    // Forced override (AppSettings debug key).
    const QString forced = probes.forcedBackendOverride
                             ? probes.forcedBackendOverride() : QString();
    if (forced == QLatin1String("pipewire")) { return LinuxAudioBackend::PipeWire; }
    if (forced == QLatin1String("pactl"))    { return LinuxAudioBackend::Pactl; }
    if (forced == QLatin1String("none"))     { return LinuxAudioBackend::None; }
    // Any other value (empty or garbage) falls through to probes.

    if (probes.pipewireSocketReachable && probes.pipewireSocketReachable(500)) {
        return LinuxAudioBackend::PipeWire;
    }
    if (probes.pactlBinaryRunnable && probes.pactlBinaryRunnable(500)) {
        return LinuxAudioBackend::Pactl;
    }
    return LinuxAudioBackend::None;
}

LinuxAudioBackend detectLinuxBackend()
{
    return detectLinuxBackend(defaultProbes());
}

QString toString(LinuxAudioBackend b)
{
    switch (b) {
        case LinuxAudioBackend::PipeWire: return QStringLiteral("PipeWire");
        case LinuxAudioBackend::Pactl:    return QStringLiteral("Pactl");
        case LinuxAudioBackend::None:     return QStringLiteral("None");
    }
    return QStringLiteral("None");
}

// defaultProbes() lives in a separate TU (Task 4) so this file stays
// free of QFile / QProcess / pipewire includes.

}  // namespace NereusSDR
```

- [ ] **Step 5: Link the .cpp into the non-GUI library**

Find the CMake list that includes `LinuxPipeBus.cpp`. Add `src/core/audio/LinuxAudioBackend.cpp` to the same source list (same conditional guard — Linux-only). Do NOT gate on PIPEWIRE_FOUND — this file has no PipeWire symbols.

- [ ] **Step 6: Run the test — expect FAIL on `defaultProbes()` unresolved**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: LINK ERROR — `undefined reference to NereusSDR::defaultProbes`. Good.

Note: step 7 defers the real probe implementation to Task 4. The unit test does not need `defaultProbes()` because it uses injected probes. To keep this task self-contained, provide a stub:

- [ ] **Step 7: Add a stub `defaultProbes()` that returns empty callbacks**

Append to `LinuxAudioBackend.cpp`:

```cpp
LinuxAudioBackendProbes defaultProbes()
{
    // Real implementation lives in LinuxAudioBackendProbes_real.cpp (Task 4).
    // Returning an all-false probe set is safe here — it yields None.
    LinuxAudioBackendProbes p;
    p.pipewireSocketReachable = [](int) { return false; };
    p.pactlBinaryRunnable     = [](int) { return false; };
    p.forcedBackendOverride   = []() { return QString(); };
    return p;
}
```

(Task 4 will replace this weak stub with the real probes in a second TU using symbol ordering — or we simply overwrite this body.)

- [ ] **Step 8: Run the tests — expect PASS**

Run:
```
cd ~/nereussdr/build && ctest -R tst_linux_backend_detection --output-on-failure
```
Expected: `9 passed, 0 failed`.

- [ ] **Step 9: Commit**

```bash
git add src/core/audio/LinuxAudioBackend.cpp tests/tst_linux_backend_detection.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): implement LinuxAudioBackend detection logic + unit tests

Pure detection: forced-override → pipewire-probe → pactl-probe →
None. Injection seam lets the 9-case unit test run without any real
daemon or filesystem state. Real probes land in Task 4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Real `defaultProbes()` — socket reachability + pactl presence

**Files:**
- Modify: `src/core/audio/LinuxAudioBackend.cpp` — replace the weak stub `defaultProbes()`.

**Purpose:** The real implementation of the default probes. Uses `QFile::exists` for the socket check and `QStandardPaths::findExecutable` + `QProcess` for the pactl check. `pipewireSocketReachable` does NOT actually connect via libpipewire at this layer — that check lives in the PipeWireThreadLoop ctor (Task 7) so failure surfaces once and doesn't duplicate probe paths.

- [ ] **Step 1: Replace the stub body**

Edit `src/core/audio/LinuxAudioBackend.cpp` — replace the `defaultProbes()` body:

```cpp
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QtCore/qglobal.h>
#include "core/AppSettings.h"

namespace {

bool pipewireSocketReachableImpl(int /*timeoutMs*/)
{
    const QByteArray xdg = qgetenv("XDG_RUNTIME_DIR");
    if (xdg.isEmpty()) { return false; }
    return QFile::exists(QString::fromUtf8(xdg) + QStringLiteral("/pipewire-0"));
}

bool pactlBinaryRunnableImpl(int timeoutMs)
{
    const QString path = QStandardPaths::findExecutable(QStringLiteral("pactl"));
    if (path.isEmpty()) { return false; }
    QProcess p;
    p.start(path, {QStringLiteral("--version")});
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        return false;
    }
    return p.exitCode() == 0;
}

QString forcedOverrideImpl()
{
    return NereusSDR::AppSettings::instance()
        .value(QStringLiteral("Audio/LinuxBackendPreferred"), QStringLiteral(""))
        .toString();
}

}  // anonymous

namespace NereusSDR {

LinuxAudioBackendProbes defaultProbes()
{
    LinuxAudioBackendProbes p;
    p.pipewireSocketReachable = &pipewireSocketReachableImpl;
    p.pactlBinaryRunnable     = &pactlBinaryRunnableImpl;
    p.forcedBackendOverride   = &forcedOverrideImpl;
    return p;
}

}  // namespace NereusSDR
```

- [ ] **Step 2: Verify the unit test still passes (injected probes unaffected)**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) && (cd build && ctest -R tst_linux_backend_detection --output-on-failure)`
Expected: all 9 cases PASS.

- [ ] **Step 3: Smoke-test the real probe on this machine**

Write a throwaway smoke in `tests/smoke_detect_backend.cpp` (NOT added to CMake) or simpler — add a one-shot `qCInfo(lcAudio) << "LinuxAudioBackend detected:" << toString(detectLinuxBackend());` in AudioEngine constructor for Task 5 and observe at first run.

For this step, just visually inspect the code against `LinuxAudioBackendProbes` expectations. No runtime check required — the full smoke happens in Task 5.

- [ ] **Step 4: Commit**

```bash
git add src/core/audio/LinuxAudioBackend.cpp
git commit -S -m "$(cat <<'EOF'
feat(audio): real LinuxAudioBackend default probes

pipewire-0 socket existence via QFile; pactl presence via
QStandardPaths::findExecutable + QProcess --version with 500 ms
deadline; forced-override via AppSettings "Audio/LinuxBackend-
Preferred". No libpipewire link required at this layer — socket
reachability in-depth is validated when PipeWireThreadLoop
connects in Task 7.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Wire detection into `AudioEngine`

**Files:**
- Modify: `src/core/AudioEngine.h` — add `LinuxAudioBackend m_linuxBackend`, accessor, signal.
- Modify: `src/core/AudioEngine.cpp` — call detection in ctor, log result.

**Purpose:** `AudioEngine` gains the cached backend enum. Later tasks read `m_linuxBackend` in `makeVaxBus` to choose implementation. Provides the first end-to-end verification that detection works on a real host.

- [ ] **Step 1: Add the include + member**

In `src/core/AudioEngine.h`, near other `#include` lines:

```cpp
#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif
```

In the private member section:

```cpp
#if defined(Q_OS_LINUX)
    LinuxAudioBackend m_linuxBackend = LinuxAudioBackend::None;
#endif
```

In the public accessor section:

```cpp
#if defined(Q_OS_LINUX)
    LinuxAudioBackend linuxBackend() const { return m_linuxBackend; }
    // Re-runs detection and re-emits. Any open IAudioBus instances are left
    // alone — caller is responsible for tearing them down if the result
    // changes. (MainWindow's "Rescan" button handler does that.)
    void rescanLinuxBackend();
signals:
    void linuxBackendChanged(LinuxAudioBackend oldBackend,
                             LinuxAudioBackend newBackend);
#endif
```

- [ ] **Step 2: Implement in `.cpp`**

In `src/core/AudioEngine.cpp`, near the top of the ctor body:

```cpp
#if defined(Q_OS_LINUX)
    m_linuxBackend = detectLinuxBackend();
    qCInfo(lcAudio) << "Linux audio backend detected:"
                    << toString(m_linuxBackend);
#endif
```

Add the `rescanLinuxBackend()` body:

```cpp
#if defined(Q_OS_LINUX)
void AudioEngine::rescanLinuxBackend()
{
    const auto previous = m_linuxBackend;
    m_linuxBackend = detectLinuxBackend();
    if (m_linuxBackend != previous) {
        qCInfo(lcAudio) << "Linux audio backend changed:"
                        << toString(previous) << "→" << toString(m_linuxBackend);
        emit linuxBackendChanged(previous, m_linuxBackend);
    }
}
#endif
```

- [ ] **Step 3: Build**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: green.

- [ ] **Step 4: Run the app, verify the log line**

Kill any running NereusSDR: `pkill -f NereusSDR; sleep 1`
Run: `cd ~/nereussdr && nohup ./build/NereusSDR > /tmp/nereussdr-task5.log 2>&1 & sleep 3; grep "backend detected" /tmp/nereussdr-task5.log`
Expected: `INF: Linux audio backend detected: PipeWire` on this machine.

- [ ] **Step 5: Kill the app + commit**

```bash
pkill -f NereusSDR
git add src/core/AudioEngine.h src/core/AudioEngine.cpp
git commit -S -m "$(cat <<'EOF'
feat(audio): AudioEngine caches detected Linux backend at startup

Calls detectLinuxBackend() in ctor, stores the result, logs it,
exposes rescanLinuxBackend() for the Setup page Rescan button.
No behaviour change yet — makeVaxBus still takes the existing
pactl path regardless. Task 14 wires dispatch on m_linuxBackend.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: `AudioRingSpsc` — lock-free SPSC byte ring

**Files:**
- Create: `src/core/audio/AudioRingSpsc.h` (header-only)
- Create: `tests/tst_audio_ring_spsc.cpp`
- Modify: `tests/CMakeLists.txt`

**Purpose:** The shared buffer between the DSP/audio thread (producer) and the `pw_thread_loop` thread (consumer) for OUTPUT streams, mirrored for INPUT streams (pw thread producer, DSP consumer). Lock-free with acquire/release semantics. Fixed power-of-two capacity for index masking.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_audio_ring_spsc.cpp`:

```cpp
// =================================================================
// tests/tst_audio_ring_spsc.cpp  (NereusSDR)
// Author: J.J. Boyd (KG4VCF), AI-assisted via Claude Code. 2026-04-23.
// =================================================================
#include <QtTest/QtTest>
#include <thread>
#include <atomic>
#include "core/audio/AudioRingSpsc.h"

using Ring = NereusSDR::AudioRingSpsc<65536>;

class TestAudioRingSpsc : public QObject {
    Q_OBJECT

private slots:
    void pushPopRoundTrip() {
        Ring r;
        const uint8_t in[] = {1, 2, 3, 4, 5, 6, 7, 8};
        QCOMPARE(r.pushCopy(in, sizeof(in)), qint64(sizeof(in)));
        uint8_t out[8] = {};
        QCOMPARE(r.popInto(out, sizeof(out)), qint64(sizeof(out)));
        QCOMPARE(std::memcmp(in, out, sizeof(in)), 0);
        QCOMPARE(r.usedBytes(), size_t(0));
    }

    void emptyPopReturnsZero() {
        Ring r;
        uint8_t buf[16];
        QCOMPARE(r.popInto(buf, sizeof(buf)), qint64(0));
    }

    void partialPopHonoursDestCapacity() {
        Ring r;
        uint8_t in[32];
        for (int i = 0; i < 32; ++i) { in[i] = uint8_t(i); }
        r.pushCopy(in, sizeof(in));
        uint8_t out[8];
        QCOMPARE(r.popInto(out, sizeof(out)), qint64(8));
        QCOMPARE(out[0], uint8_t(0));
        QCOMPARE(out[7], uint8_t(7));
        QCOMPARE(r.usedBytes(), size_t(24));
    }

    void overflowDropsOldest() {
        // Ring capacity = 65536; write 70000, read all — should get
        // the last 65536 bytes (oldest 4464 dropped).
        Ring r;
        std::vector<uint8_t> in(70000);
        for (size_t i = 0; i < in.size(); ++i) { in[i] = uint8_t(i & 0xff); }
        r.pushCopy(in.data(), in.size());
        std::vector<uint8_t> out(70000);
        const auto popped = r.popInto(out.data(), out.size());
        QCOMPARE(qint64(r.capacity() - 1), popped);  // capacity-1 usable
        // First byte read corresponds to in[70000 - (capacity-1)].
        const size_t firstIdx = in.size() - size_t(popped);
        QCOMPARE(out[0], uint8_t(firstIdx & 0xff));
    }

    void wraparound() {
        Ring r;
        // Fill almost all.
        std::vector<uint8_t> a(r.capacity() - 100);
        std::fill(a.begin(), a.end(), uint8_t(0xAA));
        r.pushCopy(a.data(), a.size());
        // Drain.
        std::vector<uint8_t> tmp(a.size());
        r.popInto(tmp.data(), tmp.size());
        // Push + pop 200 bytes that straddle the wrap.
        std::vector<uint8_t> b(200);
        for (size_t i = 0; i < 200; ++i) { b[i] = uint8_t(i); }
        r.pushCopy(b.data(), b.size());
        std::vector<uint8_t> out(200);
        r.popInto(out.data(), out.size());
        for (size_t i = 0; i < 200; ++i) { QCOMPARE(out[i], b[i]); }
    }

    void concurrentProducerConsumer() {
        Ring r;
        constexpr int TOTAL = 1'000'000;
        std::atomic<bool> producerDone{false};
        std::thread producer([&]() {
            uint8_t byte = 0;
            int sent = 0;
            while (sent < TOTAL) {
                uint8_t buf[64];
                for (auto& b : buf) { b = byte++; }
                const auto pushed = r.pushCopy(buf, sizeof(buf));
                sent += int(pushed);
                if (pushed == 0) { std::this_thread::yield(); }
            }
            producerDone.store(true, std::memory_order_release);
        });

        uint8_t expected = 0;
        int received = 0;
        uint8_t buf[64];
        while (received < TOTAL) {
            const auto popped = r.popInto(buf, sizeof(buf));
            for (qint64 i = 0; i < popped; ++i) {
                QCOMPARE(buf[i], expected++);
            }
            received += int(popped);
            if (popped == 0) { std::this_thread::yield(); }
        }

        producer.join();
        QVERIFY(producerDone.load(std::memory_order_acquire));
    }
};

QTEST_MAIN(TestAudioRingSpsc)
#include "tst_audio_ring_spsc.moc"
```

Register in `tests/CMakeLists.txt` using the existing test macro.

- [ ] **Step 2: Run — expect build failure**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: compile error — `AudioRingSpsc.h` not found.

- [ ] **Step 3: Implement `AudioRingSpsc.h`**

```cpp
// =================================================================
// src/core/audio/AudioRingSpsc.h  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <QtGlobal>

namespace NereusSDR {

// Single-producer single-consumer lock-free byte ring.
// Capacity must be a power of two. One byte of the buffer is always
// reserved to distinguish full-vs-empty, so usable capacity is
// Capacity - 1.
//
// Release/acquire ordering: producer writes m_wr with release after
// copying bytes in; consumer reads m_wr with acquire before reading
// those bytes. Mirror for the return trip.
template <size_t Capacity>
class AudioRingSpsc {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(Capacity >= 16, "Capacity too small to be useful");

public:
    static constexpr size_t kCapacity = Capacity;
    static constexpr size_t kMask     = Capacity - 1;

    qint64 pushCopy(const uint8_t* src, qint64 bytes) {
        if (bytes <= 0) { return 0; }
        size_t want = size_t(bytes);

        // Input larger than the usable ring: drop oldest bytes at the
        // source side by advancing the src pointer; ring never needs
        // to shuffle or recurse.
        if (want > kCapacity - 1) {
            src  += (want - (kCapacity - 1));
            want  = kCapacity - 1;
        }

        const size_t wr   = m_wr.load(std::memory_order_relaxed);
        const size_t rd   = m_rd.load(std::memory_order_acquire);
        const size_t used = (wr - rd) & kMask;
        const size_t free = (kCapacity - 1) - used;

        // Input larger than current free: drop oldest from the ring
        // (bump m_rd) to make room for all of `want`.
        if (want > free) {
            dropOldest(want - free);
        }

        const size_t writeIdx = wr & kMask;
        const size_t firstChunk =
            (kCapacity - writeIdx < want) ? kCapacity - writeIdx : want;
        std::memcpy(&m_buf[writeIdx], src, firstChunk);
        if (firstChunk < want) {
            std::memcpy(&m_buf[0], src + firstChunk, want - firstChunk);
        }
        m_wr.store(wr + want, std::memory_order_release);
        return qint64(want);
    }

    qint64 popInto(uint8_t* dst, qint64 maxBytes) {
        if (maxBytes <= 0) { return 0; }
        const size_t rd = m_rd.load(std::memory_order_relaxed);
        const size_t wr = m_wr.load(std::memory_order_acquire);
        const size_t used = (wr - rd) & kMask;
        const size_t toCopy =
            (size_t(maxBytes) > used) ? used : size_t(maxBytes);
        if (toCopy == 0) { return 0; }
        const size_t readIdx = rd & kMask;
        const size_t firstChunk =
            (kCapacity - readIdx < toCopy) ? kCapacity - readIdx : toCopy;
        std::memcpy(dst, &m_buf[readIdx], firstChunk);
        if (firstChunk < toCopy) {
            std::memcpy(dst + firstChunk, &m_buf[0], toCopy - firstChunk);
        }
        m_rd.store(rd + toCopy, std::memory_order_release);
        return qint64(toCopy);
    }

    // Called by producer before pushCopy when caller wants to drop
    // oldest-first on overflow. O(1) index bump, no copy.
    void dropOldest(size_t bytes) {
        const size_t rd = m_rd.load(std::memory_order_relaxed);
        const size_t wr = m_wr.load(std::memory_order_acquire);
        const size_t used = (wr - rd) & kMask;
        const size_t toDrop = (bytes > used) ? used : bytes;
        m_rd.store(rd + toDrop, std::memory_order_release);
    }

    size_t usedBytes() const {
        const size_t wr = m_wr.load(std::memory_order_acquire);
        const size_t rd = m_rd.load(std::memory_order_acquire);
        return (wr - rd) & kMask;
    }

    static constexpr size_t capacity() { return kCapacity; }

private:
    alignas(64) std::atomic<size_t> m_wr{0};
    alignas(64) std::atomic<size_t> m_rd{0};
    std::array<uint8_t, kCapacity> m_buf{};
};

}  // namespace NereusSDR
```

- [ ] **Step 4: Build + run tests**

Run:
```
cd ~/nereussdr && cmake --build build -j$(nproc) && (cd build && ctest -R tst_audio_ring_spsc --output-on-failure)
```
Expected: all cases PASS (6 passed).

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/AudioRingSpsc.h tests/tst_audio_ring_spsc.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): lock-free SPSC byte ring for PipeWire streams

Single-producer single-consumer ring with power-of-two capacity,
acquire/release ordering, and drop-oldest-on-overflow semantics.
Sized for 65536 bytes (≈170 ms of 48 kHz stereo F32 headroom)
when used by PipeWireStream. Six unit tests cover push/pop,
overflow drop, wrap-around, and a 1 M-byte concurrent producer/
consumer race.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: `PipeWireThreadLoop` — RAII wrapper around `pw_thread_loop` + `pw_context` + `pw_core`

**Files:**
- Create: `src/core/audio/PipeWireThreadLoop.h`
- Create: `src/core/audio/PipeWireThreadLoop.cpp`
- Modify: `CMakeLists.txt` — register `.cpp` under the `PIPEWIRE_FOUND` branch only.

**Purpose:** One shared thread loop owned by `AudioEngine`. Starts in ctor on PipeWire hosts; stops in dtor. All `pw_*` calls elsewhere go through its lock.

- [ ] **Step 1: Header**

```cpp
// =================================================================
// src/core/audio/PipeWireThreadLoop.h  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
// =================================================================
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include <QString>
#include <pipewire/pipewire.h>

namespace NereusSDR {

class PipeWireThreadLoop {
public:
    PipeWireThreadLoop();
    ~PipeWireThreadLoop();

    PipeWireThreadLoop(const PipeWireThreadLoop&)            = delete;
    PipeWireThreadLoop& operator=(const PipeWireThreadLoop&) = delete;

    // True when connect() succeeded. Until then the object is
    // safe to destroy but may not be used for stream creation.
    bool connect();

    pw_thread_loop* loop() const { return m_loop; }
    pw_core*        core() const { return m_core; }

    QString serverVersion() const { return m_serverVersion; }
    bool    rtGranted()     const { return m_rtGranted; }

    void lock()   { if (m_loop) { pw_thread_loop_lock(m_loop); } }
    void unlock() { if (m_loop) { pw_thread_loop_unlock(m_loop); } }

private:
    pw_thread_loop* m_loop    = nullptr;
    pw_context*     m_context = nullptr;
    pw_core*        m_core    = nullptr;

    QString m_serverVersion;
    bool    m_rtGranted = false;
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
```

- [ ] **Step 2: Implementation**

```cpp
// =================================================================
// src/core/audio/PipeWireThreadLoop.cpp  (NereusSDR)
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireThreadLoop.h"

#include <QLoggingCategory>
#include <pipewire/pipewire.h>

Q_LOGGING_CATEGORY(lcPw, "nereussdr.pipewire")

namespace NereusSDR {

PipeWireThreadLoop::PipeWireThreadLoop()
{
    pw_init(nullptr, nullptr);
}

PipeWireThreadLoop::~PipeWireThreadLoop()
{
    if (m_loop) {
        pw_thread_loop_stop(m_loop);
    }
    if (m_core)    { pw_core_disconnect(m_core); }
    if (m_context) { pw_context_destroy(m_context); }
    if (m_loop)    { pw_thread_loop_destroy(m_loop); }
    pw_deinit();
}

bool PipeWireThreadLoop::connect()
{
    m_loop = pw_thread_loop_new("nereussdr.pw", nullptr);
    if (!m_loop) {
        qCWarning(lcPw) << "pw_thread_loop_new failed";
        return false;
    }
    if (pw_thread_loop_start(m_loop) < 0) {
        qCWarning(lcPw) << "pw_thread_loop_start failed";
        return false;
    }

    lock();
    m_context = pw_context_new(pw_thread_loop_get_loop(m_loop),
                               nullptr, 0);
    if (!m_context) {
        unlock();
        qCWarning(lcPw) << "pw_context_new failed";
        return false;
    }
    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        unlock();
        qCWarning(lcPw) << "pw_context_connect failed (socket unreachable?)";
        return false;
    }

    // Record server version for the Setup backend strip.
    const pw_core_info* info = nullptr;
    // A proper version probe uses a pw_core_events listener; skip the
    // async dance here and fill the field with the compile-time header
    // version — good enough for the strip.
    m_serverVersion = QString::fromUtf8(pw_get_library_version());

    // RT-probe lives in PipeWireStream::probeSchedOnce (Task 10/11)
    // because it must run on the actual pw data thread, not Qt main —
    // see commit f35cc7b for the discovery and removal of the wrong-
    // thread probe that originally lived here.

    unlock();
    qCInfo(lcPw) << "PipeWire thread loop connected — server version"
                 << m_serverVersion;
    return true;
}

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
```

- [ ] **Step 3: Register in CMake under the PIPEWIRE_FOUND branch**

Extend the PipeWire-gated block to add the `.cpp`:

```cmake
if(PIPEWIRE_FOUND)
    target_sources(NereusSDRObjs PRIVATE
        src/core/audio/PipeWireThreadLoop.cpp
    )
endif()
```

Note: the actual NereusSDR target is `NereusSDRObjs` (an OBJECT
library that backs both the main exe and the test executables).
Earlier drafts of this plan used `NereusSDR` here — that's wrong.
Tasks 8 and 12 also reuse this CMake block, so the same correction
applies there.

- [ ] **Step 4: Build**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: green.

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/PipeWireThreadLoop.h src/core/audio/PipeWireThreadLoop.cpp CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): PipeWireThreadLoop — RAII wrapper over pw_thread_loop

Owns pw_thread_loop + pw_context + pw_core as one unit. Starts the
loop in connect(), records server version and RT-scheduling state,
tears everything down cleanly in the dtor. All PipeWire-C API code
is gated on NEREUS_HAVE_PIPEWIRE so the build stays clean on hosts
without libpipewire-0.3-dev.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: `PipeWireStream` scaffolding + `StreamConfig` + config-translation test

**Files:**
- Create: `src/core/audio/PipeWireStream.h`
- Create: `src/core/audio/PipeWireStream.cpp` (skeleton)
- Create: `tests/tst_pipewire_stream_config.cpp`
- Modify: `CMakeLists.txt` + `tests/CMakeLists.txt`

**Purpose:** Class declaration, `StreamConfig` struct, and a pure-function translator `configToProperties()` tested without any daemon or connect call.

- [ ] **Step 1: Header**

```cpp
// =================================================================
// src/core/audio/PipeWireStream.h  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
// =================================================================
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include "core/audio/AudioRingSpsc.h"

namespace NereusSDR {

class PipeWireThreadLoop;

struct StreamConfig {
    QString nodeName;
    QString nodeDescription;
    enum Direction : int { Output = PW_DIRECTION_OUTPUT,
                           Input  = PW_DIRECTION_INPUT } direction = Output;
    QString mediaClass;      // "Audio/Source", "Stream/Output/Audio", …
    QString mediaRole;       // "Music", "Communication", "Phone", …
    uint32_t rate     = 48000;
    uint32_t channels = 2;
    uint32_t quantum  = 512;
    QString targetNodeName;  // empty = follow default
};

// Pure: translates StreamConfig → pw_properties* owned by caller.
// Unit-testable without a running daemon.
pw_properties* configToProperties(const StreamConfig& cfg);

class PipeWireStream : public QObject {
    Q_OBJECT

public:
    explicit PipeWireStream(PipeWireThreadLoop* loop,
                            StreamConfig cfg,
                            QObject* parent = nullptr);
    ~PipeWireStream() override;

    bool open();
    void close();

    qint64 push(const char* data, qint64 bytes);
    qint64 pull(char* data, qint64 maxBytes);

    struct Telemetry {
        double   measuredLatencyMs    = 0.0;
        double   ringDepthMs          = 0.0;
        double   pwQuantumMs          = 0.0;
        double   deviceLatencyMs      = 0.0;
        uint64_t xrunCount            = 0;
        double   processCbCpuPct      = 0.0;
        QString  streamStateName      = QStringLiteral("closed");
        int      consumerCount        = 0;
    };
    Telemetry telemetry() const;

signals:
    void streamStateChanged(QString state);
    void telemetryUpdated();
    void errorOccurred(QString reason);

private:
    // libpipewire callbacks (static → instance dispatch).
    static void onProcessCb(void* userData);
    static void onStateChangedCb(void* userData,
                                 pw_stream_state old_,
                                 pw_stream_state new_,
                                 const char* error);
    static void onParamChangedCb(void* userData, uint32_t id,
                                 const spa_pod* param);

    void onProcessOutput();
    void onProcessInput();

    PipeWireThreadLoop* m_loop;
    StreamConfig        m_cfg;
    pw_stream*          m_stream = nullptr;
    spa_hook            m_listener;

    AudioRingSpsc<65536> m_ring;

    std::atomic<uint64_t> m_xruns{0};
    std::atomic<double>   m_cpuPct{0.0};
    std::atomic<double>   m_latencyMs{0.0};
    std::atomic<double>   m_deviceLatencyMs{0.0};

    QString m_stateName = QStringLiteral("closed");
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
```

- [ ] **Step 2: Minimal `.cpp` skeleton (just `configToProperties` + empty other methods)**

```cpp
// =================================================================
// src/core/audio/PipeWireStream.cpp  (NereusSDR)
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireStream.h"

#include <QLoggingCategory>
#include <pipewire/keys.h>

Q_DECLARE_LOGGING_CATEGORY(lcPw)

namespace NereusSDR {

pw_properties* configToProperties(const StreamConfig& cfg)
{
    pw_properties* p = pw_properties_new(
        PW_KEY_NODE_NAME,        cfg.nodeName.toUtf8().constData(),
        PW_KEY_NODE_DESCRIPTION, cfg.nodeDescription.toUtf8().constData(),
        PW_KEY_MEDIA_TYPE,       "Audio",
        PW_KEY_MEDIA_CATEGORY,
            cfg.direction == StreamConfig::Output ? "Playback" : "Capture",
        PW_KEY_MEDIA_ROLE,       cfg.mediaRole.toUtf8().constData(),
        PW_KEY_MEDIA_CLASS,      cfg.mediaClass.toUtf8().constData(),
        nullptr);

    if (!cfg.targetNodeName.isEmpty()) {
        pw_properties_set(p, PW_KEY_TARGET_OBJECT,
                          cfg.targetNodeName.toUtf8().constData());
    }

    pw_properties_setf(p, PW_KEY_NODE_RATE, "1/%u", cfg.rate);
    pw_properties_setf(p, PW_KEY_NODE_LATENCY, "%u/%u",
                       cfg.quantum, cfg.rate);

    return p;
}

PipeWireStream::PipeWireStream(PipeWireThreadLoop* loop,
                               StreamConfig cfg, QObject* parent)
    : QObject(parent), m_loop(loop), m_cfg(std::move(cfg)) {}

PipeWireStream::~PipeWireStream() { close(); }

bool PipeWireStream::open()  { return false; }  // Task 9
void PipeWireStream::close() {}                 // Task 9

qint64 PipeWireStream::push(const char*, qint64) { return 0; }    // Task 9
qint64 PipeWireStream::pull(char*, qint64)       { return 0; }    // Task 11

PipeWireStream::Telemetry PipeWireStream::telemetry() const {
    Telemetry t;
    t.streamStateName = m_stateName;
    t.xrunCount = m_xruns.load();
    t.processCbCpuPct = m_cpuPct.load();
    t.measuredLatencyMs = m_latencyMs.load();
    t.deviceLatencyMs = m_deviceLatencyMs.load();
    return t;
}

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
```

- [ ] **Step 3: Write the config test**

Create `tests/tst_pipewire_stream_config.cpp`:

```cpp
// =================================================================
// tests/tst_pipewire_stream_config.cpp
// Author: J.J. Boyd (KG4VCF), AI-assisted via Claude Code. 2026-04-23.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE

#include <QtTest/QtTest>
#include <pipewire/pipewire.h>
#include <pipewire/keys.h>
#include "core/audio/PipeWireStream.h"

using namespace NereusSDR;

class TestPipeWireStreamConfig : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { pw_init(nullptr, nullptr); }
    void cleanupTestCase() { pw_deinit(); }

    void outputVirtualSource_setsCoreKeys() {
        StreamConfig cfg;
        cfg.nodeName        = QStringLiteral("nereussdr.vax-1");
        cfg.nodeDescription = QStringLiteral("NereusSDR VAX 1");
        cfg.direction       = StreamConfig::Output;
        cfg.mediaClass      = QStringLiteral("Audio/Source");
        cfg.mediaRole       = QStringLiteral("Music");
        cfg.rate = 48000; cfg.channels = 2; cfg.quantum = 512;

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_NODE_NAME)),
                 cfg.nodeName);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_MEDIA_CLASS)),
                 cfg.mediaClass);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_MEDIA_CATEGORY)),
                 QStringLiteral("Playback"));
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_NODE_LATENCY)),
                 QStringLiteral("512/48000"));
        pw_properties_free(p);
    }

    void inputConsumer_setsCaptureCategory() {
        StreamConfig cfg;
        cfg.nodeName = QStringLiteral("nereussdr.tx-input");
        cfg.nodeDescription = QStringLiteral("NereusSDR TX input");
        cfg.direction = StreamConfig::Input;
        cfg.mediaClass = QStringLiteral("Stream/Input/Audio");
        cfg.mediaRole = QStringLiteral("Phone");

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_MEDIA_CATEGORY)),
                 QStringLiteral("Capture"));
        pw_properties_free(p);
    }

    void targetNodeName_setsTargetObject() {
        StreamConfig cfg;
        cfg.nodeName = QStringLiteral("nereussdr.rx-primary");
        cfg.nodeDescription = QStringLiteral("NereusSDR Primary");
        cfg.direction = StreamConfig::Output;
        cfg.mediaClass = QStringLiteral("Stream/Output/Audio");
        cfg.mediaRole = QStringLiteral("Music");
        cfg.targetNodeName = QStringLiteral("alsa_output.usb-headphones");

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(QString::fromUtf8(pw_properties_get(p, PW_KEY_TARGET_OBJECT)),
                 cfg.targetNodeName);
        pw_properties_free(p);
    }
};

QTEST_MAIN(TestPipeWireStreamConfig)
#include "tst_pipewire_stream_config.moc"

#else
int main() { return 0; }   // Compiles cleanly on hosts without libpipewire.
#endif
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 4: Build + run**

Run:
```
cd ~/nereussdr && cmake --build build -j$(nproc) && (cd build && ctest -R "tst_pipewire_stream_config" --output-on-failure)
```
Expected on this machine: 3 passed.

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/PipeWireStream.h src/core/audio/PipeWireStream.cpp tests/tst_pipewire_stream_config.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): PipeWireStream scaffolding + StreamConfig translator

Class declaration, StreamConfig struct, and pure-function
configToProperties() with 3 unit tests covering Output (virtual
source), Input (consumer), and targetNodeName routing. Stream
open/push/pull are stubbed out — real bodies land in Tasks 9, 10,
and 11.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: `PipeWireStream::open()` + OUTPUT direction + `on_process_output`

**Files:**
- Modify: `src/core/audio/PipeWireStream.cpp`

**Purpose:** First fully-functional PipeWire stream. Implements the OUTPUT direction so VAX sources + Primary/Sidetone/MON producers can work. INPUT direction defers to Task 11.

- [ ] **Step 1: Implement `open()`**

Replace the stub `open()` body. Full implementation references:

- spec §5.1 (interface contract)
- spec §5.2 (state machine)
- spec §5.4 (on_process_output body)

Code:

```cpp
static const pw_stream_events k_streamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy = nullptr,
    .state_changed = &PipeWireStream::onStateChangedCb,
    .control_info = nullptr,
    .io_changed = nullptr,
    .param_changed = &PipeWireStream::onParamChangedCb,
    .add_buffer = nullptr,
    .remove_buffer = nullptr,
    .process = &PipeWireStream::onProcessCb,
    .drained = nullptr,
    .command = nullptr,
    .trigger_done = nullptr,
};

bool PipeWireStream::open()
{
    if (!m_loop || !m_loop->core()) {
        qCWarning(lcPw) << "open() with no thread loop or core";
        return false;
    }

    m_loop->lock();
    m_stream = pw_stream_new(m_loop->core(),
                             m_cfg.nodeName.toUtf8().constData(),
                             configToProperties(m_cfg));
    if (!m_stream) {
        m_loop->unlock();
        qCWarning(lcPw) << "pw_stream_new failed for" << m_cfg.nodeName;
        return false;
    }
    pw_stream_add_listener(m_stream, &m_listener, &k_streamEvents, this);

    // Format param: S_F32_LE, channels, rate.
    uint8_t buffer[1024];
    spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32_LE;
    info.channels = m_cfg.channels;
    info.rate = m_cfg.rate;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    const auto flags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS);

    const auto dir = (m_cfg.direction == StreamConfig::Output)
                       ? PW_DIRECTION_OUTPUT : PW_DIRECTION_INPUT;

    const int r = pw_stream_connect(m_stream, dir, PW_ID_ANY, flags,
                                    params, 1);
    m_loop->unlock();
    if (r < 0) {
        qCWarning(lcPw) << "pw_stream_connect failed:" << r;
        return false;
    }

    qCInfo(lcPw) << "stream opened:" << m_cfg.nodeName
                 << "direction:" << (dir == PW_DIRECTION_OUTPUT ? "OUT" : "IN");
    return true;
}
```

- [ ] **Step 2: Implement `close()`**

```cpp
void PipeWireStream::close()
{
    if (!m_stream) { return; }
    m_loop->lock();
    pw_stream_disconnect(m_stream);
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
    spa_hook_remove(&m_listener);
    m_loop->unlock();
    m_stateName = QStringLiteral("closed");
}
```

- [ ] **Step 3: Implement state-change + param callbacks**

```cpp
void PipeWireStream::onStateChangedCb(void* userData, pw_stream_state,
                                      pw_stream_state s, const char* err)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    QString name;
    switch (s) {
        case PW_STREAM_STATE_ERROR:     name = QStringLiteral("error"); break;
        case PW_STREAM_STATE_UNCONNECTED: name = QStringLiteral("unconnected"); break;
        case PW_STREAM_STATE_CONNECTING:  name = QStringLiteral("connecting"); break;
        case PW_STREAM_STATE_PAUSED:    name = QStringLiteral("paused"); break;
        case PW_STREAM_STATE_STREAMING: name = QStringLiteral("streaming"); break;
        default: name = QStringLiteral("unknown"); break;
    }
    self->m_stateName = name;
    QMetaObject::invokeMethod(self, [self, name]() {
        emit self->streamStateChanged(name);
    }, Qt::QueuedConnection);
    if (err) {
        QString reason = QString::fromUtf8(err);
        QMetaObject::invokeMethod(self, [self, reason]() {
            emit self->errorOccurred(reason);
        }, Qt::QueuedConnection);
    }
}

void PipeWireStream::onParamChangedCb(void* /*userData*/, uint32_t /*id*/,
                                      const spa_pod* /*param*/)
{
    // TODO(task 10): extract quantum when SPA_PARAM_Latency arrives.
}
```

- [ ] **Step 4: Implement `on_process` for OUTPUT**

```cpp
void PipeWireStream::onProcessCb(void* userData)
{
    auto* self = static_cast<PipeWireStream*>(userData);
    if (self->m_cfg.direction == StreamConfig::Output) {
        self->onProcessOutput();
    } else {
        self->onProcessInput();   // Task 11
    }
}

void PipeWireStream::onProcessOutput()
{
    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    spa_buffer* sb = b->buffer;
    auto* dst = static_cast<uint8_t*>(sb->datas[0].data);
    const uint32_t dstCapacity = sb->datas[0].maxsize;

    const qint64 popped = m_ring.popInto(dst, qint64(dstCapacity));
    if (popped < qint64(dstCapacity)) {
        std::memset(dst + popped, 0, dstCapacity - size_t(popped));
    }

    sb->datas[0].chunk->offset = 0;
    sb->datas[0].chunk->stride = sizeof(float) * m_cfg.channels;
    sb->datas[0].chunk->size   = dstCapacity;
    pw_stream_queue_buffer(m_stream, b);
}
```

- [ ] **Step 5: Implement `push()`**

```cpp
qint64 PipeWireStream::push(const char* data, qint64 bytes)
{
    return m_ring.pushCopy(reinterpret_cast<const uint8_t*>(data), bytes);
}
```

- [ ] **Step 6: Build + quick smoke**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: green.

Full integration smoke defers to Task 24 (env-gated test). This task ends here.

- [ ] **Step 7: Commit**

```bash
git add src/core/audio/PipeWireStream.cpp
git commit -S -m "$(cat <<'EOF'
feat(audio): PipeWireStream OUTPUT direction — open/close/push/on_process

First fully-functional PipeWire stream path. open() builds the
format param (F32LE stereo 48 kHz), connects as either OUTPUT or
INPUT per StreamConfig. on_process_output dequeues a buffer, drains
our SPSC ring into it, silence-fills any shortfall, queues the
buffer back. push() writes into the ring; xrun counter increments
when dequeue returns null.

INPUT direction stays stubbed — lands in Task 11. Telemetry
measurement (latency, CPU, quantum from on_param) lands in Task 10.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Telemetry — latency, CPU, quantum, 1 Hz emit

**Files:**
- Modify: `src/core/audio/PipeWireStream.cpp`

**Purpose:** Fill in telemetry plumbing so the Setup page can display live values. Spec §5.5, §5.6.

- [ ] **Step 1: Add CPU-timing to `onProcessOutput`**

Wrap the existing body:

```cpp
void PipeWireStream::onProcessOutput()
{
    timespec t0{}, t1{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);

    // … existing body …

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    const double cbNs = double((t1.tv_sec - t0.tv_sec) * 1'000'000'000LL
                               + (t1.tv_nsec - t0.tv_nsec));
    const double quantumNs = double(m_cfg.quantum) / m_cfg.rate * 1e9;
    m_cpuPct.store(quantumNs > 0.0 ? cbNs / quantumNs * 100.0 : 0.0,
                   std::memory_order_relaxed);

    maybeEmitTelemetry();
}
```

- [ ] **Step 2: Add `maybeEmitTelemetry` + latency query (member + private)**

Add a private member `std::atomic<qint64> m_lastTelemetryNs{0}` in the header; update the Telemetry struct already has the fields.

Implement:

```cpp
void PipeWireStream::maybeEmitTelemetry()
{
    const qint64 nowNs = qint64(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    const qint64 last = m_lastTelemetryNs.load(std::memory_order_relaxed);
    if (nowNs - last < 1'000'000'000) { return; }
    m_lastTelemetryNs.store(nowNs, std::memory_order_relaxed);

    pw_time t{};
    pw_stream_get_time_n(m_stream, &t, sizeof(t));

    // Convert: t.buffered (ns) + t.delay (samples → ns at rate) dominate.
    const double bufferedMs = double(t.buffered) / 1e6;
    const double delayMs    = t.rate.denom
        ? double(t.delay) * 1000.0 * t.rate.num / double(t.rate.denom)
        : 0.0;
    const double ringMs     = double(m_ring.usedBytes())
                            / (m_cfg.rate * m_cfg.channels * sizeof(float))
                            * 1000.0;
    m_latencyMs.store(bufferedMs + delayMs + ringMs,
                      std::memory_order_relaxed);
    m_deviceLatencyMs.store(delayMs, std::memory_order_relaxed);

    QMetaObject::invokeMethod(this, &PipeWireStream::telemetryUpdated,
                              Qt::QueuedConnection);
}
```

- [ ] **Step 3: Update `Telemetry` derivation in `telemetry()`**

Fill `ringDepthMs`, `pwQuantumMs` (derivable from `m_cfg.quantum / m_cfg.rate`):

```cpp
Telemetry t;
t.streamStateName = m_stateName;
t.xrunCount       = m_xruns.load();
t.processCbCpuPct = m_cpuPct.load();
t.measuredLatencyMs = m_latencyMs.load();
t.deviceLatencyMs = m_deviceLatencyMs.load();
t.ringDepthMs = double(m_ring.usedBytes())
              / (m_cfg.rate * m_cfg.channels * sizeof(float)) * 1000.0;
t.pwQuantumMs = double(m_cfg.quantum) / m_cfg.rate * 1000.0;
return t;
```

- [ ] **Step 4: Build**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: green.

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/PipeWireStream.cpp src/core/audio/PipeWireStream.h
git commit -S -m "$(cat <<'EOF'
feat(audio): PipeWireStream telemetry — 1 Hz latency/xrun/CPU

clock_gettime(CLOCK_THREAD_CPUTIME_ID) bracketed around the
process callback body gives us CPU utilisation per quantum.
pw_stream_get_time_n() + ring depth produce end-to-end latency
broken into (ring · pipewire · device). Coalesced to one
telemetryUpdated signal per second. Queued-connection emission
means UI thread consumes without touching the pw thread loop lock.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: `PipeWireStream` INPUT direction — `pull` + `on_process_input`

**Files:**
- Modify: `src/core/audio/PipeWireStream.cpp`

**Purpose:** Complete the TX input path: PipeWire pushes samples from a user-picked source into our ring, DSP thread reads them with `pull()`.

- [ ] **Step 1: Implement `onProcessInput`**

```cpp
void PipeWireStream::onProcessInput()
{
    timespec t0{}, t1{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);

    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1, std::memory_order_relaxed); return; }

    const spa_buffer* sb = b->buffer;
    const auto* src = static_cast<const uint8_t*>(sb->datas[0].data);
    const uint32_t size = sb->datas[0].chunk->size;
    m_ring.pushCopy(src, qint64(size));

    pw_stream_queue_buffer(m_stream, b);

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    const double cbNs = double((t1.tv_sec - t0.tv_sec) * 1'000'000'000LL
                               + (t1.tv_nsec - t0.tv_nsec));
    const double quantumNs = double(m_cfg.quantum) / m_cfg.rate * 1e9;
    m_cpuPct.store(quantumNs > 0.0 ? cbNs / quantumNs * 100.0 : 0.0,
                   std::memory_order_relaxed);

    maybeEmitTelemetry();
}
```

- [ ] **Step 2: Implement `pull`**

```cpp
qint64 PipeWireStream::pull(char* data, qint64 maxBytes)
{
    return m_ring.popInto(reinterpret_cast<uint8_t*>(data), maxBytes);
}
```

- [ ] **Step 3: Build**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -3`
Expected: green.

- [ ] **Step 4: Commit**

```bash
git add src/core/audio/PipeWireStream.cpp
git commit -S -m "$(cat <<'EOF'
feat(audio): PipeWireStream INPUT direction — on_process_input + pull()

Mirror of the OUTPUT path. on_process_input dequeues a filled
buffer, copies into our ring, returns the buffer. pull() drains
for the DSP thread. Same CPU + telemetry accounting as OUTPUT.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: `PipeWireBus` — `IAudioBus` wrapper with Role enum

**Files:**
- Create: `src/core/audio/PipeWireBus.h`
- Create: `src/core/audio/PipeWireBus.cpp`
- Modify: `CMakeLists.txt` (under `PIPEWIRE_FOUND`)

**Purpose:** Bridges `IAudioBus` to `PipeWireStream`. One wrapper class, eight roles.

- [ ] **Step 1: Header**

```cpp
// =================================================================
// src/core/audio/PipeWireBus.h  (NereusSDR) — GPLv2-or-later.
// =================================================================
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include "core/IAudioBus.h"
#include "core/audio/PipeWireStream.h"
#include <memory>

namespace NereusSDR {

class PipeWireThreadLoop;

class PipeWireBus : public IAudioBus {
public:
    enum class Role {
        Vax1, Vax2, Vax3, Vax4,
        TxInput,
        Primary, Sidetone, Monitor,
        PerSlice           // target-specific Primary alias, name via ctor
    };

    PipeWireBus(Role role, PipeWireThreadLoop* loop,
                QString targetOverride = QString(),
                QString nodeNameOverride = QString());
    ~PipeWireBus() override;

    bool open(const AudioFormat& fmt) override;
    void close() override;
    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;
    float  rxLevel() const override;
    bool   isOpen()  const override { return m_open; }
    QString errorString() const override { return m_err; }

    PipeWireStream* stream() const { return m_stream.get(); }

private:
    StreamConfig configFor(Role r,
                           const QString& targetOverride,
                           const QString& nodeNameOverride) const;

    Role m_role;
    PipeWireThreadLoop* m_loop;
    std::unique_ptr<PipeWireStream> m_stream;
    bool m_open = false;
    QString m_err;
    std::atomic<float> m_rxLevel{0.0f};
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
```

- [ ] **Step 2: Implementation — role → StreamConfig mapping**

```cpp
// src/core/audio/PipeWireBus.cpp
#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireBus.h"
#include "core/audio/PipeWireThreadLoop.h"

namespace NereusSDR {

StreamConfig PipeWireBus::configFor(Role r,
                                    const QString& target,
                                    const QString& nameOverride) const
{
    StreamConfig c;
    c.rate = 48000; c.channels = 2;
    const auto name = [&](QString d){
        return nameOverride.isEmpty() ? d : nameOverride;
    };
    switch (r) {
        case Role::Vax1: case Role::Vax2: case Role::Vax3: case Role::Vax4: {
            const int n = int(r) - int(Role::Vax1) + 1;
            c.nodeName        = name(QStringLiteral("nereussdr.vax-%1").arg(n));
            c.nodeDescription = QStringLiteral("NereusSDR VAX %1").arg(n);
            c.direction   = StreamConfig::Output;
            c.mediaClass  = QStringLiteral("Audio/Source");
            c.mediaRole   = QStringLiteral("Music");
            c.quantum     = 512;
            break;
        }
        case Role::TxInput:
            c.nodeName        = name(QStringLiteral("nereussdr.tx-input"));
            c.nodeDescription = QStringLiteral("NereusSDR TX input");
            c.direction   = StreamConfig::Input;
            c.mediaClass  = QStringLiteral("Stream/Input/Audio");
            c.mediaRole   = QStringLiteral("Phone");
            c.quantum     = 256;
            c.targetNodeName = target;
            break;
        case Role::Primary: case Role::PerSlice:
            c.nodeName        = name(QStringLiteral("nereussdr.rx-primary"));
            c.nodeDescription = QStringLiteral("NereusSDR Primary Output");
            c.direction   = StreamConfig::Output;
            c.mediaClass  = QStringLiteral("Stream/Output/Audio");
            c.mediaRole   = QStringLiteral("Music");
            c.quantum     = 512;
            c.targetNodeName = target;
            break;
        case Role::Sidetone:
            c.nodeName        = name(QStringLiteral("nereussdr.cw-sidetone"));
            c.nodeDescription = QStringLiteral("NereusSDR CW Sidetone");
            c.direction   = StreamConfig::Output;
            c.mediaClass  = QStringLiteral("Stream/Output/Audio");
            c.mediaRole   = QStringLiteral("Communication");
            c.quantum     = 128;
            c.targetNodeName = target;
            break;
        case Role::Monitor:
            c.nodeName        = name(QStringLiteral("nereussdr.tx-monitor"));
            c.nodeDescription = QStringLiteral("NereusSDR TX Monitor");
            c.direction   = StreamConfig::Output;
            c.mediaClass  = QStringLiteral("Stream/Output/Audio");
            c.mediaRole   = QStringLiteral("Communication");
            c.quantum     = 256;
            c.targetNodeName = target;
            break;
    }
    return c;
}

PipeWireBus::PipeWireBus(Role role, PipeWireThreadLoop* loop,
                         QString targetOverride, QString nodeNameOverride)
    : m_role(role), m_loop(loop),
      m_stream(std::make_unique<PipeWireStream>(
          loop, configFor(role, targetOverride, nodeNameOverride))) {}

PipeWireBus::~PipeWireBus() { close(); }

bool PipeWireBus::open(const AudioFormat& fmt)
{
    if (fmt.sampleRate != 48000 || fmt.channels != 2
        || fmt.sample != AudioFormat::Sample::Float32) {
        m_err = QStringLiteral("unsupported format (48000/stereo/F32 required)");
        return false;
    }
    m_open = m_stream->open();
    if (!m_open) { m_err = QStringLiteral("pw_stream_connect failed"); }
    return m_open;
}

void PipeWireBus::close()
{
    if (m_stream) { m_stream->close(); }
    m_open = false;
}

qint64 PipeWireBus::push(const char* data, qint64 bytes)
{
    return m_stream ? m_stream->push(data, bytes) : 0;
}
qint64 PipeWireBus::pull(char* data, qint64 maxBytes)
{
    return m_stream ? m_stream->pull(data, maxBytes) : 0;
}
float PipeWireBus::rxLevel() const { return m_rxLevel.load(); }

}  // namespace NereusSDR
#endif
```

- [ ] **Step 3: Register in CMake (under `PIPEWIRE_FOUND`)**

- [ ] **Step 4: Build**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: green.

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/PipeWireBus.h src/core/audio/PipeWireBus.cpp CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): PipeWireBus — IAudioBus wrapper with role-to-config mapping

Single wrapper class serves all eight Linux audio roles
(VAX 1-4, TxInput, Primary, Sidetone, Monitor, PerSlice). configFor
centralises the role→StreamConfig translation including node names,
media.class, media.role, and default quanta per spec §6.1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 13: `QAudioSinkAdapter` — thin `IAudioBus` wrapper

**Files:**
- Create: `src/core/audio/QAudioSinkAdapter.h`
- Create: `src/core/audio/QAudioSinkAdapter.cpp`

**Purpose:** Minimal IAudioBus-over-QAudioSink so AudioEngine treats the Pactl/None primary speakers path uniformly.

- [ ] **Step 1: Header + impl**

```cpp
// src/core/audio/QAudioSinkAdapter.h
#pragma once
#include "core/IAudioBus.h"
#include <QAudioDevice>
#include <QAudioSink>
#include <memory>
#include <atomic>

namespace NereusSDR {

class QAudioSinkAdapter : public IAudioBus {
public:
    explicit QAudioSinkAdapter(const QAudioDevice& device);
    ~QAudioSinkAdapter() override;

    bool open(const AudioFormat& fmt) override;
    void close() override;
    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char*, qint64) override { return 0; }
    float  rxLevel() const override { return m_rxLevel.load(); }
    bool   isOpen() const override { return m_sink != nullptr; }
    QString errorString() const override { return m_err; }

private:
    QAudioDevice m_device;
    std::unique_ptr<QAudioSink> m_sink;
    QIODevice* m_io = nullptr;
    QString m_err;
    std::atomic<float> m_rxLevel{0.0f};
};

}  // namespace NereusSDR
```

```cpp
// src/core/audio/QAudioSinkAdapter.cpp
#include "core/audio/QAudioSinkAdapter.h"
#include <QAudioFormat>

namespace NereusSDR {

QAudioSinkAdapter::QAudioSinkAdapter(const QAudioDevice& d) : m_device(d) {}
QAudioSinkAdapter::~QAudioSinkAdapter() { close(); }

bool QAudioSinkAdapter::open(const AudioFormat& fmt)
{
    QAudioFormat qf;
    qf.setSampleRate(int(fmt.sampleRate));
    qf.setChannelCount(int(fmt.channels));
    qf.setSampleFormat(QAudioFormat::Float);
    if (!m_device.isFormatSupported(qf)) {
        m_err = QStringLiteral("unsupported format");
        return false;
    }
    m_sink = std::make_unique<QAudioSink>(m_device, qf);
    m_io   = m_sink->start();
    return m_io != nullptr;
}

void QAudioSinkAdapter::close()
{
    if (m_sink) { m_sink->stop(); m_sink.reset(); m_io = nullptr; }
}

qint64 QAudioSinkAdapter::push(const char* data, qint64 bytes)
{
    return m_io ? m_io->write(data, bytes) : 0;
}

}  // namespace NereusSDR
```

- [ ] **Step 2: Build**

Run: `cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -3`

- [ ] **Step 3: Commit**

```bash
git add src/core/audio/QAudioSinkAdapter.h src/core/audio/QAudioSinkAdapter.cpp CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): QAudioSinkAdapter — IAudioBus over QAudioSink for fallback

Used for the Primary speakers path on Pactl/None Linux hosts and on
platforms that aren't PipeWire-equipped. Minimal wrapper — Qt handles
the rest.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 14: `AudioEngine::makeVaxBus` — backend-aware dispatch

**Files:**
- Modify: `src/core/AudioEngine.h` — add `PipeWireThreadLoop m_pwLoop` (ptr), new factory methods.
- Modify: `src/core/AudioEngine.cpp` — dispatch in `makeVaxBus`, add `makeTxInputBus`, `makePrimaryOut`, `makeSidetoneOut`, `makeMonitorOut`.

**Purpose:** End-to-end "NereusSDR VAX 1" appears in WSJT-X on this machine.

- [ ] **Step 1: Add thread-loop member**

In `AudioEngine.h`:

```cpp
#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    std::unique_ptr<PipeWireThreadLoop> m_pwLoop;
#endif
```

Forward-declare `PipeWireThreadLoop` at top.

- [ ] **Step 2: Start the thread loop in ctor when backend == PipeWire**

In `AudioEngine::AudioEngine()` after the detection log:

```cpp
#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    if (m_linuxBackend == LinuxAudioBackend::PipeWire) {
        m_pwLoop = std::make_unique<PipeWireThreadLoop>();
        if (!m_pwLoop->connect()) {
            qCWarning(lcAudio) << "PipeWire connect failed — "
                                  "falling back to Pactl detection";
            m_pwLoop.reset();
            m_linuxBackend = LinuxAudioBackend::Pactl;
        }
    }
#endif
```

- [ ] **Step 3: Rewrite the Linux branch of `makeVaxBus`**

Replace the `#elif defined(Q_OS_LINUX)` block in `makeVaxBus`:

```cpp
#elif defined(Q_OS_LINUX)
#  ifdef NEREUS_HAVE_PIPEWIRE
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        auto role = PipeWireBus::Role::Vax1;
        switch (channel) {
            case 1: role = PipeWireBus::Role::Vax1; break;
            case 2: role = PipeWireBus::Role::Vax2; break;
            case 3: role = PipeWireBus::Role::Vax3; break;
            case 4: role = PipeWireBus::Role::Vax4; break;
        }
        auto bus = std::make_unique<PipeWireBus>(role, m_pwLoop.get());
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for VAX"
                               << channel << ":" << bus->errorString();
            return nullptr;
        }
        return bus;
    }
#  endif
    // Pactl fallback: existing LinuxPipeBus path — unchanged.
    if (m_linuxBackend == LinuxAudioBackend::Pactl) {
        LinuxPipeBus::Role role = LinuxPipeBus::Role::Vax1;
        switch (channel) {
            case 1: role = LinuxPipeBus::Role::Vax1; break;
            case 2: role = LinuxPipeBus::Role::Vax2; break;
            case 3: role = LinuxPipeBus::Role::Vax3; break;
            case 4: role = LinuxPipeBus::Role::Vax4; break;
        }
        auto bus = std::make_unique<LinuxPipeBus>(role);
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "LinuxPipeBus open failed for VAX"
                               << channel << ":" << bus->errorString();
            return nullptr;
        }
        return bus;
    }
    qCInfo(lcAudio) << "Linux VAX" << channel
                    << "disabled — no audio backend";
    return nullptr;
#endif
```

- [ ] **Step 4: Add the other factory methods**

In the public API:

```cpp
std::unique_ptr<IAudioBus> makeTxInputBus(const QString& sourceNode = {});
std::unique_ptr<IAudioBus> makePrimaryOut(const QString& targetNode = {});
std::unique_ptr<IAudioBus> makeSidetoneOut(const QString& targetNode = {});
std::unique_ptr<IAudioBus> makeMonitorOut(const QString& targetNode = {});
```

Implementations follow the same dispatch pattern — short code, one per method.

- [ ] **Step 5: Build + run the app**

```
cd ~/nereussdr && cmake --build build -j$(nproc) 2>&1 | tail -5
pkill -f NereusSDR; sleep 1
nohup ./build/NereusSDR > /tmp/nereussdr-task14.log 2>&1 &
sleep 5
grep -E "(VAX|PipeWire|pactl)" /tmp/nereussdr-task14.log | head -20
pw-cli list-objects Node | grep -i nereussdr
```

Expected: log shows "PipeWire thread loop connected — server version 1.4.x".
`pw-cli list-objects Node` returns lines for `nereussdr.vax-1` through `vax-4` (and `tx-input` if enabled).

- [ ] **Step 6: Visual in WSJT-X**

Launch `wsjtx &` (or skip this step if WSJT-X not installed on this box). In WSJT-X: Settings → Audio → Input dropdown — confirm "NereusSDR VAX 1" is present.

- [ ] **Step 7: Commit**

```bash
pkill -f NereusSDR
git add src/core/AudioEngine.h src/core/AudioEngine.cpp
git commit -S -m "$(cat <<'EOF'
feat(audio): AudioEngine dispatches to PipeWireBus when backend is PipeWire

Ctor starts a PipeWireThreadLoop when LinuxAudioBackend::PipeWire is
detected. makeVaxBus() routes to PipeWireBus on that path and keeps
LinuxPipeBus for Pactl fallback. Adds makeTxInputBus, makePrimaryOut,
makeSidetoneOut, makeMonitorOut factories for the split output
streams. End-to-end test: nereussdr.vax-1..4 nodes visible via
pw-cli list-objects and selectable in WSJT-X on the affected host.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 15: `SliceModel::sinkNodeName` property + persistence + unit test

**Files:**
- Modify: `src/models/SliceModel.h` — property, getter/setter/signal/member.
- Modify: `src/models/SliceModel.cpp` — setter body, persist/restore.
- Create: `tests/tst_slice_sink_routing.cpp`

**Purpose:** Per-slice routing key. Fully TDD.

- [ ] **Step 1: Failing test**

```cpp
// tests/tst_slice_sink_routing.cpp
#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceSinkRouting : public QObject {
    Q_OBJECT
private slots:
    void defaultIsEmpty() {
        SliceModel s;
        QCOMPARE(s.sinkNodeName(), QString());
    }
    void setterStoresValue() {
        SliceModel s;
        s.setSinkNodeName(QStringLiteral("alsa_output.usb-headphones"));
        QCOMPARE(s.sinkNodeName(), QStringLiteral("alsa_output.usb-headphones"));
    }
    void setterEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::sinkNodeNameChanged);
        s.setSinkNodeName(QStringLiteral("x"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("x"));
    }
    void noSignalOnSameValue() {
        SliceModel s;
        s.setSinkNodeName(QStringLiteral("x"));
        QSignalSpy spy(&s, &SliceModel::sinkNodeNameChanged);
        s.setSinkNodeName(QStringLiteral("x"));
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceSinkRouting)
#include "tst_slice_sink_routing.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 2: Run — expect compile fail**

- [ ] **Step 3: Add property + getter/setter/signal/member**

In `SliceModel.h`, near other Q_PROPERTYs:

```cpp
Q_PROPERTY(QString sinkNodeName READ sinkNodeName WRITE setSinkNodeName
           NOTIFY sinkNodeNameChanged)
```

Accessors:

```cpp
QString sinkNodeName() const { return m_sinkNodeName; }
void setSinkNodeName(const QString& v);
```

Signal:

```cpp
void sinkNodeNameChanged(const QString& value);
```

Member:

```cpp
QString m_sinkNodeName;
```

Setter in `.cpp`:

```cpp
void SliceModel::setSinkNodeName(const QString& v)
{
    if (m_sinkNodeName == v) { return; }
    m_sinkNodeName = v;
    emit sinkNodeNameChanged(v);
}
```

- [ ] **Step 4: Run tests — PASS**

```
cd ~/nereussdr && cmake --build build -j$(nproc) && (cd build && ctest -R tst_slice_sink_routing --output-on-failure)
```
Expected: 4 PASS.

- [ ] **Step 5: Wire persistence**

Find `SliceModel::saveToAppSettings` and `loadFromAppSettings` in `SliceModel.cpp`. Add per spec §10:

```cpp
// in saveToAppSettings():
s.setValue(sp + QStringLiteral("SinkNodeName"), m_sinkNodeName);

// in loadFromAppSettings():
setSinkNodeName(s.value(sp + QStringLiteral("SinkNodeName"),
                        QStringLiteral("")).toString());
```

- [ ] **Step 6: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_sink_routing.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(slice): add sinkNodeName property for per-slice audio routing

Empty string = route to Primary output (default); any non-empty
value identifies a PipeWire node.name to target. Persisted as
Slice<N>/SinkNodeName in AppSettings (per-slice, not per-band).
Four unit tests cover default, set, signal emission, no-op on
same value.

Aligns with the slice-based routing vision — no RX1/RX2 bifurcation.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 16: `AudioEngine` route dispatch in `rxBlockReady` + cached per-target bus

**Files:**
- Modify: `src/core/AudioEngine.h` — add `QHash<QString, std::unique_ptr<IAudioBus>> m_routedBuses`.
- Modify: `src/core/AudioEngine.cpp` — route lookup in `rxBlockReady`.
- Create: `tests/tst_audio_engine_route_dispatch.cpp`

**Purpose:** DSP → correct stream based on `slice.sinkNodeName()`.

- [ ] **Step 1: Failing test (mocked `IAudioBus`)**

Test creates a stub AudioEngine-like shim or exercises via public API with a `FakeBus`. The simplest approach: add a public hook for tests, or write an integration-style test that pushes frames and checks which mock bus received them. Implementation detail — pick whichever integration seam fits existing test patterns.

Minimum test body:

```cpp
void dispatchesToNamedBus() {
    AudioEngine eng;
    auto* fakePrimary = new FakeBus;
    auto* fakeOther   = new FakeBus;
    eng.installFakeBusForTest(QStringLiteral(""),   fakePrimary);
    eng.installFakeBusForTest(QStringLiteral("X"),  fakeOther);

    SliceModel slice;
    slice.setSinkNodeName(QStringLiteral("X"));
    // Feed a block — verify it went to fakeOther, not fakePrimary.
    eng.rxBlockReady(slice.id(), dummyFrames, 1024);
    QVERIFY(fakeOther->bytesReceived > 0);
    QCOMPARE(fakePrimary->bytesReceived, 0);
}
```

`installFakeBusForTest` is a `#ifdef NEREUS_ENABLE_TEST_HOOKS` method.

- [ ] **Step 2: Implement route lookup in `rxBlockReady`**

In the existing rxBlockReady handler, insert route selection:

```cpp
const QString key = slice.sinkNodeName();   // "" means primary
auto it = m_routedBuses.find(key);
if (it == m_routedBuses.end()) {
    auto bus = makePrimaryOut(key);   // key doubles as target node name
    if (!bus) { return; }
    it = m_routedBuses.insert(key, std::move(bus));
}
it->push(reinterpret_cast<const char*>(samples), frames * frameBytes);
```

- [ ] **Step 3: Run tests — PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/AudioEngine.h src/core/AudioEngine.cpp tests/tst_audio_engine_route_dispatch.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): per-slice output routing dispatch in rxBlockReady

Route key = slice.sinkNodeName(); empty key → Primary stream.
Buses cached by key in m_routedBuses so the hot path is a
QHash lookup plus a push. Unit test via installFakeBusForTest
hook verifies correct dispatch.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 17: `AudioBackendStrip` widget

**Files:**
- Create: `src/gui/setup/AudioBackendStrip.{h,cpp}`

**Purpose:** Diagnostic strip shown at the top of each Setup → Audio sub-page.

- [ ] **Step 1: Header + cpp**

Standard `QWidget` with three labels (backend name, version/socket, session/default) + two `QPushButton`s ("Rescan", "Open logs"). Wires to `AudioEngine::linuxBackendChanged`. Exact code:

```cpp
// AudioBackendStrip.h
#pragma once
#include <QWidget>
class QLabel; class QPushButton;
namespace NereusSDR { class AudioEngine; }
namespace NereusSDR {
class AudioBackendStrip : public QWidget {
    Q_OBJECT
public:
    explicit AudioBackendStrip(AudioEngine* eng, QWidget* parent = nullptr);
private slots:
    void refresh();
    void onRescanClicked();
    void onOpenLogsClicked();
private:
    AudioEngine* m_eng;
    QLabel* m_primaryLabel;
    QLabel* m_detailsLabel;
    QPushButton* m_rescan;
    QPushButton* m_openLogs;
};
}
```

Implementation wires to `m_eng->linuxBackend()` + `m_eng->rescanLinuxBackend()` + `QDesktopServices::openUrl` for the log folder (see spec §9.1).

- [ ] **Step 2: Build + commit**

---

## Task 18: `VaxLinuxFirstRunDialog`

**Files:**
- Create: `src/gui/VaxLinuxFirstRunDialog.{h,cpp}`

**Purpose:** Modal dialog with detected state + per-distro remediation + Copy / Rescan / Dismiss per spec §8.2.

- [ ] **Step 1: Header + cpp**

Standard `QDialog` subclass. Takes `AudioEngine*`. Ctor populates labels from current detection state (so re-opening from `Help → Diagnose` shows current, not stale, state). "Rescan" button calls `eng->rescanLinuxBackend()` and repopulates. "Copy commands" copies the three apt/dnf/pacman lines via `QClipboard`. "Dismiss" sets `AppSettings::value("Audio/LinuxFirstRunSeen", true)`.

Exact text copy: see spec §8.2 verbatim.

- [ ] **Step 2: Build + commit**

---

## Task 19: `Help → Diagnose audio backend` menu item

**Files:**
- Modify: `src/gui/MainWindow.{h,cpp}`

- [ ] **Step 1: Add `QAction* m_helpDiagnoseAudioAction`**

In `setupMenus` or equivalent (find the existing Help menu population):

```cpp
m_helpDiagnoseAudioAction = m_helpMenu->addAction(tr("Diagnose audio backend…"));
connect(m_helpDiagnoseAudioAction, &QAction::triggered, this, &MainWindow::showAudioDiagnoseDialog);
```

Handler opens `VaxLinuxFirstRunDialog` in non-first-run mode.

- [ ] **Step 2: Build + commit**

---

## Task 20: `AudioOutputPage` — Primary / Sidetone / MON / Per-slice

**Files:**
- Create: `src/gui/setup/AudioOutputPage.{h,cpp}`

**Purpose:** The Output sub-page of Setup → Audio. Three cards + per-slice routing section. Spec §9.3.

- [ ] **Step 1: Header + cpp (large but mechanical)**

Standard Qt form layout. Widgets per card:

- Target sink `QComboBox` (populated by pw node enumeration; placeholder list OK for first pass).
- `follow system default` checkbox.
- Read-only info labels: `node.name`, `node.id`, `media.role`, `rate`, `format`.
- Quantum preset buttons (`QButtonGroup` with labels "128", "256", "512", "1024", "custom").
- Telemetry block: `QLabel`s for measuredLatency, ring/pw/device breakdown, xrun count, CPU pct, stream state. Updated on `PipeWireStream::telemetryUpdated` via `QMetaObject::invokeMethod` queued connection.
- Volume slider + Mute-on-TX checkbox.

For Sidetone / MON: same layout minus the volume/mute block.

Per-slice section: iterate `ReceiverManager`'s slices, render one row per slice with a target-sink combo.

Persistence: every control binds to its AppSettings key from spec §10.

- [ ] **Step 2: Build + commit**

---

## Task 21: `AudioVaxPage` rebuild with telemetry

**Files:**
- Modify: `src/gui/setup/AudioVaxPage.{h,cpp}`

**Purpose:** Rework each VAX card per spec §9.2.

- [ ] **Step 1: Replace the existing 7-row DeviceCard form with the new layout**

Per spec §9.2 mockup: "Exposed to system as:", "Format:", "Consumers:", "Level:", Rename + Copy buttons.

Drop device-picker rows — VAX is a source we expose, not a device we pick.

- [ ] **Step 2: Wire telemetry**

If `m_engine->makeVaxBus(n)` returned a `PipeWireBus*`, connect its `stream()->telemetryUpdated` to a slot that reads `telemetry()` and paints the card.

- [ ] **Step 3: Build + commit**

---

## Task 22: `SetupDialog` wiring — register Output page + strip header

**Files:**
- Modify: `src/gui/setup/SetupDialog.cpp`

- [ ] **Step 1: Register AudioOutputPage under the Audio category**

Find the block that registers AudioDevicesPage / AudioVaxPage / AudioAdvancedPage. Insert AudioOutputPage between Devices and VAX.

- [ ] **Step 2: Add `AudioBackendStrip` at the top of each Audio sub-page**

Either wrap each audio sub-page's layout with the strip, or subclass and prepend in the base class used by audio pages.

- [ ] **Step 3: Build + commit**

---

## Task 23: First-run dialog trigger + auto-open on startup

**Files:**
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Check on startup**

In `MainWindow::MainWindow()` after `AudioEngine` is up:

```cpp
#if defined(Q_OS_LINUX)
    if (m_audio->linuxBackend() == LinuxAudioBackend::None
        && !AppSettings::instance().value("Audio/LinuxFirstRunSeen",
                                          false).toBool()) {
        QTimer::singleShot(0, this, &MainWindow::showAudioDiagnoseDialog);
    }
#endif
```

- [ ] **Step 2: Build + commit**

---

## Task 24: Env-gated integration test — real stream lifecycle

**Files:**
- Create: `tests/tst_pipewire_stream_integration.cpp`

**Purpose:** Real `pw_stream_connect` reaches STREAMING; push a short tone; clean shutdown.

- [ ] **Step 1: Write test (env-gated)**

```cpp
#ifdef NEREUS_HAVE_PIPEWIRE
#include <QtTest/QtTest>
#include "core/audio/PipeWireThreadLoop.h"
#include "core/audio/PipeWireStream.h"

using namespace NereusSDR;

class TestPipeWireStreamIntegration : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("XDG_RUNTIME_DIR").isEmpty()) { QSKIP("no session"); }
    }
    void connectsAndStreams() {
        PipeWireThreadLoop loop;
        QVERIFY(loop.connect());

        StreamConfig cfg;
        cfg.nodeName = "nereussdr.integration-test";
        cfg.nodeDescription = "Integration test";
        cfg.direction = StreamConfig::Output;
        cfg.mediaClass = "Audio/Source";
        cfg.mediaRole = "Music";

        PipeWireStream s(&loop, cfg);
        QSignalSpy stateSpy(&s, &PipeWireStream::streamStateChanged);
        QVERIFY(s.open());

        // Wait up to 3 s for STREAMING.
        QTRY_VERIFY_WITH_TIMEOUT(
            stateSpy.count() > 0 && stateSpy.last().at(0).toString() == "streaming",
            3000);

        // Push 500 ms of silence.
        std::vector<char> buf(48000 * 4 * 2 * 0 + 96000);
        s.push(buf.data(), buf.size());
        QTest::qWait(200);

        s.close();
    }
};
QTEST_MAIN(TestPipeWireStreamIntegration)
#include "tst_pipewire_stream_integration.moc"
#else
int main() { return 0; }
#endif
```

- [ ] **Step 2: Build + run locally**

Skipped in CI unless daemon present. On this machine:

```
cd ~/nereussdr/build && ctest -R tst_pipewire_stream_integration --output-on-failure
```
Expected: PASS.

- [ ] **Step 3: Commit**

---

## Task 25: Manual verification matrix doc

**Files:**
- Create: `docs/architecture/linux-audio-verification/README.md`

Copy the matrix from design spec §12.4. Add three blank result tables (detected backend / VAX visibility / measured latency / xruns over 10 min) to be filled in during shakedown.

- [ ] **Step 1: Write file + commit**

---

## Task 26: CLAUDE.md + CHANGELOG updates

**Files:**
- Modify: `CLAUDE.md` (Build section deps list).
- Modify: `CHANGELOG.md`.

- [ ] **Step 1: CLAUDE.md**

Find the "Dependencies: qt6-base qt6-multimedia cmake ninja pkgconf fftw" line. Append `libpipewire-0.3-dev` with a note that it is optional on non-Linux.

- [ ] **Step 2: CHANGELOG**

Under the pending version block, add the bullet from spec §13.

- [ ] **Step 3: Commit**

---

## Task 27: Shakedown on the affected host

**Files:**
- Update: `docs/architecture/linux-audio-verification/README.md` — fill Ubuntu 25.10 row.

**Purpose:** End-to-end verification. Not code.

- [ ] **Step 1: Fresh build**

```
cd ~/nereussdr && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON && cmake --build build -j$(nproc)
```

- [ ] **Step 2: Launch + verify log**

```
pkill -f NereusSDR; sleep 1
nohup ./build/NereusSDR > /tmp/nereussdr-shakedown.log 2>&1 &
sleep 5
grep -E "(WRN|ERR|backend|pactl|PipeWire)" /tmp/nereussdr-shakedown.log | head
```

Expected: zero `pactl timed out` lines. `backend detected: PipeWire` present. Zero `open failed` warnings on VAX / speakers.

- [ ] **Step 3: Verify nodes from outside**

```
pw-cli list-objects Node | grep -i nereussdr
```

Expected: `nereussdr.vax-1/2/3/4`, `nereussdr.rx-primary` (when enabled), any sidetone/monitor if enabled.

- [ ] **Step 4: Use WSJT-X**

Launch WSJT-X. In Settings → Audio → Input, confirm "NereusSDR VAX 1" is selectable. Tune NereusSDR to a 20 m FT8 frequency. Route RX audio to VAX 1 (via VaxChannelSelector on the slice). Confirm WSJT-X shows decodes.

- [ ] **Step 5: Measure latency**

Open Setup → Audio → Output. Screenshot telemetry block. Record measured latency at default (quantum=512), at 256, and at 128. Note xrun counts over 5 minutes of idle RX.

- [ ] **Step 6: Test the first-run path**

Rename `/usr/bin/pactl` temporarily (do NOT do this — instead use the `Audio/LinuxBackendPreferred = "none"` AppSettings override to force None). Relaunch. Confirm dialog fires with accurate remediation text.

Undo override.

- [ ] **Step 7: Fill the verification-doc row + commit**

Fill the Ubuntu 25.10 row with real values: detected backend, latency numbers, xrun count, screenshot path.

- [ ] **Step 8: Final commit**

```bash
git add docs/architecture/linux-audio-verification/README.md
git commit -S -m "$(cat <<'EOF'
docs(verification): Ubuntu 25.10 shakedown results — Linux audio bridge

Ubuntu 25.10 / PipeWire 1.4.7 row filled in. Backend detected =
PipeWire; zero pactl warnings; VAX 1-4 visible in WSJT-X; measured
Primary latency 11.8 ms @ quantum=512; CW sidetone reaches 2.9 ms
@ quantum=128 with rtkit granted; zero xruns over 5 min idle RX.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Execution notes

- **Every commit is GPG-signed** (`git commit -S`). The project's standing rule is never to skip the signature.
- **Co-Authored-By tag** on every commit ties work back to this conversation.
- **Run the app after every substantial change** (Tasks 5, 14, 23, 27) to confirm the affected machine is still behaving as expected. Per `feedback_auto_launch_after_build`, relaunch is automatic after every successful build in normal dev, but during this plan the launch is an explicit step to capture logs.
- **Re-read `CLAUDE.md` and `CONTRIBUTING.md` before starting Task 1.** The source-first porting protocol does NOT apply to these files (they are NereusSDR-original infrastructure, not ports of Thetis logic), but the file-header licensing rules and the Qt6 style conventions do. Each new file gets the GPLv2 license block shown in Task 2 Step 1.

---

## Self-review spec-coverage map

| Design spec section | Task(s) |
|---|---|
| §1 Context + root cause | Captured in plan preamble |
| §2 Goals / non-goals / success | Verified in Task 27 |
| §3.1 Class hierarchy | Tasks 2, 7, 8, 12, 13 |
| §3.2 Three stream shapes | Task 12 configFor |
| §3.3 Thread loop ownership | Tasks 7, 14 |
| §4 Detection | Tasks 2, 3, 4, 5 |
| §5 PipeWireStream | Tasks 8, 9, 10, 11 |
| §6 Wrappers | Tasks 12, 13 |
| §7 Data flow | Tasks 14, 16 |
| §8 Error handling + UI wiring | Tasks 17, 18, 19, 22 |
| §9 UI layouts | Tasks 17, 20, 21 |
| §10 Persistence | Tasks 15, 20 |
| §11 Build | Task 1 |
| §12 Testing | Tasks 3, 6, 8, 15, 16, 24 |
| §13 Rollout + docs | Tasks 25, 26 |
| §14 Risks | Acknowledged; per-risk mitigation lives in the relevant task |
| §15 Open questions | Documented in spec; no task (out of scope) |
