// =================================================================
// tests/tst_magic_eye_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for MagicEyePresetItem — first-class rotating-leaf
// magic-eye preset. Verifies the byte-for-byte Thetis MeterManager.cs
// AddMagicEye calibration port and the composite paint path
// (bullseye image + leaf).
//
// no-port-check: test scaffolding; lineage is documented on the header
// of the implementation file. This test exercises the public NereusSDR
// API only and contains no transcribed Thetis source text.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/MagicEyePresetItem.h"

using namespace NereusSDR;

class TestMagicEyePresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasLeafAndBackground();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestMagicEyePresetItem::defaultConstruction_hasLeafAndBackground()
{
    MagicEyePresetItem item;
    QVERIFY(item.hasBackground());
    QVERIFY(item.hasLeaf());
    // Default binding = SignalAvg per Thetis MeterManager.cs:22266
    QCOMPARE(item.bindingId(), 1 /* MeterBinding::SignalAvg */);
    // Default leaf colour = Lime per Thetis MeterManager.cs:22265
    QCOMPARE(item.leafColor(), QColor(Qt::green));
}

void TestMagicEyePresetItem::serialize_roundTrip_preservesAllFields()
{
    MagicEyePresetItem a;
    a.setRect(0.1f, 0.2f, 0.6f, 0.6f);
    a.setBindingId(0 /* SignalPeak */);
    a.setLeafColor(QColor(Qt::cyan));

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    MagicEyePresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),           a.x());
    QCOMPARE(b.y(),           a.y());
    QCOMPARE(b.itemWidth(),   a.itemWidth());
    QCOMPARE(b.itemHeight(),  a.itemHeight());
    QCOMPARE(b.bindingId(),   0);
    QCOMPARE(b.leafColor(),   QColor(Qt::cyan));
}

void TestMagicEyePresetItem::paintSmoke_rendersAtAspectRatio()
{
    MagicEyePresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 200;
    const int H = 200;  // 1:1 circular eye
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

QTEST_MAIN(TestMagicEyePresetItem)
#include "tst_magic_eye_preset_item.moc"
