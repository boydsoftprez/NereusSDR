// =================================================================
// tests/tst_contest_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for ContestPresetItem — NereusSDR-local full-container
// composite (VFO + band buttons + mode buttons + clock on a backdrop).
// Verifies the composition, serialisation round-trip, and paint smoke.
//
// no-port-check: NereusSDR-local scaffolding; no Thetis source is
// referenced. The implementation file is a collapse of
// src/gui/meters/ItemGroup.cpp's createContestPreset helper.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/presets/ContestPresetItem.h"

using namespace NereusSDR;

class TestContestPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_hasAllFiveChildren();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtAspectRatio();
};

void TestContestPresetItem::defaultConstruction_hasAllFiveChildren()
{
    ContestPresetItem item;
    // Five stacked sub-regions: backdrop (full), VFO (0..30%),
    // band buttons (30..55%), mode buttons (55..75%), clock (75..100%).
    QVERIFY(item.hasBackdrop());
    QVERIFY(item.hasVfo());
    QVERIFY(item.hasBandButtons());
    QVERIFY(item.hasModeButtons());
    QVERIFY(item.hasClock());
}

void TestContestPresetItem::serialize_roundTrip_preservesAllFields()
{
    ContestPresetItem a;
    a.setRect(0.1f, 0.1f, 0.8f, 0.8f);
    a.setFrequencyHz(7150000.0);
    a.setBandLabel(QStringLiteral("40m"));
    a.setModeLabel(QStringLiteral("CW"));

    const QString blob = a.serialize();
    QVERIFY(!blob.isEmpty());

    ContestPresetItem b;
    QVERIFY(b.deserialize(blob));

    QCOMPARE(b.x(),           a.x());
    QCOMPARE(b.y(),           a.y());
    QCOMPARE(b.itemWidth(),   a.itemWidth());
    QCOMPARE(b.itemHeight(),  a.itemHeight());
    QCOMPARE(b.frequencyHz(), 7150000.0);
    QCOMPARE(b.bandLabel(),   QStringLiteral("40m"));
    QCOMPARE(b.modeLabel(),   QStringLiteral("CW"));
}

void TestContestPresetItem::paintSmoke_rendersAtAspectRatio()
{
    ContestPresetItem item;
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    const int W = 400;
    const int H = 400;  // square layout
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

QTEST_MAIN(TestContestPresetItem)
#include "tst_contest_preset_item.moc"
