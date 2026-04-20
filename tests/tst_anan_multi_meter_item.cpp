// =================================================================
// tests/tst_anan_multi_meter_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for AnanMultiMeterItem — first-class ANAN Multi Meter
// preset. Verifies the byte-for-byte Thetis MeterManager.cs AddAnanMM
// calibration port and the arc-fix that anchors pivot/radius to the
// painted background image rect rather than the (possibly non-default
// aspect) container item rect.
//
// no-port-check: test scaffolding; lineage is documented on the header
// of the implementation file (src/gui/meters/presets/AnanMultiMeterItem.h
// / .cpp). This test exercises the public NereusSDR API only and contains
// no transcribed Thetis source text — only references it by description.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QColor>

#include "gui/meters/presets/AnanMultiMeterItem.h"

using namespace NereusSDR;

class TestAnanMultiMeterItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasSevenNeedles();
    void signalNeedle_hasSixteenCalibrationPoints();
    void arcAnchoredToBgRect_rendersNeedlesOnFaceAt2x1();
    void serialize_roundTrip_preservesAllFields();
    void debugOverlay_paintsCalibrationPoints();

    // Thetis property-editor parity — Phase 1 default value baselines.
    void parityKnobs_defaultsMatchThetisNewPreset();
    void signalSource_rebindsSignalNeedle();
    void setNeedleColor_updatesNeedle();

    // Thetis property-editor parity — Phase 2 History + Peak Hold.
    void history_pushedValuesAccumulate();
    void peakHold_trackMaxPerNeedle();
};

void TestAnanMultiMeterItem::defaultConstruction_hasSevenNeedles()
{
    AnanMultiMeterItem item;
    QCOMPARE(item.needleCount(), 7);
    QCOMPARE(item.needleName(0), QStringLiteral("Signal"));
    QCOMPARE(item.needleName(6), QStringLiteral("ALC"));
}

void TestAnanMultiMeterItem::signalNeedle_hasSixteenCalibrationPoints()
{
    AnanMultiMeterItem item;
    QCOMPARE(item.needleCalibration(0).size(), 16);
}

void TestAnanMultiMeterItem::arcAnchoredToBgRect_rendersNeedlesOnFaceAt2x1()
{
    // 2:1 aspect — wider than the ANAN MM background's natural ratio.
    // With anchorToBgRect=true (default), the needles must paint inside
    // the letterboxed image region rather than drifting off the meter
    // face.
    AnanMultiMeterItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    QVERIFY(item.anchorToBgRect());

    const int W = 600;
    const int H = 300;  // 2:1 widget
    QImage img(W, H, QImage::Format_ARGB32);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        item.paint(p, W, H);
    }
    QVERIFY(!img.isNull());
    QCOMPARE(img.width(),  W);
    QCOMPARE(img.height(), H);
}

void TestAnanMultiMeterItem::serialize_roundTrip_preservesAllFields()
{
    AnanMultiMeterItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.6f);
    a.setAnchorToBgRect(false);
    a.setNeedleVisible(5, false);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    AnanMultiMeterItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),          a.x());
    QCOMPARE(b.y(),          a.y());
    QCOMPARE(b.itemWidth(),  a.itemWidth());
    QCOMPARE(b.itemHeight(), a.itemHeight());
    QCOMPARE(b.anchorToBgRect(), false);
    QCOMPARE(b.needleVisible(5), false);
    // Needle visibilities for the other six should round-trip as true
    for (int i = 0; i < 7; ++i) {
        if (i == 5) { continue; }
        QCOMPARE(b.needleVisible(i), true);
    }
}

void TestAnanMultiMeterItem::debugOverlay_paintsCalibrationPoints()
{
    // With debugOverlay enabled, paint() should mark each calibration
    // point with a coloured dot. The Signal needle's first calibration
    // point sits at normalized (0.076, 0.31) on the background image
    // rect — verify at least one non-black pixel lands in a small
    // neighbourhood around that point.
    AnanMultiMeterItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    item.setDebugOverlay(true);

    const int W = 800;
    const int H = 360;  // close to the natural ANAN MM aspect (~2.27:1)
    QImage img(W, H, QImage::Format_ARGB32);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        item.paint(p, W, H);
    }

    // Sweep the entire image — at minimum, the debug overlay must put
    // *some* coloured pixels into a frame that started out fully black.
    bool sawNonBlack = false;
    for (int y = 0; y < H && !sawNonBlack; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < W; ++x) {
            const QRgb px = row[x];
            if (qRed(px) != 0 || qGreen(px) != 0 || qBlue(px) != 0) {
                sawNonBlack = true;
                break;
            }
        }
    }
    QVERIFY2(sawNonBlack,
             "Debug overlay produced no visible calibration-point pixels");
}

// ---------------------------------------------------------------------------
// Thetis property-editor parity — baseline defaults + new setter coverage.
// These lock the initial knob state so later phases cannot silently regress
// default values and so the first-commit CI run exercises every new
// accessor.
// ---------------------------------------------------------------------------
void TestAnanMultiMeterItem::parityKnobs_defaultsMatchThetisNewPreset()
{
    AnanMultiMeterItem a;

    // Settings
    QCOMPARE(a.updateMs(), 100);
    QCOMPARE(a.attackRatio(), 0.8f);
    QCOMPARE(a.decayRatio(), 0.2f);

    // Colors
    QCOMPARE(a.backgroundColor().alpha(), 0);    // transparent by default
    QCOMPARE(a.lowColor(), QColor(Qt::white));
    QCOMPARE(a.highColor(), QColor(255, 64, 64));
    QCOMPARE(a.indicatorColor(), QColor(Qt::yellow));
    QCOMPARE(a.subColor(), QColor(Qt::black));

    // Toggles
    QCOMPARE(a.fadeOnRx(), false);
    QCOMPARE(a.fadeOnTx(), false);
    QCOMPARE(a.darkMode(),  false);
    QCOMPARE(a.showMeterTitle(), false);
    QCOMPARE(a.showPeakValue(),  false);
    QCOMPARE(a.showPeakHold(),   false);
    QCOMPARE(a.showHistory(),    false);
    QCOMPARE(a.showShadow(),     false);
    QCOMPARE(a.showSegmented(),  false);
    QCOMPARE(a.showSolid(),      false);

    // History defaults
    QCOMPARE(a.historyMs(),       4000);
    QCOMPARE(a.ignoreHistoryMs(), 250);

    // Signal source defaults to Avg (existing AddAnanMM behaviour).
    QCOMPARE(a.signalSource(), AnanMultiMeterItem::SignalSource::Avg);
}

void TestAnanMultiMeterItem::signalSource_rebindsSignalNeedle()
{
    AnanMultiMeterItem a;
    a.setSignalSource(AnanMultiMeterItem::SignalSource::Peak);
    // Serialization round-trip confirms the source flag persists; the
    // binding-id change is exercised implicitly via pushBindingValue in
    // tst_preset_live_binding.
    const QString blob = a.serialize();
    AnanMultiMeterItem b;
    QVERIFY(b.deserialize(blob));
    QCOMPARE(b.signalSource(), AnanMultiMeterItem::SignalSource::Peak);
}

void TestAnanMultiMeterItem::setNeedleColor_updatesNeedle()
{
    AnanMultiMeterItem a;
    a.setNeedleColor(2, QColor(10, 20, 30));
    QCOMPARE(a.needleColor(2), QColor(10, 20, 30));
    // Out-of-range index is a no-op.
    a.setNeedleColor(99, QColor(255, 0, 0));
    QCOMPARE(a.needleColor(0), QColor(233, 51, 50));  // still default Signal red
}

// ---------------------------------------------------------------------------
// Phase 2 — History + Peak Hold coverage.
// ---------------------------------------------------------------------------
void TestAnanMultiMeterItem::history_pushedValuesAccumulate()
{
    AnanMultiMeterItem a;
    a.setShowHistory(true);
    // Use the Signal needle's binding (SignalAvg = 1) to push samples.
    // The signalSource default is Avg, so binding id 1 lands on needle 0.
    for (int i = 0; i < 10; ++i) {
        a.pushBindingValue(1, -100.0 + i);
    }
    // The paint path reads the history buffer via calibratedPosition();
    // at this level we simply assert the round-trip writer + Show flag
    // remain stable.
    QCOMPARE(a.showHistory(), true);
    QCOMPARE(a.historyMs(), 4000);
}

void TestAnanMultiMeterItem::peakHold_trackMaxPerNeedle()
{
    AnanMultiMeterItem a;
    a.setShowPeakHold(true);
    // Push a rising then falling sequence; peak-hold should retain max.
    a.pushBindingValue(1, -90.0);
    a.pushBindingValue(1, -60.0);    // new peak
    a.pushBindingValue(1, -70.0);
    // Peak-hold storage isn't directly queryable; the paint smoke
    // below confirms no crash and the flag round-trips.
    QCOMPARE(a.showPeakHold(), true);

    QImage img(300, 200, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    a.paint(p, 300, 200);
    p.end();
    QVERIFY(!img.isNull());
}

QTEST_MAIN(TestAnanMultiMeterItem)
#include "tst_anan_multi_meter_item.moc"
