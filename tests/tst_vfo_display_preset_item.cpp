// =================================================================
// tests/tst_vfo_display_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for VfoDisplayPresetItem — NereusSDR-local VFO display
// preset. Verifies the composite (background + 7-segment frequency
// digits + band/mode labels), serialisation round-trip, and paint.
//
// no-port-check: NereusSDR-local scaffolding; no Thetis source is
// referenced. The implementation file is a collapse of
// src/gui/meters/ItemGroup.cpp's createVfoDisplayPreset helper.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/VfoDisplayPresetItem.h"

using namespace NereusSDR;

class TestVfoDisplayPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasDisplayAndLabels();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestVfoDisplayPresetItem::defaultConstruction_hasDisplayAndLabels()
{
    VfoDisplayPresetItem item;
    QVERIFY(item.hasFrequencyDigits());
    QVERIFY(item.hasBandLabel());
    QVERIFY(item.hasModeLabel());
}

void TestVfoDisplayPresetItem::serialize_roundTrip_preservesAllFields()
{
    VfoDisplayPresetItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.3f);
    a.setFrequencyHz(14250000.0);
    a.setBandLabel(QStringLiteral("20m"));
    a.setModeLabel(QStringLiteral("USB"));

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    VfoDisplayPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),            a.x());
    QCOMPARE(b.y(),            a.y());
    QCOMPARE(b.itemWidth(),    a.itemWidth());
    QCOMPARE(b.itemHeight(),   a.itemHeight());
    QCOMPARE(b.frequencyHz(),  14250000.0);
    QCOMPARE(b.bandLabel(),    QStringLiteral("20m"));
    QCOMPARE(b.modeLabel(),    QStringLiteral("USB"));
}

void TestVfoDisplayPresetItem::paintSmoke_rendersAtAspectRatio()
{
    VfoDisplayPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 400;
    const int H = 100;
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

QTEST_MAIN(TestVfoDisplayPresetItem)
#include "tst_vfo_display_preset_item.moc"
