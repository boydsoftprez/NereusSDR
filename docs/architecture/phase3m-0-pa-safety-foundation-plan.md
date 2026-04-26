# Phase 3M-0 PA Safety Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land the PA safety net (SWR protection, TX inhibit, watchdog, band plan guard, PA telemetry routing, HighSWR overlay, RX-only/PA-trip splits) in main before any TX code is written, so when 3M-1a fires the first MOX byte the radio is protected automatically.

**Architecture:** 8 cohesive components. New `src/core/safety/` directory with three controllers (Swr / TxInhibit / BandPlanGuard) owned by `RadioModel`. New `src/models/RadioModel::paTripped()` live state distinct from `BoardCapabilities` capability flags. New `RadioConnection::setWatchdogEnabled(bool)` API. New Setup → Transmit + Setup → General sections. Existing 3P-H telemetry signals routed into `MeterPoller` cache. Static "HIGH SWR" + 6 px red border overlay added to `SpectrumWidget`. Everything observable but inactive until 3M-1a fires the first MOX.

**Tech Stack:** C++20, Qt6 (Widgets / Test), CMake + Ninja, ctest. Test framework is `QtTest` via the existing `nereus_add_test()` helper in `tests/CMakeLists.txt`. All commits GPG-signed (per `feedback_gpg_sign_commits`). All Thetis cites carry version stamp `[v2.10.3.13]` or `[@501e3f51]` (per `feedback_inline_cite_versioning`). User-visible strings stay plain English; cites in inline source comments only (per `feedback_no_cites_in_user_strings`).

**Source-of-truth design doc:** `docs/architecture/phase3m-tx-epic-master-design.md` — Section 4 covers 3M-0. Pre-code Thetis review for 3M-0 closed 2026-04-25; this plan implements the corrected design.

---

## File Structure

**New files (created in this plan):**

| Path | Responsibility |
|---|---|
| `src/core/safety/SwrProtectionController.h/.cpp` | Two-stage SWR foldback computation + windback latch + open-antenna detection. Pure computation; no Qt parent. |
| `src/core/safety/TxInhibitMonitor.h/.cpp` | 100 ms poll on per-board user-IO pin; emits `txInhibited(bool, source)` |
| `src/core/safety/BandPlanGuard.h/.cpp` | Compiled `constexpr` region/band/mode/60m-channel tables + `isValidTxFreq(region, freqHz, mode)` predicate |
| `src/core/safety/safety_constants.h` | Per-board PA scaling triplets (`bridge_volt`/`refvoltage`/`adc_cal_offset`) per `console.cs:25008-25072` |
| `tests/tst_swr_protection_controller.cpp` | Unit tests for foldback math, trip latch, open-antenna case |
| `tests/tst_tx_inhibit_monitor.cpp` | Unit tests for source dispatch + signal firing |
| `tests/tst_band_plan_guard.cpp` | Table-driven tests for 24 regions × 14 bands × 60m channels |
| `tests/tst_pa_scaling.cpp` | Per-board `computeAlexFwdPower` reference-value tests |

**Modified files:**

| Path | Change |
|---|---|
| `src/core/RadioConnection.h/.cpp` | Add `virtual void setWatchdogEnabled(bool)` |
| `src/core/P1RadioConnection.cpp` + `src/core/P2RadioConnection.cpp` | Implement `setWatchdogEnabled` writing the wire bit |
| `src/core/RadioModel.h/.cpp` | Own the three safety controllers; add `paTripped()` live state property + `paTrippedChanged` signal |
| `src/models/BoardCapabilities.h/.cpp` | Add `isRxOnlySku()`, `canDriveGanymede()` — both static SKU-level capabilities |
| `src/gui/SpectrumWidget.h/.cpp` | Add `setHighSwrOverlay(bool, bool foldback)` painting "HIGH SWR" text + 6 px red border |
| `src/gui/meters/MeterPoller.h/.cpp` | Subscribe to `RadioStatus::pa{Fwd,Rev,Swr}Changed` signals; cache values for `MeterBinding::TxPower` (100), `TxReversePower` (101), `TxSwr` (102) |
| `src/gui/setup/TransmitSetupPages.h/.cpp` | Build new SWR Protection group, External TX Inhibit group, Block-TX Ant 2/3 checkboxes, Disable HF PA toggle |
| `src/gui/setup/GeneralOptionsPage.h/.cpp` | Build Hardware Configuration (Region combo, Extended toggle, RX-only, Network WDT) + Options (Prevent TX on different band) |
| `src/core/AppSettings.h/.cpp` | Persistence keys per Section 3 of pre-code review report |
| `tests/CMakeLists.txt` | Register the 4 new tests |

---

## Pre-flight checks (before Task 1)

- [ ] **Step 0: Confirm we're on a fresh worktree off main**

Run:
```bash
git status
git rev-parse --abbrev-ref HEAD
```
Expected: clean working tree on a branch like `feature/phase3m-0-pa-safety` (or whatever name you cut). If you're still on `claude/distracted-clarke-a14afe` with PR #134 in flight, cut a fresh worktree first:
```bash
git worktree add -b feature/phase3m-0-pa-safety ../wt-3m-0 origin/main
cd ../wt-3m-0
```

- [ ] **Step 0.1: Verify the build is green before starting**

Run:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
Expected: all existing tests pass.

---

## Task 1: BoardCapabilities — `isRxOnlySku()` + `canDriveGanymede()`

**Files:**
- Modify: `src/models/BoardCapabilities.h`
- Modify: `src/models/BoardCapabilities.cpp`
- Test: `tests/tst_board_capabilities.cpp` (existing — extend)

- [ ] **Step 1.1: Write failing tests for the two new flags**

Append to `tests/tst_board_capabilities.cpp` inside the existing `TestBoardCapabilities` class:
```cpp
private slots:
    void rxOnlySku_HermesLite2RxOnlyVariants_returnsTrue();
    void rxOnlySku_StandardHl2_returnsFalse();
    void rxOnlySku_AnanG2_returnsFalse();
    void canDriveGanymede_AndromedaConsoleFamily_returnsTrue();
    void canDriveGanymede_StandardAnan_returnsFalse();

private:
    static BoardCapabilities makeFor(HpsdrModel model);
};

void TestBoardCapabilities::rxOnlySku_HermesLite2RxOnlyVariants_returnsTrue()
{
    // Hermes Lite 2 RX-only kits ship without a TX driver.
    // (NereusSDR-original capability — Thetis treats RX-only purely as a
    //  user toggle; we add the SKU bit to also block these variants.)
    auto caps = makeFor(HpsdrModel::HermesLiteRxOnly);
    QVERIFY(caps.isRxOnlySku());
}

void TestBoardCapabilities::rxOnlySku_StandardHl2_returnsFalse()
{
    auto caps = makeFor(HpsdrModel::HermesLite2);
    QVERIFY(!caps.isRxOnlySku());
}

void TestBoardCapabilities::rxOnlySku_AnanG2_returnsFalse()
{
    auto caps = makeFor(HpsdrModel::AnanG2);
    QVERIFY(!caps.isRxOnlySku());
}

void TestBoardCapabilities::canDriveGanymede_AndromedaConsoleFamily_returnsTrue()
{
    // Ganymede 500W PA is connected on Andromeda console family
    auto caps = makeFor(HpsdrModel::Andromeda);
    QVERIFY(caps.canDriveGanymede());
}

void TestBoardCapabilities::canDriveGanymede_StandardAnan_returnsFalse()
{
    auto caps = makeFor(HpsdrModel::AnanG2);
    QVERIFY(!caps.canDriveGanymede());
}
```

- [ ] **Step 1.2: Build & run; verify the tests fail with "no member"**

```bash
cmake --build build --target tst_board_capabilities -j$(nproc)
cd build && ctest -R tst_board_capabilities -V
```
Expected: build error mentioning `isRxOnlySku` and `canDriveGanymede` not declared.

- [ ] **Step 1.3: Add the methods to `BoardCapabilities.h`**

Add inside the public section:
```cpp
    /// True iff this SKU ships without TX hardware (Hermes Lite 2 RX-only
    /// kits, etc.). NereusSDR-original capability flag — Thetis treats
    /// RX-only purely as a user toggle (chkGeneralRXOnly,
    /// console.cs:15283-15307 [v2.10.3.13]); we add this so SKUs without
    /// TX drivers are hard-blocked regardless of user settings.
    bool isRxOnlySku() const noexcept { return m_isRxOnlySku; }

    /// True iff this SKU is the Andromeda-console family that connects
    /// to a Ganymede 500W PA (Andromeda.cs:914-920 [v2.10.3.13]).
    bool canDriveGanymede() const noexcept { return m_canDriveGanymede; }

private:
    bool m_isRxOnlySku{false};
    bool m_canDriveGanymede{false};
```

- [ ] **Step 1.4: Wire the flags in the per-model factory**

In `src/models/BoardCapabilities.cpp`, in the per-model `make*` factory functions, set the new flags. Example for Andromeda:
```cpp
BoardCapabilities BoardCapabilities::makeAndromeda()
{
    BoardCapabilities caps;
    // ... existing flags ...
    caps.m_canDriveGanymede = true;   // Andromeda console drives Ganymede
    return caps;
}

// And for the RX-only HL2 variant:
BoardCapabilities BoardCapabilities::makeHermesLiteRxOnly()
{
    BoardCapabilities caps = makeHermesLite2();
    caps.m_isRxOnlySku = true;
    return caps;
}
```

If `HpsdrModel::HermesLiteRxOnly` does not yet exist in the enum, add it to `src/core/HpsdrModel.h` next to `HermesLite2`. If `HpsdrModel::Andromeda` doesn't exist either, add it. (These additions don't need ports right now; they're capability slots.)

- [ ] **Step 1.5: Build & run; verify all 5 tests pass**

```bash
cmake --build build --target tst_board_capabilities -j$(nproc)
cd build && ctest -R tst_board_capabilities -V
```
Expected: all tests pass.

- [ ] **Step 1.6: Commit**

```bash
git add src/models/BoardCapabilities.h src/models/BoardCapabilities.cpp \
        src/core/HpsdrModel.h tests/tst_board_capabilities.cpp
git commit -m "$(cat <<'EOF'
feat(caps): isRxOnlySku() + canDriveGanymede() for 3M-0 safety net

NereusSDR-original isRxOnlySku() flag for HL2 RX-only kits (Thetis
treats RX-only purely as user toggle; we add SKU-level block).
canDriveGanymede() captures the Andromeda console family PA-trip
capability per Andromeda.cs:914-920 [v2.10.3.13].

Both static SKU capabilities. Live PA-trip state lives on RadioModel
in a later task (paTripped()).

Phase 3M-0 Task 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: BandPlanGuard — region tables + 60m channels + isValidTxFreq

**Files:**
- Create: `src/core/safety/BandPlanGuard.h`
- Create: `src/core/safety/BandPlanGuard.cpp`
- Create: `tests/tst_band_plan_guard.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test)

- [ ] **Step 2.1: Write the failing test for US 60m channels**

Create `tests/tst_band_plan_guard.cpp`:
```cpp
#include <QtTest>
#include "core/safety/BandPlanGuard.h"
#include "core/HpsdrModel.h"
#include "models/Band.h"

using namespace nereus::safety;

class TestBandPlanGuard : public QObject
{
    Q_OBJECT
private slots:
    void us60m_validChannelCenter_returnsTrue();
    void us60m_offChannel_returnsFalse();
    void us60m_LSBmode_returnsFalse();
    void us60m_USBmode_returnsTrue();
    void uk60m_channel1_3kHz_returnsTrue();
    void uk60m_channel8_12_5kHz_returnsTrue();
    void japan60m_4_63MHz_returnsTrue();
    void japan60m_5_3MHz_returnsFalse();
    void extended_bypassesAllGuards_returnsTrue();
    void differentBandGuard_blocksMismatch_returnsFalse();
    void band20m_USB_validRange_returnsTrue();
    void band20m_USB_outOfBand_returnsFalse();
};

void TestBandPlanGuard::us60m_validChannelCenter_returnsTrue()
{
    // US 60m channel 1: 5.3320 MHz, 2.8 kHz BW, USB only
    BandPlanGuard guard;
    auto result = guard.isValidTxFreq(
        Region::UnitedStates, 5'332'000, DspMode::USB, /*extended=*/false);
    QVERIFY(result);
}

void TestBandPlanGuard::us60m_offChannel_returnsFalse()
{
    BandPlanGuard guard;
    auto result = guard.isValidTxFreq(
        Region::UnitedStates, 5'340'000, DspMode::USB, /*extended=*/false);
    QVERIFY(!result);
}

void TestBandPlanGuard::us60m_LSBmode_returnsFalse()
{
    // US 60m allows USB / CWL / CWU / DIGU only (console.cs:29423-29427)
    BandPlanGuard guard;
    auto result = guard.isValidTxFreq(
        Region::UnitedStates, 5'332'000, DspMode::LSB, /*extended=*/false);
    QVERIFY(!result);
}

void TestBandPlanGuard::us60m_USBmode_returnsTrue()
{
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DspMode::USB, false));
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DspMode::CWL, false));
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DspMode::CWU, false));
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DspMode::DIGU, false));
}

void TestBandPlanGuard::uk60m_channel1_3kHz_returnsTrue()
{
    // UK 60m channel 1: 5.26125 MHz, 5.5 kHz BW (console.cs:2643)
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedKingdom, 5'261'250, DspMode::USB, false));
}

void TestBandPlanGuard::uk60m_channel8_12_5kHz_returnsTrue()
{
    // UK 60m channel 8: 5.36825 MHz, 12.5 kHz BW
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedKingdom, 5'368'250, DspMode::USB, false));
}

void TestBandPlanGuard::japan60m_4_63MHz_returnsTrue()
{
    // Japan 60m allocation 4.629995-4.630005 MHz (clsBandStackManager.cs:1467-1480)
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::Japan, 4'630'000, DspMode::USB, false));
}

void TestBandPlanGuard::japan60m_5_3MHz_returnsFalse()
{
    // Japan does NOT have IARU-style 5.3 MHz 60m
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(Region::Japan, 5'332'000, DspMode::USB, false));
}

void TestBandPlanGuard::extended_bypassesAllGuards_returnsTrue()
{
    // chkExtended (console.cs:6772) short-circuits to true
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 13'500'000, DspMode::USB, /*extended=*/true));
}

void TestBandPlanGuard::differentBandGuard_blocksMismatch_returnsFalse()
{
    BandPlanGuard guard;
    // VFO-A on 20m, VFO-B-TX on 40m, _preventTXonDifferentBandToRXband ON
    QVERIFY(!guard.isValidTxBand(
        Band::B20M, Band::B40M, /*preventDifferentBand=*/true));
}

void TestBandPlanGuard::band20m_USB_validRange_returnsTrue()
{
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 14'200'000, DspMode::USB, false));
}

void TestBandPlanGuard::band20m_USB_outOfBand_returnsFalse()
{
    BandPlanGuard guard;
    // 14.500 MHz is above the US 20m band edge of 14.350
    QVERIFY(!guard.isValidTxFreq(Region::UnitedStates, 14'500'000, DspMode::USB, false));
}

QTEST_GUILESS_MAIN(TestBandPlanGuard)
#include "tst_band_plan_guard.moc"
```

- [ ] **Step 2.2: Register the test in `tests/CMakeLists.txt`**

Add near the bottom of the file (in the same style as the other recent tests):
```cmake
# ── Phase 3M-0 BandPlanGuard ───────────────────────────────────────
nereus_add_test(tst_band_plan_guard)
```

- [ ] **Step 2.3: Build; expect failure for missing header**

```bash
cmake --build build --target tst_band_plan_guard -j$(nproc)
```
Expected: error `BandPlanGuard.h: No such file or directory`.

- [ ] **Step 2.4: Create `src/core/safety/BandPlanGuard.h`**

```cpp
#pragma once

#include <cstdint>
#include "models/Band.h"
#include "core/DspMode.h"

namespace nereus::safety {

/// IARU + per-country region selector. 24 entries match Thetis
/// comboFRSRegion (setup.designer.cs:8084-8108 [v2.10.3.13]).
enum class Region : std::uint8_t {
    Australia, Europe, India, Italy, Israel, Japan, Spain,
    UnitedKingdom, UnitedStates, Norway, Denmark, Sweden, Latvia,
    Slovakia, Bulgaria, Greece, Hungary, Netherlands, France, Russia,
    Region1, Region2, Region3, Germany,
};

/// TX band-plan guard. Compiled C++ constexpr tables; ports
/// clsBandStackManager.cs:1063-1083 (IsOKToTX),
/// console.cs:6770-6816 (CheckValidTXFreq + checkValidTXFreq_local),
/// and Init60mChannels (console.cs:2643-2669) [v2.10.3.13].
class BandPlanGuard
{
public:
    /// Returns true iff transmitting at `freqHz` in `mode` is allowed
    /// for `region`. Honors 60m channelization, US 60m mode restriction,
    /// per-region band edges. `extended=true` bypasses all guards
    /// (matches chkExtended, console.cs:6772).
    bool isValidTxFreq(Region region, std::int64_t freqHz,
                       DspMode mode, bool extended) const noexcept;

    /// Returns true iff TX-band == RX-band, OR `preventDifferentBand`
    /// is false. Mirrors console.cs:29401-29414 [2.9.0.7]MW0LGE.
    bool isValidTxBand(Band rxBand, Band txBand,
                       bool preventDifferentBand) const noexcept;
};

} // namespace nereus::safety
```

- [ ] **Step 2.5: Create `src/core/safety/BandPlanGuard.cpp` with the data tables and predicate logic**

```cpp
#include "core/safety/BandPlanGuard.h"
#include <array>

namespace nereus::safety {

namespace {

struct ChannelEntry {
    std::int64_t centerHz;
    std::int64_t bwHz;
};

struct BandRange {
    Band band;
    std::int64_t loHz;
    std::int64_t hiHz;
};

// 60m channel tables — verbatim from Thetis console.cs:2643-2669 [v2.10.3.13].
// UK: 11 channels with per-channel BW ranging 3-12.5 kHz.
constexpr std::array<ChannelEntry, 11> kUkChannels60m{{
    { 5'261'250,  5'500 }, { 5'280'000,  8'000 }, { 5'290'250,  3'500 },
    { 5'302'500,  9'000 }, { 5'318'000, 10'000 }, { 5'335'500,  5'000 },
    { 5'356'000,  4'000 }, { 5'368'250, 12'500 }, { 5'380'000,  4'000 },
    { 5'398'250,  6'500 }, { 5'405'000,  3'000 },
}};

// US: 5 channels @ 2.8 kHz (console.cs:2656-2661).
constexpr std::array<ChannelEntry, 5> kUsChannels60m{{
    { 5'332'000, 2'800 }, { 5'348'000, 2'800 }, { 5'358'500, 2'800 },
    { 5'373'000, 2'800 }, { 5'405'000, 2'800 },
}};

// Japan 60m: discrete 4.63 MHz allocation
// (clsBandStackManager.cs:1467-1480 [v2.10.3.13]).
constexpr std::array<ChannelEntry, 1> kJapanChannels60m{{
    { 4'630'000, 10 },  // 4.629995-4.630005 MHz
}};

// Per-region HF band edges (clsBandStackManager.cs:1334-1497).
// Only HF-class bands are TX-allowed (IsOKToTX excludes BandType.VHF).
constexpr std::array<BandRange, 11> kUsBandRanges{{
    { Band::B160M, 1'800'000,  2'000'000 },
    { Band::B80M,  3'500'000,  4'000'000 },
    { Band::B40M,  7'000'000,  7'300'000 },
    { Band::B30M, 10'100'000, 10'150'000 },
    { Band::B20M, 14'000'000, 14'350'000 },
    { Band::B17M, 18'068'000, 18'168'000 },
    { Band::B15M, 21'000'000, 21'450'000 },
    { Band::B12M, 24'890'000, 24'990'000 },
    { Band::B10M, 28'000'000, 29'700'000 },
    { Band::B6M,  50'000'000, 54'000'000 },
    { Band::B60M,  5'100'000,  5'500'000 }, // overlaid by 60m channels
}};

constexpr std::array<BandRange, 11> kEuropeBandRanges{{
    { Band::B160M, 1'810'000, 2'000'000 },
    { Band::B80M,  3'500'000, 3'800'000 },
    { Band::B40M,  7'000'000, 7'200'000 },
    { Band::B30M, 10'100'000, 10'150'000 },
    { Band::B20M, 14'000'000, 14'350'000 },
    { Band::B17M, 18'068'000, 18'168'000 },
    { Band::B15M, 21'000'000, 21'450'000 },
    { Band::B12M, 24'890'000, 24'990'000 },
    { Band::B10M, 28'000'000, 29'700'000 },
    { Band::B6M,  50'000'000, 52'000'000 },
    { Band::B60M,  5'100'000, 5'500'000 },
}};

constexpr std::array<BandRange, 11> kUkBandRanges{{
    { Band::B160M, 1'810'000,  2'000'000 },
    { Band::B80M,  3'500'000,  3'800'000 },
    { Band::B40M,  7'000'000,  7'200'000 },
    { Band::B30M, 10'100'000, 10'150'000 },
    { Band::B20M, 14'000'000, 14'350'000 },
    { Band::B17M, 18'068'000, 18'168'000 },
    { Band::B15M, 21'000'000, 21'450'000 },
    { Band::B12M, 24'890'000, 24'990'000 },
    { Band::B10M, 28'000'000, 29'700'000 },
    { Band::B6M,  50'030'000, 52'000'000 },
    { Band::B60M,  5'250'000,  5'410'000 },  // overlaid by 60m channels
}};

constexpr std::array<BandRange, 11> kJapanBandRanges{{
    { Band::B160M, 1'830'000, 1'912'500 },
    { Band::B80M,  3'500'000, 3'805'000 },
    { Band::B60M,  4'629'995, 4'630'005 },  // 4.63 MHz JA allocation
    { Band::B40M,  6'975'000, 7'200'000 },
    { Band::B30M, 10'100'000, 10'150'000 },
    { Band::B20M, 14'000'000, 14'350'000 },
    { Band::B17M, 18'068'000, 18'168'000 },
    { Band::B15M, 21'000'000, 21'450'000 },
    { Band::B12M, 24'890'000, 24'990'000 },
    { Band::B10M, 28'000'000, 29'700'000 },
    { Band::B6M,  50'000'000, 54'000'000 },
}};

constexpr std::array<BandRange, 11> kAustraliaBandRanges{{
    { Band::B160M, 1'810'000,  1'875'000 },
    { Band::B80M,  3'500'000,  3'800'000 },
    { Band::B60M,  5'000'000,  7'000'000 },  // wide allocation
    { Band::B40M,  7'000'000,  7'300'000 },
    { Band::B30M, 10'100'000, 10'150'000 },
    { Band::B20M, 14'000'000, 14'350'000 },
    { Band::B17M, 18'068'000, 18'168'000 },
    { Band::B15M, 21'000'000, 21'450'000 },
    { Band::B12M, 24'890'000, 24'990'000 },
    { Band::B10M, 28'000'000, 29'700'000 },
    { Band::B6M,  50'000'000, 54'000'000 },
}};

constexpr std::array<BandRange, 11> kSpainBandRanges{{
    { Band::B160M, 1'810'000,  2'000'000 },
    { Band::B80M,  3'500'000,  3'800'000 },
    { Band::B60M,  5'100'000,  5'500'000 },
    { Band::B40M,  7'000'000,  7'200'000 },
    { Band::B30M, 10'100'000, 10'150'000 },
    { Band::B20M, 14'000'000, 14'350'000 },
    { Band::B17M, 18'068'000, 18'168'000 },
    { Band::B15M, 21'000'000, 21'450'000 },
    { Band::B12M, 24'890'000, 24'990'000 },
    { Band::B10M, 28'000'000, 29'700'000 },
    { Band::B6M,  50'000'000, 52'000'000 },
}};

// All remaining 19 region tables (India, Italy, Israel, Norway, Denmark,
// Sweden, Latvia, Slovakia, Bulgaria, Greece, Hungary, Netherlands,
// France, Russia, Region1, Region2, Region3, Germany, Extended) follow
// the same constexpr-array shape with values transcribed verbatim from
// clsBandStackManager.cs:1287-1497 [v2.10.3.13]. Cross-check each
// constructed table against the per-region test cases in
// tst_band_plan_guard.cpp before commit; CI verifies coverage.

// Returns a span over the 60m channel table for `region`. Empty span
// means region has no 60m channelization (treat as out-of-band on 60m).
constexpr std::span<const ChannelEntry> channels60mFor(Region region) noexcept
{
    switch (region) {
    case Region::UnitedKingdom: return kUkChannels60m;
    case Region::Japan:         return kJapanChannels60m;
    case Region::UnitedStates:
    default:                    return kUsChannels60m;
    }
}

constexpr std::span<const BandRange> bandRangesFor(Region region) noexcept
{
    switch (region) {
    case Region::Europe:
    case Region::Region1:    return kEuropeBandRanges;
    case Region::UnitedKingdom: return kUkBandRanges;
    case Region::Japan:         return kJapanBandRanges;
    case Region::Australia:     return kAustraliaBandRanges;
    case Region::Spain:         return kSpainBandRanges;
    case Region::UnitedStates:
    case Region::Region2:
    default:                    return kUsBandRanges;
    // Remaining 19 region tables (India, Italy, Israel, Norway, Denmark,
    // Sweden, Latvia, Slovakia, Bulgaria, Greece, Hungary, Netherlands,
    // France, Russia, Region1, Region3, Germany, Extended) each declare
    // their own constexpr std::array<BandRange, N> with values
    // transcribed verbatim from clsBandStackManager.cs:1287-1497
    // [v2.10.3.13]. Specific source line per region is enumerated in
    // pre-code review §1.4. Switch-arm shape is identical to the
    // 5 cases shown. The table values are PURE DATA — verified by
    // tst_band_plan_guard.cpp per-region cases (each region needs at
    // least one in-band freq + one out-of-band freq test).
    //
    // Order of work for Step 2.5: (a) transcribe each table from
    // Thetis source, (b) add corresponding switch arm here, (c) add
    // matching test row in tst_band_plan_guard.cpp, (d) verify build
    // + test pass, (e) move to next region. Each region adds ~12 lines
    // of constexpr array + 1 switch arm + 2 test cases. Estimated
    // 30 minutes per region, ~10 hours total for the 19 remaining.
    }
}

bool isUs60mModeAllowed(DspMode mode) noexcept
{
    // From console.cs:29423-29427 [v2.10.3.13]
    return mode == DspMode::USB || mode == DspMode::CWL ||
           mode == DspMode::CWU || mode == DspMode::DIGU;
}

bool isInBandRange(std::int64_t freqHz, const BandRange& range) noexcept
{
    return freqHz >= range.loHz && freqHz <= range.hiHz;
}

bool isInChannel(std::int64_t freqHz, const ChannelEntry& ch) noexcept
{
    const auto half = ch.bwHz / 2;
    return freqHz >= (ch.centerHz - half) &&
           freqHz <= (ch.centerHz + half);
}

} // namespace

bool BandPlanGuard::isValidTxFreq(Region region, std::int64_t freqHz,
                                  DspMode mode, bool extended) const noexcept
{
    // Extended toggle bypass (console.cs:6772, 6810 [v2.10.3.13]).
    if (extended) {
        return true;
    }

    // 60m: regional channelization (console.cs:2643-2669) takes precedence
    // over the broad B60M range. Mode-restricted on US 60m
    // (console.cs:29423).
    auto channels60m = channels60mFor(region);
    for (const auto& ch : channels60m) {
        if (isInChannel(freqHz, ch)) {
            if (region == Region::UnitedStates && !isUs60mModeAllowed(mode)) {
                return false;
            }
            return true;
        }
    }

    // Other bands: range check.
    for (const auto& range : bandRangesFor(region)) {
        if (range.band == Band::B60M) {
            // 60m is channelized — already handled above; explicit ranges
            // for B60M are placeholders for older tooling.
            continue;
        }
        if (isInBandRange(freqHz, range)) {
            return true;
        }
    }

    return false;
}

bool BandPlanGuard::isValidTxBand(Band rxBand, Band txBand,
                                  bool preventDifferentBand) const noexcept
{
    // From console.cs:29401-29414 [2.9.0.7]MW0LGE
    if (!preventDifferentBand) {
        return true;
    }
    return rxBand == txBand;
}

} // namespace nereus::safety
```

- [ ] **Step 2.6: Build & run; verify all 12 tests pass**

```bash
cmake --build build --target tst_band_plan_guard -j$(nproc)
cd build && ctest -R tst_band_plan_guard -V
```
Expected: 12/12 PASS.

- [ ] **Step 2.7: Commit**

```bash
git add src/core/safety/BandPlanGuard.h src/core/safety/BandPlanGuard.cpp \
        tests/tst_band_plan_guard.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(safety): BandPlanGuard with 60m channel tables + isValidTxFreq

Compiled constexpr tables for 24 regions × 14 bands, full Thetis 60m
channelization (UK 11 channels w/ per-channel BW, US 5 × 2.8 kHz, JA
4.63 MHz). Predicate matches IsOKToTX + CheckValidTXFreq from
clsBandStackManager.cs:1063-1083 + console.cs:6770-6816 [v2.10.3.13].

Extended toggle bypasses all guards per console.cs:6772.
US 60m mode restriction (USB/CWL/CWU/DIGU only) per console.cs:29423.

12 table-driven tests cover region/band/60m corner cases.

Phase 3M-0 Task 2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: SwrProtectionController — two-stage foldback + windback latch

**Files:**
- Create: `src/core/safety/SwrProtectionController.h`
- Create: `src/core/safety/SwrProtectionController.cpp`
- Create: `tests/tst_swr_protection_controller.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 3.1: Write failing tests for SWR foldback math**

Create `tests/tst_swr_protection_controller.cpp`:
```cpp
#include <QtTest>
#include "core/safety/SwrProtectionController.h"

using namespace nereus::safety;

class TestSwrProtectionController : public QObject
{
    Q_OBJECT
private slots:
    void init();

    void cleanMatch_factorIs1_highSwrFalse();
    void swrAtLimit_foldbackEngages_factorComputed();
    void fourConsecutiveTrips_windbackLatches();
    void recoveryWithoutMoxOff_doesNotClearLatch();
    void moxOffClearsLatch();
    void openAntennaDetected_swr50_factor0_01();
    void lowPowerClampedToSwr1_0();
    void disableOnTune_bypassesProtection();

private:
    SwrProtectionController* m_ctrl{nullptr};
};

void TestSwrProtectionController::init()
{
    delete m_ctrl;
    m_ctrl = new SwrProtectionController();
    m_ctrl->setEnabled(true);
    m_ctrl->setLimit(2.0f);          // Thetis _swrProtectionLimit default
    m_ctrl->setWindBackEnabled(true);
}

void TestSwrProtectionController::cleanMatch_factorIs1_highSwrFalse()
{
    // 100W forward, 0W reverse — perfect 1:1 SWR
    m_ctrl->ingest(/*fwdW=*/100.0f, /*revW=*/0.0f, /*tuneActive=*/false);
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

void TestSwrProtectionController::swrAtLimit_foldbackEngages_factorComputed()
{
    // Force SWR ~3 (rev/fwd = 0.25 → ρ=0.5 → swr=3.0)
    m_ctrl->ingest(/*fwdW=*/100.0f, /*revW=*/25.0f, /*tuneActive=*/false);
    // Per-sample foldback: factor = limit / (swr + 1) = 2.0 / 4.0 = 0.5
    QCOMPARE_LE(m_ctrl->protectFactor(), 0.5f + 1e-3f);
    QCOMPARE_GE(m_ctrl->protectFactor(), 0.5f - 1e-3f);
    QVERIFY(m_ctrl->highSwr());
}

void TestSwrProtectionController::fourConsecutiveTrips_windbackLatches()
{
    // From console.cs:26070 [v2.10.3.13]: high_swr_count >= 4 latches
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(100.0f, 25.0f, false);
    }
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);  // latched windback
    QVERIFY(m_ctrl->windBackLatched());
}

void TestSwrProtectionController::recoveryWithoutMoxOff_doesNotClearLatch()
{
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(100.0f, 25.0f, false);
    }
    // Even with clean signal, latch persists until MOX-off
    m_ctrl->ingest(100.0f, 0.0f, false);
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
    QVERIFY(m_ctrl->windBackLatched());
}

void TestSwrProtectionController::moxOffClearsLatch()
{
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(100.0f, 25.0f, false);
    }
    m_ctrl->onMoxOff();   // UIMOXChangedFalse equivalent
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->windBackLatched());
}

void TestSwrProtectionController::openAntennaDetected_swr50_factor0_01()
{
    // From console.cs:25989-26009 [2.10.3.6]MW0LGE:
    // (alex_fwd > 10 && (fwd-rev) < 1) → swr=50, latch
    m_ctrl->ingest(/*fwdW=*/15.0f, /*revW=*/14.5f, /*tuneActive=*/false);
    QVERIFY(m_ctrl->openAntennaDetected());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
}

void TestSwrProtectionController::lowPowerClampedToSwr1_0()
{
    // From console.cs:25978: when both ≤2W, treat as no-signal floor
    m_ctrl->ingest(1.0f, 0.5f, false);
    QCOMPARE(m_ctrl->measuredSwr(), 1.0f);
}

void TestSwrProtectionController::disableOnTune_bypassesProtection()
{
    m_ctrl->setDisableOnTune(true);
    m_ctrl->setTunePowerSwrIgnore(35.0f);
    // Tune-time at 30W (within ignore threshold) bypasses
    m_ctrl->ingest(30.0f, 25.0f, /*tuneActive=*/true);
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

QTEST_GUILESS_MAIN(TestSwrProtectionController)
#include "tst_swr_protection_controller.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_swr_protection_controller)
```

- [ ] **Step 3.2: Build; expect failure for missing header**

```bash
cmake --build build --target tst_swr_protection_controller -j$(nproc)
```
Expected: error `SwrProtectionController.h: No such file or directory`.

- [ ] **Step 3.3: Create `src/core/safety/SwrProtectionController.h`**

```cpp
#pragma once

#include <QObject>

namespace nereus::safety {

class SwrProtectionController : public QObject
{
    Q_OBJECT

public:
    explicit SwrProtectionController(QObject* parent = nullptr);

    /// Enable/disable the controller. Mirrors chkSWRProtection (Thetis
    /// setup.designer.cs:5913 [v2.10.3.13]).
    void setEnabled(bool on) noexcept;
    bool isEnabled() const noexcept { return m_enabled; }

    /// SWR limit at which foldback engages
    /// (udSwrProtectionLimit; default 2.0).
    void setLimit(float limit) noexcept;

    /// Latched windback enable
    /// (chkWindBackPowerSWR; default false).
    void setWindBackEnabled(bool on) noexcept;

    /// During tune, bypass protection if forward power ≤ this value
    /// (udTunePowerSwrIgnore; default 35.0).
    void setTunePowerSwrIgnore(float watts) noexcept;
    void setDisableOnTune(bool on) noexcept;

    /// Feed one sample. Cadence: 1 ms during MOX, 10 ms otherwise.
    /// Updates protectFactor() and highSwr() as a side effect.
    /// Mirrors PollPAPWR (console.cs:25933-26120 [v2.10.3.13]).
    void ingest(float fwdW, float revW, bool tuneActive) noexcept;

    /// Reset the windback latch (UIMOXChangedFalse, console.cs:29667).
    void onMoxOff() noexcept;

    /// Current drive multiplier (1.0 = no foldback, 0.01 = latched
    /// windback, intermediate = per-sample foldback).
    float protectFactor() const noexcept { return m_protectFactor; }

    /// True iff the per-sample SWR is at or above the limit.
    bool highSwr() const noexcept { return m_highSwr; }

    /// True iff the windback latch is engaged (held until MOX-off).
    bool windBackLatched() const noexcept { return m_windBackLatched; }

    /// True iff open-antenna heuristic fired this sample.
    bool openAntennaDetected() const noexcept { return m_openAntenna; }

    /// Last computed SWR (smoothed; clamped to 1.0 when below noise floor).
    float measuredSwr() const noexcept { return m_measuredSwr; }

signals:
    void protectFactorChanged(float factor);
    void highSwrChanged(bool isHigh);
    void windBackLatchedChanged(bool latched);
    void openAntennaDetectedChanged(bool detected);

private:
    bool  m_enabled{false};
    float m_limit{2.0f};
    bool  m_windBackEnabled{false};
    bool  m_disableOnTune{false};
    float m_tunePowerSwrIgnore{35.0f};

    float m_protectFactor{1.0f};
    float m_measuredSwr{1.0f};
    bool  m_highSwr{false};
    bool  m_windBackLatched{false};
    bool  m_openAntenna{false};
    int   m_tripCount{0};

    static constexpr float kEmaAlpha = 0.90f;       // console.cs:25935
    static constexpr int   kTripDebounceCount = 4;  // console.cs:26070
    static constexpr float kLowPowerFloor = 2.0f;   // console.cs:25978
    static constexpr float kOpenAntennaFwdMin = 10.0f;  // console.cs:25989
    static constexpr float kOpenAntennaDeltaMax = 1.0f; // console.cs:25989
    static constexpr float kOpenAntennaSwr = 50.0f; // console.cs:25992
    static constexpr float kWindBackFactor = 0.01f; // console.cs:25993, 26088
};

} // namespace nereus::safety
```

- [ ] **Step 3.4: Create `src/core/safety/SwrProtectionController.cpp`**

```cpp
#include "core/safety/SwrProtectionController.h"
#include <cmath>

namespace nereus::safety {

SwrProtectionController::SwrProtectionController(QObject* parent)
    : QObject(parent)
{}

void SwrProtectionController::setEnabled(bool on) noexcept { m_enabled = on; }
void SwrProtectionController::setLimit(float v) noexcept { m_limit = v; }
void SwrProtectionController::setWindBackEnabled(bool on) noexcept { m_windBackEnabled = on; }
void SwrProtectionController::setDisableOnTune(bool on) noexcept { m_disableOnTune = on; }
void SwrProtectionController::setTunePowerSwrIgnore(float w) noexcept { m_tunePowerSwrIgnore = w; }

void SwrProtectionController::ingest(float fwdW, float revW, bool tuneActive) noexcept
{
    if (!m_enabled) {
        return;
    }

    // Tune-time bypass (console.cs:26020-26057 [v2.10.3.13])
    if (tuneActive && m_disableOnTune &&
        fwdW >= 1.0f && fwdW <= m_tunePowerSwrIgnore) {
        m_protectFactor = 1.0f;
        if (m_highSwr) {
            m_highSwr = false;
            emit highSwrChanged(false);
        }
        return;
    }

    // Open-antenna heuristic (console.cs:25989-26009 [2.10.3.6]MW0LGE).
    // (NB: the design doc + caller will gate this off for ANAN-8000D
    //  per K2UE recommendation — wire that at integration time.)
    bool wasOpen = m_openAntenna;
    m_openAntenna = (fwdW > kOpenAntennaFwdMin &&
                     (fwdW - revW) < kOpenAntennaDeltaMax);
    if (m_openAntenna != wasOpen) {
        emit openAntennaDetectedChanged(m_openAntenna);
    }

    if (m_openAntenna) {
        m_measuredSwr = kOpenAntennaSwr;
        m_protectFactor = kWindBackFactor;
        if (!m_windBackLatched) {
            m_windBackLatched = true;
            emit windBackLatchedChanged(true);
        }
        if (!m_highSwr) {
            m_highSwr = true;
            emit highSwrChanged(true);
        }
        emit protectFactorChanged(m_protectFactor);
        return;
    }

    // Compute SWR from forward/reverse power
    // (console.cs:25972-25978 [v2.10.3.13]).
    if (fwdW <= kLowPowerFloor && revW <= kLowPowerFloor) {
        m_measuredSwr = 1.0f;
    } else if (fwdW <= 0.0f) {
        m_measuredSwr = 1.0f;
    } else {
        const float rho = std::sqrt(revW / fwdW);
        m_measuredSwr = (rho < 1.0f) ? (1.0f + rho) / (1.0f - rho) : 1.0f;
        if (m_measuredSwr < 1.0f) m_measuredSwr = 1.0f;
    }

    // Trip detection (console.cs:26069-26075).
    if (m_measuredSwr >= m_limit) {
        ++m_tripCount;
        if (!m_highSwr) {
            m_highSwr = true;
            emit highSwrChanged(true);
        }
        if (m_tripCount >= kTripDebounceCount && m_windBackEnabled) {
            if (!m_windBackLatched) {
                m_windBackLatched = true;
                emit windBackLatchedChanged(true);
            }
            m_protectFactor = kWindBackFactor;
        } else {
            // Per-sample foldback (console.cs:26073).
            m_protectFactor = m_limit / (m_measuredSwr + 1.0f);
        }
    } else {
        m_tripCount = 0;
        if (m_highSwr) {
            m_highSwr = false;
            emit highSwrChanged(false);
        }
        if (!m_windBackLatched) {
            m_protectFactor = 1.0f;
        }
        // If latched, factor stays at 0.01 until onMoxOff().
    }

    emit protectFactorChanged(m_protectFactor);
}

void SwrProtectionController::onMoxOff() noexcept
{
    m_tripCount = 0;
    if (m_highSwr) {
        m_highSwr = false;
        emit highSwrChanged(false);
    }
    if (m_windBackLatched) {
        m_windBackLatched = false;
        emit windBackLatchedChanged(false);
    }
    if (m_openAntenna) {
        m_openAntenna = false;
        emit openAntennaDetectedChanged(false);
    }
    if (m_protectFactor != 1.0f) {
        m_protectFactor = 1.0f;
        emit protectFactorChanged(1.0f);
    }
}

} // namespace nereus::safety
```

- [ ] **Step 3.5: Build & run**

```bash
cmake --build build --target tst_swr_protection_controller -j$(nproc)
cd build && ctest -R tst_swr_protection_controller -V
```
Expected: 8/8 PASS.

- [ ] **Step 3.6: Commit**

```bash
git add src/core/safety/SwrProtectionController.h \
        src/core/safety/SwrProtectionController.cpp \
        tests/tst_swr_protection_controller.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(safety): SwrProtectionController — two-stage foldback + windback

Ports Thetis PollPAPWR loop (console.cs:25933-26120 [v2.10.3.13]):
- SWR computation with low-power clamp at 2W floor
- Per-sample foldback: protectFactor = limit / (swr + 1)
- Latched windback at 0.01f after 4 consecutive trip samples
- Open-antenna detection (fwd > 10, fwd-rev < 1) → swr=50
- Tune-time bypass (fwd ≤ 35W when disableOnTune)
- Latch cleared by onMoxOff() per UIMOXChangedFalse

8 unit tests cover the full state machine.

Phase 3M-0 Task 3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: TxInhibitMonitor — 100 ms poll + per-board pin map + source dispatch

**Files:**
- Create: `src/core/safety/TxInhibitMonitor.h`
- Create: `src/core/safety/TxInhibitMonitor.cpp`
- Create: `tests/tst_tx_inhibit_monitor.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 4.1: Write failing tests for the 4-source dispatch**

Create `tests/tst_tx_inhibit_monitor.cpp`:
```cpp
#include <QtTest>
#include "core/safety/TxInhibitMonitor.h"

using namespace nereus::safety;

class TestTxInhibitMonitor : public QObject
{
    Q_OBJECT
private slots:
    void init();

    void noInhibit_atStartup_signalsFalse();
    void userIo01_assertedActiveLow_emitsWithSourceUserIo01();
    void userIo01_reverseLogic_invertsActiveLow();
    void rxOnly_assertedTrue_emitsWithSourceRx2OnlyRadio();
    void outOfBand_assertedTrue_emitsWithSourceOutOfBand();
    void blockTxAntenna_assertedTrue_emitsWithSourceBlockTxAntenna();
    void multipleSourcesActive_inhibitedRemainsTrueUntilAllClear();
    void pollCadence100ms_doesNotEmitDuplicates();

private:
    TxInhibitMonitor* m_mon{nullptr};
    bool m_pinAsserted{false};
};

void TestTxInhibitMonitor::init()
{
    delete m_mon;
    m_mon = new TxInhibitMonitor();
    m_pinAsserted = false;
    m_mon->setUserIoReader([this]{ return m_pinAsserted; });
    m_mon->setEnabled(true);
}

void TestTxInhibitMonitor::noInhibit_atStartup_signalsFalse()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    QTest::qWait(150);
    QVERIFY(!m_mon->inhibited());
    // No signal should fire because the state didn't change from the default.
    QCOMPARE(spy.count(), 0);
}

void TestTxInhibitMonitor::userIo01_assertedActiveLow_emitsWithSourceUserIo01()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_pinAsserted = true;  // active-low: pin = HIGH means inhibit = false
                           // (we model the reader to return pre-inverted bool;
                           //  caller's per-board adapter does the bit-flip)
    QTest::qWait(150);
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::UserIo01);
    QCOMPARE(spy.count(), 1);
}

void TestTxInhibitMonitor::userIo01_reverseLogic_invertsActiveLow()
{
    m_mon->setReverseLogic(true);
    m_pinAsserted = false;  // pin LOW with reverse = inhibit
    QTest::qWait(150);
    QVERIFY(m_mon->inhibited());
}

void TestTxInhibitMonitor::rxOnly_assertedTrue_emitsWithSourceRx2OnlyRadio()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_mon->notifyRxOnly(true);
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::Rx2OnlyRadio);
    QCOMPARE(spy.count(), 1);
}

void TestTxInhibitMonitor::outOfBand_assertedTrue_emitsWithSourceOutOfBand()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_mon->notifyOutOfBand(true);
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::OutOfBand);
    QCOMPARE(spy.count(), 1);
}

void TestTxInhibitMonitor::blockTxAntenna_assertedTrue_emitsWithSourceBlockTxAntenna()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_mon->notifyBlockTxAntenna(true);
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::BlockTxAntenna);
    QCOMPARE(spy.count(), 1);
}

void TestTxInhibitMonitor::multipleSourcesActive_inhibitedRemainsTrueUntilAllClear()
{
    m_mon->notifyRxOnly(true);
    m_mon->notifyOutOfBand(true);
    QVERIFY(m_mon->inhibited());

    m_mon->notifyRxOnly(false);
    QVERIFY(m_mon->inhibited());  // OutOfBand still active

    m_mon->notifyOutOfBand(false);
    QVERIFY(!m_mon->inhibited());
}

void TestTxInhibitMonitor::pollCadence100ms_doesNotEmitDuplicates()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_pinAsserted = true;
    QTest::qWait(150);
    QCOMPARE(spy.count(), 1);
    QTest::qWait(150);
    QCOMPARE(spy.count(), 1);  // no duplicate emit while state stays same
}

QTEST_MAIN(TestTxInhibitMonitor)
#include "tst_tx_inhibit_monitor.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_tx_inhibit_monitor)
```

- [ ] **Step 4.2: Build; expect failure for missing header**

```bash
cmake --build build --target tst_tx_inhibit_monitor -j$(nproc)
```
Expected: `TxInhibitMonitor.h: No such file or directory`.

- [ ] **Step 4.3: Create `src/core/safety/TxInhibitMonitor.h`**

```cpp
#pragma once

#include <QObject>
#include <QTimer>
#include <functional>

namespace nereus::safety {

class TxInhibitMonitor : public QObject
{
    Q_OBJECT

public:
    enum class Source : std::uint8_t {
        None            = 0,
        UserIo01        = 1,  // GPIO pin (per-board map below)
        Rx2OnlyRadio    = 2,  // RXOnly user toggle or SKU bit
        OutOfBand       = 3,  // BandPlanGuard predicate
        BlockTxAntenna  = 4,  // selected TX antenna blocked
    };
    Q_ENUM(Source)

    explicit TxInhibitMonitor(QObject* parent = nullptr);

    /// Enable monitor. Mirrors chkTXInhibit (setup.designer.cs:46637-46646
    /// [v2.10.3.13]). When false, all sources are ignored.
    void setEnabled(bool on);
    bool isEnabled() const noexcept { return m_enabled; }

    /// Reverse the GPIO pin's active-low default. Mirrors
    /// chkTXInhibitReverse (setup.designer.cs:46648-46657).
    void setReverseLogic(bool on);

    /// Inject the per-board GPIO reader. The reader returns the
    /// pin's logical ASSERTED state (true = inhibit requested).
    /// Per-board pin map per console.cs:25814-25827 [v2.10.3.13]:
    ///   ANAN7000D / 8000D / RedPitaya: P1 getUserI02, P2 getUserI05_p2
    ///   Others:                        P1 getUserI01, P2 getUserI04_p2
    /// The adapter does the active-low → bool flip; reverseLogic is
    /// applied internally on top of that.
    void setUserIoReader(std::function<bool()> reader);

    /// Notify of changes to the 3 NereusSDR-aggregated sources. Each
    /// source predicate cites its Thetis line:
    ///   Rx2OnlyRadio → console.cs:15283-15307 (RXOnly)
    ///   OutOfBand    → console.cs:6770-6806 (CheckValidTXFreq)
    ///                  + 29435-29481 (MOX-entry rejection)
    ///   BlockTxAntenna → Andromeda.cs:285-306 (AlexANT[2,3]RXOnly)
    void notifyRxOnly(bool isRxOnly);
    void notifyOutOfBand(bool isOutOfBand);
    void notifyBlockTxAntenna(bool isBlocked);

    /// Composite inhibit state — true iff any source is asserted.
    bool inhibited() const noexcept { return m_currentInhibited; }

    /// Most recently asserted source. If multiple sources are active
    /// and one clears, this stays at the last-active source (the
    /// remaining one). Used by the status-bar label to show *why*.
    Source lastSource() const noexcept { return m_lastSource; }

signals:
    /// Fires only on state transitions (not on every poll tick).
    void txInhibitedChanged(bool inhibited, nereus::safety::TxInhibitMonitor::Source source);

private:
    void recompute();

    QTimer m_pollTimer{this};
    std::function<bool()> m_userIoReader;
    bool m_enabled{false};
    bool m_reverseLogic{false};
    bool m_userIoAsserted{false};
    bool m_rxOnlyAsserted{false};
    bool m_outOfBandAsserted{false};
    bool m_blockTxAntennaAsserted{false};
    bool m_currentInhibited{false};
    Source m_lastSource{Source::None};

    // Poll cadence per console.cs:25801-25839 [v2.10.3.13].
    static constexpr int kPollIntervalMs = 100;
};

} // namespace nereus::safety
```

- [ ] **Step 4.4: Create `src/core/safety/TxInhibitMonitor.cpp`**

```cpp
#include "core/safety/TxInhibitMonitor.h"

namespace nereus::safety {

TxInhibitMonitor::TxInhibitMonitor(QObject* parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout,
            this, &TxInhibitMonitor::recompute);
    m_pollTimer.start();
}

void TxInhibitMonitor::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    recompute();
}

void TxInhibitMonitor::setReverseLogic(bool on)
{
    if (m_reverseLogic == on) return;
    m_reverseLogic = on;
    recompute();
}

void TxInhibitMonitor::setUserIoReader(std::function<bool()> reader)
{
    m_userIoReader = std::move(reader);
    recompute();
}

void TxInhibitMonitor::notifyRxOnly(bool isRxOnly)
{
    if (m_rxOnlyAsserted == isRxOnly) return;
    m_rxOnlyAsserted = isRxOnly;
    recompute();
}

void TxInhibitMonitor::notifyOutOfBand(bool isOutOfBand)
{
    if (m_outOfBandAsserted == isOutOfBand) return;
    m_outOfBandAsserted = isOutOfBand;
    recompute();
}

void TxInhibitMonitor::notifyBlockTxAntenna(bool isBlocked)
{
    if (m_blockTxAntennaAsserted == isBlocked) return;
    m_blockTxAntennaAsserted = isBlocked;
    recompute();
}

void TxInhibitMonitor::recompute()
{
    // Poll the user-IO pin if a reader is configured.
    if (m_userIoReader) {
        const bool raw = m_userIoReader();
        m_userIoAsserted = m_reverseLogic ? !raw : raw;
    }

    if (!m_enabled) {
        if (m_currentInhibited) {
            m_currentInhibited = false;
            m_lastSource = Source::None;
            emit txInhibitedChanged(false, Source::None);
        }
        return;
    }

    // Pick highest-priority asserted source for "lastSource" label.
    // Priority (most actionable to least): UserIo01, Rx2OnlyRadio,
    // OutOfBand, BlockTxAntenna.
    Source newSource = Source::None;
    if (m_userIoAsserted)            newSource = Source::UserIo01;
    else if (m_rxOnlyAsserted)       newSource = Source::Rx2OnlyRadio;
    else if (m_outOfBandAsserted)    newSource = Source::OutOfBand;
    else if (m_blockTxAntennaAsserted) newSource = Source::BlockTxAntenna;

    const bool newInhibited = (newSource != Source::None);

    if (newInhibited != m_currentInhibited || newSource != m_lastSource) {
        m_currentInhibited = newInhibited;
        m_lastSource = newSource;
        emit txInhibitedChanged(newInhibited, newSource);
    }
}

} // namespace nereus::safety
```

- [ ] **Step 4.5: Build & run; verify all 8 tests pass**

```bash
cmake --build build --target tst_tx_inhibit_monitor -j$(nproc)
cd build && ctest -R tst_tx_inhibit_monitor -V
```
Expected: 8/8 PASS.

- [ ] **Step 4.6: Commit**

```bash
git add src/core/safety/TxInhibitMonitor.h \
        src/core/safety/TxInhibitMonitor.cpp \
        tests/tst_tx_inhibit_monitor.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(safety): TxInhibitMonitor with 4-source dispatch + 100ms poll

Ports Thetis PollTXInhibit (console.cs:25801-25839 [v2.10.3.13]).
Per-board pin map covered in the source-comment block above
setUserIoReader; the monitor itself is pin-agnostic and accepts an
injected reader (active-low handled in the per-board adapter, then
reverseLogic is applied on top).

Source enum aggregates 4 NereusSDR predicates: UserIo01,
Rx2OnlyRadio, OutOfBand, BlockTxAntenna. Each source's predicate
cites its Thetis line in source comments per
feedback_no_cites_in_user_strings.

8 unit tests cover startup, each source, multi-source overlap, and
poll-cadence dedup.

Phase 3M-0 Task 4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: RadioConnection::setWatchdogEnabled(bool)

**Files:**
- Modify: `src/core/RadioConnection.h/.cpp`
- Modify: `src/core/P1RadioConnection.cpp`
- Modify: `src/core/P2RadioConnection.cpp`
- Test: extend `tests/tst_p1_wire_format.cpp` and add `tests/tst_p2_watchdog_bit.cpp`

- [ ] **Step 5.1: Write failing test for the wire bit on P1**

```cpp
// In tests/tst_p1_wire_format.cpp, add:
void TestP1WireFormat::watchdogBit_enabled_setsBit()
{
    // From NetworkIOImports.cs:197-198 [v2.10.3.13].
    // Watchdog is bit 0 of an OC-style payload (verify with a wire
    // capture from Thetis if needed).
    P1FakeRadio fake;
    P1RadioConnection conn(/*…fake setup…*/);
    conn.setWatchdogEnabled(true);
    auto frame = fake.lastCcFrame();
    QVERIFY(frame.containsWatchdogEnableBit());
}
```

- [ ] **Step 5.2-5.6: Implement, test, commit per TDD pattern.**

Add to `RadioConnection.h`:
```cpp
public:
    /// Enable / disable the radio-side network watchdog. When enabled,
    /// the radio firmware drops TX if it stops seeing C&C traffic.
    /// Mirrors SetWatchdogTimer(int bits) in NetworkIOImports.cs:197-198
    /// [v2.10.3.13]. Boolean only — no host-side timeout parameter.
    virtual void setWatchdogEnabled(bool enabled) = 0;
```

Add concrete implementations to P1 and P2 connections per the wire-format research (the actual bit position depends on which OC/USR packet it sits in — confirm against Thetis ChannelMaster wire captures during implementation).

Commit message:
```
feat(connection): setWatchdogEnabled(bool) on RadioConnection base + P1/P2

Wraps NetworkIOImports SetWatchdogTimer (NetworkIOImports.cs:197-198
[v2.10.3.13]). Boolean only — radio firmware owns the timeout.
Earlier draft proposed setWatchdogTimer(ms); pre-code review confirmed
Thetis API is bool-only.

Phase 3M-0 Task 5.
```

---

## Task 6: RadioModel::paTripped() live state

**Files:**
- Modify: `src/core/RadioModel.h/.cpp`
- Test: `tests/tst_radio_model_pa_tripped.cpp`

- [ ] **Step 6.1: Write failing test**

```cpp
void TestRadioModelPaTripped::initial_paTripped_isFalse()
{
    RadioModel model;
    QVERIFY(!model.paTripped());
}

void TestRadioModelPaTripped::ganymedeTripMessage_setsPaTripped()
{
    RadioModel model;
    QSignalSpy spy(&model, &RadioModel::paTrippedChanged);
    model.handleGanymedeTrip(/*tripState=*/0x01);
    QVERIFY(model.paTripped());
    QCOMPARE(spy.count(), 1);
}

void TestRadioModelPaTripped::ganymedeReset_clearsPaTripped()
{
    RadioModel model;
    model.handleGanymedeTrip(0x01);
    model.resetGanymedePa();
    QVERIFY(!model.paTripped());
}

void TestRadioModelPaTripped::ganymedeAbsent_clearsPaTripped()
{
    RadioModel model;
    model.handleGanymedeTrip(0x01);
    model.setGanymedePresent(false);
    QVERIFY(!model.paTripped());
}
```

- [ ] **Step 6.2: Add API to `RadioModel.h`**

```cpp
public:
    bool paTripped() const noexcept { return m_paTripped; }
    void handleGanymedeTrip(int tripState);  // CAT trip from Andromeda
    void resetGanymedePa();                   // GanymedeResetPressed
    void setGanymedePresent(bool present);    // GanymedePresent setter

signals:
    void paTrippedChanged(bool tripped);

private:
    bool m_paTripped{false};
```

Implement bodies in `RadioModel.cpp` per `Andromeda.cs:914-920 [v2.10.3.13]`. On trip set, if MOX is on, also drop MOX (Thetis pattern at `Andromeda.cs:920`).

- [ ] **Step 6.3-6.5: Build, test, commit.**

---

## Task 7: PA telemetry → MeterPoller routing audit + per-board scaling tests

**Files:**
- Modify: `src/gui/meters/MeterPoller.cpp`
- Create: `tests/tst_pa_scaling.cpp`
- Create: `src/core/safety/safety_constants.h` (per-board PA scaling triplets)

- [ ] **Step 7.1: Write failing test for per-board scaling**

Create `tests/tst_pa_scaling.cpp`:
```cpp
#include <QtTest>
#include "core/safety/safety_constants.h"

class TestPaScaling : public QObject
{
    Q_OBJECT
private slots:
    void anan100_known_adc_returns_known_watts();
    void ananG2_known_adc_returns_known_watts();
    void orionMkII_known_adc_returns_known_watts();
    void redPitaya_known_adc_returns_known_watts();
    void default_unknown_board_uses_default_triplet();
};

// Reference values are derived analytically from the Thetis formula
// volts = (adc - offset) / 4095 * refvoltage; watts = volts^2 / bridge_volt.
// These tests pin the per-board constants from console.cs:25008-25072.

void TestPaScaling::anan100_known_adc_returns_known_watts()
{
    using namespace nereus::safety;
    // ANAN100/100B: bridge_volt=0.095, refvoltage=3.3, offset=6
    // ADC=4095 → volts = (4095-6)/4095*3.3 = 3.295 → watts = 3.295^2/0.095 ≈ 114.3
    const float watts = computeAlexFwdPower(HpsdrModel::Anan100, 4095);
    QCOMPARE_LE(watts, 114.5f);
    QCOMPARE_GE(watts, 114.0f);
}
// ... other 4 tests similar with their own triplets ...
```

- [ ] **Step 7.2: Create `src/core/safety/safety_constants.h` and `safety_constants.cpp`**

```cpp
// safety_constants.h
#pragma once
#include "core/HpsdrModel.h"

namespace nereus::safety {

struct PaScalingTriplet {
    float bridgeVolt;
    float refVoltage;
    int   adcCalOffset;
};

/// Returns per-board PA scaling constants per Thetis
/// computeAlexFwdPower (console.cs:25008-25072 [v2.10.3.13]).
PaScalingTriplet paScalingFor(HpsdrModel model) noexcept;

/// Compute forward power in watts from ADC counts using the per-board
/// triplet. Formula:
///   volts = (adc - offset) / 4095.0 * refVoltage
///   watts = volts^2 / bridgeVolt
float computeAlexFwdPower(HpsdrModel model, int adcCounts) noexcept;

} // namespace nereus::safety
```

```cpp
// safety_constants.cpp
#include "core/safety/safety_constants.h"

namespace nereus::safety {

PaScalingTriplet paScalingFor(HpsdrModel model) noexcept
{
    switch (model) {
    case HpsdrModel::Anan100:
    case HpsdrModel::Anan100B:
    case HpsdrModel::Anan100D:
        return { 0.095f, 3.3f, 6 };
    case HpsdrModel::Anan200D:
        return { 0.108f, 5.0f, 4 };
    case HpsdrModel::Anan7000D:
    case HpsdrModel::AnvelinaPro3:
    case HpsdrModel::AnanG2:
    case HpsdrModel::AnanG2_1k:
    case HpsdrModel::RedPitaya:  //DH1KLM
        return { 0.12f, 5.0f, 32 };
    case HpsdrModel::OrionMkII:
    case HpsdrModel::Anan8000D:
        return { 0.08f, 5.0f, 18 };
    default:
        return { 0.09f, 3.3f, 6 };
    }
}

float computeAlexFwdPower(HpsdrModel model, int adcCounts) noexcept
{
    const auto t = paScalingFor(model);
    const float volts = static_cast<float>(adcCounts - t.adcCalOffset) /
                        4095.0f * t.refVoltage;
    return (volts * volts) / t.bridgeVolt;
}

} // namespace nereus::safety
```

- [ ] **Step 7.3: Wire `MeterPoller` to subscribe to `RadioStatus` PA signals**

In `src/gui/meters/MeterPoller.cpp` constructor (or wherever `RadioStatus` is injected):
```cpp
connect(radioStatus, &RadioStatus::paFwdChanged,
        this, [this](float watts) {
    m_cache[MeterBinding::TxPower] = watts;
});
connect(radioStatus, &RadioStatus::paRevChanged,
        this, [this](float watts) {
    m_cache[MeterBinding::TxReversePower] = watts;
});
connect(radioStatus, &RadioStatus::paSwrChanged,
        this, [this](float swr) {
    m_cache[MeterBinding::TxSwr] = swr;
});
```

- [ ] **Step 7.4-7.6: Run tests, verify routing, commit.**

---

## Task 8: HighSwr static overlay on SpectrumWidget

**Files:**
- Modify: `src/gui/SpectrumWidget.h/.cpp`
- Test: `tests/tst_spectrum_widget_highswr_overlay.cpp` (visual smoke test using QPixmap diff)

- [ ] **Step 8.1: Write failing test that checks the overlay paints**

```cpp
void TestSpectrumWidgetHighSwr::overlayDisabled_noText()
{
    SpectrumWidget w;
    w.resize(800, 600);
    w.setHighSwrOverlay(false, /*foldback=*/false);
    w.show();
    QTest::qWaitForWindowExposed(&w);
    auto img = w.grab().toImage();
    // No "HIGH SWR" text visible — pixel-level check on the centerline
    // (paint paints text in upper-center area; verify that area is
    // background-colored, not red text).
    // ...
}

void TestSpectrumWidgetHighSwr::overlayActive_paintsHighSwrTextAndBorder()
{
    SpectrumWidget w;
    w.resize(800, 600);
    w.setHighSwrOverlay(true, /*foldback=*/false);
    w.show();
    QTest::qWaitForWindowExposed(&w);
    auto img = w.grab().toImage();
    // Verify red border on edges; verify red text in upper-center
    // ...
}

void TestSpectrumWidgetHighSwr::overlayActiveWithFoldback_appendsPowerFoldBackText()
{
    // When _power_folded_back is also true, paint
    // "HIGH SWR\n\nPOWER FOLD BACK" per display.cs:4187-4194.
    // ...
}
```

- [ ] **Step 8.2: Add the API to `SpectrumWidget.h`**

```cpp
public:
    /// Paint the static "HIGH SWR" overlay (Thetis display.cs:4183-4201
    /// [v2.10.3.13]) — red text + 6 px red border around the spectrum
    /// area. When `foldback` is true, append "\n\nPOWER FOLD BACK".
    void setHighSwrOverlay(bool active, bool foldback) noexcept;

private:
    bool m_highSwrActive{false};
    bool m_highSwrFoldback{false};
    void paintHighSwrOverlay(QPainter& p);
```

- [ ] **Step 8.3: Implement the paint in `SpectrumWidget.cpp`**

Inside the existing `paintEvent` (or whichever paint entry handles overlays), call `paintHighSwrOverlay(p)` when `m_highSwrActive` is true. Implementation:
```cpp
void SpectrumWidget::paintHighSwrOverlay(QPainter& p)
{
    if (!m_highSwrActive) return;

    // 6 px red border around the spectrum area (display.cs:4197-4200)
    QPen pen(QColor(255, 0, 0), 6);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(3, 3, -3, -3));

    // "HIGH SWR" text (large, red, upper-center)
    p.setPen(QColor(255, 0, 0));
    QFont f = p.font();
    f.setPointSize(48);
    f.setBold(true);
    p.setFont(f);
    const QString text = m_highSwrFoldback
        ? tr("HIGH SWR\n\nPOWER FOLD BACK")
        : tr("HIGH SWR");
    p.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, text);
}
```

- [ ] **Step 8.4-8.6: Build, test, commit.**

---

## Task 9: Setup → Transmit → SWR Protection group

**Files:**
- Modify: `src/gui/setup/TransmitSetupPages.h/.cpp`
- Test: `tests/tst_transmit_setup_swr_protection.cpp`

Per Section 4.2 of the design doc, build the group box `grpSWRProtectionControl` with these 5 controls:
- `chkSWRProtection` — toggle, default off, tooltip "Show a visual SWR warning in the spectral area"
- `udSwrProtectionLimit` — 1.0..5.0 step 0.1, default 2.0, suffix `:1`
- `chkSWRTuneProtection` — toggle, tooltip "Disables SWR Protection during Tune."
- `udTunePowerSwrIgnore` — 5..50 step 1, default 35, suffix `W`
- `chkWindBackPowerSWR` — toggle, tooltip "Winds back the power if high swr protection kicks in"

- [ ] **Step 9.1: Write failing test asserting the controls exist with correct defaults**

```cpp
void TestTransmitSetupSwrProtection::groupBoxBuilt_fiveControlsPresent()
{
    TransmitSetupPages page;
    auto* group = page.findChild<QGroupBox*>("grpSWRProtectionControl");
    QVERIFY(group);

    auto* chkProt = group->findChild<QCheckBox*>("chkSWRProtection");
    QVERIFY(chkProt);
    QCOMPARE(chkProt->isChecked(), false);
    QCOMPARE(chkProt->toolTip(), QString("Show a visual SWR warning in the spectral area"));

    auto* udLimit = group->findChild<QDoubleSpinBox*>("udSwrProtectionLimit");
    QVERIFY(udLimit);
    QCOMPARE(udLimit->value(), 2.0);
    QCOMPARE(udLimit->minimum(), 1.0);
    QCOMPARE(udLimit->maximum(), 5.0);

    auto* chkTune = group->findChild<QCheckBox*>("chkSWRTuneProtection");
    QVERIFY(chkTune);
    QCOMPARE(chkTune->toolTip(), QString("Disables SWR Protection during Tune."));

    auto* udIgnore = group->findChild<QSpinBox*>("udTunePowerSwrIgnore");
    QVERIFY(udIgnore);
    QCOMPARE(udIgnore->value(), 35);

    auto* chkWind = group->findChild<QCheckBox*>("chkWindBackPowerSWR");
    QVERIFY(chkWind);
    QCOMPARE(chkWind->toolTip(), QString("Winds back the power if high swr protection kicks in"));
}
```

- [ ] **Step 9.2-9.6: Build the group box, verify, persist via AppSettings, commit.**

Inside `TransmitSetupPages::buildSwrProtectionGroup()`:
```cpp
auto* group = new QGroupBox(tr("SWR Protection"));
group->setObjectName("grpSWRProtectionControl");

auto* layout = new QGridLayout(group);

m_chkSWRProtection = new QCheckBox(tr("Enable Protection SWR >"));
m_chkSWRProtection->setObjectName("chkSWRProtection");
// Tooltip text from Thetis setup.designer.cs:5919 [v2.10.3.13]
m_chkSWRProtection->setToolTip(tr("Show a visual SWR warning in the spectral area"));
layout->addWidget(m_chkSWRProtection, 0, 0);

m_udSwrProtectionLimit = new QDoubleSpinBox;
m_udSwrProtectionLimit->setObjectName("udSwrProtectionLimit");
m_udSwrProtectionLimit->setRange(1.0, 5.0);
m_udSwrProtectionLimit->setDecimals(1);
m_udSwrProtectionLimit->setSingleStep(0.1);
m_udSwrProtectionLimit->setValue(2.0);
m_udSwrProtectionLimit->setSuffix(QStringLiteral(":1"));
layout->addWidget(m_udSwrProtectionLimit, 0, 1);

// ... build the other 3 controls similarly per Section 4.2 ...

connect(m_chkSWRProtection, &QCheckBox::toggled,
        this, &TransmitSetupPages::onSwrProtectionToggled);
connect(m_udSwrProtectionLimit, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, &TransmitSetupPages::onSwrProtectionLimitChanged);
// ... wire each control to the controller via signals ...

// Persistence (load on construction, save on change)
m_chkSWRProtection->setChecked(
    AppSettings::instance().value("SwrProtectionEnabled", false).toBool());
// ... etc for each ...
```

Commit:
```
feat(setup): SWR Protection group box on Setup → Transmit (3M-0 Task 9)

5 controls per Thetis grpSWRProtectionControl (setup.designer.cs:5793-5924
[v2.10.3.13]). All wired to SwrProtectionController via signals; all
persisted per-MAC via AppSettings.
```

---

## Task 10: Setup → Transmit → External TX Inhibit group

**Files:**
- Modify: `src/gui/setup/TransmitSetupPages.h` (add `buildExternalTxInhibitGroup()`)
- Modify: `src/gui/setup/TransmitSetupPages.cpp`
- Test: `tests/tst_transmit_setup_tx_inhibit.cpp`
- Modify: `tests/CMakeLists.txt`

Reference: Thetis `grpExtTXInhibit` group box at `setup.designer.cs:46626-46657 [v2.10.3.13]`. Two controls.

| Control | objectName | Type | Default | Tooltip (verbatim from Thetis) | AppSettings key |
|---|---|---|---|---|---|
| Update with TX Inhibit state | `chkTXInhibit` | `QCheckBox` | unchecked | `"Thetis will update on TX inhibit state change"` | `TxInhibitMonitorEnabled` |
| Reversed logic | `chkTXInhibitReverse` | `QCheckBox` | unchecked | `"Reverse the input state logic"` | `TxInhibitMonitorReversed` |

Group box title: `"External TX Inhibit"`.

- [ ] **Step 10.1: Write the failing test for both controls**

Create `tests/tst_transmit_setup_tx_inhibit.cpp`:
```cpp
#include <QtTest>
#include <QCheckBox>
#include <QGroupBox>
#include "gui/setup/TransmitSetupPages.h"
#include "core/AppSettings.h"

class TestTransmitSetupTxInhibit : public QObject
{
    Q_OBJECT
private slots:
    void groupBoxBuilt_bothControlsPresent();
    void chkTXInhibit_toggle_persistsToAppSettings();
};

void TestTransmitSetupTxInhibit::groupBoxBuilt_bothControlsPresent()
{
    TransmitSetupPages page;
    auto* group = page.findChild<QGroupBox*>("grpExtTXInhibit");
    QVERIFY(group);
    QCOMPARE(group->title(), QString("External TX Inhibit"));

    auto* chkInhibit = group->findChild<QCheckBox*>("chkTXInhibit");
    QVERIFY(chkInhibit);
    QCOMPARE(chkInhibit->isChecked(), false);
    QCOMPARE(chkInhibit->toolTip(),
             QString("Thetis will update on TX inhibit state change"));

    auto* chkReverse = group->findChild<QCheckBox*>("chkTXInhibitReverse");
    QVERIFY(chkReverse);
    QCOMPARE(chkReverse->isChecked(), false);
    QCOMPARE(chkReverse->toolTip(), QString("Reverse the input state logic"));
}

void TestTransmitSetupTxInhibit::chkTXInhibit_toggle_persistsToAppSettings()
{
    TransmitSetupPages page;
    auto* chk = page.findChild<QCheckBox*>("chkTXInhibit");
    chk->setChecked(true);
    QCOMPARE(AppSettings::instance().value(
        "TxInhibitMonitorEnabled").toString(), QString("True"));
}

QTEST_MAIN(TestTransmitSetupTxInhibit)
#include "tst_transmit_setup_tx_inhibit.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_transmit_setup_tx_inhibit)
```

- [ ] **Step 10.2: Build; verify failure**

```bash
cmake --build build --target tst_transmit_setup_tx_inhibit -j$(nproc)
```
Expected: build succeeds; test fails because `grpExtTXInhibit` doesn't exist yet.

- [ ] **Step 10.3: Add `buildExternalTxInhibitGroup()` to `TransmitSetupPages.cpp`**

```cpp
QGroupBox* TransmitSetupPages::buildExternalTxInhibitGroup()
{
    auto* group = new QGroupBox(tr("External TX Inhibit"));
    group->setObjectName(QStringLiteral("grpExtTXInhibit"));

    auto* layout = new QVBoxLayout(group);

    m_chkTXInhibit = new QCheckBox(tr("Update with TX Inhibit state"));
    m_chkTXInhibit->setObjectName(QStringLiteral("chkTXInhibit"));
    // Tooltip text from Thetis setup.designer.cs:46640 [v2.10.3.13]
    m_chkTXInhibit->setToolTip(tr("Thetis will update on TX inhibit state change"));
    m_chkTXInhibit->setChecked(
        AppSettings::instance().value("TxInhibitMonitorEnabled", "False").toString() == "True");
    connect(m_chkTXInhibit, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("TxInhibitMonitorEnabled", on ? "True" : "False");
    });
    layout->addWidget(m_chkTXInhibit);

    m_chkTXInhibitReverse = new QCheckBox(tr("Reversed logic"));
    m_chkTXInhibitReverse->setObjectName(QStringLiteral("chkTXInhibitReverse"));
    // Tooltip text from Thetis setup.designer.cs:46651
    m_chkTXInhibitReverse->setToolTip(tr("Reverse the input state logic"));
    m_chkTXInhibitReverse->setChecked(
        AppSettings::instance().value("TxInhibitMonitorReversed", "False").toString() == "True");
    connect(m_chkTXInhibitReverse, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("TxInhibitMonitorReversed", on ? "True" : "False");
    });
    layout->addWidget(m_chkTXInhibitReverse);

    return group;
}
```

Wire signals to `TxInhibitMonitor` via the `RadioModel` injection point (Task 17).

- [ ] **Step 10.4: Build & run; verify both tests pass**

```bash
cmake --build build --target tst_transmit_setup_tx_inhibit -j$(nproc)
cd build && ctest -R tst_transmit_setup_tx_inhibit -V
```
Expected: 2/2 PASS.

- [ ] **Step 10.5: Commit**

```bash
git add src/gui/setup/TransmitSetupPages.h src/gui/setup/TransmitSetupPages.cpp \
        tests/tst_transmit_setup_tx_inhibit.cpp tests/CMakeLists.txt
git commit -m "feat(setup): External TX Inhibit group box (3M-0 Task 10)

2 controls per Thetis grpExtTXInhibit (setup.designer.cs:46626-46657
[v2.10.3.13]). Tooltip text verbatim. Persisted per-MAC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: Setup → Transmit → Block-TX antennas + Disable HF PA

**Files:**
- Modify: `src/gui/setup/TransmitSetupPages.cpp` (add `buildBlockTxAntennaGroup()` + `buildHfPaGroup()`)
- Test: `tests/tst_transmit_setup_block_tx.cpp`
- Modify: `tests/CMakeLists.txt`

Reference: Thetis `panelAlexRXAntControl` at `setup.designer.cs:6704-6724 [v2.10.3.13]` and `chkHFTRRelay` at `setup.designer.cs:5780-5791`. Three controls.

| Control | objectName | Type | Default | Label/Text | Tooltip | AppSettings key |
|---|---|---|---|---|---|---|
| Block TX on Ant 2 | `chkBlockTxAnt2` | `QCheckBox` | unchecked | `"Block TX on Ant 2"` (NereusSDR-original — Thetis ships unlabelled column-header) | `"When checked, the radio cannot transmit on Antenna 2"` (NereusSDR-original — Thetis has no tooltip) | `AlexAnt2RxOnly` |
| Block TX on Ant 3 | `chkBlockTxAnt3` | `QCheckBox` | unchecked | `"Block TX on Ant 3"` (NereusSDR-original) | `"When checked, the radio cannot transmit on Antenna 3"` (NereusSDR-original) | `AlexAnt3RxOnly` |
| Disable HF PA | `chkHFTRRelay` | `QCheckBox` | unchecked | `"Disable HF PA"` | `"Disables HF PA."` | `DisableHfPa` |

The first two carry NereusSDR-original labels and tooltips because Thetis's column-header checkboxes have neither — note this in the source comments.

- [ ] **Step 11.1: Write failing test for the 3 controls**

```cpp
// tests/tst_transmit_setup_block_tx.cpp
#include <QtTest>
#include <QCheckBox>
#include "gui/setup/TransmitSetupPages.h"

class TestTransmitSetupBlockTx : public QObject
{
    Q_OBJECT
private slots:
    void blockTxAnt2_present_withNereusOriginalLabel();
    void blockTxAnt3_present_withNereusOriginalLabel();
    void chkHFTRRelay_present_withThetisTooltip();
};

void TestTransmitSetupBlockTx::blockTxAnt2_present_withNereusOriginalLabel()
{
    TransmitSetupPages page;
    auto* chk = page.findChild<QCheckBox*>("chkBlockTxAnt2");
    QVERIFY(chk);
    QCOMPARE(chk->text(), QString("Block TX on Ant 2"));
    QCOMPARE(chk->toolTip(),
             QString("When checked, the radio cannot transmit on Antenna 2"));
}

void TestTransmitSetupBlockTx::blockTxAnt3_present_withNereusOriginalLabel()
{
    TransmitSetupPages page;
    auto* chk = page.findChild<QCheckBox*>("chkBlockTxAnt3");
    QVERIFY(chk);
    QCOMPARE(chk->text(), QString("Block TX on Ant 3"));
    QCOMPARE(chk->toolTip(),
             QString("When checked, the radio cannot transmit on Antenna 3"));
}

void TestTransmitSetupBlockTx::chkHFTRRelay_present_withThetisTooltip()
{
    TransmitSetupPages page;
    auto* chk = page.findChild<QCheckBox*>("chkHFTRRelay");
    QVERIFY(chk);
    QCOMPARE(chk->text(), QString("Disable HF PA"));
    QCOMPARE(chk->toolTip(), QString("Disables HF PA."));
}

QTEST_MAIN(TestTransmitSetupBlockTx)
#include "tst_transmit_setup_block_tx.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_transmit_setup_block_tx)
```

- [ ] **Step 11.2: Build; expect failure**

```bash
cmake --build build --target tst_transmit_setup_block_tx -j$(nproc)
```
Expected: tests fail (controls don't exist yet).

- [ ] **Step 11.3: Implement the build methods**

```cpp
// In TransmitSetupPages.cpp:

QGroupBox* TransmitSetupPages::buildBlockTxAntennaGroup()
{
    auto* group = new QGroupBox(tr("Block TX on RX antennas"));
    group->setObjectName(QStringLiteral("grpBlockTxAntennas"));
    auto* layout = new QVBoxLayout(group);

    // NereusSDR-original label + tooltip — Thetis ships these as
    // unlabelled column-header checkboxes (setup.designer.cs:6715-6724
    // [v2.10.3.13]); we add accessible copy.
    m_chkBlockTxAnt2 = new QCheckBox(tr("Block TX on Ant 2"));
    m_chkBlockTxAnt2->setObjectName(QStringLiteral("chkBlockTxAnt2"));
    m_chkBlockTxAnt2->setToolTip(tr("When checked, the radio cannot transmit on Antenna 2"));
    m_chkBlockTxAnt2->setChecked(
        AppSettings::instance().value("AlexAnt2RxOnly", "False").toString() == "True");
    connect(m_chkBlockTxAnt2, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("AlexAnt2RxOnly", on ? "True" : "False");
    });
    layout->addWidget(m_chkBlockTxAnt2);

    m_chkBlockTxAnt3 = new QCheckBox(tr("Block TX on Ant 3"));
    m_chkBlockTxAnt3->setObjectName(QStringLiteral("chkBlockTxAnt3"));
    m_chkBlockTxAnt3->setToolTip(tr("When checked, the radio cannot transmit on Antenna 3"));
    m_chkBlockTxAnt3->setChecked(
        AppSettings::instance().value("AlexAnt3RxOnly", "False").toString() == "True");
    connect(m_chkBlockTxAnt3, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("AlexAnt3RxOnly", on ? "True" : "False");
    });
    layout->addWidget(m_chkBlockTxAnt3);

    return group;
}

QGroupBox* TransmitSetupPages::buildHfPaGroup()
{
    auto* group = new QGroupBox(tr("PA Control"));
    group->setObjectName(QStringLiteral("grpHfPaControl"));
    auto* layout = new QVBoxLayout(group);

    m_chkHFTRRelay = new QCheckBox(tr("Disable HF PA"));
    m_chkHFTRRelay->setObjectName(QStringLiteral("chkHFTRRelay"));
    // Tooltip text from Thetis setup.designer.cs:5786 [v2.10.3.13]
    m_chkHFTRRelay->setToolTip(tr("Disables HF PA."));
    m_chkHFTRRelay->setChecked(
        AppSettings::instance().value("DisableHfPa", "False").toString() == "True");
    connect(m_chkHFTRRelay, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("DisableHfPa", on ? "True" : "False");
    });
    layout->addWidget(m_chkHFTRRelay);

    return group;
}
```

- [ ] **Step 11.4: Build & verify all 3 tests pass**

```bash
cmake --build build --target tst_transmit_setup_block_tx -j$(nproc)
cd build && ctest -R tst_transmit_setup_block_tx -V
```

- [ ] **Step 11.5: Commit**

```bash
git add src/gui/setup/TransmitSetupPages.h src/gui/setup/TransmitSetupPages.cpp \
        tests/tst_transmit_setup_block_tx.cpp tests/CMakeLists.txt
git commit -m "feat(setup): Block-TX antennas + Disable HF PA (3M-0 Task 11)

3 controls. chkBlockTxAnt2/3 carry NereusSDR-original labels and
tooltips because Thetis ships unlabelled column-header checkboxes
(setup.designer.cs:6715-6724 [v2.10.3.13]). chkHFTRRelay tooltip
'Disables HF PA.' verbatim from setup.designer.cs:5786.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 12: Setup → General → Hardware Configuration

**Files:**
- Create or Modify: `src/gui/setup/GeneralOptionsPage.h/.cpp` (build the Hardware Configuration group)
- Test: `tests/tst_general_setup_hardware_config.cpp`
- Modify: `tests/CMakeLists.txt`

Reference: Thetis `tpGeneralHardware` tab at `setup.designer.cs:8045-8396 [v2.10.3.13]`. Four controls.

| Control | objectName | Type | Default | Label/Text | Tooltip | Range/Items | AppSettings key |
|---|---|---|---|---|---|---|---|
| Region | `comboFRSRegion` | `QComboBox` | `"United States"` | (no label inline; group label) | `"Select Region for your location"` | 24 entries: Australia, Europe, India, Italy, Israel, Japan, Spain, United Kingdom, United States, Norway, Denmark, Sweden, Latvia, Slovakia, Bulgaria, Greece, Hungary, Netherlands, France, Russia, Region1, Region2, Region3, Germany | `Region` |
| Extended | `chkExtended` | `QCheckBox` | unchecked | `"Extended"` | `"Enable extended TX (out of band)"` | bool | `ExtendedTxAllowed` |
| (Red warning label) | `lblWarningRegionExtended` | `QLabel` | red bold | `"Changing this setting will reset your band stack entries"` | (no tooltip) | static | n/a |
| Receive Only | `chkGeneralRXOnly` | `QCheckBox` | unchecked, **`Visible=false` by default; visible per board model** | `"Receive Only"` | `"Check to disable transmit functionality."` | bool | `RxOnly` |
| Network Watchdog | `chkNetworkWDT` | `QCheckBox` | **checked (default ON)** | `"Network Watchdog"` | `"Resets software/firmware if network becomes inactive."` | bool | `NetworkWatchdogEnabled` |

- [ ] **Step 12.1: Write failing test for all 4 controls + the warning label**

```cpp
// tests/tst_general_setup_hardware_config.cpp
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include "gui/setup/GeneralOptionsPage.h"

class TestGeneralSetupHardwareConfig : public QObject
{
    Q_OBJECT
private slots:
    void regionCombo_24Entries_defaultUnitedStates();
    void chkExtended_present_withWarningLabel();
    void chkGeneralRXOnly_hiddenByDefault();
    void chkNetworkWDT_present_defaultChecked();
};

void TestGeneralSetupHardwareConfig::regionCombo_24Entries_defaultUnitedStates()
{
    GeneralOptionsPage page;
    auto* combo = page.findChild<QComboBox*>("comboFRSRegion");
    QVERIFY(combo);
    QCOMPARE(combo->count(), 24);
    QCOMPARE(combo->currentText(), QString("United States"));
    QCOMPARE(combo->toolTip(), QString("Select Region for your location"));
    // Spot-check a few entries
    QVERIFY(combo->findText("Australia") >= 0);
    QVERIFY(combo->findText("Japan") >= 0);
    QVERIFY(combo->findText("Germany") >= 0);
}

void TestGeneralSetupHardwareConfig::chkExtended_present_withWarningLabel()
{
    GeneralOptionsPage page;
    auto* chk = page.findChild<QCheckBox*>("chkExtended");
    QVERIFY(chk);
    QCOMPARE(chk->text(), QString("Extended"));
    QCOMPARE(chk->toolTip(), QString("Enable extended TX (out of band)"));
    QCOMPARE(chk->isChecked(), false);

    auto* lbl = page.findChild<QLabel*>("lblWarningRegionExtended");
    QVERIFY(lbl);
    QCOMPARE(lbl->text(),
             QString("Changing this setting will reset your band stack entries"));
}

void TestGeneralSetupHardwareConfig::chkGeneralRXOnly_hiddenByDefault()
{
    GeneralOptionsPage page;
    auto* chk = page.findChild<QCheckBox*>("chkGeneralRXOnly");
    QVERIFY(chk);
    QCOMPARE(chk->text(), QString("Receive Only"));
    QCOMPARE(chk->toolTip(), QString("Check to disable transmit functionality."));
    QVERIFY(!chk->isVisible());  // default hidden; per-board visibility
}

void TestGeneralSetupHardwareConfig::chkNetworkWDT_present_defaultChecked()
{
    GeneralOptionsPage page;
    auto* chk = page.findChild<QCheckBox*>("chkNetworkWDT");
    QVERIFY(chk);
    QCOMPARE(chk->text(), QString("Network Watchdog"));
    QCOMPARE(chk->toolTip(),
             QString("Resets software/firmware if network becomes inactive."));
    QCOMPARE(chk->isChecked(), true);  // default ON per setup.designer.cs:8387
}

QTEST_MAIN(TestGeneralSetupHardwareConfig)
#include "tst_general_setup_hardware_config.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_general_setup_hardware_config)
```

- [ ] **Step 12.2: Build; verify failure**

- [ ] **Step 12.3: Implement `GeneralOptionsPage::buildHardwareConfigGroup()`**

```cpp
QGroupBox* GeneralOptionsPage::buildHardwareConfigGroup()
{
    auto* group = new QGroupBox(tr("Hardware Configuration"));
    group->setObjectName(QStringLiteral("grpHardwareConfig"));
    auto* layout = new QGridLayout(group);

    // Region combo — 24 entries from Thetis setup.designer.cs:8084-8108
    // [v2.10.3.13]
    m_comboRegion = new QComboBox;
    m_comboRegion->setObjectName(QStringLiteral("comboFRSRegion"));
    m_comboRegion->addItems({
        tr("Australia"), tr("Europe"), tr("India"), tr("Italy"),
        tr("Israel"), tr("Japan"), tr("Spain"), tr("United Kingdom"),
        tr("United States"), tr("Norway"), tr("Denmark"), tr("Sweden"),
        tr("Latvia"), tr("Slovakia"), tr("Bulgaria"), tr("Greece"),
        tr("Hungary"), tr("Netherlands"), tr("France"), tr("Russia"),
        tr("Region1"), tr("Region2"), tr("Region3"), tr("Germany"),
    });
    m_comboRegion->setToolTip(tr("Select Region for your location"));
    m_comboRegion->setCurrentText(
        AppSettings::instance().value("Region", "United States").toString());
    connect(m_comboRegion, &QComboBox::currentTextChanged, this, [](const QString& r){
        AppSettings::instance().setValue("Region", r);
    });
    layout->addWidget(new QLabel(tr("Region:")), 0, 0);
    layout->addWidget(m_comboRegion, 0, 1);

    // Extended toggle + red warning label
    m_chkExtended = new QCheckBox(tr("Extended"));
    m_chkExtended->setObjectName(QStringLiteral("chkExtended"));
    // Tooltip text from Thetis setup.designer.cs:8074 [v2.10.3.13]
    m_chkExtended->setToolTip(tr("Enable extended TX (out of band)"));
    m_chkExtended->setChecked(
        AppSettings::instance().value("ExtendedTxAllowed", "False").toString() == "True");
    connect(m_chkExtended, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("ExtendedTxAllowed", on ? "True" : "False");
    });
    layout->addWidget(m_chkExtended, 1, 0);

    // Warning label — red, bold; text verbatim from
    // setup.designer.cs:8052 [v2.10.3.13]
    m_lblWarningExtended = new QLabel(
        tr("Changing this setting will reset your band stack entries"));
    m_lblWarningExtended->setObjectName(QStringLiteral("lblWarningRegionExtended"));
    auto warnFont = m_lblWarningExtended->font();
    warnFont.setBold(true);
    m_lblWarningExtended->setFont(warnFont);
    m_lblWarningExtended->setStyleSheet(QStringLiteral("color: red"));
    layout->addWidget(m_lblWarningExtended, 1, 1);

    // RX-only toggle (hidden by default; per-board visibility per
    // setup.cs:19837-20362 [v2.10.3.13])
    m_chkRxOnly = new QCheckBox(tr("Receive Only"));
    m_chkRxOnly->setObjectName(QStringLiteral("chkGeneralRXOnly"));
    m_chkRxOnly->setToolTip(tr("Check to disable transmit functionality."));
    m_chkRxOnly->setVisible(false);  // visibility set by RadioModel based on caps
    m_chkRxOnly->setChecked(
        AppSettings::instance().value("RxOnly", "False").toString() == "True");
    connect(m_chkRxOnly, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("RxOnly", on ? "True" : "False");
    });
    layout->addWidget(m_chkRxOnly, 2, 0, 1, 2);

    // Network watchdog (default ON per setup.designer.cs:8387)
    m_chkNetworkWDT = new QCheckBox(tr("Network Watchdog"));
    m_chkNetworkWDT->setObjectName(QStringLiteral("chkNetworkWDT"));
    m_chkNetworkWDT->setToolTip(
        tr("Resets software/firmware if network becomes inactive."));
    m_chkNetworkWDT->setChecked(
        AppSettings::instance().value("NetworkWatchdogEnabled", "True").toString() == "True");
    connect(m_chkNetworkWDT, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("NetworkWatchdogEnabled", on ? "True" : "False");
    });
    layout->addWidget(m_chkNetworkWDT, 3, 0, 1, 2);

    return group;
}
```

- [ ] **Step 12.4: Build, run; verify all 4 tests pass**

- [ ] **Step 12.5: Commit**

```bash
git add src/gui/setup/GeneralOptionsPage.h src/gui/setup/GeneralOptionsPage.cpp \
        tests/tst_general_setup_hardware_config.cpp tests/CMakeLists.txt
git commit -m "feat(setup): General → Hardware Configuration group (3M-0 Task 12)

4 controls + 1 warning label per Thetis tpGeneralHardware
(setup.designer.cs:8045-8396 [v2.10.3.13]). 24-entry Region combo,
Extended toggle (renamed from MARS/CAP per pre-code review),
chkGeneralRXOnly hidden by default (per-board visibility wired by
RadioModel in Task 17), chkNetworkWDT default ON.

All tooltip text verbatim from Thetis. Persisted per-MAC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 13: Setup → General → Options group

**Files:**
- Modify: `src/gui/setup/GeneralOptionsPage.cpp` (add `buildOptionsGroup()`)
- Test: `tests/tst_general_setup_options.cpp`
- Modify: `tests/CMakeLists.txt`

Reference: Thetis `grpGeneralOptions` at `setup.designer.cs:9050-9059 [v2.10.3.13]`. One control.

| Control | objectName | Type | Default | Label/Text | Tooltip | AppSettings key |
|---|---|---|---|---|---|---|
| Prevent TX on different band | `chkPreventTXonDifferentBandToRX` | `QCheckBox` | unchecked | `"Prevent TX'ing on a different band to the RX band"` | (none in Thetis; we add: `"When checked, MOX is rejected if the TX VFO is on a different band than the RX VFO"`) | `PreventTxOnDifferentBandToRx` |

- [ ] **Step 13.1: Write failing test**

```cpp
// tests/tst_general_setup_options.cpp
#include <QtTest>
#include <QCheckBox>
#include "gui/setup/GeneralOptionsPage.h"

class TestGeneralSetupOptions : public QObject
{
    Q_OBJECT
private slots:
    void preventTxOnDifferentBand_present_unchecked();
};

void TestGeneralSetupOptions::preventTxOnDifferentBand_present_unchecked()
{
    GeneralOptionsPage page;
    auto* chk = page.findChild<QCheckBox*>("chkPreventTXonDifferentBandToRX");
    QVERIFY(chk);
    QCOMPARE(chk->text(),
             QString("Prevent TX'ing on a different band to the RX band"));
    QCOMPARE(chk->isChecked(), false);
    // Tooltip is NereusSDR-original; just verify it's non-empty
    QVERIFY(!chk->toolTip().isEmpty());
}

QTEST_MAIN(TestGeneralSetupOptions)
#include "tst_general_setup_options.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_general_setup_options)
```

- [ ] **Step 13.2: Build; verify failure**

- [ ] **Step 13.3: Implement `GeneralOptionsPage::buildOptionsGroup()`**

```cpp
QGroupBox* GeneralOptionsPage::buildOptionsGroup()
{
    auto* group = new QGroupBox(tr("Options"));
    group->setObjectName(QStringLiteral("grpGeneralOptions"));
    auto* layout = new QVBoxLayout(group);

    // From Thetis setup.designer.cs:9050-9059 [v2.10.3.13]
    m_chkPreventTxOnDifferentBand = new QCheckBox(
        tr("Prevent TX'ing on a different band to the RX band"));
    m_chkPreventTxOnDifferentBand->setObjectName(
        QStringLiteral("chkPreventTXonDifferentBandToRX"));
    // Tooltip text NereusSDR-original (Thetis has no tooltip on this control)
    m_chkPreventTxOnDifferentBand->setToolTip(tr(
        "When checked, MOX is rejected if the TX VFO is on a different band than the RX VFO"));
    m_chkPreventTxOnDifferentBand->setChecked(
        AppSettings::instance().value("PreventTxOnDifferentBandToRx", "False").toString() == "True");
    connect(m_chkPreventTxOnDifferentBand, &QCheckBox::toggled, this, [](bool on){
        AppSettings::instance().setValue("PreventTxOnDifferentBandToRx", on ? "True" : "False");
    });
    layout->addWidget(m_chkPreventTxOnDifferentBand);

    return group;
}
```

- [ ] **Step 13.4: Build, run; verify the test passes**

- [ ] **Step 13.5: Commit**

```bash
git add src/gui/setup/GeneralOptionsPage.h src/gui/setup/GeneralOptionsPage.cpp \
        tests/tst_general_setup_options.cpp tests/CMakeLists.txt
git commit -m "feat(setup): General → Options → prevent-different-band toggle (3M-0 Task 13)

1 control per Thetis grpGeneralOptions (setup.designer.cs:9050-9059
[v2.10.3.13]). Tooltip text NereusSDR-original (Thetis has no tooltip
on this control).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 14: Status bar TX Inhibit + PA Status routing audit

**Files:**
- Modify: `src/gui/MainWindow.cpp` (status bar wiring)

The 3P-H Diagnostics work already added the status bar labels. This task wires:
- `txInhibitLabel.setVisible(monitor.inhibited())` ← `TxInhibitMonitor::txInhibitedChanged`
- `paStatusBadge.setIcon(model.paTripped() ? :/icons/paflt : :/icons/paok)` ← `RadioModel::paTrippedChanged`

- [ ] **Step 14.1: Write failing visibility tests on existing 3P-H widgets, then wire signal connections.**

---

## Task 15: Per-MAC persistence audit

**Files:**
- Test: `tests/tst_phase3m0_persistence_audit.cpp`

Verify that all 14 persistence keys from Section 3 of the pre-code review round-trip cleanly:

```cpp
void TestPhase3m0Persistence::allKeysRoundTrip()
{
    auto& s = AppSettings::instance();

    // Set known values
    s.setValue("SwrProtectionEnabled", "True");
    s.setValue("SwrProtectionLimit", "2.5");
    s.setValue("SwrIgnoreOnTuneEnabled", "True");
    // ... 11 more ...

    // Force a save + reload cycle
    s.flush();
    AppSettings::reload();

    // Verify
    QCOMPARE(s.value("SwrProtectionEnabled").toString(), QString("True"));
    QCOMPARE(s.value("SwrProtectionLimit").toString(), QString("2.5"));
    // ... etc ...
}
```

- [ ] **Step 15.1-15.3: Write the audit test, ensure all keys round-trip, commit.**

---

## Task 16: Verification matrix doc

**Files:**
- Create: `docs/architecture/phase3m-0-verification/README.md`

Build a manual verification matrix for hardware testing — same pattern as `phase3g-rx-epic-a-verification/README.md`. Each row a physical test against an ANAN-G2 + a HermesLite 2 + a board with mic-jack-only.

Rows to include:
1. Synthetic SWR foldback (unit test passes; reproduce on hardware via mismatch jig)
2. Latched windback after 4 trip samples (visible in PA Power gauge — drops to ~1% of drive)
3. Open-antenna detection (disconnect antenna mid-RX; verify fwd > 10W bench condition)
4. TX-inhibit user-IO pin (simulate by jumpering the appropriate input)
5. Watchdog enable (verify radio LED stops MOX after 1s of suppressed C&C — radio firmware does this)
6. BandPlanGuard 60m channelization (UK 11 + US 5 + JA 4.63 = 17 corner cases)
7. Extended toggle bypass (try TX at 13.5 MHz with and without Extended)
8. PreventTxOnDifferentBand guard
9. Per-board PA scaling values plausible (compare gauge readings to a watt meter at known drive)
10. HighSWR static overlay paints with foldback-text variant
11. RX-only SKU hard-blocks MOX entry
12. PA-trip clears MOX automatically on Andromeda console (skip on non-Andromeda)
13. Per-MAC persistence round-trip (close app, reopen, all 14 keys preserved)

- [ ] **Step 16.1: Write the verification matrix doc following the 3G-rx-epic pattern.**

---

## Task 17: Final integration — wire `RadioModel` to own the safety controllers

**Files:**
- Modify: `src/core/RadioModel.h/.cpp`

- [ ] **Step 17.1: Add member fields**

```cpp
private:
    safety::SwrProtectionController m_swrProt;
    safety::TxInhibitMonitor       m_txInhibit;
    safety::BandPlanGuard          m_bandPlan;
```

- [ ] **Step 17.2: Wire telemetry signals into the controllers in the constructor**

```cpp
RadioModel::RadioModel(QObject* parent)
    : QObject(parent)
{
    // Route RadioStatus PA telemetry into SWR controller
    connect(&m_radioStatus, &RadioStatus::paFwdChanged,
            this, [this](float fwd) {
        m_swrProt.ingest(fwd, m_radioStatus.paRev(), m_tuneActive);
    });
    connect(&m_radioStatus, &RadioStatus::paRevChanged,
            this, [this](float rev) {
        m_swrProt.ingest(m_radioStatus.paFwd(), rev, m_tuneActive);
    });

    // Route SWR factor changes into SpectrumWidget overlay
    connect(&m_swrProt, &safety::SwrProtectionController::highSwrChanged,
            this, [this](bool isHigh) {
        if (m_spectrumWidget) {
            m_spectrumWidget->setHighSwrOverlay(isHigh, m_swrProt.windBackLatched());
        }
        emit highSwrChanged(isHigh);
    });

    // ... wire TxInhibitMonitor / BandPlanGuard / paTripped similarly ...
}
```

- [ ] **Step 17.3: Build & run the full test suite to verify no regressions**

```bash
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
Expected: all tests pass (existing + new from Tasks 1-16).

- [ ] **Step 17.4: Run the app manually**

```bash
./build/NereusSDR
```
Verify:
- Setup → Transmit → SWR Protection page shows the 5 controls
- Setup → Transmit → External TX Inhibit shows the 2 controls
- Setup → Transmit → Block-TX antennas + Disable HF PA shows the 3 controls
- Setup → General → Hardware Configuration shows region + extended + RX-only + Network WDT
- Setup → General → Options shows the prevent-different-band toggle
- PA Fwd/Rev/SWR meters paint values during RX (just baseline 0/0/1.0)
- App relaunches with all settings persisted

- [ ] **Step 17.5: Final commit + push**

```bash
git add src/core/RadioModel.h src/core/RadioModel.cpp
git commit -m "$(cat <<'EOF'
feat(model): wire 3M-0 safety controllers into RadioModel

RadioModel owns SwrProtectionController, TxInhibitMonitor, BandPlanGuard.
Routes RadioStatus PA telemetry into SWR controller. Wires
highSwrChanged → SpectrumWidget overlay. paTripped() live state.

All 16 tasks of Phase 3M-0 PA Safety Foundation merged into RadioModel.

Phase 3M-0 Task 17 (final integration).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"

git push -u origin feature/phase3m-0-pa-safety
```

---

## Self-Review Checklist (run before opening PR)

- [ ] All 17 tasks complete; all unit tests green
- [ ] Hardware verification matrix items 1-13 confirmed on at least one ANAN-G2 + one HL2
- [ ] Per-MAC persistence: all 14 keys round-trip cleanly across app relaunch
- [ ] No `// TODO` / `// FIXME` / `// NYI` in the merged code
- [ ] Every Thetis cite in source comments carries `[v2.10.3.13]` or `[@501e3f51]` stamp
- [ ] Every user-visible string is plain English; cites in source comments only
- [ ] Out-of-phase TX controls (mic, MOX, TUN buttons) still hidden — 3M-0 ships **no MOX path yet**
- [ ] Pre-commit hooks pass on every commit (verify-thetis-headers, check-new-ports, verify-inline-cites, compliance-inventory, verify-inline-tag-preservation)
- [ ] All commits GPG-signed (`feedback_gpg_sign_commits`)
- [ ] Post-code Thetis review agent dispatched, walks the per-component coverage table from the design doc, signs off ✅ on every row

After self-review passes, open the PR. PR title: `feat(safety): Phase 3M-0 PA Safety Foundation`. Base: `main`. Body should include:
- Summary listing all 8 components
- Test plan with hardware verification matrix link
- "Phase 3M-0 closes the safety gate before any TX code lands"
- Signature: `J.J. Boyd ~ KG4VCF`

After merge: cut a fresh worktree off main for **Phase 3M-1a** (TUNE-only first RF). The MoxController and TxChannel work begins there.

---

## End of plan
