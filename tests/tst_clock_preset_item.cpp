// =================================================================
// tests/tst_clock_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for ClockPresetItem — NereusSDR-local narrow clock
// strip preset. Verifies the composite (backdrop + time readout),
// serialisation round-trip, and paint smoke.
//
// no-port-check: NereusSDR-local scaffolding; no Thetis source is
// referenced. The implementation file is a collapse of
// src/gui/meters/ItemGroup.cpp's createClockPreset helper.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/ClockPresetItem.h"

using namespace NereusSDR;

class TestClockPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasClock();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestClockPresetItem::defaultConstruction_hasClock()
{
    ClockPresetItem item;
    QVERIFY(item.hasClock());
    QVERIFY(item.utc());
}

void TestClockPresetItem::serialize_roundTrip_preservesAllFields()
{
    ClockPresetItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.1f);
    a.setUtc(false);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    ClockPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),          a.x());
    QCOMPARE(b.y(),          a.y());
    QCOMPARE(b.itemWidth(),  a.itemWidth());
    QCOMPARE(b.itemHeight(), a.itemHeight());
    QCOMPARE(b.utc(),        false);
}

void TestClockPresetItem::paintSmoke_rendersAtAspectRatio()
{
    ClockPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 400;
    const int H = 40;  // narrow clock strip
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

QTEST_MAIN(TestClockPresetItem)
#include "tst_clock_preset_item.moc"
