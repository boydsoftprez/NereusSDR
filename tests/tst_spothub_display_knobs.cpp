// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test file. SpectrumWidget itself
// is a NereusSDR port of AetherSDR's spectrum renderer, but the
// `loadSpotDisplaySettings` helper exercised here is a NereusSDR
// addition introduced by Phase 3J-2 + 3R M2.
//
// NereusSDR - Phase 3J-2 + 3R M2: SpotHub Display tab knobs end-to-end.
//
// SpotHubDialog F4 (commit on 2026-05-11) wires every Display tab knob
// to AppSettings on construction (knob seeded from key) and on change
// (knob -> AppSettings + emit settingsChanged). M2 closes the loop:
// after settingsChanged fires, the new values must flow from AppSettings
// into the live spectrum overlay. This file pins the consumer side
// of that loop.
//
// What is verified:
//   * SpectrumWidget::loadSpotDisplaySettings reads every Display tab
//     key (IsSpotsEnabled, SpotFontSize, SpotsMaxLevel,
//     SpotsStartingHeightPercentage, IsSpotsOverrideColorsEnabled,
//     IsSpotsOverrideBackgroundColorsEnabled, SpotsOverrideColor,
//     SpotsOverrideBgColor, SpotsBackgroundOpacity) and pushes the new
//     values into the corresponding SpectrumWidget setters.
//   * Defaults applied when keys are absent match the F4 buildDisplayTab
//     read-side defaults (SpotHubDialog.cpp:1714-1730).
//
// Why this is the cleanest seam: MainWindow itself is too heavy to
// construct in a unit test (see tst_mainwindow_status_bar_safety for
// the established pattern), so the M2 plumbing extracts the read-and-
// push loop into a single SpectrumWidget method that AppSettings drives
// directly. MainWindow's slot is now a one-liner around that call.
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 + 3R M2 initial commit.
//                                    AI tooling: Anthropic Claude Code.

#include <QtTest/QtTest>
#include <QColor>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

AppSettings& testSettings()
{
    return AppSettings::instance();
}

} // namespace

class TestSpotHubDisplayKnobs : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        testSettings().clear();
    }

    void cleanup()
    {
        testSettings().clear();
    }

    // ── loadSpotDisplaySettings applies persisted values ───────────────

    void loadSpotDisplaySettingsAppliesPersistedShowSpots()
    {
        testSettings().setValue("IsSpotsEnabled", "False");

        SpectrumWidget w;
        w.loadSpotDisplaySettings();

        QCOMPARE(w.showSpots(), false);
    }

    void loadSpotDisplaySettingsDefaultsShowSpotsTrue()
    {
        // Key absent.
        QVERIFY(!testSettings().contains("IsSpotsEnabled"));

        SpectrumWidget w;
        w.loadSpotDisplaySettings();

        // F4 default at SpotHubDialog.cpp:1714 is "True".
        QCOMPARE(w.showSpots(), true);
    }

    void loadSpotDisplaySettingsAppliesAllNumericKnobs()
    {
        testSettings().setValue("SpotFontSize", 22);
        testSettings().setValue("SpotsMaxLevel", 5);
        testSettings().setValue("SpotsStartingHeightPercentage", 35);
        testSettings().setValue("SpotsBackgroundOpacity", 80);

        SpectrumWidget w;
        w.loadSpotDisplaySettings();

        QCOMPARE(w.spotFontSizeForTest(), 22);
        QCOMPARE(w.spotMaxLevelsForTest(), 5);
        QCOMPARE(w.spotStartPctForTest(), 35);
        QCOMPARE(w.spotBgOpacityForTest(), 80);
    }

    void loadSpotDisplaySettingsAppliesColors()
    {
        testSettings().setValue("SpotsOverrideColor", "#A1B2C3");
        testSettings().setValue("SpotsOverrideBgColor", "#112233");
        testSettings().setValue("IsSpotsOverrideColorsEnabled", "True");
        testSettings().setValue("IsSpotsOverrideBackgroundColorsEnabled",
                                "False");

        SpectrumWidget w;
        w.loadSpotDisplaySettings();

        QCOMPARE(w.spotColorForTest().name().toUpper(), QString("#A1B2C3"));
        QCOMPARE(w.spotBgColorForTest().name().toUpper(), QString("#112233"));
        QCOMPARE(w.spotOverrideColorsForTest(), true);
        QCOMPARE(w.spotOverrideBgForTest(), false);
    }
};

QTEST_MAIN(TestSpotHubDisplayKnobs)
#include "tst_spothub_display_knobs.moc"
