// tests/tst_smeter_widget_scale.cpp
//
// Pins the setPowerScale snap-logic for SMeterWidget.
// From AetherSDR src/gui/SMeterWidget.cpp:642-656 [@0cd4559]:
//   setPowerScale(int maxWatts, bool hasAmplifier) maps to
//   (m_powerScaleMax, m_powerRedStart) via three branches:
//     1. hasAmplifier == true           -> (2000.0, 1500.0)   PGXL 2kW snap
//     2. !hasAmplifier && maxWatts > 100 -> (600.0,  500.0)   Aurora/Overlord
//     3. !hasAmplifier && maxWatts <= 100 -> (120.0,  100.0)  barefoot default
//
// Phase 3P-II Phase 2, Tasks 34/35/36.
//
// Modification history (NereusSDR):
//   2026-10-26 - The VHF S9 boundary at 30 MHz, the appended TX modes'
//                 scales, and a 50 W display limit. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Final review fix wave (I6, M3): Max Bin takes no VHF S9
//                 offset; ALC Group follows Thetis's uneven calibration.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include "gui/SMeterWidget.h"
#include "gui/meters/AboveS9Frequency.h"

using NereusSDR::SMeterWidget;

class SMeterWidgetScaleTest : public QObject {
    Q_OBJECT
private slots:
    void powerScaleBarefootDefault();
    void powerScaleAuroraOverlord();
    void powerScalePgxlAmplifier();
    void vhfS9BoundaryAt30MHz();
    void appendedTxModesKeepTheOldValues();
    void appendedTxModeScales();
    void powerDisplayLimitPutsHalfAtMidScale();
    void maxBinTakesNoVhfOffset();
};

void SMeterWidgetScaleTest::powerScaleBarefootDefault()
{
    NereusSDR::SMeterWidget w;
    w.setPowerScale(100, false);  // barefoot: maxWatts == 100, not > 100
    QCOMPARE(w.testPowerScaleMax(), 120.0f);
    QCOMPARE(w.testPowerRedStart(), 100.0f);
}

void SMeterWidgetScaleTest::powerScaleAuroraOverlord()
{
    NereusSDR::SMeterWidget w;
    w.setPowerScale(500, false);  // Aurora: maxWatts 500 > 100 -> 600/500
    QCOMPARE(w.testPowerScaleMax(), 600.0f);
    QCOMPARE(w.testPowerRedStart(), 500.0f);
}

void SMeterWidgetScaleTest::powerScalePgxlAmplifier()
{
    NereusSDR::SMeterWidget w;
    w.setPowerScale(0, true);  // PGXL: hasAmplifier=true -> 2000/1500
    QCOMPARE(w.testPowerScaleMax(), 2000.0f);
    QCOMPARE(w.testPowerRedStart(), 1500.0f);
}

// At exactly 30.000 MHz S9 is -93 dBm; at 29.999 MHz it is -73 dBm.
void SMeterWidgetScaleTest::vhfS9BoundaryAt30MHz()
{
    SMeterWidget at30(SMeterWidget::SettingsStore::Instance);
    at30.setAboveS9Frequency(NereusSDR::isAboveS9Frequency(30000000.0));
    QVERIFY(at30.aboveS9Frequency());
    at30.setLevel(-93.0f);
    QCOMPARE(at30.sUnitsText(), QStringLiteral("S9"));
    QCOMPARE(at30.testSUnitsReadout(), QStringLiteral("S9"));
    QCOMPARE(at30.testDbmReadout(), QStringLiteral("-93 dBm"));
    QCOMPARE(at30.testNeedleTarget(), 0.6f);
    at30.setLevel(-73.0f);
    QCOMPARE(at30.sUnitsText(), QStringLiteral("S9+20"));

    SMeterWidget below(SMeterWidget::SettingsStore::Instance);
    below.setAboveS9Frequency(NereusSDR::isAboveS9Frequency(29999000.0));
    QVERIFY(!below.aboveS9Frequency());
    below.setLevel(-73.0f);
    QCOMPARE(below.sUnitsText(), QStringLiteral("S9"));
    QCOMPARE(below.testNeedleTarget(), 0.6f);
    below.setLevel(-93.0f);
    QCOMPARE(below.sUnitsText(), QStringLiteral("S6"));

    // No reading stays no reading above 30 MHz.
    at30.setLevel(-400.0f);
    QCOMPARE(at30.sUnitsText(), QStringLiteral("--"));
    QCOMPARE(at30.testNeedleTarget(), 0.0f);
}

// Appended without renumbering: a saved SMeter_TxSelect or dial keeps its
// meaning.
void SMeterWidgetScaleTest::appendedTxModesKeepTheOldValues()
{
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Power), 0);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::SWR), 1);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Level), 2);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Compression), 3);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Alc), 4);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::AlcGroup), 5);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Volts), 6);
    QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Amps), 7);
    QCOMPARE(SMeterWidget::kTxModeCount, 8);
}

// Thetis ANANMM's ranges: ALC compression 0..30 dB, ALC group -30..25 dB,
// volts 10..15 V, amps 0..20 A.
void SMeterWidgetScaleTest::appendedTxModeScales()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    w.setTransmitting(true);
    w.setAlcMeters(15.0f, -30.0f);
    w.setSupplyMeters(12.5f, 20.0f);

    w.setTxMode(QStringLiteral("ALC Compression"));
    QCOMPARE(w.txMode(), SMeterWidget::TxMode::Alc);
    QCOMPARE(w.testTxValue(), 15.0f);
    QCOMPARE(w.testNeedleTarget(), 0.5f);
    QCOMPARE(w.testTxReadout(), QStringLiteral("15 dB"));

    // ALC Group on Thetis's calibration:
    // -30 at 0, 0 at half, 25 at 0.99.
    w.setTxMode(QStringLiteral("ALC Group"));
    QCOMPARE(w.txMode(), SMeterWidget::TxMode::AlcGroup);
    QCOMPARE(w.testNeedleTarget(), 0.0f);
    w.setAlcMeters(15.0f, 0.0f);
    QCOMPARE(w.testNeedleTarget(), 0.5f);
    w.setAlcMeters(15.0f, 25.0f);
    QCOMPARE(w.testNeedleTarget(), 0.99f);
    QCOMPARE(w.testTxReadout(), QStringLiteral("25 dB"));

    w.setTxMode(QStringLiteral("Volts"));
    QCOMPARE(w.txMode(), SMeterWidget::TxMode::Volts);
    QCOMPARE(w.testNeedleTarget(), 0.5f);
    QCOMPARE(w.testTxReadout(), QStringLiteral("12.5 V"));

    w.setTxMode(QStringLiteral("Amps"));
    QCOMPARE(w.txMode(), SMeterWidget::TxMode::Amps);
    QCOMPARE(w.testNeedleTarget(), 1.0f);
    QCOMPARE(w.testTxReadout(), QStringLiteral("20.0 A"));

    // Each face draws every appended scale (no crash, no empty paint).
    for (SMeterWidget::FaceStyle face : {SMeterWidget::FaceStyle::AgedCream, SMeterWidget::FaceStyle::Classic}) {
        w.setFaceStyle(face);
        w.resize(298, 140);
        for (const char* mode : {"ALC Compression", "ALC Group", "Volts", "Amps"}) {
            w.setTxMode(QString::fromLatin1(mode));
            QVERIFY(!w.grab().isNull());
        }
    }
}

// A 50 W limit with the scale's top at it: 25 W is mid-scale.
void SMeterWidgetScaleTest::powerDisplayLimitPutsHalfAtMidScale()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    w.setPowerDisplayScale(50.0, 50.0);
    w.setTransmitting(true);
    w.setTxMeters(25.0f, 1.0f);
    QCOMPARE(w.testNeedleTarget(), 0.5f);
}

// Fix wave I6: above 30 MHz Thetis raises Signal and its average by 20 dB
// (its getPerc), never Max Bin.
void SMeterWidgetScaleTest::maxBinTakesNoVhfOffset()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    w.setAboveS9Frequency(true);
    w.setRxMode(QStringLiteral("Max Bin"));
    QCOMPARE(w.rxMode(), SMeterWidget::RxMode::MaxBin);
    w.setLevel(-73.0f);
    QCOMPARE(w.sUnitsText(), QStringLiteral("S9"));
    QCOMPARE(w.testNeedleTarget(), 0.6f);
    w.setRxMode(QStringLiteral("Sig Avg"));
    w.setLevel(-73.0f);
    QCOMPARE(w.sUnitsText(), QStringLiteral("S9+20"));
}

QTEST_MAIN(SMeterWidgetScaleTest)
#include "tst_smeter_widget_scale.moc"
