// =================================================================
// tests/tst_power_swr_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for PowerSwrPresetItem — first-class Power + SWR dual-bar
// composite. Verifies the byte-for-byte Thetis MeterManager.cs
// AddPWRBar + AddSWRBar composition, binding (TxPower / TxSwr), and
// range mapping.
//
// no-port-check: test scaffolding; lineage is documented on the header
// of the implementation file. This test exercises the public NereusSDR
// API only and contains no transcribed Thetis source text — only
// references it by description.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/PowerSwrPresetItem.h"

using namespace NereusSDR;

class TestPowerSwrPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasTwoBars();
    void powerBar_boundToTxPower();
    void swrBar_boundToTxSwr();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestPowerSwrPresetItem::defaultConstruction_hasTwoBars()
{
    PowerSwrPresetItem item;
    QCOMPARE(item.barCount(), 2);
    QCOMPARE(item.barName(0), QStringLiteral("Power"));
    QCOMPARE(item.barName(1), QStringLiteral("SWR"));
}

void TestPowerSwrPresetItem::powerBar_boundToTxPower()
{
    PowerSwrPresetItem item;
    // Power bar: binding MeterBinding::TxPower (100), range 0..150W.
    QCOMPARE(item.barBindingId(0), 100 /* MeterBinding::TxPower */);
    QCOMPARE(item.barMinVal(0), 0.0);
    QCOMPARE(item.barMaxVal(0), 150.0);
}

void TestPowerSwrPresetItem::swrBar_boundToTxSwr()
{
    PowerSwrPresetItem item;
    // SWR bar: binding MeterBinding::TxSwr (102), range 1..10.
    QCOMPARE(item.barBindingId(1), 102 /* MeterBinding::TxSwr */);
    QCOMPARE(item.barMinVal(1), 1.0);
    QCOMPARE(item.barMaxVal(1), 10.0);
}

void TestPowerSwrPresetItem::serialize_roundTrip_preservesAllFields()
{
    PowerSwrPresetItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.4f);
    a.setShowReadout(false);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    PowerSwrPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),          a.x());
    QCOMPARE(b.y(),          a.y());
    QCOMPARE(b.itemWidth(),  a.itemWidth());
    QCOMPARE(b.itemHeight(), a.itemHeight());
    QCOMPARE(b.showReadout(), false);
}

void TestPowerSwrPresetItem::paintSmoke_rendersAtAspectRatio()
{
    PowerSwrPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 400;
    const int H = 140;  // typical PWR/SWR dual-bar rack
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

QTEST_MAIN(TestPowerSwrPresetItem)
#include "tst_power_swr_preset_item.moc"
