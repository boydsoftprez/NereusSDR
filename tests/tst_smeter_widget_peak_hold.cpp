// tests/tst_smeter_widget_peak_hold.cpp
//
// Verifies SMeterWidget peak hold decay behavior across the three DecayRate
// values (Fast / Medium / Slow) using the testAdvanceTime() / testPeakLevel()
// test seams.
//
// Decay constants come verbatim from AetherSDR
// src/gui/SMeterWidget.cpp:675-681 [@0cd4559]:
//   Fast   -> 20 dB/s    (m_peakDecayDbPerSec = 20.0)
//   Medium -> 10 dB/s    (m_peakDecayDbPerSec = 10.0)
//   Slow   ->  5 dB/s    (m_peakDecayDbPerSec =  5.0)
//
// Test protocol (per updatePeakHoldValue clamp logic):
//   1. setLevel(-50.0)  -- captures peak at -50 dBm, level = -50 dBm
//   2. setLevel(-127.0) -- drops level to noise floor so the decayed peak
//                          remains above m_levelDbm and is not clamped
//   3. testAdvanceTime(1000) -- injects 1 second of synthetic decay time
//   4. testPeakLevel() reads m_peakHoldDbm after decay
//
// Expected post-1-sec peak values:
//   Fast:   -50 - 20 * 1.0 = -70 dBm  (tolerance +/-1 dB)
//   Medium: -50 - 10 * 1.0 = -60 dBm  (tolerance +/-1 dB)
//   Slow:   -50 -  5 * 1.0 = -55 dBm  (tolerance +/-1 dB)
//
// Phase 3P-II Phase 2, Task 37.

#include <QtTest>
#include "gui/SMeterWidget.h"

class SMeterWidgetPeakHoldTest : public QObject {
    Q_OBJECT
private slots:
    void peakDecayFast();
    void peakDecayMedium();
    void peakDecaySlow();
};

// Helper: construct a widget with peak hold enabled, hold time = 0,
// inject a level, advance synthetic time, and return the peak level.
// holdTimeMs = 0 means the full testAdvanceTime(1000) is decay time.
static NereusSDR::SMeterWidget* makePeakWidget(NereusSDR::SMeterWidget::DecayRate rate)
{
    auto* w = new NereusSDR::SMeterWidget;
    // Bypass the 100 ms minimum clamp so the entire 1000 ms advance is decay.
    w->testSetPeakHoldTimeMs(0);
    w->setPeakHoldEnabled(true);
    w->setPeakDecayRate(rate);
    return w;
}

void SMeterWidgetPeakHoldTest::peakDecayFast()
{
    // Fast = 20 dB/s; after 1000 ms decay: -50 - 20 = -70 dBm
    // From AetherSDR src/gui/SMeterWidget.cpp:677 [@0cd4559]
    std::unique_ptr<NereusSDR::SMeterWidget> w(
        makePeakWidget(NereusSDR::SMeterWidget::DecayRate::Fast));

    w->setLevel(-50.0f);                // captures peak at -50 dBm
    w->setLevel(-127.0f);               // drops level to noise floor; peak stays at -50 dBm
    w->testAdvanceTime(1000);           // 1 second of synthetic decay time
    const float peak = w->testPeakLevel();

    QVERIFY2(peak >= -71.0f && peak <= -69.0f,
             qPrintable(QString("Fast decay: expected -70 +/-1 dBm, got %1").arg(peak)));
}

void SMeterWidgetPeakHoldTest::peakDecayMedium()
{
    // Medium = 10 dB/s; after 1000 ms decay: -50 - 10 = -60 dBm
    // From AetherSDR src/gui/SMeterWidget.cpp:679 [@0cd4559]
    std::unique_ptr<NereusSDR::SMeterWidget> w(
        makePeakWidget(NereusSDR::SMeterWidget::DecayRate::Medium));

    w->setLevel(-50.0f);                // captures peak at -50 dBm
    w->setLevel(-127.0f);               // drops level to noise floor; peak stays at -50 dBm
    w->testAdvanceTime(1000);           // 1 second of synthetic decay time
    const float peak = w->testPeakLevel();

    QVERIFY2(peak >= -61.0f && peak <= -59.0f,
             qPrintable(QString("Medium decay: expected -60 +/-1 dBm, got %1").arg(peak)));
}

void SMeterWidgetPeakHoldTest::peakDecaySlow()
{
    // Slow = 5 dB/s; after 1000 ms decay: -50 - 5 = -55 dBm
    // From AetherSDR src/gui/SMeterWidget.cpp:681 [@0cd4559]
    std::unique_ptr<NereusSDR::SMeterWidget> w(
        makePeakWidget(NereusSDR::SMeterWidget::DecayRate::Slow));

    w->setLevel(-50.0f);                // captures peak at -50 dBm
    w->setLevel(-127.0f);               // drops level to noise floor; peak stays at -50 dBm
    w->testAdvanceTime(1000);           // 1 second of synthetic decay time
    const float peak = w->testPeakLevel();

    QVERIFY2(peak >= -56.0f && peak <= -54.0f,
             qPrintable(QString("Slow decay: expected -55 +/-1 dBm, got %1").arg(peak)));
}

QTEST_MAIN(SMeterWidgetPeakHoldTest)
#include "tst_smeter_widget_peak_hold.moc"
