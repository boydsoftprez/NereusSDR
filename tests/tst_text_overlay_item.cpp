// no-port-check: NereusSDR-original tests for the text overlay item.
// Modification history (NereusSDR):
//   2026-10-26 - Text overlay RX/TX swap (line 1 on receive, line 2 on
//                 transmit) for new items, both lines for saved ones; the
//                 panel's MOX flip fade; copy font 1 to 2; auto scroll
//                 only when the text is wider than the item. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContentFade.h"
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
