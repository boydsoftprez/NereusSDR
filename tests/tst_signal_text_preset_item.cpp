// =================================================================
// tests/tst_signal_text_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for SignalTextPresetItem — first-class large signal
// readout ("S9 +10  -63 dBm"). Verifies the Thetis MeterManager.cs
// AddSMeterBarText composition port (backdrop + SignalText) and the
// S-unit / dBm format helpers.
//
// no-port-check: test scaffolding; lineage is documented on the header
// of the implementation file. This test exercises the public NereusSDR
// API only and contains no transcribed Thetis source text.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/SignalTextPresetItem.h"

using namespace NereusSDR;

class TestSignalTextPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasTextItem();
    void format_includesSUnitAndDbm();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestSignalTextPresetItem::defaultConstruction_hasTextItem()
{
    SignalTextPresetItem item;
    QVERIFY(item.hasText());
    // Default binding = SignalAvg (Thetis AddSMeterBarText:21696 —
    // cst.ReadingSource = Reading.AVG_SIGNAL_STRENGTH).
    QCOMPARE(item.bindingId(), 1 /* MeterBinding::SignalAvg */);
}

void TestSignalTextPresetItem::format_includesSUnitAndDbm()
{
    SignalTextPresetItem item;
    // S9 = -73 dBm — expect "S9" literal and "dBm" suffix in the
    // produced readout string.
    const QString t = item.formatReadout(-73.0);
    QVERIFY(t.contains(QStringLiteral("S9")));
    QVERIFY(t.contains(QStringLiteral("dBm")));

    // S9 + 10 dB = -63 dBm — expect "S9 +10" and "-63 dBm" fragments.
    const QString t2 = item.formatReadout(-63.0);
    QVERIFY(t2.contains(QStringLiteral("S9")));
    QVERIFY(t2.contains(QStringLiteral("+10")));
    QVERIFY(t2.contains(QStringLiteral("-63")));
}

void TestSignalTextPresetItem::serialize_roundTrip_preservesAllFields()
{
    SignalTextPresetItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.3f);
    a.setBindingId(0 /* SignalPeak */);
    a.setTextColor(QColor(Qt::cyan));
    a.setFontPoint(56.0f);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    SignalTextPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),            a.x());
    QCOMPARE(b.y(),            a.y());
    QCOMPARE(b.itemWidth(),    a.itemWidth());
    QCOMPARE(b.itemHeight(),   a.itemHeight());
    QCOMPARE(b.bindingId(),    0);
    QCOMPARE(b.textColor(),    QColor(Qt::cyan));
    QCOMPARE(b.fontPoint(),    56.0f);
}

void TestSignalTextPresetItem::paintSmoke_rendersAtAspectRatio()
{
    SignalTextPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 300;
    const int H = 80;
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

QTEST_MAIN(TestSignalTextPresetItem)
#include "tst_signal_text_preset_item.moc"
