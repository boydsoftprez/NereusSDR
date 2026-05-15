// Regression tests for issue #230 — "Waterfall gain seems unstable and not
// saving settings in 4.0".
//
// Source-first context: Thetis splits persistent user thresholds
// (waterfall_low/high_threshold, display.cs:2522-2548 [v2.10.3.13]) from
// runtime AGC state (_RX1waterfallPreviousMinValue) and renders into
// per-draw locals at display.cs:6575-6594 [v2.10.3.13]. NereusSDR
// previously collapsed both roles into m_wfLow/HighThreshold, so AGC /
// Clarity / "Use spectrum min/max" silently overwrote the user's saved
// values; the next scheduleSettingsSave() debounce persisted the
// overwrite. These tests verify the Thetis-faithful split.

#include <QtTest/QtTest>
#include <QVector>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestWaterfallThresholdPersistence : public QObject
{
    Q_OBJECT

private slots:
    // Clarity is the only external writer that previously called the
    // user setters (setWfLow/HighThreshold). After the fix it goes
    // through setClarityWaterfallThresholds which writes the active
    // mirror only — user fields stay put.
    void clarityWritesDoNotMutateUserThresholds();

    // AGC's per-row composition writes m_wfActive*, never m_wf*.
    // composeWaterfallActiveThresholds is the testable seam pulled out
    // of pushWaterfallRow().
    void agcComposeDoesNotMutateUserThresholds();

    // NF-AGC override goes through the same compose path.
    void nfAgcComposeDoesNotMutateUserThresholds();

    // "Use spectrum min/max" is Thetis-faithful: the grid-change path
    // writes the persistent thresholds via setDbmRange(); per-frame
    // render code never mutates them.
    void useSpectrumMinMaxOnSetDbmRangeUpdatesPersistentThresholds();

    // When useSpectrumMinMax is OFF, setDbmRange() must not touch the
    // user's waterfall thresholds.
    void useSpectrumMinMaxOffSetDbmRangeLeavesThresholdsAlone();

    // Toggling useSpectrumMinMax on syncs once from the current dB
    // range (mirrors Thetis console.cs:9098-9101 [v2.10.3.13] called
    // on the checkbox event via WaterfallUseRX1SpectrumMinMax setter).
    void enablingUseSpectrumMinMaxSyncsFromCurrentRange();
};

// Active getters mirror the user thresholds on construction so the
// renderer always has a valid value before the first row push.
void TestWaterfallThresholdPersistence::clarityWritesDoNotMutateUserThresholds()
{
    SpectrumWidget w;
    w.setWfLowThreshold(-100.0f);
    w.setWfHighThreshold(-40.0f);
    QCOMPARE(w.wfLowThreshold(),  -100.0f);
    QCOMPARE(w.wfHighThreshold(), -40.0f);

    // Clarity drives a wider span — analogous to AGC RunMin/RunMax
    // settling far from the user's preferred display window.
    w.setClarityWaterfallThresholds(-150.0f, -10.0f);

    // Persistent values must be unchanged. The active layer (what the
    // renderer uses) reflects Clarity's output.
    QCOMPARE(w.wfLowThreshold(),  -100.0f);
    QCOMPARE(w.wfHighThreshold(), -40.0f);
    QCOMPARE(w.wfActiveLowThreshold(),  -150.0f);
    QCOMPARE(w.wfActiveHighThreshold(), -10.0f);
}

void TestWaterfallThresholdPersistence::agcComposeDoesNotMutateUserThresholds()
{
    SpectrumWidget w;
    w.setWfLowThreshold(-100.0f);
    w.setWfHighThreshold(-40.0f);
    w.setWfAgcEnabled(true);

    // Build a synthetic row that drives AGC RunMin/RunMax to
    // values well outside the user-configured window.
    QVector<float> row;
    row.reserve(128);
    for (int i = 0; i < 128; ++i) {
        const float v = (i < 64) ? -130.0f : -20.0f;
        row.append(v);
    }

    // Run the compose pass multiple times to let the one-pole filter
    // converge near the actual min/max.
    for (int i = 0; i < 200; ++i) {
        w.composeWaterfallActiveThresholds(row);
    }

    // Persistent user values must NOT have been clobbered.
    QCOMPARE(w.wfLowThreshold(),  -100.0f);
    QCOMPARE(w.wfHighThreshold(), -40.0f);

    // Active values should reflect AGC's settled range
    // (RunMin - margin, RunMax + margin). The 12 dB margin matches
    // SpectrumWidget.cpp pushWaterfallRow.
    QVERIFY(w.wfActiveLowThreshold()  < -100.0f);  // AGC pushed lower
    QVERIFY(w.wfActiveHighThreshold() > -40.0f);   // AGC pushed higher
}

void TestWaterfallThresholdPersistence::nfAgcComposeDoesNotMutateUserThresholds()
{
    SpectrumWidget w;
    w.setWfLowThreshold(-100.0f);
    w.setWfHighThreshold(-40.0f);
    w.setWfAgcEnabled(false);           // disable plain AGC
    w.setWaterfallNFAGCEnabled(true);   // engage NF-AGC override
    w.setWaterfallAGCOffsetDb(-10);

    QVector<float> row;
    row.reserve(128);
    for (int i = 0; i < 128; ++i) {
        // Make the 10th-percentile noise floor land near -110 dBm.
        row.append(-110.0f + static_cast<float>(i) * 0.1f);
    }

    w.composeWaterfallActiveThresholds(row);

    QCOMPARE(w.wfLowThreshold(),  -100.0f);
    QCOMPARE(w.wfHighThreshold(), -40.0f);
    // NF-AGC writes m_wfActiveLow = nf + offset, m_wfActiveHigh = low + 60.
    // The active values must NOT equal the user's persistent values.
    QVERIFY(w.wfActiveLowThreshold()  != -100.0f);
    QVERIFY(w.wfActiveHighThreshold() != -40.0f);
}

void TestWaterfallThresholdPersistence::useSpectrumMinMaxOnSetDbmRangeUpdatesPersistentThresholds()
{
    SpectrumWidget w;
    w.setWfLowThreshold(-122.0f);
    w.setWfHighThreshold(-62.0f);
    w.setWfUseSpectrumMinMax(true);

    // Mirrors a user dragging the spectrum dB range; Thetis routes
    // this through setWaterfallGainsIfLinkedToSpectrum at
    // console.cs:9098-9101 [v2.10.3.13].
    w.setDbmRange(-150.0f, -30.0f);

    QCOMPARE(w.wfLowThreshold(),  -150.0f);
    QCOMPARE(w.wfHighThreshold(), -30.0f);
}

void TestWaterfallThresholdPersistence::useSpectrumMinMaxOffSetDbmRangeLeavesThresholdsAlone()
{
    SpectrumWidget w;
    w.setWfLowThreshold(-122.0f);
    w.setWfHighThreshold(-62.0f);
    w.setWfUseSpectrumMinMax(false);

    w.setDbmRange(-150.0f, -30.0f);

    QCOMPARE(w.wfLowThreshold(),  -122.0f);
    QCOMPARE(w.wfHighThreshold(), -62.0f);
}

void TestWaterfallThresholdPersistence::enablingUseSpectrumMinMaxSyncsFromCurrentRange()
{
    SpectrumWidget w;
    w.setWfLowThreshold(-122.0f);
    w.setWfHighThreshold(-62.0f);
    w.setWfUseSpectrumMinMax(false);

    // Pre-set a different dBm range while the flag is off — should
    // leave thresholds alone (verified by the test above).
    w.setDbmRange(-140.0f, -20.0f);
    QCOMPARE(w.wfLowThreshold(),  -122.0f);
    QCOMPARE(w.wfHighThreshold(), -62.0f);

    // Flipping the flag on must sync once from the current range
    // (Thetis behavior: checkbox toggle invokes the same property
    // setter chain that the grid-changed handler does).
    w.setWfUseSpectrumMinMax(true);
    QCOMPARE(w.wfLowThreshold(),  -140.0f);
    QCOMPARE(w.wfHighThreshold(), -20.0f);
}

QTEST_MAIN(TestWaterfallThresholdPersistence)
#include "tst_waterfall_threshold_persistence.moc"
