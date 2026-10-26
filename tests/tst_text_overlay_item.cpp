// no-port-check: NereusSDR-original tests for the text overlay item.
// Modification history (NereusSDR):
//   2026-10-26 - Item editors' colour swatches keep their colour under the
//                 workspace look. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Text overlay RX/TX swap (line 1 on receive, line 2 on
//                 transmit) for new items, both lines for saved ones; the
//                 panel's MOX flip fade; copy font 1 to 2; auto scroll
//                 only when the text is wider than the item. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - RX and TX LED gates: the TX gate follows its own logic
//                 flag (Thetis tests the RX one), an unknown ID hides
//                 nothing, an LED's ID survives save and load, previews
//                 never publish and a gone LED clears its result. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QCheckBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContentFade.h"
#include "gui/containers/meter_property_editors/ClockItemEditor.h"
#include "gui/containers/meter_property_editors/LedItemEditor.h"
#include "gui/containers/meter_property_editors/TextOverlayItemEditor.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/meters/LedResultRegistry.h"
#include "gui/meters/TextOverlayItem.h"

using namespace NereusSDR;

namespace {

constexpr int kWidgetW = 400;
constexpr int kWidgetH = 200;
const QColor kLine1Colour(255, 0, 0);
const QColor kLine2Colour(0, 255, 0);

// A record saved before LineMode existed (22 fields), lines in red and green.
const QString kOldRecord = QStringLiteral(
    "TEXTOVERLAY|0|0|1|1|-1|0|#ffff0000|#ff00ff00|#ff202020|#ff202020|0"
    "|Arial|12|0|Arial|12|0|0|0.1|") + QString::fromLatin1(QByteArray("RECEIVE").toBase64())
    + QLatin1Char('|') + QString::fromLatin1(QByteArray("TRANSMIT").toBase64());

// Far wider than the 400 px item at the new-item font size.
const QString kWideText = QStringLiteral(
    "A receive line far wider than this item can show at once, and longer again to be sure");

std::unique_ptr<TextOverlayItem> swapItem()
{
    auto item = std::make_unique<TextOverlayItem>();
    item->applyNewItemDefaults();
    item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    item->setText1(QStringLiteral("RECEIVE"));
    item->setText2(QStringLiteral("TRANSMIT"));
    item->setTextColour1(kLine1Colour);
    item->setTextColour2(kLine2Colour);
    item->setShowBackPanel(false);
    return item;
}

struct InkCount {
    int line1{0};
    int line2{0};
};

InkCount paintAndCount(TextOverlayItem& item)
{
    QImage image(kWidgetW, kWidgetH, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter p(&image);
        item.paint(p, kWidgetW, kWidgetH);
    }
    InkCount count;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.alpha() < 128) { continue; }
            if (c.red() > 128 && c.green() < 80) { ++count.line1; }
            if (c.green() > 128 && c.red() < 80) { ++count.line2; }
        }
    }
    return count;
}

} // namespace

class TstTextOverlayItem : public QObject {
    Q_OBJECT

private slots:
    void newItemTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("TEXTOVERLAY"));
        std::unique_ptr<MeterItem> made(registry.createMeterItem(entry, nullptr));
        auto* item = qobject_cast<TextOverlayItem*>(made.get());
        QVERIFY(item);
        QCOMPARE(item->lineMode(), TextOverlayItem::LineMode::RxTxSwap);
        QCOMPARE(item->scrollX(), -0.15f);
        QCOMPARE(item->padding(), 0.1f);
        QCOMPARE(item->fontFamily1(), QStringLiteral("Trebuchet MS"));
        QCOMPARE(item->fontSize2(), 18.0f);
        QCOMPARE(item->panelBackColour1(), QColor(32, 32, 32));
        QCOMPARE(item->textBackColour2(), QColor(64, 64, 64));
        QCOMPARE(item->textXOffset1(), 0.0f);
        QVERIFY(!item->fontItalic1());
        QVERIFY(item->showBackPanel());

        // A plain constructed item (what an older record loads into) keeps
        // both lines and no scroll.
        TextOverlayItem plain;
        QCOMPARE(plain.lineMode(), TextOverlayItem::LineMode::Both);
        QCOMPARE(plain.scrollX(), 0.0f);
    }

    void swapPaintsLine1OnRxAndLine2OnTx()
    {
        auto item = swapItem();
        const InkCount rx = paintAndCount(*item);
        QVERIFY2(rx.line1 > 0, "RX paints line 1");
        QCOMPARE(rx.line2, 0);
        QCOMPARE(item->swapLine(kWidgetW, kWidgetH).text, QStringLiteral("RECEIVE"));

        item->setTransmitting(true);
        const InkCount tx = paintAndCount(*item);
        QCOMPARE(tx.line1, 0);
        QVERIFY2(tx.line2 > 0, "TX paints line 2");
        QCOMPARE(item->swapLine(kWidgetW, kWidgetH).text, QStringLiteral("TRANSMIT"));
    }

    void showRxAndShowTxGateTheText()
    {
        auto item = swapItem();
        item->setShowRx(false);
        QCOMPARE(paintAndCount(*item).line1, 0);
        QVERIFY(item->swapLine(kWidgetW, kWidgetH).text.isEmpty());
        item->setTransmitting(true);
        QVERIFY(paintAndCount(*item).line2 > 0);
        item->setShowTx(false);
        QCOMPARE(paintAndCount(*item).line2, 0);
    }

    void offsetsPlaceTheLine()
    {
        auto item = swapItem();
        item->setTextXOffset1(0.25f);
        item->setTextYOffset1(0.1f);
        const auto line = item->swapLine(kWidgetW, kWidgetH);
        // Ratios of the item's width (400 px).
        QVERIFY(qAbs(line.topLeft.x() - 100.0) < 0.01);
        QVERIFY(qAbs(line.topLeft.y() - 40.0) < 0.01);
        // plotText size: 18 * 2.1 * (400 / 52 / 16).
        QCOMPARE(line.font.pixelSize(), 18);
    }

    void oldRecordPaintsBothLines()
    {
        TextOverlayItem item;
        QVERIFY(item.deserialize(kOldRecord));
        QCOMPARE(item.lineMode(), TextOverlayItem::LineMode::Both);
        const InkCount rx = paintAndCount(item);
        QVERIFY(rx.line1 > 0);
        QVERIFY(rx.line2 > 0);
        item.setTransmitting(true);
        const InkCount tx = paintAndCount(item);
        QVERIFY(tx.line1 > 0);
        QVERIFY(tx.line2 > 0);
    }

    void newFieldsRoundTrip()
    {
        auto item = swapItem();
        item->setTextXOffset1(0.125f);
        item->setTextYOffset1(-0.5f);
        item->setTextXOffset2(1.5f);
        item->setTextYOffset2(2.25f);
        item->setFontItalic1(true);
        item->setFontUnderline2(true);
        item->setFontStrikeout2(true);
        const QString saved = item->serialize();

        TextOverlayItem restored;
        QVERIFY(restored.deserialize(saved));
        QCOMPARE(restored.lineMode(), TextOverlayItem::LineMode::RxTxSwap);
        QCOMPARE(restored.textXOffset1(), 0.125f);
        QCOMPARE(restored.textYOffset1(), -0.5f);
        QCOMPARE(restored.textXOffset2(), 1.5f);
        QCOMPARE(restored.textYOffset2(), 2.25f);
        QVERIFY(restored.fontItalic1());
        QVERIFY(!restored.fontUnderline1());
        QVERIFY(restored.fontUnderline2());
        QVERIFY(restored.fontStrikeout2());
        QCOMPARE(restored.serialize(), saved);
    }

    void panelRampsFrom48To255Over500Ms()
    {
        auto item = swapItem();
        item->setShowBackPanel(true);
        item->setPanelBackColour1(QColor(10, 20, 30));
        item->setPanelBackColour2(QColor(200, 30, 30));
        QCOMPARE(item->updateIntervalMs(), 100);

        QVERIFY(!item->advanceMeter(1000));  // takes the time
        QCOMPARE(item->panelFadeValue(), 255);

        item->setTransmitting(true);
        QCOMPARE(item->panelFadeValue(), ContentFade::kDimmedLevel);
        QCOMPARE(item->panelColour().alpha(), ContentFade::kDimmedLevel);
        QCOMPARE(item->panelColour().rgb(), QColor(200, 30, 30).rgb());

        QVERIFY(item->advanceMeter(1250));
        QVERIFY(item->panelFadeValue() > ContentFade::kDimmedLevel);
        QVERIFY(item->panelFadeValue() < 255);

        item->advanceMeter(1500);
        QCOMPARE(item->panelFadeValue(), 255);
        QCOMPARE(item->panelColour().alpha(), 255);

        // Back to receive: dims again and recovers, on the receive colour.
        item->setTransmitting(false);
        QCOMPARE(item->panelFadeValue(), ContentFade::kDimmedLevel);
        QCOMPARE(item->panelColour().rgb(), QColor(10, 20, 30).rgb());
        item->advanceMeter(2000);
        QCOMPARE(item->panelFadeValue(), 255);
    }

    void panelDoesNotFlipWithEqualColoursOrCoverFade()
    {
        auto item = swapItem();
        item->setShowBackPanel(true);
        item->setTransmitting(true);  // Thetis default: both colours 32,32,32
        QCOMPARE(item->panelFadeValue(), 255);

        auto covered = swapItem();
        covered->setPanelBackColour1(QColor(10, 20, 30));
        covered->setPanelBackColour2(QColor(200, 30, 30));
        covered->setFadeOnTx(true);
        covered->setTransmitting(true);
        QCOMPARE(covered->panelFadeValue(), 255);
        QCOMPARE(covered->panelColour().alpha(), 255);
    }

    void copyFont1To2CopiesFamilySizeAndStyles()
    {
        TextOverlayItem item;
        item.setFontFamily1(QStringLiteral("Courier New"));
        item.setFontSize1(31.0f);
        item.setFontBold1(true);
        item.setFontItalic1(true);
        item.setFontUnderline1(true);
        item.setFontStrikeout1(true);
        item.setTextColour1(QColor(1, 2, 3));
        item.setTextBackColour1(QColor(4, 5, 6));
        item.setShowTextBackColour1(true);
        item.copyFont1To2();
        QCOMPARE(item.fontFamily2(), QStringLiteral("Courier New"));
        QCOMPARE(item.fontSize2(), 31.0f);
        QVERIFY(item.fontBold2());
        QVERIFY(item.fontItalic2());
        QVERIFY(item.fontUnderline2());
        QVERIFY(item.fontStrikeout2());
        // Thetis's button also copies the text and text back colours.
        QCOMPARE(item.textColour2(), QColor(1, 2, 3));
        QCOMPARE(item.textBackColour2(), QColor(4, 5, 6));
        QVERIFY(item.showTextBackColour2());

        item.setTextXOffset1(0.3f);
        item.setTextYOffset1(0.4f);
        item.copyOffsets1To2();
        QCOMPARE(item.textXOffset2(), 0.3f);
        QCOMPARE(item.textYOffset2(), 0.4f);
    }

    void wideTextScrollsAndFittingTextDoesNot()
    {
        auto wide = swapItem();
        wide->setText1(kWideText);
        paintAndCount(*wide);
        QVERIFY(!wide->advanceMeter(0));
        QVERIFY(wide->advanceMeter(100));
        // -0.15 item widths per second: -0.015 after 100 ms.
        QCOMPARE(qRound(wide->scrollXOffset1() * 1000.0f), -15);
        QVERIFY(wide->advanceMeter(1000));
        QVERIFY(wide->scrollXOffset1() < -0.1f);
        QVERIFY(wide->swapLine(kWidgetW, kWidgetH).topLeft.x() < 0.0);

        auto fits = swapItem();
        fits->setText1(QStringLiteral("RX"));
        paintAndCount(*fits);
        fits->advanceMeter(0);
        QVERIFY(!fits->advanceMeter(1000));
        QCOMPARE(fits->scrollXOffset1(), 0.0f);
    }

    // ---- LED gates -------------------------------------------------------

    void init() { LedResultRegistry::instance().clear(); }
    void cleanup() { LedResultRegistry::instance().clear(); }

    // The divergence from Thetis (MeterManager.cs:20552-20559): its TX block
    // tests the RX logic flag, so with TX logic on and RX logic off the TX
    // gate never runs. NereusSDR's TX gate follows the TX flag.
    void txGateFollowsItsOwnLogicFlag()
    {
        LedResultRegistry& results = LedResultRegistry::instance();
        auto item = swapItem();
        item->setTxFourChar(QStringLiteral("abcd"));
        item->setTxLedLogic(true);
        item->setRxLedLogic(false);
        item->setTransmitting(true);

        results.set(QStringLiteral("abcd"), true);
        item->refreshLedGates();
        QVERIFY(item->showTx());
        QVERIFY2(paintAndCount(*item).line2 > 0, "TX line shows while abcd is true");

        results.set(QStringLiteral("abcd"), false);
        QVERIFY(item->refreshLedGates());
        QVERIFY(!item->showTx());
        QCOMPARE(paintAndCount(*item).line2, 0);

        // RX logic on and TX logic off: the TX line ignores abcd (Thetis
        // would read abcd through the RX flag and hide it).
        auto other = swapItem();
        other->setRxFourChar(QStringLiteral("wxyz"));
        other->setTxFourChar(QStringLiteral("abcd"));
        other->setRxLedLogic(true);
        other->setTxLedLogic(false);
        results.set(QStringLiteral("wxyz"), true);
        other->setTransmitting(true);
        other->advanceMeter(0);
        QVERIFY(other->showTx());
        QVERIFY(paintAndCount(*other).line2 > 0);

        // Turning the TX logic off shows the line again.
        item->setTxLedLogic(false);
        QVERIFY(item->showTx());
        QVERIFY(paintAndCount(*item).line2 > 0);
    }

    void rxGateWorksTheSameWay()
    {
        LedResultRegistry& results = LedResultRegistry::instance();
        auto item = swapItem();
        item->setRxFourChar(QStringLiteral("Rx01"));
        item->setRxLedLogic(true);
        results.set(QStringLiteral("Rx01"), true);
        item->advanceMeter(0);
        QVERIFY(paintAndCount(*item).line1 > 0);
        results.set(QStringLiteral("Rx01"), false);
        item->advanceMeter(100);
        QCOMPARE(paintAndCount(*item).line1, 0);
        // The TX gate is untouched by the RX one.
        item->setTransmitting(true);
        item->advanceMeter(200);
        QVERIFY(paintAndCount(*item).line2 > 0);
        item->setTransmitting(false);
        item->setRxLedLogic(false);
        QVERIFY(paintAndCount(*item).line1 > 0);
    }

    void unknownIdHidesNothing()
    {
        auto item = swapItem();
        item->setRxFourChar(QStringLiteral("none"));
        item->setTxFourChar(QStringLiteral("gone"));
        item->setRxLedLogic(true);
        item->setTxLedLogic(true);
        item->refreshLedGates();
        QVERIFY(paintAndCount(*item).line1 > 0);
        item->setTransmitting(true);
        item->refreshLedGates();
        QVERIFY(paintAndCount(*item).line2 > 0);
        // An empty ID hides nothing either.
        item->setTxFourChar(QString());
        item->refreshLedGates();
        QVERIFY(item->showTx());
    }

    void liveLedGatesTheOverlay()
    {
        auto led = std::make_unique<LEDItem>();
        const QString id = led->fourChar();
        QCOMPARE(id.size(), 4);
        auto item = swapItem();
        item->setRxFourChar(id);
        item->setRxLedLogic(true);

        led->setValue(1.0);   // at or above the green threshold: true
        QVERIFY(LedResultRegistry::instance().get(id) == std::optional<bool>(true));
        item->refreshLedGates();
        QVERIFY(item->showRx());
        led->setValue(-1.0);
        item->refreshLedGates();
        QVERIFY(!item->showRx());

        // The LED goes: its result goes with it, and the line shows.
        led.reset();
        QVERIFY(!LedResultRegistry::instance().get(id).has_value());
        item->refreshLedGates();
        QVERIFY(item->showRx());
    }

    void ledIdSurvivesSaveAndLoad()
    {
        LEDItem led;
        const QString id = led.fourChar();
        LEDItem back;
        QVERIFY(back.deserialize(led.serialize()));
        QCOMPARE(back.fourChar(), id);
        QCOMPARE(back.guid(), led.guid());
        QCOMPARE(back.serialize(), led.serialize());

        // A record saved before the guid: the same ID on every load, and
        // after it is saved again.
        const QString oldRecord = QStringLiteral(
            "LED|0|0|1|1|-1|0|1|0|#ff00ff88|#ff404040|#ff202020|#ff202020|0|0|0|2|0|1000|1000");
        LEDItem first;
        LEDItem second;
        QVERIFY(first.deserialize(oldRecord));
        QVERIFY(second.deserialize(oldRecord));
        QCOMPARE(first.fourChar(), second.fourChar());
        LEDItem resaved;
        QVERIFY(resaved.deserialize(first.serialize()));
        QCOMPARE(resaved.fourChar(), first.fourChar());
    }

    void previewAndRemovedLedsNeverGate()
    {
        LEDItem shape;
        const QString id = shape.fourChar();
        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.typeId = QStringLiteral("LED");
        entry.config.insert(QStringLiteral("legacyRecord"), shape.serialize());
        LedResultRegistry::instance().clear();

        for (ContentRenderMode mode : {ContentRenderMode::Preview, ContentRenderMode::Validation}) {
            std::unique_ptr<MeterItem> made(registry.createMeterItem(entry, nullptr, mode));
            auto* copy = qobject_cast<LEDItem*>(made.get());
            QVERIFY(copy);
            QVERIFY(!copy->publishEnabled());
            QCOMPARE(copy->fourChar(), id);
            copy->setValue(1.0);
            QVERIFY(!LedResultRegistry::instance().get(id).has_value());
        }

        // The live copy publishes; replacing it with a rebuilt copy and then
        // deleting the old one leaves the rebuilt copy's result.
        std::unique_ptr<MeterItem> live(registry.createMeterItem(entry, nullptr, ContentRenderMode::Live));
        auto* liveLed = qobject_cast<LEDItem*>(live.get());
        QVERIFY(liveLed && liveLed->publishEnabled());
        liveLed->setValue(1.0);
        QVERIFY(LedResultRegistry::instance().get(id) == std::optional<bool>(true));
        std::unique_ptr<MeterItem> rebuilt(registry.createMeterItem(entry, nullptr, ContentRenderMode::Live));
        qobject_cast<LEDItem*>(rebuilt.get())->setValue(-1.0);
        live.reset();
        QVERIFY(LedResultRegistry::instance().get(id) == std::optional<bool>(false));

        // Cleared on request (MeterWidget::removeItem).
        qobject_cast<LEDItem*>(rebuilt.get())->clearPublishedResult();
        QVERIFY(!LedResultRegistry::instance().get(id).has_value());
    }

    // The workspace look leaves an item editor's colour swatches alone: a
    // grey or white colour reads as itself, not as the look's dark fill.
    void colourSwatchesKeepTheirColourUnderTheLook()
    {
        WorkspacePreview::setEnabledForTest(true);
        const auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        const auto swatchSheets = [](QWidget& editor) {
            QHash<QPushButton*, QString> sheets;
            for (QPushButton* b : editor.findChildren<QPushButton*>()) {
                if (b->styleSheet().contains(QLatin1String("background: #"))) { sheets.insert(b, b->styleSheet()); }
            }
            return sheets;
        };
        const auto check = [&swatchSheets](QWidget& editor, const QString& name, const QStringList& colours) {
            const QHash<QPushButton*, QString> before = swatchSheets(editor);
            QStringList all;
            for (const QString& sheet : before) { all.append(sheet); }
            for (const QString& colour : colours) {
                QVERIFY2(all.join(QLatin1Char(' ')).contains(colour), qPrintable(name + QStringLiteral(": no swatch ") + colour));
            }
            WorkspacePreview::themeSubtree(&editor);
            for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
                QVERIFY2(it.key()->styleSheet() == it.value(),
                         qPrintable(QStringLiteral("%1: \"%2\" became \"%3\"").arg(name, it.value(), it.key()->styleSheet())));
            }
        };
        LEDItem led;
        led.setTrueColour(Qt::white);
        led.setFalseColour(QColor(192, 192, 192));
        LedItemEditor ledEditor;
        ledEditor.setItem(&led);
        check(ledEditor, QStringLiteral("LED"), {QStringLiteral("#ffffffff"), QStringLiteral("#ffc0c0c0")});
        TextOverlayItem overlay;
        overlay.setTextColour1(Qt::white);
        overlay.setTextColour2(QColor(128, 128, 128));
        TextOverlayItemEditor overlayEditor;
        overlayEditor.setItem(&overlay);
        check(overlayEditor, QStringLiteral("Text overlay"), {QStringLiteral("#ffffffff"), QStringLiteral("#ff808080")});
        ClockItem clock(nullptr, false);
        clock.applyNewItemDefaults();
        ClockItemEditor clockEditor;
        clockEditor.setItem(&clock);
        check(clockEditor, QStringLiteral("Clock"), {QStringLiteral("#ff808080")});
    }

    void editorsShowTheIdAndWriteTheGates()
    {
        LEDItem led;
        LedItemEditor ledEditor;
        ledEditor.setItem(&led);
        auto* idLabel = ledEditor.findChild<QLabel*>(QStringLiteral("ledId"));
        auto* copy = ledEditor.findChild<QPushButton*>(QStringLiteral("ledCopyId"));
        QVERIFY(idLabel && copy);
        QCOMPARE(idLabel->text(), led.fourChar());
        copy->click();
        QCOMPARE(QGuiApplication::clipboard()->text(), led.fourChar());

        auto item = swapItem();
        TextOverlayItemEditor editor;
        editor.setItem(item.get());
        auto* txLogic = editor.findChild<QCheckBox*>(QStringLiteral("overlayTxLedLogic"));
        auto* txId = editor.findChild<QLineEdit*>(QStringLiteral("overlayTxLedId"));
        auto* rxLogic = editor.findChild<QCheckBox*>(QStringLiteral("overlayRxLedLogic"));
        auto* rxId = editor.findChild<QLineEdit*>(QStringLiteral("overlayRxLedId"));
        QVERIFY(txLogic && txId && rxLogic && rxId);
        txLogic->click();
        QVERIFY(item->txLedLogic());
        QVERIFY(!item->rxLedLogic());
        txId->setText(led.fourChar());
        emit txId->editingFinished();
        QCOMPARE(item->txFourChar(), led.fourChar());
        rxLogic->click();
        QVERIFY(item->rxLedLogic());
    }

    void gateFieldsRoundTrip()
    {
        auto item = swapItem();
        item->setRxLedLogic(true);
        item->setTxLedLogic(true);
        item->setRxFourChar(QStringLiteral("Ab|%x1"));   // kept to Abx1
        item->setTxFourChar(QStringLiteral("Zz90"));
        QCOMPARE(item->rxFourChar(), QStringLiteral("Abx1"));
        const QString saved = item->serialize();
        const QStringList parts = saved.split(QLatin1Char('|'));
        QCOMPARE(parts.size(), 41);
        QCOMPARE(parts.mid(37), QStringList({QStringLiteral("1"), QStringLiteral("1"),
                                             QStringLiteral("Abx1"), QStringLiteral("Zz90")}));
        TextOverlayItem restored;
        QVERIFY(restored.deserialize(saved));
        QVERIFY(restored.rxLedLogic());
        QVERIFY(restored.txLedLogic());
        QCOMPARE(restored.rxFourChar(), QStringLiteral("Abx1"));
        QCOMPARE(restored.txFourChar(), QStringLiteral("Zz90"));
        QCOMPARE(restored.serialize(), saved);

        // A record from before the gates: logic off, no IDs.
        TextOverlayItem old;
        QVERIFY(old.deserialize(kOldRecord));
        QVERIFY(!old.rxLedLogic());
        QVERIFY(!old.txLedLogic());
        QVERIFY(old.rxFourChar().isEmpty());
    }

    void scrollRunsOnceThenRests()
    {
        auto wide = swapItem();
        wide->setText1(kWideText);
        paintAndCount(*wide);
        wide->advanceMeter(0);
        // Steps until it re-enters from the right and reaches the left edge.
        qint64 t = 0;
        for (int i = 0; i < 2000 && wide->scrollXCount1() < 2; ++i) {
            t += 100;
            wide->advanceMeter(t);
        }
        QCOMPARE(wide->scrollXCount1(), 2);
        QCOMPARE(wide->scrollXOffset1(), 0.0f);
        t += 1000;
        QVERIFY(!wide->advanceMeter(t));
        QCOMPARE(wide->scrollXOffset1(), 0.0f);
    }
};

QTEST_MAIN(TstTextOverlayItem)
#include "tst_text_overlay_item.moc"
