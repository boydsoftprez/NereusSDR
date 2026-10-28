// no-port-check: NereusSDR-original tests for the spacer options; the
// Thetis references are cites of the behaviour under test, not ported code.
// =================================================================
// tests/tst_meter_spacer.cpp  (NereusSDR)
// =================================================================
//
// Container item parity Task 24: the spacer building block (SPACER and
// meter.spacer). RX/TX colour mode with the transmit-flip fade from 48
// (Thetis renderSpacer, MeterManager.cs:36544-36576 [v2.10.3.15]),
// padding sets height (:27627), saved records keep the gradient, and
// new spacers take kSpacerThetisDefaults (:17441-17447).
//
// Modification history (NereusSDR):
//   2026-10-26 - Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QImage>
#include <QPainter>

#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/SpacerItemEditor.h"
#include "gui/meters/SpacerItem.h"

using namespace NereusSDR;

namespace {

const QColor kRed(0xff, 0x00, 0x00);
const QColor kBlue(0x00, 0x00, 0xff);

std::unique_ptr<SpacerItem> makeRxTxSpacer()
{
    auto spacer = std::make_unique<SpacerItem>();
    spacer->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    spacer->setRxTxColourMode(true);
    spacer->setColour1(kRed);
    spacer->setColour2(kBlue);
    spacer->setUpdateIntervalMs(100);
    return spacer;
}

QImage render(MeterItem& item, int w = 200, int h = 200)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    item.paint(p, w, h);
    p.end();
    return img;
}

// A record saved before the RX/TX colour mode and padding height existed.
const QString kOldRecord =
    QStringLiteral("SPACER|0|0|1|1|-1|0|0.1|#ffff0000|#ff0000ff");

QCheckBox* checkWithTip(QWidget& editor, const QString& tipStart)
{
    const auto boxes = editor.findChildren<QCheckBox*>();
    for (QCheckBox* box : boxes) {
        if (box->toolTip().startsWith(tipStart)) { return box; }
    }
    return nullptr;
}

} // namespace

class TstMeterSpacer : public QObject {
    Q_OBJECT

private slots:
    void rxTxModeShowsColourOneReceivingAndColourTwoTransmitting()
    {
        auto spacer = makeRxTxSpacer();
        spacer->setTransmitting(false);
        QCOMPARE(spacer->flipFadeValue(), 255);
        QCOMPARE(render(*spacer).pixelColor(100, 100), kRed);

        spacer->setTransmitting(true);
        QCOMPARE(spacer->rxTxColour().rgb(), kBlue.rgb());
        QCOMPARE(render(*spacer).pixelColor(100, 100).rgb(), kBlue.rgb());

        spacer->setTransmitting(false);
        QCOMPARE(spacer->rxTxColour().rgb(), kRed.rgb());
    }

    void eachFlipRampsFrom48()
    {
        // Thetis renderSpacer: FadeValue = 48 on a MOX flip, then
        // ceil(207 / ceil(500 / interval)) per step (MeterManager.cs:
        // 36556-36570 [v2.10.3.15]). At 100 ms: 5 steps of 42.
        auto spacer = makeRxTxSpacer();
        spacer->setTransmitting(true);
        QCOMPARE(spacer->flipFadeValue(), 48);
        QCOMPARE(spacer->rxTxColour().alpha(), 48);
        QCOMPARE(render(*spacer).pixelColor(100, 100).alpha(), 48);

        // The first advance only takes the time, and asks for a repaint.
        QVERIFY(spacer->advanceMeter(1000));
        QCOMPARE(spacer->flipFadeValue(), 48);
        QVERIFY(spacer->advanceMeter(1100));
        QCOMPARE(spacer->flipFadeValue(), 90);
        QVERIFY(spacer->advanceMeter(1500));
        QCOMPARE(spacer->flipFadeValue(), 255);
        QVERIFY(!spacer->advanceMeter(1600));
        QCOMPARE(spacer->rxTxColour(), kBlue);

        // Back to receive: colour 1, from 48 again.
        spacer->setTransmitting(false);
        QCOMPARE(spacer->flipFadeValue(), 48);
        QCOMPARE(spacer->rxTxColour().rgb(), kRed.rgb());
        QCOMPARE(spacer->rxTxColour().alpha(), 48);
        QVERIFY(spacer->advanceMeter(2000));
        QVERIFY(spacer->advanceMeter(2500));
        QCOMPARE(spacer->flipFadeValue(), 255);
    }

    void sameColoursOrCoverFadeDoNotRamp()
    {
        // Thetis ramps only when the colours differ and no fade on RX / TX
        // cover is active (MeterManager.cs:36553-36554 [v2.10.3.15]).
        auto same = makeRxTxSpacer();
        same->setColour2(kRed);
        same->setTransmitting(true);
        QCOMPARE(same->flipFadeValue(), 255);
        QCOMPARE(same->rxTxColour(), kRed);

        auto covered = makeRxTxSpacer();
        covered->setFadeOnTx(true);
        covered->setTransmitting(true);
        QCOMPARE(covered->flipFadeValue(), 255);
        QCOMPARE(covered->rxTxColour(), kBlue);
    }

    void paddingSetsHeightIsWidthTimesPadding()
    {
        auto spacer = makeRxTxSpacer();
        spacer->setPadding(0.1f);
        QCOMPARE(spacer->spacerRect(200, 200).height(), 200);

        spacer->setPaddingSetsHeight(true);
        QCOMPARE(spacer->spacerRect(200, 200), QRect(0, 0, 200, 20));
        QCOMPARE(spacer->spacerRect(300, 200).height(), 30);
        const QImage img = render(*spacer);
        QCOMPARE(img.pixelColor(100, 10), kRed);
        QCOMPARE(img.pixelColor(100, 30).alpha(), 0);
    }

    void oldRecordKeepsTheGradient()
    {
        SpacerItem spacer;
        QVERIFY(spacer.deserialize(kOldRecord));
        QVERIFY(!spacer.rxTxColourMode());
        QVERIFY(!spacer.paddingSetsHeight());
        QCOMPARE(spacer.spacerRect(200, 200).height(), 200);

        // Colour 1 at the top, colour 2 at the bottom, transmitting or not.
        spacer.setTransmitting(true);
        const QImage img = render(spacer);
        QVERIFY(img.pixelColor(100, 0).red() > 240);
        QVERIFY(img.pixelColor(100, 199).blue() > 240);
        QVERIFY(img.pixelColor(100, 100).red() > 100);
        QVERIFY(img.pixelColor(100, 100).blue() > 100);

        // Saving it again keeps it the same.
        SpacerItem again;
        QVERIFY(again.deserialize(spacer.serialize()));
        QVERIFY(!again.rxTxColourMode());
        QVERIFY(!again.paddingSetsHeight());
        QCOMPARE(again.colour1(), kRed);
        QCOMPARE(again.colour2(), kBlue);
    }

    void newFieldsRoundTrip()
    {
        auto spacer = makeRxTxSpacer();
        spacer->setPaddingSetsHeight(true);
        spacer->setPadding(2.5f);
        const QString record = spacer->serialize();
        QVERIFY(record.endsWith(QStringLiteral("|1|1")));

        SpacerItem back;
        QVERIFY(back.deserialize(record));
        QVERIFY(back.rxTxColourMode());
        QVERIFY(back.paddingSetsHeight());
        QCOMPARE(back.padding(), 2.5f);
        QCOMPARE(back.colour1(), kRed);
        QCOMPARE(back.colour2(), kBlue);
    }

    void newSpacersTakeThetisDefaults_data()
    {
        QTest::addColumn<QString>("typeId");
        QTest::newRow("SPACER") << QStringLiteral("SPACER");
        QTest::newRow("meter.spacer") << QStringLiteral("meter.spacer");
    }

    void newSpacersTakeThetisDefaults()
    {
        QFETCH(QString, typeId);
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(typeId);
        QCOMPARE(entry.typeId, typeId);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* spacer = qobject_cast<SpacerItem*>(item.get());
        QVERIFY(spacer);
        // From Thetis MeterManager.cs:17443-17446 [v2.10.3.15]
        QVERIFY(spacer->rxTxColourMode());
        QVERIFY(spacer->paddingSetsHeight());
        QCOMPARE(spacer->colour1(), QColor(32, 32, 32));
        QCOMPARE(spacer->colour2(), QColor(32, 32, 32));
        QCOMPARE(spacer->padding(), 0.1f);
    }

    void editorShowsAndSetsTheOptions()
    {
        auto spacer = makeRxTxSpacer();
        SpacerItemEditor editor;
        editor.setItem(spacer.get());

        QCheckBox* rxTx = checkWithTip(editor, QStringLiteral("Show color 1 while receiving"));
        QCheckBox* height = checkWithTip(editor, QStringLiteral("Draw the spacer as tall"));
        QVERIFY(rxTx);
        QVERIFY(height);
        QVERIFY(rxTx->isChecked());
        QVERIFY(!height->isChecked());

        height->setChecked(true);
        QVERIFY(spacer->paddingSetsHeight());
        rxTx->setChecked(false);
        QVERIFY(!spacer->rxTxColourMode());

        // Padding range 0.001-10 (setup.designer.cs:63167-63192 [v2.10.3.15]).
        QDoubleSpinBox* padding = nullptr;
        for (QDoubleSpinBox* spin : editor.findChildren<QDoubleSpinBox*>()) {
            if (spin->toolTip().startsWith(QStringLiteral("Size of the spacer"))) { padding = spin; }
        }
        QVERIFY(padding);
        QCOMPARE(padding->minimum(), 0.001);
        QCOMPARE(padding->maximum(), 10.0);
        QCOMPARE(padding->singleStep(), 0.001);
        padding->setValue(5.0);
        QCOMPARE(spacer->padding(), 5.0f);
    }
};

QTEST_MAIN(TstMeterSpacer)
#include "tst_meter_spacer.moc"
