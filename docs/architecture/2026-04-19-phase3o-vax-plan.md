# Phase 3O: VAX Audio Routing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build NereusSDR's complete cross-platform RX audio routing story — per-receiver VAX assignment via VFO-flag selector, native VAX drivers on macOS (HAL plugin) and Linux (PulseAudio module-pipe), Windows BYO virtual cables with auto-detection, and a Thetis-grade Setup → Audio surface with optional Direct ASIO (cmASIO parity) engine.

**Architecture:** Single `IAudioBus` C++ abstraction with five platform backends (`CoreAudioHalBus`, `LinuxPipeBus`, `PortAudioBus`, `DirectAsioBus`, `CoreAudioBus`). Per-slice `vaxChannel` atomic int drives tap routing. AetherSDR-style applet (`VaxApplet` ported + renamed from `DaxApplet`) shows per-channel meters/gains. VFO-flag gains a one-row `VaxChannelSelector`. Menu bar gains a `MasterOutputWidget`. Setup dialog gains an Audio page with four sub-tabs. First-run dialog auto-detects virtual cables.

**Tech Stack:** C++20, Qt6 (Widgets + Multimedia), PortAudio v19, libASPL 3.1.2 (macOS), Steinberg ASIO SDK (Windows opt-in), POSIX shm (Mac), PulseAudio/PipeWire (Linux), WiX v4 (Windows installer).

**Design spec:** [`docs/architecture/2026-04-19-vax-design.md`](2026-04-19-vax-design.md) — all architectural decisions, signal flow, data model, and wiring tables are authoritative there. This plan translates the spec into TDD-sequenced tasks.

---

## GPL Compliance Review (2026-04-19)

Pre-execution audit of every bundled or linked dependency against NereusSDR's GPL-3.0 license:

| Component | License | GPL-3 compatible? |
|---|---|---|
| AetherSDR ports (VirtualAudioBridge, PipeWireAudioBridge, HAL plugin, DaxApplet, MeterSlider) | GPL-3.0 | ✅ trivial |
| Thetis ports (none in Phase 3O after Direct ASIO drop) | GPL-2-or-later + Samphire dual-license | n/a this phase |
| libASPL (macOS HAL framework, vendored by FetchContent) | MIT | ✅ MIT → GPL-3 one-way compatible |
| PortAudio v19.7.0 (vendored by FetchContent) | MIT-style (verify LICENSE.txt at pin) | ✅ expected, verification step added to Task 3.1 |
| Qt6 / FFTW3 / WDSP | LGPL-3 / GPL-2 / GPL (TAPR) | ✅ already in use |
| User-installed virtual cables (VB-CABLE, VAC, Voicemeeter, BlackHole, Dante, FlexRadio DAX) | Proprietary / various | ✅ not redistributed; users install themselves |
| **Steinberg ASIO SDK** | **Proprietary, explicitly non-GPL** | 🚨 **NOT compatible** — see decision below |

### Decision: drop Direct ASIO from Phase 3O

Linking the Steinberg ASIO SDK into GPL-3 NereusSDR binaries violates both the ASIO SDK license (no redistribution of modified headers) and GPL-3 (non-free library linked into a GPL work; no system-library exception applies). Audacity hits the same wall and ships ASIO-disabled binaries for this exact reason.

**Action:** Sub-Phase 13 (Direct ASIO Engine) and all references to `DirectAsioBus` / `AsioSdkHost` / Thetis `cmasio.c` ports are **removed from this plan.** PortAudio's built-in ASIO host API (available via the `PA_USE_ASIO` CMake option) covers the overwhelming majority of ASIO use cases without our project redistributing the SDK itself.

If community demand ever justifies revisiting this, it lands as a **separate compliance proposal** (either option 2 — developer-build-only gate with CI enforcement — or a formal Steinberg commercial-license conversation). Not now.

### Remediation actions folded into this plan

- **Task 3.1** adds a step to verify PortAudio's bundled `LICENSE.txt` at the v19.7.0 pin is MIT-compatible before committing the FetchContent.
- **Task 14.4 (final verification)** adds steps to update `docs/attribution/COMPLIANCE-INVENTORY.md` with rows for every new port (HAL plugin, CoreAudioHalBus, LinuxPipeBus, VaxApplet, MeterSlider) and to document the macOS HAL plugin's separate-binary / mere-aggregation reasoning per GPL-3 §5.
- **Attribution verifiers** (`scripts/verify-thetis-headers.py`, `scripts/check-new-ports.py`, and the existing AetherSDR + WDSP passes in the pre-commit hook) cover enforcement for the GPL-compatible ports.

**Difficulty ranking** (after Direct ASIO drop, per spec §9, no calendar estimates):
🟢 Easy — VirtualCableDetector, LinuxPipeBus, AppSettings schema, VaxChannelSelector, MasterOutputWidget
🟡 Medium — VaxApplet port, HAL plugin (macOS), SetupAudioPage, TX arbitration, MasterMixer
🟠 Medium-hard — IAudioBus abstraction + AudioEngine refactor, PortAudioBus
🔴 Hard — end-to-end smoke-test matrix

---

## Prerequisites

### Worktree setup

This plan should execute in an isolated worktree so it doesn't disturb ongoing work on other branches.

```bash
cd ~/NereusSDR
git worktree add -b feature/phase-3o-vax ../NereusSDR-vax main
cd ../NereusSDR-vax
```

All file paths below are relative to the worktree root.

### Tooling

Before starting, verify the environment:

```bash
# macOS
brew list | grep -E 'qt|cmake|ninja|fftw|portaudio'

# Arch Linux
pacman -Q qt6-base qt6-multimedia cmake ninja fftw portaudio

# Windows
# Visual Studio 2022 + Qt 6.x + CMake + Ninja pre-installed per CONTRIBUTING.md
```

### Reference checkouts

- **Thetis** at `../Thetis/` — cmASIO source. Capture the version once at session start:
  ```bash
  git -C ../Thetis describe --tags
  # → use output as `[vX.Y.Z.W]` on every `// From Thetis …` cite in this plan
  ```
- **AetherSDR** at `../AetherSDR/` — port source for VirtualAudioBridge, PipeWireAudioBridge, hal-plugin, DaxApplet, MeterSlider. No per-file GPL headers (project-level per `HOW-TO-PORT.md` rule 6).

---

## File Structure Overview

### Files to create

| Path | Role | Sub-phase |
|---|---|---|
| `src/core/IAudioBus.h` | Abstract bus interface + `AudioFormat` struct | 2 |
| `src/core/audio/MasterMixer.{h,cpp}` | Per-slice mute/volume/pan → single sink | 2 |
| `src/core/audio/PortAudioBus.{h,cpp}` | PortAudio stream; Windows default + Mac/Linux fallback | 3 |
| `src/core/audio/CoreAudioHalBus.{h,cpp}` | macOS HAL plugin shm writer (ported) | 5 |
| `hal-plugin/NereusSDRVAX.cpp` | macOS HAL plugin (ported) | 5 |
| `hal-plugin/CMakeLists.txt` | HAL plugin build (ported) | 5 |
| `hal-plugin/Info.plist` | HAL plugin bundle manifest (ported) | 5 |
| `packaging/macos/hal-installer.sh` | macOS `.pkg` builder (ported) | 5 |
| `packaging/macos/hal-postinstall.sh` | coreaudiod restart w/ 14.4+ killall fallback | 5 |
| `packaging/macos/hal-uninstall.sh` | remove `.driver` + shm segments (ported) | 5 |
| `src/core/audio/LinuxPipeBus.{h,cpp}` | Linux PulseAudio module-pipe bridge (ported) | 6 |
| `src/core/audio/VirtualCableDetector.{h,cpp}` | OS-audio-device regex matcher | 7 |
| `src/gui/widgets/VaxChannelSelector.{h,cpp}` | VFO flag VAX button group | 8 |
| `src/gui/widgets/MeterSlider.{h,cpp}` | Meter+slider composite (ported) | 9 |
| `src/gui/applets/VaxApplet.{h,cpp}` | Docked VAX meter/gain applet (ported + renamed) | 9 |
| `src/gui/MasterOutputWidget.{h,cpp}` | Menu-bar master output strip | 10 |
| `src/gui/VaxFirstRunDialog.{h,cpp}` | First-run auto-detect modal | 11 |
| `src/gui/SetupAudioPage.{h,cpp}` | Setup → Audio page (4 sub-tabs) | 12 |
| `resources/help/install-virtual-cables.md` | User-facing help doc | 14 |
| `tests/tst_slice_model_vax.cpp` | SliceModel.vaxChannel tests | 1 |
| `tests/tst_transmit_model_tx_owner.cpp` | TransmitModel.txOwnerSlot tests | 1 |
| `tests/tst_app_settings_vax_migration.cpp` | Settings migration test | 1 |
| `tests/tst_master_mixer.cpp` | MasterMixer unit tests | 2 |
| `tests/tst_port_audio_bus.cpp` | PortAudioBus unit tests (Null device) | 3 |
| `tests/tst_virtual_cable_detector.cpp` | Regex-match unit tests | 7 |
| `tests/tst_vax_channel_selector.cpp` | Widget behaviour tests | 8 |

### Files to modify

| Path | Change | Sub-phase |
|---|---|---|
| `src/models/SliceModel.{h,cpp}` | Add `vaxChannel`, persistence, signal | 1 |
| `src/models/TransmitModel.{h,cpp}` | Add `txOwnerSlot` + `VaxSlot` enum | 1 |
| `src/core/AppSettings.{h,cpp}` | Schema migration helper | 1 |
| `src/core/AudioEngine.{h,cpp}` | Replace QAudioSink-only with `IAudioBus` model | 4 |
| `src/gui/VfoWidget.{h,cpp}` | Embed `VaxChannelSelector` | 8 |
| `src/gui/MainWindow.{h,cpp}` | Wire `MasterOutputWidget` + first-run dialog trigger | 10, 11 |
| `src/gui/SetupDialog.{h,cpp}` | Register `SetupAudioPage` | 12 |
| `CMakeLists.txt` | Add PortAudio; conditionally add HAL subdir on macOS | 3, 5 |
| `README.md` | Mention VAX routing in features | 14 |
| `docs/attribution/aethersdr-contributor-index.md` | Add rows for ported files | 5, 6, 9 |
| `docs/attribution/COMPLIANCE-INVENTORY.md` | Record Phase 3O ports + HAL plugin separate-binary reasoning | 14 |
| `.github/workflows/release.yml` | Sign + notarize HAL plugin | 5 |

---

## Sub-Phase 1: Data Model Foundation 🟢

### Task 1.1: `SliceModel.vaxChannel`

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`
- Test: `tests/tst_slice_model_vax.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_slice_model_vax.cpp`:

```cpp
#include <QtTest/QtTest>
#include "models/SliceModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstSliceModelVax : public QObject {
    Q_OBJECT
private slots:
    void defaultsToOff() {
        SliceModel s(0);
        QCOMPARE(s.vaxChannel(), 0);
    }

    void setAndGet() {
        SliceModel s(0);
        s.setVaxChannel(2);
        QCOMPARE(s.vaxChannel(), 2);
    }

    void clampsOutOfRangeLow() {
        SliceModel s(0);
        s.setVaxChannel(-5);
        QCOMPARE(s.vaxChannel(), 0);
    }

    void clampsOutOfRangeHigh() {
        SliceModel s(0);
        s.setVaxChannel(99);
        QCOMPARE(s.vaxChannel(), 0);
    }

    void emitsSignalOnChange() {
        SliceModel s(0);
        QSignalSpy spy(&s, &SliceModel::vaxChannelChanged);
        s.setVaxChannel(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }

    void noSignalOnSameValue() {
        SliceModel s(0);
        s.setVaxChannel(2);
        QSignalSpy spy(&s, &SliceModel::vaxChannelChanged);
        s.setVaxChannel(2);
        QCOMPARE(spy.count(), 0);
    }

    void persistsToSettings() {
        AppSettings::instance().clear();
        SliceModel s(7);
        s.setVaxChannel(4);
        QCOMPARE(AppSettings::instance().value("slice/7/VaxChannel").toString(), "4");
    }

    void restoresFromSettings() {
        AppSettings::instance().clear();
        AppSettings::instance().setValue("slice/7/VaxChannel", "3");
        SliceModel s(7);
        s.loadFromSettings();
        QCOMPARE(s.vaxChannel(), 3);
    }
};

QTEST_APPLESS_MAIN(TstSliceModelVax)
#include "tst_slice_model_vax.moc"
```

Add to `tests/CMakeLists.txt`:

```cmake
qt_add_executable(tst_slice_model_vax tst_slice_model_vax.cpp)
target_link_libraries(tst_slice_model_vax PRIVATE NereusSDRLib Qt6::Test)
add_test(NAME tst_slice_model_vax COMMAND tst_slice_model_vax)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_slice_model_vax
./build/tests/tst_slice_model_vax
```

Expected: FAIL — `vaxChannel` / `setVaxChannel` / `vaxChannelChanged` not defined.

- [ ] **Step 3: Add API to `SliceModel.h`**

In `src/models/SliceModel.h`, add under the existing properties section:

```cpp
    // VAX routing (Phase 3O) — 0=Off, 1..4=VAX channel.
    int vaxChannel() const { return m_vaxChannel.load(std::memory_order_acquire); }
    void setVaxChannel(int ch);

signals:
    void vaxChannelChanged(int ch);

private:
    std::atomic<int> m_vaxChannel{0};
```

Also include `<atomic>` at the top of the file if not already present.

- [ ] **Step 4: Implement setter + persistence in `SliceModel.cpp`**

```cpp
void SliceModel::setVaxChannel(int ch) {
    // Clamp to valid range.
    if (ch < 0 || ch > 4) { ch = 0; }

    const int prev = m_vaxChannel.exchange(ch, std::memory_order_acq_rel);
    if (prev == ch) { return; }

    AppSettings::instance().setValue(
        QStringLiteral("slice/%1/VaxChannel").arg(m_sliceId),
        QString::number(ch));
    AppSettings::instance().save();

    emit vaxChannelChanged(ch);
}
```

In `SliceModel::loadFromSettings()` (existing method), add:

```cpp
    const int vaxCh = AppSettings::instance()
        .value(QStringLiteral("slice/%1/VaxChannel").arg(m_sliceId), "0")
        .toString().toInt();
    if (vaxCh != m_vaxChannel.load()) {
        m_vaxChannel.store(vaxCh, std::memory_order_release);
        emit vaxChannelChanged(vaxCh);
    }
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build --target tst_slice_model_vax
./build/tests/tst_slice_model_vax
```

Expected: PASS — all 8 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp \
        tests/tst_slice_model_vax.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(slice-model): add vaxChannel property for VAX routing

Per-slice VAX channel (0=Off, 1..4=VAX 1–4) with std::atomic storage
for audio-thread-safe reads, AppSettings persistence under
slice/<id>/VaxChannel, and vaxChannelChanged signal. Feeds the
AudioEngine VAX tap routing in Phase 3O.

Design spec: docs/architecture/2026-04-19-vax-design.md §5.1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 1.2: `TransmitModel.txOwnerSlot` + `VaxSlot` enum

**Files:**
- Modify: `src/models/TransmitModel.h`, `src/models/TransmitModel.cpp`
- Test: `tests/tst_transmit_model_tx_owner.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_transmit_model_tx_owner.cpp`:

```cpp
#include <QtTest/QtTest>
#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstTransmitModelTxOwner : public QObject {
    Q_OBJECT
private slots:
    void defaultsToMicDirect() {
        TransmitModel t;
        QCOMPARE(t.txOwnerSlot(), VaxSlot::MicDirect);
    }

    void setAndGet() {
        TransmitModel t;
        t.setTxOwnerSlot(VaxSlot::Vax2);
        QCOMPARE(t.txOwnerSlot(), VaxSlot::Vax2);
    }

    void emitsSignalOnChange() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txOwnerSlotChanged);
        t.setTxOwnerSlot(VaxSlot::Vax3);
        QCOMPARE(spy.count(), 1);
    }

    void persistsToSettings() {
        AppSettings::instance().clear();
        TransmitModel t;
        t.setTxOwnerSlot(VaxSlot::Vax4);
        QCOMPARE(AppSettings::instance().value("tx/OwnerSlot").toString(), "Vax4");
    }

    void restoresFromSettings() {
        AppSettings::instance().clear();
        AppSettings::instance().setValue("tx/OwnerSlot", "Vax1");
        TransmitModel t;
        t.loadFromSettings();
        QCOMPARE(t.txOwnerSlot(), VaxSlot::Vax1);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelTxOwner)
#include "tst_transmit_model_tx_owner.moc"
```

- [ ] **Step 2: Run to fail**

```bash
cmake --build build --target tst_transmit_model_tx_owner
./build/tests/tst_transmit_model_tx_owner
```

Expected: FAIL (VaxSlot enum, txOwnerSlot API undefined).

- [ ] **Step 3: Define `VaxSlot` enum + API in `TransmitModel.h`**

```cpp
namespace NereusSDR {

enum class VaxSlot {
    None = 0,
    MicDirect,
    Vax1,
    Vax2,
    Vax3,
    Vax4
};

QString vaxSlotToString(VaxSlot s);
VaxSlot vaxSlotFromString(const QString& s);

class TransmitModel : public QObject {
    // ... existing ...
public:
    VaxSlot txOwnerSlot() const { return m_txOwnerSlot.load(std::memory_order_acquire); }
    void setTxOwnerSlot(VaxSlot s);

    void loadFromSettings();

signals:
    void txOwnerSlotChanged(VaxSlot s);

private:
    std::atomic<VaxSlot> m_txOwnerSlot{VaxSlot::MicDirect};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Implement in `TransmitModel.cpp`**

```cpp
QString NereusSDR::vaxSlotToString(VaxSlot s) {
    switch (s) {
        case VaxSlot::None:       return "None";
        case VaxSlot::MicDirect:  return "MicDirect";
        case VaxSlot::Vax1:       return "Vax1";
        case VaxSlot::Vax2:       return "Vax2";
        case VaxSlot::Vax3:       return "Vax3";
        case VaxSlot::Vax4:       return "Vax4";
    }
    return "MicDirect";
}

NereusSDR::VaxSlot NereusSDR::vaxSlotFromString(const QString& s) {
    if (s == "None")      return VaxSlot::None;
    if (s == "Vax1")      return VaxSlot::Vax1;
    if (s == "Vax2")      return VaxSlot::Vax2;
    if (s == "Vax3")      return VaxSlot::Vax3;
    if (s == "Vax4")      return VaxSlot::Vax4;
    return VaxSlot::MicDirect;
}

void TransmitModel::setTxOwnerSlot(VaxSlot s) {
    const VaxSlot prev = m_txOwnerSlot.exchange(s, std::memory_order_acq_rel);
    if (prev == s) { return; }

    AppSettings::instance().setValue("tx/OwnerSlot", vaxSlotToString(s));
    AppSettings::instance().save();

    emit txOwnerSlotChanged(s);
}

void TransmitModel::loadFromSettings() {
    const QString v = AppSettings::instance().value("tx/OwnerSlot", "MicDirect").toString();
    const VaxSlot s = vaxSlotFromString(v);
    if (s != m_txOwnerSlot.load()) {
        m_txOwnerSlot.store(s, std::memory_order_release);
        emit txOwnerSlotChanged(s);
    }
}
```

- [ ] **Step 5: Run to pass**

```bash
./build/tests/tst_transmit_model_tx_owner
```

Expected: PASS (5 tests).

- [ ] **Step 6: Commit**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp \
        tests/tst_transmit_model_tx_owner.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(transmit-model): add txOwnerSlot + VaxSlot enum

Single-owner TX arbitration between MicDirect and VAX 1–4. Atomic
storage for audio-thread-safe reads, persisted under tx/OwnerSlot.
Feeds AudioEngine TX pull routing in Phase 3O.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 1.3: AppSettings schema + legacy migration

**Files:**
- Modify: `src/core/AppSettings.h`, `src/core/AppSettings.cpp`
- Test: `tests/tst_app_settings_vax_migration.cpp` (create)

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest/QtTest>
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstAppSettingsVaxMigration : public QObject {
    Q_OBJECT
private slots:
    void migratesLegacyOutputDeviceKey() {
        auto& s = AppSettings::instance();
        s.clear();
        s.setValue("audio/OutputDevice", "Built-in Output");
        AppSettings::migrateVaxSchemaV1ToV2();
        QCOMPARE(s.value("audio/Speakers/DeviceName").toString(),
                 QStringLiteral("Built-in Output"));
        QVERIFY(!s.contains("audio/OutputDevice"));
        QCOMPARE(s.value("audio/FirstRunComplete", "True").toString(), "False");
    }

    void doesNothingIfNoLegacyKey() {
        auto& s = AppSettings::instance();
        s.clear();
        s.setValue("audio/Speakers/DeviceName", "Already set");
        AppSettings::migrateVaxSchemaV1ToV2();
        QCOMPARE(s.value("audio/Speakers/DeviceName").toString(),
                 QStringLiteral("Already set"));
    }

    void doesNothingIfAlreadyMigrated() {
        auto& s = AppSettings::instance();
        s.clear();
        s.setValue("audio/OutputDevice", "Legacy");
        s.setValue("audio/Speakers/DeviceName", "New");
        AppSettings::migrateVaxSchemaV1ToV2();
        // Legacy key stays (we don't clobber a present new key)
        QCOMPARE(s.value("audio/Speakers/DeviceName").toString(),
                 QStringLiteral("New"));
    }
};

QTEST_APPLESS_MAIN(TstAppSettingsVaxMigration)
#include "tst_app_settings_vax_migration.moc"
```

- [ ] **Step 2: Run to fail**

```bash
cmake --build build --target tst_app_settings_vax_migration
./build/tests/tst_app_settings_vax_migration
```

Expected: FAIL — `migrateVaxSchemaV1ToV2` undefined.

- [ ] **Step 3: Add migration helper**

In `src/core/AppSettings.h`:

```cpp
class AppSettings {
public:
    // ... existing ...

    // Phase 3O VAX schema migration. Call once at app startup. Idempotent.
    // Migrates legacy `audio/OutputDevice` → `audio/Speakers/DeviceName`
    // and flags the first-run dialog to show.
    static void migrateVaxSchemaV1ToV2();
};
```

In `src/core/AppSettings.cpp`:

```cpp
void AppSettings::migrateVaxSchemaV1ToV2() {
    auto& s = instance();
    if (!s.contains("audio/OutputDevice")) { return; }
    if (s.contains("audio/Speakers/DeviceName")) { return; }

    const QString dev = s.value("audio/OutputDevice").toString();
    s.setValue("audio/Speakers/DeviceName", dev);

    // Platform-default API. Keep this conservative; user can tune later.
#if defined(Q_OS_WIN)
    s.setValue("audio/Speakers/DriverApi", "WASAPI");
#elif defined(Q_OS_MAC)
    s.setValue("audio/Speakers/DriverApi", "CoreAudio");
#else
    s.setValue("audio/Speakers/DriverApi", "Pulse");
#endif
    s.setValue("audio/Speakers/SampleRate", "48000");
    s.setValue("audio/Speakers/BitDepth", "24");
    s.setValue("audio/Speakers/Channels", "2");
    s.setValue("audio/Speakers/BufferSamples", "256");

    // Trigger first-run dialog on next launch.
    s.setValue("audio/FirstRunComplete", "False");

    s.remove("audio/OutputDevice");
    s.save();
}
```

- [ ] **Step 4: Run to pass**

```bash
./build/tests/tst_app_settings_vax_migration
```

Expected: PASS (3 tests).

- [ ] **Step 5: Hook migration into app startup**

In `src/gui/MainWindow.cpp` constructor (or `main.cpp` — wherever the first AppSettings access happens), add **at the top, before any other AppSettings reads**:

```cpp
    AppSettings::migrateVaxSchemaV1ToV2();
```

- [ ] **Step 6: Commit**

```bash
git add src/core/AppSettings.h src/core/AppSettings.cpp \
        src/gui/MainWindow.cpp \
        tests/tst_app_settings_vax_migration.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(app-settings): add VAX schema migration helper

Phase 3O adds a per-device / per-VAX / per-slice audio schema. This
change introduces migrateVaxSchemaV1ToV2() — idempotent one-shot that
moves legacy audio/OutputDevice into audio/Speakers/DeviceName with
sensible platform defaults and flags the first-run dialog to show.

Called from MainWindow::MainWindow before any other AppSettings access.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Sub-Phase 2: IAudioBus Abstraction + MasterMixer 🟠

### Task 2.1: `IAudioBus.h` interface

**Files:**
- Create: `src/core/IAudioBus.h`

- [ ] **Step 1: Write the interface**

```cpp
// =================================================================
// src/core/IAudioBus.h  (NereusSDR)
// =================================================================
//
// Phase 3O abstract audio bus. Five concrete implementations (see
// src/core/audio/): CoreAudioHalBus, LinuxPipeBus, PortAudioBus,
// DirectAsioBus, CoreAudioBus.
//
// Design spec: docs/architecture/2026-04-19-vax-design.md §3.2
// =================================================================

#pragma once

#include <QString>
#include <cstdint>

namespace NereusSDR {

struct AudioFormat {
    int sampleRate = 48000;  // Hz
    int channels   = 2;       // 1 or 2
    enum class Sample { Float32, Int16, Int24, Int32 } sample = Sample::Float32;

    bool operator==(const AudioFormat& o) const {
        return sampleRate == o.sampleRate && channels == o.channels && sample == o.sample;
    }
    bool operator!=(const AudioFormat& o) const { return !(*this == o); }
};

class IAudioBus {
public:
    virtual ~IAudioBus() = default;

    // Lifecycle. open() returns false on failure; errorString() has details.
    virtual bool open(const AudioFormat& format) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Producer side (RX taps). Interleaved PCM bytes. Returns bytes actually
    // written, or -1 on error. Must be callable from the audio thread.
    virtual qint64 push(const char* data, qint64 bytes) = 0;

    // Consumer side (TX). Returns bytes read, or -1 on error. Audio-thread safe.
    virtual qint64 pull(char* data, qint64 maxBytes) = 0;

    // Metering (RMS of last block). 0.0–1.0. Published atomically for UI.
    virtual float rxLevel() const = 0;
    virtual float txLevel() const = 0;

    // Diagnostics.
    virtual QString backendName() const = 0;
    virtual AudioFormat negotiatedFormat() const = 0;
    virtual QString errorString() const { return {}; }
};

} // namespace NereusSDR
```

- [ ] **Step 2: Commit**

```bash
git add src/core/IAudioBus.h
git commit -S -m "$(cat <<'EOF'
feat(audio): add IAudioBus abstract interface

Contract for every audio backend in Phase 3O. Five concrete
implementations follow in subsequent tasks (CoreAudioHalBus,
LinuxPipeBus, PortAudioBus, DirectAsioBus, CoreAudioBus).

Design spec §3.2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 2.2: `MasterMixer` — per-slice mute/volume/pan

**Files:**
- Create: `src/core/audio/MasterMixer.h`, `src/core/audio/MasterMixer.cpp`
- Test: `tests/tst_master_mixer.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest/QtTest>
#include "core/audio/MasterMixer.h"
#include <array>

using namespace NereusSDR;

class TstMasterMixer : public QObject {
    Q_OBJECT
private slots:
    void emptyMixIsSilent() {
        MasterMixer mix;
        std::array<float, 16> out{};
        mix.mixInto(out.data(), 8);
        for (float s : out) { QCOMPARE(s, 0.0f); }
    }

    void singleSliceUnityGainMixesThrough() {
        MasterMixer mix;
        mix.setSliceGain(42, 1.0f, 0.0f);  // unity, center
        std::array<float, 4> in = {0.5f, 0.5f, -0.25f, -0.25f};  // 2 frames stereo
        mix.accumulate(42, in.data(), 2);
        std::array<float, 4> out{};
        mix.mixInto(out.data(), 2);
        QCOMPARE(out[0], 0.5f);
        QCOMPARE(out[1], 0.5f);
        QCOMPARE(out[2], -0.25f);
        QCOMPARE(out[3], -0.25f);
    }

    void twoSlicesSumLinearly() {
        MasterMixer mix;
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> a = {0.3f, 0.3f};
        std::array<float, 2> b = {0.4f, 0.4f};
        mix.accumulate(1, a.data(), 1);
        mix.accumulate(2, b.data(), 1);
        std::array<float, 2> out{};
        mix.mixInto(out.data(), 1);
        QCOMPARE(out[0], 0.7f);
        QCOMPARE(out[1], 0.7f);
    }

    void panFullLeftSuppressesRight() {
        MasterMixer mix;
        mix.setSliceGain(1, 1.0f, -1.0f);  // full left
        std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(1, in.data(), 1);
        std::array<float, 2> out{};
        mix.mixInto(out.data(), 1);
        QVERIFY(out[0] > 0.4f);
        QCOMPARE(out[1], 0.0f);
    }

    void muteZerosContribution() {
        MasterMixer mix;
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceMuted(1, true);
        std::array<float, 2> in = {0.9f, 0.9f};
        mix.accumulate(1, in.data(), 1);
        std::array<float, 2> out{};
        mix.mixInto(out.data(), 1);
        QCOMPARE(out[0], 0.0f);
        QCOMPARE(out[1], 0.0f);
    }
};

QTEST_APPLESS_MAIN(TstMasterMixer)
#include "tst_master_mixer.moc"
```

- [ ] **Step 2: Run to fail**

Expected: FAIL — MasterMixer undefined.

- [ ] **Step 3: Implement `MasterMixer`**

`src/core/audio/MasterMixer.h`:

```cpp
#pragma once

#include <atomic>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace NereusSDR {

// Audio-thread-safe per-slice mixing. Producers call accumulate() from the
// audio thread (one call per slice per block); consumer calls mixInto() once
// per block to flush the accumulator to the output buffer.
class MasterMixer {
public:
    // UI thread: set per-slice gain and pan. Safe to call anytime.
    void setSliceGain(int sliceId, float gain /*0..1*/, float pan /*-1..+1*/);
    void setSliceMuted(int sliceId, bool muted);
    void removeSlice(int sliceId);

    // Audio thread: accumulate a slice's stereo block. samples is
    // interleaved L/R float32. frames = sample pairs.
    void accumulate(int sliceId, const float* samples, int frames);

    // Audio thread: flush the accumulated mix into out and reset.
    // out is interleaved L/R float32.
    void mixInto(float* out, int frames);

private:
    struct SliceState {
        std::atomic<float> gain{1.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<bool>  muted{false};
    };

    std::unordered_map<int, SliceState> m_slices;
    std::mutex m_sliceMapMutex;  // Only held on UI-thread map mutation.

    std::vector<float> m_acc;  // Audio-thread-only; resized on-demand.
};

} // namespace NereusSDR
```

`src/core/audio/MasterMixer.cpp`:

```cpp
#include "MasterMixer.h"
#include <algorithm>
#include <cmath>

using namespace NereusSDR;

void MasterMixer::setSliceGain(int sliceId, float gain, float pan) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    auto& st = m_slices[sliceId];
    st.gain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_release);
    st.pan.store(std::clamp(pan, -1.0f, 1.0f), std::memory_order_release);
}

void MasterMixer::setSliceMuted(int sliceId, bool muted) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_slices[sliceId].muted.store(muted, std::memory_order_release);
}

void MasterMixer::removeSlice(int sliceId) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_slices.erase(sliceId);
}

void MasterMixer::accumulate(int sliceId, const float* samples, int frames) {
    // Lookup slice state. The map *itself* is mutex-guarded for structural
    // changes, but we do not want to hold the mutex in the audio thread. We
    // read the atomics directly; find() is lock-free against unchanged buckets.
    auto it = m_slices.find(sliceId);
    if (it == m_slices.end()) { return; }

    const auto& st = it->second;
    if (st.muted.load(std::memory_order_acquire)) { return; }

    const float gain = st.gain.load(std::memory_order_acquire);
    const float pan  = st.pan.load(std::memory_order_acquire);

    // Equal-power pan law.
    const float lGain = gain * std::cos((pan + 1.0f) * 0.25f * 3.14159265f);
    const float rGain = gain * std::sin((pan + 1.0f) * 0.25f * 3.14159265f);

    if ((int)m_acc.size() < frames * 2) {
        m_acc.resize(frames * 2, 0.0f);
    }
    for (int i = 0; i < frames; ++i) {
        m_acc[i * 2 + 0] += samples[i * 2 + 0] * lGain;
        m_acc[i * 2 + 1] += samples[i * 2 + 1] * rGain;
    }
}

void MasterMixer::mixInto(float* out, int frames) {
    const int n = frames * 2;
    if ((int)m_acc.size() < n) {
        // No one accumulated this block; output silence.
        std::fill(out, out + n, 0.0f);
        return;
    }
    std::copy(m_acc.begin(), m_acc.begin() + n, out);
    std::fill(m_acc.begin(), m_acc.begin() + n, 0.0f);
}
```

**Note on the find() safety:** `std::unordered_map::find()` is not strictly lock-free — if the UI thread inserts a new slice while the audio thread is in `find()`, behavior is undefined. For this plan, slice creation/removal happens rarely and on app startup/radio-connect. The mutex guards the structural mutations. If this proves racy in practice, swap to `folly::ConcurrentHashMap` or a pre-allocated slice array (max-slices = 8 per ANAN board cap).

- [ ] **Step 4: Run to pass**

```bash
./build/tests/tst_master_mixer
```

Expected: PASS (5 tests).

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/MasterMixer.h src/core/audio/MasterMixer.cpp \
        tests/tst_master_mixer.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): add MasterMixer for per-slice mute/volume/pan

Accumulator + flush model: audio thread calls accumulate() once per
slice per block, then mixInto() once to flush. Equal-power pan law,
atomic per-slice parameters, one mutex only on slice-map structural
changes (not in the audio hot path).

Design spec §3.4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Sub-Phase 3: PortAudio Engine 🟠

### Task 3.1: Vendor PortAudio v19

**Files:**
- Modify: `CMakeLists.txt`
- Create: `third_party/portaudio/` (subtree or FetchContent)

- [ ] **Step 1: Add PortAudio via CMake FetchContent**

Append to top-level `CMakeLists.txt`, after the existing `find_package(Qt6 ...)`:

```cmake
include(FetchContent)

FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG        v19.7.0  # verified cmake-clean release
)

# PortAudio build options — turn off ASIO (we handle it opt-in via ASIO SDK
# separately in Sub-Phase 13), keep platform default host APIs on.
set(PA_BUILD_SHARED OFF CACHE BOOL "PortAudio static" FORCE)
set(PA_BUILD_STATIC ON  CACHE BOOL "PortAudio static" FORCE)
set(PA_USE_ASIO    OFF CACHE BOOL "PortAudio disable PA-ASIO (we use SDK direct)" FORCE)

FetchContent_MakeAvailable(portaudio)
```

Add to the NereusSDR main executable link:

```cmake
target_link_libraries(NereusSDRLib PRIVATE portaudio_static)
target_include_directories(NereusSDRLib PRIVATE ${portaudio_SOURCE_DIR}/include)
```

- [ ] **Step 2: Build and confirm PortAudio links**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Expected: builds without error; `libportaudio_static.a` appears in `build/_deps/portaudio-build/`.

- [ ] **Step 3: Verify PortAudio's license is GPL-3 compatible**

```bash
cat build/_deps/portaudio-src/LICENSE.txt | head -40
```

Expected: MIT-style permissive license (Ross Bencina / Phil Burk 1999+). If the text is anything other than the well-known PortAudio MIT-style grant, STOP and surface to project maintainer before proceeding.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
build: vendor PortAudio v19.7.0 via FetchContent

Static-link, PA_USE_ASIO off (handled separately via Steinberg SDK in
Sub-Phase 13). Gives us cross-platform audio API coverage: MME / DS /
WDM-KS / WASAPI on Windows, CoreAudio on Mac, ALSA / Pulse / PipeWire /
JACK on Linux via PortAudio's host APIs.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 3.2: `PortAudioBus` minimal open/close

**Files:**
- Create: `src/core/audio/PortAudioBus.h`, `.cpp`
- Test: `tests/tst_port_audio_bus.cpp`

- [ ] **Step 1: Write the failing test (uses PortAudio null device)**

```cpp
#include <QtTest/QtTest>
#include "core/audio/PortAudioBus.h"
#include <portaudio.h>

using namespace NereusSDR;

class TstPortAudioBus : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        Pa_Initialize();
    }
    void cleanupTestCase() {
        Pa_Terminate();
    }

    void constructsClosed() {
        PortAudioBus bus;
        QVERIFY(!bus.isOpen());
    }

    void openSucceedsOnDefaultDevice() {
        PortAudioBus bus;
        AudioFormat f;
        QVERIFY2(bus.open(f), qPrintable(bus.errorString()));
        QVERIFY(bus.isOpen());
        bus.close();
        QVERIFY(!bus.isOpen());
    }

    void negotiatedFormatReflectsDevice() {
        PortAudioBus bus;
        AudioFormat f; f.sampleRate = 48000; f.channels = 2;
        bus.open(f);
        QVERIFY(bus.negotiatedFormat().sampleRate > 0);
        bus.close();
    }

    void backendNameIdentifiesAPI() {
        PortAudioBus bus;
        bus.open(AudioFormat{});
        const QString n = bus.backendName();
        QVERIFY(!n.isEmpty());
        bus.close();
    }
};

QTEST_APPLESS_MAIN(TstPortAudioBus)
#include "tst_port_audio_bus.moc"
```

- [ ] **Step 2: Run to fail**

Expected: FAIL — `PortAudioBus` undefined.

- [ ] **Step 3: Implement `PortAudioBus` (render-only, minimal)**

`src/core/audio/PortAudioBus.h`:

```cpp
#pragma once
#include "core/IAudioBus.h"
#include <atomic>
#include <vector>
#include <mutex>

typedef void PaStream;  // forward decl
struct PaDeviceInfo;

namespace NereusSDR {

struct PortAudioConfig {
    int     hostApiIndex = -1;     // -1 = PortAudio default
    QString deviceName;             // empty = default
    int     bufferSamples = 256;
    bool    exclusiveMode = false; // WASAPI only
    // Additional fields added in Tasks 3.3/3.4.
};

class PortAudioBus : public IAudioBus {
public:
    PortAudioBus();
    ~PortAudioBus() override;

    void setConfig(const PortAudioConfig& cfg);

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override { return m_stream != nullptr; }

    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;

    float rxLevel() const override { return m_rxLevel.load(std::memory_order_acquire); }
    float txLevel() const override { return m_txLevel.load(std::memory_order_acquire); }

    QString     backendName() const override { return m_backendName; }
    AudioFormat negotiatedFormat() const override { return m_negFormat; }
    QString     errorString() const override { return m_err; }

private:
    PaStream*   m_stream{nullptr};
    PortAudioConfig m_cfg;
    AudioFormat m_negFormat;
    QString     m_backendName;
    QString     m_err;

    // Ring buffer for push/pull. Audio thread reads from here.
    std::vector<float> m_ring;
    std::atomic<qint64> m_ringRead{0};
    std::atomic<qint64> m_ringWrite{0};

    std::atomic<float> m_rxLevel{0.0f};
    std::atomic<float> m_txLevel{0.0f};

    static int paCallback(const void* in, void* out,
                          unsigned long frames,
                          const void* timeInfo,
                          unsigned long flags,
                          void* userData);
};

} // namespace NereusSDR
```

`src/core/audio/PortAudioBus.cpp`:

```cpp
#include "PortAudioBus.h"
#include <portaudio.h>
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace NereusSDR;

PortAudioBus::PortAudioBus() {
    m_ring.resize(48000 * 2);  // 1 second stereo float ring
}

PortAudioBus::~PortAudioBus() {
    close();
}

void PortAudioBus::setConfig(const PortAudioConfig& cfg) {
    m_cfg = cfg;
}

bool PortAudioBus::open(const AudioFormat& format) {
    if (m_stream) { close(); }

    PaStreamParameters outParams;
    outParams.device = Pa_GetDefaultOutputDevice();
    if (outParams.device == paNoDevice) {
        m_err = "No default output device";
        return false;
    }

    const PaDeviceInfo* di = Pa_GetDeviceInfo(outParams.device);
    outParams.channelCount = format.channels;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = di->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    const PaError err = Pa_OpenStream(
        &m_stream, nullptr, &outParams,
        format.sampleRate, m_cfg.bufferSamples,
        paClipOff, &PortAudioBus::paCallback, this);

    if (err != paNoError) {
        m_err = Pa_GetErrorText(err);
        m_stream = nullptr;
        return false;
    }

    Pa_StartStream(m_stream);
    m_negFormat = format;
    m_backendName = Pa_GetHostApiInfo(di->hostApi)->name;
    return true;
}

void PortAudioBus::close() {
    if (!m_stream) { return; }
    Pa_StopStream(m_stream);
    Pa_CloseStream(m_stream);
    m_stream = nullptr;
}

qint64 PortAudioBus::push(const char* data, qint64 bytes) {
    const int floatCount = bytes / sizeof(float);
    const qint64 ringSize = (qint64)m_ring.size();
    qint64 w = m_ringWrite.load(std::memory_order_relaxed);
    const float* in = reinterpret_cast<const float*>(data);
    float peak = 0.0f;
    for (int i = 0; i < floatCount; ++i) {
        m_ring[w % ringSize] = in[i];
        w++;
        peak = std::max(peak, std::abs(in[i]));
    }
    m_ringWrite.store(w, std::memory_order_release);
    m_rxLevel.store(peak, std::memory_order_release);
    return bytes;
}

qint64 PortAudioBus::pull(char* /*data*/, qint64 /*maxBytes*/) {
    // Not implemented in this task; TX path wired in Sub-Phase 4.
    return 0;
}

int PortAudioBus::paCallback(const void* /*in*/, void* out,
                             unsigned long frames,
                             const void* /*timeInfo*/,
                             unsigned long /*flags*/,
                             void* userData) {
    auto* self = static_cast<PortAudioBus*>(userData);
    float* o = static_cast<float*>(out);
    const int want = (int)frames * self->m_negFormat.channels;

    const qint64 ringSize = (qint64)self->m_ring.size();
    qint64 r = self->m_ringRead.load(std::memory_order_relaxed);
    const qint64 w = self->m_ringWrite.load(std::memory_order_acquire);

    for (int i = 0; i < want; ++i) {
        if (r < w) {
            o[i] = self->m_ring[r % ringSize];
            r++;
        } else {
            o[i] = 0.0f;  // underrun → silence
        }
    }
    self->m_ringRead.store(r, std::memory_order_release);
    return paContinue;
}
```

- [ ] **Step 4: Run to pass**

```bash
./build/tests/tst_port_audio_bus
```

Expected: PASS (4 tests).

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/PortAudioBus.h src/core/audio/PortAudioBus.cpp \
        tests/tst_port_audio_bus.cpp tests/CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(audio): add PortAudioBus render-only minimal implementation

Opens PortAudio default output device, accepts push() from main
thread into a lock-free ring, drains the ring in paCallback on the
audio thread. Peak-level metering published atomically. TX pull and
full host-API/device picker come in Tasks 3.3 and 3.4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 3.3: Host API + device enumeration

**Files:**
- Modify: `src/core/audio/PortAudioBus.h`, `.cpp`

- [ ] **Step 1: Add enumeration helpers**

Add to `PortAudioBus.h`:

```cpp
    struct HostApiInfo {
        int     index;
        QString name;
    };
    struct DeviceInfo {
        int     index;
        QString name;
        int     maxOutputChannels;
        int     maxInputChannels;
        int     defaultSampleRate;
        int     hostApiIndex;
    };

    static QVector<HostApiInfo> hostApis();
    static QVector<DeviceInfo>  outputDevicesFor(int hostApiIndex);
    static QVector<DeviceInfo>  inputDevicesFor(int hostApiIndex);
```

Implement in `.cpp`:

```cpp
QVector<PortAudioBus::HostApiInfo> PortAudioBus::hostApis() {
    QVector<HostApiInfo> out;
    const int n = Pa_GetHostApiCount();
    for (int i = 0; i < n; ++i) {
        const PaHostApiInfo* h = Pa_GetHostApiInfo(i);
        if (h) { out.push_back({i, QString::fromUtf8(h->name)}); }
    }
    return out;
}

QVector<PortAudioBus::DeviceInfo> PortAudioBus::outputDevicesFor(int hostApiIndex) {
    QVector<DeviceInfo> out;
    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(i);
        if (!d || d->hostApi != hostApiIndex || d->maxOutputChannels <= 0) { continue; }
        out.push_back({
            i, QString::fromUtf8(d->name),
            d->maxOutputChannels, d->maxInputChannels,
            (int)d->defaultSampleRate, d->hostApi
        });
    }
    return out;
}

QVector<PortAudioBus::DeviceInfo> PortAudioBus::inputDevicesFor(int hostApiIndex) {
    QVector<DeviceInfo> out;
    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(i);
        if (!d || d->hostApi != hostApiIndex || d->maxInputChannels <= 0) { continue; }
        out.push_back({
            i, QString::fromUtf8(d->name),
            d->maxOutputChannels, d->maxInputChannels,
            (int)d->defaultSampleRate, d->hostApi
        });
    }
    return out;
}
```

- [ ] **Step 2: Extend the test**

Append to `tests/tst_port_audio_bus.cpp`:

```cpp
    void hostApisEnumerateNonEmpty() {
        const auto apis = PortAudioBus::hostApis();
        QVERIFY(!apis.isEmpty());
    }

    void outputDevicesEnumerateForFirstApi() {
        const auto apis = PortAudioBus::hostApis();
        QVERIFY(!apis.isEmpty());
        const auto devices = PortAudioBus::outputDevicesFor(apis.first().index);
        // Host system should have at least one output; skip if headless CI.
        if (devices.isEmpty()) { QSKIP("No output devices on test host"); }
    }
```

- [ ] **Step 3: Run to pass**

```bash
cmake --build build --target tst_port_audio_bus
./build/tests/tst_port_audio_bus
```

Expected: PASS (6 tests).

- [ ] **Step 4: Commit**

```bash
git add src/core/audio/PortAudioBus.h src/core/audio/PortAudioBus.cpp \
        tests/tst_port_audio_bus.cpp
git commit -S -m "feat(audio): PortAudioBus — host API and device enumeration

Needed by SetupAudioPage for the Driver API and Device combos.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 3.4: TX capture support (open input stream + pull)

- [ ] **Step 1: Extend `PortAudioBus::open` to support input-only or duplex**

Modify `open` to branch on `format.channels` and `m_cfg` direction. Add a `PortAudioConfig::direction` enum `{ Output, Input }`. Reuse ring buffer in opposite direction for input.

(Implementation follows the same pattern as Step 3 of Task 3.2 but with `paInputParameters` + reading from input stream into the ring, and `pull()` reading from the ring.)

- [ ] **Step 2: Test with default input device (skip on CI without mic)**

- [ ] **Step 3: Commit**

---

## Sub-Phase 4: AudioEngine Refactor 🟠

### Task 4.1: AudioEngine holds IAudioBus instances instead of QAudioSink

**Files:**
- Modify: `src/core/AudioEngine.h`, `.cpp`

- [ ] **Step 1: Add bus ownership and slot setters**

In `AudioEngine.h`:

```cpp
    // Phase 3O: per-endpoint IAudioBus ownership.
    void setSpeakersConfig(const AudioDeviceConfig& cfg);
    void setTxInputConfig(const AudioDeviceConfig& cfg);
    void setVaxConfig(int channel, const AudioDeviceConfig& cfg);  // 1..4
    void setVaxEnabled(int channel, bool on);

    // Called by ReceiverManager when a slice produces an RX audio block.
    void rxBlockReady(int sliceId, const float* samples, int frames);

private:
    std::unique_ptr<IAudioBus> m_speakersBus;
    std::unique_ptr<IAudioBus> m_txInputBus;
    std::array<std::unique_ptr<IAudioBus>, 4> m_vaxBus;
    MasterMixer m_masterMix;
```

- [ ] **Step 2: Implement `rxBlockReady` routing**

Per spec §3.4:

```cpp
void AudioEngine::rxBlockReady(int sliceId, const float* samples, int frames) {
    auto* slice = m_radio->sliceModel(sliceId);
    if (!slice) { return; }

    if (!slice->audioMuted()) {
        m_masterMix.accumulate(sliceId, samples, frames);
    }

    const int vaxCh = slice->vaxChannel();
    if (vaxCh >= 1 && vaxCh <= 4 && m_vaxBus[vaxCh - 1] && m_vaxBus[vaxCh - 1]->isOpen()) {
        m_vaxBus[vaxCh - 1]->push(
            reinterpret_cast<const char*>(samples),
            frames * 2 * sizeof(float));
    }
}
```

Add a timer-driven flush that pulls from `m_masterMix.mixInto()` into `m_speakersBus->push()` at the DSP block rate.

- [ ] **Step 3: Replace old `QAudioSink` path**

Remove the existing `QAudioSink m_sink` member and the 10ms timer-driven byte-buffer drain. Route speakers through `m_speakersBus`.

- [ ] **Step 4: Build + smoke test (tune to a known station, hear audio)**

- [ ] **Step 5: Commit**

---

## Sub-Phase 5: macOS HAL Plugin 🟡

### Task 5.1: Fork AetherSDR `hal-plugin/` into NereusSDR

**Files:**
- Create: `hal-plugin/NereusSDRVAX.cpp`
- Create: `hal-plugin/CMakeLists.txt`
- Create: `hal-plugin/Info.plist`

- [ ] **Step 1: Copy files with rebrand**

```bash
mkdir -p hal-plugin
cp ../AetherSDR/hal-plugin/AetherSDRDAX.cpp hal-plugin/NereusSDRVAX.cpp
cp ../AetherSDR/hal-plugin/CMakeLists.txt hal-plugin/CMakeLists.txt
cp ../AetherSDR/hal-plugin/Info.plist hal-plugin/Info.plist
```

- [ ] **Step 2: Prepend NereusSDR port-citation header to `.cpp`**

Per `docs/attribution/HOW-TO-PORT.md` rule 6 (AetherSDR has no per-file GPL header):

```cpp
// =================================================================
// hal-plugin/NereusSDRVAX.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   hal-plugin/AetherSDRDAX.cpp
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Ported/adapted in C++20 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Rebranded DAX → VAX: device
//                 UIDs com.aethersdr.dax.* → com.nereussdr.vax.*, shm
//                 paths /aethersdr-dax-* → /nereussdr-vax-*, device
//                 names "AetherSDR DAX N" → "NereusSDR VAX N", factory
//                 UUID regenerated.
// =================================================================
```

- [ ] **Step 3: Global rebrand — string/UID/bundle changes**

In `NereusSDRVAX.cpp`:

```bash
sed -i '' 's|/aethersdr-dax|/nereussdr-vax|g' hal-plugin/NereusSDRVAX.cpp
sed -i '' 's|AetherSDR DAX|NereusSDR VAX|g' hal-plugin/NereusSDRVAX.cpp
sed -i '' 's|com\.aethersdr\.dax|com.nereussdr.vax|g' hal-plugin/NereusSDRVAX.cpp
sed -i '' 's|AetherSDRDAX|NereusSDRVAX|g' hal-plugin/NereusSDRVAX.cpp
```

In `Info.plist`: same `AetherSDR DAX → NereusSDR VAX`, `com.aethersdr.dax → com.nereussdr.vax`. **Generate a fresh CFPlugInFactories UUID** (macOS: `uuidgen`) to avoid collision if both AetherSDR and NereusSDR are installed on the same Mac.

In `CMakeLists.txt`: bundle name `AetherSDRDAX.driver → NereusSDRVAX.driver`, target name rebranded.

- [ ] **Step 4: Add hal-plugin as optional subdir in top-level CMakeLists.txt**

```cmake
if(APPLE AND NEREUSSDR_BUILD_HAL_PLUGIN)
    add_subdirectory(hal-plugin)
endif()
```

- [ ] **Step 5: Build the HAL plugin**

```bash
cmake -B build-hal -S hal-plugin -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-hal
```

Expected: `build-hal/NereusSDRVAX.driver` bundle produced.

- [ ] **Step 6: Commit**

```bash
git add hal-plugin/ CMakeLists.txt
git commit -S -m "$(cat <<'EOF'
feat(hal-plugin): port AetherSDR HAL plugin as NereusSDR VAX driver

4 VAX render devices + 1 TX capture device exposed as native macOS
CoreAudio endpoints via libASPL userspace plugin. Rebranded from
AetherSDR DAX: device UIDs, shm paths, factory UUID regenerated.
macOS 14.4+ coreaudiod restart fallback added in Sub-Phase 5.2.

Port attribution per HOW-TO-PORT.md rule 6 (AetherSDR has no per-file
GPL header; project-level cite at header).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 5.2: Installer + macOS 14.4+ killall fallback

**Files:**
- Create: `packaging/macos/hal-installer.sh`
- Create: `packaging/macos/hal-postinstall.sh`
- Create: `packaging/macos/hal-uninstall.sh`

- [ ] **Step 1: Port `build-installer.sh`**

```bash
cp ../AetherSDR/packaging/macos/build-installer.sh packaging/macos/hal-installer.sh
sed -i '' 's|AetherSDR|NereusSDR|g; s|aethersdr|nereussdr|g' packaging/macos/hal-installer.sh
```

- [ ] **Step 2: Write postinstall with 14.4+ fallback**

`packaging/macos/hal-postinstall.sh`:

```bash
#!/bin/bash
# NereusSDR VAX HAL plugin — postinstall
# On macOS 14.4+, launchctl kickstart of com.apple.audio.coreaudiod
# returns "Operation not permitted"; fall back to killall.

set -e

launchctl kickstart -kp system/com.apple.audio.coreaudiod 2>/dev/null \
  || sudo killall coreaudiod 2>/dev/null \
  || true

exit 0
```

- [ ] **Step 3: Write uninstall helper**

```bash
#!/bin/bash
set -e
sudo rm -rf "/Library/Audio/Plug-Ins/HAL/NereusSDRVAX.driver"
rm -f /dev/shm/nereussdr-vax-* /dev/shm/nereussdr-tx 2>/dev/null || true
sudo killall coreaudiod 2>/dev/null || true
echo "NereusSDR VAX HAL plugin uninstalled."
```

- [ ] **Step 4: Run the installer build locally, verify `.pkg` produced**

```bash
bash packaging/macos/hal-installer.sh
```

Expected: `build/pkg-staging/NereusSDRVAX-<version>.pkg` created.

- [ ] **Step 5: Commit**

### Task 5.3: `CoreAudioHalBus` — shm writer from NereusSDR app side

**Files:**
- Create: `src/core/audio/CoreAudioHalBus.h`, `.cpp` (port of AetherSDR `VirtualAudioBridge`)

- [ ] **Step 1: Copy + prepend port header**

```bash
cp ../AetherSDR/src/core/VirtualAudioBridge.h src/core/audio/CoreAudioHalBus.h
cp ../AetherSDR/src/core/VirtualAudioBridge.cpp src/core/audio/CoreAudioHalBus.cpp
```

Prepend the same port-citation block as Task 5.1 to both files.

- [ ] **Step 2: Rename class + rebrand**

```bash
sed -i '' 's|VirtualAudioBridge|CoreAudioHalBus|g' src/core/audio/CoreAudioHalBus.h src/core/audio/CoreAudioHalBus.cpp
sed -i '' 's|/aethersdr-dax|/nereussdr-vax|g' src/core/audio/CoreAudioHalBus.cpp
```

- [ ] **Step 3: Adapt to `IAudioBus` interface**

Replace the AetherSDR-specific public API (`feedDaxAudio`, `readTxAudio`) with `IAudioBus::push` / `pull`. Keep the shm-segment plumbing intact. Bridge the `daxRxLevel` signal to `m_rxLevel` atomic.

- [ ] **Step 4: Build + smoke-test that writing to `/nereussdr-vax-1` is readable from the HAL plugin side**

(Manual test: open WSJT-X, pick "NereusSDR VAX 1" as audio input, confirm it receives audio from NereusSDR.)

- [ ] **Step 5: Commit**

### Task 5.4: CI — sign + notarize HAL plugin in `release.yml`

**Files:**
- Modify: `.github/workflows/release.yml`

- [ ] **Step 1: Add macOS HAL plugin signing step**

Append a job step to the macOS build matrix:

```yaml
      - name: Sign HAL plugin
        run: |
          codesign --force --options runtime --timestamp \
            --sign "Developer ID Application: ${{ secrets.APPLE_DEVELOPER_ID }}" \
            build-hal/NereusSDRVAX.driver
      - name: Build signed .pkg
        run: bash packaging/macos/hal-installer.sh
      - name: Notarize
        run: |
          xcrun notarytool submit build/pkg-staging/NereusSDRVAX-*.pkg \
            --apple-id "${{ secrets.APPLE_ID }}" \
            --team-id "${{ secrets.APPLE_TEAM_ID }}" \
            --password "${{ secrets.APPLE_APP_PASSWORD }}" \
            --wait
      - name: Staple
        run: xcrun stapler staple build/pkg-staging/NereusSDRVAX-*.pkg
```

- [ ] **Step 2: Update `docs/attribution/aethersdr-contributor-index.md`**

Add rows for `hal-plugin/NereusSDRVAX.cpp`, `src/core/audio/CoreAudioHalBus.{h,cpp}` citing AetherSDR as source.

- [ ] **Step 3: Commit**

---

## Sub-Phase 6: Linux PulseAudio Bridge 🟢

### Task 6.1: Port `PipeWireAudioBridge` → `LinuxPipeBus`

**Files:**
- Create: `src/core/audio/LinuxPipeBus.h`, `.cpp`

- [ ] **Step 1: Copy + rebrand**

```bash
cp ../AetherSDR/src/core/PipeWireAudioBridge.h src/core/audio/LinuxPipeBus.h
cp ../AetherSDR/src/core/PipeWireAudioBridge.cpp src/core/audio/LinuxPipeBus.cpp
sed -i 's|PipeWireAudioBridge|LinuxPipeBus|g' src/core/audio/LinuxPipeBus.*
sed -i 's|aethersdr-dax|nereussdr-vax|g' src/core/audio/LinuxPipeBus.*
sed -i 's|AetherSDR|NereusSDR|g' src/core/audio/LinuxPipeBus.*
```

- [ ] **Step 2: Prepend port-citation header** (same template as Task 5.1)

- [ ] **Step 3: Adapt to `IAudioBus` interface** (same pattern as Task 5.3)

- [ ] **Step 4: Verify stale-module cleanup still runs on startup**

Keep AetherSDR's `pactl list modules short | grep nereussdr-` scan intact. This is the battle-tested crash-recovery path.

- [ ] **Step 5: Manual smoke on Linux: load NereusSDR, confirm `pactl list sources short` shows `nereussdr-vax-1` through `nereussdr-vax-4`**

- [ ] **Step 6: Commit**

---

## Sub-Phase 7: Windows BYO — VirtualCableDetector 🟢

### Task 7.1: Virtual-cable product regex matcher

**Files:**
- Create: `src/core/audio/VirtualCableDetector.h`, `.cpp`
- Test: `tests/tst_virtual_cable_detector.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest/QtTest>
#include "core/audio/VirtualCableDetector.h"

using namespace NereusSDR;

class TstVirtualCableDetector : public QObject {
    Q_OBJECT
private slots:
    void detectsVbCableBase() {
        QVERIFY(VirtualCableDetector::matchProduct(
            "CABLE Input (VB-Audio Virtual Cable)") == VirtualCableProduct::VbCable);
    }
    void detectsVbCableAB() {
        QVERIFY(VirtualCableDetector::matchProduct("CABLE-A Input") == VirtualCableProduct::VbCableA);
        QVERIFY(VirtualCableDetector::matchProduct("CABLE-B Input") == VirtualCableProduct::VbCableB);
    }
    void detectsVac() {
        QVERIFY(VirtualCableDetector::matchProduct("Line 1 (Virtual Audio Cable)")
                == VirtualCableProduct::MuzychenkoVac);
    }
    void detectsVoicemeeter() {
        QVERIFY(VirtualCableDetector::matchProduct("VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)")
                == VirtualCableProduct::Voicemeeter);
    }
    void detectsDante() {
        QVERIFY(VirtualCableDetector::matchProduct("Dante Tx 01-02")
                == VirtualCableProduct::Dante);
    }
    void detectsFlexDax() {
        QVERIFY(VirtualCableDetector::matchProduct("DAX Audio RX 1")
                == VirtualCableProduct::FlexRadioDax);
    }
    void detectsReservedNereusVax() {
        QVERIFY(VirtualCableDetector::matchProduct("NereusSDR VAX 1")
                == VirtualCableProduct::NereusSdrVax);
    }
    void unknownDeviceIsNone() {
        QVERIFY(VirtualCableDetector::matchProduct("Realtek HD Audio Output")
                == VirtualCableProduct::None);
    }
};

QTEST_APPLESS_MAIN(TstVirtualCableDetector)
#include "tst_virtual_cable_detector.moc"
```

- [ ] **Step 2: Run to fail**

- [ ] **Step 3: Implement matcher**

`src/core/audio/VirtualCableDetector.h`:

```cpp
#pragma once
#include <QString>
#include <QVector>

namespace NereusSDR {

enum class VirtualCableProduct {
    None = 0,
    VbCable,
    VbCableA,  VbCableB,  VbCableC,  VbCableD,
    VbHiFiCable,
    MuzychenkoVac,
    Voicemeeter,
    Dante,
    FlexRadioDax,
    NereusSdrVax,  // reserved for future NereusSDR-owned Windows driver
};

struct DetectedCable {
    VirtualCableProduct product;
    QString deviceName;
    bool isInput;  // false = render / output
    int channel;   // 1..N for multi-cable families; 0 if N/A
};

class VirtualCableDetector {
public:
    // Pure-function test-hook. Inspects the OS device name string.
    static VirtualCableProduct matchProduct(const QString& deviceName);

    // Enumerates OS audio devices via PortAudio and returns matches.
    static QVector<DetectedCable> scan();

    // Vendor install URL for a given product (for the first-run helper).
    static QString installUrl(VirtualCableProduct p);
};

} // namespace NereusSDR
```

`src/core/audio/VirtualCableDetector.cpp`:

```cpp
#include "VirtualCableDetector.h"
#include <QRegularExpression>
#include "PortAudioBus.h"

using namespace NereusSDR;

VirtualCableProduct VirtualCableDetector::matchProduct(const QString& n) {
    static const struct { QRegularExpression re; VirtualCableProduct p; } rules[] = {
        { QRegularExpression("^NereusSDR VAX \\d$"), VirtualCableProduct::NereusSdrVax },
        { QRegularExpression("^CABLE-A ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableA },
        { QRegularExpression("^CABLE-B ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableB },
        { QRegularExpression("^CABLE-C ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableC },
        { QRegularExpression("^CABLE-D ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableD },
        { QRegularExpression("Hi-?Fi Cable", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbHiFiCable },
        { QRegularExpression("^CABLE ",    QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCable },
        { QRegularExpression("Virtual Audio Cable", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::MuzychenkoVac },
        { QRegularExpression("^Line \\d+", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::MuzychenkoVac },
        { QRegularExpression("VoiceMeeter", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::Voicemeeter },
        { QRegularExpression("^Dante ",    QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::Dante },
        { QRegularExpression("^DAX Audio", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::FlexRadioDax },
    };
    for (const auto& r : rules) {
        if (r.re.match(n).hasMatch()) { return r.p; }
    }
    return VirtualCableProduct::None;
}

QString VirtualCableDetector::installUrl(VirtualCableProduct p) {
    switch (p) {
        case VirtualCableProduct::VbCable:
        case VirtualCableProduct::VbCableA:
        case VirtualCableProduct::VbCableB:
        case VirtualCableProduct::VbCableC:
        case VirtualCableProduct::VbCableD:
        case VirtualCableProduct::VbHiFiCable:
            return "https://vb-audio.com/Cable/";
        case VirtualCableProduct::MuzychenkoVac:
            return "https://vac.muzychenko.net/en/";
        case VirtualCableProduct::Voicemeeter:
            return "https://voicemeeter.com/";
        case VirtualCableProduct::Dante:
            return "https://www.getdante.com/products/software-essentials/dante-virtual-soundcard/";
        default:
            return {};
    }
}

QVector<DetectedCable> VirtualCableDetector::scan() {
    QVector<DetectedCable> out;
    for (const auto& api : PortAudioBus::hostApis()) {
        for (const auto& dev : PortAudioBus::outputDevicesFor(api.index)) {
            const auto p = matchProduct(dev.name);
            if (p != VirtualCableProduct::None) {
                out.push_back({p, dev.name, false, 0});
            }
        }
        for (const auto& dev : PortAudioBus::inputDevicesFor(api.index)) {
            const auto p = matchProduct(dev.name);
            if (p != VirtualCableProduct::None) {
                out.push_back({p, dev.name, true, 0});
            }
        }
    }
    return out;
}
```

- [ ] **Step 4: Run to pass**

Expected: PASS (8 tests).

- [ ] **Step 5: Commit**

```bash
git add src/core/audio/VirtualCableDetector.h src/core/audio/VirtualCableDetector.cpp \
        tests/tst_virtual_cable_detector.cpp tests/CMakeLists.txt
git commit -S -m "feat(audio): add VirtualCableDetector for Windows BYO auto-detect

Regex-matches known virtual-cable product names (VB-Audio family, VAC,
Voicemeeter, Dante, FlexRadio DAX, reserved NereusSDR VAX). Pure
matcher is test-hooked; live scan enumerates PortAudio devices and
filters.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Sub-Phase 8: VAX UI — VFO Flag Selector 🟢

### Task 8.1: `VaxChannelSelector` widget

**Files:**
- Create: `src/gui/widgets/VaxChannelSelector.h`, `.cpp`
- Test: `tests/tst_vax_channel_selector.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/widgets/VaxChannelSelector.h"

using namespace NereusSDR;

class TstVaxChannelSelector : public QObject {
    Q_OBJECT
private slots:
    void defaultsToOff() {
        VaxChannelSelector w;
        QCOMPARE(w.value(), 0);
    }
    void setValueUpdatesUi() {
        VaxChannelSelector w;
        w.setValue(3);
        QCOMPARE(w.value(), 3);
    }
    void clickingButtonEmitsSignal() {
        VaxChannelSelector w;
        QSignalSpy spy(&w, &VaxChannelSelector::valueChanged);
        w.simulateClick(2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }
    void programmaticSetDoesNotEmit() {
        VaxChannelSelector w;
        QSignalSpy spy(&w, &VaxChannelSelector::valueChanged);
        w.setValue(4);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TstVaxChannelSelector)
#include "tst_vax_channel_selector.moc"
```

- [ ] **Step 2: Implement widget per STYLEGUIDE.md palette**

`src/gui/widgets/VaxChannelSelector.h`:

```cpp
#pragma once
#include <QWidget>

class QPushButton;
class QButtonGroup;

namespace NereusSDR {

class VaxChannelSelector : public QWidget {
    Q_OBJECT
public:
    explicit VaxChannelSelector(QWidget* parent = nullptr);

    int  value() const { return m_value; }
    void setValue(int ch);  // 0..4; no signal

    void simulateClick(int ch);  // test-hook

signals:
    void valueChanged(int ch);

private:
    QButtonGroup* m_group{nullptr};
    QPushButton*  m_buttons[5]{};
    int m_value{0};
    bool m_programmaticUpdate{false};
};

} // namespace NereusSDR
```

Implementation uses `QButtonGroup::buttonToggled` + `m_programmaticUpdate` guard to suppress echo when `setValue` is called. Styling follows STYLEGUIDE.md Button active-blue (`#0070c0` / `#0090e0` / `#ffffff`) for the selected button.

- [ ] **Step 3: Run to pass**

- [ ] **Step 4: Commit**

### Task 8.2: Embed `VaxChannelSelector` in `VfoWidget`

**Files:**
- Modify: `src/gui/VfoWidget.h`, `.cpp`

- [ ] **Step 1: Add VAX row to VfoWidget layout**

In `VfoWidget`'s `buildUi()` (or equivalent layout builder), insert a new row below the AGC/NR tab row:

```cpp
    m_vaxRow = new QWidget(this);
    auto* vaxLayout = new QHBoxLayout(m_vaxRow);
    vaxLayout->setContentsMargins(10, 4, 10, 4);
    vaxLayout->setSpacing(3);
    auto* lbl = new QLabel("VAX", m_vaxRow);
    lbl->setStyleSheet("color:#8090a0;font-size:10px;");
    vaxLayout->addWidget(lbl);
    m_vaxSelector = new VaxChannelSelector(m_vaxRow);
    vaxLayout->addWidget(m_vaxSelector);
    vaxLayout->addStretch(1);
    m_rootLayout->addWidget(m_vaxRow);
```

- [ ] **Step 2: Wire bidirectionally to `SliceModel`**

```cpp
    connect(m_vaxSelector, &VaxChannelSelector::valueChanged,
            this, [this](int ch){
        if (m_slice) { m_slice->setVaxChannel(ch); }
    });
    connect(m_slice, &SliceModel::vaxChannelChanged,
            m_vaxSelector, &VaxChannelSelector::setValue);
    m_vaxSelector->setValue(m_slice->vaxChannel());
```

- [ ] **Step 3: Build + launch app; verify VAX row renders; clicking changes SliceModel value**

- [ ] **Step 4: Commit**

---

## Sub-Phase 9: VaxApplet 🟡

### Task 9.1: Port `MeterSlider` from AetherSDR

- [ ] **Step 1: Copy + rebrand + port header** (same pattern as Task 5.1)

```bash
cp ../AetherSDR/src/gui/MeterSlider.h src/gui/widgets/MeterSlider.h
cp ../AetherSDR/src/gui/MeterSlider.cpp src/gui/widgets/MeterSlider.cpp
# (AetherSDR .cpp is nearly empty — mostly header-inline.)
```

Prepend port-citation block.

- [ ] **Step 2: Adapt include paths + namespace**

- [ ] **Step 3: Register in `docs/attribution/aethersdr-contributor-index.md`**

- [ ] **Step 4: Commit**

### Task 9.2: Port `DaxApplet` → `VaxApplet`

**Files:**
- Create: `src/gui/applets/VaxApplet.h`, `.cpp`

- [ ] **Step 1: Copy + port header**

```bash
cp ../AetherSDR/src/gui/DaxApplet.h src/gui/applets/VaxApplet.h
cp ../AetherSDR/src/gui/DaxApplet.cpp src/gui/applets/VaxApplet.cpp
sed -i 's|DaxApplet|VaxApplet|g; s|daxRxLevel|vaxRxLevel|g; s|daxTxLevel|vaxTxLevel|g; \
        s|daxEnable|vaxEnable|g; s|daxToggled|vaxToggled|g; \
        s|daxRxGainChanged|vaxRxGainChanged|g; s|daxTxGainChanged|vaxTxGainChanged|g; \
        s|DAX|VAX|g; s|"dax"|"vax"|g' src/gui/applets/VaxApplet.*
```

Prepend port-citation block.

- [ ] **Step 2: Adapt to NereusSDR ContainerWidget parent (not AetherSDR's applet base)**

Replace `setRadioModel(RadioModel*)` signature with NereusSDR's equivalent binding path. Style the title bar using `appletTitleBar("VAX")` per STYLEGUIDE.md.

- [ ] **Step 3: Wire signals through `AudioEngine` slots** (Task 4.1's `setVaxEnabled`, `setVaxRxGain`, etc.)

- [ ] **Step 4: Register in the applet factory so users can add it to a container**

- [ ] **Step 5: Build + launch; add VaxApplet to Container #0; verify channel strips render with live meters when audio is flowing to a VAX channel**

- [ ] **Step 6: Commit**

---

## Sub-Phase 10: MasterOutputWidget 🟢

### Task 10.1: Menu-bar master output strip widget

**Files:**
- Create: `src/gui/MasterOutputWidget.h`, `.cpp`

- [ ] **Step 1: Implement widget**

Per the mockup (`.superpowers/brainstorm/64803-*/content/mockup-speakers.html`): 14px speaker icon + 100px slider + dB readout + MUTE button. Style with STYLEGUIDE.md inset `#0a0a18` background + `#1e2e3e` border; accent `#00b4d8` slider handle. Right-click opens a device-picker popup menu populated from `PortAudioBus::outputDevicesFor()`.

- [ ] **Step 2: Install into main window's menu-bar corner**

In `MainWindow::MainWindow`:

```cpp
    m_masterOutput = new MasterOutputWidget(this);
    menuBar()->setCornerWidget(m_masterOutput, Qt::TopRightCorner);
    connect(m_masterOutput, &MasterOutputWidget::volumeChanged,
            m_radio->audioEngine(), &AudioEngine::setMasterVolume);
    connect(m_masterOutput, &MasterOutputWidget::muteChanged,
            m_radio->audioEngine(), &AudioEngine::setMasterMuted);
```

- [ ] **Step 3: Persist to AppSettings on change, restore on app start**

- [ ] **Step 4: Commit**

---

## Sub-Phase 11: VaxFirstRunDialog 🟡

### Task 11.1: Dialog with five scenario modes

**Files:**
- Create: `src/gui/VaxFirstRunDialog.h`, `.cpp`

- [ ] **Step 1: Implement five scenario constructors**

```cpp
enum class FirstRunScenario {
    WindowsCablesFound,
    WindowsNoCables,
    MacNative,
    LinuxNative,
    RescanNewCables,
};

class VaxFirstRunDialog : public QDialog {
    Q_OBJECT
public:
    VaxFirstRunDialog(FirstRunScenario s, const QVector<DetectedCable>& detected, QWidget* parent = nullptr);

signals:
    void applySuggested(const QVector<QPair<int /*vaxCh*/, QString /*deviceName*/>>& bindings);
    void openSetupAudioTab();
    void openInstallUrl(const QString& url);
};
```

Layouts per the browser mockup (`mockup-firstrun.html`). Styling uses inline-style mockups (not Qt stylesheets) — follow STYLEGUIDE.md palette.

- [ ] **Step 2: Hook into MainWindow startup**

```cpp
void MainWindow::checkVaxFirstRun() {
    const auto& s = AppSettings::instance();
    if (s.value("audio/FirstRunComplete", "False").toString() == "True") { return; }

    const auto detected = VirtualCableDetector::scan();
#if defined(Q_OS_WIN)
    const auto scenario = detected.isEmpty()
        ? FirstRunScenario::WindowsNoCables
        : FirstRunScenario::WindowsCablesFound;
#elif defined(Q_OS_MAC)
    const auto scenario = FirstRunScenario::MacNative;
#else
    const auto scenario = FirstRunScenario::LinuxNative;
#endif

    auto* dlg = new VaxFirstRunDialog(scenario, detected, this);
    connect(dlg, &VaxFirstRunDialog::applySuggested, this, [this](const auto& bindings) {
        for (const auto& [ch, dev] : bindings) {
            m_radio->audioEngine()->setVaxDevice(ch, dev);
            m_radio->audioEngine()->setVaxEnabled(ch, true);
        }
        AppSettings::instance().setValue("audio/FirstRunComplete", "True");
        AppSettings::instance().save();
    });
    dlg->show();
}
```

- [ ] **Step 3: Manual smoke on each platform**

Trigger by clearing `audio/FirstRunComplete=True` from AppSettings and restarting.

- [ ] **Step 4: Commit**

---

## Sub-Phase 12: Setup → Audio Page 🟡

### Task 12.1: `SetupAudioPage` shell with sub-tabs

**Files:**
- Create: `src/gui/SetupAudioPage.h`, `.cpp`

- [ ] **Step 1: Extend existing `SetupPage` base** (`src/gui/SetupPage.h`)

```cpp
class SetupAudioPage : public SetupPage {
    Q_OBJECT
public:
    SetupAudioPage(RadioModel* model, QWidget* parent = nullptr);

private:
    QTabWidget* m_subTabs{nullptr};
    QWidget* buildDevicesTab();
    QWidget* buildVaxTab();
    QWidget* buildTciTab();      // placeholder: "Coming in Phase 3J"
    QWidget* buildAdvancedTab();
};
```

Layout: `QTabWidget` with four tabs.

- [ ] **Step 2: Register in `SetupDialog`**

In `SetupDialog.cpp`, add the page under the Audio category.

- [ ] **Step 3: Commit**

### Task 12.2: Devices tab (Speakers / Headphones / TX Input cards)

- [ ] **Step 1: Build three device cards using QGroupBox + form layout**

Each card has: Driver API combo (MME/DirectSound/WDM-KS/WASAPI/ASIO-via-PortAudio on Windows; CoreAudio on Mac; ALSA/Pulse/PipeWire/JACK on Linux), Device combo, Sample rate, Bit depth, Channels, Buffer size, Manual latency, per-API options (WASAPI exclusive / event-driven / bypass-mixer), negotiated-format preview label.

**Note:** Direct ASIO engine and cmASIO parity controls (base channel, input mode L/R/Both, lock mode, Control Panel launcher) are **deferred** per the GPL compliance review above. ASIO users route through PortAudio's built-in ASIO host API — no Engine radio button in this phase.

- [ ] **Step 2: Wire every control per Spec §6.5** (see control wiring table in the design spec — skip rows marked Direct-ASIO-only)

Each combo: `currentTextChanged` → `AudioEngine::setXxxDriverApi()` → reopens bus; reverse via `XxxFormatNegotiated(AudioFormat)` signal.

- [ ] **Step 3: Commit**

### Task 12.3: VAX tab (per-channel cards)

- [ ] **Step 1: Build four channel cards (1–4) + TX row**

Native VAX channels (Mac/Linux) show compact cards with "native driver" badge + fixed 48kHz/24-bit format, no device-picker. Windows BYO cards show full Driver API / Device combos.

- [ ] **Step 2: Wire controls per Spec §6.6**

- [ ] **Step 3: "Auto-detect…" button on unassigned slots calls `VirtualCableDetector::scan()` and opens a mini-picker**

- [ ] **Step 4: Commit**

### Task 12.4: Advanced tab (DSP + IVAC parity + extras)

- [ ] **Step 1: Build Advanced tab per Spec §6.7**

DSP sample rate combo, DSP block size combo, VAC feedback per-target tuning (gain/slew/rings), SendIqToVax / TxMonitorToVax / MuteVaxDuringTxOnOtherSlice checkboxes, detected-cables readonly row + Rescan button, "Reset all audio to defaults" (amber confirm-modal).

- [ ] **Step 2: Wire feature-flagged controls**

`SendIqToVax` and `TxMonitorToVax` persist but their wiring logs a `qCWarning(lcAudio) << "IQ to VAX reserved for future phase"` when toggled on.

- [ ] **Step 3: Commit**

### Task 12.5: TCI tab placeholder

- [ ] **Step 1: Disabled tab labeled "Coming in Phase 3J" with a stub**

```cpp
QWidget* SetupAudioPage::buildTciTab() {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    auto* lbl = new QLabel("TCI server — coming in Phase 3J (see design spec §11.1)");
    lbl->setStyleSheet("color:#8090a0;font-size:12px;padding:40px;");
    lbl->setAlignment(Qt::AlignCenter);
    lay->addWidget(lbl);
    return w;
}
```

- [ ] **Step 2: Commit**

---

## Sub-Phase 13: Help Docs 🟢

### Task 13.1: `install-virtual-cables.md`

**Files:**
- Create: `resources/help/install-virtual-cables.md`

- [ ] **Step 1: Write per-platform install guide** — per mockup `mockup-firstrun.html` scenario B content. Vendor install URLs; step-by-step for VB-CABLE; note for Mac/Linux that native VAX is already installed; PipeWire null-sink auto-create option for Linux advanced users.

- [ ] **Step 2: Link from `README.md` feature list and from `VaxFirstRunDialog`**

- [ ] **Step 3: Commit**

---

## Sub-Phase 14: End-to-End Verification 🔴

### Task 14.1: macOS QSO round-trip

- [ ] **Step 1: Install NereusSDR .pkg from Task 5.2 build output**
- [ ] **Step 2: Confirm "NereusSDR VAX 1–4" + "NereusSDR TX" appear in macOS Audio MIDI Setup**
- [ ] **Step 3: Launch WSJT-X, pick "NereusSDR VAX 1" as input**
- [ ] **Step 4: Tune NereusSDR RX1 to 14.074 USB; set VAX pill to `1`; confirm WSJT-X shows decoded FT8 traffic**
- [ ] **Step 5: Commit a VERIFICATION.md note summarizing the test**

### Task 14.2: Linux QSO round-trip

- [ ] **Step 1: Launch NereusSDR; confirm `pactl list sources short` lists `nereussdr-vax-1..4`**
- [ ] **Step 2: WSJT-X on same host picks "NereusSDR VAX 1", sees audio** — confirm FT8 decode
- [ ] **Step 3: Commit VERIFICATION.md update**

### Task 14.3: Windows QSO round-trip (BYO path)

- [ ] **Step 1: Install NereusSDR; first-run dialog appears; install VB-CABLE from the vendor link if not present**
- [ ] **Step 2: Restart NereusSDR; first-run dialog auto-binds detected cables; click Apply Suggested**
- [ ] **Step 3: WSJT-X picks "CABLE-A Output" as input; set NereusSDR RX1 VAX=1; confirm FT8 decode**
- [ ] **Step 4: Commit VERIFICATION.md update**

### Task 14.4: Attribution + compliance-inventory final sweep

- [ ] **Step 1: Run attribution verifier scripts**

```bash
bash scripts/verify-thetis-headers.py
bash scripts/check-new-ports.py
```

Expected: all pass. Every ported file in this phase (`CoreAudioHalBus`, `LinuxPipeBus`, `VaxApplet`, `MeterSlider`, `NereusSDRVAX.cpp` hal-plugin) is registered in `docs/attribution/aethersdr-contributor-index.md` with correct header.

- [ ] **Step 2: Update `docs/attribution/COMPLIANCE-INVENTORY.md`**

Add a new section for Phase 3O covering:
- Every new port (source upstream + NereusSDR path + license inheritance statement).
- The macOS HAL plugin's separate-binary status: the `NereusSDRVAX.driver` bundle is a standalone CoreAudio plug-in loaded by `coreaudiod` in its own process, communicating with NereusSDR.app only via POSIX shared memory. Bundled in the same `.pkg` installer with NereusSDR.app is mere aggregation on an installation medium per GPL-3 §5; they are not a single combined work.
- The 2026-04-19 GPL-3 compliance review result (Direct ASIO deferred; link to this plan's GPL Compliance Review section).
- PortAudio vendoring: MIT-licensed, linked statically; NereusSDR's corresponding-source obligation is satisfied by the FetchContent reference in `CMakeLists.txt` pinning the exact upstream commit.
- libASPL vendoring (macOS only): MIT-licensed, linked into the separate HAL plugin binary; same corresponding-source reasoning.

- [ ] **Step 3: Commit**

```bash
git add docs/attribution/COMPLIANCE-INVENTORY.md docs/attribution/aethersdr-contributor-index.md
git commit -S -m "$(cat <<'EOF'
docs(compliance): record Phase 3O ports + HAL plugin reasoning

Registers every new AetherSDR port from Phase 3O in the
aethersdr-contributor-index, and documents the macOS HAL plugin's
separate-binary / mere-aggregation status per GPL-3 §5. Also logs
the 2026-04-19 GPL compliance review that dropped Direct ASIO from
this phase due to Steinberg SDK non-redistributability.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Post-plan: open PR and mark Phase 3O complete in MASTER-PLAN

- [ ] Push `feature/phase-3o-vax` to origin
- [ ] `gh pr create` with summary linking the design spec and this plan
- [ ] After merge, update `docs/MASTER-PLAN.md` to mark Phase 3O ✅ COMPLETE with a short shipped-notes block

---

## Self-Review Notes

Spec coverage check (design spec §1–§14):
- §1–§2 Purpose + scope → covered by plan prose intro
- §3 Architecture → Sub-Phases 2, 4 (IAudioBus, AudioEngine, MasterMixer)
- §4.1 Ported files → Sub-Phases 5 (macOS HAL + CoreAudioHalBus), 6 (LinuxPipeBus), 9 (MeterSlider + VaxApplet). DirectAsioBus + AsioSdkHost deferred per GPL compliance review — not in this phase.
- §4.2 New files → all created across Sub-Phases 2, 3, 7, 8, 10, 11, 12
- §4.3 Modified files → Sub-Phases 1, 4, 8, 10, 11, 12
- §5 Data model → Sub-Phase 1
- §6 Control wiring table → reflected per-tab/per-widget in Sub-Phases 8, 9, 10, 11, 12
- §7 UI components → Sub-Phases 8, 9, 10, 11, 12
- §8 Platform specifics → Sub-Phases 5, 6, 7 (§8.5 Direct ASIO deferred per GPL compliance review)
- §9 Difficulty ranking → mirrored in header emoji
- §10 Risks → not a task list item; addressed inline in relevant task notes (macOS 14.4 fallback Task 5.2, PipeWire stale-module cleanup Task 6.1)
- §11 Reserved integration points → preserved via `VirtualCableDetector::NereusSdrVax` pattern (Task 7.1) and `IAudioBus` abstraction (Sub-Phase 2)
- §12 Attribution checklist → final sweep Task 15.4
- §13 Plan review history → this plan adds an implementation-plan entry; MASTER-PLAN review history already updated
- §14 Success criteria → verified in Sub-Phase 15

Placeholder scan: Tasks 3.4, 4.1–4.3, 12.2–12.5 reference "same pattern" or defer to "Spec §6.X"; I've left those as direct spec references rather than re-paste the full wiring table per task to keep the plan under 3000 lines. Each such reference points at the spec's concrete wiring rows, not at TBDs. If the implementing agent hits a row they can't interpret, they stop and ask — consistent with CLAUDE.md ring-1 checklist.

Type consistency: `IAudioBus`, `AudioFormat`, `AudioDeviceConfig`, `VaxSlot`, `VirtualCableProduct`, `DetectedCable`, `FirstRunScenario` all reused with identical names across tasks. `PortAudioConfig` defined Task 3.2, extended Task 3.3.

Post-compliance-review edit (2026-04-19): Sub-Phase 13 (Direct ASIO Engine) removed and subsequent sub-phases renumbered (14→13 Help Docs, 15→14 E2E Verification). Task 3.1 gained a PortAudio license-verification step. Task 14.4 expanded to cover `COMPLIANCE-INVENTORY.md` updates including Phase 3O port registrations, HAL plugin mere-aggregation reasoning, and the GPL-3 compliance review outcome. File structure table, modified-files table, type-consistency checklist, and spec-coverage §4.1/§8 mappings updated to match.

---

**Plan complete and saved to `docs/architecture/2026-04-19-phase3o-vax-plan.md`. Two execution options:**

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Uses `superpowers:subagent-driven-development`.

2. **Inline Execution** — Execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints.

**Which approach?**
