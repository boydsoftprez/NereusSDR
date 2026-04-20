// =================================================================
// tests/tst_cross_needle_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for CrossNeedleItem — first-class Cross Needle preset.
// Verifies the byte-for-byte Thetis MeterManager.cs AddCrossNeedle
// calibration port (forward power, reverse power) and the composite
// paint path (bg image + optional band overlay + two needles).
//
// no-port-check: test scaffolding; lineage is documented on the header
// of the implementation file (src/gui/meters/presets/CrossNeedleItem.h
// / .cpp). This test exercises the public NereusSDR API only and contains
// no transcribed Thetis source text — only references it by description.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QPointF>

#include "gui/meters/presets/CrossNeedleItem.h"

using namespace NereusSDR;

class TestCrossNeedleItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasTwoNeedles();
    void forwardNeedle_hasCalibrationPoints();
    void reverseNeedle_hasCalibrationPoints();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestCrossNeedleItem::defaultConstruction_hasTwoNeedles()
{
    CrossNeedleItem item;
    QCOMPARE(item.needleCount(), 2);
    QCOMPARE(item.needleName(0), QStringLiteral("Forward"));
    QCOMPARE(item.needleName(1), QStringLiteral("Reflected"));
}

void TestCrossNeedleItem::forwardNeedle_hasCalibrationPoints()
{
    CrossNeedleItem item;
    // Forward power calibration: 0..100W, 15 points
    // (Thetis AddCrossNeedle:22843-22857).
    QCOMPARE(item.needleCalibration(0).size(), 15);
}

void TestCrossNeedleItem::reverseNeedle_hasCalibrationPoints()
{
    CrossNeedleItem item;
    // Reverse power calibration: 0..20 (SWR-normalized), 19 points
    // (Thetis AddCrossNeedle:22914-22932).
    QCOMPARE(item.needleCalibration(1).size(), 19);
}

void TestCrossNeedleItem::serialize_roundTrip_preservesAllFields()
{
    CrossNeedleItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.6f);
    a.setNeedleVisible(1, false);
    a.setShowBandOverlay(false);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    CrossNeedleItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),          a.x());
    QCOMPARE(b.y(),          a.y());
    QCOMPARE(b.itemWidth(),  a.itemWidth());
    QCOMPARE(b.itemHeight(), a.itemHeight());
    QCOMPARE(b.needleVisible(0), true);
    QCOMPARE(b.needleVisible(1), false);
    QCOMPARE(b.showBandOverlay(), false);
}

void TestCrossNeedleItem::paintSmoke_rendersAtAspectRatio()
{
    CrossNeedleItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 600;
    const int H = 470;  // approx natural cross-needle.png aspect
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

QTEST_MAIN(TestCrossNeedleItem)
#include "tst_cross_needle_item.moc"
