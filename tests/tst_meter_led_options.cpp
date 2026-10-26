// no-port-check: NereusSDR-original tests for the LED options; the Thetis
// references are cites of the behaviour under test, not ported code.
// =================================================================
// tests/tst_meter_led_options.cpp  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave M7: the offset tooltip names the item's width,
//                 American spelling. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - The appended options are read by place; the LED record
//                 now ends with the condition, No TX fields and guid.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// Container item parity Task 25: the LED building block. The placed look
// (Thetis renderLed, MeterManager.cs:34010-34130 [v2.10.3.15]): the colour
// fade at 250 ms, the blink edge and drop at 100 ms, pulsate, the panel
// RX/TX colour with its flip fade, offset and size; show true / show false;
// an update interval under 50 ms raised to 100 ms (:27446); saved records
// keep the fill look and thresholds; new LEDs take kLedThetisDefaults
// (:20825-20873).
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
#include <QSpinBox>

#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/LedItemEditor.h"
#include "gui/meters/LEDItem.h"

using namespace NereusSDR;

namespace {

const QColor kRed(0xff, 0x00, 0x00);
const QColor kBlue(0x00, 0x00, 0xff);

// A placed LED with the new-item defaults, 50 ms frames.
std::unique_ptr<LEDItem> makePlacedLed()
{
    auto led = std::make_unique<LEDItem>();
    led->applyNewItemDefaults();
    led->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    led->setUpdateIntervalMs(50);
    return led;
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

// Runs `frames` placed-look frames of `intervalMs` after the first call,
// which only takes the time. Returns the colour fade after each frame.
QList<float> runFrames(LEDItem& led, int frames, int intervalMs, qint64 start = 1000)
{
    led.advanceMeter(start);
    QList<float> fades;
    for (int i = 1; i <= frames; ++i) {
        led.advanceMeter(start + static_cast<qint64>(i) * intervalMs);
        fades.append(led.colourFade());
    }
    return fades;
}

bool near(float a, float b)
{
    return qAbs(a - b) < 1e-4f;
}

bool nearRect(const QRectF& a, const QRectF& b)
{
    return qAbs(a.x() - b.x()) < 1e-3 && qAbs(a.y() - b.y()) < 1e-3
        && qAbs(a.width() - b.width()) < 1e-3 && qAbs(a.height() - b.height()) < 1e-3;
}

// A record saved before the options existed: round, flat, thresholds
// green 0 / amber 1000 / red 1000, padding 2 px. The format string then had
// three placeholders more than values, so saved records end "|%15|%16|%17".
const QString kOldRecord = QStringLiteral(
    "LED|0|0|1|1|-1|0|1|0|#ff00ff88|#ff404040|#ff202020|#ff202020|0|0|0|2|0|1000|1000|%15|%16|%17");
// The same without the unfilled placeholders.
const QString kOldRecordShort = QStringLiteral(
    "LED|0|0|1|1|-1|0|1|0|#ff00ff88|#ff404040|#ff202020|#ff202020|0|0|0|2|0|1000|1000");

QDoubleSpinBox* spinWithTip(QWidget& editor, const QString& tipStart)
{
    for (QDoubleSpinBox* spin : editor.findChildren<QDoubleSpinBox*>()) {
        if (spin->toolTip().startsWith(tipStart)) { return spin; }
    }
    return nullptr;
}

QCheckBox* checkWithTip(QWidget& editor, const QString& tipStart)
{
    for (QCheckBox* box : editor.findChildren<QCheckBox*>()) {
        if (box->toolTip().startsWith(tipStart)) { return box; }
    }
    return nullptr;
}

} // namespace

class TstMeterLedOptions : public QObject {
    Q_OBJECT

private slots:
    void updateIntervalUnder50Becomes100()
    {
        // From Thetis MeterManager.cs:27446 [v2.10.3.15]
        LEDItem led;
        led.setUpdateIntervalMs(20);
        QCOMPARE(led.updateIntervalMs(), 100);
        led.setUpdateIntervalMs(49);
        QCOMPARE(led.updateIntervalMs(), 100);
        led.setUpdateIntervalMs(50);
        QCOMPARE(led.updateIntervalMs(), 50);
        led.setUpdateIntervalMs(10000);
        QCOMPARE(led.updateIntervalMs(), 10000);
        // 0: a saved item without an interval keeps taking every poll.
        led.setUpdateIntervalMs(0);
        QCOMPARE(led.updateIntervalMs(), 0);
    }

    void isTrueFollowsTheThresholds()
    {
        LEDItem led;
        led.setGreenThreshold(-80.0);
        led.setAmberThreshold(-60.0);
        led.setRedThreshold(-40.0);
        led.setValue(-90.0);
        QVERIFY(!led.isTrue());
        led.setValue(-80.0);
        QVERIFY(led.isTrue());
        led.setValue(-50.0);
        QVERIFY(led.isTrue());
        led.setValue(-10.0);
        QVERIFY(led.isTrue());
        led.setValue(-81.0);
        QVERIFY(!led.isTrue());
    }

    void fadeToTrueTakes250Ms()
    {
        // intervalSpeed = 250 (MeterManager.cs:34050 [v2.10.3.15]): at
        // 50 ms frames, five steps of 0.2.
        auto led = makePlacedLed();
        led->setValue(1.0);
        const QList<float> fades = runFrames(*led, 6, 50);
        QVERIFY(near(fades.at(0), 0.2f));
        QVERIFY(fades.at(3) < 1.0f);          // 200 ms
        QCOMPARE(fades.at(4), 1.0f);          // 250 ms
        QCOMPARE(fades.at(5), 1.0f);
        QCOMPARE(led->placedLightColour(), QColor(0, 255, 0));

        // Back to false: down over the same 250 ms. Single-precision steps
        // of 0.2 leave a few 1e-8 after five, as in Thetis, so the clamp to
        // 0 lands one frame later.
        led->setValue(-1.0);
        // Frames continue from 1300 ms, where the climb stopped.
        const QList<float> down = runFrames(*led, 6, 50, 1300);
        QVERIFY(down.at(3) > 0.1f);
        QVERIFY(near(down.at(4), 0.0f));
        QCOMPARE(down.at(5), 0.0f);
        QCOMPARE(led->placedLightColour(), QColor(192, 192, 192));
    }

    void blinkEdgeAndDropUse100Ms()
    {
        // Blink: the edge frame and the first drop frame use intervalSpeed
        // 100 (:34058, :34073); the climbs use 250. Two flashes, then the
        // light stays at the false colour while still true (:34074-34079).
        auto led = makePlacedLed();
        led->setLightMode(LEDItem::LightMode::Blink);
        led->setValue(1.0);
        const QList<float> fades = runFrames(*led, 40, 50);
        QVERIFY(near(fades.at(0), 0.5f));     // edge: 1 / ceil(100 / 50)
        QVERIFY(near(fades.at(1), 0.7f));     // climb: 1 / ceil(250 / 50)
        QVERIFY(near(fades.at(2), 0.9f));
        QCOMPARE(fades.at(3), 1.0f);
        QVERIFY(near(fades.at(4), 0.5f));     // drop: 100 ms speed
        QVERIFY(near(fades.at(5), 0.3f));
        // Second flash, then dark.
        QVERIFY(fades.contains(1.0f));
        QCOMPARE(fades.constLast(), 0.0f);
        QCOMPARE(fades.at(30), 0.0f);
        QVERIFY(led->isTrue());
    }

    void pulsateRisesAndFallsAt250Ms()
    {
        auto led = makePlacedLed();
        led->setLightMode(LEDItem::LightMode::Pulsate);
        QCOMPARE(led->lightMode(), LEDItem::LightMode::Pulsate);
        led->setValue(1.0);
        const QList<float> fades = runFrames(*led, 13, 50);
        QCOMPARE(fades.at(4), 1.0f);          // up in 250 ms
        QVERIFY(near(fades.at(5), 0.8f));
        QVERIFY(near(fades.at(9), 0.0f));     // down in 250 ms
        // Up again once the fade reaches 0 (one frame later, see above).
        QVERIFY(near(fades.at(11), 0.2f) || near(fades.at(10), 0.2f));
        QVERIFY(fades.at(12) > fades.at(11));
    }

    void lightModeIsBlinkAndPulsate()
    {
        LEDItem led;
        QCOMPARE(led.lightMode(), LEDItem::LightMode::OnOff);
        led.setLightMode(LEDItem::LightMode::Blink);
        QVERIFY(led.blink());
        QVERIFY(!led.pulsate());
        led.setPulsate(true);
        // renderLed checks Blink first (:34065).
        QCOMPARE(led.lightMode(), LEDItem::LightMode::Blink);
        led.setLightMode(LEDItem::LightMode::OnOff);
        QVERIFY(!led.blink());
        QVERIFY(!led.pulsate());
    }

    void showFalseOffHidesTheLightWhileFalse()
    {
        // Placed look: the panel stays, the light goes.
        auto led = makePlacedLed();
        led->setShowFalse(false);
        led->setValue(-1.0);
        QVERIFY(!led->isTrue());
        QImage img = render(*led);
        QCOMPARE(img.pixelColor(100, 10), QColor(32, 32, 32));
        led->setShowFalse(true);
        img = render(*led);
        QCOMPARE(img.pixelColor(100, 10), QColor(192, 192, 192));

        // Show true off hides it while true.
        led->setShowTrue(false);
        led->setValue(1.0);
        QCOMPARE(render(*led).pixelColor(100, 10), QColor(32, 32, 32));

        // Fill look: nothing is drawn while false.
        LEDItem fill;
        QVERIFY(fill.deserialize(kOldRecord));
        fill.setShowFalse(false);
        fill.setValue(-1.0);
        QCOMPARE(render(fill).pixelColor(100, 100).alpha(), 0);
        fill.setValue(1.0);
        QCOMPARE(render(fill).pixelColor(100, 100), QColor(0x00, 0xff, 0x88));
    }

    void placedGeometryUsesTheWidth()
    {
        // Panel width x padding (:27481); light centred at offset x width,
        // size x width (:34125-34129).
        auto led = makePlacedLed();
        QCOMPARE(led->placedPanelRect(200, 200), QRect(0, 0, 200, 20));
        QVERIFY(nearRect(led->placedLightRect(200, 200), QRectF(95.0, 5.0, 10.0, 10.0)));
        led->setOffsetX(0.25f);
        led->setOffsetY(0.5f);
        led->setSizeX(0.1f);
        led->setSizeY(0.2f);
        QVERIFY(nearRect(led->placedLightRect(200, 200), QRectF(40.0, 80.0, 20.0, 40.0)));
        // The panel stops at the item's bottom edge.
        led->setPadding(2.0f);
        QCOMPARE(led->placedPanelRect(200, 100).height(), 100);
    }

    void panelFollowsTransmitWithTheFlipFade()
    {
        // :34019-34045: panel colour 1 receiving, 2 transmitting, from 48 on
        // each flip over half a second.
        auto led = makePlacedLed();
        led->setUpdateIntervalMs(100);
        led->setPanelBackColour1(kRed);
        led->setPanelBackColour2(kBlue);
        QCOMPARE(led->placedPanelColour(), kRed);
        led->setTransmitting(true);
        QCOMPARE(led->panelFadeValue(), 48);
        QCOMPARE(led->placedPanelColour().rgb(), kBlue.rgb());
        QCOMPARE(led->placedPanelColour().alpha(), 48);
        led->advanceMeter(1000);
        led->advanceMeter(1100);
        QCOMPARE(led->panelFadeValue(), 90);
        led->advanceMeter(1500);
        QCOMPARE(led->panelFadeValue(), 255);
        QCOMPARE(led->placedPanelColour(), kBlue);
    }

    void oldRecordKeepsThresholdsAndTheFillLook()
    {
        LEDItem led;
        QVERIFY(led.deserialize(kOldRecord));
        QVERIFY(!led.placed());
        QVERIFY(led.showTrue());
        QVERIFY(led.showFalse());
        QVERIFY(!led.processWhenHidden());
        QCOMPARE(led.padding(), 2.0f);

        // Thresholds and the fill: the light fills the item inside 2 px.
        led.setValue(5.0);
        QVERIFY(led.isTrue());
        QImage img = render(led);
        QCOMPARE(img.pixelColor(100, 100), QColor(0x00, 0xff, 0x88));
        QCOMPARE(img.pixelColor(100, 20), QColor(0x00, 0xff, 0x88));
        QCOMPARE(img.pixelColor(0, 0).alpha(), 0);
        led.setValue(-5.0);
        QCOMPARE(render(led).pixelColor(100, 100), QColor(0x40, 0x40, 0x40));
        led.setAmberThreshold(10.0);
        led.setValue(20.0);
        QCOMPARE(render(led).pixelColor(100, 100), QColor(0xff, 0xb8, 0x00));
        // No placed animation runs.
        QVERIFY(!led.advanceMeter(1000));
        QVERIFY(!led.advanceMeter(2000));

        // The short form loads the same way.
        LEDItem shortForm;
        QVERIFY(shortForm.deserialize(kOldRecordShort));
        QVERIFY(!shortForm.placed());
        QVERIFY(shortForm.showFalse());

        // Saved again: the old fields are unchanged, the placeholders are
        // gone, and it loads the same.
        const QString again = led.serialize();
        QVERIFY(!again.contains(QLatin1Char('%')));
        QCOMPARE(again.section(QLatin1Char('|'), 7, 17),
                 kOldRecord.section(QLatin1Char('|'), 7, 17));
        LEDItem back;
        QVERIFY(back.deserialize(again));
        QVERIFY(!back.placed());
        QCOMPARE(back.padding(), 2.0f);
        QCOMPARE(back.ledShape(), LEDItem::LedShape::Round);
        QCOMPARE(back.trueColour(), QColor(0x00, 0xff, 0x88));
        QCOMPARE(back.falseColour(), QColor(0x40, 0x40, 0x40));
    }

    void newFieldsRoundTrip()
    {
        auto led = makePlacedLed();
        led->setOffsetX(-1.5f);
        led->setOffsetY(2.25f);
        led->setSizeX(0.125f);
        led->setSizeY(3.0f);
        led->setShowTrue(false);
        led->setShowFalse(false);
        led->setProcessWhenHidden(true);
        const QString record = led->serialize();
        // The options at [20]-[27]; the condition, No TX fields and guid follow.
        QCOMPARE(record.section(QLatin1Char('|'), 20, 27), QStringLiteral("1|-1.5|2.25|0.125|3|0|0|1"));

        LEDItem back;
        QVERIFY(back.deserialize(record));
        QVERIFY(back.placed());
        QCOMPARE(back.offsetX(), -1.5f);
        QCOMPARE(back.offsetY(), 2.25f);
        QCOMPARE(back.sizeX(), 0.125f);
        QCOMPARE(back.sizeY(), 3.0f);
        QVERIFY(!back.showTrue());
        QVERIFY(!back.showFalse());
        QVERIFY(back.processWhenHidden());
        QCOMPARE(back.padding(), 0.1f);

        // A bad appended number is refused, as the old fields are.
        QString bad = record;
        bad.replace(QStringLiteral("|2.25|"), QStringLiteral("|x|"));
        LEDItem refused;
        QVERIFY(!refused.deserialize(bad));
    }

    void newLedTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("LED"));
        QCOMPARE(entry.typeId, QStringLiteral("LED"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* led = qobject_cast<LEDItem*>(item.get());
        QVERIFY(led);
        // From Thetis MeterManager.cs:20836-20873 [v2.10.3.15]
        QVERIFY(led->placed());
        QCOMPARE(led->trueColour(), QColor(0, 255, 0));
        QCOMPARE(led->falseColour(), QColor(192, 192, 192));
        QCOMPARE(led->panelBackColour1(), QColor(32, 32, 32));
        QCOMPARE(led->panelBackColour2(), QColor(32, 32, 32));
        QVERIFY(led->showBackPanel());
        QCOMPARE(led->offsetX(), 0.5f);
        QCOMPARE(led->offsetY(), 0.05f);
        QCOMPARE(led->sizeX(), 0.05f);
        QCOMPARE(led->sizeY(), 0.05f);
        QCOMPARE(led->padding(), 0.1f);
        QCOMPARE(led->ledShape(), LEDItem::LedShape::Square);
        QCOMPARE(led->ledStyle(), LEDItem::LedStyle::Flat);
        QCOMPARE(led->lightMode(), LEDItem::LightMode::OnOff);
        QVERIFY(led->showTrue());
        QVERIFY(led->showFalse());
        QVERIFY(!led->processWhenHidden());
        QCOMPARE(led->updateIntervalMs(), 100);
        // The thresholds keep NereusSDR's defaults.
        QCOMPARE(led->greenThreshold(), 0.0);
        QCOMPARE(led->amberThreshold(), 1000.0);
        QCOMPARE(led->redThreshold(), 1000.0);
    }

    void editorShowsAndSetsTheOptions()
    {
        auto led = makePlacedLed();
        LedItemEditor editor;
        editor.setItem(led.get());

        // Update interval 50-10000 (setup.designer.cs:63735-63744 [v2.10.3.15]).
        auto* interval = editor.findChild<QSpinBox*>(QStringLiteral("updateIntervalMs"));
        QVERIFY(interval);
        QCOMPARE(interval->minimum(), 50);
        QCOMPARE(interval->maximum(), 10000);

        QCheckBox* placed = checkWithTip(editor, QStringLiteral("Draw the light at the offset"));
        QVERIFY(placed);
        QVERIFY(placed->isChecked());
        QDoubleSpinBox* padding = spinWithTip(editor, QStringLiteral("Height of the panel"));
        QVERIFY(padding);
        QCOMPARE(padding->minimum(), 0.001);
        QCOMPARE(padding->maximum(), 10.0);

        QDoubleSpinBox* offsetX = spinWithTip(editor, QStringLiteral("Center of the light from the left"));
        // Fix wave M7: Thetis scales the offset by the item's width.
        QVERIFY(offsetX && offsetX->toolTip().contains(QStringLiteral("item's width")));
        QVERIFY(offsetX);
        QCOMPARE(offsetX->minimum(), -10.0);
        QCOMPARE(offsetX->maximum(), 10.0);
        QVERIFY(offsetX->isEnabled());
        offsetX->setValue(0.75);
        QCOMPARE(led->offsetX(), 0.75f);

        QCheckBox* showFalse = checkWithTip(editor, QStringLiteral("Draw the light while it is false"));
        QVERIFY(showFalse);
        showFalse->setChecked(false);
        QVERIFY(!led->showFalse());
        QCheckBox* hidden = checkWithTip(editor, QStringLiteral("Keep working out"));
        QVERIFY(hidden);
        hidden->setChecked(true);
        QVERIFY(led->processWhenHidden());

        // A placed light takes one light mode.
        QCheckBox* blink = checkWithTip(editor, QStringLiteral("Placed light: flash twice"));
        QCheckBox* pulsate = checkWithTip(editor, QStringLiteral("Placed light: fade up"));
        QVERIFY(blink);
        QVERIFY(pulsate);
        blink->setChecked(true);
        pulsate->setChecked(true);
        QVERIFY(!blink->isChecked());
        QCOMPARE(led->lightMode(), LEDItem::LightMode::Pulsate);

        // Off: the fill look, pixel padding, offset and size disabled.
        placed->setChecked(false);
        QVERIFY(!led->placed());
        QCOMPARE(led->padding(), 2.0f);
        QVERIFY(padding->toolTip().startsWith(QStringLiteral("Space in pixels")));
        QCOMPARE(padding->maximum(), 50.0);
        QVERIFY(!offsetX->isEnabled());
    }
};

QTEST_MAIN(TstMeterLedOptions)
#include "tst_meter_led_options.moc"
