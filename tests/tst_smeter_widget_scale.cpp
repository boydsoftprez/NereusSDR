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

#include <QtTest>
#include "gui/SMeterWidget.h"

class SMeterWidgetScaleTest : public QObject {
    Q_OBJECT
private slots:
    void powerScaleBarefootDefault();
    void powerScaleAuroraOverlord();
    void powerScalePgxlAmplifier();
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

QTEST_MAIN(SMeterWidgetScaleTest)
#include "tst_smeter_widget_scale.moc"
