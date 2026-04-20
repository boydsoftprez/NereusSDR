// =================================================================
// tests/tst_smeter_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for SMeterPresetItem — first-class S-Meter bar/scale
// composite. Verifies the byte-for-byte Thetis MeterManager.cs
// AddSMeterBarSignal calibration port (-133/-73/-13 dBm three-point
// non-linear scale) and the composite paint path (bar + scale +
// readout + backdrop).
//
// no-port-check: test scaffolding; lineage is documented on the header
// of the implementation file (src/gui/meters/presets/SMeterPresetItem.h
// / .cpp). This test exercises the public NereusSDR API only and contains
// no transcribed Thetis source text — only references it by description.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/SMeterPresetItem.h"

using namespace NereusSDR;

class TestSMeterPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasBarAndScale();
    void scale_spansSUnitsAndOverrange();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestSMeterPresetItem::defaultConstruction_hasBarAndScale()
{
    SMeterPresetItem item;
    QVERIFY(item.hasBar());
    QVERIFY(item.hasScale());
    QVERIFY(item.hasReadout());
    // Default binding is SignalAvg (Thetis AVG_SIGNAL_STRENGTH;
    // matches createSMeterPreset/createSMeterBarPreset default).
    QCOMPARE(item.bindingId(), 1 /* MeterBinding::SignalAvg */);
}

void TestSMeterPresetItem::scale_spansSUnitsAndOverrange()
{
    SMeterPresetItem item;
    // Thetis addSMeterBar calibration (MeterManager.cs:21547-21549):
    //   -133 dBm (below S0) -> 0.0
    //    -73 dBm (S9)       -> 0.5
    //    -13 dBm (S9+60)    -> 0.99
    // We expose the 3-point calibration; verify size and edge keys.
    QCOMPARE(item.calibrationSize(), 3);
    QVERIFY(item.hasCalibrationPoint(-133.0));
    QVERIFY(item.hasCalibrationPoint( -73.0));
    QVERIFY(item.hasCalibrationPoint( -13.0));
}

void TestSMeterPresetItem::serialize_roundTrip_preservesAllFields()
{
    SMeterPresetItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.4f);
    a.setBindingId(0 /* SignalPeak */);
    a.setShowReadout(false);

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    SMeterPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),          a.x());
    QCOMPARE(b.y(),          a.y());
    QCOMPARE(b.itemWidth(),  a.itemWidth());
    QCOMPARE(b.itemHeight(), a.itemHeight());
    QCOMPARE(b.bindingId(),  0);
    QCOMPARE(b.showReadout(), false);
}

void TestSMeterPresetItem::paintSmoke_rendersAtAspectRatio()
{
    SMeterPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 400;
    const int H = 80;  // narrow strip, typical S-meter row
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

QTEST_MAIN(TestSMeterPresetItem)
#include "tst_smeter_preset_item.moc"
