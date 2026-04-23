# Band Button Auto-Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire band-button clicks to auto-set DSP mode and recall per-band state, closing [issue #118](https://github.com/boydsoftprez/NereusSDR/issues/118).

**Architecture:** One new pure-data module (`BandDefaults`) holds 13 Thetis-derived seed values. One new `RadioModel::onBandButtonClicked(Band)` slot is the single handler for both band-button entry points — the `SpectrumOverlayPanel` flyout (currently drops mode) and the `BandButtonItem` in meter containers (currently orphaned). The handler uses the 3G-10 Stage 2 per-band persistence already on `SliceModel`: if the band has been visited, restore; if not, seed from the table and save.

**Tech Stack:** C++20, Qt6 (Core, Test), existing `AppSettings` XML persistence, `NereusSDRObjs` CMake OBJECT library for test linkage.

**Spec reference:** [docs/architecture/band-button-auto-mode-design.md](../../architecture/band-button-auto-mode-design.md)

---

## File Structure

| File | Purpose | New/Modified |
| --- | --- | --- |
| `src/models/Band.h`, `.cpp` | Add `bandFromName(const QString&)` string→enum lookup | Modified |
| `src/models/BandDefaults.h`, `.cpp` | `constexpr` seed table `(Band, freqHz, mode)` with Thetis cites | **New** |
| `src/models/SliceModel.h`, `.cpp` | Add `hasSettingsFor(Band) const` AppSettings probe | Modified |
| `src/models/RadioModel.h`, `.cpp` | Add `onBandButtonClicked(Band)` public slot | Modified |
| `src/gui/MainWindow.cpp` | Replace overlay-flyout lambda body; add container `bandClicked` connection | Modified |
| `tests/tst_band_defaults.cpp` | Seed-table integrity tests | **New** |
| `tests/tst_band_from_name.cpp` | `bandFromName` lookup tests | **New** |
| `tests/tst_slice_has_settings_for.cpp` | `hasSettingsFor` probe tests | **New** |
| `tests/tst_radio_model_band_click.cpp` | Full handler behavior tests | **New** |
| `tests/CMakeLists.txt` | Register the four new test binaries | Modified |
| `CMakeLists.txt` | Register `BandDefaults.cpp` in the objects library | Modified |

Persistence key format is the existing **`Slice<N>/Band<Label>/<Field>`** (confirmed at [SliceModel.cpp:757](../../../src/models/SliceModel.cpp:757)) — no schema change.

---

## Task 1: `Band::bandFromName()` helper

Accepts both overlay-flyout short names (`"160"`, `"80"`, `"WWV"`) and label form (`"160m"`, `"80m"`). The overlay emits short names ([SpectrumOverlayPanel.cpp:148](../../../src/gui/SpectrumOverlayPanel.cpp:148)); container path uses `bandFromUiIndex`. Tolerating both forms keeps the helper useful beyond just the overlay.

**Files:**
- Modify: `src/models/Band.h`
- Modify: `src/models/Band.cpp`
- Test: `tests/tst_band_from_name.cpp` *(new)*

- [ ] **Step 1: Write the failing test**

Create `tests/tst_band_from_name.cpp`:

```cpp
// tst_band_from_name.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// Unit tests for Band::bandFromName() — string-to-Band lookup used by the
// SpectrumOverlayPanel band flyout which emits short-name strings
// ("160", "80", "WWV") instead of the label-form strings ("160m", "80m").
// See docs/architecture/band-button-auto-mode-design.md.

#include <QtTest/QtTest>

#include "models/Band.h"

using namespace NereusSDR;

class TestBandFromName : public QObject {
    Q_OBJECT

private slots:
    void shortName_maps_to_band() {
        QCOMPARE(bandFromName(QStringLiteral("160")),  Band::Band160m);
        QCOMPARE(bandFromName(QStringLiteral("80")),   Band::Band80m);
        QCOMPARE(bandFromName(QStringLiteral("60")),   Band::Band60m);
        QCOMPARE(bandFromName(QStringLiteral("40")),   Band::Band40m);
        QCOMPARE(bandFromName(QStringLiteral("30")),   Band::Band30m);
        QCOMPARE(bandFromName(QStringLiteral("20")),   Band::Band20m);
        QCOMPARE(bandFromName(QStringLiteral("17")),   Band::Band17m);
        QCOMPARE(bandFromName(QStringLiteral("15")),   Band::Band15m);
        QCOMPARE(bandFromName(QStringLiteral("12")),   Band::Band12m);
        QCOMPARE(bandFromName(QStringLiteral("10")),   Band::Band10m);
        QCOMPARE(bandFromName(QStringLiteral("6")),    Band::Band6m);
    }

    void label_form_also_maps() {
        QCOMPARE(bandFromName(QStringLiteral("160m")), Band::Band160m);
        QCOMPARE(bandFromName(QStringLiteral("20m")),  Band::Band20m);
        QCOMPARE(bandFromName(QStringLiteral("6m")),   Band::Band6m);
    }

    void special_bands_map() {
        QCOMPARE(bandFromName(QStringLiteral("GEN")),  Band::GEN);
        QCOMPARE(bandFromName(QStringLiteral("WWV")),  Band::WWV);
        QCOMPARE(bandFromName(QStringLiteral("XVTR")), Band::XVTR);
    }

    void unknown_falls_back_to_gen() {
        QCOMPARE(bandFromName(QStringLiteral("")),         Band::GEN);
        QCOMPARE(bandFromName(QStringLiteral("bogus")),    Band::GEN);
        QCOMPARE(bandFromName(QStringLiteral("9999m")),    Band::GEN);
    }

    void case_sensitive_special_names() {
        // Matches bandKeyName() output exactly. "gen"/"wwv" lowercase is
        // NOT the canonical key and should fall back to GEN.
        QCOMPARE(bandFromName(QStringLiteral("gen")),  Band::GEN);
        QCOMPARE(bandFromName(QStringLiteral("wwv")),  Band::GEN);
    }
};

QTEST_MAIN(TestBandFromName)
#include "tst_band_from_name.moc"
```

- [ ] **Step 2: Register the test and run to verify failure**

Append to `tests/CMakeLists.txt` (next to the other `nereus_add_test` lines, around line 200):

```cmake
# ── Issue #118: band-button auto-mode ────────────────────────────────────
nereus_add_test(tst_band_from_name)
```

Build + run:
```bash
cmake --build build -j$(nproc) --target tst_band_from_name 2>&1 | tail -20
```
Expected: build fails with undefined symbol `NereusSDR::bandFromName(QString const&)`.

- [ ] **Step 3: Declare the function in `src/models/Band.h`**

Insert after the existing `int uiIndexFromBand(Band b);` declaration (around line 127):

```cpp
/// Maps a string band name to the corresponding Band enum. Accepts both
/// short-name form ("160", "80", "WWV") used by SpectrumOverlayPanel and
/// label form ("160m", "80m") used by bandKeyName(). Returns Band::GEN
/// for unknown strings. Case-sensitive for special names ("GEN", "WWV",
/// "XVTR" — uppercase).
///
/// Added Phase 3G-?? (issue #118) to route SpectrumOverlayPanel::bandSelected
/// and ContainerWidget::bandClicked through a single RadioModel handler.
Band bandFromName(const QString& name);
```

- [ ] **Step 4: Implement the function in `src/models/Band.cpp`**

Insert after the existing `int uiIndexFromBand(Band b)` definition (around line 165):

```cpp
Band bandFromName(const QString& name)
{
    // Short-name form (as emitted by SpectrumOverlayPanel::kBands).
    if (name == QLatin1String("160")) { return Band::Band160m; }
    if (name == QLatin1String("80"))  { return Band::Band80m; }
    if (name == QLatin1String("60"))  { return Band::Band60m; }
    if (name == QLatin1String("40"))  { return Band::Band40m; }
    if (name == QLatin1String("30"))  { return Band::Band30m; }
    if (name == QLatin1String("20"))  { return Band::Band20m; }
    if (name == QLatin1String("17"))  { return Band::Band17m; }
    if (name == QLatin1String("15"))  { return Band::Band15m; }
    if (name == QLatin1String("12"))  { return Band::Band12m; }
    if (name == QLatin1String("10"))  { return Band::Band10m; }
    if (name == QLatin1String("6"))   { return Band::Band6m; }

    // Label form (matches bandKeyName() output).
    if (name == QLatin1String("160m")) { return Band::Band160m; }
    if (name == QLatin1String("80m"))  { return Band::Band80m; }
    if (name == QLatin1String("60m"))  { return Band::Band60m; }
    if (name == QLatin1String("40m"))  { return Band::Band40m; }
    if (name == QLatin1String("30m"))  { return Band::Band30m; }
    if (name == QLatin1String("20m"))  { return Band::Band20m; }
    if (name == QLatin1String("17m"))  { return Band::Band17m; }
    if (name == QLatin1String("15m"))  { return Band::Band15m; }
    if (name == QLatin1String("12m"))  { return Band::Band12m; }
    if (name == QLatin1String("10m"))  { return Band::Band10m; }
    if (name == QLatin1String("6m"))   { return Band::Band6m; }

    // Special bands.
    if (name == QLatin1String("GEN"))  { return Band::GEN; }
    if (name == QLatin1String("WWV"))  { return Band::WWV; }
    if (name == QLatin1String("XVTR")) { return Band::XVTR; }

    return Band::GEN;
}
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build -j$(nproc) --target tst_band_from_name && ctest --test-dir build -R tst_band_from_name -V
```
Expected: 5/5 cases PASS.

- [ ] **Step 6: Commit**

```bash
git add src/models/Band.h src/models/Band.cpp \
        tests/tst_band_from_name.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(models): add Band::bandFromName string→enum helper (#118)

Accepts both short-name form ("160", "80", "WWV") emitted by
SpectrumOverlayPanel::bandSelected and label form ("160m", "80m")
matching bandKeyName() output. Falls back to Band::GEN for unknowns.
Prereq for routing both band-button entry points through a single
RadioModel handler.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `SliceModel::hasSettingsFor(Band)` probe

Detects first visit to a band by probing whether the `Slice<N>/Band<key>/DspMode` key exists in AppSettings. `DspMode` is the sentinel because it is the field most directly coupled to the bug.

**Files:**
- Modify: `src/models/SliceModel.h` (add declaration)
- Modify: `src/models/SliceModel.cpp` (add definition)
- Test: `tests/tst_slice_has_settings_for.cpp` *(new)*

- [ ] **Step 1: Write the failing test**

Create `tests/tst_slice_has_settings_for.cpp`:

```cpp
// tst_slice_has_settings_for.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// Unit tests for SliceModel::hasSettingsFor(Band) — first-visit probe used
// by RadioModel::onBandButtonClicked to decide between seed and restore.
// Key format matches Phase 3G-10 Stage 2 persistence:
// "Slice<N>/Band<key>/DspMode".

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceHasSettingsFor : public QObject {
    Q_OBJECT

private:
    static void clearKeys() {
        auto& s = AppSettings::instance();
        for (const QString& band : {
                QStringLiteral("20m"), QStringLiteral("40m"),
                QStringLiteral("60m"), QStringLiteral("XVTR") }) {
            s.remove(QStringLiteral("Slice0/Band") + band + QStringLiteral("/DspMode"));
        }
    }

private slots:
    void init() { clearKeys(); }
    void cleanup() { clearKeys(); }

    void absent_key_returns_false() {
        SliceModel slice(0);
        QVERIFY(!slice.hasSettingsFor(Band::Band20m));
        QVERIFY(!slice.hasSettingsFor(Band::Band40m));
        QVERIFY(!slice.hasSettingsFor(Band::XVTR));
    }

    void present_key_returns_true() {
        SliceModel slice(0);
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band20m/DspMode"),
                   static_cast<int>(DSPMode::USB));
        QVERIFY(slice.hasSettingsFor(Band::Band20m));
        QVERIFY(!slice.hasSettingsFor(Band::Band40m));
    }

    void per_slice_scoping() {
        SliceModel slice0(0);
        SliceModel slice1(1);
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band20m/DspMode"),
                   static_cast<int>(DSPMode::USB));
        QVERIFY(slice0.hasSettingsFor(Band::Band20m));
        QVERIFY(!slice1.hasSettingsFor(Band::Band20m));
    }
};

QTEST_MAIN(TestSliceHasSettingsFor)
#include "tst_slice_has_settings_for.moc"
```

- [ ] **Step 2: Register the test and run to verify failure**

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_slice_has_settings_for)
```

Run:
```bash
cmake --build build -j$(nproc) --target tst_slice_has_settings_for 2>&1 | tail -20
```
Expected: build fails with `no member named 'hasSettingsFor' in 'NereusSDR::SliceModel'`.

- [ ] **Step 3: Declare the method in `src/models/SliceModel.h`**

Find the per-band persistence section in the header (search for `void saveToSettings(Band band);`) and insert above it:

```cpp
    // Returns true iff the per-band AppSettings namespace for this slice
    // has any persisted state for `band` (probes the DspMode key).
    // Used by RadioModel::onBandButtonClicked (#118) to decide whether a
    // band click should seed defaults or restore last-used state.
    bool hasSettingsFor(Band band) const;
```

- [ ] **Step 4: Implement the method in `src/models/SliceModel.cpp`**

Insert just before the existing `void SliceModel::saveToSettings(Band band)` definition (around line 775):

```cpp
bool SliceModel::hasSettingsFor(Band band) const
{
    auto& s = AppSettings::instance();
    return s.contains(bandPrefix(m_sliceIndex, band) + QStringLiteral("DspMode"));
}
```

Note: `bandPrefix` is an anonymous-namespace helper defined at [SliceModel.cpp:757](../../../src/models/SliceModel.cpp:757), so the new method sits in the same translation unit and has access to it without any header export.

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build -j$(nproc) --target tst_slice_has_settings_for && ctest --test-dir build -R tst_slice_has_settings_for -V
```
Expected: 3/3 cases PASS.

- [ ] **Step 6: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp \
        tests/tst_slice_has_settings_for.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(models): add SliceModel::hasSettingsFor(Band) probe (#118)

Returns true iff Slice<N>/Band<key>/DspMode is present in AppSettings.
Used by the upcoming RadioModel::onBandButtonClicked handler to decide
between seeding and restoring on a band click. Probe is per-slice so a
band visited on slice 0 does not mask first-visit seeding on slice 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: `BandDefaults` seed table

Pure data module. 13 seeded bands + `XVTR` marked invalid. Frequencies verbatim from Thetis `AddRegion2BandStack()` at [`clsBandStackManager.cs:2099-2176 [v2.10.3.13]`](../../../../Thetis/Project Files/Source/Console/clsBandStackManager.cs) (local clone at `/Users/j.j.boyd/Thetis/`). Mode selection is the documented voice-first pick (see design doc §Design decision 2).

**Files:**
- Create: `src/models/BandDefaults.h`
- Create: `src/models/BandDefaults.cpp`
- Modify: `CMakeLists.txt` (register `BandDefaults.cpp`)
- Test: `tests/tst_band_defaults.cpp` *(new)*

- [ ] **Step 1: Write the failing test**

Create `tests/tst_band_defaults.cpp`:

```cpp
// tst_band_defaults.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here. Thetis cites are in BandDefaults.cpp.
//
// Seed-table integrity tests for BandDefaults (#118).
// Frequencies sourced verbatim from Thetis Region 2 seeds:
//   clsBandStackManager.cs:2099-2176 [v2.10.3.13 @ 501e3f51]

#include <QtTest/QtTest>

#include "models/Band.h"
#include "models/BandDefaults.h"
#include "core/DspMode.h"

using namespace NereusSDR;

class TestBandDefaults : public QObject {
    Q_OBJECT

private slots:
    void every_ham_band_has_valid_seed() {
        for (Band b : { Band::Band160m, Band::Band80m, Band::Band60m,
                        Band::Band40m,  Band::Band30m, Band::Band20m,
                        Band::Band17m,  Band::Band15m, Band::Band12m,
                        Band::Band10m,  Band::Band6m,  Band::WWV,
                        Band::GEN }) {
            BandSeed s = BandDefaults::seedFor(b);
            QVERIFY2(s.valid, qPrintable(QStringLiteral("Band %1 must have a valid seed")
                                          .arg(bandLabel(b))));
            QCOMPARE(s.band, b);
            QVERIFY(s.frequencyHz > 0.0);
        }
    }

    void xvtr_seed_is_invalid() {
        BandSeed s = BandDefaults::seedFor(Band::XVTR);
        QVERIFY(!s.valid);
    }

    void seed_freq_lands_inside_its_own_band() {
        // Skips GEN (fallback band — any-freq maps to it) and WWV
        // (detected at discrete centers ± window, so the 10 MHz seed
        // lands in WWV, not a ham band).
        for (Band b : { Band::Band160m, Band::Band80m, Band::Band60m,
                        Band::Band40m,  Band::Band30m, Band::Band20m,
                        Band::Band17m,  Band::Band15m, Band::Band12m,
                        Band::Band10m,  Band::Band6m }) {
            BandSeed s = BandDefaults::seedFor(b);
            Band derived = bandFromFrequency(s.frequencyHz);
            QVERIFY2(derived == b,
                     qPrintable(QStringLiteral("Seed for %1 (%2 Hz) derives to %3")
                                  .arg(bandLabel(b))
                                  .arg(s.frequencyHz)
                                  .arg(bandLabel(derived))));
        }
    }

    void wwv_seed_maps_to_wwv() {
        BandSeed s = BandDefaults::seedFor(Band::WWV);
        QCOMPARE(bandFromFrequency(s.frequencyHz), Band::WWV);
    }

    void seeds_match_thetis_region2_values() {
        // Exact values from clsBandStackManager.cs:2104-2168 (first voice
        // entry per band, Thetis v2.10.3.13).
        struct Expected { Band band; double hz; DSPMode mode; };
        const Expected table[] = {
            { Band::Band160m, 1840000.0,  DSPMode::LSB },   // :2104
            { Band::Band80m,  3650000.0,  DSPMode::LSB },   // :2109
            { Band::Band60m,  5354000.0,  DSPMode::USB },   // :2114
            { Band::Band40m,  7152000.0,  DSPMode::LSB },   // :2120
            { Band::Band30m,  10107000.0, DSPMode::CWU },   // :2123 (CW only)
            { Band::Band20m,  14155000.0, DSPMode::USB },   // :2130
            { Band::Band17m,  18125000.0, DSPMode::USB },   // :2135
            { Band::Band15m,  21205000.0, DSPMode::USB },   // :2140
            { Band::Band12m,  24931000.0, DSPMode::USB },   // :2145
            { Band::Band10m,  28305000.0, DSPMode::USB },   // :2150
            { Band::Band6m,   50125000.0, DSPMode::USB },   // :2155
            { Band::WWV,      10000000.0, DSPMode::SAM },   // :2165
            { Band::GEN,      13845000.0, DSPMode::SAM },   // :2168
        };
        for (const Expected& e : table) {
            BandSeed s = BandDefaults::seedFor(e.band);
            QCOMPARE(s.frequencyHz, e.hz);
            QCOMPARE(s.mode, e.mode);
        }
    }
};

QTEST_MAIN(TestBandDefaults)
#include "tst_band_defaults.moc"
```

- [ ] **Step 2: Register the test and run to verify failure**

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_band_defaults)
```

Run:
```bash
cmake --build build -j$(nproc) --target tst_band_defaults 2>&1 | tail -15
```
Expected: fatal error `'models/BandDefaults.h' file not found`.

- [ ] **Step 3: Create the header**

Write `src/models/BandDefaults.h`:

```cpp
// =================================================================
// src/models/BandDefaults.h  (NereusSDR)
// =================================================================
//
// Seed lookup table for per-band default freq + demod mode used on the
// first band-button click before per-band persistence has been written.
//
// Issue #118. Spec: docs/architecture/band-button-auto-mode-design.md.
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsBandStackManager.cs:2099-2176
//   (AddRegion2BandStack, v2.10.3.13 @ 501e3f51) — US Region 2 seeds.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — New file for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

//=================================================================
// clsBandStackManager.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2019-2026  Richard Samphire (MW0LGE)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#pragma once

#include "Band.h"
#include "../core/DspMode.h"

namespace NereusSDR {

/// Seed entry for a single band: the default VFO frequency and DSP mode
/// applied by RadioModel::onBandButtonClicked(Band) when the slice has
/// never been on `band` before. `valid == false` means "no seed for this
/// band" (currently only XVTR — seed becomes meaningful once transverter
/// config ships).
struct BandSeed {
    Band    band;
    double  frequencyHz;
    DSPMode mode;
    bool    valid;
};

namespace BandDefaults {

/// Returns the seed entry for `b`. `seedFor(Band::XVTR)` returns
/// `valid == false` — callers must handle the XVTR no-op path.
BandSeed seedFor(Band b);

} // namespace BandDefaults
} // namespace NereusSDR
```

- [ ] **Step 4: Create the implementation**

Write `src/models/BandDefaults.cpp`:

```cpp
// =================================================================
// src/models/BandDefaults.cpp  (NereusSDR)
// =================================================================
//
// Header provenance + GPL/dual-license notice: see BandDefaults.h.
// Thetis cite stamps below use [v2.10.3.13] from the tagged release
// v2.10.3.13-7-g501e3f51.
// =================================================================

#include "BandDefaults.h"

namespace NereusSDR {
namespace BandDefaults {

BandSeed seedFor(Band b)
{
    switch (b) {
        // From Thetis clsBandStackManager.cs:2104 [v2.10.3.13] —
        // first voice entry in AddRegion2BandStack "160M" list.
        case Band::Band160m: return { Band::Band160m,  1840000.0,  DSPMode::LSB, true };
        // From Thetis clsBandStackManager.cs:2109 [v2.10.3.13]
        case Band::Band80m:  return { Band::Band80m,   3650000.0,  DSPMode::LSB, true };
        // From Thetis clsBandStackManager.cs:2114 [v2.10.3.13] —
        // sole uncommented 60m entry; 60m is channelized USB.
        case Band::Band60m:  return { Band::Band60m,   5354000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2120 [v2.10.3.13]
        case Band::Band40m:  return { Band::Band40m,   7152000.0,  DSPMode::LSB, true };
        // From Thetis clsBandStackManager.cs:2123 [v2.10.3.13] —
        // 30m is CW/digital only per FCC §97.305(c); no voice entry exists.
        case Band::Band30m:  return { Band::Band30m,  10107000.0,  DSPMode::CWU, true };
        // From Thetis clsBandStackManager.cs:2130 [v2.10.3.13]
        case Band::Band20m:  return { Band::Band20m,  14155000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2135 [v2.10.3.13]
        case Band::Band17m:  return { Band::Band17m,  18125000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2140 [v2.10.3.13]
        case Band::Band15m:  return { Band::Band15m,  21205000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2145 [v2.10.3.13]
        case Band::Band12m:  return { Band::Band12m,  24931000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2150 [v2.10.3.13]
        case Band::Band10m:  return { Band::Band10m,  28305000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2155 [v2.10.3.13]
        case Band::Band6m:   return { Band::Band6m,   50125000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2165 [v2.10.3.13] —
        // 10 MHz is mid-list of 5 WWV entries and most commonly usable.
        // Thetis uses synchronous AM (SAM), not plain AM.
        case Band::WWV:      return { Band::WWV,      10000000.0,  DSPMode::SAM, true };
        // From Thetis clsBandStackManager.cs:2168 [v2.10.3.13] —
        // index 0 of GEN stack (SW broadcast). Full GEN sub-band port
        // (LMF/120m/90m/…) deferred to Phase 3H.
        case Band::GEN:      return { Band::GEN,      13845000.0,  DSPMode::SAM, true };
        // XVTR has no intrinsic seed — depends on user-configured
        // transverters. Handler must no-op on XVTR first-visit until
        // the XVTR epic lands.
        case Band::XVTR:     return { Band::XVTR,     0.0,         DSPMode::USB, false };
        case Band::Count:    break;
    }
    return { Band::GEN, 0.0, DSPMode::USB, false };
}

} // namespace BandDefaults
} // namespace NereusSDR
```

- [ ] **Step 5: Register `BandDefaults.cpp` in the build**

Append to the source list in `CMakeLists.txt` near [line 383](../../../CMakeLists.txt) (where `src/models/SliceModel.cpp` is listed — add alphabetically adjacent):

```cmake
    src/models/BandDefaults.cpp
    src/models/Band.cpp
```
(If `src/models/Band.cpp` is already listed, add only the `BandDefaults.cpp` line adjacent to it. Check with `grep -n 'src/models/' CMakeLists.txt` first.)

- [ ] **Step 6: Run tests to verify they pass**

```bash
cmake --build build -j$(nproc) --target tst_band_defaults && ctest --test-dir build -R tst_band_defaults -V
```
Expected: 5/5 cases PASS.

- [ ] **Step 7: Register the new file in GPL compliance inventory**

The pre-commit hook `check-new-ports.py` requires every new C++ file touching Thetis logic to appear in `docs/attribution/THETIS-PROVENANCE.md`. Add one row (after the `SliceModel.cpp` row, around line 247):

```markdown
| src/models/BandDefaults.h | Project Files/Source/Console/clsBandStackManager.cs | 2099-2176 | port | thetis-samphire | Seed table for band-button auto-mode (#118). Region 2 first-voice-entry pick. |
| src/models/BandDefaults.cpp | Project Files/Source/Console/clsBandStackManager.cs | 2099-2176 | port | thetis-samphire | Seed table for band-button auto-mode (#118). Region 2 first-voice-entry pick. |
```

- [ ] **Step 8: Commit**

```bash
git add src/models/BandDefaults.h src/models/BandDefaults.cpp \
        tests/tst_band_defaults.cpp tests/CMakeLists.txt \
        CMakeLists.txt docs/attribution/THETIS-PROVENANCE.md
git commit -m "$(cat <<'EOF'
feat(models): add BandDefaults seed table (#118)

13 bands seeded from Thetis Region 2 AddRegion2BandStack
(clsBandStackManager.cs:2099-2176 [v2.10.3.13]). First-voice-entry pick:
30m=CWU (voice prohibited), 60m=USB (channelized), all other ham bands
LSB below 10 MHz / USB above. WWV + GEN use SAM per Thetis. XVTR marked
invalid; handler will no-op until transverter config ships.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: `RadioModel::onBandButtonClicked(Band)` handler

The central behavior. Single slot that the two band-button entry points both route through. Per the spec, order is:
1. Guard on active slice.
2. Compute `current` band from the slice's current frequency.
3. If `clicked == current`, return (no-op).
4. `saveToSettings(current)` to snapshot outgoing state.
5. If `hasSettingsFor(clicked)`, `restoreFromSettings(clicked)` — last-used wins.
6. Otherwise pull the seed, set freq + mode, `saveToSettings(clicked)` to bake it.
7. XVTR with no seed and no persisted state → info log and return.

**Files:**
- Modify: `src/models/RadioModel.h` (declare the slot)
- Modify: `src/models/RadioModel.cpp` (implement the slot)
- Test: `tests/tst_radio_model_band_click.cpp` *(new)*

- [ ] **Step 1: Write the failing test**

Create `tests/tst_radio_model_band_click.cpp`:

```cpp
// tst_radio_model_band_click.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// Unit tests for RadioModel::onBandButtonClicked(Band) — the central
// band-button handler for issue #118. Exercises the three branches:
// same-band no-op, first-visit seed, second-visit restore. Also covers
// null-active-slice, XVTR, and locked-slice edge cases from the spec.
//
// The tests drive RadioModel directly (no live radio, no WDSP) and
// observe SliceModel state and AppSettings writes.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/BandDefaults.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRadioModelBandClick : public QObject {
    Q_OBJECT

private:
    static void clearAllBandKeys() {
        auto& s = AppSettings::instance();
        for (const QString& band : {
                 QStringLiteral("160m"), QStringLiteral("80m"),
                 QStringLiteral("60m"),  QStringLiteral("40m"),
                 QStringLiteral("30m"),  QStringLiteral("20m"),
                 QStringLiteral("17m"),  QStringLiteral("15m"),
                 QStringLiteral("12m"),  QStringLiteral("10m"),
                 QStringLiteral("6m"),   QStringLiteral("WWV"),
                 QStringLiteral("GEN"),  QStringLiteral("XVTR") }) {
            const QString bp = QStringLiteral("Slice0/Band") + band + QStringLiteral("/");
            for (const QString& field : {
                     QStringLiteral("Frequency"),  QStringLiteral("DspMode"),
                     QStringLiteral("FilterLow"),  QStringLiteral("FilterHigh"),
                     QStringLiteral("AgcMode"),    QStringLiteral("StepHz") }) {
                s.remove(bp + field);
            }
        }
    }

private slots:
    void init() { clearAllBandKeys(); }
    void cleanup() { clearAllBandKeys(); }

    void first_visit_seeds_freq_and_mode() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceAt(0);
        QVERIFY(slice);
        // Start on 40m so the 20m click is a genuine cross-band event.
        slice->setFrequency(7100000.0);
        slice->setDspMode(DSPMode::LSB);

        radio.onBandButtonClicked(Band::Band20m);

        QCOMPARE(slice->frequency(), 14155000.0);   // seed freq
        QCOMPARE(slice->dspMode(),   DSPMode::USB); // seed mode
    }

    void second_visit_restores_last_used() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceAt(0);

        // First visit 20m: seeds 14.155 USB.
        slice->setFrequency(7100000.0);
        slice->setDspMode(DSPMode::LSB);
        radio.onBandButtonClicked(Band::Band20m);

        // User tunes to 14.100 and switches to CWU.
        slice->setFrequency(14100000.0);
        slice->setDspMode(DSPMode::CWU);

        // Jump to 40m (persists 20m state).
        radio.onBandButtonClicked(Band::Band40m);
        QCOMPARE(slice->frequency(), 7152000.0);   // 40m seed
        QCOMPARE(slice->dspMode(),   DSPMode::LSB);

        // Jump back to 20m — should restore, NOT re-seed.
        radio.onBandButtonClicked(Band::Band20m);
        QCOMPARE(slice->frequency(), 14100000.0);
        QCOMPARE(slice->dspMode(),   DSPMode::CWU);
    }

    void same_band_is_no_op() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceAt(0);
        slice->setFrequency(14100000.0);
        slice->setDspMode(DSPMode::USB);

        const double  freqBefore = slice->frequency();
        const DSPMode modeBefore = slice->dspMode();

        // 14.100 is inside 20m; clicking 20m again is the reproducer
        // for the same-band no-op branch.
        radio.onBandButtonClicked(Band::Band20m);

        QCOMPARE(slice->frequency(), freqBefore);
        QCOMPARE(slice->dspMode(),   modeBefore);
    }

    void xvtr_no_seed_no_persisted_is_no_op() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceAt(0);
        slice->setFrequency(14100000.0);   // on 20m
        slice->setDspMode(DSPMode::USB);

        radio.onBandButtonClicked(Band::XVTR);

        // Neither freq nor mode should change.
        QCOMPARE(slice->frequency(), 14100000.0);
        QCOMPARE(slice->dspMode(),   DSPMode::USB);
    }

    void null_active_slice_is_safe() {
        RadioModel radio;
        // No slice added — activeSlice() returns nullptr.
        // Must not crash, must not emit any signals we can observe.
        radio.onBandButtonClicked(Band::Band20m);
        QVERIFY(radio.activeSlice() == nullptr);
    }

    void locked_slice_freezes_freq_but_mode_still_changes() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceAt(0);
        slice->setFrequency(7100000.0);   // on 40m
        slice->setDspMode(DSPMode::LSB);
        slice->setLocked(true);

        radio.onBandButtonClicked(Band::Band20m);

        // Lock blocks setFrequency, so freq stays at 7.1 MHz.
        // Mode is not locked and does move to the 20m seed (USB).
        QCOMPARE(slice->frequency(), 7100000.0);
        QCOMPARE(slice->dspMode(),   DSPMode::USB);
    }

    void save_on_exit_persists_current_band_state() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceAt(0);
        slice->setFrequency(14100000.0);   // 20m
        slice->setDspMode(DSPMode::CWU);

        radio.onBandButtonClicked(Band::Band40m);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("Slice0/Band20m/Frequency")).toDouble(),
                 14100000.0);
        QCOMPARE(s.value(QStringLiteral("Slice0/Band20m/DspMode")).toInt(),
                 static_cast<int>(DSPMode::CWU));
    }
};

QTEST_MAIN(TestRadioModelBandClick)
#include "tst_radio_model_band_click.moc"
```

- [ ] **Step 2: Register the test and run to verify failure**

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_radio_model_band_click)
```

Run:
```bash
cmake --build build -j$(nproc) --target tst_radio_model_band_click 2>&1 | tail -15
```
Expected: build fails with `no member named 'onBandButtonClicked' in 'NereusSDR::RadioModel'`.

- [ ] **Step 3: Declare the slot in `src/models/RadioModel.h`**

Find the existing `public:` slice-management block (around line 191-197 where `activeSlice()` and `setActiveSlice()` live). Insert after the `setActiveSlice(int index);` declaration:

```cpp
    // Band-button click handler. Routes both SpectrumOverlayPanel::bandSelected
    // and ContainerWidget::bandClicked through one code path. On first
    // visit to `band`, applies BandDefaults::seedFor(band) and persists;
    // on subsequent visits, restores last-used per-band state via the
    // 3G-10 Stage 2 persistence already on SliceModel.
    //
    // Same-band click is a no-op. XVTR with no seed and no persisted
    // state is a logged no-op. Locked slices freeze frequency (mode still
    // changes, matching Thetis lock semantics).
    //
    // Acts on activeSlice(). No-op if active slice is null.
    //
    // Issue #118.
    void onBandButtonClicked(NereusSDR::Band band);
```

- [ ] **Step 4: Implement the slot in `src/models/RadioModel.cpp`**

Find a stable insertion point in `src/models/RadioModel.cpp`. A good anchor is just after the existing `void RadioModel::setActiveSlice(int index)` method. If uncertain, run `grep -n 'void RadioModel::setActiveSlice' src/models/RadioModel.cpp` to locate, then insert the new method immediately after its closing brace.

Required includes — at the top of `RadioModel.cpp`, near the existing `#include "models/Band.h"` and `#include "models/SliceModel.h"` lines, add:

```cpp
#include "models/BandDefaults.h"
```

Implementation:

```cpp
void RadioModel::onBandButtonClicked(Band band)
{
    SliceModel* slice = activeSlice();
    if (!slice) {
        // No active slice (pre-connection, between-slice teardown, etc.).
        // Silent — avoids log spam from UI events firing during startup.
        return;
    }

    const Band current = bandFromFrequency(slice->frequency());
    if (band == current) {
        // Same-band click — design decision Q1(a). Keeps UX predictable;
        // avoids yanking the VFO when the user is already in the band.
        return;
    }

    // Snapshot outgoing band before we touch freq/mode.
    slice->saveToSettings(current);

    if (slice->hasSettingsFor(band)) {
        // Second+ visit: restore last-used state for the clicked band.
        slice->restoreFromSettings(band);
        return;
    }

    // First visit: apply the seed if one exists, otherwise no-op.
    BandSeed seed = BandDefaults::seedFor(band);
    if (!seed.valid) {
        // XVTR today. Becomes meaningful once the XVTR epic ships.
        qCInfo(lcRadio) << "onBandButtonClicked: band" << bandLabel(band)
                        << "has no seed and no persisted state — no-op";
        return;
    }

    // Order: freq before mode. Matches Thetis SetBand (console.cs:5867-5886).
    // Alex band-dependent filters track the freq change before mode-
    // dependent bandwidth is applied.
    slice->setFrequency(seed.frequencyHz);
    slice->setDspMode(seed.mode);
    slice->saveToSettings(band);   // Bake seed for next visit.
}
```

If `lcRadio` (the logging category) is not already declared in `RadioModel.cpp`, check with `grep -n "lcRadio\|Q_LOGGING_CATEGORY" src/models/RadioModel.cpp | head -5`. If absent, the method can use `lcConnection` (which is declared and used throughout the file), swapping the category name accordingly. The category name is not load-bearing for correctness.

- [ ] **Step 5: Run tests to verify all pass**

```bash
cmake --build build -j$(nproc) --target tst_radio_model_band_click && ctest --test-dir build -R tst_radio_model_band_click -V
```
Expected: 7/7 cases PASS.

- [ ] **Step 6: Re-run the band-change recursion regression test**

There is a prior regression (tst_band_change_recursion_guard) that specifically asserts wheel-tuning across a band boundary does NOT recurse or snap-back. The new handler must not break it.

```bash
ctest --test-dir build -R tst_band_change_recursion_guard -V
```
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp \
        tests/tst_radio_model_band_click.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(models): RadioModel::onBandButtonClicked handler (#118)

Central handler for both band-button entry points. On first visit to
a band, applies BandDefaults seed; on subsequent visits, restores
last-used state via 3G-10 Stage 2 per-band persistence already on
SliceModel. Same-band click is a no-op (design decision Q1(a)); XVTR
with no seed and no persisted state is a logged no-op; locked slices
freeze freq but still change mode (matches Thetis lock semantics).

Order: setFrequency then setDspMode — matches Thetis SetBand
(console.cs:5867-5886 [v2.10.3.13]). Alex filters track the freq
change before mode-dependent bandwidth is applied.

Does not interact with the v0.2.0 band-crossing recursion guard —
restore is reserved for the explicit band-button press path per
RadioModel.cpp:1084-1095 commentary, which is exactly this path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Route `SpectrumOverlayPanel::bandSelected` through the new handler

Replace the existing lambda body that calls `setFrequency` and discards `mode`. The signature of `bandSelected(QString name, double freqHz, QString mode)` stays unchanged to avoid touching `SpectrumOverlayPanel` itself — the `freqHz` / `mode` arguments are simply no longer consulted.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (replace lambda body around line 2612-2615)

- [ ] **Step 1: Find the existing connection**

```bash
grep -n "overlayPanel.*bandSelected" src/gui/MainWindow.cpp
```
Expected: one match near line 2612 inside `MainWindow::wireSliceToSpectrum()`.

- [ ] **Step 2: Replace the lambda body**

Replace the block at `src/gui/MainWindow.cpp:2610-2616` verbatim:

FROM:
```cpp
    // --- Wire overlay Band flyout to slice ---
    if (m_overlayPanel) {
        connect(m_overlayPanel, &SpectrumOverlayPanel::bandSelected,
                this, [slice](const QString& /*name*/, double freqHz, const QString& /*mode*/) {
            slice->setFrequency(freqHz);
        });
    }
```

TO:
```cpp
    // --- Wire overlay Band flyout to RadioModel band-click handler (#118) ---
    // The signal still carries legacy (name, freqHz, mode) args, but the
    // handler owns seed/restore policy now. Only the name is consulted.
    if (m_overlayPanel) {
        connect(m_overlayPanel, &SpectrumOverlayPanel::bandSelected,
                this, [this](const QString& name, double /*freqHz*/, const QString& /*mode*/) {
            m_radioModel->onBandButtonClicked(bandFromName(name));
        });
    }
```

- [ ] **Step 3: Ensure `bandFromName` is reachable from MainWindow.cpp**

`MainWindow.cpp` already includes `models/Band.h` indirectly via `models/SliceModel.h`, but verify explicitly:

```bash
grep -n '"models/Band.h"\|<models/Band.h>' src/gui/MainWindow.cpp
```

If no match, add the include near the other `models/` includes at the top of the file:

```cpp
#include "models/Band.h"
```

Also confirm the namespace import. The file uses `NereusSDR::` or a `using namespace NereusSDR;` somewhere — check:

```bash
grep -n "using namespace NereusSDR\|NereusSDR::bandFromName" src/gui/MainWindow.cpp | head -5
```

If `using namespace NereusSDR;` is not in scope, qualify the call as `NereusSDR::bandFromName(name)` and `NereusSDR::Band` accordingly. Do not add a new `using namespace` — match the file's existing style.

- [ ] **Step 4: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
```
Expected: build succeeds.

- [ ] **Step 5: Run the full unit test suite**

```bash
ctest --test-dir build -j$(nproc) 2>&1 | tail -20
```
Expected: all tests pass. The four new Task 1-4 tests plus the prior recursion-guard test should all be green.

- [ ] **Step 6: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
fix(main): route overlay Band flyout through onBandButtonClicked (#118)

Previously the SpectrumOverlayPanel::bandSelected lambda called
slice->setFrequency(freqHz) and silently discarded the mode argument.
Clicking 80m tuned the VFO to 3.5 MHz but left mode stale — the user's
reproducer for #118.

The lambda now delegates to RadioModel::onBandButtonClicked(Band), which
owns seed/restore policy and always sets mode alongside frequency.
Signal signature stays the same; the legacy freqHz/mode args are
ignored (handler derives the correct values from BandDefaults or
per-band persistence).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Wire `ContainerWidget::bandClicked` through the new handler

Hook on `ContainerManager::containerAdded` so every container (default or user-created, new or restored) has the connection attached. The existing container-caps push at [MainWindow.cpp:584-589](../../../src/gui/MainWindow.cpp:584) is the right pattern to follow.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (extend the `containerAdded` lambda)

- [ ] **Step 1: Find the existing `containerAdded` handler**

```bash
grep -n "containerAdded" src/gui/MainWindow.cpp
```
Expected: two matches — the `connect` call at line 584, and the lambda immediately after.

- [ ] **Step 2: Extend the lambda to also connect `bandClicked`**

Replace the block at `src/gui/MainWindow.cpp:584-589` verbatim:

FROM:
```cpp
    connect(m_containerManager, &ContainerManager::containerAdded, this,
            [this](const QString& id) {
        if (auto* c = m_containerManager->container(id)) {
            c->setBoardCapabilities(m_radioModel->boardCapabilities());
        }
    });
```

TO:
```cpp
    connect(m_containerManager, &ContainerManager::containerAdded, this,
            [this](const QString& id) {
        if (auto* c = m_containerManager->container(id)) {
            c->setBoardCapabilities(m_radioModel->boardCapabilities());
            // Phase 3G-?? (issue #118) — route BandButtonItem clicks inside
            // this container through the RadioModel handler so they get
            // the same seed/restore behavior as the overlay flyout.
            connect(c, &ContainerWidget::bandClicked, this, [this](int idx) {
                m_radioModel->onBandButtonClicked(bandFromUiIndex(idx));
            });
        }
    });
```

**Also hook the containers that existed at startup** — `containerAdded` fires for future adds, but not for the ones `restoreState()` / `createDefaultContainers()` materialized before the signal was connected. Scan the startup block for a loop over `m_containerManager->allContainers()`. If no such loop wires `bandClicked`, add one immediately before the `m_containerManager->restoreState()` call (around line 615). Use this snippet:

```cpp
    // Issue #118 — retrofit bandClicked on containers created during
    // restoreState() / createDefaultContainers(). containerAdded covers
    // future adds; this loop covers the initial set.
    auto wireContainerBandClick = [this](ContainerWidget* c) {
        if (!c) { return; }
        connect(c, &ContainerWidget::bandClicked, this, [this](int idx) {
            m_radioModel->onBandButtonClicked(bandFromUiIndex(idx));
        });
    };
    // Wired here so both the in-lambda path (containerAdded, for later
    // user-created containers) and this post-restore sweep call the same
    // helper — no drift.
    QObject::connect(m_containerManager, &ContainerManager::containerAdded, this,
                     [this, wireContainerBandClick](const QString& id) {
        wireContainerBandClick(m_containerManager->container(id));
    });
    // NOTE: the earlier connect at line ~584 already wires caps; leave it.
    // Call the sweep once restoreState() has materialized the initial set.
```

Then, immediately after `m_containerManager->restoreState();` at line 615, add:

```cpp
    for (ContainerWidget* c : m_containerManager->allContainers()) {
        wireContainerBandClick(c);
    }
```

**If the existing `containerAdded` connection already does exactly this work, delete the duplicate block.** Read the final file with `grep -n "containerAdded\|bandClicked" src/gui/MainWindow.cpp` before moving on — there should be exactly **one** place that connects `bandClicked` per container, reachable from both startup and runtime adds.

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
```
Expected: build succeeds.

- [ ] **Step 4: Run the full unit test suite**

```bash
ctest --test-dir build -j$(nproc) 2>&1 | tail -20
```
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
fix(main): wire container BandButtonItem clicks (#118)

ContainerWidget::bandClicked was orphaned — the signal was emitted but
no slot consumed it, so clicking a band button inside a meter container
did nothing (no freq change, no mode change).

Both the containerAdded signal (runtime adds) and a post-restoreState
sweep (default + persisted containers) now connect each container's
bandClicked to RadioModel::onBandButtonClicked via bandFromUiIndex.
Shares one helper lambda so there's no drift between the two code paths.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Build, smoke, and manual verification

This is the verification-before-completion gate. No code changes — just confirm the fix works end-to-end.

- [ ] **Step 1: Full clean build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```
Expected: no errors, no warnings from the touched files.

- [ ] **Step 2: Full ctest**

```bash
ctest --test-dir build -j$(nproc) --output-on-failure 2>&1 | tail -30
```
Expected: all tests PASS (including the four new ones and the existing recursion-guard test).

- [ ] **Step 3: Launch the app**

Per user standing rule ("auto-launch after build"):

```bash
# Kill any existing instance first.
pkill -f NereusSDR 2>/dev/null; sleep 1
./build/NereusSDR &
```

- [ ] **Step 4: Manual verification — Path A (overlay flyout) — the #118 reproducer**

Without connecting to a radio (so the test is deterministic and reproducible even off the bench):

1. Wait for the main window to appear.
2. Click the `BAND` button on the spectrum overlay (left-edge 10-button strip) to open the flyout.
3. Click **80m**.
4. Observe the VFO flag:
   - Frequency should read **3.650.000** (3.65 MHz).
   - Mode tab should show **LSB**.
5. Click **20m** in the same flyout.
   - Frequency should read **14.155.000**.
   - Mode should show **USB**.
6. Click **80m** again.
   - Frequency should return to 3.650.000 (restored from what was just persisted).
   - Mode should be LSB.

If any of the above fails, STOP and diagnose before claiming the task complete.

- [ ] **Step 5: Manual verification — Path B (container band buttons)**

1. Ensure there is a meter container visible with the 12-button band grid (the default layout has one).
2. Click the **40m** button inside the container grid.
   - VFO should jump to **7.152.000** LSB.
3. Click **15m**.
   - VFO should jump to **21.205.000** USB.
4. Click **40m** again.
   - VFO should restore to 7.152.000 LSB.

- [ ] **Step 6: Manual verification — Restore path**

1. Click **20m** (seeds 14.155.000 USB if not already visited this session).
2. Manually tune VFO to **14.074.000** and switch mode to **DIGU** (FT8 frequency).
3. Click **40m** (any band).
4. Click **20m**.
5. Expected: VFO returns to **14.074.000 DIGU** — the last-used state, not the seed.

- [ ] **Step 7: Manual verification — Same-band no-op**

1. VFO on 14.074 MHz (inside 20m from Step 6).
2. Click **20m**.
3. Expected: VFO stays on 14.074 MHz, mode stays DIGU. Nothing changes.

- [ ] **Step 8: Close #118 via PR**

Assuming the branch was opened for this work, push and open a PR referencing #118. Example:

```bash
git push -u origin claude/suspicious-davinci-a2609e
gh pr create --title "fix: band button auto-mode (#118)" --body "$(cat <<'EOF'
## Summary
- Wires the two band-button entry points (`SpectrumOverlayPanel::bandSelected` and `ContainerWidget::bandClicked`) through one handler, `RadioModel::onBandButtonClicked(Band)`.
- First click on a band applies a Thetis Region 2 seed (LSB/USB/CWU/SAM per [BandDefaults.cpp](../blob/HEAD/src/models/BandDefaults.cpp) with inline `clsBandStackManager.cs:2099-2176 [v2.10.3.13]` cites).
- Subsequent clicks restore last-used state via the existing Phase 3G-10 Stage 2 per-band persistence.
- Same-band click is a no-op; XVTR with no seed + no persisted state is a logged no-op; locked slices freeze freq but still change mode.

Design: `docs/architecture/band-button-auto-mode-design.md`
Plan: `docs/superpowers/plans/2026-04-23-band-button-auto-mode.md`

Closes #118.

## Test plan
- [ ] `ctest -R 'tst_band_defaults|tst_band_from_name|tst_slice_has_settings_for|tst_radio_model_band_click|tst_band_change_recursion_guard'` — all pass
- [ ] Overlay flyout: click 80m → VFO lands on 3.650 MHz LSB (was the #118 reproducer)
- [ ] Container band button grid: click 40m → VFO lands on 7.152 MHz LSB
- [ ] Tune 20m to 14.074 DIGU; switch bands; return to 20m → restores 14.074 DIGU (not the seed)
- [ ] Click 20m while already on 20m → no-op

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

> Per user standing rule `feedback_no_public_posts_without_review`: draft the PR body in chat first, wait for explicit "post it" before running `gh pr create`. The block above is the draft — do NOT execute until the user approves.

---

## Self-Review

**Spec coverage:**
- ✅ Section 1 (Architecture) — Tasks 1-6 touch exactly the files listed.
- ✅ Section 2 (Seed table) — Task 3 builds the `BandDefaults` module with all 13 seeds at their cited Thetis line numbers. Task 3 test case `seeds_match_thetis_region2_values` asserts each freq/mode pair.
- ✅ Section 3 (Wiring) — Task 5 replaces the overlay lambda; Task 6 adds the container connection plus a post-restoreState sweep for the pre-existing containers the `containerAdded` signal wouldn't catch.
- ✅ Section 4 (Persistence) — Task 2 adds `hasSettingsFor`; Task 4's handler calls the existing `saveToSettings` / `restoreFromSettings`.
- ✅ Section 5 (Edge cases) — Task 4 tests cover all five cases (null slice, locked, XVTR, same-band, freq-outside-band is implicitly covered by `bandFromFrequency` being deterministic).
- ✅ Section 6 (Testing) — Tasks 1-4 each produce a focused unit-test binary; Task 7 provides manual verification steps.

**Placeholder scan:** Every code step shows the actual code. No "TBD", "similar to…", or "add appropriate handling". Thetis cite stamps are present on every seed.

**Type consistency:** `BandSeed` struct shape matches across the header declaration, the `seedFor` return statements, and the test helper. `DSPMode` values (`LSB`, `USB`, `CWU`, `SAM`) are consistent. `bandFromName` / `bandFromUiIndex` / `bandFromFrequency` are distinct functions with non-overlapping signatures.

**Known runtime interaction:** The handler's `setFrequency` and `setDspMode` calls trigger the existing `frequencyChanged` → T10 band-crossing lambda in `RadioModel.cpp:1096`. That lambda does NOT recall bandstack state — per the deliberate v0.2.0 fix at `RadioModel.cpp:1084-1095` — so no recursion path through `onBandButtonClicked`. Task 4 Step 6 re-runs `tst_band_change_recursion_guard` to catch any regression.

---

Plan complete and saved to `docs/superpowers/plans/2026-04-23-band-button-auto-mode.md`.

**Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

**Which approach?**
