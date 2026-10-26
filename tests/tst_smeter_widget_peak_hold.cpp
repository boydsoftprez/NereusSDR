// tests/tst_smeter_widget_peak_hold.cpp
//
// no-port-check: NereusSDR test; its Thetis cites name the behaviour it
// checks (SMeterWidget carries the port and its header).
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
//
// 2026-10-26: a container copy's dynamics and appearance: Thetis attack and
// decay ratios (clsNeedleItem.Update), the update interval, the history
// (sized by its duration, emptied by clearHistory with its ignore time),
// the peak hold's clear, the receive and transmit needle colours and dark
// mode.

#include <QtTest>
#include "gui/SMeterWidget.h"
#include "gui/VintageMeterFace.h"
#include "gui/meters/MeterItem.h"

using NereusSDR::SMeterWidget;

class SMeterWidgetPeakHoldTest : public QObject {
    Q_OBJECT
private slots:
    void peakDecayFast();
    void peakDecayMedium();
    void peakDecaySlow();

    void ratiosMoveTheNeedleThatShareEachUpdate();
    void updateIntervalIsClamped();
    void historySpansTheNeedleOverItsDuration();
    void clearHistoryResetsThePeakAndIgnoresReadings();
    void mainPanelDialNeverIgnores();
    void needleColoursFollowReceiveAndTransmit();
    void darkModeGivesALightFaceTheBlackfaceCard();
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


// From Thetis MeterManager.cs:22013-22016 [v2.10.3.15]: each update moves the
// value by the ratio's share of the way to the reading; without a ratio the
// built-in time constants apply.
void SMeterWidgetPeakHoldTest::ratiosMoveTheNeedleThatShareEachUpdate()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    QCOMPARE(w.testNeedleFraction(), 0.0f);
    w.setAttackRatio(0.5f);
    w.setDecayRatio(0.25f);
    w.setLevel(-73.0f);                 // S9: fraction 0.6
    QCOMPARE(w.testNeedleTarget(), 0.6f);
    w.testStepNeedle(100);
    QVERIFY(qFuzzyCompare(w.testNeedleFraction(), 0.3f));
    w.testStepNeedle(100);
    QVERIFY(qFuzzyCompare(w.testNeedleFraction(), 0.45f));
    w.setLevel(-127.0f);                // S0: fraction 0
    w.testStepNeedle(100);
    QVERIFY(qFuzzyCompare(w.testNeedleFraction(), 0.3375f));

    // Out of range clamps to 0..1; empty is the built-in attack (45 ms).
    w.setAttackRatio(4.0f);
    QCOMPARE(*w.attackRatio(), 1.0f);
    w.setAttackRatio(std::nullopt);
    QVERIFY(!w.attackRatio());
    SMeterWidget built(SMeterWidget::SettingsStore::Instance);
    built.setLevel(-73.0f);
    built.testStepNeedle(45);
    const float expected = 0.6f * (1.0f - std::exp(-1.0f));
    QVERIFY2(std::abs(built.testNeedleFraction() - expected) < 1e-4f,
             qPrintable(QString::number(built.testNeedleFraction())));
}

void SMeterWidgetPeakHoldTest::updateIntervalIsClamped()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    QCOMPARE(w.updateIntervalMs(), SMeterWidget::kDefaultUpdateIntervalMs);
    QCOMPARE(SMeterWidget::kDefaultUpdateIntervalMs, 33);
    w.setUpdateIntervalMs(5);
    QCOMPARE(w.updateIntervalMs(), NereusSDR::kMinUpdateIntervalMs);
    w.setUpdateIntervalMs(99999);
    QCOMPARE(w.updateIntervalMs(), NereusSDR::kMaxUpdateIntervalMs);
    w.setUpdateIntervalMs(250);
    QCOMPARE(w.updateIntervalMs(), 250);
}

// From Thetis MeterManager.cs:22018-22029 [v2.10.3.15]: one value per update,
// duration / interval values kept, the oldest dropped.
void SMeterWidgetPeakHoldTest::historySpansTheNeedleOverItsDuration()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    QVERIFY(!w.showHistory());
    QCOMPARE(w.historyDurationMs(), 4000);
    QCOMPARE(w.historyColor(), QColor::fromRgba(qRgba(233, 51, 50, 64)));
    QCOMPARE(w.ignoreHistoryMs(), 2000);
    w.setUpdateIntervalMs(100);
    w.setHistoryDurationMs(300);        // three values
    w.setShowHistory(true);
    w.setAttackRatio(1.0f);
    w.setDecayRatio(1.0f);
    // Empty: the needle itself.
    QCOMPARE(w.testHistoryRange(), std::make_pair(0.0f, 0.0f));

    w.setLevel(-73.0f);
    w.testStepNeedle(100);
    QCOMPARE(w.testNeedleFraction(), 0.6f);
    w.testSampleHistory();
    w.setLevel(-100.0f);
    w.testStepNeedle(100);
    const float low = w.testNeedleFraction();
    QVERIFY(low < 0.6f);
    w.testSampleHistory();
    QCOMPARE(w.testHistoryRange(), std::make_pair(low, 0.6f));
    w.testSampleHistory();
    w.testSampleHistory();
    QCOMPARE(w.testHistorySize(), 3);
    // The S9 value has left the three kept.
    QCOMPARE(w.testHistoryRange(), std::make_pair(low, low));

    // A duration under the interval keeps one value (HistoryDuration's floor).
    w.setHistoryDurationMs(50);
    w.testSampleHistory();
    QCOMPARE(w.testHistorySize(), 1);
    // Hiding the history empties it.
    w.setShowHistory(false);
    QCOMPARE(w.testHistorySize(), 0);
}

// From Thetis MeterManager.cs:22131-22139 [v2.10.3.15]: ClearHistory empties
// the history and ignores the next IgnoreHistoryDuration of readings; the
// peak hold (the history's maximum in Thetis) clears with it.
void SMeterWidgetPeakHoldTest::clearHistoryResetsThePeakAndIgnoresReadings()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    w.testSetPeakHoldTimeMs(1000);
    w.setShowHistory(true);
    w.setLevel(-50.0f);
    w.setLevel(-90.0f);
    QCOMPARE(w.testPeakLevel(), -50.0f);
    w.testSampleHistory();
    QCOMPARE(w.testHistorySize(), 1);

    w.clearHistory();
    QCOMPARE(w.testHistorySize(), 0);
    QCOMPARE(w.testPeakLevel(), -90.0f);
    QVERIFY(w.testIgnoringHistory());
    // Within the ignore time a reading reaches neither the history nor the
    // peak hold, which rides on the reading.
    w.setLevel(-60.0f);
    QCOMPARE(w.testPeakLevel(), -60.0f);
    w.setLevel(-80.0f);
    QCOMPARE(w.testPeakLevel(), -80.0f);
    w.testSampleHistory();
    QCOMPARE(w.testHistorySize(), 0);

    w.testAdvanceTime(1500);
    QVERIFY(w.testIgnoringHistory());
    w.testAdvanceTime(500);
    QVERIFY(!w.testIgnoringHistory());
    // After it, a reading raises the peak hold again.
    w.setLevel(-50.0f);
    QCOMPARE(w.testPeakLevel(), -50.0f);
    w.testSampleHistory();
    QCOMPARE(w.testHistorySize(), 1);

    // A shorter ignore time of the copy's own.
    w.setIgnoreHistoryMs(0);
    w.clearHistory();
    QVERIFY(!w.testIgnoringHistory());
    w.setIgnoreHistoryMs(99999);
    QCOMPARE(w.ignoreHistoryMs(), NereusSDR::kMaxBarIgnoreHistoryMs);
}

// The Main Panel's dial has no clear and so never ignores a reading.
void SMeterWidgetPeakHoldTest::mainPanelDialNeverIgnores()
{
    std::unique_ptr<SMeterWidget> w(makePeakWidget(SMeterWidget::DecayRate::Medium));
    QVERIFY(!w->testIgnoringHistory());
    w->setLevel(-50.0f);
    QCOMPARE(w->testPeakLevel(), -50.0f);
}

// The receive needle is Thetis's primary needle (MarkerColour), the
// transmit needle its sub needles (SubMarkerColour); with none of the
// copy's own, the face's colour.
void SMeterWidgetPeakHoldTest::needleColoursFollowReceiveAndTransmit()
{
    SMeterWidget w(SMeterWidget::SettingsStore::Instance);
    w.setFaceStyle(SMeterWidget::FaceStyle::Classic);
    QCOMPARE(w.testNeedleColorShown(), QColor(0xff, 0xff, 0xff));
    w.setNeedleColor(QColor(0x00, 0xff, 0x00));
    w.setSubNeedleColor(QColor(0xff, 0x00, 0xff));
    QCOMPARE(w.testNeedleColorShown(), QColor(0x00, 0xff, 0x00));
    w.setTransmitting(true);
    QCOMPARE(w.testNeedleColorShown(), QColor(0xff, 0x00, 0xff));
    w.setSubNeedleColor(QColor());
    QCOMPARE(w.testNeedleColorShown(), QColor(0xff, 0xff, 0xff));
    w.setFaceStyle(SMeterWidget::FaceStyle::VuAmber);
    QCOMPARE(w.testNeedleColorShown(),
             QColor(NereusSDR::VintageMeterFace::theme(static_cast<int>(SMeterWidget::FaceStyle::VuAmber)).pointer));
}

void SMeterWidgetPeakHoldTest::darkModeGivesALightFaceTheBlackfaceCard()
{
    using NereusSDR::VintageMeterFace;
    const QColor blackfacePointer(VintageMeterFace::theme(static_cast<int>(SMeterWidget::FaceStyle::Blackface)).pointer);
    const QColor blackfaceInk(VintageMeterFace::theme(static_cast<int>(SMeterWidget::FaceStyle::Blackface)).ink);
    const QColor creamPointer(VintageMeterFace::theme(static_cast<int>(SMeterWidget::FaceStyle::AgedCream)).pointer);
    const QColor carbonPointer(VintageMeterFace::theme(static_cast<int>(SMeterWidget::FaceStyle::Carbon)).pointer);
    QCOMPARE(SMeterWidget::faceNeedleColor(SMeterWidget::FaceStyle::AgedCream, false), creamPointer);
    QCOMPARE(SMeterWidget::faceNeedleColor(SMeterWidget::FaceStyle::AgedCream, true), blackfacePointer);
    QCOMPARE(SMeterWidget::facePowerScaleColor(SMeterWidget::FaceStyle::AgedCream, true), blackfaceInk);
    // A dark face and the Classic face are already dark.
    QCOMPARE(SMeterWidget::faceNeedleColor(SMeterWidget::FaceStyle::Carbon, true), carbonPointer);
    QCOMPARE(SMeterWidget::faceNeedleColor(SMeterWidget::FaceStyle::Classic, true), QColor(0xff, 0xff, 0xff));
    QCOMPARE(SMeterWidget::facePowerScaleColor(SMeterWidget::FaceStyle::Classic, true), QColor(0x00, 0x80, 0xd0));

    // The dark card is drawn.
    SMeterWidget plain(SMeterWidget::SettingsStore::Instance);
    SMeterWidget dark(SMeterWidget::SettingsStore::Instance);
    for (SMeterWidget* w : {&plain, &dark}) {
        w->setFaceStyle(SMeterWidget::FaceStyle::AgedCream);
        w->resize(298, 140);
    }
    dark.setDarkMode(true);
    QVERIFY(dark.darkMode());
    QVERIFY(plain.grab().toImage() != dark.grab().toImage());
}

QTEST_MAIN(SMeterWidgetPeakHoldTest)
#include "tst_smeter_widget_peak_hold.moc"
