// no-port-check: unit tests for dBm-strip geometry and adaptive step helpers
#include <QtTest/QtTest>
#include <QRect>

#include "gui/dbm_strip_math.h"

using namespace NereusSDR::DbmStrip;

class TestSpectrumDbmStrip : public QObject
{
    Q_OBJECT

private slots:
    void stripRect_occupiesRightEdge();
    void arrowRowRect_atTopOfStrip();
    void arrowHit_leftHalfReturnsZero();
    void arrowHit_rightHalfReturnsOne();
    void arrowHit_outsideReturnsNegativeOne();
    void adaptiveStepDb_selectsByRange_data();
    void adaptiveStepDb_selectsByRange();
    void dssRoundedLabelSet_emptyWhenRangeNonPositive();
    void dssRoundedLabelSet_matchesKnownValues();
    void dssRoundedLabelSet_subStepShiftLeavesSetUnchanged();
    void dssRoundedLabelSet_fullStepShiftChangesSet();
};

void TestSpectrumDbmStrip::stripRect_occupiesRightEdge()
{
    const QRect spec(0, 0, 800, 400);
    const QRect strip = stripRect(spec, 36);
    QCOMPARE(strip.left(), 764);  // spec.right() - stripW + 1 = 799 - 36 + 1
    QCOMPARE(strip.top(),  0);
    QCOMPARE(strip.width(), 36);
    QCOMPARE(strip.height(), 400);
}

void TestSpectrumDbmStrip::arrowRowRect_atTopOfStrip()
{
    const QRect strip(764, 10, 36, 400);
    const QRect arrows = arrowRowRect(strip, 14);
    QCOMPARE(arrows.left(), 764);
    QCOMPARE(arrows.top(),  10);
    QCOMPARE(arrows.width(), 36);
    QCOMPARE(arrows.height(), 14);
}

void TestSpectrumDbmStrip::arrowHit_leftHalfReturnsZero()
{
    const QRect arrows(100, 0, 36, 14);
    QCOMPARE(arrowHit(100, arrows), 0);  // leftmost
    QCOMPARE(arrowHit(117, arrows), 0);  // just left of midpoint (100 + 18)
}

void TestSpectrumDbmStrip::arrowHit_rightHalfReturnsOne()
{
    const QRect arrows(100, 0, 36, 14);
    QCOMPARE(arrowHit(118, arrows), 1);  // at midpoint
    QCOMPARE(arrowHit(135, arrows), 1);  // rightmost
}

void TestSpectrumDbmStrip::arrowHit_outsideReturnsNegativeOne()
{
    const QRect arrows(100, 0, 36, 14);
    QCOMPARE(arrowHit(99,  arrows), -1);
    QCOMPARE(arrowHit(136, arrows), -1);
    QCOMPARE(arrowHit(-5,  arrows), -1);
}

void TestSpectrumDbmStrip::adaptiveStepDb_selectsByRange_data()
{
    QTest::addColumn<float>("dynamicRange");
    QTest::addColumn<float>("expectedStep");

    // rawStep = dynamicRange / 5
    QTest::newRow("120dB -> 20 step") << 120.0f << 20.0f;  // raw 24
    QTest::newRow("100dB -> 20 step") << 100.0f << 20.0f;  // raw 20
    QTest::newRow("99dB  -> 10 step") <<  99.0f << 10.0f;  // raw 19.8
    QTest::newRow("50dB  -> 10 step") <<  50.0f << 10.0f;  // raw 10
    QTest::newRow("49dB  ->  5 step") <<  49.0f <<  5.0f;  // raw 9.8
    QTest::newRow("25dB  ->  5 step") <<  25.0f <<  5.0f;  // raw 5
    QTest::newRow("24dB  ->  2 step") <<  24.0f <<  2.0f;  // raw 4.8
    QTest::newRow("10dB  ->  2 step") <<  10.0f <<  2.0f;  // raw 2
}

void TestSpectrumDbmStrip::adaptiveStepDb_selectsByRange()
{
    QFETCH(float, dynamicRange);
    QFETCH(float, expectedStep);
    QCOMPARE(adaptiveStepDb(dynamicRange), expectedStep);
}

// I2 fix (final review of the 3D stacked-trace spectrum plan): unit tests
// for the pure label-set generator that lets SpectrumWidget's GPU-path 3D
// dBm-scale overlay-cache staleness check ask "would the operator see
// different text" without a QPainter. See tst_dss_dbm_scale.cpp for the
// SpectrumWidget-level integration test that binds this to the actual
// overlay-dirty behaviour.
void TestSpectrumDbmStrip::dssRoundedLabelSet_emptyWhenRangeNonPositive()
{
    QCOMPARE(dssRoundedLabelSet(-40.0f, 0.0f),  QVector<int>());
    QCOMPARE(dssRoundedLabelSet(-40.0f, -10.0f), QVector<int>());
}

void TestSpectrumDbmStrip::dssRoundedLabelSet_matchesKnownValues()
{
    // rangeDb=100 -> adaptiveStepDb=20 (see adaptiveStepDb_selectsByRange
    // above). topDbm=-30 -> bottomDbm=-130, a 20 dB grid line, so
    // firstLabel is picked deliberately 10 dB clear of any boundary in
    // either direction (unlike e.g. topDbm=-40/bottomDbm=-140, which sits
    // exactly ON a grid line and is far more sensitive to float noise --
    // ceil() snaps to a different multiple for even a 0.01 dB nudge across
    // an exact boundary).
    const QVector<int> labels = dssRoundedLabelSet(-30.0f, 100.0f);
    const QVector<int> expected{-120, -100, -80, -60, -40, -130};
    QCOMPARE(labels, expected);
}

void TestSpectrumDbmStrip::dssRoundedLabelSet_subStepShiftLeavesSetUnchanged()
{
    // A sub-tenth-dB shift, safely clear of the nearest 20 dB grid line
    // (10 dB away at topDbm=-30), must not move a single rounded label.
    // Precondition tst_dss_dbm_scale.cpp's
    // overlayFreshness_ignoresSubLabelFloorDrift relies on.
    QCOMPARE(dssRoundedLabelSet(-30.05f, 100.0f),
             dssRoundedLabelSet(-30.0f, 100.0f));
}

void TestSpectrumDbmStrip::dssRoundedLabelSet_fullStepShiftChangesSet()
{
    // A full 20 dB shift (one adaptive step at this range) must change
    // the set. Precondition
    // overlayFreshness_dirtiesOnLabelChangingFloorShift relies on.
    QVERIFY(dssRoundedLabelSet(-10.0f, 100.0f)
            != dssRoundedLabelSet(-30.0f, 100.0f));
}

QTEST_APPLESS_MAIN(TestSpectrumDbmStrip)
#include "tst_spectrum_dbm_strip.moc"
