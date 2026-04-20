// =================================================================
// tests/tst_history_graph_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for HistoryGraphPresetItem — NereusSDR-local rolling
// history strip preset. Verifies the composite (graph + scale ticks
// + readout), serialisation round-trip, and paint smoke.
//
// no-port-check: NereusSDR-local scaffolding; no Thetis source is
// referenced. The implementation file is a collapse of
// src/gui/meters/ItemGroup.cpp's createHistoryPreset helper.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/HistoryGraphPresetItem.h"

using namespace NereusSDR;

class TestHistoryGraphPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasGraphAndScale();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestHistoryGraphPresetItem::defaultConstruction_hasGraphAndScale()
{
    HistoryGraphPresetItem item;
    QVERIFY(item.hasGraph());
    QVERIFY(item.hasScale());
    QVERIFY(item.hasReadout());
    // Default binding = SignalAvg (matches createHistoryPreset
    // default caller path: bindingId is passed by caller, default
    // MeterBinding::SignalAvg).
    QCOMPARE(item.bindingId(), 1 /* MeterBinding::SignalAvg */);
    QVERIFY(item.capacity() > 0);
}

void TestHistoryGraphPresetItem::serialize_roundTrip_preservesAllFields()
{
    HistoryGraphPresetItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.3f);
    a.setBindingId(0 /* SignalPeak */);
    a.setCapacity(200);
    a.setShowReadout(false);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    HistoryGraphPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),            a.x());
    QCOMPARE(b.y(),            a.y());
    QCOMPARE(b.itemWidth(),    a.itemWidth());
    QCOMPARE(b.itemHeight(),   a.itemHeight());
    QCOMPARE(b.bindingId(),    0);
    QCOMPARE(b.capacity(),     200);
    QCOMPARE(b.showReadout(),  false);
}

void TestHistoryGraphPresetItem::paintSmoke_rendersAtAspectRatio()
{
    HistoryGraphPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 400;
    const int H = 120;
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

QTEST_MAIN(TestHistoryGraphPresetItem)
#include "tst_history_graph_preset_item.moc"
